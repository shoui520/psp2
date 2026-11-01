/*
 * paf::cxml, as far as converted C++ uses it: an element is a document and the
 * offset of the element in its tree. Names after ScePaf-RE.
 */
#ifndef PAF_CXML_CXML_H
#define PAF_CXML_CXML_H

#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>

namespace paf {

class File;

namespace cxml {

class Document;

/* what a document allocates */
enum AllocationType {
	AllocationType_Alloc_Tree = 0,
	AllocationType_Alloc_IDTable,
	AllocationType_Alloc_IDHashTable,
	AllocationType_Alloc_StringTable,
	AllocationType_Alloc_WStringTable,
	AllocationType_Alloc_HashTable,
	AllocationType_Alloc_IntArrayTable,
	AllocationType_Alloc_FloatArrayTable,
	AllocationType_Alloc_FileTable,
	AllocationType_Free_Tree,
	AllocationType_Free_IDTable,
	AllocationType_Free_IDHashTable,
	AllocationType_Free_StringTable,
	AllocationType_Free_WStringTable,
	AllocationType_Free_HashTable,
	AllocationType_Free_IntArrayTable,
	AllocationType_Free_FloatArrayTable,
	AllocationType_Free_FileTable
};

typedef void (*AllocatorFunction)(AllocationType type, void *userdata, void *old_addr,
				  size_t required_size, void **addr, size_t *size);

void DefaultAllocator(AllocationType type, void *userdata, void *old_addr, size_t required_size,
		      void **addr, size_t *size);

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

/* a file attribute's data: in the document's file table, read through a file when needed */
class File {
public:
	File() : doc(NULL), offset(-1), size(0)
	{
	}

	const void *GetAddr() const;
	int32_t Load(common::SharedPtr<paf::File> fp, void *buf, uint32_t size) const;

	Document *doc;
	int32_t offset;
	int32_t size;
};

namespace util {

bool CheckElementName(const Element &elm, const char *name);
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
/* the ID hash of an attribute, and the element it refers to */
int32_t scePafResourceGetAttributeIdInt(const paf::cxml::Element *elm, const char *attrname,
					uint32_t *value, paf::cxml::Element *entity);
int32_t scePafResourceGetAttributeInteger(const paf::cxml::Element *elm, const char *attrname,
					  int32_t *value);
int32_t scePafResourceGetAttributeString(const paf::cxml::Element *elm, const char *attrname,
					 const char **value, uint32_t *len);
int32_t scePafResourceGetAttributeStyleId(const paf::cxml::Element *elm, const char *attrname,
					  uint32_t *value);
int32_t scePafResourceGetAttributeIntegerArray(const paf::cxml::Element *elm,
					       const char *attrname, const int32_t **addr,
					       uint32_t *num);
int32_t scePafResourceGetAttributeFloatArray(const paf::cxml::Element *elm, const char *attrname,
					     const float **addr, uint32_t *num);
}

#endif
