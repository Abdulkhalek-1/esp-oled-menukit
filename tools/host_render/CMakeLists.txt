cmake_minimum_required(VERSION 3.16)
project(host_render C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# Step 1: build sh1106_host + font8x8 into a static lib (no executable yet).
add_library(render_core STATIC
    sh1106_host.c
    ${REPO_ROOT}/components/font8x8/src/font8x8.c
)

target_include_directories(render_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${REPO_ROOT}/components/sh1106/include
    ${REPO_ROOT}/components/font8x8/include
)
