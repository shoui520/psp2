/*
 * swagner's start code, the GCC build's variant (not the SNC crt0 the shell links): the
 * DSO handle, the TLS module registration, the argument split (31 arguments at most),
 * the constructors from the linker's table, then main, and exit with its result.
 */

#include <stddef.h>
#include <scetypes.h>
#include <string.h>

#define SWAGNER_ARGV_MAX	31

extern void __cxa_set_dso_handle_main(void *handle);
extern int _sceLdTlsRegisterModuleInfo(void *dso, void *info, int flags);
extern void abort(void) __attribute__((noreturn));
extern void exit(int status) __attribute__((noreturn));
extern int main(int argc, char *argv[]) __attribute__((weak));

extern void (*const __start__Zinit_array[])(void) __attribute__((weak));
extern void (*const __stop__Zinit_array[])(void) __attribute__((weak));

/* the C library's per-program hooks */
void *__dso_handle;
void *g_tlsModuleInfo;
int __argc;
char *__argv[SWAGNER_ARGV_MAX + 1];

void _start(SceSize args, char *argp)
{
	void (**constructor)(void);
	char *argument;
	int count;
	int length;
	int argc;

	__dso_handle = &__dso_handle;
	__cxa_set_dso_handle_main(&__dso_handle);
	if (g_tlsModuleInfo != NULL && _sceLdTlsRegisterModuleInfo(__dso_handle, g_tlsModuleInfo, 1) != 0)
		abort();

	argc = 0;
	argument = argp;
	__argv[0] = NULL;
	for (count = 0;; count += 4) {
		__argc = argc;
		if (argc >= SWAGNER_ARGV_MAX || argument - argp >= (int)args)
			break;
		__argv[argc] = argument;
		length = 0;
		if (*argument != 0) {
			char *scan = argument;

			do {
				++scan;
				++length;
			} while (*scan != 0);
		}
		argument += length + 1;
		++argc;
	}
	*(char **)((char *)__argv + count) = NULL;

	for (constructor = (void (**)(void))__start__Zinit_array;
	     constructor < __stop__Zinit_array; ++constructor) {
		if (*constructor != NULL)
			(*constructor)();
	}
	exit(main(__argc, __argv));
}
