/* SceKernelBlueScreenOfDeath startup and public compatibility entry. */
#include <compat/compat_regs.h>
#include <sysmem/cp15.h>
#include <excpmgr/excpmgr_cpu.h>
#include <sysmem/sysroot.h>

#include "bsod_internal.h"
#include "exception_record.h"
#include "report_data.h"
#include <intrmgr/intrmgr.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>

BsodDisplay s_bsodDisplay;
int s_bsodQafEnabled;

#if FW_DEBUG
static const SceKernelDebugInfo s_initAssert = {
	0x42b85a92, 0xdfcfee9f, 0x22ce179a,
	"bsod.c", 200, "_sceKernelBsodInit",
};
#endif

int module_start(unsigned arglen, void *argp)
{
	SceKernelAllocMemBlockKernelOpt options;
	void *base;
	unsigned state;
	int uid, qaf;

	(void)arglen;
	(void)argp;
	qaf = SceQafMgrForDriver_382C71E8();
	s_bsodQafEnabled = qaf;
	if ((SceSblAIMgrForDriver_D78B04A2() && !qaf) ||
	    sceKernelCheckDipsw(DIPSW_SYSTEM_DECI) || sceKernelCheckDipsw(DIPSW_DECI))
		return 1;
	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.attr = 2;
	options.paddr = COMPAT_SRAM_BASE;
	uid = sceKernelAllocMemBlockForKernel("SceKernelBsodSram", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_NC_RW,
				    COMPAT_SRAM_SIZE, &options);
	if (uid < 0) {
#if FW_DEBUG
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0, &s_initAssert,
					     __builtin_return_address(0));
#endif
		return 1;
	}
	sceKernelGetMemBlockBase(uid, &base);
	s_bsodDisplay.pixels = base;
	s_bsodDisplay.current.physical = COMPAT_SRAM_BASE;
	s_bsodDisplay.current.pitch = 960;
	s_bsodDisplay.current.x = 0;
	s_bsodDisplay.current.width = 960;
	s_bsodDisplay.current.height = 544;
	s_bsodDisplay.current.scaleX = 0x10000;
	s_bsodDisplay.current.scaleY = 0x10000;
	_sceKernelBsodSysconInit();
	_sceKernelBsodIftuInit(SceSblAIMgrForDriver_71608CA3() != 0);
	state = SceCpuForDriver_821FC0EE();
	sceKernelRegisterExceptionHandler(3, 2, &bsodPrefetchEntry);
	sceKernelRegisterExceptionHandler(4, 2, &bsodDataEntry);
	sceKernelRegisterExceptionHandler(1, 2, &bsodUndefinedEntry);
	/* bsodDebugCrashCallback ignores the fault */
	SceDebugForKernel_67E9D1EC((DebugFaultHandler)bsodDebugCrashCallback);
	bsodInitializePutchar();
	SceDebugForKernel_A465A31A(0);
	sceKernelCpuResumeIntr(state);
	_sceKernelBsodDummyFsInit();
	return 0;
}

/* Fatal screen loop: real display changes, input edges and reset/power SMCs. */
extern void (*s_bsodPrinter)(const char *format, ...);

uint32_t s_bsodButtons;

static void bsodReboot(unsigned dump)
{
	void *sysroot = SceSysrootForKernel_3E455842();

	sceCrashDumpMonitorCall(dump, readTtbr0(), readVbar(), (uintptr_t)sysroot);
}

