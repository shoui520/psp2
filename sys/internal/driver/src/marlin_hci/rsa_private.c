/* Descriptive source name: the original library has no debug records. */
#include <marlin_hci/rsa.h>
#include <marlin_hci/rsa_arithmetic.h>

int MarlinRsaPrivateOperation(unsigned int *output, const unsigned int *input,
			      const MarlinRsaPrivateKey *key, int count)
{
	unsigned int residueP[32];
	unsigned int residueQ[32];
	unsigned int product[64];
	int half;
	unsigned int *temporary;
	unsigned int inverseP;
	unsigned int inverseQ;
	unsigned int maskP;
	unsigned int maskQ;
	unsigned int bit;
	unsigned int carry;
	int index;

	if (count <= 0 || count > 64)
		return -1;
	half = count / 2;
	temporary = product + half;
	MarlinMultiplyIntegerWords(product, key->primeP, half, key->primeQ, half);
	if ((product[count - 1] & key->primeP[half - 1] &
	     key->primeQ[half - 1] & 0x80000000) == 0)
		return -1;
	if ((product[0] & key->primeP[0] & key->primeQ[0] & 1) == 0)
		return -1;
	if (!MarlinIntegerWordsGreater(product, input, count))
		return -1;
	for (index = 0; index < half; index++)
		product[index] = 0;
	inverseQ = 1;
	maskQ = 2;
	inverseP = 1;
	maskP = 2;
	do {
		bit = (inverseQ * -key->primeQ[0]) & maskQ;
		maskQ *= 2;
		inverseQ |= bit;
	} while (maskQ != 0);
	MarlinRemainderIntegerWords(temporary, input, count, key->primeQ, half);
	MarlinRemainderIntegerWords(temporary, product, count, key->primeQ, half);
	MarlinMontgomeryPower(residueQ, temporary, key->exponentQ,
			      key->primeQ, half, inverseQ);
	MarlinMontgomeryReduce(residueQ, residueQ, half, key->primeQ,
			       half, inverseQ);
	do {
		bit = (inverseP * -key->primeP[0]) & maskP;
		maskP *= 2;
		inverseP |= bit;
	} while (maskP != 0);
	MarlinRemainderIntegerWords(temporary, input, count, key->primeP, half);
	MarlinRemainderIntegerWords(temporary, product, count, key->primeP, half);
	MarlinMontgomeryPower(residueP, temporary, key->exponentP,
			      key->primeP, half, inverseP);
	for (index = 0; index < half; index++)
		temporary[index] = residueQ[index];
	MarlinRemainderIntegerWords(temporary, product, count, key->primeP, half);
	if (MarlinSubtractIntegerWords(residueP, residueP, temporary, half, 0))
		MarlinAddIntegerWords(residueP, residueP, key->primeP, half, 0);
	MarlinMontgomeryMultiply(residueP, residueP, key->coefficient,
				 key->primeP, half, inverseP);
	MarlinMultiplyIntegerWords(product, residueP, half, key->primeQ, half);
	carry = MarlinAddIntegerWords(output, residueQ, product, half, 0);
	for (index = half; index < count; index++) {
		unsigned int word = product[index];
		unsigned int sum = carry + word;

		output[index] = sum;
		carry = (word & ~sum) >> 31;
	}
	return 0;
}
