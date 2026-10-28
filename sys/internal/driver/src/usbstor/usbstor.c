/*
 * SceUsbstorDriver: USB bulk-only storage transport. Private names unknown.
 * Both device paths and all supported firmwares share the same executable code.
 *
 * The transport takes a command block wrapper (CBW) from the host, hands its
 * SCSI command to the registered storage functions (usbstor/usbstor.h), moves the
 * data and answers with a command status wrapper (CSW). REQUEST SENSE is
 * answered here from the sense the functions leave for each unit.
 *
 * Request slots and the module state retain the original layout. The driver
 * rounds its transfer workspace to 64 bytes after the two descriptor sets.
 * IdStorage leaf 0x112 supplies the USB serial string, with a zero-digit fallback.
 *
 * Original quirks retained: transfer counters include the caller's requested
 * bytes before direction/length validation; device-string reads reject records
 * beyond the 512-byte information area; successful buffered flushes replace a
 * command error with success; the worker persists across driver stop/start.
 */
#include <stddef.h>
#include <stdint.h>
#include <udcd/udcd.h>
#include <acmgr/acmgr.h>
#include <sysmem/cpu.h>
#include <modulemgr/modulemgr.h>
#include <idstorage/idstorage.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysmem.h>
#include <intrmgr/syscall.h>
#include <kernel/error.h>
#include <usbstor/usbstor.h>

/* The SDK has no names for these storage-specific error values. */
#define STOR_ERROR_GENERAL ((int)0x80244101)
#define STOR_ERROR_DUPLICATE ((int)0x80244102)
#define STOR_ERROR_ARGUMENT ((int)0x80244103)
#define STOR_ERROR_FULL ((int)0x80244104)
#define STOR_ERROR_NOT_FOUND ((int)0x80244105)
#define STOR_ERROR_BUSY ((int)0x80244106)
#define STOR_ERROR_DIRECTION ((int)0x80244107)
#define STOR_ERROR_TIMEOUT ((int)0x80244108)
#define STOR_ERROR_CANCEL ((int)0x80244109)
#define STOR_ERROR_MEMORY ((int)0x8024410a)
#define STOR_ERROR_CONTROL ((int)0x8024410b)
#define STOR_ERROR_STATE ((int)0x8024410c)

/* USB Mass Storage Class, Bulk-Only Transport 1.0 */
#define USBMS_SUBCLASS_SFF8070I		0x05
#define USBMS_PROTOCOL_BULK_ONLY	0x50
#define USBMS_REQ_GET_MAX_LUN		0xfe
#define USBMS_REQ_RESET			0xff

#define USBMS_CBW_SIGNATURE		0x43425355	/* "USBC" */
#define USBMS_CBW_SIZE			31
#define USBMS_CBW_DATA_IN		0x80		/* bmCBWFlags */
#define USBMS_CBW_LUN_MASK		0x0f

#define USBMS_CSW_SIGNATURE		0x53425355	/* "USBS" */
#define USBMS_CSW_SIZE			13
#define USBMS_CSW_PASSED		0
#define USBMS_CSW_FAILED		1
#define USBMS_CSW_PHASE_ERROR		2

struct UsbmsCbw {
	uint32_t dCBWSignature;
	uint32_t dCBWTag;
	uint32_t dCBWDataTransferLength;
	uint8_t bmCBWFlags;
	uint8_t bCBWLUN;
	uint8_t bCBWCBLength;
	uint8_t CBWCB[16];
};

struct UsbmsCsw {
	uint32_t dCSWSignature;
	uint32_t dCSWTag;
	uint32_t dCSWDataResidue;
	uint8_t bCSWStatus;
};

/* the fixed-format sense data REQUEST SENSE returns */
struct ScsiSenseData {
	uint8_t responseCode;			/* SCSI_SENSE_CURRENT, _DEFERRED */
	uint8_t obsolete;
	uint8_t key;
	uint8_t information[4];
	uint8_t additionalLength;
	uint8_t commandSpecific[4];
	uint8_t asc, ascq;
	uint8_t fru;
	uint8_t specific[3];
};

/* s.eventFlag bits */
#define EVF_ATTACH		0x0001
#define EVF_DETACH		0x0002
#define EVF_SETTING		0x0004	/* the alternate setting changed */
#define EVF_RESET		0x0008	/* Bulk-Only Mass Storage Reset */
#define EVF_CSW_SENT		0x0010
#define EVF_CBW_RECEIVED	0x0020
#define EVF_SENT		0x0100
#define EVF_RECEIVED		0x0200
#define EVF_IN_CANCELLED	0x0400
#define EVF_OUT_CANCELLED	0x0800
#define EVF_STOP		0x1000
#define EVF_STOPPED		0x2000
#define EVF_START		0x4000
#define EVF_TRANSPORT		(EVF_STOP | EVF_OUT_CANCELLED | EVF_CBW_RECEIVED | \
				 EVF_CSW_SENT | EVF_RESET | EVF_SETTING | EVF_DETACH | EVF_ATTACH)

/* s.flags bits, also changed from the UDCD callbacks */
#define FLAG_ATTACHED		0x01
#define FLAG_RESET		0x02
#define FLAG_SETTING		0x04

/* s.status bits */
#define STATUS_TIMEOUT		0x0001	/* a data transfer timed out */
#define STATUS_CANCELLED	0x0002	/* a data transfer was cancelled */
#define STATUS_IN_STALLED	0x0004
#define STATUS_OUT_STALLED	0x0008
#define STATUS_STOPPED		0x0100	/* no descriptors: before the worker sets up, after module_stop */
#define STATUS_READY		0x1000	/* every unit has its strings */
#define STATUS_STARTED		0x2000	/* the functions are started */
#define STATUS_BAD_CBW		0x4000	/* waiting for a reset */

/* s.direction */
#define DIRECTION_OUT		1	/* host to device */
#define DIRECTION_IN		2

/* SceUdcdDeviceRequest.returnCode between transfers, and while one is queued */
#define REQUEST_IDLE		1
#define REQUEST_QUEUED		2

#define UNITS			8
#define SENSE_UNIT_OTHER	7	/* the sense of every LUN above 6 */
#define PACKET_SIZE		512	/* the buffered transfer size */
#define DEVICE_INFO_SIZE	512
#define IDSTORAGE_LEAF_USB	0x112	/* the serial number */
#define STORAGE_THREAD_CPU	0x80000

#define USB_STRING_LENGTH(n)	(2 + 2 * (n))	/* a string descriptor of n characters */

