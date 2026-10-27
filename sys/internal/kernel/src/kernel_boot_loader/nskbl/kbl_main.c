/*
 * NSKBL: sceKblMain (kbl_main.c in Sony's debug records), which every CPU enters
 * from boot() in Non-secure state. As SKBL's, core 0 sets the sysroot up and each
 * core starts its part of the kernel linked into NSKBL (SceSysmem, then on its boot
 * stack the module manager, exception and interrupt managers, SceSblAuthMgr and
 * the secure module scheduler proxy); KblArchive (archive_ns.c) then loads the
 * kernel from os0:. sceSdStandaloneInit gives it os0: and sd0:, SceSfat volumes on
 * the eMMC's and the SD card's sectors; os0:'s go through a small sector cache.
 */

#include <stdint.h>
#include <string.h>
#include <fw.h>
#include <sceerror.h>
#include <kernel_boot_loader/kbl_param.h>
#include <kernel_boot_loader/kernel_boot_param.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/debugled.h>
#include <sysmem/dipsw.h>
#include <sysmem/sysroot.h>
#include <sysmem/uid_class.h>
#include <authmgr/sblauthmgr.h>
#include <smsc_proxy/smsc_proxy.h>
#include <sdif/sdif.h>
#include "../skbl/boot.h"
#include "../../sysmem/sysmem_internal.h"
#include "sfat.h"

/* the rest of the kernel */
void sceKernelModulemgrStart(SceKernelBootParam *param);
int sceKernelExcpmgrStart(void);
int sceKernelIntrMgrStart(void);

/* archive_ns.c, when linked */
int KblArchive(SceKernelBootParam *param) __attribute__((weak));

/* boot.c's copy of the KBL Param */
extern SceKblParam s_kblParam;

#define KBL_STACK_SIZE		0x4000
#define KBL_STACK_ALIGN		0x1000
#define KBL_STACK_GUARD		0x1000		/* the part DIPSW_STACK_FILL leaves */

#define KBL_INIT_TRIES		3

/* the storage a volume reads: the SceSdif host, and the device on it */
#define KBL_STORAGE_MMC		0
#define KBL_STORAGE_SD		1

/* name unknown */
typedef struct KblStorage {
	SceUInt32 host;
	void *device;
} KblStorage;

/* the sector cache os0: reads through: aligned runs of 32 sectors, least recent out */
#define KBL_SECTOR_SIZE		0x200
#define KBL_CACHE_SECTORS	32
#define KBL_CACHE_ENTRIES	32

/* name unknown */
typedef struct KblSectorCache {
	int stamp;			/* when last used; 0: free */
	SceUInt32 sector;		/* the first sector, or -1 */
	char data[KBL_CACHE_SECTORS * KBL_SECTOR_SIZE];
} KblSectorCache;

static KblStorage s_storage[2] = {
	{ KBL_STORAGE_MMC, NULL },
	{ KBL_STORAGE_SD, NULL },
};
static int s_cacheStamp = 1;
static KblSectorCache s_cache[KBL_CACHE_ENTRIES];

SfatVolume g_os0Volume;
SfatVolume g_sd0Volume;

static const char *const s_bootStackNames[SCE_KERNEL_BOOT_CPU_MAX] = {
	"SceKernelBootStackCore0",
	"SceKernelBootStackCore1",
	"SceKernelBootStackCore2",
	"SceKernelBootStackCore3",
};

#if FW_SINCE(0x04000000)
/* name unknown: the build number sceKblMain prints */
__attribute__((noinline))
int KblMainGetBuildNumber(void)
{
	return 284;
}
#else
/* the 3.6x boot loader prints boot()'s build number */
int KblGetBuildNumber(void);
#endif

/* name unknown: a volume's sectors from its device */
static int KblReadSectors(void *argument, SceUInt32 sector, SceUInt32 count, void *buffer)
{
	KblStorage *storage = argument;

	if (storage->host == KBL_STORAGE_MMC) {
		if (storage->device == NULL)
			return SCE_ERROR_ERRNO_ENODEV;
		return sceMmcReadSector(storage->device, sector, buffer, (SceUInt16)count);
	}
	if (storage->host != KBL_STORAGE_SD)
		return SCE_ERROR_ERRNO_ENXIO;
	if (storage->device == NULL)
		return SCE_ERROR_ERRNO_ENODEV;
	return SceSdifForDriver_B9593652(storage->device, sector, buffer, (SceUInt16)count);
}

