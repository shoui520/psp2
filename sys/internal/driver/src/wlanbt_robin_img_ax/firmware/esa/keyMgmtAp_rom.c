/*
 * The access point's key management, the part the Mask ROM build keeps
 * patchable: the group key, the EAPOL-Key frames the access point sends, the
 * key data of message 3 and the group key message, the PTK, and the WPA/RSN
 * element (Marvell's keyMgmtAp_rom.c, from 0x2dd10).
 *
 * Written from Marvell's source (Copyright (C) 2014-2017, Marvell International
 * Ltd., GPL-2.0) as this firmware has it.
 */
#include "robin/buffer.h"
#include "robin/esa.h"
#include "robin/keyMgmtAp.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"

/* this file's part of the ROM-patch table, 0x55c20.. */
ROBIN_PATCH_SLOT(PopulateKeyMsg_hook);
ROBIN_PATCH_SLOT(prepareKDE_hook);
ROBIN_PATCH_SLOT(Encrypt_keyData_hook);
ROBIN_PATCH_SLOT(keyMgmtAp_FormatWPARSN_IE_internal_hook);

/* The lowest bit of val that is set (0 if none). */
uint32_t util_FindLowestBitSet(uint32_t val)
{
	uint32_t bitmap = 1;

	while (bitmap && (!(bitmap & val)))
		bitmap <<= 1;

	return bitmap;
}

uint8_t convertMrvlAuthToIEEEAuth(uint32_t mrvlauth)
{
	uint8_t auth;

	switch (mrvlauth) {
	case UAP_HOSTCMD_KEYMGMT_EAP:
		auth = IEEEtypes_RSN_AUTH_KEY_SUITE_8021X;
		break;
	case UAP_HOSTCMD_KEYMGMT_PSK:
		auth = IEEEtypes_RSN_AUTH_KEY_SUITE_PSK;
		break;
	default:
		auth = IEEEtypes_RSN_AUTH_KEY_SUITE_RSVD;
		break;
	}
	return auth;
}

uint8_t convertMrvlCipherToIEEECipher(uint32_t mrvlcipher)
{
	uint8_t cipher;

	switch (mrvlcipher) {
	case UAP_HOSTCMD_CIPHER_WEP40:
		cipher = IEEEtypes_RSN_CIPHER_SUITE_WEP40;
		break;
	case UAP_HOSTCMD_CIPHER_WEP104:
		cipher = IEEEtypes_RSN_CIPHER_SUITE_WEP104;
		break;
	case UAP_HOSTCMD_CIPHER_TKIP:
		cipher = IEEEtypes_RSN_CIPHER_SUITE_TKIP;
		break;
	case UAP_HOSTCMD_CIPHER_CCMP:
		cipher = IEEEtypes_RSN_CIPHER_SUITE_CCMP;
		break;
	default:
		cipher = IEEEtypes_RSN_CIPHER_SUITE_NONE;
		break;
	}
	return cipher;
}

/* A new group key and nonce, from random numbers expanded with the PRF. */
void GenerateGTK_internal(KeyData_t *grpKeyData, uint8_t *nonce, const uint8_t *StaMacAddr)
{
	uint8_t inp_data[NONCE_SIZE + sizeof(IEEEtypes_MacAddr_t)];
	uint8_t prefix[] = "Group key expansion";
	uint8_t GTK[32];		/* group transient key */
	uint8_t grpMasterKey[32];

	if (!grpKeyData || !nonce)
		return;

	__aeabi_memcpy(inp_data, StaMacAddr, sizeof(IEEEtypes_MacAddr_t));
	supplicantGenerateRand(nonce, NONCE_SIZE);
	__aeabi_memcpy(inp_data + sizeof(IEEEtypes_MacAddr_t), nonce, NONCE_SIZE);
	supplicantGenerateRand(grpMasterKey, sizeof(grpMasterKey));
	Mrvl_PRF(grpMasterKey, sizeof(grpMasterKey), prefix, strlen((char *)prefix), inp_data,
		 sizeof(inp_data), GTK, sizeof(GTK));
	__aeabi_memcpy(grpKeyData->Key, GTK, TK_SIZE);
	__aeabi_memcpy(grpKeyData->TxMICKey, GTK + TK_SIZE, 8);
	__aeabi_memcpy(grpKeyData->RxMICKey, GTK + TK_SIZE + 8, 8);
}

