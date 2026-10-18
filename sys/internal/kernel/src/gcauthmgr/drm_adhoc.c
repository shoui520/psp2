/* Authentication message assembly. */
#include <threadmgr/threadmgr.h>
#include <sysmem/utils.h>
#include "adhoc.h"

GcAuthAdhocWorkspace gcAuthAdhocWorkspace;
SceUID gcAuthAdhocSemaphore;
GcAuthAdhocSigningScratch gcAuthAdhocSigningScratch;
int GcAuthExecuteKirkCommand(void *output, const void *input, int command,
		size_t size, int keyId);
int GcAuthGenerateRandom20(unsigned char *data);

void GcAuthAdhocBuildMessage(unsigned char *output,
		const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize)
{
	unsigned int index;
	unsigned int remaining;
	unsigned char *cursor;
	unsigned char *end;

	for (index = 0; index != firstSize; ++index)
		output[index] = first[index];
	output += firstSize;
	for (index = 0; index != secondSize; ++index)
		output[index] = second[index];
	output += secondSize;
	for (index = 0; index != thirdSize; ++index)
		output[index] = third[index];
	output += thirdSize;
	for (index = 0; index != fourthSize; ++index)
		output[index] = fourth[index];
	cursor = output + fourthSize;
	remaining = 192 - fourthSize - thirdSize - secondSize - firstSize;
	if (remaining != 0) {
		end = cursor + remaining;
		do {
			*cursor++ = 0;
		} while (cursor != end);
	}
}

void GcAuthAdhocBuildExtendedMessage(unsigned char *output,
		const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize)
{
	unsigned int index;
	unsigned int remaining;
	unsigned char *cursor;
	unsigned char *end;

	for (index = 0; index != firstSize; ++index)
		output[index] = first[index];
	output += firstSize;
	for (index = 0; index != secondSize; ++index)
		output[index] = second[index];
	output += secondSize;
	for (index = 0; index != thirdSize; ++index)
		output[index] = third[index];
	output += thirdSize;
	for (index = 0; index != fourthSize; ++index)
		output[index] = fourth[index];
	cursor = output + fourthSize;
	remaining = 256 - fourthSize - thirdSize - secondSize - firstSize;
	if (remaining != 0) {
		end = cursor + remaining;
		do {
			*cursor++ = 0;
		} while (cursor != end);
	}
}

