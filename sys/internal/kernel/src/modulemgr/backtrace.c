/*
 * backtrace.c - stack backtraces from the ARM exception index (EHABI
 * unwind tables) of the loaded modules
 *
 * Sony built this file with -Os.
 */

#include "modulemgr_internal.h"
#include <excpmgr/excpmgr.h>
#include <excpmgr/excpmgr_cpu.h>
#include <threadmgr/threadcb.h>

#define BACKTRACE_FLAG_KERNEL_STACK	0x1	/* the kernel stack of a user thread */
#define BACKTRACE_FLAG_LIMIT		0x2	/* stop when the buffer is full */
#define BACKTRACE_FLAG_LINEAR_EXIDX	0x8
#define BACKTRACE_FLAGS			0xb

#define BACKTRACE_THID_EXCEPTION	(-16)	/* the context of the current exception */

#define UNWIND_FAILED			9

enum { REG_SP = 13, REG_LR = 14, REG_PC = 15 };

typedef struct SceKernelBacktraceFrame {
	SceUInt32 sp;
	SceUInt32 pc;
} SceKernelBacktraceFrame;

/* one frame (sysmem/sysroot_modulemgr.h) */
typedef struct SceKernelBacktraceFrameForKernel SceKernelBacktraceFrameForKernel;

/* the reader of the unwind instructions */
typedef struct UnwindState {
	SceUInt32 *regs;
	const SceUInt32 *insn;
	int words;
	int byte;
	SceUID pid;
} UnwindState;

int copyWord(SceUID pid, SceUInt32 *dst, const SceUInt32 *src)
{
	int ret;

	if (pid == SCE_KERNEL_PROCESS_ID) {
		*dst = *src;
		return 0;
	}
	ret = sceKernelCopyFromUserProc(pid, dst, src, 4);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 120,
			" error: sceKernelCopyFromUserProc(0x%08X, %p %p, 0x%08X) = 0x%08X\n",
			pid, dst, src, 4, ret);
	return ret;
}

/* name unknown: the next byte of the unwind instructions */
int unwindGetByte(UnwindState *st)
{
	SceUInt32 w;
	int b;

	if (st->words <= 0)
		return 0;
	if ((SceInt32)st->insn < 0) {
		if (copyWord(st->pid, &w, st->insn) < 0)
			return 0;
	} else {
		w = *st->insn;
	}
	b = (w >> (st->byte * 8)) & 0xff;
	if (st->byte == 0) {
		st->byte = 3;
		st->insn++;
		st->words--;
	} else {
		st->byte--;
	}
	return b;
}

int copyStack(SceUID pid, SceUInt32 *dst, const SceUInt32 *src)
{
	SceUInt32 pa;
	int ret;

	if (pid == SCE_KERNEL_PROCESS_ID) {
		ret = SceCpuForKernel_14E0B0C4((uintptr_t)src, &pa);
		if (ret > 0)
			ret = 0;
		if (ret == 0)
			*dst = *src;
		return ret;
	}
	ret = sceKernelCopyFromUserProc(pid, dst, src, 4);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xb, 204,
			" error: sceKernelCopyFromUserProc(0x%08X, %p %p, 0x%08X) = 0x%08X\n",
			pid, dst, src, 4, ret);
	return ret;
}

