/*
 * The supplicant's key management, the part the Mask ROM build keeps
 * patchable: the association's RSN choice, the PMKIDs, the AKM tests, the
 * WPA/RSN element and the EAPOL-Key frames (Marvell's keyMgmtSta_rom.c,
 * from 0x6abc).
 *
 * Written from Marvell's source (Copyright (C) 2014-2017, Marvell International
 * Ltd., GPL-2.0) as this firmware has it.
 */
#include "robin/buffer.h"
#include "robin/btrom.h"
#include "robin/esa.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"

/* this file's part of the ROM-patch table, 0x55b10.. (0x55b20/24: the WPA and RSN parsers') */
ROBIN_PATCH_SLOT(FillKeyMaterialStruct_internal_hook);
ROBIN_PATCH_SLOT(keyMgmtFormatWpaRsnIe_internal_hook);
ROBIN_PATCH_SLOT(install_wpa_none_keys_internal_hook);
ROBIN_PATCH_SLOT(keyMgmtGetKeySize_internal_hook);
ROBIN_PATCH_SLOT(parseKeyKDE_DataType_hook);
ROBIN_PATCH_SLOT(parseKeyDataGTK_hook);
ROBIN_PATCH_SLOT(KeyMgmtSta_ApplyKEK_hook);
ROBIN_PATCH_SLOT(KeyMgmtSta_IsRxEAPOLValid_hook);
ROBIN_PATCH_SLOT(KeyMgmtSta_PrepareEAPOLFrame_hook);
ROBIN_PATCH_SLOT(KeyMgmtSta_PopulateEAPOLLengthMic_hook);
ROBIN_PATCH_SLOT(KeyMgmtSta_PrepareEAPOLMicErrFrame_hook);
ROBIN_PATCH_SLOT(KeyMgmtSta_DeriveKeys_hook);

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

/* The cipher, AKM and capabilities the association uses, from what both sides allow. */
void supplicantSetAssocRsn_internal(RSNConfig_t *pRsnConfig,
				    SecurityParams_t *pSecurityParams,
				    SecurityMode_t wpaType, Cipher_t *pMcstCipher,
				    Cipher_t *pUcstCipher, AkmSuite_t *pAkm,
				    IEEEtypes_RSNCapability_t *pRsnCap,
				    Cipher_t *pGrpMgmtCipher)
{
	__aeabi_memclr(&pRsnConfig->wpaType, sizeof(pRsnConfig->wpaType));
	*(uint8_t *)&pRsnConfig->ucstCipher = 0;
	*(uint8_t *)&pRsnConfig->mcstCipher = 0;
	pRsnConfig->pmkidValid = 0;
	pRsnConfig->rsnCapValid = 0;
	pRsnConfig->grpMgmtCipherValid = 0;
	pRsnConfig->rsvd = 0;

	if (pSecurityParams->wpaType.wpa2 && wpaType.wpa2) {
		/* encryption mode is WPA2 */
		__aeabi_memcpy(&pRsnConfig->AKM, pAkm, sizeof(pRsnConfig->AKM));
		pRsnConfig->wpaType.wpa2 = 1;
		if (pRsnCap) {
			pRsnConfig->rsnCapValid = 1;
			pRsnConfig->rsnCap = *pRsnCap;
		}
		if (pGrpMgmtCipher) {
			pRsnConfig->grpMgmtCipherValid = 1;
			pRsnConfig->grpMgmtCipher = *pGrpMgmtCipher;
		}
	} else if (pSecurityParams->wpaType.wpaNone && wpaType.wpaNone) {
		__aeabi_memcpy(&pRsnConfig->AKM, wpa_oui_none, sizeof(pRsnConfig->AKM));
		/* encryption mode is WPA None */
		pRsnConfig->wpaType.wpaNone = 1;
		if (pSecurityParams->mcstCipher.ccmp && pMcstCipher->ccmp)
			pRsnConfig->mcstCipher.ccmp = 1;
		else
			pRsnConfig->mcstCipher.tkip = 1;
	} else if (pSecurityParams->wpaType.wpa && wpaType.wpa) {
		/* encryption mode is WPA */
		__aeabi_memcpy(&pRsnConfig->AKM, pAkm, sizeof(pRsnConfig->AKM));
		pRsnConfig->wpaType.wpa = 1;
	} else if (pSecurityParams->wpaType.noRsn) {
		/* no encryption */
		pRsnConfig->wpaType.noRsn = 1;
	}

	if (pRsnConfig->wpaType.wpa || pRsnConfig->wpaType.wpa2) {
		if (pSecurityParams->ucstCipher.ccmp && pUcstCipher->ccmp)
			pRsnConfig->ucstCipher.ccmp = 1;
		else
			pRsnConfig->ucstCipher.tkip = 1;

		if (pSecurityParams->mcstCipher.ccmp && pMcstCipher->ccmp)
			pRsnConfig->mcstCipher.ccmp = 1;
		else if (pSecurityParams->mcstCipher.tkip && pMcstCipher->tkip)
			pRsnConfig->mcstCipher.tkip = 1;
		else if (pSecurityParams->mcstCipher.wep104 && pMcstCipher->wep104)
			pRsnConfig->mcstCipher.wep104 = 1;
		else
			pRsnConfig->mcstCipher.wep40 = 1;
	}
}

