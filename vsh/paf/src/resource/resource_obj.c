#include "resource_obj.h"
#include "../cxml/util.h"
#include "../file/localfile.h"
#include "../file/bufferedfile.h"
#include "../file/memfile.h"
#include "../compress/decompress.h"
#include "../paf_types.h"
#include "../std/stdlib.h"
#include "../std/new.h"
#include "../versatileparam/idparam.h"
#include <sce_atomic.h>
#include <kernel/iofilemgr.h>

extern int sce_paf_strcmp(const char *left, const char *right);
extern int sce_paf_strncmp(const char *left, const char *right, size_t len);
extern char *sce_paf_strncpy(char *dst, const char *src, size_t len);
extern unsigned long sce_paf_strtoul(const char *str, char **end, int base);
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

static inline __attribute__((always_inline))
void *ResourceObjAllocFileBuffer(const ResourceObj *self, HeapAllocator *allocator,
				 size_t size)
{
	if (allocator)
		return scePafMallocAlignWithContext(allocator, 64, size);
	if (self->rscallocator)
		return scePafMallocAlignWithContext(self->rscallocator, 64, size);
	return sce_paf_memalign(64, size);
}

static inline __attribute__((always_inline))
void ResourceObjFreeFileBuffer(const ResourceObj *self, HeapAllocator *allocator,
			      void *addr)
{
	if (allocator)
		scePafFreeWithContext(allocator, addr);
	else if (self->rscallocator)
		scePafFreeWithContext(self->rscallocator, addr);
	else
		sce_paf_free(addr);
}

void ResourceObjGetFileBufferElement(FileSharedPtr *result, const ResourceObj *self,
		const CxmlElement *elm, void *addr, size_t *size, const char **type,
		HeapAllocator *decomp_allocator)
{
	CxmlFile src = { NULL, -1, 0 };
	const char *compress;
	uint32_t len, compress_len;
	int32_t original_size, error, mem_error;
	unsigned char compressed = 0;
	unsigned char load_failed;
	FileSharedPtr opened, fp;
	BufferSharedPtr allocated, buffer = { NULL, NULL }, arg;
	FileSharedPtr file;
	void *data;

	if (CxmlUtilGetFile(elm, "src", &src))
		goto failed;
	error = 0;
	if (CxmlUtilGetString(elm, "type", type, &len))
		goto failed;
	error = 0;
	original_size = src.size;
	*size = src.size;
	error = CxmlUtilGetString(elm, "compress", &compress, &compress_len);
	if (!error && !sce_paf_strncmp(compress, "on", 2) && compress_len == 2) {
		CxmlUtilGetInt(elm, "origsize", &original_size);
		compressed = 1;
	}
	if (self->create_type) {
		addr = (void *)CxmlFileGetAddr(&src);
		goto make_file;
	}
	if (self->rscfp.m_ptr->vtable->IsOpened(self->rscfp.m_ptr)) {
		if (compressed) {
			data = ResourceObjAllocFileBuffer(self, decomp_allocator, *size);
			if (!data)
				goto failed;
			FileSharedPtrCopy(&fp, &self->rscfp);
			error = CxmlFileLoad(&src, &fp, data, *size);
			FileSharedPtrRelease(&fp);
			load_failed = error != 0;
			if (!load_failed)
				load_failed = Decompress(addr, original_size, data, *size,
						 DecompressType_Zlib) < 0;
			ResourceObjFreeFileBuffer(self, decomp_allocator, data);
			if (load_failed)
				goto failed;
			*size = original_size;
		} else {
			FileSharedPtrCopy(&fp, &self->rscfp);
			error = CxmlFileLoad(&src, &fp, addr, *size);
			FileSharedPtrRelease(&fp);
			if (error)
				goto failed;
		}
		goto make_file;
	}
	LocalFileOpen(&opened, self->filename.ptr, SCE_O_RDONLY, 0, &error);
	if (!opened.m_ptr)
		goto failed;
	if (compressed) {
		data = ResourceObjAllocFileBuffer(self, decomp_allocator, *size);
		if (!data)
			goto close_failed;
		FileSharedPtrCopy(&fp, &opened);
		error = CxmlFileLoad(&src, &fp, data, *size);
		FileSharedPtrRelease(&fp);
		load_failed = error != 0;
		if (!load_failed)
			load_failed = Decompress(addr, original_size, data, *size,
					 DecompressType_Zlib) < 0;
		ResourceObjFreeFileBuffer(self, decomp_allocator, data);
		if (load_failed)
			goto close_failed;
		*size = original_size;
	} else {
		FileSharedPtrCopy(&fp, &opened);
		error = CxmlFileLoad(&src, &fp, addr, *size);
		FileSharedPtrRelease(&fp);
		if (error)
			goto close_failed;
	}
	opened.m_ptr->vtable->Close(opened.m_ptr);
	FileSharedPtrRelease(&opened);
make_file:
	UnmanagedBufferCreate(&allocated, addr, original_size);
	if (allocated.m_ptr)
		BufferSharedPtrCopy(&buffer, &allocated);
	BufferSharedPtrRelease(&allocated);
	if (!buffer.m_ptr)
		goto failed;
	BufferSharedPtrCopy(&arg, &buffer);
	MemFileOpenWritableBuffer(&file, &arg, &mem_error);
	BufferSharedPtrRelease(&arg);
	FileSharedPtrCopy(result, &file);
	FileSharedPtrRelease(&file);
	BufferSharedPtrRelease(&buffer);
	return;
close_failed:
	opened.m_ptr->vtable->Close(opened.m_ptr);
	result->m_ptr = NULL;
	result->m_ref_counter = NULL;
	FileSharedPtrRelease(&opened);
	return;
failed:
	result->m_ptr = NULL;
	result->m_ref_counter = NULL;
}

