/*
 * SceSysmem: assertion failures, panics and kernel exceptions. Each is
 * reported on the console, dumped into the CPU's page of the shared memory
 * (header, kernel and user backtraces, registers, the current syscall frame)
 * and handed to the registered crash-dump handlers. A panic, or an assertion
 * failure with DIP switch 200 set, stops the CPU.
 *
 * Assert and Panic run the report on a per-CPU fault stack, since the
 * failing thread's own stack may be what failed.
 *
 * Also the kernel TTY ring's reader and the event log.
 */

#include <sysmem/cp15.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <sysmem/uid_guid.h>
#include <sysmem/sysclib.h>
#include <fw.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <threadmgr/threadcb.h>
#include <sysmem/cpu.h>
#include <excpmgr/excpmgr.h>
#include <sysmem/sysroot_modulemgr.h>
#include "debug_tty.h"

int SceCpuForDriver_821FC0EE(void);
int sceKernelCpuResumeIntr(int flags);
int sceKernelCpuId(void);
int SceDebugForDriver_411C0733(const char *format, va_list arguments);
int SceDipswForDriver_A98FC2FD(unsigned int bit);
SceUInt64 SceSysrootForKernel_E20F6FC8(void);
SceUID SceSysrootForKernel_7385CADE(void);
SceUID SceSysrootForKernel_D441DC34(void);
SceUInt32 SceSysrootForDriver_44EA3197(void);
SceUInt32 SceSysrootForKernel_C8C8C321(void);
int SceSysrootForDriver_CC7A0E63(int type, SceSize size, const char *buffer);
void SceSysrootForKernel_0DF574A9(void);
void *SceSysrootForKernel_3E455842(void);
int SceQafMgrForDriver_382C71E8(void);
int SceSysrootForKernel_FF9F80FF(SceUID pid, SceUInt32 addr, struct ModulemgrStubInfo *info);
int SceSysrootForKernel_1D84C4D4(SceUID pid, SceUInt32 addr, SceUInt32 *fingerprint,
				void **textBase, char *name);
int SceSysrootForKernel_0B79E220(uint32_t nid, const char **name);

/* debug_asm.S */
void DebugCallOnStack(void *argument, void *stackTop, void (*func)(void *));
void DebugSecureCrashDump(int kind, SceUInt32 ttbr0, SceUInt32 vbar, void *sysroot);

/* debug_state.c */
extern volatile uint32_t s_assertLevel;

/* debug_output.c: the TTY ring and its lock */

extern volatile SceKernelSpinlock s_debugPrintLock;


/* the backtrace flags: whose frames */
#define DEBUG_BACKTRACE_USER		2
#define DEBUG_BACKTRACE_KERNEL		3

#define DEBUG_FAULT_ASSERT		0
#define DEBUG_FAULT_PANIC		1

/* the crash-dump types the handlers see */
#define DEBUG_DUMP_FAULT		1
#define DEBUG_DUMP_EXCEPTION		3

#define DEBUG_DIPSW_STOP_ON_ASSERT	200

/* a failed assertion or a panic, as Assert/Panic hand it to DebugFaultDump */
typedef struct DebugFault {
	int kind;
	int interrupts;			/* SceCpuForDriver_821FC0EE's result */
	const SceKernelDebugInfo *info;
	const void *caller;		/* where Assert/Panic itself was called */
	const void *lr;			/* the return address the caller passed */
	const char *format;		/* the Printf variants' message */
	va_list arguments;
} DebugFault;

/* the shared-memory dump: this header, then the sections it counts */
/* the dump keeps its time on a word boundary */
typedef SceUInt64 DebugDumpTime __attribute__((aligned(4)));

typedef struct DebugDumpHeader {
	SceSize size;
	SceUID pid;
	SceUID tid;
	int cpu;
	DebugDumpTime time;
	SceUInt32 fileHash;
	SceUInt32 lineHash;
	SceUInt32 funcHash;
	const void *caller;
	const void *lr;
	SceUInt32 kernelFrames;
	SceUInt32 userFrames;
	SceSize frameSize;
	SceUInt32 registerWords;	/* the exception context and its lookups */
	SceUInt32 syscallWords;		/* the syscall frame and its module */
} DebugDumpHeader;

