/*
 * mtp_service_session_mgr.cpp: MtpServiceSessionMgr ("SESMGR", 0x812B2728 - 0x812B48DF)
 * Tracks connected IPMI client sessions, active ticket->context mappings, host discovery,
 * network impose references, and status/register/bgcopy event broadcasts.
 */
#include <appmgr/appmgr.h>
#include "mtp_server_internal.h"

static bool isStatusConnectedState(int state)
{
	return state >= 2 && state <= 6;
}

static bool isStatusActiveTransferState(int state)
{
	return state > 2 && state <= 4;
}

/* 0x812B2728 */
MtpServiceSessionMgr::MtpServiceSessionMgr()
{
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		m_slots[i].session = NULL;
		m_slots[i].hostReset = 0;
		m_slots[i].lastHostState = -1;
		m_slots[i].networkImposeToken = 0;
		m_slots[i].flags = 0;
	}
}

/* 0x812B27B4 */
MtpServiceSessionMgr::~MtpServiceSessionMgr()
{
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		for (paf::list<MtpContextMapping *>::iterator it = m_slots[i].contexts.begin();
		     it != m_slots[i].contexts.end(); ++it) {
			sce_paf_free(*it);
		}
		m_slots[i].contexts.clear();
	}
}

void MtpServiceSessionMgr::lock()
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): LockLwMutex failed: 0x%x\n",
			      "SESMGR", "lock", 934, ret);
}

void MtpServiceSessionMgr::unlock()
{
	int ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): UnlockLwMutex failed: 0x%x\n",
			      "SESMGR", "unlock", 943, ret);
}

/* 0x812B2882 */
int MtpServiceSessionMgr::initialize()
{
	int ret = sceKernelCreateLwMutex(&m_mutex, "SceMtpServiceSessionMgr", 0x2000u, 0, NULL);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): CreateLwMutex failed: 0x%x\n",
			      "SESMGR", "Initialize", 47, ret);
	return ret;
}

/* 0x812B28C8 */
int MtpServiceSessionMgr::finalize()
{
	int ret = sceKernelDeleteLwMutex(&m_mutex);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): DeleteLwMutex failed: 0x%x\n",
			      "SESMGR", "Finalize", 56, ret);
	return ret;
}

/* 0x812B28FE */
int MtpServiceSessionMgr::prepare(int *outSlotIndex)
{
	lock();
	*outSlotIndex = 0;
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		if (m_slots[i].session == NULL) {
			m_slots[i].session = MTP_SESSION_PREPARING;
			m_slots[i].contexts.clear();
			m_slots[i].lastHostState = -1;
			m_slots[i].hostReset = 0;
			m_slots[i].flags = 0;
			m_slots[i].networkImposeToken = 0;
			m_slots[i].deferredRegisterEvents.clear();
			*outSlotIndex = (int)i;
			unlock();
			return 0;
		}
		*outSlotIndex = (int)(i + 1);
	}

	unlock();
	sceClibPrintf("[MTP %s] Error %s(%u): max client\n", "SESMGR", "Prepare", 81);
	return MTP_SVC_ERROR_MAX_CLIENTS;
}

/* 0x812B2A76 */
int MtpServiceSessionMgr::set(SceUInt32 slotIndex, IPMI::Session *session)
{
	if (slotIndex >= MTP_MAX_CLIENTS) {
		sceClibPrintf("[MTP %s] Error %s(%u): Invalid session: 0x%x\n",
			      "SESMGR", "Set", 88, session);
		return MTP_SVC_ERROR_INVALID_ARGUMENT;
	}

	m_slots[slotIndex].session = session;
	MtpStatus hostStatus;
	sce_paf_memset(&hostStatus, 0, sizeof(hostStatus));
	if (sceMtpGetHostInfo(&hostStatus) == 0)
		m_slots[slotIndex].lastHostState = hostStatus.state;
	return 0;
}

