/*
 * liblocation.h: the location and direction of the device, under Sony's names and with Sony's
 * definitions (SDK Release 03.570.011, liblocation Reference: the header is <liblocation.h>; the
 * SDK release here ships the documentation only, so the order and the forms are ours).
 * SceLibLocation exports the functions.
 */
#ifndef _SCE_LIBLOCATION_H
#define _SCE_LIBLOCATION_H

#include <scetypes.h>
#include <rtc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef SceUInt32 SceLocationHandle;

#define SCE_LOCATION_HANDLE_INVALID			(0xFFFFFFFFU)

/* sceLocationInit's mode: the version of the interface, and how the user's permission is got */
#define SCE_LOCATION_MODE_V1				(0x00000000U)
#define SCE_LOCATION_MODE_V2				(0x00000001U)
#define SCE_LOCATION_MODE_V3				(0x00000002U)
#define SCE_LOCATION_MODE_CONFIRM_MANUAL		(0x00000000U)
#define SCE_LOCATION_MODE_CONFIRM_AUTO			(0x00000100U)

/* the location method (lmethod): what calculates the location, and how often */
#define SCE_LOCATION_LMETHOD_NONE			(0x00000000U)
#define SCE_LOCATION_LMETHOD_AGPS_AND_3G_AND_WIFI	(0x00000001U)
#define SCE_LOCATION_LMETHOD_3G_AND_WIFI		(0x00000002U)
#define SCE_LOCATION_LMETHOD_WIFI			(0x00000003U)
#define SCE_LOCATION_LMETHOD_3G				(0x00000004U)
#define SCE_LOCATION_LMETHOD_GPS			(0x00000005U)
#define SCE_LOCATION_LMETHOD_GPS_EMULATION		(0x0000000FU)
#define SCE_LOCATION_LMETHOD_MASK			(0x0000000FU)

#define SCE_LOCATION_LOCATION_TIMING_MINIMUM		(0x00000000U)
#define SCE_LOCATION_LOCATION_TIMING_5SEC		(0x00000010U)
#define SCE_LOCATION_LOCATION_TIMING_10SEC		(0x00000020U)
#define SCE_LOCATION_LOCATION_TIMING_30SEC		(0x00000030U)
#define SCE_LOCATION_LOCATION_TIMING_60SEC		(0x00000040U)
#define SCE_LOCATION_LOCATION_TIMING_10MIN		(0x00000050U)
#define SCE_LOCATION_LOCATION_TIMING_MASK		(0x00000070U)

/* the heading method (hmethod): the reference system, the timing, the resolution, the filters */
#define SCE_LOCATION_HMETHOD_NONE			(0x00000000U)
#define SCE_LOCATION_HMETHOD_AUTO			(0x00000001U)
#define SCE_LOCATION_HMETHOD_VERTICAL			(0x00000002U)
#define SCE_LOCATION_HMETHOD_HORIZONTAL			(0x00000003U)
#define SCE_LOCATION_HMETHOD_CAMERA			(0x00000004U)
#define SCE_LOCATION_HMETHOD_MASK			(0x00000007U)

#define SCE_LOCATION_HEADING_TIMING_MINIMUM		(0x00000000U)
#define SCE_LOCATION_HEADING_TIMING_64MS		(0x00000010U)
#define SCE_LOCATION_HEADING_TIMING_100MS		(0x00000020U)
#define SCE_LOCATION_HEADING_TIMING_500MS		(0x00000030U)
#define SCE_LOCATION_HEADING_TIMING_1SEC		(0x00000040U)
#define SCE_LOCATION_HEADING_TIMING_MASK		(0x00000070U)

#define SCE_LOCATION_HEADING_RESOLUTION_MINIMUM		(0x00000000U)
#define SCE_LOCATION_HEADING_RESOLUTION_DEG1		(0x00000100U)
#define SCE_LOCATION_HEADING_RESOLUTION_DEG5		(0x00000200U)
#define SCE_LOCATION_HEADING_RESOLUTION_DIV32		(0x00000300U)
#define SCE_LOCATION_HEADING_RESOLUTION_DIV16		(0x00000400U)
#define SCE_LOCATION_HEADING_RESOLUTION_DIV8		(0x00000500U)
#define SCE_LOCATION_HEADING_RESOLUTION_DIV4		(0x00000600U)
#define SCE_LOCATION_HEADING_RESOLUTION_MASK		(0x00000700U)

#define SCE_LOCATION_HEADING_FILTER_AVERAGE_DISABLE	(0x00000000U)
#define SCE_LOCATION_HEADING_FILTER_AVERAGE_ENABLE	(0x00001000U)
#define SCE_LOCATION_HEADING_FILTER_AVERAGE_MASK	(0x00001000U)
#define SCE_LOCATION_HEADING_FILTER_HYSTERESIS_DISABLE	(0x00000000U)
#define SCE_LOCATION_HEADING_FILTER_HYSTERESIS_ENABLE	(0x00002000U)
#define SCE_LOCATION_HEADING_FILTER_HYSTERESIS_MASK	(0x00002000U)

