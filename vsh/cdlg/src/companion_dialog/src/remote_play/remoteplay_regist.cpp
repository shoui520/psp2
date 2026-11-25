/*
 * SceCompanionDialogPlugin: RemotePlay device registration, socket wrapper,
 * and AES-128-CFB / HMAC-SHA256 cipher implementation.
 */
#include "remoteplay_regist.h"
#include <libaes.h>
#include <libhmac.h>
#include <kernel/threadmgr.h>

extern "C" {
void *sce_paf_memcpy(void *dst, const void *src, SceSize size);
void *sce_paf_memset(void *dst, int c, SceSize size);
int sce_paf_memcmp(const void *lhs, const void *rhs, SceSize n);
int sce_paf_strncmp(const char *lhs, const char *rhs, SceSize n);
SceSize sce_paf_strlen(const char *s);
char *sce_paf_strncpy(char *dst, const char *src, SceSize n);
int sce_paf_snprintf(char *buf, SceSize size, const char *fmt, ...);
long sce_paf_strtol(const char *nptr, char **endptr, int base);
unsigned int sce_paf_look_ctype_table(unsigned int ch);
void sce_paf_srand(unsigned int seed);
int sce_paf_rand(void);
void *sceClibMemcpy_safe(void *dst, const void *src, SceSize size);
SceInt32 sceRtcGetCurrentTick(SceUInt64 *tick);
SceInt32 sceAppUtilSystemParamGetString(SceUInt32 paramId, char *buf, SceSize bufSize);
SceInt32 sceAppUtilShutdown(void);
SceInt32 sceNpManagerGetCachedParam(void *param);
SceInt32 sceNetSocket(const char *name, int domain, int type, int protocol);
SceInt32 sceNetSocketClose(int s);
SceInt32 sceNetSocketAbort(int s, int flags);
SceInt32 sceNetConnect(int s, const void *name, unsigned int namelen);
SceInt32 sceNetSend(int s, const void *msg, unsigned int len, int flags);
SceInt32 sceNetSendto(int s, const void *msg, unsigned int len, int flags, const void *to, unsigned int tolen);
SceInt32 sceNetRecv(int s, void *buf, unsigned int len, int flags);
SceInt32 sceNetRecvfrom(int s, void *buf, unsigned int len, int flags, void *from, unsigned int *fromlen);
SceInt32 sceNetSetsockopt(int s, int level, int optname, const void *optval, unsigned int optlen);
SceUInt16 sceNetHtons(SceUInt16 host16);
SceUInt32 sceNetHtonl(SceUInt32 host32);
SceInt32 sceNetInetPton(int af, const char *src, void *dst);
SceInt32 sceNetTerm(void);
SceInt32 sceNetCtlTerm(void);
SceInt32 sceNetCtlCheckCallback(void);
SceInt32 SceNetCtl_AC326DB7(SceInt32 (*cb)(SceInt32, void *), void *arg, SceInt32 *cid);
SceInt32 SceNetCtl_229EAA64(SceInt32 cid);
SceInt32 SceNetCtl_C33D7374(SceInt32 code, void *info);
SceInt32 SceNetCtl_300E8877(SceInt32 eventType);
SceInt32 SceNetInternal_235DE96C(SceInt32 index, void *ifInfo, SceInt32 count);
SceInt32 SceNetInternal_DE8DD55E(SceUInt32 *flags, SceInt32 zero);
SceInt32 SceNetInternal_7493FB0C(SceUInt32 flags);
SceUInt32 CompanionGetTargetIpAddr();
}

struct NetSockAddrIn {
	uint8_t sin_len;
	uint8_t sin_family;
	SceUInt16 sin_port;
	SceUInt32 sin_addr;
	uint8_t sin_zero[8];
};

struct NetIfInfo {
	uint8_t header[32];
	SceUInt32 ipAddrHost;
	uint8_t middle[44];
	uint8_t macAddr[6];
	uint8_t tail[234];
};

struct RemotePlayCryptoContext {
	SceInt32 mode;
	uint8_t key[16];
	uint8_t nonce[16];
	uint8_t iv[16];
};

struct ClientProfile {
	uint8_t reserved[8];
	const char *clientType;
};

static const uint8_t kPinKeyBase[16] = {
	0x5b, 0x64, 0x40, 0xc5, 0x2e, 0x74, 0xc0, 0x46,
	0x48, 0x72, 0xc9, 0xc5, 0x49, 0x0c, 0x79, 0x04
};

static const uint8_t kIvMask[16] = {
	0xec, 0x6d, 0x70, 0x6b, 0x1e, 0x0a, 0x9a, 0x75,
	0x8c, 0xda, 0x78, 0x27, 0x51, 0xa3, 0xc3, 0x7b
};

static const uint8_t kHmacKey[16] = {
	0xac, 0x07, 0x88, 0x83, 0xc8, 0x3a, 0x1f, 0xe8,
	0x11, 0x46, 0x3a, 0xf3, 0x9e, 0xe3, 0xe3, 0x77
};

