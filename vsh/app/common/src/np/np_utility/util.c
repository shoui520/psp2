/*
 * SceNpUtility: the lookup library's helpers. Numbers as decimal text and back, the online
 * ID check, reading a response body, the response headers and the body's signature, and the
 * NP ID, XML and Base64 conversions into a service's heap.
 */

#include "np_utility_internal.h"

/* the signature of a response: "<!--Sce-Np-Signature: " and 40 hex digits, then "-->" */
#define SIGNATURE_SIZE		20
#define SIGNATURE_LINE		65

/* appends value's decimal digits at *p */
static void appendInt(char **p, int value)
{
	if (value != 0) {
		appendInt(p, value / 10);
		**p = value % 10 + '0';
		(*p)++;
	}
}

/* value as decimal text at str, not terminated */
void SceNpUtility_4D52A224(char *str, int value)
{
	char *p = str;

	if (value < 0) {
		sceClibStrncpy(p, "-", 1);
		p++;
		value = -value;
	}
	if (value != 0)
		appendInt(&p, value);
	else
		sceClibStrncpy(p, "0", 1);
}

static void appendUInt64(char **p, SceUInt64 value)
{
	if (value != 0) {
		appendUInt64(p, value / 10);
		**p = value % 10 + '0';
		(*p)++;
	}
}

void SceNpUtility_2E4D8BCF(char *str, SceUInt64 value)
{
	char *p = str;

	if (value != 0)
		appendUInt64(&p, value);
	else
		sceClibStrncpy(p, "0", 1);
}

void SceNpUtility_FF9F1571(char *str, SceInt64 value)
{
	char *p = str;

	if (value < 0) {
		sceClibStrncpy(p, "-", 1);
		p++;
		value = -value;
	}
	if (value != 0)
		appendUInt64(&p, value);
	else
		sceClibStrncpy(p, "0", 1);
}

/* the decimal number at str, up to 20 digits or the first space */
SceInt64 SceNpUtility_BDFC2D58(const char *str)
{
	SceInt64 value = 0;
	int negative = 0;
	const char *p;
	SceUInt32 i;

	if (str != NULL) {
		if (*str == '-')
			negative = 1;
		p = &str[negative];
		for (i = negative; *p != '\0'; i++) {
			if (sceClibLookCtypeTable(*p) & SCE_CLIB_CTYPE_SPACE)
				break;
			if (i >= 20)
				break;
			value = value * 10 + (*p - '0');
			p++;
		}
		if (negative == 1)
			value = -value;
	}
	return value;
}

/* an online ID: 3 to 16 letters, digits, '-' and '_', starting with a letter */
SceBool SceNpUtility_475B35A8(const char *onlineId)
{
	SceSize length;
	SceSize i;

	if (onlineId == NULL)
		return SCE_FALSE;
	length = sceClibStrnlen(onlineId, 0x7FFFFFFF);
	if (length < 3 || length > SCE_NP_ONLINEID_MAX_LENGTH)
		return SCE_FALSE;
	if ((sceClibLookCtypeTable(onlineId[0]) & (SCE_CLIB_CTYPE_UPPER | SCE_CLIB_CTYPE_LOWER)) == 0)
		return SCE_FALSE;
	for (i = 1; i < length; i++) {
		char c = onlineId[i];

		if ((sceClibLookCtypeTable(c) &
		     (SCE_CLIB_CTYPE_UPPER | SCE_CLIB_CTYPE_LOWER | SCE_CLIB_CTYPE_DIGIT)) == 0 &&
		    c != '-' && c != '_')
			return SCE_FALSE;
	}
	return SCE_TRUE;
}

/* reads up to size bytes of the response body, *read of them; stops when the body ends */
static inline int readData(NpLookupTrans *trans, void *buf, SceSize size, SceSize *read)
{
	SceSize left = size;
	int ret = 0;

	while (left != 0) {
		if (trans->aborted == 1)
			return SCE_NP_COMMUNITY_ERROR_ABORTED;
		ret = sceHttpReadData(trans->request, (char *)buf + size - left, left);
		if (ret < 0)
			return ret;
		if (ret == 0)
			break;
		left -= ret;
	}
	*read = size - left;
	return ret;
}

int SceNpUtility_A3E828ED(NpLookupTrans *trans, void *buf, SceSize size, SceSize *read)
{
	return readData(trans, buf, size, read);
}

/* the signature's 40 hex digits at str as its 20 bytes */
static inline int parseSignature(const char *str, SceSize length, SceUInt8 *signature)
{
	char hex[4];
	int value;
	int i;

	if (length < SIGNATURE_SIZE * 2)
		return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
	for (i = 0; i < SIGNATURE_SIZE; i++) {
		sceClibMemcpy_safe(hex, str, 2);
		hex[2] = '\0';
		value = SceNpCommon_4C4AA451(hex, NULL, 16);
		if (value < 0 || value > 0xFF)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		*signature++ = value;
		str += 2;
	}
	return 0;
}

