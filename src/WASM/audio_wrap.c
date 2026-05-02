/*
 * Wrap SDL_OpenAudio so the audio callback always sees a silence-filled
 * output buffer before the original callback runs. The shared
 * sdl_fill_sound() in src/snddrv/quasi88-SDL/sdl.c writes only the
 * bytes for which it has samples (sample.amountRead), leaving the rest
 * of SDL's output buffer untouched. When the emulation is paused
 * (menu/pause/monitor) or temporarily can't keep up (menu rendering
 * spikes), those untouched bytes are stale audio — heard as crackle.
 *
 * We can't change the upstream callback without modifying shared src/,
 * so instead we install a wrapper callback at SDL_OpenAudio time that
 * memsets stream to 0 and then invokes the original. Partial fills end
 * up padded with silence; full underruns produce clean silence.
 *
 * Built with -Wl,--wrap=SDL_OpenAudio in Makefile.wasm.
 */

#include <SDL.h>
#include <string.h>

extern int __real_SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);

/* Saved pointer to the callback installed by sdl.c. */
static SDL_AudioCallback original_callback = NULL;
static void *original_userdata = NULL;

static void silent_pad_callback(void *userdata, Uint8 *stream, int len)
{
  /* Web Audio expects either silence or valid samples — stale bytes
   * from the previous buffer cycle become audible noise. */
  memset(stream, 0, (size_t)len);
  if (original_callback) original_callback(original_userdata, stream, len);
}

int __wrap_SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained)
{
  if (desired && desired->callback) {
    original_callback = desired->callback;
    original_userdata = desired->userdata;
    desired->callback = silent_pad_callback;
    desired->userdata = NULL;  /* userdata flows through original_userdata */
  }
  return __real_SDL_OpenAudio(desired, obtained);
}
