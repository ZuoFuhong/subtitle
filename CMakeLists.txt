cmake_minimum_required(VERSION 3.18)
project(subtitle)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++17")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Debug or Release
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")

include_directories(/opt/homebrew/include/)
link_directories(/opt/homebrew/lib/)

# Ensure thread safety for SDL2
add_definitions(-D_THREAD_SAFE)

# Find SDL2 installation
find_package(SDL2 CONFIG REQUIRED)
message(STATUS "Using SDL2 ${SDL2_VERSION}")

# Find spdlog installation
find_package(SPDLOG CONFIG REQUIRED)
message(STATUS "Using spdlog ${SPDLOG_VERSION}")

# Find boost installation
find_package(Boost CONFIG REQUIRED)
message(STATUS "Using boost ${Boost_VERSION}")

add_library(common_lib
        src/utils.cc
        src/lru_queue.cc
        src/audio_recorder.cc
        src/udp_codec.cc
        src/subtitle_window.cc
        src/convert_timer.cc)

foreach(_target main test robot)
    add_executable(${_target} src/${_target}.cc)
    target_link_libraries(${_target}
        common_lib ssl crypto
        SDL2::SDL2
        spdlog::spdlog
        Boost::boost)
endforeach()
