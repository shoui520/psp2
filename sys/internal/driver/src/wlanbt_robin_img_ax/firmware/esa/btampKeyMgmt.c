/*
 * The 4-way handshake of an AMP physical link (0x17bd8..0x18814): the link
 * runs its own WPA2-PSK handshake in AMP security frames (LLC/SNAP of the
 * Bluetooth SIG, protocol 3). The initiator is the authenticator (messages 1
 * and 3), the responder the supplicant (messages 2 and 4); both keep their
 * state in one keyMgmtInfoSta_t, and the link's linkFlags says which message
 * is awaited. Once the pairwise key is installed the PAL is told the link is
 * up. Shaped after Marvell's keyMgmtAp.c and keyMgmtSta.c (Copyright (C)
 * 2014-2017, Marvell International Ltd., GPL-2.0); this firmware's AMP code
 * is not in that source, so the btamp* names are inferred, as is the
 * filename.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/amp.h"
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/mdev.h"
#include "robin/mgmt.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sys.h"
#include "robin/txring.h"
#include "robin/wlan.h"

/* WlanConnection.linkFlags while the handshake runs: the message awaited. */
#define BTAMP_HSK_WAIT_MSG1		3
#define BTAMP_HSK_WAIT_MSG2		4
#define BTAMP_HSK_WAIT_MSG3		5
#define BTAMP_HSK_WAIT_MSG4		6

#define BTAMP_EAPOL_PACKET_CLASS	8	/* OsaBuffer.packetClass of an EAPOL frame */
#define BTAMP_TX_PACKET_TYPE		0xe0	/* WlanTxFrameHeader.packetType: an AMP frame */
#define BTAMP_TX_EVENT			15	/* mdevForwardEvent(): an EAPOL frame for the MAC */
#define BTAMP_DEAUTH_EVENT		8	/* wlanReportEvent(): the link was deauthenticated */

#define BTAMP_EAPOL_KEY_LEN		16	/* CCMP */
#define BTAMP_EAPOL_DESC_RSN		2
#define BTAMP_EAPOL_KEY_BODY		95	/* the EAPOL-Key body without key data */
#define BTAMP_FRAME_OVERHEAD		26	/* the frame without the EAPOL-Key body */
#define BTAMP_PTK_BUFFER		256	/* wlanEncrPool buffer for the PTK */
#define BTAMP_WRAP_BUFFER		400	/* wlanEncrPool buffers for the key wrap */
#define BTAMP_GTK_KDE_LENGTH		22	/* KDE_t.length of the GTK KDE */

/* Key information of the installed pairwise key (key_MgtMaterial_t.keyInfo). */
#define BTAMP_KEY_INFO_PAIRWISE		6

/* An EAPOL-Key frame to the AMP peer: the transmit header, then the 802.1X frame. */
struct __attribute__((packed)) BtampEapolFrame {
	WlanTxFrameHeader header;
	EAPOL_KeyMsg_t keyMsg;
};

_Static_assert(offsetof(struct BtampEapolFrame, keyMsg) == 37, "AMP EAPOL-Key frame");

/* An encryption key kept as words, copied whole. */
struct __attribute__((may_alias)) BtampKey {
	uint32_t words[4];
};

/* Not converted yet; provisional names. */
void macMgmtAcquireState(struct WlanConnection *connection, const uint8_t *peer,
			 unsigned int reason, int unused);			/* 0x34f7c */
extern struct OsaBufferPool wlanEncrPool;					/* 0x04000e70 */

keyMgmtInfoSta_t btampKeyMgmtInfo __attribute__((section(".itcm.bss")));	/* 0x56ba4 */

/* Deauthenticates the peer and tells the host. */
void keyMgmtSendDeauth(struct WlanConnection *connPtr, IEEEtypes_MacAddr_t *pPeer, uint16_t reason)
{
	struct {
		IEEEtypes_MacAddr_t peer;
		uint16_t reason;
	} event;

	macMgmtAcquireState(connPtr, *pPeer, reason, 0);
	event.reason = reason;
	__aeabi_memcpy(event.peer, pPeer, sizeof(event.peer));
	wlanReportEvent(connPtr, &event, BTAMP_DEAUTH_EVENT);
}

