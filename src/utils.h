#pragma once

#include <string_view>
#include <set>
#include "../third_party/json.hpp"

namespace utils {

    // 检查字符串后缀
    bool ends_with(std::string_view str, std::string_view suffix);

    // 字符串转大写
    std::string to_upper_case(const std::string& str);

    // 清除空格
    std::string trim(const std::string& str);

    // 时间戳, 单位: ms
    int64_t current_timestamp();

    // 时间格式化
    std::string format_timestamp(int64_t timestamp, const std::string& pattern = "%Y-%m-%d %H:%M:%S");

    // 替换字符串
    void replace_substr(std::string& str, std::string_view old_substr, std::string_view new_substr);

    // 解析网络地址
    bool parse_address(const std::string& address, std::string& ip, unsigned short& port);

    // 发送 POST 请求
    bool http_post(std::string_view url, const std::set<std::string> &headers, std::string_view request, std::string &response, int &http_code);
}
