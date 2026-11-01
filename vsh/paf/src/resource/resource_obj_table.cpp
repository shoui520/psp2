/*
 * paf::resource::ResourceObj's string, texture and sound tables: the string table of a
 * locale read into its own document, and the tables' buffers (the file's name ours: Sony's
 * second file of ResourceObj, by its own string pool).
 */
#include <stddef.h>
#include <stdint.h>
#include <compress/compress.h>
#include <cxml/cxml.h>
#include <file/file.h>
#include <file/localfile.h>
#include <kernel/iofilemgr.h>
#include <memory/heap_allocator.h>
#include <resource/resource_obj.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <sysmem/memblock.h>
#include <sysmem/sysmem.h>
#include <src/paf_types.h>
#include "resource_file.h"

namespace paf {
namespace resource {

/* a buffer for a compressed file, on heap, else the resource's allocator (names ours) */
static inline __attribute__((always_inline)) void *
AllocDecompBuffer(const ResourceObj *rscobj, memory::HeapAllocator *heap, size_t size)
{
	if (heap != NULL)
		return heap->sce_paf_memalign(64, size);
	if (rscobj->m_rscallocator != NULL)
		return rscobj->m_rscallocator->sce_paf_memalign(64, size);
	return sce_paf_memalign(64, size);
}

static inline __attribute__((always_inline)) void
FreeDecompBuffer(const ResourceObj *rscobj, memory::HeapAllocator *heap, void *ptr)
{
	if (heap != NULL)
		heap->sce_paf_free(ptr);
	else if (rscobj->m_rscallocator != NULL)
		rscobj->m_rscallocator->sce_paf_free(ptr);
	else
		sce_paf_free(ptr);
}

static inline __attribute__((always_inline)) void FreeStringBuffer(ResourceObj *rscobj)
{
	if (rscobj->m_rscallocator != NULL)
		rscobj->m_rscallocator->sce_paf_free(rscobj->m_string_buffer);
	else
		sce_paf_free(rscobj->m_string_buffer);
}

/* lets a texture, model or sound table's entries and buffer go after a failed load (name ours) */
template<typename Entry>
static inline __attribute__((always_inline)) void
FreeBufferTable(ResourceObj *rscobj, memory::HeapAllocator *heap, void *&buffer, Entry *&entries)
{
	if (entries != NULL) {
		delete[] entries;
		entries = NULL;
	}

	void *addr = buffer;

	if (heap != NULL)
		heap->sce_paf_free(addr);
	else if (rscobj->m_rscallocator != NULL)
		rscobj->m_rscallocator->sce_paf_free(addr);
	else
		sce_paf_free(buffer);
}

/* lets a table's buffer on heap and its entries go (name ours) */
template<typename Entry>
static inline __attribute__((always_inline)) void
ReleaseHeapTable(ResourceObj *rscobj, memory::HeapAllocator *heap, void *&buffer,
		 Entry *&entries, uint32_t &num)
{
	if (buffer != NULL) {
		if (heap != NULL || (heap = rscobj->m_rscallocator) != NULL)
			heap->sce_paf_free(buffer);
		else
			sce_paf_free(buffer);
		buffer = NULL;
	}
	if (entries != NULL) {
		delete[] entries;
		entries = NULL;
		num = 0;
	}
}

/* lets a texture or model table go (name ours) */
static inline __attribute__((always_inline)) void
ReleaseBufferTable(ResourceObj *rscobj, memory::HeapAllocator *heap, void *&buffer,
		   ResourceObj::BufferEntry *&entries, uint32_t &num)
{
	if (rscobj->m_create_type != 1)
		ReleaseHeapTable(rscobj, heap, buffer, entries, num);
}

/*
 * The common resource's sounds, in a memory block every process shares: the process that
 * loads the common resource first makes it, the others open it (names ours).
 */
SceUID s_sound_storage_uid = -1;
void *s_sound_storage_addr = NULL;	/* written, never read: Sony's keeps it */
void *s_sound_storage_base = NULL;

/* the storage could not be opened, and the sounds went to the heap */
static bool s_sound_storage_on_heap;

/* lets the storage and the sound table's entries go (name ours) */
static inline __attribute__((always_inline)) void
ReleaseSoundStorage(ResourceObj *rscobj, bool create)
{
	if (create)
		sceKernelFreeMemBlock(s_sound_storage_uid);
	else
		sceKernelCloseMemBlock(s_sound_storage_uid);
	s_sound_storage_addr = NULL;
	s_sound_storage_base = NULL;
	if (rscobj->m_sounds != NULL) {
		delete[] rscobj->m_sounds;
		rscobj->m_sounds = NULL;
		rscobj->m_num_sounds = 0;
	}
}

/* the next address at align after size bytes from addr */
static inline __attribute__((always_inline)) uint8_t *
NextTableAddr(uint8_t *addr, size_t size, uint32_t align)
{
	addr += size;
	if (align != 0 && size % align != 0)
		addr += align - size % align;
	return addr;
}

}
}

using namespace paf;
using namespace paf::resource;

