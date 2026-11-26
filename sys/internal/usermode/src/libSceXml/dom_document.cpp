/*
 * dom_document.cpp: sce::Xml::Dom::Document and DocumentImpl
 * (0x810009CC-0x81000F7F, 0x81001384-0x8100200F).
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{
namespace Dom
{

Document::Document()
	: impl(NULL)
{
}

int Document::initialize(const Initializer *init)
{
	if (impl != NULL)
		return SCE_XML_INFO_MULTIPLE_INITIALIZED;
	if (init == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	InitializerImpl *initImpl = InitializerImpl::get(init);
	if (initImpl == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	void *mem = initImpl->param.allocator->allocate(sizeof(DocumentImpl),
							initImpl->param.userData);
	if (mem == NULL)
		return SCE_XML_ERROR_NO_MEMORY;

	impl = new (mem) DocumentImpl(init);
	return impl->initializeCore();
}

bool Document::isAvailable() const
{
	if (impl != NULL)
		return impl->isCoreAvailable();
	return false;
}

Document::Document(const Document &src)
	: impl(NULL)
{
	int ret = SCE_XML_SUCCESS;
	if (src.isAvailable()) {
		ret = initialize(src.impl->initializer);
		if (ret == SCE_XML_SUCCESS) {
			impl->copyCore(*src.impl);
			impl->setOwnerDocument(this);
		}
	}
	if (impl != NULL)
		impl->status = ret;
}

int Document::terminate()
{
	if (impl == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	const InitParameter *param = impl->getInitParam();
	impl->terminate();
	param->allocator->deallocate(impl, param->userData);
	impl = NULL;
	return SCE_XML_SUCCESS;
}

Document::~Document()
{
	if (impl != NULL) {
		const InitParameter *param = impl->getInitParam();
		impl->terminate();
		param->allocator->deallocate(impl, param->userData);
		impl = NULL;
	}
}

const Document &Document::operator=(const Document &src)
{
	if (&src != this) {
		if (impl == NULL) {
			InitializerImpl *initImpl = InitializerImpl::get(src.impl->initializer);
			if (initImpl == NULL)
				return *this;
			void *mem = initImpl->param.allocator->allocate(sizeof(DocumentImpl),
									initImpl->param.userData);
			if (mem == NULL)
				return *this;
			impl = new (mem) DocumentImpl(src.impl->initializer);
			if (impl->initializeCore() != SCE_XML_SUCCESS)
				return *this;
		}
		impl->copyCore(*src.impl);
	}
	return *this;
}

NodeId Document::getDocRoot() const
{
	if (impl != NULL)
		return impl->getDocRoot();
	return SCE_XML_INVALID_NODE_ID;
}

NodeId Document::getRoot() const
{
	if (impl != NULL)
		return impl->getRoot();
	return SCE_XML_INVALID_NODE_ID;
}

NodeId Document::getFirstChild(NodeId node) const
{
	if (impl != NULL)
		return impl->getFirstChild(node);
	return SCE_XML_INVALID_NODE_ID;
}

NodeId Document::getSibling(NodeId node) const
{
	if (impl != NULL)
		return impl->getSibling(node);
	return SCE_XML_INVALID_NODE_ID;
}

String Document::getNodeName(NodeId node) const
{
	if (impl != NULL)
		return impl->getNodeName(node);
	return String(NULL);
}

String Document::getText(NodeId node) const
{
	if (impl != NULL)
		return impl->getText(node);
	return String(NULL);
}

NodeId Document::getFirstAttr(NodeId node) const
{
	if (impl != NULL)
		return impl->getFirstAttr(node);
	return SCE_XML_INVALID_NODE_ID;
}

NodeId Document::getNextAttr(NodeId node) const
{
	if (impl != NULL)
		return impl->getNextAttr(node);
	return SCE_XML_INVALID_NODE_ID;
}

String Document::getAttrName(NodeId node) const
{
	if (impl != NULL)
		return impl->getAttrName(node);
	return String(NULL);
}

String Document::getAttrValue(NodeId node) const
{
	if (impl != NULL)
		return impl->getAttrValue(node);
	return String(NULL);
}

NodeId Document::getParent(NodeId node) const
{
	if (impl != NULL)
		return impl->getParent(node);
	return SCE_XML_INVALID_NODE_ID;
}

String Document::getSkippedText(NodeId node) const
{
	if (impl != NULL)
		return impl->getSkippedText(node);
	return String();
}

EntityType Document::getEntityType(NodeId node) const
{
	if (impl != NULL)
		return impl->getEntityType(node);
	return entityUnknown;
}

String Document::getEntity(NodeId node) const
{
	if (impl != NULL)
		return impl->getEntity(node);
	return String();
}

MetaNodeId Document::getXmlMeta() const
{
	if (impl != NULL)
		return impl->getXmlMeta();
	return SCE_XML_INVALID_NODE_ID;
}

NodeType Document::getNodeType(NodeId node) const
{
	if (impl != NULL)
		return impl->getNodeType(node);
	return nodeUnknown;
}

int Document::getElementsByTagName(NodeId node, const String *name, NodeList *nodelist) const
{
	return impl->getElementsByTagName(node, name, nodelist);
}

int Document::getStatus() const
{
	if (impl != NULL)
		return impl->getStatus();
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

void Document::resetStatus()
{
	if (impl != NULL)
		impl->resetStatus();
}

bool Document::isReadOnly() const
{
	if (impl != NULL)
		return impl->isReadOnly();
	return true;
}

NodeId Document::setWritable()
{
	if (impl != NULL)
		return impl->setWritable();
	return SCE_XML_INVALID_NODE_ID;
}

NodeId Document::addElementChild(NodeId parent, const String *name,
				 const AttributeList *list, const String *text)
{
	if (impl != NULL)
		return impl->addElementChild(parent, name, list, text);
	return SCE_XML_INVALID_NODE_ID;
}

NodeId Document::createElement(const String *name, const AttributeList *list, const String *text)
{
	if (impl != NULL)
		return impl->createElement(name, list, text);
	return SCE_XML_INVALID_NODE_ID;
}

NodeId Document::insertNode(NodeId parent, NodeId ref, NodeId child)
{
	if (impl != NULL)
		return impl->insertNode(parent, ref, child);
	return SCE_XML_INVALID_NODE_ID;
}

NodeId Document::createTextNode(const String *text)
{
	if (impl != NULL)
		return impl->createTextNode(text);
	return SCE_XML_INVALID_NODE_ID;
}

int Document::recurseDelete(NodeId node)
{
	if (impl != NULL)
		return impl->recurseDelete(node);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

NodeId Document::removeChild(NodeId child, NodeId parent)
{
	if (impl != NULL)
		return impl->removeChild(child, parent);
	return SCE_XML_INVALID_NODE_ID;
}

int Document::importParent(const Document *parentDoc, NodeId parent)
{
	if (impl != NULL)
		return impl->importParent(parentDoc, parent);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

NodeId Document::importNode(NodeId nparent, NodeId ref, const Document *doc, NodeId node)
{
	if (impl != NULL)
		return impl->importNode(nparent, ref, doc, node);
	return SCE_XML_INVALID_NODE_ID;
}

int Document::setText(NodeId node, const String *text)
{
	if (impl != NULL)
		return impl->setText(node, text);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

int Document::setAttrValue(NodeId node, const String *name, const String *value)
{
	if (impl != NULL)
		return impl->setAttrValue(node, name, value);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

int Document::setAttribute(NodeId node, const String *name, const String *value)
{
	if (impl != NULL)
		return impl->setAttribute(node, name, value);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

int Document::setAttributeList(NodeId node, const AttributeList *list)
{
	if (impl != NULL)
		return impl->setAttributeList(node, list);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

int Document::removeAttribute(NodeId node, const String *name)
{
	if (impl != NULL)
		return impl->removeAttribute(node, name);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

int Document::removeAttributes(NodeId node)
{
	if (impl != NULL)
		return impl->removeAttributes(node);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

int Document::getChildNodes(NodeId node, NodeList *nodelist) const
{
	if (impl != NULL)
		return impl->getChildNodes(node, nodelist);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

NodeId Document::getLastChild(NodeId node) const
{
	if (impl != NULL)
		return impl->getLastChild(node);
	return SCE_XML_INVALID_NODE_ID;
}

bool Document::hasChildNodes(NodeId node) const
{
	if (impl != NULL)
		return impl->hasChildNodes(node);
	return false;
}

bool Document::hasAttributes(NodeId node) const
{
	if (impl != NULL)
		return impl->hasAttributes(node);
	return false;
}

String Document::getAttribute(const NodeId node, const String *name) const
{
	if (impl != NULL)
		return impl->getAttribute(node, name);
	return String();
}

int Document::getAttributes(NodeId node, NodeList *nodelist) const
{
	if (impl != NULL)
		return impl->getAttributes(node, nodelist);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

int Document::serialize(const SerializeParameter *param, XmlText *outputString)
{
	if (impl != NULL)
		return impl->serialize(param, outputString);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

/* ---- DocumentImpl ---- */

} /* namespace Dom */
} /* namespace Xml */
} /* namespace sce */
