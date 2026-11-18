/*
 * The MTP device ("DEVICE:"): the interface the responder runs on, USB or the network,
 * opened through SceMtpIf, and the wait for a host to connect.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <udcd/udcd_user.h>
#include "mtp_internal.h"

/* how a host is waited for */
#define MTP_DEV_USB_CONNECT_TIMEOUT	10000000	/* us */
#define MTP_DEV_NET_CONNECT_TIMEOUT	30000000
#define MTP_DEV_NET_WAKE_TIMEOUT	90000000	/* a host woken by wake-on-LAN */
#define MTP_DEV_DISCOVERY_INTERVAL	500000


static struct {
	const void *port;		/* SceMtpIf's port, NULL while stopped */
	int ifType;
	int hasHost;			/* the network host to connect to was given */
	int reserved;
	MtpNetHost host;
	int connectTimeout;		/* us, for the next wait; 0: the default */
	int reserved2;
} s_dev;

void mtpDevInitialize(void)
{
	mtpUtilMemset(&s_dev, 0, sizeof(s_dev));
	netMtpDeviceInitialize();
	if (mtpUtilIsUsbAvailable())
		usbChargeInit();
}

void mtpDevTerminate(void)
{
	if (mtpUtilIsUsbAvailable())
		usbChargeEnd();
	netMtpDeviceTerminate();
	mtpUtilMemset(&s_dev, 0, sizeof(s_dev));
}

int mtpDevStart(int ifType, const MtpNetHost *host)
{
	const void *port;
	int type;
	int ret;

	if (ifType != MTP_IF_TYPE_USB && ifType != MTP_IF_TYPE_NET) {
		MTP_ERR("DEVICE:", 113, "unsupported interface %d\n", ifType);
		return SCE_MTP_ERROR_NOT_SUPPORTED;
	}
	if (ifType == MTP_IF_TYPE_USB && !mtpUtilIsUsbAvailable()) {
		MTP_ERR("DEVICE:", 118, "MTP_IF_TYPE_USB is not supported\n");
		return SCE_MTP_ERROR_NOT_SUPPORTED;
	}
	if (s_dev.ifType == ifType) {
		MTP_ERR("DEVICE:", 130, "device type = %d is already started\n", ifType);
		return SCE_MTP_ERROR_ALREADY;
	}
	if (mtpUtilIsUsbAvailable()) {
		if (ifType == MTP_IF_TYPE_USB)
			usbChargeStop();
		else if (ifType == MTP_IF_TYPE_NET)
			usbChargeStart();
	}
	type = 0;
	if (ifType == MTP_IF_TYPE_USB)
		type = 1;
	else if (ifType == MTP_IF_TYPE_NET)
		type = 2;
	port = MtpIfGetPort(type);
	if (port == NULL) {
		MTP_ERR("DEVICE:", 156, "unable to hook I/F\n");
		return SCE_MTP_ERROR_DEVICE;
	}
	ret = MtpIfStartPort(port);
	if (ret < 0) {
		MTP_ERR("DEVICE:", 163, "unable to connect interface with MTP device=0x%x\n", ret);
		return SCE_MTP_ERROR_DEVICE;
	}
	mtpDriverClearLastError();
	s_dev.port = port;
	s_dev.ifType = ifType;
	if (ifType == MTP_IF_TYPE_NET) {
		s_dev.hasHost = 0;
		mtpUtilMemset(&s_dev.host, 0, sizeof(s_dev.host));
		if (host != NULL) {
			s_dev.hasHost = 1;
			mtpUtilMemcpy(&s_dev.host, host, sizeof(s_dev.host));
		}
	}
	return 0;
}

