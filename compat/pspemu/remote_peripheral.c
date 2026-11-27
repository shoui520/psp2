/*
 * remote_peripheral.c: Kermit peripheral/system-service server (channel 9)
 * and periodic battery/power/peripheral shared-state updater for ScePspemu.
 */
#include "host_common.h"

#define PERIPHERAL_CHANNEL_ID               9
#define PERIPHERAL_CMD_BASE                 1041
#define PERIPHERAL_ERROR_PERIPH_MASK        0xFFFF0000U
#define PERIPHERAL_ERROR_PERIPH_CLASS       0x802B0000U

int g_peripheralBusy = 1;

int g_peripheralSoundExclusive;
int g_peripheralPsLocked;
int g_peripheralBootFinished;
int g_peripheralKernelOpenFlag;
int g_peripheralGuestFlag;
int g_peripheralPowerExtraFlag;

static uint64_t __attribute__((noinline, noclone)) peripheralGetCompatPair64(int lowParamIdx, int highParamIdx)
{
	int lowVal = pspemuCompatGetParam(lowParamIdx);
	if (lowVal == -1)
		return 0ULL;
	int highVal = pspemuCompatGetParam(highParamIdx);
	if (highVal == -1)
		return 0ULL;
	return ((uint64_t)(uint32_t)highVal << 32) | (uint64_t)(uint32_t)lowVal;
}

