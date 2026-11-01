#ifndef PAF_RESOURCE_RESOURCE_OBJ_H
#define PAF_RESOURCE_RESOURCE_OBJ_H

#include "../cxml/cxml.h"
#include "../memory/heap_allocator.h"
#include "../std/string.h"

typedef enum TableType {
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
} TableType;

typedef enum TextureType {
	TextureType_Unknown = -1,
	TextureType_Png = 0,
	TextureType_Jpg = 1,
	TextureType_Tif = 2,
	TextureType_Gif = 3,
	TextureType_Bmp = 4,
	TextureType_Gim = 5,
	TextureType_Dds = 6,
	TextureType_Gxt = 7
} TextureType;

typedef enum SoundType {
	SoundType_Unknown = -1,
	SoundType_WAV = 0,
	SoundType_VAG = 1,
	SoundType_MP3 = 2,
	SoundType_AT3 = 3,
	SoundType_AAC = 4
} SoundType;

typedef struct ResourceIDEntry {
	uint32_t first;
	PafString second;
} ResourceIDEntry;

typedef struct ResourceIDMapNode {
	struct ResourceIDMapNode *_Parent;
	struct ResourceIDMapNode *_Left;
	struct ResourceIDMapNode *_Right;
	ResourceIDEntry _Myval;
	unsigned char _Color;
	unsigned char _Isnil;
} ResourceIDMapNode;

typedef struct ResourceIDMap {
	ResourceIDMapNode *_Myhead;
	size_t _Mysize;
	unsigned char _allocator;
	unsigned char _compare;
} ResourceIDMap;

typedef struct ResourceBufferEntry {
	uint32_t idhash;
	void *addr;
	size_t size;
	uint32_t type;
} ResourceBufferEntry;

typedef struct ResourceSoundEntry {
	uint32_t idhash;
	void *addr1;
	void *addr2;
	size_t size1;
	size_t size2;
	int32_t samplerate;
	uint32_t type;
} ResourceSoundEntry;

typedef struct ResourceObj {
	FileSharedPtr rscfp;
	PafString filename;
	CxmlDocument *document;
	CxmlDocument *string_document;
	HeapAllocator *rscallocator;
	void *string_buffer;
	void *texture_buffer;
	void *model_buffer;
	void *sound_buffer;
	ResourceBufferEntry *textures;
	ResourceBufferEntry *models;
	ResourceSoundEntry *sounds;
	uint32_t num_textures;
	uint32_t num_models;
	uint32_t num_sounds;
	int32_t load_status;
	uint32_t create_type;
	ResourceIDMap id_map;
	PafString default_string;
	uint32_t resolution;
} ResourceObj;

typedef struct ResourceObjOption {
	uint32_t load_type;
	HeapAllocator *allocator;
	CxmlAllocator cxml_allocator_func;
} ResourceObjOption;

typedef char ResourceIDMapNodeSizeCheck[sizeof(ResourceIDMapNode) == 32 ? 1 : -1];
typedef char ResourceIDMapSizeCheck[sizeof(ResourceIDMap) == 12 ? 1 : -1];
typedef char ResourceObjSizeCheck[sizeof(ResourceObj) == 108 ? 1 : -1];
typedef char ResourceBufferEntrySizeCheck[sizeof(ResourceBufferEntry) == 16 ? 1 : -1];
typedef char ResourceSoundEntrySizeCheck[sizeof(ResourceSoundEntry) == 28 ? 1 : -1];
typedef char ResourceObjOptionSizeCheck[sizeof(ResourceObjOption) == 12 ? 1 : -1];

