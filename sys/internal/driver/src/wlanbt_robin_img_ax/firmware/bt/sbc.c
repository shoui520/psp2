/*
 * The Bluetooth audio source (0x4cee0..): the ROM's SBC codec and PCM port
 * drivers, and the encoder that fills the packets of the A2DP stream.
 * Marvell published no source; the names say what the code does and the file
 * name is inferred.
 */
#include <stdint.h>
#include "robin/btpool.h"
#include "robin/btpower.h"
#include "robin/btbuffer.h"
#include "robin/btsource.h"
#include "robin/btcontext.h"
#include "robin/btrom.h"
#include "robin/hct.h"
#include "robin/eventpower.h"
#include "robin/rom.h"
#include "robin/sbc.h"
#include "robin/soc.h"
#include "robin/sys.h"

uint32_t sbcTimestamp __attribute__((section(".itcm.rw")));
struct SbcMsbcState sbcMsbc __attribute__((section(".itcm.rw"))) = {
	.plcEnable = 1,
	.frameGood = 1,
	.threshold = 20,
	.seed = 1,
};

/* An mSBC frame of silence. */
const uint8_t sbcMsbcSilentFrame[SBC_MSBC_FRAME_LENGTH] = {
	0xad, 0x00, 0x00, 0xc5, 0x00, 0x00, 0x00, 0x00, 0x77, 0x6d, 0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76,
	0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77, 0x6d, 0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd,
	0xb6, 0xdb, 0x77, 0x6d, 0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77,
	0x6d, 0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6c,
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

/* Takes the mSBC receive settings: concealment, the drop test's mode and threshold (little-endian). */
void btSbcConfigureState(const uint8_t *settings)
{
	sbcMsbc.plcEnable = *settings++;
	sbcMsbc.dropMode = *settings++;
	sbcMsbc.threshold = settings[0] | settings[1] << 8;
}

/*
 * A field of two bit pairs (4-5 and 6-7) whose bits must agree: the pair's
 * value (bits 5-6), or 15 when they do not.
 */
__attribute__((noinline)) unsigned int btSbcCheckState(uint8_t value)
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
__attribute__((noinline)) int btSbcPrepareState(void)
{
	uint32_t count = sbcMsbc.count;
	unsigned int threshold = sbcMsbc.threshold;

	switch (sbcMsbc.dropMode) {
	case SBC_DROP_CYCLE:
		return (count & 0x7f) < threshold;
	case SBC_DROP_RANDOM:
		sbcMsbc.seed = sbcMsbc.seed * 0x19660d + 0x7fff;
		return (sbcMsbc.seed & 0x7fff) > threshold;
	case SBC_DROP_EVERY:
		if (count < threshold)
			return 0;
		sbcMsbc.count = 0;
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

__attribute__((noinline)) int btSbcDispatchState(const uint8_t *data)
{
	return btSbcRomBuildContext(data);
}

__attribute__((noinline)) int btSbcParseState(const uint8_t *data)
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

/*
 * Sets the codec and the PCM port up for wideband speech (mSBC): the encoder
 * and the decoder at once, both on one PCM descriptor.
 */
int btSbcBuildState(void)
{
	uint32_t segment;
	struct SbcPcmDescriptor descriptor;

	if (sbcHeld == 0) {
		btSbcRomSyncContext(0);
		sbcPacket = osa_BufferAlloc(&btPacketPool, 0, ~0u);
		sbcDecoderPacket = osa_BufferAlloc(&btPacketPool, 0, ~0u);
		segment = (uint32_t)((uint8_t *)sbcPacket + sbcPacket->dataOffset) >> 16;
		mdevIoctl(sbcDevices.encoder, SBC_IOCTL_SET_BUFFER, (uintptr_t)&segment);
		segment = (uint32_t)((uint8_t *)sbcDecoderPacket + sbcDecoderPacket->dataOffset) >> 16;
		mdevIoctl(sbcDevices.decoder, SBC_IOCTL_SET_BUFFER, (uintptr_t)&segment);
		sbcEncoderConfig.bitpool = SBC_MSBC_BITPOOL;
		sbcEncoderConfig.allocation = 0;
		sbcEncoderConfig.subbandFlags = SBC_MSBC_SUBBAND_FLAGS;
		sbcEncoderConfig.channelMode = 0;
		sbcEncoderConfig.samplingFrequency = 0;
		sbcEncoderConfig.wideband = 1;
		mdevIoctl(sbcDevices.encoder, SBC_IOCTL_CONFIGURE, (uintptr_t)&sbcEncoderConfig);
		sbcFrame.length = SBC_MSBC_FRAME;
		mdevIoctl(sbcDevices.decoder, SBC_IOCTL_CONFIGURE, (uintptr_t)&sbcEncoderConfig);
		sbcDecoderFrame.length = SBC_MSBC_FRAME;
		sbcFrame.dataOffset = sbcPacket->dataOffset;
		sbcFrame.samples = SBC_MSBC_SAMPLES;
		sbcFrame.sampleBytes = SBC_MSBC_SAMPLE_BYTES;
		sbcFrame.active = 1;
		sbcFrame.frames = 0;
		sbcDecoderFrame.dataOffset = sbcDecoderPacket->dataOffset;
		sbcDecoderFrame.samples = SBC_MSBC_SAMPLES;
		sbcDecoderFrame.sampleBytes = SBC_MSBC_SAMPLE_BYTES;
		sbcDecoderFrame.active = 0;
		sbcDecoderFrame.frames = 0;
		btSbcValidateState(&descriptor, (uintptr_t)((uint8_t *)sbcPacket + sbcPacket->dataOffset),
				   (uintptr_t)((uint8_t *)sbcDecoderPacket + sbcDecoderPacket->dataOffset));
		mdevIoctl(sbcDevices.pcm, SBC_IOCTL_SET_BUFFER, (uintptr_t)&descriptor);
		btSbcRomConfigureState(0, 2, 0);
		*(BtHalf *)btLinkSlotRegisters &= ~0x1800;
	}
	return 0;
}

/* An mSBC frame is encoded: has the encoder write the next one into the SCO packet. */
int btSbcComputeState(void)
{
	if (sbcMediaPacket == NULL) {
		sbcMediaPacket = osa_BufferAlloc(&btTablePool, SBC_MSBC_SCO_SIZE, ~0u);
		sbcMediaPacket->transportTag = SBC_SCO_TAG;
		sbcMediaPacket->data[7] = SBC_SCO_TAG;
		sbcMediaPacket->dataOffset = SBC_MEDIA_OFFSET;
		sbcFrame.frames = 0;
		sbcFrame.headerLength = SBC_MSBC_SCO_HEADER;
		if (sbcMediaPacket == NULL)
			return 0;
	}
	sbcMediaPacket->dataOffset = sbcFrame.headerLength;
	sbcPacket->allocationMetadata = (uint32_t)sbcMediaPacket;
	mdevForwardEvent(sbcDevices.encoder, sbcPacket, 1, 4);
	sbcFrame.frames++;
	return 0;
}

/*
 * Sends the SCO packet of the encoded mSBC frames: the HCI SCO header (the
 * connection handle the stream's header holds, the length) and the H2
 * synchronization header with the next sequence number.
 */
int btSbcQueryState(void)
{
	uint8_t *sco;
	unsigned int h2;
	unsigned int sequence;

	mdevRomUpdateState(sbcDevices.encoder, sbcPacket, 1, 4);
	if (sbcFrame.frames == 0)
		return 0;
	sbcMediaPacket->dataOffset = SBC_MEDIA_OFFSET;
	sbcMediaPacket->length = sbcFrame.frames * sbcFrame.length + 2;
	sco = (uint8_t *)sbcMediaPacket + SBC_MEDIA_OFFSET;
	sco[0] = sbcStream.header[0];
	sco[1] = sbcStream.header[1] & 0xf;
	sco[2] = sbcMediaPacket->length;
	sco[3] = SBC_H2_SYNC;
	sequence = sbcMsbc.h2Sequence;
	h2 = ((sco[4] >> 4) << 4) + SBC_H2_SIGNATURE;
	h2 = (h2 & ~0x20) | (sequence & 1) << 5;
	h2 = (h2 & ~0x10) | ((h2 >> 5) & 1) << 4;
	h2 = (h2 & 0x7f) | ((sequence >> 1) & 1) << 7;
	h2 = (h2 & ~0x40) | (h2 >> 7) << 6;
	sco[4] = h2;
	sbcMsbc.h2Sequence = (sequence + 1) & 3;
	btSbcRomResetContext(sco + 5, (sequence + 1) & 3, sequence + 1, h2);
	mdevForwardEvent(btHciDev, sbcMediaPacket, sbcMediaPacket->length + 3, SBC_SCO_TAG);
	sbcMediaPacket = NULL;
	return 0;
}

/*
 * A SCO packet of wideband speech: takes the next one from the receive queue
 * (dropping one that waited while another came), joins it to what is left of
 * the last, finds the next H2 header with the expected sequence number and
 * has the decoder take its frame, or the silent frame when it is lost, and
 * conceals the loss; then the encoder's side of the exchange.
 */
int btSbcResetState(void)
{
	int deliver = 0;
	int havePacket = 0;
	int lost = 0;
	OsaBuffer *packet;
	uint8_t *data;
	uint8_t *frame;
	int16_t *samples;
	unsigned int limit;
	unsigned int length;
	unsigned int i = 0;
	unsigned int sequence;
	uint16_t offset;

	sbcMsbc.frameCalls++;
	if (sbcReceivedPacket == NULL) {
		sbcReceivedPacket = osa_MsgQueueRecv(&sbcReceiveQueue);
		if (sbcReceiveQueue.words[0] != 0) {
			romHctRxHook(sbcReceivedPacket);
			sbcReceivedPacket = osa_MsgQueueRecv(&sbcReceiveQueue);
			lost = 1;
			sbcMsbc.rxSynced = 0;
		}
		packet = sbcReceivedPacket;
		if (packet != NULL) {
			sbcDecoderFrame.frames = 0;
			offset = packet->dataOffset;
			havePacket = 1;
			packet->length = ((uint8_t *)packet + offset)[2];
			sbcDecoderFrame.headerLength = offset + 3;
			packet->dataOffset = offset + 3;
		}
	} else if (sbcDecoderFrame.frames + sbcDecoderFrame.length + 2 <= sbcReceivedPacket->length) {
		havePacket = 1;
	} else {
		packet = osa_MsgQueueRecv(&sbcReceiveQueue);
		if (sbcReceiveQueue.words[0] != 0) {
			romHctRxHook(packet);
			packet = osa_MsgQueueRecv(&sbcReceiveQueue);
			lost = 1;
			sbcMsbc.rxSynced = 0;
		}
		if (packet != NULL) {
			OsaBuffer *rest = sbcReceivedPacket;
			unsigned int restLength;

			havePacket = 1;
			packet->length = ((uint8_t *)packet + packet->dataOffset)[2];
			packet->dataOffset = sbcDecoderFrame.headerLength;
			restLength = rest->length;
			frame = (uint8_t *)rest + rest->dataOffset + restLength;
			length = (uint16_t)(restLength + packet->length);
			rest->length = length;
			if (length > SBC_SCO_MAXIMUM)
				for (;;)
					;
			__aeabi_memcpy(frame, (uint8_t *)packet + sbcDecoderFrame.headerLength, packet->length);
			romHctRxHook(packet);
		}
	}

	limit = (uint16_t)(sbcReceivedPacket->length - sbcDecoderFrame.length - 2);
	if (havePacket) {
		data = (uint8_t *)sbcReceivedPacket + sbcReceivedPacket->dataOffset;
		for (; sbcDecoderFrame.frames + i <= limit; i++) {
			if (btSbcDispatchState(data + i) == 0)
				continue;
			sequence = btSbcCheckState(data[i + 1]);
			if (sbcMsbc.rxSynced == 0) {
				sbcMsbc.rxSequence = (sequence + 1) & 3;
				sbcMsbc.rxSynced = 1;
			} else if (sbcMsbc.rxSequence == sequence) {
				sbcMsbc.rxSequence = (sbcMsbc.rxSequence + 1) & 3;
			} else if (((sbcMsbc.rxSequence + 1) & 3) == sequence) {
				/* One frame was missed: this one is kept for the next call. */
				sbcMsbc.rxSequence = (sequence + 1) & 3;
				limit = 0;
				lost = 1;
				goto consumed;
			} else {
				sbcMsbc.rxSequence = (sequence + 1) & 3;
			}
			i += 2;
			frame = data + i;
			if (btSbcParseState(frame) != 0) {
				btSbcRomComputeContext(frame);
				limit = 0xffff;
				goto found;
			}
			lost = 1;
			break;
		}
		if (limit == 0xffff) {
found:
			if (btSbcPrepareState() != 0)
				limit = 0x7ff;
		}
consumed:
		sbcDecoderFrame.frames += i;
		sbcReceivedPacket->dataOffset = sbcDecoderFrame.headerLength + sbcDecoderFrame.frames;
	}

	if (limit == 0xffff && lost == 0) {
		sbcMsbc.frameGood = 1;
		sbcMsbc.goodFrames++;
		sbcDecoderPacket->allocationMetadata = (uint32_t)sbcReceivedPacket;
		deliver = 1;
	} else {
		if (limit == 0x7ff || lost != 0)
			deliver = 1;
		else
			sbcReceiveEmpty++;
		sbcMsbc.frameGood = 0;
		sbcMsbc.silentFrame = osa_BufferAlloc(&btPacketPool, 0, ~0u);
		frame = (uint8_t *)sbcMsbc.silentFrame + sbcMsbc.silentFrame->dataOffset;
		sbcMsbc.silentFrame->length = SBC_MSBC_FRAME_LENGTH;
		__aeabi_memcpy(frame, sbcMsbcSilentFrame, SBC_MSBC_FRAME_LENGTH);
		btSbcRomComputeContext(frame);
		sbcDecoderPacket->allocationMetadata = (uint32_t)sbcMsbc.silentFrame;
	}
	mdevForwardEvent(sbcDevices.decoder, sbcDecoderPacket, 1, 5);
	if (sbcMsbc.frameGood == 0 && sbcPlcPending == 0)
		sbcPlcStatus = btSbcSetupBuffer(sbcPlcState);
	mdevRomUpdateState(sbcDevices.decoder, sbcDecoderPacket, 1, 5);

	if (deliver != 0) {
		sbcDecoderFrame.frames += sbcDecoderFrame.length;
		packet = sbcReceivedPacket;
		packet->dataOffset = sbcDecoderFrame.headerLength + sbcDecoderFrame.frames;
		if (sbcDecoderFrame.frames == packet->length) {
			romHctRxHook(packet);
			sbcReceivedPacket = NULL;
			goto encode;
		}
	}
	packet = sbcReceivedPacket;
	if (packet != NULL) {
		length = packet->length;
		if ((unsigned int)(sbcDecoderFrame.frames + sbcDecoderFrame.length + 2) > length) {
			/* Moves what is left of the packet to its start. */
			frame = (uint8_t *)packet + packet->dataOffset;
			packet->dataOffset = sbcDecoderFrame.headerLength;
			packet->length = length - sbcDecoderFrame.frames;
			sbcDecoderFrame.frames = 0;
			__aeabi_memcpy((uint8_t *)packet + sbcDecoderFrame.headerLength, frame, packet->length);
		}
	}

encode:
	mdevRomUpdateState(sbcDevices.pcm, sbcPacket, sbcFrame.samples, 8);
	btSbcComputeState();
	samples = (int16_t *)((uint8_t *)sbcDecoderPacket + sbcDecoderPacket->dataOffset);
	sbcMsbc.count++;
	if (sbcMsbc.plcEnable != 0) {
		if (sbcMsbc.frameGood == 1) {
			btSbcRecordBuffer(sbcPlcState, samples, samples);
		} else {
			if (sbcMsbc.silentFrame != NULL)
				osa_BufferFree(sbcMsbc.silentFrame);
			btSbcAdjustBuffer(sbcPlcState, samples, samples);
		}
	}
	mdevForwardEvent(sbcDevices.pcm, sbcDecoderPacket, sbcDecoderFrame.samples, 8);
	btSbcQueryState();
	return 0;
}

/* Takes the SCO stream's requests (SBC_SCO_*), in this order. */
int btSbcSyncState(uint32_t requests)
{
	int result = 0;

	if (requests & SBC_SCO_OPEN)
		result = btSbcOpenCodecDevices();
	if (sbcDevicesOpen == 1) {
		if (requests & SBC_SCO_MSBC)
			result = btSbcBuildState();
		if (requests & SBC_SCO_HOLD)
			result = btSbcResolveState();
		if (requests & SBC_SCO_RESUME)
			result = btSbcScheduleState();
		if (requests & SBC_SCO_FRAME)
			result = btSbcResetState();
		if (requests & SBC_SCO_CLOSE)
			result = btSbcApplyState();
	}
	return result;
}

/* Closes the codec and PCM devices. */
int btSbcApplyState(void)
{
	if (sbcDevicesOpen == 1) {
		sbcHeld = 0;
		mdevReleaseDevice(sbcDevices.encoder);
		mdevReleaseDevice(sbcDevices.decoder);
		mdevReleaseDevice(sbcDevices.pcm);
		sbcDevices.encoder = NULL;
		sbcDevices.decoder = NULL;
		sbcDevices.pcm = NULL;
		romCoexPrimaryFlag = 0;
		sbcDevicesOpen = 0;
		/*
		 * The original passes what mdevReleaseDevice() left in r0; the
		 * ROM routine's use of it is not known.
		 */
		btSbcRomSyncContext(0);
	}
	romHctFilterEvent = NULL;
	return 0;
}

/* Holds the stream: stops the PCM port and starts the receive state over. */
int btSbcResolveState(void)
{
	if (sbcHeld == 0) {
		mdevIoctl(sbcDevices.pcm, SBC_IOCTL_STOP, 0);
		sbcMsbc.h2Sequence = 0;
		sbcMsbc.rxSequence = 0;
		sbcMsbc.rxSynced = 0;
	}
	sbcHeld = 1;
	sbcReceiveEmpty = 0;
	sbcMediaDropped = 0;
	btSbcReleaseBuffer(sbcPlcState);
	sbcMsbc.count = 0;
	return 0;
}

/* Resumes the stream, dropping the packets that came while it was held. */
int btSbcScheduleState(void)
{
	if (sbcHeld == 1) {
		mdevIoctl(sbcDevices.pcm, SBC_IOCTL_START, 0);
		mdevIoctl(sbcDevices.encoder, SBC_IOCTL_RESTART, 0);
		mdevIoctl(sbcDevices.decoder, SBC_IOCTL_RESTART, 0);
	}
	sbcHeld = 0;
	if (sbcMediaPacket != NULL) {
		osa_BufferFree(sbcMediaPacket);
		sbcMediaPacket = NULL;
	}
	do {
		if (sbcReceivedPacket != NULL)
			romHctRxHook(sbcReceivedPacket);
		sbcReceivedPacket = osa_MsgQueueRecv(&sbcReceiveQueue);
	} while (sbcReceivedPacket != NULL);
	return 0;
}

/* Sets the PCM port up for a stream without a codec (CVSD): 240 samples a frame. */
int btSbcNotifyState(void)
{
	struct SbcPcmRequest request;
	struct SbcPcmConfig config;

	if (sbcHeld == 0) {
		if (sbcPacket != NULL) {
			osa_BufferFree(sbcPacket);
			sbcPacket = NULL;
		}
		if (sbcDecoderPacket != NULL) {
			osa_BufferFree(sbcDecoderPacket);
			sbcDecoderPacket = NULL;
		}
		sbcPacket = osa_BufferAlloc(&btPacketPool, 0, ~0u);
		sbcFrame.dataOffset = sbcPacket->dataOffset;
		sbcFrame.samples = SBC_MSBC_SAMPLES;
		sbcFrame.sampleBytes = SBC_MSBC_SAMPLE_BYTES;
		sbcFrame.active = SBC_FRAME_PCM;
		sbcFrame.frames = 0;
		request.mode = 2;
		request.length = SBC_MSBC_SAMPLE_BYTES;
		request.data = (uint8_t *)sbcPacket + sbcPacket->dataOffset;
		mdevIoctl(sbcDevices.pcm, SBC_IOCTL_QUEUE, (uintptr_t)&request);
		config.enable = 1;
		config.mono = 1;
		config.cleared[0] = 0;
		config.sampleBits = 8;
		config.cleared[3] = 0;
		config.cleared[1] = 0;
		config.cleared[2] = 0;
		config.rate = 0;
		mdevIoctl(sbcDevices.pcm, SBC_IOCTL_SET_BUFFER, (uintptr_t)&config);
		mdevIoctl(sbcDevices.pcm, SBC_IOCTL_CONFIGURE, (uintptr_t)&config);
	}
	return 0;
}

/* Sends a frame of PCM samples to the host as a SCO packet (CVSD). */
int btSbcFormatState(void)
{
	OsaBuffer *packet;
	uint8_t *sco;

	sbcDecoderPacket = osa_BufferAlloc(&btTablePool, 0x100, ~0u);
	packet = sbcDecoderPacket;
	if (packet != NULL) {
		packet->dataOffset = 32;
		packet->transportTag = SBC_SCO_TAG;
		packet->data[7] = SBC_SCO_TAG;
		packet = sbcDecoderPacket;
		packet->length = sbcFrame.samples;
		packet->dataOffset = SBC_PCM_SCO_OFFSET;
		packet->length = sbcFrame.samples;
		mdevRomUpdateState(sbcDevices.pcm, packet, sbcFrame.event == 2 ? 0 : sbcFrame.samples, 8);
		packet = sbcDecoderPacket;
		packet->dataOffset = SBC_MEDIA_OFFSET;
		sco = (uint8_t *)packet + SBC_MEDIA_OFFSET;
		sco[0] = sbcStream.header[0];
		sco[1] = sbcStream.header[1] & 0xf;
		sco[2] = sbcDecoderPacket->length;
		mdevForwardEvent(btHciDev, sbcDecoderPacket, sbcDecoderPacket->length + 3, SBC_SCO_TAG);
		sbcDecoderPacket = NULL;
	}
	sbcFrame.event = 0;
	return 0;
}

/* The CVSD stream's requests (SBC_SCO_*; SBC_SCO_FRAME is 0x100 here), in this order. */
int btSbcInspectState(uint32_t requests, uintptr_t argument)
{
	int result = 0;

	if (requests & SBC_SCO_OPEN)
		result = btSbcRomCheckContext(argument);
	if (sbcDevicesOpen == 1) {
		if (requests & SBC_SCO_MSBC)
			result = btSbcNotifyState();
		if (requests & SBC_SCO_HOLD)
			result = btSbcRomPrepareContext();
		if (requests & SBC_SCO_RESUME)
			result = btSbcRomDispatchContext();
		if (requests & 0x100)
			result = btSbcFormatState();
		if (requests & SBC_SCO_CLOSE)
			result = btSbcRomValidateContext();
	}
	return result;
}

/* The audio clock changed: halves its dividers when the path presets took the new rate. */
int btSbcTrackState(void)
{
	uint32_t keep;
	uint32_t divider;

	btSbcRomUpdateState();
	keep = SBC_AUDIO_CLOCK & SBC_AUDIO_CLOCK_KEEP;
	if (sysPathPresetApply() != 0) {
		SBC_AUDIO_CLOCK &= ~SBC_AUDIO_CLOCK_KEEP;
		if (keep == 0) {
			divider = SBC_AUDIO_CLOCK & 0xf;
			if (divider != 0)
				divider = ((divider + 1) >> 1) - 1;
			SBC_AUDIO_CLOCK = (SBC_AUDIO_CLOCK >> 4) << 4 | divider;
			divider = (SBC_AUDIO_CLOCK >> 8) & 0xff;
			SBC_AUDIO_CLOCK = (SBC_AUDIO_CLOCK & ~0xff00u) | (((divider + 1) >> 1) - 1) << 8;
		}
	}
	return 0;
}

int (*btSbcManageState(void))(void)
{
	sbcClockHook = btSbcTrackState;
	return btSbcTrackState;
}

/* The SCO line's interrupt: the SQRAM code that waits on it, else the link's frame timing. */
int btSbcAcquireState(int line, uint32_t pending)
{
	(void)line;
	(void)pending;
	SOC_IRQ_ENABLE_CLEAR = 1u << SBC_SCO_IRQ_LINE;
	if (p2pSqramTableActive != 0) {
		p2pSqramUpdateTable();
	} else if (p2pSqramEntryActive != 0) {
		p2pSqramSetupEntry();
	} else {
		btLmpLcComputeContext();
		sysUpdateEventDeadline();
		btLmpLcClearFrame();
	}
	SOC_IRQ_ENABLE_SET |= 1u << SBC_SCO_IRQ_LINE;
	return 1;
}

struct SbcIrqHandler sbcIrqHandlers[] __attribute__((section(".itcm.rw"))) = {
	{ btSbcAcquireState, SBC_SCO_IRQ_LINE },
	{ NULL, 0 },
};

void btSbcReleaseState(void)
{
	struct SbcIrqHandler *entry;

	for (entry = sbcIrqHandlers; entry->handler != NULL; entry++)
		sysIrqRegister(entry->line, entry->handler);
}

void btSbcSetupState(void)
{
}

/* Unmasks the SCO line's interrupt. */
void btSbcClearState(void)
{
	SOC_IRQ_ENABLE_SET |= 1u << SBC_SCO_IRQ_LINE;
}

/* Points the ROM's link controller and SBC hooks at the image's code. */
void btSbcConvertState(void)
{
	romLmpPrepareHook = btLmpLcPrepareContext;
	romLmpDispatchHook = btLmpLcDispatchContext;
	romLmpParseHook = btLmpLcParseContext;
	romLmpAcquireHook = btLmpLcAcquireState;
	btLmpLcQueryEntry();
	btLmpLcManageState();
	btSbcPreparePacket();
	romSbcRecordHook = btSbcRecordContext;
	romSbcFilterHook = btSbcFilterContext;
	btBdhRomComputeState();
	romSbcProcessHook = btSbcProcessPacket;
	romSbcUpdateHook = btSbcUpdatePacket;
}
