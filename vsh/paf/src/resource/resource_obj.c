#include "resource_obj.h"
#include "../cxml/util.h"
#include "../file/localfile.h"
#include "../file/bufferedfile.h"
#include "../file/memfile.h"
#include <compress/compress.h>
#include "../paf_types.h"
#include "../std/stdlib.h"
#include "../std/new.h"
#include "../versatileparam/idparam.h"
#include <sce_atomic.h>
#include <arm_neon.h>
#include <kernel/iofilemgr.h>

extern int sce_paf_strcmp(const char *left, const char *right);
extern int sce_paf_strncmp(const char *left, const char *right, size_t len);
extern char *sce_paf_strncpy(char *dst, const char *src, size_t len);
extern unsigned long sce_paf_strtoul(const char *str, char **end, int base);
extern size_t sce_paf_strlen(const char *str);
extern void ScePafStdc_5C468294(void *ptr);

static const char s_default_string[] = "";
static const char s_buffer_filename[] = "";

static inline __attribute__((always_inline))
HeapAllocator *ReleaseResourceBuffer(uint8_t *self, HeapAllocator *allocator,
				     size_t bufferOffset, size_t arrayOffset,
				     size_t countOffset)
{
	HeapAllocator *result = allocator;

	if (*(uint32_t *)(self + 76) != 1) {
		void *buffer = *(void **)(self + bufferOffset);

		if (buffer) {
			if (result ||
			    (result = *(HeapAllocator **)(self + 28)) != NULL)
				HeapAllocatorFree(result, buffer);
			else
				sce_paf_free(*(void **)(self + bufferOffset));
			*(void **)(self + bufferOffset) = NULL;
		}
		result = *(HeapAllocator **)(self + arrayOffset);
		if (result) {
			PafDeleteArray(result);
			*(void **)(self + arrayOffset) = NULL;
			*(uint32_t *)(self + countOffset) = 0;
			return NULL;
		}
	}
	return result;
}

HeapAllocator *ScePafResource_D60FF214(uint8_t *self,
				       HeapAllocator *allocator)
{
	return ReleaseResourceBuffer(self, allocator, 36, 48, 60);
}

HeapAllocator *ScePafResource_C240421A(uint8_t *self,
				       HeapAllocator *allocator)
{
	return ReleaseResourceBuffer(self, allocator, 40, 52, 64);
}

ResourceObj *ResourceObjConstructFromBuffer(ResourceObj *self, const void *addr,
					  size_t size, const ResourceObjOption *option)
{
	ResourceIDMapNode *head;
	CxmlDocument *document;
	int32_t ret;

	self->rscfp.m_ptr = NULL;
	self->rscfp.m_ref_counter = NULL;
	PafStringInitialize(&self->filename);
	self->document = NULL;
	self->string_document = NULL;
	self->rscallocator = option->allocator;
	self->string_buffer = NULL;
	self->texture_buffer = NULL;
	self->model_buffer = NULL;
	self->sound_buffer = NULL;
	self->textures = NULL;
	self->models = NULL;
	self->sounds = NULL;
	self->num_textures = 0;
	self->num_models = 0;
	self->num_sounds = 0;
	self->load_status = 0;
	self->create_type = 1;
	self->id_map._Mysize = 0;
	head = ScePafStdc_FC5CD359(sizeof(*head));
	if (!head)
		ScePafStdc_D608246A();
	self->id_map._Myhead = head;
	if (head) {
		head->_Parent = head;
		head->_Left = head;
		head->_Right = head;
		head->_Color = 0;
		self->id_map._Myhead->_Isnil = 1;
	}
	PafStringInitialize(&self->default_string);
	PafStringAssign(&self->default_string, s_default_string,
			sce_paf_strlen(s_default_string));
	self->resolution = 0;
	PafStringAssign(&self->filename, s_buffer_filename,
			sce_paf_strlen(s_buffer_filename));
	document = ScePafStdc_FC5CD359(sizeof(*document));
	if (document)
		document = CxmlDocumentConstruct(document);
	self->document = document;
	document->allocator = option->cxml_allocator_func;
	document->allocator_userdata = self->rscallocator;
	ret = CxmlDocumentCreateFromBuffer(document, addr, size, 0);
	if (ret)
		self->load_status = ret;
	return self;
}

