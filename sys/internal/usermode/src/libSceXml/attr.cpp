/*
 * attr.cpp: sce::Xml::Attr and AttrImpl (0x810002F4-0x810005A7).
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{

Attr::Attr()
	: impl(NULL)
{
}

int Attr::initialize(const Initializer *init)
{
	if (impl != NULL)
		return SCE_XML_INFO_MULTIPLE_INITIALIZED;
	if (init == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	InitializerImpl *initImpl = InitializerImpl::get(init);
	if (initImpl == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	void *mem = initImpl->param.allocator->allocate(sizeof(AttrImpl), initImpl->param.userData);
	if (mem == NULL)
		return SCE_XML_ERROR_NO_MEMORY;

	impl = new (mem) AttrImpl(init);
	int ret = impl->initializeCore();
	if (ret != SCE_XML_SUCCESS)
		initImpl->param.allocator->deallocate(mem, initImpl->param.userData);
	return ret;
}

Attr::Attr(const Attr &src)
	: impl(NULL)
{
	int ret = SCE_XML_SUCCESS;
	if (src.impl != NULL) {
		ret = initialize(src.impl->initializer);
		if (ret == SCE_XML_SUCCESS)
			ret = impl->cloneCore(*src.impl);
	}
	if (impl != NULL)
		impl->status = ret;
}

int Attr::terminate()
{
	if (impl == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	const InitParameter *param = impl->getInitParam();
	impl->terminateCore();
	param->allocator->deallocate(impl, param->userData);
	impl = NULL;
	return SCE_XML_SUCCESS;
}

Attr::~Attr()
{
	if (impl != NULL) {
		const InitParameter *param = impl->getInitParam();
		impl->terminateCore();
		param->allocator->deallocate(impl, param->userData);
		impl = NULL;
	}
}

Attr &Attr::operator=(const Attr &src)
{
	if (&src != this) {
		if (src.impl != NULL) {
			if (impl == NULL) {
				if (initialize(src.impl->initializer) != SCE_XML_SUCCESS)
					return *this;
			}
			impl->cloneCore(*src.impl);
			return *this;
		}
		if (impl != NULL) {
			const InitParameter *param = impl->getInitParam();
			impl->terminateCore();
			param->allocator->deallocate(impl, param->userData);
			impl = NULL;
		}
	}
	return *this;
}

String Attr::getName() const
{
	if (impl != NULL)
		return impl->getName();
	return String();
}

String Attr::getValue() const
{
	if (impl != NULL)
		return impl->getValue();
	return String();
}

int Attr::setName(const String *str)
{
	if (str == NULL || str->buffer == NULL || str->size == 0)
		return SCE_XML_ERROR_INVALID_ARGUMENT;
	if (impl != NULL)
		return impl->setName(str);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

int Attr::setValue(const String *str)
{
	if (str == NULL || str->buffer == NULL || str->size == 0)
		return SCE_XML_ERROR_INVALID_ARGUMENT;
	if (impl != NULL)
		return impl->setValue(str);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

bool Attr::isAvailable() const
{
	if (impl != NULL)
		return impl->isCoreAvailable();
	return false;
}

} /* namespace Xml */
} /* namespace sce */
