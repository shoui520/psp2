/*
 * DTCPIP-SW AKE configuration, state tracking, session table, and base session.
 */

#include "../engine/sw_engine.h"

#include <string.h>

/* ---- SwAkeConfig ---- */

SwAkeConfig::SwAkeConfig(uint8_t akeProc, uint8_t exchKeyType, uint8_t versionFlag)
	: m_akeProcedure(akeProc), m_exchKeyType(exchKeyType),
	  m_versionFlag(versionFlag), m_reserved(0)
{
}

SwAkeConfig::~SwAkeConfig() {}

SwAkeConfig &SwAkeConfig::operator=(const SwAkeConfig &other)
{
	if (this != &other) {
		m_akeProcedure = other.m_akeProcedure;
		m_exchKeyType = other.m_exchKeyType;
		m_versionFlag = other.m_versionFlag;
	}
	return *this;
}

/* ---- SwAkeProcedureInfo ---- */

SwAkeProcedureInfo::SwAkeProcedureInfo()
	: m_akeProcedure(0), m_exchKeyType(0), m_akeLabel(0), m_isMove(0)
{
}

SwAkeProcedureInfo::~SwAkeProcedureInfo() {}

int SwAkeProcedureInfo::set(uint8_t akeProc, uint8_t exchKeyType, uint8_t akeLabel)
{
	if ((akeProc != 0 && akeProc != 1 && akeProc != 2 && akeProc != 4 && akeProc != 8) ||
	    (exchKeyType & 0x0Fu) != exchKeyType) {
		m_akeProcedure = 0;
		m_exchKeyType = 0;
		m_akeLabel = 0;
		return 5;
	}
	m_akeProcedure = akeProc;
	m_exchKeyType = exchKeyType;
	m_akeLabel = akeLabel;
	m_isMove = 0;
	return 0;
}

void SwAkeProcedureInfo::setMove(uint8_t akeLabel)
{
	m_akeProcedure = 0;
	m_exchKeyType = 0;
	m_akeLabel = akeLabel;
	m_isMove = 1;
}

/* ---- SwAkeStateTracker ---- */

SwAkeStateTracker::SwAkeStateTracker(SwAkePhase initial)
	: m_phase(initial), m_mask(0)
{
}

SwAkeStateTracker::~SwAkeStateTracker() {}

void SwAkeStateTracker::transition(SwAkePhase nextPhase, uint64_t allowedMask)
{
	m_phase = nextPhase;
	m_mask.setMask(allowedMask);
}

bool SwAkeStateTracker::isInAuth() const
{
	return m_phase >= SW_AKE_PHASE_AUTH_START &&
	       m_phase <= SW_AKE_PHASE_WAIT_RTT_VERIFY;
}

bool SwAkeStateTracker::isInSrm() const
{
	return m_phase == SW_AKE_PHASE_WAIT_SRM_CMD ||
	       m_phase == SW_AKE_PHASE_WAIT_SRM_RSP;
}

bool SwAkeStateTracker::isInMove() const
{
	return m_phase >= SW_AKE_PHASE_WAIT_MV_INIT_RSP &&
	       m_phase <= SW_AKE_PHASE_WAIT_MV_COMPLETE_RSP;
}

/* ---- SwAkeSessionTable ---- */

SwAkeSessionTable::SwAkeSessionTable()
	: m_sinkSessions(), m_moveSessions()
{
}

SwAkeSessionTable::~SwAkeSessionTable()
{
	m_moveSessions.clearAll();
	m_sinkSessions.clearAll();
}

int SwAkeSessionTable::addSinkSession(const SwNetAddress &addr, uint32_t sessionId,
				      uint8_t flags)
{
	SwAkeSessionEntry *existing = m_sinkSessions.findByAddr(addr);
	if (existing != NULL) {
		existing->sessionId = sessionId;
		existing->akeLabel = flags;
		existing->authenticated = 1;
		existing->active = 1;
		return 0;
	}
	SwAkeSessionEntry *entry = new (std::nothrow) SwAkeSessionEntry();
	if (entry == NULL)
		return 2;
	entry->sessionId = sessionId;
	entry->peerAddr.set(addr.host(), addr.port());
	entry->akeLabel = flags;
	entry->kxmLabel = 0;
	entry->isMove = 0;
	entry->authenticated = 1;
	entry->keyExpireSec = 0;
	entry->active = 1;
	int err = m_sinkSessions.add(entry);
	if (err != 0)
		delete entry;
	return err;
}

