cmake_minimum_required(VERSION 3.20)
project(LamportsFactory_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(lf
  src/main.cpp
)

target_include_directories(lf PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/clask
)

target_compile_options(lf PRIVATE -Wall -Wextra -Wpedantic -Wconversion)

find_package(Threads REQUIRED)
target_link_libraries(lf PRIVATE Threads::Threads)