_Static_assert(sizeof(DebugDumpHeader) == 0x40, "fault dump header size");
_Static_assert(sizeof(DebugExceptionDumpHeader) == 0x38, "exception dump header size");

/* name unknown: the name of the module a syscall entry is in */
typedef struct DebugModuleName {
	char name[0x20];
} DebugModuleName;

/* the syscall frame as dumped, with the module of its entry */
typedef struct DebugDumpSyscall {
	ThreadSyscallFrame frame;
	SceUInt32 fingerprint;
	void *textBase;
	DebugModuleName moduleName;
} DebugDumpSyscall;

/* where a code address lies, as dumped */
typedef struct DebugDumpAddress {
	SceUInt32 fingerprint;
	void *textBase;
} DebugDumpAddress;

#define DEBUG_DUMP_OFFSET		0x2000	/* in the shared memory, one page per CPU */
#define DEBUG_TTY_OFFSET		0x1000

DebugCrashDumpHandler s_crashDumpHandler;
static DebugFaultHandler s_faultHandler;
static DebugPrintBacktraceHandler s_printBacktraceHandler;
static DebugBacktraceHandler s_backtraceHandler;
int s_crashDumpEnabled;			/* set at boot unless the KBL says not to */
static DebugFrame s_backtraceFrames[4][32];
static SceUInt8 s_faultStacks[4][0x1000];

void SceDebugForKernel_4C62F8CC(DebugCrashDumpHandler handler)
{
	s_crashDumpHandler = handler;
}

int SceDebugForKernel_A465A31A(int enable)
{
	int previous = s_crashDumpEnabled;

	s_crashDumpEnabled = enable;
	return previous;
}

void SceDebugForKernel_91E7D24F(DebugBacktraceHandler handler)
{
	s_backtraceHandler = handler;
}

void SceDebugForKernel_38F1A352(DebugPrintBacktraceHandler handler)
{
	s_printBacktraceHandler = handler;
}

void SceDebugForKernel_67E9D1EC(DebugFaultHandler handler)
{
	s_faultHandler = handler;
}

static inline int DebugCpuId(void)
{
	return readMpidr() & 3;
}

/* name unknown: prints the kernel backtrace, then thid's user one */
__attribute__((noinline))
void DebugPrintBacktrace(SceUID pid, SceUID thid, const void *lr)
{
	DebugFrame *frames;
	SceSize count;

	if (s_backtraceHandler == NULL || s_printBacktraceHandler == NULL)
		return;
	frames = s_backtraceFrames[sceKernelCpuId()];
	memset(frames, 0, sizeof(s_backtraceFrames[0]));
	if (s_backtraceHandler(thid, frames, sizeof(s_backtraceFrames[0]), &count,
			       DEBUG_BACKTRACE_KERNEL) >= 0)
		s_printBacktraceHandler(SCE_KERNEL_PROCESS_ID, frames, count, lr);
	if (pid == -1)
		return;
	memset(frames, 0, sizeof(s_backtraceFrames[0]));
	if (s_backtraceHandler(thid, frames, sizeof(s_backtraceFrames[0]), &count,
			       DEBUG_BACKTRACE_USER) >= 0)
		s_printBacktraceHandler(pid, frames, count, NULL);
}

