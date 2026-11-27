/*
 * DTCPIP-SW SwDrv (/dev/swdrv wrapper), SacSessionCXD3705, and SwTtlProbe.
 */

#include "sw_sdi.h"

#include <string.h>

/* ---- SwTtlProbe ---- */

bool SwTtlProbe::openSink(int sock)
{
	return swTtlOpenSink(&m_handle, sock) == 0;
}

int SwTtlProbe::openSource(int sock)
{
	return swTtlOpenSource(&m_handle, sock);
}

bool SwTtlProbe::openSourceFromSocket(int sock, const SwSocket &refSock)
{
	return swTtlOpenSourceWithCtx((void *)(uintptr_t)refSock.fd(), &m_handle, sock) == 0;
}

int SwTtlProbe::getPeerTtl()
{
	int ttl = 255;
	if (swTtlGetPeerTtl(m_handle, &ttl) != 0)
		ttl = 255;
	return (uint8_t)ttl;
}

int SwTtlProbe::close()
{
	if (m_handle != 0) {
		swTtlCloseSource(m_handle);
		m_handle = 0;
	}
	return 0;
}

/* ---- SwDrvBase / SwDrv ---- */

static SwMutex g_swDrvOpenMutex;
static int g_swDrvRefCount = 0;
static SwMutex g_swDrvIoctlMutex;
static int g_swDrvInFlightIoctls = 0;

SwDrvBase::SwDrvBase() {}
SwDrvBase::~SwDrvBase() {}

SwDrv::SwDrv()
	: m_fd(-1)
{
}

SwDrv::~SwDrv() {}

extern "C" int SwMwSessionInit(SwDrv **outDrv)
{
	if (!outDrv)
		return 5;
	static SwDrv s_instance;
	*outDrv = &s_instance;
	return 0;
}

int SwDrv::drvOpen(const char *path)
{
	g_swDrvIoctlMutex.lock();
	int fd = swOpen(path);
	g_swDrvIoctlMutex.unlock();
	return fd;
}

int SwDrv::isStreamIoctl(unsigned int cmd)
{
	switch (cmd) {
	case 0x80047819u:
	case 0x8004781Au:
	case 0x800C7818u:
	case 0xC0017816u:
	case 0xC0507817u:
	case 0xC0987815u:
	case 0xC4B07814u:
		return 0;
	default:
		return 1;
	}
}

int SwDrv::drvIoctl(int fd, unsigned int cmd, void *arg, unsigned int size)
{
	int mapped = 0;
	g_swDrvIoctlMutex.lock();
	++g_swDrvInFlightIoctls;
	g_swDrvIoctlMutex.unlock();

	isStreamIoctl(cmd);
	int ret = swIoctl(fd, cmd, arg, size);
	if (ret != 0) {
		if (cmd == 0xC0087885u) {
			if (ret == 15)
				mapped = 1005;
			else if (ret == 4)
				mapped = 11018;
			else
				mapped = 1;
		} else {
			switch (ret) {
			case 4:  mapped = 11018; break;
			case 5:  mapped = 11013; break;
			case 6:  mapped = 11008; break;
			case 9:  mapped = 11016; break;
			case 11: mapped = 11014; break;
			case 12: mapped = 11015; break;
			default: mapped = 1; break;
			}
		}
	}

	g_swDrvIoctlMutex.lock();
	--g_swDrvInFlightIoctls;
	g_swDrvIoctlMutex.unlock();
	return mapped;
}

int SwDrv::drvClose(int fd)
{
	if (fd == -1)
		return 5;
	swClose(fd);
	return 0;
}

bool SwDrv::open()
{
	g_swDrvOpenMutex.lock();
	bool failed = false;
	if (m_fd == -1) {
		m_fd = drvOpen("/dev/swdrv");
		failed = (m_fd == -1);
	}
	if (!failed)
		++g_swDrvRefCount;
	g_swDrvOpenMutex.unlock();
	return failed;
}

int SwDrv::close()
{
	g_swDrvOpenMutex.lock();
	if (g_swDrvRefCount > 0 && --g_swDrvRefCount == 0) {
		for (;;) {
			g_swDrvIoctlMutex.lock();
			int active = g_swDrvInFlightIoctls;
			if (active <= 0)
				break;
			g_swDrvIoctlMutex.unlock();
			swYield();
		}
		if (m_fd != -1) {
			drvClose(m_fd);
			m_fd = -1;
		}
		g_swDrvIoctlMutex.unlock();
	}
	g_swDrvOpenMutex.unlock();
	return 0;
}

int SwDrv::reset()
{
	close();
	open();
	if (m_fd == -1)
		return 1;
	return drvIoctl(m_fd, 0x20007800u, NULL, 0);
}

int SwDrv::setParamA(void *buf, unsigned int size)
{
	if (!buf || size == 0)
		return 5;
	if (!isOpen())
		return 13;
	return drvIoctl(m_fd, 0x80207801u, buf, size);
}

