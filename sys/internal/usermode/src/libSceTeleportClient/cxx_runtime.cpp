/*
 * cxx_runtime.cpp: module-local libc/C++ runtime shims for SceTeleportClient
 * (0x810049AE-0x81004A83).
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>

struct AtexitEntry {
	struct AtexitEntry *next;
	void (*func)(void *);
	void *arg;
	void *dsoHandle;
};

static SceKernelLwMutexWork s_atexitMutex;
void *g_mspace;
static AtexitEntry *s_atexitHead = NULL;
static AtexitEntry s_atexitPool[128];

extern "C" {

void *__dso_handle = NULL;

__attribute__((noinline))
void *malloc(size_t size)
{
	return sceClibMspaceMalloc(g_mspace, size);
}

__attribute__((noinline))
void free(void *ptr)
{
	sceClibMspaceFree(g_mspace, ptr);
}

void *memcpy(void *dst, const void *src, size_t n)
{
	return sceClibMemcpy_safe(dst, src, n);
}

void *memset(void *dst, int ch, size_t n)
{
	return sceClibMemset(dst, ch, n);
}

int __aeabi_atexit(void *arg, void (*func)(void *), void *dsoHandle)
{
	sceKernelLockLwMutex(&s_atexitMutex, 1, NULL);
	AtexitEntry *slot = NULL;
	for (int i = 0; i < 128; ++i) {
		if (s_atexitPool[i].func == NULL) {
			slot = &s_atexitPool[i];
			break;
		}
	}
	if (slot == NULL) {
		slot = (AtexitEntry *)malloc(sizeof(AtexitEntry));
		if (slot == NULL) {
			sceKernelLockLwMutex(&s_atexitMutex, 1, NULL);
			return 1;
		}
	}
	void *dso = *(void **)dsoHandle;
	slot->func = func;
	slot->next = s_atexitHead;
	slot->arg = arg;
	slot->dsoHandle = dso;
	s_atexitHead = slot;
	sceKernelUnlockLwMutex(&s_atexitMutex, 1);
	return 0;
}

void __cxa_pure_virtual(void)
{
	__builtin_trap();
}

} /* extern "C" */

void *operator new(size_t size)
{
	return malloc(size);
}

void operator delete(void *ptr)
{
	free(ptr);
}
