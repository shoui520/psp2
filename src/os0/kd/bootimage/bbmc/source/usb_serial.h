#ifndef BBMC_USB_SERIAL_H
#define BBMC_USB_SERIAL_H

#include <stddef.h>
#include <psp2kern/usbd.h>

typedef struct BbmcUsbSerialPipe BbmcUsbSerialPipe;
typedef struct BbmcUsbSerialChannel BbmcUsbSerialChannel;

typedef union BbmcUsbSerialQueueOwner {
	BbmcUsbSerialPipe *controlPipe;
	BbmcUsbSerialChannel *channel;
} BbmcUsbSerialQueueOwner;

typedef struct BbmcUsbSerialBulkTransfer {
	SceSize length;
	void *data;
} BbmcUsbSerialBulkTransfer;

typedef union BbmcUsbSerialTransferArguments {
	BbmcUsbSerialBulkTransfer bulk;
	SceUsbdDeviceRequest control;
} BbmcUsbSerialTransferArguments;

typedef struct BbmcUsbSerialRequest {
	struct BbmcUsbSerialRequest *next;
	SceUInt32 transferType;
	BbmcUsbSerialTransferArguments arguments;
	void *controlData;
	int (*completed)(int forwardedValue, struct BbmcUsbSerialRequest *request,
		int resultOrCount);
	void *context;
	BbmcUsbSerialQueueOwner queueOwner;
} BbmcUsbSerialRequest;

struct BbmcUsbSerialPipe {
	int pipeId;
	SceUInt32 lock;
	BbmcUsbSerialRequest *head;
	BbmcUsbSerialRequest *tail;
};

struct BbmcUsbSerialChannel {
	int kind;
	BbmcUsbSerialPipe pipes[3];
};

typedef struct BbmcUsbSerialEndpoints {
	SceUsbdEndpointDescriptor *interruptEndpoint;
	SceUsbdEndpointDescriptor *bulkIn;
	SceUsbdEndpointDescriptor *bulkOut;
} BbmcUsbSerialEndpoints;

typedef struct BbmcUsbSerialHost {
	int deviceId[4];
	int attached[4];
	SceUID event;
	SceUInt32 lock;
	BbmcUsbSerialPipe controlPipe;
	BbmcUsbSerialEndpoints endpoints[4];
	void (*stateChanged[2])(SceUInt32 events, void *context);
	void *stateContext[2];
} BbmcUsbSerialHost;

_Static_assert(sizeof(BbmcUsbSerialQueueOwner) == 4, "USB serial queue owner");
_Static_assert(sizeof(BbmcUsbSerialPipe) == 16, "USB serial pipe");
_Static_assert(sizeof(BbmcUsbSerialChannel) == 52, "USB serial channel");
_Static_assert(sizeof(BbmcUsbSerialEndpoints) == 12, "USB serial endpoints");
_Static_assert(sizeof(BbmcUsbSerialHost) == 120, "USB serial host");
_Static_assert(offsetof(BbmcUsbSerialHost, stateChanged) == 104, "USB serial state callbacks");

extern BbmcUsbSerialHost *g_usbSerialHost;
extern BbmcUsbSerialHost g_usbSerialHostStorage;
extern BbmcUsbSerialChannel g_usbSerialChannels[2];

typedef struct BbmcUsbExtIfCompletion {
	SceUID semaphore;
	int result;
} BbmcUsbExtIfCompletion;

typedef struct BbmcUsbExtIfControl {
	int restoreMtp;
	int controllerStarted;
	int mode;
	SceUInt32 lock;
	int qcomChannel;
	int atChannel;
} BbmcUsbExtIfControl;

_Static_assert(sizeof(BbmcUsbExtIfControl) == 24, "USB external-interface control");

int bbmcInitializeUsbSerial(SceSize argumentSize, const void *arguments);
int bbmcFinalizeUsbSerial(void);
void bbmcCloseUsbSerialChannels(void);
void bbmcWakeUsbSerial(void);
int bbmcWaitUsbSerialHost(SceUInt32 events, SceUInt8 waitMode, SceUInt *timeout);
int bbmcOpenUsbSerialQcom(int *channel);
int bbmcOpenUsbSerialAt(int *channel);
int bbmcCloseUsbSerialQcom(int channel);
int bbmcCloseUsbSerialAt(int channel);
int bbmcReadUsbSerial(int channel, BbmcUsbSerialRequest *request);
int bbmcWriteUsbSerial(int channel, BbmcUsbSerialRequest *request);
int bbmcControlUsbSerial(int channel, BbmcUsbSerialRequest *request);
int bbmcInterruptUsbSerial(int channel, BbmcUsbSerialRequest *request);

_Static_assert(sizeof(BbmcUsbSerialBulkTransfer) == 8, "USB serial bulk arguments");
_Static_assert(sizeof(BbmcUsbSerialTransferArguments) == 8, "USB serial transfer arguments");
_Static_assert(sizeof(BbmcUsbSerialRequest) == 32, "USB serial request");
_Static_assert(offsetof(BbmcUsbSerialRequest, context) == 24, "USB serial request context");
_Static_assert(sizeof(BbmcUsbExtIfCompletion) == 8, "USB external-interface completion");

#endif
