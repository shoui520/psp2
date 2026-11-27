/*
 * DTCPIP-SW certificate, device ID, public key, SRM, and buffer factory implementation.
 */

#include "sw_cert_srm.h"

#include <stdio.h>
#include <string.h>

/* ---- Default SdiBufferFactory & swSetBufferFactory ---- */

class SwDefaultBufferFactory : public SdiBufferFactoryBase {
public:
	SwDefaultBufferFactory() {}
	virtual ~SwDefaultBufferFactory() {}

	virtual SdiBuffer *create(void *extMem, uint32_t capacity, const SwString &label)
	{
		return new (std::nothrow) SdiBuffer(extMem, capacity, label);
	}
};

static SwDefaultBufferFactory g_defaultBufferFactory;
static SdiBufferFactoryBase *g_activeBufferFactory = &g_defaultBufferFactory;

extern "C" void swSetBufferFactory(SdiBufferFactoryBase *factory)
{
	if (factory)
		g_activeBufferFactory = factory;
	else
		g_activeBufferFactory = &g_defaultBufferFactory;
}

SdiBufferFactoryBase *swGetBufferFactory()
{
	return g_activeBufferFactory ? g_activeBufferFactory : &g_defaultBufferFactory;
}

/* ---- SwDeviceId ---- */

void SwDeviceId::xorCopy5(const uint8_t *src, uint8_t *dst)
{
	for (unsigned int i = 0; i < 5; ++i)
		dst[i] = (uint8_t)(src[i] ^ 0x13u);
}

SwDeviceId::SwDeviceId()
{
	memset(m_masked, 0, sizeof(m_masked));
	memset(m_reserved, 0, sizeof(m_reserved));
}

SwDeviceId::SwDeviceId(const SwDeviceId &other)
{
	memcpy(m_masked, other.m_masked, 5);
	memset(m_reserved, 0, sizeof(m_reserved));
}

SwDeviceId::~SwDeviceId() {}

int SwDeviceId::set(const uint8_t *raw5)
{
	if (!raw5)
		return 5;
	xorCopy5(raw5, m_masked);
	return 0;
}

int SwDeviceId::get(uint8_t *out5) const
{
	if (!out5)
		return 5;
	xorCopy5(m_masked, out5);
	return 0;
}

int SwDeviceId::compare(uint8_t *outSame, const SwDeviceId &other) const
{
	*outSame = (memcmp(m_masked, other.m_masked, 5) == 0) ? 1 : 0;
	return 0;
}

void SwDeviceId::toHexString(SwString &out) const
{
	uint8_t raw[5];
	char hex[3];
	out.clear();
	xorCopy5(m_masked, raw);
	for (unsigned int i = 0; i < 5; ++i) {
		snprintf(hex, sizeof(hex), "%02x", raw[i]);
		hex[2] = '\0';
		out.append(hex);
	}
}

bool SwDeviceId::equals(const SwDeviceId &other) const
{
	if (this == &other)
		return true;
	return memcmp(m_masked, other.m_masked, 5) == 0;
}

/* ---- SwPubKey ---- */

SwPubKey::SwPubKey()
	: m_data(NULL)
{
}

SwPubKey::~SwPubKey()
{
	delete[] m_data;
	m_data = NULL;
}

int SwPubKey::set(const void *raw40)
{
	if (!raw40)
		return 5;
	delete[] m_data;
	m_data = new (std::nothrow) uint8_t[40];
	if (!m_data)
		return 2;
	memcpy(m_data, raw40, 40);
	return 0;
}

int SwPubKey::get(void *out40) const
{
	if (!out40)
		return 5;
	if (!m_data)
		return 4;
	memcpy(out40, m_data, 40);
	return 0;
}

/* ---- SwCert ---- */

SwCert::SwCert()
	: m_raw(NULL), m_len(0), m_certType(0), m_formatVer(0), m_category(0),
	  m_reserved0f(0), m_reserved10(0), m_apFlag(0), m_kpFlag(0),
	  m_reserved13(0), m_deviceId(NULL), m_pubKey(NULL), m_sig(NULL),
	  m_extInfo(0), m_extSig(NULL)
{
}

SwCert::~SwCert()
{
	clear();
}

