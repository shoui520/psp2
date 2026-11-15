/*
 * The access point's key management (Marvell's authenticator): the 4-way
 * and group key handshakes with each station, the group rekey and the TKIP
 * countermeasures. keyMgmtAp_rom.c is what the Mask ROM build keeps patchable
 * (keyMgmtAp_rom.c of the host port of the same code, GPL-2.0); keyMgmtAp.c
 * is the part on top of it. The state of one station's handshake is its
 * connection's apKeyMgmtInfoSta_t (esatypes.h); the access point's group key
 * and RSN settings are in its WlanConnectionInfo.
 */
#ifndef ROBIN_KEYMGMTAP_H
#define ROBIN_KEYMGMTAP_H

#include <stdint.h>

#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/esa.h"

/* The original calls these; they are not folded into their callers. */
#define KEYMGMT_CALL		__attribute__((noinline))

/* authenticator.h: EAPOL-Key message types, as PopulateKeyMsg() takes them */
#define SECURE_HANDSHAKE_FLAG	0x0080
#define PAIRWISE_KEY_MSG	0x0800
#define WPA2_HANDSHAKE		0x8000

/* keyCommonDef.h: the sizes of the group key's KDE (type, length, OUI, data type) and its parts */
#define KDE_SIZE		6
#define GTK_IE_SIZE		2
#define KEYDATA_SIZE		22	/* the KDE's length: OUI and data type, GTK header, key */
#define KDE_IE_SIZE		2

/* The IEEE 802.11 reason codes the handshakes use */
#define IEEEtypes_REASON_UNSPEC			1
#define IEEEtypes_REASON_MIC_FAILURE		14
#define IEEEtypes_REASON_4WAY_HANDSHK_TIMEOUT	15
#define IEEEtypes_REASON_GRP_KEY_UPD_TIMEOUT	16
#define IEEEtypes_REASON_IE_4WAY_DIFF		17

/* convertMrvlAuthToIEEEAuth(): the host's key management bits and the AKM suite types */
#define UAP_HOSTCMD_KEYMGMT_EAP			1
#define UAP_HOSTCMD_KEYMGMT_PSK			2
#define IEEEtypes_RSN_AUTH_KEY_SUITE_RSVD	0
#define IEEEtypes_RSN_AUTH_KEY_SUITE_8021X	1
#define IEEEtypes_RSN_AUTH_KEY_SUITE_PSK	2

/* the host's cipher bits (Cipher_t's) and the cipher suite types */
#define UAP_HOSTCMD_CIPHER_WEP40		1
#define UAP_HOSTCMD_CIPHER_WEP104		2
#define UAP_HOSTCMD_CIPHER_TKIP			4
#define UAP_HOSTCMD_CIPHER_CCMP			8
#define IEEEtypes_RSN_CIPHER_SUITE_NONE		0
#define IEEEtypes_RSN_CIPHER_SUITE_WEP40	1
#define IEEEtypes_RSN_CIPHER_SUITE_TKIP		2
#define IEEEtypes_RSN_CIPHER_SUITE_CCMP		4
#define IEEEtypes_RSN_CIPHER_SUITE_WEP104	5

/* keyMgmtAp_rom.c */
KEYMGMT_CALL uint32_t util_FindLowestBitSet(uint32_t val);						/* 0x2839e */
void GenerateGTK_internal(KeyData_t *grpKeyData, uint8_t *nonce, const uint8_t *StaMacAddr);	/* 0x2dd10 */
void PopulateKeyMsg(EAPOL_KeyMsg_Tx_t *tx_eapol_ptr, Cipher_t *Cipher, uint32_t Type,
		    uint32_t replay_cnt[2], uint8_t *Nonce);				/* 0x2dd8e */
void prepareKDE(EAPOL_KeyMsg_Tx_t *tx_eapol_ptr, KeyData_t *grKey, Cipher_t *cipher);	/* 0x2de7c */
BOOLEAN Encrypt_keyData(EAPOL_KeyMsg_Tx_t *tx_eapol_ptr, uint8_t *EAPOL_Encr_Key,
			Cipher_t *cipher);						/* 0x2df4a */
void KeyMgmtAp_DerivePTK(uint8_t *pPMK, uint8_t *da, uint8_t *sa, uint8_t *ANonce,
			 uint8_t *SNonce, uint8_t *EAPOL_MIC_Key, uint8_t *EAPOL_Encr_Key,
			 KeyData_t *newPWKey, BOOLEAN use_kdf);				/* 0x2e016 */
BOOLEAN KeyData_CopyWPAWP2(EAPOL_KeyMsg_Tx_t *pTxEAPOL, void *pIe);			/* 0x2e04e */
BOOLEAN KeyData_UpdateKeyMaterial(EAPOL_KeyMsg_Tx_t *pTxEAPOL, SecurityMode_t *pSecType,
				  void *pWPA, void *pWPA2);				/* 0x2e070 */
void KeyData_AddGTK(EAPOL_KeyMsg_Tx_t *pTxEAPOL, KeyData_t *grKey, Cipher_t *cipher);	/* 0x2e096 */
void KeyData_AddKey(EAPOL_KeyMsg_Tx_t *pTxEAPOL, SecurityMode_t *pSecType, KeyData_t *grKey,
		    Cipher_t *cipher);							/* 0x2e0f0 */
