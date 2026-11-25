/*
 * Port lifecycle, resource binding, and SysVoice command handlers.
 */

#include "voice_internal.h"

int16_t gVadSampleScratch[256];

void voicePortInit(VoicePort *port, uint16_t portId)
{
	port->portId = portId;
	port->param.portType = SCEVOICE_PORTTYPE_NULL;
	port->codecSlotId = VOICE_UNBOUND_SLOT;
	port->sampleRate = 16000;
	port->paused = 0;
	voiceMemset(port->resampleHistory, 0, sizeof(port->resampleHistory));
	voiceMemset(&port->reservedTiming, 0, 24);
}

void voicePortConfigure(VoicePort *port, const SceVoicePortParam *param, const VoicePortFormatMetrics *metrics)
{
	port->param = *param;
	port->frameDurationMs = metrics->frameDurationMs;
	port->bufferBytes = metrics->bufferBytes;
	port->frameSize = metrics->frameSize;
	port->pcmFrameSize = metrics->pcmFrameSize;
	if (port->param.portType == SCEVOICE_PORTTYPE_IN_DEVICE) {
		port->param.threshold = 16;
	}
}

void voicePortResetIds(VoicePort *port)
{
	port->param.portType = SCEVOICE_PORTTYPE_NULL;
	port->codecSlotId = VOICE_UNBOUND_SLOT;
}

void voicePortBindShared(VoicePort *port, SysVoiceInstance *inst)
{
	intptr_t delta = (uint8_t *)inst->sharedMemStart - (uint8_t *)inst->sharedMemBase;
	VoiceSharedPort *sharedPort = sysVoiceInstanceGetSharedPort(inst, port->portId);
	voiceSharedPortInit(sharedPort, port, delta);
}

void voicePortUnbindShared(VoicePort *port, SysVoiceInstance *inst)
{
	VoiceSharedPort *sharedPort = sysVoiceInstanceGetSharedPort(inst, port->portId);
	voiceSharedPortTerm(sharedPort);
}

VoiceSharedPort *voicePortSyncShared(VoicePort *port, SysVoiceInstance *inst)
{
	VoiceSharedPort *sharedPort = sysVoiceInstanceGetSharedPort(inst, port->portId);
	if ((uint32_t)port->param.portType > SCEVOICE_PORTTYPE_IN_VOICE) {
		port->sharedRing.readIdx = sharedPort->dataRing.readIdx;
		port->sharedRing.readCount = sharedPort->dataRing.readCount;
		sharedPort->dataRing.writeIdx = port->sharedRing.writeIdx;
		sharedPort->dataRing.writeCount = port->sharedRing.writeCount;
	} else {
		port->sharedRing.writeIdx = sharedPort->dataRing.writeIdx;
		port->sharedRing.writeCount = sharedPort->dataRing.writeCount;
		sharedPort->dataRing.readIdx = port->sharedRing.readIdx;
		sharedPort->dataRing.readCount = port->sharedRing.readCount;

		if (port->param.portType == SCEVOICE_PORTTYPE_IN_VOICE ||
		    port->param.portType == SCEVOICE_PORTTYPE_OUT_VOICE) {
			port->indexRing.writeIdx = sharedPort->indexRing.writeIdx;
			port->indexRing.writeCount = sharedPort->indexRing.writeCount;
			sharedPort->indexRing.readIdx = port->indexRing.readIdx;
			sharedPort->indexRing.readCount = port->indexRing.readCount;
		}
	}

	if (port->paused != 0 || inst->pausedAll != 0) {
		sharedPort->portState = SCEVOICE_PORTSTATE_IDLE;
	} else {
		sharedPort->portState = port->state;
	}
	return sharedPort;
}

