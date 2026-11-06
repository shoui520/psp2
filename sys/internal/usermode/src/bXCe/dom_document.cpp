/*
 * ScebXCe's Document: the C++ interface object of a DOM document, over a DocumentImpl. It holds
 * a reference on the library while it has one.
 */
#include "bxce_internal.h"

namespace bXCe {
namespace Dom {

Document::Document() : impl(NULL)
{
	if (bxceLibraryAddRef() < 0)
		return;
	impl = new DocumentImpl(0);
}

Document::Document(const Document &src) : impl(NULL)
{
	bxceLibraryAddRef();
	if (src.impl != NULL)
		impl = new DocumentImpl(*src.impl);
}

/* a document over another one's document (DocumentBuilder::getDocument) */
Document::Document(const DocumentImpl *src) : impl(NULL)
{
	bxceLibraryAddRef();
	impl = new DocumentImpl(src);
}

/* takes another reference on the library, and does not free the old implementation */
Document &Document::operator=(const DocumentImpl *src)
{
	bxceLibraryAddRef();
	impl = new DocumentImpl(src);
	return *this;
}

Document::~Document()
{
	if (impl != NULL) {
		delete impl;
		impl = NULL;
		bxceLibraryRelease();
	}
}

/* the argument is not used */
int Document::initialize(const void *init)
{
	if (impl == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return impl->initialize(NULL, true);
}

int Document::terminate()
{
	if (impl == NULL)
		return 0;
	return impl->terminate();
}

BxceNodeId Document::getDocRoot() const
{
	if (impl == NULL)
		return 0;
	return impl->getDocRoot();
}

BxceNodeId Document::getRoot() const
{
	if (impl == NULL)
		return 0;
	return impl->getRoot();
}

BxceNodeId Document::getFirstChild(BxceNodeId node) const
{
	if (impl == NULL)
		return 0;
	return impl->getFirstChild(node);
}

BxceNodeId Document::getSibling(BxceNodeId node) const
{
	if (impl == NULL)
		return 0;
	return impl->getSibling(node);
}

const char *Document::getNodeName(BxceNodeId node) const
{
	if (impl == NULL)
		return NULL;
	return impl->getNodeName(node);
}

const char *Document::getText(BxceNodeId node) const
{
	if (impl == NULL)
		return NULL;
	return impl->getText(node);
}

BxceNodeId Document::getFirstAttr(BxceNodeId node) const
{
	if (impl == NULL)
		return 0;
	return impl->getFirstAttr(node);
}

const char *Document::getAttrName(BxceNodeId node) const
{
	if (impl == NULL)
		return NULL;
	return impl->getAttrName(node);
}

const char *Document::getAttrValue(BxceNodeId attr) const
{
	if (impl == NULL)
		return NULL;
	return impl->getAttrValue(attr);
}

BxceNodeId Document::getNextAttr(BxceNodeId node) const
{
	if (impl == NULL)
		return 0;
	return impl->getNextAttr(node);
}

BxceNodeId Document::getParent(BxceNodeId node) const
{
	if (impl == NULL)
		return 0;
	return impl->getParent(node);
}

const char *Document::getSkippedText(BxceNodeId node) const
{
	if (impl == NULL)
		return NULL;
	return impl->getSkippedText(node);
}

int Document::getEntityType(BxceNodeId node) const
{
	if (impl == NULL)
		return 0;
	return impl->getEntityType(node);
}

/* the methods this library does not implement return 0 */
int Document::getEntity() const
{
	return 0;
}

int Document::getXmlMeta() const
{
	return 0;
}

int Document::reserved1() const
{
	return 0;
}

int Document::reserved2() const
{
	return 0;
}

int Document::reserved3() const
{
	return 0;
}

int Document::reserved4() const
{
	return 0;
}

const char *Document::getEntityName(BxceNodeId node) const
{
	if (impl == NULL)
		return NULL;
	return impl->getEntityName(node);
}

int Document::reserved5() const
{
	return 0;
}

int Document::reserved6() const
{
	return 0;
}

int Document::reserved7() const
{
	return 0;
}

int Document::reserved8() const
{
	return 0;
}

int Document::getNodeType(BxceNodeId node) const
{
	if (impl == NULL)
		return 0;
	return impl->getNodeType(node);
}

NodeList *Document::getElementsByTagName(BxceNodeId node, const char *name) const
{
	if (impl == NULL)
		return NULL;
	return impl->getElementsByTagName(node, name);
}

int Document::getStatus() const
{
	return 0;
}

void Document::resetStatus()
{
}

int Document::isReadOnly() const
{
	return 0;
}

BxceNodeId Document::addElementChild(BxceNodeId parent, const char *name,
				     const BxceAttributeList *attrs, const char *text,
				     SceUInt32 textLength)
{
	if (impl == NULL)
		return 0;
	return impl->addElementChild(parent, name, attrs, text, textLength);
}

int Document::createElement()
{
	return 0;
}

BxceNodeId Document::insertNode(BxceNodeId parent, BxceNodeId after, BxceNodeId child)
{
	if (impl == NULL)
		return 0;
	return impl->insertNode(parent, after, child);
}

BxceNodeId Document::createTextNode(const char *text, SceUInt32 length)
{
	if (impl == NULL)
		return 0;
	return impl->createTextNode(text, length);
}

void Document::recurseDelete()
{
}

BxceNodeId Document::removeChild(BxceNodeId child, BxceNodeId parent)
{
	if (impl == NULL)
		return 0;
	return impl->removeChild(child, parent);
}

int Document::importParent()
{
	return 0;
}

BxceNodeId Document::importNode(BxceNodeId parent, BxceNodeId after, const BxceDocument *src,
				BxceNodeId node)
{
	if (impl == NULL)
		return 0;
	return impl->importNode(parent, after, src, node);
}

int Document::setText()
{
	return 0;
}

int Document::setAttrValue()
{
	return 0;
}

int Document::setAttribute()
{
	return 0;
}

int Document::setAttributeList(BxceNodeId node, const char *name, const char *value,
			       SceUInt32 length)
{
	if (impl == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return impl->setAttributeList(node, name, value, length);
}

int Document::removeAttribute()
{
	return 0;
}

int Document::removeAttributes()
{
	return 0;
}

int Document::reserved9()
{
	return 0;
}

BxceNodeList *Document::getChildNodes(BxceNodeId node) const
{
	if (impl == NULL)
		return NULL;
	return impl->getChildNodes(node);
}

int Document::getLastChild() const
{
	return 0;
}

int Document::hasChildNodes() const
{
	return 0;
}

int Document::hasAttributes() const
{
	return 0;
}

const char *Document::getAttribute(BxceNodeId node, const char *name) const
{
	if (impl == NULL)
		return NULL;
	return impl->getAttribute(node, name);
}

int Document::getAttributes() const
{
	return 0;
}

} /* namespace Dom */
} /* namespace bXCe */
