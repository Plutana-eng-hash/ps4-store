#include <stdio.h>
#include <string.h>
#include "ui.h"

#define C_BG_TOP RGB(15, 22, 40)
#define C_BG_BOT RGB(6, 9, 18)
#define C_PANEL RGBA(255, 255, 255, 14)
#define C_LINE RGBA(255, 255, 255, 30)
#define C_ACCENT RGB(61, 139, 255)
#define C_SEL RGBA(61, 139, 255, 70)
#define C_TEXT RGB(236, 240, 250)
#define C_DIM RGB(138, 150, 176)
#define C_GREEN RGB(76, 208, 138)
#define C_AMBER RGB(244, 190, 80)
#define C_RED RGB(240, 100, 100)

#define MX 60
#define LIST_Y 150
#define ROW_H 68
#define LIST_W 1100

void ui_fmt_size(char *out, int n, uint64_t b) {
  if (b >= 1073741824ull) snprintf(out, n, "%u.%02u GB", (unsigned)(b >> 30), (unsigned)(((b & 0x3FFFFFFFull) * 100) >> 30));
  else if (b >= 1048576ull) snprintf(out, n, "%u.%u MB", (unsigned)(b >> 20), (unsigned)(((b & 0xFFFFFull) * 10) >> 20));
  else if (b >= 1024) snprintf(out, n, "%u KB", (unsigned)(b >> 10));
  else if (b) snprintf(out, n, "%u B", (unsigned)b);
  else snprintf(out, n, "--");
}

void ui_fix_scroll(UI *u) {
  if (u->sel < u->top) u->top = u->sel;
  if (u->sel >= u->top + UI_ROWS) u->top = u->sel - UI_ROWS + 1;
  if (u->top > u->count - UI_ROWS) u->top = u->count - UI_ROWS;
  if (u->top < 0) u->top = 0;
}

static void chip(Surf *s, int x, int y, const char *key, uint32_t col, const char *label, int *xo) {
  int kw = gfx_text_width(&font_bold, key) + 22;
  gfx_rrect(s, x, y, kw, 36, 18, col);
  gfx_text_center(s, &font_bold, x + kw / 2, y + 26, key, RGB(255, 255, 255));
  int lw = gfx_text(s, &font_small, x + kw + 10, y + 26, label, C_DIM);
  *xo = x + kw + 10 + lw + 34;
}

static void footer(Surf *s, int mode) {
  gfx_fill(s, 0, 1008, s->w, 72, RGBA(0, 0, 0, 90));
  int x = MX;
  if (mode == UI_LIST) {
    chip(s, x, 1026, "X", RGB(70, 120, 220), "Download", &x);
    chip(s, x, 1026, "O", RGB(210, 80, 90), "Exit", &x);
    chip(s, x, 1026, "Triangle", RGB(60, 170, 130), "Reload list", &x);
    chip(s, x, 1026, "L1 / R1", RGB(110, 110, 140), "Page up / down", &x);
    chip(s, x, 1026, "Up / Down", RGB(110, 110, 140), "Select", &x);
  } else if (mode == UI_ERROR) {
    chip(s, x, 1026, "Triangle", RGB(60, 170, 130), "Retry", &x);
    chip(s, x, 1026, "O", RGB(210, 80, 90), "Exit", &x);
  }
}

static void row(Surf *s, const Game *g, int y, int selected) {
  if (selected) { gfx_rrect(s, MX, y, LIST_W, ROW_H - 8, 14, C_SEL); gfx_fill(s, MX, y + 10, 5, ROW_H - 28, C_ACCENT); }
  else gfx_rrect(s, MX, y, LIST_W, ROW_H - 8, 14, RGBA(255, 255, 255, 8));
  char sz[32]; ui_fmt_size(sz, sizeof(sz), g->size);
  int right = MX + LIST_W - 24;
  int rw = 0;
  if (g->size) { rw = gfx_text_width(&font_small, sz); gfx_text_right(s, &font_small, right, y + 40, sz, C_DIM); }
  if (g->version[0]) {
    char v[40]; snprintf(v, sizeof(v), "v%s", g->version);
    int vx = right - (rw ? rw + 30 : 0);
    gfx_text_right(s, &font_small, vx, y + 40, v, C_DIM);
    rw += gfx_text_width(&font_small, v) + 30;
  }
  gfx_text_fit(s, selected ? &font_bold : &font_body, MX + 28, y + 41, LIST_W - 80 - rw, g->name, selected ? RGB(255, 255, 255) : C_TEXT);
}

