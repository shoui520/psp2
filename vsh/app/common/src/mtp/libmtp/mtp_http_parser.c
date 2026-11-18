/*
 * The parser of the hosts' HTTP-like replies ("HTTP PARSER:"): a response's status line,
 * a host's request to cancel a registration and a host's discovery response, line by line.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "mtp_internal.h"

#define MTP_TAG_HTTP_PARSER		"HTTP PARSER:"

/* the parser's errors */
#define HTTP_PARSER_ERROR_ARGUMENT	-1
#define HTTP_PARSER_ERROR_LINE		-2
#define HTTP_PARSER_ERROR_KEY		-3
#define HTTP_PARSER_ERROR_VALUE		-4
#define HTTP_PARSER_ERROR_FORMAT	-5

#define HTTP_PARSER_LINE_SIZE		256
#define HTTP_PARSER_FIELD_SIZE		128
#define HTTP_PARSER_DEVICE_SIZE		16

/* the fields of a discovery response that were found */
#define HOST_FIELD_ID			0x01
#define HOST_FIELD_TYPE			0x02
#define HOST_FIELD_NAME			0x04
#define HOST_FIELD_MTP_PROTOCOL_VERSION	0x08
#define HOST_FIELD_REQUEST_PORT		0x10
#define HOST_FIELD_WIRELESS_PROTOCOL	0x20
#define HOST_FIELD_MAC_ADDRESS		0x40
#define HOST_FIELD_SUPPORTED_DEVICE	0x80
#define HOST_FIELDS_REQUIRED		0x3F

/* a nasne's MAC address: the last bytes of its host ID */
#define HOST_ID_MAC_OFFSET		10

/* the line at src, without its end: its length, or -1 when it is too long */
static int getLine(const char *src, char *line)
{
	char *dst = line;
	SceUInt32 i;
	char c;

	for (i = 0; (c = *src) != '\0'; i++) {
		if (c == '\r' || c == '\n')
			break;
		if (i >= HTTP_PARSER_LINE_SIZE - 1)
			return -1;
		*dst++ = c;
		src++;
	}
	line[i] = '\0';
	return i;
}

/* "HTTP/1.1 <status> OK|NG" */
int parse_response(int *status, int *result, const char *buf)
{
	char line[HTTP_PARSER_LINE_SIZE];
	char *token;

	if (status == NULL || buf == NULL)
		return HTTP_PARSER_ERROR_ARGUMENT;
	if (getLine(buf, line) < 0) {
		MTP_ERR(MTP_TAG_HTTP_PARSER, 47, "invalid line\n");
		return HTTP_PARSER_ERROR_LINE;
	}
	token = ScePafStdc_766D2A69(line, " ");
	if (token == NULL)
		return HTTP_PARSER_ERROR_FORMAT;
	if (sce_paf_strcmp("HTTP/1.1", token) != 0)
		return HTTP_PARSER_ERROR_FORMAT;
	token = ScePafStdc_766D2A69(NULL, " ");
	if (token == NULL)
		return HTTP_PARSER_ERROR_FORMAT;
	if (status != NULL)
		*status = sce_paf_atoi(token);
	token = ScePafStdc_766D2A69(NULL, " ");
	if (token == NULL)
		return HTTP_PARSER_ERROR_FORMAT;
	if (sce_paf_strcasecmp("OK", token) == 0) {
		if (result != NULL)
			*result = 1;
	} else if (sce_paf_strcasecmp("NG", token) == 0) {
		if (result != NULL)
			*result = 0;
	} else {
		return HTTP_PARSER_ERROR_FORMAT;
	}
	return 0;
}

