#include <regex>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <curl/curl.h>
#include "utils.h"

bool utils::ends_with(std::string_view str, std::string_view suffix) {
    if (str.length() < suffix.length()) {
        return false;
    }
    return str.substr(str.length() - suffix.length()) == suffix;
}

std::string utils::to_upper_case(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

std::string utils::trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();
    return (start < end) ? std::string(start, end) : std::string();
}

int64_t utils::current_timestamp() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::chrono::duration<double, std::milli> timestamp = now.time_since_epoch();
    auto timestamp_ms = static_cast<std::int64_t>(timestamp.count());
    return timestamp_ms;
}

std::string utils::format_timestamp(int64_t timestamp, const std::string& pattern) {
    auto timePoint = std::chrono::time_point<std::chrono::system_clock>(std::chrono::milliseconds(timestamp));
    std::time_t time = std::chrono::system_clock::to_time_t(timePoint);
    std::tm* timeinfo = std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(timeinfo, pattern.c_str());
    return oss.str();
}

void utils::replace_substr(std::string& str, std::string_view old_substr, std::string_view new_substr) {
    size_t pos = 0;
    while ((pos = str.find(old_substr, pos)) != std::string::npos) {
        str.replace(pos, old_substr.length(), new_substr);
        pos += new_substr.length();
    }
}

bool utils::parse_address(const std::string& address, std::string& ip, unsigned short& port) {
    std::regex pattern(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})(?::(\d+))?)");
    std::smatch match;
    if (std::regex_match(address, match, pattern)) {
        ip = match[1].str();
        std::string port_str = match[2].str();
        if (port_str.empty()) {
            port = 80;
        } else {
            port = static_cast<unsigned short>(std::stoi(port_str));
        }
        return true;
    }
    return false;
}

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    auto str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), realsize);
    return realsize;
}

bool utils::http_post(std::string_view url, const std::set<std::string> &headers, std::string_view request, std::string &response, int &http_code) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* curl = curl_easy_init();
    if (curl) {
        struct curl_slist *req_headers = nullptr;
        for (const std::string& item : headers) {
            req_headers = curl_slist_append(req_headers, item.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_URL, url.data());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, req_headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.data());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "http_post curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            http_code = 500;
            return false;
        } else {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE , &http_code);
        }
        curl_slist_free_all(req_headers);
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    return true;
}