void SwCert::clear()
{
	delete[] m_raw;
	m_raw = NULL;
	m_len = 0;
	m_certType = 0;
	m_formatVer = 0;
	m_category = 0;
	m_reserved0f = 0;
	m_reserved10 = 0;
	m_apFlag = 0;
	m_kpFlag = 0;
	m_reserved13 = 0;
	delete m_deviceId;
	m_deviceId = NULL;
	delete m_pubKey;
	m_pubKey = NULL;
	m_sig = NULL;
	m_extInfo = 0;
	m_extSig = NULL;
}

int SwCert::setRaw(const void *data, uint32_t len)
{
	if (!data || (len != 88 && len != 132))
		return 5;
	clear();

	m_raw = new (std::nothrow) uint8_t[len];
	m_deviceId = new (std::nothrow) SwDeviceId();
	m_pubKey = new (std::nothrow) SwPubKey();
	if (!m_raw || !m_deviceId || !m_pubKey) {
		clear();
		return 2;
	}

	memcpy(m_raw, data, len);
	m_len = len;
	m_certType = (uint8_t)(m_raw[0] >> 4);
	m_formatVer = (uint8_t)(m_raw[0] & 0x0Fu);
	m_category = (uint8_t)(m_raw[1] >> 4);
	m_reserved0f = (uint8_t)(m_raw[1] & 0x0Fu);
	m_reserved10 = (uint8_t)(m_raw[2] >> 2);
	m_apFlag = (uint8_t)((m_raw[2] >> 1) & 1u);
	m_kpFlag = (uint8_t)(m_raw[2] & 1u);

	int err = m_deviceId->set(m_raw + 3);
	if (err == 0)
		err = m_pubKey->set(m_raw + 8);
	if (err != 0) {
		clear();
		return err;
	}

	m_sig = m_raw + 48;
	if (len == 132) {
		uint32_t extBe = 0;
		memcpy(&extBe, m_raw + 88, 4);
		m_extInfo = swSwap32(extBe);
		m_extSig = m_raw + 92;
	}
	return 0;
}

int SwCert::copyFrom(const SwCert &other)
{
	const void *data = NULL;
	uint32_t len = 0;
	int err = other.getRaw(&data, &len);
	if (err != 0)
		return err;
	return setRaw(data, len);
}

int SwCert::getCategory(uint8_t *outCat) const
{
	if (!m_raw)
		return 4;
	*outCat = m_category;
	return 0;
}

int SwCert::getRaw(const void **outData, uint32_t *outLen,
		   const void **outSig, const void **outExtSig) const
{
	if (!outData)
		return 5;
	if (!m_raw || !m_sig)
		return 4;
	*outData = m_raw;
	if (outLen)
		*outLen = m_len;
	if (outSig)
		*outSig = m_sig;
	if (outExtSig)
		*outExtSig = m_extSig;
	return 0;
}

int SwCert::getPubKey(SwPubKey **outKey) const
{
	if (!outKey)
		return 5;
	if (!m_pubKey)
		return 4;
	*outKey = m_pubKey;
	return 0;
}

int SwCert::getDeviceIdObj(SwDeviceId **outId) const
{
	if (!outId)
		return 5;
	if (!m_deviceId)
		return 4;
	*outId = m_deviceId;
	return 0;
}

int SwCert::getApFlag(uint8_t *outFlag) const
{
	*outFlag = 0;
	if (!m_raw || !m_sig)
		return 4;
	if (m_apFlag == 0)
		return 0;
	if (m_apFlag == 1) {
		*outFlag = 1;
		return 0;
	}
	return 1;
}

int SwCert::getKpFlag(uint8_t *outFlag) const
{
	*outFlag = 0;
	if (!m_raw || !m_sig)
		return 4;
	if (m_kpFlag == 0)
		return 0;
	if (m_kpFlag == 1) {
		*outFlag = 1;
		return 0;
	}
	return 1;
}

int SwCert::validateFormat() const
{
	if (!m_raw || !m_sig)
		return 4;
	if (m_certType != 0)
		return 1004;
	if (m_formatVer == 1 || m_formatVer == 2)
		return 0;
	return 1004;
}

/* ---- SwSrm ---- */

SwSrm::SwSrm()
	: m_raw(NULL), m_len(0), m_srmType(0), m_reserved0d(0), m_genFlag(0),
	  m_reserved0f(0), m_version(0), m_generation(0), m_reserved13(0), m_sig(NULL)
{
}

