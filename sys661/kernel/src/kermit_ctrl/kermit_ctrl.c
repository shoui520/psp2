#include <kermit.h>

typedef void (*SceKernelButtonCallbackFunction)(u32 curButtons, u32 lastButtons, void *opt);

typedef struct SceCtrlData {
	u32 timeStamp;
	u32 buttons;
	u8 aX;
	u8 aY;
	u8 rX;
	u8 rY;
	u8 rsrv[4];
} SceCtrlData;

typedef struct SceCtrlData2 {
	u32 timeStamp;
	u32 buttons;
	u8 aX;
	u8 aY;
	u8 rX;
	u8 rY;
	u8 rsrv[4];
	s32 DPadSenseA;
	s32 DPadSenseB;
	s32 GPadSenseA;
	s32 GPadSenseB;
	s32 AxisSenseA;
	s32 AxisSenseB;
	s32 TiltA;
	s32 TiltB;
} SceCtrlData2;

typedef struct SceCtrlLatch {
	u32 buttonMake;
	u32 buttonBreak;
	u32 buttonPress;
	u32 buttonRelease;
} SceCtrlLatch;

typedef struct SceCtrlRapidFire {
	u32 uiMask;
	u32 uiTrigger;
	u32 uiTarget;
	u8 eventData;
	u8 uiMake;
	u8 uiBreak;
	u8 uiDelay;
} SceCtrlRapidFire;

typedef struct SceCtrlEmulationData {
	u8 analogX;
	u8 analogY;
	u8 pad[2];
	u32 uiAnalogMake;
	u32 userButtons;
	u32 kernelButtons;
	u32 uiButtonMake;
} SceCtrlEmulationData;

typedef struct SceCtrlButtonCallback {
	u32 buttonMask;
	SceKernelButtonCallbackFunction callback;
	u32 gp;
	void *arg;
} SceCtrlButtonCallback;

typedef struct SceCtrlInternalData {
	u32 btnMake;
	u32 btnBreak;
	u32 btnPress;
	u32 btnRelease;
	u32 readLatchCount;
	s32 firstUnReadUpdatedBufIndex;
	s32 curUpdatableBufIndex;
	void *pCtrlInputBuffers[3];
} SceCtrlInternalData;

typedef struct SceCtrl {
	s32 timerID;
	SceUID updateEventId;
	u32 updateCycle;
	u8 samplingMode[2];
	u8 cableTypeReq;
	s8 unk15;
	union {
		struct {
			u8 intr1;
			u8 intr2;
			u8 resv[2];
		} busy;
		u32 status;
	} sysconStatus;
	u8 cancelIdleTimer;
	u8 pollMode;
	s16 suspendSamples;
	s32 sysconTransfersLeft;
	u8 sysPacketPad[0xC0];
	SceCtrlInternalData userModeData;
	SceCtrlInternalData kernelModeData;
	SceCtrlRapidFire rapidFire[16];
	u32 rawButtons;
	u32 prevKernelButtons[2];
	u32 prevButtons;
	u32 userButtons;
	u32 prevModifiedButtons;
	union {
		struct {
			u8 analogX;
			u8 analogY;
			u8 rX;
			u8 rY;
		};
		u8 analog[4];
		u32 analogWord;
	};
	u8 pad248[2];
	u8 unk582;
	u8 unk583;
	SceCtrlEmulationData emulationData[4];
	u32 userModeButtons;
	u32 maskSupportButtons;
	u32 maskApplyButtons;
	u32 idleResetAllSupportedButtons;
	u32 resetButtonsMake;
	u32 resetButtonsPress;
	u32 resetButtonsMakeHoldMode;
	u32 resetButtonsPressHoldMode;
	s32 unHoldThreshold;
	s32 holdThreshold;
	SceCtrlButtonCallback buttonCallback[4];
} SceCtrl;

int sceKernelApplicationType(void);
int sceKernelGetModel(void);
int sceKernelGetCompiledSdkVersion(void);
void *memset(void *dst, int c, SceSize len);
int sceKernelSetAlarm(SceUInt clock, SceUInt (*handler)(void *common), void *common);
int sceKernelPowerTick(int tick_type);
int sceDisplayWaitVblankStart(void);
int sceKermitPeripheral_driver_9EF7501D(void);

int sceSTimerAlloc(void);
int sceSTimerFree(int timer_id);
int sceSTimerStartCount(int timer_id);
int sceSTimerStopCount(int timer_id);
int sceSTimerSetPrscl(int timer_id, int num, int den);
int sceSTimerSetHandler(int timer_id, int cycle, int (*handler)(int timer_id, u32 count, void *common, u32 arg4), void *common);

s32 sceCtrlInit(void);
s32 sceCtrlEnd(void);
s32 sceCtrlSuspend(void);
s32 sceCtrlResume(void);

static int _sceCtrlSysEventHandler(int ev_id, char *ev_name, void *param, int *result);
static int _sceCtrlVblankIntr(int sub_intr, void *arg);
static int _sceCtrlTimerIntr(int timer_id, u32 count, void *common, u32 arg4);
static int _sceCtrlUpdateButtons(u32 rawButtons, const u8 *axes);
static SceUInt _sceCtrlDummyAlarm(void *common);
static int _sceCtrlReadBuf(SceCtrlData2 *pData, u8 nBufs, u32 port, u8 mode);
static void _sceCtrlPollHardware(void);
static SceUInt _sceCtrlPollAlarm(void *common);

