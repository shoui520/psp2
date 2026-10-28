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

CxmlHeader *CxmlHeaderConstruct(CxmlHeader *self);
CxmlHeader *CxmlHeaderConstructBase(CxmlHeader *self);
CxmlDocument *CxmlDocumentConstruct(CxmlDocument *self);
CxmlDocument *CxmlDocumentConstructBase(CxmlDocument *self);
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
