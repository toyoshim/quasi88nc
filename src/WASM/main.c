#include <stdio.h>
#include <stdlib.h>

#include "quasi88.h"
#include "initval.h"
#include "file-op.h"
#include "getconf.h"
#include "device.h"
#include "snddrv.h"
#include "suspend.h"
#include "menu.h"

static void the_end(void);

int main(int argc, char *argv[])
{
  sdl_system_init();
  xmame_system_init();

  quasi88_atexit(the_end);

  if (osd_environment()) {
    if (config_init(argc, argv)) {
      quasi88();
    }
  }

  xmame_system_term();
  sdl_system_term();
  return 0;
}

static void the_end(void)
{
  xmame_system_term();
  sdl_system_term();
}

int stateload_system(void) { return TRUE; }
int statesave_system(void) { return TRUE; }

static int about_lang;
static int about_line;

int about_msg_init(int japanese)
{
  about_lang = japanese ? 1 : 0;
  about_line = 0;
  return -1;
}

const char *about_msg(void) { return NULL; }
