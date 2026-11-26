/*
 * sax_parser_impl.cpp: sce::Xml::Sax::SaxParserCore and DocumentHandlerAdapter.
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{
namespace Sax
{

int DocumentHandlerAdapter::startDocument(void *userData)
{
	return handler->startDocument(userData);
}

int DocumentHandlerAdapter::startElement(void *userData, const char *name, int length,
					 const BxceAttributeList *attrList)
{
	AttributeList attrs;
	if (attrs.initialize(initializer) == SCE_XML_SUCCESS)
		AttributeListImpl::get(&attrs)->attachReference(
			const_cast<BxceAttributeList *>(attrList));
	String elemName(name, (size_t)length);
	return handler->startElement(userData, &elemName, &attrs);
}

int DocumentHandlerAdapter::characters(void *userData, const char *ch, int length)
{
	String text(ch, (size_t)length);
	return handler->characters(userData, &text);
}

int DocumentHandlerAdapter::endElement(void *userData, const char *name, int length)
{
	String elemName(name, (size_t)length);
	return handler->endElement(userData, &elemName);
}

int DocumentHandlerAdapter::endDocument(void *userData)
{
	return handler->endDocument(userData);
}

int DocumentHandlerAdapter::skippedText(void *userData, int tokenType,
					const char *text, int length)
{
	String skipped(text, (size_t)length);
	return handler->skippedText(userData, (TokenType)tokenType, &skipped);
}

int DocumentHandlerAdapter::entityData(void *userData, int entityType,
				       const char *name, int length)
{
	String entityName(name, (size_t)length);
	return handler->entityData(userData, (EntityType)entityType, &entityName);
}

void DocumentHandlerAdapter::fatalError(void *userData, int errCode)
{
	handler->fatalError(userData, bXResultToResultType(errCode));
}

SaxParserCore::SaxParserCore(const Initializer *init)
	: InitializerUser(init), sax(NULL), handlerAdapter(NULL)
{
}

SaxParserCore::~SaxParserCore()
{
	if (sax != NULL) {
		ScebXCe_928189AB(sax);
		if (sax != NULL)
			ScebXCe_EC560647(sax);
		sax = NULL;
	}
	if (handlerAdapter != NULL) {
		delete handlerAdapter;
		handlerAdapter = NULL;
	}
}

int SaxParserCore::initialize()
{
	if (sax != NULL)
		return SCE_XML_INFO_MULTIPLE_INITIALIZED;

	InitializerImpl *initImpl = InitializerImpl::get(initializer);
	sax = (BxceSaxParser *)initImpl->param.allocator->allocate(sizeof(BxceSaxParser),
								   initImpl->param.userData);
	if (sax == NULL)
		return SCE_XML_ERROR_NO_MEMORY;

	int bxRet = ScebXCe_C85586BD(sax);
	return bXResultToResultType(bxRet);
}

int SaxParserCore::terminate()
{
	if (sax == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	InitializerImpl *initImpl = InitializerImpl::get(initializer);
	if (handlerAdapter != NULL) {
		initImpl->param.allocator->deallocate(handlerAdapter, initImpl->param.userData);
		handlerAdapter = NULL;
	}
	int bxRet = ScebXCe_928189AB(sax);
	initImpl->param.allocator->deallocate(sax, initImpl->param.userData);
	sax = NULL;
	return bXResultToResultType(bxRet);
}

int SaxParserCore::setDocumentHandler(DocumentHandler *docHandler)
{
	if (sax == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	InitializerImpl *initImpl = InitializerImpl::get(initializer);
	if (handlerAdapter != NULL)
		initImpl->param.allocator->deallocate(handlerAdapter, initImpl->param.userData);

	void *mem = initImpl->param.allocator->allocate(sizeof(DocumentHandlerAdapter),
							initImpl->param.userData);
	if (mem == NULL)
		return SCE_XML_ERROR_NO_MEMORY;

	handlerAdapter = new (mem) DocumentHandlerAdapter(initializer, docHandler);
	sax->handler = handlerAdapter;
	sax->characterOffsets = 0;
	return bXResultToResultType(0);
}

void SaxParserCore::setUserData(void *data)
{
	if (sax != NULL)
		sax->userData = data;
}

int SaxParserCore::parse(const XmlText *text, bool isFinal)
{
	if (sax == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	if (text == NULL || text->buffer == NULL || text->size == 0)
		return SCE_XML_ERROR_INVALID_ARGUMENT;

	int bxRet = ScebXCe_4E9A7061(sax, text->buffer, (int)text->size, (int)isFinal);
	return bXResultToResultType(bxRet);
}

int SaxParserCore::reset()
{
	if (sax == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	int bxRet = ScebXCe_9E06C329(sax);
	return bXResultToResultType(bxRet);
}

void SaxParserCore::setSkipIgnorableWhiteSpace(bool isSkipped)
{
	if (sax != NULL)
		ScebXCe_A40944DD(&sax->parser, (int)isSkipped);
}

void SaxParserCore::setResolveEntity(bool isResolved)
{
	if (sax != NULL) {
		ScebXCe_FF7CB737(&sax->parser, (int)isResolved);
		bxceParserSetResolveAttrEntity(&sax->parser, (int)isResolved);
	}
}

} /* namespace Sax */
} /* namespace Xml */
} /* namespace sce */
