/*
 * telephony_plugin.cpp: PAF plugin entry points, SMS database migration,
 * attribute manager bootstrap, and exported interface table for
 * telephony_plugin (3.65-CEX: 0x8124E796..0x8124EC9B).
 */
#include <std/string.h>
#include <scetypes.h>
#include <plugin.h>
#include <kernel/processmgr.h>
#include <processmgr/processmgr.h>
#include "plugins.h"
#include "tel_internal.h"

namespace telephony {

static TelSmsManagerAttributeFactory s_attributeFactory;
static TelLogicFactory s_logicFactory;

static TelSmsLogicObserverInterface *s_smsObserver = NULL;
static TelSmsLogicApiInterface *s_smsApi = NULL;
static TelephonyModem *s_generalModem = NULL;
static TelephonyService *s_generalService = NULL;
static int s_useLegacyDbPath = 0;

/* 0x8124EBF0 */
static TelSmsLogicObserverInterface *telGetSmsObserverInterface(void)
{
	return s_smsObserver;
}

/* 0x8124EBFC */
static TelSmsLogicApiInterface *telGetSmsApiInterface(void)
{
	return s_smsApi;
}

/* 0x8124EC08 */
static TelephonyModem *telGetGeneralModemInterface(void)
{
	return s_generalModem;
}

/* 0x8124EC14 */
static TelephonyService *telGetGeneralServiceInterface(void)
{
	return s_generalService;
}

/* 0x81545648 */
static TelephonyPluginFullInterface s_telephonyPluginInterface = {
	telGetSmsObserverInterface,
	telGetSmsApiInterface,
	telGetGeneralModemInterface,
	telGetGeneralServiceInterface
};

/* 0x8124E7C8 */
static TelephonyResult telMigrateSmsDatabase(const paf::string &oldDbPath,
					     const paf::string &newDbPath)
{
	TelephonyResult res(TEL_OK);
	void *mem = telMalloc(sizeof(TelSmsDbMigrator));
	if (mem == NULL)
		return res;

	TelSmsDbMigrator *migrator = new (mem) TelSmsDbMigrator(oldDbPath, newDbPath);
	if (migrator != NULL) {
		res = migrator->MigrateIfNeeded();
		migrator->~TelSmsDbMigrator();
		telFree(migrator);
	}
	return res;
}

/* 0x8124E882 */
static int telPluginInitInternal(paf::Plugin *plugin)
{
	plugin->SetInterface(1, &s_telephonyPluginInterface);
	TelAttributeManager::Initialize(&s_attributeFactory);

	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	TelAttributeValue comExistAttr(TEL_ATTR_IS_COM_EXIST);
	TelephonyResult attrRes = attrMgr->GetAttribute(&comExistAttr);
	int comExist = attrRes.IsSuccess() ? comExistAttr.intValue : 0;

	if (comExist != 0) {
		sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
		paf::string oldDbPath("ur0:vsh/shell/telephony/telephony_sms.db");
		paf::string newDbPath("ur0:shell/telephony/telephony_sms.db");
		TelephonyResult migRes = telMigrateSmsDatabase(oldDbPath, newDbPath);
		s_useLegacyDbPath = migRes.GetCode();
		sceKernelProcessPowerUnlockForUser(SCE_KERNEL_POWER_TICK_DEFAULT);
	}

	return 0;
}

/* 0x8124EA46 */
static int telPluginStartInternal(void)
{
	TelAttributeManager *attrMgr = TelAttributeManager::GetInstance();
	TelAttributeValue comExistAttr(TEL_ATTR_IS_COM_EXIST);
	TelephonyResult attrRes = attrMgr->GetAttribute(&comExistAttr);
	int comExist = attrRes.IsSuccess() ? comExistAttr.intValue : 0;

	int factoryMode = TEL_FACTORY_MODE_MINI;
	if (comExist == 2)
		factoryMode = TEL_FACTORY_MODE_LOOPBACK;
	else if (comExist == 1)
		factoryMode = TEL_FACTORY_MODE_BBMC;

	s_logicFactory.CreateAndStart(factoryMode,
				      &s_smsObserver,
				      &s_smsApi,
				      &s_generalModem,
				      &s_generalService,
				      s_useLegacyDbPath);
	return 0;
}

/* 0x8124EB48 */
static int telPluginStopInternal(void)
{
	s_logicFactory.StopAndDestroy();
	s_smsObserver = NULL;
	s_smsApi = NULL;
	s_generalModem = NULL;
	s_generalService = NULL;

	TelAttributeManager::Terminate();
	return 0;
}

} /* namespace telephony */

/* 0x8124E796 */
void telephonyPluginInit(paf::Plugin *plugin)
{
	telephony::telSetPluginAllocator(plugin);
	telephony::telPluginInitInternal(plugin);
}

/* 0x8124E7A6 */
void telephonyPluginStart(paf::Plugin *plugin)
{
	(void)plugin;
	telephony::telPluginStartInternal();
}

/* 0x8124E7AE */
void telephonyPluginStop(paf::Plugin *plugin)
{
	(void)plugin;
	telephony::telPluginStopInternal();
}

/* 0x8124E7B6 */
void telephonyPluginExit(paf::Plugin *plugin)
{
	(void)plugin;
}
