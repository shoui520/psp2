/* SceKernelThreadMgr: kernel callback information (the type: threadmgr/threadmgr.h). */
#pragma once

#include <threadmgr/threadmgr.h>

#ifdef __cplusplus
extern "C" {
#endif

int SceThreadmgrForDriver_FDC044AC(SceUID callbackId, SceKernelCallbackInfo *info);

#ifdef __cplusplus
}
#endif