/*
 * Populates an EAPOL-Key frame from the cipher, the nonce, the replay counter
 * and the type, which says whether it is secure and part of the WPA2 or the
 * WPA handshake. The access point sends message 1, message 3 and the group
 * key message with it.
 */
void PopulateKeyMsg(EAPOL_KeyMsg_Tx_t *tx_eapol_ptr, Cipher_t *Cipher, uint32_t Type,
		    uint32_t replay_cnt[2], uint8_t *Nonce)
{
	key_info_t *pKeyInfo;

	if (PopulateKeyMsg_hook(tx_eapol_ptr, Cipher, Type, replay_cnt, Nonce))
		return;
	if (!tx_eapol_ptr || !Cipher)
		return;

	pKeyInfo = &tx_eapol_ptr->keyMsg.key_info;

	if (Cipher->tkip) {
		/* TKIP unicast */
		tx_eapol_ptr->keyMsg.key_length = htons(TK_SIZE + TK_SIZE);
	} else if (Cipher->ccmp) {
		/* CCMP unicast */
		tx_eapol_ptr->keyMsg.key_length = htons(TK_SIZE);
	}

	pKeyInfo->KeyAck = 1;

	if (Type & PAIRWISE_KEY_MSG) {
		pKeyInfo->KeyType = 1;
		if (Type & SECURE_HANDSHAKE_FLAG) {
			pKeyInfo->KeyMIC = 1;
			pKeyInfo->Install = 1;
			pKeyInfo->EncryptedKeyData = pKeyInfo->Secure =
				(Type & WPA2_HANDSHAKE) ? 1 : 0;
		}
	} else {
		pKeyInfo->Secure = 1;
		pKeyInfo->KeyMIC = 1;
		pKeyInfo->EncryptedKeyData = (Type & WPA2_HANDSHAKE) ? 1 : 0;
	}

	tx_eapol_ptr->keyMsg.replay_cnt[0] = htonl(replay_cnt[0]);
	tx_eapol_ptr->keyMsg.replay_cnt[1] = htonl(replay_cnt[1]);
	__aeabi_memcpy(tx_eapol_ptr->keyMsg.key_nonce, Nonce, NONCE_SIZE);
}

/* The key data of the group key message (WPA2): the group key in a GTK KDE, padded to 8 bytes. */
void prepareKDE(EAPOL_KeyMsg_Tx_t *tx_eapol_ptr, KeyData_t *grKey, Cipher_t *cipher)
{
	KDE_t *pKeyDataWPA2;
	GTK_KDE_t *pGTK_IE;
	uint8_t RsnIE_len;
	uint8_t PadLen;
	uint8_t *buf_p;

	if (prepareKDE_hook(tx_eapol_ptr, grKey, cipher))
		return;
	if (!tx_eapol_ptr || !grKey || !cipher)
		return;

	RsnIE_len = tx_eapol_ptr->keyMsg.key_material_len;
	buf_p = tx_eapol_ptr->keyMsg.key_data + RsnIE_len;

	pKeyDataWPA2 = (KDE_t *)buf_p;
	pKeyDataWPA2->type = 0xdd;
	pKeyDataWPA2->length = KEYDATA_SIZE;
	pKeyDataWPA2->OUI[0] = kde_oui[0];
	pKeyDataWPA2->OUI[1] = kde_oui[1];
	pKeyDataWPA2->OUI[2] = kde_oui[2];
	pKeyDataWPA2->dataType = KDE_DATA_TYPE_GTK;
	buf_p = buf_p + KDE_SIZE;

	pGTK_IE = (GTK_KDE_t *)buf_p;
	pGTK_IE->KeyID = 1;
	buf_p = buf_p + GTK_IE_SIZE;

	/* copy over the GTK */
	__aeabi_memcpy(buf_p, grKey->Key, TK_SIZE);
	buf_p = buf_p + TK_SIZE;

	if (cipher->tkip) {
		pKeyDataWPA2->length += 8 + 8;
		__aeabi_memcpy(buf_p, grKey->TxMICKey, 8);
		buf_p = buf_p + 8;
		__aeabi_memcpy(buf_p, grKey->RxMICKey, 8);
		buf_p = buf_p + 8;
	}

	tx_eapol_ptr->keyMsg.key_material_len += pKeyDataWPA2->length + KDE_IE_SIZE;

	PadLen = (8 - tx_eapol_ptr->keyMsg.key_material_len) & 7;
	if (PadLen) {
		*(buf_p) = 0xdd;
		__aeabi_memclr(buf_p + 1, PadLen - 1);
		tx_eapol_ptr->keyMsg.key_material_len += PadLen;
	}
}

