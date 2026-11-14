/*
 * The access point's key management (Marvell's keyMgmtAp.c, from 0x1f3c8):
 * the 4-way and the group key handshakes with each station, the group rekey,
 * and the TKIP countermeasures. One handshake's state is the connection's
 * apKeyMgmtInfoSta_t; this firmware counts the states by the message it waits
 * for, and a failed message is retried when the handshake timer runs out.
 *
 * Written from Marvell's source (Copyright (C) 2014-2017, Marvell International
 * Ltd., GPL-2.0) as this firmware has it.
 */
#include "robin/assocAp.h"
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/icverr.h"
#include "robin/ie.h"
#include "robin/keyMgmtAp.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/wlan.h"
#include "robin/wlantask.h"

#define EAPOL_PROTOCOL_V1	1
#define MRVDRV_TIMER_60S	60000000	/* microseconds */

/* Stops the station's handshake timer (the group rekey timer of the access point if groupRekey is 1). */
int KeyMgmtStopHskTimer(struct WlanConnection *connPtr, int groupRekey)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connPtr);
	struct WlanConnectionInfo *info = wlanConnectionInfo(connPtr);
	uint32_t timer;

	if (groupRekey == 1)
		timer = info->grpRekeyTimer;
	else
		timer = role->keyMgmt.HskTimer;
	return osaCancelTimer(timer);
}

/*
 * The handshake timer ran out: once the retries are used up (the pairwise
 * handshake's, the group handshake's) the station is deauthenticated, else
 * the message is sent again.
 */
void KeyMgmtHskTimeout(uint32_t timer, struct WlanConnection *connPtr)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connPtr);
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connPtr);
	apKeyMgmtInfoSta_t *pKeyMgmtInfo = &role->keyMgmt;
	unsigned int reason;

	pKeyMgmtInfo->numHskTries++;
	if (pKeyMgmtInfo->keyMgmtState == WAITING_4_MSG2 ||
	    pKeyMgmtInfo->keyMgmtState == WAITING_4_MSG4) {
		if (pKeyMgmtInfo->numHskTries > info->apRsn.MaxPwsHskRetries) {
			reason = IEEEtypes_REASON_4WAY_HANDSHK_TIMEOUT;
			goto failed;
		}
	} else if (pKeyMgmtInfo->keyMgmtState == WAITING_4_GRPMSG2 ||
		   pKeyMgmtInfo->keyMgmtState == WAITING_4_GRP_REKEY_MSG2) {
		if (pKeyMgmtInfo->numHskTries > info->apRsn.MaxGrpHskRetries) {
			reason = IEEEtypes_REASON_GRP_KEY_UPD_TIMEOUT;
failed:
			macMgmtInspectBuffer(connPtr, connPtr->peerAddress, reason);
			pKeyMgmtInfo->keyMgmtState = HSK_END;
			return;
		}
	}
	GenerateApEapolMsg(connPtr, pKeyMgmtInfo->keyMgmtState);
}

/* Starts the station's handshake timer (milliseconds). */
void KeyMgmtStartHskTimer(struct WlanConnection *connPtr, uint32_t timeoutInMs)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connPtr);

	osaCreateTimer((OsaTimerCallback)KeyMgmtHskTimeout, (uint32_t)connPtr, 1000 * timeoutInMs,
		       &role->keyMgmt.HskTimer, 0);
}

