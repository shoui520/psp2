/*
 * DTCPIP-SW core protocol engine:
 *   - SwSinkObserver (ABI-compatible with SwMwManagerSinkSlots vtable)
 *   - SwAkeConfig, SwAkeProcedureInfo, SwAkeStateMask, SwAkeStateTracker
 *   - SwAkeSessionTable (normal sink + move sink authenticated session registries)
 *   - SwAkeSessionBase, SwSinkAkeSession, SwSinkAuthThread, SwSinkCkcThread
 *   - SwContent, SwTransfer, SwContentManager, SwContentSupport
 */

#ifndef SW_ENGINE_H
#define SW_ENGINE_H

#include "../osal/sw_osal_cpp.h"
#include "../sdi/sw_sdi.h"
#include "../common/sw_cert_srm.h"
#include "../ake/sw_ake_packet.h"
#include "../ake/sw_ake_transport.h"

#include "../../../../DTCPIP-MW/Src/Mw/SwMwContent.h"
#include "../../../../DTCPIP-MW/Src/Mw/SwMwManager.h"

/* ---- SwSinkObserver (16-slot callback interface matching SwMwManagerSinkSlots) ---- */
class SwSinkObserver {
public:
	virtual void destroy() = 0;							/* 0x00 */
	virtual void dispose() = 0;							/* 0x04 */
	virtual void onLegacySinkAuthSuccess(const SwNetAddress &addr) = 0;		/* 0x08 */
	virtual void onLegacySinkAuthFailed(const SwNetAddress &addr, int err) = 0;	/* 0x0C */
	virtual void onSinkSrmTxFailed(const SwNetAddress &addr, int err) = 0;		/* 0x10 */
	virtual bool isValidConnection(int sock) = 0;					/* 0x14 */
	virtual void onSrmUpdated(const SwNetAddress &addr, const void *data,
				  size_t len) = 0;					/* 0x18 */
	virtual void onMoveAuthSucceeded(const SwNetAddress &addr,
					 uint32_t sessionId) = 0;			/* 0x1C */
	virtual void onMoveAuthFailed(const SwNetAddress &addr, uint32_t sessionId,
				      int err) = 0;					/* 0x20 */
	virtual void onMoveFinalizeAccepted(const SwNetAddress &addr,
					    uint8_t label) = 0;				/* 0x24 */
	virtual void onMoveFinalizeFailed(const SwNetAddress &addr, uint8_t label,
					  int err) = 0;					/* 0x28 */
	virtual void onMoveCompleteAccepted(const SwNetAddress &addr,
					    uint8_t label) = 0;				/* 0x2C */
	virtual void onMoveCompleteFailed(const SwNetAddress &addr, uint8_t label,
					  int err) = 0;					/* 0x30 */
	virtual void onMoveCancelReceived(const SwNetAddress &addr,
					  uint8_t label) = 0;				/* 0x34 */
	virtual void onSinkAuthSuccess(const SwNetAddress &addr,
				       uint32_t sessionId) = 0;				/* 0x38 */
	virtual void onSinkAuthFailed(const SwNetAddress &addr, uint32_t sessionId,
				      int err) = 0;					/* 0x3C */
};

/* ---- SwAkeConfig (AKE procedure/key-exchange configuration) ---- */
class SwAkeConfig {
public:
	SwAkeConfig(uint8_t akeProc = 4, uint8_t exchKeyType = 8, uint8_t versionFlag = 1);
	virtual ~SwAkeConfig();

	SwAkeConfig &operator=(const SwAkeConfig &other);

	uint8_t akeProcedure() const { return m_akeProcedure; }
	uint8_t exchangeKeyType() const { return m_exchKeyType; }
	uint8_t versionFlag() const { return m_versionFlag; }

private:
	uint8_t m_akeProcedure;
	uint8_t m_exchKeyType;
	uint8_t m_versionFlag;
	uint8_t m_reserved;
};