/* reads the string table of the locale into the string document, decompressing it on heap */
int32_t ScePafResource_42E4E566(ResourceObj *rscobj, int32_t locale, memory::HeapAllocator *heap)
{
	cxml::Element elm = rscobj->GetLocaleElement(locale);

	if (!elm)
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	if (rscobj->m_string_document != NULL) {
		delete rscobj->m_string_document;
		rscobj->m_string_document = NULL;
		if (rscobj->m_create_type == 0 && rscobj->m_string_buffer != NULL)
			FreeStringBuffer(rscobj);
		rscobj->m_string_buffer = NULL;
	}

	cxml::File src;
	int32_t error = scePafResourceGetAttributeData(&elm, "src", &src);

	if (error != 0)
		return error;

	uint32_t size = src.size;
	int32_t orig_size = src.size;
	const char *compress;
	uint32_t compress_len;
	bool compressed = false;

	error = scePafResourceGetAttributeString(&elm, "compress", &compress, &compress_len);

	if (error == 0 && sce_paf_strncmp(compress, "on", 2) == 0 && compress_len == 2) {
		scePafResourceGetAttributeInteger(&elm, "origsize", &orig_size);
		compressed = true;
	}
	if (rscobj->m_create_type == 0) {
		memory::HeapAllocator *allocator = rscobj->m_rscallocator;

		rscobj->m_string_buffer = allocator != NULL ?
			allocator->sce_paf_memalign(64, orig_size) : sce_paf_memalign(64, orig_size);
		if (rscobj->m_string_buffer == NULL)
			return SCE_PAF_ERROR_RESOURCE_NO_MEMORY;
	}
	if (rscobj->m_create_type != 0) {
		rscobj->m_string_buffer = const_cast<void *>(src.GetAddr());
		if (rscobj->m_string_buffer == NULL)
			return SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
	} else if (!rscobj->m_rscfp->IsOpened()) {
		rscobj->m_rscfp = LocalFile::Open(rscobj->m_filename.c_str(), SCE_O_RDONLY, 0, &error);
		if (rscobj->m_rscfp.get() == NULL) {
			FreeStringBuffer(rscobj);
			return SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
		}
		if (compressed) {
			void *buf = AllocDecompBuffer(rscobj, heap, size);

			if (buf == NULL) {
				rscobj->m_rscfp->Close();
				FreeStringBuffer(rscobj);
				return SCE_PAF_ERROR_RESOURCE_NO_MEMORY;
			}
			error = src.Load(rscobj->m_rscfp, buf, size);
			if (error != 0) {
				FreeDecompBuffer(rscobj, heap, buf);
				rscobj->m_rscfp->Close();
				FreeStringBuffer(rscobj);
				return SCE_PAF_ERROR_RESOURCE_NO_MEMORY;
			}
			if (compress::Decompress(rscobj->m_string_buffer, orig_size, buf, size,
						 compress::DecompressType_Zlib) < 0) {
				FreeDecompBuffer(rscobj, heap, buf);
				rscobj->m_rscfp->Close();
				FreeStringBuffer(rscobj);
				return SCE_PAF_ERROR_RESOURCE_DECOMPRESS;
			}
			FreeDecompBuffer(rscobj, heap, buf);
		} else {
			error = src.Load(rscobj->m_rscfp, rscobj->m_string_buffer, size);
			if (error != 0) {
				rscobj->m_rscfp->Close();
				FreeStringBuffer(rscobj);
				return error;
			}
		}
		rscobj->m_rscfp->Close();
	} else {
		if (compressed) {
			void *buf = AllocDecompBuffer(rscobj, heap, size);

			if (buf == NULL) {
				FreeStringBuffer(rscobj);
				return SCE_PAF_ERROR_RESOURCE_NO_MEMORY;
			}
			error = src.Load(rscobj->m_rscfp, buf, size);
			if (error != 0) {
				FreeDecompBuffer(rscobj, heap, buf);
				FreeStringBuffer(rscobj);
				return SCE_PAF_ERROR_RESOURCE_NO_MEMORY;
			}
			if (compress::Decompress(rscobj->m_string_buffer, orig_size, buf, size,
						 compress::DecompressType_Zlib) < 0) {
				FreeDecompBuffer(rscobj, heap, buf);
				FreeStringBuffer(rscobj);
				return SCE_PAF_ERROR_RESOURCE_DECOMPRESS;
			}
			FreeDecompBuffer(rscobj, heap, buf);
		} else {
			error = src.Load(rscobj->m_rscfp, rscobj->m_string_buffer, size);
			if (error != 0) {
				FreeStringBuffer(rscobj);
				return error;
			}
		}
	}
	rscobj->m_string_document = new cxml::Document;
	error = rscobj->m_string_document->CreateFromBuffer(rscobj->m_string_buffer, orig_size);
	if (error != 0) {
		if (rscobj->m_string_document != NULL) {
			delete rscobj->m_string_document;
			rscobj->m_string_document = NULL;
		}
		if (rscobj->m_create_type == 0)
			FreeStringBuffer(rscobj);
		return error;
	}
	return 0;
}

/*
 * Reads the texture table's files into one buffer, on heap, each at a multiple of align:
 * the entry of this resolution or model where the texture has one. A texture to be made a
 * surface later is left empty.
 */
