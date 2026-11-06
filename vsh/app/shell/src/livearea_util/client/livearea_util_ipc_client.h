/* the shell's LiveArea methods (livearea_util_ipc_client.cpp; the names are ours) */
#ifndef LIVEAREA_UTIL_CLIENT_LIVEAREA_UTIL_IPC_CLIENT_H
#define LIVEAREA_UTIL_CLIENT_LIVEAREA_UTIL_IPC_CLIENT_H

#include <scetypes.h>
#include "../../../../../lowlayer/src/ipmi/client.h"

/*
 * The asynchronous requests (their results come with the event flag) and their results. A
 * request a system application makes through the module's own exports says so (fromSystem),
 * and the requests from a background application say so (isBgApp).
 */
int liveAreaUtilRequestReplaceAll(IPMI::Client *client, int *pRequestId,
				  const IPMI::EventFlagInfo *pEventFlag, const char *dirpath,
				  const char *pOptionStr, const SceUInt64 *pOption,
				  bool fromSystem, bool isBgApp);
int liveAreaUtilGetReplaceAllResult(IPMI::Client *client, int requestId);
int liveAreaUtilRequestUpdateFrame(IPMI::Client *client, int *pRequestId,
				   const IPMI::EventFlagInfo *pEventFlag, const char *formatVer,
				   const char *frameXmlStr, SceInt32 frameXmlLen,
				   const char *dirpathTop, SceUInt32 flag, const char *pOptionStr,
				   bool fromSystem, bool isBgApp);
int liveAreaUtilGetUpdateFrameResult(IPMI::Client *client, int requestId);
int liveAreaUtilRequestUpdateFrameList(IPMI::Client *client, int *pRequestId,
				       const IPMI::EventFlagInfo *pEventFlag,
				       const char *formatVer, const char **frameXmlStrList,
				       SceUInt32 frameNum, const char *dirpathTop, SceUInt32 flag,
				       const char *pOptionStr, bool fromSystem);
int liveAreaUtilGetUpdateFrameListResult(IPMI::Client *client, int requestId);
int liveAreaUtilRequestGetFrameRevision(IPMI::Client *client, int *pRequestId,
					const IPMI::EventFlagInfo *pEventFlag, const char *frameId,
					bool isBgApp);
int liveAreaUtilGetFrameRevisionResult(IPMI::Client *client, int requestId, SceInt64 *revision);
int liveAreaUtilRequestGetFrameUserData(IPMI::Client *client, int *pRequestId,
					const IPMI::EventFlagInfo *pEventFlag, const char *frameId,
					SceUInt32 userDataSize, bool isBgApp);
int liveAreaUtilGetFrameUserDataResult(IPMI::Client *client, int requestId, char *userData,
				       SceUInt32 *pUserDataSize);

#endif