/* PMKID = HMAC-SHA1-128(PMK, "PMK Name" || AA || SPA) */
void supplicantGenerateSha1Pmkid(uint8_t *pPMK, IEEEtypes_MacAddr_t *pBssid,
				 IEEEtypes_MacAddr_t *pSta, uint8_t *pPMKID)
{
	char pmkidString[] = "PMK Name";
	void *pText[3];
	int len[3] = { 8, 6, 6 };

	pText[0] = pmkidString;
	pText[1] = pBssid;
	pText[2] = pSta;
	/* the PMK is always 32 bytes */
	Mrvl_hmac_sha1((uint8_t **)pText, len, 3, pPMK, 32, pPMKID, 16);
}

/* PMKID = Truncate-128(SHA-256("PMK Name" || AA || SPA)) */
void supplicantGenerateSha256Pmkid(uint8_t *pPMK, IEEEtypes_MacAddr_t *pBssid,
				   IEEEtypes_MacAddr_t *pSta, uint8_t *pPMKID)
{
	OsaBuffer *pBufDesc;
	uint8_t *pOutput;
	uint8_t *vectors[3];
	int vectLen[3];

	/* waiting forever ensures a buffer */
	pBufDesc = osa_BufferAlloc(ramHook_encrPoolConfig, 500, 0xffffffff);
	pOutput = (uint8_t *)pBufDesc + pBufDesc->dataOffset;
	vectors[0] = (uint8_t *)"PMK Name";
	vectLen[0] = 8;			/* strlen("PMK Name") */
	vectLen[1] = sizeof(IEEEtypes_MacAddr_t);
	vectLen[2] = sizeof(IEEEtypes_MacAddr_t);
	vectors[1] = (uint8_t *)pBssid;
	vectors[2] = (uint8_t *)pSta;
	mrvl_sha256_crypto_vector(3, vectors, vectLen, pOutput);
	__aeabi_memcpy(pPMKID, pOutput, PMKID_LEN);
	osa_BufferFree(pBufDesc);
}

int supplicantAkmIsWpa2(AkmSuite_t *pAkm)
{
	return memcmp(pAkm->akmOui, kde_oui, sizeof(kde_oui)) == 0;
}

int supplicantAkmUsesSha256Pmkid(AkmSuite_t *pAkm)
{
	if (supplicantAkmIsWpa2(pAkm)) {
		if (pAkm->akmType == AKM_SHA256_PSK || pAkm->akmType == AKM_SHA256_1X)
			return 1;
	}
	return 0;
}

int supplicantAkmIsWpaWpa2(AkmSuite_t *pAkm)
{
	return !memcmp(pAkm->akmOui, wpa_oui, sizeof(wpa_oui)) ||
	       !memcmp(pAkm->akmOui, kde_oui, sizeof(kde_oui));
}

int supplicantAkmIsWpaWpa2Psk(AkmSuite_t *pAkm)
{
	if (supplicantAkmIsWpaWpa2(pAkm))
		return pAkm->akmType == AKM_PSK || pAkm->akmType == AKM_SHA256_PSK ||
		       pAkm->akmType == AKM_FT_PSK;
	return 0;
}

/* The PMKID of a cached PMK for an 802.1X AKM; 0 when there is none. */
int supplicantGetPmkid(IEEEtypes_MacAddr_t *pBssid, IEEEtypes_MacAddr_t *pStaAddr,
		       AkmSuite_t *pAkm, uint8_t *pPMKID)
{
	int retval = 0;
	uint8_t *pPMK;

	if (!supplicantAkmIsWpaWpa2Psk(pAkm)) {
		pPMK = pmkCacheFindPMK(pBssid);
		/* found the PMK so generate the PMKID */
		if (pPMK) {
			if (supplicantAkmUsesSha256Pmkid(pAkm))
				supplicantGenerateSha256Pmkid(pPMK, pBssid, pStaAddr, pPMKID);
			else
				supplicantGenerateSha1Pmkid(pPMK, pBssid, pStaAddr, pPMKID);
			retval = 1;
		}
	}
	return retval;
}

