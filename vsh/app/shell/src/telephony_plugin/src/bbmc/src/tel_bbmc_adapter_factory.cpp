/*
 * tel_bbmc_adapter_factory.cpp: TelBbmcAdapterFactory::Create
 * (3.65-CEX: 0x813CFBAC..0x813CFC19).
 */
#include <stddef.h>
#include <scetypes.h>
#include "../tel_bbmc_adapter.h"

namespace telephony {

/* 0x813CFBAC */
TelephonyResult TelBbmcAdapterFactory::Create(int adapterKind,
					      TelBbmcAdapterInterface **outAdapter)
{
	TelephonyResult res(TEL_OK);

	if (adapterKind == TEL_BBMC_ADAPTER_HARDWARE) {
		*outAdapter = new TelBbmcAdapter();
		return res;
	}
	if (adapterKind == TEL_BBMC_ADAPTER_NONE)
		return res;
	if (adapterKind == TEL_BBMC_ADAPTER_LOOPBACK) {
		*outAdapter = new TelBbmcAdapterLoopback();
		return res;
	}

	res.SetCode(TEL_ERR_FAILED);
	return res;
}

} /* namespace telephony */
