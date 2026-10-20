/* Crash record consumed by the register, stack and log pages. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <excpmgr/excpmgr.h>
#include <sysmem/sysroot_modulemgr.h>

typedef struct SceKernelBacktraceFrameForKernel BsodStackFrame;

typedef struct BsodCrashRecord {
	uint32_t reserved;
	uint32_t excpCode;
	uint32_t r[13];
	uint32_t sp, lr, pc;
	uint32_t vfp[32][2];
	uint32_t cpsr;
	uint32_t midr, ctr, tcmtr, tlbtr, mpidr;
	uint32_t pfr[2], dfr0, mmfr[4], isar[5];
	uint32_t ccsidr, clidr, aidr, csselr;
	uint32_t sctlr, actlr, cpacr;
	uint32_t ttbr0, ttbr1, ttbcr, dacr;
	uint32_t dfsr, ifsr, adfsr, aifsr, dfar, ifar, par;
	uint32_t pmcr, pmcntenset, pmovsr, pmselr, pmccntr;
	uint32_t pmuType[6], pmuCount[6], pmuserenr;
	uint32_t prrr, nmrr;
	uint32_t pleuar, plepcr;		/* ~0 when NSACR disables the PLE */
	uint32_t vbar, fcseidr, contextId, tpidrurw, tpidruro;
	ThreadCB *thread;
	uint32_t powerControl, fpsid, fpscr, mvfr1, mvfr0, fpexc;
	uint32_t teecr, teehbr;
	uint32_t jidr, joscr, jmcr, jaolr;
	uint32_t dbgdscr;
	uint8_t sessionId[16];
	SceSize kernelFrameCount, userFrameCount;
	BsodStackFrame kernelFrames[32];
	BsodStackFrame userFrames[32];
} BsodCrashRecord;

_Static_assert(offsetof(BsodCrashRecord, cpsr) == 0x148, "crash CPSR");
_Static_assert(offsetof(BsodCrashRecord, thread) == 0x244, "crash thread");
_Static_assert(offsetof(BsodCrashRecord, sessionId) == 0x27c, "crash session");
_Static_assert(offsetof(BsodCrashRecord, kernelFrames) == 0x294, "kernel backtrace");
_Static_assert(offsetof(BsodCrashRecord, userFrames) == 0x494, "user backtrace");
_Static_assert(sizeof(BsodCrashRecord) == 0x694, "crash record");
extern BsodCrashRecord s_bsodException;

void bsodReportRegisters(const BsodCrashRecord *frame);
void bsodReportKernelStack(const BsodCrashRecord *frame);
void bsodReportUserStack(const BsodCrashRecord *frame);

void bsodCaptureException(const SceExcpmgrContext *context, unsigned exception,
			  const SceKernelDebugInfo *info) __attribute__((noreturn));