static RemotePlayCryptoContext *g_cryptoCtx = 0;
static SceUInt64 g_encryptCounter = 0;
static SceUInt64 g_decryptCounter = 0;
static uint8_t g_netInitFlags = 0;
static SceUInt32 g_netEventFlags = 0;
static SceInt32 g_netCtlCallbackId = 0;
static SceInt32 g_globalSocketHandle = 0;

static const SceUInt16 kRemotePlayRegistPort = 9295;
static const SceInt32 kErrRegistParseFailed = (SceInt32)0x8010D9C1;
static const SceInt32 kErrRegistAborted = (SceInt32)0x8010D9C5;
static const SceInt32 kErrRegistNotFound = (SceInt32)0x8010D9C7;
static const SceInt32 kErrRegistRefused = (SceInt32)0x8010D913;
static const SceInt32 kErrRegistInvalidResp = (SceInt32)0x80115003;
static const SceInt32 kErrRegistTimeout = (SceInt32)0x80115008;

static void FillClientProfile(ClientProfile *profile, uint8_t clientMode)
{
	sce_paf_memset(profile, 0, sizeof(*profile));
	if (clientMode == 2)
		profile->clientType = "VITA";
	else
		profile->clientType = "PC";
}

static int HexDigitValue(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	return -1;
}

static SceSize DecodeHexBytes(uint8_t *dst, const char *src, SceInt32 maxBytes)
{
	const char *cursor = src;
	for (SceInt32 idx = 0; idx < maxBytes; ++idx) {
		int hi = HexDigitValue(cursor[0]);
		if (hi < 0)
			return (SceSize)(cursor - src);
		int lo = HexDigitValue(cursor[1]);
		if (lo < 0)
			return (SceSize)((cursor + 1) - src);
		dst[idx] = (uint8_t)((hi << 4) | lo);
		cursor += 2;
	}
	return (SceSize)(cursor - src);
}

static SceSize CopyPrintableField(char *dst, const char *src, SceInt32 maxLen)
{
	SceInt32 copied = 0;
	while (copied < maxLen && (unsigned char)src[copied] >= 0x20) {
		dst[copied] = src[copied];
		++copied;
	}
	dst[copied] = '\0';
	return (SceSize)copied;
}

static SceInt32 ParseRegistResponse(const char *body, SceInt32 bodyLen, RemotePlayRecord *dst)
{
	const char *cursor = body;
	const char *end = body + bodyLen;
	SceUInt32 apMask = 0;
	SceUInt32 ps4Mask = 0;

	sce_paf_memset(dst, 0, sizeof(*dst));
	if (cursor >= end)
		return kErrRegistParseFailed;

	while (cursor < end) {
		if (sce_paf_strncmp("AP-Ssid: ", cursor, 9) == 0) {
			cursor += DecodeHexBytes((uint8_t *)dst->apSsid, cursor + 9, 33);
			apMask |= 0x01;
		} else if (sce_paf_strncmp("AP-Bssid: ", cursor, 10) == 0) {
			cursor += DecodeHexBytes((uint8_t *)dst->apBssid, cursor + 10, 33);
			apMask |= 0x02;
		} else if (sce_paf_strncmp("AP-Key: ", cursor, 8) == 0) {
			cursor += DecodeHexBytes((uint8_t *)dst->apKey, cursor + 8, 64);
			apMask |= 0x04;
		} else if (sce_paf_strncmp("AP-Name: ", cursor, 9) == 0) {
			cursor += CopyPrintableField(dst->apName, cursor + 9, 61);
			apMask |= 0x08;
		} else if (sce_paf_strncmp("PS4-Mac: ", cursor, 9) == 0) {
			cursor += CopyPrintableField(dst->ps4Mac, cursor + 9, 12);
			ps4Mask |= 0x10;
		} else if (sce_paf_strncmp("PS4-RegistKey: ", cursor, 15) == 0) {
			cursor += DecodeHexBytes((uint8_t *)dst->ps4RegistKey, cursor + 15, 8);
			ps4Mask |= 0x20;
		} else if (sce_paf_strncmp("PS4-Nickname: ", cursor, 14) == 0) {
			cursor += CopyPrintableField(dst->ps4Nickname, cursor + 14, 125);
			ps4Mask |= 0x40;
		} else if (sce_paf_strncmp("RP-KeyType: ", cursor, 12) == 0) {
			long keyType = sce_paf_strtol(cursor + 12, 0, 10);
			if (keyType >= 0) {
				dst->rpKeyType = (uint8_t)keyType;
				ps4Mask |= 0x80;
			}
		} else if (sce_paf_strncmp("RP-Key: ", cursor, 8) == 0) {
			cursor += DecodeHexBytes(dst->rpKey, cursor + 8, 16);
			ps4Mask |= 0x100;
		}

		while (cursor < end && *cursor != '\0' && *cursor != '\n')
			++cursor;
		if (cursor < end)
			++cursor;
	}

	if ((apMask == 0 || apMask == 0x0F) && ps4Mask == 0x1F0)
		return 0;
	return kErrRegistParseFailed;
}

