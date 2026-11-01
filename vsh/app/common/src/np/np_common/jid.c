/*
 * SceNpCommon: Jabber IDs. An NP ID is the JID onlineId@xx.yy.<environment>.playstation.net
 * with the platform as its resource; xx and yy are the two halves of opt[0..3]. Also the
 * URL form encoding of a string.
 */

#include "np_common_internal.h"

#define NP_IS_ALNUM(c) \
	((sceClibLookCtypeTable(c) & (SCE_CLIB_CTYPE_UPPER | SCE_CLIB_CTYPE_LOWER | SCE_CLIB_CTYPE_DIGIT)) != 0)

static const char s_npEnvironment[] = "rc";

/* 1: the character is sent as is, 3: as %XX */
static const signed char s_urlCharacterSize[256] = {
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 3, 3, 1, 1, 3,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3,
	3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1,
	3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
};

static char s_npEnvironmentName[17] = "rc";

/*
 * URL form encoding of size bytes: space as '+', other reserved bytes as %XX. Without dst it
 * only measures; *required gets the size with the terminator.
 */
int SceNpCommon_14BC2B99(char *dst, SceSize dstSize, const SceUInt8 *src, SceSize size,
			 SceSize *required)
{
	static const char hex[] = "0123456789ABCDEF";
	const SceUInt8 *end;
	SceSize length = 0;
	int write = 1;

	if (src == NULL || size == 0)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (dst == NULL) {
		if (required == NULL)
			return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
		write = 0;
	}
	for (end = src + size; src < end; src++) {
		if (*src == ' ') {
			if (++length <= dstSize) {
				if (write)
					*dst++ = '+';
			} else if (write) {
				return SCE_NP_UTIL_ERROR_INSUFFICIENT;
			}
		} else if (s_urlCharacterSize[*src] == 3) {
			length += 3;
			if (length <= dstSize) {
				if (write) {
					dst[0] = '%';
					dst[1] = hex[(*src >> 4) & 0xf];
					dst[2] = hex[*src & 0xf];
					dst += 3;
				}
			} else if (write) {
				return SCE_NP_UTIL_ERROR_INSUFFICIENT;
			}
		} else if (++length <= dstSize) {
			if (write)
				*dst++ = *src;
		} else if (write) {
			return SCE_NP_UTIL_ERROR_INSUFFICIENT;
		}
	}
	length++;
	if (length <= dstSize) {
		if (write)
			*dst = '\0';
	} else if (write) {
		return SCE_NP_UTIL_ERROR_INSUFFICIENT;
	}
	if (required != NULL)
		*required = length;
	return 0;
}

/* copies a part of a JID with its terminator, or an empty part */
static inline int npJidCopyPart(char *dst, SceSize dstSize, const char *src, SceSize length,
				SceSize *used, SceSize *copied, int write)
{
	if (length != 0) {
		if (write) {
			if (length + 1 > dstSize)
				return SCE_NP_UTIL_ERROR_INSUFFICIENT;
			sceClibMemcpy_safe(dst, src, length);
			dst[length] = '\0';
		}
		if (used != NULL)
			*used = length + 1;
		if (copied != NULL)
			*copied = length;
		return 0;
	}
	if (used != NULL)
		*used = 1;
	if (write) {
		if (dstSize == 0)
			return SCE_NP_UTIL_ERROR_INSUFFICIENT;
		dst[0] = '\0';
	}
	if (copied != NULL)
		*copied = 0;
	return 0;
}

static inline SceSize npStrnlen(const char *str, SceSize count)
{
	SceSize length = 0;

	if (str == NULL || count == 0)
		return 0;
	do {
		if (str[length] == '\0')
			break;
		length++;
	} while (length < count);
	return length;
}

/*
 * Splits a JID into its parts, copied into buf; without jid or buf it only measures
 * (*required gets the buffer size it needs).
 */
