cmake_minimum_required(VERSION 3.24)

project(QuickJSDebugger LANGUAGES CXX)

# Stop in source builds
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

set(SRC_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(3RD_PARTY_ROOT_DIR ${SRC_ROOT_DIR}/3rdparty)
set(DEBUGGER_ROOT_DIR ${SRC_ROOT_DIR}/debugger)

add_subdirectory(${3RD_PARTY_ROOT_DIR})
add_subdirectory(${DEBUGGER_ROOT_DIR})