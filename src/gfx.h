#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#include "font.h"
typedef struct { uint32_t *px; int w, h, stride; } Surf; /* stride in pixels; pixel = 0xAARRGGBB */
#define RGB(r, g, b) (0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define RGBA(r, g, b, a) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
void gfx_fill(Surf *s, int x, int y, int w, int h, uint32_t c);                 /* alpha-blended if a < 255 */
void gfx_vgradient(Surf *s, int x, int y, int w, int h, uint32_t top, uint32_t bottom);
void gfx_rrect(Surf *s, int x, int y, int w, int h, int r, uint32_t c);
int gfx_text_width(const Font *f, const char *utf8);
int gfx_text(Surf *s, const Font *f, int x, int baseline, const char *utf8, uint32_t c);  /* returns advance */
int gfx_text_right(Surf *s, const Font *f, int right, int baseline, const char *utf8, uint32_t c);
int gfx_text_center(Surf *s, const Font *f, int cx, int baseline, const char *utf8, uint32_t c);
int gfx_text_fit(Surf *s, const Font *f, int x, int baseline, int max_w, const char *utf8, uint32_t c); /* ellipsis */
int gfx_text_wrap(Surf *s, const Font *f, int x, int baseline, int max_w, int max_lines, const char *utf8, uint32_t c); /* returns lines */
#endif
