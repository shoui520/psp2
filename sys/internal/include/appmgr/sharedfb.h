/* SceSharedFb interface additions to vitasdk. */
#pragma once

#include <psp2/sharedfb.h>

typedef struct SceSharedFbCreate {
	SceSize AllocMemSize;
	unsigned int creationArgument;
	unsigned char reserved[16];
} SceSharedFbCreate;

_Static_assert(sizeof(SceSharedFbCreate) == 24, "SharedFb creation parameter ABI");

SceUID sceSharedFbCreate(int index, const SceSharedFbCreate *param);

int sceSharedFbDelete(SceUID fbId);
int sceSharedFbGetRenderingInfo(SceUID processId);
int sceSharedFbGetShellRenderPort(void);
int sceSharedFbUpdateProcess(void);
int sceSharedFbUpdateProcessBegin(SceUID fbId, const void *info, unsigned int *processCount);
int sceSharedFbUpdateProcessEnd(void);
