cmake_minimum_required(VERSION 3.16)
project(Electronic_Limit_Order_Book)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(.)

add_executable(TestLadder TestLadder.cpp)
add_executable(TestFirst Test_first.cpp
        Test_snapshot.cpp
        Test_cache.cpp
        ICacheClient.hpp
        RedisCacheClient.hpp
        MemcachedCacheClient.hpp
        benchmark_caches.cpp)