/* Encrypts the key data in place with the key encryption key: AES key wrap for CCMP, RC4 for TKIP. */
BOOLEAN Encrypt_keyData(EAPOL_KeyMsg_Tx_t *tx_eapol_ptr, uint8_t *EAPOL_Encr_Key,
			Cipher_t *cipher)
{
	BOOLEAN result;
	uint8_t key[20];
	OsaBuffer *pBufDesc;
	uint8_t *cipherText;

	if (Encrypt_keyData_hook(tx_eapol_ptr, EAPOL_Encr_Key, cipher, &result))
		return result;
	if (!tx_eapol_ptr || !EAPOL_Encr_Key || !cipher)
		return 0;

	if (cipher->ccmp) {
		/* Pairwise is CCMP */
		pBufDesc = osa_BufferAlloc(ramHook_encrPoolConfig, 400, 0xffffffff);
		cipherText = (uint8_t *)pBufDesc + pBufDesc->dataOffset;
		__aeabi_memcpy(key, EAPOL_Encr_Key, 16);

		/* AES-only mode of the AEU, to perform the AES wrap */
		MRVL_AesWrap(key, 2, tx_eapol_ptr->keyMsg.key_material_len / 8,
			     tx_eapol_ptr->keyMsg.key_data, NULL, cipherText);

		tx_eapol_ptr->keyMsg.key_material_len += 8;
		__aeabi_memcpy(tx_eapol_ptr->keyMsg.key_data, cipherText,
			       tx_eapol_ptr->keyMsg.key_material_len);
		osa_BufferFree(pBufDesc);
	} else if (cipher->tkip) {
		/* Pairwise is TKIP */
		supplicantGenerateRand(tx_eapol_ptr->keyMsg.EAPOL_key_IV, 16);
		RC4_Encrypt(EAPOL_Encr_Key, tx_eapol_ptr->keyMsg.EAPOL_key_IV, 16,
			    tx_eapol_ptr->keyMsg.key_data, tx_eapol_ptr->keyMsg.key_material_len,
			    256);
	} else {
		return 0;
	}

	return 1;
}

/* The station's PTK: the supplicant's derivation, then the access point's Rx and Tx MIC keys swap. */
void KeyMgmtAp_DerivePTK(uint8_t *pPMK, uint8_t *da, uint8_t *sa, uint8_t *ANonce,
			 uint8_t *SNonce, uint8_t *EAPOL_MIC_Key, uint8_t *EAPOL_Encr_Key,
			 KeyData_t *newPWKey, BOOLEAN use_kdf)
{
	uint8_t tmp[8];

	/* call the station's PTK generation first */
	KeyMgmtSta_DeriveKeys(pPMK, da, sa, ANonce, SNonce, EAPOL_MIC_Key, EAPOL_Encr_Key,
			      newPWKey, use_kdf);

	/* We need to swap Rx/Tx Keys for the access point */
	__aeabi_memcpy(tmp, newPWKey->RxMICKey, 8);
	__aeabi_memcpy(newPWKey->RxMICKey, newPWKey->TxMICKey, 8);
	__aeabi_memcpy(newPWKey->TxMICKey, tmp, 8);
}