/* the signature line that starts the body, then its line end; the rest is the content */
static int readSignature(NpLookupTrans *trans, SceUInt8 *signature, SceUInt64 *contentLength)
{
	char line[SIGNATURE_LINE + 3];
	SceSize read = 0;
	SceSize length;
	SceSize prefix;
	int ret;

	if (trans->aborted == 1)
		return SCE_NP_COMMUNITY_ERROR_ABORTED;
	ret = readData(trans, line, SIGNATURE_LINE, &read);
	if (ret < 0)
		return ret;
	if (read != SIGNATURE_LINE)
		return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
	length = read;
	prefix = sceClibStrnlen("<!--", 0x7FFFFFFF) +
		 sceClibStrnlen("Sce-Np-Signature", 0x7FFFFFFF) + sceClibStrnlen(": ", 0x7FFFFFFF);
	if (sceClibStrncasecmp(line, "<!--Sce-Np-Signature: ", prefix) != 0)
		return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
	ret = parseSignature(&line[prefix], SIGNATURE_LINE - prefix, signature);
	if (ret < 0)
		return ret;

	ret = readData(trans, line, 1, &read);
	if (ret < 0)
		return ret;
	if (read != 1)
		return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
	length += read;
	if (line[0] == '\r') {
		ret = readData(trans, line, 1, &read);
		if (ret < 0)
			return ret;
		if (read != 1)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		length += read;
	}
	if (line[0] != '\n')
		return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
	if (*contentLength != 0)
		*contentLength -= length;
	return 0;
}

/*
 * The response of the transaction's request: its status, content length, info length and
 * signature (from the header, or from the body for the operations that sign it there).
 */