/* The buffer of an EAPOL-Key frame to the station, its header and descriptor version filled. */
OsaBuffer *PrepDefaultEapolMsg(struct WlanConnection *connPtr, EAPOL_KeyMsg_Tx_t **pTxEapolPtr)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connPtr);
	struct WlanConnectionInfo *info = wlanConnectionInfo(connPtr);
	EAPOL_KeyMsg_Tx_t *tx_eapol_ptr;
	OsaBuffer *pmbuf;
	BOOLEAN nonTkip;

	pmbuf = GetTxEAPOLBuffer(connPtr, &tx_eapol_ptr);
	if (pmbuf != NULL) {
		pmbuf->context = connPtr->device;
		((uint8_t *)pmbuf)[pmbuf->dataOffset + 12] = 0;
		__aeabi_memclr(tx_eapol_ptr, sizeof(EAPOL_KeyMsg_Tx_t));
		formEAPOLEthHdr(tx_eapol_ptr, connPtr->peerAddress, (uint8_t *)connPtr->interfaceAddress);
		nonTkip = 1;
		if (!role->keyMgmt.staUcstCipher.ccmp)
			nonTkip = info->apRsn.mcstCipher.ccmp != 0;
		SetEAPOLKeyDescTypeVersion(tx_eapol_ptr, role->keyMgmt.staSecType.wpa2, 0, nonTkip);
		*pTxEapolPtr = tx_eapol_ptr;
	}
	return pmbuf;
}

/* The group key message of the WPA handshake or of a rekey; 1 if no buffer is free. */
int GenerateGrpMsg1(struct WlanConnection *connPtr)
{
	EAPOL_KeyMsg_Tx_t *tx_eapol_ptr;
	struct WlanConnectionRole *role;
	struct WlanConnectionInfo *info;
	apKeyMgmtInfoSta_t *pKeyMgmtInfo;
	uint32_t replay_cnt[2];
	OsaBuffer *pmbuf;
	uint32_t frameLen;

	pmbuf = PrepDefaultEapolMsg(connPtr, &tx_eapol_ptr);
	if (pmbuf == NULL)
		return 1;

	role = wlanConnectionInfoSlot(connPtr);
	info = wlanConnectionInfo(connPtr);
	pKeyMgmtInfo = &role->keyMgmt;

	incrementReplayCounter(pKeyMgmtInfo);

	replay_cnt[0] = pKeyMgmtInfo->counterHi;
	replay_cnt[1] = pKeyMgmtInfo->counterLo;

	PopulateKeyMsg(tx_eapol_ptr, &info->apRsn.mcstCipher,
		       SECURE_HANDSHAKE_FLAG | (pKeyMgmtInfo->staSecType.wpa2 ? WPA2_HANDSHAKE : 0),
		       replay_cnt, info->GNonce);

	KeyData_AddKey(tx_eapol_ptr, &pKeyMgmtInfo->staSecType, &info->grpKeyData,
		       &info->apRsn.mcstCipher);
	Encrypt_keyData(tx_eapol_ptr, pKeyMgmtInfo->EAPOL_Encr_Key, &pKeyMgmtInfo->staUcstCipher);

	frameLen = KeyMgmtSta_PopulateEAPOLLengthMic(tx_eapol_ptr, pKeyMgmtInfo->EAPOL_MIC_Key,
						     EAPOL_PROTOCOL_V1,
						     tx_eapol_ptr->keyMsg.key_info.KeyDescriptorVersion);
	wlanSetEventPacketLength(pmbuf, frameLen);
	return 0;
}

