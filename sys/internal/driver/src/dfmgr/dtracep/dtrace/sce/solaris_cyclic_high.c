/*
 * High-level cyclics (Sony's dtrace/sce/solaris_cyclic_high.c): a system timer
 * calls the handler from its interrupt, on the cyclic's CPUs. Each cyclic with a
 * system timer is listed, so the timers can be given back across a suspend and
 * set up again on resume. Memory a handler may still use is freed later, when
 * the next high-level cyclic is made.
 */
#include <dtrace/logging.h>
#include <dtrace/heap.h>
#include <dtrace/sce/solaris_cyclic.h>
#include <kernel/error.h>
#include <sceerror.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <systimer/systimer.h>

/* any system timer type, the smallest the interval fits */
#define CYCLIC_HIGH_TIMER_ANY	(-1)

/* the system timers count at 48 MHz: 6/125 cycles a nanosecond */
#define CYCLIC_HIGH_NSEC_TO_CYCLES(ns)	((ns) * 6 / 125)

/* a cyclic with a system timer */
typedef struct CyclicInfo {
	cyclic_id_t cyclic;
	int type;			/* its system timer's type */
	SceUInt64 count;		/* the timer's count, across a suspend */
	struct CyclicInfo *next;
} CyclicInfo;

/* memory to free once no handler runs */
typedef struct FreeLater {
	void *ptr;
	struct FreeLater *next;
} FreeLater;

static CyclicInfo *s_cyclicInfo;
static FreeLater *s_freeLater;
static SceKernelSpinlock s_freeLaterLock;

static int _cyclic_high_handler(SceSysTimerId timer, void *arg)
{
	cyclic_id_t id = arg;

	if (id != NULL && id->cy_handler.cyh_func != NULL)
		id->cy_handler.cyh_func(id->cy_handler.cyh_arg);

	return 0;
}

void _dtrace_heap_free_later(void *ptr)
{
	SceKernelIntrStatus status;
	FreeLater *entry, *next;

	status = sceKernelSpinlockLowLockCpuSuspendIntr(&s_freeLaterLock);
	if (s_freeLater == NULL) {
		s_freeLater = dtrace_heap_alloc(sizeof(*s_freeLater));
		if (s_freeLater == NULL)
			SCE_PANIC_AT(55);
		s_freeLater->ptr = NULL;
		s_freeLater->next = s_freeLater;
	}

	entry = dtrace_heap_alloc(sizeof(*entry));
	if (s_freeLater == NULL)
		SCE_PANIC_AT(63);
	entry->ptr = ptr;
	next = s_freeLater->next;
	s_freeLater->next = entry;
	entry->next = next;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_freeLaterLock, status);
}

cyclic_id_t _cyclic_high_alloc(cyc_handler_t *hdlr, cyc_time_t *when, uint32_t cpumask)
{
	SceKernelIntrStatus status;
	FreeLater *entry, *next;
	cyclic_id_t id;

	id = dtrace_heap_zalloc(sizeof(*id));
	if (id == NULL) {
		DTRACE_LOG_ERROR_AT(119, "failed to allocate timer wrapper");
		return NULL;
	}

	id->cy_handler = *hdlr;
	id->cy_when = *when;
	id->cy_cpumask = cpumask;
	id->cy_timer = -1;
	id->cy_thread = -1;

	status = sceKernelSpinlockLowLockCpuSuspendIntr(&s_freeLaterLock);
	if (s_freeLater != NULL) {
		for (entry = s_freeLater->next; entry != s_freeLater; entry = next) {
			dtrace_heap_free(entry->ptr);
			next = entry->next;
			dtrace_heap_free(entry);
		}
		dtrace_heap_free(s_freeLater);
		s_freeLater = NULL;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_freeLaterLock, status);

	return id;
}

void _cyclic_high_free(cyclic_id_t id)
{
	if (id == NULL)
		return;

	SCE_DBG_ASSERT(id->cy_handler.cyh_level == CY_HIGH_LEVEL, 144);
	dtrace_heap_free(id);
}

