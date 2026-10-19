/* Descriptive file name: no source-file debug records exist in the original. */
#include <scetypes.h>
#include <sysmem/sysmem.h>

int copyVideoDataFromUser(void *destination, const void *source, SceSize size)
{
	return sceKernelCopyFromUser(destination, source, size);
}

int copyVideoDataToUser(void *destination, const void *source, SceSize size)
{
	return sceKernelCopyToUser(destination, source, size);
}
