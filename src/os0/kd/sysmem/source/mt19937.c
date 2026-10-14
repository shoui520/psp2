/*
 * SceKernelUtils: the Mersenne Twister MT19937. Sony's generator advances one
 * state word per call ("on the fly"): it returns the tempered word it is about
 * to replace, so the first 624 outputs after seeding are the seed table's and
 * ksceMt19937Init discards them.
 *
 * The kernel's own generator (GlobalInit/GlobalUninit) is shared under a
 * spinlock.
 */

#include <stdint.h>
#include <psp2kern/types.h>
#include <sce/error.h>

#define MT_N			624
#define MT_M			397
#define MT_MATRIX_A		0x9908b0df
#define MT_UPPER_MASK		0x80000000
#define MT_LOWER_MASK		0x7fffffff

typedef struct SceKernelMt19937Context {
	SceUInt32 count;		/* the next word to replace */
	SceUInt32 state[MT_N];
} SceKernelMt19937Context;

_Static_assert(sizeof(SceKernelMt19937Context) == 0x9c4, "MT19937 context size");

int ksceKernelSpinlockLowLockCpuSuspendIntr(volatile SceUInt32 *lock);
void ksceKernelSpinlockLowUnlockCpuResumeIntr(volatile SceUInt32 *lock, int flags);

static SceKernelMt19937Context s_mt19937;
static volatile SceUInt32 s_mt19937Lock;

/* note: in the original this one is hand-written assembly */
SceUInt32 ksceMt19937UInt(SceKernelMt19937Context *context)
{
	SceUInt32 i = context->count, next, y, value;

	next = i == MT_N - 1 ? 0 : i + 1;
	value = context->state[i];
	context->count = next;
	y = (value & MT_UPPER_MASK) | (context->state[next] & MT_LOWER_MASK);
	context->state[i] = context->state[(int)i - (MT_N - MT_M) >= 0 ? i - (MT_N - MT_M) :
					   i + MT_M] ^ (y >> 1) ^ ((y & 1) ? MT_MATRIX_A : 0);

	/* tempering */
	value ^= value >> 11;
	value ^= (value << 7) & 0x9d2c5680;
	value ^= (value << 15) & 0xefc60000;
	return value ^ (value >> 18);
}

/* the seed table, then the first 624 outputs (the table itself) dropped */
int ksceMt19937Init(SceKernelMt19937Context *context, SceUInt32 seed)
{
	unsigned int i;

	context->state[0] = seed;
	for (i = 1; i < MT_N; i++)
		context->state[i] = 0x6c078965 *
			(context->state[i - 1] ^ (context->state[i - 1] >> 30)) + i;
	context->count = 0;
	for (i = MT_N; i != 0; i--)
		ksceMt19937UInt(context);
	return 0;
}

int ksceMt19937GlobalInit(SceUInt32 seed)
{
	ksceMt19937Init(&s_mt19937, seed);
	return 0;
}

/* fills a word-aligned buffer from the kernel's generator */
int ksceMt19937GlobalUninit(SceUInt32 *buffer, SceSize size)
{
	unsigned int i, count;
	int flags;

	if (((uintptr_t)buffer & 3) || (size & 3))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	flags = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_mt19937Lock);
	count = size >> 2;
	for (i = 0; i != count; i++)
		*buffer++ = ksceMt19937UInt(&s_mt19937);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_mt19937Lock, flags);
	return 0;
}
