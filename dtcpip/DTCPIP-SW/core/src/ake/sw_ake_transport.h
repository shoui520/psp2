/*
 * DTCPIP-SW AKE transport, timer, endpoint, and session management classes.
 */

#ifndef SW_AKE_TRANSPORT_H
#define SW_AKE_TRANSPORT_H

#include "../osal/sw_osal_cpp.h"
#include "../sdi/sw_sdi.h"
#include "sw_ake_packet.h"

/* ---- SwTimer (monotonic millisecond expiration timer) ---- */
class SwTimer {
public:
	SwTimer();
	virtual ~SwTimer();

	void start(uint32_t durationMs);
	void stop();
	bool isExpired() const;
	bool isRunning() const { return m_running != 0; }

private:
	uint8_t m_running;
	uint8_t m_reserved[3];
	uint32_t m_startMs;
	uint32_t m_durationMs;
};

/* ---- SwAkeCmdTimerThread (fires callback on AKE command timeout) ---- */
class SwAkeCmdTimerCallback {
public:
	virtual ~SwAkeCmdTimerCallback() {}
	virtual void onAkeTimeout(int errorCode) = 0;
};

class SwAkeCmdTimerThread : public SwThread {
public:
	explicit SwAkeCmdTimerThread(SwAkeCmdTimerCallback *cb);
	virtual ~SwAkeCmdTimerThread();

	void startTimer(uint32_t timeoutMs);
	void stopTimer();

	virtual void run();

private:
	uint8_t m_active;
	uint8_t m_reserved[3];
	SwAkeCmdTimerCallback *m_cb;
	SwTimer m_timer;
};

/* ---- SwAkeTcpChannel (buffered DTCP-IP AKE TCP frame reader/writer + TTL probe) ---- */
class SwAkeTcpChannel {
public:
	SwAkeTcpChannel();
	virtual ~SwAkeTcpChannel();

	int connectTo(const SwNetAddress &addr, int timeoutMs = 5000);
	int attachAccepted(SwTcpSocket *clientSock, const SwSocket &serverSock);
	int listenOn(const SwNetAddress &addr);
	int acceptChannel(SwAkeTcpChannel **outChannel);
	int waitReadable(int timeoutMs);
	int sendPacket(SwAkePacket *pkt);
	int recvPacket(SwAkePacket **outPkt, int timeoutMs);
	int getPeerTtl();
	int getPeerAddress(SwNetAddress &out);
	void cancel();
	void close();

private:
	static int configureSocket(SwTcpSocket *sock);
	int fillBuffer(int timeoutMs);
	bool hasHeader() const { return m_bufUsed >= 3; }
	bool hasFullFrame(uint16_t payloadLen) const { return m_bufUsed >= (uint32_t)payloadLen + 3u; }

	SwTcpSocket *m_sock;		/* +0x04 */
	uint8_t m_rxBuf[1036];		/* +0x08 */
	uint32_t m_bufUsed;		/* +0x414 */
	uint8_t m_ttlEnabled;		/* +0x418 */
	uint8_t m_reserved[3];		/* +0x419 */
	SwTtlProbe m_ttlProbe;		/* +0x41C */
	int m_lastErr;			/* +0x424 */
	int m_cancelFlag;		/* +0x428 */
};

/* ---- SwAkeSessionRecord & SwAkeSessionList ---- */
struct SwAkeSessionEntry {
	uint32_t sessionId;
	SwNetAddress peerAddr;
	uint8_t akeLabel;
	uint8_t kxmLabel;
	uint8_t isMove;
	uint8_t authenticated;
	uint32_t keyExpireSec;
	SwCert peerCert;
	uint8_t active;
};

class SwAkeSessionList {
public:
	SwAkeSessionList();
	virtual ~SwAkeSessionList();

	int add(SwAkeSessionEntry *entry);
	int removeById(uint32_t sessionId);
	SwAkeSessionEntry *findByAddr(const SwNetAddress &addr);
	SwAkeSessionEntry *findById(uint32_t sessionId);
	SwAkeSessionEntry *findByLabel(uint8_t label);
	void clearAll();

private:
	SwPtrList<SwAkeSessionEntry> m_entries;
	SwMutex m_mutex;
};

#endif /* SW_AKE_TRANSPORT_H */
