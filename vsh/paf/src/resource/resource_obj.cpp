/*
 * paf::resource::ResourceObj: a plugin's resource (RCO), read from a file or a buffer
 * into its document and string document, with the texture, model and sound tables
 * loaded into buffers on request. Names after ScePaf-RE.
 */
#include <stddef.h>
#include <stdint.h>
#include <buffer/buffer.h>
#include <compress/compress.h>
#include <cxml/cxml.h>
#include <file/file.h>
#include <file/bufferedfile.h>
#include <file/localfile.h>
#include <file/memfile.h>
#include <kernel/iofilemgr.h>
#include <memory/heap_allocator.h>
#include <resource/resource_obj.h>
#include <std/list>
#include <std/stdlib.h>
#include <std/string.h>
#include <system/system.h>
#include <versatileparam/idparam.h>
#include <src/paf_types.h>
#include "resource_file.h"

namespace paf {
namespace resource {

/* a buffer for a compressed file, from the caller's allocator, else the resource's */
#define ALLOC_DECOMP_BUFFER(decomp_allocator, size)					\
	((decomp_allocator) != NULL ? (decomp_allocator)->sce_paf_memalign(64, size) :	\
	 m_rscallocator != NULL ? m_rscallocator->sce_paf_memalign(64, size) :		\
	 sce_paf_memalign(64, size))

#define FREE_DECOMP_BUFFER(decomp_allocator, ptr)					\
	do {										\
		if ((decomp_allocator) != NULL)						\
			(decomp_allocator)->sce_paf_free(ptr);				\
		else if (m_rscallocator != NULL)					\
			m_rscallocator->sce_paf_free(ptr);				\
		else									\
			sce_paf_free(ptr);						\
	} while (0)

const char *ResourceObj::GetFileName() const
{
	return m_filename.c_str();
}

int32_t ResourceObj::GetLoadStatus() const
{
	return m_load_status;
}

uint32_t ResourceObj::GetCreateType() const
{
	return m_create_type;
}

cxml::Document *ResourceObj::GetDocument() const
{
	return m_document;
}

cxml::Document *ResourceObj::GetStringDocument() const
{
	return m_string_document;
}

cxml::Element ResourceObj::GetTableElement(TableType type) const
{
	cxml::Element elm;

	if (m_document != NULL) {
		elm = m_document->GetDocumentElement();
		if (elm)
			cxml::util::CheckElementName(elm, "resource");
		switch (type) {
		case TableType_Page:
			elm = cxml::util::FindChildElement(elm, "pagetable");
			break;
		case TableType_Style:
			elm = cxml::util::FindChildElement(elm, "styletable");
			break;
		case TableType_String:
			elm = cxml::util::FindChildElement(elm, "stringtable");
			break;
		case TableType_Texture:
			elm = cxml::util::FindChildElement(elm, "texturetable");
			break;
		case TableType_Model:
			elm = cxml::util::FindChildElement(elm, "modeltable");
			break;
		case TableType_Sound:
			elm = cxml::util::FindChildElement(elm, "sounddatatable");
			break;
		case TableType_SoundIndex:
			elm = cxml::util::FindChildElement(elm, "soundindextable");
			break;
		case TableType_File:
			elm = cxml::util::FindChildElement(elm, "filetable");
			break;
		case TableType_Template:
			elm = cxml::util::FindChildElement(elm, "templatetable");
			break;
		case TableType_Alias:
			elm = cxml::util::FindChildElement(elm, "aliastable");
			break;
		default:
			break;
		}
	}
	return elm;
}

int32_t ResourceObj::GetPage(uint32_t idhash, cxml::Element &elm) const
{
	elm = GetTableElement(TableType_Page);
	if (!elm)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	elm = cxml::util::FindChildElement(elm, "page", "id", idhash);
	if (!elm)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	return 0;
}

int32_t ResourceObj::GetPage(const char *id, cxml::Element &elm) const
{
	IDParam param(id);

	return GetPage(param.GetIDHash(), elm);
}

int32_t ResourceObj::GetStyle(uint32_t idhash, cxml::Element &elm) const
{
	uint32_t value;

	elm = GetTableElement(TableType_Style);
	if (!elm)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;

	uint32_t num = elm.GetNumChild();

	elm = elm.GetFirstChild();
	for (uint32_t i = 0; i < num; i++) {
		if (scePafResourceGetAttributeIdIntLpb(&elm, "id", &value) == 0 && value == idhash)
			return 0;
		elm = elm.GetNextSibling();
	}
	return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
}

int32_t ResourceObj::GetStyle(const char *id, cxml::Element &elm) const
{
	IDParam param(id);

	return GetStyle(param.GetIDHash(), elm);
}

int32_t ResourceObj::GetString(const cxml::Element &elm, const wchar_t **str,
			       uint32_t *length) const
{
	int32_t ret = scePafResourceGetAttributeChar(&elm, "src", str, length);

	if (ret != 0)
		return ret;
	return 0;
}

int32_t ResourceObj::GetString(uint32_t idhash, const wchar_t **str, uint32_t *length) const
{
	cxml::Element elm;

	if (m_string_document == NULL)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	elm = m_string_document->GetDocumentElement();

	cxml::Element string = cxml::util::FindChildElement(elm, "string", "id", idhash);

	if (!string)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	elm = string;
	return GetString(elm, str, length);
}

int32_t ResourceObj::GetString(const char *id, const wchar_t **str, uint32_t *length) const
{
	IDParam param(id);
	cxml::Element elm;
	int32_t ret;

	if (m_string_document != NULL) {
		elm = m_string_document->GetDocumentElement();
		elm = cxml::util::FindChildElement(elm, "string", "id", param.GetIDHash());
		if (elm)
			ret = GetString(elm, str, length);
		else
			ret = SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	} else {
		ret = SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	}
	return ret;
}

uint32_t ResourceObj::GetTextureType(const char *type) const
{
	if (sce_paf_strcmp(type, "texture/png") == 0)
		return 0;
	if (sce_paf_strcmp(type, "texture/jpg") == 0)
		return 1;
	if (sce_paf_strcmp(type, "texture/tif") == 0)
		return 2;
	if (sce_paf_strcmp(type, "texture/gif") == 0)
		return 3;
	if (sce_paf_strcmp(type, "texture/bmp") == 0)
		return 4;
	if (sce_paf_strcmp(type, "texture/gim") == 0)
		return 5;
	if (sce_paf_strcmp(type, "texture/dds") == 0)
		return 6;
	if (sce_paf_strcmp(type, "texture/gxt") == 0)
		return 7;
	return -1;
}

int32_t ResourceObj::GetTextureFromFile(const cxml::Element &elm, cxml::File *src,
					uint32_t *size, uint32_t *type) const
{
	const char *mime;
	uint32_t len;
	int32_t ret = scePafResourceGetAttributeData(&elm, "src", src);

	if (ret != 0)
		return ret;
	*size = src->size;
	scePafResourceGetAttributeString(&elm, "type", &mime, &len);
	*type = GetTextureType(mime);
	return 0;
}

int32_t ResourceObj::GetTextureFromFile(uint32_t idhash, cxml::File *src, uint32_t *size,
					uint32_t *type) const
{
	cxml::Element elm = GetTableElement(TableType_Texture);

	if (!elm)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	elm = cxml::util::FindChildElement(elm, "texture", "id", idhash);
	if (!elm)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	return GetTextureFromFile(elm, src, size, type);
}

int32_t ResourceObj::GetTextureFromFile(const char *id, cxml::File *src, uint32_t *size,
					uint32_t *type) const
{
	IDParam param(id);

	return GetTextureFromFile(param.GetIDHash(), src, size, type);
}

int32_t ResourceObj::GetTextureFromBuffer(uint32_t idhash, void **addr, uint32_t *size,
					  uint32_t *type) const
{
	if (m_textures == NULL)
		return SCE_PAF_ERROR_RESOURCE_BUFFER_NOT_FOUND;

	uint32_t num = GetTableElement(TableType_Texture).GetNumChild();

	for (uint32_t i = 0; i < num; i++) {
		if (m_textures[i].idhash == idhash) {
			*addr = m_textures[i].addr;
			*size = m_textures[i].size;
			*type = m_textures[i].type;
			return 0;
		}
	}
	return SCE_PAF_ERROR_RESOURCE_BUFFER_NOT_FOUND;
}

int32_t ResourceObj::GetTextureFromBuffer(const char *id, void **addr, uint32_t *size,
					  uint32_t *type) const
{
	IDParam param(id);

	return GetTextureFromBuffer(param.GetIDHash(), addr, size, type);
}

int32_t ResourceObj::GetTextureFromBuffer(const cxml::Element &elm, void **addr,
					  uint32_t *size, uint32_t *type) const
{
	uint32_t idhash;
	int32_t ret = scePafResourceGetAttributeIdIntLpb(&elm, "id", &idhash);

	if (ret != 0)
		return ret;
	return GetTextureFromBuffer(idhash, addr, size, type);
}

uint32_t ResourceObj::GetModelType(const char *type) const
{
	if (sce_paf_strcmp(type, "model/gmo") == 0)
		return 0;
	return -1;
}

int32_t ResourceObj::GetModelFromFile(const cxml::Element &elm, cxml::File *src,
				      uint32_t *size, uint32_t *type) const
{
	const char *mime;
	uint32_t len;
	int32_t ret = scePafResourceGetAttributeData(&elm, "src", src);

	if (ret != 0)
		return ret;
	*size = src->size;
	ret = scePafResourceGetAttributeString(&elm, "type", &mime, &len);
	if (ret != 0)
		return ret;
	*type = GetModelType(mime);
	return 0;
}

int32_t ResourceObj::GetModelFromFile(uint32_t idhash, cxml::File *src, uint32_t *size,
				      uint32_t *type) const
{
	cxml::Element elm = GetTableElement(TableType_Model);

	if (!elm)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	elm = cxml::util::FindChildElement(elm, "model", "id", idhash);
	if (!elm)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	return GetModelFromFile(elm, src, size, type);
}

int32_t ResourceObj::GetModelFromFile(const char *id, cxml::File *src, uint32_t *size,
				      uint32_t *type) const
{
	IDParam param(id);

	return GetModelFromFile(param.GetIDHash(), src, size, type);
}

int32_t ResourceObj::GetModelFromBuffer(uint32_t idhash, void **addr, uint32_t *size,
					uint32_t *type) const
{
	if (m_models == NULL)
		return SCE_PAF_ERROR_RESOURCE_BUFFER_NOT_FOUND;

	uint32_t num = GetTableElement(TableType_Model).GetNumChild();

	for (uint32_t i = 0; i < num; i++) {
		if (m_models[i].idhash == idhash) {
			*addr = m_models[i].addr;
			*size = m_models[i].size;
			*type = m_models[i].type;
			return 0;
		}
	}
	return SCE_PAF_ERROR_RESOURCE_BUFFER_NOT_FOUND;
}

int32_t ResourceObj::GetModelFromBuffer(const char *id, void **addr, uint32_t *size,
					uint32_t *type) const
{
	IDParam param(id);

	return GetModelFromBuffer(param.GetIDHash(), addr, size, type);
}

int32_t ResourceObj::GetModelFromBuffer(const cxml::Element &elm, void **addr, uint32_t *size,
					uint32_t *type) const
{
	uint32_t idhash;
	int32_t ret = scePafResourceGetAttributeIdIntLpb(&elm, "id", &idhash);

	if (ret != 0)
		return ret;
	return GetModelFromBuffer(idhash, addr, size, type);
}

uint32_t ResourceObj::GetSoundType(const char *type) const
{
	if (sce_paf_strcmp(type, "sound/vag") == 0)
		return 1;
	if (sce_paf_strcmp(type, "sound/wav") == 0)
		return 0;
	if (sce_paf_strcmp(type, "sound/mp3") == 0)
		return 2;
	if (sce_paf_strcmp(type, "sound/at3") == 0)
		return 3;
	if (sce_paf_strcmp(type, "sound/aac") == 0)
		return 4;
	return -1;
}

}
}