/* ---- SwAkeProcedureInfo (negotiated AKE procedure + label state) ---- */
class SwAkeProcedureInfo {
public:
	SwAkeProcedureInfo();
	virtual ~SwAkeProcedureInfo();

	int set(uint8_t akeProc, uint8_t exchKeyType, uint8_t akeLabel);
	void setMove(uint8_t akeLabel);

	uint8_t akeProcedure() const { return m_akeProcedure; }
	uint8_t exchangeKeyType() const { return m_exchKeyType; }
	uint8_t akeLabel() const { return m_akeLabel; }
	uint8_t isMove() const { return m_isMove; }

private:
	uint8_t m_akeProcedure;
	uint8_t m_exchKeyType;
	uint8_t m_akeLabel;
	uint8_t m_isMove;
};

/* ---- SwAkeStateMask / SwAkeStateTracker ---- */
enum SwAkePhase {
	SW_AKE_PHASE_IDLE = 0,
	SW_AKE_PHASE_AUTH_START,
	SW_AKE_PHASE_WAIT_CHALLENGE_RSP,
	SW_AKE_PHASE_WAIT_CHALLENGE_CMD,
	SW_AKE_PHASE_WAIT_RESPONSE_CMD,
	SW_AKE_PHASE_WAIT_RESPONSE_RSP,
	SW_AKE_PHASE_WAIT_EXCHANGE_KEY_CMD,
	SW_AKE_PHASE_WAIT_RTT_READY,
	SW_AKE_PHASE_WAIT_RTT_SETUP,
	SW_AKE_PHASE_WAIT_RTT_TEST,
	SW_AKE_PHASE_WAIT_RTT_VERIFY,
	SW_AKE_PHASE_WAIT_SRM_CMD,
	SW_AKE_PHASE_WAIT_SRM_RSP,
	SW_AKE_PHASE_WAIT_MV_INIT_RSP,
	SW_AKE_PHASE_WAIT_MV_FINALIZE_RSP,
	SW_AKE_PHASE_WAIT_MV_COMPLETE_RSP,
	SW_AKE_PHASE_DONE,
	SW_AKE_PHASE_ERROR
};

class SwAkeStateMask {
public:
	explicit SwAkeStateMask(uint64_t mask = 0) : m_mask(mask) {}
	virtual ~SwAkeStateMask() {}

	uint64_t mask() const { return m_mask; }
	void setMask(uint64_t m) { m_mask = m; }
	bool allows(uint64_t pktBit) const { return (m_mask & pktBit) != 0; }
	bool equals(const SwAkeStateMask &other) const { return m_mask == other.m_mask; }

private:
	uint64_t m_mask;
};

class SwAkeStateTracker {
public:
	explicit SwAkeStateTracker(SwAkePhase initial = SW_AKE_PHASE_IDLE);
	virtual ~SwAkeStateTracker();

	void transition(SwAkePhase nextPhase, uint64_t allowedMask = 0);
	SwAkePhase phase() const { return m_phase; }
	const SwAkeStateMask &allowedMask() const { return m_mask; }

	bool isDone() const { return m_phase == SW_AKE_PHASE_DONE; }
	bool isError() const { return m_phase == SW_AKE_PHASE_ERROR; }
	bool isInAuth() const;
	bool isInSrm() const;
	bool isInMove() const;
	bool isMoveFinalize() const { return m_phase == SW_AKE_PHASE_WAIT_MV_FINALIZE_RSP; }
	bool isMoveComplete() const { return m_phase == SW_AKE_PHASE_WAIT_MV_COMPLETE_RSP; }

private:
	SwAkePhase m_phase;
	SwAkeStateMask m_mask;
};

/* ---- SwAkeSessionTable (normal + move authenticated sink sessions) ---- */
class SwAkeSessionTable {
public:
	SwAkeSessionTable();
	virtual ~SwAkeSessionTable();

	int addSinkSession(const SwNetAddress &addr, uint32_t sessionId, uint8_t flags);
	int removeSinkSession(const SwNetAddress &addr);
	int getSinkSessionId(const SwNetAddress &addr, uint32_t *outSessionId);
	int findSinkSessionIds(const SwNetAddress &addr, SwPtrList<uint32_t> &outIds);
	int checkSinkSessionCkc(uint32_t sessionId, SwNetAddress *outAddr, uint8_t *outNeedCkc);

