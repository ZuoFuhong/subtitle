#include "utils.h"
#include <chrono>

int64_t current_timestamp() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::chrono::duration<double, std::milli> timestamp = now.time_since_epoch();
    auto timestamp_ms = static_cast<std::int64_t>(timestamp.count());
    return timestamp_ms;
}