int voicePortHasSameTopology(const VoicePort *portA, const VoicePort *portB)
{
	if (portB->param.portType != portA->param.portType ||
	    portB->param.volume != portA->param.volume ||
	    portB->param.voice.bitrate != portA->param.voice.bitrate ||
	    portB->edgeList.count != portA->edgeList.count) {
		return 0;
	}

	for (VoiceEdge *edgeA = (VoiceEdge *)portA->edgeList.head;
	     edgeA != NULL;
	     edgeA = (VoiceEdge *)edgeA->node.next) {
		int found = 0;
		for (VoiceEdge *edgeB = (VoiceEdge *)portB->edgeList.head;
		     edgeB != NULL;
		     edgeB = (VoiceEdge *)edgeB->node.next) {
			if (edgeB->iPortId == edgeA->iPortId) {
				found = 1;
				break;
			}
		}
		if (!found) {
			return 0;
		}
	}
	return 1;
}

float voicePortGetThresholdMs(const VoicePort *port)
{
	float minMs = (float)(int32_t)port->frameDurationMs * 2.0f;
	float thresholdMs = (float)port->param.threshold;
	return (thresholdMs >= minMs) ? thresholdMs : minMs;
}

float voicePortGetBufferedMs(const VoicePort *port)
{
	uint16_t sharedUsed = (uint16_t)(port->sharedRing.writeCount - port->sharedRing.readCount);
	uint16_t internalUsed = (uint16_t)(port->internalRing.writeCount - port->internalRing.readCount);
	float internalSamples = (float)internalUsed * 0.5f;
	if (port->param.portType == SCEVOICE_PORTTYPE_OUT_DEVICE) {
		internalSamples /= 6.0f;
	}
	return internalSamples * 0.0625f +
	       (float)(int32_t)port->frameDurationMs * ((float)sharedUsed / (float)port->frameSize);
}

void voicePortAllocBuffers(VoicePort *port)
{
	switch (port->param.portType) {
	case SCEVOICE_PORTTYPE_IN_VOICE:
	case SCEVOICE_PORTTYPE_OUT_VOICE: {
		uint8_t *buf = (uint8_t *)sysVoiceAllocMem(
			&gSysVoice, port->bufferBytes + 384u, "Port workspace (voice)", 0);
		uint16_t dataCap = (uint16_t)((port->bufferBytes / port->frameSize) * port->frameSize);
		voiceRingInit(&port->sharedRing, buf, dataCap);
		uint16_t idxCap = (uint16_t)(2u * (port->bufferBytes / port->frameSize));
		uint8_t *idxBuf = buf + port->bufferBytes;
		voiceRingInit(&port->indexRing, idxBuf, idxCap);
		voiceMemset(idxBuf, 0, idxCap);
		break;
	}
	case SCEVOICE_PORTTYPE_IN_PCMAUDIO:
	case SCEVOICE_PORTTYPE_OUT_PCMAUDIO: {
		void *privBuf = sysVoiceAllocMem(&gSysVoice, port->bufferBytes, "Decoded Workspace", 1);
		voiceRingInit(&port->internalRing, privBuf, port->bufferBytes);
		void *sharedBuf = sysVoiceAllocMem(&gSysVoice, port->bufferBytes, "Port workspace (PCM)", 0);
		voiceRingInit(&port->sharedRing, sharedBuf, port->bufferBytes);
		break;
	}
	case SCEVOICE_PORTTYPE_IN_DEVICE:
	case SCEVOICE_PORTTYPE_OUT_DEVICE: {
		void *privBuf = sysVoiceAllocMem(&gSysVoice, port->bufferBytes, "Decoded Workspace", 1);
		voiceRingInit(&port->internalRing, privBuf, port->bufferBytes);
		break;
	}
	default:
		break;
	}
}