#define ResourceObjConstructFromBuffer _ZN3paf8resource11ResourceObjC1EPvjPKNS1_6OptionE
#define ResourceObjConstructFromFile _ZN3paf8resource11ResourceObjC1EPKcPKNS1_6OptionE
#define ResourceObjConstructFromFileBase _ZN3paf8resource11ResourceObjC2EPKcPKNS1_6OptionE
#define ResourceObjConstructFromBufferBase _ZN3paf8resource11ResourceObjC2EPvjPKNS1_6OptionE
#define ResourceObjDestroy _ZN3paf8resource11ResourceObjD1Ev
#define ResourceObjDestroyBase _ZN3paf8resource11ResourceObjD2Ev
#define ResourceObjGetFileName _ZNK3paf8resource11ResourceObj11GetFileNameEv
#define ResourceObjGetLoadStatus _ZNK3paf8resource11ResourceObj13GetLoadStatusEv
#define ResourceObjGetCreateType _ZNK3paf8resource11ResourceObj13GetCreateTypeEv
#define ResourceObjGetDocument _ZNK3paf8resource11ResourceObj11GetDocumentEv
#define ResourceObjGetStringDocument _ZNK3paf8resource11ResourceObj17GetStringDocumentEv
#define ResourceObjGetTableElement _ZNK3paf8resource11ResourceObj15GetTableElementENS0_9TableTypeE
#define ResourceObjIsRegisteredIDHash _ZN3paf8resource11ResourceObj14IsRegisteredIDEj
#define ResourceObjGetStringElement _ZNK3paf8resource11ResourceObj9GetStringERKNS_4cxml7ElementEPPKwPj
#define ResourceObjGetPageHash _ZNK3paf8resource11ResourceObj7GetPageEjRNS_4cxml7ElementE
#define ResourceObjGetStyleHash _ZNK3paf8resource11ResourceObj8GetStyleEjRNS_4cxml7ElementE
#define ResourceObjGetStringHash _ZNK3paf8resource11ResourceObj9GetStringEjPPKwPj
#define ResourceObjGetPage _ZNK3paf8resource11ResourceObj7GetPageEPKcRNS_4cxml7ElementE
#define ResourceObjGetStyle _ZNK3paf8resource11ResourceObj8GetStyleEPKcRNS_4cxml7ElementE
#define ResourceObjGetString _ZNK3paf8resource11ResourceObj9GetStringEPKcPPKwPj
#define ResourceObjIsRegisteredID _ZN3paf8resource11ResourceObj14IsRegisteredIDEPKcj
#define ResourceObjGetStringIDRef _ZNK3paf8resource11ResourceObj14GetStringIDRefERKNS_4cxml7ElementEPKcPS7_PS3_
#define ResourceObjGetStringIDHashRef _ZNK3paf8resource11ResourceObj14GetStringIDRefERKNS_4cxml7ElementEPKcPjPS3_
#define ResourceObjGetTextureFromBufferHash _ZNK3paf8resource11ResourceObj20GetTextureFromBufferEjPPvPjS4_
#define ResourceObjGetTextureFromBuffer _ZNK3paf8resource11ResourceObj20GetTextureFromBufferEPKcPPvPjS6_
#define ResourceObjGetTextureFromBufferElement _ZNK3paf8resource11ResourceObj20GetTextureFromBufferERKNS_4cxml7ElementEPPvPjS8_
#define ResourceObjGetModelFromBufferHash _ZNK3paf8resource11ResourceObj18GetModelFromBufferEjPPvPjS4_
#define ResourceObjGetModelFromBuffer _ZNK3paf8resource11ResourceObj18GetModelFromBufferEPKcPPvPjS6_
#define ResourceObjGetModelFromBufferElement _ZNK3paf8resource11ResourceObj18GetModelFromBufferERKNS_4cxml7ElementEPPvPjS8_
#define ResourceObjGetTextureFromFileElement _ZNK3paf8resource11ResourceObj18GetTextureFromFileERKNS_4cxml7ElementEPNS2_4FileEPjS8_
#define ResourceObjGetTextureFromFileHash _ZNK3paf8resource11ResourceObj18GetTextureFromFileEjPNS_4cxml4FileEPjS5_
#define ResourceObjGetTextureFromFile _ZNK3paf8resource11ResourceObj18GetTextureFromFileEPKcPNS_4cxml4FileEPjS7_
#define ResourceObjGetModelFromFileElement _ZNK3paf8resource11ResourceObj16GetModelFromFileERKNS_4cxml7ElementEPNS2_4FileEPjS8_
#define ResourceObjGetModelFromFileHash _ZNK3paf8resource11ResourceObj16GetModelFromFileEjPNS_4cxml4FileEPjS5_
#define ResourceObjGetModelFromFile _ZNK3paf8resource11ResourceObj16GetModelFromFileEPKcPNS_4cxml4FileEPjS7_
#define ResourceObjGetSoundFromFileElement ScePafResource_B4714AEA
#define ResourceObjGetSoundFromFileHash ScePafResource_08129BF8
#define ResourceObjGetSoundFromFile ScePafResource_BEAEA02A
#define ResourceObjGetSoundFromBufferHash ScePafResource_ADDC0711
#define ResourceObjGetSoundFromBuffer ScePafResource_DD1FF94F
#define ResourceObjGetSoundFromBufferElement ScePafResource_65C0FDF9
#define ResourceObjGetFileBufferElement _ZNK3paf8resource11ResourceObj7GetFileERKNS_4cxml7ElementEPvPjPPKcPNS_6memory13HeapAllocatorE
#define ResourceObjGetFileBufferHash _ZNK3paf8resource11ResourceObj7GetFileEjPvPjPPKcPNS_6memory13HeapAllocatorE
#define ResourceObjGetFileBuffer _ZNK3paf8resource11ResourceObj7GetFileEPKcPvPjPS3_PNS_6memory13HeapAllocatorE
#define ResourceObjGetFileElement _ZNK3paf8resource11ResourceObj7GetFileERKNS_4cxml7ElementEPjPPKcPNS_6memory13HeapAllocatorESC_
#define ResourceObjGetFileHash _ZNK3paf8resource11ResourceObj7GetFileEjPjPPKcPNS_6memory13HeapAllocatorES8_
#define ResourceObjGetFile _ZNK3paf8resource11ResourceObj7GetFileEPKcPjPS3_PNS_6memory13HeapAllocatorES8_