/* name unknown: prints a syscall frame (the current one by default) and its result */
__attribute__((noinline))
void DebugPrintSyscallFrame(SceUID pid, const ThreadSyscallFrame *frame, const SceUInt32 *result)
{
	struct ModulemgrStubInfo stub;
	char name[0x20];
	SceUInt32 fingerprint;
	void *textBase;
	const char *nidName;

	if (pid == 0)
		pid = SceSysrootForKernel_7385CADE();
	if (pid == SCE_KERNEL_PROCESS_ID)
		return;
	if (frame == NULL) {
		frame = (const ThreadSyscallFrame *)SceSysrootForDriver_44EA3197();
		if (frame == NULL)
			return;
	}
	SceDebugForDriver_391B74B7("=== Syscall Frame ===\n");
	SceDebugForDriver_391B74B7("r0-r3:     0x%08X 0x%08X 0x%08X 0x%08X\n",
			 frame->r[0], frame->r[1], frame->r[2], frame->r[3]);
	SceDebugForDriver_391B74B7("r12:       0x%08X\n", frame->r[12]);
	SceDebugForDriver_391B74B7("SP:        0x%08X\n", frame->sp);
	SceDebugForDriver_391B74B7("LR:        0x%08X\n", frame->lr);
	SceDebugForDriver_391B74B7("PC:        0x%08X\n", frame->pc);
	if (SceQafMgrForDriver_382C71E8()) {
		if (SceSysrootForKernel_1D84C4D4(SCE_KERNEL_PROCESS_ID, frame->entry,
						 &fingerprint, &textBase, name) < 0)
			SceDebugForDriver_391B74B7("Entry:     0x%08X\n", frame->entry);
		else
			SceDebugForDriver_391B74B7("Entry:     0x%08X [%s + 0x%08X] # 0x%08X\n", frame->entry,
					 name, frame->entry - (SceUInt32)textBase, fingerprint);
	}
	if (result != NULL) {
		/* bit 30 of a result marks an error the syscall layer passes on */
		switch (frame->apiDeclare) {
		case 0:
			SceDebugForDriver_391B74B7("Result:    0x%08X\n", result[0] & ~0x40000000);
			break;
		case 1:
			SceDebugForDriver_391B74B7("Result:    0x%08X%08X\n", result[1],
					 result[0] & ~0x40000000);
			break;
		case 2:
			SceDebugForDriver_391B74B7("Result:    0x%08X\n", result[0]);
			break;
		case 3:
			SceDebugForDriver_391B74B7("Result:    0x%08X%08X\n", result[1], result[0]);
			break;
		default:
			SceDebugForDriver_391B74B7("Result:    0x%08X 0x%08X 0x%08X 0x%08X\n",
					 result[0], result[1], result[2], result[3]);
			break;
		}
	}
	stub.size = sizeof(stub);
	if (SceSysrootForKernel_FF9F80FF(pid, frame->pc - 8, &stub) < 0)
		return;
	SceDebugForDriver_391B74B7("Module:    0x%08x [%s]\n", stub.modid, stub.moduleName);
	SceDebugForDriver_391B74B7("Library:   %s (ver:%d)\n", stub.libname, stub.libVersion);
	if (SceSysrootForKernel_0B79E220(stub.funcnid, &nidName) < 0)
		SceDebugForDriver_391B74B7("NID:       0x%08x\n", stub.funcnid);
	else
		SceDebugForDriver_391B74B7("NID:       0x%08x [%s]\n", stub.funcnid, nidName);
}

/* name unknown: copies whole 16-byte blocks into the dump */
static inline void DebugDumpCopy(void *dst, const void *src, SceSize size)
{
	SceUInt32 *out = dst;
	const SceUInt32 *in;
	SceSize offset;

	for (offset = 0; offset != size; offset += 16) {
		in = (const SceUInt32 *)((const char *)src + offset);
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
		out[3] = in[3];
		out += 4;
	}
}

