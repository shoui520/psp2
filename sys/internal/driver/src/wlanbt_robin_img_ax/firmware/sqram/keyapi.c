/*
 * The key API's half in SQRAM (0xc0008304 .. 0xc0008896): the connection
 * state the ITCM half sets through its RAM hooks, the key updates of a
 * station and of an access point (WAPI keys here, the others through
 * keyApiSta_UpdateKeyMaterial()), reading a key back, and dropping keys.
 * Marvell's names where their host port has them (keyApi_UpdateKeyMaterial(),
 * keyApi_ApUpdateKeyMaterial(), the ramHook_keyApiSta_* setters); the rest
 * and the file name are inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/hostcmd.h"
#include "robin/join.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/stream.h"
#include "robin/wapi.h"

#define IS_KEY_STATE_ENABLED(key)	((key)->hdr.keyState & KEY_STATE_ENABLE)

/* The packet number a WAPI key starts from when the host gives none (0x5c365c36...5c36, least significant byte first). */
static const uint8_t wapiInitialPn[16] = {
	0x36, 0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36, 0x5c,
	0x36, 0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36, 0x5c,
};

/* The hooks keyApiSta_RomInit() installs. */
void keyApi_PalladiumHook1(struct WlanConnection *connPtr)
{
}

void keyApiSta_NullHook(struct WlanConnection *connPtr, BOOLEAN enable)
{
}

BOOLEAN keyApiSta_isConnStaticWepEnabled(struct WlanConnection *connPtr)
{
	if (connPtr == NULL)
		return 0;
	return (connPtr->hostFlags >> 3) & 1;
}

void keyApiSta_setConnStaticWepEnabled(struct WlanConnection *connPtr, BOOLEAN enable)
{
	if (connPtr != NULL)
		connPtr->hostFlags = (connPtr->hostFlags & ~WLAN_CONN_HOST_FLAG_8) | ((enable & 1) << 3);
}

void keyApiSta_setConnExcludeUnencrypted(struct WlanConnection *connPtr, BOOLEAN enable)
{
	if (connPtr != NULL)
		connPtr->txOptions = (connPtr->txOptions & ~4) | ((enable & 1) << 2);
}

void keyApiSta_setConnCurPktTxEnabled(struct WlanConnection *connPtr, BOOLEAN enable)
{
	if (connPtr != NULL)
		connPtr->flags = (connPtr->flags & ~WLAN_CONN_ENCRYPTED) | (enable & 1);
}

void keyApiSta_setConnDataTrafficEnabled(struct WlanConnection *connPtr, BOOLEAN enable)
{
	if (connPtr != NULL)
		connPtr->flags = (connPtr->flags & ~WLAN_CONN_JOIN_OPEN) | ((enable & 1) << 1);
}

void keyApiSta_setConnWapiEnabled(struct WlanConnection *connPtr, BOOLEAN enable)
{
	if (connPtr != NULL)
		connPtr->flags = (connPtr->flags & ~WLAN_CONN_WAPI) | ((enable & 1) << 4);
}

/* The connection's pairwise and group keys are disabled (keyApiSta_DisableKeys()). */
void keyApi_DisableKeys(struct WlanConnection *connPtr, BOOLEAN pairwise, BOOLEAN group)
{
	cipher_key_t *pwkey = NULL;
	cipher_key_t *gwkey = NULL;

	if (connPtr == NULL)
		return;
	if (connPtr->keyBlocks[1] != NULL)
		pwkey = &connPtr->keyBlocks[1]->key;
	if (connPtr->keyBlocks[0] != NULL)
		gwkey = &connPtr->keyBlocks[0]->key;
	keyApiSta_DisableKeys(connPtr, pwkey, gwkey, pairwise, group);
}

/* A key buffer is given back, with the copy of the WAPI key it replaced. */
void keyApi_FreeKey(struct WlanKeyBuffer **keyBuffer)
{
	if (*keyBuffer == NULL)
		return;
	if (__aeabi_uread4(&(*keyBuffer)->key.ckd.wapi.pLastKey) != 0) {
		wlanKeyBufferFree((struct WlanKeyBuffer *)__aeabi_uread4(&(*keyBuffer)->key.ckd.wapi.pLastKey));
		__aeabi_uwrite4(0, &(*keyBuffer)->key.ckd.wapi.pLastKey);
	}
	wlanKeyBufferFree(*keyBuffer);
	*keyBuffer = NULL;
}

