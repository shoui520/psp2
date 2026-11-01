/*
 * SceGxm: the programs that draw a scene's background object: the color
 * surface's background texture, resolved by the downscale filter when the
 * surface takes fewer samples than the render target.
 */

#include "gxm_internal.h"

/* the background's USSE code: write the fetched texel (or the filtered samples) out */
int gxmBackgroundBuildProgram(GxmChunkBuffer *buffer, const SceGxmColorSurface *surface,
			      SceUInt32 samples, GxmCodeRef *program, SceUInt32 *dms)
{
	SceUInt32 *code = gxmChunkBufferReserve(buffer, 0x40, 0x10);
	SceUInt32 *end, offset, taskControl, primaryRegs, secondaryRegs, tempRegs;

	if (code == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	offset = gxmChunkBufferOffset(buffer);
	code[1] = 0xfa440700;
	code[0] = 0x00000000;
	if (samples <= 1) {
		tempRegs = 0;
		secondaryRegs = 0;
		if (surface->outputRegisterSize != 0) {
			code[3] = 0x38840541;
			code[2] = 0x83000000;
		} else {
			code[3] = 0x50a50009;
			code[2] = 0xa0000000;
		}
		end = code + 4;
		taskControl = 10;
		primaryRegs = 4;
	} else {
		SceUInt32 mode = 0;
		int ret;

		tempRegs = 4 * samples;
		ret = gxmColorFormatGetFloatMode14(sceGxmColorSurfaceGetFormat(surface),
						   surface->outputRegisterSize, &mode);
		if (ret != 0)
			return ret;
		code[12] = 0x00000000;
		code[11] = mode | 0xe0a10400;
		code[2] = 0x60202a00;
		code[3] = 0x502b000a;
		code[4] = 0x20003000;
		code[5] = 0x50230008;
		code[7] = 0x50230008;
		code[8] = 0x20041000;
		code[9] = 0x08048180;
		code[10] = 0xd0000430;
		code[13] = 0xf9200000;
		code[6] = 0x20203280;
		if (surface->outputRegisterSize != 0) {
			code[15] = 0x38840541;
			code[14] = 0x03000000;
		} else {
			code[15] = 0x50a50009;
			code[14] = 0x20000000;
		}
		end = code + 16;
		taskControl = 38;
		primaryRegs = 2;
		secondaryRegs = 20;
	}
	buffer->cur = (SceUInt32)end;
	program->taskControl = taskControl;
	program->address = 0;
	gxmCodeRefSetAddress(program, offset);
	gxmDmsFragmentInfo(primaryRegs, secondaryRegs, tempRegs, SCE_FALSE, dms);
	return 0;
}

/* the background's PDS program: fetch the background texture (or iterate the samples) */
int gxmBackgroundBuildTextureProgram(GxmChunkBuffer *buffer, const SceGxmColorSurface *surface,
				     SceUInt32 samples, const GxmCodeRef *program,
				     GxmCommandBlock *block)
{
	GxmProgramFragmentInput input;

	if (samples > 1) {
		input.word = 0x0240000f;
		input.textureIndex = 0;
		input.iteratorBits = 16;
		input.samplerBits = 0;
	} else {
		SceUInt32 wide = surface->outputRegisterSize == 1;
		SceUInt32 mode = 0;
		int ret;

		ret = gxmColorFormatGetFloatMode4(sceGxmColorSurfaceGetFormat(surface),
						  surface->outputRegisterSize, &mode);
		if (ret != 0)
			return ret;
		input.iteratorBits = wide << 6;
		input.samplerBits = mode;
		input.word = 0xf800;
		input.textureIndex = 0;
	}
	return gxmPdsBuildFragmentProgram(buffer, &surface->backgroundTex, &input, 1, NULL,
					  program, block, NULL);
}

/* the downscale filter's constants: the four sample offsets, then the background texture */
typedef struct GxmBackgroundFilter {
	float offsets[16];
	SceGxmTexture texture;
} GxmBackgroundFilter;

int gxmBackgroundBuildFilterProgram(GxmChunkBuffer *buffer, const SceGxmColorSurface *surface,
				    SceUInt32 width, SceUInt32 height, SceUInt32 samples,
				    const GxmCodeRef *program, GxmCommandBlock *block)
{
	GxmBackgroundFilter *filter;
	float dx, dy;

	if (samples <= 1)
		return gxmPdsBuildSimpleProgram(buffer, 0, 0, program, block);
	filter = gxmChunkBufferReserve(buffer, sizeof(*filter), 4);
	if (filter == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	filter->offsets[0] = 0.0f;
	dy = 0.25f / (float)height;
	filter->offsets[3] = 0.0f;
	filter->offsets[5] = 0.0f;
	filter->offsets[10] = 0.0f;
	filter->offsets[11] = 0.0f;
	filter->offsets[12] = 0.0f;
	filter->offsets[14] = 0.0f;
	filter->offsets[15] = 0.0f;
	dx = 0.25f / (float)width;
	filter->offsets[1] = -dx;
	filter->offsets[4] = -dx;
	filter->offsets[6] = -dy;
	filter->offsets[7] = -dy;
	filter->offsets[2] = dx;
	filter->offsets[8] = dx;
	filter->offsets[9] = dy;
	filter->offsets[13] = dy;
	filter->texture.controlWords[0] = surface->backgroundTex.controlWords[0];
	filter->texture.controlWords[1] = surface->backgroundTex.controlWords[1];
	filter->texture.controlWords[2] = surface->backgroundTex.controlWords[2];
	filter->texture.controlWords[3] = surface->backgroundTex.controlWords[3];
	buffer->cur = (SceUInt32)(filter + 1);
	return gxmPdsBuildSimpleProgram(buffer, (SceUInt32)filter, sizeof(*filter) / 4, program,
					block);
}
