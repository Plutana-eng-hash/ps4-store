#ifndef FEED_H
#define FEED_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
  char name[96], url[512], id[24], version[16], region[24], type[24];
  uint64_t size; /* bytes, 0 = unknown */
} Game;

/* Parses a JSON feed (any reasonable layout - see README) into out[]. `base_url` resolves relative
   links. Returns number of games, or -1 with a message in err. */
int feed_parse(const char *text, size_t len, const char *base_url, Game *out, int max, char *err, int errsz);
void feed_filename(const char *url, char *out, int outsz); /* decoded last path segment */
#endif
