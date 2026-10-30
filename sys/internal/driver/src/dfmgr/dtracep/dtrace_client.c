/*
 * dtrace_client.c: DTrace's clients, a debugger's connection or a process's
 * helper DOF. The first client attaches the providers, every client opens
 * them, and the last one to close detaches them. A client is closed when its
 * process exits or is killed, and a helper DOF's client when its module
 * unloads.
 */
#include <sys/dtrace.h>
#include <dtrace/logging.h>
#include <dtrace/sce/solaris_process.h>
#include <modulemgr/modulemgr.h>
#include <modulemgr/modulemgr_control.h>
#include <sysmem/debug.h>
#include <sysmem/procevent.h>
#include <sysmem/sysclib.h>
#include <sysmem/uid_guid.h>
#include <threadmgr/threadmgr.h>
#include <kernel/error.h>
#include <sceerror.h>
#include "dtrace_client.h"
#include "dtrace_client_list.h"

/* a provider's device: attached by the first client, opened by each */
typedef struct DtraceProvider {
	const char *name;
	dev_info_t devi;
	int refs;			/* the clients holding it open */
} DtraceProvider;

static DtraceProvider s_dtrace = { "/dev/dtrace/dtrace" };
static DtraceProvider s_sdt = { "/dev/dtrace/provider/sdt" };
static DtraceProvider s_irqtrace = { "/dev/dtrace/provider/interrupt" };
static DtraceProvider s_profile = { "/dev/dtrace/provider/profile" };
static int s_nextToken = 1;
static DtraceProvider s_fasttrap = { "/dev/dtrace/provider/fasttrap" };
static SceUID s_procEventId = -1;
static SceUID s_clientMutex = -1;
static DtraceProvider s_systrace = { "/dev/dtrace/provider/systrace" };

list_t *dtraceClients;

static int dtraceClientProcKill(SceUID pid, ProcEventParam *param, void *common);
static int dtraceClientProcExit(SceUID pid, ProcEventParam *param, void *common);
static void dtraceClientModuleStatus(SceModuleCB *cb, int status);

int dtraceClientInit(void)
{
	SceKernelProcEventHandler handler = {
		.size = sizeof(handler),
		.exit = dtraceClientProcExit,
		.kill = dtraceClientProcKill,
	};

	SceModulemgrForKernel_74A4E0EE(dtraceClientModuleStatus);

	s_procEventId = sceKernelRegisterProcEventHandler("SceDeci4pDtracep", &handler, NULL);
	if (s_procEventId < 0) {
		DTRACE_LOG_ERROR_AT(163, "failed to register dtrace process event handler (0x%08x)",
				    s_procEventId);
		s_procEventId = -1;
	}

	s_clientMutex = sceKernelCreateMutex("SceDeci4pDtracepClient", 0x2000, 0, NULL);
	if (s_clientMutex < 0) {
		DTRACE_LOG_ERROR_AT(169, "failed to create dtrace client mutex (0x%08x)", s_clientMutex);
		return s_clientMutex;
	}

	dtraceClients = dtraceCreateClientList();
	return dtraceClients != NULL ? SCE_OK : SCE_KERNEL_ERROR_NO_MEMORY;
}

int dtraceClientFini(void)
{
	if (dtraceClients != NULL) {
		dtraceDestroyClientList(dtraceClients);
		dtraceClients = NULL;
	}
	if (s_clientMutex != -1) {
		sceKernelDeleteMutex(s_clientMutex);
		s_clientMutex = -1;
	}
	if (s_procEventId != -1) {
		sceKernelUnregisterProcEventHandler(s_procEventId);
		s_procEventId = -1;
	}
	SceModulemgrForKernel_ACBC97C9(dtraceClientModuleStatus);
	return SCE_OK;
}

/* attach a provider for the first client */
#define DTRACE_PROVIDER_ATTACH(_prov, _attach, _assertLine, _logLine)		\
	if (!(_prov).devi.attached) {						\
		SCE_DBG_ASSERT((_prov).refs == 0, _assertLine);			\
		res = _attach(&(_prov).devi, DDI_ATTACH);			\
		if (res != 0) {							\
			(_prov).devi.attached = 0;				\
			DTRACE_LOG_ERROR_AT(_logLine, "(%d) %s attach failed: %d",	\
					    client->token, (_prov).name, res);	\
			res = SCE_ERROR_ERRNO_EACCES;				\
			goto failed;						\
		}								\
		(_prov).devi.attached = 1;					\
	}

