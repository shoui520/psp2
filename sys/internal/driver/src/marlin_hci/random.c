/* Descriptive source name: the original library has no debug records. */
#include "buffer.h"
#include <rtc/rtc.h>
#include <sysmem/sysmem.h>
#include <sysmem/utils.h>

int MarlinRandomCreate(SceMt19937Context **context)
{
	int result = 0;
	SceMt19937Context *memory = 0;
	unsigned int seed;
	SceRtcTick tick;

	*context = 0;
	if (context == 0) {
		result = 3;
		goto out;
	}
	memory = MarlinAllocateMemory(0, sizeof(*memory));
	if (memory == 0) {
		result = 2;
		goto out;
	}
	sceRtcGetCurrentTick(&tick);
	seed = (unsigned int)tick.tick;
	sceMt19937Init(memory, seed);
	*context = memory;
out:
	if (*context == 0)
		MarlinFreeMemory(0, memory);
	return result;
}

int MarlinRandomNoOp(void *context)
{
	int result = 0;

	return result;
}

int MarlinRandomNoOpContext(void *context)
{
	int result = 0;

	return result;
}

int MarlinRandomRead(SceMt19937Context *context, int size, void *data)
{
	int result = 0;
	unsigned char *cursor;
	SceMt19937Context *random = context;
	unsigned int value;

	cursor = data;
	if (context == 0 || data == 0) {
		result = 3;
	} else {
		while (size != 0) {
			value = sceMt19937UInt(random);
			cursor[0] = value;
			size--;
			if (size == 0)
				break;
			cursor[1] = value >> 8;
			size--;
			if (size == 0)
				break;
			cursor[2] = value >> 16;
			size--;
			if (size == 0)
				break;
			cursor[3] = value >> 24;
			cursor += 4;
			size--;
		}
	}
	return result;
}

int MarlinRandomDestroy(SceMt19937Context *context)
{
	int result = 0;
	SceMt19937Context *memory = context;

	MarlinFreeMemory(0, memory);
	return result;
}