ResourceObj *ResourceObjConstructFromBufferBase(ResourceObj *self, const void *addr,
					      size_t size, const ResourceObjOption *option)
{
	return ResourceObjConstructFromBuffer(self, addr, size, option);
}

static inline __attribute__((always_inline))
void ResourceIDMapLrotate(ResourceIDMapNode *head,
			     ResourceIDMapNode *node)
{
	ResourceIDMapNode *right = node->_Right;

	node->_Right = right->_Left;
	if (!right->_Left->_Isnil)
		right->_Left->_Parent = node;
	right->_Parent = node->_Parent;
	if (node->_Parent->_Isnil)
		head->_Parent = right;
	else if (node == node->_Parent->_Left)
		node->_Parent->_Left = right;
	else
		node->_Parent->_Right = right;
	right->_Left = node;
	node->_Parent = right;
}

static inline __attribute__((always_inline))
void ResourceIDMapRrotate(ResourceIDMapNode *head,
			     ResourceIDMapNode *node)
{
	ResourceIDMapNode *left = node->_Left;

	node->_Left = left->_Right;
	if (!left->_Right->_Isnil)
		left->_Right->_Parent = node;
	left->_Parent = node->_Parent;
	if (node->_Parent->_Isnil)
		head->_Parent = left;
	else if (node == node->_Parent->_Right)
		node->_Parent->_Right = left;
	else
		node->_Parent->_Left = left;
	left->_Right = node;
	node->_Parent = left;
}

ResourceIDMapNode *ResourceIDMapRebalance(ResourceIDMap *self,
					ResourceIDMapNode *node)
{
	ResourceIDMapNode *head = self->_Myhead;

	if (!head)
		return (ResourceIDMapNode *)self;
	node->_Color = 1;
	while (node != head->_Parent && node->_Parent->_Color) {
		ResourceIDMapNode *parent = node->_Parent;
		ResourceIDMapNode *grandparent = parent->_Parent;
		ResourceIDMapNode *uncle;

		if (parent == grandparent->_Left) {
			uncle = grandparent->_Right;
			if (uncle->_Color) {
				parent->_Color = 0;
				uncle->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				node = node->_Parent->_Parent;
			} else {
				if (node == parent->_Right) {
					node = node->_Parent;
					ResourceIDMapLrotate(head, node);
				}
				node->_Parent->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				ResourceIDMapRrotate(head, node->_Parent->_Parent);
			}
		} else {
			uncle = grandparent->_Left;
			if (uncle->_Color) {
				parent->_Color = 0;
				uncle->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				node = node->_Parent->_Parent;
			} else {
				if (node == parent->_Left) {
					node = node->_Parent;
					ResourceIDMapRrotate(head, node);
				}
				node->_Parent->_Color = 0;
				node->_Parent->_Parent->_Color = 1;
				ResourceIDMapLrotate(head, node->_Parent->_Parent);
			}
		}
	}
	head->_Parent->_Color = 0;
	return head->_Parent;
}

ResourceIDMapNode *ResourceIDMapInsert(ResourceIDMap *self, const ResourceIDEntry *value,
				   unsigned char *inserted)
{
	ResourceIDMapNode *parent = self->_Myhead;
	ResourceIDMapNode *node, *entry;
	int side = 0;

	if (!parent)
		return NULL;
	node = parent->_Parent;
	if (inserted)
		*inserted = 0;
	if (!node->_Isnil) {
		uint32_t key = value->first;

		do {
			parent = node;
			if (key >= node->_Myval.first) {
				if (node->_Myval.first >= key)
					return node;
				node = node->_Right;
				side = 1;
			} else {
				node = node->_Left;
				side = -1;
			}
		} while (!node->_Isnil);
	}
	node = ScePafStdc_FC5CD359(sizeof(*node));
	if (!node) {
		ScePafStdc_D608246A();
		return self->_Myhead;
	}
	entry = PafPlacementNew(sizeof(*entry), node);
	if (entry) {
		ResourceIDMapNode *head = self->_Myhead;

		entry->_Parent = parent;
		entry->_Left = head;
		entry->_Right = head;
		entry->_Myval.first = value->first;
		PafStringInitialize(&entry->_Myval.second);
		PafStringCopy(&entry->_Myval.second, &value->second);
		entry->_Isnil = 0;
		entry->_Color = 1;
	}
	if (side < 0) {
		parent->_Left = node;
		if (parent == self->_Myhead->_Left)
			self->_Myhead->_Left = node;
	} else if (side > 0) {
		parent->_Right = node;
		if (parent == self->_Myhead->_Right)
			self->_Myhead->_Right = node;
	} else {
		ResourceIDMapNode *head = self->_Myhead;

		head->_Parent = node;
		head->_Left = node;
		head->_Right = node;
	}
	++self->_Mysize;
	ResourceIDMapRebalance(self, node);
	if (inserted)
		*inserted = 1;
	return node;
}

