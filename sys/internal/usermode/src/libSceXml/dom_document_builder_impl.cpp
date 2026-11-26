/*
 * dom_document_builder_impl.cpp: sce::Xml::Dom::DocumentBuilderImpl.
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{
namespace Dom
{

DocumentBuilderImpl::DocumentBuilderImpl(const Initializer *init)
	: CoreWrapper<BxceDocumentBuilder, DocumentBuilderCoreTraits>(init),
	  CoreWrapper<BxceDocument, DocumentCoreTraits>(init),
	  document()
{
}

DocumentBuilderImpl::~DocumentBuilderImpl()
{
}

int DocumentBuilderImpl::initialize()
{
	CoreWrapper<BxceDocumentBuilder, DocumentBuilderCoreTraits>::initializeCoreInline();
	return CoreWrapper<BxceDocument, DocumentCoreTraits>::initializeCoreInline();
}

int DocumentBuilderImpl::terminate()
{
	document.terminate();
	CoreWrapper<BxceDocumentBuilder, DocumentBuilderCoreTraits>::terminateCoreInline();
	return CoreWrapper<BxceDocument, DocumentCoreTraits>::terminateCoreInline();
}

int DocumentBuilderImpl::parse(const XmlText *text, bool isFinal)
{
	BxceDocumentBuilder *builder =
		CoreWrapper<BxceDocumentBuilder, DocumentBuilderCoreTraits>::core;
	if (builder == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	if (text == NULL || text->buffer == NULL || text->size == 0)
		return SCE_XML_ERROR_INVALID_ARGUMENT;

	int bxRet = ScebXCe_BC0D9A6B(builder, text->buffer, (int)text->size, (int)isFinal);
	return bXResultToResultType(bxRet);
}

Document DocumentBuilderImpl::getDocument()
{
	CoreWrapper<BxceDocumentBuilder, DocumentBuilderCoreTraits> *builderBase = this;
	BxceDocumentBuilder *builder = builderBase->core;
	if (builder != NULL && builder->document != NULL) {
		if (document.isAvailable()) {
			document.impl->attachReferenceCore(builder->document);
			document.impl->setOwnerDocument(&document);
		} else {
			const InitParameter *param = builderBase->getInitParam();
			if (param != NULL) {
				void *mem = param->allocator->allocate(sizeof(DocumentImpl),
								       param->userData);
				if (mem != NULL) {
					document.impl = new (mem) DocumentImpl(builderBase->initializer);
					document.impl->attachReferenceCore(builder->document);
					document.impl->setOwnerDocument(&document);
				}
			}
		}
	}
	return Document(document);
}

void DocumentBuilderImpl::setSkipIgnorableWhiteSpace(bool isSkipped)
{
	BxceDocumentBuilder *builder =
		CoreWrapper<BxceDocumentBuilder, DocumentBuilderCoreTraits>::core;
	if (builder != NULL)
		ScebXCe_A40944DD(&builder->parser, (int)isSkipped);
}

void DocumentBuilderImpl::setResolveEntity(bool isResolved)
{
	BxceDocumentBuilder *builder =
		CoreWrapper<BxceDocumentBuilder, DocumentBuilderCoreTraits>::core;
	if (builder != NULL) {
		ScebXCe_FF7CB737(&builder->parser, (int)isResolved);
		bxceParserSetResolveAttrEntity(&builder->parser, (int)isResolved);
	}
}

int DocumentBuilderImpl::setSkipIgnorableText(bool isSkipped)
{
	BxceDocumentBuilder *builder =
		CoreWrapper<BxceDocumentBuilder, DocumentBuilderCoreTraits>::core;
	if (builder == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	int bxRet = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
	if (builder->parser.status != SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS) {
		builder->skipIgnorableText = (SceUInt8)isSkipped;
		bxRet = 0;
	}
	return bXResultToResultType(bxRet);
}

} /* namespace Dom */
} /* namespace Xml */
} /* namespace sce */
