/* Boot DIP-switch state and kernel accessors. */
#include <stdint.h>
#include <sysmem/dipsw.h>
#include "sysmem_boot.h"
#include "sysmem_internal.h"
#if SYSMEM_NSKBL || defined(SYSMEM_SKBL)
#include <sysmem/cp15.h>
#endif

union DipswState {
	SceDipsw info;
	uint32_t words[8];
};

static union DipswState s_bootDipsw;
static union DipswState *s_dipsw;

#if SYSMEM_NSKBL || defined(SYSMEM_SKBL)
/*
 * name unknown: the boot loader's start of the performance monitor, its counters reset
 * and enabled (between l2cc_maintenance.S and this file in NSKBL; no file of os0 has it)
 */
int KblPmuInit(void *sysroot)
{
	(void)sysroot;
	writePmcr(PMCR_C);
	writePmcr(PMCR_E);
	return 0;
}
#endif

/*
 * Name unknown; replaces the DIP-switch words the kernel reads (no call reaches it in
 * os0; the boot loaders point it at their KBL Param's)
 */
int DipswSetState(SceDipsw *dipsw)
{
	s_dipsw = (union DipswState *)dipsw;
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

#if !defined(SYSMEM_SKBL)
SceUInt32 SceDipswForDriver_B2AD48BE(unsigned int index)
{
	return s_dipsw->words[index];
}
#endif

__attribute__((noinline))
int sceKernelCheckDipsw(unsigned int bit)
{
	return (s_dipsw->words[bit >> 5] >> (bit & 31)) & 1;
}

#if !SYSMEM_NSKBL || defined(SYSMEM_SKBL)
__attribute__((noinline))
void sceKernelSetDipsw(unsigned int bit)
{
	s_dipsw->words[bit >> 5] |= UINT32_C(1) << (bit & 31);
}
#endif

__attribute__((noinline))
void sceKernelClearDipsw(unsigned int bit)
{
	s_dipsw->words[bit >> 5] &= ~(UINT32_C(1) << (bit & 31));
}

#if !SYSMEM_NSKBL && !defined(SYSMEM_SKBL)
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

int sceKernelSetDipswForUser(unsigned int bit)
{
	int result;

	if (bit > 255)
		return SCE_KERNEL_ERROR_ILLEGAL_DIPSW_NUMBER;
	if (bit > 63) {
		result = CheckDipswAccess();
		if (result < 0)
			return result;
	}
	sceKernelSetDipsw(bit);
	return 0;
}

int sceKernelClearDipswForUser(unsigned int bit)
{
	int result;

	if (bit > 255)
		return SCE_KERNEL_ERROR_ILLEGAL_DIPSW_NUMBER;
	if (bit > 63) {
		result = CheckDipswAccess();
		if (result < 0)
			return result;
	}
	sceKernelClearDipsw(bit);
	return 0;
}
#endif
