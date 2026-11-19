/*
 * applauncher_changelog.cpp: an application's change log (0x810B891C-0x810BA444), its
 * sce_sys/changeinfo read in the "LoadChangeLog" job, from the application's directory the game
 * plugin mounts, and shown in a layout dialog over the home or over another application. The
 * file's name and the names here are ours (PLACEHOLDER) unless a comment says otherwise: no
 * record of the 3.6x shell names them; "LoadChangeLog" is Sony's job name.
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <framework.h>
#include <std/string>
#include <std/vector>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/w_rich_text.h>
#include <widget/w_busyindicator.h>
#include <timer/timer.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include <common/encoded_string.h>
#include <common/char_encoding.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../../../game/game_plugin/game_plugin.h"
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"
#include "applauncher_home.h"

using namespace paf;

extern "C" int sceErrorGetExternalString(char *buf, SceUInt32 code);
/* the application's subtype, as the game plugin tells it (0 without the plugin); not declared in
   SceLsdb's header yet */
extern "C" int SceLsdb_298134FE(const LsdbAppInfoPtr *app);

/* how the change log was opened: from an icon of the home, or over another application */
#define APPLAUNCHER_CHANGE_LOG_ICON		1
#define APPLAUNCHER_CHANGE_LOG_APP		2

/* the dialog's priority over the home, and over an application */
#define APPLAUNCHER_CHANGE_LOG_PRIORITY_ICON	40
#define APPLAUNCHER_CHANGE_LOG_PRIORITY_APP	33

/* the dialog's contents part, and its widgets: the change log's text, the error text, the busy
   indicator */
#define APPLAUNCHER_CHANGE_LOG_CONTENTS		4
#define APPLAUNCHER_CHANGE_LOG_TEXT		0xD0791765
#define APPLAUNCHER_CHANGE_LOG_ERROR		0xF60382AE
#define APPLAUNCHER_CHANGE_LOG_BUSY		0x531E99E7

/* the error texts: a network error (0x80AF5022), an empty change log, a change log not read,
   and the title over the change log of an application */
#define APPLAUNCHER_CHANGE_LOG_MSG_NETWORK	0x3D12ADF2
#define APPLAUNCHER_CHANGE_LOG_MSG_EMPTY	0xB30BDC23
#define APPLAUNCHER_CHANGE_LOG_MSG_PARSE	0x0176210C
#define APPLAUNCHER_CHANGE_LOG_MSG_TITLE	0xB64C92FB
#define APPLAUNCHER_CHANGE_LOG_NET_ERROR	APPLAUNCHER_ERROR_NOT_FOUND

/* the size of a mount point the game plugin gives */
#define APPLAUNCHER_CHANGE_LOG_MOUNT_POINT_SIZE	17

/* the change log's dialog page (resource IDs) */
static const CommonGuiDialogPage s_changeLogPage = {
	{ 0x3A918C2C, 0x438C7285, 0xB505C20A }
};

/* PLACEHOLDER: the "LoadChangeLog" job: a function run on the queue and one called when done,
   both with the change log */
class ApplauncherChangeLogJob : public job::JobItem {
public:
	typedef int (*RunFunc)(ApplauncherChangeLogJob *job, void *arg);
	typedef void (*FinishFunc)(int result, void *arg);

	ApplauncherChangeLogJob(const char *name, void *arg, RunFunc run, FinishFunc finish)
		: job::JobItem(name, NULL), m_run(run), m_finish(finish), m_reserved(0), m_arg(arg)
	{
	}

	virtual ~ApplauncherChangeLogJob()
	{
	}

	virtual int Run()
	{
		if (m_run != NULL)
			return m_run(this, m_arg);
		return 0;
	}

	virtual void Finish(int result)
	{
		if (m_finish != NULL) {
			if (IsCanceled())
				result = APPLAUNCHER_ERROR_CANCELED;
			m_finish(result, m_arg);
		}
	}

	RunFunc m_run;
	FinishFunc m_finish;
	SceUInt32 m_reserved;
	void *m_arg;
};

/* PLACEHOLDER: an application's change log in its dialog */
class ApplauncherChangeLog {
public:
	ApplauncherChangeLog(const common::SharedPtr<ApplauncherIcon> &icon);
	ApplauncherChangeLog(const string *titleId, void *loadArg,
			     ApplauncherChangeLogCallback callback, int value, void *arg);
	~ApplauncherChangeLog();

