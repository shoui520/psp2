/*
 * DTCPIP-SW SdiDevice & SdiDeviceCXD3705 (Wagner hardware/software driver C++ interface).
 */

#include "sw_sdi.h"
#include "../common/sw_cert_srm.h"

#include <string.h>

static SwMutex g_sdiSessionListMutex;

/* ---- SdiDevice singleton ---- */

SdiDevice::SdiDevice() {}
SdiDevice::~SdiDevice() {}

struct SdiDeviceHolder {
	SdiDevice *ptr;
	SdiDeviceHolder() : ptr(new (std::nothrow) SdiDeviceCXD3705()) {}
	~SdiDeviceHolder()
	{
		delete ptr;
		ptr = NULL;
	}
};

int SdiDevice::getInstance(SdiDevice **out)
{
	if (!out)
		return 5;
	static SdiDeviceHolder s_holder;
	*out = s_holder.ptr;
	swSetBufferFactory(&g_sdiBufferFactory);
	return 0;
}

/* ---- SdiDeviceCXD3705 ---- */

SdiDeviceCXD3705::SdiDeviceCXD3705()
	: m_sourceSessions(), m_sinkSessions(), m_drv(NULL)
{
	SwMwSessionInit(&m_drv);
	m_sourceSessions.clear();
	m_sinkSessions.clear();
}

SdiDeviceCXD3705::~SdiDeviceCXD3705()
{
	if (!m_sourceSessions.empty()) {
		for (SwListIterator it = m_sourceSessions.begin(); it != m_sourceSessions.end(); ++it)
			delete SwPtrList<uint32_t>::deref(it);
		m_sourceSessions.clear();
	}
	if (!m_sinkSessions.empty()) {
		for (SwListIterator it = m_sinkSessions.begin(); it != m_sinkSessions.end(); ++it)
			delete SwPtrList<uint32_t>::deref(it);
		m_sinkSessions.clear();
	}
}

int SdiDeviceCXD3705::drvFd() const
{
	return m_drv ? m_drv->fd() : -1;
}

bool SdiDeviceCXD3705::isDrvOpen() const
{
	return m_drv && m_drv->isOpen();
}

int SdiDeviceCXD3705::init()
{
	return 0;
}

int SdiDeviceCXD3705::openSourceSession(uint32_t *outSessionId, uint8_t *outLabel,
					unsigned int akeType, uint8_t flags, uint8_t moveFlag)
{
	SwString obs("OBSOLETED");
	if (akeType > 2)
		return 5;
	if (!isDrvOpen())
		return 13;

	struct {
		uint8_t sessionId;
		uint8_t reserved1;
		uint8_t status;
		uint8_t reserved3;
		uint8_t isFull;
		uint8_t flags;
		uint8_t isMove;
		uint8_t label;
	} req;
	memset(&req, 0, sizeof(req));
	req.isFull = (akeType <= 1) ? 1 : 0;
	req.flags = flags;
	req.isMove = (moveFlag == 1) ? 1 : 0;

	uint32_t *slot = new (std::nothrow) uint32_t;
	if (!slot)
		return 2;

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5BCu, &req, sizeof(req));
	if (ret != 0) {
		delete slot;
		return ret;
	}
	if (req.status == 26 || req.status == 3) {
		delete slot;
		return 1022;
	}
	if (req.status != 0) {
		delete slot;
		return 1;
	}

	int res = 1;
	if (g_sdiSessionListMutex.lock()) {
		*slot = req.sessionId;
		m_sourceSessions.pushBackPtr(slot);
		*outSessionId = req.sessionId;
		*outLabel = req.label;
		res = 0;
	} else {
		delete slot;
		SwDrv::drvIoctl(drvFd(), 0x8004C633u, &req.sessionId, 1);
		if (moveFlag == 1)
			setMoveMode(req.label);
		res = 1;
	}
	g_sdiSessionListMutex.unlock();
	return res;
}

int SdiDeviceCXD3705::openSinkSession(uint32_t *outSessionId, const SwNetAddress &addr,
				      unsigned int akeType, uint8_t moveFlag)
{
	if (akeType > 2)
		return 5;
	if (!isDrvOpen())
		return 13;

	struct {
		uint8_t sessionId;
		uint8_t reserved1;
		uint8_t status;
		uint8_t isFull;
		uint8_t peerAddr[20];
		uint8_t isSink;
		uint8_t isMove;
		uint8_t reserved2[2];
	} req;
	memset(&req, 0, sizeof(req));

	SwString host;
	uint16_t port = 0;
	addr.get(host, port);
	size_t hostLen = host.length();
	if (hostLen == 0 || hostLen > 18)
		return 5;

	memcpy(req.peerAddr, host.c_str(), hostLen);
	memcpy(req.peerAddr + hostLen, &port, 2);
	req.isFull = (akeType <= 1) ? 1 : 0;
	req.isSink = 1;
	req.isMove = (moveFlag == 1) ? 1 : 0;

	uint32_t *slot = new (std::nothrow) uint32_t;
	if (!slot)
		return 2;

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5D0u, &req, sizeof(req));
	if (ret != 0) {
		delete slot;
		return ret;
	}
	if (req.status != 0) {
		delete slot;
		return 1;
	}

	int res = 1;
	if (g_sdiSessionListMutex.lock()) {
		*slot = req.sessionId;
		m_sinkSessions.pushBackPtr(slot);
		*outSessionId = req.sessionId;
		res = 0;
	} else {
		delete slot;
		SwDrv::drvIoctl(drvFd(), 0x8004C633u, &req.sessionId, 1);
		res = 1;
	}
	g_sdiSessionListMutex.unlock();
	return res;
}

int SdiDeviceCXD3705::closeSession(uint32_t sessionId)
{
	int res = 100;
	if (!g_sdiSessionListMutex.lock())
		return 1;

	if (!m_sourceSessions.empty()) {
		for (SwListIterator it = m_sourceSessions.begin(); it != m_sourceSessions.end(); ++it) {
			uint32_t *p = SwPtrList<uint32_t>::deref(it);
			if (p && *p == sessionId) {
				delete p;
				m_sourceSessions.erase(it);
				res = SwDrv::drvIoctl(drvFd(), 0x8004C633u, &sessionId, sizeof(sessionId));
				break;
			}
		}
	}
	if (!m_sinkSessions.empty()) {
		for (SwListIterator it = m_sinkSessions.begin(); it != m_sinkSessions.end(); ++it) {
			uint32_t *p = SwPtrList<uint32_t>::deref(it);
			if (p && *p == sessionId) {
				delete p;
				m_sinkSessions.erase(it);
				res = SwDrv::drvIoctl(drvFd(), 0x8004C633u, &sessionId, sizeof(sessionId));
				break;
			}
		}
	}
	g_sdiSessionListMutex.unlock();
	return res;
}

