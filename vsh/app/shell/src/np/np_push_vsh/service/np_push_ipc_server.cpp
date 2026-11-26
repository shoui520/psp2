/*
 * np_push_ipc_server.cpp: IPMI server-side request handlers for SceNpPush service
 * (methods 0x10000..0x10007: register/unregister/peek/get by OnlineId or DataType).
 * Sony path: vsh/app/common/src/np/np_push_vsh/service/np_push_ipc_server.cpp
 * 3.65-CEX: 0x81402D34..0x814037DF
 */
#include "../np_push_internal.h"

extern int ipmiAsyncNotSupported(void);

#define NP_PUSH_IPMI_UNSUPPORTED			(-2147351103)	/* 0x800205C1 */

#define NP_PUSH_IPC_METHOD_REGISTER_ONLINE_ID		0x10000U
#define NP_PUSH_IPC_METHOD_UNREGISTER_ONLINE_ID		0x10001U
#define NP_PUSH_IPC_METHOD_PEEK_ONLINE_ID		0x10002U
#define NP_PUSH_IPC_METHOD_GET_ONLINE_ID		0x10003U
#define NP_PUSH_IPC_METHOD_REGISTER_DATA_TYPE		0x10004U
#define NP_PUSH_IPC_METHOD_UNREGISTER_DATA_TYPE		0x10005U
#define NP_PUSH_IPC_METHOD_PEEK_DATA_TYPE		0x10006U
#define NP_PUSH_IPC_METHOD_GET_DATA_TYPE		0x10007U

