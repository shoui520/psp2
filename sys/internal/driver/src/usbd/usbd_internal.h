/*
 * SceUsbd: the USB host stack.
 *
 * usbd.c is the bus and driver layer (device enumeration, the LDD registry, the
 * kernel and user transfer API); ehci.c and ohci.c drive the host controllers;
 * hub.c is the external hub class driver. Three buses share one heap: bus n has
 * an EHCI and an OHCI controller, each reached through a UsbdHcd operation table.
 */

#ifndef USBD_INTERNAL_H
#define USBD_INTERNAL_H

#include <stddef.h>
#include <psp2kern/types.h>
#include <psp2kern/usbd.h>
#include <sysmem/procevent.h>

/* Sony's helpers are inline functions; GCC would outline the bigger ones */
#define __always_inline		inline __attribute__((always_inline))

/* doubly linked lists of structures with next and prev links */
#define USBD_LIST_APPEND_LINK(head, tail, item, next, prev) do {		\
	(item)->prev = (tail);						\
	if ((tail) != NULL)						\
		(tail)->next = (item);					\
	else								\
		(head) = (item);					\
	(item)->next = NULL;						\
	(tail) = (item);						\
} while (0)

#define USBD_LIST_REMOVE_LINK(head, tail, item, next, prev) do {		\
	if ((item)->next != NULL)					\
		(item)->next->prev = (item)->prev;			\
	else								\
		(tail) = (item)->prev;					\
	if ((item)->prev != NULL)					\
		(item)->prev->next = (item)->next;			\
	else								\
		(head) = (item)->next;					\
} while (0)

#define USBD_LIST_APPEND(head, tail, item)	USBD_LIST_APPEND_LINK(head, tail, item, next, prev)
#define USBD_LIST_REMOVE(head, tail, item)	USBD_LIST_REMOVE_LINK(head, tail, item, next, prev)

#define USBD_BUS_COUNT			3
#define USBD_DEVICES_PER_BUS		8	/* address 0 is the root hub */
#define USBD_PIPES_PER_BUS		32
#define USBD_REQUEST_COUNT		64
#define USBD_DRIVER_COUNT		16
#define USBD_INTERFACE_COUNT		16

/* device and pipe IDs: bus << 16 | address (or pipe index) */
#define USBD_ID(bus, index)		((bus) << 16 | (index))
#define USBD_ID_BUS(id)			((unsigned char)((id) >> 16))
#define USBD_ID_INDEX(id)		((unsigned char)(id))

/* SceUsbdDeviceDescriptor.bMaxPacketSize0 before the descriptor is read */
#define USBD_DEFAULT_MAX_PACKET_SIZE	8

/* descriptor buffer of each device, and the transfer buffer pools */
#define USBD_DESCRIPTOR_BUFFER_SIZE	4096
#define USBD_SMALL_BUFFER_SIZE		0x1000
#define USBD_SMALL_BUFFER_COUNT		16
#define USBD_LARGE_BUFFER_SIZE		0x10000
#define USBD_LARGE_BUFFER_COUNT		4

#define USBD_RESPONSE_TIMEOUT		3000000	/* usbdWaitDeviceResponse */

/* UsbdDriver.flags */
#define USBD_DRIVER_KERNEL		0x1	/* ksceUsbdRegisterDriver; else a user LDD */
#define USBD_DRIVER_COMPOSITE		0x4	/* attaches to one interface */

/* UsbdEvent.type: the event thread's and the callback thread's queues */
#define USBD_EVENT_ATTACH		1
#define USBD_EVENT_DETACH		2
#define USBD_EVENT_COMPLETE		4
#define USBD_EVENT_QUIT			16

/* UsbdClientEvent.type, as sceUsbdReceiveEvent returns it */
#define USBD_CLIENT_EVENT_ATTACH	1
#define USBD_CLIENT_EVENT_DETACH	2
#define USBD_CLIENT_EVENT_COMPLETE	4
#define USBD_CLIENT_EVENT_WAKEUP	8	/* only wakes the receiver */

/* vendor/product IDs whose descriptors usbdSanityCheck corrects */
#define USBD_VENDOR_SONY		0x054c
#define USBD_PRODUCT_SONY_08A5		0x08a5
#define USBD_PRODUCT_SONY_09CC		0x09cc
#define USBD_VENDOR_0BDA		0x0bda
#define USBD_PRODUCT_0BDA_8152		0x8152

