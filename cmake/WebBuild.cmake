# Emscripten 3.1.64; no desktop GLFW, GLAD, audio backend or pthreads required.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(WOODLAND_WEB_DIST "${CMAKE_SOURCE_DIR}/dist/web" CACHE PATH "Static browser distribution")
set(WOODLAND_WEB_ASSETS "${CMAKE_BINARY_DIR}/web-assets")
file(GLOB_RECURSE public_assets CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/tiles/*.png"
    "${CMAKE_SOURCE_DIR}/assets/fonts/*.png"
    "${CMAKE_SOURCE_DIR}/assets/characters/*.png"
    "${CMAKE_SOURCE_DIR}/assets/characters/animations/*/frames.txt"
    "${CMAKE_SOURCE_DIR}/assets/backgrounds/*.png")
add_custom_command(OUTPUT "${WOODLAND_WEB_ASSETS}/inventory.json"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/prepare_web_assets.py"
        "${CMAKE_SOURCE_DIR}/assets" "${WOODLAND_WEB_ASSETS}"
    DEPENDS ${public_assets} "${CMAKE_SOURCE_DIR}/tools/prepare_web_assets.py"
    VERBATIM)
add_custom_target(web_assets DEPENDS "${WOODLAND_WEB_ASSETS}/inventory.json")
add_dependencies(little_woodland web_assets)
set_target_properties(little_woodland PROPERTIES SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${WOODLAND_WEB_DIST}"
    LINK_DEPENDS "${WOODLAND_WEB_ASSETS}/inventory.json")
# The existing renderer uses a ~1 MiB stack scratch array; the SDK default is 64 KiB.
target_link_options(little_woodland PRIVATE
    -fexceptions "-sUSE_GLFW=3" "-sMIN_WEBGL_VERSION=2" "-sMAX_WEBGL_VERSION=2"
    "-sSTACK_SIZE=4194304" "-sALLOW_MEMORY_GROWTH=1" "-sINITIAL_MEMORY=134217728" "-sMAXIMUM_MEMORY=536870912"
    "-sENVIRONMENT=web" "-sEXIT_RUNTIME=1" "-sFILESYSTEM=1"
    "--preload-file" "${WOODLAND_WEB_ASSETS}/assets@/assets")
# Copy the shell on every build, including after edits that do not relink C++.
add_custom_target(web_distribution ALL
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/web/index.html" "${WOODLAND_WEB_DIST}/index.html"
    COMMAND ${CMAKE_COMMAND} -E copy "${WOODLAND_WEB_ASSETS}/inventory.json" "${WOODLAND_WEB_DIST}/inventory.json"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/web/notices" "${WOODLAND_WEB_DIST}/notices"
    COMMAND ${CMAKE_COMMAND} -E touch "${WOODLAND_WEB_DIST}/.nojekyll"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/validate_web_dist.py" "${WOODLAND_WEB_DIST}"
    DEPENDS little_woodland VERBATIM)
