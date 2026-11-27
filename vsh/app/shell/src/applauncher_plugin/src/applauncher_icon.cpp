/*
 * applauncher_icon.cpp: an icon of the home's pages (0x810CF650-0x810D2578): its data and image,
 * its view taken from and given back to the icon pool, its selection, edit, drag over the pages
 * and folders. The file's name and the names here are ours (PLACEHOLDER): no record of the 3.6x
 * shell names them.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/stdlib.h>
#include <std/memory>
#include <std/vector>
#include <versatileparam/idparam.h>
#include <common/shared_ptr.h>
#if !defined(FW_TYPE_CEX)
#include <common/main_thread_call_list.h>
#include <std/math.h>
#endif
#include <common/char_encoding.h>
#include <cache/cache_image.h>
#include <widget/core/widget.h>
#include <widget/w_appicon_base.h>
#include <regmgr/regmgr.h>
#include <libkernel/libkernel.h>
#include <timer/timer.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <ces/error.h>
#include <ces/libces_ucs.h>
#include "../../../shell.h"
#include "../../../theme.h"
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"
#include "applauncher_home.h"

using namespace paf;

/* the frame texture an icon's widget shows its image in */
#define APPLAUNCHER_ICON_FRAME_TEXTURE	0xAFDEF9D0
#if !defined(FW_TYPE_CEX)
#define APPLAUNCHER_ICON_APPHOME_FALLBACK_TEXTURE	0xAFDF1D10u
#endif
/* the power icon's label */
#define APPLAUNCHER_ICON_MSG_POWER	0xC75A2636

/* PLACEHOLDER: an icon's type for a web link, its URL started in the browser */
#define APPLAUNCHER_ICON_TYPE_WEBLINK	2
/* PLACEHOLDER: the launch's flag for a link */
#define APPLAUNCHER_ICON_LAUNCH_LINK	0x20000

/* PLACEHOLDER: the error and the text of the dialog a press of an unavailable icon shows */
#define APPLAUNCHER_ICON_UNAVAILABLE_ERROR	0x8010330C
#define APPLAUNCHER_ICON_MSG_UNAVAILABLE	0x78D452F9

/* the icons a folder holds */
#define APPLAUNCHER_FOLDER_ICONS	10

/* the image of an icon its parental lock keeps from starting */
#define APPLAUNCHER_ICON_LOCKED_PATH	"vs0:data/internal/icon/parental_lock.png"

/* PLACEHOLDER: the languages' codes by SCE_SYSTEM_PARAM_LANG_*, Turkish none; nothing in this
   build reads them */
extern "C" int sceAppMgrLaunchAppByNameForShell(int flags, const char *name, const char *param);

static string s_languageCodes[20] = {
	"jp", "en", "fr", "es", "de", "it", "nl", "pt", "ru", "kr",
	"ct", "cs", "fi", "sw", "dk", "no", "pl", "pb", "gb",
};

/* PLACEHOLDER: the applications of the system's that are not system icons: Welcome Park and
   the settings */
static string s_userTitles[2] = {
	"NPXS10007", "NPXS10010",
};

/*
 * The drag's pages (0x81544358-0x81544367): the page the dragged icon was last on (Sony's
 * "OldParentTab", from its messages), the page an edge turns to and the seconds it has been
 * held there (PLACEHOLDER names), and whether an edge may turn the page (PLACEHOLDER: set once
 * the icon has been away from the edges).
 */
static ApplauncherPage *s_oldParentTab = NULL;
static ApplauncherPage *s_turnPage = NULL;
static float s_turnTime = 0.0f;
static bool s_edgeArmed = false;

static common::SharedPtr<File> applauncherFolderImageOpen(const char *path, void *userdata,
							   int32_t *error);
#if !defined(FW_TYPE_CEX)
static void applauncherIconAppHomeUpdate(void *arg);
#endif

/* (0x810CF650) */
int applauncherIconIsSystem(const LsdbIconPtr *icon)
{
	for (SceUInt32 i = 0; i < 2; ++i) {
		if (s_userTitles[i] == (*icon)->titleId)
			return 0;
	}
	if (sce_paf_strcmp("NPXS10013", (*icon)->titleId.c_str()) == 0) {
		int ps4link = 0;

		if (sceRegMgrGetKeyInt("/CONFIG/SPECIFIC", "idu_ps4link", &ps4link) == 0 &&
		    ps4link == 1)
			return 0;
	}
	if ((*icon)->type != 0)
		return 1;
	if ((*icon)->titleId.size() >= 9) {
		if (sce_paf_strncmp((*icon)->titleId.c_str(), "NPXS10",
				    sce_paf_strlen("NPXS10")) == 0) {
			char *end;
			long number = sce_paf_strtol((*icon)->titleId.c_str() + 6, &end, 10);

			if (*end == '\0' && number < 150)
				return 1;
		} else if (sce_paf_strcmp((*icon)->titleId.c_str(), "PCSI90016") == 0) {
			return 1;
		}
	}
	return 0;
}

/* (0x810CF74C) */
bool ApplauncherIcon::IsFolder()
{
	return m_data.get() != NULL && m_data->type == LSDB_ICON_TYPE_FOLDER;
}

