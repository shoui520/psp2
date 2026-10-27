/*
 * SKBL: sceKblMain (kbl_main.c in Sony's debug records), which every CPU
 * enters from boot() with the MMU on. Core 0 builds the sysroot; then each
 * core starts its part of the TrustZone kernel linked into SKBL (SceSysmem,
 * its module manager, exception and interrupt managers), switches to its
 * boot stack and goes on in KblMainContinue: the cores line up in Power
 * Scratchpad code, core 0 unpacks NSKBL, or on a resume puts the resume
 * image in the scratchpad, and every core leaves for Non-secure state.
 */

#include <stdint.h>
#include <string.h>
#include <fw.h>
#include <kernel_boot_loader/kbl_param.h>
#include <sysmem/cpu.h>
#include <sysmem/barrier.h>
#include <sysmem/cp15.h>
#include <sysmem/debug.h>
#include <kernel_boot_loader/kernel_boot_param.h>
#include <power/spad_regs.h>
#include <sysmem/sysroot.h>
#include "boot.h"
#include "../../sysmem/sysmem_internal.h"
#include <sysmem/dipsw.h>
#include <sysmem/debugled.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_class.h>

/* boot.c and monitor.S */
void jumpToNonSecure(uintptr_t entry) __attribute__((noreturn));

/* the sysroot (sysroot.c) */
void sceKernelSysrootSetVbaseMonitorVector(uint32_t value);
void sceKernelSysrootSetBootStatus(unsigned int status);

/* the rest of the TrustZone kernel */
int KblMainGetBuildNumber(void);
int InitializeDebugCorelock(void);
int InitializeDebugPutcharPrivate(void);
int KblMMUInit(SceKernelBootParam *param);
int SysmemStart(SceKernelBootParam *param);
void KblCallWithStack(SceKernelBootParam *param, void (*func)(SceKernelBootParam *),
		      void *stack);
int sceKernelModulemgrStart(SceKernelBootParam *param);
int ExcpmgrModuleStart(void);
int sceKernelIntrMgrModuleStart(SceKernelBootParam *param);
int initDmacSecure(void);
int sceKernelMMUMapSection(uint32_t *l1, uint32_t type, uint32_t domain, uintptr_t va,
			   uintptr_t pa);
int sceKernelMMUUnmapSectionWithFlags(uint32_t *l1, uint32_t asid, uintptr_t va,
				      uint32_t flags);
SceSize sceArlzDecode(void *dst, SceSize dstSize, const void *src, int flags);
int sceArlzArmFilter(void *buffer, SceSize size, int flags);

/* compat_sram.S: the code put in the Power Scratchpad */
extern const char compatSramImage[], compatSramResume[], compatSramResumeEnd[];

#define KBL_STACK_SIZE		0x4000
#define KBL_STACK_ALIGN		0x1000
#define KBL_STACK_GUARD		0x1000		/* the part DIPSW_STACK_FILL leaves */

#define NSKBL_ADDRESS		0x51000000	/* where NSKBL runs */
#define NSKBL_SIZE		0x1000000
#define NSKBL_ARCHIVE		0x50000004	/* kernel_boot_loader.self segment 3, ARZL */

/* the window the secure world maps the scratchpad at while it fills it */
#define SPAD_WINDOW		0x10000000
#define SPAD_WINDOW_EXEC	0x10100000	/* the same pages, executable */
#define SPAD_WINDOW_SIZE	0x8000
#define SPAD_MAP_DATA		0x18006
#define SPAD_MAP_CODE		0x18005
#define SPAD_DOMAIN		0xc
#define SPAD_UNMAP_ASID		0xf0
#define SPAD_CORE_SYNC		((void *)(SPAD_WINDOW + 0x100))	/* compatSramCoreSync's words */
#define SPAD_RESUME_CONTEXT	((void *)(SPAD_WINDOW + 0x1000))
#define SPAD_RESUME_INFO	((KblResumeInfo *)(SPAD_WINDOW + 0x1800))

/* suspendinfo_adr: 8-byte aligned in the kernel's DRAM */
#define SUSPEND_INFO_FIRST	0x40200000
#define SUSPEND_INFO_RANGE	0x1fe00000
#define RESUME_CONTEXT_SIZE	0x68
/* the sizes of the two images: their bounds since 4.00, constants before */
#if FW_SINCE(0x04000000)
#define CORE_SYNC_IMAGE_SIZE	(compatSramResume - compatSramImage)
#define RESUME_IMAGE_SIZE	(compatSramResumeEnd - compatSramResume)
#else
#define CORE_SYNC_IMAGE_SIZE	0xe0
#define RESUME_IMAGE_SIZE	0x408
#endif

