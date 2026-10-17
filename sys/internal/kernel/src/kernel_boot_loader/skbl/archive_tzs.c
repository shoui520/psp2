/*
 * SKBL: the TrustZone modules it carries (archive_tzs.c in Sony's debug
 * records). Core 0 unpacks the ARZL archive (the TrustZone SceSysmem) into
 * Compat SRAM and loads it; then the modules SKBL holds as plain ELFs
 * (SceExcpmgr, SceKernelIntrMgr, SceKernelBusError, SceSblSmsched,
 * SceDriverTzs). Core 0 loads and starts each module, and every core then
 * runs the module's boot entry for its own part.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "fw.h"
#include <psp2common/kernel/modulemgr.h>
#include <psp2kern/kernel/cpu.h>
#include <compat/compat_regs.h>
#include <sysmem/debug.h>
#include <kernel_boot_loader/kernel_boot_param.h>

/* the sysroot (sysroot.c) */
void sceKernelSysrootCorelockLock(SceUInt32 core);
void sceKernelSysrootCorelockUnlock(void);

/* the rest of the TrustZone kernel */
int SerialDebugPutchar(void *args, int c);
int ksceKernelPrintf(const char *format, ...);
uintptr_t ksceKernelGetDebugPutcharHandler(void);
uint32_t DebugGetPrintLevel(void);
SceUID sceKernelLoadModuleBufferWithoutStart(const void *buffer, SceSize size);
int sceKernelStartModule(SceUID uid, SceSize args, void *argp, int flags, void *option,
			 int *status);
int sceKernelGetModuleBootEntry(SceUID uid, int (**entry)(SceSize args, void *argp));
int sceKernelMMUMapSections(uint32_t *l1, uint32_t type, uint32_t domain, uintptr_t va,
			    SceSize size, uintptr_t pa);
int sceKernelMMUUnmapSectionsWithFlags(uint32_t *l1, uint32_t asid, uintptr_t va,
				       SceSize size, uint32_t flags);
SceSize sceArlzDecode(void *dst, SceSize dstSize, const void *src, int flags);
int sceArlzArmFilter(void *buffer, SceSize size, int flags);

/* the module images in SKBL's data, not packed yet: ARZL, then plain ELFs */
extern const char tzsSysmemArchive[];
extern const char tzsExcpmgrImage[], tzsIntrMgrImage[], tzsBusErrorImage[];
extern const char tzsSmschedImage[], tzsDriverTzsImage[], tzsImagesEnd[];

#define KBL_L1(_param)		((uint32_t *)(_param)->ttbr0.vbase)
#define ARCHIVE_MAP		0x1061d007	/* read/write/execute, global */
#define ARCHIVE_DOMAIN		0xc
#define ARCHIVE_DECODE_SIZE	0x1000000
#define ARZL_HEADER_SIZE	4		/* "ARZL" */
#define MODULE_BOOT_ARGS	4		/* argp is the boot param */

/* Sony's release builds, without Archive's message, inline it into KblArchive */
#if FW_DEBUG
#define ARCHIVE_INLINE
#else
#define ARCHIVE_INLINE		inline
#endif

/* name unknown: one module image in SKBL, from its first byte to its end */
typedef struct KblModuleImage {
	const char *start;
	const char *end;
} KblModuleImage;

/* the archive once unpacked, and the end of the list */
static KblModuleImage s_archive[2];
static int s_moduleStatus;
static SceUID s_moduleId;

static const KblModuleImage s_modules[] = {
	{ tzsExcpmgrImage, tzsIntrMgrImage },
	{ tzsIntrMgrImage, tzsBusErrorImage },
	{ tzsBusErrorImage, tzsSmschedImage },
	{ tzsSmschedImage, tzsDriverTzsImage },
	{ tzsDriverTzsImage, tzsImagesEnd },
	{ NULL, NULL },
};