/* Message 3 of the 4-way handshake: the access point's WPA or RSN element, and with WPA2 the group key. */
int GeneratePWKMsg3(struct WlanConnection *connPtr)
{
	EAPOL_KeyMsg_Tx_t *tx_eapol_ptr;
	struct WlanConnectionRole *role;
	struct WlanConnectionInfo *info;
	apKeyMgmtInfoSta_t *pKeyMgmtInfo;
	eapolHskData_t *pHskData;
	uint32_t replay_cnt[2];
	OsaBuffer *pmbuf;
	WlanWirelessFrame *beacon;
	uint8_t *pWpaIe = NULL;
	uint8_t *pRsnIe = NULL;
	int ok;
	uint32_t frameLen;

	pmbuf = PrepDefaultEapolMsg(connPtr, &tx_eapol_ptr);
	if (pmbuf == NULL)
		return 1;

	role = wlanConnectionInfoSlot(connPtr);
	info = wlanConnectionInfo(connPtr);
	pKeyMgmtInfo = &role->keyMgmt;
	pHskData = connPtr->hskData;

	incrementReplayCounter(pKeyMgmtInfo);

	replay_cnt[0] = pKeyMgmtInfo->counterHi;
	replay_cnt[1] = pKeyMgmtInfo->counterLo;

	PopulateKeyMsg(tx_eapol_ptr, &pKeyMgmtInfo->staUcstCipher,
		       (PAIRWISE_KEY_MSG | SECURE_HANDSHAKE_FLAG) |
		       (pKeyMgmtInfo->staSecType.wpa2 ? WPA2_HANDSHAKE : 0),
		       replay_cnt, pHskData->ANonce);

	/* the element of the access point's own beacon */
	beacon = (WlanWirelessFrame *)((uint8_t *)connPtr->beaconBuffer + connPtr->beaconBuffer->dataOffset);
	if (pKeyMgmtInfo->staSecType.wpa)
		pWpaIe = wlanFindIe(beacon, ELEM_ID_VENDOR_SPECIFIC, WLAN_FRAME_BEACON, wpa_oui01,
				    sizeof(wpa_oui01));
	else if (pKeyMgmtInfo->staSecType.wpa2)
		pRsnIe = wlanFindIe(beacon, ELEM_ID_RSN, WLAN_FRAME_BEACON, NULL, 0);
	ok = KeyData_UpdateKeyMaterial(tx_eapol_ptr, &pKeyMgmtInfo->staSecType, pWpaIe, pRsnIe);
	if (!ok) {
		/* WPA/WPA2 is enabled but there is no corresponding element */
		osa_BufferFree(pmbuf);
		return 1;
	}

	if (pKeyMgmtInfo->staSecType.wpa2) {
		prepareKDE(tx_eapol_ptr, &info->grpKeyData, &info->apRsn.mcstCipher);
		if (!Encrypt_keyData(tx_eapol_ptr, pKeyMgmtInfo->EAPOL_Encr_Key,
				     &pKeyMgmtInfo->staUcstCipher)) {
			osa_BufferFree(pmbuf);
			return 1;
		}
	}

	frameLen = KeyMgmtSta_PopulateEAPOLLengthMic(tx_eapol_ptr, pKeyMgmtInfo->EAPOL_MIC_Key,
						     EAPOL_PROTOCOL_V1,
						     tx_eapol_ptr->keyMsg.key_info.KeyDescriptorVersion);
	wlanSetEventPacketLength(pmbuf, frameLen);
	return 0;
}

/* Message 1 of the 4-way handshake: the access point's nonce, no key data. */
int GeneratePWKMsg1(struct WlanConnection *connPtr)
{
	EAPOL_KeyMsg_Tx_t *tx_eapol_ptr;
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connPtr);
	apKeyMgmtInfoSta_t *pKeyMgmtInfo;
	eapolHskData_t *pHskData;
	uint32_t replay_cnt[2];
	OsaBuffer *pmbuf;
	uint16_t frameLen;

	pmbuf = PrepDefaultEapolMsg(connPtr, &tx_eapol_ptr);
	if (pmbuf == NULL)
		return 1;

	pKeyMgmtInfo = &role->keyMgmt;
	pHskData = connPtr->hskData;

	incrementReplayCounter(pKeyMgmtInfo);
	replay_cnt[0] = pKeyMgmtInfo->counterHi;
	replay_cnt[1] = pKeyMgmtInfo->counterLo;

	supplicantGenerateRand(pHskData->ANonce, NONCE_SIZE);

	PopulateKeyMsg(tx_eapol_ptr, &pKeyMgmtInfo->staUcstCipher, PAIRWISE_KEY_MSG, replay_cnt,
		       pHskData->ANonce);

	/* the key material length is 0 here; 95 is the frame after the 802.1X header, without key data */
	frameLen = tx_eapol_ptr->keyMsg.key_material_len + 95;
	tx_eapol_ptr->keyMsg.hdr_8021x.protocol_ver = EAPOL_PROTOCOL_V1;
	tx_eapol_ptr->keyMsg.hdr_8021x.pckt_type = IEEE_8021X_PACKET_TYPE_EAPOL_KEY;
	tx_eapol_ptr->keyMsg.hdr_8021x.pckt_body_len = htons(frameLen);
	wlanSetEventPacketLength(pmbuf, frameLen);
	return 0;
}

