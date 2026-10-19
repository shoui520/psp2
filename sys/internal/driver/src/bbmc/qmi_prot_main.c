/* QMI transport protocol control. */
#include <threadmgr/threadmgr.h>
#include <string.h>
#include <sysmem/debug.h>
#include "qmi_device.h"
#include "bbmc_memory.h"
#include "bbmc_control.h"
#include "bbmc_netif.h"
#include "qmi_fifo.h"
#include "qmi_backend.h"

int UsbCtrlOutFinCb(QmiUsbRequest *request, int resultOrCount);
int qmiUsbStateChanged(int state, void *context);
int qmiActivateDeviceUart(int (*completed)(BbcmdRequest *request, int result),
	BbcmdRequest *request);
int qmiDeactivateDeviceUart(int (*completed)(BbcmdRequest *request),
	BbcmdRequest *request);
int qmiResumeDeviceUart(void *forwardedValue1, void *forwardedValue2);
int qmiSuspendDeviceUart(void *forwardedValue1, void *forwardedValue2);
int qmiActivateDeviceUsb(int (*completed)(BbcmdRequest *request, int result),
	BbcmdRequest *request);
int qmiDeactivateDeviceUsb(int (*completed)(BbcmdRequest *request),
	BbcmdRequest *request);
int qmiInitializeDeviceUart(void);
int qmiFinalizeDeviceUart(void);
int qmiInitializeDeviceUsb(void);
int qmiFinalizeDeviceUsb(void);
int qmiSendFramedUartPacket(const void *data, SceSize length);
int qmiSendUsbPacket(const void *data, SceSize length);
void qmiUsbDisconnectTimedOut(void *context);

const QmiDeviceOperations g_qmiDeviceOperations[2] = {
	{
		.open = qmiInitializeDeviceUart,
		.close = qmiFinalizeDeviceUart,
		.activate = qmiActivateDeviceUart,
		.deactivate = qmiDeactivateDeviceUart,
		.resume = qmiResumeDeviceUart,
		.suspend = qmiSuspendDeviceUart,
		.send = qmiSendFramedUartPacket,
	},
	{
		.open = qmiInitializeDeviceUsb,
		.close = qmiFinalizeDeviceUsb,
		.activate = qmiActivateDeviceUsb,
		.deactivate = qmiDeactivateDeviceUsb,
		.resume = NULL,
		.suspend = NULL,
		.send = qmiSendUsbPacket,
	},
};
int qmiUsbReceiveCompleted(QmiUsbRequest *request, int resultOrCount);

extern int bbmcLockFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("lockFastMutex");
extern int bbmcUnlockFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("sceKernelUnlockFastMutex");
extern int bbmcInitializeFastMutex(struct ThreadmgrFastMutex *mutex,
	const char *name, int attr, void *opt)
	__asm__("sceKernelInitializeFastMutex");
extern int bbmcFinalizeFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("sceKernelFinalizeFastMutex");

QmiDeviceControl *g_qmiDeviceControl;
QmiDeviceControl g_qmiDeviceStorage;
SceUInt8 g_qmiUartReceiveBuffer[32];

/* name unknown: clear the outstanding UART transmit marker. */
int qmiUartTransmitCompleted(unsigned int requestId, unsigned int count,
	unsigned int flags, void *context)
{
	QmiDeviceControl *control = context;

	(void)requestId;
	(void)count;
	(void)flags;
	control->uartTransmitPending = 0;
	return 0;
}

/* name unknown: poll the outstanding UART transmit marker. */
unsigned int qmiGetUartTransmitPending(void)
{
	if (g_qmiDeviceControl != NULL)
		return g_qmiDeviceControl->uartTransmitPending;
	return 0;
}

/* name unknown: serialize access to the UART framing state. */
int qmiLockUartFrame(void *context)
{
	QmiDeviceControl *control = context;

	return bbmcLockFastMutex(&control->uartFrameMutex);
}

/* name unknown: release the UART framing state's synchronization lock. */
int qmiUnlockUartFrame(void *context)
{
	QmiDeviceControl *control = context;

	return bbmcUnlockFastMutex(&control->uartFrameMutex);
}

