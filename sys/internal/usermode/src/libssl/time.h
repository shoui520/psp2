/* Private descriptive names and filename are ours, not recovered Sony names. */
#ifndef LIBSSL_TIME_H
#define LIBSSL_TIME_H

#include <scetypes.h>

typedef struct SslTime {
	SceInt32 secondsHigh;
	SceUInt32 secondsLow;
	SceUInt32 milliseconds;
} SslTime;

typedef SslTime *(*SslTimeAddCallback)(SslTime *result, const SslTime *source,
				     int seconds);
typedef int (*SslTimeCompareCallback)(const SslTime *left, const SslTime *right);
typedef int (*SslTimeDecodeCallback)(const void *data, int length, int format,
				   SslTime *result);
typedef int (*SslTimeEncodeCallback)(const SslTime *time, int format, void *data,
				   int capacity);
typedef SslTime *(*SslTimeCurrentCallback)(SslTime *time);

/* The native time codec uses this format for unsigned big-endian seconds.
 * This descriptive constant name is ours.
 */
#define SSL_TIME_FORMAT_SECONDS 3

SslTime *SceSslInternal_4AAF0D45(SslTime *time);
SslTime *sslCreateTime(void);
void sslFreeTime(SslTime *time);
SslTime *sslAddTime(SslTime *result, const SslTime *source, int seconds);
SslTimeAddCallback sslGetTimeAddCallback(void);
SslTimeAddCallback sslSetTimeAddCallback(SslTimeAddCallback callback);
int sslDecodeTime(const void *data, int length, int format, SslTime *result);
int sslDecodeTimeSeconds(SslTime *result, SceUInt32 seconds);
int SceSslInternal_6D2A86F0(const SslTime *time, int format, void *data,
			  int capacity);
int sslCompareTime(const SslTime *left, const SslTime *right);
SslTimeCompareCallback sslGetTimeCompareCallback(void);
SslTimeCompareCallback sslSetTimeCompareCallback(SslTimeCompareCallback callback);
SslTimeDecodeCallback sslGetTimeDecodeCallback(void);
SslTimeDecodeCallback sslSetTimeDecodeCallback(SslTimeDecodeCallback callback);
SslTimeEncodeCallback sslGetTimeEncodeCallback(void);
SslTimeEncodeCallback sslSetTimeEncodeCallback(SslTimeEncodeCallback callback);
SceUInt32 sslGetTimeSize(void);
SslTimeCurrentCallback sslGetTimeCurrentCallback(void);
SslTimeCurrentCallback sslSetTimeCurrentCallback(SslTimeCurrentCallback callback);

_Static_assert(sizeof(SslTime) == 12, "SslTime");

#endif
