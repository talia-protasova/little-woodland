// Compile the vendored implementation in exactly one translation unit. CMake
// applies the same feature exclusions to this unit and Audio.cpp; the public
// Audio facade hides miniaudio types from gameplay and rendering headers.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
