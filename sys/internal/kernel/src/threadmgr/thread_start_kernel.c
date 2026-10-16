/* SceKernelThreadMgr: kernel thread-start entry points. */
#include <psp2kern/types.h>

int startThread(SceUID threadId, SceSize argSize, const void *arg, int checkSource);

int ksceKernelStartThread(SceUID threadId, SceSize argSize, const void *arg)
{
	return startThread(threadId, argSize, arg, 0);
}

/* A start whose argument may not be kernel memory. */
int SceThreadmgrForKernel_CBADD6B3(SceUID threadId, SceSize argSize, const void *arg)
{
	return startThread(threadId, argSize, arg, 1);
}
