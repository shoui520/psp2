/* Descriptive source name: the original library has no debug records. */
#include "key.h"

int MarlinRsaPrivate512(unsigned int *output, const unsigned int *input,
			    const MarlinRsaPrivateKey *key);
int MarlinRsaPrivate1024(unsigned int *output, const unsigned int *input,
			    const MarlinRsaPrivateKey *key);
int MarlinRsaPrivate2048(unsigned int *output, const unsigned int *input,
			    const MarlinRsaPrivateKey *key);
int MarlinRsaExponent512(unsigned int *output, const unsigned int *input,
			    const MarlinRsaExponentKey *key);
int MarlinRsaExponent1024(unsigned int *output, const unsigned int *input,
			    const MarlinRsaExponentKey *key);
int MarlinRsaExponent2048(unsigned int *output, const unsigned int *input,
			    const MarlinRsaExponentKey *key);

int MarlinRsaSignPrivate512(unsigned int *output, const unsigned int *input,
			        const MarlinRsaPrivateKey *key);
int MarlinRsaSignPrivate1024(unsigned int *output, const unsigned int *input,
			        const MarlinRsaPrivateKey *key);
int MarlinRsaSignPrivate2048(unsigned int *output, const unsigned int *input,
			        const MarlinRsaPrivateKey *key);
int MarlinRsaSignExponent512(unsigned int *output, const unsigned int *input,
			        const MarlinRsaExponentKey *key);
int MarlinRsaSignExponent1024(unsigned int *output, const unsigned int *input,
			        const MarlinRsaExponentKey *key);
int MarlinRsaSignExponent2048(unsigned int *output, const unsigned int *input,
			        const MarlinRsaExponentKey *key);

int MarlinRsaPrivateTransform(MarlinRsaContext *context,
			      const unsigned int *input, unsigned int *output)
{
	int result = 0;
	MarlinRsaExponentKey key;

	if (context->primeP != 0 && context->primeQ != 0 &&
	    context->exponentP != 0 && context->exponentQ != 0 &&
	    context->coefficient != 0) {
		switch (context->bits) {
		case 512:
			result = MarlinRsaPrivate512(output, input,
						     &context->privateKey);
			break;
		case 1024:
			result = MarlinRsaPrivate1024(output, input,
						      &context->privateKey);
			break;
		default:
			result = MarlinRsaPrivate2048(output, input,
						      &context->privateKey);
			break;
		}
	} else if (context->privateExponent != 0) {
		key.modulus = context->modulus;
		key.exponent = context->privateExponent;
		switch (context->bits) {
		case 512:
			result = MarlinRsaExponent512(output, input, &key);
			break;
		case 1024:
			result = MarlinRsaExponent1024(output, input, &key);
			break;
		default:
			result = MarlinRsaExponent2048(output, input, &key);
			break;
		}
	} else {
		result = -1;
	}
	return result == 0 ? 0 : 0x2206000a;
}

int MarlinRsaSignTransform(MarlinRsaContext *context,
			      const unsigned int *input, unsigned int *output)
{
	int result = 0;
	MarlinRsaExponentKey key;

	if (context->primeP != 0 && context->primeQ != 0 &&
	    context->exponentP != 0 && context->exponentQ != 0 &&
	    context->coefficient != 0) {
		switch (context->bits) {
		case 512:
			result = MarlinRsaSignPrivate512(output, input,
						     &context->privateKey);
			break;
		case 1024:
			result = MarlinRsaSignPrivate1024(output, input,
						      &context->privateKey);
			break;
		default:
			result = MarlinRsaSignPrivate2048(output, input,
						      &context->privateKey);
			break;
		}
	} else if (context->privateExponent != 0) {
		key.modulus = context->modulus;
		key.exponent = context->privateExponent;
		switch (context->bits) {
		case 512:
			result = MarlinRsaSignExponent512(output, input, &key);
			break;
		case 1024:
			result = MarlinRsaSignExponent1024(output, input, &key);
			break;
		default:
			result = MarlinRsaSignExponent2048(output, input, &key);
			break;
		}
	} else {
		result = -1;
	}
	return result == 0 ? 0 : 0x22060002;
}
