/*
 * settings.c: ScePspemu system registry settings, controller/touch mapping,
 * shell menu event handler, and guide/error dialogs (0x81020628..0x81021efb).
 */
#include "host_common.h"
#include "settings_tz_data.h"

#include <string.h>

#define SETTINGS_COMMON_DIALOG_SDK_VERSION  0x03650000U
#define SETTINGS_COMMON_DIALOG_MAGIC_BASE   0xC0D1A109U
#define SETTINGS_ERR_UNSUPPORTED_MODE       ((int)0x80010086)

#define SETTINGS_REG_TYPE_INT               0
#define SETTINGS_REG_TYPE_STR               1
#define SETTINGS_REG_TYPE_BIN               2

#define SETTINGS_TOUCH_CORNER_RADIUS_SQ     32399
#define SETTINGS_FRONT_TOUCH_MAX_X          1920
#define SETTINGS_FRONT_TOUCH_MAX_Y          1088
#define SETTINGS_ANALOG_LOW_THRESHOLD       0x3FU
#define SETTINGS_ANALOG_HIGH_THRESHOLD      0xC0U

typedef struct SettingsRegKeyEntry {
	uint32_t size;
	const char *name;
	uint16_t type;
	uint16_t dataSize;
	union {
		uint32_t intValue;
		void *ptrValue;
	} value;
} SettingsRegKeyEntry;

_Static_assert(sizeof(SettingsRegKeyEntry) == 16U, "SettingsRegKeyEntry size");

typedef struct SettingsRegionDefaults {
	int32_t language;
	int32_t buttonAssign;
	int32_t timeFormat;
	int32_t dateFormat;
	int32_t timeZoneIndex;
} SettingsRegionDefaults;

typedef struct SettingsGuideDialogParam {
	uint32_t sdkVersion;
	uint8_t commonParamHeader[72];
	uint32_t commonMagic;
	uint8_t reservedHead[92];
	uint8_t dialogMode;
	uint8_t modePad[11];
	int32_t errorCode;
	uint8_t reservedTail[84];
} SettingsGuideDialogParam;

_Static_assert(sizeof(SettingsGuideDialogParam) == 0x110U, "SettingsGuideDialogParam size");

typedef struct SettingsGuideDialogResult {
	int32_t resultCode;
	uint8_t reservedHead[53];
	uint8_t checkState;
	uint8_t reservedTail[42];
} SettingsGuideDialogResult;

_Static_assert(sizeof(SettingsGuideDialogResult) == 0x64U, "SettingsGuideDialogResult size");

static const uint32_t g_settingsButtonTable[17] = {
	0x0000U, 0x0010U, 0x0040U, 0x0080U,
	0x0020U, 0x1000U, 0x2000U, 0x4000U,
	0x8000U, 0x0008U, 0x0001U, 0x0400U,
	0x0800U, 0x0100U, 0x0200U, 0x0002U,
	0x0004U
};

static const SettingsRegionDefaults g_settingsRegionDefaults[10] = {
	{  0, 0, 0, 0, 104 },
	{  1, 1, 1, 1,  12 },
	{  1, 1, 0, 2, 106 },
	{  9, 0, 0, 2,  29 },
	{  1, 1, 0, 2,  50 },
	{  3, 1, 0, 0, 103 },
	{  1, 1, 1, 0, 102 },
	{  1, 0, 1, 0,  99 },
	{ 10, 0, 0, 2,  80 },
	{  1, 1, 1, 2,  10 },
};

static SettingsRegKeyEntry g_settingsRegKeys[9] = {
	{ 16U, "disp_filter",       SETTINGS_REG_TYPE_INT, 4U, { 0U } },
	{ 16U, "camera_direction",  SETTINGS_REG_TYPE_INT, 4U, { 0U } },
	{ 16U, "ra_up",             SETTINGS_REG_TYPE_INT, 4U, { 0U } },
	{ 16U, "color_space",       SETTINGS_REG_TYPE_INT, 4U, { 0U } },
	{ 16U, "screen_left_up",    SETTINGS_REG_TYPE_INT, 4U, { 0U } },
	{ 16U, "screen_right_up",   SETTINGS_REG_TYPE_INT, 4U, { 0U } },
	{ 16U, "screen_left_down",  SETTINGS_REG_TYPE_INT, 4U, { 0U } },
	{ 16U, "screen_right_down", SETTINGS_REG_TYPE_INT, 4U, { 0U } },
	{ 16U, "delete_contents",   SETTINGS_REG_TYPE_INT, 4U, { 0U } },
};

static int g_activePocsDialogMode;
static int g_popsReadyState;
static SceKernelLwMutexWork g_showMenuMutex;
static uint32_t g_rearTouchButtons;
static SceUID g_showMenuEvf;
static uint8_t g_menuDialogOpen;
static uint32_t g_frontCornerButtons[4];
static PspemuMenuSettings g_menuSettings;
static int g_fatalNoticeActive;
static int g_storeStartControl;
static int g_pocsFirstBootDone;
static int g_firstBootDone;
static const char *g_firstBootRegKey;

static int __attribute__((noinline, noclone)) settingsSaveRegKeys(const PspemuMenuSettings *settings);
static int __attribute__((noinline, noclone)) settingsLoadRegKeys(PspemuMenuSettings *settings);

/* 0x81020628 */
int pspemuCompatGetDefaultCameraDevice(void)
{
	return (int)g_menuSettings.cameraDirection;
}

/* 0x81020634 */
int pspemuSettingsApplyStickFilter(SceCtrlData *padData)
{
	if ((g_menuSettings.padModeFlags & 1U) == 0U && g_menuSettings.dpadAssign == 1U) {
		uint32_t clearMask = 0U;
		uint32_t stickX = padData->lx;
		uint32_t stickY = padData->ly;

		if (stickX <= SETTINGS_ANALOG_LOW_THRESHOLD)
			clearMask |= 0x80U;
		else if (stickX > SETTINGS_ANALOG_HIGH_THRESHOLD)
			clearMask |= 0x20U;

		if (stickY <= SETTINGS_ANALOG_LOW_THRESHOLD)
			clearMask |= 0x10U;
		else if (stickY > SETTINGS_ANALOG_HIGH_THRESHOLD)
			clearMask |= 0x40U;

		padData->buttons &= ~clearMask;
	}
	return 0;
}

