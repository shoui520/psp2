/*
 * crashdump_tzs.c: the crash dump (monitor call 0x122). The secure world
 * stops the other cores and the grab ports, then has DMAC5 encrypt the
 * Non-secure kernel's memory, with the key in key ring slot 27, into the
 * dump area at the top of DRAM: a header with the monitor call's arguments,
 * the sysroot's session ID and coredump key and the crash reports, the
 * DRAM below the dump area in one piece, and every other page the
 * Non-secure kernel maps. Each source is cleared once copied. Then the
 * console powers off.
 *
 * Names other than sceCrashDumpMonitorCall's (its debug record) are ours.
 */
#include "driver_tzs.h"

#include <dmacmgr/dmac_regs.h>
#include <dmacmgr/dmacmgr.h>
#include <sysmem/cp15.h>
#include <sysmem/memblock.h>

#define CRASHDUMP_HEADER_SIZE		0x20000
#define CRASHDUMP_ZERO_PADDR		0x40000000	/* the first page of DRAM */
#define CRASHDUMP_DRAM_BASE		0x40200000	/* the Non-secure kernel's DRAM */
#define CRASHDUMP_STAGING_PADDR		0x20000000	/* the mapped pages, before the copy */
#define CRASHDUMP_PAGE_MAX		0x4000

#define CRASHDUMP_DMAC_CMD_ENCRYPT	0x0c002309
#define CRASHDUMP_DMAC_KEY_SLOT		27

typedef struct SceCrashDumpMagic {
	SceUInt32 words[2];
} SceCrashDumpMagic;

typedef struct SceCrashDumpIv {
	SceUInt32 words[4];
} SceCrashDumpIv;

/* the dump area's header */
typedef struct SceCrashDumpHeader {
	SceCrashDumpMagic magic;
	SceUInt32 version;
	SceUInt32 cpu;				/* the core that made the dump */
	SceUInt32 args[4];			/* 0x10: the monitor call's */
	SceUInt8 sessionId[0x10];		/* 0x20 */
	SceUInt8 coredumpKey[0x20];		/* 0x30 */
	SceUInt32 dramRegionCount;		/* 0x50 */
	SceUInt32 dramBase;
	SceSize dramSize;
	SceUInt32 dramOffset;			/* in the dump area */
	SceUInt8 reserved0[0x54];
	SceUInt32 pageCount;			/* 0xb4 */
	SceSize pageSize;
	SceUInt32 pageOffset;			/* in the dump area */
	SceUInt32 reserved1[2];			/* 0xc0 */
	SceUInt8 reserved2[0x7f38];
	SceUInt8 sharedMemory[0x8000];		/* 0x8000: the sysroot's reports and logs */
	SceUInt32 pages[CRASHDUMP_PAGE_MAX];	/* 0x10000: each mapped page's address */
} SceCrashDumpHeader;

_Static_assert(sizeof(SceCrashDumpHeader) == CRASHDUMP_HEADER_SIZE, "crash dump header");

static SceCrashDumpHeader *s_crashDumpHeader;
static void *s_crashDumpZero;
static volatile SceDmacChannelReg *s_crashDumpDmac;

static const SceCrashDumpMagic s_crashDumpMagic = { { 0x8b4a3174, 0x410049fe } };
static const SceCrashDumpIv s_crashDumpIv = { { 0x7347e60b, 0x0f46c440, 0x2dc0b9db, 0x64e93cb1 } };

/* the dump area: the top of DRAM, which is bigger on some SoC revisions */
static inline __attribute__((always_inline)) SceUInt32 CrashDumpBase(void)
{
	unsigned int revision = GetSoCRevision();

	if ((revision & 0x1ff00) != 0)
		return (revision & 0x30000000) != 0 ? 0x60000000 : 0x58000000;
	return (int)revision >= 0 ? 0x60000000 : 0x58000000;
}

static inline __attribute__((always_inline)) void CrashDumpDmacWait(volatile SceDmacChannelReg *dmac)
{
	dmac->intrStatus = dmac->intrStatus;
	dmac->ctrl = 1;
	while (dmac->stat & SCE_KERNEL_DMAC_STAT_BUSY)
		;
}

