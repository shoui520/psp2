/*
 * The station supplicant (Marvell's keyMgmtSta.c, 0x1a60..0x21dc): the one
 * supplicant's state, the 4-way and group handshakes around the ROM's
 * EAPOL-Key code, the TKIP countermeasures and the profile the association
 * uses. This build has a single connection: the state is global, and the
 * functions take the connection only where the frame or key code needs it.
 *
 * Written from Marvell's source (Copyright (C) 2014-2017, Marvell International
 * Ltd., GPL-2.0) as this firmware has it.
 */
#include "robin/buffer.h"
#include "robin/btrom.h"
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/join.h"
#include "robin/rom.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/wlan.h"
#include "robin/wlantask.h"

/* Not converted yet; provisional names */
void keyMgmtSendDeauth(struct WlanConnection *connPtr, IEEEtypes_MacAddr_t *pPeer,
		       uint16_t reason);					/* 0x17bd8 */
/* the SQRAM build's PMK cache and key API */
uint8_t *pmkCacheFindPSK(uint8_t *pSsid, uint8_t ssidLen);		/* 0xc0008190 */
uint32_t keyApi_UpdateKeyMaterial(struct WlanConnection *connPtr,
				  key_MgtMaterial_t *keyMgtData_p);	/* 0xc000869e */

/* the queue of data frames waiting for the MAC Tx task (wlan/driver.c) */
extern struct WlanDriverQueue wlanDriverQueue4;

#define EAPOL_PROTOCOL_V1			1
#define PWK_MSG1_RETRIES			7
#define RSNSECUREDTIMEOUT			10000000	/* us */
#define DEAUTH_DELAY_TIME_INTERVAL		40000		/* us */
#define MICRO_TIMER_FLAG_KILL_ON_PS_ENTRY	8

customMIB_RSNStats_t customMIB_RSNStats __attribute__((section(".itcm.rw"))) = { 0 };
SecurityParams_t currParams __attribute__((section(".itcm.rw"))) = { { 0 }, { 0 }, { 0 } };
IEEEtypes_MacAddr_t localBssid __attribute__((section(".itcm.rw"))) = { 0 };
IEEEtypes_MacAddr_t localStaAddr __attribute__((section(".itcm.rw"))) = { 0 };
supplicantData_t suppData __attribute__((section(".itcm.bss")));

void keyMgmtSendDeauth2Peer(struct WlanConnection *connPtr, uint16_t reason)
{
	/* Assumes we are sending to the AP */
	keyMgmtSendDeauth(connPtr, &localBssid, reason);
}

BOOLEAN keyMgmtProcessMsgExt(EAPOL_KeyMsg_t *pKeyMsg)
{
	return 1;
}

uint16_t keyMgmtFormatWpaRsnIe(uint8_t *pos, IEEEtypes_MacAddr_t *pBssid,
			       IEEEtypes_MacAddr_t *pStaAddr, uint8_t *pPmkid, BOOLEAN addPmkid)
{
	return keyMgmtFormatWpaRsnIe_internal(&suppData.customMIB_RSNConfig, pos, pBssid,
					      pStaAddr, pPmkid, addPmkid);
}

int GeneratePWKMsg2(OsaBuffer *pmbuf, uint8_t *pSNonce, uint8_t *pEAPOLMICKey,
		    uint8_t forceKeyDescVersion)
{
	EAPOL_KeyMsg_t *pRxEapol = (EAPOL_KeyMsg_t *)((uint8_t *)pmbuf + pmbuf->dataOffset);
	EAPOL_KeyMsg_Tx_t *pTxEapol;
	OsaBuffer *pBufDesc;
	uint16_t frameLen;

	pBufDesc = GetTxEAPOLBuffer(pmbuf->context, &pTxEapol);
	if (pBufDesc == NULL)
		return 1;

	KeyMgmtSta_PrepareEAPOLFrame(pTxEapol, pRxEapol, localBssid, localStaAddr, pSNonce);

	/* Add the RSN/WPA IE if not dynamic WEP */
	if (pTxEapol->keyMsg.desc_type != 1)
		pTxEapol->keyMsg.key_material_len =
			keyMgmtFormatWpaRsnIe(pTxEapol->keyMsg.key_data, &localBssid,
					      &localStaAddr, NULL, 0);

	frameLen = KeyMgmtSta_PopulateEAPOLLengthMic(pTxEapol, pEAPOLMICKey, EAPOL_PROTOCOL_V1,
						     forceKeyDescVersion);
	wlanSetEventPacketLength(pBufDesc, frameLen);
	return 0;
}

