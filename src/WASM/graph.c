#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "quasi88.h"
#include "initval.h"
#include "device.h"
#include "graph.h"
#include "screen.h"
#include "emu.h"

#define BIT_OF_DEPTH   32
#define SIZE_OF_DEPTH  4

/* Globals declared in device.h ---------------------------------------------*/
int SCREEN_DX = 0;
int SCREEN_DY = 0;
int status_fg = 0x000000;
int status_bg = 0xd6d6d6;
int use_hwsurface  = 1;
int use_doublebuf  = 0;
int use_swcursor   = 1;
int mouse_rel_move = 0;

/* Globals declared in graph.h ----------------------------------------------*/
int now_screen_size   = SCREEN_SIZE_FULL;
int now_status        = 0;
int now_half_interp   = 0;
int now_fullscreen    = 0;
int enable_fullscreen = 0;

/* SDL2 objects -------------------------------------------------------------*/
static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture  *texture  = NULL;

/* Buffer layout: WIDTH x (HEIGHT + STATUS_HEIGHT) of 32bpp ARGB.
 * `screen_buf` owns the entire buffer; `screen_start` and `status_buf`
 * point into it. Texture height is fixed; status rows always exist
 * even when not displayed (just ignored on copy).
 */
static int buf_h = 0;

#define SET_STATUS_COLOR(n, r, g, b) \
    status_pixel[(n)] = 0xFF000000u | ((Uint32)(r) << 16) | ((Uint32)(g) << 8) | (Uint32)(b)


void sdl_system_init(void)
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
  }
}

void sdl_system_term(void)
{
  SDL_Quit();
}

int graphic_system_init(void)
{
  DEPTH = BIT_OF_DEPTH;

  /* Use the standard 640x400 layout. PC-8801 native resolution is 640x400;
   * the canvas is fixed via index.html's <canvas> element. We don't need
   * SCREEN_SIZE_DOUBLE/HALF to vary the buffer size for the web target. */
  WIDTH    = 640;
  HEIGHT   = 400;
  SCREEN_W = WIDTH;
  SCREEN_H = HEIGHT;
  SCREEN_DX = 0;
  SCREEN_DY = 0;

  buf_h = HEIGHT + STATUS_HEIGHT;

  if (!SDL_WasInit(SDL_INIT_VIDEO)) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
      fprintf(stderr, "SDL_InitSubSystem(VIDEO): %s\n", SDL_GetError());
      return 0;
    }
  }

  window = SDL_CreateWindow(Q_TITLE,
                            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                            WIDTH, buf_h, 0);
  if (!window) {
    fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
    return 0;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    /* Fallback to software renderer; under Emscripten this still maps
     * to the canvas via SDL2's internal blit. */
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!renderer) {
    fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
    return 0;
  }

  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING, WIDTH, buf_h);
  if (!texture) {
    fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
    return 0;
  }

  screen_buf = (char *)calloc(WIDTH * buf_h, SIZE_OF_DEPTH);
  if (!screen_buf) return 0;

  screen_start = screen_buf;

  /* Status bar layout (matches the SDL platform's 1:3:1 split). */
  status_sx[0] = WIDTH / 5;
  status_sx[1] = WIDTH - status_sx[0] * 2;
  status_sx[2] = WIDTH / 5;
  status_sy[0] = status_sy[1] = status_sy[2] = STATUS_HEIGHT - 3;

  status_buf = &screen_buf[WIDTH * HEIGHT * SIZE_OF_DEPTH];
  status_start[0] = status_buf + 3 * (WIDTH * SIZE_OF_DEPTH);
  status_start[1] = status_start[0] + (status_sx[0] * SIZE_OF_DEPTH);
  status_start[2] = status_start[1] + (status_sx[1] * SIZE_OF_DEPTH);

  SET_STATUS_COLOR(STATUS_BG,   (status_bg >> 16) & 0xff, (status_bg >> 8) & 0xff, status_bg & 0xff);
  SET_STATUS_COLOR(STATUS_FG,   (status_fg >> 16) & 0xff, (status_fg >> 8) & 0xff, status_fg & 0xff);
  SET_STATUS_COLOR(STATUS_BLACK, 0x00, 0x00, 0x00);
  SET_STATUS_COLOR(STATUS_WHITE, 0xff, 0xff, 0xff);
  SET_STATUS_COLOR(STATUS_RED,   0xff, 0x00, 0x00);
  SET_STATUS_COLOR(STATUS_GREEN, 0x00, 0xff, 0x00);

  black_pixel = 0xFF000000u;

  set_mouse_state();
  set_half_interp();

  now_screen_size = SCREEN_SIZE_FULL;
  now_fullscreen  = 0;
  now_status      = show_status;

  return 1;
}

