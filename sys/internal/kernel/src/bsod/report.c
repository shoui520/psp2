/* Register and stack reports from the captured ARM exception frame. */
#include <sysmem/cp15.h>
#include "bsod_internal.h"
#include <stdio.h>
#include "report_data.h"
#include "exception_record.h"
#include <sysmem/sysmem.h>

extern void (*s_bsodPrinter)(const char *format, ...);
int ksceKernelGetModuleInfoMinByAddr(int pid, uint32_t address, int *uid,
				    uint32_t *base, char *name);
static void bsodFormatCpsr(uint32_t cpsr, char text[52])
{
	armFormatCpsr(cpsr, text);
}

void bsodReportRegisters(const BsodCrashRecord *frame)
{
	const uint8_t *session = frame->sessionId;
	const ThreadCB *context = frame->thread;
	const char *name;
	uint32_t status, index, domain;
	unsigned i;
	char flags[52];

	if (frame->excpCode - 1 < 7)
		s_bsodPrinter(s_bsodTextCause, s_bsodReasons[frame->excpCode], frame->mpidr & 3);
	else
		s_bsodPrinter(s_bsodTextUnknownCause, frame->excpCode, frame->mpidr & 3);
	s_bsodPrinter(s_bsodTextSessionId, session[0], session[1], session[2], session[3],
		session[4], session[5], session[6], session[7], session[8], session[9],
		session[10], session[11], session[12], session[13], session[14], session[15]);
	if (!context) {
		s_bsodPrinter(s_bsodTextHandlerContext);
	} else {
		if (SceSysmemForDriver_A78755EB(context->processId, &name) < 0)
			s_bsodPrinter(s_bsodTextProcessId, context->processId);
		else
			s_bsodPrinter(s_bsodTextProcessIdName, context->processId, name);
		if (SceSysmemForDriver_A78755EB(context->threadId, &name) < 0)
			s_bsodPrinter(s_bsodTextThreadId, context->threadId);
		else
			s_bsodPrinter(s_bsodTextThreadIdName, context->threadId, name);
	}
	s_bsodPrinter(s_bsodTextR0R3, frame->r[0], frame->r[1], frame->r[2], frame->r[3]);
	s_bsodPrinter(s_bsodTextR4R7, frame->r[4], frame->r[5], frame->r[6], frame->r[7]);
	s_bsodPrinter(s_bsodTextR8R11, frame->r[8], frame->r[9], frame->r[10], frame->r[11]);
	s_bsodPrinter(s_bsodTextR12, frame->r[12]);
	s_bsodPrinter(s_bsodTextSp, frame->sp);
	s_bsodPrinter(s_bsodTextLr, frame->lr);
	s_bsodPrinter(s_bsodTextPc, frame->pc);
	bsodFormatCpsr(frame->cpsr, flags);
	s_bsodPrinter(s_bsodTextCpsr, frame->cpsr, flags);
	if ((frame->cpacr & 0xf00000) && (frame->fpexc & 0x40000000)) {
		for (i = 0; i < 32; i += 4) {
			s_bsodPrinter(s_bsodTextVfpRegisters, i, i + 3,
				frame->vfp[i][1], frame->vfp[i][0],
				frame->vfp[i + 1][1], frame->vfp[i + 1][0],
				frame->vfp[i + 2][1], frame->vfp[i + 2][0],
				frame->vfp[i + 3][1], frame->vfp[i + 3][0]);
		}
		s_bsodPrinter(s_bsodTextFpexc, frame->fpexc);
		s_bsodPrinter(s_bsodTextFpscr, frame->fpscr);
		s_bsodPrinter(s_bsodTextNewline);
	}
	s_bsodPrinter(s_bsodTextContextId, frame->contextId);
	s_bsodPrinter(s_bsodTextDacr, frame->dacr);
	s_bsodPrinter(s_bsodTextTtbr1, frame->ttbr1);
	s_bsodPrinter(s_bsodTextTpidrurw, frame->tpidrurw);
	s_bsodPrinter(s_bsodTextTpidruro, frame->tpidruro);
	s_bsodPrinter(s_bsodTextTpidrprw, frame->thread);
	if (frame->excpCode == 3) {
		status = frame->ifsr;
		index = (status & 15) | (status >> 6 & 16);
		s_bsodPrinter(s_bsodTextIfsr, status, s_bsodIfsr[index]);
		if ((status & 0x40f) == 2)
			s_bsodPrinter(s_bsodTextDbgdscr, frame->dbgdscr,
				s_bsodDebugStatus[frame->dbgdscr >> 2 & 15]);
		s_bsodPrinter(s_bsodTextAifsr, frame->aifsr);
		s_bsodPrinter(s_bsodTextIfar, frame->ifar);
		s_bsodPrinter(s_bsodTextNewline);
	}
	if (frame->excpCode == 4) {
		status = readDfsr();
		index = (status & 15) | (status >> 6 & 16);
		s_bsodPrinter(s_bsodTextDfsr, status, s_bsodDfsr[index].name);
		if (s_bsodDfsr[index].domainValid) {
			domain = status >> 4 & 15;
			s_bsodPrinter(s_bsodTextDfsrDomain, domain,
				s_bsodDomains[frame->dacr >> (domain * 2) & 3]);
		} else {
			s_bsodPrinter(s_bsodTextDfsrDomainInvalid);
		}
		s_bsodPrinter(s_bsodTextAdfsr, frame->adfsr);
		s_bsodPrinter(s_bsodTextDfar, frame->dfar,
			s_bsodDfsr[index].addressValid ? s_bsodTextValid : s_bsodTextInvalid);
		s_bsodPrinter(s_bsodTextNewline);
	}
}

void bsodReportStack(const BsodCrashRecord *frame, int user)
{
	const ThreadCB *context = frame->thread;
	const BsodStackFrame *entry;
	int pid, count, i, uid;
	uint32_t base, size;
	char name[28];

	if (!user) {
		entry = frame->kernelFrames;
		count = frame->kernelFrameCount;
		pid = SCE_KERNEL_PROCESS_ID;
		s_bsodPrinter(s_bsodTextKernelBacktrace);
	} else {
		if (!context || !context->processId || context->processId == SCE_KERNEL_PROCESS_ID) {
			s_bsodPrinter(s_bsodTextUserBacktraceEmpty);
			return;
		}
		entry = frame->userFrames;
		count = frame->userFrameCount;
		pid = context->processId;
		s_bsodPrinter(s_bsodTextUserBacktrace);
	}
	for (i = 0; i != count; i++, entry++) {
		uid = 0;
		base = 0;
		int result = ksceKernelGetModuleInfoMinByAddr(pid, entry->pc, &uid, &base, name);
		size = i == count - 1 ? 0 : entry[1].sp - entry->sp;
		if (result < 0) {
			s_bsodPrinter(s_bsodTextBacktraceFrameNoModule, i, entry->pc, entry->sp, size);
		} else {
			if (!user && !s_bsodQafEnabled)
				snprintf(name, 0x1b, s_bsodTextHex, uid);
			s_bsodPrinter(s_bsodTextBacktraceFrame, i, entry->pc, entry->sp, size,
				name, entry->pc - base);
		}
	}
}
