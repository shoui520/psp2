/*
 * applauncher_contextmenu.cpp: an icon's context menu, the popup list of what can be done
 * with it (make a folder, delete, update history, intellectual property, information), its
 * items read from the application's database row by the "DispInfomationDialogJob" job
 * (0x810D4218-0x810D58B4). The job's name is Sony's; the file's name and the other names are
 * ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/vector>
#include <std/string.h>
#include <versatileparam/idparam.h>
#include <job/job.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <datetime/datetime.h>
#include <common/shared_ptr.h>
#include "../../../shell.h"
#include "../../../popup_list.h"
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"
#include "applauncher_home.h"
#include "applauncher_mode.h"

using namespace paf;

/* livespace_db's export, not declared in its headers yet: a title's database row through the
   shell's client */
extern "C" int SceLsdb_87EE0526(AppInfoSClient *client, const string *titleId,
				LsdbAppInfoPtr *info);
#if !defined(FW_TYPE_CEX)
extern "C" int sceAppMgrLaunchAppByName(int flags, const char *name, const char *param);
#endif

/* PLACEHOLDER: the menu's items, the indexes of its label table */
enum {
	APPLAUNCHER_CONTEXT_MENU_FOLDER		= 0,	/* the icon made a folder */
	APPLAUNCHER_CONTEXT_MENU_DELETE		= 1,
	APPLAUNCHER_CONTEXT_MENU_CHANGE_LOG	= 2,	/* the update history */
	APPLAUNCHER_CONTEXT_MENU_IP		= 3,	/* the intellectual property notices */
	APPLAUNCHER_CONTEXT_MENU_INFO		= 4,
#if !defined(FW_TYPE_CEX)
	APPLAUNCHER_CONTEXT_MENU_DEVKIT		= 5,
	APPLAUNCHER_CONTEXT_MENU_ITEMS		= 6,
#else
	APPLAUNCHER_CONTEXT_MENU_ITEMS		= 5,
#endif
};

#if !defined(FW_TYPE_CEX)
enum {
	APPLAUNCHER_DEVKIT_MENU_PKG_INSTALLER	= 0,
	APPLAUNCHER_DEVKIT_MENU_DELETE_PATCH	= 1,
	APPLAUNCHER_DEVKIT_MENU_DELETE_ADDCONT	= 2,
	APPLAUNCHER_DEVKIT_MENU_DELETE_SAVEDATA	= 3,
	APPLAUNCHER_DEVKIT_MENU_DELETE_TROPHY	= 4,
	APPLAUNCHER_DEVKIT_MENU_COREDUMP	= 5,
};
#endif

/* PLACEHOLDER: the popup list's type and flags the menu opens it with */
#define APPLAUNCHER_CONTEXT_MENU_LIST_TYPE	3
#define APPLAUNCHER_CONTEXT_MENU_LIST_FLAGS	40

/* the application can be deleted (its #_promote_attr) */
#define APPLAUNCHER_PROMOTE_ATTR_DELETABLE	1

class ApplauncherContextMenu;

/* the job reading which items the icon's menu has (68 bytes, vtable 0x8152F588) */
class DispInfomationDialogJob : public job::JobItem {
public:
	DispInfomationDialogJob(common::SharedPtr<ApplauncherIcon> icon,
				ApplauncherContextMenu *menu);
	virtual ~DispInfomationDialogJob();

	virtual int Run();
	virtual void Finish(int result);

	LsdbAppInfoPtr m_appInfo;		/* the application's database row */
	ApplauncherContextMenu *m_menu;
	common::SharedPtr<ApplauncherIcon> m_icon;
	bool m_canMakeFolder;			/* the icon is on a page of the home */
	vector<int> m_items;			/* APPLAUNCHER_CONTEXT_MENU_* */
};

/* PLACEHOLDER: an icon's context menu (40 bytes) */
class ApplauncherContextMenu {
public:
	ApplauncherContextMenu(common::SharedPtr<ApplauncherIcon> icon);
	~ApplauncherContextMenu();

