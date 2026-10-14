/*
 * SceSysmem: the name heap's hash, MurmurHash3_x86_32 by Austin Appleby
 * (public domain, smhasher). Sony's copy returns the hash instead of storing
 * it through an out pointer, and rotates the tail by 16 where upstream
 * rotates by 15; the rest is the upstream algorithm.
 */

#include <psp2kern/types.h>

static inline SceUInt32 rotl32(SceUInt32 x, int r)
{
	return (x << r) | (x >> (32 - r));
}

static inline SceUInt32 fmix32(SceUInt32 h)
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

/* sceKernelNameHeapGetKey */
SceUInt32 SceSysmemForKernel_EB48682E(const char *key, SceSize len, SceUInt32 seed)
{
	const SceUInt8 *data = (const SceUInt8 *)key;
	const int nblocks = (int)len / 4;
	const SceUInt32 *blocks = (const SceUInt32 *)(data + nblocks * 4);
	const SceUInt8 *tail;
	const SceUInt32 c1 = 0xcc9e2d51;
	const SceUInt32 c2 = 0x1b873593;
	SceUInt32 h1 = seed, k1;
	int i;

	for (i = -nblocks; i; i++) {
		k1 = blocks[i];
		k1 *= c1;
		k1 = rotl32(k1, 15);
		k1 *= c2;
		h1 ^= k1;
		h1 = rotl32(h1, 13);
		h1 = h1 * 5 + 0xe6546b64;
	}

	tail = data + nblocks * 4;
	k1 = 0;
	switch (len & 3) {
	case 3:
		k1 ^= tail[2] << 16;
		/* fall through */
	case 2:
		k1 ^= tail[1] << 8;
		/* fall through */
	case 1:
		k1 ^= tail[0];
		k1 *= c1;
		k1 = rotl32(k1, 16);	/* upstream: 15 */
		k1 *= c2;
		h1 ^= k1;
	}

	h1 ^= len;
	return fmix32(h1);
}
