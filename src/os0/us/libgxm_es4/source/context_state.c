/*
 * SceGxm: context render state.
 *
 * Every setter folds its value into the state word it lives in and, if
 * the word changed, marks it dirty so the next draw sends it.
 */

#include "gxm_internal.h"

#define GXM_SET_STATE(context, word, mask, value, dirtyBit)			\
	do {									\
		SceUInt32 _new = (value) | ((context)->word & ~(mask));	\
		if ((context)->word != _new) {					\
			(context)->word = _new;					\
			(context)->state.dirty |= (dirtyBit);				\
		}								\
	} while (0)

void gxmContextUpdateVertexOutputs(SceGxmContext *context, const SceGxmVertexProgram *program)
{
	const GxmProgramVaryings *varyings = gxmProgramGetVaryings(program->programHeader);
	SceUInt32 dirty;

	if (context->state.reserved0) {
		context->state.reserved0 = 0;
		dirty = GXM_DIRTY_RESERVED0;
	} else {
		dirty = 0;
	}
	if (context->state.vertexOutputs != varyings->vertex.outputs) {
		context->state.vertexOutputs = varyings->vertex.outputs;
		dirty |= GXM_DIRTY_VERTEX_OUTPUTS;
	}
	if (context->state.texCoordOutputs != varyings->vertex.texCoordOutputs) {
		context->state.texCoordOutputs = varyings->vertex.texCoordOutputs;
		dirty |= GXM_DIRTY_TEXCOORD_OUTPUTS;
	}
	if (context->state.vertexOutputs2 != varyings->vertex.outputs2) {
		context->state.vertexOutputs2 = varyings->vertex.outputs2;
		dirty |= GXM_DIRTY_VERTEX_OUTPUTS2;
	}
	context->state.dirty |= dirty;
}

void gxmContextUpdateFrontFragmentProgram(SceGxmContext *context,
					  const SceGxmFragmentProgram *program)
{
	SceUInt32 isp = context->frontFragmentProgramMode | program->ispFlags |
			(context->state.frontIsp & ~GXM_ISP_FRAGMENT_PROGRAM_MASK);

	if (context->state.frontIsp != isp) {
		context->state.frontIsp = isp;
		context->state.dirty |= GXM_DIRTY_FRONT_ISP;
	}
}

void gxmContextUpdateBackFragmentProgram(SceGxmContext *context,
					 const SceGxmFragmentProgram *program)
{
	SceUInt32 isp = context->backFragmentProgramMode | program->ispFlags |
			(context->state.backIsp & ~GXM_ISP_FRAGMENT_PROGRAM_MASK);

	if (context->state.backIsp != isp) {
		context->state.backIsp = isp;
		context->state.dirty |= GXM_DIRTY_BACK_ISP;
	}
}

void sceGxmSetFrontDepthFunc(SceGxmContext *context, SceUInt32 depthFunc)
{
	SceUInt32 isp = (depthFunc & GXM_ISP_DEPTH_FUNC) | (context->state.frontIsp & ~GXM_ISP_DEPTH_FUNC);

	if (context->state.frontIsp == isp)
		return;
	context->state.frontIsp = isp;
	context->state.dirty |= GXM_DIRTY_FRONT_ISP;
	context->flags |= GXM_CONTEXT_FLAG_DEPTH_STENCIL_USED;
}

void sceGxmSetBackDepthFunc(SceGxmContext *context, SceUInt32 depthFunc)
{
	SceUInt32 isp = (depthFunc & GXM_ISP_DEPTH_FUNC) | (context->state.backIsp & ~GXM_ISP_DEPTH_FUNC);

	if (context->state.backIsp == isp)
		return;
	context->state.backIsp = isp;
	context->state.dirty |= GXM_DIRTY_BACK_ISP;
	context->flags |= GXM_CONTEXT_FLAG_DEPTH_STENCIL_USED;
}

void sceGxmSetFrontFragmentProgramEnable(SceGxmContext *context, SceUInt32 enable)
{
	SceUInt32 programFlags = 0;

	if (context->fragmentProgram != NULL)
		programFlags = context->fragmentProgram->ispFlags;
	context->frontFragmentProgramMode = enable & GXM_ISP_FRAGMENT_PROGRAM_DISABLE;
	GXM_SET_STATE(context, state.frontIsp, GXM_ISP_FRAGMENT_PROGRAM_DISABLE,
		      (enable & GXM_ISP_FRAGMENT_PROGRAM_DISABLE) | programFlags, GXM_DIRTY_FRONT_ISP);
}