static int remotePeripheralThread(SceSize args, void *argp)
{
	PspemuKermitRequest *req;

	(void)args;
	(void)argp;

	for (;;) {
		int64_t retVal;
		int cmd = remotefsWaitAndGetRequest(PERIPHERAL_CHANNEL_ID, &req);

		switch (cmd - PERIPHERAL_CMD_BASE) {
		case 0: {
			int allowed = vshSblQafMgrIsAllowPSPEmuSelectNPEnv();
			if (allowed != 1)
				allowed = (vshSblUtMgrHasStoreFlag() == 1) ? 1 : 0;
			retVal = allowed;
			break;
		}
		case 1: {
			uint32_t pairIdx = (uint32_t)req->args[0];
			if (pspemuIsPs1Mode() != 0) {
				retVal = 0;
			} else if (pairIdx == 0U) {
				retVal = (int64_t)peripheralGetCompatPair64(25, 26);
			} else if (pairIdx == 1U) {
				retVal = (int64_t)peripheralGetCompatPair64(27, 28);
			} else if (pairIdx == 2U) {
				retVal = (int64_t)peripheralGetCompatPair64(29, 30);
			} else {
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			}
			break;
		}
		case 2:
			if (SceCommonDialog_FA26EB30() != SCE_COMMON_DIALOG_STATUS_RUNNING)
				pspemuSettingsApplyDisplay();
			pspemuSetUseTempFrameBuffer(0);
			remoteLowPriorityUnlockPsButton();
			pspemuPowerUnlock();
			pspemuSetRebootReady();
			retVal = 0;
			break;
		case 3:
			retVal = g_peripheralGuestFlag;
			g_peripheralGuestFlag = 1;
			break;
		case 4:
			if (pspemuIsPs1Mode() != 1) {
				pspemuRecordTitleListFlags(16U);
				retVal = 0xFFFFFFFFLL;
			} else {
				retVal = popsHostHandlePeripheralCmd((uint32_t)req->args[0], (uint32_t)req->args[1]);
			}
			break;
		case 5: {
			uint32_t anyNonZero = (uint32_t)req->args[0] | (uint32_t)req->args[1] | (uint32_t)req->args[2];
			if (anyNonZero != 0U) {
				if ((g_peripheralSoundExclusive != 0 && sceAppMgrReleaseSoundOutExclusive3() != 0) ||
				    sceAppMgrAcquireSoundOutExclusive3(0) != 0) {
					pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
				}
				g_peripheralSoundExclusive = 1;
				retVal = 0;
			} else {
				if (g_peripheralSoundExclusive != 0) {
					if (sceAppMgrReleaseSoundOutExclusive3() != 0)
						pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
					g_peripheralSoundExclusive = 0;
				}
				retVal = 0;
			}
			break;
		}
		case 6:
			retVal = (pspemuCompatHasFlag(16U) != 0) ? 1 : 0;
			break;
		case 7: {
			uint32_t unlockReq = (uint32_t)req->args[0];
			if (unlockReq == 0U) {
				if (g_peripheralPsLocked == 0) {
					remoteLowPriorityLockPsButton();
					pspemuPowerLock();
					g_peripheralPsLocked = 1;
				}
			} else if (unlockReq == 1U && g_peripheralPsLocked == 1) {
				remoteLowPriorityUnlockPsButton();
				pspemuPowerUnlock();
				g_peripheralPsLocked = 0;
			}
			retVal = 0;
			break;
		}
		case 8:
			if (pspemuIsPs1Mode() == 0) {
				uint32_t disableThrottle = (uint32_t)req->args[0];
				if (disableThrottle == 0U)
					remoteNetSetThrottle(1);
				else if (disableThrottle == 1U)
					remoteNetSetThrottle(0);
			}
			retVal = 0;
			break;
		case 9:
			retVal = remoteMeAvcdecSetActive((int)req->args[0]);
			break;
		case 10:
			if (pspemuIsPs1Mode() == 0)
				pspemuTriggerFatalError(0);
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		case 11:
			retVal = (int64_t)sceRtcGetAccumulativeTime();
			break;
		case 12: {
			uint32_t leaf = (uint16_t)req->args[0];
			uint32_t offset = (uint32_t)req->args[1];
			SceSize len = (SceSize)req->args[3];
			void *dstBuf = remotefsTranslateGuestAddr((uint32_t)req->args[2], 2, len);
			int ret = sceCompatIdStorageLookup(leaf, offset, dstBuf, len);
			if (ret >= 0)
				remotefsFlushGuestAddr(dstBuf, len);
			retVal = ret;
			break;
		}
		case 13:
			retVal = (vshSblAimgrIsDEX() != 0) ? 1 : 0;
			break;
		case 14:
			retVal = remoteNetFetchTick(req, sceCompatGetCurrentSecureTick);
			break;
		case 15:
			pspemuSettingsNotifyPopsReady();
			if (pspemuIsPs1Mode() == 1) {
				pspemuCopyPopsTitleStrings();
				remoteLowPriorityUnlockPsButton();
				pspemuPowerUnlock();
			} else {
				pspemuRecordTitleListFlags(16U);
			}
			retVal = 0;
			break;
		case 16:
			if (pspemuCompatHasFlag(512U) == 0)
				pspemuRequestSuspend();
			retVal = 0;
			break;
		case 17:
			remoteMsfsSetBootAllowed(0);
			if (pspemuCompatIsRestrictedBootTitle() != 0) {
				pspemuRecordTitleListFlags(8U);
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			}
			if (pspemuIsRebootReady() != 0) {
				g_peripheralBusy = 1;
				remoteLowPriorityLockPsButton();
				pspemuPowerLock();
				remoteNetResetOnReboot();
				remoteMsfsCloseCachedEboot();
				retVal = remoteFlashfsReset();
			} else {
				retVal = -1;
			}
			break;
		case 18:
			retVal = pspemuCompatHasFlag(4U);
			break;
		case 19:
			remoteFlashfsClearGuest();
			if (pspemuCompatHasFlag(128U) != 0)
				sceKernelDelayThread(2000000U);
			if (pspemuIsPs1Mode() == 0) {
				if (g_peripheralBootFinished == 0) {
					if (SceCommonDialog_FA26EB30() != SCE_COMMON_DIALOG_STATUS_RUNNING)
						pspemuSettingsApplyDisplay();
					pspemuSetUseTempFrameBuffer(0);
					pspemuFinishSplashAndLockModules();
					g_peripheralBootFinished = 1;
				}
				remoteLowPriorityUnlockPsButton();
				pspemuPowerUnlock();
			}
			g_peripheralBusy = 0;
			pspemuSetRebootReady();
			switch (pspemuCompatGetParam(11)) {
			case 1: pspemuApplyTitlePatchReturnZero(); break;
			case 2: pspemuApplyTitlePatchReturnZeroAlt(); break;
			case 3: pspemuApplyTitlePatchClearCallbacks(); break;
			case 4: pspemuApplyTitlePatchMemoryHook(); break;
			case 5: pspemuApplyTitlePatchFrameSync(); break;
			case 6: pspemuApplyTitlePatchShiftPrologue(); break;
			case 7: pspemuApplyTitlePatchShiftPrologueWithHook(); break;
			case 8: pspemuApplyTitlePatchAudioDelay(); break;
			case 9: pspemuApplyTitlePatchAudioDelayAlt(); break;
			case 10: remoteMeAvcdecSetPatchFlag(10); break;
			case 11: remoteMeAvcdecSetPatchFlag(11); break;
			case 12: pspemuApplyTitlePatchBranchTrampoline(); break;
			case 13: pspemuApplyTitlePatchBranchTrampolineAlt(); break;
			case 14: pspemuApplyTitlePatchRelocStubAlt(); break;
			case 15: pspemuApplyTitlePatchRelocStub(); break;
			case 16: pspemuApplyTitlePatchStackCallStub(); break;
			case 17: pspemuApplyTitlePatchInlineJump(); break;
			case 18: pspemuApplyTitlePatchInlineJumpAlt(); break;
			default: break;
			}
			remoteNetSetThrottle(0);
			retVal = 0;
			break;
		case 20:
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		case 21:
			retVal = remoteNetFetchTick(req, sceRtcGetCurrentTick);
			break;
		case 22:
			g_peripheralKernelOpenFlag = (int)req->args[0];
			retVal = 0;
			break;
		default:
			pspemuRecordTitleListFlags(16U);
			retVal = 0xFFFFFFFFLL;
			break;
		}

		remotefsReturnValue(PERIPHERAL_CHANNEL_ID, req, retVal);
	}
}

