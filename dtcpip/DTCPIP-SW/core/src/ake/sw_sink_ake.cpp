/*
 * DTCPIP-SW sink AKE session state machine, sink authentication thread,
 * and periodic Content Key Confirmation (CKC) verification thread.
 */

#include "../engine/sw_engine.h"

#include <string.h>

/* ---- SwSinkAkeSession ---- */

SwSinkAkeSession::SwSinkAkeSession(uint8_t isMoveSubOp, uint8_t isMoveFinalize,
				   uint32_t sessionId, const SwAkeConfig &config,
				   SwAkeTcpChannel *channel, SwContent *content,
				   uint8_t isMoveAuth)
	: SwAkeSessionBase(sessionId, config, channel, isMoveAuth),
	  m_content(content), m_thread(NULL), m_authenticated(0),
	  m_isMoveFinalize(isMoveFinalize), m_hasMoveMsg(0), m_reservedC(0)
{
	m_moveSubOp = isMoveSubOp;
	memset(&m_moveMsg, 0, sizeof(m_moveMsg));
}

SwSinkAkeSession::~SwSinkAkeSession() {}

int SwSinkAkeSession::attachThread(SwSinkAuthThread *thread)
{
	m_thread = thread;
	return initSession();
}

bool SwSinkAkeSession::getActiveMoveLabel(uint8_t *outLabel) const
{
	if (outLabel == NULL)
		return false;
	if (m_hasMoveMsg) {
		*outLabel = m_moveMsg.label;
		return true;
	}
	*outLabel = 0;
	return false;
}

int SwSinkAkeSession::notifySrmUpdated()
{
	if (m_content == NULL || m_content->observer() == NULL)
		return 1;
	void *rawPtr = NULL;
	size_t rawLen = 0;
	int err = m_peerSrm.getRaw(&rawPtr, &rawLen);
	if (err == 0)
		m_content->observer()->onSrmUpdated(m_peerAddr, rawPtr, rawLen);
	return err;
}

void SwSinkAkeSession::onKeyCommitted()
{
	if (m_content == NULL)
		return;
	int err = 0;
	if (m_isMove) {
		err = m_content->registerAuthenticatedMove(m_peerAddr, m_sessionId,
							   m_moveLabel, m_thread);
		if (err == 0) {
			m_authenticated = 1;
			if (m_content->observer() != NULL)
				m_content->observer()->onMoveAuthSucceeded(m_peerAddr,
									   m_sessionId);
		}
	} else {
		err = m_content->registerAuthenticatedSink(m_peerAddr, m_sessionId, m_thread);
		if (err == 0) {
			m_authenticated = 1;
			if (m_content->observer() != NULL) {
				m_content->observer()->onLegacySinkAuthSuccess(m_peerAddr);
				m_content->observer()->onSinkAuthSuccess(m_peerAddr, m_sessionId);
			}
		}
	}
}

void SwSinkAkeSession::onAuthComplete()
{
	if (m_peerSrm.length() > 0)
		notifySrmUpdated();
	m_state.transition(SW_AKE_PHASE_DONE, 0);
}

void SwSinkAkeSession::onAuthError(int err, uint8_t sendStatus)
{
	m_lastError = err;
	if (sendStatus && m_channel != NULL) {
		SwAkePacket *st = SwAkePacket::createStatus(false, m_procInfo.akeLabel(),
							    m_procInfo.akeProcedure(), 0x0F);
		if (st != NULL) {
			sendPacket(st);
			delete st;
		}
	}
	if (m_content != NULL && m_content->observer() != NULL) {
		if (m_isMove) {
			m_content->observer()->onMoveAuthFailed(m_peerAddr, m_sessionId, err);
		} else {
			m_content->observer()->onLegacySinkAuthFailed(m_peerAddr, err);
			m_content->observer()->onSinkAuthFailed(m_peerAddr, m_sessionId, err);
		}
	}
	m_state.transition(SW_AKE_PHASE_ERROR, 0);
	stopChannel();
}

void SwSinkAkeSession::onSrmError(int err, uint8_t sendStatus)
{
	(void)sendStatus;
	m_lastError = err;
	m_state.transition(SW_AKE_PHASE_ERROR, 0);
	if (m_content != NULL && m_content->observer() != NULL)
		m_content->observer()->onSinkSrmTxFailed(m_peerAddr, err);
	stopChannel();
}

void SwSinkAkeSession::notifyMoveFinalizeError(int err, uint8_t label)
{
	m_lastError = err;
	m_state.transition(SW_AKE_PHASE_ERROR, 0);
	if (m_content != NULL && m_content->observer() != NULL)
		m_content->observer()->onMoveFinalizeFailed(m_peerAddr, label, err);
	stopChannel();
}

void SwSinkAkeSession::notifyMoveCompleteError(int err, uint8_t label)
{
	m_lastError = err;
	m_state.transition(SW_AKE_PHASE_ERROR, 0);
	if (m_content != NULL && m_content->observer() != NULL)
		m_content->observer()->onMoveCompleteFailed(m_peerAddr, label, err);
	stopChannel();
}