static inline u32 pspGetGp(void)
{
	u32 gp;
	__asm__ volatile("move %0, $gp" : "=r"(gp));
	return gp;
}

static inline void pspSetGp(u32 gp)
{
	__asm__ volatile("move $gp, %0" : : "r"(gp));
}

static inline s32 pspMax(s32 a, s32 b)
{
	return (a > b) ? a : b;
}

static SceSysEventHandler g_ctrlSysEvent = {
	.size = sizeof(SceSysEventHandler),
	.name = "SceCtrl",
	.type_mask = 0x00FFFF00,
	.handler = _sceCtrlSysEventHandler,
};

static int g_hasFastSample;
static SceCtrl g_ctrl;
static SceCtrlData g_ctrlUserBufs[64];
static SceCtrlData g_ctrlKernelBufs[64];
static SceCtrlData g_fastSampleBuf;

s32 sceCtrlInit(void)
{
	memset(&g_ctrl, 0, sizeof(SceCtrl));
	g_ctrl.pollMode = 1;
	g_ctrl.userModeData.pCtrlInputBuffers[0] = g_ctrlUserBufs;
	g_ctrl.analogX = 0x80;
	g_ctrl.analogY = 0x80;
	g_ctrl.rX = 0;
	g_ctrl.rY = 0;
	g_ctrl.kernelModeData.pCtrlInputBuffers[0] = g_ctrlKernelBufs;
	g_ctrl.sysconTransfersLeft = -1;

	g_ctrl.updateEventId = sceKernelCreateEventFlag("SceCtrl", 1, 0, NULL);
	s32 timerId = sceSTimerAlloc();
	if (timerId < 0) {
		return timerId;
	}
	g_ctrl.timerID = timerId;
	sceSTimerSetPrscl(timerId, 1, 48);
	g_ctrl.unk15 = -1;

	sceKernelRegisterSysEventHandler(&g_ctrlSysEvent);

	s32 appType = sceKernelApplicationType();
	u32 userButtons;
	if (appType == 0x110 || appType == 0x300 || appType == 0x100) {
		userButtons = 0x0003FFFFu;
	} else {
		userButtons = 0x0003F3F9u;
	}
	g_ctrl.userModeButtons = userButtons;
	g_ctrl.maskApplyButtons = 0;
	g_ctrl.maskSupportButtons = userButtons;

	s32 model = sceKernelGetModel();
	switch (model) {
	case 4:
	case 5:
	case 7:
	case 9:
		g_ctrl.idleResetAllSupportedButtons = 0x39FFF3F9u;
		break;
	default:
		g_ctrl.idleResetAllSupportedButtons = 0x01FFF3F9u;
		break;
	}

	g_ctrl.resetButtonsMake = 0x39FFF3F9u;
	g_ctrl.resetButtonsPress = 0x00F1F3F9u;
	g_ctrl.resetButtonsMakeHoldMode = 0x390E0000u;
	g_ctrl.resetButtonsPressHoldMode = 0;
	g_ctrl.unHoldThreshold = 0x81;
	g_ctrl.holdThreshold = 0x81;

	sceKernelRegisterSubIntrHandler(0x1E, 0x13, _sceCtrlVblankIntr, NULL);
	sceKernelEnableSubIntr(0x1E, 0x13);
	return 0;
}

s32 sceCtrlEnd(void)
{
	sceKernelUnregisterSysEventHandler(&g_ctrlSysEvent);
	s32 timerId = g_ctrl.timerID;
	g_ctrl.timerID = -1;
	if (timerId >= 0) {
		sceSTimerStopCount(timerId);
		sceSTimerFree(timerId);
	}
	sceKernelReleaseSubIntrHandler(0x1E, 0x13);
	sceKernelDeleteEventFlag(g_ctrl.updateEventId);
	while (g_ctrl.sysconStatus.status != 0) {
		sceDisplayWaitVblankStart();
	}
	return 0;
}

static int _sceCtrlSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	if (ev_id == 0x402) {
		if (g_ctrl.sysconStatus.status == 0) {
			return 0;
		}
		if (g_ctrl.sysconTransfersLeft == 0) {
			return 0;
		}
		return 0x80000021;
	}
	if (ev_id < 0x403) {
		if (ev_id == 0x400) {
			g_ctrl.sysconTransfersLeft = g_ctrl.suspendSamples;
		}
		return 0;
	}
	if (ev_id == 0x400C) {
		if (g_ctrl.updateCycle == 0) {
			sceKernelDisableSubIntr(0x1E, 0x13);
		} else {
			sceSTimerStopCount(g_ctrl.timerID);
		}
		return 0;
	}
	if (ev_id == 0x1000C) {
		g_ctrl.unk15 = -1;
		if (g_ctrl.updateCycle == 0) {
			sceKernelEnableSubIntr(0x1E, 0x13);
		} else {
			sceSTimerStartCount(g_ctrl.timerID);
			sceSTimerSetHandler(g_ctrl.timerID, g_ctrl.updateCycle, _sceCtrlTimerIntr, NULL);
		}
		g_ctrl.sysconTransfersLeft = -1;
	}
	return 0;
}

