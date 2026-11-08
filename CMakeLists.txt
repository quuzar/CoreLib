cmake_minimum_required(VERSION 4.0)
project(CoreLib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(PLATFORM win)
elseif(UNIX)
    set(PLATFORM psx)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

add_library(CoreLib STATIC
        core/${PLATFORM}/file.cpp
        core/${PLATFORM}/mapping.cpp
        core/${PLATFORM}/memory.cpp
        core/${PLATFORM}/thread.cpp
        core/${PLATFORM}/sync.cpp

        src/${PLATFORM}/file.cpp
        src/${PLATFORM}/mapped_file.cpp
        src/${PLATFORM}/virtual_memory.cpp
        src/${PLATFORM}/thread.cpp
        src/${PLATFORM}/mutex.cpp
        src/${PLATFORM}/condition_variable.cpp

        src/common/memory_pool.cpp
        src/common/arena.cpp
        src/common/thread_pool.cpp
        src/common/concurrent_queue.cpp
)

target_include_directories(CoreLib
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/core
)

add_executable(CoreLibTest src/main.cpp)
target_link_libraries(CoreLibTest PRIVATE CoreLib)
