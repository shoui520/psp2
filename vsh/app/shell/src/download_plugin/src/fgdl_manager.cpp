/*
 * fgdl_manager.cpp: the foreground downloads, a title's content installed while the user waits
 * ("SceFgdlManagerJob", "SceFgdlManagerDialog", "SceFgdlMutex"), with game_plugin's dialog
 * showing why one failed, and the indicator and topmenu calls download_plugin's error dialog
 * makes. The file's name and every name in it are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <std/list>
#include <common/shared_ptr.h>
#include <common/string_util.h>
#include <common/encoded_string.h>
#include <thread/thread.h>
#include <file/localfile.h>
#include <graphics/surface/surface.h>
#include <plugin.h>
#include <framework.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <vshbridge/vshbridge.h>
#include <error/error.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <widget/w_progress_bar.h>
#include <widget/w_scene.h>

#include "../../esecron/include/esebase.hpp"
#include "../../esecron/include/esetask.hpp"
#include "../../esecron/include/eseparam.hpp"
#include "../../esecron/include/eseutil.hpp"
#include "../../esecron/include/esehttp.hpp"
#include "../../../plugins.h"
#include "../../../bgdl_api.h"
#include "../../../../game/game_plugin/game_plugin.h"
#include "../../../../game/game_plugin/game_appinfo.h"
#include "src/livespace_db/livespace_db_internal.h"
#include "src/livespace_db/lsdb_keys.h"
#include "download_plugin.h"

using namespace paf;

#define FGDL_ERROR_FAILED		0x80103A39	/* PLACEHOLDER name: what a step gives when it fails */
#define FGDL_ERROR_NO_GRW0		0x80103A42	/* PLACEHOLDER name: grw0: is not there */
#define FGDL_ERROR_80870006		0x80870006	/* PLACEHOLDER: another facility's error */
#define FGDL_ERROR_CANCELLED		0x80103A3A	/* PLACEHOLDER name: the user cancelled the install */
#define FGDL_ERROR_GRW0_GONE		0x80103A41	/* PLACEHOLDER name: the install failed, grw0: gone */
#define FGDL_ERROR_8010111C		0x8010111C	/* PLACEHOLDER: game_plugin's error */
#define FGDL_ERROR_DECLINED		0x80103A3D	/* PLACEHOLDER name: the user chose not to go on */
#define FGDL_ERROR_APP_RUNNING		0x80103A3E	/* PLACEHOLDER name: the application did not close */

/* indicator_plugin's functions download_plugin's error dialog calls (PLACEHOLDER names) */
#define FGDL_INDICATOR_FUNC_HOLD	27
#define FGDL_INDICATOR_FUNC_RELEASE	28
/* game_plugin's error dialog, opened (PLACEHOLDER name; next to GAME_PLUGIN_FUNC_DIALOG_CLOSE) */
#define FGDL_GAME_FUNC_ERROR_DIALOG	37
/* game_plugin's applications, into a vector of their names (PLACEHOLDER name) */
#define FGDL_GAME_FUNC_GET_APPS		9

/* topmenu_plugin's: the application closed for the install, and done (PLACEHOLDER names) */
#define FGDL_TOPMENU_FUNC_CLOSE_APP	76
#define FGDL_TOPMENU_FUNC_CLOSE_APP_DONE 77
/* topmenu_plugin's functions called round an install (PLACEHOLDER names) */
#define FGDL_TOPMENU_FUNC_INSTALL_BEGIN	31
#define FGDL_TOPMENU_FUNC_INSTALL_END	32

/* the indicator's text ids, minutes and hours left (PLACEHOLDER names) */
#define FGDL_STRING_MINUTES		0x95954687
#define FGDL_STRING_HOURS		0x1F2827C8

extern "C" int sceAppMgrGetMediaTypeFromDriveByPid(SceUID pid, const char *drive, int *mediaType);
extern "C" int vshKernelGetCompiledSdkVersionByPid(SceUID pid, SceUInt32 *version);
extern int sceGameUpdateRecordCacheStatus(const char *titleId, SceInt32 status);

class FgdlManager;

/* a dialog of the install (PLACEHOLDER names) */
struct FgdlDialog {
	int kind;			/* 1: the title's own content */
	void *context;			/* PLACEHOLDER: the install it belongs to */
	bool opened;			/* the dialog is open */
	int dialog;			/* game_plugin's dialog, or -1 */
	int reserved;
};

/* game_plugin's promote dialog (GAME_PLUGIN_FUNC_DIALOG_OPEN) */
typedef int (*FgdlPromoteDialogOpenFunc)(const string &title, intrusive_ptr<graph::Surface> icon,
					 const wstring &text, bool frame, const float *progress,
					 bool second);
typedef void (*FgdlDialogCallback)(int dialog, int event, void *arg);
typedef int (*FgdlErrorDialogOpenFunc)(int *dialog, FgdlDialogCallback callback, void *arg, int error,
				       intrusive_ptr<graph::Surface> icon, const wstring &name,
				       const wstring &title, const wstring &text, bool frame,
				       const intrusive_ptr<graph::Surface> *texture, int reserved,
				       const wstring &extra);

/* an install's state, which the dialogs and the manager's listener share (PLACEHOLDER) */
struct FgdlContext {
	FgdlContext(SceUInt32 id) : taskId(id), error(0), started(false), state(3),
				    mutex("SceFgdlMutex", 0), id(id)
	{
	}

	SceUInt32 taskId;
	int error;			/* the task's result, set by the listener */
	bool started;			/* the listener heard of the task */
	SceInt64 state;			/* what the listener made of the task's event */
	thread::Mutex mutex;
	SceUInt32 id;			/* PLACEHOLDER: the task again */
};

/* the install's progress dialog (PLACEHOLDER names) */
struct FgdlProgressDialog {
	FgdlContext *context;
	bool opened;			/* the dialog is open */
	int dialog;			/* the library's dialog, or -1 */
	bool cancelled;			/* the user cancelled the install */
	bool closing;
	int cancelDialog;		/* the dialog asking whether to cancel, or -1 */
};

/* the dialog that asks whether to go on (PLACEHOLDER names) */
struct FgdlConfirmDialog {
	FgdlContext *context;
	bool opened;			/* the dialog is open */
	int dialog;			/* the library's dialog, or -1 */
	int button;			/* the button it closed with; 4 to go on */
};

/* the manager: its job queue and the semaphore a dialog's close releases (PLACEHOLDER) */
class FgdlManager {
public:
	FgdlManager() : m_queue(NULL), m_dialog(NULL)
	{
		job::JobQueue::Option option;

		option.priority = 0x1000010A;
		option.stackSize = 0x3000;
		option.numThreads = 1;
		m_queue = new job::JobQueue("SceFgdlManagerJob", &option);
		m_dialog = new thread::Semaphore("SceFgdlManagerDialog", 0, 1, 0);
	}

	~FgdlManager()
	{
		if (m_queue != NULL) {
			delete m_queue;
			m_queue = NULL;
		}
		if (m_dialog != NULL) {
			delete m_dialog;
			m_dialog = NULL;
		}
	}

	int Enqueue(common::SharedPtr<job::JobItem> job)
	{
		return m_queue->Enqueue(job);
	}

	job::JobQueue *m_queue;
	thread::Semaphore *m_dialog;
};

/* a foreground install, queued on the manager's queue (PLACEHOLDER names) */
class FgdlJob : public job::JobItem {
public:
	/* a patch's install (type 1) */
	FgdlJob(SceUInt32 taskId) : job::JobItem("FgdlJob", NULL), m_taskId(taskId), m_type(1), m_result(0)
	{
	}

	/* a task's install (type 0) */
	FgdlJob(SceUInt32 taskId, const FgdlInstallArgs *args)
		: job::JobItem("FgdlJob", NULL), m_taskId(taskId), m_type(0), m_result(0), m_args(*args)
	{
	}

	/* a title's install (type 2) */
	FgdlJob(const FgdlTitleRequest *request)
		: job::JobItem("FgdlJob", NULL), m_taskId(FGDL_TASK_TITLE), m_type(2), m_result(0)
	{
		sce_paf_memset(&m_request, 0, sizeof(m_request));
		sce_paf_memset(&m_titleResult, 0, sizeof(m_titleResult));
		if (request != NULL)
			m_request = *request;
	}

	virtual ~FgdlJob()					/* (0x8112CAD6) */
	{
	}

	virtual int Run();					/* (0x8112CB12) */
	virtual void Finish(int result);			/* (0x8112CCAC) */

