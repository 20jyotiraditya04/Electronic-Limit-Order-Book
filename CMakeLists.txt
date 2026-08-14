cmake_minimum_required(VERSION 4.3)
project(Electronic_Limit_Order_Book)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(Electronic_Limit_Order_Book
        main.cpp
        OrderBookPrimitives.hpp
        PriceLevelTable.hpp
        TestLadder.cpp)
