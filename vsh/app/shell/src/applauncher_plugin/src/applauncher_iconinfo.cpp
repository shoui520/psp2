/*
 * applauncher_iconinfo.cpp: an icon's information over the home (0x810D8ACC-0x810DAEB0): its
 * template opened in the topmenu's layer with the icon's image, and a "DispInfomationDialogJob"
 * job reading the application's database row, its license's status and free trial's license
 * through the game plugin, then filling the information's list with the items a factory makes.
 * Where the application has a sce_sys/about/right.suprx, a key sequence opens its "about this
 * game". The job's name is Sony's (contextmenu.cpp's job has the same); the file's name and the
 * other names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/string.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/w_plane.h>
#include <widget/w_plane_button.h>
#include <widget/w_list_view.h>
#include <widget/w_scene.h>
#include <graphics/surface/surface.h>
#include <inputdevice/inputdevice.h>
#include <inputdevice/inputlistener.h>
#include <timer/timer.h>
#include <job/job.h>
#include <file/localfile.h>
#include <common/shared_ptr.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../../../game/game_plugin/game_plugin.h"
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"
#include "applauncher_home.h"
#include "applauncher_mode.h"
#include "applauncher_iconinfo.h"

using namespace paf;

/* the information's template, opened in the topmenu's layer, and the ID string it is given */
#define APPLAUNCHER_INFO_TEMPLATE		0x6677A4D5
#define APPLAUNCHER_INFO_NAME			"info"

/* the template's widgets: its view, the close button, the icon's image and the list */
#define APPLAUNCHER_INFO_VIEW			0x0CD9299D
#define APPLAUNCHER_INFO_CLOSE			0xC57612A0
#define APPLAUNCHER_INFO_IMAGE			0x08CBD18B
#define APPLAUNCHER_INFO_LIST			0xAA21C75B

/* PLACEHOLDER: the key the close button is pressed with */
#define APPLAUNCHER_INFO_CLOSE_KEY		32

/* PLACEHOLDER: the list's event the view's widgets are shown on */
#define APPLAUNCHER_INFO_LIST_EVENT		0x100008

/* the input locked while the view slides out */
#define APPLAUNCHER_INFO_LOCK_DEVICES		127

/* the list's cells */
#define APPLAUNCHER_INFO_CELL_WIDTH		380.0f
#define APPLAUNCHER_INFO_CELL_HEIGHT		40.0f

/* PLACEHOLDER: the application's database value whose bit 0x400 the game's factory is told of
   (a gd or gp application's) */
#define APPLAUNCHER_INFO_KEY_FLAGS		0x5142196Bu
#define APPLAUNCHER_INFO_FLAG			0x400

/* the "#_promote_attr" bit of an application with a free trial's license */
#define APPLAUNCHER_INFO_PROMOTE_ATTR_FTT	4

/* LsdbValue's integer type */
#define APPLAUNCHER_INFO_VALUE_INTEGER		0

class ApplauncherIconInfo;

/* PLACEHOLDER: the job reading the information (360 bytes, vtable 0x8152F634) */
class ApplauncherIconInfoJob : public job::JobItem {
public:
	ApplauncherIconInfoJob(LsdbAppInfoPtr appInfo, ApplauncherIconInfo *info);
	virtual ~ApplauncherIconInfoJob();

	virtual int Run();
	virtual void Finish(int result);

	LsdbAppInfoPtr m_appInfo;
	ApplauncherIconInfo *m_info;
	bool m_hasRight;		/* the application has a sce_sys/about/right.suprx */
	GameFttInfo m_ftt;
	GameLicenseStatus m_license;
	bool m_flag;			/* APPLAUNCHER_INFO_KEY_FLAGS has APPLAUNCHER_INFO_FLAG */
	bool m_reserved;
	bool m_hasFtt;			/* the free trial's license was read */
};

/* PLACEHOLDER: an icon's information (368 bytes) */
class ApplauncherIconInfo {
public:
	ApplauncherIconInfo(const common::SharedPtr<ApplauncherIcon> &icon,
			    LsdbAppInfoPtr *appInfo);
	~ApplauncherIconInfo();

