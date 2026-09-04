cmake_minimum_required(VERSION 3.16)
project(Electronic_Limit_Order_Book LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(TestLadder tests/TestLadder.cpp)
add_executable(TestFirst tests/Test_first.cpp)
add_executable(TestSnapshot tests/Test_snapshot.cpp)
add_executable(TestCache tests/Test_cache.cpp)
add_executable(TestEvents tests/test_events.cpp)
add_executable(BenchmarkCaches benchmarks/benchmark_caches.cpp)
