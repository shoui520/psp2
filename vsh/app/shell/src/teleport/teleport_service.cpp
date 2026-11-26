/*
 * teleport_service.cpp: SceShellTeleport service entry point, IPMI 0x001A0000
 * dispatcher, and server lifecycle orchestration (0x8139AB6C..0x8139B2BB).
 */
#include "teleport_internal.h"

#include <ipmi/server.h>
#include <ipmi/session.h>
#include <std/string.h>
#include <system/system.h>
#include "../../shell.h"

static const SceUInt8 kTeleportMulticastGroupIpv4[4] = { 239, 255, 255, 250 };

/* 0x8139ABDE */
TeleportService *TeleportService::GetInstance(void)
{
	static TeleportService instance;
	return &instance;
}

/* 0x8139AFA8 / 0x8139B056 */
TeleportService::~TeleportService()
{
	TeleportMulticastServer *multicastServer = TeleportMulticastServer::GetInstance();
	TeleportHttpServer *httpServer = TeleportHttpServer::GetInstance();

	httpServer->Stop();
	multicastServer->Stop();
	httpServer->DestroyThread();
	multicastServer->DestroyThread();
}

/* 0x8139AC42 */
SceInt32 TeleportService::Init(void)
{
	if (paf::system::Model::GetType() != 1)
		return 0;

	TeleportNetMonitor *netMonitor = TeleportNetMonitor::GetInstance();
	netMonitor->Initialize();
	teleportEnsureVideoTempDir();

	TeleportHttpCallbacks httpCallbacks;
	httpCallbacks.onGet = TeleportHttpProtocol::HandleGet;
	httpCallbacks.onPost = TeleportHttpProtocol::HandlePost;
	httpCallbacks.onDelete = TeleportHttpProtocol::HandleDelete;
	httpCallbacks.onOptions = TeleportHttpProtocol::HandleOptions;
	httpCallbacks.onDisconnect = TeleportHttpProtocol::HandleDisconnect;
	httpCallbacks.onIpmiResponse = TeleportHttpProtocol::HandleIpmiResponse;
	httpCallbacks.recvBuffer = new char[TELEPORT_HTTP_BUFFER_SIZE];
	httpCallbacks.recvBufferSize = TELEPORT_HTTP_BUFFER_SIZE;
	httpCallbacks.userData = TeleportHttpProtocol::GetInstance();

	TeleportHttpServer *httpServer = TeleportHttpServer::GetInstance();
	httpServer->Start(httpCallbacks);

	TeleportMulticastCallbacks multicastCallbacks;
	multicastCallbacks.onRecv = TeleportDiscoveryProtocol::HandleSearchPacket;
	multicastCallbacks.onSend = TeleportDiscoveryProtocol::SendSearchReply;
	multicastCallbacks.recvBuffer = new char[TELEPORT_UDP_BUFFER_SIZE];
	multicastCallbacks.recvBufferSize = TELEPORT_UDP_BUFFER_SIZE;
	multicastCallbacks.userData = TeleportDiscoveryProtocol::GetInstance();

	TeleportMulticastServer *multicastServer = TeleportMulticastServer::GetInstance();
	multicastServer->Start(multicastCallbacks);

	if (netMonitor->m_mediaTransferDisabled == 0 && netMonitor->m_netDisconnected == 1) {
		SceUInt8 localIpv4[4] = { 0, 0, 0, 0 };
		char macAddress[18];
		sce_paf_memset(macAddress, 0, sizeof(macAddress));

		if (netMonitor->GetLocalIpv4(localIpv4) >= 0 &&
		    netMonitor->GetMacAddress(macAddress) >= 0) {
			multicastServer->Reconfigure(localIpv4, kTeleportMulticastGroupIpv4,
						     TELEPORT_MULTICAST_PORT);
			httpServer->Reconfigure(localIpv4, 0);
		}
	}
	return 0;
}

/* 0x8139B10A */
SceInt32 TeleportService::HandleIpmiLaunchResponse(const TeleportIpmiLaunchResponse *response,
						   SceInt32 timeoutUs)
{
	if (response != NULL) {
		bool hasNullTerminator = false;
		for (SceSize pos = 0; pos < sizeof(response->extText); ++pos) {
			if (response->extText[pos] == '\0')
				hasNullTerminator = true;
		}
		if (!hasNullTerminator)
			return TELEPORT_ERR_INVALID_PARAM;

		TeleportAppInfoHelper helper;
		paf::string extStr(response->extText);
		if (!extStr.empty() && !helper.ValidateUtf8NoBom(extStr))
			return TELEPORT_ERR_INVALID_PARAM;
		if (teleportHasNoControlChars(extStr) == 0)
			return TELEPORT_ERR_INVALID_PARAM;
	}

	return TeleportHttpServer::GetInstance()->DispatchIpmiResponse(response, timeoutUs);
}

/* 0x8139AB6C */
int shellNoticeServiceInit(void)
{
	return TeleportService::GetInstance()->Init();
}

/* 0x8139ABA0 */
static int teleportHandleIpmiSync(IPMI::Session *session, SceUInt32 methodId,
				  const IPMI::DataInfo *inList, SceUInt32 inCount,
				  IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;
	if (methodId != TELEPORT_IPMI_CMD_LAUNCH_NOTIFY)
		return TELEPORT_IPMI_ERR_UNSUPPORTED;

	const SceInt32 *timeoutPtr = static_cast<const SceInt32 *>(inList[0].pData);
	const TeleportIpmiLaunchResponse *response =
		static_cast<const TeleportIpmiLaunchResponse *>(inList[1].pData);
	SceInt32 result = TeleportService::GetInstance()->HandleIpmiLaunchResponse(
		response, (timeoutPtr != NULL) ? *timeoutPtr : 0);

	return session->respondToSyncMethodRequest(result, outList, outCount);
}

/* 0x8139AB78 */
int shellSvcDisp_1A_Sync(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inList, SceUInt32 inCount,
			 IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	return teleportHandleIpmiSync(session, methodId, inList, inCount, outList, outCount);
}

/* 0x8139AB8C */
int shellSvcDisp_1A_Async(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
			  const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	(void)session;
	(void)reqId;
	(void)methodId;
	(void)inList;
	(void)inCount;
	return TELEPORT_IPMI_ERR_UNSUPPORTED;
}
