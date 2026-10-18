/* USB bulk-only mass storage for the Dolce safe-mode boot image. */
#include <stddef.h>
#include <stdint.h>
#include <sysmem/barrier.h>
#include <psp2kern/usbd.h>
#include <psp2kern/usbserv.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/debug.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/intrmgr.h>
#include <sysmem/cpu.h>
#include <kernel/error.h>
#include <umass/umass.h>

#define UMASS_ERROR(n) ((int)(0x802a0000U | (n))) /* official names unavailable */
#define logError(func, fmt, ...) SceDebugForDriver_391B74B7("[Umass]%08x:%s: " fmt, \
	SceThreadmgrForDriver_47F6DE49(), func, ##__VA_ARGS__)

/* Bulk-Only Transport (USB Mass Storage Class) */
#define UMASS_CBW_SIGNATURE	0x43425355	/* "USBC" */
#define UMASS_CSW_SIGNATURE	0x53425355	/* "USBS" */
#define UMASS_CBW_SIZE		31
#define UMASS_CSW_SIZE		13
#define UMASS_CBW_DATA_OUT	0x00
#define UMASS_CBW_DATA_IN	0x80

/* UsbMassCsw.status */
#define UMASS_CSW_PASSED	0
#define UMASS_CSW_FAILED	1
#define UMASS_CSW_PHASE_ERROR	2

/* class requests */
#define UMASS_REQUEST_RESET	0xff
#define UMASS_REQUEST_GET_MAX_LUN 0xfe

/* SCSI commands */
#define SCSI_TEST_UNIT_READY	0x00
#define SCSI_REQUEST_SENSE	0x03
#define SCSI_MODE_SENSE_6	0x1a
#define SCSI_READ_CAPACITY_10	0x25
#define SCSI_READ_10		0x28
#define SCSI_WRITE_10		0x2a

#define SCSI_SENSE_NOT_READY		0x2
#define SCSI_SENSE_UNIT_ATTENTION	0x6
#define SCSI_ASC_MEDIUM_CHANGED		0x28	/* (ASC & ~2): also 0x2a, parameters changed */
#define SCSI_MODE_SENSE_ALL_PAGES	0x3f
#define SCSI_MODE_WRITE_PROTECT		0x80

#define UMASS_SECTOR_SIZE	512
#define UMASS_TRANSFER_MAX	0x10000		/* one bulk transfer */
#define UMASS_SECTORS_MAX	128		/* one READ(10)/WRITE(10) */

/* a SCSI command block */
typedef union ScsiCdb {
	SceUInt8 bytes[16];			/* bytes[0]: the operation code */
	struct {
		SceUInt8 opcode;
		SceUInt8 reserved[3];
		SceUInt8 allocationLength;
		SceUInt8 control;
	} requestSense;
	struct {
		SceUInt8 opcode;
		SceUInt8 flags;
		SceUInt8 pageCode;
		SceUInt8 subpageCode;
		SceUInt8 allocationLength;
		SceUInt8 control;
	} modeSense6;
	struct {
		SceUInt8 opcode;
		SceUInt8 flags;
		SceUInt32 lba;			/* big-endian */
		SceUInt8 group;
		SceUInt16 length;		/* big-endian, in sectors */
		SceUInt8 control;
	} __attribute__((packed)) rw10;
} ScsiCdb;

/* Command Block Wrapper */
typedef struct UsbMassCbw {
	SceUInt32 signature;			/* UMASS_CBW_SIGNATURE */
	SceUInt32 tag;
	SceUInt32 dataTransferLength;
	SceUInt8 flags;				/* UMASS_CBW_DATA_* */
	SceUInt8 lun;
	SceUInt8 cbLength;
	ScsiCdb cb;
} UsbMassCbw;

/* Command Status Wrapper */
typedef struct UsbMassCsw {
	SceUInt32 signature;			/* UMASS_CSW_SIGNATURE */
	SceUInt32 tag;
	SceUInt32 dataResidue;
	SceUInt8 status;			/* UMASS_CSW_* */
} UsbMassCsw;

/* fixed format sense data (REQUEST SENSE) */
typedef struct ScsiSenseData {
	SceUInt8 responseCode;
	SceUInt8 obsolete;
	SceUInt8 senseKey;			/* low 4 bits */
	SceUInt8 information[4];
	SceUInt8 additionalLength;
	SceUInt8 commandInformation[4];
	SceUInt8 asc;
	SceUInt8 ascq;
	SceUInt8 fruCode;
	SceUInt8 senseKeySpecific[3];
} ScsiSenseData;

