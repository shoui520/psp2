/*
 * SceNetCtl UPnP client: maps a port on the Internet Gateway Device of the
 * current network (AddPortMapping / DeletePortMapping) and reports the
 * external IP address.
 *
 * The gateway is found with an SSDP M-SEARCH on 239.255.255.250:1900, its
 * device description gives the control URL of the WANIPConnection or
 * WANPPPConnection service, and the actions are SOAP requests whose answers
 * are checked element by element against the schema tables below.
 */

#include <psp2/net/netctl.h>
#include "netctl_internal.h"
#include "upnp.h"

int SceNetCtl_C33D7374(int code, SceNetCtlInfo *info);
int SceNetCtl_E8E8DF18(int *state);
int SceNetCtl_68C081D1(NetCtlUpnp *upnp);

#define NETCTL_INET_STATE_IPOBTAINED	4	/* state reported by SceNetCtl_E8E8DF18 */

#define SSDP_ADDRESS		"239.255.255.250"
#define SSDP_PORT		1900
#define SSDP_TRIES		5
#define SSDP_WAIT		1000000		/* us per try */
#define SSDP_DEVICE_TYPE	"urn:schemas-upnp-org:device:InternetGatewayDevice:1"

const char *const g_upnpActionNames[] = {
	"AddPortMapping",
	"DeletePortMapping",
	"GetExternalIPAddress",
	"GetStatusInfo",
	NULL,
};

const char *const g_upnpServiceTypes[] = {
	"urn:schemas-upnp-org:service:WANIPConnection:1",
	"urn:schemas-upnp-org:service:WANPPPConnection:1",
};

static const char *const g_upnpResponseNames[] = {
	"AddPortMappingResponse",
	"DeletePortMappingResponse",
	"GetExternalIPAddressResponse",
	"GetStatusInfoResponse",
	NULL,
};

static const char *const g_upnpProtocolNames[] = { "UDP", "TCP" };

/* SOAP response schema */
static const UpnpXmlAttribute g_soapEnvelopeAttributes[] = {
	{ UPNP_XML_ATTR_NAME_PREFIX, "xmlns", UPNP_XML_VALUE_EXACT,
	  "http://schemas.xmlsoap.org/soap/envelope/" },
	{ UPNP_XML_ATTR_PREFIX_NAME, "encodingStyle", UPNP_XML_VALUE_EXACT,
	  "http://schemas.xmlsoap.org/soap/encoding/" },
	{ UPNP_XML_ATTR_NAME, "", UPNP_XML_VALUE_EXACT, "" },
};

static const UpnpXmlAttribute g_soapResponseAttributes[] = {
	{ UPNP_XML_ATTR_NAME_PREFIX, "xmlns", UPNP_XML_VALUE_SERVICE, "" },
	{ UPNP_XML_ATTR_NAME, "", UPNP_XML_VALUE_EXACT, "" },
};

static const UpnpXmlAttribute g_soapUpnpErrorAttributes[] = {
	{ UPNP_XML_ATTR_NAME, "xmlns", UPNP_XML_VALUE_EXACT, "urn:schemas-upnp-org:control-1-0" },
	{ UPNP_XML_ATTR_NAME, "", UPNP_XML_VALUE_EXACT, "" },
};

static const UpnpXmlElement g_soapLevel0[] = {
	{ "Envelope", 0, 1 },
	{ "", 1, 0 },
};

static const UpnpXmlElement g_soapLevel1[] = {
	{ "Body", 0, 1 },
	{ "", 1, 0 },
};

static const UpnpXmlElement g_soapLevel2[] = {
	{ "AddPortMappingResponse", 0, 1 },
	{ "DeletePortMappingResponse", 0, 1 },
	{ "GetExternalIPAddressResponse", 0, 1 },
	{ "GetStatusInfoResponse", 0, 1 },
	{ "Fault", 0, 1 },
	{ "", 1, 0 },
};

static const UpnpXmlElement g_soapLevel3[] = {
	{ "NewConnectionStatus", 1, 0 },
	{ "NewExternalIPAddress", 1, 0 },
	{ "faultcode", 1, 0 },
	{ "faultstring", 1, 0 },
	{ "detail", 0, 0 },
	{ "", 1, 0 },
};

static const UpnpXmlElement g_soapLevel4[] = {
	{ "UPnPError", 0, 0 },
	{ "", 1, 0 },
};

static const UpnpXmlElement g_soapLevel5[] = {
	{ "errorCode", 1, 0 },
	{ "", 1, 0 },
};

static const UpnpXmlElement *const g_soapLevels[] = {
	g_soapLevel0, g_soapLevel1, g_soapLevel2, g_soapLevel3, g_soapLevel4, g_soapLevel5,
};

static const UpnpXmlSchema g_soapSchema = { g_soapLevels, 6, 0 };

/* element indices of the SOAP schema */
#define SOAP_LEVEL3_EXTERNAL_IP		1
#define SOAP_LEVEL3_FAULT_CODE		2
#define SOAP_LEVEL3_FAULT_STRING	3
#define SOAP_LEVEL3_DETAIL		4
#define SOAP_LEVEL2_FAULT		4

/* device description schema */
static const UpnpXmlElement g_deviceLevel0[] = {
	{ "root", 0, 0 },
	{ "", 1, 0 },
};

static const UpnpXmlElement g_deviceLevel1[] = {
	{ "URLBase", 1, 0 },
	{ "device", 0, 0 },
	{ "", 1, 0 },
};

static const UpnpXmlElement g_deviceLevel2[] = {
	{ "modelName", 1, 0 },
	{ "friendlyName", 1, 0 },
	{ "deviceList", 0, 0 },
	{ "", 1, 0 },
};

static const UpnpXmlElement g_deviceLevel3[] = {
	{ "modelName", 1, 0 },
	{ "friendlyName", 1, 0 },
	{ "", 1, 0 },
};

static const UpnpXmlElement *const g_deviceLevels[] = {
	g_deviceLevel0, g_deviceLevel1, g_deviceLevel2, g_deviceLevel3,
};

static const UpnpXmlSchema g_deviceSchema = { g_deviceLevels, 4, 0 };

/* service list: every <service> element at any depth */
static const UpnpXmlElement g_serviceLevel0[] = {
	{ "service", 0, 0 },
	{ "", 1, 0 },
};

static const UpnpXmlElement g_serviceLevel1[] = {
	{ "serviceType", 1, 0 },
	{ "controlURL", 1, 0 },
	{ "", 1, 0 },
};

static const UpnpXmlElement *const g_serviceLevels[] = {
	g_serviceLevel0, g_serviceLevel1,
};

static const UpnpXmlSchema g_serviceSchema = { g_serviceLevels, 2, 0 };

int netCtlUpnpInit(NetCtlUpnp *upnp);
void netCtlUpnpTerm(NetCtlUpnp *upnp);
int netCtlUpnpAbort(NetCtlUpnp *upnp);
int netCtlUpnpRequest(NetCtlUpnp *upnp, int action, int protocol, SceUInt16 internalPort,
		      SceUInt16 externalPort, const char **externalIp, SceUInt32 *attempted,
		      SceUInt32 *succeeded);
int netCtlUpnpSearch(int sock, char *buf, int size, const SceNetInAddr *gateway,
		     SceUInt16 *port, char *path, SceSize pathSize);
