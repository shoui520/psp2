/*
 * dom_node.cpp: sce::Xml::Dom::Node (0x8100245C-0x81002B9F).
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{
namespace Dom
{

static inline NodeImpl *nodeImplPtr(Node *self)
{
	return reinterpret_cast<NodeImpl *>(self);
}

static inline const NodeImpl *nodeImplPtr(const Node *self)
{
	return reinterpret_cast<const NodeImpl *>(self);
}

static __attribute__((noinline)) bool isChildNodeTypeAllowed(int parentType, int childType)
{
	switch (parentType) {
	case SCE_XML_TOKEN_DOM_DOCUMENT:
		switch (childType) {
		case SCE_XML_TOKEN_DOM_ELEMENT:
		case SCE_XML_TOKEN_DOM_COMPACT_ELEMENT:
		case SCE_XML_TOKEN_DOM_SKIP_TEXT_PI:
		case SCE_XML_TOKEN_DOM_SKIP_TEXT_COMMENT:
			return true;
		default:
			return false;
		}
	case SCE_XML_TOKEN_DOM_ATTR:
		switch (childType) {
		case SCE_XML_TOKEN_DOM_TEXT:
			return true;
		default:
			return false;
		}
	case SCE_XML_TOKEN_DOM_ELEMENT:
	case SCE_XML_TOKEN_DOM_COMPACT_ELEMENT:
		switch (childType) {
		case SCE_XML_TOKEN_DOM_ELEMENT:
		case SCE_XML_TOKEN_DOM_COMPACT_ELEMENT:
		case SCE_XML_TOKEN_DOM_SKIP_TEXT_CDATA:
		case SCE_XML_TOKEN_DOM_TEXT:
		case SCE_XML_TOKEN_DOM_SKIP_TEXT_PI:
		case SCE_XML_TOKEN_DOM_SKIP_TEXT_COMMENT:
			return true;
		default:
			return false;
		}
	case SCE_XML_TOKEN_DOM_HAS_TEXT_SIBLING:
		switch (childType) {
		case SCE_XML_TOKEN_DOM_ENTITY:
		case SCE_XML_TOKEN_DOM_TEXT:
			return true;
		default:
			return false;
		}
	default:
		return false;
	}
}

Node::Node(NodeId id)
{
	NodeImpl *data = nodeImplPtr(this);
	data->id = id;
	data->doc = nodeIdDocument(id);
	data->node = nodeIdLocal(id);
	data->initParam = NULL;
	data->childNodes = NULL;
	data->attributes = NULL;

	if (data->doc != NULL) {
		const InitializerUser *owner =
			static_cast<const InitializerUser *>(data->doc->ownerDocument);
		if (owner != NULL)
			data->initParam = reinterpret_cast<const InitParameter *>(owner->initializer);
	}
}

Node::~Node()
{
	NodeImpl *data = nodeImplPtr(this);
	if (data->childNodes != NULL) {
		data->childNodes->terminate();
		data->initParam->allocator->deallocate(data->childNodes, data->initParam->userData);
		data->childNodes = NULL;
	}
	if (data->attributes != NULL) {
		data->attributes->terminate();
		data->initParam->allocator->deallocate(data->attributes, data->initParam->userData);
		data->attributes = NULL;
	}
}

String Node::getNodeName() const
{
	const NodeImpl *data = nodeImplPtr(this);
	if (data->doc == NULL)
		return String();

	int type = ScebXCe_FA18A039(data->doc, data->node);
	const char *name = NULL;
	switch (type) {
	case SCE_XML_TOKEN_DOM_DOCUMENT:
		name = "xml";
		break;
	case SCE_XML_TOKEN_DOM_ATTR:
		name = ScebXCe_B4989A58(data->doc, data->node);
		break;
	case SCE_XML_TOKEN_DOM_ELEMENT:
		name = ScebXCe_1189A2B1(data->doc, data->node);
		break;
	default:
		name = NULL;
		break;
	}
	return String(name);
}

String Node::getNodeValue() const
{
	const NodeImpl *data = nodeImplPtr(this);
	if (data->doc == NULL)
		return String();

	int type = ScebXCe_FA18A039(data->doc, data->node);
	const char *val;
	if (type == SCE_XML_TOKEN_DOM_DOCUMENT)
		val = "version=\"1.0\"";
	else if (type == SCE_XML_TOKEN_DOM_ATTR)
		val = ScebXCe_84383D8E(data->doc, data->node);
	else
		val = ScebXCe_33825EFF(data->doc, data->node);
	return String(val);
}

NodeType Node::getNodeType() const
{
	const NodeImpl *data = nodeImplPtr(this);
	if (data->doc != NULL)
		return (NodeType)ScebXCe_FA18A039(data->doc, data->node);
	return nodeUnknown;
}

NodeId Node::getParentNode() const
{
	const NodeImpl *data = nodeImplPtr(this);
	if (data->doc == NULL)
		return SCE_XML_INVALID_NODE_ID;
	BxceNodeId parent = bxceDocumentGetParent(data->doc, data->node);
	return makeNodeId(data->doc, parent);
}

Document *Node::getOwnerDocument() const
{
	const NodeImpl *data = nodeImplPtr(this);
	if (data->doc != NULL)
		return static_cast<Document *>(data->doc->ownerDocument);
	return NULL;
}

/* Matches Sony 0x8100261A: constructs two empty NodeLists and returns temporary address */
NodeList *Node::getChildNodes() const
{
	NodeList empty;
	NodeList temp;
	return &temp;
}