/* name unknown: the report of a failed assertion or a panic, on the fault stack */
void DebugFaultDump(void *argument)
{
	DebugFault *fault = argument;
	const SceKernelDebugInfo *info = fault->info;
	DebugDumpHeader header;
	DebugFrame *frames, *out;
	DebugDumpHeader *dump;
	const ThreadSyscallFrame *syscall;
	DebugDumpSyscall *syscallDump;
	SceUInt64 time = SceSysrootForKernel_E20F6FC8();
	int cpu = sceKernelCpuId();
	SceUInt32 fingerprint;
	void *textBase;
	DebugModuleName name;
	char *report;
	SceSize count, size;
	const void *caller, *lr;
	SceUID tid;

	if (fault->kind == DEBUG_FAULT_ASSERT) {
		if (info->file == NULL)
			SceDebugForDriver_391B74B7("*** Kernel Assertion failed at <%d:0x%08x%08x:0x%08x:%p>\n",
					 cpu, info->fileHash, info->lineHash, info->funcHash,
					 fault->lr);
		else
			SceDebugForDriver_391B74B7("*** Kernel Assertion failed at <%d:0x%08x%08x(%s:%d):0x%08x(%s):%p>\n",
					 cpu, info->fileHash, info->lineHash, info->file,
					 info->line, info->funcHash, info->func, fault->lr);
	} else {
		if (info->file == NULL)
			SceDebugForDriver_391B74B7("*** Kernel Panic at <%d:0x%08x%08x:0x%08x:%p>\n",
					 cpu, info->fileHash, info->lineHash, info->funcHash,
					 fault->lr);
		else
			SceDebugForDriver_391B74B7("*** Kernel Panic at <%d:0x%08x%08x(%s:%d):0x%08x(%s):%p>\n",
					 cpu, info->fileHash, info->lineHash, info->file,
					 info->line, info->funcHash, info->func, fault->lr);
	}
	if (fault->format != NULL)
		SceDebugForDriver_411C0733(fault->format, fault->arguments);

	caller = fault->caller;
	lr = fault->lr;
	syscall = (const ThreadSyscallFrame *)SceSysrootForDriver_44EA3197();
	dump = (DebugDumpHeader *)(SceSysrootForKernel_C8C8C321() + DEBUG_DUMP_OFFSET +
				   cpu * 0x1000);
	out = (DebugFrame *)(dump + 1);
	memset(dump, 0, 0x1000);
	header.size = sizeof(header);
	header.pid = SceSysrootForKernel_7385CADE();
	tid = SceSysrootForKernel_D441DC34();
	header.tid = tid < 0 ? -1 : tid;
	header.cpu = cpu;
	header.time = time;
	header.fileHash = info->fileHash;
	header.lineHash = info->lineHash;
	header.funcHash = info->funcHash;
	header.caller = caller;
	header.lr = lr;
	header.kernelFrames = 0;
	header.userFrames = 0;
	header.frameSize = sizeof(DebugFrame);
	header.registerWords = 0;
	header.syscallWords = 0;
	if (s_backtraceHandler != NULL) {
		frames = s_backtraceFrames[cpu];
		if (s_backtraceHandler(0, frames, sizeof(s_backtraceFrames[0]), &count,
				       DEBUG_BACKTRACE_KERNEL) >= 0) {
			header.kernelFrames = count;
			DebugDumpCopy(out, frames, count * sizeof(DebugFrame));
			out += count;
		}
		if (s_backtraceHandler(0, frames, sizeof(s_backtraceFrames[0]), &count,
				       DEBUG_BACKTRACE_USER) >= 0) {
			header.userFrames = count;
			DebugDumpCopy(out, s_backtraceFrames[cpu], count * sizeof(DebugFrame));
			out += count;
		}
	}
	if (syscall != NULL) {
		syscallDump = (DebugDumpSyscall *)out;
		memcpy(&syscallDump->frame, syscall, sizeof(*syscall));
		fingerprint = -1;
		textBase = NULL;
		memset(&name, 0, sizeof(name));
		SceSysrootForKernel_1D84C4D4(SCE_KERNEL_PROCESS_ID, syscall->entry,
					     &fingerprint, &textBase, name.name);
		header.syscallWords = sizeof(*syscallDump) / 4;
		syscallDump->moduleName = name;
		syscallDump->fingerprint = fingerprint;
		syscallDump->textBase = textBase;
	}
	DebugDumpCopy(dump, &header, sizeof(header));
	size = header.frameSize * (header.kernelFrames + header.userFrames) +
	       (header.registerWords + header.syscallWords) * 4 + sizeof(header);

	/* with crash dumps on, the handlers get the TTY's last output instead */
	report = (char *)dump;
	if (s_crashDumpEnabled) {
		report = (char *)(SceSysrootForKernel_C8C8C321() + DEBUG_TTY_OFFSET);
		SceDebugForDriver_62466B0A(report, 0x1000);
	}
	if (s_crashDumpHandler != NULL)
		s_crashDumpHandler(DEBUG_DUMP_FAULT, size, report);
	SceSysrootForDriver_CC7A0E63(DEBUG_DUMP_FAULT, size, report);
	if (s_crashDumpEnabled) {
		void *sysroot = SceSysrootForKernel_3E455842();
		SceUInt32 ttbr0, vbar;

		ttbr0 = readTtbr0();
		vbar = readVbar();
		DebugSecureCrashDump(DEBUG_DUMP_FAULT, ttbr0, vbar, sysroot);
	}
	DebugPrintBacktrace(0, 0, fault->lr);
	DebugPrintSyscallFrame(0, NULL, NULL);
	if (s_faultHandler != NULL)
		s_faultHandler(time, cpu, info->fileHash, info->lineHash, info->funcHash, 0, 0,
			       fault->caller, fault->lr);
	if ((fault->kind == DEBUG_FAULT_ASSERT &&
	     SceDipswForDriver_A98FC2FD(DEBUG_DIPSW_STOP_ON_ASSERT)) ||
	    fault->kind == DEBUG_FAULT_PANIC) {
		sceKernelCpuResumeIntr(fault->interrupts);
		SceDebugForDriver_391B74B7("Stopped\n");
		for (;;)
			SceSysrootForKernel_0DF574A9();
	}
}