using namespace paf;
using namespace paf::resource;

/* a sound's file: "src" for a mono one, else "left" and "right" */
int32_t ScePafResource_B4714AEA(const ResourceObj *rscobj, const cxml::Element &elm,
				cxml::File *left_or_mono, uint32_t *left_or_mono_size,
				cxml::File *right, uint32_t *right_size, int32_t *samplerate,
				uint32_t *format)
{
	int32_t rate;
	const char *mime;
	uint32_t len;
	int32_t ret = scePafResourceGetAttributeData(&elm, "src", left_or_mono);

	if (ret != 0) {
		if ((scePafResourceGetAttributeData(&elm, "left", left_or_mono) |
		     scePafResourceGetAttributeData(&elm, "right", right)) != 0)
			return ret;
		*left_or_mono_size = left_or_mono->size;
		*right_size = right->size;
	} else {
		*left_or_mono_size = left_or_mono->size;
		*right_size = 0;
	}
	ret = scePafResourceGetAttributeInteger(&elm, "rate", &rate);
	if (ret != 0)
		return ret;
	*samplerate = rate;
	ret = scePafResourceGetAttributeString(&elm, "type", &mime, &len);
	if (ret != 0)
		return ret;
	*format = rscobj->GetSoundType(mime);
	return 0;
}