/* Writes the WPA or RSN element of pRsnConfig at pos; returns its size. */
uint16_t keyMgmtFormatWpaRsnIe_internal(RSNConfig_t *pRsnConfig, uint8_t *pos,
					IEEEtypes_MacAddr_t *pBssid,
					IEEEtypes_MacAddr_t *pStaAddr,
					uint8_t *pPmkid, int addPmkid)
{
	IEEEtypes_WPAElement_t *pWpaIe = (IEEEtypes_WPAElement_t *)pos;
	IEEEtypes_RSNElement_t *pRsnIe = (IEEEtypes_RSNElement_t *)pos;
	uint32_t ieSize = 0;
	uint16_t ptr_val;

	if (keyMgmtFormatWpaRsnIe_internal_hook(pRsnConfig, pos, pBssid, pStaAddr,
						pPmkid, addPmkid, &ptr_val))
		return ptr_val;

	if (pRsnConfig->wpaType.wpa2) {
		/* encryption mode is WPA2 */
		pRsnIe->ElementId = ELEM_ID_RSN;
		pRsnIe->Len = sizeof(pRsnIe->Ver) + sizeof(pRsnIe->GrpKeyCipher) +
			      sizeof(pRsnIe->PwsKeyCnt) + sizeof(pRsnIe->PwsKeyCipherList) +
			      sizeof(pRsnIe->AuthKeyCnt) + sizeof(pRsnIe->AuthKeyList);

		__aeabi_memcpy(pRsnIe->AuthKeyList, &pRsnConfig->AKM,
			       sizeof(pRsnIe->AuthKeyList));

		pRsnIe->Ver = 1;
		pRsnIe->PwsKeyCnt = 1;
		pRsnIe->AuthKeyCnt = 1;

		if (pRsnConfig->ucstCipher.ccmp)
			/* unicast cipher is aes */
			__aeabi_memcpy(pRsnIe->PwsKeyCipherList, wpa2_oui04,
				       sizeof(pRsnIe->PwsKeyCipherList));
		else
			/* if not AES then TKIP */
			__aeabi_memcpy(pRsnIe->PwsKeyCipherList, wpa2_oui02,
				       sizeof(pRsnIe->PwsKeyCipherList));

		if (pRsnConfig->mcstCipher.ccmp)
			__aeabi_memcpy(pRsnIe->GrpKeyCipher, wpa2_oui04,
				       sizeof(pRsnIe->GrpKeyCipher));
		else if (pRsnConfig->mcstCipher.tkip)
			__aeabi_memcpy(pRsnIe->GrpKeyCipher, wpa2_oui02,
				       sizeof(pRsnIe->GrpKeyCipher));
		else if (pRsnConfig->mcstCipher.wep104)
			__aeabi_memcpy(pRsnIe->GrpKeyCipher, wpa2_oui05,
				       sizeof(pRsnIe->GrpKeyCipher));
		else
			__aeabi_memcpy(pRsnIe->GrpKeyCipher, wpa2_oui01,
				       sizeof(pRsnIe->GrpKeyCipher));

		if (addPmkid && ((!pRsnConfig->pmkidValid && pBssid) || pPmkid)) {
			if (pPmkid) {
				__aeabi_memcpy(pRsnConfig->PMKID, pPmkid,
					       sizeof(pRsnConfig->PMKID));
				pRsnConfig->pmkidValid = 1;
			} else {
				pRsnConfig->pmkidValid =
					supplicantGetPmkid(pBssid, pStaAddr,
							   &pRsnConfig->AKM,
							   pRsnConfig->PMKID);
			}
		}

		if (pRsnConfig->rsnCapValid || pRsnConfig->pmkidValid ||
		    pRsnConfig->grpMgmtCipherValid) {
			pRsnIe->RsnCap = pRsnConfig->rsnCap;
			pRsnIe->Len += sizeof(pRsnIe->RsnCap);
		}

		if (pRsnConfig->pmkidValid || pRsnConfig->grpMgmtCipherValid) {
			pRsnIe->PMKIDCnt = 0;
			pRsnIe->Len += sizeof(pRsnIe->PMKIDCnt);

			if (pRsnConfig->pmkidValid) {
				/* add the PMKID to the RSN element if not an EAPOL message */
				pRsnIe->PMKIDCnt = 1;
				__aeabi_memcpy(pRsnIe->PMKIDList, pRsnConfig->PMKID,
					       sizeof(pRsnIe->PMKIDList));
				pRsnIe->Len += sizeof(pRsnIe->PMKIDList);
			}
		}

		if (pRsnConfig->grpMgmtCipherValid) {
			/* Marvell's: four bytes from the one-byte grpMgmtCipher */
			__aeabi_memcpy(pRsnIe->GrpMgmtCipher, &pRsnConfig->grpMgmtCipher,
				       sizeof(pRsnIe->GrpMgmtCipher));
			pRsnIe->Len += sizeof(pRsnIe->GrpMgmtCipher);
		}

		ieSize = sizeof(pRsnIe->ElementId) + sizeof(pRsnIe->Len);
		ieSize += pRsnIe->Len;
	} else if (pRsnConfig->wpaType.wpaNone || pRsnConfig->wpaType.wpa) {
		/* encryption mode is WPA */
		pWpaIe->ElementId = ELEM_ID_VENDOR_SPECIFIC;
		pWpaIe->Len = sizeof(IEEEtypes_WPAElement_t) - sizeof(pWpaIe->ElementId) -
			      sizeof(pWpaIe->Len);

		__aeabi_memcpy(pWpaIe->OuiType, wpa_oui01, sizeof(pWpaIe->OuiType));

		pWpaIe->Ver = 1;
		pWpaIe->PwsKeyCnt = 1;
		pWpaIe->AuthKeyCnt = 1;

		__aeabi_memcpy(pWpaIe->AuthKeyList, &pRsnConfig->AKM,
			       sizeof(pWpaIe->AuthKeyList));

		if (pRsnConfig->wpaType.wpaNone) {
			__aeabi_memcpy(pWpaIe->PwsKeyCipherList, wpa_oui_none,
				       sizeof(pWpaIe->PwsKeyCipherList));

			if (pRsnConfig->mcstCipher.tkip)
				__aeabi_memcpy(pWpaIe->GrpKeyCipher, wpa_oui02,
					       sizeof(pWpaIe->GrpKeyCipher));
			else if (pRsnConfig->mcstCipher.ccmp)
				__aeabi_memcpy(pWpaIe->GrpKeyCipher, wpa_oui04,
					       sizeof(pWpaIe->GrpKeyCipher));
		} else {
			if (pRsnConfig->ucstCipher.ccmp)
				__aeabi_memcpy(pWpaIe->PwsKeyCipherList, wpa_oui04,
					       sizeof(pWpaIe->PwsKeyCipherList));
			else
				__aeabi_memcpy(pWpaIe->PwsKeyCipherList, wpa_oui02,
					       sizeof(pWpaIe->PwsKeyCipherList));

			if (pRsnConfig->mcstCipher.ccmp)
				__aeabi_memcpy(pWpaIe->GrpKeyCipher, wpa_oui04,
					       sizeof(pWpaIe->GrpKeyCipher));
			else if (pRsnConfig->mcstCipher.tkip)
				__aeabi_memcpy(pWpaIe->GrpKeyCipher, wpa_oui02,
					       sizeof(pWpaIe->GrpKeyCipher));
			else if (pRsnConfig->mcstCipher.wep104)
				__aeabi_memcpy(pWpaIe->GrpKeyCipher, wpa_oui05,
					       sizeof(pWpaIe->GrpKeyCipher));
			else
				__aeabi_memcpy(pWpaIe->GrpKeyCipher, wpa_oui01,
					       sizeof(pWpaIe->GrpKeyCipher));
		}

		ieSize = sizeof(pWpaIe->ElementId) + sizeof(pWpaIe->Len);
		ieSize += pWpaIe->Len;
	}

	return ieSize;
}