static int _sceCtrlVblankIntr(int sub_intr, void *arg)
{
	int intr = sceKernelCpuSuspendIntr();
	if (g_ctrl.updateCycle == 0) {
		if (g_ctrl.sysconStatus.busy.intr1 == 0 && g_ctrl.pollMode != 0) {
			g_ctrl.sysconStatus.busy.intr1 = 1;
			sceKernelSetAlarm(700, _sceCtrlPollAlarm, NULL);
		} else {
			sceKernelSetAlarm(700, _sceCtrlDummyAlarm, NULL);
		}
	}
	if (g_ctrl.cancelIdleTimer != 0) {
		g_ctrl.cancelIdleTimer = 0;
		sceKernelPowerTick(0);
	}
	sceKermitPeripheral_driver_9EF7501D();
	sceKernelCpuResumeIntr(intr);
	return -1;
}

static int _sceCtrlTimerIntr(int timer_id, u32 count, void *common, u32 arg4)
{
	int intr = sceKernelCpuSuspendIntr();
	if (g_ctrl.updateCycle != 0) {
		if (g_ctrl.sysconStatus.busy.intr1 == 0 && g_ctrl.pollMode != 0) {
			g_ctrl.sysconStatus.busy.intr1 = 1;
			_sceCtrlPollHardware();
		} else {
			sceKernelSetAlarm(700, _sceCtrlDummyAlarm, NULL);
		}
	}
	if (g_ctrl.cancelIdleTimer != 0) {
		g_ctrl.cancelIdleTimer = 0;
		sceKernelPowerTick(0);
	}
	sceKermitPeripheral_driver_9EF7501D();
	sceKernelCpuResumeIntr(intr);
	return -1;
}

static int _sceCtrlUpdateButtons(u32 rawButtons, const u8 *axes)
{
	u8 aX = axes[0];
	u8 aY = axes[1];
	u32 sysTimeLow = sceKernelGetSystemTimeLow();

	SceCtrlData *userBuf = (SceCtrlData *)g_ctrl.userModeData.pCtrlInputBuffers[0] + g_ctrl.userModeData.curUpdatableBufIndex;
	SceCtrlData *kernelBuf = (SceCtrlData *)g_ctrl.kernelModeData.pCtrlInputBuffers[0] + g_ctrl.kernelModeData.curUpdatableBufIndex;
	userBuf->timeStamp = sysTimeLow;
	kernelBuf->timeStamp = sysTimeLow;

	for (int i = 0; i < 4; i++) {
		if ((s32)g_ctrl.emulationData[i].uiAnalogMake > 0) {
			g_ctrl.emulationData[i].uiAnalogMake--;
			aX = g_ctrl.emulationData[i].analogX;
			aY = g_ctrl.emulationData[i].analogY;
		}
	}

	userBuf->aX = aX;
	userBuf->aY = aY;
	userBuf->rX = axes[2];
	userBuf->rY = axes[3];

	kernelBuf->aX = aX;
	kernelBuf->aY = aY;
	kernelBuf->rX = axes[2];
	kernelBuf->rY = axes[3];

	if (g_ctrl.samplingMode[0] == 0) {
		userBuf->aY = 0x80;
		userBuf->aX = 0x80;
	}
	if (g_ctrl.samplingMode[1] == 0) {
		kernelBuf->aY = 0x80;
		kernelBuf->aX = 0x80;
	}

	*(u32 *)userBuf->rsrv = 0;
	*(u32 *)kernelBuf->rsrv = 0;

	s32 nextUser = (g_ctrl.userModeData.curUpdatableBufIndex + 1) % 64;
	g_ctrl.userModeData.curUpdatableBufIndex = nextUser;
	if (nextUser == g_ctrl.userModeData.firstUnReadUpdatedBufIndex) {
		g_ctrl.userModeData.firstUnReadUpdatedBufIndex = (nextUser + 1) % 64;
	}

	s32 nextKernel = (g_ctrl.kernelModeData.curUpdatableBufIndex + 1) % 64;
	g_ctrl.kernelModeData.curUpdatableBufIndex = nextKernel;
	if (nextKernel == g_ctrl.kernelModeData.firstUnReadUpdatedBufIndex) {
		g_ctrl.kernelModeData.firstUnReadUpdatedBufIndex = (nextKernel + 1) % 64;
	}

	u32 updatedButtons = rawButtons;
	if ((rawButtons & 0x00020000u) != 0) {
		updatedButtons &= 0x3B0E0000u;
	}
	for (int i = 0; i < 4; i++) {
		updatedButtons |= g_ctrl.emulationData[i].kernelButtons;
	}
	kernelBuf->buttons = updatedButtons;

	u32 unpressedButtons = ~updatedButtons;
	u32 changedButtons = g_ctrl.prevModifiedButtons ^ updatedButtons;
	g_ctrl.prevModifiedButtons = updatedButtons;
	g_ctrl.kernelModeData.btnPress |= updatedButtons;
	g_ctrl.kernelModeData.btnRelease |= unpressedButtons;
	g_ctrl.kernelModeData.btnMake |= changedButtons & updatedButtons;
	g_ctrl.kernelModeData.btnBreak |= changedButtons & unpressedButtons;
	g_ctrl.kernelModeData.readLatchCount++;

	updatedButtons = rawButtons & g_ctrl.maskSupportButtons;
	if ((updatedButtons & 0x00020000u) != 0) {
		updatedButtons &= 0x00030000u;
	}

	u32 uModeEmuAll = 0;
	u32 kModeEmuAll = 0;
	for (int i = 0; i < 4; i++) {
		u32 uBtn = g_ctrl.emulationData[i].userButtons;
		u32 kBtn = g_ctrl.emulationData[i].kernelButtons;
		s32 count = (s32)g_ctrl.emulationData[i].uiButtonMake;
		uModeEmuAll |= uBtn;
		kModeEmuAll |= kBtn;
		if (count > 0) {
			count--;
			g_ctrl.emulationData[i].uiButtonMake = (u32)count;
			if (count == 0) {
				g_ctrl.emulationData[i].userButtons = 0;
				g_ctrl.emulationData[i].kernelButtons = 0;
			}
		}
	}

	updatedButtons |= uModeEmuAll;
	u32 savedGp = pspGetGp();
	u32 curButtons = rawButtons | kModeEmuAll;
	u32 prevButtons = g_ctrl.prevButtons;
	g_ctrl.prevButtons = curButtons;
	u32 diffButtons = curButtons ^ prevButtons;

	for (int i = 0; i < 4; i++) {
		if ((diffButtons & g_ctrl.buttonCallback[i].buttonMask) != 0) {
			if (g_ctrl.buttonCallback[i].callback != NULL) {
				pspSetGp(g_ctrl.buttonCallback[i].gp);
				g_ctrl.buttonCallback[i].callback(curButtons, prevButtons, g_ctrl.buttonCallback[i].arg);
			}
		}
	}
	pspSetGp(savedGp);

	for (int i = 0; i < 16; i++) {
		SceCtrlRapidFire *rf = &g_ctrl.rapidFire[15 - i];
		if (rf->uiMask != 0) {
			if ((updatedButtons & rf->uiMask) == rf->uiTrigger) {
				u8 eventData = rf->eventData;
				u8 timeLeft = (u8)((eventData & 0x3Fu) - 1u);
				u32 mode = eventData >> 6;
				if (mode == 0) {
					timeLeft = rf->uiDelay;
					mode = 2;
				} else {
					if (mode == 2) {
						updatedButtons |= rf->uiTarget;
					} else {
						updatedButtons &= ~rf->uiTarget;
					}
					if (timeLeft == 0) {
						mode ^= 1u;
						timeLeft = ((mode & 1u) != 0) ? rf->uiBreak : rf->uiMake;
					}
				}
				rf->eventData = (u8)((mode << 6) | timeLeft);
			} else {
				rf->eventData = 0;
			}
		}
	}

	updatedButtons = (updatedButtons & g_ctrl.maskSupportButtons) | g_ctrl.maskApplyButtons;
	if ((updatedButtons & 0x00020000u) != 0) {
		if (uModeEmuAll != 0 && (updatedButtons & g_ctrl.userModeButtons) != 0) {
			updatedButtons &= ~0x00020000u;
		}
	}

	changedButtons = g_ctrl.userButtons ^ updatedButtons;
	if ((updatedButtons & 0x00020000u) != 0) {
		updatedButtons &= 0x00030000u;
		changedButtons = g_ctrl.userButtons ^ updatedButtons;
	}

	if ((changedButtons & 0x00020000u) != 0) {
		g_ctrl.unk582 = g_ctrl.unk583;
	}
	if (g_ctrl.unk582 != 0) {
		updatedButtons &= 0x00010000u;
		g_ctrl.unk582--;
		changedButtons = g_ctrl.userButtons ^ updatedButtons;
	}
	userBuf->buttons = updatedButtons;
	g_ctrl.userButtons = updatedButtons;

	unpressedButtons = ~updatedButtons;
	g_ctrl.userModeData.btnMake |= changedButtons & updatedButtons;
	g_ctrl.userModeData.btnBreak |= changedButtons & unpressedButtons;
	g_ctrl.userModeData.btnPress |= updatedButtons;
	g_ctrl.userModeData.btnRelease |= unpressedButtons;
	g_ctrl.userModeData.readLatchCount++;

	return 0;
}