int32_t ScePafResource_08129BF8(const ResourceObj *rscobj, uint32_t idhash,
				cxml::File *left_or_mono, uint32_t *left_or_mono_size,
				cxml::File *right, uint32_t *right_size, int32_t *samplerate,
				uint32_t *format)
{
	cxml::Element elm = rscobj->GetTableElement(TableType_Sound);

	if (!elm)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	elm = cxml::util::FindChildElement(elm, "sounddata", "id", idhash);
	if (!elm)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	return ScePafResource_B4714AEA(rscobj, elm, left_or_mono, left_or_mono_size, right,
				       right_size, samplerate, format);
}

int32_t ScePafResource_BEAEA02A(const ResourceObj *rscobj, const char *id,
				cxml::File *left_or_mono, uint32_t *left_or_mono_size,
				cxml::File *right, uint32_t *right_size, int32_t *samplerate,
				uint32_t *format)
{
	IDParam param(id);

	return ScePafResource_08129BF8(rscobj, param.GetIDHash(), left_or_mono, left_or_mono_size,
				       right, right_size, samplerate, format);
}

int32_t ScePafResource_ADDC0711(const ResourceObj *rscobj, uint32_t idhash, void **addr1,
				uint32_t *size1, void **addr2, uint32_t *size2,
				int32_t *samplerate, uint32_t *type)
{
	if (rscobj->m_sounds == NULL) {
		*addr1 = NULL;
		*size1 = 0;
		*addr2 = NULL;
		*size2 = 0;
		*samplerate = 0;
		*type = 0;
		return SCE_PAF_ERROR_RESOURCE_BUFFER_NOT_FOUND;
	}

	uint32_t num = rscobj->GetTableElement(TableType_Sound).GetNumChild();

	for (uint32_t i = 0; i < num; i++) {
		if (rscobj->m_sounds[i].idhash == idhash) {
			*addr1 = rscobj->m_sounds[i].addr1;
			*addr2 = rscobj->m_sounds[i].addr2;
			*size1 = rscobj->m_sounds[i].size1;
			*size2 = rscobj->m_sounds[i].size2;
			*samplerate = rscobj->m_sounds[i].samplerate;
			*type = rscobj->m_sounds[i].type;
			return 0;
		}
	}
	*addr1 = NULL;
	*size1 = 0;
	*size2 = 0;
	*addr2 = NULL;
	*samplerate = 0;
	*type = 0;
	return SCE_PAF_ERROR_RESOURCE_BUFFER_NOT_FOUND;
}