/* The WPA-None (ad-hoc) keys come straight from the PMK. */
void install_wpa_none_keys_internal(key_MgtMaterial_t *pKeyMgtData, uint8_t *pPMK,
				    uint8_t type, uint8_t unicast)
{
	if (install_wpa_none_keys_internal_hook(pKeyMgtData, pPMK, type, unicast))
		return;

	__aeabi_memclr(pKeyMgtData, sizeof(key_MgtMaterial_t));

	/* Marvell's: the unicast key is flagged multicast and the other way round */
	if (unicast)
		pKeyMgtData->keyInfo = KEY_INFO_MULTICAST | KEY_INFO_ENABLED;
	else
		pKeyMgtData->keyInfo = KEY_INFO_UNICAST | KEY_INFO_ENABLED;

	if (type) {
		__aeabi_memcpy(pKeyMgtData->keyEncypt.AES.key, pPMK, 16);
		pKeyMgtData->keyType = KEY_TYPE_AES;
		pKeyMgtData->keyLen = WPA_AES_KEY_LEN;
	} else {
		__aeabi_memcpy(pKeyMgtData->keyEncypt.TKIP.key, pPMK, 16);
		pPMK += 16;
		pKeyMgtData->keyType = KEY_TYPE_TKIP;

		/* in WPA None the TX and RX MIC keys are the same */
		__aeabi_memcpy(pKeyMgtData->keyEncypt.TKIP.txMicKey, pPMK, 8);
		__aeabi_memcpy(pKeyMgtData->keyEncypt.TKIP.rxMicKey, pPMK, 8);

		pKeyMgtData->keyLen = WPA_TKIP_KEY_LEN;
	}
}

uint16_t keyMgmtGetKeySize_internal(RSNConfig_t *pRsnConfig, uint8_t isPairwise)
{
	/* default to the TKIP key size */
	uint16_t retval = 32;

	if (keyMgmtGetKeySize_internal_hook(pRsnConfig, isPairwise, &retval))
		return retval;

	if (isPairwise) {
		if (pRsnConfig->ucstCipher.ccmp)
			retval = 16;
	} else {
		if (pRsnConfig->mcstCipher.ccmp)
			retval = 16;
		else if (pRsnConfig->mcstCipher.wep104)
			retval = 13;
		else if (pRsnConfig->mcstCipher.wep40)
			retval = 5;
	}
	return retval;
}

/* Whether the authenticator's replay counter is newer than the last valid one. */
int isApReplayCounterFresh(keyMgmtInfoSta_t *pKeyMgmtInfoSta, uint8_t *pRxReplayCount)
{
	uint32_t tmpHi;
	uint32_t tmpLo;
	uint32_t rxCountHi;
	uint32_t rxCountLo;
	/* initialize the value as stale */
	int retVal = 0;

	__aeabi_memcpy(&tmpHi, pRxReplayCount, 4);
	__aeabi_memcpy(&tmpLo, pRxReplayCount + 4, 4);

	rxCountHi = ntohl(tmpHi);
	rxCountLo = ntohl(tmpLo);

	/* check the high word first */
	if (rxCountHi > pKeyMgmtInfoSta->apCounterHi) {
		retVal = 1;
	} else if (rxCountHi == pKeyMgmtInfoSta->apCounterHi) {
		/* the high word is equal, check the low word */
		if (rxCountLo > pKeyMgmtInfoSta->apCounterLo) {
			retVal = 1;
		} else if (rxCountLo == pKeyMgmtInfoSta->apCounterLo) {
			/* counters are equal: check the special case of zero */
			if (rxCountHi == 0 && rxCountLo == 0) {
				if (!pKeyMgmtInfoSta->apCounterZeroDone)
					retVal = 1;
			}
		}
	}

	return retVal;
}

