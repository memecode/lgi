cmake_minimum_required(VERSION 3.18)

set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
set(LGI_LIB_ONLY TRUE)

project(GoogleTranslatePlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(google_cloud_cpp_translate CONFIG REQUIRED)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../.." lgi_dir)

add_library(GoogleTranslatePlugin SHARED
    plugin.cpp)

target_include_directories(GoogleTranslatePlugin
    PRIVATE
        "../../resources")

target_link_libraries(GoogleTranslatePlugin
    PRIVATE
        lgi
        google-cloud-cpp::translate)
