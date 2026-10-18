/* SceUsbstorVStorDriver: file-backed USB storage. Private names unknown. */
#include <stddef.h>
#include <stdint.h>
#include <psp2kern/udcd.h>
#include <acmgr/acmgr.h>
#include <psp2kern/kernel/iofilemgr.h>
#include <modulemgr/modulemgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/threadmgr.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <kernel/error.h>
#include "usbstorvstor.h"
#include <intrmgr/syscall.h>

/* Original VStor error names are absent from the SDK. */
#define VSTOR_ERROR_ARGUMENT ((int)0x80244103)
#define VSTOR_ERROR_MEMORY ((int)0x8024410a)
#define VSTOR_ERROR_PATH ((int)0x80244112)
#define VSTOR_ERROR_TYPE ((int)0x80244113)
#define VSTOR_ERROR_ACTIVE ((int)0x80244114)
#define VSTOR_ERROR_INACTIVE ((int)0x80244115)
#define VSTOR_ERROR_LOCATION ((int)0x80244116)

/* s.eventFlag bits */
#define EVF_SEND		0x0001	/* the worker sends s.transfer to the host */
#define EVF_WRITE		0x0002	/* the worker writes it to the image */
#define EVF_SENT		0x0010
#define EVF_WRITTEN		0x0020
#define EVF_CLOSE		0x0100
#define EVF_OPEN		0x0200
#define EVF_EXIT		0x0400

/* s.pipeline: the transfers the worker overlaps with the image I/O */
#define PIPELINE_READ		EVF_SEND
#define PIPELINE_WRITE		EVF_WRITE

#define USBSTOR_TYPE_NONE	0xff
#define VSTOR_PID_DISK		0x5d3	/* USB product IDs */
#define VSTOR_PID_CDROM		0x69a
#define TRANSFER_TIMEOUT	1000000
#define RESPONSE_INQUIRY_SIZE	56

/* the FAT boot sector of a disk image */
struct FatBootSector {
	uint8_t jump[3];
	char oemName[8];
	uint16_t bytesPerSector;
	uint8_t sectorsPerCluster;
	uint16_t reservedSectors;
	uint8_t fats;
	uint16_t rootEntries;
	uint16_t totalSectors16;
	uint8_t media;
	uint16_t sectorsPerFat;
	uint16_t sectorsPerTrack;
	uint16_t heads;
	uint32_t hiddenSectors;
	uint32_t totalSectors;
	uint8_t bootCode[474];
	uint8_t signature[2];			/* 0x55 0xaa */
} __attribute__((packed));

#define FAT_SIGNATURE		0x55aa

/* the ISO 9660 primary volume descriptor of a disc image, up to its block size */
struct IsoVolumeDescriptor {
	uint8_t type;				/* ISO_VD_PRIMARY */
	char identifier[5];			/* "CD001" */
	uint8_t version;
	uint8_t reserved[121];
	uint16_t logicalBlockSize;		/* both-endian: little endian first */
	uint16_t logicalBlockSizeBE;
};

#define ISO_VD_PRIMARY		1
#define ISO_VD_OFFSET		0x8000	/* system area: 16 sectors of 2048 bytes */

/* standard INQUIRY data */
struct ScsiInquiryData {
	uint8_t peripheral;			/* USBSTOR_TYPE_* */
	uint8_t removable;			/* 0x80: removable medium */
	uint8_t version;
	uint8_t responseFormat;
	uint8_t additionalLength;
	uint8_t flags[3];
	char vendor[8];
	char product[16];
	char revision[4];
	char vendorSpecific[20];
};

static struct {
	int manager, speed, changed, fd;
	uint32_t blocks;
	void *transfer;
	unsigned transferSize;
	int transferResult, pending;
	unsigned readLimit, writeLimit, verifyLimit, pipeline;
	int thread, eventFlag, commandBlock, dataBlock;
	uint8_t *buffers[2], *response;
	unsigned blockSize, type;
	char path[256];
	struct UsbstorFunction driver;
} s;

static int command(int unit, const uint8_t *cdb, struct UsbstorSense *sense);
static int openImage(void);
static int closeImage(void);

static int authorized(void)
{
	return SceSblACMgrForDriver_D0E11C89(0) || sceSblACMgrIsSystemProgram(0);
}

static int setSense(struct UsbstorSense *sense, int key, int asc)
{
	sense->key = key;
	sense->asc = asc;
	sense->ascq = 0;
	return USBSTOR_COMMAND_FAILED;
}

static unsigned be16(const uint8_t *p)
{
	return (unsigned)p[0] << 8 | p[1];
}

