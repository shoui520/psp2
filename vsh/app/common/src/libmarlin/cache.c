/*
 * The library's cache: values by kind and key in the shared state, each kind a single
 * entry or a table of 16 chains by the key's hash, with the entries changed since the
 * last flush on a dirty list (0x8105D444-). The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_CACHE_KINDS		18
#define MLN_CACHE_BUCKETS	16

typedef struct MlnCacheEntry {
	struct MlnCacheEntry *next;		/* in its chain */
	struct MlnCacheEntry *dirtyNext;	/* on the dirty list, the last one itself */
	unsigned int key;
	MlnBuffer *value;
} MlnCacheEntry;

typedef struct MlnCache {
	void *kinds[MLN_CACHE_KINDS];	/* an entry, or a table of MLN_CACHE_BUCKETS chains */
	int count;
	unsigned int size;		/* of every value */
	MlnCacheEntry *dirty;
	int dirtyCount;
	unsigned int dirtySize;
} MlnCache;

/* whether a kind holds one entry, not a table */
static inline int MlnCacheIsSingle(unsigned int kind)
{
	switch (kind) {
	case 3:
	case 4:
	case 9:
	case 10:
	case 13:
	case 14:
	case 15:
	case 16:
	case 17:
		return 1;
	}
	return 0;
}

static inline unsigned int MlnCacheValueSize(const MlnBuffer *value)
{
	return value != NULL ? value->size : 0;
}

/* the cache, made on first use */
static inline MlnCache *MlnCacheGet(void)
{
	MlnShared *shared = MlnImpl_81006EAC();
	MlnCache *cache = shared->buffers[0];

	if (cache == NULL) {
		cache = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*cache));
		if (cache == NULL)
			return NULL;
		shared->buffers[0] = cache;
		MlnMemset(cache, 0, sizeof(cache->kinds));
		cache->dirty = NULL;
		cache->count = 0;
		cache->size = 0;
		cache->dirtyCount = 0;
		cache->dirtySize = 0;
	}
	return cache;
}

/* the entry of a kind and key, or NULL */
static inline MlnCacheEntry *MlnCacheFind(MlnCache *cache, unsigned int kind,
					  unsigned int hash, unsigned int key)
{
	MlnCacheEntry *entry = cache->kinds[kind];

	if (MlnCacheIsSingle(kind))
		return entry;
	if (entry == NULL)
		return NULL;
	for (entry = ((MlnCacheEntry **)entry)[hash & 0xF]; entry != NULL; entry = entry->next) {
		if (entry->key == key)
			return entry;
	}
	return NULL;
}

/* the value cached for a kind and key, or NULL */
int MlnImpl_8105D444(unsigned int kind, unsigned int hash, unsigned int key, MlnBuffer **value)
{
	MlnCache *cache;
	MlnCacheEntry *entry;

	cache = MlnCacheGet();
	if (cache == NULL)
		return 2;
	entry = MlnCacheFind(cache, kind, hash, key);
	*value = entry != NULL ? entry->value : NULL;
	return 0;
}

/* take an entry off the dirty list's count */
static inline void MlnCacheClean(MlnCache *cache, MlnCacheEntry *entry)
{
	entry->dirtyNext = NULL;
	cache->dirtySize -= MlnCacheValueSize(entry->value);
	cache->dirtyCount--;
}

/* empty the dirty list */
int MlnImpl_8105D52C(void)
{
	MlnCache *cache;
	MlnCacheEntry *entry;
	MlnCacheEntry *next;

	cache = MlnCacheGet();
	if (cache == NULL)
		return 2;
	entry = cache->dirty;
	if (entry != NULL) {
		while (entry->dirtyNext != entry) {
			next = entry->dirtyNext;
			MlnCacheClean(cache, entry);
			entry = next;
		}
		MlnCacheClean(cache, entry);
	}
	cache->dirty = NULL;
	return 0;
}

