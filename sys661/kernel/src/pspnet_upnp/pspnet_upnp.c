#include "upnp_int.h"

static s32 g_upnp_initialized;
static s32 g_xml_found_service;
static s32 g_xml_service_depth;
static s32 g_xml_service_idx;
static s32 g_xml_service_type;
static UpnpCtx g_upnp;
static UpnpHandler g_upnp_handlers[3];
static s32 g_apctl_connected;
static const char *const g_proto_names[2] = { "UDP", "TCP" };
static const char *const g_soap_actions[4] = {
	"AddPortMapping",
	"DeletePortMapping",
	"GetExternalIPAddress",
	"GetStatusInfo"
};
static const char *const g_service_types[2] = {
	"urn:schemas-upnp-org:service:WANIPConnection:1",
	"urn:schemas-upnp-org:service:WANPPPConnection:1"
};

static const XmlTagEntry g_desc_root[] = { { "root", 1 }, { "", 1 } };
static const XmlTagEntry g_desc_device[] = { { "URLBase", 1 }, { "device", 0 }, { "", 1 } };
static const XmlTagEntry g_desc_dev_children[] = { { "serviceList", 0 }, { "deviceList", 0 }, { "", 1 } };
static const XmlTagEntry g_desc_sub_device[] = { { "service", 0 }, { "device", 0 }, { "", 1 } };
static const XmlTagEntry g_desc_sub_children[] = { { "serviceList", 0 }, { "deviceList", 0 }, { "", 1 } };
static const XmlTagEntry g_desc_wan_device[] = { { "service", 0 }, { "device", 0 }, { "", 1 } };
static const XmlTagEntry g_desc_wan_children[] = { { "serviceType", 1 }, { "controlURL", 1 }, { "serviceList", 0 }, { "", 1 } };
static const XmlTagEntry g_desc_service[] = { { "service", 0 }, { "", 1 } };
static const XmlTagEntry g_desc_service_fields[] = { { "serviceType", 1 }, { "controlURL", 1 }, { "", 1 } };

static const XmlTagEntry *const g_desc_levels[9] = {
	g_desc_root, g_desc_device, g_desc_dev_children, g_desc_sub_device, g_desc_sub_children,
	g_desc_wan_device, g_desc_wan_children, g_desc_service, g_desc_service_fields
};
static const XmlSchema g_desc_schema = { g_desc_levels, 9 };

static const XmlTagEntry g_extip_root[] = { { "NewExternalIPAddress", 1 }, { "", 1 } };
static const XmlTagEntry *const g_extip_levels[1] = { g_extip_root };
static const XmlSchema g_extip_schema = { g_extip_levels, 1 };

static const XmlTagEntry g_soap_envelope[] = { { "Envelope", 0 }, { "", 1 } };
static const XmlTagEntry g_soap_body[] = { { "Body", 0 }, { "", 1 } };
static const XmlTagEntry g_soap_response[] = { { "AddPortMappingResponse", 0 }, { "Fault", 0 }, { "", 1 } };
static const XmlTagEntry g_soap_detail[] = { { "detail", 0 }, { "", 1 } };
static const XmlTagEntry g_soap_upnp_error[] = { { "UPnPError", 0 }, { "", 1 } };
static const XmlTagEntry g_soap_error_code[] = { { "errorCode", 1 }, { "", 1 } };

static const XmlTagEntry *const g_soap_levels[6] = {
	g_soap_envelope, g_soap_body, g_soap_response, g_soap_detail, g_soap_upnp_error, g_soap_error_code
};
static const XmlSchema g_soap_schema = { g_soap_levels, 6 };

static const u8 g_stun_hmac_key[20] = {
	0xa0, 0x07, 0x0e, 0x61, 0xad, 0x3f, 0xca, 0x50,
	0x6d, 0xb5, 0xad, 0x3d, 0x57, 0x13, 0x32, 0xce,
	0xa7, 0x32, 0x0d, 0x9e
};

/* Forward declarations */
static s32 soapWriteEnvelopeHeader(char *buf, s32 bufsize, s32 action, s32 svc_type);
static s32 soapWriteEnvelopeFooter(char *buf, s32 bufsize, s32 action);
static s32 upnpSoapFaultXmlCallback(XmlParser *p, s32 event, s32 depth, s32 idx, const char *text, const char *attr_val, void *arg);
static s32 upnpResolveControlUrl(const char *url, u32 *gw_ip, u16 *gw_port, const char *base_path, char *tmp_buf, u32 tmp_size, char *out_path, u32 out_size);
static s32 httpBuildGetRequest(char *buf, s32 bufsize, const SockAddrIn *addr, const char *path);
static s32 httpRecvResponse(s32 sock, char *buf, u32 bufsize, XmlParser *parser);
static s32 httpBuildSoapPostHeader(char *buf, s32 bufsize, const SockAddrIn *addr, const char *path, s32 action, s32 svc_type, s32 content_len);
static s32 httpConnectTimeout(s32 sock, const SockAddrIn *addr, u32 timeout_us);
static s32 httpRecvBodyToXml(s32 sock, char *buf, u32 initial_len, u32 content_len, u32 buf_cap, XmlParser *parser);
static s32 upnpInitInternal(s32 stack_size, s32 prio);

static s32 upnpAddHandlerInternal(UpnpHandlerCb cb, void *arg, s32 *out_id);
static s32 upnpGetNatInfoExInternal(UpnpNatInfoEx *info);
static s32 upnpMapErrorCode(s32 *out_err, s32 upnp_code);
static s32 upnpNotifyHandlers(UpnpCtx *ctx, s32 prev_state);
static void upnpTermInternal(void);
static s32 upnpDelHandlerInternal(s32 id);
static s32 upnpStartInternal(s32 ext_flag);
static s32 upnpStopInternal(void);
static s32 upnpAbortInternal(void);
static s32 upnpGetStateInternal(s32 *out_state);
static s32 upnpGetNatInfoInternal(UpnpNatInfo *info);
static s32 upnpGetLastErrorInternal(s32 *out_err);
static s32 upnpSetEventBits(u32 bits);
static s32 upnpClearEventBits(u32 bits);
static s32 upnpWaitEventBits(u32 bits, u32 *out_bits);
static s32 upnpPollEventBits(u32 bits, u32 *out_bits);
static s32 upnpReadRegistryString(const char *cat, const char *key, char *out, s32 out_size);
static s32 ssdpParseLocationUrl(char *url, u32 *expected_ip, u16 *out_port, char *out_path, u32 out_path_size);
static s32 ssdpRecvResponse(s32 sock, char *buf, u32 bufsize, u32 *gw_ip, u16 *gw_port, char *desc_path, u32 desc_path_size);
static s32 ssdpFindLocationHeader(char *buf, u32 len);
static s32 ssdpCheckStatusOk(char *line);
static s32 ssdpBuildMSearch(char *buf, s32 bufsize);
static s32 ssdpDiscoverGateway(s32 sock, char *buf, u32 bufsize, u32 *gw_ip, u16 *gw_port, char *desc_path, u32 desc_path_size);
static s32 upnpStateDiscoverIgd(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);
static s32 upnpStateAddPortMapping(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);
static s32 upnpStateDeletePortMapping(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);
static s32 upnpStateRunStunProbe(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);
static s32 upnpWorkerThread(u32 args, void *argp);
static s32 upnpStateWaitStart(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);
static s32 upnpStateIdle(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);
static s32 upnpStateWaitStop(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);
static s32 upnpStateWaitApctl(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event);

static const UpnpStateFn g_upnp_state_handlers[8] = {
	upnpStateWaitStart, upnpStateDiscoverIgd, upnpStateAddPortMapping, upnpStateDeletePortMapping,
	upnpStateIdle, upnpStateRunStunProbe, upnpStateWaitStop, upnpStateWaitApctl
};
static void upnpApctlHandler(s32 prev_state, s32 new_state, s32 event, s32 err, void *arg);
static s32 upnpExecSoapAction(UpnpCtx *ctx, s32 svc_idx, s32 action, u16 ext_port, s32 proto, u16 int_port, s32 *out_err, u32 timeout_us);
static void upnpStunCallback(s32 stun_id, void *arg, s32 event, void *data);
s32 sceNetStun_EBF3C24D(s32 stack_size, s32 prio, const char *host, u16 port, const StunConfig *cfg, StunCallback cb, s32 *out_id);
s32 sceNetStun_4403D9E7(s32 id);
s32 sceNetStun_A0CF3A47(s32 id, void *arg);
s32 sceNetStun_FBF60C6B(s32 id, u16 port, void *arg);
s32 sceNetStun_40971567(s32 id, void *arg);