void updateApReplayCounter(keyMgmtInfoSta_t *pKeyMgmtStaInfo, uint8_t *pRxReplayCount)
{
	uint32_t tmpHi;
	uint32_t tmpLo;
	uint32_t rxCountHi;
	uint32_t rxCountLo;

	__aeabi_memcpy(&tmpHi, pRxReplayCount, 4);
	__aeabi_memcpy(&tmpLo, pRxReplayCount + 4, 4);

	rxCountHi = ntohl(tmpHi);
	rxCountLo = ntohl(tmpLo);

	pKeyMgmtStaInfo->apCounterHi = rxCountHi;
	pKeyMgmtStaInfo->apCounterLo = rxCountLo;

	if (rxCountHi == 0 && rxCountLo == 0)
		pKeyMgmtStaInfo->apCounterZeroDone = 1;
}

/* The key material the MAC installs, from a derived key of key_len bytes. */
void FillKeyMaterialStruct_internal(key_MgtMaterial_t *pKeyMgtData, uint16_t key_len,
				    uint8_t isPairwise, KeyData_t *pKey)
{
	uint8_t keyInfo;

	if (FillKeyMaterialStruct_internal_hook(pKeyMgtData, key_len, isPairwise, pKey))
		return;

	/* update the key material */
	__aeabi_memclr(pKeyMgtData, sizeof(key_MgtMaterial_t));

	/* check whether the key type is pairwise */
	if (isPairwise)
		keyInfo = KEY_INFO_UNICAST;
	else
		keyInfo = KEY_INFO_MULTICAST;

	if (key_len == WPA_AES_KEY_LEN) {
		pKeyMgtData->keyType = KEY_TYPE_AES;
		pKeyMgtData->keyInfo = keyInfo | KEY_INFO_ENABLED;
		__aeabi_memcpy(pKeyMgtData->keyEncypt.AES.key, pKey->Key, key_len);
	} else if (key_len == WPA_TKIP_KEY_LEN) {
		pKeyMgtData->keyType = KEY_TYPE_TKIP;
		pKeyMgtData->keyInfo = keyInfo | KEY_INFO_ENABLED;
		__aeabi_memcpy(pKeyMgtData->keyEncypt.TKIP.key, pKey->Key, TK_SIZE);
		__aeabi_memcpy(pKeyMgtData->keyEncypt.TKIP.txMicKey, pKey->TxMICKey, 8);
		__aeabi_memcpy(pKeyMgtData->keyEncypt.TKIP.rxMicKey, pKey->RxMICKey, 8);
	} else if (key_len == WPA_WEP104_KEY_LEN || key_len == WPA_WEP40_KEY_LEN) {
		pKeyMgtData->keyType = KEY_TYPE_WEP;
		pKeyMgtData->keyInfo = keyInfo | KEY_INFO_ENABLED;

		if (isPairwise) {
			pKeyMgtData->keyEncypt.WEP.keyIndex = 0;
			pKeyMgtData->keyEncypt.WEP.isDefaultTx = 1;
		} else {
			/* use the key index provided */
			pKeyMgtData->keyEncypt.WEP.keyIndex = pKey->KeyIndex;
			pKeyMgtData->keyEncypt.WEP.isDefaultTx = 0;
		}
		__aeabi_memcpy(pKeyMgtData->keyEncypt.WEP.key, pKey->Key, key_len);
	} else {
		/* the key length does not match: send down nothing */
		return;
	}

	pKeyMgtData->keyLen = key_len;
}

/*
 * In this build's ROM these two of Marvell's keyMgmtApStaCommon.c sit between
 * the key-material and KDE code, so they are kept here at their place.
 */
void formEAPOLEthHdr(EAPOL_KeyMsg_Tx_t *pTxEapol, uint8_t *da, uint8_t *sa)
{
	__aeabi_memcpy(pTxEapol->ethHdr.da, da, sizeof(pTxEapol->ethHdr.da));
	__aeabi_memcpy(pTxEapol->ethHdr.sa, sa, sizeof(pTxEapol->ethHdr.sa));
	pTxEapol->ethHdr.type = ETH_P_EAPOL_SWAPPED;
}

int IsEAPOL_MICValid(EAPOL_KeyMsg_t *pKeyMsg, uint8_t *pMICKey)
{
	uint8_t msgMIC[EAPOL_MIC_SIZE];

	/* pull the MIC */
	__aeabi_memcpy(msgMIC, pKeyMsg->key_MIC, EAPOL_MIC_SIZE);

	/* zero the MIC field before calculating over the data */
	__aeabi_memclr(pKeyMsg->key_MIC, EAPOL_MIC_SIZE);

	ComputeEAPOL_MIC(pKeyMsg, ntohs(pKeyMsg->hdr_8021x.pckt_body_len) +
				  sizeof(pKeyMsg->hdr_8021x),
			 pMICKey, EAPOL_MIC_KEY_SIZE, 0);

	/* a MIC failure when they differ */
	return memcmp(pKeyMsg->key_MIC, msgMIC, EAPOL_MIC_SIZE) == 0;
}

