#include <libkernel/libkernel.h>
#define _Static_assert(expression, message)
#include <processmgr/processmgr.h>
#undef _Static_assert
#include "shell_svc_client.h"
#include "../ipmi/assert.h"
#include "../ipmi/ipmi_error.h"

#define SHELL_SVC_SERVICE_NAME		"SceShellSvc"
#define SHELL_SVC_CLIENT_BUFFER_SIZE	0x400
#define SHELL_SVC_CALL_COUNT		4
#define SHELL_SVC_MAX_CALL_SIZE		0x3F00
#define SHELL_SVC_EVENT_FLAG_COUNT	8
/* The budget a game process's client is created in. */
#define SHELL_SVC_GAME_BUDGET		2

static char s_clientBuffer[SHELL_SVC_CLIENT_BUFFER_SIZE];
static bool s_created;

int ShellSvcClient::connect()
{
	char param[16];
	int result;
	int ret;
	int err;

	if (m_connected)
		return 0;
	ret = m_pClient->connect(param, sizeof(param), &result);
	if (ret != 0) {
		err = SHELL_SVC_ERROR_UNAVAILABLE;
		switch (ret) {
		case IPMI_ERROR_CONNECTION_REFUSED:
			err = result;
			break;
		case IPMI_ERROR_SERVER_NOT_FOUND:
			err = SHELL_SVC_ERROR_SERVER_NOT_FOUND;
			break;
		}
		sceClibPrintf("\n[SceShellSvc] *WARNING* IPC Server is not available. (0x%x)\n", ret);
		return err;
	}
	IPMI_ASSERT(result == 0);
	m_connected = true;
	return 0;
}

int ShellSvcClient::create()
{
	int ret;

	if (m_pClient != NULL)
		return SHELL_SVC_ERROR_ALREADY_EXISTS;

	IPMI::Client::Config config;

	if (sceKernelIsGameBudget() != 0)
		config.budget = SHELL_SVC_GAME_BUDGET;
	config.syncCallCount = SHELL_SVC_CALL_COUNT;
	config.maxSyncCallInSize = SHELL_SVC_MAX_CALL_SIZE;
	config.maxSyncCallOutSize = SHELL_SVC_MAX_CALL_SIZE;
	config.asyncCallCount = SHELL_SVC_CALL_COUNT;
	config.maxAsyncCallInSize = SHELL_SVC_MAX_CALL_SIZE;
	config.maxAsyncCallOutSize = SHELL_SVC_MAX_CALL_SIZE;
	config.eventFlagCount = SHELL_SVC_EVENT_FLAG_COUNT;
	sceClibStrncpy(config.serviceName, SHELL_SVC_SERVICE_NAME, sizeof(config.serviceName));
	IPMI_ASSERT(IPMI::Client::getBufferSize(&config) <= sizeof(s_clientBuffer));
	ret = IPMI::Client::create(&m_pClient, &config, this, s_clientBuffer);
	if (ret != 0)
		return ret;
	m_connected = false;
	connect();
	return 0;
}

__attribute__((noinline)) int ShellSvcClient::destroy()
{
	int ret;

	if (m_pClient == NULL)
		return SHELL_SVC_ERROR_NOT_CREATED;
	if (!m_connected)
		return 0;
	ret = m_pClient->disconnect();
	if (ret == IPMI_ERROR_BUSY)
		return SHELL_SVC_ERROR_ALREADY_EXISTS;
	if (ret != 0)
		return ret;
	m_connected = false;
	return m_pClient->destroy();
}

/* module_start's: create the client (connecting is retried on use). */
int SceShellSvc_E91AEAD4(void)
{
	if (s_created)
		return 0;
	s_created = g_shellSvcClient.create() == 0;
	return 0;
}

int shellSvcFinalize(void)
{
	if (s_created) {
		g_shellSvcClient.destroy();
		s_created = false;
	}
	return 0;
}

/*
 * Two wrappers nothing refers to, each passing its arguments and a mode (0 or 1) to a weak
 * function the library is linked without, so each returns 9. The names are ours.
 */
extern "C" int shellSvcWeakHandler(int arg0, int arg1, int arg2, int mode) __attribute__((weak));

int shellSvcWeakCall0(int arg0, int arg1, int arg2)
{
	if (shellSvcWeakHandler != NULL)
		return shellSvcWeakHandler(arg0, arg1, arg2, 0);
	return 9;
}

int shellSvcWeakCall1(int arg0, int arg1, int arg2)
{
	if (shellSvcWeakHandler != NULL)
		return shellSvcWeakHandler(arg0, arg1, arg2, 1);
	return 9;
}
