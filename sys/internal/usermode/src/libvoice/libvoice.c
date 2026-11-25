/*
 * SceVoice public API and LibVoiceInstance userspace interface.
 */

#include "voice_internal.h"

LibVoiceInstance *gLibVoiceInstance = NULL;
LibVoiceInstance gLibVoiceInstanceStorage;
uint32_t gVoiceStartReserved = 0;

int voiceSendCommand(VoiceCommand *cmd)
{
	return sysVoiceDispatchCommand(&gSysVoice, cmd);
}

int sceVoiceInit(SceVoiceInitParam *pArg, SceVoiceVersion version)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance != NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_INITIALIZED;
	}
	if (pArg == NULL) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	gLibVoiceInstance = &gLibVoiceInstanceStorage;
	cmd.commandId = VOICE_CMD_INIT;
	cmd.payload.init.initParam = *pArg;
	cmd.payload.init.version = version;
	cmd.payload.init.libInstance = &gLibVoiceInstanceStorage;

	int ret = sysVoiceDispatchCommand(&gSysVoice, &cmd);
	if (ret != 0) {
		gLibVoiceInstance = NULL;
		return ret;
	}

	libVoiceInitUserLock(gLibVoiceInstance);
	return 0;
}

int sceVoiceEnd(void)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}

	cmd.commandId = VOICE_CMD_END;
	cmd.instanceId = gLibVoiceInstance->instanceId;

	int ret = sysVoiceDispatchCommand(&gSysVoice, &cmd);
	if (ret == 0) {
		libVoiceTermUserLock(gLibVoiceInstance);
		gLibVoiceInstance = NULL;
	}
	return ret;
}