struct RequestSlot {
	SceUdcdDeviceRequest request;
	uint8_t reserved[20];
};

/* a unit's SCSI INQUIRY identification */
struct UnitIdentity {
	char vendor[8];
	char product[16];
	char revision[4];
	uint8_t vendorSpecific[128];
};

struct DeviceInfo {
	uint32_t reserved;
	struct UnitIdentity unit[3];
};

/* the transfer workspace after the descriptor sets */
struct Workspace {
	union {
		struct UsbmsCbw cbw;
		uint8_t cbwArea[64];
	};
	union {
		struct UsbmsCsw csw;
		uint8_t cswArea[64];
	};
	uint8_t control[64];
	uint8_t send[PACKET_SIZE];
	uint8_t scratch[PACKET_SIZE];
	union {
		struct DeviceInfo info;
		uint8_t infoArea[DEVICE_INFO_SIZE];
	};
};

struct DescriptorSet {
	SceUdcdDeviceDescriptor device;
	SceUdcdConfiguration configuration;
	SceUdcdConfigDescriptor config;
	SceUdcdInterfaceSettings settings;
	SceUdcdInterfaceDescriptor interfaces[2];
	SceUdcdEndpointDescriptor endpoints[3];
};

static struct {
	SceUdcdStringDescriptor serial, product;
	unsigned deviceClass, flags, status, speed, type;
	struct UsbstorFunction *functions[8];
	int count, bindings[UNITS];
	uint8_t units[UNITS];			/* the functions serving each unit */
	int maxUnit;
	unsigned packetSize;
	int memblock, thread, eventFlag;
	unsigned direction, transferred, residue;
	uint8_t *controlBuffer, *sendBuffer, *scratch;
	struct DeviceInfo *deviceInfo;
	struct RequestSlot control, send, receive, cbwRequest, cswRequest;
	struct UsbmsCbw *cbw;
	struct UsbmsCsw *csw;
	struct UsbstorSense sense[UNITS];
	struct UnitIdentity strings[UNITS];
	uint8_t identity[128];
	SceUdcdDriver driver;
	SceUdcdEndpoint endpoints[3];
	SceUdcdInterface interface;
} s;

_Static_assert(offsetof(__typeof__(s), driver) == 0x7f8, "driver layout");
_Static_assert(offsetof(__typeof__(s), cbw) == 0x250, "CBW layout");
_Static_assert(sizeof(s) == 0x888, "module state layout");
_Static_assert(sizeof(struct RequestSlot) == 0x40, "request slot");
_Static_assert(sizeof(struct UnitIdentity) == 0x9c, "unit identity");
_Static_assert(sizeof(struct ScsiSenseData) == 18, "sense data");
_Static_assert(offsetof(struct Workspace, infoArea) == 0x4c0, "workspace");
_Static_assert(2 * sizeof(struct DescriptorSet) == 336, "descriptor sets");
_Static_assert(sizeof(SceKernelAllocMemBlockKernelOpt) == 0x58, "memblock option");

static int storageThread(SceSize size, void *args);
static int startStorage(int type, void *args, void *ctx);
static int stopStorage(int size, void *args, void *ctx);
static int attachStorage(int version, void *ctx);
static void detachStorage(void *ctx);
static int changeSetting(int number, int alternate, int bus);
static int processRequest(int recipient, int number, SceUdcdEP0DeviceRequest *req, void *ctx);
static void completeControl(SceUdcdDeviceRequest *req);
static void completeSend(SceUdcdDeviceRequest *req);
static void completeReceive(SceUdcdDeviceRequest *req);
static void completeCbw(SceUdcdDeviceRequest *req);
static void completeCsw(SceUdcdDeviceRequest *req);

static inline __attribute__((always_inline)) void copyBytes(uint8_t *p, const uint8_t *q, unsigned n)
{
	while (n--)
		*p++ = *q++;
}

static inline __attribute__((always_inline)) void clearBytes(uint8_t *p, unsigned n)
{
	while (n--)
		*p++ = 0;
}

static inline __attribute__((always_inline)) void notifyFunctions(int event)
{
	for (int i = 0; i < s.count; i++) {
		if (s.functions[i]->event)
			s.functions[i]->event(event, s.speed);
	}
}

static inline __attribute__((always_inline)) void initializeRequest(SceUdcdDeviceRequest *req, SceUdcdEndpoint *ep,
			      void *buffer, int size, int control,
			      void (*complete)(SceUdcdDeviceRequest *))
{
	req->endpoint = ep;
	req->data = buffer;
	req->attributes = 0;
	req->size = size;
	req->isControlRequest = control;
	req->onComplete = complete;
	req->transmitted = 0;
	req->returnCode = REQUEST_IDLE;
	req->next = NULL;
	req->unused = NULL;
}

static inline __attribute__((always_inline)) void initializeRequests(void)
{
	initializeRequest(&s.control.request, &s.endpoints[0], s.controlBuffer, 1, 1, completeControl);
	initializeRequest(&s.send.request, &s.endpoints[1], s.sendBuffer, 0, 0, completeSend);
	initializeRequest(&s.receive.request, &s.endpoints[2], NULL, 0, 0, completeReceive);
	initializeRequest(&s.cbwRequest.request, &s.endpoints[2], s.cbw, 64, 0, completeCbw);
	initializeRequest(&s.cswRequest.request, &s.endpoints[1], s.csw, USBMS_CSW_SIZE, 0, completeCsw);
}

static int stopStorage(int size, void *args, void *ctx)
{
	if (s.flags & FLAG_ATTACHED)
		return STOR_ERROR_BUSY;
	sceKernelSetEventFlag(s.eventFlag, EVF_STOP);
	sceKernelWaitEventFlag(s.eventFlag, EVF_STOPPED, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL,
				NULL);
	for (int i = s.count - 1; i >= 0; i--) {
		if (s.functions[i]->stop)
			s.functions[i]->stop();
	}
	s.status &= ~(STATUS_READY | STATUS_STARTED);
	return 0;
}