/* 0x812B2AEC */
int MtpServiceSessionMgr::clear(IPMI::Session *session)
{
	lock();
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		if (m_slots[i].session == session) {
			if (m_slots[i].networkImposeToken > 0) {
				int ret = SceShell_794BF5BE(
					(unsigned int)m_slots[i].networkImposeToken);
				if (ret < 0)
					sceClibPrintf(
						"[MTP %s] Error %s(%u): failed sceNetCtlDisableNetworkImposeForShellSocketVsh() 0x%x\n",
						"SESMGR", "Clear", 117, ret);
			}
			if ((m_slots[i].flags & MTP_SESSION_FLAG_HOST_DISCOVERY) != 0) {
				int discoveryState = 0;
				int ret = SceLibMtp_06123E14(&discoveryState);
				if (ret != 0) {
					sceClibPrintf(
						"[MTP %s] Error %s(%u): failed to get host discovery state 0x%x\n",
						"SESMGR", "Clear", 136, ret);
				} else if (discoveryState != 0) {
					int stopRet = SceLibMtp_71197C62();
					if (stopRet != 0)
						sceClibPrintf(
							"[MTP %s] Error %s(%u): failed to stop host discovery 0x%x\n",
							"SESMGR", "Clear", 130, stopRet);
				}
			}

			for (paf::list<MtpContextMapping *>::iterator it =
				     m_slots[i].contexts.begin();
			     it != m_slots[i].contexts.end(); ++it) {
				SceLibMtp_1021F75F((*it)->contextId);
				sce_paf_free(*it);
			}
			m_slots[i].contexts.clear();
			m_slots[i].session = NULL;
			unlock();
			return 0;
		}
	}

	sceClibPrintf("[MTP %s] Error %s(%u): Invalid session: 0x%x\n",
		      "SESMGR", "Clear", 109, session);
	unlock();
	return MTP_SVC_ERROR_INVALID_ARGUMENT;
}

/* 0x812B2CCC */
int MtpServiceSessionMgr::findSlotByProcessId(SceUID pid, SceUInt32 *outIndex)
{
	*outIndex = 0;
	for (SceUInt32 i = 0; i < MTP_MAX_CLIENTS; i++) {
		IPMI::Session *session = m_slots[i].session;
		if (session != NULL && session != MTP_SESSION_PREPARING) {
			if (session->getProcessId() == pid) {
				*outIndex = i;
				return 0;
			}
		}
		*outIndex = i + 1;
	}
	return MTP_SVC_ERROR_NOT_FOUND;
}

/* 0x812B2D10 */
int MtpServiceSessionMgr::cancelByProcessId(SceUID pid)
{
	lock();
	SceUInt32 slotIndex = 0;
	int ret = findSlotByProcessId(pid, &slotIndex);
	if (ret < 0) {
		unlock();
		return ret;
	}

	for (paf::list<MtpContextMapping *>::iterator it = m_slots[slotIndex].contexts.begin();
	     it != m_slots[slotIndex].contexts.end(); ++it) {
		SceLibMtp_1021F75F((*it)->contextId);
	}
	m_slots[slotIndex].hostReset = 0;
	unlock();
	return 0;
}

/* 0x812B2E14 */
int MtpServiceSessionMgr::terminateByProcessId(SceUID pid)
{
	lock();
	SceUInt32 slotIndex = 0;
	int ret = findSlotByProcessId(pid, &slotIndex);
	if (ret < 0) {
		unlock();
		return ret;
	}

	sceMtpTerminateExecution();
	unlock();
	return 0;
}

/* 0x812B2EDE */
int MtpServiceSessionMgr::disableNotify(IPMI::Session *session)
{
	lock();
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		if (m_slots[i].session == session) {
			m_slots[i].flags |= MTP_SESSION_FLAG_DISABLE_NOTIFY;
			unlock();
			return 0;
		}
	}
	unlock();
	return 0;
}

/* 0x812B2FAC */
int MtpServiceSessionMgr::lockSession(IPMI::Session *session)
{
	lock();
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		if (m_slots[i].session == session)
			return 0;
	}
	unlock();
	return MTP_SVC_ERROR_NOT_FOUND;
}

/* 0x812B3032 */
int MtpServiceSessionMgr::unlockSession()
{
	unlock();
	return 0;
}

