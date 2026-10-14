/* DMAC5 operation state shared by the manager and its keyring helpers. */
#pragma once
#include <stdint.h>
#include <psp2kern/kernel/dmac.h>

/* Name unknown: the first 40 bytes hold an IV or incremental hash state. */
typedef struct Dmac5Context {
	uint8_t state[40];
	uint32_t command;
	uint32_t keyRingIndex;
	uint32_t dataCoherency;
	uint32_t ivCoherency;
	SceKernelDmaOpId operation;
	uint32_t reserved;
} Dmac5Context;

_Static_assert(sizeof(Dmac5Context) == 64, "DMAC5 context ABI");

int dmac5ValidateReservedKey(unsigned int key, uint32_t *index);
int dmac5SetKeySize(uint32_t *command, unsigned int bits);
