#ifndef BBMC_QMI_USB_H
#define BBMC_QMI_USB_H

#include "qmi_backend.h"
#include <usbd/usbd.h>

typedef struct QmiUsbStateSubscriber {
	int (*callback)(int state, void *context);
	void *context;
} QmiUsbStateSubscriber;

typedef struct QmiUsbTransport {
	int deviceId;
	int controlPipe;
	int interruptPipe;
	SceUsbdEndpointDescriptor *bulkOut;
	SceUsbdEndpointDescriptor *bulkIn;
	SceUID event;
	struct ThreadmgrFastMutex mutex;
	QmiUsbStateSubscriber subscribers[2];
} QmiUsbTransport;

_Static_assert(sizeof(QmiUsbStateSubscriber) == 8, "USB state subscriber");
_Static_assert(sizeof(QmiUsbTransport) == 104, "QMI USB transport");
_Static_assert(offsetof(QmiUsbTransport, mutex) == 24, "QMI USB mutex");
_Static_assert(offsetof(QmiUsbTransport, subscribers) == 88, "QMI USB subscribers");

typedef struct QmiUsbRequest QmiUsbRequest;

struct QmiUsbRequest {
	SceUInt32 transferType;
	SceUInt8 *data;
	SceSize length;
	int (*callback)(QmiUsbRequest *request, int resultOrCount);
	void *context;
};

_Static_assert(sizeof(QmiUsbRequest) == 20, "QMI USB request size");
_Static_assert(offsetof(QmiUsbRequest, context) == 16,
	"QMI USB request context");

int qmiGetUsbDeviceSpeed(SceUInt32 *speed);

#endif
