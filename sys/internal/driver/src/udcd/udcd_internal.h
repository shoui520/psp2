#ifndef UDCD_INTERNAL_H
#define UDCD_INTERNAL_H

#include <stddef.h>
#include "udcd_status.h"
#include <lowio/pervasive_regs.h>
#include <udcd/udcd.h>
#include <sysmem/cpu.h>
#include <sysmem/procevent.h>

/* SceUdcd codes after vitasdk's SCE_UDCD_ERROR_* (names unknown) */
#define UDCD_ERROR_INVALID_ARGUMENT	0x8024300a
#define UDCD_ERROR_TIMEOUT		0x8024300c	/* a state wait or the controller timed out */
#define UDCD_ERROR_INTERRUPTED		0x8024300d	/* the wait was cancelled or the thread stopped */
#define UDCD_ERROR_SUSPENDED		0x8024300e	/* the system is suspended */
#define UDCD_ERROR_TOO_MANY_INTERFACES	0x80243203
#define UDCD_ERROR_DRIVER_NOT_REGISTERED	0x80243204
#define UDCD_ERROR_NO_THREAD		0x80243fff

/* udcdBusState.controllerDriverState: "USBDeviceControllerDriver" */
#define UDCD_CONTROLLER_DRIVER_STOPPED	0
#define UDCD_CONTROLLER_DRIVER_STARTED	1
#define UDCD_CONTROLLER_DRIVER_ABSENT	2	/* before module_start finishes */

/* udcdBusState.controllerState (state event bits) */
#define UDCD_CONTROLLER_STOPPED		SCE_UDCD_STATUS_CONTROLLER_STOPPED
#define UDCD_CONTROLLER_STARTED		SCE_UDCD_STATUS_CONTROLLER_STARTED

/* udcdBusState.connectionState: after SET_ADDRESS, before SET_CONFIGURATION */
#define UDCD_CONNECTION_ADDRESSED	0x0008

/* udcdBusState.suspendState: the bus suspend seen by the controller */
#define UDCD_BUS_RESUMED		0x10
#define UDCD_BUS_SUSPENDED		0x20

/* the state event bit raised while waitError is set */
#define UDCD_STATE_ERROR		0x80000000

struct __attribute__((packed)) udcdWireConfiguration {
	unsigned char bLength, bDescriptorType;
	unsigned short wTotalLength;
	unsigned char bNumInterfaces, bConfigurationValue, iConfiguration;
	unsigned char bmAttributes, bMaxPower;
};

struct __attribute__((packed)) udcdWireInterface {
	unsigned char bLength, bDescriptorType, bInterfaceNumber;
	unsigned char bAlternateSetting, bNumEndpoints, bInterfaceClass;
	unsigned char bInterfaceSubClass, bInterfaceProtocol, iInterface;
};

struct __attribute__((packed)) udcdWireEndpoint {
	unsigned char bLength, bDescriptorType, bEndpointAddress, bmAttributes;
	unsigned short wMaxPacketSize;
	unsigned char bInterval;
};

_Static_assert(sizeof(struct udcdWireConfiguration) == 9, "USB configuration");
_Static_assert(sizeof(struct udcdWireInterface) == 9, "USB interface");
_Static_assert(sizeof(struct udcdWireEndpoint) == 7, "USB endpoint");

struct udcdEndpointState {
	SceUdcdEndpoint *endpoint;
	unsigned int type;			/* bmAttributes & 3 (1: isochronous) */
	unsigned int direction;			/* bEndpointAddress & 0x80 */
	unsigned int maxPacketSize;
	unsigned int transferState;		/* cleared on configure, never read */
	SceUdcdDeviceRequest *requestHead;
	SceUdcdDeviceRequest *requestTail;
};

struct udcdConfigurationAllocation {
	SceUdcdConfiguration configuration;
	SceUdcdConfigDescriptor descriptor;
};

