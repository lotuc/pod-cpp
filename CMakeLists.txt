cmake_minimum_required(VERSION 3.24)
project(pod-cpp)

set(CMAKE_CXX_STANDARD 20)

find_package(nlohmann_json REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(asio QUIET asio)
if (NOT asio_FOUND)
  find_package(asio CONFIG REQUIRED)
endif()

set(test_ns_sources
  src-dev/cpp/test_ns.h
  src-dev/cpp/test_ns.cpp
)

add_executable(test_pod src-dev/cpp/test_pod.cpp ${test_ns_sources})
add_executable(test_jsonrpc src-dev/cpp/test_jsonrpc.cpp ${test_ns_sources})

set(TEST_TARGETS test_pod test_jsonrpc)

foreach(t ${TEST_TARGETS})
  target_include_directories(${t} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/bencode.hpp/include"
  )
endforeach()

# json encoder support
target_link_libraries(test_pod PUBLIC nlohmann_json::nlohmann_json)

# asio transport (TCP) support
if (asio_INCLUDE_DIRS)
  target_include_directories(test_pod PUBLIC asio_INCLUDE_DIRS)
else()
  target_link_libraries(${target} PRIVATE asio::asio)
endif()
