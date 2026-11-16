/*
 * The key API's half in ITCM (0x2e2dc .. 0x2e602 and 0x2e726): installing key
 * material into a connection's pairwise and group keys, disabling them, and
 * reading them back, with the RAM hooks it calls the connection's state
 * through. Marvell's host port of the 8977's keyMgmtSta.c keeps this code as
 * comments in keyApi_UpdateKeyMaterial(); the file name is inferred from
 * keyApiStaTypes.h, and the names other than Marvell's are inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/rom.h"

#define IS_KEY_STATE_ENABLED(key)	((key)->hdr.keyState & KEY_STATE_ENABLE)
#define SET_KEY_STATE_ENABLED(key, enable)					\
	((key)->hdr.keyState = (enable) ? (key)->hdr.keyState | KEY_STATE_ENABLE	\
					: (key)->hdr.keyState & ~KEY_STATE_ENABLE)

/* The key buffers are word-aligned: their keys are copied word by word, without a call. */
#ifdef __ARMCC_VERSION
#define KEY_WORDS(field)	((void *)(field))
#else
#define KEY_WORDS(field)	__builtin_assume_aligned(field, 4)
#endif

/* The RAM hooks, 0x54ef8 .. 0x54f14 (keyApiSta_RomInit()). */
void (*ramHook_keyApi_PalladiumHook1)(struct WlanConnection *connPtr) __attribute__((section(".itcm.rw")));
keyApiSta_ConnHook_t ramHook_keyApiSta_setConnStaticWepEnabled __attribute__((section(".itcm.rw")));
keyApiSta_ConnHook_t ramHook_keyApiSta_setConnExcludeUnencrypted __attribute__((section(".itcm.rw")));
keyApiSta_ConnHook_t ramHook_keyApiSta_setConnCurPktTxEnabled __attribute__((section(".itcm.rw")));
keyApiSta_ConnHook_t ramHook_keyApiSta_setConnDataTrafficEnabled __attribute__((section(".itcm.rw")));
keyApiSta_ConnHook_t ramHook_keyApiSta_nullHook __attribute__((section(".itcm.rw")));
BOOLEAN (*ramHook_keyApiSta_isConnStaticWepEnabled)(struct WlanConnection *connPtr) __attribute__((section(".itcm.rw")));
keyApiSta_ConnHook_t ramHook_keyApiSta_setConnWapiEnabled __attribute__((section(".itcm.rw")));

/* The keys are disabled: the pairwise one with the connection's traffic, the group one with the unprotected-frame filter. */
void keyApiSta_DisableKeys(struct WlanConnection *connPtr, cipher_key_t *pwkey, cipher_key_t *gwkey,
			   BOOLEAN pairwise, BOOLEAN group)
{
	if (pairwise) {
		if (pwkey != NULL) {
			pwkey->hdr.keyType = KEY_TYPE_WEP;
			pwkey->hdr.keyState = (pwkey->hdr.keyState & ~KEY_STATE_ENABLE) |
					      KEY_STATE_FORCE_EAPOL_UNENCRYPTED;
		}
		ramHook_keyApiSta_setConnCurPktTxEnabled(connPtr, 0);
		ramHook_keyApiSta_setConnDataTrafficEnabled(connPtr, 0);
		ramHook_keyApiSta_setConnExcludeUnencrypted(connPtr, 0);
		ramHook_keyApiSta_setConnStaticWepEnabled(connPtr, 0);
	}
	if (group) {
		if (gwkey != NULL) {
			gwkey->hdr.keyType = KEY_TYPE_WEP;
			gwkey->hdr.keyState &= ~KEY_STATE_ENABLE;
		}
		ramHook_keyApiSta_setConnExcludeUnencrypted(connPtr, 0);
	}
}

/*
 * The key material is installed: a WEP key goes into the group key's table of
 * default keys (and into the pairwise key, unless that one holds another
 * enabled key), a TKIP or AES key into the pairwise or group key as its key
 * information says; an AES pairwise key is copied into a disabled group key
 * for ad hoc. Enabling a key opens the connection's traffic. The original
 * writes through a missing key where it does not test for one. Returns 0.
 */
