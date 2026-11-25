/*
 * SysVoice central service dispatcher, attach/detach lifecycle, and update loop.
 */

#include "voice_internal.h"

SysVoice gSysVoice;

int sysVoiceDispatchCommand(SysVoice *sys, VoiceCommand *cmd)
{
	if (cmd->commandId == VOICE_CMD_INIT) {
		return sysVoiceHandleInit(&gSysVoice, cmd);
	}

	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	if (inst == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}

	switch (cmd->commandId) {
	case VOICE_CMD_END:
		return sysVoiceHandleEnd(&gSysVoice, cmd);
	case VOICE_CMD_START:
		return sysVoiceHandleStart(&gSysVoice, cmd);
	case VOICE_CMD_STOP:
		return sysVoiceHandleStop(&gSysVoice, cmd);
	case VOICE_CMD_ATTACH:
		return sysVoiceHandleAttach(&gSysVoice, cmd);
	case VOICE_CMD_DETACH:
		return sysVoiceHandleDetach(&gSysVoice);
	case VOICE_CMD_CREATE_PORT:
		return sysVoiceHandleCreatePort(&gSysVoice, cmd);
	case VOICE_CMD_UPDATE_PORT:
		return sysVoiceHandleUpdatePort(&gSysVoice, cmd);
	case VOICE_CMD_CONNECT_PORT:
		return sysVoiceHandleConnectPort(&gSysVoice, cmd);
	case VOICE_CMD_DISCONNECT_PORT:
		return sysVoiceHandleDisconnectPort(&gSysVoice, cmd);
	case VOICE_CMD_DELETE_PORT: {
		sceKernelLockLwMutex(&gSysVoice.globalMutex, 1, NULL);
		gSysVoice.globalLockOwner = "DeletePort";
		gSysVoice.globalLockCount++;
		int ret = sysVoiceHandleDeletePort(&gSysVoice, cmd);
		gSysVoice.globalLockCount--;
		gSysVoice.globalLockOwner = "<none>";
		sceKernelUnlockLwMutex(&gSysVoice.globalMutex, 1);
		return ret;
	}
	case VOICE_CMD_SET_MUTE_ALL:
		return sysVoiceHandleSetMuteAll(&gSysVoice, cmd);
	case VOICE_CMD_SET_MUTE:
		return sysVoiceHandleSetMute(&gSysVoice, cmd);
	case VOICE_CMD_GET_MUTE:
		return sysVoiceHandleGetMute(&gSysVoice, cmd);
	case VOICE_CMD_SET_VOLUME:
		return sysVoiceHandleSetVolume(&gSysVoice, cmd);
	case VOICE_CMD_GET_VOLUME:
		return sysVoiceHandleGetVolume(&gSysVoice, cmd);
	case VOICE_CMD_SET_BITRATE:
		return sysVoiceHandleSetBitRate(&gSysVoice, cmd);
	case VOICE_CMD_GET_BITRATE:
		return sysVoiceHandleGetBitRate(&gSysVoice, cmd);
	case VOICE_CMD_SET_PORT_ATTR:
		return sysVoiceHandleSetPortAttr(&gSysVoice, cmd);
	case VOICE_CMD_GET_PORT_ATTR:
		return sysVoiceHandleGetPortAttr(&gSysVoice, cmd);
	case VOICE_CMD_RESET_PORT:
		return sysVoiceHandleResetPort(&gSysVoice, cmd);
	case VOICE_CMD_PAUSE_PORT:
		return sysVoiceHandlePausePort(&gSysVoice, cmd);
	case VOICE_CMD_RESUME_PORT:
		return sysVoiceHandleResumePort(&gSysVoice, cmd);
	case VOICE_CMD_PAUSE_PORT_ALL:
		return sysVoiceHandlePausePortAll(&gSysVoice, cmd);
	case VOICE_CMD_RESUME_PORT_ALL:
		return sysVoiceHandleResumePortAll(&gSysVoice, cmd);
	case VOICE_CMD_GET_RESOURCE_INFO:
		return sysVoiceHandleGetResourceInfo(&gSysVoice, cmd);
	case VOICE_CMD_CHECK_TOPOLOGY:
		return sysVoiceHandleCheckTopology(&gSysVoice, cmd);
	default:
		return VOICE_ERROR_INVALID_CMD;
	}
}

SysVoiceInstance *sysVoiceGetInstance(SysVoice *sys, uint32_t instanceId)
{
	if (instanceId != 0) {
		return NULL;
	}
	SysVoiceInstance *inst = &sys->instance;
	if (inst->libInstance == NULL) {
		return NULL;
	}
	if (inst->libInstance->instanceId != instanceId) {
		return NULL;
	}
	return inst;
}

