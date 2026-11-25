/*
 * SysVoiceInstance port scheduling, topology management, and per-tick processing.
 */

#include "voice_internal.h"

int sysVoiceInstanceLockData(SysVoiceInstance *inst, const char *owner)
{
	return libVoiceLockData(inst->libInstance, owner);
}

int sysVoiceInstanceUnlockData(SysVoiceInstance *inst)
{
	return libVoiceUnlockData(inst->libInstance);
}

int sysVoiceInstanceInit(SysVoiceInstance *inst, const VoiceCommand *cmd, uint32_t instanceId)
{
	inst->libInstance = cmd->payload.init.libInstance;
	inst->version = cmd->payload.init.version;
	inst->appType = cmd->payload.init.initParam.appType;

	libVoiceInitDataLock(inst->libInstance, instanceId);
	libVoiceLockData(inst->libInstance, "SysVoiceInstance::Init");

	inst->edgeCount = 0;
	inst->portCount = 0;
	inst->muteAll = 0;
	inst->attached = 0;
	inst->pausedAll = 0;
	inst->sharedMemStart = NULL;
	voiceListInit(&inst->inputPortList);
	voiceListInit(&inst->outputPortList);

	sysVoiceInitResourceLimits(&inst->limits, inst->appType);
	inst->pcmMemUsed = 0;
	inst->pcmPortCount = 0;
	inst->inVoiceCount = 0;
	inst->outVoiceCount = 0;
	inst->inDeviceCount = 0;
	inst->outDeviceCount = 0;
	return 0;
}

int sysVoiceInstanceTerm(SysVoiceInstance *inst)
{
	int ret = libVoiceTermDataLock(inst->libInstance);
	inst->libInstance = NULL;
	return ret;
}