s32 sceCtrlSetSamplingMode(u8 mode)
{
	if (mode >= 2) {
		return 0x80000107;
	}
	int intr = sceKernelCpuSuspendIntr();
	u32 isUser = (pspGetK1() >> 20) & 1u;
	u32 idx = (isUser == 0) ? 1u : 0u;
	s32 prev = g_ctrl.samplingMode[idx];
	g_ctrl.samplingMode[idx] = mode;
	sceKernelCpuResumeIntr(intr);
	return prev;
}

u32 sceCtrlGetSamplingMode(u8 *pMode)
{
	u32 oldK1 = pspGetK1();
	u32 shiftedK1 = oldK1 << 11;
	u32 idx = (shiftedK1 >> 31) ^ 1u;
	pspSetK1(shiftedK1);
	if (((s32)(shiftedK1 & (u32)pMode)) >= 0) {
		*(u32 *)pMode = g_ctrl.samplingMode[idx];
	}
	pspSetK1(oldK1);
	return 0;
}

s32 sceCtrlSetSamplingCycle(u32 cycle)
{
	u32 oldK1 = pspGetK1();
	pspSetK1(oldK1 << 11);
	int intr = sceKernelCpuSuspendIntr();
	s32 prevCycle;

	if (cycle == 0) {
		prevCycle = (s32)g_ctrl.updateCycle;
		g_ctrl.updateCycle = 0;
		sceKernelEnableSubIntr(0x1E, 0x13);
		sceSTimerSetHandler(g_ctrl.timerID, 0, NULL, NULL);
		sceSTimerStopCount(g_ctrl.timerID);
	} else if (cycle < 5555u || cycle > 20000u) {
		prevCycle = 0x800001FE;
	} else {
		prevCycle = (s32)g_ctrl.updateCycle;
		g_ctrl.updateCycle = cycle;
		sceSTimerStartCount(g_ctrl.timerID);
		s32 sdkVer = sceKernelGetCompiledSdkVersion();
		u32 adjCycle = (sdkVer > 0x0204FFFF) ? cycle : (cycle + 1u);
		sceSTimerSetHandler(g_ctrl.timerID, (int)adjCycle, _sceCtrlTimerIntr, NULL);
		sceKernelDisableSubIntr(0x1E, 0x13);
	}

	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return prevCycle;
}

