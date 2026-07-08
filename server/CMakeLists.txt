cmake_minimum_required(VERSION 3.20)
project(LamportsFactory_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

set(LF_CORE_SOURCES
  src/app/common/utils.cpp
  src/app/common/encoding.cpp
  src/app/common/glob.cpp
  src/app/common/path_utils.cpp
  src/app/common/time_utils.cpp
  src/app/config/config.cpp
  src/app/compiler/latex_backend.cpp
  src/app/events/event_bus.cpp
  src/app/build/build_log_buffer.cpp
  src/app/process/process.cpp
  src/app/build/build_executor.cpp
  src/app/build/build_events.cpp
  src/app/build/build_manager.cpp
  src/app/watch/file_watcher.cpp
  src/app/workspace/workspace_documents.cpp
  src/app/http/api_payloads.cpp
  src/app/http/response.cpp
  src/app/http/server_handle.cpp
)

add_library(lf_core STATIC ${LF_CORE_SOURCES})

target_include_directories(lf_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party
)

target_include_directories(lf_core SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/httplib
)

target_compile_options(lf_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)

find_package(Threads REQUIRED)
target_link_libraries(lf_core PUBLIC Threads::Threads)

add_executable(lf src/main.cpp)
target_link_libraries(lf PRIVATE lf_core)

if(BUILD_TESTING)
  add_executable(lf_unit_tests tests/unit_tests.cpp)
  target_link_libraries(lf_unit_tests PRIVATE lf_core)
  target_compile_options(lf_unit_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
  add_test(NAME lf_unit_tests COMMAND lf_unit_tests)
endif()