uint16_t keyMgmtGetKeySize(uint8_t isPairwise)
{
	return keyMgmtGetKeySize_internal(&suppData.customMIB_RSNConfig, isPairwise);
}

void FillKeyMaterialStruct(struct WlanConnection *connPtr, uint16_t key_len, uint8_t isPairwise,
			   KeyData_t *pKey)
{
	key_MgtMaterial_t keyMgtData;

	FillKeyMaterialStruct_internal(&keyMgtData, key_len, isPairwise, pKey);
	keyApi_UpdateKeyMaterial(connPtr, &keyMgtData);
}

void keyMgmtPlumbPairwiseKey(struct WlanConnection *connPtr)
{
	keyMgmtInfoSta_t *pKeyMgmtInfoSta = &suppData.keyMgmtInfoSta;

	__aeabi_memcpy4(&pKeyMgmtInfoSta->PWKey, &pKeyMgmtInfoSta->newPWKey,
			sizeof(pKeyMgmtInfoSta->PWKey));
	FillKeyMaterialStruct(connPtr, keyMgmtGetKeySize(1), 1, &pKeyMgmtInfoSta->PWKey);
}

/* Also the release callback of message 4. */
int keyMgmtKeyPairwiseTxDone(OsaBuffer *pBufDesc)
{
	keyMgmtInfoSta_t *pKeyMgmtInfoSta = &suppData.keyMgmtInfoSta;

	keyMgmtPlumbPairwiseKey(pBufDesc->context);
	pKeyMgmtInfoSta->pwkHandshakeComplete = 1;
	if (pKeyMgmtInfoSta->pRxDecryptKey != NULL)
		pKeyMgmtInfoSta->pRxDecryptKey->hdr.keyState &= ~KEY_STATE_FORCE_EAPOL_UNENCRYPTED;
	return 0;
}

/* Also the release callback of group message 2. */
int keyMgmtKeyGroupTxDone(OsaBuffer *pBufDesc)
{
	keyMgmtInfoSta_t *pKeyMgmtInfoSta = &suppData.keyMgmtInfoSta;

	FillKeyMaterialStruct(pBufDesc->context, keyMgmtGetKeySize(0), 0, &pKeyMgmtInfoSta->GRKey);
	pKeyMgmtInfoSta->RSNDataTrafficEnabled = 1;
	if (pKeyMgmtInfoSta->RSNSecured == 0) {
		pKeyMgmtInfoSta->RSNSecured = 1;
		keyMgmtControlledPortOpen();
	}
	return 0;
}

int GeneratePWKMsg4(OsaBuffer *pmbuf, keyMgmtInfoSta_t *pKeyMgmtInfoSta, BOOLEAN groupKeyReceived)
{
	EAPOL_KeyMsg_t *pRxEapol = (EAPOL_KeyMsg_t *)((uint8_t *)pmbuf + pmbuf->dataOffset);
	EAPOL_KeyMsg_Tx_t *pTxEapol;
	OsaBuffer *pBufDesc;
	uint32_t frameLen;

	pBufDesc = GetTxEAPOLBuffer(pmbuf->context, &pTxEapol);
	if (pBufDesc == NULL)
		return 1;

	KeyMgmtSta_PrepareEAPOLFrame(pTxEapol, pRxEapol, localBssid, localStaAddr, NULL);
	frameLen = KeyMgmtSta_PopulateEAPOLLengthMic(pTxEapol, pKeyMgmtInfoSta->EAPOL_MIC_Key,
						     EAPOL_PROTOCOL_V1, 0);

	/*
	 * Once data flows, the new key is set only when the AP has the 4th
	 * message: the buffer's release does it.
	 */
	if (pKeyMgmtInfoSta->RSNDataTrafficEnabled) {
		pBufDesc->flags |= 1;
		if (groupKeyReceived)
			pBufDesc->release = keyMgmtKeyPairAndGroupTxDone;
		else
			pBufDesc->release = keyMgmtKeyPairwiseTxDone;
	} else if (groupKeyReceived) {
		keyMgmtKeyPairAndGroupTxDone(pmbuf);
	} else {
		keyMgmtKeyPairwiseTxDone(pmbuf);
	}

	wlanSetEventPacketLength(pBufDesc, frameLen);
	return 0;
}

