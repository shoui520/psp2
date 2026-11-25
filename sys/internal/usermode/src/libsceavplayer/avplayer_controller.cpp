#include "avplayer_internal.h"

static void avControllerThreadEntry(void *context)
{
	AvStateMachine *sm = (AvStateMachine *)context;
	if (sm != NULL)
		sm->controllerThread();
}

static int32_t avFlowMgrDispatch(AvController *controller, AvEvent *event, int32_t sender, void *outData)
{
	if (controller == NULL)
		return -1;
	return controller->handleFlowEvent(event, sender, outData);
}

/* AvStateMachine implementation */
int32_t AvStateMachine::init(void *player, SceAvPlayerMemAllocator *alloc, int32_t priority, int32_t dbgLevel, AvController *ctrl, AvFlowMgrCallback cb)
{
	initBaseClock();
	reservedAlign = 0;
	stopFlag = 0;
	previousState = AV_STATE_UNINIT;
	pendingAckCount = 0;
	threadId = -1;
	syncCheckFlag = 0;

	SceKernelLwMutexOptParam opt;
	sceClibMemset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	if (sceKernelCreateLwMutex(&stateMutex, "StateMachine Critical ", 0, 0, &opt) < 0)
		return -1;
	if (sceKernelCreateLwMutex(&eventMutex, "EventHandler Critical ", 0, 0, &opt) < 0) {
		sceKernelDeleteLwMutex(&stateMutex);
		return -1;
	}

	userEventQueue.init();
	ackEventQueue.init();

	avAtomicStore8(&currentState, AV_STATE_INIT);
	debugLevel = dbgLevel;
	memAlloc = alloc;
	basePriority = (priority != 0) ? priority : 140;
	controller = ctrl;
	flowCallback = cb;
	playerInstance = player;
	return 0;
}

int32_t AvStateMachine::start()
{
	avAtomicStore32(&stopFlag, 0);
	SceUID tid = sceKernelCreateThread("avPlayer Controller",
	                                   (SceKernelThreadEntry)avThreadTrampoline,
	                                   basePriority + 9,
	                                   0x4000u,
	                                   0,
	                                   0,
	                                   NULL);
	if (tid >= 0) {
		threadId = tid;
		AvThreadStartArg startArg;
		startArg.entry = avControllerThreadEntry;
		startArg.context = this;
		sceKernelStartThread(tid, sizeof(startArg), &startArg);
	}
	return 0;
}

int32_t AvStateMachine::stop()
{
	if (stopFlag == 1)
		return 0;

	avAtomicStore32(&stopFlag, 1);
	avLogDebug(debugLevel, "Stop", 68, "AvController stopped");
	if (sceKernelLockLwMutex(&eventMutex, 1, NULL) < 0) {
		avLogError(debugLevel, "Stop", 71, "Failed to enter Critical section for Events");
		return -1;
	}

	ackEventQueue.clear();
	userEventQueue.clear();

	if (sceKernelUnlockLwMutex(&eventMutex, 1) < 0) {
		avLogError(debugLevel, "Stop", 80, "Failed to Exit Critical section for Events");
		return -2;
	}
	resetClock();
	return 0;
}

int32_t AvStateMachine::shutdown()
{
	avLogDebug(debugLevel, "Shutdown", 92, "Shuting down AvController");
	stop();
	ackEventQueue.clear();
	userEventQueue.clear();

	if (threadId < 0 ||
	    sceKernelWaitThreadEnd(threadId, NULL, NULL) < 0 ||
	    sceKernelDeleteThread(threadId) < 0) {
		avLogError(debugLevel, "Shutdown", 99, "Failed to exit AvController thread ");
	}
	threadId = -1;

	avAtomicStore8(&currentState, AV_STATE_UNINIT);
	sceKernelDeleteLwMutex(&stateMutex);
	return 0;
}

int32_t AvStateMachine::isStateTransitionValid(int8_t nextState)
{
	int8_t cur = (int8_t)currentState;
	switch (nextState) {
	case AV_STATE_PLAY:
		if (cur == AV_STATE_EOF || cur == AV_STATE_STOP || cur == AV_STATE_EOF_PAUSE || cur == AV_STATE_ERROR) {
			avLogWarn(debugLevel, "IsStateTransitionValid", 522,
			          "Cannot change state from %d to SCE_MVPCORE_STATE_PLAY", cur);
			return 0;
		}
		return 1;
	case AV_STATE_PAUSE:
		if (cur == AV_STATE_EOF || cur == AV_STATE_STOP || cur == AV_STATE_EOF_PAUSE ||
		    cur == AV_STATE_TRANSITION || cur == AV_STATE_ERROR) {
			avLogWarn(debugLevel, "IsStateTransitionValid", 515,
			          "Cannot change state from %d to SCE_MVPCORE_STATE_PAUSE", cur);
			return 0;
		}
		return 1;
	case AV_STATE_JUMP:
		if (cur == AV_STATE_EOF || cur == AV_STATE_STOP || cur == AV_STATE_EOF_PAUSE ||
		    cur == AV_STATE_TRANSITION || cur == AV_STATE_ERROR) {
			avLogWarn(debugLevel, "IsStateTransitionValid", 540,
			          "Cannot change state from %d to SCE_MVPCORE_STATE_JUMP", cur);
			return 0;
		}
		return (cur == AV_STATE_TRICKMODE) ? 0 : 1;
	case AV_STATE_TRICKMODE:
		if (cur == AV_STATE_EOF || cur == AV_STATE_STOP || cur == AV_STATE_EOF_PAUSE ||
		    cur == AV_STATE_TRANSITION || cur == AV_STATE_ERROR) {
			avLogWarn(debugLevel, "IsStateTransitionValid", 530,
			          "Cannot change state from %d to SCE_MVPCORE_STATE_TRICKMODE", cur);
			return 0;
		}
		return (cur == AV_STATE_JUMP) ? 0 : 1;
	case AV_STATE_BUFFERING:
		if (cur == AV_STATE_EOF || cur == AV_STATE_EOF_PAUSE || cur == AV_STATE_STOP ||
		    cur == AV_STATE_TRANSITION || cur == AV_STATE_ERROR || cur == AV_STATE_PAUSE) {
			avLogWarn(debugLevel, "IsStateTransitionValid", 550,
			          "Cannot change state from %d to SCE_MVPCORE_STATE_BUFFERING", cur);
			return 0;
		}
		return 1;
	default:
		return 1;
	}
}

int32_t AvStateMachine::setState(int8_t nextState)
{
	sceKernelLockLwMutex(&stateMutex, 1, NULL);
	int32_t valid = isStateTransitionValid(nextState);
	if (valid == 1) {
		avAtomicStore8(&previousState, currentState);
		avAtomicStore8(&currentState, (uint8_t)nextState);
		avLogDebug(debugLevel, "SetState", 488, "State changed to : %d ", (int8_t)currentState);
	}
	sceKernelUnlockLwMutex(&stateMutex, 1);
	return (valid == 1) ? 0 : -1;
}

volatile uint8_t *AvStateMachine::getStatePtr()
{
	return &currentState;
}

int8_t AvStateMachine::getPreviousState()
{
	return (int8_t)previousState;
}

int32_t AvStateMachine::postUserEvent(int32_t eventType, int64_t payload)
{
	int8_t cur = (int8_t)currentState;
	if (cur == AV_STATE_STOP || cur == AV_STATE_EOF || cur == AV_STATE_EOF_PAUSE ||
	    cur == AV_STATE_ERROR || cur == AV_STATE_TRANSITION || cur == AV_STATE_READY) {
		avLogError(debugLevel, "UserEvent", 116, "Event cannot be handled in this state: %d ", cur);
		return -1;
	}
	if (sceKernelLockLwMutex(&eventMutex, 1, NULL) < 0) {
		avLogError(debugLevel, "UserEvent", 123, "Failed to enter Critical section for Events");
		return -2;
	}

	AvEvent ev;
	ev.type = eventType;
	ev.reserved = 0;
	ev.payload = payload;
	userEventQueue.pushBack(ev);

	if (sceKernelUnlockLwMutex(&eventMutex, 1) < 0) {
		avLogError(debugLevel, "UserEvent", 130, "Failed to Exit Critical section for Events");
		return -3;
	}
	avLogDebug(debugLevel, "UserEvent", 133, "New User Event added to Q, Type: %d at state: %d ", eventType, (int8_t)currentState);
	return 0;
}

void AvStateMachine::postAckEvent(int32_t sender, int32_t eventType, int64_t timeStamp, int32_t status)
{
	AvAckEvent ack;
	sceClibMemset(&ack, 0, sizeof(ack));
	ack.sender = sender;
	ack.eventType = eventType;
	ack.timeStamp = timeStamp;
	ack.status = status;
	ackEventQueue.pushBack(ack);
}

int32_t AvStateMachine::mapStateCode(int32_t stateCode)
{
	if (stateCode >= AV_STATE_READY && stateCode <= AV_STATE_TRANSITION)
		return stateCode;
	return 0;
}

int32_t AvStateMachine::processUserEvent()
{
	if (currentState == AV_STATE_JUMP)
		return -2;

	if (sceKernelLockLwMutex(&eventMutex, 1, NULL) < 0) {
		avLogError(debugLevel, "ProcessEvent", 146, "Failed to enter Critical section for Events");
		return -3;
	}

	int8_t savedState = (int8_t)currentState;
	if (userEventQueue.count == 0) {
		if (sceKernelUnlockLwMutex(&eventMutex, 1) < 0) {
			avLogError(debugLevel, "ProcessEvent", 153, "Failed to ExitCritical section for Events");
			return -3;
		}
		return -1;
	}

	AvEvent ev = userEventQueue.at(0);
	uint32_t ackExpected = 0;

	switch (ev.type) {
	case AV_EVENT_JUMP:
		if (setState(AV_STATE_JUMP) >= 0) {
			if (flowCallback(controller, &ev, AV_SENDER_DEMUX, &ackExpected) < 0) {
				avLogWarn(debugLevel, "ProcessEvent", 169, "Cannot JUMP, revert back state");
				setState(savedState);
			}
			avAtomicStore32(&pendingAckCount, ackExpected);
		} else {
			avLogWarn(debugLevel, "ProcessEvent", 164, "Cannot move to Jump State");
		}
		break;
	case AV_EVENT_TRICKMODE:
		if (flowCallback(controller, &ev, AV_SENDER_DEMUX, &ackExpected) < 0) {
			avLogWarn(debugLevel, "ProcessEvent", 187, "Cannot JUMP, revert back state");
			setState(savedState);
		}
		avAtomicStore32(&pendingAckCount, ackExpected);
		break;
	case AV_EVENT_JUMP_DONE_WARN: {
		AvEvent warnEv;
		warnEv.type = AV_EVENT_SEND_WARNING;
		warnEv.reserved = 0;
		warnEv.payload = (int64_t)SCE_AVPLAYER_WAR_JUMP_COMPLETE;
		flowCallback(controller, &warnEv, AV_SENDER_MONITOR, NULL);
		break;
	}
	case AV_EVENT_THUMBNAIL_DONE:
		avLogDebug(debugLevel, "ProcessEvent", 202, "Thumbnail mode to Previous state (%d) ", (int8_t)previousState);
		setState((int8_t)previousState);
		break;
	case AV_EVENT_CHECK_SOURCE: {
		int32_t isFirst = (ev.payload != 1) ? 1 : 0;
		if (flowCallback(controller, &ev, AV_SENDER_DEMUX, &isFirst) < 0) {
			userEventQueue.at(0).payload = ev.payload + 1;
			if (ev.payload + 1 < 2) {
				if (sceKernelUnlockLwMutex(&eventMutex, 1) < 0)
					avLogError(debugLevel, "ProcessEvent", 226, "Failed to Exit Eventhandler mutex");
				return 0;
			}
			AvEvent warnEv;
			warnEv.type = AV_EVENT_SEND_WARNING;
			warnEv.reserved = 0;
			warnEv.payload = (int64_t)SCE_AVPLAYER_ERR_NOT_SUPPORTED;
			flowCallback(controller, &warnEv, AV_SENDER_MONITOR, NULL);
			int8_t cur = (int8_t)currentState;
			if (cur != AV_STATE_STOP && cur != AV_STATE_EOF && cur != AV_STATE_ERROR)
				setState(AV_STATE_ERROR);
		}
		break;
	}
	case AV_EVENT_ERROR:
		avLogDebug(debugLevel, "ProcessEvent", 194, "Processing Error Event ");
		if (flowCallback(controller, &ev, AV_SENDER_DEMUX, &ackExpected) < 0) {
			avLogWarn(debugLevel, "ProcessEvent", 196, "Cannot JUMP, revert back state");
			setState(savedState);
		}
		break;
	default:
		break;
	}

	userEventQueue.eraseAt(0);
	if (sceKernelUnlockLwMutex(&eventMutex, 1) < 0)
		avLogError(debugLevel, "ProcessEvent", 240, "Failed to Exit Eventhandler mutex");
	return 0;
}

