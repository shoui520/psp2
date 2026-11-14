/*
 * module.c: SceLibc's module entry points. module_start brings up the heap, the locks, the
 * thread-local storage lock and the standard streams, opens the process's standard I/O
 * descriptors and runs the library's static constructors; module_exit undoes it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <kernel/threadmgr.h>
#include <kernel/processmgr.h>
#include <processmgr/processmgr.h>
#include <libkernel/libkernel.h>

#include "psp2io.h"
#include "libc_internal.h"

typedef void (*InitFunc)(void);

extern InitFunc __start__Zpreinit_array[];
extern InitFunc __stop__Zpreinit_array[];
extern InitFunc __start__Zinit_array[];
extern InitFunc __stop__Zinit_array[];

SceUID _StdinFd;
SceUID _StdoutFd;
SceUID _StderrFd;
void *__dso_handle;
int _ExitInProgress;

int module_start(SceSize argc, const void *argp)
{
	InitFunc *init;
	int res;
	int err;

	__dso_handle = &__dso_handle;
	res = _MallocInit();
	if (res != 0) {
		sceClibPrintf("Initialize malloc : Failed (err=0x%08x)\n", res);
		SceThreadmgrCoredumpTime_sceKernelExitThread(2);
	}
	res = _InitLocks();
	if (res < 0) {
		sceClibPrintf("Initialize lock : Failed (err=0x%08x)\n", res);
		SceThreadmgrCoredumpTime_sceKernelExitThread(2);
	}
	res = _InitTlsMutex();
	if (res < 0) {
		sceClibPrintf("Initialize tls mutex : Failed (err=0x%08x)\n", res);
		SceThreadmgrCoredumpTime_sceKernelExitThread(2);
	}
	_InitStdFiles();

	err = 0;
	_StdinFd = sceKernelGetStdin();
	if (_StdinFd < 0)
		err = -1;
	_StdoutFd = sceKernelGetStdout();
	if (_StdoutFd < 0)
		err = -1;
	_StderrFd = sceKernelGetStderr();
	if (_StderrFd < 0)
		err = -1;
	if (err < 0)
		sceClibPrintf("openStdio : failed\n");

	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; init++) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; init++) {
		if (*init != NULL)
			(*init)();
	}
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return SCE_KERNEL_START_SUCCESS;
}

int module_exit(SceSize argc, const void *argp)
{
	if (_ExitInProgress == 0) {
		__cxa_finalize(&__dso_handle);
		_Fcloseall(0);
		_TermTlsMutex();
		_TermLocks();
		return _MallocTerm();
	}
	/* SCE: exit() is unloading the module: no status, r0 is what _Fcloseall leaves */
	_Fcloseall(1);
}

int atexit(void (*func)(void))
{
	return __cxa_atexit((void (*)(void *))func, NULL, &__dso_handle);
}

/*
 * A division by zero in SNC's helpers (__udivsi3, __udivdi3, __divdi3) stops at a
 * breakpoint: the module's own definitions take the place of the runtime's weak ones
 * (libsnc aeabi_div0.o).
 */
int __aeabi_idiv0(int return_value)
{
	__asm__ volatile("bkpt 0x80");
	return return_value;
}

long long __aeabi_ldiv0(long long return_value)
{
	__asm__ volatile("bkpt 0x80");
	return return_value;
}