/* make an entry for a key and value */
static inline int MlnCacheEntryCreate(unsigned int key, MlnBuffer *value, MlnCacheEntry **out)
{
	MlnCacheEntry *entry;
	int ret;

	entry = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*entry));
	if (entry == NULL) {
		ret = 2;
		goto out;
	}
	entry->key = key;
	entry->next = NULL;
	entry->dirtyNext = NULL;
	entry->value = value;
	*out = entry;
	entry = NULL;
	ret = 0;
out:
	MlnFree(MLN_HEAP_GENERAL, entry);
	return ret;
}

/* make a kind's table */
static inline int MlnCacheTableCreate(MlnCacheEntry ***out)
{
	MlnCacheEntry **table;
	int ret;

	table = MlnAllocate(MLN_HEAP_GENERAL, MLN_CACHE_BUCKETS * sizeof(MlnCacheEntry *));
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(table, 0, MLN_CACHE_BUCKETS * sizeof(MlnCacheEntry *));
	*out = table;
	table = NULL;
	ret = 0;
out:
	MlnFree(MLN_HEAP_GENERAL, table);
	return ret;
}

/* put an entry on the dirty list */
static inline void MlnCacheMarkDirty(MlnCache *cache, MlnCacheEntry *entry)
{
	entry->dirtyNext = cache->dirty != NULL ? cache->dirty : entry;
	cache->dirty = entry;
	cache->dirtyCount++;
	cache->dirtySize += MlnCacheValueSize(entry->value);
}

/* replace an entry's value, keeping its place on the dirty list, or putting it there */
static inline void MlnCacheReplace(MlnCache *cache, MlnCacheEntry *entry, MlnBuffer *value,
				   int dirty)
{
	if (entry->dirtyNext != NULL) {
		cache->count--;
		cache->size -= MlnCacheValueSize(entry->value);
		cache->dirtyCount--;
		cache->dirtySize -= MlnCacheValueSize(entry->value);
		MlnBufferDestroy(MLN_HEAP_GENERAL, entry->value);
		entry->value = value;
		cache->count++;
		cache->size += MlnCacheValueSize(value);
		cache->dirtyCount++;
		cache->dirtySize += MlnCacheValueSize(entry->value);
		return;
	}
	cache->count--;
	cache->size -= MlnCacheValueSize(entry->value);
	MlnBufferDestroy(MLN_HEAP_GENERAL, entry->value);
	entry->value = value;
	cache->count++;
	cache->size += MlnCacheValueSize(value);
	if (dirty)
		MlnCacheMarkDirty(cache, entry);
}

/* cache a value for a kind and key, replacing what was there; dirty puts it on the list */
int MlnImpl_8105D620(unsigned int kind, unsigned int hash, unsigned int key, int dirty,
		     MlnBuffer *value)
{
	MlnCache *cache;
	MlnCacheEntry *entry;
	MlnCacheEntry **table;
	MlnCacheEntry **bucket;
	int ret;

	cache = MlnCacheGet();
	if (cache == NULL)
		return 2;
	if (MlnCacheIsSingle(kind)) {
		entry = cache->kinds[kind];
		if (entry != NULL) {
			MlnCacheReplace(cache, entry, value, dirty);
			return 0;
		}
		ret = MlnCacheEntryCreate(key, value, &entry);
		if (ret != 0)
			return ret;
		cache->kinds[kind] = entry;
	} else {
		table = cache->kinds[kind];
		if (table == NULL) {
			ret = MlnCacheTableCreate(&table);
			if (ret != 0)
				return ret;
			cache->kinds[kind] = table;
		}
		bucket = &table[hash & 0xF];
		for (entry = *bucket; entry != NULL; entry = entry->next) {
			if (entry->key == key) {
				MlnCacheReplace(cache, entry, value, dirty);
				return 0;
			}
		}
		ret = MlnCacheEntryCreate(key, value, &entry);
		if (ret != 0)
			return ret;
		entry->next = *bucket;
		*bucket = entry;
	}
	cache->count++;
	cache->size += MlnCacheValueSize(entry->value);
	if (dirty)
		MlnCacheMarkDirty(cache, entry);
	return 0;
}

