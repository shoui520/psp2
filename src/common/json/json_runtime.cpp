/*
 * libSceJson: C++ runtime pieces.  new/delete go to the library heap
 * (sce::Json::Malloc/Free on the user's MemAllocator).
 */

#include "json_internal.h"

void *operator new(size_t size)
{
	return sce::Json::Malloc(size);
}

void operator delete(void *ptr)
{
	sce::Json::Free(ptr);
}

extern "C" void __cxa_pure_virtual(void)
{
	sceClibAbort();
}

namespace sce {
namespace Json {

/* Sony's copy is one "return 0" function shared (folded) by both uses. */
int jsonDebugHook(void)
{
	return 0;
}

/* Value::serialize(String &) callback: the text stays in the buffer. */
int jsonNoReceive(String &buf, void *userdata)
{
	return 0;
}

void *jsonMemcpy(void *dst, const void *src, size_t size)
{
	return sceClibMemcpy_safe(dst, src, size);
}

} /* namespace Json */
} /* namespace sce */
