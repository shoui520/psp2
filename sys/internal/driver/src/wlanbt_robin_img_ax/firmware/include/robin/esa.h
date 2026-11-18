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

#include "robin/buffer.h"
#include "robin/esatypes.h"
#include "robin/md5.h"
#include "robin/sha1.h"
#include "robin/sha256.h"

#define MAX_PMK_SIZE		32
#define BSSID_FLAG		0xff	/* pmkElement_t.length of a PMK keyed by BSSID */

typedef uint8_t IEEEtypes_MacAddr_t[6];

#define EAPOL_MIC_SIZE		16
#define IEEE_8021X_PACKET_TYPE_EAPOL_KEY 3



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



/*
 * keyCommonDef.h: the header of a key the MAC installed. Marvell's splits the
 * byte after keyDirection into keyType:4 and version:4; this firmware reads
 * and writes it as one byte, the key type.
 */
typedef struct cipher_key_hdr_t {
	IEEEtypes_MacAddr_t macAddr;
	uint8_t keyDirection;
	uint8_t keyType;
	uint16_t keyLen;
	uint8_t keyState;
	uint8_t keyInfo;
} cipher_key_hdr_t;

_Static_assert(sizeof(cipher_key_hdr_t) == 12, "cipher key header");

/* keyApiStaTypes.h: cipher_key_hdr_t.keyState */
#define KEY_STATE_ENABLE			0x01
#define KEY_STATE_FORCE_EAPOL_UNENCRYPTED	0x02

/* keyApiStaTypes.h: cipher_key_hdr_t.keyInfo */
#define KEY_INFO_PWK				0x01
#define KEY_INFO_GWK				0x02
#define KEY_INFO_KEY_MAPPING_KEY		0x04
#define KEY_INFO_WEP_SUBTYPE			0x08	/* a WEP key of 104 bits */
#define KEY_INFO_GLOBAL				0x10

struct WlanKeyBuffer;

/*
 * The key data union (ckd) follows the header. TKIP and AES keep their key
 * with the receive and send counters (the next packet number to send is
 * txIv32:txIv16); WAPI keeps two keys, the packet number step and the send
 * and receive packet numbers, least significant byte first, and a copy of
 * the key it replaced (Marvell's pLastKey: "keep the orig cipher_key_t
 * pointer"). WEP keeps the four default keys (wl_mib_rom.h's
 * MIB_WEP_DEFAULT_KEYS) and the index of the one frames are sent with; an
 * access point's pairwise key holds the handshake's data until the key is
 * installed. The layout is read off the frame paths.
 */
typedef struct __attribute__((packed)) cipher_key_wapi_t {
	uint8_t keyIndex;
	uint8_t dataKey[16];
	uint8_t micKey[16];
	uint8_t pnStep;			/* 2 for a unicast key, 1 for a group key */
	uint8_t txPn[16];
	uint8_t rxPn[16];
	struct WlanKeyBuffer *pLastKey;	/* the key this one replaced, or NULL (unaligned) */
} cipher_key_wapi_t;

/* wl_mib_rom.h */
typedef struct MIB_WepDefaultKeys_s {
	uint8_t WepDefaultKeyIdx;
	uint8_t WepDefaultKeyType;	/* 1: 104 bits */
	uint8_t WepDefaultKeyValue[13];
} MIB_WEP_DEFAULT_KEYS;

#define MAX_WEP_KEYS	4

/* keyCommonDef.h */
typedef struct wep_key_data_t {
	MIB_WEP_DEFAULT_KEYS WepDefaultKeys[MAX_WEP_KEYS];
	uint8_t default_key_idx;
	uint8_t keyCfg;
	uint8_t Reserved;
} wep_key_data_t;

typedef struct cipher_key_aes_t {
	uint8_t key[16];
	uint8_t txMicKey[8];
	uint8_t rxMicKey[8];
	uint32_t rxIv32;
	uint16_t rxIv16;
	uint8_t reserved50[2];
	uint32_t txIv32;
	uint16_t txIv16;
	uint8_t reserved58[2];
	uint32_t decryptState[5];	/* where wlanCcmpDecrypt() keeps a fragment's state for the next one */
} cipher_key_aes_t;

