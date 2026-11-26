/*
 * np_push_dispatcher.cpp: Central push packet dispatcher routing incoming WebSocket
 * packets to IPMI client processes and in-process SceShell notification/comm callbacks.
 * Sony path: vsh/app/common/src/np/np_push_vsh/service/np_push_dispatcher.cpp
 * 3.65-CEX: 0x813B24C0..0x813B4243
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

static Dispatcher *s_dispatcherInstance = NULL;

/* 0x813B259E: InitInstance */
int Dispatcher::InitInstance(void)
{
	NP_PUSH_ASSERT(s_dispatcherInstance == NULL, "InitInstance",
		       "service\\np_push_dispatcher.cpp", 68, "s_dispatcherInstance == NULL");

	void *rawMem = SceNpCommonPs4_0C922423(sizeof(Dispatcher), GetAllocator());
	if (rawMem == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	Dispatcher *instance = new (rawMem) Dispatcher();
	int ret = instance->Init();
	if (ret < 0) {
		instance->~Dispatcher();
		SceNpCommonPs4_6B428829(GetAllocator(), instance);
		return ret;
	}

	s_dispatcherInstance = instance;
	return 0;
}

/* 0x813B264E: TermInstance */
void Dispatcher::TermInstance(void)
{
	if (s_dispatcherInstance == NULL)
		return;

	s_dispatcherInstance->Finalize();
	s_dispatcherInstance->~Dispatcher();
	SceNpCommonPs4_6B428829(GetAllocator(), s_dispatcherInstance);
	s_dispatcherInstance = NULL;
}

/* 0x813B2670: GetInstance */
Dispatcher *Dispatcher::GetInstance(void)
{
	return s_dispatcherInstance;
}

/* 0x813B2688 */
Dispatcher::Dispatcher()
	: m_nextCallbackHandle(1), m_initialized(false)
{
	SceNpCommonPs4_4266D870(&m_mutex);
}

/* 0x813B40EC */
Dispatcher::~Dispatcher()
{
	Finalize();
	SceNpCommonPs4_294F0B1A(&m_mutex);
}

/* 0x813B2750: Init */
int Dispatcher::Init(void)
{
	NP_PUSH_ASSERT(!m_initialized, "Init",
		       "service\\np_push_dispatcher.cpp", 118, "!m_initialized");

	int ret = SceNpCommonPs4_8AA7E995(&m_mutex, "SceNpPushDispatcher", 1);
	if (ret < 0)
		return ret;

	m_ipcProcesses.SetAllocator(GetAllocator());
	m_callbacks.SetAllocator(GetAllocator());

	Ctl *ctl = Ctl::GetInstance();
	if (ctl != NULL)
		ctl->RegisterHandler(this);

	m_initialized = true;
	return 0;
}

/* 0x813B2850: Finalize */
void Dispatcher::Finalize(void)
{
	if (!m_initialized)
		return;

	Ctl *ctl = Ctl::GetInstance();
	if (ctl != NULL)
		ctl->RegisterHandler(NULL);

	{
		ScopedLock lock(&m_mutex);
		for (ListNode<IpcProcessContext *> *iter = m_ipcProcesses.Begin();
		     iter != m_ipcProcesses.End(); iter = iter->next) {
			IpcProcessContext *procCtx = iter->value;
			if (procCtx != NULL) {
				procCtx->~IpcProcessContext();
				SceNpCommonPs4_6B428829(GetAllocator(), procCtx);
			}
		}
		m_ipcProcesses.Clear();
		m_callbacks.Clear();
	}

	SceNpCommonPs4_63EED478(&m_mutex);
	m_initialized = false;
}

/* 0x813B29E0: NewIpcProcessContext */
int Dispatcher::NewIpcProcessContext(IPMI::Session *session, IpcProcessContext **outContext)
{
	NP_PUSH_ASSERT(session != NULL, "NewIpcProcessContext",
		       "service\\np_push_dispatcher.cpp", 174, "session != NULL");

	ScopedLock lock(&m_mutex);
	if (FindIpcProcessContext(session) != NULL)
		return SCE_NP_PUSH_ERROR_ALREADY_EXISTS;

	void *rawMem = SceNpCommonPs4_0C922423(sizeof(IpcProcessContext), GetAllocator());
	if (rawMem == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	IpcProcessContext *procCtx = new (rawMem) IpcProcessContext(session);
	if (!m_ipcProcesses.PushBack(procCtx)) {
		procCtx->~IpcProcessContext();
		SceNpCommonPs4_6B428829(GetAllocator(), procCtx);
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	}

	if (outContext != NULL)
		*outContext = procCtx;
	return 0;
}

/* 0x813B2B40: DeleteIpcProcessContext */
int Dispatcher::DeleteIpcProcessContext(IPMI::Session *session)
{
	if (session == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	ScopedLock lock(&m_mutex);
	for (ListNode<IpcProcessContext *> *iter = m_ipcProcesses.Begin();
	     iter != m_ipcProcesses.End(); iter = iter->next) {
		if (iter->value->GetSession() == session) {
			IpcProcessContext *procCtx = iter->value;
			m_ipcProcesses.Erase(iter);
			procCtx->~IpcProcessContext();
			SceNpCommonPs4_6B428829(GetAllocator(), procCtx);
			return 0;
		}
	}
	return SCE_NP_PUSH_ERROR_NOT_FOUND;
}

IpcProcessContext *Dispatcher::FindIpcProcessContext(IPMI::Session *session)
{
	for (ListNode<IpcProcessContext *> *iter = m_ipcProcesses.Begin();
	     iter != m_ipcProcesses.End(); iter = iter->next) {
		if (iter->value->GetSession() == session)
			return iter->value;
	}
	return NULL;
}

/* 0x813B30DE: RegisterNotificationCallback */
int Dispatcher::RegisterNotificationCallback(const char *dataType, FilterMatchType matchType,
					     PushNotificationCallback callback, void *userContext,
					     int *outHandle)
{
	if (dataType == NULL || callback == NULL || outHandle == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	ScopedLock lock(&m_mutex);
	InternalCallbackEntry entry;
	sceClibMemset(&entry, 0, sizeof(entry));
	entry.handleId = m_nextCallbackHandle++;
	entry.isCommCallback = false;
	entry.matchType = matchType;
	sceClibStrncpy(entry.dataType, dataType, sizeof(entry.dataType) - 1);
	entry.notificationCb = callback;
	entry.userContext = userContext;

	if (!m_callbacks.PushBack(entry))
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	*outHandle = entry.handleId;
	return 0;
}

/* 0x813B31D2: RegisterCommCallback */
int Dispatcher::RegisterCommCallback(const char *dataType, FilterMatchType matchType,
				     PushCommCallback callback, void *userContext,
				     int *outHandle)
{
	if (dataType == NULL || callback == NULL || outHandle == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	ScopedLock lock(&m_mutex);
	InternalCallbackEntry entry;
	sceClibMemset(&entry, 0, sizeof(entry));
	entry.handleId = m_nextCallbackHandle++;
	entry.isCommCallback = true;
	entry.matchType = matchType;
	sceClibStrncpy(entry.dataType, dataType, sizeof(entry.dataType) - 1);
	entry.commCb = callback;
	entry.userContext = userContext;

	if (!m_callbacks.PushBack(entry))
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	*outHandle = entry.handleId;
	return 0;
}

/* 0x813B32C6: UnregisterCallback */
int Dispatcher::UnregisterCallback(int handleId)
{
	ScopedLock lock(&m_mutex);
	for (ListNode<InternalCallbackEntry> *iter = m_callbacks.Begin();
	     iter != m_callbacks.End(); iter = iter->next) {
		if (iter->value.handleId == handleId) {
			m_callbacks.Erase(iter);
			return 0;
		}
	}
	return SCE_NP_PUSH_ERROR_NOT_FOUND;
}

/* Serialize an embedded JSON DOM subtree (body or extendedData) into a freshly allocated buffer */
int Dispatcher::SerializeJsonValue(NpPushJsonValue *value, char **outBuffer, SceSize *outSize)
{
	*outBuffer = NULL;
	*outSize = 0;
	if (value == NULL)
		return 0;

	NpPushJsonSerializer serializer;
	SceNpCommonPs4_4A1E54C6(&serializer, GetAllocator());
	serializer.root = value;

	SceSize needed = 0;
	int ret = SceNpCommonPs4_4A94BDE6(&serializer, NULL, 0, &needed);
	if (ret >= 0 && needed > 0) {
		char *buf = static_cast<char *>(
			SceNpCommonPs4_AA980113(GetAllocator(), needed + 1));
		if (buf == NULL) {
			ret = SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
		} else {
			SceSize written = 0;
			ret = SceNpCommonPs4_4A94BDE6(&serializer, buf, needed + 1, &written);
			if (ret >= 0) {
				buf[written] = '\0';
				*outBuffer = buf;
				*outSize = written;
			} else {
				SceNpCommonPs4_6B428829(GetAllocator(), buf);
			}
		}
	}

	serializer.root = NULL;
	SceNpCommonPs4_74F1FA26(&serializer);
	return ret;
}

/* 0x813B377C: OnCtlReceivedPacket */
void Dispatcher::OnCtlReceivedPacket(const RefPtr<Packet> &packet)
{
	if (packet.IsNull())
		return;

	int method = PACKET_METHOD_NONE;
	if (packet->GetMethod(&method) < 0)
		return;

	if (method == PACKET_METHOD_NOTIFICATION) {
		ReceiveNotificationPacket(packet);
	} else if (method == PACKET_METHOD_SERVICE_COMM) {
		if (packet->HasQuery())
			ReceiveServiceQueryRequestPacket(packet);
		else
			ReceiveServiceCommunicationPacket(packet);
	}
}

void Dispatcher::OnCtlDisconnected(int reason)
{
	(void)reason;
}

/* 0x813B3A02: ReceiveNotificationPacket */
int Dispatcher::ReceiveNotificationPacket(const RefPtr<Packet> &packet)
{
	ScopedLock lock(&m_mutex);

	/* Deliver to IPMI client processes */
	for (ListNode<IpcProcessContext *> *iter = m_ipcProcesses.Begin();
	     iter != m_ipcProcesses.End(); iter = iter->next) {
		if (packet->HasDataType())
			iter->value->ReceiveNotificationDataTypePacket(packet);
		iter->value->ReceiveUserNotificationPacket(packet);
	}

	/* Deliver to registered in-process shell notification callbacks */
	if (!packet->HasDataType())
		return 0;

	char dataType[64];
	sceClibMemset(dataType, 0, sizeof(dataType));
	if (packet->GetDataType(dataType, sizeof(dataType)) < 0)
		return 0;

	NpPushOnlineIdData fromId;
	NpPushOnlineIdData toId;
	sceClibMemset(&fromId, 0, sizeof(fromId));
	sceClibMemset(&toId, 0, sizeof(toId));
	packet->GetFromOnlineId(&fromId);
	packet->GetToOnlineId(&toId);

	char *bodyJson = NULL;
	SceSize bodySize = 0;
	SerializeJsonValue(packet->GetBodyData(), &bodyJson, &bodySize);

	for (ListNode<InternalCallbackEntry> *iter = m_callbacks.Begin();
	     iter != m_callbacks.End(); iter = iter->next) {
		const InternalCallbackEntry &cb = iter->value;
		if (cb.isCommCallback || cb.notificationCb == NULL)
			continue;

		bool matched = false;
		if (cb.matchType == FILTER_MATCH_EXACT)
			matched = packet->MatchDataTypeExact(cb.dataType);
		else if (cb.matchType == FILTER_MATCH_PREFIX)
			matched = packet->MatchDataTypePrefix(cb.dataType);

		if (matched) {
			cb.notificationCb(dataType, fromId.text, toId.text,
					  bodyJson, bodySize, cb.userContext);
		}
	}

	if (bodyJson != NULL)
		SceNpCommonPs4_6B428829(GetAllocator(), bodyJson);
	return 0;
}

/* 0x813B3CC4: ReceiveServiceCommunicationPacket */
int Dispatcher::ReceiveServiceCommunicationPacket(const RefPtr<Packet> &packet)
{
	NP_PUSH_ASSERT(!packet.IsNull(), "ReceiveServiceCommunicationPacket",
		       "service\\np_push_dispatcher.cpp", 464, "packet != NULL");

	ScopedLock lock(&m_mutex);
	if (!packet->HasDataType())
		return 0;

	char dataType[64];
	sceClibMemset(dataType, 0, sizeof(dataType));
	if (packet->GetDataType(dataType, sizeof(dataType)) < 0)
		return 0;

	NpPushOnlineIdData fromId;
	NpPushOnlineIdData toId;
	sceClibMemset(&fromId, 0, sizeof(fromId));
	sceClibMemset(&toId, 0, sizeof(toId));
	packet->GetFromOnlineId(&fromId);
	packet->GetToOnlineId(&toId);

	char *bodyJson = NULL;
	SceSize bodySize = 0;
	char *extJson = NULL;
	SceSize extSize = 0;
	SerializeJsonValue(packet->GetBodyData(), &bodyJson, &bodySize);
	SerializeJsonValue(packet->GetExtendedData(), &extJson, &extSize);

	for (ListNode<InternalCallbackEntry> *iter = m_callbacks.Begin();
	     iter != m_callbacks.End(); iter = iter->next) {
		const InternalCallbackEntry &cb = iter->value;
		if (!cb.isCommCallback || cb.commCb == NULL)
			continue;

		bool matched = false;
		if (cb.matchType == FILTER_MATCH_EXACT)
			matched = packet->MatchDataTypeExact(cb.dataType);
		else if (cb.matchType == FILTER_MATCH_PREFIX)
			matched = packet->MatchDataTypePrefix(cb.dataType);

		if (matched) {
			cb.commCb(dataType, NULL, fromId.text, toId.text,
				  bodyJson, bodySize, extJson, extSize, cb.userContext);
		}
	}

	if (bodyJson != NULL)
		SceNpCommonPs4_6B428829(GetAllocator(), bodyJson);
	if (extJson != NULL)
		SceNpCommonPs4_6B428829(GetAllocator(), extJson);
	return 0;
}

/* 0x813B408E: ReceiveServiceQueryRequestPacket */
int Dispatcher::ReceiveServiceQueryRequestPacket(const RefPtr<Packet> &packet)
{
	NP_PUSH_ASSERT(!packet.IsNull(), "ReceiveServiceQueryRequestPacket",
		       "service\\np_push_dispatcher.cpp", 542, "packet != NULL");

	ScopedLock lock(&m_mutex);
	char dataType[64];
	char queryType[64];
	sceClibMemset(dataType, 0, sizeof(dataType));
	sceClibMemset(queryType, 0, sizeof(queryType));
	packet->GetDataType(dataType, sizeof(dataType));
	packet->GetQueryType(queryType, sizeof(queryType));

	NpPushOnlineIdData fromId;
	NpPushOnlineIdData toId;
	sceClibMemset(&fromId, 0, sizeof(fromId));
	sceClibMemset(&toId, 0, sizeof(toId));
	packet->GetFromOnlineId(&fromId);
	packet->GetToOnlineId(&toId);

	char *bodyJson = NULL;
	SceSize bodySize = 0;
	char *extJson = NULL;
	SceSize extSize = 0;
	SerializeJsonValue(packet->GetBodyData(), &bodyJson, &bodySize);
	SerializeJsonValue(packet->GetExtendedData(), &extJson, &extSize);

	for (ListNode<InternalCallbackEntry> *iter = m_callbacks.Begin();
	     iter != m_callbacks.End(); iter = iter->next) {
		const InternalCallbackEntry &cb = iter->value;
		if (!cb.isCommCallback || cb.commCb == NULL)
			continue;

		bool matched = false;
		if (cb.matchType == FILTER_MATCH_EXACT)
			matched = packet->MatchDataTypeExact(cb.dataType);
		else if (cb.matchType == FILTER_MATCH_PREFIX)
			matched = packet->MatchDataTypePrefix(cb.dataType);

		if (matched) {
			cb.commCb(dataType, queryType, fromId.text, toId.text,
				  bodyJson, bodySize, extJson, extSize, cb.userContext);
		}
	}

	if (bodyJson != NULL)
		SceNpCommonPs4_6B428829(GetAllocator(), bodyJson);
	if (extJson != NULL)
		SceNpCommonPs4_6B428829(GetAllocator(), extJson);
	return 0;
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
