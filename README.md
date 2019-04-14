# **Steam Achievement Icons Downloader**

### A simple downloader for the Steam achievement icons

## Usage

```
steam_icons_downloader <app_id> <steam_webapi_key>
```

`app_id` - is a Steam application ID (e.g. https://store.steampowered.com/app/292030 `292030` is an actual ID)
`steam_webapi_key` - a special key to access Steam WebAPI features. Get it here: https://steamcommunity.com/dev/apikey

## Stuff used

- **Boost** (https://www.boost.org/)
- **cURL** (_v7.64.1_) (https://github.com/curl/curl)
- **OpenSSL** (_v1.1.1b_) (https://github.com/openssl/openssl)
- **JSON for Modern C++** (https://github.com/nlohmann/json)

## Compilation notes

- You need **Boost** installed and added to your system environment variables (_%BOOST_PATH%_ - a path to the boost dir)
- **Visual Studio 2017** is required to compile this project with the included static libraries