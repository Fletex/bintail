cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(bintail VERSION 0.1)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
find_package(Catch2 REQUIRED)
pkg_search_module(ELF REQUIRED libelf)
pkg_search_module(MULTIVERSE REQUIRED libmultiverse)

enable_testing()
add_subdirectory(samples)
add_subdirectory(src)
