/* Descriptive source name: the original library has no debug records. */
#include "key.h"
#include "integer_arithmetic.h"

int MarlinRsaPrivate512(unsigned int *output, const unsigned int *input,
			const MarlinRsaPrivateKey *key)
{
	return MarlinRsaPrivateOperation(output, input, key, 16);
}

int MarlinRsaPrivate1024(unsigned int *output, const unsigned int *input,
			 const MarlinRsaPrivateKey *key)
{
	return MarlinRsaPrivateOperation(output, input, key, 32);
}

int MarlinRsaPrivate2048(unsigned int *output, const unsigned int *input,
			 const MarlinRsaPrivateKey *key)
{
	return MarlinRsaPrivateOperation(output, input, key, 64);
}
