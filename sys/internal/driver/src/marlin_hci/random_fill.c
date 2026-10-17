/* Descriptive source name: the original library has no debug records. */
#include <sysmem/sysmem.h>
#include <sysmem/utils.h>

int MarlinRandomCreate(SceMt19937Context **context);
int MarlinRandomRead(SceMt19937Context *context, int size, void *data);
int MarlinRandomDestroy(SceMt19937Context *context);

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