int sysVoiceInstanceReservePort(SysVoiceInstance *inst, const SceVoicePortParam *param,
                                VoicePortFormatMetrics *metrics)
{
	if (inst->portCount == inst->limits.maxTotalPort) {
		return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
	}

	switch (param->portType) {
	case SCEVOICE_PORTTYPE_IN_DEVICE:
		metrics->bufferBytes = 4096;
		if (inst->inDeviceCount == inst->limits.maxDevicePort) {
			return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
		}
		if (param->device.playerId != 0) {
			return SCE_VOICE_ERROR_ARGUMENT_INVALID;
		}
		inst->inDeviceCount++;
		break;

	case SCEVOICE_PORTTYPE_IN_PCMAUDIO:
	case SCEVOICE_PORTTYPE_OUT_PCMAUDIO: {
		uint32_t frameBytes = metrics->frameSize;
		uint16_t bufBytes = (uint16_t)((param->pcmaudio.bufSize / frameBytes) * frameBytes);
		metrics->bufferBytes = bufBytes;
		if ((uint16_t)(bufBytes - 1024u) > 0xF800u) {
			return SCE_VOICE_ERROR_ARGUMENT_INVALID;
		}
		if (param->portType == SCEVOICE_PORTTYPE_IN_PCMAUDIO &&
		    (uint32_t)(uint16_t)(bufBytes / frameBytes) * (uint32_t)metrics->frameDurationMs <= param->threshold) {
			return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
		}
		if (inst->pcmPortCount == inst->limits.maxPcmPort) {
			return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
		}
		if (inst->pcmMemUsed + 2u * bufBytes > inst->limits.maxPcmMemBytes) {
			return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
		}
		inst->pcmPortCount++;
		inst->pcmMemUsed += 2u * metrics->bufferBytes;
		break;
	}

	case SCEVOICE_PORTTYPE_IN_VOICE:
		metrics->bufferBytes = 2048;
		if ((uint32_t)(uint16_t)(2048u / metrics->frameSize) * (uint32_t)metrics->frameDurationMs <= param->threshold) {
			return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
		}
		if (inst->inVoiceCount == inst->limits.maxInVoicePort) {
			return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
		}
		inst->inVoiceCount++;
		break;

	case SCEVOICE_PORTTYPE_OUT_VOICE:
		metrics->bufferBytes = 2048;
		if (inst->outVoiceCount == inst->limits.maxOutVoicePort) {
			return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
		}
		inst->outVoiceCount++;
		break;

	case SCEVOICE_PORTTYPE_OUT_DEVICE:
		metrics->bufferBytes = 4096;
		if (inst->outDeviceCount == inst->limits.maxDevicePort) {
			return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
		}
		if (param->device.playerId != 0) {
			return SCE_VOICE_ERROR_ARGUMENT_INVALID;
		}
		inst->outDeviceCount++;
		break;

	default:
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	inst->portCount++;
	return 0;
}

int sysVoiceInstanceReleasePort(SysVoiceInstance *inst, const VoicePort *port)
{
	switch (port->param.portType) {
	case SCEVOICE_PORTTYPE_IN_DEVICE:
		inst->inDeviceCount--;
		break;
	case SCEVOICE_PORTTYPE_IN_PCMAUDIO:
	case SCEVOICE_PORTTYPE_OUT_PCMAUDIO:
		inst->pcmPortCount--;
		inst->pcmMemUsed -= 2u * port->bufferBytes;
		break;
	case SCEVOICE_PORTTYPE_IN_VOICE:
		inst->inVoiceCount--;
		break;
	case SCEVOICE_PORTTYPE_OUT_VOICE:
		inst->outVoiceCount--;
		break;
	case SCEVOICE_PORTTYPE_OUT_DEVICE:
		inst->outDeviceCount--;
		break;
	default:
		return SCE_VOICE_ERROR_ARGUMENT_INVALID;
	}

	inst->portCount--;
	return 0;
}

void sysVoiceInstanceNoop(void)
{
}

VoiceSharedPort *sysVoiceInstanceGetSharedPort(SysVoiceInstance *inst, uint32_t portId)
{
	if (portId >= VOICE_MAX_PORTS) {
		return NULL;
	}
	intptr_t delta = (uint8_t *)inst->sharedMemBase - (uint8_t *)inst->sharedMemStart;
	VoiceSharedPortTable *table = (VoiceSharedPortTable *)((uint8_t *)inst->libInstance->sharedPortTable + delta);
	return &table->ports[portId];
}

void sysVoiceInstanceAddPort(SysVoiceInstance *inst, VoicePort *port)
{
	if ((uint32_t)port->param.portType > SCEVOICE_PORTTYPE_IN_VOICE) {
		voiceListPushFront(&inst->outputPortList, &port->node);
	} else {
		voiceListPushFront(&inst->inputPortList, &port->node);
	}
}

void sysVoiceInstanceRemovePort(SysVoiceInstance *inst, VoicePort *port)
{
	if ((uint32_t)port->param.portType > SCEVOICE_PORTTYPE_IN_VOICE) {
		voiceListRemove(&inst->outputPortList, &port->node);
	} else {
		voiceListRemove(&inst->inputPortList, &port->node);
	}
}

void sysVoiceInstanceDeduplicateOutputs(SysVoiceInstance *inst)
{
	for (VoicePort *p = (VoicePort *)inst->outputPortList.head;
	     p != NULL;
	     p = (VoicePort *)p->node.next) {
		p->sharedOwnerPortId = VOICE_NO_OWNER_PORT;
	}

	for (VoicePort *primary = (VoicePort *)inst->outputPortList.head;
	     primary != NULL;
	     primary = (VoicePort *)primary->node.next) {
		for (VoicePort *dup = (VoicePort *)primary->node.next;
		     dup != NULL && dup->sharedOwnerPortId == VOICE_NO_OWNER_PORT;
		     dup = (VoicePort *)dup->node.next) {
			if (voicePortHasSameTopology(dup, primary) != 0) {
				if (dup->param.portType == SCEVOICE_PORTTYPE_OUT_VOICE &&
				    dup->codecSlotId != VOICE_UNBOUND_SLOT) {
					voicePortReleaseEncoder(dup);
				}
				dup->sharedOwnerPortId = primary->portId;
			}
		}
	}
}

int sysVoiceInstanceConnectPorts(SysVoiceInstance *inst, VoicePort *iPort, VoicePort *oPort)
{
	if ((uint32_t)iPort->param.portType > SCEVOICE_PORTTYPE_IN_VOICE) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	if ((uint32_t)oPort->param.portType <= SCEVOICE_PORTTYPE_IN_VOICE) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	if (iPort->param.portType == SCEVOICE_PORTTYPE_IN_VOICE &&
	    oPort->param.portType == SCEVOICE_PORTTYPE_OUT_VOICE) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}
	if (inst->edgeCount == inst->limits.maxEdges) {
		return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
	}

	for (VoiceEdge *edge = (VoiceEdge *)oPort->edgeList.head;
	     edge != NULL;
	     edge = (VoiceEdge *)edge->node.next) {
		if (edge->iPortId == iPort->portId) {
			return SCE_VOICE_ERROR_TOPOLOGY;
		}
	}

	VoiceEdge *edge = (VoiceEdge *)voiceListPopFront(&gSysVoice.freeEdgeList);
	if (edge == NULL) {
		return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
	}

	edge->iPortId = iPort->portId;
	edge->oPortId = oPort->portId;
	iPort->edgeCount++;
	oPort->edgeCount++;
	voiceListPushBack(&oPort->edgeList, &edge->node);

	sysVoiceInstanceDeduplicateOutputs(inst);
	inst->edgeCount++;
	return 0;
}

