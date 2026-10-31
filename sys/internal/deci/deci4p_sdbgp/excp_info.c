/*
 * SceDeci4pSDbgp exception stop detail (excp_info.c).
 */

#include <scetypes.h>
#include "sdbgp_internal.h"

int excpGetExcpStopReason(SdbgpCpuState *cpu, SdbgpExcpDetail *detail, uint32_t stopReason)
{
	detail->value[0] = sizeof(*detail);
	detail->value[1] = stopReason;
	detail->value[2] = 0;
	detail->value[3] = 0;
	detail->value[4] = 0;
	return 0;
}