/* the top of this CPU's fault stack */
#define DEBUG_FAULT_STACK()	(&s_faultStacks[DebugCpuId()][0xff8])

void SceDebugForDriver_35A35322(SceBool cond, const SceKernelDebugInfo *info, const void *lr)
{
	DebugFault fault;

	if (cond)
		return;
	fault.interrupts = SceCpuForDriver_821FC0EE();
	fault.kind = DEBUG_FAULT_ASSERT;
	fault.info = info;
	fault.caller = __builtin_return_address(0);
	fault.lr = lr;
	fault.format = NULL;
	DebugCallOnStack(&fault, DEBUG_FAULT_STACK(), DebugFaultDump);
	sceKernelCpuResumeIntr(fault.interrupts);
}

/* returns no value: its callers' int is what r0 holds */
int SceDebugForDriver_821A2D59(SceUInt32 level, SceBool cond, const SceKernelDebugInfo *info,
			       const void *lr, const char *fmt, ...)
{
	DebugFault fault;

	va_start(fault.arguments, fmt);
	if (!cond && (int)level <= (int)s_assertLevel) {
		fault.interrupts = SceCpuForDriver_821FC0EE();
		fault.kind = DEBUG_FAULT_ASSERT;
		fault.info = info;
		fault.caller = __builtin_return_address(0);
		fault.lr = lr;
		fault.format = fmt;
		DebugCallOnStack(&fault, DEBUG_FAULT_STACK(), DebugFaultDump);
		sceKernelCpuResumeIntr(fault.interrupts);
	}
}

void SceDebugForDriver_391B5B74(const SceKernelDebugInfo *info, const void *lr)
{
	DebugFault fault;

	fault.interrupts = SceCpuForDriver_821FC0EE();
	fault.kind = DEBUG_FAULT_PANIC;
	fault.info = info;
	fault.caller = __builtin_return_address(0);
	fault.lr = lr;
	fault.format = NULL;
	DebugCallOnStack(&fault, DEBUG_FAULT_STACK(), DebugFaultDump);
	for (;;)
		;
}

void SceDebugForDriver_00CCE39C(const SceKernelDebugInfo *info, const void *lr, const char *fmt, ...)
{
	DebugFault fault;

	va_start(fault.arguments, fmt);
	fault.interrupts = SceCpuForDriver_821FC0EE();
	fault.kind = DEBUG_FAULT_PANIC;
	fault.info = info;
	fault.caller = __builtin_return_address(0);
	fault.lr = lr;
	fault.format = fmt;
	DebugCallOnStack(&fault, DEBUG_FAULT_STACK(), DebugFaultDump);
	for (;;)
		;
}

