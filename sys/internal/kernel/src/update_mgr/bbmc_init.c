#include <string.h>
#include <threadmgr/threadmgr.h>
#include "bbmc_update.h"

int BbmcUtilInitialize(void)
{
	unsigned int pattern;
	int result;

	if (!BbmcUtilIsAvailable())
		return (int)0x800f0202;
	if (updateBbmcInitialized)
		return 0;
	memset(&updateBbmcContext, 0, sizeof(updateBbmcContext));
	result = sceKernelCreateEventFlag("SceSblUsBbmcUpdaterResultEVF",
		0x1000, 0, NULL);
	if (result < 0)
		return result;
	updateBbmcContext.resultEventFlagId = result;
	result = UpdateUtilCreateThread("SceSblUsComUtilCallbackHandler",
		SceSblUsComUtilCallbackHandler, 96, 8192, 0x50000, 0x80000, NULL);
	if (result < 0)
		return result;
	updateBbmcContext.threadId = result;
	updateBbmcContext.running = 1;
	result = sceKernelStartThread(result, 0, NULL);
	if (result < 0)
		return result;
	result = sceKernelWaitEventFlagCB(updateBbmcContext.resultEventFlagId,
		3, 5, &pattern, NULL);
	if (!result) {
		if (pattern & 2)
			result = (int)0x800f0229;
		else
			updateBbmcInitialized = 1;
	}
	return result;
}

