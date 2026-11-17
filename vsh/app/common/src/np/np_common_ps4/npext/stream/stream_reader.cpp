/*
 * SceNpCommonPs4: reading from a stream (a reader of bytes, with an observer that sees the bytes read).
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

typedef int (*NpCommonPs4StreamReadFunction)(struct NpCommonPs4StreamReader *reader, int context, void *buffer, SceSize size,
	SceSize *pRead);
typedef int (*NpCommonPs4StreamObserveFunction)(struct NpCommonPs4StreamObserver *observer, void *buffer, SceSize size);

#define NP_COMMON_PS4_STREAM_READ(reader) ((NpCommonPs4StreamReadFunction)(reader)->vtable[2])
#define NP_COMMON_PS4_STREAM_OBSERVE(observer) ((NpCommonPs4StreamObserveFunction)(observer)->vtable[2])

int SceNpCommonPs4_7C84C1D5(struct NpCommonPs4StreamReader *reader, int context, struct NpCommonPs4StreamObserver *observer,
	void *buffer, SceSize size, SceSize *pRead)
{
	int result;

	result = NP_COMMON_PS4_STREAM_READ(reader)(reader, context, buffer, size, pRead);
	if (result >= 0) {
		if (observer == NULL)
			return 0;
		result = NP_COMMON_PS4_STREAM_OBSERVE(observer)(observer, buffer, *pRead);
		if (result >= 0)
			return 0;
	}
	return result;
}

__attribute__((noinline)) int SceNpCommonPs4_C4D5B918(struct NpCommonPs4StreamReader *reader, int context, struct NpCommonPs4StreamObserver *observer,
	void *buffer, SceSize nbytes, SceSize *pTotal)
{
	SceSize total;
	SceSize remaining;
	SceSize rsize;
	SceUInt8 extra[4];
	NpCommonPs4StreamReadFunction read;
	int result;
	static const char functionName[] = "ReadAll";

	total = 0;
	rsize = 0;
	remaining = nbytes;
	read = NP_COMMON_PS4_STREAM_READ(reader);
	if (nbytes != 0) {
		while (1) {
			result = read(reader, context, buffer, remaining, &rsize);
			if (result >= 0 && (observer == NULL || (result = NP_COMMON_PS4_STREAM_OBSERVE(observer)(observer, buffer, rsize)) >= 0))
				result = 0;
			if (result < 0)
				break;
			NP_COMMON_PS4_ASSERT(rsize <= remaining, functionName, "npext\\stream\\stream_reader.cpp", 74, "rsize <= nbytes-total");
			if (rsize == 0)
				goto done;
			total += rsize;
			remaining = nbytes - total;
			read = NP_COMMON_PS4_STREAM_READ(reader);
			if (nbytes == total)
				goto check;
		}
	} else {
check:
		result = read(reader, context, extra, 1, &rsize);
		if (result >= 0 && (observer == NULL || (result = NP_COMMON_PS4_STREAM_OBSERVE(observer)(observer, extra, rsize)) >= 0))
			result = 0;
		if (result >= 0) {
			if (rsize != 0) {
				result = NP_COMMON_PS4_ERROR_80559E0D;
			} else {
done:
				result = 0;
				*pTotal = total;
			}
		}
	}
	return result;
}


int SceNpCommonPs4_2047507F(struct NpCommonPs4StreamReader *reader, int context, void *buffer, SceSize nbytes, SceSize *pTotal)
{
	return SceNpCommonPs4_C4D5B918(reader, context, NULL, buffer, nbytes, pTotal);
}

__attribute__((noinline)) int SceNpCommonPs4_F5C0066F(struct NpCommonPs4StreamReader *reader, int context,
	struct NpCommonPs4StreamObserver *observer, SceUInt8 *buffer, SceSize nbytes, SceSize *pTotal)
{
	SceSize total;
	SceSize remaining;
	SceSize rsize;
	int result;
	volatile int ok;

	total = 0;
	ok = 0;
	rsize = 0;
	remaining = nbytes;
	while (1) {
		result = NP_COMMON_PS4_STREAM_READ(reader)(reader, context, buffer + total, remaining, &rsize);
		if (result >= 0 && (observer == NULL || (result = NP_COMMON_PS4_STREAM_OBSERVE(observer)(observer, buffer + total, rsize)) >= 0))
			result = ok;
		if (result < 0)
			break;
		total += rsize;
		remaining = nbytes - total;
		if (nbytes == total || rsize == 0) {
			if (total == nbytes)
				result = ok;
			else
				result = NP_COMMON_PS4_ERROR_80559E10;
			break;
		}
	}
	if (pTotal != NULL)
		*pTotal = total;
	return result;
}

int SceNpCommonPs4_672E5C18(struct NpCommonPs4StreamReader *reader, int context, SceUInt8 *buffer, SceSize nbytes, SceSize *pTotal)
{
	return SceNpCommonPs4_F5C0066F(reader, context, NULL, buffer, nbytes, pTotal);
}

int SceNpCommonPs4_770D5A51(struct NpCommonPs4StreamReader *reader, int context, struct NpCommonPs4StreamObserver *observer, int padding,
	SceInt64 count, SceInt64 *pTotal)
{
	SceSize rsize;
	SceSize chunk;
	SceUInt8 buffer[256];
	int result;

	if (count > 0) {
		while (1) {
			chunk = count;
			if (count >= 256)
				chunk = 256;
			rsize = 0;
			result = SceNpCommonPs4_F5C0066F(reader, context, observer, buffer, chunk, &rsize);
			if (pTotal != NULL)
				*pTotal += rsize;
			if (result < 0)
				break;
			count -= rsize;
			if (count <= 0)
				return 0;
		}
	} else {
		result = 0;
	}
	return result;
}

int SceNpCommonPs4_5C54CDE7(struct NpCommonPs4StreamReader *reader, int context, SceInt64 count, SceInt64 *pTotal)
{
	SceSize rsize;
	SceSize chunk;
	SceUInt8 buffer[256];
	int result;

	if (count > 0) {
		while (1) {
			chunk = count;
			if (count >= 256)
				chunk = 256;
			rsize = 0;
			result = SceNpCommonPs4_F5C0066F(reader, context, NULL, buffer, chunk, &rsize);
			if (pTotal != NULL)
				*pTotal += rsize;
			if (result < 0)
				break;
			count -= rsize;
			if (count <= 0)
				return 0;
		}
	} else {
		result = 0;
	}
	return result;
}

__attribute__((noinline)) int SceNpCommonPs4_BA4F694C(struct NpCommonPs4StreamReader *reader, int context,
	struct NpCommonPs4StreamObserver *observer, SceInt64 *pTotal)
{
	SceUInt8 buffer[256];
	SceSize rsize;
	int result;

	while (1) {
		rsize = 0;
		result = NP_COMMON_PS4_STREAM_READ(reader)(reader, context, buffer, 256, &rsize);
		if (result >= 0 && (observer == NULL || (result = NP_COMMON_PS4_STREAM_OBSERVE(observer)(observer, buffer, rsize)) >= 0))
			result = 0;
		if (pTotal != NULL)
			*pTotal += rsize;
		if (result < 0)
			break;
		if (rsize == 0) {
			result = 0;
			break;
		}
	}
	return result;
}

int SceNpCommonPs4_F0E6D965(struct NpCommonPs4StreamReader *reader, int context, SceInt64 *pTotal)
{
	return SceNpCommonPs4_BA4F694C(reader, context, NULL, pTotal);
}

int SceNpCommonPs4_50F92428(struct NpCommonPs4StreamReader *reader, int context, struct NpCommonPs4StreamObserver *observer,
	void *buffer, SceSize size, SceSize *pRead)
{
	int result;

	result = NP_COMMON_PS4_STREAM_READ(reader)(reader, context, buffer, size, pRead);
	if (result >= 0) {
		if (observer == NULL)
			return 0;
		result = NP_COMMON_PS4_STREAM_OBSERVE(observer)(observer, buffer, *pRead);
		if (result >= 0)
			return 0;
	}
	return result;
}

__attribute__((noinline)) int SceNpCommonPs4_DA842424(struct NpCommonPs4StreamReader *reader, int context,
	struct NpCommonPs4StreamObserver *observer, SceUInt8 *buffer, SceSize nbytes, SceSize *pTotal)
{
	SceSize total;
	SceSize rsize;
	int result;

	total = 0;
	rsize = 0;
	if (nbytes != 0) {
		while (1) {
			result = NP_COMMON_PS4_STREAM_READ(reader)(reader, context, buffer + total, nbytes - total, &rsize);
			if (result >= 0 && (observer == NULL || (result = NP_COMMON_PS4_STREAM_OBSERVE(observer)(observer, buffer + total, rsize)) >= 0))
				result = 0;
			if (result < 0)
				break;
			total += rsize;
			if (nbytes <= total)
				goto done;
		}
	} else {
done:
		result = 0;
	}
	if (pTotal != NULL)
		*pTotal = total;
	return result;
}

int SceNpCommonPs4_ECBB0877(struct NpCommonPs4StreamReader *reader, int context, SceUInt8 *buffer, SceSize nbytes, SceSize *pTotal)
{
	return SceNpCommonPs4_DA842424(reader, context, NULL, buffer, nbytes, pTotal);
}

__attribute__((noinline)) int SceNpCommonPs4_E978DD60(struct NpCommonPs4StreamReader *reader, int context,
	struct NpCommonPs4StreamObserver *observer, char fill, SceInt64 count)
{
	SceUInt8 buffer[256];
	SceSize chunk;
	SceSize rsize;
	int result;

	sceClibMemset(buffer, fill, 256);
	if (count > 0) {
		while (1) {
			chunk = 256;
			if (count <= 256)
				chunk = count;
			result = NP_COMMON_PS4_STREAM_READ(reader)(reader, context, buffer, chunk, &rsize);
			if (result < 0)
				break;
			if (observer != NULL) {
				result = NP_COMMON_PS4_STREAM_OBSERVE(observer)(observer, buffer, rsize);
				if (result < 0)
					break;
			}
			count -= rsize;
			if (count <= 0)
				return 0;
		}
	} else {
		result = 0;
	}
	return result;
}

int SceNpCommonPs4_ABD26D32(struct NpCommonPs4StreamReader *reader, int context, char fill, int padding, SceInt64 count)
{
	return SceNpCommonPs4_E978DD60(reader, context, NULL, fill, count);
}

}