	SceUInt32 m_taskId;
	int m_type;			/* 0 a task, 1 a patch, 2 a title */
	int m_result;
	FgdlInstallArgs m_args;
	FgdlTitleRequest m_request;
	FgdlTitleResult m_titleResult;
	int m_reserved;		/* PLACEHOLDER: never read; Sony's job is 0x90 bytes */
};

static ui::Scene *s_page;				/* (0x81544890) the page shown while an install runs (PLACEHOLDER name) */
static FgdlManager *s_manager;				/* (0x81544894) */

/* a job's type, kept in s_jobs as it starts and ends (PLACEHOLDER names) */
struct FgdlJobEntry {
	int type;
	int reserved;
};

static paf::list<FgdlJobEntry> s_jobs;			/* (0x81563A9C) PLACEHOLDER name: never read */

void Fgdl_8112C9E0(int value, int button, SceUInt32 id, void *arg);
void Fgdl_8112C30A(SceInt32 dialog, void *arg);
void Fgdl_8112C628(SceInt32 dialog, void *arg);
void Fgdl_8112C104(int value, int button, void *arg);
void Fgdl_8112C136(int value, int button, void *arg);
void Fgdl_8112BF7C(BgdlEvent *event, void *arg);
void Fgdl_8112CA22(int value, int button, void *arg);
void Fgdl_8112C9EC(int value, int button, void *arg);
void Fgdl_8112CA6E(int dialog, int event, void *arg);
void Fgdl_8112CDF8(void *arg);
void Fgdl_8112CEE4(void *arg);

/* the progress dialog's handler: its function called with the button and the task (PLACEHOLDER) */
class DownloadTaskDialogHandler : public CommonGuiDialogHandler {
public:
	DownloadTaskDialogHandler(SceUInt32 id, void (*func)(int value, int button, SceUInt32 id, void *arg),
				  void *arg)
		: m_id(id), m_func(func), m_arg(arg)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		if (m_func != NULL)
			m_func(value, button, m_id, m_arg);
	}

	SceUInt32 m_id;
	void (*m_func)(int value, int button, SceUInt32 id, void *arg);
	void *m_arg;
};

/* the confirm dialog's handler: the dialog let go, then its function called (PLACEHOLDER) */
class DownloadConfirmDialogHandler : public CommonGuiDialogHandler {
public:
	DownloadConfirmDialogHandler(CommonGuiDialogFunction func, void *arg) : m_func(func), m_arg(arg)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		SceCommonGuiDialog_BC311DC0(value);
		if (m_func != NULL)
			m_func(value, button, m_arg);
	}

	CommonGuiDialogFunction m_func;
	void *m_arg;
};

/*
 * The error dialog's handler: its function called with the button (PLACEHOLDER names). The
 * shell's copy of operator() (0x81417038) is another file's class of the same name, which also
 * keeps the dialog, so the one kept is not this one's.
 */
class DownloadDialogHandler : public CommonGuiDialogHandler {
public:
	DownloadDialogHandler(CommonGuiDialogFunction func, void *arg) : m_func(func), m_arg(arg)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		if (m_func != NULL)
			m_func(value, button, m_arg);
	}

	CommonGuiDialogFunction m_func;
	void *m_arg;
};

/* indicator_plugin told the error dialog is up */
int Fgdl_811281F0(void)
{
	int ret;

	thread::RMutex::main_thread_mutex.Lock();

	Plugin *plugin = Plugin::Find("indicator_plugin");

	if (plugin != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)plugin->GetInterface(1);

		if (iface != NULL) {
			iface->func[FGDL_INDICATOR_FUNC_HOLD]();
			ret = 0;
		} else {
			ret = BGDL_ERROR_NO_MEMORY;
		}
	} else {
		ret = BGDL_ERROR_NO_MEMORY;
	}
	thread::RMutex::main_thread_mutex.Unlock();
	return ret;
}

/* and that it is gone */
int Fgdl_8112824E(void)
{
	int ret;

	thread::RMutex::main_thread_mutex.Lock();

	Plugin *plugin = Plugin::Find("indicator_plugin");

	if (plugin != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)plugin->GetInterface(1);

		if (iface != NULL) {
			iface->func[FGDL_INDICATOR_FUNC_RELEASE]();
			ret = 0;
		} else {
			ret = BGDL_ERROR_NO_MEMORY;
		}
	} else {
		ret = BGDL_ERROR_NO_MEMORY;
	}
	thread::RMutex::main_thread_mutex.Unlock();
	return ret;
}

/* topmenu_plugin held, and released */
int Fgdl_811282AC(void)
{
	Plugin *plugin = Plugin::Find("topmenu_plugin");

	if (plugin != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)plugin->GetInterface(1);

		if (iface != NULL)
			return ((int (*)(void))iface->func[TOPMENU_PLUGIN_FUNC_GAME_HOLD])();
	}
	return 0;
}

int Fgdl_811282CE(void)
{
	Plugin *plugin = Plugin::Find("topmenu_plugin");

	if (plugin != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)plugin->GetInterface(1);

		if (iface != NULL)
			return ((int (*)(void))iface->func[TOPMENU_PLUGIN_FUNC_GAME_RELEASE])();
	}
	return 0;
}

/* download_plugin's string, NULL without the plugin */
static inline const wchar_t *fgdlGetString(const IDParam &id)
{
	Plugin *plugin = downloadPluginGet();

	if (plugin != NULL)
		return plugin->GetString(id);
	return NULL;
}

/* the time left as text: minutes, or from 120 on hours */
wstring Fgdl_811282F2(int minutes)
{
	wstring text;
	wchar_t number[16] = L"";
	const wchar_t *format;

	if (minutes >= 120) {
		sce_paf_swprintf(number, 8, L"%d", (minutes + 59) / 60);
		format = fgdlGetString(IDParam("", FGDL_STRING_HOURS));
	} else {
		sce_paf_swprintf(number, 8, L"%d", minutes);
		format = fgdlGetString(IDParam("", FGDL_STRING_MINUTES));
	}
	if (format != NULL)
		text = shellFormatText(format, number);
	return text;
}

/* the first of the applications whose record names the title, into out */
int Fgdl_811284DA(const string *titleId, const vector<string> *apps, vector<string> *out)
{
	SceUInt32 i;

	for (i = 0; i < apps->size(); i++) {
		if ((*apps)[i].length() != 0 &&
		    gameAppRecordNamesTitle(titleId->c_str(), (*apps)[i].c_str()) == 1) {
			out->push_back(string((*apps)[i].c_str()));
			break;
		}
	}
	return 0;
}

/*
 * The running application, when its record names the title and it is one of apps, into out;
 * *outOfPeriod when its content's install period is over, *onAddcont when addcont0: is its
 */
int Fgdl_811285CC(const string *titleId, const vector<string> *apps, vector<string> *out,
		  bool *outOfPeriod, SceUInt32 *sdkVersion, bool *onAddcont)
{
	SceAppMgrAppStatus status;

	out->clear();
	if (sceAppMgrGetStatusByAppId(-3, &status) != 0)
		return 0;
	if (status.appName[0] == '\0' || sce_paf_strlen(status.appName) == 0)
		return FGDL_ERROR_FAILED;
	if (sdkVersion != NULL)
		vshKernelGetCompiledSdkVersionByPid(status.processId, sdkVersion);
	if (onAddcont != NULL) {
		char drive[16] = "addcont0:";
		int type;

		*onAddcont = sceAppMgrGetMediaTypeFromDriveByPid(status.processId, drive, &type) == 0;
	}

	SceUInt32 i;

	for (i = 0; i < apps->size(); i++) {
		if ((*apps)[i].length() != 0 &&
		    gameAppRecordNamesTitle(titleId->c_str(), (*apps)[i].c_str()) == 1 &&
		    sce_paf_strncmp((*apps)[i].c_str(), status.appName, 31) == 0) {
			out->push_back(string((*apps)[i].c_str()));
			break;
		}
	}
	if (out->size() != 0 && outOfPeriod != NULL) {
		if ((int)vshAppMgrCheckContentInstallPeriod(status.processId) < 0)
			*outOfPeriod = true;
		else
			*outOfPeriod = false;
	}
	return 0;
}

