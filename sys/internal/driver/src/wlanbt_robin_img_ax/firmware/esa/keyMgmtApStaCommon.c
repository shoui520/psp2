/*
 * What the supplicant and the authenticator share: random numbers, the
 * EAPOL-Key MIC and lengths, the PTK derivation and the WPA/RSN element
 * parsing (Marvell's keyMgmtApStaCommon.c, from 0x677c).
 *
 * Written from Marvell's source (Copyright (C) 2014-2017, Marvell International
 * Ltd., GPL-2.0) as this firmware has it.
 */
#include "robin/esa.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"

const uint8_t wpa_oui[3] = { 0x00, 0x50, 0xf2 };
const uint8_t kde_oui[3] = { 0x00, 0x0f, 0xac };
const uint8_t wpa_oui_none[4] = { 0x00, 0x50, 0xf2, 0x00 };
const uint8_t wpa_oui01[4] = { 0x00, 0x50, 0xf2, 0x01 };
const uint8_t wpa_oui02[4] = { 0x00, 0x50, 0xf2, 0x02 };
const uint8_t wpa_oui04[4] = { 0x00, 0x50, 0xf2, 0x04 };
const uint8_t wpa_oui05[4] = { 0x00, 0x50, 0xf2, 0x05 };
const uint8_t wpa2_oui01[4] = { 0x00, 0x0f, 0xac, 0x01 };
const uint8_t wpa2_oui02[4] = { 0x00, 0x0f, 0xac, 0x02 };
const uint8_t wpa2_oui04[4] = { 0x00, 0x0f, 0xac, 0x04 };
const uint8_t wpa2_oui05[4] = { 0x00, 0x0f, 0xac, 0x05 };

ROBIN_PATCH_SLOT(ComputeEAPOL_MIC_hook);
ROBIN_PATCH_SLOT(supplicantParseWpaIe_hook);
ROBIN_PATCH_SLOT(supplicantParseRsnIe_hook);

void supplicantGenerateRand(uint8_t *dataOut, uint32_t length)
{
	uint32_t i;
	uint32_t valueHi, valueLo;

	/*
	 * Read mac 0 timer. Doesn't matter which one we read. We just need a
	 * good seed.
	 */
	msi_wl_GetMCUCoreTimerTxTSF(&valueHi, &valueLo);
	srand(valueLo);
	for (i = 0; i < length; i++)
		dataOut[i] = rand();
}

void ComputeEAPOL_MIC(EAPOL_KeyMsg_t *pKeyMsg, uint16_t data_length,
		      uint8_t *MIC_Key, uint8_t MIC_Key_length,
		      uint8_t micKeyDescVersion)
{
	int len = data_length;
	uint8_t *pMicData;

	if (ComputeEAPOL_MIC_hook(pKeyMsg, data_length, MIC_Key, MIC_Key_length,
				  micKeyDescVersion))
		return;
	pMicData = (uint8_t *)pKeyMsg;

	/*
	 * Allow the caller to override the algorithm used to get by some
	 * Cisco CCX bugs where the wrong MIC algorithm is used.
	 */
	if (micKeyDescVersion == 0) {
		/* algorithm not specified: the one key_info gives */
		micKeyDescVersion = pKeyMsg->key_info.KeyDescriptorVersion;
	}

	switch (micKeyDescVersion) {
	case 2:
		/* CCMP */
		Mrvl_hmac_sha1(&pMicData, &len, 1, MIC_Key, MIC_Key_length,
			       pKeyMsg->key_MIC, EAPOL_MIC_SIZE);
		break;
	case 3:
		/* AES-128-CMAC */
		mrvl_aes_cmac(MIC_Key, pMicData, len, pKeyMsg->key_MIC);
		break;
	default:
	case 1:
		/* TKIP or WEP */
		Mrvl_hmac_md5(pMicData, data_length, MIC_Key, MIC_Key_length,
			      pKeyMsg->key_MIC);
		break;
	}
}

