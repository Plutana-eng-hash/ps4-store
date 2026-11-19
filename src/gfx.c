#include <string.h>
#include "gfx.h"

static inline uint32_t blend(uint32_t dst, uint32_t src, int a) { /* a: 0..255 */
  if (a >= 255) return src | 0xFF000000u;
  if (a <= 0) return dst;
  uint32_t rb = ((dst & 0xFF00FFu) * (255 - a) + (src & 0xFF00FFu) * a) >> 8;
  uint32_t g = (((dst >> 8) & 0xFF) * (255 - a) + ((src >> 8) & 0xFF) * a) >> 8;
  return 0xFF000000u | (rb & 0xFF00FFu) | ((g & 0xFF) << 8);
}

void gfx_fill(Surf *s, int x, int y, int w, int h, uint32_t c) {
  int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w > s->w ? s->w : x + w, y1 = y + h > s->h ? s->h : y + h;
  int a = c >> 24;
  for (int j = y0; j < y1; j++) {
    uint32_t *p = s->px + (size_t)j * s->stride;
    if (a >= 255) for (int i = x0; i < x1; i++) p[i] = c;
    else for (int i = x0; i < x1; i++) p[i] = blend(p[i], c, a);
  }
}

void gfx_vgradient(Surf *s, int x, int y, int w, int h, uint32_t t, uint32_t b) {
  for (int j = 0; j < h; j++) {
    int k = h > 1 ? j * 255 / (h - 1) : 0;
    uint32_t c = 0;
    for (int sh = 0; sh < 32; sh += 8) {
      int tv = (t >> sh) & 0xFF, bv = (b >> sh) & 0xFF;
      c |= (uint32_t)((tv * (255 - k) + bv * k) / 255) << sh;
    }
    gfx_fill(s, x, y + j, w, 1, c);
  }
}

void gfx_rrect(Surf *s, int x, int y, int w, int h, int r, uint32_t c) {
  if (r * 2 > h) r = h / 2;
  if (r * 2 > w) r = w / 2;
  for (int j = 0; j < h; j++) {
    int inset = 0;
    int jj = j < r ? j : (j >= h - r ? h - 1 - j : r);   /* row index inside the corner band */
    if (jj < r) {
      int dy2 = 2 * (r - jj) - 1;
      while (inset < r) { int dx2 = 2 * (r - inset) - 1; if (dx2 * dx2 + dy2 * dy2 <= 4 * r * r) break; inset++; }
    }
    gfx_fill(s, x + inset, y + j, w - 2 * inset, 1, c);
  }
}

static int utf8_next(const char **pp) {
  const unsigned char *p = (const unsigned char *)*pp;
  int c = *p++;
  if (c < 0x80) { *pp = (const char *)p; return c; }
  int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0, cp = n == 3 ? c & 7 : n == 2 ? c & 15 : n == 1 ? c & 31 : '?';
  for (int i = 0; i < n; i++) { if ((*p & 0xC0) != 0x80) { cp = '?'; break; } cp = (cp << 6) | (*p++ & 63); }
  *pp = (const char *)p; return cp;
}
static const FontGlyph *glyph(const Font *f, int cp) {
  if (cp >= 32 && cp <= 126) return &f->glyphs[cp - 32];
  for (int i = 95; i < f->count; i++) if (f->glyphs[i].cp == cp) return &f->glyphs[i];
  if (cp == 0x2009 || cp == 0x200A || cp == 0xA0) return &f->glyphs[0];
  return &f->glyphs['?' - 32];
}

int gfx_text_width(const Font *f, const char *s) {
  int w = 0;
  while (*s) { int cp = utf8_next(&s); if (cp < 32) continue; w += glyph(f, cp)->adv; }
  return w;
}

static int draw_glyph(Surf *s, const Font *f, const FontGlyph *g, int x, int base, uint32_t c) {
  if (g->w && g->h) {
    int gx = x + g->xoff, gy = base + g->yoff, ca = c >> 24;
    const unsigned char *src = f->px + g->off;
    for (int j = 0; j < g->h; j++) {
      int py = gy + j; if (py < 0 || py >= s->h) continue;
      uint32_t *dst = s->px + (size_t)py * s->stride;
      for (int i = 0; i < g->w; i++) {
        int px = gx + i; if (px < 0 || px >= s->w) continue;
        int a = src[j * g->w + i]; if (!a) continue;
        dst[px] = blend(dst[px], c, a * ca / 255);
      }
    }
  }
  return g->adv;
}

int gfx_text(Surf *s, const Font *f, int x, int base, const char *t, uint32_t c) {
  int x0 = x;
  while (*t) { int cp = utf8_next(&t); if (cp < 32) continue; x += draw_glyph(s, f, glyph(f, cp), x, base, c); }
  return x - x0;
}
int gfx_text_right(Surf *s, const Font *f, int right, int base, const char *t, uint32_t c) { return gfx_text(s, f, right - gfx_text_width(f, t), base, t, c); }
int gfx_text_center(Surf *s, const Font *f, int cx, int base, const char *t, uint32_t c) { return gfx_text(s, f, cx - gfx_text_width(f, t) / 2, base, t, c); }

int gfx_text_fit(Surf *s, const Font *f, int x, int base, int max_w, const char *t, uint32_t c) {
  if (gfx_text_width(f, t) <= max_w) return gfx_text(s, f, x, base, t, c);
  int dots = glyph(f, '.')->adv * 3, w = 0; const char *p = t, *cut = t;
  while (*p) { const char *q = p; int cp = utf8_next(&q); int aw = cp < 32 ? 0 : glyph(f, cp)->adv; if (w + aw + dots > max_w) break; w += aw; p = q; cut = q; }
  char buf[256]; int n = (int)(cut - t); if (n > 250) n = 250;
  memcpy(buf, t, n); memcpy(buf + n, "...", 4);
  return gfx_text(s, f, x, base, buf, c);
}

int gfx_text_wrap(Surf *s, const Font *f, int x, int base, int max_w, int max_lines, const char *t, uint32_t c) {
  int lines = 0;
  while (*t && lines < max_lines) {
    while (*t == ' ') t++;
    const char *p = t, *brk = NULL; int w = 0;
    while (*p) {
      const char *q = p; int cp = utf8_next(&q); int aw = cp < 32 ? 0 : glyph(f, cp)->adv;
      if (w + aw > max_w) break;
      if (cp == ' ') brk = p;
      w += aw; p = q;
    }
    const char *end = *p ? (brk && brk > t ? brk : p) : p;
    char buf[256]; int n = (int)(end - t); if (n > 250) n = 250; memcpy(buf, t, n); buf[n] = 0;
    int last = (lines == max_lines - 1) && *end;
    if (last) gfx_text_fit(s, f, x, base + lines * f->line_h, max_w, t, c);
    else gfx_text(s, f, x, base + lines * f->line_h, buf, c);
    lines++; t = end;
    if (last) break;
  }
  return lines;
}