/* 0x81020688 */
int pspemuSettingsReadDs4TouchPad(int portOffset)
{
	SceCtrlData2 ctrlData;
	SceTouchData touchData;
	int port = portOffset + 1;
	int buttons = 0;

	if (sceCtrlPeekBufferPositive2(port, &ctrlData, 1) > 0 &&
	    (ctrlData.buttons & 0x04000000U) != 0U &&
	    sceTouchPeek2(port, 0, &touchData, 1) >= 0 &&
	    touchData.reportNum != 0U) {
		uint32_t idx;
		for (idx = 0; idx < touchData.reportNum; idx++) {
			int16_t posX = touchData.report[idx].x;
			if (posX < 640)
				buttons |= 1;
			else if (posX > 1280)
				buttons |= 8;
		}
	}
	return buttons;
}

static inline void settingsUpdateFrontTouchCorners(void)
{
	SceTouchData touchData;
	int readCount;

	memset(&touchData, 0, sizeof(touchData));
	sceTouchSetSamplingState(0, SCE_TOUCH_SAMPLING_STATE_START);
	readCount = sceTouchPeek(0, &touchData, 1);
	sceTouchSetSamplingState(0, SCE_TOUCH_SAMPLING_STATE_STOP);

	if (readCount > 0 && touchData.reportNum != 0U) {
		int hitLeftUp = 0;
		int hitRightUp = 0;
		int hitLeftDown = 0;
		int hitRightDown = 0;
		uint32_t btnLeftUp = g_frontCornerButtons[0];
		uint32_t btnRightUp = g_frontCornerButtons[1];
		uint32_t btnLeftDown = g_frontCornerButtons[2];
		uint32_t btnRightDown = g_frontCornerButtons[3];
		uint32_t idx;

		for (idx = 0; idx < touchData.reportNum; idx++) {
			int posX = touchData.report[idx].x;
			int posY = touchData.report[idx].y;
			int dxRight = posX - SETTINGS_FRONT_TOUCH_MAX_X;
			int dyBottom = posY - SETTINGS_FRONT_TOUCH_MAX_Y;

			if (posX * posX + posY * posY <= SETTINGS_TOUCH_CORNER_RADIUS_SQ) {
				hitLeftUp = 1;
				btnLeftUp = g_settingsButtonTable[g_menuSettings.screenLeftUp];
			} else if (dxRight * dxRight + posY * posY <= SETTINGS_TOUCH_CORNER_RADIUS_SQ) {
				hitRightUp = 1;
				btnRightUp = g_settingsButtonTable[g_menuSettings.screenRightUp];
			} else if (posX * posX + dyBottom * dyBottom <= SETTINGS_TOUCH_CORNER_RADIUS_SQ) {
				hitLeftDown = 1;
				btnLeftDown = g_settingsButtonTable[g_menuSettings.screenLeftDown];
			} else if (dxRight * dxRight + dyBottom * dyBottom <= SETTINGS_TOUCH_CORNER_RADIUS_SQ) {
				hitRightDown = 1;
				btnRightDown = g_settingsButtonTable[g_menuSettings.screenRightDown];
			}
		}

		g_frontCornerButtons[0] = hitLeftUp ? btnLeftUp : 0U;
		g_frontCornerButtons[1] = hitRightUp ? btnRightUp : 0U;
		g_frontCornerButtons[2] = hitLeftDown ? btnLeftDown : 0U;
		g_frontCornerButtons[3] = hitRightDown ? btnRightDown : 0U;
	} else {
		g_frontCornerButtons[0] = 0U;
		g_frontCornerButtons[1] = 0U;
		g_frontCornerButtons[2] = 0U;
		g_frontCornerButtons[3] = 0U;
	}
}

