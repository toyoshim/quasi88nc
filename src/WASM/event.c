#include <SDL.h>
#include <emscripten.h>
#include <stdlib.h>
#include <string.h>

#include "quasi88.h"
#include "keyboard.h"
#include "graph.h"
#include "drive.h"
#include "emu.h"
#include "device.h"
#include "screen.h"
#include "event.h"

int  use_cmdkey      = 1;
int  keyboard_type   = 1;
char *file_keyboard  = NULL;
int  use_joydevice   = TRUE;

/* SDL2 → KEY88 keycode translation.
 * Keeps the table small by switching on the SDL_Keycode value directly.
 * Returns KEY88_INVALID for unmapped keys. */
static int sdlkey_to_key88(SDL_Keycode k)
{
  switch (k) {
    /* Letters: SDLK_a..SDLK_z are ASCII 'a'..'z'; KEY88 wants uppercase. */
    case SDLK_a: return KEY88_A;
    case SDLK_b: return KEY88_B;
    case SDLK_c: return KEY88_C;
    case SDLK_d: return KEY88_D;
    case SDLK_e: return KEY88_E;
    case SDLK_f: return KEY88_F;
    case SDLK_g: return KEY88_G;
    case SDLK_h: return KEY88_H;
    case SDLK_i: return KEY88_I;
    case SDLK_j: return KEY88_J;
    case SDLK_k: return KEY88_K;
    case SDLK_l: return KEY88_L;
    case SDLK_m: return KEY88_M;
    case SDLK_n: return KEY88_N;
    case SDLK_o: return KEY88_O;
    case SDLK_p: return KEY88_P;
    case SDLK_q: return KEY88_Q;
    case SDLK_r: return KEY88_R;
    case SDLK_s: return KEY88_S;
    case SDLK_t: return KEY88_T;
    case SDLK_u: return KEY88_U;
    case SDLK_v: return KEY88_V;
    case SDLK_w: return KEY88_W;
    case SDLK_x: return KEY88_X;
    case SDLK_y: return KEY88_Y;
    case SDLK_z: return KEY88_Z;

    case SDLK_0: return KEY88_0;
    case SDLK_1: return KEY88_1;
    case SDLK_2: return KEY88_2;
    case SDLK_3: return KEY88_3;
    case SDLK_4: return KEY88_4;
    case SDLK_5: return KEY88_5;
    case SDLK_6: return KEY88_6;
    case SDLK_7: return KEY88_7;
    case SDLK_8: return KEY88_8;
    case SDLK_9: return KEY88_9;

    case SDLK_SPACE:        return KEY88_SPACE;
    case SDLK_MINUS:        return KEY88_MINUS;
    case SDLK_EQUALS:       return KEY88_CARET;
    case SDLK_LEFTBRACKET:  return KEY88_AT;
    case SDLK_RIGHTBRACKET: return KEY88_BRACKETLEFT;
    case SDLK_BACKSLASH:    return KEY88_BRACKETRIGHT;
    case SDLK_SEMICOLON:    return KEY88_SEMICOLON;
    case SDLK_QUOTE:        return KEY88_COLON;
    case SDLK_COMMA:        return KEY88_COMMA;
    case SDLK_PERIOD:       return KEY88_PERIOD;
    case SDLK_SLASH:        return KEY88_SLASH;
    case SDLK_BACKQUOTE:    return KEY88_UNDERSCORE;

    case SDLK_RETURN:    return KEY88_RETURN;
    case SDLK_BACKSPACE: return KEY88_BS;
    case SDLK_TAB:       return KEY88_TAB;
    case SDLK_ESCAPE:    return KEY88_ESC;
    case SDLK_DELETE:    return KEY88_DEL;
    case SDLK_INSERT:    return KEY88_INS;
    case SDLK_HOME:      return KEY88_HOME;
    case SDLK_END:       return KEY88_HELP;
    case SDLK_PAGEUP:    return KEY88_ROLLUP;
    case SDLK_PAGEDOWN:  return KEY88_ROLLDOWN;

    case SDLK_UP:    return KEY88_UP;
    case SDLK_DOWN:  return KEY88_DOWN;
    case SDLK_LEFT:  return KEY88_LEFT;
    case SDLK_RIGHT: return KEY88_RIGHT;

    case SDLK_LSHIFT: return KEY88_SHIFT;
    case SDLK_RSHIFT: return KEY88_SHIFT;
    case SDLK_LCTRL:  return KEY88_CTRL;
    case SDLK_RCTRL:  return KEY88_CTRL;
    case SDLK_LALT:   return KEY88_GRAPH;
    case SDLK_RALT:   return KEY88_KANA;
    case SDLK_CAPSLOCK: return KEY88_CAPS;

    case SDLK_F1:  return KEY88_F1;
    case SDLK_F2:  return KEY88_F2;
    case SDLK_F3:  return KEY88_F3;
    case SDLK_F4:  return KEY88_F4;
    case SDLK_F5:  return KEY88_F5;
    case SDLK_F6:  return KEY88_F6;
    case SDLK_F7:  return KEY88_F7;
    case SDLK_F8:  return KEY88_F8;
    case SDLK_F9:  return KEY88_F9;
    case SDLK_F10: return KEY88_F10;
    case SDLK_F11: return KEY88_F11;
    case SDLK_F12: return KEY88_F12;

    case SDLK_KP_0: return KEY88_KP_0;
    case SDLK_KP_1: return KEY88_KP_1;
    case SDLK_KP_2: return KEY88_KP_2;
    case SDLK_KP_3: return KEY88_KP_3;
    case SDLK_KP_4: return KEY88_KP_4;
    case SDLK_KP_5: return KEY88_KP_5;
    case SDLK_KP_6: return KEY88_KP_6;
    case SDLK_KP_7: return KEY88_KP_7;
    case SDLK_KP_8: return KEY88_KP_8;
    case SDLK_KP_9: return KEY88_KP_9;
    case SDLK_KP_PERIOD:   return KEY88_KP_PERIOD;
    case SDLK_KP_DIVIDE:   return KEY88_KP_DIVIDE;
    case SDLK_KP_MULTIPLY: return KEY88_KP_MULTIPLY;
    case SDLK_KP_MINUS:    return KEY88_KP_SUB;
    case SDLK_KP_PLUS:     return KEY88_KP_ADD;
    case SDLK_KP_ENTER:    return KEY88_RETURN;
    case SDLK_KP_EQUALS:   return KEY88_KP_EQUAL;
    case SDLK_KP_COMMA:    return KEY88_KP_COMMA;

    default: return KEY88_INVALID;
  }
}