int execUnwindInstruction(SceUID pid, SceUInt32 *regs, SceUInt32 pc, SceUInt32 flags)
{
	SceUInt32 exidx[2], w, insn, mask, vsp, newsp, uleb;
	UnwindState st;
	int reg, shift, b, ret;

	ret = BacktraceFindExidx(pid, pc, exidx, flags);
	if (ret < 0)
		return ret;
	regs[REG_PC] = 0;
	st.regs = regs;
	st.pid = pid;
	if (exidx[0] == 1)
		return UNWIND_FAILED;
	if ((SceInt32)exidx[0] < 0) {
		/* inline: personality routine 0 */
		w = exidx[0];
		st.insn = &exidx[0];
		if ((w & 0xff000000) != 0x80000000)
			return UNWIND_FAILED;
	} else {
		st.insn = (const SceUInt32 *)exidx[1];
		if ((SceInt32)st.insn < 0) {
			ret = copyWord(pid, &w, st.insn);
			if (ret < 0)
				return ret;
		} else {
			w = *st.insn;
		}
	}
	if ((w & 0xff000000) == 0x80000000) {
		st.byte = 2;
		st.words = 1;
	} else if ((w & 0xff000000) == 0x81000000) {
		st.byte = 1;
		st.words = ((w >> 16) & 0xff) + 1;
	} else {
		if ((SceInt32)w < 0)
			return UNWIND_FAILED;
		/* generic model: skip the personality routine */
		st.insn++;
		ret = copyWord(pid, &w, st.insn);
		if (ret < 0)
			return ret;
		st.byte = 2;
		st.words = (w >> 24) + 1;
	}

	while (st.words > 0) {
		insn = unwindGetByte(&st);
		if ((insn & 0xc0) == 0x00) {
			regs[REG_SP] += (insn & 0x3f) * 4 + 4;
		} else if ((insn & 0xc0) == 0x40) {
			regs[REG_SP] -= (insn & 0x3f) * 4 + 4;
		} else if ((insn & 0xf0) == 0x80) {
			vsp = regs[REG_SP];
			newsp = 0;
			mask = (insn << 8 | unwindGetByte(&st)) & 0xfff;
			if (mask == 0)
				return UNWIND_FAILED;
			for (reg = 4; mask != 0; mask >>= 1, reg++) {
				if ((mask & 1) == 0)
					continue;
				ret = copyStack(pid, reg == REG_SP ? &newsp : &regs[reg],
						(const SceUInt32 *)vsp);
				vsp += 4;
				if (ret < 0)
					return UNWIND_FAILED;
			}
			regs[REG_SP] = newsp != 0 ? newsp : vsp;
		} else if ((insn & 0xf0) == 0x90 && (insn & 0xd) != 0xd) {
			regs[REG_SP] = regs[insn & 0xf];
		} else if ((insn & 0xf0) == 0xa0) {
			vsp = regs[REG_SP];
			for (reg = 4; reg <= (int)(insn & 7) + 4; reg++) {
				ret = copyStack(pid, &regs[reg], (const SceUInt32 *)vsp);
				vsp += 4;
				if (ret < 0)
					return UNWIND_FAILED;
			}
			if ((insn & 8) != 0) {
				ret = copyStack(pid, &regs[REG_LR], (const SceUInt32 *)vsp);
				if (ret < 0)
					return UNWIND_FAILED;
				vsp += 4;
			}
			regs[REG_SP] = vsp;
		} else if (insn == 0xb0) {
			regs[REG_PC] = regs[REG_LR];
			st.words = 0;
		} else if (insn == 0xb1) {
			mask = unwindGetByte(&st);
			vsp = regs[REG_SP];
			if (mask == 0 || (mask & 0xf0) != 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 302, " spare unwind instruction(0x%02X)\n", 0xb1);
				return UNWIND_FAILED;
			}
			for (reg = 0; mask != 0; mask >>= 1, reg++) {
				if ((mask & 1) == 0)
					continue;
				ret = copyStack(pid, &regs[reg], (const SceUInt32 *)vsp);
				vsp += 4;
				if (ret < 0)
					return UNWIND_FAILED;
			}
			regs[REG_SP] = vsp;
		} else if (insn == 0xb2) {
			uleb = 0;
			shift = 0;
			do {
				b = unwindGetByte(&st);
				if (b < 0)
					break;
				uleb |= (b & 0x7f) << shift;
				shift += 7;
			} while ((b & 0x80) != 0);
			regs[REG_SP] += 0x204 + uleb * 4;
		} else if (insn == 0xb3) {
			regs[REG_SP] += (unwindGetByte(&st) & 0xf) * 8 + 0xc;
		} else if ((insn & 0xfc) == 0xb4) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 329, " spare unwind instruction(0x%02X)\n", insn);
			return UNWIND_FAILED;
		} else if ((insn & 0xf8) == 0xb8) {
			regs[REG_SP] += (insn & 7) * 8 + 0xc;
		} else if (insn == 0xc7) {
			if (unwindGetByte(&st) == 0)
				SCE_DBG_PRINTF_LEVEL(1, 0, 340, " spare unwind instruction(0x%02X)\n",
						     insn);
			return UNWIND_FAILED;
		} else if (insn == 0xc8 || insn == 0xc9) {
			regs[REG_SP] += ((unwindGetByte(&st) & 0xf) + 1) * 8;
		} else {
			SCE_DBG_PRINTF_LEVEL(1, 0, 358, " unsupported unwind instruction(0x%02X)\n",
					     insn);
			return UNWIND_FAILED;
		}
	}
	if (regs[REG_PC] == 0)
		regs[REG_PC] = regs[REG_LR];
	return 0;
}