void sceGxmSetBackFragmentProgramEnable(SceGxmContext *context, SceUInt32 enable)
{
	SceUInt32 programFlags = 0;

	if (context->fragmentProgram != NULL)
		programFlags = context->fragmentProgram->ispFlags;
	context->backFragmentProgramMode = enable & GXM_ISP_FRAGMENT_PROGRAM_DISABLE;
	GXM_SET_STATE(context, state.backIsp, GXM_ISP_FRAGMENT_PROGRAM_DISABLE,
		      (enable & GXM_ISP_FRAGMENT_PROGRAM_DISABLE) | programFlags, GXM_DIRTY_BACK_ISP);
}

void sceGxmSetFrontDepthWriteEnable(SceGxmContext *context, SceUInt32 enable)
{
	GXM_SET_STATE(context, state.frontIsp, GXM_ISP_DEPTH_WRITE_DISABLE,
		      enable & GXM_ISP_DEPTH_WRITE_DISABLE, GXM_DIRTY_FRONT_ISP);
}

void sceGxmSetBackDepthWriteEnable(SceGxmContext *context, SceUInt32 enable)
{
	GXM_SET_STATE(context, state.backIsp, GXM_ISP_DEPTH_WRITE_DISABLE,
		      enable & GXM_ISP_DEPTH_WRITE_DISABLE, GXM_DIRTY_BACK_ISP);
}

void sceGxmSetFrontLineFillLastPixelEnable(SceGxmContext *context, SceUInt32 enable)
{
	GXM_SET_STATE(context, state.frontIsp, GXM_ISP_LINE_FILL_LAST_PIXEL,
		      enable & GXM_ISP_LINE_FILL_LAST_PIXEL, GXM_DIRTY_FRONT_ISP);
}

void sceGxmSetBackLineFillLastPixelEnable(SceGxmContext *context, SceUInt32 enable)
{
	GXM_SET_STATE(context, state.backIsp, GXM_ISP_LINE_FILL_LAST_PIXEL,
		      enable & GXM_ISP_LINE_FILL_LAST_PIXEL, GXM_DIRTY_BACK_ISP);
}

void sceGxmSetFrontStencilRef(SceGxmContext *context, SceUInt32 sref)
{
	GXM_SET_STATE(context, state.frontIsp, GXM_ISP_STENCIL_REF, sref, GXM_DIRTY_FRONT_ISP);
}

void sceGxmSetBackStencilRef(SceGxmContext *context, SceUInt32 sref)
{
	GXM_SET_STATE(context, state.backIsp, GXM_ISP_STENCIL_REF, sref, GXM_DIRTY_BACK_ISP);
}

void sceGxmSetFrontPointLineWidth(SceGxmContext *context, SceUInt32 width)
{
	GXM_SET_STATE(context, state.frontIsp, GXM_ISP_POINT_LINE_WIDTH,
		      (width - 1) << GXM_ISP_POINT_LINE_WIDTH_SHIFT, GXM_DIRTY_FRONT_ISP);
}

void sceGxmSetBackPointLineWidth(SceGxmContext *context, SceUInt32 width)
{
	GXM_SET_STATE(context, state.backIsp, GXM_ISP_POINT_LINE_WIDTH,
		      (width - 1) << GXM_ISP_POINT_LINE_WIDTH_SHIFT, GXM_DIRTY_BACK_ISP);
}

void sceGxmSetFrontPolygonMode(SceGxmContext *context, SceUInt32 mode)
{
	GXM_SET_STATE(context, state.frontIsp, GXM_ISP_POLYGON_MODE, mode & GXM_ISP_POLYGON_MODE,
		      GXM_DIRTY_FRONT_ISP);
}

void sceGxmSetBackPolygonMode(SceGxmContext *context, SceUInt32 mode)
{
	GXM_SET_STATE(context, state.backIsp, GXM_ISP_POLYGON_MODE, mode & GXM_ISP_POLYGON_MODE,
		      GXM_DIRTY_BACK_ISP);
}