static int startStorage(int type, void *args, void *ctx)
{
	int result = STOR_ERROR_GENERAL;
	SceUdcdDeviceInfo info;

	s.flags = 0;
	s.speed = 1;
	if (type != USBSTOR_TYPE_DISK && type != USBSTOR_TYPE_CDROM)
		return STOR_ERROR_ARGUMENT;
	s.type = type;
	if (s.status & STATUS_STOPPED)
		return STOR_ERROR_STATE;
	initializeRequests();
	if (s.maxUnit < 0) {
		s.status = STATUS_READY;
	} else {
		for (int i = 0; i <= s.maxUnit; i++) {
			if (!s.strings[i].product[0] || !s.strings[i].revision[0])
				return STOR_ERROR_GENERAL;
			/* Sony advances the source LUN but always overwrites record zero. */
			struct UnitIdentity *id = &s.deviceInfo->unit[0];
			strncpy(id->vendor, "SONY", 8);
			strncpy(id->product, s.strings[i].product, 16);
			strncpy(id->revision, s.strings[i].revision, 4);
		}
		s.status |= STATUS_READY;
	}
	if ((s.status & (STATUS_READY | STATUS_STARTED)) == STATUS_READY) {
		s.deviceClass = USBMS_SUBCLASS_SFF8070I;
		s.driver.configuration_hi->interfaceDescriptors->bInterfaceSubClass = USBMS_SUBCLASS_SFF8070I;
		s.driver.configuration->interfaceDescriptors->bInterfaceSubClass = USBMS_SUBCLASS_SFF8070I;
	}
	for (int i = 0; i < s.count; i++) {
		struct UsbstorFunction *fn = s.functions[i];
		if (s.type == USBSTOR_TYPE_CDROM && fn->priority == 1)
			continue;
		if (fn->start) {
			fn->busy = 0;
			result = fn->start();
			if (result < 0) {
				for (int j = 0; j < i; j++) {
					if (s.functions[j]->stop)
						s.functions[j]->stop();
				}
				return result;
			}
		}
	}
	s.status |= STATUS_STARTED;
	sceKernelSetEventFlag(s.eventFlag, EVF_START);
	info.eventDetail = s.type == USBSTOR_TYPE_DISK ? 5 : 0xffff;
	SceUdcdForDriver_78BF2A28(&info);
	return result;
}

static int attachStorage(int version, void *ctx)
{
	SceUdcdDeviceInfo info;

	s.speed = version == 2 ? 2 : 1;
	s.packetSize = version == 2 ? 512 : 64;
	notifyFunctions(USBSTOR_EVENT_ATTACH);
	s.flags |= FLAG_ATTACHED;
	initializeRequests();
	s.status = STATUS_READY | STATUS_STARTED;
	sceKernelSetEventFlag(s.eventFlag, EVF_ATTACH);
	return sceUdcdGetDeviceInfo(&info);
}

static void detachStorage(void *ctx)
{
	notifyFunctions(USBSTOR_EVENT_DETACH);
	s.flags &= ~FLAG_ATTACHED;
	sceKernelSetEventFlag(s.eventFlag, EVF_DETACH);
}

static int changeSetting(int number, int alternate, int bus)
{
	s.flags |= FLAG_SETTING;
	return sceKernelSetEventFlag(s.eventFlag, EVF_SETTING);
}

static int processRequest(int recipient, int number, SceUdcdEP0DeviceRequest *req, void *ctx)
{
	if (req->bRequest == USB_REQ_CLEAR_FEATURE && req->wValue == USB_FEATURE_ENDPOINT_HALT) {
		unsigned ep = req->wIndex & 0x1f;
		if (ep == 1)
			s.status &= ~STATUS_IN_STALLED;
		else if (ep == 2)
			s.status &= ~STATUS_OUT_STALLED;
	}
	if (number >= 0) {
		if (req->bmRequestType == (USB_CTRLTYPE_DIR_HOST2DEVICE | USB_CTRLTYPE_TYPE_CLASS |
					   USB_CTRLTYPE_REC_INTERFACE) &&
		    req->bRequest == USBMS_REQ_RESET) {
			s.flags |= FLAG_RESET;
			notifyFunctions(USBSTOR_EVENT_RESET);
			SceUdcdForDriver_38787672(&s.endpoints[1]);
			SceUdcdForDriver_38787672(&s.endpoints[2]);
			SceUdcdForDriver_9F53D64D(&s.endpoints[1]);
			SceUdcdForDriver_9F53D64D(&s.endpoints[2]);
			sceKernelSetEventFlag(s.eventFlag, EVF_RESET);
			return 0;
		}
		if (req->bmRequestType == (USB_CTRLTYPE_DIR_DEVICE2HOST | USB_CTRLTYPE_TYPE_CLASS |
					   USB_CTRLTYPE_REC_INTERFACE) &&
		    req->bRequest == USBMS_REQ_GET_MAX_LUN) {
			*s.controlBuffer = s.maxUnit;
			sceUdcdReqSend(&s.control.request);
			return 0;
		}
	}
	return STOR_ERROR_CONTROL;
}

static void completeControl(SceUdcdDeviceRequest *req)
{
	req->returnCode = REQUEST_IDLE;
}

static void completeSend(SceUdcdDeviceRequest *req)
{
	if (req->returnCode >= 0) {
		sceKernelSetEventFlag(s.eventFlag, EVF_SENT);
		req->returnCode = REQUEST_IDLE;
	} else if (req->returnCode == SCE_UDCD_RETCODE_CANCEL_TRANSMISSION) {
		sceKernelSetEventFlag(s.eventFlag, EVF_IN_CANCELLED);
		sceKernelClearEventFlag(s.eventFlag, ~EVF_IN_CANCELLED);
	}
}

static void completeReceive(SceUdcdDeviceRequest *req)
{
	if (req->returnCode >= 0) {
		sceKernelSetEventFlag(s.eventFlag, EVF_RECEIVED);
		req->returnCode = REQUEST_IDLE;
	} else if (req->returnCode == SCE_UDCD_RETCODE_CANCEL_TRANSMISSION) {
		sceKernelSetEventFlag(s.eventFlag, EVF_OUT_CANCELLED);
		sceKernelClearEventFlag(s.eventFlag, ~EVF_OUT_CANCELLED);
	}
}

static void completeCbw(SceUdcdDeviceRequest *req)
{
	if (req->returnCode >= 0) {
		sceKernelSetEventFlag(s.eventFlag, EVF_CBW_RECEIVED);
		req->returnCode = REQUEST_IDLE;
	} else if (s.flags & FLAG_SETTING) {
		sceKernelSetEventFlag(s.eventFlag, EVF_SETTING);
	} else {
		sceKernelSetEventFlag(s.eventFlag, EVF_OUT_CANCELLED);
		sceKernelClearEventFlag(s.eventFlag, ~EVF_OUT_CANCELLED);
	}
}

