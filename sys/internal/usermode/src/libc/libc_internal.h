/*
 * libc_internal.h: SceLibc's functions that its files share and that no SDK header declares.
 * The names are ours where Dinkumware's headers give none.
 */
#ifndef _LIBC_INTERNAL_H
#define _LIBC_INTERNAL_H

#include <stddef.h>

/* module.c */
extern void *__dso_handle;
extern int _ExitInProgress;

/* the process parameters the program's crt0 gives (sceKernelGetProcessParam) */
struct SceLibcParam;
typedef struct SceProcessParam {
	unsigned int size;
	unsigned int magic;			/* "PSP2" */
	unsigned int version;
	unsigned int sdkVersion;
	const char *sceUserMainThreadName;
	int *sceUserMainThreadPriority;
	unsigned int *sceUserMainThreadStackSize;
	unsigned int *sceUserMainThreadAttribute;
	const char *sceProcessName;
	unsigned int *sceKernelPreloadModuleInhibit;
	int *sceUserMainThreadCpuAffinityMask;
	const struct SceLibcParam *sceLibcParam;	/* version 5 and later */
} SceProcessParam;

/* the program's replacement of the C library's allocator (user_malloc_init ...) */
typedef struct SceLibcMallocReplace {
	unsigned int size;
	unsigned int version;
	void (*user_malloc_init)(void);
	void (*user_malloc_finalize)(void);
	void *(*user_malloc)(size_t size);
	void (*user_free)(void *ptr);
	void *(*user_calloc)(size_t nelem, size_t size);
	void *(*user_realloc)(void *ptr, size_t size);
	void *(*user_memalign)(size_t boundary, size_t size);
	void *(*user_reallocalign)(void *ptr, size_t size, size_t boundary);
	int (*user_malloc_stats)(struct malloc_managed_size *mmsize);
	int (*user_malloc_stats_fast)(struct malloc_managed_size *mmsize);
	size_t (*user_malloc_usable_size)(void *ptr);
} SceLibcMallocReplace;

/* the program's replacement of the TLS blocks' allocator (user_malloc_for_tls_init ...) */
typedef struct SceLibcMallocReplaceForTls {
	unsigned int size;
	unsigned int version;
	void (*user_malloc_for_tls_init)(void);
	void (*user_malloc_for_tls_finalize)(void);
	void *(*user_malloc_for_tls)(size_t size);
	void (*user_free_for_tls)(void *ptr);
} SceLibcMallocReplaceForTls;

/* the C library's parameters (__sce_libcparam) */
typedef struct SceLibcParam {
	unsigned int size;
	unsigned int version;
	unsigned int *sceLibcHeapSize;
	unsigned int *sceLibcHeapSizeDefault;
	unsigned int *sceLibcHeapExtendedAlloc;
	unsigned int *sceLibcHeapDelayedAlloc;
	unsigned int sdkVersion;
	unsigned int reserved;
	const SceLibcMallocReplace *_sceLibcMallocReplace;	/* size 0x28 and later */
	const void *_sceLibcNewReplace;
	unsigned int *sceLibcHeapInitialSize;			/* size 0x30 and later */
	unsigned int *sceLibcHeapUnitSize1MiB;
	unsigned int *sceLibcHeapDetectOverrun;		/* size 0x34 and later */
	const SceLibcMallocReplaceForTls *_sceLibcMallocReplaceForTls;	/* size 0x38 and later */
} SceLibcParam;

/* the heap (malloc.c) */
int _MallocInit(void);
int _MallocTerm(void);

/* the default heap's dlmalloc state: set up, and its lock deleted */
int _HeapInit(void *msp);
void _HeapTerm(void);

/* C++: take the program's operator new replacement (user_new ...) */
int _InitNewReplace(const void *newReplace) __attribute__((weak));

/* the library's locks */
int _InitLocks(void);
int _TermLocks(void);
int _Guardbroadcast(int num);
int _Guardwait(int num);

/* the thread-local storage lock */
int _InitTlsMutex(void);
int _TermTlsMutex(void);

/*
 * the allocator of the TLS blocks: the program's user_malloc_for_tls and user_free_for_tls
 * when it replaces them, else malloc and free (set when the heap starts)
 */
extern void *(*_MallocForTls)(size_t size);
extern void (*_FreeForTls)(void *ptr);

/* the standard streams */
void _InitStdFiles(void);
void _Fcloseall(int exiting);

/* C++ termination */
int __cxa_atexit(void (*func)(void *), void *arg, void *dso);
void __cxa_finalize(void *dso);

/* the thread-local data of the C library (sceKernelGetTLSAddr slots) */
#include <wchar.h>
mbstate_t *_Getmbrlenstate(void);
mbstate_t *_Getmbrtowcstate(void);
mbstate_t *_Getmbsrtowcsstate(void);
mbstate_t *_Getmbtowcstate(void);
mbstate_t *_Getwcrtombstate(void);
mbstate_t *_Getwcsrtombsstate(void);
mbstate_t *_Getwctombstate(void);
mbstate_t *_Getwctomb_sstate(void);
char *_Getstrerrorbuf(void);
struct tm *_Gettmbuf(void);
char **_Getstrtokptr(void);
unsigned int _Getjmpbufguard(void);
unsigned int _Getptrguard(void);

extern unsigned long long _Randseed;

/* the reasons the C library gives sceKernelCallAbortHandler */
#define SCE_LIBC_EXIT_SUCCESS_CODE	0x102	/* exit(0) */
#define SCE_LIBC_EXIT_FAILURE_CODE	0x103	/* exit(nonzero) */
#define SCE_LIBC__EXIT_SUCCESS_CODE	0x104	/* _Exit(0) */
#define SCE_LIBC__EXIT_FAILURE_CODE	0x105	/* _Exit(nonzero) */
#define SCE_LIBC_ASSERT_CODE		0x107	/* a failed assertion */
void *_Gettlsbuf(size_t size);

/* exit's DSO: __cxa_finalize's argument (__cxa_set_dso_handle_main) */
extern void *_Dso_handle_main;

/* the message for an error code, in buf or a static buffer when buf is NULL */
char *_Strerror(int errcode, char *buf);


/* the lightweight conditions of the C++ static guards (xmtx.c) */
struct _SceKernelLwCondWork;
struct _SceLibcMutexType;
int _Cndinit(struct _SceKernelLwCondWork *cnd, const char *name, struct _SceLibcMutexType *mtx);
int _Cnddst(struct _SceKernelLwCondWork *cnd);
int _Cndwait(struct _SceKernelLwCondWork *cnd);
int _Cndbroadcast(struct _SceKernelLwCondWork *cnd);

#endif /* _LIBC_INTERNAL_H */