void SwSinkAkeSession::onMoveError(int err, uint8_t sendStatus)
{
	(void)sendStatus;
	if (m_state.isMoveFinalize())
		notifyMoveFinalizeError(err, m_moveMsg.label);
	else if (m_state.isMoveComplete())
		notifyMoveCompleteError(err, m_moveMsg.label);
	else
		onAuthError(err, 0);
}

int SwSinkAkeSession::sendChallengeCommand()
{
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) != 0 || dev == NULL)
		return 1;
	int err = dev->getRandom(m_localNonce, sizeof(m_localNonce));
	if (err != 0)
		return err;
	const void *certData = NULL;
	uint32_t certLen = 0;
	err = m_localCert.getRaw(&certData, &certLen);
	if (err != 0)
		return err;
	SwAkePacket *pkt = SwAkePacket::createChallenge(false, m_procInfo.akeLabel(),
							m_procInfo.akeProcedure(), 0x0F,
							m_localNonce, certData, certLen);
	if (pkt == NULL)
		return 2;
	err = sendPacket(pkt);
	delete pkt;
	return err != 0 ? 1508 : 0;
}

int SwSinkAkeSession::runAuth(uint8_t akeProc, uint8_t exchKeyType)
{
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) != 0 || dev == NULL) {
		onAuthError(4, 1);
		return 4;
	}
	int err = 0;
	for (unsigned int attempt = 0; attempt < 10; ++attempt) {
		err = dev->getExchangeKeyLabel(&m_keyType, (uint8_t)m_sessionId);
		if (err == 1507) {
			swSleepMs(1000);
			continue;
		}
		break;
	}
	if (err == 0) {
		m_keyPending = 1;
		uint8_t label = (uint8_t)(m_isMove ? m_moveSessionId : m_keyType);
		err = m_procInfo.set(akeProc, exchKeyType, label);
		if (err == 0) {
			if (m_isMove) {
				SwAkePacket *mvInit = SwAkePacket::createMovePacket(
					SW_AKE_SUBFN_MV_INIT, false,
					m_procInfo.akeLabel(), 0,
					m_procInfo.akeProcedure(), 0x0F, NULL, 0);
				if (mvInit == NULL) {
					err = 2;
				} else {
					err = sendPacket(mvInit);
					delete mvInit;
					if (err != 0)
						err = 1517;
				}
			} else {
				err = sendChallengeCommand();
			}
		}
	}
	if (err != 0) {
		onAuthError(err, 1);
		return err;
	}
	if (m_isMove) {
		m_state.transition(SW_AKE_PHASE_WAIT_MV_INIT_RSP, SW_AKE_PKT_MV_INIT_CMD |
				   SW_AKE_PKT_REJECT_RSP | SW_AKE_PKT_AKE_STATUS_RSP);
	} else {
		m_state.transition(SW_AKE_PHASE_WAIT_CHALLENGE_RSP, SW_AKE_PKT_CHALLENGE_RSP |
				   SW_AKE_PKT_REJECT_RSP | SW_AKE_PKT_AKE_STATUS_RSP);
	}
	if (m_cmdTimer != NULL)
		m_cmdTimer->startTimer(30000);
	err = runReceiveLoop();
	cleanupObjects();
	return err;
}

int SwSinkAkeSession::runMoveFinalize(const struct SwMwMoveMessage *msg, uint8_t akeProc,
				      uint8_t exchKeyType)
{
	if (msg == NULL) {
		notifyMoveFinalizeError(4, 0);
		return 4;
	}
	int err = m_procInfo.set(akeProc, exchKeyType, 0xFF);
	if (err == 0) {
		uint8_t body[18];
		memcpy(body, msg->key, 8);
		memcpy(body + 8, msg->first, 10);
		SwAkePacket *pkt = SwAkePacket::createMovePacket(
			SW_AKE_SUBFN_MV_FINALIZE, false, 0xFF, msg->label,
			m_procInfo.akeProcedure(), 0x0F, body, sizeof(body));
		if (pkt == NULL) {
			err = 2;
		} else {
			err = sendPacket(pkt);
			delete pkt;
			if (err != 0)
				err = 1017;
		}
	}
	if (err != 0) {
		notifyMoveFinalizeError(err, msg->label);
		return err;
	}
	m_moveMsg = *msg;
	m_hasMoveMsg = 1;
	m_state.transition(SW_AKE_PHASE_WAIT_MV_FINALIZE_RSP,
			   SW_AKE_PKT_MV_FINALIZE_CMD | SW_AKE_PKT_REJECT_RSP);
	if (m_cmdTimer != NULL)
		m_cmdTimer->startTimer(30000);
	err = runReceiveLoop();
	cleanupObjects();
	return err;
}