/* Not packed: Sony reads the counters with word and halfword loads. */
typedef struct cipher_key_t {
	cipher_key_hdr_t hdr;
	union {
		cipher_key_aes_t aes;		/* also TKIP */
		cipher_key_wapi_t wapi;
		wep_key_data_t wep;
		eapolHskData_t hskData;
	} ckd;
} cipher_key_t;

_Static_assert(sizeof(cipher_key_t) == 84, "cipher key");
_Static_assert(__builtin_offsetof(cipher_key_t, ckd.aes.txIv32) == 52, "send counter");
_Static_assert(__builtin_offsetof(cipher_key_t, ckd.aes.decryptState) == 60, "decryption state");
_Static_assert(__builtin_offsetof(cipher_key_t, ckd.wapi.rxPn) == 62, "WAPI receive packet number");
_Static_assert(__builtin_offsetof(cipher_key_t, ckd.wapi.pLastKey) == 78, "WAPI previous key");
_Static_assert(__builtin_offsetof(cipher_key_t, ckd.wep.default_key_idx) == 72, "WEP default key");
_Static_assert(__builtin_offsetof(cipher_key_t, ckd.hskData.pwsKeyData) == 44, "handshake key data");

/* A key the MAC installed, in a buffer of the key pools: the buffer's header, then the key (wlanKeyBufferAlloc()). */
struct WlanKeyBuffer {
	OsaBuffer buffer;
	union {
		cipher_key_t key;
		uint8_t keyData[84];
	};
};

_Static_assert(sizeof(struct WlanKeyBuffer) == 112, "key buffer");

struct WlanKeyBuffer *wlanKeyBufferAlloc(void);				/* SQRAM 0xc000824c */
void wlanKeyBufferFree(struct WlanKeyBuffer *key);				/* SQRAM 0xc0008292 */
int wlanKeyPoolsInit(void);							/* SQRAM 0xc00082bc */

#define KEY_DIRECTION_RX	0x02	/* cipher_key_hdr_t.keyDirection: the key decrypts received frames (inferred) */
#define KEY_DIRECTION_RXTX	0x03	/* keyApiStaTypes.h */

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
	BOOLEAN RSNDataTrafficEnabled;	/* enabled after the 4-way handshake */
	BOOLEAN RSNSecured;		/* enabled after the group key is established */
	BOOLEAN pwkHandshakeComplete;
	struct cipher_key_t *pRxDecryptKey;
	KeyData_t PWKey;
	KeyData_t GRKey;
	KeyData_t newPWKey;
	MIC_Error_t sta_MIC_Error;
	uint32_t rsnTimer;
	uint32_t micTimer;
	uint32_t deauthDelayTimer;	/* delays the deauthentication */
	void *psapriv;			/* commented out in the 8977's */
	KeyData_t IGtk;
} keyMgmtInfoSta_t;

_Static_assert(__builtin_offsetof(keyMgmtInfoSta_t, GRKey) == 172, "group key");
_Static_assert(__builtin_offsetof(keyMgmtInfoSta_t, sta_MIC_Error) == 252, "MIC error");
_Static_assert(__builtin_offsetof(keyMgmtInfoSta_t, psapriv) == 276, "supplicant context");
_Static_assert(__builtin_offsetof(keyMgmtInfoSta_t, apCounterZeroDone) == 104, "replay counters");
_Static_assert(sizeof(keyMgmtInfoSta_t) == 320, "station key management");

/* IEEE_types.h */
typedef struct __attribute__((packed)) IEEEtypes_SsIdElement_t {
	uint8_t ElementId;
	uint8_t Len;
	uint8_t SsId[32];
} IEEEtypes_SsIdElement_t;

/* authenticator.h */
typedef struct customMIB_RSNStats_t {
	uint8_t TKIPICVErrors;
	uint8_t TKIPLocalMICFailures;
	uint8_t TKIPCounterMeasuresInvoked;
} customMIB_RSNStats_t;

/*
 * The one station supplicant (authenticator.h's supplicantData). This build
 * keeps no inUse or suppInitialized, and holds the statistics, the profile and
 * the addresses apart in initialised data (keyMgmtSta.c).
 */
typedef struct supplicantData_t {
	IEEEtypes_SsIdElement_t hashSsId;
	RSNConfig_t customMIB_RSNConfig;
	keyMgmtInfoSta_t keyMgmtInfoSta;
} supplicantData_t;

