/* Descriptive source name: the original library has no debug records. */
#include <sysmem/sysmem.h>
#include <sysmem/utils.h>
#include "cipher.h"

unsigned char marlinRandomBytes[256];
unsigned char *marlinRandomCursor;

int MarlinFillRandomBytes(unsigned char *data, int size)
{
	unsigned char *end;
	SceMt19937Context *context;

	MarlinRandomCreate(&context);
	MarlinRandomRead(context, 256, marlinRandomBytes);
	MarlinRandomDestroy(context);
	marlinRandomCursor = marlinRandomBytes;
	end = data + size;
	while (data < end) {
		*data = *marlinRandomCursor;
		data++;
		marlinRandomCursor++;
	}
	return 0;
}