int SceNpUtility_4FAC4566(NpLookupTrans *trans, int *statusCode, SceUInt64 *contentLength,
			  int *hasContentLength, SceUInt32 *infoLength, SceUInt8 *signature,
			  int operation)
{
	char *header = NULL;
	SceSize headerSize = 0;
	const char *value = NULL;
	SceSize valueLength = 0;
	int ret;

	if (operation != 1 && operation != 10 && signature == NULL)
		return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
	ret = sceHttpGetStatusCode(trans->request, statusCode);
	if (ret < 0)
		return ret;
	if (*statusCode != 200) {
		if (operation == 3 || operation == 4)
			return SCE_NP_COMMUNITY_SERVER_ERROR_NO_SUCH_TITLE;
		ret = SCE_NP_COMMUNITY_ERROR_HTTP_SERVER;
		if (operation == 10 && *statusCode == 404)
			ret = NP_LOOKUP_ERROR_NOT_FOUND;
		return ret;
	}

	if (sceHttpGetResponseContentLength(trans->request, contentLength) < 0) {
		*contentLength = 0;
		*hasContentLength = 0;
	} else {
		*hasContentLength = 1;
	}
	ret = sceHttpGetAllResponseHeaders(trans->request, &header, &headerSize);
	if (ret < 0)
		return ret;
	if (sceHttpParseResponseHeader(header, headerSize, "Sce-Np-Info-Length", &value,
				       &valueLength) >= 0 && infoLength != NULL)
		*infoLength = SceNpCommon_5B770B80(value, NULL, 10);
	if (operation == 2 &&
	    sceHttpParseResponseHeader(header, headerSize, "Sce-Np-Signature", &value,
				       &valueLength) >= 0 && signature != NULL) {
		ret = parseSignature(value, valueLength, signature);
		if (ret < 0)
			return ret;
	}
	if (operation == 4 || operation == 3 || operation == 5 || operation == 6 ||
	    operation == 7 || operation == 9 || operation == 8) {
		ret = readSignature(trans, signature, contentLength);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* the request's Sce-Np-Info-Length header */
void SceNpUtility_72FC55A7(int reqId, SceUInt32 infoLength)
{
	char value[24];

	sceClibMemset(value, 0, 21);
	SceNpUtility_2E4D8BCF(value, infoLength);
	sceHttpAddRequestHeader(reqId, "Sce-Np-Info-Length", value, SCE_HTTP_HEADER_OVERWRITE);
}

/* the NP ID of a JID's text */
int SceNpUtility_54C48F92(SceNpId *npId, const char *str, int service)
{
	NpJid jid;
	SceSize size;
	char *buf;
	int ret;

	ret = SceNpCommon_D2B9BCA8(&jid, str, NULL, 0, &size);
	if (ret < 0)
		return ret;
	if (size == 0)
		return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;

	buf = SceNpUtility_8CA24D19(size, service);
	if (buf == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
	} else {
		ret = SceNpCommon_D2B9BCA8(&jid, str, buf, size, NULL);
		if (ret >= 0) {
			ret = SceNpCommon_9B7CAA67(npId, &jid);
			if (ret >= 0)
				ret = 0;
		}
	}
	if (buf != NULL)
		SceNpUtility_6C1161AD(buf, service);
	return ret;
}

/* the JID text of an NP ID without its resource, on the service's heap */
int SceNpUtility_14437509(char **str, const SceNpId *npId, int service)
{
	NpJid jid;
	SceSize size;
	char *parts;
	char *text = NULL;
	int ret;

	ret = SceNpCommon_40CC4D15(NULL, npId, NULL, 0, &size);
	if (ret < 0)
		return ret;

	parts = SceNpUtility_8CA24D19(size, service);
	if (parts == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
	} else {
		ret = SceNpCommon_40CC4D15(&jid, npId, parts, size, NULL);
		if (ret >= 0) {
			jid.resource = NULL;
			ret = SceNpCommon_85699AE2(&jid, NULL, 0, &size);
			if (ret >= 0) {
				text = SceNpUtility_8CA24D19(size, service);
				if (text == NULL) {
					ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
				} else {
					ret = SceNpCommon_85699AE2(&jid, text, size, NULL);
					if (ret >= 0) {
						SceNpUtility_6C1161AD(parts, service);
						*str = text;
						return 0;
					}
				}
			}
		}
	}
	if (parts != NULL)
		SceNpUtility_6C1161AD(parts, service);
	if (text != NULL)
		SceNpUtility_6C1161AD(text, service);
	return ret;
}

/* src escaped for XML, on the service's heap */
int SceNpUtility_B8DFA4D1(char **dst, const char *src, int service)
{
	char *text = NULL;
	SceSize length;
	SceSize size;
	int ret;

	length = sceClibStrnlen(src, 0x7FFFFFFF);
	if (length == 0) {
		text = SceNpUtility_8CA24D19(1, service);
		*text = '\0';
		ret = 0;
	} else {
		ret = SceNpCommon_C2BF5C1F(NULL, 0, (const SceUInt8 *)src, length, &size);
		if (ret >= 0) {
			text = SceNpUtility_8CA24D19(size, service);
			if (text == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				sceClibMemset(text, 0, size);
				ret = SceNpCommon_C2BF5C1F(text, size, (const SceUInt8 *)src, length, NULL);
				if (ret < 0) {
					SceNpUtility_6C1161AD(text, service);
					text = NULL;
				} else {
					ret = 0;
				}
			}
		}
	}
	*dst = text;
	return ret;
}

/* XML text unescaped, on the service's heap */
int SceNpUtility_15D37FB1(char **dst, const char *src, int service)
{
	char *text = NULL;
	SceSize length;
	SceSize size;
	int ret;

	length = sceClibStrnlen(src, 0x7FFFFFFF);
	ret = SceNpCommon_389F8C14(NULL, 0, (const SceUInt8 *)src, length, &size);
	if (ret >= 0) {
		text = SceNpUtility_8CA24D19(size, service);
		if (text == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			ret = SceNpCommon_389F8C14(text, size, (const SceUInt8 *)src, length, NULL);
			if (ret < 0) {
				SceNpUtility_6C1161AD(text, service);
				text = NULL;
			} else {
				ret = 0;
			}
		}
	}
	*dst = text;
	return ret;
}

/* takes nothing to do: returns its second argument */
int SceNpUtility_89484A9C(void *arg, int value)
{
	return value;
}

/* frees a server list */
int SceNpUtility_E916C86A(NpLookupServer *server, int service)
{
	NpLookupServer *next;

	for (; server != NULL; server = next) {
		next = server->next;
		SceNpUtility_6C1161AD(server, service);
	}
	return 0;
}

/* src as Base64 text, on the service's heap */
int SceNpUtility_3A1BA624(char **dst, const void *src, SceSize size, int service)
{
	char *text;
	int ret;

	text = SceNpUtility_8CA24D19((size + 2) / 3 * 4 + 1, service);
	if (text == NULL)
		return SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
	sceClibMemset(text, 0, (size + 2) / 3 * 4 + 1);
	ret = sceNpCommonBase64Encode(src, text, size);
	if (ret < 0)
		SceNpUtility_6C1161AD(text, service);
	else
		*dst = text;
	return ret;
}

/* the system software version as "major.minor" */
void SceNpUtility_450C80FB(char *version)
{
	SceNpCommon_0624AE44(version);
}