void ResourceObjGetFileBufferHash(FileSharedPtr *result, const ResourceObj *self,
		uint32_t idhash, void *addr, size_t *size, const char **type,
		HeapAllocator *decomp_allocator)
{
	CxmlElement table, elm;

	ResourceObjGetTableElement(&table, self, TableType_File);
	if (table.doc && table.offset >= 0) {
		CxmlUtilFindChildElementByIDHash(&elm, &table, "file", "id", idhash);
		if (elm.doc && elm.offset >= 0) {
			table = elm;
			ResourceObjGetFileBufferElement(result, self, &table, addr, size,
						 type, decomp_allocator);
			return;
		}
	}
	result->m_ptr = NULL;
	result->m_ref_counter = NULL;
}

void ResourceObjGetFileBuffer(FileSharedPtr *result, const ResourceObj *self,
		const char *id, void *addr, size_t *size, const char **type,
		HeapAllocator *decomp_allocator)
{
	IDParam idparam;

	IDParamConstructFromString(&idparam, id);
	ResourceObjGetFileBufferHash(result, self, idparam.m_idhash, addr, size,
				     type, decomp_allocator);
	PafStringDestroy(&idparam.m_id);
}

static inline __attribute__((always_inline))
void ResourceIDMapLrotate(ResourceIDMapNode *head,
			     ResourceIDMapNode *node)
{
	ResourceIDMapNode *right = node->_Right;

	node->_Right = right->_Left;
	if (!right->_Left->_Isnil)
		right->_Left->_Parent = node;
	right->_Parent = node->_Parent;
	if (node->_Parent->_Isnil)
		head->_Parent = right;
	else if (node == node->_Parent->_Left)
		node->_Parent->_Left = right;
	else
		node->_Parent->_Right = right;
	right->_Left = node;
	node->_Parent = right;
}

static inline __attribute__((always_inline))
void ResourceIDMapRrotate(ResourceIDMapNode *head,
			     ResourceIDMapNode *node)
{
	ResourceIDMapNode *left = node->_Left;

	node->_Left = left->_Right;
	if (!left->_Right->_Isnil)
		left->_Right->_Parent = node;
	left->_Parent = node->_Parent;
	if (node->_Parent->_Isnil)
		head->_Parent = left;
	else if (node == node->_Parent->_Right)
		node->_Parent->_Right = left;
	else
		node->_Parent->_Left = left;
	left->_Right = node;
	node->_Parent = left;
}

