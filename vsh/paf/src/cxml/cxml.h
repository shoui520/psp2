#ifndef PAF_CXML_CXML_H
#define PAF_CXML_CXML_H

#include <stdint.h>
#include <stddef.h>

typedef enum AccessMode {
	AccessMode_ReadWrite = 0,
	AccessMode_ReadOnly_File = 1,
	AccessMode_ReadOnly_Memory = 2
} AccessMode;

typedef enum AllocationType {
	AllocationType_Alloc_Tree = 0,
	AllocationType_Alloc_IDTable,
	AllocationType_Alloc_IDHashTable,
	AllocationType_Alloc_StringTable,
	AllocationType_Alloc_WStringTable,
	AllocationType_Alloc_HashTable,
	AllocationType_Alloc_IntArrayTable,
	AllocationType_Alloc_FloatArrayTable,
	AllocationType_Alloc_FileTable,
	AllocationType_Free_Tree,
	AllocationType_Free_IDTable,
	AllocationType_Free_IDHashTable,
	AllocationType_Free_StringTable,
	AllocationType_Free_WStringTable,
	AllocationType_Free_HashTable,
	AllocationType_Free_IntArrayTable,
	AllocationType_Free_FloatArrayTable,
	AllocationType_Free_FileTable
} AllocationType;

typedef struct CxmlHeader {
	char magic[4];
	int32_t version;
	int32_t tree_offset;
	int32_t tree_size;
	int32_t idtable_offset;
	int32_t idtable_size;
	int32_t idhashtable_offset;
	int32_t idhashtable_size;
	int32_t stringtable_offset;
	int32_t stringtable_size;
	int32_t wstringtable_offset;
	int32_t wstringtable_size;
	int32_t hashtable_offset;
	int32_t hashtable_size;
	int32_t intarraytable_offset;
	int32_t intarraytable_size;
	int32_t floatarraytable_offset;
	int32_t floatarraytable_size;
	int32_t filetable_offset;
	int32_t filetable_size;
} CxmlHeader;

typedef void (*CxmlAllocator)(AllocationType type, void *userdata,
			      void *old_addr, size_t required_size,
			      void **addr, size_t *size);

typedef struct CxmlDocument CxmlDocument;

struct CxmlDocument {
	AccessMode access_mode;
	CxmlHeader header;
	char *tree;
	uint32_t tree_capacity;
	char *idtable;
	uint32_t idtable_capacity;
	char *idhashtable;
	uint32_t idhashtable_capacity;
	char *stringtable;
	uint32_t stringtable_capacity;
	char *wstringtable;
	uint32_t wstringtable_capacity;
	char *hashtable;
	uint32_t hashtable_capacity;
	int32_t *intarraytable;
	uint32_t intarraytable_capacity;
	float *floatarraytable;
	uint32_t floatarraytable_capacity;
	char *filetable;
	uint32_t filetable_capacity;
	CxmlAllocator allocator;
	void *allocator_userdata;
};

typedef struct ElementBin {
	int32_t name;
	int32_t attr_num;
	int32_t parent;
	int32_t prev;
	int32_t next;
	int32_t first_child;
	int32_t last_child;
} ElementBin;

typedef struct CxmlElement {
	CxmlDocument *doc;
	int32_t offset;
} CxmlElement;

typedef enum AttributeType {
	AttributeType_None = 0,
	AttributeType_Int = 1,
	AttributeType_Float = 2,
	AttributeType_String = 3,
	AttributeType_WString = 4,
	AttributeType_Hash = 5,
	AttributeType_IntArray = 6,
	AttributeType_FloatArray = 7,
	AttributeType_File = 8,
	AttributeType_ID = 9,
	AttributeType_IDRef = 10,
	AttributeType_IDHash = 11,
	AttributeType_IDHashRef = 12
} AttributeType;