	ui::Widget *m_root;		/* the topmenu's layer the template is opened in */
	ui::Widget *m_view;
	common::SharedPtr<inputdevice::InputListener> m_listener;
	common::SharedPtr<job::JobItem> m_reserved;
	common::SharedPtr<ApplauncherIcon> m_icon;
	LsdbAppInfoPtr m_appInfo;
	ApplauncherIconInfoFactory *m_factory;
	bool m_reserved1;
	int m_dialogId;
	int m_reserved2;
	GameFttInfo m_ftt;		/* the job's, once done */
	GameLicenseStatus m_license;	/* the job's, once done */
	common::SharedPtr<job::JobItem> m_job;
};

typedef char ApplauncherIconInfoSize[sizeof(ApplauncherIconInfo) == 368 ? 1 : -1];
typedef char ApplauncherIconInfoJobSize[sizeof(ApplauncherIconInfoJob) == 360 ? 1 : -1];

/* the information open (0x815443D8) */
static ApplauncherIconInfo *s_iconInfo;

/* the widget of the parent with the ID, when it is of the type */
#define APPLAUNCHER_INFO_FIND(parent, type, id)						\
	((parent)->FindChild(IDParam(s_empty_name, (id)), 0) != NULL &&			\
	 (parent)->FindChild(IDParam(s_empty_name, (id)), 0)->IsInherit(type::TypeName()) ?	\
	 (type *)(parent)->FindChild(IDParam(s_empty_name, (id)), 0) : NULL)

/* shows and hides a widget over the timer's time (inline in paf's callers) */
static inline void ShowWidget(ui::Widget *widget, Timer *timer)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, NULL);
}