/* name unknown: publish UART receive count and flags to the device worker. */
int qmiUartReceiveCompleted(unsigned int requestId, unsigned int count,
	unsigned int flags, void *context)
{
	QmiDeviceControl *control = context;
	SceUID event = control->event;

	(void)requestId;
	control->uartReceiveCount = count;
	control->uartReceiveFlags = flags;
	sceKernelSetEventFlag(event, 1);
	return 0;
}

/* name unknown: queue receive work and rearm the USB notification request. */
int qmiUsbInterruptCompleted(QmiUsbRequest *request, int resultOrCount)
{
	QmiDeviceControl *control = request->context;

	if (resultOrCount >= 0) {
		if (request->data[1] == 1) {
			bbmcLockFastMutex(&control->usbRequestMutex);
			if (control->usbReceiveRequest.data != NULL) {
				++control->pendingUsbReceives;
			} else {
				control->usbReceiveRequest.context = control;
				control->usbReceiveRequest.callback =
					qmiUsbReceiveCompleted;
				control->usbReceiveRequest.data = control->usbReceiveBuffer;
				control->usbReceiveRequest.length = 2048;
				qmiSubmitUsbControlIn(&control->usbReceiveRequest);
			}
			bbmcUnlockFastMutex(&control->usbRequestMutex);
		}
		qmiSubmitUsbInterrupt(request);
	}
	return 0;
}

/* name unknown: publish receive bytes or retire the completed USB request. */
int qmiUsbReceiveCompleted(QmiUsbRequest *request, int resultOrCount)
{
	QmiDeviceControl *control = request->context;

	if (resultOrCount <= 0) {
		bbmcLockFastMutex(&control->usbRequestMutex);
		memset(request, 0, sizeof(*request));
		bbmcUnlockFastMutex(&control->usbRequestMutex);
	} else {
		sceKernelSetEventFlag(control->event, 2);
	}
	return 0;
}

/* name unknown: send a USB control packet, retrying the two transient errors. */
int qmiSendUsbPacket(const void *data, SceSize length)
{
	QmiDeviceControl *control = g_qmiDeviceControl;
	int attempts;
	int ret;
	int result;

	for (attempts = 4; attempts != 0; --attempts) {
		void *buffer = control->usbSendBuffer;

		control->usbSendResult = 0;
		control->usbSendRequest.data = buffer;
		memcpy(buffer, data, length);
		control = g_qmiDeviceControl;
		control->usbSendRequest.length = length;
		control->usbSendRequest.callback = UsbCtrlOutFinCb;
		control->usbSendRequest.context = control;
		ret = qmiSubmitUsbControlOut(&control->usbSendRequest);
		if (ret < 0)
			break;
		ret = sceKernelWaitEventFlag(g_qmiDeviceControl->event,
			8, 5, NULL, NULL);
		if (ret < 0)
			break;
		control = g_qmiDeviceControl;
		result = control->usbSendResult;
		if (result != (int)0x80240009 && result != (int)0x80240007)
			return result & (result >> 31);
	}
	return ret;
}

/* name unknown: retire USB bindings and release the three transfer buffers. */
int qmiFinalizeDeviceUsb(void)
{
	QmiDeviceControl *control;
	int ret;

	qmiUnregisterUsbStateCallback(qmiUsbStateChanged);
	bbmcFinalizeUsbPacketInterface();
	ret = qmiFinalizeUsbTransport();
	control = g_qmiDeviceControl;
	if (control->usbInterruptBuffer != NULL) {
		bbmcFree(control->usbInterruptBuffer);
		control = g_qmiDeviceControl;
		control->usbInterruptBuffer = NULL;
	}
	if (control->usbSendBuffer != NULL) {
		bbmcFree(control->usbSendBuffer);
		control = g_qmiDeviceControl;
		control->usbSendBuffer = NULL;
	}
	if (control->usbReceiveBuffer != NULL) {
		bbmcFree(control->usbReceiveBuffer);
		g_qmiDeviceControl->usbReceiveBuffer = NULL;
	}
	return ret;
}