/*
 * A WAPI key is installed: an enabled key is first copied into its pLastKey
 * buffer; a unicast key takes the peer's address (an access point's starts
 * one packet number further), a group key none. The packet numbers start
 * from the host's receive packet number, byte-reversed, or from
 * wapiInitialPn. Returns 1 without a key, else 0.
 */
static __attribute__((noinline)) uint32_t keyApi_UpdateWapiKey(struct WlanConnection *connPtr, BOOLEAN isAp,
							      const uint8_t *peerAddress, cipher_key_t *pwkey,
							      cipher_key_t *gwkey, key_MgtMaterial_t *keyMgtData_p)
{
	uint8_t initialPn[16];
	uint8_t noPn[16];
	cipher_key_t *key = pwkey;
	struct WlanKeyBuffer *lastKey;
	uint8_t keyInfo = 0;

	__builtin_memcpy(initialPn, wapiInitialPn, sizeof(initialPn));
	__builtin_memset(noPn, 0, sizeof(noPn));
	if (gwkey == NULL && pwkey == NULL)
		return 1;
	if (keyMgtData_p->keyInfo & KEY_INFO_MULTICAST)
		key = gwkey;
	if (key != NULL && IS_KEY_STATE_ENABLED(key)) {
		if (__aeabi_uread4(&key->ckd.wapi.pLastKey) == 0)
			__aeabi_uwrite4((int)wlanKeyBufferAlloc(), &key->ckd.wapi.pLastKey);
		lastKey = (struct WlanKeyBuffer *)__aeabi_uread4(&key->ckd.wapi.pLastKey);
		if (lastKey != NULL)
			__aeabi_memcpy4(&lastKey->key, key, sizeof(cipher_key_t));
	}
	if (keyMgtData_p->keyInfo & KEY_INFO_UNICAST) {
		if (isAp) {
			ramHook_keyApiSta_setConnWapiEnabled(connPtr, 1);
			initialPn[0]++;
		}
		__aeabi_memcpy(key->hdr.macAddr, peerAddress, 6);
		keyInfo = KEY_INFO_PWK | KEY_INFO_KEY_MAPPING_KEY;
		ramHook_keyApiSta_setConnDataTrafficEnabled(connPtr, 1);
		ramHook_keyApiSta_setConnCurPktTxEnabled(connPtr, 1);
		key->ckd.wapi.pnStep = 2;
	}
	if (keyMgtData_p->keyInfo & KEY_INFO_MULTICAST) {
		keyInfo = KEY_INFO_GWK | KEY_INFO_GLOBAL;
		__aeabi_memclr4(key->hdr.macAddr, 6);
		key->ckd.wapi.pnStep = 1;
	}
	key->hdr.keyType = keyMgtData_p->keyType;
	key->hdr.keyDirection = KEY_DIRECTION_RXTX;
	key->hdr.keyLen = keyMgtData_p->keyLen;
	key->hdr.keyInfo = keyInfo;
	if (memcmp(noPn, keyMgtData_p->keyEncypt.WAPI.rxPN, 16) == 0) {
		__aeabi_memcpy(key->ckd.wapi.txPn, initialPn, 16);
		__aeabi_memcpy(key->ckd.wapi.rxPn, initialPn, 16);
	} else {
		wlanReverseBytes(key->ckd.wapi.txPn, keyMgtData_p->keyEncypt.WAPI.rxPN, 16);
		wlanReverseBytes(key->ckd.wapi.rxPn, keyMgtData_p->keyEncypt.WAPI.rxPN, 16);
	}
	key->ckd.wapi.keyIndex = keyMgtData_p->keyEncypt.WAPI.keyIndex;
	__aeabi_memcpy(key->ckd.wapi.dataKey, keyMgtData_p->keyEncypt.WAPI.key, 16);
	__aeabi_memcpy(key->ckd.wapi.micKey, keyMgtData_p->keyEncypt.WAPI.mickey, 16);
	key->hdr.keyState |= KEY_STATE_ENABLE;
	return 0;
}

/*
 * The key material is installed into the given keys. A WEP key also goes into
 * the connection's WEP default keys; a station's or ad hoc connection's new
 * pairwise key clears its receive counter (and its send counter while the port
 * is still closed), and a group key opens the port when the supplicant does not
 * run. The connection changed when its pairwise key is enabled.
 */