/* where a kernel code address lies, or -1/NULL for user addresses */
static inline void DebugDumpResolve(SceUInt32 address, SceUInt32 *fingerprint, void **textBase)
{
	*fingerprint = -1;
	*textBase = NULL;
	if (address < 0x40000000)
		SceSysrootForKernel_1D84C4D4(SCE_KERNEL_PROCESS_ID, address, fingerprint,
					     textBase, NULL);
}

/* the exception dump: returns its size */
SceSize SceDebugForKernel_5D6F0CFC(DebugExceptionDumpHeader *dump, int cpu, SceUInt64 time,
				   const SceKernelDebugInfo *info,
				   const SceExcpmgrContext *context, SceUInt32 arg)
{
	DebugExceptionDumpHeader header;
	const ThreadSyscallFrame *syscall;
	DebugFrame *frames, *out = (DebugFrame *)(dump + 1);
	SceUInt32 *registers;
	DebugDumpAddress *addresses;
	DebugDumpSyscall *syscallDump;
	SceUInt32 fingerprint;
	void *textBase;
	SceSize count;
	int i;

	if (context->thread != NULL) {
		header.pid = context->thread->processId;
		header.tid = context->thread->threadId;
		syscall = context->thread->userFrame;
	} else {
		header.pid = 0;
		header.tid = -1;
		syscall = NULL;
	}
	header.fileHash = info->fileHash;
	header.lineHash = info->lineHash;
	header.funcHash = info->funcHash;
	header.kernelFrames = 0;
	header.userFrames = 0;
	header.registerWords = 0;
	header.syscallWords = 0;
	header.size = sizeof(header);
	header.frameSize = sizeof(DebugFrame);
	header.cpu = cpu;
	header.time = time;
	if (s_backtraceHandler != NULL) {
		frames = s_backtraceFrames[cpu];
		if (s_backtraceHandler(-16, frames, sizeof(s_backtraceFrames[0]), &count,
				       DEBUG_BACKTRACE_KERNEL) >= 0 &&
		    (header.kernelFrames = count) != 0) {
			DebugDumpCopy(out, frames, count * sizeof(DebugFrame));
			out += count;
		}
		if (s_backtraceHandler(-16, frames, sizeof(s_backtraceFrames[0]), &count,
				       DEBUG_BACKTRACE_USER) >= 0 &&
		    (header.userFrames = count) != 0) {
			DebugDumpCopy(out, frames, count * sizeof(DebugFrame));
			out += count;
		}
	}

	/* the context, its PC's module, then where each saved address lies */
	registers = (SceUInt32 *)out;
	memcpy(registers, context, sizeof(*context));
	DebugDumpResolve(context->pc, &fingerprint, &textBase);
	registers[0x3f] = arg;
	registers[0x3c] = fingerprint;
	registers[0x3d] = (SceUInt32)textBase;
	header.registerWords = sizeof(*context) / 4;
	addresses = (DebugDumpAddress *)(registers + sizeof(*context) / 4);
	for (i = 0; i != 32; i++) {
		DebugDumpResolve(context->waypoint[i].pc, &fingerprint, &textBase);
		addresses->fingerprint = fingerprint;
		addresses->textBase = textBase;
		addresses++;
		DebugDumpResolve(context->waypoint[i].targetPc, &fingerprint, &textBase);
		addresses->fingerprint = fingerprint;
		addresses->textBase = textBase;
		addresses++;
	}
	header.registerWords += 32 * 2 * sizeof(DebugDumpAddress) / 4;
	if (syscall != NULL) {
		DebugModuleName name;

		syscallDump = (DebugDumpSyscall *)addresses;
		memcpy(&syscallDump->frame, syscall, sizeof(*syscall));
		fingerprint = -1;
		textBase = NULL;
		memset(&name, 0, sizeof(name));
		SceSysrootForKernel_1D84C4D4(SCE_KERNEL_PROCESS_ID, syscall->entry,
					     &fingerprint, &textBase, name.name);
		syscallDump->fingerprint = fingerprint;
		syscallDump->textBase = textBase;
		syscallDump->moduleName = name;
		header.syscallWords = sizeof(*syscallDump) / 4;
	}
	*dump = header;
	return (header.kernelFrames + header.userFrames) * header.frameSize + sizeof(header) +
	       (header.registerWords + header.syscallWords) * 4;
}