/* the error as the user is shown it, "(C1-2345-6)", with its number when code is set */
wstring Fgdl_811287F6(int error, bool code)
{
	wstring text(L"(");
	char external[16];
	common::EncodedString encoded;

	sceErrorGetExternalString(external, error);
	text += ScePafMisc_53948F03(ScePafMisc_C11CB5E4(&encoded, external, 1));
	ScePafMisc_B45A7521(&encoded);
	if (code) {
		wstring hex;
		wchar_t number[48] = L"";

		sce_paf_swprintf(number, 24, L"(E-%08x)", error);
		hex = number;
		text.append(hex);
	}
	text += L")";
	return text;
}

/* the title's icon (livespace_db), as eseutil.cpp's Bgdl_81113DE6 */
string Fgdl_8112890A(const char *titleId)
{
	string iconPath("");
	LsdbIconPtr icon(new LsdbIcon());

	if (icon.get() == NULL)
		return iconPath;

	AppInfoQClient *client = new AppInfoQClient;

	if (client == NULL || (client = SceLsdb_A6D8772C(client)) == NULL)
		return iconPath;
	if (SceLsdb_E90B81CD(client) >= 0) {
		int ret;

		{
			string id(titleId);

			ret = SceLsdb_7AF9008C(client, &id, &icon, false);
		}
		if (ret >= 0)
			iconPath = icon->iconPath;
	}
	operator delete(SceLsdb_E2101B03(client));
	return iconPath;
}

/*
 * The dialog's icon: the file icon, else fallback (*fromFallback then set), else
 * indicator_plugin's (0xBB2891AC); 0 when there is one (as esetask.cpp's bgdlLoadTaskIcon)
 */
int Fgdl_81128BB4(const string *icon, const string *fallback, intrusive_ptr<graph::Surface> *surface,
		  bool *fromFallback)
{
	int error = 0;

	*surface = NULL;
	*fromFallback = false;
	if (icon->length() != 0) {
		common::SharedPtr<File> file = LocalFile::Open(icon->c_str(), SCE_O_RDONLY, 0, &error);

		if (error == 0) {
			Plugin *plugin = Plugin::Find("download_plugin");

			if (plugin != NULL)
				*surface = graph::Surface::Load(plugin->m_surface_pool, file, NULL);
			file->Close();
		}
	}
	if (surface->get() == NULL && fallback->length() != 0) {
		common::SharedPtr<File> file = LocalFile::Open(fallback->c_str(), SCE_O_RDONLY, 0, &error);

		if (error == 0) {
			Plugin *plugin = Plugin::Find("download_plugin");

			if (plugin != NULL) {
				*surface = graph::Surface::Load(plugin->m_surface_pool, file, NULL);
				if (surface->get() != NULL)
					*fromFallback = true;
			}
			file->Close();
		}
	}
	if (surface->get() == NULL) {
		Plugin *plugin = Plugin::Find("indicator_plugin");

		if (plugin != NULL)
			*surface = plugin->GetTexture(IDParam("", 0xBB2891AC));
	}
	if (surface->get() != NULL)
		return 0;
	return -1;
}

/*
 * game_plugin's error dialog opened for the task's failure, with why it failed and the task's
 * icon; on success the manager's dialog semaphore is taken until it closes
 */
int Fgdl_81128F0C(FgdlDialog *request, int error, BgdlTaskInfo *info)
{
	int ret = FGDL_ERROR_FAILED;
	wstring title(L"");
	wstring text(L"");
	wstring name(L"");
	bool fromFallback = false;
	intrusive_ptr<graph::Surface> icon;
	string fallback("");
	intrusive_ptr<graph::Surface> texture;

	title = downloadPluginGet()->GetString(IDParam("", 0x77F396A2));
	if (error == (int)FGDL_ERROR_80870006) {
		if (request->kind == 1) {
			text = downloadPluginGet()->GetString(IDParam("", 0xB80E5DBE));

			Plugin *livearea = Plugin::Find("livearea_plugin");

			if (livearea != NULL)
				texture = livearea->GetTexture(IDParam("", 0x3CAD5A6F));
		} else {
			text = downloadPluginGet()->GetString(IDParam("", 0xBB3E6D51));
		}
	} else if (error == (int)FGDL_ERROR_NO_GRW0 || error == (int)BGDL_ERROR_GAMECARD_REMOVED) {
		if (request->kind == 1) {
			text = downloadPluginGet()->GetString(IDParam("", 0xA07D67EF));
			text += L"\n\n";
			text += downloadPluginGet()->GetString(IDParam("", 0x7F4A477F));
		} else {
			text = Fgdl_811287F6(error, false);
		}
	} else {
		text = Fgdl_811287F6(error, false);
	}
	if (info->category == 2 && info->url.length() != 0)
		fallback = Fgdl_8112890A(info->url.c_str());
	if (Fgdl_81128BB4(&info->title, &fallback, &icon, &fromFallback) == 0) {
		Plugin *game = Plugin::Find("game_plugin");

		if (game != NULL) {
			GamePluginInterface *iface = (GamePluginInterface *)game->GetInterface(1);

			if (iface != NULL) {
				common::String id(info->titleId.c_str());
				int opened;

				name = id.GetWString();
				thread::RMutex::main_thread_mutex.Lock();
				opened = ((FgdlErrorDialogOpenFunc)iface->func[FGDL_GAME_FUNC_ERROR_DIALOG])(
					&request->dialog, Fgdl_8112CA6E, request, error, icon, name, title,
					text, fromFallback, texture.get() != NULL ? &texture : NULL, 0,
					wstring(L""));
				thread::RMutex::main_thread_mutex.Unlock();
				if (opened == 0 && request->dialog >= 0) {
					request->opened = true;
					s_manager->m_dialog->Acquire();
					ret = 0;
				}
			}
		}
	}
	return ret;
}

/*
 * The common dialog library's error dialog for the install's failure: its own text for the
 * errors the manager knows, the library's for the others; on success the manager's dialog
 * semaphore is taken until it closes
 */
int Fgdl_8112957E(FgdlDialog *request, int error)
{
	{
		wstring text;
		CommonGuiMessageDialogText message;
		CommonGuiMessageDialogParam param;

		param.defaultButton = 0;
		param.cancelButton = 0;
		switch (error) {
		case (int)FGDL_ERROR_80870006:
			if (request->kind == 1) {
				text = downloadPluginGet()->GetString(IDParam("", 0x77F396A2));
				text += L"\n";
				text += downloadPluginGet()->GetString(IDParam("", 0xB80E5DBE));
			} else {
				text = downloadPluginGet()->GetString(IDParam("", 0x77F396A2));
				text += L"\n";
				text += downloadPluginGet()->GetString(IDParam("", 0xBB3E6D51));
			}
			message.format = text;
			param.title = message;
			break;
		case (int)FGDL_ERROR_NO_GRW0:
		case (int)BGDL_ERROR_GAMECARD_REMOVED:
			text = downloadPluginGet()->GetString(IDParam("", 0xA07D67EF));
			if (request->kind == 0) {
				text += L"\n\n";
				text += downloadPluginGet()->GetString(IDParam("", 0x7F4A477F));
			}
			message.format = text;
			param.title = message;
			break;
		case (int)FGDL_ERROR_8010111C:
			text = downloadPluginGet()->GetString(IDParam("", 0x77F396A2));
			text += L"\n";
			text += downloadPluginGet()->GetString(IDParam("", 0x6C570743));
			message.format = text;
			param.title = message;
			break;
		default:
			param.type = error;
			param.messageId = 0x77F396A2;
			param.defaultButton = 1;
			break;
		}
		param.priority = 34;
		param.resourcePlugin = downloadPluginGet();
		param.modal = false;
		param.useLayout = true;
		param.layout = SceCommonGuiDialog_AF75CA6D;
		param.handler = new DownloadDialogHandler(Fgdl_8112CA22, request);
		thread::RMutex::main_thread_mutex.Lock();
		request->dialog = SceCommonGuiDialog_20B141F1(&param);
		thread::RMutex::main_thread_mutex.Unlock();
	}
	if (request->dialog < 0)
		return FGDL_ERROR_FAILED;
	request->opened = true;
	s_manager->m_dialog->Acquire();
	return 0;
}

/* the library's small waiting dialog, "please wait", over the install */
int Fgdl_81129C58(FgdlDialog *request)
{
	thread::RMutex::main_thread_mutex.Lock();
	{
		CommonGuiLayoutDialogParam param;

		param.priority = 34;
		param.layout = SceCommonGuiDialog_2A68B4A4;
		param.text = downloadPluginGet()->GetString(IDParam("", 0x632B6733));
		request->dialog = SceCommonGuiDialog_CCFCD9A6(&param);
	}
	thread::RMutex::main_thread_mutex.Unlock();
	if (request->dialog < 0)
		return FGDL_ERROR_FAILED;
	request->opened = true;
	return 0;
}