/* The 802.1X header, key data length and MIC of an EAPOL-Key frame to send. */
uint16_t KeyMgmtSta_PopulateEAPOLLengthMic(EAPOL_KeyMsg_Tx_t *pTxEapol,
					   uint8_t *pEAPOLMICKey,
					   uint8_t eapolProtocolVersion,
					   uint8_t forceKeyDescVersion)
{
	uint16_t frameLen;

	if (KeyMgmtSta_PopulateEAPOLLengthMic_hook(pTxEapol, pEAPOLMICKey,
						   eapolProtocolVersion,
						   forceKeyDescVersion, &frameLen))
		return frameLen;

	if (!pTxEapol)
		return 0;

	frameLen = sizeof(pTxEapol->keyMsg);
	frameLen -= sizeof(pTxEapol->keyMsg.hdr_8021x);
	frameLen -= sizeof(pTxEapol->keyMsg.key_data);
	frameLen += pTxEapol->keyMsg.key_material_len;

	pTxEapol->keyMsg.hdr_8021x.protocol_ver = eapolProtocolVersion;
	pTxEapol->keyMsg.hdr_8021x.pckt_type = IEEE_8021X_PACKET_TYPE_EAPOL_KEY;
	pTxEapol->keyMsg.hdr_8021x.pckt_body_len = htons(frameLen);

	pTxEapol->keyMsg.key_material_len = htons(pTxEapol->keyMsg.key_material_len);

	ComputeEAPOL_MIC(&pTxEapol->keyMsg, frameLen + sizeof(pTxEapol->keyMsg.hdr_8021x),
			 pEAPOLMICKey, EAPOL_MIC_KEY_SIZE, forceKeyDescVersion);

	return frameLen;
}

/* pIe as a KDE (a vendor element with the IEEE 802.11 OUI), or NULL. */
KDE_t *parseKeyKDE(IEEEtypes_InfoElementHdr_t *pIe)
{
	KDE_t *pKde = NULL;

	if (pIe->ElementId == ELEM_ID_VENDOR_SPECIFIC) {
		pKde = (KDE_t *)pIe;

		if (pKde->length > sizeof(KDE_t) &&
		    !memcmp(pKde->OUI, kde_oui, sizeof(kde_oui)))
			return pKde;
	}

	return NULL;
}

/* The first KDE of type KDEDataType in the key data, or NULL. */
KDE_t *parseKeyKDE_DataType(uint8_t *pData, int32_t dataLen, int KDEDataType)
{
	IEEEtypes_InfoElementHdr_t *pIe;
	KDE_t *pKde;
	uint32_t ptr_val;

	if (parseKeyKDE_DataType_hook(pData, dataLen, KDEDataType, &ptr_val))
		return (KDE_t *)ptr_val;

	if (pData == NULL)
		return NULL;

	while (dataLen > (int32_t)sizeof(IEEEtypes_InfoElementHdr_t)) {
		pIe = (IEEEtypes_InfoElementHdr_t *)pData;

		if (pIe->ElementId == ELEM_ID_VENDOR_SPECIFIC) {
			pKde = parseKeyKDE(pIe);

			if (pKde != NULL && pKde->dataType == KDEDataType)
				return pKde;
			else if (pIe->Len == 0)
				/* the rest is padding: end the loop */
				dataLen = sizeof(IEEEtypes_InfoElementHdr_t);
		}

		dataLen -= pIe->Len + sizeof(IEEEtypes_InfoElementHdr_t);
		pData += pIe->Len + sizeof(IEEEtypes_InfoElementHdr_t);
	}

	return NULL;
}

/* The group key and its index from the GTK KDE of the key data. */
KDE_t *parseKeyDataGTK(uint8_t *pKey, uint16_t len, KeyData_t *pGRKey)
{
	GTK_KDE_t *pGtk;
	KDE_t *pKde;
	uint32_t ptr_val;

	if (parseKeyDataGTK_hook(pKey, len, pGRKey, &ptr_val))
		return (KDE_t *)ptr_val;

	pKde = parseKeyKDE_DataType(pKey, len, KDE_DATA_TYPE_GTK);

	if (pKde) {
		pGtk = (GTK_KDE_t *)pKde->data;

		/* the KDE overhead is 6 bytes */
		__aeabi_memcpy(pGRKey->Key, pGtk->GTK, pKde->length - 6);

		/* save the group key index */
		pGRKey->KeyIndex = pGtk->KeyID;
	}

	return pKde;
}

