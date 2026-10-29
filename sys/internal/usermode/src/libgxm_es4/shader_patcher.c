/*
 * SceGxm: the shader patcher.
 *
 * Programs are registered once; vertex and fragment programs are then
 * created from them for a vertex layout or blend/output setup and shared
 * by reference count. Memory comes from callbacks or from heaps the
 * patcher manages on fixed regions.
 */

#include "blending_private.h"

GXM_ASSERT_OFFSET(SceGxmShaderPatcher, programs, 0x30);
GXM_ASSERT_OFFSET(SceGxmShaderPatcher, hostMemAllocated, 0x84);
_Static_assert(sizeof(SceGxmShaderPatcher) == 0xb8, "SceGxmShaderPatcher size");
_Static_assert(sizeof(SceGxmRegisteredProgram) == 0x24, "SceGxmRegisteredProgram size");

/* heap nodes come from host memory */
static GxmHeapNode *gxmPatcherAllocNode(void *patcher)
{
	SceGxmShaderPatcher *self = patcher;

	return self->hostAlloc(self->userData, sizeof(GxmHeapNode));
}

static void gxmPatcherFreeNode(void *patcher, GxmHeapNode *node)
{
	SceGxmShaderPatcher *self = patcher;

	self->hostFree(self->userData, node);
}

static void gxmPatcherFragmentUsseFree(void *patcher, void *mem)
{
	gxmHeapFree(&((SceGxmShaderPatcher *)patcher)->fragmentUsseHeap, (SceUInt32)mem);
}

static void gxmPatcherVertexUsseFree(void *patcher, void *mem)
{
	gxmHeapFree(&((SceGxmShaderPatcher *)patcher)->vertexUsseHeap, (SceUInt32)mem);
}

static void gxmPatcherBufferFree(void *patcher, void *mem)
{
	gxmHeapFree(&((SceGxmShaderPatcher *)patcher)->bufferHeap, (SceUInt32)mem);
}

static void *gxmPatcherFragmentUsseAlloc(void *patcher, SceSize size, SceUInt32 *usseOffset)
{
	SceGxmShaderPatcher *self = patcher;
	SceUInt32 mem = gxmHeapAlloc(&self->fragmentUsseHeap, size, gxmUsseAlignment());

	if (mem != 0)
		*usseOffset = mem + (self->fragmentUsseOffset - (SceUInt32)self->fragmentUsseMem);
	return (void *)mem;
}

/* vertex and fragment code share the vertex heap */
static void *gxmPatcherSharedUsseAlloc(void *patcher, SceSize size, SceUInt32 *usseOffset)
{
	SceGxmShaderPatcher *self = patcher;
	SceUInt32 mem = gxmHeapAlloc(&self->vertexUsseHeap, size, gxmUsseAlignment());

	if (mem != 0)
		*usseOffset = mem + (self->fragmentUsseOffset - (SceUInt32)self->vertexUsseMem);
	return (void *)mem;
}

static void *gxmPatcherVertexUsseAlloc(void *patcher, SceSize size, SceUInt32 *usseOffset)
{
	SceGxmShaderPatcher *self = patcher;
	SceUInt32 mem = gxmHeapAlloc(&self->vertexUsseHeap, size, gxmUsseAlignment());

	if (mem != 0)
		*usseOffset = mem + (self->vertexUsseOffset - (SceUInt32)self->vertexUsseMem);
	return (void *)mem;
}

static void *gxmPatcherBufferAlloc(void *patcher, SceSize size)
{
	return (void *)gxmHeapAlloc(&((SceGxmShaderPatcher *)patcher)->bufferHeap, size, 4);
}

/* two ranges overlap */
static inline _Bool gxmRangesOverlap(SceUInt32 a, SceSize aSize, SceUInt32 b, SceSize bSize)
{
	SceUInt32 start = a > b ? a : b;
	SceUInt32 end = a + aSize < b + bSize ? a + aSize : b + bSize;

	return start < end;
}