s32 sceCtrlReadBufferPositive(SceCtrlData *pData, u8 nBufs)
{
	u32 oldK1 = pspGetK1();
	u32 shiftedK1 = oldK1 << 11;
	if (g_hasFastSample == 0) {
		return _sceCtrlReadBuf((SceCtrlData2 *)pData, nBufs, 0, 2);
	}
	pspSetK1(shiftedK1);
	g_hasFastSample = 0;
	if (((s32)(shiftedK1 & (u32)pData)) < 0) {
		pspSetK1(oldK1);
		return 0x80000023;
	}
	if ((s32)shiftedK1 < 0) {
		g_fastSampleBuf.buttons &= g_ctrl.userModeButtons;
	}
	pData->timeStamp = g_fastSampleBuf.timeStamp;
	pData->buttons = g_fastSampleBuf.buttons;
	pData->aX = g_fastSampleBuf.aX;
	pData->aY = g_fastSampleBuf.aY;
	pData->rX = 0;
	pData->rY = 0;
	pData->rsrv[0] = 0;
	pData->rsrv[1] = 0;
	pData->rsrv[2] = 0;
	pData->rsrv[3] = 0;
	pspSetK1(oldK1);
	return 1;
}

s32 sceCtrlPeekLatch(SceCtrlLatch *pLatch)
{
	u32 oldK1 = pspGetK1();
	u32 shiftedK1 = oldK1 << 11;
	pspSetK1(shiftedK1);
	int intr = sceKernelCpuSuspendIntr();
	if (((s32)(shiftedK1 & (u32)pLatch)) < 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(oldK1);
		return 0x80000023;
	}
	SceCtrlInternalData *data = ((s32)shiftedK1 < 0) ? &g_ctrl.userModeData : &g_ctrl.kernelModeData;
	pLatch->buttonMake = data->btnMake;
	pLatch->buttonBreak = data->btnBreak;
	pLatch->buttonPress = data->btnPress;
	pLatch->buttonRelease = data->btnRelease;
	s32 count = (s32)data->readLatchCount;
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return count;
}

s32 sceCtrlReadLatch(SceCtrlLatch *pLatch)
{
	u32 oldK1 = pspGetK1();
	u32 shiftedK1 = oldK1 << 11;
	pspSetK1(shiftedK1);
	int intr = sceKernelCpuSuspendIntr();
	if (((s32)(shiftedK1 & (u32)pLatch)) < 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(oldK1);
		return 0x80000023;
	}
	SceCtrlInternalData *data = ((s32)shiftedK1 < 0) ? &g_ctrl.userModeData : &g_ctrl.kernelModeData;
	pLatch->buttonMake = data->btnMake;
	pLatch->buttonBreak = data->btnBreak;
	pLatch->buttonPress = data->btnPress;
	pLatch->buttonRelease = data->btnRelease;
	s32 count = (s32)data->readLatchCount;
	data->btnMake = 0;
	data->btnBreak = 0;
	data->btnPress = 0;
	data->btnRelease = 0;
	data->readLatchCount = 0;
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return count;
}

u32 sceCtrlGetIdleCancelKey(u32 *pResetButtonsMake, u32 *pResetButtonsPress, u32 *pResetButtonsMakeHoldMode, u32 *pResetButtonsPressHoldMode)
{
	if (pResetButtonsMake != NULL) {
		*pResetButtonsMake = g_ctrl.resetButtonsMake;
	}
	if (pResetButtonsPress != NULL) {
		*pResetButtonsPress = g_ctrl.resetButtonsPress;
	}
	if (pResetButtonsMakeHoldMode != NULL) {
		*pResetButtonsMakeHoldMode = g_ctrl.resetButtonsMakeHoldMode;
	}
	if (pResetButtonsPressHoldMode != NULL) {
		*pResetButtonsPressHoldMode = g_ctrl.resetButtonsPressHoldMode;
	}
	return 0;
}

