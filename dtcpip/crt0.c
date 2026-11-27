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
extern int __cxa_atexit(void (*func)(void *), void *arg, void *dso);
extern int __at_quick_exit(void (*func)(void), void *arg, void *dso);
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

#define SCE_PROCESS_PARAM_MAGIC		0x32505350	/* "PSP2" */
#define SCE_PROCESS_PARAM_VERSION	6
#define SCE_LIBC_PARAM_VERSION		9
#define SCE_LIBC_HEAP_SIZE_DEFAULT	0x40000
#if FW_VERSION == 0x03630011
#define SWAGNER_CRT0_SDK_VERSION	0x03100000
#else
#define SWAGNER_CRT0_SDK_VERSION	0x03650000
#endif

struct SwagnerLibcMallocReplace {
	SceSize size;
	SceUInt32 version;
	void *mallocInit;
	void *mallocFinalize;
	void *malloc;
	void *free;
	void *calloc;
	void *realloc;
	void *memalign;
	void *reallocalign;
	void *mallocStats;
	void *mallocStatsFast;
	void *mallocUsableSize;
};

struct SwagnerLibcNewReplace {
	SceSize size;
	SceUInt32 version;
	void *operatorNew;
	void *operatorNewNothrow;
	void *operatorNewArray;
	void *operatorNewArrayNothrow;
	void *operatorDelete;
	void *operatorDeleteNothrow;
	void *operatorDeleteArray;
	void *operatorDeleteArrayNothrow;
};

struct SwagnerLibcMallocReplaceForTls {
	SceSize size;
	SceUInt32 version;
	void *mallocInitForTls;
	void *mallocFinalizeForTls;
	void *mallocForTls;
	void *freeForTls;
};

struct SwagnerLibcParam {
	SceSize size;
	SceUInt32 unusedPad;
	const unsigned int *heapSize;
	const unsigned int *heapSizeDefault;
	const unsigned int *heapExtendedAlloc;
	const unsigned int *heapDelayedAlloc;
	SceUInt32 sdkVersion;
	SceUInt32 version;
	const struct SwagnerLibcMallocReplace *mallocReplace;
	const struct SwagnerLibcNewReplace *newReplace;
	const unsigned int *heapInitialSize;
	const unsigned int *heapUnitSize1MiB;
	const unsigned int *heapDetectOverrun;
	const struct SwagnerLibcMallocReplaceForTls *mallocReplaceForTls;
};

struct SwagnerProcessParam {
	SceSize size;
	SceUInt32 magic;
	SceUInt32 version;
	SceUInt32 sdkVersion;
	const char *mainThreadName;
	const int *mainThreadPriority;
	const unsigned int *mainThreadStackSize;
	const unsigned int *mainThreadAttribute;
	const char *processName;
	const unsigned int *processPreloadDisabled;
	const int *mainThreadCpuAffinityMask;
	const struct SwagnerLibcParam *libcParam;
	const void *mainThreadOptParam;
};

extern const unsigned int sce_process_preload_disabled __attribute__((weak));
extern const unsigned int sceLibcHeapSize __attribute__((weak));

static unsigned int g_swagnerHeapSizeDefault = SCE_LIBC_HEAP_SIZE_DEFAULT;

static const struct SwagnerLibcMallocReplace g_swagnerLibcMallocReplace = {
	sizeof(struct SwagnerLibcMallocReplace), 1,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

static const struct SwagnerLibcNewReplace g_swagnerLibcNewReplace = {
	sizeof(struct SwagnerLibcNewReplace), 1,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

static const struct SwagnerLibcMallocReplaceForTls g_swagnerLibcMallocReplaceForTls = {
	sizeof(struct SwagnerLibcMallocReplaceForTls), 1,
	NULL, NULL, NULL, NULL,
};

static const struct SwagnerLibcParam g_swagnerLibcParam = {
	sizeof(struct SwagnerLibcParam), 0,
	&sceLibcHeapSize, &g_swagnerHeapSizeDefault, NULL, NULL,
	SWAGNER_CRT0_SDK_VERSION, SCE_LIBC_PARAM_VERSION,
	&g_swagnerLibcMallocReplace, &g_swagnerLibcNewReplace,
	NULL, NULL, NULL, &g_swagnerLibcMallocReplaceForTls,
};

struct SwagnerProcessParam sceProcessParam = {
	sizeof(struct SwagnerProcessParam), SCE_PROCESS_PARAM_MAGIC, SCE_PROCESS_PARAM_VERSION,
	SWAGNER_CRT0_SDK_VERSION,
	NULL, NULL, NULL, NULL, NULL,
	&sce_process_preload_disabled, NULL, &g_swagnerLibcParam, NULL,
};

int main(int argc, char *argv[]);

void _start(SceSize args, char *argp) __attribute__((noreturn));

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

int _stop(void)
{
	return 0;
}

int atexit(void (*func)(void))
{
	return __cxa_atexit((void (*)(void *))func, NULL, &__dso_handle);
}

#if FW_VERSION != 0x03630011
int at_quick_exit(void (*func)(void))
{
	return __at_quick_exit(func, NULL, &__dso_handle);
}
#endif

int getargc(void)
{
	return __argc;
}

char **getargv(void)
{
	return __argv;
}

void module_start(SceSize arglen, const void *argp)
{
	_start(arglen, (char *)argp);
}
