/*
 * SceGxmDbg: the debug build's checks of a context's state.
 *
 * Only the debug build (vs0:sys/external/libgxm_dbg_es4) has this file. A
 * draw checks its primitive against the polygon mode, the visibility test
 * against the scene's visibility buffer, the fragment program against the
 * color surface and render target, and every texture a program samples
 * against the query it samples it with. A deferred context keeps what its
 * draws used in the command list, for sceGxmExecuteCommandList to check
 * against the scene it runs in. An error the debug build reports also goes
 * to a game's screen as a notification.
 */

#include <kernel/processmgr.h>
#include "gxm_internal.h"

int sceAppMgrIsGameProgram(void);
int sceAppMgrSendNotificationRequest(const void *request, int flags);

/* --- the error notification --- */

typedef struct GxmNotificationRequest {
	SceInt32 type;
	char text[64];
} GxmNotificationRequest;

/* at most one notification a second */
#define GXM_ERROR_NOTIFICATION_INTERVAL	1000000

void gxmDebugReportError(int error, const char *function)
{
	GxmNotificationRequest request;
	SceUInt64 last, now;

	if (error == 0 || sceAppMgrIsGameProgram() != 1)
		return;
	last = gxmGlobals.errorNotificationTime;
	now = sceKernelGetProcessTimeWide();
	if (now - last > GXM_ERROR_NOTIFICATION_INTERVAL) {
		request.type = 0;
		sceClibSnprintf(request.text, 60, "GXM:\n%.42s - %08X", function, error);
		sceAppMgrSendNotificationRequest(&request, 0);
		gxmGlobals.errorNotificationTime = now;
	}
}

/* --- textures --- */

static const char *const gxmQueryPrecisionNames[8] = {
	"f32", "f16", "s8", "u8", "s16", "u16", "s32", "u32"
};

