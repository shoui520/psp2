/*
 * SceUsbd (os0:kd/bootimage/usbd): the USB host stack's exports, each declared once with its
 * definition as the prototype, and the records its class drivers pass. No Sony header or record
 * names the types; the names are vitasdk's (psp2kern/usbd.h, without its k prefix), the isochronous
 * request is usbd's own (vitasdk's layout is not the one usbd reads).
 */
#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

/* SceUsbd's errors */
#define SCE_USBD_ERROR_NOT_INITIALIZED		0x80240001
#define SCE_USBD_ERROR_ALREADY_INITIALIZED	0x80240002
#define SCE_USBD_ERROR_INVALID_PARAM		0x80240003
#define SCE_USBD_ERROR_PIPE_NOT_FOUND		0x80240004
#define SCE_USBD_ERROR_NO_MEMORY		0x80240005
#define SCE_USBD_ERROR_DEVICE_NOT_FOUND		0x80240006
#define SCE_USBD_ERROR_80240007			0x80240007	/* a stalled endpoint (OHCI) */
#define SCE_USBD_ERROR_80240009			0x80240009
#define SCE_USBD_ERROR_8024000A			0x8024000A
#define SCE_USBD_ERROR_FATAL			0x802400FF

/* bDeviceClass, bInterfaceClass */
#define SCE_USBD_CLASS_AUDIO			0x01
#define SCE_USBD_CLASS_HID			0x03
#define SCE_USBD_CLASS_STORAGE			0x08
#define SCE_USBD_CLASS_HUB			0x09

/* bDescriptorType */
typedef enum SceUsbdDescriptorType {
	SCE_USBD_DESCRIPTOR_DEVICE		= 0x01,
	SCE_USBD_DESCRIPTOR_CONFIGURATION	= 0x02,
	SCE_USBD_DESCRIPTOR_STRING		= 0x03,
	SCE_USBD_DESCRIPTOR_INTERFACE		= 0x04,
	SCE_USBD_DESCRIPTOR_ENDPOINT		= 0x05,
	SCE_USBD_DESCRIPTOR_DEVICE_QUALIFIER	= 0x06,
	SCE_USBD_DESCRIPTOR_OTHER_SPEED		= 0x07,
	SCE_USBD_DESCRIPTOR_INTERFACE_POWER	= 0x08,
	SCE_USBD_DESCRIPTOR_OTG			= 0x09,
	SCE_USBD_DESCRIPTOR_HID			= 0x21,
	SCE_USBD_DESCRIPTOR_REPORT		= 0x22
} SceUsbdDescriptorType;

/* SceUsbdEndpointDescriptor.bmAttributes */
#define SCE_USBD_ENDPOINT_TRANSFER_TYPE_BITS		0x03
#define SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL		0x00
#define SCE_USBD_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS	0x01
#define SCE_USBD_ENDPOINT_TRANSFER_TYPE_BULK		0x02
#define SCE_USBD_ENDPOINT_TRANSFER_TYPE_INTERRUPT	0x03

/* SceUsbdEndpointDescriptor.bEndpointAddress */
#define SCE_USBD_ENDPOINT_DIRECTION_BITS	0x80
#define SCE_USBD_ENDPOINT_DIRECTION_OUT		0x00
#define SCE_USBD_ENDPOINT_DIRECTION_IN		0x80

typedef struct SceUsbdDeviceDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdUSB;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubclass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t iManufacturer;
	uint8_t iProduct;
	uint8_t iSerialNumber;
	uint8_t bNumConfigurations;
} SceUsbdDeviceDescriptor;

typedef struct SceUsbdConfigurationDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wTotalLength;
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue;
	uint8_t iConfiguration;
	uint8_t bmAttributes;
	uint8_t MaxPower;
} SceUsbdConfigurationDescriptor;

typedef struct SceUsbdInterfaceDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubclass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
} SceUsbdInterfaceDescriptor;

typedef struct SceUsbdEndpointDescriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
} SceUsbdEndpointDescriptor;

/* the hub port at each tier (sceUsbdGetDeviceAddress) */
typedef struct SceUsbdDeviceAddress {
	SceUInt8 port[6];
} SceUsbdDeviceAddress;

_Static_assert(sizeof(SceUsbdDeviceDescriptor) == 0x12, "SceUsbdDeviceDescriptor");
_Static_assert(sizeof(SceUsbdConfigurationDescriptor) == 0xa, "SceUsbdConfigurationDescriptor");
_Static_assert(sizeof(SceUsbdInterfaceDescriptor) == 9, "SceUsbdInterfaceDescriptor");
_Static_assert(sizeof(SceUsbdEndpointDescriptor) == 8, "SceUsbdEndpointDescriptor");
_Static_assert(sizeof(SceUsbdDeviceAddress) == 6, "SceUsbdDeviceAddress");