SwSrm::~SwSrm()
{
	clear();
}

void SwSrm::clear()
{
	delete[] m_raw;
	m_raw = NULL;
	m_len = 0;
	m_srmType = 0;
	m_reserved0d = 0;
	m_genFlag = 0;
	m_reserved0f = 0;
	m_version = 0;
	m_generation = 0;
	m_reserved13 = 0;
	m_sig = NULL;
}

int SwSrm::parseGenerations(uint8_t *outGen, const uint8_t *data, size_t len)
{
	*outGen = 0;
	if (!data)
		return 5;
	if (len < 0x30u)
		return 1014;

	const uint8_t *p = data + 4;
	size_t pos = 4;
	int err = 0;
	for (;;) {
		uint16_t genLenBe = 0;
		memcpy(&genLenBe, p, 2);
		uint16_t genLen = swSwap16(genLenBe);
		if (genLen < 0x2Cu) {
			err = 1014;
			break;
		}
		p += genLen;
		pos += genLen;
		if (pos >= len) {
			if (pos != len)
				err = 1014;
			break;
		}
		++(*outGen);
	}
	if (err != 0)
		*outGen = 0;
	return err;
}

int SwSrm::setRaw(const void *data, size_t len, const SwCert *deviceCert)
{
	(void)deviceCert;
	if (!data) {
		if (len != 0)
			return 5;
		clear();
		return 0;
	}
	if (len < 0x30u)
		return 5;

	clear();
	m_raw = new (std::nothrow) uint8_t[len];
	if (!m_raw)
		return 2;
	memcpy(m_raw, data, len);
	m_len = len;
	m_srmType = (uint8_t)(m_raw[0] >> 4);
	m_reserved0d = (uint8_t)(m_raw[0] & 0x0Fu);
	m_genFlag = m_raw[1];
	uint16_t verBe = 0;
	memcpy(&verBe, m_raw + 2, 2);
	m_version = swSwap16(verBe);
	m_sig = m_raw + len - 40;

	int err = parseGenerations(&m_generation, m_raw, m_len);
	if (err != 0)
		clear();
	return err;
}

int SwSrm::copyFrom(const SwSrm &other)
{
	void *ptr = NULL;
	size_t len = 0;
	int err = other.getRaw(&ptr, &len);
	if (err != 0)
		return err;
	return setRaw(ptr, len);
}

int SwSrm::getRaw(void **outPtr, size_t *outLen) const
{
	if (!outPtr)
		return 5;
	*outPtr = m_raw;
	if (outLen)
		*outLen = m_len;
	return 0;
}

int SwSrm::getMaxSrmSize(uint32_t *outSize, uint8_t certType)
{
	if (certType == 0) {
		*outSize = 128;
		return 0;
	}
	if (certType == 1) {
		*outSize = 1024;
		return 0;
	}
	return 1003;
}

int SwSrm::compareForExchange(int *outAction, void **outSendPtr, uint32_t *outSendLen,
			      uint8_t *outRequestGen, uint16_t peerVer,
			      uint8_t peerMaxGen, uint8_t peerGen, uint8_t localMaxGen) const
{
	if (!outSendPtr) {
		*outSendLen = 0;
		*outRequestGen = 0;
		return 5;
	}

	int err = 0;
	if (peerVer > m_version) {
		*outAction = 2;
		*outRequestGen = (peerGen < localMaxGen) ? peerGen : localMaxGen;
	} else if (peerVer == m_version) {
		if (peerGen > m_generation) {
			if (localMaxGen == m_generation) {
				*outAction = 1;
			} else {
				*outAction = 2;
				*outRequestGen = (peerGen > localMaxGen) ? localMaxGen : peerGen;
			}
		} else if (peerGen == m_generation || peerMaxGen == peerGen) {
			*outAction = 1;
		} else {
			*outAction = 0;
			if (peerMaxGen < m_generation) {
				uint32_t sz = 0;
				err = getMaxSrmSize(&sz, peerMaxGen);
				if (err == 0) {
					*outSendPtr = m_raw;
					*outSendLen = sz;
				}
			} else {
				*outSendPtr = m_raw;
				*outSendLen = (uint32_t)m_len;
			}
		}
	} else {
		*outAction = 0;
		if (peerMaxGen < m_generation) {
			uint32_t sz = 0;
			err = getMaxSrmSize(&sz, peerMaxGen);
			if (err == 0) {
				*outSendPtr = m_raw;
				*outSendLen = sz;
			}
		} else {
			*outSendPtr = m_raw;
			*outSendLen = (uint32_t)m_len;
		}
	}

	if (err != 0) {
		*outSendPtr = NULL;
		*outSendLen = 0;
		*outRequestGen = 0;
		return err;
	}
	if (*outAction == 0) {
		*outRequestGen = 0;
		if (!m_raw || m_len == 0) {
			*outSendPtr = NULL;
			*outSendLen = 0;
			return 1;
		}
	} else if (*outAction == 2) {
		*outSendPtr = NULL;
		*outSendLen = 0;
	} else {
		*outSendPtr = NULL;
		*outSendLen = 0;
		*outRequestGen = 0;
	}
	return 0;
}

