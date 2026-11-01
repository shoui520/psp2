/*
 * libhttp/libhttp_url.h: under Sony's names and with Sony's definitions
 * (SDK Release 03.570.011, target/include/libhttp/libhttp_url.h).
 */
#ifndef _SCE_HTTP_LIBHTTP_URL_H
#define _SCE_HTTP_LIBHTTP_URL_H

#include <scetypes.h>

#define SCE_HTTP_URI_BUILD_WITH_SCHEME		0x01
#define SCE_HTTP_URI_BUILD_WITH_HOSTNAME	0x02
#define SCE_HTTP_URI_BUILD_WITH_PORT		0x04
#define SCE_HTTP_URI_BUILD_WITH_PATH		0x08
#define SCE_HTTP_URI_BUILD_WITH_USERNAME	0x10
#define SCE_HTTP_URI_BUILD_WITH_PASSWORD	0x20
#define SCE_HTTP_URI_BUILD_WITH_QUERY		0x40
#define SCE_HTTP_URI_BUILD_WITH_FRAGMENT	0x80
#define SCE_HTTP_URI_BUILD_WITH_ALL			0xFFFF

typedef struct SceHttpUriElement{
	SceBool		opaque;
	char		*scheme	;
	char		*username;
	char		*password;
	char		*hostname;
	char		*path;
	char		*query;
	char		*fragment;
	SceUShort16	port;
	SceUChar8	reserved[10];
} SceHttpUriElement;

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif

int sceHttpUriEscape(char *out, SceSize *require, SceSize prepare,
				 const char *in);
int sceHttpUriUnescape(char *out, SceSize *require, SceSize prepare,
				   const char *in);
int sceHttpUriParse(SceHttpUriElement *out, const char *srcUrl, void *pool,
				SceSize *require, SceSize prepare);
int sceHttpUriBuild(char *out, SceSize *require, SceSize prepare,
				const SceHttpUriElement *srcElement, SceUInt32 option);
int sceHttpUriMerge(char *mergedUrl, const char *url, const char *relativeUrl,
				SceSize *require, SceSize prepare, SceUInt32 option);
int sceHttpUriSweepPath(char *dst, const char *src, SceSize srcSize);

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif

#endif
