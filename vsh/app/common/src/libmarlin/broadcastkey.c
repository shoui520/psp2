/*
 * A broadcast key block's keys (0x8107D17C-0x8107E30C): the revocation
 * information's table holds, for each digit of a node id, runs of the digits
 * whose subsets it keeps; the node id the HCI gives picks the subset that covers
 * it, and the HCI derives its key from the block's key at that subset's index.
 * The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_BKB_UNUSED	15	/* an entry's group, and a run's last digit, that ends */

/* a big-endian word of the block */
#define MLN_BKB_WORD(p)	((p)[0] << 24 | (p)[1] << 16 | (p)[2] << 8 | (p)[3])

/* the subsets of a run of digits of one group counted, the last one kept */
void MlnImpl_8107D17C(const unsigned char *run, unsigned int length, MlnBkbSubset *subset)
{
	signed char previous;
	unsigned int i;
	int count;
	int wrap;
	int digit;

	previous = run[0] & 0xF;
	if (run[0] == 0) {
		count = 0;
		wrap = 0;
	} else if (run[length - 1] == MLN_BKB_UNUSED) {
		wrap = 0;
		count = 1;
		subset->start = 0;
		subset->end = (run[0] & 0xF) - 1;
	} else {
		count = 0;
		wrap = 1;
	}
	for (i = 0; i < length; i++) {
		digit = run[i] & 0xF;
		if (previous + 1 < digit) {
			subset->start = previous + 1;
			count++;
			subset->end = (run[i] & 0xF) - 1;
			digit = run[i] & 0xF;
		}
		previous = digit;
	}
	if (run[length - 1] != MLN_BKB_UNUSED) {
		subset->start = (run[length - 1] & 0xF) + 1;
		count++;
		if (wrap)
			subset->end = (run[0] & 0xF) - 1;
		else
			subset->end = 15;
	}
	subset->index += count;
}

/*
 * The subsets of the run that holds the node's digit counted, up to the one
 * that covers the digit when the run's subsets do not wrap around.
 */
static inline void MlnBkbRunIndex(const unsigned char *run, unsigned int length, int nodeDigit,
				  MlnBkbSubset *subset)
{
	signed char previous;
	unsigned int i;
	int count;
	int wrap;
	int digit;

	previous = run[0] & 0xF;
	if (run[0] == 0) {
		count = 0;
		wrap = 0;
	} else if (run[length - 1] == MLN_BKB_UNUSED) {
		wrap = 0;
		count = 1;
		subset->start = 0;
		subset->end = (run[0] & 0xF) - 1;
	} else {
		count = 0;
		wrap = 1;
	}
	for (i = 0; i < length; i++) {
		digit = run[i] & 0xF;
		if (previous + 1 < digit) {
			subset->start = previous + 1;
			count++;
			subset->end = (run[i] & 0xF) - 1;
			digit = run[i] & 0xF;
		}
		previous = digit;
		if (!wrap && nodeDigit < digit)
			goto found;
	}
	if (run[length - 1] != MLN_BKB_UNUSED) {
		subset->start = (run[length - 1] & 0xF) + 1;
		count++;
		if (wrap)
			subset->end = (run[0] & 0xF) - 1;
		else
			subset->end = 15;
	}
found:
	subset->index += count;
}