/* =========================================================================
 * sceNetUpnp exports (0x0fa0..0x1394)
 * ========================================================================= */

s32 sceNetUpnp_E24220B5(s32 stack_size, s32 prio)
{
	s32 ret;
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (g_upnp_initialized > 0)
		return (s32)0x80411602;
	ret = upnpInitInternal(stack_size, prio);
	if (ret < 0)
		return ret;
	g_upnp_initialized = 1;
	return ret;
}

s32 sceNetUpnp_540491EF(void)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return 0;
	upnpTermInternal();
	g_upnp_initialized = 0;
	return 0;
}

s32 sceNetUpnp_8513C6D1(UpnpHandlerCb cb, void *arg, s32 *out_id)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	return upnpAddHandlerInternal(cb, arg, out_id);
}

s32 sceNetUpnp_74A51211(s32 id)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	return upnpDelHandlerInternal(id);
}

s32 sceNetUpnp_3432B2E5(void)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return (s32)0x80411601;
	return upnpStartInternal(0);
}

s32 sceNetUpnp_FDA78483(void)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return (s32)0x80411601;
	return upnpStartInternal(1);
}

s32 sceNetUpnp_3E32ED9E(void)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return (s32)0x80411601;
	return upnpStopInternal();
}

s32 sceNetUpnp_C3A35217(s32 *out_state)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return (s32)0x80411601;
	return upnpGetStateInternal(out_state);
}

s32 sceNetUpnp_27045362(UpnpNatInfo *info)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return (s32)0x80411601;
	return upnpGetNatInfoInternal(info);
}

s32 sceNetUpnp_1038E77A(UpnpNatInfoEx *info)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return (s32)0x80411601;
	return upnpGetNatInfoExInternal(info);
}

s32 sceNetUpnp_EAF1C841(s32 *out_err)
{
	if (_sceKernelCheckThreadStack() < 0x7f0)
		return (s32)0x80410005;
	if (!g_upnp_initialized)
		return (s32)0x80411601;
	return upnpGetLastErrorInternal(out_err);
}

/* =========================================================================
 * SOAP request builders & XML response callbacks (0x1398..0x2c5b)
 * ========================================================================= */

static s32 soapBuildAddPortMappingBody(char *buf, s32 bufsize, const char *client_ip, s32 svc_type, u16 ext_port, s32 proto, u16 int_port)
{
	char line[0x80];
	char *p;
	s32 rem, n;

	n = soapWriteEnvelopeHeader(buf, bufsize, 0, svc_type);
	if (n < 0)
		return n;
	p = buf + n;
	rem = bufsize - n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%s</%s>\r\n", "NewRemoteHost", "", "NewRemoteHost");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%d</%s>\r\n", "NewExternalPort", (u32)ext_port, "NewExternalPort");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%s</%s>\r\n", "NewProtocol", g_proto_names[proto], "NewProtocol");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%d</%s>\r\n", "NewInternalPort", (u32)int_port, "NewInternalPort");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%s</%s>\r\n", "NewInternalClient", client_ip, "NewInternalClient");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%d</%s>\r\n", "NewEnabled", 1, "NewEnabled");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%s:%d to %d (%s)</%s>\r\n",
		"NewPortMappingDescription", client_ip, (u32)int_port, (u32)ext_port,
		g_proto_names[proto], "NewPortMappingDescription");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%d</%s>\r\n", "NewLeaseDuration", 0, "NewLeaseDuration");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	n = soapWriteEnvelopeFooter(p, rem, 0);
	if (n < 0)
		return n;
	return (s32)((p + n) - buf);
}

static s32 soapBuildDeletePortMappingBody(char *buf, s32 bufsize, s32 svc_type, u16 ext_port, s32 proto)
{
	char line[0x80];
	char *p;
	s32 rem, n;

	n = soapWriteEnvelopeHeader(buf, bufsize, 1, svc_type);
	if (n < 0)
		return n;
	p = buf + n;
	rem = bufsize - n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%s</%s>\r\n", "NewRemoteHost", "", "NewRemoteHost");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%d</%s>\r\n", "NewExternalPort", (u32)ext_port, "NewExternalPort");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, " <%s>%s</%s>\r\n", "NewProtocol", g_proto_names[proto], "NewProtocol");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	n = soapWriteEnvelopeFooter(p, rem, 1);
	if (n < 0)
		return n;
	return (s32)((p + n) - buf);
}

static s32 soapBuildActionBody(char *buf, s32 bufsize, s32 action, s32 svc_type)
{
	s32 n = soapWriteEnvelopeHeader(buf, bufsize, action, svc_type);
	if (n < 0)
		return n;
	s32 m = soapWriteEnvelopeFooter(buf + n, bufsize - n, action);
	if (m < 0)
		return m;
	return n + m;
}

static s32 soapWriteEnvelopeHeader(char *buf, s32 bufsize, s32 action, s32 svc_type)
{
	char line[0x80];
	char *p = buf;
	s32 rem = bufsize;
	s32 n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "<s:%s xmlns:s=\"%s\" s:encodingStyle=\"%s\">\r\n",
		"Envelope",
		"http://schemas.xmlsoap.org/soap/envelope/",
		"http://schemas.xmlsoap.org/soap/encoding/");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "   <s:%s>\r\n", "Body");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "      <u:%s xmlns:u=\"%s\">\r\n",
		g_soap_actions[action], g_service_types[svc_type]);
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	return (s32)(p - buf);
}

static s32 soapWriteEnvelopeFooter(char *buf, s32 bufsize, s32 action)
{
	char line[0x40];
	char *p = buf;
	s32 rem = bufsize;
	s32 n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "      </u:%s>\r\n", g_soap_actions[action]);
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "   </s:%s>\r\n", "Body");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "</s:%s>\r\n", "Envelope");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	return (s32)(p - buf);
}

static s32 upnpDescXmlCallback(XmlParser *p, s32 event, s32 depth, s32 idx, const char *text, const char *attr_val, void *arg)
{
	UpnpCtx *ctx = (UpnpCtx *)arg;
	s32 ret;

	if (event == 3 && depth == 1 && idx == 0) {
		ssdpParseLocationUrl((char *)text, &ctx->gw_ip, &ctx->gw_port, ctx->url_base, 0x400);
		ctx->xml_parse_flags |= 1;
	}

	if (event == 3 && (depth == 4 || depth == 6 || depth == 8) && idx == 0) {
		if (sceNetStrcmp("urn:schemas-upnp-org:service:WANIPConnection:1", text) == 0) {
			g_xml_service_type = 0;
			g_xml_found_service = 1;
			g_xml_service_depth = depth;
			g_xml_service_idx = idx;
		} else if (sceNetStrcmp("urn:schemas-upnp-org:service:WANPPPConnection:1", text) == 0) {
			g_xml_service_type = 1;
			g_xml_found_service = 1;
			g_xml_service_depth = depth;
			g_xml_service_idx = idx;
		} else {
			return 0;
		}
	}

	if (event == 3 && g_xml_found_service == 1 && depth == g_xml_service_depth && idx == g_xml_service_idx + 1) {
		if (g_xml_service_type == 0) {
			if (ctx->found_svc_mask & 1)
				return 0;
			ret = upnpResolveControlUrl(text, &ctx->gw_ip, &ctx->gw_port, ctx->url_base,
				ctx->http_buf, 0x400, ctx->ctrl_url[0], 0x400);
			if (ret < 0)
				return ret;
			ctx->found_svc_mask |= 1;
		} else if (g_xml_service_type == 1) {
			if (ctx->found_svc_mask & 2)
				return 0;
			ret = upnpResolveControlUrl(text, &ctx->gw_ip, &ctx->gw_port, ctx->url_base,
				ctx->http_buf, 0x400, ctx->ctrl_url[1], 0x400);
			if (ret < 0)
				return ret;
			ctx->found_svc_mask |= 2;
		}
		if (ctx->found_svc_mask == 0)
			return (s32)0x8041160b;
		g_xml_found_service = 0;
	}

	if (event == 2 && depth == 0 && idx == 0) {
		if (!(ctx->xml_parse_flags & 1))
			return (s32)0x8041168d;
		ctx->xml_parse_flags |= 0x80000000u;
	}
	return 0;
}

