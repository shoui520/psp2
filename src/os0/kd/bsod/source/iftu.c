/* BSOD reads the first IFTU buffer and programs both hardware buffers. */
#include "bsod_internal.h"
#include <sce/iftu_regs.h>

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