/*
 * Kinds whose values a load could not read, a bit per index (below 8) in the kind's byte
 * and the ones after it; a store clears the bit.
 */
static unsigned char s_cacheUnreadable[MLN_CACHE_KINDS];

/* the key a kind and index are cached under */
static inline int MlnCacheKey(unsigned int kind, int index, unsigned int *key)
{
	switch (kind) {
	case 2:
		*key = index + 0x88;
		break;
	case 3:
		*key = 0;
		break;
	case 4:
		*key = 1;
		break;
	case 6:
		*key = index + 0x98;
		break;
	case 7:
		*key = index + 0x80;
		break;
	case 9:
		*key = 0x21;
		break;
	case 10:
		*key = 0x20;
		break;
	case 11:
		*key = index + 0x90;
		break;
	case 12:
		*key = index + 0xA0;
		break;
	case 13:
		*key = 0x22;
		break;
	case 14:
		*key = 0x23;
		break;
	case 15:
		*key = 3;
		break;
	case 16:
		*key = 4;
		break;
	case 17:
		*key = 5;
		break;
	default:
		return 5;
	}
	return 0;
}

/* cache a copy of a value for a kind and index, on the dirty list */
int MlnImpl_8105DAD0(unsigned int kind, int index, const MlnBuffer *value)
{
	MlnBuffer *copy;
	unsigned int key;
	int ret;

	if (MlnCacheKey(kind, index, &key) != 0) {
		copy = NULL;
		ret = 5;
		goto out;
	}
	if (value != NULL) {
		copy = MlnBufferCreate(MLN_HEAP_GENERAL, value->size, value->data);
		if (copy == NULL) {
			ret = 2;
			goto out;
		}
	} else {
		copy = NULL;
	}
	ret = MlnImpl_8105D620(kind, index, key, 1, copy);
	if (ret == 0)
		copy = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, copy);
	return ret;
}

/* store a copy of a value for a kind and index, which is then readable again */
int MlnImpl_8105DC0C(unsigned int kind, int index, const MlnBuffer *value)
{
	MlnBuffer *copy;
	unsigned int key;
	int ret;

	if (kind - 1 > 16) {
		copy = NULL;
		ret = 3;
		goto out;
	}
	if (value != NULL) {
		copy = MlnBufferCreate(MLN_HEAP_GENERAL, value->size, value->data);
		if (copy == NULL) {
			ret = 2;
			goto out;
		}
	} else {
		copy = NULL;
	}
	if (MlnCacheKey(kind, index, &key) != 0) {
		ret = 5;
		goto out;
	}
	ret = MlnImpl_8105D620(kind, index, key, 1, copy);
	if (ret == 0) {
		copy = NULL;
		s_cacheUnreadable[kind + index / 8] &= ~(1 << (index % 8));
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, copy);
	return ret;
}

/* free a value a read below returned */
void MlnImpl_8105DD94(MlnBuffer *value)
{
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
}

/* a copy of the value cached for a kind and index; nothing is written when none is */
int MlnImpl_8105DDA8(unsigned int kind, int index, MlnBuffer **out)
{
	MlnBuffer *value = NULL;
	unsigned int key;
	int ret;

	if (kind - 1 > 16 || out == NULL)
		return 3;
	if (MlnCacheKey(kind, index, &key) != 0)
		return 5;
	ret = MlnImpl_8105D444(kind, index, key, &value);
	if (ret != 0 || value == NULL)
		return ret;
	*out = MlnBufferCreate(MLN_HEAP_GENERAL, value->size, value->data);
	return *out == NULL ? 2 : 0;
}

/*
 * A copy of the value cached for a kind and index: 1 when none is, 6 when a load could not
 * read it; out may be NULL to ask only whether there is one.
 */
