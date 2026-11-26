/*
 * initializer.cpp: sce::Xml::Initializer and InitializerUser (0x81002010-0x81002247).
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{

namespace
{

/* Bridges sce::Xml::MemAllocator into bXCe::MemAllocator (8 bytes) */
class MemAllocatorAdapter : public bXCe::MemAllocator
{
public:
	MemAllocatorAdapter(sce::Xml::MemAllocator *alloc) : allocator(alloc) {}
	virtual ~MemAllocatorAdapter() {}

	virtual void *allocate(SceSize size, void *userData)
	{
		return allocator->allocate((size_t)size, userData);
	}

	virtual void deallocate(void *ptr, void *userData)
	{
		allocator->deallocate(ptr, userData);
	}

	sce::Xml::MemAllocator *allocator;
};

int s_initRefCount = 0;
MemAllocatorAdapter *s_allocatorAdapter = NULL;

} /* namespace */

Initializer::Initializer()
	: impl(NULL)
{
}

int Initializer::terminate()
{
	if (impl == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	bxceLibraryRelease();
	if (s_initRefCount > 0)
		--s_initRefCount;

	if (s_allocatorAdapter != NULL) {
		impl->param.allocator->deallocate(s_allocatorAdapter, impl->param.userData);
		s_allocatorAdapter = NULL;
	}

	impl->param.allocator->deallocate(impl, impl->param.userData);
	impl = NULL;
	return SCE_XML_SUCCESS;
}

Initializer::~Initializer()
{
	if (impl != NULL)
		terminate();
}

int Initializer::initialize(const InitParameter *initParam)
{
	if (impl != NULL)
		return SCE_XML_INFO_MULTIPLE_INITIALIZED;
	if (initParam == NULL || initParam->allocator == NULL)
		return SCE_XML_ERROR_INVALID_ARGUMENT;
	if (s_initRefCount > 0)
		return SCE_XML_INFO_MULTIPLE_INITIALIZED;

	void *implMem = initParam->allocator->allocate(sizeof(InitializerImpl), initParam->userData);
	if (implMem == NULL)
		return SCE_XML_ERROR_NO_MEMORY;

	InitializerImpl *newImpl = new (implMem) InitializerImpl();
	newImpl->param = *initParam;
	impl = newImpl;

	void *adapterMem = initParam->allocator->allocate(sizeof(MemAllocatorAdapter),
							  initParam->userData);
	if (adapterMem == NULL) {
		terminate();
		return SCE_XML_ERROR_NO_MEMORY;
	}

	s_allocatorAdapter = new (adapterMem) MemAllocatorAdapter(initParam->allocator);
	ScebXCe_EB4E6C8D(s_allocatorAdapter, initParam->userData);
	int ret = bxceLibraryAddRef();
	if (ret >= 0)
		++s_initRefCount;
	return ret;
}

const InitParameter *InitializerUser::getInitParam() const
{
	if (initializer == NULL)
		return NULL;
	return &InitializerImpl::get(initializer)->param;
}

} /* namespace Xml */
} /* namespace sce */