/* (0x810CF75E) */
void ApplauncherIcon::SetSelf(const common::SharedPtr<ApplauncherIcon> &self)
{
	m_self = self;
}

/* the parental lock keeps the icon's application from starting: the browser's, the store's and
   email's by their own controls, the others by the content level */
static inline bool applauncherIconIsLocked(const ApplauncherIcon *icon)
{
	if (sce_paf_strcmp(icon->m_data->titleId.c_str(), "NPXS10003") == 0) {
		int control = 0;

		if (sceRegMgrGetKeyInt("/CONFIG/SECURITY/PARENTAL", "browser_start_control",
					      &control) != 0)
			return true;
		return control != 0;
	}
	if (sce_paf_strcmp(icon->m_data->titleId.c_str(), "NPXS10002") == 0) {
		int control = 0;

		if (sceRegMgrGetKeyInt("/CONFIG/SECURITY/PARENTAL", "store_start_control",
					      &control) != 0)
			return false;
		return control != 0;
	}
	if (sce_paf_strcmp(icon->m_data->titleId.c_str(), "NPXS10072") == 0) {
		int control = 0;

		if (sceRegMgrGetKeyInt("/CONFIG/SECURITY/PARENTAL", "email_start_control",
					      &control) != 0)
			return false;
		return control != 0;
	}

	int control = 0;

	if (sceRegMgrGetKeyInt("/CONFIG/SECURITY/PARENTAL", "content_start_control",
				      &control) != 0)
		return true;

	int level = icon->m_data->parentalLockLv;

	if (level == 99)
		return true;
	return control != 0 && level != 0 && level > control;
}

/* the icon's image loaded again (a folder's made of its children's), its widget given it; the
   old image dropped from the cache when asked, unless it was the lock's (0x810CF7DE) */
void ApplauncherIcon::Update(bool clear)
{
	intrusive_ptr<cache::CacheImageInterface> old = m_image;

	m_image.clear();
	if (m_icon != NULL && !IsFolder()) {
		{
			intrusive_ptr<cache::CacheImageInterface> none;

			m_icon->m_widget->SetDrawObjMaskBit(&none, 0, 0);
		}
		static_cast<ui::Widget *>(m_icon->m_widget)->SetTexture(
			intrusive_ptr<graph::Surface>());
		m_icon->m_widget->SetTexture(
			g_applauncherPlugin->GetTexture(IDParam(APPLAUNCHER_ICON_FRAME_TEXTURE)), 0, 0);
	}
	if (clear) {
		cache::CacheImage *image = static_cast<cache::CacheImage *>(old.get());

		if (image != NULL && image->m_path != APPLAUNCHER_ICON_LOCKED_PATH)
			applauncherIconCacheClear(old);
		old.clear();
	}
	if (m_data.get() == NULL || m_data->type != LSDB_ICON_TYPE_FOLDER) {
		char path[2048];

		if (applauncherIconIsLocked(this))
			sce_paf_strncpy(path, APPLAUNCHER_ICON_LOCKED_PATH, sizeof(path));
		else
			sce_paf_strncpy(path, m_data->iconPath.c_str(), sizeof(path));
		applauncherIconCacheLoad(string(path), &m_image, NULL, NULL, false);
	} else if (m_data->rowid != 0 && m_data->folder.get() != NULL) {
		cache::CacheImage::Option opt;
		char key[64];

		opt.auto_delete = true;
		opt.retry = true;
		opt.retry_count = -1;
		opt.reserved = true;
		opt.read_size = 0;
		opt.read_offset = 0;
		opt.max_size_kb = 0;
		opt.open_func = NULL;
		opt.open_userdata = NULL;
		memset(&opt.sub_rect, 0, sizeof(opt.sub_rect));
		memset(&opt.scale, 0, sizeof(opt.scale));
		opt.resize_type = image::ImageResizeType_NearestNeighbor;
		opt.direct_load = false;
		opt.image_order = ImageOrder_NONE;
		opt.image_order_param = 0;
		opt.check_image = NULL;
		opt.check_image_userdata = NULL;
		opt.surface_flag = false;
		sce_paf_snprintf(key, sizeof(key), "%d:%lld", m_data->folder->id,
				 m_data->rowid);
		opt.open_func = applauncherFolderImageOpen;
		applauncherIconCacheLoad(string(key), &m_image, &opt, NULL, false);
	}
	if (m_icon != NULL)
		m_icon->m_widget->SetDrawObjMaskBit(&m_image, 0, 0);
}

/* the text in UTF-8 */
static inline string applauncherIconUtf8(const wstring &text)
{
	string utf8;

	common::Utf16ToUtf8(text, &utf8);
	return utf8;
}

/* the text in UTF-16 */
static inline wstring applauncherIconUtf16(const string &text)
{
	wstring utf16;

	common::Utf8ToUtf16(text, &utf16, 0);
	return utf16;
}

