/*
 * DTCPIP-SW AKE wire framing and DTCP-IP AKE command/response packet hierarchy.
 */

#ifndef SW_AKE_PACKET_H
#define SW_AKE_PACKET_H

#include "../osal/sw_osal_cpp.h"
#include "../common/sw_cert_srm.h"

/* ---- DTCP-IP AKE packet type bitmask flags (64-bit) ---- */
enum SwAkePacketTypeBits {
	SW_AKE_PKT_UNKNOWN              = 0,
	SW_AKE_PKT_CHALLENGE_CMD        = 1,
	SW_AKE_PKT_RESPONSE_CMD         = 2,
	SW_AKE_PKT_EXCHANGE_KEY_CMD     = 4,
	SW_AKE_PKT_SRM_CMD              = 8,
	SW_AKE_PKT_CONTENT_KEY_REQ_CMD  = 16,
	SW_AKE_PKT_RTT_READY_CMD        = 32,
	SW_AKE_PKT_AKE_STATUS_CMD       = 64,
	SW_AKE_PKT_RTT_SETUP_CMD        = 128,
	SW_AKE_PKT_RTT_TEST_CMD         = 256,
	SW_AKE_PKT_RTT_VERIFY_CMD       = 512,
	SW_AKE_PKT_RTT_VERIFY_READY_CMD = 1024,
	SW_AKE_PKT_CAPABILITY_EXCH_CMD  = 2048,
	SW_AKE_PKT_BG_SRM_CMD           = 4096,
	SW_AKE_PKT_MV_INIT_CMD          = 0x2000,
	SW_AKE_PKT_MV_FINALIZE_CMD      = 0x4000,
	SW_AKE_PKT_CHALLENGE_RSP        = 0x10000,
	SW_AKE_PKT_RESPONSE_RSP         = 0x20000,
	SW_AKE_PKT_EXCHANGE_KEY_RSP     = 0x40000,
	SW_AKE_PKT_SRM_RSP              = 0x80000,
	SW_AKE_PKT_CONTENT_KEY_REQ_RSP  = 0x100000,
	SW_AKE_PKT_RTT_READY_RSP        = 0x200000,
	SW_AKE_PKT_RTT_SETUP_RSP        = 0x400000,
	SW_AKE_PKT_AKE_STATUS_RSP       = 0x1000000,
	SW_AKE_PKT_RTT_TEST_RSP         = 0x2000000,
	SW_AKE_PKT_RTT_VERIFY_RSP       = 0x4000000,
	SW_AKE_PKT_RTT_VERIFY_READY_RSP = 0x8000000,
	SW_AKE_PKT_CAPABILITY_EXCH_RSP  = 0x10000000,
	SW_AKE_PKT_BG_SRM_RSP           = 0x20000000,
	SW_AKE_PKT_REJECT_RSP           = 0x40000000
};

/* ---- DTCP-IP AKE subfunction codes ---- */
enum SwAkeSubfunction {
	SW_AKE_SUBFN_CHALLENGE         = 0x01,
	SW_AKE_SUBFN_RESPONSE          = 0x02,
	SW_AKE_SUBFN_EXCHANGE_KEY      = 0x03,
	SW_AKE_SUBFN_SRM               = 0x04,
	SW_AKE_SUBFN_CONTENT_KEY_REQ   = 0x05,
	SW_AKE_SUBFN_CAPABILITY_EXCH   = 0x06,
	SW_AKE_SUBFN_RTT_READY         = 0x40,
	SW_AKE_SUBFN_RTT_SETUP         = 0x41,
	SW_AKE_SUBFN_RTT_TEST          = 0x42,
	SW_AKE_SUBFN_RTT_VERIFY        = 0x43,
	SW_AKE_SUBFN_RTT_VERIFY_READY  = 0x44,
	SW_AKE_SUBFN_MV_INIT           = 0x48,
	SW_AKE_SUBFN_MV_FINALIZE       = 0x49,
	SW_AKE_SUBFN_MV_COMPLETE       = 0x4A,
	SW_AKE_SUBFN_MV_CANCEL         = 0x4B,
	SW_AKE_SUBFN_AKE_CANCEL        = 0x80,
	SW_AKE_SUBFN_STATUS            = 0x81
};

/* ---- SwAkeFrameBase (3-byte DTCP-IP transport header: type + 16-bit big-endian length) ---- */
class SwAkeFrameBase {
public:
	SwAkeFrameBase(uint32_t frameKind, uint16_t payloadLen, uint8_t frameType,
		       uint8_t akeProcedure, uint8_t statusCode);
	virtual ~SwAkeFrameBase();

	virtual int serialize(uint8_t **outBuf, uint32_t *outLen);
	virtual int build();
	virtual int parse(const uint8_t *buf, uint32_t len);
	virtual int getFrameHeader(void *out4) const;
	virtual uint16_t payloadLength() const { return m_payloadLen; }
	virtual uint8_t frameType() const { return m_frameType; }
	virtual uint8_t akeProcedure() const { return m_akeProcedure; }
	virtual uint8_t statusCode() const { return m_statusCode; }
	virtual bool isAkeFrame() const { return m_frameKind == 0; }
	virtual bool isControlFrame() const { return m_frameKind == 1; }

