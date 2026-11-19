#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json.h"
#include "feed.h"
#include "config.h"

static JNode g_nodes[FEED_MAX_NODES];
static char g_strs[FEED_MAX_BYTES];
static JDoc doc = { g_nodes, 0, FEED_MAX_NODES, g_strs, 0, FEED_MAX_BYTES };
static Game *G; static int gmax, gcount; static const char *gbase;

static int ieq(const char *a, const char *b) {
  for (; *a && *b; a++, b++) { char x = *a, y = *b; if (x >= 'A' && x <= 'Z') x += 32; if (y >= 'A' && y <= 'Z') y += 32; if (x != y) return 0; }
  return *a == *b;
}
static int has_suffix_ci(const char *s, const char *suf) {
  size_t a = strlen(s), b = strlen(suf); if (b > a) return 0;
  return ieq(s + a - b, suf);
}
static int is_http(const char *s) { return !strncmp(s, "http://", 7) || !strncmp(s, "https://", 8); }
static int is_image(const char *s) {
  char t[600]; snprintf(t, sizeof(t), "%s", s);
  char *q = strpbrk(t, "?#"); if (q) *q = 0;
  return has_suffix_ci(t, ".png") || has_suffix_ci(t, ".jpg") || has_suffix_ci(t, ".jpeg") || has_suffix_ci(t, ".webp") || has_suffix_ci(t, ".gif") || has_suffix_ci(t, ".ico");
}
static int looks_pkg(const char *s) {
  char t[600]; snprintf(t, sizeof(t), "%s", s);
  char *q = strpbrk(t, "?#"); if (q) *q = 0;
  return has_suffix_ci(t, ".pkg");
}
static int in_list(const char *k, const char *const *list) { for (; *list; list++) if (ieq(k, *list)) return 1; return 0; }

static const char *const URL_KEYS[] = { "pkg", "pkg_url", "pkgurl", "pkg_link", "pkglink", "pkg_path", "download", "download_url", "downloadurl",
  "download_link", "downloadlink", "direct_link", "directlink", "url", "link", "file", "file_url", "fileurl", "path", "href", "dl", "dl_url", NULL };
static const char *const NAME_KEYS[] = { "name", "title", "game", "gamename", "game_name", "app_name", "appname", "display_name", "displayname",
  "label", "title_name", "titlename", NULL };
static const char *const ID_KEYS[] = { "title_id", "titleid", "tid", "cusa", "content_id", "contentid", "id", "code", "serial", NULL };
static const char *const VER_KEYS[] = { "version", "ver", "app_ver", "appver", "v", NULL };
static const char *const REG_KEYS[] = { "region", "regions", "locale", NULL };
static const char *const TYPE_KEYS[] = { "type", "category", "genre", "platform", "kind", NULL };
static const char *const SIZE_KEYS[] = { "size", "file_size", "filesize", "pkg_size", "bytes", "length", NULL };

/* first string reachable inside node that is an http(s) link (depth-limited) */
static const char *first_http(int n, int depth, int need_pkg) {
  if (n < 0 || depth > 4) return NULL;
  const JNode *nd = &doc.nodes[n];
  if (nd->type == J_STR) {
    const char *s = jstr(&doc, n);
    if (is_http(s) && !is_image(s) && (!need_pkg || looks_pkg(s))) return s;
    return NULL;
  }
  if (nd->type == J_ARR || nd->type == J_OBJ)
    for (int c = nd->first; c >= 0; c = doc.nodes[c].next) { const char *r = first_http(c, depth + 1, need_pkg); if (r) return r; }
  return NULL;
}

/* chooses the download link of a game object */
static const char *find_url(int obj) {
  const JNode *o = &doc.nodes[obj];
  for (int c = o->first; c >= 0; c = doc.nodes[c].next) {           /* 1. well-known keys */
    if (!in_list(jkey(&doc, c), URL_KEYS)) continue;
    const char *s = first_http(c, 0, 0); if (s) return s;
    const char *rel = jstr(&doc, c);                                /*    relative link to a pkg */
    if (rel && *rel && looks_pkg(rel)) return rel;
  }
  const char *s = first_http(obj, 0, 1); if (s) return s;           /* 2. any .pkg link anywhere inside */
  return NULL;
}