/* stencil word: func | fail | depth fail | depth pass ops, compare and write masks */
static inline SceUInt32 gxmStencilWord(SceUInt32 func, SceUInt32 stencilFail, SceUInt32 depthFail,
				       SceUInt32 depthPass, SceUInt8 compareMask, SceUInt8 writeMask)
{
	return (compareMask << 8 | writeMask) | ((depthPass << 16) & 0x00070000) |
	       ((depthFail << 19) & 0x00380000) | (func & 0x0e000000) |
	       ((stencilFail << 22) & 0x01c00000);
}

/* stencil testing is off when it always passes and keeps everything */
#define GXM_STENCIL_NOP		0x0e00

void sceGxmSetFrontStencilFunc(SceGxmContext *context, SceUInt32 func, SceUInt32 stencilFail,
			       SceUInt32 depthFail, SceUInt32 depthPass, SceUInt8 compareMask,
			       SceUInt8 writeMask)
{
	SceUInt32 stencil = gxmStencilWord(func, stencilFail, depthFail, depthPass,
					   compareMask, writeMask);
	SceUInt32 isp = (stencil >> 16 == GXM_STENCIL_NOP ? 0 : GXM_ISP_STENCIL_ENABLE) |
			(context->state.frontIsp & ~GXM_ISP_STENCIL_ENABLE);

	if (context->state.frontStencil != stencil) {
		context->state.frontStencil = stencil;
		context->state.dirty |= GXM_DIRTY_FRONT_STENCIL;
		context->flags |= GXM_CONTEXT_FLAG_DEPTH_STENCIL_USED;
	}
	if (context->state.frontIsp != isp) {
		context->state.frontIsp = isp;
		context->state.dirty |= GXM_DIRTY_FRONT_ISP;
	}
}

void sceGxmSetBackStencilFunc(SceGxmContext *context, SceUInt32 func, SceUInt32 stencilFail,
			      SceUInt32 depthFail, SceUInt32 depthPass, SceUInt8 compareMask,
			      SceUInt8 writeMask)
{
	SceUInt32 stencil = gxmStencilWord(func, stencilFail, depthFail, depthPass,
					   compareMask, writeMask);
	SceUInt32 isp = (stencil >> 16 == GXM_STENCIL_NOP ? 0 : GXM_ISP_STENCIL_ENABLE) |
			(context->state.backIsp & ~GXM_ISP_STENCIL_ENABLE);

	if (context->state.backStencil != stencil) {
		context->state.backStencil = stencil;
		context->state.dirty |= GXM_DIRTY_BACK_STENCIL;
		context->flags |= GXM_CONTEXT_FLAG_DEPTH_STENCIL_USED;
	}
	if (context->state.backIsp != isp) {
		context->state.backIsp = isp;
		context->state.dirty |= GXM_DIRTY_BACK_ISP;
	}
}

void sceGxmSetFrontDepthBias(SceGxmContext *context, SceInt32 factor, SceInt32 units)
{
	GXM_SET_STATE(context, state.frontBias, GXM_BIAS_MASK,
		      ((factor << GXM_BIAS_SLOPE_SHIFT) & 0x03e00000) |
		      ((units << GXM_BIAS_UNITS_SHIFT) & 0x001f0000),
		      GXM_DIRTY_FRONT_BIAS);
}

void sceGxmSetBackDepthBias(SceGxmContext *context, SceInt32 factor, SceInt32 units)
{
	GXM_SET_STATE(context, state.backBias, GXM_BIAS_MASK,
		      ((factor << GXM_BIAS_SLOPE_SHIFT) & 0x03e00000) |
		      ((units << GXM_BIAS_UNITS_SHIFT) & 0x001f0000),
		      GXM_DIRTY_BACK_BIAS);
}

void sceGxmSetTwoSidedEnable(SceGxmContext *context, SceUInt32 enable)
{
	SceUInt32 isp = (enable & GXM_ISP_TWO_SIDED) | (context->state.frontIsp & ~GXM_ISP_TWO_SIDED);

	if (context->state.frontIsp == isp)
		return;
	context->state.frontIsp = isp;
	context->state.dirty |= GXM_DIRTY_FRONT_ISP;
	/* the back face starts from the current fragment program */
	if (enable != 0 && context->fragmentProgram != NULL)
		gxmContextUpdateBackFragmentProgram(context, context->fragmentProgram);
}

