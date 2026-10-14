/* Shared Rijndael word transformations (original internal names unknown). */
#pragma once

#include <stdint.h>

uint32_t AesSubWord(uint32_t word);
uint32_t AesInverseMixColumn(uint32_t column);
