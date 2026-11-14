/* Repeat a 32-bit fill pattern without imposing word alignment. */
#include <stddef.h>
#include <stdint.h>

/* setjmp_abi.S: the pattern stored over bytes from destination on */
void *sce_paf_memfill32(void *destination, uint32_t pattern, size_t bytes);

void *sce_paf_memset32(void *destination, uint32_t pattern, size_t words)
{
	sce_paf_memfill32(destination, pattern, words * 4);
	return destination;
}
