#include <psp2/types.h>

struct ThreadSizedRecord;

extern int sceKernelSuspendThreadForVM(SceUID id);
extern int sceKernelResumeThreadForVM(SceUID id);
extern int sceKernelSetThreadContextForVM(SceUID id,
		const struct ThreadSizedRecord *cpu,
		const struct ThreadSizedRecord *vfp);
extern int sceKernelGetThreadContextForVM(SceUID id,
		struct ThreadSizedRecord *cpu, struct ThreadSizedRecord *vfp);
extern int sceKernelWaitException(SceUID id, struct ThreadSizedRecord *info,
		SceUInt *timeout);
extern int sceKernelWaitExceptionCB(SceUID id, struct ThreadSizedRecord *info,
		SceUInt *timeout);

int SceKernelForVM_F5F8F795(SceUID id)
{
	int result = sceKernelSuspendThreadForVM(id);

	return result & (result >> 31);
}

int SceKernelForVM_70F3F49D(SceUID id)
{
	int result = sceKernelResumeThreadForVM(id);

	return result & (result >> 31);
}

int SceKernelForVM_010BB885(SceUID id, struct ThreadSizedRecord *cpu,
		struct ThreadSizedRecord *vfp)
{
	return sceKernelGetThreadContextForVM(id, cpu, vfp);
}

int SceKernelForMono_38839DA2(SceUID id)
{
	return sceKernelSuspendThreadForVM(id);
}

int SceKernelForMono_9A6D085B(SceUID id)
{
	return sceKernelResumeThreadForVM(id);
}

int SceKernelForMono_92A0964D(SceUID id,
		const struct ThreadSizedRecord *cpu,
		const struct ThreadSizedRecord *vfp)
{
	return sceKernelSetThreadContextForVM(id, cpu, vfp);
}

int SceKernelForMono_AD210F16(SceUID id, struct ThreadSizedRecord *cpu,
		struct ThreadSizedRecord *vfp)
{
	return sceKernelGetThreadContextForVM(id, cpu, vfp);
}

int SceKernelForMono_1BECC64C(SceUID id, struct ThreadSizedRecord *info,
		SceUInt *timeout)
{
	return sceKernelWaitException(id, info, timeout);
}

int SceKernelForMono_E513151F(SceUID id, struct ThreadSizedRecord *info,
		SceUInt *timeout)
{
	return sceKernelWaitExceptionCB(id, info, timeout);
}
