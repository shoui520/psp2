#ifndef PAF_CXML_PRIVATE_H
#define PAF_CXML_PRIVATE_H

#include "cxml.h"

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

static inline const char *CxmlDocumentGetString(const CxmlDocument *self,
					       int32_t offset)
{
	if (offset < 0 || offset >= self->header.stringtable_size)
		return NULL;
	return self->stringtable + offset;
}

static inline const char *CxmlDocumentGetIDString(const CxmlDocument *self,
						 int32_t offset)
{
	if (offset < 0 || offset >= self->header.idtable_size)
		return NULL;
	return ((const IDBin *)(self->idtable + offset))->id;
}

static inline uint32_t CxmlDocumentGetIDHash(const CxmlDocument *self, int32_t offset)
{
	if (offset < 0 || offset >= self->header.idhashtable_size)
		return 0;
	return ((const IDHashBin *)(self->idhashtable + offset))->idhash;
}

#endif
