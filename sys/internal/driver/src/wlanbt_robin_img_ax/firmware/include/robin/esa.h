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

/* keyMgmtStaTypes.h: AkmSuite_t.akmType */
enum {
	AKM_NONE = 0,
	AKM_1X = 1,
	AKM_PSK = 2,
	AKM_FT_1X = 3,
	AKM_FT_PSK = 4,
	AKM_SHA256_1X = 5,
	AKM_SHA256_PSK = 6,
};

#define PMKID_LEN	16

typedef struct SecurityParams_t {
	SecurityMode_t wpaType;
	Cipher_t mcstCipher;
	Cipher_t ucstCipher;
} SecurityParams_t;

/* authenticator.h */
typedef struct RSNConfig_t {
	uint8_t RSNEnabled : 1;		/* WPA, WPA2 */
	uint8_t pmkidValid : 1;		/* PMKID valid */
	uint8_t rsnCapValid : 1;
	uint8_t grpMgmtCipherValid : 1;
	uint8_t rsvd : 4;
	SecurityMode_t wpaType;
	Cipher_t mcstCipher;
	Cipher_t ucstCipher;
	AkmSuite_t AKM;
	uint8_t PMKID[16];
	IEEEtypes_RSNCapability_t rsnCap;
	Cipher_t grpMgmtCipher;
} RSNConfig_t;

_Static_assert(sizeof(RSNConfig_t) == 28, "RSN configuration");
_Static_assert(__builtin_offsetof(RSNConfig_t, rsnCap) == 25, "RSN configuration capabilities");

#define ELEM_ID_RSN		48
#define ELEM_ID_VENDOR_SPECIFIC	221

/* KeyApiStaDefs.h */
#define KEY_INFO_MULTICAST	0x1
#define KEY_INFO_UNICAST	0x2
#define KEY_INFO_ENABLED	0x4

/* keyMgmtStaTypes.h */
#define WPA_AES_KEY_LEN		16
#define WPA_TKIP_KEY_LEN	32
#define WPA_WEP104_KEY_LEN	13
#define WPA_WEP40_KEY_LEN	5

#define KEY_TYPE_WEP		0
#define KEY_TYPE_TKIP		1
#define KEY_TYPE_AES		2
#define KEY_TYPE_WAPI		3
#define KEY_TYPE_AES_CMAC	4

typedef struct __attribute__((packed)) key_Type_TKIP_t {
	uint8_t key[16];
	uint8_t txMicKey[8];
	uint8_t rxMicKey[8];
} key_Type_TKIP_t;

typedef struct __attribute__((packed)) key_Type_AES_t {
	uint8_t key[16];
} key_Type_AES_t;

typedef struct __attribute__((packed)) key_Type_WEP_t {
	uint8_t keyIndex;
	uint8_t isDefaultTx;
	uint8_t key[13];
} key_Type_WEP_t;

typedef struct __attribute__((packed)) key_Type_WAPI_t {
	uint8_t keyIndex;
	uint8_t isDefKey;
	uint8_t key[16];
	uint8_t mickey[16];
	uint8_t rxPN[16];
} key_Type_WAPI_t;

typedef struct __attribute__((packed)) key_Type_AES_CMAC_t {
	uint8_t ipn[6];
	uint8_t reserved[2];
	uint8_t key[16];
} key_Type_AES_CMAC_t;

/* this build keeps the WAPI key in the union (install_wpa_none_keys clears 56 bytes) */
typedef struct __attribute__((packed)) key_MgtMaterial_t {
	uint16_t keyType;
	uint16_t keyInfo;
	uint16_t keyLen;
	union __attribute__((packed)) {
		key_Type_TKIP_t TKIP;
		key_Type_AES_t AES;
		key_Type_WEP_t WEP;
		key_Type_WAPI_t WAPI;
		key_Type_AES_CMAC_t iGTK;
	} keyEncypt;
} key_MgtMaterial_t;

_Static_assert(sizeof(key_MgtMaterial_t) == 56, "key material");

/* keyCommonDef.h */
#define TK_SIZE			16

typedef struct KeyData_t {
	uint8_t Key[TK_SIZE];
	uint8_t RxMICKey[8];
	uint8_t TxMICKey[8];
	uint32_t TxIV32;
	uint16_t TxIV16;
	uint16_t KeyIndex;
} KeyData_t;

/* authenticator.h */
typedef enum MIC_Fail_State_e {
	NO_MIC_FAILURE,
	FIRST_MIC_FAIL_IN_60_SEC,
	SECOND_MIC_FAIL_IN_60_SEC
} MIC_Fail_State_e;

typedef struct MIC_Error_t {
	MIC_Fail_State_e status;
	uint8_t MICCounterMeasureEnabled;
	uint32_t disableStaAsso;
} MIC_Error_t;

struct cipher_key_t;