ResourceIDMapNode *ResourceIDMapRebalance(ResourceIDMap *self,
					ResourceIDMapNode *node)
{
	ResourceIDMapNode *head = self->_Myhead;

	if (!head)
		return (ResourceIDMapNode *)self;
	node->_Color = 1;
	while (node != head->_Parent && node->_Parent->_Color) {
		ResourceIDMapNode *parent = node->_Parent;
		ResourceIDMapNode *grandparent = parent->_Parent;
		ResourceIDMapNode *uncle;

		if (parent == grandparent->_Left) {
			uncle = grandparent->_Right;
			if (uncle->_Color) {
				parent->_Color = 0;
				uncle->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				node = node->_Parent->_Parent;
			} else {
				if (node == parent->_Right) {
					node = node->_Parent;
					ResourceIDMapLrotate(head, node);
				}
				node->_Parent->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				ResourceIDMapRrotate(head, node->_Parent->_Parent);
			}
		} else {
			uncle = grandparent->_Left;
			if (uncle->_Color) {
				parent->_Color = 0;
				uncle->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				node = node->_Parent->_Parent;
			} else {
				if (node == parent->_Left) {
					node = node->_Parent;
					ResourceIDMapRrotate(head, node);
				}
				node->_Parent->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				ResourceIDMapLrotate(head, node->_Parent->_Parent);
			}
		}
	}
	head->_Parent->_Color = 0;
	return head->_Parent;
}

ResourceIDMapNode *ResourceIDMapInsert(ResourceIDMap *self, const ResourceIDEntry *value,
				   unsigned char *inserted)
{
	ResourceIDMapNode *parent = self->_Myhead;
	ResourceIDMapNode *node, *entry;
	int side = 0;

	if (!parent)
		return NULL;
	node = parent->_Parent;
	if (inserted)
		*inserted = 0;
	if (!node->_Isnil) {
		uint32_t key = value->first;

		do {
			parent = node;
			if (key >= node->_Myval.first) {
				if (node->_Myval.first >= key)
					return node;
				node = node->_Right;
				side = 1;
			} else {
				node = node->_Left;
				side = -1;
			}
		} while (!node->_Isnil);
	}
	node = ScePafStdc_FC5CD359(sizeof(*node));
	if (!node) {
		ScePafStdc_D608246A();
		return self->_Myhead;
	}
	entry = PafPlacementNew(sizeof(*entry), node);
	if (entry) {
		ResourceIDMapNode *head = self->_Myhead;

		entry->_Parent = parent;
		entry->_Left = head;
		entry->_Right = head;
		entry->_Myval.first = value->first;
		PafStringInitialize(&entry->_Myval.second);
		PafStringCopy(&entry->_Myval.second, &value->second);
		entry->_Isnil = 0;
		entry->_Color = 1;
	}
	if (side < 0) {
		parent->_Left = node;
		if (parent == self->_Myhead->_Left)
			self->_Myhead->_Left = node;
	} else if (side > 0) {
		parent->_Right = node;
		if (parent == self->_Myhead->_Right)
			self->_Myhead->_Right = node;
	} else {
		ResourceIDMapNode *head = self->_Myhead;

		head->_Parent = node;
		head->_Left = node;
		head->_Right = node;
	}
	++self->_Mysize;
	ResourceIDMapRebalance(self, node);
	if (inserted)
		*inserted = 1;
	return node;
}

static inline __attribute__((always_inline))
void ResourceObjFreeDescriptionBuffer(ResourceObj *self, void *addr)
{
	if (self->rscallocator)
		scePafFreeWithContext(self->rscallocator, addr);
	else
		sce_paf_free(addr);
}