void btampKeyMgmtInit(void)
{
	KeyMgmtSta_InitSession(&btampKeyMgmtInfo);
	btampKeyMgmtInfo.apCounterHi = 0;
	btampKeyMgmtInfo.apCounterLo = 0;
	btampKeyMgmtInfo.apCounterZeroDone = 0;
}

/* A transmit buffer with the header of an AMP security frame to the peer. */
static __attribute__((noinline, noclone)) OsaBuffer *btampPrepDefaultEapolMsg(struct WlanConnection *connPtr)
{
	OsaBuffer *pBufDesc = wlanTxBufferAlloc(&wlanBlockPool28, 0, 0, 3);
	WlanTxFrameHeader *header;
	uint16_t headroom;

	if (pBufDesc == NULL)
		return NULL;
	headroom = (uint16_t)wlanPacketHeadroom();
	pBufDesc->dataOffset = headroom;
	pBufDesc->packetClass = BTAMP_EAPOL_PACKET_CLASS;
	header = (WlanTxFrameHeader *)((uint8_t *)pBufDesc + headroom);
	pBufDesc->context = connPtr;
	__aeabi_memclr(header, 15);
	header->bssType = connPtr->bssType;
	header->bssNumber = connPtr->bssNumber;
	header->packetOffset[0] = 15;
	header->packetOffset[1] = 0;
	header->packetType[0] = BTAMP_TX_PACKET_TYPE;
	header->packetType[1] = 0;
	header->flags = 0;
	header->delay = 0;
	__aeabi_memcpy(header->destination, connPtr->peerAddress, 6);
	__aeabi_memcpy(header->source, connPtr->interfaceAddress, 6);
	__aeabi_memcpy(header->snap, ampSnapPrefix, 6);
	header->protocol[0] = 0;
	header->protocol[1] = AMP_PROTOCOL_SECURITY;
	return pBufDesc;
}

/* The frame's lengths, and the frame to the MAC. */
static inline __attribute__((always_inline)) void btampSendEapolMsg(OsaBuffer *pBufDesc, struct BtampEapolFrame *frame, uint16_t length)
{
	frame->header.packetLength[0] = length;
	frame->header.packetLength[1] = length >> 8;
	pBufDesc->length = length;
	frame->header.lengthBigEndian[0] = length >> 8;
	frame->header.lengthBigEndian[1] = length;
	mdevForwardEvent(wlanDevice, pBufDesc, pBufDesc->length, BTAMP_TX_EVENT);
}

static inline __attribute__((always_inline)) void btampSetReplayCounter(EAPOL_KeyMsg_t *pKeyMsg)
{
	if (++btampKeyMgmtInfo.apCounterLo == 0)
		btampKeyMgmtInfo.apCounterHi++;
	__aeabi_uwrite4(htonl(btampKeyMgmtInfo.apCounterHi), &pKeyMsg->replay_cnt[0]);
	__aeabi_uwrite4(htonl(btampKeyMgmtInfo.apCounterLo), &pKeyMsg->replay_cnt[1]);
}

static __attribute__((noinline, noclone)) void btampFormEapolHdr(Hdr_8021x_t *pHdr, uint16_t bodyLength);
static __attribute__((noinline, noclone)) int btampProcessPWKMsg4(struct WlanConnection *connPtr, EAPOL_KeyMsg_t *pKeyMsg);

