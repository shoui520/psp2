/*
 * paf::GraphMem: memory blocks the GPU maps, from the system's memory, the
 * video memory or a block another process shares.
 */
#ifndef PAF_GRAPHICS_GRAPHMEM_GRAPHMEM_H
#define PAF_GRAPHICS_GRAPHMEM_GRAPHMEM_H

#include <stddef.h>
#include <stdint.h>

namespace paf {
namespace GraphMem {

enum DeviceType {
	DeviceType_SystemMemory = 0,
	DeviceType_VideoMemory = 1,
	/* a block another process made, opened by its name */
	DeviceType_SharedMemory = 2
};

class Option {
public:
	enum MemoryType {
		MemoryType_Default = 0,
		MemoryType_Tool = 1,
		MemoryType_Phycont = 2
	};

	enum GpuMapType {
		GpuMapType_Default = 0,
		GpuMapType_VertexUsse = 1,
		GpuMapType_FragmentUsse = 2,
		GpuMapType_NoMap = 3
	};

	Option()
	{
		type = MemoryType_Default;
		memblock_option = NULL;
		gpu_map_type = GpuMapType_Default;
		gpu_map_attrib = 1 | 2;
		gpu_usse_offset = 0;
		report_error = false;
	}

	MemoryType type;
	/* SceKernelAllocMemBlockOpt, or the whole MemBlockUserOpt */
	void *memblock_option;
	GpuMapType gpu_map_type;
	/* SceGxmMemoryAttribFlags */
	uint32_t gpu_map_attrib;
	/* set by AllocOSMemory for the USSE map types */
	uint32_t gpu_usse_offset;
	/* each failing step calls on_error */
	bool report_error;
};

void *AllocOSMemory(DeviceType device_type, uint32_t size, const char *name, Option *option);
void FreeOSMemory(DeviceType device_type, void *p);

}
}

#endif