static unsigned be32(const uint8_t *p)
{
	return (unsigned)p[0] << 24 | (unsigned)p[1] << 16 |
	       (unsigned)p[2] << 8 | p[3];
}

static void put32(uint8_t *p, unsigned value)
{
	p[0] = value >> 24;
	p[1] = value >> 16;
	p[2] = value >> 8;
	p[3] = value;
}

static void clearBytes(uint8_t *p, unsigned size)
{
	while (size--)
		*p++ = 0;
}

static void copyBytes(uint8_t *p, const uint8_t *q, unsigned size)
{
	while (size--)
		*p++ = *q++;
}

static int sendResponse(unsigned size)
{
	return SceUsbstorForDriver_D3C7EE70(s.response, size, TRANSFER_TIMEOUT) < 0 ?
	       USBSTOR_COMMAND_FAILED : USBSTOR_COMMAND_DONE;
}

static int onEvent(int event, int argument)
{
	if (event == USBSTOR_EVENT_ATTACH)
		s.speed = argument;
	else if (event == USBSTOR_EVENT_DETACH)
		s.speed = 0;
	s.changed = 1;
	return 0;
}

static int transferThread(SceSize size, void *args)
{
	unsigned bits;

	for (;;) {
		sceKernelWaitEventFlag(s.eventFlag, EVF_OPEN,
					SCE_KERNEL_EVF_WAITMODE_OR |
					SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					&bits, NULL);
		while (sceKernelWaitEventFlag(s.eventFlag, EVF_EXIT | EVF_CLOSE | EVF_WRITE | EVF_SEND,
					       SCE_KERNEL_EVF_WAITMODE_OR |
					       SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits,
					       NULL) >= 0) {
			if (bits & EVF_CLOSE)
				break;
			if (bits & EVF_EXIT) {
				sceKernelExitThread(0);
				return 0;
			}
			if (bits & EVF_SEND) {
				s.transferResult = SceUsbstorForDriver_D3C7EE70(s.transfer,
						 s.transferSize, TRANSFER_TIMEOUT);
				sceKernelSetEventFlag(s.eventFlag, EVF_SENT);
			} else if (bits & EVF_WRITE) {
				s.transferResult = ksceIoWrite(s.fd, s.transfer, s.transferSize);
				sceKernelSetEventFlag(s.eventFlag, EVF_WRITTEN);
			}
		}
	}
}

static int closeImage(void)
{
	sceKernelSetEventFlag(s.eventFlag, EVF_CLOSE);
	if (s.fd >= 0)
		ksceIoClose(s.fd);
	s.fd = -1;
	s.blocks = 0;
	if (s.dataBlock >= 0) {
		SceSysmemForDriver_009E1C61(s.dataBlock);
		s.dataBlock = -1;
	}
	SceSysmemForDriver_009E1C61(s.commandBlock);
	s.buffers[0] = NULL;
	s.buffers[1] = NULL;
	return 0;
}

