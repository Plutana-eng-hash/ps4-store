/* PS4 Downloader - OpenOrbis homebrew (GoldHEN). Untested scaffold, see README. */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <orbis/libkernel.h>
#include <orbis/Net.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include "config.h"

#define MAX_ITEMS 256

typedef struct { char name[128]; char path[256]; } Item;
static Item items[MAX_ITEMS];
static int item_count = 0;

/* ---------- notifications (on-screen status) ---------- */
typedef struct {
  int type, req_id, priority, msg_id, target_id, user_id, unk1, unk2, app_id, error_num, unk3;
  char use_icon_image_uri; char message[1024]; char icon_uri[1024]; char unk[1024];
} notify_t;
extern int sceKernelSendNotificationRequest(int, void *, size_t, int);

static void notify(const char *fmt, ...) {
  notify_t n; memset(&n, 0, sizeof(n));
  n.type = 0; n.use_icon_image_uri = 1; n.user_id = -1;
  va_list ap; va_start(ap, fmt); vsnprintf(n.message, sizeof(n.message), fmt, ap); va_end(ap);
  strcpy(n.icon_uri, "cxml://psnotification/tex_icon_system");
  sceKernelSendNotificationRequest(0, &n, sizeof(n), 0);
}

/* ---------- networking (plain HTTP over sceNet) ---------- */
static int net_pool = -1;

static int net_start(void) {
  int r = sceNetInit();
  if (r < 0) return r;
  net_pool = sceNetPoolCreate("dl", 4 * 1024 * 1024, 0);
  return net_pool < 0 ? net_pool : 0;
}

static int http_connect(const char *host, int port) {
  OrbisNetInAddr addr;
  if (sceNetInetPton(ORBIS_NET_AF_INET, host, &addr) != 1) {
    int rid = sceNetResolverCreate("res", net_pool, 0);
    if (rid < 0) return -1;
    int r = sceNetResolverStartNtoa(rid, host, &addr, 2, 3, 0);
    sceNetResolverDestroy(rid);
    if (r < 0) return -1;
  }
  int s = sceNetSocket("dl", ORBIS_NET_AF_INET, ORBIS_NET_SOCK_STREAM, 0);
  if (s < 0) return -1;
  OrbisNetSockaddrIn sa; memset(&sa, 0, sizeof(sa));
  sa.sin_len = sizeof(sa); sa.sin_family = ORBIS_NET_AF_INET;
  sa.sin_port = sceNetHtons(port); sa.sin_addr = addr;
  if (sceNetConnect(s, (OrbisNetSockaddr *)&sa, sizeof(sa)) < 0) { sceNetSocketClose(s); return -1; }
  return s;
}

/* GET path. If out_fd >= 0, body is written to it; else into mem buffer.
   Returns body bytes or -1. */
static long http_get(const char *path, int out_fd, char *mem, long memsz, const char *label) {
  int s = http_connect(SERVER_HOST, SERVER_PORT);
  if (s < 0) return -1;
  char req[512];
  int rl = snprintf(req, sizeof(req),
    "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: PS4Downloader\r\nConnection: close\r\n\r\n", path, SERVER_HOST);
  if (sceNetSend(s, req, rl, 0) < 0) { sceNetSocketClose(s); return -1; }

  static char buf[64 * 1024];
  long total = 0, content_len = -1; int header_done = 0; int hlen = 0; int last_pct = -10;
  char hdr[2048];
  for (;;) {
    int n = sceNetRecv(s, buf, sizeof(buf), 0);
    if (n <= 0) break;
    char *p = buf; int len = n;
    if (!header_done) {
      int room = (int)sizeof(hdr) - 1 - hlen; int cp = len < room ? len : room;
      memcpy(hdr + hlen, p, cp); hlen += cp; hdr[hlen] = 0;
      char *end = strstr(hdr, "\r\n\r\n");
      if (!end) { if (hlen >= (int)sizeof(hdr) - 1) break; continue; }
      if (strncmp(hdr, "HTTP/1.", 7) != 0 || !strstr(hdr, " 200")) { sceNetSocketClose(s); return -1; }
      char *cl = strstr(hdr, "Content-Length:");
      if (!cl) cl = strstr(hdr, "content-length:");
      if (cl) content_len = atol(cl + 15);
      int hdr_bytes = (int)(end - hdr) + 4;
      /* body bytes that arrived in the same recv as the header */
      int already = hlen - hdr_bytes;
      header_done = 1;
      p = buf + (len - already); len = already;
      if (len <= 0) continue;
    }
    if (out_fd >= 0) {
      long w = 0; while (w < len) { int r = write(out_fd, p + w, len - w); if (r <= 0) { sceNetSocketClose(s); return -1; } w += r; }
    } else if (mem && total + len < memsz) {
      memcpy(mem + total, p, len);
    }
    total += len;
    if (label && content_len > 0) {
      int pct = (int)(total * 100 / content_len);
      if (pct >= last_pct + 20) { last_pct = pct; notify("%s: %d%%", label, pct); }
    }
  }
  sceNetSocketClose(s);
  if (mem && total < memsz) mem[total] = 0;
  return total;
}