static s32 upnpInitDescXmlParser(UpnpCtx *ctx, XmlParser *parser)
{
	return xmlParserInit(parser, &g_desc_schema, upnpDescXmlCallback, ctx);
}

static s32 upnpResetDescXmlState(void)
{
	return 0;
}

static s32 upnpResolveControlUrl(const char *url, u32 *gw_ip, u16 *gw_port, const char *base_path,
                    char *tmp_buf, u32 tmp_size, char *out_path, u32 out_size)
{
	u16 parsed_port;
	char combined[0x100];
	s32 ret;

	ret = ssdpParseLocationUrl((char *)url, gw_ip, &parsed_port, tmp_buf, tmp_size);
	if (ret >= 0) {
		*gw_port = parsed_port;
		if (sceNetStrlen(tmp_buf) >= out_size)
			return (s32)0x80411685;
		sceNetStrcpy(out_path, tmp_buf);
		return 0;
	}

	if (url[0] == '/') {
		if (sceNetStrlen(url) >= out_size)
			return (s32)0x80411685;
		sceNetStrcpy(out_path, url);
		return 0;
	}

	sceKernelMemset(combined, 0, sizeof(combined));
	ret = sceNetSprintf(combined, "%s%s", base_path, url);
	if (ret >= (s32)out_size)
		return (s32)0x80411685;
	sceNetStrcpy(out_path, combined);
	return 0;
}

static s32 upnpSendSoapRequest(s32 sock, char *buf, u32 bufsize, const SockAddrIn *addr, const char *path,
                    s32 action, s32 svc_type, const char *body, u32 body_len, XmlParser *parser, u32 timeout_us)
{
	s32 ret, hdr_len;

	ret = httpConnectTimeout(sock, addr, timeout_us);
	if (ret < 0)
		return ret;

	hdr_len = httpBuildSoapPostHeader(buf, (s32)bufsize, addr, path, action, svc_type, (s32)body_len);
	if (hdr_len < 0)
		return hdr_len;

	if ((u32)hdr_len + body_len <= bufsize) {
		sceKernelMemcpy(buf + hdr_len, body, body_len);
		ret = sceNetInetSend(sock, buf, (u32)hdr_len + body_len, 0);
		if (ret < 0)
			return ret;
	} else {
		ret = sceNetInetSend(sock, buf, (u32)hdr_len, 0);
		if (ret < 0)
			return ret;
		ret = sceNetInetSend(sock, body, body_len, 0);
		if (ret < 0)
			return ret;
	}

	return httpRecvResponse(sock, buf, bufsize, parser);
}

static s32 upnpExtIpXmlCallback(XmlParser *p, s32 event, s32 depth, s32 idx, const char *text, const char *attr_val, void *arg)
{
	UpnpCtx *ctx = (UpnpCtx *)arg;
	s32 is_root = (depth == 0);
	s32 ret = 0;

	if (((event == 3) & is_root) && idx == 0) {
		if (sceNetStrlen(text) >= 0x10)
			return (s32)0x80411609;
		sceKernelMemset(ctx->ext_ip_str, 0, 0x10);
		sceNetStrcpy(ctx->ext_ip_str, text);
		ctx->xml_parse_flags |= 2;
	}

	if (((event == 2) & is_root) && idx == 0) {
		if (!(ctx->xml_parse_flags & 2))
			return (s32)0x8041168d;
		ctx->xml_parse_flags |= 0x80000000u;
	}
	return ret;
}

static s32 upnpHttpGetXml(s32 sock, char *buf, u32 bufsize, const SockAddrIn *addr, const char *path, XmlParser *parser)
{
	s32 ret;

	ret = sceNetInetConnect(sock, addr, sizeof(SockAddrIn));
	if (ret < 0)
		return ret;

	ret = httpBuildGetRequest(buf, (s32)bufsize, addr, path);
	if (ret < 0)
		return ret;

	ret = sceNetInetSend(sock, buf, (u32)ret, 0);
	if (ret < 0)
		return ret;

	return httpRecvResponse(sock, buf, bufsize, parser);
}

static s32 upnpInitSoapXmlParser(UpnpCtx *ctx, XmlParser *parser, s32 action)
{
	switch (action) {
	case 0:
	case 1:
		return xmlParserInit(parser, &g_soap_schema, upnpSoapFaultXmlCallback, ctx);
	case 2:
		return xmlParserInit(parser, &g_extip_schema, upnpExtIpXmlCallback, ctx);
	default:
		return (s32)0x8041168f;
	}
}

static s32 upnpSoapFaultXmlCallback(XmlParser *p, s32 event, s32 depth, s32 idx, const char *text, const char *attr_val, void *arg)
{
	UpnpCtx *ctx = (UpnpCtx *)arg;
	s32 err = 0;

	if (event == 3 && depth == 5 && idx == 0) {
		s32 code = sceNetStrtoul(text, NULL, 10);
		upnpMapErrorCode(&err, code);
		return err;
	}

	if (event == 2 && depth == 0 && idx == 0) {
		ctx->xml_parse_flags |= 0x80000000u;
	}
	return err;
}

static s32 httpBuildGetRequest(char *buf, s32 bufsize, const SockAddrIn *addr, const char *path)
{
	char ip_str[0x10];
	char line[0x80];
	char *p = buf;
	s32 rem = bufsize;
	s32 n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "GET %s HTTP/1.1\r\n", path);
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	const char *host = sceNetInetInetNtop(2, &addr->sin_addr, ip_str, sizeof(ip_str));
	n = sceNetSprintf(line, "HOST: %s:%d\r\n", host, (u32)bswap16(addr->sin_port));
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "\r\n");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	return (s32)(p - buf);
}

static s32 httpRecvResponse(s32 sock, char *buf, u32 bufsize, XmlParser *parser)
{
	u32 cap = bufsize - 1;
	char *end = buf + cap;
	char *cur = buf;
	u32 buffered = 0;
	s32 discard_line = 0;
	s32 header_done = 0;
	u32 content_len = 0;
	s32 ret;

	*end = '\0';

	for (;;) {
		if (cur >= end) {
			discard_line = 1;
			cur = buf;
			buffered = 0;
		}
		ret = sceNetInetRecv(sock, cur, (u32)(end - cur), 0);
		if (ret < 0)
			return ret;
		if (ret == 0)
			return (s32)0x8041168b;
		buffered += (u32)ret;

		for (;;) {
			s32 line_len = ssdpFindLocationHeader(buf, buffered);
			if (line_len == (s32)0x80411688) {
				cur = buf + buffered;
				break;
			}
			if (line_len < 0)
				return (s32)0x8041168b;

			char *next_line = buf + line_len;
			if (discard_line == 1) {
				discard_line = 0;
			} else if (buf[0] == '\0') {
				header_done = 1;
			} else {
				s32 val_off = ssdpCheckStatusOk(buf);
				if (val_off != (s32)0x80411689) {
					if (val_off < 0)
						return (s32)0x8041168b;
					char *val = buf + val_off;
					if (sceNetStrcasecmp(buf, "Content-Length") == 0) {
						if (sceNetStrlen(val) < 5)
							content_len = (u32)sceNetStrtoul(val, NULL, 10);
					}
				}
			}

			buffered -= (u32)(next_line - buf);
			sceNetMemmove(buf, next_line, buffered);
			if (header_done == 1) {
				ret = httpRecvBodyToXml(sock, buf, buffered, content_len, cap, parser);
				if (ret < 0)
					return ret;
				if (content_len != 0 && (u32)ret != content_len)
					return (s32)0x8041168d;
				return ret;
			}
		}
	}
}

