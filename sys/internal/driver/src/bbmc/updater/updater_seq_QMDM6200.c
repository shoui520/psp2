/*
 * The QMDM6200 update sequence: the states _udSeqChkComPower hands every message to once
 * the modem is powered. It checks the modem's images against the package, writes the
 * boot loaders over the emergency download port (EDL), the carrier images over the
 * Qualcomm download port (QDL), and saves or restores the modem's NV items around them.
 */
#include <string.h>
#include <sysmem/debug.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_memory.h"
#include "../sw_dload/sw_dload.h"
#include "updater.h"

int bbmcRequestSetFirmwarePreference(BbmcControl *control,
	const QmiSetFirmwarePreference *preference);
int bbmcRequestStoredImages(BbmcControl *control);
int bbmcRequestFirmwareImage(BbmcControl *control, const QmiFirmwareImage *image);
int bbmcPollUsbSerialHost(SceUInt32 *events);
void swdlEdlComPowerTimeoutHandler(void *arg);
void swdlEdlReplyTimeoutHandler(void *arg);
void swdlEdlPowerOffTimeoutHandler(void *arg);

#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, dbg__udSeqEdlPrepare_L943, 943, "_udSeqEdlPrepare");
SCE_DBGINFO_DEFINE(static, dbg__udSeqEdlPrepare_L1055, 1055, "_udSeqEdlPrepare");
SCE_DBGINFO_DEFINE(static, dbg__udSeqEdlPrepare_L1108, 1108, "_udSeqEdlPrepare");
SCE_DBGINFO_DEFINE(static, dbg__udSeqEdlFinalize_L1481, 1481, "_udSeqEdlFinalize");
SCE_DBGINFO_DEFINE(static, dbg__udSeqEdlFinalize_L1552, 1552, "_udSeqEdlFinalize");
SCE_DBGINFO_DEFINE(static, dbg__udSeqQdlPrepare_L1842, 1842, "_udSeqQdlPrepare");
SCE_DBGINFO_DEFINE(static, dbg__udSeqQdlFinalize_L2182, 2182, "_udSeqQdlFinalize");
SCE_DBGINFO_DEFINE(static, dbg__udSeqQdlFinalize_L2252, 2252, "_udSeqQdlFinalize");
SCE_DBGINFO_DEFINE(static, dbg__udSeqRestore_L2884, 2884, "_udSeqRestore");
#endif

/* name unknown: the sequence's states */
enum {
	UD_SEQ_IDLE = 0,
	UD_SEQ_CHECK_IMAGE = 1,
	UD_SEQ_EDL_PREPARE = 2,
	UD_SEQ_EDL_DOWNLOAD = 3,
	UD_SEQ_EDL_FINALIZE = 4,
	UD_SEQ_QDL_PREPARE = 5,
	UD_SEQ_QDL_DOWNLOAD = 6,
	UD_SEQ_QDL_FINALIZE = 7,
	UD_SEQ_BACKUP = 8,
	UD_SEQ_RESTORE = 9,
	UD_SEQ_FINISHED = 10,
};

/* name unknown: where each phase of the update ends, in percent of the whole */
typedef struct UdSeqProgress {
	SceUInt32 phase;
	SceUInt32 percent;
} UdSeqProgress;

static const UdSeqProgress udSeqProgressTable[7] = {
	{ 0, 0 }, { 1, 20 }, { 2, 40 }, { 3, 60 }, { 4, 75 }, { 5, 95 }, { 6, 100 },
};

static SceUInt32 udSeqState;
static SceUID udSeqTimerId;
static SceUInt32 udSeqSubState;
static SceUInt32 udSeqProgressBase;
static SceUInt32 udSeqFirstPass;
static SceUInt32 udSeqRestoreBackup;
static SceUInt32 udSeqDeleteSizeSet;
static SceUInt32 udSeqComPowerRetry;
static SceUInt32 udSeqProgress;
static SceUInt32 udSeqPreviousState;
static SceUInt32 udSeqRestoreVersion;
static SceUInt32 udSeqPreviousSubState;

/* name unknown: close the two images of a carrier, stopping at the first error */
__attribute__((noinline)) int udSeqCloseCarrierImages(UdCarrierImages *images)
{
	SceUID fd;
	int result;

	if (images == NULL)
		return (int)0x80330002;
	result = fd = (SceUID)images->firm[0];
	if (fd != 0) {
		result = udIoClose(fd);
		if (result < 0)
			return result;
	}
	fd = (SceUID)images->firm[1];
	if (fd != 0)
		return udIoClose(fd);
	return result;
}

/* name unknown: report the progress of a phase, scaled into its share of the whole */
__attribute__((noinline)) SceUInt32 udSeqNotifyProgress(BbmcControl *control, SceUInt32 percent, SceUInt32 base,
	SceUInt32 phase)
{
	SceUInt32 end = udSeqProgressTable[phase].percent;
	SceUInt32 progress;

	if (end < base)
		return 0;
	control->pendingEvents[8] &= 0xFFFFFF;
	progress = base + percent * (end - base) / 100;
	bbmcNotifyFeatureEvent(control, 7, (progress << 24) | 0x100);
	return progress;
}

/* name unknown: hand the boot loader images and their sizes to the EDL download */
__attribute__((noinline)) int udSeqRequestUpdate(BbmcControl *control, SceUInt32 partitionTableForced)
{
	SwdlUpdateRequest request = { 0 };
	UdFirmImages images = { 0 };
	SceOff size;
	int result;

	result = udGetFirmImages_QMDM6200(&images);
	if (result < 0)
		return result;
	request.unlock = 0;
	request.partitionTableForced = partitionTableForced;
	request.fd = (SceUID)images.enprgHex;
	request.partitionTableFd = (SceUID)images.partition;
	request.imageFds[0] = (SceUID)images.dbl;
	request.imageFds[1] = (SceUID)images.osbl;
	request.imageFds[2] = (SceUID)images.safe;
	if (images.enprgHex != NULL) {
		size = udIoGetSize((SceUID)images.enprgHex);
		if (size < 0)
			return size;
		request.hexSize = size;
	}
	if (images.partition != NULL) {
		size = udIoGetSize((SceUID)images.partition);
		if (size < 0)
			return size;
		request.partitionTableSize = size;
	}
	if (images.dbl != NULL) {
		size = udIoGetSize((SceUID)images.dbl);
		if (size < 0)
			return size;
		request.imageSizes[0] = size;
	}
	if (images.osbl != NULL) {
		size = udIoGetSize((SceUID)images.osbl);
		if (size < 0)
			return size;
		request.imageSizes[1] = size;
	}
	if (images.safe != NULL) {
		size = udIoGetSize((SceUID)images.safe);
		if (size < 0)
			return size;
		request.imageSizes[2] = size;
	}
	control->updateFiles[0] = (SceUID)images.enprgHex;
	control->updateFiles[1] = (SceUID)images.partition;
	control->updateFiles[2] = (SceUID)images.dbl;
	control->updateFiles[3] = (SceUID)images.osbl;
	control->updateFiles[4] = (SceUID)images.safe;
	return swdlRequestUpdate(&request);
}

