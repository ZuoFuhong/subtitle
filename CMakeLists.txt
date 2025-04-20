cmake_minimum_required(VERSION 3.18)
project(subtitle VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo;Debug;Release;MinSizeRel" CACHE STRING "" FORCE)
if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "None")
    set (CMAKE_BUILD_TYPE "RelWithDebInfo")
    message (STATUS "CMAKE_BUILD_TYPE is not set, set to default = ${CMAKE_BUILD_TYPE}")
endif ()
message (STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_COMPILER "clang")
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wpedantic -fsanitize=address")

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
        src/offline_convert_timer.cc
        src/asrapi.cc)

foreach(_target main)
   add_executable(${_target} src/${_target}.cc)
   target_link_libraries(${_target}
       common_lib ssl crypto opus spdlog whisper onnxruntime fmt curl iconv SDL2
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
       "-framework Accelerate"
       "-framework Foundation"
       "-framework Metal"
       "-framework MetalKit")
endforeach()

foreach(_target test)
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
