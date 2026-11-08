/* Native SSL-C time interface. Filename, types, fields and helper names are
 * ours; no upstream donor or Sony source filename has been established here.
 */
#include <stddef.h>
#include <kernel/threadmgr.h>
#include "memory.h"
#include "time.h"

static SslTimeAddCallback sslTimeAddCallback;
static SslTimeCompareCallback sslTimeCompareCallback;
static SslTimeDecodeCallback sslTimeDecodeCallback;
static SslTimeEncodeCallback sslTimeEncodeCallback;
static SslTimeCurrentCallback sslTimeCurrentCallback;

int sslInitPlatformTimeCallbacks(void)
{
	sslSetTimeCurrentCallback(sslGetPlatformCurrentTime);
	sslSetTimeAddCallback(sslAddPlatformTime);
	sslSetTimeCompareCallback(sslComparePlatformTime);
	sslSetTimeDecodeCallback(sslDecodePlatformTime);
	sslSetTimeEncodeCallback(sslEncodePlatformTime);
	return 1;
}

__attribute__((noinline)) SslTime *sslGetPlatformCurrentTime(SslTime *time)
{
	SceInt64 now;
	SceUInt64 seconds;

	if (time != NULL) {
		now = sceKernelGetSystemTimeWide();
		seconds = (SceUInt64)now / 1000000ULL;
		time->secondsHigh = (SceInt32)(seconds >> 32);
		time->secondsLow = (SceUInt32)seconds;
		time->milliseconds = (SceUInt32)((SceUInt64)now / 1000ULL);
	}
	return time;
}

__attribute__((noinline)) SslTime *sslAddPlatformTime(SslTime *result, const SslTime *source,
			    int seconds, int milliseconds)
{
	SceUInt32 base;

	(void)milliseconds;
	if (result != NULL && source != NULL) {
		base = source->secondsLow;
		if (seconds <= 0) {
			if (seconds < 0 && base < (SceUInt32)(-seconds))
				return NULL;
		} else if (0xffffffffU - base < (SceUInt32)seconds) {
			return NULL;
		}
		result->secondsLow = base + (SceUInt32)seconds;
		return result;
	}
	return NULL;
}

__attribute__((noinline)) int sslEncodePlatformTime(const SslTime *time, int format, void *data,
			  int capacity)
{
	static const unsigned char endianMarker[4] = { 1, 0, 0, 0 };
	int status = 0;
	int length;
	int shift;
	const char *src;
	char *dst;

	if (data == NULL) {
		status = 10017;
	} else if (format == SSL_TIME_FORMAT_SECONDS) {
		length = 4;
		shift = 32;
		do {
			shift -= 8;
			if ((unsigned char)(time->secondsLow >> shift) != 0)
				break;
			length--;
		} while (length > 0);

		if (capacity < length) {
			status = 10016;
		} else {
			sslFillMemory(data, 0, capacity);
			src = (const char *)&time->secondsLow;
			if (endianMarker[0] != 0) {
				dst = (char *)data + capacity - 1;
				if (length > 0) {
					do {
						*dst-- = *src++;
						length--;
					} while (length != 0);
				}
			} else {
				sslCopyMemory((char *)data + capacity - length,
					      (const char *)(time + 1) - length - 4,
					      length);
			}
		}
	} else {
		status = 10020;
	}
	return status;
}

__attribute__((noinline)) int sslDecodePlatformTime(const void *data, int length, int format,
			  SslTime *result)
{
	static const unsigned char endianMarker[4] = { 1, 0, 0, 0 };
	int status = 0;
	int copyLength = 4;
	char *dst;
	const char *src;

	if (data == NULL) {
		status = 10017;
	} else if (format == SSL_TIME_FORMAT_SECONDS) {
		if ((unsigned int)length > 4U) {
			status = 10023;
		} else {
			if (length < 4)
				copyLength = length;
			result->secondsLow = 0;
			result->milliseconds = 0;
			dst = (char *)result - copyLength + 8;
			result->secondsHigh = 0;
			if (endianMarker[0] != 0) {
				src = (const char *)data + length - 1;
				if (copyLength > 0) {
					do {
						*dst++ = *src--;
						copyLength--;
					} while (copyLength != 0);
				}
			} else {
				sslCopyMemory(dst, data, copyLength);
			}
		}
	} else {
		status = 10020;
	}
	return status;
}

__attribute__((noinline)) int sslComparePlatformTime(const SslTime *left, const SslTime *right)
{
	SslTime fallback;
	SceUInt32 leftSeconds;
	SceUInt32 rightSeconds;

	if (left == NULL)
		left = &fallback;
	leftSeconds = left->secondsLow;
	if (right == NULL)
		right = &fallback;
	rightSeconds = right->secondsLow;
	if (leftSeconds > rightSeconds)
		return 1;
	if (leftSeconds < rightSeconds)
		return -1;
	return 0;
}

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

SslTime *sslAddTime(SslTime *result, const SslTime *source, int seconds,
		    int milliseconds)
{
	if (sslTimeAddCallback != NULL)
		return sslTimeAddCallback(result, source, seconds, milliseconds);
	return NULL;
}

SslTimeAddCallback sslGetTimeAddCallback(void)
{
	return sslTimeAddCallback;
}

__attribute__((noinline)) SslTimeAddCallback sslSetTimeAddCallback(SslTimeAddCallback callback)
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

SceUInt32 sslEncodeTimeSeconds(const SslTime *time)
{
	unsigned char data[4];

	if (sslTimeEncodeCallback != NULL &&
	    sslTimeEncodeCallback(time, SSL_TIME_FORMAT_SECONDS, data,
				  sizeof(data)) != 0)
		return 0xffffffffU;
	return ((SceUInt32)data[0] << 24) | ((SceUInt32)data[1] << 16) |
	       ((SceUInt32)data[2] << 8) | (SceUInt32)data[3];
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

__attribute__((noinline)) SslTimeCompareCallback sslSetTimeCompareCallback(SslTimeCompareCallback callback)
{
	sslTimeCompareCallback = callback;
	return callback;
}

SslTimeDecodeCallback sslGetTimeDecodeCallback(void)
{
	return sslTimeDecodeCallback;
}

__attribute__((noinline)) SslTimeDecodeCallback sslSetTimeDecodeCallback(SslTimeDecodeCallback callback)
{
	sslTimeDecodeCallback = callback;
	return callback;
}

SslTimeEncodeCallback sslGetTimeEncodeCallback(void)
{
	return sslTimeEncodeCallback;
}

__attribute__((noinline)) SslTimeEncodeCallback sslSetTimeEncodeCallback(SslTimeEncodeCallback callback)
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

__attribute__((noinline)) SslTimeCurrentCallback sslSetTimeCurrentCallback(SslTimeCurrentCallback callback)
{
	sslTimeCurrentCallback = callback;
	return callback;
}
