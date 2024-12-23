#include <iostream>
#include <fmt/format.h>
#include "../src/utils.h"

int main() {
    std::set<std::string> headers = {"Content-Type: application/json", fmt::format("Authorization: xxx")};
    std::string response;
    int resp_code = 0;
    bool ret = utils::http_post("https://api.openai.com/v1/chat/completions", headers, "{}", response, resp_code);
    if (ret) {
        std::cout << fmt::format("code: {} data: {}", resp_code, response) << std::endl;
    }
    return 0;
}