/*
 * Sends the message the handshake state asks for (the state is the message
 * waited for after it) and starts the handshake timer, whether or not the
 * message could be sent: a message that fails is sent again when the timer
 * runs out. Returns 0 if it was sent.
 */
int GenerateApEapolMsg(struct WlanConnection *connPtr, int msgState)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connPtr);
	apRsnConfig_t *pRsnConfig = &wlanConnectionInfo(connPtr)->apRsn;
	int status;
	uint32_t timeout;

	role->keyMgmt.keyMgmtState = msgState;
	switch (msgState) {
	case WAITING_4_MSG2:
		status = GeneratePWKMsg1(connPtr);
		timeout = pRsnConfig->PwsHskTimeOut;
		break;
	case WAITING_4_MSG4:
		status = GeneratePWKMsg3(connPtr);
		timeout = pRsnConfig->PwsHskTimeOut;
		break;
	case WAITING_4_GRPMSG2:
	case WAITING_4_GRP_REKEY_MSG2:
		status = GenerateGrpMsg1(connPtr);
		timeout = pRsnConfig->GrpHskTimeOut;
		break;
	default:
		return 1;
	}
	KeyMgmtStartHskTimer(connPtr, timeout);
	return status;
}

/* Sends the new group key to every station of the access point's BSS whose handshake is done. */
void KeyMgmtSendGrpKeyMsgToAllSta(struct WlanConnection *connPtr)
{
	uint32_t posture = sysIrqDisable();

	while (connPtr != NULL) {
		struct WlanConnectionRole *role = wlanConnectionInfoSlot(connPtr);

		if ((connPtr->flags2 & WLAN_CONN2_ASSOCIATED) && role->keyMgmt.keyMgmtState == HSK_END)
			GenerateApEapolMsg(connPtr, WAITING_4_GRP_REKEY_MSG2);
		connPtr = wlanConnectionNextByBss(connPtr, connPtr->bssType, connPtr->bssNumber);
	}
	sysIrqRestore(posture);
}

/* A new group key, given to the MAC. */
void ReInitGTK(struct WlanConnection *connPtr)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connPtr);

	ROM_InitGTK(&info->grpKeyData, info->GNonce, connPtr->interfaceAddress);
	keyApi_ApUpdateKeyMaterial(connPtr, 1);
}

/* The group rekey timer's message (the connection of the access point): rekey and tell the stations. */
void uapGroupRekey(uintptr_t message)
{
	struct WlanConnection *connPtr = (struct WlanConnection *)message;

	ReInitGTK(connPtr);
	KeyMgmtSendGrpKeyMsgToAllSta(connPtr);
}

/*
 * The group rekey timer ran out. The rekey time in seconds may not fit the
 * timer's 32 bits of microseconds, so the timer runs 2^32 us a time and
 * grpRekeyCntRemaining counts those; at zero the rekey is queued for the
 * task that sends it (retried a second later if its queue is full).
 */
void KeyMgmtGrpRekeyCountUpdate(uint32_t timer, struct WlanConnection *connPtr)
{
	struct WlanConnectionInfo *info;
	uint32_t delay = 0xffffffff;

	if (connPtr == NULL)
		return;

	info = wlanConnectionInfo(connPtr);
	if (info->grpRekeyCntRemaining != 0) {
		info->grpRekeyCntRemaining--;
	} else {
		delay = 1000000;
		if (osaMsgQueueSend(uapGroupRekeyQueue, 4, (uintptr_t)connPtr, 0) == 0) {
			macTxPostEvent(MAC_TX_EVENT_REKEY);
			info->grpRekeyCntRemaining = romMul32x32(info->apRsn.GrpReKeyTime, 1000000) >> 32;
			delay = (uint32_t)romMul32x32(info->apRsn.GrpReKeyTime, 1000000);
			if (delay == 0) {
				delay = 0xffffffff;
				info->grpRekeyCntRemaining--;
			}
		}
	}
	osaCreateTimer((OsaTimerCallback)KeyMgmtGrpRekeyCountUpdate, (uint32_t)connPtr, delay,
		       &info->grpRekeyTimer, 0);
}