static inline int addCyclicInfo(cyclic_id_t id, int type)
{
	CyclicInfo *info, *next;

	if (s_cyclicInfo == NULL) {
		s_cyclicInfo = dtrace_heap_alloc(sizeof(*s_cyclicInfo));
		if (s_cyclicInfo == NULL)
			return SCE_ERROR_ERRNO_ENOMEM;
		s_cyclicInfo->cyclic = NULL;
		s_cyclicInfo->type = CYCLIC_HIGH_TIMER_ANY;
		s_cyclicInfo->next = s_cyclicInfo;
	}

	info = dtrace_heap_alloc(sizeof(*info));
	if (info == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	info->cyclic = id;
	info->type = type;
	next = s_cyclicInfo->next;
	s_cyclicInfo->next = info;
	info->next = next;
	return 0;
}

static inline int allocCyclicTimer(cyclic_id_t id, int type)
{
	SceSysTimerId timer;

	timer = sceKernelSysTimerAlloc(type);
	if (timer < 0)
		return timer;

	id->cy_timer = timer;
	return addCyclicInfo(id, type);
}

/* gives the cyclic a system timer of the type (any for a new one) and sets it up */
static inline int setupCyclicTimer(cyclic_id_t id, int type)
{
	hrtime_t cycles;
	int res;

	if (id == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

	cycles = CYCLIC_HIGH_NSEC_TO_CYCLES(id->cy_when.cyt_interval);
	id->cy_timer = -1;

	if (type == CYCLIC_HIGH_TIMER_ANY) {
		if (cycles < 0x100000000ULL)
			res = allocCyclicTimer(id, SCE_SYSTIMER_TYPE_WORD);
		if (id->cy_timer < 0)
			res = allocCyclicTimer(id, SCE_SYSTIMER_TYPE_LONG);
	} else if (type == SCE_SYSTIMER_TYPE_WORD && cycles >= 0x100000000ULL) {
		res = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	} else if (type == SCE_SYSTIMER_TYPE_WORD || type == SCE_SYSTIMER_TYPE_LONG) {
		res = sceKernelSysTimerAlloc(type);
		if (res >= 0)
			id->cy_timer = res;
	} else {
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}

	if (id->cy_timer < 0) {
		DTRACE_LOG_ERROR_AT(233, "failed to allocate cyclic %p systimer (0x%08x)", id, res);
		id->cy_timer = -1;
		return SCE_KERNEL_ERROR_NO_FREE_TIMER;
	}

	res = SceSystimerForDriver_0450933B(id->cy_timer, 3, 0);
	if (res != 0) {
		DTRACE_LOG_ERROR_AT(240, "failed to set cyclic %p clock source: 0x%x", id, res);
		return res;
	}

	res = SceSystimerForDriver_A73351C1(id->cy_timer, cycles);
	if (res != 0) {
		DTRACE_LOG_ERROR_AT(246, "failed to set cyclic %p interval in cycles: 0x%x", id, res);
		return res;
	}

	res = SceSystimerForDriver_CCCD1EE3(id->cy_timer, (SceSysTimerCallback)_cyclic_high_handler,
					    id->cy_cpumask, id);
	if (res != 0) {
		DTRACE_LOG_ERROR_AT(254, "failed to set cyclic %p handler: 0x%x", id, res);
		return res;
	}

	return res;
}

int _cyclic_high_init(cyclic_id_t id)
{
	return setupCyclicTimer(id, CYCLIC_HIGH_TIMER_ANY);
}

int _cyclic_high_start(cyclic_id_t id)
{
	int res;

	if (id == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

	SCE_DBG_ASSERT(id->cy_handler.cyh_level == CY_HIGH_LEVEL, 270);
	if (id->cy_timer == -1)
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;

	res = SceSystimerForDriver_59EBD22D(id->cy_timer);
	if (res != 0) {
		DTRACE_LOG_ERROR_AT(275, "failed to start cyclic %p: 0x%x", id, res);
		return res;
	}

	return res;
}

int _cyclic_high_stop(cyclic_id_t id)
{
	int res;

	if (id == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

	SCE_DBG_ASSERT(id->cy_handler.cyh_level == CY_HIGH_LEVEL, 295);
	if (id->cy_timer == -1)
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;

	res = SceSystimerForDriver_BF8D42B0(id->cy_timer);
	if (res == SCE_KERNEL_ERROR_TIMER_STOPPED || res == 0)
		return 0;

	DTRACE_LOG_ERROR_AT(300, "failed to stop cyclic %p: 0x%x", id, res);
	return res;
}

static inline void removeCyclicInfo(cyclic_id_t id)
{
	CyclicInfo *prev, *info;

	SCE_DBG_ASSERT(s_cyclicInfo != NULL, 380);
	prev = s_cyclicInfo;
	for (info = s_cyclicInfo->next; info != s_cyclicInfo; info = info->next) {
		if (info->cyclic == id) {
			prev->next = info->next;
			dtrace_heap_free(info);
			SceSystimerForDriver_8358611F(id->cy_timer);
			id->cy_timer = -1;
			return;
		}
		prev = info;
	}

	SCE_PANIC_AT(394);
}

void _cyclic_high_remove(cyclic_id_t id)
{
	if (id == NULL)
		return;

	SCE_DBG_ASSERT(id->cy_handler.cyh_level == CY_HIGH_LEVEL, 319);
	if (id->cy_timer != -1)
		removeCyclicInfo(id);

	id->cy_handler.cyh_func = NULL;
	_dtrace_heap_free_later(id);
}

int sceDtracepCyclicHighSuspend(void)
{
	CyclicInfo *info;
	cyclic_id_t id;

	if (s_cyclicInfo == NULL)
		return 0;

	for (info = s_cyclicInfo->next; info != s_cyclicInfo; info = info->next) {
		SCE_DBG_ASSERT(info->cyclic->cy_handler.cyh_level == CY_HIGH_LEVEL, 408);
		id = info->cyclic;
		if (id->cy_timer != -1) {
			SceSystimerForDriver_BF8D42B0(id->cy_timer);
			SceSystimerForDriver_963F7C0D(info->cyclic->cy_timer, &info->count);
			SceSystimerForDriver_8358611F(info->cyclic->cy_timer);
			info->cyclic->cy_timer = -1;
		}
	}

	return 0;
}

int sceDtracepCyclicHighResume(void)
{
	CyclicInfo *info;

	if (s_cyclicInfo == NULL)
		return 0;

	for (info = s_cyclicInfo->next; info != s_cyclicInfo; info = info->next) {
		setupCyclicTimer(info->cyclic, info->type);
		SceSystimerForDriver_1B14DFD6(info->cyclic->cy_timer, info->count);
		_cyclic_high_start(info->cyclic);
	}

	return 0;
}
