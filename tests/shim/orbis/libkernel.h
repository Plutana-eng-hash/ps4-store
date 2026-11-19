#include <stddef.h>
typedef struct { int x; } OrbisNotificationRequest;
int sceKernelSendNotificationRequest(int, OrbisNotificationRequest *, size_t, int);
