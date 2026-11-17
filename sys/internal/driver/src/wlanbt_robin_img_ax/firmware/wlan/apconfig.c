/*
 * The access point's configuration: HostCmd 0xb0 (Marvell's
 * UAP_SYS_CONFIGURE) sets or reads the BSS's parameters as TLVs, and the
 * STA_INFO TLV of HostCmd 0xb3 (UAP_STA_LIST). Marvell published no source
 * for it; the filename and the names are inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/adhoc.h"
#include "robin/apconfig.h"
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/apsta.h"
#include "robin/ie.h"
#include "robin/macentry.h"
#include "robin/mgmt.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/txpower.h"
#include "robin/util.h"
#include "robin/wep.h"
#include "robin/wapiie.h"
#include "robin/wmm.h"

#define U16(bytes)		(((const struct __attribute__((packed)) { uint16_t word; } *)(bytes))->word)
#define PUT_U16(bytes, value)	(((struct __attribute__((packed)) { uint16_t word; } *)(bytes))->word = (uint16_t)(value))

/* The TLVs of HostCmd 0xb0 (Marvell's TLV numbers; below 0x100 an element's ID). */
#define WLAN_TLV_SSID		0x0000
#define WLAN_TLV_RATES		0x0001
#define WLAN_TLV_HT_CAPABILITY	0x002d
#define WLAN_TLV_HT_OPERATION	0x003d
#define WLAN_TLV_VENDOR		0x00dd
#define WLAN_TLV_AUTH_TYPE	0x011f
#define WLAN_TLV_CHANNEL_CONFIG	0x012a
#define WLAN_TLV_BEACON_PERIOD	0x012c
#define WLAN_TLV_DTIM_PERIOD	0x012d
#define WLAN_TLV_012e		0x012e	/* not supported */
#define WLAN_TLV_TX_POWER	0x012f
#define WLAN_TLV_BCAST_SSID_CTL	0x0130
#define WLAN_TLV_CHANNEL_LIST	0x0101
#define WLAN_TLV_MAC_ADDRESS	0x012b
#define WLAN_TLV_PREAMBLE_CTL	0x0131
#define WLAN_TLV_ANTENNA_CTL	0x0132
#define WLAN_TLV_RTS_THRESHOLD	0x0133
#define WLAN_TLV_0134		0x0134	/* bit 2 of the protection flags */
#define WLAN_TLV_UNICAST_RATE	0x0135
#define WLAN_TLV_013a		0x013a	/* not read back */
#define WLAN_TLV_MAC_FILTER	0x0138
#define WLAN_TLV_STA_AGEOUT	0x0139
#define WLAN_TLV_WEP_KEY	0x013b
#define WLAN_TLV_PASSPHRASE	0x013c
#define WLAN_TLV_ENCRYPT_PROTOCOL	0x0140
#define WLAN_TLV_AUTH_KEY	0x0141
#define WLAN_TLV_WPA_CIPHER	0x0142
#define WLAN_TLV_FRAG_THRESHOLD	0x0146
#define WLAN_TLV_GRP_REKEY_TIME	0x0147
#define WLAN_TLV_MAX_STA_COUNT	0x0155
#define WLAN_TLV_RETRY_LIMIT	0x015d
#define WLAN_TLV_WAPI		0x015e
#define WLAN_TLV_TX_DATA_RATE	0x0162
#define WLAN_TLV_BSS_READY	0x0164
#define WLAN_TLV_MGMT_IE	0x0169	/* the custom IEs (wlanCustomIeSet()) */
#define WLAN_TLV_MGMT_FORWARD_MASK	0x0170
#define WLAN_TLV_PWK_HSK_TIMEOUT	0x0175
#define WLAN_TLV_PWK_HSK_RETRIES	0x0176
#define WLAN_TLV_GWK_HSK_TIMEOUT	0x0177
#define WLAN_TLV_GWK_HSK_RETRIES	0x0178
#define WLAN_TLV_PS_STA_AGEOUT	0x017b
#define WLAN_TLV_UCAST_CIPHER	0x0191
#define WLAN_TLV_MCAST_CIPHER	0x0192
#define WLAN_TLV_BSS_STATUS	0x0193	/* whether the BSS runs: read only */
#define WLAN_TLV_HT2040_MANAGED	0x0198

/* The ucast cipher TLV's protocol bits the ciphers it gives apply to. */
#define WLAN_PROTOCOL_WPA	0x08
#define WLAN_PROTOCOL_WPA2	0x20

/* *status of wlanApConfigTlvSet(): what keeps the command from being applied. */
#define WLAN_AP_CONFIG_RESTART		1	/* the BSS runs: the setting cannot change now, or the value is invalid */
#define WLAN_AP_CONFIG_UNSUPPORTED	2

/* A channel's scan times when the host gives a channel list: 2000 TU over all of them. */
#define WLAN_AP_SCAN_TOTAL	2000u
#define WLAN_AP_SCAN_TIME_MAX	110

/* 0x540fc: the TLVs a write may carry while the BSS runs. */
static const uint16_t wlanApConfigRunningTlvs[12] = {
	WLAN_TLV_TX_POWER, WLAN_TLV_RETRY_LIMIT, WLAN_TLV_RTS_THRESHOLD, WLAN_TLV_TX_DATA_RATE,
	WLAN_TLV_UNICAST_RATE, WLAN_TLV_ANTENNA_CTL, WLAN_TLV_0134, WLAN_TLV_FRAG_THRESHOLD,
	WLAN_TLV_BSS_READY, WLAN_TLV_VENDOR, WLAN_TLV_MGMT_IE, WLAN_TLV_MGMT_FORWARD_MASK,
};

