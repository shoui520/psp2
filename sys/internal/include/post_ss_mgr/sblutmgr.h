#pragma once

#include <scetypes.h>
#include <stdint.h>
#include <authmgr/sblauthmgr.h>

/*
 * The utoken's Trilithium buffer (record 0xb): the authentication sceSblAuthMgrAuthHeader puts in
 * place of an fself's ("overwrite fself authinfo"), read through sysroot D75D4F37. Names unknown;
 * the fields are SceSelfAuthInfo's they replace.
 */
typedef struct SceSblUtMgrTrilithiumBuffer {
	SceUInt64 programAuthorityId;
	SceSelfAuthBits capability;
	SceSelfAuthBits attribute;
	SceSelfAuthSecret sharedSecret0;
} SceSblUtMgrTrilithiumBuffer;

#ifndef __cplusplus
_Static_assert(sizeof(SceSblUtMgrTrilithiumBuffer) == 0x58, "SceSblUtMgrTrilithiumBuffer");
#endif

int sceSblUtMgrGetCurrentSecureTick(SceUInt32 *left);
int sceSblUtMgrIsTrilithiumFlagEnabled(char *flag, SceSize size);
int sceSblUtMgrReadUtoken(void *utoken, SceSize size);
int sceSblUtMgrResetUtokenFile(void);
int sceSblUtMgrUpdateUtoken(const void *utoken, SceSize size);
