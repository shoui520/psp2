/*
 * The RAM firmware's part of the access point's association check: the
 * function the ROM code calls when an association request has passed
 * (Marvell's AssocSrvAp_InitStaKeyInfo; the ROM build leaves it to the RAM
 * firmware, which installs it at start-up).
 */
#include "robin/assocAp.h"
#include "robin/keyMgmtAp.h"
#include "robin/rom.h"
#include "robin/runtime.h"

void (*ramHook_AssocSrvAp_InitStaKeyInfo)(struct WlanConnection *connPtr, SecurityMode_t *secType,
					  Cipher_t *pwCipher, uint16_t staRsnCap,
					  uint8_t akmType) __attribute__((section(".itcm.bss")));

/*
 * The station's key state starts over (an access point that handles the
 * handshakes itself only): the handshake timer stops, the state is cleared
 * and set up for the cipher and key management the station chose.
 */
void AssocSrvAp_InitStaKeyInfo(struct WlanConnection *connPtr, SecurityMode_t *secType,
			       Cipher_t *pwCipher, uint16_t staRsnCap, uint8_t akmType)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connPtr);
	apKeyMgmtInfoSta_t *pKeyMgmtInfo = &role->keyMgmt;

	wlanConnectionInfo(connPtr);
	if (wlanInterfaceNeedsSecurity(connPtr)) {
		KeyMgmtStopHskTimer(connPtr, 0);
		__aeabi_memclr4(pKeyMgmtInfo, sizeof(apKeyMgmtInfoSta_t));
		AssocSrvAp_InitKeyMgmtInfo(pKeyMgmtInfo, secType, pwCipher, staRsnCap, akmType);
	}
}

void AssocSrvAp_InstallHooks(void)
{
	ramHook_AssocSrvAp_InitStaKeyInfo = AssocSrvAp_InitStaKeyInfo;
}
