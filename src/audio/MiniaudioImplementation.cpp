#if AERO_AUDIO_MINIAUDIO
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_GENERATION

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#include <miniaudio.h>

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif
