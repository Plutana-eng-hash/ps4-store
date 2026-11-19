/* Host mock of the PS4 APIs used by the app, so the real main.c/http.c/feed.c can be run on Linux
   against a local HTTP server. Test-only; never part of the PS4 build. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/Net.h>
#include "sys.h"

static double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static double t0;
__attribute__((constructor)) static void init0(void) { t0 = now_s(); }

int sceKernelSendNotificationRequest(int a, OrbisNotificationRequest *r, size_t n, int b) { (void)a; (void)r; (void)n; (void)b; fprintf(stderr, "[notify]\n"); return 0; }
int sceUserServiceInitialize(void *p) { (void)p; return 0; }
int sceUserServiceGetInitialUser(int *u) { *u = 1; return 0; }
int sceNetInit(void) { return 0; }
int sceNetPoolCreate(const char *n, int s, int f) { (void)n; (void)s; (void)f; return 1; }
int scePadInit(void) { return 0; }
int scePadOpen(int a, int b, int c, void *d) { (void)a; (void)b; (void)c; (void)d; return 1; }
/* scripted controller: CROSS at 1.0s (for 0.15s), CIRCLE at EXIT_AT seconds */
int scePadReadState(int h, OrbisPadData *d) {
  (void)h; double t = now_s() - t0; memset(d, 0, sizeof(*d));
  double exit_at = getenv("EXIT_AT") ? atof(getenv("EXIT_AT")) : 6.0;
  if (t > 1.0 && t < 1.15) d->buttons = BTN_CROSS;
  if (getenv("CANCEL_AT") && t > atof(getenv("CANCEL_AT")) && t < atof(getenv("CANCEL_AT")) + 0.3) d->buttons = BTN_CIRCLE;
  if (getenv("SECOND_X") && t > atof(getenv("SECOND_X")) && t < atof(getenv("SECOND_X")) + 0.15) d->buttons = BTN_CROSS;
  if (t > exit_at) d->buttons = BTN_CIRCLE;
  return 0;
}

int sys_usleep(unsigned us) { usleep(us); return 0; }
uint64_t sys_process_time(void) { return (uint64_t)((now_s() - t0) * 1e6); }
int sys_rename(const char *a, const char *b) { return rename(a, b); }
int64_t sys_direct_mem_size(void) { return 1LL << 30; }
int sys_alloc_direct(int64_t s, int64_t e, size_t l, size_t al, int ty, int64_t *p) { (void)s; (void)e; (void)l; (void)al; (void)ty; *p = 0; return 0; }
int sys_map_direct(void **a, size_t l, int pr, int fl, int64_t ph, size_t al) { (void)pr; (void)fl; (void)ph; *a = aligned_alloc(al, l); return *a ? 0 : -1; }

static void *g_bufs[2]; static int g_w = 1920, g_h = 1080, g_flips;
int sceVideoOutOpen(int u, int b, int i, const void *p) { (void)u; (void)b; (void)i; (void)p; return 1; }
void sceVideoOutSetBufferAttribute(void *a, uint32_t f, uint32_t t, uint32_t r, uint32_t w, uint32_t h, uint32_t p) { (void)a; (void)f; (void)t; (void)r; (void)p; g_w = w; g_h = h; }
int sceVideoOutRegisterBuffers(int h, int s, void *const *a, int n, const void *at) { (void)h; (void)s; (void)at; for (int i = 0; i < n && i < 2; i++) g_bufs[i] = a[i]; return 0; }
int sceVideoOutIsFlipPending(int h) { (void)h; return 0; }
int sceVideoOutSetFlipRate(int h, int r) { (void)h; (void)r; return 0; }
int sceVideoOutSubmitFlip(int h, int idx, uint32_t m, int64_t a) {
  (void)h; (void)m; (void)a; g_flips++;
  const char *dir = getenv("DUMP_DIR");
  if (dir) {
    char p[512]; snprintf(p, sizeof(p), "%s/frame_%03d.ppm", dir, g_flips);
    FILE *f = fopen(p, "wb"); fprintf(f, "P6\n%d %d\n255\n", g_w, g_h);
    uint32_t *px = g_bufs[idx];
    for (int i = 0; i < g_w * g_h; i++) { unsigned char c[3] = { px[i] >> 16, px[i] >> 8, px[i] }; fwrite(c, 1, 3, f); }
    fclose(f);
  }
  return 0;
}
int sceSystemServiceHideSplashScreen(void) { fprintf(stderr, "[splash hidden]\n"); return 0; }

