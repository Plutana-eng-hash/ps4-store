/* PS4 Store - OpenOrbis homebrew (GoldHEN). Reads a JSON game list, shows it on screen and
   downloads the selected .pkg to /data/pkg. See README.md. */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include "config.h"
#include "sys.h"
#include "gfx.h"
#include "ui.h"
#include "feed.h"
#include "http.h"

/* ---------- notifications (system toast, shown even if the app is in the background) ---------- */
typedef struct {
  int type, req_id, priority, msg_id, target_id, user_id, unk1, unk2, app_id, error_num, unk3;
  char use_icon_image_uri; char message[1024]; char icon_uri[1024]; char unk[1024];
} notify_t;

static void notify(const char *fmt, ...) {
  static notify_t n; memset(&n, 0, sizeof(n));
  n.type = 0; n.use_icon_image_uri = 1; n.user_id = -1;
  va_list ap; va_start(ap, fmt); vsnprintf(n.message, sizeof(n.message), fmt, ap); va_end(ap);
  strcpy(n.icon_uri, "cxml://psnotification/tex_icon_system");
  sceKernelSendNotificationRequest(0, (OrbisNotificationRequest *)&n, sizeof(n), 0);
}

/* ---------- screen ---------- */
#define SCR_W 1920
#define SCR_H 1080
#define BUF_BYTES ((size_t)SCR_W * SCR_H * 4)
#define BUF_STRIDE ((BUF_BYTES + 0xFFFF) & ~(size_t)0xFFFF)

static int g_video = -1, g_cur = 0;
static Surf g_surf[2];

static int video_init(void) {
  g_video = sceVideoOutOpen(VO_USER_SYSTEM, 0, 0, NULL);
  if (g_video < 0) return -1;
  sceVideoOutSetFlipRate(g_video, 0);
  size_t total = (2 * BUF_STRIDE + 0x1FFFFF) & ~(size_t)0x1FFFFF;
  int64_t phys = 0; void *base = NULL;
  if (sys_alloc_direct(0, sys_direct_mem_size(), total, 0x200000, 3, &phys) < 0) return -2;
  if (sys_map_direct(&base, total, 0x33, 0, phys, 0x200000) < 0) return -3;
  void *addrs[2];
  for (int i = 0; i < 2; i++) {
    addrs[i] = (char *)base + i * BUF_STRIDE;
    g_surf[i].px = (uint32_t *)addrs[i]; g_surf[i].w = SCR_W; g_surf[i].h = SCR_H; g_surf[i].stride = SCR_W;
    memset(addrs[i], 0, BUF_BYTES);
  }
  uint64_t attr[8]; memset(attr, 0, sizeof(attr));
  sceVideoOutSetBufferAttribute(attr, VO_FORMAT_A8R8G8B8_SRGB, VO_TILING_LINEAR, VO_ASPECT_16_9, SCR_W, SCR_H, SCR_W);
  if (sceVideoOutRegisterBuffers(g_video, 0, addrs, 2, attr) < 0) return -4;
  return 0;
}

static int g_first_frame = 1;
static void present(const UI *ui) {
  Surf *s = &g_surf[g_cur];
  ui_draw(s, ui);
  while (sceVideoOutIsFlipPending(g_video) > 0) sys_usleep(1000);
  sceVideoOutSubmitFlip(g_video, g_cur, VO_FLIP_VSYNC, 0);
  g_cur ^= 1;
  if (g_first_frame) { g_first_frame = 0; if (sceSystemServiceHideSplashScreen) sceSystemServiceHideSplashScreen(); }
}

/* ---------- pad ---------- */
static int g_pad = -1;
static uint32_t pad_buttons(void) {
  OrbisPadData d; memset(&d, 0, sizeof(d));
  if (g_pad < 0 || scePadReadState(g_pad, &d) != 0) return 0;
  return d.buttons;
}

/* ---------- feed ---------- */
static Game g_games[MAX_GAMES];
static char g_feed_url[512] = FEED_URL;
static char g_feed_host[96];
static char g_feedbuf[FEED_MAX_BYTES + 1];

