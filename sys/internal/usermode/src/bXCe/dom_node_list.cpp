/*
 * ScebXCe's NodeList: the C++ interface object of a node list, over a NodeListImpl.
 */
#include "bxce_internal.h"

namespace bXCe {
namespace Dom {

NodeList::NodeList() : impl(new NodeListImpl())
{
}

NodeList::~NodeList()
{
	if (impl != NULL) {
		delete impl;
		impl = NULL;
	}
}

/* a node list over the library's list, which it does not own */
NodeList::NodeList(BxceNodeList *list) : impl(new NodeListImpl(list))
{
}

int NodeList::initialize(const Document *doc)
{
	if (impl == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return impl->initialize(doc);
}

int NodeList::terminate()
{
	if (impl == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return impl->terminate();
}

/* empties the list and initializes it again for its document */
void NodeList::clear()
{
	NodeListImpl *p = impl;

	if (p != NULL) {
		p->terminate();
		p->initialize(p->doc);
	}
}

int NodeList::getLength() const
{
	if (impl == NULL)
		return 0;
	return impl->getLength();
}

BxceNodeId NodeList::insertFirst(BxceNodeId node)
{
	if (impl == NULL)
		return 0;
	return impl->insertFirst(node);
}

BxceNodeId NodeList::insertLast(BxceNodeId node)
{
	if (impl == NULL)
		return 0;
	return impl->insertLast(node);
}

BxceNodeId NodeList::removeItem(BxceNodeId node)
{
	if (impl == NULL)
		return 0;
	return impl->removeItem(node);
}

BxceNodeId NodeList::item(SceUInt32 index) const
{
	if (impl == NULL)
		return 0;
	return impl->item(index);
}

BxceNodeId NodeList::operator[](SceUInt32 index) const
{
	BxceNodeId node = 0;

	if (impl != NULL)
		node = impl->item(index);
	return node;
}

BxceNodeId NodeList::findItem(const char *name) const
{
	if (impl == NULL)
		return 0;
	return impl->findItem(name);
}

BxceNodeId NodeList::findItem(BxceNodeId node) const
{
	if (impl == NULL)
		return 0;
	return impl->findItem(node);
}

} /* namespace Dom */
} /* namespace bXCe */