_Static_assert(sizeof(struct udcdConfigurationAllocation) == 0x28,
	       "configuration allocation header");

/*
 * SceUdcdForDriver_FBEA3703's record. The SDK reserves 64 bytes; the driver
 * fills only this pair.
 */
struct udcdDeviceInformation {
	unsigned int lastEvent;			/* what happened last (1: cable connected, ...) */
	unsigned int eventDetail;
};

struct udcdBusState {
	SceUdcdDriver *drivers;
	struct udcdEndpointState endpoints[8];
	unsigned int controllerDriverState;	/* UDCD_CONTROLLER_DRIVER_* */
	unsigned int usbVersion;
	unsigned int suspendState;		/* UDCD_BUS_RESUMED/SUSPENDED */
	unsigned int controllerResumed;
	unsigned int controllerState;		/* UDCD_CONTROLLER_STOPPED/STARTED */
	unsigned int resumeControllerState;	/* restart the controller on resume */
	unsigned int activationState;		/* SCE_UDCD_STATUS_DEACTIVATED... */
	unsigned int cableState;		/* SCE_UDCD_STATUS_CABLE_* */
	unsigned int connectionState;		/* SCE_UDCD_STATUS_CONNECTION_* */
	unsigned int waitError;				/* returned by SceUdcdForDriver_D03017C0 */
	unsigned int reserved;
	unsigned int interfaceCount;
	unsigned int endpointCount;		/* endpoint 0 included */
	SceUdcdDeviceDescriptor *highSpeedDevice;
	SceUdcdConfiguration *highSpeedConfiguration;
	SceUdcdDeviceDescriptor *fullSpeedDevice;
	SceUdcdConfiguration *fullSpeedConfiguration;
	SceUdcdStringDescriptor *stringDescriptors;
	SceUdcdDeviceQualifierDescriptor *highSpeedQualifier;
	SceUdcdDeviceQualifierDescriptor *fullSpeedQualifier;
	void *highSpeedDescriptorData;
	void *fullSpeedDescriptorData;
	SceUdcdStringDescriptor *languageDescriptor;
	SceUdcdStringDescriptor *builtInStringDescriptors[3];
	unsigned char reserved0[0x3f4];
	unsigned int stringDescriptorCount;
	SceUdcdEP0DeviceRequest controlRequest;
	SceUdcdEndpoint controlEndpoint;
	SceUdcdDeviceRequest controlTransfer;
	unsigned char reserved1[0x14];
	SceUID stateEvent;
	SceUID phyReadyEvent;
	SceUID deviceMemBlock;
	SceUID fullSpeedConfigMemBlock;
	SceUID highSpeedConfigMemBlock;
	SceUID descriptorMemBlock;
	SceKernelSpinlock lock;
	int (*startController)(int args, void *argp);
	int (*stopController)(int args, void *argp);
	unsigned int activationEnabled;
	unsigned int systemSuspended;
	unsigned int requestBlocked;
	unsigned int busPowered;		/* SceUdcdForDriver_9119247B bus_powered */
	unsigned int chargingEnabled;
	unsigned int driverSetting;		/* SceUdcdForDriver_B1454F11, never read */
	unsigned int hardwareInfo;		/* SceSysconForDriver_CBD6D8BC */
	SceUID mtpProcess;
	SceUID procEventHandler;
	SceUID alarmTickTimer;
	struct udcdDeviceInformation deviceInformation;
	unsigned int pendingEventDetail;	/* eventDetail after the next deactivate */
	SceUID callbackEvent;
	SceUID callback;
	SceUID callbackProcess;			/* -1: none */
	SceUID callbackThread;			/* -1: none */
	unsigned int callbackMask;
	unsigned int notifiedMask;
};