NodeId Node::getFirstChild() const
{
	const NodeImpl *data = nodeImplPtr(this);
	if (data->doc == NULL)
		return SCE_XML_INVALID_NODE_ID;
	BxceNodeId child = ScebXCe_FEA6E4A1(data->doc, data->node);
	return makeNodeId(data->doc, child);
}

NodeId Node::getLastChild() const
{
	const NodeImpl *data = nodeImplPtr(this);
	if (data->doc == NULL)
		return SCE_XML_INVALID_NODE_ID;
	BxceNodeId child = bxceDocumentGetLastChild(data->doc, data->node);
	return makeNodeId(data->doc, child);
}

NodeId Node::getNextSibling() const
{
	const NodeImpl *data = nodeImplPtr(this);
	if (data->doc == NULL)
		return SCE_XML_INVALID_NODE_ID;
	BxceNodeId sib = ScebXCe_6FC9DC3F(data->doc, data->node);
	return makeNodeId(data->doc, sib);
}

/* Matches Sony 0x810026DA: constructs two empty NodeLists and returns temporary address */
NodeList *Node::getAttributes() const
{
	NodeList empty;
	NodeList temp;
	return &temp;
}

NodeId Node::insertBefore(NodeId newChild, NodeId refChild)
{
	NodeImpl *data = nodeImplPtr(this);
	if (data->doc == NULL || data->node == 0)
		return SCE_XML_INVALID_NODE_ID;
	BxceNodeId inserted = ScebXCe_3BF2F11B(data->doc, data->node,
					       nodeIdLocal(refChild), nodeIdLocal(newChild));
	return makeNodeId(data->doc, inserted);
}

NodeId Node::removeChild(NodeId oldChild)
{
	NodeImpl *data = nodeImplPtr(this);
	if (data->doc == NULL || data->node == 0)
		return SCE_XML_INVALID_NODE_ID;
	BxceNodeId removed = ScebXCe_568CE24D(data->doc, nodeIdLocal(oldChild), data->node);
	return makeNodeId(data->doc, removed);
}

bool Node::isAvailable() const
{
	const NodeImpl *data = nodeImplPtr(this);
	return this != NULL && data->doc != NULL && data->node != 0;
}

