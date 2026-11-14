/*
 * Marvell's embedded supplicant and authenticator ("esa"): the PMK cache, the
 * EAPOL-Key handling and the key derivation. The names are Marvell's, from
 * the host port of the same code (mlan/esa of Marvell's 88W8977 driver,
 * GPL-2.0); this firmware calls its runtime directly where the port passes
 * priv and util_fns. Each _rom function first offers its call to its
 * <name>_hook ROM-patch slot.
 */
#ifndef ROBIN_ESA_H
#define ROBIN_ESA_H

#include <stdint.h>

#define MAX_PMK_SIZE		32
#define BSSID_FLAG		0xff	/* pmkElement_t.length of a PMK keyed by BSSID */
#define PSK_PASS_PHRASE_LEN_MAX	64

typedef uint8_t IEEEtypes_MacAddr_t[6];

#define NONCE_SIZE		32
#define EAPOL_MIC_KEY_SIZE	16
#define EAPOL_MIC_SIZE		16
#define EAPOL_ENCR_KEY_SIZE	16
#define IEEE_8021X_PACKET_TYPE_EAPOL_KEY 3

/* keyMgmtStaTypes.h */
typedef struct Cipher_t {
	uint8_t wep40 : 1;
	uint8_t wep104 : 1;
	uint8_t tkip : 1;
	uint8_t ccmp : 1;
	uint8_t rsvd : 4;
} Cipher_t;

typedef struct __attribute__((packed)) SecurityMode_t {
	uint16_t noRsn : 1;
	uint16_t wepStatic : 1;
	uint16_t wepDynamic : 1;
	uint16_t wpa : 1;
	uint16_t wpaNone : 1;
	uint16_t wpa2 : 1;
	uint16_t cckm : 1;
	uint16_t wapi : 1;
	uint16_t rsvd : 8;
} SecurityMode_t;

typedef struct AkmSuite_t {
	uint8_t akmOui[3];
	uint8_t akmType;
} AkmSuite_t;

/* IEEE_types.h */
typedef struct __attribute__((packed)) ether_hdr_t {
	uint8_t da[6];
	uint8_t sa[6];
	uint16_t type;
} ether_hdr_t;

typedef struct __attribute__((packed)) IEEEtypes_WPAElement_t {
	uint8_t ElementId;
	uint8_t Len;
	uint8_t OuiType[4];	/* 00:50:f2:01 */
	uint16_t Ver;
	uint8_t GrpKeyCipher[4];
	uint16_t PwsKeyCnt;
	uint8_t PwsKeyCipherList[4];
	uint16_t AuthKeyCnt;
	uint8_t AuthKeyList[4];
} IEEEtypes_WPAElement_t;

typedef struct __attribute__((packed)) IEEEtypes_RSNCapability_t {
	uint8_t PreAuth : 1;
	uint8_t NoPairwise : 1;
	uint8_t PtksaReplayCtr : 2;
	uint8_t GtksaReplayCtr : 2;
	uint8_t MFPR : 1;
	uint8_t MFPC : 1;
	uint8_t Reserved_8 : 1;
	uint8_t PeerkeyEnabled : 1;
	uint8_t SppAmsduCap : 1;
	uint8_t SppAmsduReq : 1;
	uint8_t PBAC : 1;
	uint8_t Reserved_13_15 : 3;
} IEEEtypes_RSNCapability_t;

/* the fixed part; the lists after GrpKeyCipher have variable length */
typedef struct __attribute__((packed)) IEEEtypes_RSNElement_t {
	uint8_t ElementId;
	uint8_t Len;
	uint16_t Ver;
	uint8_t GrpKeyCipher[4];
	uint16_t PwsKeyCnt;
	uint8_t PwsKeyCipherList[4];
	uint16_t AuthKeyCnt;
	uint8_t AuthKeyList[4];
	IEEEtypes_RSNCapability_t RsnCap;
	uint16_t PMKIDCnt;
	uint8_t PMKIDList[16];
	uint8_t GrpMgmtCipher[4];
} IEEEtypes_RSNElement_t;

/* keyCommonDef.h */
typedef struct __attribute__((packed)) Hdr_8021x_t {
	uint8_t protocol_ver;
	uint8_t pckt_type;
	uint16_t pckt_body_len;
} Hdr_8021x_t;

/* in message byte order: key information byte 1 first */
typedef struct __attribute__((packed)) key_info_t {
	uint16_t KeyMIC : 1;
	uint16_t Secure : 1;
	uint16_t Error : 1;
	uint16_t Request : 1;
	uint16_t EncryptedKeyData : 1;
	uint16_t Reserved : 3;
	uint16_t KeyDescriptorVersion : 3;
	uint16_t KeyType : 1;
	uint16_t KeyIndex : 2;
	uint16_t Install : 1;
	uint16_t KeyAck : 1;
} key_info_t;

typedef struct __attribute__((packed)) EAPOL_KeyMsg_t {
	Hdr_8021x_t hdr_8021x;
	uint8_t desc_type;
	key_info_t key_info;
	uint16_t key_length;
	uint32_t replay_cnt[2];
	uint8_t key_nonce[NONCE_SIZE];
	uint8_t EAPOL_key_IV[16];
	uint8_t key_RSC[8];
	uint8_t key_ID[8];
	uint8_t key_MIC[EAPOL_MIC_KEY_SIZE];
	uint16_t key_material_len;
	uint8_t key_data[1];
} EAPOL_KeyMsg_t;

