cmake_minimum_required(VERSION 3.16)

project(little_woodland
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    SOURCE_SUBDIR cmake
)

FetchContent_MakeAvailable(glad)

glad_add_library(
    glad_gl_core_33
    STATIC
    REPRODUCIBLE
    API gl:core=3.3
)

find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

add_library(woodland_puzzle src/puzzle/Board.cpp)
target_include_directories(woodland_puzzle PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(little_woodland
    src/main.cpp
    src/rendering/ShaderProgram.cpp
    src/rendering/BoardRenderer.cpp
)

target_link_libraries(little_woodland
    PRIVATE
        woodland_puzzle
        glfw
        glad_gl_core_33
        OpenGL::GL
)

include(CTest)
if(BUILD_TESTING)
    add_executable(board_tests tests/BoardTests.cpp)
    target_link_libraries(board_tests PRIVATE woodland_puzzle)
    add_test(NAME board_invariants COMMAND board_tests)
endif()