int sceGxmShaderPatcherCreate(const SceGxmShaderPatcherParams *params,
			      SceGxmShaderPatcher **shaderPatcher)
{
	SceGxmShaderPatcher *patcher;
	SceUInt32 bufferMem, vertexMem, fragmentMem;
	SceSize bufferSize, vertexSize, fragmentSize;

	if (params == NULL || shaderPatcher == NULL || params->hostAllocCallback == NULL ||
	    params->hostFreeCallback == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;

	/* each kind of memory comes from callbacks or from a fixed region, not both */
	if (params->bufferAllocCallback == NULL) {
		if (params->bufferFreeCallback != NULL)
			return SCE_GXM_ERROR_INVALID_VALUE;
		bufferMem = (SceUInt32)params->bufferMem;
		if (bufferMem == 0)
			return SCE_GXM_ERROR_INVALID_VALUE;
		bufferSize = params->bufferMemSize;
		if (bufferSize == 0)
			return SCE_GXM_ERROR_INVALID_VALUE;
	} else {
		if (params->bufferFreeCallback == NULL || params->bufferMem != NULL ||
		    params->bufferMemSize != 0)
			return SCE_GXM_ERROR_INVALID_VALUE;
		bufferMem = 0;
		bufferSize = 0;
	}
	if (params->vertexUsseAllocCallback == NULL) {
		if (params->vertexUsseFreeCallback != NULL)
			return SCE_GXM_ERROR_INVALID_VALUE;
		vertexMem = (SceUInt32)params->vertexUsseMem;
		if (vertexMem == 0)
			return SCE_GXM_ERROR_INVALID_VALUE;
		vertexSize = params->vertexUsseMemSize;
		if (vertexSize == 0)
			return SCE_GXM_ERROR_INVALID_VALUE;
	} else {
		if (params->vertexUsseFreeCallback == NULL || params->vertexUsseMem != NULL ||
		    params->vertexUsseMemSize != 0)
			return SCE_GXM_ERROR_INVALID_VALUE;
		vertexMem = 0;
		vertexSize = 0;
	}
	if (params->fragmentUsseAllocCallback == NULL) {
		if (params->fragmentUsseFreeCallback != NULL)
			return SCE_GXM_ERROR_INVALID_VALUE;
		fragmentMem = (SceUInt32)params->fragmentUsseMem;
		if (fragmentMem == 0)
			return SCE_GXM_ERROR_INVALID_VALUE;
		fragmentSize = params->fragmentUsseMemSize;
		if (fragmentSize == 0)
			return SCE_GXM_ERROR_INVALID_VALUE;
	} else {
		if (params->fragmentUsseFreeCallback == NULL || params->fragmentUsseMem != NULL ||
		    params->fragmentUsseMemSize != 0)
			return SCE_GXM_ERROR_INVALID_VALUE;
		fragmentMem = 0;
		fragmentSize = 0;
	}

	/* the regions may not overlap, except a vertex and fragment heap that are the same */
	if (bufferMem != 0) {
		if (vertexMem != 0 && gxmRangesOverlap(bufferMem, bufferSize, vertexMem, vertexSize))
			return SCE_GXM_ERROR_INVALID_VALUE;
		if (fragmentMem != 0 &&
		    gxmRangesOverlap(bufferMem, bufferSize, fragmentMem, fragmentSize))
			return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (vertexMem != 0 && fragmentMem != 0) {
		if (fragmentMem == vertexMem) {
			if (fragmentSize != vertexSize)
				return SCE_GXM_ERROR_INVALID_VALUE;
		} else if (gxmRangesOverlap(vertexMem, vertexSize, fragmentMem, fragmentSize)) {
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	}

	patcher = params->hostAllocCallback(params->userData, sizeof(*patcher));
	if (patcher == NULL)
		return SCE_GXM_ERROR_OUT_OF_HOST_MEMORY;
	patcher->userData = params->userData;
	patcher->hostAlloc = params->hostAllocCallback;
	patcher->hostFree = params->hostFreeCallback;
	patcher->bufferUserData = params->userData;
	patcher->bufferAlloc = params->bufferAllocCallback;
	patcher->bufferFree = params->bufferFreeCallback;
	patcher->vertexUsseUserData = params->userData;
	patcher->vertexUsseAlloc = params->vertexUsseAllocCallback;
	patcher->vertexUsseFree = params->vertexUsseFreeCallback;
	patcher->fragmentUsseUserData = params->userData;
	patcher->fragmentUsseAlloc = params->fragmentUsseAllocCallback;
	patcher->fragmentUsseFree = params->fragmentUsseFreeCallback;
	patcher->programs = &patcher->programList;
	gxmHeapInit(&patcher->bufferHeap, patcher, gxmPatcherAllocNode, gxmPatcherFreeNode);
	gxmHeapInit(&patcher->vertexUsseHeap, patcher, gxmPatcherAllocNode, gxmPatcherFreeNode);
	gxmHeapInit(&patcher->fragmentUsseHeap, patcher, gxmPatcherAllocNode, gxmPatcherFreeNode);
	if (params->bufferMem != NULL) {
		gxmHeapAddRange(&patcher->bufferHeap, (SceUInt32)params->bufferMem,
				params->bufferMemSize);
		patcher->bufferUserData = patcher;
		patcher->bufferAlloc = gxmPatcherBufferAlloc;
		patcher->bufferFree = gxmPatcherBufferFree;
	}
	if (params->vertexUsseMem != NULL) {
		gxmHeapAddRange(&patcher->vertexUsseHeap, (SceUInt32)params->vertexUsseMem,
				params->vertexUsseMemSize);
		patcher->vertexUsseUserData = patcher;
		patcher->vertexUsseAlloc = gxmPatcherVertexUsseAlloc;
		patcher->vertexUsseFree = gxmPatcherVertexUsseFree;
	}
	if (params->fragmentUsseMem != NULL) {
		gxmHeapAddRange(&patcher->fragmentUsseHeap, (SceUInt32)params->fragmentUsseMem,
				params->fragmentUsseMemSize);
		patcher->fragmentUsseUserData = patcher;
		if (params->fragmentUsseMem == params->vertexUsseMem) {
			patcher->fragmentUsseAlloc = gxmPatcherSharedUsseAlloc;
			patcher->fragmentUsseFree = gxmPatcherVertexUsseFree;
		} else {
			patcher->fragmentUsseAlloc = gxmPatcherFragmentUsseAlloc;
			patcher->fragmentUsseFree = gxmPatcherFragmentUsseFree;
		}
	}
	patcher->bufferMem = params->bufferMem;
	patcher->vertexUsseMem = params->vertexUsseMem;
	patcher->vertexUsseOffset = params->vertexUsseOffset;
	patcher->fragmentUsseMem = params->fragmentUsseMem;
	patcher->fragmentUsseOffset = params->fragmentUsseOffset;
	patcher->programList.patcher = patcher;
	patcher->programList.next = &patcher->programList;
	patcher->programList.prev = &patcher->programList;
	patcher->bufferMemAllocated = 0;
	patcher->vertexUsseMemAllocated = 0;
	patcher->fragmentUsseMemAllocated = 0;
	patcher->programList.programHeader = NULL;
	patcher->programList.literals = NULL;
	patcher->programList.scratch = NULL;
	patcher->programList.thread = NULL;
	patcher->programList.vertexPrograms = NULL;
	patcher->programList.fragmentPrograms = NULL;
	patcher->hostMemAllocated = sizeof(*patcher);
	*shaderPatcher = patcher;
	return 0;
}

int sceGxmShaderPatcherSetUserData(SceGxmShaderPatcher *shaderPatcher, void *userData)
{
	if (shaderPatcher == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	shaderPatcher->userData = userData;
	/* the callbacks of the patcher's own heaps keep the patcher */
	if (shaderPatcher->bufferMem == NULL)
		shaderPatcher->bufferUserData = userData;
	if (shaderPatcher->vertexUsseMem == NULL)
		shaderPatcher->vertexUsseUserData = userData;
	if (shaderPatcher->fragmentUsseMem == NULL)
		shaderPatcher->fragmentUsseUserData = userData;
	return 0;
}

void *sceGxmShaderPatcherGetUserData(const SceGxmShaderPatcher *shaderPatcher)
{
	if (shaderPatcher == NULL)
		return NULL;
	return shaderPatcher->userData;
}

/* the words a registered program's buffers take */
static inline SceSize gxmProgramBufferWords(const SceGxmProgram *program)
{
	return program->scratchBufferCount + program->literalBufferCount + program->threadBufferCount;
}

static inline void gxmPatcherFreeRegistered(SceGxmShaderPatcher *patcher,
					    SceGxmRegisteredProgram *registered)
{
	registered->prev->next = registered->next;
	registered->next->prev = registered->prev;
	patcher->bufferMemAllocated -= gxmProgramBufferWords(registered->programHeader) * 4;
	if (registered->thread != NULL) {
		patcher->bufferFree(patcher->bufferUserData, registered->thread);
		registered->thread = NULL;
	}
	if (registered->scratch != NULL) {
		patcher->bufferFree(patcher->bufferUserData, registered->scratch);
		registered->scratch = NULL;
	}
	if (registered->literals != NULL) {
		patcher->bufferFree(patcher->bufferUserData, registered->literals);
		registered->literals = NULL;
	}
	patcher->hostFree(patcher->userData, registered);
	patcher->hostMemAllocated -= sizeof(*registered);
}

int sceGxmShaderPatcherDestroy(SceGxmShaderPatcher *shaderPatcher)
{
	SceGxmRegisteredProgram *list, *registered;

	if (shaderPatcher == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	list = shaderPatcher->programs;
	for (registered = list->prev; registered != list; registered = list->prev) {
		if (registered->vertexPrograms != NULL || registered->fragmentPrograms != NULL)
			return SCE_GXM_ERROR_PROGRAM_IN_USE;
		gxmPatcherFreeRegistered(shaderPatcher, registered);
	}
	gxmHeapClear(&shaderPatcher->fragmentUsseHeap);
	gxmHeapClear(&shaderPatcher->vertexUsseHeap);
	gxmHeapClear(&shaderPatcher->bufferHeap);
	shaderPatcher->hostFree(shaderPatcher->userData, shaderPatcher);
	return 0;
}

/* buffer memory must be word aligned unless an old SDK allowed less */
static inline _Bool gxmPatcherMisaligned(const void *mem)
{
	return !(gxmGlobals.compatFlags & GXM_COMPAT_PATCHER_UNALIGNED) && ((SceUInt32)mem & 3) != 0;
}

int sceGxmShaderPatcherRegisterProgram(SceGxmShaderPatcher *shaderPatcher,
				       const SceGxmProgram *programHeader,
				       SceGxmShaderPatcherId *programId)
{
	SceGxmRegisteredProgram *registered, *list;
	SceSize size;
	void *mem;
	int err;

	if (shaderPatcher == NULL || programHeader == NULL || programId == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	err = sceGxmProgramCheck(programHeader);
	if (err != 0)
		return err;
	registered = shaderPatcher->hostAlloc(shaderPatcher->userData, sizeof(*registered));
	if (registered == NULL)
		return SCE_GXM_ERROR_OUT_OF_HOST_MEMORY;
	if (gxmPatcherMisaligned(registered)) {
		shaderPatcher->hostFree(shaderPatcher->userData, registered);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	registered->programHeader = programHeader;

	/* the literals, copied from the program */
	if (programHeader->literalBufferCount == 0) {
		registered->literals = NULL;
	} else {
		size = programHeader->literalBufferCount << 2;
		mem = shaderPatcher->bufferAlloc(shaderPatcher->bufferUserData, size);
		if (mem == NULL) {
			err = SCE_GXM_ERROR_OUT_OF_BUFFER_MEMORY;
			goto freeRegistered;
		}
		if (gxmPatcherMisaligned(mem)) {
			shaderPatcher->bufferFree(shaderPatcher->bufferUserData, mem);
			err = SCE_GXM_ERROR_INVALID_ALIGNMENT;
			goto freeRegistered;
		}
		registered->literals = mem;
		sceClibMemcpy(mem, GXM_RELATIVE(programHeader->literalBufferOffset), size);
		SceGxmInternalForTest_631E95AA(registered->literals, size, 0, 0, "Literal buffer");
	}

	/* scratch and per-thread buffers */
	if (programHeader->scratchBufferCount == 0) {
		registered->scratch = NULL;
	} else {
		mem = shaderPatcher->bufferAlloc(shaderPatcher->bufferUserData,
						 programHeader->scratchBufferCount << 2);
		if (mem == NULL) {
			err = SCE_GXM_ERROR_OUT_OF_BUFFER_MEMORY;
			goto freeLiterals;
		}
		if (gxmPatcherMisaligned(mem)) {
			err = SCE_GXM_ERROR_INVALID_ALIGNMENT;
			shaderPatcher->bufferFree(shaderPatcher->bufferUserData, mem);
			goto freeLiterals;
		}
		registered->scratch = mem;
	}
	if (programHeader->threadBufferCount == 0) {
		registered->thread = NULL;
	} else {
		mem = shaderPatcher->bufferAlloc(shaderPatcher->bufferUserData,
						 programHeader->threadBufferCount << 2);
		if (mem == NULL) {
			err = SCE_GXM_ERROR_OUT_OF_BUFFER_MEMORY;
			goto freeScratch;
		}
		if (gxmPatcherMisaligned(mem)) {
			err = SCE_GXM_ERROR_INVALID_ALIGNMENT;
			shaderPatcher->bufferFree(shaderPatcher->bufferUserData, mem);
			goto freeScratch;
		}
		registered->thread = mem;
	}

	shaderPatcher->bufferMemAllocated += gxmProgramBufferWords(programHeader) * 4;
	registered->patcher = shaderPatcher;
	registered->vertexPrograms = NULL;
	registered->fragmentPrograms = NULL;
	list = shaderPatcher->programs;
	registered->next = list;
	registered->prev = list->prev;
	list->prev = registered;
	registered->prev->next = registered;
	shaderPatcher->hostMemAllocated += sizeof(*registered);
	*programId = registered;
	return 0;

freeScratch:
	if (registered->scratch != NULL)
		shaderPatcher->bufferFree(shaderPatcher->bufferUserData, registered->scratch);
freeLiterals:
	if (registered->literals != NULL)
		shaderPatcher->bufferFree(shaderPatcher->bufferUserData, registered->literals);
freeRegistered:
	shaderPatcher->hostFree(shaderPatcher->userData, registered);
	return err;
}

int sceGxmShaderPatcherUnregisterProgram(SceGxmShaderPatcher *shaderPatcher,
					 SceGxmShaderPatcherId programId)
{
	if (shaderPatcher == NULL || programId == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (shaderPatcher != programId->patcher)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (programId->vertexPrograms != NULL || programId->fragmentPrograms != NULL)
		return SCE_GXM_ERROR_PROGRAM_IN_USE;
	gxmPatcherFreeRegistered(shaderPatcher, programId);
	return 0;
}

/* release every program made from it first */
int sceGxmShaderPatcherForceUnregisterProgram(SceGxmShaderPatcher *shaderPatcher,
					      SceGxmShaderPatcherId programId)
{
	SceGxmVertexProgram *vertexProgram;
	SceGxmFragmentProgram *fragmentProgram;

	if (shaderPatcher == NULL || programId == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (shaderPatcher != programId->patcher)
		return SCE_GXM_ERROR_INVALID_VALUE;
	while ((vertexProgram = programId->vertexPrograms) != NULL) {
		programId->vertexPrograms = vertexProgram->next;
		shaderPatcher->hostMemAllocated -= vertexProgram->hostSize;
		shaderPatcher->vertexUsseMemAllocated -= vertexProgram->usseSize;
		shaderPatcher->vertexUsseFree(shaderPatcher->vertexUsseUserData,
					      vertexProgram->usseMem);
		shaderPatcher->hostFree(shaderPatcher->userData, vertexProgram);
	}
	fragmentProgram = programId->fragmentPrograms;
	if (fragmentProgram != NULL) {
		do {
			programId->fragmentPrograms = fragmentProgram->next;
			shaderPatcher->hostMemAllocated -= fragmentProgram->hostSize;
			shaderPatcher->fragmentUsseMemAllocated -= fragmentProgram->usseSize;
			if (fragmentProgram->usseMem != NULL)
				shaderPatcher->fragmentUsseFree(shaderPatcher->fragmentUsseUserData,
								fragmentProgram->usseMem);
			shaderPatcher->hostFree(shaderPatcher->userData, fragmentProgram);
			fragmentProgram = programId->fragmentPrograms;
		} while (fragmentProgram != NULL);
		if (programId->vertexPrograms != NULL)
			return SCE_GXM_ERROR_PROGRAM_IN_USE;
	}
	gxmPatcherFreeRegistered(shaderPatcher, programId);
	return 0;
}

const SceGxmProgram *sceGxmShaderPatcherGetProgramFromId(SceGxmShaderPatcherId programId)
{
	if (programId == NULL)
		return NULL;
	return programId->programHeader;
}

int sceGxmShaderPatcherAddRefVertexProgram(SceGxmShaderPatcher *shaderPatcher,
					   SceGxmVertexProgram *vertexProgram)
{
	if (shaderPatcher == NULL || vertexProgram == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (shaderPatcher != vertexProgram->patcher)
		return SCE_GXM_ERROR_INVALID_VALUE;
	vertexProgram->refCount++;
	return 0;
}

int sceGxmShaderPatcherAddRefFragmentProgram(SceGxmShaderPatcher *shaderPatcher,
					     SceGxmFragmentProgram *fragmentProgram)
{
	if (shaderPatcher == NULL || fragmentProgram == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (shaderPatcher != fragmentProgram->patcher)
		return SCE_GXM_ERROR_INVALID_VALUE;
	fragmentProgram->refCount++;
	return 0;
}

int sceGxmShaderPatcherReleaseVertexProgram(SceGxmShaderPatcher *shaderPatcher,
					    SceGxmVertexProgram *vertexProgram)
{
	SceGxmVertexProgram *prev, *cur;

	if (shaderPatcher == NULL || vertexProgram == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (shaderPatcher != vertexProgram->patcher)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (--vertexProgram->refCount != 0)
		return 0;

	/* unlink it from its registered program */
	prev = vertexProgram->registered->vertexPrograms;
	if (vertexProgram == prev) {
		vertexProgram->registered->vertexPrograms = vertexProgram->next;
	} else {
		if (prev == NULL)
			return SCE_GXM_ERROR_INVALID_VALUE;
		for (cur = prev->next; vertexProgram != cur; cur = cur->next) {
			if (cur == NULL)
				return SCE_GXM_ERROR_INVALID_VALUE;
			prev = cur;
		}
		prev->next = vertexProgram->next;
	}
	shaderPatcher->hostMemAllocated -= vertexProgram->hostSize;
	shaderPatcher->vertexUsseMemAllocated -= vertexProgram->usseSize;
	shaderPatcher->vertexUsseFree(shaderPatcher->vertexUsseUserData, vertexProgram->usseMem);
	shaderPatcher->hostFree(shaderPatcher->userData, vertexProgram);
	return 0;
}

int sceGxmShaderPatcherReleaseFragmentProgram(SceGxmShaderPatcher *shaderPatcher,
					      SceGxmFragmentProgram *fragmentProgram)
{
	SceGxmFragmentProgram *prev, *cur;

	if (shaderPatcher == NULL || fragmentProgram == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (shaderPatcher != fragmentProgram->patcher)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (--fragmentProgram->refCount != 0)
		return 0;

	prev = fragmentProgram->registered->fragmentPrograms;
	if (fragmentProgram == prev) {
		fragmentProgram->registered->fragmentPrograms = fragmentProgram->next;
	} else {
		if (prev == NULL)
			return SCE_GXM_ERROR_INVALID_VALUE;
		for (cur = prev->next; fragmentProgram != cur; cur = cur->next) {
			if (cur == NULL)
				return SCE_GXM_ERROR_INVALID_VALUE;
			prev = cur;
		}
		prev->next = fragmentProgram->next;
	}
	shaderPatcher->hostMemAllocated -= fragmentProgram->hostSize;
	shaderPatcher->fragmentUsseMemAllocated -= fragmentProgram->usseSize;
	if (fragmentProgram->usseMem != NULL)
		shaderPatcher->fragmentUsseFree(shaderPatcher->fragmentUsseUserData,
						fragmentProgram->usseMem);
	shaderPatcher->hostFree(shaderPatcher->userData, fragmentProgram);
	return 0;
}

int sceGxmShaderPatcherGetVertexProgramRefCount(const SceGxmShaderPatcher *shaderPatcher,
						const SceGxmVertexProgram *vertexProgram,
						SceUInt32 *count)
{
	if (shaderPatcher == NULL || vertexProgram == NULL || count == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (shaderPatcher != vertexProgram->patcher)
		return SCE_GXM_ERROR_INVALID_VALUE;
	*count = vertexProgram->refCount;
	return 0;
}

int sceGxmShaderPatcherGetFragmentProgramRefCount(const SceGxmShaderPatcher *shaderPatcher,
						  const SceGxmFragmentProgram *fragmentProgram,
						  SceUInt32 *count)
{
	if (shaderPatcher == NULL || fragmentProgram == NULL || count == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (shaderPatcher != fragmentProgram->patcher)
		return SCE_GXM_ERROR_INVALID_VALUE;
	*count = fragmentProgram->refCount;
	return 0;
}

SceSize sceGxmShaderPatcherGetHostMemAllocated(const SceGxmShaderPatcher *shaderPatcher)
{
	if (shaderPatcher == NULL)
		return 0;
	return shaderPatcher->hostMemAllocated;
}

SceSize sceGxmShaderPatcherGetBufferMemAllocated(const SceGxmShaderPatcher *shaderPatcher)
{
	if (shaderPatcher == NULL)
		return 0;
	return shaderPatcher->bufferMemAllocated;
}

SceSize sceGxmShaderPatcherGetVertexUsseMemAllocated(const SceGxmShaderPatcher *shaderPatcher)
{
	if (shaderPatcher == NULL)
		return 0;
	return shaderPatcher->vertexUsseMemAllocated;
}

SceSize sceGxmShaderPatcherGetFragmentUsseMemAllocated(const SceGxmShaderPatcher *shaderPatcher)
{
	if (shaderPatcher == NULL)
		return 0;
	return shaderPatcher->fragmentUsseMemAllocated;
}

#define SCE_GXM_INDEX_SOURCE_MAX	3
#define GXM_MAX_ATTRIBUTES		16
#define GXM_MAX_STREAMS			16
#define GXM_PROGRAM_INSTANCED		0x00020000
#define GXM_PROGRAM_INDEXED		0x00040000
#define GXM_PROGRAM_PHASED		0x00100000

static _Bool gxmVertexLayoutValid(const SceGxmVertexAttribute *attributes, SceUInt32 attributeCount,
				  const SceGxmVertexStream *streams, SceUInt32 streamCount)
{
	SceUInt32 i, components;

	if (streamCount > GXM_MAX_STREAMS)
		return 0;
	for (i = 0; i < streamCount; i++) {
		if (streams[i].indexSource > SCE_GXM_INDEX_SOURCE_MAX)
			return 0;
	}
	if (attributeCount > GXM_MAX_ATTRIBUTES)
		return 0;
	for (i = 0; i < attributeCount; i++) {
		if (attributes[i].streamIndex >= streamCount ||
		    attributes[i].format > SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED_REG)
			return 0;
		components = attributes[i].componentCount - 1;
		if (attributes[i].format == SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED_REG) {
			if (components > 63)
				return 0;
		} else if (components > 3) {
			return 0;
		}
		if (attributes[i].regIndex > 63 ||
		    attributes[i].regIndex + attributes[i].componentCount > 64)
			return 0;
	}
	return 1;
}

/* an existing program for the same layout */
static SceGxmVertexProgram *gxmFindVertexProgram(SceGxmShaderPatcherId programId,
						 const SceGxmVertexAttribute *attributes,
						 SceUInt32 attributeCount,
						 const SceGxmVertexStream *streams,
						 SceUInt32 streamCount)
{
	SceGxmVertexProgram *program;
	SceUInt32 i;

	for (program = programId->vertexPrograms; program != NULL; program = program->next) {
		if (program->attributeCount != attributeCount || program->streamCount != streamCount)
			continue;
		for (i = 0; i < attributeCount; i++) {
			if (program->attributes[i].streamIndex != attributes[i].streamIndex ||
			    program->attributes[i].offset != attributes[i].offset ||
			    *(const SceUInt16 *)&program->attributes[i].format !=
			    *(const SceUInt16 *)&attributes[i].format ||
			    program->attributes[i].regIndex != attributes[i].regIndex)
				break;
		}
		if (i != attributeCount)
			continue;
		for (i = 0; i < streamCount; i++) {
			if (program->streams[i].stride != streams[i].stride ||
			    program->streams[i].indexSource != streams[i].indexSource)
				break;
		}
		if (i == streamCount)
			return program;
	}
	return NULL;
}

int sceGxmShaderPatcherCreateVertexProgram(SceGxmShaderPatcher *shaderPatcher,
					   SceGxmShaderPatcherId programId,
					   const SceGxmVertexAttribute *attributes,
					   SceUInt32 attributeCount,
					   const SceGxmVertexStream *streams, SceUInt32 streamCount,
					   SceGxmVertexProgram **vertexProgram)
{
	const SceGxmProgram *program;
	const GxmProgramVaryings *varyings;
	SceGxmVertexProgram *created;
	GxmChunkBuffer pdsBuffer, codeBuffer;
	GxmPdsAttributePatch *patches;
	SceUInt32 instanced, indexed, usseOffset, mask, bank, i;
	SceSize pdsSize, hostSize, codeSize, usseSize;
	void *usseMem;
	int err;

	if (shaderPatcher == NULL || programId == NULL || vertexProgram == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (shaderPatcher != programId->patcher)
		return SCE_GXM_ERROR_INVALID_VALUE;
	program = programId->programHeader;
	if (program->flags & GXM_PROGRAM_FRAGMENT)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (attributeCount == 0) {
		if (streamCount != 0)
			return SCE_GXM_ERROR_INVALID_VALUE;
	} else {
		if (attributes == NULL)
			return SCE_GXM_ERROR_INVALID_POINTER;
		if (streamCount == 0)
			return SCE_GXM_ERROR_INVALID_VALUE;
		if (streams == NULL)
			return SCE_GXM_ERROR_INVALID_POINTER;
		if (!(gxmGlobals.compatFlags & GXM_COMPAT_VERTEX_LAYOUT_ANY) &&
		    !gxmVertexLayoutValid(attributes, attributeCount, streams, streamCount))
			return SCE_GXM_ERROR_INVALID_VALUE;
	}

	created = gxmFindVertexProgram(programId, attributes, attributeCount, streams, streamCount);
	if (created != NULL)
		goto done;

	/* one host block: the program, the layout copies and the PDS program */
	instanced = (program->flags & GXM_PROGRAM_INSTANCED) >> 17;
	indexed = (program->flags & GXM_PROGRAM_INDEXED) >> 18;
	pdsSize = gxmPdsVertexProgramSize(attributeCount, streams, streamCount, NULL, instanced,
					  indexed) + 15;
	hostSize = pdsSize + (streamCount + 30 + attributeCount * 4) * 4;
	created = shaderPatcher->hostAlloc(shaderPatcher->userData, hostSize);
	if (created == NULL)
		return SCE_GXM_ERROR_OUT_OF_HOST_MEMORY;
	if (gxmPatcherMisaligned(created)) {
		shaderPatcher->hostFree(shaderPatcher->userData, created);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	created->attributes = (SceGxmVertexAttribute *)(created + 1);
	created->streams = (SceGxmVertexStream *)(created->attributes + attributeCount);
	patches = (GxmPdsAttributePatch *)(created->streams + streamCount);
	gxmChunkBufferInitFixed(&pdsBuffer, patches + attributeCount, 0, pdsSize);
	varyings = gxmProgramGetVaryings(program);

	/* the USSE code */
	codeSize = gxmProgramGetVertexCodeSize(program, attributes, attributeCount);
	usseOffset = 0;
	usseSize = ((codeSize - 1 + gxmUsseAlignment()) & -gxmUsseAlignment()) +
		   gxmProgramGetSecondaryCodeSize(program);
	usseMem = shaderPatcher->vertexUsseAlloc(shaderPatcher->vertexUsseUserData, usseSize,
						 &usseOffset);
	if (usseMem == NULL) {
		err = SCE_GXM_ERROR_OUT_OF_VERTEX_USSE_MEMORY;
		goto freeHost;
	}
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_PATCHER_UNALIGNED) && ((SceUInt32)usseMem & 0xf)) {
		err = SCE_GXM_ERROR_INVALID_ALIGNMENT;
		shaderPatcher->vertexUsseFree(shaderPatcher->vertexUsseUserData, usseMem);
		goto freeHost;
	}
	bank = usseOffset >> 24;
	if (bank != 8 && bank > 3) {
		err = SCE_GXM_ERROR_INVALID_POINTER;
		shaderPatcher->vertexUsseFree(shaderPatcher->vertexUsseUserData, usseMem);
		goto freeHost;
	}
	created->usseMem = usseMem;
	gxmChunkBufferInitFixed(&codeBuffer, usseMem, usseOffset, usseSize);
	created->usseSize = usseSize;
	created->hostSize = hostSize;
	created->programHeader = program;
	err = gxmProgramBuildVertexCode(&codeBuffer, program, attributes, attributeCount,
					&created->code);
	if (err != 0)
		goto freeUsse;
	err = gxmProgramBuildSecondaryCode(&codeBuffer, program, &created->secondaryCode);
	if (err != 0)
		goto freeUsse;
	SceGxmInternalForTest_631E95AA(created->usseMem, usseSize, 1, usseOffset,
				       "Cached vertex program USSE code");

	/* the PDS program fetching the attributes */
	created->pdsFlags = 0x700;
	if (program->textureBufferCount != 0)
		created->pdsFlags = 0x740;
	if (program->dataBufferCount != 0)
		created->pdsFlags = program->textureBufferCount != 0 ? 0x7c0 : 0x780;
	err = gxmPdsBuildVertexProgram(&pdsBuffer, attributes, attributeCount, streams, streamCount,
				       &created->code, &created->pds, patches, instanced, indexed,
				       instanced ? varyings->vertex.instanceData : 0,
				       indexed ? varyings->vertex.indexData : 0);
	if (err != 0)
		goto freeUsse;
	created->secondaryPdsSize = gxmPdsSimpleProgramSize(program->secondaryPdsDataCount, NULL, 0);
	created->patchCount = attributeCount;
	created->patches = patches;
	created->literals = programId->literals;
	created->scratch = programId->scratch;
	created->thread = programId->thread;
	gxmDmsVertexInfo(program->primaryRegCount, program->secondaryRegCount, program->tempRegCount,
			 varyings->vertex.outputs >> 24, (program->flags & GXM_PROGRAM_PHASED) >> 20,
			 &created->dms);
	created->attributeCount = attributeCount;
	created->streamCount = streamCount;
	sceClibMemcpy(created->attributes, attributes, attributeCount * sizeof(*attributes));
	sceClibMemcpy(created->streams, streams, streamCount * sizeof(*streams));
	mask = 0;
	for (i = 0; i < attributeCount; i++)
		mask |= 1 << attributes[i].streamIndex;
	created->streamMask = mask;

	created->registered = programId;
	created->patcher = shaderPatcher;
	created->refCount = 0;
	created->next = programId->vertexPrograms;
	programId->vertexPrograms = created;
	shaderPatcher->hostMemAllocated += hostSize;
	shaderPatcher->vertexUsseMemAllocated += usseSize;
done:
	created->refCount++;
	*vertexProgram = created;
	return 0;

freeUsse:
	shaderPatcher->vertexUsseFree(shaderPatcher->vertexUsseUserData, created->usseMem);
freeHost:
	shaderPatcher->hostFree(shaderPatcher->userData, created);
	return err;
}


/* which vertex texture coordinate feeds each fragment texture coordinate */
static inline void gxmTexCoordRemap(SceUInt8 *remap, const SceGxmProgram *vertexProgram)
{
	SceUInt32 outputs, next, i;

	if (vertexProgram == NULL) {
		for (i = 0; i < 10; i++)
			remap[i] = i;
		return;
	}
	outputs = gxmProgramGetVaryings(vertexProgram)->vertex.texCoordOutputs;
	next = 0;
	if (gxmGlobals.compatFlags & GXM_COMPAT_TEXCOORD_PACKED) {
		for (i = 0; i < 10; i++) {
			remap[i] = next;
			if ((outputs >> (i * 3)) & 7)
				next++;
		}
	} else {
		for (i = 0; i < 10; i++) {
			if ((outputs >> (i * 3)) & 7)
				remap[i] = next++;
			else
				remap[i] = 9;
		}
	}
}

/* blending that changes nothing: the output written as it is */
static inline SceBool gxmBlendIsPassThrough(const SceGxmBlendInfo *blendInfo)
{
	return (blendInfo->colorFunc == SCE_GXM_BLEND_FUNC_NONE ||
		((blendInfo->colorFunc == SCE_GXM_BLEND_FUNC_ADD ||
		  blendInfo->colorFunc == SCE_GXM_BLEND_FUNC_SUBTRACT) &&
		 blendInfo->colorSrc == SCE_GXM_BLEND_FACTOR_ONE &&
		 blendInfo->colorDst == SCE_GXM_BLEND_FACTOR_ZERO)) &&
	       (blendInfo->alphaFunc == SCE_GXM_BLEND_FUNC_NONE ||
		((blendInfo->alphaFunc == SCE_GXM_BLEND_FUNC_ADD ||
		  blendInfo->alphaFunc == SCE_GXM_BLEND_FUNC_SUBTRACT) &&
		 blendInfo->alphaSrc == SCE_GXM_BLEND_FACTOR_ONE &&
		 blendInfo->alphaDst == SCE_GXM_BLEND_FACTOR_ZERO)) &&
	       blendInfo->colorMask == SCE_GXM_COLOR_MASK_ALL;
}

/* the result depends on the colour already in the output buffer */
static inline SceBool gxmBlendNeedsDestination(const SceGxmBlendInfo *blendInfo)
{
	if (blendInfo->colorFunc != SCE_GXM_BLEND_FUNC_NONE &&
	    !(blendInfo->colorSrc <= SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA &&
	      blendInfo->colorDst == SCE_GXM_BLEND_FACTOR_ZERO))
		return SCE_TRUE;
	if (blendInfo->alphaFunc != SCE_GXM_BLEND_FUNC_NONE &&
	    !(blendInfo->alphaSrc <= SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA &&
	      blendInfo->alphaDst == SCE_GXM_BLEND_FACTOR_ZERO))
		return SCE_TRUE;
	return blendInfo->colorMask != SCE_GXM_COLOR_MASK_ALL;
}

/* blending leaves the colour in the output buffer as it was */
static inline SceBool gxmBlendKeepsDestination(const SceGxmBlendInfo *blendInfo)
{
	SceBool color, alpha;

	if (!(blendInfo->colorMask & (SCE_GXM_COLOR_MASK_R | SCE_GXM_COLOR_MASK_G |
				      SCE_GXM_COLOR_MASK_B)))
		color = SCE_TRUE;
	else
		color = (blendInfo->colorFunc == SCE_GXM_BLEND_FUNC_ADD ||
			 blendInfo->colorFunc == SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT) &&
			blendInfo->colorSrc == SCE_GXM_BLEND_FACTOR_ZERO &&
			blendInfo->colorDst == SCE_GXM_BLEND_FACTOR_ONE;
	if (!(blendInfo->colorMask & SCE_GXM_COLOR_MASK_A))
		alpha = SCE_TRUE;
	else
		alpha = (blendInfo->alphaFunc == SCE_GXM_BLEND_FUNC_ADD ||
			 blendInfo->alphaFunc == SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT) &&
			blendInfo->alphaSrc == SCE_GXM_BLEND_FACTOR_ZERO &&
			blendInfo->alphaDst == SCE_GXM_BLEND_FACTOR_ONE;
	return color && alpha;
}

/* an existing program for the same setup */
static inline SceGxmFragmentProgram *gxmFindFragmentProgram(SceGxmShaderPatcherId programId,
							    const SceUInt8 *remap,
							    SceUInt32 outputFormat,
							    const SceGxmBlendInfo *blendInfo,
							    SceBool phased, SceUInt32 samples)
{
	SceGxmFragmentProgram *program;
	SceUInt32 i;

	for (program = programId->fragmentPrograms; program != NULL; program = program->next) {
		for (i = 0; i < 10; i++) {
			if (program->texCoordRemap[i] != remap[i])
				break;
		}
		if (i != 10 || program->outputFormat != outputFormat)
			continue;
		if (blendInfo == NULL) {
			if (program->blendInfo != NULL)
				continue;
		} else {
			if (program->blendInfo == NULL ||
			    *(const SceUInt32 *)program->blendInfo != *(const SceUInt32 *)blendInfo)
				continue;
		}
		if (phased && samples != program->multisampleMode)
			continue;
		return program;
	}
	return NULL;
}

/*
 * Both creation entry points; texCoordMap (GLES) picks, for each fragment
 * texture coordinate, the vertex output it reads.
 */
static inline __attribute__((always_inline))
int gxmCreateFragmentProgram(SceGxmShaderPatcher *shaderPatcher, SceGxmShaderPatcherId programId,
			     SceUInt32 outputFormat, SceUInt32 multisampleMode,
			     const SceGxmBlendInfo *blendInfo, const SceGxmProgram *vertexProgram,
			     const SceUInt8 *texCoordMap, SceGxmFragmentProgram **fragmentProgram)
{
	const SceGxmProgram *program;
	const GxmProgramVaryings *varyings;
	const GxmProgramFragmentInput *inputs;
	SceGxmFragmentProgram *created;
	GxmFragmentBuild build;
	GxmChunkBuffer pdsBuffer, codeBuffer;
	SceUInt8 remap[10], vertexRemap[10];
	SceUInt32 flags, samples, usseOffset, bank, passFlags, i;
	SceSize pdsSize, hostSize, usseSize;
	SceBool phased, colorUnchanged, depthOnly;
	void *usseMem, *tail;
	int err;

	if (shaderPatcher == NULL || programId == NULL || fragmentProgram == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (shaderPatcher != programId->patcher)
		return SCE_GXM_ERROR_INVALID_VALUE;
	program = programId->programHeader;
	flags = program->flags;
	if (!(flags & GXM_PROGRAM_FRAGMENT))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_SURFACE_ANY) &&
	    (outputFormat > 8 || multisampleMode > SCE_GXM_MULTISAMPLE_4X))
		return SCE_GXM_ERROR_INVALID_VALUE;

	/* how the program writes its output */
	if (flags & GXM_PROGRAM_NATIVE_COLOR) {
		if (flags & GXM_PROGRAM_FRAG_COLOR) {
			if (multisampleMode == SCE_GXM_MULTISAMPLE_2X) {
				if (!(flags & (GXM_PROGRAM_MSAA_2X | GXM_PROGRAM_MSAA_4X)))
					return SCE_GXM_ERROR_INVALID_VALUE;
			} else if (multisampleMode == SCE_GXM_MULTISAMPLE_NONE) {
				if (flags & (GXM_PROGRAM_MSAA_2X | GXM_PROGRAM_MSAA_4X))
					return SCE_GXM_ERROR_INVALID_VALUE;
			} else if (multisampleMode == SCE_GXM_MULTISAMPLE_4X) {
				if (!(flags & GXM_PROGRAM_MSAA_4X))
					return SCE_GXM_ERROR_INVALID_VALUE;
			}
		} else if (flags & GXM_PROGRAM_UNK2000) {
			blendInfo = NULL;
			samples = 0;
			depthOnly = SCE_FALSE;
			colorUnchanged = SCE_FALSE;
			phased = SCE_FALSE;
			goto setup;
		}
		blendInfo = NULL;
		if (flags & GXM_PROGRAM_UNK2000) {
			colorUnchanged = SCE_FALSE;
			depthOnly = SCE_FALSE;
		} else {
			colorUnchanged = SCE_TRUE;
			depthOnly = !(flags & (GXM_PROGRAM_DISCARD | GXM_PROGRAM_DEPTH_REPLACE |
					       GXM_PROGRAM_UNK4000));
		}
	} else if (flags & GXM_PROGRAM_UNK10000) {
		colorUnchanged = SCE_TRUE;
		depthOnly = !(flags & (GXM_PROGRAM_DISCARD | GXM_PROGRAM_DEPTH_REPLACE |
				       GXM_PROGRAM_UNK4000));
		blendInfo = NULL;
	} else {
		if (blendInfo != NULL && gxmBlendIsPassThrough(blendInfo))
			blendInfo = NULL;
		if (blendInfo == NULL || !gxmBlendNeedsDestination(blendInfo)) {
			colorUnchanged = SCE_FALSE;
			depthOnly = SCE_FALSE;
			phased = SCE_FALSE;
			samples = 0;
			goto setup;
		}
		if (gxmBlendKeepsDestination(blendInfo)) {
			colorUnchanged = SCE_TRUE;
			depthOnly = !(flags & (GXM_PROGRAM_DISCARD | GXM_PROGRAM_DEPTH_REPLACE |
					       GXM_PROGRAM_UNK4000));
		} else {
			depthOnly = SCE_FALSE;
			colorUnchanged = SCE_FALSE;
		}
	}
	phased = SCE_TRUE;
	samples = multisampleMode;

setup:
	if (texCoordMap == NULL) {
		gxmTexCoordRemap(remap, vertexProgram);
	} else {
		gxmTexCoordRemap(vertexRemap, vertexProgram);
		for (i = 0; i < 10; i++) {
			if (texCoordMap[i] >= 10)
				return SCE_GXM_ERROR_INVALID_VALUE;
			remap[i] = vertexRemap[texCoordMap[i]];
		}
	}
	err = gxmFragmentBuildInit(&build, program, outputFormat, samples, blendInfo);
	if (err != 0)
		return err;

	created = gxmFindFragmentProgram(programId, remap, outputFormat, blendInfo, phased,
					 samples);
	if (created != NULL)
		goto done;

	/* one host block: the program, the texture indices, the blend info, the PDS program */
	varyings = gxmProgramGetVaryings(program);
	inputs = GXM_RELATIVE(varyings->fragment.inputsOffset);
	pdsSize = gxmPdsFragmentProgramSize(inputs, varyings->fragment.inputCount, 0) + 15;
	hostSize = pdsSize + (varyings->fragment.textureIndexCount + 32) * 4 +
		   (blendInfo != NULL ? 4 : 0);
	created = shaderPatcher->hostAlloc(shaderPatcher->userData, hostSize);
	if (created == NULL)
		return SCE_GXM_ERROR_OUT_OF_HOST_MEMORY;
	if (gxmPatcherMisaligned(created)) {
		shaderPatcher->hostFree(shaderPatcher->userData, created);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	tail = (SceUInt32 *)created + varyings->fragment.textureIndexCount + 32;
	if (blendInfo == NULL) {
		created->blendInfo = NULL;
	} else {
		created->blendInfo = tail;
		sceClibMemcpy(tail, blendInfo, sizeof(*blendInfo));
		tail = created->blendInfo + 1;
	}
	created->multisampleMode = samples;
	for (i = 0; i < 10; i++)
		created->texCoordRemap[i] = remap[i];
	created->outputFormat = outputFormat;
	gxmChunkBufferInitFixed(&pdsBuffer, tail, 0, pdsSize);

	/* the USSE code */
	usseSize = ((gxmFragmentBuildGetCodeSize(&build) - 1 + gxmUsseAlignment()) &
		    -gxmUsseAlignment()) + gxmProgramGetSecondaryCodeSize(program);
	usseOffset = 0;
	usseMem = shaderPatcher->fragmentUsseAlloc(shaderPatcher->fragmentUsseUserData, usseSize,
						   &usseOffset);
	if (usseMem == NULL) {
		err = SCE_GXM_ERROR_OUT_OF_FRAGMENT_USSE_MEMORY;
		goto freeHost;
	}
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_PATCHER_UNALIGNED) && ((SceUInt32)usseMem & 0xf)) {
		err = SCE_GXM_ERROR_INVALID_ALIGNMENT;
		shaderPatcher->fragmentUsseFree(shaderPatcher->fragmentUsseUserData, usseMem);
		goto freeHost;
	}
	bank = usseOffset >> 24;
	if (bank != 9 && (bank < 4 || bank > 6)) {
		err = SCE_GXM_ERROR_INVALID_POINTER;
		shaderPatcher->fragmentUsseFree(shaderPatcher->fragmentUsseUserData, usseMem);
		goto freeHost;
	}
	created->usseMem = usseMem;
	gxmChunkBufferInitFixed(&codeBuffer, usseMem, usseOffset, usseSize);
	created->hostSize = hostSize;
	created->usseSize = usseSize;
	created->programHeader = program;
	err = gxmFragmentBuildCode(&build, &codeBuffer, &created->code);
	if (err != 0)
		goto freeUsse;
	err = gxmProgramBuildSecondaryCode(&codeBuffer, program, &created->secondaryCode);
	if (err != 0)
		goto freeUsse;
	SceGxmInternalForTest_631E95AA(created->usseMem, usseSize, 1, usseOffset,
				       "Cached fragment program USSE code");

	/* the state words and the PDS program fetching the inputs */
	created->pdsFlags = 0x38 | (program->textureBufferCount != 0 ? 2 : 0) |
			    (program->dataBufferCount != 0 ? 4 : 0);
	if (flags & GXM_PROGRAM_DEPTH_REPLACE)
		created->ispFlags = 0x0a000000;
	else if (flags & GXM_PROGRAM_DISCARD)
		created->ispFlags = 0x04000000;
	else
		created->ispFlags = phased ? 0x02000000 : 0;
	if (depthOnly)
		created->ispFlags |= 0x00200000;
	created->dms = gxmProgramGetFragmentDms(program, phased, samples);
	err = gxmPdsBuildFragmentProgram(&pdsBuffer, NULL, inputs, varyings->fragment.inputCount,
					 remap, &created->code, &created->pds,
					 (SceUInt32 *)(created + 1));
	if (err != 0)
		goto freeUsse;
	created->secondaryPdsSize = gxmPdsSimpleProgramSize(program->secondaryPdsDataCount, NULL, 0);
	created->textureIndexCount = varyings->fragment.textureIndexCount;
	created->textureIndices = (SceUInt32 *)(created + 1);
	created->literals = programId->literals;
	created->scratch = programId->scratch;
	created->thread = programId->thread;
	if (!phased)
		passFlags = build.wideOutput ? 0x02 : 0x01;
	else if (samples == SCE_GXM_MULTISAMPLE_4X)
		passFlags = build.wideOutput ? 0x12 : 0x11;
	else if (samples == SCE_GXM_MULTISAMPLE_2X)
		passFlags = build.wideOutput ? 0x0a : 0x09;
	else
		passFlags = build.wideOutput ? 0x06 : 0x05;
	if (colorUnchanged)
		passFlags |= 0x20;
	created->passFlags = passFlags;

	created->refCount = 0;
	created->patcher = shaderPatcher;
	created->registered = programId;
	created->next = programId->fragmentPrograms;
	programId->fragmentPrograms = created;
	shaderPatcher->hostMemAllocated += hostSize;
	shaderPatcher->fragmentUsseMemAllocated += usseSize;
done:
	created->refCount++;
	*fragmentProgram = created;
	return 0;

freeUsse:
	shaderPatcher->fragmentUsseFree(shaderPatcher->fragmentUsseUserData, created->usseMem);
freeHost:
	shaderPatcher->hostFree(shaderPatcher->userData, created);
	return err;
}

int sceGxmShaderPatcherCreateFragmentProgram(SceGxmShaderPatcher *shaderPatcher,
					     SceGxmShaderPatcherId programId,
					     SceUInt32 outputFormat, SceUInt32 multisampleMode,
					     const SceGxmBlendInfo *blendInfo,
					     const SceGxmProgram *vertexProgram,
					     SceGxmFragmentProgram **fragmentProgram)
{
	return gxmCreateFragmentProgram(shaderPatcher, programId, outputFormat, multisampleMode,
					blendInfo, vertexProgram, NULL, fragmentProgram);
}

int sceGxmShaderPatcherCreateFragmentProgramForGles(SceGxmShaderPatcher *shaderPatcher,
						    SceGxmShaderPatcherId programId,
						    SceUInt32 outputFormat,
						    SceUInt32 multisampleMode,
						    const SceGxmBlendInfo *blendInfo,
						    const SceGxmProgram *vertexProgram,
						    const SceUInt8 *texCoordMap,
						    SceGxmFragmentProgram **fragmentProgram)
{
	return gxmCreateFragmentProgram(shaderPatcher, programId, outputFormat, multisampleMode,
					blendInfo, vertexProgram, texCoordMap, fragmentProgram);
}

/*
 * The mask update program: a fragment program around a built-in empty
 * program header, kept on the patcher's list head.
 */
typedef struct GxmMaskUpdateProgram {
	SceGxmFragmentProgram fragment;
	SceGxmProgram program;		/* 0x80 */
	SceUInt32 pad;
	SceUInt8 varyings[0x14];	/* 0x11c: the program's fragment varyings */
} GxmMaskUpdateProgram;

_Static_assert(sizeof(GxmMaskUpdateProgram) == 0x130, "GxmMaskUpdateProgram size");

#define GXM_PROGRAM_MAGIC	0x00505847	/* "GXP" */

int sceGxmShaderPatcherCreateMaskUpdateFragmentProgram(SceGxmShaderPatcher *shaderPatcher,
						       SceGxmFragmentProgram **fragmentProgram)
{
	SceGxmRegisteredProgram *registered;
	GxmMaskUpdateProgram *created;
	GxmProgramVaryings *varyings;

	if (shaderPatcher == NULL || fragmentProgram == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	registered = shaderPatcher->programs;
	created = (GxmMaskUpdateProgram *)registered->fragmentPrograms;
	if (created == NULL) {
		created = shaderPatcher->hostAlloc(shaderPatcher->userData, sizeof(*created));
		if (created == NULL)
			return SCE_GXM_ERROR_OUT_OF_HOST_MEMORY;
		if (gxmPatcherMisaligned(created)) {
			shaderPatcher->hostFree(shaderPatcher->userData, created);
			return SCE_GXM_ERROR_INVALID_ALIGNMENT;
		}
		sceClibMemset(created, 0, sizeof(*created));
		created->program.magic = GXM_PROGRAM_MAGIC;
		created->program.flags = GXM_PROGRAM_FRAGMENT | GXM_PROGRAM_UNK80000;
		created->program.majorVersion = 1;
		created->program.minorVersion = 5;
		created->program.sdkVersion = FW_VERSION >> 16;
		created->program.size = sizeof(created->program) + sizeof(created->pad) +
					sizeof(created->varyings);
		created->fragment.programHeader = &created->program;
		created->program.varyingsOffset = (char *)created->varyings -
						  (char *)&created->program.varyingsOffset;
		created->program.reserved2 = 0;
		varyings = (GxmProgramVaryings *)created->varyings;
		varyings->fragment.outputRegisterType = GXM_OUTPUT_TYPE_U8;
		varyings->fragment.outputComponentCount = 4;
		created->fragment.pdsFlags = 0x838;
		created->fragment.ispFlags = 0x06200000;
		created->fragment.dms = gxmProgramGetFragmentDms(&created->program, SCE_FALSE, 0);
		created->fragment.patcher = shaderPatcher;
		created->fragment.registered = registered;
		created->fragment.passFlags = 0x21;
		created->fragment.hostSize = sizeof(*created);
		registered->fragmentPrograms = &created->fragment;
		shaderPatcher->hostMemAllocated += sizeof(*created);
	}
	created->fragment.refCount++;
	*fragmentProgram = &created->fragment;
	return 0;
}
