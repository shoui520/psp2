/*
 * remote_usb.c: Kermit remote USB device proxy (PSP Chotto Shot camera + mic,
 * PSP GPS via SceLibLocation, and USB-PSPCM lifecycle) for ScePspemu.
 */
#include "host_common.h"
#include <string.h>
#include <stdlib.h>

#define PSPEMU_USB_CHANNEL                 12
#define PSPEMU_USB_PID_PSPCM               0x1CB
#define PSPEMU_USB_PID_CAM                 0x282
#define PSPEMU_USB_PID_GPS                 0x283

#define PSPEMU_USB_ERR_ALREADY_ACTIVATED   ((int)0x80243001)
#define PSPEMU_USB_ERR_INVALID_PID         ((int)0x80243002)
#define PSPEMU_USB_ERR_NOT_ACTIVATED       ((int)0x80243007)
#define PSPEMU_USB_ERR_PSPCM_NOT_SUPPORTED ((int)0x80243407)
#define PSPEMU_USB_ERR_CAM_NOT_STARTED     ((int)0x8024380C)
#define PSPEMU_USB_ERR_CAM_NOT_OPEN        ((int)0x80243901)
#define PSPEMU_USB_ERR_CAM_NO_DEVICE       ((int)0x80243902)
#define PSPEMU_USB_ERR_CAM_INVALID_SIZE    ((int)0x80243903)
#define PSPEMU_USB_ERR_CAM_INVALID_MODE    ((int)0x80243905)
#define PSPEMU_USB_ERR_CAM_INVALID_RANGE   ((int)0x80243906)
#define PSPEMU_USB_ERR_CAM_INVALID_PARAM   ((int)0x80243907)
#define PSPEMU_USB_ERR_CAM_NOT_READY       ((int)0x80243908)
#define PSPEMU_USB_ERR_CAM_MIC_RATE        ((int)0x8024390B)
#define PSPEMU_USB_ERR_CAM_NOT_ACTIVE      ((int)0x80243812)
#define PSPEMU_USB_ERR_GPS_NOT_OPEN        ((int)0x80244002)
#define PSPEMU_USB_ERR_GPS_ALREADY_OPEN    ((int)0x80244003)
#define PSPEMU_USB_ERR_GPS_IO              ((int)0x80244004)
#define PSPEMU_USB_ERR_GPS_FAIL            ((int)0x80244010)
#define PSPEMU_USB_ERR_ALIGNMENT           ((int)0x80020007)
#define PSPEMU_USB_COMPAT_INTR_RETRY       ((int)0x80010086)

#define PSPEMU_CAM_DIRTY_WB                0x001U
#define PSPEMU_CAM_DIRTY_SATURATION        0x002U
#define PSPEMU_CAM_DIRTY_BRIGHTNESS        0x004U
#define PSPEMU_CAM_DIRTY_CONTRAST          0x008U
#define PSPEMU_CAM_DIRTY_SHARPNESS         0x010U
#define PSPEMU_CAM_DIRTY_EFFECT            0x020U
#define PSPEMU_CAM_DIRTY_ISO               0x040U
#define PSPEMU_CAM_DIRTY_EV                0x080U
#define PSPEMU_CAM_DIRTY_ZOOM              0x100U

typedef struct PspemuCamResEntry {
	uint16_t vitaResCode;
	uint16_t width;
	uint16_t height;
} PspemuCamResEntry;

typedef struct PspemuUsbCamVideoParam {
	int32_t  size;
	uint32_t resolution;
	uint32_t framerate;
	uint32_t whiteBalance;
	int32_t  saturation;
	uint32_t brightness;
	uint32_t contrast;
	uint32_t sharpness;
	uint32_t effectMode;
	uint32_t frameSize;
	uint32_t isoLevel;
	uint32_t evLevel;
	int32_t  zoom;
} PspemuUsbCamVideoParam;

typedef struct PspemuUsbCamStillParam {
	int32_t  size;
	uint32_t resolution;
	uint32_t jpegSize;
	int32_t  reverseFlags;
	uint32_t delay;
	uint32_t compLevel;
} PspemuUsbCamStillParam;

typedef struct PspemuUsbCamMicParam {
	int32_t size;
	int32_t alc;
	int32_t gain;
	int32_t noiseReduction;
	int32_t freq;
} PspemuUsbCamMicParam;

typedef struct PspemuGpsData {
	uint16_t year;
	uint16_t month;
	uint16_t day;
	uint16_t hour;
	uint16_t minute;
	uint16_t second;
	uint32_t microsecond;
	float    hdop;
	uint8_t  valid;
	uint8_t  fixMode;
	uint16_t padStatus;
	float    latitude;
	float    longitude;
	float    altitude;
	int32_t  garbage;
	float    speed;
	float    bearing;
} PspemuGpsData;

typedef struct PspemuGpsSatEntry {
	uint8_t  id;
	uint8_t  elevation;
	uint16_t azimuth;
	uint8_t  snr;
	uint8_t  good;
	uint16_t pad;
} PspemuGpsSatEntry;

typedef struct PspemuGpsSatInfo {
	uint32_t          satCount;
	PspemuGpsSatEntry sats[32];
} PspemuGpsSatInfo;

_Static_assert(sizeof(PspemuGpsData) == 48, "PspemuGpsData size");
_Static_assert(sizeof(PspemuGpsSatInfo) == 260, "PspemuGpsSatInfo size");
_Static_assert(sizeof(PspemuUsbCamVideoParam) == 52, "PspemuUsbCamVideoParam size");
_Static_assert(sizeof(PspemuUsbCamStillParam) == 24, "PspemuUsbCamStillParam size");
_Static_assert(sizeof(PspemuUsbCamMicParam) == 20, "PspemuUsbCamMicParam size");

static const PspemuCamResEntry g_usbCamResTable[9] = {
	{ SCE_CAMERA_RESOLUTION_QQVGA, 160U, 120U },
	{ SCE_CAMERA_RESOLUTION_QCIF,  176U, 144U },
	{ SCE_CAMERA_RESOLUTION_QVGA,  320U, 240U },
	{ SCE_CAMERA_RESOLUTION_CIF,   352U, 288U },
	{ SCE_CAMERA_RESOLUTION_VGA,   640U, 480U },
	{ SCE_CAMERA_RESOLUTION_UNKNOWN, 0U,   0U },
	{ SCE_CAMERA_RESOLUTION_UNKNOWN, 0U,   0U },
	{ SCE_CAMERA_RESOLUTION_PSP1,  480U, 272U },
	{ SCE_CAMERA_RESOLUTION_UNKNOWN, 0U,   0U },
};