/* list.txt format: one item per line:  Display Name|/path/on/server/file.pkg */
static void parse_list(char *text) {
  item_count = 0;
  for (char *line = strtok(text, "\r\n"); line && item_count < MAX_ITEMS; line = strtok(NULL, "\r\n")) {
    if (line[0] == '#' || !line[0]) continue;
    char *bar = strchr(line, '|'); if (!bar) continue;
    *bar = 0;
    snprintf(items[item_count].name, sizeof(items[0].name), "%s", line);
    snprintf(items[item_count].path, sizeof(items[0].path), "%s", bar + 1);
    item_count++;
  }
}

static int download_item(const Item *it) {
  mkdir("/data", 0777); mkdir(SAVE_DIR, 0777);
  const char *base = strrchr(it->path, '/'); base = base ? base + 1 : it->path;
  char dest[400]; snprintf(dest, sizeof(dest), "%s/%s", SAVE_DIR, base);
  int fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if (fd < 0) { notify("Cannot write %s", dest); return -1; }
  notify("Downloading %s", it->name);
  long n = http_get(it->path, fd, NULL, 0, it->name);
  close(fd);
  if (n < 0) { unlink(dest); notify("Download failed: %s", it->name); return -1; }
  notify("Saved %s (%ld KB)\n%s", it->name, n / 1024, dest);
  return 0;
}

int main(void) {
  sceUserServiceInitialize(NULL);
  int uid = 0; sceUserServiceGetInitialUser(&uid);
  scePadInit();
  int pad = scePadOpen(uid, 0, 0, NULL);

  if (net_start() < 0) { notify("Network init failed"); sceKernelSleep(5); return 1; }

  static char listbuf[64 * 1024];
  notify("PS4 Downloader\nFetching list...");
  if (http_get(LIST_PATH, -1, listbuf, sizeof(listbuf), NULL) < 0) {
    notify("Could not load list from %s:%d%s", SERVER_HOST, SERVER_PORT, LIST_PATH);
    sceKernelSleep(8); return 1;
  }
  parse_list(listbuf);
  if (!item_count) { notify("List is empty"); sceKernelSleep(5); return 1; }

  int sel = 0; unsigned prev = 0;
  notify("%d files. D-pad Up/Down = choose, X = download, Circle = exit", item_count);
  notify("> %s", items[sel].name);

  for (;;) {
    OrbisPadData d; memset(&d, 0, sizeof(d));
    if (scePadReadState(pad, &d) == 0) {
      unsigned b = d.buttons, pressed = b & ~prev; prev = b;
      if (pressed & ORBIS_PAD_BUTTON_DOWN) { sel = (sel + 1) % item_count; notify("> %s (%d/%d)", items[sel].name, sel + 1, item_count); }
      if (pressed & ORBIS_PAD_BUTTON_UP)   { sel = (sel + item_count - 1) % item_count; notify("> %s (%d/%d)", items[sel].name, sel + 1, item_count); }
      if (pressed & ORBIS_PAD_BUTTON_CROSS)  download_item(&items[sel]);
      if (pressed & ORBIS_PAD_BUTTON_CIRCLE) break;
    }
    sceKernelUsleep(30000);
  }
  return 0;
}