	int addMoveSession(const SwNetAddress &addr, uint32_t sessionId, uint8_t label,
			   uint32_t keyExpireSec, uint8_t flags);
	int removeMoveSession(const SwNetAddress &addr, uint8_t label);
	int getMoveSessionId(const SwNetAddress &addr, uint8_t label, uint32_t *outSessionId);
	int findMoveSessionIds(const SwNetAddress &addr, uint8_t label,
			       SwPtrList<uint32_t> &outIds);
	int hasMoveSession(uint8_t *outFound, const SwNetAddress &addr, uint8_t label);
	int checkMoveSessionCkc(uint32_t sessionId, uint8_t label, SwNetAddress *outAddr,
				uint8_t *outNeedCkc);
	int closeAllSessions();

	SwAkeSessionList &sinkList() { return m_sinkSessions; }
	SwAkeSessionList &moveList() { return m_moveSessions; }

private:
	SwAkeSessionList m_sinkSessions;
	SwAkeSessionList m_moveSessions;
};

class SwContent;
class SwSinkAuthThread;

/* ---- SwAkeSessionBase (base AKE protocol session) ---- */
class SwAkeSessionBase : public SwAkeCmdTimerCallback {
public:
	SwAkeSessionBase(uint32_t sessionId, const SwAkeConfig &config,
			 SwAkeTcpChannel *channel, uint8_t isMoveSession);
	virtual ~SwAkeSessionBase();

	virtual void onAuthComplete();
	virtual void onKeyCommitted();
	virtual void onAuthError(int err, uint8_t sendStatus);
	virtual void onSrmError(int err, uint8_t sendStatus);
	virtual void onMoveError(int err, uint8_t sendStatus);
	virtual int initSession();
	virtual int computeRttMacPair(uint8_t outMac10[10], uint8_t mode);
	virtual int handleReceivedPacket(SwAkePacket *pkt);
	virtual void onAkeTimeout(int errorCode);

	void cancelSession();
	bool isActive() const { return m_active != 0; }
	void stopChannel();
	int checkPeerTtl();
	int sendSrmPacket(const void *srmData, uint32_t srmLen);
	int handleSrmCommand(SwAkePacket *pkt);
	int handleSrmResponse(SwAkePacket *pkt);
	int handleStatusCommand(SwAkePacket *pkt);

protected:
	void cleanupObjects();
	int sendPacket(SwAkePacket *pkt);
	int sendAckResponse(const SwAkePacket *req, uint8_t statusCode = 0x0F);

	uint32_t m_sessionId;
	uint8_t m_isMove;
	uint8_t m_moveLabelValid;
	uint8_t m_moveLabel;
	uint8_t m_active;
	uint32_t m_moveSessionId;
	SwAkeConfig m_config;
	SwAkeTcpChannel *m_channel;
	SwAkeProcedureInfo m_procInfo;
	uint32_t m_keyType;
	uint8_t m_keyPending;
	uint8_t m_srmUpdateNeeded;
	uint8_t m_reservedA[2];
	SwAkeStateTracker m_state;
	SwAkeCmdTimerThread *m_cmdTimer;
	SwSrm m_localSrm;
	SwSrm m_peerSrm;
	SwCert m_localCert;
	SwCert m_peerCert;
	uint8_t m_localNonce[16];
	uint8_t m_peerNonce[16];
	uint8_t m_ephemeralPriv[20];
	uint8_t m_rttMacA[10];
	uint8_t m_rttMacB[10];
	uint16_t m_rttN;
	uint16_t m_peerSrmVersion;
	uint8_t m_peerSrmGen;
	uint8_t m_moveSubOp;
	uint8_t m_reservedB[2];
	SwNetAddress m_peerAddr;
	SwString m_localHost;
	int m_lastError;
};