/* Starts the periodic group rekey, if the access point is set to rekey. */
void KeyMgmtStartGrpRekeyTimer(struct WlanConnection *connPtr)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connPtr);
	uint32_t delay;

	if (info->apRsn.GrpReKeyTime != 0) {
		info->grpRekeyCntRemaining = romMul32x32(info->apRsn.GrpReKeyTime, 1000000) >> 32;
		delay = (uint32_t)romMul32x32(info->apRsn.GrpReKeyTime, 1000000);
		if (delay == 0) {
			delay = 0xffffffff;
			info->grpRekeyCntRemaining--;
		}
		osaCreateTimer((OsaTimerCallback)KeyMgmtGrpRekeyCountUpdate, (uint32_t)connPtr, delay,
			       &info->grpRekeyTimer, 0);
	}
}

/* The access point starts: the group key, the PSK from the pass phrase if it changed, the rekey timer. */
void KeyMgmtInit(struct WlanConnection *connPtr)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connPtr);

	ROM_InitGTK(&info->grpKeyData, info->GNonce, connPtr->interfaceAddress);

	if (info->updatePassPhrase == 1) {
		pmkCacheGeneratePSK(connPtr->networkSsid, connPtr->networkSsidLength,
				    (char *)info->apRsn.PSKPassPhrase, info->apRsn.PSKValue);
		uapSqramPskUpdated(connPtr, connPtr->localAddress, connPtr->networkSsid,
				   connPtr->networkSsidLength);
		info->updatePassPhrase = 0;
	}
	KeyMgmtStartGrpRekeyTimer(connPtr);
}

/*
 * Message 2 of the 4-way handshake: the station's element must be the one it
 * associated with; the PTK is derived from its nonce and the message's MIC
 * checked, then message 3 goes out. Returns 0 if it did.
 */
int ProcessPWKMsg2(OsaBuffer *pmbuf)
{
	struct WlanConnection *connPtr = pmbuf->context;
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connPtr);
	struct WlanConnectionInfo *info = wlanConnectionInfo(connPtr);
	eapolHskData_t *pHskData = connPtr->hskData;
	apKeyMgmtInfoSta_t *pKeyMgmtInfo = &role->keyMgmt;
	EAPOL_KeyMsg_t *rx_eapol_ptr = (EAPOL_KeyMsg_t *)((uint8_t *)pmbuf + pmbuf->dataOffset);
	WlanIePointers iePointers;

	/* compare the element of the association request to the one of the station's message */
	wlanParseIes(rx_eapol_ptr->key_data, rx_eapol_ptr->key_data[1] + sizeof(IEEEtypes_InfoElementHdr_t),
		     &iePointers);
	if (assocSrvAp_checkRsnWpa(connPtr, pKeyMgmtInfo, info->apRsn.wpaUcstCipher,
				   info->apRsn.wpa2UcstCipher, info->apRsn.mcstCipher,
				   info->apRsn.AuthKey, &pKeyMgmtInfo->staSecType,
				   (IEEEtypes_RSNElement_t *)iePointers.pRsn,
				   (IEEEtypes_WPAElement_t *)iePointers.pWpa, 1) == 1) {
		handleFailedHSK(connPtr, IEEEtypes_REASON_IE_4WAY_DIFF);
		return 1;
	}

	KeyMgmtAp_DerivePTK(info->apRsn.PSKValue, connPtr->peerAddress, connPtr->localAddress,
			    pHskData->ANonce, rx_eapol_ptr->key_nonce, pKeyMgmtInfo->EAPOL_MIC_Key,
			    pKeyMgmtInfo->EAPOL_Encr_Key, &pHskData->pwsKeyData, 0);

	if (!IsEAPOL_MICValid(rx_eapol_ptr, pKeyMgmtInfo->EAPOL_MIC_Key))
		return 1;

	osaCancelTimer(pKeyMgmtInfo->HskTimer);
	return GenerateApEapolMsg(connPtr, WAITING_4_MSG4);
}

