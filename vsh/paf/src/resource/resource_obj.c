#include "resource_obj.h"
#include "../cxml/util.h"
#include "../file/localfile.h"
#include "../file/bufferedfile.h"
#include "../paf_types.h"
#include "../std/stdlib.h"
#include "../versatileparam/idparam.h"
#include <sce_atomic.h>
#include <kernel/iofilemgr.h>

extern int sce_paf_strcmp(const char *left, const char *right);
extern size_t sce_paf_strlen(const char *str);
extern void ScePafStdc_5C468294(void *ptr);

static const char s_default_string[] = "";
static const char s_buffer_filename[] = "";

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

ResourceObj *ResourceObjConstructFromBuffer(ResourceObj *self, const void *addr,
					  size_t size, const ResourceObjOption *option)
{
	ResourceIDMapNode *head;
	CxmlDocument *document;
	int32_t ret;

	self->rscfp.m_ptr = NULL;
	self->rscfp.m_ref_counter = NULL;
	PafStringInitialize(&self->filename);
	self->document = NULL;
	self->string_document = NULL;
	self->rscallocator = option->allocator;
	self->string_buffer = NULL;
	self->texture_buffer = NULL;
	self->model_buffer = NULL;
	self->sound_buffer = NULL;
	self->textures = NULL;
	self->models = NULL;
	self->sounds = NULL;
	self->num_textures = 0;
	self->num_models = 0;
	self->num_sounds = 0;
	self->load_status = 0;
	self->create_type = 1;
	self->id_map._Mysize = 0;
	head = ScePafStdc_FC5CD359(sizeof(*head));
	if (!head)
		ScePafStdc_D608246A();
	self->id_map._Myhead = head;
	if (head) {
		head->_Parent = head;
		head->_Left = head;
		head->_Right = head;
		head->_Color = 0;
		self->id_map._Myhead->_Isnil = 1;
	}
	PafStringInitialize(&self->default_string);
	PafStringAssign(&self->default_string, s_default_string,
			sce_paf_strlen(s_default_string));
	self->resolution = 0;
	PafStringAssign(&self->filename, s_buffer_filename,
			sce_paf_strlen(s_buffer_filename));
	document = ScePafStdc_FC5CD359(sizeof(*document));
	if (document)
		document = CxmlDocumentConstruct(document);
	self->document = document;
	document->allocator = option->cxml_allocator_func;
	document->allocator_userdata = self->rscallocator;
	ret = CxmlDocumentCreateFromBuffer(document, addr, size, 0);
	if (ret)
		self->load_status = ret;
	return self;
}

ResourceObj *ResourceObjConstructFromBufferBase(ResourceObj *self, const void *addr,
					      size_t size, const ResourceObjOption *option)
{
	return ResourceObjConstructFromBuffer(self, addr, size, option);
}

ResourceIDMap *ResourceIDMapClear(ResourceIDMap *self)
{
	ResourceIDMapNode *head = self->_Myhead;
	ResourceIDMapNode *node, *parent;

	if (!head)
		return self;
	node = head->_Left;
	while (!node->_Isnil) {
		if (!node->_Left->_Isnil) {
			node = node->_Left;
		} else if (!node->_Right->_Isnil) {
			node = node->_Right;
		} else {
			parent = node->_Parent;
			if (!parent->_Isnil) {
				if (parent->_Left == node)
					parent->_Left = head;
				else
					parent->_Right = head;
			}
			PafStringDestroy(&node->_Myval.second);
			ScePafStdc_1B77082E(node);
			head = self->_Myhead;
			node = parent;
		}
	}
	head->_Parent = head;
	head->_Left = head;
	head->_Right = head;
	self->_Mysize = 0;
	return NULL;
}

ResourceObj *ResourceObjDestroy(ResourceObj *self)
{
	CxmlDocument *document;

	if (self->create_type == 1) {
		if (self->textures) {
			ScePafStdc_5C468294(self->textures);
			self->textures = NULL;
			self->num_textures = 0;
		}
		if (self->models) {
			ScePafStdc_5C468294(self->models);
			self->models = NULL;
			self->num_models = 0;
		}
		if (self->sounds) {
			ScePafStdc_5C468294(self->sounds);
			self->sounds = NULL;
			self->num_sounds = 0;
		}
	}
	if (self->string_document) {
		ScePafStdc_1B77082E(CxmlDocumentDestroy(self->string_document));
		self->string_document = NULL;
		if (self->create_type == 0 && self->string_buffer) {
			if (self->rscallocator)
				scePafFreeWithContext(self->rscallocator, self->string_buffer);
			else
				sce_paf_free(self->string_buffer);
		}
		self->string_buffer = NULL;
	}
	document = self->document;
	if (document) {
		if (self->create_type == 0) {
			CxmlDocumentClear(document);
			document = self->document;
		}
		if (document)
			ScePafStdc_1B77082E(CxmlDocumentDestroy(document));
		self->document = NULL;
	}
	if (self->rscfp.m_ptr)
		self->rscfp.m_ptr->vtable->Close(self->rscfp.m_ptr);
	PafStringDestroy(&self->default_string);
	ResourceIDMapClear(&self->id_map);
	if (self->id_map._Myhead)
		ScePafStdc_1B77082E(self->id_map._Myhead);
	self->id_map._Myhead = (ResourceIDMapNode *)-1;
	self->id_map._Mysize = (size_t)-1;
	PafStringDestroy(&self->filename);
	if (self->rscfp.m_ptr &&
	    sceAtomicDecrement32AcqRel(&self->rscfp.m_ref_counter->m_counter) == 1) {
		self->rscfp.m_ref_counter->m_deleter(self->rscfp.m_ptr);
		if (sceAtomicDecrement32AcqRel(&self->rscfp.m_ref_counter->m_weak_counter) == 1)
			ScePafStdc_1B77082E(self->rscfp.m_ref_counter);
	}
	return self;
}