static s32 httpBuildSoapPostHeader(char *buf, s32 bufsize, const SockAddrIn *addr, const char *path,
                    s32 action, s32 svc_type, s32 content_len)
{
	char ip_str[0x10];
	char line[0x80];
	char *p = buf;
	s32 rem = bufsize;
	s32 n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "POST %s HTTP/1.1\r\n", path);
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	const char *host = sceNetInetInetNtop(2, &addr->sin_addr, ip_str, sizeof(ip_str));
	n = sceNetSprintf(line, "HOST: %s:%d\r\n", host, (u32)bswap16(addr->sin_port));
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "Content-Length: %d\r\n", content_len);
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "Content-Type: %s\r\n", "text/xml; charset=\"utf-8\"");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "SOAPACTION: \"%s#%s\"\r\n",
		g_service_types[svc_type], g_soap_actions[action]);
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "\r\n");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	return (s32)(p - buf);
}

static s32 httpConnectTimeout(s32 sock, const SockAddrIn *addr, u32 timeout_us)
{
	FdSet rfds, wfds;
	TimeVal tv;
	s32 nonblock;
	s32 so_err;
	s32 optlen;
	s32 ret;
	u32 elapsed = 0;

	if (timeout_us == 0)
		return sceNetInetConnect(sock, addr, sizeof(SockAddrIn));

	nonblock = 1;
	ret = sceNetInetSetsockopt(sock, 0xffff, 0x1009, &nonblock, 4);
	if (ret < 0)
		return ret;

	ret = sceNetInetConnect(sock, addr, sizeof(SockAddrIn));
	if (ret >= 0)
		return 0;

	if (sceNetInetGetPspError() != (s32)0x80010077)
		return ret;

	while (elapsed < timeout_us) {
		sceKernelMemset(&rfds, 0, sizeof(rfds));
		rfds.bits[sock >> 5] |= (1u << (sock & 31));
		sceKernelMemset(&wfds, 0, sizeof(wfds));
		wfds.bits[sock >> 5] |= (1u << (sock & 31));
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		ret = sceNetInetSelect(sock + 1, &rfds, &wfds, NULL, &tv);
		if (ret < 0)
			return ret;
		if (ret != 0) {
			if ((rfds.bits[sock >> 5] & (1u << (sock & 31))) ||
			    (wfds.bits[sock >> 5] & (1u << (sock & 31)))) {
				so_err = 0;
				optlen = 4;
				ret = sceNetInetGetsockopt(sock, 0xffff, 0x1007, &so_err, &optlen);
				if (ret < 0)
					return ret;
				if (so_err != 0)
					return (s32)(0x80010000u | (u32)so_err);
				break;
			}
		}
		elapsed += 100000;
	}

	if (elapsed >= timeout_us)
		return (s32)0x8041160a;

	nonblock = 0;
	return sceNetInetSetsockopt(sock, 0xffff, 0x1009, &nonblock, 4);
}

static s32 httpRecvBodyToXml(s32 sock, char *buf, u32 initial_len, u32 content_len, u32 buf_cap, XmlParser *parser)
{
	u32 total = 0;
	s32 ret;

	if (initial_len != 0) {
		u32 chunk = initial_len;
		if (content_len != 0 && chunk > content_len)
			chunk = content_len;
		ret = xmlParserFeed(parser, buf, (s32)chunk);
		if (ret < 0)
			return ret;
		total = chunk;
	}

	while (content_len == 0 || total < content_len) {
		ret = sceNetInetRecv(sock, buf, buf_cap, 0);
		if (ret < 0)
			return ret;
		if (ret == 0)
			return (s32)total;
		u32 chunk = (u32)ret;
		if (content_len != 0 && total + chunk > content_len)
			chunk = content_len - total;
		ret = xmlParserFeed(parser, buf, (s32)chunk);
		total += chunk;
		if (ret < 0)
			return ret;
	}

	return (s32)total;
}

/* =========================================================================
 * UPnP lifecycle, handlers, NAT info, event flag & registry helpers (0x2d74..0x44a7)
 * ========================================================================= */

static s32 upnpInitInternal(s32 stack_size, s32 prio)
{
	char env_buf[9];
	UpnpCtx *ctx = &g_upnp;
	s32 ret;

	sceKernelMemset(g_upnp_handlers, 0, sizeof(g_upnp_handlers));
	sceKernelMemset(ctx, 0, sizeof(UpnpCtx));
	sceKernelMemset(env_buf, 0, sizeof(env_buf));
	sceKernelMemset(ctx->stun_host, 0, sizeof(ctx->stun_host));

	do {
		ret = upnpReadRegistryString("/CONFIG/NP", "env", env_buf, sizeof(env_buf));
		if (ret >= 0)
			sceNetSprintf(ctx->stun_host, "%s.%s.%s", "us", env_buf, "stun.playstation.net");
		if (ret < 0)
			break;

		ctx->stack_size = stack_size;
		ctx->thread_prio = prio;
		ctx->state = 0;
		ctx->apctl_handler_id = -1;
		ctx->sock_fd = -1;
		ctx->thread_id = -1;
		ctx->evf_id = -1;
		ctx->mutex_id = -1;

		ret = sceKernelCreateEventFlag("SceNetUpnpEvf", 0, 0, NULL);
		if (ret < 0)
			break;
		ctx->evf_id = ret;

		ret = sceKernelCreateMutex("SceNetUpnpMutex", 0x200, 0, NULL);
		if (ret < 0)
			break;
		ctx->mutex_id = ret;
		ctx->flags |= 8;

		ret = sceKernelCreateThread("SceNetUpnpThread", upnpWorkerThread, ctx->thread_prio, ctx->stack_size, 0, NULL);
		if (ret < 0)
			break;
		ctx->thread_id = ret;

		ret = sceKernelStartThread(ctx->thread_id, 4, &ctx);
		if (ret < 0)
			break;

		ret = stunInitGlobal();
		if (ret < 0)
			break;

		return 0;
	} while (0);

	stunTermGlobal();
	if (g_upnp.thread_id != -1) {
		sceKernelWaitThreadEnd(g_upnp.thread_id, NULL);
		sceKernelDeleteThread(g_upnp.thread_id);
		sceNetFreeThreadinfo(g_upnp.thread_id);
		g_upnp.thread_id = -1;
	}
	if (g_upnp.evf_id != -1) {
		sceKernelDeleteEventFlag(g_upnp.evf_id);
		g_upnp.evf_id = -1;
	}
	if (g_upnp.flags & 8) {
		sceKernelDeleteMutex(g_upnp.mutex_id);
		g_upnp.mutex_id = -1;
		g_upnp.flags &= ~8u;
	}
	return ret;
}

static s32 upnpAddHandlerInternal(UpnpHandlerCb cb, void *arg, s32 *out_id)
{
	s32 ret, i;

	if (cb == NULL)
		return (s32)0x80411605;

	ret = sceKernelLockMutex(g_upnp.mutex_id, 1, NULL);
	if (ret < 0)
		return ret;

	for (i = 0; i < 3; i++) {
		if (g_upnp_handlers[i].cb == NULL)
			break;
	}
	if (i == 3) {
		ret = (s32)0x8041160c;
	} else {
		g_upnp_handlers[i].id = i;
		g_upnp_handlers[i].cb = cb;
		g_upnp_handlers[i].arg = arg;
		if (out_id != NULL)
			*out_id = i;
	}
	sceKernelUnlockMutex(g_upnp.mutex_id, 1);
	return ret;
}