	/* the error's code as the dialogs show it: "(C1-2345-6)" */
	string GetErrorCode(int error);
	/* the error's text shown in the dialog */
	void ShowError(int error);
	/* the dialog opened and the change log's job started */
	void Open();

	ApplauncherTopmenuHold *m_hold;
	ui::BusyIndicator *m_busy;
	common::SharedPtr<job::JobItem> m_job;
	common::SharedPtr<ApplauncherIcon> m_icon;
	ApplauncherChangeInfo *m_info;
	int m_dialogId;
	int m_subType;			/* the application's subtype (SceLsdb_298134FE) */
	string m_mountPoint;
	string m_titleId;
	void *m_loadArg;
	int m_mode;
	ApplauncherChangeLogCallback m_callback;
	int m_value;
	void *m_arg;
	bool m_reserved;
	bool m_errorShown;
};

typedef char ApplauncherChangeLogSize[sizeof(ApplauncherChangeLog) == 84 ? 1 : -1];

/* PLACEHOLDER: the change log open (0x8154427C; not file-local in Sony's build: its stores stay
   in order around the calls) */
ApplauncherChangeLog *g_applauncherChangeLog;

static void applauncherChangeLogOnClose(SceInt32 value, SceInt32 button, void *arg);
static int applauncherChangeLogLoad(ApplauncherChangeLogJob *job, void *arg);
static void applauncherChangeLogOnLoaded(int result, void *arg);
static void applauncherChangeLogOnLayout(int32_t type, ui::Handler *self, ui::Event *e,
					 void *arg);

/* the widget of the parent with the ID, when it is of the type */
#define APPLAUNCHER_CHANGE_LOG_FIND(parent, type, id)					\
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

/* (0x810B891C) */
string ApplauncherChangeLog::GetErrorCode(int error)
{
	wstring text(L"(");
	char code[16];
	common::EncodedString encoded;

	sceErrorGetExternalString(code, error);
	text += ScePafMisc_53948F03(ScePafMisc_C11CB5E4(&encoded, code, 1));
	ScePafMisc_B45A7521(&encoded);
	text += L")";

	string ret;

	common::Utf16ToUtf8(text, &ret);
	return ret;
}

/* (0x810B89E0) */
void ApplauncherChangeLog::ShowError(int error)
{
	m_errorShown = true;

	ui::Widget *contents = SceCommonGuiDialog_8947FC23(m_dialogId, APPLAUNCHER_CHANGE_LOG_CONTENTS);

	if (contents == NULL)
		return;

	ui::Widget *text = contents->FindChild(IDParam(s_empty_name, APPLAUNCHER_CHANGE_LOG_ERROR), 0);
	Plugin *plugin = g_applauncherHome->m_plugin;
	string message;

	switch (error) {
	case APPLAUNCHER_CHANGE_LOG_NET_ERROR:
		common::Utf16ToUtf8(wstring(plugin->GetString(IDParam(s_empty_name,
			APPLAUNCHER_CHANGE_LOG_MSG_NETWORK))), &message);
		break;
	case APPLAUNCHER_ERROR_NO_CHANGE_INFO:
		common::Utf16ToUtf8(wstring(plugin->GetString(IDParam(s_empty_name,
			APPLAUNCHER_CHANGE_LOG_MSG_EMPTY))), &message);
		break;
	case APPLAUNCHER_ERROR_CHANGE_INFO_PARSE:
		common::Utf16ToUtf8(wstring(plugin->GetString(IDParam(s_empty_name,
			APPLAUNCHER_CHANGE_LOG_MSG_PARSE))), &message);
		break;
	default: {
		SceUInt32 id = applauncherGetErrorTextId(error);

		common::Utf16ToUtf8(wstring(plugin->GetString(IDParam(s_empty_name, id))), &message);
		if (id == APPLAUNCHER_ERROR_TEXT_GENERIC) {
			message += "\n";
			message.append(GetErrorCode(error));
		}
		break;
	}
	}

	wstring wmessage;

	common::Utf8ToUtf16(message, &wmessage, 0);
	text->SetString(wmessage);
	text->AddEventListener(ui::Handler::CB_STATE_READY,
			       new ui::EventCBListener(applauncherChangeLogOnLayout, this));
}

