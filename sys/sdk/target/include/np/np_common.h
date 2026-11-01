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

#endif /* _SCE_NP_NP_COMMON_H_ */