static s32 upnpGetNatInfoExInternal(UpnpNatInfoEx *info)
{
	sceKernelMemset(info, 0, sizeof(UpnpNatInfoEx));
	sceKernelMemcpy(&info->stun_mapped_ip, &g_upnp.stun_mapped_ip, 4);
	sceNetInetInetPton(2, g_upnp.ext_ip_str, &info->upnp_ext_ip);

	if ((u32)(g_upnp.state - 5) >= 3u) {
		info->ip_match = 0;
		info->upnp_status = 0;
		info->stun_status = 0;
		return 0;
	}

	if (g_upnp.flags & 4) {
		info->upnp_status = 2;
		info->ext_port = g_upnp.ext_port;
		info->ip_match = (info->stun_mapped_ip.value == info->upnp_ext_ip.value) ? 2 : 1;
	} else {
		info->ext_port = 0x0e4a;
		info->ip_match = 1;
		info->upnp_status = 1;
	}

	if (g_upnp.state == 5) {
		info->stun_status = 0;
		return 0;
	}

	if (!(g_upnp.flags & 0x10)) {
		info->stun_status = 1;
		return 0;
	}

	info->stun_status = 2;
	sceKernelMemcpy(info->stun_result, g_upnp.stun_result, 0x10);
	info->stun_ext_ok = (g_upnp.stun_ext_ok == 1) ? 1 : 0;

	if (info->stun_result[0] == g_upnp.client_ip) {
		info->nat_type = 1;
	} else if (info->ip_match == 2) {
		info->nat_type = 2;
	} else if (info->stun_result[1] != 0) {
		info->nat_type = 3;
	} else {
		info->nat_type = 2;
	}
	return 0;
}

static s32 upnpMapErrorCode(s32 *out_err, s32 upnp_code)
{
	u32 err;
	switch (upnp_code) {
	case 401: err = 0x80411671u; break;
	case 402: err = 0x80411672u; break;
	case 404: err = 0x80411673u; break;
	case 501: err = 0x80411674u; break;
	case 714: err = 0x80411675u; break;
	case 715: err = 0x80411676u; break;
	case 716: err = 0x80411677u; break;
	case 718: err = 0x80411678u; break;
	case 724: err = 0x80411679u; break;
	case 725: err = 0x8041167au; break;
	case 726: err = 0x8041167bu; break;
	case 727: err = 0x8041167cu; break;
	default:  err = 0x8041167du; break;
	}
	*out_err = (s32)err;
	return 0;
}

static s32 upnpNotifyHandlers(UpnpCtx *ctx, s32 prev_state)
{
	s32 ret, i;

	ret = sceKernelLockMutex(ctx->mutex_id, 1, NULL);
	if (ret < 0)
		return ret;

	for (i = 0; i < 3; i++) {
		if (g_upnp_handlers[i].cb != NULL) {
			s32 ev = ctx->event_code;
			s32 err = (ev == 4 || ev == 8 || ev == 9) ? ctx->last_error : 0;
			g_upnp_handlers[i].cb(prev_state, ctx->state, ev, err, g_upnp_handlers[i].arg);
		}
	}

	sceKernelUnlockMutex(ctx->mutex_id, 1);
	return 0;
}

static void upnpTermInternal(void)
{
	if (g_upnp.flags & 1) {
		if (g_upnp.sock_fd >= 0)
			sceNetInetSocketAbort(g_upnp.sock_fd);
		sceKernelSetEventFlag(g_upnp.evf_id, 4);
	}
	sceKernelSetEventFlag(g_upnp.evf_id, 1);
	stunTermGlobal();

	if (g_upnp.thread_id != -1) {
		sceKernelWaitThreadEnd(g_upnp.thread_id, NULL);
		sceKernelDeleteThread(g_upnp.thread_id);
		sceNetFreeThreadinfo(g_upnp.thread_id);
		g_upnp.thread_id = -1;
	}
	if (g_upnp.evf_id != -1) {
		sceKernelDeleteEventFlag(g_upnp.evf_id);
		g_upnp.evf_id = -1;
	}
	if (g_upnp.flags & 8) {
		sceKernelDeleteMutex(g_upnp.mutex_id);
		g_upnp.mutex_id = -1;
		g_upnp.flags &= ~8u;
	}
}

static s32 upnpDelHandlerInternal(s32 id)
{
	s32 ret;

	ret = sceKernelLockMutex(g_upnp.mutex_id, 1, NULL);
	if (ret < 0)
		return ret;

	if ((u32)id >= 3u) {
		ret = (s32)0x8041160d;
	} else if (g_upnp_handlers[id].cb == NULL) {
		ret = (s32)0x8041160e;
	} else {
		g_upnp_handlers[id].cb = NULL;
	}

	sceKernelUnlockMutex(g_upnp.mutex_id, 1);
	return ret;
}

static s32 upnpStartInternal(s32 ext_flag)
{
	if (g_upnp.flags & 1)
		return (s32)0x80411603;

	g_upnp.flags |= 1;
	if (ext_flag == 1)
		g_upnp.flags |= 0x800;
	else
		g_upnp.flags &= ~0x800u;

	g_upnp.use_upnp = 1;
	sceKernelSetEventFlag(g_upnp.evf_id, 2);
	return 0;
}

static s32 upnpStopInternal(void)
{
	if (g_upnp.sock_fd >= 0)
		sceNetInetSocketAbort(g_upnp.sock_fd);
	sceKernelSetEventFlag(g_upnp.evf_id, 4);
	return 0;
}

static s32 upnpAbortInternal(void)
{
	if (g_upnp.sock_fd >= 0)
		sceNetInetSocketAbort(g_upnp.sock_fd);
	sceKernelSetEventFlag(g_upnp.evf_id, 0x20);
	return 0;
}

static s32 upnpGetStateInternal(s32 *out_state)
{
	*out_state = g_upnp.state;
	return 0;
}

static s32 upnpGetNatInfoInternal(UpnpNatInfo *info)
{
	UpnpNatInfoEx ex;
	s32 ret;

	sceKernelMemset(&ex, 0, sizeof(ex));
	ret = upnpGetNatInfoExInternal(&ex);
	if (ret < 0)
		return ret;

	if (info != NULL) {
		info->upnp_status = ex.upnp_status;
		info->stun_status = ex.stun_status;
		info->nat_type = ex.nat_type;
		sceKernelMemcpy(info->stun_mapped_ip, &ex.stun_mapped_ip, 4);
	}
	return ret;
}

static s32 upnpGetLastErrorInternal(s32 *out_err)
{
	*out_err = g_upnp.last_error;
	return 0;
}

static s32 upnpSetEventBits(u32 bits)
{
	return sceKernelClearEventFlag(g_upnp.evf_id, bits);
}

static s32 upnpClearEventBits(u32 bits)
{
	return sceKernelSetEventFlag(g_upnp.evf_id, bits);
}

static s32 upnpWaitEventBits(u32 bits, u32 *out_bits)
{
	return sceKernelWaitEventFlag(g_upnp.evf_id, bits, 0x21, out_bits, NULL);
}

static s32 upnpPollEventBits(u32 bits, u32 *out_bits)
{
	return sceKernelPollEventFlag(g_upnp.evf_id, bits, 0x21, out_bits);
}

static s32 upnpReadRegistryString(const char *cat, const char *key, char *out, s32 out_size)
{
	u32 reg_param[0x44];
	u32 reg_h, cat_h, key_h, key_type, key_size;
	s32 ret;

	sceKernelMemset(reg_param, 0, sizeof(reg_param));
	reg_param[0] = 1;
	reg_param[0x42] = 1;
	reg_param[0x43] = 1;

	ret = sceRegOpenRegistry(reg_param, 2, &reg_h);
	if (ret != 0)
		return ret;

	ret = sceRegOpenCategory(reg_h, cat, 2, &cat_h);
	if (ret != 0) {
		sceRegCloseRegistry(reg_h);
		return ret;
	}

	ret = sceRegGetKeyInfo(cat_h, key, &key_h, &key_type, &key_size);
	if (ret != 0) {
		sceRegCloseCategory(cat_h);
		sceRegCloseRegistry(reg_h);
		return ret;
	}

	if (key_type != 3) {
		sceRegCloseCategory(cat_h);
		sceRegCloseRegistry(reg_h);
		return (s32)0x80411690;
	}

	if (out_size < (s32)key_size) {
		sceRegCloseCategory(cat_h);
		sceRegCloseRegistry(reg_h);
		return (s32)0x80411691;
	}

	ret = sceRegGetKeyValue(cat_h, key_h, out, (u32)out_size);
	if (ret != 0) {
		sceRegCloseCategory(cat_h);
		sceRegCloseRegistry(reg_h);
		return ret;
	}

	if (out[key_size - 1] != '\0')
		out[key_size - 1] = '\0';

	sceRegCloseCategory(cat_h);
	sceRegCloseRegistry(reg_h);
	return 0;
}