/* READ CAPACITY(10) data, big-endian */
typedef struct ScsiCapacity {
	SceUInt32 lastLba;
	SceUInt32 blockLength;
} ScsiCapacity;

/* MODE SENSE(6) parameter header */
typedef struct ScsiModeHeader {
	SceUInt8 modeDataLength;
	SceUInt8 mediumType;
	SceUInt8 deviceSpecific;		/* SCSI_MODE_WRITE_PROTECT */
	SceUInt8 blockDescriptorLength;
} ScsiModeHeader;

/* name unknown: the device; the USB transfer areas each take a cache line */
typedef struct UsbMassDevice {
	UsbMassCbw cbw __attribute__((aligned(64)));
	UsbMassCsw csw __attribute__((aligned(64)));
	UsbMassCbw senseCbw __attribute__((aligned(64)));
	UsbMassCsw senseCsw __attribute__((aligned(64)));
	ScsiSenseData sense __attribute__((aligned(64)));
	SceUInt8 maxLun __attribute__((aligned(64)));
	SceUID mutex __attribute__((aligned(64)));
	int deviceId;
	SceUsbdInterfaceDescriptor *interface;
	SceUsbdEndpointDescriptor *outEndpoint;
	SceUID outPipe;
	SceUsbdEndpointDescriptor *inEndpoint;
	SceUID inPipe;
	SceUID controlPipe;
	SceUInt32 capacityValid;
	SceUInt32 mediaChanged;
	SceUInt32 sectors;
	SceUInt32 sectorSize;
	SceUInt32 readOnly;
	SceUInt32 lun;
	SceUInt8 *buffer;			/* DMA buffer for the data phase */
	SceUInt32 tag;
	SceUInt32 transferred;
	SceUID eventFlag;
	int result;				/* of the last transfer */
	int count;
} UsbMassDevice;

_Static_assert(offsetof(UsbMassDevice, mutex) == 0x180, "UsbMassDevice");

static UsbMassDevice device __attribute__((aligned(64))) = {
	.mutex = -1, .outPipe = -1, .inPipe = -1, .controlPipe = -1, .eventFlag = -1,
};
static UsbMassDevice *attached;
static SceUID mutex, eventFlag, memblock;
static SceUInt8 *ioBuffer;
static SceInt32 pending, enabled;
static SceKernelSpinlock intrLock;

/* name unknown */
static void transferDone(int result, int count, void *arg)
{
	UsbMassDevice *dev = arg;

	dev->result = result;
	dev->count = count;
	sceKernelSetEventFlag(dev->eventFlag, 1);
}

/* name unknown */
static void dataDone(int result, int count, void *arg)
{
	UsbMassDevice *dev = arg;

	dev->result = result;
	dev->count = count;
	if (!result && count >= 0)
		dev->transferred += count;
	sceKernelSetEventFlag(dev->eventFlag, 1);
}

/* name unknown */
static int waitTransfer(UsbMassDevice *dev, unsigned int mode)
{
	unsigned int bits;
	int ret = sceKernelWaitEventFlag(dev->eventFlag, 3, mode, &bits, NULL);

	if (ret == (int)SCE_KERNEL_ERROR_WAIT_DELETE || ret == (int)SCE_KERNEL_ERROR_WAIT_CANCEL)
		return UMASS_ERROR(1);
	if (!ret)
		return bits & 2 ? UMASS_ERROR(1) : dev->result;
	return ret;
}

static int _sceUsbMassDetach(int deviceId)
{
	UsbMassDevice *dev = attached;

	if (!dev || dev->deviceId != deviceId)
		return -1;
	dev->inPipe = dev->controlPipe = dev->outPipe = -1;
	dev->mediaChanged = dev->sectors = dev->sectorSize = dev->readOnly = 0;
	dev->capacityValid = dev->lun = 0;
	attached = NULL;
	sceKernelSetEventFlag(dev->eventFlag, 2);
	SceCpuForDriver_2A40BB93(&pending, 2);
	ksceKernelTriggerSGI(15, 2, 0);
	return 0;
}