/* Message 1: the authenticator's nonce. */
int btampGeneratePWKMsg1(struct WlanConnection *connPtr)
{
	OsaBuffer *pBufDesc = btampPrepDefaultEapolMsg(connPtr);
	struct BtampEapolFrame *frame;
	EAPOL_KeyMsg_t *pKeyMsg;

	if (pBufDesc == NULL)
		return 1;
	frame = (struct BtampEapolFrame *)((uint8_t *)pBufDesc + pBufDesc->dataOffset);
	pKeyMsg = &frame->keyMsg;
	pKeyMsg->desc_type = BTAMP_EAPOL_DESC_RSN;
	pKeyMsg->key_length = htons(BTAMP_EAPOL_KEY_LEN);
	pKeyMsg->key_info.KeyDescriptorVersion = 2;
	pKeyMsg->key_info.KeyType = 1;
	pKeyMsg->key_info.KeyIndex = 0;
	pKeyMsg->key_info.Install = 0;
	pKeyMsg->key_info.KeyAck = 1;
	pKeyMsg->key_info.KeyMIC = 0;
	pKeyMsg->key_info.Secure = 0;
	pKeyMsg->key_info.Error = 0;
	pKeyMsg->key_info.Request = 0;
	pKeyMsg->key_info.EncryptedKeyData = 0;
	pKeyMsg->key_info.Reserved = 0;
	btampSetReplayCounter(pKeyMsg);
	supplicantGenerateRand(btampKeyMgmtInfo.ANonce, NONCE_SIZE);
	__aeabi_memcpy(pKeyMsg->key_nonce, btampKeyMgmtInfo.ANonce, NONCE_SIZE);
	__aeabi_memclr(pKeyMsg->EAPOL_key_IV, sizeof(pKeyMsg->EAPOL_key_IV));
	__aeabi_memclr(pKeyMsg->key_RSC, sizeof(pKeyMsg->key_RSC));
	__aeabi_memclr(pKeyMsg->key_ID, sizeof(pKeyMsg->key_ID));
	__aeabi_memclr(pKeyMsg->key_MIC, sizeof(pKeyMsg->key_MIC));
	pKeyMsg->key_material_len = 0;
	btampFormEapolHdr(&pKeyMsg->hdr_8021x, BTAMP_EAPOL_KEY_BODY);
	connPtr->linkFlags = BTAMP_HSK_WAIT_MSG2;
	btampSendEapolMsg(pBufDesc, frame, BTAMP_EAPOL_KEY_BODY + BTAMP_FRAME_OVERHEAD);
	return 0;
}

