/* SceEnumWakeUp: The MTP heap. */
#include "ew_internal.h"

int mtpHeapCreate(SceSize size)
{
	SceKernelHeapCreateOpt opt;
	int ret;

	if (ewg.heap == 0) {
		ewg.heap = -1;
		if (size & 0xfff)
			return MTP_ERROR_NO_MEMORY;
	} else {
		if (size & 0xfff)
			return MTP_ERROR_NO_MEMORY;
		if (ewg.heap != -1)
			return MTP_ERROR_ALREADY;
	}
	opt.size = sizeof(opt);
	opt.attr = HEAP_AUTO_EXTEND | HEAP_HAS_MAX_SIZE;
	opt.maxSize = 0x80000;
	ret = ksceKernelCreateHeap("SceMtpHeap", size, &opt);
	if (ret < 0)
		ret = MTP_ERROR_NO_MEMORY;
	else
		ewg.heap = ret;
	ewg.allocOpt.size = sizeof(ewg.allocOpt);
	ewg.allocOpt.mmapKind = 0;
	ewg.allocOpt.align = 0x40;
	ewg.allocOpt.mmapBase = NULL;
	ewg.allocOpt.mmapSize = 0;
	return ret;
}

int mtpHeapDelete(void)
{
	if (ewg.heap == 0) {
		ewg.heap = -1;
		return MTP_ERROR_FAILED;
	}
	if (ewg.heap == -1)
		return MTP_ERROR_FAILED;
	ksceKernelDeleteHeap(ewg.heap);
	ewg.heap = -1;
	return 0;
}

void *mtpAlloc(SceSize size)
{
	return ksceKernelAllocHeapMemoryWithOption(ewg.heap, size, &ewg.allocOpt);
}

void mtpFree(void *p)
{
	if (p)
		ksceKernelFreeHeapMemory(ewg.heap, p);
}