int32_t ScePafResource_B72D030D(ResourceObj *rscobj, uint32_t align, memory::HeapAllocator *heap)
{
	int32_t error;

	if (rscobj->m_create_type == 1)
		return 0;

	int32_t num = 0;
	const char *type;
	uint32_t type_len;
	const char *ignore;
	uint32_t ignore_len;
	size_t total = rscobj->GetTableBufferSize(TableType_Texture, align, &num);

	rscobj->m_num_textures = num;
	if (num == 0)
		return 0;

	void *buffer = AllocDecompBuffer(rscobj, heap, total);

	rscobj->m_texture_buffer = buffer;
	if (buffer == NULL)
		return SCE_PAF_ERROR_RESOURCE_NO_MEMORY;
	rscobj->m_textures = new ResourceObj::BufferEntry[num];

	cxml::Element elm = rscobj->GetTableElement(TableType_Texture);

	if (!elm) {
		FreeBufferTable(rscobj, heap, rscobj->m_texture_buffer, rscobj->m_textures);
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	}
	elm = elm.GetFirstChild();

	uint8_t *addr = (uint8_t *)buffer;

	for (uint32_t i = 0; i < (uint32_t)num; i++) {
		cxml::File file;

		error = scePafResourceGetAttributeString(&elm, "ignore_make_surface_first", &ignore,
							 &ignore_len);
		if (error == 0 && sce_paf_strncmp(ignore, "on", 2) == 0 && ignore_len == 2) {
			elm = elm.GetNextSibling();
			continue;
		}
		error = scePafResourceGetAttributeIdIntLpb(&elm, "id", &rscobj->m_textures[i].idhash);
		if (error != 0) {
			FreeBufferTable(rscobj, heap, rscobj->m_texture_buffer, rscobj->m_textures);
			return error;
		}
		error = scePafResourceGetAttributeData(&elm, "src", &file);
		if (error != 0) {
			FreeBufferTable(rscobj, heap, rscobj->m_texture_buffer, rscobj->m_textures);
			return error;
		}

		size_t size = file.size;

		rscobj->m_textures[i].addr = addr;
		rscobj->m_textures[i].size = size;
		error = scePafResourceGetAttributeString(&elm, "type", &type, &type_len);
		if (error != 0) {
			FreeBufferTable(rscobj, heap, rscobj->m_texture_buffer, rscobj->m_textures);
			return error;
		}
		rscobj->m_textures[i].type = rscobj->GetTextureType(type);

		cxml::Element entry = rscobj->GetOverrideElement(elm);
		cxml::Element child = elm.GetFirstChild();
		uint8_t *child_addr = addr;
		cxml::File child_file;

		for (uint32_t j = 0; j < elm.GetNumChild(); j++) {
			error = scePafResourceGetAttributeData(&child, "src", &child_file);
			if (error != 0) {
				FreeBufferTable(rscobj, heap, rscobj->m_texture_buffer, rscobj->m_textures);
				return error;
			}

			size_t child_size = child_file.size;

			error = scePafResourceGetAttributeString(&child, "type", &type, &type_len);
			if (error != 0) {
				FreeBufferTable(rscobj, heap, rscobj->m_texture_buffer, rscobj->m_textures);
				return error;
			}
			if (child.doc == entry.doc && child.offset == entry.offset) {
				file = child_file;
				rscobj->m_textures[i].addr = child_addr;
				rscobj->m_textures[i].size = child_size;
				size = child_size;
				rscobj->m_textures[i].type = rscobj->GetTextureType(type);
			}
			child_addr += child_size;
			if (align != 0 && child_size % align != 0)
				child_addr += align - child_size % align;
			child = child.GetNextSibling();
		}
		error = scePafResourceGetAttributeString(&entry, "ignore_make_surface_first", &ignore,
							 &ignore_len);
		if (error == 0 && sce_paf_strncmp(ignore, "on", 2) == 0 && ignore_len == 2) {
			rscobj->m_textures[i].idhash = 0;
			rscobj->m_textures[i].addr = NULL;
			rscobj->m_textures[i].size = 0;
			rscobj->m_textures[i].type = 0;
			elm = elm.GetNextSibling();
			continue;
		}
		if (!rscobj->m_rscfp->IsOpened()) {
			rscobj->m_rscfp = LocalFile::Open(rscobj->m_filename.c_str(), SCE_O_RDONLY, 0,
							  &error);
			if (rscobj->m_rscfp.get() == NULL) {
				FreeBufferTable(rscobj, heap, rscobj->m_texture_buffer, rscobj->m_textures);
				return SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
			}
			error = file.Load(rscobj->m_rscfp, rscobj->m_textures[i].addr,
					  rscobj->m_textures[i].size);
			if (error != 0) {
				rscobj->m_rscfp->Close();
				FreeBufferTable(rscobj, heap, rscobj->m_texture_buffer, rscobj->m_textures);
				return error;
			}
			rscobj->m_rscfp->Close();
		} else {
			error = file.Load(rscobj->m_rscfp, rscobj->m_textures[i].addr,
					  rscobj->m_textures[i].size);
			if (error != 0) {
				FreeBufferTable(rscobj, heap, rscobj->m_texture_buffer, rscobj->m_textures);
				return error;
			}
		}
		addr += size;
		if (align != 0 && size % align != 0)
			addr += align - size % align;
		elm = elm.GetNextSibling();
	}
	return 0;
}