int gxmDebugCheckTextureQuery(SceUInt32 query, SceUInt32 unitFlags, SceUInt32 textureIndex,
			      const SceGxmTexture *texture, const char *stage)
{
	SceGxmTextureFormat format;
	SceUInt32 type, precisions, componentCount, swizzleComponents;
	int err, i;

	err = sceGxmTextureValidate(texture);
	if (err != 0) {
		GXM_DBG_ERROR(60, "Texture validation failed for %s texture %u\n", stage,
			      textureIndex);
		return err;
	}
	format = sceGxmTextureGetFormat(texture);
	type = sceGxmTextureGetType(texture);
	precisions = query & GXM_TEXTURE_QUERY_PRECISIONS;
	if (precisions != 0) {
		if (!gxmTextureFormatIsQueryPrecisionSupported(format, precisions)) {
			const char *names[16];

			for (i = 0; i < 16; i++)
				names[i] = "";
			for (i = 0; i < 8; i++) {
				if ((precisions >> i) & 1) {
					names[2 * i] = gxmQueryPrecisionNames[i];
					if (precisions >> (i + 1))
						names[2 * i + 1] = "|";
				}
			}
			GXM_DBG_ERROR(117, "Invalid texture query precisions of "
				      "[%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s] for %s texture %u with format [%s|%s].",
				      names[0], names[1], names[2], names[3], names[4], names[5],
				      names[6], names[7], names[8], names[9], names[10], names[11],
				      names[12], names[13], names[14], names[15], stage, textureIndex,
				      gxmTextureFormatGetName(format),
				      gxmTextureFormatGetSwizzleName(format));
			return SCE_GXM_ERROR_INVALID_SAMPLER_RESULT_TYPE_PRECISION;
		}
		if (sceGxmTextureGetGammaMode(texture) != SCE_GXM_TEXTURE_GAMMA_NONE &&
		    (precisions & ~3)) {
			GXM_DBG_ERROR(126, "Texture %u [%s|%s] uses gamma correction, but is sampled "
				      "at precisions other than half or float.", textureIndex,
				      gxmTextureFormatGetName(format),
				      gxmTextureFormatGetSwizzleName(format));
			return SCE_GXM_ERROR_INVALID_SAMPLER_RESULT_TYPE_PRECISION;
		}
		componentCount = ((query & GXM_TEXTURE_QUERY_COMPONENT_COUNT) >>
				  GXM_TEXTURE_QUERY_COMPONENT_COUNT_SHIFT) + 1;
		swizzleComponents = gxmTextureFormatGetSwizzleComponents(format);
		if (componentCount != swizzleComponents) {
			GXM_DBG_ERROR(136, "Texture query component count of %u does not match swizzle "
				      "component count %u for %s texture %u with format [%s|%s]. %s",
				      componentCount, swizzleComponents, stage, textureIndex,
				      gxmTextureFormatGetName(format),
				      gxmTextureFormatGetSwizzleName(format),
				      componentCount == 1 && (query & GXM_TEXTURE_QUERY_GATHER4) ?
				      "This is likely to be the case when performing a shadow map "
				      "query on a non-depth format." : "");
			return SCE_GXM_ERROR_INVALID_SAMPLER_RESULT_TYPE_COMPONENT_COUNT;
		}
	}
	if ((query & (GXM_TEXTURE_QUERY_GATHER4 | GXM_TEXTURE_QUERY_SAMPLE_INFO)) &&
	    gxmTextureFormatIsYuvPlanar(format)) {
		GXM_DBG_ERROR(145, "Texture %u [%s|%s] is a planar YUV format, which does not support "
			      "the acquisition of sample information, or use by a gather4 texture query.",
			      textureIndex, gxmTextureFormatGetName(format),
			      gxmTextureFormatGetSwizzleName(format));
		return SCE_GXM_ERROR_INVALID_TEXTURE;
	}
	if (query & GXM_TEXTURE_QUERY_CUBE) {
		if (type != SCE_GXM_TEXTURE_CUBE && type != SCE_GXM_TEXTURE_CUBE_ARBITRARY) {
			GXM_DBG_ERROR(154, "Texture %u [%s|%s] does not have a type of "
				      "SCE_GXM_TEXTURE_CUBE or SCE_GXM_TEXTURE_CUBE_ARBITRARY, but it is "
				      "being sampled as a cube map.", textureIndex,
				      gxmTextureFormatGetName(format),
				      gxmTextureFormatGetSwizzleName(format));
			return SCE_GXM_ERROR_INVALID_TEXTURE;
		}
	} else if (type == SCE_GXM_TEXTURE_CUBE || type == SCE_GXM_TEXTURE_CUBE_ARBITRARY) {
		GXM_DBG_ERROR(160, "Texture %u [%s|%s] has a type of SCE_GXM_TEXTURE_CUBE or "
			      "SCE_GXM_TEXTURE_CUBE_ARBITRARY, but it is being sampled as 1D or 2D.",
			      textureIndex, gxmTextureFormatGetName(format),
			      gxmTextureFormatGetSwizzleName(format));
		return SCE_GXM_ERROR_INVALID_TEXTURE;
	}
	if ((query & GXM_TEXTURE_QUERY_GATHER4) &&
	    (sceGxmTextureGetMinFilter(texture) != SCE_GXM_TEXTURE_FILTER_LINEAR ||
	     sceGxmTextureGetMagFilter(texture) != SCE_GXM_TEXTURE_FILTER_LINEAR ||
	     sceGxmTextureGetMipFilter(texture) != SCE_GXM_TEXTURE_MIP_FILTER_DISABLED)) {
		GXM_DBG_ERROR(172, "Texture %u [%s|%s] is used by a shadow map or gather4 texture "
			      "query, but is not configured for bilinear filtering.", textureIndex,
			      gxmTextureFormatGetName(format),
			      gxmTextureFormatGetSwizzleName(format));
		return SCE_GXM_ERROR_INVALID_SAMPLER_FILTER_MODE;
	}
	if ((query & GXM_TEXTURE_QUERY_SAMPLE_COEFFICIENTS) &&
	    !gxmTextureFormatSupportsSampleCoefficients(format)) {
		GXM_DBG_ERROR(182, "Texture %u [%s|%s] is not a format supported by either a shadow "
			      "map texture query or a gather4 texture query that returns sample "
			      "coefficients.", textureIndex, gxmTextureFormatGetName(format),
			      gxmTextureFormatGetSwizzleName(format));
		return SCE_GXM_ERROR_INVALID_TEXTURE;
	}
	if (type == SCE_GXM_TEXTURE_CUBE_ARBITRARY && unitFlags != 1) {
		GXM_DBG_ERROR(193, "Texture %u [%s|%s] has a type of SCE_GXM_TEXTURE_CUBE_ARBITRARY, "
			      "but is being used in a dependent texture read which will not always "
			      "produce correct results.", textureIndex, gxmTextureFormatGetName(format),
			      gxmTextureFormatGetSwizzleName(format));
		return SCE_GXM_ERROR_INVALID_TEXTURE;
	}
	return 0;
}

