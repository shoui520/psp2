/*
 * SceGxm: mapping memory for the GPU.
 *
 * Plain memory goes into the GPU's address space; USSE code memory goes
 * into one of the vertex or fragment code heaps, where the shaders see it
 * through an offset.
 */

#include "gxm_internal.h"

#define GXM_MAP_FLAG_READ_WRITE		0x00000100
#define GXM_USSE_HEAP_SIZE		0x00800000

/* the USSE heaps as tools see them */
#define GXM_MEMORY_PLAIN		0
#define GXM_MEMORY_VERTEX_USSE		1
#define GXM_MEMORY_FRAGMENT_USSE	2
#define GXM_MEMORY_VERTEX_USSE_INTERNAL	3
#define GXM_MEMORY_FRAGMENT_USSE_INTERNAL	4

#define GXM_USSE_VERTEX			0
#define GXM_USSE_FRAGMENT		1
#define GXM_USSE_APPLICATION		0
#define GXM_USSE_INTERNAL		1

int sceGxmFinish(SceGxmContext *context);

static void gxmFinishContext(void *context)
{
	sceGxmFinish(context);
}

int sceGxmMapMemory(void *base, SceSize size, SceUInt32 attr)
{
	int err;

	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	if (base == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	err = SceGpuEs4User_56893E98(&gxmGlobals.top, gxmGlobals.top.memContext, 0, size, 0, base,
				     attr | GXM_MAP_FLAG_READ_WRITE, NULL);
	if (err != 0)
		return SCE_GXM_ERROR_DRIVER;
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.memoryMapped(GXM_MEMORY_PLAIN, base, (SceUInt32)base, size);
	return err;
}

int sceGxmUnmapMemory(void *base)
{
	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	if (base == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	/* older SDKs let the GPU finish every context first */
	if (gxmGlobals.compatFlags & GXM_COMPAT_FINISH_ON_UNMAP)
		gxmSlotTableForEach(&gxmGlobals.top.contexts, gxmFinishContext);
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.memoryUnmapped(GXM_MEMORY_PLAIN, base);
	if (SceGpuEs4User_AD20D1A0(&gxmGlobals.top, base, 0, 0) != 0)
		return SCE_GXM_ERROR_DRIVER;
	return 0;
}

#define SCE_GPU_ERROR_NOT_MAPPED	0x3a

int sceGxmCheckMemoryInternal(SceUInt32 a, SceUInt32 b, SceUInt32 c)
{
	int err;

	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	err = SceGpuEs4UserForVsh_3DC96093(&gxmGlobals.top, gxmGlobals.top.memContext, a, b, c);
	if (err == SCE_GPU_ERROR_NOT_MAPPED)
		return SCE_GXM_ERROR_INVALID_MAPPING;
	if (err != 0)
		return SCE_GXM_ERROR_DRIVER;
	return 0;
}

/*
 * A USSE offset keeps the low 23 bits of the position in the heap and,
 * from bit 24, the 8 MiB bank plus the heap's first bank.
 */
static inline int gxmMapUsseMemory(void *base, SceSize size, SceUInt32 *offset, SceUInt32 owner,
				   SceUInt32 kind, SceUInt32 firstBank, SceUInt32 type)
{
	SceUInt32 gpuAddress, position;

	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	if (base == NULL || offset == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_USSE_ANY_SIZE) && size > GXM_USSE_HEAP_SIZE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	gpuAddress = 0;
	if (SceGpuEs4User_56893E98(&gxmGlobals.top, gxmGlobals.top.memContext,
				   gxmGlobals.top.usse[owner].heap[kind], size, GXM_USSE_HEAP_SIZE, base, 1,
				   &gpuAddress) != 0)
		return SCE_GXM_ERROR_DRIVER;
	position = gpuAddress - gxmGlobals.top.usse[owner].base[kind];
	*offset = (position & 0x7fffff) | ((position >> 23) + firstBank) << 24;
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.memoryMapped(type, base, gpuAddress, size);
	return 0;
}

static inline int gxmUnmapUsseMemory(void *base, SceUInt32 owner, SceUInt32 kind, SceUInt32 type)
{
	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	if (base == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.memoryUnmapped(type, base);
	if (SceGpuEs4User_AD20D1A0(&gxmGlobals.top, base, gxmGlobals.top.usse[owner].heap[kind], 0) != 0)
		return SCE_GXM_ERROR_DRIVER;
	return 0;
}

int sceGxmMapVertexUsseMemory(void *base, SceSize size, SceUInt32 *offset)
{
	return gxmMapUsseMemory(base, size, offset, GXM_USSE_APPLICATION, GXM_USSE_VERTEX, 0,
				GXM_MEMORY_VERTEX_USSE);
}

int sceGxmUnmapVertexUsseMemory(void *base)
{
	return gxmUnmapUsseMemory(base, GXM_USSE_APPLICATION, GXM_USSE_VERTEX, GXM_MEMORY_VERTEX_USSE);
}

int sceGxmMapFragmentUsseMemory(void *base, SceSize size, SceUInt32 *offset)
{
	return gxmMapUsseMemory(base, size, offset, GXM_USSE_APPLICATION, GXM_USSE_FRAGMENT, 4,
				GXM_MEMORY_FRAGMENT_USSE);
}

int sceGxmUnmapFragmentUsseMemory(void *base)
{
	return gxmUnmapUsseMemory(base, GXM_USSE_APPLICATION, GXM_USSE_FRAGMENT, GXM_MEMORY_FRAGMENT_USSE);
}

int sceGxmMapVertexUsseMemoryInternal(void *base, SceSize size, SceUInt32 *offset)
{
	return gxmMapUsseMemory(base, size, offset, GXM_USSE_INTERNAL, GXM_USSE_VERTEX, 8,
				GXM_MEMORY_VERTEX_USSE_INTERNAL);
}

int sceGxmUnmapVertexUsseMemoryInternal(void *base)
{
	return gxmUnmapUsseMemory(base, GXM_USSE_INTERNAL, GXM_USSE_VERTEX,
				  GXM_MEMORY_VERTEX_USSE_INTERNAL);
}

int sceGxmMapFragmentUsseMemoryInternal(void *base, SceSize size, SceUInt32 *offset)
{
	return gxmMapUsseMemory(base, size, offset, GXM_USSE_INTERNAL, GXM_USSE_FRAGMENT, 9,
				GXM_MEMORY_FRAGMENT_USSE_INTERNAL);
}

int sceGxmUnmapFragmentUsseMemoryInternal(void *base)
{
	return gxmUnmapUsseMemory(base, GXM_USSE_INTERNAL, GXM_USSE_FRAGMENT,
				  GXM_MEMORY_FRAGMENT_USSE_INTERNAL);
}

int sceGxmAddRazorGpuCaptureBuffer(void *base, SceSize size)
{
	if (base == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (size == 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC)
		return 0;
	if (gxmInitCallbacks.addRazorGpuCaptureBuffer(base, size) != 0)
		return SCE_GXM_ERROR_RAZOR;
	return 0;
}

int sceGxmRemoveRazorGpuCaptureBuffer(void *base)
{
	if (base == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC)
		return 0;
	if (gxmInitCallbacks.removeRazorGpuCaptureBuffer(base) != 0)
		return SCE_GXM_ERROR_RAZOR;
	return 0;
}
