/*
 * tel_sms_dbmanager_factory.cpp: factory create/destroy for TelSmsDbManager
 * (3.65-CEX: 0x813E8984..0x813E89E3).
 */
#include <stddef.h>
#include <scetypes.h>
#include <memory/heap_allocator.h>
#include "../tel_sms_dbmanager.h"

namespace telephony {

/* 0x813E8984 */
TelephonyResult TelSmsDbManagerFactory::Create(int managerType, TelSmsDbManager **outManager,
					       const paf::string &dbPath)
{
	if (managerType != TEL_SMS_DBMANAGER_SQLITE)
		return TelephonyResult(TEL_ERR_FAILED);

	TelSmsDbManager *instance = new (telGetAllocator()) TelSmsDbManager(dbPath);
	*outManager = instance;
	return TelephonyResult(TEL_OK);
}

/* 0x813E89CC */
TelephonyResult TelSmsDbManagerFactory::Destroy(TelSmsDbManager *manager)
{
	delete manager;
	return TelephonyResult(TEL_OK);
}

} /* namespace telephony */