/* 0x812B3068 */
int MtpServiceSessionMgr::addContext(IPMI::Session *session, SceUInt32 ticket,
				     SceUInt32 contextId)
{
	lock();
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		if (m_slots[i].session == session) {
			MtpContextMapping *mapping =
				(MtpContextMapping *)sce_paf_malloc(sizeof(MtpContextMapping));
			if (mapping == NULL) {
				sceClibPrintf("[MTP %s] Error %s(%u): no memory\n",
					      "SESMGR", "Add", 220);
				unlock();
				return MTP_SVC_ERROR_NO_MEMORY;
			}
			sce_paf_memset(mapping, 0, sizeof(MtpContextMapping));
			mapping->ticket = ticket;
			mapping->contextId = contextId;
			m_slots[i].contexts.push_back(mapping);
			unlock();
			return 0;
		}
	}

	sceClibPrintf("[MTP %s] Error %s(%u): Invalid sessoin: 0x%x\n",
		      "SESMGR", "Add", 213, session);
	unlock();
	return MTP_SVC_ERROR_INVALID_ARGUMENT;
}

/* 0x812B31EC */
int MtpServiceSessionMgr::deleteContext(IPMI::Session *session, SceUInt32 ticket)
{
	lock();
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		if (m_slots[i].session == session) {
			for (paf::list<MtpContextMapping *>::iterator it =
				     m_slots[i].contexts.begin();
			     it != m_slots[i].contexts.end(); ++it) {
				if ((*it)->ticket == ticket) {
					sce_paf_free(*it);
					m_slots[i].contexts.erase(it);
					unlock();
					return 0;
				}
			}
			sceClibPrintf(
				"[MTP %s] Error %s(%u): session 0x%x ticket %u is not found\n",
				"SESMGR", "Delete", 279, session, ticket);
			unlock();
			return MTP_SVC_ERROR_NOT_FOUND;
		}
	}

	sceClibPrintf("[MTP %s] Error %s(%u): Invalid session: 0x%x\n",
		      "SESMGR", "Delete", 265, session);
	unlock();
	return MTP_SVC_ERROR_INVALID_ARGUMENT;
}

/* 0x812B3380 */
int MtpServiceSessionMgr::getContext(IPMI::Session *session, SceUInt32 ticket,
				     SceUInt32 *outContextId)
{
	lock();
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		if (m_slots[i].session == session) {
			for (paf::list<MtpContextMapping *>::iterator it =
				     m_slots[i].contexts.begin();
			     it != m_slots[i].contexts.end(); ++it) {
				if ((*it)->ticket == ticket) {
					*outContextId = (*it)->contextId;
					unlock();
					return 0;
				}
			}
			unlock();
			return MTP_SVC_ERROR_NOT_FOUND;
		}
	}

	sceClibPrintf("[MTP %s] Error %s(%u): Invalid session: 0x%x\n",
		      "SESMGR", "Get", 241, session);
	unlock();
	return MTP_SVC_ERROR_INVALID_ARGUMENT;
}

/* 0x812B34DC */
int MtpServiceSessionMgr::getSessionIndex(IPMI::Session *session, SceUInt32 *outIndex)
{
	lock();
	if (outIndex == NULL)
		return MTP_SVC_ERROR_INVALID_ARGUMENT;

	*outIndex = 0;
	for (SceUInt32 i = 0; i < MTP_MAX_CLIENTS; i++) {
		if (m_slots[i].session == session) {
			*outIndex = i;
			unlock();
			return 0;
		}
		*outIndex = i + 1;
	}
	unlock();
	return MTP_SVC_ERROR_INVALID_ARGUMENT;
}

/* 0x812B3584 */
int MtpServiceSessionMgr::getSessionByIndex(SceUInt32 index, IPMI::Session **outSession)
{
	if (outSession == NULL || index >= MTP_MAX_CLIENTS)
		return MTP_SVC_ERROR_INVALID_ARGUMENT;

	IPMI::Session *session = m_slots[index].session;
	if (session == NULL || session == MTP_SESSION_PREPARING)
		return MTP_SVC_ERROR_NOT_FOUND;

	*outSession = session;
	return 0;
}

