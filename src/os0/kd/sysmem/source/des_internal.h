/* Internal DES permutations; the original has no source-name debug records. */
#ifndef SYSMEM_DES_INTERNAL_H
#define SYSMEM_DES_INTERNAL_H

#include <stdint.h>
#include "fw.h"

extern const uint16_t desKeySelection[24];
extern const uint32_t desKeyRotationSequence;

uint64_t DesInitialPermutation(uint64_t block);
uint64_t DesFinalPermutation(uint64_t block);

#if FW_SINCE(0x04000000)
uint32_t DesRound(uint32_t first, uint32_t second);
#else
uint32_t DesRound(uint32_t block, const uint32_t roundKey[2]);
#endif

#endif