int sysVoiceInstanceDisconnectPorts(SysVoiceInstance *inst, VoicePort *iPort, VoicePort *oPort)
{
	if ((uint32_t)iPort->param.portType > SCEVOICE_PORTTYPE_IN_VOICE ||
	    (uint32_t)oPort->param.portType <= SCEVOICE_PORTTYPE_IN_VOICE) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	VoiceEdge *edge = (VoiceEdge *)oPort->edgeList.head;
	while (edge != NULL &&
	       (edge->iPortId != iPort->portId || edge->oPortId != oPort->portId)) {
		edge = (VoiceEdge *)edge->node.next;
	}
	if (edge == NULL) {
		return SCE_VOICE_ERROR_TOPOLOGY;
	}

	voiceListRemove(&oPort->edgeList, &edge->node);
	voiceListPushFront(&gSysVoice.freeEdgeList, &edge->node);
	iPort->edgeCount--;
	oPort->edgeCount--;

	sysVoiceInstanceDeduplicateOutputs(inst);
	inst->edgeCount--;
	return 0;
}

void *sysVoiceInstanceAllocSharedTable(SysVoiceInstance *inst)
{
	intptr_t delta = (uint8_t *)inst->sharedMemStart - (uint8_t *)inst->sharedMemBase;
	VoiceSharedPortTable *table = (VoiceSharedPortTable *)sysVoiceAllocMem(
		&gSysVoice, sizeof(VoiceSharedPortTable), "LibVoice Userspace Instance", 0);
	if (table != NULL) {
		voiceMemset(table, 0, sizeof(VoiceSharedPortTable));
		table->maxPorts = VOICE_MAX_PORTS;
		VoiceSharedPortTable *userTable = (VoiceSharedPortTable *)((uint8_t *)table + delta);
		inst->libInstance->sharedPortTable = userTable;
		userTable->maxPorts = table->maxPorts;
	}
	return table;
}

void sysVoiceInstanceFreeSharedTable(SysVoiceInstance *inst)
{
	VoiceSharedPortTable *userTable = inst->libInstance->sharedPortTable;
	if (userTable != NULL) {
		intptr_t delta = (uint8_t *)inst->sharedMemStart - (uint8_t *)inst->sharedMemBase;
		sysVoiceFreeMem(&gSysVoice, (uint8_t *)userTable - delta, 0);
		inst->libInstance->sharedPortTable = NULL;
	}
}

