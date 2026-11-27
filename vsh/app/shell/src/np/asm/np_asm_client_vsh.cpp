/*
 * np_asm_client_vsh.cpp: SceNpAsmClientVsh service state, active game title tracking,
 * suspended-feature notice, and IPMI method dispatcher (0x30000..0x30005).
 * 3.65-CEX: 0x813C0C4C - 0x813C12C7
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <appmgr/appmgr.h>
#include <ipmi/session.h>
#include <ipmi/ipmi_error.h>
#include <plugin.h>
#include <versatileparam/idparam.h>

#include "../../../plugins.h"

#define NP_ASM_ERROR_ALREADY_INITIALIZED	(-2141872831)	/* 0x80559D41 */
#define NP_ASM_ERROR_OUT_OF_MEMORY		(-2141872827)	/* 0x80559D45 */
#define NP_ASM_ERROR_TITLE_NOT_FOUND		(-2141872767)	/* 0x80559D81 */
#define NP_ASM_IPMI_ERROR_UNSUPPORTED_METHOD	(-2147351103)	/* 0x800205C1 */

#define NP_ASM_TITLE_ID_SIZE			16
#define NP_ASM_BUDGET_TYPE_GAME			1
#define NP_ASM_APPMGR_PARAM_CATEGORY		8

enum NpAsmIpcMethod {
	NP_ASM_IPC_METHOD_NOTIFY_SUSPENDED	= 0x30000,
	NP_ASM_IPC_METHOD_GET_SERVICE_URL	= 0x30001,
	NP_ASM_IPC_METHOD_PUT_SERVICE_URL	= 0x30002,
	NP_ASM_IPC_METHOD_SET_ACTIVE_TITLE	= 0x30003,
	NP_ASM_IPC_METHOD_CLEAR_ACTIVE_TITLE	= 0x30004,
	NP_ASM_IPC_METHOD_GET_ACTIVE_TITLE	= 0x30005
};

struct NpCommonPs4Allocator;
struct NpCommonPs4AllocatorWithError;

class NpCommonPs4Mutex {
public:
	virtual ~NpCommonPs4Mutex();
	SceUInt32 handleWords[2];
};

struct NpCommonPs4StringHolder {
	const void *vtable;
	NpCommonPs4AllocatorWithError *allocator;
	char *text;
};

extern "C" {
extern void *SceNpCommonPs4_0C922423(SceSize size, NpCommonPs4Allocator *allocator);
extern NpCommonPs4Mutex *SceNpCommonPs4_4266D870(NpCommonPs4Mutex *mutex);
extern int SceNpCommonPs4_8AA7E995(NpCommonPs4Mutex *mutex, const char *name, char recursive);
extern int SceNpCommonPs4_63EED478(NpCommonPs4Mutex *mutex);
extern int SceNpCommonPs4_A2A8493B(NpCommonPs4Mutex *mutex);
extern int SceNpCommonPs4_6BA5A164(NpCommonPs4Mutex *mutex);
extern NpCommonPs4AllocatorWithError *SceNpCommonPs4_03C9ACE2(void);
extern NpCommonPs4StringHolder *SceNpCommonPs4_FDA863C0(NpCommonPs4StringHolder *holder,
							NpCommonPs4AllocatorWithError *allocator);
extern const char *SceNpCommonPs4_2CE76BE5(const NpCommonPs4StringHolder *holder);
extern NpCommonPs4StringHolder *SceNpCommonPs4_483168C0(NpCommonPs4StringHolder *holder);
extern int sceAppMgrAppParamGetString(SceUID pid, int paramId, char *outString, SceSize maxLen);
}

extern int shellIncomingDialogServerAsyncDispatch(IPMI::Session *session, SceUInt32 requestId,
						  SceUInt32 methodId, const IPMI::DataInfo *inList,
						  SceUInt32 inCount); /* 0x8115B554 */
struct NpPushStringHolder;
extern int npAsmServiceCacheGet(const char *titleId, const char *serviceName,
				NpPushStringHolder *outUrl); /* 0x813B9826 */
extern int npAsmServiceCachePut(const char *titleId, const char *serviceName,
				const char *url); /* 0x813B983E */

static SceUInt8 s_npAsmInitialized = 0;		/* 0x815492FC */
static SceUID s_activeGameAppId = -1;		/* 0x81549300 */
static char s_activeGameTitleId[NP_ASM_TITLE_ID_SIZE]; /* 0x81549304 */
static NpCommonPs4Mutex *s_npAsmMutex = NULL;	/* 0x81549314 */
static SceUInt8 s_npAsmIpcInitialized = 0;	/* 0x81549318 */

