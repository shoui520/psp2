/*
 * What the access point checks of the WPA or RSN element a station
 * associates with, and again of the one in message 2 of the 4-way handshake
 * (Marvell's AssocAp_srv_rom.c, from 0x2b43c).
 *
 * Written from Marvell's source (Copyright (C) 2014-2017, Marvell International
 * Ltd., GPL-2.0) as this firmware has it.
 */
#include "robin/assocAp.h"
#include "robin/esa.h"
#include "robin/keyMgmtAp.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"

#define AKM_SUITE_MAX	5

/* this file's part of the ROM-patch table, 0x55bec */
ROBIN_PATCH_SLOT(assocSrvAp_checkRsnWpa_hook);

/* The station's cipher is one the access point allows. */
int AssocSrvAp_checkCipherSupport(Cipher_t cipher, Cipher_t allowedCiphers)
{
	int match = 0;

	if (cipher.ccmp && allowedCiphers.ccmp)
		match = 1;
	else if (cipher.tkip && allowedCiphers.tkip)
		match = 1;
	else if (cipher.wep40 && allowedCiphers.wep40)
		match = 1;
	else if (cipher.wep104 && allowedCiphers.wep104)
		match = 1;
	return match;
}

/* The key management of the station's suite (802.1X or PSK), if the access point allows it. */
int AssocSrvAp_checkAkm(const AkmSuite_t *pAkm, uint16_t allowedAkms)
{
	uint16_t matchedAkms = 0;

	if (memcmp(pAkm->akmOui, wpa_oui, sizeof(wpa_oui)) != 0 &&
	    memcmp(pAkm->akmOui, kde_oui, sizeof(kde_oui)) != 0)
		return matchedAkms;

	switch (pAkm->akmType) {
	case AKM_1X:
		matchedAkms = allowedAkms & UAP_HOSTCMD_KEYMGMT_EAP;
		break;
	case AKM_PSK:
		matchedAkms = allowedAkms & UAP_HOSTCMD_KEYMGMT_PSK;
		break;
	default:
		break;
	}
	return matchedAkms;
}

/* The element of message 2 is the one the station associated with; 1 if it is not. */
int assocSrvAp_validate4WayHandshakeIe(SecurityMode_t secType, Cipher_t pwCipher,
				       Cipher_t grpCipher, apKeyMgmtInfoSta_t *pKeyMgmtInfo,
				       uint8_t akmType, uint16_t rsnCap,
				       Cipher_t config_mcstCipher)
{
	if (memcmp(&secType, &pKeyMgmtInfo->staSecType, sizeof(secType)) != 0)
		return 1;
	if (memcmp(&grpCipher, &config_mcstCipher, sizeof(grpCipher)) != 0)
		return 1;
	if (memcmp(&pwCipher, &pKeyMgmtInfo->staUcstCipher, sizeof(pwCipher)) != 0)
		return 1;
	if (akmType != pKeyMgmtInfo->staAkmType)
		return 1;
	if (secType.wpa2 && pKeyMgmtInfo->staRsnCap != rsnCap)
		return 1;
	return 0;
}

/* The station's key state at the start of its handshake. */
void AssocSrvAp_InitKeyMgmtInfo(apKeyMgmtInfoSta_t *pKeyMgmtInfo, const SecurityMode_t *secType,
				const Cipher_t *pwCipher, uint16_t staRsnCap, uint8_t akmType)
{
	const uint8_t *src = (const uint8_t *)secType;
	uint8_t *dst = (uint8_t *)&pKeyMgmtInfo->staSecType;

	pKeyMgmtInfo->keyMgmtState = HSK_NOT_STARTED;
	dst[0] = src[0];
	dst[1] = src[1];
	pKeyMgmtInfo->staUcstCipher = *pwCipher;
	pKeyMgmtInfo->staAkmType = akmType;
	if (secType->wpa2)
		pKeyMgmtInfo->staRsnCap = staRsnCap;
}

