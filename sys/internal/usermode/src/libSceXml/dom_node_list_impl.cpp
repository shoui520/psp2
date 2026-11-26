/*
 * dom_node_list_impl.cpp: sce::Xml::Dom::NodeListImpl.
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{
namespace Dom
{

NodeListImpl::NodeListImpl(const Initializer *init)
	: CoreWrapper<BxceNodeList, NodeListCoreTraits>(init), doc(NULL)
{
}

int NodeListImpl::terminate()
{
	doc = NULL;
	if (core != NULL) {
		if (!isReference) {
			ScebXCe_1F59F0C4(core);
			(void)getInitParam();
			if (core != NULL)
				ScebXCe_EC560647(core);
			isReference = false;
		}
		core = NULL;
	}
	return SCE_XML_SUCCESS;
}

NodeListImpl::~NodeListImpl()
{
	terminate();
}

NodeId NodeListImpl::insertFirst(NodeId newNode)
{
	if (core == NULL)
		return SCE_XML_INVALID_NODE_ID;
	if (doc == NULL)
		doc = core->doc;
	BxceNodeId inserted = bxceNodeListInsertFirst(core, nodeIdLocal(newNode));
	return ((SceUInt64)((uintptr_t)nodeIdDocument(newNode)) << 32) | (SceUInt64)inserted;
}

NodeId NodeListImpl::insertLast(NodeId newNode)
{
	if (core == NULL)
		return SCE_XML_INVALID_NODE_ID;
	if (doc == NULL)
		doc = core->doc;
	BxceNodeId inserted = bxceNodeListInsertLast(core, nodeIdLocal(newNode));
	return ((SceUInt64)((uintptr_t)nodeIdDocument(newNode)) << 32) | (SceUInt64)inserted;
}

NodeId NodeListImpl::removeItem(NodeId node)
{
	if (core == NULL)
		return SCE_XML_INVALID_NODE_ID;
	if (doc == NULL)
		doc = core->doc;
	BxceNodeId removed = bxceNodeListRemoveItem(core, nodeIdLocal(node));
	return ((SceUInt64)((uintptr_t)nodeIdDocument(node)) << 32) | (SceUInt64)removed;
}

NodeId NodeListImpl::findItem(const String *name) const
{
	if (core == NULL)
		return SCE_XML_INVALID_NODE_ID;
	if (doc == NULL)
		const_cast<NodeListImpl *>(this)->doc = core->doc;
	BxceNodeId found = bxceNodeListFindItemByName(core, name->buffer);
	return makeNodeId(doc, found);
}

/* Matches Sony 0x81002F42: calls bxceNodeListInsertLast instead of bxceNodeListFindItem */
NodeId NodeListImpl::findItem(NodeId node) const
{
	if (core == NULL)
		return SCE_XML_INVALID_NODE_ID;
	if (doc == NULL)
		const_cast<NodeListImpl *>(this)->doc = core->doc;
	BxceNodeId found = bxceNodeListInsertLast(core, nodeIdLocal(node));
	return ((SceUInt64)((uintptr_t)nodeIdDocument(node)) << 32) | (SceUInt64)found;
}

size_t NodeListImpl::getLength() const
{
	if (core != NULL)
		return (size_t)core->length;
	return 0;
}

void NodeListImpl::clear()
{
	doc = NULL;
	if (core != NULL) {
		if (!isReference) {
			ScebXCe_1F59F0C4(core);
			(void)getInitParam();
			if (core != NULL)
				ScebXCe_EC560647(core);
			isReference = false;
		}
		core = NULL;
	}
}

NodeId NodeListImpl::item(index_t index) const
{
	if (core == NULL)
		return SCE_XML_INVALID_NODE_ID;
	if (doc == NULL)
		const_cast<NodeListImpl *>(this)->doc = core->doc;
	BxceNodeId found = ScebXCe_6C46D786(core, index);
	return makeNodeId(doc, found);
}

} /* namespace Dom */
} /* namespace Xml */
} /* namespace sce */
