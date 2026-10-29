/*
 * SceGxm: the scene settings an immediate context keeps for its next scene.
 */

#include "gxm_internal.h"

int sceGxmSetVisibilityBuffer(SceGxmContext *context, void *bufferBase, uint32_t stridePerCore)
{
	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (((SceUInt32)bufferBase & 0xf) || (stridePerCore & 0xf))
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (context->insideScene)
		return SCE_GXM_ERROR_WITHIN_SCENE;

	context->nextScene.settings.visibilityBuffer = bufferBase;
	context->nextScene.settings.visibilityStride = stridePerCore;
	return 0;
}

int sceGxmSetYuvProfile(SceGxmContext *context, uint32_t cscIndex, SceGxmYuvProfile profile)
{
	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE || cscIndex > 1 ||
	    (!(gxmGlobals.compatFlags & GXM_COMPAT_SURFACE_ANY) &&
	     profile > SCE_GXM_YUV_PROFILE_BT709_FULL_RANGE))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (context->insideScene)
		return SCE_GXM_ERROR_WITHIN_SCENE;

	context->nextScene.settings.yuvProfile[cscIndex] = profile;
	return 0;
}

int SceGxmInternalForTest_C65CED80(SceGxmContext *context, SceUInt32 option)
{
	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (option & ~0x80)
		return SCE_GXM_ERROR_INVALID_VALUE;

	context->nextScene.tuning.option = option;
	return 0;
}

int SceGxmInternalForTest_65FF05BC(SceGxmContext *context, SceUInt32 count, SceUInt32 limit)
{
	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE || count > 16 ||
	    limit - 1 > 0x7ffe)
		return SCE_GXM_ERROR_INVALID_VALUE;

	context->nextScene.tuning.count = count;
	context->nextScene.tuning.limit = limit;
	return 0;
}

int SceGxmInternalForTest_46D6C306(SceGxmContext *context, int first, int second)
{
	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	/* two signed bytes */
	if ((SceUInt32)(first + 128) > 0xff || (SceUInt32)(second + 128) > 0xff)
		return SCE_GXM_ERROR_INVALID_POINTER;

	context->nextScene.settings.offsets = (SceUInt16)(first << 8) | (SceUInt8)second;
	return 0;
}

int SceGxmInternalForTest_D352F90A(SceGxmContext *context, SceUInt32 mode, SceUInt32 modeValue)
{
	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	/* only mode 1 takes a value */
	if ((mode != 1 && modeValue != 0) || modeValue >= 0x400 ||
	    (!(gxmGlobals.compatFlags & GXM_COMPAT_SURFACE_ANY) && mode > 1))
		return SCE_GXM_ERROR_INVALID_VALUE;

	context->nextScene.settings.mode = mode;
	context->nextScene.settings.modeValue = modeValue;
	return 0;
}

/* sceGxmSetVisibilityBuffer, also inside a scene */
int SceGxmInternalForTest_1D68FB73(SceGxmContext *context, void *bufferBase, SceUInt32 stridePerCore)
{
	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (((SceUInt32)bufferBase & 0xf) || (stridePerCore & 0xf))
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;

	context->nextScene.settings.visibilityBuffer = bufferBase;
	context->nextScene.settings.visibilityStride = stridePerCore;
	return 0;
}

/* sceGxmSetYuvProfile, also inside a scene and without the compatibility flag */
int SceGxmInternalForTest_6E9565D5(SceGxmContext *context, SceUInt32 cscIndex, SceUInt32 profile)
{
	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE || cscIndex > 1 ||
	    profile > SCE_GXM_YUV_PROFILE_BT709_FULL_RANGE)
		return SCE_GXM_ERROR_INVALID_VALUE;

	context->nextScene.settings.yuvProfile[cscIndex] = profile;
	return 0;
}
