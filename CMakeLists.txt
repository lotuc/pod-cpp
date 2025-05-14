cmake_minimum_required(VERSION 3.24)
project(pod-cpp)

set(CMAKE_CXX_STANDARD 20)

find_package(nlohmann_json REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(asio REQUIRED asio)

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/bencode.hpp/include"
)

add_executable(test_pod src-dev/cpp/test_pod.cpp src/cpp/pod.h)
target_link_libraries(test_pod PUBLIC nlohmann_json::nlohmann_json)
target_include_directories(test_pod PUBLIC asio_INCLUDE_DIRS)
