/*
 * sax_parser.cpp: sce::Xml::Sax::Parser, SaxParserCore, and DocumentHandlerAdapter
 * (0x81002FDC-0x810033AF, 0x8100C468-0x8100C68F).
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{
namespace Sax
{

Parser::Parser()
	: impl(NULL)
{
}

int Parser::terminate()
{
	if (impl == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	int ret = impl->core->terminate();
	InitializerImpl *initImpl = InitializerImpl::get(impl->initializer);
	initImpl->param.allocator->deallocate(impl->core, initImpl->param.userData);
	impl->core = NULL;
	initImpl->param.allocator->deallocate(impl, initImpl->param.userData);
	impl = NULL;
	return ret;
}

Parser::~Parser()
{
	terminate();
}

int Parser::initialize(const Initializer *init)
{
	if (impl != NULL)
		return SCE_XML_INFO_MULTIPLE_INITIALIZED;
	if (init == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	InitializerImpl *initImpl = InitializerImpl::get(init);
	if (initImpl == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	void *handleMem = initImpl->param.allocator->allocate(sizeof(ParserImpl),
							      initImpl->param.userData);
	if (handleMem == NULL)
		return SCE_XML_ERROR_NO_MEMORY;

	impl = new (handleMem) ParserImpl();
	impl->initializer = init;

	void *coreMem = initImpl->param.allocator->allocate(sizeof(SaxParserCore),
							    initImpl->param.userData);
	if (coreMem == NULL) {
		initImpl->param.allocator->deallocate(handleMem, initImpl->param.userData);
		return SCE_XML_ERROR_NO_MEMORY;
	}

	impl->core = new (coreMem) SaxParserCore(init);
	return impl->core->initialize();
}

void Parser::setDocumentHandler(DocumentHandler *processor)
{
	if (impl != NULL)
		impl->core->setDocumentHandler(processor);
}

void Parser::setUserData(void *data)
{
	if (impl != NULL)
		impl->core->setUserData(data);
}

int Parser::parse(const XmlText *text, bool isFinal)
{
	if (impl != NULL)
		return impl->core->parse(text, isFinal);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

int Parser::reset()
{
	if (impl != NULL)
		return impl->core->reset();
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

void Parser::setSkipIgnorableWhiteSpace(bool isSkipped)
{
	if (impl != NULL)
		impl->core->setSkipIgnorableWhiteSpace(isSkipped);
}

void Parser::setResolveEntity(bool isResolved)
{
	if (impl != NULL)
		impl->core->setResolveEntity(isResolved);
}
} /* namespace Sax */
} /* namespace Xml */
} /* namespace sce */
