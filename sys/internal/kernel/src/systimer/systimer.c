/*
 * SceSystimer: the SoC's general purpose timers.
 *
 * Two banks (systimer/timer_regs.h): long-range timers LT0-4 (64-bit count) and
 * word timers WT0-6 (32-bit count). A 64-bit software base is added to the
 * hardware count; it absorbs SetCount and the interval of periodic timers.
 */

#include <sysmem/cpu.h>
#include <sysmem/sysclib.h>
#include <sysmem/debug.h>
#include <sysmem/compiler.h>
#include <kernel/error.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/sysmem.h>
#include <systimer/systimer.h>
#include <systimer/timer_regs.h>

#define LONG_TIMERS		5
#define WORD_TIMERS		7
#define LONG_INTR_BASE		0x88
#define WORD_INTR_BASE		0x80

/* TimerInfo.flags */
#define TIMER_ALLOCATED		0x1
#define TIMER_COUNTING		0x2

/* control register */
#define CTRL_START_ONESHOT_LONG	0x01
#define CTRL_START_ONESHOT_WORD	0x03
#define CTRL_START_PERIODIC	0x1d

typedef struct TimerInfo {
	union {
		SceSysTimerId id;		/* (type << 16) | index */
		struct {
			SceUInt16 index;
			SceInt16 type;
		};
	};
	SceUInt32 flags;
	SceSysTimerCallback handler;
	void *arg;
	union {
		SceUInt64 base;			/* added to the hardware count */
		struct {
			SceUInt32 baseLo;
			SceUInt32 baseHi;
		};
	};
	SceUInt64 interval;			/* 0: one-shot */
	SceKernelSpinlock lock;
	int intrCode;
	union {
		SceLongTimerRegisters *longReg;
		SceWordTimerRegisters *wordReg;
		void *reg;
	};
} __attribute__((aligned(64))) TimerInfo;

static TimerInfo s_wordTimer[WORD_TIMERS];
static TimerInfo s_longTimer[LONG_TIMERS];

static inline int TimerType(const TimerInfo *info)
{
	return info->type;
}

static inline TimerInfo *GetTimerInfo(SceSysTimerId id)
{
	if ((id >> 16) == SCE_SYSTIMER_TYPE_LONG)
		return &s_longTimer[id & 0xffff];
	return &s_wordTimer[id & 0xffff];
}

/* name unknown: interrupt handler of every timer */
static int TimerIntrHandler(int intrCode, void *userCtx)
{
	TimerInfo *info = userCtx;
	SceSysTimerId id = info->id;
	int type = id >> 16;

	if (type == SCE_SYSTIMER_TYPE_LONG)
		info->longReg->intr = TIMER_INTR_CLEAR;
	else
		info->wordReg->intr = TIMER_INTR_CLEAR;

	if (info->interval != 0) {
		info->base += info->interval;
		if (info->handler != NULL)
			info->handler(id, info->arg);
	} else if (type == SCE_SYSTIMER_TYPE_WORD) {
		/* one-shot word timer wrapped: carry into the base */
		info->baseHi++;
	}
	return -1;
}

int DelTimerInfo(TimerInfo *info)
{
	int ret;

	sceKernelDisableIntr(info->intrCode);
	ret = sceKernelReleaseIntrHandler(info->intrCode);
	if (ret < 0) {
		SCE_DBG_PRINTF(6, 809, "release systimer handler failed. 0x%08x\n", ret);
		return ret;
	}
	return 0;
}

int InitTimerInfo(TimerInfo *info)
{
	char name[32];
	unsigned int index;
	int ret;

	info->flags = 0;
	info->base = 0;
	info->handler = NULL;
	info->arg = NULL;
	info->lock = 0;

	if (TimerType(info) == SCE_SYSTIMER_TYPE_LONG) {
		info->longReg->control = 0;
		info->longReg->count = 0;
		info->longReg->compare = 0;
		info->longReg->value = 0;
		info->longReg->intr = TIMER_INTR_CLEAR;
	} else {
		info->wordReg->control = 0;
		info->wordReg->count = 0;
		info->wordReg->compare = 0;
		info->wordReg->value = 0;
		info->wordReg->intr = TIMER_INTR_CLEAR;
	}

	index = info->intrCode - WORD_INTR_BASE;
	if (index < WORD_TIMERS) {
		snprintf(name, sizeof(name), "SceSystimerWordTimer%d", index);
	} else {
		index = info->intrCode - LONG_INTR_BASE;
		snprintf(name, sizeof(name), "SceSystimerLongrangeTimer%d", index);
	}

	ret = sceKernelRegisterIntrHandler(info->intrCode, name, 0, TimerIntrHandler,
					    info, 0x80, 0xf, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF(6, 794, "register systimer handler failed. 0x%08x\n", ret);
		return ret;
	}
	return 0;
}