int32_t ScePafResource_DD1FF94F(const ResourceObj *rscobj, const char *id, void **addr1,
				uint32_t *size1, void **addr2, uint32_t *size2,
				int32_t *samplerate, uint32_t *type)
{
	IDParam param(id);

	return ScePafResource_ADDC0711(rscobj, param.GetIDHash(), addr1, size1, addr2, size2,
				       samplerate, type);
}

int32_t ScePafResource_65C0FDF9(const ResourceObj *rscobj, const cxml::Element &elm,
				void **addr1, uint32_t *size1, void **addr2, uint32_t *size2,
				int32_t *samplerate, uint32_t *type)
{
	uint32_t idhash;
	int32_t ret = scePafResourceGetAttributeIdIntLpb(&elm, "id", &idhash);

	if (ret != 0)
		return ret;
	return ScePafResource_ADDC0711(rscobj, idhash, addr1, size1, addr2, size2, samplerate,
				       type);
}

namespace paf {
namespace resource {

common::SharedPtr<MemFile> ResourceObj::GetFile(const cxml::Element &elm, void *addr,
						uint32_t *size, const char **type,
						memory::HeapAllocator *decomp_allocator) const
{
	cxml::File src;
	uint32_t len;

	if (scePafResourceGetAttributeData(&elm, "src", &src) != 0)
		return common::SharedPtr<MemFile>();

	int32_t error = 0;

	if (scePafResourceGetAttributeString(&elm, "type", type, &len) != 0)
		return common::SharedPtr<MemFile>();

	const char *compress;
	uint32_t compress_len;
	int32_t orig_size = src.size;

	*size = src.size;
	error = scePafResourceGetAttributeString(&elm, "compress", &compress, &compress_len);

	bool compressed = false;

	if (error == 0 && sce_paf_strncmp(compress, "on", 2) == 0 && compress_len == 2) {
		scePafResourceGetAttributeInteger(&elm, "origsize", &orig_size);
		compressed = true;
	}

	if (m_create_type == 0) {
		if (!m_rscfp->IsOpened()) {
			common::SharedPtr<File> fp = LocalFile::Open(m_filename.c_str(), SCE_O_RDONLY,
								     0, &error);

			if (fp.get() == NULL)
				return common::SharedPtr<MemFile>();
			if (compressed) {
				void *buf = ALLOC_DECOMP_BUFFER(decomp_allocator, *size);

				if (buf == NULL) {
					fp->Close();
					return common::SharedPtr<MemFile>();
				}
				error = src.Load(fp, buf, *size);
				if (error != 0) {
					FREE_DECOMP_BUFFER(decomp_allocator, buf);
					fp->Close();
					return common::SharedPtr<MemFile>();
				}
				if (compress::Decompress(addr, orig_size, buf, *size,
							 compress::DecompressType_Zlib) < 0) {
					FREE_DECOMP_BUFFER(decomp_allocator, buf);
					fp->Close();
					return common::SharedPtr<MemFile>();
				}
				FREE_DECOMP_BUFFER(decomp_allocator, buf);
				*size = orig_size;
			} else {
				error = src.Load(fp, addr, *size);
				if (error != 0) {
					fp->Close();
					return common::SharedPtr<MemFile>();
				}
			}
			fp->Close();
		} else if (compressed) {
			void *buf = ALLOC_DECOMP_BUFFER(decomp_allocator, *size);

			if (buf == NULL)
				return common::SharedPtr<MemFile>();
			error = src.Load(m_rscfp, buf, *size);
			if (error != 0) {
				FREE_DECOMP_BUFFER(decomp_allocator, buf);
				return common::SharedPtr<MemFile>();
			}
			if (compress::Decompress(addr, orig_size, buf, *size,
						 compress::DecompressType_Zlib) < 0) {
				FREE_DECOMP_BUFFER(decomp_allocator, buf);
				return common::SharedPtr<MemFile>();
			}
			FREE_DECOMP_BUFFER(decomp_allocator, buf);
			*size = orig_size;
		} else {
			error = src.Load(m_rscfp, addr, *size);
			if (error != 0)
				return common::SharedPtr<MemFile>();
		}
	} else {
		addr = const_cast<void *>(src.GetAddr());
	}

	common::SharedPtr<Buffer> buffer = UnmanagedBuffer::Create(addr, orig_size);

	if (buffer.get() == NULL)
		return common::SharedPtr<MemFile>();

	int32_t open_error;
	common::SharedPtr<MemFile> memfile = MemFile::Open(buffer, &open_error);

	return memfile;
}

__attribute__((noinline))
common::SharedPtr<MemFile> ResourceObj::GetFile(uint32_t idhash, void *addr, uint32_t *size,
						const char **type,
						memory::HeapAllocator *decomp_allocator) const
{
	cxml::Element elm = GetTableElement(TableType_File);

	if (!elm)
		return common::SharedPtr<MemFile>();
	elm = cxml::util::FindChildElement(elm, "file", "id", idhash);
	if (!elm)
		return common::SharedPtr<MemFile>();
	return GetFile(elm, addr, size, type, decomp_allocator);
}

common::SharedPtr<MemFile> ResourceObj::GetFile(const char *id, void *addr, uint32_t *size,
						const char **type,
						memory::HeapAllocator *decomp_allocator) const
{
	IDParam param(id);

	return GetFile(param.GetIDHash(), addr, size, type, decomp_allocator);
}

common::SharedPtr<MemFile> ResourceObj::GetFile(const cxml::Element &elm, uint32_t *size,
						const char **type,
						memory::HeapAllocator *file_allocator,
						memory::HeapAllocator *decomp_allocator) const
{
	cxml::File src;
	uint32_t len;

	if (scePafResourceGetAttributeData(&elm, "src", &src) != 0)
		return common::SharedPtr<MemFile>();

	int32_t error = 0;

	if (scePafResourceGetAttributeString(&elm, "type", type, &len) != 0)
		return common::SharedPtr<MemFile>();

	const char *compress;
	uint32_t compress_len;
	int32_t orig_size = src.size;

	*size = src.size;
	error = scePafResourceGetAttributeString(&elm, "compress", &compress, &compress_len);

	bool compressed = false;

	if (error == 0 && sce_paf_strncmp(compress, "on", 2) == 0 && compress_len == 2) {
		scePafResourceGetAttributeInteger(&elm, "origsize", &orig_size);
		compressed = true;
	}

	if (m_create_type == 0) {
		common::SharedPtr<Buffer> buffer;

		if (file_allocator != NULL)
			buffer = HeapAllocBuffer::Allocate(orig_size, file_allocator);
		else
			buffer = MallocBuffer::Allocate(orig_size);
		if (buffer.get() == NULL)
			return common::SharedPtr<MemFile>();

		if (!m_rscfp->IsOpened()) {
			common::SharedPtr<File> fp = LocalFile::Open(m_filename.c_str(), SCE_O_RDONLY, 0,
								     &error);

			if (fp.get() == NULL)
				return common::SharedPtr<MemFile>();
			if (compressed) {
				void *buf = ALLOC_DECOMP_BUFFER(decomp_allocator, *size);

				if (buf == NULL) {
					fp->Close();
					return common::SharedPtr<MemFile>();
				}
				error = src.Load(fp, buf, *size);
				if (error != 0) {
					FREE_DECOMP_BUFFER(decomp_allocator, buf);
					/* the resource's file, not fp, as Sony's code does */
					m_rscfp->Close();
					return common::SharedPtr<MemFile>();
				}
				if (compress::Decompress(buffer->GetAddr(), orig_size, buf, *size,
							 compress::DecompressType_Zlib) < 0) {
					FREE_DECOMP_BUFFER(decomp_allocator, buf);
					m_rscfp->Close();
					return common::SharedPtr<MemFile>();
				}
				FREE_DECOMP_BUFFER(decomp_allocator, buf);
				*size = orig_size;
			} else {
				error = src.Load(fp, buffer->GetAddr(), *size);
				if (error != 0) {
					fp->Close();
					return common::SharedPtr<MemFile>();
				}
			}
			fp->Close();
		} else {
			if (compressed) {
				void *buf = ALLOC_DECOMP_BUFFER(decomp_allocator, *size);

				if (buf == NULL)
					return common::SharedPtr<MemFile>();
				error = src.Load(m_rscfp, buf, *size);
				if (error != 0) {
					FREE_DECOMP_BUFFER(decomp_allocator, buf);
					return common::SharedPtr<MemFile>();
				}
				if (compress::Decompress(buffer->GetAddr(), orig_size, buf, *size,
							 compress::DecompressType_Zlib) < 0) {
					FREE_DECOMP_BUFFER(decomp_allocator, buf);
					return common::SharedPtr<MemFile>();
				}
				FREE_DECOMP_BUFFER(decomp_allocator, buf);
				*size = orig_size;
			} else {
				error = src.Load(m_rscfp, buffer->GetAddr(), *size);
				if (error != 0)
					return common::SharedPtr<MemFile>();
			}
		}

		int32_t open_error;
		common::SharedPtr<MemFile> memfile = MemFile::Open(buffer, &open_error);

		return memfile;
	} else {
		int32_t open_error;
		common::SharedPtr<MemFile> memfile = MemFile::Open(src.GetAddr(), src.size,
								   &open_error);

		return memfile;
	}
}

__attribute__((noinline))
common::SharedPtr<MemFile> ResourceObj::GetFile(uint32_t idhash, uint32_t *size,
						const char **type,
						memory::HeapAllocator *file_allocator,
						memory::HeapAllocator *decomp_allocator) const
{
	cxml::Element elm = GetTableElement(TableType_File);

	if (!elm)
		return common::SharedPtr<MemFile>();
	elm = cxml::util::FindChildElement(elm, "file", "id", idhash);
	if (!elm)
		return common::SharedPtr<MemFile>();
	return GetFile(elm, size, type, file_allocator, decomp_allocator);
}

common::SharedPtr<MemFile> ResourceObj::GetFile(const char *id, uint32_t *size,
						const char **type,
						memory::HeapAllocator *file_allocator,
						memory::HeapAllocator *decomp_allocator) const
{
	IDParam param(id);

	return GetFile(param.GetIDHash(), size, type, file_allocator, decomp_allocator);
}

int32_t ResourceObj::GetStringIDRef(const cxml::Element &elm, const char *attrname,
				    const char **str, cxml::Element *entity) const
{
	int32_t ret = scePafResourceGetAttributeIdStr(&elm, attrname, str, entity);

	if (ret != 0)
		return ret;
	if (m_string_document == NULL)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	/* Sony's code looks the string up by the ID's first character as its hash */
	*entity = cxml::util::FindChildElement(m_string_document->GetDocumentElement(), "string",
					       "id", **str);
	if (!*entity)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	return 0;
}

int32_t ResourceObj::GetStringIDRef(const cxml::Element &elm, const char *attrname,
				    uint32_t *value, cxml::Element *entity) const
{
	int32_t ret = scePafResourceGetAttributeIdInt(&elm, attrname, value, entity);

	if (ret != 0)
		return ret;
	if (m_string_document == NULL)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	*entity = cxml::util::FindChildElement(m_string_document->GetDocumentElement(), "string",
					       "id", *value);
	if (!*entity)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	return 0;
}

bool ResourceObj::IsRegisteredID(const char *id, uint32_t mode)
{
	if (m_document == NULL)
		return false;
	switch (mode) {
	case 0:
		return m_document->IsRegisteredID(id);
	case 1: {
		IDParam param(id);

		return m_document->IsRegisteredIDHash(param.GetIDHash());
	}
	default:
		return false;
	}
}

bool ResourceObj::IsRegisteredID(uint32_t idhash)
{
	if (m_document == NULL)
		return false;
	return m_document->IsRegisteredIDHash(idhash);
}

cxml::Element ResourceObj::GetOverrideElement(const cxml::Element &elm) const
{
	cxml::Element result = elm;
	const char *name = elm.GetName();

	if (name != NULL && sce_paf_strncmp(name, "override_", sce_paf_strlen("override_")) != 0) {
		int32_t model = system::Model::GetType();

		for (cxml::Element child = result.GetFirstChild(); child;
		     child = child.GetNextSibling()) {
			uint32_t resolution;
			int32_t model_type;
			bool match = false;

			if (scePafResourceGetAttributeStyleId(&child, "resolution", &resolution) == 0) {
				if (resolution != m_resolution_resource.m_idhash)
					continue;
				match = true;
			}
			if (scePafResourceGetAttributeInteger(&child, "model_type", &model_type) == 0) {
				if (model != model_type)
					continue;
				match = true;
			}
			if (match) {
				result = child;
				break;
			}
		}
	}
	return result;
}

IDParam ResourceObj::GetAliasID(const IDParam &id) const
{
	IDParam alias(s_empty_name, 0);
	cxml::Element table = GetTableElement(TableType_Alias);

	if (table) {
		cxml::Element elm = cxml::util::FindChildElement(table, "alias", "id", id.m_idhash);

		if (elm) {
			int32_t model = system::Model::GetType();

			for (cxml::Element child = elm.GetFirstChild(); child;
			     child = child.GetNextSibling()) {
				int32_t model_type;

				if (scePafResourceGetAttributeInteger(&child, "model_type",
								      &model_type) == 0 &&
				    model == model_type) {
					elm = child;
					break;
				}
			}

			cxml::Element entity;
			uint32_t org_id;

			if (scePafResourceGetAttributeIdInt(&elm, "org_id", &org_id, &entity) >= 0)
				alias.m_idhash = org_id;
		}
	}
	return alias;
}

/* the locale element IDs, by the system language (name ours) */
extern const char *const s_locale_id[] = {
	"ja", "en", "fr", "es", "de", "it", "nl", "pt", "ru", "ko", "zh-t", "zh-s", "fi", "sv",
	"da", "no", "pl", "pt-br", "en-gb", "tr"
};

cxml::Element ResourceObj::GetLocaleElement(int32_t locale) const
{
	cxml::Element elm = GetTableElement(TableType_String);
	const char *id;

	switch (locale) {
	case 0:
		id = s_locale_id[0];
		break;
	case 1:
		id = s_locale_id[1];
		break;
	case 2:
		id = s_locale_id[2];
		break;
	case 3:
		id = s_locale_id[3];
		break;
	case 4:
		id = s_locale_id[4];
		break;
	case 5:
		id = s_locale_id[5];
		break;
	case 6:
		id = s_locale_id[6];
		break;
	case 7:
		id = s_locale_id[7];
		break;
	case 8:
		id = s_locale_id[8];
		break;
	case 9:
		id = s_locale_id[9];
		break;
	case 10:
		id = s_locale_id[10];
		break;
	case 11:
		id = s_locale_id[11];
		break;
	case 12:
		id = s_locale_id[12];
		break;
	case 13:
		id = s_locale_id[13];
		break;
	case 14:
		id = s_locale_id[14];
		break;
	case 15:
		id = s_locale_id[15];
		break;
	case 16:
		id = s_locale_id[16];
		break;
	case 17:
		id = s_locale_id[17];
		break;
	case 18:
		id = s_locale_id[18];
		break;
	case 19:
		id = s_locale_id[19];
		break;
	default:
		id = "";
		break;
	}
	elm = cxml::util::FindChildElement(elm, "locale", "id", id);
	return elm;
}

/* size rounded up to a multiple of align, when there is one */
static inline __attribute__((always_inline)) size_t AlignSize(size_t size, uint32_t align)
{
	if (align != 0 && size % align != 0)
		return size + align - size % align;
	return size;
}

size_t ResourceObj::GetTableBufferSize(TableType type, uint32_t align, int32_t *num) const
{
	if (type != TableType_Texture && type != TableType_Model && type != TableType_Sound)
		return 0;

	cxml::Element elm = GetTableElement(type);

	if (!elm)
		return 0;

	uint32_t count = elm.GetNumChild();
	size_t total = 0;

	*num = count;
	elm = elm.GetFirstChild();
	for (uint32_t i = 0; i < count; i++) {
		cxml::File file;
		cxml::Element entry = GetOverrideElement(elm);
		const char *ignore;
		uint32_t len;

		if (scePafResourceGetAttributeString(&entry, "ignore_make_surface_first", &ignore,
						     &len) == 0 &&
		    sce_paf_strncmp(ignore, "on", 2) == 0 && len == 2) {
			elm = elm.GetNextSibling();
			continue;
		}
		if (scePafResourceGetAttributeData(&entry, "src", &file) == 0) {
			total += AlignSize(file.size, align);
		} else if (type == TableType_Sound) {
			cxml::File left;
			cxml::File right;

			if ((scePafResourceGetAttributeData(&elm, "left", &left) |
			     scePafResourceGetAttributeData(&elm, "right", &right)) == 0)
				total += AlignSize(left.size, align) + AlignSize(right.size, align);
		}
		elm = elm.GetNextSibling();
	}
	return total;
}

}
}