/* name unknown: close the boot loader images, stopping at the first error */
static inline int udSeqCloseUpdateFiles(BbmcControl *control)
{
	SceUID hex = control->updateFiles[0];
	SceUID partition = control->updateFiles[1];
	SceUID dbl = control->updateFiles[2];
	SceUID osbl = control->updateFiles[3];
	SceUID safe = control->updateFiles[4];
	int result = 0;

	if (hex != 0 && (result = udIoClose(hex)) < 0)
		return result;
	if (partition != 0 && (result = udIoClose(partition)) < 0)
		return result;
	if (dbl != 0 && (result = udIoClose(dbl)) < 0)
		return result;
	if (osbl != 0 && (result = udIoClose(osbl)) < 0)
		return result;
	if (safe != 0 && (result = udIoClose(safe)) < 0)
		return result;
	return result;
}

/* name unknown: the modem's images are already the package's: report 100% and finish */
static inline int udSeqComplete(BbmcControl *control)
{
	control->pendingEvents[8] &= 0xFFFFFF;
	bbmcNotifyFeatureEvent(control, 7, 0x64000100);
	udSeqState = UD_SEQ_FINISHED;
	return 0;
}

/* name unknown: the modem's link came back after the transport reset (59, 21, 23) */
static inline int udSeqIsLinkMessage(SceUInt32 command)
{
	return command == 59 || command == 21 || command == 23;
}

/* the modem's power off acknowledgements (136, 138) */
static inline int udSeqIsPowerOffMessage(SceUInt32 command)
{
	return command == 136 || command == 138;
}

/* name unknown: read the modem's images, list what to delete and decide what to write */
static int _udSeqCheckImage(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 command = message->command;
	UdDeleteEntry entry;
	int result;

	if (command == 123) {
		control->updateCancelled = 1;
		udSeqState = UD_SEQ_FINISHED;
		return 0;
	}
	switch (udSeqSubState) {
	case 0:
		if (command != 178)
			return 0;
		result = bbmcRequestStoredImages(control);
		if (result >= 0)
			udSeqSubState = 1;
		return result;
	case 1:
		if (command != 156)
			return 0;
		control->storedImagesState = 0;
		if (message->body.firmwareMessage.storedImagesResult < 0) {
			/* the modem cannot list its images: write them all */
			result = udSetUpdateFirmImages_QMDM6200();
			if (result < 0 && result != (int)0x8033070E)
				return result;
			result = udSetUpdateCarrierImages_QMDM6200();
			if (result < 0)
				return result;
			result = bbmcSendSimpleMessage(control, 178);
			if (result >= 0)
				udSeqSubState = 4;
			return result;
		}
		control->updateStoredImages = &control->storedImages;
		result = udCreateDeleteList((UdModemFirmList *)&control->storedImages);
		if (result < 0)
			return result;
		result = bbmcSendSimpleMessage(control, 178);
		if (result >= 0)
			udSeqSubState = 2;
		return result;
	case 2:
		if (command != 178)
			return 0;
		result = udCheckUpdateNoNeed((const SceUInt8 *)&control->replyVersionValid);
		if (result < 0)
			return result;
		result = bbmcSendSimpleMessage(control, 178);
		if (result >= 0)
			udSeqSubState = 3;
		return result;
	case 3:
		if (command != 178 && command != 160)
			return 0;
		if (command == 160) {
			control->firmwareImageState = 0;
			result = message->body.firmwareMessage.firmwareImageResult;
			if (result < 0)
				return result;
		}
		/* one delete entry per pass, until the list is empty */
		memset(&entry, 0, sizeof(entry));
		result = udGetDeleteEntry_QMDM6200(&entry);
		if (result < 0)
			return result;
		if (entry.nameLength != 0)
			return bbmcRequestFirmwareImage(control, (const QmiFirmwareImage *)&entry);
		result = bbmcSendSimpleMessage(control, 178);
		if (result >= 0)
			udSeqSubState = 4;
		return result;
	case 4:
		if (command != 178)
			return 0;
		if (udIsFirmUpdateNeeded_QMDM6200() == 1) {
			result = bbmcRequestOperationMode(control, udSeqFirstPass == 1 ? 2 : 6);
			if (result >= 0) {
				udSeqState = UD_SEQ_EDL_PREPARE;
				udSeqSubState = 0;
			}
			return result;
		}
		if (udIsCarrierUpdatePending_QMDM6200() == 1) {
			result = bbmcSendSimpleMessage(control, 142);
			if (result >= 0) {
				udSeqState = UD_SEQ_QDL_PREPARE;
				udSeqSubState = 0;
			}
			return result;
		}
		if (udIsImageVersionChanged_QMDM6200(control->decodedReply) != 1)
			return udSeqComplete(control);
		result = bbmcSendSimpleMessage(control, 178);
		if (result >= 0) {
			udSeqState = UD_SEQ_RESTORE;
			udSeqSubState = 0;
		}
		return result;
	default:
		return (int)0x80330001;
	}
}