int MlnImpl_8105DEDC(unsigned int kind, int index, MlnBuffer **out)
{
	MlnBuffer *value = NULL;
	MlnBuffer *copy = NULL;
	unsigned int key;
	int ret;

	if (kind - 1 > 16) {
		ret = 3;
		goto out;
	}
	switch (kind) {
	case 1:
	case 5:
	case 8:
		ret = 5;
		goto out;
	}
	if (MlnCacheKey(kind, index, &key) != 0) {
		ret = 5;
		goto out;
	}
	if ((s_cacheUnreadable[kind + index / 8] >> (index % 8)) & 1) {
		ret = 6;
		goto out;
	}
	ret = MlnImpl_8105D444(kind, index, key, &value);
	if (ret != 0)
		goto out;
	if (value == NULL) {
		ret = 1;
		goto out;
	}
	if (out == NULL)
		goto out;
	copy = MlnBufferCreate(MLN_HEAP_GENERAL, value->size, value->data);
	if (copy == NULL) {
		ret = 2;
		goto out;
	}
	*out = copy;
	copy = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, copy);
	return ret;
}

/* write an entry, its key and its value's size big-endian and then its value sealed */
static inline int MlnCacheWriteEntry(const MlnCacheEntry *entry, unsigned char **p,
				     unsigned int *remaining)
{
	MlnBuffer *sealed = NULL;
	unsigned int key = entry->key;
	const MlnBuffer *value = entry->value;
	const unsigned char *data;
	unsigned char *q = *p;
	unsigned int size;
	int ret;

	if (*remaining < 8) {
		ret = 5;
		goto out;
	}
	if (value != NULL) {
		ret = MlnImpl_8105ED38(key, value, &sealed);
		if (ret != 0)
			goto out;
		size = sealed->size;
		data = sealed->data;
	} else {
		size = 0;
		data = NULL;
	}
	q[0] = key >> 24;
	q[1] = key >> 16;
	q[2] = key >> 8;
	q[3] = key;
	q[4] = size >> 24;
	q[5] = size >> 16;
	q[6] = size >> 8;
	q[7] = size;
	if (size > *remaining - 8) {
		ret = 5;
		goto out;
	}
	q += 8;
	if (size != 0) {
		MlnMemcpy(q, data, size);
		q += size;
	}
	*remaining -= q - *p;
	*p = q;
	ret = 0;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, sealed);
	return ret;
}