int32_t ResourceObjLoadResourceDescription(ResourceObj *self, const char *name)
{
	PafString filename, stem, path, device;
	FileSharedPtr opened;
	int32_t error, ret = 0;
	int required;
	uint32_t length;
	char *data;

	PafStringInitialize(&filename);
	PafStringAssign(&filename, name, name ? sce_paf_strlen(name) : 0);
	PafStringSubstring(&stem, &filename, 0, filename.count - 3);
	PafStringAppendBytes(&stem, "rcd", sce_paf_strlen("rcd"));
	PafStringInitialize(&path);
	PafStringCopy(&path, &stem);
	PafStringDestroy(&stem);
	PafStringSubstring(&device, &path, 0, 6);
	required = !sce_paf_strncmp(device.ptr, "host0:", 6);
	PafStringDestroy(&device);
	LocalFileOpen(&opened, path.ptr, SCE_O_RDONLY, 0, &error);
	if (opened.m_ptr) {
		BufferSharedPtr buffer = { NULL, NULL };
		BufferSharedPtr allocated, base_buffer, buffer_arg;
		FileSharedPtr base_file, file_arg, buffered;
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
		FileSharedPtrCopy(&base_file, &opened);
		BufferSharedPtrCopy(&base_buffer, &buffer);
		FileSharedPtrCopy(&file_arg, &base_file);
		BufferSharedPtrCopy(&buffer_arg, &base_buffer);
		BufferedFileOpen(&buffered, &file_arg, &error, &buffer_arg);
		BufferSharedPtrRelease(&buffer_arg);
		FileSharedPtrRelease(&file_arg);
		FileSharedPtrAssign(&opened, &buffered);
		FileSharedPtrRelease(&buffered);
		BufferSharedPtrRelease(&base_buffer);
		FileSharedPtrRelease(&base_file);
		BufferSharedPtrRelease(&buffer);
	}
	if (!opened.m_ptr) {
		ret = required ? SCE_PAF_ERROR_RESOURCE_CANT_OPEN : 0;
		goto strings;
	}
	/* The RCD reader tests only the low, signed word of the file size. */
	length = (uint32_t)opened.m_ptr->vtable->GetFileSize(opened.m_ptr);
	if (length & 0x80000000)
		goto file;
	if (self->rscallocator)
		data = scePafMallocAlignWithContext(self->rscallocator, 64, length);
	else
		data = sce_paf_memalign(64, length);
	if (!data) {
		ret = required ? SCE_PAF_ERROR_RESOURCE_NO_MEMORY : 0;
		goto file;
	}
	error = opened.m_ptr->vtable->Read(opened.m_ptr, data, length);
	if ((uint32_t)error != length) {
		ResourceObjFreeDescriptionBuffer(self, data);
		ret = SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
		goto file;
	}
	if (length) {
		uint32_t consumed = 0;
		char *cursor = data;
		ResourceIDMap *map = &self->id_map;

		do {
			char *next = cursor + 1;

			if (*cursor == 'k') {
				char hash[12], text[512];
				char *out = text;
				const char *src;
				uint32_t id;
				PafString empty;
				ResourceIDEntry value;
				ResourceIDMapNode *node;

				sce_paf_strncpy(hash, cursor + 4, 8);
				hash[8] = 0;
				src = cursor + 16;
				id = sce_paf_strtoul(hash, NULL, 16);
				next = cursor + 17;
				consumed += 17;
				if (*src != '\n') {
					do {
						char *dest = out++;
						const char *from = src;
						int ch;

						src = next;
						sce_paf_strncpy(dest, from, 1);
						ch = *next++;
						++consumed;
						if (ch == '\n')
							break;
					} while (1);
				}
				/* Sony expects CRLF and removes the byte before LF. */
				*(out - 1) = 0;
				PafStringInitialize(&empty);
				value.first = id;
				PafStringInitialize(&value.second);
				PafStringCopy(&value.second, &empty);
				node = ResourceIDMapInsert(map, &value, NULL);
				PafStringDestroy(&value.second);
				PafStringDestroy(&empty);
				PafStringAssign(&node->_Myval.second, text, sce_paf_strlen(text));
			} else {
				++consumed;
				if (*cursor != '\n') {
					do {
						int ch = *next++;

						++consumed;
						if (ch == '\n')
							break;
					} while (1);
				}
			}
			cursor = next;
		} while (consumed < length);
	}
	ResourceObjFreeDescriptionBuffer(self, data);
	opened.m_ptr->vtable->Close(opened.m_ptr);
file:
	FileSharedPtrRelease(&opened);
strings:
	PafStringDestroy(&path);
	PafStringDestroy(&filename);
	return ret;
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
