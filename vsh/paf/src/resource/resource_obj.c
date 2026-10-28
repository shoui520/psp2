#include "resource_obj.h"
#include "../cxml/util.h"
#include "../paf_types.h"
#include "../versatileparam/idparam.h"

extern int sce_paf_strcmp(const char *left, const char *right);

char *ResourceObjGetFileName(const ResourceObj *self)
{
	return self->filename.ptr;
}

int32_t ResourceObjGetLoadStatus(const ResourceObj *self)
{
	return self->load_status;
}

uint32_t ResourceObjGetCreateType(const ResourceObj *self)
{
	return self->create_type;
}

CxmlDocument *ResourceObjGetDocument(const ResourceObj *self)
{
	return self->document;
}

CxmlDocument *ResourceObjGetStringDocument(const ResourceObj *self)
{
	return self->string_document;
}

void ResourceObjGetTableElement(CxmlElement *result, const ResourceObj *self,
				TableType type)
{
	CxmlElement elm;
	const char *name;

	result->doc = NULL;
	result->offset = -1;
	if (!self->document)
		return;
	CxmlDocumentGetDocumentElement(&elm, self->document);
	*result = elm;
	if (result->doc && result->offset >= 0)
		CxmlUtilCheckElementName(result, "resource");
	switch (type) {
	case TableType_Page:
		name = "pagetable";
		break;
	case TableType_Style:
		name = "styletable";
		break;
	case TableType_String:
		name = "stringtable";
		break;
	case TableType_Texture:
		name = "texturetable";
		break;
	case TableType_Model:
		name = "modeltable";
		break;
	case TableType_Sound:
		name = "sounddatatable";
		break;
	case TableType_SoundIndex:
		name = "soundindextable";
		break;
	case TableType_File:
		name = "filetable";
		break;
	case TableType_Template:
		name = "templatetable";
		break;
	case TableType_Alias:
		name = "aliastable";
		break;
	default:
		return;
	}
	CxmlUtilFindChildElement(&elm, result, name);
	*result = elm;
}

unsigned char ResourceObjIsRegisteredIDHash(ResourceObj *self, uint32_t value)
{
	if (!self->document)
		return 0;
	return CxmlDocumentIsRegisteredIDHash(self->document, value);
}

int32_t ResourceObjGetStringElement(const ResourceObj *self, const CxmlElement *elm,
				    const uint16_t **src, uint32_t *length)
{
	return CxmlUtilGetWString(elm, "src", src, length);
}

int32_t ResourceObjGetPageHash(const ResourceObj *self, uint32_t idhash, CxmlElement *elm)
{
	CxmlElement table;

	ResourceObjGetTableElement(&table, self, TableType_Page);
	*elm = table;
	if (!elm->doc || elm->offset < 0)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	CxmlUtilFindChildElementByIDHash(&table, elm, "page", "id", idhash);
	*elm = table;
	if (!elm->doc || elm->offset < 0)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	return SCE_PAF_OK;
}

int32_t ResourceObjGetStyleHash(const ResourceObj *self, uint32_t idhash, CxmlElement *elm)
{
	CxmlElement table, next;
	uint32_t count, index, value;

	ResourceObjGetTableElement(&table, self, TableType_Style);
	*elm = table;
	if (!elm->doc || elm->offset < 0)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	count = CxmlElementGetNumChild(elm);
	CxmlElementGetFirstChild(&table, elm);
	*elm = table;
	for (index = 0; index < count; index++) {
		if (!CxmlUtilGetIDHash(elm, "id", &value) && value == idhash)
			return SCE_PAF_OK;
		CxmlElementGetNextSibling(&next, elm);
		*elm = next;
	}
	return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
}

int32_t ResourceObjGetStringHash(const ResourceObj *self, uint32_t idhash,
				 const uint16_t **src, uint32_t *length)
{
	CxmlElement root, elm;

	if (!self->string_document)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	CxmlDocumentGetDocumentElement(&elm, self->string_document);
	root = elm;
	CxmlUtilFindChildElementByIDHash(&elm, &root, "string", "id", idhash);
	if (!elm.doc || elm.offset < 0)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	root = elm;
	return CxmlUtilGetWString(&root, "src", src, length);
}

