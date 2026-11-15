/*
 * np/np_bandwidth_test.h: the bandwidth test, under Sony's names and with Sony's definitions
 * (SDK Release 03.570.011, target/include/np/np_bandwidth_test.h).
 */
#ifndef _SCE_NP_BANDWIDTH_TEST_H_
#define _SCE_NP_BANDWIDTH_TEST_H_

#include <scetypes.h>

#define SCE_NP_BANDWIDTH_TEST_THREAD_STACK_SIZE		(32 * 1024)
#define SCE_NP_BANDWIDTH_TEST_LEAST_HTTP_POOL_SIZE	(16 * 1024)

typedef struct SceNpBandwidthTestResult {
	double uploadBps;
	double downloadBps;
	int result;
	char padding[4];
} SceNpBandwidthTestResult;

#define SCE_NP_BANDWIDTH_TEST_STATUS_NONE	0
#define SCE_NP_BANDWIDTH_TEST_STATUS_RUNNING	1
#define SCE_NP_BANDWIDTH_TEST_STATUS_FINISHED	2

#ifdef __cplusplus
extern "C" {
#endif

int sceNpBandwidthTestInitStart(SceInt32 initPriority, SceInt32 cpuAffinityMask);
int sceNpBandwidthTestGetStatus(void);
int sceNpBandwidthTestShutdown(SceNpBandwidthTestResult *result);
int sceNpBandwidthTestAbort(void);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NP_BANDWIDTH_TEST_H_ */
