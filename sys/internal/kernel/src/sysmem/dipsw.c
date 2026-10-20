/* Boot DIP-switch state and kernel accessors. */
#include <stdint.h>
#include <sysmem/dipsw.h>
#include "sysmem_boot.h"
#include "sysmem_internal.h"

union DipswState {
	SceDipsw info;
	uint32_t words[8];
};

static union DipswState s_bootDipsw;
static union DipswState *s_dipsw;

/* Name unknown; replaces the DIP-switch words the kernel reads (no call reaches it). */
int DipswSetState(union DipswState *dipsw)
{
	s_dipsw = dipsw;
	return 0;
}

/* Name unknown; called before the first startup DIP-switch query. */
int InitializeDipsw(const struct SysmemBootContext *context)
{
	const uint32_t *word = ((const union DipswState *)&context->kblParam->dipsw)->words;
	const uint32_t *end = word + 8;
	union DipswState *dipsw = &s_bootDipsw;
	uint32_t *copy = dipsw->words;

	do
		*copy++ = *word++;
	while (word != end);
	s_dipsw = dipsw;
	return 0;
}

SceUInt32 SceDipswForDriver_B2AD48BE(unsigned int index)
{
	return s_dipsw->words[index];
}

__attribute__((noinline))
int sceKernelCheckDipsw(unsigned int bit)
{
	return (s_dipsw->words[bit >> 5] >> (bit & 31)) & 1;
}

__attribute__((noinline))
void SceDipswForDriver_82E45FBF(unsigned int bit)
{
	s_dipsw->words[bit >> 5] |= UINT32_C(1) << (bit & 31);
}

__attribute__((noinline))
void SceDipswForDriver_F1F3E9FE(unsigned int bit)
{
	s_dipsw->words[bit >> 5] &= ~(UINT32_C(1) << (bit & 31));
}

/* Name unknown; high DIP-switch numbers require process capability bit1. */
__attribute__((noinline))
int CheckDipswAccess(void)
{
	return CheckProcessDebugCapability(0) == 1 ? 0 :
		SCE_KERNEL_ERROR_ILLEGAL_DIPSW_NUMBER;
}

int sceKernelCheckDipswForUser(unsigned int bit)
{
	int result;

	if (bit > 255)
		return SCE_KERNEL_ERROR_ILLEGAL_DIPSW_NUMBER;
	if (bit > 63) {
		result = CheckDipswAccess();
		if (result < 0)
			return result;
	}
	return sceKernelCheckDipsw(bit);
}

int sceKernelSetDipsw(unsigned int bit)
{
	int result;

	if (bit > 255)
		return SCE_KERNEL_ERROR_ILLEGAL_DIPSW_NUMBER;
	if (bit > 63) {
		result = CheckDipswAccess();
		if (result < 0)
			return result;
	}
	SceDipswForDriver_82E45FBF(bit);
	return 0;
}

int sceKernelClearDipsw(unsigned int bit)
{
	int result;

	if (bit > 255)
		return SCE_KERNEL_ERROR_ILLEGAL_DIPSW_NUMBER;
	if (bit > 63) {
		result = CheckDipswAccess();
		if (result < 0)
			return result;
	}
	SceDipswForDriver_F1F3E9FE(bit);
	return 0;
}