static void load_feed_url(void) {
  snprintf(g_feed_url, sizeof(g_feed_url), "%s", FEED_URL);
  int fd = open(FEED_OVERRIDE_FILE, O_RDONLY);
  if (fd >= 0) {
    char t[512]; int n = (int)read(fd, t, sizeof(t) - 1); close(fd);
    if (n > 0) { t[n] = 0; char *e = t; while (*e && *e != '\r' && *e != '\n' && *e != ' ') e++; *e = 0; if (!strncmp(t, "http", 4)) snprintf(g_feed_url, sizeof(g_feed_url), "%s", t); }
  }
  const char *h = strstr(g_feed_url, "://"); h = h ? h + 3 : g_feed_url;
  int i = 0; while (h[i] && h[i] != '/' && i < (int)sizeof(g_feed_host) - 1) { g_feed_host[i] = h[i]; i++; } g_feed_host[i] = 0;
}

static int fetch_feed(char *err, int errsz) {
  Http h;
  if (http_get(&h, g_feed_url, 0, err, errsz) < 0) return -1;
  if (h.status != 200) { snprintf(err, errsz, "Server answered HTTP %d for the game list", h.status); http_close(&h); return -1; }
  size_t total = 0;
  for (;;) {
    int n = http_read(&h, g_feedbuf + total, FEED_MAX_BYTES - total);
    if (n < 0) { snprintf(err, errsz, "Download of the game list was interrupted (0x%08X)", (unsigned)n); http_close(&h); return -1; }
    if (n == 0) break;
    total += n;
    if (total >= FEED_MAX_BYTES) { snprintf(err, errsz, "Game list is bigger than %d MB", FEED_MAX_BYTES / 1048576); http_close(&h); return -1; }
  }
  http_close(&h);
  g_feedbuf[total] = 0;
  return feed_parse(g_feedbuf, total, g_feed_url, g_games, MAX_GAMES, err, errsz);
}

/* ---------- files ---------- */
static void dest_paths(const Game *g, char *dest, int dsz, char *part, int psz) {
  char fn[200]; feed_filename(g->url, fn, sizeof(fn));
  size_t l = strlen(fn);
  if (l < 4 || strcmp(fn + l - 4, ".pkg") != 0) {                  /* link has no readable file name: use the game name */
    int o = 0;
    for (const char *p = g->name; *p && o < 150; p++) fn[o++] = ((*p >= '0' && *p <= '9') || ((*p | 32) >= 'a' && (*p | 32) <= 'z') || *p == '-' || *p == '.') ? *p : '_';
    fn[o] = 0; strcat(fn, ".pkg");
  }
  snprintf(dest, dsz, "%s/%s", SAVE_DIR, fn);
  snprintf(part, psz, "%s.part", dest);
}
static long long file_size(const char *path) {
  int fd = open(path, O_RDONLY); if (fd < 0) return -1;
  off_t e = lseek(fd, 0, SEEK_END); close(fd); return (long long)e;
}
static void refresh_saved(UI *ui) {
  ui->sel_saved = SAVED_NO; ui->sel_saved_bytes = 0;
  if (ui->count <= 0) return;
  char d[400], p[420]; dest_paths(&g_games[ui->sel], d, sizeof(d), p, sizeof(p));
  if (file_size(d) >= 0) { ui->sel_saved = SAVED_YES; return; }
  long long ps = file_size(p);
  if (ps > 0) { ui->sel_saved = SAVED_PARTIAL; ui->sel_saved_bytes = (uint64_t)ps; }
}

/* ---------- download ---------- */
static uint64_t now_ms(void) { return sys_process_time() / 1000; }
static uint8_t g_dlbuf[1024 * 1024];

