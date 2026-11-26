/*
 * attribute_list.cpp: sce::Xml::AttributeList and AttributeListImpl
 * (0x810005A8-0x810009B3).
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{

AttributeListImpl *AttributeListImpl::create(const Initializer *init)
{
	InitializerImpl *initImpl = InitializerImpl::get(init);
	if (initImpl == NULL)
		return NULL;

	void *mem = initImpl->param.allocator->allocate(sizeof(AttributeListImpl),
							initImpl->param.userData);
	if (mem == NULL)
		return NULL;

	return new (mem) AttributeListImpl(init);
}

AttributeList::AttributeList()
	: impl(NULL)
{
}

AttributeList::AttributeList(const AttributeList &src)
	: impl(NULL)
{
	impl = AttributeListImpl::create(src.impl->initializer);
	if (impl != NULL)
		impl->copyFrom(src.impl);
}

int AttributeList::terminate()
{
	if (impl != NULL) {
		const InitParameter *param = impl->getInitParam();
		if (param != NULL) {
			impl->~AttributeListImpl();
			param->allocator->deallocate(impl, param->userData);
			impl = NULL;
		}
	}
	return SCE_XML_SUCCESS;
}

AttributeList::~AttributeList()
{
	if (impl != NULL) {
		impl->terminate();
		terminate();
	}
}

int AttributeList::initialize(const Initializer *init)
{
	if (impl != NULL)
		return SCE_XML_INFO_MULTIPLE_INITIALIZED;

	impl = AttributeListImpl::create(init);
	if (impl == NULL)
		return SCE_XML_ERROR_NO_MEMORY;

	int ret = impl->initialize(init);
	if (ret != SCE_XML_SUCCESS)
		terminate();
	return ret;
}

Attr AttributeList::getAttribute(const String *attName) const
{
	if (impl != NULL)
		return impl->getAttribute(attName);
	return Attr();
}

Attr AttributeList::getAttribute(index_t index) const
{
	if (impl != NULL)
		return impl->getAttribute(index);
	return Attr();
}

size_t AttributeList::getLength() const
{
	if (impl != NULL)
		return impl->getLength();
	return 0;
}

int AttributeList::addAttribute(const String *name, const String *value)
{
	if (impl != NULL)
		return impl->addAttribute(name, value);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

void AttributeList::clear()
{
	if (impl != NULL)
		impl->clear();
}

bool AttributeList::isAvailable() const
{
	if (impl != NULL)
		return impl->isAvailable();
	return false;
}

} /* namespace Xml */
} /* namespace sce */
