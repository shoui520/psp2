#include <sysmem/sysmem.h>
#include <string.h>

#include "bt_internal.h"

int BtCopyIn(const void *source, void *destination, unsigned int length,
	     int userMode)
{
	if (userMode)
		return sceKernelCopyFromUser(destination, source, length);
	memcpy(destination, source, length);
	return 0;
}

int BtCopyOut(const void *source, void *destination, unsigned int length,
	      int userMode)
{
	if (userMode)
		return sceKernelCopyToUser(destination, source, length);
	memcpy(destination, source, length);
	return 0;
}

int BtCopyOutProcess(const void *source, void *destination, unsigned int length,
		     SceUID processId)
{
	if (processId != -1)
		return sceKernelCopyToUserProc(processId, destination, source, length);
	memcpy(destination, source, length);
	return 0;
}