static void CryptoSessionInit(SceUInt32 pin)
{
	if (g_cryptoCtx != 0) {
		delete g_cryptoCtx;
		g_cryptoCtx = 0;
		return;
	}

	g_cryptoCtx = new RemotePlayCryptoContext();
	if (g_cryptoCtx == 0)
		return;
	sce_paf_memset(g_cryptoCtx, 0, sizeof(*g_cryptoCtx));

	uint8_t pinBytes[16];
	sce_paf_memset(pinBytes, 0, sizeof(pinBytes));
	pinBytes[4] = (uint8_t)((pin >> 24) & 0xFF);
	pinBytes[5] = (uint8_t)((pin >> 16) & 0xFF);
	pinBytes[6] = (uint8_t)((pin >> 8) & 0xFF);
	pinBytes[7] = (uint8_t)(pin & 0xFF);

	uint8_t derivedKey[16];
	uint8_t rawNonce[16];
	uint8_t maskedIv[16];
	sce_paf_memset(derivedKey, 0, sizeof(derivedKey));
	sce_paf_memset(rawNonce, 0, sizeof(rawNonce));
	sce_paf_memset(maskedIv, 0, sizeof(maskedIv));

	RemotePlayCipher cipher;
	cipher.SeedRng();
	SceUInt32 *nonceWords = (SceUInt32 *)rawNonce;
	for (int idx = 0; idx < 4; ++idx)
		nonceWords[idx] = (SceUInt32)cipher.NextRandom();

	sce_paf_memcpy(maskedIv, rawNonce, sizeof(maskedIv));
	for (int idx = 0; idx < 16; ++idx) {
		uint8_t shifted = (uint8_t)(maskedIv[idx] + idx + 43);
		maskedIv[idx] = (uint8_t)(shifted ^ kIvMask[idx]);
	}

	sce_paf_memcpy(derivedKey, kPinKeyBase, sizeof(derivedKey));
	for (int idx = 0; idx < 8; ++idx)
		derivedKey[idx] ^= pinBytes[idx];

	sce_paf_memcpy(g_cryptoCtx->key, derivedKey, sizeof(derivedKey));
	sce_paf_memcpy(g_cryptoCtx->nonce, rawNonce, sizeof(rawNonce));
	sce_paf_memcpy(g_cryptoCtx->iv, maskedIv, sizeof(maskedIv));
	g_encryptCounter = 0;
	g_decryptCounter = 0;
}

static void CryptoFillRandomAscii(uint8_t *buf, SceSize len)
{
	if (g_cryptoCtx == 0 || buf == 0)
		return;
	RemotePlayCipher cipher;
	cipher.SeedRng();
	SceSize filled = 0;
	while (filled < len) {
		uint8_t candidate = (uint8_t)cipher.NextRandom();
		if ((sce_paf_look_ctype_table(candidate) & 0x17) != 0) {
			buf[filled] = candidate;
			++filled;
		}
	}
}

static void CryptoEncryptPayload(uint8_t *buf, SceInt32 len, uint8_t *outIv, SceInt32 ivLen)
{
	if (g_cryptoCtx == 0 || buf == 0 || ivLen != 16)
		return;
	RemotePlayCipher cipher;
	cipher.Encrypt(g_cryptoCtx->mode, buf, len, g_cryptoCtx->key, g_cryptoCtx->nonce, &g_encryptCounter);
	sce_paf_memcpy(outIv, g_cryptoCtx->iv, 16);
}

static void CryptoDecryptPayload(uint8_t *buf, SceInt32 len)
{
	if (g_cryptoCtx == 0 || buf == 0)
		return;
	RemotePlayCipher cipher;
	cipher.Decrypt(g_cryptoCtx->mode, buf, len, g_cryptoCtx->key, g_cryptoCtx->nonce, &g_decryptCounter);
}

static void CryptoSessionTerm()
{
	if (g_cryptoCtx != 0) {
		delete g_cryptoCtx;
		g_cryptoCtx = 0;
	}
}

