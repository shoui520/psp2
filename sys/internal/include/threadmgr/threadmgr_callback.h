/* SceKernelThreadMgr: kernel callback information (the type: threadmgr/threadmgr.h). */
#pragma once

#include <threadmgr/threadmgr.h>

#ifdef __cplusplus
extern "C" {
#endif

int sceKernelGetCallbackInfo(SceUID callbackId, SceKernelCallbackInfo *info);

#ifdef __cplusplus
}
#endif