static int openImage(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	union {
		uint8_t sector[512];
		struct FatBootSector fat;
		struct IsoVolumeDescriptor iso;
	} image;
	void *base;
	int result, permission;
	int64_t length;

	s.driver.busy = 0;
	s.changed = 0;
	s.blocks = 0;
	s.pipeline = 0;
	s.speed = 2;
	s.fd = -1;
	s.verifyLimit = 0x80;
	s.readLimit = 0x100;
	s.writeLimit = 0x100;
	result = SceUsbstorForDriver_73F2FF9D(6);	/* SCSI transparent command set */
	if (result < 0)
		return result;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT;
	s.commandBlock = SceSysmemForDriver_C94850C9("SceUsbstorVstor", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_RW,
						 0x1000, &opt);
	if (s.commandBlock < 0)
		return VSTOR_ERROR_MEMORY;
	if (SceSysmemForDriver_A841EDDA(s.commandBlock, &base) < 0) {
		result = VSTOR_ERROR_MEMORY;
		goto freeCommand;
	}
	s.response = base;
	if (!s.buffers[0]) {
		s.dataBlock = SceSysmemForDriver_C94850C9("SceUsbstorExt", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_RW,
						      0x20000, &opt);
		if (s.dataBlock < 0)
			return VSTOR_ERROR_MEMORY;
		if (SceSysmemForDriver_A841EDDA(s.dataBlock, &base) < 0) {
			SceSysmemForDriver_009E1C61(s.dataBlock);
			result = VSTOR_ERROR_MEMORY;
			goto freeData;
		}
		s.buffers[0] = base;
		s.buffers[1] = (uint8_t *)base + s.blockSize * s.verifyLimit;
	}
	result = VSTOR_ERROR_PATH;
	if (s.type == USBSTOR_TYPE_DISK) {
		permission = sceKernelSetThreadAccessLevel(0x40);
		s.fd = ksceIoOpen(s.path, SCE_O_RDWR, SCE_S_IWSYS | SCE_S_IRSYS);
		length = ksceIoLseek(s.fd, 0, SCE_SEEK_SET);
		if (length >= 0 && ksceIoRead(s.fd, image.sector, sizeof(image.sector)) >= 0 &&
		    be16(image.fat.signature) == FAT_SIGNATURE && image.fat.totalSectors &&
		    image.fat.bytesPerSector == 512) {
			s.blocks = image.fat.totalSectors;
			s.blockSize = image.fat.bytesPerSector;
			length = ksceIoLseek(s.fd, 0, SCE_SEEK_END);
			if (length >= 0)
				result = 0;
		}
		sceKernelSetThreadAccessLevel(permission);
	} else if (s.type == USBSTOR_TYPE_CDROM) {
		s.fd = ksceIoOpen(s.path, SCE_O_RDONLY, SCE_S_IRSYS);
		length = ksceIoLseek(s.fd, ISO_VD_OFFSET, SCE_SEEK_SET);
		if (length >= 0 && ksceIoRead(s.fd, image.sector, sizeof(image.iso)) >= 0 &&
		    image.iso.type == ISO_VD_PRIMARY && strncmp(image.iso.identifier, "CD001", 5) == 0) {
			s.blockSize = image.iso.logicalBlockSize;
			s.blocks = 0x2e7;		/* fixed by the original */
			if (s.blockSize == 2048) {
				length = ksceIoLseek(s.fd, 0, SCE_SEEK_END);
				if (length >= 0)
					result = 0;
			}
		}
	} else {
		result = VSTOR_ERROR_TYPE;
	}
	if (result >= 0) {
		/* The original accepts short images when open() returned a valid fd. */
		if (length / s.blockSize >= s.blocks || s.fd >= 0) {
			sceKernelSetEventFlag(s.eventFlag, EVF_OPEN);
			return 0;
		}
	}
freeData:
	if (s.dataBlock >= 0) {
		SceSysmemForDriver_009E1C61(s.dataBlock);
		s.dataBlock = -1;
		s.buffers[0] = NULL;
		s.buffers[1] = NULL;
	}
freeCommand:
	SceSysmemForDriver_009E1C61(s.commandBlock);
	return result;
}

static int waitTransfer(unsigned bit)
{
	if (s.pending) {
		if (sceKernelWaitEventFlag(s.eventFlag, bit,
					    SCE_KERNEL_EVF_WAITMODE_OR |
					    SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					    NULL, NULL) < 0)
			return -1;
		s.pending = 0;
	}
	return s.transferResult < 0 ? -1 : 0;
}

static int transferBlocks(unsigned block, unsigned count, struct UsbstorSense *sense, int write)
{
	unsigned index = 0, first = 1, limit, n, bytes, mask = write ? EVF_WRITE : EVF_SEND;
	unsigned completion = write ? EVF_WRITTEN : EVF_SENT;
	int result = USBSTOR_COMMAND_FAILED;

	if (s.fd < 0)
		return setSense(sense, SCSI_KEY_HARDWARE_ERROR, SCSI_ASC_INTERNAL_FAILURE);
	if (block >= s.blocks || count > s.blocks - block)
		return setSense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_LBA_OUT_OF_RANGE);
	if (!count)
		return USBSTOR_COMMAND_DONE;
	s.driver.busy = 1;
	if (ksceIoLseek(s.fd, (uint64_t)block * s.blockSize, SCE_SEEK_SET) < 0) {
		setSense(sense, SCSI_KEY_MEDIUM_ERROR, SCSI_ASC_NO_SEEK_COMPLETE);
		goto done;
	}
	limit = write ? s.writeLimit : s.readLimit;
	while (count) {
		n = count < limit ? count : limit;
		bytes = n * s.blockSize;
		result = write ? SceUsbstorForDriver_F1A690DC(s.buffers[index], bytes, TRANSFER_TIMEOUT) :
				 ksceIoRead(s.fd, s.buffers[index], bytes);
		if (result < 0) {
			if (s.pending && sceKernelWaitEventFlag(s.eventFlag, completion,
								 SCE_KERNEL_EVF_WAITMODE_OR |
								 SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
								 NULL, NULL) >= 0)
				s.pending = 0;
			if (!write)
				setSense(sense, SCSI_KEY_MEDIUM_ERROR, SCSI_ASC_UNRECOVERED_READ_ERROR);
			result = USBSTOR_COMMAND_FAILED;
			goto done;
		}
		if (!first && (s.pipeline & mask) && waitTransfer(completion) < 0)
			goto failed;
		s.transferResult = 0x80244101;		/* until the worker reports */
		s.pending = 1;
		s.transfer = s.buffers[index];
		s.transferSize = bytes;
		sceKernelSetEventFlag(s.eventFlag, mask);
		if (!(s.pipeline & mask)) {
			if (waitTransfer(completion) < 0)
				goto failed;
		} else {
			index ^= 1;
		}
		count -= n;
		first = 0;
	}
	if ((s.pipeline & mask) && waitTransfer(completion) < 0)
		goto failed;
	result = USBSTOR_COMMAND_DONE;
	goto done;