int SwAkeSessionTable::removeSinkSession(const SwNetAddress &addr)
{
	SwAkeSessionEntry *entry = m_sinkSessions.findByAddr(addr);
	if (entry == NULL)
		return 100;
	uint32_t sid = entry->sessionId;
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) == 0 && dev != NULL)
		dev->closeSession(sid);
	return m_sinkSessions.removeById(sid);
}

int SwAkeSessionTable::getSinkSessionId(const SwNetAddress &addr, uint32_t *outSessionId)
{
	if (outSessionId == NULL || !addr.isValid())
		return 5;
	SwAkeSessionEntry *entry = m_sinkSessions.findByAddr(addr);
	if (entry == NULL)
		return 100;
	*outSessionId = entry->sessionId;
	return 0;
}

int SwAkeSessionTable::findSinkSessionIds(const SwNetAddress &addr,
					  SwPtrList<uint32_t> &outIds)
{
	SwAkeSessionEntry *entry = m_sinkSessions.findByAddr(addr);
	if (entry == NULL)
		return 0;
	uint32_t *copy = new (std::nothrow) uint32_t(entry->sessionId);
	if (copy == NULL)
		return 2;
	return outIds.pushBackPtr(copy);
}

int SwAkeSessionTable::checkSinkSessionCkc(uint32_t sessionId, SwNetAddress *outAddr,
					   uint8_t *outNeedCkc)
{
	if (outNeedCkc != NULL)
		*outNeedCkc = 0;
	SwAkeSessionEntry *entry = m_sinkSessions.findById(sessionId);
	if (entry == NULL)
		return 100;
	if (outAddr != NULL)
		outAddr->set(entry->peerAddr.host(), entry->peerAddr.port());
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) == 0 && dev != NULL && outNeedCkc != NULL) {
		uint8_t ok = 0;
		if (dev->checkCkc(&ok, (uint8_t)sessionId, 0) == 0)
			*outNeedCkc = (ok != 0) ? 1 : 0;
	}
	return 0;
}

int SwAkeSessionTable::addMoveSession(const SwNetAddress &addr, uint32_t sessionId,
				      uint8_t label, uint32_t keyExpireSec, uint8_t flags)
{
	SwAkeSessionEntry *existing = m_moveSessions.findByLabel(label);
	if (existing != NULL) {
		existing->sessionId = sessionId;
		existing->peerAddr.set(addr.host(), addr.port());
		existing->akeLabel = flags;
		existing->keyExpireSec = keyExpireSec;
		existing->authenticated = 1;
		existing->active = 1;
		return 0;
	}
	SwAkeSessionEntry *entry = new (std::nothrow) SwAkeSessionEntry();
	if (entry == NULL)
		return 2;
	entry->sessionId = sessionId;
	entry->peerAddr.set(addr.host(), addr.port());
	entry->akeLabel = flags;
	entry->kxmLabel = label;
	entry->isMove = 1;
	entry->authenticated = 1;
	entry->keyExpireSec = keyExpireSec;
	entry->active = 1;
	int err = m_moveSessions.add(entry);
	if (err != 0)
		delete entry;
	return err;
}

int SwAkeSessionTable::removeMoveSession(const SwNetAddress &addr, uint8_t label)
{
	(void)addr;
	SwAkeSessionEntry *entry = m_moveSessions.findByLabel(label);
	if (entry == NULL)
		return 100;
	uint32_t sid = entry->sessionId;
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) == 0 && dev != NULL)
		dev->closeSession(sid);
	return m_moveSessions.removeById(sid);
}

int SwAkeSessionTable::getMoveSessionId(const SwNetAddress &addr, uint8_t label,
					uint32_t *outSessionId)
{
	if (outSessionId == NULL || !addr.isValid())
		return 5;
	SwAkeSessionEntry *entry = m_moveSessions.findByLabel(label);
	if (entry == NULL)
		return 100;
	*outSessionId = entry->sessionId;
	return 0;
}

int SwAkeSessionTable::findMoveSessionIds(const SwNetAddress &addr, uint8_t label,
					  SwPtrList<uint32_t> &outIds)
{
	(void)addr;
	SwAkeSessionEntry *entry = m_moveSessions.findByLabel(label);
	if (entry == NULL)
		return 0;
	uint32_t *copy = new (std::nothrow) uint32_t(entry->sessionId);
	if (copy == NULL)
		return 2;
	return outIds.pushBackPtr(copy);
}

int SwAkeSessionTable::hasMoveSession(uint8_t *outFound, const SwNetAddress &addr,
				      uint8_t label)
{
	if (outFound == NULL)
		return 5;
	*outFound = 0;
	if (!addr.isValid())
		return 5;
	SwAkeSessionEntry *entry = m_moveSessions.findByLabel(label);
	if (entry != NULL)
		*outFound = 1;
	return 0;
}

