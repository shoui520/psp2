/*
 * SceDrmBridge user-pointer exports (Sony's file name unknown): the copies
 * around the bridges in drmbridge.c, as in vshbridge_user.c.
 */
#include <scetypes.h>
#include <vshbridge/vshbridge_imports.h>
#include "vshbridge.h"
#include "vshbridge_copy.h"

int _sceDrmBridgeGetCurrentSecureTick(SceRtcTick *tick)
{
	int ret;

	BLOCK_OUT(tick, sizeof(*tick), drmBridgeGetCurrentSecureTick(NULL),
		  drmBridgeGetCurrentSecureTick((SceRtcTick *)k));
}

int _sceDrmBridgeIsAllowRemotePlayDebug(void)
{
	return drmBridgeIsAllowRemotePlayDebug();
}

int SceDrmBridge_B81B597A(void)
{
	return drmBridgeB81B597A();
}

int _sceDrmBridgeMlnpsnlAuth1(const void *a, const void *b, void *out, const VshMlnpsnlAuth1Args *uargs)
{
	MLNPSNL_AUTH1(drmBridgeMlnpsnlAuth1);
}

int _sceDrmBridgeMlnpsnlAuth2(const void *a, const void *b, void *out, const VshMlnpsnlAuth2Args *uargs)
{
	MLNPSNL_AUTH2(drmBridgeMlnpsnlAuth2);
}