namespace sce {
namespace Np {
namespace Push {

struct GetNotificationRequest {
	SceUInt32 filterFlags;
	SceInt32 extKeyCount;
	SceSize tlvBufferSize;
};

/* 0x81402D80: RegisterNotificationOnlineId */
static int RegisterNotificationOnlineId(IPMI::Session *session, const void *onlineIdData)
{
	Dispatcher *dispatcher = Dispatcher::GetInstance();
	NP_PUSH_ASSERT(dispatcher != NULL, "RegisterNotificationOnlineId",
		       "service\\np_push_ipc_server.cpp", 109, "dispatcher");

	NpPushOnlineId onlineId;
	SceNpCommonPs4_D0787194(&onlineId, onlineIdData);
	int ret = dispatcher->RegisterNotificationOnlineId(session, &onlineId);
	SceNpCommonPs4_1543D7EE(&onlineId);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81402E12: UnregisterNotificationOnlineId */
static int UnregisterNotificationOnlineId(IPMI::Session *session, const void *onlineIdData)
{
	Dispatcher *dispatcher = Dispatcher::GetInstance();
	NP_PUSH_ASSERT(dispatcher != NULL, "UnregisterNotificationOnlineId",
		       "service\\np_push_ipc_server.cpp", 140, "dispatcher");

	NpPushOnlineId onlineId;
	SceNpCommonPs4_D0787194(&onlineId, onlineIdData);
	int ret = dispatcher->UnregisterNotificationOnlineId(session, &onlineId);
	SceNpCommonPs4_1543D7EE(&onlineId);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81402EA4: PeekNotificationOnlineId */
static int PeekNotificationOnlineId(IPMI::Session *session, SceUInt32 *outHasMore,
				    void *outToOnlineId, void *outFromOnlineId,
				    void *outCommId, char *outDataType)
{
	Dispatcher *dispatcher = Dispatcher::GetInstance();
	NP_PUSH_ASSERT(dispatcher != NULL, "PeekNotificationOnlineId",
		       "service\\np_push_ipc_server.cpp", 163, "dispatcher");

	int ret = dispatcher->PeekNotificationOnlineId(session, outHasMore, outToOnlineId,
						       outFromOnlineId, outCommId, outDataType);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81402F14: GetNotificationOnlineId */
static int GetNotificationOnlineId(IPMI::Session *session, SceUInt32 filterFlags,
				   const char *requestedExtKeys, SceInt32 extKeyCount,
				   void *outToOnlineId, void *outFromOnlineId,
				   void *outCommId, char *outDataType,
				   char *outTlvBuffer, SceSize tlvBufferSize,
				   SceSize *outTlvSize)
{
	Dispatcher *dispatcher = Dispatcher::GetInstance();
	NP_PUSH_ASSERT(dispatcher != NULL, "GetNotificationOnlineId",
		       "service\\np_push_ipc_server.cpp", 192, "dispatcher");

	*outTlvSize = 0;

	IpcJsonEntry bodyEntry;
	IpcJsonEntry extEntries[NP_PUSH_IPC_MAX_EXDATA_COUNT];

	int ret = dispatcher->GetNotificationOnlineId(session, filterFlags, requestedExtKeys,
						      extKeyCount, outToOnlineId, outFromOnlineId,
						      outCommId, outDataType, &bodyEntry,
						      extEntries, NP_PUSH_IPC_MAX_EXDATA_COUNT);
	if (ret < 0)
		return ret;

	IpcNotificationPayload payload;
	payload.Init();

	if (!bodyEntry.ownerPacket.IsNull()) {
		ret = payload.SetBody(bodyEntry.jsonValue);
		if (ret < 0)
			return ret;
	}

	for (int extIndex = 0; extIndex < NP_PUSH_IPC_MAX_EXDATA_COUNT; ++extIndex) {
		if (extEntries[extIndex].ownerPacket.IsNull() || extKeyCount <= 0)
			continue;

		const char *entryKey = extEntries[extIndex].keyName;
		if (entryKey == NULL)
			continue;

		for (SceInt32 keyIndex = 0; keyIndex < extKeyCount; ++keyIndex) {
			const char *candidate =
				requestedExtKeys + keyIndex * NP_PUSH_IPC_EXDATA_KEY_STRIDE;
			if (sceClibStrncmp(candidate, entryKey,
					   NP_PUSH_IPC_EXDATA_KEY_STRIDE) == 0) {
				ret = payload.AddExData(entryKey, extEntries[extIndex].jsonValue);
				if (ret < 0)
					return ret;
				break;
			}
		}
	}

	ret = payload.Build(outTlvBuffer, tlvBufferSize, outTlvSize);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81403184: RegisterNotificationDataType */
static int RegisterNotificationDataType(IPMI::Session *session, const char *dataTypeName,
					SceInt32 matchType)
{
	Dispatcher *dispatcher = Dispatcher::GetInstance();
	NP_PUSH_ASSERT(dispatcher != NULL, "RegisterNotificationDataType",
		       "service\\np_push_ipc_server.cpp", 259, "dispatcher");

	DataType dataType;
	int ret = dataType.Set(dataTypeName);
	if (ret < 0)
		return ret;

	ret = dispatcher->RegisterNotificationDataType(session, &dataType, matchType);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8140323A: UnregisterNotificationDataType */
static int UnregisterNotificationDataType(IPMI::Session *session, const char *dataTypeName)
{
	Dispatcher *dispatcher = Dispatcher::GetInstance();
	NP_PUSH_ASSERT(dispatcher != NULL, "UnregisterNotificationDataType",
		       "service\\np_push_ipc_server.cpp", 284, "dispatcher");

	DataType dataType;
	int ret = dataType.Set(dataTypeName);
	if (ret < 0)
		return ret;

	ret = dispatcher->UnregisterNotificationDataType(session, &dataType);
	return (ret >= 0) ? 0 : ret;
}

/* 0x814032EA: PeekNotificationDataType */
static int PeekNotificationDataType(IPMI::Session *session, SceUInt32 *outHasMore,
				    void *outToOnlineId, void *outFromOnlineId,
				    void *outCommId, char *outDataType)
{
	Dispatcher *dispatcher = Dispatcher::GetInstance();
	NP_PUSH_ASSERT(dispatcher != NULL, "PeekNotificationDataType",
		       "service\\np_push_ipc_server.cpp", 314, "dispatcher");

	int ret = dispatcher->PeekNotificationDataType(session, outHasMore, outToOnlineId,
						       outFromOnlineId, outCommId, outDataType);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8140335C: GetNotificationDataType */
static int GetNotificationDataType(IPMI::Session *session, SceUInt32 filterFlags,
				   const char *requestedExtKeys, SceInt32 extKeyCount,
				   void *outToOnlineId, void *outFromOnlineId,
				   void *outCommId, char *outDataType,
				   char *outTlvBuffer, SceSize tlvBufferSize,
				   SceSize *outTlvSize)
{
	Dispatcher *dispatcher = Dispatcher::GetInstance();
	NP_PUSH_ASSERT(dispatcher != NULL, "GetNotificationDataType",
		       "service\\np_push_ipc_server.cpp", 343, "dispatcher");

	*outTlvSize = 0;

	IpcJsonEntry bodyEntry;
	IpcJsonEntry extEntries[NP_PUSH_IPC_MAX_EXDATA_COUNT];

	int ret = dispatcher->GetNotificationDataType(session, filterFlags, outToOnlineId,
						      outFromOnlineId, outCommId, outDataType,
						      &bodyEntry, extEntries,
						      NP_PUSH_IPC_MAX_EXDATA_COUNT);
	if (ret < 0)
		return ret;

	IpcNotificationPayload payload;
	payload.Init();

	if (!bodyEntry.ownerPacket.IsNull()) {
		ret = payload.SetBody(bodyEntry.jsonValue);
		if (ret < 0)
			return ret;
	}

	for (int extIndex = 0; extIndex < NP_PUSH_IPC_MAX_EXDATA_COUNT; ++extIndex) {
		if (extEntries[extIndex].ownerPacket.IsNull() || extKeyCount <= 0)
			continue;

		const char *entryKey = extEntries[extIndex].keyName;
		if (entryKey == NULL)
			continue;

		for (SceInt32 keyIndex = 0; keyIndex < extKeyCount; ++keyIndex) {
			const char *candidate =
				requestedExtKeys + keyIndex * NP_PUSH_IPC_EXDATA_KEY_STRIDE;
			if (sceClibStrncmp(candidate, entryKey,
					   NP_PUSH_IPC_EXDATA_KEY_STRIDE) == 0) {
				ret = payload.AddExData(entryKey, extEntries[extIndex].jsonValue);
				if (ret < 0)
					return ret;
				break;
			}
		}
	}

	ret = payload.Build(outTlvBuffer, tlvBufferSize, outTlvSize);
	return (ret >= 0) ? 0 : ret;
}

/* 0x814035BC: Handle sync method 0x10003 */
static int DispatchGetNotificationOnlineId(IPMI::Session *session, const IPMI::DataInfo *inList,
					   SceUInt32 inCount, IPMI::BufferInfo *outList,
					   SceUInt32 outCount)
{
	(void)inCount;
	const GetNotificationRequest *req =
		static_cast<const GetNotificationRequest *>(inList[0].pData);
	const char *extKeys = static_cast<const char *>(inList[1].pData);
	SceSize *tlvWritten = static_cast<SceSize *>(outList[5].pBuffer);

	int ret = GetNotificationOnlineId(session, req->filterFlags, extKeys, req->extKeyCount,
					  outList[0].pBuffer, outList[1].pBuffer,
					  outList[2].pBuffer,
					  static_cast<char *>(outList[3].pBuffer),
					  static_cast<char *>(outList[4].pBuffer),
					  req->tlvBufferSize, tlvWritten);

	outList[0].outSize = NP_PUSH_IPC_ONLINE_ID_WIRE_SIZE;
	outList[1].outSize = NP_PUSH_IPC_ONLINE_ID_WIRE_SIZE;
	outList[2].outSize = NP_PUSH_IPC_COMM_ID_WIRE_SIZE;
	outList[3].outSize = NP_PUSH_IPC_DATA_TYPE_WIRE_SIZE;
	outList[4].outSize = *tlvWritten;
	outList[5].outSize = sizeof(SceSize);
	return session->respondToSyncMethodRequest(ret, outList, outCount);
}

/* 0x8140363C: Handle sync method 0x10007 */
static int DispatchGetNotificationDataType(IPMI::Session *session, const IPMI::DataInfo *inList,
					   SceUInt32 inCount, IPMI::BufferInfo *outList,
					   SceUInt32 outCount)
{
	(void)inCount;
	const GetNotificationRequest *req =
		static_cast<const GetNotificationRequest *>(inList[0].pData);
	const char *extKeys = static_cast<const char *>(inList[1].pData);
	SceSize *tlvWritten = static_cast<SceSize *>(outList[5].pBuffer);

	int ret = GetNotificationDataType(session, req->filterFlags, extKeys, req->extKeyCount,
					  outList[0].pBuffer, outList[1].pBuffer,
					  outList[2].pBuffer,
					  static_cast<char *>(outList[3].pBuffer),
					  static_cast<char *>(outList[4].pBuffer),
					  req->tlvBufferSize, tlvWritten);

	outList[0].outSize = NP_PUSH_IPC_ONLINE_ID_WIRE_SIZE;
	outList[1].outSize = NP_PUSH_IPC_ONLINE_ID_WIRE_SIZE;
	outList[2].outSize = NP_PUSH_IPC_COMM_ID_WIRE_SIZE;
	outList[3].outSize = NP_PUSH_IPC_DATA_TYPE_WIRE_SIZE;
	outList[4].outSize = *tlvWritten;
	outList[5].outSize = sizeof(SceSize);
	return session->respondToSyncMethodRequest(ret, outList, outCount);
}

/* 0x814036BA: Dispatch all SceNpPush synchronous IPMI methods */
static int DispatchPushSyncMethod(IPMI::Session *session, SceUInt32 methodId,
				  const IPMI::DataInfo *inList, SceUInt32 inCount,
				  IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	switch (methodId) {
	case NP_PUSH_IPC_METHOD_REGISTER_ONLINE_ID: {
		int ret = RegisterNotificationOnlineId(session, inList[0].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case NP_PUSH_IPC_METHOD_UNREGISTER_ONLINE_ID: {
		int ret = UnregisterNotificationOnlineId(session, inList[0].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case NP_PUSH_IPC_METHOD_PEEK_ONLINE_ID: {
		int ret = PeekNotificationOnlineId(session,
						   static_cast<SceUInt32 *>(outList[0].pBuffer),
						   outList[1].pBuffer, outList[2].pBuffer,
						   outList[3].pBuffer,
						   static_cast<char *>(outList[4].pBuffer));
		outList[0].outSize = sizeof(SceUInt32);
		outList[1].outSize = NP_PUSH_IPC_ONLINE_ID_WIRE_SIZE;
		outList[2].outSize = NP_PUSH_IPC_ONLINE_ID_WIRE_SIZE;
		outList[3].outSize = NP_PUSH_IPC_COMM_ID_WIRE_SIZE;
		outList[4].outSize = NP_PUSH_IPC_DATA_TYPE_WIRE_SIZE;
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case NP_PUSH_IPC_METHOD_GET_ONLINE_ID:
		return DispatchGetNotificationOnlineId(session, inList, inCount, outList,
						       outCount);
	case NP_PUSH_IPC_METHOD_REGISTER_DATA_TYPE: {
		SceInt32 matchType = *static_cast<const SceInt32 *>(inList[0].pData);
		const char *dataTypeName = static_cast<const char *>(inList[1].pData);
		int ret = RegisterNotificationDataType(session, dataTypeName, matchType);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case NP_PUSH_IPC_METHOD_UNREGISTER_DATA_TYPE: {
		const char *dataTypeName = static_cast<const char *>(inList[0].pData);
		int ret = UnregisterNotificationDataType(session, dataTypeName);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case NP_PUSH_IPC_METHOD_PEEK_DATA_TYPE: {
		int ret = PeekNotificationDataType(session,
						   static_cast<SceUInt32 *>(outList[0].pBuffer),
						   outList[1].pBuffer, outList[2].pBuffer,
						   outList[3].pBuffer,
						   static_cast<char *>(outList[4].pBuffer));
		outList[0].outSize = sizeof(SceUInt32);
		outList[1].outSize = NP_PUSH_IPC_ONLINE_ID_WIRE_SIZE;
		outList[2].outSize = NP_PUSH_IPC_ONLINE_ID_WIRE_SIZE;
		outList[3].outSize = NP_PUSH_IPC_COMM_ID_WIRE_SIZE;
		outList[4].outSize = NP_PUSH_IPC_DATA_TYPE_WIRE_SIZE;
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case NP_PUSH_IPC_METHOD_GET_DATA_TYPE:
		return DispatchGetNotificationDataType(session, inList, inCount, outList,
						       outCount);
	default:
		return NP_PUSH_IPMI_UNSUPPORTED;
	}
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */

/* 0x81402D34: Client connected to NpIpc2 SceNpPush service */
int npPushIpcOnClientOpen(IPMI::Session *session)
{
	sce::Np::Push::Dispatcher *dispatcher = sce::Np::Push::Dispatcher::GetInstance();
	if (dispatcher == NULL)
		return 0;
	return dispatcher->NewIpcProcessContext(session, NULL);
}

/* 0x81402D48: Client disconnected from NpIpc2 SceNpPush service */
int npPushIpcOnClientClose(IPMI::Session *session)
{
	sce::Np::Push::Dispatcher *dispatcher = sce::Np::Push::Dispatcher::GetInstance();
	if (dispatcher == NULL)
		return 0;
	return dispatcher->DeleteIpcProcessContext(session);
}

/* 0x81402D5C: Synchronous method callback registered with NpIpc2Server */
int npPushIpcOnSyncMethod(IPMI::Session *session, SceUInt32 methodId,
			  const IPMI::DataInfo *inList, SceUInt32 inCount,
			  IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	return sce::Np::Push::DispatchPushSyncMethod(session, methodId, inList, inCount, outList,
						     outCount);
}

/* 0x81402D70: Asynchronous method callback (unsupported) */
int npPushIpcOnAsyncMethod(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
			   const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	(void)session;
	(void)requestId;
	(void)methodId;
	(void)inList;
	(void)inCount;
	return ipmiAsyncNotSupported();
}
