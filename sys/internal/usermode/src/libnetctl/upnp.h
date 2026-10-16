#ifndef NETCTL_UPNP_H
#define NETCTL_UPNP_H

/*
 * UPnP Internet Gateway Device client of SceNetCtl: SSDP discovery of the
 * gateway, device description download and SOAP port mapping actions over
 * a small streaming XML parser.  Names are descriptive (no debug records).
 */

#include <psp2/types.h>
#include <psp2/net/net.h>
#include <psp2/kernel/clib.h>

extern long long sceClibStrtoll(const char *str, char **end, int base);

/* Error codes (official names unknown). */
#define UPNP_ERROR_INVALID_POINTER	0x80414105	/* NULL context, prefix or tag */
#define UPNP_ERROR_NOT_INITIALIZED	0x80414101
#define UPNP_ERROR_NOT_CONNECTED	0x80414102	/* NetCtl is not connected */
#define UPNP_ERROR_TAG_TOO_LONG		0x80414106
#define UPNP_ERROR_NOT_UPNP_ERROR	0x80414107	/* faultstring is not "UPnPError" */
#define UPNP_ERROR_UPNP_FAULT		0x80414108	/* gateway answered with a UPnP fault */
#define UPNP_ERROR_RESULT_TOO_LONG	0x80414109
#define UPNP_ERROR_CONNECT_TIMEOUT	0x8041410a
#define UPNP_ERROR_NO_SERVICE		0x8041410b	/* no WAN connection service found */
#define UPNP_ERROR_FAULT_401		0x80414171	/* Invalid Action */
#define UPNP_ERROR_FAULT_402		0x80414172	/* Invalid Args */
#define UPNP_ERROR_FAULT_404		0x80414173	/* Invalid Var */
#define UPNP_ERROR_FAULT_501		0x80414174	/* Action Failed */
#define UPNP_ERROR_FAULT_714		0x80414175	/* NoSuchEntryInArray */
#define UPNP_ERROR_FAULT_715		0x80414176	/* WildCardNotPermittedInSrcIP */
#define UPNP_ERROR_FAULT_716		0x80414177	/* WildCardNotPermittedInExtPort */
#define UPNP_ERROR_FAULT_718		0x80414178	/* ConflictInMappingEntry */
#define UPNP_ERROR_FAULT_724		0x80414179	/* SamePortValuesRequired */
#define UPNP_ERROR_FAULT_725		0x8041417a	/* OnlyPermanentLeasesSupported */
#define UPNP_ERROR_FAULT_726		0x8041417b	/* RemoteHostOnlySupportsWildcard */
#define UPNP_ERROR_FAULT_727		0x8041417c	/* ExternalPortOnlySupportsWildcard */
#define UPNP_ERROR_FAULT_OTHER		0x8041417d
#define UPNP_ERROR_NO_EXTERNAL_IP	0x8041417e
#define UPNP_ERROR_BAD_PREFIX		0x80414180	/* element not in the expected namespace */
#define UPNP_ERROR_ATTR_MISMATCH	0x80414181
#define UPNP_ERROR_ATTR_MISSING		0x80414183
#define UPNP_ERROR_INCOMPLETE_RESPONSE	0x80414184
#define UPNP_ERROR_BUFFER_TOO_SMALL	0x80414185
#define UPNP_ERROR_BAD_ADDRESS		0x80414186
#define UPNP_ERROR_NO_GATEWAY		0x80414187	/* no SSDP answer after 5 tries */
#define UPNP_ERROR_NO_LINE		0x80414188	/* no complete line in the buffer */
#define UPNP_ERROR_BAD_URL		0x80414189	/* also: header line without ':' */
#define UPNP_ERROR_NOT_GATEWAY		0x8041418a	/* SSDP answer not usable */
#define UPNP_ERROR_BAD_HTTP		0x8041418b
#define UPNP_ERROR_LENGTH_MISMATCH	0x8041418d
#define UPNP_ERROR_INVALID_ACTION	0x8041418f
#define UPNP_ERROR_INCOMPLETE_XML	0x80414190
#define UPNP_XML_ERROR_INVALID		0x804141a0
#define UPNP_XML_ERROR_NO_SPACE		0x804141a2	/* output too small / input incomplete */

/* SOAP actions (index into the action name tables). */
#define UPNP_ACTION_ADD_PORT_MAPPING		0
#define UPNP_ACTION_DELETE_PORT_MAPPING		1
#define UPNP_ACTION_GET_EXTERNAL_IP_ADDRESS	2
#define UPNP_ACTION_GET_STATUS_INFO		3
#define UPNP_ACTION_COUNT			4

/* WAN connection services. */
#define UPNP_SERVICE_WAN_IP		0
#define UPNP_SERVICE_WAN_PPP		1
#define UPNP_SERVICE_COUNT		2

