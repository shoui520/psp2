/*
 * vitasdk's message pipe header: SceKernelMsgPipeInfo is Sony's (threadmgr/threadmgr.h), so
 * vitasdk's copy and its getter are renamed aside.
 */
#pragma once

#include <threadmgr/threadmgr.h>

#define SceKernelMsgPipeInfo SceKernelMsgPipeInfo_vitasdk
#define ksceKernelGetMsgPipeInfo ksceKernelGetMsgPipeInfo_vitasdk
#include_next <psp2kern/kernel/threadmgr/msg_pipe.h>
#undef ksceKernelGetMsgPipeInfo
#undef SceKernelMsgPipeInfo

/* our NID db's name for SceThreadmgrForDriver's message pipe query, with Sony's type */
int ksceKernelGetMsgPipeInfo(SceUID msgPipeId, SceKernelMsgPipeInfo *info);
