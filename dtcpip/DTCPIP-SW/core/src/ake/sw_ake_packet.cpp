/*
 * DTCPIP-SW AKE wire framing and DTCP-IP AKE packet serialization/parsing.
 */

#include "sw_ake_packet.h"

#include <string.h>

/* ---- SwAkeFrameBase ---- */

SwAkeFrameBase::SwAkeFrameBase(uint32_t frameKind, uint16_t payloadLen, uint8_t frameType,
			       uint8_t akeProcedure, uint8_t statusCode)
	: m_frameKind(frameKind), m_payloadLen(payloadLen), m_frameType(frameType),
	  m_akeProcedure(akeProcedure), m_statusCode(statusCode),
	  m_hdr4(NULL), m_wireBuf(NULL), m_wireLen(0)
{
	m_reserved0d[0] = 0;
	m_reserved0d[1] = 0;
	m_reserved0d[2] = 0;
}

SwAkeFrameBase::~SwAkeFrameBase()
{
	delete[] m_hdr4;
	m_hdr4 = NULL;
	resetWire();
}

void SwAkeFrameBase::resetWire()
{
	delete[] m_wireBuf;
	m_wireBuf = NULL;
	m_wireLen = 0;
}

int SwAkeFrameBase::buildFrameHeader()
{
	delete[] m_hdr4;
	m_hdr4 = new (std::nothrow) uint8_t[4];
	if (!m_hdr4)
		return 2;
	m_hdr4[0] = m_frameType;
	uint16_t lenBe = swSwap16(m_payloadLen);
	memcpy(m_hdr4 + 1, &lenBe, 2);
	m_hdr4[3] = (uint8_t)((m_akeProcedure << 4) | (m_statusCode & 0x0Fu));
	return 0;
}

int SwAkeFrameBase::parseFrameHeader(const uint8_t *buf, uint32_t len)
{
	if (!buf || len < 4)
		return 5;
	delete[] m_hdr4;
	m_hdr4 = new (std::nothrow) uint8_t[4];
	if (!m_hdr4)
		return 2;
	memcpy(m_hdr4, buf, 4);
	m_frameType = buf[0];
	uint16_t lenBe = 0;
	memcpy(&lenBe, buf + 1, 2);
	m_payloadLen = swSwap16(lenBe);
	m_akeProcedure = (uint8_t)(buf[3] >> 4);
	m_statusCode = (uint8_t)(buf[3] & 0x0Fu);
	return 0;
}

int SwAkeFrameBase::getFrameHeader(void *out4) const
{
	if (!out4)
		return 5;
	if (!m_hdr4)
		return 4;
	memcpy(out4, m_hdr4, 4);
	return 0;
}

int SwAkeFrameBase::build()
{
	int err = buildFrameHeader();
	if (err != 0)
		return err;
	resetWire();
	m_wireBuf = new (std::nothrow) uint8_t[4];
	if (!m_wireBuf)
		return 2;
	memcpy(m_wireBuf, m_hdr4, 4);
	m_wireLen = 4;
	return 0;
}

int SwAkeFrameBase::parse(const uint8_t *buf, uint32_t len)
{
	return parseFrameHeader(buf, len);
}

int SwAkeFrameBase::serialize(uint8_t **outBuf, uint32_t *outLen)
{
	if (!outBuf || !outLen)
		return 5;
	if (!m_wireBuf) {
		int err = build();
		if (err != 0)
			return err;
	}
	*outBuf = m_wireBuf;
	*outLen = m_wireLen;
	return 0;
}

/* ---- SwAkePacket ---- */

SwAkePacket::SwAkePacket(uint64_t typeMask, uint16_t payloadLen, uint8_t frameType,
			 uint8_t ctypeOrResponse, uint8_t subfunction, uint8_t akeLabel,
			 uint8_t exchKey, uint8_t akeInfoFlag, uint8_t exchKeyType,
			 uint8_t subfnParam, uint8_t akeProcedure, uint8_t statusCode,
			 uint8_t versionHi, uint8_t versionLo)
	: SwAkeFrameBase(0, payloadLen, frameType, akeProcedure, statusCode),
	  m_typeMask(typeMask),
	  m_versionHi(versionHi),
	  m_versionLo(versionLo),
	  m_ctype(ctypeOrResponse),
	  m_subfunction(subfunction),
	  m_akeLabel(akeLabel),
	  m_exchKey(exchKey),
	  m_akeInfoFlag(akeInfoFlag),
	  m_exchKeyType(exchKeyType),
	  m_subfnParam(subfnParam),
	  m_akeHdr7(NULL),
	  m_payload(NULL),
	  m_payloadSize(0)
{
}

