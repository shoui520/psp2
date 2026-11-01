/*
 * SceGxm: vertex program queries.
 */

#include "gxm_internal.h"

const SceGxmProgram *sceGxmVertexProgramGetProgram(const SceGxmVertexProgram *vertexProgram)
{
	GXM_DBG_ASSERT(13, vertexProgram);
	GXM_DBG_ASSERT(14, vertexProgram->program);
	GXM_DBG_ASSERT(15, sceGxmProgramGetProgramType(vertexProgram->program) ==
			   SCE_GXM_VERTEX_PROGRAM);
	return vertexProgram->program;
}
