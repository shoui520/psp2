/*
 * Exception hooks, the driver at +0xd000 of the image. A hooked vector slot
 * jumps to an entry of drivers/entry.S, which saves the state into the
 * taking core's frame and runs the hook on it. The frames are carved from
 * the memory region VK_MEMORY_EXCEPTION, an eighth for each core.
 *
 * The driver is not in the configuration's module table: its resume and
 * suspend do nothing, and the crash reporter (drivers/report.c) calls it.
 */
#include "exception.h"

/* the hooks' state, reached physically (0x4c bytes) */
typedef struct VkExceptionHooks {
	uint32_t frames[VNZ_CORES];		/* each core's frame: its top */
	VkExceptionHandler handlers[VK_EXCEPTIONS];
	uint8_t kinds[VNZ_CORES];		/* the exception each core is in */
} VkExceptionHooks;

#define VK_EXCEPTION_NONE	0xff
#define CACHE_LINE		64

#define VK_EXCEPTION_LOCAL	VK_SECTION(".exception.local")

VkExceptionHooks g_vkExceptionHooks VK_EXCEPTION_LOCAL;
extern uint32_t g_vkExceptionSaved[VK_EXCEPTIONS];
extern uint32_t g_vkExceptionJumps[VK_EXCEPTIONS];

void vkExceptionResume(VkExceptionState *state, uint32_t *target, uint32_t kind);

static void moduleResume(uint32_t primary, uint32_t resumed)
{
}

static void moduleSuspend(void)
{
}

const VkModule g_vkExceptionModule VK_SECTION(".exception.module") = {
	.magic = VK_MODULE_DRIVER,
	.self = VNZ_PHYSICAL_OF(0x80d000),
	.size = 0x780,
	.attributes = VK_MODULE_ATTRIBUTES,
	.version = VK_MODULE_VERSION,
	.resume = moduleResume,
	.suspend = moduleSuspend,
};

/* an address that the cores reach through the data cache */
static uint32_t inCachedImage(uint32_t address)
{
	return address >= VNZ_IMAGE && address < VNZ_DRAM + 0xa8000000;
}

static uint32_t cacheLine(uint32_t address)
{
	return address & ~(CACHE_LINE - 1);
}

/* write a vector word back, to every core's view */
static void writebackWord(uint32_t *word, uint32_t allCores)
{
	if (!inCachedImage((uint32_t)word))
		return;
	vnzCacheWriteback(cacheLine((uint32_t)word), CACHE_LINE);
	if (allCores)
		vnzL2PurgeAllCores(cacheLine((uint32_t)word), CACHE_LINE);
}

/* the vector slot of an exception */
static uint32_t *vectorSlot(uint32_t kind)
{
	uint32_t offset;

	if (kind == VK_EXCEPTION_NMI)
		return (uint32_t *)(vnzReadControl(VNZ_CR_RES30) + 4);
	if (kind == VK_EXCEPTION_DEBUG)
		return (uint32_t *)(vnzReadControl(VNZ_CR_RES30) + 0x18);
	switch (kind) {
	case VK_EXCEPTION_RESERVED:
		offset = 8;
		break;
	case VK_EXCEPTION_ZERO_DIVIDE:
		offset = 0xc;
		break;
	case VK_EXCEPTION_BREAK:
		offset = 0x10;
		break;
	default:
		offset = 0;	/* not reached: the kind is checked */
		break;
	}
	return (uint32_t *)(vnzReadControl(VNZ_CR_RES31) + offset);
}

/*
 * Hook an exception (handler 0: unhook it), on every core. The first hook
 * of a kind takes its vector slot; *previous receives the hook replaced.
 */
int32_t vkExceptionHook(uint32_t kind, VkExceptionHandler handler, VkExceptionHandler *previous)
{
	uint32_t base, size, core, i;
	uint32_t *slot, *saved, jump;

	base = vnzMemoryRegion(VK_MEMORY_EXCEPTION, &size);
	if (base == 0 || size < 0x800 || (size & 0x7ff) != 0)
		return VNZ_ERROR;
	switch (kind) {
	case VK_EXCEPTION_NMI:
	case VK_EXCEPTION_RESERVED:
	case VK_EXCEPTION_ZERO_DIVIDE:
	case VK_EXCEPTION_BREAK:
	case VK_EXCEPTION_DEBUG:
		break;
	default:
		return VNZ_ERROR;
	}

	vnzDisableDispatch();
	core = vnzCurrentCoreId() & 7;
	slot = vectorSlot(kind);
	jump = g_vkExceptionJumps[kind];
	if (inCachedImage((uint32_t)&g_vkExceptionJumps[kind]))
		vnzCachePurge(cacheLine((uint32_t)&g_vkExceptionJumps[kind]), CACHE_LINE);
	saved = &g_vkExceptionSaved[kind];

	if (handler) {
		if (*slot != jump) {
			for (i = 0; i < VNZ_CORES; i++) {
				base += size / VNZ_CORES;
				g_vkExceptionHooks.frames[i] = base;
			}
			g_vkExceptionHooks.handlers[kind] = 0;
			g_vkExceptionHooks.kinds[core] = VK_EXCEPTION_NONE;
			*saved = *slot;
			writebackWord(saved, 0);
			*slot = jump;
			writebackWord(slot, 1);
		}
	} else if (*slot == jump) {
		*slot = *saved;
		writebackWord(slot, 1);
		if (inCachedImage(base))
			vnzCachePurgeAllCores(base, size);
	}

	if (previous)
		*previous = g_vkExceptionHooks.handlers[kind];
	g_vkExceptionHooks.handlers[kind] = handler;
	vnzEnableDispatch();
	return 0;
}

/* this core's saved state, in a hook */
VkExceptionState *vkExceptionState(void)
{
	return (VkExceptionState *)(g_vkExceptionHooks.frames[vnzCurrentCoreId() & 7] -
				    sizeof(VkExceptionState));
}

/* the exception this core is in, in a hook */
uint32_t vkExceptionKind(void)
{
	return g_vkExceptionHooks.kinds[vnzCurrentCoreId() & 7];
}

/*
 * End a hook: with a function, call it; without, go on to what the vector
 * slot held before the hook took it.
 */
void vkExceptionChain(void (*function)(void))
{
	VkExceptionState *state;
	uint32_t kind;

	if (function) {
		function();
		return;
	}
	state = vkExceptionState();
	kind = g_vkExceptionHooks.kinds[vnzCurrentCoreId() & 7];
	vkExceptionResume(state, &g_vkExceptionSaved[kind], kind);
}