int SceBacktraceForDriver_7C878F90(SceUID pid, const SceKernelBacktraceFrameForKernel *frames,
				   SceSize count, SceUInt32 start)
{
	const SceKernelBacktraceFrameForKernel *f;
	SceUInt32 cpu = readMpidr() & 3, fingerprint, size;
	void *textBase;
	char name[0x1c];
	SceSize i;
	int ret;

	if (frames == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (start != 0) {
		for (i = 0; i != count; i++) {
			if (frames[i].pc == start) {
				frames += i;
				count -= i;
				break;
			}
		}
	}
	if (pid == 0)
		pid = sceKernelGetProcessId();
	SceDebugForDriver_391B74B7(pid == SCE_KERNEL_PROCESS_ID ?
				   "[%d]=== Print Backtrace(Kernel stack) ===\n" :
				   "[%d]=== Print Backtrace(User stack) =====\n", cpu);
	for (i = 0, f = frames; i != count; i++, f++) {
		fingerprint = 0;
		textBase = NULL;
		ret = SceModulemgrForKernel_5564A860(pid, f->pc, &fingerprint, &textBase, name);
		if (ret < 0) {
			size = i == count - 1 ? 0 : f[1].sp - f->sp;
			SceDebugForDriver_391B74B7(
				"[%d](%d) PC:0x%08X, SP:0x%08X(size=0x%08X byte) [no module info]\n",
				cpu, i, f->pc, f->sp, size);
		} else {
			if (pid == SCE_KERNEL_PROCESS_ID && !SceQafMgrForDriver_382C71E8())
				snprintf(name, 0x1b, "0x%08X", fingerprint);
			size = i == count - 1 ? 0 : f[1].sp - f->sp;
			SceDebugForDriver_391B74B7(
				"[%d](%d) PC:0x%08X, SP:0x%08X(size=0x%08X byte) [%s + 0x%08X]\n",
				cpu, i, f->pc, f->sp, size, name, f->pc - (SceUInt32)textBase);
		}
	}
	SceDebugForDriver_391B74B7("[%d]=====================================\n", cpu);
	return 0;
}

/* the registers of a thread or of the current exception */
static inline __attribute__((always_inline)) void BacktraceCopyException(SceUInt32 *regs, const SceExcpmgrContext *exc)
{
	int i;

	for (i = 0; i < 13; i++)
		regs[i] = exc->r[i];
	regs[REG_SP] = exc->sp;
	regs[REG_LR] = exc->lr;
	if (exc->excpCode == SCE_EXCP_PABT && (readIfsr() & 0x140f) != 2) {
		/* a prefetch abort: pc is not valid */
		regs[REG_PC] = regs[REG_LR];
		regs[REG_LR] = 0;
	} else {
		regs[REG_PC] = exc->pc;
	}
}

static inline __attribute__((always_inline)) void BacktraceCopyKernel(SceUInt32 *regs, const ThreadRegisterContext *k)
{
	int i;

	for (i = 0; i < 13; i++)
		regs[i] = k->r[i];
	regs[REG_SP] = k->sp;
	regs[REG_LR] = k->lr;
	regs[REG_PC] = k->full != 0 ? k->pc - 4 : 0;
}

static inline __attribute__((always_inline)) void BacktraceCopyUser(SceUInt32 *regs, const ThreadSyscallFrame *u)
{
	int i;

	for (i = 0; i < 13; i++)
		regs[i] = u->r[i];
	regs[REG_SP] = u->sp;
	regs[REG_LR] = u->lr;
	regs[REG_PC] = u->pc - 4;
}

int _sceKernelBacktraceCore(SceUID thid, SceKernelBacktraceFrame *frames, SceSize size,
			    SceSize *count, SceUInt32 flags, SceUInt32 *regs)
{
	const SceExcpmgrContext *exc;
	ThreadCB *ctx;
	struct ThreadmgrThreadObject *obj;
	ThreadSyscallFrame *u;
	SceUInt32 saved, pc, sp0, pc0, n, i, found;
	SceUID pid;
	int intr, inIntr, excp, refer, ret;

	saved = ModulemgrSyscallEnter();
	if (!SceSblACMgrForKernel_BBA13D9C()) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_PERMISSION;
		goto leave;
	}
	if (frames == NULL) {
		if (size != 0) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
			goto leave;
		}
	} else if (size < 8) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto leave;
	}
	if ((flags & ~BACKTRACE_FLAGS) != 0 ||
	    ((flags & BACKTRACE_FLAG_KERNEL_STACK) != 0 && !SceQafMgrForDriver_382C71E8())) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto leave;
	}

	intr = SceCpuForDriver_821FC0EE();
	ctx = getCurrentThreadCB();
	inIntr = SceIntrmgrForDriver_182EE3E3();
	excp = SceExcpmgrForKernel_B615A7DA();
	refer = 0;
	if (thid == 0) {
		if (inIntr)
			goto intr_context;
		thid = ctx->threadId;
		if (thid == BACKTRACE_THID_EXCEPTION)
			goto exception;
		if (thid == 0)
			goto current;
	} else if (thid == BACKTRACE_THID_EXCEPTION) {
		goto exception;
	}
	if (ctx != NULL && thid == ctx->threadId)
		goto current;

	/* another thread */
	if (sceKernelGetThreadmgrUIDClass(thid) != 1) {
		ret = SCE_KERNEL_ERROR_INVALID_UID;
		goto resume;
	}
	ret = sceGUIDReferObject(thid, (void **)&obj);
	if (ret < 0)
		goto resume;
	ctx = &obj->context;
	sceKernelSpinlockLowLock(&ctx->lock);
	refer = 1;
	if ((ctx->status & 1) != 0) {
		ret = SCE_KERNEL_ERROR_RUNNING;
		goto unlock;
	}
	if ((flags & BACKTRACE_FLAG_KERNEL_STACK) != 0 || ctx->userFrame != NULL)
		BacktraceCopyKernel(regs, ctx->pRegisterContext);
	goto locked;