static void completeCsw(SceUdcdDeviceRequest *req)
{
	if (req->returnCode >= 0) {
		sceKernelSetEventFlag(s.eventFlag, EVF_CSW_SENT);
		req->returnCode = REQUEST_IDLE;
	} else if (req->returnCode == SCE_UDCD_RETCODE_CANCEL_TRANSMISSION) {
		sceKernelSetEventFlag(s.eventFlag, EVF_IN_CANCELLED);
		sceKernelClearEventFlag(s.eventFlag, ~EVF_IN_CANCELLED);
	}
}

static inline __attribute__((always_inline)) void cancelEndpoint(int ep, unsigned cancelled)
{
	unsigned timeout = USBSTOR_TRANSFER_TIMEOUT;
	if (SceUdcdForDriver_38787672(&s.endpoints[ep]) != (int)SCE_UDCD_ERROR_DRIVER_IN_PROGRESS ||
	    sceKernelWaitEventFlag(s.eventFlag, cancelled, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL,
				    &timeout) >= 0)
		SceUdcdForDriver_9F53D64D(&s.endpoints[ep]);
}

static int flushSend(unsigned timeout)
{
	unsigned bits;
	int result;

	if (s.send.request.returnCode >= REQUEST_QUEUED)
		return STOR_ERROR_DUPLICATE;
	if (s.flags & FLAG_RESET)
		return STOR_ERROR_CANCEL;
	result = sceUdcdReqSend(&s.send.request);
	if (result < 0)
		return result;
	result = sceKernelWaitEventFlag(s.eventFlag, EVF_IN_CANCELLED | EVF_SENT,
					 SCE_KERNEL_EVF_WAITMODE_OR |
					 SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, &timeout);
	s.send.request.size = 0;
	if ((bits & EVF_IN_CANCELLED) || s.send.request.returnCode < 0) {
		s.send.request.returnCode = REQUEST_IDLE;
		s.status |= STATUS_CANCELLED;
		return STOR_ERROR_CANCEL;
	}
	if (result == 0)
		return s.send.request.transmitted;
	if (result == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
		cancelEndpoint(1, EVF_IN_CANCELLED);
		s.status |= STATUS_TIMEOUT;
		return STOR_ERROR_TIMEOUT;
	}
	s.status |= STATUS_CANCELLED;
	return STOR_ERROR_CANCEL;
}

/* name unknown: send data to the host (a data-in command) */
int SceUsbstorForDriver_D3C7EE70(const void *buffer, unsigned size, unsigned timeout)
{
	unsigned limit, n, copied = 0;
	const uint8_t *p = buffer;
	int result;

	if (!buffer)
		return STOR_ERROR_ARGUMENT;
	if (!size)
		return 0;
	s.direction = DIRECTION_IN;
	unsigned previous = s.transferred;
	s.transferred += size;
	if (!(s.cbw->bmCBWFlags & USBMS_CBW_DATA_IN))
		return STOR_ERROR_DIRECTION;
	unsigned requested = s.cbw->dCBWDataTransferLength;
	if (previous >= requested)
		return 0;
	limit = size < requested - previous ? size : requested - previous;
	if (!(limit & (s.packetSize - 1))) {
		if (!s.send.request.size) {
			s.send.request.data = (void *)buffer;
			s.send.request.size = limit;
			return flushSend(timeout);
		}
		s.send.request.data = s.sendBuffer;
		if (!limit)
			return 0;
	}
	s.send.request.data = s.sendBuffer;
	while (limit) {
		n = limit < PACKET_SIZE - s.send.request.size ? limit : PACKET_SIZE - s.send.request.size;
		memcpy((uint8_t *)s.send.request.data + s.send.request.size, p, n);
		copied += n;
		limit -= n;
		p += n;
		s.send.request.size += n;
		if (s.send.request.size < PACKET_SIZE)
			return copied;
		result = flushSend(timeout);
		if (result < 0)
			return result;
		if (!limit)
			return copied;
		s.send.request.size = 0;
	}
	return 0;
}

/* name unknown: receive data from the host (a data-out command) */
int SceUsbstorForDriver_F1A690DC(void *buffer, unsigned size, unsigned timeout)
{
	unsigned bits;
	unsigned previous = s.transferred;
	unsigned requested = s.cbw->dCBWDataTransferLength;
	int result;

	if (!buffer)
		return STOR_ERROR_ARGUMENT;
	if (!size)
		return 0;
	s.direction = DIRECTION_OUT;
	s.transferred = previous + size;
	if (s.cbw->bmCBWFlags & USBMS_CBW_DATA_IN)
		return STOR_ERROR_DIRECTION;
	if (previous >= requested)
		return 0;
	s.receive.request.size = size < requested - previous ? size : requested - previous;
	s.receive.request.data = buffer;
	if (s.receive.request.returnCode >= REQUEST_QUEUED)
		return STOR_ERROR_DUPLICATE;
	if (s.flags & FLAG_RESET)
		return STOR_ERROR_CANCEL;
	result = sceUdcdReqRecv(&s.receive.request);
	if (result < 0)
		return result;
	result = sceKernelWaitEventFlag(s.eventFlag, EVF_OUT_CANCELLED | EVF_RECEIVED,
					 SCE_KERNEL_EVF_WAITMODE_OR |
					 SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, &timeout);
	if (!(bits & EVF_OUT_CANCELLED) && s.receive.request.returnCode >= 0) {
		if (result == 0)
			return s.receive.request.transmitted;
		if (result == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
			cancelEndpoint(2, EVF_OUT_CANCELLED);
			s.status |= STATUS_TIMEOUT;
			return STOR_ERROR_TIMEOUT;
		}
	}
	s.status |= STATUS_CANCELLED;
	return STOR_ERROR_CANCEL;
}

/* name unknown: register a storage function */
int SceUsbstorForDriver_C0944A04(struct UsbstorFunction *fn)
{
	int index;

	if (s.status & STATUS_STARTED)
		return STOR_ERROR_BUSY;
	if (!fn || !fn->command || (fn->unit & USBSTOR_UNIT_INVALID))
		return STOR_ERROR_ARGUMENT;
	if (s.count > 7)
		return STOR_ERROR_FULL;
	for (index = 0; index < s.count; index++) {
		if (s.functions[index] == fn)
			return STOR_ERROR_DUPLICATE;
	}
	if (fn->priority != USBSTOR_PRIORITY_LAST) {
		for (index = 0; index < s.count; index++) {
			if (fn->priority < s.functions[index]->priority)
				break;
		}
		if (index < s.count)
			memmove(s.functions + index + 1, s.functions + index,
				(s.count - index) * sizeof(s.functions[0]));
	}
	s.functions[index] = fn;
	s.count++;
	if (!(fn->unit & USBSTOR_UNIT_ANY)) {
		unsigned unit = fn->unit & USBSTOR_UNIT_MASK;
		if (s.maxUnit <= (int)unit)
			s.maxUnit = unit;
		s.units[unit]++;
	}
	return 0;
}

