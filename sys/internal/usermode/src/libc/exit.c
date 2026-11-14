/* exit function */
#include <stdlib.h>
#include <kernel/processmgr.h>
#include <libkernel/libkernel.h>

#include "libc_internal.h"

#define SCE_LIBC_EXIT_SUCCESS_CODE	0x102	/* sceKernelCallAbortHandler: exit(0) */
#define SCE_LIBC_EXIT_FAILURE_CODE	0x103	/* sceKernelCallAbortHandler: exit(nonzero) */

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
