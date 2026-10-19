/* SceDeci4pCpup: the communication processor's update, RTC and serial ID requests over DECI4P */
#pragma once

#include <scetypes.h>

/* cpup.c */
int sceDeci4pCpupInquireVersionForDriver(SceUInt32 *protocolVersion, SceUInt32 *packageVersion,
	SceUInt32 *boardVersion);
int SceDeci4pCpupForDriver_765F7234(SceUInt32 arg0, SceUInt32 arg1, SceUInt32 arg2, SceUInt32 *ctx);
int sceDeci4pCpupStartUpdateForDriver(SceUInt32 arg0, SceUInt32 arg1, SceUInt32 *ctx);
int sceDeci4pCpupTransferUpdatePackageForDriver(SceUInt32 ctx, SceUInt32 offset, SceSize size,
	const void *data);
int SceDeci4pCpupForDriver_02CDA82C(SceUInt32 ctx);
int sceDeci4pCpupTriggerUpdateForDriver(SceUInt32 ctx);
int sceDeci4pCpupGetProgressForDriver(SceUInt32 ctx, SceUInt32 *out0, SceUInt32 *out1,
	SceUInt32 *out2);
int SceDeci4pCpupForDriver_2BAD7B39(SceUInt32 ctx);
int sceDeci4pCpupExecCommand(SceUInt32 command);
int sceDeci4pCpupGetRtcForDriver(SceUInt32 *rtc, int physical);
int sceDeci4pCpupSetRtcForDriver(SceUInt32 rtc, int physical);
int sceDeci4pCpupSetRtcWithActivationKeyForDriver(SceUInt32 rtc, int physical,
	const SceUInt32 *key);
int sceDeci4pCpupGetCpSerialIdForDriver(void *serialId, SceSize *size);