int netCtlUpnpParseUrl(char *url, const SceNetInAddr *gateway, SceUInt16 *port, char *path,
		       SceSize pathSize);
int netCtlUpnpDescInit(NetCtlUpnp *upnp, char *controlUrl, SceSize controlUrlSize);
int netCtlUpnpDescResult(NetCtlUpnp *upnp);
int netCtlUpnpSoapInit(NetCtlUpnp *upnp, int action, int service, int *errorCode, int unk,
		       char *result, SceSize resultSize);
int netCtlUpnpSoapResult(NetCtlUpnp *upnp);
int netCtlUpnpBuildAddPortMapping(char *buf, int size, const char *client, int service,
				  SceUInt16 externalPort, int protocol, SceUInt16 internalPort);
int netCtlUpnpBuildDeletePortMapping(char *buf, int size, int service, SceUInt16 externalPort,
				     int protocol);
int netCtlUpnpBuildAction(char *buf, int size, int action, int service);

/* name unknown: initialises a UPnP client context */
__attribute__((noinline))
int SceNetCtl_743E9FCE(NetCtlUpnp *upnp)
{
	int ret;

	if (upnp == NULL)
		return UPNP_ERROR_INVALID_POINTER;
	ret = netCtlUpnpInit(upnp);
	if (ret < 0)
		return ret;
	upnp->initialized = 1;
	return 0;
}

/* name unknown: terminates a UPnP client context */
__attribute__((noinline))
void SceNetCtl_9B678FEE(NetCtlUpnp *upnp)
{
	if (upnp == NULL || upnp->initialized == 0)
		return;
	netCtlUpnpTerm(upnp);
}

/* name unknown: aborts the request in progress */
__attribute__((noinline))
int SceNetCtl_68C081D1(NetCtlUpnp *upnp)
{
	int ret;

	if (upnp == NULL)
		return UPNP_ERROR_INVALID_POINTER;
	if (upnp->initialized == 0)
		return UPNP_ERROR_NOT_INITIALIZED;
	ret = netCtlUpnpAbort(upnp);
	if (ret < 0)
		return ret;
	return 0;
}

/* name unknown: AddPortMapping on the gateway (discovers it first) */
__attribute__((noinline))
int SceNetCtl_9CB6FEE1(NetCtlUpnp *upnp, int protocol, SceUInt16 internalPort,
		       SceUInt16 externalPort, const char **externalIp, SceUInt32 *attempted,
		       SceUInt32 *succeeded)
{
	int ret;

	if (attempted != NULL)
		*attempted = 0;
	if (succeeded != NULL)
		*succeeded = 0;
	if (upnp == NULL)
		return UPNP_ERROR_INVALID_POINTER;
	if (upnp->initialized == 0)
		return UPNP_ERROR_NOT_INITIALIZED;
	ret = netCtlUpnpRequest(upnp, UPNP_ACTION_ADD_PORT_MAPPING, protocol, internalPort,
				externalPort, externalIp, attempted, succeeded);
	if (ret < 0)
		return ret;
	return 0;
}

/* name unknown: DeletePortMapping on the gateway */
__attribute__((noinline))
int SceNetCtl_9AFCDAC4(NetCtlUpnp *upnp, int protocol, SceUInt16 externalPort)
{
	int ret;

	if (upnp == NULL)
		return UPNP_ERROR_INVALID_POINTER;
	if (upnp->initialized == 0)
		return UPNP_ERROR_NOT_INITIALIZED;
	ret = netCtlUpnpRequest(upnp, UPNP_ACTION_DELETE_PORT_MAPPING, protocol, 0, externalPort,
				NULL, NULL, NULL);
	if (ret < 0)
		return ret;
	return 0;
}

__attribute__((noinline))
int netCtlUpnpInit(NetCtlUpnp *upnp)
{
	sceClibMemset(upnp, 0, sizeof(*upnp));
	upnp->sock = -1;
	return 0;
}

__attribute__((noinline))
void netCtlUpnpTerm(NetCtlUpnp *upnp)
{
	if (upnp->flags & UPNP_FLAG_BUSY)
		SceNetCtl_68C081D1(upnp);
	sceClibMemset(upnp, 0, sizeof(*upnp));
}

__attribute__((noinline))
int netCtlUpnpAbort(NetCtlUpnp *upnp)
{
	upnp->aborted = 1;
	if (upnp->sock >= 0)
		sceNetSocketAbort(upnp->sock, SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION |
				  SCE_NET_SOCKET_ABORT_FLAG_SND_PRESERVATION);
	return 0;
}

