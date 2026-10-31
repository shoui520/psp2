#ifndef SHELL_SVC_CLIENT_H
#define SHELL_SVC_CLIENT_H

#include <kernel.h>
#include "../ipmi/client.h"

/*
 * The process's connection to the shell's "SceShellSvc" IPMI service, made on first use.
 * Its event flags carry the shell's events: words 0-3 (events 0-127) are the ones a
 * process registers callbacks for, words 4-7 the ones it waits on. The class and its
 * names are ours (Sony's files: shell_svc_client.cpp, shell_svc_ipc_client.cpp).
 */

/* Our names for SceShellSvc's error codes (0x801009xx). */
#define SHELL_SVC_ERROR_ALREADY_EXISTS		-2146432767	/* 0x80100901 */
#define SHELL_SVC_ERROR_NOT_CREATED		-2146432766	/* 0x80100902 */
#define SHELL_SVC_ERROR_TIMEOUT			-2146432764	/* 0x80100904 */
#define SHELL_SVC_ERROR_INVALID_EVENT		-2146432752	/* 0x80100910 */
#define SHELL_SVC_ERROR_SERVER_NOT_FOUND	-2146432671	/* 0x80100961 */
#define SHELL_SVC_ERROR_NO_CALLBACK		-2146432670	/* 0x80100962 */
#define SHELL_SVC_ERROR_EVENT_NOT_SET		-2146432669	/* 0x80100963 */
#define SHELL_SVC_ERROR_INVALID_WAIT_EVENT	-2146432656	/* 0x80100970 */
#define SHELL_SVC_ERROR_UNAVAILABLE		-2146432513	/* 0x801009FF */

#define SHELL_SVC_EVENT_COUNT		128
/* The first event flag word of the events a process waits on. */
#define SHELL_SVC_WAIT_EVENT_FLAG	4
/* How every ShellSvc event flag is read: any bit of the pattern, clearing it. */
#define SHELL_SVC_EVENT_WAIT_MODE	(SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT)

typedef int (*ShellSvcEventCallback)(int event, void *pArg);

class ShellSvcClient {
public:
	IPMI::Client *m_pClient;
	bool m_connected;

	ShellSvcClient() : m_pClient(NULL) {}

	int create();
	int connect();
	int destroy();

	/* The client, connecting first; NULL while the service is unavailable. */
	IPMI::Client *getClient()
	{
		connect();
		return m_connected ? m_pClient : NULL;
	}
};

struct ShellSvcEventCallbackEntry {
	ShellSvcEventCallback callback;
	void *pArg;

	ShellSvcEventCallbackEntry() : callback(NULL), pArg(NULL) {}
};

extern ShellSvcClient g_shellSvcClient;

extern "C" {
IPMI::Client *SceShellSvc_B31E7F1C(void);
int SceShellSvc_1FAB9A1E(int event);
int SceShellSvc_D943CE15(int event, ShellSvcEventCallback callback, void *pArg);
int SceShellSvc_7CBF442B(int event);
int SceShellSvc_FB03FBC9(int event);
int SceShellSvc_A0B067AC(IPMI::EventFlagInfo *pEventFlag, int event);
int SceShellSvc_BD4161B0(int event, bool *pSet);
int SceShellSvc_8D4F9F2E(int event, SceUInt32 *pTimeout);
int SceShellSvc_B452A177(int event);
int SceShellSvc_E91AEAD4(void);

/* module_stop's: destroy the client SceShellSvc_E91AEAD4 created. */
int shellSvcFinalize(void);
}

#endif
