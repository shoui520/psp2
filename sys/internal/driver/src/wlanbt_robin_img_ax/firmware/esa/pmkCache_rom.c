/*
 * The PMK cache: PMKs keyed by BSSID and PSKs keyed by SSID, replaced least
 * recently used first (Marvell's common/pmkCache_rom.c, 0x64ac..0x677c).
 *
 * Written from Marvell's source (Copyright (C) 2014-2017, Marvell International
 * Ltd., GPL-2.0) as this firmware has it.
 */
#include "robin/esa.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"

int8_t replacementRankMax __attribute__((section(".itcm.rw"))) = 0;
int ramHook_MAX_PMK_CACHE_ENTRIES __attribute__((section(".itcm.rw"))) = 0;
pmkElement_t *ramHook_pmkCache __attribute__((section(".itcm.rw"))) = NULL;
void (*ramHook_hal_SetCpuMaxSpeed)(void) __attribute__((section(".itcm.rw"))) = NULL;
void (*ramHook_hal_RestoreCpuSpeed)(void) __attribute__((section(".itcm.rw"))) = NULL;

ROBIN_PATCH_SLOT(pmkCacheFindPSKElement_hook);
ROBIN_PATCH_SLOT(pmkCacheAddPMK_hook);
ROBIN_PATCH_SLOT(pmkCacheAddPSK_hook);
ROBIN_PATCH_SLOT(pmkCacheGeneratePSK_hook);
ROBIN_PATCH_SLOT(pmkCacheNewElement_hook);
ROBIN_PATCH_SLOT(pmkCacheFindPMKElement_hook);

pmkElement_t *pmkCacheNewElement(void)
{
	uint8_t index;
	pmkElement_t *pPMK = NULL;

	if (pmkCacheNewElement_hook(&pPMK))
		return pPMK;
	for (index = 0; index < ramHook_MAX_PMK_CACHE_ENTRIES; index++) {
		/*
		 * If the cache is full the least recently used entry will be
		 * replaced. Decrement all the replacement ranks to have a free
		 * cache entry.
		 */
		if (ramHook_MAX_PMK_CACHE_ENTRIES == replacementRankMax)
			ramHook_pmkCache[index].replacementRank--;

		/* Either the entry is free or it can be replaced */
		if (!pPMK && ramHook_pmkCache[index].replacementRank == 0) {
			pPMK = &ramHook_pmkCache[index];
			/* clear the entry in case this is a replacement */
			__aeabi_memclr(pPMK, sizeof(pmkElement_t));
			/* the cache isn't full: increment the max possible rank */
			if (ramHook_MAX_PMK_CACHE_ENTRIES > replacementRankMax)
				replacementRankMax++;
			/* set the rank so it is the last to be replaced */
			ramHook_pmkCache[index].replacementRank = replacementRankMax;
		}
	}
	return pPMK;
}

void pmkCacheUpdateReplacementRank(pmkElement_t *pPMKElement)
{
	uint8_t index;

	/* update the replacement rank of an entry that is found */
	if (pPMKElement && pPMKElement->replacementRank != replacementRankMax) {
		/*
		 * The entries with a larger rank are adjusted; the given entry
		 * gets the largest rank.
		 */
		for (index = 0; index < ramHook_MAX_PMK_CACHE_ENTRIES; index++) {
			if (ramHook_pmkCache[index].replacementRank >
			    pPMKElement->replacementRank)
				ramHook_pmkCache[index].replacementRank--;
		}
		pPMKElement->replacementRank = replacementRankMax;
	}
}

/* The PMK entry of a BSSID, or NULL. */
pmkElement_t *pmkCacheFindPMKElement(IEEEtypes_MacAddr_t *pBssid)
{
	uint8_t index = 0;
	pmkElement_t *pPMKElement = NULL;

	if (pmkCacheFindPMKElement_hook(pBssid, &pPMKElement))
		return pPMKElement;
	for (index = 0; index < ramHook_MAX_PMK_CACHE_ENTRIES; index++) {
		/* a valid entry, a PMK, the same BSSID */
		if (ramHook_pmkCache[index].replacementRank > 0 &&
		    ramHook_pmkCache[index].length == BSSID_FLAG &&
		    memcmp(ramHook_pmkCache[index].key.Bssid, pBssid,
			   sizeof(ramHook_pmkCache[index].key.Bssid)) == 0)
			pPMKElement = ramHook_pmkCache + index;
	}
	/* update the rank of the entry found (NULL is accepted) */
	pmkCacheUpdateReplacementRank(pPMKElement);
	return pPMKElement;
}

