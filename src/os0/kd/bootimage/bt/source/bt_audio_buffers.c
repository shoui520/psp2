/* Bluetooth audio buffer ownership; original source filename unavailable. */
#include "bt_internal.h"
#include <string.h>

extern int gBtPendingAudioPackets;
extern int gBtAudioUnderruns;
extern int gBtAudioOverruns;
extern int gBtAudioLastSequence;
BtConnection *BtFindConnection(BtStack *stack, unsigned char selector,
			       uint64_t deviceAddress);
int BtCopyIn(const void *source, void *destination, unsigned int length,
	     int userMode);
int BtSendScoPacket(BtConnection *connection, const void *data, int length,
		    int userMode);
int BtPack(uint8_t *destination, int remaining, const char *format, ...);
int BtSendAvdtpMedia(BtConnection *connection, const void *packet, int length,
		    int mode);
int BtSendHciCommand(BtStack *stack, unsigned int opcode,
		     const char *format, ...);
int BtSendL2capPacket(BtConnection *connection, int channelId,
		     const char *format, ...);
uintptr_t BtNotifyEvent(unsigned int eventId, unsigned int status,
		       unsigned int argument, uint64_t deviceAddress);

uint8_t gBtSbcSilenceFrame[21];
BtAudioSendState gBtAudioSend;
BtAudioReceiveState gBtAudioReceive;

void BtClearAudioConnection(BtConnection *connection)
{
	if (gBtAudioSend.connection == connection) {
		gBtAudioSend.connection = NULL;
		gBtPendingAudioPackets = 0;
	}
	if (gBtAudioReceive.connection == connection) {
		gBtAudioReceive.connection = NULL;
		gBtAudioReceive.pendingPcmBytes = 0;
		gBtAudioReceive.ring.used = 0;
		gBtAudioReceive.ring.writePosition = 0;
		gBtAudioReceive.ring.readPosition = 0;
	}
}

uintptr_t BtNotifyAudioStarted(BtConnection *connection, int status,
			     unsigned int profiles)
{
	unsigned int packetType;

	if (status) {
		connection->startingProfiles &= ~profiles;
		connection->activeProfiles &= ~profiles;
		goto notify;
	}
	connection->startingProfiles &= ~profiles;
	connection->activeProfiles |= profiles;
	if (profiles & 0x20000004) {
		gBtAudioReceive.connection = connection;
		BtRingReset(&gBtAudioReceive.ring);
		gBtAudioReceive.ring.capacity = sizeof(gBtAudioReceive.ringData);
		gBtAudioReceive.ring.data = gBtAudioReceive.ringData;
		gBtAudioReceive.pendingPcmBytes = 0;
		gBtAudioReceive.queuedPcmBytes = 0;
		gBtAudioReceive.prebuffering = 1;
	}
	if (profiles & 0x20000008) {
		gBtAudioSend.connection = connection;
		gBtAudioSend.resetEncoder = 1;
		gBtAudioSend.sequence = BtGetSystemTimeWide();
		gBtAudioSend.startupDropPackets = 4;
	}
	if (!(profiles & 0xc))
		goto notify;
	if (connection->stack->controllerType == 1 &&
	    connection->avdtp.controllerSleepState != 1) {
		BtSendHciCommand(connection->stack, 0xfc1e, "1 888", 1,
				UINT64_C(0x3131313131313131), UINT64_C(0),
				UINT64_C(0x0c0c0c0c0c0c0c0c));
		connection->avdtp.controllerSleepState = 1;
		BtSendHciCommand(connection->stack, 0xfc24, "21",
				connection->aclHandle, 1);
	}
	if (connection->linkState == 1) {
		packetType = connection->remoteFeatures & UINT64_C(0x8000000000) ?
			     0x3c18 : 0xcc18;
		if (packetType != connection->packetType) {
			connection->packetType = packetType;
			BtSendHciCommand(connection->stack, 0x40f, "22",
					connection->aclHandle, packetType);
		}
	}
notify:
	return BtNotifyEvent(0x10, status, profiles, connection->deviceAddress);
}

