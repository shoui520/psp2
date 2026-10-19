/* Bluetooth audio packet reception; original source filename unavailable. */
#include "bt_internal.h"

extern int gBtAudioLastSequence;
extern uint64_t gBtAudioContiguousPackets;
extern uint64_t gBtAudioIntervalStartUs;
extern uint64_t gBtAudioIntervalEndUs;

void BtReceiveAudioPacket(BtConnection *connection, unsigned int profile,
			  const uint8_t *packet, int length)
{
	const BtRtpPacket *rtp;
	const BtSbcTransportPacket *transport;
	const BtSbcFrame *frame;
	uint64_t now;
	int sequence;
	int remaining;
	int flags;
	int framesRemaining;
	int receivedSamples;
	unsigned int configuration;

	if (profile != 4) {
		if (profile == 0x20000000 &&
		    (connection->activeProfiles & 0x20000000)) {
			const BtHciScoPacket *sco = (const BtHciScoPacket *)packet;
			unsigned int status = (sco->handleLe[1] >> 4) & 3;

			connection->statistics.scoPacketStatusCounts[status]++;
			BtRingWrite(&gBtAudioReceive.ring, sco->payload, length - 3, 0);
			BtNotifyEvent(18, 0, gBtAudioReceive.ring.used,
				      connection->deviceAddress);
		}
		return;
	}
	if (!(connection->activeProfiles & 4))
		return;
	now = BtGetSystemTimeWide();
	rtp = (const BtRtpPacket *)packet;
	BtUnpack(rtp->sequenceBe, 2, "H", &sequence);
	if (gBtAudioContiguousPackets == 128) {
		int duration = (uint32_t)1000000 * gBtAudioReceive.receivedSamples;
		int interval;

		switch ((int)gBtAudioReceive.sbcFrameHeader >> 6) {
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
		default:
			break;
		}
		interval = (int32_t)((uint32_t)gBtAudioIntervalEndUs + 64 -
				    (uint32_t)gBtAudioIntervalStartUs) >> 7;
		if (interval < duration - 1 || interval > duration + 1)
			goto resetInterval;
	}
	if (gBtAudioLastSequence >= 0 &&
	    (uint16_t)(gBtAudioLastSequence + 1) == sequence) {
		gBtAudioIntervalEndUs = now;
		gBtAudioContiguousPackets++;
	} else {
resetInterval:
		gBtAudioIntervalStartUs = now;
		gBtAudioContiguousPackets = 0;
	}
	gBtAudioLastSequence = sequence;
	remaining = length - sizeof(*rtp);
	if (remaining < 0)
		return;
	transport = (const BtSbcTransportPacket *)rtp->payload;
	if (connection->avdtp.receiveContentProtection) {
		remaining--;
		if (remaining < 0)
			return;
		transport = (const BtSbcTransportPacket *)(rtp->payload + 1);
	}
	flags = transport->flagsAndFrameCount;
	remaining--;
	if (remaining == -1 || (flags & 0x80))
		return;
	framesRemaining = (flags & 15) - 1;
	frame = (const BtSbcFrame *)transport->frames;
	receivedSamples = 0;
	configuration = 0;
	if (framesRemaining == -1)
		goto notifyAudio;
	if (remaining <= 3 || frame->syncword != 0x9c)
		return;
	for (;;) {
		int subbands;
		int headerLength;
		int stereoHeaderLength;
		int mode;
		int blocks;
		int channels;
		int jointBits;
		int encodedLength;
		int samples;
		int pcmBytes;
		int tailSpace;
		uint8_t recordFlags = 0;

		configuration = frame->configuration;
		blocks = 4 * (((configuration >> 4) & 3) + 1);
		mode = (configuration >> 2) & 3;
		if (configuration & 1) {
			headerLength = 8;
			stereoHeaderLength = 12;
			subbands = 8;
		} else {
			headerLength = 6;
			stereoHeaderLength = 8;
			subbands = 4;
		}
		channels = 1;
		jointBits = 0;
		if (mode) {
			headerLength = stereoHeaderLength;
			if (mode == 3)
				jointBits = subbands;
			else if (mode == 1)
				channels = 2;
		}
		encodedLength = headerLength +
			((jointBits + frame->bitpool * blocks * channels + 7) >> 3);
		remaining -= encodedLength;
		if (remaining < 0)
			return;
		if (encodedLength + 8 >=
		    gBtAudioReceive.ring.capacity - gBtAudioReceive.ring.used)
			goto overrun;
		tailSpace = gBtAudioReceive.ring.capacity -
			gBtAudioReceive.ring.writePosition;
		if (encodedLength + 8 >= tailSpace) {
			gBtAudioReceive.ring.data[gBtAudioReceive.ring.writePosition] = 0xff;
			gBtAudioReceive.ring.used += tailSpace;
			gBtAudioReceive.ring.writePosition = 0;
			if (encodedLength + 8 >=
			    gBtAudioReceive.ring.capacity - gBtAudioReceive.ring.used)
				goto overrun;
		}
		samples = blocks * subbands;
		framesRemaining--;
		BtRingWrite(&gBtAudioReceive.ring, &recordFlags, 1, 0);
		pcmBytes = 4 * samples;
		receivedSamples += samples;
		BtRingWrite(&gBtAudioReceive.ring, (uint8_t *)&pcmBytes, 4, 0);
		BtRingWrite(&gBtAudioReceive.ring, (uint8_t *)&encodedLength, 4, 0);
		BtRingWrite(&gBtAudioReceive.ring, (const uint8_t *)frame,
			    encodedLength, 0);
		frame = (const BtSbcFrame *)((const uint8_t *)frame + encodedLength);
		gBtAudioReceive.queuedPcmBytes += pcmBytes;
		if (framesRemaining == -1)
			break;
		if (remaining <= 3 || frame->syncword != 0x9c)
			return;
	}
notifyAudio:
	BtNotifyEvent(18, 0, gBtAudioReceive.queuedPcmBytes,
		      connection->deviceAddress);
	if (gBtAudioReceive.previousPacketTime &&
	    gBtAudioReceive.previousPacketTime + 192000 >= now)
		gBtAudioReceive.arrivalIntervalAverage16 =
			(uint32_t)gBtAudioReceive.arrivalIntervalAverage16 -
			(uint32_t)gBtAudioReceive.previousPacketTime -
			(gBtAudioReceive.arrivalIntervalAverage16 >> 4) + (uint32_t)now;
	gBtAudioReceive.previousPacketTime = now;
	gBtAudioReceive.receivedSamples = receivedSamples;
	gBtAudioReceive.sbcFrameHeader = configuration;
	return;
overrun:
	gBtAudioReceive.overruns++;
}

