/*
 * np/np_npid.h: the NP ID, under Sony's names and with Sony's definitions (SDK Release
 * 03.570.011, target/include/np/np_npid.h).
 */
#ifndef _SCE_NP_NPID_H_
#define _SCE_NP_NPID_H_

#include <scetypes.h>

#define SCE_NP_ONLINEID_MIN_LENGTH	3
#define SCE_NP_ONLINEID_MAX_LENGTH	16

typedef struct SceNpOnlineId {
	char data[SCE_NP_ONLINEID_MAX_LENGTH];
	char term;
	char dummy[3];
} SceNpOnlineId;

typedef struct SceNpId {
	SceNpOnlineId handle;
	SceUChar8 opt[8];
	SceUChar8 reserved[8];
} SceNpId;

#endif /* _SCE_NP_NPID_H_ */
