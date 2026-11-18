/*
 * The XML tree: bXCe's SAX parser reads a document into XmlElements, a handler per level of
 * the document taking its events; and an element written back as XML. Nothing names it; the
 * names are ours.
 */
#include <std/string.h>
#include <std/vector>
#include <bXCe/bxce_internal.h>
#include "xml_tree.h"

PAF_VECTOR_INLINE_DTOR(XmlElement *)

/* the bXCe result that only says the parser wants more of the document */
#define XML_PARSE_CONTINUE	0x851001

/*
 * The handler of one level of the document: the start of an element there, its text and its
 * end, the handler of its children and the handler of the level above
 */
class XmlHandler {
public:
	XmlHandler(XmlHandler *parent) : m_parent(parent)
	{
	}

	virtual ~XmlHandler()
	{
	}

	virtual int StartElement(const char *name, int length, const bXCe::AttributeList *attrs)
	{
		return -1;
	}

	virtual int EndElement()
	{
		return 0;
	}

	virtual int Characters(const char *text, int length)
	{
		return 0;
	}

	virtual XmlHandler *GetChild(int index)
	{
		return NULL;
	}

	virtual XmlHandler *GetParent()
	{
		return m_parent;
	}

	XmlHandler *m_parent;
};

/* the elements under the root: each start pushes the element it reads, each end pops it */
class XmlElementHandler : public XmlHandler {
public:
	XmlElementHandler(XmlHandler *parent, XmlElement *root);

	virtual int StartElement(const char *name, int length, const bXCe::AttributeList *attrs);
	virtual int Characters(const char *text, int length);
	virtual XmlHandler *GetChild(int index);
	virtual XmlHandler *GetParent();

	paf::vector<XmlElement *> m_stack;	/* the element read, and those above it */
};

/* the document: its one child is the root element */
class XmlDocumentHandler : public XmlHandler {
public:
	XmlDocumentHandler(XmlElement *root) : XmlHandler(NULL), m_element(this, root)
	{
	}

	virtual XmlHandler *GetChild(int index);

	XmlElementHandler m_element;
	bool m_resolveEntity;
};

/* the parser's handler: passes each event to the handler of the level being read */
class XmlSaxHandler : public bXCe::Sax::DocumentHandler {
public:
	XmlSaxHandler(XmlHandler *handler) : m_default(NULL),
		m_current(handler != NULL ? handler : &m_default)
	{
	}

	virtual ~XmlSaxHandler();
	virtual int startDocument(void *userData);
	virtual int startElement(void *userData, const char *name, int length,
				 const bXCe::AttributeList *attrList);
	virtual int characters(void *userData, const char *ch, int length);
	virtual int endElement(void *userData, const char *name, int length);
	virtual int endDocument(void *userData);
	virtual int skippedText(void *userData, int tokenType, const char *text, int length);
	virtual int entityData(void *userData, int entityType, const char *name, int length);
	virtual void fatalError(void *userData, int errCode);

	XmlHandler m_default;		/* the handler above the document */
	XmlHandler *m_current;
};

/* what an XmlTreeParser keeps: the handlers and bXCe's parser */
class XmlTreeBuilder {
public:
	XmlTreeBuilder(XmlElement *root);

	~XmlTreeBuilder();

	/* bXCe's parser, set up to call the handlers; 0, or bXCe's error */
	int Init(bool resolveEntity, bool resolveAttributeEntity)
	{
		m_parser = new bXCe::Sax::Parser;
		if (m_parser == NULL)
			return -1;

		int result = m_parser->initialize(NULL);

		if (result < 0)
			return result;
		m_parser->setResolveEntity(resolveEntity);
		m_parser->setResolveAttributeEntity(resolveAttributeEntity);
		m_parser->setDocumentHandler(&m_sax);
		result = m_parser->reset();
		if (result < 0)
			return result;
		m_document.m_resolveEntity = resolveEntity;
		return 0;
	}

	XmlDocumentHandler m_document;
	XmlSaxHandler m_sax;
	bXCe::Sax::Parser *m_parser;
};

XmlSaxHandler::~XmlSaxHandler()
{
}

XmlTreeBuilder::~XmlTreeBuilder()
{
	if (m_parser != NULL) {
		m_parser->terminate();
		delete m_parser;
	}
}

XmlElementHandler::XmlElementHandler(XmlHandler *parent, XmlElement *root) : XmlHandler(parent)
{
	m_stack.push_back(root);
}

XmlTreeBuilder::XmlTreeBuilder(XmlElement *root) : m_document(root), m_sax(&m_document),
	m_parser(NULL)
{
}

XmlTreeParser::XmlTreeParser(XmlElement *root, bool resolveEntity, bool resolveAttributeEntity)
{
	m_builder = NULL;

	XmlTreeBuilder *builder = new XmlTreeBuilder(root);

	if (builder == NULL)
		return;
	if (builder->Init(resolveEntity, resolveAttributeEntity) != 0)
		delete builder;
	m_builder = builder;
}

__attribute__((noinline)) XmlTreeParser::~XmlTreeParser()
{
	if (m_builder != NULL)
		delete m_builder;
}