/* name unknown: unregister a storage function */
int SceUsbstorForDriver_EAC08C24(struct UsbstorFunction *fn)
{
	int index;

	if (s.status & STATUS_STARTED)
		return STOR_ERROR_BUSY;
	if (!fn || !s.count)
		return STOR_ERROR_ARGUMENT;
	for (index = 0; index < s.count && s.functions[index] != fn; index++)
		;
	if (index == s.count)
		return STOR_ERROR_NOT_FOUND;
	if (index < s.count - 1)
		memmove(s.functions + index, s.functions + index + 1,
			(s.count - index - 1) * sizeof(s.functions[0]));
	s.count--;
	if (!(fn->unit & USBSTOR_UNIT_ANY)) {
		s.units[fn->unit & USBSTOR_UNIT_MASK]--;
		s.maxUnit = 0;
		for (int i = 0; i < s.count; i++) {
			unsigned unit = s.functions[i]->unit & USBSTOR_UNIT_MASK;
			if (s.maxUnit < (int)unit)
				s.maxUnit = unit;
		}
	}
	return 0;
}

/* name unknown: read one of a unit's identification strings, printable ASCII only */
int SceUsbstorForDriver_B6689597(unsigned unit, int field, void *buffer, unsigned size)
{
	const char *p;
	uint8_t *out = buffer;
	unsigned limit;

	if (!buffer || unit >= UNITS ||
	    offsetof(struct DeviceInfo, unit) + (unit + 1) * sizeof(struct UnitIdentity) > DEVICE_INFO_SIZE)
		return STOR_ERROR_ARGUMENT;
	struct UnitIdentity *id = &s.deviceInfo->unit[unit];
	switch (field) {
	case 0:
		limit = sizeof(id->vendor);
		p = id->vendor;
		break;
	case 1:
		limit = sizeof(id->product);
		p = id->product;
		break;
	case 2:
		limit = sizeof(id->revision);
		p = id->revision;
		break;
	case 3:
		if (size > sizeof(id->vendorSpecific))
			size = sizeof(id->vendorSpecific);
		memcpy(buffer, id->vendorSpecific, size);
		return size;
	default:
		return STOR_ERROR_ARGUMENT;
	}
	if (size < limit)
		limit = size;
	for (unsigned i = 0; i < limit; i++)
		out[i] = p[i] >= ' ' && p[i] <= '~' ? p[i] : ' ';
	return limit;
}

/* name unknown: the interface subclass reported */
int SceUsbstorForDriver_BEF6B5AE(void)
{
	return s.deviceClass;
}

/* name unknown: set the interface subclass, before the driver starts */
int SceUsbstorForDriver_73F2FF9D(unsigned deviceClass)
{
	if ((s.status & (STATUS_READY | STATUS_STARTED)) != STATUS_READY)
		return STOR_ERROR_STATE;
	if (deviceClass >= 7)
		return STOR_ERROR_ARGUMENT;
	s.deviceClass = deviceClass;
	s.driver.configuration_hi->interfaceDescriptors->bInterfaceSubClass = deviceClass;
	s.driver.configuration->interfaceDescriptors->bInterfaceSubClass = deviceClass;
	return 0;
}

/* name unknown: set a unit's INQUIRY product and revision */
int SceUsbstorForDriver_ADB2AAF2(const char *product, const char *revision, unsigned unit)
{
	if (unit >= UNITS)
		return STOR_ERROR_ARGUMENT;
	memset(s.strings[unit].product, 0, sizeof(s.strings[unit].product));
	strncpy(s.strings[unit].product, product, sizeof(s.strings[unit].product));
	clearBytes((uint8_t *)s.strings[unit].revision, sizeof(s.strings[unit].revision));
	strncpy(s.strings[unit].revision, revision, sizeof(s.strings[unit].revision));
	return 0;
}

/* name unknown: nonzero while a function is busy */
unsigned SceUsbstorForDriver_77B7E69E(void)
{
	unsigned busy = 0;
	for (int i = 0; i < s.count; i++)
		busy |= s.functions[i]->busy;
	return busy;
}

int SceUsbstor_5B3DF8DF(void)
{
	unsigned saved = sceSyscallEnter();
	int result = SCE_ERROR_ERRNO_ENOSYS;

	if (SceSblACMgrForDriver_D0E11C89(0) || sceSblACMgrIsSystemProgram(0))
		result = SceUsbstorForDriver_77B7E69E();
	sceSyscallLeave(saved);
	return result;
}