int mtpDevGetHostInfo(MtpNetHost *host)
{
	if (host == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	if (s_dev.ifType != MTP_IF_TYPE_NET)
		return SCE_MTP_ERROR_NO_HOST;
	if (!s_dev.hasHost)
		return SCE_MTP_ERROR_NO_HOST;
	mtpUtilMemcpy(host, &s_dev.host, sizeof(s_dev.host));
	return 0;
}

int mtpDevGetIfType(void)
{
	return s_dev.ifType;
}

int mtpDevStop(void)
{
	int ret;

	ret = MtpIfStopPort(s_dev.port);
	if (ret < 0) {
		MTP_ERR("DEVICE:", 213, "unable to stop MTP I/F 0x%x\n", ret);
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	if (s_dev.ifType == MTP_IF_TYPE_USB && mtpUtilIsUsbAvailable()) {
		SceUdcd_F1A3690B();
		usbChargeStart();
	}
	s_dev.port = NULL;
	s_dev.ifType = 0;
	return 0;
}

void mtpDevSetConnectTimeout(int timeout)
{
	s_dev.connectTimeout = timeout * 1000;
}

/*
 * Wait for a host: on USB for the MTP driver to be activated, on the network for the host
 * the device was started for to be discovered and connected to. Returns whether one
 * connected; otherwise *reason says why not.
 */
int mtpDevWaitConnect(int *found, int *reason)
{
	SceUInt32 timeout;
	SceUInt32 count;
	int state;
	SceUInt32 num;
	SceUInt32 i;
	int connected = 0;
	int code;
	int ret;
	const MtpNetHost *target;
	SceUdcdWaitParam wait;
	MtpNetHost host;

	*found = 0;
	*reason = 1;
	if (s_dev.ifType == MTP_IF_TYPE_USB) {
		timeout = MTP_DEV_USB_CONNECT_TIMEOUT;
	} else if (s_dev.ifType == MTP_IF_TYPE_NET) {
		timeout = s_dev.connectTimeout;
		if (timeout == 0) {
			if (s_dev.host.type == MTP_NET_HOST_TYPE_NASNE)
				timeout = MTP_DEV_NET_WAKE_TIMEOUT;
			else
				timeout = MTP_DEV_NET_CONNECT_TIMEOUT;
		}
	} else {
		timeout = 0;
	}
	if (s_dev.ifType == MTP_IF_TYPE_USB) {
		if (!mtpUsbMonitorIsCableConnected()) {
			*reason = 15;
		} else {
			mtpUtilMemset(&wait, 0, sizeof(wait));
			wait.status = SCE_UDCD_STATUS_ACTIVATED;
			wait.driverName = "USB_MTP_Driver";
			ret = sceUdcdWaitState(&wait, timeout);
			if (ret < 0) {
				mtpDriverPostError(ret);
				if (!mtpUsbMonitorIsCableConnected())
					*reason = 15;
			} else {
				connected = 1;
			}
		}
	} else if (s_dev.ifType == MTP_IF_TYPE_NET) {
		target = NULL;
		if (!s_dev.hasHost) {
			*reason = 2;
		} else if (!netMtpIsWlanAvailable()) {
			*reason = 3;
		} else {
			count = timeout / MTP_DEV_DISCOVERY_INTERVAL;
			if (s_dev.host.type == MTP_NET_HOST_TYPE_NASNE)
				netMtpSendWakeOnLanPacket(&s_dev.host);
			startHostDiscovery();
			while (count-- != 0 && target == NULL && s_dev.port != NULL) {
				sceKernelDelayThread(MTP_DEV_DISCOVERY_INTERVAL);
				netMtpDeviceGetNumDiscoveredHosts(&num);
				for (i = 0; i < num; i++) {
					netMtpDeviceGetDiscoveredHost(i, &host);
					if (mtpUtilMemcmp(host.id, s_dev.host.id, sizeof(host.id)) == 0) {
						target = &host;
						break;
					}
				}
			}
			netMtpDeviceGetHostDiscoveryState(&state);
			stopHostDiscovery();
			if (state == 2) {
				*reason = 4;
			} else {
				ret = netMtpDeviceConnectToHost(target);
				if (ret < 0) {
					switch (ret) {
					case (int)0x805400D3:
						code = 5;
						break;
					case (int)0x805400D4:
						code = 7;
						break;
					case (int)0x805400DC:
						code = 6;
						break;
					case (int)0x805400E0:
						code = 10;
						break;
					case (int)0x805400E1:
						code = 11;
						break;
					case (int)0x805400E3:
						code = 13;
						break;
					default:
						code = 4;
						break;
					}
					*reason = code;
				} else {
					*found = netMtpDeviceTakeFound();
					connected = 1;
				}
			}
		}
	}
	s_dev.connectTimeout = 0;
	return connected;
}

int mtpDevIfWaitConnect(SceUInt32 timeout)
{
	return MtpIfWaitConnect(s_dev.port, timeout);
}

int mtpDevIfIsConnected(void)
{
	return MtpIfIsConnected(s_dev.port);
}

int mtpDevIfChangePhase(int phase)
{
	return MtpIfChangePhase(s_dev.port, phase);
}

int mtpDevIfRecvCommand(MtpOperation *operation)
{
	return MtpIfRecvCommand(s_dev.port, operation);
}

int mtpDevIfSendResponse(const void *response)
{
	return MtpIfSendResponse(s_dev.port, response);
}

int mtpDevIfSendData(SceUInt32 transactionId, const void *data, SceSize size,
	SceUInt64 totalSize)
{
	return MtpIfSendData(s_dev.port, transactionId, data, size, totalSize);
}

int mtpDevIfRecvData(SceUInt32 transactionId, void *data, SceSize size, SceUInt64 *totalSize)
{
	return MtpIfRecvData(s_dev.port, transactionId, data, size, totalSize);
}

int mtpDevDriverSendData(SceUInt32 transactionId, const void *data, SceSize size,
	SceUInt64 totalSize)
{
	return mtpDriverSendData(s_dev.port, s_dev.ifType, transactionId, data, size, totalSize);
}

int mtpDevDriverRecvData(SceUInt32 transactionId, void *data, SceSize size,
	SceUInt64 *totalSize)
{
	return mtpDriverRecvData(s_dev.port, s_dev.ifType, transactionId, data, size, totalSize);
}

int mtpDevIfSendEvent(const MtpOperation *command, const void *event)
{
	return mtpDriverSendEvent(s_dev.port, command, event);
}

int mtpDevDriverHandleErrorResponse(const MtpOperation *operation, MtpOperation *response,
	SceUInt32 value, int code, int send)
{
	return mtpDriverHandleErrorResponse(s_dev.port, operation, response, value, code, send);
}

int mtpDevIfReset(void)
{
	return MtpIfReset(s_dev.port);
}

int mtpDevStopStartPort(void)
{
	int ret;

	ret = MtpIfStopPort(s_dev.port);
	if (ret < 0)
		MTP_ERR("DEVICE:", 419, "MtpIfStopPort failed: 0x%x\n", ret);
	ret = MtpIfStartPort(s_dev.port);
	if (ret < 0)
		MTP_ERR("DEVICE:", 423, "MtpIfStartPort failed: 0x%x\n", ret);
	return ret;
}

int mtpDevIfCancelTransfer(void)
{
	return MtpIfCancelTransfer(s_dev.port);
}

const void *mtpDevGetPort(void)
{
	return s_dev.port;
}
