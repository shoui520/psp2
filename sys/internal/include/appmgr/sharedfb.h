/* SceAppMgr's shared framebuffer (sharedfb.c): its records and its exports. */
#pragma once

#include <scetypes.h>

/*
 * A shared framebuffer's information, kept in its port: the framebuffer the creator allocated,
 * then the record sceSharedFbUpdateProcessBegin copies from its caller.
 */
typedef struct SharedFbInfo {
	void *base;
	int size;
	unsigned char update[64];
	unsigned char reserved[16];
} SharedFbInfo;

_Static_assert(sizeof(SharedFbInfo) == 0x58, "SharedFb information");

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
