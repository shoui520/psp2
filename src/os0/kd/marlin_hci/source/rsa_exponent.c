/* Descriptive source name: the original library has no debug records. */
#include "key.h"
#include "integer_arithmetic.h"

int MarlinRsaExponentOperation(unsigned int *output, const unsigned int *input,
			       const MarlinRsaExponentKey *key, int count);

int MarlinRsaExponent512(unsigned int *output, const unsigned int *input,
			 const MarlinRsaExponentKey *key)
{
	return MarlinRsaExponentOperation(output, input, key, 16);
}

int MarlinRsaExponent1024(unsigned int *output, const unsigned int *input,
			  const MarlinRsaExponentKey *key)
{
	return MarlinRsaExponentOperation(output, input, key, 32);
}

int MarlinRsaExponent2048(unsigned int *output, const unsigned int *input,
			  const MarlinRsaExponentKey *key)
{
	return MarlinRsaExponentOperation(output, input, key, 64);
}
