/*
 * touch.h: the touch service's error codes, ports, records and functions, under Sony's names and
 * with Sony's definitions (SDK Release 03.570.011, target/include/touch.h). The touch drivers
 * define the functions.
 */
#ifndef _SCE_TOUCH_TOUCH_H
#define _SCE_TOUCH_TOUCH_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* error codes */
#define SCE_TOUCH_ERROR_INVALID_ARG	-2144010239	/* 0x80350001 */
#define SCE_TOUCH_ERROR_PRIV_REQUIRED	-2144010238	/* 0x80350002 */
#define SCE_TOUCH_ERROR_FATAL		-2144009985	/* 0x803500FF */

/* ports */
#define SCE_TOUCH_PORT_FRONT		0
#define SCE_TOUCH_PORT_BACK		1
#define SCE_TOUCH_PORT_MAX_NUM		2

typedef struct SceTouchPanelInfo {
	SceInt16 minAaX;		/* the active area */
	SceInt16 minAaY;
	SceInt16 maxAaX;
	SceInt16 maxAaY;
	SceInt16 minDispX;		/* the display area */
	SceInt16 minDispY;
	SceInt16 maxDispX;
	SceInt16 maxDispY;
	SceUInt8 minForce;
	SceUInt8 maxForce;
	SceUInt8 rsv[30];
} SceTouchPanelInfo;

int sceTouchGetPanelInfo(SceUInt32 port, SceTouchPanelInfo *pPanelInfo);

/* SceTouchReport's info */
#define SCE_TOUCH_REPORT_INFO_HIDE_UPPER_LAYER	0x0001

typedef struct SceTouchReport {
	SceUInt8 id;
	SceUInt8 force;
	SceInt16 x;
	SceInt16 y;
	SceInt8 rsv[8];
	SceUInt16 info;
} SceTouchReport;

#define SCE_TOUCH_MAX_REPORT		8

/* SceTouchData's status */
#define SCE_TOUCH_STATUS_INTERCEPTED	0x00010000

typedef struct SceTouchData {
	SceUInt64 timeStamp;
	SceUInt32 status;
	SceUInt32 reportNum;
	SceTouchReport report[SCE_TOUCH_MAX_REPORT];
} SceTouchData;

int sceTouchRead(SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs);
int sceTouchPeek(SceUInt32 port, SceTouchData *pData, SceUInt32 nBufs);

/* sampling states */
#define SCE_TOUCH_SAMPLING_STATE_STOP	0
#define SCE_TOUCH_SAMPLING_STATE_START	1

int sceTouchSetSamplingState(SceUInt32 port, SceUInt32 state);
int sceTouchGetSamplingState(SceUInt32 port, SceUInt32 *pState);
int sceTouchEnableTouchForce(SceUInt32 port);
int sceTouchDisableTouchForce(SceUInt32 port);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_TOUCH_TOUCH_H */
