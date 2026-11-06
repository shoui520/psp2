/* the shell's notification methods (notification_util_ipc_client.cpp; the names are ours) */
#ifndef NOTIFICATION_UTIL_NOTIFICATION_UTIL_IPC_CLIENT_H
#define NOTIFICATION_UTIL_NOTIFICATION_UTIL_IPC_CLIENT_H

#include <scetypes.h>
#include "../../../../../vsh/lowlayer/src/ipmi/client.h"
#include "notification_util.h"

/*
 * The asynchronous requests (their results come with the event flag) and their results. The
 * requests from a background application say so.
 */
int notificationUtilRequestSend(IPMI::Client *client, int *pRequestId,
				const IPMI::EventFlagInfo *pEventFlag,
				const SceNotificationUtilSendParam *pParam, bool isBgApp);
int notificationUtilGetSendResult(IPMI::Client *client, int requestId);
int notificationUtilRequestCleanHistory(IPMI::Client *client, int *pRequestId,
					const IPMI::EventFlagInfo *pEventFlag, bool isBgApp);
int notificationUtilGetCleanHistoryResult(IPMI::Client *client, int requestId);
int notificationUtilRequestProgressBegin(IPMI::Client *client, int *pRequestId,
					 const IPMI::EventFlagInfo *pEventFlag,
					 const SceNotificationUtilProgressBeginParam *pParam,
					 bool isBgApp);
int notificationUtilGetProgressBeginResult(IPMI::Client *client, int requestId);
int notificationUtilProgressUpdate(IPMI::Client *client,
				   const SceNotificationUtilProgressUpdateParam *pParam);
int notificationUtilRequestProgressFinish(IPMI::Client *client, int *pRequestId,
					  const IPMI::EventFlagInfo *pEventFlag,
					  const SceNotificationUtilProgressFinishParam *pParam);
int notificationUtilGetProgressFinishResult(IPMI::Client *client, int requestId);

#endif
