/*
 * SceAppUtil's exports outside the SDK that SceAppUtilExt calls: the checks its save data data calls
 * run on their arguments, the library's state, and the notice it sends when a save data slot
 * changes. No record names them; the prototypes are SceAppUtil's (vs0:sys/external/apputil.suprx).
 */
#ifndef _SCE_APPUTIL_APPUTIL_INTERNAL_H
#define _SCE_APPUTIL_APPUTIL_INTERNAL_H

#include <scetypes.h>
#include <apputil.h>

#ifdef __cplusplus
extern "C" {
#endif

/* a save data slot changed (SceAppUtil_F93C691C's event) */
#define SCE_APPUTIL_INTERNAL_EVENT_SAVEDATA_SLOT	1

/* the error between SCE_APPUTIL_ERROR_PHOTO_DEVICE_NOT_FOUND and _MUSIC_DEVICE_NOT_FOUND that
   the SDK does not name (name ours): the message dialog shows it as no memory card */
#define SCE_APPUTIL_INTERNAL_ERROR_DEVICE_NOT_FOUND	(-2146433404)	/* 0x80100684 */

/* the library's state: SCE_APPUTIL_ERROR_NOT_INITIALIZED before sceAppUtilInit */
typedef struct SceAppUtilInternalState {
	SceUInt32 sdkVersion;		/* the main module's */
	SceUInt8 reserved[60];
} SceAppUtilInternalState;

_Static_assert(sizeof(SceAppUtilInternalState) == 64, "SceAppUtilInternalState");

/* sceAppUtilSaveDataDataSave's and sceAppUtilSaveDataDataRemove's argument checks */
int SceAppUtil_E8CADCA9(const SceAppUtilSaveDataDataSlot *slot,
			const SceAppUtilSaveDataDataSaveItem *data, SceUInt32 dataNum,
			const SceAppUtilMountPoint *mountPoint, SceSize *requiredSizeKiB);
int SceAppUtil_1CFFE892(const SceAppUtilSaveDataDataSlot *slot,
			const SceAppUtilSaveDataDataRemoveItem *data, SceUInt32 dataNum,
			const SceAppUtilMountPoint *mountPoint);
/* nonzero when str (at most size bytes, up to its NUL) is not valid UTF-8 */
/*
 * System parameters past the SDK's: the enter button (Sony's later SDKs name it, with
 * SCE_SYSTEM_PARAM_ENTER_BUTTON_ASSIGN_CIRCLE and _CROSS) and the ones system applications read,
 * named by their registry keys
 */
#define SCE_SYSTEM_PARAM_ID_ENTER_BUTTON		(2)
#define SYSTEM_PARAM_ID_NP_COUNTRY			(8)	/* string, 3 bytes */
#define SYSTEM_PARAM_ID_IMPOSE_AUDIO_BALANCE		(9)
#define SYSTEM_PARAM_ID_NP_ENV				(10)	/* string, 17 bytes */
#define SYSTEM_PARAM_ID_VOICE_PRIORITY			(11)
#define SYSTEM_PARAM_ID_HDMI_OUT_SCALING_RATIO		(12)
#define SYSTEM_PARAM_ID_PSNOW_APP_CACHED_URL		(100)	/* string, 256 bytes */

int SceAppUtil_7DA34311(const SceChar8 *str, SceSize size);
int SceAppUtil_A3E0694F(SceAppUtilInternalState *state);
/* passes the event to the callback the library was given, if any */
int SceAppUtil_F93C691C(int event, SceAppUtilSaveDataSlotId id);
/* mount and unmount a drive SceAppUtil keeps for the system (202: the PSP save data root);
   SCE_APPUTIL_ERROR_BUSY when it is mounted already, mountPoint still filled in */
int SceAppUtil_C3CB128B(int type, char *mountPoint);
int SceAppUtil_C0418B1C(int type);
/* the application's own updates in the background download queue, not ready and ready:
   SceMsgDialogPlugin takes a first one above 0 as a patch found for it */
int SceAppUtil_B2FE3BB1(SceInt32 *notReady, SceInt32 *ready);

/*
 * SceAppUtilWebBrowserCBLimited_3ED8F841's parameter (name ours): the web browser opened at wbstr
 * like sceAppUtilLaunchWebBrowser, told to call the application back with cbstr, and the user
 * agent it is to use kept in useragent.dat
 */
typedef struct SceAppUtilWebBrowserCBLimitedParam {
	const SceChar8 *wbstr;
	SceUInt32 wbstrLength;
	const SceChar8 *cbstr;
	SceUInt32 cbstrLength;
	SceUInt32 launchMode;
	const SceChar8 *userAgent;
} SceAppUtilWebBrowserCBLimitedParam;

int SceAppUtilWebBrowserCBLimited_3ED8F841(const SceAppUtilWebBrowserCBLimitedParam *param);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_APPUTIL_APPUTIL_INTERNAL_H */