int BtGetAudioStatistics(void *buffer)
{
	BtAudioStatistics *statistics = buffer;
	uint64_t intervalEnd = gBtAudioIntervalEndUs;
	uint64_t intervalStart = gBtAudioIntervalStartUs;
	uint32_t contiguousLow;
	uint32_t contiguousHigh;
	uint32_t adjustmentSilenceBytes;

	statistics->queuedPcmBytes = gBtAudioReceive.queuedPcmBytes;
	contiguousLow = (uint32_t)gBtAudioContiguousPackets;
	statistics->targetBufferedBytes = gBtAudioReceive.targetBufferedBytes;
	contiguousHigh = (uint32_t)(gBtAudioContiguousPackets >> 32);
	statistics->prebuffering = gBtAudioReceive.prebuffering;
	statistics->skipBytes = gBtAudioReceive.skipBytes;
	statistics->silenceBytes = gBtAudioReceive.silenceBytes;
	statistics->pendingPcmBytes = gBtAudioReceive.pendingPcmBytes;
	statistics->underruns = gBtAudioReceive.underruns;
	statistics->overruns = gBtAudioReceive.overruns;
	statistics->ringUsedBytes = gBtAudioReceive.ring.used;
	statistics->skippedBytes = gBtAudioReceive.skippedBytes;
	statistics->concealedBytes = gBtAudioReceive.concealedBytes;
	statistics->startupSkippedBytes = gBtAudioReceive.startupSkippedBytes;
	statistics->adjustmentSkippedBytes = gBtAudioReceive.adjustmentSkippedBytes;
	adjustmentSilenceBytes = gBtAudioReceive.adjustmentSilenceBytes;
	statistics->contiguousPackets = ((uint64_t)contiguousHigh << 32) | contiguousLow;
	statistics->intervalUs = intervalEnd - intervalStart;
	statistics->adjustmentSilenceBytes = adjustmentSilenceBytes;
	return 0;
}

int BtAdjustAudioReceiveBuffer(int operation)
{
	switch (operation) {
	case 0:
		gBtAudioReceive.skipBytes += 1024;
		break;
	case 1:
		gBtAudioReceive.silenceBytes += 1024;
		break;
	case 2:
		if (gBtAudioReceive.targetBufferedBytes >= 2048)
			gBtAudioReceive.targetBufferedBytes -= 1024;
		break;
	case 3:
		if (gBtAudioReceive.targetBufferedBytes < 19456)
			gBtAudioReceive.targetBufferedBytes += 1024;
		break;
	}
	return 0;
}

int BtInitializeAudio(void)
{
	gBtAudioReceive.targetBufferedBytes = 10240;
	return 0;
}
