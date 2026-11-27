/*
 * DTCPIP-SW certificate, device ID, public key, and SRM data classes.
 */

#ifndef SW_CERT_SRM_H
#define SW_CERT_SRM_H

#include "../osal/sw_osal_cpp.h"
#include "../sdi/sw_sdi.h"

/* ---- SwDeviceId (5-byte DTCP Device ID, obfuscated in memory with XOR 0x13) ---- */
class SwDeviceId {
public:
	SwDeviceId();
	SwDeviceId(const SwDeviceId &other);
	virtual ~SwDeviceId();

	virtual bool equals(const SwDeviceId &other) const;

	int set(const uint8_t *raw5);
	int get(uint8_t *out5) const;
	int compare(uint8_t *outSame, const SwDeviceId &other) const;
	void toHexString(SwString &out) const;

	bool operator==(const SwDeviceId &other) const { return equals(other); }
	bool operator!=(const SwDeviceId &other) const { return !equals(other); }

private:
	static void xorCopy5(const uint8_t *src, uint8_t *dst);

	uint8_t m_masked[5];
	uint8_t m_reserved[3];
};

/* ---- SwPubKey (40-byte ECC public key) ---- */
class SwPubKey {
public:
	SwPubKey();
	virtual ~SwPubKey();

	int set(const void *raw40);
	int get(void *out40) const;
	bool isSet() const { return m_data != NULL; }

private:
	uint8_t *m_data;
};

/* ---- SwCert (88-byte baseline or 132-byte extended DTCP certificate) ---- */
class SwCert {
public:
	SwCert();
	virtual ~SwCert();

	int setRaw(const void *data, uint32_t len);
	int copyFrom(const SwCert &other);
	int getCategory(uint8_t *outCat) const;
	int getRaw(const void **outData, uint32_t *outLen,
		   const void **outSig = NULL, const void **outExtSig = NULL) const;
	int getPubKey(SwPubKey **outKey) const;
	int getDeviceIdObj(SwDeviceId **outId) const;
	int getApFlag(uint8_t *outFlag) const;
	int getKpFlag(uint8_t *outFlag) const;
	int validateFormat() const;
	uint32_t length() const { return m_len; }
	const uint8_t *raw() const { return m_raw; }

private:
	void clear();

	uint8_t *m_raw;		/* +0x04 */
	uint32_t m_len;		/* +0x08 */
	uint8_t m_certType;	/* +0x0C */
	uint8_t m_formatVer;	/* +0x0D */
	uint8_t m_category;	/* +0x0E */
	uint8_t m_reserved0f;	/* +0x0F */
	uint8_t m_reserved10;	/* +0x10 */
	uint8_t m_apFlag;	/* +0x11 */
	uint8_t m_kpFlag;	/* +0x12 */
	uint8_t m_reserved13;	/* +0x13 */
	SwDeviceId *m_deviceId;	/* +0x14 */
	SwPubKey *m_pubKey;	/* +0x18 */
	uint8_t *m_sig;		/* +0x1C */
	uint32_t m_extInfo;	/* +0x20 */
	uint8_t *m_extSig;	/* +0x24 */
};

/* ---- SwSrm (DTCP System Renewability Message) ---- */
class SwSrm {
public:
	SwSrm();
	virtual ~SwSrm();

	int setRaw(const void *data, size_t len, const SwCert *deviceCert = NULL);
	int copyFrom(const SwSrm &other);
	int getRaw(void **outPtr, size_t *outLen) const;
	size_t length() const { return m_len; }
	uint16_t version() const { return m_version; }
	uint8_t generation() const { return m_generation; }

	int compareForExchange(int *outAction, void **outSendPtr, uint32_t *outSendLen,
			       uint8_t *outRequestGen, uint16_t peerVer,
			       uint8_t peerMaxGen, uint8_t peerGen, uint8_t localMaxGen) const;
	int isDeviceRevoked(uint8_t *outRevoked, const SwDeviceId &devId) const;
	int isDeviceRevokedRaw(uint8_t *outRevoked, uint8_t highByte, uint32_t lowWord) const;

private:
	static int parseGenerations(uint8_t *outGen, const uint8_t *data, size_t len);
	static int getMaxSrmSize(uint32_t *outSize, uint8_t certType);
	void clear() {
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

	uint8_t *m_raw;		/* +0x04 */
	size_t m_len;		/* +0x08 */
	uint8_t m_srmType;	/* +0x0C */
	uint8_t m_reserved0d;	/* +0x0D */
	uint8_t m_genFlag;	/* +0x0E */
	uint8_t m_reserved0f;	/* +0x0F */
	uint16_t m_version;	/* +0x10 */
	uint8_t m_generation;	/* +0x12 */
	uint8_t m_reserved13;	/* +0x13 */
	uint8_t *m_sig;		/* +0x14 */
};

/* ---- Global buffer factory access ---- */
SdiBufferFactoryBase *swGetBufferFactory();

#endif /* SW_CERT_SRM_H */