static SceInt32 SocketOpen(RemotePlaySocket *sock, SceInt32 protoMode)
{
	int type = (protoMode == 0) ? 1 : 2;
	SceInt32 handle = sceNetSocket("SceRemotePlaySaSocket", 2, type, 0);
	sock->handle = handle;
	if (handle < 0)
		return handle;

	if (protoMode == 2) {
		int optval = 1;
		SceInt32 ret = sceNetSetsockopt(handle, 0xFFFF, 32, &optval, sizeof(optval));
		if (ret < 0)
			return ret;
		ret = sceNetSetsockopt(sock->handle, 0, 10, &optval, sizeof(optval));
		if (ret < 0)
			return ret;
		ret = sceNetSetsockopt(sock->handle, 0xFFFF, 2048, &optval, sizeof(optval));
		if (ret < 0)
			return ret;
		ret = sceNetSetsockopt(sock->handle, 0xFFFF, 0x10000, &optval, sizeof(optval));
		if (ret < 0)
			return ret;

		NetIfInfo ifInfo;
		SceNetInternal_235DE96C(4, &ifInfo, 1);
		SceUInt32 mcastIf = sceNetHtonl(ifInfo.ipAddrHost);
		sceNetSetsockopt(sock->handle, 0, 9, &mcastIf, sizeof(mcastIf));
	}
	return 0;
}

static SceInt32 SocketClose(RemotePlaySocket *sock)
{
	if (sock->handle > 0)
		sceNetSocketClose(sock->handle);
	sock->handle = 0;
	return 0;
}

static SceInt32 SocketAbort(RemotePlaySocket *sock)
{
	if (sock->handle > 0)
		sceNetSocketAbort(sock->handle, 3);
	return 0;
}

static SceInt32 SocketConnect(RemotePlaySocket *sock, const NetSockAddrIn *addr, SceUInt32 addrLen)
{
	SceInt32 ret = sceNetConnect(sock->handle, addr, addrLen);
	switch (ret) {
	case (SceInt32)0x8041013C:
		return kErrRegistTimeout;
	case (SceInt32)0x8041013D:
		return kErrRegistRefused;
	case (SceInt32)0x804101A1:
		return (SceInt32)0x804101A1;
	default:
		return ret;
	}
}

static SceInt32 SocketSend(RemotePlaySocket *sock, const void *buf, SceUInt32 len, SceInt32 *outSent)
{
	SceInt32 sent = sceNetSend(sock->handle, buf, len, 0);
	SceInt32 ret = 0;
	if (sent < 0) {
		ret = sent;
		sent = 0;
	}
	if (outSent != 0)
		*outSent = sent;
	return ret;
}

static SceInt32 SocketSendTo(RemotePlaySocket *sock, const void *buf, SceUInt32 len,
	SceInt32 *outSent, const NetSockAddrIn *to, SceUInt32 toLen)
{
	SceInt32 sent = sceNetSendto(sock->handle, buf, len, 0, to, toLen);
	if (sent < 0)
		return sent;
	if (outSent != 0)
		*outSent = sent;
	return 0;
}

static SceInt32 SocketRecv(RemotePlaySocket *sock, void *buf, SceUInt32 len, SceInt32 *outRecv)
{
	SceInt32 recvd = sceNetRecv(sock->handle, buf, len, 0);
	SceInt32 ret = 0;
	if (recvd < 0) {
		if (recvd == (SceInt32)0x8041013C)
			recvd = kErrRegistTimeout;
		ret = recvd;
		recvd = 0;
	}
	if (outRecv != 0)
		*outRecv = recvd;
	return ret;
}

static SceInt32 SocketRecvFrom(RemotePlaySocket *sock, void *buf, SceUInt32 len,
	SceInt32 *outRecv, NetSockAddrIn *from, SceUInt32 *fromLen)
{
	SceInt32 recvd = sceNetRecvfrom(sock->handle, buf, len, 0, from, fromLen);
	SceInt32 ret = 0;
	if (recvd < 0) {
		if (recvd == (SceInt32)0x8041013C)
			recvd = kErrRegistTimeout;
		ret = recvd;
		recvd = 0;
	}
	if (outRecv != 0)
		*outRecv = recvd;
	return ret;
}

static SceInt32 SocketSetSendTimeout(RemotePlaySocket *sock, SceInt32 seconds)
{
	SceInt32 timeoutUs = 1000000 * seconds;
	return sceNetSetsockopt(sock->handle, 0xFFFF, 4101, &timeoutUs, sizeof(timeoutUs));
}

static SceInt32 SocketSetRecvTimeout(RemotePlaySocket *sock, SceInt32 seconds)
{
	SceInt32 timeoutUs = 1000000 * seconds;
	return sceNetSetsockopt(sock->handle, 0xFFFF, 4102, &timeoutUs, sizeof(timeoutUs));
}

static SceInt32 GetLocalInterfaceAddresses(SceInt32 *inOutCount, SceUInt32 *outIp, SceUInt32 *outMask)
{
	SceInt32 found = 0;
	SceInt32 ret = 0;
	if (*inOutCount > 0) {
		char addrStr[256];
		SceUInt32 addrVal = 0;
		ret = SceNetCtl_C33D7374(15, addrStr);
		if (ret >= 0) {
			sceNetInetPton(2, addrStr, &addrVal);
			*outIp = addrVal;
			ret = SceNetCtl_C33D7374(16, addrStr);
			if (ret >= 0) {
				sceNetInetPton(2, addrStr, &addrVal);
				*outMask = addrVal;
				found = 1;
			}
		}
	}
	*inOutCount = found;
	return ret;
}