#define UPNP_TIMEOUT			5000000		/* us */

/* XML parser events handed to the element callbacks. */
#define UPNP_XML_EVENT_START		1
#define UPNP_XML_EVENT_END		2
#define UPNP_XML_EVENT_TEXT		3	/* end of an element with text content */
#define UPNP_XML_EVENT_ATTRIBUTE	4

#define UPNP_XML_NAME_SIZE		0xff
#define UPNP_XML_VALUE_SIZE		0x1ff
#define UPNP_XML_DEPTH_MAX		8

/* One element of a schema level; a "" name ends the list (8 bytes). */
typedef struct UpnpXmlElement {
	const char *name;
	SceUInt8 hasText;	/* 1: report the text content (UPNP_XML_EVENT_TEXT) */
	SceUInt8 matchSuffix;	/* 1: match the end of the tag name ("s:Envelope") */
} UpnpXmlElement;

/* Accepted elements, one list per nesting depth. */
typedef struct UpnpXmlSchema {
	const UpnpXmlElement *const *levels;
	SceUInt32 depth;
	SceUInt32 reserved;
} UpnpXmlSchema;

/* Expected attribute of an element (16 bytes); a "" name ends the list. */
#define UPNP_XML_ATTR_NAME		0	/* name */
#define UPNP_XML_ATTR_PREFIX_NAME	1	/* <prefix>:name */
#define UPNP_XML_ATTR_NAME_PREFIX	2	/* name:<prefix> (xmlns:s) */
#define UPNP_XML_VALUE_EXACT		0
#define UPNP_XML_VALUE_SERVICE		1	/* same service type as the request */

typedef struct UpnpXmlAttribute {
	SceUInt32 nameMode;
	const char *name;
	SceUInt32 valueMode;
	const char *value;
} UpnpXmlAttribute;

struct UpnpXmlParser;
typedef int (*UpnpXmlCallback)(struct UpnpXmlParser *parser, int event, int depth, int index,
			       const char *text, const char *value, void *arg);

/* Parser states */
#define UPNP_XML_STATE_TEXT		0	/* outside of tags, waiting for '<' */
#define UPNP_XML_STATE_TAG		1	/* tag name */
#define UPNP_XML_STATE_IN_TAG		2	/* between attributes */
#define UPNP_XML_STATE_CONTENT		3	/* text content of a leaf element */
#define UPNP_XML_STATE_END_TAG		4	/* name of an end tag */
#define UPNP_XML_STATE_ATTR_NAME	5
#define UPNP_XML_STATE_ATTR_VALUE	6

typedef struct UpnpXmlParser {
	const UpnpXmlSchema *schema;		/* 0x000 */
	char name[UPNP_XML_NAME_SIZE];		/* 0x004 tag or attribute name */
	char value[UPNP_XML_VALUE_SIZE];	/* 0x103 text content or attribute value */
	char pending[6];			/* 0x302 incomplete character */
	SceSize pendingLen;			/* 0x308 */
	SceSize pendingNeed;			/* 0x30C bytes still missing */
	SceUInt32 depth;			/* 0x310 */
	SceUInt32 index[UPNP_XML_DEPTH_MAX];	/* 0x314 matched element per depth */
	int state;				/* 0x334 */
	SceUInt32 flags;			/* 0x338 sub-state of the current state */
	SceSize len;				/* 0x33C */
	SceUInt8 closing;			/* 0x340 '/' seen in the tag */
	UpnpXmlCallback callback;		/* 0x344 */
	void *arg;				/* 0x348 */
} UpnpXmlParser;

/* attribute value flags (UPNP_XML_STATE_ATTR_VALUE) */
#define UPNP_XML_VALUE_STARTED		1
#define UPNP_XML_VALUE_QUOTED		2

/* Device description download (0x6A4 bytes). */
typedef struct UpnpDescription {
	UpnpXmlParser device;		/* 0x000 */
	UpnpXmlParser service;		/* 0x34C */
	SceUInt32 flags;		/* 0x698 */
	char *controlUrl;		/* 0x69C */
	SceSize controlUrlSize;		/* 0x6A0 */
} UpnpDescription;

#define UPNP_DESC_URL_BASE		0x01
#define UPNP_DESC_DONE			0x02
#define UPNP_DESC_SERVICE_TYPE		0x04
#define UPNP_DESC_WAN_IP		0x08
#define UPNP_DESC_WAN_PPP		0x10
#define UPNP_DESC_CONTROL_URL		0x20

