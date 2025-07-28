cmake_minimum_required(VERSION 3.30)
project(rokuecp VERSION 0.2.0.20250728)
set(CMAKE_C_STANDARD 11)
option(DOCS "Generate documentation" off)

if(DOCS)
    find_package(Doxygen REQUIRED doxygen)
    set(DOXYGEN_PROJECT_NAME RokuECP)
    set(DOXYGEN_PROJECT_ICON controku.png)
    set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
    set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C YES)
    set(DOXYGEN_TYPEDEF_HIDES_STRUCT YES)
    set(DOXYGEN_EXTRACT_LOCAL_CLASSES NO)
    set(DOXYGEN_SHOW_INCLUDE_FILES NO)
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)
    set(DOXYGEN_ALPHABETICAL_INDEX NO)
    set(DOXYGEN_HTML_COLORSTYLE AUTO_DARK)
    set(DOXYGEN_DISABLE_INDEX YES)
    set(DOXYGEN_GENERATE_LATEX NO)
    set(DOXYGEN_GENERATE_MAN YES)
    doxygen_add_docs(doc ALL)
endif()

add_library(rokuecp SHARED rokuecp.c)
set_target_properties(rokuecp PROPERTIES
    VERSION 0.2.0.20250716
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
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION include)
install(FILES "${CMAKE_BINARY_DIR}/rokuecp.pc" DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
