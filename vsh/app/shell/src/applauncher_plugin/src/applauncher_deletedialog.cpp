/*
 * applauncher_deletedialog.cpp: an icon's deletion over the home (0x810D58B4-0x810D808C): the
 * application checked in a "DeleteDialog" job (its save data not in use, the application it
 * was started from not running), a dialog asking whether to delete it with its image, name and
 * a message for where it is installed, and once confirmed, a waiting dialog while a second job
 * closes the application and has the game plugin delete it. The job's name is Sony's; the
 * file's name and the other names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <appmgr.h>
#include <plugin.h>
#include <std/string>
#include <std/string.h>
#include <std/stdlib.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/w_plane.h>
#include <widget/w_text.h>
#include <widget/w_busyindicator.h>
#include <graphics/surface/surface.h>
#include <timer/timer.h>
#include <thread/thread.h>
#include <job/job.h>
#include <file/localfile.h>
#include <common/shared_ptr.h>
#include <common/char_encoding.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../../../game/game_plugin/game_plugin.h"
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"
#include "applauncher_home.h"
#include "applauncher_mode.h"

using namespace paf;

/* the dialogs' priority over the home */
#define APPLAUNCHER_DELETE_PRIORITY		40

/* the confirm dialog's contents part, and its widgets: the icon's image, its name, the message
   and the busy indicator */
#define APPLAUNCHER_DELETE_CONTENTS		4
#define APPLAUNCHER_DELETE_IMAGE		0x9DA17ACA
#define APPLAUNCHER_DELETE_NAME			0x2902A586
#define APPLAUNCHER_DELETE_MESSAGE		0xD0791765
#define APPLAUNCHER_DELETE_BUSY			0x531E99E7

/* the waiting dialog's text ("deleting") */
#define APPLAUNCHER_DELETE_DELETING		0x632B6733

/* the confirm dialog's messages: a PSP game's (with or without its save data), a game card's
   (its data only), and an application's (with ux0:umass files or without) */
#define APPLAUNCHER_DELETE_MSG_PSP_SAVEDATA	0xDE6EF897
#define APPLAUNCHER_DELETE_MSG_PSP		0x73A4CFC1
#define APPLAUNCHER_DELETE_MSG_GC_DATA		0x2F039495
#define APPLAUNCHER_DELETE_MSG_GC		0x98BB2925
#define APPLAUNCHER_DELETE_MSG_UMASS		0xDF34D480
#define APPLAUNCHER_DELETE_MSG_APP		0xBD0DD6F9

/* the confirm dialog's button that deletes, and the one the waiting dialog closes with when
   its message dialog does not open */
#define APPLAUNCHER_DELETE_BUTTON_OK		2
#define APPLAUNCHER_DELETE_BUTTON_ERROR		3

/* PLACEHOLDER: a PSP game's ATTRIBUTE bit for its save data, a game card application's for its
   data */
#define APPLAUNCHER_DELETE_ATTR_PSP_SAVEDATA	0x200000
#define APPLAUNCHER_DELETE_ATTR_GC_DATA		1

/* the game plugin's category kind of a PlayStation Vita application */
#define APPLAUNCHER_DELETE_KIND_APP		1

/* LsdbValue's integer type */
#define APPLAUNCHER_DELETE_VALUE_INTEGER	0

/* PLACEHOLDER: the "REFERER_TITLE_ID" parameter, the application this one was started from */
#define LSDB_KEY_REFERER_TITLE_ID		0x9F130D41u
/* "INSTALL_DIR_SAVEDATA" (also game_appinfo.h's) */
#define APPLAUNCHER_DELETE_KEY_SAVEDATA_DIR	0x10954174u

/* the application's save data directory name taken from its INSTALL_DIR_SAVEDATA */
#define APPLAUNCHER_DELETE_SAVEDATA_DIR_LEN	9