VoicePort *sysVoiceGetPort(SysVoice *sys, uint32_t portId)
{
	if (portId >= VOICE_MAX_PORTS) {
		return NULL;
	}
	VoicePort *port = &sys->ports[portId];
	if (port->param.portType == SCEVOICE_PORTTYPE_NULL) {
		return NULL;
	}
	if (port->portId != portId) {
		return NULL;
	}
	return port;
}

int sysVoiceGetBitRateMetrics(SysVoice *sys, uint32_t bitrate, uint16_t *frameSize,
                              uint16_t *pcmFrameSize, uint16_t *frameDurationMs)
{
	(void)sys;
	switch (bitrate) {
	case SCEVOICE_BITRATE_3850:
		*frameSize = 20;
		*pcmFrameSize = 640;
		*frameDurationMs = 40;
		return 0;
	case SCEVOICE_BITRATE_4650:
		*frameSize = 24;
		*pcmFrameSize = 640;
		*frameDurationMs = 40;
		return 0;
	case SCEVOICE_BITRATE_5700:
		*frameSize = 15;
		*pcmFrameSize = 320;
		*frameDurationMs = 20;
		return 0;
	case SCEVOICE_BITRATE_7300:
		*frameSize = 19;
		*pcmFrameSize = 320;
		*frameDurationMs = 20;
		return 0;
	case VOICE_BITRATE_12200:
		*frameSize = 16;
		*pcmFrameSize = 160;
		*frameDurationMs = 10;
		return 0;
	default:
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}
}

int sysVoiceValidatePortParam(SysVoice *sys, const SceVoicePortParam *param, VoicePortFormatMetrics *metrics)
{
	switch (param->portType) {
	case SCEVOICE_PORTTYPE_IN_DEVICE:
	case SCEVOICE_PORTTYPE_OUT_DEVICE:
		metrics->pcmFrameSize = 512;
		metrics->frameSize = 512;
		metrics->frameDurationMs = 16;
		return 0;
	case SCEVOICE_PORTTYPE_IN_PCMAUDIO:
	case SCEVOICE_PORTTYPE_OUT_PCMAUDIO:
		metrics->frameDurationMs = 16;
		if (param->pcmaudio.format.sampleRate != SCEVOICE_SAMPLINGRATE_16000 ||
		    param->pcmaudio.format.dataType != SCEVOICE_PCM_SHORT_LITTLE_ENDIAN) {
			return SCE_VOICE_ERROR_ARGUMENT_INVALID;
		}
		metrics->frameSize = 512;
		metrics->pcmFrameSize = 512;
		return 0;
	case SCEVOICE_PORTTYPE_IN_VOICE:
	case SCEVOICE_PORTTYPE_OUT_VOICE:
		return sysVoiceGetBitRateMetrics(sys, (uint32_t)param->voice.bitrate,
		                                 &metrics->frameSize, &metrics->pcmFrameSize,
		                                 &metrics->frameDurationMs);
	default:
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}
}

int sysVoiceInitGlobalLock(SysVoice *sys)
{
	int ret = sceKernelCreateLwMutex(&sys->globalMutex, "SceSysvoiceGlobal", 0, 0, NULL);
	sys->globalLockCount = 0;
	return ret;
}

int sysVoiceTermGlobalLock(SysVoice *sys)
{
	return sceKernelDeleteLwMutex(&sys->globalMutex);
}

int sysVoiceLockGlobal(SysVoice *sys, const char *owner)
{
	int ret = sceKernelLockLwMutex(&sys->globalMutex, 1, NULL);
	sys->globalLockOwner = owner;
	sys->globalLockCount++;
	return ret;
}

int sysVoiceUnlockGlobal(SysVoice *sys)
{
	sys->globalLockOwner = "<none>";
	sys->globalLockCount--;
	return sceKernelUnlockLwMutex(&sys->globalMutex, 1);
}

void sysVoiceInitResourceLimits(VoiceResourceLimits *limits, int32_t appType)
{
	limits->maxTotalPort = VOICE_MAX_PORTS;
	limits->maxPcmPort = 14;
	limits->maxEdges = VOICE_MAX_EDGES;
	limits->maxEncoders = VOICE_MAX_ENCODERS;
	limits->maxDevicePort = 1;
	limits->maxDecoders = VOICE_MAX_DECODERS;
	limits->maxInVoicePort = VOICE_MAX_DECODERS;
	limits->maxOutVoicePort = SCE_VOICE_MAX_OUT_VOICE_PORT;
	if (appType == (int32_t)0x80000000u || appType == SCEVOICE_APPTYPE_GAME) {
		limits->maxPcmMemBytes = 0x10000;
		limits->privateMemSize = 98304;
		limits->totalMemSize = 163840;
	}
}

