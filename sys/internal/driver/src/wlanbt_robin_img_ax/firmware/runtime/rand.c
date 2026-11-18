/*
 * The C library's random numbers (0x521bc - 0x52222): the additive generator of 55 words, Knuth's
 * subtractive method run as a sum, as the ARM C library has it. The filename is inferred.
 */
#include <stdint.h>
#include "robin/runtime.h"

#define RAND_WORDS	55

/* The two places in the table the next number is taken from (0x55784). */
struct RandState {
	int first;
	int second;
};

struct RandState randState;	/* 0x55784: zero in the image until srand() sets it */
static unsigned int randTable[RAND_WORDS];		/* 0x5ad78 */

void srand(unsigned int seed)
{
	int i;

	randState.first = 23;
	randState.second = RAND_WORDS - 1;
	for (i = 0; i < RAND_WORDS; i++) {
		randTable[i] = (seed >> 16) + seed;
		seed = seed * 0x10dcd + 0x66d619e1;
	}
}

/* The library's start-up seed: srand(1), as the C standard says. */
void robinInitHeapPool(void)
{
	srand(1);
}

int rand(void)
{
	int second = randState.second;
	int first = randState.first;
	unsigned int value;

	value = randTable[second] + randTable[first];
	randTable[second] = value;
	randState.first = --first;
	if (first < 0) {
		randState.first = RAND_WORDS - 1;
		randState.second = second - 1;
	} else {
		randState.second = --second;
		if (second < 0)
			randState.second = RAND_WORDS - 1;
	}
	return (int)(value << 1 >> 1);
}