/* an application asked to close: checked every half second for a minute */
#define APPLAUNCHER_DELETE_WAIT_INTERVAL	500
#define APPLAUNCHER_DELETE_WAIT_COUNT		120

/* PLACEHOLDER: the deletion's errors: the application deleted while the context menu's value is
   set (0x80103300), an application still running (0x80103305, 0x80103309), the category kind
   not given (0x8010330D), and a game card's application, the card gone (0x8010330E) */
#define APPLAUNCHER_ERROR_DELETE_VALUE		((int)0x80103300)
#define APPLAUNCHER_ERROR_DELETE_TIMEOUT	((int)0x80103305)
#define APPLAUNCHER_ERROR_DELETE_RUNNING	((int)0x80103309)
#define APPLAUNCHER_ERROR_DELETE_KIND		((int)0x8010330D)
#define APPLAUNCHER_ERROR_DELETE_NO_CARD	((int)0x8010330E)
/* PLACEHOLDER: what the game plugin gives without the game card (value only), what LocalFile
   gives for gro0: then (value only), and AppMgr's for save data it cannot read (value only) */
#define APPLAUNCHER_DELETE_GC_GONE		((int)0x8032001A)
#define APPLAUNCHER_DELETE_GRO0_GONE		((int)0x80AF5024)
#define APPLAUNCHER_DELETE_SAVEDATA_ERROR	((int)0x80800017)

/* the confirm dialog's page (resource IDs) */
static const CommonGuiDialogPage s_deletePage = {
	{ 0x3A918C2C, 0x438C7285, 0xFEA43112 }
};

/* PLACEHOLDER: a "DeleteDialog" job: a function run on the queue and one called when done, both
   with the argument (its code folds with ApplauncherAboutThisGameJob's; its vtable is its own,
   0x8152F27C) */
class ApplauncherDeleteDialogJob : public job::JobItem {
public:
	typedef int (*RunFunc)(ApplauncherDeleteDialogJob *job, void *arg);
	typedef void (*FinishFunc)(int result, void *arg);

	ApplauncherDeleteDialogJob(const char *name, void *arg, RunFunc run, FinishFunc finish)
		: job::JobItem(name, NULL), m_run(run), m_finish(finish), m_reserved(0), m_arg(arg)
	{
	}

	virtual ~ApplauncherDeleteDialogJob()
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

/* PLACEHOLDER: an icon's deletion over the home (64 bytes) */
class ApplauncherDeleteDialog {
public:
	ApplauncherDeleteDialog(const common::SharedPtr<ApplauncherIcon> &icon, void *context,
				int value);
	~ApplauncherDeleteDialog();

