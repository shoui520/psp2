#include "spkg_peripherals.h"
#include "syscon_update.h"

extern int updateSysconFirmwareUpdateStarted;
extern int updateSysconShutdownWorkaround;
extern int updateSysconLargeChunks;
int SceSysconForDriver_D2F456DC(uint32_t *version);
int SceSysconForDriver_CBA836FF(const void *signature, unsigned int size);
int ksceSysconUpdaterSetRunMode(int mode);
int ksceSysconUpdaterCalcChecksum(const void *data, unsigned int size, int *checksum);
int ksceSysconUpdaterSetSegment(unsigned int segment);
int ksceSysconUpdaterExecProgramming(int checksum);
int ksceSysconUpdaterExecFinalize(const void *digest, unsigned int size);
int SysconUtilHandshake(void);
int UpdateUtilUpdateNodeStatus(unsigned int nodeType, UpdateUtilNodeStatus *status);
int SceSysconForDriver_356B9696(const void *data, unsigned int offset, unsigned int size);
int SceSysconForDriver_734544E4(const void *data, unsigned int offset, unsigned int size);

int SysconUtilWriteFirmware(int kind, const void *buffer, uint64_t size,
		unsigned int nodeType, UpdateUtilNodeStatus *status)
{
	const unsigned char *cursor = buffer;
	uint64_t remaining = size;
	unsigned int downloaderVersion;
	int runMode;
	int result;
	int (*sendProgramData)(const void *, unsigned int, unsigned int) = SceSysconForDriver_356B9696;
	int (*sendProgramDataLarge)(const void *, unsigned int, unsigned int) = SceSysconForDriver_734544E4;

	if (!size || !buffer)
		return (int)0x800f0216;
	switch (kind) {
	case 2:
		runMode = 0x3665;
		break;
	case 3:
		runMode = 0xc5e7;
		break;
	case 0:
		runMode = 0x9a54;
		updateSysconFirmwareUpdateStarted = 1;
		break;
	default:
		return (int)0x800f0216;
	}
	downloaderVersion = updateSysconDownloaderVersion & 0xffff00;
	if (downloaderVersion >= 0x1700)
		updateSysconLargeChunks = 1;
	if (downloaderVersion == 0x1800 || downloaderVersion == 0x1900 ||
	    downloaderVersion == 0x2000)
		updateSysconShutdownWorkaround = 1;
	if (runMode == 0x9a54) {
		uint32_t version;

		if (!SceSysconForDriver_D2F456DC(&version))
			updateSysconDownloaderVersion = version;
		result = ksceSysconUpdaterSetRunMode(0x9a54);
		if (result)
			return result;
		if ((updateSysconDownloaderVersion & 0xffffff) >= 0x3600) {
			result = SysconUtilHandshake();
			if (result)
				return result;
		} else {
			updateUseSysconSnvs = 0;
		}
	} else {
		result = ksceSysconUpdaterSetRunMode(runMode);
		if (result)
			return result;
	}
	while ((int64_t)remaining > 0) {
		const SysconUpdatePacket *packet = (const SysconUpdatePacket *)cursor;

		if ((int64_t)remaining < 4)
			return (int)0x800f0216;
		switch (packet->header.type) {
		case 1:
		case 2:
			remaining -= sizeof(SysconUpdateVersionPacket);
			cursor += sizeof(SysconUpdateVersionPacket);
			break;
		case 3:
			result = SceSysconForDriver_CBA836FF(packet->signature.signature, 24);
			remaining -= sizeof(SysconUpdateSignaturePacket);
			cursor += sizeof(SysconUpdateSignaturePacket);
			if (result)
				return result;
			result = ksceSysconUpdaterSetRunMode(runMode);
			if (result)
				return result;
			break;
		case 0x10: {
			int segmentSize = packet->segment.size;
			const unsigned char *data = (const unsigned char *)(&packet->segment + 1);
			int checksum = 0;
			unsigned int segmentNumber = packet->segment.segmentNumber;
			int unwritten = segmentSize;
			int maxChunkSize;
			unsigned int offset = 0;
			int (*send)(const void *, unsigned int, unsigned int);

			result = ksceSysconUpdaterCalcChecksum(data, segmentSize, &checksum);
			if (result)
				return result;
			result = ksceSysconUpdaterSetSegment((unsigned char)segmentNumber);
			if (result)
				return result;
			if (updateSysconLargeChunks) {
				maxChunkSize = 64;
				send = sendProgramDataLarge;
			} else {
				maxChunkSize = 16;
				send = sendProgramData;
			}
			while (unwritten > 0) {
				int chunkSize = unwritten >= maxChunkSize ? maxChunkSize : unwritten;

				result = send(data, offset, (unsigned char)chunkSize);
				if (result)
					return result;
				unwritten -= chunkSize;
				offset = (unsigned short)(offset + chunkSize);
				data += chunkSize;
			}
			result = ksceSysconUpdaterExecProgramming(checksum);
			if (result)
				return result;
			remaining -= sizeof(SysconUpdateSegmentHeader);
			remaining -= (uint32_t)segmentSize;
			cursor = (const unsigned char *)(&packet->segment + 1) + segmentSize;
			break;
		}
		case 0x20:
			result = ksceSysconUpdaterExecFinalize(packet->checksum.digest, 20);
			remaining -= sizeof(SysconUpdateChecksumPacket);
			cursor += sizeof(SysconUpdateChecksumPacket);
			if (result)
				return result;
			break;
		default:
			return (int)0x80010016;
		}
		if (status) {
			status->progress = 3;
			++status->seq_no;
			status->written_rates = 100 - 100 * (unsigned int)(remaining / size);
			UpdateUtilUpdateNodeStatus(nodeType, status);
		}
	}
	if ((unsigned int)(kind - 2) <= 1)
		return ksceSysconUpdaterSetRunMode(0x72ba);
	return 0;
}
