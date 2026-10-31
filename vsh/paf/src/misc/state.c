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

typedef struct PafRecordState {
	uint32_t kind;
	float sourceValue;
	uint32_t unused1;
	float computedValue;
	uint32_t flags;
	uint32_t unused2;
	uint32_t weight;
} PafRecordState;

void ScePafMisc_7CF66185(PafRecordState *state)
{
	uint32_t flags = state->flags;

	if ((flags & 0xf000U) == 0x3000U)
		return;
	state->flags = (flags & ~0xf000U) | 0x2000U;
	if ((flags & 0xf00U) == 0xc00U) {
		state->computedValue = 0.0f;
		state->weight = 0;
	} else {
		state->computedValue = state->sourceValue;
		state->weight = 0x3f800000U;
	}
}
