/*
 * paf::GraphMem: memory blocks mapped for the GPU, each kept in a list with its memory block and
 * how it was mapped, so FreeOSMemory can unmap and free it by its address; and the system and
 * video memory heaps the graphics allocate from (the file's name ours, after the header).
 */
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <kernel/sysmem/memblock.h>
#include <sysmem/memblock.h>
#include <common/common_internal.h>
#include <graphics/graphmem/graphmem.h>
#include <std/list>
#include <std/stdlib.h>
#include <std/string>
#include <thread/thread.h>
#include <src/module.h>

extern "C" {
int sceGxmMapVertexUsseMemoryInternal(void *base, SceSize size, unsigned int *offset);
int sceGxmMapFragmentUsseMemoryInternal(void *base, SceSize size, unsigned int *offset);
int sceGxmUnmapVertexUsseMemoryInternal(void *base);
int sceGxmUnmapFragmentUsseMemoryInternal(void *base);
}

namespace paf {
namespace GraphMem {

/* a block AllocOSMemory made: its address, name, memory block and GPU mapping (names ours) */
class OSMemory {
public:
	~OSMemory()
	{
	}

	void *base;
	string name;
	SceUID uid;
	Option::GpuMapType gpu_map_type;
};

static void *s_system_memory;
static void *s_video_memory;
static ModeType s_mode;

static thread::Mutex s_mutex("ScePafGraphMem", 0);
static common::SharedPtr<GraphHeapAllocator> s_system_heap;
static common::SharedPtr<GraphHeapAllocator> s_video_heap;
static list<OSMemory> s_os_memory;

/* the memory block type of the system and the video memory */
static const SceKernelMemBlockType s_memblock_type[] = {
	SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW,
	SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW
};

GraphHeapAllocator::GraphHeapAllocator(void *membase, size_t size, const char *name)
	: memory::HeapAllocator(membase, size, name)
{
}

GraphHeapAllocator::~GraphHeapAllocator()
{
}

void SetMode(ModeType mode)
{
	s_mode = mode;
}

void *AllocOSMemory(DeviceType device_type, uint32_t size, const char *name, Option *option)
{
	if (size == 0)
		return NULL;

	thread::MutexLock lock(&s_mutex);

	void *base = NULL;
	SceUID uid;

	if (device_type == DeviceType_SharedMemory) {
		uid = sceKernelOpenMemBlock(name, MEMBLOCK_OPEN_RW);
	} else {
		SceKernelMemBlockType type = s_memblock_type[device_type];

		if (option != NULL) {
			if (option->type == Option::MemoryType_Tool)
				type = SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_TOOL_NC_RW;
			if (option->type == Option::MemoryType_Phycont)
				type = SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW;
		}
		if (type == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW)
			size = (size + 0x3ffff) & ~0x3ffff;
		else
			size = (size + 0xfff) & ~0xfff;
		if (s_mode == ModeType_CommonDialog && device_type == DeviceType_SystemMemory)
			type = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_NC_RW;
		uid = sceKernelAllocMemBlock(name, type, size,
					     option != NULL ?
						     (SceKernelAllocMemBlockOpt *)option->memblock_option :
						     NULL);
	}
	if (option != NULL && option->report_error && uid < 0)
		common_internal::on_error();
	if (uid < 0) {
		return NULL;
	}

	int32_t ret = sceKernelGetMemBlockBase(uid, &base);

	if (option != NULL && option->report_error && ret != 0)
		common_internal::on_error();
	if (ret != 0) {
		if (device_type == DeviceType_SharedMemory)
			sceKernelCloseMemBlock(uid);
		else
			sceKernelFreeMemBlock(uid);
		return NULL;
	}

	uint32_t gpu_map_type = option != NULL ? option->gpu_map_type : Option::GpuMapType_Default;
	uint32_t gpu_map_attrib = option != NULL ? option->gpu_map_attrib :
						     SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE;
	unsigned int offset = 0;

	switch (gpu_map_type) {
	case Option::GpuMapType_Default:
		ret = sceGxmMapMemory(base, size, (SceGxmMemoryAttribFlags)gpu_map_attrib);
		break;
	case Option::GpuMapType_VertexUsse:
		if (sce_paf_is_common_dialog_mode != 0)
			ret = sceGxmMapVertexUsseMemoryInternal(base, size, &offset);
		else
			ret = sceGxmMapVertexUsseMemory(base, size, &offset);
		break;
	case Option::GpuMapType_FragmentUsse:
		if (sce_paf_is_common_dialog_mode != 0)
			ret = sceGxmMapFragmentUsseMemoryInternal(base, size, &offset);
		else
			ret = sceGxmMapFragmentUsseMemory(base, size, &offset);
		break;
	default:
		break;
	}
	if (option != NULL && option->report_error && ret != 0)
		common_internal::on_error();
	if (ret != 0) {
		if (device_type == DeviceType_SharedMemory)
			sceKernelCloseMemBlock(uid);
		else
			sceKernelFreeMemBlock(uid);
		return NULL;
	}
	if (option != NULL)
		option->gpu_usse_offset = offset;

	OSMemory memory = { base, name, uid, (Option::GpuMapType)gpu_map_type };

	s_os_memory.push_back(memory);

	return base;
}

void Init(uint32_t system_heap_size, uint32_t video_heap_size)
{
	s_system_memory = NULL;
	if (system_heap_size != 0) {
		Option option;

		option.report_error = true;
		s_system_memory = AllocOSMemory(DeviceType_SystemMemory, system_heap_size,
						"ScePafGraphMemSystem", &option);
		if (s_system_memory == NULL)
			common_internal::on_error();
		if (s_system_memory != NULL)
			s_system_heap = common::SharedPtr<GraphHeapAllocator>(new GraphHeapAllocator(
				s_system_memory, system_heap_size, "ScePafGraphMemSystem"));
	}

	s_video_memory = NULL;
	if (video_heap_size != 0) {
		s_video_memory = AllocOSMemory(DeviceType_VideoMemory, video_heap_size,
					       "ScePafGraphMemVideo", NULL);
		if (s_video_memory == NULL)
			common_internal::on_error();
		if (s_video_memory != NULL)
			s_video_heap = common::SharedPtr<GraphHeapAllocator>(new GraphHeapAllocator(
				s_video_memory, video_heap_size, "ScePafGraphMemVideo"));
	}
}

void FreeOSMemory(DeviceType device_type, void *p)
{
	s_mutex.Lock();
	if (p == NULL) {
		s_mutex.Unlock();
		return;
	}

	list<OSMemory>::iterator it = s_os_memory.begin();

	for (; it != s_os_memory.end(); ++it) {
		if (it->base == p)
			break;
	}
	if (it == s_os_memory.end()) {
		s_mutex.Unlock();
		return;
	}

	switch ((uint32_t)it->gpu_map_type) {
	case Option::GpuMapType_Default:
		sceGxmUnmapMemory(p);
		break;
	case Option::GpuMapType_VertexUsse:
		if (sce_paf_is_common_dialog_mode != 0)
			sceGxmUnmapVertexUsseMemoryInternal(p);
		else
			sceGxmUnmapVertexUsseMemory(p);
		break;
	case Option::GpuMapType_FragmentUsse:
		if (sce_paf_is_common_dialog_mode != 0)
			sceGxmUnmapFragmentUsseMemoryInternal(p);
		else
			sceGxmUnmapFragmentUsseMemory(p);
		break;
	default:
		break;
	}
	if (device_type == DeviceType_SharedMemory)
		sceKernelCloseMemBlock(it->uid);
	else
		sceKernelFreeMemBlock(it->uid);
	s_os_memory.erase(it);
	s_mutex.Unlock();
}

void Exit()
{
	s_system_heap.reset();
	if (s_system_memory != NULL)
		FreeOSMemory(DeviceType_SystemMemory, s_system_memory);
	s_system_memory = NULL;

	s_video_heap.reset();
	if (s_video_memory != NULL)
		FreeOSMemory(DeviceType_VideoMemory, s_video_memory);
	s_video_memory = NULL;

	s_mode = ModeType_Normal;
}

common::WeakPtr<GraphHeapAllocator> SystemMemoryHeap()
{
	return s_system_heap;
}

common::WeakPtr<GraphHeapAllocator> VideoMemoryHeap()
{
	return s_video_heap;
}

}
}