/* the texture bound to a unit, against the program's query of the unit */
int gxmDebugCheckTexture(const SceGxmProgram *program, const SceGxmTexture *texture,
			 SceUInt32 textureIndex, const char *stage)
{
	const SceGxmProgramParameter *parameter;
	const SceUInt16 *queries;
	SceUInt32 count, query, i;

	if (program->minorVersion > 4) {
		queries = GXM_RELATIVE(program->textureQueriesOffset);
		if (queries == NULL)
			return 0;
		query = queries[textureIndex];
		if (query == 0)
			return 0;
		return gxmDebugCheckTextureQuery(query,
						 gxmProgramGetTextureUnitFlags(program, textureIndex),
						 textureIndex, texture, stage);
	}
	count = sceGxmProgramGetParameterCount(program);
	for (i = 0; i < count; i++) {
		parameter = sceGxmProgramGetParameter(program, i);
		if (gxmParameterGetCategory(parameter) == SCE_GXM_PARAMETER_CATEGORY_SAMPLER &&
		    sceGxmProgramParameterGetResourceIndex(parameter) == textureIndex) {
			query = gxmParameterGetTextureQuery(parameter);
			return gxmDebugCheckTextureQuery(query,
							 gxmProgramGetTextureUnitFlags(program,
										       textureIndex),
							 textureIndex, texture, stage);
		}
	}
	return 0;
}

/* every texture unit the program samples */
int gxmDebugCheckTextures(const SceGxmProgram *program, const SceGxmTexture *textures,
			  const char *stage)
{
	const SceGxmProgramParameter *parameter;
	const SceUInt16 *queries;
	SceUInt32 count, query, textureIndex, i;
	int err;

	if (program->minorVersion > 4) {
		queries = GXM_RELATIVE(program->textureQueriesOffset);
		if (queries == NULL)
			return 0;
		for (i = 0; i < SCE_GXM_MAX_TEXTURE_UNITS; i++, textures++) {
			query = queries[i];
			if (query == 0)
				continue;
			err = gxmDebugCheckTextureQuery(query, gxmProgramGetTextureUnitFlags(program, i),
							i, textures, stage);
			if (err != 0)
				return err;
		}
		return 0;
	}
	count = sceGxmProgramGetParameterCount(program);
	for (i = 0; i < count; i++) {
		parameter = sceGxmProgramGetParameter(program, i);
		if (gxmParameterGetCategory(parameter) != SCE_GXM_PARAMETER_CATEGORY_SAMPLER)
			continue;
		textureIndex = sceGxmProgramParameterGetResourceIndex(parameter);
		query = gxmParameterGetTextureQuery(parameter);
		err = gxmDebugCheckTextureQuery(query, gxmProgramGetTextureUnitFlags(program, textureIndex),
						textureIndex, &textures[textureIndex], stage);
		if (err != 0)
			return err;
	}
	return 0;
}

/* --- draws --- */

static inline SceBool gxmPolygonModeIsPoint(SceUInt32 mode)
{
	return mode == SCE_GXM_POLYGON_MODE_POINT_10UV || mode == SCE_GXM_POLYGON_MODE_POINT ||
	       mode == SCE_GXM_POLYGON_MODE_POINT_01UV;
}

