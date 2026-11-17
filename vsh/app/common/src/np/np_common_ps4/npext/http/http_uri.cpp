/*
 * SceNpCommonPs4: HttpUri (npext/http/http_uri.cpp), a URI taken apart into its elements, which point into
 * one buffer the object owns. The file name is ours.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

extern const void *g_httpUriVtable[];		/* 0x8102a758 */

struct NpCommonPs4HttpUri *SceNpCommonPs4_EF053155(struct NpCommonPs4HttpUri *uri, struct NpCommonPs4Allocator *pool)
{
	uri->vtable = NP_COMMON_PS4_VTABLE(g_httpUriVtable);
	uri->opaque = 0;
	uri->scheme = NULL;
	uri->username = NULL;
	uri->password = NULL;
	uri->hostname = NULL;
	uri->path = NULL;
	uri->query = NULL;
	uri->fragment = NULL;
	uri->port = 0;
	uri->buffer = NULL;
	uri->pool = pool;
	return uri;
}

struct NpCommonPs4HttpUri *SceNpCommonPs4_DBC6B803(struct NpCommonPs4HttpUri *uri, struct NpCommonPs4Allocator *pool)
{
	uri->vtable = NP_COMMON_PS4_VTABLE(g_httpUriVtable);
	uri->opaque = 0;
	uri->scheme = NULL;
	uri->username = NULL;
	uri->password = NULL;
	uri->hostname = NULL;
	uri->path = NULL;
	uri->query = NULL;
	uri->fragment = NULL;
	uri->port = 0;
	uri->buffer = NULL;
	uri->pool = pool;
	return uri;
}

struct NpCommonPs4HttpUri *SceNpCommonPs4_349710F4(struct NpCommonPs4HttpUri *uri)
{
	void *buffer;

	buffer = uri->buffer;
	if (buffer != NULL) {
		uri->vtable = NP_COMMON_PS4_VTABLE(g_httpUriVtable);
		SceNpCommonPs4_38CC9AF5(uri->pool, buffer);
	}
	uri->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return uri;
}

int SceNpCommonPs4_5390A90A(struct NpCommonPs4HttpUri *uri)
{
	void *buffer;

	buffer = uri->buffer;
	if (buffer != NULL) {
		uri->vtable = NP_COMMON_PS4_VTABLE(g_httpUriVtable);
		SceNpCommonPs4_38CC9AF5(uri->pool, buffer);
	}
	uri->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(uri);
}

struct NpCommonPs4HttpUri *SceNpCommonPs4_F75DC58F(struct NpCommonPs4HttpUri *uri)
{
	void *buffer;

	buffer = uri->buffer;
	if (buffer != NULL) {
		uri->vtable = NP_COMMON_PS4_VTABLE(g_httpUriVtable);
		SceNpCommonPs4_38CC9AF5(uri->pool, buffer);
	}
	uri->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return uri;
}

int SceNpCommonPs4_A9EAFF5E(struct NpCommonPs4HttpUri *uri, const char *srcUrl)
{
	int result;
	void *buffer;
	char *scheme;
	char *username;
	char *password;
	char *hostname;
	char *path;
	char *query;
	char *fragment;
	SceUShort16 port;
	SceSize require;
	SceHttpUriElement element;

	if (uri->buffer != NULL)
		SceNpCommonPs4_38CC9AF5(uri->pool, uri->buffer);
	result = sceHttpUriParse(&element, srcUrl, NULL, &require, 0);
	if (result >= 0) {
		buffer = SceNpCommonPs4_299ADFC3(uri->pool, require);
		uri->buffer = buffer;
		if (buffer != NULL) {
			result = sceHttpUriParse(&element, srcUrl, buffer, NULL, require);
			scheme = element.scheme;
			username = element.username;
			password = element.password;
			hostname = element.hostname;
			path = element.path;
			query = element.query;
			fragment = element.fragment;
			port = element.port;
			if (result >= 0) {
				uri->opaque = element.opaque != 0;
				result = 0;
				uri->scheme = scheme;
				uri->username = username;
				uri->password = password;
				uri->hostname = hostname;
				uri->path = path;
				uri->query = query;
				uri->fragment = fragment;
				uri->port = port;
			}
		} else {
			result = ((int *)uri->pool)[4];
		}
	}
	return result;
}

int SceNpCommonPs4_9BCF8626(struct NpCommonPs4HttpUri *uri, char *out, SceSize prepare, SceSize *require,
	SceUInt32 option)
{
	int result;
	SceHttpUriElement element;

	element.opaque = uri->opaque;
	element.scheme = uri->scheme;
	element.username = uri->username;
	element.password = uri->password;
	element.hostname = uri->hostname;
	element.path = uri->path;
	element.query = uri->query;
	element.fragment = uri->fragment;
	element.port = uri->port;
	result = sceHttpUriBuild(out, require, prepare, &element, option);
	if (result >= 0)
		result = 0;
	return result;
}

int SceNpCommonPs4_D9298357(const char *base, const char *path, char *out, SceSize outSize, SceSize *required)
{
	SceSize baseLength;
	SceSize pathLength;
	const char *end;
	const char *last;
	SceSize length;

	baseLength = sceClibStrnlen(base, 0xFFFFFFFF);
	pathLength = sceClibStrnlen(path, 0xFFFFFFFF);
	if (baseLength == 0 || pathLength == 0)
		return NP_COMMON_PS4_ERROR_80559E11;
	if (*path != '/')
		return NP_COMMON_PS4_ERROR_80559E11;
	end = &base[baseLength];
	last = end - 1;
	if (*last == '/') {
		while (1) {
			last--;
			if (last <= base)
				return NP_COMMON_PS4_ERROR_80559E11;
			if (*last != '/')
				break;
		}
	}
	length = last - base + 1;
	if (out != NULL) {
		if (outSize < length + pathLength + 1)
			return NP_COMMON_PS4_ERROR_80559E0D;
		sceClibStrncpy(out, base, length);
		sceClibStrncpy(&out[length], path, pathLength);
		out[length + pathLength] = 0;
	}
	if (required != NULL)
		*required = length + pathLength + 1;
	return 0;
}

const void *g_httpUriVtable[] = {		/* 0x8102a758 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_349710F4,
	(const void *)SceNpCommonPs4_5390A90A,
};

}