static inline void HideWidget(ui::Widget *widget, Timer *timer)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(timer != NULL ? timer->GetTotalTime() : 0.0f, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

/* the information closed: its list cleared, its factory closed, the key sequence no longer
   listened to, and the edit asked for (0x810D8ACC) */
static void applauncherIconInfoOnClose(int32_t type, ui::Handler *self, ui::Event *e,
				       void *data)
{
	ApplauncherIconInfo *info = (ApplauncherIconInfo *)data;
	ui::ListView *list = APPLAUNCHER_INFO_FIND(info->m_view, ui::ListView,
						   APPLAUNCHER_INFO_LIST);

	if (list != NULL)
		list->Clear();
	if (info->m_factory != NULL) {
		info->m_factory->m_closed = true;
		info->m_factory = NULL;
	}

	ApplauncherKeySequenceListener *listener =
		(ApplauncherKeySequenceListener *)info->m_listener.get();

	if (listener != NULL)
		listener->m_enabled = false;
	g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
}

/* the error's dialog closed (0x810D8C7C) */
static void applauncherIconInfoOnErrorClose(SceInt32 value, SceInt32 button, void *arg)
{
	applauncherIconInfoOnClose(0, NULL, NULL, arg);
}

/* the error's message dialog opened; without it, the information closed (0x810D8C8C) */
static void applauncherIconInfoShowError(ApplauncherIconInfo *info, int *dialogId, int error)
{
	if (*dialogId != -1) {
		SceCommonGuiDialog_BC311DC0(*dialogId);
		info->m_dialogId = -1;
	}

	CommonGuiMessageDialogParam param;

	param.type = error;

	int ret = SceCommonGuiDialog_A6422883(g_applauncherHome->m_plugin, &param,
		new ApplauncherDialogHandler(applauncherIconInfoOnErrorClose, info),
		new ApplauncherErrorOpenHandler());

	if (ret >= 0) {
		*dialogId = ret;
		return;
	}
	applauncherIconInfoOnClose(0, NULL, NULL, info);
}

/* (0x810D8E5A) */
ApplauncherIconInfoJob::ApplauncherIconInfoJob(LsdbAppInfoPtr appInfo, ApplauncherIconInfo *info)
	: job::JobItem("DispInfomationDialogJob", NULL), m_appInfo(appInfo), m_info(info)
{
	m_hasFtt = false;
	m_reserved = false;
	m_flag = false;
	m_hasRight = false;
	sce_paf_memset(&m_ftt, 0, sizeof(m_ftt));
	sce_paf_memset(&m_license, 0, sizeof(m_license));
}

/* the job made for the information (0x810D8ED0) */
__attribute__((noinline))
static common::SharedPtr<job::JobItem> applauncherIconInfoJobCreate(LsdbAppInfoPtr appInfo,
								   ApplauncherIconInfo *info)
{
	return common::SharedPtr<job::JobItem>(new ApplauncherIconInfoJob(appInfo, info));
}

/* the image's plane sized to the image once laid out: shown (0x810DA120) */
static void applauncherIconInfoOnImageReady(int32_t type, ui::Handler *self, ui::Event *e,
					    void *data);
/* the key sequence pressed: "about this game" opened (0x810DA1D8) */
static void applauncherIconInfoOnAboutKeys(void *arg);
/* the list's event: the view's close button focused, and the list shown (0x810D9EA0) */
static void applauncherIconInfoOnListEvent(int32_t type, ui::Handler *self, ui::Event *e,
					   void *data);

/*
 * The template opened in the topmenu's layer and slid in, its close button, the icon's image in
 * the topmenu's frame, and the job reading the information put on the queue (0x810D8FC8).
 */
ApplauncherIconInfo::ApplauncherIconInfo(const common::SharedPtr<ApplauncherIcon> &icon,
					 LsdbAppInfoPtr *appInfo)
	: m_root(NULL), m_view(NULL), m_icon(icon), m_appInfo(*appInfo), m_factory(NULL),
	  m_reserved1(false), m_dialogId(-1), m_reserved2(0)
{
	s_iconInfo = this;
	g_applauncherIconInfoAppSize = 0;
	g_applauncherIconInfoSaveDataState = 4;
	sce_paf_memset(&m_ftt, 0, sizeof(m_ftt));
	sce_paf_memset(&m_license, 0, sizeof(m_license));

	ShellPluginInterface *topmenu =
		(ShellPluginInterface *)applauncherPluginGetInterface("topmenu_plugin", 1);
	Plugin *plugin = g_applauncherHome->m_plugin;

	m_root = ((ui::Widget *(*)(int))topmenu->func[TOPMENU_PLUGIN_FUNC_HOLD])(0);
	plugin->TemplateOpen(m_root, IDParam(s_empty_name, APPLAUNCHER_INFO_TEMPLATE),
			     Plugin::TemplateOpenParam());
	m_root->m_name.SetID(string(APPLAUNCHER_INFO_NAME));

	m_view = APPLAUNCHER_INFO_FIND(m_root, ui::Plane, APPLAUNCHER_INFO_VIEW);

	int32_t width, height;

	ScePafWidget_29BCC252(m_view->m_scene, &width, &height);
	m_view->SetSize(math::v4((float)width, (float)height, 0.0f, 0.0f));
	ScePafWidget_42C5A4D4(m_view, 0, 0);
	HideWidget(m_view, NULL);
	ScePafWidget_44B22ACA(m_view, common::transition::Type_SlideFromBottom5, 0.0f, NULL, NULL);

	ui::Widget *close = APPLAUNCHER_INFO_FIND(m_view, ui::PlaneButton, APPLAUNCHER_INFO_CLOSE);

	if (close != NULL) {
		ScePafWidget_21125870(close, APPLAUNCHER_INFO_CLOSE_KEY);
		close->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
			new ui::EventCBListener(applauncherIconInfoOnClose, this));
	}

	ui::Widget *image = APPLAUNCHER_INFO_FIND(m_view, ui::Plane, APPLAUNCHER_INFO_IMAGE);

	if (image != NULL) {
		HideWidget(image, NULL);

		intrusive_ptr<graph::Surface> surface = icon->m_image.get() != NULL ?
			icon->m_image->GetImage() : intrusive_ptr<graph::Surface>();

		image->SetTexture(surface);
		image->SetTexture(*((const intrusive_ptr<graph::Surface> *(*)(void))
			topmenu->func[TOPMENU_PLUGIN_FUNC_GET_ICON_FRAME])(), 0, 1);
		if (surface.get() != NULL) {
			image->SetSize(math::v4((float)surface->GetWidth(),
						(float)surface->GetHeight(), 0.0f, 0.0f));
			image->AddEventListener(ui::Handler::CB_STATE_READY,
				new ui::EventCBListener(applauncherIconInfoOnImageReady, NULL));
		}
	}

	close = APPLAUNCHER_INFO_FIND(m_view, ui::PlaneButton, APPLAUNCHER_INFO_CLOSE);
	if (close != NULL) {
		ScePafWidget_EC6175D6(close, true);
		close->SetFocusDirectionWidget(0, close);
		close->SetFocusDirectionWidget(2, close);

		ui::Widget *list = APPLAUNCHER_INFO_FIND(m_view, ui::ListView,
							 APPLAUNCHER_INFO_LIST);

		if (list != NULL)
			close->SetFocusDirectionWidget(1, list);
	}

	m_job = applauncherIconInfoJobCreate(*appInfo, this);
	if (m_job.get() != NULL && job::JobQueue::default_queue->Enqueue(m_job) <= 0)
		return;
	applauncherIconInfoShowError(this, &m_dialogId, 0);
}