/* Message 3: the RSN element and the GTK KDE (no group key on a link), wrapped with the KEK. */
static __attribute__((noinline, noclone)) int btampGeneratePWKMsg3(struct WlanConnection *connPtr, EAPOL_KeyMsg_t *pRxKeyMsg)
{
	OsaBuffer *pBufDesc = btampPrepDefaultEapolMsg(connPtr);
	OsaBuffer *pPlainDesc, *pCipherDesc;
	struct BtampEapolFrame *frame;
	EAPOL_KeyMsg_t *pKeyMsg;
	struct BtampKey kek;
	uint8_t kdeHeader[6];
	uint8_t *pPlain, *pCipher, *pKeyData;
	uint16_t keyDataLen, bodyLength;
	uint8_t ieLen, padLen;
	GTK_KDE_t *pGtk;

	if (pBufDesc == NULL)
		return 1;
	frame = (struct BtampEapolFrame *)((uint8_t *)pBufDesc + pBufDesc->dataOffset);
	pKeyMsg = &frame->keyMsg;
	pKeyMsg->desc_type = BTAMP_EAPOL_DESC_RSN;
	pKeyMsg->key_info.KeyDescriptorVersion = 2;
	pKeyMsg->key_length = htons(BTAMP_EAPOL_KEY_LEN);
	pKeyMsg->key_info.KeyType = 1;
	pKeyMsg->key_info.KeyIndex = 0;
	pKeyMsg->key_info.Install = 1;
	pKeyMsg->key_info.KeyAck = 1;
	pKeyMsg->key_info.KeyMIC = 1;
	pKeyMsg->key_info.Error = 0;
	pKeyMsg->key_info.Request = 0;
	pKeyMsg->key_info.Secure = 1;
	pKeyMsg->key_info.EncryptedKeyData = 1;
	pKeyMsg->key_info.Reserved = 0;
	btampSetReplayCounter(pKeyMsg);
	__aeabi_memcpy(pKeyMsg->key_nonce, btampKeyMgmtInfo.ANonce, NONCE_SIZE);
	__aeabi_memclr(pKeyMsg->EAPOL_key_IV, sizeof(pKeyMsg->EAPOL_key_IV));
	__aeabi_memclr(pKeyMsg->key_RSC, sizeof(pKeyMsg->key_RSC));
	pKeyMsg->key_RSC[0] = 0;
	pKeyMsg->key_RSC[1] = 0;
	__aeabi_memclr(pKeyMsg->key_RSC + 2, 4);
	__aeabi_memclr(pKeyMsg->key_ID, sizeof(pKeyMsg->key_ID));

	/* The RSN element, then the GTK KDE, padded to the key wrap's 8 bytes. */
	ieLen = connPtr->rsnElement[1] + 2;
	pKeyData = pKeyMsg->key_data;
	__aeabi_memcpy(pKeyData, connPtr->rsnElement, ieLen);
	pKeyMsg->key_material_len = ieLen;
	kdeHeader[0] = 0xdd;
	kdeHeader[1] = BTAMP_GTK_KDE_LENGTH;
	kdeHeader[2] = 0x00;
	kdeHeader[3] = 0x0f;
	kdeHeader[4] = 0xac;
	kdeHeader[5] = KDE_DATA_TYPE_GTK;
	pKeyMsg->key_material_len = ieLen + 6 + 2 + TK_SIZE;
	__aeabi_memcpy(pKeyData + ieLen, kdeHeader, sizeof(kdeHeader));
	pGtk = (GTK_KDE_t *)(pKeyData + ieLen + sizeof(kdeHeader));
	pGtk->Tx = 1;
	pGtk->KeyID = 0;
	pGtk->rsvd = 0;
	__aeabi_memclr(pGtk->GTK, TK_SIZE);
	padLen = (8 - pKeyMsg->key_material_len) & 7;
	if (padLen != 0) {
		pKeyData[pKeyMsg->key_material_len] = 0xdd;
		__aeabi_memclr(pKeyData + pKeyMsg->key_material_len + 1, padLen - 1);
		pKeyMsg->key_material_len = pKeyMsg->key_material_len + padLen;
	}

	pPlainDesc = osa_BufferAlloc(&wlanEncrPool, BTAMP_WRAP_BUFFER, 0xffffffff);
	if (pPlainDesc == NULL)
		goto fail;
	pCipherDesc = osa_BufferAlloc(&wlanEncrPool, BTAMP_WRAP_BUFFER, 0xffffffff);
	if (pCipherDesc == NULL) {
		osa_BufferFree(pPlainDesc);
		goto fail;
	}
	pPlain = (uint8_t *)pPlainDesc + pPlainDesc->dataOffset;
	pCipher = (uint8_t *)pCipherDesc + pCipherDesc->dataOffset;
	kek = *(const struct BtampKey *)btampKeyMgmtInfo.EAPOL_Encr_Key;
	__aeabi_memcpy(pPlain, pKeyData, pKeyMsg->key_material_len);
	MRVL_AesWrap((uint8_t *)&kek, 2, pKeyMsg->key_material_len >> 3, pPlain, NULL, pCipher);
	keyDataLen = pKeyMsg->key_material_len + 8;	/* the key wrap's IV */
	pKeyMsg->key_material_len = keyDataLen;
	__aeabi_memcpy(pKeyData, pCipher, keyDataLen);
	osa_BufferFree(pPlainDesc);
	osa_BufferFree(pCipherDesc);

	bodyLength = pKeyMsg->key_material_len + BTAMP_EAPOL_KEY_BODY;
	pKeyMsg->key_material_len = htons(pKeyMsg->key_material_len);
	btampFormEapolHdr(&pKeyMsg->hdr_8021x, bodyLength);
	ComputeEAPOL_MIC(pKeyMsg, bodyLength + sizeof(Hdr_8021x_t), btampKeyMgmtInfo.EAPOL_MIC_Key,
			 EAPOL_MIC_KEY_SIZE, pKeyMsg->key_info.KeyDescriptorVersion);
	connPtr->linkFlags = BTAMP_HSK_WAIT_MSG4;
	btampSendEapolMsg(pBufDesc, frame, bodyLength + BTAMP_FRAME_OVERHEAD);
	return 0;

fail:
	osa_BufferFree(pBufDesc);
	return 1;
}

/* The PTK from the PMK, the lower address and nonce first. */
static __attribute__((noinline, noclone)) void btampDerivePTK(uint8_t *pAddr1, uint8_t *pAddr2, uint8_t *pNonce1, uint8_t *pNonce2,
			   uint8_t *pPTK, uint8_t *pPMK)
{
	uint8_t context[76];
	char *prefix;

	if (memcmp(pAddr1, pAddr2, 6) < 0) {
		__aeabi_memcpy(context, pAddr1, 6);
		__aeabi_memcpy(context + 6, pAddr2, 6);
	} else {
		__aeabi_memcpy(context, pAddr2, 6);
		__aeabi_memcpy(context + 6, pAddr1, 6);
	}
	if (memcmp(pNonce1, pNonce2, NONCE_SIZE) < 0) {
		__aeabi_memcpy(context + 12, pNonce1, NONCE_SIZE);
		__aeabi_memcpy(context + 12 + NONCE_SIZE, pNonce2, NONCE_SIZE);
	} else {
		__aeabi_memcpy(context + 12, pNonce2, NONCE_SIZE);
		__aeabi_memcpy(context + 12 + NONCE_SIZE, pNonce1, NONCE_SIZE);
	}
	prefix = "Pairwise key expansion";
	Mrvl_PRF(pPMK, 32, (unsigned char *)prefix, strlen(prefix), context, sizeof(context), pPTK, 64);
}

