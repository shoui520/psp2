/*
 * SceDeci4pSDbgp exception stop detail (excp_info.c).
 */

#include <scetypes.h>
#include "sdbgp_internal.h"

int excpGetExcpStopReason(SdbgpCpuState *cpu, SdbgpExcpDetail *detail, uint32_t stopReason)
{
	detail->size = sizeof(*detail);
	detail->stopReason = stopReason;
	detail->id = 0;
	detail->reserved0 = 0;
	detail->reserved1 = 0;
	return 0;
}