NodeId Node::appendChild(NodeId newChild)
{
	Node childNode(newChild);
	if (!isAvailable() || !childNode.isAvailable())
		return SCE_XML_INVALID_NODE_ID;

	NodeImpl *data = nodeImplPtr(this);
	int parentType = (int)getNodeType();
	int childType = (int)childNode.getNodeType();
	if (!isChildNodeTypeAllowed(parentType, childType))
		return SCE_XML_INVALID_NODE_ID;

	String childVal(NULL);
	if (childType == SCE_XML_TOKEN_DOM_ENTITY || childType == SCE_XML_TOKEN_DOM_TEXT)
		childVal = childNode.getNodeValue();

	BxceNodeId appended = 0;
	switch (parentType) {
	case SCE_XML_TOKEN_DOM_ELEMENT:
	case SCE_XML_TOKEN_DOM_COMPACT_ELEMENT:
		switch (childType) {
		case SCE_XML_TOKEN_DOM_ROOT_DOCUMENT:
		case SCE_XML_TOKEN_DOM_ELEMENT:
		case SCE_XML_TOKEN_DOM_COMPACT_ELEMENT: {
			NodeList *attrNodeList = childNode.getAttributes();
			if (attrNodeList != NULL) {
				BxceAttributeList *attrList = NULL;
				int status = -1;
				BxceNodeList *rawAttrs = attrNodeList->impl->core;
				if (rawAttrs != NULL) {
					attrList = (BxceAttributeList *)ScebXCe_B8DDE741(
						sizeof(BxceAttributeList));
					if (attrList != NULL) {
						status = 0;
						for (SceUInt32 i = 0; i < (SceUInt32)rawAttrs->length; ++i) {
							BxceNodeId attrId = ScebXCe_6C46D786(rawAttrs, i);
							if (attrId != 0) {
								const char *aName =
									ScebXCe_B4989A58(data->doc, attrId);
								const char *aVal =
									ScebXCe_84383D8E(data->doc, attrId);
								status = ScebXCe_3DD761CF(attrList, aName, aVal);
								if (status != 0)
									break;
							}
						}
					}
				}
				if (status == 0) {
					String elemName = childNode.getNodeName();
					appended = ScebXCe_AC0E4F67(data->doc, data->node,
								    elemName.buffer, attrList, NULL, 0);
				}
			}
			break;
		}
		case SCE_XML_TOKEN_DOM_SKIP_TEXT_CDATA:
		case SCE_XML_TOKEN_DOM_SKIP_TEXT_PI:
		case SCE_XML_TOKEN_DOM_SKIP_TEXT_COMMENT:
			appended = bxceDocumentAddSkipped(data->doc, childVal.buffer,
							  childVal.size, childType);
			break;
		case SCE_XML_TOKEN_DOM_TEXT:
			appended = bxceDocumentAppendText(data->doc, data->node,
							  childVal.buffer, childVal.size);
			break;
		default:
			appended = 0;
			break;
		}
		break;
	case SCE_XML_TOKEN_DOM_HAS_TEXT_SIBLING:
		if (childType == SCE_XML_TOKEN_DOM_ENTITY || childType == SCE_XML_TOKEN_DOM_TEXT)
			appended = (bxceDocumentExtendText(data->doc, data->node,
							   childVal.buffer, childVal.size) == 0) ? 1 : 0;
		break;
	case SCE_XML_TOKEN_DOM_ATTR:
		if (childType == SCE_XML_TOKEN_DOM_TEXT)
			appended = (bxceDocumentExtendText(data->doc, data->node,
							   childVal.buffer, childVal.size) == 0) ? 1 : 0;
		break;
	default:
		return SCE_XML_INVALID_NODE_ID;
	}

	if (appended == 0)
		return SCE_XML_INVALID_NODE_ID;
	return newChild;
}

bool Node::hasChildNodes() const
{
	const NodeImpl *data = nodeImplPtr(this);
	if (data->doc != NULL)
		return bxceDocumentHasChildNodes(data->doc, data->node);
	return false;
}

bool Node::hasAttributes() const
{
	const NodeImpl *data = nodeImplPtr(this);
	if (data->doc != NULL)
		return bxceDocumentHasAttributes(data->doc, data->node);
	return false;
}

} /* namespace Dom */
} /* namespace Xml */
} /* namespace sce */
