/*
 * ctrl.h: the controller service's error codes, buttons, sampling modes and samples, under Sony's
 * names and with Sony's definitions (SDK Release 03.570.011, target/include/ctrl.h). The kernel
 * takes these; the user library's functions come with the user-mode SDK headers.
 */
#ifndef _SCE_CTRL_CTRL_H
#define _SCE_CTRL_CTRL_H

#include <scetypes.h>

#if !defined(_LANGUAGE_ASSEMBLY)

#ifdef __cplusplus
extern "C" {
#endif

/* error codes */
#define SCE_CTRL_ERROR_INVALID_ARG	-2144075775	/* 0x80340001 */
#define SCE_CTRL_ERROR_PRIV_REQUIRED	-2144075774	/* 0x80340002 */
#define SCE_CTRL_ERROR_NO_DEVICE	-2144075744	/* 0x80340020 */
#define SCE_CTRL_ERROR_NOT_SUPPORTED	-2144075743	/* 0x80340021 */
#define SCE_CTRL_ERROR_INVALID_MODE	-2144075742	/* 0x80340022 */
#define SCE_CTRL_ERROR_FATAL		-2144075521	/* 0x803400FF */

/* buttons */
#define SCE_CTRL_SELECT		(1<<0)
#define SCE_CTRL_L3		(1<<1)
#define SCE_CTRL_R3		(1<<2)
#define SCE_CTRL_START		(1<<3)
#define SCE_CTRL_UP		(1<<4)
#define SCE_CTRL_RIGHT		(1<<5)
#define SCE_CTRL_DOWN		(1<<6)
#define SCE_CTRL_LEFT		(1<<7)
#define SCE_CTRL_L		(1<<8)
#define SCE_CTRL_R		(1<<9)
#define SCE_CTRL_L2		(1<<8)
#define SCE_CTRL_R2		(1<<9)
#define SCE_CTRL_L1		(1<<10)
#define SCE_CTRL_R1		(1<<11)
#define SCE_CTRL_TRIANGLE	(1<<12)
#define SCE_CTRL_CIRCLE		(1<<13)
#define SCE_CTRL_CROSS		(1<<14)
#define SCE_CTRL_SQUARE		(1<<15)
#define SCE_CTRL_INTERCEPTED	(1<<16)	/* input intercepted: the system has the PS button */

/* the analog sticks' sampling modes */
#define SCE_CTRL_MODE_DIGITALONLY		0
#define SCE_CTRL_MODE_DIGITALANALOG		1
#define SCE_CTRL_MODE_DIGITALANALOG_WIDE	2	/* wide range */

/* a sample */
typedef struct SceCtrlData {
	SceUInt64 timeStamp;	/* usec */
	SceUInt32 buttons;
	SceUInt8 lx;
	SceUInt8 ly;
	SceUInt8 rx;
	SceUInt8 ry;
	SceUInt8 rsrv[16];
} SceCtrlData;

/* a sample with the buttons' pressure */
typedef struct SceCtrlData2 {
	SceUInt64 timeStamp;	/* usec */
	SceUInt32 buttons;
	SceUInt8 lx;
	SceUInt8 ly;
	SceUInt8 rx;
	SceUInt8 ry;
	SceUInt8 up;
	SceUInt8 right;
	SceUInt8 down;
	SceUInt8 left;
	SceUInt8 l2;
	SceUInt8 r2;
	SceUInt8 l1;
	SceUInt8 r1;
	SceUInt8 triangle;
	SceUInt8 circle;
	SceUInt8 cross;
	SceUInt8 square;
	SceUInt8 rsrv[4];
} SceCtrlData2;

/* a rapid-fire rule: while (buttons & uiMask) == uiTrigger, uiTarget fires */
typedef struct SceCtrlRapidFireRule {
	SceUInt32 uiMask;
	SceUInt32 uiTrigger;
	SceUInt32 uiTarget;
	SceUInt32 uiDelay;	/* samples before the first */
	SceUInt32 uiMake;	/* samples pressed */
	SceUInt32 uiBreak;	/* samples released */
} SceCtrlRapidFireRule;

#define SCE_CTRL_MAX_WIRELESS_NUM		(4)
#define SCE_CTRL_WIRELESS_INFO_NOT_CONNECTED	(0)
#define SCE_CTRL_WIRELESS_INFO_CONNECTED	(1)

typedef struct SceCtrlWirelessControllerInfo {
	SceUInt8 connected[SCE_CTRL_MAX_WIRELESS_NUM];	/* SCE_CTRL_WIRELESS_INFO_* */
	SceUInt8 reserved[12];
} SceCtrlWirelessControllerInfo;

#ifdef __cplusplus
}
#endif

#endif /* !_LANGUAGE_ASSEMBLY */

#endif /* _SCE_CTRL_CTRL_H */
