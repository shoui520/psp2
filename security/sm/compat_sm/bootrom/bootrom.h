/*
 * bootrom.h: Types, hardware register definitions, and prototypes for the
 * ePSP (Allegrex) BootROM Stage 2 payload in os0:sm/compat_sm.self.
 *
 * Function, structure, and constant names (except sceSysregSync) are ours.
 */

#ifndef COMPAT_BOOTROM_H
#define COMPAT_BOOTROM_H

typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef signed char		s8;
typedef short			s16;
typedef int			s32;
typedef unsigned int		SceSize;

#define NULL			((void *)0)
#define PHYS(a)			((u32)(a) & 0x1fffffffu)
#define HW(a)			(*(volatile u32 *)(a))

/* Allegrex DDR controller & sysreg MMIO addresses */
#define DDR_FLUSH_PATTERN	0xbc000000u
#define DDR_FLUSH_REQUEST	0xbc000030u
#define DDR_FLUSH_CONTROL	0xbc000044u

#define SYSREG_MEMORY_CONFIG	0xbc100040u
#define SYSREG_RESET_ENABLE	0xbc10004cu
#define SYSREG_BUS_CLOCK	0xbc100050u
#define SYSREG_FUSE_ID_LOW	0xbc100090u
#define SYSREG_FUSE_ID_HIGH	0xbc100094u
#define SYSREG_BUS_SYNC		0xbd000004u

/* KIRK hardware engine at 0xBDE00000 */
#define KIRK_BASE		0xbde00000u
#define KIRK_PHASE		HW(KIRK_BASE + 0x0cu)
#define KIRK_CMD		HW(KIRK_BASE + 0x10u)
#define KIRK_RESULT		HW(KIRK_BASE + 0x14u)
#define KIRK_STATUS		HW(KIRK_BASE + 0x1cu)
#define KIRK_STATUS_ACK		HW(KIRK_BASE + 0x28u)
#define KIRK_SRC		HW(KIRK_BASE + 0x2cu)
#define KIRK_DST		HW(KIRK_BASE + 0x30u)

#define KIRK_CMD_DECRYPT_PRIVATE	1u

/* Kermit shared BootROM SRAM window (0xBFC00000..0xBFC00FFF) */
#define BOOTROM_SRAM_BASE	0xbfc00000u
#define BOOTROM_SRAM_SIZE	0x00001000u
#define BOOTROM_SCRAMBLE_SEED	0xbfc00200u
#define BOOTROM_PARAM_ADDR	0xbfc00fbcu
#define BOOTROM_CHALLENGE_ADDR	0xbfc00fc0u

/* Kermit <-> F00D handshake register */
#define KERMIT_HANDSHAKE_REG	0xabd78000u

/* ePSP IPL load addresses in DDR */
#define IPL_UNCACHED_BASE	0xa8600000u
#define IPL_UNCACHED_BODY	0xa86000a0u
#define IPL_CACHED_ENTRY	0x88600000u

/* 212-byte (0xD4) boot context passed to warm-boot / resume target */
typedef struct SceCompatResumeBootInfo {
	u32 size;			/* 0x00: 0xd4 (212) */
	u32 memBase;			/* 0x04: 0 */
	u32 memSize;			/* 0x08: 0 */
	u32 reserved0c;			/* 0x0c */
	u32 bootFlags;			/* 0x10: 0 */
	void (*callback)(void);		/* 0x14: bootRomDefaultCallback */
	u32 reserved18;			/* 0x18: 0 */
	u32 numProtects;		/* 0x1c: 0 */
	s32 modProtId;			/* 0x20: -1 */
	u32 moduleApiDefaultMode;	/* 0x24: 2 */
	u32 reserved28[2];		/* 0x28..0x2f */
	u32 maxModules;			/* 0x30: 1200 (0x4b0) */
	u32 reserved34[3];		/* 0x34..0x3f */
	u32 buildVersion;		/* 0x40: 0 */
	u8 reserved44[0x90];		/* 0x44..0xd3 */
} SceCompatResumeBootInfo;

_Static_assert(sizeof(SceCompatResumeBootInfo) == 0xd4, "SceCompatResumeBootInfo size");

#define sync()			__asm__ __volatile__("sync" ::: "memory")

static inline __attribute__((always_inline)) u32 cp0Config(void)
{
	u32 v;

	__asm__ __volatile__("mfc0 %0, $16" : "=r"(v));
	return v;
}

static inline __attribute__((always_inline)) u32 wsbw32(u32 v)
{
	u32 out;

	__asm__("wsbw %0, %1" : "=r"(out) : "r"(v));
	return out;
}

static inline __attribute__((always_inline)) u32 ror32(u32 v, u32 shift)
{
	return (v >> shift) | (v << (32u - shift));
}

struct SceCompatUnaligned32 {
	u32 v;
} __attribute__((packed));

static inline __attribute__((always_inline)) u32 load32Ua(const void *p)
{
	return ((const struct SceCompatUnaligned32 *)p)->v;
}

static inline __attribute__((always_inline)) void store32Ua(void *p, u32 v)
{
	((struct SceCompatUnaligned32 *)p)->v = v;
}

void bootRomMain(void) __attribute__((noreturn, section(".text.entry")));
void icacheInvalidateAll(void);
void dcacheWritebackInvalidateAll(void);
int kirkDecryptCmd1(void *dst, const void *src);
u32 memcpyWords(u32 *dst, const u32 *src, int size);
void *memsetBytes(void *dst, int val, int size);
void delayLoop(u32 count);
void bootRomPanicHook(void);
void bootRomDefaultCallback(void);
void resumeDispatch(u32 bootParam) __attribute__((noreturn));
int resumeMain(u32 bootParam);
void *transferBootKey(void);
int setupKeySeedAndVerify(void);
int initDdrAndHandshake(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6);
void abortClearBootRam(void) __attribute__((noreturn));
void sha1Transform(const u32 *block, u32 *state);
void sha1Compress(u32 *W, u32 *state);
int sha1Digest(const void *data, u32 len, void *outDigest);
int sceSysregSync(u32 mask);

#endif /* COMPAT_BOOTROM_H */