/* open an attached provider for a client */
#define DTRACE_PROVIDER_OPEN(_prov, _open, _dev, _logLine)			\
	if ((_prov).devi.attached) {						\
		res = _open(_dev, 0, 0, NULL);					\
		if (res != 0) {							\
			DTRACE_LOG_ERROR_AT(_logLine, "(%d) %s open failed: %d",	\
					    client->token, (_prov).name, res);	\
			res = SCE_ERROR_ERRNO_EPERM;				\
			goto failed;						\
		}								\
		(_prov).refs++;							\
	}

int sceDTraceClientOpen(const char *path, SceUID processId, SceUID moduleId, int flags)
{
	DtraceClientCB *client;
	int res;

	if (sceKernelLockMutex(s_clientMutex, 1, NULL) < 0)
		SCE_DBG_ASSERT_FAIL(212);

	if (strcmp(path, "/dev/dtrace/dtrace") != 0 && strcmp(path, "/dev/dtrace/helper") != 0) {
		res = SCE_ERROR_ERRNO_ENODEV;
		DTRACE_LOG_ERROR_AT(439, "%s open/attach failed - no such device", path);
		goto out;
	}

	client = dtraceAllocClient(&s_dtrace.devi, processId, moduleId, flags, s_nextToken++);

	if (dtraceIsClientListEmpty(dtraceClients)) {
		DTRACE_PROVIDER_ATTACH(s_dtrace, dtrace_attach, 226, 234)
		DTRACE_PROVIDER_ATTACH(s_systrace, systrace_attach, 247, 255)
		DTRACE_PROVIDER_ATTACH(s_irqtrace, irqtrace_attach, 265, 273)
		DTRACE_PROVIDER_ATTACH(s_fasttrap, fasttrap_attach, 283, 291)
		DTRACE_PROVIDER_ATTACH(s_profile, profile_attach, 301, 309)
		DTRACE_PROVIDER_ATTACH(s_sdt, sdt_attach, 319, 327)
	}

	if (s_dtrace.devi.attached) {
		DTRACE_PROVIDER_OPEN(s_systrace, systrace_open, &client->systraceDev, 349)
		DTRACE_PROVIDER_OPEN(s_irqtrace, irqtrace_open, &client->irqtraceDev, 364)
		DTRACE_PROVIDER_OPEN(s_fasttrap, fasttrap_open, &client->fasttrapDev, 379)
		DTRACE_PROVIDER_OPEN(s_profile, profile_open, &client->profileDev, 394)
		DTRACE_PROVIDER_OPEN(s_sdt, sdt_open, &client->sdtDev, 409)
		res = dtrace_open(&client->dtraceDev, 0, 0, NULL);
		if (res != 0) {
			DTRACE_LOG_ERROR_AT(424, "(%d) %s open failed: %d",
					    client->token, s_dtrace.name, res);
			res = SCE_ERROR_ERRNO_EPERM;
			goto failed;
		}
		s_dtrace.refs++;
	}

	dtraceAddClient(dtraceClients, client);
	res = client->token;
	goto out;

failed:
	dtraceFreeClient(client);
out:
	if (sceKernelUnlockMutex(s_clientMutex, 1) < 0)
		SCE_DBG_ASSERT_FAIL(445);

	return res;
}

/* close a provider a client holds open */
#define DTRACE_PROVIDER_CLOSE(_prov, _close, _dev, _assertLine)		\
	if ((_prov).refs > 0) {							\
		SCE_DBG_ASSERT((_prov).devi.attached, _assertLine);		\
		_close(_dev, 0, 0, NULL);					\
		(_prov).refs--;							\
	}

/* detach a provider after the last client */
#define DTRACE_PROVIDER_DETACH(_prov, _detach, _assertLine)			\
	SCE_DBG_ASSERT((_prov).refs == 0, _assertLine);				\
	if ((_prov).devi.attached && _detach(&(_prov).devi, DDI_DETACH) == 0)	\
		(_prov).devi.attached = 0;