/* a kernel exception: report, dump and stop */
void SceDebugForKernel_CCABDD98(const SceKernelDebugInfo *info,
				const SceExcpmgrContext *context, SceUInt32 arg,
				const char *type)
{
	SceUInt64 time = SceSysrootForKernel_E20F6FC8();
	DebugExceptionDumpHeader *dump;
	char *report;
	SceUID pid = 0;
	SceSize size;
	int cpu;

	SceCpuForDriver_821FC0EE();
	cpu = sceKernelCpuId();
	if (context->thread != NULL)
		pid = context->thread->processId;
	if (info->file == NULL)
		SceDebugForDriver_391B74B7("*** Kernel %s Exception at CPU%d <0x%08x%08x:0x%08x>\n", type, cpu,
				 info->fileHash, info->lineHash, info->funcHash);
	else
		SceDebugForDriver_391B74B7("*** Kernel %s Exception at CPU%d <0x%08x%08x(%s:%d):0x%08x(%s)>\n",
				 type, cpu, info->fileHash, info->lineHash, info->file,
				 info->line, info->funcHash, info->func);
	dump = (DebugExceptionDumpHeader *)(SceSysrootForKernel_C8C8C321() +
					    DEBUG_DUMP_OFFSET + cpu * 0x1000);
	memset(dump, 0, 0x1000);
	size = SceDebugForKernel_5D6F0CFC(dump, cpu, time, info, context, arg);
	report = (char *)dump;
	if (s_crashDumpEnabled) {
		report = (char *)(SceSysrootForKernel_C8C8C321() + DEBUG_TTY_OFFSET);
		SceDebugForDriver_62466B0A(report, 0x1000);
	}
	if (s_crashDumpHandler != NULL)
		s_crashDumpHandler(DEBUG_DUMP_EXCEPTION, size, report);
	SceSysrootForDriver_CC7A0E63(DEBUG_DUMP_EXCEPTION, size, report);
	if (s_crashDumpEnabled) {
		void *sysroot = SceSysrootForKernel_3E455842();
		SceUInt32 ttbr0, vbar;

		ttbr0 = readTtbr0();
		vbar = readVbar();
		DebugSecureCrashDump(0, ttbr0, vbar, sysroot);
	}
	DebugPrintBacktrace(pid, -16, NULL);
	DebugPrintSyscallFrame(pid, NULL, NULL);
	SceDebugForDriver_391B74B7("Stopped\n");
	for (;;)
		SceSysrootForKernel_0DF574A9();
}

void SceDebugForKernel_BCC8D0B2(SceUID pid, const ThreadSyscallFrame *frame,
				const SceUInt32 *result)
{
	DebugPrintSyscallFrame(pid, frame, result);
}

/* the last output in the TTY ring, oldest first */
int SceDebugForDriver_62466B0A(char *buffer, SceSize size)
{
	SceUInt32 index, tail;
	int flags;

	if (size >= sizeof(s_debugTty.ring))
		size = sizeof(s_debugTty.ring);
	flags = SceCpuForDriver_D32ACE9E(&s_debugPrintLock);
	index = s_debugTty.index;
	tail = sizeof(s_debugTty.ring) - index;
	if (tail >= size) {
		if (size != 0)
			memcpy(buffer, &s_debugTty.ring[index], size);
	} else {
		if (index != sizeof(s_debugTty.ring))
			memcpy(buffer, &s_debugTty.ring[index], tail);
		if (size + index != sizeof(s_debugTty.ring))
			memcpy(buffer + tail, s_debugTty.ring,
			       size + index - sizeof(s_debugTty.ring));
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_debugPrintLock, flags);
	return size;
}

/*
 * The event log: 16 entries of 0xc0 bytes, four per CPU used round robin, each
 * a 0x40-byte header and up to 0x80 bytes of data.
 */
#define EVENT_LOG_ENTRIES		16
#define EVENT_LOG_DATA_MAX		0x80