uint32_t keyApiSta_UpdateKeyMaterial(struct WlanConnection *connPtr, cipher_key_t *pwkey,
				     cipher_key_t *gwkey, key_MgtMaterial_t *keyMgtData_p)
{
	MIB_WEP_DEFAULT_KEYS *wepKey;
	uint8_t keyIndex;

	switch (keyMgtData_p->keyType) {
	case KEY_TYPE_WEP:
		SET_KEY_STATE_ENABLED(gwkey, keyMgtData_p->keyInfo & KEY_INFO_ENABLED);
		gwkey->hdr.keyType = keyMgtData_p->keyType;
		gwkey->hdr.keyDirection = KEY_DIRECTION_RXTX;
		gwkey->hdr.keyLen = keyMgtData_p->keyLen;
		keyIndex = keyMgtData_p->keyEncypt.WEP.keyIndex;
		wepKey = &gwkey->ckd.wep.WepDefaultKeys[keyIndex];
		__aeabi_memcpy(wepKey->WepDefaultKeyValue, keyMgtData_p->keyEncypt.WEP.key,
			       WPA_WEP104_KEY_LEN);
		if (keyMgtData_p->keyLen == WPA_WEP104_KEY_LEN) {
			if (gwkey != NULL)
				gwkey->hdr.keyInfo |= KEY_INFO_WEP_SUBTYPE;
			wepKey->WepDefaultKeyType = 1;
		} else {
			if (gwkey != NULL)
				gwkey->hdr.keyInfo &= ~KEY_INFO_WEP_SUBTYPE;
			wepKey->WepDefaultKeyType = 0;
		}
		wepKey->WepDefaultKeyIdx = keyIndex;
		if (keyMgtData_p->keyEncypt.WEP.isDefaultTx)
			gwkey->ckd.wep.default_key_idx = keyIndex;
		if (gwkey == NULL || !IS_KEY_STATE_ENABLED(gwkey))
			return 0;
		ramHook_keyApiSta_setConnCurPktTxEnabled(connPtr, 1);
		ramHook_keyApiSta_setConnDataTrafficEnabled(connPtr, 1);
		if (pwkey != NULL && IS_KEY_STATE_ENABLED(pwkey) && pwkey->hdr.keyType != KEY_TYPE_WEP) {
			/* the original then goes on as for an AES key, with the WEP material */
			if (pwkey->hdr.keyType != KEY_TYPE_AES)
				return 0;
			goto tkipAes;
		}
		/* the pairwise key becomes the group's WEP keys */
		pwkey->hdr.keyState |= KEY_STATE_ENABLE;
		pwkey->hdr.keyType = keyMgtData_p->keyType;
		pwkey->hdr.keyDirection = KEY_DIRECTION_RXTX;
		pwkey->hdr.keyLen = keyMgtData_p->keyLen;
		__aeabi_memcpy4(&pwkey->ckd, &gwkey->ckd, sizeof(wep_key_data_t));
		return 0;
	case KEY_TYPE_TKIP:
	case KEY_TYPE_AES:
tkipAes:
		if (keyMgtData_p->keyInfo & KEY_INFO_UNICAST) {
			SET_KEY_STATE_ENABLED(pwkey, keyMgtData_p->keyInfo & KEY_INFO_ENABLED);
			pwkey->hdr.keyType = keyMgtData_p->keyType;
			pwkey->hdr.keyDirection = KEY_DIRECTION_RXTX;
			pwkey->hdr.keyLen = keyMgtData_p->keyLen;
			if (pwkey != NULL && IS_KEY_STATE_ENABLED(pwkey)) {
				ramHook_keyApiSta_setConnCurPktTxEnabled(connPtr, 1);
				ramHook_keyApiSta_setConnDataTrafficEnabled(connPtr, 1);
				pwkey->hdr.keyState |= KEY_STATE_FORCE_EAPOL_UNENCRYPTED;
			}
			if (keyMgtData_p->keyLen != 0) {
				if (pwkey->hdr.keyType == KEY_TYPE_TKIP) {
					__aeabi_memcpy(pwkey->ckd.aes.key, keyMgtData_p->keyEncypt.TKIP.key, TK_SIZE);
					__aeabi_memcpy(pwkey->ckd.aes.txMicKey, keyMgtData_p->keyEncypt.TKIP.txMicKey, 8);
					__aeabi_memcpy(pwkey->ckd.aes.rxMicKey, keyMgtData_p->keyEncypt.TKIP.rxMicKey, 8);
				} else if (pwkey->hdr.keyType == KEY_TYPE_AES) {
					__aeabi_memcpy(pwkey->ckd.aes.key, keyMgtData_p->keyEncypt.AES.key, TK_SIZE);
					/* duplicate to the group key, for ad hoc AES */
					if (gwkey == NULL || !IS_KEY_STATE_ENABLED(gwkey)) {
						SET_KEY_STATE_ENABLED(gwkey, keyMgtData_p->keyInfo & KEY_INFO_ENABLED);
						gwkey->hdr.keyType = keyMgtData_p->keyType;
						gwkey->hdr.keyDirection = KEY_DIRECTION_RXTX;
						gwkey->hdr.keyLen = keyMgtData_p->keyLen;
						if (gwkey != NULL && IS_KEY_STATE_ENABLED(gwkey)) {
							gwkey->ckd.aes.rxIv16 = 0;
							gwkey->ckd.aes.rxIv32 = 0xffffffff;
						}
						__aeabi_memcpy(gwkey->ckd.aes.key, keyMgtData_p->keyEncypt.AES.key,
							       TK_SIZE);
					}
				}
			}
		}
		if (keyMgtData_p->keyInfo & KEY_INFO_MULTICAST) {
			SET_KEY_STATE_ENABLED(gwkey, keyMgtData_p->keyInfo & KEY_INFO_ENABLED);
			gwkey->hdr.keyType = keyMgtData_p->keyType;
			gwkey->hdr.keyDirection = KEY_DIRECTION_RXTX;
			gwkey->hdr.keyLen = keyMgtData_p->keyLen;
			if (gwkey != NULL && IS_KEY_STATE_ENABLED(gwkey)) {
				gwkey->ckd.aes.rxIv16 = 0;
				gwkey->ckd.aes.rxIv32 = 0xffffffff;
				if (pwkey == NULL || !IS_KEY_STATE_ENABLED(pwkey)) {
					gwkey->ckd.aes.txIv32 = 0;
					gwkey->ckd.aes.txIv16 = 1;
					ramHook_keyApiSta_setConnCurPktTxEnabled(connPtr, 1);
					ramHook_keyApiSta_setConnDataTrafficEnabled(connPtr, 1);
				}
			}
			if (keyMgtData_p->keyLen != 0) {
				if (gwkey->hdr.keyType == KEY_TYPE_TKIP) {
					__aeabi_memcpy(gwkey->ckd.aes.key, keyMgtData_p->keyEncypt.TKIP.key, TK_SIZE);
					__aeabi_memcpy(gwkey->ckd.aes.txMicKey, keyMgtData_p->keyEncypt.TKIP.txMicKey, 8);
					__aeabi_memcpy(gwkey->ckd.aes.rxMicKey, keyMgtData_p->keyEncypt.TKIP.rxMicKey, 8);
				} else if (gwkey->hdr.keyType == KEY_TYPE_AES) {
					__aeabi_memcpy(gwkey->ckd.aes.key, keyMgtData_p->keyEncypt.AES.key, TK_SIZE);
				}
			}
		}
		ramHook_keyApi_PalladiumHook1(connPtr);
		return 0;
	default:
		return 0;
	}
}

