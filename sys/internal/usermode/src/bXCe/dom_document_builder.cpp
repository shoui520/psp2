/*
 * ScebXCe's DocumentBuilder: the C++ interface object of the DOM builder, over a
 * DocumentBuilderImpl. It holds a reference on the library while it has one.
 */
#include "bxce_internal.h"

namespace bXCe {
namespace Dom {

DocumentBuilder::DocumentBuilder() : impl(NULL)
{
	if (bxceLibraryAddRef() < 0)
		return;
	impl = new DocumentBuilderImpl(0);
}

DocumentBuilder::~DocumentBuilder()
{
	if (impl != NULL) {
		delete impl;
		impl = NULL;
		bxceLibraryRelease();
	}
}

int DocumentBuilder::initialize()
{
	if (impl == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return impl->initialize();
}

int DocumentBuilder::terminate()
{
	if (impl == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return impl->terminate();
}

int DocumentBuilder::parse(const char *input, int size, bool isFinal)
{
	if (impl == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return impl->parse(input, size, isFinal);
}

DocumentImpl *DocumentBuilder::getDocument()
{
	if (impl == NULL)
		return NULL;
	return impl->getDocument();
}

void DocumentBuilder::setSkipIgnorableWhiteSpace(bool isSkipped)
{
	if (impl != NULL)
		impl->setSkipIgnorableWhiteSpace(isSkipped);
}

void DocumentBuilder::setResolveEntity(bool isResolved)
{
	if (impl != NULL)
		impl->setResolveEntity(isResolved);
}

int DocumentBuilder::setSkipIgnorableText(bool isSkipped)
{
	if (impl == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return impl->setSkipIgnorableText(isSkipped);
}

} /* namespace Dom */
} /* namespace bXCe */
