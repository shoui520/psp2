/* Shared C state operations around the module's CPU entry boundaries. */
#pragma once
#include "excpmgr.h"

#define excpReturnToContext SceExcpmgrForKernel_3E55B5C3
#define EXCP_CONTEXT_DEBUG_LINKAGE extern
#include "sce/excpmgr_context_state.h"

static inline void excpCallback(SceExcpmgrCallback callback, SceExcpmgrContext *context)
{
	if (callback != NULL)
		callback(context, 0, g_excpData.kernelMmu, DACR_KERNEL);
}