int32_t AvStateMachine::processAckEvent()
{
	if (ackEventQueue.count == 0)
		return -1;

	AvAckEvent ack = ackEventQueue.at(0);

	if (ack.eventType == AV_EVENT_RESET && ack.sender == AV_SENDER_DECODER) {
		if (ackEventQueue.count < pendingAckCount)
			return -1;

		uint32_t matched = 0;
		uint32_t cursor = 0;
		while (cursor < ackEventQueue.count && matched < pendingAckCount) {
			AvAckEvent &curAck = ackEventQueue.at(cursor);
			if (curAck.sender == AV_SENDER_DECODER && curAck.eventType == AV_EVENT_RESET) {
				ackEventQueue.eraseAt(cursor);
				matched++;
				cursor = 0;
			} else {
				cursor++;
			}
		}

		int8_t prev = (int8_t)previousState;
		if (prev == AV_STATE_PLAY || prev == AV_STATE_BUFFERING) {
			setState(AV_STATE_PLAY);
		} else if (prev == AV_STATE_PAUSE) {
			setState(AV_STATE_JUMP_PAUSE);
			previousState = AV_STATE_PAUSE;
		} else {
			setState(prev);
		}
		syncCheckFlag = 1;
		return 0;
	}

	if (ack.eventType == AV_EVENT_JUMP) {
		if (ack.sender == AV_SENDER_DEMUX) {
			AvEvent decJump;
			decJump.type = AV_EVENT_JUMP;
			decJump.reserved = 0;
			decJump.payload = ack.timeStamp;
			updateVideoPts((uint64_t)ack.timeStamp);
			resumeClock();
			if (ack.status != 0) {
				int8_t prev = (int8_t)previousState;
				if (prev == AV_STATE_PLAY || prev == AV_STATE_BUFFERING)
					setState(AV_STATE_PLAY);
				else
					setState(prev);
			} else {
				flowCallback(controller, &decJump, AV_SENDER_DECODER, NULL);
			}
		} else if (ack.sender == AV_SENDER_DECODER) {
			if (ackEventQueue.count < pendingAckCount)
				return -1;

			uint32_t matched = 0;
			uint32_t cursor = 0;
			while (cursor < ackEventQueue.count && matched < pendingAckCount) {
				AvAckEvent &curAck = ackEventQueue.at(cursor);
				if (curAck.sender == AV_SENDER_DECODER && curAck.eventType == AV_EVENT_JUMP) {
					AvEvent notifyEv;
					notifyEv.type = AV_EVENT_JUMP;
					notifyEv.reserved = 0;
					notifyEv.payload = ack.timeStamp;
					flowCallback(controller, &notifyEv, AV_SENDER_DECODER, (void *)-1);
					ackEventQueue.eraseAt(cursor);
					matched++;
					cursor = 0;
				} else {
					cursor++;
				}
			}

			int8_t prev = (int8_t)previousState;
			if (prev == AV_STATE_PLAY || prev == AV_STATE_BUFFERING) {
				setState(AV_STATE_PLAY);
			} else if (prev == AV_STATE_PAUSE) {
				setState(AV_STATE_JUMP_PAUSE);
				previousState = AV_STATE_PAUSE;
			} else {
				setState(prev);
			}
			syncCheckFlag = 1;
			resumeClock();
			return 0;
		}
	} else if (ack.eventType == AV_EVENT_TRICKMODE) {
		avLogDebug(debugLevel, "ProcessAckEvent", 356, "Received ack event for Trickspeed");
	}

	ackEventQueue.eraseAt(0);
	return 0;
}

int32_t AvStateMachine::monitorBuffering()
{
	int8_t cur = (int8_t)currentState;
	if (cur == AV_STATE_PLAY) {
		int32_t needsBuffer = 0;
		AvEvent ev;
		ev.type = AV_EVENT_CHECK_BUFFERING;
		ev.reserved = 0;
		ev.payload = 0;
		if (flowCallback(controller, &ev, AV_SENDER_MONITOR, &needsBuffer) < 0) {
			avLogDebug(debugLevel, "MonitorPlayer", 436, " Cannot perform buffering check mode");
			return -1;
		}
		if (needsBuffer == 1) {
			avLogDebug(debugLevel, "MonitorPlayer", 441, "Entered Buffering mode");
			ev.type = AV_EVENT_CHANGE_STATE;
			ev.payload = AV_STATE_BUFFERING;
			if (flowCallback(controller, &ev, AV_SENDER_MONITOR, NULL) < 0)
				avLogWarn(debugLevel, "MonitorPlayer", 445, "Failed to process Callback event");
		}
	} else if (cur == AV_STATE_BUFFERING) {
		int32_t stillBuffering = 0;
		AvEvent ev;
		ev.type = AV_EVENT_CHECK_BUFFERING;
		ev.reserved = 0;
		ev.payload = 50;
		if (flowCallback(controller, &ev, AV_SENDER_MONITOR, &stillBuffering) < 0) {
			avLogDebug(debugLevel, "MonitorPlayer", 454, " Cannot perform buffering check mode");
			return -1;
		}
		if (stillBuffering == 0) {
			avLogDebug(debugLevel, "MonitorPlayer", 458, "Changing from Buffering mode to previous state");
			int8_t prev = (int8_t)previousState;
			ev.type = AV_EVENT_CHANGE_STATE;
			ev.payload = (prev == AV_STATE_JUMP || prev == AV_STATE_TRICKMODE) ? AV_STATE_PLAY : prev;
			resumeClock();
			if (flowCallback(controller, &ev, AV_SENDER_MONITOR, NULL) < 0)
				avLogWarn(debugLevel, "MonitorPlayer", 472, "Failed to process Callback event");
		}
	}
	return 0;
}

int32_t AvStateMachine::monitorAvSync()
{
	int8_t cur = (int8_t)currentState;
	if (cur != AV_STATE_PLAY && cur != AV_STATE_EOF && cur != AV_STATE_EOF_PAUSE)
		return -1;

	int64_t videoPtsVal = 0;
	int64_t audioPtsVal = 0;
	AvEvent ev;
	ev.type = AV_EVENT_GET_VIDEO_PTS;
	ev.reserved = 0;
	ev.payload = 0;
	flowCallback(controller, &ev, AV_SENDER_MONITOR, &videoPtsVal);

	ev.type = AV_EVENT_GET_AUDIO_PTS;
	flowCallback(controller, &ev, AV_SENDER_MONITOR, &audioPtsVal);

	if (videoPtsVal > 0 && audioPtsVal > 0) {
		updateVideoPts((uint64_t)videoPtsVal);
		int32_t syncDecision = evaluateVideoSync((uint64_t)videoPtsVal);
		if (syncDecision == AV_CLOCK_AUDIO_AHEAD) {
			ev.type = AV_EVENT_DROP_VIDEO_FRAME;
			flowCallback(controller, &ev, AV_SENDER_MONITOR, NULL);
			if (syncCheckFlag == 1)
				syncCheckFlag = 0;
		} else if (syncDecision == AV_CLOCK_DROP_LATE && syncCheckFlag == 1) {
			ev.type = AV_EVENT_DROP_VIDEO_FRAME;
			flowCallback(controller, &ev, AV_SENDER_MONITOR, NULL);
		}
	}
	return 0;
}

void AvStateMachine::controllerThread()
{
	avLogDebug(debugLevel, "avControllerThread", 369, "Thread started");
	while (stopFlag == 0) {
		if (ackEventQueue.count != 0) {
			processAckEvent();
		} else if (userEventQueue.count != 0) {
			processUserEvent();
		} else {
			sceKernelDelayThread(5000);
			monitorAvSync();
			monitorBuffering();
		}
	}
	avLogDebug(debugLevel, "avControllerThread", 382, "Thread exiting");
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
}

/* AvController implementation */
int32_t AvController::init(SceAvPlayerInitData *initData, void *playerInstance)
{
	if (initData == NULL)
		return -1;

	streamList.init();
	streamDescList.init();
	decoderList.init();

	demux = NULL;
	loopFlagPending = 0;
	clockSynced = 0;
	loopingEnabled = 0;
	videoLoopPending = 0;
	audioLoopPending = 0;
	loopBothReady = 0;
	activeAudioStream = -1;
	streamMutex = sceKernelCreateMutex("AvControllerMutex", 0x2000u, 0, NULL);
	playbackMode = AV_PLAYMODE_NORMAL;
	trickSpeed = SCE_AVPLAYER_SPEED_NORMAL;
	jumpCompletePending = 0;
	memAlloc = initData->memoryReplacement;
	eventCb = initData->eventReplacement;
	fileOps = initData->fileReplacement;
	audioDecoder = NULL;
	videoDecoder = NULL;
	sceClibMemset(sourcePath, 0, sizeof(sourcePath));
	sourceId = 0;
	basePriority = (initData->basePriority != 0) ? (int32_t)initData->basePriority : 160;
	numOutputVideoBuffers = (initData->numOutputVideoFrameBuffers >= 2 && initData->numOutputVideoFrameBuffers <= 6)
	                        ? initData->numOutputVideoFrameBuffers : 2;
	debugLevel = initData->debugLevel;
	highFrameRateFlag = 0;
	timedTextAuBufSize = 0x10000u;
	audioAuBufSize = 0x20000u;
	videoAuBufSize = 0x80000u;

	stateMachine = (AvStateMachine *)avMspaceAlloc(8, sizeof(AvStateMachine));
	if (stateMachine == NULL ||
	    stateMachine->init(playerInstance, &memAlloc, basePriority, debugLevel, this, avFlowMgrDispatch) < 0) {
		return -1;
	}
	stateMachine->start();

	demux = (AvCoreDemux *)avMspaceAlloc(8, sizeof(AvCoreDemux));
	if (demux == NULL || demux->init(stateMachine, this, basePriority, debugLevel) < 0)
		return -1;

	registerVideoDecoder(new AvAvcDecoder());
	registerAudioDecoder(new AvAacDecoder());
	return 0;
}

