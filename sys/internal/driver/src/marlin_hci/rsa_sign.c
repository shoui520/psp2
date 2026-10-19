/* Descriptive source name: the original library has no debug records. */
#include "key.h"

int MarlinRsaSignPrivate512(unsigned int *output, const unsigned int *input,
			    const MarlinRsaPrivateKey *key)
{
	return MarlinRsaPrivateOperation(output, input, key, 16);
}

int MarlinRsaSignPrivate1024(unsigned int *output, const unsigned int *input,
			     const MarlinRsaPrivateKey *key)
{
	return MarlinRsaPrivateOperation(output, input, key, 32);
}

int MarlinRsaSignPrivate2048(unsigned int *output, const unsigned int *input,
			     const MarlinRsaPrivateKey *key)
{
	return MarlinRsaPrivateOperation(output, input, key, 64);
}

int MarlinRsaSignExponent512(unsigned int *output, const unsigned int *input,
			     const MarlinRsaExponentKey *key)
{
	return MarlinRsaExponentOperation(output, input, key, 16);
}

int MarlinRsaSignExponent1024(unsigned int *output, const unsigned int *input,
			     const MarlinRsaExponentKey *key)
{
	return MarlinRsaExponentOperation(output, input, key, 32);
}

int MarlinRsaSignExponent2048(unsigned int *output, const unsigned int *input,
			     const MarlinRsaExponentKey *key)
{
	return MarlinRsaExponentOperation(output, input, key, 64);
}
