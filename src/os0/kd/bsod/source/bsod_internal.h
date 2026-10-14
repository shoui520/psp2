/* Private candidate: BSOD display and mapping contracts from native code. */
#pragma once
#include <stdint.h>

typedef int (*BsodFormatCallback)(void *context, const uint32_t *state, int character);
int bsodFormat(BsodFormatCallback callback, void *context, int count,
	       const char *format, const void *args);
#include <stddef.h>
#include <string.h>
#include <sce/debug.h>
#include <psp2kern/kernel/sysmem.h>

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
	uint32_t captureLock;
	BsodFramebuffer previous;
	BsodFramebuffer current;
} BsodDisplay;
_Static_assert(sizeof(BsodFramebuffer) == 0x1c, "BSOD framebuffer fields");
_Static_assert(sizeof(BsodDisplay) == 0x44, "BSOD display fields");
extern BsodDisplay s_bsodDisplay;
extern uint8_t *s_bsodIftu[3];
extern uint8_t *s_bsodSyscon[2];
extern int s_bsodQafEnabled;

int ksceKernelCheckDipsw(unsigned bit);
int ksceSblAimgrIsCEX(void);
int ksceSblAimgrIsDolce(void);
int SceQafMgrForDriver_382C71E8(void);
unsigned ksceKernelCpuSuspendIntr(void);
void ksceKernelCpuResumeIntr(unsigned state);
int ksceExcpmgrRegisterHandler(unsigned kind, unsigned priority, void *handler);
void SceDebugForKernel_67E9D1EC(void *handler);
int ksceKernelEnableCrashDump(int enable);

int _sceKernelBsodIftuInit(int dolce);
int _sceKernelBsodSysconInit(void);
int _sceKernelBsodDummyFsInit(void);
int bsodInitializePutchar(void);
int bsodReadFramebuffer(unsigned bank, BsodFramebuffer *framebuffer);
void bsodWriteFramebuffer(unsigned bank, const BsodFramebuffer *framebuffer);
void bsodCrashHandler(int kind) __attribute__((noreturn));
void bsodDebugCrashCallback(void) __attribute__((noreturn));
extern uint32_t bsodPrefetchEntry[];
extern uint32_t bsodDataEntry[];
extern uint32_t bsodUndefinedEntry[];
