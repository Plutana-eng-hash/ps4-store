#ifndef UI_H
#define UI_H
#include <stdint.h>
#include "gfx.h"
#include "feed.h"
#include "config.h"
#define APP_NAME_TXT APP_NAME
#define SAVE_DIR_TXT SAVE_DIR

typedef enum { UI_LOADING, UI_LIST, UI_ERROR, UI_DOWNLOADING } UiMode;
typedef enum { SAVED_NO, SAVED_PARTIAL, SAVED_YES } SavedState;

typedef struct {
  UiMode mode;
  const Game *games; int count, sel, top;
  char msg[768];                 /* loading / error text */
  const char *feed_host;
  SavedState sel_saved; uint64_t sel_saved_bytes;
  /* download overlay */
  char dl_name[96]; uint64_t dl_done, dl_total; double dl_speed; int dl_resumed;
  char toast[160];
} UI;

#define UI_ROWS 12
void ui_draw(Surf *s, const UI *u);
void ui_fix_scroll(UI *u);
void ui_fmt_size(char *out, int n, uint64_t bytes);
#endif
