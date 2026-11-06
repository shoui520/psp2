/*
 * SceAppUtilExt's own declarations: its exports outside the SDK, the checks every export runs
 * first, and the SceAppMgr errors it translates into SceAppUtil's.
 */
#ifndef APPUTIL_EXT_INTERNAL_H
#define APPUTIL_EXT_INTERNAL_H

#include <scetypes.h>
#include <apputil.h>

/* SceAppMgr's errors (no header names them) */
#define APPMGR_ERROR_NOT_MOUNTED		(-2139095038)	/* 0x80800002 */
#define APPMGR_ERROR_ALREADY_MOUNTED		(-2139095037)	/* 0x80800003 */
#define APPMGR_ERROR_NO_PERMISSION		(-2139095031)	/* 0x80800009 */
#define APPMGR_ERROR_MOUNT_LIMIT_OVER		(-2139094968)	/* 0x80800048 */
#define APPMGR_ERROR_SAVEDATA_NO_SPACE_QUOTA	(-2139090804)	/* 0x8080108c */
#define APPMGR_ERROR_SAVEDATA_NO_SPACE_FS	(-2139090803)	/* 0x8080108d */
#define APPMGR_ERROR_SAVEDATA_PARAMETER		(-2139090802)	/* 0x8080108e */
#define APPMGR_ERROR_PLAYREADY_NO_DATA		(-2139090701)	/* 0x808010f3 */
#define APPMGR_ERROR_MARLIN_IPTV_DIRTY		(-2139090699)	/* 0x808010f5 */

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
