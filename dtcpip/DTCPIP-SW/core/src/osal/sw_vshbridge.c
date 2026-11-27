/*
 * sw_vshbridge.c — user-side thunks forwarding to SceVshBridge syscalls.
 */
#include <scetypes.h>
#include <rtc.h>

int _vshRtcGetCurrentSecureTick(SceRtcTick *tick);
int _vshSblSsDecryptWithPortability(unsigned int keyType, const void *iv,
				    void *data, SceSize size);

int vshRtcGetCurrentSecureTick(SceRtcTick *tick)
{
	return _vshRtcGetCurrentSecureTick(tick);
}

int vshSblSsDecryptWithPortability(unsigned int keyType, const void *iv,
				   void *data, SceSize size)
{
	return _vshSblSsDecryptWithPortability(keyType, iv, data, size);
}
