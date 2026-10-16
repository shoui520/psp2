/* BSOD maps IFTU, reads the first IFTU buffer and programs both hardware buffers. */
#include "bsod_internal.h"
#include <sce/iftu_regs.h>

/* name unknown: one register page BSOD maps for itself */
typedef struct BsodRegisterMap {
	const char *name;
	uint32_t paddr;
	SceSize size;
	uint8_t **base;
} BsodRegisterMap;

uint8_t *s_bsodIftu[3];

static const BsodRegisterMap s_iftuMaps[2][3] = {
	{
		{ "SceBsodIftu0RegA", IFTU_PAGE_BASE(0, 0), 0x1000, &s_bsodIftu[0] },
		{ "SceBsodIftu0RegB", IFTU_PAGE_BASE(0, 1), 0x1000, &s_bsodIftu[1] },
		{ "SceBsodIftuc0Reg", IFTUC_REG_BASE(0), 0x1000, &s_bsodIftu[2] },
	},
	{
		{ "SceBsodIftu1RegA", IFTU_PAGE_BASE(1, 0), 0x1000, &s_bsodIftu[0] },
		{ "SceBsodIftu1RegB", IFTU_PAGE_BASE(1, 1), 0x1000, &s_bsodIftu[1] },
		{ "SceBsodIftuc1Reg", IFTUC_REG_BASE(1), 0x1000, &s_bsodIftu[2] },
	},
};

#if FW_DEBUG
static const SceKernelDebugInfo s_iftuAssert = {
	0x6ec74984, 0x62f7350e, 0xe6d8021d,
	"iftu.c", 94, "_sceKernelBsodIftuInit",
};
#endif

int _sceKernelBsodIftuInit(int dolce)
{
	const BsodRegisterMap *map;
	SceKernelAllocMemBlockKernelOpt options;
	void *base;
	unsigned i;
	int uid;

	for (i = 0; i < 3; i++) {
		map = &s_iftuMaps[dolce][i];
		memset(&options, 0, sizeof(options));
		options.size = sizeof(options);
		options.attr = 2;
		options.paddr = map->paddr;
		uid = ksceKernelAllocMemBlock(map->name, SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW,
					    map->size, &options);
		if (uid < 0) {
#if FW_DEBUG
			if (ksceKernelGetAssertLevel() > 0)
				ksceKernelAssertLevel(1, 0, &s_iftuAssert,
						     __builtin_return_address(0));
#endif
			return uid;
		}
		ksceKernelGetMemBlockBase(uid, &base);
		*map->base = base;
	}
	return 0;
}

int bsodReadFramebuffer(unsigned bank, BsodFramebuffer *framebuffer)
{
	volatile SceIftuRegisters *registers = (void *)s_bsodIftu[bank ? 1 : 0];
	volatile SceIftuBufferRegisters *buffer = &registers->buffer[0];

	if (buffer->disabled) {
		framebuffer->physical = 0;
		framebuffer->pitch = 0;
		framebuffer->x = 0;
		framebuffer->width = 0;
		framebuffer->height = 0;
		framebuffer->scaleX = 0;
		framebuffer->scaleY = 0;
	} else {
		framebuffer->physical = buffer->address[0];
		framebuffer->pitch = buffer->width + (buffer->linePadding >> 2);
		framebuffer->x = 0;
		framebuffer->width = buffer->width;
		framebuffer->height = buffer->height;
		framebuffer->scaleX = buffer->scaleX;
		framebuffer->scaleY = buffer->scaleY;
	}
	return 0;
}

void bsodWriteFramebuffer(unsigned bank, const BsodFramebuffer *framebuffer)
{
	volatile SceIftuRegisters *registers = (void *)s_bsodIftu[bank ? 1 : 0];
	volatile SceIftuBufferRegisters *buffer;
	unsigned i;

	for (i = 0; i < 2; i++) {
		buffer = &registers->buffer[i];
		if (!framebuffer) {
			buffer->disabled = 1;
			buffer->format = 0x10;
			buffer->width = 960;
			buffer->height = 544;
			buffer->vtopPadding = 0;
			buffer->vbotPadding = 0;
			buffer->hleftPadding = 0;
			buffer->hrightPadding = 0;
			buffer->scaleX = 0x10000;
			buffer->scaleY = 0x10000;
		} else {
			buffer->disabled = 0;
			buffer->format = 0x10;
			buffer->width = framebuffer->width;
			buffer->height = framebuffer->height;
			buffer->linePadding = (framebuffer->pitch - framebuffer->width) * 4;
			buffer->leftoverAlign = 0;
			buffer->address[0] = framebuffer->physical;
			buffer->address[1] = 0;
			buffer->address[2] = 0;
			buffer->sourceConfig[0] = 0;
			buffer->sourceConfig[1] = 0;
			buffer->sourceConfig[2] = 0;
			buffer->vtopPadding = 0;
			buffer->vbotPadding = 0;
			buffer->hleftPadding = 0;
			buffer->hrightPadding = 0;
			buffer->scaleX = framebuffer->scaleX;
			buffer->scaleY = framebuffer->scaleY;
			buffer->dstX = 0;
			buffer->dstY = 0;
			buffer->sampleX = 0;
			buffer->sampleY = 0;
		}
	}
}
