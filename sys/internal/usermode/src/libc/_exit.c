/* _Exit function */
#include <stdlib.h>
#include <kernel/processmgr.h>

#include "libc_internal.h"
#include "psp2io.h"

void _Exit(int status)
{
	sceKernelCallAbortHandler(status == 0 ? SCE_LIBC__EXIT_SUCCESS_CODE
					      : SCE_LIBC__EXIT_FAILURE_CODE, NULL);
	_ExitProcess(status);
}