/* returns 0 on success, 1 if cancelled by the user, -1 on error (message in err) */
static int do_download(UI *ui, const Game *g, char *err, int errsz) {
  char dest[400], part[420]; dest_paths(g, dest, sizeof(dest), part, sizeof(part));
  mkdir("/data", 0777); mkdir(SAVE_DIR, 0777);
  long long have = file_size(part); if (have < 0) have = 0;
  Http h;
  if (http_get(&h, g->url, (uint64_t)have, err, errsz) < 0) return -1;
  if (h.status == 416 && have > 0) {                                 /* "range not satisfiable": partial is >= the real file */
    http_close(&h);
    if (g->size && (uint64_t)have == g->size) {                      /* exactly the advertised size: it is complete */
      unlink(dest);
      if (sys_rename(part, dest) < 0) { snprintf(err, errsz, "Could not rename the finished file"); return -1; }
      return 0;
    }
    unlink(part); have = 0;                                          /* otherwise it is bogus: start over */
    if (http_get(&h, g->url, 0, err, errsz) < 0) return -1;
  }
  if (h.status == 416) { http_close(&h); snprintf(err, errsz, "Server refused the request (HTTP 416)"); return -1; }
  if (have > 0 && h.status == 200) have = 0;                         /* server ignored the Range header: start over */
  uint64_t total = h.has_len ? h.content_len + (uint64_t)have : g->size;
  int fd = open(part, O_WRONLY | O_CREAT | (have ? 0 : O_TRUNC), 0777);
  if (fd < 0) { snprintf(err, errsz, "Cannot write %s", part); http_close(&h); return -1; }
  if (have) lseek(fd, (off_t)have, SEEK_SET);

  snprintf(ui->dl_name, sizeof(ui->dl_name), "%s", g->name);
  ui->mode = UI_DOWNLOADING; ui->dl_done = (uint64_t)have; ui->dl_total = total; ui->dl_speed = 0; ui->dl_resumed = have > 0;
  present(ui);

  uint64_t done = (uint64_t)have, t_last = now_ms(), t_ui = t_last, bytes_last = done;
  int result = 0; uint32_t prev = pad_buttons();
  for (;;) {
    int n = http_read(&h, g_dlbuf, sizeof(g_dlbuf));
    if (n < 0) { snprintf(err, errsz, "Connection lost (0x%08X). Press X to resume.", (unsigned)n); result = -1; break; }
    if (n == 0) break;
    int w = 0;
    while (w < n) { int r = (int)write(fd, g_dlbuf + w, n - w); if (r <= 0) { snprintf(err, errsz, "Disk write failed - is the PS4 storage full?"); result = -1; goto out; } w += r; }
    done += (uint64_t)n;
    uint64_t t = now_ms();
    if (t - t_ui >= 250) {
      t_ui = t;
      if (t - t_last >= 1000) { double inst = (double)(done - bytes_last) * 1000.0 / (double)(t - t_last); ui->dl_speed = ui->dl_speed > 0 ? ui->dl_speed * 0.6 + inst * 0.4 : inst; t_last = t; bytes_last = done; }
      ui->dl_done = done; present(ui);
      uint32_t b = pad_buttons();
      if ((b & ~prev) & BTN_CIRCLE) { result = 1; break; }
      prev = b;
    }
  }
out:
  close(fd); http_close(&h);
  if (result != 0) return result;
  if (total && done < total) { snprintf(err, errsz, "Download ended early (%llu of %llu bytes). Press X to resume.", (unsigned long long)done, (unsigned long long)total); return -1; }
  unlink(dest);
  if (sys_rename(part, dest) < 0) { snprintf(err, errsz, "Downloaded, but could not rename the file"); return -1; }
  return 0;
}

/* ---------- main ---------- */
static void set_toast(UI *ui, uint64_t *until, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); vsnprintf(ui->toast, sizeof(ui->toast), fmt, ap); va_end(ap);
  *until = now_ms() + 5000;
}