SwAkePacket::~SwAkePacket()
{
	delete[] m_akeHdr7;
	m_akeHdr7 = NULL;
	delete[] m_payload;
	m_payload = NULL;
}

int SwAkePacket::buildAkeHeader()
{
	delete[] m_akeHdr7;
	m_akeHdr7 = new (std::nothrow) uint8_t[7];
	if (!m_akeHdr7)
		return 2;
	memset(m_akeHdr7, 0, 7);
	m_akeHdr7[0] = (uint8_t)((m_versionHi << 4) | (m_versionLo & 0x0Fu));
	m_akeHdr7[1] = m_ctype;
	m_akeHdr7[2] = m_subfunction;
	m_akeHdr7[3] = m_akeLabel;
	m_akeHdr7[4] = m_exchKey;
	m_akeHdr7[5] = m_akeInfoFlag;
	m_akeHdr7[6] = (uint8_t)((m_exchKeyType << 4) | (m_subfnParam & 0x0Fu));
	return 0;
}

int SwAkePacket::parseAkeHeader(const uint8_t *buf, uint32_t len)
{
	if (!buf || len < 7)
		return 5;
	delete[] m_akeHdr7;
	m_akeHdr7 = new (std::nothrow) uint8_t[7];
	if (!m_akeHdr7)
		return 2;
	memcpy(m_akeHdr7, buf, 7);
	m_versionHi = (uint8_t)(buf[0] >> 4);
	m_versionLo = (uint8_t)(buf[0] & 0x0Fu);
	m_ctype = buf[1];
	m_subfunction = buf[2];
	m_akeLabel = buf[3];
	m_exchKey = buf[4];
	m_akeInfoFlag = buf[5];
	m_exchKeyType = (uint8_t)(buf[6] >> 4);
	m_subfnParam = (uint8_t)(buf[6] & 0x0Fu);
	return 0;
}

int SwAkePacket::getAkeHeader(void *out7) const
{
	if (!out7)
		return 5;
	if (!m_akeHdr7)
		return 4;
	memcpy(out7, m_akeHdr7, 7);
	return 0;
}

int SwAkePacket::setPayload(const void *data, uint32_t len)
{
	delete[] m_payload;
	m_payload = NULL;
	m_payloadSize = 0;
	if (!data || len == 0)
		return 0;
	m_payload = new (std::nothrow) uint8_t[len];
	if (!m_payload)
		return 2;
	memcpy(m_payload, data, len);
	m_payloadSize = len;
	m_payloadLen = (uint16_t)(8u + len);
	return 0;
}

int SwAkePacket::build()
{
	m_payloadLen = (uint16_t)(8u + m_payloadSize);
	int err = buildFrameHeader();
	if (err != 0)
		return err;
	err = buildAkeHeader();
	if (err != 0)
		return err;

	resetWire();
	uint32_t total = 11u + m_payloadSize;
	m_wireBuf = new (std::nothrow) uint8_t[total];
	if (!m_wireBuf)
		return 2;
	memcpy(m_wireBuf, m_hdr4, 4);
	memcpy(m_wireBuf + 4, m_akeHdr7, 7);
	if (m_payload && m_payloadSize > 0)
		memcpy(m_wireBuf + 11, m_payload, m_payloadSize);
	m_wireLen = total;
	return 0;
}

int SwAkePacket::parse(const uint8_t *buf, uint32_t len)
{
	if (!buf || len < 11)
		return 5;
	int err = parseFrameHeader(buf, len);
	if (err != 0)
		return err;
	err = parseAkeHeader(buf + 4, len - 4);
	if (err != 0)
		return err;
	if (len > 11) {
		err = setPayload(buf + 11, len - 11);
		if (err != 0)
			return err;
	}
	return 0;
}

