cmake_minimum_required(VERSION 3.25)

project(
    aymidi
    VERSION 0.1
    LANGUAGES C)

add_subdirectory(src)

configure_file(${CMAKE_SOURCE_DIR}/files/manifest.ttl ${CMAKE_SOURCE_DIR}/dist/manifest.ttl COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/files/aymidi.ttl ${CMAKE_SOURCE_DIR}/dist/aymidi.ttl COPYONLY)