int32_t ResourceObjGetPage(const ResourceObj *self, const char *id, CxmlElement *elm)
{
	IDParam idparam;
	int32_t ret;

	IDParamConstructFromString(&idparam, id);
	ret = ResourceObjGetPageHash(self, idparam.m_idhash, elm);
	PafStringDestroy(&idparam.m_id);
	return ret;
}

int32_t ResourceObjGetStyle(const ResourceObj *self, const char *id, CxmlElement *elm)
{
	IDParam idparam;
	int32_t ret;

	IDParamConstructFromString(&idparam, id);
	ret = ResourceObjGetStyleHash(self, idparam.m_idhash, elm);
	PafStringDestroy(&idparam.m_id);
	return ret;
}

int32_t ResourceObjGetString(const ResourceObj *self, const char *id,
			     const uint16_t **src, uint32_t *length)
{
	IDParam idparam;
	int32_t ret;

	IDParamConstructFromString(&idparam, id);
	ret = ResourceObjGetStringHash(self, idparam.m_idhash, src, length);
	PafStringDestroy(&idparam.m_id);
	return ret;
}

unsigned char ResourceObjIsRegisteredID(ResourceObj *self, const char *str, uint32_t mode)
{
	IDParam idparam;
	unsigned char ret;

	if (!self->document)
		return 0;
	if (!mode)
		return CxmlDocumentIsRegisteredID(self->document, str);
	if (mode != 1)
		return 0;
	IDParamConstructFromString(&idparam, str);
	ret = CxmlDocumentIsRegisteredIDHash(self->document, idparam.m_idhash);
	PafStringDestroy(&idparam.m_id);
	return ret;
}

int32_t ResourceObjGetStringIDRef(const ResourceObj *self, const CxmlElement *elm,
				const char *attrname, const char **str, CxmlElement *entity)
{
	CxmlElement root, child;
	int32_t ret;

	ret = CxmlUtilGetIDRef(elm, attrname, str, entity);
	if (ret)
		return ret;
	if (!self->string_document)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	CxmlDocumentGetDocumentElement(&root, self->string_document);
	/* The firmware passes the first signed byte as the entire ID hash. */
	CxmlUtilFindChildElementByIDHash(&child, &root, "string", "id",
				      (uint32_t)(int32_t)(int8_t)**str);
	*entity = child;
	if (!child.doc || child.offset < 0)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	return SCE_PAF_OK;
}

int32_t ResourceObjGetStringIDHashRef(const ResourceObj *self, const CxmlElement *elm,
				    const char *attrname, uint32_t *value, CxmlElement *entity)
{
	CxmlElement root, child;
	int32_t ret;

	ret = CxmlUtilGetIDHashRef(elm, attrname, value, entity);
	if (ret)
		return ret;
	if (!self->string_document)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	CxmlDocumentGetDocumentElement(&root, self->string_document);
	CxmlUtilFindChildElementByIDHash(&child, &root, "string", "id", *value);
	*entity = child;
	if (!child.doc || child.offset < 0)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	return SCE_PAF_OK;
}

int32_t ResourceObjGetTextureFromBufferHash(const ResourceObj *self, uint32_t idhash,
					  void **addr, size_t *size, uint32_t *type)
{
	CxmlElement table;
	ResourceBufferEntry *entry;
	uint32_t count, index;

	if (!self->textures)
		return SCE_PAF_ERROR_RESOURCE_BUFFER_NOT_FOUND;
	ResourceObjGetTableElement(&table, self, TableType_Texture);
	count = CxmlElementGetNumChild(&table);
	entry = self->textures;
	for (index = 0; index < count; index++, entry++) {
		if (entry->idhash != idhash)
			continue;
		*addr = self->textures[index].addr;
		entry = &self->textures[index];
		*size = entry->size;
		*type = entry->type;
		return SCE_PAF_OK;
	}
	return SCE_PAF_ERROR_RESOURCE_BUFFER_NOT_FOUND;
}

