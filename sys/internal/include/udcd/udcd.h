/*
 * SceUdcd (os0:kd/bootimage/udcd): the USB device controller's exports, each declared once with
 * its definition as the prototype, and the records its function drivers pass. No Sony header or
 * record names them; the names and layouts are vitasdk's (psp2common/udcd.h, psp2kern/udcd.h),
 * what vitasdk leaves as offsets is named from SceUdcd's own use.
 */
#pragma once

#include <scetypes.h>
#include <stdint.h>

/* SceUdcd's errors */
#define SCE_UDCD_ERROR_ALREADY_DONE		0x80243001
#define SCE_UDCD_ERROR_INVALID_ARGUMENT		0x80243002
#define SCE_UDCD_ERROR_ARGUMENT_EXCEEDED_LIMIT	0x80243003
#define SCE_UDCD_ERROR_MEMORY_EXHAUSTED		0x80243004
#define SCE_UDCD_ERROR_DRIVER_NOT_FOUND		0x80243005
#define SCE_UDCD_ERROR_DRIVER_IN_PROGRESS	0x80243006
#define SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED	0x80243007
#define SCE_UDCD_ERROR_WAIT_TIMEOUT		0x80243008
#define SCE_UDCD_ERROR_WAIT_CANCEL		0x80243009
#define SCE_UDCD_ERROR_USBDRIVER_INVALID_DRIVER	0x80243200
#define SCE_UDCD_ERROR_USBDRIVER_INVALID_NAME	0x80243201
#define SCE_UDCD_ERROR_USBDRIVER_INVALID_FUNCS	0x80243202

/* the state event flag's bits, one group per SceUdcdWaitParam word */
typedef enum SceUdcdStatus {
	SCE_UDCD_STATUS_CONNECTION_NEW		= 0x0001,
	SCE_UDCD_STATUS_CONNECTION_ESTABLISHED	= 0x0002,
	SCE_UDCD_STATUS_CONNECTION_SUSPENDED	= 0x0004,
	SCE_UDCD_STATUS_CABLE_DISCONNECTED	= 0x0010,
	SCE_UDCD_STATUS_CABLE_CONNECTED		= 0x0020,
	SCE_UDCD_STATUS_DEACTIVATED		= 0x0100,
	SCE_UDCD_STATUS_ACTIVATED		= 0x0200,
	SCE_UDCD_STATUS_IS_CHARGING		= 0x0400,
	SCE_UDCD_STATUS_USE_USB_CHARGING	= 0x0800,
	SCE_UDCD_STATUS_CONTROLLER_STOPPED	= 0x1000,	/* vitasdk UNKNOWN_1000 */
	SCE_UDCD_STATUS_CONTROLLER_STARTED	= 0x2000	/* vitasdk UNKNOWN_2000 */
} SceUdcdStatus;

typedef enum SceUdcdStatusDriver {
	SCE_UDCD_STATUS_DRIVER_STARTED		= 0x01,
	SCE_UDCD_STATUS_DRIVER_REGISTERED	= 0x02
} SceUdcdStatusDriver;

/* the bus's last event, as a driver reads and sets it; udcd uses the first two words */
typedef struct SceUdcdDeviceInfo {
	unsigned int lastEvent;		/* what happened last (1: cable connected, ...) */
	unsigned int eventDetail;
	unsigned int reserved[14];
} SceUdcdDeviceInfo;

/* wait until any of the given state bits is set (and driverName, if any, runs) */
typedef struct SceUdcdWaitParam {
	int controller;			/* SCE_UDCD_STATUS_CONTROLLER_* */
	int status;			/* DEACTIVATED, ACTIVATED, IS_CHARGING */
	int cable;			/* SCE_UDCD_STATUS_CABLE_* */
	int connection;			/* SCE_UDCD_STATUS_CONNECTION_* */
	int charging;			/* USE_USB_CHARGING; not waited on */
	const char *driverName;
} SceUdcdWaitParam;

/* the USB 2.0 chapter 9 values the function drivers use */
#define USB_CLASS_MASS_STORAGE		0x08

