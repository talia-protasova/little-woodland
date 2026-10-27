cmake_minimum_required(VERSION 3.16)

project(little_woodland
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(little_woodland
    src/main.cpp
)

target_link_libraries(little_woodland
    PRIVATE
        glfw
        OpenGL::GL
)