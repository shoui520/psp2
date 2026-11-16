/*
 * SceNpCommerce2: reading the HTTP response of a request.
 */

#include "np_commerce2_internal.h"

/* the status of the response, and (once) the version the server reports in X-NativeBuildVersion, as the
 * number its four pairs of digits make */
int npCommerce2GetResponseStatus(int httpRequest, int *nativeBuildVersion)
{
	char *fieldValue;
	char *header;
	SceSize headerSize;
	SceSize valueLen;
	int statusCode;
	char *p;
	int version = 0;
	int ret;

	ret = sceHttpGetStatusCode(httpRequest, &statusCode);
	if (ret >= 0) {
		if (*nativeBuildVersion == 0) {
			headerSize = 0;
			if (sceHttpGetAllResponseHeaders(httpRequest, &header, &headerSize) >= 0 &&
			    sceHttpParseResponseHeader(header, headerSize, "X-NativeBuildVersion",
						       (const char **)&fieldValue, &valueLen) >= 0) {
				if (sceClibStrnlen("XX.XX.XX.XX", 12) <= valueLen) {
					p = fieldValue;
					if (p < fieldValue + 9) {
						do {
							int tens = 10 * p[0] - 480 + 100 * version;
							int units = p[1];

							p += 3;
							version = tens + (units - 48);
						} while (p <= fieldValue + 9);
					}
					*nativeBuildVersion = version;
				}
			}
		}
		if (statusCode == 503 || statusCode == 500)
			ret = SCE_NP_COMMERCE2_ERROR_SERVER_MAINTENANCE;
		else if (statusCode == 200)
			ret = 0;
		else
			ret = SCE_NP_COMMERCE2_ERROR_SERVER_UNKNOWN;
	}
	return ret;
}

/* reads size bytes (fewer at the end of the response); gives how many it read */
int npCommerce2ReadResponseData(int httpRequest, char *data, SceSize size)
{
	SceSize left = size;
	int total = 0;
	int ret;

	if (size == 0)
		return total;
	for (;;) {
		ret = sceHttpReadData(httpRequest, data, left);
		if (ret < 0)
			break;
		if (ret != 0) {
			left -= ret;
			data += ret;
			total += ret;
			if (left != 0)
				continue;
		}
		return total;
	}
	return ret;
}

/* reads the body of the response into the buffer the caller gives, as far as the buffer and the length the
 * server states go; tooLong says some of the body did not fit */
int npCommerce2ReadResponseBody(int httpRequest, const char *base, char *data, SceSize size, int *tooLong, int skip)
{
	char probe[8];
	SceULong64 contentLength = 0;
	SceULong64 stated;
	SceSize expected = 0;
	SceSize capacity;
	SceSize want;
	int total = 0;
	int ended = 0;
	int ret;

	*tooLong = 0;
	sceHttpGetResponseContentLength(httpRequest, &contentLength);
	stated = contentLength;
	if ((SceUInt32)(stated >> 32) != 0)
		return NP_COMMERCE2_ERROR_CONTENT_TOO_LONG;
	capacity = size - (data - base);
	want = capacity;
	if ((SceUInt32)stated != 0) {
		expected = (SceUInt32)stated - skip;
		if ((int)(SceUInt32)stated - skip < want)
			want = (SceUInt32)stated - skip;
	}
	if (want != 0) {
		for (;;) {
			ret = sceHttpReadData(httpRequest, data, want);
			if (ret < 0)
				return ret;
			if (ret == 0) {
				stated = contentLength;
				ended = 1;
				break;
			}
			want -= ret;
			data += ret;
			total += ret;
			if (want == 0) {
				stated = contentLength;
				break;
			}
		}
	}
	if (stated != 0) {
		if (expected > capacity)
			*tooLong = 1;
		else if (total != expected)
			return SCE_NP_COMMERCE2_ERROR_COULD_NOT_RECV_WHOLE_RESPONSE_DATA;
	} else if (ended != 1) {
		ret = sceHttpReadData(httpRequest, probe, 1);
		if (ret < 0)
			return ret;
		if (ret != 0)
			*tooLong = 1;
	}
	return total;
}

/* takes the cookie the response sets and adds it to the template's and the connection's requests */
int npCommerce2ForwardCookie(int httpTemplate, int httpConnection, int httpRequest, char *buf, SceSize size)
{
	char *fieldValue;
	char *header;
	SceSize headerSize = 0;
	SceSize len;
	char *semicolon;

	if (sceHttpGetAllResponseHeaders(httpRequest, &header, &headerSize) >= 0 &&
	    sceHttpParseResponseHeader(header, headerSize, "Set-Cookie", (const char **)&fieldValue, &len) >= 0) {
		buf[0] = '\0';
		if (size > len) {
			sceClibMemcpy_safe(buf, fieldValue, len);
			buf[len] = '\0';
			semicolon = sceClibStrchr(buf, ';');
			if (semicolon != NULL) {
				*semicolon = '\0';
				if (sceHttpAddRequestHeader(httpTemplate, "Cookie", buf, 0) >= 0)
					sceHttpAddRequestHeader(httpConnection, "Cookie", buf, 0);
			}
		}
	}
	return 0;
}

int npCommerce2DeleteHttpRequest(int httpRequest)
{
	return sceHttpDeleteRequest(httpRequest);
}

int npCommerce2AbortHttpRequest(int httpRequest)
{
	return sceHttpAbortRequest(httpRequest);
}
