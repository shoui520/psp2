/* SceKernelBlueScreenOfDeath startup and public compatibility entry. */
#include <compat/compat_regs.h>
#include <sysmem/cp15.h>
#include <excpmgr/excpmgr_cpu.h>
#include <excpmgr/moncall.h>
#include <sysmem/sysroot.h>

#include "bsod_internal.h"
#include "exception_record.h"
#include "report_data.h"

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
	int uid;

	(void)arglen;
	(void)argp;
	s_bsodQafEnabled = SceQafMgrForDriver_382C71E8();
	if ((ksceSblAimgrIsCEX() && !s_bsodQafEnabled) ||
	    ksceKernelCheckDipsw(0xc1) || ksceKernelCheckDipsw(0xc2))
		return 1;
	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.attr = 2;
	options.paddr = COMPAT_SRAM_BASE;
	uid = ksceKernelAllocMemBlock("SceKernelBsodSram", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_NC_RW,
				    COMPAT_SRAM_SIZE, &options);
	if (uid < 0) {
#if FW_DEBUG
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0, &s_initAssert,
					     __builtin_return_address(0));
#endif
		return 1;
	}
	ksceKernelGetMemBlockBase(uid, &base);
	s_bsodDisplay.pixels = base;
	s_bsodDisplay.current.physical = COMPAT_SRAM_BASE;
	s_bsodDisplay.current.pitch = 960;
	s_bsodDisplay.current.x = 0;
	s_bsodDisplay.current.width = 960;
	s_bsodDisplay.current.height = 544;
	s_bsodDisplay.current.scaleX = 0x10000;
	s_bsodDisplay.current.scaleY = 0x10000;
	_sceKernelBsodSysconInit();
	_sceKernelBsodIftuInit(ksceSblAimgrIsDolce() != 0);
	state = ksceKernelCpuSuspendIntr();
	ksceExcpmgrRegisterHandler(3, 2, bsodPrefetchEntry);
	ksceExcpmgrRegisterHandler(4, 2, bsodDataEntry);
	ksceExcpmgrRegisterHandler(1, 2, bsodUndefinedEntry);
	SceDebugForKernel_67E9D1EC(bsodDebugCrashCallback);
	bsodInitializePutchar();
	ksceKernelEnableCrashDump(0);
	ksceKernelCpuResumeIntr(state);
	_sceKernelBsodDummyFsInit();
	return 0;
}

/* Fatal screen loop: real display changes, input edges and reset/power SMCs. */
extern void (*s_bsodPrinter)(const char *format, ...);
int ksceKernelSpinlockLowLock(void *lock);
int ksceKernelDisableIntr(unsigned interrupt);
void *ksceSysrootGetSysroot(void);
void bsodPrintf(const char *format, ...);
int bsodFreezeLog(void);
int bsodPrintLog(const void *frame, uint32_t buttons);
void bsodSetFont(unsigned width, unsigned height);
void bsodSetFramebuffer(void *pixels, unsigned pitch, unsigned height, unsigned format);
void bsodResetColors(void);
void bsodSetBackground(uint32_t color);
void bsodClearGrid(void);
void bsodRasterize(void);
uint32_t bsodReadButtons(void);
void bsodPowerOff(void);

uint32_t s_bsodButtons;

static void bsodReboot(unsigned dump)
{
	void *sysroot = ksceSysrootGetSysroot();

	moncall4(MONCALL_CRASH_DUMP, dump, readTtbr0(), readVbar(), (uintptr_t)sysroot);
}

void bsodCrashHandler(int kind)
{
	uint32_t buttons, edge;
	unsigned page = kind ? 0 : 3;

	ksceKernelSpinlockLowLock(&s_bsodDisplay.lock);
	ksceKernelDisableIntr(0xd5);
	ksceKernelDisableIntr(0xd2);
	ksceKernelDisableIntr(0xf8);
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
		case 1: bsodReportStack(&s_bsodException, 0); break;
		case 2: bsodReportStack(&s_bsodException, 1); break;
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
int ksceSysrootGetSessionId(void *session);
int sceKernelSysrootBacktrace(int thread, void *frames, unsigned size,
			       void *count, int flags);
uint64_t SceSysrootForKernel_E20F6FC8(void);
unsigned ksceKernelCpuId(void);
int SceDebugForKernel_5D6F0CFC(void *buffer, unsigned core, uint64_t time,
			    const char *name, const SceExcpmgrContext *context,
			    unsigned exception);

void bsodCaptureException(const SceExcpmgrContext *context, unsigned exception,
			  const char *name)
{
	BsodCrashRecord *record = &s_bsodException;
	SceSysrootSharedMemory *shared;
	uint32_t jmcr;
	uint64_t time;
	unsigned core, i;
	char *report;

	ksceKernelSpinlockLowLock(&s_bsodDisplay.captureLock);
	record->excpCode = context->excpCode;
	for (i = 0; i < 13; i++)
		record->r[i] = context->r[i];
	record->sp = context->sp;
	record->lr = context->lr;
	record->pc = context->pc;
	for (i = 0; i < 32; i++) {
		record->vfp[i][0] = context->vfp[i];
		record->vfp[i][1] = context->vfp[i] >> 32;
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
	for (i = 0; i < 6; i++)
		record->pmuType[i] = context->pmuCounter[0].type;
	for (i = 0; i < 6; i++)
		record->pmuCount[i] = context->pmuCounter[i].count;
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
	ksceSysrootGetSessionId(record->sessionId);
	sceKernelSysrootBacktrace(-16, record->kernelFrames,
				   sizeof(record->kernelFrames), &record->kernelFrameCount, 3);
	if (record->thread)
		sceKernelSysrootBacktrace(-16, record->userFrames,
					   sizeof(record->userFrames), &record->userFrameCount, 10);
	time = SceSysrootForKernel_E20F6FC8();
	core = ksceKernelCpuId();
	shared = sysrootSharedMemory();
	ksceKernelGetTtyInfo(shared->tty, sizeof(shared->tty));
	report = shared->report[core];
	memset(report, 0, sizeof(shared->report[core]));
	SceDebugForKernel_5D6F0CFC(report, core, time, name, context, exception);
	bsodCrashHandler(0);
}

/* Native exception wrappers ignore user-mode exceptions and capture kernel faults. */
BsodCrashRecord s_bsodException;

void _sceKernelBsodPrefetchAbort(const SceExcpmgrContext *context)
{
	if ((context->cpsr & 0x1f) != 0x10)
		bsodCaptureException(context, 4, "_sceKernelBsodPrefetchAbort");
}

void _sceKernelBsodDataAbort(const SceExcpmgrContext *context)
{
	if ((context->cpsr & 0x1f) != 0x10)
		bsodCaptureException(context, 1, "_sceKernelBsodDataAbort");
}

void _sceKernelBsodUndfinedAbort(const SceExcpmgrContext *context)
{
	if ((context->cpsr & 0x1f) != 0x10)
		bsodCaptureException(context, 6, "_sceKernelBsodUndfinedAbort");
}

int SceDeci4pUserp_C53CFD12(void)
{
	return 0;
}
