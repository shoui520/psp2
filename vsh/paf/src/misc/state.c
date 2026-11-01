/* Miscellaneous record state transitions. */
#include <stdint.h>
#include <sce_atomic.h>

typedef struct PafReferenceBlock {
	uint32_t reserved;
	volatile uint32_t strongCount;
} PafReferenceBlock;

typedef struct PafReferencePair {
	void *object;
	PafReferenceBlock *block;
} PafReferencePair;

typedef struct PafReferenceSource {
	uint8_t reserved[72];
	void *object;
	PafReferenceBlock *block;
} PafReferenceSource;


volatile uint32_t *ScePafMisc_679B51BB(PafReferencePair *destination,
					const PafReferenceSource *source)
{
	PafReferenceBlock *block = source->block;

	destination->object = source->object;
	destination->block = block;
	if (!block)
		return 0;
	sceAtomicIncrement32AcqRel((volatile int32_t *)&block->strongCount);
	return &block->strongCount;
}