	common::SharedPtr<job::JobItem> m_job;
	common::SharedPtr<ApplauncherIcon> m_icon;
	vector<int> m_items;			/* the job's, once done */
	LsdbAppInfoPtr m_appInfo;		/* the job's, once done */
#if !defined(FW_TYPE_CEX)
	vector<int> m_subItems;
#endif
};

/* the menu open (0x81544368) */
static ApplauncherContextMenu *s_contextMenu;

/* (0x810D4218) */
DispInfomationDialogJob::DispInfomationDialogJob(common::SharedPtr<ApplauncherIcon> icon,
						 ApplauncherContextMenu *menu)
	: job::JobItem("DispInfomationDialogJob", NULL), m_menu(menu), m_icon(icon),
	  m_canMakeFolder(false)
{
	if (m_icon.get() != NULL && m_icon->m_page->m_type == 0)
		m_canMakeFolder = true;
}

/* the job made for the menu (0x810D4286) */
__attribute__((noinline))
static common::SharedPtr<job::JobItem> applauncherContextMenuJobCreate(
	common::SharedPtr<ApplauncherIcon> icon, ApplauncherContextMenu *menu)
{
	return common::SharedPtr<job::JobItem>(new DispInfomationDialogJob(icon, menu));
}

/* the menu's job put on the queue; without it, back to the edit (0x810D437C) */
ApplauncherContextMenu::ApplauncherContextMenu(common::SharedPtr<ApplauncherIcon> icon)
	: m_icon(icon)
{
	s_contextMenu = this;
	m_job = applauncherContextMenuJobCreate(icon, this);
	if (m_job.get() != NULL) {
		if (job::JobQueue::default_queue->Enqueue(m_job) < 0)
			g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
	} else {
		g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
	}
}

/* (0x810D4568) */
void applauncherContextMenuOpen(common::SharedPtr<ApplauncherIcon> icon)
{
	new ApplauncherContextMenu(icon);
	g_applauncherHome->RequestEditState(5);
}

/* the job canceled, the list and the one under it closed (0x810D461E) */
ApplauncherContextMenu::~ApplauncherContextMenu()
{
	if (m_job.get() != NULL)
		m_job->Cancel();
	s_contextMenu = NULL;
	shellPopupListClose(1);
	shellPopupListClose(0);
}

/* (0x810D474A) */
void applauncherContextMenuClose(void)
{
	if (s_contextMenu != NULL)
		delete s_contextMenu;
}

static void applauncherContextMenuOnClose(int result, void *arg);
#if !defined(FW_TYPE_CEX)
static void applauncherContextMenuSubOnSelect(int index, int value, void *arg);
#endif