int SdiDeviceCXD3705::isAkeTypeSupported(uint8_t akeType)
{
	uint8_t req[4] = { akeType, 0, 0, 0 };
	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5ACu, req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req[2] == 0)
		return 0;
	if (req[2] == 1)
		return 10;
	if (req[2] == 2)
		return 100;
	return 1;
}

uint32_t SdiDeviceCXD3705::getHwCapabilities() const
{
	struct {
		uint32_t reserved;
		uint32_t caps;
	} req;
	memset(&req, 0, sizeof(req));
	if (SwDrv::drvIoctl(drvFd(), 0xC008C5B5u, &req, sizeof(req)) != 0)
		return 0;
	return req.caps;
}

int SdiDeviceCXD3705::getRandom(void *outBuf, size_t len)
{
	if (!outBuf || len == 0 || len > 20)
		return 5;
	if (!isDrvOpen())
		return 13;

	struct {
		uint8_t reserved0;
		uint8_t reserved1;
		uint8_t status;
		uint8_t reserved3;
		uint32_t len;
		uint8_t data[20];
	} req;
	memset(&req, 0, sizeof(req));
	req.len = (uint32_t)len;

	int ret = SwDrv::drvIoctl(drvFd(), 0x4008C5AFu, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 8)
		return 10006;
	if (req.status != 0)
		return 1;
	memcpy(outBuf, req.data + (20 - len), len);
	return 0;
}

int SdiDeviceCXD3705::verifySignature(const void *msg, size_t msgLen,
				      const void *sig40, const void *pubKeyObj)
{
	if (!msg || msgLen > 0xC10 || !sig40)
		return 5;
	if (!isDrvOpen())
		return 13;

	struct {
		uint8_t reserved0;
		uint8_t reserved1;
		uint8_t status;
		uint8_t reserved3;
		uint32_t msgLen;
		uint8_t msg[3088];
		uint8_t hasPubKey;
		uint8_t reserved4[3];
		uint8_t sig[40];
		uint8_t pubKey[40];
	} req;
	memset(&req, 0, sizeof(req));
	req.msgLen = (uint32_t)msgLen;
	memcpy(req.msg, msg, msgLen);
	memcpy(req.sig, sig40, 40);
	if (pubKeyObj) {
		req.hasPubKey = 1;
		int err = static_cast<const SwPubKey *>(pubKeyObj)->get(req.pubKey);
		if (err != 0)
			return err;
	}

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5A8u, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 0)
		return 0;
	if (req.status == 1)
		return 10001;
	if (req.status == 2)
		return 5;
	return 1;
}

int SdiDeviceCXD3705::genKeyPair(void *outPub40, void *outPriv20)
{
	if (!outPub40 || !outPriv20)
		return 5;
	if (!isDrvOpen())
		return 13;

	struct {
		uint8_t reserved0;
		uint8_t reserved1;
		uint8_t status;
		uint8_t reserved3;
		uint8_t priv[20];
		uint8_t pub[40];
	} req;
	memset(&req, 0, sizeof(req));

	int ret = SwDrv::drvIoctl(drvFd(), 0x4008C5AAu, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 0) {
		memcpy(outPub40, req.pub, 40);
		memcpy(outPriv20, req.priv, 20);
		return 0;
	}
	if (req.status == 1)
		return 10002;
	if (req.status == 8)
		return 10006;
	return 1;
}

int SdiDeviceCXD3705::signData(void *outSig40, const void *msg, size_t msgLen)
{
	if (!outSig40 || !msg || msgLen > 0xC10)
		return 5;
	if (!isDrvOpen())
		return 13;

	struct {
		uint8_t reserved0;
		uint8_t reserved1;
		uint8_t status;
		uint8_t reserved3;
		uint32_t msgLen;
		uint8_t msg[3088];
		uint8_t sig[40];
	} req;
	memset(&req, 0, sizeof(req));
	req.msgLen = (uint32_t)msgLen;
	memcpy(req.msg, msg, msgLen);

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5ABu, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 0) {
		memcpy(outSig40, req.sig, 40);
		return 0;
	}
	if (req.status == 1 || req.status == 9)
		return 10000;
	if (req.status == 8)
		return 10006;
	return 1;
}

int SdiDeviceCXD3705::computeRttMac(void *outMac20, uint8_t sessionId, uint8_t role,
				    uint16_t nVal, uint8_t mode)
{
	if (!outMac20 || nVal > 0x3FFu)
		return 5;

	struct {
		uint8_t sessionId;
		uint8_t role;
		uint8_t status;
		uint8_t reserved3;
		uint16_t nVal;
		uint8_t isMode1;
		uint8_t reserved7;
		uint8_t mac[20];
	} req;
	memset(&req, 0, sizeof(req));
	req.sessionId = sessionId;
	req.role = role;
	req.nVal = nVal;
	req.isMode1 = (mode == 1) ? 1 : 0;

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5B2u, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 0) {
		memcpy(outMac20, req.mac, 20);
		return 0;
	}
	if (req.status == 1)
		return 10007;
	if (req.status == 2)
		return 5;
	return 1;
}

int SdiDeviceCXD3705::genMoveKey(void *outMac10, void *outNonce8, uint8_t sessionId,
				 const void *inNonce8)
{
	if (!outMac10 || !outNonce8 || !inNonce8)
		return 5;

	struct {
		uint8_t sessionId;
		uint8_t status;
		uint8_t reserved2[2];
		uint8_t inNonce[8];
		uint8_t outNonce[8];
		uint8_t outMac[10];
		uint8_t reserved3[2];
	} req;
	memset(&req, 0, sizeof(req));
	req.sessionId = sessionId;
	memcpy(req.inNonce, inNonce8, 8);

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5D8u, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 0) {
		memcpy(outNonce8, req.outNonce, 8);
		memcpy(outMac10, req.outMac, 10);
		return 0;
	}
	if (req.status == 1)
		return 10007;
	if (req.status == 2)
		return 5;
	return 1;
}

int SdiDeviceCXD3705::verifyMoveMac(uint8_t *outOk, uint8_t sessionId,
				    const void *nonceA8, const void *nonceB8,
				    const void *mac10)
{
	if (!nonceA8 || !nonceB8 || !mac10)
		return 5;

	struct {
		uint8_t sessionId;
		uint8_t status;
		uint8_t reserved2[2];
		uint8_t nonceB[8];
		uint8_t nonceA[8];
		uint8_t mac[10];
		uint8_t reserved3[2];
	} req;
	memset(&req, 0, sizeof(req));
	req.sessionId = sessionId;
	memcpy(req.nonceB, nonceB8, 8);
	memcpy(req.nonceA, nonceA8, 8);
	memcpy(req.mac, mac10, 10);

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5D9u, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 0) {
		*outOk = 1;
		return 0;
	}
	if (req.status == 1) {
		*outOk = 0;
		return 0;
	}
	if (req.status == 2)
		return 5;
	return 1;
}