int32_t AvController::setDemuxVideoBufferSize(uint32_t size)
{
	if (size == 0)
		return -1;
	videoAuBufSize = size;
	return 0;
}

int32_t AvController::addSource(const char *path)
{
	if (path == NULL || demux == NULL || stateMachine == NULL)
		return -1;

	sceClibSnprintf(sourcePath, sizeof(sourcePath), "%s", path);
	stateMachine->setState(AV_STATE_OPENING);
	return stateMachine->postUserEvent(AV_EVENT_CHECK_SOURCE, 0);
}

int32_t AvController::getStreamCount()
{
	return (int32_t)streamDescList.count;
}

int32_t AvController::getStreamInfo(uint32_t streamIndex, SceAvPlayerStreamInfo *outInfo)
{
	if (streamIndex >= streamDescList.count) {
		avLogError(debugLevel, "GetStreamInfo", 1028, " Cannot find Stream Index ");
		return -1;
	}

	sceClibMemset(outInfo, 0, sizeof(SceAvPlayerStreamInfo));
	AvStreamDescriptor *desc = streamDescList.at(streamIndex);
	outInfo->duration = desc->duration / 10u;
	outInfo->startTime = desc->startTime;

	if (desc->streamType == SCE_AVPLAYER_VIDEO) {
		outInfo->type = SCE_AVPLAYER_VIDEO;
		outInfo->details.video.width = (desc->width + 15u) & ~15u;
		outInfo->details.video.height = (desc->height + 15u) & ~15u;
		for (int i = 0; i < 4; i++)
			outInfo->details.video.languageCode[i] = desc->languageCode[i];
		outInfo->details.video.aspectRatio = (float)((double)desc->aspectRatioScaled / 1000000.0);
		if (desc->frameRateNum >= 100)
			highFrameRateFlag = 1;
	} else if (desc->streamType == SCE_AVPLAYER_AUDIO) {
		outInfo->type = SCE_AVPLAYER_AUDIO;
		outInfo->details.audio.channelCount = desc->channelCount;
		outInfo->details.audio.sampleRate = desc->sampleRate;
		for (int i = 0; i < 4; i++)
			outInfo->details.audio.languageCode[i] = desc->languageCode[i];
	} else if (desc->streamType == SCE_AVPLAYER_TIMEDTEXT) {
		outInfo->type = SCE_AVPLAYER_TIMEDTEXT;
		for (int i = 0; i < 4; i++)
			outInfo->details.subs.languageCode[i] = desc->languageCode[i];
		outInfo->details.subs.fontSize = desc->fontSize;
	} else {
		outInfo->type = SCE_AVPLAYER_UNKNOWN;
	}
	return 0;
}

int32_t AvController::registerVideoDecoder(AvDecoder *dec)
{
	if (dec == NULL)
		return -1;
	videoDecoder = dec;
	decoderList.pushBack(dec);
	return 0;
}

int32_t AvController::registerAudioDecoder(AvDecoder *dec)
{
	if (dec == NULL)
		return -1;
	audioDecoder = dec;
	decoderList.pushBack(dec);
	return 0;
}

int32_t AvController::hasDecoderForType(int32_t streamType)
{
	for (uint32_t i = 0; i < decoderList.count; i++) {
		AvDecoder *dec = decoderList.at(i);
		if (dec == NULL)
			break;
		if (dec->descriptor.streamType == streamType || dec->streamType == streamType)
			return 1;
	}
	return 0;
}

AvDecoder *AvController::findDecoderForType(int32_t streamType)
{
	for (uint32_t i = 0; i < decoderList.count; i++) {
		AvDecoder *dec = decoderList.at(i);
		if (dec == NULL)
			break;
		if (dec->descriptor.streamType == streamType || dec->streamType == streamType)
			return dec;
	}
	return NULL;
}

void *AvController::avMemAlign(uint32_t alignment, SceSize size)
{
	if (size == 0) {
		avLogError(debugLevel, "avMemAlign", 954, "Error in allocation size");
		return NULL;
	}
	void *mem = memAlloc.allocate(memAlloc.objectPointer, alignment, size);
	if (mem != NULL)
		sceClibMemset(mem, 0, size);
	return mem;
}

void *AvController::avGraphicsMemAlign(uint32_t alignment, SceSize size)
{
	if (memAlloc.allocateTexture == NULL) {
		avLogError(debugLevel, "avGraphicsMemAlign", 992, " texture allocation fun not found ");
		return NULL;
	}
	return memAlloc.allocateTexture(memAlloc.objectPointer, alignment, size);
}

void AvController::avMemFree(void *ptr)
{
	if (ptr != NULL && memAlloc.deallocate != NULL)
		memAlloc.deallocate(memAlloc.objectPointer, ptr);
}

void AvController::avGraphicsFree(void *ptr)
{
	if (memAlloc.deallocateTexture != NULL)
		memAlloc.deallocateTexture(memAlloc.objectPointer, ptr);
	else
		avLogError(debugLevel, "avGraphicsFree", 1001, " texture deallocation fun not found ");
}

int32_t AvController::enableFrameOutput(AvStreamSlot *slot)
{
	AvOutputFifo *fifo = (AvOutputFifo *)avMspaceAlloc(32, sizeof(AvOutputFifo));
	if (fifo != NULL) {
		fifo->readyQueue.init();
		fifo->freeQueue.init();
	}
	slot->outputFifo = fifo;

	AvStreamDescriptor *desc = slot->descriptor;
	uint32_t frameSize = 0;
	uint32_t totalSize = 0;
	uint32_t maxHold = 0;
	void *mem = NULL;

	if (desc->streamType == SCE_AVPLAYER_VIDEO) {
		uint32_t alignedW = (desc->width + 15u) & ~15u;
		uint32_t alignedH = (desc->height + 15u) & ~15u;
		frameSize = (3u * alignedW * alignedH) >> 1;
		totalSize = frameSize * (uint32_t)numOutputVideoBuffers;
		maxHold = (numOutputVideoBuffers - 1 > 3) ? 3u : (uint32_t)(numOutputVideoBuffers - 1);
		mem = avGraphicsMemAlign(128, totalSize);
		slot->outputBufferMem = mem;
		if (mem == NULL) {
			avLogError(debugLevel, "EnableFrameOutput", 1169,
			           " Video Frame Output Memory of %d cannot be allocated ", totalSize);
			return -1;
		}
		if (totalSize == 0)
			return 0;
	} else if (desc->streamType == SCE_AVPLAYER_AUDIO) {
		slot->outputBufferMem = NULL;
		frameSize = 0x8000u;
		totalSize = 0x20000u;
		maxHold = 2;
		mem = avMemAlign(32, totalSize);
		slot->outputBufferMem = mem;
		if (mem == NULL) {
			avLogError(debugLevel, "EnableFrameOutput", 1184,
			           " Audio Frame Output Memory of %d cannot be allocated ", totalSize);
			return -1;
		}
	} else if (desc->streamType == SCE_AVPLAYER_TIMEDTEXT) {
		slot->outputBufferMem = NULL;
		if (fifo != NULL) {
			fifo->freeQueue.destroy();
			fifo->readyQueue.destroy();
			avMspaceFree(fifo);
		}
		slot->outputFifo = NULL;
		return 0;
	} else {
		avLogError(debugLevel, "EnableFrameOutput", 1195, " Invalid Stream Type ");
		return -1;
	}

	if (slot->outputFifo != NULL)
		slot->outputFifo->init(totalSize, frameSize, maxHold, mem, "decbuff", debugLevel);
	return 0;
}

int32_t AvController::enableAuInput(AvStreamSlot *slot, uint32_t trackOffset)
{
	uint32_t allocSize = 0;
	int32_t stype = slot->descriptor->streamType;
	if (stype == SCE_AVPLAYER_VIDEO) {
		allocSize = videoAuBufSize + trackOffset;
		avLogDebug(debugLevel, "EnableAuInput", 1106,
		           "Attempting to allocate %d bytes (%d user-defined + %d track offset) for video AU buffer",
		           allocSize, videoAuBufSize, trackOffset);
	} else if (stype == SCE_AVPLAYER_AUDIO) {
		allocSize = audioAuBufSize;
	} else if (stype == SCE_AVPLAYER_TIMEDTEXT) {
		allocSize = timedTextAuBufSize;
	} else {
		avLogError(debugLevel, "EnableAuInput", 1118, " Invalid Stream Type ");
		return -1;
	}

	void *auMem = avMemAlign(1, allocSize);
	slot->auBufferMem = auMem;
	if (auMem == NULL) {
		avLogError(debugLevel, "EnableAuInput", 1123, " AU buffer of %d cannot be allocated ", allocSize);
		return -1;
	}
	sceClibMemset(auMem, 0, allocSize);

	AvAuFifo *fifo = (AvAuFifo *)avMspaceAlloc(1, sizeof(AvAuFifo));
	if (fifo == NULL) {
		avLogError(debugLevel, "EnableAuInput", 1130, "Cannot allocate memory for AU Input Buffer");
		return -1;
	}
	fifo->queue.init();
	fifo->debugLevel = debugLevel;
	slot->auFifo = fifo;

	if (fifo->init(allocSize, auMem, "FilterAu") < 0) {
		avLogError(debugLevel, "EnableAuInput", 1136,
		           " Cannot Initialze AU buffer for decoder with size: %d ", allocSize);
		return -1;
	}
	return 0;
}

int32_t AvController::enableDecoder(AvStreamSlot *slot, uint32_t trackOffset)
{
	if (enableAuInput(slot, trackOffset) < 0)
		return -1;
	if (enableFrameOutput(slot) < 0)
		return -1;

	AvCoreDecoder *core = (AvCoreDecoder *)avMspaceAlloc(4, sizeof(AvCoreDecoder));
	if (core != NULL) {
		core->initCore();
		core->outputFifo = slot->outputFifo;
		core->stateMachine = stateMachine;
	}
	slot->coreDecoder = core;

	AvDecoder *dec = NULL;
	int32_t stype = slot->descriptor->streamType;
	if (stype == SCE_AVPLAYER_VIDEO) {
		dec = videoDecoder;
	} else if (stype == SCE_AVPLAYER_AUDIO) {
		dec = audioDecoder;
	} else if (stype == SCE_AVPLAYER_TIMEDTEXT) {
		avLogDebug(debugLevel, "EnableDecoder", 1253,
		           "Nothing to Enable as there is no Decoder for Timed-Text");
	} else {
		avLogError(debugLevel, "EnableDecoder", 1256, " Invalid Stream Type ");
		return -1;
	}

	if (slot->outputFifo != NULL && core != NULL) {
		core->setInputAndDecoder(slot->auFifo, dec);
		if (core->initDecoder(slot->descriptor, &memAlloc, debugLevel) < 0) {
			avLogError(debugLevel, "EnableDecoder", 1266, " Cannot supply details to decoder filter ");
			return -1;
		}
	}
	return 0;
}

