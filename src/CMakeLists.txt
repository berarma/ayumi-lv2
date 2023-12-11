cmake_minimum_required(VERSION 3.25)

add_library(${PROJECT_NAME} SHARED aymidi.c synth.c synth.h ayumi.c ayumi.h)

set_target_properties(aymidi PROPERTIES PREFIX "")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}>
    ${CMAKE_SOURCE_DIR}/dist/$<TARGET_FILE_NAME:${PROJECT_NAME}>
)
