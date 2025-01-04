cmake_minimum_required(VERSION 3.30)
project(rokuecp)

set(CMAKE_CXX_STANDARD 11)
add_library(rokuecp SHARED rokuecp.cpp)

find_package(PkgConfig)
pkg_check_modules(gssdp REQUIRED gssdp-1.6)
pkg_check_modules(soup REQUIRED libsoup-3.0)
pkg_check_modules(tinyxml2 REQUIRED tinyxml2)

target_include_directories(rokuecp PRIVATE ${gssdp_INCLUDE_DIRS})
target_include_directories(rokuecp PRIVATE ${soup_INCLUDE_DIRS})
target_include_directories(rokuecp PRIVATE ${tinyxml2_INCLUDE_DIRS})
target_link_libraries(rokuecp PRIVATE ${gssdp_LINK_LIBRARIES})
target_link_libraries(rokuecp PRIVATE ${soup_LINK_LIBRARIES})
target_link_libraries(rokuecp PRIVATE ${tinyxml2_LINK_LIBRARIES})