static const int32_t g_usbCamFpsTable[8] = {
	SCE_CAMERA_FRAMERATE_3_75,
	SCE_CAMERA_FRAMERATE_5,
	SCE_CAMERA_FRAMERATE_7_5,
	SCE_CAMERA_FRAMERATE_10,
	SCE_CAMERA_FRAMERATE_15,
	SCE_CAMERA_FRAMERATE_20,
	SCE_CAMERA_FRAMERATE_30,
	SCE_CAMERA_FRAMERATE_60,
};

static const int32_t g_usbCamEffectTable[7] = {
	SCE_CAMERA_ATTRIBUTE_EFFECT_NORMAL,
	SCE_CAMERA_ATTRIBUTE_EFFECT_NEGA,
	SCE_CAMERA_ATTRIBUTE_EFFECT_BW,
	SCE_CAMERA_ATTRIBUTE_EFFECT_SEPIA,
	SCE_CAMERA_ATTRIBUTE_EFFECT_BLUISH,
	SCE_CAMERA_ATTRIBUTE_EFFECT_REDISH,
	SCE_CAMERA_ATTRIBUTE_EFFECT_GREENISH,
};

static const int32_t g_usbCamWhiteBalanceTable[4] = {
	SCE_CAMERA_ATTRIBUTE_WHITEBALANCE_AUTO,
	SCE_CAMERA_ATTRIBUTE_WHITEBALANCE_DAY,
	SCE_CAMERA_ATTRIBUTE_WHITEBALANCE_CWF,
	SCE_CAMERA_ATTRIBUTE_WHITEBALANCE_A,
};

static const int32_t g_usbCamAntiFlickerTable[3] = {
	SCE_CAMERA_ATTRIBUTE_ANTIFLICKER_AUTO,
	SCE_CAMERA_ATTRIBUTE_ANTIFLICKER_60HZ,
	SCE_CAMERA_ATTRIBUTE_ANTIFLICKER_50HZ,
};

static const int32_t g_usbCamSharpnessTable[4] = {
	SCE_CAMERA_ATTRIBUTE_SHARPNESS_1,
	SCE_CAMERA_ATTRIBUTE_SHARPNESS_2,
	SCE_CAMERA_ATTRIBUTE_SHARPNESS_3,
	SCE_CAMERA_ATTRIBUTE_SHARPNESS_4,
};

static const PspemuGpsSatInfo g_usbGpsDefaultSatInfo = {
	5U,
	{
		{ 12U, 36U,  42U, 22U, 1U, 0U },
		{ 14U, 77U, 331U, 20U, 1U, 0U },
		{ 22U, 31U, 217U, 27U, 1U, 0U },
		{ 25U, 75U,  40U, 18U, 1U, 0U },
		{ 29U, 29U, 136U, 21U, 1U, 0U },
	}
};

static const int32_t g_usbCamEvTable[17] = {
	20, 17, 15, 13, 10, 7, 5, 3, 0, -3, -5, -7, -10, -13, -15, -17, -20,
};

static const int32_t g_usbCamSaturationTable[6] = {
	0, 5, 10, 20, 30, 40,
};

static const int32_t g_usbCamIsoTable[4] = {
	1, 100, 200, 400,
};

static int g_usbCamMicReadBytes;
static int16_t g_usbCamResampleOut[256][2];
static int g_usbCamAutoReverseOff;
static int g_usbCamStreaming;
static SceKernelLwMutexWork g_usbMutex;
static void *g_usbWorkBuf;
static int g_usbCamMicPort;
static uint16_t g_usbCamHeight;
static SceUID g_usbLocationConfirmThreadId;
static SceCameraInfo g_usbCameraInfo;
static int g_usbGpsActive;
static void *g_usbZoomWorkBuf;
static int g_usbCamVideoReverse;
static uint32_t g_usbCamDirtyFlags;
static uint16_t g_usbCamWidth;
static SceJpegArmEncoderContext g_usbJpegEncCtx;
static int g_usbCamFramerate;
static int g_usbCamWhiteBalance;
static int g_usbCamSaturation;
static int g_usbCamBrightness;
static int g_usbCamContrast;
static int g_usbCamSharpness;
static int g_usbCamEffect;
static int g_usbCamIso;
static int g_usbCamEv;
static int g_usbCamZoom;
static int g_usbCamDigitalZoom;
static PspemuUsbCamMicParam g_usbCamMicParam;
static int g_usbCamLastJpegSize;
static int g_usbPspcmEnabled;
static int g_usbCamSwitchPending;
static int16_t g_usbCamMicPcm[256][2];
static void *g_usbCamWorkBuf;
static int g_usbCamStillMode;
static int g_usbCamAppliedReverse;
static SceUInt64 g_usbCamLastFrameTime;
static PspemuAudioResampler g_usbCamResampler;
static uint8_t g_usbCamActive;
static int g_usbCamDevice;
static SceLocationHandle g_usbLocationHandle;
static int g_usbCamStillReverse;

static int remoteUsbCamSetSaturation(int value)
{
	uint32_t idx = ((uint32_t)(value * 3) >> 7) & 0xFFFFFFU;

	if (idx > 5U)
		return 0;
	if (g_usbCamSaturation != g_usbCamSaturationTable[idx]) {
		g_usbCamSaturation = g_usbCamSaturationTable[idx];
		g_usbCamDirtyFlags |= PSPEMU_CAM_DIRTY_SATURATION;
	}
	return 1;
}

static int remoteUsbCamSetSharpness(unsigned int value)
{
	uint32_t idx = (value >> 6) & 0xFFFFFFU;

	if (idx > 3U)
		return 0;
	if (g_usbCamSharpness != g_usbCamSharpnessTable[idx]) {
		g_usbCamSharpness = g_usbCamSharpnessTable[idx];
		g_usbCamDirtyFlags |= PSPEMU_CAM_DIRTY_SHARPNESS;
	}
	return 1;
}

static int __attribute__((noinline, noclone)) remoteUsbCamApplySetter(int (*setter)(int),
                                                                      const PspemuKermitRequest *req)
{
	if (!pspemuHasCameraCapability())
		return PSPEMU_USB_ERR_CAM_NO_DEVICE;
	if (g_usbCamActive != 1U)
		return PSPEMU_USB_ERR_CAM_NOT_READY;
	if (setter((int)req->args[0]) != 0)
		return 0;
	return PSPEMU_USB_ERR_CAM_INVALID_PARAM;
}

