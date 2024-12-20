#pragma once

#include <cstdint>
#include <iostream>

namespace utils {

    // 清除空格
    std::string trim(const std::string& str);

    // 时间戳, 单位: ms
    int64_t current_timestamp();

    // 时间格式化
    std::string format_timestamp(int64_t timestamp, const std::string& pattern = "%Y-%m-%d %H:%M:%S");

    // 替换字符串
    void replace_substr(std::string& str, const std::string& old_substr, const std::string& new_substr);

    // 解析网络地址
    bool parse_address(const std::string& address, std::string& ip, unsigned short& port);

    // 翻译英文句子
    std::string translate_sentence(const std::string& sentence);
}