SceSysTimerId SceSystimerForDriver_E2B9D8E9(int type)
{
	TimerInfo *table, *info;
	unsigned int i, count;
	int state;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;

	switch (type) {
	case SCE_SYSTIMER_TYPE_LONG:
		table = s_longTimer;
		count = LONG_TIMERS;
		break;
	case SCE_SYSTIMER_TYPE_WORD:
		table = s_wordTimer;
		count = WORD_TIMERS;
		break;
	default:
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}

	for (i = 0; i < count; i++) {
		info = &table[i];
		state = SceCpuForDriver_D32ACE9E(&info->lock);
		if (!(info->flags & TIMER_ALLOCATED)) {
			info->flags |= TIMER_ALLOCATED;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
			break;
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	}
	if (i >= count)
		return SCE_KERNEL_ERROR_NO_FREE_TIMER;

	sceKernelEnableIntr(info->intrCode);
	sceKernelSetIntrTargetCpu(info->intrCode, 0xf);
	return info->id;
}

int SceSystimerForDriver_8358611F(SceSysTimerId id)
{
	TimerInfo *info;
	int state;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;

	info = GetTimerInfo(id);
	sceKernelDisableIntr(info->intrCode);

	state = SceCpuForDriver_D32ACE9E(&info->lock);
	if (!(info->flags & TIMER_ALLOCATED)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_NOT_ALLOCATED;
	}

	if (TimerType(info) == SCE_SYSTIMER_TYPE_LONG) {
		info->longReg->control = 0;
		info->longReg->count = 0;
		info->longReg->compare = 0;
		info->longReg->value = 0;
		info->longReg->intr = TIMER_INTR_CLEAR;
	} else {
		info->wordReg->control = 0;
		info->wordReg->count = 0;
		info->wordReg->compare = 0;
		info->wordReg->value = 0;
		info->wordReg->intr = TIMER_INTR_CLEAR;
	}
	info->flags = 0;
	info->handler = NULL;
	info->arg = NULL;
	info->base = 0;
	info->interval = 0;

	sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

int SceSystimerForDriver_0450933B(SceSysTimerId id, SceUInt32 source, SceUInt32 prescale)
{
	TimerInfo *info;
	SceUInt32 sel;
	int state;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (source == 2 || source == 14 || source == 15 || source > 21)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

	info = GetTimerInfo(id);
	state = SceCpuForDriver_D32ACE9E(&info->lock);
	if (unlikely(!(info->flags & TIMER_ALLOCATED))) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_NOT_ALLOCATED;
	}
	if (info->flags & TIMER_COUNTING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_COUNTING;
	}

	if (source < 16)
		sel = source << 20;
	else
		sel = ((source & 7) << 8) | 0x200000;
	if (TimerType(info) == SCE_SYSTIMER_TYPE_LONG)
		info->longReg->control = ((info->longReg->control & 0xff8ff) | prescale << 24) | sel;
	else
		info->wordReg->control = ((info->wordReg->control & 0xff8ff) | prescale << 24) | sel;

	sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

int SceSystimerForDriver_A73351C1(SceSysTimerId id, SceUInt64 interval)
{
	TimerInfo *info;
	int state;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;

	info = GetTimerInfo(id);
	state = SceCpuForDriver_D32ACE9E(&info->lock);
	if (!(info->flags & TIMER_ALLOCATED)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_NOT_ALLOCATED;
	}
	if (info->flags & TIMER_COUNTING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_COUNTING;
	}

	if (TimerType(info) == SCE_SYSTIMER_TYPE_LONG)
		info->longReg->compare = interval;
	else
		info->wordReg->compare = (SceUInt32)interval;
	info->interval = interval;

	sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

int SceSystimerForDriver_CCCD1EE3(SceSysTimerId id, SceSysTimerCallback handler,
				 SceUInt32 targetCpu, void *arg)
{
	TimerInfo *info;
	int state;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;

	/* a CPU mask in bits 0-3, or in bits 16-19 */
	if (targetCpu == 0)
		return SCE_KERNEL_ERROR_ILLEGAL_TARGET_CPU;
	if (targetCpu & 0xf0000) {
		if (targetCpu & 0xf)
			return SCE_KERNEL_ERROR_ILLEGAL_TARGET_CPU;
		targetCpu >>= 16;
	} else if (!(targetCpu & 0xf)) {
		targetCpu >>= 16;
	} else if ((int)targetCpu < 0) {
		return SCE_KERNEL_ERROR_ILLEGAL_TARGET_CPU;
	}

	info = GetTimerInfo(id);
	state = SceCpuForDriver_D32ACE9E(&info->lock);
	if (!(info->flags & TIMER_ALLOCATED)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_NOT_ALLOCATED;
	}
	if (info->flags & TIMER_COUNTING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_COUNTING;
	}

	info->handler = handler;
	if (handler == NULL) {
		info->arg = NULL;
	} else {
		info->arg = arg;
		sceKernelSetIntrTargetCpu(info->intrCode, targetCpu);
	}

	sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

int SceSystimerForDriver_59EBD22D(SceSysTimerId id)
{
	TimerInfo *info = GetTimerInfo(id);
	int state;

	state = SceCpuForDriver_D32ACE9E(&info->lock);
	if (unlikely(!(info->flags & TIMER_ALLOCATED))) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_NOT_ALLOCATED;
	}
	if (info->flags & TIMER_COUNTING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_COUNTING;
	}

	info->flags |= TIMER_COUNTING;
	if (TimerType(info) == SCE_SYSTIMER_TYPE_LONG) {
		if (info->interval != 0)
			info->longReg->control |= CTRL_START_PERIODIC;
		else
			info->longReg->control |= CTRL_START_ONESHOT_LONG;
	} else {
		if (info->interval != 0)
			info->wordReg->control |= CTRL_START_PERIODIC;
		else
			info->wordReg->control |= CTRL_START_ONESHOT_WORD;
	}

	sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

int SceSystimerForDriver_BF8D42B0(SceSysTimerId id)
{
	TimerInfo *info = GetTimerInfo(id);
	int state;

	state = SceCpuForDriver_D32ACE9E(&info->lock);
	if (!(info->flags & TIMER_COUNTING)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_STOPPED;
	}

	if (TimerType(info) == SCE_SYSTIMER_TYPE_LONG)
		info->longReg->control &= ~TIMER_CONTROL_ENABLE;
	else
		info->wordReg->control &= ~TIMER_CONTROL_ENABLE;
	info->flags &= ~TIMER_COUNTING;

	sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

int SceSystimerForDriver_7DEBA6A5(SceSysTimerId id)
{
	TimerInfo *info;
	int state;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;

	info = GetTimerInfo(id);
	state = SceCpuForDriver_D32ACE9E(&info->lock);
	if (!(info->flags & TIMER_ALLOCATED)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_NOT_ALLOCATED;
	}

	/* a running timer is stopped around the count write */
	if (info->flags & TIMER_COUNTING) {
		SceLongTimerRegisters *longReg = info->longReg;
		SceWordTimerRegisters *wordReg = info->wordReg;

		if (TimerType(info) == SCE_SYSTIMER_TYPE_LONG) {
			longReg->control &= ~TIMER_CONTROL_ENABLE;
			longReg->count = 0;
			info->base = 0;
			if (info->interval != 0)
				longReg->control |= CTRL_START_PERIODIC;
			else
				longReg->control |= CTRL_START_ONESHOT_LONG;
		} else {
			wordReg->control &= ~TIMER_CONTROL_ENABLE;
			wordReg->count = 0;
			info->base = 0;
			if (info->interval != 0)
				wordReg->control |= CTRL_START_PERIODIC;
			else
				wordReg->control |= CTRL_START_ONESHOT_WORD;
		}
	} else {
		if (TimerType(info) == SCE_SYSTIMER_TYPE_LONG)
			info->longReg->count = 0;
		else
			info->wordReg->count = 0;
		info->base = 0;
	}

	sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

int SceSystimerForDriver_1B14DFD6(SceSysTimerId id, SceUInt64 count)
{
	TimerInfo *info;
	int state;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;

	info = GetTimerInfo(id);
	state = SceCpuForDriver_D32ACE9E(&info->lock);
	if (!(info->flags & TIMER_ALLOCATED)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_NOT_ALLOCATED;
	}

	/* a running timer is stopped around the count write */
	if (info->flags & TIMER_COUNTING) {
		if (TimerType(info) == SCE_SYSTIMER_TYPE_LONG) {
			info->longReg->control &= ~TIMER_CONTROL_ENABLE;
			info->longReg->count = count;
			info->base += count;
			if (info->interval != 0)
				info->longReg->control |= CTRL_START_PERIODIC;
			else
				info->longReg->control |= CTRL_START_ONESHOT_LONG;
		} else {
			info->wordReg->control &= ~TIMER_CONTROL_ENABLE;
			info->wordReg->count = (SceUInt32)count;
			info->base += count;
			if (info->interval != 0)
				info->wordReg->control |= CTRL_START_PERIODIC;
			else
				info->wordReg->control |= CTRL_START_ONESHOT_WORD;
		}
	} else {
		if (TimerType(info) == SCE_SYSTIMER_TYPE_LONG)
			info->longReg->count = count;
		else
			info->wordReg->count = (SceUInt32)count;
		info->base += count;
	}

	sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

int SceSystimerForDriver_963F7C0D(SceSysTimerId id, SceUInt64 *count)
{
	TimerInfo *info = GetTimerInfo(id);
	volatile SceLongTimerRegisters *reg;
	SceUInt32 hi, lo, hi2;

	if (!(info->flags & TIMER_ALLOCATED))
		return SCE_KERNEL_ERROR_TIMER_NOT_ALLOCATED;

	if (TimerType(info) == SCE_SYSTIMER_TYPE_LONG) {
		/* re-read the high word until it is stable around the low word */
		reg = info->longReg;
		do {
			hi = reg->countHi;
			lo = reg->countLo;
			hi2 = reg->countHi;
		} while (hi != hi2);
		*count = (SceUInt64)hi << 32 | lo;
	} else {
		*count = info->wordReg->count;
	}
	return 0;
}

/* name unknown: count plus software base */
int SceSystimerForDriver_85AAE0DD(SceSysTimerId id, SceUInt64 *count)
{
	TimerInfo *info = GetTimerInfo(id);
	SceUInt32 hi, lo, hi2;

	if (!(info->flags & TIMER_ALLOCATED))
		return SCE_KERNEL_ERROR_TIMER_NOT_ALLOCATED;

	if (TimerType(info) == SCE_SYSTIMER_TYPE_LONG) {
		do {
			hi = info->longReg->countHi;
			lo = info->longReg->countLo;
			hi2 = info->longReg->countHi;
		} while (hi != hi2);
		*count = (SceUInt64)hi << 32 | lo;
	} else {
		SceUInt64 value = info->wordReg->count;
		*count = value;
	}
	*count += info->base;
	return 0;
}

/* name unknown: control register field at bits 16-19 (a < 14), bit 7 (b), sets bit 6 */
int SceSystimerForDriver_904DF737(SceSysTimerId id, SceUInt32 a, SceUInt32 b)
{
	TimerInfo *info;
	int state;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (a >= 14)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

	info = GetTimerInfo(id);
	state = SceCpuForDriver_D32ACE9E(&info->lock);
	if (unlikely(!(info->flags & TIMER_ALLOCATED))) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_NOT_ALLOCATED;
	}
	if (info->flags & TIMER_COUNTING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_COUNTING;
	}

	if (TimerType(info) == SCE_SYSTIMER_TYPE_LONG)
		info->longReg->control = (info->longReg->control & 0xfff0ff7f) | b << 7 | 0x40 | a << 16;
	else
		info->wordReg->control = (info->wordReg->control & 0xfff0ff7f) | b << 7 | 0x40 | a << 16;

	sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

/* name unknown: control register field at bits 12-15 (a < 14), sets bit 5 */
int SceSystimerForDriver_C34D7940(SceSysTimerId id, SceUInt32 a)
{
	TimerInfo *info;
	int state;

	if (SceIntrmgrForDriver_182EE3E3() != 0)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (a >= 14)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

	info = GetTimerInfo(id);
	state = SceCpuForDriver_D32ACE9E(&info->lock);
	if (unlikely(!(info->flags & TIMER_ALLOCATED))) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_NOT_ALLOCATED;
	}
	if (info->flags & TIMER_COUNTING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_TIMER_COUNTING;
	}

	if (TimerType(info) == SCE_SYSTIMER_TYPE_LONG)
		info->longReg->control = (info->longReg->control & 0xffff0fff) | a << 12 | 0x20;
	else
		info->wordReg->control = (info->wordReg->control & 0xffff0fff) | a << 12 | 0x20;

	sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

/* name unknown: reads the value register (pair) */
int SceSystimerForDriver_FD5F1742(SceSysTimerId id, SceUInt64 *value)
{
	TimerInfo *info = GetTimerInfo(id);
	SceUInt32 hi, lo, hi2;

	if (!(info->flags & TIMER_ALLOCATED))
		return SCE_KERNEL_ERROR_TIMER_NOT_ALLOCATED;

	if (TimerType(info) == SCE_SYSTIMER_TYPE_LONG) {
		do {
			hi = info->longReg->valueHi;
			lo = info->longReg->valueLo;
			hi2 = info->longReg->valueHi;
		} while (hi != hi2);
		*value = (SceUInt64)hi << 32 | lo;
	} else {
		*value = info->wordReg->value;
	}
	return 0;
}

int StartSystimer(SceSize args, void *argp)
{
	SceKernelAllocMemBlockKernelOpt opt = {
		.size = sizeof(opt),
		.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR,
		.paddr = TIMER_LONG_REG_BASE(0),
	};
	SceUID uid;
	void *base;
	SceUInt32 i;

	uid = SceSysmemForDriver_C94850C9("SceLongRangeTimerReg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW,
				      LONG_TIMERS * TIMER_REG_SIZE, &opt);
	if (uid < 0) {
		SCE_DBG_PRINTF(0xb, 836, "allocate systimer register area failed. 0x%08x\n", uid);
		SCE_DBG_ASSERT_FAIL(837);
	}
	SceSysmemForDriver_A841EDDA(uid, &base);
	for (i = 0; i < LONG_TIMERS; i++) {
		s_longTimer[i].id = i | SCE_SYSTIMER_TYPE_LONG << 16;
		s_longTimer[i].intrCode = i + LONG_INTR_BASE;
		s_longTimer[i].reg = (char *)base + i * TIMER_REG_SIZE;
		if (InitTimerInfo(&s_longTimer[i]) < 0) {
			while (i != 0)
				DelTimerInfo(&s_longTimer[--i]);
			return 0;
		}
	}

	opt.paddr = TIMER_WORD_REG_BASE(0);
	uid = SceSysmemForDriver_C94850C9("SceWordTimerReg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW,
				      WORD_TIMERS * TIMER_REG_SIZE, &opt);
	if (uid < 0) {
		SCE_DBG_PRINTF(0xb, 865, "allocate systimer register area failed. 0x%08x\n", uid);
		SCE_DBG_ASSERT_FAIL(866);
	}
	SceSysmemForDriver_A841EDDA(uid, &base);
	for (i = 0; i < WORD_TIMERS; i++) {
		s_wordTimer[i].id = i | SCE_SYSTIMER_TYPE_WORD << 16;
		s_wordTimer[i].intrCode = i + WORD_INTR_BASE;
		s_wordTimer[i].reg = (char *)base + i * TIMER_REG_SIZE;
		if (InitTimerInfo(&s_wordTimer[i]) < 0) {
			while (i != 0)
				DelTimerInfo(&s_wordTimer[--i]);
			for (i = 0; i < LONG_TIMERS; i++)
				DelTimerInfo(&s_longTimer[i]);
			return 0;
		}
	}
	return 0;
}
