#include <iostream>
#include "../src/utils.h"

int main() {
    auto sentence_zh = "Are you OK!";
    std::cout << "\033[38;5;222m" << sentence_zh << "\033[0m" << std::endl;
    std::cout << utils::format_timestamp(1715229908521, "%H:%M:%S") << std::endl;
    return 0;
}