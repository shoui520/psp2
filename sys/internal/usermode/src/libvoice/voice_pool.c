/*
 * Decoder, encoder, mixer, AudioIn, and AudioOut resource pools for SceVoice.
 */

#include "voice_internal.h"
#include <arm_neon.h>

int voiceDecoderPoolInit(VoiceDecoderPool *pool)
{
	voiceListInit(&pool->freeList);
	voiceListInit(&pool->activeList);
	voiceMemset(pool->slots, 0, sizeof(pool->slots));

	for (uint16_t i = 0; i < VOICE_MAX_DECODERS; i++) {
		voiceListPushBack(&pool->freeList, &pool->slots[i].node);
	}
	for (uint16_t i = 0; i < VOICE_MAX_DECODERS; i++) {
		voiceDecoderSlotInit(&pool->slots[i], i);
	}

	SceAudiodecInitParam initParam;
	initParam.celp.size = sizeof(SceAudiodecInitStreamParam);
	initParam.celp.totalStreams = VOICE_MAX_DECODERS;
	return sceAudiodecInitLibrary(VOICE_AUDIODEC_TYPE_CELP, &initParam);
}

int voiceDecoderPoolTerm(VoiceDecoderPool *pool)
{
	for (int i = 0; i < VOICE_MAX_DECODERS; i++) {
		voiceListPopFront(&pool->freeList);
	}
	return sceAudiodecTermLibrary(VOICE_AUDIODEC_TYPE_CELP);
}

void *voiceDecoderPoolAllocSlotBuf(VoiceDecoderPool *pool, uint32_t index)
{
	return voiceDecoderSlotAllocBuf(&pool->slots[index]);
}

void voiceDecoderPoolFreeSlotBuf(VoiceDecoderPool *pool, uint32_t index)
{
	voiceDecoderSlotFreeBuf(&pool->slots[index]);
}

int voiceDecoderPoolRelease(VoiceDecoderPool *pool, VoicePort *port)
{
	VoiceDecoderSlot *slot = &pool->slots[port->codecSlotId];
	voiceListRemove(&pool->activeList, &slot->node);
	int ret = voiceDecoderSlotDelete(slot, port);
	port->codecSlotId = VOICE_UNBOUND_SLOT;
	voiceListPushFront(&pool->freeList, &slot->node);
	return ret;
}

int voiceDecoderPoolAcquire(VoiceDecoderPool *pool, VoicePort *port, const SysVoiceInstance *inst)
{
	if (pool->activeList.count >= inst->limits.maxDecoders) {
		return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
	}
	VoiceDecoderSlot *slot = (VoiceDecoderSlot *)voiceListPopFront(&pool->freeList);
	int ret = voiceDecoderSlotCreate(slot, port);
	port->codecSlotId = slot->slotIndex;
	voiceListPushBack(&pool->activeList, &slot->node);
	return ret;
}

void voiceDecoderPoolDecodeFrames(VoiceDecoderPool *pool, VoicePort *port, uint32_t frameCount)
{
	int16_t *pcmScratch = (int16_t *)pool->outputScratch;
	for (uint32_t i = 0; i < frameCount; i++) {
		uint8_t lostFlag = 0;
		if (port->param.portType == SCEVOICE_PORTTYPE_IN_VOICE ||
		    port->param.portType == SCEVOICE_PORTTYPE_OUT_VOICE) {
			uint16_t idxCap = port->indexRing.capacity;
			uint16_t idxPos = port->indexRing.readIdx;
			uint16_t entry = 0;
			voiceRingRead(&port->indexRing, &entry, sizeof(entry));
			if ((entry & 0xF000u) != 0) {
				entry &= 0x0FFFu;
				lostFlag = 1;
				voiceRingWriteAt(&port->indexRing, idxPos % idxCap, &entry, sizeof(entry));
			}
		}

		voiceRingRead(&port->sharedRing, pool->inputScratch, port->frameSize);

		uint32_t outBytes;
		if (port->param.portType == SCEVOICE_PORTTYPE_IN_VOICE ||
		    port->param.portType == SCEVOICE_PORTTYPE_OUT_VOICE) {
			VoiceDecoderSlot *slot = &pool->slots[port->codecSlotId];
			outBytes = voiceDecoderDecodeFrame(slot, port, pool->inputScratch, pcmScratch, lostFlag);
		} else {
			outBytes = voiceDecoderScalePcm((VoiceDecoderSlot *)pool->pcmPassthrough, port,
			                                (const int16_t *)pool->inputScratch, pcmScratch);
		}
		voiceRingWrite(&port->internalRing, pcmScratch, outBytes);
	}
}

