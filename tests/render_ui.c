/* Host-side preview: renders the UI into a PPM so the layout can be checked without a PS4. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"
static Game gm[40]; static uint32_t fb[1920 * 1080];
static void save(const char *path) {
  FILE *f = fopen(path, "wb"); fprintf(f, "P6\n1920 1080\n255\n");
  for (int i = 0; i < 1920 * 1080; i++) { unsigned char c[3] = { fb[i] >> 16, fb[i] >> 8, fb[i] }; fwrite(c, 1, 3, f); }
  fclose(f);
}
int main(int argc, char **argv) {
  const char *names[] = { "Example Racer", "Café Quest ™", "Sample Platformer Deluxe Edition With A Really Long Title To Test Truncation", "Alpha", "Beta - Base", "Gamma Strike", "Delta Force Demo", "Echo", "Foxtrot", "Golf Simulator", "Hotel Tycoon", "Island Escape", "Jungle Run", "Kart Kings" };
  for (int i = 0; i < 40; i++) {
    snprintf(gm[i].name, sizeof(gm[i].name), "%s", names[i % 14]);
    snprintf(gm[i].id, sizeof(gm[i].id), "CUSA%05d", 1000 + i); snprintf(gm[i].version, sizeof(gm[i].version), "01.%02d", i);
    snprintf(gm[i].region, sizeof(gm[i].region), "USA"); snprintf(gm[i].type, sizeof(gm[i].type), "Game");
    snprintf(gm[i].url, sizeof(gm[i].url), "https://example.com/files/%d.pkg", i); gm[i].size = (uint64_t)(i + 1) * 391000000ull;
  }
  Surf s = { fb, 1920, 1080, 1920 };
  UI u; memset(&u, 0, sizeof(u)); u.games = gm; u.count = 40; u.feed_host = "raw.githubusercontent.com";
  u.mode = UI_LIST; u.sel = 2; u.sel_saved = SAVED_PARTIAL; u.sel_saved_bytes = 1500000000ull; ui_fix_scroll(&u);
  ui_draw(&s, &u); save("list.ppm");
  u.sel = 17; u.sel_saved = SAVED_YES; ui_fix_scroll(&u); snprintf(u.toast, sizeof(u.toast), "Download complete: Hotel Tycoon"); ui_draw(&s, &u); save("list2.ppm");
  u.toast[0] = 0; u.mode = UI_DOWNLOADING; snprintf(u.dl_name, sizeof(u.dl_name), "%s", gm[17].name); u.dl_done = 2300000000ull; u.dl_total = 6900000000ull; u.dl_speed = 31.5e6; ui_draw(&s, &u); save("dl.ppm");
  u.mode = UI_ERROR; snprintf(u.msg, sizeof(u.msg), "HTTP error 404 for https://raw.githubusercontent.com/example/feed.json"); ui_draw(&s, &u); save("err.ppm");
  u.mode = UI_LOADING; snprintf(u.msg, sizeof(u.msg), "raw.githubusercontent.com"); u.count = 0; ui_draw(&s, &u); save("load.ppm");
  return 0;
}
