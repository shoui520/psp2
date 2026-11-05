/* SceKernelThreadMgr: kernel timer and alarm information (SceKernelTimerInfo: threadmgr/threadmgr.h). */
#pragma once

#include <threadmgr/threadmgr.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SceThreadmgrForDriver_4A038803 (0x38) */
typedef struct ThreadmgrAlarmInfo {
	SceSize size;
	SceUID alarmId;
	char name[32];
	uint64_t expiry;			/* 0x28 */
	ThreadmgrAlarmHandler handler;		/* 0x30 */
	void *arg;
} ThreadmgrAlarmInfo;

int sceKernelGetTimerInfo(SceUID timerId, SceKernelTimerInfo *info);
int SceThreadmgrForDriver_4A038803(SceUID alarmId, ThreadmgrAlarmInfo *info);
int SceThreadmgrForDriver_2828F884(int level, ThreadmgrAlarmInfo *infos, SceSize size,
				   SceSize *count);

#ifdef __cplusplus
}
#endif
