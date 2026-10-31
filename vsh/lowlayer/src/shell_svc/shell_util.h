#ifndef SHELL_UTIL_H
#define SHELL_UTIL_H

#include <kernel.h>
#include "../ipmi/client.h"

/*
 * ShellUtil: a process's requests to the shell (locks, notices, launching an app), sent as
 * IPMI methods 0xC0000-0xC001E over ShellSvc's client. Most requests are asynchronous: the
 * request waits in one of four slots until the client thread (or the owner's poll) gets its
 * result and passes it to the event handler. Sony's libraries are SceShellUtil and
 * SceShellUtilUketorne (one function, two NIDs); the file names and the names below are
 * ours where no Sony name is known (agent namecheck).
 */

/* Our names for SceShellUtil's error codes (0x801018xx). */
#define SHELL_UTIL_ERROR_NO_STORAGE		-2146428928	/* 0x80101800 */
#define SHELL_UTIL_ERROR_NOT_INITIALIZED	-2146428927	/* 0x80101801 */
#define SHELL_UTIL_ERROR_ALREADY_INITIALIZED	-2146428926	/* 0x80101802 */
#define SHELL_UTIL_ERROR_BUSY			-2146428925	/* 0x80101803 */
#define SHELL_UTIL_ERROR_INVALID_ARGUMENT	-2146428924	/* 0x80101804 */
#define SHELL_UTIL_ERROR_TOO_LONG		-2146428912	/* 0x80101810 */

/* sceShellUtilInitEvents: no client thread; the owner polls. */
#define SHELL_UTIL_INIT_NO_THREAD	1

/* A lock type with the PS button in it also follows the shell's PS-button events. */
#define SHELL_UTIL_LOCK_TYPE_PS_BTN	0x800

/* The event a PS-button callback reports to the handler. */
#define SHELL_UTIL_EVENT_PS_BTN		21

/* Called with a request's result: the request's kind and parameter, and the user data. */
typedef void (*ShellUtilEventHandler)(int result, int event, int param, void *pUserData);

/* Called by the impose thread for each impose event. */
typedef int (*ShellUtilImposeEventCallback)(int result, int event, int param, void *pArg);

typedef struct SceShellUtilLaunchAppParam {
	const char *cmd;
} SceShellUtilLaunchAppParam;

#ifdef __cplusplus
extern "C" {
#endif

int sceShellUtilInitEvents(int flags);
void SceShellUtilUketorne_FB551721(void);
int SceShellUtilUketorne_FDC48BBE(void);
int sceShellUtilRegisterEventHandler(ShellUtilEventHandler handler, void *pUserData);
int sceShellUtilLock(int type);
int sceShellUtilUnlock(int type);
int SceShellUtilUketorne_156FEACF(int type);
int SceShellUtil_040997D6(int value);
int SceShellUtil_40DFAC6B(const SceWChar16 *pString1, const SceWChar16 *pString2);
int SceShellUtil_03201F4B(const char *pString);
int SceShellUtil_96BBF91B(int value);
int SceShellUtil_8B69AD27(int value);
int SceShellUtil_B65B60CA(int value);
int SceShellUtil_3219A1C0(int value);
int SceShellUtil_9B0EE918(void);
int SceShellUtil_8F2F143D(int value);
int SceShellUtil_636544FB(int value);
int SceShellUtil_BB54D049(int value1, int value2, int value3);
int SceShellUtil_6430C45E(void);
int SceShellUtil_29DB95D1(int value);
int SceShellUtil_0B066FAD(void);
int SceShellUtil_01141995(int value);
int SceShellUtil_E701BA71(const char *pString);
int SceShellUtil_0E7EAD89(int value);
int SceShellUtil_F75C684D(int value);
int SceShellUtil_4A234ABB(void);
int sceShellUtilLaunchAppRequestLaunchApp(const SceShellUtilLaunchAppParam *param);
int SceShellUtil_6F730976(void);
int SceShellUtil_8CAAB23B(void);
int SceShellUtil_48C7E8F8(const char *pString);
int SceShellUtil_B67A0C68(const char *pString);
int SceShellUtil_71C21A8C(void);
int SceShellUtil_FB612CA8(void);
int SceShellUtil_9A83AC60(int value1, int value2);
int SceShellUtil_CE35B2B8(ShellUtilImposeEventCallback callback, void *pArg);
int SceShellUtil_EC5881A5(void);
void SceShellUtil_F979148A(void);

#ifdef __cplusplus
}

/*
 * The IPMI stubs (shell_util_ipc_client.cpp), one per method, named after the export that
 * sends it. An asynchronous stub returns the request ID in *pRequestId.
 */
int shellUtilIpcLock(IPMI::Client *pClient, int *pRequestId,
		     const IPMI::EventFlagInfo *pEventFlag, int type);
int shellUtilIpcUnlock(IPMI::Client *pClient, int *pRequestId,
		       const IPMI::EventFlagInfo *pEventFlag, int type);
int shellUtilIpc_156FEACF(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, int type);
int shellUtilIpc_040997D6(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, int value);
int shellUtilIpc_40DFAC6B(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, const SceWChar16 *pString1,
			  const SceWChar16 *pString2);
int shellUtilIpc_03201F4B(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, const char *pString);
int shellUtilIpc_96BBF91B(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, int value);
int shellUtilIpc_8B69AD27(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, int value);
int shellUtilIpc_B65B60CA(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, int value);
int shellUtilIpc_3219A1C0(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, int value);
int shellUtilIpc_9B0EE918(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag);
int shellUtilIpc_8F2F143D(IPMI::Client *pClient, int value);
int shellUtilIpc_636544FB(IPMI::Client *pClient, int value);
int shellUtilIpc_BB54D049(IPMI::Client *pClient, int value1, int value2, int value3);
int shellUtilIpc_6430C45E(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag);
int shellUtilIpc_29DB95D1(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, int value);
int shellUtilIpc_0B066FAD(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag);
int shellUtilIpc_01141995(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, int value);
int shellUtilIpc_E701BA71(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, const char *pString);
int shellUtilIpc_0E7EAD89(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, int value);
int shellUtilIpc_F75C684D(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, int value);
int shellUtilIpc_4A234ABB(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag);
int shellUtilIpcLaunchApp(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, const char *pCmd);
int shellUtilIpc_6F730976(IPMI::Client *pClient);
int shellUtilIpc_8CAAB23B(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag);
int shellUtilIpc_48C7E8F8(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, const char *pString);
int shellUtilIpc_B67A0C68(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, const char *pString);
int shellUtilIpc_71C21A8C(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag);
int shellUtilIpc_FB612CA8(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag);
int shellUtilIpc_9A83AC60(IPMI::Client *pClient, int *pRequestId,
			  const IPMI::EventFlagInfo *pEventFlag, int value1, int value2);
int shellUtilIpcGetResult(IPMI::Client *pClient, int requestId);
#endif

#endif
