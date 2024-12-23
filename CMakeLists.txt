cmake_minimum_required(VERSION 3.18)
project(subtitle)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++17")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Debug or Release
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")

include_directories(/opt/homebrew/include/)
link_directories(/opt/homebrew/lib/)

foreach (_target spdlog ggml whisper onnxruntime fmt)
    include_directories(third_party/${_target}/include/)
    link_directories(third_party/${_target}/lib/)
endforeach()

# Ensure thread safety for SDL2
add_definitions(-D_THREAD_SAFE)

# Find SDL2 installation
find_package(SDL2 CONFIG REQUIRED)
message(STATUS "Using SDL2 ${SDL2_VERSION}")

add_library(common_lib
        src/utils.cc
        src/lru_queue.cc
        src/audio_recorder.cc
        src/audio_codec.cc
        src/udp_codec.cc
        src/subtitle_window.cc
        src/convert_timer.cc
        src/asrapi.cc)

foreach(_target main)
    add_executable(${_target} src/${_target}.cc)
    target_link_libraries(${_target}
        common_lib ssl crypto opus spdlog onnxruntime fmt curl
        SDL2::SDL2)
endforeach()

foreach(_target test audio_vad)
    add_executable(${_target} tests/${_target}.cc)
    target_link_libraries(${_target}
            common_lib ssl crypto opus whisper onnxruntime fmt curl
            "-framework Accelerate -framework Foundation -framework Metal -framework MetalKit")
endforeach()
