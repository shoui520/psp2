/*
 * paf::resource::ResourceObj, as far as converted C++ uses it: a plugin's
 * resource (RCO), its tables by type. Names after ScePaf-RE; the class itself
 * is still C (src/resource/resource_obj.h).
 */
#ifndef PAF_RESOURCE_RESOURCE_OBJ_H
#define PAF_RESOURCE_RESOURCE_OBJ_H

#include <stdint.h>
#include <cxml/cxml.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <widget/core/environment.h>

namespace paf {

class File;

namespace memory {
class HeapAllocator;
}

namespace resource {

enum TableType {
	TableType_Unknown = -1,
	TableType_Page = 0,
	TableType_Style = 1,
	TableType_String = 2,
	TableType_Texture = 3,
	TableType_Model = 4,
	TableType_Sound = 5,
	TableType_SoundIndex = 6,
	TableType_File = 7,
	TableType_Template = 8,
	TableType_Alias = 9
};

class ResourceObj {
public:
	/* how the resource is read and what its documents are allocated with */
	class Option {
	public:
		Option() : load_type(0), allocator(NULL), cxml_allocator_func(cxml::DefaultAllocator)
		{
		}

		uint32_t load_type;
		memory::HeapAllocator *allocator;
		cxml::AllocatorFunction cxml_allocator_func;
	};

	/* a texture or model of the resource */
	class BufferEntry {
	public:
		uint32_t idhash;
		void *addr;
		size_t size;
		uint32_t type;
	};

	/* a sound of the resource */
	class SoundEntry {
	public:
		uint32_t idhash;
		void *addr1;
		void *addr2;
		size_t size1;
		size_t size2;
		int32_t samplerate;
		uint32_t type;
	};

	ResourceObj(void *addr, uint32_t size, const Option *option);
	ResourceObj(const char *name, const Option *option);
	~ResourceObj();

	int32_t GetLoadStatus() const;
	/* whether the resource's document holds the ID */
	bool IsRegisteredID(uint32_t idhash);
	bool IsRegisteredID(const char *id, uint32_t mode);
	cxml::Element GetTableElement(TableType type) const;
	/* the page element of an ID; 0 when found */
	int32_t GetPage(uint32_t idhash, cxml::Element &elm) const;
	int32_t GetPage(const char *id, cxml::Element &elm) const;
	/* the string of an ID; 0 when found */
	int32_t GetString(uint32_t idhash, const wchar_t **str, uint32_t *length) const;
	/* the "id:", "src:" or "type:" (kind 0, 1, 2) of an ID's debug symbol; -1 without one */
	/* the ID the alias table gives the ID, for this model's entry first; empty without one (name ours) */
	IDParam GetAliasID(const IDParam &id) const;
	int32_t GetDebugSymbol(uint32_t idhash, int32_t kind, string *symbol) const;
	const char *GetFileName() const;
	int32_t GetTextureFromBuffer(const cxml::Element &elm, void **addr, uint32_t *size,
				     uint32_t *type) const;
	int32_t GetTextureFromFile(const cxml::Element &elm, cxml::File *src, uint32_t *size,
				   uint32_t *type) const;
	/*
	 * The child of a texture element for this resolution or model, else the element
	 * (names ours, not converted yet: 0x8106e652, the tables 0x81072ad6, 0x81072cfa).
	 */
	cxml::Element GetOverrideElement(const cxml::Element &elm) const;
	int32_t SetTextureTable(uint32_t flag);
	int32_t SetSoundTable(uint32_t flag);

	void SetResolutionResource(ui::ResolutionResource resource)
	{
		m_resolution_resource = resource;
	}

	/* the layout of the C struct */
	common::SharedPtr<File> m_rscfp;
	string m_filename;
	cxml::Document *m_document;
	cxml::Document *m_string_document;
	memory::HeapAllocator *m_rscallocator;
	void *m_string_buffer;
	void *m_texture_buffer;
	void *m_model_buffer;
	void *m_sound_buffer;
	BufferEntry *m_textures;
	BufferEntry *m_models;
	SoundEntry *m_sounds;
	uint32_t m_num_textures;
	uint32_t m_num_models;
	uint32_t m_num_sounds;
	int32_t m_load_status;
	uint32_t m_create_type;
	/* the debug symbols' map, not read from C++ yet */
	uint8_t m_id_map[12];
	ui::ResolutionResource m_resolution_resource;
};

}
}

/* the texture table's loads (not converted yet; the names are their NIDs) */
extern "C" {
/* reads the string table for the locale */
int32_t ScePafResource_42E4E566(paf::resource::ResourceObj *rscobj, int32_t locale,
				paf::memory::HeapAllocator *heap);
/* lets the sound table's buffers go */
int32_t ScePafResource_556665FC(paf::resource::ResourceObj *rscobj, int32_t arg, bool mode,
				bool pool);
/* closes the resource's file */
int32_t ScePafResource_CEED7338(paf::resource::ResourceObj *rscobj);
/* the cxml allocator of a resource on the heap of its userdata (0x8106ead8, name ours) */
void ResourceObjCxmlAllocator(paf::cxml::AllocationType type, void *userdata, void *old_addr,
			      size_t required_size, void **addr, size_t *size);
/* a sound of the resource by ID hash; 0 when found */
int32_t ScePafResource_ADDC0711(const paf::resource::ResourceObj *rscobj, uint32_t idhash,
				const char **addr1, uint32_t *size1, const char **addr2,
				uint32_t *size2, int32_t *samplerate, uint32_t *type);
int32_t ScePafResource_B72D030D(paf::resource::ResourceObj *rscobj, uint32_t flag,
				paf::memory::HeapAllocator *heap);
paf::memory::HeapAllocator *ScePafResource_D60FF214(paf::resource::ResourceObj *rscobj,
						    paf::memory::HeapAllocator *heap);
int32_t ScePafResource_EA11BD0B(paf::resource::ResourceObj *rscobj, uint32_t flag, int32_t arg,
				bool mode, bool pool);
}

#endif
