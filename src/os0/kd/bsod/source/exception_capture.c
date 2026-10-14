/* Capture a saved exception context and the live diagnostic CPU registers. */
#include "bsod_internal.h"
#include "exception_record.h"
#include <sce/sysroot.h>
#include <sce/cp15.h>
#include <sce/excpmgr_cpu.h>

int ksceKernelSpinlockLowLock(void *lock);
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
