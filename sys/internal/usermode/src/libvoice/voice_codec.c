/*
 * CELP encoder/decoder slot wrappers and port property/topology command handlers.
 */

#include "voice_internal.h"

void voiceEncoderSlotInit(VoiceEncoderSlot *slot, uint16_t slotIndex)
{
	slot->slotIndex = slotIndex;
	slot->portId = VOICE_INVALID_SLOT;
	slot->pcmBuf = NULL;
}

int voiceEncoderSlotCreate(VoiceEncoderSlot *slot, VoicePort *port)
{
	slot->portId = port->portId;
	voiceRingInit(&port->internalRing, slot->pcmBuf, port->bufferBytes);
	voiceMemset(&slot->celpInfo, 0, sizeof(slot->celpInfo));
	voiceMemset(&slot->ctrl, 0, sizeof(slot->ctrl));
	slot->ctrl.pInfo = (SceAudioencInfo *)&slot->celpInfo;

	uint32_t bitrate = (uint32_t)port->param.voice.bitrate;
	switch (bitrate) {
	case SCEVOICE_BITRATE_3850:
	case SCEVOICE_BITRATE_4650:
	case SCEVOICE_BITRATE_5700:
	case SCEVOICE_BITRATE_7300:
	case VOICE_BITRATE_12200:
		slot->celpInfo.bitRate = bitrate;
		slot->ctrl.size = sizeof(SceAudioencCtrl);
		slot->ctrl.wordLength = 16;
		slot->celpInfo.size = sizeof(VoiceCelpEncInfo);
		slot->celpInfo.excitationMode = 0;
		slot->celpInfo.samplingRate = 8000;
		{
			int ret = sceAudioencCreateEncoder(&slot->ctrl, VOICE_AUDIOENC_TYPE_CELP);
			slot->ctrl.pOptInfo = NULL;
			return ret;
		}
	default:
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}
}

int voiceEncoderSlotDelete(VoiceEncoderSlot *slot, VoicePort *port)
{
	slot->portId = VOICE_INVALID_SLOT;
	voiceRingTerm(&port->internalRing);
	return sceAudioencDeleteEncoder(&slot->ctrl);
}

void *voiceEncoderSlotAllocBuf(VoiceEncoderSlot *slot)
{
	void *buf = sysVoiceAllocMem(&gSysVoice, 2048, "Decoder buffer", 1);
	slot->pcmBuf = buf;
	return buf;
}

void voiceEncoderSlotFreeBuf(VoiceEncoderSlot *slot)
{
	sysVoiceFreeMem(&gSysVoice, slot->pcmBuf, 1);
	slot->pcmBuf = NULL;
}

uint32_t voiceEncoderPassThrough(VoiceEncoderSlot *slot, VoicePort *port, const void *src, void *dst)
{
	(void)slot;
	voiceMemcpy(dst, src, port->frameSize);
	return port->frameSize;
}

uint32_t voiceEncoderEncodeFrame(VoiceEncoderSlot *slot, VoicePort *port, const void *src, void *dst)
{
	slot->ctrl.pInputPcm = (void *)src;
	slot->ctrl.pOutputEs = dst;
	int ret = sceAudioencEncode(&slot->ctrl);
	if (ret != 0) {
		return 0;
	}
	if (slot->ctrl.outputEsSize == port->frameSize) {
		return port->frameSize;
	}
	slot->ctrl.outputEsSize = 0;
	return 0;
}

void voiceDecoderSlotInit(VoiceDecoderSlot *slot, uint16_t slotIndex)
{
	slot->slotIndex = slotIndex;
	slot->portId = VOICE_INVALID_SLOT;
	slot->pcmBuf = NULL;
}