/* a hexadecimal digit's value, -1 when it is none */
static inline int MlnBkbHexDigit(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

/* the node's encrypted key: its XML's CipherValue, base64-decoded */
static inline int MlnBkbCipherValue(const MlnBuffer *keyData, MlnBuffer **out)
{
	MlnBuffer *encoded = NULL;
	MlnBuffer *decoded = NULL;
	char *text = NULL;
	char *value;
	char *end;
	int ret;

	if (keyData == NULL || keyData->data == NULL) {
		ret = MLN_BKB_ERROR_FORMAT;
		goto out;
	}
	text = MlnAllocate(MLN_HEAP_GENERAL, keyData->size + 1);
	if (text == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemcpy(text, keyData->data, keyData->size);
	text[keyData->size] = '\0';
	value = MlnStrstr(text, "CipherValue>");
	if (value == NULL || &text[keyData->size] - value <= 11) {
		ret = MLN_BKB_ERROR_FORMAT;
		goto out;
	}
	value += 12;
	end = MlnStrstr(value, "</");
	if (end == NULL) {
		ret = MLN_BKB_ERROR_FORMAT;
		goto out;
	}
	encoded = MlnBufferCreate(MLN_HEAP_GENERAL, end - value, value);
	if (encoded == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, encoded, &decoded);
	if (ret != 0)
		goto out;
	*out = decoded;
	decoded = NULL;
out:
	MlnFree(MLN_HEAP_GENERAL, text);
	MlnBufferDestroy(MLN_HEAP_GENERAL, encoded);
	MlnBufferDestroy(MLN_HEAP_GENERAL, decoded);
	return ret;
}

/*
 * The node's key of a broadcast key block. Row by row, each digit of the HCI's
 * node id keeps the columns whose entry is that digit; at the first row where
 * none does, the run holding the column left gives the subset, and the subsets
 * counted before it the index of its key. The HCI derives the content key from
 * that key and the node's encrypted key.
 */
int MlnImpl_8107D270(MlnBkbRevocation *revocation, const MlnBuffer *keyData, void **out)
{
	void *loaded = NULL;
	MlnBuffer *nodeId = NULL;
	MlnBuffer *cipher = NULL;
	unsigned char digits[MLN_BKB_DIGITS];
	unsigned char run[MLN_BKB_DIGITS];
	MlnBuffer *key = NULL;
	MlnBuffer *id = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *iv = NULL;
	MlnBkbSubset *subset;
	const unsigned char *entries;
	unsigned char *alive;
	unsigned char c;
	unsigned int columns;
	unsigned int row;
	unsigned int length;
	unsigned int i;
	int digit;
	int any;
	int ret;

	subset = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*subset));
	if (subset == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_810812EC(&nodeId);
	if (ret != 0) {
		if (ret != 2)
			ret = MLN_BKB_ERROR_FAILED;
		goto out;
	}
	if (nodeId->size != MLN_BKB_DIGITS) {
		ret = MLN_BKB_ERROR_NODE_ID;
		goto out;
	}
	for (i = 0; i < MLN_BKB_DIGITS; i++) {
		digit = MlnBkbHexDigit(nodeId->data[i]);
		if (digit < 0) {
			ret = MLN_BKB_ERROR_NODE_ID;
			goto out;
		}
		digits[i] = digit;
	}
	ret = MlnBkbCipherValue(keyData, &cipher);
	if (ret != 0)
		goto out;

	alive = MlnAllocate(MLN_HEAP_GENERAL, revocation->columns);
	if (alive == NULL) {
		ret = 2;
		goto fail;
	}
	for (c = 0; c < revocation->columns; c++)
		alive[c] = 1;
	subset->index = 0;
	for (row = 0; row < MLN_BKB_DIGITS; row++) {
		columns = revocation->columns;
		digit = digits[row];
		if (columns == 0)
			goto revoked;
		entries = revocation->table + columns * row;
		for (i = 0; i < columns; i++) {
			if (!alive[i])
				continue;
			if (entries[i] >> 4 == MLN_BKB_UNUSED)
				alive[i] = 0;
			else if (digit == (entries[i] & 0xF))
				break;
		}
		if (i == columns) {
			/* the node's column is left out here: its subset is in this row */
			columns = revocation->columns;
			any = 0;
			for (i = 0; i < columns; i++)
				if (alive[i])
					any = 1;
			if (!any)
				goto revoked;
			entries = revocation->table + columns * row;
			length = 0;
			for (i = 0; i < columns; i++) {
				if (entries[i] >> 4 == MLN_BKB_UNUSED)
					continue;
				run[length++] = entries[i];
				if (i + 1 < columns && entries[i] >> 4 == entries[i + 1] >> 4)
					continue;
				if (alive[i] == 1) {
					MlnBkbRunIndex(run, length, digit, subset);
					break;
				}
				MlnImpl_8107D17C(run, length, subset);
				length = 0;
			}
			goto found;
		}
		for (i = 0; i != columns; i++)
			if (alive[i] && digit != (entries[i] & 0xF))
				alive[i] = 0;
		columns = revocation->columns;
		any = 0;
		for (i = 0; i < columns; i++)
			if (alive[i])
				any = 1;
		if (!any)
			goto revoked;
		entries = revocation->table + columns * row;
		length = 0;
		for (i = 0; i < columns; i++) {
			if (entries[i] >> 4 == MLN_BKB_UNUSED)
				continue;
			run[length++] = entries[i];
			if (i + 1 < columns && entries[i] >> 4 == entries[i + 1] >> 4)
				continue;
			MlnImpl_8107D17C(run, length, subset);
			length = 0;
		}
	}
revoked:
	ret = MLN_BKB_ERROR_REVOKED;
	goto fail;

found:
	if (subset->index == 0) {
		ret = MLN_BKB_ERROR_FORMAT;
		goto fail;
	}
	subset->row = row;
	subset->index--;
	MlnFree(MLN_HEAP_GENERAL, alive);
	key = MlnBufferCreate(MLN_HEAP_GENERAL, MLN_BKB_KEY_SIZE,
			      revocation->keys + MLN_BKB_KEY_SIZE * subset->index);
	if (key == NULL) {
		ret = 2;
		goto out;
	}
	id = MlnBufferCreate(MLN_HEAP_GENERAL, 16, revocation->id);
	if (id == NULL) {
		ret = 2;
		goto out;
	}
	data = MlnBufferCreate(MLN_HEAP_GENERAL, cipher->size - 16, cipher->data + 16);
	if (data == NULL) {
		ret = 2;
		goto out;
	}
	iv = MlnBufferCreate(MLN_HEAP_GENERAL, 16, cipher->data);
	if (iv == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81080EF8((subset->end + 16 - digits[subset->row]) % 16,
			       subset->start + 16 * subset->row, key, id, iv, data, &loaded);
	if (ret == 0) {
		*out = loaded;
		loaded = NULL;
	} else if (ret != 2) {
		ret = MLN_BKB_ERROR_FAILED;
	}
	goto out;

fail:
	MlnFree(MLN_HEAP_GENERAL, alive);
out:
	MlnImpl_81080C80(loaded);
	MlnFree(MLN_HEAP_GENERAL, subset);
	MlnBufferDestroy(MLN_HEAP_GENERAL, cipher);
	MlnBufferDestroy(MLN_HEAP_GENERAL, key);
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnBufferDestroy(MLN_HEAP_GENERAL, iv);
	MlnBufferDestroy(MLN_HEAP_GENERAL, nodeId);
	return ret;
}

/* the revocation information freed */
static inline void MlnBkbRevocationFree(MlnBkbRevocation *revocation)
{
	if (revocation == NULL)
		return;
	MlnFree(MLN_HEAP_GENERAL, revocation->id);
	MlnFree(MLN_HEAP_GENERAL, revocation->table);
	MlnFree(MLN_HEAP_GENERAL, revocation->keys);
	MlnFree(MLN_HEAP_GENERAL, revocation);
}

/* a block's revocation information read, its keys copied, and the node's key */
unsigned int MlnImpl_8107DF18(MlnOctopusBroadcastKeyBlock *block, MlnBuffer *key, void **out)
{
	MlnBkbRevocation *revocation;
	const unsigned char *p;
	unsigned int size;
	unsigned int tableSize;
	int ret;

	if (key == NULL || block == NULL || block->revocationInformation == NULL ||
	    block->encryptedBroadcastKeys == NULL || block->revocationInformation->data == NULL)
		return MLN_BKB_ERROR_FAILED;
	revocation = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*revocation));
	if (revocation == NULL)
		return 2;
	MlnMemset(revocation, 0, sizeof(*revocation));
	size = block->revocationInformation->size;
	p = block->revocationInformation->data;
	if (size < 40)
		goto format;
	revocation->size = MLN_BKB_WORD(p);
	if (revocation->size != size + block->encryptedBroadcastKeys->size + 128)
		goto format;
	if (MLN_BKB_WORD(p + 4) != 0) {
		ret = MLN_BKB_ERROR_VERSION;
		goto out;
	}
	revocation->id = MlnAllocate(MLN_HEAP_GENERAL, 16);
	if (revocation->id == NULL)
		goto memory;
	MlnMemcpy(revocation->id, p + 12, 16);
	revocation->columns = MLN_BKB_WORD(p + 36);
	tableSize = MLN_BKB_DIGITS * revocation->columns;
	if (size != tableSize + 40)
		goto format;
	revocation->table = MlnAllocate(MLN_HEAP_GENERAL, tableSize);
	if (revocation->table == NULL)
		goto memory;
	MlnMemcpy(revocation->table, p + 40, tableSize);
	if (block->encryptedBroadcastKeys->size == 0)
		goto format;
	revocation->keys = MlnAllocate(MLN_HEAP_GENERAL, block->encryptedBroadcastKeys->size);
	if (revocation->keys == NULL)
		goto memory;
	MlnMemcpy(revocation->keys, block->encryptedBroadcastKeys->data,
		  block->encryptedBroadcastKeys->size);
	ret = MlnImpl_8107D270(revocation, key, out);
	goto out;

memory:
	ret = 2;
	goto out;
format:
	ret = MLN_BKB_ERROR_FORMAT;
out:
	MlnBkbRevocationFree(revocation);
	return ret;
}

