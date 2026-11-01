/*
 * libhttp/methods.h: under Sony's names and with Sony's definitions
 * (SDK Release 03.570.011, target/include/libhttp/methods.h).
 */
#ifndef _SCE_HTTP_METHODS_H
#define _SCE_HTTP_METHODS_H

typedef enum {
	SCE_HTTP_METHOD_GET,
	SCE_HTTP_METHOD_POST,
	SCE_HTTP_METHOD_HEAD,
	SCE_HTTP_METHOD_OPTIONS,
	SCE_HTTP_METHOD_PUT,
	SCE_HTTP_METHOD_DELETE,
	SCE_HTTP_METHOD_TRACE,
	SCE_HTTP_METHOD_CONNECT
} SceHttpMethods;

#endif