/* 0x54114: the TLVs a read without any gives back, in this order. */
static const uint16_t wlanApConfigAllTlvs[48] = {
	WLAN_TLV_SSID, WLAN_TLV_RATES, WLAN_TLV_MAC_ADDRESS, WLAN_TLV_BEACON_PERIOD,
	WLAN_TLV_DTIM_PERIOD, WLAN_TLV_TX_POWER, WLAN_TLV_BCAST_SSID_CTL, WLAN_TLV_PREAMBLE_CTL,
	WLAN_TLV_ANTENNA_CTL, WLAN_TLV_ANTENNA_CTL, WLAN_TLV_0134, WLAN_TLV_PKT_FWD_CTL,
	WLAN_TLV_RTS_THRESHOLD, WLAN_TLV_FRAG_THRESHOLD, WLAN_TLV_UNICAST_RATE, WLAN_TLV_STA_AGEOUT,
	WLAN_TLV_PS_STA_AGEOUT, WLAN_TLV_WEP_KEY, WLAN_TLV_WEP_KEY, WLAN_TLV_WEP_KEY,
	WLAN_TLV_WEP_KEY, WLAN_TLV_AUTH_TYPE, WLAN_TLV_MAC_FILTER, WLAN_TLV_ENCRYPT_PROTOCOL,
	WLAN_TLV_WAPI, WLAN_TLV_MAX_STA_COUNT, WLAN_TLV_RETRY_LIMIT, WLAN_TLV_CHANNEL_CONFIG,
	WLAN_TLV_CHANNEL_LIST, WLAN_TLV_TX_DATA_RATE, WLAN_TLV_AUTH_KEY, WLAN_TLV_UCAST_CIPHER,
	WLAN_TLV_UCAST_CIPHER, WLAN_TLV_MCAST_CIPHER, WLAN_TLV_GRP_REKEY_TIME, WLAN_TLV_PASSPHRASE,
	WLAN_TLV_PWK_HSK_TIMEOUT, WLAN_TLV_PWK_HSK_RETRIES, WLAN_TLV_GWK_HSK_TIMEOUT, WLAN_TLV_GWK_HSK_RETRIES,
	WLAN_TLV_BSS_READY, WLAN_TLV_HT_CAPABILITY, WLAN_TLV_VENDOR, WLAN_TLV_MGMT_IE,
	WLAN_TLV_BSS_STATUS, WLAN_TLV_MGMT_FORWARD_MASK, WLAN_TLV_HT2040_MANAGED, WLAN_TLV_HT_OPERATION,
};

/* What a read without TLVs asks for: both antennas, the four WEP keys, both ucast ciphers. */
#define WLAN_AP_CONFIG_ANTENNAS		2
#define WLAN_AP_CONFIG_WEP_KEYS		0xe4	/* keys 0, 1, 2, 3 */
#define WLAN_AP_CONFIG_AUTH_KEY_LENGTH	4

/* The SSID's pointer is to WlanConnectionInfo.ssid; the length and periods follow it. */
#define WLAN_SSID_INFO(ssid) \
	((struct WlanConnectionInfo *)((uint8_t *)(ssid) - offsetof(struct WlanConnectionInfo, ssid)))

/* 0xc0011bac: writes the custom IE TLV's value to out, gives back its length; nothing installs it. */
uint16_t (*wlanApConfigTlvGetHook)(uint8_t *out, uint16_t value) __attribute__((section(".sqram1.bss")));

extern int (*wlanEdcaApplyHook)(struct WlanConnection *connection);	/* SQRAM 0xc0011bb4 */

/* 0x0002b3c8: a STA_INFO TLV's header and the station's address. */
void wlanTlvStaInfoSet(uint8_t *tlv, uint8_t *address, const uint8_t *peer)
{
	PUT_U16(tlv, WLAN_TLV_STA_INFO);
	PUT_U16(tlv + 2, 8);
	__aeabi_memcpy(address, peer, 6);
}

/*
 * 0x00037db4: the TLVs a read of HostCmd 0xb0 asks for, as the list of types
 * the reply is built from. A TLV that comes back several times is listed as
 * many times (an empty antenna or cipher TLV twice, an empty or out of range
 * WEP key TLV for all four keys); the antennas and the WEP keys asked for are
 * kept in *antennas (a bit each) and *wepKeys (two bits each), and the
 * authentication key TLV's length in *authKeyLength. Gives back the last TLV.
 */
const uint8_t *wlanApConfigRequestParse(unsigned int length, const uint8_t *tlv, uint16_t *types,
					 int *count, uint32_t *antennas, uint32_t *wepKeys,
					 uint32_t *unused, uint16_t *authKeyLength)
{
	const uint8_t *last = NULL;
	unsigned int antenna = 0;
	unsigned int wepKey = 0;
	unsigned int type;

	while (length >= 4) {
		last = tlv;
		types[(*count)++] = U16(tlv);
		type = U16(tlv);
		switch (type) {
		case WLAN_TLV_ANTENNA_CTL:
			if (U16(tlv + 2) == 0) {
				types[(*count)++] = type;
				break;
			}
			*antennas &= ~(1u << antenna);
			*antennas |= tlv[4] << antenna;
			antenna++;
			break;
		case WLAN_TLV_WEP_KEY:
			if (U16(tlv + 2) == 0 || tlv[4] >= 4) {
				types[(*count)++] = type;
				types[(*count)++] = U16(tlv);
				types[(*count)++] = U16(tlv);
				break;
			}
			*wepKeys &= ~(3u << 2 * wepKey);
			*wepKeys |= tlv[4] << 2 * wepKey;
			wepKey++;
			break;
		case WLAN_TLV_AUTH_KEY:
			*authKeyLength = U16(tlv + 2);
			break;
		case WLAN_TLV_UCAST_CIPHER:
			if (U16(tlv + 2) == 0)
				types[(*count)++] = type;
			break;
		}
		length = length - U16(tlv + 2) - 4;
		tlv += U16(tlv + 2) + 4;
	}
	return last;
}