/* Checks the MIC of a received message with the KCK; the MIC field is cleared. */
static inline __attribute__((always_inline)) int btampMicValid(EAPOL_KeyMsg_t *pKeyMsg)
{
	uint8_t mic[EAPOL_MIC_KEY_SIZE];

	__aeabi_memcpy(mic, pKeyMsg->key_MIC, EAPOL_MIC_KEY_SIZE);
	__aeabi_memclr(pKeyMsg->key_MIC, EAPOL_MIC_KEY_SIZE);
	ComputeEAPOL_MIC(pKeyMsg, ntohs(pKeyMsg->hdr_8021x.pckt_body_len) + sizeof(Hdr_8021x_t),
			 btampKeyMgmtInfo.EAPOL_MIC_Key, EAPOL_MIC_KEY_SIZE,
			 pKeyMsg->key_info.KeyDescriptorVersion);
	return memcmp(pKeyMsg->key_MIC, mic, EAPOL_MIC_KEY_SIZE) == 0;
}

/* Message 2 at the authenticator: the PTK, then message 3. */
static __attribute__((noinline, noclone)) int btampProcessPWKMsg2(struct WlanConnection *connPtr, EAPOL_KeyMsg_t *pKeyMsg)
{
	IEEEtypes_MacAddr_t *pPeer;
	OsaBuffer *pPtkDesc;
	uint8_t *pPMK, *pPTK;

	__aeabi_memcpy(btampKeyMgmtInfo.SNonce, pKeyMsg->key_nonce, NONCE_SIZE);
	pPeer = (IEEEtypes_MacAddr_t *)connPtr->peerAddress;
	pPMK = pmkCacheFindPMK(pPeer);
	if (pPMK == NULL)
		for (;;)
			;
	pPtkDesc = osa_BufferAlloc(&wlanEncrPool, BTAMP_PTK_BUFFER, 0xffffffff);
	pPTK = (uint8_t *)pPtkDesc + pPtkDesc->dataOffset;
	btampDerivePTK(connPtr->interfaceAddress, *pPeer, btampKeyMgmtInfo.ANonce,
		       btampKeyMgmtInfo.SNonce, pPTK, pPMK);
	__aeabi_memcpy(btampKeyMgmtInfo.EAPOL_MIC_Key, pPTK, 16);
	__aeabi_memcpy(btampKeyMgmtInfo.EAPOL_Encr_Key, pPTK + 16, 16);
	__aeabi_memcpy(btampKeyMgmtInfo.PWKey.Key, pPTK + 32, 16);
	__aeabi_memcpy(btampKeyMgmtInfo.PWKey.TxMICKey, pPTK + 48, 8);
	__aeabi_memcpy(btampKeyMgmtInfo.PWKey.RxMICKey, pPTK + 56, 8);
	osa_BufferFree(pPtkDesc);
	if (!btampMicValid(pKeyMsg) || btampGeneratePWKMsg3(connPtr, pKeyMsg) != 0)
		wlanConnectionRelease(connPtr);
	return 0;
}

