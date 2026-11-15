/*
 * The Bluetooth audio source (0x4cee0..): the ROM's SBC codec and PCM port
 * drivers, and the encoder that fills the packets of the A2DP stream.
 * Marvell published no source; the names say what the code does and the file
 * name is inferred.
 */
#include <stdint.h>
#include "robin/btpool.h"
#include "robin/btrom.h"
#include "robin/hct.h"
#include "robin/rom.h"
#include "robin/sbc.h"

uint32_t sbcTimestamp __attribute__((section(".itcm.rw")));
struct SbcFrameDrop sbcFrameDrop __attribute__((section(".itcm.rw"))) = {
	.enable = 1,
	.pending2 = { 0, 0, 0, 0, 1, 0 },
	.threshold = 20,
	.seed = 1,
};

/* Registers the ROM's two SBC codec drivers. */
void fmCoexFilterBuffer(void)
{
	int i;

	for (i = 0; i < SBC_DRIVERS; i++) {
		struct MdevDriver *driver = &sbcCodecDrivers[i];

		sbcCodecDriverStatus[i] = 0;
		driver->index = i;
		driver->name = sbcCodecDriverNames[i];
		driver->callbacks[0] = btSbcRomFormatState;
		driver->callbacks[1] = btSbcRomInspectState;
		driver->callbacks[2] = btSbcRomTrackState;
		driver->callbacks[3] = btSbcRomManageState;
		driver->callbacks[4] = btSbcRomAcquireState;
		driver->callbacks[5] = btSbcRomSetupState;
		driver->callbacks[6] = btSbcRomReleaseState;
		mdevRegisterDriver(driver, btSbcRomReleaseState);
	}
}

/* Registers the ROM's two PCM port drivers and gives them the interrupt calls. */
void fmCoexHandlePacket(void)
{
	int i;

	for (i = 0; i < SBC_DRIVERS; i++) {
		struct MdevDriver *driver = &sbcPcmDrivers[i];

		sbcPcmDriverStatus[i] = 0;
		driver->index = i;
		driver->name = sbcPcmDriverNames[i];
		driver->callbacks[0] = btSbcRomCheckState;
		driver->callbacks[1] = btSbcRomValidateState;
		driver->callbacks[2] = btSbcRomPrepareState;
		driver->callbacks[3] = btSbcRomDispatchState;
		driver->callbacks[4] = btSbcRomParseState;
		driver->callbacks[5] = btSbcRomComputeState;
		driver->callbacks[6] = btSbcRomBuildState;
		mdevRegisterDriver(driver, btSbcRomBuildState);
	}
	sbcPcmIrqRegister = sysIrqRegister;
	sbcPcmIrqUnregister = sysIrqUnregister;
}

/*
 * Takes a packet for the stream and sets the encoder and the PCM port up for
 * it: the encoder writes into the packet, the port fills one frame's samples,
 * and the stream carries as many frames as the packet holds.
 */
int fmCoexProcessPacket(void)
{
	uint32_t segment;
	struct SbcPcmRequest request;
	unsigned int samples;
	uint16_t length;

	sbcPacket = osa_BufferAlloc(&btPacketPool, 0, ~0u);
	segment = (uint32_t)((uint8_t *)sbcPacket + sbcPacket->dataOffset) >> 16;
	mdevIoctl(sbcDevices.pcm, SBC_IOCTL_SET_BUFFER, (uintptr_t)&sbcPcmConfig);
	mdevIoctl(sbcDevices.encoder, SBC_IOCTL_SET_BUFFER, (uintptr_t)&segment);
	mdevIoctl(sbcDevices.pcm, SBC_IOCTL_CONFIGURE, (uintptr_t)&sbcPcmConfig);
	length = mdevIoctl(sbcDevices.encoder, SBC_IOCTL_CONFIGURE, (uintptr_t)&sbcEncoderConfig);
	sbcFrame.length = length;
	sbcFrame.dataOffset = sbcPacket->dataOffset;
	samples = btSbcRomClearState();
	sbcFrame.samples = samples;
	sbcFrame.sampleBytes = samples << 1;
	sbcFrame.active = 1;
	sbcFrame.frames = 0;
	request.length = (uint16_t)(samples << 1);
	request.data = (uint8_t *)sbcPacket + sbcPacket->dataOffset;
	mdevIoctl(sbcDevices.pcm, SBC_IOCTL_QUEUE, (uintptr_t)&request);
	sbcStream.framesPerPacket = sbcPacketCapacity / sbcFrame.length;
	return 0;
}

/*
 * Sets the encoder and the PCM port up again for a new A2DP configuration:
 * the port's format follows the encoder's channel mode and sampling frequency.
 */