/* a document's allocator over the HeapAllocator its userdata is, else the paf heap */
void ResourceObjCxmlAllocator(cxml::AllocationType type, void *userdata, void *old_addr,
			      size_t required_size, void **addr, size_t *size)
{
	memory::HeapAllocator *allocator = static_cast<memory::HeapAllocator *>(userdata);

	switch (type) {
	case cxml::AllocationType_Alloc_Tree:
	case cxml::AllocationType_Alloc_IDTable:
	case cxml::AllocationType_Alloc_IDHashTable:
	case cxml::AllocationType_Alloc_StringTable:
	case cxml::AllocationType_Alloc_WStringTable:
	case cxml::AllocationType_Alloc_HashTable:
	case cxml::AllocationType_Alloc_IntArrayTable:
	case cxml::AllocationType_Alloc_FloatArrayTable:
	case cxml::AllocationType_Alloc_FileTable:
		if (allocator != NULL) {
			if (old_addr != NULL)
				*addr = allocator->sce_paf_realloc(old_addr, required_size);
			else
				*addr = allocator->sce_paf_memalign(64, required_size);
		} else if (old_addr != NULL) {
			*addr = sce_paf_realloc(old_addr, required_size);
		} else {
			*addr = sce_paf_memalign(64, required_size);
		}
		*size = required_size;
		break;
	case cxml::AllocationType_Free_Tree:
	case cxml::AllocationType_Free_IDTable:
	case cxml::AllocationType_Free_IDHashTable:
	case cxml::AllocationType_Free_StringTable:
	case cxml::AllocationType_Free_WStringTable:
	case cxml::AllocationType_Free_HashTable:
	case cxml::AllocationType_Free_IntArrayTable:
	case cxml::AllocationType_Free_FloatArrayTable:
	case cxml::AllocationType_Free_FileTable:
		if (allocator != NULL)
			allocator->sce_paf_free(old_addr);
		else
			sce_paf_free(old_addr);
		break;
	default:
		break;
	}
}