static int _sceUsbMassProbe(int deviceId)
{
	SceUsbdDeviceDescriptor *desc;
	SceUsbdInterfaceDescriptor *interface;
	void *endpoint;
	int special;

	if (attached)
		return -1;
	desc = ksceUsbdScanStaticDescriptor(deviceId, NULL, SCE_USBD_DESCRIPTOR_DEVICE);
	if (!desc)
		return -1;
	special = desc->idVendor == 0x54c && desc->idProduct == 0x2d2;
	if (desc->bNumConfigurations != 1)
		return -1;
	interface = ksceUsbdScanStaticDescriptor(deviceId, desc, SCE_USBD_DESCRIPTOR_INTERFACE);
	if (!interface || interface->bInterfaceClass != SCE_USBD_CLASS_STORAGE ||
	    interface->bInterfaceProtocol != 0x50)
		return -1;
	if (interface->bInterfaceSubclass != 6 && interface->bInterfaceSubclass != 1 &&
	    !(interface->bInterfaceSubclass == 5 && special))
		return -1;
	if (interface->bNumEndpoints != 2) {
		if (interface->bNumEndpoints != 3) {
			logError("_sceUsbMassProbe", "Too many endpoints %d\n", interface->bNumEndpoints);
			return -1;
		}
		logError("_sceUsbMassProbe", "3 endpoints\n");
	}
	endpoint = ksceUsbdScanStaticDescriptor(deviceId, interface, SCE_USBD_DESCRIPTOR_ENDPOINT);
	while (endpoint)
		endpoint = ksceUsbdScanStaticDescriptor(deviceId, endpoint, SCE_USBD_DESCRIPTOR_ENDPOINT);
	return 0;
}