/* Message 2 from the supplicant: the station's nonce and its RSN element. */
static __attribute__((noinline, noclone)) int btampGeneratePWKMsg2(struct WlanConnection *connPtr, EAPOL_KeyMsg_t *pRxKeyMsg)
{
	OsaBuffer *pBufDesc = btampPrepDefaultEapolMsg(connPtr);
	struct BtampEapolFrame *frame;
	EAPOL_KeyMsg_t *pKeyMsg;
	uint16_t ieLen;

	if (pBufDesc == NULL)
		return 1;
	frame = (struct BtampEapolFrame *)((uint8_t *)pBufDesc + pBufDesc->dataOffset);
	pKeyMsg = &frame->keyMsg;
	pKeyMsg->key_length = 0;
	pKeyMsg->key_info.KeyDescriptorVersion = pRxKeyMsg->key_info.KeyDescriptorVersion;
	pKeyMsg->key_info.KeyType = 1;
	pKeyMsg->key_info.KeyMIC = 1;
	__aeabi_uwrite4(__aeabi_uread4(&pRxKeyMsg->replay_cnt[0]), &pKeyMsg->replay_cnt[0]);
	__aeabi_uwrite4(__aeabi_uread4(&pRxKeyMsg->replay_cnt[1]), &pKeyMsg->replay_cnt[1]);
	__aeabi_memcpy(pKeyMsg->key_nonce, btampKeyMgmtInfo.SNonce, NONCE_SIZE);
	__aeabi_memclr(pKeyMsg->EAPOL_key_IV, sizeof(pKeyMsg->EAPOL_key_IV));
	__aeabi_memclr(pKeyMsg->key_RSC, sizeof(pKeyMsg->key_RSC));
	__aeabi_memclr(pKeyMsg->key_ID, sizeof(pKeyMsg->key_ID));
	__aeabi_memclr(pKeyMsg->key_MIC, sizeof(pKeyMsg->key_MIC));
	ieLen = connPtr->rsnElement[1] + 2;
	__aeabi_memcpy(pKeyMsg->key_data, connPtr->rsnElement, ieLen);
	pKeyMsg->key_material_len = ieLen;
	btampFormEapolHdr(&pKeyMsg->hdr_8021x, ieLen + BTAMP_EAPOL_KEY_BODY);
	pKeyMsg->key_material_len = htons(pKeyMsg->key_material_len);
	ComputeEAPOL_MIC(pKeyMsg, ieLen + BTAMP_EAPOL_KEY_BODY + sizeof(Hdr_8021x_t),
			 btampKeyMgmtInfo.EAPOL_MIC_Key, EAPOL_MIC_KEY_SIZE,
			 pKeyMsg->key_info.KeyDescriptorVersion);
	connPtr->linkFlags = BTAMP_HSK_WAIT_MSG3;
	btampSendEapolMsg(pBufDesc, frame, ieLen + BTAMP_EAPOL_KEY_BODY + BTAMP_FRAME_OVERHEAD);
	return 0;
}

/* Message 4 from the supplicant: the handshake is done. */
static __attribute__((noinline, noclone)) int btampGeneratePWKMsg4(struct WlanConnection *connPtr, EAPOL_KeyMsg_t *pRxKeyMsg)
{
	OsaBuffer *pBufDesc = btampPrepDefaultEapolMsg(connPtr);
	struct BtampEapolFrame *frame;
	EAPOL_KeyMsg_t *pKeyMsg;

	if (pBufDesc == NULL)
		return 1;
	frame = (struct BtampEapolFrame *)((uint8_t *)pBufDesc + pBufDesc->dataOffset);
	pKeyMsg = &frame->keyMsg;
	pKeyMsg->key_info.KeyDescriptorVersion = pRxKeyMsg->key_info.KeyDescriptorVersion;
	pKeyMsg->key_info.KeyType = 1;
	pKeyMsg->key_info.KeyMIC = 1;
	pKeyMsg->key_info.Secure = pRxKeyMsg->key_info.Secure;
	pKeyMsg->key_length = 0;
	__aeabi_uwrite4(__aeabi_uread4(&pRxKeyMsg->replay_cnt[0]), &pKeyMsg->replay_cnt[0]);
	__aeabi_uwrite4(__aeabi_uread4(&pRxKeyMsg->replay_cnt[1]), &pKeyMsg->replay_cnt[1]);
	__aeabi_memclr(pKeyMsg->key_nonce, sizeof(pKeyMsg->key_nonce));
	__aeabi_memclr(pKeyMsg->EAPOL_key_IV, sizeof(pKeyMsg->EAPOL_key_IV));
	__aeabi_memclr(pKeyMsg->key_RSC, sizeof(pKeyMsg->key_RSC));
	__aeabi_memclr(pKeyMsg->key_ID, sizeof(pKeyMsg->key_ID));
	__aeabi_memclr(pKeyMsg->key_MIC, sizeof(pKeyMsg->key_MIC));
	pKeyMsg->key_material_len = 0;
	btampFormEapolHdr(&pKeyMsg->hdr_8021x, BTAMP_EAPOL_KEY_BODY);
	pKeyMsg->key_material_len = htons(pKeyMsg->key_material_len);
	ComputeEAPOL_MIC(pKeyMsg, BTAMP_EAPOL_KEY_BODY + sizeof(Hdr_8021x_t),
			 btampKeyMgmtInfo.EAPOL_MIC_Key, EAPOL_MIC_KEY_SIZE,
			 pKeyMsg->key_info.KeyDescriptorVersion);
	btampSendEapolMsg(pBufDesc, frame, BTAMP_EAPOL_KEY_BODY + BTAMP_FRAME_OVERHEAD);
	return 0;
}