/*
 * 0x0002ab60: applies one TLV of a HostCmd 0xb0 write to the BSS's settings.
 * The settings a TLV's value is copied to whole (rates, the passphrase, the
 * timeouts) are given back in *copyTo for the caller to copy; an SSID or
 * passphrase that changes sets *pmkChanged. A setting that cannot change while
 * the BSS runs (bssStarted) sets *status instead. Gives back 0 for a TLV it
 * does not know (and *copyTo NULL), else 1.
 */
int wlanApConfigTlvSet(const uint8_t *tlv, struct WlanConnectionInfo *info, uint8_t *ssid,
		       uint8_t *channelFlags, int bssStarted, uint32_t *pmkChanged, void **copyTo,
		       const uint8_t *value, uint16_t *status, struct WlanConnection *connection)
{
	struct WlanConnectionInfo *bss = WLAN_SSID_INFO(ssid);
	apRsnConfig_t *rsn = &info->apRsn;
	int handled = 1;
	unsigned int length;
	unsigned int type = U16(tlv);
	uint32_t zeros[4] = { 0, 0, 0, 0 };
	int wmm;
	uint8_t qosInfo;

	switch (type) {
	case WLAN_TLV_SSID:
		length = bss->ssidLength;
		if (length != U16(tlv + 2) || memcmp(ssid, value, length) != 0)
			*pmkChanged = 1;
		length = (uint8_t)U16(tlv + 2);
		bss->ssidLength = length;
		__aeabi_memcpy(ssid, value, length);
		if (bss->ssidLength < sizeof(bss->ssid))
			ssid[bss->ssidLength] = 0;
		break;
	case WLAN_TLV_RATES:
		__aeabi_memclr(info->rates, sizeof(info->rates));
		*copyTo = info->rates;
		break;
	case WLAN_TLV_HT_CAPABILITY:
		__aeabi_memcpy(&info->htCapability[2], value, 26);
		info->htOperation[3] = (info->htOperation[3] & ~4) | (((info->htCapability[2] >> 1) & 1) << 2);
		if (info->htCapability[2] & 2) {
			if ((info->htOperation[3] & 3) == 0 && (channelFlags[0] & 3) == 0) {
				channelFlags[0] |= 0x30;
				wlanSetSecondaryChannel(channelFlags, channelFlags[1], info->htOperation, 0);
			}
		} else {
			channelFlags[0] &= 0xcf;
			info->htOperation[3] = (info->htOperation[3] & ~3) | ((channelFlags[0] >> 4) & 3);
		}
		info->htEnabled = info->htCapability[5] != 0 || (info->htCapability[9] & 2) != 0;
		break;
	case WLAN_TLV_VENDOR:
		if (wlanWmmElementType(value) != 2)
			break;
		wmm = memcmp(value + 8, zeros, sizeof(zeros)) != 0;
		if (bssStarted == 1 &&
		    (info->qosEnabled != (uint32_t)wmm || (uint32_t)(value[6] >> 7) != info->uapsdEnabled)) {
			*status = WLAN_AP_CONFIG_RESTART;
			return 1;
		}
		info->qosEnabled = wmm;
		if ((value[6] >> 7) != 0 && wmm != 0)
			qosInfo = info->wmmElement[8] | 0x80;
		else
			qosInfo = info->wmmElement[8] & 0x7f;
		info->wmmElement[8] = qosInfo;
		info->uapsdEnabled = qosInfo >> 7;
		if (wmm == 1 && memcmp(info->acParameters, value + 8, 16) != 0) {
			__aeabi_memcpy(info->acParameters, value + 8, 16);
			info->apQosInfo.parameterSetCount++;
			if (bssStarted == 1) {
				wlanSqramFrameHook(connection);
				wlanEdcaApplyHook(connection);
			}
		}
		break;
	case WLAN_TLV_AUTH_TYPE:
		info->authAlgorithm = value[0];
		break;
	case WLAN_TLV_CHANNEL_CONFIG:
		channelFlags[0] = value[0];
		info->htOperation[2] = channelFlags[1] = value[1];
		if ((channelFlags[0] >> 4) & 3) {
			info->htOperation[3] = (info->htOperation[3] & ~3) | ((channelFlags[0] >> 4) & 3);
			info->htCapability[2] |= 2;
			info->htOperation[3] = (info->htOperation[3] & ~4) | ((info->htCapability[2] & 2) ? 4 : 0);
		} else if (info->htCapability[2] & 2) {
			channelFlags[0] = (channelFlags[0] & 0xcf) | ((info->htOperation[3] << 4) & 0x30);
			wlanSetSecondaryChannel(channelFlags, channelFlags[1], info->htOperation, 0);
		}
		break;
	case WLAN_TLV_BEACON_PERIOD:
		*copyTo = &bss->beaconPeriod;
		break;
	case WLAN_TLV_DTIM_PERIOD:
		bss->dtimPeriod = value[0];
		break;
	case WLAN_TLV_BCAST_SSID_CTL:
		info->protectionFlags = (info->protectionFlags & ~2) | ((value[0] << 1) & 3);
		break;
	case WLAN_TLV_PREAMBLE_CTL:
		*status = WLAN_AP_CONFIG_UNSUPPORTED;
		break;
	case WLAN_TLV_PKT_FWD_CTL:
		info->pktFwdCtlConfigured = value[0];
		break;
	case WLAN_TLV_MAC_FILTER:
		wlanSqramMacFilterSetHook(&info->macFilterPolicy, &info->macFilterCount,
					  info->macFilterList, value);
		break;
	case WLAN_TLV_STA_AGEOUT:
		*copyTo = &info->staAgeout;
		break;
	case WLAN_TLV_WEP_KEY: {
		WlanWepHostConfig *wep;
		int keyType;

		length = U16(tlv + 2);
		if (length == 7) {
			keyType = 1;
		} else if (length == 15) {
			keyType = 2;
		} else {
			*status = WLAN_AP_CONFIG_RESTART;
			break;
		}
		wep = (WlanWepHostConfig *)info->bssKey->keyData;
		wlanWepHostKeySet(wep, &wep->keys, value, keyType);
		break;
	}
	case WLAN_TLV_PASSPHRASE:
		if (rsn->PSKPassPhraseLen == U16(tlv + 2) &&
		    memcmp(rsn->PSKPassPhrase, value, rsn->PSKPassPhraseLen) == 0)
			break;
		*pmkChanged = 1;
		rsn->PSKPassPhraseLen = U16(tlv + 2);
		__aeabi_memclr4(rsn->PSKPassPhrase, sizeof(rsn->PSKPassPhrase));
		*copyTo = rsn->PSKPassPhrase;
		break;
	case WLAN_TLV_ENCRYPT_PROTOCOL:
		*copyTo = &info->securityMode;
		break;
	case WLAN_TLV_AUTH_KEY:
		rsn->AuthKey = value[0] | value[1] << 8;
		rsn->AuthKeyCount = util_CountBitsSet(rsn->AuthKey);
		break;
	case WLAN_TLV_WPA_CIPHER:
		*(uint8_t *)&rsn->wpaUcstCipher = value[0];
		rsn->wpaUcstCipherCount = util_CountBitsSet(value[0]);
		*(uint8_t *)&rsn->wpa2UcstCipher = *(uint8_t *)&rsn->wpaUcstCipher;
		rsn->wpa2UcstCipherCount = rsn->wpaUcstCipherCount;
		*(uint8_t *)&rsn->mcstCipher = value[1];
		rsn->mcstCipherCount = util_CountBitsSet(value[1]);
		break;
	case WLAN_TLV_GRP_REKEY_TIME:
		*copyTo = &rsn->GrpReKeyTime;
		break;
	case WLAN_TLV_MAX_STA_COUNT:
		info->maxStations = value[0];
		break;
	case WLAN_TLV_TX_DATA_RATE:
		info->txRate = value[0];
		break;
	case WLAN_TLV_BSS_READY:
		info->ready = value[0] != 0;
		break;
	case WLAN_TLV_PWK_HSK_TIMEOUT:
		*copyTo = &rsn->PwsHskTimeOut;
		break;
	case WLAN_TLV_PWK_HSK_RETRIES:
		rsn->MaxPwsHskRetries = value[0];
		break;
	case WLAN_TLV_GWK_HSK_TIMEOUT:
		*copyTo = &rsn->GrpHskTimeOut;
		break;
	case WLAN_TLV_GWK_HSK_RETRIES:
		rsn->MaxGrpHskRetries = value[0];
		break;
	case WLAN_TLV_PS_STA_AGEOUT:
		*copyTo = &info->psStaAgeout;
		break;
	case WLAN_TLV_UCAST_CIPHER: {
		unsigned int protocol = U16(value);
		uint8_t cipher = value[2];

		if (protocol & WLAN_PROTOCOL_WPA) {
			*(uint8_t *)&rsn->wpaUcstCipher = cipher;
			rsn->wpaUcstCipherCount = util_CountBitsSet(value[2]);
		}
		if (protocol & WLAN_PROTOCOL_WPA2) {
			*(uint8_t *)&rsn->wpa2UcstCipher = cipher;
			rsn->wpa2UcstCipherCount = util_CountBitsSet(value[2]);
		}
		break;
	}
	case WLAN_TLV_MCAST_CIPHER:
		*(uint8_t *)&rsn->mcstCipher = value[0];
		rsn->mcstCipherCount = util_CountBitsSet(value[0]);
		break;
	default:
		handled = 0;
		*copyTo = NULL;
		break;
	}
	return handled;
}

