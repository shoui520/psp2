/*
 * The GPS tracking sequence: start (93, 94), progress (95, 55) and stop (99, 100);
 * and the network information update (6, 7): serving system and signal levels.
 */
#include <sysmem/debug.h>
#include <gps/gps.h>
#include <string.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_memory.h"
#include "../bbcmd.h"

#if FW_DEBUG
static const SceKernelDebugInfo dbg_seqUpdateGPS_L230
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "sequence/update_gps.c",
	.line = 230,
	.func = "seqUpdateGPS",
};

static const SceKernelDebugInfoSource dbg_seqUpdateGPS_L230Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_seqUpdateGPS_L230, "sequence/update_gps.c", 230, "seqUpdateGPS",
};
#endif

SceUInt32 gpsUpdateStep;

int seqUpdateGPS(BbmcControl *control, BbmcMessage *message)
{
	int result;

	if ((control->flags & ~4U) != 0) {
		gpsUpdateStep = 0;
		return 1;
	}
	switch (gpsUpdateStep) {
	case 0: {
		BbcmdArgument input[7];
		unsigned int command;

		if (message->command - 93 > 1)
			return 1;
		input[0].value = 0;
		input[2].value = 0;
		input[3].value = 0;
		input[4].value = 0;
		input[5].value = 0;
		input[6].value = 0;
		if (message->command == 93) {
			input[0].bytes[1] = 64;
			input[0].halfwords[1] = 1;
			command = 21;
			input[1].value = 250;
		} else {
			command = 20;
			input[0].bytes[1] = 20;
			input[0].bytes[2] = 7;
			input[1].value = 1000;
		}
		result = bbcmdExecute(0, command, input, NULL, bbcmdManagerCompletion,
				      control);
		if (result < 0)
			break;
		bbmcEnterCS(&control->mutex);
		control->trackingState = message->command == 94 ? 2 : 1;
		bbmcLeaveCS(&control->mutex);
		gpsUpdateStep = 1;
		return result;
	}
	case 1:
		if (message->command == 95) {
			bbmcEnterCS(&control->mutex);
			result = message->body.commandResult;
			if (result != 0) {
				if (result == (int)0x80390090) {
					result = 1;
					SceGpsForDriver_F2049C02(0, 0x10090);
				} else {
					SceGpsForDriver_F2049C02(0, 0x10030);
				}
				gpsUpdateStep = 4;
				control->trackingState = 0;
			} else if (control->trackingState == 2) {
				control->trackingState = 4;
				gpsUpdateStep = 4;
				result = 1;
				SceGpsForDriver_F2049C02(0, 0x10000);
			} else if (control->trackingState == 1) {
				control->trackingState = 3;
				result = 1;
				gpsUpdateStep = 2;
				SceGpsForDriver_F2049C02(0, 0x10000);
			} else {
				result = (int)0x80330001;
#if FW_DEBUG
				sceKernelAssert(0, &dbg_seqUpdateGPS_L230,
						__builtin_return_address(0));
#endif
				gpsUpdateStep = 4;
			}
			bbmcLeaveCS(&control->mutex);
		} else if (message->command == 55) {
			gpsUpdateStep = 4;
			result = 1;
			bbmcEnterCS(&control->mutex);
			control->trackingState = 0;
			bbmcLeaveCS(&control->mutex);
		} else {
			return 1;
		}
		if (gpsUpdateStep != 4)
			return result;
		break;
	case 2:
		if (message->command != 99)
			return 1;
		result = bbcmdExecute(0, 0x16, NULL, NULL, bbcmdManagerCompletion,
				      control);
		if (result < 0)
			break;
		bbmcEnterCS(&control->mutex);
		control->trackingState = 5;
		bbmcLeaveCS(&control->mutex);
		gpsUpdateStep = 3;
		return result;
	case 3:
		if (message->command != 100)
			return 1;
		result = message->body.commandResult;
		if (result != 0) {
			SceGpsForDriver_F2049C02(0, 0x20031);
			if (gpsUpdateStep != 4)
				return result;
			break;
		}
		SceGpsForDriver_F2049C02(0, 0x20000);
		bbmcEnterCS(&control->mutex);
		control->trackingState = 0;
		result = 1;
		bbmcLeaveCS(&control->mutex);
		break;
	default:
		result = (int)0x80330001;
		break;
	}
	gpsUpdateStep = 0;
	return result;
}

SceUInt32 networkInfoStep;
SceUInt32 networkInfoRequeue;
BbmcSignalStrengthInfo pendingSignalStrength;
BbmcServingSystemInfo pendingServingSystem;