/* name unknown: allocate transfer buffers and attach both USB interfaces. */
int qmiInitializeDeviceUsb(void)
{
	QmiDeviceControl *control = g_qmiDeviceControl;
	QmiDeviceControl *current;
	void *buffer;
	int ret;

	control->usbReceiveBuffer = NULL;
	control->usbSendBuffer = NULL;
	control->usbInterruptBuffer = NULL;
	buffer = bbmcAlloc(2048, 64);
	current = g_qmiDeviceControl;
	control->usbReceiveBuffer = buffer;
	control = current;
	if (control->usbReceiveBuffer == NULL)
		goto allocation_failed;
	buffer = bbmcAlloc(2048, 64);
	current = g_qmiDeviceControl;
	control->usbSendBuffer = buffer;
	control = current;
	if (control->usbSendBuffer == NULL)
		goto allocation_failed;
	buffer = bbmcAlloc(64, 64);
	current = g_qmiDeviceControl;
	control->usbInterruptBuffer = buffer;
	control = current;
	if (control->usbInterruptBuffer == NULL)
		goto allocation_failed;
	ret = qmiInitializeUsbTransport();
	if (ret < 0)
		goto failed;
	ret = bbmcInitializeUsbPacketInterface();
	if (ret < 0)
		goto transport;
	ret = qmiRegisterUsbStateCallback(qmiUsbStateChanged, g_qmiDeviceControl);
	if (ret >= 0)
		return ret;
	bbmcFinalizeUsbPacketInterface();
transport:
	qmiFinalizeUsbTransport();
	goto failed;
allocation_failed:
	ret = 0;
failed:
	control = g_qmiDeviceControl;
	if (control->usbInterruptBuffer != NULL) {
		bbmcFree(control->usbInterruptBuffer);
		control = g_qmiDeviceControl;
		control->usbInterruptBuffer = NULL;
	}
	if (control->usbSendBuffer != NULL) {
		bbmcFree(control->usbSendBuffer);
		control = g_qmiDeviceControl;
		control->usbSendBuffer = NULL;
	}
	if (control->usbReceiveBuffer != NULL) {
		bbmcFree(control->usbReceiveBuffer);
		g_qmiDeviceControl->usbReceiveBuffer = NULL;
	}
	return ret;
}

/* name unknown: pass a channel packet to the UART framing queue. */
int qmiSendFramedUartPacket(const void *data, SceSize length)
{
	return qmiQueueUartFrame(data, length);
}

/* name unknown: submit a UART packet and maintain its outstanding marker. */
int qmiSendUartPacket(const void *data, SceSize length)
{
	QmiDeviceControl *control = g_qmiDeviceControl;
	int ret = (int)data;

	if (control != NULL) {
		QmiUartPort *port = control->uartPort;

		ret = (int)port;
		if (port != NULL) {
			control->uartTransmitPending = 1;
			ret = qmiUartTransmit(port, data, length,
				qmiUartTransmitCompleted, control);
			if (ret < 0)
				g_qmiDeviceControl->uartTransmitPending = 0;
		}
	}
	return ret;
}

/* name unknown: shut down the UART driver through the device interface. */
int qmiFinalizeDeviceUart(void)
{
	qmiFinalizeUart();
	return 0;
}

/* name unknown: initialize the UART driver through the device interface. */
int qmiInitializeDeviceUart(void)
{
	return qmiInitializeUart();
}

/* name unknown: finish the delayed USB disconnect and wake the device worker. */
void qmiUsbDisconnectTimedOut(void *context)
{
	QmiDeviceControl *control = context;
	QmiDeviceControl *device = g_qmiDeviceControl;

	control->usbDisconnectTimer = 0;
	control->usbConnected = 0;
	if (device->channelState[1] == 0)
		sceKernelSetEventFlag(control->event, 32);
}

int UsbCtrlOutFinCb(QmiUsbRequest *request, int resultOrCount)
{
	QmiDeviceControl *control = request->context;
	int ret;

	control->usbSendResult = resultOrCount;
	ret = sceKernelSetEventFlag(control->event, 8);
	if (ret < 0)
		SCE_DBG_ASSERT(0, 847);
	return ret;
}