/* the cache written out, all of it or only its dirty entries, and the dirty list emptied */
unsigned int MlnImpl_8105E060(int dirty, MlnBuffer **out)
{
	MlnCache *cache;
	MlnCacheEntry *entry;
	MlnCacheEntry **table;
	MlnBuffer *buffer;
	unsigned char *p;
	unsigned int remaining;
	unsigned int kind;
	int ret;
	int i;

	cache = MlnCacheGet();
	if (cache == NULL) {
		buffer = NULL;
		ret = 2;
		goto out;
	}
	if (out == NULL) {
		buffer = NULL;
		ret = 3;
		goto out;
	}
	if (dirty) {
		remaining = cache->dirtySize + 60 * cache->dirtyCount;
		buffer = MlnBufferCreate(MLN_HEAP_GENERAL, remaining, NULL);
		if (buffer == NULL) {
			ret = 2;
			goto out;
		}
		p = buffer->data;
		entry = cache->dirty;
		if (entry != NULL) {
			for (; entry->dirtyNext != entry; entry = entry->dirtyNext) {
				ret = MlnCacheWriteEntry(entry, &p, &remaining);
				if (ret != 0)
					goto out;
			}
			ret = MlnCacheWriteEntry(entry, &p, &remaining);
			if (ret != 0)
				goto out;
		}
	} else {
		remaining = cache->size + 60 * cache->count;
		buffer = MlnBufferCreate(MLN_HEAP_GENERAL, remaining, NULL);
		if (buffer == NULL) {
			ret = 2;
			goto out;
		}
		p = buffer->data;
		for (kind = 1; kind < MLN_CACHE_KINDS; kind++) {
			if (MlnCacheIsSingle(kind)) {
				entry = cache->kinds[kind];
				if (entry == NULL)
					continue;
				ret = MlnCacheWriteEntry(entry, &p, &remaining);
				if (ret != 0)
					goto out;
				continue;
			}
			table = cache->kinds[kind];
			if (table == NULL)
				continue;
			for (i = 0; i < MLN_CACHE_BUCKETS; i++) {
				for (entry = table[i]; entry != NULL; entry = entry->next) {
					ret = MlnCacheWriteEntry(entry, &p, &remaining);
					if (ret != 0)
						goto out;
				}
			}
		}
	}
	buffer->size -= remaining;
	ret = MlnImpl_8105D52C();
	if (ret == 0) {
		*out = buffer;
		buffer = NULL;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/* the kind and index a key is cached under */
static inline int MlnCacheKindOf(unsigned int key, unsigned int *kind, int *index)
{
	*index = 0;
	if (key == 3)
		*kind = 15;
	else if (key == 4)
		*kind = 16;
	else if (key == 5)
		*kind = 17;
	else if (key == 0)
		*kind = 3;
	else if (key == 1)
		*kind = 4;
	else if (key - 0x98 < 8) {
		*kind = 6;
		*index = key - 0x98;
	} else if (key - 0x80 < 8) {
		*kind = 7;
		*index = key - 0x80;
	} else if (key - 0x88 < 8) {
		*kind = 2;
		*index = key - 0x88;
	} else if (key - 0x90 < 8) {
		*kind = 11;
		*index = key - 0x90;
	} else if (key == 0x20)
		*kind = 10;
	else if (key == 0x21)
		*kind = 9;
	else if (key - 0xA0 < 8) {
		*kind = 12;
		*index = key - 0xA0;
	} else if (key == 0x22)
		*kind = 13;
	else if (key == 0x23)
		*kind = 14;
	else
		return 5;
	return 0;
}

/*
 * Load the cache from what a write above made; a value that cannot be unsealed is kept
 * empty and its kind and index marked unreadable.
 */
unsigned int MlnImpl_8105E5E4(const MlnBuffer *state)
{
	const unsigned char *p;
	MlnBuffer *sealed;
	MlnBuffer *value;
	MlnBuffer *stored;
	unsigned int remaining;
	unsigned int key;
	unsigned int size;
	unsigned int kind;
	int index;
	int ret;

	if (state == NULL) {
		stored = NULL;
		ret = 3;
		goto out;
	}
	MlnMemset(s_cacheUnreadable, 0, sizeof(s_cacheUnreadable));
	remaining = state->size;
	p = state->data;
	while (remaining >= 8) {
		key = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
		size = p[4] << 24 | p[5] << 16 | p[6] << 8 | p[7];
		value = NULL;
		if (size > remaining - 8) {
			ret = 5;
			goto fail;
		}
		sealed = MlnBufferCreate(MLN_HEAP_GENERAL, size, p + 8);
		if (sealed == NULL) {
			ret = 2;
			goto fail;
		}
		if (size == 0) {
			stored = sealed;
			sealed = NULL;
		} else {
			ret = MlnImpl_8105F124(key, sealed, &value);
			if (ret == 6) {
				if (MlnCacheKindOf(key, &kind, &index) == 0)
					s_cacheUnreadable[kind] |= 1 << index;
				stored = NULL;
			} else {
				stored = value;
				value = NULL;
				MlnBufferDestroy(MLN_HEAP_GENERAL, sealed);
				MlnBufferDestroy(MLN_HEAP_GENERAL, value);
				if (ret != 0)
					goto out;
				goto next;
			}
		}
		value = NULL;
		MlnBufferDestroy(MLN_HEAP_GENERAL, sealed);
		MlnBufferDestroy(MLN_HEAP_GENERAL, value);
next:
		p += size + 8;
		remaining -= size + 8;
		if (MlnCacheKindOf(key, &kind, &index) != 0) {
			ret = 5;
			goto out;
		}
		ret = MlnImpl_8105D620(kind, index, key, 0, stored);
		if (ret != 0)
			goto out;
	}
	stored = NULL;
	ret = remaining != 0 ? 5 : 0;
	goto out;
fail:
	stored = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, NULL);
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, stored);
	return ret;
}