/* 0x81020700 */
int pspemuSettingsUpdateController(SceCtrlData *padData)
{
	SceTouchPanelInfo panelInfo;
	SceTouchData touchData;
	int ps1Mode = pspemuIsPs1Mode();

	if (ps1Mode == 0) {
		if (g_menuDialogOpen != 0U) {
			padData->lx = 0x80U;
			padData->ly = 0x80U;
			padData->rx = 0x80U;
			padData->ry = 0x80U;
		}

		if ((padData->buttons & 0x10000U) == 0U) {
			switch (g_menuSettings.rightAnalogMode) {
			case 1:
				if (padData->lx > 0x9FU) {
					if (padData->lx < padData->rx)
						padData->lx = padData->rx;
				} else if (padData->lx > padData->rx ||
				           (padData->lx > 0x60U && padData->lx < padData->rx)) {
					padData->lx = padData->rx;
				}
				if (padData->ly > 0x9FU) {
					if (padData->ly < padData->ry)
						padData->ly = padData->ry;
				} else if (padData->ly > padData->ry ||
				           (padData->ly > 0x60U && padData->ly < padData->ry)) {
					padData->ly = padData->ry;
				}
				break;
			case 2:
				if (padData->rx <= SETTINGS_ANALOG_LOW_THRESHOLD)
					padData->buttons |= 0x80U;
				else if (padData->rx > SETTINGS_ANALOG_HIGH_THRESHOLD)
					padData->buttons |= 0x20U;
				if (padData->ry <= SETTINGS_ANALOG_LOW_THRESHOLD)
					padData->buttons |= 0x10U;
				else if (padData->ry > SETTINGS_ANALOG_HIGH_THRESHOLD)
					padData->buttons |= 0x40U;
				break;
			case 3:
				if (padData->rx <= SETTINGS_ANALOG_LOW_THRESHOLD)
					padData->buttons |= 0x80U;
				else if (padData->rx > SETTINGS_ANALOG_HIGH_THRESHOLD)
					padData->buttons |= 0x20U;
				break;
			case 4:
				if (padData->ry <= SETTINGS_ANALOG_LOW_THRESHOLD)
					padData->buttons |= 0x10U;
				else if (padData->ry > SETTINGS_ANALOG_HIGH_THRESHOLD)
					padData->buttons |= 0x40U;
				break;
			case 5:
				if (padData->rx <= SETTINGS_ANALOG_LOW_THRESHOLD)
					padData->buttons |= 0x8000U;
				else if (padData->rx > SETTINGS_ANALOG_HIGH_THRESHOLD)
					padData->buttons |= 0x2000U;
				if (padData->ry <= SETTINGS_ANALOG_LOW_THRESHOLD)
					padData->buttons |= 0x1000U;
				else if (padData->ry > SETTINGS_ANALOG_HIGH_THRESHOLD)
					padData->buttons |= 0x4000U;
				break;
			case 6:
				if (padData->rx <= SETTINGS_ANALOG_LOW_THRESHOLD)
					padData->buttons |= 0x100U;
				else if (padData->rx > SETTINGS_ANALOG_HIGH_THRESHOLD)
					padData->buttons |= 0x200U;
				break;
			case 7:
				if (padData->rx <= SETTINGS_ANALOG_LOW_THRESHOLD)
					padData->buttons |= 0x01U;
				else if (padData->rx > SETTINGS_ANALOG_HIGH_THRESHOLD)
					padData->buttons |= 0x08U;
				break;
			default:
				break;
			}
		}

		padData->ry = 0U;
		padData->rx = 0U;

		{
			SceCtrlData2 extCtrl;
			if (sceCtrlPeekBufferPositive2(1, &extCtrl, 1) > 0)
				padData->buttons |= extCtrl.buttons & 0x300U;
		}

		settingsUpdateFrontTouchCorners();
		padData->buttons |= g_frontCornerButtons[0] | g_frontCornerButtons[1] |
		                    g_frontCornerButtons[2] | g_frontCornerButtons[3] |
		                    g_rearTouchButtons;

		if (pspemuHasRearTouchCapability() != 0)
			padData->buttons |= (uint32_t)pspemuSettingsReadDs4TouchPad(0);
	} else if (ps1Mode == 1) {
		if (pspemuHasRearTouchCapability() != 0) {
			padData->buttons &= 0x10000U;
			padData->lx = 0x80U;
			padData->ly = 0x80U;
			padData->rx = 0x80U;
			padData->ry = 0x80U;
		} else {
			memset(&touchData, 0, sizeof(touchData));
			sceTouchSetSamplingState(1, SCE_TOUCH_SAMPLING_STATE_START);
			if (sceTouchPeek(1, &touchData, 1) > 0) {
				int centerX;
				int halfWidth;
				int minActiveX;
				int maxActiveX;
				uint32_t idx;

				sceTouchGetPanelInfo(1, &panelInfo);
				centerX = (panelInfo.maxAaX + panelInfo.minAaX) / 2;
				halfWidth = (int)(((float)(panelInfo.maxAaX - panelInfo.minAaX) * 0.72407705f) * 0.5f);
				minActiveX = centerX - halfWidth;
				maxActiveX = centerX + halfWidth;

				for (idx = 0; idx < touchData.reportNum; idx++) {
					int posX = touchData.report[idx].x;
					int posY = touchData.report[idx].y;

					if (posY < (2 * panelInfo.minAaY + panelInfo.maxAaY) / 3) {
						if (posX > centerX && posX < maxActiveX)
							g_rearTouchButtons |= g_settingsButtonTable[g_menuSettings.rearTouchRightUp];
						else if (posX < centerX && posX > minActiveX)
							g_rearTouchButtons |= g_settingsButtonTable[g_menuSettings.rearTouchLeftUp];
					} else if (posY > (2 * panelInfo.maxAaY + panelInfo.minAaY) / 3) {
						if (posX > centerX && posX < maxActiveX)
							g_rearTouchButtons |= g_settingsButtonTable[g_menuSettings.rearTouchRightDown];
						else if (posX < centerX && posX > minActiveX)
							g_rearTouchButtons |= g_settingsButtonTable[g_menuSettings.rearTouchLeftDown];
					}
				}
			}

			padData->buttons = popsHostMapButtons(padData->buttons) | g_rearTouchButtons;
			if ((g_menuSettings.padModeFlags & 1U) == 0U)
				padData->buttons &= 0xFFFFFFF9U;

			settingsUpdateFrontTouchCorners();
			padData->buttons |= g_frontCornerButtons[0] | g_frontCornerButtons[1] |
			                    g_frontCornerButtons[2] | g_frontCornerButtons[3];

			if ((g_menuSettings.padModeFlags & 1U) == 0U && g_menuSettings.analogStickAssign != 0U) {
				if (padData->lx <= SETTINGS_ANALOG_LOW_THRESHOLD)
					padData->buttons |= popsHostGetAnalogButtonMask(12);
				else if (padData->lx > SETTINGS_ANALOG_HIGH_THRESHOLD)
					padData->buttons |= popsHostGetAnalogButtonMask(13);

				if (padData->ly <= SETTINGS_ANALOG_LOW_THRESHOLD)
					padData->buttons |= popsHostGetAnalogButtonMask(10);
				else if (padData->ly > SETTINGS_ANALOG_HIGH_THRESHOLD)
					padData->buttons |= popsHostGetAnalogButtonMask(11);

				if (padData->rx <= SETTINGS_ANALOG_LOW_THRESHOLD)
					padData->buttons |= popsHostGetAnalogButtonMask(16);
				else if (padData->rx > SETTINGS_ANALOG_HIGH_THRESHOLD)
					padData->buttons |= popsHostGetAnalogButtonMask(17);

				if (padData->ry <= SETTINGS_ANALOG_LOW_THRESHOLD)
					padData->buttons |= popsHostGetAnalogButtonMask(14);
				else if (padData->ry > SETTINGS_ANALOG_HIGH_THRESHOLD)
					padData->buttons |= popsHostGetAnalogButtonMask(15);
			}
		}
	}

	g_rearTouchButtons = 0U;
	return 0;
}

/* 0x81020e54 */
int pspemuSettingsApplyDisplay(void)
{
	if (pspemuIsPs1Mode() == 1)
		return 0;
	if (pspemuIsAlternateDisplayHead() != 0)
		return sceCompatSetDisplayConfig(0x100000, 0);
	return sceCompatSetDisplayConfig((int)g_menuSettings.dispFilter,
	                                 (int)g_menuSettings.colorSpace);
}

/* 0x81020e88 */
int pspemuSettingsGetDialogActive(void)
{
	return g_popsReadyState;
}

/* 0x81020e94 */
int pspemuSettingsAbortPocsDialog(void)
{
	if (g_activePocsDialogMode == 2)
		SceCommonDialog_7030E4A6();
	return 0;
}