int SwSinkAkeSession::runMoveComplete(uint8_t label, uint8_t akeProc, uint8_t exchKeyType)
{
	int err = m_procInfo.set(akeProc, exchKeyType, 0xFF);
	if (err == 0) {
		SwAkePacket *pkt = SwAkePacket::createMovePacket(
			SW_AKE_SUBFN_MV_COMPLETE, false, 0xFF, label,
			m_procInfo.akeProcedure(), 0x0F, NULL, 0);
		if (pkt == NULL) {
			err = 2;
		} else {
			err = sendPacket(pkt);
			delete pkt;
			if (err != 0)
				err = 1017;
		}
	}
	if (err != 0) {
		notifyMoveCompleteError(err, label);
		return err;
	}
	m_moveMsg.label = label;
	m_hasMoveMsg = 1;
	m_state.transition(SW_AKE_PHASE_WAIT_MV_COMPLETE_RSP, SW_AKE_PKT_REJECT_RSP);
	if (m_cmdTimer != NULL)
		m_cmdTimer->startTimer(30000);
	err = runReceiveLoop();
	cleanupObjects();
	return err;
}

int SwSinkAkeSession::sendMoveFinalizeOnActive(const struct SwMwMoveMessage *msg)
{
	if (msg == NULL || m_channel == NULL) {
		notifyMoveFinalizeError(4, msg ? msg->label : 0);
		return 4;
	}
	uint8_t body[18];
	memcpy(body, msg->key, 8);
	memcpy(body + 8, msg->first, 10);
	SwAkePacket *pkt = SwAkePacket::createMovePacket(
		SW_AKE_SUBFN_MV_FINALIZE, false, 0xFF, msg->label,
		m_procInfo.akeProcedure(), 0x0F, body, sizeof(body));
	if (pkt == NULL) {
		notifyMoveFinalizeError(2, msg->label);
		return 2;
	}
	m_moveMsg = *msg;
	m_hasMoveMsg = 1;
	m_state.transition(SW_AKE_PHASE_WAIT_MV_FINALIZE_RSP,
			   SW_AKE_PKT_MV_FINALIZE_CMD | SW_AKE_PKT_REJECT_RSP);
	int err = sendPacket(pkt);
	delete pkt;
	if (err != 0) {
		notifyMoveFinalizeError(1017, msg->label);
		return 1017;
	}
	return 0;
}

int SwSinkAkeSession::sendMoveCompleteOnActive(uint8_t label)
{
	if (m_channel == NULL) {
		notifyMoveCompleteError(4, label);
		return 4;
	}
	SwAkePacket *pkt = SwAkePacket::createMovePacket(
		SW_AKE_SUBFN_MV_COMPLETE, false, 0xFF, label,
		m_procInfo.akeProcedure(), 0x0F, NULL, 0);
	if (pkt == NULL) {
		notifyMoveCompleteError(2, label);
		return 2;
	}
	m_moveMsg.label = label;
	m_hasMoveMsg = 1;
	m_state.transition(SW_AKE_PHASE_WAIT_MV_COMPLETE_RSP, SW_AKE_PKT_REJECT_RSP);
	int err = sendPacket(pkt);
	delete pkt;
	if (err != 0) {
		notifyMoveCompleteError(1017, label);
		return 1017;
	}
	return 0;
}

int SwSinkAkeSession::sendMoveCancel(uint8_t label)
{
	SwAkePacket *pkt = SwAkePacket::createMovePacket(
		SW_AKE_SUBFN_MV_CANCEL, false, 0xFF, label,
		m_procInfo.akeProcedure(), 0x0F, NULL, 0);
	if (pkt == NULL)
		return 2;
	int err = sendPacket(pkt);
	delete pkt;
	return err != 0 ? 1017 : 0;
}

int SwSinkAkeSession::runReceiveLoop()
{
	int err = 0;
	while (m_active && !m_state.isDone() && !m_state.isError()) {
		SwAkePacket *pkt = NULL;
		err = m_channel->recvPacket(&pkt, 5000);
		if (err == 9 || err == 1010 || err == 1011 || err == 1035)
			continue;
		if (err != 0) {
			if (err != 1012 && !m_state.isDone() && !m_state.isError())
				onAkeTimeout(err);
			break;
		}
		if (pkt == NULL) {
			onAkeTimeout(1);
			break;
		}
		err = handleReceivedPacket(pkt);
		delete pkt;
		if (err != 0)
			break;
	}
	return m_lastError;
}

int SwSinkAkeSession::handleChallengeResponse(SwAkePacket *pkt)
{
	(void)pkt;
	m_state.transition(SW_AKE_PHASE_WAIT_CHALLENGE_CMD, SW_AKE_PKT_CHALLENGE_CMD);
	return 0;
}

