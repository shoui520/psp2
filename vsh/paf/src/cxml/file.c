#include "cxml.h"
#include "../paf_types.h"
#include "../std/string.h"
#include <kernel/iofilemgr.h>
#include <sce_atomic.h>

int32_t CxmlFileLoad(const CxmlFile *self, const FileSharedPtr *fp,
		      void *buf, size_t bufsize)
{
	const CxmlDocument *doc = self->doc;
	File *file;
	FileRefCounter *counter;
	uint32_t size;
	int32_t ret;

	if (!doc)
		return SCE_PAF_ERROR_CXML_INVALID_NODE;
	file = fp->m_ptr;
	counter = fp->m_ref_counter;
	if (file)
		sceAtomicIncrement32AcqRel(&counter->m_counter);
	size = (uint32_t)self->size;
	if (doc->access_mode != AccessMode_ReadOnly_File) {
		ret = SCE_PAF_ERROR_CXML_ACCESS_MODE;
	} else if (bufsize < size) {
		ret = SCE_PAF_ERROR_CXML_NOT_ENOUGH_BUF;
	} else {
		int32_t offset = (uint32_t)doc->header.filetable_offset +
			(uint32_t)self->offset;

		ret = (int32_t)file->vtable->Seek(file, (int64_t)offset, SCE_SEEK_SET);
		if (ret < 0)
			ret = SCE_PAF_ERROR_CXML_CANT_SEEK;
		else if ((uint32_t)file->vtable->Read(file, buf, size) != size)
			ret = SCE_PAF_ERROR_CXML_CANT_READ;
		else
			ret = SCE_PAF_OK;
	}
	if (file && sceAtomicDecrement32AcqRel(&counter->m_counter) == 1) {
		counter->m_deleter(file);
		if (sceAtomicDecrement32AcqRel(&counter->m_weak_counter) == 1)
			ScePafStdc_1B77082E(counter);
	}
	return ret;
}
