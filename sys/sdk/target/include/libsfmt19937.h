/*
 * libsfmt19937.h: the SDK's SFMT-19937 random number generator (SDK Release 03.570.011,
 * target/include/libsfmt19937.h; the static library libSceSfmt19937.a).
 */
#ifndef _SCE_SFMT19937_LIBSFMT19937_H
#define _SCE_SFMT19937_LIBSFMT19937_H

#include <scetypes.h>

#define SCE_SFMT19937_ERROR_INVALID_POINTER	-2142697728	/* 0x80490700 */
#define SCE_SFMT19937_ERROR_INVALID_STATE	-2142697727	/* 0x80490701 */

#define SCE_SFMT19937_ARRAY_SIZE		156		/* (19937 / 128) + 1 */

typedef struct SceSfmt19937Context {
	SceUInt32 idx;
	SceUInt32 sfmt[SCE_SFMT19937_ARRAY_SIZE][4];
} SceSfmt19937Context;

SceInt32 sceSfmt19937InitGenRand(SceSfmt19937Context *pCtx, SceUInt32 seed);
SceInt32 sceSfmt19937InitByArray(SceSfmt19937Context *pCtx, const SceUInt32 initkey[],
				 SceUInt32 keylength);
SceUInt32 sceSfmt19937GenRand32(SceSfmt19937Context *pCtx);
SceUInt64 sceSfmt19937GenRand64(SceSfmt19937Context *pCtx);
SceInt32 sceSfmt19937FillArray32(SceSfmt19937Context *pCtx, SceUInt32 array[], SceUInt32 size);
SceInt32 sceSfmt19937FillArray64(SceSfmt19937Context *pCtx, SceUInt64 array[], SceUInt32 size);

#endif /* _SCE_SFMT19937_LIBSFMT19937_H */