exception:
	if (excp == 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto resume;
	}
	/* the frame of the current exception ends at the top of the CPU's stack */
	exc = (const SceExcpmgrContext *)SceExcpmgrForKernel_5420ED8F(sceKernelCpuId()) - 1;
	BacktraceCopyException(regs, exc);
	if (excp > 1) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto resume;
	}
	if (inIntr) {
		if ((flags & BACKTRACE_FLAG_KERNEL_STACK) != 0) {
			thid = BACKTRACE_THID_EXCEPTION;
			goto kernel;
		}
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto resume;
	}
	ctx = exc->thread;
	if (ctx == NULL)
		SCE_DBG_ASSERT_LEVEL(0, 0, 712);
	thid = BACKTRACE_THID_EXCEPTION;
	sceKernelSpinlockLowLock(&ctx->lock);
	goto locked;

current:
	if (excp != 0 || inIntr)
		goto intr_context;
	if (ctx == NULL)
		SCE_DBG_ASSERT_LEVEL(0, 0, 748);
	sceKernelSpinlockLowLock(&ctx->lock);
	goto locked;

intr_context:
	if ((flags & BACKTRACE_FLAG_KERNEL_STACK) == 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto resume;
	}
kernel:
	refer = 0;
	ctx = NULL;
	pid = SCE_KERNEL_PROCESS_ID;
	goto walk;

locked:
	if (ctx == NULL) {
		pid = SCE_KERNEL_PROCESS_ID;
		goto walk;
	}
	SCE_DBG_ASSERT_CHECK(ctx->lock != 0, 817);
	if ((ctx->status & 0x20) != 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto unlock;
	}
	if ((flags & BACKTRACE_FLAG_KERNEL_STACK) != 0) {
		if ((int)ctx->currentAttr >= 0 || ctx->userFrame != NULL) {
			pid = SCE_KERNEL_PROCESS_ID;
			goto walk;
		}
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto unlock;
	}
	if ((int)ctx->currentAttr >= 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto unlock;
	}
	u = ctx->userFrame;
	if (u != NULL)
		BacktraceCopyUser(regs, u);
	pid = ctx->processId;

walk:
	n = size >> 3;
	pc = regs[REG_PC];
	found = 0;
	if (n != 0) {
		frames[0].sp = regs[REG_SP];
		frames[0].pc = regs[REG_PC];
		found = 1;
	}
	for (i = 1; (flags & BACKTRACE_FLAG_LIMIT) == 0 || i < n; i++, frames++) {
		sp0 = regs[REG_SP];
		pc0 = regs[REG_PC];
		if (execUnwindInstruction(pid, regs, pc, flags) != 0)
			break;
		if (sp0 == regs[REG_SP] && pc0 == regs[REG_PC])
			break;
		pc = (regs[REG_PC] & 1) != 0 ? regs[REG_PC] - 2 : regs[REG_PC] - 4;
		if (i < n) {
			frames[1].sp = regs[REG_SP];
			found++;
			frames[1].pc = regs[REG_PC];
		}
	}
	if (count != NULL)
		*count = found;
	ret = (flags & BACKTRACE_FLAG_LIMIT) != 0 ? 0 : i;