void supplicantParseWpaIe(IEEEtypes_WPAElement_t *pIe, SecurityMode_t *pWpaType,
			  Cipher_t *pMcstCipher, Cipher_t *pUcstCipher,
			  AkmSuite_t *pAkmList, uint8_t akmOutMax)
{
	IEEEtypes_WPAElement_t *pTemp = pIe;
	int count;
	int akmCount = akmOutMax;
	AkmSuite_t *pAkm = pAkmList;

	if (supplicantParseWpaIe_hook(pIe, pWpaType, pMcstCipher, pUcstCipher,
				      pAkmList, akmOutMax))
		return;
	*(uint8_t *)pMcstCipher = 0;
	*(uint8_t *)pUcstCipher = 0;
	__aeabi_memclr(pAkmList, akmOutMax * sizeof(AkmSuite_t));
	__aeabi_memclr(pWpaType, sizeof(SecurityMode_t));
	pWpaType->wpa = 1;

	/* record the AP's multicast cipher */
	if (!memcmp(pTemp->GrpKeyCipher, wpa_oui02, sizeof(wpa_oui02)))
		pMcstCipher->tkip = 1;		/* WPA TKIP */
	else if (!memcmp(pTemp->GrpKeyCipher, wpa_oui04, sizeof(wpa_oui04)))
		pMcstCipher->ccmp = 1;		/* WPA AES */
	else if (!memcmp(pTemp->GrpKeyCipher, wpa_oui01, sizeof(wpa_oui01)))
		pMcstCipher->wep40 = 1;		/* WPA WEP 40 */
	else if (!memcmp(pTemp->GrpKeyCipher, wpa_oui05, sizeof(wpa_oui05)))
		pMcstCipher->wep104 = 1;	/* WPA WEP 104 */

	count = pTemp->PwsKeyCnt;
	while (count) {
		/* record the AP's unicast cipher */
		if (!memcmp(pTemp->PwsKeyCipherList, wpa_oui02, sizeof(wpa_oui02)))
			pUcstCipher->tkip = 1;
		else if (!memcmp(pTemp->PwsKeyCipherList, wpa_oui04, sizeof(wpa_oui04)))
			pUcstCipher->ccmp = 1;
		count--;
		if (count)
			pTemp = (IEEEtypes_WPAElement_t *)((uint8_t *)pTemp +
							   sizeof(pTemp->PwsKeyCipherList));
	}

	count = pTemp->AuthKeyCnt;
	while (count) {
		if (akmCount) {
			/* store the AKM */
			__aeabi_memcpy(pAkm, pTemp->AuthKeyList, sizeof(pTemp->AuthKeyList));
			pAkm++;
			akmCount--;
		}
		count--;
		if (count)
			pTemp = (IEEEtypes_WPAElement_t *)((uint8_t *)pTemp +
							   sizeof(pTemp->AuthKeyList));
	}

	if (!memcmp(pAkmList, wpa_oui_none, sizeof(wpa_oui_none)))
		pWpaType->wpaNone = 1;
}

void supplicantParseMcstCipher(Cipher_t *pMcstCipherOut, uint8_t *pGrpKeyCipher)
{
	*(uint8_t *)pMcstCipherOut = 0;

	/* record the AP's multicast cipher */
	if (!memcmp(pGrpKeyCipher, wpa2_oui02, sizeof(wpa2_oui02)))
		pMcstCipherOut->tkip = 1;	/* WPA2 TKIP */
	else if (!memcmp(pGrpKeyCipher, wpa2_oui04, sizeof(wpa2_oui04)))
		pMcstCipherOut->ccmp = 1;	/* WPA2 AES */
	else if (!memcmp(pGrpKeyCipher, wpa2_oui01, sizeof(wpa2_oui01)))
		pMcstCipherOut->wep40 = 1;	/* WPA2 WEP 40 */
	else if (!memcmp(pGrpKeyCipher, wpa2_oui05, sizeof(wpa2_oui05)))
		pMcstCipherOut->wep104 = 1;	/* WPA2 WEP 104 */
}