/* lets the texture table go */
void ScePafResource_D60FF214(ResourceObj *rscobj, memory::HeapAllocator *heap)
{
	ReleaseBufferTable(rscobj, heap, rscobj->m_texture_buffer, rscobj->m_textures,
			   rscobj->m_num_textures);
}

/* reads the model table's files into one buffer, on heap, each at a multiple of align */
int32_t ScePafResource_933C078B(ResourceObj *rscobj, uint32_t align, memory::HeapAllocator *heap)
{
	int32_t error;

	if (rscobj->m_create_type == 1)
		return 0;

	int32_t num = 0;
	size_t total = rscobj->GetTableBufferSize(TableType_Model, align, &num);

	rscobj->m_num_models = num;
	if (num == 0)
		return 0;

	void *buffer = AllocDecompBuffer(rscobj, heap, total);

	rscobj->m_model_buffer = buffer;
	if (buffer == NULL)
		return SCE_PAF_ERROR_RESOURCE_NO_MEMORY;
	rscobj->m_models = new ResourceObj::BufferEntry[num];

	cxml::Element elm = rscobj->GetTableElement(TableType_Model);

	if (!elm) {
		FreeBufferTable(rscobj, heap, rscobj->m_model_buffer, rscobj->m_models);
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	}
	elm = elm.GetFirstChild();

	uint8_t *addr = (uint8_t *)buffer;

	for (uint32_t i = 0; i < (uint32_t)num; i++) {
		cxml::File file;
		const char *type;
		uint32_t type_len;

		error = scePafResourceGetAttributeIdIntLpb(&elm, "id", &rscobj->m_models[i].idhash);
		if (error != 0) {
			FreeBufferTable(rscobj, heap, rscobj->m_model_buffer, rscobj->m_models);
			return error;
		}
		error = scePafResourceGetAttributeData(&elm, "src", &file);
		if (error != 0) {
			FreeBufferTable(rscobj, heap, rscobj->m_model_buffer, rscobj->m_models);
			return error;
		}

		size_t size = file.size;

		rscobj->m_models[i].size = size;
		if (!rscobj->m_rscfp->IsOpened()) {
			rscobj->m_rscfp = LocalFile::Open(rscobj->m_filename.c_str(), SCE_O_RDONLY, 0,
							  &error);
			if (rscobj->m_rscfp.get() == NULL) {
				FreeBufferTable(rscobj, heap, rscobj->m_model_buffer, rscobj->m_models);
				return SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
			}
			error = file.Load(rscobj->m_rscfp, addr, size);
			if (error != 0) {
				rscobj->m_rscfp->Close();
				FreeBufferTable(rscobj, heap, rscobj->m_model_buffer, rscobj->m_models);
				return error;
			}
			rscobj->m_rscfp->Close();
		} else {
			error = file.Load(rscobj->m_rscfp, addr, size);
			if (error != 0) {
				FreeBufferTable(rscobj, heap, rscobj->m_model_buffer, rscobj->m_models);
				return error;
			}
		}
		rscobj->m_models[i].addr = addr;
		addr += size;
		if (align != 0 && size % align != 0)
			addr += align - size % align;
		error = scePafResourceGetAttributeString(&elm, "type", &type, &type_len);
		if (error != 0) {
			FreeBufferTable(rscobj, heap, rscobj->m_model_buffer, rscobj->m_models);
			return error;
		}
		rscobj->m_models[i].type = rscobj->GetModelType(type);
		elm = elm.GetNextSibling();
	}
	return 0;
}

/* lets the model table go */
void ScePafResource_C240421A(ResourceObj *rscobj, memory::HeapAllocator *heap)
{
	ReleaseBufferTable(rscobj, heap, rscobj->m_model_buffer, rscobj->m_models,
			   rscobj->m_num_models);
}

/*
 * Reads the sound table's files into the shared storage, each at a multiple of align: the
 * process that makes the storage reads them, the others only take their addresses. Fails
 * with SCE_PAF_ERROR_RESOURCE_NO_SOUND_STORAGE when there is no storage to open (name ours).
 */