ResourceObj *ResourceObjConstructFromBuffer(ResourceObj *self, const void *addr,
					  size_t size, const ResourceObjOption *option);
ResourceObj *ResourceObjConstructFromBufferBase(ResourceObj *self, const void *addr,
					      size_t size, const ResourceObjOption *option);
ResourceObj *ResourceObjConstructFromFile(ResourceObj *self, const char *name,
					const ResourceObjOption *option);
ResourceObj *ResourceObjConstructFromFileBase(ResourceObj *self, const char *name,
					    const ResourceObjOption *option);
int32_t ResourceObjLoadResourceDescription(ResourceObj *self, const char *name);
ResourceObj *ResourceObjDestroy(ResourceObj *self);
ResourceObj *ResourceObjDestroyBase(ResourceObj *self);
ResourceIDMap *ResourceIDMapClear(ResourceIDMap *self);
ResourceIDMapNode *ResourceIDMapRebalance(ResourceIDMap *self, ResourceIDMapNode *node);
ResourceIDMapNode *ResourceIDMapInsert(ResourceIDMap *self, const ResourceIDEntry *value,
				   unsigned char *inserted);
char *ResourceObjGetFileName(const ResourceObj *self);
int32_t ResourceObjGetLoadStatus(const ResourceObj *self);
uint32_t ResourceObjGetCreateType(const ResourceObj *self);
CxmlDocument *ResourceObjGetDocument(const ResourceObj *self);
CxmlDocument *ResourceObjGetStringDocument(const ResourceObj *self);
void ResourceObjGetTableElement(CxmlElement *result, const ResourceObj *self,
				TableType type);
unsigned char ResourceObjIsRegisteredIDHash(ResourceObj *self, uint32_t value);
int32_t ResourceObjGetStringElement(const ResourceObj *self, const CxmlElement *elm,
				    const uint16_t **src, uint32_t *length);
int32_t ResourceObjGetPageHash(const ResourceObj *self, uint32_t idhash, CxmlElement *elm);
int32_t ResourceObjGetStyleHash(const ResourceObj *self, uint32_t idhash, CxmlElement *elm);
int32_t ResourceObjGetStringHash(const ResourceObj *self, uint32_t idhash,
				 const uint16_t **src, uint32_t *length);
int32_t ResourceObjGetPage(const ResourceObj *self, const char *id, CxmlElement *elm);
int32_t ResourceObjGetStyle(const ResourceObj *self, const char *id, CxmlElement *elm);
int32_t ResourceObjGetString(const ResourceObj *self, const char *id,
			     const uint16_t **src, uint32_t *length);
unsigned char ResourceObjIsRegisteredID(ResourceObj *self, const char *str, uint32_t mode);
int32_t ResourceObjGetStringIDRef(const ResourceObj *self, const CxmlElement *elm,
				const char *attrname, const char **str, CxmlElement *entity);
int32_t ResourceObjGetStringIDHashRef(const ResourceObj *self, const CxmlElement *elm,
				    const char *attrname, uint32_t *value, CxmlElement *entity);
int32_t ResourceObjGetTextureFromBufferHash(const ResourceObj *self, uint32_t idhash,
					  void **addr, size_t *size, uint32_t *type);
int32_t ResourceObjGetTextureFromBuffer(const ResourceObj *self, const char *id,
				      void **addr, size_t *size, uint32_t *type);
int32_t ResourceObjGetTextureFromBufferElement(const ResourceObj *self, const CxmlElement *elm,
					     void **addr, size_t *size, uint32_t *type);