int SwAkeSessionTable::checkMoveSessionCkc(uint32_t sessionId, uint8_t label,
					   SwNetAddress *outAddr, uint8_t *outNeedCkc)
{
	if (outNeedCkc != NULL)
		*outNeedCkc = 0;
	SwAkeSessionEntry *entry = m_moveSessions.findById(sessionId);
	if (entry == NULL)
		entry = m_moveSessions.findByLabel(label);
	if (entry == NULL)
		return 100;
	if (outAddr != NULL)
		outAddr->set(entry->peerAddr.host(), entry->peerAddr.port());
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) == 0 && dev != NULL && outNeedCkc != NULL) {
		uint8_t ok = 0;
		if (dev->checkCkc(&ok, (uint8_t)entry->sessionId, 1) == 0)
			*outNeedCkc = (ok != 0) ? 1 : 0;
	}
	return 0;
}

int SwAkeSessionTable::closeAllSessions()
{
	m_sinkSessions.clearAll();
	m_moveSessions.clearAll();
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) != 0 || dev == NULL)
		return 1;
	return dev->closeSession(255);
}

/* ---- SwAkeSessionBase ---- */

SwAkeSessionBase::SwAkeSessionBase(uint32_t sessionId, const SwAkeConfig &config,
				   SwAkeTcpChannel *channel, uint8_t isMoveSession)
	: m_sessionId(sessionId), m_isMove(isMoveSession), m_moveLabelValid(0),
	  m_moveLabel(0), m_active(1), m_moveSessionId(sessionId), m_config(config),
	  m_channel(channel), m_procInfo(), m_keyType(0), m_keyPending(0),
	  m_srmUpdateNeeded(0), m_state(SW_AKE_PHASE_IDLE), m_cmdTimer(NULL),
	  m_localSrm(), m_peerSrm(), m_localCert(), m_peerCert(),
	  m_rttN(0xFFFFu), m_peerSrmVersion(0), m_peerSrmGen(0), m_moveSubOp(0),
	  m_peerAddr(), m_localHost(), m_lastError(0)
{
	m_reservedA[0] = 0;
	m_reservedA[1] = 0;
	m_reservedB[0] = 0;
	m_reservedB[1] = 0;
	memset(m_localNonce, 0, sizeof(m_localNonce));
	memset(m_peerNonce, 0, sizeof(m_peerNonce));
	memset(m_ephemeralPriv, 0, sizeof(m_ephemeralPriv));
	memset(m_rttMacA, 0, sizeof(m_rttMacA));
	memset(m_rttMacB, 0, sizeof(m_rttMacB));
}

SwAkeSessionBase::~SwAkeSessionBase()
{
	cleanupObjects();
}

void SwAkeSessionBase::cleanupObjects()
{
	if (m_cmdTimer != NULL) {
		m_cmdTimer->stopTimer();
		delete m_cmdTimer;
		m_cmdTimer = NULL;
	}
}

void SwAkeSessionBase::cancelSession()
{
	m_active = 0;
	if (m_channel != NULL)
		m_channel->cancel();
	if (m_cmdTimer != NULL)
		m_cmdTimer->stopTimer();
}

void SwAkeSessionBase::stopChannel()
{
	m_active = 0;
	if (m_channel != NULL)
		m_channel->cancel();
}

int SwAkeSessionBase::initSession()
{
	cleanupObjects();
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) != 0 || dev == NULL)
		return 1;
	int err = dev->getDeviceCert(&m_localCert);
	if (err != 0)
		return err;
	err = dev->getSrm(&m_localSrm);
	if (err != 0)
		return err;
	if (m_channel != NULL)
		m_channel->getPeerAddress(m_peerAddr);
	m_cmdTimer = new (std::nothrow) SwAkeCmdTimerThread(this);
	if (m_cmdTimer == NULL)
		return 2;
	return 0;
}

int SwAkeSessionBase::checkPeerTtl()
{
	if (m_channel == NULL)
		return 1023;
	int ttl = m_channel->getPeerTtl();
	if (ttl <= 0 || ttl > 3)
		return 1023;
	return 0;
}

