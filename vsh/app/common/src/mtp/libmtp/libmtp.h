/*
 * libmtp.h: SceLibMtp's exports the shell uses (link names SceLibMtp_<NID>: no Sony name is
 * known for them). Names ours.
 */
#ifndef _LIBMTP_LIBMTP_H
#define _LIBMTP_LIBMTP_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the host's functions libmtp calls, six of them (their meaning is the shell's) */
typedef struct SceLibMtpHostFuncs {
	int (*func[6])(int arg);
} SceLibMtpHostFuncs;

/* registers the host's functions */
int SceLibMtp_67DB4C13(const SceLibMtpHostFuncs *funcs);

struct MtpStatus;

/* mtp_mini.cpp: the API's requests */
int sceMtpBeginHttpGetDataWithUrl(const char *url, int arg, SceUInt64 offset,
	SceUInt32 *contextId);
int SceLibMtp_520D2696(SceUInt32 contextId);
int sceMtpBeginHttpGetPropertyWithUrl(const char *url, int arg, SceUInt32 *contextId);
int SceLibMtp_54CDF039(SceUInt32 contextId, int arg1, int arg2);
int SceLibMtp_CA704934(SceUInt32 contextId, void *data, SceSize size, SceSize *received,
	SceUInt64 *totalSize);
int sceMtpSendData(SceUInt32 contextId, const void *data, SceSize size, SceSize *sent,
	SceUInt64 *totalSize);
int SceLibMtp_77793BF2(SceUInt32 contextId, int result);
int sceMtpGetHostInfo(struct MtpStatus *status);
int SceLibMtp_F82B75F5(struct MtpStatus *status);
int SceLibMtp_1021F75F(SceUInt32 contextId);
int sceMtpTerminateExecution(void);

#ifdef __cplusplus
}
#endif

#endif
