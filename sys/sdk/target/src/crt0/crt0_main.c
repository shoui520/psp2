/*
 * The SDK's start code of a program (crt0_main_snc.o, the SDK's own object name; the folder and
 * file names are ours), as the 0.996 SDK built it, linked into cui_setupper: _start splits the
 * argument block into __argc and __argv, runs the constructors and calls main, then exit with
 * main's result. The data are the process and C library parameters the module exports
 * (sceProcessParam), each field the address of the program's own setting, or 0 where the
 * program does not define it (the settings are weak).
 */
#include <stddef.h>
#include <scetypes.h>

#define SCE_CRT0_ARGV_MAX		32
#define SCE_PROCESS_PARAM_MAGIC		0x32505350	/* "PSP2" */
#define SCE_PROCESS_PARAM_VERSION	5
#define SCE_LIBC_PARAM_VERSION		5
#define SCE_CRT0_SDK_VERSION		0x00996070
#define SCE_LIBC_HEAP_SIZE_DEFAULT	0x40000

typedef void (*SceCrt0InitFunc)(void);

/* the process parameters (version 5) */
typedef struct SceProcessParam {
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
} SceProcessParam;

/* the program's replacements of the C library's allocator */
typedef struct SceLibcMallocReplace {
	SceSize size;
	SceUInt32 version;		/* 1 */
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
} SceLibcMallocReplace;

/* the program's replacements of C++'s operators new and delete */
typedef struct SceLibcNewReplace {
	SceSize size;
	SceUInt32 version;		/* 1 */
	void *operatorNew;
	void *operatorNewNothrow;
	void *operatorNewArray;
	void *operatorNewArrayNothrow;
	void *operatorDelete;
	void *operatorDeleteNothrow;
	void *operatorDeleteArray;
	void *operatorDeleteArrayNothrow;
} SceLibcNewReplace;

/* the C library parameters (version 5) */
typedef struct SceLibcParam {
	SceSize size;
	SceUInt32 reserved;		/* 0 */
	const unsigned int *heapSize;
	const unsigned int *heapSizeDefault;
	const unsigned int *heapExtendedAlloc;
	const unsigned int *heapDelayedAlloc;
	SceUInt32 sdkVersion;
	SceUInt32 version;
	const SceLibcMallocReplace *mallocReplace;
	const SceLibcNewReplace *newReplace;
} SceLibcParam;

/* the program's settings */
extern const char sceUserMainThreadName[] __attribute__((weak));
extern const int sceUserMainThreadPriority __attribute__((weak));
extern const unsigned int sceUserMainThreadStackSize __attribute__((weak));
extern const unsigned int sceUserMainThreadAttribute __attribute__((weak));
extern const char sceProcessName[] __attribute__((weak));
extern const unsigned int sce_process_preload_disabled __attribute__((weak));
extern const int sceUserMainThreadCpuAffinityMask __attribute__((weak));
extern const unsigned int sceLibcHeapSize __attribute__((weak));
extern const unsigned int sceLibcHeapExtendedAlloc __attribute__((weak));
extern const unsigned int sceLibcHeapDelayedAlloc __attribute__((weak));

void user_malloc_init(void) __attribute__((weak));
void user_malloc_finalize(void) __attribute__((weak));
void *user_malloc(size_t size) __attribute__((weak));
void user_free(void *ptr) __attribute__((weak));
void *user_calloc(size_t nelem, size_t size) __attribute__((weak));
void *user_realloc(void *ptr, size_t size) __attribute__((weak));
void *user_memalign(size_t boundary, size_t size) __attribute__((weak));
void *user_reallocalign(void *ptr, size_t size, size_t boundary) __attribute__((weak));
int user_malloc_stats(void *mmsize) __attribute__((weak));
int user_malloc_stats_fast(void *mmsize) __attribute__((weak));
size_t user_malloc_usable_size(void *ptr) __attribute__((weak));
void _Z8user_newj(void) __attribute__((weak));
void _Z8user_newjRKSt9nothrow_t(void) __attribute__((weak));
void _Z14user_new_arrayj(void) __attribute__((weak));
void _Z14user_new_arrayjRKSt9nothrow_t(void) __attribute__((weak));
void _Z11user_deletePv(void) __attribute__((weak));
void _Z11user_deletePvRKSt9nothrow_t(void) __attribute__((weak));
void _Z17user_delete_arrayPv(void) __attribute__((weak));
void _Z17user_delete_arrayPvRKSt9nothrow_t(void) __attribute__((weak));

