/* Bluetooth audio capabilities; original source filename unavailable. */
#include "bt_internal.h"

int BtSelectAudioCapabilities(BtConnection *connection, unsigned int profile,
			      uint8_t *capabilities, int length)
{
	BtAvdtpState *avdtp = &connection->avdtp;
	unsigned int transmitting = profile & 8;
	unsigned int receiving = profile & 4;
	int sampleRate = 0;

	if (transmitting) {
		avdtp->transmitContentProtection = 0;
		avdtp->minimumBitpool = 0;
		avdtp->maximumBitpool = 0;
	} else if (profile == 4) {
		avdtp->receiveContentProtection = 0;
	}
	while (length > 0) {
		BtAvdtpServiceCapability *service =
			(BtAvdtpServiceCapability *)capabilities;
		unsigned int category = service->header.category;
		unsigned int capabilityLength = service->header.length;

		if (category == 4) {
			if (capabilityLength == 2 && length > 3) {
				BtAvdtpContentProtection *protection =
					(BtAvdtpContentProtection *)service;
				int protectionType;

				BtUnpack(protection->typeLe, 2, "2", &protectionType);
				if (protectionType == 2) {
					BtDevice *device;
					unsigned int protectionProfile;

					if (transmitting) {
						avdtp->transmitContentProtection = 1;
						protectionProfile = 0x200000;
					} else if (receiving) {
						avdtp->receiveContentProtection = 1;
						protectionProfile = 0x100000;
					} else {
						goto nextCapability;
					}
					device = connection->device;
					if (!(device->profiles & protectionProfile)) {
						device->profiles |= protectionProfile;
						device->generation++;
						BtFlushConnectionDevice(connection);
					}
				}
				goto nextCapability;
			}
		} else if (category == 7) {
			BtAvdtpSbcCapability *sbc = (BtAvdtpSbcCapability *)service;
			unsigned int frequencies;
			unsigned int blocks;
			unsigned int minimumBitpool;

			if (capabilityLength != 6 || length <= 7)
				goto advance;
			if (transmitting) {
				frequencies = sbc->frequencyChannels;
				if (!(frequencies & 0x10))
					return 7;
				sbc->frequencyChannels = frequencies & 0x1f;
				if (frequencies & 1) {
					sbc->frequencyChannels = frequencies & 0x11;
					avdtp->channelMode = 3;
				} else if (frequencies & 2) {
					sbc->frequencyChannels = frequencies & 0x13;
					avdtp->channelMode = 2;
				} else if (frequencies & 4) {
					sbc->frequencyChannels = frequencies & 0x17;
					avdtp->channelMode = 1;
				} else {
					if (!(frequencies & 8))
						return 7;
					avdtp->channelMode = 0;
				}
				blocks = sbc->blocksSubbandsAllocation;
				if (blocks & 0x10) {
					sbc->blocksSubbandsAllocation = blocks & 0x1f;
					avdtp->blockCount = 16;
				} else if (blocks & 0x20) {
					sbc->blocksSubbandsAllocation = blocks & 0x3f;
					avdtp->blockCount = 12;
				} else if (blocks & 0x40) {
					sbc->blocksSubbandsAllocation = blocks & 0x7f;
					avdtp->blockCount = 8;
				} else {
					if (!(blocks & 0x80))
						return service->header.category;
					avdtp->blockCount = 4;
				}
				blocks = sbc->blocksSubbandsAllocation;
				if (!(blocks & 4))
					return service->header.category;
				sbc->blocksSubbandsAllocation = blocks & 0xf7;
				if (!(blocks & 1))
					return service->header.category;
				minimumBitpool = sbc->minimumBitpool;
				sbc->blocksSubbandsAllocation = blocks & 0xf5;
				avdtp->minimumBitpool = minimumBitpool;
				avdtp->maximumBitpool = sbc->maximumBitpool;
			}
			if (receiving) {
				frequencies = sbc->frequencyChannels;
				if (frequencies & 0x10) {
					sampleRate = 48000;
					frequencies &= 0x1f;
					sbc->frequencyChannels = frequencies;
				} else if (frequencies & 0x20) {
					frequencies &= 0x3f;
					sampleRate = 44100;
					sbc->frequencyChannels = frequencies;
				} else if (frequencies & 0x40) {
					frequencies &= 0x7f;
					sampleRate = 32000;
					sbc->frequencyChannels = frequencies;
				} else {
					if (!(frequencies & 0x80))
						return 0;
					sampleRate = 16000;
				}
				if (frequencies & 1)
					sbc->frequencyChannels = frequencies & 0xf1;
				else if (frequencies & 2)
					sbc->frequencyChannels = frequencies & 0xf3;
				else if (frequencies & 4)
					sbc->frequencyChannels = frequencies & 0xf7;
				else if (!(frequencies & 8))
					return 0;
				blocks = sbc->blocksSubbandsAllocation;
				if (blocks & 0x10) {
					blocks &= 0x1f;
					sbc->blocksSubbandsAllocation = blocks;
				} else if (blocks & 0x20) {
					blocks &= 0x3f;
					sbc->blocksSubbandsAllocation = blocks;
				} else if (blocks & 0x40) {
					blocks &= 0x7f;
					sbc->blocksSubbandsAllocation = blocks;
				} else if (!(blocks & 0x80)) {
					return 0;
				}
				if (blocks & 4) {
					blocks &= 0xf7;
					sbc->blocksSubbandsAllocation = blocks;
				} else if (!(blocks & 8)) {
					return service->header.category;
				}
				if (blocks & 1)
					sbc->blocksSubbandsAllocation = blocks & 0xfd;
				else if (!(blocks & 2))
					return service->header.category;
			}
			goto nextCapability;
		} else if (category == 1 && capabilityLength != 0) {
			return category;
		}
		goto advance;
nextCapability:
		capabilityLength = service->header.length;
advance:
		capabilities += capabilityLength + 2;
		length -= capabilityLength + 2;
	}
	if (transmitting) {
		int packetSize;
		int blockCount;
		int maximumBitpool;

		if (connection->remoteFeatures & 0x8000000000ULL) {
			if (connection->remoteFeatures & 0x4000000ULL)
				packetSize = 534;
			else {
				packetSize = 321;
				if (connection->remoteFeatures & 0x2000000ULL)
					packetSize = 349;
			}
		} else {
			packetSize = 321;
		}
		blockCount = avdtp->blockCount;
		maximumBitpool = (8 * (packetSize / (64 / blockCount) - 13)) /
				blockCount;
		if (maximumBitpool < avdtp->maximumBitpool)
			avdtp->maximumBitpool = maximumBitpool;
	} else {
		avdtp->receiveSampleRate = sampleRate;
		BtNotifyEvent(28, 0, sampleRate, connection->deviceAddress);
	}
	return 0;
}