typedef struct __attribute__((packed)) EAPOL_KeyMsg_Tx_t {
	ether_hdr_t ethHdr;
	EAPOL_KeyMsg_t keyMsg;
} EAPOL_KeyMsg_Tx_t;

_Static_assert(__builtin_offsetof(EAPOL_KeyMsg_t, key_MIC) == 81, "EAPOL-Key MIC");
_Static_assert(sizeof(EAPOL_KeyMsg_Tx_t) == 114, "EAPOL-Key frame");
_Static_assert(__builtin_offsetof(IEEEtypes_WPAElement_t, AuthKeyCnt) == 18, "WPA element");

/* keyMgmtApStaCommon.c */
extern const uint8_t wpa_oui[3];
extern const uint8_t kde_oui[3];
extern const uint8_t wpa_oui_none[4];
extern const uint8_t wpa_oui01[4];
extern const uint8_t wpa_oui02[4];
extern const uint8_t wpa_oui04[4];
extern const uint8_t wpa_oui05[4];
extern const uint8_t wpa2_oui01[4];
extern const uint8_t wpa2_oui02[4];
extern const uint8_t wpa2_oui04[4];
extern const uint8_t wpa2_oui05[4];

/* pmkCache_rom.h; this firmware keeps no passphrase */
typedef struct pmkElement_t {
	union {
		IEEEtypes_MacAddr_t Bssid;
		char Ssid[32];
	} key;
	uint8_t PMK[MAX_PMK_SIZE];	/* PMK / PSK */
	uint8_t length;
	int8_t replacementRank;
} pmkElement_t;

_Static_assert(sizeof(pmkElement_t) == 66, "PMK cache element");

extern int8_t replacementRankMax;
extern int ramHook_MAX_PMK_CACHE_ENTRIES;
extern pmkElement_t *ramHook_pmkCache;
extern void (*ramHook_hal_SetCpuMaxSpeed)(void);
extern void (*ramHook_hal_RestoreCpuSpeed)(void);

pmkElement_t *pmkCacheNewElement(void);
void pmkCacheUpdateReplacementRank(pmkElement_t *pPMKElement);
pmkElement_t *pmkCacheFindPMKElement(IEEEtypes_MacAddr_t *pBssid);
pmkElement_t *pmkCacheFindPSKElement(uint8_t *pSsid, uint8_t ssidLen);
uint8_t *pmkCacheFindPMK(IEEEtypes_MacAddr_t *pBssid);
void pmkCacheAddPMK(IEEEtypes_MacAddr_t *pBssid, uint8_t *pPMK);
void pmkCacheAddPSK(uint8_t *pSsid, uint8_t ssidLen, uint8_t *pPSK);
void pmkCacheDeletePMK(uint8_t *pBssid);
void pmkCacheDeletePSK(uint8_t *pSsid, uint8_t ssidLen);
uint8_t pmkCacheGetHexNibble(uint8_t nibble);
void pmkCacheGeneratePSK(uint8_t *pSsid, uint8_t ssidLen, char *pPassphrase,
			 uint8_t *pPSK);

void supplicantGenerateRand(uint8_t *dataOut, uint32_t length);
void ComputeEAPOL_MIC(EAPOL_KeyMsg_t *pKeyMsg, uint16_t data_length,
		      uint8_t *MIC_Key, uint8_t MIC_Key_length,
		      uint8_t micKeyDescVersion);
void supplicantParseWpaIe(IEEEtypes_WPAElement_t *pIe, SecurityMode_t *pWpaType,
			  Cipher_t *pMcstCipher, Cipher_t *pUcstCipher,
			  AkmSuite_t *pAkmList, uint8_t akmOutMax);
void supplicantParseMcstCipher(Cipher_t *pMcstCipherOut, uint8_t *pGrpKeyCipher);
void supplicantParseUcstCipher(Cipher_t *pUcstCipherOut, uint8_t pwsKeyCnt,
			       uint8_t *pPwsKeyCipherList);
void supplicantParseRsnIe(IEEEtypes_RSNElement_t *pRsnIe, SecurityMode_t *pWpaTypeOut,
			  Cipher_t *pMcstCipherOut, Cipher_t *pUcstCipherOut,
			  AkmSuite_t *pAkmListOut, uint8_t akmOutMax,
			  IEEEtypes_RSNCapability_t *pRsnCapOut,
			  Cipher_t *pGrpMgmtCipherOut);

/* the MIC algorithms: hmac_sha1.c (0x2db58), aes_cmac_rom.c (0x2d636), hmac_md5.c (0x2da70) */
void Mrvl_hmac_sha1(uint8_t **ppText, int *pTextLen, int textNum, uint8_t *key,
		    int key_len, uint8_t *output, int outputLen);
void mrvl_aes_cmac(uint8_t *key, uint8_t *input, int length, uint8_t *mac);
void Mrvl_hmac_md5(uint8_t *text_data, int text_len, uint8_t *key, int key_len,
		   uint8_t *digest);

/* pass_phrase.c (0x2c068) */
int Mrvl_PasswordHash(char *password, uint8_t *ssid, int ssidlength,
		      uint8_t *output);

/* the MAC's TSF timer (Mask ROM) */
void msi_wl_GetMCUCoreTimerTxTSF(uint32_t *pValueHi, uint32_t *pValueLo);

#endif /* ROBIN_ESA_H */
