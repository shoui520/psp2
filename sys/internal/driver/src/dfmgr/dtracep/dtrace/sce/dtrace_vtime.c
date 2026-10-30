/*
 * Virtual time (Sony's dtrace/sce/dtrace_vtime.c): the thread manager keeps
 * each thread's time on a CPU, so DTrace has nothing to switch on or off.
 */
#include <sys/dtrace_impl.h>
#include <excpmgr/excpmgr_cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadcb.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_private.h>

void dtrace_vtime_enable(void)
{
}

void dtrace_vtime_disable(void)
{
}

int64_t dtrace_calc_current_thread_recent_vtime(void)
{
	struct SceUIDThreadObject *obj;
	SceObjectBase *base;
	struct ThreadMgrCpuCB *cpu;
	ThreadCBInfo info;
	uint64_t vtime, now;
	int res;

	SCE_DBG_ASSERT_CHECK((readCpsr() & PSR_I_BIT) != 0, 34);
	res = sceKernelGetThreadContextInfo(&info);
	if (res < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 38);
		return 0;
	}
	res = sceGUIDReferObjectWithClass(info.threadId, SceThreadmgrForKernel_565BD2DA(),
					  &base);
	if (res < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 43);
		return 0;
	}
	obj = (struct SceUIDThreadObject *)base;
	SCE_DBG_ASSERT_LEVEL(0, obj->context.magic == THREAD_CB_MAGIC, 47);
	cpu = &SceThreadmgrForKernel_19440C83()->cpuCB[dtrace_processor_id()];
	vtime = obj->context.runClock;
	now = sceKernelGetSystemTimeWide();
	vtime = vtime - cpu->lastThreadSwitchTime + now;
	sceGUIDReleaseObject(info.threadId);
	return vtime * 1000;
}
