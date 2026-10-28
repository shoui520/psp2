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

#define ResourceObjConstructFromBuffer ScePafResource_9E3EE281
#define ResourceObjConstructFromBufferBase ScePafResource_E629FBF7
#define ResourceObjDestroy ScePafResource_F194FC6D
#define ResourceObjDestroyBase ScePafResource_E617D3AE
#define ResourceObjGetFileName ScePafResource_FA00F835
#define ResourceObjGetLoadStatus ScePafResource_04FC462E
#define ResourceObjGetCreateType ScePafResource_E3349545
#define ResourceObjGetDocument ScePafResource_80B84E6A
#define ResourceObjGetStringDocument ScePafResource_1172424E
#define ResourceObjGetTableElement ScePafResource_A7368FD6
#define ResourceObjIsRegisteredIDHash ScePafResource_0F67A8FC
#define ResourceObjGetStringElement ScePafResource_47A0AC77
#define ResourceObjGetPageHash ScePafResource_7E56FE51
#define ResourceObjGetStyleHash ScePafResource_C860A953
#define ResourceObjGetStringHash ScePafResource_55EB9585
#define ResourceObjGetPage ScePafResource_9EDFB093
#define ResourceObjGetStyle ScePafResource_8A444C50
#define ResourceObjGetString ScePafResource_50CD574F
#define ResourceObjIsRegisteredID ScePafResource_9228A0D1
#define ResourceObjGetStringIDRef ScePafResource_7E078F77
#define ResourceObjGetStringIDHashRef ScePafResource_77233826
#define ResourceObjGetTextureFromBufferHash ScePafResource_A667DEF2
#define ResourceObjGetTextureFromBuffer ScePafResource_BDA4F782
#define ResourceObjGetTextureFromBufferElement ScePafResource_862D4A5F
#define ResourceObjGetModelFromBufferHash ScePafResource_25D47C62
#define ResourceObjGetModelFromBuffer ScePafResource_B80E2E4D
#define ResourceObjGetModelFromBufferElement ScePafResource_5A179BBE
#define ResourceObjGetTextureFromFileElement ScePafResource_5E48B664
#define ResourceObjGetTextureFromFileHash ScePafResource_3FA9B5D3
#define ResourceObjGetTextureFromFile ScePafResource_5E1D961A
#define ResourceObjGetModelFromFileElement ScePafResource_C88E6968
#define ResourceObjGetModelFromFileHash ScePafResource_C3185EF6
#define ResourceObjGetModelFromFile ScePafResource_ACD33BFC
#define ResourceObjGetSoundFromFileElement ScePafResource_B4714AEA
#define ResourceObjGetSoundFromFileHash ScePafResource_08129BF8
#define ResourceObjGetSoundFromFile ScePafResource_BEAEA02A
#define ResourceObjGetSoundFromBufferHash ScePafResource_ADDC0711
#define ResourceObjGetSoundFromBuffer ScePafResource_DD1FF94F
#define ResourceObjGetSoundFromBufferElement ScePafResource_65C0FDF9

ResourceObj *ResourceObjConstructFromBuffer(ResourceObj *self, const void *addr,
					  size_t size, const ResourceObjOption *option);
ResourceObj *ResourceObjConstructFromBufferBase(ResourceObj *self, const void *addr,
					      size_t size, const ResourceObjOption *option);
ResourceObj *ResourceObjDestroy(ResourceObj *self);
ResourceObj *ResourceObjDestroyBase(ResourceObj *self);
ResourceIDMap *ResourceIDMapClear(ResourceIDMap *self);
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
int32_t ResourceObjGetSoundFromBufferElement(const ResourceObj *self, const CxmlElement *elm,
		void **addr1, size_t *size1, void **addr2, size_t *size2,
		int32_t *samplerate, uint32_t *type);

#endif