/* an item chosen: the list closed, what it asks begun (0x810D4766) */
static void applauncherContextMenuOnSelect(int index, int value, void *arg)
{
	ApplauncherContextMenu *menu = (ApplauncherContextMenu *)arg;

	if (index < 0)
		return;

	switch (menu->m_items[index]) {
	case APPLAUNCHER_CONTEXT_MENU_FOLDER:
		shellPopupListClose(0);
		if (menu->m_icon.get() != NULL) {
			ApplauncherPage *page = menu->m_icon->m_page;

			if (page != NULL && page->m_type == 0) {
				page->MakeFolder(menu->m_icon);
				menu->m_icon.reset();
			}
		}
		break;
	case APPLAUNCHER_CONTEXT_MENU_DELETE:
		shellPopupListClose(0);
		applauncherIconDelEnter(menu->m_icon, &menu->m_appInfo, 0);
		break;
	case APPLAUNCHER_CONTEXT_MENU_CHANGE_LOG:
		shellPopupListClose(0);
		applauncherChangeLogEnter(menu->m_icon);
		break;
	case APPLAUNCHER_CONTEXT_MENU_IP:
		shellPopupListClose(0);
		applauncherIntellectualPropertyEnter();
		break;
	case APPLAUNCHER_CONTEXT_MENU_INFO:
		shellPopupListClose(0);
		applauncherIconInfoEnter(menu->m_icon, &menu->m_appInfo);
		break;
#if !defined(FW_TYPE_CEX)
	case APPLAUNCHER_CONTEXT_MENU_DEVKIT: {
		LsdbAppInfo::iterator it = menu->m_appInfo->find(LSDB_KEY_APPTYPE);
		if (it != menu->m_appInfo->end() &&
		    (it->second.type == LSDB_VALUE_INTEGER ? (int)it->second.integer : 0) == 1) {
			ShellPopupListParam param(APPLAUNCHER_CONTEXT_MENU_LIST_TYPE,
						  g_applauncherHome->m_plugin);
			param.SetSelectCallback(applauncherContextMenuSubOnSelect, menu);
			param.SetCloseCallback(applauncherContextMenuOnClose, menu);

			menu->m_subItems.push_back(APPLAUNCHER_DEVKIT_MENU_PKG_INSTALLER);
			menu->m_subItems.push_back(APPLAUNCHER_DEVKIT_MENU_DELETE_PATCH);
			int subType = SceLsdb_298134FE(&menu->m_appInfo);
			if (subType == 2 || subType == 3)
				menu->m_subItems.push_back(APPLAUNCHER_DEVKIT_MENU_DELETE_ADDCONT);
			menu->m_subItems.push_back(APPLAUNCHER_DEVKIT_MENU_DELETE_SAVEDATA);
			menu->m_subItems.push_back(APPLAUNCHER_DEVKIT_MENU_DELETE_TROPHY);
			menu->m_subItems.push_back(APPLAUNCHER_DEVKIT_MENU_COREDUMP);

			for (unsigned int i = 0; i < menu->m_subItems.size(); ++i) {
				ShellPopupListItem item;
				IDParam labelId("", 0);
				switch (menu->m_subItems[i]) {
				case APPLAUNCHER_DEVKIT_MENU_PKG_INSTALLER:
					labelId = IDParam("", 0xF0595DD6u);
					break;
				case APPLAUNCHER_DEVKIT_MENU_DELETE_PATCH:
					labelId = IDParam("", 0xDA081F12u);
					break;
				case APPLAUNCHER_DEVKIT_MENU_DELETE_ADDCONT:
					labelId = IDParam("", 0x7EE4CDFEu);
					break;
				case APPLAUNCHER_DEVKIT_MENU_DELETE_SAVEDATA:
					labelId = IDParam("", 0xB06B97ABu);
					break;
				case APPLAUNCHER_DEVKIT_MENU_DELETE_TROPHY:
					labelId = IDParam("", 0x65053379u);
					break;
				case APPLAUNCHER_DEVKIT_MENU_COREDUMP:
					labelId = IDParam("", 0x57F9BFC0u);
					break;
				default:
					continue;
				}
				const wchar_t *label = param.plugin->GetString(labelId);
				item.text.assign(label, label != NULL ? sce_paf_wcslen(label) : 0);
				param.items.push_back(item);
			}
			shellPopupListOpen(&param, 1, index);
		}
		break;
	}
#endif
	}
}

/* the list closed with an error: back to the edit (0x810D4844) */
static void applauncherContextMenuOnClose(int result, void *arg)
{
	if (s_contextMenu != NULL && result < 0)
		g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
}