/* the node's key of a block in one buffer: revocation information, 128 bytes, keys */
int MlnImpl_8107E124(const MlnBuffer *block, const MlnBuffer *keyData, void **out)
{
	MlnBkbRevocation *revocation;
	const unsigned char *p;
	unsigned int size;
	unsigned int tableSize;
	unsigned int keysSize;
	int ret;

	if (keyData == NULL || block == NULL || block->data == NULL)
		return MLN_BKB_ERROR_FAILED;
	revocation = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*revocation));
	if (revocation == NULL)
		return 2;
	MlnMemset(revocation, 0, sizeof(*revocation));
	size = block->size;
	p = block->data;
	if (size < 40)
		goto format;
	revocation->size = MLN_BKB_WORD(p);
	revocation->id = MlnAllocate(MLN_HEAP_GENERAL, 16);
	if (revocation->id == NULL)
		goto memory;
	MlnMemcpy(revocation->id, p + 12, 16);
	revocation->columns = MLN_BKB_WORD(p + 36);
	tableSize = MLN_BKB_DIGITS * revocation->columns;
	if (size < tableSize + 40)
		goto format;
	revocation->table = MlnAllocate(MLN_HEAP_GENERAL, tableSize);
	if (revocation->table == NULL)
		goto memory;
	MlnMemcpy(revocation->table, p + 40, tableSize);
	keysSize = revocation->size - 168 - tableSize;
	if (keysSize == 0)
		goto format;
	revocation->keys = MlnAllocate(MLN_HEAP_GENERAL, keysSize);
	if (revocation->keys == NULL)
		goto memory;
	MlnMemcpy(revocation->keys, p + 168 + tableSize, keysSize);
	ret = MlnImpl_8107D270(revocation, keyData, out);
	MlnBkbRevocationFree(revocation);
	return ret;

memory:
	ret = 2;
	goto out;
format:
	ret = MLN_BKB_ERROR_FORMAT;
out:
	MlnBkbRevocationFree(revocation);
	return ret;
}