static s32 ssdpParseLocationUrl(char *url, u32 *expected_ip, u16 *out_port, char *out_path, u32 out_path_size)
{
	const char *prefix = "http://";
	u32 plen = sceNetStrlen(prefix);
	char *host, *slash, *colon;
	u32 ip;

	if (sceNetStrncmp(url, prefix, plen) != 0)
		return (s32)0x80411689;

	host = url + sceNetStrlen(prefix);
	slash = sceNetStrchr(host, '/');
	if (slash != NULL) {
		if (sceNetStrlen(slash) >= out_path_size)
			return (s32)0x80411689;
		sceNetStrcpy(out_path, slash);
		*slash = '\0';
	} else {
		if (out_path_size < 2)
			return (s32)0x80411689;
		sceNetStrcpy(out_path, "/");
	}

	colon = sceNetStrchr(host, ':');
	if (colon != NULL) {
		char *port_str = colon + 1;
		*colon = '\0';
		if (sceNetStrlen(port_str) >= 6)
			return (s32)0x80411689;
		*out_port = (u16)sceNetStrtoul(port_str, NULL, 10);
	} else {
		*out_port = 80;
	}

	ip = sceNetInetInetAddr(host);
	if (ip == 0xffffffffu)
		return (s32)0x80411689;

	if (ip != *expected_ip)
		return (s32)0x80411689;

	return 0;
}

static s32 ssdpRecvResponse(s32 sock, char *buf, u32 bufsize, u32 *gw_ip, u16 *gw_port,
                    char *desc_path, u32 desc_path_size)
{
	SockAddrIn mcast_addr;
	TimeVal tv;
	FdSet rfds;
	u8 ttl = 4;
	s32 retry = 0;
	s32 ret;

	ret = sceNetInetSetsockopt(sock, 0, 10, &ttl, 1);
	if (ret < 0)
		return ret;

	sceKernelMemset(&mcast_addr, 0, sizeof(mcast_addr));
	mcast_addr.sin_addr = sceNetInetInetAddr("239.255.255.250");
	if (mcast_addr.sin_addr == 0xffffffffu)
		return (s32)0x80411686;
	mcast_addr.sin_family = 2;
	mcast_addr.sin_port = bswap16(1900);

	while ((u32)retry < 6u) {
		ret = ssdpBuildMSearch(buf, (s32)bufsize);
		retry++;
		if (ret < 0)
			return ret;

		ret = sceNetInetSendto(sock, buf, (u32)ret, 0, &mcast_addr, sizeof(mcast_addr));
		if (ret < 0)
			return ret;

		u64 deadline = sceKernelGetSystemTimeWide() + 2000000ULL;
		for (;;) {
			u64 now = sceKernelGetSystemTimeWide();
			if (now >= deadline)
				break;
			u64 rem = deadline - now;
			sceKernelMemset(&tv, 0, sizeof(tv));
			tv.tv_sec = (s32)(rem / 1000000ULL);
			tv.tv_usec = (s32)(rem % 1000000ULL);

			sceKernelMemset(&rfds, 0, sizeof(rfds));
			rfds.bits[sock >> 5] |= (1u << (sock & 31));
			ret = sceNetInetSelect(sock + 1, &rfds, NULL, NULL, &tv);
			if (ret < 0)
				return ret;
			if (ret == 0)
				break;

			ret = ssdpDiscoverGateway(sock, buf, bufsize, gw_ip, gw_port, desc_path, desc_path_size);
			if (ret != (s32)0x8041168a)
				return ret;
		}
	}

	return (s32)0x80411687;
}

static s32 ssdpFindLocationHeader(char *buf, u32 len)
{
	char *nl = sceNetStrchr(buf, '\n');
	if (nl == NULL || (u32)(nl - buf) >= len)
		return (s32)0x80411688;

	char *end = nl;
	*nl = '\0';
	char *next = nl + 1;

	if (end > buf && end[-1] == '\r') {
		end--;
		*end = '\0';
	}
	while (end > buf && (end[-1] == ' ' || end[-1] == '\t')) {
		end--;
		*end = '\0';
	}
	return (s32)(next - buf);
}

static s32 ssdpCheckStatusOk(char *line)
{
	char *colon = sceNetStrchr(line, ':');
	if (colon == NULL)
		return (s32)0x80411689;

	char *p = colon;
	*colon = '\0';
	char *val = colon + 1;

	while (p > line && (p[-1] == ' ' || p[-1] == '\t')) {
		p--;
		*p = '\0';
	}
	while (*val == ' ' || *val == '\t')
		val++;

	return (s32)(val - line);
}

static s32 ssdpBuildMSearch(char *buf, s32 bufsize)
{
	char line[0x40];
	char *p = buf;
	s32 rem = bufsize;
	s32 n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "M-SEARCH * HTTP/1.1\r\n");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "HOST: %s:%d\r\n", "239.255.255.250", 1900);
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "MAN: %s\r\n", "\"ssdp:discover\"");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "MX: %d\r\n", 5);
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "ST: %s\r\n", "urn:schemas-upnp-org:device:InternetGatewayDevice:1");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	rem -= n;

	sceKernelMemset(line, 0, sizeof(line));
	n = sceNetSprintf(line, "\r\n");
	if (((u32)n >> 31) | (n >= rem))
		return (s32)0x80411685;
	sceNetStrcpy(p, line);
	p += n;
	return (s32)(p - buf);
}

static s32 ssdpDiscoverGateway(s32 sock, char *buf, u32 bufsize, u32 *gw_ip, u16 *gw_port,
                    char *desc_path, u32 desc_path_size)
{
	SockAddrIn from;
	s32 fromlen = sizeof(from);
	s32 got_loc = 0;
	s32 got_st = 0;
	s32 ret;

	buf[bufsize - 1] = '\0';
	ret = sceNetInetRecvfrom(sock, buf, bufsize - 1, 0, &from, &fromlen);
	if (ret < 0)
		return ret;

	if (from.sin_addr != *gw_ip)
		return (s32)0x8041168a;

	u32 rem = (u32)ret;
	char *cur = buf;
	s32 line_len = ssdpFindLocationHeader(cur, rem);
	if (line_len < 0)
		return (s32)0x8041168a;
	cur += line_len;
	rem -= (u32)line_len;

	for (;;) {
		char *line = cur;
		line_len = ssdpFindLocationHeader(cur, rem);
		if (line_len < 0)
			return (s32)0x8041168a;
		if (cur[0] == '\0') {
			if (!got_loc || !got_st)
				return (s32)0x8041168a;
			return line_len;
		}
		cur += line_len;

		s32 val_off = ssdpCheckStatusOk(line);
		if (val_off != (s32)0x80411689) {
			if (val_off < 0)
				return (s32)0x8041168a;
			char *val = line + val_off;
			if (sceNetStrcasecmp(line, "Location") == 0) {
				if (ssdpParseLocationUrl(val, gw_ip, gw_port, desc_path, desc_path_size) >= 0)
					got_loc = 1;
			} else if (sceNetStrcasecmp(line, "ST") == 0) {
				if (sceNetStrcmp(val, "urn:schemas-upnp-org:device:InternetGatewayDevice:1") == 0)
					got_st = 1;
			}
		}
		rem -= (u32)(cur - line);
	}
}

/* =========================================================================
 * UPnP state machine (0x44a8..0x541b)
 * ========================================================================= */