#if !defined(FW_TYPE_CEX)
/* (0x810D1F06, TOOL) */
static void applauncherContextMenuSubOnSelect(int index, int value, void *arg)
{
	ApplauncherContextMenu *menu = (ApplauncherContextMenu *)arg;
	(void)value;

	if (index < 0)
		return;

	switch (menu->m_subItems[index]) {
	case APPLAUNCHER_DEVKIT_MENU_PKG_INSTALLER:
		shellPopupListClose(0);
		sceAppMgrLaunchAppByName(0x10000, "NPXS10068",
					 menu->m_icon->m_data->titleId.c_str());
		g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_LAUNCHER);
		break;
	case APPLAUNCHER_DEVKIT_MENU_DELETE_PATCH:
		shellPopupListClose(0);
		applauncherIconDelEnter(menu->m_icon, &menu->m_appInfo, 1);
		break;
	case APPLAUNCHER_DEVKIT_MENU_DELETE_ADDCONT:
		shellPopupListClose(0);
		applauncherIconDelEnter(menu->m_icon, &menu->m_appInfo, 2);
		break;
	case APPLAUNCHER_DEVKIT_MENU_DELETE_SAVEDATA:
		shellPopupListClose(0);
		applauncherIconDelEnter(menu->m_icon, &menu->m_appInfo, 3);
		break;
	case APPLAUNCHER_DEVKIT_MENU_DELETE_TROPHY:
		shellPopupListClose(0);
		applauncherIconDelEnter(menu->m_icon, &menu->m_appInfo, 4);
		break;
	case APPLAUNCHER_DEVKIT_MENU_COREDUMP:
		shellPopupListClose(0);
		applauncherIconDelEnter(menu->m_icon, &menu->m_appInfo, 5);
		break;
	}
}
#endif

/* (0x810D486C, 0x810D4938) */
DispInfomationDialogJob::~DispInfomationDialogJob()
{
}

/*
 * The items the icon has: a folder made of it on a home page, delete when the application can
 * be, the update history when its patch has change information, the intellectual property
 * notices for some applications, and the information always (0x810D4948).
 */
int DispInfomationDialogJob::Run()
{
	m_appInfo = LsdbAppInfoPtr(new LsdbAppInfo());
	if (m_appInfo.get() == NULL)
		return APPLAUNCHER_ERROR_NO_MEMORY;

	LsdbIcon *data = m_icon->m_data.get();

	if (data->type == APPLAUNCHER_ICON_TYPE_POWER) {
		if (m_canMakeFolder)
			m_items.push_back(APPLAUNCHER_CONTEXT_MENU_FOLDER);
		m_items.push_back(APPLAUNCHER_CONTEXT_MENU_INFO);
		return 0;
	}

	int ret = SceLsdb_87EE0526(g_shellLsdbClient->client, &data->titleId, &m_appInfo);

	if (ret != 0)
		return ret;

	LsdbAppInfo::iterator it = m_appInfo->find(LSDB_KEY_PROMOTE_ATTR);

	if (it == m_appInfo->end())
		return APPLAUNCHER_ERROR_NO_TITLE;
	if (m_canMakeFolder)
		m_items.push_back(APPLAUNCHER_CONTEXT_MENU_FOLDER);
	if ((it->second.type == LSDB_VALUE_INTEGER ? it->second.integer : 0) &
	    APPLAUNCHER_PROMOTE_ATTR_DELETABLE)
		m_items.push_back(APPLAUNCHER_CONTEXT_MENU_DELETE);

	it = m_appInfo->find(LSDB_KEY_TITLE_ID);
	if (it == m_appInfo->end())
		return APPLAUNCHER_ERROR_NO_TITLE;

	string titleId(LsdbValueText(it->second));

	if (titleId.length() != 0 && sceLsdbGetType(&m_appInfo) == 1) {
		string path;
		int subType = SceLsdb_298134FE(&m_appInfo);

		if (subType > 1) {
			if (subType <= 3)
				path = string("ux0:patch/") + titleId +
				       string("/sce_sys/changeinfo/changeinfo.xml");
		} else if (subType > 0) {
			path = string("grw0:patch/") + titleId +
			       string("/sce_sys/changeinfo/changeinfo.xml");
		}

		datetime::DateTime created(0, 0, 0, 0, 0, 0, 0);
		datetime::DateTime accessed(0, 0, 0, 0, 0, 0, 0);
		datetime::DateTime modified(0, 0, 0, 0, 0, 0, 0);

		it = m_appInfo->find(LSDB_KEY_ORG_PATH);
		if (it == m_appInfo->end())
			return APPLAUNCHER_ERROR_NO_TITLE;

		bool exists = Dir::IsExist(LsdbValueText(it->second));

		if (LocalFile::GetFileSize(path.c_str()) >= 0 && exists)
			m_items.push_back(APPLAUNCHER_CONTEXT_MENU_CHANGE_LOG);
	}

	it = m_appInfo->find(LSDB_KEY_APPTYPE);
	if (it != m_appInfo->end()) {
		int type = sceLsdbGetType(&m_appInfo);

		if ((it->second.type == LSDB_VALUE_INTEGER ? (int)it->second.integer : 0) == 1 &&
		    type == 1)
			m_items.push_back(APPLAUNCHER_CONTEXT_MENU_IP);
	}
	m_items.push_back(APPLAUNCHER_CONTEXT_MENU_INFO);
#if !defined(FW_TYPE_CEX)
	it = m_appInfo->find(LSDB_KEY_APPTYPE);
	if (it != m_appInfo->end()) {
		int type = sceLsdbGetType(&m_appInfo);

		if ((it->second.type == LSDB_VALUE_INTEGER ? (int)it->second.integer : 0) == 1 &&
		    type == 1)
			m_items.push_back(APPLAUNCHER_CONTEXT_MENU_DEVKIT);
	}
#endif
	return 0;
}