/* SOAP response (0x370 bytes). */
typedef struct UpnpSoap {
	UpnpXmlParser parser;		/* 0x000 */
	SceUInt32 flags;		/* 0x34C */
	int action;			/* 0x350 */
	int service;			/* 0x354 */
	SceUInt8 attrSeen[8];		/* 0x358 */
	int *errorCode;			/* 0x360 */
	int unk364;			/* 0x364 always 0 */
	char *result;			/* 0x368 */
	SceSize resultSize;		/* 0x36C */
} UpnpSoap;

#define UPNP_SOAP_ENVELOPE		0x0001
#define UPNP_SOAP_BODY			0x0002
#define UPNP_SOAP_RESPONSE		0x0004
#define UPNP_SOAP_STATUS		0x0008	/* never set */
#define UPNP_SOAP_EXTERNAL_IP		0x0010
#define UPNP_SOAP_FAULT			0x0020
#define UPNP_SOAP_FAULT_CODE		0x0040
#define UPNP_SOAP_FAULT_STRING		0x0080
#define UPNP_SOAP_DETAIL		0x0100
#define UPNP_SOAP_UPNP_ERROR		0x0200
#define UPNP_SOAP_ERROR_CODE		0x0400
#define UPNP_SOAP_ENVELOPE_END		0x0800
#define UPNP_SOAP_RESPONSE_END		0x1000
#define UPNP_SOAP_UPNP_ERROR_END	0x2000

#define UPNP_BUFFER_SIZE		0x400
#define UPNP_PATH_SIZE			0x80

/* Client context owned by the caller (0x1F44 bytes). */
typedef struct NetCtlUpnp {
	SceUInt8 initialized;			/* 0x0000 */
	SceUInt8 aborted;			/* 0x0001 */
	SceUInt32 flags;			/* 0x0004 */
	int sock;				/* 0x0008 */
	char buf[UPNP_BUFFER_SIZE];		/* 0x000C */
	char body[UPNP_BUFFER_SIZE];		/* 0x040C SOAP request body */
	char path[UPNP_PATH_SIZE];		/* 0x080C description path, then SOAP prefix */
	UpnpDescription desc;			/* 0x088C */
	UpnpSoap soap;				/* 0x0F30 */
	int unk12A0;				/* 0x12A0 */
	SceNetInAddr gateway;			/* 0x12A4 */
	SceUInt16 port;				/* 0x12A8 */
	SceUInt16 unk12AA;			/* 0x12AA */
	char urlBase[UPNP_BUFFER_SIZE];		/* 0x12AC */
	int service;				/* 0x16AC */
	SceUInt32 services;			/* 0x16B0 bit per found service */
	char controlUrl[UPNP_SERVICE_COUNT][UPNP_BUFFER_SIZE]; /* 0x16B4 */
	char modelName[0x40];			/* 0x1EB4 */
	char friendlyName[0x40];		/* 0x1EF4 */
	char externalIp[16];			/* 0x1F34 */
} NetCtlUpnp;

#define UPNP_FLAG_BUSY			1
#define UPNP_FLAG_DISCOVERED		8

/* upnp.c */
int netCtlUpnpReadLine(char *line, SceSize size);
int netCtlUpnpSplitHeader(char *line);
int netCtlUpnpSoapParse(NetCtlUpnp *upnp, const char *data, SceSize size, int last);
int netCtlUpnpDescParse(NetCtlUpnp *upnp, const char *data, SceSize size, int last);

/* upnp_http.c */
typedef int (*NetCtlUpnpBodyCallback)(NetCtlUpnp *upnp, const char *data, SceSize size, int last);
int netCtlUpnpHttpReceive(NetCtlUpnp *upnp, int sock, char *buf, SceSize size,
			  NetCtlUpnpBodyCallback callback);
int netCtlUpnpHttpGet(NetCtlUpnp *upnp, int sock, char *buf, int size,
		      const SceNetSockaddrIn *addr, const char *path);
int netCtlUpnpHttpPost(NetCtlUpnp *upnp, int sock, char *buf, SceSize size,
		       const SceNetSockaddrIn *addr, const char *path, int action, int service,
		       const char *body, SceSize bodyLen, SceUInt32 timeout);

/* upnp_xml.c */
int netCtlUpnpXmlInit(UpnpXmlParser *parser, const UpnpXmlSchema *schema,
		      UpnpXmlCallback callback, void *arg);
int netCtlUpnpXmlParse(UpnpXmlParser *parser, const char *data, int size);
int netCtlUpnpXmlUnescape(char *out, SceSize outSize, const char *in, SceSize inLen,
			  SceSize *outLen);
int netCtlUpnpXmlDecodeChar(char *out, SceSize outSize, const unsigned char *in, SceSize inLen,
			    SceSize *outLen, SceSize *consumed);

extern const char *const g_upnpActionNames[];
extern const char *const g_upnpServiceTypes[];

#endif
