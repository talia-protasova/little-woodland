# stb_image

PNG-only decoder from https://github.com/nothings/stb
Revision: `003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5`

`stb_image.h` is vendored unchanged. Its implementation is compiled once in
`src/rendering/Texture.cpp` with `STBI_ONLY_PNG`. See LICENSE for the upstream
MIT / public-domain license options. No additional download is needed at build time.