int SwSinkAkeSession::handleChallengeCommand(SwAkePacket *pkt)
{
	int err = sendAckResponse(pkt, 0x0F);
	if (err != 0) {
		onAuthError(err, 1);
		return err;
	}
	if (pkt->payload() == NULL || pkt->payloadSize() <= 16) {
		onAuthError(1, 1);
		return 1;
	}
	memcpy(m_peerNonce, pkt->payload(), 16);
	err = m_peerCert.setRaw(pkt->payload() + 16, pkt->payloadSize() - 16);
	if (err == 0) {
		const void *certData = NULL;
		uint32_t certLen = 0;
		const void *sig = NULL;
		err = m_peerCert.getRaw(&certData, &certLen, &sig, NULL);
		if (err == 0) {
			SdiDevice *dev = NULL;
			if (SdiDevice::getInstance(&dev) == 0 && dev != NULL) {
				err = dev->verifySignature(certData, 48, sig, NULL);
				if (err == 0)
					err = m_peerCert.validateFormat();
				if (err == 0) {
					SwDeviceId *devId = NULL;
					err = m_peerCert.getDeviceIdObj(&devId);
					if (err == 0 && devId != NULL) {
						uint8_t revoked = 0;
						err = dev->checkCertRevoked(&revoked, devId);
						if (err == 0 && revoked != 0)
							err = 1559;
					}
				}
			} else {
				err = 1;
			}
		}
	}
	if (err != 0) {
		if (err == 10001)
			err = 1558;
		onAuthError(err, 1);
		return err;
	}
	m_state.transition(SW_AKE_PHASE_WAIT_RESPONSE_CMD, SW_AKE_PKT_RESPONSE_CMD);
	return 0;
}

int SwSinkAkeSession::handleResponseCommand(SwAkePacket *pkt)
{
	int err = sendAckResponse(pkt, 0x0F);
	if (err != 0) {
		onAuthError(err, 1);
		return err;
	}
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) != 0 || dev == NULL) {
		onAuthError(1, 1);
		return 1;
	}
	if (pkt->payload() != NULL && pkt->payloadSize() >= 83) {
		m_peerSrmVersion = (uint16_t)(((uint16_t)pkt->payload()[40] << 8) |
					      pkt->payload()[41]);
		m_peerSrmGen = pkt->payload()[42] & 0x0Fu;
		err = dev->setRemoteKey((uint8_t)m_sessionId, (uint8_t)m_keyType,
					pkt->payload(), pkt->payload() + 43);
	}
	if (err == 0) {
		uint8_t pub40[40];
		uint8_t sig40[40];
		uint8_t signBuf[68];
		memset(pub40, 0, sizeof(pub40));
		memset(sig40, 0, sizeof(sig40));
		memset(signBuf, 0, sizeof(signBuf));
		err = dev->genKeyPair(pub40, m_ephemeralPriv);
		if (err == 0) {
			memcpy(signBuf, m_peerNonce, 16);
			memcpy(signBuf + 16, pub40, 40);
			uint16_t srmVer = swSwap16(m_localSrm.version());
			memcpy(signBuf + 56, &srmVer, 2);
			signBuf[58] = m_localSrm.generation() & 0x0Fu;
			err = dev->signData(sig40, signBuf, 59);
		}
		if (err == 0) {
			SwAkePacket *rsp = SwAkePacket::createResponse(
				false, m_procInfo.akeLabel(), m_procInfo.akeProcedure(),
				0x0F, pub40, sig40);
			if (rsp == NULL) {
				err = 2;
			} else {
				err = sendPacket(rsp);
				delete rsp;
				if (err != 0)
					err = 1510;
			}
		}
	}
	if (err != 0) {
		onAuthError(err, 1);
		return err;
	}
	m_state.transition(SW_AKE_PHASE_WAIT_RESPONSE_RSP,
			   SW_AKE_PKT_RESPONSE_RSP | SW_AKE_PKT_EXCHANGE_KEY_CMD);
	return 0;
}

int SwSinkAkeSession::handleResponseResponse(SwAkePacket *pkt)
{
	(void)pkt;
	m_state.transition(SW_AKE_PHASE_WAIT_EXCHANGE_KEY_CMD,
			   SW_AKE_PKT_EXCHANGE_KEY_CMD | SW_AKE_PKT_RTT_READY_CMD |
			   SW_AKE_PKT_RTT_SETUP_CMD | SW_AKE_PKT_RTT_TEST_CMD);
	return 0;
}

