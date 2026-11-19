#ifndef JSON_H
#define JSON_H
#include <stddef.h>
#include <stdint.h>

/* Tiny allocation-free JSON DOM parser (caller supplies the pools). Lenient: tolerates BOM,
   trailing commas and // or block comments. */
typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } JType;
typedef struct { uint8_t type; int32_t key, str, first, next; double num; } JNode;
typedef struct { JNode *nodes; int n, cap; char *strs; int slen, scap; } JDoc;

int json_parse(JDoc *d, const char *text, size_t len); /* returns root node index or -1 */
const char *jstr(const JDoc *d, int idx);              /* string value of a J_STR node, else NULL */
const char *jkey(const JDoc *d, int idx);              /* member key of a node, or "" */
int jget(const JDoc *d, int obj, const char *key);     /* case-insensitive member lookup, -1 if none */
#endif