/* 0x812B35C6 */
int MtpServiceSessionMgr::markHostResetAll()
{
	lock();
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		IPMI::Session *session = m_slots[i].session;
		if (session != NULL && session != MTP_SESSION_PREPARING)
			m_slots[i].hostReset = 1;
	}
	unlock();
	return 0;
}

/* 0x812B3660 */
int MtpServiceSessionMgr::checkHostReset(IPMI::Session *session, SceUInt8 *outReset)
{
	lock();
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		if (m_slots[i].session == session) {
			*outReset = m_slots[i].hostReset;
			m_slots[i].hostReset = 0;
			unlock();
			return 0;
		}
	}

	sceClibPrintf("[MTP %s] Error %s(%u): Invalid session: 0x%x\n",
		      "SESMGR", "CheckHostReset", 302, session);
	unlock();
	return MTP_SVC_ERROR_INVALID_ARGUMENT;
}

/* 0x812B3754 */
int MtpServiceSessionMgr::onStartHostDiscovery(IPMI::Session *session)
{
	lock();
	bool found = false;
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		if (m_slots[i].session == session) {
			m_slots[i].flags |= MTP_SESSION_FLAG_HOST_DISCOVERY;
			found = true;
			break;
		}
	}
	if (!found) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): session with client 0x%x is not established\n",
			"SESMGR", "OnStartHostDiscovery", 321, session->getProcessId());
	}
	unlock();
	return 0;
}

/* 0x812B381C */
int MtpServiceSessionMgr::onStopHostDiscovery(IPMI::Session *session)
{
	lock();
	bool found = false;
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		if (m_slots[i].session == session) {
			m_slots[i].flags &= ~MTP_SESSION_FLAG_HOST_DISCOVERY;
			found = true;
			break;
		}
	}
	if (!found) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): session with client 0x%x is not established\n",
			"SESMGR", "OnStopHostDiscovery", 346, session->getProcessId());
	}
	unlock();
	return 0;
}

/* 0x812B38EC */
int MtpServiceSessionMgr::onStartRegister(IPMI::Session *session)
{
	lock();
	bool found = false;
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		if (m_slots[i].session == session) {
			m_slots[i].flags |= MTP_SESSION_FLAG_NET_REGISTER;
			found = true;
			break;
		}
	}
	if (!found) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): session with client 0x%x is not established\n",
			"SESMGR", "OnStartRegister", 371, session->getProcessId());
	}
	unlock();
	return 0;
}

/* 0x812B39B4 */
int MtpServiceSessionMgr::onStopRegister(IPMI::Session *session)
{
	lock();
	bool found = false;
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		if (m_slots[i].session == session) {
			m_slots[i].flags &= ~MTP_SESSION_FLAG_NET_REGISTER;
			found = true;
			break;
		}
	}
	if (!found) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): session with client 0x%x is not established\n",
			"SESMGR", "OnStopRegister", 396, session->getProcessId());
	}
	unlock();
	return 0;
}

/* 0x812B3A84 */
int MtpServiceSessionMgr::enableNetworkImpose(IPMI::Session *session)
{
	lock();
	int slotIndex = -1;
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		if (m_slots[i].session == session) {
			slotIndex = (int)i;
			break;
		}
	}

	if (slotIndex < 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): session with client 0x%x is not established\n",
			"SESMGR", "EnableNetworkImpose", 431, session->getProcessId());
	} else if ((m_slots[slotIndex].flags & MTP_SESSION_FLAG_NETWORK_IMPOSE) != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): network impose for client 0x%x is already enabled\n",
			"SESMGR", "EnableNetworkImpose", 436, session->getProcessId());
	} else {
		SceUID pid = session->getProcessId();
		int token = SceShell_6B8772A2(pid);
		if (token < 0) {
			sceClibPrintf(
				"[MTP %s] Error %s(%u): failed sceNetCtlEnableNetworkImposeForShellSocketVsh() 0x%x\n",
				"SESMGR", "EnableNetworkImpose", 442, token);
		} else if (token == 0) {
			sceClibPrintf(
				"[MTP %s] Error %s(%u): process 0x%x is not foreground\n",
				"SESMGR", "EnableNetworkImpose", 445, session->getProcessId());
		} else {
			m_slots[slotIndex].networkImposeToken = token;
			m_slots[slotIndex].flags |= MTP_SESSION_FLAG_NETWORK_IMPOSE;
		}
	}

	unlock();
	return 0;
}

