/*
 * ePSP IPL: the PSP-side boot loader of the PSP emulator (compat mode).
 *
 * Runs on the Allegrex from 0x88600000. It decrypts and loads the PSP kernel
 * modules named by pspbtcnf.bin out of a file table the Vita side leaves at
 * 0x8B000000, then starts sceLoaderCore. Most of it is PSP kernel code linked
 * in statically: memlmd's module decrypter, loadcore's ELF loader
 * (loadelf_boot.c) and loadcore's cache/interrupt primitives.
 */
#ifndef IPL_H
#define IPL_H

typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef signed char		s8;
typedef short			s16;
typedef int			s32;
typedef unsigned int		SceSize;

typedef void (*SceIplHaltEntry)(void *memBase, SceSize memSize);

#define NULL			((void *)0)

#define KSEG1(a)		((u32)(a) | 0xA0000000)
#define PHYS(a)			((u32)(a) & 0x1FFFFFFF)
#define HW(a)			(*(volatile u32 *)(a))

#define SCE_ERROR_ERROR			0x80020001
#define SCE_ERROR_KERNEL_UNSUPPORTED	0x80020148	/* "PSP cannot load this image" */
#define SCE_ERROR_KERNEL_NOT_LINKED	0x8002013A
#define SCE_ERROR_KERNEL_NO_DECRYPTOR	0x8002013A

/* Allegrex primitives */
#define sync()			__asm__ __volatile__("sync" ::: "memory")

/* CACHE 0x14: index writeback-invalidate one data cache line */
#define dcacheIndexWritebackInvalidate(index) \
	__asm__ __volatile__("cache 0x14, 0(%0)" :: "r"(index))

static inline u32 cp0Config(void)
{
	u32 v;

	__asm__ __volatile__("mfc0 %0, $16" : "=r"(v));
	return v;
}

/* Interrupt control and caches: cache.S (loadcore) */
int sceKernelCpuSuspendIntr(void);
void sceKernelCpuResumeIntr(int flags);
void sceKernelCpuResumeIntrWithSync(int flags);
int sceKernelIsCpuIntrSuspended(int flags);
int sceKernelIsCpuIntrEnable(void);
int sceKernelDcacheWritebackAll(void);
int sceKernelDcacheWritebackInvalidateAll(void);
int sceKernelIcacheInvalidateAll(void);

/* sysreg */
int sceSysregSync(u32 mask);

/* libc.c */
void *memset(void *dst, int c, SceSize n);
void *memcpy(void *dst, const void *src, SceSize n);
int memcmp(const void *a, const void *b, SceSize n);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, SceSize n);
char *strncpy(char *dst, const char *src, SceSize n);

/* kl4e.c */
int decompressKl4e(void *dest, SceSize size, const void *src, const void **srcEnd);

/* halt.c and its linked code boundary */
void haltStub(void *memBase, SceSize memSize) __attribute__((noreturn));
extern const u8 haltStubEnd[];

/* boot_services.c */
void Panic(const char *func, int line) __attribute__((noreturn));
int Kprintf(const char *fmt, ...);
int bootStartupHook(void);
void bootHandoffCallback(void);
SceSize bootStringLength(const u8 *string);
int memlmdPrepareGetLength(u8 *buffer, SceSize size);
void *transferBootKey(void);

/* boot_handoff.S: the target inherits RA and runs on stackTop - 64. */
int callWithBootStack(int (*entry)(u32 mode, void *args), u32 mode,
		     void *args, void *stackTop);

#endif