static int32_t LoadSoundStorage(ResourceObj *rscobj, uint32_t align, bool create)
{
	int32_t error;
	int32_t num = 0;
	int32_t rate;
	const char *type;
	uint32_t type_len;
	size_t total = rscobj->GetTableBufferSize(TableType_Sound, align, &num);

	if (create) {
		MemBlockUserOpt opt;

		sce_paf_memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = ALLOC_ATTR_CLIENT_INFO | ALLOC_ATTR_HAS_CLIENT_INFO;
		opt.openFlags = MEMBLOCK_OPEN_RO;
		for (int i = 0; i < 8; i++)
			opt.capabilities[i] = 0;

		size_t pad = 0;

		if (total % 0x1000 != 0)
			pad = 0x1000 - total % 0x1000;

		SceUID uid = sceKernelAllocMemBlock("ScePafCRSoundDataStorage", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
						    total + pad,
						    (const SceKernelAllocMemBlockOpt *)&opt);

		if (uid < 0)
			return uid;
		s_sound_storage_uid = uid;
	} else {
		SceUID uid = sceKernelOpenMemBlock("ScePafCRSoundDataStorage", MEMBLOCK_OPEN_RO);

		if (uid < 0)
			return SCE_PAF_ERROR_RESOURCE_NO_SOUND_STORAGE;
		s_sound_storage_uid = uid;
	}

	error = sceKernelGetMemBlockBase(s_sound_storage_uid, &s_sound_storage_base);
	if (error < 0) {
		ReleaseSoundStorage(rscobj, create);
		return error;
	}

	uint8_t *addr = (uint8_t *)s_sound_storage_base;

	s_sound_storage_addr = s_sound_storage_base;
	rscobj->m_sound_buffer = s_sound_storage_base;
	rscobj->m_sounds = new ResourceObj::SoundEntry[num];

	cxml::Element elm = rscobj->GetTableElement(TableType_Sound);

	if (!elm) {
		ReleaseSoundStorage(rscobj, create);
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	}
	elm = elm.GetFirstChild();

	for (uint32_t i = 0; i < (uint32_t)num; i++) {
		cxml::File file;

		error = scePafResourceGetAttributeIdIntLpb(&elm, "id", &rscobj->m_sounds[i].idhash);
		if (error != 0) {
			ReleaseSoundStorage(rscobj, create);
			return error;
		}
		error = scePafResourceGetAttributeData(&elm, "src", &file);
		if (error != 0) {
			cxml::File left;
			cxml::File right;

			if ((scePafResourceGetAttributeData(&elm, "left", &left) |
			     scePafResourceGetAttributeData(&elm, "right", &right)) != 0) {
				ReleaseSoundStorage(rscobj, create);
				return error;
			}

			size_t left_size = left.size;

			rscobj->m_sounds[i].size1 = left_size;
			if (create) {
				if (!rscobj->m_rscfp->IsOpened()) {
					rscobj->m_rscfp = LocalFile::Open(rscobj->m_filename.c_str(), SCE_O_RDONLY, 0,
										  &error);
					if (rscobj->m_rscfp.get() == NULL) {
						ReleaseSoundStorage(rscobj, create);
						return SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
					}
					error = left.Load(rscobj->m_rscfp, addr, left_size);
					if (error != 0) {
						rscobj->m_rscfp->Close();
						ReleaseSoundStorage(rscobj, create);
						return error;
					}
					rscobj->m_rscfp->Close();
				} else {
					error = left.Load(rscobj->m_rscfp, addr, left_size);
					if (error != 0) {
						ReleaseSoundStorage(rscobj, create);
						return error;
					}
				}
			}
			rscobj->m_sounds[i].addr1 = addr;
			addr = NextTableAddr(addr, left_size, align);

			size_t right_size = right.size;

			rscobj->m_sounds[i].size2 = right_size;
			if (create) {
				if (!rscobj->m_rscfp->IsOpened()) {
					rscobj->m_rscfp = LocalFile::Open(rscobj->m_filename.c_str(), SCE_O_RDONLY, 0,
										  &error);
					if (rscobj->m_rscfp.get() == NULL) {
						ReleaseSoundStorage(rscobj, create);
						return SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
					}
					error = right.Load(rscobj->m_rscfp, addr, right_size);
					if (error != 0) {
						rscobj->m_rscfp->Close();
						ReleaseSoundStorage(rscobj, create);
						return error;
					}
					rscobj->m_rscfp->Close();
				} else {
					error = right.Load(rscobj->m_rscfp, addr, right_size);
					if (error != 0) {
						ReleaseSoundStorage(rscobj, create);
						return error;
					}
				}
			}
			rscobj->m_sounds[i].addr2 = addr;
			addr = NextTableAddr(addr, right_size, align);
			error = scePafResourceGetAttributeInteger(&elm, "rate", &rate);
			if (error != 0) {
				ReleaseSoundStorage(rscobj, create);
				return error;
			}
			rscobj->m_sounds[i].samplerate = rate;
			error = scePafResourceGetAttributeString(&elm, "type", &type, &type_len);
			if (error != 0) {
				ReleaseSoundStorage(rscobj, create);
				return error;
			}
			rscobj->m_sounds[i].type = rscobj->GetSoundType(type);
		} else {
			size_t size = file.size;

			rscobj->m_sounds[i].size1 = size;
			rscobj->m_sounds[i].size2 = 0;
			if (create) {
				if (!rscobj->m_rscfp->IsOpened()) {
					rscobj->m_rscfp = LocalFile::Open(rscobj->m_filename.c_str(), SCE_O_RDONLY, 0,
										  &error);
					if (rscobj->m_rscfp.get() == NULL) {
						ReleaseSoundStorage(rscobj, create);
						return SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
					}
					error = file.Load(rscobj->m_rscfp, addr, size);
					if (error != 0) {
						rscobj->m_rscfp->Close();
						ReleaseSoundStorage(rscobj, create);
						return error;
					}
					rscobj->m_rscfp->Close();
				} else {
					error = file.Load(rscobj->m_rscfp, addr, size);
					if (error != 0) {
						ReleaseSoundStorage(rscobj, create);
						return error;
					}
				}
			}
			rscobj->m_sounds[i].addr1 = addr;
			rscobj->m_sounds[i].addr2 = NULL;
			addr = NextTableAddr(addr, size, align);
			error = scePafResourceGetAttributeInteger(&elm, "rate", &rate);
			if (error != 0) {
				ReleaseSoundStorage(rscobj, create);
				return error;
			}
			rscobj->m_sounds[i].samplerate = rate;
			error = scePafResourceGetAttributeString(&elm, "type", &type, &type_len);
			if (error != 0) {
				ReleaseSoundStorage(rscobj, create);
				return error;
			}
			rscobj->m_sounds[i].type = rscobj->GetSoundType(type);
		}
		elm = elm.GetNextSibling();
	}
	return 0;
}