int32_t AvController::enableStream(uint32_t streamIndex)
{
	int8_t curState = (int8_t)*stateMachine->getStatePtr();
	if (curState == AV_STATE_EOF || curState == AV_STATE_STOP) {
		avLogError(debugLevel, "EnableStream", 1276, " Cannot Enable stream, state in %d ", curState);
		return -1;
	}

	if (streamIndex < streamList.count) {
		AvStreamSlot *slot = streamList.at(streamIndex);
		if (slot->descriptor->streamType == SCE_AVPLAYER_AUDIO && activeAudioStream >= 0) {
			if (slot->auFifo == NULL) {
				int32_t trackOff = demux->getTrackOffset(streamIndex);
				if (enableAuInput(slot, (uint32_t)trackOff) < 0)
					return -1;
				avLogDebug(debugLevel, "EnableStream", 1287,
				           "Created a new AU input buffer for stream %d: 0x%x", streamIndex, slot->auFifo);
			}
			if (demux->setStreamFifo(streamIndex, slot->auFifo) < 0)
				return -1;
			if (slot->outputFifo == NULL && enableFrameOutput(slot) < 0)
				return -1;
			AvStreamSlot *activeSlot = streamList.at((uint32_t)activeAudioStream);
			activeSlot->coreDecoder->setInputAndDecoder(slot->auFifo, findDecoderForType(slot->descriptor->streamType));
		} else {
			int8_t savedState = (int8_t)*stateMachine->getStatePtr();
			if (savedState == AV_STATE_READY)
				stateMachine->setState(AV_STATE_TRANSITION);

			int32_t trackOff = demux->getTrackOffset(streamIndex);
			if (enableDecoder(slot, (uint32_t)trackOff) < 0) {
				avLogError(debugLevel, "EnableStream", 1305, " Cannot create & enable Stream:\t%d ", streamIndex);
				if (*stateMachine->getStatePtr() == AV_STATE_TRANSITION)
					stateMachine->setState(savedState);
				return -1;
			}
			demux->setStreamFifo(streamIndex, slot->auFifo);
			avLogDebug(debugLevel, "EnableStream", 1314, " Added & Enabled Stream:\t%d\t", streamIndex);
			if (*stateMachine->getStatePtr() == AV_STATE_TRANSITION)
				stateMachine->setState(savedState);
			if (slot->descriptor->streamType == SCE_AVPLAYER_AUDIO && activeAudioStream == -1)
				activeAudioStream = (int32_t)streamIndex;
		}

		sceKernelLockMutex(streamMutex, 1, NULL);
		slot->enabled = 1;
		slot->active = 1;
		sceKernelUnlockMutex(streamMutex, 1);
		avLogDebug(debugLevel, "EnableStream", 1330, " Enabled Stream:\t%d ", streamIndex);
		return 0;
	}

	int8_t savedState = (int8_t)*stateMachine->getStatePtr();
	if (savedState == AV_STATE_READY)
		stateMachine->setState(AV_STATE_TRANSITION);

	AvStreamSlot *newSlot = (AvStreamSlot *)avMspaceAlloc(1, sizeof(AvStreamSlot));
	if (newSlot == NULL) {
		avLogError(debugLevel, "EnableStream", 1338, " Failed to allocate memory");
		return -1;
	}
	newSlot->active = 1;
	newSlot->enabled = 1;
	newSlot->descriptor = streamDescList.at(streamIndex);

	int32_t trackOff = demux->getTrackOffset(streamIndex);
	if (enableDecoder(newSlot, (uint32_t)trackOff) < 0) {
		streamList.pushBack(newSlot);
		avLogError(debugLevel, "EnableStream", 1347, " Cannot create & enable Stream:\t%d ", streamIndex);
		if (*stateMachine->getStatePtr() == AV_STATE_TRANSITION)
			stateMachine->setState(savedState);
		return -1;
	}

	streamList.pushBack(newSlot);
	demux->setStreamFifo(streamIndex, newSlot->auFifo);
	avLogDebug(debugLevel, "EnableStream", 1356, " Added & Enabled Stream:\t%d\t", streamIndex);
	if (*stateMachine->getStatePtr() == AV_STATE_TRANSITION)
		stateMachine->setState(savedState);
	return 0;
}

int32_t AvController::disableStream(uint32_t streamIndex)
{
	if (streamIndex >= streamList.count) {
		avLogError(debugLevel, "DisableStream", 1376, "Stream Index not found");
		return -1;
	}
	sceKernelLockMutex(streamMutex, 1, NULL);
	streamList.at(streamIndex)->enabled = 0;
	avLogDebug(debugLevel, "DisableStream", 1370, "Disabled Stream:  %d ", streamIndex);
	sceKernelUnlockMutex(streamMutex, 1);
	demux->disableStream(streamIndex);
	return 0;
}

int32_t AvController::isStreamEnabled(uint32_t streamIndex)
{
	if (streamIndex >= streamList.count)
		return 0;
	return (int32_t)streamList.at(streamIndex)->enabled;
}

int32_t AvController::changeFlowState(int8_t nextState)
{
	int32_t publicEvent = 0;
	avLogDebug(debugLevel, "ChangeFlowState", 817, "State changed to %d ", nextState);
	switch (nextState) {
	case AV_STATE_ERROR:
		return stateMachine->setState(nextState);
	case AV_STATE_READY:
		publicEvent = SCE_AVPLAYER_STATE_READY;
		break;
	case AV_STATE_PLAY:
		publicEvent = SCE_AVPLAYER_STATE_PLAY;
		break;
	case AV_STATE_STOP:
		publicEvent = SCE_AVPLAYER_STATE_STOP;
		break;
	case AV_STATE_PAUSE:
		publicEvent = SCE_AVPLAYER_STATE_PAUSE;
		break;
	case AV_STATE_BUFFERING:
		publicEvent = SCE_AVPLAYER_STATE_BUFFERING;
		break;
	default:
		break;
	}

	int32_t ret = stateMachine->setState(nextState);
	if (ret == 0 && eventCb.eventCallback != NULL)
		eventCb.eventCallback(eventCb.objectPointer, publicEvent, sourceId, NULL);
	return ret;
}

int32_t AvController::stop()
{
	int8_t curState = (int8_t)*stateMachine->getStatePtr();
	if (curState == AV_STATE_UNINIT || curState == AV_STATE_INIT) {
		avLogDebug(debugLevel, "Stop", 1449, "Stop not required from state: %d ", curState);
		return -1;
	}

	avLogDebug(debugLevel, "Stop", 1452, " Waiting for Valid state to Stop");
	while (*stateMachine->getStatePtr() == AV_STATE_JUMP || *stateMachine->getStatePtr() == AV_STATE_TRICKMODE)
		sceKernelDelayThread(90);
	avLogDebug(debugLevel, "Stop", 1459, " Stopped waiting for Valid state to Stop");

	if (*stateMachine->getStatePtr() == AV_STATE_OPENING)
		stateMachine->stop();

	if (changeFlowState(AV_STATE_STOP) < 0) {
		avLogError(debugLevel, "Stop", 1466, " cannot perform stop");
		return -1;
	}

	for (uint32_t i = 0; i < streamList.count; i++) {
		AvStreamSlot *slot = streamList.at(i);
		if (slot->active == 1 &&
		    (slot->descriptor->streamType == SCE_AVPLAYER_VIDEO || slot->descriptor->streamType == SCE_AVPLAYER_AUDIO)) {
			if (slot->coreDecoder != NULL && slot->coreDecoder->stop() < 0) {
				avLogError(debugLevel, "Stop", 1477, " Failed to stop Decoder thread");
				return -1;
			}
			slot->currentFrame = NULL;
		}
	}

	if (demux->stop() < 0) {
		avLogError(debugLevel, "Stop", 1487, " Failed to stop Demux thread");
		return -1;
	}
	stateMachine->resetClock();
	return 0;
}

int32_t AvController::start()
{
	int8_t curState = (int8_t)*stateMachine->getStatePtr();
	if (curState != AV_STATE_READY && curState != AV_STATE_STOP) {
		if (stop() < 0)
			return -1;
	}

	stateMachine->setState(AV_STATE_TRANSITION);
	int32_t startedVideo = -1;
	int32_t startedAudio = -1;

	for (uint32_t i = 0; i < streamList.count; i++) {
		AvStreamSlot *slot = streamList.at(i);
		if (slot->active != 1 || slot->coreDecoder == NULL)
			continue;

		if (slot->descriptor->streamType == SCE_AVPLAYER_VIDEO && startedVideo < 0) {
			if (slot->coreDecoder->startVideo(basePriority + 5, "avPlayer VideoDec", highFrameRateFlag) < 0) {
				avLogError(debugLevel, "Start", 1396, "Video Decoder thread creation failed");
				stateMachine->setState(AV_STATE_ERROR);
				return -1;
			}
			startedVideo = 1;
			avLogDebug(debugLevel, "Start", 1401, " VideoDec thread started ");
		} else if (slot->descriptor->streamType == SCE_AVPLAYER_AUDIO && startedAudio < 0) {
			if (slot->coreDecoder->startAudio(basePriority + 6, "avPlayer AudioDec") < 0) {
				avLogError(debugLevel, "Start", 1411, "Audio Decoder thread creation failed");
				stateMachine->setState(AV_STATE_ERROR);
				return -1;
			}
			startedAudio = 1;
			avLogDebug(debugLevel, "Start", 1415, " AudioDec thread started ");
		}
	}

	int32_t ret = demux->start();
	if (ret < 0) {
		avLogError(debugLevel, "Start", 1424, "Demux thread creation failed");
		stateMachine->setState(AV_STATE_ERROR);
		return -1;
	}
	avLogDebug(debugLevel, "Start", 1428, " Demux thread started ");
	changeFlowState(AV_STATE_PLAY);
	return ret;
}

int32_t AvController::jumpInternal(uint64_t targetPts90k)
{
	for (uint32_t i = 0; i < streamList.count; i++) {
		uint64_t dur = streamDescList.at(i)->duration;
		if (dur != 0) {
			uint64_t maxPts = (dur * 90u) / 10u;
			if (targetPts90k > maxPts) {
				avLogWarn(debugLevel, "Jump", 615,
				          "Cannot Jump to %lld as max duration is %lld ", targetPts90k, maxPts);
				return -1;
			}
		}
	}

	int8_t prevState = (int8_t)*stateMachine->getStatePtr();
	if (stateMachine->postUserEvent(AV_EVENT_JUMP, (int64_t)targetPts90k) < 0)
		return -1;
	while ((int8_t)*stateMachine->getStatePtr() == prevState)
		sceKernelDelayThread(100);
	return 0;
}

int32_t AvController::pause()
{
	int8_t curState = (int8_t)*stateMachine->getStatePtr();
	if (curState != AV_STATE_PLAY && curState != AV_STATE_EOF && curState != AV_STATE_BUFFERING) {
		if (curState == AV_STATE_PAUSE) {
			avLogDebug(debugLevel, "Pause", 734, " Already in PAUSE State, need to RESUME ");
			return -1;
		}
		avLogDebug(debugLevel, "Pause", 737, " Invalid state %d to change to Pause ", curState);
		if (curState == AV_STATE_READY || curState == AV_STATE_INIT ||
		    curState == AV_STATE_UNINIT || curState == AV_STATE_OPENING) {
			return -1;
		}
	}

	avLogDebug(debugLevel, "Pause", 747, " Waiting for Valid state to Pause");
	while (*stateMachine->getStatePtr() == AV_STATE_JUMP || *stateMachine->getStatePtr() == AV_STATE_TRICKMODE)
		sceKernelDelayThread(90);
	avLogDebug(debugLevel, "Pause", 753, " Stopped waiting for Valid state to Pause");

	if (playbackMode != AV_PLAYMODE_NORMAL) {
		avLogError(debugLevel, "Pause", 757, " Cannot Pause in Trickmode playback state %d ", playbackMode);
		return -1;
	}

	avLogDebug(debugLevel, "Pause", 761, " playback Paused ");
	if (*stateMachine->getStatePtr() == AV_STATE_EOF)
		return changeFlowState(AV_STATE_EOF_PAUSE);
	return changeFlowState(AV_STATE_PAUSE);
}