static int reload(UI *ui) {
  ui->mode = UI_LOADING; ui->count = 0; ui->games = g_games;
  snprintf(ui->msg, sizeof(ui->msg), "%s", g_feed_host);
  present(ui);
  char err[256] = "";
  int n = fetch_feed(err, sizeof(err));
  if (n < 0) { ui->mode = UI_ERROR; ui->count = 0; snprintf(ui->msg, sizeof(ui->msg), "%s\n%s", err, g_feed_url); return -1; }
  ui->mode = UI_LIST; ui->count = n; ui->sel = 0; ui->top = 0; refresh_saved(ui);
  return 0;
}

int main(void) {
  sceUserServiceInitialize(NULL);
  int uid = 0; sceUserServiceGetInitialUser(&uid);
  scePadInit(); g_pad = scePadOpen(uid, 0, 0, NULL);

  int vr = video_init();
  if (vr < 0) { notify("%s: video init failed (%d)", APP_NAME, vr); sys_usleep(5000000); return 1; }

  static UI ui; memset(&ui, 0, sizeof(ui));
  ui.games = g_games; ui.feed_host = g_feed_host;
  load_feed_url();

  char err[256] = "";
  if (net_start(err, sizeof(err)) < 0) { ui.mode = UI_ERROR; snprintf(ui.msg, sizeof(ui.msg), "%s", err); }
  else reload(&ui);
  present(&ui);

  uint32_t prev = pad_buttons(); uint64_t next_rep = 0, toast_until = 0;
  int dirty = 0;
  for (;;) {
    uint32_t b = pad_buttons(), pressed = b & ~prev; prev = b;
    uint64_t t = now_ms();
    int move = 0, fresh = (int)(pressed & (BTN_UP | BTN_DOWN));
    if (fresh) { next_rep = t + 380; move = (pressed & BTN_DOWN) ? 1 : -1; }
    else if ((b & (BTN_UP | BTN_DOWN)) && t >= next_rep) { next_rep = t + 60; move = (b & BTN_DOWN) ? 1 : -1; }

    if (ui.mode == UI_LIST && ui.count > 0) {
      int old = ui.sel;
      if (move) {
        ui.sel += move;
        if (ui.sel < 0) ui.sel = fresh ? ui.count - 1 : 0;              /* wrap only on a fresh press */
        if (ui.sel >= ui.count) ui.sel = fresh ? 0 : ui.count - 1;
      }
      if (pressed & (BTN_R1 | BTN_RIGHT)) { ui.sel += UI_ROWS; if (ui.sel >= ui.count) ui.sel = ui.count - 1; }
      if (pressed & (BTN_L1 | BTN_LEFT)) { ui.sel -= UI_ROWS; if (ui.sel < 0) ui.sel = 0; }
      if (ui.sel != old) { ui_fix_scroll(&ui); refresh_saved(&ui); dirty = 1; }

      if (pressed & BTN_CROSS) {
        char e[256] = ""; Game *g = &g_games[ui.sel];
        int r = do_download(&ui, g, e, sizeof(e));
        ui.mode = UI_LIST; refresh_saved(&ui);
        if (r == 0) { set_toast(&ui, &toast_until, "Download complete: %s", g->name); notify("%s\nDownload complete.\nInstall it from %s", g->name, SAVE_DIR); }
        else if (r == 1) set_toast(&ui, &toast_until, "Download cancelled - press X to resume");
        else set_toast(&ui, &toast_until, "%s", e);
        prev = pad_buttons(); dirty = 1;
      }
      if (pressed & BTN_TRIANGLE) { reload(&ui); dirty = 1; }
      if (pressed & BTN_CIRCLE) break;
    } else if (ui.mode == UI_ERROR) {
      if (pressed & BTN_TRIANGLE) { if (net_start(err, sizeof(err)) < 0) { snprintf(ui.msg, sizeof(ui.msg), "%s", err); ui.mode = UI_ERROR; }
        else reload(&ui);
        dirty = 1; }
      if (pressed & BTN_CIRCLE) break;
    }

    if (ui.toast[0] && t >= toast_until) { ui.toast[0] = 0; dirty = 1; }
    if (dirty) { present(&ui); dirty = 0; }
    sys_usleep(16000);
  }
  return 0;
}
