#include <psp2kern/kernel/sysmem/data_transfers.h>
#include <string.h>

#include "bt_internal.h"

int BtCopyIn(const void *source, void *destination, unsigned int length,
	     int userMode)
{
	if (userMode)
		return ksceKernelCopyFromUser(destination, source, length);
	memcpy(destination, source, length);
	return 0;
}

int BtCopyOut(const void *source, void *destination, unsigned int length,
	      int userMode)
{
	if (userMode)
		return ksceKernelCopyToUser(destination, source, length);
	memcpy(destination, source, length);
	return 0;
}

int BtCopyOutProcess(const void *source, void *destination, unsigned int length,
		     SceUID processId)
{
	if (processId != -1)
		return ksceKernelCopyToUserProc(processId, destination, source, length);
	memcpy(destination, source, length);
	return 0;
}
