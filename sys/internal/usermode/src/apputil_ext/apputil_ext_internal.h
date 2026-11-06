/*
 * SceAppUtilExt's own declarations: its exports outside the SDK, the checks every export runs
 * first, and the errors it translates SceAppMgr's into.
 */
#ifndef APPUTIL_EXT_INTERNAL_H
#define APPUTIL_EXT_INTERNAL_H

#include <scetypes.h>
#include <apputil.h>
#include <appmgr/appmgr.h>

/* SceAppUtil's errors beyond the SDK's */
#define APPUTIL_EXT_ERROR_PLAYREADY_NO_DATA	(-2146433375)	/* 0x801006a1 */
#define APPUTIL_EXT_ERROR_MARLIN_IPTV_DIRTY	(-2146433372)	/* 0x801006a4 */

/* the system data files */
#define APPUTIL_EXT_PLAYREADY_INFORMATION	1
#define APPUTIL_EXT_PLAYREADY_DATA		2
#define APPUTIL_EXT_MARLIN_IPTV			3

#define APPUTIL_EXT_PLAYREADY_DATA_MAXSIZE	0x4000
#define APPUTIL_EXT_MARLIN_IPTV_SIZE		0x1000

/* the stack every export needs */
#define APPUTIL_EXT_STACK_SIZE			0x800
#define APPUTIL_EXT_VIDEO_STACK_SIZE		0x400

void paramError(int code);
int checkStack(SceSize size);
int initVideo(void);

int SceAppUtilExtMarlinIptv_DFFF8D1F(const void *buffer, SceSize size);
int SceAppUtilExtMarlinIptv_B86104DB(void *buffer, SceSize size);
int SceAppUtilExtPlayReady_A9C2098C(const SceUInt64 *information);
int SceAppUtilExtPlayReady_E915A695(SceUInt64 *information);
int SceAppUtilExtPlayReady_4C91CDB1(const void *buffer, SceSize size);
int SceAppUtilExtPlayReady_9B52C63D(void *buffer, SceSize size);
int SceAppUtilExtPsNow_2CF451BE(const SceAppUtilSaveDataDataSlot *slot,
				const SceAppUtilSaveDataDataSaveItem *data, SceUInt32 dataNum,
				const SceAppUtilMountPoint *mountPoint, SceSize *requiredSizeKiB);
int SceAppUtilExtPsNow_2BF9A33D(const SceAppUtilSaveDataDataSlot *slot,
				const SceAppUtilSaveDataDataRemoveItem *data, SceUInt32 dataNum,
				const SceAppUtilMountPoint *mountPoint);

#endif
