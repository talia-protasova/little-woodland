#pragma once
// GL objects retain the same RAII owners on desktop GL and browser WebGL 2.
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/gl.h>
#endif