/* 0x812B3BE8 */
int MtpServiceSessionMgr::disableNetworkImpose(IPMI::Session *session)
{
	lock();
	int slotIndex = -1;
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		if (m_slots[i].session == session) {
			slotIndex = (int)i;
			break;
		}
	}

	if (slotIndex < 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): session with client 0x%x is not established\n",
			"SESMGR", "DisableNetworkImpose", 468, session->getProcessId());
	} else if ((m_slots[slotIndex].flags & MTP_SESSION_FLAG_NETWORK_IMPOSE) != 0) {
		int ret = SceShell_794BF5BE((unsigned int)m_slots[slotIndex].networkImposeToken);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): failed sceNetCtlDisableNetworkImposeForShellSocketVsh() 0x%x\n",
				"SESMGR", "DisableNetworkImpose", 479, ret);
		m_slots[slotIndex].networkImposeToken = 0;
		m_slots[slotIndex].flags &= ~MTP_SESSION_FLAG_NETWORK_IMPOSE;
	}

	unlock();
	return 0;
}

/* 0x812B3CF4 */
void MtpServiceSessionMgr::restartHostDiscoveryOnProcessActivate(SceUID pid)
{
	lock();
	SceUInt32 slotIndex = 0;
	if (findSlotByProcessId(pid, &slotIndex) >= 0 &&
	    (m_slots[slotIndex].flags & MTP_SESSION_FLAG_HOST_DISCOVERY) != 0) {
		int ret = SceLibMtp_79F3EE1B();
		if (ret != 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): failed to start host discovery 0x%x\n",
				"SESMGR", "restartHostDiscoveryOnProcessActivate", 600, ret);
	}
	unlock();
}

/* 0x812B3DC0 */
void MtpServiceSessionMgr::enableNetworkImposeOnProcessActivate(SceUID pid)
{
	lock();
	SceUInt32 slotIndex = 0;
	if (findSlotByProcessId(pid, &slotIndex) >= 0 &&
	    (m_slots[slotIndex].flags & MTP_SESSION_FLAG_NETWORK_IMPOSE) != 0) {
		int oldToken = m_slots[slotIndex].networkImposeToken;
		if (oldToken > 0) {
			sceClibPrintf(
				"[MTP %s] Error %s(%u): old stat of network impose for shell socket is not valid %d\n",
				"SESMGR", "enableNetworkImposeOnProcessActivate", 523, oldToken);
		} else {
			int token = SceShell_6B8772A2(pid);
			if (token < 0) {
				sceClibPrintf(
					"[MTP %s] Error %s(%u): failed sceNetCtlEnableNetworkImposeForShellSocketVsh() 0x%x\n",
					"SESMGR", "enableNetworkImposeOnProcessActivate", 529, token);
			} else if (token == 0) {
				sceClibPrintf(
					"[MTP %s] Error %s(%u): process 0x%x is not foreground\n",
					"SESMGR", "enableNetworkImposeOnProcessActivate", 532, pid);
			} else {
				m_slots[slotIndex].networkImposeToken = token;
			}
		}
	}
	unlock();
}

/* 0x812B3EEE */
void MtpServiceSessionMgr::onProcessActivate(SceUID pid)
{
	enableNetworkImposeOnProcessActivate(pid);
	restartHostDiscoveryOnProcessActivate(pid);
}

