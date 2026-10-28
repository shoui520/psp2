#include "private.h"
#include "../paf_types.h"

int32_t CxmlElementGetAttributeByIndex(const CxmlElement *self, int32_t index,
				      CxmlAttribute *attr)
{
	CxmlDocument *doc = self->doc;
	const ElementBin *element;
	int32_t offset;

	if (!doc)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	offset = self->offset;
	element = (const ElementBin *)(doc->tree + offset);
	if ((uint32_t)index >= (uint32_t)element->attr_num)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	attr->doc = doc;
	attr->element_offset = offset;
	attr->offset = (uint32_t)offset + sizeof(ElementBin) +
		(uint32_t)index * sizeof(AttributeBin);
	return SCE_PAF_OK;
}

int32_t CxmlElementGetAttributeByName(const CxmlElement *self, const char *name,
				     CxmlAttribute *attr)
{
	extern int sce_paf_strcmp(const char *left, const char *right);
	CxmlDocument *doc = self->doc;
	const ElementBin *element;
	int32_t offset;
	uint32_t i;

	if (!doc)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	offset = self->offset;
	element = (const ElementBin *)(doc->tree + offset);
	for (i = 0; i < (uint32_t)element->attr_num; i++) {
		const AttributeBin *bin = (const AttributeBin *)(doc->tree +
			(uint32_t)offset + sizeof(ElementBin) + i * sizeof(AttributeBin));
		const char *attr_name = CxmlDocumentGetString(doc, bin->name);

		if (attr_name && *attr_name == *name) {
			int ret = sce_paf_strcmp(attr_name, name);

			doc = self->doc;
			offset = self->offset;
			if (!ret) {
				attr->doc = doc;
				attr->element_offset = offset;
				attr->offset = (uint32_t)offset + sizeof(ElementBin) +
					i * sizeof(AttributeBin);
				return SCE_PAF_OK;
			}
		}
	}
	return SCE_PAF_ERROR_CXML_NOT_EXIST;
}

const char *CxmlAttributeGetName(const CxmlAttribute *self)
{
	const AttributeBin *bin;

	if (!self->doc)
		return NULL;
	bin = (const AttributeBin *)(self->doc->tree + self->offset);
	return CxmlDocumentGetString(self->doc, bin->name);
}

AttributeType CxmlAttributeGetType(const CxmlAttribute *self)
{
	const AttributeBin *bin;

	if (!self->doc)
		return AttributeType_None;
	bin = (const AttributeBin *)(self->doc->tree + self->offset);
	return (AttributeType)bin->type;
}

int32_t CxmlAttributeGetInt(const CxmlAttribute *self, int32_t *value)
{
	const AttributeBin *bin;

	if (!self->doc)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	bin = (const AttributeBin *)(self->doc->tree + self->offset);
	if (bin->type != AttributeType_Int)
		return SCE_PAF_ERROR_CXML_TYPE;
	*value = bin->i;
	return SCE_PAF_OK;
}

int32_t CxmlAttributeGetFloat(const CxmlAttribute *self, float *value)
{
	const AttributeBin *bin;

	if (!self->doc)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	bin = (const AttributeBin *)(self->doc->tree + self->offset);
	if (bin->type != AttributeType_Float)
		return SCE_PAF_ERROR_CXML_TYPE;
	*value = bin->f;
	return SCE_PAF_OK;
}

int32_t CxmlAttributeGetString(const CxmlAttribute *self,
			      const char **value, uint32_t *len)
{
	const AttributeBin *bin;

	if (!self->doc)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	bin = (const AttributeBin *)(self->doc->tree + self->offset);
	if (bin->type != AttributeType_String)
		return SCE_PAF_ERROR_CXML_TYPE;
	*value = CxmlDocumentGetString(self->doc, bin->s.offset);
	if (!*value)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	if ((*value)[bin->s.len])
		return SCE_PAF_ERROR_CXML_FORMAT;
	*len = bin->s.len;
	return SCE_PAF_OK;
}

int32_t CxmlAttributeGetWString(const CxmlAttribute *self,
			       const uint16_t **value, uint32_t *len)
{
	const CxmlDocument *doc = self->doc;
	const AttributeBin *bin;
	int32_t offset;

	if (!doc)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	bin = (const AttributeBin *)(doc->tree + self->offset);
	if (bin->type != AttributeType_WString)
		return SCE_PAF_ERROR_CXML_TYPE;
	offset = bin->ws.offset;
	if (offset >= 0 && (uint32_t)offset * 2 < (uint32_t)doc->header.wstringtable_size)
		*value = (const uint16_t *)(doc->wstringtable + (uint32_t)offset * 2);
	else
		*value = NULL;
	if (!*value)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	if ((*value)[(uint32_t)bin->ws.len >> 1])
		return SCE_PAF_ERROR_CXML_FORMAT;
	*len = (uint32_t)bin->ws.len >> 1;
	return SCE_PAF_OK;
}

int32_t CxmlAttributeGetHash(const CxmlAttribute *self, uint32_t *value)
{
	const CxmlDocument *doc = self->doc;
	const AttributeBin *bin;
	int32_t offset;

	if (!doc)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	bin = (const AttributeBin *)(doc->tree + self->offset);
	if (bin->type != AttributeType_Hash)
		return SCE_PAF_ERROR_CXML_TYPE;
	offset = bin->hash;
	if (offset >= 0 && (uint32_t)offset * 4 < (uint32_t)doc->header.hashtable_size)
		*value = ((const uint32_t *)doc->hashtable)[offset];
	else
		*value = 0;
	return *value ? SCE_PAF_OK : SCE_PAF_ERROR_CXML_INVALID_NODE;
}