int sceDTraceClientClose(int token)
{
	DtraceClientCB *client;
	SceUID pid;
	proc_t *p;
	int gen;
	int res;

	if (sceKernelLockMutex(s_clientMutex, 1, NULL) < 0)
		SCE_DBG_ASSERT_FAIL(458);

	client = dtraceFindClientByToken(dtraceClients, token);
	if (client == NULL) {
		res = SCE_ERROR_ERRNO_EBADF;
		DTRACE_LOG_ERROR_AT(680, "(%d) close/detach failed - invalid file descriptor",
				    token);
		goto out;
	}

	gen = client->helperDofGen;
	if (gen >= 0) {
		p = proc_find(client->processId);
		if (p != NULL) {
			int rv = 0;

			res = dtrace_ioctl_helper(p, DTRACEHIOC_REMOVE, gen, &rv);
			if (res >= 0 && rv >= 0)
				client->helperDofGen = -1;
			proc_rele(p);
		}
	}

	DTRACE_PROVIDER_CLOSE(s_dtrace, dtrace_close, client->dtraceDev, 495)
	DTRACE_PROVIDER_CLOSE(s_sdt, sdt_close, client->sdtDev, 506)
	DTRACE_PROVIDER_CLOSE(s_profile, profile_close, client->profileDev, 517)
	DTRACE_PROVIDER_CLOSE(s_fasttrap, fasttrap_close, client->fasttrapDev, 528)
	DTRACE_PROVIDER_CLOSE(s_irqtrace, irqtrace_close, client->irqtraceDev, 539)
	DTRACE_PROVIDER_CLOSE(s_systrace, systrace_close, client->systraceDev, 550)

	dtraceRemoveClient(dtraceClients, client);

	/* the process's last client cleans up after it: in that process, or from the kernel */
	pid = sceKernelGetProcessId();
	if ((client->processId == pid ? client->processId != SCE_KERNEL_PROCESS_ID :
					pid == SCE_KERNEL_PROCESS_ID) &&
	    dtraceFindClientByProcessId(dtraceClients, client->processId) == NULL) {
		p = proc_find(client->processId);
		DTRACE_LOG_DEBUG_AT(572, 0, "cleaning up helpers in process 0x%08x",
				    client->processId);
		if (p->dtraceHelpers != NULL)
			dtrace_helpers_cleanup(p);
		DTRACE_LOG_DEBUG_AT(581, 0, "cleaning up probes in process 0x%08x",
				    client->processId);
		if (p->dtraceProbes != 0)
			dtrace_fasttrap_exit_ptr(p);
		proc_rele(p);
	}

	if (dtraceIsClientListEmpty(dtraceClients)) {
		DTRACE_PROVIDER_DETACH(s_sdt, sdt_detach, 594)
		DTRACE_PROVIDER_DETACH(s_profile, profile_detach, 608)
		DTRACE_PROVIDER_DETACH(s_fasttrap, fasttrap_detach, 622)
		DTRACE_PROVIDER_DETACH(s_irqtrace, irqtrace_detach, 636)
		DTRACE_PROVIDER_DETACH(s_systrace, systrace_detach, 650)
		DTRACE_PROVIDER_DETACH(s_dtrace, dtrace_detach, 664)
	}

	dtraceFreeClient(client);
	res = SCE_OK;
out:
	if (sceKernelUnlockMutex(s_clientMutex, 1) < 0)
		SCE_DBG_ASSERT_FAIL(686);

	return res;
}

static int dtraceClientProcKill(SceUID pid, ProcEventParam *param, void *common)
{
	DtraceClientCB *client = dtraceFindClientByProcessId(dtraceClients, pid);

	if (client != NULL && client->token != -1)
		sceDTraceClientClose(client->token);
	return SCE_OK;
}

static int dtraceClientProcExit(SceUID pid, ProcEventParam *param, void *common)
{
	DtraceClientCB *client = dtraceFindClientByProcessId(dtraceClients, pid);

	if (client != NULL && client->token != -1)
		sceDTraceClientClose(client->token);
	return SCE_OK;
}

static void dtraceClientModuleStatus(SceModuleCB *cb, int status)
{
	DtraceClientCB *client;

	if (status == MODULE_STATUS_STARTED) {
		dtrace_module_loaded((struct modctl *)cb);
	} else if (status == MODULE_STATUS_STOPPED) {
		dtrace_module_unloaded((struct modctl *)cb);
		client = dtraceFindClientByModuleId(dtraceClients, cb->pid, cb->modid);
		if (client != NULL && client->token != -1)
			sceDTraceClientClose(client->token);
	}
}