/* name unknown: service transport events, decoded input and the packet FIFO. */
int qmiDeviceThread(SceSize args, void *argp)
{
	QmiDeviceControl *control = *(QmiDeviceControl **)argp;
	unsigned int timeout = 0;
	unsigned int flags;
	unsigned int channel;
	void *packet;
	unsigned int length;

	(void)args;
	while (control->state != 0) {
		unsigned int *waitTimeout = NULL;
		unsigned int ret;

		if (control->uartPort != NULL) {
			if (timeout == 0)
				timeout = 10000;
			waitTimeout = &timeout;
		}
		flags = 0;
		ret = sceKernelWaitEventFlag(control->event, 0x80000037,
			5, &flags, waitTimeout);
		if (ret != 0x80028005 && (int)ret < 0)
			break;
		if (ret == 0x80028005) {
			qmiPumpUartFrames();
			if (timeout != 0)
				timeout = 0;
			if (control->uartSuspendSema > 0 &&
				qmiIsUartFrameQueueEmpty() == 1)
				sceKernelSignalSema(control->uartSuspendSema, 1);
		}
		if ((int)flags < 0)
			break;
		if (flags & 16) {
			QmiDeviceControl *device = g_qmiDeviceControl;
			QmiClientInstance *instance;

			device->pendingUsbReceives = 0;
			memset(device->usbInterruptBuffer, 0, 64);
			memset(&g_qmiDeviceControl->usbInterruptRequest, 0,
				sizeof(QmiUsbRequest));
			device = g_qmiDeviceControl;
			device->usbInterruptRequest.length = 64;
			device->usbInterruptRequest.callback = qmiUsbInterruptCompleted;
			device->usbInterruptRequest.context = device;
			device->usbInterruptRequest.data = device->usbInterruptBuffer;
			if (qmiSubmitUsbInterrupt(&device->usbInterruptRequest) < 0) {
				bbmcReleaseGpio(NULL);
			} else {
				instance = qmiGetClientInstance(11);
				instance->activationCallback =
					&g_qmiDeviceControl->activateCallback;
				instance->activationRequest =
					&g_qmiDeviceControl->activateRequest;
				qmiWaitControlTransaction(instance);
			}
			control->usbControlActive = 1;
		}
		if (flags & 32) {
			if (control->deactivateCallback != NULL) {
				control->deactivateCallback(g_qmiDeviceControl->deactivateRequest);
				control->deactivateCallback = NULL;
				control->deactivateRequest = NULL;
			}
			sceKernelClearEventFlag(control->event, ~2U);
			control->usbControlActive = 0;
			flags &= ~2U;
		}
		if (flags & 1) {
			if (control->uartReceiveCount > 0) {
				for (;;) {
					unsigned int decoded = qmiDecodeUartFrame(
						control->uartReceiveBuffer, control->uartReceiveCount,
						&packet, &length);
					unsigned int chunkLength;
					unsigned int combinedLength;
					unsigned int expectedLength;
					void *chunk;
					unsigned char *buffer;
					const QmiControlWireHeader *wire;

					if (decoded != 0 && decoded != 1)
						break;
					if (packet == NULL || length == 0)
						goto next_frame;
					qmiPumpUartFrames();
					chunkLength = length;
					chunk = packet;
					combinedLength = length;
					if (control->partialReceiveLength + length > 2048)
						goto clear_partial;
					if (control->partialReceiveLength != 0) {
						buffer = bbmcAlloc(control->partialReceiveLength + length, 64);
						if (buffer == NULL)
							goto clear_partial;
						memcpy(buffer, control->partialReceiveBuffer,
							control->partialReceiveLength);
						memcpy(buffer + control->partialReceiveLength, chunk,
							chunkLength);
						bbmcFree(control->partialReceiveBuffer);
						control->partialReceiveBuffer = buffer;
						combinedLength = control->partialReceiveLength + chunkLength;
						control->partialReceiveLength = combinedLength;
						wire = (const void *)buffer;
						expectedLength = wire->lengthLow | (wire->lengthHigh << 8);
						if (expectedLength != combinedLength - 1) {
							if (expectedLength >= combinedLength - 1 && combinedLength == 0)
								goto save_partial;
							goto next_frame;
						}
						if (combinedLength != 0) {
							packet = buffer;
							length = combinedLength;
						}
					} else {
						wire = packet;
						if (wire->marker != 1)
							goto next_frame;
						expectedLength = wire->lengthLow | (wire->lengthHigh << 8);
						if (expectedLength != length - 1) {
							if (expectedLength >= length - 1) {
								buffer = packet;
								goto save_partial;
							}
							goto next_frame;
						}
					}
					qmiReceiveClientPacket(0, packet, length);
					if (control->partialReceiveLength != 0) {
clear_partial:
						bbmcFree(control->partialReceiveBuffer);
						control->partialReceiveBuffer = NULL;
						control->partialReceiveLength = 0;
					}
next_frame:
					if (decoded == 0)
						break;
					continue;
save_partial:
					{
						void *saved = bbmcAlloc(combinedLength, 64);

						if (saved != NULL) {
							memcpy(saved, buffer, combinedLength);
							control->partialReceiveBuffer = saved;
							control->partialReceiveLength = combinedLength;
						}
					}
					goto next_frame;
				}
			}
			control->uartReceiveCount = 0;
			qmiUartReceive(control->uartPort, control->uartReceiveBuffer, 32,
				qmiUartReceiveCompleted, control);
		}
		if (flags & 2) {
			const QmiControlWireHeader *wire;
			SceUInt16 packetLength;

			bbmcLockFastMutex(&control->usbRequestMutex);
			wire = (const void *)control->usbReceiveRequest.data;
			__builtin_memcpy(&packetLength, &wire->lengthLow,
				sizeof(packetLength));
			packetLength += 1;
			qmiReceiveClientPacket(1, wire, packetLength);
			if (control->pendingUsbReceives != 0) {
				qmiSubmitUsbControlIn(&control->usbReceiveRequest);
				--control->pendingUsbReceives;
			} else {
				memset(&control->usbReceiveRequest, 0, sizeof(QmiUsbRequest));
			}
			bbmcUnlockFastMutex(&control->usbRequestMutex);
		}
		if (flags & 4) {
			flags &= ~4U;
			channel = 2;
			packet = NULL;
			length = 0;
			if (qmiClaimQueuedPacket(&channel, &packet, &length) == 0) {
				if (channel <= 1 &&
					g_qmiDeviceOperations[channel].send(packet, length) == 0) {
					qmiReleaseQueuedPacket();
					qmiNotifyTransmittedPacket(channel, packet);
				} else {
					qmiRetryQueuedPacket();
					continue;
				}
			}
		}
		if (qmiIsPacketFifoEmpty() != 1)
			sceKernelSetEventFlag(g_qmiDeviceControl->event, 4);
	}
	return 0;
}