/* The WPA or RSN element as the key data (its length is the element's plus two). */
BOOLEAN KeyData_CopyWPAWP2(EAPOL_KeyMsg_Tx_t *pTxEAPOL, void *pIe)
{
	IEEEtypes_InfoElementHdr_t *pElement = (IEEEtypes_InfoElementHdr_t *)pIe;

	if (!pIe)
		return 0;

	pTxEAPOL->keyMsg.key_material_len = pElement->Len + sizeof(IEEEtypes_InfoElementHdr_t);
	__aeabi_memcpy(pTxEAPOL->keyMsg.key_data, pIe, pTxEAPOL->keyMsg.key_material_len);

	return 1;
}

BOOLEAN KeyData_UpdateKeyMaterial(EAPOL_KeyMsg_Tx_t *pTxEAPOL, SecurityMode_t *pSecType,
				  void *pWPA, void *pWPA2)
{
	if (pSecType->wpa || pSecType->wpaNone) {
		if (KeyData_CopyWPAWP2(pTxEAPOL, pWPA) == 0)
			return 0;
	} else if (pSecType->wpa2) {
		if (KeyData_CopyWPAWP2(pTxEAPOL, pWPA2) == 0)
			return 0;
	}

	return 1;
}

/* The group key as the key data of a WPA message (the Tx MIC key first). */
void KeyData_AddGTK(EAPOL_KeyMsg_Tx_t *pTxEAPOL, KeyData_t *grKey, Cipher_t *cipher)
{
	uint8_t *buf_p = pTxEAPOL->keyMsg.key_data;

	__aeabi_memcpy(buf_p, grKey, TK_SIZE);
	buf_p = buf_p + TK_SIZE;

	pTxEAPOL->keyMsg.key_material_len += TK_SIZE;

	if (cipher->tkip) {
		__aeabi_memcpy(buf_p, grKey->TxMICKey, 8);
		buf_p = buf_p + 8;
		__aeabi_memcpy(buf_p, grKey->RxMICKey, 8);
		pTxEAPOL->keyMsg.key_material_len += 8 + 8;
	}
}

/* The group key's index and send counter, and the key as the key data (nothing for other security). */
void KeyData_AddKey(EAPOL_KeyMsg_Tx_t *pTxEAPOL, SecurityMode_t *pSecType, KeyData_t *grKey,
		    Cipher_t *cipher)
{
	pTxEAPOL->keyMsg.key_info.KeyIndex = grKey->KeyIndex;

	pTxEAPOL->keyMsg.key_RSC[0] = grKey->TxIV16 & 0x00ff;
	pTxEAPOL->keyMsg.key_RSC[1] = (grKey->TxIV16 >> 8) & 0x00ff;
	__aeabi_memcpy(pTxEAPOL->keyMsg.key_RSC + 2, &grKey->TxIV32, 4);

	if (pSecType->wpa || pSecType->wpaNone)
		KeyData_AddGTK(pTxEAPOL, grKey, cipher);
	else if (pSecType->wpa2)
		prepareKDE(pTxEAPOL, grKey, cipher);
}

void ROM_InitGTK(KeyData_t *grpKeyData, uint8_t *nonce, const uint8_t *StaMacAddr)
{
	grpKeyData->KeyIndex = 1;
	grpKeyData->TxIV16 = 1;
	grpKeyData->TxIV32 = 0;

	GenerateGTK_internal(grpKeyData, nonce, StaMacAddr);
}