void graphic_system_term(void)
{
  if (texture)  { SDL_DestroyTexture(texture);   texture = NULL; }
  if (renderer) { SDL_DestroyRenderer(renderer); renderer = NULL; }
  if (window)   { SDL_DestroyWindow(window);     window = NULL; }
  if (screen_buf) { free(screen_buf); screen_buf = NULL; screen_start = NULL; }
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

int graphic_system_restart(void)
{
  /* Resolution is fixed on web; nothing to do beyond noting state. */
  now_status = show_status;
  return 1;
}

void trans_palette(SYSTEM_PALETTE_T syspal[])
{
  int i, j;
  for (i = 0; i < 16; i++) {
    color_pixel[i] = 0xFF000000u
                   | ((Uint32)syspal[i].red   << 16)
                   | ((Uint32)syspal[i].green <<  8)
                   |  (Uint32)syspal[i].blue;
  }
  if (now_half_interp) {
    for (i = 0; i < 16; i++) {
      for (j = 0; j < 16; j++) {
        Uint32 r = ((syspal[i].red   + syspal[j].red)   >> 1) & 0xff;
        Uint32 g = ((syspal[i].green + syspal[j].green) >> 1) & 0xff;
        Uint32 b = ((syspal[i].blue  + syspal[j].blue)  >> 1) & 0xff;
        color_half_pixel[i][j] = 0xFF000000u | (r << 16) | (g << 8) | b;
      }
    }
  }
}

void put_image_all(void)
{
  if (!texture || !renderer || !screen_buf) return;

  SDL_UpdateTexture(texture, NULL, screen_buf, WIDTH * SIZE_OF_DEPTH);

  int dst_h = HEIGHT + (now_status ? STATUS_HEIGHT : 0);
  SDL_Rect src = { 0, 0, WIDTH, dst_h };
  SDL_Rect dst = { 0, 0, WIDTH, dst_h };

  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, &src, &dst);
  SDL_RenderPresent(renderer);
}

void put_image(int x0, int y0, int x1, int y1, int st0, int st1, int st2)
{
  /* Partial updates are only useful for software blit; with a streaming
   * GPU texture and Emscripten's batched canvas commit, full uploads are
   * actually faster than per-rect texture updates. */
  (void)x0; (void)y0; (void)x1; (void)y1;
  (void)st0; (void)st1; (void)st2;
  put_image_all();
}

int set_status_window(void)
{
  if (now_status == show_status) return 0;
  now_status = show_status;
  return 1;
}

int set_half_interp(void)
{
  /* Half-resolution interpolation is unused on the web target. */
  now_half_interp = 0;
  return now_half_interp;
}

void set_window_title(const char *title)
{
  if (window) SDL_SetWindowTitle(window, title);
}

int set_mouse_state(void)
{
  int mouse;
  if (get_emu_mode() == EXEC) {
    mouse = hide_mouse ? 0 : 1;
  } else {
    mouse = 1;
  }
  SDL_ShowCursor(mouse ? SDL_ENABLE : SDL_DISABLE);
  mouse_rel_move = 0;
  return mouse;
}
