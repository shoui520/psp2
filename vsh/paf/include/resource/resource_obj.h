/*
 * paf::resource::ResourceObj: a plugin's resource (RCO), its tables by type.
 * Names after ScePaf-RE.
 */
#ifndef PAF_RESOURCE_RESOURCE_OBJ_H
#define PAF_RESOURCE_RESOURCE_OBJ_H

#include <stdint.h>
#include <cxml/cxml.h>
#include <std/map>
#include <std/string>
#include <common/shared_ptr.h>
#include <widget/core/environment.h>

namespace paf {

class File;
class MemFile;

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

	ResourceObj(const char *name, const Option *option);
	ResourceObj(void *addr, uint32_t size, const Option *option);
	~ResourceObj();

	const char *GetFileName() const;
	int32_t GetLoadStatus() const;
	uint32_t GetCreateType() const;
	cxml::Document *GetDocument() const;
	cxml::Document *GetStringDocument() const;
	cxml::Element GetTableElement(TableType type) const;
	/* the page element of an ID; 0 when found */
	int32_t GetPage(uint32_t idhash, cxml::Element &elm) const;
	int32_t GetPage(const char *id, cxml::Element &elm) const;
	int32_t GetStyle(uint32_t idhash, cxml::Element &elm) const;
	int32_t GetStyle(const char *id, cxml::Element &elm) const;
	/* the string of an ID; 0 when found */
	int32_t GetString(const cxml::Element &elm, const wchar_t **str, uint32_t *length) const;
	int32_t GetString(uint32_t idhash, const wchar_t **str, uint32_t *length) const;
	int32_t GetString(const char *id, const wchar_t **str, uint32_t *length) const;
	int32_t GetTextureFromFile(const cxml::Element &elm, cxml::File *src, uint32_t *size,
				   uint32_t *type) const;
	int32_t GetTextureFromFile(uint32_t idhash, cxml::File *src, uint32_t *size,
				   uint32_t *type) const;
	int32_t GetTextureFromFile(const char *id, cxml::File *src, uint32_t *size,
				   uint32_t *type) const;
	int32_t GetTextureFromBuffer(uint32_t idhash, void **addr, uint32_t *size,
				     uint32_t *type) const;
	int32_t GetTextureFromBuffer(const char *id, void **addr, uint32_t *size,
				     uint32_t *type) const;
	int32_t GetTextureFromBuffer(const cxml::Element &elm, void **addr, uint32_t *size,
				     uint32_t *type) const;
	int32_t GetModelFromFile(const cxml::Element &elm, cxml::File *src, uint32_t *size,
				 uint32_t *type) const;
	int32_t GetModelFromFile(uint32_t idhash, cxml::File *src, uint32_t *size,
				 uint32_t *type) const;
	int32_t GetModelFromFile(const char *id, cxml::File *src, uint32_t *size,
				 uint32_t *type) const;
	int32_t GetModelFromBuffer(uint32_t idhash, void **addr, uint32_t *size,
				   uint32_t *type) const;
	int32_t GetModelFromBuffer(const char *id, void **addr, uint32_t *size,
				   uint32_t *type) const;
	int32_t GetModelFromBuffer(const cxml::Element &elm, void **addr, uint32_t *size,
				   uint32_t *type) const;
	/* a file of the resource as a memory file, read into addr or a buffer of its own */
	common::SharedPtr<MemFile> GetFile(const cxml::Element &elm, void *addr, uint32_t *size,
					   const char **type,
					   memory::HeapAllocator *decomp_allocator = NULL) const;
	common::SharedPtr<MemFile> GetFile(uint32_t idhash, void *addr, uint32_t *size,
					   const char **type,
					   memory::HeapAllocator *decomp_allocator = NULL) const;
	common::SharedPtr<MemFile> GetFile(const char *id, void *addr, uint32_t *size,
					   const char **type,
					   memory::HeapAllocator *decomp_allocator = NULL) const;
	common::SharedPtr<MemFile> GetFile(const cxml::Element &elm, uint32_t *size,
					   const char **type,
					   memory::HeapAllocator *file_allocator = NULL,
					   memory::HeapAllocator *decomp_allocator = NULL) const;
	common::SharedPtr<MemFile> GetFile(uint32_t idhash, uint32_t *size, const char **type,
					   memory::HeapAllocator *file_allocator = NULL,
					   memory::HeapAllocator *decomp_allocator = NULL) const;
	common::SharedPtr<MemFile> GetFile(const char *id, uint32_t *size, const char **type,
					   memory::HeapAllocator *file_allocator = NULL,
					   memory::HeapAllocator *decomp_allocator = NULL) const;
	/* the ID of a string attribute, and its string element */
	int32_t GetStringIDRef(const cxml::Element &elm, const char *attrname, const char **str,
			       cxml::Element *entity) const;
	/* the ID hash of a string attribute, and its string element */
	int32_t GetStringIDRef(const cxml::Element &elm, const char *attrname, uint32_t *value,
			       cxml::Element *entity) const;
	/* whether the resource's document holds the ID (mode 0) or its hash (mode 1) */
	bool IsRegisteredID(const char *id, uint32_t mode);
	bool IsRegisteredID(uint32_t idhash);