namespace paf {
namespace resource {

/* the fields of a string between its delimiters, empty ones left out (name ours) */
static list<string> SplitString(const string &str, const char *delimiter)
{
	string rest = str;
	string delim = delimiter;
	list<string> fields;

	for (;;) {
		size_t pos = rest.find(delim);

		if (pos == string::npos)
			break;
		if ((int32_t)pos > 0)
			fields.push_back(rest.substr(0, pos));
		rest = rest.substr(pos + 1, string::npos);
	}
	if (rest.size() != 0)
		fields.push_back(rest);
	return fields;
}

int32_t ResourceObj::GetDebugSymbol(uint32_t idhash, int32_t kind, string *symbol) const
{
	if (m_id_map.count == 0)
		return -1;

	map<uint32_t, string>::iterator it = m_id_map.find(idhash);

	if (it == m_id_map.end())
		return -1;

	list<string> fields = SplitString(it->second, ",");

	switch (kind) {
	case 0:
		for (list<string>::iterator field = fields.begin(); field != fields.end(); ++field) {
			if (field->substr(0, 3) == "id:") {
				*symbol = field->substr(3, string::npos);
				return 0;
			}
		}
		break;
	case 1:
		for (list<string>::iterator field = fields.begin(); field != fields.end(); ++field) {
			if (field->substr(0, 4) == "src:") {
				*symbol = field->substr(4, string::npos);
				return 0;
			}
		}
		break;
	case 2:
		for (list<string>::iterator field = fields.begin(); field != fields.end(); ++field) {
			if (field->substr(0, 5) == "type:") {
				*symbol = field->substr(5, string::npos);
				return 0;
			}
		}
		break;
	default:
		break;
	}
	return -1;
}


/*
 * Reads the debug symbols of the resource's IDs from the .rcd file beside it: a line
 * "k" + 3 bytes + the ID hash in 8 hexadecimal digits + 4 bytes, then the symbol up to
 * CRLF. Only a resource on the host must have one (name ours).
 */
int32_t ResourceObj::LoadResourceDescription(const char *name)
{
	string filename = name;
	string path = filename.substr(0, filename.size() - 3).append("rcd");
	bool optional = true;
	int32_t error;

	if (sce_paf_strncmp(path.substr(0, 6).c_str(), "host0:", 6) == 0)
		optional = false;

	common::SharedPtr<File> fp = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &error);