/*
 * 0x0002afc4: where a HostCmd 0xb0 read finds a TLV's value: the value's
 * pieces in addresses[2..4] with their lengths in lengths[2..4] (the caller
 * gathers them behind the TLV's header), its length in *length. A value
 * that is not stored as it is sent is put together in one of the caller's
 * scratch buffers; the WEP key and MAC filter TLVs are written to tlv
 * directly. Each antenna and WEP key TLV takes the next of the ones asked
 * for out of *antennas and *wepKeys. Gives back 0 for a TLV it does not know.
 * The ucast cipher TLV's protocol is given back in a local of this
 * function, which the caller reads after it returned (the original's).
 */
int wlanApConfigTlvGet(unsigned int type, void **addresses, uint8_t *lengths,
		       struct WlanConnectionInfo *info, uint8_t *ssid, uint8_t *channelFlags,
		       uint16_t *length, uint8_t *tlv, int shortPreamble, int bssStarted,
		       uint32_t *antennas, uint32_t *wepKeys, unsigned int hookValue,
		       uint8_t *scratchFlag, uint8_t *scratchByte, uint16_t *scratchHalf,
		       uint32_t *scratchWord, uint8_t *htScratch, uint8_t *wmmScratch,
		       uint8_t *protocols)
{
	struct WlanConnectionInfo *bss = WLAN_SSID_INFO(ssid);
	apRsnConfig_t *rsn = &info->apRsn;
	int handled = 1;
	uint16_t protocol;

	switch (type) {
	case WLAN_TLV_SSID:
		*length = bss->ssidLength;
		addresses[2] = ssid;
		break;
	case WLAN_TLV_RATES:
		*length = romStringLength(info->rates, sizeof(info->rates));
		addresses[2] = info->rates;
		break;
	case WLAN_TLV_HT_CAPABILITY:
		*length = 26;
		addresses[2] = &info->htCapability[2];
		if (info->htEnabled == 1)
			break;
		/* HT off: the capability without its MCS set */
		lengths[2] = 3;
		__aeabi_memclr(htScratch, 16);
		addresses[3] = htScratch;
		lengths[3] = 16;
		addresses[4] = &info->htCapability[21];
		lengths[4] = 7;
		return handled;
	case WLAN_TLV_VENDOR:
		*length = 24;
		addresses[2] = &info->wmmElement[2];
		if (info->qosEnabled == 1)
			break;
		/* WMM off: the element without its parameter records */
		__aeabi_memclr(wmmScratch, 16);
		lengths[2] = *length - 16;
		addresses[3] = wmmScratch;
		lengths[3] = 16;
		return handled;
	case WLAN_TLV_CHANNEL_LIST:
		*length = sizeof(info->channels[0]) * info->channelCount;
		addresses[2] = info->channels;
		break;
	case WLAN_TLV_AUTH_TYPE:
		*length = 1;
		addresses[2] = &info->authAlgorithm;
		break;
	case WLAN_TLV_CHANNEL_CONFIG:
		*length = 2;
		addresses[2] = &channelFlags[0];
		lengths[2] = 1;
		addresses[3] = &channelFlags[1];
		lengths[3] = 1;
		return handled;
	case WLAN_TLV_MAC_ADDRESS:
		*length = 6;
		addresses[2] = bss->bssid;
		break;
	case WLAN_TLV_BEACON_PERIOD:
		*length = 2;
		addresses[2] = &bss->beaconPeriod;
		break;
	case WLAN_TLV_DTIM_PERIOD:
		*length = 1;
		addresses[2] = &bss->dtimPeriod;
		break;
	case WLAN_TLV_BCAST_SSID_CTL:
		*length = 1;
		*scratchByte = (info->protectionFlags >> 1) & 1;
		addresses[2] = scratchByte;
		break;
	case WLAN_TLV_PREAMBLE_CTL:
		*length = 1;
		*scratchByte = shortPreamble ? 1 : 2;
		addresses[2] = scratchByte;
		break;
	case WLAN_TLV_ANTENNA_CTL:
		*length = 2;
		if ((*scratchFlag = *antennas & 1) != 0)
			*scratchByte = info->antenna.tx;
		else
			*scratchByte = info->antenna.rx;
		addresses[2] = scratchFlag;
		lengths[2] = 1;
		addresses[3] = scratchByte;
		lengths[3] = 1;
		*antennas >>= 1;
		return handled;
	case WLAN_TLV_RTS_THRESHOLD:
		*length = 2;
		addresses[2] = &bss->rtsThreshold;
		break;
	case WLAN_TLV_0134:
		*length = 1;
		*scratchByte = (info->protectionFlags >> 2) & 1;
		addresses[2] = scratchByte;
		break;
	case WLAN_TLV_UNICAST_RATE:
		*length = 2;
		*scratchHalf = info->unicastRate;
		addresses[2] = scratchHalf;
		break;
	case WLAN_TLV_PKT_FWD_CTL:
		*length = 1;
		*scratchByte = bssStarted ? info->pktFwdCtl : info->pktFwdCtlConfigured;
		addresses[2] = scratchByte;
		break;
	case WLAN_TLV_STA_INFO:
	case WLAN_TLV_013a:
		return 0;
	case WLAN_TLV_MAC_FILTER:
		wlanSqramMacFilterGetHook(info->macFilterPolicy, info->macFilterCount,
					  info->macFilterList, tlv + 4);
		*length = 6 * tlv[5] + 2;
		lengths[2] = *length;
		return handled;
	case WLAN_TLV_STA_AGEOUT:
		*length = 4;
		addresses[2] = &info->staAgeout;
		break;
	case WLAN_TLV_WEP_KEY: {
		WlanWepHostConfig *wep = (WlanWepHostConfig *)info->bssKey->keyData;
		WlanWepHostKeys *keys = &wep->keys;
		unsigned int index = *wepKeys & 3;
		WlanWepHostKey *entry;
		unsigned int size;

		*wepKeys >>= 2;
		entry = &keys->key[index];
		size = entry->isWep104 == 1 ? 13 : 5;
		tlv[4] = index;
		tlv[5] = keys->defaultKey == index;
		__aeabi_memcpy(tlv + 6, entry->key, size);
		*length = size + 2;
		lengths[2] = size + 2;
		return handled;
	}
	case WLAN_TLV_PASSPHRASE:
		*length = rsn->PSKPassPhraseLen;
		addresses[2] = rsn->PSKPassPhrase;
		break;
	case WLAN_TLV_ENCRYPT_PROTOCOL:
		*length = 2;
		addresses[2] = &info->securityMode;
		break;
	case WLAN_TLV_AUTH_KEY:
		*length = 2;
		addresses[2] = &rsn->AuthKey;
		break;
	case WLAN_TLV_WPA_CIPHER:
		*length = 2;
		addresses[2] = &rsn->wpaUcstCipher;
		lengths[2] = 1;
		addresses[3] = &rsn->mcstCipher;
		lengths[3] = 1;
		return handled;
	case WLAN_TLV_FRAG_THRESHOLD:
		*length = 2;
		addresses[2] = &bss->fragThreshold;
		break;
	case WLAN_TLV_GRP_REKEY_TIME:
		*length = 4;
		addresses[2] = &rsn->GrpReKeyTime;
		break;
	case WLAN_TLV_MAX_STA_COUNT:
		*length = 2;
		*scratchHalf = info->maxStations;
		addresses[2] = scratchHalf;
		break;
	case WLAN_TLV_RETRY_LIMIT:
		*length = 1;
		addresses[2] = &bss->shortRetryLimit;
		break;
	case WLAN_TLV_TX_DATA_RATE:
		*length = 2;
		*scratchHalf = info->txRate;
		addresses[2] = scratchHalf;
		break;
	case WLAN_TLV_BSS_READY:
		*length = 1;
		addresses[2] = &info->ready;
		break;
	case WLAN_TLV_MGMT_IE:
		*length = wlanApConfigTlvGetHook(tlv + 4, hookValue);
		lengths[2] = *length;
		return handled;
	case WLAN_TLV_PWK_HSK_TIMEOUT:
		*length = 4;
		addresses[2] = &rsn->PwsHskTimeOut;
		break;
	case WLAN_TLV_PWK_HSK_RETRIES:
		*length = 4;
		*scratchWord = rsn->MaxPwsHskRetries;
		addresses[2] = scratchWord;
		break;
	case WLAN_TLV_GWK_HSK_TIMEOUT:
		*length = 4;
		addresses[2] = &rsn->GrpHskTimeOut;
		break;
	case WLAN_TLV_GWK_HSK_RETRIES:
		*length = 4;
		*scratchWord = rsn->MaxGrpHskRetries;
		addresses[2] = scratchWord;
		break;
	case WLAN_TLV_PS_STA_AGEOUT:
		*length = 4;
		addresses[2] = &info->psStaAgeout;
		break;
	case WLAN_TLV_UCAST_CIPHER:
		protocol = 0;
		*length = 4;
		if (*protocols & WLAN_PROTOCOL_WPA) {
			protocol |= WLAN_PROTOCOL_WPA;
			*protocols &= ~WLAN_PROTOCOL_WPA;
			addresses[3] = &rsn->wpaUcstCipher;
		} else if (*protocols & WLAN_PROTOCOL_WPA2) {
			protocol |= WLAN_PROTOCOL_WPA2;
			*protocols &= ~WLAN_PROTOCOL_WPA2;
			addresses[3] = &rsn->wpa2UcstCipher;
		}
		addresses[2] = &protocol;
		lengths[2] = 2;
		lengths[3] = 1;
		*scratchByte = 0;
		addresses[4] = scratchByte;
		lengths[4] = 1;
		return handled;
	case WLAN_TLV_MCAST_CIPHER:
		*length = 2;
		addresses[2] = &rsn->mcstCipher;
		lengths[2] = 1;
		*scratchByte = 0;
		addresses[3] = scratchByte;
		lengths[3] = 1;
		return handled;
	default:
		return 0;
	}
	lengths[2] = *length;
	return handled;
}

