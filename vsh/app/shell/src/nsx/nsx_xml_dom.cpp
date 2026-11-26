/*
 * nsx_xml_dom.cpp: bXCe SAX-to-DOM tree builder, XML node lookup helpers,
 * entity escaper, and pretty-printer shared by SceShell NSX and XML readers.
 * 3.65-CEX: 0x81389C1A - 0x8138B15F
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <bXCe/bxce_internal.h>

#include "nsx_internal.h"

#define BXCE_SAX_PARSE_DONE			8720385		/* 0x00851001 */

static paf::string s_emptyAttrValue;				/* 0x815CA878 */
static NsxXmlNode s_emptyXmlNode;				/* 0x815CA848 */

/* Hierarchical SAX element handler base */
class XmlElementHandler {
public:
	virtual ~XmlElementHandler() {}
	virtual int OnStartElement(const char *name, int length,
				   const bXCe::AttributeList *attrList) = 0;
	virtual int OnEndElement() = 0;
	virtual int OnCharacters(const char *ch, int length) = 0;
	virtual XmlElementHandler *GetChildHandler(int index) = 0;
	virtual XmlElementHandler *GetParentHandler() = 0;
};

/* Dispatches bXCe SAX callbacks through a stack of XmlElementHandlers */
class XmlSaxDispatchAdapter : public bXCe::Sax::DocumentHandler {
public:
	XmlSaxDispatchAdapter(XmlElementHandler *rootHandler)
		: m_currentHandler(rootHandler),
		  m_fallbackHandler(rootHandler)
	{
	}

	virtual ~XmlSaxDispatchAdapter() {}

	/* 0x81389F6A */
	virtual int startElement(void *userData, const char *name, int length,
				 const bXCe::AttributeList *attrList)
	{
		(void)userData;
		int childIndex = 0;
		for (;;) {
			XmlElementHandler *child = m_currentHandler->GetChildHandler(childIndex);
			if (child == NULL)
				break;
			if (child->OnStartElement(name, length, attrList) == 0) {
				m_currentHandler = child;
				return 0;
			}
			++childIndex;
		}
		return -1;
	}

	/* 0x81389FB4 */
	virtual int characters(void *userData, const char *ch, int length)
	{
		(void)userData;
		return m_currentHandler->OnCharacters(ch, length);
	}

	/* 0x81389FC4 */
	virtual int endElement(void *userData, const char *name, int length)
	{
		(void)userData;
		(void)name;
		(void)length;
		int ret = m_currentHandler->OnEndElement();
		if (ret < 0)
			return ret;

		XmlElementHandler *parent = m_currentHandler->GetParentHandler();
		m_currentHandler = (parent != NULL) ? parent : m_fallbackHandler;
		return 0;
	}

	/* 0x81389FEC */
	virtual int entityData(void *userData, int entityType, const char *name, int length)
	{
		(void)userData;
		(void)entityType;
		m_currentHandler->OnCharacters("&", 1);
		m_currentHandler->OnCharacters(name, length);
		m_currentHandler->OnCharacters(";", 1);
		return 0;
	}

private:
	XmlElementHandler *m_currentHandler;
	XmlElementHandler *m_fallbackHandler;
};

/* Recursive DOM node handler building NsxXmlNode trees */
class XmlDomTreeBuilder : public XmlElementHandler {
public:
	/* 0x81389DDA */
	XmlDomTreeBuilder(XmlElementHandler *parentHandler, NsxXmlNode *rootNode)
		: m_parentHandler(parentHandler),
		  m_nodeStack()
	{
		m_nodeStack.reserve(10);
		m_nodeStack.push_back(rootNode);
	}

	virtual ~XmlDomTreeBuilder() {}

	/* 0x8138A02A */
	virtual int OnStartElement(const char *name, int length,
				   const bXCe::AttributeList *attrList)
	{
		NsxXmlNode *current = m_nodeStack.back();
		current->name.assign(name, static_cast<size_t>(length));

		int attrCount = attrList->getLength();
		for (int i = 0; i < attrCount; ++i) {
			const BxceAttr *attr = attrList->getAttribute(static_cast<SceSize>(i));
			if (attr == NULL)
				continue;
			paf::string attrKey(attr->name, static_cast<size_t>(attr->nameLength));
			paf::string attrVal(attr->value, static_cast<size_t>(attr->valueLength));
			current->attrs[attrKey] = attrVal;
		}
		return 0;
	}