bool SwAkePacket::isCommand() const
{
	return (m_typeMask & 0x0000000000007FFFull) != 0 ||
	       (m_typeMask & 0x0000103F00000000ull) != 0;
}

bool SwAkePacket::isResponse() const
{
	return (m_typeMask & 0x00000000FEFF8000ull) != 0 ||
	       (m_typeMask & 0x000007C000000000ull) != 0;
}

static uint64_t swClassifyAkePacket(uint8_t ctype, uint8_t subfn)
{
	bool isRsp = (ctype == 0x08 || ctype == 0x09 || ctype == 0x0A || ctype == 0x0C);
	if (ctype == 0x0A)
		return SW_AKE_PKT_REJECT_RSP;
	switch (subfn) {
	case SW_AKE_SUBFN_CHALLENGE:
		return isRsp ? SW_AKE_PKT_CHALLENGE_RSP : SW_AKE_PKT_CHALLENGE_CMD;
	case SW_AKE_SUBFN_RESPONSE:
		return isRsp ? SW_AKE_PKT_RESPONSE_RSP : SW_AKE_PKT_RESPONSE_CMD;
	case SW_AKE_SUBFN_EXCHANGE_KEY:
		return isRsp ? SW_AKE_PKT_EXCHANGE_KEY_RSP : SW_AKE_PKT_EXCHANGE_KEY_CMD;
	case SW_AKE_SUBFN_SRM:
		return isRsp ? SW_AKE_PKT_SRM_RSP : SW_AKE_PKT_SRM_CMD;
	case SW_AKE_SUBFN_CONTENT_KEY_REQ:
		return isRsp ? SW_AKE_PKT_CONTENT_KEY_REQ_RSP : SW_AKE_PKT_CONTENT_KEY_REQ_CMD;
	case SW_AKE_SUBFN_CAPABILITY_EXCH:
		return isRsp ? SW_AKE_PKT_CAPABILITY_EXCH_RSP : SW_AKE_PKT_CAPABILITY_EXCH_CMD;
	case SW_AKE_SUBFN_RTT_READY:
		return isRsp ? SW_AKE_PKT_RTT_READY_RSP : SW_AKE_PKT_RTT_READY_CMD;
	case SW_AKE_SUBFN_RTT_SETUP:
		return isRsp ? SW_AKE_PKT_RTT_SETUP_RSP : SW_AKE_PKT_RTT_SETUP_CMD;
	case SW_AKE_SUBFN_RTT_TEST:
		return isRsp ? SW_AKE_PKT_RTT_TEST_RSP : SW_AKE_PKT_RTT_TEST_CMD;
	case SW_AKE_SUBFN_RTT_VERIFY:
		return isRsp ? SW_AKE_PKT_RTT_VERIFY_RSP : SW_AKE_PKT_RTT_VERIFY_CMD;
	case SW_AKE_SUBFN_RTT_VERIFY_READY:
		return isRsp ? SW_AKE_PKT_RTT_VERIFY_READY_RSP : SW_AKE_PKT_RTT_VERIFY_READY_CMD;
	case SW_AKE_SUBFN_MV_INIT:
		return isRsp ? 0x0000004000000000ull : SW_AKE_PKT_MV_INIT_CMD;
	case SW_AKE_SUBFN_MV_FINALIZE:
		return isRsp ? 0x0000008000000000ull : SW_AKE_PKT_MV_FINALIZE_CMD;
	case SW_AKE_SUBFN_MV_COMPLETE:
		return isRsp ? 0x0000010000000000ull : 0x0000000400000000ull;
	case SW_AKE_SUBFN_MV_CANCEL:
		return isRsp ? 0x0000020000000000ull : 0x0000000800000000ull;
	case SW_AKE_SUBFN_STATUS:
		return isRsp ? SW_AKE_PKT_AKE_STATUS_RSP : SW_AKE_PKT_AKE_STATUS_CMD;
	default:
		return SW_AKE_PKT_UNKNOWN;
	}
}

