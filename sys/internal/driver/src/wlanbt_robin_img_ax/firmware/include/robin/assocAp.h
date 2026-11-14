/*
 * The access point's check of the WPA or RSN element a station associates
 * with (Marvell's AssocAp_srv_rom.c, from 0x2b43c), and the station's key
 * state it starts. The RAM firmware gives the ROM code the function that
 * resets a station's key state (ramHook_AssocSrvAp_InitStaKeyInfo; the name
 * follows Marvell's ramHook_ convention and is inferred).
 */
#ifndef ROBIN_ASSOCAP_H
#define ROBIN_ASSOCAP_H

#include <stdint.h>
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/esatypes.h"

/* Marvell's WL_STATUS: 0 on success. */
typedef uint8_t WL_STATUS;
#define WL_STATUS_SUCCESS	0
#define WL_STATUS_FAIL		1

/* AssocAp_srv_rom.c */
int AssocSrvAp_checkCipherSupport(Cipher_t cipher, Cipher_t allowedCiphers);		/* 0x2b43c */
int AssocSrvAp_checkAkm(const AkmSuite_t *pAkm, uint16_t allowedAkms);			/* 0x2b46c */
int assocSrvAp_validate4WayHandshakeIe(SecurityMode_t secType, Cipher_t pwCipher,
				       Cipher_t grpCipher, apKeyMgmtInfoSta_t *pKeyMgmtInfo,
				       uint8_t akmType, uint16_t rsnCap,
				       Cipher_t config_mcstCipher);			/* 0x2b4a6 */
void AssocSrvAp_InitKeyMgmtInfo(apKeyMgmtInfoSta_t *pKeyMgmtInfo, const SecurityMode_t *secType,
				const Cipher_t *pwCipher, uint16_t staRsnCap, uint8_t akmType);	/* 0x2b4f6 */
WL_STATUS assocSrvAp_checkRsnWpa(struct WlanConnection *connPtr, apKeyMgmtInfoSta_t *pKeyMgmtInfo,
				 Cipher_t apWpaCipher, Cipher_t apWpa2Cipher,
				 Cipher_t apMcstCipher, uint16_t apAuthKey,
				 SecurityMode_t *pSecType, IEEEtypes_RSNElement_t *pRsn,
				 IEEEtypes_WPAElement_t *pWpa, int validate4WayHandshakeIE);	/* 0x2b516 */

/* AssocAp_srv.c: the RAM firmware's part */
extern void (*ramHook_AssocSrvAp_InitStaKeyInfo)(struct WlanConnection *connPtr,
						 SecurityMode_t *secType, Cipher_t *pwCipher,
						 uint16_t staRsnCap, uint8_t akmType);	/* 0x54e7c */
void AssocSrvAp_InitStaKeyInfo(struct WlanConnection *connPtr, SecurityMode_t *secType,
			       Cipher_t *pwCipher, uint16_t staRsnCap, uint8_t akmType);	/* 0x3905e */
void AssocSrvAp_InstallHooks(void);								/* 0x398b2 */

#endif /* ROBIN_ASSOCAP_H */
