/* The kernel HMAC dispatcher uses one complete descriptor per SHA variant. */
#ifndef SYSMEM_HMAC_INTERNAL_H
#define SYSMEM_HMAC_INTERNAL_H

#include <sysmem/utils.h>

typedef struct HmacAlgorithm {
	SceSize blockSize;
	SceSize digestSize;
	int (*digest)(const void *plain, SceSize len, void *result);
	int (*init)(void *context);
	int (*update)(void *context, const void *plain, SceSize len);
	int (*result)(void *context, void *result);
} HmacAlgorithm;

_Static_assert(sizeof(HmacAlgorithm) == 24, "kernel HMAC algorithm descriptor");

extern const HmacAlgorithm hmacSha1Algorithm;
extern const HmacAlgorithm hmacSha224Algorithm;
extern const HmacAlgorithm hmacSha256Algorithm;

/* Each context pointer has the same verified AAPCS pointer-argument ABI. */
#define HMAC_ALGORITHM(name, variant, digestFunc, initFunc, updateFunc, resultFunc) \
	const HmacAlgorithm name = { \
		.blockSize = SCE_SHA##variant##_BLOCK_SIZE, \
		.digestSize = SCE_SHA##variant##_DIGEST_SIZE, \
		.digest = digestFunc, \
		.init = (int (*)(void *))initFunc, \
		.update = (int (*)(void *, const void *, SceSize))updateFunc, \
		.result = (int (*)(void *, void *))resultFunc, \
	}

#endif