	/* the ID the alias table gives the ID, for this model's entry first; empty without one (name ours) */
	IDParam GetAliasID(const IDParam &id) const;
	/* the "id:", "src:" or "type:" (kind 0, 1, 2) of an ID's debug symbol; -1 without one */
	int32_t GetDebugSymbol(uint32_t idhash, int32_t kind, string *symbol) const;
	/*
	 * The child of a texture element for this resolution or model, else the element
	 * (name ours).
	 */
	cxml::Element GetOverrideElement(const cxml::Element &elm) const;
	/* the string table's element of a language (name ours) */
	cxml::Element GetLocaleElement(int32_t locale) const;
	/*
	 * The size of the buffer a texture, model or sound table's files take, each aligned to
	 * align, and the number of entries (name ours)
	 */
	size_t GetTableBufferSize(TableType type, uint32_t align, int32_t *num) const;
	/* the texture and sound tables of a resource read from memory, pointing into it */
	int32_t SetTextureTable(uint32_t align);
	int32_t SetSoundTable(uint32_t align);

	/* the type of a texture, model or sound by its MIME name; -1 for an unknown one (names ours) */
	uint32_t GetTextureType(const char *type) const;
	uint32_t GetModelType(const char *type) const;
	uint32_t GetSoundType(const char *type) const;
	/* the IDs' debug symbols from the resource's .rcd file (name ours) */
	int32_t LoadResourceDescription(const char *name);

	void SetResolutionResource(IDParam resource)
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
	/* each ID's debug symbol: its "id:", "src:" and "type:" fields, comma separated */
	map<uint32_t, string> m_id_map;
	IDParam m_resolution_resource;
};

}
}

/* ResourceObj's exports without a Sony name (named by their NIDs) */
extern "C" {
/* reads the string table for the locale */
int32_t ScePafResource_42E4E566(paf::resource::ResourceObj *rscobj, int32_t locale,
				paf::memory::HeapAllocator *heap);
/* lets the sound table's buffers go */
void ScePafResource_556665FC(paf::resource::ResourceObj *rscobj,
			     paf::memory::HeapAllocator *heap, bool create, bool shared);
/* closes the resource's file */
int32_t ScePafResource_CEED7338(paf::resource::ResourceObj *rscobj);
/* the cxml allocator of a resource on the heap of its userdata (0x8106ead8, name ours) */
void ResourceObjCxmlAllocator(paf::cxml::AllocationType type, void *userdata, void *old_addr,
			      size_t required_size, void **addr, size_t *size);
/*
 * A sound of the resource, from its file (mono, or left and right) or the sound table's
 * buffers; 0 when found. ResourceObj's methods, by their NIDs.
 */
int32_t ScePafResource_B4714AEA(const paf::resource::ResourceObj *rscobj,
				const paf::cxml::Element &elm, paf::cxml::File *left_or_mono,
				uint32_t *left_or_mono_size, paf::cxml::File *right,
				uint32_t *right_size, int32_t *samplerate, uint32_t *format);
int32_t ScePafResource_08129BF8(const paf::resource::ResourceObj *rscobj, uint32_t idhash,
				paf::cxml::File *left_or_mono, uint32_t *left_or_mono_size,
				paf::cxml::File *right, uint32_t *right_size, int32_t *samplerate,
				uint32_t *format);
int32_t ScePafResource_BEAEA02A(const paf::resource::ResourceObj *rscobj, const char *id,
				paf::cxml::File *left_or_mono, uint32_t *left_or_mono_size,
				paf::cxml::File *right, uint32_t *right_size, int32_t *samplerate,
				uint32_t *format);
int32_t ScePafResource_ADDC0711(const paf::resource::ResourceObj *rscobj, uint32_t idhash,
				void **addr1, uint32_t *size1, void **addr2, uint32_t *size2,
				int32_t *samplerate, uint32_t *type);
int32_t ScePafResource_DD1FF94F(const paf::resource::ResourceObj *rscobj, const char *id,
				void **addr1, uint32_t *size1, void **addr2, uint32_t *size2,
				int32_t *samplerate, uint32_t *type);
int32_t ScePafResource_65C0FDF9(const paf::resource::ResourceObj *rscobj,
				const paf::cxml::Element &elm, void **addr1, uint32_t *size1,
				void **addr2, uint32_t *size2, int32_t *samplerate, uint32_t *type);
/* reads the texture and model tables' files into a buffer on heap, and lets them go */
int32_t ScePafResource_B72D030D(paf::resource::ResourceObj *rscobj, uint32_t align,
				paf::memory::HeapAllocator *heap);
void ScePafResource_D60FF214(paf::resource::ResourceObj *rscobj, paf::memory::HeapAllocator *heap);
int32_t ScePafResource_933C078B(paf::resource::ResourceObj *rscobj, uint32_t align,
				paf::memory::HeapAllocator *heap);
void ScePafResource_C240421A(paf::resource::ResourceObj *rscobj, paf::memory::HeapAllocator *heap);
/*
 * reads the sound table's files, into the common resource's shared storage when shared (made
 * when create, else opened), else on heap
 */
int32_t ScePafResource_EA11BD0B(paf::resource::ResourceObj *rscobj, uint32_t align,
				paf::memory::HeapAllocator *heap, bool create, bool shared);
}

#endif