typedef struct keyMgmtInfoSta_t {
	uint8_t ANonce[NONCE_SIZE];
	uint8_t SNonce[NONCE_SIZE];
	uint8_t EAPOL_MIC_Key[EAPOL_MIC_KEY_SIZE];
	uint8_t EAPOL_Encr_Key[EAPOL_ENCR_KEY_SIZE];
	uint32_t apCounterLo;		/* last valid replay counter from authenticator */
	uint32_t apCounterHi;
	uint32_t apCounterZeroDone;	/* have we processed replay == 0? */
	uint32_t staCounterLo;		/* counter used in request EAPOL frames */
	uint32_t staCounterHi;
	uint8_t RSNDataTrafficEnabled;	/* enabled after the 4-way handshake */
	uint8_t RSNSecured;		/* enabled after the group key is established */
	uint8_t pwkHandshakeComplete;
	struct cipher_key_t *pRxDecryptKey;
	KeyData_t PWKey;
	KeyData_t GRKey;
	KeyData_t newPWKey;
	MIC_Error_t sta_MIC_Error;
	void *rsnTimer;
	void *micTimer;
	void *deauthDelayTimer;		/* delays the deauthentication */
	KeyData_t IGtk;
} keyMgmtInfoSta_t;

_Static_assert(__builtin_offsetof(keyMgmtInfoSta_t, apCounterZeroDone) == 104, "replay counters");

#define EAPOL_MIC_SIZE				16
#define IEEE_8021X_PACKET_TYPE_EAPOL_KEY	3
#define ETH_P_EAPOL_SWAPPED			0x8e88	/* 0x888e in network order */

/* IEEE_types.h */
typedef struct __attribute__((packed)) IEEEtypes_InfoElementHdr_t {
	uint8_t ElementId;
	uint8_t Len;
} IEEEtypes_InfoElementHdr_t;

enum {
	KDE_DATA_TYPE_RESERVED,
	KDE_DATA_TYPE_GTK = 1,
	KDE_DATA_TYPE_RESERVED2,
	KDE_DATA_TYPE_MACADDR = 3,
	KDE_DATA_TYPE_PMKID = 4,
	KDE_DATA_TYPE_SMK = 5,
	KDE_DATA_TYPE_NONCE = 6,
	KDE_DATA_TYPE_LIFETIME = 7,
	KDE_DATA_TYPE_ERROR = 8,
	KDE_DATA_TYPE_IGTK = 9,
};

/* keyCommonDef.h: WPA2 GTK IE */
typedef struct __attribute__((packed)) GTK_KDE_t {
	uint8_t KeyID : 2;
	uint8_t Tx : 1;
	uint8_t rsvd : 5;
	uint8_t rsvd1;
	uint8_t GTK[1];
} GTK_KDE_t;

/* WPA2 key data */
typedef struct __attribute__((packed)) KDE_t {
	uint8_t type;
	uint8_t length;
	uint8_t OUI[3];
	uint8_t dataType;
	uint8_t data[1];
} KDE_t;

/* authenticator.h */
typedef struct TkipPtk_t {
	uint8_t kck[16];	/* PTK_KCK = L(PTK,   0, 128) */
	uint8_t kek[16];	/* PTK_KEK = L(PTK, 128, 128) */
	uint8_t tk[16];		/* PTK_TK  = L(PTK, 256, 128) */
	uint8_t rxMicKey[8];
	uint8_t txMicKey[8];
} TkipPtk_t;

#define PMK_LEN_MAX	32

/* sha1.c (0x2dc5c), mrvl_sha256_crypto.c (0x2e7c8) */
void Mrvl_PRF(unsigned char *key, int key_len, unsigned char *prefix, int prefix_len,
	      unsigned char *data, int data_len, unsigned char *output, int len);
void mrvl_sha256_crypto_kdf(uint8_t *pKey, uint8_t key_len, char *label,
			    uint8_t label_len, uint8_t *pContext, uint16_t context_len,
			    uint8_t *pOutput, uint16_t output_len);

/* crypt_new_rom.c, rc4.c */
int MRVL_AesUnWrap(uint8_t *kek, uint8_t kekLen, uint32_t n, uint8_t *cipher,
		   uint8_t *keyIv, uint8_t *plain);
void RC4_Encrypt(uint8_t *Encr_Key, uint8_t *IV, uint16_t iv_length, uint8_t *Data,
		 uint16_t data_length, uint16_t skipBytes);

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

/* keyMgmtSta_rom.c */
void supplicantSetAssocRsn_internal(RSNConfig_t *pRsnConfig,
				    SecurityParams_t *pSecurityParams,
				    SecurityMode_t wpaType, Cipher_t *pMcstCipher,
				    Cipher_t *pUcstCipher, AkmSuite_t *pAkm,
				    IEEEtypes_RSNCapability_t *pRsnCap,
				    Cipher_t *pGrpMgmtCipher);
void supplicantGenerateSha1Pmkid(uint8_t *pPMK, IEEEtypes_MacAddr_t *pBssid,
				 IEEEtypes_MacAddr_t *pSta, uint8_t *pPMKID);
void supplicantGenerateSha256Pmkid(uint8_t *pPMK, IEEEtypes_MacAddr_t *pBssid,
				   IEEEtypes_MacAddr_t *pSta, uint8_t *pPMKID);