ResourceObj *ResourceObjDestroyBase(ResourceObj *self)
{
	return ResourceObjDestroy(self);
}

ResourceObj *ResourceObjConstructFromFile(ResourceObj *self, const char *name,
					const ResourceObjOption *option)
{
	ResourceIDMapNode *head;
	CxmlDocument *document;
	FileSharedPtr opened, buffered, fp;
	size_t length;
	int32_t ret;

	self->rscfp.m_ptr = NULL;
	self->rscfp.m_ref_counter = NULL;
	PafStringInitialize(&self->filename);
	self->document = NULL;
	self->string_document = NULL;
	self->rscallocator = option->allocator;
	self->string_buffer = NULL;
	self->texture_buffer = NULL;
	self->model_buffer = NULL;
	self->sound_buffer = NULL;
	self->textures = NULL;
	self->models = NULL;
	self->sounds = NULL;
	self->num_textures = 0;
	self->num_models = 0;
	self->num_sounds = 0;
	self->load_status = 0;
	self->create_type = 0;
	self->id_map._Mysize = 0;
	head = ScePafStdc_FC5CD359(sizeof(*head));
	if (!head)
		ScePafStdc_D608246A();
	self->id_map._Myhead = head;
	if (head) {
		head->_Parent = head;
		head->_Left = head;
		head->_Right = head;
		head->_Color = 0;
		self->id_map._Myhead->_Isnil = 1;
	}
	PafStringInitialize(&self->default_string);
	PafStringAssign(&self->default_string, s_default_string,
			sce_paf_strlen(s_default_string));
	self->resolution = 0;
	length = name ? sce_paf_strlen(name) : 0;
	PafStringAssign(&self->filename, name, length);
	document = ScePafStdc_FC5CD359(sizeof(*document));
	if (document)
		document = CxmlDocumentConstruct(document);
	self->document = document;
	LocalFileOpen(&opened, self->filename.ptr, SCE_O_RDONLY, 0, &ret);
	FileSharedPtrAssign(&self->rscfp, &opened);
	FileSharedPtrRelease(&opened);
	if (self->rscfp.m_ptr) {
		BufferSharedPtr buffer = { NULL, NULL };
		BufferSharedPtr allocated, base_buffer, buffer_arg;
		FileSharedPtr base_file, file_arg;
		void *addr;

		if (self->rscallocator)
			addr = scePafMallocAlignWithContext(self->rscallocator, 64, 4096);
		else
			addr = sce_paf_memalign(64, 4096);
		if (addr) {
			HeapAllocBufferCreate(&allocated, addr, 4096, self->rscallocator);
			if (allocated.m_ptr)
				BufferSharedPtrCopy(&buffer, &allocated);
			BufferSharedPtrRelease(&allocated);
		}
		FileSharedPtrCopy(&base_file, &self->rscfp);
		BufferSharedPtrCopy(&base_buffer, &buffer);
		FileSharedPtrCopy(&file_arg, &base_file);
		BufferSharedPtrCopy(&buffer_arg, &base_buffer);
		BufferedFileOpen(&buffered, &file_arg, &ret, &buffer_arg);
		BufferSharedPtrRelease(&buffer_arg);
		FileSharedPtrRelease(&file_arg);
		FileSharedPtrAssign(&self->rscfp, &buffered);
		FileSharedPtrRelease(&buffered);
		BufferSharedPtrRelease(&base_buffer);
		FileSharedPtrRelease(&base_file);
		BufferSharedPtrRelease(&buffer);
	}
	if (self->rscfp.m_ptr) {
		document = self->document;
		document->allocator = option->cxml_allocator_func;
		document->allocator_userdata = self->rscallocator;
		FileSharedPtrCopy(&fp, &self->rscfp);
		ret = CxmlDocumentCreateFromFile(document, &fp, 0);
		FileSharedPtrRelease(&fp);
		if (ret) {
			CxmlDocumentClear(self->document);
			self->rscfp.m_ptr->vtable->Close(self->rscfp.m_ptr);
			self->load_status = ret;
		}
		if (option->load_type & 4) {
			ret = ResourceObjLoadResourceDescription(self, self->filename.ptr);
			if (ret)
				self->load_status = ret;
		}
	} else {
		self->load_status = SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
	}
	return self;
}

ResourceObj *ResourceObjConstructFromFileBase(ResourceObj *self, const char *name,
					    const ResourceObjOption *option)
{
	return ResourceObjConstructFromFile(self, name, option);
}