int32_t ResourceObjGetTextureFromBuffer(const ResourceObj *self, const char *id,
				      void **addr, size_t *size, uint32_t *type)
{
	IDParam idparam;
	int32_t ret;

	IDParamConstructFromString(&idparam, id);
	ret = ResourceObjGetTextureFromBufferHash(self, idparam.m_idhash, addr, size, type);
	PafStringDestroy(&idparam.m_id);
	return ret;
}

int32_t ResourceObjGetTextureFromBufferElement(const ResourceObj *self, const CxmlElement *elm,
					     void **addr, size_t *size, uint32_t *type)
{
	uint32_t value;
	int32_t ret = CxmlUtilGetIDHash(elm, "id", &value);

	if (ret)
		return ret;
	return ResourceObjGetTextureFromBufferHash(self, value, addr, size, type);
}

int32_t ResourceObjGetModelFromBufferHash(const ResourceObj *self, uint32_t idhash,
					void **addr, size_t *size, uint32_t *type)
{
	CxmlElement table;
	ResourceBufferEntry *entry;
	uint32_t count, index;

	if (!self->models)
		return SCE_PAF_ERROR_RESOURCE_BUFFER_NOT_FOUND;
	ResourceObjGetTableElement(&table, self, TableType_Model);
	count = CxmlElementGetNumChild(&table);
	entry = self->models;
	for (index = 0; index < count; index++, entry++) {
		if (entry->idhash != idhash)
			continue;
		*addr = self->models[index].addr;
		entry = &self->models[index];
		*size = entry->size;
		*type = entry->type;
		return SCE_PAF_OK;
	}
	return SCE_PAF_ERROR_RESOURCE_BUFFER_NOT_FOUND;
}

int32_t ResourceObjGetModelFromBuffer(const ResourceObj *self, const char *id,
				    void **addr, size_t *size, uint32_t *type)
{
	IDParam idparam;
	int32_t ret;

	IDParamConstructFromString(&idparam, id);
	ret = ResourceObjGetModelFromBufferHash(self, idparam.m_idhash, addr, size, type);
	PafStringDestroy(&idparam.m_id);
	return ret;
}

int32_t ResourceObjGetModelFromBufferElement(const ResourceObj *self, const CxmlElement *elm,
					   void **addr, size_t *size, uint32_t *type)
{
	uint32_t value;
	int32_t ret = CxmlUtilGetIDHash(elm, "id", &value);

	if (ret)
		return ret;
	return ResourceObjGetModelFromBufferHash(self, value, addr, size, type);
}

uint32_t ResourceObjGetTextureType(const ResourceObj *self, const char *type)
{
	if (!sce_paf_strcmp(type, "texture/png"))
		return TextureType_Png;
	if (!sce_paf_strcmp(type, "texture/jpg"))
		return TextureType_Jpg;
	if (!sce_paf_strcmp(type, "texture/tif"))
		return TextureType_Tif;
	if (!sce_paf_strcmp(type, "texture/gif"))
		return TextureType_Gif;
	if (!sce_paf_strcmp(type, "texture/bmp"))
		return TextureType_Bmp;
	if (!sce_paf_strcmp(type, "texture/gim"))
		return TextureType_Gim;
	if (!sce_paf_strcmp(type, "texture/dds"))
		return TextureType_Dds;
	if (!sce_paf_strcmp(type, "texture/gxt"))
		return TextureType_Gxt;
	return (uint32_t)TextureType_Unknown;
}

int32_t ResourceObjGetTextureFromFileElement(const ResourceObj *self, const CxmlElement *elm,
					   CxmlFile *src, size_t *size, uint32_t *type)
{
	const char *format;
	uint32_t len;
	int32_t ret = CxmlUtilGetFile(elm, "src", src);

	if (ret)
		return ret;
	*size = src->size;
	CxmlUtilGetString(elm, "type", &format, &len);
	*type = ResourceObjGetTextureType(self, format);
	return SCE_PAF_OK;
}