int SdiDeviceCXD3705::computeMoveCompleteMac(void *outMac10, uint8_t sessionId,
					     const void *nonceA8, const void *nonceB8,
					     const void *inMac10)
{
	if (!outMac10 || !nonceA8 || !nonceB8 || !inMac10)
		return 5;

	struct {
		uint8_t sessionId;
		uint8_t status;
		uint8_t reserved2[2];
		uint8_t nonceB[8];
		uint8_t nonceA[8];
		uint8_t inMac[10];
		uint8_t outMac[10];
	} req;
	memset(&req, 0, sizeof(req));
	req.sessionId = sessionId;
	memcpy(req.nonceB, nonceB8, 8);
	memcpy(req.nonceA, nonceA8, 8);
	memcpy(req.inMac, inMac10, 10);

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5C3u, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 0) {
		memcpy(outMac10, req.outMac, 10);
		return 0;
	}
	if (req.status == 1)
		return 10007;
	if (req.status == 2)
		return 5;
	return 1;
}

int SdiDeviceCXD3705::getExchangeKeyLabel(uint32_t *outLabel, uint8_t sessionId)
{
	uint8_t req[4] = { sessionId, 0, 0, 0 };
	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5D1u, req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req[2] == 0) {
		*outLabel = req[1];
		return 0;
	}
	if (req[2] == 14)
		return 1507;
	if (req[2] == 2)
		return 5;
	return 1;
}

int SdiDeviceCXD3705::setRemoteKey(uint8_t sessionId, uint8_t keyType,
				   const void *pub20, const void *sig40)
{
	if (!pub20 || !sig40)
		return 5;

	struct {
		uint8_t sessionId;
		uint8_t keyType;
		uint8_t status;
		uint8_t reserved3;
		uint8_t pub[20];
		uint8_t sig[40];
	} req;
	memset(&req, 0, sizeof(req));
	req.sessionId = sessionId;
	req.keyType = keyType;
	memcpy(req.pub, pub20, 20);
	memcpy(req.sig, sig40, 40);

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5D4u, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 15)
		return 1013;
	if (req.status == 2)
		return 5;
	if (req.status == 1)
		return 10003;
	return (req.status != 0) ? 1 : 0;
}

int SdiDeviceCXD3705::setExchangeKey(uint8_t sessionId, uint8_t keyType, int unusedArg,
				     const void *kx12, uint8_t label, uint8_t flags)
{
	(void)unusedArg;
	if (!kx12)
		return 5;

	struct {
		uint8_t sessionId;
		uint8_t keyType;
		uint8_t status;
		uint8_t reserved3;
		uint8_t kx[12];
		uint8_t label;
		uint8_t flags;
		uint8_t reserved4[2];
	} req;
	memset(&req, 0, sizeof(req));
	req.sessionId = sessionId;
	req.keyType = keyType;
	memcpy(req.kx, kx12, 12);
	req.label = label;
	req.flags = flags;

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5D5u, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 15)
		return 1013;
	if (req.status == 2)
		return 5;
	return (req.status != 0) ? 1 : 0;
}

int SdiDeviceCXD3705::clearSinkKey(uint8_t sessionId, uint8_t keyType)
{
	uint8_t req[4] = { sessionId, keyType, 0, 0 };
	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5D2u, req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req[2] == 15)
		return 1013;
	if (req[2] == 2)
		return 5;
	return (req[2] != 0) ? 1 : 0;
}

void SdiDeviceCXD3705::deleteSinkKey(uint8_t sessionId, uint8_t keyType)
{
	uint8_t req[4] = { sessionId, keyType, 0, 0 };
	SwDrv::drvIoctl(drvFd(), 0xC008C5D3u, req, sizeof(req));
}

int SdiDeviceCXD3705::verifyRemoteCert(uint32_t *outResult, uint8_t sessionId,
				       const void *certObj, uint8_t flags)
{
	struct {
		uint8_t sessionId;
		uint8_t outVal;
		uint8_t status;
		uint8_t reserved3;
		uint8_t deviceId[5];
		uint8_t flags;
		uint8_t reserved4[2];
	} req;
	memset(&req, 0, sizeof(req));
	req.sessionId = sessionId;
	req.flags = flags;
	int err = static_cast<const SwDeviceId *>(certObj)->get(req.deviceId);
	if (err != 0)
		return err;

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5BDu, &req, sizeof(req));
	if (ret != 0)
		return ret;
	switch (req.status) {
	case 0:    *outResult = req.outVal; return 0;
	case 2:    return 5;
	case 0x0A: return 1613;
	case 0x0B: return 1650;
	case 0x0D: return 1014;
	case 0x13: return 1016;
	default:   return 1;
	}
}

int SdiDeviceCXD3705::computeSharedSecret(uint8_t sessionId, uint8_t keyType,
					  const void *pub20, const void *sig40)
{
	if (!pub20 || !sig40)
		return 5;

	struct {
		uint8_t sessionId;
		uint8_t keyType;
		uint8_t status;
		uint8_t reserved3;
		uint8_t pub[20];
		uint8_t sig[40];
	} req;
	memset(&req, 0, sizeof(req));
	req.sessionId = sessionId;
	req.keyType = keyType;
	memcpy(req.pub, pub20, 20);
	memcpy(req.sig, sig40, 40);

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5C0u, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 0)
		return 0;
	if (req.status == 1)
		return 10003;
	if (req.status == 2)
		return 5;
	return 1;
}

int SdiDeviceCXD3705::getExchangeKey(void *outKx12, uint8_t *outLabel, uint8_t *outFlags,
				     uint8_t sessionId, uint8_t keyType)
{
	if (!outKx12)
		return 5;

	struct {
		uint8_t sessionId;
		uint8_t keyType;
		uint8_t status;
		uint8_t reserved3;
		uint8_t kx[12];
		uint8_t label;
		uint8_t flags;
		uint8_t reserved4[2];
	} req;
	memset(&req, 0, sizeof(req));
	req.sessionId = sessionId;
	req.keyType = keyType;

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5C1u, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 0) {
		memcpy(outKx12, req.kx, 12);
		*outLabel = req.label;
		*outFlags = req.flags;
		return 0;
	}
	return (req.status == 2) ? 5 : 1;
}