	/* 0x8138A122 */
	virtual int OnCharacters(const char *ch, int length)
	{
		NsxXmlNode *current = m_nodeStack.back();
		current->text.append(ch, static_cast<size_t>(length));
		return 0;
	}

	virtual int OnEndElement()
	{
		return 0;
	}

	/* 0x8138A18A */
	virtual XmlElementHandler *GetChildHandler(int index)
	{
		if (index != 0)
			return NULL;

		NsxXmlNode *current = m_nodeStack.back();
		current->children.push_back(NsxXmlNode());
		m_nodeStack.push_back(&current->children.back());
		return this;
	}

	/* 0x8138AA24 */
	virtual XmlElementHandler *GetParentHandler()
	{
		if (!m_nodeStack.empty())
			m_nodeStack.pop_back();
		return m_nodeStack.empty() ? m_parentHandler : this;
	}

private:
	XmlElementHandler *m_parentHandler;
	paf::vector<NsxXmlNode *> m_nodeStack;
};

/* Root handler owning the SAX parser, dispatch adapter, and tree builder */
class XmlSaxParserContext : public XmlElementHandler {
public:
	/* 0x81389E24 */
	XmlSaxParserContext(NsxXmlNode *rootNode)
		: m_treeBuilder(this, rootNode),
		  m_resolveEntity(false),
		  m_dispatchAdapter(this),
		  m_saxParser(NULL)
	{
	}

	/* 0x81389D74 */
	virtual ~XmlSaxParserContext()
	{
		if (m_saxParser != NULL) {
			m_saxParser->terminate();
			delete m_saxParser;
			m_saxParser = NULL;
		}
	}

	virtual int OnStartElement(const char *name, int length,
				   const bXCe::AttributeList *attrList)
	{
		(void)name;
		(void)length;
		(void)attrList;
		return 0;
	}

	virtual int OnEndElement()
	{
		return 0;
	}

	virtual int OnCharacters(const char *ch, int length)
	{
		(void)ch;
		(void)length;
		return 0;
	}

	/* 0x8138AA50 */
	virtual XmlElementHandler *GetChildHandler(int index)
	{
		return (index == 0) ? &m_treeBuilder : NULL;
	}

	virtual XmlElementHandler *GetParentHandler()
	{
		return NULL;
	}

	bool Initialize(bool resolveEntity, bool resolveAttrEntity)
	{
		m_saxParser = new bXCe::Sax::Parser();
		if (m_saxParser == NULL)
			return false;
		if (m_saxParser->initialize(NULL) < 0)
			return false;

		m_saxParser->setResolveEntity(resolveEntity);
		m_saxParser->setResolveAttributeEntity(resolveAttrEntity);
		m_saxParser->setDocumentHandler(&m_dispatchAdapter);
		if (m_saxParser->reset() < 0)
			return false;

		m_resolveEntity = resolveEntity;
		return true;
	}

	int ParseChunk(const char *chunk, SceSize size, bool isFinal)
	{
		if (m_saxParser == NULL)
			return -2;
		int ret = m_saxParser->parse(chunk, static_cast<int>(size), isFinal);
		if (ret == BXCE_SAX_PARSE_DONE)
			return 0;
		return ret;
	}

private:
	XmlDomTreeBuilder m_treeBuilder;
	bool m_resolveEntity;
	XmlSaxDispatchAdapter m_dispatchAdapter;
	bXCe::Sax::Parser *m_saxParser;
};

/* 0x81389C1A */
const paf::string *shellXmlNodeGetAttr(const NsxXmlNode *node, const char *attrName)
{
	NsxXmlNode *mutableNode = const_cast<NsxXmlNode *>(node);
	paf::string key(attrName != NULL ? attrName : "");
	paf::map<paf::string, paf::string>::iterator it = mutableNode->attrs.find(key);
	if (it == mutableNode->attrs.end())
		return &s_emptyAttrValue;
	return &it->second;
}