/* name unknown */
static int clearHalt(UsbMassDevice *dev, SceUsbdEndpointDescriptor *endpoint, int inspect)
{
	SceUsbdDeviceRequest req = {
		.bmRequestType = SCE_USBD_REQTYPE_RECIP_ENDPOINT,
		.bRequest = SCE_USBD_REQUEST_CLEAR_FEATURE,
		.wValue = 0, .wIndex = endpoint->bEndpointAddress, .wLength = 0,
	};
	unsigned int bits;
	int ret = ksceUsbdControlTransfer(dev->controlPipe, &req, NULL, transferDone, dev);

	if (ret >= 0) {
		if (inspect)
			ret = waitTransfer(dev, 5);
		else
			sceKernelWaitEventFlag(dev->eventFlag, 3,
						SCE_KERNEL_EVF_WAITMODE_OR |
						SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
	}
	return ret;
}

/* name unknown */
static int resetDevice(UsbMassDevice *dev)
{
	SceUsbdDeviceRequest req = {
		.bmRequestType = SCE_USBD_REQTYPE_TYPE_CLASS | SCE_USBD_REQTYPE_RECIP_INTERFACE,
		.bRequest = UMASS_REQUEST_RESET, .wValue = 0,
		.wIndex = dev->interface->bInterfaceNumber, .wLength = 0,
	};
	int ret, inRet;

	ksceUsbdControlTransfer(dev->controlPipe, &req, NULL, transferDone, dev);
	ret = waitTransfer(dev, 5);
	if (ret == UMASS_ERROR(1))
		return ret;
	inRet = clearHalt(dev, dev->inEndpoint, 1);
	ret = clearHalt(dev, dev->outEndpoint, 1);
	return inRet > 0 ? inRet : ret;
}

static int _sceUsbMassDoCBW(UsbMassDevice *dev, UsbMassCbw *cbw, SceUInt8 *buffer, unsigned int length)
{
	int ret, tries = 2;
	unsigned int chunk;
	int input = cbw->flags & UMASS_CBW_DATA_IN;

	for (;;) {
		ret = ksceUsbdBulkTransfer(dev->outPipe, (unsigned char *)cbw,
					 UMASS_CBW_SIZE, transferDone, dev);
		if (ret >= 0)
			ret = waitTransfer(dev, 3);
		if (ret == UMASS_ERROR(1))
			return ret;
		if (ret == SCE_USBD_ERROR_80240007 || ret == SCE_USBD_ERROR_8024000A) {
			resetDevice(dev);
			if (!tries) {
				ret = UMASS_ERROR(10);
				break;
			}
		} else if (!tries || ret != UMASS_ERROR(10)) {
			break;
		}
		tries--;
	}
	if (ret < 0) {
		logError("_sceUsbMassDoCBW", "CBW send failed %#x\n", ret);
		return ret;
	}
	dev->transferred = 0;
	while (length) {
		chunk = length > UMASS_TRANSFER_MAX ? UMASS_TRANSFER_MAX : length;
		if (input) {
			ret = ksceUsbdBulkTransfer(dev->inPipe, dev->buffer, chunk, dataDone, dev);
		} else {
			if (dev->buffer != buffer)
				memcpy(dev->buffer, buffer, chunk);
			dmb();
			dsb();
			ret = ksceUsbdBulkTransfer(dev->outPipe, dev->buffer, chunk, dataDone, dev);
		}
		if (ret >= 0) {
			ret = waitTransfer(dev, 3);
			if (ret == SCE_USBD_ERROR_80240007 || ret == SCE_USBD_ERROR_8024000A)
				clearHalt(dev, input ? dev->inEndpoint : dev->outEndpoint, 0);
		}
		if (ret == UMASS_ERROR(1))
			return ret;
		if (ret < 0) {
			logError("_sceUsbMassDoCBW", "Data transfer failed ret=%#x usbd=%#x\n", ret, dev->result);
			break;
		}
		if (input && dev->buffer != buffer)
			memcpy(buffer, dev->buffer, chunk);
		buffer += chunk;
		length -= chunk;
	}
	return ret;
}

static int _sceUsbMassDoCSW(UsbMassDevice *dev, UsbMassCbw *cbw, UsbMassCsw *csw, int requestSense);

/* a little-endian USB field, stored a byte at a time */
static inline void putLe32(void *field, SceUInt32 value)
{
	SceUInt8 *p = field;

	p[0] = value;
	p[1] = value >> 8;
	p[2] = value >> 16;
	p[3] = value >> 24;
}

/* name unknown */
static void newCommand(UsbMassDevice *dev, UsbMassCbw *cbw, unsigned int length,
		       unsigned int direction, unsigned int cbLength, unsigned int opcode)
{
	memset(cbw, 0, UMASS_CBW_SIZE);
	putLe32(&cbw->signature, UMASS_CBW_SIGNATURE);
	cbw->tag = ++dev->tag;
	putLe32(&cbw->dataTransferLength, length);
	cbw->flags = direction;
	cbw->cbLength = cbLength;
	cbw->cb.bytes[0] = opcode;
}

static void _sceUsbMassDoRequestSense(UsbMassDevice *dev)
{
	int result = dev->result, count = dev->count;
	int ret;
	unsigned int senseKey;

	memset(&dev->sense, 0, sizeof(dev->sense));
	newCommand(dev, &dev->senseCbw, sizeof(dev->sense), UMASS_CBW_DATA_IN, 12, SCSI_REQUEST_SENSE);
	dev->senseCbw.cb.requestSense.allocationLength = sizeof(dev->sense);
	ret = _sceUsbMassDoCBW(dev, &dev->senseCbw, (SceUInt8 *)&dev->sense, sizeof(dev->sense));
	if (ret != UMASS_ERROR(1)) {
		if (ret < 0)
			logError("_sceUsbMassDoRequestSense", "RequestSense CBW/Data failed %#x\n", ret);
		ret = _sceUsbMassDoCSW(dev, &dev->senseCbw, &dev->senseCsw, 0);
		senseKey = dev->sense.senseKey & 15;
		if (!ret && (senseKey == SCSI_SENSE_NOT_READY || senseKey == SCSI_SENSE_UNIT_ATTENTION)) {
			if ((dev->sense.asc & ~2) != SCSI_ASC_MEDIUM_CHANGED || dev->sense.ascq)
				logError("_sceUsbMassDoRequestSense", "Unit Attention. Drive may not work %#x key=%#x asc=%#x ascq=%#x\n",
					 0, dev->sense.senseKey, dev->sense.asc, dev->sense.ascq);
			dev->mediaChanged = 1;
		}
	}
	dev->result = result;
	dev->count = count;
}

static int _sceUsbMassDoCSW(UsbMassDevice *dev, UsbMassCbw *cbw, UsbMassCsw *csw, int requestSense)
{
	int ret, tries = 1;

	/* cleared byte by byte: the device writes it by DMA */
	for (int i = 0; i < UMASS_CSW_SIZE; i++)
		((volatile SceUInt8 *)csw)[i] = 0;
	for (;;) {
		ret = ksceUsbdBulkTransfer(dev->inPipe, (unsigned char *)csw,
					 UMASS_CSW_SIZE, transferDone, dev);
		if (ret >= 0)
			ret = waitTransfer(dev, 3);
		if (ret == UMASS_ERROR(1))
			return ret;
		if (ret == SCE_USBD_ERROR_80240007 || ret == SCE_USBD_ERROR_8024000A) {
			if (tries == 1) {
				clearHalt(dev, dev->inEndpoint, 0);
				tries = 0;
				continue;
			}
			resetDevice(dev);
		} else if (ret < 0) {
			return ret;
		} else if (dev->count != UMASS_CSW_SIZE) {
			logError("_sceUsbMassDoCSW", "CSW length wrong %d\n", dev->count);
			resetDevice(dev);
			ret = UMASS_ERROR(2);
		} else if (cbw->tag != csw->tag) {
			logError("_sceUsbMassDoCSW", "CSW tag doesnot match %#x %#x\n", cbw->tag, csw->tag);
			resetDevice(dev);
			ret = UMASS_ERROR(3);
		} else if (csw->signature != UMASS_CSW_SIGNATURE) {
			logError("_sceUsbMassDoCSW", "CSW signature wrong %#x\n", csw->signature);
			resetDevice(dev);
			ret = UMASS_ERROR(4);
		} else {
			break;
		}
		if (tries) {
			tries--;
			continue;
		}
		if (ret < 0) {
			logError("_sceUsbMassDoCSW", "CSW receive failed %#x\n", ret);
			return ret;
		}
		break;
	}
	/* Sony checks the primary CSW even while processing the Request Sense CSW. */
	if (dev->csw.status > UMASS_CSW_FAILED ||
	    dev->cbw.dataTransferLength < dev->csw.dataResidue) {
		logError("_sceUsbMassDoCSW", "CSW is not meainingful %#x\n", csw->status);
		resetDevice(dev);
		return UMASS_ERROR(5);
	}
	if (csw->status == UMASS_CSW_PHASE_ERROR)
		resetDevice(dev);
	if (csw->status == UMASS_CSW_FAILED) {
		if (requestSense)
			_sceUsbMassDoRequestSense(dev);
		return UMASS_ERROR(6);
	}
	if (csw->status == UMASS_CSW_PASSED) {
		if (!csw->dataResidue || csw->dataResidue == cbw->dataTransferLength - dev->transferred)
			return 0;
		logError("_sceUsbMassDoCSW", "residue mismatch %#x %#x %#x\n",
			 csw->dataResidue, cbw->dataTransferLength, dev->transferred);
		return UMASS_ERROR(9);
	}
	return csw->status == UMASS_CSW_PHASE_ERROR ? UMASS_ERROR(7) : UMASS_ERROR(8);
}

static int _sceUsbMassDoReadCapacity(UsbMassDevice *dev)
{
	ScsiCapacity *capacity = (ScsiCapacity *)dev->buffer;
	int ret;

	newCommand(dev, &dev->cbw, sizeof(*capacity), UMASS_CBW_DATA_IN, 10, SCSI_READ_CAPACITY_10);
	ret = _sceUsbMassDoCBW(dev, &dev->cbw, dev->buffer, sizeof(*capacity));
	if (ret == UMASS_ERROR(1))
		return ret;
	ret = _sceUsbMassDoCSW(dev, &dev->cbw, &dev->csw, 1);
	if (ret < 0)
		return ret;
	if (dev->csw.dataResidue) {
		logError("_sceUsbMassDoReadCapacity", "ReadCapacity failed\n");
		return UMASS_ERROR(11);
	}
	capacity->lastLba = __builtin_bswap32(capacity->lastLba);
	capacity->blockLength = __builtin_bswap32(capacity->blockLength);
	if (capacity->blockLength != UMASS_SECTOR_SIZE) {
		logError("_sceUsbMassDoReadCapacity", "Sector size is not %d (%d)\n",
			 UMASS_SECTOR_SIZE, capacity->blockLength);
		return UMASS_ERROR(11);
	}
	dev->sectors = capacity->lastLba + 1;
	dev->sectorSize = UMASS_SECTOR_SIZE;
	if (dev->sectors & 1)
		logError("_sceUsbMassDoReadCapacity", "Capacity is not even %#x\n", dev->sectors);
	dev->capacityValid = 1;
	return ret;
}

/* name unknown: the attach-time commands build the CBW without clearing it first */
static void testUnitReadyCommand(UsbMassDevice *dev)
{
	UsbMassCbw *cbw = &dev->cbw;

	putLe32(&cbw->signature, UMASS_CBW_SIGNATURE);
	cbw->tag = ++dev->tag;
	putLe32(&cbw->dataTransferLength, 0);
	cbw->flags = UMASS_CBW_DATA_IN;
	cbw->lun = 0;
	cbw->cbLength = 6;
	memset(&cbw->cb, 0, sizeof(cbw->cb));
	cbw->cb.bytes[0] = SCSI_TEST_UNIT_READY;
}

/* name unknown: READ(10) of sector 0 */
static void firstReadCommand(UsbMassDevice *dev)
{
	UsbMassCbw *cbw = &dev->cbw;

	putLe32(&cbw->signature, UMASS_CBW_SIGNATURE);
	cbw->tag = ++dev->tag;
	putLe32(&cbw->dataTransferLength, UMASS_SECTOR_SIZE);
	cbw->flags = UMASS_CBW_DATA_IN;
	cbw->lun = 0;
	cbw->cbLength = 10;
	memset(&cbw->cb, 0, sizeof(cbw->cb));
	cbw->cb.rw10.opcode = SCSI_READ_10;
	cbw->cb.rw10.length = __builtin_bswap16(1);
}

static int _sceUsbMassAttach(int deviceId)
{
	SceUsbdEndpointDescriptor *endpoint;
	SceUsbdConfigurationDescriptor *config;
	SceUsbdDeviceDescriptor *desc;
	SceUsbdDeviceRequest req;
	UsbMassDevice *dev = &device;
	int inPipe = -1, outPipe = -1, ret;

	if (attached) {
		logError("_sceUsbMassAttach", "already attached \n");
		return -1;
	}
	dev->deviceId = deviceId;
	dev->interface = ksceUsbdScanStaticDescriptor(deviceId, NULL, SCE_USBD_DESCRIPTOR_INTERFACE);
	endpoint = ksceUsbdScanStaticDescriptor(deviceId, dev->interface, SCE_USBD_DESCRIPTOR_ENDPOINT);
	while (endpoint) {
		if (inPipe > 0 && outPipe > 0)
			break;
		if (endpoint->bEndpointAddress & 0x80) {
			dev->inEndpoint = endpoint;
			inPipe = ksceUsbdOpenPipe(deviceId, endpoint);
			if (inPipe < 0)
				goto fail;
			dev->inPipe = inPipe;
		} else {
			dev->outEndpoint = endpoint;
			outPipe = ksceUsbdOpenPipe(deviceId, endpoint);
			if (outPipe < 0)
				goto fail;
			dev->outPipe = outPipe;
		}
		endpoint = ksceUsbdScanStaticDescriptor(deviceId, endpoint, SCE_USBD_DESCRIPTOR_ENDPOINT);
	}
	dev->controlPipe = ksceUsbdOpenPipe(deviceId, NULL);
	if (dev->controlPipe < 0)
		goto fail;
	dev->eventFlag = eventFlag;
	dev->buffer = ioBuffer;
	sceKernelClearEventFlag(eventFlag, 0);
	dev->mutex = mutex;
	config = ksceUsbdScanStaticDescriptor(deviceId, NULL, SCE_USBD_DESCRIPTOR_CONFIGURATION);
	req.bmRequestType = 0;
	req.bRequest = SCE_USBD_REQUEST_SET_CONFIGURATION;
	req.wValue = config->bConfigurationValue;
	req.wIndex = 0;
	req.wLength = 0;
	ret = ksceUsbdControlTransfer(dev->controlPipe, &req, NULL, transferDone, dev);
	if (ret >= 0)
		ret = waitTransfer(dev, 5);
	if (ret < 0)
		goto fail;
	req.bmRequestType = SCE_USBD_REQTYPE_DIR_TO_HOST | SCE_USBD_REQTYPE_TYPE_CLASS |
			    SCE_USBD_REQTYPE_RECIP_INTERFACE;
	req.bRequest = UMASS_REQUEST_GET_MAX_LUN;
	req.wValue = 0;
	req.wIndex = dev->interface->bInterfaceNumber;
	req.wLength = 1;
	ksceUsbdControlTransfer(dev->controlPipe, &req, &dev->maxLun, transferDone, dev);
	ret = waitTransfer(dev, 5);
	if (ret >= 0) {
		dev->lun = dev->maxLun;
		if (dev->lun) {
			logError("_sceUsbMassAttach", "Multiple LUN %d\n", dev->lun);
			goto fail;
		}
	} else {
		dev->lun = 0;
	}
	for (int tries = 50; tries; tries--) {
		testUnitReadyCommand(dev);
		ret = _sceUsbMassDoCBW(dev, &dev->cbw, NULL, 0);
		if (ret == UMASS_ERROR(1))
			break;
		ret = _sceUsbMassDoCSW(dev, &dev->cbw, &dev->csw, 1);
		if (!ret) {
			dev->mediaChanged = 0;
			_sceUsbMassDoReadCapacity(dev);
			dev->mediaChanged = 0;
			if (dev->capacityValid || (!_sceUsbMassDoReadCapacity(dev) && !dev->mediaChanged)) {
				firstReadCommand(dev);
				ret = _sceUsbMassDoCBW(dev, &dev->cbw, dev->buffer, UMASS_SECTOR_SIZE);
				if (ret < 0) {
					logError("_sceUsbMassReadSector", "Read10 CBW/Data failed %#x\n", ret);
					if (ret == UMASS_ERROR(1))
						goto connected;
				}
				_sceUsbMassDoCSW(dev, &dev->cbw, &dev->csw, 1);
			}
connected:
			attached = dev;
			desc = ksceUsbdScanStaticDescriptor(deviceId, NULL, SCE_USBD_DESCRIPTOR_DEVICE);
			SceDebugForDriver_391B74B7("[Umass]%08x:USB mass storage (%04x:%04x) attached\n",
				SceThreadmgrForDriver_47F6DE49(), desc->idVendor, desc->idProduct);
			SceCpuForDriver_2A40BB93(&pending, 1);
			ksceKernelTriggerSGI(15, 2, 0);
			return 0;
		}
		if (ret != UMASS_ERROR(6))
			break;
		sceKernelDelayThread(100000);
	}
	logError("_sceUsbMassAttach", "Device did not become ready\n");
fail:
	if (dev->controlPipe >= 0) {
		ksceUsbdClosePipe(dev->controlPipe);
		dev->controlPipe = -1;
	}
	if (dev->outPipe >= 0) {
		ksceUsbdClosePipe(dev->outPipe);
		dev->outPipe = -1;
	}
	if (dev->inPipe >= 0) {
		ksceUsbdClosePipe(dev->inPipe);
		dev->inPipe = -1;
	}
	return -1;
}

int _sceUsbMassGetDevInfo(SceUsbMassDevInfo *info)
{
	UsbMassDevice *dev = attached;
	int ret;

	if (!dev || sceKernelLockMutex(dev->mutex, 1, NULL))
		return (int)SCE_ERROR_ERRNO_ENODEV;
	ret = 0;
	if (!dev->capacityValid)
		ret = _sceUsbMassDoReadCapacity(dev);
	if (ret >= 0) {
		if (dev->mediaChanged) {
			ret = UMASS_ERROR(13);
			goto unlock;
		}
		newCommand(dev, &dev->cbw, 0xc0, UMASS_CBW_DATA_IN, 6, SCSI_MODE_SENSE_6);
		dev->cbw.cb.modeSense6.pageCode = SCSI_MODE_SENSE_ALL_PAGES;
		dev->cbw.cb.modeSense6.allocationLength = 0xc0;
		ret = _sceUsbMassDoCBW(dev, &dev->cbw, dev->buffer, 0xc0);
		if (ret == UMASS_ERROR(1))
			goto cancelled;
		if (ret < 0)
			logError("_sceUsbMassGetDevInfo", "ReadCapacity CBW/Data failed %#x\n", ret);
		ret = _sceUsbMassDoCSW(dev, &dev->cbw, &dev->csw, 1);
		if (ret >= 0) {
			dev->readOnly = ((ScsiModeHeader *)dev->buffer)->deviceSpecific >> 7;
			info->sectorCount = dev->sectors;
			info->blockSize = dev->sectorSize;
			info->writeProtected = dev->readOnly;
			ret = 0;
		}
	}
	if (ret == UMASS_ERROR(1)) {
cancelled:
		sceKernelCancelMutex(dev->mutex, -1, NULL);
		return ret;
	}
unlock:
	sceKernelUnlockMutex(dev->mutex, 1);
	return ret;
}

/* name unknown: common body of the two sector entry points */
static int transferSectors(unsigned int sector, SceUInt8 *buffer, unsigned int count, int input)
{
	UsbMassDevice *dev = attached;
	unsigned int chunk;
	int ret = 0, status;

	if (!dev || sceKernelLockMutex(dev->mutex, 1, NULL))
		return (int)SCE_ERROR_ERRNO_ENODEV;
	while (count) {
		chunk = count > UMASS_SECTORS_MAX ? UMASS_SECTORS_MAX : count;
		if (!dev->capacityValid) {
			ret = _sceUsbMassDoReadCapacity(dev);
			if (ret != 0)
				goto next;
		}
		if (dev->mediaChanged) {
			ret = UMASS_ERROR(13);
			break;
		}
		if (!input && dev->readOnly) {
			ret = UMASS_ERROR(12);
			break;
		}
		newCommand(dev, &dev->cbw, chunk * UMASS_SECTOR_SIZE,
			   input ? UMASS_CBW_DATA_IN : UMASS_CBW_DATA_OUT, 10,
			   input ? SCSI_READ_10 : SCSI_WRITE_10);
		dev->cbw.cb.rw10.lba = __builtin_bswap32(sector);
		dev->cbw.cb.rw10.length = (chunk << 8) | (chunk >> 8);
		ret = _sceUsbMassDoCBW(dev, &dev->cbw, buffer, chunk * UMASS_SECTOR_SIZE);
		if (ret < 0) {
			if (input)
				logError("_sceUsbMassReadSector", "Read10 CBW/Data failed %#x\n", ret);
			else
				logError("_sceUsbMassWriteSector", "Write10 CBW/Data failed %#x\n", ret);
			if (ret == UMASS_ERROR(1))
				break;
		}
		status = _sceUsbMassDoCSW(dev, &dev->cbw, &dev->csw, 1);
		if (!status && dev->csw.dataResidue) {
			ret = UMASS_ERROR(9);
			break;
		}
next:
		if (ret < 0)
			break;
		sector += chunk;
		count = (count - chunk) & 0xffff;
		buffer += chunk * UMASS_SECTOR_SIZE;
	}
	sceKernelUnlockMutex(dev->mutex, 1);
	return ret;
}

int _sceUsbMassWriteSector(SceUInt32 sector, void *buffer, SceUInt32 count)
{
	return transferSectors(sector, buffer, count, 0);
}

int _sceUsbMassReadSector(SceUInt32 sector, void *buffer, SceUInt32 count)
{
	return transferSectors(sector, buffer, count, 1);
}

/* export names unknown: subinterrupt masks */
int SceUsbMassForDriver_3C821E99(int intr, unsigned int subintr)
{
	unsigned int bit = subintr & 255;

	if (intr != 15)
		return (int)SCE_ERROR_ERRNO_EINVAL;
	SceCpuForDriver_2A40BB93(&enabled, bit < 32 ? 1U << bit : 0);
	return 0;
}

int SceUsbMassForDriver_7833D935(int intr, unsigned int subintr)
{
	unsigned int bit = subintr & 255;

	if (intr != 15)
		return (int)SCE_ERROR_ERRNO_EINVAL;
	SceCpuForDriver_E36F3A46(&enabled, bit < 32 ? 1U << bit : 0);
	return 0;
}

int SceUsbMassForDriver_F2BAB182(int intr)
{
	unsigned int state;

	if (intr != 15)
		return (int)SCE_ERROR_ERRNO_EINVAL;
	state = SceCpuForDriver_D32ACE9E(&intrLock);
	if ((enabled & 1) && (SceCpuForDriver_E36F3A46(&pending, 1) & 1))
		ksceKernelTriggerSubIntr(15, 0, NULL);
	if ((enabled & 2) && (SceCpuForDriver_E36F3A46(&pending, 2) & 2))
		ksceKernelTriggerSubIntr(15, 1, NULL);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&intrLock, state);
	return -1;
}