void *sysVoiceAllocMem(SysVoice *sys, uint32_t size, const char *tag, int isPrivate)
{
	(void)tag;
	if (isPrivate != 0) {
		return voiceMemPoolAlloc(&sys->privateMemPool, size);
	}
	return voiceMemPoolAlloc(&sys->sharedMemPool, size);
}

void sysVoiceFreeMem(SysVoice *sys, void *ptr, int isPrivate)
{
	if (isPrivate != 0) {
		voiceMemPoolFree(&sys->privateMemPool, ptr);
	} else {
		voiceMemPoolFree(&sys->sharedMemPool, ptr);
	}
}

int sysVoiceUpdateThreadEntry(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	int ret = sceKernelSetEventFlag(gSysVoice.updateEventFlag, 0x20000000u);
	SysVoiceInstance *inst = gSysVoice.activeInstance;

	while (gSysVoice.updateRunning != 0) {
		sceKernelLockLwMutex(&gSysVoice.globalMutex, 1, NULL);
		gSysVoice.globalLockOwner = "Update";
		gSysVoice.globalLockCount++;

		int needDelay = sysVoiceProcessAudioInPorts(inst);
		sysVoiceProcessInputPorts(inst);
		sysVoiceProcessMixPorts(inst);
		sysVoiceProcessOutputPorts(inst);
		sysVoiceProcessAudioOutPorts(inst);

		gSysVoice.globalLockOwner = "<none>";
		gSysVoice.globalLockCount--;
		sceKernelUnlockLwMutex(&gSysVoice.globalMutex, 1);

		gSysVoice.updateTickCount++;
		if (needDelay != 0) {
			sceKernelDelayThread(4000);
		}
	}
	return ret;
}

void sysVoiceStaticInit(void)
{
	voiceListInit(&gSysVoice.instance.inputPortList);
	voiceListInit(&gSysVoice.instance.outputPortList);
	for (int i = 0; i < VOICE_MAX_PORTS; i++) {
		gSysVoice.ports[i].node.prev = NULL;
		gSysVoice.ports[i].node.next = NULL;
		gSysVoice.ports[i].node.owner = NULL;
		voiceListInit(&gSysVoice.ports[i].edgeList);
	}
	voiceListInit(&gSysVoice.freePortList);
	for (int i = 0; i < VOICE_MAX_EDGES; i++) {
		gSysVoice.edges[i].node.prev = NULL;
		gSysVoice.edges[i].node.next = NULL;
		gSysVoice.edges[i].node.owner = NULL;
	}
	voiceListInit(&gSysVoice.sharedMemPool.availBlocks);
	voiceListInit(&gSysVoice.sharedMemPool.usedBlocks);
	voiceListInit(&gSysVoice.sharedMemPool.freeBlocks);
	voiceListInit(&gSysVoice.privateMemPool.availBlocks);
	voiceListInit(&gSysVoice.privateMemPool.usedBlocks);
	voiceListInit(&gSysVoice.privateMemPool.freeBlocks);
	voiceListInit(&gSysVoice.audioInPool.activeList);
	voiceListInit(&gSysVoice.audioInPool.freeList);
	gSysVoice.audioInPool.devices[0].node.prev = NULL;
	gSysVoice.audioInPool.devices[0].node.next = NULL;
	gSysVoice.audioInPool.devices[0].node.owner = NULL;
	voiceVadHangoverConfigInit(&gSysVoice.audioInPool.devices[0].vad.hangoverCfg);
	voiceVadHangoverStateInit(&gSysVoice.audioInPool.devices[0].vad.hangoverState);
	voiceListInit(&gSysVoice.decoderPool.activeList);
	voiceListInit(&gSysVoice.decoderPool.freeList);
	for (int i = 0; i < VOICE_MAX_DECODERS; i++) {
		gSysVoice.decoderPool.slots[i].node.prev = NULL;
		gSysVoice.decoderPool.slots[i].node.next = NULL;
		gSysVoice.decoderPool.slots[i].node.owner = NULL;
	}
	voiceListInit(&gSysVoice.encoderPool.activeList);
	voiceListInit(&gSysVoice.encoderPool.freeList);
	gSysVoice.encoderPool.slots[0].node.prev = NULL;
	gSysVoice.encoderPool.slots[0].node.next = NULL;
	gSysVoice.encoderPool.slots[0].node.owner = NULL;
	voiceListInit(&gSysVoice.audioOutPool.activeList);
	voiceListInit(&gSysVoice.audioOutPool.freeList);
	gSysVoice.audioOutPool.devices[0].node.prev = NULL;
	gSysVoice.audioOutPool.devices[0].node.next = NULL;
	gSysVoice.audioOutPool.devices[0].node.owner = NULL;
	voiceListInit(&gSysVoice.freeEdgeList);
}

