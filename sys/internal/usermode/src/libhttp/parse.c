/*
 * SceLibHttp's response parsers: a status line into its version, code and reason phrase, and
 * a response header's field value.
 */

#include "http_internal.h"

/* "HTTP/<major>.<minor> ": where the status code starts */
static inline int _sceHttpParseVersion(const char *line, SceSize len, int *major, int *minor)
{
	int pos;

	*major = 0;
	*minor = 0;
	if (len < 8)
		return SCE_HTTP_ERROR_PARSE_HTTP_INVALID_RESPONSE;
	if (sceClibStrncmp(line, "HTTP/", 5) != 0)
		return SCE_HTTP_ERROR_PARSE_HTTP_INVALID_RESPONSE;
	pos = 5;
	if (!isdigit(line[pos]))
		return SCE_HTTP_ERROR_PARSE_HTTP_INVALID_RESPONSE;
	while (isdigit(line[pos])) {
		*major = line[pos] + *major * 10 - '0';
		pos++;
	}
	if (line[pos] != '.')
		return SCE_HTTP_ERROR_PARSE_HTTP_INVALID_RESPONSE;
	pos++;
	if (!isdigit(line[pos]))
		return SCE_HTTP_ERROR_PARSE_HTTP_INVALID_RESPONSE;
	while (isdigit(line[pos])) {
		*minor = line[pos] + *minor * 10 - '0';
		pos++;
	}
	if (line[pos] != ' ')
		return SCE_HTTP_ERROR_PARSE_HTTP_INVALID_RESPONSE;
	return pos + 1;
}

/* the three digits of the status code */
static inline int _sceHttpParseStatusCode(const char *p, SceSize len, int *code)
{
	if (len < 3)
		return SCE_HTTP_ERROR_PARSE_HTTP_INVALID_RESPONSE;
	if (!isdigit(p[0]) || !isdigit(p[1]) || !isdigit(p[2]))
		return SCE_HTTP_ERROR_PARSE_HTTP_INVALID_RESPONSE;
	*code = (p[0] - '0') * 100 + (p[1] - '0') * 10 + (p[2] - '0');
	return 3;
}

/* the reason phrase, up to the LF (or CR LF) */
static inline int _sceHttpParseReasonPhrase(const char *p, SceSize len, const char **phrase,
					    SceSize *phraseLen)
{
	const char *q = p;
	SceSize n = 0;
	SceBool cr;

	while (*q != '\n') {
		n++;
		q++;
		if (n > len)
			return SCE_HTTP_ERROR_PARSE_HTTP_INVALID_RESPONSE;
	}
	cr = 0;
	if (n != 0)
		cr = p[n - 1] == '\r';
	*phraseLen = cr ? n - 1 : n;
	*phrase = p;
	return n + 1;
}

int sceHttpParseStatusLine(const char *statusLine, SceSize lineLen, SceInt32 *httpMajorVer,
			   SceInt32 *httpMinorVer, SceInt32 *responseCode, const char **reasonPhrase,
			   SceSize *phraseLen)
{
	const char *p;
	SceSize len;
	int ret;

	if (statusLine == NULL)
		return SCE_HTTP_ERROR_PARSE_HTTP_INVALID_RESPONSE;
	if (httpMajorVer == NULL || httpMinorVer == NULL || responseCode == NULL ||
	    reasonPhrase == NULL || phraseLen == NULL)
		return SCE_HTTP_ERROR_PARSE_HTTP_INVALID_VALUE;
	ret = _sceHttpParseVersion(statusLine, lineLen, httpMajorVer, httpMinorVer);
	if (ret < 0)
		return ret;
	p = statusLine + ret;
	len = lineLen - ret;
	ret = _sceHttpParseStatusCode(p, len, responseCode);
	if (ret < 0)
		return ret;
	p += ret;
	len -= ret;
	ret = _sceHttpParseReasonPhrase(p, len, reasonPhrase, phraseLen);
	if (ret < 0)
		return ret;
	return p + ret - statusLine;
}

/*
 * Finds "<fieldStr>:" at the start of a line of header and gives its value, folded lines
 * included, without the spaces before it or the line end after it; returns where it ends.
 */
int sceHttpParseResponseHeader(const char *header, SceSize headerLen, const char *fieldStr,
			       const char **fieldValue, SceSize *valueLen)
{
	SceBool lineStart = 1;
	SceBool crlf = 0;
	SceSize fieldLen;
	SceSize pos, start, end;

	if (header == NULL)
		return SCE_HTTP_ERROR_PARSE_HTTP_INVALID_RESPONSE;
	if (fieldStr == NULL || fieldValue == NULL || valueLen == NULL)
		return SCE_HTTP_ERROR_PARSE_HTTP_INVALID_VALUE;
	fieldLen = sceClibStrnlen(fieldStr, 0xfff);
	pos = 0;
	if (headerLen == 0)
		return SCE_HTTP_ERROR_PARSE_HTTP_NOT_FOUND;
	for (;;) {
		if (lineStart != 0) {
			if (!isspace(header[pos]) && fieldLen < headerLen - pos &&
			    sceClibStrncasecmp(fieldStr, &header[pos], fieldLen) == 0 &&
			    header[pos + fieldLen] == ':')
				break;
			lineStart = 0;
		}
		if (header[pos] == '\n')
			lineStart = 1;
		pos++;
		if (pos >= headerLen)
			return SCE_HTTP_ERROR_PARSE_HTTP_NOT_FOUND;
	}
	pos += fieldLen;
	if (pos > headerLen || header[pos] != ':')
		return SCE_HTTP_ERROR_PARSE_HTTP_NOT_FOUND;
	pos++;
	while (pos < headerLen && isspace(header[pos]))
		pos++;
	start = pos;
	end = headerLen;
	for (; pos < headerLen; pos++) {
		if (header[pos] != '\n')
			continue;
		if (pos != 1 && header[pos - 1] == '\r')
			crlf = 1;
		if (pos + 1 < headerLen && header[pos + 1] != ' ' && header[pos + 1] != '\t') {
			if (crlf == 1)
				pos--;
			end = pos;
			break;
		}
	}
	*fieldValue = &header[start];
	*valueLen = end - start;
	return pos;
}