	if (fp.get() != NULL) {
		common::SharedPtr<Buffer> buffer = CreateBuffer(4096, m_rscallocator);

		fp = OpenBufferedFile(fp, &error, buffer);
	}
	if (fp.get() == NULL) {
		if (!optional)
			return SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
		return 0;
	}

	size_t len = fp->GetFileSize();

	if ((int32_t)len < 0)
		return 0;

	char *data;

	if (m_rscallocator != NULL)
		data = (char *)m_rscallocator->sce_paf_memalign(64, len);
	else
		data = (char *)sce_paf_memalign(64, len);
	if (data == NULL) {
		if (optional)
			return 0;
		return SCE_PAF_ERROR_RESOURCE_NO_MEMORY;
	}
	error = fp->Read(data, len);
	if (error != len) {
		if (m_rscallocator != NULL)
			m_rscallocator->sce_paf_free(data);
		else
			sce_paf_free(data);
		return SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
	}
	if (len != 0) {
		uint32_t pos = 0;
		char *line = data;

		do {
			char *next;

			if (*line == 'k') {
				char hash[12];
				char symbol[512];
				char *out = symbol;
				const char *src;
				uint32_t id;

				sce_paf_strncpy(hash, line + 4, 8);
				hash[8] = 0;
				src = line + 16;
				id = sce_paf_strtoul(hash, NULL, 16);
				next = line + 17;
				pos += 17;
				while (*src != '\n') {
					sce_paf_strncpy(out++, src, 1);
					src = next++;
					pos++;
				}
				/* the CR before the LF */
				out[-1] = 0;
				m_id_map[id] = symbol;
			} else {
				next = line + 1;
				pos++;
				if (*line != '\n') {
					char ch;

					do {
						ch = *next++;
						pos++;
					} while (ch != '\n');
				}
			}
			line = next;
		} while (pos < len);
	}
	if (m_rscallocator != NULL)
		m_rscallocator->sce_paf_free(data);
	else
		sce_paf_free(data);
	fp->Close();
	return 0;
}

}
}