/*
 * Reads the sound table's files into the common resource's shared storage when shared (made
 * when create, else opened), else, or when there is no storage to open, into one buffer on
 * heap; each at a multiple of align.
 */
int32_t ScePafResource_EA11BD0B(ResourceObj *rscobj, uint32_t align, memory::HeapAllocator *heap,
				bool create, bool shared)
{
	int32_t error;

	if (rscobj->m_create_type == 1)
		return 0;

	int32_t num = 0;
	int32_t rate;
	const char *type;
	uint32_t type_len;
	size_t total = rscobj->GetTableBufferSize(TableType_Sound, align, &num);

	rscobj->m_num_sounds = num;
	if (num == 0)
		return 0;
	if (shared) {
		error = LoadSoundStorage(rscobj, align, create);
		if (error == 0)
			return 0;
		if (error != SCE_PAF_ERROR_RESOURCE_NO_SOUND_STORAGE)
			return error;
		s_sound_storage_on_heap = true;
	}

	void *buffer = AllocDecompBuffer(rscobj, heap, total);

	rscobj->m_sound_buffer = buffer;
	if (buffer == NULL)
		return SCE_PAF_ERROR_RESOURCE_NO_MEMORY;
	rscobj->m_sounds = new ResourceObj::SoundEntry[num];

	cxml::Element elm = rscobj->GetTableElement(TableType_Sound);

	if (!elm) {
		FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	}
	elm = elm.GetFirstChild();

	uint8_t *addr = (uint8_t *)buffer;

	for (uint32_t i = 0; i < (uint32_t)num; i++) {
		cxml::File file;

		error = scePafResourceGetAttributeIdIntLpb(&elm, "id", &rscobj->m_sounds[i].idhash);
		if (error != 0) {
			FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
			return error;
		}
		error = scePafResourceGetAttributeData(&elm, "src", &file);
		if (error != 0) {
			cxml::File left;
			cxml::File right;

			if ((scePafResourceGetAttributeData(&elm, "left", &left) |
			     scePafResourceGetAttributeData(&elm, "right", &right)) != 0) {
				FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
				return error;
			}

			size_t left_size = left.size;

			rscobj->m_sounds[i].size1 = left_size;
			if (!rscobj->m_rscfp->IsOpened()) {
				rscobj->m_rscfp = LocalFile::Open(rscobj->m_filename.c_str(), SCE_O_RDONLY, 0,
									  &error);
				if (rscobj->m_rscfp.get() == NULL) {
					FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
					return SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
				}
				error = left.Load(rscobj->m_rscfp, addr, left_size);
				if (error != 0) {
					rscobj->m_rscfp->Close();
					FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
					return error;
				}
				rscobj->m_rscfp->Close();
			} else {
				error = left.Load(rscobj->m_rscfp, addr, left_size);
				if (error != 0) {
					FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
					return error;
				}
			}
			rscobj->m_sounds[i].addr1 = addr;
			addr = NextTableAddr(addr, left_size, align);

			size_t right_size = right.size;

			rscobj->m_sounds[i].size2 = right_size;
			if (!rscobj->m_rscfp->IsOpened()) {
				rscobj->m_rscfp = LocalFile::Open(rscobj->m_filename.c_str(), SCE_O_RDONLY, 0,
									  &error);
				if (rscobj->m_rscfp.get() == NULL) {
					FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
					return SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
				}
				error = right.Load(rscobj->m_rscfp, addr, right_size);
				if (error != 0) {
					rscobj->m_rscfp->Close();
					FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
					return error;
				}
				rscobj->m_rscfp->Close();
			} else {
				error = right.Load(rscobj->m_rscfp, addr, right_size);
				if (error != 0) {
					FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
					return error;
				}
			}
			rscobj->m_sounds[i].addr2 = addr;
			addr = NextTableAddr(addr, right_size, align);
			error = scePafResourceGetAttributeInteger(&elm, "rate", &rate);
			if (error != 0) {
				FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
				return error;
			}
			rscobj->m_sounds[i].samplerate = rate;
			error = scePafResourceGetAttributeString(&elm, "type", &type, &type_len);
			if (error != 0) {
				FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
				return error;
			}
			rscobj->m_sounds[i].type = rscobj->GetSoundType(type);
		} else {
			size_t size = file.size;

			rscobj->m_sounds[i].size1 = size;
			rscobj->m_sounds[i].size2 = 0;
			if (!rscobj->m_rscfp->IsOpened()) {
				rscobj->m_rscfp = LocalFile::Open(rscobj->m_filename.c_str(), SCE_O_RDONLY, 0,
									  &error);
				if (rscobj->m_rscfp.get() == NULL) {
					FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
					return SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
				}
				error = file.Load(rscobj->m_rscfp, addr, size);
				if (error != 0) {
					rscobj->m_rscfp->Close();
					FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
					return error;
				}
				rscobj->m_rscfp->Close();
			} else {
				error = file.Load(rscobj->m_rscfp, addr, size);
				if (error != 0) {
					FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
					return error;
				}
			}
			rscobj->m_sounds[i].addr1 = addr;
			rscobj->m_sounds[i].addr2 = NULL;
			addr = NextTableAddr(addr, size, align);
			error = scePafResourceGetAttributeInteger(&elm, "rate", &rate);
			if (error != 0) {
				FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
				return error;
			}
			rscobj->m_sounds[i].samplerate = rate;
			error = scePafResourceGetAttributeString(&elm, "type", &type, &type_len);
			if (error != 0) {
				FreeBufferTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds);
				return error;
			}
			rscobj->m_sounds[i].type = rscobj->GetSoundType(type);
		}
		elm = elm.GetNextSibling();
	}
	return 0;
}

