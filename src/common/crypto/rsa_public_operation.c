/* Descriptive source name: the original library has no debug records. */
#include <sce/rsa.h>
#include <sce/rsa_arithmetic.h>

int MarlinRsaExponentOperation(unsigned int *output, const unsigned int *input,
			       const MarlinRsaExponentKey *key, int count);

int MarlinRsaExponentOperation(unsigned int *output, const unsigned int *input,
			       const MarlinRsaExponentKey *key, int count)
{
	unsigned int temporary[128];
	unsigned int inverse;
	unsigned int mask;
	unsigned int bit;
	int index;
	int remaining;

	if (count <= 0 || count > 64)
		return -1;
	if ((key->modulus[count - 1] & 0x80000000) == 0)
		return -1;
	if ((key->modulus[0] & 1) == 0)
		return -1;
	if (!MarlinIntegerWordsGreater(key->modulus, input, count))
		return -1;
	if (count > 0) {
		index = 0;
		remaining = count;
		do {
			unsigned int word = *input++;

			remaining--;
			temporary[index] = 0;
			temporary[count + index] = word;
			index++;
		} while (remaining != 0);
	}
	inverse = 1;
	for (mask = 2; mask != 0; mask *= 2) {
		bit = (inverse * -key->modulus[0]) & mask;
		inverse |= bit;
	}
	MarlinRemainderIntegerWords(temporary, temporary, 2 * count,
				    key->modulus, count);
	MarlinMontgomeryPower(temporary, temporary, key->exponent,
			      key->modulus, count, inverse);
	MarlinMontgomeryReduce(output, temporary, count,
			       key->modulus, count, inverse);
	return 0;
}