/* name unknown: the same through the sector cache, for reads inside one run */
static int KblReadSectorsCached(void *argument, SceUInt32 sector, SceUInt32 count, void *buffer)
{
	SceUInt32 offset = sector & (KBL_CACHE_SECTORS - 1);
	SceUInt32 first = sector & ~(KBL_CACHE_SECTORS - 1);
	int stamp, oldest, oldestStamp, i, res;

	if (offset + count > KBL_CACHE_SECTORS)
		return KblReadSectors(argument, sector, count, buffer);
	stamp = ++s_cacheStamp;
	for (i = 0; i < KBL_CACHE_ENTRIES; i++) {
		if (s_cache[i].sector == first) {
			s_cache[i].stamp = stamp;
			memcpy(buffer, &s_cache[i].data[offset * KBL_SECTOR_SIZE],
			       count * KBL_SECTOR_SIZE);
			return 0;
		}
	}
	oldest = 0;
	oldestStamp = 0x7fffffff;
	for (i = 0; i < KBL_CACHE_ENTRIES; i++) {
		if (s_cache[i].stamp < oldestStamp) {
			oldest = i;
			oldestStamp = s_cache[i].stamp;
		}
	}
	res = KblReadSectors(argument, first, KBL_CACHE_SECTORS, s_cache[oldest].data);
	if (res < 0) {
		s_cache[oldest].stamp = 0;
		s_cache[oldest].sector = -1;
		return res;
	}
	s_cache[oldest].stamp = s_cacheStamp;
	s_cache[oldest].sector = first;
	memcpy(buffer, &s_cache[oldest].data[offset * KBL_SECTOR_SIZE], count * KBL_SECTOR_SIZE);
	return 0;
}

/* name unknown: the boot stack's part of sceKblMain */
static void KblMainContinue(SceKernelBootParam *param)
{
	sceKernelSetGPO(0xa8);
	sceKernelCpuId();
	if (sceKernelCpuId() == 0)
		sceKernelSysrootSetStatus(4);
	sceKernelModulemgrStart(param);
	sceKernelExcpmgrStart();
	sceKernelIntrMgrStart();
	sceSblAuthMgrStart();
	SceSblSmSchedProxyForKernel_A488D604();
	if (sceKernelCpuId() == 0)
		sceKernelSysrootSetStatus(16);
	sceKernelSetGPO(0xa9);
	if (KblArchive != NULL)
		KblArchive(param);
	for (;;)
		;
}

/* os0: from the eMMC, sd0: from the SD card the KBL Param reports */
void sceSdStandaloneInit(void)
{
	int tries, res, i;

	tries = KBL_INIT_TRIES;
	sceSdifInit();
	while (SceSdifForDriver_22C82E79(s_storage[KBL_STORAGE_MMC].host,
					 (SdifMmcContext **)&s_storage[KBL_STORAGE_MMC].device) != 0) {
		if (--tries == 0) {
			SCE_DBG_PRINTF(0, 308, "mmc(OS) init: Failed\n");
			s_storage[KBL_STORAGE_MMC].device = NULL;
			break;
		}
	}
	if (s_kblParam.boot_type_indicator_1 & BOOT_TYPE_SD_CARD) {
		tries = KBL_INIT_TRIES;
		while ((res = SceSdifForDriver_C1271539(s_storage[KBL_STORAGE_SD].host,
				(SdifContextPart **)&s_storage[KBL_STORAGE_SD].device)) != 0) {
			if (--tries == 0) {
				SCE_DBG_PRINTF(0, 322, "sd init: Failed. 0x%x\n", res);
				s_storage[KBL_STORAGE_SD].device = NULL;
				break;
			}
		}
	} else {
		s_storage[KBL_STORAGE_SD].device = NULL;
	}

	SfatInit();
	for (i = 0; i < KBL_CACHE_ENTRIES; i++) {
		s_cache[i].stamp = 0;
		s_cache[i].sector = -1;
	}
	s_cacheStamp = 1;
	res = SfatMount(&g_os0Volume, SFAT_MOUNT_SCE_MBR | SFAT_MOUNT_CACHE_FAT,
			KblReadSectorsCached, &s_storage[KBL_STORAGE_MMC]);
	if (res != 0)
		SCE_DBG_PRINTF(0, 348, "mmc(OS) mount: failed(0x%x)\n", res);
	if (s_storage[KBL_STORAGE_SD].device != NULL) {
		res = SfatMount(&g_sd0Volume, SFAT_MOUNT_CACHE_FAT, KblReadSectors,
				&s_storage[KBL_STORAGE_SD]);
		if (res != 0)
			SCE_DBG_PRINTF(0, 358, "SD mount: failed(0x%x)\n", res);
	}
}

