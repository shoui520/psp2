/*
 * dom_document_builder.cpp: sce::Xml::Dom::DocumentBuilder and DocumentBuilderImpl
 * (0x81000F80-0x81001383).
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{
namespace Dom
{

DocumentBuilder::DocumentBuilder()
	: impl(NULL)
{
}

int DocumentBuilder::terminate()
{
	if (impl == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	const InitParameter *param =
		static_cast<CoreWrapper<BxceDocumentBuilder, DocumentBuilderCoreTraits> *>(impl)
			->getInitParam();
	impl->terminate();
	param->allocator->deallocate(impl, param->userData);
	impl = NULL;
	return SCE_XML_SUCCESS;
}

DocumentBuilder::~DocumentBuilder()
{
	if (impl != NULL) {
		const InitParameter *param =
			static_cast<CoreWrapper<BxceDocumentBuilder, DocumentBuilderCoreTraits> *>(impl)
				->getInitParam();
		impl->terminate();
		param->allocator->deallocate(impl, param->userData);
		impl = NULL;
	}
}

int DocumentBuilder::initialize(const Initializer *init)
{
	if (impl != NULL)
		return SCE_XML_INFO_MULTIPLE_INITIALIZED;
	if (init == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	InitializerImpl *initImpl = InitializerImpl::get(init);
	if (initImpl == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	void *mem = initImpl->param.allocator->allocate(sizeof(DocumentBuilderImpl),
							initImpl->param.userData);
	if (mem == NULL)
		return SCE_XML_ERROR_NO_MEMORY;

	impl = new (mem) DocumentBuilderImpl(init);
	return impl->initialize();
}

int DocumentBuilder::parse(const XmlText *text, bool isFinal)
{
	if (impl != NULL)
		return impl->parse(text, isFinal);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

Document DocumentBuilder::getDocument()
{
	if (impl != NULL)
		return impl->getDocument();
	return Document();
}

void DocumentBuilder::setSkipIgnorableWhiteSpace(bool isSkipped)
{
	if (impl != NULL)
		impl->setSkipIgnorableWhiteSpace(isSkipped);
}

void DocumentBuilder::setSkipIgnorableText(bool isSkipped)
{
	if (impl != NULL)
		impl->setSkipIgnorableText(isSkipped);
}

void DocumentBuilder::setResolveEntity(bool isResolved)
{
	if (impl != NULL)
		impl->setResolveEntity(isResolved);
}

/* ---- DocumentBuilderImpl ---- */

} /* namespace Dom */
} /* namespace Xml */
} /* namespace sce */