static void GetLocalMacAddress(uint8_t *outMac)
{
	NetIfInfo ifInfo;
	if (SceNetInternal_235DE96C(4, &ifInfo, 1) > 0 || SceNetInternal_235DE96C(1, &ifInfo, 1) > 0)
		sce_paf_memcpy(outMac, ifInfo.macAddr, 6);
}

static void GetLocalDeviceId(uint8_t *outId)
{
	NetIfInfo ifInfo;
	sce_paf_memset(outId, 1, 16);
	if (SceNetInternal_235DE96C(4, &ifInfo, 1) > 0 || SceNetInternal_235DE96C(1, &ifInfo, 1) > 0)
		sce_paf_memcpy(outId, ifInfo.macAddr, 6);
}

SceInt32 RemotePlayGetClientName(char *buf, SceSize bufSize)
{
	SceInt32 ret = sceAppUtilSystemParamGetString(3, buf, bufSize);
	if (ret >= 0) {
		SceSize len = sce_paf_strlen(buf);
		if (len + 11 < bufSize)
			sce_paf_strncpy(buf + len, " (PS Vita)", bufSize - len);
	}
	return ret;
}

void RemotePlayNetAbortGlobal()
{
	if (g_globalSocketHandle > 0)
		sceNetSocketAbort(g_globalSocketHandle, 3);
	g_netEventFlags |= 0x10;
}

void RemotePlayNetSetKeepAlive(SceInt32 disableFlag)
{
	SceUInt32 flags = 0;
	SceNetInternal_DE8DD55E(&flags, 0);
	if (disableFlag == 0)
		flags |= 8;
	SceNetInternal_7493FB0C(flags);
}

static SceInt32 OnNetCtlEvent(SceInt32 eventType, void *arg)
{
	(void)arg;
	switch (eventType) {
	case 4:
		g_netEventFlags |= 0x01;
		break;
	case 5:
		g_netEventFlags |= 0x02;
		break;
	case 6:
		g_netEventFlags |= 0x04;
		break;
	case 1:
		g_netEventFlags |= 0x08;
		break;
	case 2:
		g_netEventFlags |= 0x20;
		break;
	default:
		break;
	}
	return SceNetCtl_300E8877(eventType);
}

void RemotePlayNetTerm()
{
	if ((g_netInitFlags & 0x10) != 0)
		sceAppUtilShutdown();
	if ((g_netInitFlags & 0x08) != 0)
		SceNetCtl_229EAA64(g_netCtlCallbackId);
	if ((g_netInitFlags & 0x04) != 0)
		sceNetCtlTerm();
	if ((g_netInitFlags & 0x02) != 0)
		sceNetTerm();
	g_netInitFlags = 0;
}

SceInt32 RemotePlayNetInit()
{
	g_netInitFlags = 0;
	g_globalSocketHandle = 0;
	SceInt32 ret = SceNetCtl_AC326DB7(OnNetCtlEvent, 0, &g_netCtlCallbackId);
	if (ret < 0)
		RemotePlayNetTerm();
	else
		g_netInitFlags |= 0x08;
	return ret;
}

SceInt32 RemotePlayNetCheckCallback()
{
	return sceNetCtlCheckCallback();
}

RemotePlayCipherBackend::RemotePlayCipherBackend()
{
}

RemotePlayCipherBackend::~RemotePlayCipherBackend()
{
}

void RemotePlayCipherBackend::CryptCfb(const uint8_t *inBuf, uint8_t *outBuf, SceInt32 length,
	const uint8_t *key, uint8_t *iv, SceInt32 *ivOffset, SceInt32 encrypt)
{
	SceAesContext aesCtx;
	uint8_t keystream[32];
	SceInt32 pos = *ivOffset;

	if (sceAesScheduleKey(&aesCtx, 128, 128, key) < 0)
		return;

	for (SceInt32 idx = 0; idx < length; ++idx) {
		if (pos == 0) {
			if (sceAesEncrypt(&aesCtx, iv, keystream) < 0)
				return;
			sceClibMemcpy_safe(iv, keystream, 16);
		}
		uint8_t inByte = inBuf[idx];
		if (encrypt != 0) {
			uint8_t cipherByte = (uint8_t)(inByte ^ iv[pos]);
			outBuf[idx] = cipherByte;
			iv[pos] = cipherByte;
		} else {
			outBuf[idx] = (uint8_t)(iv[pos] ^ inByte);
			iv[pos] = inByte;
		}
		pos = (pos + 1) & 0xF;
	}
	*ivOffset = pos;
}

void RemotePlayCipherBackend::EncryptInPlace(SceInt32 mode, uint8_t *buf, SceInt32 length,
	const uint8_t *key, const uint8_t *iv)
{
	(void)mode;
	SceInt32 ivOffset = 0;
	uint8_t ivCopy[16];
	sceClibMemcpy_safe(ivCopy, iv, sizeof(ivCopy));
	CryptCfb(buf, buf, length, key, ivCopy, &ivOffset, 1);
}

