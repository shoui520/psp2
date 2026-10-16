/*
 * usleep.c: sceKernelUsleep for the secure world, on word timer 7. A core
 * that sleeps takes its own lock and then waits on it; the timer interrupt
 * (0x87, routed to the core that wakes first) releases each core whose wake
 * time has passed and sets the compare register to the next one.
 */

#include "intrmgr.h"
#include <psp2kern/kernel/cpu.h>
#include <sce/timer_regs.h>

#define USLEEP_TIMER		7
#define USLEEP_INTR		0x87
#define USLEEP_MAX		1000000

/* 1 MHz count: prescaler 0xdd */
#define USLEEP_TIMER_CONTROL	0xdd00000d

/* names unknown */
typedef struct UsleepState {
	volatile SceUInt32 waitingCpus;		/* bit n: core n sleeps */
	volatile SceUInt32 targetCpu;		/* the interrupt's core: the first to wake */
	volatile SceUInt32 wakeTime[4];		/* per core, in timer counts */
	volatile SceKernelSpinlock lock;
} UsleepState;

/* the lock as the spinlock functions take it */
#define USLEEP_LOCK	((SceKernelSpinlock *)&s_usleep.lock)

static UsleepState s_usleep;
static SceKernelSpinlock s_cpuWait[4];		/* held while the core sleeps */
static volatile SceWordTimerRegisters *s_timer;

static int SetTargetCPU(int cpu)
{
	int ret = sceKernelSetIntrTargetCpu(USLEEP_INTR, 1 << cpu);

	if (ret < 0) {
		SCE_DBG_PRINTF(6, 64, "set target cpu failed. 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(65);
	}
	return ret;
}

static int UsleepIntrHandler(int intrCode, void *userCtx)
{
	SceUInt32 control, now, next = 0;
	int cpu;

	ksceKernelSpinlockLowLock(USLEEP_LOCK);
	control = s_timer->control;
	s_timer->control = 0;
	s_timer->intr = TIMER_INTR_CLEAR;
	now = s_timer->count;
	for (cpu = 0; cpu < 4; cpu++) {
		if (!((s_usleep.waitingCpus >> cpu) & 1))
			continue;
		if (now > s_usleep.wakeTime[cpu]) {
			s_usleep.waitingCpus &= ~(1 << cpu);
			s_usleep.wakeTime[cpu] = 0;
			ksceKernelSpinlockLowUnlock(&s_cpuWait[cpu]);
		} else if (next == 0 || s_usleep.wakeTime[cpu] < next) {
			next = s_usleep.wakeTime[cpu];
			s_usleep.targetCpu = cpu;
		}
	}
	if (s_usleep.waitingCpus) {
		SetTargetCPU(s_usleep.targetCpu);
		s_timer->compare = next;
		s_timer->control = control;
	}
	ksceKernelSpinlockLowUnlock(USLEEP_LOCK);
	return -1;
}

void sceKernelUsleep(SceUInt32 usec)
{
	int cpu = SceCpuForTZS_44C423D3();
	SceKernelIntrStatus intr;
	SceUInt32 wake;

	if (IllegalContext())
		SCE_DBG_ASSERT_FAIL(127);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(USLEEP_LOCK);
	if (usec >= USLEEP_MAX)
		usec = USLEEP_MAX;
	s_cpuWait[cpu] = 0;
	ksceKernelSpinlockLowLock(&s_cpuWait[cpu]);
	if (s_usleep.waitingCpus) {
		wake = usec + s_timer->count;
		s_usleep.wakeTime[cpu] = wake;
		if (wake < s_usleep.wakeTime[s_usleep.targetCpu]) {
			SetTargetCPU(cpu);
			s_usleep.targetCpu = cpu;
			s_timer->compare = wake;
		}
	} else {
		SetTargetCPU(cpu);
		s_usleep.targetCpu = cpu;
		s_usleep.wakeTime[cpu] = usec;
		s_timer->compare = usec;
		s_timer->count = 0;
		s_timer->control = USLEEP_TIMER_CONTROL;
	}
	s_usleep.waitingCpus |= 1 << cpu;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(USLEEP_LOCK, intr);
	/* until UsleepIntrHandler releases it */
	ksceKernelSpinlockLowLock(&s_cpuWait[cpu]);
}

int InitUsleepTimer(int remap)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SceUID memBlockId;
	void *base;
	int ret;

	if (SceCpuForTZS_44C423D3() != 0)
		return 0;
	if (remap) {
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
		opt.paddr = TIMER_WORD_REG_BASE(USLEEP_TIMER);
		memBlockId = ksceKernelAllocMemBlock("SceTimerForUsleep",
						     SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW,
						     TIMER_REG_SIZE, &opt);
		if (memBlockId < 0) {
			SCE_DBG_PRINTF(0xb, 205, "remap Word Timer7 failed. 0x%08x\n", memBlockId);
			SCE_DBG_ASSERT_FAIL(206);
		}
		ksceKernelGetMemBlockBase(memBlockId, &base);
		s_timer = base;
	} else {
		s_timer = (void *)TIMER_WORD_REG_BASE(USLEEP_TIMER);
	}
	s_usleep.lock = 0;
	s_usleep.waitingCpus = 0;
	s_usleep.wakeTime[0] = 0;
	s_usleep.wakeTime[1] = 0;
	s_usleep.wakeTime[2] = 0;
	s_usleep.wakeTime[3] = 0;
	ret = sceKernelRegisterIntrHandler(USLEEP_INTR, "SceTimerForUsleep", 0, UsleepIntrHandler,
					   NULL, 0x80, 0xf, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF(6, 230, "register usleep handler failed. 0x%08x\n", ret);
		return ret;
	}
	sceKernelEnableIntr(USLEEP_INTR);
	return 0;
}
