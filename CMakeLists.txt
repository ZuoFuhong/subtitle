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

# Ensure thread safety for SDL2
add_definitions(-D_THREAD_SAFE)

foreach (_target spdlog ggml whisper onnxruntime fmt opus sdl2 openssl tiff webp opencv png openjpeg xz jpeg-turbo openexr zstd tbb)
    include_directories(third_party/${_target}/include/)
    link_directories(third_party/${_target}/lib/)
endforeach()

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
        common_lib ssl crypto opus spdlog onnxruntime fmt curl iconv SDL2
        "-framework AppKit"
        "-framework AudioToolbox"
        "-framework GameController"
        "-framework IOKit"
        "-framework Carbon"
        "-framework CoreAudio"
        "-framework CoreVideo"
        "-framework CoreHaptics"
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework ForceFeedback"
        "-framework Metal"
        "-framework MetalKit")
endforeach()

foreach(_target audio_vad test)
    add_executable(${_target} tests/${_target}.cc)
    target_link_libraries(${_target}
            common_lib ssl crypto opus whisper onnxruntime fmt curl z tiff webp sharpyuv png jpeg openjp2 lzma
            zstd tbb tegra_hal ittnotify OpenEXR opencv_core opencv_imgcodecs opencv_highgui opencv_imgproc
            "-framework OpenCL"
            "-framework Cocoa"
            "-framework Accelerate"
            "-framework Foundation"
            "-framework Metal"
            "-framework MetalKit")
endforeach()
