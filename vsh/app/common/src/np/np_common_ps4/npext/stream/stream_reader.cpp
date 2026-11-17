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

int SceNpCommonPs4_C4D5B918(struct NpCommonPs4StreamReader *reader, int context, struct NpCommonPs4StreamObserver *observer,
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

}