int SwDrv::setParamB(void *buf, unsigned int size)
{
	if (!buf || size == 0)
		return 5;
	if (!isOpen())
		return 13;
	return drvIoctl(m_fd, 0x80087802u, buf, size);
}

int SwDrv::getParamB(void *buf, unsigned int size)
{
	if (!buf || size == 0)
		return 5;
	if (!isOpen())
		return 13;
	return drvIoctl(m_fd, 0xC0087885u, buf, size);
}

int SwDrv::getInfo(uint8_t kind, void *outBuf, uint32_t *outSize)
{
	if (kind < 5 || !outBuf)
		return 5;
	if (!isOpen())
		return 13;

	struct {
		uint32_t kind;
		uint32_t len;
		uint8_t data[3088];
	} req;
	req.kind = kind;
	req.len = 0;

	int ret = drvIoctl(m_fd, 0xC0087886u, &req, sizeof(req));
	if (ret == 0) {
		memcpy(outBuf, req.data, req.len);
		if (outSize)
			*outSize = req.len;
	}
	return ret;
}

int SwDrv::setDebugOption(void *buf, unsigned int size)
{
	if (!buf || size == 0)
		return 5;
	if (!isOpen())
		return 13;
	return drvIoctl(m_fd, 0x80087892u, buf, size);
}

int SwDrv::setModeFlag(int flag)
{
	if (!isOpen())
		return 13;
	uint8_t val = (uint8_t)flag;
	return drvIoctl(m_fd, 0x80047807u, &val, 1);
}

int SwDrv::setDeviceConfig(int p0, int p1, int p2, char p3, int p4, int p5,
			   char p6, char p7, char p8, char p9, char p10,
			   char p11, char p12)
{
	(void)p4;
	(void)p5;
	if (!isOpen())
		return 13;
	uint8_t cfg[20];
	memset(cfg, 0, sizeof(cfg));
	cfg[0] = (uint8_t)p0;
	cfg[1] = (p1 == 1) ? 1 : 0;
	cfg[2] = (uint8_t)p2;
	cfg[3] = (p3 == 1) ? 1 : 0;
	cfg[6] = (p6 == 1) ? 1 : 0;
	cfg[7] = (p7 == 1) ? 1 : 0;
	cfg[8] = (p8 == 1) ? 1 : 0;
	cfg[9] = (p9 == 1) ? 1 : 0;
	cfg[10] = (p10 == 1) ? 1 : 0;
	cfg[11] = (uint8_t)p11;
	cfg[12] = (uint8_t)p12;
	return drvIoctl(m_fd, 0x80087888u, cfg, sizeof(cfg));
}

int SwDrv::setTimeOffset(uint32_t offset)
{
	if (!isOpen())
		return 13;
	return drvIoctl(m_fd, 0x80047809u, &offset, sizeof(offset));
}

int SwDrv::getSrm(void *outBuf, size_t *outSize)
{
	if (!outBuf || !outSize)
		return 5;
	if (!isOpen())
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
	int ret = drvIoctl(m_fd, 0x400878ADu, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status != 0)
		return 1;
	*outSize = req.len;
	if (req.len != 0)
		memcpy(outBuf, req.data, req.len);
	return 0;
}

int SwDrv::setSrm(const void *buf, size_t size)
{
	if (!buf || size == 0)
		return 5;
	if (!isOpen())
		return 13;

	uint8_t oldSrm[2048];
	size_t oldLen = 0;
	memset(oldSrm, 0, sizeof(oldSrm));
	int ret = getSrm(oldSrm, &oldLen);
	if (ret != 0)
		return ret;

	struct {
		uint8_t reserved0;
		uint8_t reserved1;
		uint8_t status;
		uint8_t reserved3;
		uint32_t len;
		uint8_t data[2048];
	} req;
	memset(&req, 0, sizeof(req));
	req.len = (uint32_t)size;
	memcpy(req.data, buf, size);

	ret = drvIoctl(m_fd, 0xC00878AEu, &req, sizeof(req));
	if (ret != 0)
		return ret;

	switch (req.status) {
	case 0:
	case 0x11: {
		uint8_t newSrm[2048];
		size_t newLen = 0;
		memset(newSrm, 0, sizeof(newSrm));
		ret = getSrm(newSrm, &newLen);
		if (ret != 0)
			return ret;
		if (newLen != oldLen || memcmp(newSrm, oldSrm, newLen) != 0)
			return 0;
		return 1015;
	}
	case 2:    return 5;
	case 0x0C: return 1046;
	case 0x0D: return 10008;
	default:   return 1;
	}
}

/* ---- SacSessionBase / SacSessionCXD3705 ---- */

SacSessionBase::SacSessionBase() {}
SacSessionBase::~SacSessionBase() {}