/* name unknown: open UART5 and bind its framing callbacks for activation. */
int qmiActivateDeviceUart(int (*completed)(BbcmdRequest *request, int result),
	BbcmdRequest *request)
{
	QmiUartConfig config;
	int ret;

	(void)completed;
	config.baudRate = request->input[0].value;
	config.keepTransmitControlCleared = request->input[1].value;
	config.parity = 0;
	config.dataBits = 8;
	config.stopBits = 1;
	config.flowControl = 3;
	config.receiveFifoTrigger = 16;
	config.transmitFifoTrigger = 16;
	config.receiveTimeoutControl = 32;
	ret = qmiOpenUart(5, &g_qmiDeviceControl->uartPort, &config);
	if (ret < 0)
		return ret;
	ret = qmiInitializeUartFraming(0, qmiLockUartFrame, qmiUnlockUartFrame,
		g_qmiDeviceControl, qmiSendUartPacket, qmiGetUartTransmitPending);
	if (ret < 0) {
		qmiCloseUart(g_qmiDeviceControl->uartPort);
		g_qmiDeviceControl->uartPort = NULL;
		return (int)0x80330001;
	}
	g_qmiDeviceControl->uartTransmitPending = 0;
	memset(g_qmiDeviceControl->uartReceiveBuffer, 0, 32);
	g_qmiDeviceControl->uartReceiveFlags = 0;
	g_qmiDeviceControl->uartReceiveCount = 0;
	sceKernelSetEventFlag(g_qmiDeviceControl->event, 1);
	return ret;
}

/* name unknown: close the active UART port and discard its framing state. */
int qmiDeactivateDeviceUart(int (*completed)(BbcmdRequest *request),
	BbcmdRequest *request)
{
	(void)completed;
	(void)request;
	qmiCloseUart(g_qmiDeviceControl->uartPort);
	g_qmiDeviceControl->uartPort = NULL;
	g_qmiDeviceControl->uartTransmitPending = 0;
	return qmiFinalizeUartFraming();
}