int GenerateGrpMsg2(OsaBuffer *pmbuf, keyMgmtInfoSta_t *pKeyMgmtInfoSta)
{
	EAPOL_KeyMsg_t *pRxEapol = (EAPOL_KeyMsg_t *)((uint8_t *)pmbuf + pmbuf->dataOffset);
	EAPOL_KeyMsg_Tx_t *pTxEapol;
	OsaBuffer *pBufDesc;
	uint16_t frameLen;

	pBufDesc = GetTxEAPOLBuffer(pmbuf->context, &pTxEapol);
	if (pBufDesc == NULL)
		return 1;

	KeyMgmtSta_PrepareEAPOLFrame(pTxEapol, pRxEapol, localBssid, localStaAddr, NULL);
	frameLen = KeyMgmtSta_PopulateEAPOLLengthMic(pTxEapol, pKeyMgmtInfoSta->EAPOL_MIC_Key,
						     EAPOL_PROTOCOL_V1, 0);
	pBufDesc->flags |= 1;
	pBufDesc->release = keyMgmtKeyGroupTxDone;
	wlanSetEventPacketLength(pBufDesc, frameLen);
	return 0;
}

BOOLEAN KeyMgmtStaHsk_Recvd_PWKMsg1(OsaBuffer *pmbuf, IEEEtypes_MacAddr_t *sa,
				    IEEEtypes_MacAddr_t *da)
{
	keyMgmtInfoSta_t *pKeyMgmtInfoSta = &suppData.keyMgmtInfoSta;
	RSNConfig_t *pRsnConfig = &suppData.customMIB_RSNConfig;
	EAPOL_KeyMsg_t *pKeyMsg;
	BOOLEAN retval = 0;
	uint8_t *pPmk;

	pKeyMsg = GetKeyMsgNonceFromEAPOL(pmbuf, pKeyMgmtInfoSta);
	if (pKeyMsg == NULL)
		return 0;

	if (supplicantAkmIsWpaWpa2(&pRsnConfig->AKM)) {
		if (supplicantAkmIsWpaWpa2Psk(&pRsnConfig->AKM))
			pPmk = pmkCacheFindPSK(suppData.hashSsId.SsId, suppData.hashSsId.Len);
		else
			pPmk = pmkCacheFindPMK(&localBssid);
		if (pPmk == NULL)
			return 0;

		KeyMgmtSta_DeriveKeys(pPmk, *da, *sa, pKeyMgmtInfoSta->ANonce,
				      pKeyMgmtInfoSta->SNonce, pKeyMgmtInfoSta->EAPOL_MIC_Key,
				      pKeyMgmtInfoSta->EAPOL_Encr_Key, &pKeyMgmtInfoSta->newPWKey,
				      supplicantAkmUsesKdf(&pRsnConfig->AKM));
		retval = 1;
	}

	/* construct message 2 */
	if (GeneratePWKMsg2(pmbuf, pKeyMgmtInfoSta->SNonce, pKeyMgmtInfoSta->EAPOL_MIC_Key, 0) != 0)
		return 0;

	if (retval == 1) {
		updateApReplayCounter(pKeyMgmtInfoSta, (uint8_t *)pKeyMsg->replay_cnt);
		pKeyMgmtInfoSta->RSNSecured = 0;
	}
	return retval;
}