/* Sends one SOAP action to the control URL of a service. */
__attribute__((noinline))
int netCtlUpnpAction(NetCtlUpnp *upnp, int service, int action, SceUInt16 externalPort,
		     int protocol, SceUInt16 internalPort, int *upnpError, int timeout)
{
	char *body = upnp->body;
	int errorCode;
	SceNetSockaddrIn addr;
	SceNetCtlInfo info;
	int bodyLen;
	int ret;

	switch (action) {
	case UPNP_ACTION_ADD_PORT_MAPPING:
		ret = SceNetCtl_C33D7374(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info);
		if (ret < 0)
			return ret;
		bodyLen = netCtlUpnpBuildAddPortMapping(body, UPNP_BUFFER_SIZE, info.ip_address,
							service, externalPort, protocol,
							internalPort);
		break;
	case UPNP_ACTION_DELETE_PORT_MAPPING:
		bodyLen = netCtlUpnpBuildDeletePortMapping(body, UPNP_BUFFER_SIZE, service,
							   externalPort, protocol);
		break;
	case UPNP_ACTION_GET_EXTERNAL_IP_ADDRESS:
		bodyLen = netCtlUpnpBuildAction(body, UPNP_BUFFER_SIZE, action, service);
		break;
	default:
		return UPNP_ERROR_INVALID_ACTION;
	}
	if (bodyLen < 0)
		return bodyLen;

	switch (action) {
	case UPNP_ACTION_ADD_PORT_MAPPING:
	case UPNP_ACTION_DELETE_PORT_MAPPING:
		ret = netCtlUpnpSoapInit(upnp, action, service, &errorCode, 0, NULL, 0);
		if (ret < 0)
			return ret;
		break;
	case UPNP_ACTION_GET_EXTERNAL_IP_ADDRESS:
		ret = netCtlUpnpSoapInit(upnp, action, service, &errorCode, 0, upnp->externalIp,
					 sizeof(upnp->externalIp));
		if (ret < 0)
			return ret;
		break;
	default:
		ret = UPNP_ERROR_INVALID_ACTION;
		break;
	}

	upnp->sock = sceNetSocket("SceUpnp", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
	if (upnp->sock < 0)
		return upnp->sock;
	if (timeout != 0) {
		sceNetSetsockopt(upnp->sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO, &timeout,
				 sizeof(timeout));
		sceNetSetsockopt(upnp->sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &timeout,
				 sizeof(timeout));
	}
	sceClibMemset(&addr, 0, sizeof(addr));
	addr.sin_addr = upnp->gateway;
	addr.sin_family = SCE_NET_AF_INET;
	addr.sin_port = sceNetHtons(upnp->port);

	switch (action) {
	case UPNP_ACTION_ADD_PORT_MAPPING:
	case UPNP_ACTION_DELETE_PORT_MAPPING:
	case UPNP_ACTION_GET_EXTERNAL_IP_ADDRESS:
		ret = netCtlUpnpHttpPost(upnp, upnp->sock, upnp->buf, UPNP_BUFFER_SIZE, &addr,
					 upnp->controlUrl[service], action, service, body, bodyLen,
					 timeout);
		break;
	default:
		ret = UPNP_ERROR_INVALID_ACTION;
		break;
	}
	sceNetSocketClose(upnp->sock);
	upnp->sock = -1;
	if (ret < 0)
		return ret;

	ret = netCtlUpnpSoapResult(upnp);
	if (ret != UPNP_ERROR_UPNP_FAULT || upnpError == NULL)
		return ret;
	switch (errorCode) {
	case 401:
		*upnpError = UPNP_ERROR_FAULT_401;
		break;
	case 402:
		*upnpError = UPNP_ERROR_FAULT_402;
		break;
	case 404:
		*upnpError = UPNP_ERROR_FAULT_404;
		break;
	case 501:
		*upnpError = UPNP_ERROR_FAULT_501;
		break;
	case 714:
		*upnpError = UPNP_ERROR_FAULT_714;
		break;
	case 715:
		*upnpError = UPNP_ERROR_FAULT_715;
		break;
	case 716:
		*upnpError = UPNP_ERROR_FAULT_716;
		break;
	case 718:
		*upnpError = UPNP_ERROR_FAULT_718;
		break;
	case 724:
		*upnpError = UPNP_ERROR_FAULT_724;
		break;
	case 725:
		*upnpError = UPNP_ERROR_FAULT_725;
		break;
	case 726:
		*upnpError = UPNP_ERROR_FAULT_726;
		break;
	case 727:
		*upnpError = UPNP_ERROR_FAULT_727;
		break;
	default:
		*upnpError = UPNP_ERROR_FAULT_OTHER;
		break;
	}
	return ret;
}

/* Finds the gateway, downloads its description and collects the control URLs. */
static inline __attribute__((always_inline))
int netCtlUpnpDiscover(NetCtlUpnp *upnp, char *controlUrl, SceSize controlUrlSize)
{
	int timeout;
	SceNetSockaddrIn addr;
	int ret = 0;

	sceClibMemset(&upnp->unk12A0, 0, sizeof(upnp->unk12A0));
	upnp->port = 0;
	sceClibMemset(upnp->urlBase, 0, sizeof(upnp->urlBase));
	upnp->services = 0;
	sceClibMemset(upnp->modelName, 0, sizeof(upnp->modelName));
	sceClibMemset(upnp->friendlyName, 0, sizeof(upnp->friendlyName));
	sceClibMemset(upnp->externalIp, 0, sizeof(upnp->externalIp));

	upnp->sock = sceNetSocket("SceUpnp", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
	if (upnp->sock < 0)
		return ret;
	ret = netCtlUpnpSearch(upnp->sock, upnp->buf, UPNP_BUFFER_SIZE, &upnp->gateway,
			       &upnp->port, upnp->path, UPNP_PATH_SIZE);
	sceNetSocketClose(upnp->sock);
	upnp->sock = -1;
	if (ret < 0)
		return ret;

	ret = netCtlUpnpDescInit(upnp, controlUrl, controlUrlSize);
	if (ret < 0)
		return ret;
	upnp->sock = sceNetSocket("SceUpnp", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
	if (upnp->sock < 0)
		return ret;
	timeout = UPNP_TIMEOUT;
	sceNetSetsockopt(upnp->sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO, &timeout,
			 sizeof(timeout));
	sceNetSetsockopt(upnp->sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &timeout,
			 sizeof(timeout));
	sceClibMemset(&addr, 0, sizeof(addr));
	addr.sin_family = SCE_NET_AF_INET;
	addr.sin_addr = upnp->gateway;
	addr.sin_port = sceNetHtons(upnp->port);
	ret = netCtlUpnpHttpGet(upnp, upnp->sock, upnp->buf, UPNP_BUFFER_SIZE, &addr, upnp->path);
	sceNetSocketClose(upnp->sock);
	upnp->sock = -1;
	if (ret < 0)
		return ret;
	return netCtlUpnpDescResult(upnp);
}

__attribute__((noinline))
int netCtlUpnpRequest(NetCtlUpnp *upnp, int action, int protocol, SceUInt16 internalPort,
		      SceUInt16 externalPort, const char **externalIp, SceUInt32 *attempted,
		      SceUInt32 *succeeded)
{
	char controlUrl[0x200];
	int state;
	int upnpError;
	int faultError;
	SceNetCtlInfo info;
	int service;
	int ret;
	int i;

	upnp->flags |= UPNP_FLAG_BUSY;
	sceClibMemset(&upnp->gateway, 0, sizeof(upnp->gateway));
	ret = SceNetCtl_E8E8DF18(&state);
	if (ret < 0)
		goto out;
	if (state != NETCTL_INET_STATE_IPOBTAINED) {
		ret = UPNP_ERROR_NOT_CONNECTED;
		goto out;
	}
	ret = SceNetCtl_C33D7374(SCE_NETCTL_INFO_GET_DEFAULT_ROUTE, &info);
	if (ret < 0)
		goto out;
	sceNetInetPton(SCE_NET_AF_INET, info.default_route, &upnp->gateway);

	if (upnp->flags & UPNP_FLAG_DISCOVERED) {
		if (attempted != NULL)
			*attempted |= 3;
		if (succeeded != NULL)
			*succeeded |= 3;
	} else {
		ret = netCtlUpnpDiscover(upnp, controlUrl, sizeof(controlUrl) - 1);
		if (attempted != NULL)
			*attempted |= 1;
		if (ret < 0)
			goto out;
		if (succeeded != NULL)
			*succeeded |= 1;

		/* the first service that tells its external address is used */
		for (i = 0; i < UPNP_SERVICE_COUNT; i++)
			if (upnp->services & (1 << i))
				break;
		if (i == UPNP_SERVICE_COUNT) {
			/* Sony leaves service holding the last result (always 0 here) */
			service = ret;
			ret = UPNP_ERROR_NO_SERVICE;
		} else {
			ret = 0;
			service = i;
		}
		for (;;) {
			if (ret >= 0) {
				ret = netCtlUpnpAction(upnp, service,
						       UPNP_ACTION_GET_EXTERNAL_IP_ADDRESS, 0, 0,
						       0, &faultError, UPNP_TIMEOUT);
				if (ret == UPNP_ERROR_UPNP_FAULT) {
					ret = faultError;
				} else if (ret >= 0) {
					if (upnp->externalIp[0] == '\0')
						ret = UPNP_ERROR_NO_EXTERNAL_IP;
					else
						upnp->service = service;
				}
				if (ret >= 0)
					break;
			}
			for (i = service + 1; i < UPNP_SERVICE_COUNT; i++)
				if (upnp->services & (1 << i))
					break;
			if (i == UPNP_SERVICE_COUNT)
				break;
			service = i;
		}
		if (attempted != NULL)
			*attempted |= 2;
		if (ret < 0)
			goto out;
		if (succeeded != NULL)
			*succeeded |= 2;
		upnp->flags |= UPNP_FLAG_DISCOVERED;
	}

	if (externalIp != NULL)
		*externalIp = upnp->externalIp;
	if (action < UPNP_ACTION_ADD_PORT_MAPPING || action > UPNP_ACTION_DELETE_PORT_MAPPING) {
		ret = UPNP_ERROR_INVALID_ACTION;
	} else {
		ret = netCtlUpnpAction(upnp, upnp->service, action, externalPort, protocol,
				       internalPort, &upnpError, UPNP_TIMEOUT);
		if (ret == UPNP_ERROR_UPNP_FAULT)
			ret = upnpError;
		if (attempted != NULL)
			*attempted |= 4;
		if (ret >= 0 && succeeded != NULL)
			*succeeded |= 4;
	}
out:
	upnp->flags &= ~UPNP_FLAG_BUSY;
	return ret;
}

/*
 * Terminates the line at the start of the buffer (CR LF or LF, trailing
 * blanks removed) and returns the offset of the next line.
 */
__attribute__((noinline))
int netCtlUpnpReadLine(char *line, SceSize size)
{
	char *nl, *end;

	nl = sceClibStrchr(line, '\n');
	if (nl == NULL || nl - line >= size)
		return UPNP_ERROR_NO_LINE;
	*nl = '\0';
	end = nl;
	if (line < nl && nl[-1] == '\r') {
		end = nl - 1;
		nl[-1] = '\0';
	}
	while (line < end && (end[-1] == ' ' || end[-1] == '\t')) {
		end[-1] = '\0';
		end--;
	}
	return nl - line + 1;
}

/* Splits "Name: value" in place and returns the offset of the value. */
int netCtlUpnpSplitHeader(char *line)
{
	char *colon, *end, *value;

	colon = sceClibStrchr(line, ':');
	if (colon == NULL)
		return UPNP_ERROR_BAD_URL;
	*colon = '\0';
	value = colon + 1;
	end = colon;
	while (line < end && (end[-1] == ' ' || end[-1] == '\t'))
		*--end = '\0';
	while (*value == ' ' || *value == '\t')
		value++;
	return value - line;
}

/* Parses "http://<gateway>[:port]/path"; the host must be the gateway. */
__attribute__((noinline))
int netCtlUpnpParseUrl(char *url, const SceNetInAddr *gateway, SceUInt16 *port, char *path,
		       SceSize pathSize)
{
	SceNetInAddr addr;
	char *p;

	if (sceClibStrncmp(url, "http://", 7) != 0)
		goto fail;
	url += 7;
	p = sceClibStrchr(url, '/');
	if (p != NULL) {
		if (sceClibStrnlen(p, pathSize + 1) >= pathSize)
			goto fail;
		sceClibStrncpy(path, p, pathSize);
		*p = '\0';
	} else {
		if (pathSize < 2)
			goto fail;
		sceClibStrncpy(path, "/", 1);
	}
	p = sceClibStrchr(url, ':');
	if (p != NULL) {
		*p = '\0';
		if (sceClibStrnlen(p + 1, 6) > 5)
			goto fail;
		*port = sceClibStrtoll(p + 1, NULL, 10);
	} else {
		*port = 80;
	}
	if (sceNetInetPton(SCE_NET_AF_INET, url, &addr) > 0 && addr.s_addr == gateway->s_addr)
		return 0;
fail:
	return UPNP_ERROR_BAD_URL;
}

static inline __attribute__((always_inline)) int netCtlUpnpBuildSearch(char *buf, int size)
{
	char *p = buf;
	int n;

	n = sceClibSnprintf(p, size, "M-SEARCH * HTTP/1.1\r\n");
	if (n < 0 || n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "HOST: %s:%d\r\n", SSDP_ADDRESS, SSDP_PORT);
	if (n < 0 || n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "MAN: %s\r\n", "\"ssdp:discover\"");
	if (n < 0 || n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "MX: %d\r\n", 1);
	if (n < 0 || n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "ST: %s\r\n", SSDP_DEVICE_TYPE);
	if (n < 0 || n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "\r\n");
	if (n < 0 || n >= size)
		goto fail;
	return p + n - buf;
fail:
	return UPNP_ERROR_BUFFER_TOO_SMALL;
}

/* SSDP search for the Internet Gateway Device; returns its description path. */
__attribute__((noinline))
int netCtlUpnpSearch(int sock, char *buf, int size, const SceNetInAddr *gateway,
		     SceUInt16 *port, char *path, SceSize pathSize)
{
	SceUInt8 ttl;
	int error;
	unsigned int optlen;
	unsigned int fromlen;
	SceNetEpollEvent event;
	SceNetEpollEvent result;
	SceNetSockaddrIn addr;
	SceNetSockaddrIn from;
	SceUInt64 deadline;
	SceUInt32 now;
	char *line, *next;
	int left, len;
	char hasLocation, hasType;
	unsigned int tries;
	int epoll;
	int ret, n;

	ttl = 4;
	ret = sceNetSetsockopt(sock, SCE_NET_IPPROTO_IP, SCE_NET_IP_MULTICAST_TTL, &ttl, 1);
	if (ret < 0)
		return ret;
	sceClibMemset(&addr, 0, sizeof(addr));
	if (sceNetInetPton(SCE_NET_AF_INET, SSDP_ADDRESS, &addr.sin_addr) <= 0)
		return UPNP_ERROR_BAD_ADDRESS;
	addr.sin_family = SCE_NET_AF_INET;
	addr.sin_port = sceNetHtons(SSDP_PORT);

	tries = 0;
	do {
		tries++;
		ret = netCtlUpnpBuildSearch(buf, size);
		if (ret < 0)
			return ret;
		ret = sceNetSendto(sock, buf, ret, 0, (SceNetSockaddr *)&addr, sizeof(addr));
		if (ret < 0)
			return ret;
		deadline = (SceUInt64)ksceKernelGetSystemTimeLow() + SSDP_WAIT;
		while ((now = ksceKernelGetSystemTimeLow()) < deadline) {
			epoll = -1;
			ret = sceNetEpollCreate("SceNetUpnpAction", 0);
			if (ret < 0)
				goto fail;
			epoll = ret;
			sceClibMemset(&event, 0, sizeof(event));
			event.events = SCE_NET_EPOLLIN;
			ret = sceNetEpollControl(epoll, SCE_NET_EPOLL_CTL_ADD, sock, &event);
			if (ret < 0)
				goto fail;
			n = sceNetEpollWait(epoll, &result, 1, deadline - now);
			if (n < 0) {
				ret = n;
				goto fail;
			}
			sceNetEpollDestroy(epoll);
			if (n == 0)
				break;
			if ((result.events & SCE_NET_EPOLLIN) == 0) {
				optlen = sizeof(error);
				ret = sceNetGetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_ERROR,
						       &error, &optlen);
				if (ret >= 0)
					ret = error | 0x80410100;
				return ret;
			}
			fromlen = sizeof(from);
			buf[size - 1] = '\0';
			ret = sceNetRecvfrom(sock, buf, size - 1, 0, (SceNetSockaddr *)&from,
					     &fromlen);
			if (ret >= 0) {
				if (from.sin_addr.s_addr == gateway->s_addr &&
				    (n = netCtlUpnpReadLine(buf, ret)) >= 0) {
					hasType = 0;
					line = buf + n;
					left = ret - n;
					hasLocation = 0;
					while ((ret = netCtlUpnpReadLine(line, left)) >= 0) {
						if (*line == '\0') {
							if (hasLocation && hasType)
								goto done;
							break;
						}
						next = line + ret;
						len = netCtlUpnpSplitHeader(line);
						if (len != UPNP_ERROR_BAD_URL) {
							if (len < 0)
								break;
							if (sceClibStrncasecmp(line, "Location", 9) == 0) {
								if (netCtlUpnpParseUrl(line + len, gateway,
										       port, path,
										       pathSize) >= 0)
									hasLocation = 1;
							} else if (sceClibStrncasecmp(line, "ST", 3) == 0 &&
								   sceClibStrncmp(line + len,
										  SSDP_DEVICE_TYPE,
										  sizeof(SSDP_DEVICE_TYPE)) == 0) {
								hasType = 1;
							}
						}
						left -= next - line;
						line = next;
					}
				}
				ret = UPNP_ERROR_NOT_GATEWAY;
			}
done:
			if (ret != UPNP_ERROR_NOT_GATEWAY)
				return ret;
		}
	} while (tries < SSDP_TRIES);
	return UPNP_ERROR_NO_GATEWAY;

fail:
	if (epoll >= 0)
		sceNetEpollDestroy(epoll);
	return ret;
}

/* Opens <s:Envelope><s:Body><u:Action xmlns:u="service">. */
__attribute__((noinline))
int netCtlUpnpBuildEnvelope(char *buf, int size, int action, int service)
{
	char *p = buf;
	int n;

	n = sceClibSnprintf(p, size, "<s:%s xmlns:s=\"%s\" s:encodingStyle=\"%s\">\r\n", "Envelope",
			    "http://schemas.xmlsoap.org/soap/envelope/",
			    "http://schemas.xmlsoap.org/soap/encoding/");
	if (n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "   <s:%s>\r\n", "Body");
	if (n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "      <u:%s xmlns:u=\"%s\">\r\n", g_upnpActionNames[action],
			    g_upnpServiceTypes[service]);
	if (n >= size)
		goto fail;
	return p + n - buf;
fail:
	return UPNP_ERROR_BUFFER_TOO_SMALL;
}

static inline __attribute__((always_inline))
int netCtlUpnpBuildEnvelopeEnd(char *buf, int size, int action)
{
	char *p = buf;
	int n;

	n = sceClibSnprintf(p, size, "      </u:%s>\r\n", g_upnpActionNames[action]);
	if (n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "   </s:%s>\r\n", "Body");
	if (n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, "</s:%s>\r\n", "Envelope");
	if (n >= size)
		goto fail;
	return p + n - buf;
fail:
	return UPNP_ERROR_BUFFER_TOO_SMALL;
}

__attribute__((noinline))
int netCtlUpnpBuildAddPortMapping(char *buf, int size, const char *client, int service,
				  SceUInt16 externalPort, int protocol, SceUInt16 internalPort)
{
	char *p = buf;
	int n;

	n = netCtlUpnpBuildEnvelope(p, size, UPNP_ACTION_ADD_PORT_MAPPING, service);
	if (n < 0)
		return n;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, " <%s>%s</%s>\r\n", "NewRemoteHost", "", "NewRemoteHost");
	if (n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, " <%s>%d</%s>\r\n", "NewExternalPort", externalPort,
			    "NewExternalPort");
	if (n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, " <%s>%s</%s>\r\n", "NewProtocol",
			    g_upnpProtocolNames[protocol], "NewProtocol");
	if (n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, " <%s>%d</%s>\r\n", "NewInternalPort", internalPort,
			    "NewInternalPort");
	if (n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, " <%s>%s</%s>\r\n", "NewInternalClient", client,
			    "NewInternalClient");
	if (n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, " <%s>%d</%s>\r\n", "NewEnabled", 1, "NewEnabled");
	if (n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, " <%s>%s:%d to %d (%s)</%s>\r\n",
			    "NewPortMappingDescription", client, internalPort, externalPort,
			    g_upnpProtocolNames[protocol], "NewPortMappingDescription");
	if (n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, " <%s>%d</%s>\r\n", "NewLeaseDuration", 0,
			    "NewLeaseDuration");
	if (n >= size)
		goto fail;
	p += n;
	size -= n;
	n = netCtlUpnpBuildEnvelopeEnd(p, size, UPNP_ACTION_ADD_PORT_MAPPING);
	if (n >= 0)
		n = p + n - buf;
	return n;
fail:
	return UPNP_ERROR_BUFFER_TOO_SMALL;
}

__attribute__((noinline))
int netCtlUpnpBuildDeletePortMapping(char *buf, int size, int service, SceUInt16 externalPort,
				     int protocol)
{
	char *p = buf;
	int n;

	n = netCtlUpnpBuildEnvelope(p, size, UPNP_ACTION_DELETE_PORT_MAPPING, service);
	if (n < 0)
		return n;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, " <%s>%s</%s>\r\n", "NewRemoteHost", "", "NewRemoteHost");
	if (n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, " <%s>%d</%s>\r\n", "NewExternalPort", externalPort,
			    "NewExternalPort");
	if (n >= size)
		goto fail;
	p += n;
	size -= n;
	n = sceClibSnprintf(p, size, " <%s>%s</%s>\r\n", "NewProtocol",
			    g_upnpProtocolNames[protocol], "NewProtocol");
	if (n >= size)
		goto fail;
	p += n;
	size -= n;
	n = netCtlUpnpBuildEnvelopeEnd(p, size, UPNP_ACTION_DELETE_PORT_MAPPING);
	if (n >= 0)
		n = p + n - buf;
	return n;
fail:
	return UPNP_ERROR_BUFFER_TOO_SMALL;
}