/* power the modem up in its download mode, retrying the power request five times */
static int _udSeqEdlPrepare(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 command = message->command;
	int result;

	if (command == 123)
		control->updateCancelled = 1;
	if (control->updateCancelled == 1) {
		switch (udSeqSubState) {
		case 0:
			if (command == 117) {
				result = message->body.commandResult;
				if (result < 0)
					return result;
				udSeqComPowerRetry = 0;
			} else {
				if (command != 178)
					return 0;
				if (udSeqComPowerRetry > 4) {
					result = bbmcReleasePowerReference(control);
					if (result >= 0)
						udSeqSubState = 1;
					return result;
				}
			}
			result = bbmcTimerSetHandler(1000000, swdlEdlComPowerTimeoutHandler, control);
			if (result < 0) {
#if FW_DEBUG
				sceKernelAssert(0, &dbg__udSeqEdlPrepare_L943,
						__builtin_return_address(0));
#endif
				return result;
			}
			udSeqTimerId = result;
			udSeqComPowerRetry++;
			return result;
		case 1:
			if (bbmcIsTransportHandshakeComplete(control))
				return (int)0x80330710;
			if (!bbmcIsTransportReady(control))
				return 0;
			result = bbmcRequestOperationMode(control, 1);
			if (result >= 0)
				udSeqSubState = 3;
			return result;
		case 2:
			if (command == 187) {
				if (message->body.commandResult != 59)
					return 0;
				udSeqTimerId = 0;
				return (int)0x80330006;
			}
			if (!udSeqIsLinkMessage(command))
				return 0;
			if (control->currentState <= 1)
				return 0;
			bbmcTimerCancelHandler(udSeqTimerId);
			udSeqTimerId = 0;
			result = bbmcReleasePowerReference(control);
			if (result >= 0)
				udSeqSubState = 1;
			return result;
		case 3:
			if (command == 117)
				udSeqState = UD_SEQ_FINISHED;
			return 0;
		default:
			return 0;
		}
	}
	switch (udSeqSubState) {
	case 0:
		if (command == 117) {
			result = message->body.commandResult;
			if (result < 0)
				return result;
			udSeqComPowerRetry = 0;
		} else {
			if (command != 178)
				return 0;
			if (udSeqComPowerRetry > 4) {
				result = bbmcReleasePowerReference(control);
				if (result >= 0)
					udSeqSubState = 1;
				return result;
			}
		}
		result = bbmcTimerSetHandler(1000000, swdlEdlComPowerTimeoutHandler, control);
		if (result < 0) {
#if FW_DEBUG
			sceKernelAssert(0, &dbg__udSeqEdlPrepare_L1055, __builtin_return_address(0));
#endif
			return result;
		}
		udSeqTimerId = result;
		udSeqComPowerRetry++;
		return result;
	case 1:
		if (bbmcIsTransportHandshakeComplete(control))
			return (int)0x80330710;
		if (!bbmcIsTransportReady(control))
			return 0;
		result = bbmcAcquirePowerReference(control);
		if (result < 0)
			return result;
		result = bbmcTimerSetHandler(60000000, swdlEdlReplyTimeoutHandler, control);
		if (result < 0) {
#if FW_DEBUG
			sceKernelAssert(0, &dbg__udSeqEdlPrepare_L1108, __builtin_return_address(0));
#endif
			return result;
		}
		udSeqTimerId = result;
		udSeqSubState = 2;
		return result;
	case 2:
		if (command == 187) {
			if (message->body.commandResult != 59)
				return 0;
			udSeqTimerId = 0;
			return (int)0x80330006;
		}
		if (!udSeqIsLinkMessage(command))
			return 0;
		if (control->currentState <= 1)
			return 0;
		bbmcTimerCancelHandler(udSeqTimerId);
		udSeqTimerId = 0;
		if ((SceInt16)control->secondBootReply < 0)
			return (int)0x80330006;
		result = bbmcRegisterDownloadNotification(control);
		if (result < 0)
			return result;
		bbmcAcquireGpio(NULL);
		swdlWaitAttach(1);
		control->updateCompletedBytes = 0;
		control->updateTotalBytes = udGetWriteSize();
		if (control->updateTotalBytes == 0)
			return (int)0x80330002;
		udSeqSubState = 0;
		udSeqState = UD_SEQ_EDL_DOWNLOAD;
		return result;
	default:
		return (int)0x80330001;
	}
}

/* name unknown: write the boot loaders over the EDL port */
static int _udSeqEdlDownload(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 command = message->command;
	SceUInt32 events;
	SceUInt32 percent;
	int result;

	if (command == 123)
		control->updateCancelled = 1;
	if (control->updateCancelled == 1) {
		switch (udSeqSubState) {
		case 0:
			if (command == 135) {
				bbmcReleaseGpio(NULL);
				swdlWaitDetach();
				udSeqSubState = 3;
				return 0;
			}
			if (command != 137)
				return 0;
			bbmcReleaseGpio(NULL);
			result = bbmcReleasePowerReference(control);
			if (result >= 0) {
				udSeqState = UD_SEQ_EDL_FINALIZE;
				udSeqSubState = 2;
			}
			return result;
		case 1:
			swdlCancel();
			udSeqSubState = 2;
			return 0;
		case 2:
			if (command != 131 && command != 128 && command != 134)
				return 0;
			result = udSeqCloseUpdateFiles(control);
			if (result < 0)
				return result;
			if (message->command == 134) {
				bbmcEnterCS(&control->mutex);
				control->lastError = message->notificationValue1;
				bbmcLeaveCS(&control->mutex);
			}
			bbmcReleaseGpio(NULL);
			swdlWaitDetach();
			udSeqState = UD_SEQ_EDL_FINALIZE;
			udSeqSubState = 0;
			return result;
		case 3:
			if (!udSeqIsPowerOffMessage(command))
				return 0;
			result = bbmcReleasePowerReference(control);
			if (result >= 0) {
				udSeqState = UD_SEQ_EDL_FINALIZE;
				udSeqSubState = 2;
			}
			return result;
		default:
			return 0;
		}
	}
	switch (udSeqSubState) {
	case 0:
		if (command == 135) {
			result = bbmcPollUsbSerialHost(&events);
			if (result < 0)
				return result;
			if ((events & 0x10) == 0)
				return (int)0x80330711;
			result = udSeqRequestUpdate(control, udSeqFirstPass != 1);
			if (result >= 0)
				udSeqSubState = 1;
			return result;
		}
		if (command != 137)
			return 0;
		bbmcReleaseGpio(NULL);
		return (int)0x80330006;
	case 1:
		if (command == 128 || command == 131) {
			result = udSeqCloseUpdateFiles(control);
			if (result < 0)
				return result;
			udSeqProgressBase = udSeqProgress;
			if (udSeqFirstPass == 1) {
				/* 128: the modem asks for the whole package after its boot loaders */
				if (message->command == 128)
					udRequestUpdateAll_QMDM6200(1);
				else
					udSeqFirstPass = 0;
			}
			bbmcReleaseGpio(NULL);
			swdlWaitDetach();
			udSeqState = UD_SEQ_EDL_FINALIZE;
			udSeqSubState = 0;
			return result;
		}
		if (command == 132) {
			udRequestUpdateAll_QMDM6200(0);
			swdlCancel();
			return 0;
		}
		if (command == 133) {
			control->updateCompletedBytes += message->notificationValue2;
			if (udSeqFirstPass == 1) {
				percent = 100 * control->updateCompletedBytes / control->updateTotalBytes;
				udSeqProgress = percent <= 100 ?
					udSeqNotifyProgress(control, percent, udSeqProgressBase, 1) : 0;
			} else {
				percent = 100 * control->updateCompletedBytes / control->updateTotalBytes;
				udSeqProgress = percent <= 100 ?
					udSeqNotifyProgress(control, percent, udSeqProgressBase, 3) : 0;
			}
			return 0;
		}
		if (command == 134) {
			result = udSeqCloseUpdateFiles(control);
			if (result < 0)
				return result;
			bbmcReleaseGpio(NULL);
			swdlWaitDetach();
			bbmcEnterCS(&control->mutex);
			control->lastError = message->notificationValue1;
			bbmcLeaveCS(&control->mutex);
			udSeqState = UD_SEQ_EDL_FINALIZE;
			udSeqSubState = 0;
			return result;
		}
		if (bbmcIsTransportReady(control)) {
			swdlCancel();
			control->lastError = (int)0x80330101;
		}
		return 0;
	default:
		return (int)0x80330001;
	}
}

