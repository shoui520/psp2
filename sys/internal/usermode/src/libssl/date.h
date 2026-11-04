/* Filename and descriptive private names are ours, not recovered Sony names. */
#ifndef LIBSSL_DATE_H
#define LIBSSL_DATE_H

#include <stddef.h>
#include "CRYPTOCME/common/library/lib_ctx.h"
#include "time.h"

/* Shared date-module interface prefix. */
typedef struct SslDateMethods {
	int (*CreateContext)(SslLibrary *library, int parameter, void **result);
	int (*DestroyContext)(void *context);
	int (*CreateDate)(void *context, void **result);
	int (*DestroyDate)(void *date);
	int (*RetainDate)(void *date);
	int (*CompareDate)(const void *left, const void *right, int *result);
	int (*AddDate)(void *result, const void *source, int seconds);
	int (*EncodeDate)(void *date, int format, void *data,
			  SceUInt32 *length, SceUInt32 capacity);
	int (*DecodeDate)(void *date, int format, const void *data, SceUInt32 length);
} SslDateMethods;

typedef const SslDateMethods *(*SslDateModuleFactory)(void *data);

typedef struct SslDateContext {
	SceUInt32 heapObject;
	const SslDateMethods *methods;
	SceUInt32 opaqueState;
} SslDateContext;

typedef struct SslDateValue {
	SceInt32 secondsHigh;
	SceUInt32 secondsLow;
	SceUInt32 fraction;
} SslDateValue;

typedef struct SslDateObject {
	SceUInt32 heapObject;
	SslDateContext *context;
	SslDateValue value;
} SslDateObject;

#define SSL_DATE_MODULE_TYPE 5

_Static_assert(sizeof(SslDateContext) == 12, "SslDateContext");
_Static_assert(sizeof(SslDateObject) == 20, "SslDateObject");
_Static_assert(offsetof(SslDateObject, value) == 8, "value");
_Static_assert(offsetof(SslDateMethods, DecodeDate) == 32, "DecodeDate");

int SceSslInternal_D3C049B9(SslLibrary *library, int parameter, void **result);
int SceSslInternal_D4ADF069(void *context);
int SceSslInternal_5E50CB55(void *context, void **result);
int SceSslInternal_6CDB4EE8(void *date);
int SceSslInternal_04BAED82(const void *left, const void *right, int *result);
int SceSslInternal_D1A82B1A(void *date, int format, void *data,
			  SceUInt32 *length, SceUInt32 capacity);
int SceSslInternal_88391E03(void *date, int format, const void *data, SceUInt32 length);
int sslRetainDateObject(void *date);
int sslAddDateObject(void *result, const void *source, int seconds);
int sslCopyDateObject(void *date, void **result);
int sslNewDateObject(void *context, void **result);
int sslFreeDateObject(void *date);
int sslAddDateValue(void *result, const void *source, int seconds);
int sslNewDateContext(SslLibrary *library, int parameter, void **result);
int sslFreeDateContext(void *context);
int sslCompareDateValues(const void *left, const void *right, int *result);

#endif