int32_t ResourceObjGetTextureFromFileHash(const ResourceObj *self, uint32_t idhash,
					CxmlFile *src, size_t *size, uint32_t *type)
{
	CxmlElement table, elm;

	ResourceObjGetTableElement(&table, self, TableType_Texture);
	if (!table.doc || table.offset < 0)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	CxmlUtilFindChildElementByIDHash(&elm, &table, "texture", "id", idhash);
	if (!elm.doc || elm.offset < 0)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	table = elm;
	return ResourceObjGetTextureFromFileElement(self, &table, src, size, type);
}

int32_t ResourceObjGetTextureFromFile(const ResourceObj *self, const char *id,
				    CxmlFile *src, size_t *size, uint32_t *type)
{
	IDParam idparam;
	int32_t ret;

	IDParamConstructFromString(&idparam, id);
	ret = ResourceObjGetTextureFromFileHash(self, idparam.m_idhash, src, size, type);
	PafStringDestroy(&idparam.m_id);
	return ret;
}

int32_t ResourceObjGetModelFromFileElement(const ResourceObj *self, const CxmlElement *elm,
					 CxmlFile *src, size_t *size, uint32_t *type)
{
	const char *format;
	uint32_t len;
	int32_t ret = CxmlUtilGetFile(elm, "src", src);

	if (ret)
		return ret;
	*size = src->size;
	ret = CxmlUtilGetString(elm, "type", &format, &len);
	if (ret)
		return ret;
	*type = sce_paf_strcmp(format, "model/gmo") ? (uint32_t)-1 : 0;
	return SCE_PAF_OK;
}

int32_t ResourceObjGetModelFromFileHash(const ResourceObj *self, uint32_t idhash,
				      CxmlFile *src, size_t *size, uint32_t *type)
{
	CxmlElement table, elm;

	ResourceObjGetTableElement(&table, self, TableType_Model);
	if (!table.doc || table.offset < 0)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	CxmlUtilFindChildElementByIDHash(&elm, &table, "model", "id", idhash);
	if (!elm.doc || elm.offset < 0)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	table = elm;
	return ResourceObjGetModelFromFileElement(self, &table, src, size, type);
}

int32_t ResourceObjGetModelFromFile(const ResourceObj *self, const char *id,
				  CxmlFile *src, size_t *size, uint32_t *type)
{
	IDParam idparam;
	int32_t ret;

	IDParamConstructFromString(&idparam, id);
	ret = ResourceObjGetModelFromFileHash(self, idparam.m_idhash, src, size, type);
	PafStringDestroy(&idparam.m_id);
	return ret;
}

uint32_t ResourceObjGetSoundType(const ResourceObj *self, const char *type)
{
	if (!sce_paf_strcmp(type, "sound/vag"))
		return SoundType_VAG;
	if (!sce_paf_strcmp(type, "sound/wav"))
		return SoundType_WAV;
	if (!sce_paf_strcmp(type, "sound/mp3"))
		return SoundType_MP3;
	if (!sce_paf_strcmp(type, "sound/at3"))
		return SoundType_AT3;
	if (!sce_paf_strcmp(type, "sound/aac"))
		return SoundType_AAC;
	return (uint32_t)SoundType_Unknown;
}

int32_t ResourceObjGetSoundFromFileElement(const ResourceObj *self, const CxmlElement *elm,
		CxmlFile *left_or_mono, size_t *left_or_mono_size, CxmlFile *right,
		size_t *right_size, int32_t *samplerate, uint32_t *format)
{
	int32_t value, ret;
	const char *type;
	uint32_t len;

	ret = CxmlUtilGetFile(elm, "src", left_or_mono);
	if (ret) {
		int32_t left_ret, right_ret;

		left_ret = CxmlUtilGetFile(elm, "left", left_or_mono);
		right_ret = CxmlUtilGetFile(elm, "right", right);
		if (left_ret | right_ret)
			return ret;
		*left_or_mono_size = left_or_mono->size;
		*right_size = right->size;
	} else {
		*left_or_mono_size = left_or_mono->size;
		*right_size = 0;
	}
	ret = CxmlUtilGetInt(elm, "rate", &value);
	if (ret)
		return ret;
	*samplerate = value;
	ret = CxmlUtilGetString(elm, "type", &type, &len);
	if (ret)
		return ret;
	*format = ResourceObjGetSoundType(self, type);
	return SCE_PAF_OK;
}