int gxmDebugCheckDraw(SceGxmContext *context, SceUInt32 primType, SceUInt32 indexCount,
		      SceUInt32 indexWrap)
{
	const SceGxmFragmentProgram *fragmentProgram;
	const SceGxmVertexProgram *vertexProgram;
	SceUInt32 mode, count, frontIsp, backIsp, passFlags, outputRegisterSize, visibilityCount;
	SceBool twoSided, backVisibility, discardOrDepthReplace;
	SceInt32 visibilityIndex;
	int err;

	mode = context->state.frontIsp & GXM_ISP_POLYGON_MODE;
	switch (primType) {
	case SCE_GXM_PRIMITIVE_TRIANGLES:
	case SCE_GXM_PRIMITIVE_TRIANGLE_STRIP:
	case SCE_GXM_PRIMITIVE_TRIANGLE_FAN:
	case SCE_GXM_PRIMITIVE_TRIANGLE_EDGES:
		if (mode != SCE_GXM_POLYGON_MODE_TRIANGLE_FILL &&
		    mode != SCE_GXM_POLYGON_MODE_TRIANGLE_LINE &&
		    mode != SCE_GXM_POLYGON_MODE_TRIANGLE_POINT) {
			GXM_DBG_ERROR(282, "Polygon mode should be set to 'Triangle', 'LineTriangle', "
				      "or 'PointTriangle' for triangle/strip/fan primitives.");
			return SCE_GXM_ERROR_INVALID_POLYGON_MODE;
		}
		break;
	case SCE_GXM_PRIMITIVE_LINES:
		if (mode != SCE_GXM_POLYGON_MODE_LINE) {
			GXM_DBG_ERROR(289, "Polygon mode should be set to 'Line' for line primitives.");
			return SCE_GXM_ERROR_INVALID_POLYGON_MODE;
		}
		break;
	case SCE_GXM_PRIMITIVE_POINTS:
		if (!gxmPolygonModeIsPoint(mode)) {
			GXM_DBG_ERROR(298, "Polygon mode should be set to 'Point10UV', 'Point', or "
				      "'Point01UV' for point primitives.");
			return SCE_GXM_ERROR_INVALID_POLYGON_MODE;
		}
		break;
	}
	if (sceGxmProgramIsSpriteCoordUsed(context->fragmentProgram->program) &&
	    !gxmPolygonModeIsPoint(mode)) {
		GXM_DBG_ERROR(313, "Polygon mode should be set to 'Point10UV', 'Point', or "
			      "'Point01UV' when the SPRITECOORD semantic is used.");
		return SCE_GXM_ERROR_INVALID_POLYGON_MODE;
	}

	if (indexCount == 0) {
		GXM_DBG_ERROR(320, "Index count is zero.");
		return SCE_GXM_ERROR_INVALID_INDEX_COUNT;
	}
	count = indexWrap != 0 ? indexWrap : indexCount;
	switch (primType) {
	case SCE_GXM_PRIMITIVE_TRIANGLES:
		if (count % 3 != 0) {
			GXM_DBG_ERROR(333, "Index %s per instance is %u, but must be a multiple of 3 "
				      "for triangle primitives.", indexWrap != 0 ? "wrap" : "count",
				      count);
			return SCE_GXM_ERROR_INVALID_INDEX_COUNT;
		}
		break;
	case SCE_GXM_PRIMITIVE_TRIANGLE_EDGES:
		if (count % 4 != 0) {
			GXM_DBG_ERROR(342, "Index %s per instance is %u, but must be a multiple of 4 "
				      "for triangle edge primitives.", indexWrap != 0 ? "wrap" : "count",
				      count);
			return SCE_GXM_ERROR_INVALID_INDEX_COUNT;
		}
		break;
	case SCE_GXM_PRIMITIVE_TRIANGLE_STRIP:
	case SCE_GXM_PRIMITIVE_TRIANGLE_FAN:
		if (count < 3) {
			GXM_DBG_ERROR(352, "Index %s per instance is %u, but must be at least 3 for "
				      "triangle/strip/fan primitives.", indexWrap != 0 ? "wrap" : "count",
				      count);
			return SCE_GXM_ERROR_INVALID_INDEX_COUNT;
		}
		break;
	case SCE_GXM_PRIMITIVE_LINES:
		if (count % 2 != 0) {
			GXM_DBG_ERROR(361, "Index %s per instance is %u, but must be a multiple of 2 "
				      "for line primitives.", indexWrap != 0 ? "wrap" : "count", count);
			return SCE_GXM_ERROR_INVALID_INDEX_COUNT;
		}
		break;
	}
	if (primType == SCE_GXM_PRIMITIVE_POINTS &&
	    !(context->state.vertexOutputs & GXM_VERTEX_OUTPUT_PSIZE)) {
		GXM_DBG_ERROR(374, "Primitive type is SCE_GXM_PRIMITIVE_POINTS, but the vertex "
			      "program does not output PSIZE.");
		return SCE_GXM_ERROR_INVALID_PRIMITIVE_TYPE;
	}

	frontIsp = context->state.frontIsp;
	backIsp = context->state.backIsp;
	twoSided = (frontIsp & GXM_ISP_TWO_SIDED) != 0;
	backVisibility = (backIsp & GXM_ISP_VISIBILITY_ENABLE) != 0;
	if ((frontIsp & GXM_ISP_VISIBILITY_ENABLE) || (backVisibility && twoSided)) {
		if (context->type == SCE_GXM_CONTEXT_TYPE_IMMEDIATE &&
		    context->scene.settings.visibilityBuffer == NULL) {
			GXM_DBG_ERROR(389, "Visibility testing is enabled, but the visibility buffer "
				      "address is NULL.");
			return SCE_GXM_ERROR_INVALID_VISIBILITY_BUFFER_POINTER;
		}
		discardOrDepthReplace = (frontIsp & GXM_ISP_PASS_TYPE) == SCE_GXM_PASS_TYPE_DISCARD ||
					(frontIsp & GXM_ISP_PASS_TYPE) == SCE_GXM_PASS_TYPE_DEPTH_REPLACE;
		if (frontIsp & GXM_ISP_VISIBILITY_ENABLE) {
			visibilityIndex = (context->state.frontBias & GXM_BIAS_VISIBILITY_INDEX) |
					  (frontIsp & GXM_ISP_VISIBILITY_INDEX_HIGH);
			if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
				if (context->listFrontVisibilityIndexMax >= visibilityIndex)
					visibilityIndex = context->listFrontVisibilityIndexMax;
				context->listFrontVisibilityIndexMax = visibilityIndex;
			} else {
				visibilityCount = context->scene.settings.visibilityStride >> 2;
				if (visibilityCount <= (SceUInt32)visibilityIndex) {
					GXM_DBG_ERROR(411, "Visibility index for front-facing primitives "
						      "has value %u, which is not in the range [0, %u].",
						      visibilityIndex, visibilityCount);
					return SCE_GXM_ERROR_INVALID_VISIBILITY_INDEX;
				}
			}
			if ((frontIsp & GXM_ISP_VISIBILITY_OP) == SCE_GXM_VISIBILITY_TEST_OP_INCREMENT &&
			    discardOrDepthReplace) {
				GXM_DBG_ERROR(424, "Visibility test op for front-facing primitives is "
					      "INCREMENT, which cannot be used with a fragment program that "
					      "uses discard or depth replace.");
				return SCE_GXM_ERROR_INVALID_VISIBILITY_OP;
			}
		}
		if (backVisibility && twoSided) {
			visibilityIndex = (context->state.backBias & GXM_BIAS_VISIBILITY_INDEX) |
					  (backIsp & GXM_ISP_VISIBILITY_INDEX_HIGH);
			if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
				if (context->listBackVisibilityIndexMax >= visibilityIndex)
					visibilityIndex = context->listBackVisibilityIndexMax;
				context->listBackVisibilityIndexMax = visibilityIndex;
			} else {
				visibilityCount = context->scene.settings.visibilityStride >> 2;
				if (visibilityCount <= (SceUInt32)visibilityIndex) {
					GXM_DBG_ERROR(439, "Visibility index for back-facing primitives "
						      "has value %u, which is not in the range [0, %u].",
						      visibilityIndex, visibilityCount);
					return SCE_GXM_ERROR_INVALID_VISIBILITY_INDEX;
				}
			}
			if ((backIsp & GXM_ISP_VISIBILITY_OP) == SCE_GXM_VISIBILITY_TEST_OP_INCREMENT &&
			    discardOrDepthReplace) {
				GXM_DBG_ERROR(452, "Visibility test op for back-facing primitives is "
					      "INCREMENT, which cannot be used with a fragment program that "
					      "uses discard or depth replace.");
				return SCE_GXM_ERROR_INVALID_VISIBILITY_OP;
			}
		}
	}

	if (!(frontIsp & GXM_ISP_FRAGMENT_PROGRAM_DISABLE) ||
	    (twoSided && !(backIsp & GXM_ISP_FRAGMENT_PROGRAM_DISABLE))) {
		fragmentProgram = context->fragmentProgram;
		passFlags = fragmentProgram->passFlags;
		if (!(passFlags & GXM_PASS_COLOR_UNCHANGED)) {
			if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
				context->listFragmentPassFlags |= passFlags;
			} else {
				if (sceGxmColorSurfaceIsEnabled(&context->colorSurface)) {
					outputRegisterSize = context->colorSurface.outputRegisterSize;
					if (outputRegisterSize !=
					    ((fragmentProgram->passFlags & GXM_PASS_OUTPUT_64BIT) != 0)) {
						GXM_DBG_ERROR(479, "Non-matching output register sizes. "
							      "Fragment program expects %s, while color surface "
							      "with format [%s|%s] expects %s.",
							      (fragmentProgram->passFlags & GXM_PASS_OUTPUT_64BIT) ?
							      "64-bit" : "32-bit",
							      gxmColorFormatGetName(sceGxmColorSurfaceGetFormat(&context->colorSurface)),
							      gxmColorFormatGetSwizzleName(sceGxmColorSurfaceGetFormat(&context->colorSurface)),
							      outputRegisterSize ? "64-bit" : "32-bit");
						return SCE_GXM_ERROR_INVALID_OUTPUT_REGISTER_SIZE;
					}
				}
				switch (context->sampleCount) {
				case 1:
					if (passFlags & (GXM_PASS_MSAA_2X | GXM_PASS_MSAA_4X)) {
						GXM_DBG_ERROR(489, "The fragment program was built for MSAA "
							      "translucency, but the render target does not use MSAA");
						return SCE_GXM_ERROR_INVALID_FRAGMENT_MSAA_MODE;
					}
					break;
				case 2:
					if (passFlags & GXM_PASS_MSAA_NONE) {
						GXM_DBG_ERROR(497, "The render target uses 2xMSAA, but the "
							      "fragment program was not built for MSAA translucency.");
						return SCE_GXM_ERROR_INVALID_FRAGMENT_MSAA_MODE;
					}
					break;
				case 4:
					if (passFlags & (GXM_PASS_MSAA_NONE | GXM_PASS_MSAA_2X)) {
						GXM_DBG_ERROR(505, "The render target uses 4xMSAA, but the "
							      "fragment program was not built for 4xMSAA "
							      "translucency.");
						return SCE_GXM_ERROR_INVALID_FRAGMENT_MSAA_MODE;
					}
					break;
				}
			}
		}
		if (context->precomputedFragmentState != NULL) {
			if (fragmentProgram != context->precomputedFragmentState->program) {
				GXM_DBG_ERROR(524, "Precomputed fragment state program does not match "
					      "current fragment program.");
				return SCE_GXM_ERROR_INVALID_PRECOMPUTED_FRAGMENT_STATE;
			}
		} else {
			err = gxmDebugCheckTextures(fragmentProgram->program, context->fragmentTextures,
						    "fragment");
			if (err != 0)
				return err;
		}
	}

	vertexProgram = context->vertexProgram;
	if (context->precomputedVertexState != NULL) {
		if (vertexProgram != context->precomputedVertexState->program) {
			GXM_DBG_ERROR(541, "Precomputed vertex state program does not match current "
				      "vertex program.");
			return SCE_GXM_ERROR_INVALID_PRECOMPUTED_VERTEX_STATE;
		}
	} else {
		err = gxmDebugCheckTextures(vertexProgram->program, context->vertexTextures, "vertex");
		if (err != 0)
			return err;
	}

	if (context->type == SCE_GXM_CONTEXT_TYPE_DEFERRED) {
		if (!(context->listDepthStencilUsage & GXM_LIST_VIEWPORT_SET) &&
		    gxmWarningIsEnabled(SCE_GXM_WARNING_DEFERRED_CONTEXT_MISSING_VIEWPORT))
			GXM_DBG_WARNING(558, "Draw call made on deferred context without a valid "
					"viewport set.");
		if (!(context->listDepthStencilUsage & GXM_LIST_REGION_CLIP_SET) &&
		    gxmWarningIsEnabled(SCE_GXM_WARNING_DEFERRED_CONTEXT_MISSING_REGION_CLIP))
			GXM_DBG_WARNING(564, "Draw call made on deferred context without a valid "
					"region clip set.");
	}
	return 0;
}

