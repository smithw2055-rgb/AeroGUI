#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include "stb_image.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
