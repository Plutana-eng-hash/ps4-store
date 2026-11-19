#ifndef FONT_H
#define FONT_H
#include <stdint.h>
typedef struct { int cp, adv, w, h, xoff, yoff, off; } FontGlyph; /* yoff: top of bitmap relative to baseline */
typedef struct { int line_h, ascent, count; const FontGlyph *glyphs; const unsigned char *px; } Font;
extern const Font font_small, font_body, font_bold, font_title;
#endif
