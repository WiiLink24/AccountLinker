#pragma once

#include <iostream>
#include <format>
#include <vector>
#include "tinyjson.h"
#include <curl/curl.h>

using json = nlohmann::json;

enum JSONExceptionID : int {
    SyntaxError = 101,
};

struct HTTPResponse {
    json j;
    std::string json_error;
    int json_error_id;
    long status_code;
    CURLcode curl_code;
    std::string resp;
};

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output);
HTTPResponse http_post(std::string_view url, std::string_view post_data, const std::vector<std::string>& headers);