/* --- command lists --- */

/* what a command list's draws used, against the scene sceGxmExecuteCommandList runs it in */
int gxmDebugCheckCommandList(const SceGxmContext *context, const SceGxmCommandList *list)
{
	SceInt32 visibilityCount;
	SceUInt32 passFlags;
	SceBool colorEnabled;

	if (list->frontVisibilityIndexMax >= 0 || list->backVisibilityIndexMax >= 0) {
		if (context->scene.settings.visibilityBuffer == NULL) {
			GXM_DBG_ERROR(581, "Visibility testing is used by the command list, but the "
				      "visibility buffer address is NULL.");
			return SCE_GXM_ERROR_INVALID_VISIBILITY_BUFFER_POINTER;
		}
		visibilityCount = context->scene.settings.visibilityStride >> 2;
		if (list->frontVisibilityIndexMax >= visibilityCount) {
			GXM_DBG_ERROR(591, "During construction of commandList a visibility index for "
				      "front-facing primitives that had a value %d was specified, which is "
				      "not in the range [0, %d].", list->frontVisibilityIndexMax,
				      visibilityCount);
			return SCE_GXM_ERROR_INVALID_VISIBILITY_INDEX;
		}
		if (list->backVisibilityIndexMax >= visibilityCount) {
			GXM_DBG_ERROR(597, "During construction of commandList a visibility index for "
				      "back-facing primitives that had value %d was specified, which is "
				      "not in the range [0, %d].", list->backVisibilityIndexMax,
				      visibilityCount);
			return SCE_GXM_ERROR_INVALID_VISIBILITY_INDEX;
		}
	}
	colorEnabled = sceGxmColorSurfaceIsEnabled(&context->colorSurface);
	passFlags = list->fragmentPassFlags;
	if (colorEnabled) {
		if (context->colorSurface.outputRegisterSize == 0) {
			if (passFlags & GXM_PASS_OUTPUT_64BIT) {
				GXM_DBG_ERROR(607, "During construction of commandList a fragment program "
					      "was used that expects 64-bit output registers, but the "
					      "current color surface has a 32-bit output register.");
				return SCE_GXM_ERROR_INVALID_OUTPUT_REGISTER_SIZE;
			}
		} else if (passFlags & GXM_PASS_OUTPUT_32BIT) {
			GXM_DBG_ERROR(612, "During construction of commandList a fragment program was "
				      "used that expects 32-bit output registers, but the current color "
				      "surface has a 64-bit output register.");
			return SCE_GXM_ERROR_INVALID_OUTPUT_REGISTER_SIZE;
		}
	}
	switch (context->sampleCount) {
	case 1:
		if (passFlags & (GXM_PASS_MSAA_2X | GXM_PASS_MSAA_4X)) {
			GXM_DBG_ERROR(624, "During construction of commandList a fragment program was "
				      "used that was built for MSAA translucency, but the render target "
				      "does not use MSAA");
			return SCE_GXM_ERROR_INVALID_FRAGMENT_MSAA_MODE;
		}
		break;
	case 2:
		if (passFlags & GXM_PASS_MSAA_NONE) {
			GXM_DBG_ERROR(632, "The render target uses 2xMSAA, but a fragment program was "
				      "used during the construction of commandList that was not built for "
				      "MSAA translucency.");
			return SCE_GXM_ERROR_INVALID_FRAGMENT_MSAA_MODE;
		}
		break;
	case 4:
		if (passFlags & (GXM_PASS_MSAA_NONE | GXM_PASS_MSAA_2X)) {
			GXM_DBG_ERROR(640, "The render target uses 4xMSAA, but a fragment program was "
				      "used during the construction of commandList that was not built for "
				      "4xMSAA translucency.");
			return SCE_GXM_ERROR_INVALID_FRAGMENT_MSAA_MODE;
		}
		break;
	}
	return 0;
}