/* 0x81020eb0 */
int pspemuSettingsRunPocsMenuFlow(int dialogMode)
{
	int hasMedia;

	if (SceCommonDialog_500229F0() == 1 ||
	    sceNetCheckDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING) {
		return 0;
	}

	remotePocsSetEmulationPaused(0);
	if (popsHostGetDiscLoadSpeed(g_popsMenuConfig) == 2)
		hasMedia = 2;
	else
		hasMedia = (remotePocsIsAudioActive() == 0) ? 1 : 0;

	for (;;) {
		int bgType;
		int dialogAction = 0;
		int displayType = popsHostGetPocsBgType();

		if (displayType == 1)
			bgType = 3;
		else if (displayType == 2)
			bgType = 5;
		else
			bgType = 1;

		g_activePocsDialogMode = dialogMode;
		pspemuBgDialogShowMenu(dialogMode, hasMedia, &bgType, &dialogAction);
		g_activePocsDialogMode = 0;

		popsHostSetPocsBgTypeAndSave((uint8_t)bgDialogMapBgTypeToMode(bgType));
		if (pspemuIsPocketStationLaunch() == 0)
			break;

		if (dialogAction == 1) {
			uint32_t alertResult = 0U;
			if (remotePocsRunConnectDialog(&alertResult) != 0)
				continue;
			if (alertResult == 0U) {
				dialogMode = 1;
				continue;
			}
			if (alertResult != 2U)
				continue;
			break;
		} else if (dialogAction == 2) {
			int32_t importResult = 0;
			if (pspemuBgImageImportPhotoDialog(&importResult) != 0)
				continue;
			if ((uint32_t)importResult <= 1U) {
				dialogMode = 6;
				continue;
			}
			if (importResult == 2)
				break;
			continue;
		} else {
			break;
		}
	}

	if (pspemuIsPocketStationLaunch() != 0) {
		if (pspemuSettingsIsFatalNoticeActive() == 0)
			remotePocsSetEmulationPaused(1);
	} else if (popsHostIsPocsOverlayEnabled() != 0 &&
	           pspemuSettingsIsFatalNoticeActive() == 0) {
		remotePocsSetEmulationPaused(1);
	}

	return 0;
}

/* 0x81020fd4 */
int pspemuSettingsIsMenuEnabled(void)
{
	return sceKernelPollEventFlag(g_showMenuEvf, 1U, 0U, NULL) == 0;
}

/* 0x81020ff4 */
static int settingsShellEventCallback(int eventArg, int eventType, int eventParam)
{
	(void)eventArg;

	switch (eventType) {
	case 1:
		if (!pspemuSettingsIsMenuEnabled())
			return 0;
		if (pspemuMenuIsOskDialogLocked())
			remoteOskAbort();

		if (pspemuIsPs1Mode() != 0) {
			if (pspemuIsPocketStationLaunch() != 0) {
				pspemuSettingsRunPocsMenuFlow(0);
			} else {
				if (pspemuIsPocketStationEnabled() != 0)
					remotePocsSetEmulationPaused(0);
				sceTouchSetSamplingState(1, SCE_TOUCH_SAMPLING_STATE_STOP);
				popsHostSetPauseState(1);
				if (remotePocsHasMountedMedia() == 0)
					sceCompatCache(PSPEMU_COMPAT_CACHE_INVALIDATE, popsHostGetVmcBuffer(0U), 0x20080U);
				sceCompatCache(PSPEMU_COMPAT_CACHE_INVALIDATE, popsHostGetVmcBuffer(1U), 0x20080U);
				g_menuSettings.discLoadSpeed = (uint8_t)popsHostGetDiscLoadSpeed(g_popsMenuConfig);
				g_menuSettings.customScreenActive = (uint8_t)popsHostIsCustomScreenActive();
				g_menuSettings.multiTapEnabled = (uint8_t)popsHostIsMultiTapEnabled();
				pspemuMenuRunDialog(&g_menuSettings);
				popsHostApplyMenuSettings(g_menuSettings);
				g_menuSettings.resetRequest = 0U;
				if (pspemuIsPocketStationLaunch() != 0)
					popsHostFinishPocsTransition();
				else
					popsHostSetPauseState(0);
				if (pspemuIsPocketStationEnabled() != 0 &&
				    pspemuSettingsIsFatalNoticeActive() == 0) {
					remotePocsSetEmulationPaused(1);
				}
			}
		} else {
			uint8_t prevCamera = g_menuSettings.cameraDirection;
			int dialogRet;

			settingsLoadRegKeys(&g_menuSettings);
			if (g_menuSettings.colorSpace > 1U)
				g_menuSettings.colorSpace = 0U;
			g_menuDialogOpen = 1U;
			dialogRet = pspemuMenuRunDialog(&g_menuSettings);
			settingsSaveRegKeys(&g_menuSettings);
			if (g_menuSettings.cameraDirection != prevCamera)
				remoteUsbRequestCameraSwitch();
			if (dialogRet == 0)
				pspemuSettingsApplyDisplay();
			g_menuDialogOpen = 0U;
		}
		return 0;
	case 2:
		if (pspemuIsPs1Mode() == 1)
			popsHostSetVibrationEnabled(eventParam == 1);
		return 0;
	case 3:
		return pspemuMenuIsOskDialogLocked() ? 2 : 0;
	case 4:
		if (pspemuIsPs1Mode() == 0)
			remoteNetNotifyGuestEvent(2);
		return 0;
	default:
		return 0;
	}
}

/* 0x81021198 */
int pspemuSettingsWaitMenuEnabled(void)
{
	return sceKernelWaitEventFlag(g_showMenuEvf, 1U, 0U, NULL, NULL);
}

/* 0x810211b8 */
int pspemuSettingsSetMenuEnabled(int enable)
{
	int ret;

	sceKernelLockLwMutex(&g_showMenuMutex, 1, NULL);
	if (enable != 0) {
		ret = SceShellUtil_CE35B2B8((void *)settingsShellEventCallback, NULL);
		sceKernelSetEventFlag(g_showMenuEvf, 1U);
	} else {
		ret = SceShellUtil_CE35B2B8(NULL, NULL);
		sceKernelClearEventFlag(g_showMenuEvf, 0xFFFFFFFEU);
	}
	sceKernelUnlockLwMutex(&g_showMenuMutex, 1);
	return ret;
}

/* 0x81021220 */
int pspemuSettingsNotifyPopsReady(void)
{
	int ret;

	if (pspemuIsPs1Mode() != 1)
		return 0;

	ret = pspemuMountPs1VmcSlots();
	if (ret < 0)
		pspemuTriggerFatalError(ret);

	pspemuFinishSplashAndLockModules();
	ret = popsHostInitSettings(&g_menuSettings);
	g_popsReadyState = 1;
	pspemuSettingsSetMenuEnabled(1);
	return ret;
}

