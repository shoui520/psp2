/* SceKernelUtils: Marsaglia's xorshift128 generator. */

#include <psp2kern/types.h>

typedef struct SceXorshift128State {
	SceUInt32 x, y, z, w;
} SceXorshift128State;

/* a state of all zeros never leaves zero: make it all ones */
int SceKernelUtilsForDriver_B55C69B7(SceXorshift128State *state)
{
	if ((state->x | state->y | state->z | state->w) == 0) {
		state->z = 1;
		state->w = 1;
		state->x = 1;
		state->y = 1;
	}
	return 0;
}

/* sceXorshift128: the next value */
SceUInt32 SceKernelUtilsForDriver_335AF34D(SceXorshift128State *state)
{
	SceUInt32 t = state->x ^ (state->x << 11);
	SceUInt32 w = state->w;

	state->x = state->y;
	state->y = state->z;
	state->z = w;
	state->w = (w ^ (w >> 19)) ^ (t ^ (t >> 8));
	return state->w;
}