uintptr_t BtNotifyAudioStopped(BtConnection *connection, int status,
			     unsigned int profiles)
{
	unsigned int stoppingProfiles = connection->stoppingProfiles;
	unsigned int activeProfiles = connection->activeProfiles;
	BtStack *stack = connection->stack;

	activeProfiles &= ~profiles;
	stoppingProfiles &= ~profiles;
	connection->activeProfiles = activeProfiles;
	connection->stoppingProfiles = stoppingProfiles;
	if (stack->controllerType == 1 &&
	    connection->avdtp.controllerSleepState == 1 &&
	    !(activeProfiles & 0xc)) {
		connection->avdtp.controllerSleepState = 2;
		BtSendHciCommand(stack, 0xfc24, "21", connection->aclHandle, 2);
	}
	if (connection->linkState != 1 && connection->packetType != 0x3318) {
		connection->packetType = 0x3318;
		BtSendHciCommand(connection->stack, 0x40f, "22",
				connection->aclHandle, 0x3318);
	}
	return BtNotifyEvent(0x11, status, profiles, connection->deviceAddress);
}

int BtSendAudio(BtStack *stack, uint64_t deviceAddress, void *buffer,
		unsigned int size, int userMode)
{
	BtConnection *connection = BtFindConnection(stack, 1, deviceAddress);
	unsigned int profiles;
	uint8_t protection;
	int headerLength;
	int encodedLength;
	int result = 0;

	if (!connection)
		return (int)0x802f0d05;
	profiles = connection->activeProfiles;
	if (profiles & 0x20000000) {
		if (size != stack->scoPacketLength)
			return (int)0x802f0d07;
		result = BtCopyIn(buffer, gBtAudioSend.packet, size, userMode);
		if (result < 0)
			return result;
		result = BtSendScoPacket(connection, gBtAudioSend.packet, size, 0);
		return result ? result : size;
	}
	if (size != 2048)
		return (int)0x802f0d07;
	if (!(profiles & 8))
		return (int)0x802f0d06;
	if (stack->contentProtection && !gBtAudioSend.contentProtection)
		return (int)0x802f0d10;
	if (gBtAudioSend.startupDropPackets > 0) {
		gBtAudioSend.startupDropPackets--;
		return 2048;
	}
	if (connection->linkState == 1)
		return 2048;
	if (gBtAudioSend.resetEncoder) {
		uint32_t synchronizationSource;
		int blocks;

		gBtPendingAudioPackets = 0;
		gBtAudioSend.resetEncoder = 0;
		synchronizationSource = BtGetSystemTimeWide();
		gBtAudioSend.synchronizationSource = synchronizationSource;
		blocks = connection->avdtp.blockCount;
		gBtAudioSend.timestamp = synchronizationSource;
		gBtAudioSend.framesPerPacket = 64 / blocks;
		gBtAudioSend.contentProtection = connection->avdtp.transmitContentProtection;
		SbcInitializeEncoder(&gBtAudioSend.codec, 3, blocks,
				     connection->avdtp.channelMode, 0, 8,
				     connection->avdtp.minimumBitpool,
				     connection->avdtp.maximumBitpool);
		gBtAudioSend.codec.fadeState = 0;
		gBtAudioSend.codec.fadeProgress = 0;
		gBtAudioSend.codec.stereoInput = 1;
	} else if (gBtAudioSend.codec.fadeState == 2) {
		if (gBtPendingAudioPackets > 4) {
			gBtAudioUnderruns++;
			goto advance;
		}
		gBtAudioSend.codec.fadeState = 3;
		gBtAudioSend.codec.fadeProgress = 0;
	}
	if (!gBtAudioSend.codec.fadeState && gBtPendingAudioPackets > 23) {
		gBtAudioSend.codec.fadeState = 1;
		gBtAudioSend.codec.fadeProgress = 0;
		gBtAudioOverruns++;
	}
	if (stack->contentProtection == 1)
		protection = 2;
	else if (stack->contentProtection == 2)
		protection = 0;
	else
		protection = 3;
	headerLength = BtPack(gBtAudioSend.packet, 14, "11HWW#1", 128, 96,
			      gBtAudioSend.sequence, (uint32_t)gBtAudioSend.timestamp,
			      gBtAudioSend.synchronizationSource, &protection,
			      gBtAudioSend.contentProtection != 0,
			      gBtAudioSend.framesPerPacket);
	result = BtCopyIn(buffer, stack->scratch, 2048, userMode);
	if (result < 0)
		return result;
	encodedLength = SbcEncodeFrames(&gBtAudioSend.codec, stack->scratch,
				       gBtAudioSend.packet + headerLength,
				       gBtAudioSend.framesPerPacket);
	if (BtSendAvdtpMedia(connection, gBtAudioSend.packet,
			     headerLength + encodedLength, 1) >= 0)
		gBtPendingAudioPackets++;
advance:
	gBtAudioSend.sequence++;
	gBtAudioSend.timestamp += 8000;
	return result ? result : 2048;
}

