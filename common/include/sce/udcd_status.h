/*
 * The UDCD state bits and the wait parameter (sceUdcdWaitState). This
 * replaces vitasdk's psp2common/udcd.h, whose include guard it takes, so it
 * must come before any vitasdk udcd header. Layouts and constant names are
 * vitasdk's; what vitasdk leaves as offsets is named from SceUdcd's own use.
 */

#pragma once

#ifdef _PSP2COMMON_UDCD_H_
#error "include sce/udcd_status.h before vitasdk's psp2common/udcd.h"
#endif
#define _PSP2COMMON_UDCD_H_

#include <psp2common/types.h>

/* the state event flag's bits, one group per SceUdcdWaitParam word */
typedef enum SceUdcdStatus {
	SCE_UDCD_STATUS_CONNECTION_NEW		= 0x0001,
	SCE_UDCD_STATUS_CONNECTION_ESTABLISHED	= 0x0002,
	SCE_UDCD_STATUS_CONNECTION_SUSPENDED	= 0x0004,
	SCE_UDCD_STATUS_CABLE_DISCONNECTED	= 0x0010,
	SCE_UDCD_STATUS_CABLE_CONNECTED		= 0x0020,
	SCE_UDCD_STATUS_DEACTIVATED		= 0x0100,
	SCE_UDCD_STATUS_ACTIVATED		= 0x0200,
	SCE_UDCD_STATUS_IS_CHARGING		= 0x0400,
	SCE_UDCD_STATUS_USE_USB_CHARGING	= 0x0800,
	SCE_UDCD_STATUS_CONTROLLER_STOPPED	= 0x1000,	/* vitasdk UNKNOWN_1000 */
	SCE_UDCD_STATUS_CONTROLLER_STARTED	= 0x2000	/* vitasdk UNKNOWN_2000 */
} SceUdcdStatus;

typedef enum SceUdcdStatusDriver {
	SCE_UDCD_STATUS_DRIVER_STARTED		= 0x01,
	SCE_UDCD_STATUS_DRIVER_REGISTERED	= 0x02
} SceUdcdStatusDriver;

typedef struct SceUdcdDeviceInfo {
	unsigned char info[64];
} SceUdcdDeviceInfo;

/* wait until any of the given state bits is set (and driverName, if any, runs) */
typedef struct SceUdcdWaitParam {
	int controller;			/* SCE_UDCD_STATUS_CONTROLLER_* */
	int status;			/* DEACTIVATED, ACTIVATED, IS_CHARGING */
	int cable;			/* SCE_UDCD_STATUS_CABLE_* */
	int connection;			/* SCE_UDCD_STATUS_CONNECTION_* */
	int charging;			/* USE_USB_CHARGING; not waited on */
	const char *driverName;
} SceUdcdWaitParam;

_Static_assert(sizeof(SceUdcdDeviceInfo) == 0x40, "SceUdcdDeviceInfo");
_Static_assert(sizeof(SceUdcdWaitParam) == 0x18, "SceUdcdWaitParam");