/* free an entry and its value */
static inline int MlnCacheEntryFree(MlnCacheEntry *entry)
{
	MlnCache *cache;

	cache = MlnCacheGet();
	if (cache == NULL)
		return 2;
	if (entry != NULL) {
		cache->count--;
		cache->size -= MlnCacheValueSize(entry->value);
		MlnBufferDestroy(MLN_HEAP_GENERAL, entry->value);
		MlnFree(MLN_HEAP_GENERAL, entry);
	}
	return 0;
}

/* empty the cache */
unsigned int MlnImpl_8105EA2C(void)
{
	MlnCache *cache;
	MlnCacheEntry **table;
	MlnCacheEntry *entry;
	MlnCacheEntry *next;
	unsigned int kind;
	unsigned int ret;
	int i;

	cache = MlnCacheGet();
	if (cache == NULL)
		return 2;
	ret = MlnImpl_8105D52C();
	if (ret != 0)
		return ret;
	for (kind = 1; kind < MLN_CACHE_KINDS; kind++) {
		if (MlnCacheIsSingle(kind)) {
			if (MlnCacheEntryFree(cache->kinds[kind]) != 0)
				return 2;
		} else {
			table = cache->kinds[kind];
			if (table != NULL) {
				for (i = 0; i < MLN_CACHE_BUCKETS; i++) {
					for (entry = table[i]; entry != NULL; entry = next) {
						next = entry->next;
						MlnCacheEntryFree(entry);
					}
				}
				MlnFree(MLN_HEAP_GENERAL, table);
			}
		}
		cache->kinds[kind] = NULL;
	}
	return 0;
}

/* seal a value for a key */
int MlnImpl_8105EC7C(unsigned int key, const MlnBuffer *value, MlnBuffer **out)
{
	return MlnImpl_8105ED38(key, value, out);
}

/*
 * The key values are sealed with: the two halves of the SHA-256 of the library's secret
 * XORed, then decrypted with the personalization key of the platform.
 */
int MlnImpl_8105EC88(unsigned char *key)
{
	unsigned char digest[32];
	MlnBuffer *secret;
	int ret;
	int i;

	secret = MlnImpl_8106A08C(MLN_HEAP_GENERAL);
	if (secret == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_8105713C(secret->data, secret->size, digest);
	if (ret != 0)
		goto out;
	for (i = 0; i < 16; i++)
		digest[i] ^= digest[16 + i];
	switch (MlnPlatformMode()) {
	case 1:
		ret = MlnImpl_8105D34C(digest);
		break;
	case 2:
		ret = MlnImpl_8105D428(digest);
		break;
	default:
		ret = 5;
		goto out;
	}
	if (ret != 0)
		goto out;
	__builtin_memcpy(key, digest, 16);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, secret);
	return ret;
}

/*
 * Seal a value: its HMAC (20 bytes) over what follows, a random IV and the value encrypted
 * (AES-128-CBC, padded) with the cache's key. The key it is cached under is not used.
 */