typedef struct UsbdBus UsbdBus;
typedef struct UsbdHcd UsbdHcd;
typedef struct UsbdDevice UsbdDevice;
typedef struct UsbdPipe UsbdPipe;
typedef struct UsbdRequest UsbdRequest;
typedef struct UsbdDriver UsbdDriver;
typedef struct UsbdClient UsbdClient;

/* the packet list sceUsbdIsochTransferData and ksceUsbdIsochronousTransfer take */
typedef struct UsbdIsochPacket {
	SceUInt16 length;
	SceUInt16 status;
} UsbdIsochPacket;

typedef struct UsbdIsochRequest {
	void *buffer;
	int numPackets;			/* 1..8 */
	UsbdIsochPacket packets[8];
} UsbdIsochRequest;

/* a transfer; the 64 of them are found by id from the user API */
struct UsbdRequest {
	UsbdRequest *next;
	UsbdRequest *prev;
	SceUInt8 id;
	SceUInt8 state;
	SceUInt8 isDeviceRequest;	/* UsbdDevice.request: never freed */
	SceUInt8 padding;
	int result;
	int count;
	SceSize length;
	void *buffer;
	void *poolBuffer;		/* from the transfer buffer pools */
	void *userBuffer;
	void *arg;
	UsbdPipe *pipe;
	UsbdClient *client;		/* the user API's requests */
	SceUsbdDeviceRequest *setup;
	int syncCache;			/* the controller cleans and invalidates the buffer; 0: ksceUsbdBulkTransfer2 */
	UsbdIsochRequest isochRequest;
	UsbdIsochRequest isochStatus;
	ksceUsbdDoneCallback done;
	void (*isochDone)(int result, UsbdIsochRequest *status, void *arg);
	void (*complete)(UsbdRequest *request);
};

/* the controllers' own records of a pipe's endpoint and transfers (ehci.c, ohci.c) */
struct EhciDescriptor;
struct OhciDescriptor;

typedef union UsbdHcdDescriptor {
	struct EhciDescriptor *ehci;
	struct OhciDescriptor *ohci;
} UsbdHcdDescriptor;

/* an open endpoint */
struct UsbdPipe {
	UsbdPipe *next;
	UsbdPipe *prev;
	UsbdPipe *waitNext;		/* the controller's pipes waiting for descriptors */
	UsbdPipe *waitPrev;
	SceUInt8 index;
	SceUInt8 busy;			/* a transfer is on the controller */
	SceUInt8 interfaceNumber;
	SceUInt8 type;			/* SCE_USBD_ENDPOINT_TRANSFER_TYPE_* */
	SceUInt8 direction;		/* SCE_USBD_ENDPOINT_DIRECTION_* */
	SceUInt8 waiting;		/* on the wait list: USBD_PIPE_WAIT_* */
	SceUInt16 maxPacketSize;
	int slot;			/* OHCI: the static ED its ED follows */
	int frame;			/* last isochronous frame; -1: start anew */
	int bandwidthFrame;		/* OHCI interrupt: the first frame it is polled in, */
	int bandwidthCount;		/* how many of the 32 frames, */
	int bandwidthPeriod;		/* every how many frames, */
	int bandwidthCost;		/* and its bytes per frame (full speed) */
	UsbdHcdDescriptor endpoint;	/* the controller's queue head / endpoint descriptor */
	UsbdHcdDescriptor dummy;	/* OHCI: the empty TD at the tail of the ED */
	UsbdHcd *hcd;
	UsbdDevice *device;
	UsbdRequest *requestHead;	/* waiting for the controller */
	UsbdRequest *requestTail;
	UsbdHcdDescriptor descHead;	/* the transfer descriptors on the controller */
	UsbdHcdDescriptor descTail;
};

/* UsbdPipe.waiting */
#define USBD_PIPE_WAIT_TRANSFER		1	/* OHCI: control, bulk and interrupt */
#define USBD_PIPE_WAIT_ISOCHRONOUS	2
#define USBD_PIPE_WAIT_DESCRIPTORS	3

/* a device, or the root hub (address 0) */
/* the hub port at each tier, as sceUsbdGetDeviceAddress returns it */
typedef union UsbdRoute {
	SceUInt8 port[6];
	SceUsbdDeviceAddress packed;
} UsbdRoute;

