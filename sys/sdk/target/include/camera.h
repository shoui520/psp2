/*
 * camera.h: the camera service's error codes, settings, records and functions, under Sony's names
 * and with Sony's definitions (SDK Release 03.570.011, target/include/camera.h). The camera
 * driver defines the functions.
 */
#ifndef _SCE_CAMERA_CAMERA_H
#define _SCE_CAMERA_CAMERA_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_CAMERA_ERROR_PARAM			-2144468992	/* 0x802E0000 */
#define SCE_CAMERA_ERROR_ALREADY_INIT		-2144468991	/* 0x802E0001 */
#define SCE_CAMERA_ERROR_NOT_INIT		-2144468990	/* 0x802E0002 */
#define SCE_CAMERA_ERROR_ALREADY_OPEN		-2144468989	/* 0x802E0003 */
#define SCE_CAMERA_ERROR_NOT_OPEN		-2144468988	/* 0x802E0004 */
#define SCE_CAMERA_ERROR_ALREADY_START		-2144468987	/* 0x802E0005 */
#define SCE_CAMERA_ERROR_NOT_START		-2144468986	/* 0x802E0006 */
#define SCE_CAMERA_ERROR_FORMAT_UNKNOWN		-2144468985	/* 0x802E0007 */
#define SCE_CAMERA_ERROR_RESOLUTION_UNKNOWN	-2144468984	/* 0x802E0008 */
#define SCE_CAMERA_ERROR_BAD_FRAMERATE		-2144468983	/* 0x802E0009 */
#define SCE_CAMERA_ERROR_TIMEOUT		-2144468982	/* 0x802E000A */
#define SCE_CAMERA_ERROR_EXCLUSIVE		-2144468981	/* 0x802E000B */
#define SCE_CAMERA_ERROR_ATTRIBUTE_UNKNOWN	-2144468980	/* 0x802E000C */
#define SCE_CAMERA_ERROR_MAX_PROCESS		-2144468979	/* 0x802E000D */
#define SCE_CAMERA_ERROR_NOT_ACTIVE		-2144468978	/* 0x802E000E */
#define SCE_CAMERA_ERROR_ALREADY_READ		-2144468977	/* 0x802E000F */
#define SCE_CAMERA_ERROR_NOT_MOUNTED		-2144468976	/* 0x802E0010 */
#define SCE_CAMERA_ERROR_DATA_RANGE_UNKNOWN	-2144468975	/* 0x802E0011 */
#define SCE_CAMERA_ERROR_OTHER_ALREADY_START	-2144468974	/* 0x802E0012 */
#define SCE_CAMERA_ERROR_FATAL			-2144468737	/* 0x802E00FF */

/* Camera Device */
#define SCE_CAMERA_DEVICE_FRONT			0
#define SCE_CAMERA_DEVICE_BACK			1

/* Camera Priority */
#define SCE_CAMERA_PRIORITY_SHARE		0
#define SCE_CAMERA_PRIORITY_EXCLUSIVE		1

/* Camera Format */
#define SCE_CAMERA_FORMAT_UNKNOWN		0
#define SCE_CAMERA_FORMAT_YUV422_PLANE		1
#define SCE_CAMERA_FORMAT_YUV422_PACKED		2
#define SCE_CAMERA_FORMAT_YUV420_PLANE		3
#define SCE_CAMERA_FORMAT_YUV422_TO_ARGB	4
#define SCE_CAMERA_FORMAT_YUV422_TO_ABGR	5
#define SCE_CAMERA_FORMAT_RAW8			6

/* Camera Resolution */
#define SCE_CAMERA_RESOLUTION_UNKNOWN		0
#define SCE_CAMERA_RESOLUTION_VGA		1	/* 640x480 */
#define SCE_CAMERA_RESOLUTION_QVGA		2	/* 320x240 */
#define SCE_CAMERA_RESOLUTION_QQVGA		3	/* 160x120 */
#define SCE_CAMERA_RESOLUTION_CIF		4	/* 352x288 */
#define SCE_CAMERA_RESOLUTION_QCIF		5	/* 176x144 */
#define SCE_CAMERA_RESOLUTION_PSP1		6	/* 480x272 */
#define SCE_CAMERA_RESOLUTION_NGP1		8	/* 640x360 */

