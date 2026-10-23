/*
 * SceSblGcAuthMgrMsSaveBB: the PSP save data's MAC and cipher. Not written in C yet: the
 * prototypes follow appmgr's use (savedata.c). A MAC context is 40 bytes, a cipher
 * context 24.
 */
#pragma once

#include <scetypes.h>

int SceSblGcAuthMgrMsSaveBBForDriver_4A249828(void *macContext, int keyType);
int SceSblGcAuthMgrMsSaveBBForDriver_79A9BE44(void *macContext, const void *data, SceSize size);
int SceSblGcAuthMgrMsSaveBBForDriver_8E6626A0(void *macContext, void *digest, const void *key);
int SceSblGcAuthMgrMsSaveBBForDriver_3C185A67(void *cipherContext, int keyType, int mode,
					      const void *header, const void *key);
int SceSblGcAuthMgrMsSaveBBForDriver_F66D5F76(void *cipherContext, void *data, SceSize size);
int SceSblGcAuthMgrMsSaveBBForDriver_805C860B(void *cipherContext);