void supplicantParseUcstCipher(Cipher_t *pUcstCipherOut, uint8_t pwsKeyCnt,
			       uint8_t *pPwsKeyCipherList)
{
	uint8_t count;

	*(uint8_t *)pUcstCipherOut = 0;

	/* cycle through the pairwise cipher list and record each one */
	for (count = 0; count < pwsKeyCnt; count++) {
		if (!memcmp(pPwsKeyCipherList + count * 4, wpa2_oui02, sizeof(wpa2_oui02)))
			pUcstCipherOut->tkip = 1;	/* WPA2 TKIP */
		else if (!memcmp(pPwsKeyCipherList + count * 4, wpa2_oui04,
				 sizeof(wpa2_oui04)))
			pUcstCipherOut->ccmp = 1;	/* WPA2 AES */
	}
}

void supplicantParseRsnIe(IEEEtypes_RSNElement_t *pRsnIe, SecurityMode_t *pWpaTypeOut,
			  Cipher_t *pMcstCipherOut, Cipher_t *pUcstCipherOut,
			  AkmSuite_t *pAkmListOut, uint8_t akmOutMax,
			  IEEEtypes_RSNCapability_t *pRsnCapOut,
			  Cipher_t *pGrpMgmtCipherOut)
{
	uint8_t *pIeData;
	uint8_t *pIeEnd;
	uint8_t *pGrpKeyCipher;
	uint16_t pwsKeyCnt;
	uint8_t *pPwsKeyCipherList;
	uint16_t authKeyCnt;
	uint8_t *pAuthKeyList;
	IEEEtypes_RSNCapability_t *pRsnCap;
	uint16_t PMKIDCnt;

	if (supplicantParseRsnIe_hook(pRsnIe, pWpaTypeOut, pMcstCipherOut,
				      pUcstCipherOut, pAkmListOut, akmOutMax,
				      pRsnCapOut, pGrpMgmtCipherOut))
		return;
	__aeabi_memclr(pWpaTypeOut, sizeof(SecurityMode_t));
	pWpaTypeOut->wpa2 = 1;

	/* the start and end of the element data */
	pIeData = (uint8_t *)&pRsnIe->Ver;
	pIeEnd = pIeData + pRsnIe->Len;

	/* skip past the version field */
	pIeData += sizeof(pRsnIe->Ver);

	/* the group key cipher */
	pGrpKeyCipher = pIeData;
	pIeData += sizeof(pRsnIe->GrpKeyCipher);
	supplicantParseMcstCipher(pMcstCipherOut, pGrpKeyCipher);

	/* the pairwise key cipher list */
	pwsKeyCnt = *(uint16_t *)pIeData;
	pIeData += sizeof(pRsnIe->PwsKeyCnt);
	pPwsKeyCipherList = pIeData;
	pIeData += pwsKeyCnt * sizeof(pRsnIe->PwsKeyCipherList);
	supplicantParseUcstCipher(pUcstCipherOut, pwsKeyCnt, pPwsKeyCipherList);

	/* the AKM list */
	authKeyCnt = *(uint16_t *)pIeData;
	pIeData += sizeof(pRsnIe->AuthKeyCnt);
	pAuthKeyList = pIeData;
	pIeData += authKeyCnt * sizeof(pRsnIe->AuthKeyList);
	__aeabi_memclr(pAkmListOut, akmOutMax * sizeof(AkmSuite_t));
	__aeabi_memcpy(pAkmListOut, pAuthKeyList,
		       (authKeyCnt < akmOutMax ? authKeyCnt : akmOutMax) *
		       sizeof(pRsnIe->AuthKeyList));

	/* the RSN capabilities, when included */
	if (pIeData < pIeEnd) {
		pRsnCap = (IEEEtypes_RSNCapability_t *)pIeData;
		pIeData += sizeof(pRsnIe->RsnCap);
		if (pRsnCapOut)
			*pRsnCapOut = *pRsnCap;
	}

	/* the PMKID count and list, when included */
	if (pIeData < pIeEnd) {
		PMKIDCnt = *(uint16_t *)pIeData;
		pIeData += sizeof(pRsnIe->PMKIDCnt);
		if (pIeData < pIeEnd)
			pIeData += PMKIDCnt * sizeof(pRsnIe->PMKIDList);
	}

	/* the group management cipher, when included */
	if (pIeData < pIeEnd && pGrpMgmtCipherOut)
		__aeabi_memcpy(pGrpMgmtCipherOut, pIeData, sizeof(pRsnIe->GrpMgmtCipher));
}