#define SCE_LOCATION_HEADING_NORTH_TRUE			(0x00000000U)
#define SCE_LOCATION_HEADING_NORTH_MAGNETIC		(0x80000000U)
#define SCE_LOCATION_HEADING_NORTH_MASK			(0x80000000U)

#define SCE_LOCATION_HEADING_STABILITY_UNSTABLE		(0x00000000U)
#define SCE_LOCATION_HEADING_STABILITY_STABLE		(0x00000001U)
#define SCE_LOCATION_HEADING_STABILITY_VERYSTABLE	(0x00000002U)

/* the confirmation dialog (sceLocationConfirm) */
typedef SceInt32 SceLocationDialogStatus;
#define SCE_LOCATION_DIALOG_STATUS_IDLE			(0)
#define SCE_LOCATION_DIALOG_STATUS_RUNNING		(1)
#define SCE_LOCATION_DIALOG_STATUS_FINISHED		(2)

typedef SceInt32 SceLocationDialogResult;
#define SCE_LOCATION_DIALOG_RESULT_NONE			(0)
#define SCE_LOCATION_DIALOG_RESULT_DISABLE		(1)
#define SCE_LOCATION_DIALOG_STATUS_ENABLE		(2)

/* the permission to use the location (sceLocationGetPermission) */
typedef SceInt32 SceLocationPermissionApplicationStatus;
#define SCE_LOCATION_PERMISSION_APPLICATION_NONE	(0)
#define SCE_LOCATION_PERMISSION_APPLICATION_INIT	(1)
#define SCE_LOCATION_PERMISSION_APPLICATION_DENY	(2)
#define SCE_LOCATION_PERMISSION_APPLICATION_ALLOW	(3)

typedef SceInt32 SceLocationPermissionStatus;
#define SCE_LOCATION_PERMISSION_DENY			(0)
#define SCE_LOCATION_PERMISSION_ALLOW			(1)

#define SCE_LOCATION_PERMISSION_NOT_UPDATED		(0x00000000U)
#define SCE_LOCATION_PERMISSION_UPDATED			(0x00000001U)

typedef struct SceLocationLocationInfo {
	SceDouble64 latitude;
	SceDouble64 longitude;
	SceDouble64 altitude;
	SceFloat32 accuracy;
	SceFloat32 reserve;
	SceFloat32 direction;
	SceFloat32 speed;
	SceRtcTick timestamp;
} SceLocationLocationInfo;

typedef struct SceLocationHeadingInfo {
	SceFloat32 heading;
	SceFQuaternion nedQuat;
	SceUInt32 stability;
	SceRtcTick timestamp;
} SceLocationHeadingInfo;

typedef struct SceLocationPermissionInfo {
	SceLocationPermissionStatus parentalstatus;
	SceLocationPermissionStatus mainstatus;
	SceLocationPermissionApplicationStatus applicationstatus;
	SceUInt32 updated;
	SceUInt32 reserve;
} SceLocationPermissionInfo;

typedef void (*SceLocationLocationInfoCallback)(SceInt32 result, SceLocationHandle handle,
						const SceLocationLocationInfo *location,
						void *userdata);
typedef void (*SceLocationHeadingInfoCallback)(SceInt32 result, SceLocationHandle handle,
					       const SceLocationHeadingInfo *heading,
					       void *userdata);