void ROM_InitGTK(KeyData_t *grpKeyData, uint8_t *nonce, const uint8_t *StaMacAddr);	/* 0x2e172 */
uint16_t keyMgmtAp_FormatWPARSN_IE_internal(IEEEtypes_InfoElementHdr_t *pIe, uint8_t isRsn,
					    Cipher_t *pCipher, uint32_t cipherCnt,
					    Cipher_t *pMcastCipher, uint32_t authKey,
					    uint32_t authKeyCnt);			/* 0x2e184 */
KEYMGMT_CALL uint8_t convertMrvlAuthToIEEEAuth(uint32_t mrvlauth);					/* 0x2e6fa */
KEYMGMT_CALL uint8_t convertMrvlCipherToIEEECipher(uint32_t mrvlcipher);				/* 0x2e706 */

/* keyMgmtAp.c */
int KeyMgmtStopHskTimer(struct WlanConnection *connPtr, int groupRekey);		/* 0x1f3c8 */
void KeyMgmtHskTimeout(uint32_t timer, struct WlanConnection *connPtr);			/* 0x1f3f0 */
void KeyMgmtStartHskTimer(struct WlanConnection *connPtr, uint32_t timeoutInSeconds);	/* 0x1f456 */
OsaBuffer *PrepDefaultEapolMsg(struct WlanConnection *connPtr, EAPOL_KeyMsg_Tx_t **pTxEapolPtr);	/* 0x1f478 */
int GenerateGrpMsg1(struct WlanConnection *connPtr);					/* 0x1f4ea */
int GeneratePWKMsg3(struct WlanConnection *connPtr);					/* 0x1f57a */
int GeneratePWKMsg1(struct WlanConnection *connPtr);					/* 0x1f672 */
int GenerateApEapolMsg(struct WlanConnection *connPtr, int msgState);			/* 0x1f6f2 */
void KeyMgmtSendGrpKeyMsgToAllSta(struct WlanConnection *connPtr);			/* 0x1f748 */
void ReInitGTK(struct WlanConnection *connPtr);						/* 0x1f788 */
void uapGroupRekey(uintptr_t message);							/* 0x1f7b4 */
void KeyMgmtGrpRekeyCountUpdate(uint32_t timer, struct WlanConnection *connPtr);	/* 0x1f7c4 */
void KeyMgmtStartGrpRekeyTimer(struct WlanConnection *connPtr);				/* 0x1f844 */
void KeyMgmtInit(struct WlanConnection *connPtr);					/* 0x1f8ac */
int ProcessPWKMsg2(OsaBuffer *pmbuf);							/* 0x1f910 */
uint16_t keyMgmtAp_FormatWPARSN_IE(IEEEtypes_InfoElementHdr_t *pIe, uint8_t isRsn,
				   Cipher_t *pCipher, uint32_t cipherCount,
				   Cipher_t *pMcastCipher, uint32_t authKey,
				   uint32_t authKeyCount);				/* 0x1f9e8 */
void sendEventRsnConnect(struct WlanConnection *connPtr, apKeyMgmtInfoSta_t *pKeyMgmtInfo);	/* 0x1fa08 */
int ProcessPWKMsg4(OsaBuffer *pmbuf);							/* 0x1fa76 */
int ProcessGrpMsg2(OsaBuffer *pmbuf);							/* 0x1faea */
int wlanInterfaceNeedsSecurity(const void *context);					/* 0x1fb32 */
void Hostsa_DisAssocAllSta(struct WlanConnection *connPtr);				/* 0x1fb66 */
void ApMicErrTimerExpCb(uint32_t timer, struct WlanConnection *connPtr);		/* 0x1fbb0 */
int ApMicCounterMeasureInvoke(struct WlanConnection *connPtr);				/* 0x1fc24 */
int AuthenticatorProcessEapolPacket(OsaBuffer *pmbuf);					/* 0x1fcca */
void handleFailedHSK(struct WlanConnection *connPtr, uint16_t reason);			/* 0x1fd30 */
KEYMGMT_CALL void incrementReplayCounter(apKeyMgmtInfoSta_t *pKeyMgmtInfo);				/* 0x1fd4c */
int isValidReplayCount(apKeyMgmtInfoSta_t *pKeyMgmtInfo, uint8_t *pRxReplayCount);	/* 0x1fd5c */

/* sqram/keyapi.c */
uint32_t keyApi_ApUpdateKeyMaterial(struct WlanConnection *connPtr, int updateGrpKey);	/* SQRAM 0xc000878c */

/* not converted */
void uapSqramPskUpdated(struct WlanConnection *connPtr, const uint8_t *localAddress,
			const uint8_t *ssid, unsigned int ssidLength);			/* SQRAM, veneer 0x52f5c (inferred) */
uint64_t romMul32x32(uint32_t a, uint32_t b);						/* 0x03f3f3b4 */

#endif /* ROBIN_KEYMGMTAP_H */