/* power the modem off after the EDL download and back up in its normal mode */
static int _udSeqEdlFinalize(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 command = message->command;
	int result;

	if (command == 123)
		control->updateCancelled = 1;
	switch (udSeqSubState) {
	case 0:
		if (!udSeqIsPowerOffMessage(command))
			return 0;
		if (control->lastError < 0) {
			result = bbmcReleasePowerReference(control);
			if (result >= 0)
				udSeqSubState = 2;
			return result;
		}
		result = bbmcTimerSetHandler(60000000, swdlEdlPowerOffTimeoutHandler, control);
		if (result < 0) {
#if FW_DEBUG
			sceKernelAssert(0, &dbg__udSeqEdlFinalize_L1481, __builtin_return_address(0));
#endif
			return result;
		}
		udSeqTimerId = result;
		result = bbmcSendSimpleMessage(control, 178);
		if (result >= 0) {
			udSeqSubState = 2;
			return result;
		}
		bbmcTimerCancelHandler(udSeqTimerId);
		udSeqTimerId = 0;
		return result;
	case 1:
		if (command != 117)
			return 0;
		result = message->body.commandResult;
		if (result < 0)
			return result;
		result = control->lastError;
		if (result < 0)
			return result;
		if (control->updateCancelled == 1) {
			udSeqState = UD_SEQ_FINISHED;
			return 0;
		}
		result = bbmcAcquirePowerReference(control);
		if (result < 0)
			return result;
		result = bbmcTimerSetHandler(60000000, swdlEdlReplyTimeoutHandler, control);
		if (result < 0) {
#if FW_DEBUG
			sceKernelAssert(0, &dbg__udSeqEdlFinalize_L1552, __builtin_return_address(0));
#endif
			return result;
		}
		udSeqTimerId = result;
		udSeqSubState = 3;
		return result;
	case 2:
		if (bbmcIsTransportHandshakeComplete(control)) {
			udSeqTimerId = 0;
			return (int)0x80330710;
		}
		if (!bbmcIsTransportReady(control))
			return 0;
		bbmcTimerCancelHandler(udSeqTimerId);
		udSeqTimerId = 0;
		result = bbmcRequestOperationMode(control, 1);
		if (result >= 0)
			udSeqSubState = 1;
		return result;
	case 3:
		if (control->updateCancelled == 1) {
			bbmcTimerCancelHandler(udSeqTimerId);
			udSeqTimerId = 0;
			udSeqState = UD_SEQ_FINISHED;
			return 0;
		}
		if (command == 187) {
			if (message->body.commandResult != 59)
				return 0;
			udSeqTimerId = 0;
			return (int)0x80330006;
		}
		if (!udSeqIsLinkMessage(command))
			return 0;
		if (control->currentState <= 1)
			return 0;
		bbmcTimerCancelHandler(udSeqTimerId);
		udSeqTimerId = 0;
		if ((SceInt16)control->secondBootReply < 0)
			return (int)0x80330006;
		if (udIsUpdateAllRequested_QMDM6200() == 1) {
			result = bbmcSendSimpleMessage(control, 178);
			if (result >= 0) {
				udSeqSubState = 0;
				udSeqState = UD_SEQ_BACKUP;
			}
		} else if (udIsBackupRestoreNeeded_QMDM6200() == 1) {
			result = bbmcSendSimpleMessage(control, 178);
			if (result >= 0) {
				udSeqSubState = 0;
				udSeqState = UD_SEQ_RESTORE;
			}
		} else {
			result = bbmcSendSimpleMessage(control, 142);
			if (result >= 0) {
				udSeqSubState = 0;
				udSeqState = UD_SEQ_QDL_PREPARE;
			}
		}
		return result;
	default:
		return (int)0x80330001;
	}
}