__attribute__((noinline)) int XmlTreeParser::Parse(const char *data, int size, bool last)
{
	if (m_builder == NULL)
		return -2;

	int result = m_builder->m_parser->parse(data, size, last);

	if (result == XML_PARSE_CONTINUE)
		return 0;
	return result;
}

int XmlSaxHandler::startDocument(void *userData)
{
	return 0;
}

/* the first handler under the current one that takes the element becomes the current one */
int XmlSaxHandler::startElement(void *userData, const char *name, int length,
				const bXCe::AttributeList *attrList)
{
	int index = 0;

	for (;;) {
		XmlHandler *child = m_current->GetChild(index);

		if (child == NULL)
			return -1;
		if (child->StartElement(name, length, attrList) == 0) {
			m_current = child;
			return 0;
		}
		index++;
	}
}

int XmlSaxHandler::characters(void *userData, const char *ch, int length)
{
	return m_current->Characters(ch, length);
}

int XmlSaxHandler::endElement(void *userData, const char *name, int length)
{
	int result = m_current->EndElement();

	if (result < 0)
		return result;
	m_current = m_current->GetParent();
	if (m_current == NULL)
		m_current = &m_default;
	return 0;
}

int XmlSaxHandler::endDocument(void *userData)
{
	return 0;
}

int XmlSaxHandler::skippedText(void *userData, int tokenType, const char *text, int length)
{
	return 0;
}

/* an entity the parser does not resolve stays in the text as it was written */
int XmlSaxHandler::entityData(void *userData, int entityType, const char *name, int length)
{
	m_current->Characters("&", 1);
	m_current->Characters(name, length);
	m_current->Characters(";", 1);
	return 0;
}

void XmlSaxHandler::fatalError(void *userData, int errCode)
{
}

/* the element read: its name and its attributes */
int XmlElementHandler::StartElement(const char *name, int length,
				    const bXCe::AttributeList *attrs)
{
	XmlElement *element = m_stack.back();

	element->name.assign(name, length);

	int count = attrs->getLength();
	int result = 0;

	for (int i = 0; i < count; i++) {
		const bXCe::Attr *attr = (const bXCe::Attr *)attrs->getAttribute(i);

		if (attr != NULL) {
			paf::string key(attr->getName(), attr->getNameLength());
			paf::string value(attr->getValue(), attr->getValueLength());

			element->attrs[key] = value;
		}
	}
	return result;
}

int XmlElementHandler::Characters(const char *text, int length)
{
	m_stack.back()->text.append(text, length);
	return 0;
}

/* a child of the element read: a new element at the end of its children, read next */
XmlHandler *XmlElementHandler::GetChild(int index)
{
	if (index != 0)
		return NULL;

	XmlElement *element = m_stack.back();

	element->children.push_back(XmlElement());
	m_stack.push_back(&element->children.back());
	return this;
}

/* the element's end: the one above it is read again (the level above after the root) */
XmlHandler *XmlElementHandler::GetParent()
{
	m_stack.pop_back();
	if (m_stack.size() == 0)
		return m_parent;
	return this;
}

XmlHandler *XmlDocumentHandler::GetChild(int index)
{
	if (index != 0)
		return NULL;
	return &m_element;
}

int XmlParseDocument(XmlElement *root, const char *xml)
{
	XmlTreeParser parser(root, true, true);
	int result = parser.Parse("<?xml version=\"1.0\" encoding=\"utf-8\"?>", 38, false);

	if (result < 0)
		return result;
	return parser.Parse(xml, sce_paf_strlen(xml), true);
}

/* text appended to out with XML's five characters as entities */
static void XmlAppendEscaped(paf::string *out, const char *text)
{
	for (int c = *text; c != '\0'; c = *++text) {
		const char *entity;

		switch (c) {
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
			entity = NULL;
			break;
		}
		if (entity != NULL)
			*out += entity;
		else
			out->append(1, c);
	}
}

int XmlWriteElement(paf::string *out, XmlElement *element, int depth)
{
	out->append(depth, ' ');
	out->append(1, '<');
	out->append(element->name);
	out->append(1, ' ');
	for (XmlElement::AttrMap::iterator it = element->attrs.begin();
	     it != element->attrs.end(); ++it) {
		out->append(it->first);
		*out += "=\"";
		XmlAppendEscaped(out, it->second.c_str());
		*out += "\" ";
	}
	if (element->text.size() != 0) {
		out->append(1, '>');
		XmlAppendEscaped(out, element->text.c_str());
		*out += "</";
		out->append(element->name);
		*out += ">\n";
		return 0;
	}
	if (element->children.size() == 0) {
		*out += "/>\n";
		return 0;
	}
	*out += ">\n";
	for (paf::list<XmlElement>::iterator child = element->children.begin();
	     child != element->children.end(); ++child) {
		int result = XmlWriteElement(out, &*child, depth + 1);

		if (result < 0)
			return result;
	}
	out->append(depth, ' ');
	*out += "</";
	out->append(element->name);
	*out += ">\n";
	return 0;
}