/* (0x810B8F06) */
void ApplauncherChangeLog::Open()
{
	g_applauncherChangeLog = this;

	CommonGuiLayoutDialogParam param;
	ShellPluginInterface *topmenu =
		(ShellPluginInterface *)applauncherPluginGetInterface("topmenu_plugin", 1);

	m_hold = ((ApplauncherTopmenuHold *(*)(int))topmenu->func[TOPMENU_PLUGIN_FUNC_HOLD])(0);
	param.resourcePlugin = g_applauncherHome->m_plugin;
	param.priority = m_mode == APPLAUNCHER_CHANGE_LOG_ICON ? APPLAUNCHER_CHANGE_LOG_PRIORITY_ICON :
								 APPLAUNCHER_CHANGE_LOG_PRIORITY_APP;
	param.dimmerColor[0] = 0.0f;
	param.dimmerColor[1] = 0.0f;
	param.dimmerColor[2] = 0.0f;
	param.dimmerColor[3] = 0.3f;
	param.layout = SceCommonGuiDialog_AF75CA6D;
	if (m_mode == APPLAUNCHER_CHANGE_LOG_APP)
		param.notifyClose = true;
	param.layout.page = &s_changeLogPage;
	param.handler = new ApplauncherDialogHandler(applauncherChangeLogOnClose, this);

	int ret = SceCommonGuiDialog_CCFCD9A6(&param);

	if (ret < 0) {
		m_dialogId = -1;
		goto error;
	}
	m_dialogId = ret;

	ui::Widget *contents = SceCommonGuiDialog_8947FC23(ret, APPLAUNCHER_CHANGE_LOG_CONTENTS);
	ui::Widget *text = contents->FindChild(IDParam(s_empty_name, APPLAUNCHER_CHANGE_LOG_TEXT), 0);

	if (text != NULL) {
		text->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		HideWidget(text, NULL);
	}

	ui::Widget *error = contents->FindChild(IDParam(s_empty_name, APPLAUNCHER_CHANGE_LOG_ERROR), 0);

	if (error != NULL)
		HideWidget(error, NULL);

	m_busy = APPLAUNCHER_CHANGE_LOG_FIND(contents, ui::BusyIndicator, APPLAUNCHER_CHANGE_LOG_BUSY);
	if (m_busy != NULL)
		m_busy->Start();

	m_info = new ApplauncherChangeInfo();
	if (m_info != NULL) {
		m_job = common::SharedPtr<job::JobItem>(new ApplauncherChangeLogJob("LoadChangeLog",
			this, applauncherChangeLogLoad, applauncherChangeLogOnLoaded));
		if (m_job.get() != NULL) {
			ret = job::JobQueue::default_queue->Enqueue(m_job);
			if (ret >= 0)
				return;
		}
	}
error:
	ShowError(ret);
}

/* (0x810B9552) */
__attribute__((noinline)) ApplauncherChangeLog::ApplauncherChangeLog(const common::SharedPtr<ApplauncherIcon> &icon)
	: m_hold(NULL), m_busy(NULL), m_job(), m_icon(icon), m_info(NULL), m_dialogId(-1),
	  m_subType(0), m_loadArg(NULL), m_mode(APPLAUNCHER_CHANGE_LOG_ICON), m_callback(NULL),
	  m_value(-1), m_arg(NULL), m_reserved(false), m_errorShown(false)
{
	m_titleId = icon->m_data->titleId;
	Open();
}

/* (0x810B95CA) */
ApplauncherChangeLog *applauncherChangeLogOpenIcon(const common::SharedPtr<ApplauncherIcon> &icon)
{
	return new ApplauncherChangeLog(icon);
}

/* (0x810B95E0) */
__attribute__((noinline)) ApplauncherChangeLog::ApplauncherChangeLog(const string *titleId, void *loadArg,
					   ApplauncherChangeLogCallback callback, int value,
					   void *arg)
	: m_hold(NULL), m_busy(NULL), m_job(), m_icon(), m_info(NULL), m_dialogId(-1),
	  m_subType(0)
{
	m_titleId = *titleId;
	m_loadArg = loadArg;
	m_mode = APPLAUNCHER_CHANGE_LOG_APP;
	m_callback = callback;
	m_value = value;
	m_arg = arg;
	m_reserved = false;
	m_errorShown = false;
	Open();
}

