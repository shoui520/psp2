#include <lowio/pervasive.h>
#include <sysmem/debug.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_memory.h"
#include "../bbcmd.h"
#include "../usb_extif.h"

int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);

SceUInt32 gpsStopStep;
SceUInt32 networkServiceStopStep;
SceUInt32 positionTrackingStopStep;

static const SceKernelDebugInfo dbg_gpsStopAutoTracking_L74
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "state_machine/suspend.c",
	.line = 74,
	.func = "gpsStopAutoTracking",
#endif
};

static const SceKernelDebugInfoSource dbg_gpsStopAutoTracking_L74Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_gpsStopAutoTracking_L74, "state_machine/suspend.c", 74, "gpsStopAutoTracking",
};

static const SceKernelDebugInfo dbg_gpsStopOneShotTracking_L102
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "state_machine/suspend.c",
	.line = 102,
	.func = "gpsStopOneShotTracking",
#endif
};

static const SceKernelDebugInfoSource dbg_gpsStopOneShotTracking_L102Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_gpsStopOneShotTracking_L102, "state_machine/suspend.c", 102,
	"gpsStopOneShotTracking",
};

static inline int gpsStopAutoTracking(BbmcControl *control)
{
	BbmcMessage *request;

	bbmcEnterCS(&control->mutex);
	control->gpsProviderRouting = 1;
	bbmcLeaveCS(&control->mutex);
	request = msgAllocMsgPacket(control, 99);
	if (!request)
		return (int)0x80330005;
	bbmcEnterCS(&control->mutex);
	control->trackingState = 5;
	bbmcLeaveCS(&control->mutex);
	if (msgSendMsgPacket(control, request) < 0)
		SceDebugForDriver_391B5B74(&dbg_gpsStopAutoTracking_L74,
					   __builtin_return_address(0));
	gpsStopStep = 5;
	return 1;
}

int gpsStopOneShotTracking(BbmcControl *control, BbmcMessage *message)
{
	BbmcMessage *request;
	SceUInt32 state;
	int result;

	switch (gpsStopStep) {
	case 0:
		if (control->gpsIoctlPending != 0 && message->command != 102) {
			gpsStopStep = 3;
			return 1;
		}
		goto stop;
	case 1:
		if (message->command == 95)
			goto stopTracking;
		if (message->command == 55)
			goto done;
		return 1;
	case 2:
		state = control->trackingState;
		if (state == 2 || state == 4) {
			request = msgAllocMsgPacket(control, 101);
			if (!request) {
				result = (int)0x80330005;
				goto finish;
			}
			request->body.gpsRequest.data = NULL;
			request->body.gpsRequest.code = 0x53001000;
			request->body.gpsRequest.size = 0;
			bbmcEnterCS(&control->mutex);
			control->gpsIoctlPending = 1;
			bbmcLeaveCS(&control->mutex);
			if (msgSendMsgPacket(control, request) < 0)
				SceDebugForDriver_391B5B74(&dbg_gpsStopOneShotTracking_L102,
							   __builtin_return_address(0));
			gpsStopStep = 4;
			return 1;
		}
		result = 0;
		if ((state & ~2u) == 1) {
			result = gpsStopAutoTracking(control);
			if (result == 1)
				return 1;
			goto finish;
		}
		if (state == 5)
			goto waitStop;
		goto finish;
	case 3:
		if (message->command != 102)
			return 1;
	stop:
		state = control->trackingState;
		if (state - 1 <= 1) {
			gpsStopStep = 1;
			return 1;
		}
		if (state - 3 <= 1) {
	stopTracking:
			gpsStopStep = 2;
			result = bbmcSendSimpleMessage(control, 188);
			if (result < 0)
				goto finish;
			if (gpsStopStep != 6)
				return 1;
			goto done;
		}
		if (state == 5) {
	waitStop:
			gpsStopStep = 5;
			return 1;
		}
		goto done;
	case 4:
		if (message->command == 102)
			goto done;
		return 1;
	case 5:
		if (message->command == 100)
			goto done;
		result = control->trackingState;
		if (result == 0)
			goto finish;
		return 1;
	default:
		if (gpsStopStep == 6)
			goto done;
		return 1;
	}

done:
	gpsStopStep = 0;
	return 0;
finish:
	gpsStopStep = 0;
	return result;
}

int suspendStopNetworkService(BbmcControl *control, BbmcMessage *message)
{
	SceUInt32 command;
	int result;

	switch (networkServiceStopStep) {
	case 0: {
		BbcmdArgument input[7] = {{0}};

		if (bbcmdExecute(0, 0, input, NULL, bbcmdManagerCompletion, control) < 0) {
			result = bbmcSendSimpleMessage(control, 188);
			if (result < 0)
				goto failed;
		}
		networkServiceStopStep = 1;
		return 1;
	}
	case 1:
		command = message->command;
		if (command == 188 || command == 55) {
			result = bbcmdExecute(0, 7, NULL, NULL, bbcmdManagerCompletion, control);
			if (result < 0)
				goto failed;
			networkServiceStopStep = 2;
		}
		return 1;
	case 2:
		if (message->command == 60) {
			result = 0;
			goto finish;
		}
		if (message->command == 61) {
			ScePervasiveForDriver_4B99558F();
			result = 0;
			control->flags |= 2;
			goto finish;
		}
		return 1;
	case 3:
		result = 1;
		goto failed;
	default:
		return 1;
	}

failed:
	result &= result >> 31;
finish:
	networkServiceStopStep = 0;
	return result;
}

int bbmcPrepareUsbShutdown(BbmcControl *control)
{
	int mode;

	if (bbmcUsbExtIfGetMode(&mode) == 0 && mode >= 0) {
		control->backendShutdownRequested = 1;
		bbmcUsbExtIfReleaseGpio();
	}
	return 0;
}

int suspendStopPositionTracking(BbmcControl *control, BbmcMessage *message)
{
	int result;

	switch (positionTrackingStopStep) {
	case 0: {
		BbcmdArgument input[7] = {{0}};

		if (message->command != 79)
			return 1;
		result = bbcmdExecute(0, 30, input, NULL, bbcmdManagerCompletion, control);
		if (result < 0) {
			result &= result >> 31;
			positionTrackingStopStep = 0;
			return result;
		}
		positionTrackingStopStep = 1;
		return 1;
	}
	case 1:
		if (message->command != 81)
			return 1;
		break;
	case 2:
		break;
	default:
		return 1;
	}
	positionTrackingStopStep = 0;
	return 0;
}