static void prop(Surf *s, int x, int *y, const char *k, const char *v) {
  if (!v[0]) return;
  gfx_text(s, &font_small, x, *y, k, C_DIM);
  gfx_text_fit(s, &font_body, x + 190, *y, 400, v, C_TEXT);
  *y += 40;
}

static void details(Surf *s, const UI *u) {
  const Game *g = &u->games[u->sel];
  int x = 1210, w = 650, y0 = LIST_Y;
  gfx_rrect(s, x, y0, w, 700, 20, C_PANEL);
  /* cover tile with the initial of the title */
  gfx_rrect(s, x + 24, y0 + 24, 602, 150, 14, RGB(30, 52, 104));
  gfx_vgradient(s, x + 24, y0 + 100, 602, 74, RGBA(61, 139, 255, 0), RGBA(61, 139, 255, 70));
  char ini[8] = "?"; const char *p = g->name; while (*p == ' ') p++;
  if (*p) { ini[0] = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p; ini[1] = 0; }
  gfx_text_center(s, &font_title, x + 325, y0 + 112, ini, RGB(255, 255, 255));
  int ty = y0 + 224;
  int lines = gfx_text_wrap(s, &font_bold, x + 28, ty, w - 56, 3, g->name, RGB(255, 255, 255));
  int y = ty + lines * font_bold.line_h + 22;
  gfx_fill(s, x + 28, y - 24, w - 56, 1, C_LINE);
  y += 14;
  char sz[32]; ui_fmt_size(sz, sizeof(sz), g->size);
  prop(s, x + 28, &y, "Title ID", g->id);
  prop(s, x + 28, &y, "Version", g->version);
  prop(s, x + 28, &y, "Region", g->region);
  prop(s, x + 28, &y, "Type", g->type);
  if (g->size) prop(s, x + 28, &y, "Size", sz);
  prop(s, x + 28, &y, "Source", u->feed_host);
  /* saved state pill */
  const char *label; uint32_t col; char buf[96];
  if (u->sel_saved == SAVED_YES) { label = "Downloaded - saved in " SAVE_DIR_TXT; col = C_GREEN; }
  else if (u->sel_saved == SAVED_PARTIAL) { char b[32]; ui_fmt_size(b, sizeof(b), u->sel_saved_bytes); snprintf(buf, sizeof(buf), "Partial download (%s) - X resumes", b); label = buf; col = C_AMBER; }
  else { label = "Not downloaded yet"; col = C_DIM; }
  int py = y0 + 700 - 70;
  gfx_rrect(s, x + 28, py, w - 56, 44, 22, RGBA(255, 255, 255, 14));
  gfx_fill(s, x + 48, py + 17, 10, 10, col);
  gfx_text_fit(s, &font_small, x + 72, py + 30, w - 56 - 60, label, col);
}

static void scrollbar(Surf *s, const UI *u) {
  if (u->count <= UI_ROWS) return;
  int th = UI_ROWS * ROW_H - 8, x = MX + LIST_W + 18;
  gfx_rrect(s, x, LIST_Y, 8, th, 4, RGBA(255, 255, 255, 20));
  int bh = th * UI_ROWS / u->count; if (bh < 40) bh = 40;
  int by = LIST_Y + (th - bh) * u->top / (u->count - UI_ROWS);
  gfx_rrect(s, x, by, 8, bh, 4, C_ACCENT);
}

