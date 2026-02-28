//#define STBI_ASSERT(x)
//#define STBI_MALLOC(size)
//#define STBI_REALLOC(ptr, newSize)
//#define STBI_FREE(ptr)

#define STBI_NO_STDIO

#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"