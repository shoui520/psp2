/*
 * np_push_ipc_context.cpp: Per-process and per-user IPC notification queues and
 * DataType filter matching for IPMI clients of SceNpPush.
 * Sony path: vsh/app/common/src/np/np_push_vsh/service/np_push_ipc_context.cpp
 * 3.65-CEX: 0x813B4244..0x813B4C73
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

/* ============================================================================
 * IpcUserContext (0x813B4244..0x813B43A9)
 * ============================================================================ */

IpcUserContext::IpcUserContext(SceUInt32 userContextId, const NpPushOnlineId *onlineId)
	: m_userContextId(userContextId), m_packetQueue(GetAllocator())
{
	if (onlineId != NULL)
		SceNpCommonPs4_AEC0DAC5(&m_onlineId, onlineId);
	else
		SceNpCommonPs4_B0BAB9BE(&m_onlineId);
}

IpcUserContext::~IpcUserContext()
{
	m_packetQueue.Clear();
	SceNpCommonPs4_1543D7EE(&m_onlineId);
}

int IpcUserContext::EnqueuePacket(const RefPtr<Packet> &packet)
{
	if (packet.IsNull())
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;
	if (m_packetQueue.Size() >= NP_PUSH_MAX_IPC_QUEUED_PACKETS)
		return SCE_NP_PUSH_ERROR_QUEUE_FULL;
	if (!m_packetQueue.PushBack(packet))
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	return 0;
}

