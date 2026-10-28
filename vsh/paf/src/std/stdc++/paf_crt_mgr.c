#include "paf_crt_mgr.h"
#include "atexit.h"
#include "../string.h"
#include <libkernel/libkernel.h>
#include <stddef.h>

static PafCrtManager *crtManager;
static PafCrtManager crtManagerStorage;

static const char crtAssertion[] =
	"[PAF C/C++ Runtime Mgr] %s:%u: INTERNAL ASSERION FAILURE\n";

#if defined(FW_TYPE_CEX)
#define PAF_CRT_BUILD "J01509657"
#elif defined(FW_TYPE_TOOL)
#define PAF_CRT_BUILD "J01509658"
#else
#define PAF_CRT_BUILD "J01508051"
#endif

static const char crtSourceFile[] =
	"W:\\Build\\" PAF_CRT_BUILD "\\vsh\\paf\\src\\std\\stdc++\\paf_crt_mgr.cpp";

PafCrtManager *PafCrtManagerGet(void)
{
	return crtManager;
}

PafCrtManager *PafCrtManagerConstruct(PafCrtManager *self)
{
	char name[16];

	if (crtManager) {
		sceClibPrintf(crtAssertion, crtSourceFile, 36);
		__breakpoint(0);
	}
	ScePafStdc_5883A6E3(name, "ScePafCrtMutex0", sizeof(name));
	for (unsigned int i = 0; i < 2; i++) {
		if (sceKernelCreateLwMutex(&self->mutex[i], name,
					 SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO |
					 SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL)) {
			sceClibPrintf(crtAssertion, crtSourceFile, 48);
			__breakpoint(0);
		}
		name[14]++;
	}
	crtManager = self;
	return self;
}

PafCrtManager *PafCrtManagerDestruct(PafCrtManager *self)
{
	crtManager = NULL;
	for (unsigned int i = 0; i < 2; i++) {
		if (sceKernelDeleteLwMutex(&self->mutex[i])) {
			sceClibPrintf(crtAssertion, crtSourceFile, 65);
			__breakpoint(0);
		}
	}
	crtManager = NULL;
	return self;
}

int PafCrtManagerLock(PafCrtManager *self, unsigned int index)
{
	int result;

	if (index >= 2) {
		sceClibPrintf(crtAssertion, crtSourceFile, 75);
		__breakpoint(0);
	}
	result = sceKernelLockLwMutex(&self->mutex[index], 1, NULL);
	if (result) {
		result = sceClibPrintf(crtAssertion, crtSourceFile, 78);
		__breakpoint(0);
	}
	return result;
}

int PafCrtManagerUnlock(PafCrtManager *self, unsigned int index)
{
	int result;

	if (index >= 2) {
		sceClibPrintf(crtAssertion, crtSourceFile, 85);
		__breakpoint(0);
	}
	result = sceKernelUnlockLwMutex(&self->mutex[index], 1);
	if (result) {
		result = sceClibPrintf(crtAssertion, crtSourceFile, 88);
		__breakpoint(0);
	}
	return result;
}

int PafCrtManagerInitialize(void)
{
	extern void *__dso_handle;

	PafCrtManagerConstruct(&crtManagerStorage);
	return ScePafStdc_C95732BB(&crtManagerStorage,
			(void (*)(void *))PafCrtManagerDestruct, &__dso_handle);
}