static void progress_overlay(Surf *s, const UI *u) {
  gfx_fill(s, 0, 0, s->w, s->h, RGBA(0, 0, 0, 200));
  int w = 1100, h = 330, x = (s->w - w) / 2, y = (s->h - h) / 2;
  gfx_rrect(s, x, y, w, h, 24, RGB(22, 30, 52));
  gfx_text(s, &font_small, x + 48, y + 56, u->dl_resumed ? "RESUMING DOWNLOAD" : "DOWNLOADING", C_ACCENT);
  gfx_text_fit(s, &font_bold, x + 48, y + 106, w - 96, u->dl_name, RGB(255, 255, 255));
  int bx = x + 48, bw = w - 96, by = y + 140;
  gfx_rrect(s, bx, by, bw, 26, 13, RGBA(255, 255, 255, 25));
  int pct = 0; if (u->dl_total) pct = (int)(u->dl_done * 100 / u->dl_total);
  int fw = u->dl_total ? (int)((double)bw * (double)u->dl_done / (double)u->dl_total) : 0;
  if (fw > 0) gfx_rrect(s, bx, by, fw < 26 ? 26 : fw, 26, 13, C_ACCENT);
  char a[32], b[32], l[160], sp[32], eta[32] = "";
  ui_fmt_size(a, sizeof(a), u->dl_done); ui_fmt_size(b, sizeof(b), u->dl_total);
  if (u->dl_total) snprintf(l, sizeof(l), "%d%%   -   %s / %s", pct, a, b); else snprintf(l, sizeof(l), "%s", a);
  gfx_text(s, &font_body, bx, by + 74, l, C_TEXT);
  ui_fmt_size(sp, sizeof(sp), (uint64_t)u->dl_speed);
  if (u->dl_speed > 1 && u->dl_total > u->dl_done) {
    unsigned sec = (unsigned)((double)(u->dl_total - u->dl_done) / u->dl_speed);
    snprintf(eta, sizeof(eta), "   -   ETA %u:%02u:%02u", sec / 3600, (sec / 60) % 60, sec % 60);
  }
  char r[96]; snprintf(r, sizeof(r), "%s/s%s", sp, eta);
  gfx_text_right(s, &font_body, bx + bw, by + 74, r, C_DIM);
  int cx = bx;
  chip(s, cx, y + h - 70, "O", RGB(210, 80, 90), "Cancel (the partial file is kept - X resumes later)", &cx);
}

void ui_draw(Surf *s, const UI *u) {
  gfx_vgradient(s, 0, 0, s->w, s->h, C_BG_TOP, C_BG_BOT);
  gfx_text(s, &font_title, MX, 104, APP_NAME_TXT, RGB(255, 255, 255));
  gfx_fill(s, MX, 124, s->w - 2 * MX, 1, C_LINE);
  if (u->count > 0 && u->mode != UI_ERROR) {
    char c[48]; snprintf(c, sizeof(c), "%d %s", u->count, u->count == 1 ? "game" : "games");
    gfx_text_right(s, &font_body, s->w - MX, 100, c, C_DIM);
  }

  if (u->mode == UI_LOADING) {
    gfx_text_center(s, &font_bold, s->w / 2, s->h / 2 - 6, "Loading game list...", C_TEXT);
    gfx_text_center(s, &font_small, s->w / 2, s->h / 2 + 40, u->msg, C_DIM);
  } else if (u->mode == UI_ERROR) {
    gfx_text_center(s, &font_bold, s->w / 2, s->h / 2 - 30, "Could not load the game list", C_RED);
    gfx_text_wrap(s, &font_body, s->w / 2 - 560, s->h / 2 + 24, 1120, 4, u->msg, C_TEXT);
  } else if (u->count > 0) {
    for (int i = 0; i < UI_ROWS && u->top + i < u->count; i++)
      row(s, &u->games[u->top + i], LIST_Y + i * ROW_H, u->top + i == u->sel);
    scrollbar(s, u);
    details(s, u);
  }
  footer(s, u->mode == UI_DOWNLOADING ? UI_LIST : u->mode);
  if (u->mode == UI_DOWNLOADING) progress_overlay(s, u);
  if (u->toast[0]) {
    int tw = gfx_text_width(&font_body, u->toast) + 56, tx = (s->w - tw) / 2;
    gfx_rrect(s, tx, 960, tw, 46, 23, RGB(36, 48, 84));
    gfx_text_center(s, &font_body, s->w / 2, 992, u->toast, RGB(255, 255, 255));
  }
}