_Static_assert(__builtin_offsetof(supplicantData_t, customMIB_RSNConfig) == 0x22, "RSN configuration");
_Static_assert(__builtin_offsetof(supplicantData_t, keyMgmtInfoSta) == 0x40, "key management");
_Static_assert(sizeof(supplicantData_t) == 0x180, "supplicant");

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


/* sha1.c (robin/sha1.h: Mrvl_PRF 0x2dc5c), mrvl_sha256_crypto.c (robin/sha256.h) */

/* crypt_new_rom.c, rc4.c */
int MRVL_AesEncrypt(uint8_t *kek, uint8_t kekLen, uint8_t *data, uint8_t *ret);
int MRVL_AesUnWrap(uint8_t *kek, uint8_t kekLen, uint32_t n, uint8_t *cipher,
		   uint8_t *keyIv, uint8_t *plain);
int MRVL_AesWrap(uint8_t *kek, uint8_t kekLen, uint32_t n, uint8_t *plain,
		 uint8_t *keyIv, uint8_t *cipher);
void RC4_Encrypt(uint8_t *Encr_Key, uint8_t *IV, uint16_t iv_length, uint8_t *Data,
		 uint16_t data_length, uint16_t skipBytes);

/* pass_phrase.c (0x2bfc0, 0x2c068): the pre-shared key of a pass phrase and an SSID */
void Mrvl_F(char *password, uint8_t *ssid, int ssidlength, int iterations, int count,
	    uint8_t *output);
int Mrvl_PasswordHash(char *password, uint8_t *ssid, int ssidlength, uint8_t *output);

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
uint8_t *pmkCacheFindPSK(uint8_t *pSsid, uint8_t ssidLen);	/* SQRAM 0xc0008190 */
void pmkCacheInit(void);					/* SQRAM 0xc00081d4 */
void pmkCacheFlush(void);					/* SQRAM 0xc00081f0 */
void pmkCacheRamInit(void);					/* SQRAM 0xc0008204 */

/*
 * pmkCache.c: the host's SUPPLICANT_PMK command (0x00c4) on the cache. The command body is a
 * run of TLVs (SSID, BSSID, passphrase, PMK); a query appends its answer to the run, whose
 * length is passed by pointer. The firmware's versions take the TLVs where Marvell's driver
 * has an mlan_ds_passphrase.
 */
extern void (*ramHook_supplicantEnable)(void);					/* 0x54ea0 */
extern uint8_t *(*ramHook_pmkCacheFindPSK)(uint8_t *pSsid, uint8_t ssidLen);	/* 0x54ea4 */
extern uint8_t *ramHook_PSKPassPhrase;						/* 0x548a8 */
unsigned int SupplicantSetPassphrase(const uint8_t *buffer, int length);	/* 0x2c688 */
unsigned int SupplicantQueryPassphrase(uint8_t *buffer, uint16_t *length);	/* 0x2c790 */
int SupplicantClearPMK_internal(const uint8_t *buffer, int length);		/* 0x2c88a */

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

/* the MIC algorithms: hmac_sha1.c (0x2db58), aes_cmac_rom.c (0x2d636), hmac_md5.c (0x2da70; robin/md5.h) */
void mrvl_aes_cmac(const uint8_t *key, const uint8_t *input, int length, uint8_t *mac);		/* 0x2d636 */

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
void SetEAPOLKeyDescTypeVersion(EAPOL_KeyMsg_Tx_t *pTxEapol, BOOLEAN isWPA2,
				BOOLEAN isKDF, BOOLEAN nonTKIP);
EAPOL_KeyMsg_t *GetKeyMsgNonceFromEAPOL(OsaBuffer *pmbuf, keyMgmtInfoSta_t *pKeyMgmtInfoSta);
EAPOL_KeyMsg_t *ProcessRxEAPOL_PwkMsg3(OsaBuffer *pmbuf, keyMgmtInfoSta_t *pKeyMgmtInfoSta);
EAPOL_KeyMsg_t *ProcessRxEAPOL_GrpMsg1(OsaBuffer *pmbuf, keyMgmtInfoSta_t *pKeyMgmtInfoSta);
void KeyMgmtResetCounter(keyMgmtInfoSta_t *pKeyMgmtInfo);
void MicErrTimerExp_Sta(uint32_t timerId, keyMgmtInfoSta_t *pKeyMgmtInfo);
void DeauthDelayTimerExp_Sta(uint32_t timerId, keyMgmtInfoSta_t *pKeyMgmtInfoSta);
void keyMgmtStaRsnSecuredTimeoutHandler(uint32_t timerId, keyMgmtInfoSta_t *pKeyMgmtInfoSta);