/* (0x810B965A) */
int applauncherChangeLogOpen(const string *titleId, void *loadArg,
			     ApplauncherChangeLogCallback callback, int value, void *arg)
{
	new ApplauncherChangeLog(titleId, loadArg, callback, value, arg);
	return g_applauncherChangeLog->m_dialogId;
}

/* (0x810B9696) */
ApplauncherChangeLog::~ApplauncherChangeLog()
{
	if (m_job.get() != NULL)
		m_job->Cancel();

	if (m_mountPoint.length() != 0) {
		GamePluginInterface *game =
			(GamePluginInterface *)applauncherPluginGetInterface("game_plugin", 1);

		if (game != NULL)
			((int (*)(const char *))game->func[GAME_PLUGIN_FUNC_APP_DIR_UNMOUNT])(
				m_mountPoint.c_str());
	}

	if (m_dialogId != -1) {
		SceCommonGuiDialog_BC311DC0(m_dialogId);
		m_dialogId = -1;
	}
	if (m_busy != NULL)
		m_busy->Stop();

	ui::Widget *contents = SceCommonGuiDialog_8947FC23(m_dialogId,
							   APPLAUNCHER_CHANGE_LOG_CONTENTS);

	if (contents != NULL) {
		ui::Widget *text =
			contents->FindChild(IDParam(s_empty_name, APPLAUNCHER_CHANGE_LOG_TEXT), 0);

		if (text != NULL)
			text->DeleteEventCallback(ui::RichText::CB_RICH_TEXT_FINISH_LAYOUT,
						  applauncherChangeLogOnLayout, NULL);
	}

	if (m_hold != NULL)
		m_hold->vtable[APPLAUNCHER_TOPMENU_HOLD_RELEASE](m_hold);

	delete m_info;
	m_info = NULL;
	g_applauncherChangeLog = NULL;
}

/* (0x810B9882) */
void applauncherChangeLogClose(void)
{
	if (g_applauncherChangeLog != NULL)
		delete g_applauncherChangeLog;
}

/* the dialog closed: let go of over an application, the open one closed and its callback
   called; over the home, the home's request held (0x810B989E) */
static void applauncherChangeLogOnClose(SceInt32 value, SceInt32 button, void *arg)
{
	ApplauncherChangeLog *log = (ApplauncherChangeLog *)arg;

	if (value != -1)
		SceCommonGuiDialog_BC311DC0(value);
	log->m_dialogId = -1;

	if (log->m_mode == APPLAUNCHER_CHANGE_LOG_APP) {
		if (button == 0)
			applauncherChangeLogClose();
		else if (log->m_callback != NULL)
			log->m_callback(&log->m_titleId, log->m_value, log->m_arg, 1);
	} else {
		ApplauncherModeManager *state = g_applauncherModeManager;

		if (state->m_currentMode != -1)
			state->m_nextMode = 1;
	}
}

/*
 * The job: the application's information read, its directory mounted by the game plugin, and its
 * change information read from it (0x810B9902).
 */