/* (0x810CFB92) */
ApplauncherIcon::ApplauncherIcon(Plugin *plugin, const LsdbIconPtr &data)
	: m_icon(NULL), m_page(NULL), m_launchLock(0), m_selected(false), m_hidden(false),
	  m_focusPending(false), m_added(false), m_slot(-1), m_movePage(NULL), m_moveSlot(-1),
	  m_homeSlot(-1), m_data(data), m_unavailable(false), m_removeEntry(false)
{
	if (m_data->type == APPLAUNCHER_ICON_TYPE_POWER)
		m_data->title = applauncherIconUtf8(
			wstring(g_applauncherPlugin->GetString(IDParam(APPLAUNCHER_ICON_MSG_POWER))));
	m_label = applauncherIconUtf16(m_data->title);
	shellThemeGetIconPath(m_data->titleId.c_str(), &m_data->iconPath, m_data->type);
	Update(false);
#if !defined(FW_TYPE_CEX)
	if (m_data->titleId == string("NPXS10998"))
		common::MainThreadCallList::Register(applauncherIconAppHomeUpdate, this);
#endif
	if (shellStartupGetIduMode() != 0 && applauncherIconIsSystem(&data) != 0)
		m_hidden = true;
}

/* (0x810CFD82) */
common::SharedPtr<ApplauncherIcon> applauncherIconCreate(Plugin *plugin, const LsdbIconPtr &data)
{
	common::SharedPtr<ApplauncherIcon> icon(new ApplauncherIcon(plugin, data));

	icon->SetSelf(icon);
	if (icon->IsFolder())
		++g_applauncherHome->m_folderCount;
	return icon;
}

/* (0x810CFE08) */
void ApplauncherIcon::ReleaseView(bool animate)
{
	if (m_icon != NULL) {
		applauncherIconPoolFree(m_icon, animate);
		m_icon = NULL;
	}
}

/* the icon taken off its page and the home, its view given back; the topmenu entry of its title
   removed with it when asked (0x810CFE1C) */
void applauncherIconDelete(common::SharedPtr<ApplauncherIcon> &icon, bool removeEntry,
			   bool animate)
{
	if (icon->m_page != NULL)
		applauncherIconRemove(icon, false);
	if (icon->IsFolder() && g_applauncherHome != NULL)
		--g_applauncherHome->m_folderCount;
	applauncherHomeClearAppIcon(icon->m_self);
	icon->m_removeEntry = removeEntry;
	icon->ReleaseView(animate);
	icon->m_self.reset();
	icon.reset();
}

/* (0x810CFFD2) */
void ApplauncherIcon::BeginEdit(bool animate)
{
	m_movePage = m_page;
	m_moveSlot = m_homeSlot = m_slot;
	if (m_icon != NULL)
		applauncherIconWidgetBeginEdit(m_icon, animate);
}

/* (0x810CFFEC) */
void ApplauncherIcon::EndEdit()
{
	m_movePage = NULL;
	m_moveSlot = -1;
	m_homeSlot = -1;
	if (m_icon != NULL)
		applauncherIconWidgetEndEdit(m_icon);
}

/* (0x810D0006) */
ui::Widget *ApplauncherIcon::GetWidget()
{
	if (m_icon == NULL)
		return NULL;
	return m_icon->m_widget;
}

/* (0x810D0012) */
math::v4 ApplauncherIcon::GetSlotPos()
{
	if (m_slot < 0)
		return math::v4(applauncherPageGetSlotPos(0).extract_x(),
				applauncherPageGetSlotPos(0).extract_y(), 0.0f, 0.0f);
	if (m_page != NULL)
		return math::v4(m_page->GetSlotPos(m_slot).extract_x(),
				m_page->GetSlotPos(m_slot).extract_y(), 0.0f, 0.0f);
	return math::v4(applauncherPageGetSlotPos(m_slot).extract_x(),
			applauncherPageGetSlotPos(m_slot).extract_y(), 0.0f, 0.0f);
}

/* the folder's name set (cut to what fits the database's 51 bytes of UTF-8) on its label, its
   widget and its data, and written to the database when it is on a page (0x810D00CE) */
void applauncherFolderRename(ApplauncherIcon *folder, wstring &name)
{
	SceCesUcsContext context;
	char utf8[52];
	uint32_t used;
	uint32_t length;
	int ret;

	sceCesUcsContextInit(&context);
	ret = sceCesUtf16StrToUtf8Str(&context, (const uint16_t *)name.c_str(), name.length(),
				      &used, (uint8_t *)utf8, sizeof(utf8), &length);
	if (ret == 0 || ret == SCE_CES_ERROR_DST_BUFFER_END) {
		name = applauncherIconUtf16(string(utf8));
		folder->m_label = name;
		if (folder->m_icon != NULL)
			applauncherIconWidgetSetLabel(folder->m_icon, folder->m_label);
		folder->m_data->title.assign(utf8, sce_paf_strlen(utf8));
	}
	if (folder->m_page != NULL) {
		LsdbIconPtr data(new LsdbIcon);

		data->title = folder->m_data->title;
		data->pos = folder->m_data->pos;
		applauncherSetIconInfoJobStart(((job::JobQueue *(*)(void))
			g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_GET_JOB_QUEUE])(),
			folder->m_page->GetIndex(), data, 1, NULL, NULL);
	}
}

/* (0x810D0326) */
void ApplauncherIcon::Select()
{
	m_selected = true;
}

/* (0x810D032C) */
void ApplauncherIcon::Unselect()
{
	m_selected = false;
}