/* select the next carrier's images on the modem and power it up in its QDL mode */
static int _udSeqQdlPrepare(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 command = message->command;
	QmiSetFirmwarePreference preference;
	UdCarrierImages images;
	SceUInt32 imageCount;
	SceUInt16 reply;
	int result;

	if (command == 123)
		control->updateCancelled = 1;
	switch (udSeqSubState) {
	case 0:
		if (control->updateCancelled == 1) {
			udSeqState = UD_SEQ_FINISHED;
			return 0;
		}
		if (command != 142)
			return 0;
		if (udIsCarrierUpdatePending_QMDM6200() == 0) {
			if (udIsImageVersionChanged_QMDM6200(control->decodedReply) != 1)
				return udSeqComplete(control);
			result = bbmcSendSimpleMessage(control, 178);
			if (result >= 0) {
				udSeqSubState = 0;
				udSeqState = UD_SEQ_RESTORE;
			}
			return result;
		}
		result = udGetCarrierImages_QMDM6200(&images);
		if (result < 0)
			return result;
		if (images.id == 0 || images.firm[0] == NULL || images.firm[1] == NULL)
			return (int)0x80330002;
		/* the images' trailers are the preference's image records */
		result = udReadFileInformation((SceUID)images.firm[0],
			(UdFileInformation *)&preference.preference.images[0]);
		if (result < 0 || (result = udReadFileInformation((SceUID)images.firm[1],
			(UdFileInformation *)&preference.preference.images[1])) < 0) {
			udSeqCloseCarrierImages(&images);
			return result;
		}
		result = udSeqCloseCarrierImages(&images);
		if (result < 0)
			return result;
		control->updateEntryId = images.id;
		preference.storageIndex = images.id;
		preference.preference.count = 2;
		preference.preference.images[0].imageType = 0;
		preference.preference.images[1].imageType = 1;
		preference.hasStorageIndex = 1;
		result = bbmcRequestSetFirmwarePreference(control, &preference);
		if (result >= 0)
			udSeqSubState = 2;
		return result;
	case 2:
		if (command != 162)
			return 0;
		bbmcEnterCS(&control->mutex);
		result = message->body.firmwareMessage.setFirmwarePreferenceResult;
		imageCount = message->body.firmwareMessage.imageCount;
		control->setFirmwarePreferenceState = 0;
		bbmcLeaveCS(&control->mutex);
		if (result < 0)
			return result;
		if (imageCount != 0) {
			/* the modem wants the images: download them */
			result = bbmcRequestOperationMode(control, 5);
			if (result >= 0)
				udSeqSubState = 3;
			return result;
		}
		/* the modem has them: the carrier is done */
		result = udSetCarrierWritten_QMDM6200(control->updateEntryId);
		if (result < 0)
			return result;
		result = bbmcSendSimpleMessage(control, 142);
		if (result >= 0)
			udSeqSubState = 0;
		return result;
	case 3:
		if (command != 117)
			return 0;
		result = message->body.commandResult;
		if (result < 0)
			return result;
		result = bbmcReleasePowerReference(control);
		if (result >= 0)
			udSeqSubState = 4;
		return result;
	case 4:
		if (bbmcIsTransportHandshakeComplete(control))
			return (int)0x80330710;
		if (!bbmcIsTransportReady(control))
			return 0;
		result = bbmcAcquirePowerReference(control);
		if (result < 0)
			return result;
		result = bbmcTimerSetHandler(60000000, swdlEdlReplyTimeoutHandler, control);
		if (result < 0) {
#if FW_DEBUG
			sceKernelAssert(0, &dbg__udSeqQdlPrepare_L1842, __builtin_return_address(0));
#endif
			return result;
		}
		udSeqTimerId = result;
		udSeqSubState = 5;
		return result;
	case 5:
		if (control->updateCancelled == 1) {
			udSeqSubState = 6;
			return 0;
		}
		if (command == 187) {
			if (message->body.commandResult != 59)
				return 0;
			udSeqTimerId = 0;
			return (int)0x80330006;
		}
		if (!udSeqIsLinkMessage(command))
			return 0;
		if (control->currentState <= 1)
			return 0;
		bbmcTimerCancelHandler(udSeqTimerId);
		reply = control->secondBootReply;
		udSeqTimerId = 0;
		if ((SceInt16)reply < 0 || control->currentState != 8 || reply != 6)
			return (int)0x80330006;
		if (udSeqDeleteSizeSet == 0) {
			control->updateCompletedBytes = 0;
			udSeqDeleteSizeSet = 1;
			control->updateTotalBytes = udGetDeleteSize();
			if (control->updateTotalBytes == 0)
				return (int)0x80330002;
		}
		result = bbmcRegisterDownloadNotification(control);
		if (result < 0)
			return result;
		bbmcAcquireGpio(NULL);
		swdlWaitAttach(0);
		udSeqState = UD_SEQ_QDL_DOWNLOAD;
		udSeqSubState = 0;
		return result;
	case 6:
		if (command == 187) {
			if (message->body.commandResult == 59) {
				udSeqTimerId = 0;
				return (int)0x80330006;
			}
		} else if (udSeqIsLinkMessage(command) && control->currentState > 1) {
			bbmcTimerCancelHandler(udSeqTimerId);
			udSeqTimerId = 0;
		}
		if (!bbmcIsTransportReady(control))
			return 0;
		result = bbmcRequestOperationMode(control, 1);
		if (result >= 0)
			udSeqSubState = 7;
		return result;
	case 7:
		if (command == 117)
			udSeqState = UD_SEQ_FINISHED;
		return 0;
	default:
		return (int)0x80330001;
	}
}

/* name unknown: write a carrier's two images over the QDL port */
static int _udSeqQdlDownload(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 command = message->command;
	SwdlDownloadRequest request;
	UdCarrierImages images;
	SceUID first, second;
	SceUInt32 events;
	SceUInt32 percent;
	SceOff size;
	int result;

	if (command == 123)
		control->updateCancelled = 1;
	if (control->updateCancelled == 1) {
		if (udSeqSubState == 1) {
			swdlCancel();
			udSeqSubState = 2;
			return 0;
		}
		if (udSeqSubState == 0) {
			if (command == 135) {
				bbmcReleaseGpio(NULL);
				swdlWaitDetach();
				udSeqState = UD_SEQ_QDL_FINALIZE;
				udSeqSubState = 0;
			} else if (command == 137) {
				bbmcReleaseGpio(NULL);
				udSeqState = UD_SEQ_QDL_FINALIZE;
				udSeqSubState = 1;
			}
			return 0;
		}
		if (udSeqSubState != 2)
			return 0;
		if (command != 129 && command != 131 && command != 134)
			return 0;
		first = control->updateFilePair[0];
		second = control->updateFilePair[1];
		if (first != 0 && (result = udIoClose(first)) < 0)
			return result;
		if (second != 0 && (result = udIoClose(second)) < 0)
			return result;
		if (message->command == 134) {
			bbmcEnterCS(&control->mutex);
			control->lastError = message->notificationValue1;
			bbmcLeaveCS(&control->mutex);
		}
		bbmcReleaseGpio(NULL);
		swdlWaitDetach();
		udSeqState = UD_SEQ_QDL_FINALIZE;
		udSeqSubState = 0;
		return result;
	}
	if (udSeqSubState == 0) {
		if (command == 135) {
			result = bbmcPollUsbSerialHost(&events);
			if (result < 0)
				return result;
			if ((events & 0x40) == 0)
				return (int)0x80330711;
			memset(&request, 0, sizeof(request));
			result = udGetCarrierImages_QMDM6200(&images);
			if (result < 0)
				return result;
			if (images.id == 0 || images.firm[0] == NULL || images.firm[1] == NULL)
				return (int)0x80330002;
			size = udIoGetSize((SceUID)images.firm[0]);
			if (size < 0)
				return size;
			request.images[request.count].size = size;
			request.images[request.count].type = 5;
			request.images[request.count].fd = (SceUID)images.firm[0];
			request.count++;
			size = udIoGetSize((SceUID)images.firm[1]);
			control->updateFilePair[1] = (SceUID)images.firm[1];
			request.images[request.count].type = 13;
			control->updateFilePair[0] = (SceUID)images.firm[0];
			request.images[request.count].size = size;
			request.images[request.count].fd = (SceUID)images.firm[1];
			control->updateEntryId = images.id;
			request.count++;
			result = swdlRequestDownload(&request);
			if (result >= 0)
				udSeqSubState = 1;
			return result;
		}
		if (command != 137)
			return 0;
		bbmcReleaseGpio(NULL);
		return (int)0x80330711;
	}
	if (udSeqSubState != 1)
		return (int)0x80330001;
	if (command == 129) {
		first = control->updateFilePair[0];
		second = control->updateFilePair[1];
		if (first != 0 && (result = udIoClose(first)) < 0)
			return result;
		if (second != 0 && (result = udIoClose(second)) < 0)
			return result;
		udSeqProgressBase = udSeqProgress;
		result = udSetCarrierWritten_QMDM6200(control->updateEntryId);
		if (result < 0)
			return result;
		bbmcReleaseGpio(NULL);
		swdlWaitDetach();
		udSeqState = UD_SEQ_QDL_FINALIZE;
		udSeqSubState = 0;
		return result;
	}
	if (command == 133) {
		control->updateCompletedBytes += message->notificationValue2;
		percent = 100 * control->updateCompletedBytes / control->updateTotalBytes;
		udSeqProgress = percent <= 100 ?
			udSeqNotifyProgress(control, percent, udSeqProgressBase, 5) : 0;
		return 0;
	}
	if (command != 134)
		return 0;
	images.firm[0] = (UdFirmInfo *)control->updateFilePair[0];
	images.firm[1] = (UdFirmInfo *)control->updateFilePair[1];
	result = udSeqCloseCarrierImages(&images);
	if (result < 0)
		return result;
	bbmcReleaseGpio(NULL);
	swdlWaitDetach();
	bbmcEnterCS(&control->mutex);
	control->lastError = message->notificationValue1;
	bbmcLeaveCS(&control->mutex);
	udSeqState = UD_SEQ_QDL_FINALIZE;
	udSeqSubState = 0;
	return result;
}

