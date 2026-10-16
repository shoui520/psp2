/* Descriptive source name: the original library has no debug records. */
#include "buffer.h"

/* Shift-JIS: 美容と健康に食後に一杯の紅茶を。 */
static const unsigned char marlinDefaultRecordSecret[32] =
	"\x94\xfc\x97\x65\x82\xc6\x8c\x92\x8d\x4e\x82\xc9\x90\x48\x8c\xe3"
	"\x82\xc9\x88\xea\x94\x74\x82\xcc\x8d\x67\x92\x83\x82\xf0\x81\x42";

/* Zero-initialized storage preceding the original size and presence words. */
static unsigned char marlinRecordSecret[36];
static unsigned int marlinRecordSecretSize;
static int marlinRecordSecretPresent;

MarlinBuffer *MarlinGetRecordSecret(void *allocator)
{
	MarlinBuffer *buffer = 0;

	if (marlinRecordSecretPresent != 0)
		buffer = MarlinBufferCreate(allocator, marlinRecordSecretSize,
					    marlinRecordSecret);
	else
		buffer = MarlinBufferCreate(allocator,
					    sizeof(marlinDefaultRecordSecret),
					    marlinDefaultRecordSecret);
	return buffer;
}

int MarlinRecordSecretNoOp(unsigned int first, unsigned int second,
			   unsigned int third)
{
	return 0;
}
