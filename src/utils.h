// Copyright (c) 2025 Mars Zuo
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <string_view>
#include <set>

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
    bool http_post(std::string_view url, const std::set<std::string> &headers, const std::string& request, std::string &response, int &http_code);

    // 下载文件
    bool curl_download(std::string_view target_url, std::string_view filepath, std::string_view limit_rate);

    // 创建目录
    bool create_directories(std::string_view filepath);
}