/* 0x813C0C4C */
int npAsmVshInit(NpCommonPs4Allocator *allocator)
{
	if (s_npAsmInitialized != 0)
		return NP_ASM_ERROR_ALREADY_INITIALIZED;

	NpCommonPs4Mutex *mutex = NULL;
	void *rawMutex = SceNpCommonPs4_0C922423(sizeof(NpCommonPs4Mutex), allocator);
	if (rawMutex != NULL)
		mutex = SceNpCommonPs4_4266D870(static_cast<NpCommonPs4Mutex *>(rawMutex));

	s_npAsmMutex = mutex;
	int ret;
	if (mutex != NULL) {
		ret = SceNpCommonPs4_8AA7E995(mutex, "SceNpAsmClientVsh", 1);
		if (ret >= 0) {
			s_npAsmInitialized = 1;
			return ret;
		}
	} else {
		ret = NP_ASM_ERROR_OUT_OF_MEMORY;
	}

	if (s_npAsmMutex != NULL) {
		delete s_npAsmMutex;
		s_npAsmMutex = NULL;
	}
	return ret;
}

/* 0x813C0CDE */
void npAsmVshTerm(void)
{
	if (s_npAsmInitialized == 0)
		return;

	SceNpCommonPs4_63EED478(s_npAsmMutex);
	if (s_npAsmMutex != NULL) {
		delete s_npAsmMutex;
		s_npAsmMutex = NULL;
	}
	s_npAsmInitialized = 0;
}

/* 0x813C0D1C */
static void npAsmVshShowSuspendedNotice(void)
{
	ImposeNotice notice;
	paf::Plugin *imposePlugin = paf::Plugin::Find("impose_plugin");
	if (imposePlugin == NULL)
		return;

	ImposePluginInterface *imposeIface =
		static_cast<ImposePluginInterface *>(imposePlugin->GetInterface(1));
	if (imposeIface == NULL)
		return;

	notice.name = "SceNpAsmClient";
	paf::IDParam msgId("msg_network_feature_suspended_short");
	notice.message = imposeIface->getString(msgId);
	imposeIface->showNotice(&notice);
}

/* 0x813C0E64 */
static bool npAsmVshIsGameAppActive(SceUID appId)
{
	char category[4];
	SceAppMgrAppStatus appStatus;

	sceClibMemset(category, 0, sizeof(category));
	sceClibMemset(&appStatus, 0, sizeof(appStatus));

	if (sceAppMgrGetStatusByAppId(appId, &appStatus) < 0)
		return false;
	if (sceAppMgrAppParamGetString(appStatus.processId, NP_ASM_APPMGR_PARAM_CATEGORY,
				       category, sizeof(category)) < 0)
		return false;

	SceSize categoryLen = sceClibStrnlen(category, (SceSize)-1);
	if (sceClibStrncmp(category, "gd", categoryLen) != 0)
		return false;

	return appStatus.budgetType == NP_ASM_BUDGET_TYPE_GAME;
}

/* 0x813C0EEA */
static int npAsmVshRegisterActiveTitleId(SceUID appId, const char *titleId)
{
	NpCommonPs4Mutex *mutex = s_npAsmMutex;
	SceNpCommonPs4_A2A8493B(mutex);
	if (npAsmVshIsGameAppActive(appId)) {
		s_activeGameAppId = appId;
		for (int i = 0; i < NP_ASM_TITLE_ID_SIZE; ++i)
			s_activeGameTitleId[i] = titleId[i];
	}
	SceNpCommonPs4_6BA5A164(mutex);
	return 0;
}

/* 0x813C0F86 */
static int npAsmVshGetActiveTitleId(SceUID appId, char *outTitleId)
{
	NpCommonPs4Mutex *mutex = s_npAsmMutex;
	SceNpCommonPs4_A2A8493B(mutex);

	SceNpCommonPs4_A2A8493B(mutex);
	bool matched = (appId != -1) && (appId == s_activeGameAppId);
	SceNpCommonPs4_6BA5A164(mutex);

	if (matched) {
		for (int i = 0; i < NP_ASM_TITLE_ID_SIZE; ++i)
			outTitleId[i] = s_activeGameTitleId[i];
		SceNpCommonPs4_6BA5A164(mutex);
		return 0;
	}

	SceNpCommonPs4_6BA5A164(mutex);
	return NP_ASM_ERROR_TITLE_NOT_FOUND;
}

/* 0x813C105A */
int npAsmVshUnregisterActiveTitleId(SceUID appId)
{
	NpCommonPs4Mutex *mutex = s_npAsmMutex;
	SceNpCommonPs4_A2A8493B(mutex);
	if (appId == s_activeGameAppId) {
		s_activeGameAppId = -1;
		sceClibMemset(s_activeGameTitleId, 0, sizeof(s_activeGameTitleId));
	}
	SceNpCommonPs4_6BA5A164(mutex);
	return 0;
}

/* 0x813C10C8 */
static int npAsmVshHandleNotifySuspended(void)
{
	npAsmVshShowSuspendedNotice();
	return 0;
}

