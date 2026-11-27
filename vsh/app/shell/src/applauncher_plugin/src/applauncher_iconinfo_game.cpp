/*
 * applauncher_iconinfo_game.cpp: the list of a game's information, its items chosen by the
 * application's type, its license's period and what the system shows of upgrades
 * (0x810E005C-0x810E138C). The file's name is ours (PLACEHOLDER): no record of the 3.6x shell
 * names it.
 */

#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include <common/char_encoding.h>
#if !defined(FW_TYPE_CEX)
#include <file/localfile.h>
extern "C" int sceFsUtilGetDevInfo(const char *path, SceUInt64 *maxSize, SceUInt64 *freeSize);
#endif
#include <vshbridge/vshbridge.h>
#include "../../livespace_db/lsdb_keys.h"
#include "../../../shell.h"
#include "../../../plugins.h"
#include "applauncher.h"
#include "applauncher_home.h"
#include "applauncher_iconinfo.h"

using namespace paf;

/* PLACEHOLDER: the kinds of the items of a game's information */
enum {
	APPLAUNCHER_INFO_GAME_TITLE		= 0,
	APPLAUNCHER_INFO_GAME_SAVEDATA		= 1,
	APPLAUNCHER_INFO_GAME_PARENTAL_LEVEL	= 2,
	APPLAUNCHER_INFO_GAME_MTIME		= 3,
	APPLAUNCHER_INFO_GAME_SIZE		= 4,
	APPLAUNCHER_INFO_GAME_VERSION		= 5,
	APPLAUNCHER_INFO_GAME_START_TIME	= 6,
	APPLAUNCHER_INFO_GAME_EXPIRE_TIME	= 7,
	APPLAUNCHER_INFO_GAME_REMAINING		= 8,
	APPLAUNCHER_INFO_GAME_CHECK		= 9,
#if defined(FW_TYPE_CEX)
	APPLAUNCHER_INFO_GAME_UP_STATUS		= 10
#else
	APPLAUNCHER_INFO_GAME_TITLE_ID		= 10,
	APPLAUNCHER_INFO_GAME_CARD_RW_AREA	= 11,
	APPLAUNCHER_INFO_GAME_UP_STATUS		= 12,
	APPLAUNCHER_INFO_GAME_SAVEDATA_MODE	= 13,
	APPLAUNCHER_INFO_GAME_SAVEDATA_SIZE	= 14,
	APPLAUNCHER_INFO_GAME_SAVEDATA_SLOT	= 15
#endif
};

/* PLACEHOLDER: the items' labels */
#define APPLAUNCHER_INFO_GAME_LABEL_TITLE	0x0BB94ACA
#define APPLAUNCHER_INFO_GAME_LABEL_SAVEDATA	0xD9856323
#define APPLAUNCHER_INFO_GAME_LABEL_PARENTAL	0x672AAF09
#define APPLAUNCHER_INFO_GAME_LABEL_MTIME	0xBCD53A45
#define APPLAUNCHER_INFO_GAME_LABEL_SIZE	0x29C686EC
#define APPLAUNCHER_INFO_GAME_LABEL_VERSION	0x2A6326F2
#define APPLAUNCHER_INFO_GAME_LABEL_START	0x40D2B66C
#define APPLAUNCHER_INFO_GAME_LABEL_EXPIRE	0x1C2C0E54
#define APPLAUNCHER_INFO_GAME_LABEL_REMAINING	0x5747EC79
#if !defined(FW_TYPE_CEX)
#define APPLAUNCHER_INFO_GAME_LABEL_SD_MODE	0xC2400D6Du
#define APPLAUNCHER_INFO_GAME_LABEL_SD_SIZE	0xFCB9D3CCu
#endif

/* PLACEHOLDER: the parental level's text (its number), the remaining time of a license without
   an end, and a value not known */
#define APPLAUNCHER_INFO_GAME_MSG_PARENTAL	0x3C165504
#define APPLAUNCHER_INFO_GAME_MSG_NO_EXPIRY	0x97546463
#define APPLAUNCHER_INFO_GAME_MSG_NONE		0xAF471967

/* PLACEHOLDER: a #_up_status: a free trial, and the full game */
#define APPLAUNCHER_INFO_GAME_UP_TRIAL		1
#define APPLAUNCHER_INFO_GAME_UP_FULL		3