uint16_t keyMgmtFormatWpaRsnIe_internal(RSNConfig_t *pRsnConfig, uint8_t *pos,
					IEEEtypes_MacAddr_t *pBssid,
					IEEEtypes_MacAddr_t *pStaAddr,
					uint8_t *pPmkid, int addPmkid);
void install_wpa_none_keys_internal(key_MgtMaterial_t *pKeyMgtData, uint8_t *pPMK,
				    uint8_t type, uint8_t unicast);
uint16_t keyMgmtGetKeySize_internal(RSNConfig_t *pRsnConfig, uint8_t isPairwise);
int isApReplayCounterFresh(keyMgmtInfoSta_t *pKeyMgmtInfoSta, uint8_t *pRxReplayCount);
void updateApReplayCounter(keyMgmtInfoSta_t *pKeyMgmtStaInfo, uint8_t *pRxReplayCount);
void FillKeyMaterialStruct_internal(key_MgtMaterial_t *pKeyMgtData, uint16_t key_len,
				    uint8_t isPairwise, KeyData_t *pKey);
void formEAPOLEthHdr(EAPOL_KeyMsg_Tx_t *pTxEapol, uint8_t *da, uint8_t *sa);
int IsEAPOL_MICValid(EAPOL_KeyMsg_t *pKeyMsg, uint8_t *pMICKey);
uint16_t KeyMgmtSta_PopulateEAPOLLengthMic(EAPOL_KeyMsg_Tx_t *pTxEapol,
					   uint8_t *pEAPOLMICKey,
					   uint8_t eapolProtocolVersion,
					   uint8_t forceKeyDescVersion);
KDE_t *parseKeyKDE(IEEEtypes_InfoElementHdr_t *pIe);
KDE_t *parseKeyKDE_DataType(uint8_t *pData, int32_t dataLen, int KDEDataType);
KDE_t *parseKeyDataGTK(uint8_t *pKey, uint16_t len, KeyData_t *pGRKey);
void KeyMgmtSta_ApplyKEK(EAPOL_KeyMsg_t *pKeyMsg, KeyData_t *pGRKey,
			 uint8_t *EAPOL_Encr_Key);
int KeyMgmtSta_IsRxEAPOLValid(keyMgmtInfoSta_t *pKeyMgmtInfoSta, EAPOL_KeyMsg_t *pKeyMsg);
void KeyMgmtSta_PrepareEAPOLFrame(EAPOL_KeyMsg_Tx_t *pTxEapol, EAPOL_KeyMsg_t *pRxEapol,
				  uint8_t *da, uint8_t *sa, uint8_t *pSNonce);
void KeyMgmtSta_PrepareEAPOLMicErrFrame(EAPOL_KeyMsg_Tx_t *pTxEapol, int isUnicast,
					IEEEtypes_MacAddr_t *da, IEEEtypes_MacAddr_t *sa,
					keyMgmtInfoSta_t *pKeyMgmtInfoSta);
int supplicantAkmUsesKdf(AkmSuite_t *pAkm);
void supplicantConstructContext(uint8_t *pAddr1, uint8_t *pAddr2, uint8_t *pNonce1,
				uint8_t *pNonce2, uint8_t *pContext);
void KeyMgmt_DerivePTK(uint8_t *pAddr1, uint8_t *pAddr2, uint8_t *pNonce1,
		       uint8_t *pNonce2, uint8_t *pPTK, uint8_t *pPMK, int use_kdf);
void KeyMgmtSta_DeriveKeys(uint8_t *pPMK, uint8_t *da, uint8_t *sa, uint8_t *ANonce,
			   uint8_t *SNonce, uint8_t *EAPOL_MIC_Key, uint8_t *EAPOL_Encr_Key,
			   KeyData_t *newPWKey, int use_kdf);
int supplicantAkmIsWpa2(AkmSuite_t *pAkm);
int supplicantAkmUsesSha256Pmkid(AkmSuite_t *pAkm);
int supplicantAkmIsWpaWpa2(AkmSuite_t *pAkm);
int supplicantAkmIsWpaWpa2Psk(AkmSuite_t *pAkm);
int supplicantGetPmkid(IEEEtypes_MacAddr_t *pBssid, IEEEtypes_MacAddr_t *pStaAddr,
		       AkmSuite_t *pAkm, uint8_t *pPMKID);

/* the pool of 500-byte buffers the key derivation works in */
struct OsaBufferPool;
extern struct OsaBufferPool *ramHook_encrPoolConfig;

/* mrvl_sha256_crypto.c (0x2e790) */
int mrvl_sha256_crypto_vector(int num_elem, uint8_t *addr[], int *len, uint8_t *mac);

/* pass_phrase.c (0x2c068) */
int Mrvl_PasswordHash(char *password, uint8_t *ssid, int ssidlength,
		      uint8_t *output);

/* the MAC's TSF timer (Mask ROM) */
void msi_wl_GetMCUCoreTimerTxTSF(uint32_t *pValueHi, uint32_t *pValueLo);

#endif /* ROBIN_ESA_H */