int SdiDeviceCXD3705::commitSourceKey(uint8_t sessionId, uint8_t keyType)
{
	uint8_t req[4] = { sessionId, keyType, 0, 0 };
	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5BEu, req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req[2] == 0)
		return 0;
	return (req[2] == 2) ? 5 : 1;
}

void SdiDeviceCXD3705::deleteSourceKey(uint8_t sessionId, uint8_t keyType)
{
	uint8_t req[4] = { sessionId, keyType, 0, 0 };
	SwDrv::drvIoctl(drvFd(), 0xC008C5BFu, req, sizeof(req));
}

int SdiDeviceCXD3705::checkCertRevoked(uint8_t *outRevoked, const void *certObj)
{
	if (!isDrvOpen())
		return 13;

	struct {
		uint8_t reserved0;
		uint8_t reserved1;
		uint8_t status;
		uint8_t reserved3;
		uint8_t deviceId[5];
		uint8_t reserved4[3];
	} req;
	memset(&req, 0, sizeof(req));
	int err = static_cast<const SwDeviceId *>(certObj)->get(req.deviceId);
	if (err != 0)
		return err;

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5B0u, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 13)
		return 0;
	if (req.status == 11) {
		*outRevoked = 1;
		return 0;
	}
	if (req.status == 0) {
		*outRevoked = 0;
		return 0;
	}
	return 1;
}

int SdiDeviceCXD3705::getSrm(void *srmObj)
{
	if (!isDrvOpen())
		return 13;

	struct {
		uint8_t reserved0;
		uint8_t reserved1;
		uint8_t status;
		uint8_t reserved3;
		uint32_t len;
		uint8_t data[2048];
	} req;
	memset(&req, 0, sizeof(req));

	int ret = SwDrv::drvIoctl(drvFd(), 0x4008C5ADu, &req, sizeof(req));
	if (ret != 0)
		return ret;

	SwCert cert;
	ret = getDeviceCert(&cert);
	if (ret == 0) {
		if (req.status != 0)
			ret = 1;
		else
			ret = static_cast<SwSrm *>(srmObj)->setRaw(
				req.len ? req.data : NULL, req.len, &cert);
	}
	return ret;
}

int SdiDeviceCXD3705::setSrm(const void *srmObj)
{
	if (!isDrvOpen())
		return 13;

	void *rawPtr = NULL;
	size_t rawLen = 0;
	int err = static_cast<const SwSrm *>(srmObj)->getRaw(&rawPtr, &rawLen);
	if (err != 0)
		return err;
	if (rawLen > 2048)
		return 1014;

	struct {
		uint8_t reserved0;
		uint8_t reserved1;
		uint8_t status;
		uint8_t reserved3;
		uint32_t len;
		uint8_t data[2048];
	} req;
	memset(&req, 0, sizeof(req));
	if (rawLen != 0)
		memcpy(req.data, rawPtr, rawLen);
	req.len = (uint32_t)rawLen;

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5AEu, &req, sizeof(req));
	if (ret != 0)
		return ret;
	switch (req.status) {
	case 0:    return 0;
	case 2:    return 5;
	case 0x0C: return 1046;
	case 0x0D: return 10008;
	case 0x11: return 1015;
	default:   return 1;
	}
}

/* ---- Stream start / stop / ioctl helpers ---- */

struct SdiStreamStartReq {
	uint8_t sessionId;		/* +0x00 */
	uint8_t streamId;		/* +0x01 */
	uint8_t reserved2;		/* +0x02 */
	uint8_t reserved3;		/* +0x03 */
	uint8_t hasIv;			/* +0x04 */
	uint8_t iv[11];			/* +0x05 */
	uint32_t inAddr;		/* +0x10 */
	uint32_t inSize;		/* +0x14 */
	uint32_t inUnit;		/* +0x18 */
	uint32_t reserved1c[3];		/* +0x1C */
	uint32_t outAddr;		/* +0x28 */
	uint32_t outSize;		/* +0x2C */
	uint32_t outUnit;		/* +0x30 */
	uint32_t chunkMax;		/* +0x34 */
	uint32_t chunkAlign;		/* +0x38 */
	uint32_t reserved3c;		/* +0x3C */
	uint32_t streamType;		/* +0x40 */
	uint32_t packetSize;		/* +0x44 */
	uint8_t lifetimeMin;		/* +0x48 */
	uint8_t hasTimestamp;		/* +0x49 */
	uint8_t isPcpMode;		/* +0x4A */
	uint8_t reserved4b;		/* +0x4B */
	uint32_t reserved4c;		/* +0x4C */
	uint32_t syncPattern;		/* +0x50 */
	uint32_t syncMask;		/* +0x54 */
	uint8_t reserved58;		/* +0x58 */
	uint8_t mediaFormat;		/* +0x59 */
	uint8_t extFlag;		/* +0x5A */
	uint8_t sacChannelId;		/* +0x5B */
	uint8_t cciInfo[48];		/* +0x5C */
	uint8_t extKeyA[16];		/* +0x8C */
	uint8_t extKeyB[16];		/* +0x9C */
	uint32_t extKeyMode;		/* +0xAC */
	uint8_t optionData[1024];	/* +0xB0 */
};

#define swIsTxDirectOnly(t) ( \
	(t) == 0x10040002u || (t) == 0x10080002u || (t) == 0x10100002u || (t) == 0x10200002u || \
	(t) == 0x10400002u || (t) == 0x10800002u || (t) == 0x11000002u || (t) == 0x12000002u)

#define swIsTxOutOnly(t) ( \
	(t) == 0x50040001u || (t) == 0x50080001u || (t) == 0x50100001u || (t) == 0x50200001u || \
	(t) == 0x50400001u || (t) == 0x50800001u || (t) == 0x51000001u || (t) == 0x52000001u)

#define swIsTxDualBuf(t) ( \
	(t) == 0x10010002u || (t) == 0x20020228u || (t) == 0x20020231u)

#define swIsRxInOnly(t) ( \
	(t) == 0x90010004u || (t) == 0x90010008u || (t) == 0x90010010u || (t) == 0x90010020u || \
	(t) == 0x90010040u || (t) == 0x90010080u || (t) == 0x90010100u || (t) == 0x90010200u)

#define swIsRxOutOnly(t) ( \
	(t) == 0x50010004u || (t) == 0x50010008u || (t) == 0x50010010u || (t) == 0x50010020u || \
	(t) == 0x50010040u || (t) == 0x50010080u || (t) == 0x50010100u || (t) == 0x50010200u)

#define swIsRxDualBuf(t) ( \
	(t) == 0x90010001u || (t) == 0xA0020228u || (t) == 0xA0020231u)

