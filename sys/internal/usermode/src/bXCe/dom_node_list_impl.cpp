/*
 * ScebXCe's NodeListImpl: the C++ node list over the library's node list, its own or another
 * object's.
 */
#include "bxce_internal.h"

namespace bXCe {
namespace Dom {

NodeListImpl::NodeListImpl() : list(NULL), isReference(false)
{
	list = (BxceNodeList *)ScebXCe_B8DDE741(sizeof(BxceNodeList));
}

NodeListImpl::~NodeListImpl()
{
	if (list != NULL) {
		ScebXCe_1F59F0C4(list);
		if (list != NULL)
			ScebXCe_EC560647(list);
	}
	isReference = false;
	list = NULL;
}

NodeListImpl::NodeListImpl(BxceNodeList *list) : list(list), isReference(true)
{
}

int NodeListImpl::initialize(const Document *doc)
{
	if (list == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	bxceNodeListInitialize(list, doc->impl->document);
	return bxceConvertResult(0);
}

int NodeListImpl::terminate()
{
	if (list == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	ScebXCe_1F59F0C4(list);
	return bxceConvertResult(0);
}

BxceNodeId NodeListImpl::insertFirst(BxceNodeId node)
{
	if (list == NULL)
		return 0;
	bxceNodeListInsertFirst(list, node);
	return node;
}

BxceNodeId NodeListImpl::insertLast(BxceNodeId node)
{
	if (list == NULL)
		return 0;
	bxceNodeListInsertLast(list, node);
	return node;
}

BxceNodeId NodeListImpl::removeItem(BxceNodeId node)
{
	if (list == NULL)
		return 0;
	bxceNodeListRemoveItem(list, node);
	return 0;
}

BxceNodeId NodeListImpl::findItem(const char *name) const
{
	if (list == NULL)
		return 0;
	return bxceNodeListFindItemByName(list, name);
}

BxceNodeId NodeListImpl::findItem(BxceNodeId node) const
{
	if (list == NULL)
		return 0;
	return bxceNodeListFindItem(list, node);
}

int NodeListImpl::getLength() const
{
	if (list == NULL)
		return 0;
	return list->length;
}

BxceNodeId NodeListImpl::item(SceUInt32 index) const
{
	if (list == NULL)
		return 0;
	return ScebXCe_6C46D786(list, index);
}

} /* namespace Dom */
} /* namespace bXCe */