/* ---- SwSinkAkeSession (sink-side DTCP-IP AKE + Move state machine) ---- */
class SwSinkAkeSession : public SwAkeSessionBase {
public:
	SwSinkAkeSession(uint8_t isMoveSubOp, uint8_t isMoveFinalize,
			 uint32_t sessionId, const SwAkeConfig &config,
			 SwAkeTcpChannel *channel, SwContent *content,
			 uint8_t isMoveAuth);
	virtual ~SwSinkAkeSession();

	int attachThread(SwSinkAuthThread *thread);
	int runAuth(uint8_t akeProc, uint8_t exchKeyType);
	int runMoveFinalize(const struct SwMwMoveMessage *msg, uint8_t akeProc,
			    uint8_t exchKeyType);
	int runMoveComplete(uint8_t label, uint8_t akeProc, uint8_t exchKeyType);
	int sendMoveCancel(uint8_t label);
	int sendMoveFinalizeOnActive(const struct SwMwMoveMessage *msg);
	int sendMoveCompleteOnActive(uint8_t label);

	bool isAuthenticated() const { return m_authenticated != 0; }
	bool getActiveMoveLabel(uint8_t *outLabel) const;

	virtual void onAuthComplete();
	virtual void onKeyCommitted();
	virtual void onAuthError(int err, uint8_t sendStatus);
	virtual void onSrmError(int err, uint8_t sendStatus);
	virtual void onMoveError(int err, uint8_t sendStatus);
	virtual int handleReceivedPacket(SwAkePacket *pkt);

private:
	int runReceiveLoop();
	int sendChallengeCommand();
	int handleChallengeResponse(SwAkePacket *pkt);
	int handleChallengeCommand(SwAkePacket *pkt);
	int handleResponseCommand(SwAkePacket *pkt);
	int handleResponseResponse(SwAkePacket *pkt);
	int handleExchangeKeyCommand(SwAkePacket *pkt);
	int handleRttCommand(SwAkePacket *pkt);
	int handleMoveInitResponse(SwAkePacket *pkt);
	int handleMoveFinalizeResponse(SwAkePacket *pkt);
	int handleMoveCompleteResponse(SwAkePacket *pkt);
	int handleMoveCancelCommand(SwAkePacket *pkt);
	int handleRejectOrStatusResponse(SwAkePacket *pkt);
	int notifySrmUpdated();
	void notifyMoveFinalizeError(int err, uint8_t label);
	void notifyMoveCompleteError(int err, uint8_t label);

	SwContent *m_content;
	SwSinkAuthThread *m_thread;
	uint8_t m_authenticated;
	uint8_t m_isMoveFinalize;
	uint8_t m_hasMoveMsg;
	uint8_t m_reservedC;
	struct SwMwMoveMessage m_moveMsg;
};

/* ---- SwSinkAuthThread (background AKE / Move worker thread) ---- */
class SwSinkAuthThread : public SwThread {
public:
	SwSinkAuthThread(uint8_t isMoveAuth = 0, uint8_t isMoveSubOp = 0,
			 uint8_t isMoveFinalize = 0);
	virtual ~SwSinkAuthThread();

	int configure(SwContent *content, const SwNetAddress &addr, uint32_t mode,
		      const SwAkeConfig &config);
	int configureWithId(uint32_t *outSessionId, SwContent *content,
			    const SwNetAddress &addr, uint32_t mode,
			    const SwAkeConfig &config);
	int configureMoveFinalize(SwContent *content, const SwNetAddress &addr,
				  uint32_t mode, const SwAkeConfig &config,
				  const struct SwMwMoveMessage *msg);
	int configureMoveComplete(SwContent *content, const SwNetAddress &addr,
				  uint32_t mode, const SwAkeConfig &config,
				  uint8_t label);

	virtual void run();

	bool isRunning() { return !isStopped(); }
	void cancel(int reason);
	const SwNetAddress &peerAddress() const { return m_peerAddr; }
	SwSinkAkeSession *session() const { return m_session; }
	bool isCanceled() const { return m_keepRunning == 0; }
	bool isMoveAuth() const { return m_isMoveAuth != 0; }
	uint32_t sessionId() const { return m_sessionId; }

private:
	void cleanupChannelAndSession();