int fmCoexUpdatePacket(void)
{
	uint32_t segment;
	struct SbcPcmConfig config;
	struct SbcPcmRequest request;
	unsigned int samples;
	uint16_t length;

	sbcPacket = osa_BufferAlloc(&btPacketPool, 0, ~0u);
	segment = (uint32_t)((uint8_t *)sbcPacket + sbcPacket->dataOffset) >> 16;
	mdevIoctl(sbcDevices.encoder, SBC_IOCTL_SET_BUFFER, (uintptr_t)&segment);
	config.enable = 1;
	if (sbcEncoderConfig.channelMode == 0)
		config.mono = 1;
	else
		config.mono = 0;
	config.cleared[0] = 0;
	config.sampleBits = 8;
	config.cleared[3] = 0;
	config.cleared[1] = 0;
	config.cleared[2] = 0;
	switch (sbcEncoderConfig.samplingFrequency) {
	case 0:
		config.rate = SBC_PCM_RATE_16K;
		break;
	case 1:
		config.rate = SBC_PCM_RATE_32K;
		break;
	case 2:
		config.rate = SBC_PCM_RATE_44K;
		break;
	default:
		config.rate = SBC_PCM_RATE_48K;
		break;
	}
	mdevIoctl(sbcDevices.pcm, SBC_IOCTL_SET_BUFFER, (uintptr_t)&config);
	mdevIoctl(sbcDevices.pcm, SBC_IOCTL_CONFIGURE, (uintptr_t)&config);
	length = mdevIoctl(sbcDevices.encoder, SBC_IOCTL_CONFIGURE, (uintptr_t)&sbcEncoderConfig);
	sbcFrame.length = length;
	sbcFrame.dataOffset = sbcPacket->dataOffset;
	samples = btSbcRomClearState();
	sbcFrame.samples = samples;
	sbcFrame.sampleBytes = samples << 1;
	sbcFrame.active = 1;
	sbcFrame.frames = 0;
	request.length = (uint16_t)(samples << 1);
	request.data = (uint8_t *)sbcPacket + sbcPacket->dataOffset;
	request.mode = 2;
	mdevIoctl(sbcDevices.pcm, SBC_IOCTL_QUEUE, (uintptr_t)&request);
	sbcStream.framesPerPacket = sbcPacketCapacity / sbcFrame.length;
	return 0;
}

/*
 * An encoded frame is ready: waits for the encoder, sends the A2DP packet
 * once it holds its frames (the transport header filled in: lengths, RTP
 * sequence number and timestamp, frame count), takes a new packet when the
 * pool can spare one, and has the encoder write the next frame into it.
 */
int fmCoexConfigurePacket(void)
{
	uint32_t status;
	unsigned int tries = 0;
	uint8_t *rtp;
	unsigned int sequence;
	uint32_t timestamp;

	do {
		mdevIoctl(sbcDevices.encoder, SBC_IOCTL_STATUS, (uintptr_t)&status);
		tries++;
	} while ((status & 1) != 0 && tries < 0xffff);

	if (sbcFrame.frames == sbcStream.framesPerPacket) {
		sbcMediaPacket->dataOffset = SBC_MEDIA_OFFSET;
		sbcMediaPacket->length = sbcFrame.frames * sbcFrame.length + sbcStream.headerLength;
		sbcStream.header[sbcStream.headerLength - 1] =
			(sbcStream.header[sbcStream.headerLength - 1] >> 4) << 4 | (sbcStream.framesPerPacket & 0xf);
		rtp = &sbcStream.header[sbcStream.headerLength - 1 - SBC_RTP_HEADER];
		sequence = (rtp[2] << 8 | rtp[3]) + 1;
		rtp[2] = sequence >> 8;
		rtp[3] = sequence;
		timestamp = sbcTimestamp;
		rtp[4] = timestamp >> 24;
		rtp[5] = timestamp >> 16;
		rtp[6] = timestamp >> 8;
		rtp[7] = timestamp;
		sbcStream.header[2] = sbcMediaPacket->length - 4;
		sbcStream.header[3] = (sbcMediaPacket->length - 4) >> 8;
		sbcStream.header[4] = sbcMediaPacket->length - 8;
		sbcStream.header[5] = (sbcMediaPacket->length - 8) >> 8;
		__aeabi_memcpy((uint8_t *)sbcMediaPacket + sbcMediaPacket->dataOffset, sbcStream.header,
			       sbcStream.headerLength);
		mdevForwardEvent(btHciDev, sbcMediaPacket, sbcMediaPacket->length, SBC_MEDIA_TAG);
		sbcFrame.frames = 0;
		sbcMediaPacket = NULL;
	} else if (sbcMediaPacket != NULL) {
		goto dropped;
	}
	if (osa_BufferClone(&btPacketPool) <= 4) {
		sbcMediaPacket = osa_BufferAlloc(&btPacketPool, 0, ~0u);
		sbcMediaPacket->transportTag = SBC_MEDIA_TAG;
		sbcMediaPacket->data[7] = 2;
		sbcMediaPacket->dataOffset = SBC_MEDIA_OFFSET;
		sbcFrame.frames = 0;
		sbcFrame.headerLength = sbcStream.headerLength + SBC_MEDIA_OFFSET;
	} else {
dropped:
		sbcMediaDropped++;
	}
	if (sbcMediaPacket != NULL) {
		OsaBuffer *samples = sbcPacket;

		/* The samples' buffer carries the packet the frame goes into in its first word. */
		samples->allocationMetadata = (uint32_t)sbcMediaPacket;
		sbcMediaPacket->dataOffset = sbcFrame.frames * sbcFrame.length + sbcFrame.headerLength;
		samples->dataOffset = sbcFrame.event == 4 ? sbcFrame.dataOffset + sbcFrame.samples : sbcFrame.dataOffset;
		mdevForwardEvent(sbcDevices.encoder, samples, 1, 4);
		sbcFrame.frames++;
	}
	sbcFrame.event = 0;
	sbcTimestamp++;
	return 0;
}

