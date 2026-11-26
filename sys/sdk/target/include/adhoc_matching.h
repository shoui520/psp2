/*
 * adhoc_matching.h: SceNetAdhocMatching SDK header, under Sony's names and definitions
 * (SDK Release 03.570.011, target/include/adhoc_matching.h).
 */
#ifndef _SCE_ADHOC_MATCHING_H_
#define _SCE_ADHOC_MATCHING_H_

#include <kernel/threadmgr.h>
#include <net.h>

/* adhoc_matching error codes */
#define SCE_NET_ADHOC_MATCHING_ERROR_INVALID_MODE		-2143211263	/* 0x80413101 */
#define SCE_NET_ADHOC_MATCHING_ERROR_INVALID_PORT		-2143211262	/* 0x80413102 */
#define SCE_NET_ADHOC_MATCHING_ERROR_INVALID_MAXNUM		-2143211261	/* 0x80413103 */
#define SCE_NET_ADHOC_MATCHING_ERROR_RXBUF_TOO_SHORT		-2143211260	/* 0x80413104 */
#define SCE_NET_ADHOC_MATCHING_ERROR_INVALID_OPTLEN		-2143211259	/* 0x80413105 */
#define SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ARG		-2143211258	/* 0x80413106 */
#define SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ID			-2143211257	/* 0x80413107 */
#define SCE_NET_ADHOC_MATCHING_ERROR_ID_NOT_AVAIL		-2143211256	/* 0x80413108 */
#define SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE			-2143211255	/* 0x80413109 */
#define SCE_NET_ADHOC_MATCHING_ERROR_IS_RUNNING			-2143211254	/* 0x8041310a */
#define SCE_NET_ADHOC_MATCHING_ERROR_NOT_RUNNING		-2143211253	/* 0x8041310b */
#define SCE_NET_ADHOC_MATCHING_ERROR_UNKNOWN_TARGET		-2143211252	/* 0x8041310c */
#define SCE_NET_ADHOC_MATCHING_ERROR_TARGET_NOT_READY		-2143211251	/* 0x8041310d */
#define SCE_NET_ADHOC_MATCHING_ERROR_EXCEED_MAXNUM		-2143211250	/* 0x8041310e */
#define SCE_NET_ADHOC_MATCHING_ERROR_REQUEST_IN_PROGRESS	-2143211249	/* 0x8041310f */
#define SCE_NET_ADHOC_MATCHING_ERROR_ALREADY_ESTABLISHED	-2143211248	/* 0x80413110 */
#define SCE_NET_ADHOC_MATCHING_ERROR_BUSY			-2143211247	/* 0x80413111 */
#define SCE_NET_ADHOC_MATCHING_ERROR_ALREADY_INITIALIZED	-2143211246	/* 0x80413112 */
#define SCE_NET_ADHOC_MATCHING_ERROR_NOT_INITIALIZED		-2143211245	/* 0x80413113 */
#define SCE_NET_ADHOC_MATCHING_ERROR_PORT_IN_USE		-2143211244	/* 0x80413114 */
#define SCE_NET_ADHOC_MATCHING_ERROR_STACKSIZE_TOO_SHORT	-2143211243	/* 0x80413115 */
#define SCE_NET_ADHOC_MATCHING_ERROR_INVALID_DATALEN		-2143211242	/* 0x80413116 */
#define SCE_NET_ADHOC_MATCHING_ERROR_NOT_ESTABLISHED		-2143211241	/* 0x80413117 */
#define SCE_NET_ADHOC_MATCHING_ERROR_DATA_BUSY			-2143211240	/* 0x80413118 */
#define SCE_NET_ADHOC_MATCHING_ERROR_INVALID_ALIGNMENT		-2143211239	/* 0x80413119 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SceNetAdhocMatchingMember {
	struct SceNetInAddr addr;
} SceNetAdhocMatchingMember;

