/* SCE kernel statistics adapter; original source filename is unavailable. */

#include "net_ps_internal.h"

unsigned int sceNetPsGetFreeMemory(int lowest);
extern int gNetPsPacketCount;
extern int gNetPsQosPacketCount;

/* Name unknown; fill the kernel's complete four-word statistics result. */
int sceNetPsGetKernelStatistics(SceNetPsKernelStatistics *statistics,
			     int networkOrder)
{
	statistics->kernel_mem_free_min = sceNetPsGetFreeMemory(1);
	statistics->kernel_mem_free_size = sceNetPsGetFreeMemory(0);
	statistics->packet_count = gNetPsPacketCount;
	statistics->packet_qos_count = gNetPsQosPacketCount;
	if (networkOrder) {
		statistics->kernel_mem_free_min =
			__builtin_bswap32(statistics->kernel_mem_free_min);
		statistics->kernel_mem_free_size =
			__builtin_bswap32(statistics->kernel_mem_free_size);
		statistics->packet_count = __builtin_bswap32(statistics->packet_count);
		statistics->packet_qos_count =
			__builtin_bswap32(statistics->packet_qos_count);
	}
	return 0;
}