/* The encoder's event flags (bits 1-2, 3, 6, 7, 8) and its start (bit 0). */
int btSbcEncoderProcess(uint32_t events, uint32_t argument)
{
	int status = 0;

	if ((events & 1) != 0) {
		if (sbcDevices.state == 0)
			sbcPcmStreamHook = btSbcPcmStreamHandler;
		status = btSbcRomAdjustState(argument);
	}
	if (sbcDevicesOpen != 1)
		return status;
	if ((events & 0x8) != 0) {
		if (sbcHeld == 0) {
			btSbcRomConvertState();
			if (sbcDevices.state == 0)
				fmCoexProcessPacket();
			if (sbcDevices.state == 2)
				fmCoexUpdatePacket();
		}
		status = 0;
	}
	if ((events & 0x40) != 0) {
		sbcTimestamp = 0;
		status = btSbcRomVerifyState(0);
	}
	if ((events & 0x80) != 0)
		status = btSbcRomSelectState();
	if ((events & 0x100) != 0)
		status = fmCoexConfigurePacket();
	if (((events >> 1) & 3) != 0)
		status = btSbcRomRecordState();
	return status;
}

/* Sets the decoder and the sink's PCM port up for a new stream. */
int btSbcHandleState(void)
{
	uint32_t segment;
	struct SbcPcmRequest request;
	unsigned int samples;
	uint16_t length;

	if (sbcHeld == 0) {
		btSbcRomFilterState();
		sbcDecoderPacket = osa_BufferAlloc(&btPacketPool, 0, ~0u);
		segment = (uint32_t)((uint8_t *)sbcDecoderPacket + sbcDecoderPacket->dataOffset) >> 16;
		sbcPcmConfig.control = (sbcPcmConfig.control & ~SBC_PCM_DECODER_FORMAT_MASK) + SBC_PCM_DECODER_FORMAT;
		mdevIoctl(sbcDevices.sinkPcm, SBC_IOCTL_SET_BUFFER, (uintptr_t)&sbcPcmConfig);
		mdevIoctl(sbcDevices.decoder, SBC_IOCTL_SET_BUFFER, (uintptr_t)&segment);
		mdevIoctl(sbcDevices.sinkPcm, SBC_IOCTL_CONFIGURE, (uintptr_t)&sbcPcmConfig);
		length = mdevIoctl(sbcDevices.decoder, SBC_IOCTL_CONFIGURE, (uintptr_t)&sbcEncoderConfig);
		sbcDecoderFrame.length = length;
		sbcDecoderFrame.dataOffset = sbcDecoderPacket->dataOffset;
		samples = btSbcRomClearState();
		sbcDecoderFrame.samples = samples;
		sbcDecoderFrame.sampleBytes = samples << 1;
		sbcDecoderFrame.active = 0;
		sbcDecoderFrame.frames = 0;
		request.length = (uint16_t)(samples << 1);
		request.data = (uint8_t *)sbcDecoderPacket + sbcDecoderPacket->dataOffset;
		mdevIoctl(sbcDevices.sinkPcm, SBC_IOCTL_QUEUE, (uintptr_t)&request);
	}
	return 0;
}

