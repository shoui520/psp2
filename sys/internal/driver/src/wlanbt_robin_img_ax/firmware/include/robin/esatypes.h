/*
 * The types of Marvell's embedded supplicant and authenticator (robin/esa.h)
 * that the connection blocks hold (robin/conn.h): the security mode and
 * ciphers, the keys the handshakes derive, and the access point's
 * per-station key management state. Marvell's names (keyMgmtStaTypes.h,
 * keyCommonDef.h, authenticator.h, keyMgmtApTypes.h of the host port of the
 * same code, GPL-2.0), with this build's layout.
 */
#ifndef ROBIN_ESATYPES_H
#define ROBIN_ESATYPES_H

#include <stdint.h>

#define NONCE_SIZE		32
#define EAPOL_MIC_KEY_SIZE	16
#define EAPOL_ENCR_KEY_SIZE	16
#define PSK_PASS_PHRASE_LEN_MAX	64
#define PMK_LEN_MAX		32

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

/*
 * This build's BOOLEAN is 32 bits and its enums take the smallest type that
 * holds them (armcc), so the layout differs from the 8977's.
 */
typedef uint32_t BOOLEAN;

typedef struct MIC_Error_t {
	uint8_t status;			/* MIC_Fail_State_e */
	BOOLEAN MICCounterMeasureEnabled;
	uint32_t disableStaAsso;
} MIC_Error_t;

/* keyMgmtApTypes_rom.h: the states of an access point's key handshake with one station. */
typedef enum keyMgmtState_e {
	HSK_NOT_STARTED,
	WAITING_4_MSG2,			/* message 1 of the 4-way handshake is out */
	WAITING_4_MSG4,			/* message 3 is out */
	WAITING_4_GRPMSG2,		/* the group key handshake's message 1 is out */
	WAITING_4_GRP_REKEY_MSG2,	/* a group rekey's message 1 is out */
	HSK_END,
} keyMgmtState_e;

/*
 * keyMgmtApTypes.h: what the access point keeps of one station's handshake
 * (the 8977's has a state per message and the EAPOL version after these).
 * It starts at offset 36 of the connection's role block.
 */
typedef struct apKeyMgmtInfoSta_t {
	uint16_t staRsnCap;
	SecurityMode_t staSecType;
	Cipher_t staUcstCipher;
	uint8_t staAkmType;
	uint8_t keyMgmtState;		/* keyMgmtState_e */
	uint8_t numHskTries;
	uint32_t counterLo;		/* the replay counter of the EAPOL-Key frames sent */
	uint32_t counterHi;
	uint32_t HskTimer;		/* osaCreateTimer() handle */
	uint8_t EAPOL_MIC_Key[EAPOL_MIC_KEY_SIZE];
	uint8_t EAPOL_Encr_Key[EAPOL_ENCR_KEY_SIZE];
} apKeyMgmtInfoSta_t;

_Static_assert(sizeof(apKeyMgmtInfoSta_t) == 52, "access point station key management");

/*
 * What an access point keeps of one station for the handshake's keys: the
 * pointer the station's connection holds to it (conn.h).
 */
typedef struct eapolHskData_t {
	uint8_t reserved0[40];
	uint8_t ANonce[NONCE_SIZE];
	KeyData_t pwsKeyData;		/* the pairwise key being derived */
} eapolHskData_t;

_Static_assert(sizeof(eapolHskData_t) == 112, "handshake data");

/* authenticator.h: the access point's RSN settings; this build's has no replay protection flag. */
typedef struct Operation_t {
	uint16_t keyExchange : 1;
	uint16_t authenticate : 1;
	uint16_t reserved : 14;
} Operation_t;

typedef struct apRsnConfig_t {
	Cipher_t mcstCipher;
	uint8_t mcstCipherCount;
	Cipher_t wpaUcstCipher;
	uint8_t wpaUcstCipherCount;
	Cipher_t wpa2UcstCipher;
	uint8_t wpa2UcstCipherCount;
	uint16_t AuthKey;
	uint16_t AuthKeyCount;
	Operation_t Akmp;
	uint32_t GrpReKeyTime;		/* seconds */
	uint8_t PSKPassPhrase[PSK_PASS_PHRASE_LEN_MAX];
	uint8_t PSKPassPhraseLen;
	uint8_t PSKValue[PMK_LEN_MAX];
	uint8_t MaxPwsHskRetries;
	uint8_t MaxGrpHskRetries;
	uint32_t PwsHskTimeOut;		/* seconds */
	uint32_t GrpHskTimeOut;
} apRsnConfig_t;

_Static_assert(sizeof(apRsnConfig_t) == 124, "access point RSN configuration");

#endif /* ROBIN_ESATYPES_H */
