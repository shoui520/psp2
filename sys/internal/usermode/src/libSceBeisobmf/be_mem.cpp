/*
 * BEAV allocator-backed base object (BeMemObject).
 */

#include "be_isobmf_internal.h"

static BeAllocFunc g_allocFunc = NULL;
static BeReallocFunc g_reallocFunc = NULL;
static BeFreeFunc g_freeFunc = NULL;

void *BeMemObject::operator new[](size_t size)
{
	return g_allocFunc(size);
}

void BeMemObject::operator delete[](void *ptr)
{
	g_freeFunc(ptr);
}

BeMemObject::BeMemObject()
{
}

BeMemObject::~BeMemObject()
{
}

void BeMemObject::operator delete(void *ptr)
{
	g_freeFunc(ptr);
}

void *BeMemObject::operator new(size_t size)
{
	return g_allocFunc(size);
}

void BeMemObject::setAllocator(BeAllocFunc allocFn, BeReallocFunc reallocFn, BeFreeFunc freeFn)
{
	g_allocFunc = allocFn;
	g_reallocFunc = reallocFn;
	g_freeFunc = freeFn;
}
