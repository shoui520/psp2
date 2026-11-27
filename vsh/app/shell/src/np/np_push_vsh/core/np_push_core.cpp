/*
 * np_push_core.cpp: SceNpPush VSH initialization, protocol version configuration,
 * and service callback registration entry points.
 * Sony path: vsh/app/common/src/np/np_push_vsh/core/np_push_core.cpp
 * 3.65-CEX: 0x813AB034..0x813AB343, 0x813AEF90..0x813AF123
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

static const ProtocolConfigEntry kProtocolConfigTable[] = {
	{ "e" "1-np",	0x00010001U, 0, 0x00010001U, 1 },
	{ "sp-int",	0x00010001U, 0, 0x00010001U, 1 },
	{ NULL,		0x00010001U, 0, 0x00010001U, 1 }
};

static SceUInt32 s_coreInitFlags = 0;
static SceUInt32 s_serviceInitFlags = 0;
static const ProtocolConfigEntry *s_activeProtocolConfig = NULL;

enum CoreInitBit {
	CORE_INIT_HEAP		= (1U << 0),
	CORE_INIT_NET		= (1U << 1),
	CORE_INIT_CALLOUT	= (1U << 2),
	CORE_INIT_CTL		= (1U << 3),
	CORE_INIT_PROTOCOL	= (1U << 4)
};

enum ServiceInitBit {
	SERVICE_INIT_DISPATCHER	= (1U << 0)
};

/* 0x813AB0A0: _sceNpPushInitProtocolConfig */
int InitProtocolConfig(void)
{
	char envName[20];
	sceClibMemset(envName, 0, sizeof(envName));

	int ret = sceNpCommonGetNpEnviroment(envName);
	if (ret < 0) {
		s_activeProtocolConfig = &kProtocolConfigTable[2];
		return 0;
	}

	const ProtocolConfigEntry *matched = NULL;
	for (SceSize index = 0; index < sizeof(kProtocolConfigTable) / sizeof(kProtocolConfigTable[0]);
	     ++index) {
		const ProtocolConfigEntry *candidate = &kProtocolConfigTable[index];
		if (candidate->envName == NULL) {
			matched = candidate;
			break;
		}
		if (sceClibStrncmp(envName, candidate->envName, sizeof(envName)) == 0) {
			matched = candidate;
			break;
		}
	}

	NP_PUSH_ASSERT(matched != NULL, "_sceNpPushInitProtocolConfig",
		       "core\\np_push_core.cpp", 104, "matched != NULL");
	s_activeProtocolConfig = matched;
	return 0;
}