int SdiDeviceCXD3705::startTxStream(uint32_t *outStreamId, uint8_t sessionId,
				    SdiBufferCXD3705 *buf, void *streamCb,
				    unsigned int mediaFmt, const void *iv8,
				    int chunkSize, SdiStreamParams *params)
{
	if ((unsigned int)chunkSize < 4u || mediaFmt >= 2u || !params)
		return 5;
	if (params->isPcpMode > 1u || params->hasTimestamp > 1u)
		return 5;

	uint32_t st = params->streamType;
	if (swIsTxDirectOnly(st) || swIsTxOutOnly(st) ||
	    st == 0x20020228u || st == 0x20020231u) {
		int err = SwDrv::drvIoctl(drvFd(), 0x8008C688u, params->modeParam, 20);
		if (err != 0)
			return err;
	} else if (st != 0x10010002u) {
		return 5;
	}

	params->mainBuffer = buf;
	SdiStreamStartReq req;
	memset(&req, 0, sizeof(req));
	req.sessionId = sessionId;
	req.streamType = st;
	req.isPcpMode = params->isPcpMode;
	req.hasTimestamp = params->hasTimestamp;

	if (mediaFmt != 0) {
		req.packetSize = 2048;
		req.syncPattern = 0x1BA;
		req.syncMask = 0xFFFFFFFFu;
		req.mediaFormat = 1;
	} else {
		req.packetSize = 188;
		req.syncPattern = 0x47000000u;
		req.syncMask = 0xFF000000u;
		req.mediaFormat = 0;
	}

	req.cciInfo[0] = params->cciMode;
	req.cciInfo[3] = params->cciInfo[3];

	SdiBufferCXD3705 *auxBuf = NULL;
	uint32_t memSize = 0;

	if (swIsTxDirectOnly(st)) {
		req.inAddr = (uint32_t)(uintptr_t)buf->bindMemory(&memSize);
		req.inSize = memSize;
		if (chunkSize < (int)(req.packetSize + 4u * req.hasTimestamp))
			return 5;
		req.inUnit = (uint32_t)(chunkSize - (chunkSize % 4));
	} else if (params->extKeyMode != 0) {
		req.extFlag = params->extFlag;
		memcpy(req.extKeyA, params->extKeyA, 16);
		memcpy(req.extKeyB, params->extKeyB, 16);
		req.extKeyMode = params->extKeyMode;
	} else {
		req.cciInfo[1] = 68;
	}

	if (swIsTxOutOnly(st)) {
		req.inAddr = 0;
		req.inSize = 0;
		params->auxBuffer = NULL;
		req.outAddr = (uint32_t)(uintptr_t)buf->bindMemory(&memSize);
		req.outSize = memSize;
		if (chunkSize < (int)(req.packetSize + 4u * req.hasTimestamp))
			return 5;
		req.outUnit = (uint32_t)(chunkSize - (chunkSize % 4));
	}

	if (swIsTxDualBuf(st)) {
		req.inAddr = (uint32_t)(uintptr_t)buf->bindMemory(&memSize);
		req.inSize = memSize;
		if (chunkSize < (int)(req.packetSize + 4u * req.hasTimestamp))
			return 5;
		req.inUnit = (uint32_t)(chunkSize - (chunkSize % 4));

		void *memPtr = params->customMem;
		uint32_t memCap = params->customMemSize;
		if (!memPtr || memCap == 0) {
			memPtr = NULL;
			memCap = (uint32_t)buf->size();
		}

		SwString emptyName("");
		if (params->chunkMaxSize != 0) {
			uint32_t cm = params->chunkMaxSize;
			uint32_t ca = params->chunkAlign;
			uint32_t co = params->chunkOffset;
			if (cm < 4 || (cm % 4) != 0)
				return 5;
			if (ca < 4 || ca > cm || (ca % 4) != 0)
				return 5;
			if (co + 4u > ca)
				return 5;
			req.chunkMax = cm;
			req.chunkAlign = ca;
			req.outUnit = cm - (cm % 4);
			auxBuf = static_cast<SdiBufferCXD3705 *>(
				g_sdiBufferFactory.createWithChunks(&cm, &ca, &co, memPtr, memCap, emptyName));
		} else {
			req.outUnit = g_sdiTransferUnit;
			auxBuf = static_cast<SdiBufferCXD3705 *>(
				g_sdiBufferFactory.create(memPtr, memCap, emptyName));
		}
		if (!auxBuf)
			return 2;
		params->auxBuffer = auxBuf;
		req.outAddr = (uint32_t)(uintptr_t)auxBuf->bindMemory(&memSize);
		req.outSize = memSize;

		if (params->sacSession) {
			uint8_t chId = 0;
			SacSessionCXD3705 *sac = static_cast<SacSessionCXD3705 *>(params->sacSession);
			if (sac->getChannelId(&chId, 1) != 0 || chId == 0) {
				delete auxBuf;
				params->auxBuffer = NULL;
				return 4;
			}
			req.sacChannelId = chId;
			memcpy(req.cciInfo, params->cciInfo, 48);
		}
	}

	if (iv8) {
		req.hasIv = 1;
		memcpy(req.iv, iv8, 8);
	}
	memcpy(req.optionData, params->optionData, 1024);

	int fd = drvFd();
	int err = SwDrv::drvIoctl(fd, 0xC4B0C614u, &req, sizeof(req));
	if (err != 0) {
		if (auxBuf) {
			delete auxBuf;
			params->auxBuffer = NULL;
		}
		return (err == 19) ? 1016 : err;
	}

	*outStreamId = req.streamId;
	bool ok = true;
	if (swIsTxDirectOnly(st)) {
		ok = buf->attachStream(fd, sessionId, req.streamId, 1, 0, streamCb, NULL, params) != 0;
	} else if (swIsTxOutOnly(st)) {
		ok = buf->attachStream(fd, sessionId, req.streamId, 1, 1, streamCb, NULL, params) != 0;
	} else if (swIsTxDualBuf(st)) {
		ok = (buf->attachStream(fd, sessionId, req.streamId, 1, 0, streamCb, NULL, params) != 0) &&
		     (auxBuf->attachStream(fd, sessionId, req.streamId, 0, 1, streamCb, NULL, params) != 0);
	}
	if (!ok) {
		stopTxStream(sessionId, req.streamId, params);
		return 4;
	}
	return 0;
}

