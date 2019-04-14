#include "pch.h"

#include <iostream>
#include <Windows.h>
#include <string>
#include <thread>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <mutex>

#include <json.h>

#include <curl/curl.h>

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

namespace fs = boost::filesystem;

std::unordered_map<std::string, std::stringstream> m_icons;
std::mutex m_icons_mtx;
std::unordered_map<std::string, std::stringstream> m_icons_gray;
std::mutex m_icons_gray_mtx;

// Write data callback
inline static size_t write_data_callback( char* ptr, size_t size, size_t nmemb, void* data )
{
	*(static_cast< std::string* >(data)) += std::string { ptr, size * nmemb };
	return size * nmemb;
}

// Write image callback
inline static size_t write_image_callback( char *ptr, size_t size, size_t nmemb, void *data )
{
	auto stream = reinterpret_cast< std::ostringstream* >(data);
	auto count = size * nmemb;
	stream->write( ptr, count );
	return count;
}

int main( int argc, char* argv[] )
{
	if ( argc < 3 )
	{
		std::wcout << TEXT("Usage: steam_icons_downloader <app_id> <steam_webapi_key>") << std::endl;
		return -1;
	}

	try
	{
		boost::lexical_cast< uint32_t >(argv[1]);
	}
	catch ( boost::bad_lexical_cast & ex )
	{
		std::wcout << TEXT( "[ERROR]" ) << ' ' << ex.what() << std::endl;
		return -1;
	}

	std::string stats_data {};
	auto stats_url = (boost::format( "https://api.steampowered.com/ISteamUserStats/GetSchemaForGame/v1/?key=%1%&appid=%2%" ) % argv[2] % argv[1]).str();

	std::thread thread { [&]
	{
		auto curl = curl_easy_init();
		if ( curl )
		 {
			curl_easy_setopt( curl, CURLOPT_URL, stats_url.c_str() );
			curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, 0L );
			curl_easy_setopt( curl, CURLOPT_USE_SSL, CURLUSESSL_ALL );
			curl_easy_setopt( curl, CURLOPT_WRITEDATA, &stats_data );
			curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_data_callback );
			curl_easy_setopt( curl, CURLOPT_TIMEOUT, 5L ); // 5 seconds timeout

			auto const curl_code = curl_easy_perform( curl );
			curl_easy_cleanup( curl );

			if ( curl_code != CURLcode::CURLE_OK )
				std::wcout << __FUNCTIONW__ << ' ' << curl_easy_strerror( curl_code ) << std::endl;
		 }
	 } };
	thread.join();

	if ( stats_data.empty() )
	{
		std::wcout << TEXT( "[ERROR] Stats data is empty" ) << std::endl;
		return -1;
	}

	// Parse json data
	auto json = nlohmann::json::parse( stats_data );

	if ( json == nullptr )
	{
		std::wcout << TEXT( "[ERROR] Unable to parse the required json data" ) << std::endl;
		return -1;
	}

	auto ach_items_obj = json["game"]["availableGameStats"]["achievements"];

	if ( ach_items_obj == nullptr )
	{
		std::wcout << TEXT( "[ERROR] Unable to fetch the 'achievements' object" ) << std::endl;
		return -1;
	}

	// Path to the '<app_id>/icons' directory, where all the achievement icons should be stored
	auto icons_path = fs::current_path() / argv[1] / "icons";

	// Create the required directory if it doesn't exist
	if ( !fs::exists( icons_path ) )
		fs::create_directories( icons_path );

	// Path to the '<app_id>/icons_gray' directory, where all the achievement icons should be stored
	auto icons_path_gray = fs::current_path() / argv[1] / "icons_gray";

	// Create the required directory if it doesn't exist
	if ( !fs::exists( icons_path_gray ) )
		fs::create_directories( icons_path_gray );

	std::wcout << TEXT( "Achievements object found, starting download. Please wait..." ) << std::endl;

	for ( auto& ach : ach_items_obj.items() )
	{
		auto& ach_id = ach.key();

		// Achieved icons
		auto& ach_ico_obj = json["game"]["availableGameStats"]["achievements"][ach_id]["icon"];

		if ( ach_ico_obj == nullptr )
		{
			std::cout << (boost::format( "[ERROR] Unable to fetch the achievement object for ['%1%'][icon]" ) % ach_id).str() << std::endl;
			return -1;
		}

		// Unachieved icons
		auto& ach_ico_gray_obj = json["game"]["availableGameStats"]["achievements"][ach_id]["icongray"];

		if ( ach_ico_gray_obj == nullptr )
		{
			std::cout << (boost::format( "[ERROR] Unable to fetch the achievement object for ['%1%'][icongray]" ) % ach_id).str() << std::endl;
			return -1;
		}

		// Full path to the icon extracted from json
		auto ach_ico = ach_ico_obj.get<std::string>();
		// Local path to the downloaded icon
		auto icon_path = (icons_path / fs::path( ach_ico ).filename() ).generic_string();

		// Full path to the icon extracted from json
		auto ach_ico_gray = ach_ico_gray_obj.get<std::string>();
		// Local path to the downloaded icon
		auto icon_path_gray = (icons_path_gray / fs::path( ach_ico_gray ).filename()).generic_string();

		if ( !fs::exists( icon_path ) )
		{
			std::stringstream stream {};

			std::thread thread { [&]
			{
			auto curl = curl_easy_init();
			if ( curl )
			{
				curl_easy_setopt( curl, CURLOPT_URL, ach_ico.c_str() );
				curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, 0L );
				curl_easy_setopt( curl, CURLOPT_USE_SSL, CURLUSESSL_ALL );
				curl_easy_setopt( curl, CURLOPT_WRITEDATA, &stream );
				curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_image_callback );
				curl_easy_setopt( curl, CURLOPT_TIMEOUT, 3L ); // 3 seconds timeout

				auto const curl_code = curl_easy_perform( curl );
				curl_easy_cleanup( curl );

				if ( curl_code != CURLcode::CURLE_OK )
					std::wcout << __FUNCTIONW__ << ' ' << curl_easy_strerror( curl_code ) << std::endl;
			}
			} };
			thread.join();

			if ( stream.str().empty() )
			{
				std::cout << (boost::format( "[ERROR] Image stream for '%1%' achievement icon is invalid" ) % ach_id).str() << std::endl;
				return -1;
			}

			// Insert achievement path and the icon image data
			std::lock_guard<std::mutex> lock( m_icons_mtx );
			m_icons.try_emplace( icon_path, stream.str() );
		}
		else
			std::cout << (boost::format( "[ERROR] '%1%' already exists" ) % icon_path).str() << std::endl;

		if ( !fs::exists( icon_path_gray ) )
		{
			std::stringstream stream {};

			std::thread thread { [&]
			{
			auto curl = curl_easy_init();
			if ( curl )
			{
				curl_easy_setopt( curl, CURLOPT_URL, ach_ico_gray.c_str() );
				curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, 0L );
				curl_easy_setopt( curl, CURLOPT_USE_SSL, CURLUSESSL_ALL );
				curl_easy_setopt( curl, CURLOPT_WRITEDATA, &stream );
				curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_image_callback );
				curl_easy_setopt( curl, CURLOPT_TIMEOUT, 3L ); // 3 seconds timeout

				auto const curl_code = curl_easy_perform( curl );
				curl_easy_cleanup( curl );

				if ( curl_code != CURLcode::CURLE_OK )
					std::wcout << __FUNCTIONW__ << ' ' << curl_easy_strerror( curl_code ) << std::endl;
			}
			} };
			thread.join();

			if ( stream.str().empty() )
			{
				std::cout << (boost::format( "[ERROR] Image stream for '%1%' achievement icon is invalid" ) % ach_id).str() << std::endl;
				return -1;
			}

			// Insert achievement path and the icon image data
			std::lock_guard<std::mutex> lock( m_icons_gray_mtx );
			m_icons_gray.try_emplace( icon_path_gray, stream.str() );
		}
		else
			std::cout << (boost::format( "[ERROR] '%1%' already exists" ) % icon_path_gray).str() << std::endl;
	}

	if ( m_icons.empty() )
	{
		std::wcout << TEXT( "[ERROR] Achieved icons size is NULL" ) << std::endl;
		return -1;
	}

	for ( auto& icon : m_icons )
	{
		// Open the file stream and write the required image data
		std::ofstream file_ofstream {};
		file_ofstream.open( icon.first.c_str(), std::ios::out | std::ios::binary );

		if ( file_ofstream )
		{
			file_ofstream << icon.second.str();
			file_ofstream.close();

			std::cout << (boost::format( "'%1%' successfully written" ) % icon.first).str() << std::endl;
		}
	}

	if ( m_icons_gray.empty() )
	{
		std::wcout << TEXT( "[ERROR] Unachieved icons size is NULL" ) << std::endl;
		return -1;
	}

	for ( auto& icon_gray : m_icons_gray )
	{
		// Open the file stream and write the required image data
		std::ofstream file_ofstream {};
		file_ofstream.open( icon_gray.first.c_str(), std::ios::out | std::ios::binary );

		if ( file_ofstream )
		{
			file_ofstream << icon_gray.second.str();
			file_ofstream.close();

			std::cout << (boost::format( "'%1%' successfully written" ) % icon_gray.first).str() << std::endl;
		}
	}

	std::cout << (boost::format( "In total there are '%1%' / '%2%' icons downloaded" ) % m_icons.size() % m_icons_gray.size()).str() << std::endl;

	return 0;
}
