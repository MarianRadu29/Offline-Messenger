cmake_minimum_required(VERSION 3.29)
project(offline_messenger)

set(CMAKE_CXX_STANDARD 14)


add_executable(offline_messenger
        client.cpp
        server.cpp
        struct.h
        json.hpp
        struct_server.h
        struct_client.h
)
