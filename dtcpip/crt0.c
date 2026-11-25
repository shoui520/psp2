/*
 * swagner's start code (the GCC build's own variant, not the SDK crt0 the shell links): the
 * DSO handle, the TLS module registration, the argument split (31 arguments at most), the
 * SDK's preload module list, the constructors from the linker's arrays, then main, and exit
 * with its result.
 */

#include <stddef.h>
#include <scetypes.h>
#include <stdlib.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>

/* the argument block's most arguments */
#define SWAGNER_ARGV_MAX	31

extern void __cxa_set_dso_handle_main(void *handle);
extern int _sceLdTlsRegisterModuleInfo(void *dso, void *moduleInfo, int flags);
void abort(void) __attribute__((noreturn));

/* the linker's constructor arrays and the SDK's preload list, all absent until something
 * defines them */
extern void (*__start__Zpreinit_array[])(void) __attribute__((weak));
extern void (*__stop__Zpreinit_array[])(void) __attribute__((weak));
extern void (*__start__Zinit_array[])(void) __attribute__((weak));
extern void (*__stop__Zinit_array[])(void) __attribute__((weak));
extern const char *const _sceUserModuleList[] __attribute__((weak));
extern const unsigned int _sceUserModuleListSize __attribute__((weak));

/* the C library's per-program state: the count and the arguments main gets */
int __argc;
char *__argv[SWAGNER_ARGV_MAX + 1];

/* the DSO handle __cxa_set_dso_handle_main registers, and the TLS module's information (a
 * variable of the program's own; its name is ours) */
void *__dso_handle;
void *g_swagnerTlsModuleInfo;

int main(int argc, char *argv[]);

void _start(SceSize args, char *argp)
{
	void (**init)(void);
	char *arg;
	unsigned int i;
	int argc;
	int length;

	__dso_handle = &__dso_handle;
	__cxa_set_dso_handle_main(&__dso_handle);
	if (g_swagnerTlsModuleInfo != NULL &&
	    _sceLdTlsRegisterModuleInfo(__dso_handle, g_swagnerTlsModuleInfo, 1) != 0)
		abort();

	arg = argp;
	for (argc = 0; ; argc++) {
		__argc = argc;
		if (argc >= SWAGNER_ARGV_MAX || arg - argp >= (int)args)
			break;
		__argv[argc] = arg;
		for (length = 0; arg[length] != '\0'; length++)
			;
		arg = arg + length + 1;
	}
	__argv[argc] = NULL;

	if (_sceUserModuleList != NULL) {
		for (i = 0; i < _sceUserModuleListSize; i++) {
			int result = sceKernelLoadStartModule(_sceUserModuleList[i], 0, NULL, 0,
							     NULL, NULL);

			if (result < 0) {
				sceClibPrintf("Preload module failed 0x%08x : %s\n", result,
					      _sceUserModuleList[i]);
				abort();
			}
		}
	}

	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; init++) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; init++) {
		if (*init != NULL)
			(*init)();
	}

	exit(main(__argc, __argv));
}