/* Fields are named as each controller operation is converted from the listing. */
struct udcdControllerOperations {
	int (*resume)(int bus);
	int (*suspend)(int bus);
	int (*initializeQueues)(int bus);
	int (*clearQueues)(int bus);
	int (*start)(int bus, SceKernelIntrStatus *status);
	int (*stop)(int bus, SceKernelIntrStatus *status);
	int (*processRequest)(SceUdcdEP0DeviceRequest *request,
			      SceKernelIntrStatus *status, int bus);
	int (*configureEndpoint)(const SceUdcdEndpointDescriptor *descriptor, int bus);
	void (*resetEndpoint)(int endpoint, int bus);
	int (*abortEndpoint)(int endpoint, int bus);
	int (*stallEndpoint)(int endpoint, int bus);
	void (*submitRequest)(int endpoint, SceKernelIntrStatus *status, int bus);
	void (*completeSend)(int endpoint, SceKernelIntrStatus *status, int bus);
	void (*receiveRequest)(int endpoint, SceKernelIntrStatus *status, int bus);
	void (*completeReceive)(int endpoint, SceKernelIntrStatus *status, int bus);
	int (*cancelRequest)(int endpoint, int result, int bus);
	int (*isEndpointStalled)(int endpoint, int bus);
};

/*
 * A DMA descriptor of an endpoint (in the controller's DMA block). An
 * endpoint's chain is two long: the data, then a zero-length packet.
 */
struct udcdDmaDescriptor {
	unsigned int control;			/* bit 18: short packet ends the transfer */
	void *buffer;
	unsigned int size;
	uintptr_t next;				/* physical, -1: last */
};

struct udcdControllerEndpoint {
	struct udcdEndpointState *queue;
	unsigned int reserved0;
	unsigned char flags;			/* bit 0: busy, bit 7: stalled */
	unsigned char controlFlags;		/* bits 0/1: cancel one/all, bit 5: DMA running */
	unsigned char attributes;		/* bit 1: zero-length packet due, bit 2: cancelled */
	unsigned char spareFlags;		/* cleared with the others, never read */
	unsigned int reserved1;
	struct udcdDmaDescriptor *descriptor;
	uintptr_t dmaAddress;			/* of the descriptor, or an error */
	unsigned int reserved2;
};

/* the controller's registers */
struct udcdControllerEndpointRegisters {
	unsigned int config;
	unsigned int status;
	unsigned int count;
	union {					/* endpoint 0 */
		unsigned int word;
		unsigned char byte;		/* the odd last byte */
	} fifo;
};

struct udcdControllerDmaRegisters {
	unsigned int maxPacket;
	unsigned int status;			/* (endpoints 1-) bit 0: busy */
	unsigned int control;
	unsigned int interrupt;			/* events, write 1 to clear */
	unsigned int endpointConfig;		/* 0x10000 once configured */
	unsigned int reserved0;
	uintptr_t descriptor;
	unsigned int reserved1[3];
	unsigned int received;
	unsigned int reserved2[(0x100 - 0x2c) / 4];
};

struct udcdControllerRegisters {
	unsigned int reserved0;
	unsigned int dmaInterrupt;		/* bit n: endpoint n's DMA has events */
	unsigned int reserved1[2];
	unsigned int interruptMask;		/* all but bit 0 once powered */
	unsigned int interruptEnable;
	unsigned int reserved2[3];
	unsigned int events;			/* write 1 to clear (bits 16-21) */
	unsigned int eventEnable;		/* the events bits 19-21 */
	unsigned int initControl[2];		/* cleared before the bus initializes */
	unsigned int powerControl;		/* 2: suspended, 4: resumed */
	unsigned int interruptStatus;		/* bits 0-1: setup, bit 2: ... */
	unsigned int reserved3[(0x200 - 0x3c) / 4];
	unsigned int address;			/* bits 0-6; 0x200: addressed, 0x400: configured */
	unsigned int reserved4;
	unsigned int testMode;
	unsigned int command;			/* endpoint << 4 | command */
	unsigned int setup[4];			/* the SETUP packet: type | request << 8, value, index, length */
	unsigned int interrupt;			/* config | the endpoint 0 events to clear */
	unsigned int reserved5;
	unsigned int endpointInterruptMask;
	unsigned int reserved6;
	struct udcdControllerEndpointRegisters endpoints[29];
	struct udcdControllerDmaRegisters dma[16];
};