/* encrypt len bytes from src to dst, the IV in the shared memory's first line */
static inline __attribute__((always_inline)) void CrashDumpEncrypt(volatile SceDmacChannelReg *dmac,
	SceCrashDumpIv *iv, const uintptr_t *ivPaddr, SceUInt32 src, SceUInt32 dst, SceSize len)
{
	*iv = s_crashDumpIv;
	sceKernelDcacheCleanInvalidateRange(iv, 0x40);
	SceCpuForTZS_4FED4BCE();
	dmac->src = src;
	dmac->dst = dst;
	dmac->len = len;
	dmac->cmd = CRASHDUMP_DMAC_CMD_ENCRYPT;
	dmac->srcKeyringId = CRASHDUMP_DMAC_KEY_SLOT;
	dmac->iv = *ivPaddr;
	dmac->chainStart = 0;
	dmac->coherencyMask = 0xe070;
	dmac->ivCoherencyMask = 0x700070;
	CrashDumpDmacWait(dmac);
}

/* clear len bytes at dst */
static inline __attribute__((always_inline)) void CrashDumpClear(volatile SceDmacChannelReg *dmac,
	SceUInt32 dst, SceSize len)
{
	dmac->dst = dst;
	dmac->len = len;
	dmac->cmd = SCE_KERNEL_DMAC_CMD_OP_SET;
	dmac->chainStart = 0;
	dmac->coherencyMask = 0xe000;
	dmac->setValue = 0;
	CrashDumpDmacWait(dmac);
}

static inline __attribute__((always_inline, noreturn)) void CrashDumpPowerOff(void *shared,
	unsigned int mode)
{
	memset(shared, 0, 0x9000);
	memset(s_crashDumpZero, 0, 0x1000);
	sceSysconSetPowerMode(mode, 0);
	for (;;)
		;
}

