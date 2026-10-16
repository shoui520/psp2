/* Native exception wrappers ignore user-mode exceptions and capture kernel faults. */
#include "bsod_internal.h"
#include "exception_record.h"

BsodCrashRecord s_bsodException;

void _sceKernelBsodPrefetchAbort(const SceExcpmgrContext *context)
{
	if ((context->cpsr & 0x1f) != 0x10)
		bsodCaptureException(context, 4, "_sceKernelBsodPrefetchAbort");
}

void _sceKernelBsodDataAbort(const SceExcpmgrContext *context)
{
	if ((context->cpsr & 0x1f) != 0x10)
		bsodCaptureException(context, 1, "_sceKernelBsodDataAbort");
}

void _sceKernelBsodUndfinedAbort(const SceExcpmgrContext *context)
{
	if ((context->cpsr & 0x1f) != 0x10)
		bsodCaptureException(context, 6, "_sceKernelBsodUndfinedAbort");
}
