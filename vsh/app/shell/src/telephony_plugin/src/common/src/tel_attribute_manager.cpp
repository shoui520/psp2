/*
 * tel_attribute_manager.cpp: TelObservable and TelAttributeManager singleton
 * (3.65-CEX: 0x813DCC20..0x813DD097, 0x8142B3A4..0x8142B5A0).
 */
#include <stddef.h>
#include <scetypes.h>
#include "../tel_common.h"

namespace telephony {

namespace {

static TelAttributeManager *g_attributeManager = NULL;

} /* namespace */

/* TelObservable (0x8142B3A4..0x8142B5A0) */
TelObservable::TelObservable(const char *mutexName)
	: m_observerMutex(mutexName, 0), m_notifying(false)
{
	m_observers.reserve(16);
}

TelObservable::~TelObservable()
{
}

void TelObservable::AddObserver(TelObserver *observer)
{
	m_observerMutex.Lock();
	m_observers.push_back(observer);
	m_observerMutex.Unlock();
}

void TelObservable::RemoveObserver(TelObserver *observer)
{
	m_observerMutex.Lock();
	for (paf::vector<TelObserver *>::iterator it = m_observers.begin();
	     it != m_observers.end(); ) {
		if (*it == observer)
			it = m_observers.erase(it);
		else
			++it;
	}
	m_observerMutex.Unlock();
}

void TelObservable::NotifyObservers(const TelEventInfo &event)
{
	m_observerMutex.Lock();
	m_notifying = true;
	for (SceSize idx = 0; idx < m_observers.size(); ++idx)
		m_observers[idx]->OnNotify(event);
	m_notifying = false;
	m_observerMutex.Unlock();
}

/* 0x813DCCAE */
TelAttributeManager::TelAttributeManager(TelAttributeFactory *factory)
	: TelObservable("SceTelCmnAttributeManagerObsv"), m_factory(factory)
{
}

/* 0x813DD02E */
TelAttributeManager::~TelAttributeManager()
{
}

/* 0x813DCC20 */
TelephonyResult TelAttributeManager::InitStores()
{
	TelephonyResult res = m_factory->CreateStores(&m_stores);
	if (res.GetCode() == TEL_OK) {
		for (SceSize idx = 0; idx < m_stores.size(); ++idx) {
			res = m_stores[idx]->Initialize();
			if (res.GetCode() != TEL_OK)
				break;
		}
	}
	return res;
}

/* 0x813DCD82 */
TelephonyResult TelAttributeManager::TermStores()
{
	TelephonyResult res(TEL_OK);
	for (SceSize idx = 0; idx < m_stores.size(); ++idx)
		res = m_stores[idx]->Terminate();
	res = m_factory->DestroyStores(&m_stores);
	return res;
}

/* 0x813DCD08 */
TelephonyResult TelAttributeManager::Initialize(TelAttributeFactory *factory)
{
	TelAttributeManager *mgr = new TelAttributeManager(factory);
	g_attributeManager = mgr;
	if (mgr == NULL)
		return TelephonyResult(TEL_ERR_NO_MEMORY);
	return mgr->InitStores();
}

/* 0x813DCE14 */
TelephonyResult TelAttributeManager::Terminate()
{
	if (g_attributeManager == NULL)
		return TelephonyResult(TEL_ERR_NOT_INITIALIZED);

	TelephonyResult res = g_attributeManager->TermStores();
	delete g_attributeManager;
	g_attributeManager = NULL;
	return res;
}

/* 0x813DCE8E */
TelAttributeManager *TelAttributeManager::GetInstance()
{
	return g_attributeManager;
}

/* 0x813DCE9A */
TelephonyResult TelAttributeManager::SetAttribute(const TelAttributeValue &attr)
{
	TelephonyResult res(TEL_OK);
	for (SceSize idx = 0; idx < m_stores.size(); ++idx) {
		if ((m_stores[idx]->GetMask() & attr.attributeId) != 0) {
			res = m_stores[idx]->SetAttribute(attr);
			if (res.GetCode() == TEL_OK)
				break;
		}
	}

	if (res.GetCode() != TEL_OK) {
		TelEventInfo errEvent(TEL_EVENT_ATTRIBUTE_ERROR, attr.attributeId, res.GetCode());
		NotifyObservers(errEvent);
	} else {
		TelEventInfo changeEvent(TEL_EVENT_ATTRIBUTE_CHANGED, attr.attributeId, -1);
		NotifyObservers(changeEvent);
	}
	return res;
}

/* 0x813DCF78 */
TelephonyResult TelAttributeManager::GetAttribute(TelAttributeValue *outAttr)
{
	TelephonyResult res(TEL_OK);
	for (SceSize idx = 0; idx < m_stores.size(); ++idx) {
		if ((m_stores[idx]->GetMask() & outAttr->attributeId) != 0) {
			res = m_stores[idx]->GetAttribute(outAttr);
			if (res.GetCode() == TEL_OK)
				break;
		}
	}

	if (res.GetCode() != TEL_OK) {
		TelEventInfo errEvent(TEL_EVENT_ATTRIBUTE_ERROR, outAttr->attributeId,
				      res.GetCode());
		NotifyObservers(errEvent);
	}
	return res;
}

} /* namespace telephony */