int BtRecvAudio(BtStack *stack, uint64_t deviceAddress, void *buffer,
		unsigned int size, int userMode)
{
	BtConnection *connection;
	BtAudioReceiveState *receive = &gBtAudioReceive;
	BtRingBuffer *ring = &receive->ring;
	uint8_t *destination = buffer;
	uint8_t marker;
	int pcmLength, encodedLength;
	int remaining = size & ~3U;
	int result = 0;
	uint64_t now, deadline;

	connection = BtFindConnection(stack, 1, deviceAddress);
	if (!connection)
		return 0x802f0d08;
	if (!(connection->activeProfiles & 0x20000004))
		return 0x802f0d09;
	if (connection->activeProfiles & 0x20000000)
		return BtRingRead(ring, buffer, remaining, userMode);
	if ((unsigned int)remaining > 4096)
		return 0x802f0d0a;
	receive->bufferedBytesAverage32 += receive->queuedPcmBytes -
		(receive->bufferedBytesAverage32 >> 5);
	now = BtGetSystemTimeWide();
	if (!remaining)
		return 0;
	deadline = now + 125000;
	do {
		if (!receive->prebuffering) {
			if (!receive->concealmentUntil ||
			    now >= receive->concealmentUntil) {
				int threshold = 4096;
				int average, adjustment;

				if (ring->used > 0) {
					if (ring->data[ring->readPosition] == 255) {
						int position = ring->readPosition;

						ring->readPosition = 0;
						ring->used += position - ring->capacity;
					}
					if (ring->used > 8) {
						BtRingRead(ring, &marker, 1, 0);
						BtRingRead(ring, (uint8_t *)&pcmLength, 4, 0);
						BtRingRead(ring, (uint8_t *)&encodedLength, 4, 0);
						if (ring->used >= encodedLength)
							threshold = ring->data[ring->readPosition + 2] == 2 ?
								1024 : 4096;
						BtRingRewind(ring, 9);
					}
				}
				average = receive->bufferedBytesAverage32 >> 5;
				if (average >= receive->targetBufferedBytes + threshold) {
					adjustment = (average - receive->targetBufferedBytes) / 1024 * 1024;
					receive->concealmentUntil = deadline;
					receive->skipBytes += adjustment;
					receive->adjustmentSkippedBytes += adjustment;
				} else if (average <= receive->targetBufferedBytes - threshold) {
					adjustment = (receive->targetBufferedBytes - average) / 1024 * 1024;
					receive->concealmentUntil = deadline;
					receive->silenceBytes += adjustment;
					receive->adjustmentSilenceBytes += adjustment;
				}
			}
		} else {
			unsigned int duration = 1000000 * receive->receivedSamples;

			switch (receive->sbcFrameHeader >> 6) {
			case 0:
				duration /= 16000;
				break;
			case 1:
				duration /= 32000;
				break;
			case 2:
				duration /= 44100;
				break;
			case 3:
				duration /= 48000;
				break;
			}
			if ((duration >> 1) >= (receive->arrivalIntervalAverage16 + 8) >> 4) {
				receive->skipBytes = receive->queuedPcmBytes;
				receive->startupSkippedBytes += receive->queuedPcmBytes;
			}
		}
		if (receive->skipBytes > 49152) {
			receive->silenceBytes = 0;
			receive->prebuffering = 1;
			receive->skipBytes = 0;
		} else {
			while (ring->used > 0 && receive->skipBytes > 0) {
				if (ring->data[ring->readPosition] == 255) {
					int position = ring->readPosition;

					ring->readPosition = 0;
					ring->used += position - ring->capacity;
				}
				if (ring->used <= 8)
					break;
				BtRingRead(ring, &marker, 1, 0);
				BtRingRead(ring, (uint8_t *)&pcmLength, 4, 0);
				BtRingRead(ring, (uint8_t *)&encodedLength, 4, 0);
				if (ring->used < encodedLength || receive->skipBytes < pcmLength) {
					BtRingRewind(ring, 9);
					break;
				}
				BtRingConsume(ring, encodedLength);
				receive->skipBytes -= pcmLength;
				receive->queuedPcmBytes -= pcmLength;
				receive->skippedBytes += pcmLength;
			}
		}
		if (receive->prebuffering) {
			if (receive->targetBufferedBytes >= receive->queuedPcmBytes)
				receive->silenceBytes += remaining;
		} else if (receive->queuedPcmBytes + receive->silenceBytes < remaining) {
			receive->silenceBytes += remaining;
			receive->underruns++;
			receive->skipBytes += remaining;
		}
		if (receive->pendingPcmBytes > 0) {
			int length = receive->pendingPcmBytes;

			if (remaining < length)
				length = remaining;
			result = BtCopyOut((uint8_t *)receive->pcm + receive->pcmOffset,
					   destination, length, userMode);
			if (result < 0)
				return result;
			destination += length;
			receive->pcmOffset += length;
			receive->queuedPcmBytes -= length;
			receive->pendingPcmBytes -= length;
			remaining -= length;
			if (!remaining)
				break;
		}
		if (receive->silenceBytes > 0) {
			int length;

			if (!receive->prebuffering) {
				SbcDecodeFrame(&receive->codec, gBtSbcSilenceFrame, receive->pcm, 1);
				length = 4 * receive->codec.blocks * receive->codec.subbands;
				receive->pcmOffset = 0;
				receive->pendingPcmBytes = length;
				receive->queuedPcmBytes += length;
				receive->concealedBytes += length;
				receive->silenceBytes -= length;
				continue;
			}
			length = receive->silenceBytes;
			if (remaining < length)
				length = remaining;
			memset(stack->scratch, 0, length);
			result = BtCopyOut(stack->scratch, destination, length, userMode);
			if (result < 0)
				return result;
			destination += length;
			receive->silenceBytes -= length;
			receive->concealedBytes += length;
			remaining -= length;
			if (!remaining)
				break;
		}
		if (ring->used <= 0)
			continue;
		if (ring->data[ring->readPosition] == 255) {
			int position = ring->readPosition;

			ring->readPosition = 0;
			ring->used += position - ring->capacity;
		}
		if (ring->used <= 8)
			break;
		BtRingRead(ring, &marker, 1, 0);
		BtRingRead(ring, (uint8_t *)&pcmLength, 4, 0);
		BtRingRead(ring, (uint8_t *)&encodedLength, 4, 0);
		if (ring->used < encodedLength) {
			BtRingRewind(ring, 9);
			break;
		}
		if (receive->prebuffering)
			SbcResetDecoder(&receive->codec);
		SbcDecodeFrame(&receive->codec, ring->data + ring->readPosition,
			       receive->pcm, 1);
		BtRingConsume(ring, encodedLength);
		receive->pcmOffset = 0;
		receive->pendingPcmBytes = pcmLength;
		if (receive->prebuffering) {
			BtSbcFrame *frame = (BtSbcFrame *)gBtSbcSilenceFrame;
			int subbands = receive->codec.subbands;
			int mode = receive->codec.mode;
			int scaleBytes = receive->codec.channels * 4 * subbands / 8;
			uint8_t *fill;

			frame->crc = 0;
			frame->syncword = 0x9c;
			frame->configuration = ((subbands >> 2) - 1) |
				(receive->codec.frequency << 6) | (mode << 2) |
				(receive->codec.allocation << 1) |
				(((receive->codec.blocks >> 2) - 1) << 4);
			frame->bitpool = 2;
#if FW_VERSION >= 0x04000000
			__builtin___memset_chk(frame->payload, 0, scaleBytes,
				sizeof(gBtSbcSilenceFrame) - sizeof(*frame));
#else
			memset(frame->payload, 0, scaleBytes);
#endif
			fill = frame->payload + scaleBytes;
			if (mode == 3)
				*fill++ = subbands == 4 ? 5 : 0;
			memset(fill, 0x55, 8);
			gBtAudioLastSequence = -1;
			receive->prebuffering = 0;
			receive->bufferedBytesAverage32 = 32 * receive->queuedPcmBytes;
		}
	} while (remaining);
	return result ? result : destination - (uint8_t *)buffer;
}

int BtSendAvdtpMedia(BtConnection *connection, const void *packet, int length,
		    int mode)
{
	if (connection->avdtp.streams[1].state != 10)
		return 0x802f0508;
	return BtSendL2capPacket(connection,
		connection->avdtp.streams[1].channel->remoteCid,
		"C#", 8, mode, packet, length);
}