/* (0x810D9C70) */
ApplauncherIconInfo *applauncherIconInfoOpen(const common::SharedPtr<ApplauncherIcon> &icon,
					     void *context)
{
	return new ApplauncherIconInfo(icon, (LsdbAppInfoPtr *)context);
}

/* the key sequence no longer listened to, the job canceled, the view slid out with the input
   locked meanwhile (0x810D9C8C) */
ApplauncherIconInfo::~ApplauncherIconInfo()
{
	if (m_listener.get() != NULL)
		inputdevice::DelInputListener(m_listener);
	if (m_job.get() != NULL)
		m_job->Cancel();
	if (m_root != NULL) {
		float time = ScePafWidget_F1176ACC(m_root,
			common::transition::Type_SlideFromBottom5, 0.0f, NULL, NULL);

		ScePafWidget_5A9793B0(m_root, time);
		ScePafMisc_CAABF0B0(APPLAUNCHER_INFO_LOCK_DEVICES, time);
	}
	s_iconInfo = NULL;
}

/* (0x810D9E84) */
void applauncherIconInfoClose(void)
{
	if (s_iconInfo != NULL)
		delete s_iconInfo;
}

static void applauncherIconInfoOnListEvent(int32_t type, ui::Handler *self, ui::Event *e,
					   void *data)
{
	ApplauncherIconInfo *info = (ApplauncherIconInfo *)data;
	ui::Widget *widget = (ui::Widget *)self;

	if (widget == NULL || !widget->IsInherit(ui::ListView::TypeName()))
		return;

	if (!((ui::ListView *)widget)->IsTextEventEnabled() && info != NULL &&
	    info->m_view != NULL) {
		ui::Widget *close = APPLAUNCHER_INFO_FIND(info->m_view, ui::PlaneButton,
							  APPLAUNCHER_INFO_CLOSE);

		if (close != NULL) {
			close->SetFocusDirectionWidget(1, close);
			close->SetFocusDirectionWidget(3, close);
			close->SetFocus(NULL);
		}
	}
	ShowWidget(widget, new Timer(200.0f));
}

static void applauncherIconInfoOnImageReady(int32_t type, ui::Handler *self, ui::Event *e,
					    void *data)
{
	ui::Widget *widget = (ui::Widget *)self;

	if (widget != NULL && widget->IsInherit(ui::Widget::TypeName()))
		ShowWidget(widget, new Timer(200.0f));
}

static void applauncherIconInfoOnAboutKeys(void *arg)
{
	ApplauncherIconInfo *info = (ApplauncherIconInfo *)arg;

	applauncherIconInfoOnClose(0, NULL, NULL, info);
	((ApplauncherKeySequenceListener *)info->m_listener.get())->m_enabled = false;
	applauncherAboutThisGameEnter(info->m_icon);
}