	SwContent *m_content;
	SwNetAddress m_peerAddr;
	uint32_t m_mode;
	SwAkeConfig m_config;
	SwAkeTcpChannel *m_channel;
	SwSinkAkeSession *m_session;
	uint8_t m_keepRunning;
	uint8_t m_isMoveAuth;
	uint8_t m_isMoveSubOp;
	uint8_t m_isMoveFinalize;
	uint8_t m_hasMoveMsg;
	uint8_t m_hasMoveCompleteLabel;
	uint8_t m_moveCompleteLabel;
	uint8_t m_reservedD;
	struct SwMwMoveMessage m_moveMsg;
	uint32_t m_sessionId;
	SwMutex m_lock;
};

/* ---- SwSinkCkcThread (periodic Content Key Confirmation worker thread) ---- */
class SwSinkCkcThread : public SwThread {
public:
	SwSinkCkcThread(uint32_t sessionId, const SwNetAddress &addr, uint8_t isMove,
			uint8_t moveLabel);
	virtual ~SwSinkCkcThread();

	int init();
	void stopCkc();
	virtual void run();

private:
	int performCkcExchange();

	uint32_t m_sessionId;
	SwNetAddress m_peerAddr;
	uint8_t m_isMove;
	uint8_t m_moveLabel;
	volatile uint8_t m_running;
	uint8_t m_reservedE;
	SwAkeTcpChannel *m_channel;
	SwMutex m_waitMutex;
	SwCondition *m_waitCond;
};

/* ---- SwContent (implements SwMwContent 14-slot interface) ---- */
class SwContent : public SwMwContent {
public:
	SwContent();
	virtual ~SwContent();

	virtual int begin(void *uri, int value, int mode);
	virtual int start(int *uriSet, struct SwMwUri *uri, int mode);
	virtual int close(struct SwMwUri *uri);
	virtual int list(void *out);
	virtual int setUri(int *uriSet, struct SwMwUri *uri, int mode);
	virtual int operation(struct SwMwUri *uri, int *result);
	virtual int request(struct SwMwUri *uri, unsigned char label,
			    struct SwMwMoveMessage *message, int value);
	virtual int finalize(struct SwMwUri *uri, struct SwMwMoveMessage *message);
	virtual int complete(struct SwMwUri *uri, unsigned char state);
	virtual int abort(struct SwMwUri *uri, unsigned char state);
	virtual int getState();
	virtual int getStateResult(struct SwMwUri *uri, int mode);

	void setObserver(SwSinkObserver *obs) { m_observer = obs; }
	SwSinkObserver *observer() const { return m_observer; }
	SwAkeSessionTable &sessionTable() { return m_sessionTable; }

	int getSessionIdForUri(const SwNetAddress &addr, uint32_t *outSessionId);
	int registerAuthenticatedSink(const SwNetAddress &addr, uint32_t sessionId,
				      SwSinkAuthThread *thread);
	int registerAuthenticatedMove(const SwNetAddress &addr, uint32_t sessionId,
				      uint8_t label, SwSinkAuthThread *thread);
	int checkSinkCkc(uint32_t sessionId, SwNetAddress *outAddr, uint8_t *outNeedCkc);
	int checkMoveCkc(uint32_t sessionId, uint8_t label, SwNetAddress *outAddr,
			 uint8_t *outNeedCkc);
	bool isSinkSessionInUse(const SwNetAddress &addr);
	bool isMoveSessionInUse(const SwNetAddress &addr, uint8_t label);

private:
	int findActiveMoveSession(SwSinkAkeSession **outSession, const SwNetAddress &addr,
				  uint8_t label);
	void reapStoppedThreads();
	void eraseThread(SwSinkAuthThread *thread);

	SwAkeSessionTable m_sessionTable;
	SwSinkObserver *m_observer;
	SwPtrList<SwSinkAuthThread> m_threads;
	SwMutex m_threadsMutex;
};

