#ifndef GLYPHS_H
#define GLYPHS_H

#include <pebble.h>

static inline int char2index(char c) {
   if (c >= '0' && c <= '9')
      return c - '0';
   if (c >= 'a' && c <= 'z')
      return c - 'a' + 10;
   if (c >= 'A' && c <= 'Z')
      return c - 'A' + 10;
   return 36;
}

int drawGlyph(GPoint p, int8_t g, uint8_t s, uint8_t c);

static inline int drawChar(GPoint p, char g, uint8_t s, uint8_t c) {
   return drawGlyph(p, char2index(g), s, c);
}

#endif // !GLYPHS_H