/* (0x810DA1FA, 0x810DA264) */
ApplauncherIconInfoJob::~ApplauncherIconInfoJob()
{
}

/* PLACEHOLDER: the database's client, made and taken down by its exports */
struct ApplauncherIconInfoClient {
	ApplauncherIconInfoClient()
	{
		SceLsdb_D322A870(&client);
	}

	~ApplauncherIconInfoClient()
	{
		SceLsdb_56D8DCF0(&client);
	}

	AppInfoSClient client;
};

/*
 * The information read: whether the application has a sce_sys/about/right.suprx, a game's free
 * trial's license where its #_promote_attr says it has one, its license's status, and for a gd
 * or gp application whether its database value has the flag (0x810DA274).
 */
int ApplauncherIconInfoJob::Run()
{
	LsdbAppInfo::iterator end = m_appInfo->end();
	ApplauncherIconInfoClient client;
	LsdbValue value;
	string src;
	string path;
	int ret;
	LsdbAppInfo::iterator it = m_appInfo->find(LSDB_KEY_ORG_PATH);

	if (it == end) {
		ret = APPLAUNCHER_ERROR_NO_TITLE;
		goto out;
	}
	src = LsdbValueText(it->second);
	path = src + string("/sce_sys/about/right.suprx");
	if ((int)LocalFile::GetFileSize(path.c_str()) > 0)
		m_hasRight = true;
	else
		m_hasRight = false;

	{
		Plugin *plugin = Plugin::Find("game_plugin");
		GamePluginInterface *game;

		if (plugin == NULL) {
			ret = APPLAUNCHER_ERROR_NO_LIVEAREA;
			goto out;
		}
		game = (GamePluginInterface *)plugin->GetInterface(1);
		if (game == NULL) {
			ret = APPLAUNCHER_ERROR_NO_LIVEAREA;
			goto out;
		}

		if (sceLsdbGetType(&m_appInfo) == 1) {
			it = m_appInfo->find(LSDB_KEY_PROMOTE_ATTR);
			if (it != end) {
				SceInt64 attr = 0;

				if (it->second.type == APPLAUNCHER_INFO_VALUE_INTEGER)
					attr = it->second.integer;
				if (attr & APPLAUNCHER_INFO_PROMOTE_ATTR_FTT) {
					it = m_appInfo->find(LSDB_KEY_TITLE_ID);
					if (it == end) {
						ret = APPLAUNCHER_ERROR_NO_TITLE;
						goto out;
					}

					string titleId(LsdbValueText(it->second));

					ret = ((int (*)(const char *, GameFttInfo *))
					       game->func[GAME_PLUGIN_FUNC_FTT_READ])(titleId.c_str(),
										     &m_ftt);
					if (ret < 0)
						goto out;
					m_hasFtt = true;
				}
			}
		}

		sce_paf_strncpy(m_license.path, src.c_str(), sizeof(m_license.path) - 1);
		m_license.path[sizeof(m_license.path) - 1] = '\0';
		ret = ((int (*)(GameLicenseStatus *))
		       game->func[GAME_PLUGIN_FUNC_LICENSE_GET_STATUS])(&m_license);
		if (ret < 0)
			goto out;
	}

	it = m_appInfo->find(LSDB_KEY_CATEGORY);
	if (it == end) {
		ret = APPLAUNCHER_ERROR_NO_TITLE;
		goto out;
	}

	{
		string category(LsdbValueText(it->second));

		if (category == "gd" || category == "gp") {
			it = m_appInfo->find(LSDB_KEY_TITLE_ID);
			if (it == end) {
				ret = APPLAUNCHER_ERROR_NO_TITLE;
				goto out;
			}

			string titleId(LsdbValueText(it->second));

			if (SceLsdb_DB73BD7E(&client.client, &titleId, APPLAUNCHER_INFO_KEY_FLAGS,
					     &value) == 0 &&
			    value.type == APPLAUNCHER_INFO_VALUE_INTEGER &&
			    (value.integer & APPLAUNCHER_INFO_FLAG) != 0)
				m_flag = true;
			else
				m_flag = false;
		}
	}
	ret = 0;
out:
	return ret;
}