failed:
	if (write)
		setSense(sense, SCSI_KEY_MEDIUM_ERROR, SCSI_ASC_WRITE_FAULT);
	result = USBSTOR_COMMAND_FAILED;
done:
	s.driver.busy = 0;
	return result;
}

static int modeSense(const uint8_t *cdb, struct UsbstorSense *sense, unsigned requested, unsigned header)
{
	uint8_t *p = s.response + header;
	unsigned page = cdb[2] & 0x3f, control = cdb[2] >> 6, size = header;
	static const uint8_t managerPage[] = { 0x22, 8, 'E', 'M', 'P', 'R', '-', 'M', ' ', ' ' };
	static const uint8_t marlinPage[] = { 0x3d, 0x3e, 'M', 'A', 'R', 'L', 'I', 'N', 2 };

	if (control == 3)
		return setSense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_SAVING_NOT_SUPPORTED);
	if (control == 1)
		return setSense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB);
	if (control || page) {
		if (cdb[3] != 0 && cdb[3] != 0xff)
			return setSense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB);
		switch (page) {
		case 1:
			clearBytes(p + 2, 10);
			p[0] = 1;
			p[1] = 10;
			size += 12;
			break;
		case 8:
			memset(p, 0, 20);
			p[0] = 8;
			p[1] = 18;
			p[2] = 1;
			size += 20;
			break;
		case 0x22:
			if (!s.manager)
				return setSense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB);
			copyBytes(p, managerPage, 10);
			size += 10;
			break;
		case 0x3d:
			if (s.manager != 1)
				return setSense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB);
			memset(p, 0, 64);
			copyBytes(p, marlinPage, 9);
			p[10] = p[11] = p[12] = p[13] = 0xff;
			size += 64;
			break;
		case 0x3f:
			clearBytes(p + 2, 10);
			p[0] = 1;
			p[1] = 10;
			memset(p + 12, 0, 20);
			p[12] = 8;
			p[13] = 18;
			p[14] = 1;
			size += 32;
			if (s.manager) {
				copyBytes(p + 32, managerPage, 10);
				size += 10;
				if (s.manager == 1) {
					memset(p + 42, 0, 64);
					copyBytes(p + 42, marlinPage, 9);
					p[52] = p[53] = p[54] = p[55] = 0xff;
					size += 64;
				}
			}
			break;
		default:
			return setSense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB);
		}
	}
	if (header == 4) {
		s.response[0] = size - 1;
		clearBytes(s.response + 1, 3);
	} else {
		s.response[0] = (size - 2) >> 8;
		s.response[1] = size - 2;
		clearBytes(s.response + 2, 6);
	}
	return sendResponse(requested < size ? requested : size);
}

static int inquiry(const uint8_t *cdb)
{
	struct ScsiInquiryData *data = (struct ScsiInquiryData *)s.response;
	unsigned size = cdb[4];

	memset(data, 0, RESPONSE_INQUIRY_SIZE);
	if (s.type == USBSTOR_TYPE_DISK || s.type == USBSTOR_TYPE_CDROM) {
		if (s.type == USBSTOR_TYPE_DISK) {
			data->peripheral = USBSTOR_TYPE_DISK;
			data->removable = 0x80;
			data->version = 0;
			data->responseFormat = 1;
			data->additionalLength = 0x33;
		} else {
			data->peripheral = USBSTOR_TYPE_CDROM;
			data->removable = 0;
			data->version = 2;
			data->responseFormat = 2;
			data->additionalLength = 0x1f;
		}
		SceUsbstorForDriver_B6689597(0, 0, data->vendor, sizeof(data->vendor));
		SceUsbstorForDriver_B6689597(0, 1, data->product, sizeof(data->product));
		SceUsbstorForDriver_B6689597(0, 2, data->revision, sizeof(data->revision));
		if (s.type == USBSTOR_TYPE_DISK) {
			static const uint8_t name[] = { 'E', 'M', 'F', 'M', 'T', ' ', ' ', ' ' };
			copyBytes((uint8_t *)data->vendorSpecific, name, 8);
		}
	}
	return sendResponse(size < RESPONSE_INQUIRY_SIZE ? size : RESPONSE_INQUIRY_SIZE);
}

