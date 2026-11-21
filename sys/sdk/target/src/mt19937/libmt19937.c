/*
 * libSceMt19937 (the SDK's static library, libmt19937.o): the Mersenne Twister MT19937, seeded
 * from one word. Sony built this file with GCC; the step, sceMt19937UInt, is its hand-written
 * assembly (mtrand.S). The generator returns the tempered word it is about to replace, so the
 * first 624 words after seeding are the seed table's, and the seeding drops them.
 */

#include <stddef.h>
#include <libmt19937.h>

#define MT19937_N	624

SceInt32 sceMt19937Init(SceMt19937Context *pCtx, SceUInt32 seed)
{
	SceUInt32 *pState;
	unsigned int i;

	if (pCtx == NULL)
		return SCE_MT19937_ERROR_INVALID_POINTER;
	pCtx->state[0] = seed;
	for (pState = &pCtx->state[1], i = 1; pState != &pCtx->state[MT19937_N]; pState++, i++)
		*pState = 0x6c078965 * (pState[-1] ^ (pState[-1] >> 30)) + i;
	pCtx->count = 0;
	for (i = MT19937_N; i != 0; i--)
		sceMt19937UInt(pCtx);
	return 0;
}