unlock:
	if (ctx != NULL)
		sceKernelSpinlockLowUnlock(&ctx->lock);
	if (refer)
		sceGUIDReleaseObject(thid);
resume:
	sceKernelCpuResumeIntr(intr);
leave:
	ModulemgrSyscallLeave(saved);
	return ret;
}

int _sceKernelBacktraceForKernelCore(SceUID thid, SceKernelBacktraceFrameForKernel *frames,
				     SceSize size, SceSize *count, SceUInt32 flags, SceUInt32 *regs)
{
	const SceExcpmgrContext *exc;
	ThreadCB *ctx;
	struct ThreadmgrThreadObject *obj;
	ThreadSyscallFrame *u;
	SceUInt32 saved, pc, sp0, pc0, n, i, found, fingerprint;
	void *textBase;
	SceUID pid;
	int intr, inIntr, excp, refer, locked, ret;

	saved = ModulemgrSyscallEnter();
	if (frames == NULL) {
		if (size != 0) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
			goto leave;
		}
	} else if (size < 0x10) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto leave;
	}
	if ((flags & ~BACKTRACE_FLAGS) != 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto leave;
	}

	intr = SceCpuForDriver_821FC0EE();
	ctx = getCurrentThreadCB();
	inIntr = SceIntrmgrForDriver_182EE3E3();
	excp = SceExcpmgrForKernel_B615A7DA();
	refer = 0;
	locked = 0;
	if (thid == 0) {
		if (inIntr)
			goto intr_context;
		thid = ctx->threadId;
		if (thid == BACKTRACE_THID_EXCEPTION)
			goto exception;
		if (thid == 0)
			goto current;
	} else if (thid == BACKTRACE_THID_EXCEPTION) {
		goto exception;
	}
	if (ctx != NULL && thid == ctx->threadId)
		goto current;

	/* another thread */
	if (sceKernelGetThreadmgrUIDClass(thid) != 1) {
		ret = SCE_KERNEL_ERROR_INVALID_UID;
		goto resume;
	}
	ret = sceGUIDReferObject(thid, (void **)&obj);
	if (ret < 0)
		goto resume;
	ctx = &obj->context;
	locked = SceCpuForDriver_5AC9D394(&ctx->lock) >= 0;
	refer = 1;
	if ((ctx->status & 1) != 0) {
		ret = SCE_KERNEL_ERROR_RUNNING;
		goto unlock;
	}
	if ((flags & BACKTRACE_FLAG_KERNEL_STACK) != 0 || ctx->userFrame != NULL)
		BacktraceCopyKernel(regs, ctx->pRegisterContext);
	goto check;

exception:
	if (excp == 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto resume;
	}
	/* the frame of the current exception ends at the top of the CPU's stack */
	exc = (const SceExcpmgrContext *)SceExcpmgrForKernel_5420ED8F(sceKernelCpuId()) - 1;
	BacktraceCopyException(regs, exc);
	if (excp > 1) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto resume;
	}
	if (inIntr) {
		if ((flags & BACKTRACE_FLAG_KERNEL_STACK) != 0) {
			thid = BACKTRACE_THID_EXCEPTION;
			goto kernel;
		}
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto resume;
	}
	ctx = exc->thread;
	if (ctx == NULL)
		SCE_DBG_ASSERT_LEVEL(0, 0, 1135);
	thid = BACKTRACE_THID_EXCEPTION;
	locked = SceCpuForDriver_5AC9D394(&ctx->lock) >= 0;
	goto check;

current:
	if (excp != 0 || inIntr)
		goto intr_context;
	if (ctx == NULL)
		SCE_DBG_ASSERT_LEVEL(0, 0, 1173);
	locked = SceCpuForDriver_5AC9D394(&ctx->lock) >= 0;
	goto check;

intr_context:
	if ((flags & BACKTRACE_FLAG_KERNEL_STACK) == 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto resume;
	}
kernel:
	locked = 0;
	refer = 0;
	ctx = NULL;
	pid = SCE_KERNEL_PROCESS_ID;
	goto walk;