/* 0x81021264 */
int pspemuSettingsDialogInit(void)
{
	int ret;
	SceUID evfId;

	memset(&g_menuSettings, 0, sizeof(g_menuSettings));
	ret = settingsLoadRegKeys(&g_menuSettings);
	if (g_menuSettings.colorSpace > 1U)
		g_menuSettings.colorSpace = 0U;

	if (ret < 0) {
		g_menuSettings.dispFilter = 0U;
		g_menuSettings.rightAnalogMode = 0U;
		g_menuSettings.dpadAssign = 0U;
		g_menuSettings.colorSpace = 0U;
		g_menuSettings.padModeFlags = 0U;
		g_menuSettings.controllerPort = 0U;
		g_menuSettings.multiTapEnabled = 0U;
		g_menuSettings.volumeLevel = 0U;
		g_menuSettings.screenMode = 0U;
		g_menuSettings.screenLeftUp = 0U;
		g_menuSettings.screenRightUp = 0U;
		g_menuSettings.screenLeftDown = 0U;
		g_menuSettings.screenRightDown = 0U;
		g_menuSettings.analogStickAssign = 0U;
		g_menuSettings.deleteContents = 0U;
		g_menuSettings.cameraDirection = 1U;
		g_menuSettings.discLoadSpeed = 2U;
		memset(g_menuSettings.customButtonMap, 0, sizeof(g_menuSettings.customButtonMap));
	}

	ret = sceKernelCreateLwMutex(&g_showMenuMutex, "ScePspemuShowMenu", 0x2000U, 0, NULL);
	if (ret < 0)
		return ret;

	evfId = sceKernelCreateEventFlag("ScePspemuShowMenu", 0U, 0U, NULL);
	g_showMenuEvf = evfId;
	if (evfId < 0)
		return evfId;

	return ret;
}

/* 0x81021318 */
static int __attribute__((noinline, noclone)) settingsOpenGuideDialog(int errorCode, uint8_t mode)
{
	SettingsGuideDialogParam param;
	int ret;

	sceClibMemset(&param, 0, sizeof(param));
	param.sdkVersion = SETTINGS_COMMON_DIALOG_SDK_VERSION;
	param.commonMagic = SETTINGS_COMMON_DIALOG_MAGIC_BASE +
	                    (uint32_t)(uintptr_t)&param.commonParamHeader;
	param.dialogMode = mode;
	param.errorCode = errorCode;

	ret = SceCommonDialog_9FA21E09(&param);
	if (ret >= 0) {
		SceUInt64 now = sceKernelGetProcessTimeWide();
		ret = pspemuMenuSetImeDeadline(now);
	}
	return ret;
}

/* 0x81021374 */
static int __attribute__((noinline, noclone)) settingsRunGuideDialog(int errorCode, int mode,
                                                            uint8_t *outCheck,
                                                            int *outResult)
{
	int ret = 0;
	int pollMode;

	if (SceCommonDialog_A68A44EA() == 0) {
		while (SceCommonDialog_FA26EB30() != 0) {
			sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
			sceDisplayWaitVblankStart();
		}
	}

	if (pspemuIsPocketStationEnabled() != 0 && SceCommonDialog_7030E4A6() == 0) {
		while (SceCommonDialog_500229F0() != 0) {
			sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
			sceDisplayWaitVblankStart();
		}
	}

	if (mode == 3) {
		if (pspemuIsPs1Mode() == 1) {
			int popsErr = popsHostGetPendingError();
			if (popsErr != 0)
				errorCode = popsErr;
			popsHostClearPendingError();
		} else {
			mode = 3;
		}
	} else if (mode == -1) {
		mode = 3;
	}

	pollMode = ((uint32_t)(mode - 4) <= 2U || mode == 2) ? 1 : 0;

	for (;;) {
		int status = SceCommonDialog_FA26EB30();
		if (status == 0) {
			if (pspemuMenuLockDialog(3) != 0)
				break;
			ret = settingsOpenGuideDialog(errorCode, (uint8_t)mode);
			if (ret < 0)
				break;
		} else if (status == 1) {
			ret = pspemuMenuPollDialog(pollMode);
			if (ret < 0)
				break;
		} else if (status == 2) {
			SettingsGuideDialogResult dlgResult;
			memset(&dlgResult, 0, sizeof(dlgResult));
			ret = SceCommonDialog_64F65B8D(&dlgResult);
			if (ret >= 0) {
				if (outCheck != NULL)
					*outCheck = dlgResult.checkState;
				if (outResult != NULL)
					*outResult = dlgResult.resultCode;
				SceCommonDialog_C2D6F7F2();
				ret = pspemuMenuClearImeDeadline();
				if (ret >= 0)
					pspemuMenuUnlockDialog(3);
			}
			break;
		}

		sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
		sceDisplayWaitVblankStart();
	}

	return ret;
}

/* 0x810214ac */
int pspemuSettingsShowGuideDialog(int dialogId)
{
	return settingsRunGuideDialog(0, dialogId, NULL, NULL);
}

/* 0x810214bc */
int pspemuSettingsShowErrorDialog(int errorCode)
{
	return settingsRunGuideDialog(errorCode, 3, NULL, NULL);
}

/* 0x810214cc */
int pspemuSettingsIsFatalNoticeActive(void)
{
	return g_fatalNoticeActive;
}

/* 0x810214d8 */
int pspemuSettingsShowFatalNotice(int errorCode)
{
	int ret;

	g_fatalNoticeActive = 1;
	ret = settingsRunGuideDialog(errorCode, -1, NULL, NULL);
	g_fatalNoticeActive = 0;
	return ret;
}

/* 0x810214f8 */
int pspemuSettingsShowFirstBootConfirm(uint8_t *outCheck, int *outResult)
{
	return settingsRunGuideDialog(0, 2, outCheck, outResult);
}

/* 0x81021508 */
int pspemuSettingsShowPocsConfirmDialog(uint8_t *outCheck, int *outResult)
{
	int ret = settingsRunGuideDialog(0, 6, outCheck, outResult);
	if (ret >= 0 && *outResult != 2 && *outCheck == 1U)
		return settingsRunGuideDialog(0, 7, NULL, outResult);
	return ret;
}

/* 0x8102153c */
int pspemuSettingsGetStoreStartControl(void)
{
	return g_storeStartControl;
}

/* 0x81021548 */
int pspemuSettingsGetPocsFirstBootDone(void)
{
	return g_pocsFirstBootDone;
}

/* 0x81021554 */
int pspemuSettingsMarkPocsFirstBootDone(void)
{
	int ret = sceRegMgrSetKeyInt("/CONFIG/PSPEMU", "pocketstation_first_boot", 1);
	if (ret < 0)
		pspemuSettingsShowFatalNotice(ret);
	else
		g_pocsFirstBootDone = 1;
	return ret;
}

/* 0x81021588 */
int pspemuSettingsGetFirstBootDone(void)
{
	return g_firstBootDone;
}