/* the icon's widget given the focus, or the focus left pending until it has one (0x810D0332) */
int ApplauncherIcon::Focus()
{
	if (m_icon != NULL) {
		m_icon->m_widget->SetFocus(NULL);
		m_focusPending = false;
		return 0;
	}
	m_focusPending = true;
	return 1;
}

/* (0x810D0352) */
void ApplauncherIcon::Reset(const LsdbIconPtr &data)
{
	m_data = data;
	if (m_data->type == APPLAUNCHER_ICON_TYPE_POWER) {
		m_data->title = applauncherIconUtf8(
			wstring(g_applauncherPlugin->GetString(IDParam(APPLAUNCHER_ICON_MSG_POWER))));
		m_data->titleId = "power";
	}
	m_label = applauncherIconUtf16(m_data->title);
	if (m_icon != NULL)
		applauncherIconWidgetSetLabel(m_icon, m_label);
}

/* (0x810D055C) */
void ApplauncherIcon::Reload()
{
	Update(false);
}

/* (0x810D0566) */
int ApplauncherIcon::Launch()
{
	ShellPluginInterface *topmenu = g_applauncherHome->m_topmenu;

	if (m_launchLock != 0)
		return 0;

	const char *titleId = m_data->titleId.c_str();
	intrusive_ptr<cache::CacheImageInterface> image = m_image;
	int result = -1;

	if (m_data->type == APPLAUNCHER_ICON_TYPE_WEBLINK) {
		result = sceAppMgrLaunchAppByNameForShell(APPLAUNCHER_ICON_LAUNCH_LINK, titleId,
							  m_data->command.c_str());
		titleId = "NPXS10003";

		common::SharedPtr<ApplauncherIcon> browser =
			g_applauncherHome->FindIcon(string("NPXS10003"), NULL);

		if (browser.get() != NULL)
			image = browser->m_image;
	}
	if (m_data->type == APPLAUNCHER_ICON_TYPE_POWER) {
		((void (*)(void))g_applauncherHome->m_powerManage->func[19])();
		return 1;
	}

	int index = ((int (*)(const char *))topmenu->func[TOPMENU_PLUGIN_FUNC_SET_LAUNCH_TITLE])(
		titleId);

	if (((int (*)(const string &, int *, intrusive_ptr<cache::CacheImageInterface> *, int))
	     topmenu->func[1])(string(titleId), &index, &image, result) == 0)
		return 0;
	if (index == -1)
		return 0;
	applauncherLaunchEffectBegin(m_self);
	applauncherHomeCloseAppPage(false);
	return 1;
}

/* the icon pressed: out of the launcher mode a folder opens and the others' menu comes; otherwise a folder
   opens, an unavailable icon shows an error, and the others start (0x810D076C) */
void ApplauncherIcon::OnPressed()
{
	if (g_applauncherHome->m_homeFlags[0] == 0.0f || !applauncherLaunchEffectIsNotRising())
		return;
	if (g_applauncherModeManager->m_currentMode != APPLAUNCHER_MODE_LAUNCHER) {
		if (IsFolder())
			applauncherHomeOpenAppPage(m_self, NULL, false);
		else
			applauncherContextMenuEnter(m_self);
	} else if (m_unavailable) {
		CommonGuiMessageDialogParam param;

		param.type = APPLAUNCHER_ICON_UNAVAILABLE_ERROR;
		param.cancelButton = 0;
		param.messageId = APPLAUNCHER_ICON_MSG_UNAVAILABLE;
		SceCommonGuiDialog_A6422883(g_applauncherPlugin, &param, NULL, NULL);
	} else if (IsFolder()) {
		applauncherHomeOpenAppPage(m_self, NULL, false);
	} else {
		Launch();
	}
}

/* (0x810D0900) */
void ApplauncherIcon::Drop()
{
	if (g_applauncherDragItem.get() == NULL && g_applauncherHome->m_mode == 0)
		return;

	ApplauncherPage *page = m_page;

	if (page == NULL)
		page = s_oldParentTab;
	if (m_movePage != NULL && page != m_movePage)
		m_movePage->iconMoveEnd(common::SharedPtr<ApplauncherIcon>());
	page->iconMoveEnd(common::SharedPtr<ApplauncherIcon>(m_self));
	g_applauncherDragItem.reset();
}

/* the icon's widget dragged: begun (0), moved (1) or ended (2), the page turned when it is held
   at the top or bottom edge or an edge is given (1 the page before, 2 the page after)
   (0x810D0AB0) */