void RemotePlayCipherBackend::DecryptInPlace(SceInt32 mode, uint8_t *buf, SceInt32 length,
	const uint8_t *key, const uint8_t *iv)
{
	(void)mode;
	SceInt32 ivOffset = 0;
	uint8_t ivCopy[16];
	sceClibMemcpy_safe(ivCopy, iv, sizeof(ivCopy));
	CryptCfb(buf, buf, length, key, ivCopy, &ivOffset, 0);
}

void RemotePlayCipherBackend::SeedRng()
{
	SceUInt64 tick = 0;
	sceRtcGetCurrentTick(&tick);
	sce_paf_srand((unsigned int)tick);
}

SceInt32 RemotePlayCipherBackend::NextRandom()
{
	return sce_paf_rand();
}

RemotePlayCipher::RemotePlayCipher()
	: m_backend(new RemotePlayCipherBackend())
{
}

RemotePlayCipher::~RemotePlayCipher()
{
	delete m_backend;
}

SceInt32 RemotePlayCipher::DeriveIv(uint8_t *outIv, const uint8_t *key, SceInt32 keyLen,
	const uint8_t *seed, SceSize seedLen, SceUInt64 counter)
{
	(void)key;
	(void)keyLen;
	uint8_t msg[24];
	uint8_t digest[32];

	sce_paf_memcpy(msg, seed, seedLen);
	SceUInt32 hi = (SceUInt32)(counter >> 32);
	SceUInt32 lo = (SceUInt32)(counter & 0xFFFFFFFFU);
	msg[seedLen + 0] = (uint8_t)((hi >> 24) & 0xFF);
	msg[seedLen + 1] = (uint8_t)((hi >> 16) & 0xFF);
	msg[seedLen + 2] = (uint8_t)((hi >> 8) & 0xFF);
	msg[seedLen + 3] = (uint8_t)(hi & 0xFF);
	msg[seedLen + 4] = (uint8_t)((lo >> 24) & 0xFF);
	msg[seedLen + 5] = (uint8_t)((lo >> 16) & 0xFF);
	msg[seedLen + 6] = (uint8_t)((lo >> 8) & 0xFF);
	msg[seedLen + 7] = (uint8_t)(lo & 0xFF);

	SceInt32 ret = sceHmacSha256Digest(kHmacKey, sizeof(kHmacKey), msg, sizeof(msg), digest);
	if (ret >= 0)
		sce_paf_memcpy(outIv, digest, seedLen);
	return ret;
}

void RemotePlayCipher::Encrypt(SceInt32 mode, uint8_t *buf, SceInt32 length,
	const uint8_t *key, const uint8_t *seed, SceUInt64 *counter)
{
	if (m_backend == 0)
		return;
	uint8_t derivedIv[16];
	if (DeriveIv(derivedIv, key, 16, seed, 16, *counter) >= 0) {
		m_backend->EncryptInPlace(mode, buf, length, key, derivedIv);
		++(*counter);
	}
}

void RemotePlayCipher::Decrypt(SceInt32 mode, uint8_t *buf, SceInt32 length,
	const uint8_t *key, const uint8_t *seed, SceUInt64 *counter)
{
	if (m_backend == 0)
		return;
	uint8_t derivedIv[16];
	if (DeriveIv(derivedIv, key, 16, seed, 16, *counter) >= 0) {
		m_backend->DecryptInPlace(mode, buf, length, key, derivedIv);
		++(*counter);
	}
}

void RemotePlayCipher::SeedRng()
{
	if (m_backend != 0)
		m_backend->SeedRng();
}

SceInt32 RemotePlayCipher::NextRandom()
{
	if (m_backend != 0)
		return m_backend->NextRandom();
	return 0;
}

static SceInt32 ReadHttpLine(RemotePlaySession *session, char *lineBuf, SceInt32 maxLen)
{
	SceInt32 len = 0;
	while (len + 1 < maxLen) {
		SceInt32 nread = 0;
		char ch = '\0';
		SceInt32 ret = SocketRecv(&session->sock, &ch, 1, &nread);
		if (ret != 0 || session->aborted != 0 || nread == 0)
			break;
		if (ch == '\r')
			continue;
		if (ch == '\n' || ch == '\0') {
			lineBuf[len] = '\0';
			return len;
		}
		lineBuf[len++] = ch;
	}
	lineBuf[len] = '\0';
	return len;
}