#define USB_DT_DEVICE			0x01
#define USB_DT_CONFIG			0x02
#define USB_DT_STRING			0x03
#define USB_DT_INTERFACE		0x04
#define USB_DT_ENDPOINT			0x05

#define USB_DT_DEVICE_SIZE		18
#define USB_DT_CONFIG_SIZE		9
#define USB_DT_INTERFACE_SIZE		9
#define USB_DT_ENDPOINT_SIZE		7

#define USB_REQ_CLEAR_FEATURE		0x01
#define USB_FEATURE_ENDPOINT_HALT	0

#define USB_CTRLTYPE_DIR_HOST2DEVICE	(0 << 7)
#define USB_CTRLTYPE_DIR_DEVICE2HOST	(1 << 7)
#define USB_CTRLTYPE_TYPE_CLASS		(1 << 5)
#define USB_CTRLTYPE_TYPE_MASK		(3 << 5)
#define USB_CTRLTYPE_REC_INTERFACE	1

#define USB_ENDPOINT_IN			0x80
#define USB_ENDPOINT_OUT		0x00
#define USB_ENDPOINT_TYPE_BULK		2

/* the CDC ACM class requests and notification of the serial functions */
#define CDC_SET_LINE_CODING		0x20
#define CDC_GET_LINE_CODING		0x21
#define CDC_SET_CONTROL_LINE_STATE	0x22
#define CDC_SEND_BREAK			0x23
#define CDC_NOTIFY_SERIAL_STATE		0x20

#define CDC_LINE_STATE_DTR		0x0001	/* SET_CONTROL_LINE_STATE */
#define CDC_SERIAL_STATE_DCD		0x0001	/* SERIAL_STATE: bRxCarrier */
#define CDC_SERIAL_STATE_DSR		0x0002	/* SERIAL_STATE: bTxCarrier */

/* SceUdcdDeviceRequest.returnCode */
#define SCE_UDCD_RETCODE_CANCEL			(-1)
#define SCE_UDCD_RETCODE_CANCEL_ALL		(-2)
#define SCE_UDCD_RETCODE_CANCEL_TRANSMISSION	(-3)
#define SCE_UDCD_RETCODE_SUCCESS		0
#define SCE_UDCD_RETCODE_SEND			1
#define SCE_UDCD_RETCODE_RECV			2

/* SceUdcdDeviceRequest.attributes */
#define SCE_UDCD_DEVICE_REQUEST_ATTR_PHYCONT	0x00000001

typedef struct SceUdcdStringDescriptor {
	unsigned char bLength;
	unsigned char bDescriptorType;
	short bString[31];
} SceUdcdStringDescriptor;

typedef struct __attribute__((aligned(4))) SceUdcdDeviceDescriptor {
	unsigned char bLength;
	unsigned char bDescriptorType;
	unsigned short bcdUSB;
	unsigned char bDeviceClass;
	unsigned char bDeviceSubClass;
	unsigned char bDeviceProtocol;
	unsigned char bMaxPacketSize0;
	unsigned short idVendor;
	unsigned short idProduct;
	unsigned short bcdDevice;
	unsigned char iManufacturer;
	unsigned char iProduct;
	unsigned char iSerialNumber;
	unsigned char bNumConfigurations;
} SceUdcdDeviceDescriptor;

typedef struct __attribute__((aligned(4))) SceUdcdDeviceQualifierDescriptor {
	unsigned char bLength;
	unsigned char bDescriptorType;
	unsigned short bcdUSB;
	unsigned char bDeviceClass;
	unsigned char bDeviceSubClass;
	unsigned char bDeviceProtocol;
	unsigned char bMaxPacketSize0;
	unsigned char bNumConfigurations;
	unsigned char bReserved;
} SceUdcdDeviceQualifierDescriptor;

typedef struct SceUdcdConfigDescriptor {
	unsigned char bLength;
	unsigned char bDescriptorType;
	unsigned short wTotalLength;
	unsigned char bNumInterfaces;
	unsigned char bConfigurationValue;
	unsigned char iConfiguration;
	unsigned char bmAttributes;
	unsigned char bMaxPower;
	struct SceUdcdInterfaceSettings *settings;
	unsigned char *extra;			/* the class descriptors after it */
	int extraLength;
} SceUdcdConfigDescriptor;

