#include "be_mp2sys_internal.h"

static BeAllocFunc g_allocFunc = 0;
static BeReallocFunc g_reallocFunc = 0;
static BeFreeFunc g_freeFunc = 0;

void *beMalloc(SceSize size)
{
	return g_allocFunc(size);
}

void *operator new(SceSize size)
{
	return g_allocFunc(size);
}

void *operator new[](SceSize size)
{
	return g_allocFunc(size);
}

void beFree(void *ptr)
{
	g_freeFunc(ptr);
}

void operator delete(void *ptr)
{
	g_freeFunc(ptr);
}

void operator delete[](void *ptr)
{
	g_freeFunc(ptr);
}

BeMemObject::BeMemObject()
{
}

BeMemObject::~BeMemObject()
{
}

void *BeMemObject::operator new(SceSize size)
{
	return g_allocFunc(size);
}

void *BeMemObject::operator new[](SceSize size)
{
	return g_allocFunc(size);
}

void BeMemObject::operator delete(void *ptr)
{
	g_freeFunc(ptr);
}

void BeMemObject::operator delete[](void *ptr)
{
	g_freeFunc(ptr);
}

void beSetAllocator(BeAllocFunc allocFn, BeReallocFunc reallocFn, BeFreeFunc freeFn)
{
	g_allocFunc = allocFn;
	g_freeFunc = freeFn;
	g_reallocFunc = reallocFn;
}

void *beRealloc(void *ptr, SceSize size)
{
	return g_reallocFunc(ptr, size);
}