static int devicePage(uint8_t *p)
{
	clearBytes(p, 16);
	p[0] = 1;
	p[3] = 0x80;
	memset(p + 16, 0x20, 16);
	SceUsbstorForDriver_B6689597(0, 0, p + 16, 16);
	memset(p + 32, 0x20, 16);
	SceUsbstorForDriver_B6689597(0, 1, p + 32, 16);
	memset(p + 48, 0x20, 8);
	SceUsbstorForDriver_B6689597(0, 2, p + 48, 8);
	clearBytes(p + 56, 8);
	memset(p + 64, 0x20, 48);
	memset(p + 112, 0, 16);
	return 128;
}

static int vendorPage(const uint8_t *cdb, struct UsbstorSense *sense)
{
	uint8_t *p = s.response;
	unsigned page = be16(cdb + 2), requested = be16(cdb + 8), size;
	static const uint8_t capabilities[] = { 1, 2, 3, 0x11, 0x12, 0x13, 0x21, 0x22 };

	clearBytes(p + 4, 4);
	if (requested > 256)
		requested = 256;
	switch (page) {
	case 0:
		return setSense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB);
	case 0x100:
		size = devicePage(p + 8) + 8;
		break;
	case 0x120:
		p[8] = 1;
		p[9] = 0x20;
		p[10] = 0;
		p[11] = 0x50;
		clearBytes(p + 12, 4);
		if (!s.manager) {
			memset(p + 16, 0, 80);
		} else {
			for (unsigned i = 0; i < 8; i++) {
				p[16 + 2 * i] = 2;
				p[17 + 2 * i] = capabilities[i];
			}
			p[32] = p[34] = p[36] = 2;
			p[33] = 0x23;
			p[35] = 0x31;
			p[37] = 0x32;
			memset(p + 38, 0, 50);
		}
		size = 88;
		break;
	case 0x140:
	case 0x160:
	case 0x121:
		p[8] = 1;
		p[9] = page;
		p[10] = 0;
		p[11] = 0x30;
		memset(p + 12, 0, 44);
		if (page == 0x160 && s.manager)
			p[16] = 1;
		if (page == 0x121) {
			p[16] = 0x20;
			if (s.manager)
				p[19] = 1;
		}
		size = 56;
		break;
	default:
		return setSense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB);
	}
	p[0] = size >> 8;
	p[1] = size;
	p[2] = 0;
	p[3] = 1;
	return sendResponse(requested < size ? requested : size);
}

static int verifyBlocks(const uint8_t *cdb, struct UsbstorSense *sense, int write)
{
	unsigned block = be32(cdb + 2), count = be16(cdb + 7), n, bytes;
	int mismatch = 0, result = USBSTOR_COMMAND_FAILED;

	if (s.fd < 0)
		return setSense(sense, SCSI_KEY_HARDWARE_ERROR, SCSI_ASC_INTERNAL_FAILURE);
	if (block >= s.blocks || count > s.blocks - block)
		return setSense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_LBA_OUT_OF_RANGE);
	if (!count)
		return USBSTOR_COMMAND_DONE;
	s.driver.busy = 1;
	if (ksceIoLseek(s.fd, (uint64_t)block * s.blockSize, SCE_SEEK_SET) < 0) {
		setSense(sense, SCSI_KEY_MEDIUM_ERROR, SCSI_ASC_NO_SEEK_COMPLETE);
		goto done;
	}
	while (count) {
		n = count < s.verifyLimit ? count : s.verifyLimit;
		bytes = n * s.blockSize;
		if (write || (cdb[1] & 2)) {
			if (SceUsbstorForDriver_F1A690DC(s.buffers[0], bytes, TRANSFER_TIMEOUT) < 0)
				goto done;
		}
		if (write) {
			if (ksceIoWrite(s.fd, s.buffers[0], bytes) < 0) {
				setSense(sense, SCSI_KEY_MEDIUM_ERROR, SCSI_ASC_WRITE_FAULT);
				goto done;
			}
			if (ksceIoLseek(s.fd, -(int64_t)(int)bytes, SCE_SEEK_CUR) < 0) {
				setSense(sense, SCSI_KEY_MEDIUM_ERROR, SCSI_ASC_NO_SEEK_COMPLETE);
				goto done;
			}
		}
		if (ksceIoRead(s.fd, s.buffers[1], bytes) < 0) {
			setSense(sense, SCSI_KEY_MEDIUM_ERROR, SCSI_ASC_UNRECOVERED_READ_ERROR);
			goto done;
		}
		if ((write || (cdb[1] & 2)) && memcmp(s.buffers[0], s.buffers[1], bytes))
			mismatch = 1;
		count -= n;
	}
	if (mismatch)
		setSense(sense, SCSI_KEY_MISCOMPARE, 0);
	else
		result = USBSTOR_COMMAND_DONE;
