#include <string.h>
#include "json.h"

typedef struct { const char *p, *end; JDoc *d; int depth; int err; } P;

static void ws(P *s) {
  for (;;) {
    while (s->p < s->end && (*s->p == ' ' || *s->p == '\t' || *s->p == '\n' || *s->p == '\r')) s->p++;
    if (s->p + 1 < s->end && s->p[0] == '/' && s->p[1] == '/') { while (s->p < s->end && *s->p != '\n') s->p++; continue; }
    if (s->p + 1 < s->end && s->p[0] == '/' && s->p[1] == '*') {
      s->p += 2; while (s->p + 1 < s->end && !(s->p[0] == '*' && s->p[1] == '/')) s->p++;
      s->p += 2; if (s->p > s->end) s->p = s->end; continue;
    }
    break;
  }
}

static int newnode(P *s, int type) {
  JDoc *d = s->d;
  if (d->n >= d->cap) { s->err = 1; return -1; }
  JNode *n = &d->nodes[d->n];
  memset(n, 0, sizeof(*n));
  n->type = (uint8_t)type; n->key = -1; n->str = -1; n->first = -1; n->next = -1;
  return d->n++;
}

static void putc_pool(P *s, char c) {
  if (s->d->slen >= s->d->scap) { s->err = 1; return; }
  s->d->strs[s->d->slen++] = c;
}
static void put_utf8(P *s, unsigned cp) {
  if (cp < 0x80) putc_pool(s, (char)cp);
  else if (cp < 0x800) { putc_pool(s, (char)(0xC0 | (cp >> 6))); putc_pool(s, (char)(0x80 | (cp & 63))); }
  else if (cp < 0x10000) { putc_pool(s, (char)(0xE0 | (cp >> 12))); putc_pool(s, (char)(0x80 | ((cp >> 6) & 63))); putc_pool(s, (char)(0x80 | (cp & 63))); }
  else { putc_pool(s, (char)(0xF0 | (cp >> 18))); putc_pool(s, (char)(0x80 | ((cp >> 12) & 63))); putc_pool(s, (char)(0x80 | ((cp >> 6) & 63))); putc_pool(s, (char)(0x80 | (cp & 63))); }
}
static int hex4(const char *p, const char *end, unsigned *out) {
  if (end - p < 4) return 0;
  unsigned v = 0;
  for (int i = 0; i < 4; i++) {
    char c = p[i]; v <<= 4;
    if (c >= '0' && c <= '9') v |= c - '0'; else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10; else return 0;
  }
  *out = v; return 1;
}

/* parses a quoted string into the pool; returns pool offset or -1 */
static int parse_string(P *s) {
  if (s->p >= s->end || *s->p != '"') { s->err = 1; return -1; }
  s->p++;
  int off = s->d->slen;
  while (s->p < s->end && *s->p != '"') {
    char c = *s->p++;
    if (c != '\\') { putc_pool(s, c); continue; }
    if (s->p >= s->end) break;
    c = *s->p++;
    switch (c) {
      case 'n': putc_pool(s, '\n'); break; case 't': putc_pool(s, '\t'); break;
      case 'r': putc_pool(s, '\r'); break; case 'b': putc_pool(s, '\b'); break;
      case 'f': putc_pool(s, '\f'); break;
      case 'u': {
        unsigned cp;
        if (!hex4(s->p, s->end, &cp)) { s->err = 1; return -1; }
        s->p += 4;
        if (cp >= 0xD800 && cp < 0xDC00 && s->end - s->p >= 6 && s->p[0] == '\\' && s->p[1] == 'u') {
          unsigned lo;
          if (hex4(s->p + 2, s->end, &lo) && lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); s->p += 6; }
        }
        put_utf8(s, cp); break;
      }
      default: putc_pool(s, c); break; /* \" \\ \/ and unknown */
    }
  }
  if (s->p >= s->end) { s->err = 1; return -1; }
  s->p++; /* closing quote */
  putc_pool(s, 0);
  return s->err ? -1 : off;
}

static int parse_value(P *s);