/* 0x812B3F02 */
void MtpServiceSessionMgr::disableNetworkImposeOnProcessDeactivate(SceUID pid)
{
	lock();
	SceUInt32 slotIndex = 0;
	if (findSlotByProcessId(pid, &slotIndex) >= 0 &&
	    (m_slots[slotIndex].flags & MTP_SESSION_FLAG_NETWORK_IMPOSE) != 0) {
		int oldToken = m_slots[slotIndex].networkImposeToken;
		if (oldToken > 0) {
			int ret = SceShell_794BF5BE((unsigned int)oldToken);
			if (ret < 0)
				sceClibPrintf(
					"[MTP %s] Error %s(%u): failed sceNetCtlDisableNetworkImposeForShellSocketVsh() 0x%x\n",
					"SESMGR", "disableNetworkImposeOnProcessDeactivate", 568, ret);
			m_slots[slotIndex].networkImposeToken = 0;
		} else {
			sceClibPrintf(
				"[MTP %s] Error %s(%u): old stat of network impose for shell socket is not valid %d\n",
				"SESMGR", "disableNetworkImposeOnProcessDeactivate", 562, oldToken);
		}
	}
	unlock();
}

/* 0x812B4008 */
void MtpServiceSessionMgr::stopHostDiscoveryOnProcessDeactivate(SceUID pid)
{
	lock();
	SceUInt32 slotIndex = 0;
	if (findSlotByProcessId(pid, &slotIndex) >= 0 &&
	    (m_slots[slotIndex].flags & MTP_SESSION_FLAG_HOST_DISCOVERY) != 0) {
		int ret = SceLibMtp_71197C62();
		if (ret != 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): failed to stop host discovery 0x%x\n",
				"SESMGR", "stopHostDiscoveryOnProcessDeactivate", 629, ret);
	}
	unlock();
}

/* 0x812B40D4 */
void MtpServiceSessionMgr::onProcessDeactivate(SceUID pid)
{
	stopHostDiscoveryOnProcessDeactivate(pid);
	disableNetworkImposeOnProcessDeactivate(pid);
}

/* 0x812B40EA */
int MtpServiceSessionMgr::broadcastMtpStatusEvent(MtpStatusEventMsg *eventMsg)
{
	lock();
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		IPMI::Session *session = m_slots[i].session;
		SceUInt32 timeout = MTP_MSG_SEND_TIMEOUT_US;
		if (session == NULL || session == MTP_SESSION_PREPARING ||
		    (m_slots[i].flags & MTP_SESSION_FLAG_DISABLE_NOTIFY) != 0)
			continue;

		SceAppMgrAppStatus appStatus;
		sceAppMgrGetStatusByAppId(session->getAppId(), &appStatus);
		if (appStatus.activated == 0 &&
		    sce_paf_strncmp(appStatus.appName, "NPXS10100", 0x20u) != 0) {
			if (eventMsg->eventCode == MTP_HOST_EVENT_DISCONNECTED &&
			    isStatusConnectedState(m_slots[i].lastHostState)) {
				m_slots[i].flags |= MTP_SESSION_FLAG_PENDING_DISCONNECT;
				sce_paf_memcpy(&m_slots[i].deferredStatus,
					       &eventMsg->hostStatus, sizeof(MtpStatus));
			} else if (eventMsg->eventCode == MTP_HOST_EVENT_HOST_RESET &&
				   isStatusActiveTransferState(m_slots[i].lastHostState)) {
				m_slots[i].flags |= MTP_SESSION_FLAG_PENDING_RESET;
			}
		} else {
			if (eventMsg->eventCode == MTP_HOST_EVENT_HOST_RESET)
				eventMsg->hostStatus.state = 2;

			MtpBroadcastMessage msg;
			msg.msgType = MTP_MSG_EVENT_STATUS;
			sce_paf_memcpy(&msg.body.statusEvent, eventMsg, sizeof(MtpStatusEventMsg));
			int ret = session->sendMsg(0, &msg, sizeof(msg), &timeout);
			if (ret < 0)
				sceClibPrintf(
					"[MTP %s] Error %s(%u): Failed to send message 0x%x\n",
					"SESMGR", "BroadcastMtpStatusEvent", 720, ret);
			m_slots[i].lastHostState = eventMsg->hostStatus.state;
		}
	}
	unlock();
	return 0;
}