int SwSinkAkeSession::handleExchangeKeyCommand(SwAkePacket *pkt)
{
	int err = sendAckResponse(pkt, 0x0F);
	if (err != 0) {
		onAuthError(err, 1);
		return err;
	}
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) != 0 || dev == NULL) {
		onAuthError(1, 1);
		return 1;
	}
	if (pkt->payload() == NULL || pkt->payloadSize() < 14) {
		onAuthError(1, 1);
		return 1;
	}
	uint8_t kxLabel = pkt->payload()[0];
	uint8_t kxFlags = pkt->payload()[1];
	err = dev->setExchangeKey((uint8_t)m_sessionId, (uint8_t)m_keyType, 0,
				  pkt->payload() + 2, kxLabel, kxFlags);
	if (err == 0)
		err = dev->commitSourceKey((uint8_t)m_sessionId, (uint8_t)m_keyType);
	if (err != 0) {
		onAuthError(err, 1);
		return err;
	}
	m_moveLabel = kxLabel;
	m_moveLabelValid = 1;
	m_keyPending = 0;
	onKeyCommitted();

	int srmAction = 1;
	void *sendPtr = NULL;
	uint32_t sendLen = 0;
	uint8_t reqGen = 0;
	uint8_t peerCat = 0;
	uint8_t localCat = 0;
	m_peerCert.getCategory(&peerCat);
	m_localCert.getCategory(&localCat);
	err = m_localSrm.compareForExchange(&srmAction, &sendPtr, &sendLen, &reqGen,
					    m_peerSrmVersion, peerCat, m_peerSrmGen,
					    localCat);
	if (err != 0) {
		onSrmError(err, 1);
		return err;
	}
	if (srmAction == 0 && sendPtr != NULL && sendLen > 0) {
		m_state.transition(SW_AKE_PHASE_WAIT_SRM_RSP, SW_AKE_PKT_SRM_RSP);
		err = sendSrmPacket(sendPtr, sendLen);
		if (err != 0) {
			onSrmError(err, 1);
			return err;
		}
	} else if (srmAction == 2) {
		m_state.transition(SW_AKE_PHASE_WAIT_SRM_CMD, SW_AKE_PKT_SRM_CMD);
	} else {
		m_state.transition(SW_AKE_PHASE_DONE, 0);
		onAuthComplete();
		stopChannel();
	}
	return 0;
}

int SwSinkAkeSession::handleRttCommand(SwAkePacket *pkt)
{
	int err = sendAckResponse(pkt, 0x0F);
	if (err != 0) {
		onAuthError(err, 1);
		return err;
	}
	uint8_t subfn = pkt->subfunction();
	if (subfn == SW_AKE_SUBFN_RTT_SETUP) {
		if (pkt->payload() != NULL && pkt->payloadSize() >= 2)
			m_rttN = (uint16_t)(((uint16_t)pkt->payload()[0] << 8) |
					    pkt->payload()[1]);
		uint8_t mac10[10];
		err = computeRttMacPair(mac10, 0);
		if (err != 0) {
			onAuthError(err, 1);
			return err;
		}
	} else if (subfn == SW_AKE_SUBFN_RTT_TEST) {
		SwAkePacket *rsp = SwAkePacket::createRtt(
			SW_AKE_SUBFN_RTT_TEST, true, m_procInfo.akeLabel(),
			m_procInfo.akeProcedure(), 0x0F, m_rttMacB, 10);
		if (rsp != NULL) {
			err = sendPacket(rsp);
			delete rsp;
		}
	} else if (subfn == SW_AKE_SUBFN_RTT_VERIFY) {
		uint8_t mac10[10];
		err = computeRttMacPair(mac10, 1);
		if (err == 0) {
			SwAkePacket *rsp = SwAkePacket::createRtt(
				SW_AKE_SUBFN_RTT_VERIFY, true, m_procInfo.akeLabel(),
				m_procInfo.akeProcedure(), 0x0F, mac10, 10);
			if (rsp != NULL) {
				err = sendPacket(rsp);
				delete rsp;
			}
		}
	}
	return err;
}

int SwSinkAkeSession::handleMoveInitResponse(SwAkePacket *pkt)
{
	(void)pkt;
	int err = sendChallengeCommand();
	if (err != 0) {
		onAuthError(err, 1);
		return err;
	}
	m_state.transition(SW_AKE_PHASE_WAIT_CHALLENGE_RSP, SW_AKE_PKT_CHALLENGE_RSP);
	return 0;
}

int SwSinkAkeSession::handleMoveFinalizeResponse(SwAkePacket *pkt)
{
	uint8_t label = m_moveMsg.label;
	int err = 0;
	if (pkt->payload() == NULL || pkt->payloadSize() < 18) {
		err = 1;
	} else {
		uint8_t rxLabel = pkt->exchKeyType();
		if (rxLabel != m_moveMsg.label)
			err = 1018;
		else if (memcmp(pkt->payload() + 8, m_moveMsg.second, 10) != 0)
			err = 1019;
	}
	if (err != 0) {
		notifyMoveFinalizeError(err, label);
		return err;
	}
	if (m_content != NULL && m_content->observer() != NULL)
		m_content->observer()->onMoveFinalizeAccepted(m_peerAddr, label);
	m_state.transition(SW_AKE_PHASE_DONE, 0);
	return 0;
}