typedef struct SceUdcdInterfaceSettings {
	struct SceUdcdInterfaceDescriptor *descriptors;
	unsigned int alternateSetting;
	unsigned int numDescriptors;
} SceUdcdInterfaceSettings;

typedef struct SceUdcdInterfaceDescriptor {
	unsigned char bLength;
	unsigned char bDescriptorType;
	unsigned char bInterfaceNumber;
	unsigned char bAlternateSetting;
	unsigned char bNumEndpoints;
	unsigned char bInterfaceClass;
	unsigned char bInterfaceSubClass;
	unsigned char bInterfaceProtocol;
	unsigned char iInterface;
	struct SceUdcdEndpointDescriptor *endpoints;
	unsigned char *extra;			/* the class descriptors after it */
	int extraLength;
} SceUdcdInterfaceDescriptor;

typedef struct SceUdcdEndpointDescriptor {
	unsigned char bLength;
	unsigned char bDescriptorType;
	unsigned char bEndpointAddress;
	unsigned char bmAttributes;
	unsigned short wMaxPacketSize;
	unsigned char bInterval;
	unsigned char *extra;			/* the class descriptors after it */
	int extraLength;
} SceUdcdEndpointDescriptor;

typedef struct SceUdcdInterface {
	int expectNumber;			/* the interface number expected, or -1 */
	int interfaceNumber;			/* the end interface */
	int numInterfaces;
} SceUdcdInterface;

typedef struct SceUdcdEndpoint {
	int direction;				/* USB_ENDPOINT_IN, USB_ENDPOINT_OUT */
	int driverEndpointNumber;		/* in order, from the driver */
	int endpointNumber;			/* from the bus driver */
	int transmittedBytes;
} SceUdcdEndpoint;

typedef struct SceUdcdConfiguration {
	SceUdcdConfigDescriptor *configDescriptors;
	SceUdcdInterfaceSettings *settings;
	SceUdcdInterfaceDescriptor *interfaceDescriptors;	/* the first */
	SceUdcdEndpointDescriptor *endpointDescriptors;		/* the first */
} SceUdcdConfiguration;

/* a control request's setup packet */
typedef struct SceUdcdEP0DeviceRequest {
	unsigned char bmRequestType;
	unsigned char bRequest;
	unsigned short wValue;
	unsigned short wIndex;
	unsigned short wLength;
} SceUdcdEP0DeviceRequest;

/* a function driver (SceUdcdForDriver_4E55244D) */
typedef struct SceUdcdDriver {
	const char *driverName;
	int numEndpoints;			/* with the default control endpoint */
	SceUdcdEndpoint *endpoints;
	SceUdcdInterface *interface;
	SceUdcdDeviceDescriptor *descriptor_hi;		/* high speed */
	SceUdcdConfiguration *configuration_hi;
	SceUdcdDeviceDescriptor *descriptor;		/* full speed */
	SceUdcdConfiguration *configuration;
	SceUdcdStringDescriptor *stringDescriptors;
	SceUdcdStringDescriptor *stringDescriptorProduct;	/* iProduct 2 */
	SceUdcdStringDescriptor *stringDescriptorSerial;	/* iSerialNumber 3 */
	int (*processRequest)(int recipient, int arg, SceUdcdEP0DeviceRequest *req, void *user_data);
	int (*changeSetting)(int interfaceNumber, int alternateSetting, int bus);
	int (*attach)(int usb_version, void *user_data);
	void (*detach)(void *user_data);
	void (*configure)(int usb_version, int desc_count, SceUdcdInterfaceSettings *settings,
			  void *user_data);
	int (*start)(int size, void *args, void *user_data);
	int (*stop)(int size, void *args, void *user_data);
	void *user_data;
	int bus;
	struct SceUdcdDriver *link;		/* the next driver; NULL */
} SceUdcdDriver;