typedef struct AttributeBin {
	int32_t name;
	int32_t type;
	union {
		int32_t i;
		float f;
		struct { int32_t offset, len; } s;
		struct { int32_t offset, len; } ws;
		struct { int32_t offset, num; } ia;
		struct { int32_t offset, num; } fa;
		struct { int32_t offset, size; } file;
		int32_t id;
		int32_t idref;
		int32_t hash;
		int32_t idhash;
	};
} AttributeBin;

typedef struct CxmlAttribute {
	CxmlDocument *doc;
	int32_t element_offset;
	int32_t offset;
} CxmlAttribute;

typedef struct CxmlFile {
	CxmlDocument *doc;
	int32_t offset;
	int32_t size;
} CxmlFile;

typedef struct IDBin {
	int32_t entity_offset;
	char id[0];
} IDBin;

typedef struct IDHashBin {
	int32_t entity_offset;
	uint32_t idhash;
} IDHashBin;

typedef char AttributeBinSizeCheck[sizeof(AttributeBin) == 16 ? 1 : -1];
typedef char CxmlAttributeSizeCheck[sizeof(CxmlAttribute) == 12 ? 1 : -1];
typedef char CxmlFileSizeCheck[sizeof(CxmlFile) == 12 ? 1 : -1];
typedef char IDHashBinSizeCheck[sizeof(IDHashBin) == 8 ? 1 : -1];

#define CxmlElementGetAttributeByIndex ScePafResource_46123EF6
#define CxmlElementGetAttributeByName scePafResourceGetAttribute
#define CxmlAttributeGetName ScePafResource_1412AC17
#define CxmlAttributeGetType ScePafResource_E32AD3C0
#define CxmlAttributeGetInt scePafResourceGetInteger
#define CxmlAttributeGetFloat scePafResourceGetFloat
#define CxmlAttributeGetString scePafResourceGetString
#define CxmlAttributeGetWString scePafResourceGetChar
#define CxmlAttributeGetHash scePafResourceGetStyleId
#define CxmlAttributeGetIntArray scePafResourceGetIntegerArray
#define CxmlAttributeGetFloatArray scePafResourceGetFloatArray
#define CxmlAttributeGetFile scePafResourceGetData
#define CxmlAttributeGetID scePafResourceGetIdStrLpb
#define CxmlAttributeGetIDRef scePafResourceGetIdStr
#define CxmlAttributeGetIDHash scePafResourceGetIdIntLpb
#define CxmlAttributeGetIDHashRef scePafResourceGetIdInt
#define CxmlFileGetAddr ScePafResource_1CBB0615

int32_t CxmlElementGetAttributeByIndex(const CxmlElement *self, int32_t index,
				      CxmlAttribute *attr);
int32_t CxmlElementGetAttributeByName(const CxmlElement *self, const char *name,
				     CxmlAttribute *attr);
const char *CxmlAttributeGetName(const CxmlAttribute *self);
AttributeType CxmlAttributeGetType(const CxmlAttribute *self);
int32_t CxmlAttributeGetInt(const CxmlAttribute *self, int32_t *value);
int32_t CxmlAttributeGetFloat(const CxmlAttribute *self, float *value);
int32_t CxmlAttributeGetString(const CxmlAttribute *self,
			      const char **value, uint32_t *len);
int32_t CxmlAttributeGetWString(const CxmlAttribute *self,
			       const uint16_t **value, uint32_t *len);
int32_t CxmlAttributeGetHash(const CxmlAttribute *self, uint32_t *value);
int32_t CxmlAttributeGetIntArray(const CxmlAttribute *self,
				const int32_t **addr, uint32_t *num);
int32_t CxmlAttributeGetFloatArray(const CxmlAttribute *self,
				  const float **addr, uint32_t *num);
int32_t CxmlAttributeGetFile(const CxmlAttribute *self, CxmlFile *file);
int32_t CxmlAttributeGetID(const CxmlAttribute *self, const char **value);
int32_t CxmlAttributeGetIDRef(const CxmlAttribute *self,
			     const char **value, CxmlElement *entity);