/* Action without arguments. */
__attribute__((noinline))
int netCtlUpnpBuildAction(char *buf, int size, int action, int service)
{
	char *p = buf;
	int n;

	n = netCtlUpnpBuildEnvelope(p, size, action, service);
	if (n < 0)
		return n;
	p += n;
	size -= n;
	n = netCtlUpnpBuildEnvelopeEnd(p, size, action);
	if (n >= 0)
		n = p + n - buf;
	return n;
}

int netCtlUpnpSoapCallback(UpnpXmlParser *parser, int event, int depth, int index,
			   const char *text, const char *value, void *arg);

__attribute__((noinline))
int netCtlUpnpSoapInit(NetCtlUpnp *upnp, int action, int service, int *errorCode, int unk,
		       char *result, SceSize resultSize)
{
	sceClibMemset(&upnp->soap, 0, sizeof(upnp->soap));
	upnp->soap.action = action;
	upnp->soap.service = service;
	upnp->soap.errorCode = errorCode;
	upnp->soap.unk364 = unk;
	upnp->soap.result = result;
	upnp->soap.resultSize = resultSize;
	return netCtlUpnpXmlInit(&upnp->soap.parser, &g_soapSchema, netCtlUpnpSoapCallback, upnp);
}

__attribute__((noinline))
int netCtlUpnpSoapParse(NetCtlUpnp *upnp, const char *data, SceSize size, int last)
{
	return netCtlUpnpXmlParse(&upnp->soap.parser, data, size);
}

