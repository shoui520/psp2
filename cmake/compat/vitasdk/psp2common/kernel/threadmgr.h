/*
 * vitasdk's psp2common/kernel/threadmgr.h. Kernel code takes the thread manager's types and
 * constants from threadmgr/threadmgr.h (Sony's names and layouts), and so do the vitasdk kernel
 * headers that include this one; the one vitasdk type their declarations still name stays here.
 * User code keeps vitasdk's header until the user-mode SDK headers.
 */
#ifdef PSP2_KERNEL
#ifndef _PSP2COMMON_KERNEL_THREADMGR_H_
#define _PSP2COMMON_KERNEL_THREADMGR_H_

#include <threadmgr/threadmgr.h>

/* vitasdk's psp2kern/kernel/threadmgr/misc.h: ksceKernelGetThreadmgrUIDClass */
typedef enum SceKernelIdListType {
	SCE_KERNEL_TMID_Thread			= 1,
	SCE_KERNEL_TMID_Semaphore		= 2,
	SCE_KERNEL_TMID_EventFlag		= 3,
	SCE_KERNEL_TMID_Mbox			= 4,
	SCE_KERNEL_TMID_Vpl			= 5,
	SCE_KERNEL_TMID_Fpl			= 6,
	SCE_KERNEL_TMID_Mpipe			= 7,
	SCE_KERNEL_TMID_Callback		= 8,
	SCE_KERNEL_TMID_ThreadEventHandler	= 9,
	SCE_KERNEL_TMID_Alarm			= 10,
	SCE_KERNEL_TMID_VTimer			= 11,
	SCE_KERNEL_TMID_SleepThread		= 64,
	SCE_KERNEL_TMID_DelayThread		= 65,
	SCE_KERNEL_TMID_SuspendThread		= 66,
	SCE_KERNEL_TMID_DormantThread		= 67
} SceKernelIdListType;

#endif /* _PSP2COMMON_KERNEL_THREADMGR_H_ */
#else
#include_next <psp2common/kernel/threadmgr.h>
#endif