/*
 * Message 1 at the supplicant: the PTK, then message 2. When message 2 cannot
 * be sent, the link is released and the original returns the message's
 * address (nonzero); nothing reads the result.
 */
static __attribute__((noinline, noclone)) int btampProcessPWKMsg1(struct WlanConnection *connPtr, EAPOL_KeyMsg_t *pKeyMsg)
{
	uint8_t *pReplay = (uint8_t *)pKeyMsg->replay_cnt;
	IEEEtypes_MacAddr_t *pPeer;
	OsaBuffer *pPtkDesc;
	uint8_t *pPMK, *pPTK;

	if (!isApReplayCounterFresh(&btampKeyMgmtInfo, pReplay)) {
		wlanConnectionRelease(connPtr);
		return 0;
	}
	__aeabi_memcpy(btampKeyMgmtInfo.ANonce, pKeyMsg->key_nonce, NONCE_SIZE);
	supplicantGenerateRand(btampKeyMgmtInfo.SNonce, NONCE_SIZE);
	pPeer = (IEEEtypes_MacAddr_t *)connPtr->peerAddress;
	pPMK = pmkCacheFindPMK(pPeer);
	if (pPMK == NULL)
		for (;;)
			;
	pPtkDesc = osa_BufferAlloc(&wlanEncrPool, BTAMP_PTK_BUFFER, 0xffffffff);
	pPTK = (uint8_t *)pPtkDesc + pPtkDesc->dataOffset;
	btampDerivePTK(*pPeer, connPtr->interfaceAddress, btampKeyMgmtInfo.ANonce,
		       btampKeyMgmtInfo.SNonce, pPTK, pPMK);
	__aeabi_memcpy(btampKeyMgmtInfo.EAPOL_MIC_Key, pPTK, 16);
	__aeabi_memcpy(btampKeyMgmtInfo.EAPOL_Encr_Key, pPTK + 16, 16);
	__aeabi_memcpy(btampKeyMgmtInfo.PWKey.Key, pPTK + 32, 16);
	__aeabi_memcpy(btampKeyMgmtInfo.PWKey.RxMICKey, pPTK + 48, 8);
	__aeabi_memcpy(btampKeyMgmtInfo.PWKey.TxMICKey, pPTK + 56, 8);
	osa_BufferFree(pPtkDesc);
	if (btampGeneratePWKMsg2(connPtr, pKeyMsg) != 0) {
		wlanConnectionRelease(connPtr);
		return (int)(uintptr_t)pKeyMsg;
	}
	updateApReplayCounter(&btampKeyMgmtInfo, pReplay);
	btampKeyMgmtInfo.RSNSecured = 0;
	return 0;
}

/* The pairwise key into the MAC; the link is up. */
static inline __attribute__((always_inline)) void btampInstallPairwiseKey(struct WlanConnection *connPtr)
{
	key_MgtMaterial_t keyMgtData;

	keyMgtData.keyType = KEY_TYPE_AES;
	keyMgtData.keyInfo = BTAMP_KEY_INFO_PAIRWISE;
	keyMgtData.keyLen = WPA_AES_KEY_LEN;
	__aeabi_memcpy(keyMgtData.keyEncypt.AES.key, btampKeyMgmtInfo.PWKey.Key, WPA_AES_KEY_LEN);
	keyApi_UpdateKeyMaterial(connPtr, &keyMgtData);
}

static inline __attribute__((always_inline)) void btampLinkSecured(struct WlanConnection *connPtr)
{
	ampPalNotifySecured(connPtr->bssNumber);
	ampPalLinkEstablished(connPtr);
	supplicantDisable();
}