/* Returns the message when message 4 could not be sent. */
EAPOL_KeyMsg_t *KeyMgmtStaHsk_Recvd_PWKMsg3(OsaBuffer *pmbuf)
{
	keyMgmtInfoSta_t *pKeyMgmtInfoSta = &suppData.keyMgmtInfoSta;
	EAPOL_KeyMsg_t *pKeyMsg;

	pKeyMsg = ProcessRxEAPOL_PwkMsg3(pmbuf, pKeyMgmtInfoSta);
	if (pKeyMsg == NULL)
		return NULL;

	/* construct message 4 */
	if (GeneratePWKMsg4(pmbuf, pKeyMgmtInfoSta, pKeyMsg->desc_type == 2) != 0)
		return pKeyMsg;

	updateApReplayCounter(pKeyMgmtInfoSta, (uint8_t *)pKeyMsg->replay_cnt);
	return NULL;
}

int KeyMgmtStaHsk_Recvd_GrpMsg1(OsaBuffer *pmbuf)
{
	keyMgmtInfoSta_t *pKeyMgmtInfoSta = &suppData.keyMgmtInfoSta;
	EAPOL_KeyMsg_t *pKeyMsg;

	pKeyMsg = ProcessRxEAPOL_GrpMsg1(pmbuf, pKeyMgmtInfoSta);
	if (pKeyMsg != NULL && GenerateGrpMsg2(pmbuf, pKeyMgmtInfoSta) == 0)
		updateApReplayCounter(pKeyMgmtInfoSta, (uint8_t *)pKeyMsg->replay_cnt);
	return 0;
}

void ProcessKeyMgmtDataSta(OsaBuffer *pmbuf, IEEEtypes_MacAddr_t *sa, IEEEtypes_MacAddr_t *da)
{
	EAPOL_KeyMsg_t *pKeyMsg = (EAPOL_KeyMsg_t *)((uint8_t *)pmbuf + pmbuf->dataOffset);
	uint8_t retry = 0;

	if (!pKeyMsg->key_info.KeyType) {
		/* GRP */
		KeyMgmtStaHsk_Recvd_GrpMsg1(pmbuf);
	} else if (pKeyMsg->key_info.KeyMIC) {
		/* 3rd message of the sequence */
		KeyMgmtStaHsk_Recvd_PWKMsg3(pmbuf);
	} else {
		/*
		 * Delay and retry message 1 in case the failure was the host
		 * not having had the time to program a PMK yet (802.1X AKMs).
		 */
		while (!KeyMgmtStaHsk_Recvd_PWKMsg1(pmbuf, sa, da) && retry < PWK_MSG1_RETRIES) {
			osaTaskSleep(1);
			retry++;
		}
	}
}

void supplicantMICCounterMeasureInvoke(struct WlanConnection *connPtr, BOOLEAN isUnicast)
{
	keyMgmtInfoSta_t *pKeyMgmtInfoSta = &suppData.keyMgmtInfoSta;
	uint8_t state;

	if (!pKeyMgmtInfoSta->sta_MIC_Error.MICCounterMeasureEnabled)
		return;

	state = pKeyMgmtInfoSta->sta_MIC_Error.status;

	/* clear any pending TX packets, so a TX buffer can be had */
	cbProcFlush();
	SendMICFailReport_sta(connPtr, pKeyMgmtInfoSta, isUnicast);

	switch (state) {
	case NO_MIC_FAILURE:
		/* the first MIC failure */
		KeyMgmtSta_handleMICErr(state, pKeyMgmtInfoSta,
					(MicroTimerCallback_t)MicErrTimerExp_Sta,
					MICRO_TIMER_FLAG_KILL_ON_PS_ENTRY);
		customMIB_RSNStats.TKIPLocalMICFailures++;
		break;
	case FIRST_MIC_FAIL_IN_60_SEC:
		/* two MIC failures within 60 s: deauthenticate from the AP */
		customMIB_RSNStats.TKIPCounterMeasuresInvoked++;
		KeyMgmtSta_handleMICErr(state, pKeyMgmtInfoSta,
					(MicroTimerCallback_t)MicErrTimerExp_Sta,
					MICRO_TIMER_FLAG_KILL_ON_PS_ENTRY);
		pKeyMgmtInfoSta->psapriv = connPtr;
		KeyMgmtSta_handleMICDeauthTimer(pKeyMgmtInfoSta,
						(MicroTimerCallback_t)DeauthDelayTimerExp_Sta,
						DEAUTH_DELAY_TIME_INTERVAL,
						MICRO_TIMER_FLAG_KILL_ON_PS_ENTRY);
		break;
	default:
		break;
	}
}

