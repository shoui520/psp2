/*
 * The shell's trigger methods over SceShellSvc's IPMI connection (trigger_util_ipc_client.cpp;
 * the file and function names are ours). A request carries useCallback: the shell answers
 * an asynchronous call with SceShellSvc's callback event, a synchronous one with pEventFlag.
 */
#ifndef TRIGGER_UTIL_TRIGGER_UTIL_IPC_CLIENT_H
#define TRIGGER_UTIL_TRIGGER_UTIL_IPC_CLIENT_H

#include <scetypes.h>
#include <rtc.h>
#include "../../../../lowlayer/src/ipmi/client.h"
#include "trigger_util.h"

int triggerUtilRegisterPeriodicTimer(IPMI::Client *client, int *pRequestId,
				     const IPMI::EventFlagInfo *pEventFlag, const char *titleId,
				     const SceTriggerUtilPeriodicTimerParam *param,
				     SceUInt32 timerId, bool useCallback);
int triggerUtilUnregisterPeriodicTimer(IPMI::Client *client, int *pRequestId,
				       const IPMI::EventFlagInfo *pEventFlag, SceUInt32 timerId,
				       bool useCallback);
int triggerUtilGetPeriodicTimerRemainingTime(IPMI::Client *client, int *pRequestId,
					     const IPMI::EventFlagInfo *pEventFlag,
					     SceUInt32 timerId, bool useCallback);
int triggerUtilGetPeriodicTimerRemainingTimeResult(IPMI::Client *client, int requestId,
						   SceUInt32 *pRemainingTime);
int triggerUtilGetPeriodicTimerLastError(IPMI::Client *client, int *pRequestId,
					 const IPMI::EventFlagInfo *pEventFlag, SceUInt32 timerId,
					 bool useCallback);
int triggerUtilRegisterTimeOfDayTimer(IPMI::Client *client, int *pRequestId,
				      const IPMI::EventFlagInfo *pEventFlag, const char *titleId,
				      const SceTriggerUtilTimeOfDayTimerParam *param,
				      SceUInt32 timerId, bool useCallback);
int triggerUtilUnregisterTimeOfDayTimer(IPMI::Client *client, int *pRequestId,
					const IPMI::EventFlagInfo *pEventFlag, SceUInt32 timerId,
					bool useCallback);
int triggerUtilGetTimeOfDayTimerTime(IPMI::Client *client, int *pRequestId,
				     const IPMI::EventFlagInfo *pEventFlag, SceUInt32 timerId,
				     bool useCallback);
int triggerUtilGetTimeOfDayTimerTimeResult(IPMI::Client *client, int requestId,
					   SceRtcTick *pTime, bool *pFlag);
int triggerUtilGetTimeOfDayTimerLastError(IPMI::Client *client, int *pRequestId,
					  const IPMI::EventFlagInfo *pEventFlag,
					  SceUInt32 timerId, bool useCallback);
int triggerUtilRegisterDailyTimer(IPMI::Client *client, int *pRequestId,
				  const IPMI::EventFlagInfo *pEventFlag, const char *titleId,
				  const SceTriggerUtilDailyTimerParam *param, SceUInt32 timerId,
				  bool useCallback);
int triggerUtilUnregisterDailyTimer(IPMI::Client *client, int *pRequestId,
				    const IPMI::EventFlagInfo *pEventFlag, SceUInt32 timerId,
				    bool useCallback);
int triggerUtilGetDailyTimerInfo(IPMI::Client *client, int *pRequestId,
				 const IPMI::EventFlagInfo *pEventFlag, SceUInt32 timerId,
				 bool useCallback);
int triggerUtilGetDailyTimerInfoResult(IPMI::Client *client, int requestId,
				       SceTriggerUtilDailyTimerState *pState,
				       SceTriggerUtilDailyTimerParam *param, SceUInt32 *pStatus);
int triggerUtilGetDailyTimerLastError(IPMI::Client *client, int *pRequestId,
				      const IPMI::EventFlagInfo *pEventFlag, SceUInt32 timerId,
				      bool useCallback);
int triggerUtilGetPolicy(IPMI::Client *client, int *pRequestId,
			 const IPMI::EventFlagInfo *pEventFlag, bool useCallback);
int triggerUtilGetPolicyResult(IPMI::Client *client, int requestId, SceUInt8 *pPolicy);
/* the result of a request that answers nothing else */
int triggerUtilGetResult(IPMI::Client *client, int requestId);
int triggerUtilSetTitleFlag(IPMI::Client *client, const char *titleId, bool flag);
int triggerUtilGetAppTitleIdList(IPMI::Client *client, char *titleIdList, SceUInt32 *pCount);
int triggerUtilGetAppInfo(IPMI::Client *client, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, const char *titleId,
			  SceUInt32 option, bool useCallback);
int triggerUtilGetAppInfoResult(IPMI::Client *client, int requestId, SceTriggerUtilAppInfo *pInfo);
int triggerUtilGetSystemTitleIdList(IPMI::Client *client, char *titleIdList, SceUInt32 *pCount);
int triggerUtilGetSystemServiceInfo(IPMI::Client *client, int *pRequestId,
				    const IPMI::EventFlagInfo *pEventFlag, const char *titleId,
				    SceUInt32 option, bool useCallback);
int triggerUtilGetSystemServiceInfoResult(IPMI::Client *client, int requestId,
					  SceTriggerUtilSystemServiceInfo *pInfo);

#endif
