#ifndef SYS_H
#define SYS_H
/* Declarations for system functions that are not pulled in from the OpenOrbis headers.
   Functions bound with __asm__("realName") get a private C name so they can never clash with a
   toolchain header that declares the same function with slightly different types. */
#include <stdint.h>
#include <stddef.h>

/* ---- libkernel ---- */
int sys_usleep(unsigned int us) __asm__("sceKernelUsleep");
uint64_t sys_process_time(void) __asm__("sceKernelGetProcessTime");   /* microseconds */
int sys_rename(const char *from, const char *to) __asm__("sceKernelRename");
int64_t sys_direct_mem_size(void) __asm__("sceKernelGetDirectMemorySize");
int sys_alloc_direct(int64_t start, int64_t end, size_t len, size_t align, int type, int64_t *phys) __asm__("sceKernelAllocateDirectMemory");
int sys_map_direct(void **addr, size_t len, int prot, int flags, int64_t phys, size_t align) __asm__("sceKernelMapDirectMemory");

/* ---- libSceVideoOut ---- */
#define VO_USER_SYSTEM 0xFF
#define VO_FORMAT_A8R8G8B8_SRGB 0x80000000u /* pixel = 0xAARRGGBB */
#define VO_TILING_LINEAR 1
#define VO_ASPECT_16_9 0
#define VO_FLIP_VSYNC 1
int sceVideoOutOpen(int userId, int busType, int index, const void *param);
void sceVideoOutSetBufferAttribute(void *attr, uint32_t pixelFormat, uint32_t tilingMode, uint32_t aspectRatio,
                                   uint32_t width, uint32_t height, uint32_t pitchInPixel);
int sceVideoOutRegisterBuffers(int handle, int startIndex, void *const *addresses, int bufferNum, const void *attr);
int sceVideoOutSubmitFlip(int handle, int bufferIndex, uint32_t flipMode, int64_t flipArg);
int sceVideoOutIsFlipPending(int handle);
int sceVideoOutSetFlipRate(int handle, int rate);

/* ---- libSceSystemService (weak: only used if the stub exports it) ---- */
int sceSystemServiceHideSplashScreen(void) __attribute__((weak));

/* ---- libSceSsl / libSceHttp ---- */
int sceSslInit(size_t poolSize);
int sceSslTerm(int sslCtxId);
int sceHttpInit(int libnetMemId, int libsslCtxId, size_t poolSize);
int sceHttpTerm(int httpCtxId);
int sceHttpCreateTemplate(int httpCtxId, const char *userAgent, int httpVer, int autoProxy);
int sceHttpDeleteTemplate(int tmplId);
int sceHttpCreateConnectionWithURL(int tmplId, const char *url, int enableKeepalive);
int sceHttpDeleteConnection(int connId);
int sceHttpCreateRequestWithURL(int connId, int method, const char *url, uint64_t contentLength);
int sceHttpDeleteRequest(int reqId);
int sceHttpSendRequest(int reqId, const void *postData, size_t size);
int sceHttpReadData(int reqId, void *data, size_t size);
int sceHttpGetStatusCode(int reqId, int *statusCode);
int sceHttpGetResponseContentLength(int reqId, int *result, uint64_t *contentLength);
int sceHttpGetAllResponseHeaders(int reqId, char **header, size_t *headerSize);
int sceHttpAddRequestHeader(int id, const char *name, const char *value, uint32_t mode);
int sceHttpSetAutoRedirect(int id, int enable);
int sceHttpSetConnectTimeOut(int id, uint32_t usec);
int sceHttpSetRecvTimeOut(int id, uint32_t usec);
int sceHttpSetResolveTimeOut(int id, uint32_t usec);
int sceHttpsDisableOption(int id, uint32_t flags);
#define HTTPS_SERVER_VERIFY 0x01
#define HTTPS_CN_CHECK 0x04
#define HTTPS_NOT_AFTER_CHECK 0x08
#define HTTPS_NOT_BEFORE_CHECK 0x10
#define HTTPS_KNOWN_CA_CHECK 0x20

/* ---- DualShock 4 button bits (scePadReadState().buttons) ---- */
#define BTN_L3 0x0002u
#define BTN_R3 0x0004u
#define BTN_OPTIONS 0x0008u
#define BTN_UP 0x0010u
#define BTN_RIGHT 0x0020u
#define BTN_DOWN 0x0040u
#define BTN_LEFT 0x0080u
#define BTN_L2 0x0100u
#define BTN_R2 0x0200u
#define BTN_L1 0x0400u
#define BTN_R1 0x0800u
#define BTN_TRIANGLE 0x1000u
#define BTN_CIRCLE 0x2000u
#define BTN_CROSS 0x4000u
#define BTN_SQUARE 0x8000u
#endif