/*
 * Writes the WPA or RSN element of the access point at pIe: the group cipher,
 * the pairwise ciphers (cipherCnt of the bits of pCipher) and the AKM suites
 * (authKeyCnt of the bits of authKey), and returns its length. The counts
 * are written as Marvell's source does, which takes their bits 16-23 for the
 * second byte.
 */
uint16_t keyMgmtAp_FormatWPARSN_IE_internal(IEEEtypes_InfoElementHdr_t *pIe, uint8_t isRsn,
					    Cipher_t *pCipher, uint32_t cipherCnt,
					    Cipher_t *pMcastCipher, uint32_t authKey,
					    uint32_t authKeyCnt)
{
	uint32_t i;
	uint8_t *pBuf = NULL;
	IEEEtypes_RSNElement_t *pRsn = (IEEEtypes_RSNElement_t *)pIe;
	IEEEtypes_WPAElement_t *pWpa = (IEEEtypes_WPAElement_t *)pIe;
	uint32_t bitPos = 0;
	uint32_t authKeyBitmap = authKey;
	uint32_t ucastBitmap = *((uint8_t *)pCipher);
	uint32_t mcastBitmap = *((uint8_t *)pMcastCipher);
	uint8_t oui[3];
	uint16_t ieLength = 0;

	if (keyMgmtAp_FormatWPARSN_IE_internal_hook(pIe, isRsn, pCipher, cipherCnt, pMcastCipher,
						    authKey, authKeyCnt, &ieLength))
		return ieLength;

	pIe->ElementId = (isRsn == 1) ? ELEM_ID_RSN : ELEM_ID_VENDOR_SPECIFIC;

	if (isRsn) {
		__aeabi_memcpy(oui, kde_oui, sizeof(oui));
		pBuf = (uint8_t *)&pRsn->Ver;
	} else {
		__aeabi_memcpy(oui, wpa_oui01, sizeof(oui));
		__aeabi_memcpy(pWpa->OuiType, wpa_oui01, sizeof(wpa_oui01));
		pBuf = (uint8_t *)&pWpa->Ver;
	}

	pBuf[0] = 0x1;
	pBuf[1] = 0x0;
	pBuf += 2;

	/* the group cipher */
	__aeabi_memcpy(pBuf, oui, sizeof(oui));

	if (mcastBitmap)
		bitPos = util_FindLowestBitSet(mcastBitmap);

	pBuf[3] = convertMrvlCipherToIEEECipher(bitPos);
	pBuf += 4;

	pBuf[0] = (cipherCnt >> 0) & 0xff;
	pBuf[1] = (cipherCnt >> 16) & 0xff;
	pBuf += 2;

	for (i = 0; i < cipherCnt; i++) {
		pBuf[0] = oui[0];
		pBuf[1] = oui[1];
		pBuf[2] = oui[2];

		bitPos = util_FindLowestBitSet(ucastBitmap);

		pBuf[3] = convertMrvlCipherToIEEECipher(bitPos);

		ucastBitmap &= ~bitPos;

		pBuf += 4;
	}

	pBuf[0] = (authKeyCnt >> 0) & 0xff;
	pBuf[1] = (authKeyCnt >> 16) & 0xff;
	pBuf += 2;

	for (i = 0; i < authKeyCnt; i++) {
		pBuf[0] = oui[0];
		pBuf[1] = oui[1];
		pBuf[2] = oui[2];

		bitPos = util_FindLowestBitSet(authKeyBitmap);

		pBuf[3] = convertMrvlAuthToIEEEAuth(bitPos);

		authKeyBitmap &= ~bitPos;
		pBuf += 4;
	}

	if (isRsn) {
		pBuf[0] = 0x0;
		pBuf[1] = 0x0;
		pBuf += 2;
	}

	ieLength = (uintptr_t)pBuf - (uintptr_t)pIe;
	pIe->Len = ieLength - sizeof(IEEEtypes_InfoElementHdr_t);
	return ieLength;
}