static int applauncherChangeLogLoad(ApplauncherChangeLogJob *job, void *arg)
{
	ApplauncherChangeLog *log = (ApplauncherChangeLog *)arg;
	LsdbAppInfoPtr info(new LsdbAppInfo());
	string dir;
	int language = Framework::Instance()->GetLocale();
	void *loadArg = NULL;
	int ret;

	if (log->m_loadArg != NULL)
		loadArg = log->m_loadArg;

	string text;
	AppInfoQClient *client = new AppInfoQClient;

	if (client == NULL || (client = SceLsdb_A6D8772C(client)) == NULL) {
		ret = APPLAUNCHER_ERROR_NO_MEMORY;
		goto out;
	}
	ret = SceLsdb_E90B81CD(client);
	if (ret < 0) {
		operator delete(SceLsdb_E2101B03(client));
		goto out;
	}
	ret = SceLsdb_D239031A(client, &log->m_titleId, &info);
	if (ret < 0) {
		operator delete(SceLsdb_E2101B03(client));
		goto out;
	}
	operator delete(SceLsdb_E2101B03(client));

	log->m_subType = SceLsdb_298134FE(&info);
	{
		LsdbAppInfo::iterator it = info->find(LSDB_KEY_ORG_PATH);

		if (it == info->end()) {
			ret = APPLAUNCHER_ERROR_NO_TITLE;
			goto out;
		}

		GamePluginInterface *game =
			(GamePluginInterface *)applauncherPluginGetInterface("game_plugin", 1);

		if (game != NULL) {
			char mountPoint[APPLAUNCHER_CHANGE_LOG_MOUNT_POINT_SIZE];

			if (job->IsCanceled())
				goto out;
			ret = ((int (*)(const char *, char *, SceSize))
			       game->func[GAME_PLUGIN_FUNC_APP_DIR_MOUNT])(
				it->second.type == LSDB_VALUE_TEXT && it->second.text != NULL ?
					it->second.text->c_str() : "",
				mountPoint, APPLAUNCHER_CHANGE_LOG_MOUNT_POINT_SIZE);
			if (ret < 0 || job->IsCanceled())
				goto out;
			log->m_mountPoint = mountPoint;
			dir = log->m_mountPoint + string("/sce_sys/changeinfo");
		}
	}

	if (!job->IsCanceled()) {
		ret = log->m_info->Load(job, dir.c_str(), language, loadArg);
		if (ret >= 0) {
			log->m_info->GetText(&text);
			if (text.length() != 0)
				ret = 0;
			else
				ret = APPLAUNCHER_ERROR_NO_CHANGE_INFO;
		}
	}

out:
	if (job->IsCanceled())
		return APPLAUNCHER_ERROR_CANCELED;
	return ret;
}

/*
 * The job done: the change information shown in the RichText (under the title's heading over an
 * application), or the error (0x810B9D34).
 */
static void applauncherChangeLogOnLoaded(int result, void *arg)
{
	ApplauncherChangeLog *log = (ApplauncherChangeLog *)arg;

	if (result == APPLAUNCHER_ERROR_CANCELED)
		return;

	log->m_job.reset();

	ui::Widget *contents = SceCommonGuiDialog_8947FC23(log->m_dialogId,
							   APPLAUNCHER_CHANGE_LOG_CONTENTS);
	ui::RichText *view = (ui::RichText *)contents->FindChild(
		IDParam(s_empty_name, APPLAUNCHER_CHANGE_LOG_TEXT), 0);

	if (view != NULL) {
		view->SetVerticalBarMargin(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		view->m_margin_width = 10.0f;
	}

	if (result >= 0) {
		string text;

		log->m_info->GetText(&text);
		if (log->m_mode == APPLAUNCHER_CHANGE_LOG_APP) {
			string title;

			common::Utf16ToUtf8(wstring(g_applauncherHome->m_plugin->GetString(
				IDParam(s_empty_name, APPLAUNCHER_CHANGE_LOG_MSG_TITLE))), &title);
			title = string("<font size=1>") + title + string("</font>");
			title += "<br>";
			text = title + text;
			view->ScePafWidget_6A60296B(1, 28.0f);
		}
		view->SetCText(text.c_str(), sce_paf_strlen(text.c_str()));
		view->AddEventListener(ui::RichText::CB_RICH_TEXT_FINISH_LAYOUT,
				       new ui::EventCBListener(applauncherChangeLogOnLayout, log));
	} else {
		if ((log->m_subType == 1 || log->m_subType == 2) &&
		    result == SCE_ERROR_ERRNO_ENODEV)
			result = APPLAUNCHER_ERROR_NO_CHANGE_INFO_DIR;
		log->ShowError(result);
	}

	delete log->m_info;
	log->m_info = NULL;
}

/* the text laid out: put back in its parent and shown over 200 ms, the busy indicator stopped
   (0x810BA314) */
static void applauncherChangeLogOnLayout(int32_t type, ui::Handler *self, ui::Event *e,
					 void *arg)
{
	ApplauncherChangeLog *log = (ApplauncherChangeLog *)arg;
	ui::Widget *widget = (ui::Widget *)self;

	if (widget == NULL)
		return;

	widget->SetParent(widget->m_parent, NULL, 6);
	ShowWidget(widget, new Timer(200.0f));
	if (log != NULL && log->m_busy != NULL)
		log->m_busy->Stop();
}