int SwSinkAkeSession::handleMoveCompleteResponse(SwAkePacket *pkt)
{
	uint8_t label = m_moveMsg.label;
	if (pkt->exchKeyType() != m_moveMsg.label) {
		notifyMoveCompleteError(1018, label);
		return 1018;
	}
	if (m_content != NULL && m_content->observer() != NULL)
		m_content->observer()->onMoveCompleteAccepted(m_peerAddr, label);
	m_state.transition(SW_AKE_PHASE_DONE, 0);
	onAuthComplete();
	stopChannel();
	return 0;
}

int SwSinkAkeSession::handleMoveCancelCommand(SwAkePacket *pkt)
{
	uint8_t label = pkt->exchKeyType();
	uint8_t found = 0;
	if (m_content != NULL)
		m_content->sessionTable().hasMoveSession(&found, m_peerAddr, label);
	int err = sendAckResponse(pkt, found ? 0x0F : 0x07);
	if (err == 0 && found && m_content != NULL && m_content->observer() != NULL)
		m_content->observer()->onMoveCancelReceived(m_peerAddr, label);
	return err;
}

int SwSinkAkeSession::handleRejectOrStatusResponse(SwAkePacket *pkt)
{
	uint8_t subfn = pkt->subfunction();
	uint8_t st = pkt->statusCode();
	int err = 1542;
	switch (subfn) {
	case SW_AKE_SUBFN_CHALLENGE:
		err = (st == 1) ? 1523 : ((st == 7) ? 1524 : 1542);
		break;
	case SW_AKE_SUBFN_RESPONSE:
		err = (st == 2) ? 1525 : ((st == 7) ? 1526 : 1542);
		break;
	case SW_AKE_SUBFN_SRM:
		err = (st == 7) ? 1527 : 1542;
		onSrmError(err, 1);
		return err;
	case SW_AKE_SUBFN_RTT_READY:
		err = (st == 7) ? 1528 : 1542;
		break;
	case SW_AKE_SUBFN_RTT_SETUP:
		err = (st == 7) ? 1531 : 1542;
		break;
	case SW_AKE_SUBFN_RTT_TEST:
		err = (st == 1) ? 1532 : ((st == 7) ? 1533 : 1542);
		break;
	case SW_AKE_SUBFN_MV_INIT:
		err = (st == 1) ? 1534 : ((st == 7) ? 1535 : 1542);
		break;
	case SW_AKE_SUBFN_MV_FINALIZE:
		err = (st == 1) ? 1537 : ((st == 7) ? 1538 : 1542);
		break;
	case SW_AKE_SUBFN_MV_COMPLETE:
		err = (st == 1) ? 1539 : ((st == 7) ? 1540 : 1542);
		break;
	default:
		err = 1;
		break;
	}
	onAkeTimeout(err);
	return err;
}

int SwSinkAkeSession::handleReceivedPacket(SwAkePacket *pkt)
{
	if (pkt == NULL)
		return 1;
	if (pkt->isRejectResponse())
		return handleRejectOrStatusResponse(pkt);
	if (pkt->isStatusCommand())
		return handleStatusCommand(pkt);

	switch (pkt->subfunction()) {
	case SW_AKE_SUBFN_CHALLENGE:
		return pkt->isResponse() ? handleChallengeResponse(pkt)
					 : handleChallengeCommand(pkt);
	case SW_AKE_SUBFN_RESPONSE:
		return pkt->isResponse() ? handleResponseResponse(pkt)
					 : handleResponseCommand(pkt);
	case SW_AKE_SUBFN_EXCHANGE_KEY:
		return handleExchangeKeyCommand(pkt);
	case SW_AKE_SUBFN_SRM:
		return pkt->isResponse() ? handleSrmResponse(pkt) : handleSrmCommand(pkt);
	case SW_AKE_SUBFN_RTT_READY:
	case SW_AKE_SUBFN_RTT_SETUP:
	case SW_AKE_SUBFN_RTT_TEST:
	case SW_AKE_SUBFN_RTT_VERIFY:
	case SW_AKE_SUBFN_RTT_VERIFY_READY:
		return handleRttCommand(pkt);
	case SW_AKE_SUBFN_MV_INIT:
		return handleMoveInitResponse(pkt);
	case SW_AKE_SUBFN_MV_FINALIZE:
		return handleMoveFinalizeResponse(pkt);
	case SW_AKE_SUBFN_MV_COMPLETE:
		return handleMoveCompleteResponse(pkt);
	case SW_AKE_SUBFN_MV_CANCEL:
		return handleMoveCancelCommand(pkt);
	default:
		return 0;
	}
}

/* ---- SwSinkAuthThread ---- */

SwSinkAuthThread::SwSinkAuthThread(uint8_t isMoveAuth, uint8_t isMoveSubOp,
				   uint8_t isMoveFinalize)
	: SwThread(TYPE_SINK_AUTH), m_content(NULL), m_peerAddr(), m_mode(0),
	  m_config(4, 8, 1), m_channel(NULL), m_session(NULL), m_keepRunning(1),
	  m_isMoveAuth(isMoveAuth), m_isMoveSubOp(isMoveSubOp),
	  m_isMoveFinalize(isMoveFinalize), m_hasMoveMsg(0),
	  m_hasMoveCompleteLabel(0), m_moveCompleteLabel(0), m_reservedD(0),
	  m_sessionId(0xFFFFFFFFu), m_lock()
{
	memset(&m_moveMsg, 0, sizeof(m_moveMsg));
}