uint16_t keyMgmtAp_FormatWPARSN_IE(IEEEtypes_InfoElementHdr_t *pIe, uint8_t isRsn,
				   Cipher_t *pCipher, uint32_t cipherCount,
				   Cipher_t *pMcastCipher, uint32_t authKey,
				   uint32_t authKeyCount)
{
	return keyMgmtAp_FormatWPARSN_IE_internal(pIe, isRsn, pCipher, cipherCount, pMcastCipher,
						  authKey, authKeyCount);
}

/*
 * Tells the host the station's handshake is done: the element of the
 * connection (WPA or RSN) as an event (event 0x51), whose data is the element
 * with its two bytes as a four-byte type and length.
 */
void sendEventRsnConnect(struct WlanConnection *connPtr, apKeyMgmtInfoSta_t *pKeyMgmtInfo)
{
	struct WlanHostEventFrame *frame;
	OsaBuffer *buffer;
	struct WlanConnectionInfo *info;
	uint8_t *ie;
	uint8_t *tlv;
	uint16_t length;

	if (wlanTxAllocFrame(&buffer, (WlanTxDescriptor **)&frame, 400, WLAN_HOST_EVENT_RSN_CONNECT) != 0) {
		info = wlanConnectionInfo(connPtr);
		ie = (uint8_t *)frame + 26;
		tlv = (uint8_t *)frame + 24;
		keyMgmtAp_FormatWPARSN_IE((IEEEtypes_InfoElementHdr_t *)ie, pKeyMgmtInfo->staSecType.wpa2,
					  &pKeyMgmtInfo->staUcstCipher, 1, &info->apRsn.mcstCipher,
					  info->apRsn.AuthKey, info->apRsn.AuthKeyCount);
		tlv[0] = ie[0];
		tlv[1] = 0;
		length = ie[1];
		*(uint16_t *)(tlv + 2) = length;
		frame->length = length + 4;
		wlanHostEventSendWithAddress(connPtr, connPtr->peerAddress, buffer);
	}
}

/* Message 4 of the 4-way handshake: the MIC, then the pairwise key for the MAC; with WPA the group key message follows. */
int ProcessPWKMsg4(OsaBuffer *pmbuf)
{
	struct WlanConnection *connPtr = pmbuf->context;
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connPtr);
	apKeyMgmtInfoSta_t *pKeyMgmtInfo = &role->keyMgmt;
	eapolHskData_t *pHskData = connPtr->hskData;
	EAPOL_KeyMsg_t *rx_eapol_ptr = (EAPOL_KeyMsg_t *)((uint8_t *)pmbuf + pmbuf->dataOffset);

	if (!IsEAPOL_MICValid(rx_eapol_ptr, pKeyMgmtInfo->EAPOL_MIC_Key))
		return 1;

	pHskData->pwsKeyData.TxIV16 = 0x0001;
	pHskData->pwsKeyData.TxIV32 = 0;

	if (keyApi_ApUpdateKeyMaterial(connPtr, 0) != 0) {
		handleFailedHSK(connPtr, IEEEtypes_REASON_UNSPEC);
		return 1;
	}

	osaCancelTimer(pKeyMgmtInfo->HskTimer);
	if (!pKeyMgmtInfo->staSecType.wpa2)
		return GenerateApEapolMsg(connPtr, WAITING_4_GRPMSG2);

	sendEventRsnConnect(connPtr, pKeyMgmtInfo);
	pKeyMgmtInfo->keyMgmtState = HSK_END;
	return 0;
}