/*
 * The install's progress dialog, until the task ends: its result, or 0x80103A3A when the user
 * cancelled it (the task cancelled), 0x80103A41 when it failed with grw0: gone
 */
int Fgdl_81129DF6(FgdlProgressDialog *request)
{
	SceUInt32 id = request->context->taskId;
	BgdlTaskStatus status;
	int ret;

	ret = bgdlGetTaskStatus(id, &status);
	if (ret != 0)
		return ret;
	thread::RMutex::main_thread_mutex.Lock();
	{
		CommonGuiLayoutDialogParam param;

		param.priority = 34;
		param.resourcePlugin = downloadPluginGet();
		param.text = downloadPluginGet()->GetString(IDParam("", 0xD738CDB1));
		param.layout = SceCommonGuiDialog_25BD4084;
		param.notifyClose = false;
		param.handler = new DownloadTaskDialogHandler(id, Fgdl_8112C9E0, request);
		request->dialog = SceCommonGuiDialog_CCFCD9A6(&param);
	}
	thread::RMutex::main_thread_mutex.Unlock();
	if (request->dialog < 0)
		return FGDL_ERROR_FAILED;

	ui::ProgressBar *bar = (ui::ProgressBar *)SceCommonGuiDialog_8947FC23(request->dialog, 18);

	if (bar != NULL)
		bar->SetLabelMode(ui::ProgressBar::LABEL_MODE_MANUAL);
	request->opened = true;
	request->closing = false;
	SceCommonGuiDialog_F1A40838(request->dialog, (CommonGuiDialogOpenFunction)Fgdl_8112C30A, request,
				    60.0f);
	s_manager->m_dialog->Acquire();
	ret = request->context->error;
	if (ret != 0) {
		if (Bgdl_81112284("grw0:") != true)
			return FGDL_ERROR_GRW0_GONE;
	} else if (request->cancelled == true) {
		bgdlCancelTask(id);
		return FGDL_ERROR_CANCELLED;
	} else {
		return 0;
	}
	return ret;
}

/* the title's row read from the app info database (as log_job_manager.cpp's Bgdl_81123BC0) */
__attribute__((noinline)) int Fgdl_8112A090(const char *titleId, LsdbAppInfoPtr *info)
{
	AppInfoQClient *client = new AppInfoQClient;
	int ret;

	if (client == NULL || (client = SceLsdb_A6D8772C(client)) == NULL)
		return BGDL_ERROR_NO_MEMORY;
	ret = SceLsdb_E90B81CD(client);
	if (ret >= 0) {
		{
			string id(titleId);

			ret = SceLsdb_D239031A(client, &id, info);
		}
		operator delete(SceLsdb_E2101B03(client));
	} else {
		operator delete(SceLsdb_E2101B03(client));
	}
	return ret;
}

/*
 * The dialog asking whether to go on: with running set, that the title (named) is running and
 * will be closed, else the plain question; *accepted when the user chose to go on
 */
int Fgdl_8112A15C(FgdlConfirmDialog *request, bool running, const string *titleId, bool *accepted)
{
	int ret;

	*accepted = false;

	wstring text;
	string title("");

	{
		const char *id = titleId->c_str();
		LsdbAppInfoPtr info(new LsdbAppInfo());

		if (Fgdl_8112A090(id, &info) == 0)
			title = sceLsdbGetTitle(&info);
	}
	if (running == true) {
		text = downloadPluginGet()->GetString(IDParam("", 0xDBD996D0));
		text += L"\n\n";

		common::String name(title.c_str());

		text.append(name.GetWString());
	} else {
		text = downloadPluginGet()->GetString(IDParam("", 0x7E5BFA07));
	}
	thread::RMutex::main_thread_mutex.Lock();
	{
		CommonGuiLayoutDialogParam param;

		param.priority = 34;
		param.resourcePlugin = downloadPluginGet();
		param.text = text;
		param.layout = SceCommonGuiDialog_6CD802FC;
		param.notifyClose = false;
		param.handler = new DownloadConfirmDialogHandler(Fgdl_8112C104, request);
		request->dialog = SceCommonGuiDialog_CCFCD9A6(&param);
	}
	thread::RMutex::main_thread_mutex.Unlock();
	if (request->dialog >= 0) {
		request->opened = true;
		s_manager->m_dialog->Acquire();
		ret = 0;
		*accepted = request->button == 4;
	} else {
		ret = FGDL_ERROR_FAILED;
	}
	return ret;
}

/*
 * game_plugin's promote dialog for the task, with its icon and the install's progress (a copy
 * of esetask.cpp's, the dialog's second form)
 */
int Fgdl_8112A5FE(BgdlTaskInfo *info)
{
	bool fromFallback = false;
	intrusive_ptr<graph::Surface> icon;
	int ret = -1;
	string fallback("");

	if (info->category == 2 && info->url.length() != 0)
		fallback = Fgdl_8112890A(info->url.c_str());
	if (Fgdl_81128BB4(&info->title, &fallback, &icon, &fromFallback) == 0) {
		Plugin *game = Plugin::Find("game_plugin");

		if (game != NULL) {
			GamePluginInterface *iface = (GamePluginInterface *)game->GetInterface(1);

			if (iface != NULL) {
				wstring text(downloadPluginGet()->GetString(IDParam("", 0x2A862367)));

				thread::RMutex::main_thread_mutex.Lock();
				ret = ((FgdlPromoteDialogOpenFunc)iface->func[GAME_PLUGIN_FUNC_DIALOG_OPEN])(
					info->titleId, icon, text, fromFallback, &info->progress, true);
				thread::RMutex::main_thread_mutex.Unlock();
			}
		}
	}
	return ret;
}

/* topmenu_plugin told the application closed for the install is done with */
static inline void fgdlCloseAppDone(const char *name)
{
	if (name != NULL) {
		Plugin *topmenu = Plugin::Find("topmenu_plugin");

		if (topmenu != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

			if (iface != NULL)
				((void (*)(const char *, int))iface->func[FGDL_TOPMENU_FUNC_CLOSE_APP_DONE])(name, 0);
		}
	}
}

/*
 * The task installed in the foreground, in download_plugin's locale: the title's running
 * application, when it is one of game_plugin's and its install period is over, closed first
 * (after the user agreed); the progress dialog shown while it installs, the error dialogs when
 * it fails
 */