/* "REGISTERCANCEL * HTTP/1.1", then "errorcode:<code>" */
int parse_register_cancel_request(int *errorCode, const char *buf)
{
	char line[HTTP_PARSER_LINE_SIZE];
	const char *next;
	char *token;
	int length;

	if (errorCode == NULL || buf == NULL)
		return HTTP_PARSER_ERROR_ARGUMENT;
	length = getLine(buf, line);
	if (length < 0) {
		MTP_ERR(MTP_TAG_HTTP_PARSER, 101, "invalid line\n");
		return HTTP_PARSER_ERROR_LINE;
	}
	next = buf + length;
	while (*next == '\r' || *next == '\n')
		next++;
	token = ScePafStdc_766D2A69(line, " * ");
	if (token == NULL)
		return HTTP_PARSER_ERROR_FORMAT;
	if (sce_paf_strcmp("REGISTERCANCEL", token) != 0)
		return HTTP_PARSER_ERROR_FORMAT;
	token = ScePafStdc_766D2A69(NULL, " * ");
	if (token == NULL)
		return HTTP_PARSER_ERROR_FORMAT;
	if (sce_paf_strcmp("HTTP/1.1", token) != 0)
		return HTTP_PARSER_ERROR_FORMAT;
	if (getLine(next, line) < 0) {
		MTP_ERR(MTP_TAG_HTTP_PARSER, 131, "invalid line\n");
		return HTTP_PARSER_ERROR_LINE;
	}
	token = ScePafStdc_766D2A69(line, ":");
	if (token == NULL)
		return HTTP_PARSER_ERROR_FORMAT;
	if (sce_paf_strcmp("errorcode", token) != 0)
		return HTTP_PARSER_ERROR_FORMAT;
	token = ScePafStdc_766D2A69(NULL, ":");
	if (token == NULL)
		return HTTP_PARSER_ERROR_FORMAT;
	if (errorCode != NULL)
		*errorCode = sce_paf_atoi(token);
	return 0;
}

/*
 * One device of a host-supported-device list ("PS Vita, PS TV"), its spaces trimmed: the
 * characters it took with its comma, or -1 when it is too long
 */
static __attribute__((noinline)) int parseSupportedDevice(const char *str, int *device)
{
	unsigned char name[HTTP_PARSER_DEVICE_SIZE];
	SceUInt32 count = 0;
	int start = 0;
	int length;
	int end;
	const char *trimmed;
	SceSize left;

	mtpUtilMemset(name, 0, sizeof(name));
	while (*str != '\0') {
		if (*str == ',') {
			count++;
			break;
		}
		if (count >= HTTP_PARSER_DEVICE_SIZE - 1)
			return -1;
		name[count] = *str;
		str++;
		count++;
	}
	length = mtpUtilStrlen((const char *)name);
	for (start = 0; start < length; start++) {
		if (name[start] != ' ')
			break;
	}
	for (end = length - 1; end >= start; end--) {
		if (name[end] != ' ')
			break;
		name[end] = '\0';
	}
	trimmed = (const char *)&name[start];
	left = HTTP_PARSER_DEVICE_SIZE - start;
	if (mtpUtilStrncmp(trimmed, "PS Vita", left) == 0 ||
	    mtpUtilStrncmp(trimmed, "vita", left) == 0)
		*device = MTP_NET_HOST_SUPPORTS_VITA;
	else if (mtpUtilStrncmp(trimmed, "PS Vita TV", left) == 0 ||
		 mtpUtilStrncmp(trimmed, "dolce", left) == 0 ||
		 mtpUtilStrncmp(trimmed, "PS TV", left) == 0)
		*device = MTP_NET_HOST_SUPPORTS_DOLCE;
	return count;
}

/* a header line's key, its spaces dropped: where its value starts, or -1 */
static int getKey(const char *line, char *key)
{
	SceUInt32 i;
	int n = 0;

	for (i = 0; line[i] != '\0' && line[i] != ':'; i++) {
		if (i >= HTTP_PARSER_FIELD_SIZE - 1)
			return -1;
		if (line[i] != ' ')
			key[n++] = line[i];
	}
	key[n] = '\0';
	if (line[i] == ':')
		i++;
	return i;
}

/* the value: its length, or -1 when it is too long */
static int getValue(const char *src, char *value)
{
	SceUInt32 i;

	for (i = 0; src[i] != '\0'; i++) {
		if (i >= HTTP_PARSER_FIELD_SIZE - 1)
			return -1;
		value[i] = src[i];
	}
	value[i] = '\0';
	return i;
}

