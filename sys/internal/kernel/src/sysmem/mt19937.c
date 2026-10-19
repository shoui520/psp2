/*
 * SceKernelUtils: the Mersenne Twister MT19937. Sony's generator advances one
 * state word per call ("on the fly"): it returns the tempered word it is about
 * to replace, so the first 624 outputs after seeding are the seed table's and
 * SceKernelUtilsForDriver_4C9A5730 discards them.
 *
 * The kernel's own generator (GlobalInit/GlobalUninit) is shared under a
 * spinlock.
 */

#include <stdint.h>
#include <sysmem/utils.h>
#include <kernel/error.h>
#include <sysmem/cpu.h>

#define MT_N			624
#define MT_M			397
#define MT_MATRIX_A		0x9908b0df
#define MT_UPPER_MASK		0x80000000
#define MT_LOWER_MASK		0x7fffffff

_Static_assert(sizeof(SceMt19937Context) == 0x9c4, "MT19937 context size");

static SceMt19937Context s_mt19937;
static volatile SceKernelSpinlock s_mt19937Lock;

/* note: in the original this one is hand-written assembly */
SceUInt32 SceKernelUtilsForDriver_92AEDFBC(SceMt19937Context *context)
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
int SceKernelUtilsForDriver_4C9A5730(SceMt19937Context *context, SceUInt32 seed)
{
	unsigned int i;

	context->state[0] = seed;
	for (i = 1; i < MT_N; i++)
		context->state[i] = 0x6c078965 *
			(context->state[i - 1] ^ (context->state[i - 1] >> 30)) + i;
	context->count = 0;
	for (i = MT_N; i != 0; i--)
		SceKernelUtilsForDriver_92AEDFBC(context);
	return 0;
}

int SceKernelUtilsForDriver_D428CC2A(SceUInt32 seed)
{
	SceKernelUtilsForDriver_4C9A5730(&s_mt19937, seed);
	return 0;
}

/* fills a word-aligned buffer from the kernel's generator */
int SceKernelUtilsForDriver_875B2A1C(SceUInt32 *buffer, SceSize size)
{
	unsigned int i, count;
	int flags;

	if (((uintptr_t)buffer & 3) || (size & 3))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	flags = sceKernelSpinlockLowLockCpuSuspendIntr(&s_mt19937Lock);
	count = size >> 2;
	for (i = 0; i != count; i++)
		*buffer++ = SceKernelUtilsForDriver_92AEDFBC(&s_mt19937);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mt19937Lock, flags);
	return 0;
}