/* name unknown: sceSdStandaloneInit undone (no call reaches it) */
int sceSdStandaloneTerm(void)
{
	SfatUnmount(&g_os0Volume, 0);
	SfatUnmount(&g_sd0Volume, 0);
	return SfatTerm();
}

/* name unknown: the secure modules stopped (no call reaches it) */
int KblMainStopSm(void)
{
	return sceSblAuthMgrStopSm();
}

static inline void KblCorelockSync(SceCorelockContext **corelock)
{
	SceCpuForKernel_2FF54320(*corelock, 0);
	SceCpuForKernel_FE2B77A3(*corelock);
}

void sceKblMain(SceKernelBootParam *param)
{
	SceKernelBootCpu *cpu;
	void *stack;
	int core;

	core = sceKernelCpuId();
	SceCpuForKernel_2FF54320(param->corelock, 0);
	if (core == 0) {
		param->l2PageTable000VBase = 0;
		param->pL2PageTable000 = (uint32_t *)param->l2PageTable000.vbase;
		param->pSysroot = (void *)param->sysroot.vbase;
		SceCpuForKernel_FE2B77A3(param->corelock);
		SetupCore0(param);
		sceKernelSysrootCorelockLock(0);
		sceKernelSysrootSetVbaseResetVector((void *)param->resetVector);
		sceKernelSysrootCorelockUnlock();
		sceKernelPrintfLevel(IsAllowKernelDebug() ? 1 : 2,
				      "Starting PSP2 Kernel [0x%08x]: %d\n", FW_VERSION,
#if FW_SINCE(0x04000000)
				      KblMainGetBuildNumber());
#else
				      KblGetBuildNumber());
#endif
		if (sceKernelSysrootIsSafeMode())
			sceKernelPrintfLevel(IsAllowKernelDebug() ? 0 : 2,
					      "\tSafe Mode : [ YES ]\n");
	} else {
		SceCpuForKernel_FE2B77A3(param->corelock);
		SetupCore0(param);
		sceKernelSysrootCorelockLock(0);
		sceKernelSysrootCorelockUnlock();
	}
	InitializeDebugCorelock();
	InitializeDebugPutcharPrivate();
	KblMMUInit(param);
	SysmemStart(param);
	AimgrInit();
	KblCorelockSync(&param->corelock);

	/* each core's boot stack */
	SceCpuForKernel_2FF54320(param->corelock, 0);
	cpu = &param->cpu[core];
	cpu->stackSize = KBL_STACK_SIZE;
	cpu->stackBlockId = sceKernelAllocPartitionStackMemBlock(SysmemAllocName(s_bootStackNames[core]),
								KBL_STACK_SIZE, KBL_STACK_ALIGN);
	sceKernelGetMemBlockVBase(cpu->stackBlockId, &stack);
	cpu->stackBottom = (char *)stack + cpu->stackSize;
	if (sceKernelCheckDipsw(DIPSW_STACK_FILL))
		memset((char *)stack + KBL_STACK_GUARD, 0xff, KBL_STACK_SIZE - KBL_STACK_GUARD);
	SceCpuForKernel_FE2B77A3(param->corelock);
	KblCorelockSync(&param->corelock);
	KblCallWithStack(param, KblMainContinue, cpu->stackBottom);
	SCE_DBG_ASSERT_FAIL(825);
	for (;;)
		;
}