int SdiDeviceCXD3705::startRxStream(uint32_t *outStreamId, uint8_t sessionId,
				    SdiBufferCXD3705 *buf, void *streamCb,
				    uint32_t *outLifetimeSec, unsigned int mediaFmt,
				    const void *iv8, void *eventCb, int unusedArg,
				    SdiStreamParams *params)
{
	(void)unusedArg;
	if (mediaFmt >= 2u || !params)
		return 5;
	if (params->isPcpMode > 1u || params->hasTimestamp > 1u)
		return 5;

	uint32_t st = params->streamType;
	if (swIsRxInOnly(st) || swIsRxOutOnly(st) ||
	    st == 0xA0020228u || st == 0xA0020231u) {
		int err = SwDrv::drvIoctl(drvFd(), 0x8008C688u, params->modeParam, 20);
		if (err != 0)
			return err;
	} else if (st != 0x90010001u) {
		return 5;
	}

	params->mainBuffer = buf;
	SdiStreamStartReq req;
	memset(&req, 0, sizeof(req));
	req.sessionId = sessionId;

	if (params->hasTimestamp != 1)
		return 5;
	if (params->transferUnit == 0)
		params->transferUnit = 0x8000u;
	if (params->transferUnit < 0xC00u)
		return 5;
	params->transferUnit -= params->transferUnit % 0xC0u;
	g_sdiTransferUnit = params->transferUnit;

	req.streamType = st;
	req.isPcpMode = params->isPcpMode;
	req.hasTimestamp = params->hasTimestamp;

	if (mediaFmt != 0) {
		req.packetSize = 2048;
		req.syncPattern = 0x1BA;
		req.syncMask = 0xFFFFFFFFu;
		req.mediaFormat = 1;
		if (params->cciInfo[2] > 3u)
			return 5;
		if (params->cciInfo[2] == 0)
			params->cciInfo[2] = 3;
	} else {
		req.packetSize = 188;
		req.syncPattern = 0x47000000u;
		req.syncMask = 0xFF000000u;
		req.mediaFormat = 0;
	}
	req.cciInfo[2] = params->cciInfo[2];
	req.cciInfo[3] = params->cciInfo[3];
	if (req.packetSize < 4u || (req.packetSize & 3u) != 0)
		return 5;

	SdiBufferCXD3705 *auxBuf = NULL;
	uint32_t memSize = 0;

	if (swIsRxInOnly(st)) {
		req.inAddr = (uint32_t)(uintptr_t)buf->bindMemory(&memSize);
		req.inSize = memSize;
		req.inUnit = (memSize < g_sdiTransferUnit) ? memSize : g_sdiTransferUnit;
	} else if (params->extKeyMode != 0) {
		req.extKeyMode = params->extKeyMode;
		req.extFlag = params->extFlag;
		memcpy(req.extKeyA, params->extKeyA, 16);
		memcpy(req.extKeyB, params->extKeyB, 16);
	} else {
		req.cciInfo[1] = 87;
	}

	if (swIsRxOutOnly(st)) {
		req.inAddr = 0;
		req.inSize = 0;
		params->auxBuffer = NULL;
		req.outAddr = (uint32_t)(uintptr_t)buf->bindMemory(&memSize);
		req.outSize = memSize;
		req.outUnit = g_sdiTransferUnit;
	}

	if (swIsRxDualBuf(st)) {
		req.inAddr = (uint32_t)(uintptr_t)buf->bindMemory(&memSize);
		req.inSize = memSize;
		req.inUnit = (memSize < g_sdiTransferUnit) ? memSize : g_sdiTransferUnit;
		req.outUnit = g_sdiTransferUnit;

		void *memPtr = params->customMem;
		uint32_t memCap = params->customMemSize;
		if (!memPtr || memCap == 0) {
			memPtr = NULL;
			memCap = (uint32_t)buf->size();
		}
		SwString emptyName("");
		auxBuf = static_cast<SdiBufferCXD3705 *>(
			g_sdiBufferFactory.create(memPtr, memCap, emptyName));
		if (!auxBuf)
			return 2;
		params->auxBuffer = auxBuf;
		req.outAddr = (uint32_t)(uintptr_t)auxBuf->bindMemory(&memSize);
		req.outSize = memSize;

		if (params->sacSession) {
			uint8_t chId = 0;
			SacSessionCXD3705 *sac = static_cast<SacSessionCXD3705 *>(params->sacSession);
			if (sac->getChannelId(&chId, 1) != 0 || chId == 0) {
				delete auxBuf;
				params->auxBuffer = NULL;
				return 4;
			}
			req.sacChannelId = chId;
			req.cciInfo[32] = params->cciInfo[32];
		}
	}

	if (iv8) {
		req.hasIv = 1;
		memcpy(req.iv, iv8, 8);
	}
	memcpy(req.optionData, params->optionData, 1024);

	int fd = drvFd();
	int err = SwDrv::drvIoctl(fd, 0xC4B0C614u, &req, sizeof(req));
	if (err != 0) {
		if (auxBuf) {
			delete auxBuf;
			params->auxBuffer = NULL;
		}
		return (err == 19) ? 1016 : err;
	}

	*outStreamId = req.streamId;
	*outLifetimeSec = 60u * req.lifetimeMin;

	bool ok = true;
	if (swIsRxInOnly(st)) {
		ok = buf->attachStream(fd, sessionId, req.streamId, 0, 0, streamCb, eventCb, params) != 0;
	} else if (swIsRxOutOnly(st)) {
		ok = buf->attachStream(fd, sessionId, req.streamId, 0, 1, streamCb, eventCb, params) != 0;
	} else if (swIsRxDualBuf(st)) {
		ok = (buf->attachStream(fd, sessionId, req.streamId, 0, 0, streamCb, eventCb, params) != 0) &&
		     (auxBuf->attachStream(fd, sessionId, req.streamId, 1, 1, streamCb, eventCb, params) != 0);
	}
	if (!ok) {
		stopRxStream(sessionId, req.streamId, params);
		return 4;
	}
	return 0;
}

static void swClearRetryFlags(SdiStreamParams *params)
{
	if (params->mainBuffer)
		static_cast<SdiBufferCXD3705 *>(params->mainBuffer)->clearRetryLast();
	if (params->auxBuffer)
		static_cast<SdiBufferCXD3705 *>(params->auxBuffer)->clearRetryLast();
}

static bool swAnyLastPacketSent(SdiStreamParams *params)
{
	bool a = params->mainBuffer && static_cast<SdiBufferCXD3705 *>(params->mainBuffer)->isLastPacketSent();
	bool b = params->auxBuffer && static_cast<SdiBufferCXD3705 *>(params->auxBuffer)->isLastPacketSent();
	return a || b;
}

static bool swDrainOneChunk(SdiBufferCXD3705 *buf)
{
	SdiBufferChunk *list = NULL;
	int count = buf->getReadChunkList(&list);
	if (count == -1)
		return false;
	unsigned int sum = 0;
	for (int i = 0; i < count; ++i)
		sum += list[i].size;
	buf->consume(sum);
	return true;
}

