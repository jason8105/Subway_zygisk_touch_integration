#ifndef ZYGISKPG_AUTH_H
#define ZYGISKPG_AUTH_H

#include <curl/curl.h>
#include <fstream>
#include <string>
#include <algorithm>
#include <android/log.h>
#include <nlohmann/json.hpp>
#include "Include/obfuscate.h"          // <-- provides OBFUSCATE()

using namespace std;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
//  Credentials – replace the placeholders with your real values.
//  The strings are obfuscated at compile‑time, decrypted at runtime.
// ---------------------------------------------------------------------------
static std::string secret = OBFUSCATE("YOUR_SECRET_HERE");
static std::string aid    = OBFUSCATE("YOUR_AID_HERE");
static std::string apikey = OBFUSCATE("YOUR_APIKEY_HERE");

static std::string readBuffer;
static std::string jsonresult;

// ---------------------------------------------------------------------------
static size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    std::string& buf = *static_cast<std::string*>(userdata);
    buf.append(ptr, size * nmemb);
    return size * nmemb;
}

// ---------------------------------------------------------------------------
static std::string CleanString(std::string s)
{
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    return s;
}

// ---------------------------------------------------------------------------
static bool tryAutoLogin()
{
    // --------------------------------------------------------------
    //  Path to the licence file that the Unity side writes.
    // --------------------------------------------------------------
    const char* gamePackage = "com.innersloth.spacemafia";
    std::string configPath = "/data/data/";
    configPath += gamePackage;
    configPath += "/files/license.key";

    __android_log_print(ANDROID_LOG_ERROR, "zyCheats",
                        "Reading config from: %s", configPath.c_str());

    std::ifstream file(configPath);
    if (!file.is_open()) {
        __android_log_print(ANDROID_LOG_ERROR, "zyCheats",
                            "Failed to open license.key");
        return false;
    }

    std::string username, password, line;
    int lineno = 0;
    while (std::getline(file, line) && lineno < 2) {
        ++lineno;
        if (lineno == 1) username = line;
        else if (lineno == 2) password = line;
    }
    file.close();

    username = CleanString(username);
    password = CleanString(password);

    if (username.empty() || password.empty()) {
        __android_log_print(ANDROID_LOG_ERROR, "zyCheats",
                            "Empty credentials in license.key");
        return false;
    }

    // --------------------------------------------------------------
    //  Optional HWID – left blank for now.
    // --------------------------------------------------------------
    std::string hwid = "";

    CURL* handle = curl_easy_init();
    if (!handle) return false;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers,
                                "Content-Type: application/x-www-form-urlencoded");

    std::string postData = "type=login&aid=" + aid + "&apikey=" + apikey +
                           "&secret=" + secret + "&username=" + username +
                           "&password=" + password + "&hwid=" + hwid;

    long http_code = 0;
    curl_easy_setopt(handle, CURLOPT_URL, "https://api.auth.gg/v1/");
    curl_easy_setopt(handle, CURLOPT_POST, 1L);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(handle);
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(handle);

    if (res != CURLE_OK) {
        __android_log_print(ANDROID_LOG_ERROR, "zyCheats",
                            "Auth failed: %s", curl_easy_strerror(res));
        return false;
    }

    if (http_code == 200) {
        jsonresult = readBuffer;
        __android_log_print(ANDROID_LOG_INFO, "zyCheats",
                            "Auth Response: %s", jsonresult.c_str());

        try {
            auto j = json::parse(jsonresult);
            if (j["result"].get<std::string>() == "success")
                return true;
        } catch (...) {
            __android_log_print(ANDROID_LOG_ERROR, "zyCheats",
                                "JSON parse error");
        }
    }
    return false;
}

#endif // ZYGISKPG_AUTH_H