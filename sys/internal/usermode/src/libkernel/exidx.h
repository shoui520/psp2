#ifndef SCE_EXIDX_H
#define SCE_EXIDX_H

#include <stdint.h>

/* ARM EHABI: an index entry that cannot be unwound, and the compact models
 * (__aeabi_unwind_cpp_pr0: three opcodes inline, pr1: more words follow) */
#define EXIDX_CANTUNWIND		1
#define EHABI_PERSONALITY_MASK		0xff000000
#define EHABI_COMPACT_PR0		0x80000000
#define EHABI_COMPACT_PR1		0x81000000

struct ExidxEntry {
	uint32_t functionOffset;
	uint32_t unwind;
};

struct ExidxResult {
	uint32_t descriptor;
	const uint32_t *instructions;
};

static inline __attribute__((always_inline))
uint32_t ExidxPrel31(uint32_t value)
{
	if (value & 0x40000000)
		value |= 0x80000000;
	return value;
}

/* virtualBase may differ from entries when a kernel-owned copy is searched. */
static inline __attribute__((always_inline))
const struct ExidxEntry *ExidxFindEntry(const struct ExidxEntry *entries,
		uintptr_t virtualBase, uint32_t count, uintptr_t address)
{
	const struct ExidxEntry *entry;
	uint32_t function, nextFunction;
	int low = 0, high, middle, next;

	if (!count)
		return 0;
	high = count - 1;
	middle = high >> 1;
	address &= ~1u;
	entry = &entries[high];
	function = ExidxPrel31(entry->functionOffset);
	if (address >= ((virtualBase + high * 8 + function) & ~1u))
		return entry;
	if (middle >= high)
		return 0;
	for (;;) {
		entry = &entries[middle];
		function = ExidxPrel31(entry[0].functionOffset);
		nextFunction = ExidxPrel31(entry[1].functionOffset);
		if (address >= ((virtualBase + middle * 8 + function) & ~1u)) {
			if (address < ((virtualBase + (middle + 1) * 8 +
					 nextFunction) & ~1u))
				return entry;
			low = middle;
		} else {
			high = middle;
		}
		next = (low + high) / 2;
		if (next == middle || next >= high)
			return 0;
		middle = next;
	}
}

#endif
