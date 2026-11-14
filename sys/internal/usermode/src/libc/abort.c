/* abort function */
#include <stdio.h>
#include <stdlib.h>
#include <kernel/processmgr.h>

#include "psp2io.h"
#include "libc_internal.h"

#define SCE_LIBC_ABORT_CODE	0x106	/* sceKernelCallAbortHandler: abort() */

void abort(void)
{
	_ExitInProgress = 1;
	sceKernelCallAbortHandler(SCE_LIBC_ABORT_CODE, NULL);
	_Write(_Stderr._Handle, "abort() is called.\n", 19);
	_Exit(EXIT_FAILURE);
}
