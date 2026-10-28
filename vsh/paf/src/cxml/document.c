#include "cxml.h"
#include "../paf_types.h"

extern void *sce_paf_memcpy(void *dst, const void *src, size_t size);

int32_t CxmlDocumentCreateFromBuffer(CxmlDocument *self, const void *buf,
				    size_t bufsize, unsigned char copy)
{
	CxmlDocumentClear(self);
	self->access_mode = copy ? AccessMode_ReadWrite : AccessMode_ReadOnly_Memory;
	if (bufsize < sizeof(self->header))
		return SCE_PAF_ERROR_CXML_CANT_READ;
	sce_paf_memcpy(&self->header, buf, sizeof(self->header));
	if (self->header.version != 0x110 && self->header.version != 0x100)
		return SCE_PAF_ERROR_CXML_VERSION;
	if ((uint32_t)self->header.intarraytable_size & 3)
		return SCE_PAF_ERROR_CXML_FORMAT;
	if ((uint32_t)self->header.floatarraytable_size & 3)
		return SCE_PAF_ERROR_CXML_FORMAT;
	if (copy && !self->allocator)
		return SCE_PAF_ERROR_CXML_NO_ALLOCATOR;

	if (self->header.tree_size) {
		if (self->header.tree_offset >= (int32_t)bufsize)
			return SCE_PAF_ERROR_CXML_CANT_SEEK;
		if ((uint32_t)self->header.tree_offset +
		    (uint32_t)self->header.tree_size > bufsize)
			return SCE_PAF_ERROR_CXML_CANT_READ;
		if (copy) {
			void *addr;

			self->allocator(AllocationType_Alloc_Tree, self->allocator_userdata,
					NULL, self->header.tree_size, &addr, &self->tree_capacity);
			if (!addr)
				return SCE_PAF_ERROR_CXML_CANT_ALLOC;
			self->tree = addr;
			sce_paf_memcpy(self->tree, (const char *)buf + self->header.tree_offset,
					self->header.tree_size);
		} else {
			self->tree = (char *)buf + self->header.tree_offset;
		}
	}
	if (self->header.idtable_size) {
		if (self->header.idtable_offset >= (int32_t)bufsize)
			return SCE_PAF_ERROR_CXML_CANT_SEEK;
		if ((uint32_t)self->header.idtable_offset +
		    (uint32_t)self->header.idtable_size > bufsize)
			return SCE_PAF_ERROR_CXML_CANT_READ;
		if (copy) {
			void *addr;

			self->allocator(AllocationType_Alloc_IDTable, self->allocator_userdata,
					NULL, self->header.idtable_size, &addr, &self->idtable_capacity);
			if (!addr)
				return SCE_PAF_ERROR_CXML_CANT_ALLOC;
			self->idtable = addr;
			sce_paf_memcpy(self->idtable, (const char *)buf + self->header.idtable_offset,
					self->header.idtable_size);
		} else {
			self->idtable = (char *)buf + self->header.idtable_offset;
		}
	}
	if (self->header.idhashtable_size) {
		if (self->header.idhashtable_offset >= (int32_t)bufsize)
			return SCE_PAF_ERROR_CXML_CANT_SEEK;
		if ((uint32_t)self->header.idhashtable_offset +
		    (uint32_t)self->header.idhashtable_size > bufsize)
			return SCE_PAF_ERROR_CXML_CANT_READ;
		if (copy) {
			void *addr;

			self->allocator(AllocationType_Alloc_IDHashTable, self->allocator_userdata,
					NULL, self->header.idhashtable_size, &addr,
					&self->idhashtable_capacity);
			if (!addr)
				return SCE_PAF_ERROR_CXML_CANT_ALLOC;
			self->idhashtable = addr;
			sce_paf_memcpy(self->idhashtable, (const char *)buf + self->header.idhashtable_offset,
					self->header.idhashtable_size);
		} else {
			self->idhashtable = (char *)buf + self->header.idhashtable_offset;
		}
	}
	if (self->header.stringtable_size) {
		if (self->header.stringtable_offset >= (int32_t)bufsize)
			return SCE_PAF_ERROR_CXML_CANT_SEEK;
		if ((uint32_t)self->header.stringtable_offset +
		    (uint32_t)self->header.stringtable_size > bufsize)
			return SCE_PAF_ERROR_CXML_CANT_READ;
		if (copy) {
			void *addr;

			self->allocator(AllocationType_Alloc_StringTable, self->allocator_userdata,
					NULL, self->header.stringtable_size, &addr,
					&self->stringtable_capacity);
			if (!addr)
				return SCE_PAF_ERROR_CXML_CANT_ALLOC;
			self->stringtable = addr;
			sce_paf_memcpy(self->stringtable, (const char *)buf + self->header.stringtable_offset,
					self->header.stringtable_size);
		} else {
			self->stringtable = (char *)buf + self->header.stringtable_offset;
		}
		if (self->stringtable[(uint32_t)self->header.stringtable_size - 1])
			return SCE_PAF_ERROR_CXML_FORMAT;
	}
	if (self->header.wstringtable_size) {
		if (self->header.wstringtable_offset >= (int32_t)bufsize)
			return SCE_PAF_ERROR_CXML_CANT_SEEK;
		if ((uint32_t)self->header.wstringtable_offset +
		    (uint32_t)self->header.wstringtable_size > bufsize)
			return SCE_PAF_ERROR_CXML_CANT_READ;
		if (copy) {
			void *addr;

			self->allocator(AllocationType_Alloc_WStringTable, self->allocator_userdata,
					NULL, self->header.wstringtable_size, &addr,
					&self->wstringtable_capacity);
			if (!addr)
				return SCE_PAF_ERROR_CXML_CANT_ALLOC;
			self->wstringtable = addr;
			sce_paf_memcpy(self->wstringtable, (const char *)buf + self->header.wstringtable_offset,
					self->header.wstringtable_size);
		} else {
			self->wstringtable = (char *)buf + self->header.wstringtable_offset;
		}
		if (((const uint16_t *)self->wstringtable)
		    [((uint32_t)self->header.wstringtable_size >> 1) - 1])
			return SCE_PAF_ERROR_CXML_FORMAT;
	}
	if (self->header.hashtable_size) {
		if (self->header.hashtable_offset >= (int32_t)bufsize)
			return SCE_PAF_ERROR_CXML_CANT_SEEK;
		if ((uint32_t)self->header.hashtable_offset +
		    (uint32_t)self->header.hashtable_size > bufsize)
			return SCE_PAF_ERROR_CXML_CANT_READ;
		if (copy) {
			void *addr;

			self->allocator(AllocationType_Alloc_HashTable, self->allocator_userdata,
					NULL, self->header.hashtable_size, &addr, &self->hashtable_capacity);
			if (!addr)
				return SCE_PAF_ERROR_CXML_CANT_ALLOC;
			self->hashtable = addr;
			sce_paf_memcpy(self->hashtable, (const char *)buf + self->header.hashtable_offset,
					self->header.hashtable_size);
		} else {
			self->hashtable = (char *)buf + self->header.hashtable_offset;
		}
	}
	if (self->header.intarraytable_size) {
		if (self->header.intarraytable_offset >= (int32_t)bufsize)
			return SCE_PAF_ERROR_CXML_CANT_SEEK;
		if ((uint32_t)self->header.intarraytable_offset +
		    (uint32_t)self->header.intarraytable_size > bufsize)
			return SCE_PAF_ERROR_CXML_CANT_READ;
		if (copy) {
			void *addr;

			self->allocator(AllocationType_Alloc_IntArrayTable, self->allocator_userdata,
					NULL, self->header.intarraytable_size, &addr,
					&self->intarraytable_capacity);
			if (!addr)
				return SCE_PAF_ERROR_CXML_CANT_ALLOC;
			self->intarraytable = addr;
			sce_paf_memcpy(self->intarraytable, (const char *)buf + self->header.intarraytable_offset,
					self->header.intarraytable_size);
		} else {
			self->intarraytable = (int32_t *)((char *)buf + self->header.intarraytable_offset);
		}
	}
	if (self->header.floatarraytable_size) {
		if (self->header.floatarraytable_offset >= (int32_t)bufsize)
			return SCE_PAF_ERROR_CXML_CANT_SEEK;
		if ((uint32_t)self->header.floatarraytable_offset +
		    (uint32_t)self->header.floatarraytable_size > bufsize)
			return SCE_PAF_ERROR_CXML_CANT_READ;
		if (copy) {
			void *addr;

			self->allocator(AllocationType_Alloc_FloatArrayTable, self->allocator_userdata,
					NULL, self->header.floatarraytable_size, &addr,
					&self->floatarraytable_capacity);
			if (!addr)
				return SCE_PAF_ERROR_CXML_CANT_ALLOC;
			self->floatarraytable = addr;
			sce_paf_memcpy(self->floatarraytable, (const char *)buf + self->header.floatarraytable_offset,
					self->header.floatarraytable_size);
		} else {
			self->floatarraytable = (float *)((char *)buf + self->header.floatarraytable_offset);
		}
	}
	if (self->header.filetable_size) {
		if (self->header.filetable_offset >= (int32_t)bufsize)
			return SCE_PAF_ERROR_CXML_CANT_SEEK;
		if ((uint32_t)self->header.filetable_offset +
		    (uint32_t)self->header.filetable_size > bufsize)
			return SCE_PAF_ERROR_CXML_CANT_READ;
		if (copy) {
			void *addr;

			self->allocator(AllocationType_Alloc_FileTable, self->allocator_userdata,
					NULL, self->header.filetable_size, &addr, &self->filetable_capacity);
			if (!addr)
				return SCE_PAF_ERROR_CXML_CANT_ALLOC;
			self->filetable = addr;
			sce_paf_memcpy(self->filetable, (const char *)buf + self->header.filetable_offset,
					self->header.filetable_size);
		} else {
			self->filetable = (char *)buf + self->header.filetable_offset;
		}
	}
	return SCE_PAF_OK;
}

unsigned char CxmlDocumentIsRegisteredID(CxmlDocument *self, const char *id)
{
	extern int sce_paf_strcmp(const char *left, const char *right);
	extern size_t sce_paf_strlen(const char *text);
	uint32_t offset = 0;

	if (!self->header.idtable_size)
		return 0;
	do {
		const IDBin *bin = (const IDBin *)(self->idtable + offset);
		const char *idptr = bin->id;

		if (*idptr == *id && !sce_paf_strcmp(idptr, id))
			return 1;
		offset += sce_paf_strlen(idptr) + sizeof(IDBin) + 1;
	} while (offset < (uint32_t)self->header.idtable_size);
	return 0;
}

unsigned char CxmlDocumentIsRegisteredIDHash(CxmlDocument *self, uint32_t idhash)
{
	uint32_t size, offset = 0;
	const IDHashBin *bin;

	if (!idhash)
		return 1;
	size = (uint32_t)self->header.idhashtable_size;
	if (!size)
		return 0;
	bin = (const IDHashBin *)self->idhashtable;
	do {
		if (bin->idhash == idhash)
			return 1;
		offset += sizeof(*bin);
		bin++;
	} while (offset < size);
	return 0;
}