s32 sceCtrlSetIdleCancelThreshold(s32 unHoldThreshold, s32 holdThreshold)
{
	if ((u32)(unHoldThreshold + 1) >= 0x82u || holdThreshold < -1 || holdThreshold > 0x80) {
		return 0x800001FE;
	}
	if (unHoldThreshold == -1) {
		unHoldThreshold = 0x81;
	}
	if (holdThreshold == -1) {
		holdThreshold = 0x81;
	}
	int intr = sceKernelCpuSuspendIntr();
	g_ctrl.holdThreshold = holdThreshold;
	g_ctrl.unHoldThreshold = unHoldThreshold;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

s32 sceCtrlGetIdleCancelThreshold(s32 *pUnHoldThreshold, s32 *pHoldThreshold)
{
	u32 oldK1 = pspGetK1();
	u32 shiftedK1 = oldK1 << 11;
	pspSetK1(shiftedK1);
	if (((s32)(shiftedK1 & (u32)pUnHoldThreshold)) < 0 || ((s32)(shiftedK1 & (u32)pHoldThreshold)) < 0) {
		pspSetK1(oldK1);
		return 0x80000023;
	}
	int intr = sceKernelCpuSuspendIntr();
	if (pUnHoldThreshold != NULL) {
		s32 val = g_ctrl.unHoldThreshold;
		*pUnHoldThreshold = (val == 0x81) ? -1 : val;
	}
	if (pHoldThreshold != NULL) {
		s32 val = g_ctrl.holdThreshold;
		*pHoldThreshold = (val == 0x81) ? -1 : val;
	}
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return 0;
}

u32 sceCtrlSetButtonIntercept(u32 buttons, u32 buttonMaskMode)
{
	u32 prev = 1;
	int intr = sceKernelCpuSuspendIntr();
	if ((g_ctrl.maskSupportButtons & buttons) != 0) {
		prev = ((g_ctrl.maskApplyButtons & buttons) != 0) ? 2 : 0;
	}
	if (buttonMaskMode == 0) {
		g_ctrl.maskApplyButtons &= ~buttons;
		g_ctrl.maskSupportButtons |= buttons;
	} else if (buttonMaskMode == 1) {
		g_ctrl.maskApplyButtons &= ~buttons;
		g_ctrl.maskSupportButtons &= ~buttons;
	} else if (buttonMaskMode == 2) {
		g_ctrl.maskApplyButtons |= buttons;
		g_ctrl.maskSupportButtons |= buttons;
	}
	sceKernelCpuResumeIntr(intr);
	return prev;
}

u32 sceCtrlGetButtonIntercept(u32 buttons)
{
	u32 mode = 1;
	int intr = sceKernelCpuSuspendIntr();
	if ((g_ctrl.maskSupportButtons & buttons) != 0) {
		mode = ((g_ctrl.maskApplyButtons & buttons) != 0) ? 2 : 0;
	}
	sceKernelCpuResumeIntr(intr);
	return mode;
}

s32 sceCtrlSetRapidFire(u8 slot, u32 uiMask, u32 uiTrigger, u32 uiTarget, u8 uiDelay, u8 uiMake, u8 uiBreak)
{
	if (slot >= 16) {
		return 0x80000102;
	}
	u32 combined = (u32)uiDelay | (u32)uiMake | (u32)uiBreak;
	if ((combined - 1u) >= 0x3Fu) {
		return 0x800001FE;
	}
	u32 oldK1 = pspGetK1();
	u32 shiftedK1 = oldK1 << 11;
	pspSetK1(shiftedK1);
	if ((s32)shiftedK1 < 0) {
		u32 forbidden = (~g_ctrl.userModeButtons) | 0x00020000u;
		if (((uiMask | uiTrigger | uiTarget) & forbidden) != 0) {
			pspSetK1(oldK1);
			return 0x80000023;
		}
	}
	int intr = sceKernelCpuSuspendIntr();
	g_ctrl.rapidFire[slot].uiMake = uiMake;
	g_ctrl.rapidFire[slot].uiDelay = uiDelay;
	g_ctrl.rapidFire[slot].uiMask = uiMask;
	g_ctrl.rapidFire[slot].uiTrigger = uiTrigger;
	g_ctrl.rapidFire[slot].uiTarget = uiTarget;
	g_ctrl.rapidFire[slot].uiBreak = uiBreak;
	g_ctrl.rapidFire[slot].eventData = 0;
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return 0;
}

int module_start(SceSize args, void *argp)
{
	sceCtrlInit();
	return 0;
}

int module_reboot_before(SceSize args, void *argp)
{
	sceCtrlEnd();
	return 0;
}

s32 sceCtrlSuspend(void)
{
	if (g_ctrl.updateCycle == 0) {
		sceKernelDisableSubIntr(0x1E, 0x13);
	} else {
		sceSTimerStopCount(g_ctrl.timerID);
	}
	return 0;
}

s32 sceCtrlResume(void)
{
	g_ctrl.unk15 = -1;
	if (g_ctrl.updateCycle == 0) {
		sceKernelEnableSubIntr(0x1E, 0x13);
	} else {
		sceSTimerStartCount(g_ctrl.timerID);
		sceSTimerSetHandler(g_ctrl.timerID, (int)g_ctrl.updateCycle, _sceCtrlTimerIntr, NULL);
	}
	return 0;
}

u32 sceCtrl_driver_5886194C(u8 arg1)
{
	g_ctrl.unk583 = arg1;
	return 0;
}

u32 sceCtrl_driver_6C86AF22(s32 arg1)
{
	return 0;
}

s32 sceCtrl_driver_E467BEC8(u8 externalPort, void *transferHandler, void *inputSource)
{
	return 0;
}

u32 sceCtrlGetSamplingCycle(u32 *pCycle)
{
	u32 oldK1 = pspGetK1();
	u32 shiftedK1 = oldK1 << 11;
	if (((s32)(shiftedK1 & (u32)pCycle)) >= 0) {
		*pCycle = g_ctrl.updateCycle;
	}
	pspSetK1(oldK1);
	return 0;
}

s32 sceCtrlPeekBufferPositive(SceCtrlData *pData, u8 nBufs)
{
	return _sceCtrlReadBuf((SceCtrlData2 *)pData, nBufs, 0, 0);
}

s32 sceCtrlPeekBufferNegative(SceCtrlData *pData, u8 nBufs)
{
	return _sceCtrlReadBuf((SceCtrlData2 *)pData, nBufs, 0, 1);
}

s32 sceCtrlReadBufferNegative(SceCtrlData *pData, u8 nBufs)
{
	return _sceCtrlReadBuf((SceCtrlData2 *)pData, nBufs, 0, 3);
}

s32 sceCtrlPeekBufferPositive2(u32 port, SceCtrlData2 *pData, u8 nBufs)
{
	return _sceCtrlReadBuf(pData, nBufs, port, 4);
}

s32 sceCtrlPeekBufferNegative2(u32 port, SceCtrlData2 *pData, u8 nBufs)
{
	return _sceCtrlReadBuf(pData, nBufs, port, 5);
}

s32 sceCtrlReadBufferPositive2(u32 port, SceCtrlData2 *pData, u8 nBufs)
{
	return _sceCtrlReadBuf(pData, nBufs, port, 6);
}

s32 sceCtrlReadBufferNegative2(u32 port, SceCtrlData2 *pData, u8 nBufs)
{
	return _sceCtrlReadBuf(pData, nBufs, port, 7);
}

u32 sceCtrlSetIdleCancelKey(u32 resetButtonsMake, u32 resetButtonsPress, u32 resetButtonsMakeHoldMode, u32 resetButtonsPressHoldMode)
{
	g_ctrl.resetButtonsPressHoldMode = resetButtonsPressHoldMode;
	g_ctrl.resetButtonsMake = resetButtonsMake;
	g_ctrl.resetButtonsPress = resetButtonsPress;
	g_ctrl.resetButtonsMakeHoldMode = resetButtonsMakeHoldMode;
	return 0;
}

u32 sceCtrlSetPollingMode(u8 pollMode)
{
	g_ctrl.pollMode = pollMode;
	return 0;
}

s32 sceCtrlSetSuspendingExtraSamples(s16 suspendSamples)
{
	if ((u16)suspendSamples >= 0x12Du) {
		return 0x800001FE;
	}
	if (suspendSamples == 1) {
		suspendSamples = 0;
	}
	g_ctrl.suspendSamples = suspendSamples;
	return 0;
}

s16 sceCtrlGetSuspendingExtraSamples(void)
{
	return g_ctrl.suspendSamples;
}

s32 sceCtrlSetButtonEmulation(u8 slot, u32 userButtons, u32 kernelButtons, u32 uiMake)
{
	if (slot >= 4) {
		return 0x800001FE;
	}
	g_ctrl.emulationData[slot].uiButtonMake = uiMake;
	g_ctrl.emulationData[slot].userButtons = userButtons;
	g_ctrl.emulationData[slot].kernelButtons = kernelButtons;
	return 0;
}

s32 sceCtrlSetAnalogEmulation(u8 slot, u8 aX, u8 aY, u32 uiMake)
{
	if (slot >= 4) {
		return 0x800001FE;
	}
	g_ctrl.emulationData[slot].analogY = aY;
	g_ctrl.emulationData[slot].uiAnalogMake = uiMake;
	g_ctrl.emulationData[slot].analogX = aX;
	return 0;
}

s32 sceCtrlSetSpecialButtonCallback(u32 slot, u32 buttonMask, SceKernelButtonCallbackFunction callback, void *opt)
{
	if (slot >= 4) {
		return 0x80000102;
	}
	int intr = sceKernelCpuSuspendIntr();
	g_ctrl.buttonCallback[slot].buttonMask = buttonMask;
	g_ctrl.buttonCallback[slot].callback = callback;
	g_ctrl.buttonCallback[slot].arg = opt;
	g_ctrl.buttonCallback[slot].gp = pspGetGp();
	sceKernelCpuResumeIntr(intr);
	return 0;
}

u32 sceCtrlUpdateCableTypeReq(void)
{
	g_ctrl.cableTypeReq = 1;
	return 0;
}

s32 sceCtrlClearRapidFire(u8 slot)
{
	if (slot >= 16) {
		return 0x80000102;
	}
	g_ctrl.rapidFire[slot].uiMask = 0;
	return 0;
}

static SceUInt _sceCtrlDummyAlarm(void *common)
{
	int intr = sceKernelCpuSuspendIntr();
	u32 rawButtons = g_ctrl.rawButtons;
	if (g_ctrl.sysconTransfersLeft == 0) {
		rawButtons &= ~0x0001FFFFu;
	}
	_sceCtrlUpdateButtons(rawButtons, g_ctrl.analog);
	sceKernelSetEventFlag(g_ctrl.updateEventId, 1);
	sceKernelCpuResumeIntr(intr);
	return 0;
}

static int _sceCtrlReadBuf(SceCtrlData2 *pData, u8 nBufs, u32 port, u8 mode)
{
	if ((u32)(nBufs - 1u) >= 64u) {
		return 0x80000104;
	}
	if (port >= 3u) {
		return 0x800001FE;
	}
	if (port != 0 && (mode & 2u) != 0) {
		return 0x80000004;
	}
	if ((mode & 4u) != 0) {
		return 0x80000004;
	}

	u32 oldK1 = pspGetK1();
	u32 shiftedK1 = oldK1 << 11;
	pspSetK1(shiftedK1);
	if (((s32)(shiftedK1 & (u32)pData)) < 0) {
		pspSetK1(oldK1);
		return 0x80000023;
	}

	SceCtrlInternalData *intData = ((s32)shiftedK1 < 0) ? &g_ctrl.userModeData : &g_ctrl.kernelModeData;
	if (port != 0 && intData->pCtrlInputBuffers[port] == NULL) {
		pspSetK1(oldK1);
		return 0x80000004;
	}

	int intr;
	s32 startIdx;
	s32 numRead;

	if ((mode & 2u) != 0) {
		int res = sceKernelWaitEventFlag(g_ctrl.updateEventId, 1, 1, NULL, NULL);
		if (res < 0) {
			pspSetK1(oldK1);
			return res;
		}
		intr = sceKernelCpuSuspendIntr();
		sceKernelClearEventFlag(g_ctrl.updateEventId, ~1u);
		startIdx = intData->firstUnReadUpdatedBufIndex;
		numRead = intData->curUpdatableBufIndex - startIdx;
		if (numRead < 0) {
			numRead += 64;
		}
		intData->firstUnReadUpdatedBufIndex = intData->curUpdatableBufIndex;
		if ((s32)nBufs < numRead) {
			startIdx = intData->curUpdatableBufIndex - (s32)nBufs;
			if (startIdx < 0) {
				startIdx += 64;
			}
			numRead = nBufs;
		}
	} else {
		intr = sceKernelCpuSuspendIntr();
		numRead = 64;
		startIdx = intData->curUpdatableBufIndex;
		if ((s32)nBufs < 64) {
			startIdx -= (s32)nBufs;
			if (startIdx < 0) {
				startIdx += 64;
			}
			numRead = nBufs;
		}
	}

	u8 *src = (port == 0)
		? ((u8 *)intData->pCtrlInputBuffers[0] + (startIdx << 4))
		: ((u8 *)intData->pCtrlInputBuffers[port] + (startIdx * 0x30));
	SceCtrlData *dst = (SceCtrlData *)pData;

	for (s32 i = 0; i < numRead; i++) {
		SceCtrlData *s = (SceCtrlData *)src;
		dst->timeStamp = s->timeStamp;
		u32 buttons = s->buttons;
		if ((s32)shiftedK1 < 0) {
			buttons &= g_ctrl.userModeButtons;
		}
		if ((mode & 1u) != 0) {
			buttons = ~buttons;
		}
		dst->buttons = buttons;
		dst->aX = s->aX;
		dst->aY = s->aY;
		dst->rX = s->rX;
		dst->rY = s->rY;
		dst->rsrv[0] = 0;
		dst->rsrv[1] = 0;
		dst->rsrv[2] = 0;
		dst->rsrv[3] = 0;

		startIdx++;
		if (startIdx == 64) {
			startIdx = 0;
			src = (u8 *)intData->pCtrlInputBuffers[port];
		} else {
			src += (port == 0) ? 0x10 : 0x30;
		}
		dst++;
	}

	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return numRead;
}

static void _sceCtrlPollHardware(void)
{
	g_ctrl.sysconStatus.busy.intr1 = 0;
	if (g_ctrl.sysconTransfersLeft > 0) {
		g_ctrl.sysconTransfersLeft--;
	}
	if (g_ctrl.sysconTransfersLeft == 0) {
		int intr = sceKernelCpuSuspendIntr();
		u32 rawButtons = g_ctrl.rawButtons;
		if (g_ctrl.sysconTransfersLeft == 0) {
			rawButtons &= ~0x0001FFFFu;
		}
		_sceCtrlUpdateButtons(rawButtons, g_ctrl.analog);
		sceKernelSetEventFlag(g_ctrl.updateEventId, 1);
		sceKernelCpuResumeIntr(intr);
		return;
	}

	u32 prevButtons = g_ctrl.rawButtons;
	u32 hwBuf[4];
	u32 seq;
	do {
		seq = HW(0xBFC00140u);
		sceKernelMemcpy(hwBuf, (const void *)0xBFC00120u, 0x10);
	} while (hwBuf[0] != seq);

	u32 curButtons = hwBuf[1];
	g_ctrl.rawButtons = curButtons;
	g_ctrl.analogWord = hwBuf[2];
	u8 aX = (u8)hwBuf[2];
	u8 aY = (u8)(hwBuf[2] >> 8);
	_sceCtrlUpdateButtons(curButtons, g_ctrl.analog);

	if (g_ctrl.cancelIdleTimer == 0) {
		u32 changed = curButtons ^ prevButtons;
		u32 makeMask;
		u32 pressMask;
		s32 threshold;
		if ((curButtons & 0x00020000u) != 0) {
			makeMask = g_ctrl.resetButtonsMakeHoldMode;
			pressMask = g_ctrl.resetButtonsPressHoldMode;
			threshold = g_ctrl.holdThreshold;
		} else {
			makeMask = g_ctrl.resetButtonsMake;
			pressMask = g_ctrl.resetButtonsPress;
			threshold = g_ctrl.unHoldThreshold;
		}
		u32 active = ((changed & makeMask) | (curButtons & pressMask)) & g_ctrl.idleResetAllSupportedButtons;
		if (active != 0) {
			g_ctrl.cancelIdleTimer = 1;
		} else if (g_ctrl.samplingMode[0] != 0) {
			s32 dx = (s32)aX - 0x80;
			s32 dy = (s32)aY - 0x80;
			u32 axDist = (u32)pspMax(dx, -dx) & 0xFFu;
			u32 ayDist = (u32)pspMax(dy, -dy) & 0xFFu;
			if (axDist == 0x7Fu) {
				axDist = 0x80u;
			}
			if (ayDist == 0x7Fu) {
				ayDist = 0x80u;
			}
			if ((s32)axDist >= threshold || (s32)ayDist >= threshold) {
				g_ctrl.cancelIdleTimer = 1;
			}
		}
	}

	sceKernelSetEventFlag(g_ctrl.updateEventId, 1);
}

static SceUInt _sceCtrlPollAlarm(void *common)
{
	_sceCtrlPollHardware();
	return 0;
}