/* power the modem off after a carrier's download and back up in its normal mode */
static int _udSeqQdlFinalize(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 command = message->command;
	int result;

	if (command == 123)
		control->updateCancelled = 1;
	switch (udSeqSubState) {
	case 0:
		if (!udSeqIsPowerOffMessage(command))
			return 0;
		result = bbmcTimerSetHandler(60000000, swdlEdlPowerOffTimeoutHandler, control);
		if (result < 0) {
#if FW_DEBUG
			sceKernelAssert(0, &dbg__udSeqQdlFinalize_L2182, __builtin_return_address(0));
#endif
			return result;
		}
		udSeqTimerId = result;
		result = bbmcSendSimpleMessage(control, 178);
		if (result >= 0) {
			udSeqSubState = 1;
			return result;
		}
		bbmcTimerCancelHandler(udSeqTimerId);
		udSeqTimerId = 0;
		return result;
	case 1:
		if (bbmcIsTransportHandshakeComplete(control))
			return (int)0x80330710;
		if (!bbmcIsTransportReady(control))
			return 0;
		if (message->command != 187 || message->body.commandResult != 9) {
			bbmcTimerCancelHandler(udSeqTimerId);
			udSeqTimerId = 0;
		}
		result = bbmcRequestOperationMode(control, 1);
		if (result >= 0)
			udSeqSubState = 2;
		return result;
	case 2:
		if (command != 117)
			return 0;
		result = message->body.commandResult;
		if (result < 0)
			return result;
		result = control->lastError;
		if (result < 0)
			return result;
		if (control->updateCancelled == 1) {
			udSeqState = UD_SEQ_FINISHED;
			return 0;
		}
		result = bbmcAcquirePowerReference(control);
		if (result < 0)
			return result;
		result = bbmcTimerSetHandler(60000000, swdlEdlReplyTimeoutHandler, control);
		if (result < 0) {
#if FW_DEBUG
			sceKernelAssert(0, &dbg__udSeqQdlFinalize_L2252, __builtin_return_address(0));
#endif
			return result;
		}
		udSeqTimerId = result;
		udSeqSubState = 3;
		return result;
	case 3:
		if (control->updateCancelled == 1) {
			bbmcTimerCancelHandler(udSeqTimerId);
			udSeqTimerId = 0;
			udSeqState = UD_SEQ_FINISHED;
			return 0;
		}
		if (command == 187) {
			if (message->body.commandResult != 59)
				return 0;
			udSeqTimerId = 0;
			return (int)0x80330006;
		}
		if (!udSeqIsLinkMessage(command))
			return 0;
		if (control->currentState <= 1)
			return 0;
		bbmcTimerCancelHandler(udSeqTimerId);
		udSeqTimerId = 0;
		if ((SceInt16)control->secondBootReply < 0)
			return (int)0x80330006;
		/* the next carrier */
		result = bbmcSendSimpleMessage(control, 142);
		if (result >= 0) {
			udSeqSubState = 0;
			udSeqState = UD_SEQ_QDL_PREPARE;
		}
		return result;
	default:
		return (int)0x80330001;
	}
}