int voicePortAttachResources(VoicePort *port, SysVoiceInstance *inst)
{
	int err = 0;
	port->sampleRate = 16000;

	switch (port->param.portType) {
	case SCEVOICE_PORTTYPE_IN_DEVICE:
		err = voiceAudioInPoolAcquire(&gSysVoice.audioInPool, port, inst);
		break;
	case SCEVOICE_PORTTYPE_IN_PCMAUDIO:
	case SCEVOICE_PORTTYPE_OUT_PCMAUDIO:
		port->codecSlotId = 7;
		voicePortAllocBuffers(port);
		voicePortBindShared(port, inst);
		sysVoiceInstanceDeduplicateOutputs(inst);
		port->state = SCEVOICE_PORTSTATE_READY;
		return 0;
	case SCEVOICE_PORTTYPE_IN_VOICE:
		port->sampleRate = 8000;
		err = voiceDecoderPoolAcquire(&gSysVoice.decoderPool, port, inst);
		break;
	case SCEVOICE_PORTTYPE_OUT_VOICE:
		port->sampleRate = 8000;
		err = voiceEncoderPoolAcquire(&gSysVoice.encoderPool, port, inst);
		break;
	case SCEVOICE_PORTTYPE_OUT_DEVICE:
		err = voiceAudioOutPoolAcquire(&gSysVoice.audioOutPool, port, inst);
		break;
	default:
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	voicePortAllocBuffers(port);
	voicePortBindShared(port, inst);
	sysVoiceInstanceDeduplicateOutputs(inst);
	port->state = SCEVOICE_PORTSTATE_READY;

	if (err != 0) {
		if (((uint32_t)err & VOICE_FACILITY_MASK) == VOICE_FACILITY_MASK) {
			return err;
		}
		return SCE_VOICE_ERROR_GENERAL;
	}
	return 0;
}

void voicePortFreeBuffers(VoicePort *port)
{
	SceVoicePortType type = port->param.portType;
	if (type == SCEVOICE_PORTTYPE_NULL) {
		return;
	}

	if (type == SCEVOICE_PORTTYPE_IN_VOICE || type == SCEVOICE_PORTTYPE_OUT_VOICE) {
		sysVoiceFreeMem(&gSysVoice, port->sharedRing.buffer, 0);
		voiceRingTerm(&port->sharedRing);
	} else {
		sysVoiceFreeMem(&gSysVoice, port->internalRing.buffer, 1);
		voiceRingTerm(&port->internalRing);
		if (type == SCEVOICE_PORTTYPE_IN_PCMAUDIO || type == SCEVOICE_PORTTYPE_OUT_PCMAUDIO) {
			sysVoiceFreeMem(&gSysVoice, port->sharedRing.buffer, 0);
			voiceRingTerm(&port->sharedRing);
		}
	}
}

int voicePortDetachResources(VoicePort *port, SysVoiceInstance *inst)
{
	int err = 0;
	switch (port->param.portType) {
	case SCEVOICE_PORTTYPE_IN_DEVICE:
		err = voiceAudioInPoolRelease(&gSysVoice.audioInPool, port);
		break;
	case SCEVOICE_PORTTYPE_IN_PCMAUDIO:
	case SCEVOICE_PORTTYPE_OUT_PCMAUDIO:
		port->codecSlotId = VOICE_UNBOUND_SLOT;
		voicePortFreeBuffers(port);
		voicePortUnbindShared(port, inst);
		port->state = SCEVOICE_PORTSTATE_IDLE;
		return 0;
	case SCEVOICE_PORTTYPE_IN_VOICE:
		err = voiceDecoderPoolRelease(&gSysVoice.decoderPool, port);
		break;
	case SCEVOICE_PORTTYPE_OUT_VOICE:
		err = voiceEncoderPoolRelease(&gSysVoice.encoderPool, port);
		break;
	case SCEVOICE_PORTTYPE_OUT_DEVICE:
		err = voiceAudioOutPoolRelease(&gSysVoice.audioOutPool, port);
		break;
	default:
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	voicePortFreeBuffers(port);
	voicePortUnbindShared(port, inst);
	port->state = SCEVOICE_PORTSTATE_IDLE;

	if (err != 0) {
		if (((uint32_t)err & VOICE_FACILITY_MASK) == VOICE_FACILITY_MASK) {
			return err;
		}
		return SCE_VOICE_ERROR_GENERAL;
	}
	return 0;
}

int voicePortAcquireEncoder(VoicePort *port, SysVoiceInstance *inst)
{
	port->sampleRate = 8000;
	return voiceEncoderPoolAcquire(&gSysVoice.encoderPool, port, inst);
}

int voicePortReleaseEncoder(VoicePort *port)
{
	return voiceEncoderPoolRelease(&gSysVoice.encoderPool, port);
}

int voicePortRunVad(VoicePort *port, const int16_t *pcmSamples, VoiceVadFilter *vad)
{
	for (int i = 0; i < 256; i++) {
		gVadSampleScratch[i] = pcmSamples[i];
	}

	int speech = voiceVadFilterProcess(vad, gVadSampleScratch, gVadSampleScratch + 256);
	if (speech != 0) {
		port->vadSpeechActive = 1;
		port->idleDuration = 0.0f;
		port->activeDuration += 0.016f;
		return 1;
	}

	port->vadSpeechActive = 0;
	port->activeDuration = 0.0f;
	port->idleDuration += 0.016f;
	return 0;
}

int voicePortCheckAudioInAdopt(VoicePort *port)
{
	int adopt = sceAudioInGetAdopt(SCE_AUDIO_IN_PORT_TYPE_VOICE);
	if (adopt != 0) {
		if (port->audioInActive != 0) {
			return 1;
		}
		port->audioInActive = 1;
		if (gSysVoice.eventDispatcher.running == 0) {
			return 1;
		}
		SceVoiceEventData eventData __attribute__((aligned(32)));
		voiceMemset(&eventData, 0, sizeof(eventData));
		eventData.eventType = SCEVOICE_EVENT_AUDIOINPUT_STATUS;
		eventData.audioInput.bActive = true;
		voiceEventDispatcherSend(&gSysVoice.eventDispatcher, &eventData);
		return 1;
	}

	if (port->audioInActive != 1) {
		return (port->audioInActive != 0) ? 1 : 0;
	}
	port->audioInActive = 0;
	if (gSysVoice.eventDispatcher.running != 0) {
		SceVoiceEventData eventData __attribute__((aligned(32)));
		voiceMemset(&eventData, 0, sizeof(eventData));
		eventData.eventType = SCEVOICE_EVENT_AUDIOINPUT_STATUS;
		eventData.audioInput.bActive = false;
		voiceEventDispatcherSend(&gSysVoice.eventDispatcher, &eventData);
	}
	return (port->audioInActive != 0) ? 1 : 0;
}

int sysVoiceStartUpdateThread(SysVoice *sys)
{
	unsigned int outBits = 0;
	sys->updateEventFlag = sceKernelCreateEventFlag("SceVoiceEvf", 4096, 0x10000000, NULL);
	sys->updateThreadId = sceKernelCreateThread("SceVoiceUpdate",
	                                            (SceKernelThreadEntry)sysVoiceUpdateThreadEntry,
	                                            sys->threadPriority, 0x2000, 0,
	                                            sys->threadAffinityMask, NULL);
	sys->updateRunning = 1;
	sceKernelStartThread(sys->updateThreadId, 0, NULL);
	return sceKernelWaitEventFlag(sys->updateEventFlag, 0x20000000u, 4, &outBits, NULL);
}

int sysVoiceStopUpdateThread(SysVoice *sys)
{
	sys->updateRunning = 0;
	sceKernelWaitThreadEnd(sys->updateThreadId, NULL, NULL);
	sceKernelDeleteThread(sys->updateThreadId);
	return sceKernelDeleteEventFlag(sys->updateEventFlag);
}

int sysVoiceHandleInit(SysVoice *sys, VoiceCommand *cmd)
{
	if (cmd->payload.init.version != SCEVOICE_VERSION_100) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}
	int32_t appType = cmd->payload.init.initParam.appType;
	if (appType != (int32_t)0x80000000u && appType != SCEVOICE_APPTYPE_GAME) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	if (sys->instanceCount == 0) {
		voiceMemset(sys, 0, sizeof(SysVoice));
		int err = voiceDecoderPoolInit(&sys->decoderPool);
		if (err != 0) {
			if (((uint32_t)err & VOICE_FACILITY_MASK) == VOICE_FACILITY_MASK) {
				return err;
			}
			return SCE_VOICE_ERROR_GENERAL;
		}

		err = voiceEncoderPoolInit(&sys->encoderPool);
		if (err != 0) {
			voiceDecoderPoolTerm(&sys->decoderPool);
			if (((uint32_t)err & VOICE_FACILITY_MASK) == VOICE_FACILITY_MASK) {
				return err;
			}
			return SCE_VOICE_ERROR_GENERAL;
		}

		voiceAudioInPoolInit(&sys->audioInPool);
		voiceMixerInit(&sys->mixer);
		voiceAudioOutPoolInit(&sys->audioOutPool);

		voiceMemset(sys->ports, 0, sizeof(sys->ports));
		for (uint16_t i = 0; i < VOICE_MAX_PORTS; i++) {
			voicePortInit(&sys->ports[i], i);
		}
		voiceListInit(&sys->freePortList);
		for (int i = 0; i < VOICE_MAX_PORTS; i++) {
			voiceListPushBack(&sys->freePortList, &sys->ports[i].node);
		}

		voiceListInit(&sys->freeEdgeList);
		voiceMemset(sys->edges, 0, sizeof(sys->edges));
		for (int i = 0; i < VOICE_MAX_EDGES; i++) {
			voiceListPushBack(&sys->freeEdgeList, &sys->edges[i].node);
			sys->edges[i].iPortId = VOICE_INVALID_SLOT;
			sys->edges[i].oPortId = VOICE_INVALID_SLOT;
		}

		voiceEventDispatcherInit(&sys->eventDispatcher,
		                         cmd->payload.init.initParam.onEvent,
		                         cmd->payload.init.initParam.pUserData);

		if (sceKernelIsGameBudget() != 0) {
			sys->threadPriority = 0x100000E1;
			sys->threadAffinityMask = 0x70000;
		} else {
			sys->threadPriority = 65;
			sys->threadAffinityMask = 0;
		}
		sys->internalMemBlockUid = 0;
		sysVoiceInitGlobalLock(sys);
	}

	sysVoiceLockGlobal(sys, "Init");
	if (sys->instance.libInstance != NULL) {
		sysVoiceUnlockGlobal(sys);
		return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
	}

	sysVoiceInstanceInit(&sys->instance, cmd, 0);
	if (sys->activeInstance == NULL) {
		sys->activeInstance = &sys->instance;
	}
	sys->instanceCount++;
	sysVoiceInstanceUnlockData(&sys->instance);
	sysVoiceUnlockGlobal(sys);
	return 0;
}

int sysVoiceHandleStart(SysVoice *sys, VoiceCommand *cmd)
{
	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	if (inst == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	if (inst->attached != 0) {
		return 0;
	}

	SceUID containerUid = (SceUID)cmd->payload.start.startParam.container;
	void *baseAddr = NULL;
	if (sceKernelGetMemBlockBase(containerUid, &baseAddr) != 0) {
		return SCE_VOICE_ERROR_CONTAINER_INVALID;
	}

	cmd->commandId = VOICE_CMD_ATTACH;
	cmd->payload.attach.privateBase = baseAddr;
	cmd->payload.attach.sharedBase = (uint8_t *)baseAddr + inst->limits.totalMemSize;
	cmd->payload.attach.containerUid = containerUid;
	return sysVoiceDispatchCommand(sys, cmd);
}

int sysVoiceHandleStop(SysVoice *sys, VoiceCommand *cmd)
{
	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	if (inst == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	if (inst->attached == 0) {
		return 0;
	}
	cmd->commandId = VOICE_CMD_DETACH;
	return sysVoiceDispatchCommand(sys, cmd);
}

int sysVoiceHandleCreatePort(SysVoice *sys, VoiceCommand *cmd)
{
	VoicePortFormatMetrics metrics;

	sysVoiceLockGlobal(sys, "CreatePort");
	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	cmd->payload.portOp.portId = SCE_VOICE_INVALID_PORT_ID;
	if (inst == NULL) {
		sysVoiceUnlockGlobal(sys);
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	int err = sysVoiceValidatePortParam(sys, &cmd->payload.portOp.param, &metrics);
	if (err != 0) {
		sysVoiceUnlockGlobal(sys);
		return err;
	}

	err = sysVoiceInstanceReservePort(inst, &cmd->payload.portOp.param, &metrics);
	if (err != 0) {
		sysVoiceUnlockGlobal(sys);
		return err;
	}

	VoicePort *port = (VoicePort *)voiceListPopFront(&sys->freePortList);
	voicePortConfigure(port, &cmd->payload.portOp.param, &metrics);
	if (inst->attached != 0) {
		err = voicePortAttachResources(port, inst);
	}
	sysVoiceInstanceAddPort(inst, port);
	cmd->payload.portOp.portId = port->portId;
	sysVoiceUnlockGlobal(sys);
	return err;
}

int sysVoiceHandleDeletePort(SysVoice *sys, VoiceCommand *cmd)
{
	uint32_t portId = cmd->payload.portOp.portId;
	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	if (inst == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	VoicePort *port = sysVoiceGetPort(sys, portId);
	if (port == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	int err = sysVoiceInstanceReleasePort(inst, port);
	if (err != 0) {
		return err;
	}

	if ((uint32_t)port->param.portType > SCEVOICE_PORTTYPE_IN_VOICE) {
		while (port->edgeList.head != NULL) {
			VoiceEdge *edge = (VoiceEdge *)port->edgeList.head;
			VoicePort *iPort = sysVoiceGetPort(sys, edge->iPortId);
			VoicePort *oPort = sysVoiceGetPort(sys, edge->oPortId);
			sysVoiceInstanceDisconnectPorts(inst, iPort, oPort);
		}
	} else {
		for (VoicePort *outPort = (VoicePort *)inst->outputPortList.head;
		     outPort != NULL;
		     outPort = (VoicePort *)outPort->node.next) {
			VoiceEdge *edge = (VoiceEdge *)outPort->edgeList.head;
			while (edge != NULL) {
				if (edge->iPortId == portId) {
					sysVoiceInstanceDisconnectPorts(inst, port, outPort);
					edge = (VoiceEdge *)outPort->edgeList.head;
				} else {
					edge = (VoiceEdge *)edge->node.next;
				}
			}
		}
	}

	if (inst->attached != 0) {
		err = voicePortDetachResources(port, inst);
	}

	sysVoiceInstanceRemovePort(inst, port);
	voicePortResetIds(port);
	voiceListPushBack(&sys->freePortList, &port->node);
	return err;
}

int sysVoiceHandleEnd(SysVoice *sys, VoiceCommand *cmd)
{
	if (sys->updateRunning != 0) {
		sysVoiceStopUpdateThread(sys);
	}

	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	if (inst == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	sysVoiceInstanceLockData(inst, "SysVoice::Kill");
	sysVoiceLockGlobal(sys, "Kill");

	VoiceCommand delCmd;
	delCmd.instanceId = cmd->instanceId;
	int ret = 0;

	while (inst->outputPortList.head != NULL) {
		VoicePort *outPort = (VoicePort *)inst->outputPortList.head;
		delCmd.payload.portOp.portId = outPort->portId;
		ret = sysVoiceHandleDeletePort(sys, &delCmd);
	}
	while (inst->inputPortList.head != NULL) {
		VoicePort *inPort = (VoicePort *)inst->inputPortList.head;
		delCmd.payload.portOp.portId = inPort->portId;
		ret = sysVoiceHandleDeletePort(sys, &delCmd);
	}

	sysVoiceInstanceNoop();
	if (inst->attached != 0) {
		sysVoiceDetachInstance(sys, inst);
	}

	sys->instanceCount--;
	if (sys->instanceCount != 0) {
		sysVoiceInstanceTerm(inst);
		sysVoiceInstanceUnlockData(inst);
		sysVoiceUnlockGlobal(sys);
	} else {
		voiceAudioInPoolTerm(&sys->audioInPool);
		voiceDecoderPoolTerm(&sys->decoderPool);
		voiceMixerTerm(&sys->mixer);
		voiceEncoderPoolTerm(&sys->encoderPool);
		voiceAudioOutPoolTerm(&sys->audioOutPool);
		voiceEventDispatcherTerm(&sys->eventDispatcher);

		for (int i = 0; i < VOICE_MAX_PORTS; i++) {
			voiceListPopFront(&sys->freePortList);
		}
		for (int i = 0; i < VOICE_MAX_EDGES; i++) {
			voiceListPopFront(&sys->freeEdgeList);
		}

		sysVoiceInstanceUnlockData(inst);
		sysVoiceInstanceTerm(inst);
		sysVoiceUnlockGlobal(sys);
		sysVoiceTermGlobalLock(sys);
	}

	if (sys->internalMemBlockUid != 0) {
		ret = sceKernelFreeMemBlock(sys->internalMemBlockUid);
		sys->internalMemBlockUid = 0;
	}
	return ret;
}

int sysVoiceHandleUpdatePort(SysVoice *sys, VoiceCommand *cmd)
{
	VoicePortFormatMetrics metrics;

	sysVoiceLockGlobal(sys, "UpdatePort");
	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	VoicePort *port = (inst != NULL) ? sysVoiceGetPort(sys, cmd->payload.portOp.portId) : NULL;
	if (inst == NULL || port == NULL) {
		sysVoiceUnlockGlobal(sys);
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	if (port->param.portType != cmd->payload.portOp.param.portType) {
		sysVoiceUnlockGlobal(sys);
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	int err = sysVoiceValidatePortParam(sys, &cmd->payload.portOp.param, &metrics);
	if (err != 0) {
		sysVoiceUnlockGlobal(sys);
		return err;
	}

	SceVoicePortType type = cmd->payload.portOp.param.portType;
	if (type == SCEVOICE_PORTTYPE_IN_PCMAUDIO || type == SCEVOICE_PORTTYPE_OUT_PCMAUDIO) {
		metrics.bufferBytes = (uint16_t)((port->param.pcmaudio.bufSize / metrics.frameSize) * metrics.frameSize);
		if ((uint16_t)(metrics.bufferBytes - 1024u) > 0xF800u) {
			sysVoiceUnlockGlobal(sys);
			return SCE_VOICE_ERROR_ARGUMENT_INVALID;
		}
	} else {
		if ((type == SCEVOICE_PORTTYPE_IN_DEVICE || type == SCEVOICE_PORTTYPE_OUT_DEVICE) &&
		    port->param.device.playerId != cmd->payload.portOp.param.device.playerId) {
			sysVoiceUnlockGlobal(sys);
			return SCE_VOICE_ERROR_ARGUMENT_INVALID;
		}
		metrics.bufferBytes = 2048;
	}

	if ((port->param.portType == SCEVOICE_PORTTYPE_IN_VOICE ||
	     port->param.portType == SCEVOICE_PORTTYPE_OUT_VOICE) &&
	    port->param.voice.bitrate != cmd->payload.portOp.param.voice.bitrate) {
		voicePortConfigure(port, &cmd->payload.portOp.param, &metrics);
		voiceRingReset(&port->sharedRing);
		voiceRingReset(&port->internalRing);
		voiceRingReset(&port->indexRing);
		if (port->state == SCEVOICE_PORTSTATE_RUNNING) {
			port->state = SCEVOICE_PORTSTATE_READY;
		}
		if (inst->attached != 0) {
			voicePortDetachResources(port, inst);
			err = voicePortAttachResources(port, inst);
		}
	} else {
		voicePortConfigure(port, &cmd->payload.portOp.param, &metrics);
	}

	sysVoiceUnlockGlobal(sys);
	return err;
}

int sysVoiceHandleConnectPort(SysVoice *sys, VoiceCommand *cmd)
{
	sysVoiceLockGlobal(sys, "ConnectIPort2OPort");
	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	if (inst == NULL) {
		sysVoiceUnlockGlobal(sys);
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	VoicePort *iPort = sysVoiceGetPort(sys, cmd->payload.connect.iPortId);
	VoicePort *oPort = sysVoiceGetPort(sys, cmd->payload.connect.oPortId);
	if (iPort == NULL || oPort == NULL) {
		sysVoiceUnlockGlobal(sys);
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	int ret = sysVoiceInstanceConnectPorts(inst, iPort, oPort);
	sysVoiceUnlockGlobal(sys);
	return ret;
}

int sysVoiceHandleDisconnectPort(SysVoice *sys, VoiceCommand *cmd)
{
	sysVoiceLockGlobal(sys, "DisconnectIPort2OPort");
	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	if (inst == NULL) {
		sysVoiceUnlockGlobal(sys);
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	VoicePort *iPort = sysVoiceGetPort(sys, cmd->payload.connect.iPortId);
	VoicePort *oPort = sysVoiceGetPort(sys, cmd->payload.connect.oPortId);
	if (iPort == NULL || oPort == NULL) {
		sysVoiceUnlockGlobal(sys);
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	int ret = sysVoiceInstanceDisconnectPorts(inst, iPort, oPort);
	sysVoiceUnlockGlobal(sys);
	return ret;
}

int sysVoiceHandleResetPort(SysVoice *sys, VoiceCommand *cmd)
{
	sysVoiceLockGlobal(sys, "ResetPort");
	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	VoicePort *port = (inst != NULL) ? sysVoiceGetPort(sys, cmd->payload.portOp.portId) : NULL;
	if (inst == NULL || port == NULL) {
		sysVoiceUnlockGlobal(sys);
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	voiceRingReset(&port->sharedRing);
	voiceRingReset(&port->internalRing);
	if (port->param.portType == SCEVOICE_PORTTYPE_IN_VOICE) {
		voiceRingReset(&port->indexRing);
	}
	if (port->state == SCEVOICE_PORTSTATE_RUNNING) {
		port->state = SCEVOICE_PORTSTATE_READY;
	}

	int ret = 0;
	if (inst->attached != 0) {
		VoiceSharedPort *sharedPort = sysVoiceInstanceGetSharedPort(inst, port->portId);
		if (sharedPort != NULL) {
			voiceSharedPortReset(sharedPort);
		} else {
			ret = SCE_VOICE_ERROR_TOPOLOGY;
		}
	}

	sysVoiceUnlockGlobal(sys);
	return ret;
}

int sysVoiceHandlePausePort(SysVoice *sys, VoiceCommand *cmd)
{
	VoicePort *port = sysVoiceGetPort(sys, cmd->payload.portOp.portId);
	if (port == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	port->paused = 1;
	return 0;
}

int sysVoiceHandlePausePortAll(SysVoice *sys, VoiceCommand *cmd)
{
	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	if (inst == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	inst->pausedAll = 1;
	return 0;
}

int sysVoiceHandleResumePort(SysVoice *sys, VoiceCommand *cmd)
{
	VoicePort *port = sysVoiceGetPort(sys, cmd->payload.portOp.portId);
	if (port == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	port->paused = 0;
	return 0;
}

int sysVoiceHandleResumePortAll(SysVoice *sys, VoiceCommand *cmd)
{
	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	if (inst == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	inst->pausedAll = 0;
	return 0;
}