/* 0x813C10D2 */
static int npAsmVshHandleGetServiceUrl(IPMI::Session *session, const char *titleId,
				       const char *serviceName, SceUInt8 *outFound,
				       char *outUrl, SceSize urlBufSize)
{
	(void)session;
	NpCommonPs4StringHolder urlHolder;
	NpCommonPs4AllocatorWithError *allocator = SceNpCommonPs4_03C9ACE2();
	SceNpCommonPs4_FDA863C0(&urlHolder, allocator);

	int found = npAsmServiceCacheGet(titleId, serviceName,
					 reinterpret_cast<NpPushStringHolder *>(&urlHolder));
	*outFound = static_cast<SceUInt8>(found);
	if (found != 0) {
		sceClibMemset(outUrl, 0, urlBufSize);
		const char *cachedUrl = SceNpCommonPs4_2CE76BE5(&urlHolder);
		sceClibStrncpy(outUrl, cachedUrl, urlBufSize);
	}

	SceNpCommonPs4_483168C0(&urlHolder);
	return 0;
}

/* 0x813C115A */
static int npAsmVshHandlePutServiceUrl(IPMI::Session *session, const char *titleId,
				       const char *serviceName, const char *url)
{
	(void)session;
	npAsmServiceCachePut(titleId, serviceName, url);
	return 0;
}

/* 0x813C116A */
static int npAsmVshHandleSetActiveTitle(IPMI::Session *session, const char *titleId)
{
	return npAsmVshRegisterActiveTitleId(session->getAppId(), titleId);
}

/* 0x813C117C */
static int npAsmVshHandleClearActiveTitle(IPMI::Session *session)
{
	return npAsmVshUnregisterActiveTitleId(session->getAppId());
}

/* 0x813C118A */
static int npAsmVshHandleGetActiveTitle(IPMI::Session *session, SceUID appId, char *outTitleId)
{
	(void)session;
	return npAsmVshGetActiveTitleId(appId, outTitleId);
}

/* 0x813C1198 */
int npAsmVshIpcInit(void)
{
	if (s_npAsmIpcInitialized != 0)
		return NP_ASM_ERROR_ALREADY_INITIALIZED;
	s_npAsmIpcInitialized = 1;
	return 0;
}

/* 0x813C11B6 */
void npAsmVshIpcTerm(void)
{
	s_npAsmIpcInitialized = 0;
}

/* 0x813C11C6 */
int npAsmVshIpcDispatchSync(IPMI::Session *session, SceUInt32 methodId,
			    const IPMI::DataInfo *inList, SceUInt32 inCount,
			    IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;

	switch (methodId) {
	case NP_ASM_IPC_METHOD_NOTIFY_SUSPENDED: {
		int status = npAsmVshHandleNotifySuspended();
		return session->respondToSyncMethodRequest(status, outList, outCount);
	}
	case NP_ASM_IPC_METHOD_GET_SERVICE_URL: {
		SceSize urlBufSize = *static_cast<const SceSize *>(inList[0].pData);
		int status = npAsmVshHandleGetServiceUrl(
			session,
			static_cast<const char *>(inList[1].pData),
			static_cast<const char *>(inList[2].pData),
			static_cast<SceUInt8 *>(outList[0].pBuffer),
			static_cast<char *>(outList[1].pBuffer),
			urlBufSize);
		outList[0].outSize = 1;
		outList[1].outSize = urlBufSize;
		return session->respondToSyncMethodRequest(status, outList, outCount);
	}
	case NP_ASM_IPC_METHOD_PUT_SERVICE_URL: {
		int status = npAsmVshHandlePutServiceUrl(
			session,
			static_cast<const char *>(inList[0].pData),
			static_cast<const char *>(inList[1].pData),
			static_cast<const char *>(inList[2].pData));
		return session->respondToSyncMethodRequest(status, outList, outCount);
	}
	case NP_ASM_IPC_METHOD_SET_ACTIVE_TITLE: {
		int status = npAsmVshHandleSetActiveTitle(
			session,
			static_cast<const char *>(inList[0].pData));
		return session->respondToSyncMethodRequest(status, outList, outCount);
	}
	case NP_ASM_IPC_METHOD_CLEAR_ACTIVE_TITLE: {
		int status = npAsmVshHandleClearActiveTitle(session);
		return session->respondToSyncMethodRequest(status, outList, outCount);
	}
	case NP_ASM_IPC_METHOD_GET_ACTIVE_TITLE: {
		SceUID appId = *static_cast<const SceUID *>(inList[0].pData);
		int status = npAsmVshHandleGetActiveTitle(
			session,
			appId,
			static_cast<char *>(outList[0].pBuffer));
		outList[0].outSize = NP_ASM_TITLE_ID_SIZE;
		return session->respondToSyncMethodRequest(status, outList, outCount);
	}
	default:
		return NP_ASM_IPMI_ERROR_UNSUPPORTED_METHOD;
	}
}

/* 0x813C10A4 */
int npAsmVshIpcOnSyncMethod(IPMI::Session *session, SceUInt32 methodId,
			    const IPMI::DataInfo *inList, SceUInt32 inCount,
			    IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	return npAsmVshIpcDispatchSync(session, methodId, inList, inCount, outList, outCount);
}

/* 0x813C10B8 */
int npAsmVshIpcOnAsyncMethod(IPMI::Session *session, SceUInt32 requestId,
			     SceUInt32 methodId, const IPMI::DataInfo *inList,
			     SceUInt32 inCount)
{
	return shellIncomingDialogServerAsyncDispatch(session, requestId, methodId, inList, inCount);
}