void sysVoiceInstanceBroadcastEncoded(SysVoiceInstance *inst, const VoicePort *srcPort,
                                      const void *encodedBuf, uint32_t encodedSize)
{
	uint8_t alignedBuf[644];

	for (VoicePort *outPort = (VoicePort *)inst->outputPortList.head;
	     outPort != NULL;
	     outPort = (VoicePort *)outPort->node.next) {
		if (outPort->sharedOwnerPortId != srcPort->portId) {
			continue;
		}
		uint8_t *dst = &alignedBuf[outPort->sharedRing.writeIdx & 0xFu];
		voiceMemcpy(dst, encodedBuf, encodedSize);
		uint16_t freeBytes = (uint16_t)(outPort->sharedRing.capacity -
		                                (uint16_t)(outPort->sharedRing.writeCount -
		                                           outPort->sharedRing.readCount));
		if (encodedSize <= freeBytes) {
			voiceRingWrite(&outPort->sharedRing, dst, encodedSize);
		}
	}
}

int16_t sysVoiceInstanceCollectDataReady(SysVoiceInstance *inst, SceVoiceEventData *eventData)
{
	for (VoicePort *outPort = (VoicePort *)inst->outputPortList.head;
	     outPort != NULL;
	     outPort = (VoicePort *)outPort->node.next) {
		voicePortSyncShared(outPort, inst);
		if ((outPort->param.portType == SCEVOICE_PORTTYPE_IN_VOICE ||
		     outPort->param.portType == SCEVOICE_PORTTYPE_OUT_VOICE) &&
		    outPort->sharedRing.writeCount != outPort->sharedRing.readCount) {
			int16_t idx = eventData->dataReady.num;
			eventData->dataReady.num = (int16_t)(idx + 1);
			eventData->dataReady.pid[idx] = outPort->portId;
		}
	}
	return eventData->dataReady.num;
}

int sysVoiceProcessAudioInPorts(SysVoiceInstance *inst)
{
	int needDelay = 1;
	if (inst->pausedAll != 0) {
		return needDelay;
	}

	for (VoicePort *port = (VoicePort *)inst->inputPortList.head;
	     port != NULL;
	     port = (VoicePort *)port->node.next) {
		if (port->param.portType != SCEVOICE_PORTTYPE_IN_DEVICE &&
		    port->param.portType != SCEVOICE_PORTTYPE_OUT_DEVICE) {
			continue;
		}
		if (port->paused != 0 || port->edgeCount == 0) {
			port->state = SCEVOICE_PORTSTATE_IDLE;
			continue;
		}

		voicePortSyncShared(port, inst);
		port->mixInputBuf = NULL;
		voiceAudioInPoolProcess(&gSysVoice.audioInPool, port);
		if (port->audioInActive != 0) {
			needDelay = 0;
		}

		if (port->state == SCEVOICE_PORTSTATE_IDLE) {
			voiceRingReset(&port->internalRing);
			port->state = SCEVOICE_PORTSTATE_READY;
		} else if (port->state == SCEVOICE_PORTSTATE_READY) {
			if (port->internalRing.writeCount != port->internalRing.readCount) {
				port->state = SCEVOICE_PORTSTATE_RUNNING;
			}
		} else if (port->state == SCEVOICE_PORTSTATE_RUNNING) {
			if (port->internalRing.writeCount == port->internalRing.readCount) {
				port->state = SCEVOICE_PORTSTATE_READY;
			}
		}
	}

	return needDelay;
}