/* name unknown: drain queued frames before suspending the UART transport. */
int qmiSuspendDeviceUart(void *forwardedValue1, void *forwardedValue2)
{
	int sema;

	(void)forwardedValue1;
	(void)forwardedValue2;
	if (qmiIsUartFrameQueueEmpty() != 1) {
		sema = sceKernelCreateSema("SceQmiUartSuspendSync", 0, 0, 1, NULL);
		if (sema >= 0)
			g_qmiDeviceStorage.uartSuspendSema = sema;
		if (g_qmiDeviceControl->uartSuspendSema > 0) {
			sceKernelWaitSema(g_qmiDeviceControl->uartSuspendSema, 1, NULL);
			sceKernelDeleteSema(g_qmiDeviceControl->uartSuspendSema);
			g_qmiDeviceStorage.uartSuspendSema = -1;
		}
	}
	qmiCloseUart(g_qmiDeviceControl->uartPort);
	g_qmiDeviceControl->uartPort = NULL;
	g_qmiDeviceControl->uartTransmitPending = 0;
	return qmiSuspendUartFraming();
}

/* name unknown: release USB's GPIO claim and finish delayed deactivation. */
int qmiDeactivateDeviceUsb(int (*completed)(BbcmdRequest *request),
	BbcmdRequest *request)
{
	QmiDeviceControl *control = g_qmiDeviceControl;
	int previous;
	int ret;

	control->deactivateCallback = completed;
	control->deactivateRequest = request;
	ret = bbmcReleaseGpio(&previous);
	if (bbmcGetGpioUsers() == 0 && previous != 0) {
		control = g_qmiDeviceControl;
		control->usbDisconnectTimer = bbmcTimerSetHandler(2000000,
			qmiUsbDisconnectTimedOut, g_qmiDeviceControl);
	}
	if (ret >= 0 && bbmcGetGpioUsers() > 0) {
		control = g_qmiDeviceControl;
		if (control->usbConnected != 0) {
			unsigned int channelState = control->channelState[1];

			control->usbConnected = 0;
			if (channelState == 0 && control->usbControlActive != 0)
				sceKernelSetEventFlag(control->event, 32);
			if (control->usbDisconnectTimer > 0) {
				bbmcTimerCancelHandler(control->usbDisconnectTimer);
				control->usbDisconnectTimer = 0;
			}
		}
	}
	return ret;
}

/* name unknown: restore UART settings and resume the existing frame queue. */
int qmiResumeDeviceUart(void *forwardedValue1, void *forwardedValue2)
{
	BbcmdRequest *request = forwardedValue2;
	QmiUartConfig config;
	int ret;

	(void)forwardedValue1;
	config.baudRate = request->input[0].value;
	config.keepTransmitControlCleared = request->input[1].value;
	config.parity = 0;
	config.dataBits = 8;
	config.stopBits = 1;
	config.flowControl = 3;
	config.receiveFifoTrigger = 16;
	config.transmitFifoTrigger = 16;
	config.receiveTimeoutControl = 32;
	ret = qmiOpenUart(5, &g_qmiDeviceControl->uartPort, &config);
	if (ret < 0)
		return ret;
	ret = qmiResumeUartFraming();
	if (ret < 0) {
		qmiCloseUart(g_qmiDeviceControl->uartPort);
		g_qmiDeviceControl->uartPort = NULL;
		return (int)0x80330001;
	}
	g_qmiDeviceControl->uartTransmitPending = 0;
	memset(g_qmiDeviceControl->uartReceiveBuffer, 0, 32);
	g_qmiDeviceControl->uartReceiveFlags = 0;
	g_qmiDeviceControl->uartReceiveCount = 0;
	sceKernelSetEventFlag(g_qmiDeviceControl->event, 1);
	return ret;
}

/* name unknown: publish USB connection changes to the device event worker. */
int qmiUsbStateChanged(int state, void *context)
{
	QmiDeviceControl *control = context;
	unsigned int connected = control->usbConnected;

	if (state == 1) {
		if (connected == 0)
			control->usbConnected = 1;
		if (control->channelState[1] == 1 && control->usbControlActive == 0)
			sceKernelSetEventFlag(control->event, 16);
	} else if (connected != 0) {
		unsigned int channelState = control->channelState[1];

		control->usbConnected = 0;
		if (channelState == 0 && control->usbControlActive != 0)
			sceKernelSetEventFlag(control->event, 32);
		if (control->usbDisconnectTimer > 0) {
			bbmcTimerCancelHandler(control->usbDisconnectTimer);
			control->usbDisconnectTimer = 0;
		}
	}
	return 0;
}

