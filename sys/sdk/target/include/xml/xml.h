/*
 * The SDK's xml/xml.h: sce::Xml::Sax::Parser, sce::Xml::Dom::DocumentBuilder,
 * sce::Xml::Dom::Document, and sce::Xml::Util (Sony's names, SDK 3.570.011).
 */
#ifndef _SCE_XML_H
#define _SCE_XML_H

#include <xml/xml_types.h>

#ifdef __cplusplus
namespace sce
{
namespace Xml
{

namespace Sax
{

class DocumentHandler;
class ParserImpl;

class Parser
{
public:
	Parser();
	~Parser();

	int initialize(const Initializer *init);
	int terminate();

	void setDocumentHandler(DocumentHandler *processor);
	void setUserData(void *data);

	int parse(const XmlText *text, bool isFinal = true);
	int reset();

	void setSkipIgnorableWhiteSpace(bool isSkipped);
	void setResolveEntity(bool isResolved);

protected:
	ParserImpl *impl;

private:
	Parser(const Parser &);
	Parser &operator=(const Parser &);
};

} /* namespace Sax */

namespace Dom
{

class Document;
class DocumentBuilderImpl;

class DocumentBuilder
{
public:
	DocumentBuilder();
	~DocumentBuilder();

	int initialize(const Initializer *init);
	int terminate();

	int parse(const XmlText *text, bool isFinal = true);
	Document getDocument();

	void setSkipIgnorableWhiteSpace(bool isSkipped);
	void setSkipIgnorableText(bool isSkipped);
	void setResolveEntity(bool isResolved);

protected:
	DocumentBuilderImpl *impl;

private:
	DocumentBuilder(const DocumentBuilder &);
	DocumentBuilder &operator=(const DocumentBuilder &);
};

class DocumentImpl;

class Document
{
public:
	Document();
	Document(const Document &src);
	~Document();

	const Document &operator=(const Document &);

	int initialize(const Initializer *init);
	int terminate();

	NodeId getDocRoot() const;
	NodeId getRoot() const;
	NodeId getFirstChild(NodeId node) const;
	NodeId getSibling(NodeId node) const;
	String getNodeName(NodeId node) const;
	String getText(NodeId node) const;
	NodeId getFirstAttr(NodeId node) const;
	NodeId getNextAttr(NodeId node) const;
	String getAttrName(NodeId node) const;
	String getAttrValue(NodeId node) const;
	NodeId getParent(NodeId node) const;
	String getSkippedText(NodeId node) const;
	EntityType getEntityType(NodeId node) const;
	String getEntity(NodeId node) const;
	MetaNodeId getXmlMeta() const;
	NodeType getNodeType(NodeId node) const;

	int getElementsByTagName(NodeId node, const String *name, NodeList *nodelist) const;

	int getStatus() const;
	void resetStatus();
	bool isReadOnly() const;
	NodeId setWritable();

	NodeId addElementChild(NodeId parent, const String *name,
			       const AttributeList *list = 0, const String *text = 0);
	NodeId createElement(const String *name, const AttributeList *list = 0,
			     const String *text = 0);
	NodeId insertNode(NodeId parent, NodeId ref, NodeId child);
	NodeId createTextNode(const String *text);
	int recurseDelete(NodeId node);
	NodeId removeChild(NodeId child, NodeId parent);
	int importParent(const Document *parentDoc, NodeId parent);
	NodeId importNode(NodeId nparent, NodeId ref, const Document *doc, NodeId node);

	int setText(NodeId node, const String *text);
	int setAttrValue(NodeId node, const String *name, const String *value);
	int setAttribute(NodeId node, const String *name, const String *value);
	int setAttributeList(NodeId node, const AttributeList *list);
	int removeAttribute(NodeId node, const String *name);
	int removeAttributes(NodeId node);

	int getChildNodes(NodeId node, NodeList *nodelist) const;
	NodeId getLastChild(NodeId node) const;
	bool hasChildNodes(NodeId node) const;
	bool hasAttributes(NodeId node) const;
	String getAttribute(const NodeId node, const String *name) const;
	int getAttributes(NodeId node, NodeList *nodelist) const;

	int serialize(const SerializeParameter *param, XmlText *outputString);
	bool isAvailable() const;

protected:
	friend class DocumentImpl;
	friend class DocumentBuilderImpl;
	friend class Node;
	friend class NodeImpl;
	DocumentImpl *impl;
};

} /* namespace Dom */

namespace Util
{

const char *strResult(int result);

} /* namespace Util */

} /* namespace Xml */
} /* namespace sce */
#endif

#endif /* _SCE_XML_H */