static uint32_t remoteUsbCamBuildInfo(uint16_t resCode)
{
	uint32_t alignedWidth = ((uint32_t)g_usbCamWidth + 31U) & ~31U;
	uint32_t alignedHeight = ((uint32_t)g_usbCamHeight + 15U) & ~15U;
	uint32_t ySize = alignedWidth * alignedHeight;
	uint32_t uvSize = ySize >> 2;
	uint8_t *base = (uint8_t *)g_usbCamWorkBuf;

	g_usbCameraInfo.sizeThis = sizeof(SceCameraInfo);
	g_usbCameraInfo.wPriority = SCE_CAMERA_PRIORITY_SHARE;
	g_usbCameraInfo.wFormat = SCE_CAMERA_FORMAT_YUV420_PLANE;
	g_usbCameraInfo.wResolution = resCode;
	g_usbCameraInfo.wFramerate = (uint16_t)g_usbCamFramerate;
	g_usbCameraInfo.wWidth = 0;
	g_usbCameraInfo.wHeight = 0;
	g_usbCameraInfo.wRange = 0;
	g_usbCameraInfo.sizeIBase = ySize;
	g_usbCameraInfo.sizeUBase = uvSize;
	g_usbCameraInfo.sizeVBase = uvSize;
	g_usbCameraInfo.pvIBase = base;
	g_usbCameraInfo.pvUBase = &base[ySize];
	g_usbCameraInfo.pvVBase = &base[ySize + uvSize];
	g_usbCameraInfo.wPitch = (uint16_t)(alignedWidth - g_usbCamWidth);
	g_usbCameraInfo.wBuffer = 0;
	return uvSize;
}

static int remoteUsbCamStopDevice(void)
{
	g_usbCamStreaming = 0;
	sceCameraStop(g_usbCamDevice);
	sceCameraClose(g_usbCamDevice);
	return sceJpegArmEncoderEnd(g_usbJpegEncCtx);
}

static int remoteUsbLocationConfirmThread(SceSize argSize, void *argp)
{
	SceLocationHandle handle;
	SceLocationDialogStatus status;
	SceLocationDialogResult dlgResult;
	int ret;

	if (argSize != sizeof(SceLocationHandle))
		return PSPEMU_ERROR_INVALID_ARG;

	handle = *(const SceLocationHandle *)argp;
	ret = sceLocationConfirm(handle);
	if (ret != 0)
		return 0;

	while (sceLocationConfirmGetStatus(handle, &status) == 0) {
		if (status != SCE_LOCATION_DIALOG_STATUS_RUNNING) {
			if (sceLocationConfirmGetResult(handle, &dlgResult) == 0) {
				while (sceCompatInterrupt(12) == PSPEMU_USB_COMPAT_INTR_RETRY)
					sceKernelDelayThread(10000U);
			}
			break;
		}
		sceKernelDelayThread(500000U);
	}
	return 0;
}

static int remoteUsbCamTranslateError(int vitaErr)
{
	switch (vitaErr) {
	case SCE_CAMERA_ERROR_PARAM:
	case SCE_CAMERA_ERROR_RESOLUTION_UNKNOWN:
	case SCE_CAMERA_ERROR_BAD_FRAMERATE:
	case SCE_CAMERA_ERROR_DATA_RANGE_UNKNOWN:
		return PSPEMU_USB_ERR_CAM_INVALID_PARAM;
	case SCE_CAMERA_ERROR_NOT_INIT:
	case SCE_CAMERA_ERROR_NOT_OPEN:
		return PSPEMU_USB_ERR_CAM_NOT_OPEN;
	case SCE_CAMERA_ERROR_NOT_START:
		return PSPEMU_USB_ERR_CAM_NOT_ACTIVE;
	default:
		return PSPEMU_USB_ERR_CAM_NO_DEVICE;
	}
}

static int remoteUsbCamSetZoom(int value)
{
	if ((unsigned int)(value - 10) > 70U)
		return 0;

	if (g_usbCamWidth == 480U && g_usbCamHeight == 272U &&
	    (unsigned int)(value - 14) <= 14U) {
		if (g_usbCamZoom != 10) {
			g_usbCamZoom = 10;
			g_usbCamDirtyFlags |= PSPEMU_CAM_DIRTY_ZOOM;
		}
		g_usbCamDigitalZoom = value;
		return 1;
	}

	if (g_usbCamZoom != value) {
		g_usbCamZoom = value;
		g_usbCamDirtyFlags |= PSPEMU_CAM_DIRTY_ZOOM;
	}
	g_usbCamDigitalZoom = 0;
	return 1;
}

static int remoteUsbCamStartDevice(void)
{
	int ret;
	uint32_t alignedWidth;
	uint32_t alignedHeight;

	ret = sceCameraOpen(g_usbCamDevice, &g_usbCameraInfo);
	if (ret < 0)
		return ret;

	ret = sceCameraStart(g_usbCamDevice);
	if (ret >= 0) {
		alignedWidth = ((uint32_t)g_usbCamWidth + 31U) & ~31U;
		alignedHeight = ((uint32_t)g_usbCamHeight + 15U) & ~15U;
		ret = sceJpegArmEncoderInit(g_usbJpegEncCtx, alignedWidth, alignedHeight,
		                            SCE_JPEGENC_PIXEL_YCBCR420, NULL, 0);
		if (ret >= 0) {
			if (alignedWidth != g_usbCamWidth || alignedHeight != g_usbCamHeight) {
				ret = sceJpegArmEncoderSetValidRegion(g_usbJpegEncCtx,
				                                      (SceInt32)g_usbCamWidth,
				                                      (SceInt32)g_usbCamHeight);
			}
			if (ret >= 0) {
				ret = sceJpegArmEncoderSetHeaderMode(g_usbJpegEncCtx, SCE_JPEGENC_MODE_JPEG);
				if (ret >= 0) {
					g_usbCamDirtyFlags = 0xFFFFFFFFU;
					g_usbCamStreaming = 1;
					return 0;
				}
			}
		}
		sceCameraStop(g_usbCamDevice);
	}
	sceCameraClose(g_usbCamDevice);
	return ret;
}

static int remoteUsbAllocAlignedCdram(void **outBase, const char *name)
{
	SceKernelAllocMemBlockOpt opt;
	int ret;

	opt.size = sizeof(opt);
	opt.attr = 4U;
	opt.alignment = 0x100000U;
	opt.reserved = 0;
	opt.strBaseBlockName = NULL;

	ret = sceKernelAllocMemBlock(name, PSPEMU_MEMBLOCK_CDRAM_RW, 0x100000U, &opt);
	if (ret < 0)
		return ret;

	ret = sceKernelGetMemBlockBase(ret, outBase);
	if (ret == 0) {
		if (((uintptr_t)*outBase & 0xFFFFFU) != 0U)
			return PSPEMU_USB_ERR_ALIGNMENT;
		return 0;
	}
	return ret;
}

static int remoteUsbCamSetContrast(unsigned int value)
{
	if (value > 0xFFU)
		return 0;
	if ((int)value != g_usbCamContrast) {
		g_usbCamContrast = (int)value;
		g_usbCamDirtyFlags |= PSPEMU_CAM_DIRTY_CONTRAST;
	}
	return 1;
}

