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
	void *textures;
	void *models;
	void *sounds;
	uint32_t num_textures;
	uint32_t num_models;
	uint32_t num_sounds;
	int32_t load_status;
	uint32_t create_type;
	ResourceIDMap id_map;
	PafString default_string;
	uint32_t resolution;
} ResourceObj;

typedef char ResourceIDMapNodeSizeCheck[sizeof(ResourceIDMapNode) == 32 ? 1 : -1];
typedef char ResourceIDMapSizeCheck[sizeof(ResourceIDMap) == 12 ? 1 : -1];
typedef char ResourceObjSizeCheck[sizeof(ResourceObj) == 108 ? 1 : -1];

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

#endif