static __attribute__((noinline)) uint32_t keyApi_UpdateKeys(struct WlanConnection *connPtr, cipher_key_t *pwkey,
							   cipher_key_t *gwkey, key_MgtMaterial_t *keyMgtData_p)
{
	MIB_WEP_DEFAULT_KEYS *wepKey;
	uint32_t result = 0;
	uint8_t keyIndex;

	if (keyMgtData_p == NULL)
		goto done;
	if (keyMgtData_p->keyType == KEY_TYPE_WAPI)
		result = keyApi_UpdateWapiKey(connPtr, connPtr->mode == 2, connPtr->peerAddress, pwkey, gwkey,
					      keyMgtData_p);
	else
		result = keyApiSta_UpdateKeyMaterial(connPtr, pwkey, gwkey, keyMgtData_p);
	switch (keyMgtData_p->keyType) {
	case KEY_TYPE_WEP:
		keyIndex = keyMgtData_p->keyEncypt.WEP.keyIndex;
		wepKey = &((MIB_WEP_DEFAULT_KEYS *)connPtr->radioParams)[keyIndex];
		wepKey->WepDefaultKeyType = keyMgtData_p->keyLen == WPA_WEP104_KEY_LEN;
		__aeabi_memcpy(wepKey->WepDefaultKeyValue, keyMgtData_p->keyEncypt.WEP.key, WPA_WEP104_KEY_LEN);
		wepKey->WepDefaultKeyIdx = keyIndex;
		break;
	case KEY_TYPE_TKIP:
	case KEY_TYPE_AES:
		if (connPtr->mode != 3 && connPtr->mode != 1)
			break;
		if ((keyMgtData_p->keyInfo & KEY_INFO_UNICAST) && pwkey != NULL && IS_KEY_STATE_ENABLED(pwkey)) {
			pwkey->ckd.aes.rxIv16 = 0;
			pwkey->ckd.aes.rxIv32 = 0xffffffff;
			if (wlanJoinPortIsOpen() == 0) {
				pwkey->ckd.aes.txIv32 = 0;
				pwkey->ckd.aes.txIv16 = 1;
			}
		}
		/* fall through */
	case KEY_TYPE_WAPI:
		if (connPtr->mode != 3 && connPtr->mode != 1)
			break;
		if (supplicantIsEnabled() == 0 && (keyMgtData_p->keyInfo & KEY_INFO_MULTICAST))
			keyMgmtControlledPortOpen();
		break;
	}
done:
	wlanBaTablesChangedHandler();
	if (pwkey != NULL && IS_KEY_STATE_ENABLED(pwkey))
		hostCmdConnectionChanged(connPtr);
	return result;
}

/*
 * The host's or the supplicant's key material is installed into the
 * connection's keys, which are allocated as needed; a WPA, WPA2 or WAPI
 * access point's group key is the BSS's. Returns 0xffffffff without them.
 */
uint32_t keyApi_UpdateKeyMaterial(struct WlanConnection *connPtr, key_MgtMaterial_t *keyMgtData_p)
{
	struct WlanConnectionInfo *info;

	if (connPtr == NULL)
		return 0xffffffff;
	if (connPtr->keyBlocks[1] == NULL)
		connPtr->keyBlocks[1] = wlanKeyBufferAlloc();
	info = wlanConnectionInfo(connPtr);
	if (connPtr->mode == 2) {
		if (info->securityMode.wapi || info->securityMode.wpa || info->securityMode.wpa2) {
			if (info->groupKey == NULL) {
				info->groupKey = wlanKeyBufferAlloc();
				if (info->groupKey == NULL)
					return 0xffffffff;
			}
			connPtr->keyBlocks[0] = info->groupKey;
		}
	} else if (connPtr->keyBlocks[0] == NULL) {
		connPtr->keyBlocks[0] = wlanKeyBufferAlloc();
	}
	if (connPtr->keyBlocks[1] == NULL || connPtr->keyBlocks[0] == NULL)
		return 0xffffffff;
	return keyApi_UpdateKeys(connPtr, &connPtr->keyBlocks[1]->key, &connPtr->keyBlocks[0]->key, keyMgtData_p);
}

/* The connection's pairwise or group key as key material; 0 without one. */
uint16_t keyApi_GetKeyMaterial(struct WlanConnection *connPtr, key_MgtMaterial_t *keyMgtData_p,
			       BOOLEAN pairwise)
{
	cipher_key_t *pwkey = NULL;
	cipher_key_t *gwkey = NULL;

	if (connPtr == NULL)
		return 0;
	if (pairwise) {
		if (connPtr->keyBlocks[1] == NULL)
			return 0;
		pwkey = &connPtr->keyBlocks[1]->key;
	} else {
		if (connPtr->keyBlocks[0] == NULL)
			return 0;
		gwkey = &connPtr->keyBlocks[0]->key;
	}
	return keyApiSta_GetKeyMaterial(pwkey, gwkey, keyMgtData_p, pairwise);
}