/* Message 3 at the supplicant: the key data, message 4, then the pairwise key. */
static __attribute__((noinline, noclone)) int btampProcessPWKMsg3(struct WlanConnection *connPtr, EAPOL_KeyMsg_t *pKeyMsg)
{
	uint8_t *pReplay = (uint8_t *)pKeyMsg->replay_cnt;
	KeyData_t *pPWKey = &btampKeyMgmtInfo.PWKey;
	uint16_t keyDataLen;

	if (!isApReplayCounterFresh(&btampKeyMgmtInfo, pReplay) ||
	    memcmp(pKeyMsg->key_nonce, btampKeyMgmtInfo.ANonce, NONCE_SIZE) != 0 ||
	    !btampMicValid(pKeyMsg))
		goto fail;
	pPWKey->TxIV16 = 1;
	pPWKey->TxIV32 = 0;
	FillKeyMaterialStruct(connPtr, ntohs(pKeyMsg->key_length), 1, pPWKey);
	if (pKeyMsg->key_info.EncryptedKeyData) {
		osaCancelTimer(btampKeyMgmtInfo.rsnTimer);
		btampKeyMgmtInfo.rsnTimer = 0;
		btampKeyMgmtInfo.GRKey.TxIV16 = pKeyMsg->key_RSC[1] << 8;
		btampKeyMgmtInfo.GRKey.TxIV16 |= pKeyMsg->key_RSC[0];
		btampKeyMgmtInfo.GRKey.TxIV32 = 0xffffffff;
		keyDataLen = ntohs(pKeyMsg->key_material_len);
		MRVL_AesUnWrap(btampKeyMgmtInfo.EAPOL_Encr_Key, 2, (int)keyDataLen / 8 - 1,
			       pKeyMsg->key_data, NULL, pKeyMsg->key_data);
		parseKeyDataField(pKeyMsg->key_data, keyDataLen - 8);
		btampKeyMgmtInfo.RSNSecured = 1;
	}
	if (btampGeneratePWKMsg4(connPtr, pKeyMsg) != 0)
		goto fail;
	btampKeyMgmtInfo.RSNDataTrafficEnabled = 1;
	pPWKey->TxIV16 = 1;
	pPWKey->TxIV32 = 0;
	btampInstallPairwiseKey(connPtr);
	updateApReplayCounter(&btampKeyMgmtInfo, pReplay);
	btampLinkSecured(connPtr);
	return 0;

fail:
	wlanConnectionRelease(connPtr);
	return 0;
}

/* An EAPOL-Key message for the AMP link the buffer came in on. */
void btampProcessKeyMgmtData(OsaBuffer *pBufDesc)
{
	struct WlanConnection *connPtr = pBufDesc->context;
	EAPOL_KeyMsg_t *pKeyMsg = (EAPOL_KeyMsg_t *)((uint8_t *)pBufDesc + pBufDesc->dataOffset);

	switch (connPtr->linkFlags) {
	case BTAMP_HSK_WAIT_MSG1:
		btampProcessPWKMsg1(connPtr, pKeyMsg);
		break;
	case BTAMP_HSK_WAIT_MSG2:
		btampProcessPWKMsg2(connPtr, pKeyMsg);
		break;
	case BTAMP_HSK_WAIT_MSG3:
		btampProcessPWKMsg3(connPtr, pKeyMsg);
		break;
	case BTAMP_HSK_WAIT_MSG4:
		btampProcessPWKMsg4(connPtr, pKeyMsg);
		break;
	}
}

/* The 802.1X header of an EAPOL-Key frame. */
static __attribute__((noinline, noclone)) void btampFormEapolHdr(Hdr_8021x_t *pHdr, uint16_t bodyLength)
{
	pHdr->protocol_ver = 1;
	pHdr->pckt_type = 3;	/* EAPOL-Key */
	pHdr->pckt_body_len = htons(bodyLength);
}

/* Message 4 at the authenticator: the pairwise key; the link is up. */
static __attribute__((noinline, noclone)) int btampProcessPWKMsg4(struct WlanConnection *connPtr, EAPOL_KeyMsg_t *pKeyMsg)
{
	if (!btampMicValid(pKeyMsg)) {
		wlanConnectionRelease(connPtr);
		return 0;
	}
	btampKeyMgmtInfo.RSNDataTrafficEnabled = 1;
	btampKeyMgmtInfo.PWKey.TxIV16 = 1;
	btampKeyMgmtInfo.PWKey.TxIV32 = 0;
	btampInstallPairwiseKey(connPtr);
	btampLinkSecured(connPtr);
	return 0;
}