static int initializeDescriptors(void)
{
	static const struct DescriptorSet template = {
		.device = {
			.bLength = USB_DT_DEVICE_SIZE, .bDescriptorType = USB_DT_DEVICE, .bcdUSB = 0x200,
			.bcdDevice = 0x100, .iProduct = 2,
			.iSerialNumber = 3, .bNumConfigurations = 1,
		},
		.config = {
			.bLength = USB_DT_CONFIG_SIZE, .bDescriptorType = USB_DT_CONFIG,
			.wTotalLength = USB_DT_CONFIG_SIZE + USB_DT_INTERFACE_SIZE + 2 * USB_DT_ENDPOINT_SIZE,
			.bNumInterfaces = 1, .bConfigurationValue = 1,
			.bmAttributes = 0xc0, .bMaxPower = 1,	/* self powered */
		},
		.settings = { .numDescriptors = 1 },
		.interfaces = {
			{ .bLength = USB_DT_INTERFACE_SIZE, .bDescriptorType = USB_DT_INTERFACE,
			  .bNumEndpoints = 2, .bInterfaceClass = USB_CLASS_MASS_STORAGE,
			  .bInterfaceSubClass = USBMS_SUBCLASS_SFF8070I,
			  .bInterfaceProtocol = USBMS_PROTOCOL_BULK_ONLY },
		},
		.endpoints = {
			{ .bLength = USB_DT_ENDPOINT_SIZE, .bDescriptorType = USB_DT_ENDPOINT,
			  .bEndpointAddress = USB_ENDPOINT_IN | 1, .bmAttributes = USB_ENDPOINT_TYPE_BULK },
			{ .bLength = USB_DT_ENDPOINT_SIZE, .bDescriptorType = USB_DT_ENDPOINT,
			  .bEndpointAddress = USB_ENDPOINT_OUT | 2, .bmAttributes = USB_ENDPOINT_TYPE_BULK },
		},
	};
	static const uint16_t names[2][16] = {
		{ '"', 'P', 'S', ' ', 'V', 'i', 't', 'a', '"', ' ', 'T', 'y', 'p', 'e', ' ', 'A' },
		{ '"', 'P', 'S', ' ', 'V', 'i', 't', 'a', '"', ' ', 'T', 'y', 'p', 'e', ' ', 'C' },
	};
	SceKernelAllocMemBlockKernelOpt opt;
	struct DescriptorSet *descriptors;
	struct Workspace *workspace;
	void *base;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT;
	s.memblock = sceKernelAllocMemBlockForKernel("SceUsbstor", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_RW,
					     4096, &opt);
	if (s.memblock < 0)
		return STOR_ERROR_MEMORY;
	if (sceKernelGetMemBlockBase(s.memblock, &base) < 0) {
		sceKernelFreeMemBlock(s.memblock);
		return STOR_ERROR_MEMORY;
	}
	descriptors = base;
	for (int i = 0; i < 2; i++) {
		struct DescriptorSet *d = descriptors + i;
		copyBytes((uint8_t *)d, (const uint8_t *)&template, sizeof(template));
		d->configuration.configDescriptors = &d->config;
		d->configuration.settings = &d->settings;
		d->configuration.interfaceDescriptors = d->interfaces;
		d->configuration.endpointDescriptors = d->endpoints;
		d->config.settings = &d->settings;
		d->settings.descriptors = d->interfaces;
		d->interfaces[0].endpoints = d->endpoints;
		d->endpoints[0].wMaxPacketSize = i == 0 ? 512 : 64;
		d->endpoints[1].wMaxPacketSize = i == 0 ? 512 : 64;
		d->device.bMaxPacketSize0 = 64;
	}
	s.driver.descriptor_hi = &descriptors[0].device;
	s.driver.configuration_hi = &descriptors[0].configuration;
	s.driver.descriptor = &descriptors[1].device;
	s.driver.configuration = &descriptors[1].configuration;
	workspace = (void *)(((uintptr_t)(descriptors + 2) + 63) & ~63);
	s.cbw = &workspace->cbw;
	s.csw = &workspace->csw;
	s.controlBuffer = workspace->control;
	s.sendBuffer = workspace->send;
	s.scratch = workspace->scratch;
	s.deviceInfo = &workspace->info;
	memset(s.deviceInfo, 0, DEVICE_INFO_SIZE);
	s.serial.bLength = USB_STRING_LENGTH(17);
	s.serial.bDescriptorType = USB_DT_STRING;
	s.product.bLength = USB_STRING_LENGTH(16);
	s.product.bDescriptorType = USB_DT_STRING;
	copyBytes((uint8_t *)s.product.bString, (const uint8_t *)names[s.type == USBSTOR_TYPE_CDROM],
		  sizeof(names[0]));
	s.eventFlag = sceKernelCreateEventFlag("SceUsbstor", SCE_KERNEL_EVF_ATTR_MULTI, 0, NULL);
	if (s.eventFlag < 0) {
		sceKernelFreeMemBlock(s.memblock);
		return STOR_ERROR_GENERAL;
	}
	s.status = 0;
	return 0;
}

static inline __attribute__((always_inline)) void resetBindings(void)
{
	for (int i = 0; i < UNITS; i++) {
		s.bindings[i] = -1;
		clearBytes((uint8_t *)&s.sense[i], sizeof(s.sense[i]));
	}
}

static inline __attribute__((always_inline)) void setSense(struct UsbstorSense *sense, int key, int asc)
{
	sense->key = key;
	sense->asc = asc;
	sense->ascq = 0;
}