/* PLACEHOLDER: an item's label and the flag its list item takes */
struct ApplauncherContextMenuItem {
	__attribute__((always_inline)) ~ApplauncherContextMenuItem()
	{
	}

	IDParam label;
	bool flag;
};

/* the items given to the menu and its popup list opened; back to the edit on an error or when
   there are too many (0x810D52DC) */
void DispInfomationDialogJob::Finish(int result)
{
	if (IsCanceled())
		return;
	if (result < 0) {
		g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
		return;
	}

	ApplauncherContextMenu *menu = m_menu;

	menu->m_appInfo = m_appInfo;
	menu->m_items = m_items;
	if (menu->m_items.size() > APPLAUNCHER_CONTEXT_MENU_ITEMS) {
		g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
		return;
	}

	ShellPopupListParam param(APPLAUNCHER_CONTEXT_MENU_LIST_TYPE, g_applauncherHome->m_plugin);
	paf::Plugin *plugin = g_applauncherHome->m_plugin;

	if (menu->m_icon->GetWidget() != NULL)
		param.anchor = menu->m_icon->GetWidget();
	param.texts.resize(menu->m_items.size());
	param.flags = APPLAUNCHER_CONTEXT_MENU_LIST_FLAGS;
	param.show[0] = false;
	param.SetSelectCallback(applauncherContextMenuOnSelect, menu);
	param.SetCloseCallback(applauncherContextMenuOnClose, menu);

	static const ApplauncherContextMenuItem s_items[APPLAUNCHER_CONTEXT_MENU_ITEMS] = {
		{ IDParam("", 0xD8D28DAD), false },	/* APPLAUNCHER_CONTEXT_MENU_FOLDER */
		{ IDParam("", 0xF1811D32), false },	/* APPLAUNCHER_CONTEXT_MENU_DELETE */
		{ IDParam("", 0x3DB0979D), false },	/* APPLAUNCHER_CONTEXT_MENU_CHANGE_LOG */
		{ IDParam("", 0xED93A379), false },	/* APPLAUNCHER_CONTEXT_MENU_IP */
		{ IDParam("", 0x87A898A2), false },	/* APPLAUNCHER_CONTEXT_MENU_INFO */
#if !defined(FW_TYPE_CEX)
		{ IDParam("", 0x68A12EA6), true },	/* APPLAUNCHER_CONTEXT_MENU_DEVKIT */
#endif
	};

	for (unsigned int i = 0; i < menu->m_items.size(); i++) {
		ShellPopupListItem item;
		const wchar_t *label = plugin->GetString(s_items[menu->m_items[i]].label);

		item.text.assign(label, label != NULL ? sce_paf_wcslen(label) : 0);
		item.hasSubMenu = s_items[menu->m_items[i]].flag;
		param.items.push_back(item);
	}
	shellPopupListOpen(&param, 0, 0);
}
