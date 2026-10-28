#ifndef PAF_CRT_MGR_H
#define PAF_CRT_MGR_H

#include "../../compiler.h"
#include <kernel/threadmgr.h>

typedef struct PafCrtManager {
	SceKernelLwMutexWork mutex[2];
} PafCrtManager;

typedef char PafCrtManagerSizeCheck[sizeof(PafCrtManager) == 64 ? 1 : -1];

PafCrtManager *PafCrtManagerGet(void);
PafCrtManager *PafCrtManagerConstruct(PafCrtManager *self);
PafCrtManager *PafCrtManagerDestruct(PafCrtManager *self);
int PafCrtManagerLock(PafCrtManager *self, unsigned int index);
int PafCrtManagerUnlock(PafCrtManager *self, unsigned int index);
int PafCrtManagerInitialize(void);

#endif
