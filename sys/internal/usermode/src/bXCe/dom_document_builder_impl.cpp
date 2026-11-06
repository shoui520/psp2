/*
 * ScebXCe's DocumentBuilderImpl: the C++ DOM builder over the library's document builder.
 */
#include "bxce_internal.h"

namespace bXCe {
namespace Dom {

DocumentBuilderImpl::DocumentBuilderImpl(int reserved) : builder(NULL), document(NULL)
{
	builder = (BxceDocumentBuilder *)ScebXCe_B8DDE741(sizeof(BxceDocumentBuilder));
}

DocumentBuilderImpl::~DocumentBuilderImpl()
{
	if (builder != NULL) {
		ScebXCe_8FA315F1(builder);
		if (builder != NULL)
			ScebXCe_EC560647(builder);
		builder = NULL;
	}
	if (document != NULL) {
		delete document;
		document = NULL;
	}
}

int DocumentBuilderImpl::initialize()
{
	if (builder == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return bxceConvertResult(ScebXCe_11D1FC0D(builder));
}

/* terminates and frees the document handed out, then the builder */
int DocumentBuilderImpl::terminate()
{
	if (builder == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	if (document != NULL) {
		document->terminate();
		delete document;
		document = NULL;
	}
	return bxceConvertResult(ScebXCe_8FA315F1(builder));
}

int DocumentBuilderImpl::parse(const char *input, int size, bool isFinal)
{
	if (builder == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return bxceConvertResult(ScebXCe_BC0D9A6B(builder, input, size, isFinal));
}

/* the document parsed, as an implementation object over the builder's document, made once */
DocumentImpl *DocumentBuilderImpl::getDocument()
{
	BxceDocument *doc;

	if (builder == NULL)
		return NULL;
	doc = builder->document;
	if (document == NULL)
		document = new DocumentImpl(doc);
	return document;
}

void DocumentBuilderImpl::setSkipIgnorableWhiteSpace(bool isSkipped)
{
	if (builder != NULL)
		ScebXCe_A40944DD(&builder->parser, isSkipped);
}

void DocumentBuilderImpl::setResolveEntity(bool isResolved)
{
	if (builder != NULL)
		ScebXCe_FF7CB737(&builder->parser, isResolved);
}

/* not while a parse is in progress */
int DocumentBuilderImpl::setSkipIgnorableText(bool isSkipped)
{
	int ret;

	if (builder == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	ret = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
	if (builder->parser.status != ret) {
		ret = 0;
		builder->skipIgnorableText = isSkipped;
	}
	return bxceConvertResult(ret);
}

} /* namespace Dom */
} /* namespace bXCe */