void sysVoiceProcessInputPorts(SysVoiceInstance *inst)
{
	if (inst->pausedAll != 0) {
		return;
	}

	for (VoicePort *port = (VoicePort *)inst->inputPortList.head;
	     port != NULL;
	     port = (VoicePort *)port->node.next) {
		if (port->param.portType == SCEVOICE_PORTTYPE_IN_DEVICE ||
		    port->param.portType == SCEVOICE_PORTTYPE_OUT_DEVICE) {
			continue;
		}

		port->idleDuration += 0.016f;
		if (port->paused != 0 || port->edgeCount == 0) {
			continue;
		}

		for (;;) {
			voicePortSyncShared(port, inst);
			uint16_t internalFree = (uint16_t)(port->internalRing.capacity -
			                                   (uint16_t)(port->internalRing.writeCount -
			                                              port->internalRing.readCount));
			uint32_t maxOutFrames = (uint16_t)(internalFree / port->pcmFrameSize);
			uint32_t availInFrames = (uint16_t)((uint16_t)(port->sharedRing.writeCount -
			                                               port->sharedRing.readCount) /
			                                    port->frameSize);
			uint32_t frames = (maxOutFrames < availInFrames) ? maxOutFrames : availInFrames;
			port->mixInputBuf = NULL;

			if (port->state == SCEVOICE_PORTSTATE_READY) {
				if (port->frameSize * frames == 0) {
					break;
				}
				port->state = SCEVOICE_PORTSTATE_BUFFERING;
			} else if (port->state == SCEVOICE_PORTSTATE_BUFFERING) {
				float bufferedMs = voicePortGetBufferedMs(port);
				float thresholdMs = voicePortGetThresholdMs(port);
				if (bufferedMs < thresholdMs ||
				    port->sharedRing.writeCount == port->sharedRing.readCount) {
					if (port->idleDuration > voicePortGetThresholdMs(port) / 1000.0f) {
						port->state = SCEVOICE_PORTSTATE_READY;
					}
					break;
				}
				port->state = SCEVOICE_PORTSTATE_RUNNING;
			} else if (port->state == SCEVOICE_PORTSTATE_RUNNING) {
				if (frames != 0) {
					voiceDecoderPoolDecodeFrames(&gSysVoice.decoderPool, port, frames);
					port->idleDuration = 0.0f;
					port->activeDuration += 0.016f;
				} else if (port->idleDuration > voicePortGetThresholdMs(port) / 1000.0f) {
					port->state = SCEVOICE_PORTSTATE_BUFFERING;
				}
				break;
			} else {
				break;
			}
		}
	}
}

void sysVoiceProcessMixPorts(SysVoiceInstance *inst)
{
	if (inst->pausedAll != 0) {
		return;
	}

	voiceMixerResetCache(&gSysVoice.mixer);
	for (VoicePort *outPort = (VoicePort *)inst->outputPortList.head;
	     outPort != NULL;
	     outPort = (VoicePort *)outPort->node.next) {
		uint16_t internalUsed = (uint16_t)(outPort->internalRing.writeCount -
		                                   outPort->internalRing.readCount);
		if (outPort->sharedOwnerPortId == VOICE_NO_OWNER_PORT &&
		    internalUsed != outPort->internalRing.capacity &&
		    outPort->paused == 0) {
			voiceMixerMixPort(&gSysVoice.mixer, outPort, inst->muteAll);
		}
	}
}