/* name unknown: the storage transport worker and its command dispatcher. */
static int storageThread(SceSize size, void *args)
{
	unsigned bits, timeout, state = 0, unit, remaining, n;
	struct UsbstorSense *sense;
	const uint8_t *cdb;
	int result, commandResult, stallOut;

	s.status |= STATUS_STOPPED;
	if (initializeDescriptors() < 0)
		return 0;
	memset(s.identity, 0, sizeof(s.identity));
	if (sceIdStorageLookup(IDSTORAGE_LEAF_USB, 0, s.identity, 64) < 0) {
		for (int i = 0; i < 32; i++) {
			s.identity[i * 2] = 0;
			s.identity[i * 2 + 1] = '0';
		}
	}
	copyBytes((uint8_t *)s.serial.bString, s.identity + 31, 34);
start:
	sceKernelWaitEventFlag(s.eventFlag, EVF_START,
				SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				&bits, NULL);
next:
	if (state == 1)
		goto receive;
	if (state != 2)
		goto waitNormal;
sendCsw:
	if (!(s.status & STATUS_IN_STALLED)) {
		if (s.cswRequest.request.returnCode > REQUEST_IDLE)
			cancelEndpoint(1, EVF_IN_CANCELLED);
		if (sceUdcdReqSend(&s.cswRequest.request) < 0) {
			state = 1;
			goto receive;
		}
	}
	if (s.status & STATUS_IN_STALLED)
		goto waitTimed;
	goto waitNormal;
waitTimed:
	timeout = 1000;
	if (sceKernelWaitEventFlag(s.eventFlag, EVF_TRANSPORT,
				    SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				    &bits, &timeout) < 0)
		goto next;
	goto events;
waitNormal:
	timeout = 1000;
	if (sceKernelWaitEventFlag(s.eventFlag, EVF_TRANSPORT,
				    SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				    &bits, NULL) < 0)
		goto wake;
events:
	result = sceKernelCpuSuspendIntr();
	s.flags &= ~FLAG_RESET;
	sceKernelCpuResumeIntr(result);
	if (bits & EVF_OUT_CANCELLED)
		goto next;
	if (bits & EVF_STOP)
		goto wake;
	if (bits & EVF_SETTING) {
		state = 0;
		if (s.cbwRequest.request.returnCode >= REQUEST_QUEUED)
			goto waitNormal;
		state = s.cbwRequest.request.returnCode < 0;
		if (state)
			s.cbwRequest.request.returnCode = REQUEST_IDLE;
		result = sceKernelCpuSuspendIntr();
		s.flags &= ~FLAG_SETTING;
		sceKernelCpuResumeIntr(result);
	}
	if (bits & EVF_DETACH) {
		state = 0;
		s.status &= ~(STATUS_IN_STALLED | STATUS_OUT_STALLED);
		goto waitNormal;
	}
	if (bits & EVF_ATTACH) {
		resetBindings();
		state = 1;
		goto receive;
	}
	if (bits & EVF_RESET) {
		cancelEndpoint(1, EVF_IN_CANCELLED);
		cancelEndpoint(2, EVF_OUT_CANCELLED);
		resetBindings();
		s.status &= ~STATUS_BAD_CBW;
		state = 1;
		goto receive;
	}
	if (bits & EVF_CSW_SENT) {
		s.status &= ~STATUS_IN_STALLED;
		state = 1;
		goto receive;
	}
	if (!(bits & EVF_CBW_RECEIVED))
		goto next;
	if (s.status & STATUS_BAD_CBW) {
		state = 1;
		goto receive;
	}
	s.status &= ~(STATUS_TIMEOUT | STATUS_CANCELLED | STATUS_OUT_STALLED);
	if (s.cbwRequest.request.transmitted != USBMS_CBW_SIZE ||
	    s.cbw->dCBWSignature != USBMS_CBW_SIGNATURE) {
		s.status |= STATUS_IN_STALLED;
		SceUdcdForDriver_34079250(&s.endpoints[1]);
		s.status |= STATUS_BAD_CBW;
		state = 1;
		goto receive;
	}
	remaining = s.cbw->dCBWDataTransferLength;
	unit = s.cbw->bCBWLUN & USBMS_CBW_LUN_MASK;
	if (unit > 6)
		unit = SENSE_UNIT_OTHER;
	cdb = s.cbw->CBWCB;
	sense = &s.sense[unit];
	s.send.request.size = 0;
	s.transferred = 0;
	s.residue = 0;
	s.direction = s.cbw->bmCBWFlags & USBMS_CBW_DATA_IN ? DIRECTION_IN : DIRECTION_OUT;
	if (!s.units[unit]) {
		setSense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_LUN_NOT_SUPPORTED);
		s.bindings[unit] = -1;
	}
	commandResult = USBSTOR_COMMAND_UNKNOWN;
	if (cdb[0] == SCSI_REQUEST_SENSE) {
		int binding = s.bindings[unit];
		if (binding >= 0 && s.functions[binding]->requestSense && s.functions[binding]->command)
			commandResult = s.functions[binding]->command(unit, cdb, sense);
		if (commandResult == USBSTOR_COMMAND_UNKNOWN) {
			struct ScsiSenseData response = {0};
			response.responseCode = sense->deferred ? SCSI_SENSE_DEFERRED : SCSI_SENSE_CURRENT;
			response.key = sense->key;
			response.additionalLength = SCSI_SENSE_ADDITIONAL;
			response.asc = sense->asc;
			response.ascq = sense->ascq;
			response.specific[0] = sense->specific[0];
			response.specific[1] = sense->specific[1];
			response.specific[2] = sense->specific[2];
			n = cdb[4] < sizeof(response) ? cdb[4] : sizeof(response);
			if (SceUsbstorForDriver_D3C7EE70(&response, n, USBSTOR_TRANSFER_TIMEOUT) < 0)
				commandResult = USBSTOR_COMMAND_FAILED;
			else {
				commandResult = USBSTOR_COMMAND_DONE;
				clearBytes((uint8_t *)sense, sizeof(*sense));
			}
		}
	} else if (!s.units[unit] && cdb[0] != SCSI_INQUIRY) {
		commandResult = USBSTOR_COMMAND_FAILED;
	} else {
		if (s.units[unit])
			clearBytes((uint8_t *)sense, sizeof(*sense));
		for (int i = 0; i < s.count; i++) {
			struct UsbstorFunction *function = s.functions[i];
			if (!function->command)
				continue;
			/* a unit with functions: its own and the ANY ones; else only unit 0 */
			if (s.units[unit] ? !(function->unit & USBSTOR_UNIT_ANY) &&
					    (function->unit & USBSTOR_UNIT_MASK) != unit
					  : function->unit != 0)
				continue;
			commandResult = function->command(unit, cdb, sense);
			if (commandResult != USBSTOR_COMMAND_UNKNOWN) {
				if (s.units[unit])
					s.bindings[unit] = i;
				break;
			}
		}
		if (commandResult == USBSTOR_COMMAND_UNKNOWN) {
			setSense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB);
			s.bindings[unit] = -1;
		}
	}
	if ((s.flags & (FLAG_ATTACHED | FLAG_SETTING)) != FLAG_ATTACHED || (s.status & STATUS_CANCELLED))
		goto abortCommand;
	stallOut = 0;
	if (!remaining) {
		if (s.transferred)
			commandResult = USBSTOR_COMMAND_PHASE_ERROR;
		goto buildCsw;
	}
	if (s.cbw->bmCBWFlags & USBMS_CBW_DATA_IN) {
		if (s.direction == DIRECTION_OUT || remaining < s.transferred ||
		    (s.status & STATUS_TIMEOUT)) {
			commandResult = USBSTOR_COMMAND_PHASE_ERROR;
			goto checkCommand;
		}
		if (remaining > s.transferred) {
			if (commandResult == USBSTOR_COMMAND_UNKNOWN) {
				s.residue = remaining;
				s.csw->dCSWSignature = USBMS_CSW_SIGNATURE;
				s.csw->dCSWTag = s.cbw->dCBWTag;
				s.csw->dCSWDataResidue = remaining;
				s.csw->bCSWStatus = USBMS_CSW_FAILED;
				s.status |= STATUS_IN_STALLED;
				SceUdcdForDriver_34079250(&s.endpoints[1]);
				state = 2;
				goto sendCsw;
			}
			/* pad the data the command did not send */
			remaining -= s.transferred;
			s.residue = remaining;
			memset(s.scratch, 0, PACKET_SIZE);
			while (remaining) {
				n = remaining < PACKET_SIZE ? remaining : PACKET_SIZE;
				if (SceUsbstorForDriver_D3C7EE70(s.scratch, n, USBSTOR_TRANSFER_TIMEOUT) < 0) {
					commandResult = USBSTOR_COMMAND_PHASE_ERROR;
					goto checkCommand;
				}
				remaining -= n;
			}
		}
		if (s.send.request.size && commandResult != USBSTOR_COMMAND_PHASE_ERROR)
			commandResult = flushSend(USBSTOR_TRANSFER_TIMEOUT) < 0 ? USBSTOR_COMMAND_PHASE_ERROR
								       : USBSTOR_COMMAND_DONE;
		goto checkCommand;
	}
	if (s.direction == DIRECTION_IN) {
		s.status |= STATUS_OUT_STALLED;
		stallOut = 1;
		commandResult = USBSTOR_COMMAND_PHASE_ERROR;
		goto checkStatus;
	}
	if (s.status & STATUS_TIMEOUT) {
		s.status |= STATUS_OUT_STALLED;
		stallOut = 1;
		goto checkStatus;
	}
	if (remaining < s.transferred) {
		commandResult = USBSTOR_COMMAND_PHASE_ERROR;
		goto buildCsw;
	}
	if (remaining == s.transferred)
		goto buildCsw;
	/* drain the data the command did not take */
	remaining -= s.transferred;
	s.residue = remaining;
	while (remaining) {
		n = remaining < PACKET_SIZE ? remaining : PACKET_SIZE;
		if (SceUsbstorForDriver_F1A690DC(s.scratch, n, USBSTOR_TRANSFER_TIMEOUT) < 0) {
			stallOut = 1;
			s.status |= STATUS_OUT_STALLED;
			/* The original preserves the command result on drain failure. */
			goto checkCommand;
		}
		remaining -= n;
	}
