/* Bit and byte helpers (sys/util.c). */
#ifndef ROBIN_UTIL_H
#define ROBIN_UTIL_H

#include <stdint.h>

unsigned int util_CountBitsSet(uint32_t value);	/* 0x2838c */
uint32_t util_Swap32(uint32_t value);		/* 0x283b0 */
/* Reverses the bytes of every group of up to four in the buffer. */
void util_SwapBuffer(uint8_t *data, unsigned int length);	/* 0x283cc */

#endif /* ROBIN_UTIL_H */