/*
 * The decoder finished a frame: gives the next frame of the received packet
 * to the decoder, or, once the packet is used up, returns it and takes the
 * next one. Without a packet the PCM samples are silence.
 */
int btSbcProcessState(void)
{
	uint32_t consumed;
	uint32_t status;
	unsigned int tries = 0;
	OsaBuffer *packet;

	do {
		mdevIoctl(sbcDevices.decoder, SBC_IOCTL_STATUS, (uintptr_t)&status);
		tries++;
	} while ((status & 1) != 0 && tries < 0xffff);

	if (sbcDecoderFrame.active == 0) {
		if (sbcReceivedPacket != NULL) {
			romHctRxHook(sbcReceivedPacket);
			sbcReceivedPacket = NULL;
		}
		packet = osa_MsgQueueRecv(&sbcReceiveQueue);
		sbcReceivedPacket = packet;
		if (packet != NULL) {
			sbcDecoderFrame.active =
				((uint8_t *)packet + packet->dataOffset - 1)[sbcStream.headerLength] & 0xf;
			sbcDecoderFrame.frames = 0;
			sbcDecoderFrame.length = 0;
			sbcDecoderFrame.headerLength = packet->dataOffset + sbcStream.headerLength;
		} else {
			sbcReceiveEmpty++;
		}
	} else {
		mdevIoctl(sbcDevices.decoder, SBC_IOCTL_CONSUMED, (uintptr_t)&consumed);
		sbcDecoderFrame.length += consumed;
	}
	sbcDecoderPacket->dataOffset = sbcDecoderFrame.event == 4 ?
		sbcDecoderFrame.dataOffset + sbcDecoderFrame.samples : sbcDecoderFrame.dataOffset;
	if (sbcDecoderFrame.active != 0) {
		/* The samples' buffer carries the packet the frame comes from in its first word. */
		sbcDecoderPacket->allocationMetadata = (uint32_t)sbcReceivedPacket;
		sbcReceivedPacket->dataOffset = sbcDecoderFrame.headerLength + sbcDecoderFrame.length;
		mdevForwardEvent(sbcDevices.decoder, sbcDecoderPacket, 1, 5);
		sbcDecoderFrame.frames++;
		sbcDecoderFrame.active--;
	} else {
		__aeabi_memclr((uint8_t *)sbcDecoderPacket + sbcDecoderPacket->dataOffset, sbcDecoderFrame.samples);
	}
	return 0;
}

/* The decoder's event flags (bits 1-2, 3, 6, 7, 9) and its start (bit 0). */
int btSbcEncoderConfigure(uint32_t events, uint32_t argument)
{
	int status = 0;

	if ((events & 1) != 0) {
		btSbcRomHandleContext(argument);
		sbcPcmStreamHook = btSbcPcmStreamHandler;
	}
	if (sbcDevicesOpen == 1) {
		if ((events & 0x8) != 0)
			status = btSbcHandleState();
		if ((events & 0x40) != 0)
			status = btSbcRomUpdateContext();
		if ((events & 0x80) != 0)
			status = btSbcRomConfigureContext();
		if ((events & 0x200) != 0)
			status = btSbcProcessState();
		if (((events >> 1) & 3) != 0)
			status = btSbcRomProcessContext((events >> 1) & 3);
	}
	return status;
}

int btSbcDecoderProcess(void)
{
	return 0;
}

/* Registers the ROM's audio driver. */
int btSbcUpdateState(void)
{
	sbcAudioDriverStatus = 0;
	sbcAudioDriver.name = sbcAudioDriverName;
	sbcAudioDriver.index = 0;
	sbcAudioDriver.callbacks[0] = btSbcRomQueryState;
	sbcAudioDriver.callbacks[1] = btSbcRomResetState;
	sbcAudioDriver.callbacks[2] = btSbcRomSyncState;
	sbcAudioDriver.callbacks[3] = btSbcRomApplyState;
	sbcAudioDriver.callbacks[4] = btSbcRomResolveState;
	sbcAudioDriver.callbacks[5] = btSbcRomNotifyState;
	sbcAudioDriver.callbacks[6] = btSbcRomScheduleState;
	return mdevRegisterDriver(&sbcAudioDriver, btSbcRomScheduleState);
}

/* Takes the frame drop test's settings: enable, mode and the threshold (little-endian). */
void btSbcConfigureState(const uint8_t *settings)
{
	sbcFrameDrop.enable = *settings++;
	sbcFrameDrop.mode = *settings++;
	sbcFrameDrop.threshold = settings[0] | settings[1] << 8;
}

