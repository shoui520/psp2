/*
 * SceVshBridge NVS stub wrapper (vshSblSsSetNvsData): passes the buffer size in a
 * SyscallSizeArgs block to _vshSblSsSetNvsData.
 */
#include <scetypes.h>
#include <intrmgr/syscall_copy.h>
#include <vshbridge/vshbridge.h>

int _vshSblSsSetNvsData(int area, const void *buf, SceSize size, const SyscallSizeArgs *args);

SceUInt32 vshSblSsSetNvsData(int area, const void *buf, SceSize size)
{
	SyscallSizeArgs args;

	args.size = size;
	return (SceUInt32)_vshSblSsSetNvsData(area, buf, size, &args);
}

#ifdef UPDATER_CORE_2014
int vshSblSsDecryptWithPortability(SceUInt32 mode, const void *iv, const void *in, void *out)
{
	return _vshSblSsDecryptWithPortability(mode, iv, in, out);
}
#endif