/* the header lines of a host's discovery response, up to the empty line */
int parse_host_discovery_responce(MtpNetHost *host, const char *buf)
{
	char line[HTTP_PARSER_LINE_SIZE];
	char key[HTTP_PARSER_FIELD_SIZE];
	char value[HTTP_PARSER_FIELD_SIZE];
	const char *list;
	SceUInt32 fields = 0;
	int device;
	int length;
	int offset;
	int n;

	if (host == NULL || buf == NULL)
		return HTTP_PARSER_ERROR_ARGUMENT;
	mtpUtilMemset(host, 0, sizeof(*host));
	for (;;) {
		length = getLine(buf, line);
		if (length < 0)
			break;
		buf += length;
		while (*buf == '\r' || *buf == '\n')
			buf++;
		offset = getKey(line, key);
		if (offset < 0) {
			MTP_ERR(MTP_TAG_HTTP_PARSER, 194, "invalid key\n");
			return HTTP_PARSER_ERROR_KEY;
		}
		if (getValue(&line[offset], value) < 0) {
			MTP_ERR(MTP_TAG_HTTP_PARSER, 200, "invalid value\n");
			return HTTP_PARSER_ERROR_VALUE;
		}
		if (sce_paf_strncmp(key, "host-id", sizeof(key)) == 0) {
			netMtpStringToUuid(host->id, value);
			fields |= HOST_FIELD_ID;
		} else if (sce_paf_strncmp(key, "host-type", sizeof(key)) == 0) {
			netMtpStringToHostType(&host->type, value);
			fields |= HOST_FIELD_TYPE;
		} else if (sce_paf_strncmp(key, "host-name", sizeof(key)) == 0) {
			sce_paf_strncpy(host->name, value, sizeof(host->name));
			fields |= HOST_FIELD_NAME;
		} else if (sce_paf_strncmp(key, "host-mtp-protocol-version", sizeof(key)) == 0) {
			host->mtpProtocolVersion = sce_paf_atoi(value);
			fields |= HOST_FIELD_MTP_PROTOCOL_VERSION;
		} else if (sce_paf_strncmp(key, "host-request-port", sizeof(key)) == 0) {
			host->requestPort = sce_paf_atoi(value);
			fields |= HOST_FIELD_REQUEST_PORT;
		} else if (sce_paf_strncmp(key, "host-wireless-protocol-version",
				sizeof(key)) == 0) {
			host->wirelessProtocolVersion = sce_paf_atoi(value);
			fields |= HOST_FIELD_WIRELESS_PROTOCOL;
		} else if (sce_paf_strncmp(key, "host-mac-address", sizeof(key)) == 0) {
			if (netMtpStringToMacAddress(host->macAddress, value) == 0)
				fields |= HOST_FIELD_MAC_ADDRESS;
		} else if (sce_paf_strncmp(key, "host-supported-device", sizeof(key)) == 0) {
			list = value;
			while (*list != '\0') {
				device = 0;
				n = parseSupportedDevice(list, &device);
				if (n <= 0)
					break;
				fields |= HOST_FIELD_SUPPORTED_DEVICE;
				host->supportedDevice |= device;
				list += n;
			}
		} else if (sce_paf_strncmp(key, "host-sdk-version", sizeof(key)) == 0) {
			host->supportedDevice = MTP_NET_HOST_SUPPORTS_VITA |
				MTP_NET_HOST_SUPPORTS_DOLCE;
			fields |= HOST_FIELD_SUPPORTED_DEVICE;
		}
		if (length == 0) {
			/* a nasne without its MAC address: the end of its ID */
			if (host->type == MTP_NET_HOST_TYPE_NASNE &&
			    (fields & HOST_FIELD_MAC_ADDRESS) == 0)
				sce_paf_memcpy(host->macAddress, &host->id[HOST_ID_MAC_OFFSET],
					sizeof(host->macAddress));
			if ((fields & HOST_FIELD_SUPPORTED_DEVICE) == 0)
				host->supportedDevice = MTP_NET_HOST_SUPPORTS_VITA;
			if ((fields & HOST_FIELDS_REQUIRED) == HOST_FIELDS_REQUIRED)
				return 0;
			return HTTP_PARSER_ERROR_FORMAT;
		}
	}
	MTP_ERR(MTP_TAG_HTTP_PARSER, 179, "invalid line\n");
	return HTTP_PARSER_ERROR_LINE;
}
