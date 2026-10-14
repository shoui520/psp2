/* Camera frame buffers passed to SceCsi by SceCamera. */
#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct CsiTransferConfig {
	uint32_t iAddress[3];
	uint32_t iPitch;
	uint32_t chromaAddress[3][2];
	uint32_t chromaPitch;
} CsiTransferConfig;

_Static_assert(sizeof(CsiTransferConfig) == 44, "CSI transfer size");
_Static_assert(offsetof(CsiTransferConfig, iPitch) == 12 &&
	       offsetof(CsiTransferConfig, chromaAddress) == 16 &&
	       offsetof(CsiTransferConfig, chromaPitch) == 40, "CSI transfer layout");

uint32_t SceCsiForDriver_10545393(unsigned head, const CsiTransferConfig *data);
uint32_t SceCsiForDriver_B508822B(unsigned head, uint32_t width, uint32_t height);
uint32_t SceCsiForDriver_2AFEA1B0(unsigned head, uint32_t mode);
uint32_t SceCsiForDriver_FC165297(unsigned head, uint32_t value);
uint32_t SceCsiForDriver_A1D1805D(unsigned head);
uint32_t SceCsiForDriver_4AE39F26(unsigned head);
