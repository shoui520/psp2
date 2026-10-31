#include <libkernel/libkernel.h>
#include <kernel/error.h>
#include "shell_svc_client.h"
#include "../ipmi/assert.h"

ShellSvcClient g_shellSvcClient;
static ShellSvcEventCallbackEntry s_callbacks[SHELL_SVC_EVENT_COUNT];

IPMI::Client *SceShellSvc_B31E7F1C(void)
{
	return g_shellSvcClient.getClient();
}

/* Clears a pending callback event. */
int SceShellSvc_1FAB9A1E(int event)
{
	SceUInt32 pattern;
	int ret;

	if (event >= SHELL_SVC_EVENT_COUNT)
		return SHELL_SVC_ERROR_INVALID_EVENT;
	ret = g_shellSvcClient.getClient()->pollEventFlag(event / 32, 1 << (event % 32),
							   SHELL_SVC_EVENT_WAIT_MODE, &pattern);
	IPMI_ASSERT(ret == 0 || ret == SCE_KERNEL_ERROR_EVF_COND);
	return 0;
}

int SceShellSvc_D943CE15(int event, ShellSvcEventCallback callback, void *pArg)
{
	SceUInt32 pattern;
	int ret;

	if (event >= SHELL_SVC_EVENT_COUNT)
		return SHELL_SVC_ERROR_INVALID_EVENT;
	if (s_callbacks[event].callback != NULL)
		return SHELL_SVC_ERROR_ALREADY_EXISTS;
	ret = g_shellSvcClient.getClient()->pollEventFlag(event / 32, 1 << (event % 32),
							   SHELL_SVC_EVENT_WAIT_MODE, &pattern);
	IPMI_ASSERT(ret == 0 || ret == SCE_KERNEL_ERROR_EVF_COND);
	s_callbacks[event].callback = callback;
	s_callbacks[event].pArg = pArg;
	return 0;
}

int SceShellSvc_7CBF442B(int event)
{
	if (event >= SHELL_SVC_EVENT_COUNT)
		return SHELL_SVC_ERROR_INVALID_EVENT;
	s_callbacks[event].callback = NULL;
	return 0;
}

/* Runs the event's callback if the event is pending (and clears it). */
int SceShellSvc_FB03FBC9(int event)
{
	SceUInt32 bit;
	SceUInt32 pattern;
	int ret;

	if (event >= SHELL_SVC_EVENT_COUNT)
		return SHELL_SVC_ERROR_INVALID_EVENT;
	if (s_callbacks[event].callback == NULL)
		return SHELL_SVC_ERROR_NO_CALLBACK;
	bit = 1 << (event % 32);
	ret = g_shellSvcClient.getClient()->pollEventFlag(event / 32, 1 << (event % 32),
							   SHELL_SVC_EVENT_WAIT_MODE, &pattern);
	if (ret == 0 && (pattern & bit) != 0)
		return s_callbacks[event].callback(event, s_callbacks[event].pArg);
	IPMI_ASSERT(ret == SCE_KERNEL_ERROR_EVF_COND);
	return SHELL_SVC_ERROR_EVENT_NOT_SET;
}

int SceShellSvc_A0B067AC(IPMI::EventFlagInfo *pEventFlag, int event)
{
	if (event >= SHELL_SVC_EVENT_COUNT)
		return SHELL_SVC_ERROR_INVALID_WAIT_EVENT;
	pEventFlag->index = event / 32 + SHELL_SVC_WAIT_EVENT_FLAG;
	pEventFlag->bitPattern = 1 << (event % 32);
	return 0;
}

/* Whether a wait event is set (and clears it). */
int SceShellSvc_BD4161B0(int event, bool *pSet)
{
	SceUInt32 bit;
	SceUInt32 pattern;
	int ret;

	if (event >= SHELL_SVC_EVENT_COUNT)
		return SHELL_SVC_ERROR_INVALID_WAIT_EVENT;
	pattern = 0;
	bit = 1 << (event % 32);
	ret = g_shellSvcClient.getClient()->pollEventFlag(event / 32 + SHELL_SVC_WAIT_EVENT_FLAG,
							   bit, SHELL_SVC_EVENT_WAIT_MODE,
							   &pattern);
	*pSet = ret == 0 && (pattern & bit) != 0;
	return 0;
}

int SceShellSvc_8D4F9F2E(int event, SceUInt32 *pTimeout)
{
	SceUInt32 bit;
	SceUInt32 pattern;
	int ret;

	if (event >= SHELL_SVC_EVENT_COUNT)
		return SHELL_SVC_ERROR_INVALID_WAIT_EVENT;
	pattern = 0;
	bit = 1 << (event % 32);
	ret = g_shellSvcClient.getClient()->waitEventFlag(event / 32 + SHELL_SVC_WAIT_EVENT_FLAG,
							   bit, SHELL_SVC_EVENT_WAIT_MODE,
							   &pattern, pTimeout);
	if (ret == 0 && (pattern & bit) != 0)
		return 0;
	if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		ret = SHELL_SVC_ERROR_TIMEOUT;
	return ret;
}

/* Clears a pending wait event. */
int SceShellSvc_B452A177(int event)
{
	SceUInt32 bit;
	SceUInt32 pattern;
	int ret;

	if (event >= SHELL_SVC_EVENT_COUNT)
		return SHELL_SVC_ERROR_INVALID_WAIT_EVENT;
	pattern = 0;
	bit = 1 << (event % 32);
	ret = g_shellSvcClient.getClient()->pollEventFlag(event / 32 + SHELL_SVC_WAIT_EVENT_FLAG,
							   bit, SHELL_SVC_EVENT_WAIT_MODE,
							   &pattern);
	IPMI_ASSERT(ret == 0 || ret == SCE_KERNEL_ERROR_EVF_COND);
	return 0;
}