/* Starts the key management session. */
void keyMgmtSta_StartSession(struct WlanConnection *connPtr, IEEEtypes_MacAddr_t *pBssid,
			     IEEEtypes_MacAddr_t *pStaAddr)
{
	keyMgmtInfoSta_t *pKeyMgmtInfoSta = &suppData.keyMgmtInfoSta;

	pKeyMgmtInfoSta->psapriv = connPtr;
	__aeabi_memcpy(localStaAddr, pStaAddr, sizeof(localStaAddr));
	__aeabi_memcpy(localBssid, pBssid, sizeof(localBssid));
	keyMgmtSta_StartSession_internal(pKeyMgmtInfoSta,
					 (MicroTimerCallback_t)keyMgmtStaRsnSecuredTimeoutHandler,
					 RSNSECUREDTIMEOUT, MICRO_TIMER_FLAG_KILL_ON_PS_ENTRY);
}

void supplicantInitSession(struct WlanConnection *connPtr, uint8_t *pSsid, uint16_t len,
			   IEEEtypes_MacAddr_t *pBssid, IEEEtypes_MacAddr_t *pStaAddr)
{
	if (suppData.customMIB_RSNConfig.RSNEnabled) {
		KeyMgmtSta_InitSession(&suppData.keyMgmtInfoSta);
		__aeabi_memcpy(suppData.hashSsId.SsId, pSsid, len);
		suppData.hashSsId.Len = len;
		keyMgmtSta_StartSession(connPtr, pBssid, pStaAddr);
	}
}

uint8_t supplicantIsCounterMeasuresActive(void)
{
	return suppData.keyMgmtInfoSta.sta_MIC_Error.disableStaAsso;
}

void init_customApp_mibs(void)
{
	__aeabi_memclr(&customMIB_RSNStats, sizeof(customMIB_RSNStats));
	__aeabi_memclr(&suppData.customMIB_RSNConfig, sizeof(suppData.customMIB_RSNConfig));
	/* keep noRsn = 1 as the default setting */
	suppData.customMIB_RSNConfig.wpaType.noRsn = 1;
}

SecurityMode_t supplicantCurrentSecurityMode(void)
{
	return suppData.customMIB_RSNConfig.wpaType;
}

int supplicantIsEnabled(void)
{
	return suppData.customMIB_RSNConfig.RSNEnabled;
}

void supplicantDisable(void)
{
	suppData.customMIB_RSNConfig.RSNEnabled = 0;
	init_customApp_mibs();
}

void supplicantEnable(void)
{
	suppData.customMIB_RSNConfig.RSNEnabled = 1;
}

void supplicantSetAssocRsn(SecurityMode_t wpaType, Cipher_t *pMcstCipher, Cipher_t *pUcstCipher,
			   AkmSuite_t *pAkm, IEEEtypes_RSNCapability_t *pRsnCap,
			   Cipher_t *pGrpMgmtCipher)
{
	IEEEtypes_RSNCapability_t rsnCap;

	if (pRsnCap == NULL) {
		/*
		 * An IOT workaround for APs that do not handle association
		 * requests without the RSN capabilities.
		 */
		pRsnCap = &rsnCap;
		__aeabi_memclr(&rsnCap, sizeof(rsnCap));
	}
	supplicantSetAssocRsn_internal(&suppData.customMIB_RSNConfig, &currParams, wpaType,
				       pMcstCipher, pUcstCipher, pAkm, pRsnCap, pGrpMgmtCipher);
}