void bsodCrashHandler(int kind)
{
	uint32_t buttons, edge;
	unsigned page = kind ? 0 : 3;

	sceKernelSpinlockLowLock(&s_bsodDisplay.lock);
	sceKernelDisableIntr(0xd5);
	sceKernelDisableIntr(0xd2);
	sceKernelDisableIntr(0xf8);
	bsodFreezeLog();
	s_bsodPrinter = bsodPrintf;
	bsodReadFramebuffer(0, &s_bsodDisplay.previous);
	if (!s_bsodDisplay.previous.physical)
		bsodReadFramebuffer(1, &s_bsodDisplay.previous);
	bsodWriteFramebuffer(1, &s_bsodDisplay.current);
	bsodWriteFramebuffer(0, &s_bsodDisplay.previous);
	bsodSetFont(6, 16);
	bsodSetFramebuffer(s_bsodDisplay.pixels, 960, 544, 3);
	s_bsodButtons = bsodReadButtons();
	for (;;) {
		bsodWriteFramebuffer(1, &s_bsodDisplay.current);
		bsodWriteFramebuffer(0, &s_bsodDisplay.previous);
		buttons = bsodReadButtons();
		edge = buttons & ~s_bsodButtons;
		s_bsodButtons = buttons;
		if (edge & 0x400) {
			page = page == 3 ? 0 : page + 1;
		} else if (edge & 0x200) {
			page = page ? page - 1 : 3;
		} else if (edge & 0x100) {
			bsodResetColors();
			bsodSetBackground(0xc0400000);
			for (;;) {
				bsodClearGrid();
				bsodPrintf(s_bsodTextMenuSpacing);
				bsodPrintf(s_bsodTextMenuCross);
				bsodPrintf(s_bsodTextMenuSelect);
				bsodPrintf(s_bsodTextMenuStart);
				bsodPrintf(s_bsodTextMenuNewline);
				bsodPrintf(s_bsodTextMenuStandby);
				bsodRasterize();
				buttons = bsodReadButtons();
				edge = buttons & ~s_bsodButtons;
				s_bsodButtons = buttons;
				if (edge & 0x160)
					break;
				if (edge & 0x800) {
					bsodReboot(s_bsodException.excpCode == 0);
					for (;;) {}
				}
				if (edge & 0x4000) {
					bsodPowerOff();
					for (;;) {}
				}
			}
			bsodClearGrid();
			continue;
		}
		bsodResetColors();
		bsodSetBackground(buttons & 0x1000 ? 0 : 0xc0400000);
		bsodClearGrid();
		switch (page) {
		case 0: bsodReportRegisters(&s_bsodException); break;
		case 1: bsodReportKernelStack(&s_bsodException); break;
		case 2: bsodReportUserStack(&s_bsodException); break;
		case 3: bsodPrintLog(&s_bsodException, edge); break;
		}
		bsodRasterize();
	}
}

void bsodDebugCrashCallback(void)
{
	bsodCrashHandler(1);
}

/* Capture a saved exception context and the live diagnostic CPU registers. */