/*
 * A key as key material (HostCmd 0x5e's TLV): the pairwise key or the group
 * key, its type and whether it is enabled, and the TKIP or AES key. Returns
 * the material's size, 0 without a key.
 */
uint16_t keyApiSta_GetKeyMaterial(cipher_key_t *pwkey, cipher_key_t *gwkey,
				  key_MgtMaterial_t *keyMgtData_p, BOOLEAN pairwise)
{
	cipher_key_t *key;

	if (keyMgtData_p == NULL)
		return 0;
	if (pairwise) {
		if (pwkey == NULL)
			return 0;
		key = pwkey;
		keyMgtData_p->keyType = key->hdr.keyType;
		keyMgtData_p->keyInfo = KEY_INFO_UNICAST;
		if (IS_KEY_STATE_ENABLED(key))
			keyMgtData_p->keyInfo = KEY_INFO_UNICAST | KEY_INFO_ENABLED;
	} else {
		if (gwkey == NULL)
			return 0;
		key = gwkey;
		keyMgtData_p->keyType = key->hdr.keyType;
		keyMgtData_p->keyInfo = KEY_INFO_MULTICAST;
		if (IS_KEY_STATE_ENABLED(key))
			keyMgtData_p->keyInfo = KEY_INFO_MULTICAST | KEY_INFO_ENABLED;
	}
	if (key->hdr.keyType == KEY_TYPE_TKIP) {
		keyMgtData_p->keyLen = WPA_TKIP_KEY_LEN;
		__aeabi_memcpy(keyMgtData_p->keyEncypt.TKIP.key, key->ckd.aes.key, TK_SIZE);
		__aeabi_memcpy(keyMgtData_p->keyEncypt.TKIP.txMicKey, key->ckd.aes.txMicKey, 8);
		__aeabi_memcpy(keyMgtData_p->keyEncypt.TKIP.rxMicKey, key->ckd.aes.rxMicKey, 8);
	} else {
		keyMgtData_p->keyLen = WPA_AES_KEY_LEN;
		__aeabi_memcpy(keyMgtData_p->keyEncypt.AES.key, key->ckd.aes.key, TK_SIZE);
	}
	return keyMgtData_p->keyLen + 6;
}