void ApplauncherIcon::UpdateDrag(int phase, ui::AppIconBase *widget, int edge)
{
	ApplauncherPage *page = m_page;

	if (page == NULL)
		page = s_oldParentTab;

	ui::Widget *root = page->GetRoot();

	if (phase == 0) {
		g_applauncherMoveIcon.reset();
		g_applauncherDragItem = m_self;
		s_edgeArmed = true;
	}
	if (phase != 1) {
		s_turnPage = NULL;
		s_turnTime = 0.0f;
	}
	if (root != NULL && page->IsOpened()) {
		ApplauncherPage *target = page;
		math::v4 pos = widget->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		math::v4 size = *root->GetSize(0);

		pos = pos + (widget->m_parent->GetWorldMatrix().w() - root->GetWorldMatrix().w());
		if (page->m_type == 0) {
			if (edge == 0) {
				if (size.extract_y() * -0.5f + 50.0f > pos.extract_y()) {
					if (s_edgeArmed && phase != 0)
						target = applauncherHomeGetNextPage(page);
				} else if (size.extract_y() * 0.5f - 50.0f < pos.extract_y()) {
					if (s_edgeArmed && phase != 0)
						target = applauncherHomeGetPreviousPage(page);
				} else {
					s_edgeArmed = true;
				}
			} else if (edge == 1) {
				target = applauncherHomeGetPreviousPage(page);
			} else if (edge == 2) {
				target = applauncherHomeGetNextPage(page);
			}
		} else {
			if (m_homeSlot < 0)
				m_homeSlot = page->GetEmptySlot();
			s_edgeArmed = true;
		}
		if (page != target) {
			bool turn = true;

			if (edge == 0) {
				ApplauncherPage *turnPage = s_turnPage;

				if (turnPage != target) {
					s_turnPage = target;
					s_turnTime = 0.0f;
					turn = false;
				} else {
					s_turnTime += Timer::s_tick_unit * Timer::s_tick_interval;
					if (s_turnTime > 0.7f) {
						s_turnPage = NULL;
						target = turnPage;
						s_turnTime = 0.0f;
					} else {
						turn = false;
					}
				}
			}
			if (turn && target != NULL) {
				m_homeSlot = target->GetEmptySlot();
				applauncherHomeJumpTo(target);
				page = target;
			}
		}
	}

	switch (phase) {
	case 0:
		page->iconMoveBegin(m_self);
		if (page->m_type == 0) {
			ApplauncherPage *focused = static_cast<ApplauncherPage *>(
				g_applauncherHome->m_scroller.GetFocused());

			if (focused != page) {
				m_homeSlot = focused->GetEmptySlot();
				applauncherIconRemove(m_self, false);
				s_oldParentTab = focused;
				s_edgeArmed = false;
				sceClibPrintf("[SceShell] setOldParentTab0 %d %p %p %p %p\n", 500,
					      focused, m_page, focused, this);
			} else {
				sceClibPrintf("[SceShell] nosetOldParentTab0 %d %p %p %p %p\n", 503,
					      s_oldParentTab, m_page, page, this);
			}
		} else {
			sceClibPrintf("[SceShell] nosetOldParentTab1 %d %p %p %p %p\n", 507,
				      s_oldParentTab, m_page, page, this);
		}
		break;
	case 2: {
		ApplauncherPage *oldPage = m_page;

		if (m_icon != NULL && oldPage != NULL)
			m_icon->m_widget->SetParent(oldPage->GetRoot(), NULL, 0);
		Drop();
		s_oldParentTab = NULL;
		if (m_page != NULL)
			sceClibPrintf("[SceShell] setOldParentTab4 %d %p %p %p\n", 551, m_page,
				      oldPage, this);
		else
			sceClibPrintf("[SceShell] setOldParentTab1 %d %p %p %p\n", 548, m_page,
				      oldPage, this);
		break;
	}
	case 1:
		if (m_page != NULL && (SceUIntPtr)m_page < 0x10)
			sceClibPrintf("[SceShell] ParnetTabInvalid0 %d %p %p\n", 596, m_page, this);
		if (page->IsOpened()) {
			ApplauncherPage *next = page->iconMove(m_self, false, edge);

			if (m_page != NULL && (SceUIntPtr)m_page < 0x10)
				sceClibPrintf("[SceShell] ParnetTabInvalid1 %d %p %p\n", 606, m_page,
					      this);
			if (next == NULL) {
				if (s_oldParentTab != page)
					sceClibPrintf("[SceShell] setOldParentTab2 %d %p %p %p\n", 613,
						      page, s_oldParentTab, this);
				s_oldParentTab = page;
			} else {
				if (s_oldParentTab != next)
					sceClibPrintf("[SceShell] setOldParentTab3 %d %p %p %p %d\n", 621,
						      next, s_oldParentTab, this, next->m_type);
				s_oldParentTab = next;
				if (next->m_type == 0) {
					m_homeSlot = next->GetEmptySlot();
					next->iconMove(m_self, true, 0);
					if (m_page != NULL && (SceUIntPtr)m_page < 0x10)
						sceClibPrintf("[SceShell] ParnetTabInvalid2 %d %p %p\n",
							      646, m_page, this);
				} else {
					m_homeSlot = -1;
				}
			}
		}
		if (m_page != NULL && (SceUIntPtr)m_page < 0x10)
			sceClibPrintf("[SceShell] ParnetTabInvalid3 %d %p %p\n", 657, m_page, this);
		break;
	}
}

/* the drag's begin (0x2000D), move (0x2000E) and end (the others) given to UpdateDrag
   (0x810D1110) */
void ApplauncherIcon::OnDragEvent(ui::Event *e, ui::AppIconBase *widget)
{
	int phase;

	switch ((e->GetDetail() >> 8) & 0x7F) {
	case 0xD:
		phase = 0;
		break;
	case 0xE:
		phase = 1;
		break;
	default:
		phase = 2;
		break;
	}
	UpdateDrag(phase, widget, 0);
}