int32_t ResourceObjGetSoundFromFileHash(const ResourceObj *self, uint32_t hash,
		CxmlFile *left_or_mono, size_t *left_or_mono_size, CxmlFile *right,
		size_t *right_size, int32_t *samplerate, uint32_t *format)
{
	CxmlElement table, elm;

	ResourceObjGetTableElement(&table, self, TableType_Sound);
	if (!table.doc || table.offset < 0)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	CxmlUtilFindChildElementByIDHash(&elm, &table, "sounddata", "id", hash);
	if (!elm.doc || elm.offset < 0)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	table = elm;
	return ResourceObjGetSoundFromFileElement(self, &table, left_or_mono,
			left_or_mono_size, right, right_size, samplerate, format);
}

int32_t ResourceObjGetSoundFromFile(const ResourceObj *self, const char *id,
		CxmlFile *left_or_mono, size_t *left_or_mono_size, CxmlFile *right,
		size_t *right_size, int32_t *samplerate, uint32_t *format)
{
	IDParam idparam;
	int32_t ret;

	IDParamConstructFromString(&idparam, id);
	ret = ResourceObjGetSoundFromFileHash(self, idparam.m_idhash, left_or_mono,
			left_or_mono_size, right, right_size, samplerate, format);
	PafStringDestroy(&idparam.m_id);
	return ret;
}

int32_t ResourceObjGetSoundFromBufferHash(const ResourceObj *self, uint32_t idhash,
		void **addr1, size_t *size1, void **addr2, size_t *size2,
		int32_t *samplerate, uint32_t *type)
{
	CxmlElement table;
	ResourceSoundEntry *entry;
	uint32_t count, index;

	if (!self->sounds) {
		*size1 = 0;
		*addr1 = NULL;
		*size2 = 0;
		*addr2 = NULL;
		*samplerate = 0;
		*type = 0;
		return SCE_PAF_ERROR_RESOURCE_BUFFER_NOT_FOUND;
	}
	ResourceObjGetTableElement(&table, self, TableType_Sound);
	count = CxmlElementGetNumChild(&table);
	entry = self->sounds;
	for (index = 0; index < count; index++, entry++) {
		if (entry->idhash != idhash)
			continue;
		*addr1 = self->sounds[index].addr1;
		*addr2 = self->sounds[index].addr2;
		entry = &self->sounds[index];
		*size1 = entry->size1;
		*size2 = entry->size2;
		*samplerate = entry->samplerate;
		*type = entry->type;
		return SCE_PAF_OK;
	}
	*addr1 = NULL;
	*size1 = 0;
	*size2 = 0;
	*addr2 = NULL;
	*samplerate = 0;
	*type = 0;
	return SCE_PAF_ERROR_RESOURCE_BUFFER_NOT_FOUND;
}

int32_t ResourceObjGetSoundFromBuffer(const ResourceObj *self, const char *id,
		void **addr1, size_t *size1, void **addr2, size_t *size2,
		int32_t *samplerate, uint32_t *type)
{
	IDParam idparam;
	int32_t ret;

	IDParamConstructFromString(&idparam, id);
	ret = ResourceObjGetSoundFromBufferHash(self, idparam.m_idhash, addr1, size1,
			addr2, size2, samplerate, type);
	PafStringDestroy(&idparam.m_id);
	return ret;
}

int32_t ResourceObjGetSoundFromBufferElement(const ResourceObj *self, const CxmlElement *elm,
		void **addr1, size_t *size1, void **addr2, size_t *size2,
		int32_t *samplerate, uint32_t *type)
{
	uint32_t value;
	int32_t ret = CxmlUtilGetIDHash(elm, "id", &value);

	if (ret)
		return ret;
	return ResourceObjGetSoundFromBufferHash(self, value, addr1, size1,
			addr2, size2, samplerate, type);
}