/* The key API's RAM hooks, then the station key management's (keyMgmtSta_RomInit()). */
void keyApiSta_RomInit(void)
{
	ramHook_keyApi_PalladiumHook1 = keyApi_PalladiumHook1;
	ramHook_keyApiSta_nullHook = keyApiSta_NullHook;
	ramHook_keyApiSta_isConnStaticWepEnabled = keyApiSta_isConnStaticWepEnabled;
	ramHook_keyApiSta_setConnStaticWepEnabled = keyApiSta_setConnStaticWepEnabled;
	ramHook_keyApiSta_setConnExcludeUnencrypted = keyApiSta_setConnExcludeUnencrypted;
	ramHook_keyApiSta_setConnCurPktTxEnabled = keyApiSta_setConnCurPktTxEnabled;
	ramHook_keyApiSta_setConnDataTrafficEnabled = keyApiSta_setConnDataTrafficEnabled;
	ramHook_keyApiSta_setConnWapiEnabled = keyApiSta_setConnWapiEnabled;
	keyMgmtSta_RomInit();
}

/*
 * An access point's key is installed from its key data: the BSS's group key
 * (from the BSS's group key data) or a station's pairwise key (from the
 * handshake's, which the pairwise key buffer holds). A static WEP BSS uses its
 * WEP key for both. Returns 1 without a key buffer, else 0.
 */
uint32_t keyApi_ApUpdateKeyMaterial(struct WlanConnection *connPtr, int updateGrpKey)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connPtr);
	struct WlanConnectionInfo *info = wlanConnectionInfo(connPtr);
	struct WlanKeyBuffer *keyBuffer;
	KeyData_t pwsKeyData;
	KeyData_t *keyData;
	Cipher_t *cipher;

	connPtr->hostFlags &= ~WLAN_CONN_HOST_FLAG_8;
	connPtr->flags &= ~WLAN_CONN_PRIVACY;
	if (info->securityMode.wepStatic) {
		connPtr->keyBlocks[0] = info->bssKey;
		connPtr->keyBlocks[1] = info->bssKey;
		connPtr->hostFlags |= WLAN_CONN_HOST_FLAG_8;
	} else if (info->securityMode.wpa || info->securityMode.wpa2) {
		connPtr->flags |= WLAN_CONN_PRIVACY;
		if (updateGrpKey == 1) {
			keyData = &info->grpKeyData;
			if (connPtr->keyBlocks[0] == NULL)
				connPtr->keyBlocks[0] = wlanKeyBufferAlloc();
			keyBuffer = connPtr->keyBlocks[0];
		} else {
			keyBuffer = connPtr->keyBlocks[1];
			__aeabi_memcpy4(&pwsKeyData, &keyBuffer->key.ckd.hskData.pwsKeyData, sizeof(pwsKeyData));
			keyData = &pwsKeyData;
		}
		if (keyBuffer == NULL)
			return 1;
		if (updateGrpKey == 1) {
			/* the original sets only the first byte of the address */
			keyBuffer->key.hdr.macAddr[0] = 0;
			cipher = &info->apRsn.mcstCipher;
		} else {
			keyBuffer->key.hdr.macAddr[0] = connPtr->peerAddress[0];
			connPtr->flags |= WLAN_CONN_ENCRYPTED | WLAN_CONN_JOIN_OPEN;
			cipher = &role->keyMgmt.staUcstCipher;
		}
		keyApiSta_SetKeyFromKeyData(updateGrpKey, &keyBuffer->key, cipher, keyData);
	}
	hostCmdConnectionChanged(connPtr);
	return 0;
}

/*
 * An access point's group key is dropped when the BSS's security no longer
 * uses it: a TKIP or AES key without WPA and WPA2, a WAPI key without WAPI.
 */
void keyApi_ApReleaseGroupKey(struct WlanConnectionInfo *info)
{
	struct WlanKeyBuffer *key = info->groupKey;

	if (key == NULL)
		return;
	switch (key->key.hdr.keyType) {
	case KEY_TYPE_TKIP:
	case KEY_TYPE_AES:
		if (!info->securityMode.wpa && !info->securityMode.wpa2) {
			wlanKeyBufferFree(key);
			info->groupKey = NULL;
		}
		break;
	case KEY_TYPE_WAPI:
		if (!info->securityMode.wapi)
			keyApi_FreeKey(&info->groupKey);
		break;
	}
}