int SceSblUsComUtilCallbackHandler(SceSize args, void *argp)
{
	union {
		BbmcDriverInfo driverInfo;
		SceInt32 error;
	} reply;
	uint32_t pattern = 0, first, second, events;
	UpdateUtilNodeStatus *status;
	int result, operationResult;

	result = sceKernelCreateEventFlag("SceSblUsBbmcUpdaterEVF", 0x1000, 0, NULL);
	if (result < 0)
		goto exit;
	updateBbmcContext.subscriber.featureId = 7;
	updateBbmcContext.subscriber.eventMask = -1;
	updateBbmcContext.eventFlagId = result;
	result = sceKernelCreateCallback("SceSblUsBbmcUpdaterCallBack", 0,
		BbmcUtilCallback, NULL);
	if (result < 0)
		goto deleteEvent;
	updateBbmcContext.subscriber.callbackId = result;
	result = sceBbmcSubscribeFeatureIdForDriver(&updateBbmcContext.subscriber);
	if (result < 0) {
		sceKernelDeleteCallback(updateBbmcContext.subscriber.callbackId);
		goto deleteEvent;
	}
	updateBbmcContext.state = 0;
	updateBbmcContext.buffer = NULL;
	updateBbmcContext.chunkCount = 0;
	updateBbmcContext.chunkIndex = 0;
	updateBbmcContext.dataSize = 0;
	sceKernelSetEventFlag(updateBbmcContext.eventFlagId, 2);
	for (;;) {
		pattern = 0;
		result = sceKernelWaitEventFlagCB(updateBbmcContext.eventFlagId,
			0xffffff, 1, &pattern, NULL);
		if (result < 0)
			goto exit;
		events = pattern;
		if (events) {
			sceKernelClearEventFlag(updateBbmcContext.eventFlagId, ~events);
			events = pattern;
			if (events & 1)
				break;
		}
		switch (updateBbmcContext.state) {
		case 0:
			operationResult = sceBbmcGetDriverInfoForDriver(
				updateBbmcContext.subscriber.subscriberId, &reply.driverInfo);
			if (operationResult < 0)
				goto failure;
			if (events & 2) {
				events &= ~2u;
				if (reply.driverInfo.currentState <= 1 ||
				    reply.driverInfo.secondBootReply - 1 <= 1 ||
				    reply.driverInfo.secondBootReply == 6) {
					operationResult = sceBbmcReserveOperationModeForDriver(
						updateBbmcContext.subscriber.subscriberId, 1);
					if (operationResult < 0)
						goto failure;
				} else {
					updateBbmcContext.state = 1;
					sceKernelSetEventFlag(updateBbmcContext.eventFlagId, 2);
				}
			}
			if (events & 0x10000) {
				events &= ~0x10000u;
				if (reply.driverInfo.currentState <= 0) {
					operationResult = sceBbmcTurnOnForDriver(
						updateBbmcContext.subscriber.subscriberId);
					if (operationResult < 0)
						goto failure;
				} else {
					operationResult = sceBbmcTurnOffForDriver(
						updateBbmcContext.subscriber.subscriberId);
					if (operationResult != (int)0x80330003 && operationResult < 0)
						goto failure;
				}
			}
			if (events & 0x20000) {
				events &= ~0x20000u;
				operationResult = sceBbmcTurnOnForDriver(
					updateBbmcContext.subscriber.subscriberId);
				if (operationResult < 0)
					goto failure;
			}
			if ((events & 0x80000) && reply.driverInfo.currentState > 1) {
				updateBbmcContext.state = 1;
				sceKernelSetEventFlag(updateBbmcContext.eventFlagId, 2);
			} else if (operationResult < 0) {
				goto failure;
			}
			break;
		case 1:
			operationResult = sceBbmcGetDriverInfoForDriver(
				updateBbmcContext.subscriber.subscriberId, &reply.driverInfo);
			if (operationResult < 0)
				goto failure;
			if ((events & 0x80000) && reply.driverInfo.currentState > 1) {
				operationResult = 0;
				sceKernelSetEventFlag(updateBbmcContext.eventFlagId, 2);
			}
			if (events & 2) {
				updateBbmcContext.ready = 1;
				updateBbmcContext.state = 2;
			} else if (operationResult < 0) {
				goto failure;
			}
			break;
		case 2:
			events &= 0xfff7fffd;
			if (events & 0x1000) {
				updateBbmcContext.progress = (events >> 24) & 0x7f;
				events &= 0x80ffefff;
			}
			if (events & 0x2000) {
				sceBbmcDLGetSwdlErrorForDriver(updateBbmcContext.subscriber.subscriberId,
					&reply.error);
				operationResult = reply.error;
			} else if (events & 0x800000) {
				updateBbmcContext.transferDone = 1;
				operationResult = 0;
			} else {
				if (events & 0x400000) {
					events &= ~0x400000u;
					updateBbmcContext.transferDone = 1;
					updateBbmcContext.shutdown = 1;
					updateBbmcContext.state = 4;
				}
				operationResult = 0;
				if (events & 0x40000)
					sceBbmcDLUpdaterGetSequenceForDriver(
						updateBbmcContext.subscriber.subscriberId,
						&first, &second, &reply.driverInfo);
			}
			status = updateBbmcContext.status;
			if (status) {
				unsigned int rates = status->written_rates;
				unsigned int nodeType = updateBbmcContext.nodeType;
				UpdateUtilNodeStatus *currentStatus = updateBbmcContext.status;

				status->progress = 3;
				if (rates <= 95)
					status->written_rates = rates + 5;
				status->seq_no++;
				UpdateUtilUpdateNodeStatus(nodeType, currentStatus);
			}
			if (operationResult < 0)
				goto failure;
			break;
		case 4:
			goto completed;
		default:
			break;
		}
		if (updateBbmcContext.transferDone || updateBbmcContext.ready) {
			sceKernelSetEventFlag(updateBbmcContext.resultEventFlagId, 1);
			updateBbmcContext.transferDone = 0;
			updateBbmcContext.ready = 0;
		}
	}
completed:
	result = 0;
	goto exit;
failure:
	updateBbmcContext.state = 0;
	sceKernelSetEventFlag(updateBbmcContext.resultEventFlagId, 2);
	result = (int)0x800f0205;
	goto exit;
deleteEvent:
	sceKernelDeleteEventFlag(updateBbmcContext.eventFlagId);
	updateBbmcContext.eventFlagId = 0;
exit:
	updateBbmcContext.running = 0;
	return sceKernelExitDeleteThread(result);
}
