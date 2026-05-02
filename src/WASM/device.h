#ifndef DEVICE_H_INCLUDED
#define DEVICE_H_INCLUDED

#include <SDL.h>

extern int SCREEN_DX;
extern int SCREEN_DY;
extern int status_fg;
extern int status_bg;

extern int use_hwsurface;
extern int use_doublebuf;
extern int use_swcursor;
extern int mouse_rel_move;
extern int use_cmdkey;
extern int keyboard_type;
extern char *file_keyboard;
extern int use_joydevice;

void sdl_system_init(void);
void sdl_system_term(void);
int  joy_init(void);

#endif  /* DEVICE_H_INCLUDED */