static void swDrainUntilStopped(SdiBufferCXD3705 *pollBuf, SdiBufferCXD3705 *drainBuf)
{
	uint32_t info[2] = { 0, 0 };
	int st = pollBuf->pollStream(info);
	while (st != 1 && st != 3) {
		swDrainOneChunk(drainBuf);
		st = pollBuf->pollStream(info);
		sceKernelDelayThread(200);
	}
	pollBuf->stopStream();
	while (swDrainOneChunk(drainBuf))
		;
}

int SdiDeviceCXD3705::stopTxStream(int sessionId, int streamId, SdiStreamParams *params)
{
	(void)sessionId;
	(void)streamId;
	if (!params)
		return 5;

	SdiBufferCXD3705 *aux = static_cast<SdiBufferCXD3705 *>(params->auxBuffer);
	SdiBufferCXD3705 *mainBuf = static_cast<SdiBufferCXD3705 *>(params->mainBuffer);
	swClearRetryFlags(params);

	if (aux) {
		if (swAnyLastPacketSent(params)) {
			swDrainUntilStopped(aux, mainBuf);
		} else {
			aux->stopStream();
			uint32_t info[2] = { 0, 0 };
			for (int st = aux->pollStream(info); st != 1 && st != 3; st = aux->pollStream(info))
				swDrainOneChunk(mainBuf);
			while (swDrainOneChunk(mainBuf))
				;
		}
	} else if (swAnyLastPacketSent(params)) {
		while (swDrainOneChunk(mainBuf))
			;
		mainBuf->stopStream();
	} else {
		mainBuf->stopStream();
		while (swDrainOneChunk(mainBuf))
			;
	}

	if (aux) {
		delete aux;
		params->auxBuffer = NULL;
	}
	return 0;
}

int SdiDeviceCXD3705::stopRxStream(int sessionId, int streamId, SdiStreamParams *params)
{
	(void)sessionId;
	(void)streamId;
	if (!params)
		return 5;

	SdiBufferCXD3705 *mainBuf = static_cast<SdiBufferCXD3705 *>(params->mainBuffer);
	SdiBufferCXD3705 *aux = static_cast<SdiBufferCXD3705 *>(params->auxBuffer);
	swClearRetryFlags(params);

	if (aux) {
		if (swAnyLastPacketSent(params)) {
			swDrainUntilStopped(mainBuf, aux);
		} else {
			mainBuf->stopStream();
			uint32_t info[2] = { 0, 0 };
			for (int st = mainBuf->pollStream(info); st != 1 && st != 3; st = mainBuf->pollStream(info))
				swDrainOneChunk(aux);
			while (swDrainOneChunk(aux))
				;
		}
	} else if (swAnyLastPacketSent(params)) {
		uint32_t info[2] = { 0, 0 };
		for (int st = 0; st != 1 && st != 3; st = mainBuf->pollStream(info))
			;
		mainBuf->stopStream();
	} else {
		mainBuf->stopStream();
		uint32_t info[2] = { 0, 0 };
		for (int st = 0; st != 1 && st != 3; st = mainBuf->pollStream(info))
			;
	}

	if (aux) {
		delete aux;
		params->auxBuffer = NULL;
	}
	return 0;
}

int SdiDeviceCXD3705::updateStreamPos(int drvFd, int sessionId, uint8_t streamId,
				      uint32_t *posPtr, uint8_t dirFlag)
{
	(void)sessionId;
	if (drvFd < 0)
		return 5;
	struct {
		uint32_t streamId;
		uint8_t dirFlag;
		uint8_t reserved[3];
		uint32_t pos;
	} req;
	memset(&req, 0, sizeof(req));
	req.streamId = streamId;
	req.dirFlag = dirFlag;
	req.pos = *posPtr;
	return SwDrv::drvIoctl(drvFd, 0x800CC618u, &req, sizeof(req)) != 0 ? 1 : 0;
}

int SdiDeviceCXD3705::getStreamStatus(int drvFd, int sessionId, uint8_t streamId,
				      uint32_t addrLen[2], uint8_t outCci48[48],
				      int *outState, int *outFlags, uint8_t dirFlag,
				      void **outIvPtr, int timeoutMs)
{
	(void)sessionId;
	if (drvFd < 0)
		return 5;

	struct {
		uint32_t streamId;
		int timeoutMs;
		uint32_t addr;
		uint32_t len;
		uint8_t dirFlag;
		uint8_t cciInfo[48];
		uint8_t iv[8];
		uint8_t state;
		uint8_t reserved[2];
		int flags;
	} req;
	memset(&req, 0, sizeof(req));
	req.streamId = streamId;
	req.timeoutMs = timeoutMs;
	req.addr = addrLen[0];
	req.len = addrLen[1];
	req.dirFlag = dirFlag;

	int err = SwDrv::drvIoctl(drvFd, 0xC050C617u, &req, sizeof(req));
	if (err == 0) {
		addrLen[0] = req.addr;
		addrLen[1] = req.len;
		*outFlags = req.flags;
		memcpy(outCci48, req.cciInfo, 48);
		*outState = req.state;
		if (req.state == 1 && outIvPtr) {
			if (!*outIvPtr)
				*outIvPtr = new (std::nothrow) uint8_t[8];
			if (*outIvPtr)
				memcpy(*outIvPtr, req.iv, 8);
		}
	}
	return err;
}

int SdiDeviceCXD3705::stopStreamIoctl(int drvFd, int sessionId, uint8_t streamId,
				      SdiStreamParams *params)
{
	(void)sessionId;
	if (!params || drvFd < 0)
		return 5;
	uint8_t sid = streamId;
	int err = SwDrv::drvIoctl(drvFd, 0xC001C616u, &sid, 1);
	if (err == 0)
		return 0;
	return (err == 11016) ? 9 : 1;
}

int SdiDeviceCXD3705::setLastPacketIoctl(int drvFd, int sessionId, uint8_t streamId)
{
	(void)sessionId;
	if (drvFd < 0)
		return 5;
	uint32_t sid = streamId;
	int err = SwDrv::drvIoctl(drvFd, 0x8004C619u, &sid, sizeof(sid));
	if (err == 0 || err == 11015)
		return 0;
	return (err == 11008) ? 9 : 1;
}

int SdiDeviceCXD3705::setCci(uint8_t sessionId, int cci)
{
	switch ((int8_t)cci) {
	case 0: case 2: case 4: case 6: case 8: case 10: case 12:
		break;
	default:
		return 1048;
	}
	if (!isDrvOpen())
		return 13;
	uint8_t req[4] = { sessionId, (uint8_t)cci, 0, 0 };
	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5C4u, req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req[2] == 0)
		return 0;
	if (req[2] == 1)
		return 8;
	if (req[2] == 2)
		return 5;
	return 1;
}