/* microTimerStart's callback: the timer and its argument */
typedef void (*MicroTimerCallback_t)(uint32_t timerId, uint32_t data);

void keyMgmtSta_StartSession_internal(keyMgmtInfoSta_t *pKeyMgmtInfoSta,
				      MicroTimerCallback_t callback, uint32_t expiry,
				      uint8_t flags);
void KeyMgmtSta_handleMICDeauthTimer(keyMgmtInfoSta_t *pKeyMgmtInfoSta,
				     MicroTimerCallback_t callback, uint32_t expiry,
				     uint8_t flags);
void KeyMgmtSta_handleMICErr(uint8_t state, keyMgmtInfoSta_t *pKeyMgmtInfoSta,
			     MicroTimerCallback_t callback, uint8_t flags);
void KeyMgmtSta_InitSession(keyMgmtInfoSta_t *pKeyMgmtInfoSta);
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


/* pass_phrase.c (0x2c068) */
int Mrvl_PasswordHash(char *password, uint8_t *ssid, int ssidlength,
		      uint8_t *output);

/* the MAC's TSF timer (Mask ROM) */
void msi_wl_GetMCUCoreTimerTxTSF(uint32_t *pValueHi, uint32_t *pValueLo);

/* keyMgmtSta.c: the station supplicant around the ROM's key management */
struct WlanConnection;

extern customMIB_RSNStats_t customMIB_RSNStats;
extern SecurityParams_t currParams;
extern IEEEtypes_MacAddr_t localBssid;
extern IEEEtypes_MacAddr_t localStaAddr;
extern supplicantData_t suppData;

/* keyMgmtSta_rom.c's RAM hooks, set by keyMgmtSta_RomInit() */
extern int (*ramHook_keyMgmtProcessMsgExt)();
extern int (*ramHook_keyMgmtSendDeauth)();

OsaBuffer *GetTxEAPOLBuffer(struct WlanConnection *connPtr, EAPOL_KeyMsg_Tx_t **ppTxEapol);
void keyMgmtSendDeauth2Peer(struct WlanConnection *connPtr, uint16_t reason);
BOOLEAN keyMgmtProcessMsgExt(EAPOL_KeyMsg_t *pKeyMsg);
uint16_t keyMgmtFormatWpaRsnIe(uint8_t *pos, IEEEtypes_MacAddr_t *pBssid,
			       IEEEtypes_MacAddr_t *pStaAddr, uint8_t *pPmkid, BOOLEAN addPmkid);
int GeneratePWKMsg2(OsaBuffer *pmbuf, uint8_t *pSNonce, uint8_t *pEAPOLMICKey,
		    uint8_t forceKeyDescVersion);
uint16_t keyMgmtGetKeySize(uint8_t isPairwise);
void FillKeyMaterialStruct(struct WlanConnection *connPtr, uint16_t key_len, uint8_t isPairwise,
			   KeyData_t *pKey);
void keyMgmtPlumbPairwiseKey(struct WlanConnection *connPtr);
int keyMgmtKeyPairwiseTxDone(OsaBuffer *pBufDesc);
int keyMgmtKeyGroupTxDone(OsaBuffer *pBufDesc);
int GeneratePWKMsg4(OsaBuffer *pmbuf, keyMgmtInfoSta_t *pKeyMgmtInfoSta, BOOLEAN groupKeyReceived);
int GenerateGrpMsg2(OsaBuffer *pmbuf, keyMgmtInfoSta_t *pKeyMgmtInfoSta);
BOOLEAN KeyMgmtStaHsk_Recvd_PWKMsg1(OsaBuffer *pmbuf, IEEEtypes_MacAddr_t *sa,
				    IEEEtypes_MacAddr_t *da);
