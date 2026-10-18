/*
 * SceSblGcAuthMgrDrmBB: the game card's DRM secrets. Not written in C yet: the prototypes
 * follow vitasdk's psp2kern/kernel/gcauthmgr/drm_bb.h.
 */
#pragma once

int SceSblGcAuthMgrDrmBBForDriver_22FD5D23(const void *hash);
int SceSblGcAuthMgrDrmBBForDriver_BB451E83(void);
int SceSblGcAuthMgrDrmBBForDriver_BB70DDC0(void *secret);
