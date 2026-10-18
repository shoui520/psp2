/*
 * SceDrmBridge (Sony's file name unknown): the few SceVshBridge services a
 * DRM-licensed program (not only a system program) may call. A caller
 * passes when SceSblACMgr grants it the DRM bridge (the secure tick also
 * to a PSM developer assistant). The user-pointer exports are wrapped in
 * drmbridge_user.c.
 */
#include <psp2kern/types.h>
#include <intrmgr/syscall.h>
#include <vshbridge/vshbridge_imports.h>
#include "vshbridge.h"

int drmBridgeGetCurrentSecureTick(SceRtcTick *tick)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!SceSblACMgrForDriver_F5AE24AC(0) && !SceSblACMgrForDriver_C98D82EE(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	ret = SceRtcForDriver_401C0954(tick);
	sceSyscallLeave(t);
	return ret;
}

/* a bridge for a DRM-licensed caller */
#define DRM_BRIDGE(_call) do {						\
	SceUInt32 _t = sceSyscallEnter();				\
	int _ret;							\
									\
	if (!SceSblACMgrForDriver_F5AE24AC(0)) {			\
		sceSyscallLeave(_t);					\
		return SCE_ERROR_ERRNO_ENOSYS;				\
	}								\
	_ret = (_call);							\
	sceSyscallLeave(_t);						\
	return _ret;							\
} while (0)

int drmBridgeIsAllowRemotePlayDebug(void)
{
	DRM_BRIDGE(SceQafMgrForDriver_BFD5E463());
}

int drmBridgeB81B597A(void)
{
	DRM_BRIDGE(SceQafMgrForDriver_4BC1883F());
}

int drmBridgeMlnpsnlAuth1(void *a, void *b, void *out1, void *out2)
{
	DRM_BRIDGE(SceSblGcAuthMgrMlnpsnlForDriver_DABC01F5(a, b, out1, out2));
}

int drmBridgeMlnpsnlAuth2(void *a, void *b, void *out)
{
	DRM_BRIDGE(SceSblGcAuthMgrMlnpsnlForDriver_296ABD68(a, b, out));
}