int voiceEncoderPoolInit(VoiceEncoderPool *pool)
{
	voiceListInit(&pool->freeList);
	voiceListInit(&pool->activeList);
	voiceMemset(pool->slots, 0, sizeof(pool->slots));
	voiceListPushBack(&pool->freeList, &pool->slots[0].node);
	voiceEncoderSlotInit(&pool->slots[0], 0);

	SceAudioencInitParam initParam;
	initParam.celp.size = sizeof(SceAudioencInitStreamParam);
	initParam.celp.totalStreams = VOICE_MAX_ENCODERS;
	return sceAudioencInitLibrary(VOICE_AUDIOENC_TYPE_CELP, &initParam);
}

int voiceEncoderPoolTerm(VoiceEncoderPool *pool)
{
	voiceListPopFront(&pool->freeList);
	return sceAudioencTermLibrary(VOICE_AUDIOENC_TYPE_CELP);
}

void *voiceEncoderPoolAllocSlotBuf(VoiceEncoderPool *pool, uint32_t index)
{
	return voiceEncoderSlotAllocBuf(&pool->slots[index]);
}

void voiceEncoderPoolFreeSlotBuf(VoiceEncoderPool *pool, uint32_t index)
{
	voiceEncoderSlotFreeBuf(&pool->slots[index]);
}

int voiceEncoderPoolRelease(VoiceEncoderPool *pool, VoicePort *port)
{
	if (port->codecSlotId == VOICE_UNBOUND_SLOT) {
		return 0;
	}
	VoiceEncoderSlot *slot = &pool->slots[port->codecSlotId];
	voiceListRemove(&pool->activeList, &slot->node);
	int ret = voiceEncoderSlotDelete(slot, port);
	port->codecSlotId = VOICE_UNBOUND_SLOT;
	voiceListPushFront(&pool->freeList, &slot->node);
	return ret;
}

int voiceEncoderPoolAcquire(VoiceEncoderPool *pool, VoicePort *port, const SysVoiceInstance *inst)
{
	if (pool->activeList.count >= inst->limits.maxEncoders) {
		return 0;
	}
	VoiceEncoderSlot *slot = (VoiceEncoderSlot *)voiceListPopFront(&pool->freeList);
	int ret = voiceEncoderSlotCreate(slot, port);
	port->codecSlotId = slot->slotIndex;
	voiceListPushBack(&pool->activeList, &slot->node);
	return ret;
}

void voiceEncoderPoolEncodeFrames(VoiceEncoderPool *pool, VoicePort *port, uint32_t frameCount,
                                  SysVoiceInstance *inst)
{
	for (uint32_t i = 0; i < frameCount; i++) {
		voiceRingRead(&port->internalRing, pool->inputScratch, port->pcmFrameSize);
		uint32_t encodedBytes;
		if (port->param.portType == SCEVOICE_PORTTYPE_IN_VOICE ||
		    port->param.portType == SCEVOICE_PORTTYPE_OUT_VOICE) {
			VoiceEncoderSlot *slot = &pool->slots[port->codecSlotId];
			encodedBytes = voiceEncoderEncodeFrame(slot, port, pool->inputScratch, pool->outputScratch);
		} else {
			encodedBytes = voiceEncoderPassThrough((VoiceEncoderSlot *)pool->pcmPassthrough, port,
			                                       pool->inputScratch, pool->outputScratch);
		}
		if (encodedBytes != 0) {
			voiceRingWrite(&port->sharedRing, pool->outputScratch, encodedBytes);
			sysVoiceInstanceBroadcastEncoded(inst, port, pool->outputScratch, encodedBytes);
		}
	}
}

