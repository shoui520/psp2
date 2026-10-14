/* Descriptive file name: the original has no source-file debug records. */
#include "magicgate_private.h"

#define TREE_BIT(tree, bit) (((tree)[(bit) >> 3] >> (~(bit) & 7)) & 1)
#define COPY_RECORD_BYTE(dst, src, index) \
	((dst)->bytes[index] = (src)->bytes[index])
#define SWAP_RECORD_BYTE(first, last, index) do { \
	byte = (first)->bytes[index]; \
	(first)->bytes[index] = (last)->bytes[index]; \
	(last)->bytes[index] = byte; \
} while (0)

/* Nodes are packed in groups of eight, with three bits per node. */
int selectRecordBlocks(MagicGateBlock *output, int *count, int *depth,
		       const uint8_t *path, const MagicGateRecordHeader *record,
		       unsigned int size)
{
	const uint8_t *tree = (const uint8_t *)(record + 1);
	const MagicGateBlock *blocks = (const MagicGateBlock *)
		(tree + ((unsigned int)record->treeSize[0] << 24 |
			 (unsigned int)record->treeSize[1] << 16 |
			 (unsigned int)record->treeSize[2] << 8 |
			 record->treeSize[3]));
	const MagicGateBlock *source;
	int capacity;
	int block = -1;
	int node = 0;
	int skip = 0;
	int nextSkip;
	int pending;
	int bit;
	int left;
	int right;
	int first;
	int last;
	uint8_t byte;

	if ((uintptr_t)blocks > (uintptr_t)record + size)
		return (int)0x80501002;
	capacity = *count;
	*count = 0;
	*depth = 0;
	for (;;) {
		bit = node * 3;
		left = TREE_BIT(tree, bit + 1);
		right = TREE_BIT(tree, bit + 2);
		if (TREE_BIT(path, *depth)) {
			if (right) {
				if (!left || !TREE_BIT(tree, bit))
					return (int)0x80503001;
				break;
			}
			if (!left)
				++skip;
			nextSkip = 0;
		} else {
			if (left) {
				if (!right || !TREE_BIT(tree, bit))
					return (int)0x80503001;
				break;
			}
			nextSkip = !right;
		}
		if (TREE_BIT(tree, bit))
			++block;
		pending = skip;
		skip = nextSkip;
		while (pending) {
			if (++node == 8) {
				node = 0;
				tree += 3;
			}
			bit = node * 3;
			if (TREE_BIT(tree, bit))
				++block;
			if (!TREE_BIT(tree, bit + 2))
				++skip;
			if (!TREE_BIT(tree, bit + 1))
				++skip;
			--pending;
		}
		if (++node == 8) {
			node = 0;
			tree += 3;
		}
		if (TREE_BIT(tree, node * 3)) {
			if (capacity < *count)
				return (int)0x80501002;
			source = &blocks[block];
			COPY_RECORD_BYTE(&output[*count], source, 0);
			COPY_RECORD_BYTE(&output[*count], source, 1);
			COPY_RECORD_BYTE(&output[*count], source, 2);
			COPY_RECORD_BYTE(&output[*count], source, 3);
			COPY_RECORD_BYTE(&output[*count], source, 4);
			COPY_RECORD_BYTE(&output[*count], source, 5);
			COPY_RECORD_BYTE(&output[*count], source, 6);
			COPY_RECORD_BYTE(&output[*count], source, 7);
			COPY_RECORD_BYTE(&output[*count], source, 8);
			COPY_RECORD_BYTE(&output[*count], source, 9);
			COPY_RECORD_BYTE(&output[*count], source, 10);
			COPY_RECORD_BYTE(&output[*count], source, 11);
			COPY_RECORD_BYTE(&output[*count], source, 12);
			COPY_RECORD_BYTE(&output[*count], source, 13);
			COPY_RECORD_BYTE(&output[*count], source, 14);
			COPY_RECORD_BYTE(&output[*count], source, 15);
			++*count;
		}
		if (++*depth > 63)
			break;
	}
	first = 0;
	last = *count - 1;
	while (first < last) {
		SWAP_RECORD_BYTE(&output[first], &output[last], 0);
		SWAP_RECORD_BYTE(&output[first], &output[last], 1);
		SWAP_RECORD_BYTE(&output[first], &output[last], 2);
		SWAP_RECORD_BYTE(&output[first], &output[last], 3);
		SWAP_RECORD_BYTE(&output[first], &output[last], 4);
		SWAP_RECORD_BYTE(&output[first], &output[last], 5);
		SWAP_RECORD_BYTE(&output[first], &output[last], 6);
		SWAP_RECORD_BYTE(&output[first], &output[last], 7);
		SWAP_RECORD_BYTE(&output[first], &output[last], 8);
		SWAP_RECORD_BYTE(&output[first], &output[last], 9);
		SWAP_RECORD_BYTE(&output[first], &output[last], 10);
		SWAP_RECORD_BYTE(&output[first], &output[last], 11);
		SWAP_RECORD_BYTE(&output[first], &output[last], 12);
		SWAP_RECORD_BYTE(&output[first], &output[last], 13);
		SWAP_RECORD_BYTE(&output[first], &output[last], 14);
		SWAP_RECORD_BYTE(&output[first], &output[last], 15);
		++first;
		--last;
	}
	return 0;
}