#define SCE_LOCATION_SUCCESS				(0)
#define SCE_LOCATION_INFO_UNDETERMINED_LOCATION		(-2146430464)	/* 0x80101200 */
#define SCE_LOCATION_INFO_INSUFFICIENT_INFORMATION	(-2146430463)	/* 0x80101201 */
#define SCE_LOCATION_INFO_GET_LOCATION_CANCELED		(-2146430462)	/* 0x80101202 */
#define SCE_LOCATION_INFO_DENIED_BY_USER		(-2146430461)	/* 0x80101203 */
#define SCE_LOCATION_INFO_UNDETERMINED_HEADING		(-2146430460)	/* 0x80101204 */
#define SCE_LOCATION_INFO_DISABLE_DEVICE		(-2146430459)	/* 0x80101205 */
#define SCE_LOCATION_INFO_NOT_FOUND_AP			(-2146430458)	/* 0x80101206 */
#define SCE_LOCATION_INFO_INVALID_TIMESTAMP		(-2146430457)	/* 0x80101207 */
#define SCE_LOCATION_INFO_NETWORK_NOT_ACCESSED		(-2146430456)	/* 0x80101208 */
#define SCE_LOCATION_INFO_AP_NOT_IN_LOCATION_DB		(-2146430455)	/* 0x80101209 */
#define SCE_LOCATION_INFO_TIMEOUT			(-2146430454)	/* 0x8010120A */
#define SCE_LOCATION_ERROR_INVALID_ADDRESS		(-2146430400)	/* 0x80101240 */
#define SCE_LOCATION_ERROR_INVALID_HANDLE		(-2146430399)	/* 0x80101241 */
#define SCE_LOCATION_ERROR_NO_MEMORY			(-2146430398)	/* 0x80101242 */
#define SCE_LOCATION_ERROR_TOO_MANY_HANDLES		(-2146430397)	/* 0x80101243 */
#define SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD	(-2146430396)	/* 0x80101244 */
#define SCE_LOCATION_ERROR_INVALID_HEADING_METHOD	(-2146430395)	/* 0x80101245 */
#define SCE_LOCATION_ERROR_MULTIPLE_CALLBACK		(-2146430394)	/* 0x80101246 */
#define SCE_LOCATION_ERROR_NOT_RUNNING_CALLBACK		(-2146430393)	/* 0x80101247 */
#define SCE_LOCATION_ERROR_DIALOG_RESULT_NONE		(-2146430392)	/* 0x80101248 */
#define SCE_LOCATION_ERROR_DISABLE_APPLICATION		(-2146430391)	/* 0x80101249 */
#define SCE_LOCATION_ERROR_MULTIPLE_CONFIRM		(-2146430390)	/* 0x8010124A */
#define SCE_LOCATION_ERROR_DISABLE_DEBUG_API		(-2146430389)	/* 0x8010124B */
#define SCE_LOCATION_ERROR_INVALID_TIMING		(-2146430388)	/* 0x8010124C */
#define SCE_LOCATION_ERROR_INVALID_VALUE		(-2146430387)	/* 0x8010124D */
#define SCE_LOCATION_ERROR_INITIALIZED			(-2146430386)	/* 0x8010124E */
#define SCE_LOCATION_ERROR_UNINITIALIZED		(-2146430385)	/* 0x8010124F */
#define SCE_LOCATION_ERROR_MULTIPLE_GET_LOCATION	(-2146430384)	/* 0x80101250 */
#define SCE_LOCATION_ERROR_UNAUTHORIZED			(-2146430336)	/* 0x80101280 */
#define SCE_LOCATION_ERROR_PROVIDER_UNAVAILABLE		(-2146430335)	/* 0x80101281 */
#define SCE_LOCATION_ERROR_FILE_IO			(-2146430334)	/* 0x80101282 */
#define SCE_LOCATION_ERROR_INVALID_FILE_FORMAT		(-2146430333)	/* 0x80101283 */
#define SCE_LOCATION_ERROR_TIME_OUT			(-2146430332)	/* 0x80101284 */
#define SCE_LOCATION_ERROR_NO_SERVER_MEMORY		(-2146430331)	/* 0x80101285 */
#define SCE_LOCATION_ERROR_INVALID_TITLE_ID		(-2146430330)	/* 0x80101286 */
#define SCE_LOCATION_ERROR_INTERNAL_FILE_INVALID	(-2146430329)	/* 0x80101287 */
#define SCE_LOCATION_DBGERROR_EMULATION_FILE_ERROR	(-2146430240)	/* 0x801012E0 */
#define SCE_LOCATION_ERROR_FATAL_ERROR			(-2146430209)	/* 0x801012FF */

SceInt32 sceLocationInit(SceUInt32 open, SceUInt32 mode);
SceInt32 sceLocationTerm(void);
SceInt32 sceLocationOpen(SceLocationHandle *handle, SceUInt32 lmethod, SceUInt32 hmethod);
SceInt32 sceLocationClose(SceLocationHandle handle);
SceInt32 sceLocationReopen(SceLocationHandle handle, SceUInt32 lmethod, SceUInt32 hmethod);
SceInt32 sceLocationGetMethod(SceLocationHandle handle, SceUInt32 *lmethod, SceUInt32 *hmethod);
SceInt32 sceLocationGetLocation(SceLocationHandle handle, SceLocationLocationInfo *linfo);
SceInt32 sceLocationGetLocationWithTimeout(SceLocationHandle handle, SceUInt32 timeout,
					   SceLocationLocationInfo *linfo);
SceInt32 sceLocationCancelGetLocation(SceLocationHandle handle);
SceInt32 sceLocationStartLocationCallback(SceLocationHandle handle, SceUInt32 distance,
					  SceLocationLocationInfoCallback callback, void *userdata);
SceInt32 sceLocationStopLocationCallback(SceLocationHandle handle);
SceInt32 sceLocationGetHeading(SceLocationHandle handle, SceLocationHeadingInfo *hinfo);
SceInt32 sceLocationStartHeadingCallback(SceLocationHandle handle, SceFloat32 difference,
					 SceLocationHeadingInfoCallback callback, void *userdata);
SceInt32 sceLocationStopHeadingCallback(SceLocationHandle handle);
SceInt32 sceLocationConfirm(SceLocationHandle handle);
SceInt32 sceLocationConfirmGetStatus(SceLocationHandle handle, SceLocationDialogStatus *status);
SceInt32 sceLocationConfirmGetResult(SceLocationHandle handle, SceLocationDialogResult *result);
SceInt32 sceLocationConfirmAbort(SceLocationHandle handle);
SceInt32 sceLocationGetPermission(SceLocationHandle handle, SceLocationPermissionInfo *info);
SceInt32 sceLocationDenyApplication(SceLocationHandle handle);
SceInt32 sceLocationSetThreadParameter(SceInt32 intPriority, SceInt32 cpuAffinityMask);
SceInt32 sceLocationSetGpsEmulationFile(unsigned char *filename);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_LIBLOCATION_H */