/* 0x812B4290 */
int MtpServiceSessionMgr::broadcastMtpStatusEventDuringSuspend(SceUID pid,
							       const MtpStatus *currentStatus)
{
	lock();
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		IPMI::Session *session = m_slots[i].session;
		if (session == NULL || session == MTP_SESSION_PREPARING ||
		    (m_slots[i].flags & MTP_SESSION_FLAG_DISABLE_NOTIFY) != 0 ||
		    session->getProcessId() != pid)
			continue;

		SceUInt32 timeout = MTP_MSG_SEND_TIMEOUT_US;
		MtpStatusEventMsg currentEvent;
		currentEvent.eventCode = 0;
		sce_paf_memcpy(&currentEvent.hostStatus, currentStatus, sizeof(MtpStatus));

		int prevState = m_slots[i].lastHostState;
		int curState = currentStatus->state;
		bool sendCurrentEvent = false;

		if (!isStatusActiveTransferState(prevState) &&
		    isStatusActiveTransferState(curState)) {
			sendCurrentEvent = true;
		} else if ((m_slots[i].flags & MTP_SESSION_FLAG_PENDING_DISCONNECT) != 0) {
			MtpBroadcastMessage msg;
			msg.msgType = MTP_MSG_EVENT_STATUS;
			msg.body.statusEvent.eventCode = MTP_HOST_EVENT_DISCONNECTED;
			sce_paf_memcpy(&msg.body.statusEvent.hostStatus,
				       &m_slots[i].deferredStatus, sizeof(MtpStatus));
			int ret = session->sendMsg(0, &msg, sizeof(msg), &timeout);
			if (ret < 0)
				sceClibPrintf(
					"[MTP %s] Error %s(%u): Failed to send message 0x%x\n",
					"SESMGR", "BroadcastMtpStatusEventDuringSuspend", 772, ret);
			if (isStatusConnectedState(curState))
				sendCurrentEvent = true;
		} else if ((m_slots[i].flags & MTP_SESSION_FLAG_PENDING_RESET) != 0) {
			MtpBroadcastMessage msg;
			msg.msgType = MTP_MSG_EVENT_STATUS;
			currentEvent.eventCode = MTP_HOST_EVENT_HOST_RESET;
			sce_paf_memcpy(&msg.body.statusEvent, &currentEvent,
				       sizeof(MtpStatusEventMsg));
			int ret = session->sendMsg(0, &msg, sizeof(msg), &timeout);
			if (ret < 0)
				sceClibPrintf(
					"[MTP %s] Error %s(%u): Failed to send message 0x%x\n",
					"SESMGR", "BroadcastMtpStatusEventDuringSuspend", 782, ret);
			if (isStatusActiveTransferState(curState))
				sendCurrentEvent = true;
		} else if (!isStatusConnectedState(prevState) && curState == 2) {
			sendCurrentEvent = true;
		}

		if (sendCurrentEvent) {
			switch (curState) {
			case 2:
				currentEvent.eventCode = MTP_HOST_EVENT_CONNECTED;
				break;
			case 3:
				currentEvent.eventCode = MTP_HOST_EVENT_READY;
				break;
			case 4:
				currentEvent.eventCode = MTP_HOST_EVENT_AVAILABLE;
				break;
			default:
				break;
			}
			MtpBroadcastMessage msg;
			msg.msgType = MTP_MSG_EVENT_STATUS;
			sce_paf_memcpy(&msg.body.statusEvent, &currentEvent,
				       sizeof(MtpStatusEventMsg));
			int ret = session->sendMsg(0, &msg, sizeof(msg), &timeout);
			if (ret < 0)
				sceClibPrintf(
					"[MTP %s] Error %s(%u): Failed to send message 0x%x\n",
					"SESMGR", "BroadcastMtpStatusEventDuringSuspend", 803, ret);
		}

		m_slots[i].flags &=
			~(MTP_SESSION_FLAG_PENDING_DISCONNECT | MTP_SESSION_FLAG_PENDING_RESET);
		m_slots[i].lastHostState = curState;
	}
	unlock();
	return 0;
}