done:
	s.driver.busy = 0;
	return result;
}

static int readToc(const uint8_t *cdb)
{
	uint8_t *p = s.response;
	unsigned format = cdb[2] & 15, size;
	static const uint8_t fullToc[70] = {
		0, 0x44, 1, 1, 1, 0x14, 0, 0xa0, 0, 0, 0, 0, 1, 0x20, 0, 1,
		0x14, 0, 0xa1, 0, 0, 0, 0, 1, 0, 0, 1, 0x14, 0, 0xa2, 0, 0,
		0, 0, 0, 0x0b, 0x44, 1, 1, 0, 1, 0, 0x14, 0, 0, 0, 2, 0,
		1, 0x54, 0, 0xb0, 2, 0x29, 0x44, 2, 0x4f, 0x3b, 0x48, 1,
		0x54, 0, 0xc0, 0x80, 0, 0x30, 0, 0x61, 0x18, 1,
	};

	if (!format)
		format = cdb[9] >> 6;
	if (format == 0) {
		clearBytes(p, 20);
		p[1] = 18;
		p[2] = p[3] = p[6] = 1;
		p[5] = p[13] = 0x14;
		p[14] = 0xaa;
		p[18] = 2;
		p[19] = 0xe7;
		if (cdb[1] & 2) {
			p[10] = 2;
			p[18] = 11;
			p[19] = 0x44;
		}
		size = 20;
	} else if (format == 1) {
		clearBytes(p, 12);
		p[1] = 10;
		p[2] = p[3] = p[6] = 1;
		p[5] = 0x14;
		if (cdb[1] & 2)
			p[10] = 2;
		size = 12;
	} else if (format == 2) {
		copyBytes(p, fullToc, 70);
		size = 70;
	} else {
		s.driver.busy = 0;
		return USBSTOR_COMMAND_UNKNOWN;
	}
	unsigned requested = be16(cdb + 7);
	int result = sendResponse(requested < size ? requested : size);
	s.driver.busy = 0;
	return result;
}