int Fgdl_8112A7CC(SceUInt32 taskId, FgdlInstallArgs args)
{
	int ret;
	bool listening = false;
	bool accepted = false;
	string titleId;
	FgdlContext context(taskId);
	vector<string> apps;
	vector<string> running;
	vector<string> names;
	bool outOfPeriod = false;
	SceUInt32 sdkVersion = 0;
	bool onAddcont = false;
	int pluginLocale = 1;

	if (downloadPluginGet() != NULL)
		pluginLocale = downloadPluginGet()->m_locale;

	int commonLocale = Framework::Instance()->GetCommonResourceLocale();

	if (downloadPluginGet() != NULL)
		downloadPluginGet()->SetLocale((Locale)args.locale);
	Framework::Instance()->SetCommonResourceLocale((Locale)args.locale);
	do {
		ret = bgdlGetTaskTitle(taskId, &titleId);
		if (ret != 0 || titleId.length() == 0)
			break;
		apps.clear();

		{
			Plugin *game = Plugin::Find("game_plugin");

			if (game != NULL) {
				GamePluginInterface *iface = (GamePluginInterface *)game->GetInterface(1);

				ret = BGDL_ERROR_NO_MEMORY;
				if (iface != NULL)
					ret = ((int (*)(vector<string> *))iface->func[FGDL_GAME_FUNC_GET_APPS])(&apps);
			} else {
				ret = BGDL_ERROR_NO_MEMORY;
			}
		}
		if (ret != 0)
			break;
		ret = Fgdl_811284DA(&titleId, &apps, &names);
		if (ret != 0)
			break;
		if (names.size() == 0) {
			ret = BGDL_ERROR_NO_TITLE_ID;
			break;
		}
		ret = Fgdl_811285CC(&titleId, &apps, &running, &outOfPeriod, &sdkVersion, &onAddcont);
		if (ret != 0)
			break;
		if (outOfPeriod == true) {
			FgdlConfirmDialog confirm = { &context, false, -1, 5 };
			string name(running[0]);

			Fgdl_8112A15C(&confirm, true, &name, &accepted);
			if (accepted != true) {
				ret = FGDL_ERROR_DECLINED;
				break;
			}
		}
		if (bgdlManagerGet() != NULL)
			bgdlManagerGet()->AddListener(Fgdl_8112BF7C, &context);
		ret = bgdlReloadTask(taskId);
		listening = true;
		if (ret != 0)
			break;

		{
			FgdlProgressDialog progress = { &context, false, -1, false, false, -1 };

			ret = Fgdl_81129DF6(&progress);
			listening = true;
			if (ret != 0)
				break;
		}

		{
			FgdlDialog dialog = { 0, &context, false, -1, 0 };

			if (bgdlTaskIsWaiting(taskId) != true) {
				ret = FGDL_ERROR_FAILED;
			} else {
				bool closedOk = true;
				if (outOfPeriod == true) {
					Fgdl_81129C58(&dialog);

					const char *name = running[0].c_str();

					if (name != NULL) {
						Plugin *topmenu = Plugin::Find("topmenu_plugin");

						if (topmenu != NULL) {
							ShellPluginInterface *iface =
								(ShellPluginInterface *)topmenu->GetInterface(1);

							if (iface != NULL)
								((void (*)(const char *))iface->func[FGDL_TOPMENU_FUNC_CLOSE_APP])(name);
						}
					}

					int i;

					ret = FGDL_ERROR_APP_RUNNING;
					for (i = 0; i <= 30; i++) {
						SceAppMgrAppStatus status;

						sce_paf_memset(&status, 0, sizeof(status));
						if (sceAppMgrGetStatusByName(running[0].c_str(), &status) != 0) {
							ret = 0;
							break;
						}
						thread::Sleep(500);
					}
					thread::RMutex::main_thread_mutex.Lock();
					if (dialog.opened == true) {
						thread::RMutex::main_thread_mutex.Lock();
						SceCommonGuiDialog_BC311DC0(dialog.dialog);
						dialog.dialog = -1;
						dialog.opened = false;
						thread::RMutex::main_thread_mutex.Unlock();
					}
					thread::RMutex::main_thread_mutex.Unlock();
					if (ret != 0) {
						fgdlCloseAppDone(running[0].c_str());
						Fgdl_8112957E(&dialog, ret);
						closedOk = false;
					}
				}

				if (closedOk) {
					BgdlTaskInfo info;

					ret = shellBgdlGetTaskInfo(taskId, &info);
					if (ret == 0) {
						info.noOtherFlag = false;
						info.imported = outOfPeriod == true;
						info.progress = 0;
						info.path = names[0].c_str();

						int id = Fgdl_8112A5FE(&info);

						if (id >= 0) {
							ret = shellBgdlInstallTask2(&info);

							Plugin *game = Plugin::Find("game_plugin");

							if (game != NULL) {
								GamePluginInterface *iface =
									(GamePluginInterface *)game->GetInterface(1);

								if (iface != NULL) {
									thread::RMutex::main_thread_mutex.Lock();
									((void (*)(int))iface->func[GAME_PLUGIN_FUNC_DIALOG_CLOSE])(id);
									thread::RMutex::main_thread_mutex.Unlock();
								}
							}
						}
					}
					if (outOfPeriod == true)
						fgdlCloseAppDone(running[0].c_str());
					if (ret != 0) {
						if (Bgdl_81112284("grw0:") != true)
							ret = FGDL_ERROR_NO_GRW0;
						if (outOfPeriod == true && args.type == 2)
							Fgdl_81128F0C(&dialog, ret, &info);
					}
				}
			}
		}
		listening = true;
	} while (0);
	bgdlRemoveTask(taskId);
	bgdlUnmountGrw0();
	if (listening == true) {
		if (bgdlManagerGet() != NULL)
			bgdlManagerGet()->RemoveListener(Fgdl_8112BF7C, &context);
		bgdlReloadTasks();
	}
	if (downloadPluginGet() != NULL)
		downloadPluginGet()->SetLocale((Locale)pluginLocale);
	Framework::Instance()->SetCommonResourceLocale((Locale)commonLocale);
	return ret;
}

/*
 * The title's waiting tasks installed in the foreground, in the request's locale, each under
 * game_plugin's promote dialog: when the title's record is not in livespace_db, nothing
 */
int Fgdl_8112ADBE(SceUInt32 taskId, FgdlTitleRequest *request, FgdlTitleResult *result)
{
	int ret;
	FgdlContext context(taskId);

	do {
		if (request == NULL) {
			ret = BGDL_ERROR_NULL_FOLDER;
			break;
		}
		if (request->titleId[0] == '\0') {
			ret = BGDL_ERROR_NO_TITLE_ID;
			break;
		}
		string titleId(request->titleId);

		{
			LsdbAppInfoPtr info(new LsdbAppInfo());

			ret = Fgdl_8112A090(titleId.c_str(), &info);
			if (ret != 0)
				break;
		}

		vector<BgdlTaskInfo> infos;

		ret = shellBgdlGetTitleTaskInfos(titleId.c_str(), &infos, (request->flags & 1) != 0 ? 5 : 1, 0);
		if (ret != 0)
			break;
		if (infos.size() == 0) {
			ret = BGDL_ERROR_NOT_WAITING;
			break;
		}

		int locale = request->locale;
		int pluginLocale = 1;
		Plugin *plugin = downloadPluginGet();

		if (plugin != NULL)
			pluginLocale = plugin->m_locale;

		int commonLocale = Framework::Instance()->GetCommonResourceLocale();

		plugin = downloadPluginGet();
		if (plugin != NULL)
			plugin->SetLocale((Locale)locale);
		Framework::Instance()->SetCommonResourceLocale((Locale)locale);

		FgdlDialog dialog = { 2, &context, false, -1, 0 };
		SceUInt32 i;

		for (i = 0; i < infos.size(); i++) {
			infos[i].progress = 0;
			if (infos[i].category == 4)
				infos[i].imported = true;

			int id = Fgdl_8112A5FE(&infos[i]);

			if (id >= 0) {
				int error = shellBgdlInstallTask2(&infos[i]);
				Plugin *game = Plugin::Find("game_plugin");

				if (game != NULL) {
					GamePluginInterface *iface = (GamePluginInterface *)game->GetInterface(1);

					if (iface != NULL) {
						thread::RMutex::main_thread_mutex.Lock();
						((void (*)(int))iface->func[GAME_PLUGIN_FUNC_DIALOG_CLOSE])(id);
						thread::RMutex::main_thread_mutex.Unlock();
					}
				}
				if (error != 0) {
					Fgdl_81128F0C(&dialog, error, &infos[i]);
				} else if (infos[i].category == 4 && request->titleId != NULL) {
					Plugin *topmenu = Plugin::Find("topmenu_plugin");

					if (topmenu != NULL) {
						ShellPluginInterface *iface =
							(ShellPluginInterface *)topmenu->GetInterface(1);

						if (iface != NULL)
							((void (*)(const char *, int))iface->func[TOPMENU_PLUGIN_FUNC_UP_STATUS])(
								request->titleId, 3);
					}
				}
			}
		}
		if (result != NULL) {
			sce_paf_memset(result, 0, sizeof(*result));
			result->reserved = 0;
		}
		plugin = downloadPluginGet();
		if (plugin != NULL)
			plugin->SetLocale((Locale)pluginLocale);
		Framework::Instance()->SetCommonResourceLocale((Locale)commonLocale);
		ret = 0;
	} while (0);
	return ret;
}

/*
 * The common dialog library's error dialog over the progress dialog, unless the user cancelled:
 * its own text for 0x80103A3B and 0x80103A4A, the library's for the others; on success the
 * manager's dialog semaphore is taken until it closes
 */
int Fgdl_8112B108(FgdlProgressDialog *request, int error)
{
	if (request->cancelled == true)
		return 0;
	{
		CommonGuiMessageDialogParam param;

		param.defaultButton = 0;
		param.cancelButton = 0;
		switch (error) {
		case (int)0x80103A3B:
		case (int)0x80103A4A:
			param.messageId = 0xD09E6F9D;
			break;
		default:
			param.type = error;
			param.messageId = 0xE9FB03BC;
			param.defaultButton = 1;
			break;
		}
		param.priority = 34;
		param.resourcePlugin = downloadPluginGet();
		param.modal = false;
		param.useLayout = true;
		param.layout = SceCommonGuiDialog_AF75CA6D;
		param.handler = new DownloadDialogHandler(Fgdl_8112C9EC, request);
		thread::RMutex::main_thread_mutex.Lock();
		request->dialog = SceCommonGuiDialog_20B141F1(&param);
		thread::RMutex::main_thread_mutex.Unlock();
	}
	if (request->dialog < 0)
		return FGDL_ERROR_FAILED;
	request->opened = true;
	s_manager->m_dialog->Acquire();
	return 0;
}

