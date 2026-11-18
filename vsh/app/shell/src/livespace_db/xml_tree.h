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
template<> inline __attribute__((noinline)) list<XmlElement>::list(const list &other);
template<> inline list<XmlElement>::~list();
template<> inline __attribute__((noinline)) void list<XmlElement>::push_back(const XmlElement &val);
template<> inline __attribute__((noinline)) list<XmlElement>::iterator
list<XmlElement>::erase(iterator where);
}

/* an element: its tag, its attributes, its text and its child elements */
struct XmlElement {
	typedef paf::map<paf::string, paf::string> AttrMap;

	/* the attribute name set to value */
	void SetAttribute(const char *name, const char *value);

	paf::string name;
	AttrMap attrs;
	paf::string text;
	paf::list<XmlElement> children;
};

/* an attribute by name, out of line where Sony's build calls it */
namespace paf {
template<> inline __attribute__((noinline)) string &
map<string, string>::operator[](const string &key)
{
	return this->_Insert(value_type(key, mapped_type()), NULL)->value.second;
}
}

/*
 * The allocator's destroy, which Sony's build keeps out of line (the element's destructor
 * destroys its own children through it)
 */
namespace paf {

template<> inline __attribute__((noinline)) void allocator<XmlElement>::destroy(XmlElement *ptr)
{
	ptr->~XmlElement();
}

/*
 * and its construct, which copies an element where the list puts it, member by member (the
 * copy inlined here as in Sony's build, not a copy constructor of its own)
 */
template<> inline __attribute__((noinline)) void
allocator<XmlElement>::construct(XmlElement *ptr, const XmlElement &val)
{
	new ((void *)&ptr->name) string(val.name);
	new ((void *)&ptr->attrs) XmlElement::AttrMap(val.attrs);
	new ((void *)&ptr->text) string(val.text);
	new ((void *)&ptr->children) list<XmlElement>(val.children);
}

/* a copy of a list of elements, each copied through the allocator */
template<> inline __attribute__((noinline)) list<XmlElement>::list(const list &other)
{
	count = 0;

	_Node *node = (_Node *)operator new(sizeof(_Node));

	if (node == NULL)
		ScePafStdc_D608246A();
	head = node;
	if (node != NULL) {
		_Node *tail = node;
		_Node *src = other.head != NULL ? other.head->next : NULL;

		while (src != other.head) {
			_Node *copy = (_Node *)operator new(sizeof(_Node));

			if (copy == NULL)
				ScePafStdc_D608246A();
			if (copy != NULL)
				alloc.construct(&copy->value, src->value);
			else
				copy = head;
			if (copy == head)
				break;
			copy->prev = tail;
			tail->next = copy;
			tail = copy;
			++count;
			if (src != NULL)
				src = src->next;
		}
		tail->next = head;
		head->prev = tail;
	}
}

/* out of line, the element copied in place (not through the allocator's construct) */
template<> inline __attribute__((noinline)) void list<XmlElement>::push_back(const XmlElement &val)
{
	if (head == NULL)
		return;

	_Node *node = (_Node *)operator new(sizeof(_Node));

	if (node == NULL)
		ScePafStdc_D608246A();
	if (node != NULL)
		new ((void *)&node->value) XmlElement(val);
	else
		node = head;
	if (node != head) {
		node->prev = head->prev;
		node->next = head;
		head->prev->next = node;
		head->prev = node;
		++count;
	}
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

/* an element written as XML at depth (one space each), appended to out; 0, or an error */
int XmlWriteElement(paf::string *out, XmlElement *element, int depth);

/* a document (after its XML declaration) read into the tree under root; 0, or bXCe's error */
int XmlParseDocument(XmlElement *root, const char *xml);

#endif
