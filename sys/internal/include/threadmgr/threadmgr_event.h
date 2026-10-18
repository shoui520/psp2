/* Kernel event information returned to debugger clients (the types: threadmgr/threadmgr.h). */
#pragma once

#include <threadmgr/threadmgr.h>

int SceThreadmgrForDriver_36F8E58A(const SceKernelWaitEvent *events,
		uint32_t count, uint32_t mode, SceKernelResultEvent *results,
		SceUInt32 *timeout);

int SceThreadmgrForDriver_CBA7FAAA(SceUID eventId, SceKernelEventInfo *info);
