#ifndef BBMC_QMI_DEVICE_H
#define BBMC_QMI_DEVICE_H

#include "qmi_usb.h"
#include <sce/uart_regs.h>

typedef struct QmiUartPort {
	SceInt32 index;
	SceUInt32 interruptNumber;
	volatile struct SceUartRegisters *registers;
	SceUInt32 receiveId;
	unsigned char *receiveBuffer;
	SceSize receiveLength;
	SceUInt32 receiveCount;
	SceUInt32 receiveInterruptCount;
	SceUInt32 receiveFlags;
	SceUInt32 transmitId;
	const unsigned char *transmitBuffer;
	SceSize transmitLength;
	SceUInt32 transmitCount;
	SceUInt32 transmitInterruptCount;
	SceUInt32 transmitFlags;
	SceKernelSpinlock lock;
	SceUID receiveSema;
	SceUID transmitSema;
	SceUInt32 nextTransferId;
	int (*receiveCompleted)(unsigned int requestId, unsigned int count,
				unsigned int flags, void *context);
	void *receiveContext;
	int (*transmitCompleted)(unsigned int requestId, unsigned int count,
				 unsigned int flags, void *context);
	void *transmitContext;
	int keepTransmitControlCleared;
	int enabled;
} QmiUartPort;

_Static_assert(sizeof(QmiUartPort) == 100, "QMI UART port");
_Static_assert(offsetof(QmiUartPort, lock) == 60, "QMI UART lock");
_Static_assert(offsetof(QmiUartPort, nextTransferId) == 72, "QMI UART sequence");
_Static_assert(offsetof(QmiUartPort, enabled) == 96, "QMI UART state");

typedef struct QmiUartConfig {
	SceUInt32 baudRate;
	SceUInt8 parity;
	SceUInt8 dataBits;
	SceUInt8 stopBits;
	SceUInt8 flowControl;
	SceUInt8 receiveFifoTrigger;
	SceUInt8 transmitFifoTrigger;
	SceUInt8 receiveTimeoutControl;
	SceUInt8 keepTransmitControlCleared;
} QmiUartConfig;

_Static_assert(sizeof(QmiUartConfig) == 12, "QMI UART configuration size");

typedef struct QmiDeviceOperations {
	int (*open)(void);
	int (*close)(void);
	int (*activate)(int (*completed)(BbcmdRequest *request, int result),
		BbcmdRequest *request);
	int (*deactivate)(int (*completed)(BbcmdRequest *request),
		BbcmdRequest *request);
	int (*resume)(void *forwardedValue1, void *forwardedValue2);
	int (*suspend)(void *forwardedValue1, void *forwardedValue2);
	int (*send)(const void *data, SceSize length);
} QmiDeviceOperations;

_Static_assert(sizeof(QmiDeviceOperations) == 28, "QMI device operations size");

typedef struct QmiDeviceControl {
	SceUInt32 state;
	SceUID thread;
	SceUID event;
	SceUInt32 channelState[2];
	QmiUartPort *uartPort;
	struct ThreadmgrFastMutex uartFrameMutex;
	SceUInt32 uartTransmitPending;
	SceUInt32 uartReceiveFlags;
	SceInt32 uartReceiveCount;
	SceUInt8 *uartReceiveBuffer;
	SceUInt8 *usbReceiveBuffer;
	SceUInt8 *usbSendBuffer;
	SceUInt8 *usbInterruptBuffer;
	QmiUsbRequest usbReceiveRequest;
	QmiUsbRequest usbSendRequest;
	QmiUsbRequest usbInterruptRequest;
	SceInt32 usbSendResult;
	SceUInt32 pendingUsbReceives;
	SceUInt32 usbConnected;
	SceUInt32 usbControlActive;
	struct ThreadmgrFastMutex usbRequestMutex;
	int (*activateCallback)(BbcmdRequest *request, int result);
	BbcmdRequest *activateRequest;
	int (*deactivateCallback)(BbcmdRequest *request);
	BbcmdRequest *deactivateRequest;
	SceUID uartSuspendSema;
	SceUInt8 *partialReceiveBuffer;
	SceUInt32 partialReceiveLength;
	SceUID usbDisconnectTimer;
} QmiDeviceControl;

_Static_assert(sizeof(QmiDeviceControl) == 288, "QMI device control size");
_Static_assert(offsetof(QmiDeviceControl, uartFrameMutex) == 24,
	"QMI UART frame mutex");
_Static_assert(offsetof(QmiDeviceControl, usbReceiveRequest) == 116,
	"QMI USB receive request");
_Static_assert(offsetof(QmiDeviceControl, usbRequestMutex) == 192,
	"QMI USB request mutex");
_Static_assert(offsetof(QmiDeviceControl, usbDisconnectTimer) == 284,
	"QMI USB disconnect timer");

extern QmiDeviceControl *g_qmiDeviceControl;
extern QmiDeviceControl g_qmiDeviceStorage;
extern SceUInt8 g_qmiUartReceiveBuffer[32];
extern const QmiDeviceOperations g_qmiDeviceOperations[2];

#endif