GXM_VFP void sceGxmSetViewport(SceGxmContext *context, float xOffset, float xScale,
			       float yOffset, float yScale, float zOffset, float zScale)
{
	SceUInt32 viewport[6] = {
		gxmFloatBits(xOffset), gxmFloatBits(xScale),
		gxmFloatBits(yOffset), gxmFloatBits(yScale),
		gxmFloatBits(zOffset), gxmFloatBits(zScale),
	};

	if (context->state.viewport[0] == viewport[0] && context->state.viewport[1] == viewport[1] &&
	    context->state.viewport[2] == viewport[2] && context->state.viewport[3] == viewport[3] &&
	    context->state.viewport[4] == viewport[4] && context->state.viewport[5] == viewport[5])
		return;
	context->state.viewport[0] = viewport[0];
	context->state.viewport[1] = viewport[1];
	context->state.viewport[2] = viewport[2];
	context->state.viewport[3] = viewport[3];
	context->state.viewport[4] = viewport[4];
	context->state.viewport[5] = viewport[5];
	context->state.dirty |= GXM_DIRTY_VIEWPORT;
}

GXM_VFP void sceGxmSetWClampValue(SceGxmContext *context, float clampValue)
{
	if (context->state.wClampValue != gxmFloatBits(clampValue)) {
		context->state.wClampValue = gxmFloatBits(clampValue);
		context->state.dirty |= GXM_DIRTY_W_CLAMP_VALUE;
	}
}

void sceGxmSetWClampEnable(SceGxmContext *context, SceUInt32 enable)
{
	GXM_SET_STATE(context, state.cull, GXM_CULL_W_CLAMP, enable & GXM_CULL_W_CLAMP, GXM_DIRTY_CULL);
}

/* the clip region is kept in 32-pixel tiles */
void sceGxmSetRegionClip(SceGxmContext *context, SceUInt32 clipMode, SceUInt32 xMin,
			 SceUInt32 yMin, SceUInt32 xMax, SceUInt32 yMax)
{
	SceUInt32 tileXMin = xMin >> 5, tileYMin = yMin >> 5;
	SceUInt32 tileXMax = xMax >> 5, tileYMax = yMax >> 5;
	SceUInt32 limit, clip0, clip1;

	if (context->type == SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		limit = context->tileLimitY >> 5;
		if (tileYMin <= limit && limit < tileYMax)
			tileYMax = limit;
	}
	clip0 = (tileXMax & 0xff) | clipMode | ((tileXMin << 16) & 0xff0000);
	clip1 = (tileYMax & 0xff) | ((tileYMin << 16) & 0xff0000);
	if (context->state.regionClip[0] == clip0 && context->state.regionClip[1] == clip1)
		return;
	context->state.regionClip[0] = clip0;
	context->state.regionClip[1] = clip1;
	context->state.dirty |= GXM_DIRTY_REGION_CLIP;
}

void sceGxmSetCullMode(SceGxmContext *context, SceUInt32 mode)
{
	GXM_SET_STATE(context, state.cull, GXM_CULL_MODE, mode & GXM_CULL_MODE, GXM_DIRTY_CULL);
}

void sceGxmSetViewportEnable(SceGxmContext *context, SceUInt32 enable)
{
	GXM_SET_STATE(context, state.cull, GXM_CULL_VIEWPORT_DISABLE, enable & GXM_CULL_VIEWPORT_DISABLE,
		      GXM_DIRTY_CULL);
}

void sceGxmSetWBufferEnable(SceGxmContext *context, SceUInt32 enable)
{
	GXM_SET_STATE(context, state.cull, GXM_CULL_W_BUFFER, enable & GXM_CULL_W_BUFFER, GXM_DIRTY_CULL);
}

void sceGxmSetFrontVisibilityTestIndex(SceGxmContext *context, SceUInt32 index)
{
	GXM_SET_STATE(context, state.frontIsp, GXM_ISP_VISIBILITY_INDEX_HIGH,
		      index & GXM_ISP_VISIBILITY_INDEX_HIGH, GXM_DIRTY_FRONT_ISP);
	GXM_SET_STATE(context, state.frontBias, GXM_BIAS_VISIBILITY_INDEX,
		      index & GXM_BIAS_VISIBILITY_INDEX, GXM_DIRTY_FRONT_BIAS);
}

