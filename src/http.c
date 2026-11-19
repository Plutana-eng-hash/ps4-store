#include <stdio.h>
#include <string.h>
#include <orbis/Net.h>
#include "sys.h"
#include "http.h"

static int g_pool = -1, g_ssl = -1, g_http = -1, g_inited = 0;

int net_start(char *err, int errsz) {     /* safe to call again after a failure: finished steps are skipped */
  if (g_http >= 0) return 0;
  if (!g_inited) {
    int r = sceNetInit();
    if (r < 0) { snprintf(err, errsz, "sceNetInit failed (0x%08X)", (unsigned)r); return -1; }
    g_inited = 1;
  }
  if (g_pool < 0) {
    g_pool = sceNetPoolCreate("ps4store", 4 * 1024 * 1024, 0);
    if (g_pool < 0) { snprintf(err, errsz, "sceNetPoolCreate failed (0x%08X)", (unsigned)g_pool); int e = g_pool; g_pool = -1; (void)e; return -1; }
  }
  if (g_ssl < 0) {
    g_ssl = sceSslInit(1024 * 1024);
    if (g_ssl < 0) { snprintf(err, errsz, "sceSslInit failed (0x%08X)", (unsigned)g_ssl); return -1; }
  }
  g_http = sceHttpInit(g_pool, g_ssl, 4 * 1024 * 1024);
  if (g_http < 0) { snprintf(err, errsz, "sceHttpInit failed (0x%08X)", (unsigned)g_http); return -1; }
  return 0;
}

void http_close(Http *h) {
  if (h->req >= 0) sceHttpDeleteRequest(h->req);
  if (h->conn >= 0) sceHttpDeleteConnection(h->conn);
  if (h->tmpl >= 0) sceHttpDeleteTemplate(h->tmpl);
  h->req = h->conn = h->tmpl = -1;
}

static int ci_prefix(const char *s, const char *p) {
  for (; *p; s++, p++) { char a = *s, b = *p; if (a >= 'A' && a <= 'Z') a += 32; if (b >= 'A' && b <= 'Z') b += 32; if (a != b) return 0; }
  return 1;
}

/* pulls "Location:" out of the raw response headers */
static int find_location(Http *h, const char *cur_url, char *out, int outsz) {
  char *hdr = NULL; size_t hsz = 0;
  if (sceHttpGetAllResponseHeaders(h->req, &hdr, &hsz) < 0 || !hdr) return 0;
  for (size_t i = 0; i < hsz; i++) {
    if ((i == 0 || hdr[i - 1] == '\n') && hsz - i > 9 && ci_prefix(hdr + i, "location:")) {
      size_t j = i + 9; while (j < hsz && (hdr[j] == ' ' || hdr[j] == '\t')) j++;
      int o = 0; while (j < hsz && hdr[j] != '\r' && hdr[j] != '\n' && o < outsz - 1) out[o++] = hdr[j++];
      out[o] = 0;
      if (out[0] == '/') {                               /* relative redirect: keep scheme + host of cur_url */
        char t[1024]; snprintf(t, sizeof(t), "%s", out);
        const char *hs = strstr(cur_url, "://"); hs = hs ? hs + 3 : cur_url;
        const char *pe = strchr(hs, '/'); int hl = pe ? (int)(pe - cur_url) : (int)strlen(cur_url);
        if (hl > outsz - 2) hl = outsz - 2;
        memcpy(out, cur_url, hl); snprintf(out + hl, outsz - hl, "%s", t);
      }
      return o > 0;
    }
  }
  return 0;
}

static int open_once(Http *h, const char *url, uint64_t from, char *err, int errsz) {
  h->tmpl = h->conn = h->req = -1; h->status = 0; h->has_len = 0; h->content_len = 0;
  h->tmpl = sceHttpCreateTemplate(g_http, "Mozilla/5.0 (PlayStation 4) PS4Store/1.1", 1, 1);
  if (h->tmpl < 0) { snprintf(err, errsz, "HTTP template failed (0x%08X)", (unsigned)h->tmpl); return -1; }
  sceHttpSetAutoRedirect(h->tmpl, 0);                    /* we follow redirects ourselves (more reliable) */
  sceHttpSetResolveTimeOut(h->tmpl, 15 * 1000000u);
  sceHttpSetConnectTimeOut(h->tmpl, 20 * 1000000u);
  sceHttpSetRecvTimeOut(h->tmpl, 30 * 1000000u);
  /* Homebrew has no access to Sony's CA store, so certificate verification is switched off. */
  sceHttpsDisableOption(h->tmpl, HTTPS_SERVER_VERIFY | HTTPS_CN_CHECK | HTTPS_NOT_AFTER_CHECK | HTTPS_NOT_BEFORE_CHECK | HTTPS_KNOWN_CA_CHECK);
  h->conn = sceHttpCreateConnectionWithURL(h->tmpl, url, 0);
  if (h->conn < 0) { snprintf(err, errsz, "Cannot connect (0x%08X) - check the URL / internet connection", (unsigned)h->conn); return -1; }
  h->req = sceHttpCreateRequestWithURL(h->conn, 0 /* GET */, url, 0);
  if (h->req < 0) { snprintf(err, errsz, "HTTP request failed (0x%08X)", (unsigned)h->req); return -1; }
  if (from > 0) { char rv[48]; snprintf(rv, sizeof(rv), "bytes=%llu-", (unsigned long long)from); sceHttpAddRequestHeader(h->req, "Range", rv, 0); }
  int r = sceHttpSendRequest(h->req, NULL, 0);
  if (r < 0) { snprintf(err, errsz, "Request failed (0x%08X) - TLS/DNS/network problem", (unsigned)r); return -1; }
  r = sceHttpGetStatusCode(h->req, &h->status);
  if (r < 0) { snprintf(err, errsz, "No HTTP status (0x%08X)", (unsigned)r); return -1; }
  int res = 0; uint64_t len = 0;
  if (sceHttpGetResponseContentLength(h->req, &res, &len) >= 0 && res == 0) { h->has_len = 1; h->content_len = len; }
  return 0;
}

int http_get(Http *h, const char *url, uint64_t range_from, char *err, int errsz) {
  char cur[1024]; snprintf(cur, sizeof(cur), "%s", url);
  h->tmpl = h->conn = h->req = -1;
  for (int hop = 0; hop < 7; hop++) {
    if (open_once(h, cur, range_from, err, errsz) < 0) { http_close(h); return -1; }
    if (h->status == 200 || h->status == 206 || h->status == 416) return 0;
    if (h->status >= 300 && h->status < 400) {
      char loc[1024];
      if (!find_location(h, cur, loc, sizeof(loc))) { snprintf(err, errsz, "HTTP %d redirect without Location", h->status); http_close(h); return -1; }
      http_close(h); snprintf(cur, sizeof(cur), "%s", loc); continue;
    }
    snprintf(err, errsz, "Server answered HTTP %d%s", h->status, h->status == 404 ? " (file not found)" : h->status == 403 ? " (forbidden)" : "");
    http_close(h); return -1;
  }
  snprintf(err, errsz, "Too many redirects"); http_close(h); return -1;
}

int http_read(Http *h, void *buf, size_t n) { return sceHttpReadData(h->req, buf, n); }