/* a transfer on an endpoint */
typedef struct SceUdcdDeviceRequest {
	SceUdcdEndpoint *endpoint;
	void *data;
	unsigned int attributes;		/* SCE_UDCD_DEVICE_REQUEST_ATTR_* */
	int size;
	int isControlRequest;
	void (*onComplete)(struct SceUdcdDeviceRequest *req);
	int transmitted;
	int returnCode;				/* SCE_UDCD_RETCODE_* */
	struct SceUdcdDeviceRequest *next;	/* the driver's queue; NULL */
	void *unused;
	void *physicalAddress;
} SceUdcdDeviceRequest;

_Static_assert(sizeof(SceUdcdDeviceInfo) == 0x40, "SceUdcdDeviceInfo");
_Static_assert(sizeof(SceUdcdWaitParam) == 0x18, "SceUdcdWaitParam");
_Static_assert(sizeof(SceUdcdStringDescriptor) == 0x40, "SceUdcdStringDescriptor");
_Static_assert(sizeof(SceUdcdDeviceDescriptor) == 0x14, "SceUdcdDeviceDescriptor");
_Static_assert(sizeof(SceUdcdDeviceQualifierDescriptor) == 0xc, "SceUdcdDeviceQualifierDescriptor");
_Static_assert(sizeof(SceUdcdConfigDescriptor) == 0x18, "SceUdcdConfigDescriptor");
_Static_assert(sizeof(SceUdcdInterfaceSettings) == 0xc, "SceUdcdInterfaceSettings");
_Static_assert(sizeof(SceUdcdInterfaceDescriptor) == 0x18, "SceUdcdInterfaceDescriptor");
_Static_assert(sizeof(SceUdcdEndpointDescriptor) == 0x10, "SceUdcdEndpointDescriptor");
_Static_assert(sizeof(SceUdcdInterface) == 0xc, "SceUdcdInterface");
_Static_assert(sizeof(SceUdcdEndpoint) == 0x10, "SceUdcdEndpoint");
_Static_assert(sizeof(SceUdcdConfiguration) == 0x10, "SceUdcdConfiguration");
_Static_assert(sizeof(SceUdcdEP0DeviceRequest) == 8, "SceUdcdEP0DeviceRequest");
_Static_assert(sizeof(SceUdcdDriver) == 0x54, "SceUdcdDriver");
_Static_assert(sizeof(SceUdcdDeviceRequest) == 0x2c, "SceUdcdDeviceRequest");