/* 0x122 */
static __attribute__((noreturn)) void sceCrashDumpMonitorCall(int arg0, int arg1, int arg2, int arg3)
{
	volatile SceDmacChannelReg *dmac = s_crashDumpDmac;
	SceCrashDumpHeader *header;
	SceUInt32 *shared;
	SceUInt32 *page;
	SceUInt32 dramEnd, pageOffset, dst, base, va, pa, par;
	SceSize dramSize, offset;
	unsigned int count;
	unsigned int model;
	uintptr_t ivPaddr;

	shared = sceKernelSysrootGetSharedMemory();
	ParkOtherCpus(1);
	GrabStopMonitorCall(0xffe, 0);
	sceKernelDcacheCleanInvalidateAll();
	DmacGateEnable(5);
	if (dmac->stat & SCE_KERNEL_DMAC_STAT_BUSY) {
		dmac->ctrl = 0;
		while (dmac->stat & SCE_KERNEL_DMAC_STAT_BUSY)
			;
	}

	header = s_crashDumpHeader;
	memset(header, 0, 0x8000);
	header->magic = s_crashDumpMagic;
	header->version = 200;
	header->cpu = readMpidr() & 3;
	header = s_crashDumpHeader;
	header->args[0] = arg0;
	header->args[1] = arg1;
	header->args[2] = arg2;
	header->args[3] = arg3;
	SceSysrootForTZS_CC36643A(header->sessionId);
	SceSysrootForTZS_32546BDB(s_crashDumpHeader->coredumpKey);
	header = s_crashDumpHeader;
	header->dramRegionCount = 1;
	header->dramBase = CRASHDUMP_DRAM_BASE;
	model = SceSysrootForTZS_D6FBF445();
	switch (model) {
	case 0x10100:
	case 0x10150:
	case 0x10200:
	case 0x10220:
	case 0x11100:
		dramSize = 0x2400000;
		break;
	case 0x20100:
	case 0x20101:
	case 0x20110:
		dramSize = 0x2000000;
		break;
	default:
		SCE_DBG_PRINTF(11, 278, "Unknown model 0x%08x\n", model);
		header = s_crashDumpHeader;
		dramSize = 0x2400000;
		break;
	}
	dramEnd = CRASHDUMP_DRAM_BASE + dramSize;
	pageOffset = CRASHDUMP_HEADER_SIZE + dramSize;
	header->dramSize = dramSize;
	header->dramOffset = CRASHDUMP_HEADER_SIZE;
	header->reserved1[0] = 0;
	header->reserved1[1] = 0;
	memcpy(header->sharedMemory, shared + 0x400, sizeof(header->sharedMemory));
	if (sceKernelVAtoPA(shared, &ivPaddr) < 0)
		CrashDumpPowerOff(shared, 0);

	/* every other page the Non-secure kernel maps */
	page = s_crashDumpHeader->pages;
	memset(page, 0, sizeof(s_crashDumpHeader->pages));
	count = 0;
	dst = CRASHDUMP_STAGING_PADDR;
	for (va = 0; ; ) {
		writeAts12nsopr(va);
		par = readPar();
		if ((par & 1) == 0) {
			if ((par & 0x200) == 0)
				CrashDumpPowerOff(shared, 0x12);
			pa = par >> 12 << 12;
			if (pa - 0x40000000 < 0x40000000) {
				if (pa < CRASHDUMP_DRAM_BASE)
					CrashDumpPowerOff(shared, 0x12);
				if (pa >= dramEnd && pa - 0x70000000 >= 0x8000000) {
					*page++ = pa;
					count++;
					CrashDumpEncrypt(dmac, (SceCrashDumpIv *)shared, &ivPaddr, pa, dst, 0x1000);
					CrashDumpClear(dmac, pa, 0x1000);
					dst += 0x1000;
				}
			}
		}
		va += 0x1000;
		if (count >= CRASHDUMP_PAGE_MAX || va >= 0x40000000)
			break;
	}
	header = s_crashDumpHeader;
	header->pageCount = count;
	header->pageSize = count << 12;
	header->pageOffset = pageOffset;

	/* the DRAM below the dump area */
	base = CrashDumpBase();
	for (offset = 0; offset < dramSize; offset += 0x100000) {
		CrashDumpEncrypt(dmac, (SceCrashDumpIv *)shared, &ivPaddr, CRASHDUMP_DRAM_BASE + offset,
			base + CRASHDUMP_HEADER_SIZE + offset, 0x100000);
		CrashDumpClear(dmac, CRASHDUMP_DRAM_BASE + offset, 0x100000);
	}

	/* the pages, behind it */
	header = s_crashDumpHeader;
	dmac->src = CRASHDUMP_STAGING_PADDR;
	dmac->dst = base + header->pageOffset;
	dmac->len = header->pageSize;
	dmac->cmd = SCE_KERNEL_DMAC_CMD_OP_COPY;
	dmac->chainStart = 0;
	dmac->coherencyMask = 0xe070;
	CrashDumpDmacWait(dmac);
	CrashDumpClear(dmac, CRASHDUMP_STAGING_PADDR, header->pageSize);
	CrashDumpPowerOff(shared, 0x12);
}

int CrashDumpModuleStart(SceSize arglen, const void *argp)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SceUID uid;
	void *base;

	(void)arglen;
	(void)argp;
	if (sceKernelRegisterMonitorCallHandler(CRASHDUMP_MONCALL,
			(SceKernelMonitorCallHandler)sceCrashDumpMonitorCall) < 0)
		return 0;
	memset(&opt, 0, sizeof(opt));
	opt.size = 0x58;
	opt.paddr = DMAC5_REG_BASE;
	opt.attr = 2;
	uid = sceKernelAllocMemBlockForKernel("SceDmac5Reg", 0x20100806, DMAC_REG_SIZE, &opt);
	if (sceKernelGetMemBlockBase(uid, &base) < 0)
		return 0;
	s_crashDumpDmac = base;
	memset(&opt, 0, sizeof(opt));
	opt.size = 0x58;
	opt.attr = 2;
	opt.paddr = CrashDumpBase();
	uid = sceKernelAllocMemBlockForKernel("SceCrashDumpHeader", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_NC_RW,
		CRASHDUMP_HEADER_SIZE, &opt);
	if (sceKernelGetMemBlockBase(uid, &base) < 0)
		return 0;
	s_crashDumpHeader = base;
	memset(&opt, 0, sizeof(opt));
	opt.size = 0x58;
	opt.attr = 2;
	opt.paddr = CRASHDUMP_ZERO_PADDR;
	uid = sceKernelAllocMemBlockForKernel("SceCrashDumpZero", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_NC_RW,
		0x1000, &opt);
	if (sceKernelGetMemBlockBase(uid, &base) < 0)
		return 0;
	s_crashDumpZero = base;
	return 0;
}