/*
 * An access point's key from its key data (the group key, or a station's
 * pairwise key after the handshake): AES or TKIP as the cipher says, enabled,
 * the receive counters cleared and the send counter taken over.
 */
void keyApiSta_SetKeyFromKeyData(BOOLEAN groupKey, cipher_key_t *key, Cipher_t *cipher,
				 KeyData_t *keyData)
{
	uint8_t keyInfo;

	if (groupKey == 1)
		keyInfo = KEY_INFO_GWK | KEY_INFO_GLOBAL;
	else
		keyInfo = KEY_INFO_PWK | KEY_INFO_KEY_MAPPING_KEY;
	if (cipher->ccmp)
		key->hdr.keyType = KEY_TYPE_AES;
	else if (cipher->tkip)
		key->hdr.keyType = KEY_TYPE_TKIP;
	key->hdr.keyDirection = KEY_DIRECTION_RXTX;
	key->hdr.keyLen = TK_SIZE;
	key->hdr.keyInfo = keyInfo;
	key->hdr.keyState |= KEY_STATE_ENABLE;
	__builtin_memcpy(KEY_WORDS(key->ckd.aes.key), KEY_WORDS(keyData->Key), TK_SIZE);
	__builtin_memcpy(KEY_WORDS(key->ckd.aes.txMicKey), KEY_WORDS(keyData->TxMICKey), 8);
	__builtin_memcpy(KEY_WORDS(key->ckd.aes.rxMicKey), KEY_WORDS(keyData->RxMICKey), 8);
	key->ckd.aes.rxIv32 = 0;
	key->ckd.aes.rxIv16 = 0;
	key->ckd.aes.decryptState[1] = 0;
	key->ckd.aes.decryptState[0] = 0;
	key->ckd.aes.txIv32 = keyData->TxIV32;
	key->ckd.aes.txIv16 = keyData->TxIV16;
}