/* Mode */
#define SCE_NET_ADHOC_MATCHING_MODE_PARENT			1
#define SCE_NET_ADHOC_MATCHING_MODE_CHILD			2
#define SCE_NET_ADHOC_MATCHING_MODE_P2P				3

/* Event Type */
#define SCE_NET_ADHOC_MATCHING_EVENT_HELLO			1
#define SCE_NET_ADHOC_MATCHING_EVENT_REQUEST			2
#define SCE_NET_ADHOC_MATCHING_EVENT_LEAVE			3
#define SCE_NET_ADHOC_MATCHING_EVENT_DENY			4
#define SCE_NET_ADHOC_MATCHING_EVENT_CANCEL			5
#define SCE_NET_ADHOC_MATCHING_EVENT_ACCEPT			6
#define SCE_NET_ADHOC_MATCHING_EVENT_ESTABLISHED		7
#define SCE_NET_ADHOC_MATCHING_EVENT_TIMEOUT			8
#define SCE_NET_ADHOC_MATCHING_EVENT_ERROR			9
#define SCE_NET_ADHOC_MATCHING_EVENT_BYE			10
#define SCE_NET_ADHOC_MATCHING_EVENT_DATA			11
#define SCE_NET_ADHOC_MATCHING_EVENT_DATA_ACK			12
#define SCE_NET_ADHOC_MATCHING_EVENT_DATA_TIMEOUT		13

typedef void (*SceNetAdhocMatchingHandler)(int id, int event, struct SceNetInAddr *peer,
					   int optlen, void *opt);

#define SCE_NET_ADHOC_MATCHING_POOLSIZE_DEFAULT			(128 * 1024)
#define SCE_NET_ADHOC_MATCHING_THREAD_PRIORITY_DEFAULT		SCE_KERNEL_DEFAULT_PRIORITY_USER
#define SCE_NET_ADHOC_MATCHING_THREAD_STACK_SIZE_DEFAULT	(16 * 1024)
#define SCE_NET_ADHOC_MATCHING_THREAD_CPU_AFFINITY_MASK		SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT

#define SCE_NET_ADHOC_MATCHING_MAXNUM				16
#define SCE_NET_ADHOC_MATCHING_MAXOPTLEN			9196
#define SCE_NET_ADHOC_MATCHING_MAXDATALEN			9204
#define SCE_NET_ADHOC_MATCHING_MAXHELLOOPTLEN			1426

int sceNetAdhocMatchingInit(SceSize poolsize, void *poolptr);
int sceNetAdhocMatchingTerm(void);
int sceNetAdhocMatchingCreate(int mode, int maxnum, SceUShort16 port, int rxbuflen,
			      unsigned int helloInterval, unsigned int keepaliveInterval,
			      int initCount, unsigned int rexmtInterval,
			      SceNetAdhocMatchingHandler handler);
int sceNetAdhocMatchingStart(int id, int threadPriority, int threadStackSize,
			     int threadCpuAffinityMask, int helloOptlen, const void *helloOpt);
int sceNetAdhocMatchingStop(int id);
int sceNetAdhocMatchingDelete(int id);
int sceNetAdhocMatchingSelectTarget(int id, const struct SceNetInAddr *target,
				    int optlen, const void *opt);
int sceNetAdhocMatchingCancelTarget(int id, const struct SceNetInAddr *target);
int sceNetAdhocMatchingCancelTargetWithOpt(int id, const struct SceNetInAddr *target,
					   int optlen, const void *opt);
int sceNetAdhocMatchingSetHelloOpt(int id, int optlen, const void *opt);
int sceNetAdhocMatchingGetHelloOpt(int id, int *buflen, void *buf);
int sceNetAdhocMatchingGetMembers(int id, SceSize *memberNum,
				  SceNetAdhocMatchingMember *members);
int sceNetAdhocMatchingSendData(int id, const struct SceNetInAddr *target,
				int datalen, const void *data);
int sceNetAdhocMatchingAbortSendData(int id, const struct SceNetInAddr *target);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_ADHOC_MATCHING_H_ */