	const uint8_t *wireData() const { return m_wireBuf; }
	uint32_t wireLength() const { return m_wireLen; }

protected:
	int buildFrameHeader();
	int parseFrameHeader(const uint8_t *buf, uint32_t len);
	void resetWire();

	uint32_t m_frameKind;		/* +0x04 */
	uint16_t m_payloadLen;		/* +0x08 */
	uint8_t m_frameType;		/* +0x0A */
	uint8_t m_akeProcedure;		/* +0x0B */
	uint8_t m_statusCode;		/* +0x0C */
	uint8_t m_reserved0d[3];	/* +0x0D */
	uint8_t *m_hdr4;		/* +0x10 */
	uint8_t *m_wireBuf;		/* +0x14 */
	uint32_t m_wireLen;		/* +0x18 */
};

/* ---- SwAkePacket (7-byte AKE command/response header + optional payload) ---- */
class SwAkePacket : public SwAkeFrameBase {
public:
	SwAkePacket(uint64_t typeMask, uint16_t payloadLen, uint8_t frameType,
		    uint8_t ctypeOrResponse, uint8_t subfunction, uint8_t akeLabel,
		    uint8_t exchKey, uint8_t akeInfoLen, uint8_t exchKeyType,
		    uint8_t subfnParam, uint8_t akeProcedure, uint8_t statusCode,
		    uint8_t versionHi, uint8_t versionLo);
	virtual ~SwAkePacket();

	virtual int build();
	virtual int parse(const uint8_t *buf, uint32_t len);
	virtual int getAkeHeader(void *out7) const;

	uint64_t typeMask() const { return m_typeMask; }
	uint8_t versionHi() const { return m_versionHi; }
	uint8_t versionLo() const { return m_versionLo; }
	uint8_t ctype() const { return m_ctype; }
	uint8_t subfunction() const { return m_subfunction; }
	uint8_t akeLabel() const { return m_akeLabel; }
	uint8_t exchangeKeyByte() const { return m_exchKey; }
	uint8_t akeInfoFlag() const { return m_akeInfoFlag; }
	uint8_t exchKeyType() const { return m_exchKeyType; }
	uint8_t subfnParam() const { return m_subfnParam; }

	bool isCommand() const;
	bool isResponse() const;
	bool isStatusCommand() const { return m_typeMask == SW_AKE_PKT_AKE_STATUS_CMD; }
	bool isStatusResponse() const { return m_typeMask == SW_AKE_PKT_AKE_STATUS_RSP; }
	bool isRejectResponse() const { return m_typeMask == SW_AKE_PKT_REJECT_RSP; }

	int setPayload(const void *data, uint32_t len);
	const uint8_t *payload() const { return m_payload; }
	uint32_t payloadSize() const { return m_payloadSize; }

	/* Convenience builders for standard DTCP-IP AKE packets */
	static SwAkePacket *createChallenge(bool isResponse, uint8_t akeLabel,
					    uint8_t akeProcedure, uint8_t status,
					    const uint8_t nonce8[8],
					    const void *certData, uint32_t certLen);
	static SwAkePacket *createResponse(bool isResponse, uint8_t akeLabel,
					   uint8_t akeProcedure, uint8_t status,
					   const uint8_t pub20[20], const uint8_t sig40[40]);
	static SwAkePacket *createExchangeKey(bool isResponse, uint8_t akeLabel,
					      uint8_t exchKeyLabel, uint8_t akeProcedure,
					      uint8_t status, const uint8_t kx12[12],
					      uint8_t keyFlags);
	static SwAkePacket *createSrm(bool isResponse, uint8_t akeLabel,
				      uint8_t akeProcedure, uint8_t status,
				      const void *srmData, uint32_t srmLen);
	static SwAkePacket *createRtt(uint8_t subfn, bool isResponse, uint8_t akeLabel,
				      uint8_t akeProcedure, uint8_t status,
				      const void *body, uint32_t bodyLen);
	static SwAkePacket *createContentKeyReq(bool isResponse, uint8_t akeLabel,
						uint8_t akeProcedure, uint8_t status,
						const uint8_t nonce8[8]);
	static SwAkePacket *createMovePacket(uint8_t subfn, bool isResponse,
					     uint8_t akeLabel, uint8_t kxmLabel,
					     uint8_t akeProcedure, uint8_t status,
					     const void *body, uint32_t bodyLen);
	static SwAkePacket *createStatus(bool isResponse, uint8_t akeLabel,
					 uint8_t akeProcedure, uint8_t status);

	/* Factory: parse a complete framed DTCP-IP AKE packet from wire bytes */
	static int parseFrame(SwAkePacket **outPkt, const uint8_t *buf, unsigned int len);

protected:
	int buildAkeHeader();
	int parseAkeHeader(const uint8_t *buf, uint32_t len);

	uint64_t m_typeMask;
	uint8_t m_versionHi;
	uint8_t m_versionLo;
	uint8_t m_ctype;
	uint8_t m_subfunction;
	uint8_t m_akeLabel;
	uint8_t m_exchKey;
	uint8_t m_akeInfoFlag;
	uint8_t m_exchKeyType;
	uint8_t m_subfnParam;
	uint8_t *m_akeHdr7;
	uint8_t *m_payload;
	uint32_t m_payloadSize;
};

#endif /* SW_AKE_PACKET_H */