int sysVoiceAttachInstance(SysVoice *sys, SysVoiceInstance *inst)
{
	uint8_t *privBase = (uint8_t *)inst->privateMemBase;
	void *sharedBase = inst->sharedMemBase;
	uint32_t sharedSize = inst->limits.privateMemSize;

	voiceMemPoolInit(&sys->privateMemPool, privBase + 8960, inst->limits.totalMemSize,
	                 (VoiceMemBlock *)privBase, 192, 0, "Private Memory");
	voiceMemPoolInit(&sys->sharedMemPool, sharedBase, sharedSize,
	                 (VoiceMemBlock *)(privBase + 5376), 128, 1, "Shared Memory");

	sys->activeInstance = inst;
	sysVoiceInstanceAllocSharedTable(inst);
	voiceMixerAllocWorkBufs(&sys->mixer);

	for (uint32_t i = 0; i < inst->limits.maxDecoders; i++) {
		voiceDecoderPoolAllocSlotBuf(&sys->decoderPool, i);
	}
	for (uint32_t i = 0; i < inst->limits.maxEncoders; i++) {
		voiceEncoderPoolAllocSlotBuf(&sys->encoderPool, i);
	}

	int lastErr = 0;
	for (VoicePort *port = (VoicePort *)inst->inputPortList.head;
	     port != NULL;
	     port = (VoicePort *)port->node.next) {
		int err = voicePortAttachResources(port, inst);
		if (err != 0) {
			lastErr = err;
		}
	}
	for (VoicePort *port = (VoicePort *)inst->outputPortList.head;
	     port != NULL;
	     port = (VoicePort *)port->node.next) {
		int err = voicePortAttachResources(port, inst);
		if (err != 0) {
			lastErr = err;
		}
	}

	inst->attached = 1;
	if (lastErr != 0 && ((uint32_t)lastErr & VOICE_FACILITY_MASK) != VOICE_FACILITY_MASK) {
		return SCE_VOICE_ERROR_GENERAL;
	}
	return lastErr;
}

int sysVoiceHandleAttach(SysVoice *sys, VoiceCommand *cmd)
{
	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	sysVoiceInstanceLockData(inst, "SysVoice::Attach");
	sysVoiceLockGlobal(sys, "Attach");

	inst->privateMemBase = cmd->payload.attach.privateBase;
	inst->sharedMemBase = cmd->payload.attach.sharedBase;
	inst->containerUid = cmd->payload.attach.containerUid;
	inst->sharedMemStart = cmd->payload.attach.sharedBase;

	int ret = sysVoiceAttachInstance(sys, inst);
	if (sys->updateRunning == 0) {
		sysVoiceStartUpdateThread(sys);
	}

	sysVoiceInstanceUnlockData(inst);
	sysVoiceUnlockGlobal(sys);
	return ret;
}

int sysVoiceDetachInstance(SysVoice *sys, SysVoiceInstance *inst)
{
	int lastErr = 0;
	for (VoicePort *port = (VoicePort *)inst->inputPortList.head;
	     port != NULL;
	     port = (VoicePort *)port->node.next) {
		int err = voicePortDetachResources(port, inst);
		if (err != 0) {
			lastErr = err;
		}
	}
	for (VoicePort *port = (VoicePort *)inst->outputPortList.head;
	     port != NULL;
	     port = (VoicePort *)port->node.next) {
		int err = voicePortDetachResources(port, inst);
		if (err != 0) {
			lastErr = err;
		}
	}

	for (uint32_t i = 0; i < inst->limits.maxEncoders; i++) {
		voiceEncoderPoolFreeSlotBuf(&sys->encoderPool, i);
	}
	for (uint32_t i = 0; i < inst->limits.maxDecoders; i++) {
		voiceDecoderPoolFreeSlotBuf(&sys->decoderPool, i);
	}

	voiceMixerFreeWorkBufs(&sys->mixer);
	sysVoiceInstanceFreeSharedTable(inst);
	sys->activeInstance = NULL;
	inst->attached = 0;

	if (lastErr != 0 && ((uint32_t)lastErr & VOICE_FACILITY_MASK) != VOICE_FACILITY_MASK) {
		return SCE_VOICE_ERROR_GENERAL;
	}
	return lastErr;
}

int sysVoiceHandleDetach(SysVoice *sys)
{
	SysVoiceInstance *inst = sys->activeInstance;
	if (sys->updateRunning != 0) {
		sysVoiceStopUpdateThread(sys);
	}

	sysVoiceInstanceLockData(inst, "SysVoice::Detach()");
	sysVoiceLockGlobal(sys, "Detach");

	int ret = 0;
	if (inst->attached != 0) {
		ret = sysVoiceDetachInstance(sys, inst);
		voiceMemPoolTerm(&sys->privateMemPool);
		voiceMemPoolTerm(&sys->sharedMemPool);
	}

	sysVoiceInstanceUnlockData(inst);
	sysVoiceUnlockGlobal(sys);
	return ret;
}