struct UsbdDevice {
	UsbdDevice *next;
	UsbdDevice *prev;
	SceUInt8 index;			/* becomes its address */
	SceUInt8 port;			/* on its parent hub */
	SceUInt8 address;
	SceUInt8 maxPacketSize0;
	SceUInt8 speed;			/* SCE_USBD_DEVICE_SPEED_* */
	SceUInt8 defaultPipeRefs;	/* opens of the default pipe beyond the first */
	SceUInt8 configurationIndex;
	SceUInt8 depth;			/* hub tier; 1: on a root port */
	SceUInt8 reserved;
	UsbdRoute route;
	SceUInt8 padding;
	SceUInt16 vendorId;
	SceUInt16 productId;
	int configuration;		/* -1: not configured */
	int interfaceCount;
	SceUInt8 *descriptor;		/* device descriptor, then the configuration */
	SceUInt8 *descriptorEnd;
	unsigned int reserved1;
	UsbdBus *bus;
	UsbdHcd *hcd;
	UsbdPipe *defaultPipe;
	UsbdRequest request;		/* the enumeration's control transfers */
	UsbdDevice *parent;
	UsbdDriver *driver[USBD_INTERFACE_COUNT];
	UsbdDevice *childHead;
	UsbdDevice *childTail;
	UsbdPipe *pipeHead;
	UsbdPipe *pipeTail;
};

/* one host controller's operations; the controller finds its state by the bus */
struct UsbdHcd {
	UsbdBus *bus;
	int (*getPortCount)(UsbdBus *bus);
	UsbdPipe *(*allocPipe)(UsbdDevice *device, const SceUsbdEndpointDescriptor *endpoint);
	int (*closePipe)(UsbdPipe *pipe);
	int (*submitRequest)(UsbdPipe *pipe, UsbdRequest *request);
	int (*setAddress)(UsbdPipe *pipe, int address);
	int (*setMaxPacketSize)(UsbdPipe *pipe, int size);
	int (*resetFrame)(UsbdPipe *pipe);
	int (*powerOnPort)(UsbdBus *bus, int port);
	int (*powerOffPort)(UsbdBus *bus, int port);
	int (*getSpeed)(UsbdBus *bus);
	int (*getFrameNumber)(UsbdBus *bus, unsigned int *frame, unsigned int *mask);
	int (*resetPort)(UsbdBus *bus, int port);
	int (*suspend)(UsbdBus *bus);
	int (*resume)(UsbdBus *bus);
};

struct UsbdBus {
	SceUInt8 index;
	SceUInt8 suspended;		/* between ksceUsbdSuspendPhase2 and ksceUsbdResume */
	SceUInt8 padding[2];
	SceUInt8 *enumerationBuffer;	/* uncached: the descriptors a device sends */
	SceUInt8 *enumerationCursor;
	UsbdDevice *rootHub;
	UsbdHcd ehci;			/* high speed */
	UsbdHcd ohci;			/* full and low speed */
	UsbdHcd reserved;
	UsbdDevice *device[USBD_DEVICES_PER_BUS];	/* by address */
	UsbdPipe *pipe[USBD_PIPES_PER_BUS];		/* by index */
	UsbdPipe *freePipeHead;
	UsbdPipe *freePipeTail;
	UsbdDevice *freeDeviceHead;
	UsbdDevice *freeDeviceTail;
};

/* a registered LDD */
struct UsbdDriver {
	UsbdDriver *next;
	UsbdDriver *prev;
	SceUInt8 index;
	SceUInt8 padding[3];
	int configuration;
	int interfaceNumber;
	unsigned int flags;		/* USBD_DRIVER_* */
	const char *name;
	SceUsbdDriver driver;
	SceUsbdCompositeDriver compositeDriver;
};

/* sceUsbdInit's caller */
struct UsbdClient {
	UsbdClient *next;
	UsbdClient *prev;
	SceUID pid;			/* also the uid the user API passes */
	SceUID semaId;			/* counts queued events */
	int closing;
	struct UsbdClientEvent *eventHead;
	struct UsbdClientEvent *eventTail;
};

