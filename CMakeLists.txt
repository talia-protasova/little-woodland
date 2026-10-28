cmake_minimum_required(VERSION 3.16)

project(little_woodland
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The browser uses Emscripten's GLFW/GLES port and an intentionally silent backend.
# Desktop dependencies and release packaging remain confined to the native build.
if(EMSCRIPTEN)
    if(WOODLAND_RELEASE_WITH_AUDIO)
        message(FATAL_ERROR "Browser audio is not implemented; use the sound-free web build")
    endif()
    add_compile_options(-fexceptions)
    add_library(woodland_audio src/audio/AudioSilent.cpp)
    target_include_directories(woodland_audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
else()
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
find_package(Threads REQUIRED)
option(WOODLAND_PORTABLE "Exclude checkout-specific asset fallback from installed binaries" OFF)
option(WOODLAND_RELEASE_WITH_AUDIO "Include Mixkit sounds only in the binary package" OFF)
if(WOODLAND_RELEASE_WITH_AUDIO)
    set(WOODLAND_PORTABLE ON CACHE BOOL "Exclude checkout-specific asset fallback from installed binaries" FORCE)
endif()
add_library(woodland_audio src/audio/Audio.cpp src/audio/Miniaudio.cpp)
target_include_directories(woodland_audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(woodland_audio SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio)
target_compile_definitions(woodland_audio PRIVATE MA_NO_FLAC MA_NO_MP3 MA_NO_ENCODING MA_NO_GENERATION MA_NO_RESOURCE_MANAGER MA_NO_NODE_GRAPH MA_NO_ENGINE)
target_link_libraries(woodland_audio PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)

endif()

add_library(woodland_puzzle src/puzzle/Board.cpp src/puzzle/Level.cpp)
target_include_directories(woodland_puzzle PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(woodland_presentation src/presentation/BoardPlayback.cpp)
target_link_libraries(woodland_presentation PUBLIC woodland_puzzle)

add_executable(little_woodland
    src/main.cpp
    src/rendering/ShaderProgram.cpp
    src/rendering/BoardRenderer.cpp
    src/rendering/Texture.cpp
    src/rendering/FrameAnimation.cpp
    src/platform/Assets.cpp
)

# Resolve the source asset directory once at configure time. Both project-root and
# build-directory launches work without copying assets after each build.
if(NOT EMSCRIPTEN AND NOT WOODLAND_PORTABLE)
    target_compile_definitions(little_woodland PRIVATE WOODLAND_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
endif()
target_include_directories(little_woodland SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb)

target_link_libraries(little_woodland PRIVATE woodland_presentation woodland_audio)
if(EMSCRIPTEN)
    include(cmake/WebBuild.cmake)
else()
    target_link_libraries(little_woodland PRIVATE glfw glad_gl_core_33 OpenGL::GL)
endif()

include(CTest)
if(BUILD_TESTING AND NOT EMSCRIPTEN)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME web_distribution_validation COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/WebTests.py)
        add_test(NAME release_audio_provenance_gate COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ReleaseTests.py)
    endif()
    add_executable(feedback_tests tests/FeedbackTests.cpp)
    target_link_libraries(feedback_tests PRIVATE woodland_presentation)
    add_test(NAME audio_feedback_and_frame_timing COMMAND feedback_tests)
    add_executable(audio_tests tests/AudioTests.cpp)
    target_include_directories(audio_tests SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio)
    target_compile_definitions(audio_tests PRIVATE WOODLAND_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    target_link_libraries(audio_tests PRIVATE woodland_audio)
    add_test(NAME wav_loading_and_fallback COMMAND audio_tests)
    add_executable(result_tests tests/ResultTests.cpp)
    target_link_libraries(result_tests PRIVATE woodland_presentation)
    add_test(NAME storybook_results COMMAND result_tests)
    add_executable(texture_tests tests/TextureTests.cpp src/rendering/Texture.cpp src/rendering/FrameAnimation.cpp)
    target_include_directories(texture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_include_directories(texture_tests SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb)
    target_compile_definitions(texture_tests PRIVATE WOODLAND_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    target_link_libraries(texture_tests PRIVATE glfw glad_gl_core_33 OpenGL::GL)
    add_test(NAME png_textures COMMAND texture_tests)
    set_tests_properties(png_textures PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(level_tests tests/LevelTests.cpp)
    target_link_libraries(level_tests PRIVATE woodland_presentation)
    add_test(NAME letter_levels COMMAND level_tests)
    add_executable(board_tests tests/BoardTests.cpp)
    target_link_libraries(board_tests PRIVATE woodland_puzzle)
    add_test(NAME board_invariants COMMAND board_tests)
    add_executable(gameplay_tests tests/GameplayTests.cpp)
    target_link_libraries(gameplay_tests PRIVATE woodland_puzzle)
    add_test(NAME gameplay_rules COMMAND gameplay_tests)
    add_executable(playback_tests tests/PlaybackTests.cpp)
    target_link_libraries(playback_tests PRIVATE woodland_presentation)
    add_test(NAME resolution_playback COMMAND playback_tests)
endif()

if(NOT EMSCRIPTEN)
# Binary-only, relocatable installation. The portable build excludes source paths.
set_target_properties(little_woodland PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
install(TARGETS little_woodland RUNTIME DESTINATION bin)
install(DIRECTORY assets/ DESTINATION share/little-woodland/assets
    PATTERN "audio" EXCLUDE PATTERN "*.gif" EXCLUDE PATTERN "source.json" EXCLUDE)
install(FILES assets/audio/SOURCES.md DESTINATION share/little-woodland)
install(FILES RELEASE.md DESTINATION . RENAME README.md)
install(FILES third_party/stb/LICENSE DESTINATION share/little-woodland/licenses RENAME stb.txt)
install(FILES third_party/miniaudio/LICENSE DESTINATION share/little-woodland/licenses RENAME miniaudio.txt)
install(FILES "${glad_SOURCE_DIR}/LICENSE" DESTINATION share/little-woodland/licenses RENAME GLAD.txt)
find_library(WOODLAND_GLFW_RUNTIME NAMES glfw)
if(NOT WOODLAND_GLFW_RUNTIME)
    message(FATAL_ERROR "GLFW runtime library not found for installation")
endif()
get_filename_component(WOODLAND_GLFW_REAL "${WOODLAND_GLFW_RUNTIME}" REALPATH)
install(FILES "${WOODLAND_GLFW_REAL}" DESTINATION lib RENAME libglfw.so.3)
install(FILES third_party/GLFW-LICENSE.txt DESTINATION share/little-woodland/licenses)

set(WOODLAND_AUDIO_PROVENANCE "" CACHE FILEPATH "Optional legacy audio checksum/category JSON (SOURCES.md documents licensing)")
if(WOODLAND_RELEASE_WITH_AUDIO)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/validate_audio_release.py"
        "${CMAKE_SOURCE_DIR}/assets" "${WOODLAND_AUDIO_PROVENANCE}" RESULT_VARIABLE audio_review)
    if(NOT audio_review EQUAL 0)
        message(FATAL_ERROR "Audio release blocked: missing or invalid WAVs, integrity mismatch, or missing SOURCES.md.")
    endif()
    # Recheck at packaging/install time as well, so deleted/replaced WAVs cannot slip through.
    install(CODE "execute_process(COMMAND \"${Python3_EXECUTABLE}\" \"${CMAKE_SOURCE_DIR}/tools/validate_audio_release.py\" \"${CMAKE_SOURCE_DIR}/assets\" \"${WOODLAND_AUDIO_PROVENANCE}\" RESULT_VARIABLE review)
if(NOT review EQUAL 0)
message(FATAL_ERROR \"Audio packaging format/integrity check failed\")
endif()")
    foreach(sound tile_click success_tile_match tiles_falling letter_delivered victory)
        install(FILES "assets/audio/mixkit/${sound}.wav" DESTINATION share/little-woodland/assets/audio/mixkit)
    endforeach()
    set(WOODLAND_PACKAGE_FLAVOR "audio")
else()
    set(WOODLAND_PACKAGE_FLAVOR "silent")
endif()
set(CPACK_GENERATOR TGZ)
set(CPACK_PACKAGE_NAME little-woodland)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "little-woodland-${PROJECT_VERSION}-Linux-${CMAKE_SYSTEM_PROCESSOR}-${WOODLAND_PACKAGE_FLAVOR}")
set(CPACK_SET_DESTDIR OFF)
set(CPACK_PACKAGING_INSTALL_PREFIX "/")
set(CPACK_STRIP_FILES TRUE)
include(CPack)

endif()