int32_t AvController::resume()
{
	for (;;) {
		int8_t curState = (int8_t)*stateMachine->getStatePtr();
		if (curState == AV_STATE_INIT || curState == AV_STATE_STOP || curState == AV_STATE_READY ||
		    curState == AV_STATE_OPENING || curState == AV_STATE_ERROR) {
			avLogWarn(debugLevel, "Resume", 778, "Invalid State: %d ", curState);
			return -1;
		}

		avLogDebug(debugLevel, "Resume", 781, " Waiting for Valid state to Resume");
		while (*stateMachine->getStatePtr() == AV_STATE_TRANSITION ||
		       *stateMachine->getStatePtr() == AV_STATE_JUMP ||
		       *stateMachine->getStatePtr() == AV_STATE_TRICKMODE) {
			sceKernelDelayThread(90);
		}
		avLogDebug(debugLevel, "Resume", 787, " Stopped waiting for Valid state to Resume");

		curState = (int8_t)*stateMachine->getStatePtr();
		if (curState == AV_STATE_BUFFERING || curState == AV_STATE_PAUSE || curState == AV_STATE_JUMP_PAUSE) {
			stateMachine->resumeClock();
			avLogDebug(debugLevel, "Resume", 793, " playback Resumed ");
			return changeFlowState(AV_STATE_PLAY);
		}
		if (curState == AV_STATE_EOF_PAUSE) {
			stateMachine->resumeClock();
			avLogDebug(debugLevel, "Resume", 797, " playback Resumed ");
			return changeFlowState(AV_STATE_EOF);
		}
		if (curState == AV_STATE_PLAY) {
			avLogDebug(debugLevel, "Resume", 800, " playback already Resumed ");
			return 0;
		}
		if (curState == AV_STATE_EOF) {
			avLogDebug(debugLevel, "Resume", 803, " EOF already reached ");
			return 0;
		}
		avLogError(debugLevel, "Resume", 807, " cannot peform Resume, in state: %d ", curState);
	}
}

int32_t AvController::sendTimedTextEvent(uint64_t videoTimeMs)
{
	int32_t textStreamIdx = -1;
	for (uint32_t i = 0; i < streamList.count; i++) {
		AvStreamSlot *slot = streamList.at(i);
		if (slot->active == 1 && slot->descriptor->streamType == SCE_AVPLAYER_TIMEDTEXT && slot->enabled != 0)
			textStreamIdx = (int32_t)i;
	}

	if (textStreamIdx < 0)
		return 0;

	AvStreamSlot *slot = streamList.at((uint32_t)textStreamIdx);
	if (slot->auFifo != NULL && slot->auFifo->getCount() > 0) {
		AvAccessUnit *au = slot->auFifo->peekAu();
		uint64_t textTimeMs = au->pts / 90u;
		if (textTimeMs <= videoTimeMs) {
			if (au->isEmpty != 0) {
				slot->auFifo->popAndFreeAu(au);
			} else {
				SceAvPlayerFrameInfo textFrame;
				sceClibMemset(&textFrame, 0, sizeof(textFrame));
				uint32_t allocSize = (au->size + 7u) & ~3u;
				uint8_t *textBuf = (uint8_t *)avMspaceAlloc(4, allocSize);
				sceClibMemset(textBuf, 0, allocSize);
				sceClibMemcpy_safe(textBuf, au->buffer, au->size);
				textBuf[au->size] = 0;

				textFrame.timeStamp = textTimeMs;
				AvStreamDescriptor *desc = streamDescList.at((uint32_t)textStreamIdx);
				textFrame.details.subs.fontSize = desc->fontSize;
				for (int i = 0; i < 4; i++)
					textFrame.details.subs.languageCode[i] = desc->languageCode[i];

				uint16_t textLen = (uint16_t)(((uint16_t)textBuf[0] << 8) | textBuf[1]);
				if (textLen != 0) {
					textFrame.details.subs.textSize = textLen;
					textFrame.pData = textBuf + 2;
				} else {
					textFrame.details.subs.textSize = 0;
					textFrame.pData = NULL;
				}

				avLogDebug(debugLevel, "SendTimedTextEvent", 1579,
				           "Sending Timed-text event: videoTS[%lld] TextTS[%lld], textsize[%d] ",
				           videoTimeMs, textTimeMs, textLen);
				avLogDebug(debugLevel, "SendTimedTextEvent", 1580, "TextSent: \" %s \"", (const char *)(textBuf + 2));
				if (eventCb.eventCallback != NULL)
					eventCb.eventCallback(eventCb.objectPointer, SCE_AVPLAYER_TIMED_TEXT_DELIVERY, 0, &textFrame);

				slot->auFifo->popAndFreeAu(au);
				avMspaceFree(textBuf);
			}
		}
	}
	return 1;
}

int32_t AvController::fetchFrame(int32_t streamType, uint32_t streamIndex, SceAvPlayerFrameInfo *outFrame)
{
	uint8_t fetched = 0;
	AvStreamSlot *slot = streamList.at(streamIndex);
	if (slot->outputFifo == NULL || slot->outputFifo->getReadyCount() <= 1)
		return 0;

	AvDecodedFrame *peeked = slot->outputFifo->peekOrPopReady(0);
	if (peeked == NULL)
		return 0;

	if (*stateMachine->getStatePtr() == AV_STATE_JUMP_PAUSE) {
		if (slot->enabled == 1 && peeked->resetFlag == 2) {
			avLogDebug(debugLevel, "FetchFrame", 1998,
			           " Jump-Pause frame: %lld, reset: %d ", peeked->pts, 2);
			stateMachine->postUserEvent(AV_EVENT_THUMBNAIL_DONE, 0);
			fetched = 1;
		}
	} else if (peeked->resetFlag == 1) {
		peeked->resetFlag = 0;
		fetched = 1;
	}

	if (fetched == 1) {
		if (slot->currentFrame != NULL) {
			uint32_t forceFlag = (*stateMachine->getStatePtr() == AV_STATE_EOF) ? 1u : 0u;
			AvDecodedFrame *released = slot->outputFifo->releaseOldest(forceFlag);
			if (released != NULL)
				avMspaceFree(released);
			slot->currentFrame = NULL;
		}
		slot->currentFrame = slot->outputFifo->peekOrPopReady(1);
		if (slot->currentFrame == NULL)
			return 0;

		AvStreamDescriptor *desc = streamDescList.at(streamIndex);
		if (streamType == SCE_AVPLAYER_VIDEO) {
			stateMachine->updateVideoPts(slot->currentFrame->pts);
			outFrame->pData = slot->currentFrame->buffer;
			outFrame->timeStamp = slot->currentFrame->pts / 90u;
			outFrame->details.video.width = slot->currentFrame->width;
			outFrame->details.video.height = slot->currentFrame->heightOrSize;
			outFrame->details.video.aspectRatio = (float)((double)desc->aspectRatioScaled / 1000000.0);
			for (int i = 0; i < 4; i++)
				outFrame->details.video.languageCode[i] = desc->languageCode[i];
			sendTimedTextEvent(outFrame->timeStamp);
		} else if (streamType == SCE_AVPLAYER_AUDIO) {
			stateMachine->updateAudioPts(slot->currentFrame->pts);
			outFrame->pData = slot->currentFrame->buffer + slot->currentFrame->cropLeftOrReadOffset;
			uint32_t chunkSize = (slot->descriptor->channelCount > 2)
			                     ? ((uint32_t)slot->descriptor->channelCount << 12) : 4096u;
			slot->currentFrame->cropLeftOrReadOffset += chunkSize;
			outFrame->timeStamp = slot->currentFrame->pts / 90u;
			outFrame->details.audio.sampleRate = slot->descriptor->sampleRate;
			outFrame->details.audio.channelCount = slot->descriptor->channelCount;
			outFrame->details.audio.size = chunkSize;
			for (int i = 0; i < 4; i++)
				outFrame->details.audio.languageCode[i] = desc->languageCode[i];
			uint32_t totalBytes = slot->currentFrame->heightOrSize;
			if (totalBytes != chunkSize) {
				if (totalBytes > chunkSize)
					outFrame->details.audio.channelCount = 2;
				else
					outFrame->details.audio.channelCount = 1;
			}
		}
	} else {
		AvDecodedFrame *dropped = slot->outputFifo->popReady();
		if (dropped != NULL)
			avMspaceFree(dropped);
	}
	return (int32_t)fetched;
}

int32_t AvController::syncLoopAudio(uint32_t streamIndex, SceAvPlayerFrameInfo *outFrame)
{
	sceKernelLockMutex(streamMutex, 1, NULL);
	if (loopBothReady == 1) {
		loopBothReady = 0;
		sceKernelUnlockMutex(streamMutex, 1);
		return fetchFrame(SCE_AVPLAYER_AUDIO, streamIndex, outFrame);
	}
	if (videoLoopPending == 1) {
		stateMachine->resumeClock();
		avAtomicStore32(&loopBothReady, 1);
		avAtomicStore32(&audioLoopPending, 0);
		avAtomicStore32(&videoLoopPending, 0);
		int32_t warnCode = SCE_AVPLAYER_WAR_LOOPING_BACK;
		if (eventCb.eventCallback != NULL)
			eventCb.eventCallback(eventCb.objectPointer, SCE_AVPLAYER_WARNING_ID, sourceId, &warnCode);
		sceKernelUnlockMutex(streamMutex, 1);
		return fetchFrame(SCE_AVPLAYER_AUDIO, streamIndex, outFrame);
	}
	avAtomicStore32(&audioLoopPending, 1);
	sceKernelUnlockMutex(streamMutex, 1);
	return 0;
}

void AvController::drainInactiveAudio(uint32_t *activeAudioIdx, uint32_t *enabledAudioIdx)
{
	sceKernelLockMutex(streamMutex, 1, NULL);
	for (uint32_t i = 0; i < streamList.count; i++) {
		if (*activeAudioIdx == i)
			continue;
		AvStreamSlot *slot = streamList.at(i);
		if (slot->active != 1 || slot->descriptor->streamType != SCE_AVPLAYER_AUDIO)
			continue;

		if (slot->outputFifo != NULL && slot->outputFifo->getReadyCount() > 0) {
			if (slot->currentFrame != NULL) {
				AvDecodedFrame *released = slot->outputFifo->releaseOldest(0);
				if (released != NULL)
					avMspaceFree(released);
				slot->currentFrame = NULL;
			}
			slot->currentFrame = slot->outputFifo->peekOrPopReady(1);
		}

		if (slot->auFifo != NULL && slot->auFifo->getCount() > 0) {
			AvAccessUnit *au = slot->auFifo->peekAu();
			uint64_t mediaNow = stateMachine->getCurrentMediaTime90k();
			int32_t enabledEmpty = 0;
			if ((int32_t)*enabledAudioIdx >= 0) {
				AvStreamSlot *enSlot = streamList.at(*enabledAudioIdx);
				if (enSlot->auFifo != NULL && enSlot->auFifo->getCount() == 0)
					enabledEmpty = 1;
			}
			if (au != NULL && (au->pts < mediaNow || enabledEmpty != 0)) {
				slot->auFifo->popAndFreeAu(au);
				avMspaceFree(au);
			}
		}
	}
	sceKernelUnlockMutex(streamMutex, 1);
}