/*
 * Checks the element of an association request (or, with validate4WayHandshakeIE,
 * of message 2) against what the access point is set to allow: one pairwise
 * cipher, one key management suite, the ciphers and the suite allowed. An
 * association request that passes starts the station's key state; message 2
 * must match what it started. The staRsnCap is 0xffff if the element has none.
 */
WL_STATUS assocSrvAp_checkRsnWpa(struct WlanConnection *connPtr, apKeyMgmtInfoSta_t *pKeyMgmtInfo,
				 Cipher_t apWpaCipher, Cipher_t apWpa2Cipher,
				 Cipher_t apMcstCipher, uint16_t apAuthKey,
				 SecurityMode_t *pSecType, IEEEtypes_RSNElement_t *pRsn,
				 IEEEtypes_WPAElement_t *pWpa, int validate4WayHandshakeIE)
{
	uint32_t result = WL_STATUS_SUCCESS;
	Cipher_t apCipher = { 0 };
	Cipher_t pwCipher;
	Cipher_t grpCipher;
	SecurityMode_t wpaType = { 0 };
	AkmSuite_t akm[AKM_SUITE_MAX];
	uint8_t minimumRsnLen;
	union {
		uint16_t shortInt;
		IEEEtypes_RSNCapability_t cfg;
	} staRsnCap;

	if (assocSrvAp_checkRsnWpa_hook(connPtr, pKeyMgmtInfo, apWpaCipher, apWpa2Cipher,
					apMcstCipher, apAuthKey, pSecType, pRsn, pWpa,
					validate4WayHandshakeIE, &result))
		return result;

	staRsnCap.shortInt = 0xffff;

	if (pRsn != NULL && pSecType->wpa2 == 1) {
		/*
		 * Everything after the version is optional in the element; the
		 * association request is refused without the group cipher, one
		 * pairwise cipher and one key management suite.
		 */
		minimumRsnLen = (uintptr_t)&pRsn->RsnCap - (uintptr_t)&pRsn->Ver;
		if (pRsn->Len < minimumRsnLen)
			return WL_STATUS_FAIL;

		if (pRsn->PwsKeyCnt == 1 && pRsn->AuthKeyCnt == 1) {
			apCipher = apWpa2Cipher;
			supplicantParseRsnIe(pRsn, &wpaType, &grpCipher, &pwCipher, akm,
					     AKM_SUITE_MAX, &staRsnCap.cfg, NULL);
		} else {
			result = WL_STATUS_FAIL;
		}
	} else if (pWpa != NULL && pSecType->wpa == 1) {
		if (pWpa->PwsKeyCnt == 1 && pWpa->AuthKeyCnt == 1) {
			apCipher = apWpaCipher;
			supplicantParseWpaIe(pWpa, &wpaType, &grpCipher, &pwCipher, akm,
					     AKM_SUITE_MAX);
		} else {
			result = WL_STATUS_FAIL;
		}
	} else {
		result = WL_STATUS_FAIL;
	}

	if (pwCipher.ccmp == 0 && pwCipher.tkip == 0)
		result = WL_STATUS_FAIL;
	if (grpCipher.ccmp == 0 && grpCipher.tkip == 0)
		return WL_STATUS_FAIL;

	if (result == WL_STATUS_SUCCESS) {
		if (validate4WayHandshakeIE == 0) {
			if (AssocSrvAp_checkCipherSupport(pwCipher, apCipher) != 1 ||
			    AssocSrvAp_checkCipherSupport(grpCipher, apMcstCipher) != 1 ||
			    AssocSrvAp_checkAkm(akm, apAuthKey) == 0)
				return WL_STATUS_FAIL;
			ramHook_AssocSrvAp_InitStaKeyInfo(connPtr, &wpaType, &pwCipher,
							  staRsnCap.shortInt, akm[0].akmType);
		} else {
			result = assocSrvAp_validate4WayHandshakeIe(wpaType, pwCipher, grpCipher,
								    pKeyMgmtInfo, akm[0].akmType,
								    staRsnCap.shortInt, apMcstCipher);
		}
	}
	return result;
}
