/*
 * The network's port: SceMtpIf's operations over the content manager's MTP over IP, each
 * on the one port the network has, passed to the responder.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include "mtp_internal.h"

#define NET_MTP_EVENT_PARAMS_MAX	3
#define NET_MTP_CONNECT_TIMEOUT		999

/* a port of an interface: its type and name */
typedef struct NetMtpPort {
	int type;
	const char *name;
} NetMtpPort;

static const NetMtpPort s_netPort = { MTP_IF_PORT_NET, "TCPIP" };

int netMtpIfReset(const void *port)
{
	int ret = MTP_ERROR_INVALID;

	if (port == &s_netPort)
		ret = netMtpResponderReset();
	return ret;
}

int netMtpIfRecvCommand(const void *port, void *command)
{
	int ret = MTP_ERROR_INVALID;

	if (port == &s_netPort && command != NULL)
		ret = netMtpResponderRecvCommand((MtpOperation *)command);
	return ret;
}

int netMtpIfSendResponse(const void *port, const void *response)
{
	int ret = MTP_ERROR_INVALID;

	if (port == &s_netPort && response != NULL)
		ret = netMtpResponderSendResponse((const MtpOperation *)response);
	return ret;
}

int netMtpIfSendDataWithParam(const void *port, MtpIfTransfer *transfer)
{
	int ret = MTP_ERROR_INVALID;

	if (port == &s_netPort && transfer->buffer != NULL)
		ret = netMtpResponderSendData(transfer);
	return ret;
}

int netMtpIfRecvDataWithParam(const void *port, MtpIfTransfer *transfer)
{
	int ret = MTP_ERROR_INVALID;

	if (port == &s_netPort && transfer->buffer != NULL)
		ret = netMtpResponderRecvData(transfer);
	return ret;
}

/* the event goes out in the command's session and transaction, its unused parameters 0 */
int netMtpIfSendEvent(const void *port, const void *command, const void *event)
{
	const MtpOperation *operation = (const MtpOperation *)command;
	MtpEvent *sent = (MtpEvent *)event;
	int i;

	if (port == &s_netPort && operation != NULL && sent != NULL) {
		if (sent->numParams > NET_MTP_EVENT_PARAMS_MAX)
			return MTP_ERROR_INVALID;
		sent->sessionId = operation->sessionId;
		sent->transactionId = operation->transactionId;
		for (i = sent->numParams; i < NET_MTP_EVENT_PARAMS_MAX; i++)
			sent->param[i] = 0;
		return netMtpResponderSendEvent(sent);
	}
	return MTP_ERROR_INVALID;
}

int netMtpIfCancelTransfer(const void *port)
{
	int ret = MTP_ERROR_INVALID;

	if (port == &s_netPort)
		ret = netMtpResponderCancelTransfer();
	return ret;
}

int netMtpIfWaitConnect(const void *port, SceUInt32 timeout)
{
	int ret = MTP_ERROR_INVALID;

	if (port == &s_netPort)
		ret = netMtpResponderWaitConnect(NET_MTP_CONNECT_TIMEOUT);
	return ret;
}

int netMtpIfIsConnected(const void *port)
{
	int connected;

	if (port != &s_netPort)
		return MTP_ERROR_INVALID;
	connected = 0;
	if (netMtpResponderGetState() & 1)
		connected = 1;
	return connected;
}

int netMtpIfChangePhase(const void *port, int phase)
{
	int ret = MTP_ERROR_INVALID;

	if (port == &s_netPort)
		ret = netMtpResponderSetPhase();
	return ret;
}

const void *netMtpIfGetPort(int type)
{
	if (type != s_netPort.type)
		return NULL;
	return &s_netPort;
}

int netMtpIfStartPort(const void *port)
{
	int ret = MTP_ERROR_INVALID;

	if (port == &s_netPort)
		ret = netMtpResponderStart();
	return ret;
}

int netMtpIfStopPort(const void *port)
{
	int ret = MTP_ERROR_INVALID;

	if (port == &s_netPort)
		ret = netMtpResponderStop();
	return ret;
}

int netMtpIfStartDriver(int type)
{
	int ret = MTP_ERROR_INVALID;

	if (type == MTP_IF_PORT_NET)
		ret = 0;
	return ret;
}

int netMtpIfStopDriver(int type)
{
	int ret = MTP_ERROR_INVALID;

	if (type == MTP_IF_PORT_NET)
		ret = 0;
	return ret;
}
