/*
 * tel_general_logic_mediator_mini.cpp: minimal general telephony mediator for
 * Wi-Fi-only / no-WWAN Vita units (3.65-CEX: 0x8125EF84..0x8125F23D).
 */
#include <std/new>
#include "tel_general_internal.h"

namespace telephony {

static const SceInt64 kMiniDownloadLimitUnlimited = 0x7FFFFFFFFFFFFFFFLL;

/* 0x8125EF84 */
TelGeneralLogicMediatorMini::TelGeneralLogicMediatorMini()
	: TelGeneralLogicMediatorBase("SceTelGenLogicMediatorMiniObsv"),
	  m_callbackMutex("SceTelGenLogicMediatorMiniCb", 0),
	  m_emptyOperatorName(L"")
{
}

/* 0x8125F030 */
TelGeneralLogicMediatorMini::~TelGeneralLogicMediatorMini()
{
	m_generalListeners.clear();
}

/* 0x8125F11E */
TelephonyResult TelGeneralLogicMediatorMini::Initialize()
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr != NULL)
		attrMgr->AddObserver(this);
	return TelephonyResult(TEL_OK);
}

/* 0x8125F136 */
TelephonyResult TelGeneralLogicMediatorMini::Terminate()
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr != NULL)
		attrMgr->RemoveObserver(this);
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelGeneralLogicMediatorMini::StartLogic()
{
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelGeneralLogicMediatorMini::GetOperatorAppTitleId(const char **outTitleId)
{
	*outTitleId = NULL;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

TelephonyModem *TelGeneralLogicMediatorMini::GetModemInterface()
{
	return reinterpret_cast<TelephonyModem *>(
		static_cast<TelGeneralLogicModemInterface *>(this));
}

TelephonyService *TelGeneralLogicMediatorMini::GetServiceInterface()
{
	return reinterpret_cast<TelephonyService *>(
		static_cast<TelGeneralLogicServiceInterface *>(this));
}

int TelGeneralLogicMediatorMini::AddGeneralListener(TelephonyGeneralListener listener,
						    void *userData)
{
	if (listener == NULL)
		return TEL_ERR_INVALID_ARG;

	m_callbackMutex.Lock();
	TelGeneralListenerRegistration reg;
	reg.callback = listener;
	reg.userData = userData;
	m_generalListeners.push_back(reg);
	m_callbackMutex.Unlock();
	return 0;
}

int TelGeneralLogicMediatorMini::RemoveGeneralListener(TelephonyGeneralListener listener,
						       void *userData)
{
	m_callbackMutex.Lock();
	for (paf::vector<TelGeneralListenerRegistration>::iterator it = m_generalListeners.begin();
	     it != m_generalListeners.end(); ++it) {
		if (it->callback == listener && it->userData == userData) {
			m_generalListeners.erase(it);
			break;
		}
	}
	m_callbackMutex.Unlock();
	return 0;
}

int TelGeneralLogicMediatorMini::GetPhoneNumberString(char *outNumber)
{
	(void)outNumber;
	return TEL_ERR_NOT_SUPPORTED;
}

int TelGeneralLogicMediatorMini::CheckContentDownloadCode(int serviceType)
{
	return CheckContentDownloadAllowed(serviceType).GetCode();
}

TelephonyResult TelGeneralLogicMediatorMini::CheckImsiForOperatorCode(int *inOutOperatorCode,
								      bool *outMatched)
{
	(void)inOutOperatorCode;
	if (outMatched != NULL)
		*outMatched = false;
	return TelephonyResult(TEL_ERR_NOT_SUPPORTED);
}

int TelGeneralLogicMediatorMini::GetCountryCode(paf::string *outCountry)
{
	(void)outCountry;
	return TEL_ERR_NOT_FOUND;
}

int TelGeneralLogicMediatorMini::GetProductTargetOperatorCode(int *outOperator)
{
	return GetProductTargetOperator(outOperator).GetCode();
}

/* 0x8125F15C */
TelephonyResult TelGeneralLogicMediatorMini::GetEnableWwan(bool *outEnabled)
{
	*outEnabled = false;
	return TelephonyResult(TEL_OK);
}

/* 0x8125F16C */
TelephonyResult TelGeneralLogicMediatorMini::GetIsComExist(int *outExists)
{
	*outExists = 0;
	return TelephonyResult(TEL_OK);
}

/* 0x8125F178 */
TelephonyResult TelGeneralLogicMediatorMini::CheckContentDownloadAllowed(int serviceType)
{
	switch (serviceType) {
	case 1:
		return TelephonyResult(TEL_OK);
	case 2:
	case 3:
	case 4:
	case 5:
		return TelephonyResult(TEL_NETCTL_ERR_DENIED);
	default:
		return TelephonyResult(TEL_NETCTL_ERR_INVALID_SERVICE);
	}
}

/* 0x8125F1B0 */
TelephonyResult TelGeneralLogicMediatorMini::GetDownloadSizeLimit(unsigned int serviceType,
								  SceInt64 *outLimitBytes)
{
	if (serviceType >= 2 && serviceType <= 5) {
		*outLimitBytes = kMiniDownloadLimitUnlimited;
		return TelephonyResult(TEL_OK);
	}
	return TelephonyResult(TEL_NETCTL_ERR_INVALID_SERVICE);
}

/* 0x81261010 */
TelephonyResult TelGeneralLogicMediatorMini::CheckRoamingDownloadAllowed(unsigned int serviceType)
{
	if (serviceType >= 2 && serviceType <= 5)
		return TelephonyResult(TEL_NETCTL_ERR_DENIED);
	return TelephonyResult(TEL_NETCTL_ERR_INVALID_SERVICE);
}

/* 0x8125F1E6 */
void TelGeneralLogicMediatorMini::OnNotify(const TelEventInfo &event)
{
	if (event.GetEventId() == TEL_EVENT_ATTRIBUTE_CHANGED) {
		(void)event.GetEventArg();
	}
}

/* Factory helper called by TelLogicFactory::CreateMini */
TelGeneralLogicMediatorBase *telCreateGeneralLogicMediatorMini()
{
	void *mem = telMalloc(sizeof(TelGeneralLogicMediatorMini));
	return (mem != NULL) ? new (mem) TelGeneralLogicMediatorMini() : NULL;
}

} /* namespace telephony */