/* Message 2 of the group key handshake: the station has the group key. */
int ProcessGrpMsg2(OsaBuffer *pmbuf)
{
	struct WlanConnection *connPtr = pmbuf->context;
	EAPOL_KeyMsg_t *rx_eapol_ptr = (EAPOL_KeyMsg_t *)((uint8_t *)pmbuf + pmbuf->dataOffset);
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connPtr);
	apKeyMgmtInfoSta_t *pKeyMgmtInfo = &role->keyMgmt;

	if (IsEAPOL_MICValid(rx_eapol_ptr, pKeyMgmtInfo->EAPOL_MIC_Key)) {
		osaCancelTimer(pKeyMgmtInfo->HskTimer);
		if (pKeyMgmtInfo->keyMgmtState == WAITING_4_GRPMSG2)
			sendEventRsnConnect(connPtr, pKeyMgmtInfo);
		pKeyMgmtInfo->keyMgmtState = HSK_END;
		return 0;
	}
	handleFailedHSK(connPtr, IEEEtypes_REASON_IE_4WAY_DIFF);
	return 1;
}

/* The access point's own authenticator is to handle the connection: WPA or RSN with a pass phrase, none of the host's. */
int wlanInterfaceNeedsSecurity(const void *context)
{
	const struct WlanConnection *connPtr = context;
	struct WlanConnectionInfo *info = wlanConnectionInfo((struct WlanConnection *)context);

	if (info != NULL) {
		if (connPtr->mode == 2 && (connPtr->flags & WLAN_CONN_PRIVACY) &&
		    info->apRsn.PSKPassPhraseLen != 0 &&
		    !info->apRsn.Akmp.keyExchange && !info->apRsn.Akmp.authenticate)
			return 1;
	}
	return 0;
}

/* Disassociates every station of the access point's BSS (reason: MIC failure), and has the MAC's filter updated for each. */
void Hostsa_DisAssocAllSta(struct WlanConnection *connPtr)
{
	uint8_t broadcast[6] __attribute__((aligned(4)));
	struct WlanConnection *sta;
	uint32_t posture;

	__aeabi_memset4(broadcast, sizeof(broadcast), 0xff);
	wlanMgmtSendDisassoc(connPtr, broadcast, broadcast, IEEEtypes_REASON_MIC_FAILURE, 0);
	posture = sysIrqDisable();
	for (sta = wlanConnectionFind(connPtr->bssType, connPtr->bssNumber); sta != NULL;
	     sta = wlanConnectionNextByBss(sta, sta->bssType, sta->bssNumber))
		macMgmtFilterBuffer(sta);
	sysIrqRestore(posture);
}

/*
 * The 60 s of the countermeasures timer ran out: after a first MIC failure
 * the count starts over; after a second, the stations may associate again,
 * the group rekey timer runs again and the host is told.
 */
void ApMicErrTimerExpCb(uint32_t timer, struct WlanConnection *connPtr)
{
	uint32_t posture = sysIrqDisable();
	struct WlanConnectionInfo *info;
	uint32_t value;

	/* a call without a connection leaves the interrupts disabled */
	if (connPtr == NULL)
		return;
	info = wlanConnectionInfo(connPtr);
	if (info == NULL)
		return;

	if (info->apMicError.status == FIRST_MIC_FAIL_IN_60_SEC) {
		info->apMicError.status = NO_MIC_FAILURE;
	} else if (info->apMicError.status == SECOND_MIC_FAIL_IN_60_SEC) {
		if (info->apRsn.mcstCipher.tkip && wlanInterfaceNeedsSecurity(connPtr))
			KeyMgmtStartGrpRekeyTimer(connPtr);
		info->apMicError.status = NO_MIC_FAILURE;
		info->apMicError.disableStaAsso = 0;
		if (wlanInterfaceNeedsSecurity(connPtr)) {
			value = info->apMicError.disableStaAsso;
			wlanConnectionInfo(connPtr);
			wlanRomServices.sendHostEvent(connPtr, WLAN_HOST_EVENT_UAP_MIC_COUNTERMEASURES_END,
						      &value, 2);
		}
	}
	sysIrqRestore(posture);
}

/*
 * A station reported a MIC failure (the Error bit of its EAPOL-Key): the first
 * in 60 s starts the timer; the second disassociates every station, bars new
 * ones for 60 s and, with TKIP, rekeys the group key (the periodic rekey stops).
 */