struct udcdControllerState {
	struct udcdControllerEndpoint endpoints[9];
	const unsigned char *controlData;	/* endpoint 0 data still to send */
	int controlRemaining;
	unsigned int savedConnectionState;		/* while suspended */
	unsigned int address;			/* SET_ADDRESS */
	unsigned int config;
	unsigned int remoteWakeup;		/* SET_FEATURE DEVICE_REMOTE_WAKEUP */
	volatile struct udcdControllerRegisters *registers;
	SceUID registerBlock;
	SceUID dmaBlock;
	unsigned int board;			/* 0, 1, 2: the charging GPIO wiring */
	SceUID cookieTimer;
	unsigned int reserved;
};

struct udcdState {
	struct udcdBusState buses[3];
	struct udcdControllerOperations operations[3];
	struct udcdControllerState controllers[3];
	volatile ScePervasive2Registers *registers;
	SceUID phyRegisterBlock;
#if FW_VERSION >= 0x04000000
	int processEventResult;
	SceUdcdStringDescriptor languageDescriptors[3];
	unsigned char controlResponse[2];
	unsigned char padding0[6];
	SceUdcdDriver *registeredDrivers[3];
	unsigned char padding1[4];
#else
	SceUdcdStringDescriptor languageDescriptors[3];
	unsigned char padding0[4];
	unsigned char controlResponse[2];
	unsigned char padding1[6];
	int processEventResult;
	unsigned int padding2;
	SceUdcdDriver *registeredDrivers[3];
	unsigned char padding3[4];
#endif
};

_Static_assert(sizeof(struct udcdEndpointState) == 0x1c, "endpoint state");
_Static_assert(sizeof(struct udcdControllerEndpoint) == 0x1c, "controller endpoint");
_Static_assert(sizeof(struct udcdControllerState) == 0x12c, "controller state");
_Static_assert(offsetof(struct udcdControllerState, registers) == 0x114, "controller registers");
_Static_assert(offsetof(struct udcdControllerRegisters, command) == 0x20c, "command register");
_Static_assert(offsetof(struct udcdControllerRegisters, endpoints) == 0x230, "endpoint registers");
_Static_assert(offsetof(struct udcdControllerRegisters, dma) == 0x400, "DMA registers");
_Static_assert(sizeof(struct udcdBusState) == 0x60c, "bus state");
_Static_assert(offsetof(struct udcdBusState, highSpeedDevice) == 0x118,
	       "high-speed device");
_Static_assert(offsetof(struct udcdBusState, descriptorMemBlock) == 0x5b0,
	       "descriptor allocation");
_Static_assert(offsetof(struct udcdState, operations) == 0x1224, "operations");
_Static_assert(offsetof(struct udcdState, registers) == 0x1674, "registers");
_Static_assert(offsetof(struct udcdState, controlResponse) == 0x1740,
	       "control response");
#if FW_VERSION >= 0x04000000
_Static_assert(offsetof(struct udcdState, registeredDrivers) == 0x1748,
	       "debug registered driver lists");
_Static_assert(sizeof(struct udcdState) == 0x1758, "debug state");
#else
_Static_assert(offsetof(struct udcdState, registeredDrivers) == 0x1750,
	       "release registered driver lists");
_Static_assert(sizeof(struct udcdState) == 0x1760, "release state");
#endif

extern struct udcdState udcdState;

/* udcd.c */
void udcdCompleteRequest(struct udcdEndpointState *endpoint, int result,
			 SceKernelIntrStatus *status, int bus);