/*
 * The progress dialog of the waiting task's install (as Fgdl_81129DF6, its own interval
 * function): the task's result once it closes, 0x80103A3A when the user cancelled
 */
int Fgdl_8112B2F4(FgdlProgressDialog *request)
{
	SceUInt32 id = request->context->taskId;
	BgdlTaskStatus status;
	int ret;

	ret = bgdlGetTaskStatus(id, &status);
	if (ret != 0)
		return ret;
	thread::RMutex::main_thread_mutex.Lock();
	{
		CommonGuiLayoutDialogParam param;

		param.priority = 34;
		param.resourcePlugin = downloadPluginGet();
		param.text = downloadPluginGet()->GetString(IDParam("", 0xD738CDB1));
		param.layout = SceCommonGuiDialog_25BD4084;
		param.notifyClose = false;
		param.handler = new DownloadTaskDialogHandler(id, Fgdl_8112C9E0, request);
		request->dialog = SceCommonGuiDialog_CCFCD9A6(&param);
	}
	thread::RMutex::main_thread_mutex.Unlock();
	if (request->dialog < 0)
		return FGDL_ERROR_FAILED;

	ui::ProgressBar *bar = (ui::ProgressBar *)SceCommonGuiDialog_8947FC23(request->dialog, 18);

	if (bar != NULL)
		bar->SetLabelMode(ui::ProgressBar::LABEL_MODE_MANUAL);
	request->opened = true;
	SceCommonGuiDialog_F1A40838(request->dialog, (CommonGuiDialogOpenFunction)Fgdl_8112C628, request,
				    60.0f);
	s_manager->m_dialog->Acquire();
	ret = request->context->error;
	if (ret != 0) {
		if (Bgdl_81112284("grw0:") != true)
			return FGDL_ERROR_GRW0_GONE;
	} else if (request->cancelled == true) {
		bgdlCancelTask(id);
		return FGDL_ERROR_CANCELLED;
	} else {
		return 0;
	}
	return ret;
}

/*
 * The waiting patch installed in the foreground: the title, when it is the running
 * application, closed first (after the user agreed); the progress dialog shown while it
 * installs, the error dialogs when it fails
 */
int Fgdl_8112B58A(SceUInt32 taskId)
{
	int ret;
	bool listening = false;
	string titleId;
	FgdlContext context(taskId);
	bool accepted = false;

	do {
		ret = bgdlGetTaskTitle(taskId, &titleId);
		if (ret != 0)
			break;

		bool running = false;
		{
			SceAppMgrAppStatus status;

			if (sceAppMgrGetStatusByAppId(-3, &status) == 0 && status.appName[0] != '\0' &&
			    sce_paf_strlen(status.appName) != 0 &&
			    sce_paf_strncmp(titleId.c_str(), status.appName, 31) == 0) {
				FgdlConfirmDialog confirm = { &context, false, -1, 5 };

				running = true;

				string name(titleId);

				Fgdl_8112A15C(&confirm, false, &name, &accepted);
				if (accepted != true) {
					ret = FGDL_ERROR_DECLINED;
					break;
				}
			}
		}
		listening = true;
		if (bgdlManagerGet() != NULL)
			bgdlManagerGet()->AddListener(Fgdl_8112BF7C, &context);
		ret = bgdlReloadTask(taskId);
		if (ret != 0)
			break;

		{
			FgdlProgressDialog progress = { &context, false, -1, false, false, -1 };

			ret = Fgdl_8112B2F4(&progress);
			if (ret != 0) {
				Fgdl_8112B108(&progress, ret);
				break;
			}
		}
		if (bgdlTaskIsWaiting(taskId) != true) {
			ret = FGDL_ERROR_FAILED;
			break;
		}

		{
			FgdlDialog dialog = { 1, &context, false, -1, 0 };
			const char *name = titleId.c_str();

			if (name != NULL) {
				Plugin *topmenu = Plugin::Find("topmenu_plugin");

				if (topmenu != NULL) {
					ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

					if (iface != NULL)
						((void (*)(const char *))iface->func[FGDL_TOPMENU_FUNC_CLOSE_APP])(name);
				}
			}
			if (running == true) {
				Fgdl_81129C58(&dialog);

				int i;

				ret = FGDL_ERROR_APP_RUNNING;
				for (i = 0; i <= 30; i++) {
					SceAppMgrAppStatus status;

					sce_paf_memset(&status, 0, sizeof(status));
					if (sceAppMgrGetStatusByName(titleId.c_str(), &status) != 0) {
						ret = 0;
						break;
					}
					thread::Sleep(500);
				}
				thread::RMutex::main_thread_mutex.Lock();
				if (dialog.opened == true) {
					thread::RMutex::main_thread_mutex.Lock();
					SceCommonGuiDialog_BC311DC0(dialog.dialog);
					dialog.opened = false;
					dialog.dialog = -1;
					thread::RMutex::main_thread_mutex.Unlock();
				}
				thread::RMutex::main_thread_mutex.Unlock();
				if (ret != 0) {
					fgdlCloseAppDone(titleId.c_str());
					Fgdl_8112957E(&dialog, ret);
					break;
				}
			}

			BgdlTaskInfo info;

			ret = shellBgdlGetTaskInfo(taskId, &info);
			if (ret == 0) {
				info.noOtherFlag = false;
				info.imported = true;
				info.progress = 0;
				info.path = titleId.c_str();

				int id = Fgdl_8112A5FE(&info);

				if (id >= 0) {
					ret = shellBgdlInstallTask2(&info);

					Plugin *game = Plugin::Find("game_plugin");

					if (game != NULL) {
						GamePluginInterface *iface = (GamePluginInterface *)game->GetInterface(1);

						if (iface != NULL) {
							thread::RMutex::main_thread_mutex.Lock();
							((void (*)(int))iface->func[GAME_PLUGIN_FUNC_DIALOG_CLOSE])(id);
							thread::RMutex::main_thread_mutex.Unlock();
						}
					}
				}
			}
			if (ret != 0) {
				fgdlCloseAppDone(titleId.c_str());
			} else if (titleId.c_str() != NULL) {
				sceGameUpdateRecordCacheStatus(titleId.c_str(), 1);

				const char *done = titleId.c_str();

				if (done != NULL) {
					Plugin *topmenu = Plugin::Find("topmenu_plugin");

					if (topmenu != NULL) {
						ShellPluginInterface *iface =
							(ShellPluginInterface *)topmenu->GetInterface(1);

						if (iface != NULL)
							((void (*)(const char *, int))iface->func[FGDL_TOPMENU_FUNC_CLOSE_APP_DONE])(
								done, 1);
					}
				}
			}
			if (ret != 0)
				Fgdl_81128F0C(&dialog, ret, &info);
		}
	} while (0);
	bgdlRemoveTask(taskId);
	bgdlUnmountGrw0();
	if (listening == true) {
		if (bgdlManagerGet() != NULL)
			bgdlManagerGet()->RemoveListener(Fgdl_8112BF7C, &context);
	}
	return ret;
}

/* the dispatcher told a foreground install of the task is queued */
static inline void fgdlDispatcherQueued(SceUInt32 taskId)
{
	BgdlDispatcherState state;

	sce_paf_memset(&state, 0, sizeof(state));
	DownloadService::Bgdl_8113A434()->Bgdl_8113A440(&state, true);
	state.state = 1;
	state.taskId = taskId;
	DownloadService::Bgdl_8113A434()->Bgdl_8113A440(&state, false);
	DownloadService::Bgdl_8113A434()->Bgdl_8113A47C(&state, false);
}

/* the waiting patch's install queued (the interface's Bgdl_811239DC) */
int Fgdl_8112B9A2(void *reserved, const SceUInt32 *taskId)
{
	if (taskId != NULL) {
		fgdlDispatcherQueued(*taskId);

		common::SharedPtr<job::JobItem> job(new FgdlJob(*taskId));

		s_manager->Enqueue(job);
	}
	return 1;
}

