/*
 * me_edram.c: Media Engine EDRAM backing allocation and offset translation
 * for ScePspemu.
 */
#include "host_common.h"

uint32_t g_meEdramSize;
uint8_t *g_meEdramBase;
SceUID   g_meEdramMemBlockId;

int meEdramInit(uint32_t size)
{
	g_meEdramSize = (size + 0xFFFFFU) & ~0xFFFFFU;
	SceUID memId = sceKernelAllocMemBlock("ScePspemuMeEdram", PSPEMU_MEMBLOCK_PHYCONT_RW, g_meEdramSize, NULL);
	g_meEdramMemBlockId = memId;
	if (memId < 0)
		return -1;
	if (sceKernelGetMemBlockBase(memId, (void **)&g_meEdramBase) != 0)
		return -2;
	return 0;
}

void *meEdramGetBase(uint32_t offset)
{
	if (offset >= g_meEdramSize)
		return NULL;
	return g_meEdramBase + offset;
}
