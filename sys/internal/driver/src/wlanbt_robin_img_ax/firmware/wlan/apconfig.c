/*
 * The access point's configuration: HostCmd 0xb0 (Marvell's
 * UAP_SYS_CONFIGURE) sets or reads the BSS's parameters as TLVs, and the
 * STA_INFO TLV of HostCmd 0xb3 (UAP_STA_LIST). Marvell published no source
 * for it; the filename and the names are inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/apconfig.h"
#include "robin/conn.h"
#include "robin/rom.h"
#include "robin/runtime.h"

#define U16(bytes)		((uint16_t)((bytes)[0] | (bytes)[1] << 8))
#define PUT_U16(bytes, value)	((bytes)[0] = (uint8_t)(value), (bytes)[1] = (uint8_t)((value) >> 8))

/* The TLVs a read request treats specially (Marvell's TLV numbers). */
#define WLAN_TLV_ANTENNA_CTL	0x0132
#define WLAN_TLV_WEP_KEY	0x013b
#define WLAN_TLV_AUTH_KEY	0x0141
#define WLAN_TLV_UCAST_CIPHER	0x0191

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
					 int unused, uint16_t *authKeyLength)
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