/* The PSK entry of an SSID, or NULL. */
pmkElement_t *pmkCacheFindPSKElement(uint8_t *pSsid, uint8_t ssidLen)
{
	uint8_t index = 0;
	pmkElement_t *pPMKElement = NULL;

	if (pmkCacheFindPSKElement_hook(pSsid, ssidLen, &pPMKElement))
		return pPMKElement;
	for (index = 0; index < ramHook_MAX_PMK_CACHE_ENTRIES; index++) {
		/* a valid entry, a PSK, the same SSID */
		if (ramHook_pmkCache[index].replacementRank &&
		    ramHook_pmkCache[index].length == ssidLen &&
		    memcmp(ramHook_pmkCache[index].key.Ssid, pSsid, ssidLen) == 0)
			pPMKElement = ramHook_pmkCache + index;
	}
	pmkCacheUpdateReplacementRank(pPMKElement);
	return pPMKElement;
}

uint8_t *pmkCacheFindPMK(IEEEtypes_MacAddr_t *pBssid)
{
	uint8_t *pPMK = NULL;
	pmkElement_t *pPMKElement = pmkCacheFindPMKElement(pBssid);

	/* extract the PMK from the cache entry */
	if (pPMKElement)
		pPMK = pPMKElement->PMK;
	return pPMK;
}

void pmkCacheAddPMK(IEEEtypes_MacAddr_t *pBssid, uint8_t *pPMK)
{
	pmkElement_t *pPMKElement;

	if (pmkCacheAddPMK_hook(pBssid, pPMK))
		return;
	pPMKElement = pmkCacheFindPMKElement(pBssid);
	if (!pPMKElement) {
		/* not found: create a new entry keyed by the BSSID */
		pPMKElement = pmkCacheNewElement();
		__aeabi_memcpy(pPMKElement->key.Bssid, pBssid,
			       sizeof(pPMKElement->key.Bssid));
		/* a length invalid for an SSID flags the entry as a PMK */
		pPMKElement->length = BSSID_FLAG;
	}
	if (pPMK)
		__aeabi_memcpy(pPMKElement->PMK, pPMK, MAX_PMK_SIZE);
}

void pmkCacheAddPSK(uint8_t *pSsid, uint8_t ssidLen, uint8_t *pPSK)
{
	pmkElement_t *pPMKElement;

	if (pmkCacheAddPSK_hook(pSsid, ssidLen, pPSK))
		return;
	pPMKElement = pmkCacheFindPSKElement(pSsid, ssidLen);
	if (!pPMKElement) {
		/* not found: create a new entry keyed by the SSID */
		pPMKElement = pmkCacheNewElement();
		__aeabi_memcpy(pPMKElement->key.Ssid, pSsid, ssidLen);
		pPMKElement->length = ssidLen;
	}
	if (pPSK)
		__aeabi_memcpy(pPMKElement->PMK, pPSK, MAX_PMK_SIZE);
}

void pmkCacheDeletePMK(uint8_t *pBssid)
{
	pmkElement_t *pPMKElement = pmkCacheFindPMKElement((IEEEtypes_MacAddr_t *)pBssid);

	if (pPMKElement) {
		/* a cleared entry has rank 0: it is free */
		__aeabi_memclr(pPMKElement, sizeof(pmkElement_t));
		replacementRankMax--;
	}
}

void pmkCacheDeletePSK(uint8_t *pSsid, uint8_t ssidLen)
{
	pmkElement_t *pPMKElement = pmkCacheFindPSKElement(pSsid, ssidLen);

	if (pPMKElement) {
		__aeabi_memclr(pPMKElement, sizeof(pmkElement_t));
		replacementRankMax--;
	}
}

uint8_t pmkCacheGetHexNibble(uint8_t nibble)
{
	if (nibble >= 'a')
		return nibble - 'a' + 10;
	if (nibble >= 'A')
		return nibble - 'A' + 10;
	return nibble - '0';
}

/*
 * The PSK of a passphrase of 8 to 63 characters (PBKDF2), or of 64
 * hexadecimal digits.
 */
void pmkCacheGeneratePSK(uint8_t *pSsid, uint8_t ssidLen, char *pPassphrase,
			 uint8_t *pPSK)
{
	int i;

	if (pmkCacheGeneratePSK_hook(pSsid, ssidLen, pPassphrase, pPSK))
		return;
	if (!pPSK || !pPassphrase)
		return;
	for (i = 0; i < PSK_PASS_PHRASE_LEN_MAX; i++) {
		if (pPassphrase[i] == 0)
			break;
	}
	if (i > 7 && i < PSK_PASS_PHRASE_LEN_MAX) {
		/* bump the CPU speed for the PSK generation */
		ramHook_hal_SetCpuMaxSpeed();
		Mrvl_PasswordHash(pPassphrase, pSsid, ssidLen, pPSK);
		ramHook_hal_RestoreCpuSpeed();
	} else if (i == PSK_PASS_PHRASE_LEN_MAX) {
		/* convert ASCII to binary */
		for (i = 0; i < PSK_PASS_PHRASE_LEN_MAX; i += 2)
			pPSK[i / 2] = (pmkCacheGetHexNibble(pPassphrase[i]) << 4) |
				      pmkCacheGetHexNibble(pPassphrase[i + 1]);
	}
}