/* Decrypts the key data with the KEK: AES key wrap or RC4 by descriptor version. */
void KeyMgmtSta_ApplyKEK(EAPOL_KeyMsg_t *pKeyMsg, KeyData_t *pGRKey,
			 uint8_t *EAPOL_Encr_Key)
{
	if (KeyMgmtSta_ApplyKEK_hook(pKeyMsg, pGRKey, EAPOL_Encr_Key))
		return;

	pGRKey->TxIV16 = pKeyMsg->key_RSC[1] << 8;
	pGRKey->TxIV16 |= pKeyMsg->key_RSC[0];
	pGRKey->TxIV32 = 0xffffffff;

	pKeyMsg->key_material_len = ntohs(pKeyMsg->key_material_len) & 0xffff;

	switch (pKeyMsg->key_info.KeyDescriptorVersion) {
	case 3:
	case 2:
		/* CCMP: AES key wrap (RFC 3394) with the KEK */
		MRVL_AesUnWrap(EAPOL_Encr_Key, 2, pKeyMsg->key_material_len / 8 - 1,
			       pKeyMsg->key_data, NULL, pKeyMsg->key_data);

		/* the key wrap adds 8 bytes for the default IV */
		pKeyMsg->key_material_len -= 8;
		break;

	default:
	case 1:
		/* TKIP or WEP: ARC4 with the KEK, skipping the first 256 bytes of the stream */
		RC4_Encrypt(EAPOL_Encr_Key, pKeyMsg->EAPOL_key_IV,
			    sizeof(pKeyMsg->EAPOL_key_IV), pKeyMsg->key_data,
			    pKeyMsg->key_material_len, 256);
		break;
	}
}

/* Checks a received 4-way or group key handshake frame. */
int KeyMgmtSta_IsRxEAPOLValid(keyMgmtInfoSta_t *pKeyMgmtInfoSta, EAPOL_KeyMsg_t *pKeyMsg)
{
	int ptr_val;

	if (KeyMgmtSta_IsRxEAPOLValid_hook(pKeyMgmtInfoSta, pKeyMsg, &ptr_val))
		return ptr_val;

	if (!pKeyMgmtInfoSta || !pKeyMsg)
		return 0;

	if (!isApReplayCounterFresh(pKeyMgmtInfoSta, (uint8_t *)pKeyMsg->replay_cnt))
		return 0;

	/* check whether the MIC must be verified */
	if (pKeyMsg->key_info.KeyMIC) {
		/* KeyType 1 is message 3 of the 4-way handshake: verify the ANonce */
		if (pKeyMsg->key_info.KeyType == 1 &&
		    memcmp(pKeyMsg->key_nonce, pKeyMgmtInfoSta->ANonce, NONCE_SIZE) != 0)
			return 0;

		if (!IsEAPOL_MICValid(pKeyMsg, pKeyMgmtInfoSta->EAPOL_MIC_Key))
			return 0;
	}
	return 1;
}

/*
 * The fields common to message 2 and 4 of the 4-way handshake and message 2
 * of the group key handshake; the caller overwrites what differs.
 */
void KeyMgmtSta_PrepareEAPOLFrame(EAPOL_KeyMsg_Tx_t *pTxEapol, EAPOL_KeyMsg_t *pRxEapol,
				  uint8_t *da, uint8_t *sa, uint8_t *pSNonce)
{
	if (KeyMgmtSta_PrepareEAPOLFrame_hook(pTxEapol, pRxEapol, da, sa, pSNonce))
		return;

	if (!pTxEapol || !pRxEapol)
		return;

	__aeabi_memclr(pTxEapol, sizeof(EAPOL_KeyMsg_Tx_t));

	formEAPOLEthHdr(pTxEapol, da, sa);

	pTxEapol->keyMsg.desc_type = pRxEapol->desc_type;
	pTxEapol->keyMsg.key_info.KeyType = pRxEapol->key_info.KeyType;
	pTxEapol->keyMsg.key_info.KeyMIC = 1;
	pTxEapol->keyMsg.key_info.Secure = pRxEapol->key_info.Secure;
	pTxEapol->keyMsg.replay_cnt[0] = pRxEapol->replay_cnt[0];
	pTxEapol->keyMsg.replay_cnt[1] = pRxEapol->replay_cnt[1];

	pTxEapol->keyMsg.key_info.KeyDescriptorVersion =
		pRxEapol->key_info.KeyDescriptorVersion;

	/* only for message 2 of the 4-way handshake */
	if (pSNonce)
		__aeabi_memcpy(pTxEapol->keyMsg.key_nonce, pSNonce, NONCE_SIZE);
}

/* The Michael MIC failure report. */
void KeyMgmtSta_PrepareEAPOLMicErrFrame(EAPOL_KeyMsg_Tx_t *pTxEapol, int isUnicast,
					IEEEtypes_MacAddr_t *da, IEEEtypes_MacAddr_t *sa,
					keyMgmtInfoSta_t *pKeyMgmtInfoSta)
{
	if (KeyMgmtSta_PrepareEAPOLMicErrFrame_hook(pTxEapol, isUnicast, da, sa,
						    pKeyMgmtInfoSta))
		return;

	if (!pTxEapol || !pKeyMgmtInfoSta)
		return;

	__aeabi_memclr(pTxEapol, sizeof(EAPOL_KeyMsg_Tx_t));

	formEAPOLEthHdr(pTxEapol, (uint8_t *)da, (uint8_t *)sa);

	pTxEapol->keyMsg.key_info.KeyType = isUnicast;
	pTxEapol->keyMsg.key_info.KeyMIC = 1;
	pTxEapol->keyMsg.key_info.Secure = 1;
	pTxEapol->keyMsg.key_info.Error = 1;
	pTxEapol->keyMsg.key_info.Request = 1;
	pTxEapol->keyMsg.replay_cnt[0] = htonl(pKeyMgmtInfoSta->staCounterHi);
	pTxEapol->keyMsg.replay_cnt[1] = htonl(pKeyMgmtInfoSta->staCounterLo);
}