/* 0x81389CA0 */
const NsxXmlNode *shellXmlNodeGetChild(const NsxXmlNode *node, const char *childName)
{
	NsxXmlNode *mutableNode = const_cast<NsxXmlNode *>(node);
	for (paf::list<NsxXmlNode>::iterator it = mutableNode->children.begin();
	     it != mutableNode->children.end(); ++it) {
		const char *name = it->name.c_str();
		if (name[0] == childName[0] && sce_paf_strcmp(name, childName) == 0)
			return &(*it);
	}
	return &s_emptyXmlNode;
}

/* 0x81389E78 */
NsxXmlSaxParser *shellXmlSaxCreate(NsxXmlSaxParser *parser, NsxXmlNode *rootNode,
				   bool resolveEntity, bool resolveAttrEntity)
{
	parser->impl = NULL;
	XmlSaxParserContext *context = new XmlSaxParserContext(rootNode);
	if (context == NULL)
		return parser;

	if (!context->Initialize(resolveEntity, resolveAttrEntity)) {
		delete context;
		return parser;
	}

	parser->impl = context;
	return parser;
}

/* 0x81389F08 */
NsxXmlSaxParser *shellXmlSaxDestroy(NsxXmlSaxParser *parser)
{
	XmlSaxParserContext *context = static_cast<XmlSaxParserContext *>(parser->impl);
	if (context != NULL) {
		delete context;
		parser->impl = NULL;
	}
	return parser;
}

/* 0x81389F22 */
int shellXmlSaxParse(NsxXmlSaxParser *parser, const char *chunk, SceSize size, bool isFinal)
{
	XmlSaxParserContext *context = static_cast<XmlSaxParserContext *>(parser->impl);
	if (context == NULL)
		return -2;
	return context->ParseChunk(chunk, size, isFinal);
}

/* 0x8138AA5C */
void shellXmlAppendEscaped(paf::string *outXml, const char *text)
{
	for (const char *cursor = text; *cursor != '\0'; ++cursor) {
		const char *entity = NULL;
		switch (*cursor) {
		case '"':
			entity = "&quot;";
			break;
		case '&':
			entity = "&amp;";
			break;
		case '\'':
			entity = "&apos;";
			break;
		case '<':
			entity = "&lt;";
			break;
		case '>':
			entity = "&gt;";
			break;
		default:
			break;
		}
		if (entity != NULL)
			outXml->append(entity);
		else
			outXml->append(1u, *cursor);
	}
}

/* 0x8138AB3C */
int shellXmlNodeSerialize(paf::string *outXml, const NsxXmlNode *node, SceUInt32 indent)
{
	NsxXmlNode *mutableNode = const_cast<NsxXmlNode *>(node);
	outXml->append(indent, ' ');
	outXml->append(1u, '<');
	outXml->append(node->name);
	outXml->append(1u, ' ');

	for (paf::map<paf::string, paf::string>::iterator it = mutableNode->attrs.begin();
	     it != mutableNode->attrs.end(); ++it) {
		outXml->append(it->first);
		outXml->append("=\"");
		shellXmlAppendEscaped(outXml, it->second.c_str());
		outXml->append("\" ");
	}

	if (!node->text.empty()) {
		outXml->append(1u, '>');
		shellXmlAppendEscaped(outXml, node->text.c_str());
		outXml->append("</");
		outXml->append(node->name);
		outXml->append(">\n");
		return 0;
	}

	if (mutableNode->children.size() == 0) {
		outXml->append("/>\n");
		return 0;
	}

	outXml->append(">\n");
	for (paf::list<NsxXmlNode>::iterator it = mutableNode->children.begin();
	     it != mutableNode->children.end(); ++it) {
		int ret = shellXmlNodeSerialize(outXml, &(*it), indent + 1);
		if (ret < 0)
			return ret;
	}

	outXml->append(indent, ' ');
	outXml->append("</");
	outXml->append(node->name);
	outXml->append(">\n");
	return 0;
}
