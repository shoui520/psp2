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

struct XmlElement;

/* a list of elements destroys each one through its allocator (below) */
namespace paf {
template<> inline list<XmlElement>::~list();
template<> inline __attribute__((noinline)) list<XmlElement>::iterator
list<XmlElement>::erase(iterator where);
}

/* an element: its tag, its attributes, its text and its child elements */
struct XmlElement {
	typedef paf::map<paf::string, paf::string> AttrMap;

	paf::string name;
	AttrMap attrs;
	paf::string text;
	paf::list<XmlElement> children;
};

/*
 * The allocator's destroy, which Sony's build keeps out of line (the element's destructor
 * destroys its own children through it)
 */
namespace paf {

template<> inline __attribute__((noinline)) void allocator<XmlElement>::destroy(XmlElement *ptr)
{
	ptr->~XmlElement();
}

template<> inline list<XmlElement>::~list()
{
	if (head != NULL) {
		_Node *node = head->next;

		if (node != head) {
			do {
				_Node *next = node->next;

				alloc.destroy(&node->value);
				operator delete(node);
				node = next;
			} while (node != head);
		}
		operator delete(head);
	}
	head = (_Node *)-1;
	count = (size_type)-1;
}

template<> inline __attribute__((noinline)) list<XmlElement>::iterator
list<XmlElement>::erase(iterator where)
{
	_Node *node = where.node;
	_Node *next = node->next;

	node->next->prev = node->prev;
	node->prev->next = node->next;
	alloc.destroy(&node->value);
	operator delete(node);
	--count;
	return iterator(next);
}

}

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