int voiceMixerInit(VoiceMixer *mixer)
{
	mixer->inputCacheBuf = NULL;
	mixer->workBuf = NULL;
	return 0;
}

int voiceMixerTerm(VoiceMixer *mixer)
{
	(void)mixer;
	return 0;
}

void voiceMixerResetCache(VoiceMixer *mixer)
{
	for (int i = 0; i < VOICE_MAX_MIX_SLOTS; i++) {
		mixer->cachedPortIds[i] = VOICE_INVALID_SLOT;
	}
	mixer->cachedCount = 0;
	voiceMemset(mixer->inputCacheBuf, 0, VOICE_MAX_MIX_SLOTS * 512u);
}

void voiceMixerAllocWorkBufs(VoiceMixer *mixer)
{
	mixer->inputCacheBuf = sysVoiceAllocMem(&gSysVoice, 11264, "Mixing working buffer", 1);
	mixer->workBuf = sysVoiceAllocMem(&gSysVoice, 2208, "Mixing working buffer", 1);
}

void voiceMixerFreeWorkBufs(VoiceMixer *mixer)
{
	sysVoiceFreeMem(&gSysVoice, mixer->inputCacheBuf, 1);
	mixer->inputCacheBuf = NULL;
	sysVoiceFreeMem(&gSysVoice, mixer->workBuf, 1);
	mixer->workBuf = NULL;
}