/* lets the sound table go, from the shared storage or the heap it was read into */
void ScePafResource_556665FC(ResourceObj *rscobj, memory::HeapAllocator *heap, bool create,
			     bool shared)
{
	if (rscobj->m_create_type == 1)
		return;
	if (shared) {
		if (!s_sound_storage_on_heap) {
			ReleaseSoundStorage(rscobj, create);
			return;
		}
		s_sound_storage_on_heap = false;
	}
	ReleaseHeapTable(rscobj, heap, rscobj->m_sound_buffer, rscobj->m_sounds,
			 rscobj->m_num_sounds);
}

namespace paf {
namespace resource {

/* lets a table's entries go after a failed read of a table in memory (name ours) */
template<typename Entry>
static inline __attribute__((always_inline)) void DeleteTableEntries(Entry *&entries)
{
	if (entries != NULL) {
		delete[] entries;
		entries = NULL;
	}
}

/* lets a table of the resource's memory go (name ours) */
template<typename Entry>
static inline __attribute__((always_inline)) void
DeleteTableEntries(Entry *&entries, uint32_t &num)
{
	if (entries != NULL) {
		delete[] entries;
		entries = NULL;
		num = 0;
	}
}

/*
 * Points the texture table's entries at their files in the resource's memory: the entry of
 * this resolution or model where the texture has one.
 */
int32_t ResourceObj::SetTextureTable(uint32_t align)
{
	int32_t error;
	int32_t num = 0;
	const char *type;
	uint32_t type_len;

	GetTableBufferSize(TableType_Texture, align, &num);
	m_num_textures = num;
	if (num == 0)
		return 0;
	m_textures = new BufferEntry[num];

	cxml::Element elm = GetTableElement(TableType_Texture);

	if (!elm) {
		DeleteTableEntries(m_textures);
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	}
	elm = elm.GetFirstChild();

	for (uint32_t i = 0; i < (uint32_t)num; i++) {
		cxml::File file;

		error = scePafResourceGetAttributeIdIntLpb(&elm, "id", &m_textures[i].idhash);
		if (error != 0) {
			DeleteTableEntries(m_textures);
			return error;
		}
		error = scePafResourceGetAttributeData(&elm, "src", &file);
		if (error != 0) {
			DeleteTableEntries(m_textures);
			return error;
		}
		m_textures[i].addr = (void *)file.GetAddr();
		m_textures[i].size = file.size;
		error = scePafResourceGetAttributeString(&elm, "type", &type, &type_len);
		if (error != 0) {
			DeleteTableEntries(m_textures);
			return error;
		}
		m_textures[i].type = GetTextureType(type);

		cxml::Element entry = GetOverrideElement(elm);
		cxml::Element child = elm.GetFirstChild();
		cxml::File child_file;

		for (uint32_t j = 0; j < elm.GetNumChild(); j++) {
			error = scePafResourceGetAttributeData(&child, "src", &child_file);
			if (error != 0) {
				DeleteTableEntries(m_textures);
				return error;
			}
			error = scePafResourceGetAttributeString(&child, "type", &type, &type_len);
			if (error != 0) {
				DeleteTableEntries(m_textures);
				return error;
			}
			if (child.doc == entry.doc && child.offset == entry.offset) {
				file = child_file;
				m_textures[i].size = child_file.size;
				m_textures[i].addr = (void *)child_file.GetAddr();
				m_textures[i].type = GetTextureType(type);
			}
			child = child.GetNextSibling();
		}
		elm = elm.GetNextSibling();
	}
	return 0;
}

/* points the sound table's entries at their files (mono, or left and right) in the resource's memory */
int32_t ResourceObj::SetSoundTable(uint32_t align)
{
	int32_t error;
	int32_t num = 0;
	int32_t rate;
	const char *type;
	uint32_t type_len;

	GetTableBufferSize(TableType_Sound, align, &num);
	m_num_sounds = num;
	if (num == 0)
		return 0;
	m_sounds = new SoundEntry[num];

	cxml::Element elm = GetTableElement(TableType_Sound);

	if (!elm) {
		DeleteTableEntries(m_sounds);
		return SCE_PAF_ERROR_RESOURCE_NOT_FOUND;
	}
	elm = elm.GetFirstChild();

	for (uint32_t i = 0; i < (uint32_t)num; i++) {
		cxml::File file;

		error = scePafResourceGetAttributeIdIntLpb(&elm, "id", &m_sounds[i].idhash);
		if (error != 0) {
			DeleteTableEntries(m_sounds);
			return error;
		}
		error = scePafResourceGetAttributeData(&elm, "src", &file);
		if (error != 0) {
			cxml::File left;
			cxml::File right;

			if ((scePafResourceGetAttributeData(&elm, "left", &left) |
			     scePafResourceGetAttributeData(&elm, "right", &right)) != 0) {
				DeleteTableEntries(m_sounds);
				return error;
			}
			m_sounds[i].addr1 = (void *)left.GetAddr();
			m_sounds[i].addr2 = (void *)right.GetAddr();
			m_sounds[i].size1 = left.size;
			m_sounds[i].size2 = right.size;
			error = scePafResourceGetAttributeInteger(&elm, "rate", &rate);
			if (error != 0) {
				DeleteTableEntries(m_sounds);
				return error;
			}
			m_sounds[i].samplerate = rate;
			error = scePafResourceGetAttributeString(&elm, "type", &type, &type_len);
			if (error != 0) {
				DeleteTableEntries(m_sounds);
				return error;
			}
		} else {
			m_sounds[i].addr1 = (void *)file.GetAddr();
			m_sounds[i].addr2 = NULL;
			m_sounds[i].size1 = file.size;
			m_sounds[i].size2 = 0;
			error = scePafResourceGetAttributeInteger(&elm, "rate", &rate);
			if (error != 0) {
				DeleteTableEntries(m_sounds);
				return error;
			}
			m_sounds[i].samplerate = rate;
			error = scePafResourceGetAttributeString(&elm, "type", &type, &type_len);
			if (error != 0) {
				DeleteTableEntries(m_sounds);
				return error;
			}
		}
		m_sounds[i].type = GetSoundType(type);
		elm = elm.GetNextSibling();
	}
	return 0;
}

}
}

