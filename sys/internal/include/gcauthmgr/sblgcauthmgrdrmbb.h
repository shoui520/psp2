/*
 * SceSblGcAuthMgrDrmBB: the game card's DRM secrets. Not written in C yet: the prototypes
 * follow vitasdk's psp2kern/kernel/gcauthmgr/drm_bb.h.
 */
#pragma once

/* vitasdk: ksceSblGcAuthMgrDrmBBCheckCartHash */
int SceSblGcAuthMgrDrmBBForDriver_22FD5D23(const void *hash);
/* npdrm's use (npdrm.c) */
int SceSblGcAuthMgrDrmBBForDriver_812B2B5C(int type, unsigned int *value);
/* vitasdk: ksceSblGcAuthMgrDrmBBClearCartSecret */
int SceSblGcAuthMgrDrmBBForDriver_BB451E83(void);
/* vitasdk: ksceSblGcAuthMgrDrmBBGetCartSecret */
int SceSblGcAuthMgrDrmBBForDriver_BB70DDC0(void *secret);
/* the adhoc signatures, on a kernel stack (drm_adhoc_api.c) */
int SceSblGcAuthMgrDrmBBForDriver_30A0B441(const unsigned char *digest,
		unsigned char *signature, int keyId);
int SceSblGcAuthMgrDrmBBForDriver_4CEA150C(const unsigned char *digest,
		unsigned char *signature, int keyId);
int SceSblGcAuthMgrDrmBBForDriver_050DC6DF(const unsigned char *digest,
		unsigned char *signature, int keyId);
int SceSblGcAuthMgrDrmBBForDriver_4B506BE7(const unsigned char *digest,
		unsigned char *signature, int keyId);