static const SceUsbdDriver driver = {
	.name = "umass", .probe = _sceUsbMassProbe,
	.attach = _sceUsbMassAttach, .detach = _sceUsbMassDetach,
};

int module_start(SceSize args, void *argp)
{
	uint32_t options[22];
	int ret;

	(void)args;
	(void)argp;
	if (!SceSblAIMgrForDriver_71608CA3() || !SceSysrootForDriver_834439A7())
		return 1;
	ret = sceKernelCreateMutex("SceUsbMassLock", 0, 0, NULL);
	if (ret < 0)
		return ret;
	mutex = ret;
	ret = sceKernelCreateEventFlag("SceUsbMassUsbdSync", 0x2000, 0, NULL);
	if (ret < 0)
		goto deleteMutex;
	eventFlag = ret;
	memset(options, 0, sizeof(options));
	options[0] = 0x58;
	options[2] = 0x200004;
	options[5] = 0x1000;
	memblock = SceSysmemForDriver_C94850C9("SceUsbMassIOBuf", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_RW, 0x10000,
					 (SceKernelAllocMemBlockKernelOpt *)options);
	if (memblock >= 0) {
		ret = SceSysmemForDriver_A841EDDA(memblock, (void **)&ioBuffer);
		if (ret >= 0 && ksceUsbServAccessoryActivate() >= 0) {
			ret = ksceUsbdRegisterDriver(&driver);
			if (ret >= 0) {
				intrLock = 0;
				SceCpuForDriver_2A40BB93(&enabled, 1);
				SceCpuForDriver_2A40BB93(&enabled, 2);
				return 0;
			}
			ksceUsbServAccessoryDeactivate();
		}
		SceSysmemForDriver_009E1C61(memblock);
	}
	sceKernelDeleteEventFlag(eventFlag);
deleteMutex:
	return sceKernelDeleteMutex(mutex);
}
