/*
 * libSceCes's MbcsCtx_Ucs.c (Sony's internal/usermode/src/ces/libces/src/MBCS/MbcsCtx_Ucs.c, the
 * SDK's static library, built with SNC): a context between a multi-byte character set and the
 * Unicode forms. In the build libpaf links it only initialises the context (SDK 3.570.011's
 * also binds a table of its own).
 */
#include <stdint.h>
#include "ces_mbcs_context.h"

int sceCesMbcsUcsContextInit(SceCesMbcsUcsContext *context, const SceCesMbcsUcsProfile *prof)
{
	return _sceCesMbcsUcsContextInit(context, prof);
}
