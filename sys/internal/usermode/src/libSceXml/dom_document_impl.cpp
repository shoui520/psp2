/*
 * dom_document_impl.cpp: sce::Xml::Dom::DocumentImpl.
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{
namespace Dom
{

const char *s_xmlDeclaration = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";

DocumentImpl::DocumentImpl(const Initializer *init)
	: CoreWrapper<BxceDocument, DocumentCoreTraits>(init), serialized(NULL)
{
}

int DocumentImpl::terminate()
{
	if (serialized.buffer != NULL)
		ScebXCe_EC560647(const_cast<char *>(serialized.buffer));
	return terminateCoreInline();
}

DocumentImpl::~DocumentImpl()
{
	terminate();
}

void DocumentImpl::setOwnerDocument(Document *owner)
{
	if (core != NULL)
		core->ownerDocument = owner;
}

NodeId DocumentImpl::getDocRoot() const
{
	if (core == NULL)
		return SCE_XML_INVALID_NODE_ID;
	return ((SceUInt64)(uintptr_t)core << 32) | (SceUInt64)ScebXCe_75E1B095(core);
}

NodeId DocumentImpl::getRoot() const
{
	if (core == NULL)
		return SCE_XML_INVALID_NODE_ID;
	BxceNodeId root = ScebXCe_CED6BECF(core);
	return makeNodeId(core, root);
}

NodeId DocumentImpl::getFirstChild(NodeId node) const
{
	if (core == NULL)
		return SCE_XML_INVALID_NODE_ID;
	BxceNodeId child = ScebXCe_FEA6E4A1(core, nodeIdLocal(node));
	return makeNodeId(core, child);
}

NodeId DocumentImpl::getSibling(NodeId node) const
{
	if (core == NULL)
		return SCE_XML_INVALID_NODE_ID;
	BxceNodeId sib = ScebXCe_6FC9DC3F(core, nodeIdLocal(node));
	return makeNodeId(core, sib);
}

String DocumentImpl::getNodeName(NodeId node) const
{
	if (core == NULL)
		return String();
	String res(ScebXCe_1189A2B1(core, nodeIdLocal(node)));
	return res;
}

String DocumentImpl::getText(NodeId node) const
{
	if (core == NULL)
		return String();
	String res(ScebXCe_33825EFF(core, nodeIdLocal(node)));
	return res;
}

NodeId DocumentImpl::getFirstAttr(NodeId node) const
{
	if (core == NULL)
		return SCE_XML_INVALID_NODE_ID;
	BxceNodeId attr = ScebXCe_B034C595(core, nodeIdLocal(node));
	return makeNodeId(core, attr);
}

String DocumentImpl::getAttrName(NodeId node) const
{
	if (core == NULL)
		return String();
	String res(ScebXCe_B4989A58(core, nodeIdLocal(node)));
	return res;
}

String DocumentImpl::getAttrValue(NodeId node) const
{
	if (core == NULL)
		return String();
	String res(ScebXCe_84383D8E(core, nodeIdLocal(node)));
	return res;
}

NodeId DocumentImpl::getNextAttr(NodeId node) const
{
	if (core == NULL)
		return SCE_XML_INVALID_NODE_ID;
	BxceNodeId attr = ScebXCe_134F491C(core, nodeIdLocal(node));
	return makeNodeId(core, attr);
}

NodeId DocumentImpl::getParent(NodeId node) const
{
	if (core == NULL)
		return SCE_XML_INVALID_NODE_ID;
	BxceNodeId parent = bxceDocumentGetParent(core, nodeIdLocal(node));
	return makeNodeId(core, parent);
}

String DocumentImpl::getSkippedText(NodeId node) const
{
	if (core == NULL)
		return String();
	String res(ScebXCe_2FAD3DFA(core, nodeIdLocal(node)));
	return res;
}

EntityType DocumentImpl::getEntityType(NodeId node) const
{
	if (core != NULL)
		return (EntityType)ScebXCe_35CDE77C(core, nodeIdLocal(node));
	return entityUnknown;
}

String DocumentImpl::getEntity(NodeId node) const
{
	if (core == NULL)
		return String();
	String res(bxceDocumentGetEntityText(core, nodeIdLocal(node)));
	return res;
}

MetaNodeId DocumentImpl::getXmlMeta() const
{
	BxceDocument *doc = core;
	if (doc == NULL)
		return SCE_XML_INVALID_NODE_ID;
	return makeNodeId(doc, bxceDocumentGetXmlMeta(doc));
}

NodeType DocumentImpl::getNodeType(NodeId node) const
{
	if (core == NULL || nodeIdLocal(node) == 0)
		return nodeUnknown;
	return (NodeType)ScebXCe_FA18A039(core, nodeIdLocal(node));
}

int DocumentImpl::getElementsByTagName(NodeId node, const String *name, NodeList *nodelist) const
{
	if (core == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	if (nodelist == NULL || name == NULL || name->buffer == NULL || name->size == 0)
		return SCE_XML_ERROR_INVALID_ARGUMENT;

	if (nodelist->impl != NULL) {
		nodelist->impl->terminate();
	} else {
		const InitParameter *param = getInitParam();
		void *mem = param->allocator->allocate(sizeof(NodeListImpl), param->userData);
		if (mem == NULL)
			return SCE_XML_ERROR_NO_MEMORY;
		nodelist->impl = new (mem) NodeListImpl(initializer);
	}

	BxceNodeList *rawList = NULL;
	int bxRet = bxceDocumentGetElementsByTagNameStatus(core, nodeIdLocal(node),
							   name->buffer, name->size, &rawList);
	if (bxRet != 0)
		return bXResultToResultType(bxRet);

	nodelist->impl->attachOwnedCore(rawList);
	return SCE_XML_SUCCESS;
}

int DocumentImpl::getStatus() const
{
	if (core != NULL)
		return bXResultToResultType(core->status);
	return SCE_XML_ERROR_NOT_INITIALIZED;
}

void DocumentImpl::resetStatus()
{
	if (core != NULL)
		core->status = 0;
}

bool DocumentImpl::isReadOnly() const
{
	if (core != NULL)
		return core->isBinary;
	return false;
}

NodeId DocumentImpl::addElementChild(NodeId parent, const String *name,
				     const AttributeList *list, const String *text)
{
	if (core == NULL || name == NULL || name->buffer == NULL || name->size == 0)
		return SCE_XML_INVALID_NODE_ID;

	String textCopy;
	const char *nameStr = name->buffer;
	VarAllocBuffer nameBuf(initializer);
	if (name->buffer[name->size] != '\0') {
		if (nameBuf.reserve(name->size) != SCE_XML_SUCCESS)
			return SCE_XML_INVALID_NODE_ID;
		nameStr = reinterpret_cast<const char *>(nameBuf.buffer);
		nameBuf.copyStr(*name);
	}

	if (text != NULL && text->buffer != NULL && text->size != 0)
		textCopy = *text;

	const BxceAttributeList *rawAttrs = NULL;
	if (list != NULL)
		rawAttrs = reinterpret_cast<const AttributeListImpl *>(list)->list;

	BxceDocument *doc = core;
	BxceNodeId elem = ScebXCe_AC0E4F67(doc, nodeIdLocal(parent), nameStr,
					   rawAttrs, textCopy.buffer, textCopy.size);
	return ((SceUInt64)(uintptr_t)doc << 32) | (SceUInt64)elem;
}

NodeId DocumentImpl::createElement(const String *name, const AttributeList *list,
				   const String *text)
{
	if (core == NULL || name == NULL || name->buffer == NULL || name->size == 0)
		return SCE_XML_INVALID_NODE_ID;

	String textCopy;
	const char *nameStr = name->buffer;
	VarAllocBuffer nameBuf(initializer);
	if (name->buffer[name->size] != '\0') {
		if (nameBuf.reserve(name->size) != SCE_XML_SUCCESS)
			return SCE_XML_INVALID_NODE_ID;
		nameStr = reinterpret_cast<const char *>(nameBuf.buffer);
		nameBuf.copyStr(*name);
	}

	if (text != NULL && text->buffer != NULL && text->size != 0)
		textCopy = *text;

	const BxceAttributeList *rawAttrs = NULL;
	if (list != NULL)
		rawAttrs = reinterpret_cast<const AttributeListImpl *>(list)->list;

	BxceDocument *doc = core;
	BxceNodeId elem = bxceDocumentCreateElement(doc, nameStr, rawAttrs,
						    textCopy.buffer, textCopy.size);
	return ((SceUInt64)(uintptr_t)doc << 32) | (SceUInt64)elem;
}

NodeId DocumentImpl::insertNode(NodeId parent, NodeId ref, NodeId child)
{
	BxceDocument *doc = core;
	if (doc == NULL)
		return SCE_XML_INVALID_NODE_ID;
	BxceNodeId inserted = ScebXCe_3BF2F11B(doc, nodeIdLocal(parent),
					       nodeIdLocal(ref), nodeIdLocal(child));
	return ((SceUInt64)(uintptr_t)doc << 32) | (SceUInt64)inserted;
}

NodeId DocumentImpl::createTextNode(const String *text)
{
	BxceDocument *doc = core;
	if (doc == NULL)
		return SCE_XML_INVALID_NODE_ID;
	if (text == NULL || text->buffer == NULL || text->size == 0)
		return (NodeId)(SceUInt32)SCE_XML_ERROR_INVALID_ARGUMENT;
	BxceNodeId created = ScebXCe_AEB2CE13(doc, text->buffer, text->size);
	return ((SceUInt64)(uintptr_t)doc << 32) | (SceUInt64)created;
}

int DocumentImpl::recurseDelete(NodeId node)
{
	if (core == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	ScebXCe_2C2628BB(core, nodeIdLocal(node));
	return SCE_XML_SUCCESS;
}

NodeId DocumentImpl::removeChild(NodeId child, NodeId parent)
{
	BxceDocument *doc = core;
	if (doc == NULL)
		return SCE_XML_INVALID_NODE_ID;
	BxceNodeId removed = ScebXCe_568CE24D(doc, nodeIdLocal(child), nodeIdLocal(parent));
	return ((SceUInt64)(uintptr_t)doc << 32) | (SceUInt64)removed;
}

int DocumentImpl::importParent(const Document *parentDoc, NodeId parent)
{
	if (core == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	if (parentDoc == NULL)
		return SCE_XML_ERROR_INVALID_ARGUMENT;
	int bxRet = bxceDocumentImportParent(core, parentDoc->impl->core, nodeIdLocal(parent));
	return bXResultToResultType(bxRet);
}

NodeId DocumentImpl::importNode(NodeId nparent, NodeId ref, const Document *doc, NodeId node)
{
	BxceDocument *dstDoc = core;
	if (dstDoc == NULL)
		return SCE_XML_INVALID_NODE_ID;
	if (doc == NULL)
		return (NodeId)(SceUInt32)SCE_XML_ERROR_INVALID_ARGUMENT;
	BxceNodeId imported = ScebXCe_62C76804(dstDoc, nodeIdLocal(nparent), nodeIdLocal(ref),
					       doc->impl->core, nodeIdLocal(node));
	return ((SceUInt64)(uintptr_t)dstDoc << 32) | (SceUInt64)imported;
}

NodeId DocumentImpl::setWritable()
{
	if (core == NULL)
		return SCE_XML_INVALID_NODE_ID;
	return ((SceUInt64)(uintptr_t)core << 32) | (SceUInt64)bxceDocumentSetWritable(core);
}

int DocumentImpl::setText(NodeId node, const String *text)
{
	if (core == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	int bxRet = bxceDocumentSetText(core, nodeIdLocal(node), text->buffer, text->size);
	return bXResultToResultType(bxRet);
}

int DocumentImpl::setAttrValue(NodeId node, const String *name, const String *value)
{
	if (core == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	int bxRet = bxceDocumentSetAttribute(core, nodeIdLocal(node),
					     name->buffer, value->buffer, value->size);
	return bXResultToResultType(bxRet);
}

int DocumentImpl::setAttribute(NodeId node, const String *name, const String *value)
{
	if (core == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	int bxRet = bxceDocumentSetAttrValue(core, nodeIdLocal(node),
					     name->buffer, value->buffer, value->size);
	return bXResultToResultType(bxRet);
}

int DocumentImpl::setAttributeList(NodeId node, const AttributeList *list)
{
	if (core == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	int bxRet = bxceDocumentSetAttributeList(
		core, nodeIdLocal(node),
		reinterpret_cast<const AttributeListImpl *>(list)->list);
	return bXResultToResultType(bxRet);
}

int DocumentImpl::removeAttribute(NodeId node, const String *name)
{
	if (core == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	int bxRet = bxceDocumentRemoveAttribute(core, nodeIdLocal(node), name->buffer);
	return bXResultToResultType(bxRet);
}

int DocumentImpl::removeAttributes(NodeId node)
{
	if (core == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	int bxRet = bxceDocumentRemoveAttributes(core, nodeIdLocal(node));
	return bXResultToResultType(bxRet);
}

int DocumentImpl::getChildNodes(NodeId node, NodeList *nodelist) const
{
	if (core == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	if (nodelist == NULL)
		return SCE_XML_ERROR_INVALID_ARGUMENT;

	if (nodelist->impl != NULL) {
		nodelist->impl->terminate();
	} else {
		const InitParameter *param = getInitParam();
		void *mem = param->allocator->allocate(sizeof(NodeListImpl), param->userData);
		if (mem == NULL)
			return SCE_XML_ERROR_NO_MEMORY;
		nodelist->impl = new (mem) NodeListImpl(initializer);
	}

	BxceNodeList *rawList = ScebXCe_2B9B57AE(core, nodeIdLocal(node));
	if (rawList == NULL)
		return SCE_XML_ERROR_DOM_ERROR;

	nodelist->impl->attachOwnedCore(rawList);
	return SCE_XML_SUCCESS;
}

NodeId DocumentImpl::getLastChild(NodeId node) const
{
	if (core == NULL)
		return SCE_XML_INVALID_NODE_ID;
	return ((SceUInt64)(uintptr_t)core << 32) | (SceUInt64)bxceDocumentGetLastChild(core, nodeIdLocal(node));
}

bool DocumentImpl::hasChildNodes(NodeId node) const
{
	if (core != NULL)
		return bxceDocumentHasChildNodes(core, nodeIdLocal(node));
	return false;
}

bool DocumentImpl::hasAttributes(NodeId node) const
{
	if (core != NULL)
		return bxceDocumentHasAttributes(core, nodeIdLocal(node));
	return false;
}

String DocumentImpl::getAttribute(NodeId node, const String *name) const
{
	String result;
	if (core != NULL && name != NULL && name->buffer != NULL && name->size != 0)
		result = String(ScebXCe_753D9382(core, nodeIdLocal(node), name->buffer));
	return result;
}

int DocumentImpl::getAttributes(NodeId node, NodeList *nodelist) const
{
	if (core == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	if (nodelist == NULL)
		return SCE_XML_ERROR_INVALID_ARGUMENT;

	if (nodelist->impl != NULL) {
		nodelist->impl->terminate();
	} else {
		const InitParameter *param = getInitParam();
		void *mem = param->allocator->allocate(sizeof(NodeListImpl), param->userData);
		if (mem == NULL)
			return SCE_XML_ERROR_NO_MEMORY;
		nodelist->impl = new (mem) NodeListImpl(initializer);
	}

	BxceNodeList *rawList = bxceDocumentGetAttributes(core, nodeIdLocal(node));
	if (rawList == NULL)
		return SCE_XML_ERROR_DOM_ERROR;

	nodelist->impl->attachOwnedCore(rawList);
	return SCE_XML_SUCCESS;
}

int DocumentImpl::serialize(const SerializeParameter *param, XmlText *outputString)
{
	if (core == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	if (outputString == NULL)
		return SCE_XML_ERROR_INVALID_ARGUMENT;

	if (serialized.buffer != NULL) {
		ScebXCe_EC560647(const_cast<char *>(serialized.buffer));
		serialized.buffer = NULL;
	}
	serialized.size = 0;

	SceUInt32 total = core->nodesSize + core->textsSize +
			  core->elementNamesSize + core->attrNamesSize;
	SceUInt32 capacity = 512;
	if (total + 32 >= 400)
		capacity = total + 288;

	char *buf = (char *)ScebXCe_B8DDE741(capacity);
	serialized.buffer = buf;
	if (buf == NULL)
		return SCE_XML_ERROR_NO_MEMORY;

	char *outBuf = buf;
	SceUInt32 outLen = 0;
	if (param != NULL && (param->serializeOption & SCE_XML_SERIALIZE_OPT_USE_XML_DECLARATION)) {
		bxceStrlcpy(outBuf, s_xmlDeclaration, BXCE_ATTR_STRING_MAX);
		outLen += (SceUInt32)bxceStrnlen(outBuf, BXCE_ATTR_STRING_MAX);
	}

	SceUInt32 remaining = capacity - outLen;
	const InitParameter *initParam = getInitParam();
	BxceSerializer *ser = (BxceSerializer *)initParam->allocator->allocate(
		sizeof(BxceSerializer), initParam->userData);
	int ret = SCE_XML_SUCCESS;
	if (ser != NULL) {
		ScebXCe_DC8483D4(ser);
		BxceNodeId root = ScebXCe_CED6BECF(core);
		if (bxceSerializerWriteNodes(ser, core, root, &outBuf, &outLen, &remaining) != 0) {
			const InitParameter *p = getInitParam();
			p->allocator->deallocate(outBuf, p->userData);
		}
		serialized.buffer = outBuf;
		serialized.size = outLen;
		outputString->buffer = outBuf;
		outputString->size = outLen;

		ScebXCe_BE1AD985();
		const InitParameter *p = getInitParam();
		p->allocator->deallocate(ser, p->userData);
	} else {
		ret = SCE_XML_ERROR_NO_MEMORY;
	}
	return ret;
}

} /* namespace Dom */
} /* namespace Xml */
} /* namespace sce */
