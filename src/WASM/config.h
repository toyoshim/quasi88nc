#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

/*----------------------------------------------------------------------*/
/* WASM/Emscripten + SDL2                                              */
/*----------------------------------------------------------------------*/

#include <SDL.h>

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define LSB_FIRST
#else
#undef LSB_FIRST
#endif

#ifndef QUASI88_SDL
#define QUASI88_SDL
#endif

#define Q_COMMENT "WASM version"

/* Web canvas / WebGL is natively 32bpp ARGB. Avoid software conversion. */
#ifndef SUPPORT_32BPP
#define SUPPORT_32BPP
#endif
#undef SUPPORT_8BPP
#undef SUPPORT_16BPP

#endif  /* CONFIG_H_INCLUDED */