int voiceDecoderSlotCreate(VoiceDecoderSlot *slot, VoicePort *port)
{
	slot->portId = port->portId;
	voiceRingInit(&port->internalRing, slot->pcmBuf, port->bufferBytes);
	voiceMemset(&slot->celpInfo, 0, sizeof(slot->celpInfo));
	voiceMemset(&slot->ctrl, 0, sizeof(slot->ctrl));
	slot->ctrl.pInfo = (SceAudiodecInfo *)&slot->celpInfo;

	uint32_t bitrate = (uint32_t)port->param.voice.bitrate;
	switch (bitrate) {
	case SCEVOICE_BITRATE_3850:
	case SCEVOICE_BITRATE_4650:
	case SCEVOICE_BITRATE_5700:
	case SCEVOICE_BITRATE_7300:
	case VOICE_BITRATE_12200:
		slot->celpInfo.bitRate = bitrate;
		slot->ctrl.size = sizeof(SceAudiodecCtrl);
		slot->ctrl.wordLength = 16;
		slot->celpInfo.size = 20;
		slot->celpInfo.excitationMode = 0;
		slot->celpInfo.samplingRate = 8000;
		return sceAudiodecCreateDecoder(&slot->ctrl, VOICE_AUDIODEC_TYPE_CELP);
	default:
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}
}

int voiceDecoderSlotDelete(VoiceDecoderSlot *slot, VoicePort *port)
{
	slot->portId = VOICE_INVALID_SLOT;
	voiceRingTerm(&port->internalRing);
	return sceAudiodecDeleteDecoder(&slot->ctrl);
}

void *voiceDecoderSlotAllocBuf(VoiceDecoderSlot *slot)
{
	void *buf = sysVoiceAllocMem(&gSysVoice, 2048, "Decoder buffer", 1);
	slot->pcmBuf = buf;
	return buf;
}

void voiceDecoderSlotFreeBuf(VoiceDecoderSlot *slot)
{
	sysVoiceFreeMem(&gSysVoice, slot->pcmBuf, 1);
	slot->pcmBuf = NULL;
}

uint32_t voiceDecoderDecodeFrame(VoiceDecoderSlot *slot, VoicePort *port, void *src, int16_t *dst, uint8_t lostFlag)
{
	slot->ctrl.pPcm = dst;
	if (lostFlag != 0) {
		voiceMemcpy(src, slot->lastFrame, port->frameSize);
		slot->celpInfo.lostFlag = 1;
	} else {
		slot->celpInfo.lostFlag = 0;
	}
	slot->ctrl.pEs = src;
	voiceMemcpy(slot->lastFrame, src, port->frameSize);

	int ret = sceAudiodecDecode(&slot->ctrl);
	if (ret != 0) {
		return 0;
	}

	int16_t gainQ8 = 0;
	if (port->param.bMute == 0) {
		gainQ8 = (int16_t)(int32_t)(port->param.volume * 256.0f);
	}

	uint32_t sampleCount = slot->ctrl.outputPcmSize >> 1;
	for (uint32_t i = 0; i < sampleCount; i++) {
		int32_t scaled = ((int32_t)dst[i] * (int32_t)gainQ8) >> 8;
		if (scaled < -32768) {
			scaled = -32768;
		}
		if (scaled > 32767) {
			scaled = 32767;
		}
		dst[i] = (int16_t)scaled;
	}
	return slot->ctrl.outputPcmSize;
}

uint32_t voiceDecoderScalePcm(VoiceDecoderSlot *slot, VoicePort *port, const int16_t *src, int16_t *dst)
{
	(void)slot;
	int16_t gainQ8 = 0;
	if (port->param.bMute == 0) {
		gainQ8 = (int16_t)(int32_t)(port->param.volume * 256.0f);
	}

	uint32_t count = port->frameSize;
	for (uint32_t i = 0; i < count; i++) {
		int32_t scaled = ((int32_t)src[i] * (int32_t)gainQ8) >> 8;
		if (scaled < -32768) {
			scaled = -32768;
		}
		if (scaled > 32767) {
			scaled = 32767;
		}
		dst[i] = (int16_t)scaled;
	}
	return count;
}