int SceNpCommon_D2B9BCA8(NpJid *jid, const char *str, char *buf, SceSize bufSize,
			 SceSize *required)
{
	SceSize length, start, pos, count, used = 0, copied = 0, nodeUsed, offset;
	const char *part;
	int write = 1;
	int ret;
	char c;

	if (str == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (buf != NULL && jid != NULL) {
		sceClibMemset(jid, 0, sizeof(*jid));
	} else {
		if (required == NULL)
			return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
		write = 0;
	}
	length = sceClibStrnlen(str, 0x7fffffff);
	for (start = 0; start < length && str[start] == ' '; start++)
		;
	if (str[start] == '\0')
		return SCE_NP_UTIL_ERROR_INVALID_PROTOCOL_ID;

	/* the node, when an '@' ends it */
	part = &str[start];
	count = 0;
	if (length != start) {
		do {
			c = part[count];
			if (!NP_IS_ALNUM(c) && c != '+' && c != '-' && c != '_' && c != '.')
				break;
			count++;
		} while (count < length - start);
	}
	if (count == 0 || part[count] != '@')
		count = 0;
	ret = npJidCopyPart(buf, bufSize, part, count, &used, &copied, write);
	if (ret < 0)
		goto error;
	nodeUsed = used;
	if (write)
		jid->node = buf;

	/* the domain, without trailing dots */
	pos = start + copied;
	if (str[pos] == '@' && pos < length)
		pos++;
	part = &str[pos];
	count = 0;
	if (length != pos) {
		do {
			c = part[count];
			if (!NP_IS_ALNUM(c) && c != '.' && c != '_' && c != '-')
				break;
			count++;
		} while (count < length - pos);
	}
	while (count != 0 && part[count - 1] == '.')
		count--;
	ret = npJidCopyPart(buf + nodeUsed, bufSize - nodeUsed, part, count, &used, &copied, write);
	if (ret < 0)
		goto error;
	if (write)
		jid->domain = buf + nodeUsed;

	/* the resource, after the separator */
	pos += copied;
	offset = nodeUsed + used;
	if (pos < length)
		pos++;
	part = &str[pos];
	count = npStrnlen(part, length - pos);
	ret = npJidCopyPart(buf + offset, bufSize - offset, part, count, &used, NULL, write);
	if (ret < 0)
		goto error;
	if (write)
		jid->resource = buf + offset;
	if (required != NULL)
		*required = offset + used;
	return 0;

error:
	if (write)
		sceClibMemset(jid, 0, sizeof(*jid));
	return ret;
}

/* joins a JID's parts into node@domain/resource; *required gets the size it needs */
int SceNpCommon_85699AE2(const NpJid *jid, char *buf, SceSize bufSize, SceSize *required)
{
	SceSize nodeLength = 0, domainLength, resourceLength = 0, size, pos = 0, end;

	if (jid == NULL || jid->domain == NULL || jid->domain[0] == '\0')
		return SCE_NP_UTIL_ERROR_INVALID_PROTOCOL_ID;
	if (jid->node != NULL && jid->node[0] != '\0')
		nodeLength = sceClibStrnlen(jid->node, 0x7fffffff);
	domainLength = sceClibStrnlen(jid->domain, 0x7fffffff);
	if (jid->resource != NULL && jid->resource[0] != '\0')
		resourceLength = sceClibStrnlen(jid->resource, 0x7fffffff);
	size = nodeLength != 0 ? nodeLength + 1 : 0;
	size += domainLength;
	if (resourceLength != 0)
		size += resourceLength + 1;
	size++;
	if (required != NULL)
		*required = size;
	if (buf == NULL)
		return 0;
	if (bufSize < size)
		return SCE_NP_UTIL_ERROR_INSUFFICIENT;
	if (nodeLength != 0) {
		sceClibMemcpy_safe(buf, jid->node, nodeLength);
		buf[nodeLength] = '@';
		pos = nodeLength + 1;
	}
	sceClibMemcpy_safe(&buf[pos], jid->domain, domainLength);
	pos += domainLength;
	end = pos + 1;
	if (resourceLength != 0) {
		buf[pos] = '/';
		sceClibMemcpy_safe(&buf[end], jid->resource, resourceLength);
		pos = end + resourceLength;
		end += resourceLength + 1;
	}
	buf[pos] = '\0';
	if (required != NULL)
		*required = end;
	return 0;
}

int SceNpCommon_652FC5E0(const char *environment)
{
	SceSize length;

	if (environment == NULL)
		return 0;
	length = sceClibStrnlen(environment, 0x7fffffff);
	if (length > 16)
		return SCE_NP_UTIL_ERROR_INVALID_NP_ENV;
	sceClibMemcpy_safe(s_npEnvironmentName, environment, length);
	if (length < sizeof(s_npEnvironmentName))
		sceClibMemset(&s_npEnvironmentName[length], 0, sizeof(s_npEnvironmentName) - length);
	return 0;
}

int sceNpCommonGetNpEnviroment(char *environment)
{
	sceClibStrncpy(environment, s_npEnvironment, 16);
	return 0;
}

/* the NP ID a JID names */
int SceNpCommon_9B7CAA67(SceNpId *npId, const NpJid *jid)
{
	SceSize nodeLength, resourceLength = 0;
	const char *domain, *p;

	if (jid == NULL || npId == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (jid->node == NULL || jid->domain == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_PROTOCOL_ID;
	nodeLength = sceClibStrnlen(jid->node, 0x7fffffff);
	if (nodeLength == 0 || nodeLength > SCE_NP_ONLINEID_MAX_LENGTH)
		return SCE_NP_UTIL_ERROR_INVALID_PROTOCOL_ID;
	domain = jid->domain;
	if (!NP_IS_ALNUM((SceUChar8)domain[0]) || !NP_IS_ALNUM((SceUChar8)domain[1]) || domain[2] != '.' ||
	    !NP_IS_ALNUM((SceUChar8)domain[3]) || !NP_IS_ALNUM((SceUChar8)domain[4]) || domain[5] != '.')
		return SCE_NP_UTIL_ERROR_INVALID_PROTOCOL_ID;
	for (p = &domain[6]; *p != '.'; p++) {
		if (*p == '\0')
			return SCE_NP_UTIL_ERROR_INVALID_PROTOCOL_ID;
	}
	if (sceClibStrncmp(p + 1, "playstation.net", 16) != 0)
		return SCE_NP_UTIL_ERROR_INVALID_PROTOCOL_ID;
	if (jid->resource != NULL) {
		resourceLength = sceClibStrnlen(jid->resource, 0x7fffffff);
		if (resourceLength > 4)
			return SCE_NP_UTIL_ERROR_INVALID_PROTOCOL_ID;
	}
	sceClibMemset(npId, 0, sizeof(*npId));
	sceClibMemcpy_safe(npId->handle.data, jid->node, nodeLength);
	npId->opt[0] = domain[0];
	npId->opt[1] = domain[1];
	npId->opt[2] = domain[3];
	npId->opt[3] = domain[4];
	if (resourceLength != 0)
		sceClibMemcpy_safe(&npId->opt[4], jid->resource, resourceLength);
	npId->reserved[0] = 1;
	return 0;
}

/*
 * The JID of an NP ID in the environment set by SceNpCommon_652FC5E0, its parts in buf; without jid or buf it only measures. On success
 * Sony's *required is the online ID's length plus 10, not the size it measured.
 */
int SceNpCommon_40CC4D15(NpJid *jid, const SceNpId *npId, char *buf, SceSize bufSize,
			 SceSize *required)
{
	SceSize nodeLength, platformLength, environmentLength, size;
	const SceUChar8 *p;
	char *domain, *resource;
	int write = 1;

	if (npId == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (npId->reserved[0] != 1)
		return SCE_NP_UTIL_ERROR_INVALID_NP_ID;
	if (buf != NULL && jid != NULL) {
		sceClibMemset(jid, 0, sizeof(*jid));
	} else {
		if (required == NULL)
			return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
		write = 0;
	}
	nodeLength = sceClibStrnlen(npId->handle.data, 0x7fffffff);
	if (nodeLength == 0)
		return SCE_NP_UTIL_ERROR_INVALID_NP_ID;
	if (!NP_IS_ALNUM(npId->opt[0]) || !NP_IS_ALNUM(npId->opt[1]) ||
	    !NP_IS_ALNUM(npId->opt[2]) || !NP_IS_ALNUM(npId->opt[3]))
		return SCE_NP_UTIL_ERROR_INVALID_NP_ID;
	platformLength = 0;
	for (p = &npId->opt[4]; p < &npId->opt[8]; p++) {
		if (*p == '\0')
			break;
		platformLength++;
	}
	environmentLength = sceClibStrnlen(s_npEnvironmentName, 0x7fffffff);
	size = environmentLength + nodeLength + platformLength + 25;
	if (required != NULL)
		*required = size;
	if (!write)
		return 0;
	if (size > bufSize)
		return SCE_NP_UTIL_ERROR_INSUFFICIENT;
	jid->node = buf;
	sceClibMemcpy_safe(buf, npId->handle.data, nodeLength);
	buf[nodeLength] = '\0';
	jid->domain = &buf[nodeLength + 1];
	domain = &buf[nodeLength];
	domain[1] = npId->opt[0];
	domain[2] = npId->opt[1];
	domain[3] = '.';
	domain[4] = npId->opt[2];
	domain[5] = npId->opt[3];
	domain[6] = '.';
	sceClibMemcpy_safe(&buf[nodeLength + 7], s_npEnvironmentName, environmentLength);
	buf[nodeLength + 7 + environmentLength] = '.';
	sceClibMemcpy_safe(&buf[nodeLength + 8 + environmentLength], "playstation.net", 15);
	buf[nodeLength + 23 + environmentLength] = '\0';
	resource = &buf[nodeLength + 24 + environmentLength];
	jid->resource = resource;
	if (platformLength != 0)
		sceClibMemcpy_safe(resource, &npId->opt[4], platformLength);
	buf[nodeLength + 24 + environmentLength + platformLength] = '\0';
	if (required != NULL)
		*required = nodeLength + 10;
	return 0;
}
