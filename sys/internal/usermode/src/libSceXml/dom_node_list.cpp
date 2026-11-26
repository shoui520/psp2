/*
 * dom_node_list.cpp: sce::Xml::Dom::NodeList and NodeListImpl
 * (0x81002BA0-0x81002FDB).
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{
namespace Dom
{

static NodeListImpl *createNodeListImpl(const Initializer *init)
{
	InitializerImpl *initImpl = InitializerImpl::get(init);
	if (initImpl == NULL)
		return NULL;

	void *mem = initImpl->param.allocator->allocate(sizeof(NodeListImpl),
							initImpl->param.userData);
	if (mem == NULL)
		return NULL;

	return new (mem) NodeListImpl(init);
}

NodeList::NodeList()
	: impl(NULL)
{
}

NodeList::NodeList(const NodeList &src)
	: impl(NULL)
{
	impl = createNodeListImpl(src.impl->initializer);
	if (impl != NULL)
		impl->cloneCore(*src.impl);
}

int NodeList::terminate()
{
	if (impl == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	const InitParameter *param = impl->getInitParam();
	impl->terminate();
	param->allocator->deallocate(impl, param->userData);
	impl = NULL;
	return SCE_XML_SUCCESS;
}

NodeList::~NodeList()
{
	if (impl != NULL) {
		const InitParameter *param = impl->getInitParam();
		impl->terminate();
		param->allocator->deallocate(impl, param->userData);
		impl = NULL;
	}
}

int NodeList::initialize(const Initializer *init)
{
	if (impl != NULL)
		return SCE_XML_INFO_MULTIPLE_INITIALIZED;

	impl = createNodeListImpl(init);
	if (impl == NULL)
		return SCE_XML_ERROR_NO_MEMORY;

	int ret = impl->initializeCore();
	if (ret != SCE_XML_SUCCESS && impl != NULL) {
		const InitParameter *param = impl->getInitParam();
		impl->terminate();
		param->allocator->deallocate(impl, param->userData);
		impl = NULL;
	}
	return ret;
}

void NodeList::clear()
{
	if (impl != NULL)
		impl->clear();
}

size_t NodeList::getLength() const
{
	if (impl != NULL)
		return impl->getLength();
	return 0;
}

NodeId NodeList::insertFirst(NodeId newNode)
{
	if (impl != NULL)
		return impl->insertFirst(newNode);
	return SCE_XML_INVALID_NODE_ID;
}

NodeId NodeList::insertLast(NodeId newNode)
{
	if (impl != NULL)
		return impl->insertLast(newNode);
	return SCE_XML_INVALID_NODE_ID;
}

NodeId NodeList::removeItem(NodeId node)
{
	if (impl != NULL)
		return impl->removeItem(node);
	return SCE_XML_INVALID_NODE_ID;
}

NodeId NodeList::item(index_t index) const
{
	if (impl != NULL)
		return impl->item(index);
	return SCE_XML_INVALID_NODE_ID;
}

NodeId NodeList::operator[](index_t index) const
{
	if (impl != NULL)
		return impl->item(index);
	return SCE_XML_INVALID_NODE_ID;
}

NodeId NodeList::findItem(const String *name) const
{
	if (impl != NULL)
		return impl->findItem(name);
	return SCE_XML_INVALID_NODE_ID;
}

NodeId NodeList::findItem(NodeId node) const
{
	if (impl != NULL)
		return impl->findItem(node);
	return SCE_XML_INVALID_NODE_ID;
}

bool NodeList::isAvailable() const
{
	if (impl != NULL)
		return impl->isCoreAvailable();
	return false;
}

} /* namespace Dom */
} /* namespace Xml */
} /* namespace sce */