EAPOL_KeyMsg_t *KeyMgmtStaHsk_Recvd_PWKMsg3(OsaBuffer *pmbuf);
int KeyMgmtStaHsk_Recvd_GrpMsg1(OsaBuffer *pmbuf);
void ProcessKeyMgmtDataSta(OsaBuffer *pmbuf, IEEEtypes_MacAddr_t *sa, IEEEtypes_MacAddr_t *da);
void supplicantMICCounterMeasureInvoke(struct WlanConnection *connPtr, BOOLEAN isUnicast);
void wlanMicErrorReport(struct WlanConnection *connection, int group);	/* 0x1b504 */
void keyMgmtSta_StartSession(struct WlanConnection *connPtr, IEEEtypes_MacAddr_t *pBssid,
			     IEEEtypes_MacAddr_t *pStaAddr);
void supplicantInitSession(struct WlanConnection *connPtr, uint8_t *pSsid, uint16_t len,
			   IEEEtypes_MacAddr_t *pBssid, IEEEtypes_MacAddr_t *pStaAddr);
uint8_t supplicantIsCounterMeasuresActive(void);
void init_customApp_mibs(void);
SecurityMode_t supplicantCurrentSecurityMode(void);
int supplicantIsEnabled(void);
void supplicantDisable(void);
void supplicantEnable(void);
void supplicantSetAssocRsn(SecurityMode_t wpaType, Cipher_t *pMcstCipher, Cipher_t *pUcstCipher,
			   AkmSuite_t *pAkm, IEEEtypes_RSNCapability_t *pRsnCap,
			   Cipher_t *pGrpMgmtCipher);
void install_wpa_none_keys(struct WlanConnection *connPtr, uint8_t type, uint8_t unicast);
void supplicantInstallWpaNoneKeys(struct WlanConnection *connPtr);
void supplicantSetProfile(SecurityMode_t wpaType, Cipher_t mcstCipher, Cipher_t ucstCipher);
void supplicantGetProfile(SecurityMode_t *pWpaType, Cipher_t *pMcstCipher, Cipher_t *pUcstCipher);
void supplicantGetProfileCurrent(SecurityMode_t *pWpaType, Cipher_t *pMcstCipher,
				 Cipher_t *pUcstCipher);
void supplicantInit(void);
void supplicantStopSessionTimer(void);
void supplicantSmeResetNotification(void);
void keyMgmtSuccessfulDecryptNotify(cipher_key_t *pRxCipherKey);
void keyMgmtSta_RomInit(void);
uint8_t *parseKeyDataField(uint8_t *pKey, uint16_t len);
uint16_t btampAddRsnIe(IEEEtypes_RSNElement_t *pRsnIe);
int keyMgmtKeyPairAndGroupTxDone(OsaBuffer *pBufDesc);
void SendMICFailReport_sta(struct WlanConnection *connPtr, keyMgmtInfoSta_t *pKeyMgmtInfoSta,
			   BOOLEAN isUnicast);

/*
 * The key API: the firmware's half of what Marvell's host port keeps as
 * keyApi_UpdateKeyMaterial() and keyApi_ApUpdateKeyMaterial() (keyMgmtSta.c,
 * keyMgmtAp.c of the 8977's, whose firmware-only lines are comments there).
 * The installing half is in ITCM (esa/keyApiSta_rom.c), the rest in SQRAM
 * (sqram/keyapi.c). Names other than Marvell's are inferred.
 */
typedef void (*keyApiSta_ConnHook_t)(struct WlanConnection *connPtr, BOOLEAN enable);

/* The RAM hooks the ITCM half calls through (0x54ef8 .. 0x54f14; keyApiSta_RomInit()). */
extern void (*ramHook_keyApi_PalladiumHook1)(struct WlanConnection *connPtr);
extern keyApiSta_ConnHook_t ramHook_keyApiSta_setConnStaticWepEnabled;	/* WLAN_CONN_HOST_FLAG_8 */
extern keyApiSta_ConnHook_t ramHook_keyApiSta_setConnExcludeUnencrypted;	/* txOptions bit 2: an unprotected frame is dropped */
extern keyApiSta_ConnHook_t ramHook_keyApiSta_setConnCurPktTxEnabled;	/* WLAN_CONN_ENCRYPTED */
extern keyApiSta_ConnHook_t ramHook_keyApiSta_setConnDataTrafficEnabled;	/* WLAN_CONN_JOIN_OPEN */
extern keyApiSta_ConnHook_t ramHook_keyApiSta_nullHook;
extern BOOLEAN (*ramHook_keyApiSta_isConnStaticWepEnabled)(struct WlanConnection *connPtr);
extern keyApiSta_ConnHook_t ramHook_keyApiSta_setConnWapiEnabled;		/* WLAN_CONN_WAPI */