static s32 upnpStateDiscoverIgd(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	SockAddrIn addr;
	char ip_str[0x80];
	XmlParser parser;
	s32 apctl_state;
	u32 ev_bits;
	s32 ret = 0;
	s32 stopped = 0;

	ctx->flags &= ~0x734u;
	ctx->ext_port = 0x0e4a;
	ctx->default_tcp_ext_port = 0x244d;
	ctx->default_tcp_int_port = 0x244d;
	ctx->last_error = 0;
	sceKernelMemset(&ctx->stun_mapped_ip, 0, 4);

	do {
		if (ctx->apctl_handler_id < 0) {
			ret = sceNetApctlAddHandler(upnpApctlHandler, ctx);
			if (ret < 0)
				break;
			ctx->apctl_handler_id = ret;
		}

		ret = sceNetApctlGetState(&apctl_state);
		if (ret < 0)
			break;

		if (apctl_state != 4) {
			upnpWaitEventBits(0x1c, &ev_bits);
			if (ev_bits & 4) {
				stopped = 1;
				break;
			}
			if (ev_bits & 8)
				break;
		}

		ret = sceNetApctlGetInfo(8, ip_str);
		if (ret < 0)
			break;
		ctx->client_ip = sceNetInetInetAddr(ip_str);

		if (!ctx->use_upnp) {
			*out_event = 10;
			*next_state = 5;
			return 0;
		}

		ret = sceNetApctlGetInfo(10, ip_str);
		if (ret < 0)
			break;
		ctx->gw_ip = sceNetInetInetAddr(ip_str);

		ret = sceNetInetSocket(2, 2, 0);
		ctx->sock_fd = ret;
		if (ret < 0)
			break;

		ret = ssdpRecvResponse(ctx->sock_fd, ctx->http_buf, 0x400, &ctx->gw_ip, &ctx->gw_port, ctx->desc_path, 0x80);
		sceNetInetClose(ctx->sock_fd);
		ctx->sock_fd = -1;
		if (ret < 0)
			break;

		ret = upnpInitDescXmlParser(ctx, &parser);
		if (ret < 0)
			break;

		ret = sceNetInetSocket(2, 1, 0);
		ctx->sock_fd = ret;
		if (ret < 0)
			break;

		sceKernelMemset(&addr, 0, sizeof(addr));
		addr.sin_family = 2;
		addr.sin_port = bswap16(ctx->gw_port);
		addr.sin_addr = ctx->gw_ip;

		ret = upnpHttpGetXml(ctx->sock_fd, ctx->http_buf, 0x400, &addr, ctx->desc_path, &parser);
		sceNetInetClose(ctx->sock_fd);
		ctx->sock_fd = -1;
		if (ret < 0)
			break;

		*out_event = 1;
		*next_state = 2;
		upnpResetDescXmlState();
		return 0;
	} while (0);

	if (stopped) {
		*out_event = 6;
		*next_state = 0;
		ctx->flags &= ~1u;
		upnpResetDescXmlState();
		return 0;
	}

	if (upnpPollEventBits(0x24, &ev_bits) >= 0) {
		if (ev_bits & 4) {
			*out_event = 6;
			*next_state = 0;
			ctx->flags &= ~1u;
			upnpResetDescXmlState();
			return 0;
		}
		if (ev_bits & 0x20) {
			*out_event = 5;
			*next_state = 1;
			upnpResetDescXmlState();
			return 0;
		}
	}
	*out_event = 9;
	ctx->last_error = ret;
	*next_state = 5;
	upnpResetDescXmlState();
	return 0;
}

static s32 upnpStateAddPortMapping(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	s32 svc_idx = 0;
	s32 soap_err;
	u32 ev_bits;
	s32 ret;

	for (svc_idx = 0; svc_idx < 2; svc_idx++) {
		if (ctx->found_svc_mask & (1u << svc_idx))
			break;
	}

	if (svc_idx < 2)
		ctx->flags |= 0x20;

	for (;;) {
		if (svc_idx == 2) {
			ret = (s32)0x8041160b;
			*out_event = 9;
		} else {
			ret = upnpExecSoapAction(ctx, svc_idx, 2, 0, 0, 0, &soap_err, 0);
			if (ret == (s32)0x80411608) {
				ret = soap_err;
				*out_event = 4;
			} else if (ret < 0) {
				*out_event = 9;
			} else if (ctx->ext_ip_str[0] == '\0') {
				ret = (s32)0x8041167e;
				*out_event = 4;
			} else {
				ctx->active_svc_idx = svc_idx;
				*out_event = 3;
				*next_state = 3;
				return 0;
			}
		}

		if (upnpPollEventBits(0x24, &ev_bits) >= 0) {
			if (ev_bits & 4) {
				*out_event = 6;
				*next_state = 0;
				ctx->flags &= ~1u;
				return 0;
			}
			if (ev_bits & 0x20) {
				*out_event = 5;
				*next_state = 1;
				return 0;
			}
		}
		ctx->last_error = ret;

		svc_idx++;
		while (svc_idx < 2) {
			if (ctx->found_svc_mask & (1u << svc_idx))
				break;
			svc_idx++;
		}
		if (svc_idx >= 2) {
			*next_state = 3;
			return 0;
		}
	}
}

static s32 upnpStateDeletePortMapping(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	s32 soap_err = 0;
	u32 ev_bits;
	s32 ret;

	ret = upnpExecSoapAction(ctx, ctx->active_svc_idx, 0, ctx->ext_port, 0, ctx->ext_port, &soap_err, 0);
	if (ret == (s32)0x80411608) {
		if (soap_err == (s32)0x80411678) {
			u16 port = ctx->ext_port;
			if (port == 0x0e4a) {
				ctx->ext_port = 0xc000;
				*out_event = 11;
				*next_state = 3;
				return 0;
			}
			if ((u16)(port - 0xc000) < 14) {
				ctx->ext_port = port + 1;
				*out_event = 11;
				*next_state = 3;
				return 0;
			}
			ctx->ext_port = 0x0e4a;
		}
		ret = soap_err;
		*out_event = 4;
	} else if (ret < 0) {
		*out_event = 9;
	} else {
		ctx->flags |= 4;
		*out_event = 3;
		*next_state = 5;
		return 0;
	}

	if (upnpPollEventBits(0x24, &ev_bits) >= 0) {
		if (ev_bits & 4) {
			*out_event = 6;
			*next_state = 0;
			ctx->flags &= ~1u;
			return 0;
		}
		if (ev_bits & 0x20) {
			*out_event = 5;
			*next_state = 1;
			return 0;
		}
	}
	ctx->last_error = ret;
	*next_state = 5;
	return 0;
}

