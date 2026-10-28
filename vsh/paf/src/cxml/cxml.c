#include "cxml.h"

static const char cxmlMagic[] = "CXML";

static inline void CxmlHeaderInitialize(CxmlHeader *self)
{
	extern void *sce_paf_memcpy(void *dst, const void *src, size_t size);

	self->version = 0x110;
	self->tree_offset = 0;
	self->tree_size = 0;
	self->idtable_offset = 0;
	self->idtable_size = 0;
	self->idhashtable_offset = 0;
	self->idhashtable_size = 0;
	self->stringtable_offset = 0;
	self->stringtable_size = 0;
	self->wstringtable_offset = 0;
	self->wstringtable_size = 0;
	self->hashtable_offset = 0;
	self->hashtable_size = 0;
	self->intarraytable_offset = 0;
	self->intarraytable_size = 0;
	self->floatarraytable_offset = 0;
	self->floatarraytable_size = 0;
	self->filetable_offset = 0;
	self->filetable_size = 0;
	sce_paf_memcpy(self->magic, cxmlMagic, sizeof(self->magic));
}

CxmlHeader *CxmlHeaderConstruct(CxmlHeader *self)
{
	CxmlHeaderInitialize(self);
	return self;
}

CxmlHeader *CxmlHeaderConstructBase(CxmlHeader *self)
{
	CxmlHeaderInitialize(self);
	return self;
}

CxmlDocument *CxmlDocumentConstruct(CxmlDocument *self)
{
	self->access_mode = AccessMode_ReadWrite;
	CxmlHeaderInitialize(&self->header);
	self->tree = NULL;
	self->tree_capacity = 0;
	self->idtable = NULL;
	self->idtable_capacity = 0;
	self->idhashtable = NULL;
	self->idhashtable_capacity = 0;
	self->stringtable = NULL;
	self->stringtable_capacity = 0;
	self->wstringtable = NULL;
	self->wstringtable_capacity = 0;
	self->hashtable = NULL;
	self->hashtable_capacity = 0;
	self->intarraytable = NULL;
	self->intarraytable_capacity = 0;
	self->floatarraytable = NULL;
	self->floatarraytable_capacity = 0;
	self->filetable = NULL;
	self->filetable_capacity = 0;
	self->allocator = CxmlDefaultAllocator;
	self->allocator_userdata = NULL;
	return self;
}

CxmlDocument *CxmlDocumentConstructBase(CxmlDocument *self)
{
	return CxmlDocumentConstruct(self);
}

static inline int CxmlDocumentIsValidElement(const CxmlDocument *self,
					    int32_t offset)
{
	const ElementBin *element;
	uint32_t size;

	if (offset < 0)
		return 0;
	size = (uint32_t)self->header.tree_size;
	if ((uint32_t)offset + sizeof(ElementBin) > size)
		return 0;
	element = (const ElementBin *)(self->tree + offset);
	return (uint32_t)offset + ((uint32_t)element->attr_num << 4) +
		sizeof(ElementBin) <= size;
}

void CxmlElementGetParent(CxmlElement *result, const CxmlElement *self)
{
	CxmlDocument *doc = self->doc;

	if (doc) {
		const ElementBin *element = (const ElementBin *)(doc->tree + self->offset);
		int32_t offset = element->parent;

		if (CxmlDocumentIsValidElement(doc, offset)) {
			result->doc = doc;
			result->offset = offset;
			return;
		}
	}
	result->doc = NULL;
	result->offset = -1;
}

void CxmlElementGetPreviousSibling(CxmlElement *result, const CxmlElement *self)
{
	CxmlDocument *doc = self->doc;

	if (doc) {
		const ElementBin *element = (const ElementBin *)(doc->tree + self->offset);
		int32_t offset = element->prev;

		if (CxmlDocumentIsValidElement(doc, offset)) {
			result->doc = doc;
			result->offset = offset;
			return;
		}
	}
	result->doc = NULL;
	result->offset = -1;
}

void CxmlElementGetNextSibling(CxmlElement *result, const CxmlElement *self)
{
	CxmlDocument *doc = self->doc;

	if (doc) {
		const ElementBin *element = (const ElementBin *)(doc->tree + self->offset);
		int32_t offset = element->next;

		if (CxmlDocumentIsValidElement(doc, offset)) {
			result->doc = doc;
			result->offset = offset;
			return;
		}
	}
	result->doc = NULL;
	result->offset = -1;
}

void CxmlElementGetFirstChild(CxmlElement *result, const CxmlElement *self)
{
	CxmlDocument *doc = self->doc;

	if (doc) {
		const ElementBin *element = (const ElementBin *)(doc->tree + self->offset);
		int32_t offset = element->first_child;

		if (CxmlDocumentIsValidElement(doc, offset)) {
			result->doc = doc;
			result->offset = offset;
			return;
		}
	}
	result->doc = NULL;
	result->offset = -1;
}

void CxmlElementGetLastChild(CxmlElement *result, const CxmlElement *self)
{
	CxmlDocument *doc = self->doc;

	if (doc) {
		const ElementBin *element = (const ElementBin *)(doc->tree + self->offset);
		int32_t offset = element->last_child;

		if (CxmlDocumentIsValidElement(doc, offset)) {
			result->doc = doc;
			result->offset = offset;
			return;
		}
	}
	result->doc = NULL;
	result->offset = -1;
}

const char *CxmlElementGetName(const CxmlElement *self)
{
	const CxmlDocument *doc = self->doc;
	const ElementBin *element;
	int32_t offset;

	if (!doc)
		return NULL;
	element = (const ElementBin *)(doc->tree + self->offset);
	offset = element->name;
	if (offset < 0 || offset >= doc->header.stringtable_size)
		return NULL;
	return doc->stringtable + offset;
}

int32_t CxmlElementGetNumChild(const CxmlElement *self)
{
	CxmlElement child, next;
	uint32_t count = 0;

	CxmlElementGetFirstChild(&child, self);
	while (child.doc && child.offset >= 0) {
		count++;
		CxmlElementGetNextSibling(&next, &child);
		child = next;
	}
	return (int32_t)count;
}

int32_t CxmlElementNumAttribute(const CxmlElement *self)
{
	if (self->doc) {
		const ElementBin *element = (const ElementBin *)(self->doc->tree + self->offset);

		return element->attr_num;
	}
	return 0;
}
