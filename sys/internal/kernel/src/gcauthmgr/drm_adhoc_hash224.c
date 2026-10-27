/* Two-span SHA-224 message serialization. */
#include <sysmem/utils.h>
#include "adhoc.h"

GcAuthAdhocHashScratch gcAuthAdhocHashScratch;

int GcAuthAdhocHash224Spans(const unsigned char *first,
		unsigned int firstSize, const unsigned char *second,
		unsigned int secondSize)
{
	GcAuthAdhocHash224Input *input = &gcAuthAdhocHashScratch.input;
	unsigned int size = firstSize + secondSize;
	unsigned int index;

	input->size = size;
	for (index = 0; index != firstSize; ++index)
		input->payload[index] = first[index];
	for (index = 0; index != secondSize; ++index)
		input->payload[firstSize + index] = second[index];
	if (sceSha224Digest(input->payload, size,
			gcAuthAdhocHashScratch.digest224) != 0)
		return (int)0x808a0103;
	return 0;
}
