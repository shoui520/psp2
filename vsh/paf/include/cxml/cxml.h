/*
 * paf::cxml, as far as converted C++ uses it: an element is a document and the
 * offset of the element in its tree. Names after ScePaf-RE.
 */
#ifndef PAF_CXML_CXML_H
#define PAF_CXML_CXML_H

#include <stddef.h>
#include <stdint.h>

namespace paf {
namespace cxml {

class Document;

/* an attribute of an element */
class Attribute {
public:
	Attribute() : doc(NULL), element_offset(-1), offset(-1)
	{
	}

	Document *doc;
	int32_t element_offset;
	int32_t offset;
};

class Element {
public:
	Element() : doc(NULL), offset(-1)
	{
	}

	Element GetParent() const;
	Element GetPreviousSibling() const;
	Element GetNextSibling() const;
	Element GetFirstChild() const;
	Element GetLastChild() const;
	const char *GetName() const;
	int32_t GetNumChild() const;
	int32_t NumAttribute() const;
	int32_t GetAttribute(int32_t index, Attribute *attr) const;

	Document *doc;
	int32_t offset;
};

namespace util {

Element FindChildElement(const Element &parent, const char *name);
Element FindChildElement(const Element &parent, const char *name, const char *idhashname,
			 uint32_t idhashvalue);

}

}
}

/* the cxml exports libpaf names in C */
extern "C" {
int32_t scePafResourceGetAttribute(const paf::cxml::Element *elm, const char *name,
				   paf::cxml::Attribute *attr);
int32_t scePafResourceGetInteger(const paf::cxml::Attribute *attr, int32_t *value);
int32_t scePafResourceGetAttributeIdIntLpb(const paf::cxml::Element *elm, const char *attrname,
					   uint32_t *value);
}

#endif
