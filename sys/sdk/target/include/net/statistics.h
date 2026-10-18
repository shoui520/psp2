/*
 * net/statistics.h: the network statistics, under Sony's names and with Sony's definitions (SDK
 * Release 03.570.011, target/include/net/statistics.h).
 */
#ifndef _SCE_NET_NET_STATISTICS_H
#define _SCE_NET_NET_STATISTICS_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SceNetStatisticsInfo {
	/* kernel */
	int kernel_mem_free_size;
	int kernel_mem_free_min;
	int packet_count;
	int packet_qos_count;
	/* libnet */
	int libnet_mem_free_size;
	int libnet_mem_free_min;
} SceNetStatisticsInfo;

int sceNetGetStatisticsInfo(SceNetStatisticsInfo *info, int flags);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NET_NET_STATISTICS_H */
