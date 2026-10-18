/* Private candidate: BSOD display and mapping contracts from native code. */
#pragma once
#include <stdint.h>
#include <sysmem/cpu.h>
#include <sysmem/dipsw.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sblaimgr.h>

typedef int (*BsodFormatCallback)(void *context, const uint32_t *state, int character);
int bsodFormat(BsodFormatCallback callback, void *context, int count,
	       const char *format, const void *args);
#include <stddef.h>
#include <string.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <excpmgr/excpmgr.h>

typedef struct BsodFramebuffer {
	uint32_t physical;
	uint32_t pitch;
	uint32_t x;
	uint32_t width;
	uint32_t height;
	uint32_t scaleX;
	uint32_t scaleY;
} BsodFramebuffer;

typedef struct BsodDisplay {
	uint32_t *pixels;
	int lock;
	SceKernelSpinlock captureLock;
	BsodFramebuffer previous;
	BsodFramebuffer current;
} BsodDisplay;
_Static_assert(sizeof(BsodFramebuffer) == 0x1c, "BSOD framebuffer fields");
_Static_assert(sizeof(BsodDisplay) == 0x44, "BSOD display fields");
extern BsodDisplay s_bsodDisplay;
extern uint8_t *s_bsodIftu[3];
extern uint8_t *s_bsodSyscon[2];
extern int s_bsodQafEnabled;


int _sceKernelBsodIftuInit(int dolce);
int _sceKernelBsodSysconInit(void);
int _sceKernelBsodDummyFsInit(void);
int bsodInitializePutchar(void);
int bsodReadFramebuffer(unsigned bank, BsodFramebuffer *framebuffer);
void bsodWriteFramebuffer(unsigned bank, const BsodFramebuffer *framebuffer);
void bsodCrashHandler(int kind) __attribute__((noreturn));
void bsodDebugCrashCallback(void) __attribute__((noreturn));
extern SceExcpmgrHandler bsodPrefetchEntry;
extern SceExcpmgrHandler bsodDataEntry;
extern SceExcpmgrHandler bsodUndefinedEntry;
