/*
 * Event notification thread and message pipe dispatcher for SceVoice.
 */

#include "voice_internal.h"

int voiceEventThreadEntry(SceSize args, void *argp)
{
	VoiceEventDispatcher *disp = (VoiceEventDispatcher *)argp;
	SceVoiceEventData eventData __attribute__((aligned(32)));
	SceSize receivedSize = 0;
	(void)args;

	for (;;) {
		int status;
		do {
			voiceMemset(&eventData, 0, sizeof(eventData));
			status = sceKernelReceiveMsgPipe(disp->msgPipeUid, &eventData,
			                                 sizeof(eventData), 1, &receivedSize, NULL);
		} while (status < 0);

		eventData.pUserData = disp->userData;
		if ((uint32_t)eventData.eventType == VOICE_EVENT_QUIT) {
			break;
		}
		if (eventData.eventType == SCEVOICE_EVENT_DATA_READY) {
			if (gSysVoice.activeInstance != NULL &&
			    sysVoiceInstanceCollectDataReady(gSysVoice.activeInstance, &eventData) != 0) {
				disp->callback(&eventData);
			}
		} else if (eventData.eventType == SCEVOICE_EVENT_AUDIOINPUT_STATUS) {
			disp->callback(&eventData);
		}
	}

	return 0;
}

int voiceEventDispatcherInit(VoiceEventDispatcher *disp, SceVoiceEventCallback callback, void *userData)
{
	disp->running = 0;
	if (callback == NULL) {
		return 0;
	}

	disp->callback = callback;
	disp->userData = userData;
	disp->msgPipeUid = sceKernelCreateMsgPipe("SceVoiceMsgpipe", 64, 12,
	                                          sizeof(SceVoiceEventData), NULL);
	disp->threadId = sceKernelCreateThread("SceVoiceEvent",
	                                       (SceKernelThreadEntry)voiceEventThreadEntry,
	                                       gSysVoice.threadPriority, 0x2000, 0,
	                                       gSysVoice.threadAffinityMask, NULL);
	disp->sendPipeUid = disp->msgPipeUid;
	int ret = sceKernelStartThread(disp->threadId, 12, disp);
	disp->running = 1;
	return ret;
}

int voiceEventDispatcherSend(VoiceEventDispatcher *disp, SceVoiceEventData *eventData)
{
	uint32_t type = (uint32_t)eventData->eventType;
	if (type != SCEVOICE_EVENT_DATA_READY &&
	    type != SCEVOICE_EVENT_AUDIOINPUT_STATUS &&
	    type != VOICE_EVENT_QUIT) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	SceSize sentSize = 0;
	unsigned int timeout = 53000;
	return sceKernelSendMsgPipe(disp->sendPipeUid, eventData, sizeof(SceVoiceEventData),
	                            1, &sentSize, &timeout);
}

int voiceEventDispatcherTerm(VoiceEventDispatcher *disp)
{
	if (disp->running == 0) {
		return 0;
	}

	SceVoiceEventData quitEvent __attribute__((aligned(32)));
	voiceMemset(&quitEvent, 0, sizeof(quitEvent));
	quitEvent.eventType = (SceVoiceEventType)VOICE_EVENT_QUIT;
	voiceEventDispatcherSend(disp, &quitEvent);
	sceKernelWaitThreadEnd(disp->threadId, NULL, NULL);
	sceKernelDeleteThread(disp->threadId);
	int ret = sceKernelDeleteMsgPipe(disp->msgPipeUid);
	disp->callback = NULL;
	disp->running = 0;
	return ret;
}