/* name unknown: what the resume image gets from the KBL Param */
typedef struct KblResumeInfo {
	SceSize size;
	uint32_t wakeupFactor;
	uint32_t reserved0;
	uint32_t sleepFactor;
	uint32_t reserved1;
	uint32_t bootControlsInfo;
	uint32_t usbInfo;
	uint32_t reserved2[9];
} KblResumeInfo;

_Static_assert(sizeof(KblResumeInfo) == 0x40, "resume info");

static const char *const s_bootStackNames[SCE_KERNEL_BOOT_CPU_MAX] = {
	"SceKernelBootStackCore0",
	"SceKernelBootStackCore1",
	"SceKernelBootStackCore2",
	"SceKernelBootStackCore3",
};

#if FW_DEBUG
/* the hand-off's messages: core 0 prints them, when the QA flags allow it */
#define KBL_PRINTF(...) do {						\
	if (sceKernelCpuId() == 0 && IsAllowKernelDebug())		\
		SceDebugForDriver_391B74B7(__VA_ARGS__);				\
} while (0)
#else
#define KBL_PRINTF(...)	do { } while (0)
#endif

/* name unknown: waits until every core has reached this point */
void KblCorelockSync(SceCorelockContext **corelock)
{
	SceCpuForKernel_2FF54320(*corelock, 0);
	SceCpuForKernel_FE2B77A3(*corelock);
}

