/* Shared software DES key utilities. */
#pragma once

#include <stdint.h>

typedef struct DesContext {
	uint32_t roundKeys[16][2];
} DesContext;

_Static_assert(sizeof(DesContext) == 128, "DES context size");

int SceKernelUtilsForDriver_AE6C45FB(DesContext *context, uint64_t key);
uint64_t SceKernelUtilsForDriver_A0D45D25(const DesContext *context,
				       uint64_t block);
uint64_t SceKernelUtilsForDriver_801D0ED4(const DesContext *context,
				       uint64_t block);

uint64_t DesAdjustKeyParity(uint64_t key);
int DesCheckKeyParity(uint64_t key);
int DesIsWeakKey(uint64_t key);