/*
 * The dragged icon moved a slot up (1), down (2), left (3) or right (4) with the buttons: up from
 * the top row and down from the bottom one to the page before or after, left and right out of a
 * folder's page to the home's slot; its widget moved there over a time that grows with the
 * fourth root of the way (0x810D1138).
 */
void ApplauncherIcon::MoveDrag(int direction)
{
	ui::AppIconBase *widget = NULL;
	ApplauncherPage *page = m_page;
	int slot = m_slot;
	math::v2 pos(0.0f, 0.0f);
	ApplauncherPage *previous = NULL;
	ApplauncherPage *next = NULL;
	int edge = 0;
	bool folder = false;

	if (m_icon != NULL)
		widget = m_icon->m_widget;
	if (page == NULL)
		page = s_oldParentTab;
	if (slot < 0) {
		if (applauncherHomeGetAppPage() != NULL && !applauncherHomeGetAppPage()->IsOpened())
			return;
		if (g_applauncherDropTarget.get() == NULL)
			return;
		slot = g_applauncherDropTarget->m_slot;
	}
	pos = page->GetSlotPos(slot);
	if (page->m_type == 0) {
		previous = applauncherHomeGetPreviousPage(page);
		next = applauncherHomeGetNextPage(page);
	} else if (page->m_type == 2) {
		folder = true;
	}

	if (direction == 1) {
		switch (slot) {
		case 0:
		case 1:
		case 2:
			if (!folder && previous != page) {
				pos = page->GetSlotPos(slot + 7);
				edge = 1;
			}
			break;
		case 3:
		case 4:
		case 5:
			pos = page->GetSlotPos(slot - 3);
			break;
		case 6:
		case 7:
		case 8:
		case 9:
			pos = page->GetSlotPos(slot - 4);
			break;
		}
	} else if (direction == 2) {
		switch (slot) {
		case 0:
		case 1:
		case 2:
			pos = page->GetSlotPos(slot + 3);
			break;
		case 3:
		case 4:
		case 5:
			pos = page->GetSlotPos(slot + 4);
			break;
		case 6:
			pos = page->GetSlotPos(slot + 3);
			break;
		case 7:
		case 8:
		case 9:
			if (!folder && next != page) {
				pos = page->GetSlotPos(slot - 7);
				edge = 2;
			}
			break;
		}
	} else if (direction == 3) {
		switch (slot) {
		case 1:
		case 2:
		case 4:
		case 5:
		case 6:
		case 8:
		case 9:
			pos = page->GetSlotPos(slot - 1);
			break;
		case 0:
		case 3:
		case 7:
			if (folder) {
				pos = applauncherPageGetSlotPos(slot);
				edge = 3;
			}
			break;
		}
	} else if (direction == 4) {
		switch (slot) {
		case 0:
		case 1:
		case 3:
		case 4:
		case 5:
		case 7:
		case 8:
			pos = page->GetSlotPos(slot + 1);
			break;
		case 2:
		case 6:
		case 9:
			if (folder) {
				pos = applauncherPageGetSlotPos(slot);
				edge = 3;
			}
			break;
		}
	}

	ui::Widget *root = page->GetRoot();

	widget->m_parent->GetWorldMatrix();
	root->GetWorldMatrix();

	math::v4 from = widget->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v4 to(pos.extract_x(), pos.extract_y(), 0.0f, 0.0f);
	math::v4 way = to - from;
	float length = sce::Vectormath::Simd::Aos::length(
		sce::Vectormath::Simd::Aos::Vector2(vget_low_f32(way.getImpl().get128())));
	float root4;

	if (length < 1.0f)
		length = 1.0f;
	root4 = __builtin_sqrtf(length);

	float time = __builtin_sqrtf(root4) * 60.0f;

	widget->SetPos(to, new Timer(time, Timer::FUNC_QUART_OUT), ui::Widget::ANIM_MOVE, 2);
	UpdateDrag(1, widget, edge);
}

/* (0x810D14D6) */
void ApplauncherIcon::LoadView()
{
	if (m_icon == NULL) {
		m_icon = applauncherIconPoolAlloc(m_self);
		if (m_icon != NULL && m_focusPending) {
			m_icon->m_widget->SetFocus(NULL);
			m_focusPending = false;
		}
	}
}

/* the icon given its application's new data: its label, its image loaded again (the old one
   dropped from the cache) and given to topmenu (0x810D1502) */
void ApplauncherIcon::SetData(const LsdbIconPtr &data)
{
	m_unavailable = false;
	m_data = data;
	if (m_data->type == APPLAUNCHER_ICON_TYPE_POWER) {
		m_data->title = applauncherIconUtf8(
			wstring(g_applauncherPlugin->GetString(IDParam(APPLAUNCHER_ICON_MSG_POWER))));
		m_data->titleId = "power";
	}
	{
		wstring label;

		common::Utf8ToUtf16(m_data->title, &label, 0);
		m_label = label;
		if (m_icon != NULL)
			applauncherIconWidgetSetLabel(m_icon, m_label);
	}
	Update(true);
	((void (*)(const string &, intrusive_ptr<cache::CacheImageInterface> *))
		g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_SET_APP_IMAGE])(
		m_data->titleId, &m_image);
}

