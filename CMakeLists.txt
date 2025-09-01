cmake_minimum_required(VERSION 3.10)

project(my_torrent_client)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    ctorrent main.cpp
    source/src/Bencode.cpp
)

target_include_directories(
    ctorrent PRIVATE source/include
)