/*
 * 0x00037eb8: HostCmd 0xb0 (UAP_SYS_CONFIGURE), set: applies the command's
 * TLVs to the BSS. While the BSS runs only the TLVs of
 * wlanApConfigRunningTlvs are taken; the settings every connection of the
 * BSS has a copy of are set in each. Gives back the command's result: 0, or
 * 1 or 2 when a TLV was refused (the others are applied all the same).
 */
uint16_t hostCmd00b0Set(struct WlanConnection *connection, uint8_t *command)
{
	struct WlanConnectionRole *role;
	struct WlanConnectionInfo *info;
	struct WlanConnection *peer;
	const uint8_t *tlv;
	const uint8_t *value;
	unsigned int remaining;
	unsigned int length;
	uint16_t status = 0;
	uint16_t running[12];
	int count = 12;
	void *copyTo;
	unsigned int i;
	int k;

	__aeabi_memcpy4(running, wlanApConfigRunningTlvs, sizeof(running));
	role = wlanConnectionInfoSlot(connection);
	info = wlanConnectionInfo(connection);
	remaining = U16(command + 2) - 10;
	tlv = command + 10;
	while (remaining >= 4) {
		remaining -= 4;
		copyTo = NULL;
		length = U16(tlv + 2);
		value = tlv + 4;
		if (length > remaining)
			break;
		if (info->bssStarted) {
			for (k = 0; k < count; k++)
				if (U16(tlv) == running[k])
					break;
			if (k == count) {
				status = WLAN_AP_CONFIG_RESTART;
				goto next;
			}
		}
		switch (U16(tlv)) {
		case WLAN_TLV_UNICAST_RATE:
			info->unicastRate = value[0];
			if (value[0] != 0)
				connection->dataRate = romRateIndex(value[0]);
			else
				connection->dataRate = 0xfe;
			peer = connection;
			while ((peer = wlanConnectionNextByBss(peer, peer->bssType, peer->bssNumber)) != NULL)
				peer->dataRate = connection->dataRate;
			break;
		case WLAN_TLV_TX_POWER:
			wlanTxPowerSetHostLimit(value[0], 0);
			macMgmtResolveHeader(connection, value[0]);
			break;
		case WLAN_TLV_HT_OPERATION:
			status = WLAN_AP_CONFIG_RESTART;
			break;
		case WLAN_TLV_CHANNEL_LIST:
			info->channelCount = length / sizeof(info->channels[0]);
			__aeabi_memcpy(info->channels, value, U16(tlv + 2));
			for (i = 0; i < info->channelCount; i++) {
				struct WlanApChannel *channel = &info->channels[i];
				unsigned int time;

				channel->channelFlags &= 0xf3;
				channel->scanFlags |= 3;
				time = WLAN_AP_SCAN_TOTAL / info->channelCount;
				if (time > WLAN_AP_SCAN_TIME_MAX)
					time = WLAN_AP_SCAN_TIME_MAX;
				PUT_U16(channel->scanTime, time);
				time = WLAN_AP_SCAN_TOTAL / info->channelCount;
				PUT_U16(channel->measureTime, time);
			}
			break;
		case WLAN_TLV_MAC_ADDRESS:
			__aeabi_memcpy(connection->interfaceAddress, value, 6);
			__aeabi_memcpy(info->bssid, value, 6);
			if (macTxNotifyAcquireBuffer(2, connection->interfaceAddress,
						     connection->interfaceIndex,
						     connection->interfaceType != WLAN_IF_STATION) != 0)
				status = WLAN_AP_CONFIG_RESTART;
			break;
		case WLAN_TLV_012e:
		case WLAN_TLV_BSS_STATUS:
			status = WLAN_AP_CONFIG_UNSUPPORTED;
			break;
		case WLAN_TLV_ANTENNA_CTL:
			if (value[0] == 0)
				info->antenna.rx = value[1];
			else
				info->antenna.tx = value[1];
			break;
		case WLAN_TLV_RTS_THRESHOLD:
			copyTo = &info->rtsThreshold;
			peer = connection;
			do {
				peer->rtsThreshold[0] = value[0];
				peer->rtsThreshold[1] = value[1];
				peer = wlanConnectionNextByBss(peer, peer->bssType, peer->bssNumber);
			} while (peer != NULL);
			break;
		case WLAN_TLV_0134:
			info->protectionFlags = (info->protectionFlags & 0xfb) | ((value[0] << 2) & 7);
			break;
		case WLAN_TLV_AUTH_KEY:
			info->apRsn.AuthKey = value[0] | value[1] << 8;
			info->apRsn.AuthKeyCount = util_CountBitsSet(info->apRsn.AuthKey);
			if (U16(tlv + 2) > 2) {
				((uint8_t *)&info->apRsn.Akmp)[0] = value[2];
				((uint8_t *)&info->apRsn.Akmp)[1] = value[3];
			}
			break;
		case WLAN_TLV_FRAG_THRESHOLD:
			copyTo = &info->fragThreshold;
			peer = connection;
			do {
				peer->fragThreshold[0] = value[0];
				peer->fragThreshold[1] = value[1];
				peer = wlanConnectionNextByBss(peer, peer->bssType, peer->bssNumber);
			} while (peer != NULL);
			break;
		case WLAN_TLV_RETRY_LIMIT:
			info->shortRetryLimit = value[0];
			info->longRetryLimit = value[0];
			peer = connection;
			do {
				peer->shortRetryLimit = value[0];
				peer->longRetryLimit = value[0];
				peer = wlanConnectionNextByBss(peer, peer->bssType, peer->bssNumber);
			} while (peer != NULL);
			break;
		case WLAN_TLV_WAPI:
			if (fmCoexVerifyBuffer(info, value) != 0)
				status = WLAN_AP_CONFIG_RESTART;
			break;
		case WLAN_TLV_MGMT_IE:
			if (wlanCustomIeSet(connection, value, (int16_t)U16(tlv + 2)) != 0) {
				status = WLAN_AP_CONFIG_RESTART;
				break;
			}
			if (connection->interfaceType == WLAN_IF_AP && U16(value + 2) == 0xffff) {
				macMgmtScheduleFrame(connection, 5);
				macMgmtScheduleFrame(connection, 8);
			}
			break;
		case WLAN_TLV_MGMT_FORWARD_MASK:
			__aeabi_memcpy(&info->mgmtForwardMask, value, 4);
			break;
		case WLAN_TLV_HT2040_MANAGED:
			info->ht2040Managed = value[0];
			break;
		default:
			wlanApConfigTlvSet(tlv, info, info->ssid, &role->channelFlags, info->bssStarted,
					   &info->updatePassPhrase, &copyTo, value, &status, connection);
			break;
		}
		if (copyTo != NULL)
			__aeabi_memcpy(copyTo, value, U16(tlv + 2));
next:
		length = U16(tlv + 2);
		remaining -= length;
		tlv += length + 4;
	}
	keyApi_ApReleaseGroupKey(info);
	return status;
}