void install_wpa_none_keys(struct WlanConnection *connPtr, uint8_t type, uint8_t unicast)
{
	key_MgtMaterial_t keyMgtData;
	uint8_t *pPMK;

	pPMK = pmkCacheFindPSK(suppData.hashSsId.SsId, suppData.hashSsId.Len);
	if (pPMK == NULL)
		return;

	install_wpa_none_keys_internal(&keyMgtData, pPMK, type, unicast);
	keyApi_UpdateKeyMaterial(connPtr, &keyMgtData);

	/* there is no timer or other to initialise */
	KeyMgmtSta_InitSession(&suppData.keyMgmtInfoSta);
	suppData.keyMgmtInfoSta.RSNSecured = 1;
}

void supplicantInstallWpaNoneKeys(struct WlanConnection *connPtr)
{
	RSNConfig_t *pRsnConfig = &suppData.customMIB_RSNConfig;

	if (pRsnConfig->RSNEnabled && pRsnConfig->wpaType.wpaNone) {
		install_wpa_none_keys(connPtr, pRsnConfig->mcstCipher.ccmp, 0);
		install_wpa_none_keys(connPtr, pRsnConfig->mcstCipher.ccmp, 1);
	}
}

void supplicantSetProfile(SecurityMode_t wpaType, Cipher_t mcstCipher, Cipher_t ucstCipher)
{
	currParams.wpaType = wpaType;
	currParams.mcstCipher = mcstCipher;
	currParams.ucstCipher = ucstCipher;
}

void supplicantGetProfile(SecurityMode_t *pWpaType, Cipher_t *pMcstCipher, Cipher_t *pUcstCipher)
{
	*pWpaType = currParams.wpaType;
	*pMcstCipher = currParams.mcstCipher;
	*pUcstCipher = currParams.ucstCipher;
}

void supplicantGetProfileCurrent(SecurityMode_t *pWpaType, Cipher_t *pMcstCipher,
				 Cipher_t *pUcstCipher)
{
	*pWpaType = suppData.customMIB_RSNConfig.wpaType;
	*pMcstCipher = suppData.customMIB_RSNConfig.mcstCipher;
	*pUcstCipher = suppData.customMIB_RSNConfig.ucstCipher;
}

void supplicantInit(void)
{
	keyMgmtInfoSta_t *pKeyMgmtInfoSta = &suppData.keyMgmtInfoSta;

	init_customApp_mibs();
	__aeabi_memset4(&currParams, sizeof(currParams), 0xff);
	pKeyMgmtInfoSta->sta_MIC_Error.MICCounterMeasureEnabled = 1;
	pKeyMgmtInfoSta->sta_MIC_Error.disableStaAsso = 0;
	pKeyMgmtInfoSta->sta_MIC_Error.status = NO_MIC_FAILURE;
	KeyMgmtResetCounter(pKeyMgmtInfoSta);
}

void supplicantStopSessionTimer(void)
{
	keyMgmtInfoSta_t *pKeyMgmtInfoSta = &suppData.keyMgmtInfoSta;

	if (pKeyMgmtInfoSta->rsnTimer != 0) {
		osaCancelTimer(pKeyMgmtInfoSta->rsnTimer);
		pKeyMgmtInfoSta->rsnTimer = 0;
	}
}

void supplicantSmeResetNotification(void)
{
	supplicantStopSessionTimer();
}

void keyMgmtSuccessfulDecryptNotify(cipher_key_t *pRxCipherKey)
{
	keyMgmtInfoSta_t *pKeyMgmtInfoSta = &suppData.keyMgmtInfoSta;

	pKeyMgmtInfoSta->pRxDecryptKey = pRxCipherKey;
	if (pRxCipherKey != NULL &&
	    (!suppData.customMIB_RSNConfig.RSNEnabled || pKeyMgmtInfoSta->pwkHandshakeComplete))
		pRxCipherKey->hdr.keyState &= ~KEY_STATE_FORCE_EAPOL_UNENCRYPTED;
}

void keyMgmtSta_RomInit(void)
{
	ramHook_keyMgmtProcessMsgExt = (int (*)())keyMgmtProcessMsgExt;
	ramHook_keyMgmtSendDeauth = (int (*)())keyMgmtSendDeauth2Peer;
}

