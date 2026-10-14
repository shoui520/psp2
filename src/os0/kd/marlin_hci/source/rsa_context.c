/* Descriptive source name: the original library has no debug records. */
#include "key.h"

int MarlinCreateIntegerWords(const MarlinBuffer *buffer, int count,
			    unsigned int **words);

int MarlinRsaCreate(MarlinRsaContext **context, const MarlinBuffer *modulus,
		    const MarlinBuffer *publicExponent,
		    const MarlinBuffer *privateExponent, const MarlinBuffer *primeP,
		    const MarlinBuffer *primeQ, const MarlinBuffer *exponentP,
		    const MarlinBuffer *exponentQ, const MarlinBuffer *coefficient)
{
	int result = 0;
	unsigned char *data;
	int size;
	MarlinRsaContext *memory = 0;
	int bits;

	*context = 0;
	data = modulus->data;
	size = modulus->size;
	while (size > 0 && *data == 0) {
		size--;
		data++;
	}
	bits = 8 * size;
	memory = MarlinAllocateMemory(0, sizeof(*memory));
	if (memory == 0) {
		result = 2;
		goto out;
	}
	MarlinFillMemory(memory, 0, sizeof(*memory));
	memory->bits = bits <= 512 ? 512 : bits <= 1024 ? 1024 : 2048;
	memory->primeSize = memory->bits / 16;
	memory->modulusSize = memory->bits / 8;
	memory->primeWords = memory->primeSize >> 2;
	memory->modulusWords = memory->modulusSize >> 2;
	if ((result = MarlinCreateIntegerWords(modulus, memory->modulusWords,
					       &memory->modulus)) == 0 &&
	    (result = MarlinCreateIntegerWords(publicExponent, memory->modulusWords,
					       &memory->publicExponent)) == 0 &&
	    (result = MarlinCreateIntegerWords(privateExponent, memory->modulusWords,
					       &memory->privateExponent)) == 0 &&
	    (result = MarlinCreateIntegerWords(primeP, memory->primeWords,
					       &memory->primeP)) == 0 &&
	    (result = MarlinCreateIntegerWords(primeQ, memory->primeWords,
					       &memory->primeQ)) == 0 &&
	    (result = MarlinCreateIntegerWords(exponentP, memory->primeWords,
					       &memory->exponentP)) == 0 &&
	    (result = MarlinCreateIntegerWords(exponentQ, memory->primeWords,
					       &memory->exponentQ)) == 0 &&
	    (result = MarlinCreateIntegerWords(coefficient, memory->primeWords,
					       &memory->coefficient)) == 0) {
		memory->publicKey.modulus = memory->modulus;
		memory->publicKey.exponent = memory->publicExponent;
		memory->privateKey.reserved = 0;
		memory->privateKey.modulus = memory->modulus;
		memory->privateKey.privateExponent = memory->privateExponent;
		memory->privateKey.primeP = memory->primeP;
		memory->privateKey.primeQ = memory->primeQ;
		memory->privateKey.exponentP = memory->exponentP;
		memory->privateKey.exponentQ = memory->exponentQ;
		memory->privateKey.coefficient = memory->coefficient;
		*context = memory;
	}
out:
	if (*context == 0)
		MarlinRsaDestroy(memory);
	return result;
}

int MarlinRsaDestroy(MarlinRsaContext *context)
{
	int result = 0;
	MarlinRsaContext *memory = context;

	if (memory != 0) {
		MarlinFreeMemory(0, memory->modulus);
		MarlinFreeMemory(0, memory->publicExponent);
		MarlinFreeMemory(0, memory->privateExponent);
		MarlinFreeMemory(0, memory->primeP);
		MarlinFreeMemory(0, memory->primeQ);
		MarlinFreeMemory(0, memory->exponentP);
		MarlinFreeMemory(0, memory->exponentQ);
		MarlinFreeMemory(0, memory->coefficient);
		MarlinFreeMemory(0, memory);
	}
	return result;
}
