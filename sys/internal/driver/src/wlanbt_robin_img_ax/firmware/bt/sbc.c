/*
 * The Bluetooth audio source (0x4cee0..): the ROM's SBC codec and PCM port
 * drivers, and the encoder that fills the packets of the A2DP stream.
 * Marvell published no source; the names say what the code does and the file
 * name is inferred.
 */
#include <stdint.h>
#include "robin/btpool.h"
#include "robin/btrom.h"
#include "robin/rom.h"
#include "robin/sbc.h"

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