int32_t ResourceObjGetModelFromBufferHash(const ResourceObj *self, uint32_t idhash,
					void **addr, size_t *size, uint32_t *type);
int32_t ResourceObjGetModelFromBuffer(const ResourceObj *self, const char *id,
				    void **addr, size_t *size, uint32_t *type);
int32_t ResourceObjGetModelFromBufferElement(const ResourceObj *self, const CxmlElement *elm,
					   void **addr, size_t *size, uint32_t *type);
int32_t ResourceObjGetTextureFromFileElement(const ResourceObj *self, const CxmlElement *elm,
					   CxmlFile *src, size_t *size, uint32_t *type);
uint32_t ResourceObjGetTextureType(const ResourceObj *self, const char *type);
int32_t ResourceObjGetTextureFromFileHash(const ResourceObj *self, uint32_t idhash,
					CxmlFile *src, size_t *size, uint32_t *type);
int32_t ResourceObjGetTextureFromFile(const ResourceObj *self, const char *id,
				    CxmlFile *src, size_t *size, uint32_t *type);
int32_t ResourceObjGetModelFromFileElement(const ResourceObj *self, const CxmlElement *elm,
					 CxmlFile *src, size_t *size, uint32_t *type);
int32_t ResourceObjGetModelFromFileHash(const ResourceObj *self, uint32_t idhash,
				      CxmlFile *src, size_t *size, uint32_t *type);
int32_t ResourceObjGetModelFromFile(const ResourceObj *self, const char *id,
				  CxmlFile *src, size_t *size, uint32_t *type);
uint32_t ResourceObjGetSoundType(const ResourceObj *self, const char *type);
int32_t ResourceObjGetSoundFromFileElement(const ResourceObj *self, const CxmlElement *elm,
		CxmlFile *left_or_mono, size_t *left_or_mono_size, CxmlFile *right,
		size_t *right_size, int32_t *samplerate, uint32_t *format);
int32_t ResourceObjGetSoundFromFileHash(const ResourceObj *self, uint32_t hash,
		CxmlFile *left_or_mono, size_t *left_or_mono_size, CxmlFile *right,
		size_t *right_size, int32_t *samplerate, uint32_t *format);
int32_t ResourceObjGetSoundFromFile(const ResourceObj *self, const char *id,
		CxmlFile *left_or_mono, size_t *left_or_mono_size, CxmlFile *right,
		size_t *right_size, int32_t *samplerate, uint32_t *format);
int32_t ResourceObjGetSoundFromBufferHash(const ResourceObj *self, uint32_t idhash,
		void **addr1, size_t *size1, void **addr2, size_t *size2,
		int32_t *samplerate, uint32_t *type);
int32_t ResourceObjGetSoundFromBuffer(const ResourceObj *self, const char *id,
		void **addr1, size_t *size1, void **addr2, size_t *size2,
		int32_t *samplerate, uint32_t *type);
void ResourceObjGetFileElement(FileSharedPtr *result, const ResourceObj *self,
		const CxmlElement *elm, size_t *size, const char **type,
		HeapAllocator *file_allocator, HeapAllocator *decomp_allocator);
void ResourceObjGetFileHash(FileSharedPtr *result, const ResourceObj *self,
		uint32_t idhash, size_t *size, const char **type,
		HeapAllocator *file_allocator, HeapAllocator *decomp_allocator);
void ResourceObjGetFile(FileSharedPtr *result, const ResourceObj *self,
		const char *id, size_t *size, const char **type,
		HeapAllocator *file_allocator, HeapAllocator *decomp_allocator);
void ResourceObjGetFileBufferElement(FileSharedPtr *result, const ResourceObj *self,
		const CxmlElement *elm, void *addr, size_t *size, const char **type,
		HeapAllocator *decomp_allocator);
void ResourceObjGetFileBufferHash(FileSharedPtr *result, const ResourceObj *self,
		uint32_t idhash, void *addr, size_t *size, const char **type,
		HeapAllocator *decomp_allocator);
void ResourceObjGetFileBuffer(FileSharedPtr *result, const ResourceObj *self,
		const char *id, void *addr, size_t *size, const char **type,
		HeapAllocator *decomp_allocator);
int32_t ResourceObjGetSoundFromBufferElement(const ResourceObj *self, const CxmlElement *elm,
		void **addr1, size_t *size1, void **addr2, size_t *size2,
		int32_t *samplerate, uint32_t *type);

#endif