int32_t CxmlAttributeGetIntArray(const CxmlAttribute *self,
				const int32_t **addr, uint32_t *num)
{
	const CxmlDocument *doc = self->doc;
	const AttributeBin *bin;
	int32_t offset;

	if (!doc)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	bin = (const AttributeBin *)(doc->tree + self->offset);
	if (bin->type != AttributeType_IntArray)
		return SCE_PAF_ERROR_CXML_TYPE;
	offset = bin->ia.offset;
	if (offset >= 0 && ((uint32_t)offset + (uint32_t)bin->ia.num) * 4 <=
	    (uint32_t)doc->header.intarraytable_size)
		*addr = doc->intarraytable + offset;
	else
		*addr = NULL;
	if (!*addr)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	*num = bin->ia.num;
	return SCE_PAF_OK;
}

int32_t CxmlAttributeGetFloatArray(const CxmlAttribute *self,
				  const float **addr, uint32_t *num)
{
	const CxmlDocument *doc = self->doc;
	const AttributeBin *bin;
	int32_t offset;

	if (!doc)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	bin = (const AttributeBin *)(doc->tree + self->offset);
	if (bin->type != AttributeType_FloatArray)
		return SCE_PAF_ERROR_CXML_TYPE;
	offset = bin->fa.offset;
	if (offset >= 0 && ((uint32_t)offset + (uint32_t)bin->fa.num) * 4 <=
	    (uint32_t)doc->header.floatarraytable_size)
		*addr = doc->floatarraytable + offset;
	else
		*addr = NULL;
	if (!*addr)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	*num = bin->fa.num;
	return SCE_PAF_OK;
}

int32_t CxmlAttributeGetFile(const CxmlAttribute *self, CxmlFile *file)
{
	CxmlDocument *doc = self->doc;
	const AttributeBin *bin;
	int32_t offset, size;

	if (!doc)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	bin = (const AttributeBin *)(doc->tree + self->offset);
	if (bin->type != AttributeType_File)
		return SCE_PAF_ERROR_CXML_TYPE;
	offset = bin->file.offset;
	size = bin->file.size;
	file->doc = doc;
	file->offset = offset;
	file->size = size;
	return SCE_PAF_OK;
}

int32_t CxmlAttributeGetID(const CxmlAttribute *self, const char **value)
{
	const AttributeBin *bin;

	if (!self->doc)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	bin = (const AttributeBin *)(self->doc->tree + self->offset);
	if (bin->type != AttributeType_ID)
		return SCE_PAF_ERROR_CXML_TYPE;
	*value = CxmlDocumentGetIDString(self->doc, bin->id);
	return SCE_PAF_OK;
}

int32_t CxmlAttributeGetIDRef(const CxmlAttribute *self,
			     const char **value, CxmlElement *entity)
{
	CxmlDocument *doc = self->doc;
	const AttributeBin *bin;
	const IDBin *id_bin;
	int32_t offset;

	if (!doc)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	bin = (const AttributeBin *)(doc->tree + self->offset);
	if (bin->type != AttributeType_IDRef)
		return SCE_PAF_ERROR_CXML_TYPE;
	*value = CxmlDocumentGetIDString(doc, bin->idref);
	doc = self->doc;
	id_bin = (const IDBin *)(doc->idtable + bin->idref);
	offset = id_bin->entity_offset;
	if (CxmlDocumentIsValidElement(doc, offset)) {
		entity->doc = doc;
		entity->offset = offset;
	} else {
		entity->doc = NULL;
		entity->offset = -1;
	}
	return SCE_PAF_OK;
}

int32_t CxmlAttributeGetIDHash(const CxmlAttribute *self, uint32_t *value)
{
	const AttributeBin *bin;

	if (!self->doc)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	bin = (const AttributeBin *)(self->doc->tree + self->offset);
	if (bin->type != AttributeType_IDHash)
		return SCE_PAF_ERROR_CXML_TYPE;
	*value = CxmlDocumentGetIDHash(self->doc, bin->idhash);
	return SCE_PAF_OK;
}

int32_t CxmlAttributeGetIDHashRef(const CxmlAttribute *self,
				 uint32_t *value, CxmlElement *entity)
{
	CxmlDocument *doc = self->doc;
	const AttributeBin *bin;
	const IDHashBin *id_bin;
	int32_t offset;

	if (!doc)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	bin = (const AttributeBin *)(doc->tree + self->offset);
	if (bin->type != AttributeType_IDHashRef)
		return SCE_PAF_ERROR_CXML_TYPE;
	*value = CxmlDocumentGetIDHash(doc, bin->idhash);
	id_bin = (const IDHashBin *)(doc->idhashtable + bin->idhash);
	offset = id_bin->entity_offset;
	if (CxmlDocumentIsValidElement(doc, offset)) {
		entity->doc = doc;
		entity->offset = offset;
	} else {
		entity->doc = NULL;
		entity->offset = -1;
	}
	return SCE_PAF_OK;
}

const void *CxmlFileGetAddr(const CxmlFile *self)
{
	const CxmlDocument *doc = self->doc;

	if (!doc)
		return NULL;
	if (doc->access_mode != AccessMode_ReadWrite &&
	    doc->access_mode != AccessMode_ReadOnly_Memory)
		return NULL;
	if (!doc->filetable || self->offset >= doc->header.filetable_size)
		return NULL;
	if ((uint32_t)self->offset + (uint32_t)self->size >
	    (uint32_t)doc->header.filetable_size)
		return NULL;
	return doc->filetable + self->offset;
}