/* the task's install queued, with the caller's arguments (the interface's Bgdl_811239D4) */
int Fgdl_8112BB36(const FgdlCaller *caller, int reserved, SceUInt32 taskId)
{
	FgdlInstallArgs args;

	if (caller != NULL) {
		sce_paf_memset(&args, 0, sizeof(args));
		args = caller->args;
	}
	fgdlDispatcherQueued(taskId);

	common::SharedPtr<job::JobItem> job(new FgdlJob(taskId, &args));

	s_manager->Enqueue(job);
	return 1;
}

/* the title's install queued (the interface's Bgdl_811239E4) */
int Fgdl_8112BCEC(const FgdlTitleRequest *request)
{
	fgdlDispatcherQueued(FGDL_TASK_TITLE);

	common::SharedPtr<job::JobItem> job(new FgdlJob(request));

	s_manager->Enqueue(job);
	return 1;
}

/* the listener heard of the task, and it is done with a state the install goes on from */
bool Fgdl_8112BEC4(FgdlContext *context)
{
	bool started;

	context->mutex.Lock();
	started = context->started;
	context->mutex.Unlock();
	if (started != true)
		return false;

	SceInt64 state;

	context->mutex.Lock();
	state = context->state;
	context->mutex.Unlock();
	switch (state) {
	case 1:
	case 3:
	case 4:
		return true;
	case 0:
	case 2:
	case 80:
	case 81:
	default:
		return false;
	}
}

/*
 * The manager's listener: the install's task's events (3 and 4) made into its state, kept in
 * the context and given to the dispatcher with the task's result
 */
void Fgdl_8112BF7C(BgdlEvent *event, void *arg)
{
	FgdlContext *context = (FgdlContext *)arg;

	if (context == NULL || context->taskId != event->id)
		return;

	bool started;

	context->mutex.Lock();
	started = context->started;
	context->mutex.Unlock();
	if (started == false) {
		context->mutex.Lock();
		context->started = true;
		context->mutex.Unlock();
	}
	if (event->type < 3 || event->type > 4)
		return;

	SceInt64 state = 3;

	switch (event->param.runStatus) {
	case 0:
		if (event->param.ready) {
			state = event->param.key113 ? 82 : 80;
		} else if (event->param.key113) {
			state = 7;
		} else {
			context->error = event->result;
			if (context->error != 0) {
				if (context->error == (int)BGDL_ERROR_INTERRUPTED ||
				    context->error == (int)0x80103A1C)
					state = 2;
				else
					state = event->param.state == 3 ? 0 : 2;
			} else {
				context->error = FGDL_ERROR_FAILED;
				state = 2;
			}
		}
		break;
	case 1:
		switch (event->param.state) {
		case 0:
			break;
		case 1:
		case 2:
		case 3:
			state = 6;
			break;
		case 4:
			break;
		}
		break;
	case 2:
		switch (event->param.state) {
		case 0:
		case 1:
			break;
		case 2:
			state = 4;
			break;
		case 3:
			state = 1;
			break;
		case 4:
			break;
		}
		break;
	}
	context->mutex.Lock();
	context->state = state;

	int error = context->error;
	BgdlDispatcherState dispatcher;

	DownloadService::Bgdl_8113A434()->Bgdl_8113A47C(&dispatcher, false);
	dispatcher.state = 2;
	dispatcher.value = (int)state;
	dispatcher.error = error;
	DownloadService::Bgdl_8113A434()->Bgdl_8113A440(&dispatcher, false);
	context->mutex.Unlock();
}

/* the confirm dialog's function: the button kept, the dialog closed and the manager's
   semaphore given back */
void Fgdl_8112C104(int value, int button, void *arg)
{
	FgdlConfirmDialog *request = (FgdlConfirmDialog *)arg;

	if (request == NULL)
		return;
	request->button = button;
	SceCommonGuiDialog_BC311DC0(value == -1 ? request->dialog : value);
	request->dialog = -1;
	request->opened = false;
	s_manager->m_dialog->Release();
}

/* the cancel dialog's function: on the button to cancel (4), the progress dialog closed */
void Fgdl_8112C136(int value, int button, void *arg)
{
	FgdlProgressDialog *request = (FgdlProgressDialog *)arg;

	if (request == NULL)
		return;
	request->cancelDialog = -1;
	if (button == 4 && request->cancelled == false) {
		request->cancelled = true;
		SceCommonGuiDialog_BC311DC0(request->dialog);
		request->dialog = -1;
		request->opened = false;
		s_manager->m_dialog->Release();
	}
}

/* the dialog asking whether to cancel the install, over the progress dialog */
__attribute__((noinline)) void Fgdl_8112C170(FgdlProgressDialog *request)
{
	CommonGuiLayoutDialogParam param;

	param.priority = 34;
	param.resourcePlugin = downloadPluginGet();
	param.text = downloadPluginGet()->GetString(IDParam("", 0x2F994F56));
	param.layout = SceCommonGuiDialog_6CD802FC;
	param.notifyClose = false;
	param.handler = new DownloadConfirmDialogHandler(Fgdl_8112C136, request);
	request->cancelDialog = SceCommonGuiDialog_CCFCD9A6(&param);
}

/*
 * The progress dialog's interval function: closed (with the cancel dialog) when the user
 * cancelled or the task is done with; while it downloads, its bar, the time left and the
 * sizes done and in all
 */
void Fgdl_8112C30A(SceInt32 dialog, void *arg)
{
	FgdlProgressDialog *request = (FgdlProgressDialog *)arg;
	FgdlContext *context = request->context;
	bool started;

	context->mutex.Lock();
	started = context->started;
	context->mutex.Unlock();
	if (started == false)
		return;

	bool close = false;

	if (request->cancelled == true)
		close = true;
	if (!Fgdl_8112BEC4(request->context))
		close = true;

	BgdlTaskStatus status;

	bgdlGetTaskStatus(request->context->taskId, &status);
	if (close == true) {
		if (request->cancelDialog != -1) {
			SceCommonGuiDialog_BC311DC0(request->cancelDialog);
			request->cancelDialog = -1;
		}
		if (request->cancelled == false) {
			SceCommonGuiDialog_BC311DC0(request->dialog);
			request->opened = false;
			request->dialog = -1;
			s_manager->m_dialog->Release();
		}
		return;
	}

	SceInt64 state;

	context = request->context;
	context->mutex.Lock();
	state = context->state;
	context->mutex.Unlock();
	if (state != 4 || status.total == 0)
		return;

	ui::ProgressBar *bar = (ui::ProgressBar *)SceCommonGuiDialog_8947FC23(dialog, 18);

	if (bar != NULL)
		bar->SetValueAsync((int32_t)(status.current * 100 / status.total), true);

	ui::Widget *label = SceCommonGuiDialog_8947FC23(dialog, 19);

	if (label != NULL)
		label->SetString(Fgdl_811282F2(status.remaining));
	if (bar == NULL)
		return;

	ui::Widget *sizes = bar->FindChild(IDParam("", 0xEAFA0A2F), 0);

	if (sizes != NULL) {
		wstring current(L"");
		wstring total(L"");
		wchar_t text[30];

		shellSizeText(&current, status.current);
		shellSizeText(&total, status.total);
		sce_paf_swprintf(text, sizeof(text), L"%s / %s", current.c_str(), total.c_str());
		sizes->SetString(wstring(text));
		if (!sizes->m_transition_complete) {
			sizes->m_transition = 0xff;
			sizes->m_transition_complete = true;
			sizes->DoTransition_core(0.0f, true);
		}
		ScePafWidget_FF5BC957(sizes, NULL, 2, 0.0f, NULL);
	}
}

/*
 * The patch install's progress dialog's interval function: like Fgdl_8112C30A, but the
 * sizes are those of the files once a second file is downloaded, and the bar is filled and
 * the dialog marked closing once the task has all of them
 */