void voiceMixerMixPort(VoiceMixer *mixer, VoicePort *outPort, uint8_t muteAll)
{
	int16x4_t *mix16k = (int16x4_t *)mixer->workBuf;
	int16x4_t *mix8k = mix16k + 64;
	float32x4_t *firSrc;
	float32x4_t *firDst;
	int16x4_t *resampledInt;

	if (outPort->sampleRate == 8000u) {
		firSrc = (float32x4_t *)(mix16k + 128);
		resampledInt = mix16k + 128;
		firDst = (float32x4_t *)mix16k;
	} else {
		firDst = (float32x4_t *)(mix16k + 64);
		firSrc = (float32x4_t *)(mix16k + 192);
		resampledInt = (int16x4_t *)firSrc;
	}

	voiceMemset(mix8k, 0, 256);
	voiceMemset(mix16k, 0, 512);

	int mixedAny = 0;
	for (VoiceEdge *edge = (VoiceEdge *)outPort->edgeList.head;
	     edge != NULL;
	     edge = (VoiceEdge *)edge->node.next) {
		VoicePort *inPort = sysVoiceGetPort(&gSysVoice, edge->iPortId);
		if (inPort->paused != 0) {
			continue;
		}

		int cachedSlot = -1;
		for (int s = 0; s < VOICE_MAX_MIX_SLOTS; s++) {
			if (mixer->cachedPortIds[s] == inPort->portId) {
				cachedSlot = s;
				break;
			}
		}

		int16x4_t *srcSamples = NULL;
		if (cachedSlot >= 0) {
			srcSamples = (int16x4_t *)((uint8_t *)mixer->inputCacheBuf + (cachedSlot << 9));
			inPort->mixInputBuf = srcSamples;
			if (inPort->state != SCEVOICE_PORTSTATE_RUNNING) {
				continue;
			}
		} else {
			inPort->mixInputBuf = NULL;
			uint16_t internalUsed = (uint16_t)(inPort->internalRing.writeCount -
			                                   inPort->internalRing.readCount);
			uint32_t frameBytes = (inPort->sampleRate == 8000u) ? 256u : 512u;
			if (internalUsed < frameBytes &&
			    (inPort->sharedRing.writeCount != inPort->sharedRing.readCount || internalUsed == 0)) {
				srcSamples = (int16x4_t *)inPort->mixInputBuf;
				if (srcSamples == NULL) {
					continue;
				}
			} else if (mixer->cachedCount < VOICE_MAX_MIX_SLOTS) {
				uint16_t slotIdx = mixer->cachedCount;
				mixer->cachedPortIds[slotIdx] = inPort->portId;
				mixer->cachedCount = (uint16_t)(slotIdx + 1);
				uint8_t *cacheBuf = (uint8_t *)mixer->inputCacheBuf + ((uint32_t)slotIdx << 9);
				inPort->mixInputBuf = cacheBuf;
				uint32_t got = voiceRingRead(&inPort->internalRing, cacheBuf, frameBytes);
				voiceMemset(cacheBuf + 2u * got, 0, frameBytes - got);
				srcSamples = (int16x4_t *)inPort->mixInputBuf;
			} else {
				continue;
			}
		}

		int16_t gainQ8 = 0;
		if (outPort->param.bMute == 0 && muteAll == 0) {
			gainQ8 = (int16_t)(int32_t)(outPort->param.volume * 256.0f);
		}

		int16x4_t gainVec = vdup_n_s16(gainQ8);
		if (inPort->sampleRate == 8000u) {
			int16x4_t *dst = mix8k;
			for (int k = 0; k < 32; k++) {
				int32x4_t acc = vmlal_s16(vshlq_n_s32(vmovl_s16(*dst), 8), srcSamples[k], gainVec);
				*dst++ = vqrshrn_n_s32(vrshrq_n_s32(acc, 4), 4);
			}
		} else if (inPort->sampleRate == 16000u) {
			int16x4_t *dst = mix16k;
			for (int k = 0; k < 64; k++) {
				int32x4_t acc = vmlal_s16(vshlq_n_s32(vmovl_s16(*dst), 8), srcSamples[k], gainVec);
				*dst++ = vqrshrn_n_s32(vrshrq_n_s32(acc, 4), 4);
			}
		}
		mixedAny = 1;
	}

	if (mixedAny == 0) {
		return;
	}

	if (outPort->sampleRate == 8000u) {
		voiceMemcpy(firSrc, outPort->resampleHistory, 160);
		for (int k = 0; k < 64; k++) {
			firSrc[10 + k] = vmulq_n_f32(vcvtq_f32_s32(vmovl_s16(mix16k[k])), 1.0f / 32768.0f);
		}
		voiceMemcpy(outPort->resampleHistory, &firSrc[64], 160);
		voiceFirDownsample2x((float *)firDst, 128, (const float *)firSrc, 296);
		for (int k = 0; k < 32; k++) {
			resampledInt[k] = vqmovn_s32(vcvtq_s32_f32(vmulq_n_f32(firDst[k], 32768.0f)));
		}
		for (int k = 0; k < 32; k++) {
			mix8k[k] = vqadd_s16(mix8k[k], resampledInt[k]);
		}
		voiceRingWrite(&outPort->internalRing, mix8k, 256);
	} else {
		voiceMemcpy(firSrc, outPort->resampleHistory, 160);
		for (int k = 0; k < 32; k++) {
			firSrc[10 + k] = vmulq_n_f32(vcvtq_f32_s32(vmovl_s16(mix8k[k])), 1.0f / 32768.0f);
		}
		voiceMemcpy(outPort->resampleHistory, &firSrc[32], 160);
		voiceFirUpsample2x((float *)firDst, 256, (const float *)firSrc, 168);
		for (int k = 0; k < 64; k++) {
			resampledInt[k] = vqmovn_s32(vcvtq_s32_f32(vmulq_n_f32(firDst[k], 32768.0f)));
		}
		for (int k = 0; k < 64; k++) {
			mix16k[k] = vqadd_s16(mix16k[k], resampledInt[k]);
		}
		voiceRingWrite(&outPort->internalRing, mix16k, 512);
	}
}