int udcdCancelRequest(SceUdcdDeviceRequest *request, SceKernelIntrStatus *status, int bus);
int udcdCancelEndpointRequests(int number, SceKernelIntrStatus *status, int bus);
void udcdDetachDrivers(SceKernelIntrStatus *intrStatus, int bus);
void udcdUpdateStateEvent(int bus);
int udcdIsDriverBus(int bus);
int udcdPollBusInitialized(unsigned int delay, int bus);
int udcdGetStatusRequest(SceKernelIntrStatus *status, int bus);
int udcdGetDescriptorRequest(SceKernelIntrStatus *status, int bus);
int udcdGetConfigurationRequest(SceKernelIntrStatus *status, int bus);
int udcdGetInterfaceRequest(const SceUdcdConfigDescriptor *config, SceKernelIntrStatus *status, int bus);
int udcdSetInterfaceSetting(unsigned int number, int alternate, SceUdcdConfigDescriptor *config,
			    SceKernelIntrStatus *status, int bus);
int udcdInitializeInterfaceSettings(SceUdcdConfigDescriptor *config, SceKernelIntrStatus *status, int bus);
void udcdNotifyControlRequest(SceKernelIntrStatus *status, int bus);
int udcdDispatchControlRequest(SceKernelIntrStatus *status, int bus);
void udcdAttachDrivers(SceKernelIntrStatus *status, int bus);
void udcdUpdateUsbCharging(int bus);

int udcdInitializeDriverList(int bus);
int udcdInitializeBusOne(int args, void *argp);
int udcdInitializeBusTwo(int args, void *argp);
int udcdCallBusOneOperation(int args, void *argp);
int udcdCallBusTwoOperation(int args, void *argp);
int udcdProcessEvent(SceUID process, int event, ProcEventParam *param, int flags);

/* udcd_user.c */
int udcdRegisterCallback(SceUID callback, SceUID process, unsigned int mask);
int udcdUnregisterCallback(SceUID process, SceUID thread);

/* udcd_udc2.c */
int udcdCompleteDma(struct udcdControllerEndpoint *endpoint, SceKernelIntrStatus *status, int bus);
void udcdControllerDisconnect(int bus, SceKernelIntrStatus *status);
int udcdSuspendController(int bus);
int udcdStopController(int bus, SceKernelIntrStatus *status);
int udcdSetupDma(int number, int bus);
void udcdSubmitDma(int number, SceKernelIntrStatus *status, int bus);
void udcdResetEndpoint(int number, int bus);
SceUInt32 udcdCookieAlarm(void *argp);
int udcdAbortEndpoint(int number, int bus);
int udcdConfigureEndpoint(const SceUdcdEndpointDescriptor *descriptor, int bus);
int udcdDetectCookie(int wake);
void udcdVbusConnected(int bus, SceKernelIntrStatus *status);
int udcdVbusInterrupt(int intr, void *argp);
int udcdResumeController(int bus);
int udcdStallEndpoint(int number, int bus);
int udcdDmaInterrupt(int intr, void *argp);
int udcdStandardRequest(SceUdcdEP0DeviceRequest *request, SceKernelIntrStatus *status, int bus);
int udcdStartController(int bus, SceKernelIntrStatus *status);
void udcdSendRequest(int number, SceKernelIntrStatus *status, int bus);
int udcdControllerInterrupt(int intr, void *argp);
void udcdSuspendAll(void);
int udcdSuspendCharging(void);
void udcdResumeAll(void);
int udcdInitializeControllers(void);
int udcdPhyInterrupt(int interrupt, void *argp);
int udcdInitializeControllerQueues(int bus);
int udcdClearControllerQueues(int bus);
void udcdFinishControllerSend(int number, SceKernelIntrStatus *status, int bus);
void udcdFinishControllerReceive(int number, SceKernelIntrStatus *status, int bus);
int udcdCancelControllerRequest(int number, int result, int bus);
int udcdIsControllerEndpointStalled(int number, int bus);

#endif