int SwSrm::isDeviceRevoked(uint8_t *outRevoked, const SwDeviceId &devId) const
{
	uint8_t raw[5];
	memset(raw, 0, sizeof(raw));
	int err = devId.get(raw);
	if (err != 0) {
		*outRevoked = 0;
		return err;
	}
	uint32_t lowBe = 0;
	memcpy(&lowBe, raw + 1, 4);
	return isDeviceRevokedRaw(outRevoked, raw[0], swSwap32(lowBe));
}

int SwSrm::isDeviceRevokedRaw(uint8_t *outRevoked, uint8_t highByte, uint32_t lowWord) const
{
	*outRevoked = 0;
	if (!m_raw)
		return 0;
	if (m_len < 0x30u)
		return 1014;
	if (m_len == 4u)
		return 0;

	const uint8_t *genPtr = m_raw + 4;
	size_t totalPos = 4;
	bool done = false;
	int err = 0;

	while (totalPos != m_len && !done) {
		uint16_t genLenBe = 0;
		memcpy(&genLenBe, genPtr, 2);
		uint16_t genLen = swSwap16(genLenBe);
		const uint8_t *cur = genPtr + 2;
		totalPos += genLen;
		uint16_t bodyLen = (uint16_t)(genLen - 42);
		uint16_t off = 0;

		while (off != bodyLen && !done) {
			uint8_t hdr = *cur++;
			++off;
			uint8_t entryCount = (uint8_t)(hdr & 0x1Fu);
			uint8_t entryKind = (uint8_t)(hdr & 0xE0u);

			if (entryKind == 0x00u) {
				for (uint8_t idx = 0; idx < entryCount; ++idx) {
					uint8_t idHi = *cur++;
					uint32_t idLoBe = 0;
					memcpy(&idLoBe, cur, 4);
					uint32_t idLo = swSwap32(idLoBe);
					cur += 4;
					off = (uint16_t)(off + 5);
					if (idHi == highByte && idLo == lowWord) {
						*outRevoked = 1;
						done = true;
						break;
					}
				}
			} else if (entryKind == 0x20u) {
				for (uint8_t idx = 0; idx < entryCount; ++idx) {
					uint8_t startHi = *cur++;
					uint32_t startLoBe = 0;
					memcpy(&startLoBe, cur, 4);
					uint32_t startLo = swSwap32(startLoBe);
					cur += 4;
					uint16_t spanBe = 0;
					memcpy(&spanBe, cur, 2);
					uint16_t span = swSwap16(spanBe);
					cur += 2;
					off = (uint16_t)(off + 7);

					uint32_t endLo = startLo + span;
					uint8_t endHi = startHi;
					if (endLo < startLo) {
						if (startHi == 0xFFu) {
							*outRevoked = 0;
							err = 1014;
							done = true;
							break;
						}
						endHi = (uint8_t)(startHi + 1u);
					}

					bool inRange = false;
					if (startHi == endHi) {
						inRange = (highByte == startHi &&
							   lowWord >= startLo && lowWord <= endLo);
					} else if (highByte == startHi) {
						inRange = (lowWord >= startLo);
					} else if (highByte == endHi) {
						inRange = (lowWord <= endLo);
					}
					if (inRange) {
						*outRevoked = 1;
						done = true;
						break;
					}
				}
			} else if (entryCount != 0) {
				*outRevoked = 0;
				err = 1014;
				done = true;
			}
		}

		genPtr = cur + 40;
	}
	return err;
}