int supplicantAkmUsesKdf(AkmSuite_t *pAkm)
{
	if (supplicantAkmIsWpa2(pAkm)) {
		if (pAkm->akmType == AKM_SHA256_PSK || pAkm->akmType == AKM_SHA256_1X ||
		    pAkm->akmType == AKM_FT_PSK || pAkm->akmType == AKM_FT_1X)
			return 1;
	}
	return 0;
}

/* min(AA, SPA) || max(AA, SPA) || min(ANonce, SNonce) || max(ANonce, SNonce) */
void supplicantConstructContext(uint8_t *pAddr1, uint8_t *pAddr2, uint8_t *pNonce1,
				uint8_t *pNonce2, uint8_t *pContext)
{
	if (memcmp(pAddr1, pAddr2, 6) < 0) {
		__aeabi_memcpy(pContext, pAddr1, sizeof(IEEEtypes_MacAddr_t));
		__aeabi_memcpy(pContext + 6, pAddr2, sizeof(IEEEtypes_MacAddr_t));
	} else {
		__aeabi_memcpy(pContext, pAddr2, sizeof(IEEEtypes_MacAddr_t));
		__aeabi_memcpy(pContext + 6, pAddr1, sizeof(IEEEtypes_MacAddr_t));
	}

	if (memcmp(pNonce1, pNonce2, NONCE_SIZE) < 0) {
		__aeabi_memcpy(pContext + 6 + 6, pNonce1, NONCE_SIZE);
		__aeabi_memcpy(pContext + 6 + 6 + NONCE_SIZE, pNonce2, NONCE_SIZE);
	} else {
		__aeabi_memcpy(pContext + 6 + 6, pNonce2, NONCE_SIZE);
		__aeabi_memcpy(pContext + 6 + 6 + NONCE_SIZE, pNonce1, NONCE_SIZE);
	}
}

/*
 * The pairwise transient key. pPTK is an encryption pool buffer (at least 500
 * bytes): the PTK goes at its start and the 76-byte context at +200, followed
 * by room for the two digests the SHA-256 routine writes after it.
 */
void KeyMgmt_DerivePTK(uint8_t *pAddr1, uint8_t *pAddr2, uint8_t *pNonce1,
		       uint8_t *pNonce2, uint8_t *pPTK, uint8_t *pPMK, int use_kdf)
{
	uint8_t *pContext;
	char *prefix;

	pContext = pPTK + 200;

	supplicantConstructContext(pAddr1, pAddr2, pNonce1, pNonce2, pContext);

	prefix = "Pairwise key expansion";

	if (use_kdf)
		mrvl_sha256_crypto_kdf(pPMK, PMK_LEN_MAX, prefix, 22, /* strlen(prefix) */
				       pContext, 76, /* the context's size */
				       pPTK, 384);
	else
		Mrvl_PRF(pPMK, PMK_LEN_MAX, (uint8_t *)prefix, 22, /* strlen(prefix) */
			 pContext, 76, /* the context's size */
			 pPTK, 64);
}

/* The KCK, KEK and temporal key from the PMK and both nonces. */
void KeyMgmtSta_DeriveKeys(uint8_t *pPMK, uint8_t *da, uint8_t *sa, uint8_t *ANonce,
			   uint8_t *SNonce, uint8_t *EAPOL_MIC_Key, uint8_t *EAPOL_Encr_Key,
			   KeyData_t *newPWKey, int use_kdf)
{
	OsaBuffer *pBufDesc;
	TkipPtk_t *pPtk;

	if (KeyMgmtSta_DeriveKeys_hook(pPMK, da, sa, ANonce, SNonce, EAPOL_MIC_Key,
				       EAPOL_Encr_Key, newPWKey, use_kdf))
		return;

	if (!pPMK || !EAPOL_MIC_Key || !newPWKey)
		return;

	/* waiting forever ensures a buffer */
	pBufDesc = osa_BufferAlloc(ramHook_encrPoolConfig, 500, 0xffffffff);
	pPtk = (TkipPtk_t *)((uint8_t *)pBufDesc + pBufDesc->dataOffset);

	KeyMgmt_DerivePTK(sa, da, ANonce, SNonce, (uint8_t *)pPtk, pPMK, use_kdf);

	__aeabi_memcpy(EAPOL_MIC_Key, pPtk->kck, sizeof(pPtk->kck));
	__aeabi_memcpy(EAPOL_Encr_Key, pPtk->kek, sizeof(pPtk->kek));
	__aeabi_memcpy(newPWKey->Key, pPtk->tk, sizeof(pPtk->tk));
	__aeabi_memcpy(newPWKey->RxMICKey, pPtk->rxMicKey, sizeof(pPtk->rxMicKey));
	__aeabi_memcpy(newPWKey->TxMICKey, pPtk->txMicKey, sizeof(pPtk->txMicKey));

	osa_BufferFree(pBufDesc);
}
