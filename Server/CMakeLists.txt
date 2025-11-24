cmake_minimum_required(VERSION 3.20)
project(gameserver)

set(CMAKE_CXX_STANDARD 17)

add_executable(gameserver main.cpp)

find_package(Threads REQUIRED)
target_link_libraries(gameserver Threads::Threads)