static SceInt32 ExecuteHttpRegistration(RemotePlaySession *session, SceUInt32 pin,
	SceUInt32 localIpHost, SceUInt32 targetIpNet, RemotePlayRecord *outRecord, uint8_t *outMac)
{
	char npOnlineId[164];
	sce_paf_memset(npOnlineId, 0, sizeof(npOnlineId));
	SceInt32 ret = sceNpManagerGetCachedParam(npOnlineId);
	if (ret < 0)
		return ret;

	ClientProfile profile;
	FillClientProfile(&profile, session->clientMode);
	CryptoSessionInit(pin);

	bool socketOpened = false;
	NetSockAddrIn serverAddr;
	sce_paf_memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = 2;
	serverAddr.sin_port = sceNetHtons(kRemotePlayRegistPort);
	serverAddr.sin_addr = targetIpNet;

	ret = SocketOpen(&session->sock, 0);
	if (ret >= 0) {
		socketOpened = true;
		if (session->aborted == 0)
			ret = SocketSetSendTimeout(&session->sock, 15);
		if (ret >= 0 && session->aborted == 0)
			ret = SocketSetRecvTimeout(&session->sock, 15);
		if (ret >= 0 && session->aborted == 0) {
			sceKernelDelayThread(100000);
			ret = SocketConnect(&session->sock, &serverAddr, sizeof(serverAddr));
		}
	}

	if (ret >= 0 && session->aborted == 0) {
		uint8_t localMac[6];
		uint8_t localDeviceId[16];
		GetLocalMacAddress(localMac);
		GetLocalDeviceId(localDeviceId);

		char macHex[16];
		for (int idx = 0; idx < 6; ++idx)
			sce_paf_snprintf(macHex + idx * 2, 3, "%02x", localMac[idx]);
		macHex[12] = '\0';

		char devIdHex[33];
		for (int idx = 0; idx < 16; ++idx)
			sce_paf_snprintf(devIdHex + idx * 2, 3, "%02x", localDeviceId[idx]);
		devIdHex[32] = '\0';

		char clientName[260];
		ret = RemotePlayGetClientName(clientName, 256);
		if (ret == 0) {
			uint8_t bodyBuf[3072];
			sce_paf_memset(bodyBuf, 0, sizeof(bodyBuf));
			SceSize prefixLen = 0;
			if (session->clientMode == 2) {
				CryptoFillRandomAscii(bodyBuf, 480);
				prefixLen = 480;
			}
			int textLen = sce_paf_snprintf((char *)(bodyBuf + prefixLen),
				sizeof(bodyBuf) - prefixLen,
				"Client-Type: %s\r\nNp-Online-Id: %s\r\n",
				profile.clientType, npOnlineId);
			SceSize totalBodyLen = prefixLen + (SceSize)textLen;
			if (totalBodyLen <= sizeof(bodyBuf)) {
				if (session->clientMode == 2) {
					CryptoEncryptPayload(bodyBuf + prefixLen, textLen, bodyBuf + 284, 16);
				} else if (totalBodyLen + 16 <= sizeof(bodyBuf)) {
					CryptoEncryptPayload(bodyBuf + prefixLen, textLen, bodyBuf + totalBodyLen, 16);
					totalBodyLen += 16;
				} else {
					ret = kErrRegistInvalidResp;
				}
			} else {
				ret = kErrRegistInvalidResp;
			}

			if (ret == 0) {
				char httpBuf[2048];
				int headerLen = sce_paf_snprintf(httpBuf, sizeof(httpBuf),
					"POST /sce/rp/regist HTTP/1.1\r\n"
					"HOST: %d.%d.%d.%d\r\n"
					"User-Agent: remoteplay\r\n"
					"Connection: close\r\n"
					"Content-Length: %d\r\n"
					"\r\n",
					(int)((localIpHost >> 24) & 0xFF),
					(int)((localIpHost >> 16) & 0xFF),
					(int)((localIpHost >> 8) & 0xFF),
					(int)(localIpHost & 0xFF),
					(int)totalBodyLen);

				SceInt32 sent = 0;
				ret = SocketSend(&session->sock, httpBuf, (SceUInt32)headerLen, &sent);
				if (ret == 0 && session->aborted == 0 && sent == headerLen) {
					ret = SocketSend(&session->sock, bodyBuf, (SceUInt32)totalBodyLen, &sent);
					if (ret == 0 && session->aborted == 0 && sent == (SceInt32)totalBodyLen) {
						SceInt32 statusLen = ReadHttpLine(session, httpBuf, sizeof(httpBuf));
						if (statusLen >= 0 && session->aborted == 0) {
							if (sce_paf_strncmp("HTTP/1.1 200 OK", httpBuf, (SceSize)statusLen) != 0)
								ret = kErrRegistInvalidResp;
							SceInt32 contentLen = -1;
							for (;;) {
								SceInt32 lineLen = ReadHttpLine(session, httpBuf, sizeof(httpBuf));
								if (lineLen <= 0 || session->aborted != 0)
									break;
								if (sce_paf_strncmp("Content-Length: ", httpBuf, 16) == 0) {
									contentLen = (SceInt32)sce_paf_strtol(httpBuf + 16, 0, 10);
								} else if (sce_paf_strncmp("RP-Application-Reason: ", httpBuf, 23) == 0) {
									SceInt32 reason = (SceInt32)sce_paf_strtol(httpBuf + 23, 0, 16);
									switch (reason) {
									case (SceInt32)0x80115002:
									case (SceInt32)0x80115004:
									case (SceInt32)0x80115009:
										ret = reason;
										break;
									case (SceInt32)0x80115003:
									case (SceInt32)0x80115005:
									case (SceInt32)0x80115006:
									case (SceInt32)0x80115007:
									case (SceInt32)0x80115008:
										ret = kErrRegistInvalidResp;
										break;
									default:
										ret = (SceInt32)0x80115031;
										break;
									}
								}
							}
							if (ret == 0 && session->aborted == 0) {
								if (contentLen > 0 && contentLen < (SceInt32)sizeof(bodyBuf)) {
									SceInt32 received = 0;
									while (received < contentLen) {
										SceInt32 chunk = 0;
										ret = SocketRecv(&session->sock, bodyBuf + received,
											(SceUInt32)(contentLen - received), &chunk);
										if (ret != 0)
											break;
										received += chunk;
									}
									if (ret == 0) {
										CryptoDecryptPayload(bodyBuf, contentLen);
										bodyBuf[contentLen] = '\0';
										ret = ParseRegistResponse((const char *)bodyBuf, contentLen, outRecord);
										sce_paf_memcpy(outMac, localMac, 6);
									}
								} else {
									ret = kErrRegistInvalidResp;
								}
							}
						}
					}
				}
			}
		}
	}

	if (socketOpened)
		SocketClose(&session->sock);
	CryptoSessionTerm();
	return ret;
}

