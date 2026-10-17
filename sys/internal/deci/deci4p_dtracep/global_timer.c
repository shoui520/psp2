/*
 * global_timer.c: DTrace's time base, the Cortex-A9 global timer in the
 * private region. module_start maps it and starts it from zero; across a
 * suspend the count is saved and the timer restarted from it on resume,
 * and reads in between return the saved count.
 *
 * Internal names are unknown (no debug records name them).
 */
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/sysclib.h>
#include "fw.h"
#include <sysmem/debug.h>
#include <intrmgr/gic_regs.h>

/* the error messages are new in 4.00 */
#if FW_SINCE(0x04000000)
#define GLOBAL_TIMER_ERROR(_fmt, _ret, _line)					\
	ksceKernelPrintf("[dtrace] ERROR:   " _fmt " (0x%08x) [%s:%d]\n",		\
			 (_ret), "global_timer.c", (_line))
#else
#define GLOBAL_TIMER_ERROR(_fmt, _ret, _line)	((void)(_ret))
#endif

static struct {
	void *base;			/* the private region */
	GlobalTimer *timer;
	SceUID uid;
} s_globalTimer = { NULL, NULL, -1 };

static SceUInt64 s_suspendedCount;
static int s_suspended;

static inline SceUInt64 GlobalTimerRead(GlobalTimer *timer)
{
	SceUInt32 high = timer->counterHigh;

	return (SceUInt64)high << 32 | timer->counterLow;
}

SceUInt64 GlobalTimerGetCount(void)
{
	if (s_suspended == 1)
		return s_suspendedCount;
	return GlobalTimerRead(s_globalTimer.timer);
}

/* map the private region and start the timer at count */
static inline void GlobalTimerStart(SceUInt64 count)
{
	SceKernelAllocMemBlockKernelOpt opt;
	GlobalTimer *timer;
	void *base;
	SceUID uid;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = PERIPH_REG_BASE;
	uid = ksceKernelAllocMemBlock("ScePeriphReg", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW,
				      PERIPH_REG_SIZE, &opt);
	if (uid < 0) {
		GLOBAL_TIMER_ERROR("failed to allocate global timer memory block", uid, 48);
		return;
	}
	s_globalTimer.uid = uid;
	ksceKernelGetMemBlockBase(uid, &base);
	s_globalTimer.base = base;
	timer = (GlobalTimer *)((char *)base + GLOBAL_TIMER_OFFSET);
	s_globalTimer.timer = timer;
	(void)timer->control;		/* Sony reads the control register first */
	timer->counterLow = (SceUInt32)count;
	timer->counterHigh = (SceUInt32)(count >> 32);
	timer->control = GLOBAL_TIMER_CONTROL_ENABLE;
}

static inline void GlobalTimerStop(GlobalTimer *timer)
{
	(void)timer->control;
	timer->control = 0;
}

static inline void GlobalTimerUnmap(void)
{
	int ret;

	ret = ksceKernelFreeMemBlock(s_globalTimer.uid);
	if (ret != 0)
		GLOBAL_TIMER_ERROR("failed to free global timer memory block", ret, 90);
}

static inline void GlobalTimerForget(void)
{
	s_globalTimer.base = NULL;
	s_globalTimer.timer = NULL;
	s_globalTimer.uid = -1;
}

int GlobalTimerInit(void)
{
	GlobalTimerStart(0);
	return 0;
}

int GlobalTimerFini(void)
{
	if (s_globalTimer.uid != -1) {
		GlobalTimerStop(s_globalTimer.timer);
		GlobalTimerUnmap();
	}
	GlobalTimerForget();
	return 0;
}

int GlobalTimerSuspend(void)
{
	GlobalTimer *timer;

	if (s_globalTimer.uid != -1) {
		timer = s_globalTimer.timer;
		GlobalTimerStop(timer);
		s_suspendedCount = s_suspended == 1 ? s_suspendedCount : GlobalTimerRead(timer);
		GlobalTimerUnmap();
	}
	GlobalTimerForget();
	s_suspended = 1;
	return 0;
}

int GlobalTimerResume(void)
{
	GlobalTimerStart(s_suspendedCount);
	s_suspended = 0;
	return 0;
}