/* 0x812B4506 */
int MtpServiceSessionMgr::broadcastNetMtpRegisterEvent(const SceUInt32 *eventCode)
{
	lock();
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		IPMI::Session *session = m_slots[i].session;
		if ((m_slots[i].flags & MTP_SESSION_FLAG_NET_REGISTER) == 0)
			continue;
		if (session == NULL || session == MTP_SESSION_PREPARING ||
		    (m_slots[i].flags & MTP_SESSION_FLAG_DISABLE_NOTIFY) != 0)
			continue;

		SceAppMgrAppStatus appStatus;
		sceAppMgrGetStatusByAppId(session->getAppId(), &appStatus);
		if (appStatus.activated != 0) {
			SceUInt32 timeout = MTP_MSG_SEND_TIMEOUT_US;
			MtpBroadcastMessage msg;
			msg.msgType = MTP_MSG_EVENT_NET_REGISTER;
			msg.body.registerEvent = *eventCode;
			int ret = session->sendMsg(0, &msg, sizeof(msg), &timeout);
			if (ret < 0)
				sceClibPrintf(
					"[MTP %s] Error %s(%u): Failed to send message 0x%x\n",
					"SESMGR", "BroadcastNetMtpRegisterEvent", 842, ret);
		} else {
			m_slots[i].deferredRegisterEvents.push_back(*eventCode);
		}
	}
	unlock();
	return 0;
}

/* 0x812B4638 */
int MtpServiceSessionMgr::broadcastNetMtpRegisterEventDuringSuspend(SceUID pid)
{
	lock();
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		IPMI::Session *session = m_slots[i].session;
		if (session == NULL || session == MTP_SESSION_PREPARING ||
		    (m_slots[i].flags & MTP_SESSION_FLAG_DISABLE_NOTIFY) != 0 ||
		    session->getProcessId() != pid ||
		    (m_slots[i].flags & MTP_SESSION_FLAG_NET_REGISTER) == 0)
			continue;

		while (m_slots[i].deferredRegisterEvents.size() != 0) {
			SceUInt32 timeout = MTP_MSG_SEND_TIMEOUT_US;
			SceUInt32 regEvent = m_slots[i].deferredRegisterEvents.front();
			m_slots[i].deferredRegisterEvents.pop_front();

			MtpBroadcastMessage msg;
			msg.msgType = MTP_MSG_EVENT_NET_REGISTER;
			msg.body.registerEvent = regEvent;
			int ret = session->sendMsg(0, &msg, sizeof(msg), &timeout);
			if (ret < 0)
				sceClibPrintf(
					"[MTP %s] Error %s(%u): Failed to send message 0x%x\n",
					"SESMGR", "BroadcastNetMtpRegisterEventDuringSuspend",
					881, ret);
		}
	}
	unlock();
	return 0;
}

/* 0x812B47A4 */
int MtpServiceSessionMgr::broadcastMtpBgCopyEvent(const SceUInt32 *eventCode)
{
	lock();
	for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
		IPMI::Session *session = m_slots[i].session;
		SceUInt32 timeout = MTP_MSG_SEND_TIMEOUT_US;
		if (session == NULL || session == MTP_SESSION_PREPARING ||
		    (m_slots[i].flags & MTP_SESSION_FLAG_DISABLE_NOTIFY) != 0)
			continue;

		MtpBroadcastMessage msg;
		msg.msgType = MTP_MSG_EVENT_BGCOPY;
		msg.body.bgCopyEvent = *eventCode;
		int ret = session->sendMsg(0, &msg, sizeof(msg), &timeout);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): Failed to send message 0x%x\n",
				      "SESMGR", "BroadcastMtpBgCopyEvent", 902, ret);
	}
	unlock();
	return 0;
}

/* 0x812B48AA */
bool MtpServiceSessionMgr::hasSessionForProcess(SceUID pid)
{
	SceUInt32 slotIndex = 0;
	return findSlotByProcessId(pid, &slotIndex) == 0;
}