static void copy_field(char *dst, int dsz, int n) {
  const JNode *nd = &doc.nodes[n];
  if (nd->type == J_STR) snprintf(dst, dsz, "%s", jstr(&doc, n));
  else if (nd->type == J_NUM) snprintf(dst, dsz, "%lld", (long long)nd->num);
  else if (nd->type == J_ARR && nd->first >= 0 && doc.nodes[nd->first].type == J_STR) snprintf(dst, dsz, "%s", jstr(&doc, nd->first));
}
static void get_field(char *dst, int dsz, int obj, const char *const *keys) {
  dst[0] = 0;
  for (int c = doc.nodes[obj].first; c >= 0 && !dst[0]; c = doc.nodes[c].next)
    if (in_list(jkey(&doc, c), keys)) { copy_field(dst, dsz, c); }
}

static uint64_t parse_size(int obj) {
  for (int c = doc.nodes[obj].first; c >= 0; c = doc.nodes[c].next) {
    if (!in_list(jkey(&doc, c), SIZE_KEYS)) continue;
    const JNode *nd = &doc.nodes[c];
    if (nd->type == J_NUM) return nd->num >= 100000 ? (uint64_t)nd->num : 0;
    if (nd->type == J_STR) {
      const char *s = jstr(&doc, c); char *end;
      double v = strtod(s, &end); if (v <= 0) return 0;
      while (*end == ' ') end++;
      if (ieq(end, "gb") || ieq(end, "gib") || ieq(end, "g")) return (uint64_t)(v * 1073741824.0);
      if (ieq(end, "mb") || ieq(end, "mib") || ieq(end, "m")) return (uint64_t)(v * 1048576.0);
      if (ieq(end, "kb") || ieq(end, "kib") || ieq(end, "k")) return (uint64_t)(v * 1024.0);
      if (v >= 100000) return (uint64_t)v;
    }
  }
  return 0;
}

static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
void feed_filename(const char *url, char *out, int outsz) {
  char t[600]; snprintf(t, sizeof(t), "%s", url);
  char *q = strpbrk(t, "?#"); if (q) *q = 0;
  const char *b = strrchr(t, '/'); b = b ? b + 1 : t;
  int o = 0;
  for (; *b && o < outsz - 1; b++) {
    if (*b == '%' && hexv(b[1]) >= 0 && hexv(b[2]) >= 0) { out[o++] = (char)(hexv(b[1]) * 16 + hexv(b[2])); b += 2; }
    else out[o++] = *b;
  }
  out[o] = 0;
  for (int i = 0; i < o; i++) if (out[i] == '/' || out[i] == '\\' || out[i] == ':' || out[i] == '*' || out[i] == '?' || out[i] == '"' || out[i] == '<' || out[i] == '>' || out[i] == '|') out[i] = '_';
  if (!out[0]) snprintf(out, outsz, "download.pkg");
}

static void resolve(const char *link, char *out, int outsz) {
  if (is_http(link) || !gbase || !is_http(gbase)) { snprintf(out, outsz, "%s", link); return; }
  char base[512]; snprintf(base, sizeof(base), "%s", gbase);
  char *q = strpbrk(base, "?#"); if (q) *q = 0;
  if (link[0] == '/') {
    char *hostend = strchr(base + (base[4] == 's' ? 8 : 7), '/'); if (hostend) *hostend = 0;
    snprintf(out, outsz, "%s%s", base, link);
  } else {
    char *slash = strrchr(base + (base[4] == 's' ? 8 : 7), '/');
    if (slash) slash[1] = 0; else strncat(base, "/", sizeof(base) - strlen(base) - 1);
    if (!strncmp(link, "./", 2)) link += 2;
    snprintf(out, outsz, "%s%s", base, link);
  }
}

