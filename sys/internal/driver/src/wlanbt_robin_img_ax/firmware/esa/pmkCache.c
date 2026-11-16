/*
 * The supplicant's passphrase and PMK commands (Marvell's common/pmkCache.c,
 * SupplicantSetPassphrase, SupplicantQueryPassphrase and
 * SupplicantClearPMK_internal), as this firmware has them: they take the TLVs
 * of the host's SUPPLICANT_PMK command and not Marvell's driver structure.
 * The names of the hooks (0x54ea0, 0x54ea4, 0x548a8) are inferred.
 *
 * Written from Marvell's source (Copyright (C) 2014-2017, Marvell International
 * Ltd., GPL-2.0) as this firmware has it.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/esa.h"
#include "robin/join.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sqram.h"
#include "robin/clock.h"
#include "robin/stream.h"

#define LE16(bytes)	((uint16_t)((bytes)[0] | ((bytes)[1] << 8)))

/* Override slots of the three commands (0x55c04, 0x55c08, 0x55c0c). */
static ROBIN_PATCH_SLOT(SupplicantSetPassphraseOverride);
static ROBIN_PATCH_SLOT(SupplicantQueryPassphraseOverride);
static ROBIN_PATCH_SLOT(SupplicantClearPMKOverride);

/* Set by the ROM: the supplicant's switch on, and the search of the PSK of an SSID. */
void (*ramHook_supplicantEnable)(void) __attribute__((section(".itcm.bss")));				/* 0x54ea0 */
uint8_t *(*ramHook_pmkCacheFindPSK)(uint8_t *pSsid, uint8_t ssidLen) __attribute__((section(".itcm.bss")));	/* 0x54ea4 */

/*
 * The cache itself and the passphrase for networks the host named no SSID for, which
 * pmkCacheRamInit() hands to the ROM's code through its hooks; the RAM functions of
 * Marvell's pmkCache.c are in SQRAM.
 */
#define PMK_CACHE_ENTRIES	10
static pmkElement_t pmkCache[PMK_CACHE_ENTRIES] __attribute__((section(".itcm.bss")));		/* 0x568ce */
static uint8_t PSKPassPhrase[PSK_PASS_PHRASE_LEN_MAX] __attribute__((section(".itcm.bss")));	/* 0x56b62 */

/* 0xc0008190: the PSK of an SSID; made from the passphrase and cached when there is none yet. */
SQRAM_CODE uint8_t *pmkCacheFindPSK(uint8_t *pSsid, uint8_t ssidLen)
{
	uint8_t *pPMK = NULL;
	pmkElement_t *pPMKElement;

	pPMKElement = pmkCacheFindPSKElement(pSsid, ssidLen);
	if (pPMKElement != NULL) {
		pPMK = pPMKElement->PMK;
	} else if (PSKPassPhrase[0] != 0) {
		pmkCacheAddPSK(pSsid, ssidLen, NULL);
		pPMKElement = pmkCacheFindPSKElement(pSsid, ssidLen);
		pPMK = pPMKElement->PMK;
		pmkCacheGeneratePSK(pSsid, ssidLen, (char *)PSKPassPhrase, pPMK);
	}
	return pPMK;
}

/* 0xc00081d4 */
SQRAM_CODE void pmkCacheInit(void)
{
	__aeabi_memclr(pmkCache, sizeof(pmkCache));
	__aeabi_memclr(PSKPassPhrase, sizeof(PSKPassPhrase));
	replacementRankMax = 0;
}

/* 0xc00081f0 */
SQRAM_CODE void pmkCacheFlush(void)
{
	__aeabi_memclr(pmkCache, sizeof(pmkCache));
	replacementRankMax = 0;
}

/* 0xc0008204: the ROM's cache code works on this cache, and raises the CPU clock through these. */
SQRAM_CODE void pmkCacheRamInit(void)
{
	ramHook_MAX_PMK_CACHE_ENTRIES = PMK_CACHE_ENTRIES;
	ramHook_pmkCache = pmkCache;
	ramHook_PSKPassPhrase = PSKPassPhrase;
	ramHook_hal_SetCpuMaxSpeed = clockHostSelectPreset1;
	ramHook_hal_RestoreCpuSpeed = wlanBaTablesChangedHandler;
}

unsigned int SupplicantSetPassphrase(const uint8_t *buffer, int length)
{
	int result = 0;
	unsigned int ssidLen = 0;
	const uint8_t *pPMK = NULL;
	const uint8_t *pBssid = NULL;
	const uint8_t *pSsid = NULL;
	const uint8_t *passphraseTlv = NULL;
	const uint8_t *tlv;
	uint8_t Passphrase[PSK_PASS_PHRASE_LEN_MAX];
	int16_t remaining = length;
	unsigned int type;

	if (SupplicantSetPassphraseOverride(buffer, length, &result) == 0) {
		while (remaining > 0) {
			tlv = buffer + length - remaining;
			type = LE16(tlv);
			if (type == TLV_TYPE_SSID) {
				pSsid = tlv + 4;
				ssidLen = (uint8_t)LE16(tlv + 2);
			}
			else if (type == TLV_TYPE_BSSID) {
				pBssid = tlv + 4;
			}
			else if (type == TLV_TYPE_PASSPHRASE) {
				passphraseTlv = tlv + 4;
				__aeabi_memclr4(Passphrase, PSK_PASS_PHRASE_LEN_MAX);
				__aeabi_memcpy(Passphrase, tlv + 4, LE16(tlv + 2));
			}
			else if (type == TLV_TYPE_PMK) {
				pPMK = tlv + 4;
			}
			remaining = (int16_t)(remaining - LE16(tlv + 2) - 4);
		}
		/* Always enable the supplicant on a set */
		ramHook_supplicantEnable();
		if (pBssid != NULL && pPMK != NULL) {
			pmkCacheAddPMK((IEEEtypes_MacAddr_t *)pBssid, (uint8_t *)pPMK);
		} else if (pSsid != NULL) {
			if (pPMK != NULL) {
				pmkCacheAddPSK((uint8_t *)pSsid, ssidLen, (uint8_t *)pPMK);
			} else if (passphraseTlv != NULL) {
				pmkCacheAddPSK((uint8_t *)pSsid, ssidLen, NULL);
				pPMK = ramHook_pmkCacheFindPSK((uint8_t *)pSsid, ssidLen);
				pmkCacheGeneratePSK((uint8_t *)pSsid, ssidLen, (char *)Passphrase,
						    (uint8_t *)pPMK);
			} else {
				/* Just an SSID so we can't set anything in the cache */
				return 1;
			}
		} else if (passphraseTlv != NULL) {
			__aeabi_memcpy(ramHook_PSKPassPhrase, Passphrase, PSK_PASS_PHRASE_LEN_MAX);
		} else {
			/* Not enough data to set anything in the cache */
			return 1;
		}
	}
	return (uint16_t)result;
}