void Fgdl_8112C628(SceInt32 dialog, void *arg)
{
	FgdlProgressDialog *request = (FgdlProgressDialog *)arg;
	FgdlContext *context = request->context;
	bool started;

	context->mutex.Lock();
	started = context->started;
	context->mutex.Unlock();
	if (started == false)
		return;

	bool close = false;

	if (request->cancelled == true)
		close = true;
	if (!Fgdl_8112BEC4(request->context))
		close = true;

	BgdlTaskStatus status;

	bgdlGetTaskStatus(request->context->taskId, &status);
	if (close == true) {
		if (request->cancelDialog != -1) {
			SceCommonGuiDialog_BC311DC0(request->cancelDialog);
			request->cancelDialog = -1;
		}
		if (request->cancelled == false) {
			SceCommonGuiDialog_BC311DC0(request->dialog);
			request->opened = false;
			request->dialog = -1;
			s_manager->m_dialog->Release();
		}
		return;
	}

	SceInt64 state;

	context = request->context;
	context->mutex.Lock();
	state = context->state;
	context->mutex.Unlock();
	if (state != 4 || request->closing == true)
		return;

	SceUInt64 total = status.total;
	SceUInt64 current = status.current;
	int remaining = status.remaining;

	if (status.fileNumber >= 2) {
		total = status.filesTotal;
		current = status.filesCurrent;
		remaining = status.filesRemaining;
	}
	if (total == 0)
		return;

	ui::ProgressBar *bar;
	ui::Widget *label;

	if (total <= current) {
		request->closing = true;
		bar = (ui::ProgressBar *)SceCommonGuiDialog_8947FC23(dialog, 18);
		if (bar != NULL)
			bar->SetValueAsync(100, true);
		label = SceCommonGuiDialog_8947FC23(dialog, 19);
		if (label != NULL)
			label->SetString(Fgdl_811282F2(0));
		return;
	}

	bar = (ui::ProgressBar *)SceCommonGuiDialog_8947FC23(dialog, 18);
	if (bar != NULL)
		bar->SetValueAsync((int32_t)(current * 100 / total), true);
	label = SceCommonGuiDialog_8947FC23(dialog, 19);
	if (label != NULL)
		label->SetString(Fgdl_811282F2(remaining));
	if (bar == NULL)
		return;

	ui::Widget *sizes = bar->FindChild(IDParam("", 0xEAFA0A2F), 0);

	if (sizes != NULL) {
		wstring currentText(L"");
		wstring totalText(L"");
		wchar_t text[30];

		shellSizeText(&currentText, current);
		shellSizeText(&totalText, total);
		sce_paf_swprintf(text, sizeof(text), L"%s / %s", currentText.c_str(), totalText.c_str());
		sizes->SetString(wstring(text));
		if (!sizes->m_transition_complete) {
			sizes->m_transition = 0xff;
			sizes->m_transition_complete = true;
			sizes->DoTransition_core(0.0f, true);
		}
		ScePafWidget_FF5BC957(sizes, NULL, 2, 0.0f, NULL);
	}
}

/* the progress dialog's button: asks whether to cancel the install */
void Fgdl_8112C9E0(int value, int button, SceUInt32 id, void *arg)
{
	if (arg != NULL)
		Fgdl_8112C170((FgdlProgressDialog *)arg);
}

/* the patch install's message dialog closed */
void Fgdl_8112C9EC(int value, int button, void *arg)
{
	FgdlProgressDialog *request = (FgdlProgressDialog *)arg;

	if (request == NULL || request->opened != true)
		return;
	SceCommonGuiDialog_BC311DC0(value == -1 ? request->dialog : value);
	request->dialog = -1;
	request->opened = false;
	s_manager->m_dialog->Release();
}

/* the install's message dialog closed */
void Fgdl_8112CA22(int value, int button, void *arg)
{
	FgdlDialog *request = (FgdlDialog *)arg;

	if (request == NULL)
		return;
	thread::RMutex::main_thread_mutex.Lock();
	if (request->opened == true) {
		SceCommonGuiDialog_BC311DC0(value == -1 ? request->dialog : value);
		request->dialog = -1;
		request->opened = false;
		s_manager->m_dialog->Release();
	}
	thread::RMutex::main_thread_mutex.Unlock();
}

/* game_plugin's error dialog closed */
void Fgdl_8112CA6E(int dialog, int event, void *arg)
{
	FgdlDialog *request = (FgdlDialog *)arg;

	if (event == 0 || request == NULL)
		return;
	thread::RMutex::main_thread_mutex.Lock();
	if (request->opened == true) {
		Plugin *game = Plugin::Find("game_plugin");

		if (game != NULL) {
			GamePluginInterface *iface = (GamePluginInterface *)game->GetInterface(1);

			if (iface != NULL)
				((void (*)(int))iface->func[GAME_PLUGIN_FUNC_DIALOG_CLOSE])(request->dialog);
		}
		request->dialog = -1;
		request->opened = false;
		s_manager->m_dialog->Release();
	}
	thread::RMutex::main_thread_mutex.Unlock();
}

/* the install run: topmenu_plugin's page shown and the indicator held while it runs */
int FgdlJob::Run()
{
	int ret = 0;
	FgdlJobEntry entry = { m_type, 0 };

	s_jobs.push_back(entry);
	thread::CallQueue::main_thread_callqueue.Enqueue(NULL, Fgdl_8112CDF8, NULL);
	s_manager->m_dialog->Acquire();
	thread::RMutex::main_thread_mutex.Lock();
	{
		Plugin *indicator = Plugin::Find("indicator_plugin");

		if (indicator != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)indicator->GetInterface(1);

			if (iface != NULL)
				iface->func[FGDL_INDICATOR_FUNC_HOLD]();
		}
	}
	thread::RMutex::main_thread_mutex.Unlock();
	switch (m_type) {
	case 0:
		ret = Fgdl_8112A7CC(m_taskId, m_args);
		break;
	case 1:
		ret = Fgdl_8112B58A(m_taskId);
		break;
	case 2:
		ret = Fgdl_8112ADBE(m_taskId, &m_request, &m_titleResult);
		break;
	}
	m_result = ret;
	thread::RMutex::main_thread_mutex.Lock();
	{
		Plugin *indicator = Plugin::Find("indicator_plugin");

		if (indicator != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)indicator->GetInterface(1);

			if (iface != NULL)
				iface->func[FGDL_INDICATOR_FUNC_RELEASE]();
		}
	}
	thread::RMutex::main_thread_mutex.Unlock();

	FgdlJobEntry done = { m_type, 0 };

	s_jobs.push_back(done);
	thread::CallQueue::main_thread_callqueue.Enqueue(NULL, Fgdl_8112CEE4, NULL);
	s_manager->m_dialog->Acquire();
	return 0;
}

/* the dispatcher told the install's result */
void FgdlJob::Finish(int result)
{
	if (m_type == 2)
		DownloadService::Bgdl_8113A434()->Bgdl_8113A4B8(&m_titleResult.reserved);

	int error = m_result;
	BgdlDispatcherState state;

	DownloadService::Bgdl_8113A434()->Bgdl_8113A47C(&state, false);
	state.error = error;
	state.state = 3;
	DownloadService::Bgdl_8113A434()->Bgdl_8113A440(&state, true);
	sce_paf_memset(&state, 0, sizeof(state));
	DownloadService::Bgdl_8113A434()->Bgdl_8113A440(&state, false);
}

/* the manager made (download_plugin's start) */
void Fgdl_8112CD1A(void)
{
	s_manager = new FgdlManager;
}

/* and destroyed */
void Fgdl_8112CDB2(void)
{
	if (s_manager != NULL)
		delete s_manager;
	s_manager = NULL;
}

/* on the main thread as an install starts: topmenu_plugin told, download_plugin's page opened */
void Fgdl_8112CDF8(void *arg)
{
	Plugin *topmenu = Plugin::Find("topmenu_plugin");

	if (topmenu != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

		if (iface != NULL)
			iface->func[FGDL_TOPMENU_FUNC_INSTALL_BEGIN]();
	}

	Plugin *plugin = downloadPluginGet();
	Plugin::PageOpenParam param;

	if (plugin != NULL) {
		ui::Scene *page = plugin->PageOpen(IDParam("", 0x5FCD170E), param);

		if (page != NULL) {
			s_page = page;
			ScePafWidget_42C5A4D4(page, 0, 0);
		}
	}
	s_manager->m_dialog->Release();
}

/* and as it ends: topmenu_plugin told, the page closed */
void Fgdl_8112CEE4(void *arg)
{
	Plugin *topmenu = Plugin::Find("topmenu_plugin");

	if (topmenu != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

		if (iface != NULL)
			iface->func[FGDL_TOPMENU_FUNC_INSTALL_END]();
	}
	if (s_page != NULL) {
		ScePafWidget_42C5A4D4(s_page, 2, 0);
		s_page = NULL;
	}

	Plugin *plugin = downloadPluginGet();

	if (plugin != NULL)
		plugin->PageCloseAll();
	s_manager->m_dialog->Release();
}