__attribute__((noinline))
int netCtlUpnpSoapResult(NetCtlUpnp *upnp)
{
	SceUInt32 flags = upnp->soap.flags;

	if ((flags & UPNP_SOAP_ENVELOPE_END) == 0)
		return UPNP_ERROR_INCOMPLETE_XML;
	if (flags & UPNP_SOAP_ERROR_CODE)
		return UPNP_ERROR_UPNP_FAULT;
	if ((flags & UPNP_SOAP_RESPONSE_END) == 0)
		return UPNP_ERROR_INCOMPLETE_RESPONSE;
	switch (upnp->soap.action) {
	case UPNP_ACTION_ADD_PORT_MAPPING:
	case UPNP_ACTION_DELETE_PORT_MAPPING:
		return 0;
	case UPNP_ACTION_GET_EXTERNAL_IP_ADDRESS:
		if (flags & UPNP_SOAP_EXTERNAL_IP)
			return 0;
		return UPNP_ERROR_INCOMPLETE_RESPONSE;
	case UPNP_ACTION_GET_STATUS_INFO:
		if (flags & UPNP_SOAP_STATUS)
			return 0;
		return UPNP_ERROR_INCOMPLETE_RESPONSE;
	default:
		return UPNP_ERROR_INVALID_ACTION;
	}
}

