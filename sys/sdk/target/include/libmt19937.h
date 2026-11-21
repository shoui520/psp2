/*
 * libmt19937.h: the SDK's Mersenne Twister (MT19937) context (SDK Release 03.570.011,
 * target/include/libmt19937.h); the kernel's SceKernelUtilsForDriver keeps the same layout.
 */
#ifndef SCE_LIBMT19937_H
#define SCE_LIBMT19937_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_MT19937_ERROR_INVALID_POINTER		-2142699264	/* 0x80490100 */

#define SCE_MT19937_ERROR_INVALID_STATE			-2142699263	/* 0x80490101 */

typedef struct SceMt19937Context {
	SceUInt32	count;			/* the next word to replace */
	SceUInt32	state[624];
} SceMt19937Context;

#ifndef __cplusplus
_Static_assert(sizeof(SceMt19937Context) == 0x9c4, "SceMt19937Context size");
#endif

SceInt32  sceMt19937Init(SceMt19937Context *pCtx, SceUInt32 seed);
SceUInt32 sceMt19937UInt(SceMt19937Context *pCtx);

#ifdef __cplusplus
}
#endif

#endif /* SCE_LIBMT19937_H */