static int parse_container(P *s, int is_obj) {
  int self = newnode(s, is_obj ? J_OBJ : J_ARR);
  if (self < 0) return -1;
  if (++s->depth > 48) { s->err = 1; return -1; }
  s->p++; /* [ or { */
  int last = -1;
  for (;;) {
    ws(s);
    if (s->p >= s->end) { s->err = 1; return -1; }
    char close = is_obj ? '}' : ']';
    if (*s->p == close) { s->p++; break; }
    if (*s->p == ',') { s->p++; continue; }
    int key = -1;
    if (is_obj) {
      key = parse_string(s); if (key < 0) return -1;
      ws(s);
      if (s->p >= s->end || *s->p != ':') { s->err = 1; return -1; }
      s->p++; ws(s);
    }
    int v = parse_value(s);
    if (v < 0) return -1;
    s->d->nodes[v].key = key;
    if (last < 0) s->d->nodes[self].first = v; else s->d->nodes[last].next = v;
    last = v;
  }
  s->depth--;
  return self;
}

static int parse_value(P *s) {
  ws(s);
  if (s->p >= s->end) { s->err = 1; return -1; }
  char c = *s->p;
  if (c == '{') return parse_container(s, 1);
  if (c == '[') return parse_container(s, 0);
  if (c == '"') {
    int off = parse_string(s); if (off < 0) return -1;
    int n = newnode(s, J_STR); if (n < 0) return -1;
    s->d->nodes[n].str = off; return n;
  }
  if (c == '-' || (c >= '0' && c <= '9')) {
    int neg = 0; double v = 0;
    if (c == '-') { neg = 1; s->p++; }
    while (s->p < s->end && *s->p >= '0' && *s->p <= '9') v = v * 10 + (*s->p++ - '0');
    if (s->p < s->end && *s->p == '.') { s->p++; double f = 0.1; while (s->p < s->end && *s->p >= '0' && *s->p <= '9') { v += (*s->p++ - '0') * f; f /= 10; } }
    if (s->p < s->end && (*s->p == 'e' || *s->p == 'E')) {
      s->p++; int en = 0, eneg = 0;
      if (s->p < s->end && (*s->p == '+' || *s->p == '-')) { eneg = *s->p == '-'; s->p++; }
      while (s->p < s->end && *s->p >= '0' && *s->p <= '9') en = en * 10 + (*s->p++ - '0');
      if (en > 300) en = 300;
      while (en-- > 0) v = eneg ? v / 10 : v * 10;
    }
    int n = newnode(s, J_NUM); if (n < 0) return -1;
    s->d->nodes[n].num = neg ? -v : v; return n;
  }
  if (s->end - s->p >= 4 && !strncmp(s->p, "true", 4)) { s->p += 4; int n = newnode(s, J_BOOL); if (n >= 0) s->d->nodes[n].num = 1; return n; }
  if (s->end - s->p >= 5 && !strncmp(s->p, "false", 5)) { s->p += 5; return newnode(s, J_BOOL); }
  if (s->end - s->p >= 4 && !strncmp(s->p, "null", 4)) { s->p += 4; return newnode(s, J_NULL); }
  s->err = 1; return -1;
}

int json_parse(JDoc *d, const char *text, size_t len) {
  d->n = 0; d->slen = 0;
  P s = { text, text + len, d, 0, 0 };
  if (len >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) s.p += 3;
  int root = parse_value(&s);
  return s.err ? -1 : root;
}

const char *jstr(const JDoc *d, int idx) {
  if (idx < 0 || d->nodes[idx].type != J_STR) return NULL;
  return d->strs + d->nodes[idx].str;
}
const char *jkey(const JDoc *d, int idx) {
  if (idx < 0 || d->nodes[idx].key < 0) return "";
  return d->strs + d->nodes[idx].key;
}
static int ci_eq(const char *a, const char *b) {
  for (; *a && *b; a++, b++) { char x = *a, y = *b; if (x >= 'A' && x <= 'Z') x += 32; if (y >= 'A' && y <= 'Z') y += 32; if (x != y) return 0; }
  return *a == *b;
}
int jget(const JDoc *d, int obj, const char *key) {
  if (obj < 0 || d->nodes[obj].type != J_OBJ) return -1;
  for (int c = d->nodes[obj].first; c >= 0; c = d->nodes[c].next) if (ci_eq(jkey(d, c), key)) return c;
  return -1;
}