int32_t AvController::getAudioData(SceAvPlayerFrameInfo *outFrame)
{
	int32_t result = 0;
	int8_t curState = (int8_t)*stateMachine->getStatePtr();
	if (curState != AV_STATE_PLAY && curState != AV_STATE_EOF)
		return 0;

	int32_t enabledAudioIdx = -1;
	int32_t firstAudioIdx = -1;
	int32_t hasVideo = 0;

	for (uint32_t i = 0; i < streamList.count; i++) {
		AvStreamSlot *slot = streamList.at(i);
		if (slot->active != 1)
			continue;
		if (slot->descriptor->streamType == SCE_AVPLAYER_AUDIO) {
			if (firstAudioIdx < 0)
				firstAudioIdx = (int32_t)i;
			if (enabledAudioIdx < 0 && slot->enabled != 0)
				enabledAudioIdx = (int32_t)i;
		} else if (slot->descriptor->streamType == SCE_AVPLAYER_VIDEO) {
			hasVideo = 1;
		}
	}

	if (firstAudioIdx < 0 && enabledAudioIdx < 0)
		return 0;

	uint32_t activeIdx = (enabledAudioIdx >= 0) ? (uint32_t)enabledAudioIdx : (uint32_t)firstAudioIdx;
	uint32_t enabledArg = (uint32_t)enabledAudioIdx;
	drainInactiveAudio(&activeIdx, &enabledArg);

	uint32_t targetIdx = (enabledAudioIdx >= 0) ? (uint32_t)enabledAudioIdx : (uint32_t)firstAudioIdx;
	AvStreamSlot *slot = streamList.at(targetIdx);

	if (slot->enabled == 1 && hasVideo != 0 && slot->outputFifo != NULL) {
		AvDecodedFrame *peeked = slot->outputFifo->peekOrPopReady(0);
		if (peeked != NULL && peeked->resetFlag == 1)
			return syncLoopAudio(targetIdx, outFrame);
	}

	if (slot->currentFrame == NULL) {
		slot->currentFrame = slot->outputFifo->peekOrPopReady(1);
	} else if (slot->currentFrame->cropLeftOrReadOffset >= slot->currentFrame->heightOrSize) {
		AvDecodedFrame *released = slot->outputFifo->releaseOldest(0);
		if (released != NULL)
			avMspaceFree(released);
		slot->currentFrame = slot->outputFifo->peekOrPopReady(1);
	}

	if (slot->currentFrame == NULL)
		return 0;

	stateMachine->updateAudioPts(slot->currentFrame->pts);
	outFrame->pData = slot->currentFrame->buffer + slot->currentFrame->cropLeftOrReadOffset;
	uint32_t chunkSize = (slot->descriptor->channelCount > 2)
	                     ? ((uint32_t)slot->descriptor->channelCount << 12) : 4096u;
	slot->currentFrame->cropLeftOrReadOffset += chunkSize;
	outFrame->timeStamp = slot->currentFrame->pts / 90u;
	outFrame->details.audio.sampleRate = slot->descriptor->sampleRate;
	outFrame->details.audio.channelCount = slot->descriptor->channelCount;
	outFrame->details.audio.size = chunkSize;

	AvStreamDescriptor *desc = streamDescList.at(targetIdx);
	for (int i = 0; i < 4; i++)
		outFrame->details.audio.languageCode[i] = desc->languageCode[i];

	uint32_t totalBytes = slot->currentFrame->heightOrSize;
	if (totalBytes != chunkSize) {
		if (totalBytes > chunkSize)
			outFrame->details.audio.channelCount = 2;
		else
			outFrame->details.audio.channelCount = 1;
	}

	if (slot->currentFrame->resetFlag == 1) {
		result = syncLoopAudio(targetIdx, outFrame);
		if (result == 1 && slot->currentFrame != NULL)
			slot->currentFrame->resetFlag = 0;
		return result;
	}

	if (slot->enabled != 0)
		result = 1;
	if (slot->descriptor->channelCount > 2 && chunkSize != 0)
		outFrame->details.audio.sampleRate *= (totalBytes / chunkSize);
	if (playbackMode != AV_PLAYMODE_NORMAL)
		return 0;
	if (hasVideo == 0 && result != 0 && jumpCompletePending != 0) {
		jumpCompletePending = 0;
		stateMachine->postUserEvent(AV_EVENT_JUMP_DONE_WARN, 0);
	}
	return result;
}

int32_t AvController::syncLoopVideo(uint32_t streamIndex, SceAvPlayerFrameInfo *outFrame)
{
	sceKernelLockMutex(streamMutex, 1, NULL);
	if (loopBothReady == 1) {
		loopBothReady = 0;
		sceKernelUnlockMutex(streamMutex, 1);
		return fetchFrame(SCE_AVPLAYER_VIDEO, streamIndex, outFrame);
	}
	if (audioLoopPending == 1) {
		stateMachine->resumeClock();
		avAtomicStore32(&loopBothReady, 1);
		avAtomicStore32(&audioLoopPending, 0);
		avAtomicStore32(&videoLoopPending, 0);
		int32_t warnCode = SCE_AVPLAYER_WAR_LOOPING_BACK;
		if (eventCb.eventCallback != NULL)
			eventCb.eventCallback(eventCb.objectPointer, SCE_AVPLAYER_WARNING_ID, sourceId, &warnCode);
		sceKernelUnlockMutex(streamMutex, 1);
		return fetchFrame(SCE_AVPLAYER_VIDEO, streamIndex, outFrame);
	}
	avAtomicStore32(&videoLoopPending, 1);
	sceKernelUnlockMutex(streamMutex, 1);
	return 0;
}

int32_t AvController::getVideoData(SceAvPlayerFrameInfo *outFrame)
{
	int8_t curState = (int8_t)*stateMachine->getStatePtr();
	if (curState != AV_STATE_PLAY && curState != AV_STATE_EOF && curState != AV_STATE_JUMP_PAUSE)
		return 0;

	int32_t videoIdx = -1;
	int32_t enabledVideoIdx = -1;
	int32_t hasEnabledAudio = 0;

	for (uint32_t i = 0; i < streamList.count; i++) {
		AvStreamSlot *slot = streamList.at(i);
		if (slot->active != 1)
			continue;
		if (slot->descriptor->streamType == SCE_AVPLAYER_VIDEO) {
			videoIdx = (int32_t)i;
			if (slot->enabled != 0)
				enabledVideoIdx = (int32_t)i;
		} else if (slot->descriptor->streamType == SCE_AVPLAYER_AUDIO && slot->enabled != 0) {
			hasEnabledAudio = 1;
		}
	}

	if (videoIdx < 0 && enabledVideoIdx < 0)
		return 0;

	AvStreamSlot *slot = streamList.at((uint32_t)videoIdx);
	int32_t syncResult = stateMachine->evaluateVideoSync(slot->currentFrame != NULL ? slot->currentFrame->pts : 0);
	if (syncResult == AV_CLOCK_SHOW_NOW)
		clockSynced = 1;

	if (hasEnabledAudio == 0 && slot->currentFrame != NULL) {
		stateMachine->updateAudioPts(slot->currentFrame->pts);
		if (slot->currentFrame->resetFlag == 1) {
			int32_t warnCode = SCE_AVPLAYER_WAR_LOOPING_BACK;
			if (eventCb.eventCallback != NULL)
				eventCb.eventCallback(eventCb.objectPointer, SCE_AVPLAYER_WARNING_ID, sourceId, &warnCode);
		}
	}

	int32_t gotFrame = 0;
	if (hasEnabledAudio == 1 && slot->outputFifo != NULL) {
		AvDecodedFrame *peeked = slot->outputFifo->peekOrPopReady(0);
		if (peeked != NULL && peeked->resetFlag == 1) {
			gotFrame = syncLoopVideo((uint32_t)videoIdx, outFrame);
			if (gotFrame == 1)
				peeked->resetFlag = 0;
		}
	}

	if (*stateMachine->getStatePtr() == AV_STATE_JUMP_PAUSE) {
		int32_t fetched = fetchFrame(SCE_AVPLAYER_VIDEO, (uint32_t)videoIdx, outFrame);
		if (fetched != 0) {
			jumpCompletePending = 0;
			stateMachine->postUserEvent(AV_EVENT_JUMP_DONE_WARN, 0);
		}
		return fetched;
	}

	bool shouldDeliver = (jumpCompletePending != 0) ||
	                     (syncResult == AV_CLOCK_SHOW_NOW) ||
	                     (syncResult == AV_CLOCK_AUDIO_AHEAD) ||
	                     (syncResult == AV_CLOCK_NO_AUDIO);

	if (shouldDeliver) {
		if (slot->currentFrame != NULL) {
			uint32_t forceFlag = (*stateMachine->getStatePtr() == AV_STATE_EOF) ? 1u : 0u;
			AvDecodedFrame *released = slot->outputFifo->releaseOldest(forceFlag);
			if (released != NULL)
				avMspaceFree(released);
			slot->currentFrame = NULL;
		}
		if (slot->outputFifo->getReadyCount() <= 0) {
			sendTimedTextEvent(stateMachine->getCurrentMediaTimeMs());
			return gotFrame;
		}
		slot->currentFrame = slot->outputFifo->peekOrPopReady(1);
		if (slot->currentFrame == NULL)
			return 0;

		stateMachine->updateVideoPts(slot->currentFrame->pts);
		outFrame->pData = slot->currentFrame->buffer;
		outFrame->timeStamp = slot->currentFrame->pts / 90u;
		outFrame->details.video.width = slot->currentFrame->width;
		outFrame->details.video.height = slot->currentFrame->heightOrSize;
		AvStreamDescriptor *desc = streamDescList.at((uint32_t)videoIdx);
		outFrame->details.video.aspectRatio = (float)((double)desc->aspectRatioScaled / 1000000.0);
		for (int i = 0; i < 4; i++)
			outFrame->details.video.languageCode[i] = desc->languageCode[i];
		if (slot->enabled == 1)
			gotFrame = 1;
		sendTimedTextEvent(outFrame->timeStamp);
		if (jumpCompletePending != 0) {
			jumpCompletePending = 0;
			stateMachine->postUserEvent(AV_EVENT_JUMP_DONE_WARN, 0);
		}
	} else if (syncResult == AV_CLOCK_DROP_LATE) {
		if (hasEnabledAudio == 1 && clockSynced == 0)
			return 0;
		if (slot->outputFifo->getReadyCount() > 0 && *stateMachine->getStatePtr() == AV_STATE_EOF) {
			slot->currentFrame = slot->outputFifo->popReadyNoWait();
			if (slot->currentFrame != NULL && slot->currentFrame->resetFlag != 0)
				return 0;
			uint32_t forceFlag = (*stateMachine->getStatePtr() == AV_STATE_EOF) ? 1u : 0u;
			AvDecodedFrame *dropped = slot->outputFifo->releaseOldest(forceFlag);
			if (dropped != NULL) {
				stateMachine->updateVideoPts(dropped->pts);
				avMspaceFree(dropped);
			}
		}
	} else if (syncResult != AV_CLOCK_WAIT) {
		avLogError(debugLevel, "GetVideoData", 1977, "SCE_AVBASECLOCK_default ");
	}
	return gotFrame;
}