static int ModuleLoadBoot(const KblModuleImage *image, SceKernelBootParam *param)
{
	int (*entry)(SceSize args, void *argp);
	SceUID uid;
	int res;

	sceKernelSysrootCorelockLock(0);
	if (ksceKernelCpuId() == 0) {
		uid = sceKernelLoadModuleBufferWithoutStart(image->start,
							    image->end - image->start);
		if (uid < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 117,
					     "sceKernelLoadModuleBufferWithoutStart:uid=0x%08x\n",
					     uid);
			return uid;
		}
		s_moduleId = uid;
		res = sceKernelStartModule(uid, MODULE_BOOT_ARGS, param, 0, NULL,
					   &s_moduleStatus);
		if (res < 0) {
			/* Sony prints the module's UID here */
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 125, "sceKernelStartModule:res=0x%08x\n",
					     uid);
			return res;
		}
	}
	sceKernelSysrootCorelockUnlock();

	if (s_moduleStatus != SCE_KERNEL_START_NO_RESIDENT) {
		entry = NULL;
		res = sceKernelGetModuleBootEntry(s_moduleId, &entry);
		if (res < 0)
			return res;
		if (entry != NULL)
			entry(MODULE_BOOT_ARGS, param);
	}
	return 0;
}

static ARCHIVE_INLINE int Archive(const KblModuleImage *image, SceKernelBootParam *param)
{
	int res;

	for (; image->start != NULL; image++) {
		res = ModuleLoadBoot(image, param);
		if (res < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 171, "res=0x%08x\n", res);
			return res;
		}
	}
	return 0;
}

int KblArchive(SceKernelBootParam *param)
{
	SceSize size;
	int res;

	param->debugPutchar = (uintptr_t)SerialDebugPutchar;
	param->debugPrintf = (uintptr_t)ksceKernelPrintf;
	param->debugPutcharHandler = ksceKernelGetDebugPutcharHandler();
	param->debugPrintLevel = DebugGetPrintLevel();

	sceKernelSysrootCorelockLock(0);
	if (ksceKernelCpuId() == 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 205,
				     "mapping Compati SRAM : 0x%08x to 0x%08x (size 0x%x)\n",
				     COMPAT_SRAM_BASE, COMPAT_SRAM_BASE, COMPAT_SRAM_SIZE);
		sceKernelMMUMapSections(KBL_L1(param), ARCHIVE_MAP, ARCHIVE_DOMAIN,
					COMPAT_SRAM_BASE, COMPAT_SRAM_SIZE, COMPAT_SRAM_BASE);
		size = sceArlzDecode((void *)COMPAT_SRAM_BASE, ARCHIVE_DECODE_SIZE,
				     tzsSysmemArchive + ARZL_HEADER_SIZE, 0);
		SCE_DBG_PRINTF_LEVEL(2, 0, 221,
				     "sceArlzDecode(0x%08x, 0x%08x, %p):res=%d[0x%08x]\n",
				     COMPAT_SRAM_BASE, ARCHIVE_DECODE_SIZE, tzsSysmemArchive,
				     size, size);
		res = sceArlzArmFilter((void *)COMPAT_SRAM_BASE, size, 0);
		SCE_DBG_PRINTF_LEVEL(2, 0, 227,
				     "sceArlzArmFilter(0x%08x,0x%08x):res=%d[0x%08x]\n",
				     COMPAT_SRAM_BASE, size, res, res);
		s_archive[0].start = (const char *)COMPAT_SRAM_BASE;
		s_archive[0].end = (const char *)COMPAT_SRAM_BASE + size;
	}
	sceKernelSysrootCorelockUnlock();
	Archive(s_archive, param);

	/* the archive is loaded: wipe Compat SRAM and give it back */
	sceKernelSysrootCorelockLock(1);
	if (ksceKernelCpuId() == 0) {
		memset((void *)COMPAT_SRAM_BASE, 0, COMPAT_SRAM_SIZE);
		ksceKernelDcacheCleanInvalidateRange((void *)COMPAT_SRAM_BASE,
						     COMPAT_SRAM_SIZE);
		sceKernelMMUUnmapSectionsWithFlags(KBL_L1(param), ksceKernelCpuGetCONTEXTIDR(),
						   COMPAT_SRAM_BASE, COMPAT_SRAM_SIZE, 0);
	}
	sceKernelSysrootCorelockUnlock();
	Archive(s_modules, param);
	return 0;
}