/* Checks one attribute against an expected one; UPNP_ERROR_ATTR_MISMATCH if it differs. */
__attribute__((noinline))
int netCtlUpnpSoapCheckAttribute(const char *name, const char *value,
				 const UpnpXmlAttribute *attr, const char *prefix,
				 const char *serviceType)
{
	const char *expected;
	const char *p, *q;
	SceSize n;

	switch (attr->nameMode) {
	case UPNP_XML_ATTR_NAME:
		if (sceClibStrncmp(name, attr->name, UPNP_XML_NAME_SIZE) != 0)
			goto mismatch;
		break;
	case UPNP_XML_ATTR_PREFIX_NAME:
		expected = attr->name;
		if (name == NULL || prefix == NULL || expected == NULL)
			goto mismatch;
		if (sceClibStrncmp(prefix, name, sceClibStrnlen(prefix, UPNP_XML_VALUE_SIZE)) != 0)
			goto mismatch;
		n = sceClibStrnlen(prefix, UPNP_XML_VALUE_SIZE);
		if (name[n] != ':')
			goto mismatch;
		if (sceClibStrncmp(expected, name + n + 1, UPNP_XML_VALUE_SIZE) != 0)
			goto mismatch;
		break;
	case UPNP_XML_ATTR_NAME_PREFIX:
		expected = attr->name;
		if (name == NULL || expected == NULL || prefix == NULL)
			goto mismatch;
		if (sceClibStrncmp(expected, name, sceClibStrnlen(expected, UPNP_XML_VALUE_SIZE)) != 0)
			goto mismatch;
		n = sceClibStrnlen(expected, UPNP_XML_VALUE_SIZE);
		if (name[n] != ':')
			goto mismatch;
		if (sceClibStrncmp(prefix, name + n + 1, UPNP_XML_VALUE_SIZE) != 0)
			goto mismatch;
		break;
	}

	switch (attr->valueMode) {
	case UPNP_XML_VALUE_EXACT:
		if (sceClibStrncmp(value, attr->value, UPNP_XML_VALUE_SIZE) != 0)
			goto mismatch;
		break;
	case UPNP_XML_VALUE_SERVICE:
		/* "urn:schemas-upnp-org:" and the part after "service" must match */
		p = sceClibStrchr(serviceType, ':');
		if (p == NULL)
			goto mismatch;
		p = sceClibStrchr(p + 1, ':');
		if (p == NULL)
			goto mismatch;
		n = p + 1 - serviceType;
		if (sceClibStrncmp(serviceType, value, n) != 0)
			goto mismatch;
		p = sceClibStrchr(p + 1, ':');
		if (p == NULL)
			goto mismatch;
		q = sceClibStrchr(value + n, ':');
		if (q == NULL)
			goto mismatch;
		if (sceClibStrncmp(p, q, UPNP_XML_VALUE_SIZE) != 0)
			goto mismatch;
		break;
	}
	return 0;
mismatch:
	return UPNP_ERROR_ATTR_MISMATCH;
}

static inline __attribute__((always_inline))
void netCtlUpnpSoapClearAttributes(NetCtlUpnp *upnp, const UpnpXmlAttribute *attrs)
{
	int i;

	for (i = 0; *attrs[i].name != '\0'; i++)
		upnp->soap.attrSeen[i] = 0;
}

static inline __attribute__((always_inline))
int netCtlUpnpSoapCheckAttributes(NetCtlUpnp *upnp, const UpnpXmlAttribute *attrs)
{
	int i;

	for (i = 0; *attrs[i].name != '\0'; i++)
		if (upnp->soap.attrSeen[i] != 1)
			return UPNP_ERROR_ATTR_MISSING;
	return 0;
}

static inline __attribute__((always_inline))
int netCtlUpnpSoapMatchAttribute(NetCtlUpnp *upnp, const UpnpXmlAttribute *attrs,
				 const char *name, const char *value, const char *prefix,
				 const char *serviceType)
{
	int ret = 0;
	int i;

	for (i = 0; *attrs[i].name != '\0'; i++) {
		ret = netCtlUpnpSoapCheckAttribute(name, value, &attrs[i], prefix, serviceType);
		if (ret != UPNP_ERROR_ATTR_MISMATCH) {
			if (ret >= 0)
				upnp->soap.attrSeen[i] = 1;
			break;
		}
	}
	return ret;
}

/* Remembers the namespace prefix of "<prefix>:<name>". */
static inline __attribute__((always_inline))
int netCtlUpnpSoapSetPrefix(NetCtlUpnp *upnp, const char *tag, const char *name)
{
	const char *colon;
	int n;

	if (sceClibStrnlen(tag, UPNP_XML_VALUE_SIZE) >
	    sceClibStrnlen(name, UPNP_XML_VALUE_SIZE) + UPNP_PATH_SIZE)
		return UPNP_ERROR_TAG_TOO_LONG;
	colon = sceClibStrchr(tag, ':');
	if (colon == NULL)
		return UPNP_ERROR_BAD_PREFIX;
	if (sceClibStrncmp(name, colon + 1, sceClibStrnlen(name, UPNP_XML_VALUE_SIZE)) != 0)
		return UPNP_ERROR_BAD_PREFIX;
	n = colon + 1 - tag;
	sceClibStrncpy(upnp->path, tag, n - 1);
	upnp->path[n - 1] = '\0';
	return 0;
}

/* Checks that a tag is "<prefix>:<name>". */
static inline __attribute__((always_inline))
int netCtlUpnpSoapCheckPrefix(const char *prefix, const char *tag, const char *name)
{
	SceSize n;

	if (tag == NULL || prefix == NULL)
		return UPNP_ERROR_INVALID_POINTER;
	if (sceClibStrncmp(prefix, tag, sceClibStrnlen(prefix, UPNP_XML_VALUE_SIZE)) != 0)
		return UPNP_ERROR_BAD_PREFIX;
	n = sceClibStrnlen(prefix, UPNP_XML_VALUE_SIZE);
	if (tag[n] != ':')
		return UPNP_ERROR_BAD_PREFIX;
	if (sceClibStrncmp(name, tag + n + 1, UPNP_XML_VALUE_SIZE) != 0)
		return UPNP_ERROR_BAD_PREFIX;
	return 0;
}

