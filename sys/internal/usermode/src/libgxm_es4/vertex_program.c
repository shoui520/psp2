/*
 * SceGxm: vertex program queries.
 */

#include "gxm_internal.h"

const SceGxmProgram *sceGxmVertexProgramGetProgram(const SceGxmVertexProgram *vertexProgram)
{
	return vertexProgram->programHeader;
}