SwSinkAuthThread::~SwSinkAuthThread()
{
	if (isRunning())
		cancel(0);
	join();
	cleanupChannelAndSession();
}

void SwSinkAuthThread::cleanupChannelAndSession()
{
	SwScopedLock guard(m_lock);
	if (m_session != NULL) {
		m_session->cancelSession();
		delete m_session;
		m_session = NULL;
	}
	if (m_channel != NULL) {
		delete m_channel;
		m_channel = NULL;
	}
}

int SwSinkAuthThread::configure(SwContent *content, const SwNetAddress &addr,
				uint32_t mode, const SwAkeConfig &config)
{
	if (!addr.isValid())
		return 5;
	m_content = content;
	m_peerAddr.set(addr.host(), addr.port());
	m_mode = mode;
	m_config = config;
	if (m_channel != NULL) {
		delete m_channel;
		m_channel = NULL;
	}
	m_channel = new (std::nothrow) SwAkeTcpChannel();
	if (m_channel == NULL)
		return 2;
	if (m_isMoveSubOp) {
		m_sessionId = 0xFFFFFFFFu;
		return 0;
	}
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) != 0 || dev == NULL)
		return 1;
	return dev->openSinkSession(&m_sessionId, m_peerAddr, m_mode,
				    m_isMoveAuth ? 1 : 0);
}

int SwSinkAuthThread::configureWithId(uint32_t *outSessionId, SwContent *content,
				      const SwNetAddress &addr, uint32_t mode,
				      const SwAkeConfig &config)
{
	int err = configure(content, addr, mode, config);
	if (outSessionId != NULL)
		*outSessionId = (err == 0) ? m_sessionId : 0;
	return err;
}

int SwSinkAuthThread::configureMoveFinalize(SwContent *content, const SwNetAddress &addr,
					    uint32_t mode, const SwAkeConfig &config,
					    const struct SwMwMoveMessage *msg)
{
	if (msg != NULL) {
		m_moveMsg = *msg;
		m_hasMoveMsg = 1;
	}
	return configure(content, addr, mode, config);
}

int SwSinkAuthThread::configureMoveComplete(SwContent *content, const SwNetAddress &addr,
					    uint32_t mode, const SwAkeConfig &config,
					    uint8_t label)
{
	m_moveCompleteLabel = label;
	m_hasMoveCompleteLabel = 1;
	return configure(content, addr, mode, config);
}

void SwSinkAuthThread::cancel(int reason)
{
	SwScopedLock guard(m_lock);
	if (!isStopped()) {
		m_keepRunning = 0;
		if (m_result == 0 && reason != 0)
			m_result = reason;
		if (m_session != NULL)
			m_session->stopChannel();
		if (m_channel != NULL)
			m_channel->cancel();
	}
}

void SwSinkAuthThread::run()
{
	int err = 0;
	bool sessionStarted = false;

	if (m_content == NULL || !m_peerAddr.isValid())
		err = 4;
	if (m_session != NULL)
		err = 8;

	if (err == 0) {
		if (m_channel == NULL) {
			err = 2;
		} else {
			err = m_channel->connectTo(m_peerAddr, 5000);
			if (err == 0) {
				if (!m_keepRunning) {
					err = 1026;
				} else if (m_content->observer() != NULL) {
					SwNetAddress peer;
					m_channel->getPeerAddress(peer);
					if (!m_content->observer()->isValidConnection(0))
						err = 1026;
				}
			}
			if (err == 0 && !m_keepRunning)
				err = 8;
			if (err == 0) {
				m_session = new (std::nothrow) SwSinkAkeSession(
					m_isMoveSubOp, m_isMoveFinalize, m_sessionId,
					m_config, m_channel, m_content, m_isMoveAuth);
				if (m_session == NULL) {
					err = 2;
				} else {
					err = m_session->attachThread(this);
					if (err == 0) {
						if (!m_keepRunning) {
							m_session->stopChannel();
							err = 8;
						} else {
							sessionStarted = true;
							if (m_isMoveSubOp) {
								if (m_isMoveFinalize) {
									if (m_hasMoveMsg) {
										err = m_session->runMoveFinalize(
											&m_moveMsg, 4, 8);
									} else {
										sessionStarted = false;
										err = 4;
									}
								} else if (m_hasMoveCompleteLabel) {
									err = m_session->runMoveComplete(
										m_moveCompleteLabel, 4, 8);
								} else {
									sessionStarted = false;
									err = 4;
								}
							} else {
								err = m_session->runAuth(4, 8);
							}
						}
					}
				}
			}
		}

		if (err != 0) {
			if (!m_isMoveSubOp &&
			    (m_session == NULL || !m_session->isAuthenticated())) {
				SdiDevice *dev = NULL;
				if (SdiDevice::getInstance(&dev) == 0 && dev != NULL)
					dev->closeSession(m_sessionId);
			}
			cleanupChannelAndSession();
		}
	}

	if (!sessionStarted && m_content != NULL && m_content->observer() != NULL) {
		if (m_isMoveSubOp) {
			if (m_isMoveFinalize && m_hasMoveMsg)
				m_content->observer()->onMoveFinalizeFailed(
					m_peerAddr, m_moveMsg.label, err);
			else if (!m_isMoveFinalize && m_hasMoveCompleteLabel)
				m_content->observer()->onMoveCompleteFailed(
					m_peerAddr, m_moveCompleteLabel, err);
		} else if (m_isMoveAuth) {
			m_content->observer()->onMoveAuthFailed(m_peerAddr, m_sessionId, err);
		} else {
			m_content->observer()->onLegacySinkAuthFailed(m_peerAddr, err);
			m_content->observer()->onSinkAuthFailed(m_peerAddr, m_sessionId, err);
		}
	}

	cleanupChannelAndSession();
	m_result = err;
}

