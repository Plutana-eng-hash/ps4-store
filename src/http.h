#ifndef HTTP_H
#define HTTP_H
#include <stdint.h>
#include <stddef.h>

typedef struct { int tmpl, conn, req; int status; int has_len; uint64_t content_len; } Http;

int net_start(char *err, int errsz);                 /* sceNet + sceSsl + sceHttp init; 0 on success */
/* GET url (http or https), following up to 6 redirects. range_from > 0 sends a Range header.
   Returns 0 when the final status is 200/206/416 (check h->status); otherwise <0 with err filled in. */
int http_get(Http *h, const char *url, uint64_t range_from, char *err, int errsz);
int http_read(Http *h, void *buf, size_t n);         /* bytes read, 0 at end, <0 on error */
void http_close(Http *h);
#endif