int32_t AvController::getVideoDataEx(SceAvPlayerFrameInfoEx *outFrame)
{
	SceAvPlayerFrameInfo baseInfo;
	sceClibMemset(&baseInfo, 0, sizeof(baseInfo));
	int32_t got = getVideoData(&baseInfo);

	int32_t videoIdx = -1;
	for (uint32_t i = 0; i < streamList.count; i++) {
		AvStreamSlot *slot = streamList.at(i);
		if (slot->descriptor->streamType == SCE_AVPLAYER_VIDEO && slot->enabled != 0)
			videoIdx = (int32_t)i;
	}

	if (got == 1) {
		outFrame->pData = baseInfo.pData;
		outFrame->timeStamp = baseInfo.timeStamp;
		outFrame->details = baseInfo.details;
		if (videoIdx >= 0) {
			AvStreamSlot *slot = streamList.at((uint32_t)videoIdx);
			if (slot->currentFrame != NULL) {
				outFrame->cropLeftOffset = slot->currentFrame->cropLeftOrReadOffset;
				outFrame->cropRightOffset = slot->currentFrame->cropRightOffset;
				outFrame->cropTopOffset = slot->currentFrame->cropTopOffset;
				outFrame->cropBottomOffset = slot->currentFrame->cropBottomOffset;
			}
		}
	}
	return got;
}

int32_t AvController::isActive()
{
	int32_t videoReady = 0;
	int32_t audioReady = 0;
	int32_t totalAu = 0;

	for (uint32_t i = 0; i < streamList.count; i++) {
		AvStreamSlot *slot = streamList.at(i);
		if (slot->enabled == 0)
			continue;
		if (slot->descriptor->streamType == SCE_AVPLAYER_VIDEO && slot->outputFifo != NULL)
			videoReady = slot->outputFifo->getReadyCount();
		if (slot->descriptor->streamType == SCE_AVPLAYER_AUDIO && slot->outputFifo != NULL)
			audioReady = slot->outputFifo->getReadyCount();
		if (slot->auFifo != NULL)
			totalAu += slot->auFifo->getCount();
	}

	int8_t curState = (int8_t)*stateMachine->getStatePtr();
	return ((curState != AV_STATE_EOF || totalAu > 0 || audioReady > 2 || videoReady > numOutputVideoBuffers) &&
	        curState != AV_STATE_STOP &&
	        curState != AV_STATE_ERROR) ? 1 : 0;
}

int32_t AvController::isEof()
{
	return ((int8_t)*stateMachine->getStatePtr() == AV_STATE_EOF) ? 1 : 0;
}

int32_t AvController::finalize()
{
	stop();
	if (stateMachine->stop() < 0)
		avLogError(debugLevel, "Finalize", 2135, "Cannot stop AvState controller");

	if (demux != NULL) {
		for (uint32_t i = 0; i < streamList.count; i++) {
			AvStreamSlot *slot = streamList.at(i);
			slot->enabled = 0;
			if (slot->active == 1 &&
			    (slot->descriptor->streamType == SCE_AVPLAYER_AUDIO || slot->descriptor->streamType == SCE_AVPLAYER_VIDEO)) {
				if (slot->descriptor->streamType == SCE_AVPLAYER_AUDIO && activeAudioStream >= 0) {
					AvStreamSlot *activeSlot = streamList.at((uint32_t)activeAudioStream);
					activeSlot->coreDecoder->setInputAndDecoder(slot->auFifo, findDecoderForType(slot->descriptor->streamType));
				}
				if (slot->coreDecoder != NULL && slot->coreDecoder->shutdown() < 0)
					avLogError(debugLevel, "Finalize", 2152, " Failed to Shutdown Decoder");
			}
		}

		decoderList.clear();

		if (demux->shutdown() < 0)
			avLogError(debugLevel, "Finalize", 2164, " Failed to Shutdown Demux");

		for (uint32_t i = 0; i < streamList.count; i++) {
			AvStreamSlot *slot = streamList.at(i);
			if (slot->outputBufferMem != NULL) {
				if (slot->descriptor->streamType == SCE_AVPLAYER_VIDEO)
					avGraphicsFree(slot->outputBufferMem);
				else if (slot->descriptor->streamType == SCE_AVPLAYER_AUDIO)
					avMemFree(slot->outputBufferMem);
				slot->outputBufferMem = NULL;
			}
			if (slot->active == 1) {
				if ((slot->descriptor->streamType == SCE_AVPLAYER_AUDIO || slot->descriptor->streamType == SCE_AVPLAYER_VIDEO) &&
				    slot->outputFifo != NULL) {
					slot->outputFifo->freeQueue.destroy();
					slot->outputFifo->readyQueue.destroy();
					avMspaceFree(slot->outputFifo);
					slot->outputFifo = NULL;
				}
				if (slot->auFifo != NULL) {
					slot->auFifo->destroy();
					sceClibMspaceDestroy(slot->auFifo->mspace);
					slot->auFifo->queue.destroy();
					avMspaceFree(slot->auFifo);
					slot->auFifo = NULL;
				}
				if ((slot->descriptor->streamType == SCE_AVPLAYER_AUDIO || slot->descriptor->streamType == SCE_AVPLAYER_VIDEO) &&
				    slot->coreDecoder != NULL) {
					sceKernelDeleteMutex(slot->coreDecoder->lockMutex);
					slot->coreDecoder->eventQueue.destroy();
					avMspaceFree(slot->coreDecoder);
					slot->coreDecoder = NULL;
				}
				slot->active = 0;
			}
			if (slot->auBufferMem != NULL) {
				avMemFree(slot->auBufferMem);
				slot->auBufferMem = NULL;
			}
			avMspaceFree(slot);
		}
		streamList.clear();

		demux->eventQueue.clear();
		for (uint32_t i = 0; i < demux->streamDescList.count; i++)
			avMspaceFree(demux->streamDescList.at(i));
		demux->streamDescList.destroy();
		demux->trackEnableList.destroy();
		demux->auFifoList.destroy();
		demux->eventQueue.destroy();
		avMspaceFree(demux);
		demux = NULL;

		for (uint32_t i = 0; i < streamDescList.count; i++)
			avMspaceFree(streamDescList.at(i));
		streamDescList.clear();
	}

	if (stateMachine != NULL) {
		stateMachine->shutdown();
		sceKernelDeleteLwMutex(&stateMachine->eventMutex);
		stateMachine->ackEventQueue.destroy();
		stateMachine->userEventQueue.destroy();
		avMspaceFree(stateMachine);
		stateMachine = NULL;
	}

	if (g_avMp4Preloaded == 0 && sceSysmoduleIsLoaded(SCE_SYSMODULE_MP4) == 0) {
		if (sceSysmoduleUnloadModule(SCE_SYSMODULE_MP4) < 0) {
			avLogError(debugLevel, "Finalize", 2225, "Unloading MP4 sys module failed\n");
			return -1;
		}
	}
	return 0;
}

int32_t AvController::setLooping(uint8_t loopFlag)
{
	avLogDebug(debugLevel, "SetLoopingFlag", 2086, " Looping set for media");
	loopingEnabled = loopFlag;
	return (int32_t)loopFlag;
}

uint64_t AvController::getCurrentTime()
{
	if (streamList.count == 0)
		return 0;
	for (uint32_t i = 0; i < streamList.count; i++) {
		AvStreamSlot *slot = streamList.at(i);
		if (slot->descriptor->streamType == SCE_AVPLAYER_VIDEO && slot->enabled == 1)
			return stateMachine->getVideoPts() / 90u;
	}
	return stateMachine->getAudioPts() / 90u;
}

int32_t AvController::classifyTrickSpeed(int32_t speed, int32_t *outMode, int32_t *outClampedSpeed, uint8_t *outNeedJump)
{
	int32_t mode = AV_PLAYMODE_NORMAL;
	if (speed > 100)
		mode = AV_PLAYMODE_FF;
	else if (speed <= -200)
		mode = AV_PLAYMODE_RW;

	*outMode = mode;
	if (speed > 3200)
		speed = 3200;
	else if (speed < -3200)
		speed = -3200;
	*outClampedSpeed = speed;

	int32_t curMode = playbackMode;
	if ((mode == AV_PLAYMODE_NORMAL && (curMode == AV_PLAYMODE_FF || curMode == AV_PLAYMODE_RW)) ||
	    (curMode == AV_PLAYMODE_NORMAL && (mode == AV_PLAYMODE_FF || mode == AV_PLAYMODE_RW)) ||
	    (mode == AV_PLAYMODE_SLOW && (curMode == AV_PLAYMODE_FF || curMode == AV_PLAYMODE_RW)) ||
	    (curMode == AV_PLAYMODE_SLOW && (mode == AV_PLAYMODE_FF || mode == AV_PLAYMODE_RW)) ||
	    (mode == AV_PLAYMODE_FF && curMode == AV_PLAYMODE_RW) ||
	    (mode == AV_PLAYMODE_RW && curMode == AV_PLAYMODE_FF)) {
		*outNeedJump = 1;
	} else {
		*outNeedJump = 0;
	}
	return 0;
}

int32_t AvController::setTrickSpeed(int32_t speed)
{
	int8_t curState = (int8_t)*stateMachine->getStatePtr();
	if (curState != AV_STATE_PLAY && curState != AV_STATE_BUFFERING) {
		avLogWarn(debugLevel, "SetTrickSpeed", 2259, " Cannot perform TrickSpeed, %d ", curState);
		return -1;
	}
	if (demux->getSourceType() != 1) {
		avLogWarn(debugLevel, "SetTrickSpeed", 2264, " Jump not possible on this source type");
		return -1;
	}
	if ((uint32_t)speed == trickSpeed)
		return 0;

	int32_t nextMode = AV_PLAYMODE_NORMAL;
	int32_t clampedSpeed = 100;
	uint8_t needJump = 0;
	if (classifyTrickSpeed(speed, &nextMode, &clampedSpeed, &needJump) < 0) {
		avLogError(debugLevel, "SetTrickSpeed", 2275, " Problem with input parameters ");
		return -1;
	}

	if (stateMachine->postUserEvent(AV_EVENT_TRICKMODE, (int64_t)nextMode) < 0) {
		avLogWarn(debugLevel, "SetTrickSpeed", 2280, "Failed to accept USER Event ");
		return -1;
	}

	int32_t prevSpeed = stateMachine->getPlaybackSpeed();
	stateMachine->setPlaybackSpeed(clampedSpeed);

	for (uint32_t i = 0; i < streamList.count; i++) {
		AvStreamSlot *slot = streamList.at(i);
		if (slot->coreDecoder != NULL)
			slot->coreDecoder->setPlaybackMode(nextMode);
	}

	if (needJump != 0) {
		uint64_t nextVideoPts = 0;
		uint64_t curPts90k = getCurrentTime() * 90u;
		for (uint32_t i = 0; i < streamList.count; i++) {
			AvStreamSlot *slot = streamList.at(i);
			if (slot->descriptor->streamType == SCE_AVPLAYER_VIDEO && slot->enabled == 1 && slot->auFifo != NULL) {
				AvAccessUnit *au = slot->auFifo->peekAu();
				if (au != NULL)
					nextVideoPts = au->pts;
			}
		}
		avLogWarn(debugLevel, "SetTrickSpeed", 2308, "%lld = %lld", curPts90k, nextVideoPts);
		if (nextVideoPts > curPts90k)
			curPts90k = nextVideoPts;
		avLogWarn(debugLevel, "SetTrickSpeed", 2313, " Jump to %lld ", curPts90k);
		if (jumpInternal(curPts90k) < 0) {
			avLogError(debugLevel, "SetTrickSpeed", 2316,
			           " Cannot perform Jump to next Sync frame, revert back all alterations ");
			for (uint32_t i = 0; i < streamList.count; i++) {
				AvStreamSlot *slot = streamList.at(i);
				if (slot->coreDecoder != NULL)
					slot->coreDecoder->setPlaybackMode(playbackMode);
			}
			stateMachine->setPlaybackSpeed(prevSpeed);
			return -1;
		}
	}

	avAtomicStore32(&trickSpeed, (uint32_t)clampedSpeed);
	playbackMode = nextMode;
	avLogDebug(debugLevel, "SetTrickSpeed", 2331, " Playmode:  %d , Speed: %d ", nextMode, clampedSpeed);
	return 0;
}