/* 0x813AB266: GetServerAddrProtocolVersion */
int GetServerAddrProtocolVersion(ProtocolVersion *outVersion)
{
	if (outVersion == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;
	if (s_activeProtocolConfig == NULL)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;

	*outVersion = ProtocolVersion(s_activeProtocolConfig->serverAddrVersion,
				      s_activeProtocolConfig->serverAddrUseTitleToken);
	return 0;
}

/* 0x813AB2D4: GetWebSocketProtocolVersion */
int GetWebSocketProtocolVersion(ProtocolVersion *outVersion)
{
	if (outVersion == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;
	if (s_activeProtocolConfig == NULL)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;

	*outVersion = ProtocolVersion(s_activeProtocolConfig->webSocketVersion,
				      s_activeProtocolConfig->webSocketUseTitleToken);
	return 0;
}

/* 0x813AF09C: InitService */
int InitService(void)
{
	if (s_serviceInitFlags != 0)
		return SCE_NP_PUSH_ERROR_ALREADY_INITIALIZED;

	int ret = Dispatcher::InitInstance();
	if (ret < 0) {
		if ((s_serviceInitFlags & SERVICE_INIT_DISPATCHER) != 0)
			Dispatcher::TermInstance();
		s_serviceInitFlags = 0;
		return ret;
	}

	s_serviceInitFlags |= SERVICE_INIT_DISPATCHER;
	return 0;
}

/* 0x813AF0F4: TermService */
int TermService(void)
{
	if (s_serviceInitFlags == 0)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;

	if ((s_serviceInitFlags & SERVICE_INIT_DISPATCHER) != 0)
		Dispatcher::TermInstance();
	s_serviceInitFlags = 0;
	return 0;
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */

/* 0x813AB034: sceNpPushInit */
int sceNpPushInit(void)
{
	using namespace sce::Np::Push;

	if (s_coreInitFlags != 0)
		return SCE_NP_PUSH_ERROR_ALREADY_INITIALIZED;

	int ret = InitHeap();
	if (ret >= 0) {
		s_coreInitFlags |= CORE_INIT_HEAP;
		ret = InitProtocolConfig();
	}
	if (ret >= 0) {
		s_coreInitFlags |= CORE_INIT_PROTOCOL;
		ret = InitNet();
	}
	if (ret >= 0) {
		s_coreInitFlags |= CORE_INIT_NET;
		ret = InitCallout();
	}
	if (ret >= 0) {
		s_coreInitFlags |= CORE_INIT_CALLOUT;
		ret = Ctl::InitInstance();
	}
	if (ret >= 0) {
		s_coreInitFlags |= CORE_INIT_CTL;
		return 0;
	}

	if ((s_coreInitFlags & CORE_INIT_CTL) != 0)
		Ctl::TermInstance();
	if ((s_coreInitFlags & CORE_INIT_CALLOUT) != 0)
		TermCallout();
	if ((s_coreInitFlags & CORE_INIT_NET) != 0)
		TermNet();
	if ((s_coreInitFlags & CORE_INIT_HEAP) != 0)
		TermHeap();
	s_activeProtocolConfig = NULL;
	s_coreInitFlags = 0;
	return ret;
}

/* 0x813AB1B8: sceNpPushTerm */
int sceNpPushTerm(void)
{
	using namespace sce::Np::Push;

	if (s_coreInitFlags == 0)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;

	if ((s_coreInitFlags & CORE_INIT_CTL) != 0)
		Ctl::TermInstance();
	if ((s_coreInitFlags & CORE_INIT_CALLOUT) != 0)
		TermCallout();
	if ((s_coreInitFlags & CORE_INIT_NET) != 0)
		TermNet();
	if ((s_coreInitFlags & CORE_INIT_HEAP) != 0)
		TermHeap();

	s_activeProtocolConfig = NULL;
	s_coreInitFlags = 0;
	return 0;
}

/* 0x813AB224: sceNpPushStart */
int sceNpPushStart(void)
{
	using namespace sce::Np::Push;

	Ctl *ctl = Ctl::GetInstance();
	if (ctl == NULL)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;

	int ret = ctl->Start();
	if (ret >= 0)
		return 0;
	return ret;
}

/* 0x813AEFFC: Register internal notification callback */
int shellNpPushRegisterService(const char *servicePrefix, int matchMode,
			       void *callback, void *userContext, int *outHandle)
{
	using namespace sce::Np::Push;

	Dispatcher *dispatcher = Dispatcher::GetInstance();
	if (dispatcher == NULL)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;

	int ret = dispatcher->RegisterNotificationCallback(
		servicePrefix, static_cast<FilterMatchType>(matchMode),
		reinterpret_cast<PushNotificationCallback>(callback), userContext, outHandle);
	if (ret >= 0)
		return 0;
	return ret;
}

/* 0x813AF03A: Register internal service comm / query callback */
int shellNpPushRegisterTitle(const char *servicePrefix, int matchMode,
			     void *callback, void *userContext, int *outHandle)
{
	using namespace sce::Np::Push;

	Dispatcher *dispatcher = Dispatcher::GetInstance();
	if (dispatcher == NULL)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;

	int ret = dispatcher->RegisterCommCallback(
		servicePrefix, static_cast<FilterMatchType>(matchMode),
		reinterpret_cast<PushCommCallback>(callback), userContext, outHandle);
	if (ret >= 0)
		return 0;
	return ret;
}

/* 0x813AF078: Unregister internal push callback */
void shellNpPushUnregister(int handle)
{
	using namespace sce::Np::Push;

	Dispatcher *dispatcher = Dispatcher::GetInstance();
	if (dispatcher == NULL)
		return;

	dispatcher->UnregisterCallback(handle);
}
