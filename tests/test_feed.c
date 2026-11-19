#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "feed.h"
static Game games[64];
int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    FILE *f = fopen(argv[i], "rb"); if (!f) { perror(argv[i]); return 1; }
    static char buf[1 << 20]; size_t n = fread(buf, 1, sizeof(buf), f); fclose(f);
    char err[128] = ""; int c = feed_parse(buf, n, "https://raw.example.com/repo/main/Gms.json", games, 64, err, sizeof(err));
    printf("== %s: %d %s\n", argv[i], c, err);
    for (int k = 0; k < c; k++) printf("  [%s] id=%s ver=%s reg=%s type=%s size=%llu\n      %s\n", games[k].name, games[k].id, games[k].version, games[k].region, games[k].type, (unsigned long long)games[k].size, games[k].url);
  }
  return 0;
}