typedef struct EventLogEntry {
	SceSize size;			/* the header and the data, word aligned */
	SceSize titleIdLength;
	char titleId[12];
	SceUInt16 type;			/* 0x14 */
	SceUInt16 subtype;
	SceUID pid;			/* 0x18 */
	SceUID tid;
	SceUInt32 reserved[4];		/* zeroed */
	SceUInt64 time;			/* 0x30 */
	SceUInt32 arg;			/* 0x38 */
	SceSize dataSize;
	SceUInt8 data[EVENT_LOG_DATA_MAX];	/* 0x40 */
} EventLogEntry;

_Static_assert(sizeof(EventLogEntry) == 0xc0, "event log entry size");

/* each CPU's entries */
static const struct {
	SceUInt32 first;
	SceUInt32 last;
} s_eventLogRange[4] = { { 0, 3 }, { 4, 7 }, { 8, 11 }, { 12, 15 } };

/* the title the kernel's own events are logged under */
static const char s_eventLogTitleId[10] __attribute__((aligned(4))) = "NPXS19999";

static EventLogHandler s_eventLogHandler;
static SceUInt32 s_eventLogNext[4] = { 0, 4, 8, 12 };
static EventLogEntry s_eventLog[EVENT_LOG_ENTRIES];

void SceDebugForKernel_78027203(EventLogHandler handler)
{
	s_eventLogHandler = handler;
}

int SceDebugForDriver_912CF2BA(SceUID pid, SceUID tid, SceUInt16 type, SceUInt16 subtype, SceUInt32 arg,
		    const void *data, SceSize size)
{
	EventLogEntry *entry;
	SceUInt32 index;
	SceSize entrySize;
	SceUInt64 time;
	int flags, cpu;

	if (data == NULL && size != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	if (size > EVENT_LOG_DATA_MAX)
		return SCE_ERROR_ERRNO_EDOM;
	flags = SceCpuForDriver_821FC0EE();
	cpu = DebugCpuId();
	index = s_eventLogNext[cpu];
	entrySize = (size + offsetof(EventLogEntry, data) + 3) & ~3;
	s_eventLogNext[cpu] = index == s_eventLogRange[cpu].last ? s_eventLogRange[cpu].first :
								    index + 1;
	entry = &s_eventLog[index];
	time = SceSysrootForKernel_E20F6FC8();
	memset(entry, 0, offsetof(EventLogEntry, data));
	entry->size = entrySize;
	__builtin_memcpy(entry->titleId, s_eventLogTitleId, sizeof(s_eventLogTitleId));
	entry->type = type;
	entry->pid = pid;
	entry->tid = tid;
	entry->time = time;
	entry->subtype = subtype;
	entry->titleIdLength = 10;
	entry->arg = arg;
	entry->dataSize = size;
	memcpy(entry->data, data, size);
	if (s_eventLogHandler != NULL)
		s_eventLogHandler(entry, entrySize);
	sceKernelCpuResumeIntr(flags);
	return 0;
}

int k_sceEventLogPut(SceUInt16 type, SceUInt16 subtype, SceUInt32 arg, const void *data,
		     SceSize size)
{
	return SceDebugForDriver_912CF2BA(SCE_KERNEL_PROCESS_ID, SceSysrootForKernel_D441DC34(), type,
			       subtype, arg, data, size);
}

/* copies the valid entries out; returns the bytes copied */
SceSSize SceDebugForDriver_CC5365D3(void *buffer, SceSize size, SceUInt32 *count)
{
	const EventLogEntry *entry;
	SceSize left = size, entrySize;
	SceUInt32 n = 0;
	int flags;

	if (buffer == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	flags = SceCpuForDriver_821FC0EE();
	for (entry = s_eventLog; entry != &s_eventLog[EVENT_LOG_ENTRIES]; entry++) {
		entrySize = entry->size;
		if (entrySize - offsetof(EventLogEntry, data) <= EVENT_LOG_DATA_MAX &&
		    entrySize <= left) {
			memcpy(buffer, entry, entrySize);
			left -= entrySize;
			n++;
			buffer = (char *)buffer + entrySize;
		}
	}
	if (count != NULL)
		*count = n;
	sceKernelCpuResumeIntr(flags);
	return size - left;
}