__attribute__((noinline))
int netCtlUpnpSoapCallback(UpnpXmlParser *parser, int event, int depth, int index,
			   const char *text, const char *value, void *arg)
{
	NetCtlUpnp *upnp = arg;
	const char *name;
	SceSize size, len;
	int ret;

	if (event == UPNP_XML_EVENT_START && depth == 0 && index == 0) {
		upnp->soap.flags |= UPNP_SOAP_ENVELOPE;
		netCtlUpnpSoapClearAttributes(upnp, g_soapEnvelopeAttributes);
		ret = netCtlUpnpSoapSetPrefix(upnp, text, "Envelope");
		if (ret < 0)
			return ret;
	}
	if (event == UPNP_XML_EVENT_ATTRIBUTE && depth == 0 && index == 0) {
		ret = netCtlUpnpSoapMatchAttribute(upnp, g_soapEnvelopeAttributes, text, value,
						   upnp->path, NULL);
		if (ret < 0)
			return ret;
	}
	if (event == UPNP_XML_EVENT_START && depth == 1 && index == 0) {
		upnp->soap.flags |= UPNP_SOAP_BODY;
		ret = netCtlUpnpSoapCheckPrefix(upnp->path, text, "Body");
		if (ret < 0)
			return ret;
	}
	if (event == UPNP_XML_EVENT_START && depth == 2 && index == upnp->soap.action) {
		upnp->soap.flags |= UPNP_SOAP_RESPONSE;
		name = g_upnpResponseNames[upnp->soap.action];
		netCtlUpnpSoapClearAttributes(upnp, g_soapResponseAttributes);
		ret = netCtlUpnpSoapSetPrefix(upnp, text, name);
		if (ret < 0)
			return ret;
	}
	if (event == UPNP_XML_EVENT_ATTRIBUTE && depth == 2 && index == upnp->soap.action) {
		ret = netCtlUpnpSoapMatchAttribute(upnp, g_soapResponseAttributes, text, value,
						   upnp->path,
						   g_upnpServiceTypes[upnp->soap.service]);
		if (ret < 0)
			return ret;
	}
	if (event == UPNP_XML_EVENT_TEXT && depth == 3 && index == SOAP_LEVEL3_EXTERNAL_IP &&
	    parser->index[depth - 1] == UPNP_ACTION_GET_EXTERNAL_IP_ADDRESS) {
		upnp->soap.flags |= UPNP_SOAP_EXTERNAL_IP;
		if (upnp->soap.result != NULL && (size = upnp->soap.resultSize) != 0) {
			if (*text == '\0') {
				*upnp->soap.result = '\0';
			} else {
				len = sceClibStrnlen(text, size);
				if (len >= size)
					return UPNP_ERROR_RESULT_TOO_LONG;
				sceClibStrncpy(upnp->soap.result, text, upnp->soap.resultSize);
			}
		}
	}
	if (event == UPNP_XML_EVENT_END && depth == 2 && index == upnp->soap.action) {
		ret = netCtlUpnpSoapCheckAttributes(upnp, g_soapResponseAttributes);
		if (ret < 0)
			return ret;
		upnp->soap.flags |= UPNP_SOAP_RESPONSE_END;
	}
	if (event == UPNP_XML_EVENT_START && depth == 2 && index == SOAP_LEVEL2_FAULT) {
		upnp->soap.flags |= UPNP_SOAP_FAULT;
		ret = netCtlUpnpSoapCheckPrefix(upnp->path, text, "Fault");
		if (ret < 0)
			return ret;
	}
	if (event == UPNP_XML_EVENT_TEXT && depth == 3 && index == SOAP_LEVEL3_FAULT_CODE &&
	    parser->index[depth - 1] == SOAP_LEVEL2_FAULT) {
		upnp->soap.flags |= UPNP_SOAP_FAULT_CODE;
		ret = netCtlUpnpSoapCheckPrefix(upnp->path, text, "Client");
		if (ret < 0)
			return ret;
	}
	if (event == UPNP_XML_EVENT_TEXT && depth == 3 && index == SOAP_LEVEL3_FAULT_STRING &&
	    parser->index[depth - 1] == SOAP_LEVEL2_FAULT) {
		upnp->soap.flags |= UPNP_SOAP_FAULT_STRING;
		if (sceClibStrncmp("UPnPError", text, 10) != 0)
			return UPNP_ERROR_NOT_UPNP_ERROR;
	}
	if (event == UPNP_XML_EVENT_START && depth == 3 && index == SOAP_LEVEL3_DETAIL &&
	    parser->index[depth - 1] == SOAP_LEVEL2_FAULT)
		upnp->soap.flags |= UPNP_SOAP_DETAIL;
	if (event == UPNP_XML_EVENT_START && depth == 4 && index == 0 &&
	    parser->index[depth - 2] == SOAP_LEVEL2_FAULT && parser->index[depth - 1] == SOAP_LEVEL3_DETAIL) {
		upnp->soap.flags |= UPNP_SOAP_UPNP_ERROR;
		netCtlUpnpSoapClearAttributes(upnp, g_soapUpnpErrorAttributes);
	}
	if (event == UPNP_XML_EVENT_ATTRIBUTE && depth == 4 && index == 0 &&
	    parser->index[depth - 2] == SOAP_LEVEL2_FAULT && parser->index[depth - 1] == SOAP_LEVEL3_DETAIL) {
		ret = netCtlUpnpSoapMatchAttribute(upnp, g_soapUpnpErrorAttributes, text, value,
						   NULL, NULL);
		if (ret < 0)
			return ret;
	}
	if (event == UPNP_XML_EVENT_TEXT && depth == 5 && index == 0 &&
	    parser->index[depth - 3] == SOAP_LEVEL2_FAULT && parser->index[depth - 2] == SOAP_LEVEL3_DETAIL) {
		upnp->soap.flags |= UPNP_SOAP_ERROR_CODE;
		*upnp->soap.errorCode = sceClibStrtoll(text, NULL, 10);
	}
	if (event == UPNP_XML_EVENT_END && depth == 4 && index == 0 &&
	    parser->index[depth - 2] == SOAP_LEVEL2_FAULT && parser->index[depth - 1] == SOAP_LEVEL3_DETAIL) {
		ret = netCtlUpnpSoapCheckAttributes(upnp, g_soapUpnpErrorAttributes);
		if (ret < 0)
			return ret;
		upnp->soap.flags |= UPNP_SOAP_UPNP_ERROR_END;
	}
	if (event == UPNP_XML_EVENT_END && depth == 0 && index == 0) {
		ret = netCtlUpnpSoapCheckAttributes(upnp, g_soapEnvelopeAttributes);
		if (ret < 0)
			return ret;
		upnp->soap.flags |= UPNP_SOAP_ENVELOPE_END;
	}
	return 0;
}

int netCtlUpnpDeviceCallback(UpnpXmlParser *parser, int event, int depth, int index,
			     const char *text, const char *value, void *arg);
int netCtlUpnpServiceCallback(UpnpXmlParser *parser, int event, int depth, int index,
			      const char *text, const char *value, void *arg);

__attribute__((noinline))
int netCtlUpnpDescInit(NetCtlUpnp *upnp, char *controlUrl, SceSize controlUrlSize)
{
	int ret;

	upnp->services = 0;
	sceClibMemset(&upnp->desc, 0, sizeof(upnp->desc));
	upnp->desc.controlUrl = controlUrl;
	upnp->desc.controlUrlSize = controlUrlSize;
	ret = netCtlUpnpXmlInit(&upnp->desc.device, &g_deviceSchema, netCtlUpnpDeviceCallback,
				upnp);
	if (ret < 0)
		return ret;
	return netCtlUpnpXmlInit(&upnp->desc.service, &g_serviceSchema,
				 netCtlUpnpServiceCallback, upnp);
}

