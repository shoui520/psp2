#ifndef LIBKERNEL_BACKTRACE_H
#define LIBKERNEL_BACKTRACE_H

#include <scetypes.h>

typedef struct SceKernelCallFrame {
	SceUIntVAddr sp;
	SceUIntVAddr pc;
} SceKernelCallFrame;

int sceKernelBacktrace(SceUID threadId, SceKernelCallFrame *frames, SceSize size,
		      SceUInt32 *count, SceInt32 mode);

#endif
