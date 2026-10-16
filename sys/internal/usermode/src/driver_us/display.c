/*
 * SceDriverUser: SceDisplayUser, Display, Error history and DrmBridge
 * system call front ends.
 */

#include "driver_user.h"

int _sceDisplayGetMaximumFrameBufResolution(int *width, int *height);
int _sceDisplaySetFrameBufInternal(int head, int index, const SceSizedParam *param,
				   SceSyscallArgs4 *args);
int _sceDisplaySetFrameBufForCompat(int head, int index, const SceSizedParam *param,
				    SceSyscallArgs6 *args);
int _sceDisplayGetFrameBufInternal(int head, int index, SceSizedParam *param, SceSyscallArgs4 *args);
int _sceDisplaySetFrameBuf(const SceSizedParam *param, int sync, SceSyscallArgs2 *args);
int _sceDisplayGetFrameBuf(SceSizedParam *param, int sync, SceSyscallArgs2 *args);
int _sceDisplayGetResolutionInfoInternal(int head, SceSizedParam *info, SceSyscallArgs2 *args);

int _sceErrorGetExternalString(char *buf, SceUInt32 code);
int _sceErrorHistoryPostError(const void *record);
int _sceErrorHistorySetDefaultFormat(const void *format);
int _sceErrorHistoryGetError(SceUInt32 index, void *buf);
int _sceErrorHistoryUpdateSequenceInfo(const void *info, int reserved);
int _sceErrorHistoryClearError(int reserved);

int _sceDrmBridgeGetCurrentSecureTick(void *tick);
int _sceDrmBridgeIsAllowRemotePlayDebug(void);
int SceDrmBridge_B81B597A(void);
struct MlnpsnlAuth1Args;
int _sceDrmBridgeMlnpsnlAuth1(void *buf0, void *buf1, void *buf2, struct MlnpsnlAuth1Args *args);
int _sceDrmBridgeMlnpsnlAuth2(void *buf0, void *buf1, void *buf2, SceSyscallArgs4 *args);

int sceDisplayGetMaximumFrameBufResolution(int *width, int *height)
{
	return _sceDisplayGetMaximumFrameBufResolution(width, height);
}

int sceDisplaySetFrameBufInternal(int head, int index, const SceSizedParam *param, int sync)
{
	SceSyscallArgs4 args;

	args.arg[0] = sync;
	args.arg[1] = param != NULL ? param->size : 0;
	return _sceDisplaySetFrameBufInternal(head, index, param, &args);
}

int sceDisplaySetFrameBufForCompat(int head, int index, const SceSizedParam *param, int sync,
				   const SceSizedParam *param2, const SceSizedParam *param3)
{
	SceSyscallArgs6 args;

	args.arg[0] = sync;
	args.arg[1] = (SceUInt32)param2;
	args.arg[2] = (SceUInt32)param3;
	args.arg[3] = param2 != NULL ? param2->size : 0;
	args.arg[4] = param3 != NULL ? param3->size : 0;
	return _sceDisplaySetFrameBufForCompat(head, index, param, &args);
}

int sceDisplayGetFrameBufInternal(int head, int index, SceSizedParam *param, int sync)
{
	SceSyscallArgs4 args;

	args.arg[0] = sync;
	args.arg[1] = param != NULL ? param->size : 0;
	return _sceDisplayGetFrameBufInternal(head, index, param, &args);
}

int sceDisplaySetFrameBuf(const SceSizedParam *param, int sync)
{
	SceSyscallArgs2 args;

	args.arg[0] = param != NULL ? param->size : 0;
	return _sceDisplaySetFrameBuf(param, sync, &args);
}

int sceDisplayGetFrameBuf(SceSizedParam *param, int sync)
{
	SceSyscallArgs2 args;

	args.arg[0] = param != NULL ? param->size : 0;
	return _sceDisplayGetFrameBuf(param, sync, &args);
}

int sceDisplayGetResolutionInfoInternal(int head, SceSizedParam *info)
{
	SceSyscallArgs2 args;

	args.arg[0] = info->size;
	return _sceDisplayGetResolutionInfoInternal(head, info, &args);
}

int sceErrorGetExternalString(char *buf, SceUInt32 code)
{
	return _sceErrorGetExternalString(buf, code);
}

int sceErrorHistoryPostError(const void *record)
{
	return _sceErrorHistoryPostError(record);
}

int sceErrorHistorySetDefaultFormat(const void *format)
{
	return _sceErrorHistorySetDefaultFormat(format);
}

int sceErrorHistoryGetError(SceUInt32 index, void *buf)
{
	return _sceErrorHistoryGetError(index, buf);
}

int sceErrorHistoryUpdateSequenceInfo(const void *info, int reserved)
{
	return _sceErrorHistoryUpdateSequenceInfo(info, reserved);
}

int sceErrorHistoryClearError(int reserved)
{
	return _sceErrorHistoryClearError(reserved);
}

int sceDrmBridgeGetCurrentSecureTick(void *tick)
{
	return _sceDrmBridgeGetCurrentSecureTick(tick);
}

int sceDrmBridgeIsAllowRemotePlayDebug(void)
{
	return _sceDrmBridgeIsAllowRemotePlayDebug();
}

int SceDrmBridgeUser_6F5DC6CD(void)
{
	return SceDrmBridge_B81B597A();
}

/* the four buffers' sizes travel with the fourth buffer */
typedef struct MlnpsnlAuth1Args {
	void *buf3;
	SceSize size[4];
} MlnpsnlAuth1Args;

int sceDrmBridgeMlnpsnlAuth1(void *buf0, void *buf1, void *buf2, void *buf3)
{
	MlnpsnlAuth1Args args = {
		.buf3 = buf3,
		.size = { 0x20, 0x8, 0x80, 0x40 },
	};

	return _sceDrmBridgeMlnpsnlAuth1(buf0, buf1, buf2, &args);
}

int sceDrmBridgeMlnpsnlAuth2(void *buf0, void *buf1, void *buf2)
{
	SceSyscallArgs4 args;

	args.arg[0] = 0x50;
	args.arg[1] = 0x40;
	args.arg[2] = 0x10;
	return _sceDrmBridgeMlnpsnlAuth2(buf0, buf1, buf2, &args);
}