/*
 * A field of two bit pairs (4-5 and 6-7) whose bits must agree: the pair's
 * value (bits 5-6), or 15 when they do not.
 */
unsigned int btSbcCheckState(uint8_t value)
{
	if (((value >> 4) & 1) != ((value >> 5) & 1) || ((value >> 6) & 1) != value >> 7)
		return 15;
	return (value >> 5) & 3;
}

/* Fills the PCM port's descriptor for the encoder's and the decoder's frames. */
void btSbcValidateState(struct SbcPcmDescriptor *descriptor, uint16_t encoderOffset, uint16_t decoderOffset)
{
	unsigned int slots;

	descriptor->encoderOffset = sbcFrame.sampleBytes + encoderOffset;
	descriptor->encoderSamples = sbcFrame.samples;
	descriptor->encoderBytes = sbcFrame.sampleBytes;
	descriptor->decoderOffset = sbcDecoderFrame.sampleBytes + decoderOffset;
	descriptor->decoderSamples = sbcDecoderFrame.samples;
	descriptor->decoderBytes = sbcDecoderFrame.sampleBytes;
	descriptor->portBit1 = (sbcPcmPortConfig & 2) >> 1;
	descriptor->portBit2 = (sbcPcmPortConfig & 4) >> 2;
	descriptor->enable = 1;
	descriptor->cleared3 = 0;
	descriptor->cleared4 = 0;
	descriptor->slotMode = sbcPcmSlotConfig & 3;
	descriptor->linkBit8 = (sbcPcmLinkConfig & 0x100) >> 8;
	descriptor->valid = 1;
	descriptor->portBit0 = sbcPcmPortConfig & 1;
	descriptor->linkMode = sbcPcmLinkConfig & 3;
	descriptor->linkBit4 = (sbcPcmLinkConfig & 0x10) >> 4;
	descriptor->longFrame = (sbcPcmLinkConfig & 0x200) >> 9;
	descriptor->set5 = 1;
	descriptor->frameSync = descriptor->longFrame ? 0x7f : 0x7c;
	slots = sbcPcmSlotConfig >> 2;
	if (slots == 0)
		slots = 1;
	descriptor->slots = slots;
}

/* Whether the frame drop test drops this frame. */
int btSbcPrepareState(void)
{
	uint32_t count = sbcFrameDrop.count;
	unsigned int threshold = sbcFrameDrop.threshold;

	switch (sbcFrameDrop.mode) {
	case SBC_DROP_CYCLE:
		return (count & 0x7f) < threshold;
	case SBC_DROP_RANDOM:
		sbcFrameDrop.seed = sbcFrameDrop.seed * 0x19660d + 0x7fff;
		return (sbcFrameDrop.seed & 0x7fff) > threshold;
	case SBC_DROP_EVERY:
		if (count < threshold)
			return 0;
		sbcFrameDrop.count = 0;
		return 1;
	default:
		return 0;
	}
}

/* The controller's event filter while the codec devices are open. */
int btSbcCloseCodecDevices(OsaBuffer *event)
{
	return btSbcRomParseContext(event);
}

int btSbcDispatchState(const uint8_t *data)
{
	return btSbcRomBuildContext(data);
}

int btSbcParseState(const uint8_t *data)
{
	return btSbcRomQueryContext(data);
}

/* Opens the codec and PCM devices once and hooks the controller's events. */
int btSbcOpenCodecDevices(void)
{
	if (sbcDevicesOpen == 0) {
		sbcDevices.encoder = mdevOpenDevice("mdev_sbcEnc", 0);
		sbcDevices.decoder = mdevOpenDevice("mdev_sbcDec", 0);
		sbcDevices.pcm = mdevOpenDevice("mdev_bt_pcm", 0);
		sbcDevices.sinkPcm = NULL;
		mdevIoctl(sbcDevices.encoder, SBC_IOCTL_SET_CALLBACK, 0);
		mdevIoctl(sbcDevices.encoder, SBC_IOCTL_SET_MODE, 1);
		mdevIoctl(sbcDevices.decoder, SBC_IOCTL_SET_CALLBACK, 0);
		mdevIoctl(sbcDevices.decoder, SBC_IOCTL_SET_MODE, 1);
		mdevIoctl(sbcDevices.pcm, SBC_IOCTL_SET_CALLBACK, (uintptr_t)btSbcPcmDmaCallback);
		__aeabi_memclr(&sbcStream, sizeof(sbcStream));
		sbcDevicesOpen = 1;
		romHctFilterEvent = btSbcCloseCodecDevices;
		sbcPcmStreamHook = btSbcPcmStreamHandler;
	}
	return 0;
}