int IpcUserContext::DequeuePacket(RefPtr<Packet> *outPacket)
{
	if (outPacket == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;
	if (m_packetQueue.Empty())
		return SCE_NP_PUSH_ERROR_NO_DATA;

	*outPacket = m_packetQueue.Begin()->value;
	m_packetQueue.Erase(m_packetQueue.Begin());
	return 0;
}

int IpcUserContext::PeekPacket(RefPtr<Packet> *outPacket) const
{
	if (outPacket == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;
	if (m_packetQueue.Empty())
		return SCE_NP_PUSH_ERROR_NO_DATA;

	*outPacket = m_packetQueue.Begin()->value;
	return 0;
}

/* ============================================================================
 * IpcProcessContext (0x813B43AA..0x813B4C73)
 * ============================================================================ */

IpcProcessContext::IpcProcessContext(IPMI::Session *session)
	: m_session(session),
	  m_nextUserContextId(1),
	  m_userContexts(GetAllocator()),
	  m_filters(GetAllocator()),
	  m_dataTypeQueue(GetAllocator())
{
}

IpcProcessContext::~IpcProcessContext()
{
	for (ListNode<IpcUserContext *> *iter = m_userContexts.Begin();
	     iter != m_userContexts.End(); iter = iter->next) {
		IpcUserContext *userCtx = iter->value;
		if (userCtx != NULL) {
			userCtx->~IpcUserContext();
			SceNpCommonPs4_6B428829(GetAllocator(), userCtx);
		}
	}
	m_userContexts.Clear();
	m_filters.Clear();
	m_dataTypeQueue.Clear();
}

/* 0x813B44B0: CreateIpcUserContext */
int IpcProcessContext::CreateIpcUserContext(const NpPushOnlineId *onlineId,
					    SceUInt32 *outUserContextId)
{
	NP_PUSH_ASSERT(onlineId != NULL, "CreateIpcUserContext",
		       "service\\np_push_ipc_context.cpp", 108, "onlineId != NULL");
	NP_PUSH_ASSERT(outUserContextId != NULL, "CreateIpcUserContext",
		       "service\\np_push_ipc_context.cpp", 109, "outUserContextId != NULL");

	for (ListNode<IpcUserContext *> *iter = m_userContexts.Begin();
	     iter != m_userContexts.End(); iter = iter->next) {
		if (SceNpCommonPs4_21B2C0B7(&iter->value->GetOnlineId(), onlineId) != 0)
			return SCE_NP_PUSH_ERROR_ALREADY_EXISTS;
	}

	void *rawMem = SceNpCommonPs4_0C922423(sizeof(IpcUserContext), GetAllocator());
	if (rawMem == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	SceUInt32 assignedId = m_nextUserContextId++;
	IpcUserContext *userCtx = new (rawMem) IpcUserContext(assignedId, onlineId);
	if (!m_userContexts.PushBack(userCtx)) {
		userCtx->~IpcUserContext();
		SceNpCommonPs4_6B428829(GetAllocator(), userCtx);
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	}

	*outUserContextId = assignedId;
	return 0;
}

int IpcProcessContext::DeleteIpcUserContext(SceUInt32 userContextId)
{
	for (ListNode<IpcUserContext *> *iter = m_userContexts.Begin();
	     iter != m_userContexts.End(); iter = iter->next) {
		if (iter->value->GetId() == userContextId) {
			IpcUserContext *userCtx = iter->value;
			m_userContexts.Erase(iter);
			userCtx->~IpcUserContext();
			SceNpCommonPs4_6B428829(GetAllocator(), userCtx);
			return 0;
		}
	}
	return SCE_NP_PUSH_ERROR_NOT_FOUND;
}

int IpcProcessContext::DeleteIpcUserContextByOnlineId(const NpPushOnlineId *onlineId)
{
	if (onlineId == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	for (ListNode<IpcUserContext *> *iter = m_userContexts.Begin();
	     iter != m_userContexts.End(); iter = iter->next) {
		if (SceNpCommonPs4_21B2C0B7(&iter->value->GetOnlineId(), onlineId) != 0) {
			IpcUserContext *userCtx = iter->value;
			m_userContexts.Erase(iter);
			userCtx->~IpcUserContext();
			SceNpCommonPs4_6B428829(GetAllocator(), userCtx);
			return 0;
		}
	}
	return SCE_NP_PUSH_ERROR_NOT_FOUND;
}

int IpcProcessContext::AddDataTypeFilter(FilterMatchType matchType, const char *dataType)
{
	if (dataType == NULL || dataType[0] == '\0')
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	for (ListNode<DataTypeFilterEntry> *iter = m_filters.Begin();
	     iter != m_filters.End(); iter = iter->next) {
		if (iter->value.matchType == matchType &&
		    sceClibStrncmp(iter->value.dataType, dataType,
				   sizeof(iter->value.dataType)) == 0) {
			return SCE_NP_PUSH_ERROR_ALREADY_EXISTS;
		}
	}

	DataTypeFilterEntry filter;
	sceClibMemset(&filter, 0, sizeof(filter));
	filter.matchType = matchType;
	sceClibStrncpy(filter.dataType, dataType, sizeof(filter.dataType) - 1);
	if (!m_filters.PushBack(filter))
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	return 0;
}

int IpcProcessContext::RemoveDataTypeFilter(FilterMatchType matchType, const char *dataType)
{
	if (dataType == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	for (ListNode<DataTypeFilterEntry> *iter = m_filters.Begin();
	     iter != m_filters.End(); iter = iter->next) {
		if (iter->value.matchType == matchType &&
		    sceClibStrncmp(iter->value.dataType, dataType,
				   sizeof(iter->value.dataType)) == 0) {
			m_filters.Erase(iter);
			return 0;
		}
	}
	return SCE_NP_PUSH_ERROR_NOT_FOUND;
}

int IpcProcessContext::RemoveDataTypeFilterByName(const char *dataType)
{
	if (dataType == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	for (ListNode<DataTypeFilterEntry> *iter = m_filters.Begin();
	     iter != m_filters.End(); iter = iter->next) {
		if (sceClibStrncmp(iter->value.dataType, dataType,
				   sizeof(iter->value.dataType)) == 0) {
			m_filters.Erase(iter);
			return 0;
		}
	}
	return SCE_NP_PUSH_ERROR_NOT_FOUND;
}

/* 0x813B44EC: ReceiveNotificationDataTypePacket */
int IpcProcessContext::ReceiveNotificationDataTypePacket(const RefPtr<Packet> &packet)
{
	NP_PUSH_ASSERT(!packet.IsNull(), "ReceiveNotificationDataTypePacket",
		       "service\\np_push_ipc_context.cpp", 184, "packet != NULL");

	for (ListNode<DataTypeFilterEntry> *iter = m_filters.Begin();
	     iter != m_filters.End(); iter = iter->next) {
		bool matched = false;
		if (iter->value.matchType == FILTER_MATCH_EXACT)
			matched = packet->MatchDataTypeExact(iter->value.dataType);
		else if (iter->value.matchType == FILTER_MATCH_PREFIX)
			matched = packet->MatchDataTypePrefix(iter->value.dataType);

		if (matched) {
			if (m_dataTypeQueue.Size() >= NP_PUSH_MAX_IPC_QUEUED_PACKETS)
				return SCE_NP_PUSH_ERROR_QUEUE_FULL;
			if (!m_dataTypeQueue.PushBack(packet))
				return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
		}
	}

	if (m_dataTypeQueue.Empty())
		return 0;

	return NotifyDataTypeEvent();
}

/* 0x813B459C: ReceiveUserNotificationPacket */
int IpcProcessContext::ReceiveUserNotificationPacket(const RefPtr<Packet> &packet)
{
	if (packet.IsNull())
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	NpPushOnlineIdData toIdData;
	if (packet->GetToOnlineId(&toIdData) < 0)
		return 0;

	NpPushOnlineId toOnlineId;
	SceNpCommonPs4_D0787194(&toOnlineId, &toIdData);

	for (ListNode<IpcUserContext *> *iter = m_userContexts.Begin();
	     iter != m_userContexts.End(); iter = iter->next) {
		IpcUserContext *userCtx = iter->value;
		if (SceNpCommonPs4_21B2C0B7(&userCtx->GetOnlineId(), &toOnlineId) != 0) {
			int ret = userCtx->EnqueuePacket(packet);
			if (ret < 0) {
				SceNpCommonPs4_1543D7EE(&toOnlineId);
				return ret;
			}
		}
	}
	SceNpCommonPs4_1543D7EE(&toOnlineId);

	if (CountQueuedUserPackets() == 0)
		return 0;

	return NotifyUserEvent();
}

/* 0x813B4880: PeekUserPacket */
int IpcProcessContext::PeekUserPacket(RefPtr<Packet> *outPacket)
{
	if (outPacket == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	outPacket->Reset();
	for (ListNode<IpcUserContext *> *iter = m_userContexts.Begin();
	     iter != m_userContexts.End(); iter = iter->next) {
		if (iter->value->PeekPacket(outPacket) >= 0) {
			NotifyUserEvent();
			return 0;
		}
	}
	return SCE_NP_PUSH_ERROR_NO_DATA;
}

/* 0x813B4958: DequeueUserPacket */
int IpcProcessContext::DequeueUserPacket(RefPtr<Packet> *outPacket)
{
	if (outPacket == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	outPacket->Reset();
	for (ListNode<IpcUserContext *> *iter = m_userContexts.Begin();
	     iter != m_userContexts.End(); iter = iter->next) {
		if (iter->value->DequeuePacket(outPacket) >= 0) {
			if (CountQueuedUserPackets() > 0)
				NotifyUserEvent();
			return 0;
		}
	}
	return SCE_NP_PUSH_ERROR_NO_DATA;
}

/* 0x813B4B70: PeekDataTypePacket */
int IpcProcessContext::PeekDataTypePacket(RefPtr<Packet> *outPacket)
{
	if (outPacket == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	outPacket->Reset();
	if (m_dataTypeQueue.Empty())
		return SCE_NP_PUSH_ERROR_NO_DATA;

	*outPacket = m_dataTypeQueue.Begin()->value;
	NotifyDataTypeEvent();
	return 0;
}

/* 0x813B4BE8: DequeueDataTypePacket */
int IpcProcessContext::DequeueDataTypePacket(RefPtr<Packet> *outPacket)
{
	if (outPacket == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	outPacket->Reset();
	if (m_dataTypeQueue.Empty())
		return SCE_NP_PUSH_ERROR_NO_DATA;

	*outPacket = m_dataTypeQueue.Begin()->value;
	m_dataTypeQueue.Erase(m_dataTypeQueue.Begin());
	if (!m_dataTypeQueue.Empty())
		NotifyDataTypeEvent();
	return 0;
}

SceSize IpcProcessContext::CountQueuedUserPackets(void) const
{
	SceSize total = 0;
	for (ListNode<IpcUserContext *> *iter = m_userContexts.Begin();
	     iter != m_userContexts.End(); iter = iter->next) {
		total += iter->value->GetQueuedCount();
	}
	return total;
}

int IpcProcessContext::NotifyDataTypeEvent(void)
{
	int ret = npIpc2ServerSetEventFlag(m_session, NP_PUSH_IPC_SERVICE_ID,
					   NP_PUSH_IPC_EVF_INDEX,
					   NP_PUSH_IPC_EVENT_DATA_TYPE);
	return (ret >= 0) ? 0 : ret;
}

int IpcProcessContext::NotifyUserEvent(void)
{
	int ret = npIpc2ServerSetEventFlag(m_session, NP_PUSH_IPC_SERVICE_ID,
					   NP_PUSH_IPC_EVF_INDEX,
					   NP_PUSH_IPC_EVENT_USER_NOTIFICATION);
	return (ret >= 0) ? 0 : ret;
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
