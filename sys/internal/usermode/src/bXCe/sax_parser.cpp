/*
 * ScebXCe's Sax::Parser: the C++ interface object of a SAX parser, over a ParserImpl. It holds
 * a reference to the library while it is initialized.
 */
#include "bxce_internal.h"

namespace bXCe {
namespace Sax {

Parser::Parser() : impl(NULL)
{
}

int Parser::terminate()
{
	int ret;

	if (impl == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;

	ret = impl->terminate();
	delete impl;
	impl = NULL;
	bxceLibraryRelease();
	return ret;
}

Parser::~Parser()
{
	if (impl != NULL)
		terminate();
}

int Parser::initialize(const InitParameter *param)
{
	int ret;

	delete impl;

	ret = bxceLibraryAddRef();
	if (ret < 0)
		return ret;

	impl = new ParserImpl();
	if (impl == NULL)
		return SCE_BXCE_RESULT_NO_MEMORY;
	return impl->initialize(param);
}

int Parser::setDocumentHandler(DocumentHandler *handler)
{
	if (impl == NULL)
		return 0;
	return impl->setDocumentHandler(handler);
}

/* the handler's characters come with their offset in the document */
int Parser::setDocumentHandlerWithOffset(DocumentHandler *handler)
{
	if (impl == NULL)
		return 0;
	return impl->setDocumentHandlerWithOffset(handler);
}

void Parser::setUserData(void *data)
{
	if (impl != NULL)
		impl->setUserData(data);
}

int Parser::parse(const char *input, int size, bool isFinal)
{
	if (impl == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return impl->parse(input, size, isFinal);
}

int Parser::reset()
{
	if (impl == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return impl->reset();
}

/* the slot the parse is saved in, -1 without a parser */
int Parser::save()
{
	if (impl == NULL)
		return -1;
	return impl->save();
}

int Parser::restore(int slot)
{
	if (impl == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return impl->restore(slot);
}

void Parser::setSkipIgnorableWhiteSpace(bool isSkipped)
{
	if (impl != NULL)
		impl->setSkipIgnorableWhiteSpace(isSkipped);
}

void Parser::setResolveEntity(bool isResolved)
{
	if (impl != NULL)
		impl->setResolveEntity(isResolved);
}

void Parser::setResolveAttributeEntity(bool isResolved)
{
	if (impl != NULL)
		impl->setResolveAttributeEntity(isResolved);
}

} /* namespace Sax */
} /* namespace bXCe */