/*
 * The passphrase or the PMK the cache or the supplicant holds: the answer is written after
 * the TLVs (a PMK TLV), or replaces the passphrase TLV's value; *length grows with it.
 */
unsigned int SupplicantQueryPassphrase(uint8_t *buffer, uint16_t *length)
{
	uint16_t result = 0;
	uint8_t *tlv = NULL;
	uint8_t *pPMK = NULL;
	uint8_t *passphraseTlv = NULL;
	uint8_t *pBssid = NULL;
	uint8_t *pSsid = NULL;
	unsigned int ssidLen = 0;
	uint8_t *end;
	uint8_t *answer;
	uint8_t *pmkTlv;
	uint16_t passphraseLength;
	int16_t remaining = (int16_t)*length;
	unsigned int type;

	if (SupplicantQueryPassphraseOverride(buffer, length, &result) == 0) {
		end = buffer + *length;
		while (remaining > 0) {
			tlv = end - remaining;
			type = LE16(tlv);
			if (type == TLV_TYPE_SSID) {
				pSsid = tlv + 4;
				ssidLen = (uint8_t)LE16(tlv + 2);
			}
			else if (type == TLV_TYPE_BSSID) {
				pBssid = tlv + 4;
			}
			else if (type == TLV_TYPE_PASSPHRASE) {
				passphraseTlv = tlv + 4;
			}
			remaining = (int16_t)(remaining - LE16(tlv + 2) - 4);
		}
		pmkTlv = end;
		answer = end + 4;
		if (pBssid != NULL) {
			pPMK = pmkCacheFindPMK((IEEEtypes_MacAddr_t *)pBssid);
		} else if (pSsid != NULL) {
			pPMK = ramHook_pmkCacheFindPSK(pSsid, ssidLen);
		} else if (passphraseTlv != NULL) {
			__aeabi_memcpy(passphraseTlv, ramHook_PSKPassPhrase, PSK_PASS_PHRASE_LEN_MAX);
			passphraseLength = strlen((char *)passphraseTlv);
			*length += passphraseLength - LE16(tlv + 2);
			tlv[2] = passphraseLength;
			tlv[3] = passphraseLength >> 8;
			return result;
		} else {
			result = 1;
			return result;
		}
		if (pPMK == NULL)
			return 1;
		__aeabi_memcpy(answer, pPMK, 32);
		*length += 36;
		pmkTlv[2] = 32;
		pmkTlv[3] = 0;
		pmkTlv[0] = TLV_TYPE_PMK & 0xff;
		pmkTlv[1] = TLV_TYPE_PMK >> 8;
	}
	return result;
}

/* Removes the PMK of a BSSID, the PSK of an SSID or the passphrase; 0 when the TLVs name none of them. */
int SupplicantClearPMK_internal(const uint8_t *buffer, int length)
{
	int result;
	unsigned int ssidLen = 0;
	const uint8_t *pBssid = NULL;
	const uint8_t *pSsid = NULL;
	const uint8_t *pPassphrase = NULL;
	const uint8_t *tlv;
	int16_t remaining = (int16_t)length;
	unsigned int type;

	if (SupplicantClearPMKOverride(buffer, length, &result) != 0)
		return result;
	while (remaining > 0) {
		tlv = buffer + length - remaining;
		type = LE16(tlv);
		if (type == TLV_TYPE_SSID) {
			pSsid = tlv + 4;
			ssidLen = (uint8_t)LE16(tlv + 2);
		}
		else if (type == TLV_TYPE_BSSID) {
			pBssid = tlv + 4;
		}
		else if (type == TLV_TYPE_PASSPHRASE) {
			pPassphrase = tlv + 4;
		}
		remaining = (int16_t)(remaining - LE16(tlv + 2) - 4);
	}
	if (pBssid != NULL) {
		pmkCacheDeletePMK((uint8_t *)pBssid);
	} else if (pSsid != NULL) {
		pmkCacheDeletePSK((uint8_t *)pSsid, ssidLen);
	} else if (pPassphrase != NULL) {
		/* Clear the global passphrase by setting it to blank */
		__aeabi_memclr(ramHook_PSKPassPhrase, PSK_PASS_PHRASE_LEN_MAX);
	} else {
		return 0;
	}
	return 1;
}
