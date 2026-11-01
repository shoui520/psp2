/*
 * SceGxm: fragment program queries.
 */

#include "gxm_internal.h"

/* the debug build checks the program it is asked about, from line on */
#define GXM_CHECK_FRAGMENT_PROGRAM(line)						\
	_SCE_MACRO_BEGIN								\
		GXM_DBG_ASSERT(line, fragmentProgram);					\
		GXM_DBG_ASSERT((line) + 1, fragmentProgram->program);			\
		GXM_DBG_ASSERT((line) + 2, sceGxmProgramGetProgramType(fragmentProgram->program) == \
			       SCE_GXM_FRAGMENT_PROGRAM);					\
	_SCE_MACRO_END

const SceGxmProgram *sceGxmFragmentProgramGetProgram(const SceGxmFragmentProgram *fragmentProgram)
{
	GXM_CHECK_FRAGMENT_PROGRAM(13);
	return fragmentProgram->program;
}

SceUInt32 sceGxmFragmentProgramGetPassType(const SceGxmFragmentProgram *fragmentProgram)
{
	GXM_CHECK_FRAGMENT_PROGRAM(23);
	return fragmentProgram->ispFlags & GXM_ISP_PASS_TYPE;
}

SceBool sceGxmFragmentProgramIsEnabled(const SceGxmFragmentProgram *fragmentProgram)
{
	GXM_DBG_ASSERT(33, fragmentProgram);
	return (fragmentProgram->ispFlags ^ GXM_ISP_FRAGMENT_PROGRAM_DISABLE) >> 21 & 1;
}
