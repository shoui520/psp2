/* exit function */
#include <stdlib.h>
#include <kernel/processmgr.h>
#include <libkernel/libkernel.h>

#include "libc_internal.h"

void *_Dso_handle_main;

void __cxa_set_dso_handle_main(void *dso)
{
	_Dso_handle_main = dso;
}

void exit(int status)
{
	sceKernelCallAbortHandler(status == 0 ? SCE_LIBC_EXIT_SUCCESS_CODE
					      : SCE_LIBC_EXIT_FAILURE_CODE, NULL);
	sceKernelCallModuleExit(1);
	__cxa_finalize(_Dso_handle_main);
	_Exit(status);
}