/* PLACEHOLDER: the storage the size item of an application of another type is given */
#define APPLAUNCHER_INFO_GAME_STORAGE_OTHER	3

/*
 * The items of a game (sceLsdbGetType 1): with the license's period, with the upgrade's status
 * where the system shows it ("Store"), with the check of #appconf ("Check").
 */
#if defined(FW_TYPE_CEX)
static unsigned char s_gameItemsPeriod[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
static unsigned char s_gameItems[] = { 0, 1, 2, 3, 4, 5 };
static unsigned char s_gameItemsPeriodStore[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10 };
static unsigned char s_gameItemsStore[] = { 0, 1, 2, 3, 4, 5, 10 };
static unsigned char s_gameItemsPeriodCheck[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
static unsigned char s_gameItemsCheck[] = { 0, 1, 2, 3, 4, 5, 9 };
static unsigned char s_gameItemsPeriodCheckStore[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
static unsigned char s_gameItemsCheckStore[] = { 0, 1, 2, 3, 4, 5, 9, 10 };

/* the items of the applications of types 2 to 5 */
static unsigned char s_type45Items[] = { 0, 2, 3, 4, 5 };
static unsigned char s_type45ItemsStore[] = { 0, 2, 3, 4, 5 };
static unsigned char s_type2ItemsPeriod[] = { 0, 2, 3, 4, 5, 6, 7, 8 };
static unsigned char s_type2Items[] = { 0, 2, 3, 4, 5 };
static unsigned char s_type3ItemsPeriod[] = { 0, 2, 3, 4, 5, 6, 7, 8 };
static unsigned char s_type3Items[] = { 0, 2, 3, 4, 5 };
#else
static unsigned char s_gameItemsPeriod[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15 };
static unsigned char s_gameItems[] = { 0, 1, 2, 3, 4, 5, 10, 11, 12, 13, 14, 15 };
static unsigned char s_gameItemsPeriodStore[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15 };
static unsigned char s_gameItemsStore[] = { 0, 1, 2, 3, 4, 5, 10, 11, 12, 13, 14, 15 };
static unsigned char s_gameItemsPeriodCheck[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
static unsigned char s_gameItemsCheck[] = { 0, 1, 2, 3, 4, 5, 9, 10, 11, 12, 13, 14, 15 };
static unsigned char s_gameItemsPeriodCheckStore[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
static unsigned char s_gameItemsCheckStore[] = { 0, 1, 2, 3, 4, 5, 9, 10, 11, 12, 13, 14, 15 };

static unsigned char s_type45Items[] = { 0, 3, 4, 5, 10 };
static unsigned char s_type45ItemsStore[] = { 0, 2, 3, 4, 5, 10 };
static unsigned char s_type2ItemsPeriod[] = { 0, 2, 3, 4, 5, 6, 7, 8, 10 };
static unsigned char s_type2Items[] = { 0, 2, 3, 4, 5, 10 };
static unsigned char s_type3ItemsPeriod[] = { 0, 2, 3, 4, 5, 6, 7, 8, 10 };
static unsigned char s_type3Items[] = { 0, 2, 3, 4, 5, 10 };
#endif

/* whether the system shows what may be upgraded */
static inline bool applauncherIconInfoShowsUpgrade(void)
{
	return vshSblQafMgrIsAllowShowTitleUpgradeInfo() != 0 || vshSblUtMgrHasStoreFlag() != 0;
}

/* the license has a period: its end, or a period it was bought for */
static inline bool applauncherIconInfoHasPeriod(const GameLicenseStatus *license)
{
	return license->valid && (license->hasTimedLicense || license->expireTime != 0x7FFFFFFFFFFFFFFFull);
}

/* (0x810E005C) */
ApplauncherIconInfoGameFactory::ApplauncherIconInfoGameFactory(Plugin *plugin,
	ui::Widget *view, const common::SharedPtr<ApplauncherIcon> &icon, LsdbAppInfoPtr *appInfo,
	GameLicenseStatus *license, GameFttInfo *ftt, bool hasCheck, bool hasFtt)
	: ApplauncherIconInfoFactory(plugin, icon, appInfo), m_view(view), m_unusedAppSize(0),
	  m_unusedStorage(0), m_license(license), m_ftt(ftt), m_startTime(0),
	  m_expireTime(0x7FFFFFFFFFFFFFFFull)
{
	switch (sceLsdbGetType(appInfo)) {
	case 1:
		if (hasFtt) {
			m_startTime = ftt->startTime;
			m_expireTime = ftt->expireTime;
		} else if (applauncherIconInfoHasPeriod(m_license)) {
			m_startTime = license->startTime;
			m_expireTime = license->expireTime;
		} else {
			if (applauncherIconInfoShowsUpgrade()) {
				if (hasCheck) {
					m_items = s_gameItemsCheckStore;
					m_itemCount = sizeof(s_gameItemsCheckStore);
				} else {
					m_items = s_gameItemsStore;
					m_itemCount = sizeof(s_gameItemsStore);
				}
			} else if (hasCheck) {
				m_items = s_gameItemsCheck;
				m_itemCount = sizeof(s_gameItemsCheck);
			} else {
				m_items = s_gameItems;
				m_itemCount = sizeof(s_gameItems);
			}
			break;
		}

		if (applauncherIconInfoShowsUpgrade()) {
			if (hasCheck) {
				m_items = s_gameItemsPeriodCheckStore;
				m_itemCount = sizeof(s_gameItemsPeriodCheckStore);
			} else {
				m_items = s_gameItemsPeriodStore;
				m_itemCount = sizeof(s_gameItemsPeriodStore);
			}
		} else if (hasCheck) {
			m_items = s_gameItemsPeriodCheck;
			m_itemCount = sizeof(s_gameItemsPeriodCheck);
		} else {
			m_items = s_gameItemsPeriod;
			m_itemCount = sizeof(s_gameItemsPeriod);
		}
		break;
	case 2:
		if (applauncherIconInfoHasPeriod(m_license)) {
			m_startTime = license->startTime;
			m_expireTime = license->expireTime;
			m_itemCount = sizeof(s_type2ItemsPeriod);
			m_items = s_type2ItemsPeriod;
		} else {
			m_items = s_type2Items;
			m_itemCount = sizeof(s_type2Items);
		}
		break;
	case 3:
		if (applauncherIconInfoHasPeriod(m_license)) {
			m_startTime = license->startTime;
			m_expireTime = license->expireTime;
			m_itemCount = sizeof(s_type3ItemsPeriod);
			m_items = s_type3ItemsPeriod;
		} else {
			m_items = s_type3Items;
			m_itemCount = sizeof(s_type3Items);
		}
		break;
	case 4:
	case 5:
		if (applauncherIconInfoShowsUpgrade()) {
			m_items = s_type45ItemsStore;
			m_itemCount = sizeof(s_type45ItemsStore);
		} else {
			m_items = s_type45Items;
			m_itemCount = sizeof(s_type45Items);
		}
		break;
	default:
		m_itemCount = 0;
		break;
	}
#if !defined(FW_TYPE_CEX)
	sce_paf_memset(m_saveDataSlotCache, 0, sizeof(m_saveDataSlotCache));
#endif
}

/* (0x810E030A, 0x810E03F6) */
ApplauncherIconInfoGameFactory::~ApplauncherIconInfoGameFactory()
{
	if (m_job.get() != NULL) {
		m_job->Cancel();
		m_job.reset();
	}
	if (m_listView != NULL)
		m_listView = NULL;

	GamePluginInterface *game =
		(GamePluginInterface *)applauncherPluginGetInterface("game_plugin", 1);

	if (game != NULL)
		((void (*)(bool))game->func[GAME_PLUGIN_FUNC_SET_DIALOG_HOLD])(false);
}

/* the application's row, read again for each lookup; its find is the const one, out of line */
static inline const LsdbAppInfo *applauncherIconInfoRow(LsdbAppInfoPtr *appInfo)
{
	return appInfo->get();
}

/* (0x810E0406) */
ui::ListItem *ApplauncherIconInfoGameFactory::CreateItem(int type, CreateParam &param)
{
	const LsdbAppInfo *appInfo = m_appInfo->get();
	LsdbAppInfo::iterator end = appInfo->end();
	wstring value;

	switch (((const unsigned char *)m_items)[param.m_cell]) {
	case APPLAUNCHER_INFO_GAME_TITLE: {
		ApplauncherIconInfoItem *item = new ApplauncherIconInfoItem(param.m_parent, NULL);

		if (item != NULL) {
			GetTitle(&value);
			item->Set(APPLAUNCHER_INFO_GAME_LABEL_TITLE, value);
		}
		return item;
	}
	case APPLAUNCHER_INFO_GAME_SAVEDATA: {
		LsdbAppInfo::iterator it = appInfo->find(LSDB_KEY_ORG_PATH);
		if (it == end)
			return NULL;

		string path(LsdbValueText(it->second));

		it = applauncherIconInfoRow(m_appInfo)->find(LSDB_KEY_INSTALL_DIR_SAVEDATA);
		if (it == end)
			return NULL;

		string dir(LsdbValueText(it->second));
		int attr = 0;

		it = applauncherIconInfoRow(m_appInfo)->find(LSDB_KEY_ATTRIBUTE);
		if (it != end)
			attr = it->second.type == LSDB_VALUE_INTEGER ? (int)it->second.integer : 0;

		ApplauncherIconInfoSaveDataItem *item =
			new ApplauncherIconInfoSaveDataItem(path, dir, attr, param.m_parent, NULL);

		if (item != NULL)
			item->Setup(APPLAUNCHER_INFO_GAME_LABEL_SAVEDATA);
		return item;
	}
	case APPLAUNCHER_INFO_GAME_PARENTAL_LEVEL: {
		unsigned int level;

		LsdbAppInfo::iterator it = appInfo->find(LSDB_KEY_PARENTAL_LEVEL);
		if (it == end || (level = it->second.type == LSDB_VALUE_INTEGER ?
				  (unsigned int)it->second.integer : 0) == 0) {
			value = applauncherGetString(IDParam(s_empty_name,
							     APPLAUNCHER_INFO_GAME_MSG_NONE));
		} else {
			wstring text;
			char buf[64];

			sce_paf_snprintf(buf, sizeof(buf), "%lld", (SceUInt64)level);
			text = applauncherIconInfoUtf16(string(buf));
			value = shellFormatText(applauncherGetString(IDParam(s_empty_name,
				APPLAUNCHER_INFO_GAME_MSG_PARENTAL)), text.c_str());
		}

		ApplauncherIconInfoItem *item = new ApplauncherIconInfoItem(param.m_parent, NULL);

		if (item != NULL)
			item->Set(APPLAUNCHER_INFO_GAME_LABEL_PARENTAL, value);
		return item;
	}
	case APPLAUNCHER_INFO_GAME_MTIME: {
		string mtime;

		LsdbAppInfo::iterator it = appInfo->find(LSDB_KEY_MTIME);
		if (it != end)
			mtime = LsdbValueText(it->second);
		if (applauncherIconInfoFormatSQLiteTime(&mtime, &value) < 0)
			value = applauncherGetString(IDParam(s_empty_name,
							     APPLAUNCHER_INFO_GAME_MSG_NONE));

		ApplauncherIconInfoItem *item = new ApplauncherIconInfoItem(param.m_parent, NULL);

		if (item != NULL)
			item->Set(APPLAUNCHER_INFO_GAME_LABEL_MTIME, value);
		return item;
	}
	case APPLAUNCHER_INFO_GAME_SIZE:
		switch (type) {
		case 1: {
			LsdbAppInfo::iterator it = appInfo->find(LSDB_KEY_TITLE_ID);
			if (it == end)
				return NULL;

			string titleId(LsdbValueText(it->second));
			int storage = 0;
			GamePluginInterface *game = (GamePluginInterface *)
				applauncherPluginGetInterface("game_plugin", 1);

			if (game != NULL &&
			    ((int (*)(const LsdbAppInfoPtr *, int *))
			     game->func[GAME_PLUGIN_FUNC_GET_STORAGE])(m_appInfo, &storage) < 0)
				return NULL;

			SceUInt64 appSize = 0;
			bool hasAppSize = false;

			it = applauncherIconInfoRow(m_appInfo)->find(LSDB_KEY_APPSIZE);
			if (it != end) {
				appSize = it->second.type == LSDB_VALUE_INTEGER ?
					  (SceUInt64)it->second.integer : 0;
				hasAppSize = true;
			}

			ApplauncherIconInfoSizeItem *item = new ApplauncherIconInfoSizeItem(titleId,
				param.m_parent, storage, 1, hasAppSize, appSize, NULL);

			if (item != NULL)
				item->Setup(APPLAUNCHER_INFO_GAME_LABEL_SIZE);
			return item;
		}
		case 2:
		case 3:
		case 4:
		case 5: {
			LsdbAppInfo::iterator it = appInfo->find(LSDB_KEY_TITLE_ID);
			if (it == end)
				return NULL;

			string titleId(LsdbValueText(it->second));
			ApplauncherIconInfoSizeItem *item = new ApplauncherIconInfoSizeItem(titleId,
				param.m_parent, APPLAUNCHER_INFO_GAME_STORAGE_OTHER, type, false, 0,
				NULL);

			if (item != NULL)
				item->Setup(APPLAUNCHER_INFO_GAME_LABEL_SIZE);
			return item;
		}
		}
		/* other types show the version instead */
		/* fallthrough */
	case APPLAUNCHER_INFO_GAME_VERSION: {
		applauncherIconInfoGetVersion(m_appInfo, &value);

		ApplauncherIconInfoItem *item = new ApplauncherIconInfoItem(param.m_parent, NULL);

		if (item != NULL)
			item->Set(APPLAUNCHER_INFO_GAME_LABEL_VERSION, value);
		return item;
	}
	case APPLAUNCHER_INFO_GAME_START_TIME: {
		if (m_startTime != 0 && applauncherIconInfoFormatTime(&m_startTime, &value) < 0)
			value = applauncherGetString(IDParam(s_empty_name,
							     APPLAUNCHER_INFO_GAME_MSG_NONE));

		ApplauncherIconInfoItem *item = new ApplauncherIconInfoItem(param.m_parent, NULL);

		if (item != NULL)
			item->Set(APPLAUNCHER_INFO_GAME_LABEL_START, value);
		return item;
	}
	case APPLAUNCHER_INFO_GAME_EXPIRE_TIME: {
		if (m_expireTime != 0x7FFFFFFFFFFFFFFFull &&
		    applauncherIconInfoFormatTime(&m_expireTime, &value) < 0)
			value = applauncherGetString(IDParam(s_empty_name,
							     APPLAUNCHER_INFO_GAME_MSG_NONE));

		ApplauncherIconInfoItem *item = new ApplauncherIconInfoItem(param.m_parent, NULL);

		if (item != NULL)
			item->Set(APPLAUNCHER_INFO_GAME_LABEL_EXPIRE, value);
		return item;
	}
	case APPLAUNCHER_INFO_GAME_REMAINING: {
		if (m_expireTime == 0x7FFFFFFFFFFFFFFFull)
			value = applauncherGetString(IDParam(s_empty_name,
							     APPLAUNCHER_INFO_GAME_MSG_NO_EXPIRY));
		else
			value = gameLicenseTimeToText(&m_expireTime);

		ApplauncherIconInfoItem *item = new ApplauncherIconInfoItem(param.m_parent, NULL);

		if (item != NULL)
			item->Set(APPLAUNCHER_INFO_GAME_LABEL_REMAINING, value);
		return item;
	}
	case APPLAUNCHER_INFO_GAME_CHECK: {
		LsdbAppInfo::iterator it = appInfo->find(LSDB_KEY_TITLE_ID);
		if (it == end)
			return NULL;

		string titleId(LsdbValueText(it->second));
		ApplauncherIconInfoCheckItem *item =
			new ApplauncherIconInfoCheckItem(titleId, param.m_parent, NULL);

		if (item != NULL)
			item->Setup();
		return item;
	}
#if !defined(FW_TYPE_CEX)
	case APPLAUNCHER_INFO_GAME_TITLE_ID: {
		LsdbAppInfo::iterator it = appInfo->find(LSDB_KEY_TITLE_ID);
		if (it == end)
			return NULL;

		wstring text(applauncherIconInfoUtf16(string(LsdbValueText(it->second))));
		ApplauncherIconInfoItem *item = new ApplauncherIconInfoItem(param.m_parent, NULL);

		if (item != NULL)
			item->Set(wstring(L"★ Title ID"), text);
		return item;
	}
	case APPLAUNCHER_INFO_GAME_CARD_RW_AREA: {
		SceUInt64 maxSize = 0;
		SceUInt64 freeSize = 0;
		wstring text;

		LsdbAppInfo::iterator it = appInfo->find(LSDB_KEY_TITLE_ID);
		if (it == end)
			return NULL;

		string cardPath(string("gro0:app/") + string(LsdbValueText(it->second)));
		LocalFileStat cardStat;

		if (LocalFile::Getstat(cardPath.c_str(), &cardStat) == 0 &&
		    sceFsUtilGetDevInfo("grw0:", &maxSize, &freeSize) == 0) {
			wstring maxText;

			shellSizeText(&text, freeSize);
			text.append(L" / ");
			shellSizeText(&maxText, maxSize);
			text.append(maxText);
		}

		ApplauncherIconInfoItem *item = new ApplauncherIconInfoItem(param.m_parent, NULL);

		if (item != NULL)
			item->Set(wstring(L"★ PS Vita card R/W Area"), text);
		return item;
	}
#endif
	case APPLAUNCHER_INFO_GAME_UP_STATUS: {
		unsigned int status = 0;

		LsdbAppInfo::iterator it = appInfo->find(LSDB_KEY_UP_STATUS);
		if (it != end)
			status = it->second.type == LSDB_VALUE_INTEGER ?
				 (unsigned int)it->second.integer : 0;

		wstring text;

		if (status <= APPLAUNCHER_INFO_GAME_UP_TRIAL) {
			if (status == 0)
				text = applauncherIconInfoUtf16(string("Off"));
			else
				text = applauncherIconInfoUtf16(string("Trial"));
		} else if (status == APPLAUNCHER_INFO_GAME_UP_FULL) {
			text = applauncherIconInfoUtf16(string("Full"));
		} else {
			text = applauncherGetString(IDParam(s_empty_name,
							    APPLAUNCHER_INFO_GAME_MSG_NONE));
		}

		ApplauncherIconInfoItem *item = new ApplauncherIconInfoItem(param.m_parent, NULL);

		if (item != NULL)
			item->Set(wstring(L"★ Upgradable App"), text);
		return item;
	}
#if !defined(FW_TYPE_CEX)
	case APPLAUNCHER_INFO_GAME_SAVEDATA_MODE: {
		ui::Widget *saveDataItem = param.m_list_view != NULL ?
			param.m_list_view->GetListItem(0, 1) : NULL;
		ApplauncherIconInfoSaveDataDetailItem *item =
			new ApplauncherIconInfoSaveDataDetailItem(saveDataItem, param.m_parent, NULL);

		if (item != NULL)
			item->Setup(APPLAUNCHER_INFO_GAME_LABEL_SD_MODE);
		return item;
	}
	case APPLAUNCHER_INFO_GAME_SAVEDATA_SIZE: {
		ui::Widget *saveDataItem = param.m_list_view != NULL ?
			param.m_list_view->GetListItem(0, 1) : NULL;
		ApplauncherIconInfoSaveDataDetailItem *item =
			new ApplauncherIconInfoSaveDataDetailItem(saveDataItem, param.m_parent, NULL);

		if (item != NULL)
			item->Setup(APPLAUNCHER_INFO_GAME_LABEL_SD_SIZE);
		return item;
	}
	case APPLAUNCHER_INFO_GAME_SAVEDATA_SLOT: {
		LsdbAppInfo::iterator it = appInfo->find(LSDB_KEY_ORG_PATH);
		if (it == end)
			return NULL;

		string path(LsdbValueText(it->second));

		it = applauncherIconInfoRow(m_appInfo)->find(LSDB_KEY_INSTALL_DIR_SAVEDATA);
		if (it == end)
			return NULL;

		string dir(LsdbValueText(it->second));
		int attr = 0;

		it = applauncherIconInfoRow(m_appInfo)->find(LSDB_KEY_ATTRIBUTE);
		if (it != end)
			attr = it->second.type == LSDB_VALUE_INTEGER ? (int)it->second.integer : 0;

		ApplauncherIconInfoSaveDataSlotItem *item =
			new ApplauncherIconInfoSaveDataSlotItem(path, dir, attr, param.m_parent, NULL);

		if (item != NULL)
			item->Setup(wstring(L"★ Save Data Slot"), m_saveDataSlotCache);
		return item;
	}
#endif
	default:
		return NULL;
	}
}

/* (0x810E12A4) */
ui::ListItem *ApplauncherIconInfoGameFactory::Create(CreateParam &param)
{
	if (m_closed) {
		ui::ListItem *item = ItemFactory::Create(param);

		item->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
		return item;
	}
	m_listView = param.m_list_view;
	return CreateItem(sceLsdbGetType(m_appInfo), param);
}
