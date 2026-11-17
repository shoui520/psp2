/*
 * An XML document read into a tree of elements with bXCe's SAX parser: the code SceLsdb shares
 * with the browser and the settings (NPXS10003, NPXS10015). Nothing names it; the names are
 * ours.
 */
#ifndef XML_TREE_H
#define XML_TREE_H

#include <std/string>
#include <std/map>
#include <std/list>

/* an element: its tag, its attributes, its text and its child elements */
struct XmlElement {
	typedef paf::map<paf::string, paf::string> AttrMap;

	paf::string name;
	AttrMap attrs;
	paf::string text;
	paf::list<XmlElement> children;
};

class XmlTreeBuilder;

/* a SAX parser that builds the tree under root as the document is read, a part at a time */
class XmlTreeParser {
public:
	XmlTreeParser(XmlElement *root, bool resolveEntity, bool resolveAttributeEntity);
	~XmlTreeParser();

	/* the next size bytes of the document, last for its end; 0, or bXCe's error */
	int Parse(const char *data, int size, bool last);

	XmlTreeBuilder *m_builder;
};

#endif