static s32 upnpStateRunStunProbe(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	StunConfig cfg;
	s32 stun_id = 0;
	u32 ev_bits;
	void *ext_buf = NULL;
	s32 ret = 0;

	sceKernelMemset(&cfg, 0, sizeof(cfg));
	cfg.username = bswap32(1);
	cfg.use_xor = 1;
	sceKernelMemcpy(cfg.key, g_stun_hmac_key, sizeof(g_stun_hmac_key));

	do {
		if (ctx->flags & 0x800) {
			cfg.ext_data_len = 0x800;
			ext_buf = sceNetMallocInternal(0x800);
			if (ext_buf == NULL) {
				ret = (s32)0x80411604;
				break;
			}
			cfg.has_ext_data = 1;
			cfg.ext_data = ext_buf;
		}

		ret = sceNetStun_EBF3C24D(ctx->stack_size, ctx->thread_prio, ctx->stun_host, 3478, &cfg, upnpStunCallback, &stun_id);
		if (ret < 0) {
			*out_event = 9;
			*next_state = 7;
			ctx->last_error = ret;
			break;
		}

		sceKernelMemset(ctx->stun_result, 0, sizeof(ctx->stun_result));
		upnpSetEventBits(~0xc0u);

		ret = sceNetStun_A0CF3A47(stun_id, ctx);
		if (ret < 0) {
			*out_event = 9;
			*next_state = 7;
			ctx->last_error = ret;
			break;
		}

		upnpWaitEventBits(0x64, &ev_bits);
		if (ev_bits & 4) {
			if (sceNetStun_40971567(stun_id, ctx) >= 0)
				upnpWaitEventBits(0x80, NULL);
			ctx->flags &= ~1u;
			*next_state = 0;
			*out_event = 6;
			break;
		}
		if (ev_bits & 0x20) {
			if (sceNetStun_40971567(stun_id, ctx) >= 0)
				upnpWaitEventBits(0x80, NULL);
			*out_event = 5;
			*next_state = 1;
			break;
		}

		if (ctx->stun_cb_status < 0) {
			ctx->last_error = ctx->stun_cb_status;
			*out_event = 8;
			*next_state = (ctx->flags & 4) ? 6 : 7;
			break;
		}

		if (ctx->flags & 0x800) {
			ctx->stun_ext_ok = 1;
			ret = sceNetStun_FBF60C6B(stun_id, 0, ctx);
			if (ret >= 0) {
				upnpWaitEventBits(0x64, &ev_bits);
				if (ev_bits & 4) {
					if (sceNetStun_40971567(stun_id, ctx) >= 0)
						upnpWaitEventBits(0x80, NULL);
					ctx->flags &= ~1u;
					*next_state = 0;
					*out_event = 6;
					break;
				}
				if (ev_bits & 0x20) {
					if (sceNetStun_40971567(stun_id, ctx) >= 0)
						upnpWaitEventBits(0x80, NULL);
					*out_event = 5;
					*next_state = 1;
					break;
				}
				s32 st = ctx->stun_cb_status;
				if (st == (s32)0x80411703 || st == (s32)0x8041170b || st == (s32)0x8041170c)
					ctx->stun_ext_ok = 0;
			}
		}

		ctx->flags |= 0x10;
		*out_event = 7;
		*next_state = (ctx->flags & 4) ? 6 : 7;
	} while (0);

	if (stun_id != 0)
		sceNetStun_4403D9E7(stun_id);
	if (ext_buf != NULL)
		sceNetFreeInternal(ext_buf);
	return ret;
}

static s32 upnpWorkerThread(u32 args, void *argp)
{
	UpnpCtx *ctx = *(UpnpCtx **)argp;
	s32 next_state, out_event;

	for (;;) {
		s32 cur_state = ctx->state;
		g_upnp_state_handlers[cur_state](ctx, cur_state, &next_state, &out_event);
		if (ctx->state == 0 && (ctx->flags & 2)) {
			if (ctx->apctl_handler_id >= 0) {
				sceNetApctlDelHandler(ctx->apctl_handler_id);
				ctx->apctl_handler_id = -1;
			}
			sceKernelExitThread(0);
			return 0;
		}
		ctx->state = next_state;
		ctx->event_code = out_event;
		upnpNotifyHandlers(ctx, cur_state);
	}
}

static s32 upnpStateWaitStart(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	u32 ev_bits;

	ctx->flags &= ~0x734u;
	upnpWaitEventBits(0xffffffffu, &ev_bits);
	if (ev_bits & 1) {
		ctx->flags |= 2;
	} else if (ev_bits & 4) {
		ctx->flags &= ~1u;
		*next_state = 0;
		*out_event = 6;
	} else if (ev_bits & 2) {
		*next_state = 1;
		*out_event = 0;
	}
	return 0;
}

static s32 upnpStateIdle(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	return 0;
}

static s32 upnpStateWaitStop(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	u32 ev_bits;
	s32 soap_err;

	upnpWaitEventBits(0x24, &ev_bits);
	if (ev_bits & 0x20) {
		*out_event = 5;
		*next_state = 1;
		return 0;
	}

	upnpExecSoapAction(ctx, ctx->active_svc_idx, 1, ctx->ext_port, 0, ctx->ext_port, &soap_err, 1000000);
	*out_event = 6;
	*next_state = 0;
	ctx->flags &= ~1u;
	return 0;
}

static s32 upnpStateWaitApctl(UpnpCtx *ctx, s32 cur_state, s32 *next_state, s32 *out_event)
{
	u32 ev_bits;
	s32 soap_err;

	upnpWaitEventBits(0x24, &ev_bits);
	if (ev_bits & 0x20) {
		*out_event = 5;
		*next_state = 1;
		return 0;
	}

	upnpExecSoapAction(ctx, ctx->active_svc_idx, 1, ctx->ext_port, 0, ctx->ext_port, &soap_err, 1000000);
	*out_event = 6;
	*next_state = 0;
	ctx->flags &= ~1u;
	return 0;
}

static void upnpApctlHandler(s32 prev_state, s32 new_state, s32 event, s32 err, void *arg)
{
	UpnpCtx *ctx = (UpnpCtx *)arg;
	if (ctx == NULL)
		return;
	if (!(ctx->flags & 1))
		return;
	if (event == 4 && new_state == 4) {
		g_apctl_connected = 1;
		upnpClearEventBits(0x10);
	} else if (g_apctl_connected != 0 && new_state == 0) {
		upnpAbortInternal();
		g_apctl_connected = 0;
	}
}

static s32 upnpExecSoapAction(UpnpCtx *ctx, s32 svc_idx, s32 action, u16 ext_port, s32 proto,
                    u16 int_port, s32 *out_err, u32 timeout_us)
{
	char client_ip_str[0x80];
	SockAddrIn addr;
	XmlParser parser;
	TimeVal tv;
	s32 body_len;
	s32 ret;

	if (action == 0) {
		ret = sceNetApctlGetInfo(8, client_ip_str);
		if (ret < 0)
			return ret;
		body_len = soapBuildAddPortMappingBody(ctx->soap_buf, 0x400, client_ip_str, svc_idx, ext_port, proto, int_port);
	} else if (action == 1) {
		body_len = soapBuildDeletePortMappingBody(ctx->soap_buf, 0x400, svc_idx, ext_port, proto);
	} else if (action == 2) {
		body_len = soapBuildActionBody(ctx->soap_buf, 0x400, 2, svc_idx);
	} else {
		body_len = (s32)0x8041168f;
	}

	if (body_len < 0)
		return body_len;

	ret = upnpInitSoapXmlParser(ctx, &parser, action);
	if (ret < 0)
		return ret;

	ret = sceNetInetSocket(2, 1, 0);
	ctx->sock_fd = ret;
	if (ret < 0)
		return ret;

	if (timeout_us != 0) {
		tv.tv_sec = (s32)(timeout_us / 1000000u);
		tv.tv_usec = (s32)(timeout_us % 1000000u);
		ret = sceNetInetSetsockopt(ctx->sock_fd, 0xffff, 0x1006, &tv, sizeof(tv));
		if (ret < 0) {
			sceNetInetClose(ctx->sock_fd);
			ctx->sock_fd = -1;
			return ret;
		}
	}

	sceKernelMemset(&addr, 0, sizeof(addr));
	addr.sin_family = 2;
	addr.sin_port = bswap16(ctx->gw_port);
	addr.sin_addr = ctx->gw_ip;

	if ((u32)action < 3u) {
		ret = upnpSendSoapRequest(ctx->sock_fd, ctx->http_buf, 0x400, &addr, ctx->ctrl_url[svc_idx],
			action, svc_idx, ctx->soap_buf, (u32)body_len, &parser, timeout_us);
	} else {
		ret = (s32)0x8041168f;
	}

	sceNetInetClose(ctx->sock_fd);
	ctx->sock_fd = -1;

	if (ret < 0) {
		if (out_err != NULL)
			*out_err = ret;
		return (s32)0x80411608;
	}
	return ret;
}

static void upnpStunCallback(s32 stun_id, void *arg, s32 event, void *data)
{
	UpnpCtx *ctx = (UpnpCtx *)arg;

	if (event < 0) {
		ctx->stun_cb_status = event;
		upnpClearEventBits(0x40);
		return;
	}
	if (event == 4) {
		ctx->stun_cb_status = 0;
		upnpClearEventBits(0x80);
	} else if (event == 5) {
		sceKernelMemcpy(&ctx->stun_mapped_ip, data, 4);
	} else if (event == 1) {
		ctx->stun_cb_status = 0;
		sceKernelMemcpy(ctx->stun_result, data, 0x10);
		upnpClearEventBits(0x40);
	} else if (event == 2) {
		ctx->stun_cb_status = 0;
		upnpClearEventBits(0x40);
	}
}