int seqUpdateNetworkInfo(BbmcControl *control, BbmcMessage *message)
{
	BbmcNetworkInfo *info = &control->networkInfo;
	unsigned int mask;
	int result;
	int level;
	int i;
	int radio;
	SceInt8 strength;

	switch (networkInfoStep) {
	case 0: {
		BbcmdArgument input[7];
		BbcmdArgument output[6];

		if (message->command != 6)
			return 1;
		mask = 0;
		result = bbcmdGetCompletedMask(0, &mask);
		if (result < 0)
			break;
		if (mask & 2) {
			memset(input, 0, sizeof(input));
			output[2].value = 0;
			output[3].value = 0;
			output[4].value = 0;
			output[5].value = 0;
			memset(&pendingServingSystem, 0, sizeof(pendingServingSystem));
			memset(&pendingSignalStrength, 0, sizeof(pendingSignalStrength));
			output[0].pointer = &pendingServingSystem;
			output[1].pointer = &pendingSignalStrength;
			input[0].halfwords[0] = 31;
			result = bbcmdExecute(0, 0x1b, input, output,
					      bbcmdManagerCompletion, control);
			if (result < 0)
				break;
			networkInfoStep = 1;
			return 1;
		}
		if (networkInfoStep != 2)
			return 1;
		goto done;
	}
	case 1:
		if (message->command != 7) {
			if (message->command == 6)
				networkInfoRequeue = 1;
			return 1;
		}
		if (message->body.commandResult != 0)
			goto requeue;
		if (control->networkStarted != 0 &&
		    info->serving.psAttachState != 1 &&
		    pendingServingSystem.psAttachState == 1)
			bbmcNotifyFeatureEvent(control, 5, 8);
		bbmcEnterCS(&control->mutex);
		memcpy(&info->serving, &pendingServingSystem, sizeof(info->serving));
		memcpy(&info->signal, &pendingSignalStrength, sizeof(info->signal));
		bbmcLeaveCS(&control->mutex);

		if (info->signal.radioInterface > 8)
			goto invalid;
		radio = 1 << info->signal.radioInterface;
		if (radio & 0x136) {
			bbmcEnterCS(&control->mutex);
			strength = info->signal.strength;
			if (strength <= 0)
				strength = -strength;
			if (strength > 125 || strength == -128) {
				if (info->serving.registrationState != 1) {
					level = -1;
					goto signal_level;
				}
				strength = 125;
			}
			for (level = 0; level < control->countedBytes.length; level++)
				if (strength > control->countedBytes.values[level])
					break;
signal_level:
			info->signalLevel = level;
			bbmcLeaveCS(&control->mutex);
		} else if (radio & 1) {
			bbmcEnterCS(&control->mutex);
			info->signalLevel = -1;
			bbmcLeaveCS(&control->mutex);
		} else {
			goto invalid;
		}

		for (i = 0; i < info->signal.rssiCount; i++) {
			BbmcSignalStrength *rssi = &info->signal.rssi[i];

			if (rssi->radioInterface > 8)
				goto invalid;
			radio = 1 << rssi->radioInterface;
			if (radio & 0x136) {
				bbmcEnterCS(&control->mutex);
				strength = rssi->strength;
				if (strength < 0)
					strength = -strength;
				if (strength > 125 || strength == -128) {
					if (info->serving.registrationState != 1) {
						level = -1;
						goto rssi_level;
					}
					strength = 125;
				}
				for (level = 0; level < control->countedBytes.length;
				     level++)
					if (strength > control->countedBytes.values[level])
						break;
rssi_level:
				rssi->level = level;
				bbmcLeaveCS(&control->mutex);
			} else if (radio & 1) {
				bbmcEnterCS(&control->mutex);
				rssi->level = -1;
				bbmcLeaveCS(&control->mutex);
			} else {
				goto invalid;
			}
		}

		if (info->serving.registrationState != 1) {
			SceUInt32 registrationState = info->serving.registrationState;

			bbmcEnterCS(&control->mutex);
			memset(&info->serving, 0, sizeof(info->serving));
			info->serving.registrationState = registrationState;
			info->signal.radioInterface = 0;
			info->signal.validMask = 0;
			bbmcLeaveCS(&control->mutex);
		}
		bbmcNotifyFeatureEvent(control, 0, 4);
		goto registered;
invalid:
		networkInfoStep = 2;
registered:
		if (pendingServingSystem.registrationState == 1) {
			bbmcEnterCS(&control->mutex);
			if (control->powerEventActive == 0)
				control->recentPowerEvent = 0;
			if (control->networkRetryTimer > 0) {
				bbmcTimerCancelHandler(control->networkRetryTimer);
				control->networkRetryTimer = 0;
			}
			if (control->networkStopBusy == 0 &&
			    control->deferredNetworkStart != 0 &&
			    control->targetState == 7 && control->currentState == 7) {
				bbmcSendSimpleMessage(control, 109);
				control->deferredNetworkStart = 0;
			}
			bbmcLeaveCS(&control->mutex);
		}
requeue:
		if (networkInfoRequeue == 1) {
			networkInfoRequeue = 0;
			bbmcSendSimpleMessage(control, 6);
		}
done:
		networkInfoStep = 0;
		networkInfoRequeue = 0;
		return 0;
	default:
		result = (int)0x80330001;
		break;
	}
	networkInfoStep = 0;
	networkInfoRequeue = 0;
	return result;
}