/* 0x81021594 */
int pspemuSettingsMarkFirstBootDone(void)
{
	int ret = sceRegMgrSetKeyInt("/CONFIG/PSPEMU", g_firstBootRegKey, 2);
	if (ret < 0)
		pspemuSettingsShowFatalNotice(ret);
	else
		g_firstBootDone = 2;
	return ret;
}

/* 0x810215cc */
static int __attribute__((noinline, noclone)) settingsSaveRegKeys(const PspemuMenuSettings *settings)
{
	int ret;

	if (settings->dispFilter <= 1U)
		g_settingsRegKeys[0].value.intValue = settings->dispFilter;
	if (settings->cameraDirection <= 1U)
		g_settingsRegKeys[1].value.intValue = settings->cameraDirection;
	if (settings->rightAnalogMode <= 8U)
		g_settingsRegKeys[2].value.intValue = settings->rightAnalogMode;
	if (settings->colorSpace <= 1U)
		g_settingsRegKeys[3].value.intValue = settings->colorSpace;
	if (settings->screenLeftUp <= 11U)
		g_settingsRegKeys[4].value.intValue = settings->screenLeftUp;
	if (settings->screenRightUp <= 11U)
		g_settingsRegKeys[5].value.intValue = settings->screenRightUp;
	if (settings->screenLeftDown <= 11U)
		g_settingsRegKeys[6].value.intValue = settings->screenLeftDown;
	if (settings->screenRightDown <= 11U)
		g_settingsRegKeys[7].value.intValue = settings->screenRightDown;
	if (settings->deleteContents <= 1U)
		g_settingsRegKeys[8].value.intValue = settings->deleteContents;

	ret = sceRegMgrSetKeys("/CONFIG/PSPEMU/SETTINGS", g_settingsRegKeys, 9);
	if (ret < 0)
		pspemuSettingsShowFatalNotice(ret);
	return 0;
}

/* 0x81021688 */
static int __attribute__((noinline, noclone)) settingsLoadRegKeys(PspemuMenuSettings *settings)
{
	settings->dispFilter = (g_settingsRegKeys[0].value.intValue <= 1U) ? (uint8_t)g_settingsRegKeys[0].value.intValue : 0U;
	settings->cameraDirection = (g_settingsRegKeys[1].value.intValue <= 1U) ? (uint8_t)g_settingsRegKeys[1].value.intValue : 0U;
	settings->rightAnalogMode = (g_settingsRegKeys[2].value.intValue <= 8U) ? (uint8_t)g_settingsRegKeys[2].value.intValue : 0U;
	settings->colorSpace = (g_settingsRegKeys[3].value.intValue <= 1U) ? (uint8_t)g_settingsRegKeys[3].value.intValue : 0U;
	settings->screenLeftUp = (g_settingsRegKeys[4].value.intValue <= 11U) ? (uint8_t)g_settingsRegKeys[4].value.intValue : 0U;
	settings->screenRightUp = (g_settingsRegKeys[5].value.intValue <= 11U) ? (uint8_t)g_settingsRegKeys[5].value.intValue : 0U;
	settings->screenLeftDown = (g_settingsRegKeys[6].value.intValue <= 11U) ? (uint8_t)g_settingsRegKeys[6].value.intValue : 0U;
	settings->screenRightDown = (g_settingsRegKeys[7].value.intValue <= 11U) ? (uint8_t)g_settingsRegKeys[7].value.intValue : 0U;
	settings->deleteContents = (g_settingsRegKeys[8].value.intValue <= 1U) ? (uint8_t)g_settingsRegKeys[8].value.intValue : 0U;
	return 0;
}

/* 0x81021704 */
int pspemuSettingsIsClearMediaEnabled(void)
{
	return (int)g_settingsRegKeys[8].value.intValue;
}

/* 0x81021714 */
int pspemuSettingsRefreshDynamic(PspemuSystemSettings *settings)
{
	uint32_t regVal;
	int lastErr;
	int ret;

	lastErr = sceRegMgrGetKeyInt("/CONFIG/NET", "adhoc_channel", (int *)&regVal);
	if (lastErr >= 0) {
		lastErr = 0;
		if (regVal > 11U)
			regVal = 0U;
		settings->adhocChannel = regVal;
	} else {
		settings->adhocChannel = 0U;
	}

	ret = sceRegMgrGetKeyInt("/CONFIG/SECURITY/PARENTAL", "store_start_control", &g_storeStartControl);
	if (ret < 0) {
		lastErr = ret;
		g_storeStartControl = 0;
	}

	ret = sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "controller_vibration", (int *)&regVal);
	if (ret < 0) {
		lastErr = ret;
		regVal = 0U;
	}
	popsHostSetVibrationEnabled((uint8_t)regVal);
	return lastErr;
}

/* 0x810217c0 */
int pspemuSettingsGetEmuListFlag(void)
{
	int flagVal;
	if (sceRegMgrGetKeyInt("/CONFIG/PSPEMU", "emu_list_flag", &flagVal) >= 0)
		return flagVal;
	return 0;
}

/* 0x81021800 */
int pspemuSettingsSetEmuListFlag(void)
{
	return sceRegMgrSetKeyInt("/CONFIG/PSPEMU", "emu_list_flag", 1);
}