/* Camera Framerate */
#define SCE_CAMERA_FRAMERATE_UNKNOWN		0
#define SCE_CAMERA_FRAMERATE_3_75		3
#define SCE_CAMERA_FRAMERATE_5			5
#define SCE_CAMERA_FRAMERATE_7_5		7
#define SCE_CAMERA_FRAMERATE_10			10
#define SCE_CAMERA_FRAMERATE_15			15
#define SCE_CAMERA_FRAMERATE_20			20
#define SCE_CAMERA_FRAMERATE_24			24
#define SCE_CAMERA_FRAMERATE_25			25
#define SCE_CAMERA_FRAMERATE_30			30
#define SCE_CAMERA_FRAMERATE_60			60
#define SCE_CAMERA_FRAMERATE_120		120

/* Camera Data Range */
#define SCE_CAMERA_DATA_RANGE_FULL		0
#define SCE_CAMERA_DATA_RANGE_BT601		1

/* Camera Buffer */
#define SCE_CAMERA_BUFFER_SETBYOPEN		0
#define SCE_CAMERA_BUFFER_SETBYREAD		1

/* Camera Read Mode */
#define SCE_CAMERA_READ_MODE_WAIT_NEXTFRAME_ON	0
#define SCE_CAMERA_READ_MODE_WAIT_NEXTFRAME_OFF	1

/* Camera Read Exposure Time Mode */
#define SCE_CAMERA_READ_GET_EXPOSURE_TIME_OFF	0
#define SCE_CAMERA_READ_GET_EXPOSURE_TIME_ON	1

/* Camera Status */
#define SCE_CAMERA_STATUS_IS_ACTIVE		0
#define SCE_CAMERA_STATUS_IS_NOT_ACTIVE		-1
#define SCE_CAMERA_STATUS_IS_FORCED_STOP	-2
#define SCE_CAMERA_STATUS_IS_FORCED_STOP_POWER_CONFIG_CHANGE	-3
#define SCE_CAMERA_STATUS_IS_ALREADY_READ	1
#define SCE_CAMERA_STATUS_IS_NOT_STABLE		2

/* Camera Raw8 Format Pattern */
#define SCE_CAMERA_RAW8_FORMAT_UNKNOWN		0
#define SCE_CAMERA_RAW8_FORMAT_BGGR		1
#define SCE_CAMERA_RAW8_FORMAT_GRBG		2
#define SCE_CAMERA_RAW8_FORMAT_RGGB		3
#define SCE_CAMERA_RAW8_FORMAT_GBRG		4

/* Camera Attribute */
#define SCE_CAMERA_ATTRIBUTE_SATURATION_0	0
#define SCE_CAMERA_ATTRIBUTE_SATURATION_0_5	5
#define SCE_CAMERA_ATTRIBUTE_SATURATION_1	10
#define SCE_CAMERA_ATTRIBUTE_SATURATION_2	20
#define SCE_CAMERA_ATTRIBUTE_SATURATION_3	30
#define SCE_CAMERA_ATTRIBUTE_SATURATION_4	40
#define SCE_CAMERA_ATTRIBUTE_SHARPNESS_1	1
#define SCE_CAMERA_ATTRIBUTE_SHARPNESS_2	2
#define SCE_CAMERA_ATTRIBUTE_SHARPNESS_3	3
#define SCE_CAMERA_ATTRIBUTE_SHARPNESS_4	4
#define SCE_CAMERA_ATTRIBUTE_REVERSE_OFF	0
#define SCE_CAMERA_ATTRIBUTE_REVERSE_MIRROR	1
#define SCE_CAMERA_ATTRIBUTE_REVERSE_FLIP	2
#define SCE_CAMERA_ATTRIBUTE_REVERSE_MIRROR_FLIP	(SCE_CAMERA_ATTRIBUTE_REVERSE_MIRROR | SCE_CAMERA_ATTRIBUTE_REVERSE_FLIP)
#define SCE_CAMERA_ATTRIBUTE_EFFECT_NORMAL	0
#define SCE_CAMERA_ATTRIBUTE_EFFECT_NEGA	1
#define SCE_CAMERA_ATTRIBUTE_EFFECT_BW		2
#define SCE_CAMERA_ATTRIBUTE_EFFECT_SEPIA	3
#define SCE_CAMERA_ATTRIBUTE_EFFECT_BLUISH	4
#define SCE_CAMERA_ATTRIBUTE_EFFECT_REDISH	5
#define SCE_CAMERA_ATTRIBUTE_EFFECT_GREENISH	6
#define SCE_CAMERA_ATTRIBUTE_EV_PLUS_2		20
#define SCE_CAMERA_ATTRIBUTE_EV_PLUS_1_7	17
#define SCE_CAMERA_ATTRIBUTE_EV_PLUS_1_5	15
#define SCE_CAMERA_ATTRIBUTE_EV_PLUS_1_3	13
#define SCE_CAMERA_ATTRIBUTE_EV_PLUS_1		10
#define SCE_CAMERA_ATTRIBUTE_EV_PLUS_0_7	7
#define SCE_CAMERA_ATTRIBUTE_EV_PLUS_0_5	5
#define SCE_CAMERA_ATTRIBUTE_EV_PLUS_0_3	3
#define SCE_CAMERA_ATTRIBUTE_EV_0		0
#define SCE_CAMERA_ATTRIBUTE_EV_MINUS_0_3	-3
#define SCE_CAMERA_ATTRIBUTE_EV_MINUS_0_5	-5
#define SCE_CAMERA_ATTRIBUTE_EV_MINUS_0_7	-7
#define SCE_CAMERA_ATTRIBUTE_EV_MINUS_1		-10
#define SCE_CAMERA_ATTRIBUTE_EV_MINUS_1_3	-13
#define SCE_CAMERA_ATTRIBUTE_EV_MINUS_1_5	-15
#define SCE_CAMERA_ATTRIBUTE_EV_MINUS_1_7	-17
#define SCE_CAMERA_ATTRIBUTE_EV_MINUS_2		-20
#define SCE_CAMERA_ATTRIBUTE_ANTIFLICKER_AUTO	0
#define SCE_CAMERA_ATTRIBUTE_ANTIFLICKER_50HZ	1
#define SCE_CAMERA_ATTRIBUTE_ANTIFLICKER_60HZ	2

