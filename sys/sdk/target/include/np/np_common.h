/*
 * np/np_common.h: the NP library's common definitions, under Sony's names and with Sony's
 * definitions (SDK Release 03.570.011, target/include/np/np_common.h), as user code needs them.
 */
#ifndef _SCE_NP_NP_COMMON_H_
#define _SCE_NP_NP_COMMON_H_

#include <scetypes.h>
#include <np/np_npid.h>

/* Communication ID */
typedef struct SceNpCommunicationId {
	char data[9];
	char term;
	SceUChar8 num;
	char dummy;
} SceNpCommunicationId;

typedef SceInt32 SceNpPlatformType;
#define SCE_NP_PLATFORM_TYPE_NONE		0
#define SCE_NP_PLATFORM_TYPE_PS3		1
#define SCE_NP_PLATFORM_TYPE_VITA		2
#define SCE_NP_PLATFORM_TYPE_PS4		3

#ifdef __cplusplus
extern "C" {
#endif

int sceNpCmpNpId(const SceNpId *npid1, const SceNpId *npid2);
int sceNpCmpNpIdInOrder(const SceNpId *npid1, const SceNpId *npid2, int *order);
int sceNpCmpOnlineId(const SceNpId *npid1, const SceNpId *npid2);
int sceNpGetPlatformType(const SceNpId *npId);
int sceNpSetPlatformType(SceNpId *npId, SceNpPlatformType platformType);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NP_NP_COMMON_H_ */