static int command(int unit, const uint8_t *cdb, struct UsbstorSense *sense)
{
	unsigned op = cdb[0], block = be32(cdb + 2), count = be16(cdb + 7);

	if (s.changed) {
		s.readLimit = s.writeLimit = 128;
		if (s.verifyLimit < 128)
			s.pipeline &= ~(PIPELINE_READ | PIPELINE_WRITE);
		else
			s.pipeline |= PIPELINE_READ | PIPELINE_WRITE;
		setSense(sense, SCSI_KEY_UNIT_ATTENTION, SCSI_ASC_POWER_ON_RESET);
		if (op == SCSI_INQUIRY)
			return inquiry(cdb);
		s.changed = 0;
		if (op != SCSI_REQUEST_SENSE)
			return USBSTOR_COMMAND_FAILED;
	}
	switch (op) {
	case SCSI_TEST_UNIT_READY:
		return s.fd < 0 ? setSense(sense, SCSI_KEY_HARDWARE_ERROR, SCSI_ASC_INTERNAL_FAILURE) : 0;
	case SCSI_INQUIRY:
		return inquiry(cdb);
	case SCSI_MODE_SENSE_6:
	case SCSI_MODE_SENSE_10:
		if (s.type == USBSTOR_TYPE_DISK)
			return modeSense(cdb, sense, op == SCSI_MODE_SENSE_6 ? cdb[4] : count,
					 op == SCSI_MODE_SENSE_6 ? 4 : 8);
		return USBSTOR_COMMAND_UNKNOWN;
	case SCSI_START_STOP_UNIT:
		if (s.fd < 0)
			return setSense(sense, SCSI_KEY_HARDWARE_ERROR, SCSI_ASC_INTERNAL_FAILURE);
		return cdb[4] & 2 ? setSense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB) : 0;
	case SCSI_PREVENT_ALLOW_REMOVAL:
		return cdb[4] & 1 ? setSense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB) : 0;
	case SCSI_READ_FORMAT_CAPACITIES:
		if (s.fd < 0 || !s.blocks)
			return setSense(sense, SCSI_KEY_MEDIUM_ERROR, SCSI_ASC_INCOMPATIBLE_MEDIUM);
		put32(s.response, 0x10);
		put32(s.response + 4, s.blocks);
		put32(s.response + 8, s.blockSize | 0x02000000);
		put32(s.response + 12, s.blocks);
		put32(s.response + 16, s.blockSize);
		return sendResponse(count < 20 ? count : 20);
	case SCSI_READ_CAPACITY_10:
		if (s.fd < 0)
			return setSense(sense, SCSI_KEY_HARDWARE_ERROR, SCSI_ASC_INTERNAL_FAILURE);
		if (!s.blocks)
			return setSense(sense, SCSI_KEY_MEDIUM_ERROR, SCSI_ASC_INCOMPATIBLE_MEDIUM);
		put32(s.response, s.blocks - 1);
		put32(s.response + 4, s.blockSize);
		return sendResponse(8);
	case SCSI_READ_10:
	case SCSI_READ_12:
		return transferBlocks(block, op == SCSI_READ_10 ? count : be32(cdb + 6), sense, 0);
	case SCSI_WRITE_10:
	case SCSI_WRITE_12:
		if (s.type == USBSTOR_TYPE_DISK)
			return transferBlocks(block, op == SCSI_WRITE_10 ? count : be32(cdb + 6), sense, 1);
		return USBSTOR_COMMAND_UNKNOWN;
	case SCSI_SEEK_10:
		if (s.fd < 0)
			return setSense(sense, SCSI_KEY_HARDWARE_ERROR, SCSI_ASC_INTERNAL_FAILURE);
		if (block >= s.blocks)
			return setSense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_LBA_OUT_OF_RANGE);
		return ksceIoLseek(s.fd, (uint64_t)block * s.blockSize, SCE_SEEK_SET) < 0 ?
			setSense(sense, SCSI_KEY_MEDIUM_ERROR, SCSI_ASC_NO_SEEK_COMPLETE) : 0;
	case SCSI_WRITE_AND_VERIFY_10:
		if (s.type != USBSTOR_TYPE_DISK)
			return USBSTOR_COMMAND_UNKNOWN;
		return verifyBlocks(cdb, sense, 1);
	case SCSI_VERIFY_10:
		return verifyBlocks(cdb, sense, 0);
	case SCSI_READ_TOC:
		return s.type == USBSTOR_TYPE_CDROM ? readToc(cdb) : USBSTOR_COMMAND_UNKNOWN;
	case 0xd7:					/* vendor specific */
		return vendorPage(cdb, sense);
	default:
		return USBSTOR_COMMAND_UNKNOWN;
	}
}

static int startStorage(int type)
{
	int result;
	unsigned product;
	static const uint8_t isoPrefix[] = {
		0x89, 0x8c, 0xcf, 0xc5, 0x89, 0x8c, 0x97, 0xd0, 0x92, 0x8b,
		0x8f, 0x8d, 0x9a, 0x8c, 0x8f, 0x90, 0x91, 0x9b, 0x9a, 0x8d,
		0xd0, 0xbc, 0xb2, 0xbe, 0xb6, 0x91, 0x8c, 0x8b, 0x9e, 0x93,
		0x93, 0x9a, 0x8d, 0xd1, 0x96, 0x92, 0x98,
	};

	if (ksceUdcdGetDeviceStateInternal(2) & 0x2000)
		return VSTOR_ERROR_ACTIVE;
	unsigned prefix;
	if (type == USBSTOR_TYPE_DISK) {
		s.blockSize = 512;
		/* Keep the original byte comparison: it makes no strncmp import. */
		static const char diskPrefix[] = "ux0:/umass/";
		for (prefix = 0; prefix < 11; prefix++) {
			if (s.path[prefix] != diskPrefix[prefix])
				return VSTOR_ERROR_LOCATION;
		}
		product = VSTOR_PID_DISK;
	} else if (type == USBSTOR_TYPE_CDROM) {
		s.blockSize = 2048;
		for (prefix = 0; prefix < sizeof(isoPrefix); prefix++) {
			if ((uint8_t)s.path[prefix] != (uint8_t)~isoPrefix[prefix])
				return VSTOR_ERROR_LOCATION;
		}
		product = VSTOR_PID_CDROM;
	} else {
		return VSTOR_ERROR_TYPE;
	}
	for (unsigned i = prefix; i < 256 && s.path[i]; i++) {
		if (s.path[i] == '/')
			return VSTOR_ERROR_LOCATION;
	}
	s.type = type;
	result = ksceUdcdStart("USBDeviceControllerDriver", 0, NULL);
	if (result < 0)
		return result;
	result = ksceUdcdStart("USBStor_Driver", s.type, NULL);
	if (result < 0)
		return result;
	result = ksceUdcdActivate(product);
	return result < 0 ? result : 0;
}