int32_t CxmlAttributeGetIDHash(const CxmlAttribute *self, uint32_t *value);
int32_t CxmlAttributeGetIDHashRef(const CxmlAttribute *self,
				 uint32_t *value, CxmlElement *entity);
const void *CxmlFileGetAddr(const CxmlFile *self);

typedef char CxmlElementSizeCheck[sizeof(CxmlElement) == 8 ? 1 : -1];
typedef char CxmlHeaderSizeCheck[sizeof(CxmlHeader) == 80 ? 1 : -1];
typedef char CxmlDocumentSizeCheck[sizeof(CxmlDocument) == 164 ? 1 : -1];
typedef char ElementBinSizeCheck[sizeof(ElementBin) == 28 ? 1 : -1];

#define CxmlElementGetFirstChild ScePafResource_B673D5D7
#define CxmlHeaderConstruct ScePafResource_6877BAE5
#define CxmlHeaderConstructBase ScePafResource_776D12E0
#define CxmlDocumentConstruct ScePafResource_43B08061
#define CxmlDocumentConstructBase ScePafResource_34A10103
#define CxmlDefaultAllocator ScePafResource_23B99133
#define CxmlElementGetParent ScePafResource_5FF23A7F
#define CxmlElementGetPreviousSibling ScePafResource_AE24B68F
#define CxmlElementGetNextSibling ScePafResource_B7C0081F
#define CxmlElementGetLastChild ScePafResource_EB3CB910
#define CxmlElementGetName ScePafResource_8A71B732
#define CxmlElementGetNumChild ScePafResource_262EE6C0
#define CxmlElementNumAttribute ScePafResource_59755449
#define CxmlDocumentClear ScePafResource_1461674C
#define CxmlDocumentGetDocumentElement ScePafResource_93A5860F
#define CxmlDocumentGetHeaderMagic ScePafResource_D2022BA2
#define CxmlDocumentDestroy ScePafResource_2387124F
#define CxmlDocumentDestroyBase ScePafResource_71407B32
#define CxmlDocumentCreateFromBuffer ScePafResource_FFC4FA5F

CxmlHeader *CxmlHeaderConstruct(CxmlHeader *self);
CxmlHeader *CxmlHeaderConstructBase(CxmlHeader *self);
CxmlDocument *CxmlDocumentConstruct(CxmlDocument *self);
CxmlDocument *CxmlDocumentConstructBase(CxmlDocument *self);
int32_t CxmlDocumentClear(CxmlDocument *self);
int32_t CxmlDocumentCreateFromBuffer(CxmlDocument *self, const void *buf,
				    size_t bufsize, unsigned char copy);
void CxmlDocumentGetDocumentElement(CxmlElement *result, CxmlDocument *self);
void CxmlDocumentGetHeaderMagic(const CxmlDocument *self, char buf[4]);
CxmlDocument *CxmlDocumentDestroy(CxmlDocument *self);
CxmlDocument *CxmlDocumentDestroyBase(CxmlDocument *self);
void CxmlDefaultAllocator(AllocationType type, void *userdata, void *old_addr,
			 size_t required_size, void **addr, size_t *size);
void CxmlElementGetParent(CxmlElement *result, const CxmlElement *self);
void CxmlElementGetPreviousSibling(CxmlElement *result, const CxmlElement *self);
void CxmlElementGetNextSibling(CxmlElement *result, const CxmlElement *self);
void CxmlElementGetLastChild(CxmlElement *result, const CxmlElement *self);
const char *CxmlElementGetName(const CxmlElement *self);
int32_t CxmlElementGetNumChild(const CxmlElement *self);
int32_t CxmlElementNumAttribute(const CxmlElement *self);

void CxmlElementGetFirstChild(CxmlElement *result, const CxmlElement *self);

#endif
