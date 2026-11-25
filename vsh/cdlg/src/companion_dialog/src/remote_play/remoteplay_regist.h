/*
 * SceCompanionDialogPlugin: RemotePlay device registration protocol,
 * SceRemotePlaySaSocket, and AES-128-CFB / HMAC-SHA256 cipher.
 */
#ifndef COMPANION_REMOTEPLAY_REGIST_H
#define COMPANION_REMOTEPLAY_REGIST_H

#include <scetypes.h>
#include "remoteplay_db.h"

struct RemotePlaySocket {
	SceInt32 handle;
};

struct RemotePlaySession {
	RemotePlaySocket sock;
	uint8_t clientMode;
	uint8_t aborted;
	uint8_t pad[2];
};

class RemotePlayCipherBackend {
public:
	RemotePlayCipherBackend();
	virtual ~RemotePlayCipherBackend();

	void CryptCfb(const uint8_t *inBuf, uint8_t *outBuf, SceInt32 length,
		const uint8_t *key, uint8_t *iv, SceInt32 *ivOffset, SceInt32 encrypt);
	void EncryptInPlace(SceInt32 mode, uint8_t *buf, SceInt32 length,
		const uint8_t *key, const uint8_t *iv);
	void DecryptInPlace(SceInt32 mode, uint8_t *buf, SceInt32 length,
		const uint8_t *key, const uint8_t *iv);
	void SeedRng();
	SceInt32 NextRandom();
};

class RemotePlayCipher {
public:
	RemotePlayCipher();
	virtual ~RemotePlayCipher();

	SceInt32 DeriveIv(uint8_t *outIv, const uint8_t *key, SceInt32 keyLen,
		const uint8_t *seed, SceSize seedLen, SceUInt64 counter);
	void Encrypt(SceInt32 mode, uint8_t *buf, SceInt32 length,
		const uint8_t *key, const uint8_t *seed, SceUInt64 *counter);
	void Decrypt(SceInt32 mode, uint8_t *buf, SceInt32 length,
		const uint8_t *key, const uint8_t *seed, SceUInt64 *counter);
	void SeedRng();
	SceInt32 NextRandom();

private:
	RemotePlayCipherBackend *m_backend;
};

SceInt32 RemotePlayNetInit();
void RemotePlayNetTerm();
SceInt32 RemotePlayNetCheckCallback();
void RemotePlayNetSetKeepAlive(SceInt32 disableFlag);
void RemotePlayNetAbortGlobal();
SceInt32 RemotePlayGetClientName(char *buf, SceSize bufSize);

SceInt32 RemotePlaySessionInit(RemotePlaySession *session, uint8_t clientMode);
SceInt32 RemotePlaySessionRegister(RemotePlaySession *session, SceUInt32 pin,
	RemotePlayRecord *outRecord, uint8_t *outMac);
SceInt32 RemotePlaySessionAbort(RemotePlaySession *session);

#endif /* COMPANION_REMOTEPLAY_REGIST_H */
