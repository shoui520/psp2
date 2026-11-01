/*
 * SceNetCtl UPnP client: the gateway's device description, parsed for its URL base and the
 * control URL of its WANIPConnection or WANPPPConnection service.
 */

#include "netctl_internal.h"
#include "upnp.h"

int netCtlUpnpParseUrl(char *url, const SceNetInAddr *gateway, SceUInt16 *port, char *path,
		       SceSize pathSize);

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
	    parser->index[0] == 0 && parser->index[1] == 1) {
		sceClibStrncpy(upnp->modelName, text, sizeof(upnp->modelName));
		upnp->modelName[sizeof(upnp->modelName) - 1] = '\0';
	}
	if (event == UPNP_XML_EVENT_TEXT && depth == 2 && index == 1 &&
	    parser->index[0] == 0 && parser->index[1] == 1) {
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
