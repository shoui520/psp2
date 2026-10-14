/* Descriptive file name: no source-file debug records exist in the original. */
#include <sce/processmgr_imports.h>

int copyVideoDataFromUser(void *destination, const void *source, SceSize size)
{
	return ksceKernelCopyFromUser(destination, source, size);
}

int copyVideoDataToUser(void *destination, const void *source, SceSize size)
{
	return ksceKernelCopyToUser(destination, source, size);
}