int SdiDeviceCXD3705::getDeviceCert(void *certObj)
{
	if (!isDrvOpen())
		return 13;

	struct {
		uint8_t reserved0;
		uint8_t reserved1;
		uint8_t status;
		uint8_t reserved3;
		uint32_t len;
		uint8_t data[132];
	} req;
	memset(&req, 0, sizeof(req));

	int ret = SwDrv::drvIoctl(drvFd(), 0x4008C5A9u, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 27)
		return 1005;
	if (req.status != 0)
		return 1;
	return static_cast<SwCert *>(certObj)->setRaw(req.data, req.len);
}

int SdiDeviceCXD3705::getCkcInfo(void *outNonce8, bool *outFlag, uint8_t *outA,
				 uint8_t *outB, uint8_t sessionId)
{
	if (!outNonce8)
		return 5;

	struct {
		uint8_t sessionId;
		uint8_t reserved1;
		uint8_t status;
		uint8_t reserved3;
		uint8_t nonce[8];
		uint8_t valA;
		uint8_t valB;
		uint8_t flag;
		uint8_t reserved4;
	} req;
	memset(&req, 0, sizeof(req));
	req.sessionId = sessionId;

	if (SwDrv::drvIoctl(drvFd(), 0xC008C5C2u, &req, sizeof(req)) != 0)
		return 1;
	if (req.status == 0) {
		memcpy(outNonce8, req.nonce, 8);
		*outFlag = (req.flag != 0);
		*outA = req.valA;
		*outB = req.valB;
		return 0;
	}
	return (req.status == 2) ? 5 : 1;
}

int SdiDeviceCXD3705::checkCkc(uint8_t *outOk, uint8_t sessionId, int mode)
{
	uint8_t req[8] = { sessionId, 0, 0, 0, (uint8_t)mode, 0, 0, 0 };
	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5D6u, req, sizeof(req));
	if (ret != 0)
		return ret;
	*outOk = 0;
	if (req[2] == 0) {
		*outOk = 1;
		return 0;
	}
	return (req[2] == 2) ? 5 : 1;
}

int SdiDeviceCXD3705::checkSessionRevoked(uint8_t *outOk, uint8_t sessionId)
{
	uint8_t req[4] = { sessionId, 0, 0, 0 };
	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5D7u, req, sizeof(req));
	if (ret != 0)
		return ret;
	*outOk = 0;
	if (req[2] == 0) {
		*outOk = 1;
		return 0;
	}
	if (req[2] == 1)
		return 1506;
	if (req[2] == 2)
		return 5;
	return 1;
}

int SdiDeviceCXD3705::getKeyLifetime(uint8_t sessionId, uint32_t *outSec)
{
	uint8_t req[4] = { sessionId, 0, 0, 0 };
	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5DAu, req, sizeof(req));
	if (ret == 0) {
		if (req[1] == 0)
			*outSec = 60u * req[2];
		else if (req[1] == 2)
			ret = 12;
		else
			ret = 1;
	}
	return ret;
}

int SdiDeviceCXD3705::isMediaFormatSupported(unsigned int mediaFmt)
{
	return (mediaFmt < 2u) ? 0 : 7;
}

int SdiDeviceCXD3705::getStreamIv(void *outIv8, int unusedArg, SdiBufferCXD3705 *buf)
{
	(void)unusedArg;
	if (!outIv8 || !buf)
		return 5;
	return buf->getLastIv(outIv8);
}

int SdiDeviceCXD3705::setOptionData(size_t len, const void *data)
{
	if (len == 0 || !data || len > 256)
		return 5;

	struct {
		uint8_t reserved0;
		uint8_t reserved1;
		uint8_t status;
		uint8_t reserved3;
		uint32_t len;
		uint8_t data[256];
	} req;
	memset(&req, 0, sizeof(req));
	req.len = (uint32_t)len;
	memcpy(req.data, data, len);

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5C5u, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 0)
		return 0;
	return (req.status == 2) ? 5 : 1;
}

int SdiDeviceCXD3705::setMoveMode(int mode)
{
	uint8_t req[8] = { 0, 0, 0, 0, (uint8_t)mode, 0, 0, 0 };
	if (SwDrv::drvIoctl(drvFd(), 0xC008C5C6u, req, sizeof(req)) != 0)
		return 11021;
	if (req[2] == 0)
		return 0;
	return (req[2] == 25) ? 1020 : 1;
}

int SdiDeviceCXD3705::computeMoveAuth(void *outMac20, uint8_t sessionId, const void *nonce8)
{
	if (!outMac20 || !nonce8)
		return 5;

	struct {
		uint8_t sessionId;
		uint8_t reserved1;
		uint8_t status;
		uint8_t reserved3;
		uint8_t nonce[8];
		uint8_t mac[20];
	} req;
	memset(&req, 0, sizeof(req));
	req.sessionId = sessionId;
	memcpy(req.nonce, nonce8, 8);

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5B4u, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status == 0) {
		memcpy(outMac20, req.mac, 20);
		return 0;
	}
	return (req.status == 2) ? 5 : 1;
}

int SdiDeviceCXD3705::getDeviceId(uint8_t *outValid, uint8_t *outFlag, void *outId5)
{
	if (!outId5)
		return 5;

	struct {
		uint8_t reserved0;
		uint8_t reserved1;
		uint8_t status;
		uint8_t reserved3;
		uint8_t flags;
		uint8_t reserved5[3];
		uint8_t devId[12];
	} req;
	memset(&req, 0, sizeof(req));

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5DBu, &req, sizeof(req));
	if (ret == 0) {
		if (req.status == 27)
			return 1508;
		if (req.status != 0)
			return 1;
		if (req.flags & 1) {
			*outValid = 1;
			memcpy(outId5, req.devId, 5);
		} else {
			*outValid = 0;
		}
		*outFlag = (req.flags & 2) ? 1 : 0;
	}
	return ret;
}

int SdiDeviceCXD3705::getCertExpire(uint32_t *outExpire)
{
	struct {
		uint8_t reserved0;
		uint8_t reserved1;
		uint8_t status;
		uint8_t reserved3;
		uint8_t flags;
		uint8_t reserved5[11];
		uint32_t expire;
	} req;
	memset(&req, 0, sizeof(req));

	int ret = SwDrv::drvIoctl(drvFd(), 0xC008C5DBu, &req, sizeof(req));
	if (ret == 0) {
		*outExpire = req.expire;
		if (req.status == 27)
			return 1005;
		if (req.status != 0)
			return 1;
		if ((req.flags & 1) == 0)
			return 100;
	}
	return ret;
}
