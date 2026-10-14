#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include "sce/compiler.h"

void sceClibAbort(void)
{
	__breakpoint(BKPT_ABORT);
	sceKernelExitProcess(-1);
	__builtin_unreachable();
}

void abort(void)
{
	sceClibAbort();
	__builtin_unreachable();
}

int __sce_aeabi_idiv0(int quotient)
{
	__breakpoint(BKPT_DIVIDE_BY_ZERO);
	return quotient;
}

long long __sce_aeabi_ldiv0(long long quotient)
{
	__breakpoint(BKPT_DIVIDE_BY_ZERO);
	return quotient;
}

void SceLibGcc_CD43FEDC(void)
{
}