int sysVoiceHandleSetMuteAll(SysVoice *sys, VoiceCommand *cmd)
{
	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	if (inst == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	inst->muteAll = cmd->payload.muteAll.muted;
	return 0;
}

int sysVoiceHandleSetMute(SysVoice *sys, VoiceCommand *cmd)
{
	VoicePort *port = sysVoiceGetPort(sys, cmd->payload.mute.portId);
	if (port == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	port->param.bMute = cmd->payload.mute.muted;
	return 0;
}

int sysVoiceHandleGetMute(SysVoice *sys, VoiceCommand *cmd)
{
	VoicePort *port = sysVoiceGetPort(sys, cmd->payload.mute.portId);
	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	if (port == NULL || inst == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	if (inst->muteAll != 0) {
		cmd->payload.mute.muted = 1;
	} else {
		cmd->payload.mute.muted = (port->param.bMute != 0) ? 1 : 0;
	}
	return 0;
}

int sysVoiceHandleSetVolume(SysVoice *sys, VoiceCommand *cmd)
{
	VoicePort *port = sysVoiceGetPort(sys, cmd->payload.volume.portId);
	if (port == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	port->param.volume = cmd->payload.volume.volume;
	return 0;
}

int sysVoiceHandleGetVolume(SysVoice *sys, VoiceCommand *cmd)
{
	VoicePort *port = sysVoiceGetPort(sys, cmd->payload.volume.portId);
	if (port == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	cmd->payload.volume.volume = port->param.volume;
	return 0;
}

int sysVoiceHandleSetBitRate(SysVoice *sys, VoiceCommand *cmd)
{
	VoicePort *port = sysVoiceGetPort(sys, cmd->payload.bitrate.portId);
	if (port == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	if (port->param.portType != SCEVOICE_PORTTYPE_IN_VOICE &&
	    port->param.portType != SCEVOICE_PORTTYPE_OUT_VOICE) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	if ((uint32_t)port->param.voice.bitrate == cmd->payload.bitrate.bitrate) {
		return 0;
	}

	VoiceCommand updateCmd;
	updateCmd.commandId = VOICE_CMD_UPDATE_PORT;
	updateCmd.instanceId = cmd->instanceId;
	updateCmd.payload.portOp.portId = cmd->payload.bitrate.portId;
	updateCmd.payload.portOp.param = port->param;
	updateCmd.payload.portOp.param.voice.bitrate = (SceVoiceBitRate)cmd->payload.bitrate.bitrate;
	return sysVoiceHandleUpdatePort(sys, &updateCmd);
}

int sysVoiceHandleGetBitRate(SysVoice *sys, VoiceCommand *cmd)
{
	VoicePort *port = sysVoiceGetPort(sys, cmd->payload.bitrate.portId);
	if (port == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	if (port->param.portType != SCEVOICE_PORTTYPE_IN_VOICE &&
	    port->param.portType != SCEVOICE_PORTTYPE_OUT_VOICE) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	cmd->payload.bitrate.bitrate = (uint32_t)port->param.voice.bitrate;
	return 0;
}

int sysVoiceHandleSetPortAttr(SysVoice *sys, VoiceCommand *cmd)
{
	if (sysVoiceGetInstance(sys, cmd->instanceId) == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	if (sysVoiceGetPort(sys, cmd->payload.portAttr.portId) != NULL) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}
	return SCE_VOICE_ERROR_PORT_INVALID;
}

int sysVoiceHandleGetPortAttr(SysVoice *sys, VoiceCommand *cmd)
{
	if (sysVoiceGetInstance(sys, cmd->instanceId) == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	VoicePort *port = sysVoiceGetPort(sys, cmd->payload.portAttr.portId);
	if (port == NULL) {
		return SCE_VOICE_ERROR_PORT_INVALID;
	}
	if (cmd->payload.portAttr.attr != SCEVOICE_ATTR_AUDIOINPUT_ACTIVE) {
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}
	if (port->param.portType == SCEVOICE_PORTTYPE_IN_DEVICE) {
		cmd->payload.portAttr.getValue = port->audioInActive;
	}
	return 0;
}

int sysVoiceHandleGetResourceInfo(SysVoice *sys, VoiceCommand *cmd)
{
	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	if (inst == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	SceVoiceResourceInfo *info = cmd->payload.resourceInfo.info;
	info->maxInDevicePort = inst->limits.maxDevicePort;
	info->maxOutDevicePort = inst->limits.maxDevicePort;
	info->maxInVoicePort = inst->limits.maxInVoicePort;
	info->maxOutVoicePort = inst->limits.maxOutVoicePort;
	info->maxTotalPort = inst->limits.maxTotalPort;
	return 0;
}

int sysVoiceHandleCheckTopology(SysVoice *sys, VoiceCommand *cmd)
{
	SysVoiceInstance *inst = sysVoiceGetInstance(sys, cmd->instanceId);
	if (inst == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	for (VoicePort *outPort = (VoicePort *)inst->outputPortList.head;
	     outPort != NULL;
	     outPort = (VoicePort *)outPort->node.next) {
		if (outPort->edgeCount == 0) {
			return SCE_VOICE_ERROR_TOPOLOGY;
		}
	}

	for (VoicePort *inPort = (VoicePort *)inst->inputPortList.head;
	     inPort != NULL;
	     inPort = (VoicePort *)inPort->node.next) {
		if (inPort->edgeCount == 0) {
			return SCE_VOICE_ERROR_TOPOLOGY;
		}
	}

	VoicePort *firstOut = (VoicePort *)inst->outputPortList.head;
	if (firstOut != NULL) {
		VoicePort *prev = firstOut;
		for (VoicePort *cur = (VoicePort *)firstOut->node.next;
		     cur != NULL;
		     prev = cur, cur = (VoicePort *)cur->node.next) {
			if (prev->param.portType == SCEVOICE_PORTTYPE_OUT_VOICE &&
			    cur->param.portType == SCEVOICE_PORTTYPE_OUT_VOICE) {
				if (cur->param.voice.bitrate != prev->param.voice.bitrate ||
				    (cur->param.bMute != 0) != (prev->param.bMute != 0) ||
				    cur->param.volume != prev->param.volume) {
					return SCE_VOICE_ERROR_TOPOLOGY;
				}
			}
		}
	}

	int hasInDevice = 0;
	int hasInPcm = 0;
	for (VoicePort *inPort = (VoicePort *)inst->inputPortList.head;
	     inPort != NULL;
	     inPort = (VoicePort *)inPort->node.next) {
		if (inPort->param.portType == SCEVOICE_PORTTYPE_IN_PCMAUDIO) {
			hasInPcm = 1;
		} else if (inPort->param.portType == SCEVOICE_PORTTYPE_IN_DEVICE) {
			hasInDevice = 1;
		}
	}

	if (hasInDevice && hasInPcm) {
		for (VoicePort *outPort = firstOut;
		     outPort != NULL;
		     outPort = (VoicePort *)outPort->node.next) {
			if (outPort->param.portType != SCEVOICE_PORTTYPE_OUT_VOICE) {
				continue;
			}
			for (VoiceEdge *edge = (VoiceEdge *)outPort->edgeList.head;
			     edge != NULL;
			     edge = (VoiceEdge *)edge->node.next) {
				VoicePort *src = sysVoiceGetPort(&gSysVoice, edge->iPortId);
				if (src->param.portType == SCEVOICE_PORTTYPE_IN_PCMAUDIO &&
				    outPort->codecSlotId == VOICE_UNBOUND_SLOT) {
					return SCE_VOICE_ERROR_TOPOLOGY;
				}
			}
		}
	}

	return 0;
}
