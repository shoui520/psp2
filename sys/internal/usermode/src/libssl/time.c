/* Native SSL-C time interface. Filename, types, fields and helper names are
 * ours; no upstream donor or Sony source filename has been established here.
 */
#include <stddef.h>
#include "memory.h"
#include "time.h"

static SslTimeAddCallback sslTimeAddCallback;
static SslTimeCompareCallback sslTimeCompareCallback;
static SslTimeDecodeCallback sslTimeDecodeCallback;
static SslTimeEncodeCallback sslTimeEncodeCallback;
static SslTimeCurrentCallback sslTimeCurrentCallback;

SslTime *SceSslInternal_4AAF0D45(SslTime *time)
{
	if (sslTimeCurrentCallback != NULL)
		return sslTimeCurrentCallback(time);
	return NULL;
}

SslTime *sslCreateTime(void)
{
	SslTime *storage = sslAllocate(sizeof(SslTime));
	SslTime *result = NULL;

	if (storage != NULL) {
		if (sslTimeCurrentCallback != NULL)
			result = sslTimeCurrentCallback(storage);
		if (result == NULL)
			sslFree(storage);
	}
	return result;
}

void sslFreeTime(SslTime *time)
{
	sslFree(time);
}

SslTime *sslAddTime(SslTime *result, const SslTime *source, int seconds)
{
	if (sslTimeAddCallback != NULL)
		return sslTimeAddCallback(result, source, seconds);
	return NULL;
}

SslTimeAddCallback sslGetTimeAddCallback(void)
{
	return sslTimeAddCallback;
}

SslTimeAddCallback sslSetTimeAddCallback(SslTimeAddCallback callback)
{
	sslTimeAddCallback = callback;
	return callback;
}

int sslDecodeTime(const void *data, int length, int format, SslTime *result)
{
	if (sslTimeDecodeCallback != NULL)
		return sslTimeDecodeCallback(data, length, format, result);
	return 0;
}

int sslDecodeTimeSeconds(SslTime *result, SceUInt32 seconds)
{
	unsigned char data[4];

	data[0] = seconds >> 24;
	data[1] = seconds >> 16;
	data[2] = seconds >> 8;
	data[3] = seconds;
	if (sslTimeDecodeCallback != NULL)
		return sslTimeDecodeCallback(data, sizeof(data),
					     SSL_TIME_FORMAT_SECONDS, result);
	return 0;
}

int SceSslInternal_6D2A86F0(const SslTime *time, int format, void *data,
			  int capacity)
{
	if (sslTimeEncodeCallback != NULL)
		return sslTimeEncodeCallback(time, format, data, capacity);
	return 0;
}

int sslCompareTime(const SslTime *left, const SslTime *right)
{
	if (sslTimeCompareCallback != NULL)
		return sslTimeCompareCallback(left, right);
	return 0;
}

SslTimeCompareCallback sslGetTimeCompareCallback(void)
{
	return sslTimeCompareCallback;
}

SslTimeCompareCallback sslSetTimeCompareCallback(SslTimeCompareCallback callback)
{
	sslTimeCompareCallback = callback;
	return callback;
}

SslTimeDecodeCallback sslGetTimeDecodeCallback(void)
{
	return sslTimeDecodeCallback;
}

SslTimeDecodeCallback sslSetTimeDecodeCallback(SslTimeDecodeCallback callback)
{
	sslTimeDecodeCallback = callback;
	return callback;
}

SslTimeEncodeCallback sslGetTimeEncodeCallback(void)
{
	return sslTimeEncodeCallback;
}

SslTimeEncodeCallback sslSetTimeEncodeCallback(SslTimeEncodeCallback callback)
{
	sslTimeEncodeCallback = callback;
	return callback;
}

SceUInt32 sslGetTimeSize(void)
{
	return sizeof(SslTime);
}

SslTimeCurrentCallback sslGetTimeCurrentCallback(void)
{
	return sslTimeCurrentCallback;
}

SslTimeCurrentCallback sslSetTimeCurrentCallback(SslTimeCurrentCallback callback)
{
	sslTimeCurrentCallback = callback;
	return callback;
}