SwAkePacket *SwAkePacket::createChallenge(bool isResponse, uint8_t akeLabel,
					  uint8_t akeProcedure, uint8_t status,
					  const uint8_t nonce8[8],
					  const void *certData, uint32_t certLen)
{
	uint8_t ctype = isResponse ? 0x09 : 0x00;
	SwAkePacket *pkt = new (std::nothrow) SwAkePacket(
		isResponse ? SW_AKE_PKT_CHALLENGE_RSP : SW_AKE_PKT_CHALLENGE_CMD,
		(uint16_t)(8u + 8u + certLen), 0x01, ctype, SW_AKE_SUBFN_CHALLENGE,
		akeLabel, 0xFF, 0xFF, 0x0F, 0x0F, akeProcedure, status, 0, 1);
	if (!pkt)
		return NULL;
	if (nonce8 && certData && certLen > 0) {
		SwScopedArray<uint8_t> body(8u + certLen);
		if (!body.get()) {
			delete pkt;
			return NULL;
		}
		memcpy(body.get(), nonce8, 8);
		memcpy(body.get() + 8, certData, certLen);
		pkt->setPayload(body.get(), 8u + certLen);
	}
	return pkt;
}

SwAkePacket *SwAkePacket::createResponse(bool isResponse, uint8_t akeLabel,
					 uint8_t akeProcedure, uint8_t status,
					 const uint8_t pub20[20], const uint8_t sig40[40])
{
	uint8_t ctype = isResponse ? 0x09 : 0x00;
	SwAkePacket *pkt = new (std::nothrow) SwAkePacket(
		isResponse ? SW_AKE_PKT_RESPONSE_RSP : SW_AKE_PKT_RESPONSE_CMD,
		(uint16_t)(8u + 60u), 0x01, ctype, SW_AKE_SUBFN_RESPONSE,
		akeLabel, 0xFF, 0xFF, 0x0F, 0x0F, akeProcedure, status, 0, 1);
	if (!pkt)
		return NULL;
	if (pub20 && sig40) {
		uint8_t body[60];
		memcpy(body, pub20, 20);
		memcpy(body + 20, sig40, 40);
		pkt->setPayload(body, sizeof(body));
	}
	return pkt;
}

SwAkePacket *SwAkePacket::createExchangeKey(bool isResponse, uint8_t akeLabel,
					    uint8_t exchKeyLabel, uint8_t akeProcedure,
					    uint8_t status, const uint8_t kx12[12],
					    uint8_t keyFlags)
{
	uint8_t ctype = isResponse ? 0x09 : 0x00;
	SwAkePacket *pkt = new (std::nothrow) SwAkePacket(
		isResponse ? SW_AKE_PKT_EXCHANGE_KEY_RSP : SW_AKE_PKT_EXCHANGE_KEY_CMD,
		(uint16_t)(8u + (kx12 ? 12u : 0u)), 0x01, ctype, SW_AKE_SUBFN_EXCHANGE_KEY,
		akeLabel, exchKeyLabel, keyFlags, 0x0F, 0x0F, akeProcedure, status, 0, 1);
	if (pkt && kx12)
		pkt->setPayload(kx12, 12);
	return pkt;
}

SwAkePacket *SwAkePacket::createSrm(bool isResponse, uint8_t akeLabel,
				    uint8_t akeProcedure, uint8_t status,
				    const void *srmData, uint32_t srmLen)
{
	uint8_t ctype = isResponse ? 0x09 : 0x00;
	SwAkePacket *pkt = new (std::nothrow) SwAkePacket(
		isResponse ? SW_AKE_PKT_SRM_RSP : SW_AKE_PKT_SRM_CMD,
		(uint16_t)(8u + srmLen), 0x01, ctype, SW_AKE_SUBFN_SRM,
		akeLabel, 0xFF, 0xFF, 0x0F, 0x0F, akeProcedure, status, 0, 1);
	if (pkt && srmData && srmLen > 0)
		pkt->setPayload(srmData, srmLen);
	return pkt;
}

SwAkePacket *SwAkePacket::createRtt(uint8_t subfn, bool isResponse, uint8_t akeLabel,
				    uint8_t akeProcedure, uint8_t status,
				    const void *body, uint32_t bodyLen)
{
	uint8_t ctype = isResponse ? 0x09 : 0x00;
	SwAkePacket *pkt = new (std::nothrow) SwAkePacket(
		swClassifyAkePacket(ctype, subfn),
		(uint16_t)(8u + bodyLen), 0x01, ctype, subfn,
		akeLabel, 0xFF, 0xFF, 0x0F, 0x0F, akeProcedure, status, 0, 1);
	if (pkt && body && bodyLen > 0)
		pkt->setPayload(body, bodyLen);
	return pkt;
}