/* closes the resource's file */
int32_t ScePafResource_CEED7338(ResourceObj *rscobj)
{
	if (rscobj->m_rscfp.get() != NULL)
		return rscobj->m_rscfp->Close();
	return 0;
}

namespace paf {
namespace resource {

/*
 * Reads the resource's document from its file, through a buffer of its own; with load_type's
 * bit 2, also the IDs' debug symbols from the .rcd file beside it.
 */
ResourceObj::ResourceObj(const char *name, const Option *option)
	: m_document(NULL), m_string_document(NULL), m_rscallocator(option->allocator),
	  m_string_buffer(NULL), m_texture_buffer(NULL), m_model_buffer(NULL),
	  m_sound_buffer(NULL), m_textures(NULL), m_models(NULL), m_sounds(NULL),
	  m_num_textures(0), m_num_models(0), m_num_sounds(0), m_load_status(0),
	  m_create_type(0)
{
	int32_t error;

	m_filename = name;
	m_document = new cxml::Document;
	m_rscfp = LocalFile::Open(m_filename.c_str(), SCE_O_RDONLY, 0, &error);
	if (m_rscfp.get() != NULL) {
		common::SharedPtr<Buffer> buffer = CreateBuffer(4096, m_rscallocator);

		m_rscfp = OpenBufferedFile(m_rscfp, &error, buffer);
	}
	if (m_rscfp.get() != NULL) {
		m_document->SetAllocator(option->cxml_allocator_func, m_rscallocator);
		error = m_document->CreateFromFile(m_rscfp);
		if (error != 0) {
			m_document->Clear();
			m_rscfp->Close();
			m_load_status = error;
		}
		if ((option->load_type & 4) != 0) {
			error = LoadResourceDescription(m_filename.c_str());
			if (error != 0)
				m_load_status = error;
		}
	} else {
		m_load_status = SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
	}
}

/* reads the resource's document from size bytes at addr, which the resource keeps using */
ResourceObj::ResourceObj(void *addr, uint32_t size, const Option *option)
	: m_document(NULL), m_string_document(NULL), m_rscallocator(option->allocator),
	  m_string_buffer(NULL), m_texture_buffer(NULL), m_model_buffer(NULL),
	  m_sound_buffer(NULL), m_textures(NULL), m_models(NULL), m_sounds(NULL),
	  m_num_textures(0), m_num_models(0), m_num_sounds(0), m_load_status(0),
	  m_create_type(1)
{
	m_filename = "";
	m_document = new cxml::Document;
	m_document->SetAllocator(option->cxml_allocator_func, m_rscallocator);

	int32_t error = m_document->CreateFromBuffer(addr, size);

	if (error != 0)
		m_load_status = error;
}

ResourceObj::~ResourceObj()
{
	if (m_create_type == 1) {
		DeleteTableEntries(m_textures, m_num_textures);
		DeleteTableEntries(m_models, m_num_models);
		DeleteTableEntries(m_sounds, m_num_sounds);
	}
	if (m_string_document != NULL) {
		delete m_string_document;
		m_string_document = NULL;
		if (m_create_type == 0 && m_string_buffer != NULL)
			FreeStringBuffer(this);
		m_string_buffer = NULL;
	}
	if (m_document != NULL) {
		if (m_create_type == 0)
			m_document->Clear();
		delete m_document;
		m_document = NULL;
	}
	if (m_rscfp.get() != NULL)
		m_rscfp->Close();
}

}
}
