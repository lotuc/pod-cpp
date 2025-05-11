cmake_minimum_required(VERSION 3.24)
project(pod-cpp)

set(CMAKE_CXX_STANDARD 20)

find_package(nlohmann_json REQUIRED)
find_package(Immer REQUIRED)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/bencode.hpp/include")

add_executable(example src/cpp/example.cpp src/cpp/pod.h)
target_link_libraries(example PUBLIC immer)
target_link_libraries(example PUBLIC nlohmann_json::nlohmann_json)
