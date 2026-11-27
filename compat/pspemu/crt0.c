/*
 * ScePspemu C runtime startup (GCC 4.8 CRT0 for vs0:app/NPXS10028/eboot.bin).
 * Registers the DSO handle and TLS module metadata, splits the argument block
 * into __argc/__argv, loads any preloaded user modules, runs preinit/init
 * constructor arrays, calls main(), and exits with main's return status.
 */

#include <stddef.h>
#include <scetypes.h>
#include <stdlib.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>

#define PSPEMU_ARGV_MAX			31
#define SCE_PROCESS_PARAM_MAGIC		0x32505350U	/* "PSP2" */
#define SCE_PROCESS_PARAM_VERSION	6U
#define SCE_LIBC_PARAM_VERSION		9U
#define SCE_LIBC_HEAP_SIZE_DEFAULT	0x40000U

#if FW_VERSION == 0x03630011
#define PSPEMU_CRT0_SDK_VERSION		0x03600000U
#else
#define PSPEMU_CRT0_SDK_VERSION		0x03650000U
#endif

extern void __cxa_set_dso_handle_main(void *handle);
extern int __cxa_atexit(void (*func)(void *), void *arg, void *dso);
extern int __at_quick_exit(void (*func)(void), void *arg, void *dso);
extern int _sceLdTlsRegisterModuleInfo(void *dso, void *moduleInfo, int flags);
extern int __sce_aeabi_idiv0(int value);
extern long long __sce_aeabi_ldiv0(long long value);
void abort(void) __attribute__((noreturn));

extern void (*__start__Zpreinit_array[])(void) __attribute__((weak));
extern void (*__stop__Zpreinit_array[])(void) __attribute__((weak));
extern void (*__start__Zinit_array[])(void) __attribute__((weak));
extern void (*__stop__Zinit_array[])(void) __attribute__((weak));
extern const char *const _sceUserModuleList[] __attribute__((weak));
extern const unsigned int _sceUserModuleListSize __attribute__((weak));
extern const unsigned int sceLibcHeapSize __attribute__((weak));

int __argc;
char *__argv[PSPEMU_ARGV_MAX + 1];

void *__dso_handle;
void *g_pspemuTlsModuleInfo;

struct SceLibcMallocReplace {
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

struct SceLibcNewReplace {
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

struct SceLibcMallocReplaceForTls {
	SceSize size;
	SceUInt32 version;
	void *mallocInitForTls;
	void *mallocFinalizeForTls;
	void *mallocForTls;
	void *freeForTls;
};

struct SceLibcParam {
	SceSize size;
	SceUInt32 reserved;
	const unsigned int *heapSize;
	const unsigned int *heapSizeDefault;
	const unsigned int *heapExtendedAlloc;
	const unsigned int *heapDelayedAlloc;
	SceUInt32 sdkVersion;
	SceUInt32 version;
	const struct SceLibcMallocReplace *mallocReplace;
	const struct SceLibcNewReplace *newReplace;
	const unsigned int *heapInitialSize;
	const unsigned int *heapUnitSize1MiB;
	const unsigned int *heapDetectOverrun;
	const struct SceLibcMallocReplaceForTls *mallocReplaceForTls;
};

struct SceProcessParam {
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
	const struct SceLibcParam *libcParam;
	const void *mainThreadOptParam;
};

static unsigned int g_pspemuHeapSizeDefault = SCE_LIBC_HEAP_SIZE_DEFAULT;

static const struct SceLibcMallocReplace g_pspemuLibcMallocReplace = {
	sizeof(struct SceLibcMallocReplace), 1,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

static const struct SceLibcNewReplace g_pspemuLibcNewReplace = {
	sizeof(struct SceLibcNewReplace), 1,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

static const struct SceLibcMallocReplaceForTls g_pspemuLibcMallocReplaceForTls = {
	sizeof(struct SceLibcMallocReplaceForTls), 1,
	NULL, NULL, NULL, NULL,
};

static const struct SceLibcParam g_pspemuLibcParam = {
	sizeof(struct SceLibcParam), 0,
	&sceLibcHeapSize, &g_pspemuHeapSizeDefault, NULL, NULL,
	PSPEMU_CRT0_SDK_VERSION, SCE_LIBC_PARAM_VERSION,
	&g_pspemuLibcMallocReplace, &g_pspemuLibcNewReplace,
	NULL, NULL, NULL, &g_pspemuLibcMallocReplaceForTls,
};

struct SceProcessParam sceProcessParam = {
	sizeof(struct SceProcessParam), SCE_PROCESS_PARAM_MAGIC, SCE_PROCESS_PARAM_VERSION,
	PSPEMU_CRT0_SDK_VERSION,
	NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, &g_pspemuLibcParam, NULL,
};

int main(int argc, char *argv[]);

void _start(SceSize args, char *argp) __attribute__((noreturn));

void module_start(SceSize arglen, const void *argp) __attribute__((noreturn));

void module_start(SceSize arglen, const void *argp)
{
	_start(arglen, (char *)argp);
}

void _start(SceSize args, char *argp)
{
	void (**init)(void);
	char *arg;
	unsigned int idx;
	int argc;
	int length;

	__dso_handle = &__dso_handle;
	__cxa_set_dso_handle_main(&__dso_handle);
	if (g_pspemuTlsModuleInfo != NULL &&
	    _sceLdTlsRegisterModuleInfo(__dso_handle, g_pspemuTlsModuleInfo, 1) != 0)
		abort();

	arg = argp;
	for (argc = 0; ; argc++) {
		__argc = argc;
		if (argc >= PSPEMU_ARGV_MAX || arg - argp >= (int)args)
			break;
		__argv[argc] = arg;
		for (length = 0; arg[length] != '\0'; length++)
			;
		arg = arg + length + 1;
	}
	__argv[argc] = NULL;

	if (_sceUserModuleList != NULL) {
		for (idx = 0; idx < _sceUserModuleListSize; idx++) {
			int result = sceKernelLoadStartModule(_sceUserModuleList[idx], 0, NULL, 0,
							      NULL, NULL);

			if (result < 0) {
				sceClibPrintf("Preload module failed 0x%08x : %s\n", result,
					      _sceUserModuleList[idx]);
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

int at_quick_exit(void (*func)(void))
{
	return __at_quick_exit(func, NULL, &__dso_handle);
}

int __aeabi_idiv0(int value)
{
	return __sce_aeabi_idiv0(value);
}

long long __aeabi_ldiv0(long long value)
{
	return __sce_aeabi_ldiv0(value);
}

int getargc(void)
{
	return __argc;
}

char **getargv(void)
{
	return __argv;
}