__attribute__((noinline))
int netCtlUpnpDescParse(NetCtlUpnp *upnp, const char *data, SceSize size, int last)
{
	int ret;

	ret = netCtlUpnpXmlParse(&upnp->desc.device, data, size);
	if (ret < 0)
		return ret;
	return netCtlUpnpXmlParse(&upnp->desc.service, data, size);
}

__attribute__((noinline))
int netCtlUpnpDescResult(NetCtlUpnp *upnp)
{
	int ret = 0;

	if ((upnp->desc.flags & UPNP_DESC_DONE) == 0)
		ret = UPNP_ERROR_INCOMPLETE_XML;
	else if (upnp->services == 0)
		ret = UPNP_ERROR_NO_SERVICE;
	return ret;
}

/* No usable URLBase: the base is the directory of the description path. */
static inline __attribute__((always_inline)) int netCtlUpnpSetUrlBase(NetCtlUpnp *upnp)
{
	char *end;

	if (sceClibStrnlen(upnp->path, UPNP_BUFFER_SIZE) >= UPNP_BUFFER_SIZE)
		return UPNP_ERROR_BUFFER_TOO_SMALL;
	sceClibStrncpy(upnp->urlBase, upnp->path, UPNP_BUFFER_SIZE);
	end = upnp->urlBase + sceClibStrnlen(upnp->urlBase, UPNP_BUFFER_SIZE);
	while (upnp->urlBase < end && end[-1] != '/')
		*--end = '\0';
	return 0;
}

__attribute__((noinline))
int netCtlUpnpDeviceCallback(UpnpXmlParser *parser, int event, int depth, int index,
			     const char *text, const char *value, void *arg)
{
	NetCtlUpnp *upnp = arg;
	int ret;

	if (event == UPNP_XML_EVENT_TEXT && depth == 1 && index == 0) {
		upnp->desc.flags |= UPNP_DESC_URL_BASE;
		ret = 0;
		if (text == NULL || netCtlUpnpParseUrl((char *)text, &upnp->gateway, &upnp->port,
						       upnp->urlBase, UPNP_BUFFER_SIZE) < 0)
			ret = netCtlUpnpSetUrlBase(upnp);
		if (ret < 0)
			return ret;
	}
	if (event == UPNP_XML_EVENT_TEXT && depth == 2 && index == 0 &&
	    parser->index[depth - 2] == 0 && parser->index[depth - 1] == 1) {
		sceClibStrncpy(upnp->modelName, text, sizeof(upnp->modelName));
		upnp->modelName[sizeof(upnp->modelName) - 1] = '\0';
	}
	if (event == UPNP_XML_EVENT_TEXT && depth == 2 && index == 1 &&
	    parser->index[depth - 2] == 0 && parser->index[depth - 1] == 1) {
		sceClibStrncpy(upnp->friendlyName, text, sizeof(upnp->friendlyName));
		upnp->friendlyName[sizeof(upnp->friendlyName) - 1] = '\0';
	}
	if (event == UPNP_XML_EVENT_END && depth == 0 && index == 0) {
		if ((upnp->desc.flags & UPNP_DESC_URL_BASE) == 0) {
			ret = netCtlUpnpSetUrlBase(upnp);
			if (ret < 0)
				return ret;
		}
		upnp->desc.flags |= UPNP_DESC_DONE;
	}
	return 0;
}

/* Makes the control URL absolute (path on the gateway). */
static inline __attribute__((always_inline))
int netCtlUpnpSetControlUrl(NetCtlUpnp *upnp, char *url, char *dest)
{
	SceUInt16 port;

	if (netCtlUpnpParseUrl(url, &upnp->gateway, &port, upnp->buf, UPNP_BUFFER_SIZE) >= 0) {
		upnp->port = port;
		if (sceClibStrnlen(upnp->buf, UPNP_BUFFER_SIZE) >= UPNP_BUFFER_SIZE)
			return UPNP_ERROR_BUFFER_TOO_SMALL;
		sceClibStrncpy(dest, upnp->buf, UPNP_BUFFER_SIZE);
		return 0;
	}
	if (*url == '/') {
		if (sceClibStrnlen(url, UPNP_BUFFER_SIZE) >= UPNP_BUFFER_SIZE)
			return UPNP_ERROR_BUFFER_TOO_SMALL;
		sceClibStrncpy(dest, url, UPNP_BUFFER_SIZE);
		return 0;
	}
	if (sceClibSnprintf(dest, UPNP_BUFFER_SIZE, "%s%s", upnp->urlBase, url) < UPNP_BUFFER_SIZE)
		return 0;
	return UPNP_ERROR_BUFFER_TOO_SMALL;
}

__attribute__((noinline))
int netCtlUpnpServiceCallback(UpnpXmlParser *parser, int event, int depth, int index,
			      const char *text, const char *value, void *arg)
{
	NetCtlUpnp *upnp = arg;
	int ret;

	if (event == UPNP_XML_EVENT_START && depth == 0 && index == 0)
		upnp->desc.flags &= ~(UPNP_DESC_SERVICE_TYPE | UPNP_DESC_WAN_IP |
				      UPNP_DESC_WAN_PPP | UPNP_DESC_CONTROL_URL);
	if (event == UPNP_XML_EVENT_TEXT && depth == 1 && index == 0) {
		upnp->desc.flags |= UPNP_DESC_SERVICE_TYPE;
		if (sceClibStrncmp(g_upnpServiceTypes[UPNP_SERVICE_WAN_IP], text,
				   UPNP_XML_VALUE_SIZE) == 0)
			upnp->desc.flags |= UPNP_DESC_WAN_IP;
		else if (sceClibStrncmp(g_upnpServiceTypes[UPNP_SERVICE_WAN_PPP], text,
					UPNP_XML_VALUE_SIZE) == 0)
			upnp->desc.flags |= UPNP_DESC_WAN_PPP;
	}
	if (event == UPNP_XML_EVENT_TEXT && depth == 1 && index == 1) {
		upnp->desc.flags |= UPNP_DESC_CONTROL_URL;
		sceClibStrncpy(upnp->desc.controlUrl, text, upnp->desc.controlUrlSize);
		upnp->desc.controlUrl[upnp->desc.controlUrlSize - 1] = '\0';
	}
	if (event == UPNP_XML_EVENT_END && depth == 0 && index == 0) {
		if (upnp->desc.flags & UPNP_DESC_WAN_IP) {
			if ((upnp->services & (1 << UPNP_SERVICE_WAN_IP)) == 0) {
				ret = netCtlUpnpSetControlUrl(upnp, upnp->desc.controlUrl,
							      upnp->controlUrl[UPNP_SERVICE_WAN_IP]);
				if (ret < 0)
					return ret;
				upnp->services |= 1 << UPNP_SERVICE_WAN_IP;
			}
		} else if ((upnp->desc.flags & UPNP_DESC_WAN_PPP) &&
			   (upnp->services & (1 << UPNP_SERVICE_WAN_PPP)) == 0) {
			ret = netCtlUpnpSetControlUrl(upnp, upnp->desc.controlUrl,
						      upnp->controlUrl[UPNP_SERVICE_WAN_PPP]);
			if (ret < 0)
				return ret;
			upnp->services |= 1 << UPNP_SERVICE_WAN_PPP;
		}
	}
	return 0;
}