SacSessionCXD3705::SacSessionCXD3705()
	: m_state(11001), m_channelId(0), m_drv(NULL)
{
	m_reserved[0] = 0;
	m_reserved[1] = 0;
	m_reserved[2] = 0;
	SwMwSessionInit(&m_drv);
}

SacSessionCXD3705::~SacSessionCXD3705()
{
	SacSessionCXD3705::close();
}

bool SacSessionCXD3705::isDriverOpen() const
{
	return m_drv && m_drv->isOpen();
}

bool SacSessionCXD3705::isAllZero(const uint8_t *buf, int len)
{
	if (!buf || len <= 0)
		return false;
	for (int i = 0; i < len; ++i) {
		if (buf[i] != 0)
			return false;
	}
	return true;
}

int SacSessionCXD3705::getVersion(void *outBuf, size_t size)
{
	if (!outBuf || size < 4)
		return 5;
	if (!isDriverOpen())
		return 13;
	uint8_t ver[4];
	memset(ver, 0, sizeof(ver));
	int ret = SwDrv::drvIoctl(m_drv->fd(), 0xC008789Eu, ver, sizeof(ver));
	if (ret == 0) {
		memset(outBuf, 0, size);
		memcpy(outBuf, ver, 4);
	}
	return ret;
}

int SacSessionCXD3705::exchangeKey(const void *inBuf, unsigned int inSize,
				   void *outBuf, size_t outSize)
{
	if (!inBuf || inSize < 32 || !outBuf || outSize < 32)
		return 5;
	memset(outBuf, 0, outSize);
	if (m_state != 11001)
		return m_state;
	if (!isDriverOpen())
		return 13;

	struct {
		uint8_t inA[16];
		uint8_t inB[16];
		uint8_t outA[16];
		uint8_t outB[16];
		uint8_t channelId;
		uint8_t status;
		uint8_t reserved[2];
	} req;
	memset(&req, 0, sizeof(req));
	memcpy(req.inA, inBuf, 16);
	memcpy(req.inB, (const uint8_t *)inBuf + 16, 16);

	int ret = SwDrv::drvIoctl(m_drv->fd(), 0xC008789Fu, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status != 0)
		return 11004;
	if (isAllZero(req.outA, 16) && isAllZero(req.outB, 16))
		return 11004;
	if (req.channelId == 0)
		return 11006;

	memcpy(outBuf, req.outA, 16);
	memcpy((uint8_t *)outBuf + 16, req.outB, 16);
	m_channelId = req.channelId;
	m_state = 11002;
	return 0;
}

int SacSessionCXD3705::authenticate(const void *inBuf, unsigned int inSize)
{
	if (!inBuf || inSize < 16)
		return 5;
	if (m_state != 11002)
		return m_state;
	if (!hasChannel())
		return 1;
	if (!isDriverOpen())
		return 13;

	struct {
		uint8_t token[16];
		uint8_t channelId;
		uint8_t status;
		uint8_t reserved[2];
	} req;
	memset(&req, 0, sizeof(req));
	memcpy(req.token, inBuf, 16);
	req.channelId = m_channelId;

	int ret = SwDrv::drvIoctl(m_drv->fd(), 0xC00878A0u, &req, sizeof(req));
	if (ret != 0)
		return ret;
	if (req.status != 0) {
		close();
		return (req.status == 20) ? 11020 : 11005;
	}
	m_state = 11003;
	return 0;
}

bool SacSessionCXD3705::isAuthenticated()
{
	return m_state == 11003;
}

int SacSessionCXD3705::close()
{
	int ret = 0;
	if (m_state == 11002 || m_state == 11003) {
		if (hasChannel()) {
			if (!isDriverOpen()) {
				ret = 13;
			} else {
				uint8_t req[4];
				memset(req, 0, sizeof(req));
				req[0] = m_channelId;
				ret = SwDrv::drvIoctl(m_drv->fd(), 0xC00878A1u, req, sizeof(req));
				if (ret == 0 && req[1] != 0)
					ret = 11007;
			}
		}
	}
	m_state = 11001;
	m_channelId = 0;
	return ret;
}

int SacSessionCXD3705::getState()
{
	return m_state;
}

int SacSessionCXD3705::getChannelId(uint8_t *outId, int outSize) const
{
	if (!outId || outSize == 0)
		return 5;
	*outId = m_channelId;
	return 0;
}

/* ---- C bridges used by DTCPIP-MW (SwMwSACDevice.cpp) ---- */

extern "C" int SwMwSacChannelOpen(SacSessionBase **outSession)
{
	if (!outSession)
		return 5;
	*outSession = NULL;
	SacSessionBase *sess = new (std::nothrow) SacSessionCXD3705();
	*outSession = sess;
	return sess ? 0 : 1;
}

extern "C" int SwMwSacChannelRelease(SacSessionBase **sessionPtr)
{
	if (!sessionPtr || !*sessionPtr)
		return 5;
	delete *sessionPtr;
	*sessionPtr = NULL;
	return 0;
}
