/*
 * tel_sms_manager_attribute_factory.cpp: creates and destroys the four
 * attribute store backends registered with TelAttributeManager
 * (3.65-CEX: 0x8124EC9C..0x8124EE23).
 */
#include <std/new>
#include "../tel_internal.h"

namespace telephony {

/* 0x8124EC9C */
TelephonyResult TelSmsManagerAttributeFactory::CreateStores(
	paf::vector<TelAttributeStore *> *stores)
{
	TelephonyResult res(TEL_OK);

	void *iniMem = telMalloc(sizeof(TelSmsManagerIniFileAttribute));
	TelAttributeStore *iniStore = (iniMem != NULL)
		? static_cast<TelAttributeStore *>(new (iniMem) TelSmsManagerIniFileAttribute())
		: NULL;
	stores->push_back(iniStore);

	void *regMem = telMalloc(sizeof(TelSmsManagerRegistoryAttribute));
	TelAttributeStore *regStore = (regMem != NULL)
		? static_cast<TelAttributeStore *>(new (regMem) TelSmsManagerRegistoryAttribute())
		: NULL;
	stores->push_back(regStore);

	void *idStorageMem = telMalloc(sizeof(TelSmsManagerIdStorageAttribute));
	TelAttributeStore *idStorageStore = (idStorageMem != NULL)
		? static_cast<TelAttributeStore *>(new (idStorageMem) TelSmsManagerIdStorageAttribute())
		: NULL;
	stores->push_back(idStorageStore);

	void *memStoreBuf = telMalloc(sizeof(TelSmsManagerMemoryAttribute));
	TelAttributeStore *memStore = (memStoreBuf != NULL)
		? static_cast<TelAttributeStore *>(new (memStoreBuf) TelSmsManagerMemoryAttribute())
		: NULL;
	stores->push_back(memStore);

	return res;
}

/* 0x8124EDCE */
TelephonyResult TelSmsManagerAttributeFactory::DestroyStores(
	paf::vector<TelAttributeStore *> *stores)
{
	TelephonyResult res(TEL_OK);

	for (SceSize idx = 0; idx < stores->size(); ++idx) {
		TelAttributeStore *store = (*stores)[idx];
		if (store != NULL) {
			store->~TelAttributeStore();
			telFree(store);
		}
	}
	stores->clear();
	return res;
}

} /* namespace telephony */