/* the keys pressed in order for "about this game" */
static const unsigned int s_aboutKeys[4] = { 0x40, 0x80, 0x100, 0x200 };

/*
 * The information read: the key sequence listened to where the application has an "about this
 * game", the list's factory made for the icon (a power icon's, or a game's or another
 * application's by its _apptype) and the list filled; on an error, its dialog (0x810DA82C).
 */
void ApplauncherIconInfoJob::Finish(int result)
{
	if (IsCanceled())
		return;

	ApplauncherIconInfo *info = m_info;
	bool power = false;

	if (info->m_icon.get() != NULL && info->m_icon->m_data.get() != NULL &&
	    info->m_icon->m_data->type == APPLAUNCHER_ICON_TYPE_POWER) {
		power = true;
	} else if (result < 0) {
		applauncherIconInfoShowError(info, &info->m_dialogId, result);
		return;
	}

	if (m_hasRight) {
		ApplauncherKeySequenceListener *listener = new ApplauncherKeySequenceListener(
			s_aboutKeys, 4, applauncherIconInfoOnAboutKeys, info);

		if (listener == NULL) {
			applauncherIconInfoShowError(info, &info->m_dialogId,
						     APPLAUNCHER_ERROR_NO_MEMORY);
			return;
		}
		info->m_listener = common::SharedPtr<inputdevice::InputListener>(listener);
		inputdevice::AddInputListener(info->m_listener);
	}

	{
		int appType = 1;
		LsdbAppInfo::iterator it = m_appInfo->find(LSDB_KEY_APPTYPE);

		if (it != m_appInfo->end())
			appType = it->second.type == APPLAUNCHER_INFO_VALUE_INTEGER ?
				  (int)it->second.integer : 0;

		if (power) {
			info->m_factory = new ApplauncherIconInfoPowerFactory(g_applauncherPlugin,
				info->m_view, info->m_icon, &info->m_appInfo);
		} else if (appType == 1) {
			info->m_ftt = m_ftt;
			info->m_license = m_license;
			info->m_factory = new ApplauncherIconInfoGameFactory(g_applauncherPlugin,
				info->m_view, info->m_icon, &info->m_appInfo, &info->m_license,
				&info->m_ftt, m_flag, m_hasFtt);
		} else if (appType == 0) {
			info->m_factory = new ApplauncherIconInfoBasicFactory(g_applauncherPlugin,
				info->m_view, info->m_icon, &info->m_appInfo);
		}
		if (info->m_factory == NULL) {
			applauncherIconInfoShowError(info, &info->m_dialogId,
						     APPLAUNCHER_ERROR_NO_MEMORY);
			return;
		}
		if (appType == 1)
			applauncherContextMenuKeepHold(true);

		ui::ListView *list = APPLAUNCHER_INFO_FIND(info->m_view, ui::ListView,
							   APPLAUNCHER_INFO_LIST);

		if (list != NULL) {
			list->Clear();
			list->SetHorizontalScrollBar(NULL);
			list->SetScrollType(ui::ListView::SCROLL_TYPE_VERTICAL);
			list->SetItemFactory(info->m_factory);
			list->AddEventListener(APPLAUNCHER_INFO_LIST_EVENT,
				new ui::EventCBListener(applauncherIconInfoOnListEvent, m_info));
			list->InsertSegment(0, 1);
			list->SetCellSizeDefault(0, math::v4(APPLAUNCHER_INFO_CELL_WIDTH,
							     APPLAUNCHER_INFO_CELL_HEIGHT, 0.0f, 0.0f));
			list->m_sync_enabled = true;
			list->SetSegmentLayoutType(0, ui::ListView::LAYOUT_TYPE_LIST_VARIABLE);
			list->InsertCell(0, 0, info->m_factory->m_itemCount);
		}
		info->m_job.reset();
	}
}