/* sceUsbdReceiveEvent's record */
typedef struct UsbdClientEvent {
	struct UsbdClientEvent *next;
	struct UsbdClientEvent *prev;
	int type;			/* USBD_CLIENT_EVENT_* */
	unsigned int address;
	unsigned int bus;
	unsigned int driverKind;	/* attach: 0 none, 1 driver, 2 composite */
	unsigned int requestId;		/* complete */
} UsbdClientEvent;

/* the event and callback threads' messages */
typedef struct UsbdEvent {
	struct UsbdEvent *next;
	struct UsbdEvent *prev;
	int type;			/* USBD_EVENT_* */
	unsigned int arg0;
	unsigned int arg1;
	unsigned int arg2;
} UsbdEvent;

typedef struct UsbdEventQueue {
	UsbdEvent *head;
	UsbdEvent *tail;
} UsbdEventQueue;

/* everything usbd allocates at start, in one heap block */
typedef struct UsbdPool {
	UsbdBus bus[USBD_BUS_COUNT];
	UsbdDevice device[USBD_BUS_COUNT][USBD_DEVICES_PER_BUS];
	UsbdPipe pipe[USBD_BUS_COUNT][USBD_PIPES_PER_BUS];
	UsbdRequest request[USBD_BUS_COUNT][USBD_REQUEST_COUNT];	/* the first bus's are used */
	UsbdDriver driver[USBD_DRIVER_COUNT];
} UsbdPool;

/* a pool of transfer buffers that user transfers are staged in */
typedef struct UsbdBufferPool {
	void *base;
	void *last;
	void **head;
	void **tail;
} UsbdBufferPool;

/* sceUsbdOpenPipe's argument */
typedef struct UsbdPipeParam {
	unsigned int deviceId;
	unsigned int configurationValue;
	unsigned int interfaceNumber;
	unsigned int alternateSetting;
	unsigned int endpointAddress;
	unsigned int transferType;
} UsbdPipeParam;

/* sceUsbdTransferData's argument */
typedef struct UsbdTransferParam {
	unsigned int pipeId;
	void *buffer;
	SceSize length;
	const SceUsbdDeviceRequest *setup;
	SceSize setupSize;		/* 8 */
} UsbdTransferParam;

/* sceUsbdGetDeviceList's records */
typedef struct UsbdDeviceInfo {
	unsigned int bus;
	unsigned int address;
	unsigned int driverKind;	/* 0 none, 1 driver, 2 composite */
} UsbdDeviceInfo;

/* sceUsbdAttachCompositeLdd's argument */
typedef struct UsbdAttachCompositeParam {
	unsigned int driverId;
	unsigned int bus;
	unsigned int address;
	int interfaceNumber;
	int configuration;
} UsbdAttachCompositeParam;

/* sceUsbdGetTransferStatus / sceUsbdGetIsochTransferStatus's argument */
typedef struct UsbdTransferStatusParam {
	unsigned int requestId;
	unsigned int reserved;
	void *result;
	void *count;
} UsbdTransferStatusParam;

/* the module's state */


/* usbd.c */
void usbdNotifyAttach(unsigned int hubId, unsigned int port, unsigned int speed);
void usbdNotifyDetach(unsigned int hubId, unsigned int port);
int _sceUsbdInit(void);
UsbdPipe *usbdAllocPipe(UsbdDevice *device, const SceUsbdEndpointDescriptor *endpoint);
void usbdFreePipe(UsbdPipe *pipe);
void usbdFreeRequest(UsbdRequest *request);

/* usbd_os.c: memory */
int usbdCreateHeap(void);
int usbdDeleteHeap(void);
void *usbdAlloc(SceSize size);
int usbdFree(void *ptr);
SceUID usbdAllocMemBlock(SceSize size, void **base);
SceUIntPtr usbdVAtoPA(const void *va);
void usbdDelay(unsigned int count);

/* ehci.c */
int ehciInit(UsbdBus *bus);
int ehciTerm(UsbdBus *bus);
int ehciStart(UsbdBus *bus);
int ehciStop(UsbdBus *bus);

/* ohci.c */
int ohciInit(UsbdBus *bus);
int ohciTerm(UsbdBus *bus);
int ohciStart(UsbdBus *bus);
int ohciStop(UsbdBus *bus);

/* hub.c */
int hubResetPort(unsigned int hubId, unsigned int port);
int hubGetPortStatus(unsigned int hubId, unsigned int port, SceUInt8 *speed);
int hubInit(void);

#endif