/* esa/keyApiSta_rom.c */
void keyApiSta_DisableKeys(struct WlanConnection *connPtr, cipher_key_t *pwkey, cipher_key_t *gwkey,
			   BOOLEAN pairwise, BOOLEAN group);					/* 0x2e2dc */
uint32_t keyApiSta_UpdateKeyMaterial(struct WlanConnection *connPtr, cipher_key_t *pwkey,
				     cipher_key_t *gwkey, key_MgtMaterial_t *keyMgtData_p);	/* 0x2e33c */
uint16_t keyApiSta_GetKeyMaterial(cipher_key_t *pwkey, cipher_key_t *gwkey,
				  key_MgtMaterial_t *keyMgtData_p, BOOLEAN pairwise);	/* 0x2e602 */
void keyApiSta_SetKeyFromKeyData(BOOLEAN groupKey, cipher_key_t *key, Cipher_t *cipher,
				 KeyData_t *keyData);						/* 0x2e726 */

/* sqram/keyapi.c */
void keyApi_PalladiumHook1(struct WlanConnection *connPtr);				/* SQRAM 0xc0008304 */
void keyApiSta_NullHook(struct WlanConnection *connPtr, BOOLEAN enable);		/* SQRAM 0xc0008306 */
BOOLEAN keyApiSta_isConnStaticWepEnabled(struct WlanConnection *connPtr);		/* SQRAM 0xc0008308 */
void keyApiSta_setConnStaticWepEnabled(struct WlanConnection *connPtr, BOOLEAN enable);	/* SQRAM 0xc0008316 */
void keyApiSta_setConnExcludeUnencrypted(struct WlanConnection *connPtr, BOOLEAN enable);	/* SQRAM 0xc000832c */
void keyApiSta_setConnCurPktTxEnabled(struct WlanConnection *connPtr, BOOLEAN enable);	/* SQRAM 0xc0008342 */
void keyApiSta_setConnDataTrafficEnabled(struct WlanConnection *connPtr, BOOLEAN enable);	/* SQRAM 0xc0008356 */
void keyApiSta_setConnWapiEnabled(struct WlanConnection *connPtr, BOOLEAN enable);	/* SQRAM 0xc000836a */
void keyApi_DisableKeys(struct WlanConnection *connPtr, BOOLEAN pairwise, BOOLEAN group);	/* SQRAM 0xc000837e */
void keyApi_FreeKey(struct WlanKeyBuffer **keyBuffer);					/* SQRAM 0xc00083b0 */
uint32_t keyApi_UpdateKeyMaterial(struct WlanConnection *connPtr, key_MgtMaterial_t *keyMgtData_p);	/* SQRAM 0xc000869e */
uint16_t keyApi_GetKeyMaterial(struct WlanConnection *connPtr, key_MgtMaterial_t *keyMgtData_p,
			       BOOLEAN pairwise);						/* SQRAM 0xc0008718 */
void keyApiSta_RomInit(void);								/* SQRAM 0xc0008754 */
struct WlanConnectionInfo;
void keyApi_ApReleaseGroupKey(struct WlanConnectionInfo *info);				/* SQRAM 0xc0008854 */

/* Network byte order; the EAPOL-Key fields are big-endian. */
static inline uint32_t ntohl(uint32_t x)
{
	return (x << 24) + ((x & 0xff00) << 8) + ((x & 0xff0000) >> 8) + (x >> 24);
}

static inline uint16_t ntohs(uint16_t x)
{
	return (uint16_t)((x << 8) | (x >> 8));
}

#define htons ntohs
#define htonl ntohl

/* esa/btampKeyMgmt.c */
extern keyMgmtInfoSta_t btampKeyMgmtInfo;
void keyMgmtSendDeauth(struct WlanConnection *connPtr, IEEEtypes_MacAddr_t *pPeer, uint16_t reason);
void btampKeyMgmtInit(void);
int btampGeneratePWKMsg1(struct WlanConnection *connPtr);
void btampProcessKeyMgmtData(OsaBuffer *pBufDesc);

#endif /* ROBIN_ESA_H */