/* name unknown: the boot stack's part of sceKblMain */
static void KblMainContinue(SceKernelBootParam *param)
{
	KblResumeInfo info;
	uint64_t context[RESUME_CONTEXT_SIZE / 8];	/* the CP15 state saved at suspend */
	SceKblParam *kblParam;
	uintptr_t suspendInfo, offset;

	sceKernelSetGPO(0x88);
	sceKernelCpuId();
	if (sceKernelCpuId() == 0)
		sceKernelSysrootSetStatus(4);
	sceKernelModulemgrStart(param);
	ExcpmgrModuleStart();
	sceKernelIntrMgrModuleStart(param);
	initDmacSecure();
	KblArchive(param);

	SceCpuForKernel_2FF54320(param->corelock, 0);
	SceCpuForKernel_4BBA5C82();
	SceCpuForKernel_FE2B77A3(param->corelock);

	/* line the cores up in the scratchpad */
	SceCpuForKernel_2FF54320(param->corelock, 0);
	if (sceKernelCpuId() == 0) {
		sceKernelMMUMapSection(KBL_L1(param), SPAD_MAP_DATA, SPAD_DOMAIN, SPAD_WINDOW, SPAD32K_BASE);
		sceKernelMMUMapSection(KBL_L1(param), SPAD_MAP_CODE, SPAD_DOMAIN, SPAD_WINDOW_EXEC,
				       SPAD32K_BASE);
		memset((void *)SPAD_WINDOW, 0, SPAD_WINDOW_SIZE);
		memcpy((void *)SPAD_WINDOW, compatSramImage, CORE_SYNC_IMAGE_SIZE);
		(void)*(volatile uint32_t *)SPAD_WINDOW;
		dsb();
		SceCpuForKernel_4BBA5C82();
		sceKernelL1IcacheInvalidateEntireAllCore();
		CpuL2CleanInvalidateAll();
	}
	SceCpuForKernel_FE2B77A3(param->corelock);
	((void (*)(void *))(SPAD_WINDOW_EXEC + 1))(SPAD_CORE_SYNC);
	KblCorelockSync(&param->corelock);

	SceCpuForKernel_2FF54320(param->corelock, 0);
	if (sceKernelCpuId() == 0) {
		memset((void *)SPAD_WINDOW, 0, SPAD_WINDOW_SIZE);
		(void)*(volatile uint32_t *)SPAD_WINDOW;
		dsb();
		sceKernelMMUUnmapSectionWithFlags(KBL_L1(param), SPAD_UNMAP_ASID, SPAD_WINDOW, 0);
		sceKernelMMUUnmapSectionWithFlags(KBL_L1(param), SPAD_UNMAP_ASID, SPAD_WINDOW_EXEC, 0);
	}
	SceCpuForKernel_FE2B77A3(param->corelock);

	if (!sceKernelIsColdBoot()) {
		/* resume: the scratchpad gets the resume image and the context it restores */
		if (sceKernelCpuId() == 0) {
			memset(&info, 0, sizeof(info));
			kblParam = param->kblParam;
			info.size = sizeof(info);
			suspendInfo = kblParam->suspendinfo_adr;
			info.wakeupFactor = kblParam->wakeup_factor;
			info.sleepFactor = kblParam->poweroff_factor;
			info.usbInfo = kblParam->usb_info;
			info.bootControlsInfo = kblParam->hold_ctrl;
			if (suspendInfo - SUSPEND_INFO_FIRST >= SUSPEND_INFO_RANGE
			    || (suspendInfo & 7) != 0) {
				KBL_PRINTF("suspendinfo_adr is invalid: 0x%08X\n", suspendInfo);
				for (;;)
					;
			}
			offset = suspendInfo & 0xfffff;
			sceKernelMMUMapSection(KBL_L1(param), SPAD_MAP_DATA, SPAD_DOMAIN, SPAD_WINDOW,
					       suspendInfo - offset);
			memcpy(context, (void *)(SPAD_WINDOW + offset), sizeof(context));
			sceKernelMMUUnmapSectionWithFlags(KBL_L1(param), SPAD_UNMAP_ASID, SPAD_WINDOW,
							  suspendInfo & 7);
			sceKernelMMUMapSection(KBL_L1(param), SPAD_MAP_DATA, SPAD_DOMAIN, SPAD_WINDOW,
					       SPAD32K_BASE);
			memset((void *)SPAD_WINDOW, suspendInfo & 7, SPAD_WINDOW_SIZE);
			memcpy((void *)SPAD_WINDOW, compatSramResume, RESUME_IMAGE_SIZE);
			memcpy(SPAD_RESUME_CONTEXT, context, sizeof(context));
			*SPAD_RESUME_INFO = info;
			sceKernelMMUUnmapSectionWithFlags(KBL_L1(param), SPAD_UNMAP_ASID, SPAD_WINDOW,
							  suspendInfo & 7);
			SceCpuForKernel_49180814();
			sceKernelL1IcacheInvalidateEntireAllCore();
			CpuL2CleanInvalidateAll();
		}
		KBL_PRINTF(" Jump to Non-secure power.kprx resume!!!\n");
		KBL_PRINTF("==========================================================\n\n\n");
		KblCorelockSync(&param->corelock);
		sceKernelSetGPO(0x8a);
		jumpToNonSecure(SPAD32K_BASE);
	}

	KBL_PRINTF("\n\n\n");
	KBL_PRINTF("========= Jump to Non-secure KBL from Secure KBL =========\n");
	if (sceKernelCpuId() == 0) {
		sceArlzArmFilter((void *)NSKBL_ADDRESS,
				 sceArlzDecode((void *)NSKBL_ADDRESS, NSKBL_SIZE,
					       (const void *)NSKBL_ARCHIVE, 0), 0);
		SceCpuForKernel_49180814();
		sceKernelL1IcacheInvalidateEntireAllCore();
		CpuL2CleanInvalidateAll();
	}
	KBL_PRINTF(" Jump to Non-secure KBL!!!\n");
	KBL_PRINTF("==========================================================\n\n\n");
	KblCorelockSync(&param->corelock);
	sceKernelSetGPO(0x89);
	jumpToNonSecure(NSKBL_ADDRESS);
}

int sceKblMain(SceKernelBootParam *param)
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
		sceKernelSysrootSetVbaseMonitorVector(readMvbar());
		sceKernelSysrootSetBootStatus(bootSaveArea.state);
		sceKernelSysrootCorelockUnlock();
#if FW_DEBUG
		sceKernelPrintfLevel(IsAllowKernelDebug() ? 1 : 2,
				      "Starting PSP2 Kernel [0x%08x]: %d\n", FW_VERSION,
				      KblMainGetBuildNumber());
		if (sceKernelIsColdBoot())
			sceKernelPrintfLevel(IsAllowKernelDebug() ? 1 : 2,
					      "Time taken to load KBL: %d us\n",
					      param->kblParam->kbl_load_time);
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
