/*
 * Persist menu-modified runtime settings into ~/.quasi88/quasi88.rc.
 *
 * QUASI88's core only ever reads quasi88.rc; it never writes one back.
 * On native UNIX this is fine because the user pre-edits the file. On
 * the web target there's no other way to keep settings across reloads,
 * so we generate the file ourselves on every menu→EXEC transition.
 *
 * The file is a list of "-name [value]" lines exactly as the existing
 * option parser in src/SDL/getconf.c expects, so the next startup
 * picks the saved values back up via the normal cfg load path
 * (alloc_global_cfgname + get_config_file).
 *
 * IDBFS persists the file because /quasi88/.quasi88/quasi88.rc lives
 * inside the autoPersist mount.
 *
 * The set of options written is intentionally narrow: things that
 * meaningfully affect the running emulator and are exposed in the
 * menu. Key bindings and function-key assignments are skipped (they'd
 * need string formatting). Add to write_config() as needed.
 */

#include <stdio.h>
#include <string.h>

#include "quasi88.h"
#include "initval.h"
#include "file-op.h"
#include "memory.h"
#include "pc88main.h"
#include "soundbd.h"
#include "fdc.h"
#include "screen.h"
#include "keyboard.h"
#include "menu.h"
#include "snapshot.h"
#include "snddrv.h"
#include "mame-quasi88.h"

/* Globals not declared in any header. */
extern int    boost;
extern int    wait_rate;
extern int    no_wait;
extern int    frameskip_rate;
extern int    use_auto_skip;
extern int    screen_size;
extern int    use_interlace;
extern int    use_half_interp;
extern int    show_status;
extern int    hide_mouse;
extern int    grab_mouse;
extern int    cpu_timing;
extern int    CPU_1_COUNT;
extern int    cpu_slice_us;
extern double cpu_clock_mhz;
extern int    use_sound;
extern int    attenuation;
extern int    keyboard_type;  /* declared in src/WASM/device.h */

static void p_int(FILE *fp, const char *name, int v) {
  fprintf(fp, "-%s\t%d\n", name, v);
}
static void p_dbl(FILE *fp, const char *name, double v) {
  fprintf(fp, "-%s\t%g\n", name, v);
}
static void p_flag(FILE *fp, const char *on, const char *off, int v) {
  fprintf(fp, "-%s\n", v ? on : off);
}

void quasi88_save_config(void)
{
  const char *dir = osd_dir_gcfg();
  if (!dir) return;

  char path[OSD_MAX_FILENAME];
  if ((int)snprintf(path, sizeof(path), "%s/quasi88.rc", dir) >= (int)sizeof(path))
    return;

  FILE *fp = fopen(path, "w");
  if (!fp) return;

  fprintf(fp, "# Auto-saved by QUASI88 WASM on menu exit. Edits will be overwritten.\n\n");

  /* PC-8801 boot-time config -------------------------------------------*/
  switch (boot_basic) {
    case BASIC_N:   fputs("-n\n",   fp); break;
    case BASIC_V1S: fputs("-v1s\n", fp); break;
    case BASIC_V1H: fputs("-v1h\n", fp); break;
    case BASIC_V2:  fputs("-v2\n",  fp); break;
  }
  p_flag(fp, "4mhz",   "8mhz",   boot_clock_4mhz);
  p_flag(fp, "sd2",    "sd",     sound_board == SOUND_II);
  p_int (fp, "dipsw",  boot_dipsw);
  p_flag(fp, "pcg",    "nopcg",  use_pcg);
  p_int (fp, "extram", use_extram);
  p_flag(fp, "jisho",  "nojisho", use_jisho_rom);
  p_flag(fp, "analog", "digital", monitor_analog);
  fprintf(fp, "-%s\n", monitor_15k ? "15k" : "24k");
  switch (mouse_mode) {
    case 0: fputs("-nomouse\n",  fp); break;
    case 1: fputs("-mouse\n",    fp); break;
    case 2: fputs("-joymouse\n", fp); break;
    case 3: fputs("-joystick\n", fp); break;
  }

  /* Emulation timing ---------------------------------------------------*/
  p_int (fp, "cpu",       cpu_timing);
  p_int (fp, "cpu1count", CPU_1_COUNT);
  p_int (fp, "cpu2us",    cpu_slice_us);
  p_dbl (fp, "clock",     cpu_clock_mhz);
  p_int (fp, "speed",     wait_rate);
  p_flag(fp, "nowait",    "wait",       no_wait);
  p_flag(fp, "fdc_wait",  "fdc_nowait", fdc_wait);
  p_flag(fp, "hsbasic",   "nohsbasic",  highspeed_mode);
  p_flag(fp, "mem_wait",  "mem_nowait", memory_wait);
  p_int (fp, "boost",     boost);

  /* Display ------------------------------------------------------------*/
  p_int (fp, "frameskip", frameskip_rate);
  p_flag(fp, "autoskip",  "noautoskip", use_auto_skip);
  switch (screen_size) {
    case SCREEN_SIZE_HALF:   fputs("-half\n", fp); break;
    case SCREEN_SIZE_FULL:   fputs("-full\n", fp); break;
#ifdef SUPPORT_DOUBLE
    case SCREEN_SIZE_DOUBLE: fputs("-double\n", fp); break;
#endif
  }
  if      (use_interlace ==  1) fputs("-interlace\n",  fp);
  else if (use_interlace == -1) fputs("-skipline\n",   fp);
  else                          fputs("-noskipline\n", fp);
  p_flag(fp, "interp",     "nointerp",     use_half_interp);
  p_flag(fp, "status",     "nostatus",     show_status);
  p_flag(fp, "hide_mouse", "show_mouse",   hide_mouse);
  p_flag(fp, "grab_mouse", "ungrab_mouse", grab_mouse);

  /* Sound --------------------------------------------------------------*/
  p_flag(fp, "sound",  "nosound", use_sound);
  p_flag(fp, "fmgen",  "nofmgen", use_fmgen);
  p_int (fp, "samplefreq", sample_rate);
  p_int (fp, "volume",     attenuation);
  p_int (fp, "fmvol",      fmvol);
  p_int (fp, "psgvol",     psgvol);
  p_int (fp, "beepvol",    beepvol);
  p_int (fp, "rhythmvol",  rhythmvol);
  p_int (fp, "adpcmvol",   adpcmvol);

  /* Input --------------------------------------------------------------*/
  p_flag(fp, "tenkey",  "notenkey",  tenkey_emu);
  p_flag(fp, "cursor",  "nocursor",  cursor_key_mode);
  p_flag(fp, "numlock", "nonumlock", numlock_emu);
  p_int (fp, "romaji",  romaji_type);
  p_int (fp, "keyboard", keyboard_type);

  /* Menu ---------------------------------------------------------------*/
  p_flag(fp, "japanese", "english", menu_lang == LANG_JAPAN);

  fclose(fp);
}