int SceUdcdForDriver_03781649(SceUdcdDeviceRequest *request, int bus);
int SceUdcdForDriver_0DECE532(SceUdcdDriver *driver);	/* vitasdk: ksceUdcdUnregister */
int SceUdcdForDriver_0F3595AE(int bus);	/* vitasdk: ksceUdcdStopCurrentInternal */
int SceUdcdForDriver_1494293B(const char *name, int args, void *argp);	/* vitasdk: ksceUdcdStop */
/* vitasdk: ksceUdcdStopInternal */
int SceUdcdForDriver_150FD3BB(const char *name, int args, void *argp, int bus);
/* vitasdk: ksceUdcdReqRecvInternal */
int SceUdcdForDriver_175E6179(SceUdcdDeviceRequest *request, int bus);
int SceUdcdForDriver_1A5A3851(void);
/* vitasdk: ksceUdcdWaitBusInitialized */
int SceUdcdForDriver_1C684884(unsigned int timeout, int bus);
int SceUdcdForDriver_1ED0E89E(SceUdcdDeviceRequest *request);	/* vitasdk: ksceUdcdReqSend */
int SceUdcdForDriver_28BFEC45(void);
int SceUdcdForDriver_2A234CAF(int bus);
int SceUdcdForDriver_2D91F8C3(unsigned int enabled);
/* vitasdk: ksceUdcdReqSendInternal */
int SceUdcdForDriver_2E3E622A(SceUdcdDeviceRequest *request, int bus);
int SceUdcdForDriver_34079250(SceUdcdEndpoint *endpoint);	/* vitasdk: ksceUdcdStall */
int SceUdcdForDriver_360E95B9(int unused, int bus);	/* vitasdk: ksceUdcdStartCurrentInternal */
int SceUdcdForDriver_38787672(SceUdcdEndpoint *endpoint);	/* vitasdk: ksceUdcdReqCancelAll */
int SceUdcdForDriver_409DD196(int bus);	/* vitasdk: ksceUdcdGetDeviceStateInternal */
/* vitasdk: ksceUdcdGetDeviceInfoInternal */
int SceUdcdForDriver_45EB0177(SceUdcdDeviceInfo *information, int bus);
int SceUdcdForDriver_4E55244D(SceUdcdDriver *driver);	/* vitasdk: ksceUdcdRegister */
/* vitasdk: ksceUdcdGetDrvStateInternal */
int SceUdcdForDriver_512F77BC(const char *name, int bus);
/* vitasdk: ksceUdcdRegisterInternal */
int SceUdcdForDriver_64675918(SceUdcdDriver *driver, int bus);
/* vitasdk: ksceUdcdUnregisterInternal */
int SceUdcdForDriver_6CCD152E(SceUdcdDriver *driver, int bus);
int SceUdcdForDriver_78BF2A28(const SceUdcdDeviceInfo *information);
/* vitasdk: ksceUdcdWaitStateInternal */
int SceUdcdForDriver_7AD0C8D1(SceUdcdWaitParam *parameter, unsigned int timeout, int bus);
int SceUdcdForDriver_8AE87657(void);	/* vitasdk: ksceUdcdDeactivate */
/* vitasdk: ksceUdcdStartInternal */
int SceUdcdForDriver_90F8BCAE(const char *name, int args, void *argp, int bus);
/* vitasdk: ksceUdcdActivateInternal */
int SceUdcdForDriver_9119247B(unsigned int productId, unsigned int policy, int bus);
/* vitasdk: ksceUdcdStallInternal */
int SceUdcdForDriver_9B44DF29(SceUdcdEndpoint *endpoint, int bus);
int SceUdcdForDriver_9E7F7EFE(void);
int SceUdcdForDriver_9F53D64D(SceUdcdEndpoint *endpoint);	/* vitasdk: ksceUdcdClearFIFO */
/* vitasdk: ksceUdcdClearFIFOInternal */
int SceUdcdForDriver_A35F3BAF(SceUdcdEndpoint *endpoint, int bus);
int SceUdcdForDriver_AA5FD90E(SceUdcdDeviceRequest *request);
void SceUdcdForDriver_B1454F11(const unsigned int *value);
int SceUdcdForDriver_B1C2EA5A(SceUdcdEndpoint *endpoint, int bus);
int SceUdcdForDriver_C0CA5DDB(const char *name);	/* vitasdk: ksceUdcdGetDrvState */
int SceUdcdForDriver_C60A74B2(SceUdcdDeviceRequest *request);	/* vitasdk: ksceUdcdReqRecv */
int SceUdcdForDriver_D01CFD22(void);
/* vitasdk: ksceUdcdWaitState */
int SceUdcdForDriver_D03017C0(SceUdcdWaitParam *parameter, unsigned int timeout);
int SceUdcdForDriver_DB7EF7AB(int bus);
int SceUdcdForDriver_E054B5E4(void);	/* vitasdk: ksceUdcdGetDeviceState */
int SceUdcdForDriver_E4F32EDA(const SceUdcdDeviceInfo *information, int bus);
int SceUdcdForDriver_E9E75C49(int bus);
int SceUdcdForDriver_EBB1E86B(void);
int SceUdcdForDriver_FBEA3703(SceUdcdDeviceInfo *information);	/* vitasdk: ksceUdcdGetDeviceInfo */
int SceUdcdForDriver_FDC10F93(int bus);	/* vitasdk: ksceUdcdDeactivateInternal */
int SceUdcd_F1A3690B(void);
int SceUdcd_FCD876FB(unsigned int enabled);
int sceUdcdActivate(unsigned int productId);
int sceUdcdGetDrvState(const char *driverName);
int sceUdcdRegisterCallback(SceUID callback, unsigned int mask);
int sceUdcdStart(const char *name, int args, void *argp);
int sceUdcdUnregisterCallback(SceUID callback);