	bool m_installed;		/* the application's path is there */
	bool m_unusedFlag;
	ApplauncherTopmenuHold *m_hold;
	ui::BusyIndicator *m_busy;
	common::SharedPtr<job::JobItem> m_job;
	common::SharedPtr<ApplauncherIcon> m_icon;
	int m_dialogId;
	int m_unusedState;
	const LsdbAppInfoPtr *m_appInfo;	/* the context menu's, kept by it */
	int m_value;			/* the context menu's */
	int m_unusedIndex;
	wstring m_unusedText;
	bool m_umassFiles;		/* ux0:umass holds files of the application */
};

typedef char ApplauncherDeleteDialogSize[sizeof(ApplauncherDeleteDialog) == 64 ? 1 : -1];

/* PLACEHOLDER: the deletion open (0x815443D0) */
ApplauncherDeleteDialog *g_applauncherDeleteDialog;

static void applauncherDeleteDialogOnClose(SceInt32 value, SceInt32 button, void *arg);
static int applauncherDeleteDialogCheck(ApplauncherDeleteDialogJob *job, void *arg);
static void applauncherDeleteDialogOnChecked(int result, void *arg);
static int applauncherDeleteDialogDelete(ApplauncherDeleteDialogJob *job, void *arg);
static void applauncherDeleteDialogOnDeleted(int result, void *arg);

/* the widget of the parent with the ID, when it is of the type */
#define APPLAUNCHER_DELETE_FIND(parent, type, id)					\
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

/* the edit asked for */
static inline void applauncherDeleteDialogEndEdit(void)
{
	g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
}

static void applauncherDeleteDialogShowError(ApplauncherDeleteDialog *dialog, int *dialogId,
					     int error);

/*
 * The confirm dialog closed: on its delete button, the waiting dialog opened and the delete job
 * put on the game plugin's queue; otherwise (or when any of it fails), the edit asked for
 * (0x810D58B4).
 */
static void applauncherDeleteDialogOnConfirm(SceInt32 value, SceInt32 button, void *arg)
{
	ApplauncherDeleteDialog *dialog = (ApplauncherDeleteDialog *)arg;

	if (value != -1) {
		SceCommonGuiDialog_BC311DC0(value);
		dialog->m_dialogId = -1;
	}
	do {
		if (button != APPLAUNCHER_DELETE_BUTTON_OK)
			break;

		CommonGuiLayoutDialogParam param;

		param.priority = APPLAUNCHER_DELETE_PRIORITY;
		param.text = g_applauncherHome->m_plugin->GetString(
			IDParam(s_empty_name, APPLAUNCHER_DELETE_DELETING));
		param.layout = SceCommonGuiDialog_2A68B4A4;
		param.dimmerColor[0] = 0.0f;
		param.dimmerColor[1] = 0.0f;
		param.dimmerColor[2] = 0.0f;
		param.dimmerColor[3] = 0.3f;

		int ret = SceCommonGuiDialog_CCFCD9A6(&param);

		if (ret < 0) {
			dialog->m_dialogId = -1;
			break;
		}
		dialog->m_dialogId = ret;

		Plugin *plugin = Plugin::Find("game_plugin");

		if (plugin == NULL)
			break;

		GamePluginInterface *game = (GamePluginInterface *)plugin->GetInterface(1);

		if (game == NULL)
			break;

		job::JobQueue *queue =
			((job::JobQueue *(*)(void))game->func[GAME_PLUGIN_FUNC_GET_JOB_QUEUE])();

		if (queue == NULL)
			break;

		dialog->m_job = common::SharedPtr<job::JobItem>(new ApplauncherDeleteDialogJob(
			"DeleteDialog", dialog, applauncherDeleteDialogDelete,
			applauncherDeleteDialogOnDeleted));
		if (dialog->m_job.get() == NULL)
			break;
		if (queue->Enqueue(dialog->m_job) < 0)
			break;
		return;
	} while (0);

	applauncherDeleteDialogEndEdit();
}

/* the dialog closed and the error's message dialog opened instead; without it, the waiting
   dialog's close at once (0x810D5DEA) */
static void applauncherDeleteDialogShowError(ApplauncherDeleteDialog *dialog, int *dialogId,
					     int error)
{
	if (*dialogId != -1) {
		SceCommonGuiDialog_BC311DC0(*dialogId);
		dialog->m_dialogId = -1;
	}

	CommonGuiMessageDialogParam param;

	param.priority = APPLAUNCHER_DELETE_PRIORITY;
	param.type = error;

	int ret = SceCommonGuiDialog_A6422883(g_applauncherHome->m_plugin, &param,
		new ApplauncherDialogHandler(applauncherDeleteDialogOnClose, dialog),
		new ApplauncherErrorOpenHandler());

	if (ret >= 0) {
		*dialogId = ret;
		return;
	}
	applauncherDeleteDialogOnConfirm(*dialogId, APPLAUNCHER_DELETE_BUTTON_ERROR, dialog);
}

/* (0x810D5FBE) */
ApplauncherDeleteDialog::ApplauncherDeleteDialog(const common::SharedPtr<ApplauncherIcon> &icon,
						 void *context, int value)
	: m_installed(false), m_unusedFlag(false), m_hold(NULL), m_busy(NULL), m_job(), m_icon(icon),
	  m_dialogId(-1), m_unusedState(0), m_appInfo((const LsdbAppInfoPtr *)context), m_value(value),
	  m_unusedIndex(-1), m_umassFiles(false)
{
	g_applauncherDeleteDialog = this;

	ShellPluginInterface *topmenu =
		(ShellPluginInterface *)applauncherPluginGetInterface("topmenu_plugin", 1);

	m_hold = ((ApplauncherTopmenuHold *(*)(int))topmenu->func[TOPMENU_PLUGIN_FUNC_HOLD])(0);

	int ret;

	m_job = common::SharedPtr<job::JobItem>(new ApplauncherDeleteDialogJob("DeleteDialog",
		this, applauncherDeleteDialogCheck, applauncherDeleteDialogOnChecked));
	if (m_job.get() != NULL) {
		ret = job::JobQueue::default_queue->Enqueue(m_job);
		if (ret >= 0)
			return;
	} else {
		ret = APPLAUNCHER_ERROR_NO_LIVEAREA;
	}
	applauncherDeleteDialogShowError(this, &m_dialogId, ret);
}

/* (0x810D61D0) */
ApplauncherDeleteDialog *applauncherDeleteDialogOpen(
	const common::SharedPtr<ApplauncherIcon> &icon, void *context, int value)
{
	return new ApplauncherDeleteDialog(icon, context, value);
}

/* (0x810D61EE) */
ApplauncherDeleteDialog::~ApplauncherDeleteDialog()
{
	if (m_dialogId != -1) {
		SceCommonGuiDialog_BC311DC0(m_dialogId);
		m_dialogId = -1;
	}
	if (m_busy != NULL)
		m_busy->Stop();
	if (m_job.get() != NULL)
		m_job->Cancel();
	if (m_hold != NULL)
		m_hold->vtable[APPLAUNCHER_TOPMENU_HOLD_RELEASE](m_hold);
	g_applauncherDeleteDialog = NULL;
}

/* (0x810D62EA) */
void applauncherDeleteDialogClose(void)
{
	if (g_applauncherDeleteDialog != NULL)
		delete g_applauncherDeleteDialog;
}

/* a text value of the application's, "" without it */
static inline const char *applauncherDeleteDialogText(LsdbAppInfo::iterator it)
{
	return it->second.type == LSDB_VALUE_TEXT && it->second.text != NULL ?
	       it->second.text->c_str() : "";
}

/*
 * The application deleted by the game plugin, and taken out of every user's PlayStation Plus
 * lists; not while the context menu's value is set (0x810D6306).
 */
static __attribute__((noinline)) int applauncherDeleteDialogDeleteApp(
	ApplauncherDeleteDialog *dialog)
{
	Plugin *plugin = Plugin::Find("game_plugin");

	if (plugin == NULL)
		return APPLAUNCHER_ERROR_NO_LIVEAREA;

	GamePluginInterface *game = (GamePluginInterface *)plugin->GetInterface(1);

	if (game == NULL)
		return APPLAUNCHER_ERROR_NO_LIVEAREA;

	LsdbAppInfo::iterator it = (*dialog->m_appInfo)->find(LSDB_KEY_TITLE_ID);

	if (it == (*dialog->m_appInfo)->end())
		return APPLAUNCHER_ERROR_NO_TITLE;

	string titleId(applauncherDeleteDialogText(it));

	if (dialog->m_value == 0) {
		int ret = ((int (*)(const char *, int, int, int, int))
			   game->func[GAME_PLUGIN_FUNC_APP_DELETE_PART])(titleId.c_str(), 0, 0, 0, 0);

		if (ret < 0)
			return ret;

		((int (*)(const char *))game->func[GAME_PLUGIN_FUNC_PLUS_SD_DELETE])(
			titleId.c_str());
		((int (*)(const char *, SceUInt8))game->func[GAME_PLUGIN_FUNC_PLUS_APP_STATUS])(
			titleId.c_str(), 0);
		return 0;
	}
	return APPLAUNCHER_ERROR_DELETE_VALUE;
}

/* the path holds the text (Sony's find with the text's length) */
static inline bool applauncherDeleteDialogPathHas(const string &path, const char *text)
{
	return path.find(text, 0, sce_paf_strlen(text)) != string::npos;
}

/* the confirm dialog's message, for where the application is installed and what it leaves
   (0x810D644E) */
static void applauncherDeleteDialogGetMessage(ApplauncherDeleteDialog *dialog, IDParam *message)
{
	LsdbAppInfo::iterator end = (*dialog->m_appInfo)->end();
	LsdbAppInfo::iterator it = (*dialog->m_appInfo)->find(LSDB_KEY_ORG_PATH);

	if (it == end)
		return;

	string path(applauncherDeleteDialogText(it));

	it = (*dialog->m_appInfo)->find(LSDB_KEY_ATTRIBUTE);

	int attr = 0;

	if (it != end) {
		SceInt64 value = 0;

		if (it->second.type == APPLAUNCHER_DELETE_VALUE_INTEGER)
			value = it->second.integer;
		attr = (int)value;
	}

	if (applauncherDeleteDialogPathHas(path, "ux0:pspemu/PSP/GAME")) {
		if (attr & APPLAUNCHER_DELETE_ATTR_PSP_SAVEDATA)
			message->m_idhash = APPLAUNCHER_DELETE_MSG_PSP_SAVEDATA;
		else
			message->m_idhash = APPLAUNCHER_DELETE_MSG_PSP;
	} else if (applauncherDeleteDialogPathHas(path, "gro0:")) {
		if (attr & APPLAUNCHER_DELETE_ATTR_GC_DATA)
			message->m_idhash = APPLAUNCHER_DELETE_MSG_GC_DATA;
		else if (dialog->m_installed)
			message->m_idhash = APPLAUNCHER_DELETE_MSG_GC_DATA;
		else
			message->m_idhash = APPLAUNCHER_DELETE_MSG_GC;
	} else {
		if (dialog->m_umassFiles)
			message->m_idhash = APPLAUNCHER_DELETE_MSG_UMASS;
		else
			message->m_idhash = APPLAUNCHER_DELETE_MSG_APP;
	}
}

/* the icon's image for a plane of the dialog */
static inline intrusive_ptr<graph::Surface> applauncherDeleteDialogImage(ApplauncherIcon *icon)
{
	return icon->m_image.get() == NULL ? intrusive_ptr<graph::Surface>() :
	       icon->m_image->GetImage();
}

/*
 * The application checked: the confirm dialog opened with the icon's image, its name and the
 * message hidden and the busy indicator turning, then set and shown together (0x810D6696).
 */
static void applauncherDeleteDialogShowConfirm(ApplauncherDeleteDialog *dialog)
{
	CommonGuiLayoutDialogParam param;

	param.resourcePlugin = g_applauncherHome->m_plugin;
	param.priority = APPLAUNCHER_DELETE_PRIORITY;
	param.dimmerColor[0] = 0.0f;
	param.dimmerColor[1] = 0.0f;
	param.dimmerColor[2] = 0.0f;
	param.dimmerColor[3] = 0.3f;
	param.layout = SceCommonGuiDialog_92752DE2;
	param.layout.page = &s_deletePage;
	param.handler = new ApplauncherDialogHandler(applauncherDeleteDialogOnConfirm, dialog);

	int ret = SceCommonGuiDialog_CCFCD9A6(&param);

	if (ret < 0) {
		dialog->m_dialogId = -1;
		applauncherDeleteDialogShowError(dialog, &dialog->m_dialogId, ret);
		return;
	}
	dialog->m_dialogId = ret;

	ui::Widget *contents = SceCommonGuiDialog_8947FC23(ret, APPLAUNCHER_DELETE_CONTENTS);

	if (contents != NULL) {
		ui::Widget *image = APPLAUNCHER_DELETE_FIND(contents, ui::Plane,
							    APPLAUNCHER_DELETE_IMAGE);

		if (image != NULL) {
			HideWidget(image, NULL);

			intrusive_ptr<graph::Surface> surface =
				applauncherDeleteDialogImage(dialog->m_icon.get());

			image->SetTexture(surface);

			ShellPluginInterface *topmenu = (ShellPluginInterface *)
				applauncherPluginGetInterface("topmenu_plugin", 1);

			image->SetTexture(*((const intrusive_ptr<graph::Surface> *(*)(void))
				topmenu->func[TOPMENU_PLUGIN_FUNC_GET_ICON_FRAME])(), 0, 1);
		}

		ui::Widget *name = APPLAUNCHER_DELETE_FIND(contents, ui::Text,
							   APPLAUNCHER_DELETE_NAME);

		if (name != NULL)
			HideWidget(name, NULL);

		ui::Widget *text = APPLAUNCHER_DELETE_FIND(contents, ui::Text,
							   APPLAUNCHER_DELETE_MESSAGE);

		if (text != NULL)
			HideWidget(text, NULL);

		dialog->m_busy = APPLAUNCHER_DELETE_FIND(contents, ui::BusyIndicator,
							 APPLAUNCHER_DELETE_BUSY);
		if (dialog->m_busy != NULL)
			dialog->m_busy->Start();
	}

	IDParam message(s_empty_name, 0);

	applauncherDeleteDialogGetMessage(dialog, &message);

	SceCommonGuiDialog_8947FC23(dialog->m_dialogId, APPLAUNCHER_DELETE_CONTENTS);
	contents = SceCommonGuiDialog_8947FC23(dialog->m_dialogId, APPLAUNCHER_DELETE_CONTENTS);

	ui::Widget *image = APPLAUNCHER_DELETE_FIND(contents, ui::Plane, APPLAUNCHER_DELETE_IMAGE);

	if (image != NULL)
		image->SetTexture(applauncherDeleteDialogImage(dialog->m_icon.get()));

	ui::Widget *name = APPLAUNCHER_DELETE_FIND(contents, ui::Text, APPLAUNCHER_DELETE_NAME);

	if (name != NULL) {
		const char *title = dialog->m_icon->m_data->title.c_str();

		if (title != NULL) {
			wstring label;

			common::Utf8ToUtf16(string(title), &label, 0);
			name->SetString(label);
		}
	}

	ui::Widget *text = APPLAUNCHER_DELETE_FIND(contents, ui::Text, APPLAUNCHER_DELETE_MESSAGE);

	if (text != NULL)
		text->SetString(wstring(g_applauncherHome->m_plugin->GetString(message)));

	image = contents->FindChild(IDParam(s_empty_name, APPLAUNCHER_DELETE_IMAGE), 0);
	name = contents->FindChild(IDParam(s_empty_name, APPLAUNCHER_DELETE_NAME), 0);
	text = contents->FindChild(IDParam(s_empty_name, APPLAUNCHER_DELETE_MESSAGE), 0);

	if (image != NULL && name != NULL && text != NULL) {
		ShowWidget(image, new Timer(200.0f));
		ShowWidget(name, new Timer(200.0f));
		ShowWidget(text, new Timer(200.0f));
		if (dialog->m_busy != NULL)
			dialog->m_busy->Stop();
	}
}

/*
 * The check job: an application's save data not in use and the application it was started
 * from not running, whether ux0:umass holds its files, and whether its path is there
 * (0x810D770C).
 */
static int applauncherDeleteDialogCheck(ApplauncherDeleteDialogJob *job, void *arg)
{
	ApplauncherDeleteDialog *dialog = (ApplauncherDeleteDialog *)arg;
	LsdbAppInfo::iterator end = (*dialog->m_appInfo)->end();
	GamePluginInterface *game =
		(GamePluginInterface *)applauncherPluginGetInterface("game_plugin", 1);

	if (game == NULL)
		return APPLAUNCHER_ERROR_NO_LIVEAREA;

	int kind;

	if (((int (*)(const LsdbAppInfoPtr *, int *))game->func[GAME_PLUGIN_FUNC_GET_CATEGORY_KIND])(
		    dialog->m_appInfo, &kind) < 0)
		return APPLAUNCHER_ERROR_DELETE_KIND;

	LsdbAppInfo::iterator it = (*dialog->m_appInfo)->find(LSDB_KEY_ORG_PATH);

	if (it == end)
		return APPLAUNCHER_ERROR_NO_TITLE;

	string path(applauncherDeleteDialogText(it));

	if (kind == APPLAUNCHER_DELETE_KIND_APP) {
		it = (*dialog->m_appInfo)->find(LSDB_KEY_ATTRIBUTE);

		int attr = 0;

		if (it != end) {
			SceInt64 value = 0;

			if (it->second.type == APPLAUNCHER_DELETE_VALUE_INTEGER)
				value = it->second.integer;
			attr = (int)value;
		}

		it = (*dialog->m_appInfo)->find(APPLAUNCHER_DELETE_KEY_SAVEDATA_DIR);
		if (it == end)
			return APPLAUNCHER_ERROR_NO_TITLE;

		string savedataDir(applauncherDeleteDialogText(it));

		it = (*dialog->m_appInfo)->find(LSDB_KEY_REFERER_TITLE_ID);
		if (it != end) {
			char status[128];

			sce_paf_memset(status, 0, sizeof(status));
			if (sceAppMgrGetStatusByName(applauncherDeleteDialogText(it), status) == 0)
				return APPLAUNCHER_ERROR_DELETE_RUNNING;
		}

		it = (*dialog->m_appInfo)->find(LSDB_KEY_TITLE_ID);
		if (it == end)
			return APPLAUNCHER_ERROR_NO_TITLE;

		string titleId(applauncherDeleteDialogText(it));
		char status[128];

		sce_paf_memset(status, 0, sizeof(status));

		int closed = sceAppMgrGetStatusByName(titleId.c_str(), status) != 0;
		char dir[16];

		sce_paf_strncpy(dir, savedataDir.c_str(), APPLAUNCHER_DELETE_SAVEDATA_DIR_LEN);
		dir[APPLAUNCHER_DELETE_SAVEDATA_DIR_LEN] = '\0';

		ApplauncherSaveDataInfo info;

		sce_paf_memset(&info, 0, sizeof(info));

		int ret = sceAppMgrGetSaveDataInfo(path.c_str(), dir, attr, &info);

		if (ret == APPLAUNCHER_DELETE_SAVEDATA_ERROR)
			return ret;
		if (ret == 0 && info.inUse != 0 && closed == 1)
			return APPLAUNCHER_ERROR_DELETE_RUNNING;

		ret = ((int (*)(const string *, bool *))game->func[GAME_PLUGIN_FUNC_UMASS_CHECK])(
			&titleId, &dialog->m_umassFiles);
		if (ret < 0)
			return ret;
	}

	LocalFileStat stat;

	dialog->m_installed = false;
	if (LocalFile::Getstat(path.c_str(), &stat) == 0)
		dialog->m_installed = true;

	return 0;
}

/* the check done: the confirm dialog opened, or the error's dialog (0x810D7C86) */
static void applauncherDeleteDialogOnChecked(int result, void *arg)
{
	ApplauncherDeleteDialog *dialog = (ApplauncherDeleteDialog *)arg;

	if (result == APPLAUNCHER_ERROR_CANCELED)
		return;

	if (result < 0)
		applauncherDeleteDialogShowError(dialog, &dialog->m_dialogId, result);
	else if (dialog->m_value == 0)
		applauncherDeleteDialogShowConfirm(dialog);
}

/* an application closed: AppMgr asked every half second for a minute */
static inline int applauncherDeleteDialogWaitClosed(string name)
{
	int ret;
	int count = 0;

	for (;;) {
		char status[128];

		sce_paf_memset(status, 0, sizeof(status));
		if (sceAppMgrGetStatusByName(name.c_str(), status) != 0) {
			ret = 0;
			break;
		}
		thread::Sleep(APPLAUNCHER_DELETE_WAIT_INTERVAL);
		if (++count > APPLAUNCHER_DELETE_WAIT_COUNT) {
			ret = APPLAUNCHER_ERROR_DELETE_TIMEOUT;
			break;
		}
	}
	return ret;
}

/*
 * The delete job: the application and its background application closed by topmenu, then
 * deleted; a game card's application with the card gone is an error of its own (0x810D7CB2).
 */
static int applauncherDeleteDialogDelete(ApplauncherDeleteDialogJob *job, void *arg)
{
	ApplauncherDeleteDialog *dialog = (ApplauncherDeleteDialog *)arg;
	string titleId;
	string bgTitleId;
	int subtype = SceLsdb_298134FE(dialog->m_appInfo);
	int ret;

	sceLsdbGetType(dialog->m_appInfo);
	bgTitleId = SceLsdb_457F369E(dialog->m_appInfo);

	ShellPluginInterface *topmenu =
		(ShellPluginInterface *)applauncherPluginGetInterface("topmenu_plugin", 1);

	if (topmenu == NULL) {
		ret = APPLAUNCHER_ERROR_NO_LIVEAREA;
	} else {
		titleId = sceLsdbGetName(dialog->m_appInfo);
		((void (*)(const char *))topmenu->func[TOPMENU_PLUGIN_FUNC_APP_DELETING])(
			titleId.c_str());
		ret = applauncherDeleteDialogWaitClosed(titleId.c_str());
		if (ret >= 0) {
			if (bgTitleId.length() != 0)
				ret = applauncherDeleteDialogWaitClosed(bgTitleId.c_str());
			if (ret >= 0) {
				ret = applauncherDeleteDialogDeleteApp(dialog);
				if (ret >= 0)
					return 0;
			}
		}
	}

	if (subtype == 1) {
		if (ret == APPLAUNCHER_DELETE_GC_GONE)
			return APPLAUNCHER_ERROR_DELETE_NO_CARD;

		LocalFileStat stat;

		if (LocalFile::Getstat("gro0:", &stat) == APPLAUNCHER_DELETE_GRO0_GONE)
			return APPLAUNCHER_ERROR_DELETE_NO_CARD;
	}

	return ret;
}

/* the delete job done: the waiting dialog closed, and the edit asked for, or the error's dialog
   (0x810D7FAC) */
static void applauncherDeleteDialogOnDeleted(int result, void *arg)
{
	ApplauncherDeleteDialog *dialog = (ApplauncherDeleteDialog *)arg;

	if (dialog->m_dialogId != -1) {
		SceCommonGuiDialog_BC311DC0(dialog->m_dialogId);
		dialog->m_dialogId = -1;
	}
	if (result < 0) {
		applauncherDeleteDialogShowError(dialog, &dialog->m_dialogId, result);
		return;
	}
	applauncherDeleteDialogEndEdit();
}

/* an error's dialog closed: let go of, and the edit asked for (0x810D7FEE) */
static void applauncherDeleteDialogOnClose(SceInt32 value, SceInt32 button, void *arg)
{
	ApplauncherDeleteDialog *dialog = (ApplauncherDeleteDialog *)arg;

	if (value != -1) {
		SceCommonGuiDialog_BC311DC0(value);
		dialog->m_dialogId = -1;
	}
	applauncherDeleteDialogEndEdit();
}