/* the folder takes the title: it has room and the title is not in it yet; the titles are
   walked also when it is full */
static inline bool applauncherFolderCanAdd(LsdbFolder *folder, const char *titleId)
{
	vector<string>::iterator it;

	/* the count compared unsigned, as in Sony's */
	if ((SceUInt32)folder->iconCount >= APPLAUNCHER_FOLDER_ICONS) {
		for (it = folder->titleIds.begin(); it != folder->titleIds.end(); ++it)
			;
		return false;
	}
	for (it = folder->titleIds.begin(); it != folder->titleIds.end(); ++it) {
		if (*it == titleId)
			return false;
	}
	return true;
}

/* (0x810D1738) */
bool ApplauncherIcon::CanAddChild(const char *titleId)
{
	if (m_data.get() == NULL || m_data->type != LSDB_ICON_TYPE_FOLDER)
		return false;

	LsdbFolder *folder = m_data->folder.get();

	if (folder == NULL)
		return false;
	return applauncherFolderCanAdd(folder, titleId);
}

/* the title put in the folder, the power icon counted only; false when the folder does not
   take it (0x810D17C0) */
bool ApplauncherIcon::AddChild(const char *titleId)
{
	if (m_data.get() == NULL || m_data->type != LSDB_ICON_TYPE_FOLDER)
		return false;
	if (m_data->folder.get() == NULL)
		return false;
	if (sce_paf_strcmp("power", titleId) == 0) {
		++m_data->folder->iconCount;
		return true;
	}
	if (!applauncherFolderCanAdd(m_data->folder.get(), titleId))
		return false;
	m_data->folder->titleIds.push_back(string(titleId));
	++m_data->folder->iconCount;
	return true;
}

/* the title taken out of the folder, the power icon uncounted only (0x810D192E) */
void ApplauncherIcon::RemoveChild(const char *titleId)
{
	if (m_data.get() == NULL || m_data->type != LSDB_ICON_TYPE_FOLDER ||
	    m_data->folder.get() == NULL)
		return;
	if (sce_paf_strcmp("power", titleId) == 0) {
		--m_data->folder->iconCount;
		return;
	}

	LsdbFolder *folder = m_data->folder.get();

	for (vector<string>::iterator it = folder->titleIds.begin(); it != folder->titleIds.end();
	     ++it) {
		if (*it == titleId) {
			m_data->folder->titleIds.erase(it);
			--m_data->folder->iconCount;
			return;
		}
	}
}

/* (0x810D19F6) */
void ApplauncherIcon::Resume()
{
	if (m_icon != NULL)
		applauncherIconWidgetDropIn(m_icon);
}

/* the icon put in the folder and deleted from its page, its widget flying to the folder's (or
   below the screen); deleted alone without a widget or a folder (0x810D1A06) */
void ApplauncherIcon::PutInFolder(const common::SharedPtr<ApplauncherIcon> &folder)
{
	ui::Widget *widget = NULL;

	if (m_icon != NULL)
		widget = m_icon->m_widget;
	applauncherIconRemove(m_self, true);
	if (widget == NULL || folder.get() == NULL) {
		applauncherIconDelete(m_self, false, false);
		return;
	}

	ui::Widget *target = NULL;
	math::v4 pos(0.0f, 544.0f, 0.0f, 0.0f);

	if (folder->m_icon != NULL)
		target = folder->m_icon->m_widget;
	if (target != NULL)
		pos = target->GetWorldMatrix().w() - widget->m_parent->GetWorldMatrix().w();
	else if (folder->m_page != NULL)
		pos = folder->m_page->GetWidget()->GetWorldMatrix().w() -
		      widget->m_parent->GetWorldMatrix().w();
	folder->AddChild(m_data->titleId.c_str());
	widget->KillAnim(ui::Widget::ANIM_MOVE);
	widget->SetPos(pos, new Timer(g_applauncherFolderMoveTime, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_MOVE, 0);
	applauncherIconDelete(m_self, false, true);
}

/* (0x810D1C24) */
bool ApplauncherIcon::HasChild(const string &titleId)
{
	if (m_data.get() == NULL || m_data->type != LSDB_ICON_TYPE_FOLDER)
		return false;

	LsdbFolder *folder = m_data->folder.get();

	if (folder == NULL)
		return false;
	for (vector<string>::iterator it = folder->titleIds.begin(); it != folder->titleIds.end();
	     ++it) {
		if (*it == titleId)
			return true;
	}
	return false;
}

/* the folder's image made again from its children's (0x810D1C90) */
void ApplauncherIcon::UpdateFolder()
{
	if (m_data.get() == NULL || m_data->type != LSDB_ICON_TYPE_FOLDER || m_page == NULL ||
	    m_data->folder.get() == NULL)
		return;

	vector<ApplauncherRemovedIcon> icons;
	ApplauncherRemovedIcon item;

	item.m_id = m_data->folder->id;
	item.m_pageIndex = m_page->GetIndex();
	item.m_slot = m_slot;
	icons.push_back(item);
	applauncherPagesReloadFolderIconCapture(icons, m_data->rowid);
}

/* the widget faded in over the timer, made visible first */
static inline void ShowWidget(ui::Widget *widget, Timer *timer)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, NULL);
}