/* name unknown: save the modem's NV items before the whole package is written */
static int _udSeqBackup(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 command = message->command;
	SwdlBackupRequest request;
	UdFirmInfo *backup;
	SceUInt32 events;
	int result;

	if (command == 123)
		control->updateCancelled = 1;
	if (control->updateCancelled == 1) {
		switch (udSeqSubState) {
		case 0:
			udSeqState = UD_SEQ_FINISHED;
			return 0;
		case 1:
			if (command == 135) {
				bbmcReleaseGpio(NULL);
				swdlWaitDetach();
				udSeqSubState = 3;
			} else if (command == 137) {
				bbmcReleaseGpio(NULL);
				udSeqState = UD_SEQ_FINISHED;
			}
			return 0;
		case 2:
			swdlCancel();
			udSeqSubState = 4;
			return 0;
		case 3:
			if (!udSeqIsPowerOffMessage(command))
				return 0;
			result = control->lastError;
			if (result >= 0) {
				udSeqState = UD_SEQ_FINISHED;
				return 0;
			}
			return result;
		case 4:
			if (command != 131 && command != 127 && command != 134)
				return 0;
			result = udIoClose(control->updateFile);
			if (result < 0)
				return result;
			control->updateFile = 0;
			bbmcReleaseGpio(NULL);
			swdlWaitDetach();
			udSeqSubState = 3;
			return result;
		default:
			return 0;
		}
	}
	switch (udSeqSubState) {
	case 0:
		if (command != 178)
			return 0;
		result = udCreateBackupImage_QMDM6200();
		if (result < 0)
			return result;
		result = bbmcRegisterDownloadNotification(control);
		if (result < 0)
			return result;
		bbmcAcquireGpio(NULL);
		swdlWaitAttach(0);
		udSeqSubState = 1;
		return result;
	case 1:
		if (command != 135) {
			if (command != 137)
				return 0;
			bbmcReleaseGpio(NULL);
			return (int)0x80330006;
		}
		result = bbmcPollUsbSerialHost(&events);
		if (result < 0)
			return result;
		if ((events & 1) == 0)
			return (int)0x80330711;
		request.reserved0 = 0;
		request.fd = 0;
		result = udGetBackupImage_QMDM6200(&backup);
		if (result < 0)
			return result;
		control->updateFile = (SceUID)backup;
		request.fd = (SceUID)backup;
		result = swdlRequestBackup(&request);
		if (result >= 0)
			udSeqSubState = 2;
		return result;
	case 2:
		switch (command) {
		case 127:
			result = udIoClose(control->updateFile);
			if (result < 0)
				return result;
			control->updateFile = 0;
			udSeqProgressBase = udSeqProgress;
			udSetBackupSaved_QMDM6200();
			bbmcReleaseGpio(NULL);
			swdlWaitDetach();
			udSeqSubState = 3;
			return result;
		case 133:
			udSeqProgress = message->notificationValue2 <= 100 ?
				udSeqNotifyProgress(control, message->notificationValue2,
						    udSeqProgressBase, 2) : 0;
			return 0;
		case 134:
			result = udIoClose(control->updateFile);
			if (result < 0)
				return result;
			control->updateFile = 0;
			bbmcReleaseGpio(NULL);
			swdlWaitDetach();
			bbmcEnterCS(&control->mutex);
			control->lastError = message->notificationValue1;
			bbmcLeaveCS(&control->mutex);
			udSeqSubState = 3;
			return result;
		default:
			return 0;
		}
	case 3:
		if (!udSeqIsPowerOffMessage(command))
			return 0;
		result = control->lastError;
		if (result < 0)
			return result;
		/* the modem is erased: list its images again */
		udReleaseDeleteList_QMDM6200();
		control->updateStoredImages = NULL;
		udResetDeleteList();
		result = bbmcSendSimpleMessage(control, 178);
		if (result >= 0) {
			udSeqState = UD_SEQ_CHECK_IMAGE;
			udSeqSubState = 2;
		}
		return result;
	default:
		return (int)0x80330001;
	}
}

/* write the saved NV items (and the package's image version) back to the modem */
static int _udSeqRestore(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 command = message->command;
	SwdlRestoreRequest request;
	UdFirmInfo *backup;
	SceUInt32 version;
	SceUInt32 events;
	int result;

	if (command == 123)
		control->updateCancelled = 1;
	if (control->updateCancelled == 1) {
		switch (udSeqSubState) {
		case 0:
			udSeqState = UD_SEQ_FINISHED;
			return 0;
		case 1:
			if (command == 135) {
				bbmcReleaseGpio(NULL);
				swdlWaitDetach();
				udSeqSubState = 3;
				return 0;
			}
			if (command != 137)
				return 0;
			bbmcReleaseGpio(NULL);
			result = bbmcReleasePowerReference(control);
			if (result >= 0)
				udSeqSubState = 5;
			return result;
		case 2:
			swdlCancel();
			udSeqSubState = 7;
			return 0;
		case 3:
			if (!udSeqIsPowerOffMessage(command))
				return 0;
			result = bbmcReleasePowerReference(control);
			if (result >= 0)
				udSeqSubState = 5;
			return result;
		case 4:
			if (command != 117)
				return 0;
			result = message->body.commandResult;
			if (result < 0)
				return result;
			result = bbmcReleasePowerReference(control);
			if (result >= 0)
				udSeqSubState = 5;
			return result;
		case 5:
			if (bbmcIsTransportHandshakeComplete(control))
				return (int)0x80330710;
			if (bbmcIsTransportReady(control))
				udSeqState = UD_SEQ_FINISHED;
			return 0;
		case 6:
			if (command == 187) {
				if (message->body.commandResult != 59)
					return 0;
				udSeqTimerId = 0;
				return (int)0x80330006;
			}
			if (!udSeqIsLinkMessage(command))
				return 0;
			if (control->currentState <= 1)
				return 0;
			bbmcTimerCancelHandler(udSeqTimerId);
			udSeqTimerId = 0;
			result = bbmcReleasePowerReference(control);
			if (result >= 0)
				udSeqSubState = 5;
			return result;
		case 7:
			if (command != 134 && command != 130 && command != 131)
				return 0;
			result = udIoClose(control->updateFile);
			if (result < 0)
				return result;
			control->updateFile = 0;
			bbmcReleaseGpio(NULL);
			swdlWaitDetach();
			udSeqSubState = 3;
			return result;
		default:
			return 0;
		}
	}
	switch (udSeqSubState) {
	case 0:
		if (command != 178)
			return 0;
		result = bbmcRegisterDownloadNotification(control);
		if (result < 0)
			return result;
		bbmcAcquireGpio(NULL);
		swdlWaitAttach(0);
		udSeqSubState = 1;
		return result;
	case 1:
		if (command != 135) {
			if (command != 137)
				return 0;
			bbmcReleaseGpio(NULL);
			return (int)0x80330006;
		}
		result = bbmcPollUsbSerialHost(&events);
		if (result < 0)
			return result;
		if ((events & 1) == 0)
			return (int)0x80330711;
		backup = NULL;
		version = 0;
		memset(&request, 0, sizeof(request));
		udSeqRestoreVersion = 0;
		udSeqRestoreBackup = 0;
		if (udIsBackupRestoreNeeded_QMDM6200() == 1) {
			result = udGetBackupImage_QMDM6200(&backup);
			if (result < 0)
				return result;
			udSeqRestoreBackup = 1;
		}
		if (udIsCarrierUpdatePending_QMDM6200() == 0) {
			/* no carrier written: the image version item goes with the restore */
			result = udGetImageVersion(&version);
			if (result < 0)
				return result;
			udSeqRestoreVersion = 1;
		}
		control->updateFile = (SceUID)backup;
		request.fd = (SceUID)backup;
		request.mode = udSeqRestoreVersion;
		request.extraItemValue = version;
		result = swdlRequestRestore(&request);
		if (result >= 0)
			udSeqSubState = 2;
		return result;
	case 2:
		if (command == 130) {
			if (control->updateFile != 0) {
				result = udIoClose(control->updateFile);
				if (result < 0)
					return result;
			} else {
				result = 0;
			}
			control->updateFile = 0;
			udSeqProgressBase = udSeqProgress;
			udSetBackupRestored_QMDM6200();
			bbmcReleaseGpio(NULL);
			swdlWaitDetach();
			udSeqSubState = 3;
			return result;
		}
		if (command == 133) {
			if (udSeqRestoreBackup == 1)
				udSeqProgress = message->notificationValue2 <= 100 ?
					udSeqNotifyProgress(control, message->notificationValue2,
							    udSeqProgressBase, 4) : 0;
			else
				udSeqProgress = message->notificationValue2 <= 100 ?
					udSeqNotifyProgress(control, message->notificationValue2,
							    udSeqProgressBase, 6) : 0;
			return 0;
		}
		if (command != 134)
			return 0;
		if (control->updateFile != 0) {
			result = udIoClose(control->updateFile);
			if (result < 0)
				return result;
		} else {
			result = 0;
		}
		control->updateFile = 0;
		bbmcReleaseGpio(NULL);
		swdlWaitDetach();
		bbmcEnterCS(&control->mutex);
		control->lastError = message->notificationValue1;
		bbmcLeaveCS(&control->mutex);
		udSeqSubState = 3;
		return result;
	case 3:
		if (!udSeqIsPowerOffMessage(command))
			return 0;
		if (udSeqRestoreVersion == 1 && udSeqRestoreBackup == 0)
			return udSeqComplete(control);
		result = bbmcRequestOperationMode(control, 1);
		if (result >= 0)
			udSeqSubState = 4;
		return result;
	case 4:
		if (command != 117)
			return 0;
		result = message->body.commandResult;
		if (result < 0)
			return result;
		result = bbmcReleasePowerReference(control);
		if (result >= 0)
			udSeqSubState = 5;
		return result;
	case 5:
		if (bbmcIsTransportHandshakeComplete(control))
			return (int)0x80330710;
		if (!bbmcIsTransportReady(control))
			return 0;
		result = control->lastError;
		if (result < 0)
			return result;
		result = bbmcAcquirePowerReference(control);
		if (result < 0)
			return result;
		result = bbmcTimerSetHandler(60000000, swdlEdlReplyTimeoutHandler, control);
		if (result < 0) {
#if FW_DEBUG
			sceKernelAssert(0, &dbg__udSeqRestore_L2884, __builtin_return_address(0));
#endif
			return result;
		}
		udSeqTimerId = result;
		udSeqSubState = 6;
		return result;
	case 6:
		if (command == 187) {
			if (message->body.commandResult != 59)
				return 0;
			udSeqTimerId = 0;
			return (int)0x80330006;
		}
		if (!udSeqIsLinkMessage(command))
			return 0;
		if (control->currentState <= 1)
			return 0;
		bbmcTimerCancelHandler(udSeqTimerId);
		udSeqTimerId = 0;
		if ((SceInt16)control->secondBootReply < 0)
			return (int)0x80330006;
		if (udSeqRestoreVersion == 1)
			return udSeqComplete(control);
		result = bbmcSendSimpleMessage(control, 142);
		if (result >= 0) {
			udSeqSubState = 0;
			udSeqState = UD_SEQ_QDL_PREPARE;
		}
		return result;
	default:
		return (int)0x80330001;
	}
}