/* ---- SwSinkCkcThread ---- */

SwSinkCkcThread::SwSinkCkcThread(uint32_t sessionId, const SwNetAddress &addr,
				 uint8_t isMove, uint8_t moveLabel)
	: SwThread(TYPE_SINK_CKC), m_sessionId(sessionId), m_peerAddr(addr),
	  m_isMove(isMove), m_moveLabel(moveLabel), m_running(1), m_reservedE(0),
	  m_channel(NULL), m_waitMutex(), m_waitCond(NULL)
{
}

SwSinkCkcThread::~SwSinkCkcThread()
{
	stopCkc();
	join();
	if (m_channel != NULL) {
		delete m_channel;
		m_channel = NULL;
	}
	if (m_waitCond != NULL) {
		delete m_waitCond;
		m_waitCond = NULL;
	}
}

int SwSinkCkcThread::init()
{
	if (m_waitCond != NULL) {
		delete m_waitCond;
		m_waitCond = NULL;
	}
	return m_waitMutex.createCondition(&m_waitCond);
}

void SwSinkCkcThread::stopCkc()
{
	m_waitMutex.lock();
	m_running = 0;
	if (m_channel != NULL)
		m_channel->cancel();
	if (m_waitCond != NULL)
		m_waitCond->signal();
	m_waitMutex.unlock();
}

int SwSinkCkcThread::performCkcExchange()
{
	SdiDevice *dev = NULL;
	if (SdiDevice::getInstance(&dev) != 0 || dev == NULL)
		return 1;
	if (m_channel != NULL) {
		delete m_channel;
		m_channel = NULL;
	}
	m_channel = new (std::nothrow) SwAkeTcpChannel();
	if (m_channel == NULL)
		return 2;
	int err = m_channel->connectTo(m_peerAddr, 5000);
	if (err == 0) {
		uint8_t nonceA[8];
		uint8_t nonceB[8];
		uint8_t mac10[10];
		memset(nonceA, 0, sizeof(nonceA));
		memset(nonceB, 0, sizeof(nonceB));
		memset(mac10, 0, sizeof(mac10));
		err = dev->genMoveKey(mac10, nonceA, (uint8_t)m_sessionId, nonceB);
		if (err == 0) {
			SwAkePacket *req = SwAkePacket::createContentKeyReq(
				false, m_isMove ? m_moveLabel : 0, 4, 0x0F, nonceA);
			if (req == NULL) {
				err = 2;
			} else {
				err = m_channel->sendPacket(req);
				delete req;
				if (err != 0)
					err = m_isMove ? 1503 : 1502;
			}
		}
		if (err == 0) {
			SwAkePacket *rsp = NULL;
			err = m_channel->recvPacket(&rsp, 10000);
			if (err == 0 && rsp != NULL) {
				if (rsp->payload() != NULL && rsp->payloadSize() >= 26) {
					uint8_t ok = 0;
					err = dev->verifyMoveMac(&ok, (uint8_t)m_sessionId,
								 rsp->payload(),
								 rsp->payload() + 8,
								 rsp->payload() + 16);
					if (err == 0 && ok == 0)
						err = 1501;
				}
				delete rsp;
			}
		}
	}
	delete m_channel;
	m_channel = NULL;
	return err;
}

void SwSinkCkcThread::run()
{
	int err = 0;
	while (m_running) {
		err = performCkcExchange();
		if (err != 0 || !m_running)
			break;
		m_waitMutex.lock();
		if (m_running && m_waitCond != NULL)
			m_waitCond->wait(60000);
		m_waitMutex.unlock();
	}
	m_result = err;
}