int sceVoiceStart(SceVoiceStartParam *pArg)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (pArg == NULL) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceStart");
	gVoiceStartReserved = 0;
	cmd.commandId = VOICE_CMD_START;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.start.startParam = *pArg;
	cmd.payload.start.flags = 0;

	int ret = sysVoiceDispatchCommand(&gSysVoice, &cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceStop(void)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceStop");
	cmd.commandId = VOICE_CMD_STOP;
	cmd.instanceId = gLibVoiceInstance->instanceId;

	int ret = sysVoiceDispatchCommand(&gSysVoice, &cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceGetResourceInfo(SceVoiceResourceInfo *pInfo)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (pInfo == NULL) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceGetResourceInfo");
	cmd.commandId = VOICE_CMD_GET_RESOURCE_INFO;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.resourceInfo.info = pInfo;

	int ret = sysVoiceDispatchCommand(&gSysVoice, &cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceCheckTopology(void)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}

	libVoiceLockUser(gLibVoiceInstance, "SceVoiceCheckTopology");
	cmd.commandId = VOICE_CMD_CHECK_TOPOLOGY;
	cmd.instanceId = gLibVoiceInstance->instanceId;

	int ret = sysVoiceDispatchCommand(&gSysVoice, &cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceSetMuteFlagAll(uint16_t bMuted)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceSetMuteFlagAll");
	cmd.commandId = VOICE_CMD_SET_MUTE_ALL;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.muteAll.muted = (bMuted != 0) ? 1 : 0;

	int ret = voiceSendCommand(&cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceSetMuteFlag(uint32_t portId, uint16_t bMuted)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (portId >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceSetMuteFlag");
	cmd.commandId = VOICE_CMD_SET_MUTE;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.mute.portId = portId;
	cmd.payload.mute.muted = (bMuted != 0) ? 1 : 0;

	int ret = voiceSendCommand(&cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceGetMuteFlag(uint32_t portId, uint16_t *bMuted)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (portId >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}
	if (bMuted == NULL) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceSetMuteFlag");
	cmd.commandId = VOICE_CMD_GET_MUTE;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.mute.portId = portId;

	int ret = voiceSendCommand(&cmd);
	if (ret == 0) {
		*bMuted = cmd.payload.mute.muted;
	}
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceSetVolume(uint32_t portId, float volume)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (portId >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceSetVolume");
	cmd.commandId = VOICE_CMD_SET_VOLUME;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.volume.portId = portId;
	cmd.payload.volume.volume = volume;

	int ret = voiceSendCommand(&cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceGetVolume(uint32_t portId, float *volume)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (portId >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}
	if (volume == NULL) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceGetVolume");
	cmd.commandId = VOICE_CMD_GET_VOLUME;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.volume.portId = portId;

	int ret = voiceSendCommand(&cmd);
	if (ret == 0) {
		*volume = cmd.payload.volume.volume;
	}
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceSetBitRate(uint32_t portId, SceVoiceBitRate bitrate)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (portId >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceSetBitRate");
	cmd.commandId = VOICE_CMD_SET_BITRATE;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.bitrate.portId = portId;
	cmd.payload.bitrate.bitrate = (uint32_t)bitrate;

	int ret = voiceSendCommand(&cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceGetBitRate(uint32_t portId, uint32_t *bitrate)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (portId >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}
	if (bitrate == NULL) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceGetBitRate");
	cmd.commandId = VOICE_CMD_GET_BITRATE;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.bitrate.portId = portId;

	int ret = voiceSendCommand(&cmd);
	if (ret == 0) {
		*bitrate = cmd.payload.bitrate.bitrate;
	}
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceSetPortAttr(uint32_t portId, SceVoicePortAttr attr, void *attrValue)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (portId >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}
	if (attrValue == NULL) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceSetPortAttr");
	cmd.commandId = VOICE_CMD_SET_PORT_ATTR;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.portAttr.portId = portId;
	cmd.payload.portAttr.attr = attr;
	cmd.payload.portAttr.setValue = *(const int32_t *)attrValue;

	int ret = voiceSendCommand(&cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceGetPortAttr(uint32_t portId, SceVoicePortAttr attr, void *attrValue)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (portId >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}
	if (attrValue == NULL) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceGetPortAttr");
	cmd.commandId = VOICE_CMD_GET_PORT_ATTR;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.portAttr.portId = portId;
	cmd.payload.portAttr.attr = attr;

	int ret = voiceSendCommand(&cmd);
	if (ret == 0) {
		*(int32_t *)attrValue = cmd.payload.portAttr.getValue;
	}
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

void voiceSharedPortInit(VoiceSharedPort *sharedPort, const VoicePort *port, intptr_t addrDelta)
{
	sharedPort->portId = port->portId;
	sharedPort->portType = port->param.portType;
	sharedPort->portState = port->state;
	sharedPort->dataRing = port->sharedRing;
	sharedPort->partialBytes = 0;

	voiceRingInit(&sharedPort->dataRing, port->sharedRing.buffer + addrDelta, port->sharedRing.capacity);

	SceVoicePortType type = sharedPort->portType;
	if (type == SCEVOICE_PORTTYPE_IN_VOICE || type == SCEVOICE_PORTTYPE_OUT_VOICE) {
		sharedPort->frameSize = port->frameSize;
		if (type == SCEVOICE_PORTTYPE_IN_VOICE) {
			voiceRingInit(&sharedPort->indexRing, port->indexRing.buffer + addrDelta,
			              port->indexRing.capacity);
		}
	} else {
		sharedPort->frameSize = 2;
	}

	if (type == SCEVOICE_PORTTYPE_IN_PCMAUDIO || type == SCEVOICE_PORTTYPE_OUT_PCMAUDIO) {
		sharedPort->pcmDataType = (int32_t)port->param.pcmaudio.format.dataType;
	} else {
		sharedPort->pcmDataType = -1;
	}
}

void voiceSharedPortTerm(VoiceSharedPort *sharedPort)
{
	sharedPort->portType = SCEVOICE_PORTTYPE_NULL;
	voiceRingTerm(&sharedPort->dataRing);
}

void voiceSharedPortReset(VoiceSharedPort *sharedPort)
{
	voiceRingReset(&sharedPort->dataRing);
	sharedPort->partialBytes = 0;
	if (sharedPort->portType == SCEVOICE_PORTTYPE_IN_VOICE) {
		voiceRingReset(&sharedPort->indexRing);
	}
}

void voiceSharedPortWriteIndexedFrame(VoiceSharedPort *sharedPort, const void *src, uint32_t size,
                                      uint16_t *frameOffset, uint16_t indexOffset)
{
	voiceRingWriteAt(&sharedPort->dataRing, *frameOffset, src, size);
	voiceRingWriteAt(&sharedPort->indexRing, indexOffset, frameOffset, sizeof(uint16_t));
}

int voiceSharedPortWrite(VoiceSharedPort *sharedPort, const void *data, uint32_t *size, int16_t frameGaps)
{
	if ((uint32_t)sharedPort->portType > SCEVOICE_PORTTYPE_IN_VOICE) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	if (sharedPort->portType != SCEVOICE_PORTTYPE_IN_VOICE) {
		*size = voiceRingWrite(&sharedPort->dataRing, data, *size);
		return 0;
	}

	const uint8_t *src = (const uint8_t *)data;
	uint32_t written = 0;
	uint8_t zeroFrame[64];

	if (sharedPort->partialBytes != 0) {
		uint32_t need = (uint8_t)(sharedPort->frameSize - sharedPort->partialBytes);
		uint32_t take = (*size < need) ? *size : need;
		voiceMemcpy(sharedPort->partialBuf + sharedPort->partialBytes, src, take);
		src += take;
		*size -= take;
		written = take;
		sharedPort->partialBytes += take;

		if (sharedPort->partialBytes == sharedPort->frameSize) {
			uint32_t cap = sharedPort->dataRing.capacity;
			uint32_t freeBytes = (uint16_t)(cap - (uint16_t)(sharedPort->dataRing.writeCount -
			                                                 sharedPort->dataRing.readCount));
			if (sharedPort->partialBytes > freeBytes) {
				*size = written;
				return 0;
			}
			uint16_t offset = (uint16_t)(sharedPort->dataRing.writeIdx % cap);
			voiceRingWrite(&sharedPort->dataRing, sharedPort->partialBuf, sharedPort->partialBytes);
			voiceRingWrite(&sharedPort->indexRing, &offset, sizeof(offset));
			sharedPort->partialBytes = 0;
		}
	}

	int16_t backIdx = 0;
	if (frameGaps >= 0) {
		uint32_t idxFree = (uint16_t)(sharedPort->indexRing.capacity -
		                              (uint16_t)(sharedPort->indexRing.writeCount -
		                                         sharedPort->indexRing.readCount));
		if (idxFree < (uint32_t)(2 * (frameGaps + 1))) {
			*size = written;
			return 0;
		}
	} else {
		uint32_t idxCap = sharedPort->indexRing.capacity;
		int32_t rel = (int32_t)(sharedPort->indexRing.writeIdx % idxCap) + 2 * frameGaps;
		int32_t rem = rel % (int32_t)idxCap;
		if (rem < 0) {
			rem += (int32_t)idxCap;
		}
		backIdx = (int16_t)rem;
	}

	while (*size >= sharedPort->frameSize) {
		uint32_t cap = sharedPort->dataRing.capacity;
		uint32_t freeBytes = (uint16_t)(cap - (uint16_t)(sharedPort->dataRing.writeCount -
		                                                 sharedPort->dataRing.readCount));
		if (sharedPort->frameSize > freeBytes) {
			*size = written;
			return 0;
		}

		if (frameGaps < 0) {
			uint16_t existing = 0;
			voiceMemcpy(&existing, sharedPort->indexRing.buffer + backIdx, sizeof(existing));
			if ((existing & 0xF000u) != 0) {
				existing &= 0x0FFFu;
				voiceSharedPortWriteIndexedFrame(sharedPort, src, sharedPort->frameSize,
				                                 &existing, (uint16_t)backIdx);
			}
			backIdx = (int16_t)((backIdx + 2) % (int32_t)sharedPort->indexRing.capacity);
		} else {
			while (frameGaps > 0) {
				uint16_t lostOffset = (uint16_t)((sharedPort->dataRing.writeIdx % cap) | 0xF000u);
				voiceRingWrite(&sharedPort->dataRing, zeroFrame, sharedPort->frameSize);
				voiceRingWrite(&sharedPort->indexRing, &lostOffset, sizeof(lostOffset));
				frameGaps--;
			}
			uint16_t frameOffset = (uint16_t)(sharedPort->dataRing.writeIdx % cap);
			voiceRingWrite(&sharedPort->dataRing, src, sharedPort->frameSize);
			voiceRingWrite(&sharedPort->indexRing, &frameOffset, sizeof(frameOffset));
		}

		uint32_t step = sharedPort->frameSize;
		*size -= step;
		src += step;
		written += step;
	}

	if (*size != 0) {
		sharedPort->partialBytes = *size;
		written += *size;
		voiceMemcpy(sharedPort->partialBuf, src, *size);
	}

	*size = written;
	return 0;
}

int voiceSharedPortRead(VoiceSharedPort *sharedPort, void *data, uint32_t *size)
{
	if ((uint32_t)(sharedPort->portType - SCEVOICE_PORTTYPE_OUT_PCMAUDIO) > 2u) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	uint32_t alignedReq;
	if (sharedPort->pcmDataType != 0) {
		uint32_t unit = sharedPort->frameSize;
		alignedReq = unit * (*size / unit);
	} else {
		alignedReq = *size & ~1u;
	}

	*size = alignedReq;
	uint32_t actual = voiceRingRead(&sharedPort->dataRing, data, alignedReq);
	*size = actual;
	if ((actual % sharedPort->frameSize) != 0) {
		*size = 0;
	}
	return 0;
}

int libVoiceInitUserLock(LibVoiceInstance *inst)
{
	inst->userLockCount = 0;
	return sceKernelCreateLwMutex(&inst->userMutex, "SceSysvoiceUser", 0, 0, NULL);
}

int libVoiceTermUserLock(LibVoiceInstance *inst)
{
	return sceKernelDeleteLwMutex(&inst->userMutex);
}

int libVoiceLockUser(LibVoiceInstance *inst, const char *owner)
{
	int ret = sceKernelLockLwMutex(&inst->userMutex, 1, NULL);
	inst->userLockOwner = owner;
	inst->userLockCount++;
	return ret;
}

int libVoiceUnlockUser(LibVoiceInstance *inst)
{
	inst->userLockOwner = "<none>";
	inst->userLockCount--;
	return sceKernelUnlockLwMutex(&inst->userMutex, 1);
}

int libVoiceInitDataLock(LibVoiceInstance *inst, uint32_t instanceId)
{
	inst->instanceId = instanceId;
	inst->attached = 0;
	inst->dataLockCount = 0;
	return sceKernelCreateLwMutex(&inst->dataMutex, "SceSysvoiceData", 0, 0, NULL);
}

int libVoiceTermDataLock(LibVoiceInstance *inst)
{
	int ret = sceKernelDeleteLwMutex(&inst->dataMutex);
	inst->instanceId = 0;
	return ret;
}

int libVoiceLockData(LibVoiceInstance *inst, const char *owner)
{
	int ret = sceKernelLockLwMutex(&inst->dataMutex, 1, NULL);
	inst->dataLockOwner = owner;
	inst->dataLockCount++;
	return ret;
}

int libVoiceUnlockData(LibVoiceInstance *inst)
{
	inst->dataLockOwner = "<none>";
	inst->dataLockCount--;
	return sceKernelUnlockLwMutex(&inst->dataMutex, 1);
}

int libVoiceGetPortInfo(LibVoiceInstance *inst, uint32_t portId, SceVoiceBasePortInfo *info)
{
	sceKernelLockLwMutex(&inst->dataMutex, 1, NULL);
	inst->dataLockOwner = "LibVoiceInstance::GetPortInfo";
	inst->dataLockCount++;

	VoiceSharedPortTable *table = inst->sharedPortTable;
	if (table == NULL) {
		voiceMemset(info, 0, 24);
		info->state = SCEVOICE_PORTSTATE_IDLE;
		inst->dataLockOwner = "<none>";
		inst->dataLockCount--;
		sceKernelUnlockLwMutex(&inst->dataMutex, 1);
		return SCE_VOICE_ERROR_SERVICE_DETACHED;
	}

	if (portId >= table->maxPorts || table->ports[portId].portId != portId) {
		inst->dataLockCount--;
		inst->dataLockOwner = "<none>";
		sceKernelUnlockLwMutex(&inst->dataMutex, 1);
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	VoiceSharedPort *sharedPort = &table->ports[portId];
	if ((uint32_t)sharedPort->portType > SCEVOICE_PORTTYPE_IN_VOICE) {
		info->numByte = (uint16_t)(sharedPort->dataRing.writeCount - sharedPort->dataRing.readCount);
	} else {
		uint32_t unit = sharedPort->frameSize;
		uint16_t freeBytes = (uint16_t)(sharedPort->dataRing.capacity -
		                                (uint16_t)(sharedPort->dataRing.writeCount -
		                                           sharedPort->dataRing.readCount));
		info->numByte = unit * (uint16_t)(freeBytes / unit);
	}

	info->portType = sharedPort->portType;
	info->state = sharedPort->portState;
	info->frameSize = sharedPort->frameSize;

	inst->dataLockOwner = "<none>";
	inst->dataLockCount--;
	sceKernelUnlockLwMutex(&inst->dataMutex, 1);
	return 0;
}

int libVoiceWriteToPort(LibVoiceInstance *inst, uint32_t portId, const void *data, uint32_t *size, int16_t frameGaps)
{
	sceKernelLockLwMutex(&inst->dataMutex, 1, NULL);
	inst->dataLockOwner = "LibVoiceInstance::WriteToPort";
	inst->dataLockCount++;

	VoiceSharedPortTable *table = inst->sharedPortTable;
	if (table == NULL) {
		inst->dataLockCount--;
		inst->dataLockOwner = "<none>";
		sceKernelUnlockLwMutex(&inst->dataMutex, 1);
		*size = 0;
		return SCE_VOICE_ERROR_SERVICE_DETACHED;
	}

	if (portId >= table->maxPorts || table->ports[portId].portId != portId) {
		inst->dataLockCount--;
		inst->dataLockOwner = "<none>";
		sceKernelUnlockLwMutex(&inst->dataMutex, 1);
		*size = 0;
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	VoiceSharedPort *sharedPort = &table->ports[portId];
	if ((uint32_t)(sharedPort->portType - SCEVOICE_PORTTYPE_IN_PCMAUDIO) > 1u) {
		inst->dataLockCount--;
		inst->dataLockOwner = "<none>";
		sceKernelUnlockLwMutex(&inst->dataMutex, 1);
		*size = 0;
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	int ret = voiceSharedPortWrite(sharedPort, data, size, frameGaps);
	inst->dataLockOwner = "<none>";
	inst->dataLockCount--;
	sceKernelUnlockLwMutex(&inst->dataMutex, 1);
	return ret;
}

int libVoiceReadFromPort(LibVoiceInstance *inst, uint32_t portId, void *data, uint32_t *size)
{
	sceKernelLockLwMutex(&inst->dataMutex, 1, NULL);
	inst->dataLockOwner = "LibVoiceInstance::ReadFromPort";
	inst->dataLockCount++;

	VoiceSharedPortTable *table = inst->sharedPortTable;
	if (table == NULL) {
		inst->dataLockCount--;
		inst->dataLockOwner = "<none>";
		sceKernelUnlockLwMutex(&inst->dataMutex, 1);
		*size = 0;
		return SCE_VOICE_ERROR_SERVICE_DETACHED;
	}

	if (portId >= table->maxPorts || table->ports[portId].portId != portId) {
		inst->dataLockCount--;
		inst->dataLockOwner = "<none>";
		sceKernelUnlockLwMutex(&inst->dataMutex, 1);
		*size = 0;
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	VoiceSharedPort *sharedPort = &table->ports[portId];
	if ((uint32_t)sharedPort->portType <= SCEVOICE_PORTTYPE_IN_VOICE) {
		inst->dataLockCount--;
		inst->dataLockOwner = "<none>";
		sceKernelUnlockLwMutex(&inst->dataMutex, 1);
		*size = 0;
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	if (sharedPort->portType == SCEVOICE_PORTTYPE_OUT_DEVICE) {
		inst->dataLockCount--;
		inst->dataLockOwner = "<none>";
		sceKernelUnlockLwMutex(&inst->dataMutex, 1);
		*size = 0;
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	int ret = voiceSharedPortRead(sharedPort, data, size);
	inst->dataLockOwner = "<none>";
	inst->dataLockCount--;
	sceKernelUnlockLwMutex(&inst->dataMutex, 1);
	return ret;
}

int sceVoiceCreatePort(uint32_t *portId, const SceVoicePortParam *pArg)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (portId == NULL || pArg == NULL) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceCreatePort");
	cmd.commandId = VOICE_CMD_CREATE_PORT;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.portOp.param = *pArg;

	int ret = voiceSendCommand(&cmd);
	*portId = cmd.payload.portOp.portId;
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceUpdatePort(uint32_t portId, const SceVoicePortParam *pArg)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (portId >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}
	if (pArg == NULL) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceUpdatePort");
	cmd.commandId = VOICE_CMD_UPDATE_PORT;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.portOp.portId = portId;
	cmd.payload.portOp.param = *pArg;

	int ret = voiceSendCommand(&cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceConnectIPortToOPort(uint32_t ips, uint32_t ops)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (ips >= VOICE_MAX_PORTS || ops >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceConnectIPortToOPort");
	cmd.commandId = VOICE_CMD_CONNECT_PORT;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.connect.iPortId = ips;
	cmd.payload.connect.oPortId = ops;

	int ret = voiceSendCommand(&cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceDisconnectIPortFromOPort(uint32_t ips, uint32_t ops)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (ips >= VOICE_MAX_PORTS || ops >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceDisconnectIPortFromOPort");
	cmd.commandId = VOICE_CMD_DISCONNECT_PORT;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.connect.iPortId = ips;
	cmd.payload.connect.oPortId = ops;

	int ret = voiceSendCommand(&cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceDeletePort(uint32_t portId)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (portId >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceDeletePort");
	cmd.commandId = VOICE_CMD_DELETE_PORT;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.portOp.portId = portId;

	int ret = voiceSendCommand(&cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceWriteToIPort(uint32_t ips, const void *data, uint32_t *size, int16_t frameGaps)
{
	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (ips >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}
	if (size == NULL || data == NULL) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}
	return libVoiceWriteToPort(gLibVoiceInstance, (uint16_t)ips, data, size, frameGaps);
}

int sceVoiceReadFromOPort(uint32_t ops, void *data, uint32_t *size)
{
	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (ops >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}
	if (size == NULL || data == NULL) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}
	return libVoiceReadFromPort(gLibVoiceInstance, (uint16_t)ops, data, size);
}

int sceVoiceGetPortInfo(uint32_t portId, SceVoiceBasePortInfo *pInfo)
{
	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (portId >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}
	if (pInfo == NULL) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}
	return libVoiceGetPortInfo(gLibVoiceInstance, (uint16_t)portId, pInfo);
}

int sceVoiceResetPort(uint32_t portId)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (portId >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceResetPort");
	cmd.commandId = VOICE_CMD_RESET_PORT;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.portOp.portId = portId;

	int ret = voiceSendCommand(&cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoicePausePort(uint32_t portId)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (portId >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoicePausePort");
	cmd.commandId = VOICE_CMD_PAUSE_PORT;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.portOp.portId = portId;

	int ret = voiceSendCommand(&cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceResumePort(uint32_t portId)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}
	if (portId >= VOICE_MAX_PORTS) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceResumePort");
	cmd.commandId = VOICE_CMD_RESUME_PORT;
	cmd.instanceId = gLibVoiceInstance->instanceId;
	cmd.payload.portOp.portId = portId;

	int ret = voiceSendCommand(&cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoicePausePortAll(void)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoicePausePortAll");
	cmd.commandId = VOICE_CMD_PAUSE_PORT_ALL;
	cmd.instanceId = gLibVoiceInstance->instanceId;

	int ret = voiceSendCommand(&cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}

int sceVoiceResumePortAll(void)
{
	VoiceCommand cmd;

	if (gLibVoiceInstance == NULL) {
		return SCE_VOICE_ERROR_LIBVOICE_NOT_INIT;
	}

	libVoiceLockUser(gLibVoiceInstance, "sceVoiceResumePortAll");
	cmd.commandId = VOICE_CMD_RESUME_PORT_ALL;
	cmd.instanceId = gLibVoiceInstance->instanceId;

	int ret = voiceSendCommand(&cmd);
	libVoiceUnlockUser(gLibVoiceInstance);
	return ret;
}