/*
 * name unknown: the update handler's sequence (udSequences in sequence/sw_updater.c).
 * 1: the message was taken and the update goes on; 0x8033070D: a request is still
 * pending; otherwise the update finished with this result.
 */
int _udSeqUpdate_QMDM6200(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 command = message->command;
	int result = 0;

	udSeqPreviousState = udSeqState;
	udSeqPreviousSubState = udSeqSubState;
	if (!(command - 119 <= 24 || command - 175 <= 3 || command == 156 ||
	      command == 160 || command == 162 || command == 3 || command == 9 ||
	      command == 39 || command == 116 || command == 117 || command == 21 ||
	      command == 23 || command == 59 || command == 187)) {
		if (udSeqState != UD_SEQ_FINISHED)
			return 1;
		result = 0;
		goto finish;
	}

	switch (udSeqState) {
	case UD_SEQ_IDLE:
		if (command != 178) {
			result = 0;
			if (command == 123) {
				control->updateCancelled = 1;
				udSeqState = UD_SEQ_FINISHED;
			}
			break;
		}
		control->lastError = 0;
		udSeqProgress = 0;
		udSeqDeleteSizeSet = 0;
		udSeqProgressBase = 0;
		udSeqFirstPass = 1;
		result = bbmcSendSimpleMessage(control, 178);
		if (result >= 0) {
			udSeqState = UD_SEQ_CHECK_IMAGE;
			udSeqSubState = 0;
		}
		break;
	case UD_SEQ_CHECK_IMAGE:
		result = _udSeqCheckImage(control, message);
		break;
	case UD_SEQ_EDL_PREPARE:
		result = _udSeqEdlPrepare(control, message);
		break;
	case UD_SEQ_EDL_DOWNLOAD:
		result = _udSeqEdlDownload(control, message);
		break;
	case UD_SEQ_EDL_FINALIZE:
		result = _udSeqEdlFinalize(control, message);
		break;
	case UD_SEQ_QDL_PREPARE:
		result = _udSeqQdlPrepare(control, message);
		break;
	case UD_SEQ_QDL_DOWNLOAD:
		result = _udSeqQdlDownload(control, message);
		break;
	case UD_SEQ_QDL_FINALIZE:
		result = _udSeqQdlFinalize(control, message);
		break;
	case UD_SEQ_BACKUP:
		result = _udSeqBackup(control, message);
		break;
	case UD_SEQ_RESTORE:
		result = _udSeqRestore(control, message);
		break;
	default:
		result = (int)0x80330001;
		break;
	}
	if (result < 0 && result != (int)0x8033070D)
		udSeqState = UD_SEQ_FINISHED;

	if (udSeqState != udSeqPreviousState || udSeqPreviousSubState != udSeqSubState) {
		bbmcEnterCS(&control->mutex);
		control->firstStatusValue = udSeqState;
		control->secondStatusValue = udSeqSubState;
		bbmcLeaveCS(&control->mutex);
		bbmcNotifyFeatureEvent(control, 7, 0x4000);
	}
	if (udSeqState != UD_SEQ_FINISHED) {
		if (result == (int)0x8033070D)
			return result;
		return 1;
	}
	result &= result >> 31;

finish:
	swdlTerminate();
	control->downloadNotificationRegistered = 0;
	udSeqState = UD_SEQ_IDLE;
	udSeqSubState = UD_SEQ_IDLE;
	control->firstStatusValue = 0;
	control->secondStatusValue = 0;
	return result;
}

/* name unknown: an update in its download states keeps the modem in state 8 */
SceInt32 udSelectBootTarget(BbmcControl *control, SceInt32 targetState)
{
	if (udIsInitialized() &&
	    (control->firstStatusValue == UD_SEQ_IDLE ||
	     control->firstStatusValue - UD_SEQ_EDL_PREPARE <= 4))
		return 8;
	return targetState;
}