/* ---- SwTransfer (implements SwMwTransfer via virtual inheritance) ---- */
class SwTransfer : public SwMwTransfer {
public:
	SwTransfer(void *extMem, uint32_t bufferSize);
	~SwTransfer();

	int setup(uint32_t sessionId, int mediaFmt, struct SwMwStreamEvent *event,
		  void *workOrParams, uint8_t isMove, uint8_t moveLabel);
	uint32_t sessionId() const { return m_sessionId; }

	virtual void destroy();
	virtual void dispose();
	virtual int setEvent(struct SwMwStreamEvent *event);
	virtual int start();
	virtual int reset();
	virtual int isReady();
	virtual int getState();
	virtual int setParameter(int parameter);
	virtual int attach(int value);
	virtual int transfer(int value);
	virtual int write(int offset, int size);
	virtual int getChunkList(void **chunks, int *count);
	virtual int release(int size);
	virtual int setSessionData(const void *data);
	virtual int getSessionData(int *out);
	virtual int isKeyExpiring();
	virtual int startMove(int value);

private:
	int startRxWithCkc(int *outLifetimeSec);
	int prepareCkc(uint8_t *outNeedCkc, uint8_t *outSessionNeedCkc);
	int propagateBufferError();

	SdiBufferCXD3705 *m_buffer;
	void *m_extMem;
	uint32_t m_bufferSize;
	SwSinkCkcThread *m_ckcThread;
	uint8_t m_hasIv;
	uint8_t m_iv[8];
	uint8_t m_isMove;
	uint8_t m_moveLabel;
	uint8_t m_rxStarted;
	uint8_t m_aborted;
	uint8_t m_reservedF[3];
	uint32_t m_sessionId;
	uint32_t m_streamId;
	int m_mediaFmt;
	int m_lastError;
	int m_timeoutMs;
	struct SwMwStreamEvent *m_event;
	SdiStreamParams *m_streamParams;
	SwNetAddress m_peerAddr;
};

class SwContentSupport;

/* ---- SwContentManager (implements SwMwContentManager via virtual inheritance) ---- */
class SwContentManager : public virtual SwMwContentManager {
public:
	explicit SwContentManager(SwContentSupport *support);
	~SwContentManager();

	virtual void destroy();
	virtual void dispose();
	virtual int createTransfer(struct SwMwTransfer **transfer, struct SwMwUri *uri,
				   int mode, struct SwMwStreamEvent *event,
				   int receiveSize, struct SwMwSinkWork *work,
				   int sendBase);
	virtual int releaseTransfer(struct SwMwTransfer *transfer);
	virtual int createTransferOn(struct SwMwTransfer **transfer, struct SwMwUri *uri,
				     int mode, struct SwMwStreamEvent *event,
				     unsigned char label, int receiveSize,
				     struct SwMwSinkWork *work, int sendBase);

	bool isSessionInUse(uint32_t sessionId);

private:
	void releaseAllOrOne(struct SwMwTransfer *target);

	SwPtrList<SwTransfer> m_transfers;
	SwContentSupport *m_support;
	SwMutex m_mutex;
};

/* ---- SwContentSupport (process singleton behind SwMwContentSupportCreate) ---- */
class SwContentSupport : public SwMwContentSupport {
public:
	SwContentSupport();
	~SwContentSupport();

	virtual void destroy();
	virtual void dispose();
	virtual int createContent(void **content, void *observer);
	virtual int releaseContent();
	virtual int createManager(void **manager);
	virtual int releaseManager();
	virtual int setSrm(const void *data, size_t size);

	SwContent *content();
	SwContentManager *manager();
	bool isSessionInUse(uint32_t sessionId);

private:
	int releaseContentLocked();
	int releaseManagerLocked();

	SwMutex m_contentMutex;
	SwMutex m_managerMutex;
	SwContent *m_content;
	SwContentManager *m_manager;
};

#endif /* SW_ENGINE_H */