__attribute__((noinline)) int MlnImpl_8105ED38(unsigned int key, const MlnBuffer *value,
					      MlnBuffer **out)
{
	MlnBuffer message = { 0, NULL };
	MlnBuffer hmacKey;
	MlnBuffer *sealed;
	MlnBuffer *digest;
	unsigned char *data;
	unsigned char iv[16];
	unsigned char cacheKey[16];
	unsigned int random;
	unsigned int size;
	void *context;
	int ret;
	int i;

	sealed = MlnBufferCreate(MLN_HEAP_GENERAL, ((value->size + 16) & ~15) + 36, NULL);
	if (sealed == NULL) {
		ret = 2;
		goto out;
	}
	data = sealed->data;

	context = NULL;
	ret = MlnImpl_8105714C(&context);
	if (ret != 0)
		goto encrypted;
	for (i = 0; i < 16; i++) {
		ret = MlnImpl_810571C4(context, sizeof(random), &random);
		if (ret != 0)
			goto encrypted;
		iv[i] = (random >> 16) ^ (random >> 24);
	}
	ret = MlnImpl_8105EC88(cacheKey);
	if (ret != 0)
		goto encrypted;
	size = (value->size + 16) & ~15;
	ret = MlnImpl_81054EDC(iv, cacheKey, value->data, value->size, data + 36, &size);
	if (ret != 0)
		goto encrypted;
	MlnMemcpy(data + 20, iv, sizeof(iv));
encrypted:
	MlnImpl_81057244(context);
	if (ret != 0)
		goto out;

	message.size = sealed->size - 20;
	message.data = sealed->data + 20;
	data = sealed->data;
	hmacKey.size = sizeof(cacheKey);
	hmacKey.data = cacheKey;
	digest = NULL;
	ret = MlnImpl_8105EC88(cacheKey);
	if (ret == 0)
		ret = MlnImpl_8106A0AC(1, &hmacKey, &message, &digest);
	if (ret != 0) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, digest);
		goto out;
	}
	MlnMemcpy(data, digest->data, digest->size);
	MlnBufferDestroy(MLN_HEAP_GENERAL, digest);
	*out = sealed;
	sealed = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, sealed);
	return ret;
}

/* decrypt a sealed value's IV and ciphertext */
static inline int MlnCacheDecrypt(const MlnBuffer *message, unsigned char *cacheKey,
				  MlnBuffer **out)
{
	MlnBuffer *value;
	unsigned int size;
	int ret;

	if (message->size < 16) {
		value = NULL;
		ret = 6;
		goto out;
	}
	ret = MlnImpl_8105EC88(cacheKey);
	if (ret != 0) {
		value = NULL;
		goto out;
	}
	size = message->size - 16;
	value = MlnBufferCreate(MLN_HEAP_GENERAL, size, NULL);
	if (value == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81054F84(message->data, cacheKey, message->data + 16, message->size - 16,
			       value->data, &size);
	if (ret == 0) {
		value->size = size;
		*out = value;
		value = NULL;
	} else if (ret != 2) {
		ret = 6;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	return ret;
}

/* unseal a value: 6 when its HMAC does not hold or it does not decrypt */
int MlnImpl_8105F124(unsigned int key, const MlnBuffer *sealed, MlnBuffer **out)
{
	MlnBuffer message = { 0, NULL };
	MlnBuffer hmacKey;
	MlnBuffer *digest;
	MlnBuffer *value = NULL;
	unsigned char cacheKey[16];
	unsigned char expected[20];
	int ret;

	if (sealed == NULL) {
		ret = 3;
		goto out;
	}
	if (sealed->size < 20) {
		ret = 6;
		goto out;
	}
	message.size = sealed->size - 20;
	message.data = sealed->data + 20;
	hmacKey.size = sizeof(cacheKey);
	hmacKey.data = cacheKey;
	digest = NULL;
	ret = MlnImpl_8105EC88(cacheKey);
	if (ret == 0)
		ret = MlnImpl_8106A0AC(1, &hmacKey, &message, &digest);
	if (ret != 0) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, digest);
		goto out;
	}
	MlnMemcpy(expected, digest->data, digest->size);
	MlnBufferDestroy(MLN_HEAP_GENERAL, digest);
	if (MlnMemcmp(expected, sealed->data, sizeof(expected)) != 0) {
		ret = 6;
		goto out;
	}
	ret = MlnCacheDecrypt(&message, cacheKey, &value);
	if (ret == 0) {
		*out = value;
		value = NULL;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	return ret;
}