SwAkePacket *SwAkePacket::createContentKeyReq(bool isResponse, uint8_t akeLabel,
					      uint8_t akeProcedure, uint8_t status,
					      const uint8_t nonce8[8])
{
	uint8_t ctype = isResponse ? 0x09 : 0x00;
	SwAkePacket *pkt = new (std::nothrow) SwAkePacket(
		isResponse ? SW_AKE_PKT_CONTENT_KEY_REQ_RSP : SW_AKE_PKT_CONTENT_KEY_REQ_CMD,
		(uint16_t)(8u + (nonce8 ? 8u : 0u)), 0x01, ctype,
		SW_AKE_SUBFN_CONTENT_KEY_REQ, akeLabel, 0xFF, 0xFF, 0x0F, 0x0F,
		akeProcedure, status, 0, 1);
	if (pkt && nonce8)
		pkt->setPayload(nonce8, 8);
	return pkt;
}

SwAkePacket *SwAkePacket::createMovePacket(uint8_t subfn, bool isResponse,
					   uint8_t akeLabel, uint8_t kxmLabel,
					   uint8_t akeProcedure, uint8_t status,
					   const void *body, uint32_t bodyLen)
{
	uint8_t ctype = isResponse ? 0x09 : 0x00;
	SwAkePacket *pkt = new (std::nothrow) SwAkePacket(
		swClassifyAkePacket(ctype, subfn),
		(uint16_t)(8u + bodyLen), 0x01, ctype, subfn,
		akeLabel, kxmLabel, 0xFF, 0x0F, 0x0F, akeProcedure, status, 0, 1);
	if (pkt && body && bodyLen > 0)
		pkt->setPayload(body, bodyLen);
	return pkt;
}

SwAkePacket *SwAkePacket::createStatus(bool isResponse, uint8_t akeLabel,
				       uint8_t akeProcedure, uint8_t status)
{
	uint8_t ctype = isResponse ? 0x09 : 0x01;
	return new (std::nothrow) SwAkePacket(
		isResponse ? SW_AKE_PKT_AKE_STATUS_RSP : SW_AKE_PKT_AKE_STATUS_CMD,
		8, 0x01, ctype, SW_AKE_SUBFN_STATUS,
		akeLabel, 0xFF, 0xFF, 0x0F, 0x0F, akeProcedure, status, 0, 1);
}

int SwAkePacket::parseFrame(SwAkePacket **outPkt, const uint8_t *buf, unsigned int len)
{
	if (!outPkt)
		return 5;
	*outPkt = NULL;
	if (!buf || len < 4)
		return 5;

	uint16_t payloadLenBe = 0;
	memcpy(&payloadLenBe, buf + 1, 2);
	uint16_t payloadLen = swSwap16(payloadLenBe);
	if ((unsigned int)payloadLen + 3u != len)
		return 1006;
	if (payloadLen == 0)
		return 1006;

	uint8_t akeProc = (uint8_t)(buf[3] >> 4);
	uint8_t status = (uint8_t)(buf[3] & 0x0Fu);
	if (len < 11) {
		if (akeProc == 0 && status == 1)
			return 1006;
		return 1;
	}

	uint8_t ctype = buf[5];
	uint8_t subfn = buf[6];
	uint64_t typeMask = swClassifyAkePacket(ctype, subfn);
	SwAkePacket *pkt = new (std::nothrow) SwAkePacket(
		typeMask, payloadLen, buf[0], ctype, subfn,
		buf[7], buf[8], buf[9], (uint8_t)(buf[10] >> 4), (uint8_t)(buf[10] & 0x0Fu),
		akeProc, status, (uint8_t)(buf[4] >> 4), (uint8_t)(buf[4] & 0x0Fu));
	if (!pkt)
		return 2;
	int err = pkt->parse(buf, len);
	if (err != 0) {
		delete pkt;
		return err;
	}
	*outPkt = pkt;
	return 0;
}