ResourceIDMap *ResourceIDMapClear(ResourceIDMap *self)
{
	ResourceIDMapNode *head = self->_Myhead;
	ResourceIDMapNode *node, *parent;

	if (!head)
		return self;
	node = head->_Left;
	while (!node->_Isnil) {
		if (!node->_Left->_Isnil) {
			node = node->_Left;
		} else if (!node->_Right->_Isnil) {
			node = node->_Right;
		} else {
			parent = node->_Parent;
			if (!parent->_Isnil) {
				if (parent->_Left == node)
					parent->_Left = head;
				else
					parent->_Right = head;
			}
			PafStringDestroy(&node->_Myval.second);
			ScePafStdc_1B77082E(node);
			head = self->_Myhead;
			node = parent;
		}
	}
	head->_Parent = head;
	head->_Left = head;
	head->_Right = head;
	self->_Mysize = 0;
	return NULL;
}

ResourceObj *ResourceObjDestroy(ResourceObj *self)
{
	CxmlDocument *document;

	if (self->create_type == 1) {
		if (self->textures) {
			ScePafStdc_5C468294(self->textures);
			self->textures = NULL;
			self->num_textures = 0;
		}
		if (self->models) {
			ScePafStdc_5C468294(self->models);
			self->models = NULL;
			self->num_models = 0;
		}
		if (self->sounds) {
			ScePafStdc_5C468294(self->sounds);
			self->sounds = NULL;
			self->num_sounds = 0;
		}
	}
	if (self->string_document) {
		ScePafStdc_1B77082E(CxmlDocumentDestroy(self->string_document));
		self->string_document = NULL;
		if (self->create_type == 0 && self->string_buffer) {
			if (self->rscallocator)
				HeapAllocatorFree(self->rscallocator, self->string_buffer);
			else
				sce_paf_free(self->string_buffer);
		}
		self->string_buffer = NULL;
	}
	document = self->document;
	if (document) {
		if (self->create_type == 0) {
			CxmlDocumentClear(document);
			document = self->document;
		}
		if (document)
			ScePafStdc_1B77082E(CxmlDocumentDestroy(document));
		self->document = NULL;
	}
	if (self->rscfp.m_ptr)
		self->rscfp.m_ptr->vtable->Close(self->rscfp.m_ptr);
	PafStringDestroy(&self->default_string);
	ResourceIDMapClear(&self->id_map);
	if (self->id_map._Myhead)
		ScePafStdc_1B77082E(self->id_map._Myhead);
	self->id_map._Myhead = (ResourceIDMapNode *)-1;
	self->id_map._Mysize = (size_t)-1;
	PafStringDestroy(&self->filename);
	if (self->rscfp.m_ptr &&
	    sceAtomicDecrement32AcqRel(&self->rscfp.m_ref_counter->m_counter) == 1) {
		self->rscfp.m_ref_counter->m_deleter(self->rscfp.m_ptr);
		if (sceAtomicDecrement32AcqRel(&self->rscfp.m_ref_counter->m_weak_counter) == 1)
			ScePafStdc_1B77082E(self->rscfp.m_ref_counter);
	}
	return self;
}

ResourceObj *ResourceObjDestroyBase(ResourceObj *self)
{
	return ResourceObjDestroy(self);
}

