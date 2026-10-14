#pragma once

unsigned int MarlinAddIntegerWords(unsigned int *output,
	const unsigned int *first, const unsigned int *second, int count,
	unsigned int carry);
unsigned int MarlinSubtractIntegerWords(unsigned int *output,
	const unsigned int *first, const unsigned int *second, int count,
	unsigned int borrow);
int MarlinIntegerWordsGreater(const unsigned int *first,
			      const unsigned int *second, int count);
void MarlinMultiplyIntegerWords(unsigned int *output,
	const unsigned int *first, int firstCount,
	const unsigned int *second, int secondCount);
void MarlinMultiplyIntegerByWord(unsigned int *output,
	const unsigned int *input, int count, unsigned int factor);
void MarlinSquareIntegerWords(unsigned int *output,
	const unsigned int *input, int count);
void MarlinShiftLeftIntegerWords(unsigned int *output,
	const unsigned int *input, int count, unsigned int shift);
void MarlinShiftRightIntegerWords(unsigned int *output,
	const unsigned int *input, int count, unsigned int shift);
void MarlinRemainderIntegerWords(unsigned int *output,
	const unsigned int *input, int inputCount,
	const unsigned int *modulus, int modulusCount);
void MarlinMontgomeryPower(unsigned int *output, const unsigned int *input,
	const unsigned int *exponent, const unsigned int *modulus, int count,
	unsigned int inverse);
void MarlinMontgomeryReduce(unsigned int *output, const unsigned int *input,
	int inputCount, const unsigned int *modulus, int count,
	unsigned int inverse);
void MarlinMontgomeryMultiply(unsigned int *output, const unsigned int *first,
	const unsigned int *second, const unsigned int *modulus, int count,
	unsigned int inverse);