check:
	if (ctx == NULL) {
		pid = SCE_KERNEL_PROCESS_ID;
		goto walk;
	}
	if ((ctx->status & 0x20) != 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto unlock;
	}
	if ((flags & BACKTRACE_FLAG_KERNEL_STACK) != 0) {
		if ((int)ctx->currentAttr >= 0 || ctx->userFrame != NULL) {
			pid = SCE_KERNEL_PROCESS_ID;
			goto walk;
		}
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto unlock;
	}
	if ((int)ctx->currentAttr >= 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto unlock;
	}
	u = ctx->userFrame;
	if (u != NULL)
		BacktraceCopyUser(regs, u);
	pid = ctx->processId;

walk:
	pc = regs[REG_PC];
	n = size >> 4;
	if (n != 0) {
		fingerprint = 0;
		textBase = NULL;
		SceModulemgrForKernel_5564A860(pid, pc, &fingerprint, &textBase, NULL);
		frames[0].sp = regs[REG_SP];
		frames[0].pc = regs[REG_PC];
		frames[0].fingerprint = fingerprint;
		frames[0].textBase = (SceUInt32)textBase;
	}
	found = n != 0;
	for (i = 1; (flags & BACKTRACE_FLAG_LIMIT) == 0 || i < n; i++, frames++) {
		sp0 = regs[REG_SP];
		pc0 = regs[REG_PC];
		if (execUnwindInstruction(pid, regs, pc, flags) != 0)
			break;
		if (sp0 == regs[REG_SP] && pc0 == regs[REG_PC])
			break;
		pc = (regs[REG_PC] & 1) != 0 ? regs[REG_PC] - 2 : regs[REG_PC] - 4;
		if (i < n) {
			fingerprint = 0;
			textBase = NULL;
			SceModulemgrForKernel_5564A860(pid, regs[REG_PC], &fingerprint, &textBase,
							 NULL);
			frames[1].sp = regs[REG_SP];
			frames[1].pc = regs[REG_PC];
			frames[1].fingerprint = fingerprint;
			frames[1].textBase = (SceUInt32)textBase;
			found++;
		}
	}
	if (count != NULL)
		*count = found;
	ret = (flags & BACKTRACE_FLAG_LIMIT) != 0 ? 0 : i;

unlock:
	if (locked)
		sceKernelSpinlockLowUnlock(&ctx->lock);
	if (refer)
		sceGUIDReleaseObject(thid);
resume:
	sceKernelCpuResumeIntr(intr);
leave:
	ModulemgrSyscallLeave(saved);
	return ret;
}

int BacktraceInit(void)
{
	SceDebugForKernel_91E7D24F(SceBacktraceForDriver_888E99B8);
	SceDebugForKernel_38F1A352(SceBacktraceForDriver_7C878F90);
	return 0;
}

/* name unknown */
int sceKernelBacktraceForUser(SceUID thid, SceKernelBacktraceFrame *ubuf, SceSize size,
				     SceSize *ucount, SceUInt32 flags)
{
	SceKernelBacktraceFrame *kbuf;
	SceSize count = size;
	int ret;

	if (thid != 0) {
		thid = SceSysmemForDriver_45D22597(0, thid);
		if (thid < 0)
			return thid;
	}
	if (ubuf != NULL) {
		kbuf = SceSysmemForKernel_85571907(size);
		if (kbuf == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
	} else {
		kbuf = NULL;
	}
	ret = SceBacktraceForDriver_166B9C8C(thid, kbuf, size, &count, flags);
	if (ret >= 0) {
		if (ubuf != NULL) {
			ret = sceKernelCopyToUser(ubuf, kbuf, count * sizeof(*kbuf));
			if (ret < 0)
				goto out;
		}
		if (ucount != NULL)
			ret = sceKernelCopyToUser(ucount, &count, sizeof(count));
	}
out:
	SceSysmemForKernel_4233C16D(kbuf);
	return ret;
}

/* the syscall: the arguments past the third come in a user block */
typedef struct SceKernelBacktraceParam {
	SceSize *count;
	SceUInt32 flags;
	SceUInt32 reserved[2];
} SceKernelBacktraceParam;

int _sceKernelBacktrace(SceUID thid, SceKernelBacktraceFrame *buf, SceSize size,
			const SceKernelBacktraceParam *param)
{
	SceKernelBacktraceParam p;
	int ret;

	ret = sceKernelCopyFromUser(&p, param, sizeof(p));
	if (ret < 0)
		return ret;
	return sceKernelBacktraceForUser(thid, buf, size, p.count, p.flags);
}