/* name unknown: acquire USB's GPIO claim and start its activation exchange. */
int qmiActivateDeviceUsb(int (*completed)(BbcmdRequest *request, int result),
	BbcmdRequest *request)
{
	QmiDeviceControl *control = g_qmiDeviceControl;
	int previous;
	unsigned int state;
	int ret;

	control->activateCallback = completed;
	control->activateRequest = request;
	ret = bbmcAcquireGpio(&previous);
	if (ret >= 0 && previous != 0) {
		qmiGetUsbState(&state);
		if (state & 2) {
			control = g_qmiDeviceControl;
			if (control->usbConnected == 0)
				control->usbConnected = 1;
			if (control->channelState[1] == 1 && control->usbControlActive == 0)
				sceKernelSetEventFlag(control->event, 16);
		}
	}
	return ret;
}

/* name unknown: create and publish the complete transport device controller. */
int qmiInitializeDeviceDriver(void)
{
	int ret = (int)0x80330001;
	int frameMutexInitialized = 0;

	if (g_qmiDeviceControl == NULL) {
		memset(&g_qmiDeviceStorage, 0, sizeof(g_qmiDeviceStorage));
		g_qmiDeviceStorage.uartReceiveBuffer = g_qmiUartReceiveBuffer;
		g_qmiDeviceStorage.uartSuspendSema = -1;
		ret = bbmcInitializeFastMutex(&g_qmiDeviceStorage.uartFrameMutex,
			"SceBbmcUartFrame", 2, NULL);
		if (ret >= 0) {
			frameMutexInitialized = 1;
			ret = sceKernelCreateEventFlag("SceBbmcDevProto", 4096, 0, NULL);
			if (ret >= 0) {
				g_qmiDeviceStorage.event = ret;
				ret = sceKernelCreateThread("SceBbmcDevProto", qmiDeviceThread,
					96, 0x2000, 0x10000000, 0x80000, NULL);
				if (ret >= 0) {
					g_qmiDeviceStorage.thread = ret;
					ret = bbmcInitializeFastMutex(&g_qmiDeviceStorage.usbRequestMutex,
						"SceBbmcUsbCtrlReqLock", 0, NULL);
					if (ret >= 0) {
						g_qmiDeviceControl = &g_qmiDeviceStorage;
						g_qmiDeviceStorage.channelState[0] = 0;
						g_qmiDeviceStorage.channelState[1] = 0;
						g_qmiDeviceStorage.state = 1;
						sceKernelStartThread(g_qmiDeviceStorage.thread, 4,
							&g_qmiDeviceControl);
						return ret;
					}
				}
			}
		}
	}
	if (g_qmiDeviceStorage.thread > 0)
		sceKernelDeleteThread(g_qmiDeviceStorage.thread);
	if (g_qmiDeviceStorage.event > 0)
		sceKernelDeleteEventFlag(g_qmiDeviceStorage.event);
	if (frameMutexInitialized != 0)
		bbmcFinalizeFastMutex(&g_qmiDeviceStorage.uartFrameMutex);
	return ret;
}

/* name unknown: stop the device worker and release its complete owner state. */
int qmiFinalizeDeviceDriver(void)
{
	QmiDeviceControl *control;
	SceUInt timeout = 1000000;

	if (g_qmiDeviceControl == NULL)
		return (int)0x80330003;
	control = g_qmiDeviceControl;
	control->state = 0;
	if (sceKernelSetEventFlag(control->event, 0x80000000) >= 0)
		sceKernelWaitThreadEnd(g_qmiDeviceControl->thread, NULL, &timeout);
	control = g_qmiDeviceControl;
	if (control->usbDisconnectTimer > 0) {
		bbmcTimerCancelHandler(control->usbDisconnectTimer);
		control = g_qmiDeviceControl;
		control->usbDisconnectTimer = 0;
	}
	sceKernelDeleteThread(control->thread);
	sceKernelDeleteEventFlag(g_qmiDeviceControl->event);
	bbmcFinalizeFastMutex(&g_qmiDeviceStorage.uartFrameMutex);
	if (g_qmiDeviceControl->partialReceiveBuffer != NULL)
		bbmcFree(g_qmiDeviceControl->partialReceiveBuffer);
	bbmcFinalizeFastMutex(&g_qmiDeviceStorage.usbRequestMutex);
	memset(g_qmiDeviceControl, 0, sizeof(QmiDeviceControl));
	g_qmiDeviceControl = NULL;
	return 0;
}