/* Camera Attribute (additional functions) */
#define SCE_CAMERA_ATTRIBUTE_ISO_AUTO		1
#define SCE_CAMERA_ATTRIBUTE_ISO_100		100
#define SCE_CAMERA_ATTRIBUTE_ISO_200		200
#define SCE_CAMERA_ATTRIBUTE_ISO_400		400
#define SCE_CAMERA_ATTRIBUTE_GAIN_AUTO		0
#define SCE_CAMERA_ATTRIBUTE_GAIN_1		1
#define SCE_CAMERA_ATTRIBUTE_GAIN_2		2
#define SCE_CAMERA_ATTRIBUTE_GAIN_3		3
#define SCE_CAMERA_ATTRIBUTE_GAIN_4		4
#define SCE_CAMERA_ATTRIBUTE_GAIN_5		5
#define SCE_CAMERA_ATTRIBUTE_GAIN_6		6
#define SCE_CAMERA_ATTRIBUTE_GAIN_7		7
#define SCE_CAMERA_ATTRIBUTE_GAIN_8		8
#define SCE_CAMERA_ATTRIBUTE_GAIN_9		9
#define SCE_CAMERA_ATTRIBUTE_GAIN_10		10
#define SCE_CAMERA_ATTRIBUTE_GAIN_11		11
#define SCE_CAMERA_ATTRIBUTE_GAIN_12		12
#define SCE_CAMERA_ATTRIBUTE_GAIN_13		13
#define SCE_CAMERA_ATTRIBUTE_GAIN_14		14
#define SCE_CAMERA_ATTRIBUTE_GAIN_15		15
#define SCE_CAMERA_ATTRIBUTE_GAIN_16		16
#define SCE_CAMERA_ATTRIBUTE_WHITEBALANCE_AUTO	0
#define SCE_CAMERA_ATTRIBUTE_WHITEBALANCE_DAY	1
#define SCE_CAMERA_ATTRIBUTE_WHITEBALANCE_CWF	2
#define SCE_CAMERA_ATTRIBUTE_WHITEBALANCE_A	3
#define SCE_CAMERA_ATTRIBUTE_BACKLIGHT_OFF	0
#define SCE_CAMERA_ATTRIBUTE_BACKLIGHT_ON	1
#define SCE_CAMERA_ATTRIBUTE_NIGHTMODE_OFF	0
#define SCE_CAMERA_ATTRIBUTE_NIGHTMODE_LESS10	1
#define SCE_CAMERA_ATTRIBUTE_NIGHTMODE_LESS100	2
#define SCE_CAMERA_ATTRIBUTE_NIGHTMODE_OVER100	3
#define SCE_CAMERA_ATTRIBUTE_AUTOCONTROLHOLD_OFF	0
#define SCE_CAMERA_ATTRIBUTE_AUTOCONTROLHOLD_ON	1
#define SCE_CAMERA_ATTRIBUTE_EXPOSURE_CEILING_NORMAL	0
#define SCE_CAMERA_ATTRIBUTE_EXPOSURE_CEILING_1_2	1

