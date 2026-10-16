/* IMG pvr_debug.c formatting with the SCE unified native spinlock port. */
#include "sce_ddk_port.h"
#include <sce/debug.h>
#include <psp2kern/kernel/cpu/spinlock.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static SceKernelSpinlock gsDebugLockIRQ;

#if FW_DEBUG
static IMG_CHAR gszBufferIRQ[513];
static const SceKernelDebugInfo dbg_VBAppend_L101 = {
	0xc63ac79a, 0xd535cfa8, 0xa379d7a5,
	"env/playstation/pvr_debug.c", 101, "VBAppend"
};
#endif

/* SCE uses one interrupt-safe buffer lock instead of Linux's two buffers. */
IMG_VOID PVRDPFInit(IMG_VOID)
{
	gsDebugLockIRQ = 0;
}

#if FW_DEBUG
/* SCE combines upstream release-printf prefixing, VBAppend and printing. */
IMG_VOID VBAppend(const IMG_CHAR *pszFormat, ...)
{
	va_list VArgs;
	SceKernelIntrStatus ulLockFlags;
	IMG_UINT32 ui32Used;
	IMG_UINT32 ui32Space;
	IMG_INT32 i32Len;
	const IMG_CHAR *pszPrintFormat;

	va_start(VArgs, pszFormat);
	ulLockFlags = ksceKernelSpinlockLowLockCpuSuspendIntr(&gsDebugLockIRQ);
	strncpy(gszBufferIRQ, "PVR_K: ", 512);
	ui32Used = strnlen(gszBufferIRQ, sizeof(gszBufferIRQ));
	ksceKernelAssert(ui32Used < sizeof(gszBufferIRQ), &dbg_VBAppend_L101,
			__builtin_return_address(0));
	ui32Space = sizeof(gszBufferIRQ) - ui32Used;
	i32Len = vsnprintf(&gszBufferIRQ[ui32Used], ui32Space, pszFormat, VArgs);
	gszBufferIRQ[512] = 0;
	if (i32Len < 0)
		goto truncated;
	if (i32Len >= (IMG_INT32)ui32Space)
		goto truncated;
	pszPrintFormat = "%s\n";
	goto print;
truncated:
	pszPrintFormat = "PVR_K:(Message Truncated): %s\n";
print:
	ksceKernelPrintf(pszPrintFormat, gszBufferIRQ);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&gsDebugLockIRQ, ulLockFlags);
	va_end(VArgs);
}
#endif
