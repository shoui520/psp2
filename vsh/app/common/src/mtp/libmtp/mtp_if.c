/*
 * SceMtpIf's operations on the interface in use: USB through SceMtpIf's syscalls, the
 * network through SceLibMtp's own implementation of the same operations.
 */
#include <stddef.h>
#include <scetypes.h>
#include <mtpif/mtpif.h>
#include "mtp_internal.h"

typedef struct MtpIfOps {
	int (*reset)(const void *port);
	int (*recvCommand)(const void *port, void *command);
	int (*sendResponse)(const void *port, const void *response);
	int (*sendDataWithParam)(const void *port, MtpIfTransfer *transfer);
	int (*recvDataWithParam)(const void *port, MtpIfTransfer *transfer);
	int (*sendEvent)(const void *port, const void *command, const void *event);
	int (*cancelTransfer)(const void *port);
	int (*waitConnect)(const void *port, SceUInt32 timeout);
	int (*isConnected)(const void *port);
	int (*changePhase)(const void *port, int phase);
	int (*startPort)(const void *port);
	int (*stopPort)(const void *port);
	const void *(*getPort)(int type);
} MtpIfOps;

static const MtpIfOps s_usbOps = {
	sceMtpIfReset,
	sceMtpIfRecvCommand,
	sceMtpIfSendResponse,
	sceMtpIfSendDataWithParam,
	sceMtpIfRecvDataWithParam,
	sceMtpIfSendEvent,
	sceMtpIfCancelTransfer,
	sceMtpIfWaitConnect,
	sceMtpIfIsConnected,
	sceMtpIfChangePhase,
	sceMtpIfStartPort,
	sceMtpIfStopPort,
	sceMtpIfGetPort,
};

static const MtpIfOps s_netOps = {
	netMtpIfReset,
	netMtpIfRecvCommand,
	netMtpIfSendResponse,
	netMtpIfSendDataWithParam,
	netMtpIfRecvDataWithParam,
	netMtpIfSendEvent,
	netMtpIfCancelTransfer,
	netMtpIfWaitConnect,
	netMtpIfIsConnected,
	netMtpIfChangePhase,
	netMtpIfStartPort,
	netMtpIfStopPort,
	netMtpIfGetPort,
};

/* the operations of the interface the last port was taken from */
static const MtpIfOps *s_ifOps;

int MtpIfReset(const void *port)
{
	if (s_ifOps->reset == NULL)
		return MTP_ERROR_UNAVAILABLE;
	return s_ifOps->reset(port);
}

int MtpIfRecvCommand(const void *port, MtpOperation *operation)
{
	if (s_ifOps->recvCommand == NULL)
		return MTP_ERROR_UNAVAILABLE;
	return s_ifOps->recvCommand(port, operation);
}

int MtpIfSendResponse(const void *port, const void *response)
{
	if (s_ifOps->sendResponse == NULL)
		return MTP_ERROR_UNAVAILABLE;
	return s_ifOps->sendResponse(port, response);
}

int MtpIfSendDataWithParam(const void *port, MtpIfTransfer *transfer)
{
	if (s_ifOps->sendDataWithParam == NULL)
		return MTP_ERROR_UNAVAILABLE;
	return s_ifOps->sendDataWithParam(port, transfer);
}

int MtpIfRecvDataWithParam(const void *port, MtpIfTransfer *transfer)
{
	if (s_ifOps->recvDataWithParam == NULL)
		return MTP_ERROR_UNAVAILABLE;
	return s_ifOps->recvDataWithParam(port, transfer);
}

int MtpIfSendEvent(const void *port, const MtpOperation *command, const void *event)
{
	if (s_ifOps->sendEvent == NULL)
		return MTP_ERROR_UNAVAILABLE;
	return s_ifOps->sendEvent(port, command, event);
}

int MtpIfCancelTransfer(const void *port)
{
	if (s_ifOps->cancelTransfer == NULL)
		return MTP_ERROR_UNAVAILABLE;
	return s_ifOps->cancelTransfer(port);
}

int MtpIfWaitConnect(const void *port, SceUInt32 timeout)
{
	if (s_ifOps->waitConnect == NULL)
		return MTP_ERROR_UNAVAILABLE;
	return s_ifOps->waitConnect(port, timeout);
}

int MtpIfIsConnected(const void *port)
{
	if (s_ifOps->isConnected == NULL)
		return MTP_ERROR_UNAVAILABLE;
	return s_ifOps->isConnected(port);
}

int MtpIfChangePhase(const void *port, int phase)
{
	if (s_ifOps->changePhase == NULL)
		return MTP_ERROR_UNAVAILABLE;
	return s_ifOps->changePhase(port, phase);
}

int MtpIfStartPort(const void *port)
{
	if (s_ifOps->startPort == NULL)
		return MTP_ERROR_UNAVAILABLE;
	return s_ifOps->startPort(port);
}

int MtpIfStopPort(const void *port)
{
	if (s_ifOps->stopPort == NULL)
		return MTP_ERROR_UNAVAILABLE;
	return s_ifOps->stopPort(port);
}

const void *MtpIfGetPort(SceUInt32 type)
{
	const MtpIfOps *ops;

	switch (type) {
	case MTP_IF_PORT_USB:
		ops = &s_usbOps;
		break;
	case MTP_IF_PORT_NET:
		ops = &s_netOps;
		break;
	default:
		return NULL;
	}
	s_ifOps = ops;
	if (ops->getPort == NULL)
		return NULL;
	return ops->getPort(type);
}

int MtpIfStartDriver(SceUInt32 type)
{
	switch (type) {
	case MTP_IF_PORT_USB:
		return sceMtpIfStartDriver(MTP_IF_PORT_USB);
	case MTP_IF_PORT_NET:
		return netMtpIfStartDriver(MTP_IF_PORT_NET);
	}
	return MTP_ERROR_INVALID;
}

int MtpIfStopDriver(SceUInt32 type)
{
	switch (type) {
	case MTP_IF_PORT_USB:
		return sceMtpIfStopDriver(MTP_IF_PORT_USB);
	case MTP_IF_PORT_NET:
		return netMtpIfStopDriver(MTP_IF_PORT_NET);
	}
	return MTP_ERROR_INVALID;
}
