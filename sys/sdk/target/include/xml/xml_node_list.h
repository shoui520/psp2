/*
 * The SDK's xml/xml_node_list.h: sce::Xml::Dom::NodeList interface
 * (Sony's names, SDK 3.570.011).
 */
#ifndef _SCE_XML_NODE_LIST_H
#define _SCE_XML_NODE_LIST_H

#include <xml/xml_result.h>
#include <xml/xml_node.h>
#include <xml/xml_types.h>

#ifdef __cplusplus
namespace sce
{
namespace Xml
{
namespace Dom
{

class Document;
class Node;
class NodeListImpl;

class NodeList
{
public:
	NodeList();
	NodeList(const NodeList &src);
	~NodeList();

	NodeList &operator=(const NodeList &src);

	int initialize(const Initializer *init);
	int terminate();

	void clear();
	size_t getLength() const;

	NodeId insertFirst(NodeId newNode);
	NodeId insertLast(NodeId newNode);
	NodeId removeItem(NodeId node);

	NodeId item(index_t index) const;
	NodeId operator[](index_t index) const;

	NodeId findItem(const String *name) const;
	NodeId findItem(NodeId node) const;

	bool isAvailable() const;

protected:
	friend class DocumentImpl;
	friend class Node;
	friend class NodeListImpl;
	NodeListImpl *impl;
};

} /* namespace Dom */
} /* namespace Xml */
} /* namespace sce */
#endif

#endif /* _SCE_XML_NODE_LIST_H */