/* (0x810D1D38) */
void ApplauncherIcon::Show()
{
	ui::Widget *widget = NULL;

	if (m_icon != NULL)
		widget = m_icon->m_widget;
	m_added = false;
	if (widget != NULL)
		ShowWidget(widget, new Timer(420.0f, Timer::FUNC_LINEAR));
}

/* (0x810D1DE6) */
void applauncherIconCancelDrag(void)
{
	if (g_applauncherDragItem.get() != NULL && g_applauncherDragItem->m_icon != NULL &&
	    g_applauncherDragItem->m_icon->m_widget != NULL &&
	    g_applauncherDragItem->m_icon->m_widget->m_icon_mode == 2)
		applauncherIconWidgetLeaveDrag(g_applauncherDragItem->m_icon);
}

/* (0x810D1E0A) */
ApplauncherIcon::~ApplauncherIcon()
{
#if !defined(FW_TYPE_CEX)
	if (m_data->titleId == string("NPXS10998"))
		common::MainThreadCallList::Unregister(applauncherIconAppHomeUpdate, this);
#endif
	if (m_icon != NULL) {
		applauncherIconPoolFree(m_icon, false);
		m_icon = NULL;
	}

	int index = ((int (*)(const char *))((ShellPluginInterface *)applauncherPluginGetInterface(
		"topmenu_plugin", 1))->func[TOPMENU_PLUGIN_FUNC_SET_LAUNCH_TITLE])(
		m_data->titleId.c_str());

	if (index != -1 && m_removeEntry)
		((void (*)(int))((ShellPluginInterface *)applauncherPluginGetInterface(
			"topmenu_plugin", 1))->func[3])(index);
	if (m_page != NULL)
		applauncherIconDetach(this);
}

/* (0x810D1F44) */
void applauncherIconLoadImage(const char *dir, intrusive_ptr<cache::CacheImageInterface> *image)
{
	char path[2048];
	char base[2048];	/* copied, never read */

	sce_paf_strncpy(path, dir, sizeof(path));
	sce_paf_strncpy(base, dir, sizeof(base));
	sce_paf_strncat(path, "sce_sys/icon0.png", sizeof(path) - sce_paf_strlen(dir));
	applauncherIconCacheLoad(string(path), image, NULL, NULL, false);
}

/* (0x810D1FF6) */
void applauncherIconFolderCreated(ApplauncherLsdbJob *job, void *arg)
{
	CreateNewFolderJob *folderJob = (CreateNewFolderJob *)job;

	*folderJob->m_data = *folderJob->m_folder;

	common::SharedPtr<ApplauncherIcon> icon = g_applauncherHome->FindIcon(folderJob->m_data->rowid);

	if (icon.get() != NULL)
		icon->Update(true);
}

/* the image of a folder's icon from the database: the row after the key's ':' (0x810D2160) */
static common::SharedPtr<File> applauncherFolderImageOpen(const char *path, void *userdata,
							   int32_t *error)
{
	char *rowidText = sce_paf_private_strtok(path, ":");
	common::SharedPtr<File> file;

	if (rowidText != NULL) {
		SceInt64 rowid = sce_paf_atoll(rowidText + 1);
		int result = 0;

		file = SceLsdb_41185388(g_shellLsdbClient->client, &rowid, &result);
		if (error != NULL)
			*error = result;
	}
	return file;
}

/* (0x810D229A) */
void ApplauncherIcon::MarkUpdate()
{
	m_unavailable = true;
}

/* (0x810D22A2) */
void ApplauncherIcon::RequestData()
{
	applauncherPagesRequestData(m_data->titleId);
}

#if !defined(FW_TYPE_CEX)
/* (0x810CCCB4, TOOL) */
void ApplauncherIcon::SetLabel(const wstring &label)
{
	m_label = label;
	if (m_icon != NULL)
		applauncherIconWidgetSetLabel(m_icon, m_label);
}

/* (0x810CF17E, TOOL) */
static void applauncherIconAppHomeUpdate(void *arg)
{
	ApplauncherIcon *icon = static_cast<ApplauncherIcon *>(arg);

	if (icon->m_page == NULL || icon->m_page->GetWidget() == NULL)
		return;

	math::v4 slotPos = icon->GetSlotPos();
	ui::Widget *pageWidget = icon->m_page->GetWidget();
	math::v4 pagePos = pageWidget->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v4 screenPos = slotPos + pagePos;

	if (__builtin_fabsf(screenPos.extract_x()) >= 549.0f ||
	    __builtin_fabsf(screenPos.extract_y()) >= 341.0f) {
		if (icon->m_appHomeJob.get() != NULL) {
			icon->m_appHomeJob.reset();
			icon->m_image.clear();
		}
	} else if (icon->m_appHomeJob.get() == NULL) {
		if (icon->m_icon != NULL) {
			icon->m_icon->m_widget->SetDrawObjMaskBit(&icon->m_image, 0, 0);
			icon->m_icon->m_widget->SetTexture(
				g_applauncherPlugin->GetTexture(
					IDParam(APPLAUNCHER_ICON_APPHOME_FALLBACK_TEXTURE)),
				0, 0);
		}
		icon->m_appHomeJob = applauncherAppHomeCheckJobStart();
	}
}
#endif
