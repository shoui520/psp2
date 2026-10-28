#include "cxml.h"
#include "../std/string.h"
#include "private.h"
#include "../paf_types.h"
#include "../memory/heap_allocator.h"
#include "../std/stdlib.h"

static const char cxmlMagic[] = "CXML";

static inline void CxmlHeaderInitialize(CxmlHeader *self)
{
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

int32_t CxmlDocumentClear(CxmlDocument *self)
{
	CxmlHeader header;

	CxmlHeaderInitialize(&header);
	self->header = header;
	if (self->access_mode != AccessMode_ReadOnly_Memory) {
		if (!self->allocator)
			return SCE_PAF_ERROR_CXML_NO_ALLOCATOR;
		if (self->tree)
			self->allocator(AllocationType_Free_Tree, self->allocator_userdata,
					self->tree, 0, NULL, NULL);
		self->tree = NULL;
		self->tree_capacity = 0;
		if (self->idtable)
			self->allocator(AllocationType_Free_IDTable, self->allocator_userdata,
					self->idtable, 0, NULL, NULL);
		self->idtable = NULL;
		self->idtable_capacity = 0;
		if (self->idhashtable)
			self->allocator(AllocationType_Free_IDHashTable, self->allocator_userdata,
					self->idhashtable, 0, NULL, NULL);
		self->idhashtable = NULL;
		self->idhashtable_capacity = 0;
		if (self->stringtable)
			self->allocator(AllocationType_Free_StringTable, self->allocator_userdata,
					self->stringtable, 0, NULL, NULL);
		self->stringtable = NULL;
		self->stringtable_capacity = 0;
		if (self->wstringtable)
			self->allocator(AllocationType_Free_WStringTable, self->allocator_userdata,
					self->wstringtable, 0, NULL, NULL);
		self->wstringtable = NULL;
		self->wstringtable_capacity = 0;
		if (self->hashtable)
			self->allocator(AllocationType_Free_HashTable, self->allocator_userdata,
					self->hashtable, 0, NULL, NULL);
		self->hashtable = NULL;
		self->hashtable_capacity = 0;
		if (self->intarraytable)
			self->allocator(AllocationType_Free_IntArrayTable, self->allocator_userdata,
					self->intarraytable, 0, NULL, NULL);
		self->intarraytable = NULL;
		self->intarraytable_capacity = 0;
		if (self->floatarraytable)
			self->allocator(AllocationType_Free_FloatArrayTable, self->allocator_userdata,
					self->floatarraytable, 0, NULL, NULL);
		self->floatarraytable = NULL;
		self->floatarraytable_capacity = 0;
		if (self->filetable)
			self->allocator(AllocationType_Free_FileTable, self->allocator_userdata,
					self->filetable, 0, NULL, NULL);
		self->filetable = NULL;
		self->filetable_capacity = 0;
	}
	self->access_mode = AccessMode_ReadWrite;
	return SCE_PAF_OK;
}

void CxmlDocumentGetDocumentElement(CxmlElement *result, CxmlDocument *self)
{
	uint32_t size = (uint32_t)self->header.tree_size;

	if (size >= sizeof(ElementBin)) {
		const ElementBin *element = (const ElementBin *)self->tree;

		if (((uint32_t)element->attr_num << 4) + sizeof(ElementBin) <= size) {
			result->doc = self;
			result->offset = 0;
			return;
		}
	}
	result->doc = NULL;
	result->offset = -1;
}

void CxmlDocumentGetHeaderMagic(const CxmlDocument *self, char buf[4])
{
	sce_paf_memcpy(buf, self->header.magic, sizeof(self->header.magic));
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

CxmlDocument *CxmlDocumentDestroy(CxmlDocument *self)
{
	CxmlDocumentClear(self);
	return self;
}

CxmlDocument *CxmlDocumentDestroyBase(CxmlDocument *self)
{
	CxmlDocumentClear(self);
	return self;
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

void CxmlDefaultAllocator(AllocationType type, void *userdata, void *old_addr,
			 size_t required_size, void **addr, size_t *size)
{
	HeapAllocator *allocator = userdata;

	switch (type) {
	case AllocationType_Alloc_Tree:
	case AllocationType_Alloc_IDTable:
	case AllocationType_Alloc_IDHashTable:
	case AllocationType_Alloc_StringTable:
	case AllocationType_Alloc_WStringTable:
	case AllocationType_Alloc_HashTable:
	case AllocationType_Alloc_IntArrayTable:
	case AllocationType_Alloc_FloatArrayTable:
	case AllocationType_Alloc_FileTable:
		if (allocator)
			*addr = scePafReallocWithContext(allocator, old_addr, required_size);
		else
			*addr = sce_paf_realloc(old_addr, required_size);
		*size = required_size;
		break;
	case AllocationType_Free_Tree:
	case AllocationType_Free_IDTable:
	case AllocationType_Free_IDHashTable:
	case AllocationType_Free_StringTable:
	case AllocationType_Free_WStringTable:
	case AllocationType_Free_HashTable:
	case AllocationType_Free_IntArrayTable:
	case AllocationType_Free_FloatArrayTable:
	case AllocationType_Free_FileTable:
		if (allocator)
			scePafFreeWithContext(allocator, old_addr);
		else
			sce_paf_free(old_addr);
		break;
	default:
		break;
	}
}
