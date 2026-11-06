/*
 * ScebXCe's Sax::ParserImpl: the C++ SAX parser over the library's, which reports to the
 * application's handler through a DocumentHandlerAdapter.
 */
#include "bxce_internal.h"

namespace bXCe {
namespace Sax {

ParserImpl::ParserImpl() : sax(NULL), handler(NULL)
{
	param.allocator = NULL;
	param.userData = NULL;
	sax = (BxceSaxParser *)ScebXCe_B8DDE741(sizeof(BxceSaxParser));
}

ParserImpl::~ParserImpl()
{
	if (sax != NULL) {
		ScebXCe_928189AB(sax);
		if (sax != NULL)
			ScebXCe_EC560647(sax);
		sax = NULL;
	}
	if (handler != NULL) {
		delete handler;
		handler = NULL;
	}
}

int ParserImpl::initialize(const InitParameter *initParam)
{
	if (sax == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	if (initParam != NULL) {
		param.allocator = initParam->allocator;
		param.userData = initParam->userData;
	}
	return bxceConvertResult(ScebXCe_C85586BD(sax));
}

int ParserImpl::terminate()
{
	if (sax == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return bxceConvertResult(ScebXCe_928189AB(sax));
}

int ParserImpl::setDocumentHandler(DocumentHandler *handler)
{
	if (sax == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;

	delete this->handler;
	this->handler = new DocumentHandlerAdapter(handler);
	if (this->handler == NULL)
		return SCE_BXCE_RESULT_NO_MEMORY;

	sax->handler = this->handler;
	sax->characterOffsets = 0;
	return bxceConvertResult(0);
}

int ParserImpl::setDocumentHandlerWithOffset(DocumentHandler *handler)
{
	if (sax == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;

	delete this->handler;
	this->handler = new DocumentHandlerAdapter(handler);
	if (this->handler == NULL)
		return SCE_BXCE_RESULT_NO_MEMORY;

	sax->handler = this->handler;
	sax->characterOffsets = 1;
	return bxceConvertResult(0);
}

void ParserImpl::setUserData(void *data)
{
	if (sax != NULL)
		sax->userData = data;
}

int ParserImpl::parse(const char *input, int size, bool isFinal)
{
	if (sax == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return bxceConvertResult(ScebXCe_4E9A7061(sax, input, size, isFinal));
}

int ParserImpl::reset()
{
	if (sax == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return bxceConvertResult(ScebXCe_9E06C329(sax));
}

int ParserImpl::save()
{
	if (sax == NULL)
		return -1;
	return ScebXCe_3CAF2B78(sax);
}

int ParserImpl::restore(int slot)
{
	if (sax == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	if (ScebXCe_A12B0F90(sax, slot) != 0)
		return SCE_BXCE_RESULT_INVALID_ARGUMENT;
	return 0;
}

void ParserImpl::setSkipIgnorableWhiteSpace(bool isSkipped)
{
	if (sax != NULL)
		ScebXCe_A40944DD(&sax->parser, isSkipped);
}

void ParserImpl::setResolveEntity(bool isResolved)
{
	if (sax != NULL)
		ScebXCe_FF7CB737(&sax->parser, isResolved);
}

void ParserImpl::setResolveAttributeEntity(bool isResolved)
{
	if (sax != NULL)
		bxceParserSetResolveAttrEntity(&sax->parser, isResolved);
}

} /* namespace Sax */
} /* namespace bXCe */
