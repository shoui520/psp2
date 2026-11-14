/* snc_personality.c: SNC's personality routine, the common C++ routine for SNC's tables */
#include "unwinder.h"

_Unwind_Reason_Code __ARM_unwind_cpp_prcommon(_Unwind_State state, _Unwind_Control_Block *ucbp,
	_Unwind_Context *context, unsigned int idx);

_Unwind_Reason_Code __snc_personality_v0(_Unwind_State state, _Unwind_Control_Block *ucbp,
	_Unwind_Context *context)
{
	return __ARM_unwind_cpp_prcommon(state, ucbp, context, 4);
}