/*
 * 0x000382b8: HostCmd 0xb0 (UAP_SYS_CONFIGURE), get: writes the TLVs the
 * command asks for (all of them when it has none) over the command's own,
 * each as its type, length and the pieces wlanApConfigTlvGet() finds, and
 * grows the command's size by each. Gives back 0.
 * The original reads the custom IE TLV's count from the TLV it is about to
 * overwrite, and passes the last requested TLV's length to
 * wlanApConfigTlvGet() even when there was none (from address 2).
 */
int hostCmd00b0Get(struct WlanConnection *connection, uint8_t *command)
{
	const uint8_t *last = NULL;
	uint16_t types[48];
	int result = 0;
	void *addresses[5];
	uint8_t lengths[8] = { 0 };
	int all = 1;
	uint32_t unused[2] = { 0, 0 };
	struct WlanConnectionRole *role;
	uint16_t authKeyLength = WLAN_AP_CONFIG_AUTH_KEY_LENGTH;
	struct WlanConnectionInfo *info;
	int count = 48;
	uint32_t antennas = WLAN_AP_CONFIG_ANTENNAS;
	uint32_t wepKeys = WLAN_AP_CONFIG_WEP_KEYS;
	uint8_t protocols = WLAN_PROTOCOL_WPA | WLAN_PROTOCOL_WPA2;
	uint8_t *cursor;
	uint16_t length;
	struct {
		uint16_t AuthKey;
		Operation_t Akmp;
	} authKey;
	uint8_t wapi[24];
	uint8_t scratchFlag;
	uint8_t scratchByte;
	uint16_t scratchHalf;
	uint32_t scratchWord;
	uint8_t htScratch[16];
	uint8_t wmmScratch[16];
	int i;
	unsigned int j;

	__aeabi_memcpy4(types, wlanApConfigAllTlvs, sizeof(types));
	__aeabi_memclr4(addresses, sizeof(addresses[0]) * 5);
	role = wlanConnectionInfoSlot(connection);
	info = wlanConnectionInfo(connection);
	cursor = command + 10;
	if (U16(command + 2) != 10) {
		count = 0;
		all = 0;
		last = wlanApConfigRequestParse(U16(command + 2) - 10, command + 10, types, &count,
						&antennas, &wepKeys, unused, &authKeyLength);
	}
	PUT_U16(command + 2, 10);
	for (i = 0; i < count; i++) {
		addresses[0] = &types[i];
		lengths[0] = 2;
		lengths[1] = 2;
		addresses[1] = &length;
		for (j = 2; j < 5; j++) {
			addresses[j] = NULL;
			lengths[j] = 0;
		}
		length = 0;
		switch (types[i]) {
		case WLAN_TLV_MGMT_IE:
			length = wlanCustomIeGet(connection, cursor + 4,
						 all == 1 ? 0 : (uint8_t)*(uint16_t *)(cursor + 2));
			addresses[1] = NULL;
			addresses[0] = NULL;
			lengths[1] = 0;
			lengths[0] = 0;
			PUT_U16(cursor, WLAN_TLV_MGMT_IE);
			PUT_U16(cursor + 2, length);
			cursor += length + 4;
			break;
		case WLAN_TLV_MGMT_FORWARD_MASK:
			length = 4;
			lengths[2] = 4;
			addresses[2] = &info->mgmtForwardMask;
			break;
		case WLAN_TLV_BSS_STATUS:
			length = 2;
			lengths[2] = 2;
			scratchHalf = wlanConnectionBssStarted(connection);
			addresses[2] = &scratchHalf;
			break;
		case WLAN_TLV_HT2040_MANAGED:
			length = 1;
			lengths[2] = 1;
			addresses[2] = &info->ht2040Managed;
			break;
		case WLAN_TLV_HT_OPERATION:
			length = 22;
			lengths[2] = 22;
			addresses[2] = &info->htOperation[2];
			break;
		case WLAN_TLV_TX_POWER:
			length = 1;
			scratchByte = macMgmtApplyHeader();
			addresses[2] = &scratchByte;
			lengths[2] = length;
			break;
		case WLAN_TLV_AUTH_KEY:
			length = authKeyLength;
			authKey.AuthKey = info->apRsn.AuthKey;
			authKey.Akmp = info->apRsn.Akmp;
			addresses[2] = &authKey;
			lengths[2] = authKeyLength;
			break;
		case WLAN_TLV_WAPI: {
			unsigned int size = fmCoexSetupBuffer(info, wapi, sizeof(wapi));

			lengths[2] = size;
			if (size != 0)
				addresses[2] = wapi;
			length = size;
			break;
		}
		default:
			/* the original reads last's length even when last is NULL */
			wlanApConfigTlvGet(types[i], addresses, lengths, info, info->ssid,
					   &role->channelFlags, &length, cursor,
					   (info->capability[0] >> 5) & 1, info->bssStarted, &antennas,
					   &wepKeys, U16(last + 2), &scratchFlag, &scratchByte,
					   &scratchHalf, &scratchWord, htScratch, wmmScratch, &protocols);
			break;
		}
		for (j = 0; j < 5; j++) {
			if (addresses[j] != NULL) {
				__aeabi_memcpy(cursor, addresses[j], lengths[j]);
				addresses[j] = NULL;
			}
			if (lengths[j] != 0) {
				cursor += lengths[j];
				lengths[j] = 0;
			}
		}
		PUT_U16(command + 2, U16(command + 2) + length + 4);
	}
	return result;
}