checkCommand:
	if ((s.flags & (FLAG_ATTACHED | FLAG_SETTING)) != FLAG_ATTACHED)
		goto abortCommand;
checkStatus:
	if (s.status & STATUS_CANCELLED)
		goto abortCommand;
buildCsw:
	if (s.status & STATUS_TIMEOUT) {
		s.status &= ~STATUS_TIMEOUT;
		setSense(sense, SCSI_KEY_NOT_READY, SCSI_ASC_INTERFACE_FAILURE);
		s.bindings[unit] = -1;
	}
	s.csw->dCSWSignature = USBMS_CSW_SIGNATURE;
	s.csw->dCSWTag = s.cbw->dCBWTag;
	s.csw->dCSWDataResidue = s.residue;
	s.csw->bCSWStatus = commandResult == USBSTOR_COMMAND_DONE ? USBMS_CSW_PASSED :
			    commandResult == USBSTOR_COMMAND_FAILED ||
			    commandResult == USBSTOR_COMMAND_UNKNOWN ? USBMS_CSW_FAILED :
			    USBMS_CSW_PHASE_ERROR;
	if (commandResult != USBSTOR_COMMAND_DONE && commandResult != USBSTOR_COMMAND_FAILED) {
		s.status |= STATUS_IN_STALLED;
		SceUdcdForDriver_34079250(&s.endpoints[1]);
	}
	if (stallOut)
		SceUdcdForDriver_34079250(&s.endpoints[2]);
	state = 2;
	goto sendCsw;
abortCommand:
	s.status &= ~STATUS_CANCELLED;
	setSense(sense, SCSI_KEY_NOT_READY, SCSI_ASC_INTERFACE_FAILURE);
	s.bindings[unit] = -1;
	state = 0;
	goto waitNormal;
receive:
	if (s.cbwRequest.request.returnCode < REQUEST_QUEUED)
		s.cbwRequest.request.returnCode = state;
	else {
		timeout = USBSTOR_TRANSFER_TIMEOUT;
		result = SceUdcdForDriver_38787672(&s.endpoints[2]);
		if (result != (int)SCE_UDCD_ERROR_DRIVER_IN_PROGRESS ||
		    sceKernelWaitEventFlag(s.eventFlag, EVF_OUT_CANCELLED,
					    SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					    NULL, &timeout) >= 0)
			SceUdcdForDriver_9F53D64D(&s.endpoints[2]);
	}
	if (sceUdcdReqRecv(&s.cbwRequest.request) < 0 || (s.status & STATUS_OUT_STALLED))
		goto waitTimed;
	goto waitNormal;
wake:
	sceKernelSetEventFlag(s.eventFlag, EVF_STOPPED);
	goto start;
}

int module_start(SceSize size, void *args)
{
	s.endpoints[0].direction = 0xff;	/* control: both */
	s.endpoints[0].driverEndpointNumber = 0;
	s.endpoints[1].direction = USB_ENDPOINT_IN;
	s.endpoints[1].driverEndpointNumber = 1;
	s.endpoints[2].direction = USB_ENDPOINT_OUT;
	s.endpoints[2].driverEndpointNumber = 2;
	s.interface.expectNumber = 0;
	s.interface.numInterfaces = 1;
	s.driver.interface = &s.interface;
	s.driver.endpoints = s.endpoints;
	s.driver.driverName = "USBStor_Driver";
	s.driver.numEndpoints = 3;
	s.driver.processRequest = processRequest;
	s.driver.changeSetting = changeSetting;
	s.driver.attach = attachStorage;
	s.driver.detach = detachStorage;
	s.driver.start = startStorage;
	s.driver.stop = stopStorage;
	s.driver.configure = NULL;
	s.driver.stringDescriptors = NULL;
	s.driver.stringDescriptorProduct = &s.product;
	s.driver.stringDescriptorSerial = &s.serial;
	/* everything from deviceClass up to the driver */
	memset(&s.deviceClass, 0, offsetof(__typeof__(s), driver) - offsetof(__typeof__(s), deviceClass));
	s.thread = sceKernelCreateThread("SceUsbstor", storageThread, 0x1f, 4096, 0,
					  STORAGE_THREAD_CPU, NULL);
	if (s.thread < 0)
		return SCE_KERNEL_START_FAILED;
	if (sceKernelStartThread(s.thread, 0, NULL) < 0) {
		s.status &= ~(STATUS_READY | STATUS_STARTED);
		sceKernelDeleteThread(s.thread);
		return SCE_KERNEL_START_FAILED;
	}
	if (sceUdcdRegister(&s.driver) < 0) {
		sceKernelDeleteThread(s.thread);
		return SCE_KERNEL_START_FAILED;
	}
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize size, void *args)
{
	sceKernelDeleteThread(s.thread);
	sceKernelWaitThreadEnd(s.thread, NULL, NULL);
	sceKernelDeleteEventFlag(s.eventFlag);
	sceKernelFreeMemBlock(s.memblock);
	s.status = (s.status & ~(STATUS_READY | STATUS_STARTED)) | STATUS_STOPPED;
	return sceUdcdUnregister(&s.driver) < 0 ? SCE_KERNEL_STOP_FAIL : SCE_KERNEL_STOP_SUCCESS;
}
