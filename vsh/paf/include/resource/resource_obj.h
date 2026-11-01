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
	TableType_File = 6,
	TableType_Anim = 7,
	TableType_Shader = 8,
	TableType_Template = 9
};

class ResourceObj {
public:
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

	cxml::Element GetTableElement(TableType type) const;

	/* the layout of the C struct, up to what C++ reads; the id map and default string follow */
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
};

}
}

#endif
