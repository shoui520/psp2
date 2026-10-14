#pragma once

#include <sce/aes.h>

struct GcAuthAesCbcState {
	SceAesFullContext aes;
	unsigned char chain[32];
};

_Static_assert(sizeof(struct GcAuthAesCbcState) == 1008, "AES CBC state ABI");