int SwAkeSessionBase::computeRttMacPair(uint8_t outMac10[10], uint8_t mode)
{
	if (outMac10 == NULL)
		return 5;
	if (m_rttN > 0x03FFu)
		return 1;
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) != 0 || dev == NULL)
		return 1;
	uint8_t mac20[20];
	memset(mac20, 0, sizeof(mac20));
	uint8_t sid = (uint8_t)(m_isMove ? m_moveSessionId : m_sessionId);
	int err = dev->computeRttMac(mac20, sid, (uint8_t)m_keyType,
				     (uint16_t)(m_rttN + 1u), mode);
	if (err == 0) {
		memcpy(m_rttMacA, mac20, 10);
		memcpy(m_rttMacB, mac20 + 10, 10);
		memcpy(outMac10, mac20, 10);
	}
	return err;
}

int SwAkeSessionBase::sendPacket(SwAkePacket *pkt)
{
	if (m_channel == NULL || pkt == NULL)
		return 4;
	return m_channel->sendPacket(pkt);
}

int SwAkeSessionBase::sendAckResponse(const SwAkePacket *req, uint8_t statusCode)
{
	if (m_channel == NULL || req == NULL)
		return 4;
	SwAkePacket rsp(SW_AKE_PKT_AKE_STATUS_RSP, 7, req->frameType(), 0x09,
			req->subfunction(), req->akeLabel(), req->exchangeKeyByte(),
			0, req->exchKeyType(), req->subfnParam(),
			req->akeProcedure(), statusCode,
			req->versionHi(), req->versionLo());
	return m_channel->sendPacket(&rsp);
}

int SwAkeSessionBase::sendSrmPacket(const void *srmData, uint32_t srmLen)
{
	SwAkePacket *pkt = SwAkePacket::createSrm(false, m_procInfo.akeLabel(),
						  m_procInfo.akeProcedure(), 0x0F,
						  srmData, srmLen);
	if (pkt == NULL)
		return 2;
	int err = sendPacket(pkt);
	delete pkt;
	return err != 0 ? 1512 : 0;
}

int SwAkeSessionBase::handleSrmCommand(SwAkePacket *pkt)
{
	if (pkt == NULL) {
		onSrmError(1, 1);
		return 1;
	}
	int err = sendAckResponse(pkt, 0x0F);
	if (err != 0) {
		onSrmError(err, 1);
		return err;
	}
	err = m_peerSrm.setRaw(pkt->payload(), pkt->payloadSize(), &m_peerCert);
	if (err == 0 && m_peerSrm.version() >= m_peerSrmVersion) {
		SdiDevice *dev = NULL;
		if (SdiDevice::getInstance(&dev) == 0 && dev != NULL) {
			err = dev->setSrm(&m_peerSrm);
			if (err == 1015)
				err = 0;
		} else {
			err = 1;
		}
	}
	if (err != 0) {
		onSrmError(err, 1);
		return err;
	}
	m_state.transition(SW_AKE_PHASE_DONE, 0);
	onAuthComplete();
	stopChannel();
	return 0;
}

int SwAkeSessionBase::handleSrmResponse(SwAkePacket *pkt)
{
	if (pkt == NULL) {
		onSrmError(1, 1);
		return 1;
	}
	m_state.transition(SW_AKE_PHASE_DONE, 0);
	onAuthComplete();
	stopChannel();
	return 0;
}

int SwAkeSessionBase::handleStatusCommand(SwAkePacket *pkt)
{
	if (pkt == NULL)
		return 1;
	return sendAckResponse(pkt, 0x0F);
}

void SwAkeSessionBase::onAuthComplete() {}
void SwAkeSessionBase::onKeyCommitted() {}

void SwAkeSessionBase::onAuthError(int err, uint8_t sendStatus)
{
	(void)sendStatus;
	m_lastError = err;
	m_state.transition(SW_AKE_PHASE_ERROR, 0);
	stopChannel();
}

void SwAkeSessionBase::onSrmError(int err, uint8_t sendStatus)
{
	(void)sendStatus;
	m_lastError = err;
	m_state.transition(SW_AKE_PHASE_ERROR, 0);
	stopChannel();
}

void SwAkeSessionBase::onMoveError(int err, uint8_t sendStatus)
{
	(void)sendStatus;
	m_lastError = err;
	m_state.transition(SW_AKE_PHASE_ERROR, 0);
	stopChannel();
}

int SwAkeSessionBase::handleReceivedPacket(SwAkePacket *pkt)
{
	if (pkt == NULL)
		return 1;
	if (pkt->isStatusCommand())
		return handleStatusCommand(pkt);
	return 1;
}

void SwAkeSessionBase::onAkeTimeout(int errorCode)
{
	if (m_state.isDone() || m_state.isError())
		return;
	if (m_state.isInAuth())
		onAuthError(errorCode, 1);
	else if (m_state.isInSrm())
		onSrmError(errorCode, 1);
	else if (m_state.isInMove())
		onMoveError(errorCode, 1);
}