/* name unknown: open one of the two device transport channels. */
int qmiOpenDeviceChannel(unsigned int channel)
{
	if (g_qmiDeviceControl == NULL)
		return (int)0x80330003;
	if (channel > 1)
		return (int)0x80330002;
	return g_qmiDeviceOperations[channel].open();
}

/* name unknown: close one of the two device transport channels. */
int qmiCloseDeviceChannel(unsigned int channel)
{
	if (g_qmiDeviceControl == NULL)
		return (int)0x80330003;
	if (channel > 1)
		return (int)0x80330002;
	return g_qmiDeviceOperations[channel].close();
}

/* name unknown: activate a channel, rolling its state back on failure. */
int qmiActivateDeviceChannel(unsigned int channel,
	int (*completed)(BbcmdRequest *request, int result), BbcmdRequest *request)
{
	int ret;

	if (g_qmiDeviceControl == NULL)
		return (int)0x80330003;
	if (channel > 1)
		return (int)0x80330002;
	if (g_qmiDeviceControl->channelState[channel] & ~2U)
		return (int)0x80330001;
	g_qmiDeviceControl->channelState[channel] = 1;
	ret = g_qmiDeviceOperations[channel].activate(completed, request);
	if (ret < 0)
		g_qmiDeviceControl->channelState[channel] = 0;
	return ret;
}

/* name unknown: deactivate an active or suspended transport channel. */
int qmiDeactivateDeviceChannel(unsigned int channel,
	int (*completed)(BbcmdRequest *request), BbcmdRequest *request)
{
	if (g_qmiDeviceControl == NULL)
		return (int)0x80330003;
	if (channel > 1)
		return (int)0x80330002;
	if (g_qmiDeviceControl->channelState[channel] - 1U > 1)
		return (int)0x80330001;
	g_qmiDeviceControl->channelState[channel] = 0;
	return g_qmiDeviceOperations[channel].deactivate(completed, request);
}

/* name unknown: suspend a channel after its transport callback succeeds. */
int qmiSuspendDeviceChannel(unsigned int channel, void *forwardedValue1,
	void *forwardedValue2)
{
	QmiDeviceControl *control = g_qmiDeviceControl;
	int (*suspend)(void *, void *);
	int ret;

	if (control == NULL)
		return (int)0x80330003;
	if (channel > 1)
		return (int)0x80330002;
	if (control->channelState[channel] != 1)
		return (int)0x80330001;
	suspend = g_qmiDeviceOperations[channel].suspend;
	if (suspend != NULL) {
		ret = suspend(forwardedValue1, forwardedValue2);
		if (ret != 0)
			return ret;
		control = g_qmiDeviceControl;
	}
	control->channelState[channel] = 2;
	return 0;
}

/* name unknown: resume a channel after its transport callback succeeds. */
int qmiResumeDeviceChannel(unsigned int channel, void *forwardedValue1,
	void *forwardedValue2)
{
	QmiDeviceControl *control = g_qmiDeviceControl;
	int (*resume)(void *, void *);
	int ret;

	if (control == NULL)
		return (int)0x80330003;
	if (channel > 1)
		return (int)0x80330002;
	if (control->channelState[channel] != 2)
		return (int)0x80330001;
	resume = g_qmiDeviceOperations[channel].resume;
	if (resume != NULL) {
		ret = resume(forwardedValue1, forwardedValue2);
		if (ret != 0)
			return ret;
		control = g_qmiDeviceControl;
	}
	control->channelState[channel] = 1;
	return 0;
}

/* name unknown: signal queued transport output to the device worker. */
int qmiWakePacketTransmitter(void)
{
	return sceKernelSetEventFlag(g_qmiDeviceControl->event, 4);
}
