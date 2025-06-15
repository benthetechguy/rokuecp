cmake_minimum_required(VERSION 3.30)
project(rokuecp VERSION 0.2.0)
set(CMAKE_C_STANDARD 11)

add_library(rokuecp SHARED rokuecp.c)
set_target_properties(rokuecp PROPERTIES
#    VERSION 0.2.0
    SOVERSION 0
    PUBLIC_HEADER rokuecp.h
)

include(CheckFunctionExists)
check_function_exists(strlcpy HAVE_STRLCPY)
if(NOT HAVE_STRLCPY)
    target_compile_definitions(rokuecp PRIVATE -DNO_STRLCPY)
endif()

find_package(PkgConfig)
pkg_check_modules(gssdp REQUIRED gssdp-1.6)
pkg_check_modules(libsoup REQUIRED libsoup-3.0)
pkg_check_modules(libxml2 REQUIRED libxml-2.0>=2.13)

target_include_directories(rokuecp PRIVATE ${gssdp_INCLUDE_DIRS})
target_include_directories(rokuecp PRIVATE ${libsoup_INCLUDE_DIRS})
target_include_directories(rokuecp PRIVATE ${libxml2_INCLUDE_DIRS})
target_link_libraries(rokuecp PRIVATE ${gssdp_LINK_LIBRARIES})
target_link_libraries(rokuecp PRIVATE ${libsoup_LINK_LIBRARIES})
target_link_libraries(rokuecp PRIVATE ${libxml2_LINK_LIBRARIES})

configure_file(rokuecp.pc.in rokuecp.pc @ONLY)
install(TARGETS rokuecp
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include)
install(FILES "${CMAKE_BINARY_DIR}/rokuecp.pc" DESTINATION lib/pkgconfig)