/* a class driver (SceUsbdForDriver_1EC94F18) */
typedef struct SceUsbdDriver {
	const char *name;
	int (*probe)(int deviceId);
	int (*attach)(int deviceId);
	int (*detach)(int deviceId);
} SceUsbdDriver;

typedef struct SceUsbdCompositeDriver {
	const char *name;
	int (*probe)(int deviceId, SceUsbdInterfaceDescriptor **descriptor);
	int (*attach)(int deviceId);
	int (*detach)(int deviceId);
} SceUsbdCompositeDriver;

/* a control transfer's setup packet */
typedef struct SceUsbdDeviceRequest {
	SceUInt8 bmRequestType;
	SceUInt8 bRequest;
	SceUInt16 wValue;
	SceUInt16 wIndex;
	SceUInt16 wLength;
} SceUsbdDeviceRequest;

_Static_assert(sizeof(SceUsbdDriver) == 16, "SceUsbdDriver");
_Static_assert(sizeof(SceUsbdCompositeDriver) == 16, "SceUsbdCompositeDriver");
_Static_assert(sizeof(SceUsbdDeviceRequest) == 8, "SceUsbdDeviceRequest");

/* SceUsbdDeviceRequest.bmRequestType */
typedef enum SceUsbdReqtype {
	SCE_USBD_REQTYPE_DIR_BITS		= 0x80,
	SCE_USBD_REQTYPE_DIR_TO_DEVICE		= 0x00,
	SCE_USBD_REQTYPE_DIR_TO_HOST		= 0x80,
	SCE_USBD_REQTYPE_TYPE_BITS		= 0x60,
	SCE_USBD_REQTYPE_TYPE_STANDARD		= 0x00,
	SCE_USBD_REQTYPE_TYPE_CLASS		= 0x20,
	SCE_USBD_REQTYPE_TYPE_VENDOR		= 0x40,
	SCE_USBD_REQTYPE_TYPE_RESERVED		= 0x60,
	SCE_USBD_REQTYPE_RECIP_BITS		= 0x1f,
	SCE_USBD_REQTYPE_RECIP_DEVICE		= 0x00,
	SCE_USBD_REQTYPE_RECIP_INTERFACE	= 0x01,
	SCE_USBD_REQTYPE_RECIP_ENDPOINT		= 0x02,
	SCE_USBD_REQTYPE_RECIP_OTHER		= 0x03
} SceUsbdReqtype;

/* SceUsbdDeviceRequest.bRequest */
typedef enum SceUsbdRequest {
	SCE_USBD_REQUEST_GET_STATUS		= 0x00,
	SCE_USBD_REQUEST_CLEAR_FEATURE		= 0x01,
	SCE_USBD_REQUEST_SET_FEATURE		= 0x03,
	SCE_USBD_REQUEST_SET_ADDRESS		= 0x05,
	SCE_USBD_REQUEST_GET_DESCRIPTOR		= 0x06,
	SCE_USBD_REQUEST_SET_DESCRIPTOR		= 0x07,
	SCE_USBD_REQUEST_GET_CONFIGURATION	= 0x08,
	SCE_USBD_REQUEST_SET_CONFIGURATION	= 0x09,
	SCE_USBD_REQUEST_GET_INTERFACE		= 0x0a,
	SCE_USBD_REQUEST_SET_INTERFACE		= 0x0b,
	SCE_USBD_REQUEST_SYNCH_FRAME		= 0x0c
} SceUsbdRequest;

/* a device's speed (SceUsbdForDriver_C3BECF5F) */
#define SCE_USBD_DEVICE_SPEED_LS	0
#define SCE_USBD_DEVICE_SPEED_FS	1
#define SCE_USBD_DEVICE_SPEED_HS	2

/* a transfer's completion: its result and the bytes moved */
typedef void (*SceUsbdDoneCallback)(int result, int count, void *arg);

/* the packet list sceUsbdIsochTransferData and SceUsbdForDriver_2249BF99 take */
typedef struct UsbdIsochPacket {
	SceUInt16 length;
	SceUInt16 status;
} UsbdIsochPacket;

typedef struct UsbdIsochRequest {
	void *buffer;
	int numPackets;			/* 1..8 */
	UsbdIsochPacket packets[8];
} UsbdIsochRequest;

_Static_assert(sizeof(UsbdIsochRequest) == 0x28, "UsbdIsochRequest");