static void add_game(int obj, const char *url_or_key, const char *fallback_name) {
  if (gcount >= gmax) return;
  Game *g = &G[gcount]; memset(g, 0, sizeof(*g));
  resolve(url_or_key, g->url, sizeof(g->url));
  if (obj >= 0) {
    get_field(g->name, sizeof(g->name), obj, NAME_KEYS);
    get_field(g->id, sizeof(g->id), obj, ID_KEYS);
    get_field(g->version, sizeof(g->version), obj, VER_KEYS);
    get_field(g->region, sizeof(g->region), obj, REG_KEYS);
    get_field(g->type, sizeof(g->type), obj, TYPE_KEYS);
    g->size = parse_size(obj);
  }
  if (!g->name[0] && fallback_name && *fallback_name && !is_http(fallback_name)) snprintf(g->name, sizeof(g->name), "%s", fallback_name);
  if (!g->name[0]) {
    feed_filename(g->url, g->name, sizeof(g->name));
    char *dot = strrchr(g->name, '.'); if (dot && ieq(dot, ".pkg")) *dot = 0;
  }
  gcount++;
}

/* walks the tree; returns number of games added below `n` */
static int walk(int n, int depth) {
  if (n < 0 || depth > 12) return 0;
  const JNode *nd = &doc.nodes[n];
  int before = gcount;
  if (nd->type == J_ARR || nd->type == J_OBJ) {
    for (int c = nd->first; c >= 0; c = doc.nodes[c].next) {
      const JNode *cn = &doc.nodes[c];
      const char *key = jkey(&doc, c);
      if (cn->type == J_STR) {                      /* ["http://.../a.pkg", ...] or {"Name": "http://...pkg"} */
        const char *s = jstr(&doc, c);
        if (is_http(s) && looks_pkg(s) && !(nd->type == J_OBJ && in_list(key, URL_KEYS))) add_game(-1, s, nd->type == J_OBJ ? key : NULL);
      } else if (cn->type == J_OBJ && is_http(key) && !find_url(c)) {   /* {"http://.../a.pkg": {...meta...}} */
        add_game(c, key, NULL);
      } else if (cn->type == J_OBJ || cn->type == J_ARR) {
        int added = walk(c, depth + 1);
        if (added && cn->type == J_OBJ) {            /* entry with its own name + nested file list: "Name - file" */
          char nm[96] = ""; get_field(nm, sizeof(nm), c, NAME_KEYS);
          if (nm[0]) for (int k = gcount - added; k < gcount; k++)
            if (strcmp(G[k].name, nm)) { char t[96]; snprintf(t, sizeof(t), "%.44s - %.44s", nm, G[k].name); memcpy(G[k].name, t, sizeof(G[k].name)); }
        }
        if (!added && cn->type == J_OBJ) {          /* leaf object: is it a game? */
          const char *u = find_url(c);
          if (u) { char nm[96] = ""; get_field(nm, sizeof(nm), c, NAME_KEYS);
                   if (nm[0] || looks_pkg(u) || nd->type == J_ARR) add_game(c, u, is_http(key) ? NULL : key); }
        }
      }
    }
  }
  return gcount - before;
}

int feed_parse(const char *text, size_t len, const char *base_url, Game *out, int max, char *err, int errsz) {
  G = out; gmax = max; gcount = 0; gbase = base_url;
  int root = json_parse(&doc, text, len);
  if (root < 0) { snprintf(err, errsz, "Feed is not valid JSON (or too large)"); return -1; }
  if (doc.nodes[root].type == J_OBJ || doc.nodes[root].type == J_ARR) {
    int added = walk(root, 0);
    if (!added && doc.nodes[root].type == J_OBJ) { const char *u = find_url(root); if (u) add_game(root, u, NULL); }
  }
  if (gcount == 0) { snprintf(err, errsz, "No downloadable entries found in the feed"); return -1; }
  return gcount;
}
