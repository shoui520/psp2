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

static void gxmFinishContext(void *context)
{
	sceGxmFinish(context);
}

int sceGxmMapMemory(void *base, SceSize size, SceUInt32 attr)
{
	int err;

	if (!gxmGlobals.initialized) {
		GXM_DBG_ERROR_UNINITIALIZED(41, sceGxmMapMemory);
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (base == NULL) {
		GXM_DBG_ERROR_NULL(47, sceGxmMapMemory, base);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	err = SceGpuEs4User_56893E98(&gxmGlobals.top, gxmGlobals.top.memContext, 0, size, 0, base,
				     attr | GXM_MAP_FLAG_READ_WRITE, NULL);
	if (err != 0) {
		GXM_DBG_ERROR(59, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.memoryMapped(GXM_MEMORY_PLAIN, base, (SceUInt32)base, size);
	return err;
}

int sceGxmUnmapMemory(void *base)
{
	int err;

	if (!gxmGlobals.initialized) {
		GXM_DBG_ERROR_UNINITIALIZED(81, sceGxmUnmapMemory);
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (base == NULL) {
		GXM_DBG_ERROR_NULL(87, sceGxmUnmapMemory, base);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	/* older SDKs let the GPU finish every context first */
	if (gxmGlobals.compatFlags & GXM_COMPAT_FINISH_ON_UNMAP)
		gxmSlotTableForEach(&gxmGlobals.top.contexts, gxmFinishContext);
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.memoryUnmapped(GXM_MEMORY_PLAIN, base);
	err = SceGpuEs4User_AD20D1A0(&gxmGlobals.top, base, 0, 0);
	if (err != 0) {
		GXM_DBG_ERROR(109, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	return 0;
}

#define SCE_GPU_ERROR_NOT_MAPPED	0x3a

int sceGxmCheckMemoryInternal(SceUInt32 a, SceUInt32 b, SceUInt32 c)
{
	int err;

	if (!gxmGlobals.initialized) {
		/* the debug build names it sceGxmCheckMappedMemory */
		GXM_DBG_ERROR_UNINITIALIZED(121, sceGxmCheckMappedMemory);
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
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
				   SceUInt32 kind, SceUInt32 firstBank, SceUInt32 type, int line)
{
	SceUInt32 gpuAddress, position;
	int err;

	gpuAddress = 0;
	err = SceGpuEs4User_56893E98(&gxmGlobals.top, gxmGlobals.top.memContext,
				     gxmGlobals.top.usse[owner].heap[kind], size, GXM_USSE_HEAP_SIZE, base,
				     1, &gpuAddress);
	if (err != 0) {
		GXM_DBG_ERROR(line, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	position = gpuAddress - gxmGlobals.top.usse[owner].base[kind];
	*offset = (position & 0x7fffff) | ((position >> 23) + firstBank) << 24;
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.memoryMapped(type, base, gpuAddress, size);
	return 0;
}

static inline int gxmUnmapUsseMemory(void *base, SceUInt32 owner, SceUInt32 kind, SceUInt32 type,
				     int line)
{
	int err;

	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.memoryUnmapped(type, base);
	err = SceGpuEs4User_AD20D1A0(&gxmGlobals.top, base, gxmGlobals.top.usse[owner].heap[kind], 0);
	if (err != 0) {
		GXM_DBG_ERROR(line, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	return 0;
}

/* each map function's checks, from line on */
#define GXM_CHECK_MAP_USSE_MEMORY(function, line)					\
	_SCE_MACRO_BEGIN								\
		if (!gxmGlobals.initialized) {						\
			GXM_DBG_ERROR_UNINITIALIZED(line, function);			\
			return SCE_GXM_ERROR_UNINITIALIZED;				\
		}									\
		if (base == NULL) {							\
			GXM_DBG_ERROR_NULL((line) + 6, function, base);			\
			return SCE_GXM_ERROR_INVALID_POINTER;				\
		}									\
		if (offset == NULL) {							\
			GXM_DBG_ERROR_NULL((line) + 10, function, offset);		\
			return SCE_GXM_ERROR_INVALID_POINTER;				\
		}									\
		if (!(gxmGlobals.compatFlags & GXM_COMPAT_USSE_ANY_SIZE) &&		\
		    size > GXM_USSE_HEAP_SIZE) {					\
			GXM_DBG_ERROR((line) + 18, "Parameter size to function " #function \
				      " has value %d, which larger than the maximum value of %d.", \
				      size, GXM_USSE_HEAP_SIZE);				\
			return SCE_GXM_ERROR_INVALID_VALUE;				\
		}									\
	_SCE_MACRO_END

/* each unmap function's checks, from line on */
#define GXM_CHECK_UNMAP_USSE_MEMORY(function, line)					\
	_SCE_MACRO_BEGIN								\
		if (!gxmGlobals.initialized) {						\
			GXM_DBG_ERROR_UNINITIALIZED(line, function);			\
			return SCE_GXM_ERROR_UNINITIALIZED;				\
		}									\
		if (base == NULL) {							\
			GXM_DBG_ERROR((line) + 6, "Parameter base to " #function " is NULL."); \
			return SCE_GXM_ERROR_INVALID_POINTER;				\
		}									\
	_SCE_MACRO_END

int sceGxmMapVertexUsseMemory(void *base, SceSize size, SceUInt32 *offset)
{
	GXM_CHECK_MAP_USSE_MEMORY(sceGxmMapVertexUsseMemory, 145);
	return gxmMapUsseMemory(base, size, offset, GXM_USSE_APPLICATION, GXM_USSE_VERTEX, 0,
				GXM_MEMORY_VERTEX_USSE, 177);
}

int sceGxmUnmapVertexUsseMemory(void *base)
{
	GXM_CHECK_UNMAP_USSE_MEMORY(sceGxmUnmapVertexUsseMemory, 205);
	return gxmUnmapUsseMemory(base, GXM_USSE_APPLICATION, GXM_USSE_VERTEX, GXM_MEMORY_VERTEX_USSE,
				  225);
}

int sceGxmMapFragmentUsseMemory(void *base, SceSize size, SceUInt32 *offset)
{
	GXM_CHECK_MAP_USSE_MEMORY(sceGxmMapFragmentUsseMemory, 237);
	return gxmMapUsseMemory(base, size, offset, GXM_USSE_APPLICATION, GXM_USSE_FRAGMENT, 4,
				GXM_MEMORY_FRAGMENT_USSE, 269);
}

int sceGxmUnmapFragmentUsseMemory(void *base)
{
	GXM_CHECK_UNMAP_USSE_MEMORY(sceGxmUnmapFragmentUsseMemory, 297);
	return gxmUnmapUsseMemory(base, GXM_USSE_APPLICATION, GXM_USSE_FRAGMENT,
				  GXM_MEMORY_FRAGMENT_USSE, 317);
}

int sceGxmMapVertexUsseMemoryInternal(void *base, SceSize size, SceUInt32 *offset)
{
	GXM_CHECK_MAP_USSE_MEMORY(sceGxmMapVertexUsseMemoryInternal, 329);
	return gxmMapUsseMemory(base, size, offset, GXM_USSE_INTERNAL, GXM_USSE_VERTEX, 8,
				GXM_MEMORY_VERTEX_USSE_INTERNAL, 361);
}

int sceGxmUnmapVertexUsseMemoryInternal(void *base)
{
	GXM_CHECK_UNMAP_USSE_MEMORY(sceGxmUnmapVertexUsseMemoryInternal, 389);
	return gxmUnmapUsseMemory(base, GXM_USSE_INTERNAL, GXM_USSE_VERTEX,
				  GXM_MEMORY_VERTEX_USSE_INTERNAL, 409);
}

int sceGxmMapFragmentUsseMemoryInternal(void *base, SceSize size, SceUInt32 *offset)
{
	GXM_CHECK_MAP_USSE_MEMORY(sceGxmMapFragmentUsseMemoryInternal, 421);
	return gxmMapUsseMemory(base, size, offset, GXM_USSE_INTERNAL, GXM_USSE_FRAGMENT, 9,
				GXM_MEMORY_FRAGMENT_USSE_INTERNAL, 453);
}

int sceGxmUnmapFragmentUsseMemoryInternal(void *base)
{
	GXM_CHECK_UNMAP_USSE_MEMORY(sceGxmUnmapFragmentUsseMemoryInternal, 481);
	return gxmUnmapUsseMemory(base, GXM_USSE_INTERNAL, GXM_USSE_FRAGMENT,
				  GXM_MEMORY_FRAGMENT_USSE_INTERNAL, 501);
}

int sceGxmAddRazorGpuCaptureBuffer(void *base, SceSize size)
{
	int err;

	if (base == NULL) {
		GXM_DBG_ERROR(512, "Parameter base to sceGxmAddRazorGpuCaptureBuffer is NULL.");
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (size == 0) {
		GXM_DBG_ERROR(516, "Parameter size to sceGxmAddRazorGpuCaptureBuffer must be greater "
			      "than 0.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC)
		return 0;
	err = gxmInitCallbacks.addRazorGpuCaptureBuffer(base, size);
	if (err != 0) {
		GXM_DBG_ERROR(526, "sceGxmAddRazorGpuCaptureBuffer failed in the Razor layer (0x%08X).",
			      err);
		return SCE_GXM_ERROR_RAZOR;
	}
	return 0;
}

int sceGxmRemoveRazorGpuCaptureBuffer(void *base)
{
	int err;

	if (base == NULL) {
		GXM_DBG_ERROR(538, "Parameter base to sceGxmRemoveRazorGpuCaptureBuffer is NULL.");
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC)
		return 0;
	err = gxmInitCallbacks.removeRazorGpuCaptureBuffer(base);
	if (err != 0) {
		GXM_DBG_ERROR(548, "sceGxmRemoveRazorGpuCaptureBuffer failed in the Razor layer "
			      "(0x%08X).", err);
		return SCE_GXM_ERROR_RAZOR;
	}
	return 0;
}