typedef struct SceCameraInfo {
	SceSize sizeThis;

	SceUInt16 wPriority;
	SceUInt16 wFormat;
	SceUInt16 wResolution;
	SceUInt16 wFramerate;
	SceUInt16 wWidth;
	SceUInt16 wHeight;
	SceUInt16 wRange;
	SceUInt16 _padding_0;

	SceSize sizeIBase;
	SceSize sizeUBase;
	SceSize sizeVBase;

	void *pvIBase;
	void *pvUBase;
	void *pvVBase;

	SceUInt16 wPitch;
	SceUInt16 wBuffer;
} SceCameraInfo;

typedef struct SceCameraRead {
	SceSize sizeThis;
	SceInt32 dwMode;
	SceInt32 dwExposureTimeMode;

	SceInt32 dwStatus;
	SceUInt64 qwFrame;
	SceUInt64 qwTimestamp;
	SceUInt32 dwExposureTime;
	SceUInt32 dwExposureTimeGap;
	SceUInt32 dwRaw8Format;
	SceUInt32 _padding_0;

	SceSize sizeIBase;
	SceSize sizeUBase;
	SceSize sizeVBase;

	void *pvIBase;
	void *pvUBase;
	void *pvVBase;
} SceCameraRead;

int sceCameraOpen(int devnum, SceCameraInfo *pInfo);
int sceCameraClose(int devnum);

int sceCameraStart(int devnum);
int sceCameraStop(int devnum);

int sceCameraRead(int devnum, SceCameraRead *pRead);

int sceCameraIsActive(int devnum);
int sceCameraGetDeviceLocation(int devnum, SceFVector3 *pLocation);

/* Camera Attribute (PSP Compatible) */
int sceCameraGetSaturation(int devnum, int *pLevel);
int sceCameraSetSaturation(int devnum, int level);

int sceCameraGetBrightness(int devnum, int *pLevel);
int sceCameraSetBrightness(int devnum, int level);

int sceCameraGetContrast(int devnum, int *pLevel);
int sceCameraSetContrast(int devnum, int level);

int sceCameraGetSharpness(int devnum, int *pLevel);
int sceCameraSetSharpness(int devnum, int level);

int sceCameraGetReverse(int devnum, int *pMode);
int sceCameraSetReverse(int devnum, int mode);

int sceCameraGetEffect(int devnum, int *pMode);
int sceCameraSetEffect(int devnum, int mode);

int sceCameraGetEV(int devnum, int *pLevel);
int sceCameraSetEV(int devnum, int level);

int sceCameraGetZoom(int devnum, int *pLevel);
int sceCameraSetZoom(int devnum, int level);

int sceCameraGetAntiFlicker(int devnum, int *pMode);
int sceCameraSetAntiFlicker(int devnum, int mode);

/* Camera Attribute (additional functions) */
int sceCameraGetISO(int devnum, int *pMode);
int sceCameraSetISO(int devnum, int mode);

int sceCameraGetGain(int devnum, int *pMode);	/* obsolete */
int sceCameraSetGain(int devnum, int mode);	/* obsolete */

int sceCameraGetWhiteBalance(int devnum, int *pMode);
int sceCameraSetWhiteBalance(int devnum, int mode);

int sceCameraGetBacklight(int devnum, int *pMode);
int sceCameraSetBacklight(int devnum, int mode);

int sceCameraGetNightmode(int devnum, int *pMode);
int sceCameraSetNightmode(int devnum, int mode);

int sceCameraGetAutoControlHold(int devnum, int *pMode);
int sceCameraSetAutoControlHold(int devnum, int mode);

int sceCameraGetExposureCeiling(int devnum, int *pMode);
int sceCameraSetExposureCeiling(int devnum, int mode);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_CAMERA_CAMERA_H */