/* the constructors, from the linker */
extern SceCrt0InitFunc __start__Zpreinit_array[] __attribute__((weak));
extern SceCrt0InitFunc __stop__Zpreinit_array[] __attribute__((weak));
extern SceCrt0InitFunc __start__Zinit_array[] __attribute__((weak));
extern SceCrt0InitFunc __stop__Zinit_array[] __attribute__((weak));

int main(int argc, char *argv[]) __attribute__((weak));
void exit(int status) __attribute__((weak));

void *__dso_handle;

int __argc = 0;
char *__argv[SCE_CRT0_ARGV_MAX] = { NULL };
unsigned int __sceLibcHeapSizeDefault = SCE_LIBC_HEAP_SIZE_DEFAULT;

static const SceLibcMallocReplace __sce_libcmallocreplace = {
	sizeof(SceLibcMallocReplace), 1,
	user_malloc_init, user_malloc_finalize, user_malloc, user_free, user_calloc,
	user_realloc, user_memalign, user_reallocalign, user_malloc_stats,
	user_malloc_stats_fast, user_malloc_usable_size,
};

static const SceLibcNewReplace __sce_libcnewreplace = {
	sizeof(SceLibcNewReplace), 1,
	_Z8user_newj, _Z8user_newjRKSt9nothrow_t, _Z14user_new_arrayj,
	_Z14user_new_arrayjRKSt9nothrow_t, _Z11user_deletePv, _Z11user_deletePvRKSt9nothrow_t,
	_Z17user_delete_arrayPv, _Z17user_delete_arrayPvRKSt9nothrow_t,
};

static const SceLibcParam __sce_libcparam = {
	sizeof(SceLibcParam), 0,
	&sceLibcHeapSize, &__sceLibcHeapSizeDefault, &sceLibcHeapExtendedAlloc,
	&sceLibcHeapDelayedAlloc, SCE_CRT0_SDK_VERSION, SCE_LIBC_PARAM_VERSION,
	&__sce_libcmallocreplace, &__sce_libcnewreplace,
};

SceProcessParam __sce_process_param = {
	sizeof(SceProcessParam), SCE_PROCESS_PARAM_MAGIC, SCE_PROCESS_PARAM_VERSION,
	SCE_CRT0_SDK_VERSION,
	sceUserMainThreadName, &sceUserMainThreadPriority, &sceUserMainThreadStackSize,
	&sceUserMainThreadAttribute, sceProcessName, &sce_process_preload_disabled,
	&sceUserMainThreadCpuAffinityMask, &__sce_libcparam,
};

void _initialize(SceSize args, char *argp);

void _start(SceSize args, char *argp)
{
	_initialize(args, argp);
}

void _initialize(SceSize args, char *argp)
{
	SceCrt0InitFunc *init;
	char *arg = argp;
	int argc, length;

	__dso_handle = &__dso_handle;
	__argc = 0;
	for (argc = 0; argc < SCE_CRT0_ARGV_MAX - 1; argc++) {
		if (arg - argp >= (int)args)
			break;
		__argv[argc] = arg;
		for (length = 0; arg[length] != '\0'; length++)
			;
		arg += length + 1;
	}
	__argc = argc;
	__argv[argc] = NULL;

	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; init++) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; init++) {
		if (*init != NULL)
			(*init)();
	}

	exit(main(__argc, __argv));
	for (;;)
		;
}

int _stop(void)
{
	return 0;
}

int getargc(void)
{
	return __argc;
}

char **getargv(void)
{
	return __argv;
}