void sceGxmSetBackVisibilityTestIndex(SceGxmContext *context, SceUInt32 index)
{
	GXM_SET_STATE(context, state.backIsp, GXM_ISP_VISIBILITY_INDEX_HIGH,
		      index & GXM_ISP_VISIBILITY_INDEX_HIGH, GXM_DIRTY_BACK_ISP);
	GXM_SET_STATE(context, state.backBias, GXM_BIAS_VISIBILITY_INDEX,
		      index & GXM_BIAS_VISIBILITY_INDEX, GXM_DIRTY_BACK_BIAS);
}

void sceGxmSetFrontVisibilityTestOp(SceGxmContext *context, SceUInt32 op)
{
	GXM_SET_STATE(context, state.frontIsp, GXM_ISP_VISIBILITY_OP, op & GXM_ISP_VISIBILITY_OP,
		      GXM_DIRTY_FRONT_ISP);
}

void sceGxmSetBackVisibilityTestOp(SceGxmContext *context, SceUInt32 op)
{
	GXM_SET_STATE(context, state.backIsp, GXM_ISP_VISIBILITY_OP, op & GXM_ISP_VISIBILITY_OP,
		      GXM_DIRTY_BACK_ISP);
}

void sceGxmSetFrontVisibilityTestEnable(SceGxmContext *context, SceUInt32 enable)
{
	GXM_SET_STATE(context, state.frontIsp, GXM_ISP_VISIBILITY_ENABLE,
		      enable & GXM_ISP_VISIBILITY_ENABLE, GXM_DIRTY_FRONT_ISP);
}

void sceGxmSetBackVisibilityTestEnable(SceGxmContext *context, SceUInt32 enable)
{
	GXM_SET_STATE(context, state.backIsp, GXM_ISP_VISIBILITY_ENABLE,
		      enable & GXM_ISP_VISIBILITY_ENABLE, GXM_DIRTY_BACK_ISP);
}

#define GXM_REGION_CLIP_ALL	0x80000000

/* clip to the whole surface and map clip space onto it */
void sceGxmSetDefaultRegionClipAndViewport(SceGxmContext *context, SceUInt32 xMax, SceUInt32 yMax)
{
	SceUInt32 tileXMax = xMax >> 5, tileYMax = yMax >> 5, limit, clip0;
	float halfWidth, halfHeight, negHalfHeight;

	if (context->type == SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		limit = context->tileLimitY >> 5;
		if (tileYMax >= limit)
			tileYMax = limit;
	}
	clip0 = (tileXMax & 0xff) | GXM_REGION_CLIP_ALL;
	if (clip0 != context->state.regionClip[0] || (tileYMax & 0xff) != context->state.regionClip[1]) {
		context->state.regionClip[1] = tileYMax & 0xff;
		context->state.regionClip[0] = clip0;
		context->state.dirty |= GXM_DIRTY_REGION_CLIP;
	}

	halfWidth = (float)(xMax + 1) * 0.5f;
	halfHeight = (float)(yMax + 1) * 0.5f;
	negHalfHeight = (float)(yMax + 1) * -0.5f;
	if (context->state.viewport[0] == gxmFloatBits(halfWidth) &&
	    context->state.viewport[1] == gxmFloatBits(halfWidth) &&
	    context->state.viewport[2] == gxmFloatBits(halfHeight) &&
	    context->state.viewport[3] == gxmFloatBits(negHalfHeight) &&
	    context->state.viewport[4] == gxmFloatBits(0.5f) &&
	    context->state.viewport[5] == gxmFloatBits(0.5f))
		return;
	context->state.viewport[0] = gxmFloatBits(halfWidth);
	context->state.viewport[1] = gxmFloatBits(halfWidth);
	context->state.viewport[2] = gxmFloatBits(halfHeight);
	context->state.viewport[3] = gxmFloatBits(negHalfHeight);
	context->state.dirty |= GXM_DIRTY_VIEWPORT;
	context->state.viewport[4] = gxmFloatBits(0.5f);
	context->state.viewport[5] = gxmFloatBits(0.5f);
}
