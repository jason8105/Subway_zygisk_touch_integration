#ifndef ZYGISKPG_AUTH_H
#define ZYGISKPG_AUTH_H

#include <curl/curl.h>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include <android/log.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

// 🔧 FIX: OBFUSCATE("") empty strings bhejta hai jo API ko "Invalid type" error deta hai.
// Yahan apne actual credentials daalo. Runtime pe decrypt honge.
std::string secret = OBFUSCATE("YOUR_SECRET_HERE");
std::string aid = OBFUSCATE("YOUR_AID_HERE");
std::string apikey = OBFUSCATE("YOUR_APIKEY_HERE");

std::string readBuffer;
std::string jsonresult;

size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* resData) {
    std::string& buf = *static_cast<std::string*>(resData);
    buf.append(ptr, ptr + size * nmemb);
    return size * nmemb;
}

// 🔧 FIX: Clean newline/carriage return properly
std::string CleanString(std::string str) {
    str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
    str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
    return str;
}

bool tryAutoLogin() {
    // 🔧 FIX: Unity ka persistentDataPath C++ mein directly nahi milta.
    // Android native apps ke liye standard path use karo.
    const char* gamePackage = "com.kiloo.subwaysurf"; // Game package name match karo
    std::string configPath = "/data/data/";
    configPath += gamePackage;
    configPath += "/files/license.key";

    LOGE("Reading config from: %s", configPath.c_str());

    std::ifstream file(configPath);
    if (!file.is_open()) {
        LOGE("Failed to open license.key");
        return false;
    }

    std::string username, password;
    std::string line;
    int lineno = 0;
    while (std::getline(file, line) && lineno < 2) {
        lineno++;
        if (lineno == 1) username = line;
        else if (lineno == 2) password = line;
    }
    file.close();

    username = CleanString(username);
    password = CleanString(password);

    if (username.empty() || password.empty()) {
        LOGE("Empty credentials in license.key");
        return false;
    }

    // HWID generate karne ka basic placeholder (optional)
    std::string hwid = "";

    CURL *handle;
    CURLcode result;
    long http_code;

    curl_global_init(CURL_GLOBAL_ALL);
    handle = curl_easy_init();

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

    std::string postData = "type=login&aid=" + aid + "&apikey=" + apikey + 
                           "&secret=" + secret + "&username=" + username + 
                           "&password=" + password + "&hwid=" + hwid;

    curl_easy_setopt(handle, CURLOPT_URL, "https://api.auth.gg/v1/");
    curl_easy_setopt(handle, CURLOPT_POST, 1L);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 10L);

    result = curl_easy_perform(handle);
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(handle);
    curl_global_cleanup(CURL_GLOBAL_ALL);

    if (result != CURLE_OK) {
        LOGE("Auth failed: %s", curl_easy_strerror(result));
        return false;
    }

    if (http_code == 200) {
        jsonresult = readBuffer;
        LOGI("Auth Response: %s", jsonresult.c_str());
        
        try {
            auto j = json::parse(jsonresult);
            if (j["result"].get<std::string>() == "success") {
                return true;
            }
        } catch (...) {
            LOGE("JSON parse error");
        }
    }
    return false;
}

#endif //ZYGISKPG_AUTH_H