int module_start(SceSize size, void *args)
{
	const char *p = args;

	memset(&s, 0, offsetof(__typeof__(s), driver));
	memset(s.path, 0, 256);
	s.dataBlock = -1;
	s.manager = -1;
	while (p && (int)size > 0) {
		if (*p == '-' && strncmp(p, "-nomgr", 7) == 0) {
			s.manager = 0;
			p += 8;
			size -= 8;
		} else {
			p++;
			size--;
		}
	}
	s.driver.command = command;
	s.driver.event = onEvent;
	s.driver.start = openImage;
	s.driver.stop = closeImage;
	s.driver.priority = 4;
	s.driver.unit = s.driver.requestSense = s.driver.busy = 0;
	if (SceUsbstorForDriver_C0944A04(&s.driver) < 0)
		return SCE_KERNEL_START_FAILED;
	s.eventFlag = sceKernelCreateEventFlag("SceUsbstorVStorCmd", SCE_KERNEL_EVF_ATTR_MULTI, 0,
						NULL);
	if (s.eventFlag <= 0)
		return SCE_KERNEL_START_FAILED;
	s.thread = sceKernelCreateThread("SceUsbstorVstor", transferThread, 0x1f, 0x800, 0, 0x80000, NULL);
	if (s.thread > 0) {
		if (sceKernelStartThread(s.thread, 0, NULL) >= 0)
			return SCE_KERNEL_START_SUCCESS;
		sceKernelDeleteThread(s.thread);
	}
	sceKernelDeleteEventFlag(s.eventFlag);
	return SCE_KERNEL_START_FAILED;
}

int module_stop(SceSize size, void *args)
{
	sceKernelSetEventFlag(s.eventFlag, EVF_EXIT);
	sceKernelWaitThreadEnd(s.thread, NULL, NULL);
	sceKernelDeleteThread(s.thread);
	sceKernelDeleteEventFlag(s.eventFlag);
	return (unsigned)SceUsbstorForDriver_EAC08C24(&s.driver) >> 31;
}

int sceUsbstorVStorSetDeviceInfo(const char *product, const char *revision)
{
	unsigned saved = sceSyscallEnter();
	int result = SCE_ERROR_ERRNO_ENOSYS;
	char productCopy[17] = { 0 }, revisionCopy[5] = { 0 };

	if (authorized()) {
		result = VSTOR_ERROR_ARGUMENT;
		if (SceSysmemForDriver_DB3EC244(productCopy, product, 16) >= 0 &&
		    SceSysmemForDriver_DB3EC244(revisionCopy, revision, 4) >= 0 &&
		    productCopy[0] && revisionCopy[0]) {
			SceUsbstorForDriver_ADB2AAF2(productCopy, revisionCopy, 0);
			result = 0;
		}
	}
	sceSyscallLeave(saved);
	return result;
}

int sceUsbstorVStorSetImgFilePath(const char *path)
{
	unsigned saved = sceSyscallEnter();
	int result = SCE_ERROR_ERRNO_ENOSYS;
	char copy[257];

	if (authorized()) {
		memset(copy, 0, 257);
		result = SceSysmemForDriver_DB3EC244(copy, path, 256);
		if (result < 0 || !path) {
			result = VSTOR_ERROR_ARGUMENT;
		} else {
			memset(s.path, 0, 256);
#if FW_VERSION >= 0x04000000
			__strncpy_chk(s.path, copy, result, 256);
#else
			strncpy(s.path, copy, result);
#endif
			memset(s.path, 0, 256);
			strncpy(s.path, copy, 256);
			result = 0;
		}
	}
	sceSyscallLeave(saved);
	return result;
}

int sceUsbstorVStorStart(int type)
{
	unsigned saved = sceSyscallEnter();
	int result = authorized() ? startStorage(type) : SCE_ERROR_ERRNO_ENOSYS;
	sceSyscallLeave(saved);
	return result;
}

int sceUsbstorVStorStop(void)
{
	unsigned saved = sceSyscallEnter();
	int result = SCE_ERROR_ERRNO_ENOSYS;

	if (authorized()) {
		s.type = USBSTOR_TYPE_NONE;
		if (ksceUdcdGetDrvState("USBStor_Driver") != 2) {
			result = VSTOR_ERROR_INACTIVE;
		} else {
			result = ksceUdcdDeactivate();
			if (result >= 0) {
				result = ksceUdcdStop("USBStor_Driver", 0, NULL);
				if (result >= 0) {
					result = ksceUdcdStop("USBDeviceControllerDriver", 0, NULL);
					if (result >= 0)
						result = 0;
				}
			}
		}
	}
	sceSyscallLeave(saved);
	return result;
}
