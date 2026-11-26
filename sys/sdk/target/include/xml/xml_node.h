/*
 * The SDK's xml/xml_node.h: sce::Xml::Dom::Node interface (Sony's names, SDK 3.570.011).
 */
#ifndef _SCE_XML_NODE_H
#define _SCE_XML_NODE_H

#include <xml/xml_types.h>

#ifdef __cplusplus
namespace sce
{
namespace Xml
{
namespace Dom
{

class Document;
class NodeList;
class NodeImpl;

class Node
{
public:
	Node(NodeId id);
	Node(const Node &src);
	~Node();

	Node &operator=(const Node &src);

	String getNodeName() const;
	String getNodeValue() const;
	NodeType getNodeType() const;

	NodeId getParentNode() const;
	NodeList *getChildNodes() const;
	NodeId getFirstChild() const;
	NodeId getLastChild() const;
	NodeId getNextSibling() const;
	NodeList *getAttributes() const;
	Document *getOwnerDocument() const;

	NodeId insertBefore(NodeId newChild, NodeId refChild);
	NodeId removeChild(NodeId oldChild);
	NodeId appendChild(NodeId newChild);

	bool hasChildNodes() const;
	bool hasAttributes() const;
	bool isAvailable() const;

private:
	friend class NodeImpl;
	char impl[64];
};

} /* namespace Dom */
} /* namespace Xml */
} /* namespace sce */
#endif

#endif /* _SCE_XML_NODE_H */