void event_handle_init(void) { /* nothing to bind on the web */ }
void event_init(void)        { /* per-mode init: nothing to do */ }

/* event_handle() runs once per VSYNC tick. The Z80's BIOS keyboard
 * scan only reliably registers a press when the matrix has been "down"
 * for several consecutive frames — a single 1-frame down/up pulse can
 * be missed entirely by the scan timing.
 *
 * Strategy:
 *   - On KEYDOWN, press immediately and pin the state down for at least
 *     MIN_HOLD_FRAMES.
 *   - On KEYUP, defer the release: it fires once the hold counter
 *     expires.
 *   - A new KEYDOWN during the hold simply refreshes the counter and
 *     cancels the pending release (rapid same-key taps coalesce into
 *     one held press, which is what real keyboards do under repeat).
 */
#define MIN_HOLD_FRAMES 3

static unsigned char hold_remaining [KEY88_END];
static unsigned char pending_release[KEY88_END];

void event_handle(void)
{
  SDL_Event E;
  int key88, x, y, i;

  /* Mode-transition hook:
   *  - Pause/resume SDL audio so a paused emulator outputs silence
   *    cleanly instead of a draining ring buffer.
   *  - When leaving the menu, persist any settings the user changed
   *    to ~/.quasi88/quasi88.rc (QUASI88's core never writes this file
   *    itself; on the web target this is the only way settings carry
   *    across reloads). */
  {
    extern void quasi88_save_config(void);
    static int prev_mode = -1;
    int cur_mode = get_emu_mode();
    if (cur_mode != prev_mode) {
      SDL_PauseAudio(cur_mode == EXEC ? 0 : 1);
      if (prev_mode == MENU) quasi88_save_config();
      prev_mode = cur_mode;
    }
  }

  /* Tick down hold counters; release any key whose hold has expired. */
  for (i = 0; i < KEY88_END; i++) {
    if (hold_remaining[i]) hold_remaining[i]--;
    if (hold_remaining[i] == 0 && pending_release[i]) {
      pc88_key(i, 0);
      pending_release[i] = 0;
    }
  }

  while (SDL_PollEvent(&E)) {
    switch (E.type) {

    case SDL_KEYDOWN:
    case SDL_KEYUP:
      key88 = sdlkey_to_key88(E.key.keysym.sym);
      if (key88 == KEY88_INVALID || key88 < 0 || key88 >= KEY88_END) break;

      if (E.type == SDL_KEYDOWN) {
        pc88_key(key88, 1);
        hold_remaining [key88] = MIN_HOLD_FRAMES;
        pending_release[key88] = 0;
      } else {
        pending_release[key88] = 1;
      }
      break;

    case SDL_MOUSEMOTION:
      if (mouse_rel_move) {
        pc88_mouse_moved_rel(E.motion.xrel, E.motion.yrel);
      } else {
        x = E.motion.x - SCREEN_DX;
        y = E.motion.y - SCREEN_DY;
        if (now_screen_size == SCREEN_SIZE_HALF) { x *= 2; y *= 2; }
#ifdef SUPPORT_DOUBLE
        else if (now_screen_size == SCREEN_SIZE_DOUBLE) { x /= 2; y /= 2; }
#endif
        pc88_mouse_moved_abs(x, y);
      }
      break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
      int down = (E.type == SDL_MOUSEBUTTONDOWN);
      switch (E.button.button) {
        case SDL_BUTTON_LEFT:   pc88_mouse(KEY88_MOUSE_L, down); break;
        case SDL_BUTTON_MIDDLE:
          /* Middle click toggles into the menu mode. The web UI also
           * exposes a button for this via SDL_USEREVENT below. */
          if (down) set_emu_mode(MENU);
          break;
        case SDL_BUTTON_RIGHT:  pc88_mouse(KEY88_MOUSE_R, down); break;
        default: break;
      }
      break;
    }

    case SDL_USEREVENT:
      /* JS-injected: code field selects the action. 0 = enter menu. */
      if (E.user.code == 0) set_emu_mode(MENU);
      break;

    case SDL_QUIT:
      pc88_quit();
      break;

    default:
      break;
    }
  }
}

void init_mouse_position(int *x, int *y)
{
  if (x) *x = WIDTH / 2;
  if (y) *y = HEIGHT / 2;
}

int  numlock_on(void)  { return TRUE; }
void numlock_off(void) { /* numlock toggle not exposed via web */ }

int joy_init(void)            { return FALSE; }
int joystick_available(void)  { return FALSE; }

/* Status bar shows what key triggers the menu. On the web build the menu
 * is reachable via middle-click or the JS-side button (SDL_USEREVENT). */
const char *get_keysym_menu(void) { return "MIDDLE-CLICK"; }

/* Called from JS (web/app.js MENU button) to enter menu mode. Posting an
 * SDL_USEREVENT keeps emulator state changes on the SDL event thread,
 * matching how middle-click and other inputs flow. */
EMSCRIPTEN_KEEPALIVE
void quasi88_post_menu_event(void)
{
  SDL_Event e;
  memset(&e, 0, sizeof(e));
  e.type = SDL_USEREVENT;
  e.user.code = 0;
  SDL_PushEvent(&e);
}