ResourceObj *ResourceObjConstructFromFile(ResourceObj *self, const char *name,
					const ResourceObjOption *option)
{
	ResourceIDMapNode *head;
	CxmlDocument *document;
	FileSharedPtr opened, buffered, fp;
	size_t length;
	int32_t ret;

	self->rscfp.m_ptr = NULL;
	self->rscfp.m_ref_counter = NULL;
	PafStringInitialize(&self->filename);
	self->document = NULL;
	self->string_document = NULL;
	self->rscallocator = option->allocator;
	self->string_buffer = NULL;
	self->texture_buffer = NULL;
	self->model_buffer = NULL;
	self->sound_buffer = NULL;
	self->textures = NULL;
	self->models = NULL;
	self->sounds = NULL;
	self->num_textures = 0;
	self->num_models = 0;
	self->num_sounds = 0;
	self->load_status = 0;
	self->create_type = 0;
	self->id_map._Mysize = 0;
	head = ScePafStdc_FC5CD359(sizeof(*head));
	if (!head)
		ScePafStdc_D608246A();
	self->id_map._Myhead = head;
	if (head) {
		head->_Parent = head;
		head->_Left = head;
		head->_Right = head;
		head->_Color = 0;
		self->id_map._Myhead->_Isnil = 1;
	}
	PafStringInitialize(&self->default_string);
	PafStringAssign(&self->default_string, s_default_string,
			sce_paf_strlen(s_default_string));
	self->resolution = 0;
	length = name ? sce_paf_strlen(name) : 0;
	PafStringAssign(&self->filename, name, length);
	document = ScePafStdc_FC5CD359(sizeof(*document));
	if (document)
		document = CxmlDocumentConstruct(document);
	self->document = document;
	LocalFileOpen(&opened, self->filename.ptr, SCE_O_RDONLY, 0, &ret);
	FileSharedPtrAssign(&self->rscfp, &opened);
	FileSharedPtrRelease(&opened);
	if (self->rscfp.m_ptr) {
		BufferSharedPtr buffer = { NULL, NULL };
		BufferSharedPtr allocated, base_buffer, buffer_arg;
		FileSharedPtr base_file, file_arg;
		void *addr;

		if (self->rscallocator)
			addr = HeapAllocatorMemalign(self->rscallocator, 64, 4096);
		else
			addr = sce_paf_memalign(64, 4096);
		if (addr) {
			HeapAllocBufferCreate(&allocated, addr, 4096, self->rscallocator);
			if (allocated.m_ptr)
				BufferSharedPtrCopy(&buffer, &allocated);
			BufferSharedPtrRelease(&allocated);
		}
		FileSharedPtrCopy(&base_file, &self->rscfp);
		BufferSharedPtrCopy(&base_buffer, &buffer);
		FileSharedPtrCopy(&file_arg, &base_file);
		BufferSharedPtrCopy(&buffer_arg, &base_buffer);
		BufferedFileOpen(&buffered, &file_arg, &ret, &buffer_arg);
		BufferSharedPtrRelease(&buffer_arg);
		FileSharedPtrRelease(&file_arg);
		FileSharedPtrAssign(&self->rscfp, &buffered);
		FileSharedPtrRelease(&buffered);
		BufferSharedPtrRelease(&base_buffer);
		FileSharedPtrRelease(&base_file);
		BufferSharedPtrRelease(&buffer);
	}
	if (self->rscfp.m_ptr) {
		document = self->document;
		document->allocator = option->cxml_allocator_func;
		document->allocator_userdata = self->rscallocator;
		FileSharedPtrCopy(&fp, &self->rscfp);
		ret = CxmlDocumentCreateFromFile(document, &fp, 0);
		FileSharedPtrRelease(&fp);
		if (ret) {
			CxmlDocumentClear(self->document);
			self->rscfp.m_ptr->vtable->Close(self->rscfp.m_ptr);
			self->load_status = ret;
		}
		if (option->load_type & 4) {
			ret = _ZN3paf8resource11ResourceObj23LoadResourceDescriptionEPKc(self, self->filename.ptr);
			if (ret)
				self->load_status = ret;
		}
	} else {
		self->load_status = SCE_PAF_ERROR_RESOURCE_CANT_OPEN;
	}
	return self;
}

ResourceObj *ResourceObjConstructFromFileBase(ResourceObj *self, const char *name,
					    const ResourceObjOption *option)
{
	return ResourceObjConstructFromFile(self, name, option);
}
