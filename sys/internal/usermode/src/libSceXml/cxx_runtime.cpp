/*
 * cxx_runtime.cpp: module-local C++ allocation operators and pure virtual trap
 * routing through bXCe's default allocator (0x810039B4-0x810039D7).
 */
#include "libSceXml_internal.h"

static __attribute__((noinline)) void *libXmlAlloc(size_t size)
{
	return bxceDefaultAllocate(size);
}

static __attribute__((noinline)) void libXmlFree(void *ptr)
{
	bxceDefaultDeallocate(ptr);
}

void *operator new(size_t size)
{
	return libXmlAlloc(size);
}

void operator delete(void *ptr)
{
	libXmlFree(ptr);
}

extern "C" void __cxa_pure_virtual(void)
{
	__builtin_trap();
}