int remotePeripheralIsBusy(void)
{
	return g_peripheralBusy;
}

int remotePeripheralIsKernelOpenAllowed(void)
{
	return (g_peripheralKernelOpenFlag == 1) ? 1 : 0;
}

int remotePeripheralInit(void)
{
	SceCtrlData padData;

	if (vshSblQafMgrIsAllowPSPEmuScreenShot() != 0) {
		sceCtrlPeekBufferPositive(0, &padData, 1);
		if ((padData.buttons & 0x200U) != 0U) {
			pspemuDrawDebugColorSquare(0xFFFF0000U, 448);
			sceScreenShotEnable();
		}
	}

	SceUID thid = sceKernelCreateThread(
		"ScePspemuRemotePeripheral",
		remotePeripheralThread,
		96,
		0x1000U,
		0,
		0x10000,
		NULL);
	if (thid < 0)
		return thid;

	int ret = sceKernelStartThread(thid, 0, NULL);
	if (ret != 0)
		return ret;

	g_peripheralGuestFlag = 0;
	g_peripheralPowerExtraFlag = 0;
	g_peripheralPsLocked = 0;
	return 0;
}

int remotePeripheralUpdateState(void)
{
	int seq;
	uint32_t stateBits = (sceCompatGetPeripheralState(1) > 0) ? 1U : 0U;

	if (scePowerIsPowerOnline() > 0)
		stateBits |= 2U;
	if (scePowerIsBatteryExist() > 0)
		stateBits |= 4U;
	if (scePowerIsBatteryCharging() > 0)
		stateBits |= 8U;
	if (scePowerIsLowBattery() > 0)
		stateBits |= 0x10U;
	if (sceCompatGetPeripheralState(4) > 0)
		stateBits |= 0x20U;
	if (g_peripheralPowerExtraFlag != 0)
		stateBits |= 0x40U;

	if (sceCompatReadShared32(0, &seq) == 0) {
		seq++;
		sceCompatWriteShared32(128, seq);
		sceCompatWriteShared32(130, stateBits);
		sceCompatWriteShared32(129, seq);
	}

	int batPercent = sceCompatGetPeripheralState(2);
	if (batPercent < 0 && ((uint32_t)batPercent & PERIPHERAL_ERROR_PERIPH_MASK) != PERIPHERAL_ERROR_PERIPH_CLASS)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	if (sceCompatReadShared32(1, &seq) == 0) {
		seq++;
		sceCompatWriteShared32(131, seq);
		sceCompatWriteShared32(133, (uint32_t)batPercent);
		sceCompatWriteShared32(132, seq);
	}

	int batLifeTime = sceCompatGetPeripheralState(3);
	if (batLifeTime < 0 && ((uint32_t)batLifeTime & PERIPHERAL_ERROR_PERIPH_MASK) != PERIPHERAL_ERROR_PERIPH_CLASS)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	int ret = sceCompatReadShared32(2, &seq);
	if (ret == 0) {
		seq++;
		sceCompatWriteShared32(134, seq);
		sceCompatWriteShared32(136, (uint32_t)batLifeTime);
		ret = sceCompatWriteShared32(135, seq);
	}
	return ret;
}
