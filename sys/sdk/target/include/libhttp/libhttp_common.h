/*
 * libhttp/libhttp_common.h: under Sony's names and with Sony's definitions
 * (SDK Release 03.570.011, target/include/libhttp/libhttp_common.h).
 */
#ifndef _SCE_HTTP_LIBHTTP_COMMON_H
#define _SCE_HTTP_LIBHTTP_COMMON_H

#define SCE_HTTP_ENABLE				(1)
#define SCE_HTTP_DISABLE			(0)

#define SCE_HTTP_USERNAME_MAX_SIZE	256
#define SCE_HTTP_PASSWORD_MAX_SIZE	256

typedef enum{
	SCE_HTTP_VERSION_1_0=1,
	SCE_HTTP_VERSION_1_1
} SceHttpHttpVersion;

typedef enum{
	SCE_HTTP_PROXY_AUTO,
	SCE_HTTP_PROXY_MANUAL
} SceHttpProxyMode;

typedef enum{
	SCE_HTTP_HEADER_OVERWRITE,
	SCE_HTTP_HEADER_ADD
} SceHttpAddHeaderMode;

typedef enum{
	SCE_HTTP_AUTH_BASIC,
	SCE_HTTP_AUTH_DIGEST,
	SCE_HTTP_AUTH_RESERVED0,
	SCE_HTTP_AUTH_RESERVED1,
	SCE_HTTP_AUTH_RESERVED2
} SceHttpAuthType;

#endif
