/*
 * tel_logic_factory.cpp: instantiates, configures, starts, stops, and
 * tears down all telephony_plugin runtime components (BBMC adapter,
 * NetCtl, general/SMS mediators, DB manager, PDU transcoders, GUI)
 * (3.65-CEX: 0x812506FC..0x81251197).
 */
#include <std/new>
#include "tel_internal.h"

namespace telephony {

struct TelFactoryModeConfig {
	int mode;
	int bbmcAdapterKind;
	int dbManagerKind;
	int pduParserKind;
	int pduGeneratorKind;
	const char *description;
};

/* 0x81545668: mode configuration table */
static const TelFactoryModeConfig s_factoryModeTable[] = {
	{ TEL_FACTORY_MODE_MINI,     TEL_BBMC_ADAPTER_LOOPBACK, TEL_SMS_DBMANAGER_SQLITE,
	  TEL_SMS_FACTORY_GSM, TEL_SMS_FACTORY_GSM, "mini telephony" },
	{ TEL_FACTORY_MODE_LOOPBACK, TEL_BBMC_ADAPTER_LOOPBACK, TEL_SMS_DBMANAGER_SQLITE,
	  TEL_SMS_FACTORY_GSM, TEL_SMS_FACTORY_GSM, "local loopback" },
	{ TEL_FACTORY_MODE_BBMC,     TEL_BBMC_ADAPTER_HARDWARE, TEL_SMS_DBMANAGER_SQLITE,
	  TEL_SMS_FACTORY_GSM, TEL_SMS_FACTORY_GSM, "using BBMC Driver" }
};

/* 0x812506FC */
TelLogicFactory::TelLogicFactory()
	: m_bbmcAdapter(NULL),
	  m_dbManager(NULL),
	  m_smsMediator(NULL),
	  m_pduParser(NULL),
	  m_pduGenerator(NULL),
	  m_smsSender(NULL),
	  m_smsReceiver(NULL),
	  m_generalMediator(NULL),
	  m_netCtl(NULL),
	  m_generalGui(NULL),
	  m_generalMediatorMini(NULL),
	  m_smsMediatorMini(NULL),
	  m_smsGui(NULL)
{
}

/* 0x81250720 */
TelLogicFactory::~TelLogicFactory()
{
}

/* 0x81250744 */
TelephonyResult TelLogicFactory::StartAll()
{
	TelephonyResult res = m_bbmcAdapter->Initialize();
	res = m_generalMediator->Initialize();
	res = m_generalGui->Attach();
	res = m_dbManager->Open();
	res = m_pduParser->Initialize(TEL_SMS_TRANSCODER_GSM);
	res = m_pduGenerator->Initialize(TEL_SMS_TRANSCODER_GSM);
	res = m_smsReceiver->Initialize();
	res = m_smsSender->Initialize();
	res = m_smsMediator->Initialize();
	res = m_smsGui->Attach();
	res = m_generalMediator->StartLogic();
	return res;
}

/* 0x81250884 */
TelephonyResult TelLogicFactory::CreateMini(int mode,
					    TelSmsLogicObserverInterface **outSmsObserver,
					    TelSmsLogicApiInterface **outSmsApi,
					    TelephonyModem **outGeneralModem,
					    TelephonyService **outGeneralService)
{
	(void)mode;
	TelephonyResult res(TEL_OK);

	m_generalMediatorMini = telCreateGeneralLogicMediatorMini();
	*outGeneralModem = (m_generalMediatorMini != NULL)
		? m_generalMediatorMini->GetModemInterface() : NULL;
	*outGeneralService = (m_generalMediatorMini != NULL)
		? m_generalMediatorMini->GetServiceInterface() : NULL;

	void *smsMiniMem = telMalloc(sizeof(TelSmsLogicMediatorMini));
	m_smsMediatorMini = (smsMiniMem != NULL)
		? new (smsMiniMem) TelSmsLogicMediatorMini() : NULL;
	*outSmsObserver = m_smsMediatorMini;
	*outSmsApi = m_smsMediatorMini;

	if (m_generalMediatorMini != NULL)
		res = m_generalMediatorMini->Initialize();
	if (m_smsMediatorMini != NULL)
		res = m_smsMediatorMini->Initialize();

	return res;
}

/* 0x8125096C */
TelephonyResult TelLogicFactory::CreateAndStart(int mode,
						TelSmsLogicObserverInterface **outSmsObserver,
						TelSmsLogicApiInterface **outSmsApi,
						TelephonyModem **outGeneralModem,
						TelephonyService **outGeneralService,
						int useLegacyDbPath)
{
	const TelFactoryModeConfig *selected = NULL;
	SceSize tableCount = sizeof(s_factoryModeTable) / sizeof(s_factoryModeTable[0]);
	for (SceSize idx = 0; idx < tableCount; ++idx) {
		if (s_factoryModeTable[idx].mode == mode) {
			selected = &s_factoryModeTable[idx];
			break;
		}
	}

	if (selected == NULL || selected->mode == TEL_FACTORY_MODE_MINI) {
		return CreateMini(mode, outSmsObserver, outSmsApi,
				  outGeneralModem, outGeneralService);
	}

	TelephonyResult res = TelBbmcAdapterFactory::Create(selected->bbmcAdapterKind,
							    &m_bbmcAdapter);

	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	if (attrMgr != NULL && m_bbmcAdapter != NULL) {
		TelAttributeValue usbAttr(TEL_ATTR_USB_SETTING);
		res = attrMgr->GetAttribute(&usbAttr);
		if (res.IsSuccess()) {
			if (usbAttr.intValue != 0)
				res = m_bbmcAdapter->StartUsbExtIf();
			else
				res = m_bbmcAdapter->StopUsbExtIf();
		}

		TelAttributeValue dbgTelAttr(TEL_ATTR_DBG_LOG_TEL);
		res = attrMgr->GetAttribute(&dbgTelAttr);
		if (res.IsSuccess())
			res = m_bbmcAdapter->SetShellDebugMedia(dbgTelAttr.intValue);

		TelAttributeValue dbgBbmcAttr(TEL_ATTR_DBG_LOG_BBMC);
		res = attrMgr->GetAttribute(&dbgBbmcAttr);
		if (res.IsSuccess())
			res = m_bbmcAdapter->SetModemDebugMedia(dbgBbmcAttr.intValue);

		TelAttributeValue dbgNetAttr(TEL_ATTR_DBG_LOG_NET);
		res = attrMgr->GetAttribute(&dbgNetAttr);
		if (res.IsSuccess())
			res = m_bbmcAdapter->SetDriverDebugMedia(dbgNetAttr.intValue);
	}

	res = TelGeneralLogicNetCtlFactory::Create(m_bbmcAdapter, &m_netCtl);

	m_generalMediator = telCreateGeneralLogicMediator(m_bbmcAdapter, m_netCtl);
	*outGeneralModem = (m_generalMediator != NULL)
		? m_generalMediator->GetModemInterface() : NULL;
	*outGeneralService = (m_generalMediator != NULL)
		? m_generalMediator->GetServiceInterface() : NULL;

	void *genGuiMem = telMalloc(sizeof(TelGeneralLogicGui));
	m_generalGui = (genGuiMem != NULL)
		? new (genGuiMem) TelGeneralLogicGui(m_generalMediator) : NULL;

	paf::string dbPath = (useLegacyDbPath != 0)
		? paf::string("ur0:vsh/shell/telephony/telephony_sms.db")
		: paf::string("ur0:shell/telephony/telephony_sms.db");

	res = TelSmsDbManagerFactory::Create(selected->dbManagerKind, &m_dbManager, dbPath);
	res = TelSmsPduParserFactory::Create(selected->pduParserKind, &m_pduParser);
	res = TelSmsPduGeneratorFactory::Create(selected->pduGeneratorKind, &m_pduGenerator);

	void *receiverMem = telMalloc(sizeof(TelSmsReceiver));
	m_smsReceiver = (receiverMem != NULL)
		? new (receiverMem) TelSmsReceiver(m_bbmcAdapter, m_dbManager, m_pduParser) : NULL;

	void *senderMem = telMalloc(sizeof(TelSmsSender));
	m_smsSender = (senderMem != NULL)
		? new (senderMem) TelSmsSender(m_bbmcAdapter, m_dbManager, m_pduGenerator) : NULL;

	void *smsMedMem = telMalloc(sizeof(TelSmsLogicMediator));
	m_smsMediator = (smsMedMem != NULL)
		? new (smsMedMem) TelSmsLogicMediator(m_smsSender, m_smsReceiver,
						      m_bbmcAdapter, m_dbManager) : NULL;

	void *smsGuiMem = telMalloc(sizeof(TelSmsLogicGui));
	m_smsGui = (smsGuiMem != NULL)
		? new (smsGuiMem) TelSmsLogicGui(m_smsMediator, m_dbManager) : NULL;

	res = StartAll();

	*outSmsObserver = m_smsMediator;
	*outSmsApi = m_smsMediator;
	return res;
}

/* 0x81250F42 */
TelephonyResult TelLogicFactory::StopAll()
{
	TelephonyResult res = m_bbmcAdapter->Terminate();
	res = m_smsGui->Detach();
	res = m_smsMediator->Terminate();
	res = m_smsSender->Terminate();
	res = m_smsReceiver->Terminate();
	res = m_dbManager->Close();
	res = m_generalGui->Detach();
	res = m_generalMediator->Terminate();
	return res;
}

/* 0x8125106E */
TelephonyResult TelLogicFactory::StopAndDestroy()
{
	TelephonyResult res(TEL_OK);
	if (m_generalMediatorMini != NULL)
		return res;

	res = StopAll();

	if (m_smsSender != NULL) {
		m_smsSender->~TelSmsSender();
		telFree(m_smsSender);
		m_smsSender = NULL;
	}
	if (m_smsReceiver != NULL) {
		m_smsReceiver->~TelSmsReceiver();
		telFree(m_smsReceiver);
		m_smsReceiver = NULL;
	}
	if (m_pduGenerator != NULL) {
		delete m_pduGenerator;
		m_pduGenerator = NULL;
	}
	if (m_pduParser != NULL) {
		delete m_pduParser;
		m_pduParser = NULL;
	}
	if (m_dbManager != NULL) {
		TelSmsDbManagerFactory::Destroy(m_dbManager);
		m_dbManager = NULL;
	}
	if (m_smsGui != NULL) {
		m_smsGui->~TelSmsLogicGui();
		telFree(m_smsGui);
		m_smsGui = NULL;
	}
	if (m_smsMediator != NULL) {
		m_smsMediator->~TelSmsLogicMediator();
		telFree(m_smsMediator);
		m_smsMediator = NULL;
	}
	if (m_generalGui != NULL) {
		m_generalGui->~TelGeneralLogicGui();
		telFree(m_generalGui);
		m_generalGui = NULL;
	}
	if (m_generalMediator != NULL) {
		m_generalMediator->~TelGeneralLogicMediatorBase();
		telFree(m_generalMediator);
		m_generalMediator = NULL;
	}
	if (m_netCtl != NULL) {
		res = TelGeneralLogicNetCtlFactory::Destroy(m_netCtl);
		m_netCtl = NULL;
	}
	if (m_bbmcAdapter != NULL) {
		delete m_bbmcAdapter;
		m_bbmcAdapter = NULL;
	}

	return res;
}

} /* namespace telephony */