int ApMicCounterMeasureInvoke(struct WlanConnection *connPtr)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connPtr);
	int result = info->apMicError.MICCounterMeasureEnabled;
	uint32_t posture;

	if (result == 0)
		return result;

	posture = sysIrqDisable();
	if (info->apMicError.status == NO_MIC_FAILURE) {
		osaCreateTimer((OsaTimerCallback)ApMicErrTimerExpCb, (uint32_t)connPtr, MRVDRV_TIMER_60S,
			       &info->apMicTimer, 0);
		info->apMicError.status = FIRST_MIC_FAIL_IN_60_SEC;
	} else if (info->apMicError.status == FIRST_MIC_FAIL_IN_60_SEC) {
		info->apMicError.disableStaAsso = 1;
		info->apMicError.status = SECOND_MIC_FAIL_IN_60_SEC;
		osaCancelTimer(info->apMicTimer);
		osaCreateTimer((OsaTimerCallback)ApMicErrTimerExpCb, (uint32_t)connPtr, MRVDRV_TIMER_60S,
			       &info->apMicTimer, 0);
		sysIrqRestore(posture);
		Hostsa_DisAssocAllSta(connPtr);
		if (info->apRsn.mcstCipher.tkip && wlanInterfaceNeedsSecurity(connPtr)) {
			/* disable the periodic group rekey and rekey now */
			KeyMgmtStopHskTimer(connPtr, 1);
			ReInitGTK(connPtr);
		}
	}
	return sysIrqRestore(posture);
}

/*
 * An EAPOL-Key frame from a station of the access point: a MIC error report
 * starts the countermeasures; any other frame must carry the replay counter of
 * the message sent, and goes to the handler of the message waited for.
 */
int AuthenticatorProcessEapolPacket(OsaBuffer *pmbuf)
{
	struct WlanConnection *connPtr = pmbuf->context;
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connPtr);
	EAPOL_KeyMsg_t *rx_eapol_ptr;
	int result;

	if (!(connPtr->flags2 & WLAN_CONN2_ASSOCIATED))
		return 0;

	rx_eapol_ptr = (EAPOL_KeyMsg_t *)((uint8_t *)pmbuf + pmbuf->dataOffset);
	if (rx_eapol_ptr->key_info.Error)
		return ApMicCounterMeasureInvoke(connPtr);

	if (!isValidReplayCount(&role->keyMgmt, (uint8_t *)rx_eapol_ptr->replay_cnt))
		return 0;

	result = role->keyMgmt.keyMgmtState;
	switch (result) {
	case WAITING_4_MSG2:
		result = ProcessPWKMsg2(pmbuf);
		break;
	case WAITING_4_MSG4:
		result = ProcessPWKMsg4(pmbuf);
		break;
	case WAITING_4_GRPMSG2:
	case WAITING_4_GRP_REKEY_MSG2:
		result = ProcessGrpMsg2(pmbuf);
		break;
	default:
		return result;
	}
	if (result == 0)
		role->keyMgmt.numHskTries = 0;
	return result;
}

/* The handshake failed: stop its timer and deauthenticate the station. */
void handleFailedHSK(struct WlanConnection *connPtr, uint16_t reason)
{
	KeyMgmtStopHskTimer(connPtr, 0);
	macMgmtInspectBuffer(connPtr, connPtr->peerAddress, reason);
}

void incrementReplayCounter(apKeyMgmtInfoSta_t *pKeyMgmtInfo)
{
	if (++pKeyMgmtInfo->counterLo == 0)
		pKeyMgmtInfo->counterHi++;
}

/* The replay counter of a received frame (big endian) is the one of the message last sent. */
int isValidReplayCount(apKeyMgmtInfoSta_t *pKeyMgmtInfo, uint8_t *pRxReplayCount)
{
	uint32_t rxCounterHi;
	uint32_t rxCounterLo;

	__aeabi_memcpy(&rxCounterHi, pRxReplayCount, 4);
	__aeabi_memcpy(&rxCounterLo, pRxReplayCount + 4, 4);

	return htonl(rxCounterHi) == pKeyMgmtInfo->counterHi &&
	       htonl(rxCounterLo) == pKeyMgmtInfo->counterLo;
}
