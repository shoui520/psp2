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
GXM_ASSERT_SIZE(SceGxmShaderPatcher, 0xb8);
GXM_ASSERT_SIZE(SceGxmRegisteredProgram, 0x24);

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

	if (params == NULL) {
		GXM_DBG_ERROR_NULL(562, sceGxmShaderPatcherCreate, params);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (shaderPatcher == NULL) {
		GXM_DBG_ERROR_NULL(566, sceGxmShaderPatcherCreate, shaderPatcher);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (params->hostAllocCallback == NULL) {
		GXM_DBG_ERROR_NULL(572, sceGxmShaderPatcherCreate, params->hostAllocCallback);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (params->hostFreeCallback == NULL) {
		GXM_DBG_ERROR_NULL(576, sceGxmShaderPatcherCreate, params->hostFreeCallback);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}

	/* each kind of memory comes from callbacks or from a fixed region, not both */
	if (params->bufferAllocCallback == NULL) {
		if (params->bufferFreeCallback != NULL) {
			GXM_DBG_ERROR(596, "Parameter params->bufferFreeCallback to function sceGxmShaderPatcherCreate "
				      "must be NULL if params->bufferAllocCallback is NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		bufferMem = (SceUInt32)params->bufferMem;
		if (bufferMem == 0) {
			GXM_DBG_ERROR(600, "Parameter params->bufferMem to function sceGxmShaderPatcherCreate "
				      "must be non-NULL if params->bufferAllocCallback is NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		bufferSize = params->bufferMemSize;
		if (bufferSize == 0) {
			GXM_DBG_ERROR(604, "Parameter params->bufferMemSize to function sceGxmShaderPatcherCreate "
				      "must be non-zero if params->bufferAllocCallback is NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	} else {
		if (params->bufferFreeCallback == NULL) {
			GXM_DBG_ERROR(583, "Parameter params->bufferFreeCallback to function sceGxmShaderPatcherCreate "
				      "must be non-NULL if params->bufferAllocCallback is non-NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (params->bufferMem != NULL) {
			GXM_DBG_ERROR(587, "Parameter params->bufferMem to function sceGxmShaderPatcherCreate "
				      "must be NULL if params->bufferAllocCallback is non-NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (params->bufferMemSize != 0) {
			GXM_DBG_ERROR(591, "Parameter params->bufferMemSize to function sceGxmShaderPatcherCreate "
				      "must be zero if params->bufferAllocCallback is non-NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		bufferMem = 0;
		bufferSize = 0;
	}
	if (params->vertexUsseAllocCallback == NULL) {
		if (params->vertexUsseFreeCallback != NULL) {
			GXM_DBG_ERROR(625, "Parameter params->vertexUsseFreeCallback to function sceGxmShaderPatcherCreate "
				      "must be NULL if params->vertexUsseAllocCallback is NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		vertexMem = (SceUInt32)params->vertexUsseMem;
		if (vertexMem == 0) {
			GXM_DBG_ERROR(629, "Parameter params->vertexUsseMem to function sceGxmShaderPatcherCreate "
				      "must be non-NULL if params->vertexUsseAllocCallback is NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		vertexSize = params->vertexUsseMemSize;
		if (vertexSize == 0) {
			GXM_DBG_ERROR(633, "Parameter params->vertexUsseMemSize to function sceGxmShaderPatcherCreate "
				      "must be non-zero if params->vertexUsseAllocCallback is NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	} else {
		if (params->vertexUsseFreeCallback == NULL) {
			GXM_DBG_ERROR(612, "Parameter params->vertexUsseFreeCallback to function sceGxmShaderPatcherCreate "
				      "must be non-NULL if params->vertexUsseAllocCallback is non-NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (params->vertexUsseMem != NULL) {
			GXM_DBG_ERROR(616, "Parameter params->vertexUsseMem to function sceGxmShaderPatcherCreate "
				      "must be NULL if params->vertexUsseAllocCallback is non-NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (params->vertexUsseMemSize != 0) {
			GXM_DBG_ERROR(620, "Parameter params->vertexUsseMemSize to function sceGxmShaderPatcherCreate "
				      "must be zero if params->vertexUsseAllocCallback is non-NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		vertexMem = 0;
		vertexSize = 0;
	}
	if (params->fragmentUsseAllocCallback == NULL) {
		if (params->fragmentUsseFreeCallback != NULL) {
			GXM_DBG_ERROR(654, "Parameter params->fragmentUsseFreeCallback to function sceGxmShaderPatcherCreate "
				      "must be NULL if params->fragmentUsseAllocCallback is NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		fragmentMem = (SceUInt32)params->fragmentUsseMem;
		if (fragmentMem == 0) {
			GXM_DBG_ERROR(658, "Parameter params->fragmentUsseMem to function sceGxmShaderPatcherCreate "
				      "must be non-NULL if params->fragmentUsseAllocCallback is NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		fragmentSize = params->fragmentUsseMemSize;
		if (fragmentSize == 0) {
			GXM_DBG_ERROR(662, "Parameter params->fragmentUsseMemSize to function sceGxmShaderPatcherCreate "
				      "must be non-zero if params->fragmentUsseAllocCallback is NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	} else {
		if (params->fragmentUsseFreeCallback == NULL) {
			GXM_DBG_ERROR(641, "Parameter params->fragmentUsseFreeCallback to function sceGxmShaderPatcherCreate "
				      "must be non-NULL if params->fragmentUsseAllocCallback is non-NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (params->fragmentUsseMem != NULL) {
			GXM_DBG_ERROR(645, "Parameter params->fragmentUsseMem to function sceGxmShaderPatcherCreate "
				      "must be NULL if params->fragmentUsseAllocCallback is non-NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (params->fragmentUsseMemSize != 0) {
			GXM_DBG_ERROR(649, "Parameter params->fragmentUsseMemSize to function sceGxmShaderPatcherCreate "
				      "must be zero if params->fragmentUsseAllocCallback is non-NULL.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		fragmentMem = 0;
		fragmentSize = 0;
	}

	/* the regions may not overlap, except a vertex and fragment heap that are the same */
	if (bufferMem != 0) {
		if (vertexMem != 0 && gxmRangesOverlap(bufferMem, bufferSize, vertexMem, vertexSize)) {
			GXM_DBG_ERROR(675, "The buffer memory and vertex USSE memory are overlapping.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (fragmentMem != 0 &&
		    gxmRangesOverlap(bufferMem, bufferSize, fragmentMem, fragmentSize)) {
			GXM_DBG_ERROR(686, "The buffer memory and fragment USSE memory are overlapping.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	}
	if (vertexMem != 0 && fragmentMem != 0) {
		if (fragmentMem == vertexMem) {
			if (fragmentSize != vertexSize) {
				GXM_DBG_ERROR(697, "The vertex USSE memory size and fragment USSE memory "
					      "size must match when using the same memory area for vertex "
					      "and fragment USSE code.");
				return SCE_GXM_ERROR_INVALID_VALUE;
			}
		} else if (gxmRangesOverlap(vertexMem, vertexSize, fragmentMem, fragmentSize)) {
			GXM_DBG_ERROR(707, "The vertex USSE memory and fragment USSE memory must not "
				      "overlap when not using the same memory area for vertex and fragment "
				      "USSE code.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	}

	patcher = params->hostAllocCallback(params->userData, sizeof(*patcher));
	if (patcher == NULL) {
		GXM_DBG_ERROR(718, "Shader patcher host memory allocation returned NULL for allocation "
			      "of size %u.", sizeof(*patcher));
		return SCE_GXM_ERROR_OUT_OF_HOST_MEMORY;
	}
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
	if (shaderPatcher == NULL) {
		GXM_DBG_ERROR_NULL(748, sceGxmShaderPatcherSetUserData, shaderPatcher);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
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

/* the debug build reports these failures out of line */
static GXM_DBG_NOINLINE int
gxmPatcherVertexProgramsInUse(const SceGxmRegisteredProgram *registered)
{
	GXM_DBG_ERROR(405, "Registered program with value %p still has vertex programs in use.",
		      registered);
	return SCE_GXM_ERROR_PROGRAM_IN_USE;
}

static inline int gxmPatcherFreeRegistered(SceGxmShaderPatcher *patcher,
					   SceGxmRegisteredProgram *registered)
{
	SceGxmRegisteredProgram *prev, *next;

	if (registered->vertexPrograms != NULL)
		return gxmPatcherVertexProgramsInUse(registered);
	if (registered->fragmentPrograms != NULL) {
		GXM_DBG_ERROR(410, "Registered program with value %p still has fragment programs in use.",
			      registered);
		return SCE_GXM_ERROR_PROGRAM_IN_USE;
	}
	prev = registered->prev;
	next = registered->next;

	prev->next = next;
	next->prev = prev;
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
	return 0;
}

int sceGxmShaderPatcherDestroy(SceGxmShaderPatcher *shaderPatcher)
{
	SceGxmRegisteredProgram *list, *registered;
	int err;

	if (shaderPatcher == NULL) {
		GXM_DBG_ERROR_NULL(773, sceGxmShaderPatcherDestroy, shaderPatcher);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	list = shaderPatcher->programs;
	for (registered = list->prev; registered != list; registered = list->prev) {
		err = gxmPatcherFreeRegistered(shaderPatcher, registered);
		if (err != 0)
			return err;
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

static GXM_DBG_NOINLINE int gxmPatcherHostAllocFailed(SceSize size)
{
	GXM_DBG_ERROR(50, "Shader patcher host memory allocation returned NULL for allocation of "
		      "size %d", size);
	return SCE_GXM_ERROR_OUT_OF_HOST_MEMORY;
}

static GXM_DBG_NOINLINE int gxmPatcherBufferAllocFailed(SceSize size)
{
	GXM_DBG_ERROR(86, "Shader patcher buffer memory allocation returned NULL for allocation of "
		      "size %d", size);
	return SCE_GXM_ERROR_OUT_OF_BUFFER_MEMORY;
}

/* host memory for the patcher's own records */
static inline int gxmPatcherHostAlloc(SceGxmShaderPatcher *patcher, SceSize size, void **mem)
{
	void *allocated = patcher->hostAlloc(patcher->userData, size);

	if (allocated == NULL)
		return gxmPatcherHostAllocFailed(size);
	if (gxmPatcherMisaligned(allocated)) {
		patcher->hostFree(patcher->userData, allocated);
		GXM_DBG_ERROR(62, "Shader patcher host memory allocation of size %d returned %p, "
			      "which is not aligned to 4", size, allocated);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	*mem = allocated;
	return 0;
}

/* buffer memory for a registered program's literals, scratch and thread buffers */
static inline int gxmPatcherBufferAllocChecked(SceGxmShaderPatcher *patcher, SceSize size,
					       void **mem)
{
	void *allocated = patcher->bufferAlloc(patcher->bufferUserData, size);

	if (allocated == NULL)
		return gxmPatcherBufferAllocFailed(size);
	if (gxmPatcherMisaligned(allocated)) {
		patcher->bufferFree(patcher->bufferUserData, allocated);
		GXM_DBG_ERROR(98, "Shader patcher buffer memory allocation of size %d returned %p, "
			      "which is not aligned to 4", size, allocated);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	*mem = allocated;
	return 0;
}

int sceGxmShaderPatcherRegisterProgram(SceGxmShaderPatcher *shaderPatcher,
				       const SceGxmProgram *programHeader,
				       SceGxmShaderPatcherId *programId)
{
	SceGxmRegisteredProgram *registered, *list, *prev;
	SceSize size;
	void *mem;
	int err;

	if (shaderPatcher == NULL) {
		GXM_DBG_ERROR_NULL(797, sceGxmShaderPatcherRegisterProgram, shaderPatcher);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (programHeader == NULL) {
		GXM_DBG_ERROR(801, "Parameter program to function sceGxmShaderPatcherRegisterProgram "
			      "is NULL.");
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (programId == NULL) {
		GXM_DBG_ERROR_NULL(805, sceGxmShaderPatcherRegisterProgram, programId);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	err = sceGxmProgramCheck(programHeader);
	if (err != 0)
		return err;
#if SCE_DBG_ASSERTS_ENABLED
	/* a program registered twice gets two IDs whose programs cannot be shared */
	list = shaderPatcher->programs;
	for (registered = list->prev; registered != list; registered = registered->prev) {
		if (registered->programHeader == programHeader) {
			if (gxmWarningIsEnabled(SCE_GXM_WARNING_PROGRAM_REGISTERED_WITH_SHADER_PATCHER))
				GXM_DBG_WARNING(822, "The program at %p is being registered multiple "
						"times with the shader patcher.  Vertex and fragment "
						"programs cannot be shared between these separate IDs.",
						programHeader);
			break;
		}
	}
#endif
	err = gxmPatcherHostAlloc(shaderPatcher, sizeof(*registered), &mem);
	if (err != 0)
		return err;
	registered = mem;
	registered->programHeader = programHeader;

	/* the literals, copied from the program */
	if (programHeader->literalBufferCount == 0) {
		registered->literals = NULL;
	} else {
		size = programHeader->literalBufferCount << 2;
		err = gxmPatcherBufferAllocChecked(shaderPatcher, size, &registered->literals);
		if (err != 0)
			goto freeRegistered;
		sceClibMemcpy_safe(registered->literals,
				   GXM_RELATIVE(programHeader->literalBufferOffset), size);
		SceGxmInternalForTest_631E95AA(registered->literals, size, 0, 0, "Literal buffer");
	}

	/* scratch and per-thread buffers */
	if (programHeader->scratchBufferCount == 0) {
		registered->scratch = NULL;
	} else {
		err = gxmPatcherBufferAllocChecked(shaderPatcher,
						   programHeader->scratchBufferCount << 2,
						   &registered->scratch);
		if (err != 0)
			goto freeLiterals;
	}
	if (programHeader->threadBufferCount == 0) {
		registered->thread = NULL;
	} else {
		err = gxmPatcherBufferAllocChecked(shaderPatcher,
						   programHeader->threadBufferCount << 2,
						   &registered->thread);
		if (err != 0)
			goto freeScratch;
	}

	shaderPatcher->bufferMemAllocated += gxmProgramBufferWords(programHeader) * 4;
	registered->patcher = shaderPatcher;
	registered->vertexPrograms = NULL;
	registered->fragmentPrograms = NULL;
	list = shaderPatcher->programs;
	prev = list->prev;
	registered->next = list;
	registered->prev = prev;
	list->prev = registered;
	prev->next = registered;
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
	if (shaderPatcher == NULL) {
		GXM_DBG_ERROR_NULL(876, sceGxmShaderPatcherUnregisterProgram, shaderPatcher);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (programId == NULL) {
		GXM_DBG_ERROR_NULL(880, sceGxmShaderPatcherUnregisterProgram, programId);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (shaderPatcher != programId->patcher) {
		GXM_DBG_ERROR_NOT_CREATED(885, sceGxmShaderPatcherUnregisterProgram, programId);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	return gxmPatcherFreeRegistered(shaderPatcher, programId);
}

/* release every program made from it first */
int sceGxmShaderPatcherForceUnregisterProgram(SceGxmShaderPatcher *shaderPatcher,
					      SceGxmShaderPatcherId programId)
{
	SceGxmVertexProgram *vertexProgram;
	SceGxmFragmentProgram *fragmentProgram;

	if (shaderPatcher == NULL) {
		GXM_DBG_ERROR_NULL(901, sceGxmShaderPatcherForceUnregisterProgram, shaderPatcher);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (programId == NULL) {
		GXM_DBG_ERROR_NULL(905, sceGxmShaderPatcherForceUnregisterProgram, programId);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (shaderPatcher != programId->patcher) {
		GXM_DBG_ERROR_NOT_CREATED(910, sceGxmShaderPatcherForceUnregisterProgram, programId);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	while ((vertexProgram = programId->vertexPrograms) != NULL) {
		programId->vertexPrograms = vertexProgram->next;
		shaderPatcher->hostMemAllocated -= vertexProgram->hostSize;
		shaderPatcher->vertexUsseMemAllocated -= vertexProgram->usseSize;
#if SCE_DBG_ASSERTS_ENABLED
		vertexProgram->program = NULL;
		vertexProgram->registered = NULL;
#endif
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
#if SCE_DBG_ASSERTS_ENABLED
			fragmentProgram->program = NULL;
			fragmentProgram->registered = NULL;
#endif
			if (fragmentProgram->usseMem != NULL)
				shaderPatcher->fragmentUsseFree(shaderPatcher->fragmentUsseUserData,
								fragmentProgram->usseMem);
			shaderPatcher->hostFree(shaderPatcher->userData, fragmentProgram);
			fragmentProgram = programId->fragmentPrograms;
		} while (fragmentProgram != NULL);
	}
	return gxmPatcherFreeRegistered(shaderPatcher, programId);
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
	SceGxmVertexProgram *cachedVertexProgram = vertexProgram;

	if (shaderPatcher == NULL) {
		GXM_DBG_ERROR_NULL(1893, sceGxmShaderPatcherAddRefVertexProgram, shaderPatcher);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (vertexProgram == NULL) {
		GXM_DBG_ERROR_NULL(1897, sceGxmShaderPatcherAddRefVertexProgram, vertexProgram);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	GXM_DBG_ASSERT(1899, vertexProgram->program);
	GXM_DBG_ASSERT(1900, sceGxmProgramGetProgramType(vertexProgram->program) == SCE_GXM_VERTEX_PROGRAM);
	if (shaderPatcher != vertexProgram->patcher) {
		GXM_DBG_ERROR_NOT_CREATED(1907, sceGxmShaderPatcherAddRefVertexProgram, vertexProgram);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	GXM_DBG_ASSERT(1909, cachedVertexProgram->refCount != 0);
	cachedVertexProgram->refCount++;
	return 0;
}

int sceGxmShaderPatcherAddRefFragmentProgram(SceGxmShaderPatcher *shaderPatcher,
                                             SceGxmFragmentProgram *fragmentProgram)
{
	SceGxmFragmentProgram *cachedFragmentProgram = fragmentProgram;

	if (shaderPatcher == NULL) {
		GXM_DBG_ERROR_NULL(1921, sceGxmShaderPatcherAddRefFragmentProgram, shaderPatcher);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (fragmentProgram == NULL) {
		GXM_DBG_ERROR_NULL(1925, sceGxmShaderPatcherAddRefFragmentProgram, fragmentProgram);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	GXM_DBG_ASSERT(1927, fragmentProgram->program);
	GXM_DBG_ASSERT(1928, sceGxmProgramGetProgramType(fragmentProgram->program) == SCE_GXM_FRAGMENT_PROGRAM);
	if (shaderPatcher != fragmentProgram->patcher) {
		GXM_DBG_ERROR_NOT_CREATED(1935, sceGxmShaderPatcherAddRefFragmentProgram, fragmentProgram);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	GXM_DBG_ASSERT(1937, cachedFragmentProgram->refCount != 0);
	cachedFragmentProgram->refCount++;
	return 0;
}

int sceGxmShaderPatcherReleaseVertexProgram(SceGxmShaderPatcher *shaderPatcher,
                                            SceGxmVertexProgram *vertexProgram)
{
	SceGxmVertexProgram *cachedVertexProgram = vertexProgram;
	SceGxmVertexProgram *prev, *cur;

	if (shaderPatcher == NULL) {
		GXM_DBG_ERROR_NULL(1949, sceGxmShaderPatcherReleaseVertexProgram, shaderPatcher);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (vertexProgram == NULL) {
		GXM_DBG_ERROR_NULL(1953, sceGxmShaderPatcherReleaseVertexProgram, vertexProgram);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	GXM_DBG_ASSERT(1955, vertexProgram->program);
	GXM_DBG_ASSERT(1956, sceGxmProgramGetProgramType(vertexProgram->program) == SCE_GXM_VERTEX_PROGRAM);
	if (shaderPatcher != vertexProgram->patcher) {
		GXM_DBG_ERROR_NOT_CREATED(1963, sceGxmShaderPatcherReleaseVertexProgram, vertexProgram);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	GXM_DBG_ASSERT_MSG(1966, cachedVertexProgram->registered != NULL,
			   "vertex program has already been destroyed");
	GXM_DBG_ASSERT(1969, cachedVertexProgram->refCount > 0);
	if (--cachedVertexProgram->refCount != 0)
		return 0;

	/* unlink it from its registered program */
	prev = cachedVertexProgram->registered->vertexPrograms;
	if (cachedVertexProgram == prev) {
		cachedVertexProgram->registered->vertexPrograms = cachedVertexProgram->next;
	} else {
		if (prev == NULL) {
			GXM_DBG_FAIL(1982, "tried to remove cached vertex program from empty list");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		for (cur = prev->next; cachedVertexProgram != cur; cur = cur->next) {
			if (cur == NULL) {
				GXM_DBG_FAIL(1988, "cached vertex program not found in list");
				return SCE_GXM_ERROR_INVALID_VALUE;
			}
			prev = cur;
		}
		prev->next = cachedVertexProgram->next;
	}
	shaderPatcher->hostMemAllocated -= cachedVertexProgram->hostSize;
	shaderPatcher->vertexUsseMemAllocated -= cachedVertexProgram->usseSize;
#if SCE_DBG_ASSERTS_ENABLED
	cachedVertexProgram->program = NULL;
	cachedVertexProgram->registered = NULL;
#endif
	shaderPatcher->vertexUsseFree(shaderPatcher->vertexUsseUserData, cachedVertexProgram->usseMem);
	shaderPatcher->hostFree(shaderPatcher->userData, cachedVertexProgram);
	return 0;
}

int sceGxmShaderPatcherReleaseFragmentProgram(SceGxmShaderPatcher *shaderPatcher,
                                              SceGxmFragmentProgram *fragmentProgram)
{
	SceGxmFragmentProgram *cachedFragmentProgram = fragmentProgram;
	SceGxmFragmentProgram *prev, *cur;

	if (shaderPatcher == NULL) {
		GXM_DBG_ERROR_NULL(2011, sceGxmShaderPatcherReleaseFragmentProgram, shaderPatcher);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (fragmentProgram == NULL) {
		GXM_DBG_ERROR_NULL(2015, sceGxmShaderPatcherReleaseFragmentProgram, fragmentProgram);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	GXM_DBG_ASSERT(2017, fragmentProgram->program);
	GXM_DBG_ASSERT(2018, sceGxmProgramGetProgramType(fragmentProgram->program) == SCE_GXM_FRAGMENT_PROGRAM);
	if (shaderPatcher != fragmentProgram->patcher) {
		GXM_DBG_ERROR_NOT_CREATED(2025, sceGxmShaderPatcherReleaseFragmentProgram, fragmentProgram);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	GXM_DBG_ASSERT_MSG(2028, cachedFragmentProgram->registered != NULL,
			   "fragment program has already been destroyed");
	GXM_DBG_ASSERT(2031, cachedFragmentProgram->refCount > 0);
	if (--cachedFragmentProgram->refCount != 0)
		return 0;

	/* unlink it from its registered program */
	prev = cachedFragmentProgram->registered->fragmentPrograms;
	if (cachedFragmentProgram == prev) {
		cachedFragmentProgram->registered->fragmentPrograms = cachedFragmentProgram->next;
	} else {
		if (prev == NULL) {
			GXM_DBG_FAIL(2044, "tried to remove cached fragment program from empty list");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		for (cur = prev->next; cachedFragmentProgram != cur; cur = cur->next) {
			if (cur == NULL) {
				GXM_DBG_FAIL(2050, "cached fragment program not found in list");
				return SCE_GXM_ERROR_INVALID_VALUE;
			}
			prev = cur;
		}
		prev->next = cachedFragmentProgram->next;
	}
	shaderPatcher->hostMemAllocated -= cachedFragmentProgram->hostSize;
	shaderPatcher->fragmentUsseMemAllocated -= cachedFragmentProgram->usseSize;
#if SCE_DBG_ASSERTS_ENABLED
	cachedFragmentProgram->program = NULL;
	cachedFragmentProgram->registered = NULL;
#endif
	if (cachedFragmentProgram->usseMem != NULL)
		shaderPatcher->fragmentUsseFree(shaderPatcher->fragmentUsseUserData,
						cachedFragmentProgram->usseMem);
	shaderPatcher->hostFree(shaderPatcher->userData, cachedFragmentProgram);
	return 0;
}

int sceGxmShaderPatcherGetVertexProgramRefCount(const SceGxmShaderPatcher *shaderPatcher,
                                                const SceGxmVertexProgram *vertexProgram,
                                                SceUInt32 *refCount)
{
	if (shaderPatcher == NULL) {
		GXM_DBG_ERROR_NULL(2074, sceGxmShaderPatcherGetVertexProgramRefCount, shaderPatcher);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (vertexProgram == NULL) {
		GXM_DBG_ERROR_NULL(2078, sceGxmShaderPatcherGetVertexProgramRefCount, vertexProgram);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (refCount == NULL) {
		GXM_DBG_ERROR_NULL(2082, sceGxmShaderPatcherGetVertexProgramRefCount, refCount);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	GXM_DBG_ASSERT(2084, vertexProgram->program);
	GXM_DBG_ASSERT(2085, sceGxmProgramGetProgramType(vertexProgram->program) == SCE_GXM_VERTEX_PROGRAM);
	if (shaderPatcher != vertexProgram->patcher) {
		GXM_DBG_ERROR_NOT_CREATED(2092, sceGxmShaderPatcherGetVertexProgramRefCount, vertexProgram);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	*refCount = vertexProgram->refCount;
	return 0;
}

int sceGxmShaderPatcherGetFragmentProgramRefCount(const SceGxmShaderPatcher *shaderPatcher,
                                                  const SceGxmFragmentProgram *fragmentProgram,
                                                  SceUInt32 *refCount)
{
	if (shaderPatcher == NULL) {
		GXM_DBG_ERROR_NULL(2109, sceGxmShaderPatcherGetFragmentProgramRefCount, shaderPatcher);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (fragmentProgram == NULL) {
		GXM_DBG_ERROR_NULL(2113, sceGxmShaderPatcherGetFragmentProgramRefCount, fragmentProgram);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (refCount == NULL) {
		GXM_DBG_ERROR_NULL(2117, sceGxmShaderPatcherGetFragmentProgramRefCount, refCount);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	GXM_DBG_ASSERT(2119, fragmentProgram->program);
	GXM_DBG_ASSERT(2120, sceGxmProgramGetProgramType(fragmentProgram->program) == SCE_GXM_FRAGMENT_PROGRAM);
	if (shaderPatcher != fragmentProgram->patcher) {
		GXM_DBG_ERROR_NOT_CREATED(2127, sceGxmShaderPatcherGetFragmentProgramRefCount, fragmentProgram);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	*refCount = fragmentProgram->refCount;
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

	if (streamCount > GXM_MAX_STREAMS) {
		GXM_DBG_ERROR(1017, "Parameter streamCount to function sceGxmShaderPatcherCreateVertexProgram is %d, which is "
			      "greater than the maximum value of %d.", streamCount, GXM_MAX_STREAMS);
		return 0;
	}
	for (i = 0; i < streamCount; i++) {
		if (streams[i].indexSource > SCE_GXM_INDEX_SOURCE_MAX) {
			GXM_DBG_ERROR(1029, "Parameter streams[%d].indexSource to function sceGxmShaderPatcherCreateVertexProgram is "
				      "%d, which is not valid.", i, streams[i].indexSource);
			return 0;
		}
	}
	if (attributeCount > GXM_MAX_ATTRIBUTES) {
		GXM_DBG_ERROR(1038, "Parameter attributeCount to function sceGxmShaderPatcherCreateVertexProgram is %d, which is "
			      "greater than the maximum value of %d.", attributeCount,
			      GXM_MAX_ATTRIBUTES);
		return 0;
	}
	for (i = 0; i < attributeCount; i++) {
		if (attributes[i].streamIndex >= streamCount) {
			GXM_DBG_ERROR(1051, "Parameter attributes[%d].streamIndex is %d, which is not "
				      "less than the stream count of %d.", i, attributes[i].streamIndex,
				      streamCount);
			return 0;
		}
		if (attributes[i].format > SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED_REG) {
			GXM_DBG_ERROR(1059, "Parameter attributes[%d].format is %d, which is not valid.",
				      i, attributes[i].format);
			return 0;
		}
		components = attributes[i].componentCount - 1;
		if (attributes[i].format == SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED_REG) {
			if (components > 63) {
				GXM_DBG_ERROR(1069, "Parameter attributes[%d].componentCount has value "
					      "%d, which is not in the range [1, 64].", i,
					      attributes[i].componentCount);
				return 0;
			}
		} else if (components > 3) {
			GXM_DBG_ERROR(1077, "Parameter attributes[%d].componentCount has value %d, "
				      "which is not in the range [1, 4].", i,
				      attributes[i].componentCount);
			return 0;
		}
		if (attributes[i].regIndex > 63) {
			GXM_DBG_ERROR(1086, "Parameter attributes[%d].regIndex has value %d, which is "
				      "not in the range [0, 63].", i, attributes[i].regIndex);
			return 0;
		}
		if (attributes[i].regIndex + attributes[i].componentCount > 64) {
			GXM_DBG_ERROR(1091, "Parameter attributes[%d] would write to registers outside "
				      "of the range [0, 63].", i);
			return 0;
		}
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
	void *usseMem, *mem;
	int err;

	if (shaderPatcher == NULL || programId == NULL || vertexProgram == NULL) {
		if (shaderPatcher == NULL)
			GXM_DBG_ERROR_NULL(958, sceGxmShaderPatcherCreateVertexProgram, shaderPatcher);
		else if (programId == NULL)
			GXM_DBG_ERROR_NULL(962, sceGxmShaderPatcherCreateVertexProgram, programId);
		else
			GXM_DBG_ERROR_NULL(966, sceGxmShaderPatcherCreateVertexProgram, vertexProgram);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (shaderPatcher != programId->patcher) {
		GXM_DBG_ERROR_NOT_CREATED(971, sceGxmShaderPatcherCreateVertexProgram, programId);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (programId->programHeader->programFlags & GXM_PROGRAM_FRAGMENT) {
		GXM_DBG_ERROR(975, "Parameter programId to function sceGxmShaderPatcherCreateVertexProgram is not an ID for a "
			      "vertex program.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (attributeCount == 0) {
		if (attributes != NULL &&
		    gxmWarningIsEnabled(SCE_GXM_WARNING_ATTRIBUTES_PROVIDED_WITH_ZERO_COUNT))
			GXM_DBG_WARNING(986, "Parameter attributes to function sceGxmShaderPatcherCreateVertexProgram is non-NULL, "
					"but attributeCount is 0.");
		if (streamCount != 0) {
			GXM_DBG_ERROR(991, "Parameter attributeCount to function sceGxmShaderPatcherCreateVertexProgram is 0, but "
				      "streamCount is %d.", streamCount);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	} else if (attributes == NULL) {
		GXM_DBG_ERROR(981, "Parameter attributes to function sceGxmShaderPatcherCreateVertexProgram is NULL, but parameter "
			      "attributeCount is %d.", attributeCount);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (streamCount == 0) {
		if (streams != NULL &&
		    gxmWarningIsEnabled(SCE_GXM_WARNING_STREAMS_PROVIDED_WITH_ZERO_COUNT))
			GXM_DBG_WARNING(1003, "Parameter streams to function sceGxmShaderPatcherCreateVertexProgram is non-NULL, but "
					"streamCount is 0.");
		if (attributeCount != 0) {
			GXM_DBG_ERROR(1008, "Parameter streamCount to function sceGxmShaderPatcherCreateVertexProgram is 0, but "
				      "attributeCount is %d.", attributeCount);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	} else if (streams == NULL) {
		GXM_DBG_ERROR(998, "Parameter streams to function sceGxmShaderPatcherCreateVertexProgram is NULL, when parameter "
			      "streamCount is %d.", streamCount);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_VERTEX_LAYOUT_ANY) &&
	    !gxmVertexLayoutValid(attributes, attributeCount, streams, streamCount))
		return SCE_GXM_ERROR_INVALID_VALUE;

	created = gxmFindVertexProgram(programId, attributes, attributeCount, streams, streamCount);
	if (created != NULL)
		goto done;
	program = programId->programHeader;

	/* one host block: the program, the layout copies and the PDS program */
	instanced = (program->programFlags & GXM_PROGRAM_INSTANCED) >> 17;
	indexed = (program->programFlags & GXM_PROGRAM_INDEXED) >> 18;
	pdsSize = gxmPdsVertexProgramSize(attributeCount, streams, streamCount, NULL, instanced,
					  indexed) + 15;
	hostSize = pdsSize + (streamCount + 30 + attributeCount * 4) * 4;
	err = gxmPatcherHostAlloc(shaderPatcher, hostSize, &mem);
	if (err != 0)
		return err;
	created = mem;
	created->attributes = (SceGxmVertexAttribute *)(created + 1);
	created->streams = (SceGxmVertexStream *)(created->attributes + attributeCount);
	patches = (GxmPdsAttributePatch *)(created->streams + streamCount);
	gxmChunkBufferInitFixed(&pdsBuffer, patches + attributeCount, 0, pdsSize);
	varyings = gxmProgramGetVertexVaryings(program);

	/* the USSE code */
	codeSize = gxmProgramGetVertexCodeSize(program, attributes, attributeCount);
	usseOffset = 0;
	usseSize = ((codeSize - 1 + gxmUsseAlignment()) & -gxmUsseAlignment()) +
		   gxmProgramGetSecondaryCodeSize(program);
	usseMem = shaderPatcher->vertexUsseAlloc(shaderPatcher->vertexUsseUserData, usseSize,
						 &usseOffset);
	if (usseMem == NULL) {
		err = SCE_GXM_ERROR_OUT_OF_VERTEX_USSE_MEMORY;
		GXM_DBG_ERROR(125, "Shader patcher vertex USSE memory allocation returned NULL for "
			      "allocation of size %d", usseSize);
		goto freeHost;
	}
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_PATCHER_UNALIGNED) && ((SceUInt32)usseMem & 0xf)) {
		err = SCE_GXM_ERROR_INVALID_ALIGNMENT;
		shaderPatcher->vertexUsseFree(shaderPatcher->vertexUsseUserData, usseMem);
		GXM_DBG_ERROR(138, "Shader patcher vertex USSE memory allocation of size %d returned "
			      "%p, which is not aligned to %d", usseSize, usseMem, 16);
		goto freeHost;
	}
	bank = usseOffset >> 24;
	if (bank != 8 && bank > 3) {
		err = SCE_GXM_ERROR_INVALID_POINTER;
		shaderPatcher->vertexUsseFree(shaderPatcher->vertexUsseUserData, usseMem);
		GXM_DBG_ERROR(149, "Shader patcher vertex USSE memory allocation returned an invalid "
			      "vertex USSE offset.");
		goto freeHost;
	}
	created->usseMem = usseMem;
	gxmChunkBufferInitFixed(&codeBuffer, usseMem, usseOffset, usseSize);
	created->usseSize = usseSize;
	created->hostSize = hostSize;
	created->program = program;
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
			 varyings->vertex.outputs >> 24, (program->programFlags & GXM_PROGRAM_PHASED) >> 20,
			 &created->dms);
	created->attributeCount = attributeCount;
	created->streamCount = streamCount;
	sceClibMemcpy_safe(created->attributes, attributes, attributeCount * sizeof(*attributes));
	sceClibMemcpy_safe(created->streams, streams, streamCount * sizeof(*streams));
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
	outputs = gxmProgramGetVertexVaryings(vertexProgram)->vertex.texCoordOutputs;
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

#if SCE_DBG_ASSERTS_ENABLED
static const char *const gxmInterpolantNames[] = {
	"FOG", "COLOR0", "COLOR1",
	"TEXCOORD0", "TEXCOORD1", "TEXCOORD2", "TEXCOORD3", "TEXCOORD4",
	"TEXCOORD5", "TEXCOORD6", "TEXCOORD7", "TEXCOORD8", "TEXCOORD9",
};

/* an interpolant's bit in gxmInterpolantNames' order, by a fragment input's selector */
static inline SceUInt32 gxmInterpolantBit(SceUInt32 selector)
{
	switch (selector) {
	case 0xa:
		return 1 << 1;		/* COLOR0 */
	case 0xb:
		return 1 << 2;		/* COLOR1 */
	case 0xc:
		return 1 << 0;		/* FOG */
	default:
		return selector < 10 ? 1 << (3 + selector) : 0;
	}
}

/* the debug build warns about interpolants the vertex program does not write */
static inline void gxmCheckInterpolants(const SceGxmProgram *program,
					const SceGxmProgram *vertexProgram)
{
	const GxmProgramVaryings *varyings;
	const GxmProgramFragmentInput *input, *end;
	SceUInt32 written, used, word, i;

	varyings = gxmProgramGetVaryings(vertexProgram);
	written = 0;
	if (varyings->vertex.outputs & 0x200)
		written |= 1 << 0;
	if (varyings->vertex.outputs & 0x800)
		written |= 1 << 1;
	if (varyings->vertex.outputs & 0x400)
		written |= 1 << 2;
	for (i = 0; i < 10; i++) {
		if ((varyings->vertex.texCoordOutputs >> (i * 3)) & 7)
			written |= 1 << (3 + i);
	}

	varyings = gxmProgramGetVaryings(program);
	input = GXM_RELATIVE(varyings->fragment.inputsOffset);
	end = input + varyings->fragment.inputCount;
	used = 0;
	for (; input != end; input++) {
		word = input->word;
		if ((word & 0xf) != 0xf && !(word & 0x400))
			used |= gxmInterpolantBit(word & 0xf);
		if ((word & 0xf000) != 0xf000 && !(word & 0x40000000))
			used |= gxmInterpolantBit((word & 0xf000) >> 12);
	}

	if (used == written)
		return;
	for (i = 0; i < 13; i++) {
		if (((used >> i) & 1) && !((written >> i) & 1) &&
		    gxmWarningIsEnabled(SCE_GXM_WARNING_USING_INTERPOLANT_NOT_WRITTEN_BY_VERTEX_PROGRAM))
			GXM_DBG_WARNING(1407, "Fragment program at %p is using interpolant %s, but "
					"vertex program at %p is not writing it.", program,
					gxmInterpolantNames[i], vertexProgram);
	}
}
#endif

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

	if (shaderPatcher == NULL || programId == NULL || fragmentProgram == NULL) {
		if (shaderPatcher == NULL)
			GXM_DBG_ERROR_NULL(1457, sceGxmShaderPatcherCreateFragmentProgram, shaderPatcher);
		else if (programId == NULL)
			GXM_DBG_ERROR_NULL(1461, sceGxmShaderPatcherCreateFragmentProgram, programId);
		else
			GXM_DBG_ERROR_NULL(1465, sceGxmShaderPatcherCreateFragmentProgram, fragmentProgram);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (shaderPatcher != programId->patcher) {
		GXM_DBG_ERROR_NOT_CREATED(1470, sceGxmShaderPatcherCreateFragmentProgram, programId);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	program = programId->programHeader;
	flags = program->programFlags;
	if (!(flags & GXM_PROGRAM_FRAGMENT)) {
		GXM_DBG_ERROR(1474, "Parameter programId to function sceGxmShaderPatcherCreateFragmentProgram is not an ID for a "
			      "fragment program.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_SURFACE_ANY)) {
		if (outputFormat > 8) {
			GXM_DBG_ERROR(1489, "Parameter outputFormat to function sceGxmShaderPatcherCreateFragmentProgram is not valid.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (multisampleMode > SCE_GXM_MULTISAMPLE_4X) {
			GXM_DBG_ERROR(1496, "Parameter multisampleMode to function sceGxmShaderPatcherCreateFragmentProgram is not valid.");
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	}

	/* how the program writes its output */
	if (flags & GXM_PROGRAM_NATIVE_COLOR) {
		if (blendInfo != NULL &&
		    gxmWarningIsEnabled(SCE_GXM_WARNING_BLEND_INFO_IGNORED_FOR_NATIVECOLOR)) {
			GXM_DBG_WARNING(1515, "Ignoring non-NULL SceGxmBlendInfo for fragment program "
					"that uses __nativecolor.");
			flags = program->programFlags;
		}
		if (flags & GXM_PROGRAM_FRAG_COLOR) {
			if (multisampleMode == SCE_GXM_MULTISAMPLE_2X) {
				if (!(flags & (GXM_PROGRAM_MSAA_2X | GXM_PROGRAM_MSAA_4X))) {
					GXM_DBG_ERROR(1539, "Cannot create 2xMSAA fragment program from "
						      "program compiled with non-MSAA programmable "
						      "blending.");
					return SCE_GXM_ERROR_INVALID_VALUE;
				}
			} else if (multisampleMode == SCE_GXM_MULTISAMPLE_NONE) {
				if (flags & (GXM_PROGRAM_MSAA_2X | GXM_PROGRAM_MSAA_4X)) {
					GXM_DBG_ERROR(1531, "Cannot create non-MSAA fragment program from "
						      "program compiled with MSAA programmable "
						      "blending.");
					return SCE_GXM_ERROR_INVALID_VALUE;
				}
			} else if (multisampleMode == SCE_GXM_MULTISAMPLE_4X) {
				if (!(flags & GXM_PROGRAM_MSAA_4X)) {
					GXM_DBG_ERROR(1547, "Cannot create 4xMSAA fragment program from "
						      "program compiled with non-MSAA or 2xMSAA "
						      "programmable blending.");
					return SCE_GXM_ERROR_INVALID_VALUE;
				}
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
	} else if (flags & SCE_GXM_PROGRAM_FLAG_OUTPUT_UNDEFINED) {
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
#if SCE_DBG_ASSERTS_ENABLED
		if (vertexProgram != NULL)
			gxmCheckInterpolants(program, vertexProgram);
#endif
	} else {
		gxmTexCoordRemap(vertexRemap, vertexProgram);
		for (i = 0; i < 10; i++) {
			if (texCoordMap[i] >= 10) {
				GXM_DBG_ERROR(1380, "Fragment program texture coordinate %d is remapped "
					      "to index %d, which is not in the range [0, %d].", i,
					      texCoordMap[i], 9);
				return SCE_GXM_ERROR_INVALID_VALUE;
			}
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
	varyings = gxmProgramGetFragmentVaryings(program);
	inputs = GXM_RELATIVE(varyings->fragment.inputsOffset);
	pdsSize = gxmPdsFragmentProgramSize(inputs, varyings->fragment.inputCount, 0) + 15;
	hostSize = pdsSize + (varyings->fragment.textureIndexCount + 32) * 4 +
		   (blendInfo != NULL ? 4 : 0);
	err = gxmPatcherHostAlloc(shaderPatcher, hostSize, &tail);
	if (err != 0)
		return err;
	created = tail;
	tail = (SceUInt32 *)created + varyings->fragment.textureIndexCount + 32;
	if (blendInfo == NULL) {
		created->blendInfo = NULL;
	} else {
		created->blendInfo = tail;
		sceClibMemcpy_safe(tail, blendInfo, sizeof(*blendInfo));
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
		GXM_DBG_ERROR(176, "Shader patcher fragment USSE memory allocation returned NULL for "
			      "allocation of size %d", usseSize);
		goto freeHost;
	}
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_PATCHER_UNALIGNED) && ((SceUInt32)usseMem & 0xf)) {
		err = SCE_GXM_ERROR_INVALID_ALIGNMENT;
		shaderPatcher->fragmentUsseFree(shaderPatcher->fragmentUsseUserData, usseMem);
		GXM_DBG_ERROR(189, "Shader patcher fragment USSE memory allocation of size %d "
			      "returned %p, which is not aligned to %d", usseSize, usseMem, 16);
		goto freeHost;
	}
	bank = usseOffset >> 24;
	if (bank != 9 && bank - 4 > 3) {
		err = SCE_GXM_ERROR_INVALID_POINTER;
		shaderPatcher->fragmentUsseFree(shaderPatcher->fragmentUsseUserData, usseMem);
		GXM_DBG_ERROR(200, "Shader patcher fragment USSE memory allocation returned an "
			      "invalid fragment USSE offset.");
		goto freeHost;
	}
	created->usseMem = usseMem;
	gxmChunkBufferInitFixed(&codeBuffer, usseMem, usseOffset, usseSize);
	created->hostSize = hostSize;
	created->usseSize = usseSize;
	created->program = program;
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
	SceUInt8 varyings[0x14];	/* 0x11c: the program's fragment varyings */
} GxmMaskUpdateProgram;

GXM_ASSERT_SIZE(GxmMaskUpdateProgram, 0x130);

#define GXM_PROGRAM_MAGIC	0x00505847	/* "GXP" */

int sceGxmShaderPatcherCreateMaskUpdateFragmentProgram(SceGxmShaderPatcher *shaderPatcher,
						       SceGxmFragmentProgram **fragmentProgram)
{
	SceGxmRegisteredProgram *registered;
	GxmMaskUpdateProgram *created;
	GxmProgramVaryings *varyings;
	void *mem;
	int err;

	if (shaderPatcher == NULL) {
		GXM_DBG_ERROR_NULL(2168, sceGxmShaderPatcherCreateMaskUpdateFragmentProgram,
				   shaderPatcher);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (fragmentProgram == NULL) {
		GXM_DBG_ERROR_NULL(2172, sceGxmShaderPatcherCreateMaskUpdateFragmentProgram,
				   fragmentProgram);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	registered = shaderPatcher->programs;
	created = (GxmMaskUpdateProgram *)registered->fragmentPrograms;
	if (created == NULL) {
		err = gxmPatcherHostAlloc(shaderPatcher, sizeof(*created), &mem);
		if (err != 0)
			return err;
		created = mem;
		sceClibMemset(created, 0, sizeof(*created));
		created->program.magic = GXM_PROGRAM_MAGIC;
		created->program.programFlags = GXM_PROGRAM_FRAGMENT | GXM_PROGRAM_UNK80000;
		created->program.majorVersion = 1;
		created->program.minorVersion = 5;
		created->program.sdkVersion = FW_VERSION >> 16;
		created->program.size = sizeof(created->program) + sizeof(created->varyings);
		created->fragment.program = &created->program;
		created->program.varyingsOffset = (char *)created->varyings -
						  (char *)&created->program.varyingsOffset;
		created->program.reserved2 = 0;
		varyings = (GxmProgramVaryings *)created->varyings;
		varyings->fragment.outputRegisterType = SCE_GXM_PARAMETER_TYPE_U8;
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