uint8_t *parseKeyDataField(uint8_t *pKey, uint16_t len)
{
	KDE_t *pKde;

	/* parse the GTK KDE */
	parseKeyDataGTK(pKey, len, &suppData.keyMgmtInfoSta.GRKey);

	/* parse the PMKID, though it is not used now */
	pKde = parseKeyKDE_DataType(pKey, len, KDE_DATA_TYPE_PMKID);
	if (pKde != NULL)
		return pKde->data;
	return NULL;
}

/* Adds the RSN element (WPA2-PSK, CCMP) to a frame body. */
uint16_t btampAddRsnIe(IEEEtypes_RSNElement_t *pRsnIe)
{
	const uint8_t wpa2_psk[4] = { 0x00, 0x0f, 0xac, 0x02 };	/* WPA2 PSK */
	IEEEtypes_RSNCapability_t rsncap;
	SecurityMode_t securityMode;
	Cipher_t mcstWpa2;
	Cipher_t ucstWpa2;

	__aeabi_memclr(&securityMode, sizeof(securityMode));
	__aeabi_memclr(&mcstWpa2, sizeof(mcstWpa2));
	__aeabi_memclr(&ucstWpa2, sizeof(ucstWpa2));
	__aeabi_memclr(&rsncap, sizeof(rsncap));

	mcstWpa2.ccmp = 1;
	ucstWpa2.ccmp = 1;
	securityMode.wpa2 = 1;

	supplicantSetProfile(securityMode, mcstWpa2, ucstWpa2);
	supplicantSetAssocRsn(securityMode, &mcstWpa2, &ucstWpa2, (AkmSuite_t *)wpa2_psk,
			      &rsncap, NULL);
	return keyMgmtFormatWpaRsnIe((uint8_t *)pRsnIe, NULL, NULL, NULL, 0);
}

int keyMgmtKeyPairAndGroupTxDone(OsaBuffer *pBufDesc)
{
	keyMgmtKeyPairwiseTxDone(pBufDesc);
	keyMgmtKeyGroupTxDone(pBufDesc);
	return 0;
}

/* Sends a MIC failure report to the AP. */
void SendMICFailReport_sta(struct WlanConnection *connPtr, keyMgmtInfoSta_t *pKeyMgmtInfoSta,
			   BOOLEAN isUnicast)
{
	RSNConfig_t *pRsnConfig = &suppData.customMIB_RSNConfig;
	EAPOL_KeyMsg_Tx_t *pTxEapol;
	OsaBuffer *pBufDesc;
	uint16_t frameLen;
	UINT int_sta;

	if (pKeyMgmtInfoSta->staCounterHi == 0xffffffff &&
	    pKeyMgmtInfoSta->staCounterLo == 0xffffffff) {
		KeyMgmtResetCounter(pKeyMgmtInfoSta);
		return;
	}

	/* Since there is a MIC failure, drop all packets in the Tx queue. */
	int_sta = sysIrqDisable();
	while ((pBufDesc = osa_MsgQueueRecv(&wlanDriverQueue4)) != NULL)
		wlanCompletePacket(pBufDesc, 0);
	sysIrqRestore(int_sta);

	pBufDesc = GetTxEAPOLBuffer(connPtr, &pTxEapol);
	if (pBufDesc == NULL)
		return;

	KeyMgmtSta_PrepareEAPOLMicErrFrame(pTxEapol, isUnicast, &localBssid, &localStaAddr,
					   pKeyMgmtInfoSta);
	SetEAPOLKeyDescTypeVersion(pTxEapol, pRsnConfig->wpaType.wpa2,
				   supplicantAkmUsesKdf(&pRsnConfig->AKM), pRsnConfig->ucstCipher.ccmp);

	if (++pKeyMgmtInfoSta->staCounterLo == 1)
		pKeyMgmtInfoSta->staCounterHi++;

	frameLen = KeyMgmtSta_PopulateEAPOLLengthMic(pTxEapol, pKeyMgmtInfoSta->EAPOL_MIC_Key,
						     EAPOL_PROTOCOL_V1, 0);
	wlanSetEventPacketLength(pBufDesc, frameLen);
}