int32_t AvController::jumpToTime(int64_t timeMs)
{
	int8_t curState = (int8_t)*stateMachine->getStatePtr();
	avLogDebug(debugLevel, "JumpToTime", 637, " jumptime: %lld, state %d ", timeMs, curState);

	if (curState != AV_STATE_PLAY && curState != AV_STATE_BUFFERING && curState != AV_STATE_PAUSE) {
		if (curState == AV_STATE_JUMP)
			avLogWarn(debugLevel, "JumpToTime", 643, " Jump attempted while still in jump State: %d ", curState);
		else if (curState == AV_STATE_JUMP_PAUSE)
			avLogWarn(debugLevel, "JumpToTime", 646, " Jump attempted while waiting for Pause-jump State: %d to complete", curState);
		else
			avLogError(debugLevel, "JumpToTime", 649, " Jump cannot perform in this State: %d ", curState);
		return -1;
	}

	if (trickSpeed != SCE_AVPLAYER_SPEED_NORMAL) {
		avLogError(debugLevel, "JumpToTime", 654, " User Jump cannot performed during Trick mode ");
		return -1;
	}
	if (demux == NULL) {
		avLogError(debugLevel, "JumpToTime", 658, " Demux Module doesnt exist !!");
		return -1;
	}
	if (streamList.count == 0) {
		avLogWarn(debugLevel, "JumpToTime", 662, " Av Core Filters unavailable");
		return -1;
	}

	int32_t srcType = demux->getSourceType();
	if (srcType != 1 && srcType != 2 && srcType != 8) {
		avLogWarn(debugLevel, "JumpToTime", 669, " Jump not possible on this source type");
		return -1;
	}

	uint64_t curPts90k = getCurrentTime() * 90u;
	uint64_t targetPts90k = (uint64_t)timeMs * 90u;
	avLogDebug(debugLevel, "JumpToTime", 679, " Jumping from  %lld to %lld ", curPts90k, targetPts90k / 90u);
	int32_t ret = jumpInternal(targetPts90k);
	if (ret < 0)
		avLogWarn(debugLevel, "JumpToTime", 683, "Can't jump from %lld to %lld ", curPts90k, targetPts90k / 90u);
	return ret;
}

int32_t AvController::jumpTimeOffset(int64_t offsetMs)
{
	int8_t curState = (int8_t)*stateMachine->getStatePtr();
	avLogDebug(debugLevel, "JumpTimeOffset", 691, " jumpOffset: %lld, state %d ", offsetMs, curState);

	if (offsetMs == 0 || (curState != AV_STATE_PLAY && curState != AV_STATE_BUFFERING))
		return -1;
	if (demux == NULL) {
		avLogError(debugLevel, "JumpTimeOffset", 698, " Demux Module doesnt exist !!");
		return 0;
	}
	if (streamList.count == 0) {
		avLogWarn(debugLevel, "JumpTimeOffset", 702, " Av Core Filters unavailable");
		return 0;
	}

	int64_t curPts90k = (int64_t)(getCurrentTime() * 90u);
	int64_t targetPts90k = curPts90k + (offsetMs * 90LL);
	if (targetPts90k < 0) {
		avLogError(debugLevel, "JumpTimeOffset", 712, "Cannot Jump to desired Timestamp");
		return 0;
	}

	avLogDebug(debugLevel, "JumpTimeOffset", 717, " Jump from  %lld to %lld  \n", curPts90k, targetPts90k);
	int32_t ret = jumpInternal((uint64_t)targetPts90k);
	if (ret < 0)
		avLogWarn(debugLevel, "JumpTimeOffset", 721, "Cannot jump from %lld to %lld ", curPts90k, targetPts90k);
	return ret;
}

int32_t AvController::handleFlowEvent(AvEvent *event, int32_t sender, void *outData)
{
	if (event == NULL)
		return -1;

	int32_t videoIdx = -1;
	for (uint32_t i = 0; i < streamList.count; i++) {
		AvStreamSlot *slot = streamList.at(i);
		if (slot->descriptor->streamType == SCE_AVPLAYER_VIDEO && slot->enabled == 1)
			videoIdx = (int32_t)i;
	}

	if ((event->type == AV_EVENT_TRICKMODE || event->type == AV_EVENT_JUMP) &&
	    sender == AV_SENDER_DEMUX && outData != NULL) {
		int32_t activeDecoders = 0;
		for (uint32_t i = 0; i < streamList.count; i++) {
			AvStreamSlot *slot = streamList.at(i);
			if ((slot->descriptor->streamType == SCE_AVPLAYER_VIDEO || slot->descriptor->streamType == SCE_AVPLAYER_AUDIO) &&
			    slot->enabled == 1) {
				activeDecoders++;
			}
		}
		*(int32_t *)outData = activeDecoders;
	}

	switch (event->type) {
	case AV_EVENT_ERROR:
		return changeFlowState(AV_STATE_ERROR);
	case AV_EVENT_JUMP:
		if (sender == AV_SENDER_DEMUX) {
			int32_t ret = demux->jump(event->payload);
			avLogDebug(debugLevel, "CbFlowMgr", 126, "Sent Jump event to Demux");
			return ret;
		}
		if (sender == AV_SENDER_DECODER) {
			if (outData != NULL) {
				jumpCompletePending = 1;
				return 0;
			}
			int32_t hasAudioDecoder = 0;
			for (uint32_t i = 0; i < streamList.count; i++) {
				AvStreamSlot *slot = streamList.at(i);
				if (slot->active != 1)
					continue;
				if (slot->coreDecoder != NULL &&
				    (slot->descriptor->streamType == SCE_AVPLAYER_VIDEO || slot->descriptor->streamType == SCE_AVPLAYER_AUDIO)) {
					if (slot->coreDecoder->jump(event->payload) == -1)
						break;
					if (slot->descriptor->streamType == SCE_AVPLAYER_AUDIO)
						hasAudioDecoder = 1;
				} else if (slot->auFifo != NULL) {
					slot->auFifo->flush(avMspaceFree);
				}
			}
			if (hasAudioDecoder == 0) {
				for (uint32_t i = 0; i < streamList.count; i++) {
					AvStreamSlot *slot = streamList.at(i);
					if (slot->active == 1 && slot->currentFrame != NULL)
						slot->currentFrame->pts = (uint64_t)event->payload;
				}
			}
		}
		return 0;
	case AV_EVENT_TRICKMODE:
		if (sender == AV_SENDER_DEMUX) {
			if (demux->setPlaybackMode((int32_t)event->payload) < 0)
				avLogWarn(debugLevel, "CbFlowMgr", 118, "Playback mode failure");
			avLogDebug(debugLevel, "CbFlowMgr", 119, "Sent Trick event to Demux with %d ", (int32_t)event->payload);
		}
		return 0;
	case AV_EVENT_GET_VIDEO_PTS:
		if (outData != NULL)
			*(int64_t *)outData = -1LL;
		if (videoLoopPending != 1 && videoIdx >= 0 && outData != NULL) {
			AvStreamSlot *slot = streamList.at((uint32_t)videoIdx);
			if (slot->outputFifo != NULL && slot->outputFifo->getReadyCount() > 0) {
				AvDecodedFrame *peeked = slot->outputFifo->peekOrPopReady(0);
				if (peeked != NULL && peeked->resetFlag == 0 && peeked->pts != 0)
					*(int64_t *)outData = (int64_t)peeked->pts;
			}
		}
		return 0;
	case AV_EVENT_GET_AUDIO_PTS:
		if (outData != NULL)
			*(int64_t *)outData = -1LL;
		if (audioLoopPending != 1 && outData != NULL) {
			for (uint32_t i = 0; i < streamList.count; i++) {
				AvStreamSlot *slot = streamList.at(i);
				if (slot->active == 1 && slot->enabled == 1 && slot->descriptor->streamType == SCE_AVPLAYER_AUDIO) {
					if (slot->outputFifo != NULL && slot->outputFifo->getReadyCount() > 0) {
						AvDecodedFrame *peeked = slot->outputFifo->peekOrPopReady(0);
						if (peeked != NULL)
							*(int64_t *)outData = (int64_t)peeked->pts;
					}
					break;
				}
			}
		}
		return 0;
	case AV_EVENT_DROP_VIDEO_FRAME:
		if (videoIdx >= 0) {
			AvStreamSlot *slot = streamList.at((uint32_t)videoIdx);
			if (slot->active == 1 && slot->enabled == 1 && slot->outputFifo != NULL && slot->outputFifo->getReadyCount() > 0) {
				AvDecodedFrame *peeked = slot->outputFifo->peekOrPopReady(0);
				if (peeked != NULL && peeked->resetFlag == 0) {
					AvDecodedFrame *dropped = slot->outputFifo->popReady();
					if (dropped != NULL) {
						stateMachine->updateVideoPts(dropped->pts);
						avMspaceFree(dropped);
					}
				}
			}
		}
		return 0;
	case AV_EVENT_CHECK_BUFFERING: {
		if (outData == NULL)
			return -1;
		*(int32_t *)outData = 0;
		if (demux->sourceStatus == 5 || playbackMode != AV_PLAYMODE_NORMAL ||
		    videoLoopPending == 1 || audioLoopPending == 1 || loopBothReady == 1) {
			return 0;
		}
		int32_t starvedCount = 0;
		for (uint32_t i = 0; i < streamList.count; i++) {
			AvStreamSlot *slot = streamList.at(i);
			if (slot->active == 1 && slot->enabled == 1 &&
			    (slot->descriptor->streamType == SCE_AVPLAYER_VIDEO || slot->descriptor->streamType == SCE_AVPLAYER_AUDIO)) {
				if (slot->auFifo != NULL && slot->auFifo->isBufferFull() &&
				    slot->outputFifo != NULL && slot->outputFifo->isBufferFull()) {
					return 0;
				}
				if (slot->auFifo != NULL && (int64_t)slot->auFifo->getCount() <= event->payload)
					starvedCount++;
			}
		}
		if (starvedCount != 0)
			*(int32_t *)outData = 1;
		return 0;
	}
	case AV_EVENT_CHANGE_STATE: {
		int8_t mapped = (int8_t)stateMachine->mapStateCode((int32_t)event->payload);
		return changeFlowState(mapped);
	}
	case AV_EVENT_SEND_WARNING: {
		int32_t warnCode = (int32_t)event->payload;
		if (eventCb.eventCallback != NULL)
			eventCb.eventCallback(eventCb.objectPointer, SCE_AVPLAYER_WARNING_ID, sourceId, &warnCode);
		return 0;
	}
	case AV_EVENT_CHECK_SOURCE:
		if (outData == NULL)
			return 0;
		return demux->checkSource(sourcePath, &fileOps, *(int32_t *)outData);
	default:
		avLogWarn(debugLevel, "CbFlowMgr", 315, "Cannot handle this event: %d ", event->type);
		return -1;
	}
}
