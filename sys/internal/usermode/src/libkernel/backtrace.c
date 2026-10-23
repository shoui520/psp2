#include "libkernel_backends.h"
#include "backtrace.h"
#include "lifecycle.h"
#include <kernel/error.h>

struct BacktraceOptions {
	SceUInt32 *count;
	SceInt32 mode;
};

extern int BacktraceCapture(SceUID processId, SceKernelCallFrame *frames,
		SceSize size, SceUInt32 *count, SceUInt32 flags);

int sceKernelBacktrace(SceUID threadId, SceKernelCallFrame *frames, SceSize size,
		      SceUInt32 *count, SceInt32 mode)
{
	struct BacktraceOptions options = { count, mode };

	return _sceKernelBacktrace(threadId, frames, size, &options);
}

int sceKernelBacktraceSelf(SceKernelCallFrame *frames, SceSize size,
		SceUInt32 *count, SceUInt32 flags)
{
	if (!backtraceAvailable)
		return SCE_KERNEL_ERROR_ILLEGAL_PERMISSION;
	return BacktraceCapture(0, frames, size, count, flags);
}