int voiceAudioInPoolInit(VoiceAudioInPool *pool)
{
	voiceListInit(&pool->freeList);
	voiceListInit(&pool->activeList);
	voiceMemset(&pool->devices[0], 0, sizeof(VoiceAudioInDevice));
	voiceListPushBack(&pool->freeList, &pool->devices[0].node);
	voiceDeviceInit(&pool->devices[0], 0, 16000, 256);
	return 0;
}

void voiceAudioInPoolTerm(VoiceAudioInPool *pool)
{
	voiceListPopFront(&pool->freeList);
}

int voiceAudioInPoolRelease(VoiceAudioInPool *pool, VoicePort *port)
{
	VoiceAudioInDevice *dev = &pool->devices[port->codecSlotId];
	int ret = voiceAudioInClose(dev);
	voiceListRemove(&pool->activeList, &dev->node);
	voiceDeviceUnbindPort(dev);
	port->codecSlotId = VOICE_UNBOUND_SLOT;
	voiceListPushFront(&pool->freeList, &dev->node);
	return ret;
}

int voiceAudioInPoolAcquire(VoiceAudioInPool *pool, VoicePort *port, const SysVoiceInstance *inst)
{
	if (pool->activeList.count >= inst->limits.maxDevicePort) {
		return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
	}
	VoiceAudioInDevice *dev = (VoiceAudioInDevice *)voiceListPopFront(&pool->freeList);
	voiceDeviceBindPort(dev, port);
	port->deviceMagic = 0x3951A517u;
	port->codecSlotId = dev->slotIndex;
	voiceListPushBack(&pool->activeList, &dev->node);
	return voiceAudioInOpen(dev);
}

int voiceAudioInPoolProcess(VoiceAudioInPool *pool, VoicePort *port)
{
	return voiceAudioInProcess(&pool->devices[port->codecSlotId], port);
}

int voiceAudioOutPoolInit(VoiceAudioOutPool *pool)
{
	voiceListInit(&pool->freeList);
	voiceListInit(&pool->activeList);
	voiceMemset(&pool->devices[0], 0, sizeof(VoiceAudioOutDevice));
	voiceListPushBack(&pool->freeList, &pool->devices[0].node);
	voiceDeviceInit((VoiceAudioInDevice *)&pool->devices[0], 0, 16000, 256);
	return 0;
}

void voiceAudioOutPoolTerm(VoiceAudioOutPool *pool)
{
	voiceListPopFront(&pool->freeList);
}

int voiceAudioOutPoolRelease(VoiceAudioOutPool *pool, VoicePort *port)
{
	VoiceAudioOutDevice *dev = &pool->devices[port->codecSlotId];
	voiceListRemove(&pool->activeList, &dev->node);
	int ret = voiceAudioOutClose(dev);
	voiceDeviceUnbindPort((VoiceAudioInDevice *)dev);
	port->codecSlotId = VOICE_UNBOUND_SLOT;
	voiceListPushFront(&pool->freeList, &dev->node);
	return ret;
}

int voiceAudioOutPoolAcquire(VoiceAudioOutPool *pool, VoicePort *port, const SysVoiceInstance *inst)
{
	if (pool->activeList.count >= inst->limits.maxDevicePort) {
		return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
	}
	VoiceAudioOutDevice *dev = (VoiceAudioOutDevice *)voiceListPopFront(&pool->freeList);
	voiceDeviceBindPort((VoiceAudioInDevice *)dev, port);
	port->codecSlotId = dev->slotIndex;
	voiceListPushBack(&pool->activeList, &dev->node);
	return voiceAudioOutOpen(dev);
}

int voiceAudioOutPoolProcess(VoiceAudioOutPool *pool, VoicePort *port)
{
	return voiceAudioOutProcess(&pool->devices[port->codecSlotId], port);
}