void sysVoiceProcessOutputPorts(SysVoiceInstance *inst)
{
	if (inst->pausedAll != 0) {
		return;
	}

	uint32_t activeEncoders = gSysVoice.encoderPool.activeList.count;
	if (activeEncoders < inst->limits.maxEncoders) {
		for (VoicePort *outPort = (VoicePort *)inst->outputPortList.head;
		     outPort != NULL && activeEncoders < inst->limits.maxEncoders;
		     outPort = (VoicePort *)outPort->node.next) {
			if ((outPort->param.portType == SCEVOICE_PORTTYPE_IN_VOICE ||
			     outPort->param.portType == SCEVOICE_PORTTYPE_OUT_VOICE) &&
			    outPort->codecSlotId == VOICE_UNBOUND_SLOT &&
			    outPort->edgeCount != 0) {
				voicePortAcquireEncoder(outPort, inst);
				activeEncoders++;
			}
		}
	}

	for (VoicePort *outPort = (VoicePort *)inst->outputPortList.head;
	     outPort != NULL;
	     outPort = (VoicePort *)outPort->node.next) {
		if (outPort->param.portType == SCEVOICE_PORTTYPE_IN_DEVICE ||
		    outPort->param.portType == SCEVOICE_PORTTYPE_OUT_DEVICE) {
			continue;
		}
		if (outPort->paused != 0 || outPort->edgeCount == 0) {
			outPort->state = SCEVOICE_PORTSTATE_IDLE;
			continue;
		}

		for (;;) {
			voicePortSyncShared(outPort, inst);
			if (outPort->codecSlotId == VOICE_UNBOUND_SLOT) {
				break;
			}
			uint16_t usedBytes = (uint16_t)(outPort->internalRing.writeCount -
			                                outPort->internalRing.readCount);
			uint16_t frames = (uint16_t)(usedBytes / outPort->pcmFrameSize);
			if (outPort->state == SCEVOICE_PORTSTATE_IDLE) {
				outPort->state = SCEVOICE_PORTSTATE_READY;
			} else if (outPort->state == SCEVOICE_PORTSTATE_READY) {
				if (frames == 0) {
					break;
				}
				outPort->state = SCEVOICE_PORTSTATE_RUNNING;
			} else if (outPort->state == SCEVOICE_PORTSTATE_RUNNING) {
				if (frames != 0) {
					voiceEncoderPoolEncodeFrames(&gSysVoice.encoderPool, outPort, frames, inst);
				}
				break;
			} else {
				break;
			}
		}
	}

	if (gSysVoice.eventDispatcher.running != 0) {
		for (VoicePort *outPort = (VoicePort *)inst->outputPortList.head;
		     outPort != NULL;
		     outPort = (VoicePort *)outPort->node.next) {
			if ((outPort->param.portType == SCEVOICE_PORTTYPE_IN_VOICE ||
			     outPort->param.portType == SCEVOICE_PORTTYPE_OUT_VOICE) &&
			    outPort->sharedRing.writeCount != outPort->sharedRing.readCount) {
				SceVoiceEventData eventData __attribute__((aligned(32)));
				voiceMemset(&eventData, 0, sizeof(eventData));
				eventData.eventType = SCEVOICE_EVENT_DATA_READY;
				voiceEventDispatcherSend(&gSysVoice.eventDispatcher, &eventData);
				break;
			}
		}
	}
}

void sysVoiceProcessAudioOutPorts(SysVoiceInstance *inst)
{
	if (inst->pausedAll != 0) {
		return;
	}

	for (VoicePort *outPort = (VoicePort *)inst->outputPortList.head;
	     outPort != NULL;
	     outPort = (VoicePort *)outPort->node.next) {
		if (outPort->param.portType != SCEVOICE_PORTTYPE_IN_DEVICE &&
		    outPort->param.portType != SCEVOICE_PORTTYPE_OUT_DEVICE) {
			continue;
		}
		if (outPort->paused != 0 || outPort->edgeCount == 0) {
			outPort->state = SCEVOICE_PORTSTATE_IDLE;
			continue;
		}

		for (;;) {
			voicePortSyncShared(outPort, inst);
			if (outPort->state == SCEVOICE_PORTSTATE_IDLE) {
				outPort->state = SCEVOICE_PORTSTATE_READY;
			} else if (outPort->state == SCEVOICE_PORTSTATE_READY) {
				if (outPort->internalRing.writeCount == outPort->internalRing.readCount) {
					break;
				}
				outPort->state = SCEVOICE_PORTSTATE_RUNNING;
			} else if (outPort->state == SCEVOICE_PORTSTATE_RUNNING) {
				voiceAudioOutPoolProcess(&gSysVoice.audioOutPool, outPort);
				break;
			} else {
				break;
			}
		}
	}
}