static int remoteUsbCamSetBrightness(unsigned int value)
{
	if (value > 0xFFU)
		return 0;
	if ((int)value != g_usbCamBrightness) {
		g_usbCamBrightness = (int)value;
		g_usbCamDirtyFlags |= PSPEMU_CAM_DIRTY_BRIGHTNESS;
	}
	return 1;
}

static int remoteUsbCamSetEv(unsigned int value)
{
	if (value > 16U)
		return 0;
	if (g_usbCamEv != g_usbCamEvTable[value]) {
		g_usbCamEv = g_usbCamEvTable[value];
		g_usbCamDirtyFlags |= PSPEMU_CAM_DIRTY_EV;
	}
	return 1;
}

static int remoteUsbCamSetEffect(unsigned int value)
{
	if (value > 6U)
		return 0;
	if (g_usbCamEffect != g_usbCamEffectTable[value]) {
		g_usbCamEffect = g_usbCamEffectTable[value];
		g_usbCamDirtyFlags |= PSPEMU_CAM_DIRTY_EFFECT;
	}
	return 1;
}

static int *remoteUsbCamResetState(void)
{
	g_usbCamActive = 0U;
	if (sceKernelLockLwMutex(&g_usbMutex, 1, NULL) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	g_usbCamDevice = pspemuCompatGetDefaultCameraDevice();
	g_usbCamSwitchPending = 0;
	if (sceKernelUnlockLwMutex(&g_usbMutex, 1) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	g_usbCamStillReverse = 0;
	g_usbCamVideoReverse = 0;
	g_usbCamAppliedReverse = -1;
	g_usbCamLastJpegSize = 0;
	g_usbCamAutoReverseOff = 0;
	g_usbCamStillMode = 0;
	g_usbCamDirtyFlags = 0U;
	g_usbCamStreaming = 0;
	return &g_usbCamAutoReverseOff;
}

static int remoteUsbCamCaptureFrame(SceSize outBufSize)
{
	SceCameraRead camRead;
	int reverseMode;
	int useFallback = 0;
	const void *encodeSrc;
	int ret;

	memset(&camRead, 0, sizeof(camRead));
	camRead.sizeThis = sizeof(SceCameraRead);

	if (sceKernelLockLwMutex(&g_usbMutex, 1, NULL) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	if (g_usbCamSwitchPending != 0) {
		ret = remoteUsbCamStopDevice();
		if (ret < 0) {
			if (sceKernelUnlockLwMutex(&g_usbMutex, 1) < 0)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			return ret;
		}
		g_usbCamAppliedReverse = -1;
		g_usbCamSwitchPending = 0;
		g_usbCamDevice = (g_usbCamDevice != SCE_CAMERA_DEVICE_BACK) ?
		                 SCE_CAMERA_DEVICE_BACK : SCE_CAMERA_DEVICE_FRONT;
		ret = remoteUsbCamStartDevice();
		if (ret < 0) {
			if (sceKernelUnlockLwMutex(&g_usbMutex, 1) < 0)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			return ret;
		}
	}

	if (sceKernelUnlockLwMutex(&g_usbMutex, 1) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	if (g_usbCamStillMode == 0 && g_usbCamAutoReverseOff != 0) {
		reverseMode = SCE_CAMERA_ATTRIBUTE_REVERSE_OFF;
	} else {
		unsigned int rawFlags;
		if (g_usbCamStillMode != 0) {
			rawFlags = (unsigned int)g_usbCamStillReverse;
			if (g_usbCamDevice == SCE_CAMERA_DEVICE_BACK)
				rawFlags ^= 0x101U;
		} else {
			rawFlags = (unsigned int)g_usbCamVideoReverse ^ 0x100U;
			if (g_usbCamDevice == SCE_CAMERA_DEVICE_BACK)
				rawFlags ^= 1U;
		}

		switch (rawFlags) {
		case 0x100U:
			reverseMode = SCE_CAMERA_ATTRIBUTE_REVERSE_MIRROR;
			break;
		case 0x101U:
			reverseMode = SCE_CAMERA_ATTRIBUTE_REVERSE_MIRROR_FLIP;
			break;
		case 0x001U:
			reverseMode = SCE_CAMERA_ATTRIBUTE_REVERSE_FLIP;
			break;
		default:
			reverseMode = SCE_CAMERA_ATTRIBUTE_REVERSE_OFF;
			break;
		}
	}

	if (g_usbCamAppliedReverse != reverseMode) {
		g_usbCamAppliedReverse = reverseMode;
		ret = sceCameraSetReverse(g_usbCamDevice, reverseMode);
		if (ret < 0)
			return ret;
	}

	if (pspemuCompatHasFlag(0x10000U) != 0 && remoteMeAvcdecHasDecoded() != 0) {
		uint32_t delayUsec = 33333U;
		if (g_usbCameraInfo.wFramerate != 0U)
			delayUsec = 1000000U / g_usbCameraInfo.wFramerate;
		sceKernelDelayThread(delayUsec);
		return 0;
	}

	{
		uint32_t dirty = g_usbCamDirtyFlags;

		if ((dirty & PSPEMU_CAM_DIRTY_EFFECT) != 0U &&
		    sceCameraSetEffect(g_usbCamDevice, g_usbCamEffect) < 0)
			useFallback = 1;
		if (!useFallback && (dirty & PSPEMU_CAM_DIRTY_WB) != 0U &&
		    sceCameraSetWhiteBalance(g_usbCamDevice, g_usbCamWhiteBalance) < 0)
			useFallback = 1;
		if (!useFallback && (dirty & PSPEMU_CAM_DIRTY_SATURATION) != 0U &&
		    sceCameraSetSaturation(g_usbCamDevice, g_usbCamSaturation) < 0)
			useFallback = 1;
		if (!useFallback && (dirty & PSPEMU_CAM_DIRTY_BRIGHTNESS) != 0U &&
		    sceCameraSetBrightness(g_usbCamDevice, g_usbCamBrightness) < 0)
			useFallback = 1;
		if (!useFallback && (dirty & PSPEMU_CAM_DIRTY_CONTRAST) != 0U &&
		    sceCameraSetContrast(g_usbCamDevice, g_usbCamContrast) < 0)
			useFallback = 1;
		if (!useFallback && (dirty & PSPEMU_CAM_DIRTY_SHARPNESS) != 0U &&
		    sceCameraSetSharpness(g_usbCamDevice, g_usbCamSharpness) < 0)
			useFallback = 1;
		if (!useFallback && (dirty & PSPEMU_CAM_DIRTY_ZOOM) != 0U &&
		    sceCameraSetZoom(g_usbCamDevice, g_usbCamZoom) < 0)
			useFallback = 1;
		if (!useFallback && (dirty & PSPEMU_CAM_DIRTY_ISO) != 0U &&
		    sceCameraSetISO(g_usbCamDevice, g_usbCamIso) < 0)
			useFallback = 1;
		if (!useFallback && (dirty & PSPEMU_CAM_DIRTY_EV) != 0U &&
		    sceCameraSetEV(g_usbCamDevice, g_usbCamEv) < 0)
			useFallback = 1;

		g_usbCamDirtyFlags = 0U;

		if (!useFallback) {
			int tries = 30;
			int readOk = 0;

			while (tries > 0) {
				if (sceCameraRead(g_usbCamDevice, &camRead) == 0) {
					if (camRead.dwStatus == SCE_CAMERA_STATUS_IS_ACTIVE) {
						readOk = 1;
						break;
					}
					sceKernelDelayThread(1000U);
				}
				tries--;
			}
			if (!readOk)
				useFallback = 1;
		}
	}

	if (useFallback) {
		ret = avcdecClearEdramFrame(g_usbZoomWorkBuf, g_usbCamWidth, g_usbCamHeight);
		if (ret < 0)
			return ret;
		encodeSrc = g_usbZoomWorkBuf;
		if (g_usbCameraInfo.wFramerate != 0U) {
			SceUInt64 elapsed = sceKernelGetProcessTimeWide() - g_usbCamLastFrameTime;
			uint32_t framePeriod = 1000000U / g_usbCameraInfo.wFramerate;
			if (elapsed < (SceUInt64)framePeriod)
				sceKernelDelayThread((SceUInt)((SceUInt64)framePeriod - elapsed));
		}
	} else {
		encodeSrc = g_usbCameraInfo.pvIBase;
		if (g_usbCamDigitalZoom != 0 &&
		    remoteMeAvcdecZoomFrame(g_usbZoomWorkBuf, g_usbCameraInfo.pvIBase, g_usbCamDigitalZoom) >= 0) {
			encodeSrc = g_usbZoomWorkBuf;
		}
	}

	g_usbCamLastFrameTime = sceKernelGetProcessTimeWide();
	ret = sceJpegArmEncoderSetOutputAddr(g_usbJpegEncCtx, g_usbWorkBuf, outBufSize);
	if (ret < 0)
		return ret;

	ret = sceJpegArmEncoderEncode(g_usbJpegEncCtx, encodeSrc);
	if (ret > 0) {
		remotefsFlushGuestAddr(g_usbWorkBuf, (SceSize)ret);
		g_usbCamLastJpegSize = ret;
	}
	return ret;
}

static int remoteUsbThread(SceSize argSize, void *argp)
{
	PspemuKermitRequest *req;

	(void)argSize;
	(void)argp;

	for (;;) {
		int cmd = remotefsWaitAndGetRequest(PSPEMU_USB_CHANNEL, &req);
		int64_t retVal = 0;

		switch (cmd) {
		case 345: {
			PspemuGpsData *outGps;
			PspemuGpsSatInfo *outSat;
			PspemuGpsData gpsData;
			SceLocationLocationInfo locInfo;
			SceDateTime dt;
			int locRet;

			if (g_usbGpsActive != 1) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			if (g_usbLocationHandle == 0U) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}

			outGps = (PspemuGpsData *)remotefsTranslateGuestAddr((uint32_t)req->args[0], 2,
			                                                     sizeof(PspemuGpsData));
			outSat = (PspemuGpsSatInfo *)remotefsTranslateGuestAddr((uint32_t)req->args[1], 2,
			                                                        sizeof(PspemuGpsSatInfo));
			memset(&gpsData, 0, sizeof(gpsData));
			locRet = sceLocationGetLocation(g_usbLocationHandle, &locInfo);
			if (locRet == SCE_LOCATION_ERROR_INVALID_HANDLE) {
				*outGps = gpsData;
				memset(outSat, 0, sizeof(PspemuGpsSatInfo));
				retVal = PSPEMU_USB_ERR_GPS_NOT_OPEN;
			} else if (locRet != 0 && locRet != SCE_LOCATION_INFO_UNDETERMINED_LOCATION) {
				*outGps = gpsData;
				memset(outSat, 0, sizeof(PspemuGpsSatInfo));
				retVal = PSPEMU_USB_ERR_GPS_IO;
			} else {
				if (locRet == 0 &&
				    sceRtcSetTick(&dt, &locInfo.timestamp) == 0 &&
				    locInfo.latitude != -9999.0 &&
				    locInfo.longitude != -9999.0) {
					gpsData.year = dt.year;
					gpsData.month = dt.month;
					gpsData.day = dt.day;
					gpsData.hour = dt.hour;
					gpsData.minute = dt.minute;
					gpsData.second = dt.second;
					gpsData.microsecond = dt.microsecond;
					if (locInfo.accuracy == -9999.0f) {
						gpsData.hdop = 1.0f;
					} else {
						float hdop = locInfo.accuracy / 10.0f;
						gpsData.hdop = (hdop > 99.0f) ? 99.0f : hdop;
					}
					gpsData.valid = 1U;
					gpsData.latitude = (float)locInfo.latitude;
					gpsData.longitude = (float)locInfo.longitude;
					if (locInfo.altitude == -9999.0) {
						gpsData.fixMode = 1U;
					} else {
						gpsData.fixMode = 2U;
						gpsData.altitude = (float)locInfo.altitude;
					}
					if (locInfo.speed != -9999.0f)
						gpsData.speed = (locInfo.speed * 60.0f * 60.0f) / 1000.0f;
					if (locInfo.direction != -9999.0f)
						gpsData.bearing = 360.0f - locInfo.direction;
				}
				*outGps = gpsData;
				memcpy(outSat, &g_usbGpsDefaultSatInfo, sizeof(PspemuGpsSatInfo));
				retVal = 0;
			}
			remotefsFlushGuestAddr(outGps, sizeof(PspemuGpsData));
			remotefsFlushGuestAddr(outSat, sizeof(PspemuGpsSatInfo));
			break;
		}
		case 346:
			retVal = remoteUsbCamApplySetter(remoteUsbCamSetSaturation, req);
			break;
		case 347: {
			int capRet;
			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			g_usbCamStillMode = 1;
			capRet = remoteUsbCamCaptureFrame(0x80000U);
			retVal = (capRet >= 0) ? capRet : remoteUsbCamTranslateError(capRet);
			break;
		}
		case 348:
			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			g_usbCamStillMode = 1;
			retVal = 0;
			break;
		case 349: {
			int pid = (int)req->args[0];
			if (pid == PSPEMU_USB_PID_PSPCM) {
				if (g_usbPspcmEnabled == 0) {
					retVal = PSPEMU_USB_ERR_NOT_ACTIVATED;
				} else {
					int retry;
					for (retry = 0; retry < 20; retry++) {
						int stopRet = sceUsbPspcmStopDevice();
						retVal = stopRet;
						if (stopRet == 0)
							break;
						sceKernelDelayThread(500000U);
					}
				}
			} else if (pid == PSPEMU_USB_PID_CAM) {
				if (!pspemuHasCameraCapability()) {
					retVal = pspemuCompatHasFlag(0x4000U) ? 0 : PSPEMU_USB_ERR_NOT_ACTIVATED;
				} else {
					if (remoteUsbCamStopDevice() >= 0) {
						int micOk = 1;
						if (g_usbCamMicPort > 0) {
							int relRet = sceAudioInReleasePort(g_usbCamMicPort);
							g_usbCamMicPort = -1;
							if (relRet < 0)
								micOk = 0;
						}
						if (micOk && g_usbJpegEncCtx != NULL) {
							free(g_usbJpegEncCtx);
							g_usbJpegEncCtx = NULL;
						}
					}
					if (g_usbCamActive == 1U) {
						remoteUsbCamResetState();
						retVal = 0;
					} else {
						retVal = PSPEMU_USB_ERR_ALREADY_ACTIVATED;
					}
				}
			} else if (pid == PSPEMU_USB_PID_GPS) {
				if (g_usbGpsActive == 1) {
					g_usbGpsActive = 0;
					retVal = 0;
				} else {
					retVal = 0;
				}
			} else {
				retVal = PSPEMU_USB_ERR_INVALID_PID;
			}
			break;
		}
		case 350: {
			uint32_t mode;
			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			mode = (uint32_t)req->args[0];
			if (mode > 2U) {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_MODE;
			} else {
				int afRet = sceCameraSetAntiFlicker(g_usbCamDevice, g_usbCamAntiFlickerTable[mode]);
				retVal = (afRet >= 0) ? afRet : remoteUsbCamTranslateError(afRet);
			}
			break;
		}
		case 351: {
			int outBytes;
			int16_t *outPcm;
			int micErr = 0;

			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}

			outBytes = (int)req->args[1];
			outPcm = (int16_t *)remotefsTranslateGuestAddr((uint32_t)req->args[0], 2, (SceSize)outBytes);

			if (g_usbCamMicPort == -1) {
				int port = sceAudioInOpenPort(SCE_AUDIO_IN_PORT_TYPE_VOICE, 256, 16000,
				                              SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO);
				if (port < 0)
					micErr = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				else
					g_usbCamMicPort = port;
			}

			if (micErr == 0) {
				int totalOutSamples = (g_usbCamMicParam.freq << 8) / 16000;
				int pos;

				g_usbCamMicReadBytes = 0;
				for (pos = 0; pos < totalOutSamples; pos += 256) {
					int idx;
					int chunk;

					if (sceAudioInInput(g_usbCamMicPort, g_usbCamMicPcm) < 0) {
						g_usbCamMicPort = -1;
						micErr = PSPEMU_USB_ERR_CAM_NO_DEVICE;
						break;
					}

					for (idx = 255; idx >= 0; idx--) {
						int16_t sample = ((const int16_t *)g_usbCamMicPcm)[idx];
						g_usbCamMicPcm[idx][0] = sample;
						g_usbCamMicPcm[idx][1] = 0;
					}

					audioResamplerProcess(&g_usbCamResampler, &g_usbCamMicPcm[0][0], 256,
					                      &g_usbCamResampleOut[0][0], totalOutSamples, 0x8000U);

					chunk = (totalOutSamples <= pos + 255) ? (totalOutSamples - pos) : totalOutSamples;
					for (idx = 0; idx < chunk; idx++)
						outPcm[pos + idx] = g_usbCamResampleOut[idx][0];
				}
			}

			if (micErr != 0) {
				retVal = remoteUsbCamTranslateError(micErr);
			} else {
				g_usbCamMicReadBytes = outBytes;
				retVal = (outBytes >= 0) ? outBytes : remoteUsbCamTranslateError(outBytes);
			}
			remotefsFlushGuestAddr((void *)(uintptr_t)(uint32_t)req->args[0], (SceSize)outBytes);
			break;
		}
		case 352:
			retVal = pspemuHasCameraCapability() ? 0 : PSPEMU_USB_ERR_CAM_NO_DEVICE;
			break;
		case 353:
			retVal = remoteUsbCamApplySetter((int (*)(int))remoteUsbCamSetContrast, req);
			break;
		case 354:
			retVal = remoteUsbCamApplySetter((int (*)(int))remoteUsbCamSetEv, req);
			break;
		case 355: {
			int closeRet;
			if (g_usbLocationHandle != 0U) {
				sceLocationConfirmAbort(g_usbLocationHandle);
				sceKernelWaitThreadEnd(g_usbLocationConfirmThreadId, NULL, NULL);
			}
			closeRet = sceLocationClose(g_usbLocationHandle);
			g_usbLocationHandle = 0U;
			if (closeRet == SCE_LOCATION_ERROR_INVALID_HANDLE)
				retVal = PSPEMU_USB_ERR_GPS_NOT_OPEN;
			else if (closeRet == 0)
				retVal = 0;
			else
				retVal = PSPEMU_USB_ERR_GPS_FAIL;
			break;
		}
		case 356:
			if (g_usbPspcmEnabled != 0) {
				unsigned int titleSize = (unsigned int)req->args[0];
				const char *title = (const char *)remotefsTranslateGuestAddr(
					(uint32_t)req->args[1], 1, 256U);
				retVal = sceUsbPspcmSetTitle(title, titleSize);
			} else {
				retVal = PSPEMU_USB_ERR_PSPCM_NOT_SUPPORTED;
			}
			break;
		case 357: {
			int stopRet;
			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			g_usbCamStillMode = 0;
			stopRet = remoteUsbCamStopDevice();
			retVal = (stopRet >= 0) ? stopRet : remoteUsbCamTranslateError(stopRet);
			break;
		}
		case 359:
			retVal = remoteUsbCamApplySetter(remoteUsbCamSetZoom, req);
			break;
		case 360:
			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			g_usbCamStillMode = 0;
			retVal = g_usbCamLastJpegSize;
			break;
		case 362:
			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			retVal = g_usbCamMicReadBytes;
			break;
		case 363:
			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			g_usbCamMicParam.gain = (int32_t)req->args[0];
			retVal = 0;
			break;
		case 364: {
			int rev;
			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			rev = (int)req->args[0];
			if (rev < 0 || (rev > 1 && (unsigned int)(rev - 0x100) > 1U))
				return PSPEMU_USB_ERR_CAM_INVALID_PARAM;
			g_usbCamVideoReverse = rev;
			g_usbCamStillReverse = rev;
			retVal = 0;
			break;
		}
		case 365: {
			int startRet;
			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			g_usbCamStillMode = 0;
			startRet = remoteUsbCamStartDevice();
			retVal = (startRet >= 0) ? startRet : remoteUsbCamTranslateError(startRet);
			break;
		}
		case 366: {
			const PspemuUsbCamVideoParam *param;
			uint16_t resCode;

			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			g_usbCamStillMode = 0;
			param = (const PspemuUsbCamVideoParam *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[0], 1, sizeof(PspemuUsbCamVideoParam));

			if (param->resolution > 8U ||
			    (resCode = g_usbCamResTable[param->resolution].vitaResCode) == 0U) {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_MODE;
				break;
			}
			g_usbCamWidth = g_usbCamResTable[param->resolution].width;
			g_usbCamHeight = g_usbCamResTable[param->resolution].height;
			if ((param->frameSize - 1U) >= 0x8000U) {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_PARAM;
				break;
			}
			if (param->framerate > 7U) {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_MODE;
				break;
			}
			g_usbCamFramerate = g_usbCamFpsTable[param->framerate];
			if (param->whiteBalance > 3U) {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_MODE;
				break;
			}
			if (g_usbCamWhiteBalance != g_usbCamWhiteBalanceTable[param->whiteBalance]) {
				g_usbCamWhiteBalance = g_usbCamWhiteBalanceTable[param->whiteBalance];
				g_usbCamDirtyFlags |= PSPEMU_CAM_DIRTY_WB;
			}
			if (!remoteUsbCamSetSaturation(param->saturation) ||
			    !remoteUsbCamSetBrightness(param->brightness) ||
			    !remoteUsbCamSetContrast(param->contrast) ||
			    !remoteUsbCamSetSharpness(param->sharpness)) {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_RANGE;
				break;
			}
			if (!remoteUsbCamSetEffect(param->effectMode)) {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_MODE;
				break;
			}

			if (param->size == 40) {
				g_usbCamEv = 0;
				g_usbCamDigitalZoom = 0;
				g_usbCamIso = 100;
				g_usbCamZoom = 10;
				remoteUsbCamBuildInfo(resCode);
				retVal = 0;
				break;
			}

			if (!remoteUsbCamSetEv(param->evLevel)) {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_RANGE;
				break;
			}
			if (param->isoLevel > 3U) {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_MODE;
				break;
			}
			if (g_usbCamIso != g_usbCamIsoTable[param->isoLevel]) {
				g_usbCamIso = g_usbCamIsoTable[param->isoLevel];
				g_usbCamDirtyFlags |= PSPEMU_CAM_DIRTY_ISO;
			}

			if (param->size == 48) {
				g_usbCamZoom = 10;
				g_usbCamDigitalZoom = 0;
				remoteUsbCamBuildInfo(resCode);
				retVal = 0;
			} else if (param->size != 52) {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_SIZE;
			} else if (remoteUsbCamSetZoom(param->zoom) != 0) {
				remoteUsbCamBuildInfo(resCode);
				retVal = 0;
			} else {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_MODE;
			}
			break;
		}
		case 367:
			if (!pspemuHasCameraCapability())
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
			else
				retVal = (g_usbCamActive == 1U) ? 0 : PSPEMU_USB_ERR_CAM_NOT_READY;
			break;
		case 368:
			retVal = remoteUsbCamApplySetter((int (*)(int))remoteUsbCamSetEffect, req);
			break;
		case 369: {
			const PspemuUsbCamStillParam *param;
			uint16_t resCode;

			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			g_usbCamStillMode = 1;
			param = (const PspemuUsbCamStillParam *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[0], 1, sizeof(PspemuUsbCamStillParam));
			if (param->resolution > 8U ||
			    (resCode = g_usbCamResTable[param->resolution].vitaResCode) == 0U) {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_MODE;
				break;
			}
			g_usbCamWidth = g_usbCamResTable[param->resolution].width;
			g_usbCamHeight = g_usbCamResTable[param->resolution].height;
			if (param->size != 24) {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_SIZE;
			} else if ((param->jpegSize - 1U) >= 0x80000U) {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_PARAM;
			} else if (param->reverseFlags < 0 ||
			           (param->reverseFlags > 1 &&
			            (unsigned int)(param->reverseFlags - 0x100) > 1U) ||
			           param->delay > 3U) {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_MODE;
			} else if ((param->compLevel - 1U) > 62U) {
				retVal = PSPEMU_USB_ERR_CAM_INVALID_PARAM;
			} else {
				remoteUsbCamBuildInfo(resCode);
				g_usbCamStillReverse = param->reverseFlags;
				retVal = 0;
			}
			break;
		}
		case 371:
			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			g_usbCamStillMode = 1;
			retVal = g_usbCamLastJpegSize;
			break;
		case 372: {
			const PspemuUsbCamMicParam *param;
			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			param = (const PspemuUsbCamMicParam *)remotefsTranslateGuestAddr(
				(uint32_t)req->args[0], 1, sizeof(PspemuUsbCamMicParam));
			if (param->freq == 44100 || param->freq == 22050 || param->freq == 11025) {
				g_usbCamMicParam = *param;
				retVal = 0;
			} else {
				retVal = remoteUsbCamTranslateError(PSPEMU_USB_ERR_CAM_MIC_RATE);
			}
			break;
		}
		case 373: {
			SceLocationPermissionInfo permInfo;
			if (g_usbLocationHandle != 0U) {
				retVal = PSPEMU_USB_ERR_GPS_ALREADY_OPEN;
			} else if (sceLocationOpen(&g_usbLocationHandle,
			                           SCE_LOCATION_LMETHOD_AGPS_AND_3G_AND_WIFI,
			                           SCE_LOCATION_HMETHOD_NONE) != 0) {
				retVal = PSPEMU_USB_ERR_GPS_FAIL;
			} else if (sceLocationGetPermission(g_usbLocationHandle, &permInfo) ==
			           SCE_LOCATION_INFO_DENIED_BY_USER &&
			           sceKernelStartThread(g_usbLocationConfirmThreadId,
			                                sizeof(g_usbLocationHandle),
			                                &g_usbLocationHandle) != 0) {
				retVal = PSPEMU_USB_ERR_GPS_FAIL;
			} else {
				retVal = 0;
			}
			break;
		}
		case 374: {
			int pid = (int)req->args[0];
			if (pid == PSPEMU_USB_PID_PSPCM) {
				if (g_usbPspcmEnabled == 0)
					retVal = PSPEMU_USB_ERR_NOT_ACTIVATED;
				else
					retVal = sceUsbPspcmStartDevice();
			} else if (pid == PSPEMU_USB_PID_CAM) {
				if (!pspemuHasCameraCapability()) {
					retVal = pspemuCompatHasFlag(0x4000U) ? 0 : PSPEMU_USB_ERR_NOT_ACTIVATED;
				} else if (g_usbCamActive != 0U) {
					retVal = PSPEMU_USB_ERR_ALREADY_ACTIVATED;
				} else {
					SceSize ctxSize = sceJpegArmEncoderGetContextSize();
					g_usbJpegEncCtx = malloc(ctxSize);
					if (g_usbJpegEncCtx != NULL) {
						g_usbCamWidth = 480U;
						g_usbCamHeight = 272U;
						g_usbCamSharpness = 0;
						g_usbCamEffect = 0;
						g_usbCamBrightness = 128;
						g_usbCamContrast = 64;
						g_usbCamWhiteBalance = 0;
						g_usbCamActive = 1U;
						g_usbCamEv = 0;
						g_usbCamFramerate = 30;
						g_usbCamIso = 100;
						g_usbCamSaturation = 10;
						g_usbCamZoom = 10;
						retVal = 0;
					} else {
						retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
					}
				}
			} else if (pid == PSPEMU_USB_PID_GPS) {
				if (g_usbGpsActive != 0) {
					retVal = PSPEMU_USB_ERR_ALREADY_ACTIVATED;
				} else {
					g_usbGpsActive = 1;
					retVal = 0;
				}
			} else {
				retVal = PSPEMU_USB_ERR_INVALID_PID;
			}
			break;
		}
		case 375: {
			int capRet;
			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			if (g_usbCamStreaming == 0) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_STARTED;
				break;
			}
			g_usbCamStillMode = 0;
			capRet = remoteUsbCamCaptureFrame(0x8000U);
			retVal = (capRet >= 0) ? capRet : remoteUsbCamTranslateError(capRet);
			break;
		}
		case 376:
			retVal = remoteUsbCamApplySetter((int (*)(int))remoteUsbCamSetBrightness, req);
			break;
		case 377:
			retVal = remoteUsbCamApplySetter((int (*)(int))remoteUsbCamSetSharpness, req);
			break;
		case 378:
			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			audioResamplerInit(&g_usbCamResampler, 16000, g_usbCamMicParam.freq);
			g_usbCamMicReadBytes = 0;
			retVal = 0;
			break;
		case 379:
			if (g_usbPspcmEnabled != 0)
				retVal = remoteUsbPspcmGetEventPair();
			else
				retVal = -1;
			break;
		case 380:
			if (!pspemuHasCameraCapability())
				return PSPEMU_USB_ERR_CAM_NO_DEVICE;
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			return (g_usbCamStillMode != 0) ? g_usbCamStillReverse : g_usbCamVideoReverse;
		case 381:
			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			g_usbCamAutoReverseOff = ((int)req->args[0] == 1);
			retVal = 0;
			break;
		case 382:
			if (!pspemuHasCameraCapability()) {
				retVal = PSPEMU_USB_ERR_CAM_NO_DEVICE;
				break;
			}
			if (g_usbCamActive != 1U) {
				retVal = PSPEMU_USB_ERR_CAM_NOT_READY;
				break;
			}
			retVal = g_usbCamAutoReverseOff;
			break;
		case 383: {
			int curDev;
			if (sceKernelLockLwMutex(&g_usbMutex, 1, NULL) < 0)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			curDev = (g_usbCamSwitchPending == 0) ? g_usbCamDevice : (g_usbCamDevice != 1);
			retVal = curDev;
			if (sceKernelUnlockLwMutex(&g_usbMutex, 1) < 0)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			break;
		}
		case 384: {
			SceLocationPermissionInfo permInfo;
			SceInt32 permRet = sceLocationGetPermission(g_usbLocationHandle, &permInfo);
			if (permRet == SCE_LOCATION_INFO_DENIED_BY_USER)
				retVal = 2;
			else
				retVal = (permRet == 0) ? 1 : 0;
			break;
		}
		default:
			pspemuRecordTitleListFlags(16U);
			retVal = 0;
			break;
		}

		remotefsReturnValue(PSPEMU_USB_CHANNEL, req, retVal);
	}
}

int remoteUsbRequestCameraSwitch(void)
{
	int ret;

	if (sceKernelLockLwMutex(&g_usbMutex, 1, NULL) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	g_usbCamSwitchPending = 1;
	if (sceKernelUnlockLwMutex(&g_usbMutex, 1) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	do {
		ret = sceCompatInterrupt(11);
		if (ret == PSPEMU_USB_COMPAT_INTR_RETRY)
			sceKernelDelayThread(10000U);
	} while (ret == PSPEMU_USB_COMPAT_INTR_RETRY);
	return ret;
}

int remoteUsbIsPspcmEnabled(void)
{
	return g_usbPspcmEnabled;
}

int remoteUsbInit(void *workBuf)
{
	int ret;
	SceUID thid;

	ret = sceKernelCreateLwMutex(&g_usbMutex, "ScePspemuUsb", 0, 0, NULL);
	if (ret != 0)
		return ret;

	remoteUsbCamResetState();
	ret = remoteUsbAllocAlignedCdram(&g_usbCamWorkBuf, "ScePspemuUsbCameraWork");
	if (ret != 0)
		return ret;

	ret = remoteUsbAllocAlignedCdram(&g_usbZoomWorkBuf, "ScePspemuUsbZoomWork");
	if (ret != 0)
		return ret;

	g_usbCamMicReadBytes = 0;
	g_usbGpsActive = 0;
	g_usbLocationHandle = 0U;
	g_usbCamMicPort = -1;

	ret = SceLibLocationInternal_3500A98C(1U, 0x10000U);
	if (ret != 0)
		return ret;

	thid = sceKernelCreateThread("ScePspemuRemoteUsbLocationConfirmDlg",
	                             (SceKernelThreadEntry)remoteUsbLocationConfirmThread,
	                             96, 0x1000U, 0, 0x10000, NULL);
	g_usbLocationConfirmThreadId = thid;
	if (thid < 0)
		return thid;

	if (pspemuIsVitaModel() != 0 && pspemuCompatHasFlag(16U) == 1)
		g_usbPspcmEnabled = 1;
	else
		g_usbPspcmEnabled = 0;

	thid = sceKernelCreateThread("ScePspemuRemoteUsb",
	                             (SceKernelThreadEntry)remoteUsbThread,
	                             96, 0x1000U, 0, 0x10000, NULL);
	if (thid < 0)
		return thid;

	ret = sceKernelStartThread(thid, 0, NULL);
	if (ret == 0)
		g_usbWorkBuf = workBuf;
	return ret;
}
