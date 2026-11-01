/*
 * SceGxm: the scene settings an immediate context keeps for its next scene.
 */

#include "gxm_internal.h"

int sceGxmSetVisibilityBuffer(SceGxmContext *immediateContext, void *bufferBase,
			      uint32_t stridePerCore)
{
	if (immediateContext == NULL) {
		GXM_DBG_ERROR_NULL(25, sceGxmSetVisibilityBuffer, immediateContext);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (immediateContext->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		GXM_DBG_ERROR(29, "Parameter immediateContext to function sceGxmSetVisibilityBuffer "
			      "is not an immediate context.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if ((SceUInt32)bufferBase & 0xf) {
		GXM_DBG_ERROR(34, "Parameter bufferBase to function sceGxmSetVisibilityBuffer is not "
			      "aligned to %d bytes.", 16);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (stridePerCore & 0xf) {
		GXM_DBG_ERROR(39, "Parameter stridePerCore to function sceGxmSetVisibilityBuffer is "
			      "not aligned to %d bytes.", 16);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (immediateContext->insideScene) {
		GXM_DBG_ERROR(45, "Function sceGxmSetVisibilityBuffer cannot be called within a "
			      "scene.");
		return SCE_GXM_ERROR_WITHIN_SCENE;
	}

	immediateContext->nextScene.settings.visibilityBuffer = bufferBase;
	immediateContext->nextScene.settings.visibilityStride = stridePerCore;
	return 0;
}

int sceGxmSetYuvProfile(SceGxmContext *immediateContext, uint32_t cscIndex,
			SceGxmYuvProfile profile)
{
	if (immediateContext == NULL) {
		GXM_DBG_ERROR_NULL(63, sceGxmSetYuvProfile, immediateContext);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (immediateContext->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		GXM_DBG_ERROR(67, "Parameter immediateContext to sceGxmSetYuvProfile is not an "
			      "immediate context.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (cscIndex > 1) {
		GXM_DBG_ERROR(71, "Parameter cscIndex to function sceGxmSetYuvProfile is not 0 or 1.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_SURFACE_ANY) &&
	    profile > SCE_GXM_YUV_PROFILE_BT709_FULL_RANGE) {
		GXM_DBG_ERROR(81, "Parameter profile to function sceGxmSetYuvProfile is not valid.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (immediateContext->insideScene) {
		GXM_DBG_ERROR(88, "Function sceGxmSetYuvProfile cannot be called within a scene.");
		return SCE_GXM_ERROR_WITHIN_SCENE;
	}

	immediateContext->nextScene.settings.yuvProfile[cscIndex] = profile;
	return 0;
}

/* sceGxmSetVertexJobNearClipMode (the debug build's messages name it) */
int SceGxmInternalForTest_C65CED80(SceGxmContext *immediateContext, SceUInt32 nearClipMode)
{
	if (immediateContext == NULL) {
		GXM_DBG_ERROR_NULL(104, sceGxmSetVertexJobNearClipMode, immediateContext);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (immediateContext->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		GXM_DBG_ERROR(108, "Parameter immediateContext to function "
			      "sceGxmSetVertexJobNearClipMode is not an immediate context.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (nearClipMode & ~0x80) {
		GXM_DBG_ERROR(112, "Parameter nearClipMode to function sceGxmSetVertexJobNearClipMode "
			      "is not valid.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}

	immediateContext->nextScene.tuning.nearClipMode = nearClipMode;
	return 0;
}

/* sceGxmSetVertexJobSplitThreshold */
int SceGxmInternalForTest_65FF05BC(SceGxmContext *immediateContext, SceUInt32 maxDrawCallsPerCore,
				   SceUInt32 primitiveSplitThreshold)
{
	if (immediateContext == NULL) {
		GXM_DBG_ERROR_NULL(129, sceGxmSetVertexJobSplitThreshold, immediateContext);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (immediateContext->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		GXM_DBG_ERROR(133, "Parameter immediateContext to function "
			      "sceGxmSetVertexJobSplitThreshold is not an immediate context.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (maxDrawCallsPerCore > 16) {
		GXM_DBG_ERROR(139, "Parameter maxDrawCallsPerCore to function "
			      "sceGxmSetVertexJobSplitThreshold has value %d, which is not in the "
			      "range [0, %d].", maxDrawCallsPerCore, 16);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (primitiveSplitThreshold - 1 > 0x7ffe) {
		GXM_DBG_ERROR(145, "Parameter primitiveSplitThreshold to function "
			      "sceGxmSetVertexJobSplitThreshold has value %d, which is not in the "
			      "range [1, %d].", primitiveSplitThreshold, 0x7fff);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}

	immediateContext->nextScene.tuning.maxDrawCallsPerCore = maxDrawCallsPerCore;
	immediateContext->nextScene.tuning.primitiveSplitThreshold = primitiveSplitThreshold;
	return 0;
}

/* sceGxmSetFragmentJobDepthBiasAdjust: two signed bytes */
int SceGxmInternalForTest_46D6C306(SceGxmContext *immediateContext, int factorAdjust,
				   int unitsAdjust)
{
	if (immediateContext == NULL) {
		GXM_DBG_ERROR_NULL(166, sceGxmSetFragmentJobDepthBiasAdjust, immediateContext);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (immediateContext->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		GXM_DBG_ERROR(170, "Parameter immediateContext to function "
			      "sceGxmSetFragmentJobDepthBiasAdjust is not an immediate context.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if ((SceUInt32)(factorAdjust + 128) > 0xff) {
		GXM_DBG_ERROR(175, "Parameter factorAdjust to function "
			      "sceGxmSetFragmentJobDepthBiasAdjust has value %d, which is not in the "
			      "range [-128, 127].", factorAdjust);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if ((SceUInt32)(unitsAdjust + 128) > 0xff) {
		GXM_DBG_ERROR(180, "Parameter unitsAdjust to function "
			      "sceGxmSetFragmentJobDepthBiasAdjust has value %d, which is not in the "
			      "range [-128, 127].", unitsAdjust);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}

	immediateContext->nextScene.settings.depthBiasAdjust = (SceUInt16)(factorAdjust << 8) |
						       (SceUInt8)unitsAdjust;
	return 0;
}

/* sceGxmSetFragmentJobSplitThreshold: only the per-quadrant mode takes a threshold */
int SceGxmInternalForTest_D352F90A(SceGxmContext *immediateContext, SceUInt32 secondaryMode,
				   SceUInt32 pixelSplitThreshold)
{
	if (immediateContext == NULL) {
		GXM_DBG_ERROR_NULL(202, sceGxmSetFragmentJobSplitThreshold, immediateContext);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (immediateContext->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		GXM_DBG_ERROR(206, "Parameter immediateContext to function "
			      "sceGxmSetFragmentJobSplitThreshold is not an immediate context.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (secondaryMode != 1 && pixelSplitThreshold != 0) {
		GXM_DBG_ERROR(210, "Parameter secondaryMode to function "
			      "sceGxmSetFragmentJobSplitThreshold must be "
			      "SCE_GXM_FRAGMENT_SECONDARY_MODE_PER_QUADRANT when parameter "
			      "pixelSplitThreshold is non-zero.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (pixelSplitThreshold >= 0x400) {
		GXM_DBG_ERROR(216, "Parameter pixelSplitThreshold to function "
			      "sceGxmSetFragmentJobSplitThreshold has value %d, which is not in the "
			      "range [0, %d].", pixelSplitThreshold, 1023);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_SURFACE_ANY) && secondaryMode > 1) {
		GXM_DBG_ERROR(224, "Parameter secondaryMode to function "
			      "sceGxmSetFragmentJobSplitThreshold is not valid.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}

	immediateContext->nextScene.settings.secondaryMode = secondaryMode;
	immediateContext->nextScene.settings.pixelSplitThreshold = pixelSplitThreshold;
	return 0;
}

/* sceGxmSetFragmentJobVisibilityBuffer: sceGxmSetVisibilityBuffer, also inside a scene */
int SceGxmInternalForTest_1D68FB73(SceGxmContext *immediateContext, void *bufferBase,
				   SceUInt32 stridePerCore)
{
	if (immediateContext == NULL) {
		GXM_DBG_ERROR_NULL(245, sceGxmSetFragmentJobVisibilityBuffer, immediateContext);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (immediateContext->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		GXM_DBG_ERROR(249, "Parameter immediateContext to function "
			      "sceGxmSetFragmentJobVisibilityBuffer is not an immediate context.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if ((SceUInt32)bufferBase & 0xf) {
		GXM_DBG_ERROR(254, "Parameter bufferBase to function "
			      "sceGxmSetFragmentJobVisibilityBuffer is not aligned to %d bytes.", 16);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (stridePerCore & 0xf) {
		GXM_DBG_ERROR(259, "Parameter stridePerCore to function "
			      "sceGxmSetFragmentJobVisibilityBuffer is not aligned to %d bytes.", 16);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}

	immediateContext->nextScene.settings.visibilityBuffer = bufferBase;
	immediateContext->nextScene.settings.visibilityStride = stridePerCore;
	return 0;
}

/*
 * sceGxmSetFragmentJobYuvProfile: sceGxmSetYuvProfile, also inside a scene and
 * without the compatibility flag
 */
int SceGxmInternalForTest_6E9565D5(SceGxmContext *immediateContext, SceUInt32 cscIndex,
				   SceUInt32 profile)
{
	if (immediateContext == NULL) {
		GXM_DBG_ERROR_NULL(277, sceGxmSetFragmentJobYuvProfile, immediateContext);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (immediateContext->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		GXM_DBG_ERROR(281, "Parameter immediateContext to function "
			      "sceGxmSetFragmentJobYuvProfile is not an immediate context.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (cscIndex > 1) {
		GXM_DBG_ERROR(285, "Parameter cscIndex to function sceGxmSetFragmentJobYuvProfile is "
			      "not 0 or 1.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (profile > SCE_GXM_YUV_PROFILE_BT709_FULL_RANGE) {
		GXM_DBG_ERROR(292, "Parameter profile to function sceGxmSetFragmentJobYuvProfile is "
			      "not a valid YUV profile.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}

	immediateContext->nextScene.settings.yuvProfile[cscIndex] = profile;
	return 0;
}