/* an isochronous transfer's completion: its result and the packets' lengths and statuses */
typedef void (*SceUsbdIsochDoneCallback)(int result, UsbdIsochRequest *status, void *arg);

/* vitasdk: ksceUsbdGetDeviceLocation */
int SceUsbdForDriver_0E73D253(SceUID deviceId, SceUInt32 *location);
/* vitasdk: ksceUsbdRegisterDriver */
int SceUsbdForDriver_1EC94F18(const SceUsbdDriver *driver);
/* vitasdk: ksceUsbdUnregisterDriver */
int SceUsbdForDriver_216F108D(const SceUsbdDriver *driver);
/* vitasdk: ksceUsbdIsochronousTransfer */
int SceUsbdForDriver_2249BF99(SceUID pipeId, UsbdIsochRequest *transfer, SceUsbdIsochDoneCallback cb, void *user_data);
/* vitasdk: ksceUsbdBulkTransfer */
int SceUsbdForDriver_235C094A(SceUID pipeId, unsigned char *buffer, unsigned int length, SceUsbdDoneCallback cb, void *user_data);
/* vitasdk: ksceUsbdControlTransfer */
int SceUsbdForDriver_2E05660F(SceUID pipeId, const SceUsbdDeviceRequest *req, unsigned char *buffer, SceUsbdDoneCallback cb, void *user_data);
/* vitasdk: ksceUsbdBulkTransfer2 */
int SceUsbdForDriver_470D5289(int pipeId, unsigned char *buffer, unsigned int length, SceUsbdDoneCallback cb, void *user_data);
/* vitasdk: ksceUsbdInterruptTransfer */
int SceUsbdForDriver_A0BF85B8(SceUID pipeId, unsigned char *buffer, SceSize length, SceUsbdDoneCallback cb, void *user_data);
/* vitasdk: ksceUsbdGetDeviceSpeed */
int SceUsbdForDriver_C3BECF5F(int deviceId, SceUInt32 *speed);
int sceUsbdRegisterCompositeLdd(const SceUsbdCompositeDriver *driver);
/* vitasdk: ksceUsbdUnregisterCompositeLdd */
int SceUsbdForDriver_7934BB4A(const SceUsbdCompositeDriver *driver);

/* vitasdk: ksceUsbd_05073925 */
int SceUsbdForDriver_05073925(SceUID deviceId, int *frame, int *mask);
/* vitasdk: ksceUsbdOpenPipe */
SceUID SceUsbdForDriver_1CDBFF9F(int deviceId, SceUsbdEndpointDescriptor *endpoint);
int SceUsbdForDriver_3EBF5FFE(int busIndex);	/* vitasdk: ksceUsbdSuspend */
int SceUsbdForDriver_7938DAC7(SceUID pipeId);	/* vitasdk: ksceUsbd_7938DAC7 */
/* vitasdk: ksceUsbdScanStaticDescriptor */
void *SceUsbdForDriver_BC3EF82B(SceUID deviceId, void *start, SceUsbdDescriptorType type);
int SceUsbdForDriver_F304DC5C(SceUID pipeId);	/* vitasdk: ksceUsbdClosePipe */
int sceUsbdAttach(SceUID uid, unsigned int driverId, unsigned int busIndex, unsigned int address);
int sceUsbdClosePipe(SceUID uid, SceUID pipeId);
int sceUsbdEnd(SceUID uid);
int sceUsbdGetDescriptor(SceUID uid, unsigned int deviceId, void *descriptor, SceSize size);
int sceUsbdGetDescriptorSize(SceUID uid, unsigned int deviceId);
int sceUsbdGetDeviceAddress(SceUID uid, SceUID deviceId, SceUsbdDeviceAddress *addr);
int sceUsbdGetDeviceSpeed(SceUID uid, SceUID deviceId, unsigned int *speed);
int sceUsbdHostStart(int busIndex);
int sceUsbdHostStop(int busIndex);
int sceUsbdInit(SceUID *uid);
SceUID sceUsbdOpenDefaultPipe(SceUID uid, SceUID deviceId);
int sceUsbdRegisterCallback(SceUID cbid, int flag);
SceUID sceUsbdRegisterLdd(SceUID uid, const char *name);
int sceUsbdResetDevice(SceUID uid, SceUID deviceId);
int sceUsbdResume(int busIndex);
int sceUsbdSuspendPhase2(int busIndex, int flag);
int sceUsbdUnregisterCallback(SceUID cbid);
int sceUsbdUnregisterLdd(SceUID uid, const char *userName);