/* ---- sceSsl / sceHttp over plain sockets ---- */
int sceSslInit(size_t s) { (void)s; return 1; }
int sceSslTerm(int c) { (void)c; return 0; }
int sceHttpInit(int a, int b, size_t c) { (void)a; (void)b; (void)c; return 1; }
int sceHttpTerm(int c) { (void)c; return 0; }
typedef struct { char host[128]; int port; char path[1024]; char range[64]; int sock; char hdr[8192]; int hlen, body_off, status; uint64_t clen; int has_clen; } Req;
static Req reqs[8]; static int nreq;
int sceHttpCreateTemplate(int c, const char *u, int v, int p) { (void)c; (void)u; (void)v; (void)p; return 1; }
int sceHttpDeleteTemplate(int t) { (void)t; return 0; }
int sceHttpSetAutoRedirect(int i, int e) { (void)i; (void)e; return 0; }
int sceHttpSetConnectTimeOut(int i, uint32_t u) { (void)i; (void)u; return 0; }
int sceHttpSetRecvTimeOut(int i, uint32_t u) { (void)i; (void)u; return 0; }
int sceHttpSetResolveTimeOut(int i, uint32_t u) { (void)i; (void)u; return 0; }
int sceHttpsDisableOption(int i, uint32_t f) { (void)i; (void)f; return 0; }
static char pending_url[1100];
int sceHttpCreateConnectionWithURL(int t, const char *url, int k) { (void)t; (void)k; snprintf(pending_url, sizeof(pending_url), "%s", url); return 2; }
int sceHttpDeleteConnection(int c) { (void)c; return 0; }
int sceHttpCreateRequestWithURL(int c, int m, const char *url, uint64_t l) {
  (void)c; (void)m; (void)l; Req *r = &reqs[nreq++ % 8]; memset(r, 0, sizeof(*r)); r->sock = -1;
  if (strncmp(url, "http://", 7)) { fprintf(stderr, "mock: only http:// supported (%s)\n", url); return -1; }
  const char *h = url + 7; const char *sl = strchr(h, '/'); int hl = sl ? (int)(sl - h) : (int)strlen(h);
  char hp[128]; snprintf(hp, sizeof(hp), "%.*s", hl, h); char *col = strchr(hp, ':');
  r->port = 80; if (col) { *col = 0; r->port = atoi(col + 1); } snprintf(r->host, sizeof(r->host), "%s", hp);
  snprintf(r->path, sizeof(r->path), "%s", sl ? sl : "/");
  return 100 + (int)((r - reqs));
}
int sceHttpAddRequestHeader(int id, const char *n, const char *v, uint32_t m) { (void)m; if (!strcmp(n, "Range")) snprintf(reqs[id - 100].range, 64, "%s", v); return 0; }
int sceHttpSendRequest(int id, const void *p, size_t s) {
  (void)p; (void)s; Req *r = &reqs[id - 100];
  r->sock = socket(AF_INET, SOCK_STREAM, 0); struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(r->port) };
  inet_pton(AF_INET, strcmp(r->host, "localhost") ? r->host : "127.0.0.1", &a.sin_addr);
  if (connect(r->sock, (void *)&a, sizeof(a)) < 0) return -1;
  char q[2048]; int n = snprintf(q, sizeof(q), "GET %s HTTP/1.1\r\nHost: %s\r\n%s%s%sConnection: close\r\n\r\n", r->path, r->host, r->range[0] ? "Range: " : "", r->range, r->range[0] ? "\r\n" : "");
  send(r->sock, q, n, 0);
  for (;;) {
    int k = recv(r->sock, r->hdr + r->hlen, sizeof(r->hdr) - 1 - r->hlen, 0); if (k <= 0) return -2;
    r->hlen += k; r->hdr[r->hlen] = 0; char *e = strstr(r->hdr, "\r\n\r\n"); if (e) { r->body_off = (int)(e - r->hdr) + 4; break; }
  }
  r->status = atoi(r->hdr + 9);
  char *cl = strcasestr(r->hdr, "content-length:"); if (cl) { r->has_clen = 1; r->clen = strtoull(cl + 15, NULL, 10); }
  return 0;
}
int sceHttpGetStatusCode(int id, int *s) { *s = reqs[id - 100].status; return 0; }
int sceHttpGetResponseContentLength(int id, int *res, uint64_t *l) { Req *r = &reqs[id - 100]; *res = r->has_clen ? 0 : 1; *l = r->clen; return 0; }
int sceHttpGetAllResponseHeaders(int id, char **h, size_t *sz) { Req *r = &reqs[id - 100]; *h = r->hdr; *sz = r->body_off; return 0; }
int sceHttpReadData(int id, void *buf, size_t n) {
  Req *r = &reqs[id - 100];
  if (r->body_off < r->hlen) { int left = r->hlen - r->body_off; if ((size_t)left > n) left = (int)n; memcpy(buf, r->hdr + r->body_off, left); r->body_off += left; return left; }
  return (int)recv(r->sock, buf, n, 0);
}
int sceHttpDeleteRequest(int id) { Req *r = &reqs[id - 100]; if (r->sock >= 0) close(r->sock); r->sock = -1; return 0; }