SceInt32 RemotePlaySessionInit(RemotePlaySession *session, uint8_t clientMode)
{
	sce_paf_memset(session, 0, sizeof(*session));
	session->clientMode = clientMode;
	return 0;
}

SceInt32 RemotePlaySessionRegister(RemotePlaySession *session, SceUInt32 pin,
	RemotePlayRecord *outRecord, uint8_t *outMac)
{
	SceUInt32 targetIp = CompanionGetTargetIpAddr();
	SceUInt32 localIps[10];
	SceUInt32 localMasks[10];
	SceInt32 ifCount = 10;
	SceUInt32 matchedLocalIp = 0;

	SceInt32 ret = GetLocalInterfaceAddresses(&ifCount, localIps, localMasks);
	bool found = false;
	if (ret == 0 && ifCount > 0) {
		char searchPacket[4] = { 'S', 'R', 'C', '2' };
		uint8_t recvBuf[1500];
		for (SceInt32 idx = 0; idx < ifCount && !found; ++idx) {
			if (localIps[idx] == 0 || (localIps[idx] & localMasks[idx]) == 0x7F)
				continue;
			sce_paf_memset(&session->sock, 0, sizeof(session->sock));
			ret = SocketOpen(&session->sock, 1);
			if (ret != 0)
				break;
			ret = SocketSetRecvTimeout(&session->sock, 1);
			if (ret != 0)
				break;

			NetSockAddrIn destAddr;
			sce_paf_memset(&destAddr, 0, sizeof(destAddr));
			destAddr.sin_family = 2;
			destAddr.sin_port = sceNetHtons(kRemotePlayRegistPort);
			destAddr.sin_addr = targetIp;

			for (int attempt = 0; attempt < 3; ++attempt) {
				ret = SocketSendTo(&session->sock, searchPacket, 4, 0, &destAddr, sizeof(destAddr));
				if (session->aborted != 0)
					break;
				if (ret == 0) {
					SceInt32 recvd = 0;
					NetSockAddrIn fromAddr;
					SceUInt32 fromLen = sizeof(fromAddr);
					ret = SocketRecvFrom(&session->sock, recvBuf, sizeof(recvBuf), &recvd, &fromAddr, &fromLen);
					if (session->aborted != 0)
						break;
					if (ret == 0 && recvd >= 7 && sce_paf_memcmp(recvBuf, "RES2", 4) == 0) {
						matchedLocalIp = localIps[idx];
						targetIp = fromAddr.sin_addr;
						found = true;
						break;
					}
				}
			}
			if (!found)
				SocketClose(&session->sock);
			if (session->aborted != 0)
				break;
		}
		if (!found && ret == 0)
			ret = kErrRegistNotFound;
	}

	SocketClose(&session->sock);
	if (ret == 0 && found)
		ret = ExecuteHttpRegistration(session, pin, matchedLocalIp, targetIp, outRecord, outMac);
	if (session->aborted != 0)
		ret = kErrRegistAborted;
	return ret;
}

SceInt32 RemotePlaySessionAbort(RemotePlaySession *session)
{
	session->aborted = 1;
	RemotePlayNetAbortGlobal();
	return SocketAbort(&session->sock);
}