void bsodCaptureException(const SceExcpmgrContext *context, unsigned exception,
			  const SceKernelDebugInfo *info)
{
	BsodCrashRecord *record = &s_bsodException;
	SceSysrootSharedMemory *shared;
	uint32_t jmcr;
	uint64_t time;
	unsigned core;
	int i;
	char *report;
	const unsigned int *pmuType = &context->pmuCounter[0].type;
	const unsigned int *vfp = (const unsigned int *)context->vfp;

	sceKernelSpinlockLowLock(&s_bsodDisplay.captureLock);
	record->excpCode = context->excpCode;
	record->r[0] = context->r[0];
	record->r[1] = context->r[1];
	record->r[2] = context->r[2];
	record->r[3] = context->r[3];
	record->r[4] = context->r[4];
	record->r[5] = context->r[5];
	record->r[6] = context->r[6];
	record->r[7] = context->r[7];
	record->r[8] = context->r[8];
	record->r[9] = context->r[9];
	record->r[10] = context->r[10];
	record->r[11] = context->r[11];
	record->r[12] = context->r[12];
	record->sp = context->sp;
	record->lr = context->lr;
	record->pc = context->pc;
	/* the saved d registers are copied word by word */
	for (i = 0; i < 64; i += 2) {
		record->vfp[i / 2][0] = vfp[i];
		record->vfp[i / 2][1] = vfp[i + 1];
	}
	record->cpsr = context->cpsr;
	record->midr = readMidr();
	record->ctr = readCtr();
	record->tcmtr = readTcmtr();
	record->tlbtr = readTlbtr();
	record->mpidr = readMpidr();
	record->pfr[0] = readIdPfr0();
	record->pfr[1] = readIdPfr1();
	record->dfr0 = readIdDfr0();
	record->mmfr[0] = readIdMmfr0();
	record->mmfr[1] = readIdMmfr1();
	record->mmfr[2] = readIdMmfr2();
	record->mmfr[3] = readIdMmfr3();
	record->isar[0] = readIdIsar0();
	record->isar[1] = readIdIsar1();
	record->isar[2] = readIdIsar2();
	record->isar[3] = readIdIsar3();
	record->isar[4] = readIdIsar4();
	record->ccsidr = readCcsidr();
	record->clidr = readClidr();
	record->aidr = readAidr();
	record->csselr = readCsselr();
	record->sctlr = readSctlr();
	record->actlr = readActlr();
	record->cpacr = context->cpacr;
	record->ttbr0 = readTtbr0();
	record->ttbr1 = context->ttbr1;
	record->ttbcr = readTtbcr();
	record->dacr = context->dacr;
	record->dfsr = readDfsr();
	record->ifsr = readIfsr();
	record->adfsr = readAdfsr();
	record->aifsr = readAifsr();
	record->dfar = readDfar();
	record->ifar = readIfar();
	record->par = context->par;
	record->pmcr = readPmcr();
	record->pmcntenset = context->pmcntenset;
	record->pmovsr = readPmovsr();
	record->pmselr = context->pmselr;
	record->pmccntr = context->pmccntr;
	/* Sony repeats the first saved event type for every output counter. */
	record->pmuType[0] = *pmuType;
	record->pmuType[1] = *pmuType;
	record->pmuType[2] = *pmuType;
	record->pmuType[3] = *pmuType;
	record->pmuType[4] = *pmuType;
	record->pmuType[5] = *pmuType;
	record->pmuCount[0] = context->pmuCounter[0].count;
	record->pmuCount[1] = context->pmuCounter[1].count;
	record->pmuCount[2] = context->pmuCounter[2].count;
	record->pmuCount[3] = context->pmuCounter[3].count;
	record->pmuCount[4] = context->pmuCounter[4].count;
	record->pmuCount[5] = context->pmuCounter[5].count;
	record->pmuserenr = context->pmuserenr;
	record->prrr = readPrrr();
	record->nmrr = readNmrr();
	if (readNsacr() & NSACR_PLE) {
		record->pleuar = readPleuar();
		record->plepcr = readPlepcr();
	} else {
		record->pleuar = UINT32_MAX;
		record->plepcr = UINT32_MAX;
	}
	record->vbar = readVbar();
	record->fcseidr = readFcseidr();
	record->contextId = context->contextId;
	record->tpidrurw = readTpidrurw();
	record->tpidruro = context->tpidruro;
	record->thread = context->thread;
	record->powerControl = readPowerControl();
	record->fpsid = readFpsid();
	record->fpscr = context->fpscr;
	record->mvfr1 = readMvfr1();
	record->mvfr0 = readMvfr0();
	record->fpexc = context->fpexc;
	record->teecr = readTeecr();
	record->teehbr = readTeehbr();
	jmcr = readJmcr();
	writeJmcr(jmcr | 1);
	record->jidr = readJidr();
	record->joscr = readJoscr();
	record->jmcr = jmcr;
	record->jaolr = readJaolr();
	writeJmcr(jmcr);
	record->dbgdscr = readDbgdscr();
	SceSysrootForKernel_84783B71(record->sessionId);
	sceKernelSysrootBacktrace(-16, record->kernelFrames,
				   sizeof(record->kernelFrames), &record->kernelFrameCount, 3);
	if (record->thread)
		sceKernelSysrootBacktrace(-16, record->userFrames,
					   sizeof(record->userFrames), &record->userFrameCount, 10);
	time = SceSysrootForKernel_E20F6FC8();
	core = sceKernelCpuId();
	shared = sysrootSharedMemory();
	SceDebugForDriver_62466B0A(shared->tty, sizeof(shared->tty));
	report = shared->report[core];
	memset(report, 0, sizeof(shared->report[core]));
	SceDebugForKernel_5D6F0CFC((DebugExceptionDumpHeader *)report, core, time, info,
				   context, exception);
	bsodCrashHandler(0);
}

/* Native exception wrappers ignore user-mode exceptions and capture kernel faults. */
#if FW_DEBUG
#define BSOD_INFO(lineHash, funcHash, line, func) \
	{ 0x42b85a92, lineHash, funcHash, "bsod.c", line, func }
#else
#define BSOD_INFO(lineHash, funcHash, line, func) \
	{ 0x42b85a92, lineHash, funcHash, NULL, 0, NULL }
#endif
BsodCrashRecord s_bsodException;

void _sceKernelBsodPrefetchAbort(const SceExcpmgrContext *context)
{
	static const SceKernelDebugInfo info =
		BSOD_INFO(0xb23d7445, 0x8c331b02, 276, "_sceKernelBsodPrefetchAbort");

	if ((context->cpsr & 0x1f) != 0x10)
		bsodCaptureException(context, 4, &info);
}

void _sceKernelBsodDataAbort(const SceExcpmgrContext *context)
{
	static const SceKernelDebugInfo info =
		BSOD_INFO(0xbaf029ec, 0xc4f68af3, 290, "_sceKernelBsodDataAbort");

	if ((context->cpsr & 0x1f) != 0x10)
		bsodCaptureException(context, 1, &info);
}

void _sceKernelBsodUndfinedAbort(const SceExcpmgrContext *context)
{
	static const SceKernelDebugInfo info =
		BSOD_INFO(0x6f08a56d, 0x53bf04dd, 304, "_sceKernelBsodUndfinedAbort");

	if ((context->cpsr & 0x1f) != 0x10)
		bsodCaptureException(context, 6, &info);
}

int SceDeci4pUserp_C53CFD12(void)
{
	return 0;
}