/* 0x8102181c */
int pspemuSettingsLoadAll(PspemuSystemSettings *settings)
{
	SettingsRegKeyEntry keys[12];
	char passcodeBuf[8];
	char langBuf[8];
	uint8_t accountIdBin[8];
	char envBuf[20];
	int ps1Mode;
	int region;
	int regionIdx;
	int firstErr;
	int pspemuErr = 0;
	int dynRet;
	uint32_t tzIndex;
	int idx;

	ps1Mode = pspemuIsPs1Mode();
	if (ps1Mode == 0) {
		g_firstBootRegKey = "psp_first_boot";
	} else if (ps1Mode == 1) {
		g_firstBootRegKey = "ps1_first_boot";
	} else {
		return SETTINGS_ERR_UNSUPPORTED_MODE;
	}

	memset(settings, 0, sizeof(*settings));
	{
		int nickRet = sceAppUtilSystemParamGetString(3U, (SceChar8 *)settings->nickname, 0x11U);
		if (nickRet < 0)
			return nickRet;
	}

	region = sceRegMgrSrvGetRegion();
	firstErr = (region < 0) ? region : 0;

	switch (region) {
	case 2:
		regionIdx = 1;
		settings->regionCode = 1U;
		settings->regionDefault = 19U;
		settings->regionFormatA = 0U;
		settings->regionFormatB = 2U;
		settings->regionFormatC = 1U;
		break;
	case 3:
		regionIdx = 6;
		settings->regionCode = 5U;
		settings->regionDefault = 19U;
		settings->regionFormatA = 0U;
		settings->regionFormatB = 2U;
		settings->regionFormatC = 0U;
		break;
	case 4:
		regionIdx = 4;
		settings->regionCode = 5U;
		settings->regionDefault = 19U;
		settings->regionFormatA = 0U;
		settings->regionFormatB = 2U;
		settings->regionFormatC = 0U;
		break;
	case 5:
		regionIdx = 2;
		settings->regionCode = 5U;
		settings->regionDefault = 19U;
		settings->regionFormatA = 0U;
		settings->regionFormatB = 2U;
		settings->regionFormatC = 0U;
		break;
	case 6:
		regionIdx = 3;
		settings->regionCode = 15U;
		settings->regionDefault = 15U;
		settings->regionFormatA = 1U;
		settings->regionFormatB = 2U;
		settings->regionFormatC = 0U;
		break;
	case 7:
		regionIdx = 7;
		settings->regionCode = 5U;
		settings->regionDefault = 19U;
		settings->regionFormatA = 0U;
		settings->regionFormatB = 2U;
		settings->regionFormatC = 0U;
		break;
	case 8:
		regionIdx = 8;
		settings->regionCode = 16U;
		settings->regionDefault = 16U;
		settings->regionFormatA = 0U;
		settings->regionFormatB = 2U;
		settings->regionFormatC = 0U;
		break;
	case 9:
		regionIdx = 9;
		settings->regionCode = 11U;
		settings->regionDefault = 18U;
		settings->regionFormatA = 0U;
		settings->regionFormatB = 0U;
		settings->regionFormatC = 0U;
		break;
	case 10:
		regionIdx = 5;
		settings->regionCode = 5U;
		settings->regionDefault = 19U;
		settings->regionFormatA = 0U;
		settings->regionFormatB = 0U;
		settings->regionFormatC = 0U;
		break;
	default:
		regionIdx = 0;
		settings->regionCode = 13U;
		settings->regionDefault = 13U;
		settings->regionFormatA = 0U;
		settings->regionFormatB = 1U;
		settings->regionFormatC = 2U;
		break;
	}

	for (idx = 0; idx < 12; idx++)
		keys[idx].size = sizeof(SettingsRegKeyEntry);

	keys[0].name = "language";
	keys[0].type = SETTINGS_REG_TYPE_INT;
	keys[0].dataSize = 4U;
	keys[1].name = "button_assign";
	keys[1].type = SETTINGS_REG_TYPE_INT;
	keys[1].dataSize = 4U;

	{
		int ret = sceRegMgrGetKeys("/CONFIG/SYSTEM", keys, 2);
		if (ret < 0) {
			firstErr = ret;
			settings->language = (uint32_t)g_settingsRegionDefaults[regionIdx].language;
			settings->buttonAssign = (uint32_t)g_settingsRegionDefaults[regionIdx].buttonAssign;
		} else {
			settings->language = (keys[0].value.intValue <= 11U)
				? keys[0].value.intValue
				: (uint32_t)g_settingsRegionDefaults[regionIdx].language;
			settings->buttonAssign = (keys[1].value.intValue <= 1U)
				? keys[1].value.intValue
				: (uint32_t)g_settingsRegionDefaults[regionIdx].buttonAssign;
		}
	}

	keys[0].name = "time_format";
	keys[0].type = SETTINGS_REG_TYPE_INT;
	keys[0].dataSize = 4U;
	keys[1].name = "date_format";
	keys[1].type = SETTINGS_REG_TYPE_INT;
	keys[1].dataSize = 4U;
	keys[2].name = "summer_time";
	keys[2].type = SETTINGS_REG_TYPE_INT;
	keys[2].dataSize = 4U;
	keys[3].name = "time_zone";
	keys[3].type = SETTINGS_REG_TYPE_INT;
	keys[3].dataSize = 4U;

	{
		int ret = sceRegMgrGetKeys("/CONFIG/DATE", keys, 4);
		if (ret < 0) {
			firstErr = ret;
			settings->timeFormat = (uint32_t)g_settingsRegionDefaults[regionIdx].timeFormat;
			settings->dateFormat = (uint32_t)g_settingsRegionDefaults[regionIdx].dateFormat;
			settings->summerTime = 0U;
			tzIndex = (uint32_t)g_settingsRegionDefaults[regionIdx].timeZoneIndex;
		} else {
			if (keys[0].value.intValue == 0U)
				settings->timeFormat = 1U;
			else if (keys[0].value.intValue == 1U)
				settings->timeFormat = 0U;
			else
				settings->timeFormat = (uint32_t)g_settingsRegionDefaults[regionIdx].timeFormat;

			if (keys[1].value.intValue == 0U)
				settings->dateFormat = 0U;
			else if (keys[1].value.intValue == 1U)
				settings->dateFormat = 2U;
			else if (keys[1].value.intValue == 2U)
				settings->dateFormat = 1U;
			else
				settings->dateFormat = (uint32_t)g_settingsRegionDefaults[regionIdx].dateFormat;

			settings->summerTime = keys[2].value.intValue;
			tzIndex = (keys[3].value.intValue <= 108U)
				? keys[3].value.intValue
				: (uint32_t)g_settingsRegionDefaults[regionIdx].timeZoneIndex;
		}
	}

	settings->timeZoneOffset = g_settingsTimeZones[tzIndex].utcOffsetMinutes;
	strncpy(settings->timeZoneName, g_settingsTimeZones[tzIndex].name, sizeof(settings->timeZoneName));

	{
		int ret = sceRegMgrGetKeyStr("/CONFIG/NET", "pspnet_adhoc_ssid_prefix",
		                             settings->adhocSsidPrefix, sizeof(settings->adhocSsidPrefix));
		if (ret < 0) {
			firstErr = ret;
			memcpy(settings->adhocSsidPrefix, "PSP", 4U);
		}
	}

	keys[0].name = "env";
	keys[0].type = SETTINGS_REG_TYPE_STR;
	keys[0].dataSize = 17U;
	keys[0].value.ptrValue = envBuf;

	keys[1].name = "account_id";
	keys[1].type = SETTINGS_REG_TYPE_BIN;
	keys[1].dataSize = 8U;
	keys[1].value.ptrValue = accountIdBin;

	keys[2].name = "login_id";
	keys[2].type = SETTINGS_REG_TYPE_STR;
	keys[2].dataSize = 65U;
	keys[2].value.ptrValue = settings->npLoginId;

	keys[3].name = "password";
	keys[3].type = SETTINGS_REG_TYPE_STR;
	keys[3].dataSize = 31U;
	keys[3].value.ptrValue = settings->npPassword;

	keys[4].name = "nav_only";
	keys[4].type = SETTINGS_REG_TYPE_INT;
	keys[4].dataSize = 4U;

	keys[5].name = "np_ad_clock_diff";
	keys[5].type = SETTINGS_REG_TYPE_INT;
	keys[5].dataSize = 4U;

	keys[6].name = "np_geo_filtering";
	keys[6].type = SETTINGS_REG_TYPE_INT;
	keys[6].dataSize = 4U;

	keys[7].name = "yob";
	keys[7].type = SETTINGS_REG_TYPE_INT;
	keys[7].dataSize = 4U;

	keys[8].name = "mob";
	keys[8].type = SETTINGS_REG_TYPE_INT;
	keys[8].dataSize = 4U;

	keys[9].name = "dob";
	keys[9].type = SETTINGS_REG_TYPE_INT;
	keys[9].dataSize = 4U;

	keys[10].name = "lang";
	keys[10].type = SETTINGS_REG_TYPE_STR;
	keys[10].dataSize = 6U;
	keys[10].value.ptrValue = langBuf;

	keys[11].name = "country";
	keys[11].type = SETTINGS_REG_TYPE_STR;
	keys[11].dataSize = 3U;
	keys[11].value.ptrValue = settings->npCountry;

	{
		int ret = sceRegMgrGetKeys("/CONFIG/NP", keys, 12);
		if (ret < 0) {
			firstErr = ret;
			strncpy(settings->npEnv, "np", sizeof(settings->npEnv));
			memset(settings->npAccountIdHex, 0, sizeof(settings->npAccountIdHex));
			memset(settings->npLoginId, 0, sizeof(settings->npLoginId));
			memset(settings->npPassword, 0, sizeof(settings->npPassword));
			settings->npNavOnly = 0U;
			settings->npAdClockDiff = 0;
			settings->npGeoFiltering = 0U;
			settings->npYob = 0U;
			settings->npMob = 0U;
			settings->npDob = 0U;
			memset(settings->npLang, 0, sizeof(settings->npLang));
			memset(settings->npCountry, 0, sizeof(settings->npCountry));
		} else {
			envBuf[8] = '\0';
			strncpy(settings->npEnv, envBuf, sizeof(settings->npEnv));
			for (idx = 0; idx < 8; idx++) {
				uint8_t byteVal = accountIdBin[7 - idx];
				uint8_t hiNib = byteVal >> 4;
				uint8_t loNib = byteVal & 0x0FU;
				settings->npAccountIdHex[idx * 2] = (char)((hiNib > 9U) ? (hiNib + 87U) : (hiNib + 48U));
				settings->npAccountIdHex[idx * 2 + 1] = (char)((loNib > 9U) ? (loNib + 87U) : (loNib + 48U));
			}
			settings->npNavOnly = (keys[4].value.intValue <= 1U) ? keys[4].value.intValue : 0U;
			settings->npAdClockDiff = (int32_t)keys[5].value.intValue;
			settings->npGeoFiltering = (keys[6].value.intValue <= 2U) ? keys[6].value.intValue : 0U;
			settings->npYob = keys[7].value.intValue;
			settings->npMob = keys[8].value.intValue;
			settings->npDob = keys[9].value.intValue;
			langBuf[2] = '\0';
			strncpy(settings->npLang, langBuf, sizeof(settings->npLang));
		}
	}

	keys[0].name = "passcode";
	keys[0].type = SETTINGS_REG_TYPE_STR;
	keys[0].dataSize = 5U;
	keys[0].value.ptrValue = passcodeBuf;

	keys[1].name = "content_start_control";
	keys[1].type = SETTINGS_REG_TYPE_INT;
	keys[1].dataSize = 4U;

	keys[2].name = "browser_start_control";
	keys[2].type = SETTINGS_REG_TYPE_INT;
	keys[2].dataSize = 4U;

	{
		int ret = sceRegMgrGetKeys("/CONFIG/SECURITY/PARENTAL", keys, 3);
		if (ret < 0) {
			firstErr = ret;
			settings->parentalContentStartControl = 0U;
			settings->parentalBrowserStartControl = 0U;
			memcpy(settings->parentalPasscode, "0000", 4U);
		} else {
			settings->parentalContentStartControl = (keys[1].value.intValue <= 11U)
				? keys[1].value.intValue : 0U;
			settings->parentalBrowserStartControl = (keys[2].value.intValue <= 1U)
				? keys[2].value.intValue : 0U;
			memcpy(settings->parentalPasscode, passcodeBuf, 4U);
		}
	}

	{
		int ret = sceRegMgrGetKeyInt("/CONFIG/PSPEMU", g_firstBootRegKey, &g_firstBootDone);
		if (ret < 0) {
			pspemuErr = ret;
			g_firstBootDone = 0;
		}
		ret = sceRegMgrGetKeyInt("/CONFIG/PSPEMU", "pocketstation_first_boot", &g_pocsFirstBootDone);
		if (ret < 0) {
			pspemuErr = ret;
			g_pocsFirstBootDone = 0;
		}
		ret = sceRegMgrGetKeys("/CONFIG/PSPEMU/SETTINGS", g_settingsRegKeys, 9);
		if (ret < 0) {
			pspemuErr = ret;
			g_settingsRegKeys[0].value.intValue = 0U;
			g_settingsRegKeys[2].value.intValue = 0U;
			g_settingsRegKeys[3].value.intValue = 0U;
			g_settingsRegKeys[4].value.intValue = 0U;
			g_settingsRegKeys[5].value.intValue = 0U;
			g_settingsRegKeys[6].value.intValue = 0U;
			g_settingsRegKeys[7].value.intValue = 0U;
			g_settingsRegKeys[8].value.intValue = 0U;
			g_settingsRegKeys[1].value.intValue = 1U;
		}
	}

	dynRet = pspemuSettingsRefreshDynamic(settings);
	if (dynRet < 0) {
		pspemuSettingsShowFatalNotice(dynRet);
	} else if (pspemuErr != 0) {
		pspemuSettingsShowFatalNotice(pspemuErr);
	} else if (firstErr != 0) {
		pspemuSettingsShowFatalNotice(firstErr);
	}

	return 0;
}
