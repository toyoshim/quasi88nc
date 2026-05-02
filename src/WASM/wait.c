#include <SDL.h>
#include <emscripten.h>

#include "quasi88.h"
#include "initval.h"
#include "wait.h"
#include "suspend.h"
#include "screen.h"

static int skip_counter = 0;
static const int skip_count_max = 15;

static int wait_counter = 0;
static const int wait_count_max = 10;

typedef Sint64 T_WAIT_TICK;
static T_WAIT_TICK next_time;
static T_WAIT_TICK delta_time;

#define GET_TICK() ((T_WAIT_TICK)SDL_GetTicks() * 1000)

int wait_vsync_init(void)
{
  if (!SDL_WasInit(SDL_INIT_TIMER)) {
    if (SDL_InitSubSystem(SDL_INIT_TIMER) != 0) {
      if (verbose_wait) printf("Error Wait (SDL): %s\n", SDL_GetError());
      return FALSE;
    }
  }
  wait_vsync_reset();
  return TRUE;
}

void wait_vsync_term(void) {}

void wait_vsync_reset(void)
{
  wait_counter = 0;
  delta_time = (T_WAIT_TICK)(1000000.0 / (CONST_VSYNC_FREQ * wait_rate / 100));
  next_time = GET_TICK() + delta_time;
}

void wait_vsync(void)
{
  T_WAIT_TICK diff_ms = (next_time - GET_TICK()) / 1000;
  int on_time = FALSE;

  if (diff_ms > 0) {
    emscripten_sleep((int)diff_ms);
    on_time = TRUE;
  } else {
    /* Behind schedule: still yield once so the browser can render and
     * deliver input events. emscripten_sleep(0) is the standard ASYNCIFY
     * yield primitive. */
    emscripten_sleep(0);
  }

  next_time += delta_time;

  if (on_time) {
    wait_counter = 0;
  } else {
    if (++wait_counter >= wait_count_max) {
      wait_vsync_reset();
    }
  }

  if (use_auto_skip) {
    if (on_time) {
      skip_counter = 0;
      do_skip_draw = FALSE;
      if (already_skip_draw) {
        already_skip_draw = FALSE;
        reset_frame_counter();
      }
    } else {
      if (++skip_counter >= skip_count_max) {
        skip_counter = 0;
        do_skip_draw = FALSE;
        already_skip_draw = FALSE;
        reset_frame_counter();
      } else {
        do_skip_draw = TRUE;
      }
    }
  }
}

void wait_menu(void)
{
  emscripten_sleep(20);
}
