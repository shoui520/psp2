/*
 * SceGxm: fragment program queries.
 */

#include "gxm_internal.h"

const SceGxmProgram *sceGxmFragmentProgramGetProgram(const SceGxmFragmentProgram *fragmentProgram)
{
	return fragmentProgram->programHeader;
}

SceUInt32 sceGxmFragmentProgramGetPassType(const SceGxmFragmentProgram *fragmentProgram)
{
	return fragmentProgram->ispFlags & GXM_ISP_PASS_TYPE;
}

SceBool sceGxmFragmentProgramIsEnabled(const SceGxmFragmentProgram *fragmentProgram)
{
	return (fragmentProgram->ispFlags ^ GXM_ISP_FRAGMENT_PROGRAM_DISABLE) >> 21 & 1;
}
