/*
 * applauncher_aboutthisgame.cpp: an icon's "about this game" (0x810CAFB4-0x810CC8CE): the
 * application's directory mounted by the game plugin, its sce_sys/about/right.suprx loaded with a
 * heap of its own, and the image and text the module gives shown in a layout dialog over the home.
 * The module is unloaded once they are shown. The file's name and the names here are ours
 * (PLACEHOLDER) unless a comment says otherwise: no record of the 3.6x shell names them;
 * "AboutThisGame" (the jobs) and "SceGameRightPlugin" (the module's heap) are Sony's.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/modulemgr.h>
#include <plugin.h>
#include <std/string>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/w_busyindicator.h>
#include <graphics/surface/surface.h>
#include <timer/timer.h>
#include <job/job.h>
#include <file/localfile.h>
#include <memory/heap_allocator.h>
#include <common/shared_ptr.h>
#include <common/char_encoding.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../../../game/game_plugin/game_plugin.h"
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"
#include "applauncher_home.h"
#include "applauncher_mode.h"

using namespace paf;

/* the dialog's priority over the home */
#define APPLAUNCHER_ABOUT_PRIORITY		40

/* the dialog's contents part, and its widgets: the module's image and text, the busy
   indicator */
#define APPLAUNCHER_ABOUT_CONTENTS		4
#define APPLAUNCHER_ABOUT_IMAGE			0xDC0687AA
#define APPLAUNCHER_ABOUT_TEXT			0xD0791765
#define APPLAUNCHER_ABOUT_BUSY			0x531E99E7

/* the widgets are shown once both are laid out and the module's image and text are set */
#define APPLAUNCHER_ABOUT_READY_COUNT		2

/* the size of a mount point the game plugin gives */
#define APPLAUNCHER_ABOUT_MOUNT_POINT_SIZE	17

/* the module's heap */
#define APPLAUNCHER_ABOUT_HEAP_SIZE		0x20000

/* the dialog's page (resource IDs) */
static const CommonGuiDialogPage s_aboutPage = {
	{ 0x3A918C2C, 0x438C7285, 0x89A2810A }
};

/* PLACEHOLDER: what the module's function gives: its image (a file's bytes) and its text */
struct ApplauncherGameRightInfo {
	void *image;
	SceSize imageSize;
	const char *text;
	SceUInt32 reserved[2];
};

/* PLACEHOLDER: the module's functions, the first giving its image and text */
struct ApplauncherGameRightFuncs {
	int (*getInfo)(ApplauncherGameRightInfo *info);
};

/*
 * PLACEHOLDER: what the module is started with (a pointer to heap..., as its argument) and
 * fills: the heap and its functions the module allocates with, and its own functions. The module
 * and the heap are let go of in a job once its image and text are shown.
 */
struct ApplauncherGameRight {
	SceUID modid;
	memory::HeapAllocator *heap;
	void *(*memalign)(memory::HeapAllocator *heap, size_t alignment, size_t size);
	void (*free)(memory::HeapAllocator *heap, void *ptr);
	SceUInt32 allocPad;
	ApplauncherGameRightFuncs *funcs;
	SceUInt32 modulePad[16];
	memory::HeapAllocator *owner;
};

typedef char ApplauncherGameRightSize[sizeof(ApplauncherGameRight) == 92 ? 1 : -1];

/* PLACEHOLDER: an "AboutThisGame" job: a function run on the queue and one called when done,
   both with the argument (its code folds with ApplauncherChangeLogJob's; its vtable is its own) */
class ApplauncherAboutThisGameJob : public job::JobItem {
public:
	typedef int (*RunFunc)(ApplauncherAboutThisGameJob *job, void *arg);
	typedef void (*FinishFunc)(int result, void *arg);

	ApplauncherAboutThisGameJob(const char *name, void *arg, RunFunc run, FinishFunc finish)
		: job::JobItem(name, NULL), m_run(run), m_finish(finish), m_reserved(0), m_arg(arg)
	{
	}

	virtual ~ApplauncherAboutThisGameJob()
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

/* PLACEHOLDER: an icon's "about this game" in its dialog */
class ApplauncherAboutThisGame {
public:
	ApplauncherAboutThisGame(const common::SharedPtr<ApplauncherIcon> &icon);
	~ApplauncherAboutThisGame();

	/* the module and its heap let go of in a job, when loaded */
	void ReleaseRight();

	ApplauncherTopmenuHold *m_hold;
	ui::BusyIndicator *m_busy;
	common::SharedPtr<job::JobItem> m_job;
	common::SharedPtr<ApplauncherIcon> m_icon;
	string m_path;			/* the module's path */
	int m_dialogId;
	int m_readyCount;		/* the widgets laid out, and the module's image and text set */
	string m_mountPoint;
	ApplauncherGameRight *m_right;
};

typedef char ApplauncherAboutThisGameSize[sizeof(ApplauncherAboutThisGame) == 60 ? 1 : -1];

/* PLACEHOLDER: the "about this game" open (0x81544350) */
ApplauncherAboutThisGame *g_applauncherAboutThisGame;

static int applauncherAboutThisGameLoad(ApplauncherAboutThisGameJob *job, void *arg);
static void applauncherAboutThisGameOnLoaded(int result, void *arg);
static void applauncherAboutThisGameOnReady(int32_t type, ui::Handler *self, ui::Event *e,
					    void *arg);
static int applauncherAboutThisGameStartRight(ApplauncherAboutThisGameJob *job, void *arg);
static void applauncherAboutThisGameOnRightStarted(int result, void *arg);
static int applauncherAboutThisGameStopRight(ApplauncherAboutThisGameJob *job, void *arg);
static void *applauncherAboutThisGameMemalign(memory::HeapAllocator *heap, size_t alignment,
					      size_t size);
static void applauncherAboutThisGameFree(memory::HeapAllocator *heap, void *ptr);

/* the widget of the parent with the ID, when it is of the type */
#define APPLAUNCHER_ABOUT_FIND(parent, type, id)					\
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

/* the dialog closed: let go of, and the edit asked for (0x810CAFB4) */
static void applauncherAboutThisGameOnClose(SceInt32 value, SceInt32 button, void *arg)
{
	ApplauncherAboutThisGame *about = (ApplauncherAboutThisGame *)arg;

	if (value != -1) {
		SceCommonGuiDialog_BC311DC0(value);
		about->m_dialogId = -1;
	}
	g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
}

/* the dialog closed and the error's message dialog opened instead; without it, the edit asked
   for at once (0x810CAFE0) */
static void applauncherAboutThisGameShowError(ApplauncherAboutThisGame *about, int *dialogId,
					      int error)
{
	if (*dialogId != -1) {
		SceCommonGuiDialog_BC311DC0(*dialogId);
		about->m_dialogId = -1;
	}

	CommonGuiMessageDialogParam param;

	param.type = error;

	int ret = SceCommonGuiDialog_A6422883(g_applauncherHome->m_plugin, &param,
		new ApplauncherDialogHandler(applauncherAboutThisGameOnClose, about),
		new ApplauncherErrorOpenHandler());

	if (ret >= 0) {
		*dialogId = ret;
		return;
	}
	applauncherAboutThisGameOnClose(*dialogId, 0, about);
}

/* (0x810CB1CA) */
ApplauncherAboutThisGame::ApplauncherAboutThisGame(const common::SharedPtr<ApplauncherIcon> &icon)
	: m_hold(NULL), m_busy(NULL), m_job(), m_icon(icon), m_dialogId(-1), m_readyCount(0),
	  m_right(NULL)
{
	g_applauncherAboutThisGame = this;

	ShellPluginInterface *topmenu =
		(ShellPluginInterface *)applauncherPluginGetInterface("topmenu_plugin", 1);

	m_hold = ((ApplauncherTopmenuHold *(*)(int))topmenu->func[TOPMENU_PLUGIN_FUNC_HOLD])(0);

	CommonGuiLayoutDialogParam param;

	param.resourcePlugin = g_applauncherHome->m_plugin;
	param.priority = APPLAUNCHER_ABOUT_PRIORITY;
	param.dimmerColor[0] = 0.0f;
	param.dimmerColor[1] = 0.0f;
	param.dimmerColor[2] = 0.0f;
	param.dimmerColor[3] = 0.3f;
	param.layout = SceCommonGuiDialog_AF75CA6D;
	param.layout.page = &s_aboutPage;
	param.handler = new ApplauncherDialogHandler(applauncherAboutThisGameOnClose, this);

	int ret = SceCommonGuiDialog_CCFCD9A6(&param);

	if (ret < 0) {
		m_dialogId = -1;
	} else {
		m_dialogId = ret;

		{
			ui::Widget *contents = SceCommonGuiDialog_8947FC23(ret, APPLAUNCHER_ABOUT_CONTENTS);
			ui::Widget *image =
				contents->FindChild(IDParam(s_empty_name, APPLAUNCHER_ABOUT_IMAGE), 0);

			if (image != NULL) {
				HideWidget(image, NULL);
				image->AddEventListener(ui::Handler::CB_STATE_READY,
					new ui::EventCBListener(applauncherAboutThisGameOnReady, this));
			}

			ui::Widget *text =
				contents->FindChild(IDParam(s_empty_name, APPLAUNCHER_ABOUT_TEXT), 0);

			if (text != NULL) {
				HideWidget(text, NULL);
				text->AddEventListener(ui::Handler::CB_STATE_READY,
					new ui::EventCBListener(applauncherAboutThisGameOnReady, this));
			}

			m_busy = APPLAUNCHER_ABOUT_FIND(contents, ui::BusyIndicator, APPLAUNCHER_ABOUT_BUSY);
			sce_paf_printf_dummy();
			if (m_busy != NULL)
				m_busy->Start();
		}

		m_job = common::SharedPtr<job::JobItem>(new ApplauncherAboutThisGameJob("AboutThisGame",
			this, applauncherAboutThisGameLoad, applauncherAboutThisGameOnLoaded));
		if (m_job.get() != NULL) {
			ret = job::JobQueue::default_queue->Enqueue(m_job);
			if (ret >= 0)
				return;
		} else {
			ret = APPLAUNCHER_ERROR_NO_LIVEAREA;
		}
	}
	applauncherAboutThisGameShowError(this, &m_dialogId, ret);
}

/* (0x810CB864) */
ApplauncherAboutThisGame *applauncherAboutThisGameOpen(
	const common::SharedPtr<ApplauncherIcon> &icon)
{
	return new ApplauncherAboutThisGame(icon);
}

/* (0x810CB87A) */
void ApplauncherAboutThisGame::ReleaseRight()
{
	if (m_right != NULL) {
		common::SharedPtr<job::JobItem> job(new ApplauncherAboutThisGameJob("AboutThisGame",
			m_right, applauncherAboutThisGameStopRight, NULL));

		job::JobQueue::default_queue->Enqueue(job);
		m_right = NULL;
	}
}

/* (0x810CB97E) */
ApplauncherAboutThisGame::~ApplauncherAboutThisGame()
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

	GamePluginInterface *game =
		(GamePluginInterface *)applauncherPluginGetInterface("game_plugin", 1);

	if (game != NULL)
		((int (*)(const char *))game->func[GAME_PLUGIN_FUNC_APP_DIR_UNMOUNT])(
			m_mountPoint.c_str());

	ReleaseRight();
	g_applauncherAboutThisGame = NULL;
}

/* (0x810CBAB4) */
void applauncherAboutThisGameClose(void)
{
	if (g_applauncherAboutThisGame != NULL)
		delete g_applauncherAboutThisGame;
}

/*
 * The job: the application's information read, its directory mounted by the game plugin, and
 * the module's file opened to see that it is there (0x810CBAD0).
 */
static int applauncherAboutThisGameLoad(ApplauncherAboutThisGameJob *job, void *arg)
{
	ApplauncherAboutThisGame *about = (ApplauncherAboutThisGame *)arg;
	common::SharedPtr<File> file;
	int ret = 0;
	LsdbAppInfoPtr info(new LsdbAppInfo());
	LsdbAppInfo::iterator it = info->find(LSDB_KEY_ORG_PATH);
	AppInfoQClient *client = new AppInfoQClient;

	do {
		if (client == NULL || (client = SceLsdb_A6D8772C(client)) == NULL) {
			ret = APPLAUNCHER_ERROR_NO_MEMORY;
			break;
		}
		ret = SceLsdb_E90B81CD(client);
		if (ret < 0) {
			operator delete(SceLsdb_E2101B03(client));
			break;
		}
		ret = SceLsdb_D239031A(client, &about->m_icon->m_data->titleId, &info);
		if (ret < 0) {
			operator delete(SceLsdb_E2101B03(client));
			break;
		}
		operator delete(SceLsdb_E2101B03(client));

		it = info->find(LSDB_KEY_ORG_PATH);
		if (it == info->end()) {
			ret = APPLAUNCHER_ERROR_NO_TITLE;
			break;
		}

		{
			string path(it->second.type == LSDB_VALUE_TEXT && it->second.text != NULL ?
				    it->second.text->c_str() : "");
			GamePluginInterface *game =
				(GamePluginInterface *)applauncherPluginGetInterface("game_plugin", 1);

			if (game == NULL) {
				ret = APPLAUNCHER_ERROR_NO_LIVEAREA;
				break;
			}

			char mountPoint[APPLAUNCHER_ABOUT_MOUNT_POINT_SIZE];

			ret = ((int (*)(const char *, char *, SceSize))
			       game->func[GAME_PLUGIN_FUNC_APP_DIR_MOUNT])(path.c_str(), mountPoint,
									   APPLAUNCHER_ABOUT_MOUNT_POINT_SIZE);
			if (ret < 0)
				break;
			about->m_mountPoint = mountPoint;
			about->m_path = about->m_mountPoint + string("/sce_sys/about/right.suprx");
		}

		file = LocalFile::Open(about->m_path.c_str(), SCE_O_RDONLY, 0, &ret);
		if (ret >= 0) {
			file->Close();
			ret = 0;
		}
	} while (0);

	if (job->IsCanceled())
		return APPLAUNCHER_ERROR_CANCELED;
	return ret;
}

/*
 * The job done: the module's heap made and the module loaded and started in a job, its image and
 * text then shown; without the module, nothing; on another error, its dialog (0x810CC000).
 */
static void applauncherAboutThisGameOnLoaded(int result, void *arg)
{
	ApplauncherAboutThisGame *about = (ApplauncherAboutThisGame *)arg;

	if (result == APPLAUNCHER_ERROR_CANCELED)
		return;

	about->m_job.reset();

	if (result >= 0) {
		ApplauncherGameRight *right = new ApplauncherGameRight;
		memory::HeapAllocator *heap = new memory::HeapAllocator(NULL,
			APPLAUNCHER_ABOUT_HEAP_SIZE, "SceGameRightPlugin");
		common::SharedPtr<job::JobItem> job(new ApplauncherAboutThisGameJob("AboutThisGame",
			about, applauncherAboutThisGameStartRight,
			applauncherAboutThisGameOnRightStarted));

		if (right != NULL && heap != NULL && job.get() != NULL) {
			right->owner = heap;
			sce_paf_memset(&right->heap, 0, offsetof(ApplauncherGameRight, owner) -
							offsetof(ApplauncherGameRight, heap));
			right->heap = heap;
			right->memalign = applauncherAboutThisGameMemalign;
			right->free = applauncherAboutThisGameFree;
			about->m_right = right;
			if (job::JobQueue::default_queue->Enqueue(job) < 0) {
				delete right;
				delete heap;
				about->m_right = NULL;
			}
		} else {
			delete right;
			if (heap != NULL)
				delete heap;
		}
	} else if (result != APPLAUNCHER_ERROR_NOT_FOUND) {
		applauncherAboutThisGameShowError(about, &about->m_dialogId, result);
	}
}

/* a widget laid out (or the module's image and text set): both widgets shown, and the busy
   indicator stopped, once all are (0x810CC278) */
static void applauncherAboutThisGameOnReady(int32_t type, ui::Handler *self, ui::Event *e,
					    void *arg)
{
	ApplauncherAboutThisGame *about = (ApplauncherAboutThisGame *)arg;
	ui::Widget *contents = SceCommonGuiDialog_8947FC23(about->m_dialogId,
							   APPLAUNCHER_ABOUT_CONTENTS);

	if (contents == NULL)
		return;

	if (about->m_readyCount <= APPLAUNCHER_ABOUT_READY_COUNT) {
		about->m_readyCount++;
		return;
	}

	ui::Widget *image = contents->FindChild(IDParam(s_empty_name, APPLAUNCHER_ABOUT_IMAGE), 0);
	ui::Widget *text = contents->FindChild(IDParam(s_empty_name, APPLAUNCHER_ABOUT_TEXT), 0);

	if (image != NULL && text != NULL) {
		ShowWidget(image, new Timer(200.0f));
		ShowWidget(text, new Timer(200.0f));
		if (about->m_busy != NULL)
			about->m_busy->Stop();
	}
}

/* the module loaded and started with its heap (0x810CC4C8) */
static int applauncherAboutThisGameStartRight(ApplauncherAboutThisGameJob *job, void *arg)
{
	ApplauncherAboutThisGame *about = (ApplauncherAboutThisGame *)arg;
	void *argp = &about->m_right->heap;
	int status;
	SceUID modid = sceKernelLoadStartModule(about->m_path.c_str(), sizeof(argp), &argp, 0, NULL,
						&status);

	if (modid >= 0)
		about->m_right->modid = modid;
	return modid;
}

/* PLACEHOLDER: a UTF-8 string as UTF-16 (inline in its caller) */
static inline wstring applauncherAboutThisGameToWide(const string &src)
{
	wstring dst;

	common::Utf8ToUtf16(src, &dst, 0);
	return dst;
}

/* the module started: its image and text set in the dialog, and the module let go of; on an
   error, its dialog (0x810CC510) */
static void applauncherAboutThisGameOnRightStarted(int result, void *arg)
{
	ApplauncherAboutThisGame *about = (ApplauncherAboutThisGame *)arg;
	intrusive_ptr<graph::Surface> image;
	wstring text;
	graph::Surface::LoadOption option;
	ApplauncherGameRightInfo info;
	int ret;

	sce_paf_memset(&info, 0, sizeof(info));

	ApplauncherGameRightFuncs *funcs = about->m_right->funcs;

	do {
		if (funcs == NULL) {
			ret = APPLAUNCHER_ERROR_NO_LIVEAREA;
			break;
		}
		ret = funcs->getInfo(&info);
		if (ret < 0)
			break;

		{
			ui::Widget *contents = SceCommonGuiDialog_8947FC23(about->m_dialogId,
									   APPLAUNCHER_ABOUT_CONTENTS);

			image = graph::Surface::Load(g_applauncherHome->m_plugin->m_surface_pool, info.image,
						     info.imageSize, &option);
			ret = option.error;
			if (ret < 0)
				break;

			if (contents != NULL) {
				ui::Widget *widget = contents->FindChild(IDParam(s_empty_name,
					APPLAUNCHER_ABOUT_IMAGE), 0);

				if (widget != NULL)
					widget->SetTexture(image);
			}

			text = applauncherAboutThisGameToWide(string(info.text));

			if (contents != NULL) {
				ui::Widget *widget = contents->FindChild(IDParam(s_empty_name,
					APPLAUNCHER_ABOUT_TEXT), 0);

				if (widget != NULL)
					widget->SetString(text);
			}

			about->ReleaseRight();
			applauncherAboutThisGameOnReady(0, NULL, NULL, about);
			return;
		}
	} while (0);

	if (about->m_busy != NULL)
		about->m_busy->Stop();
	applauncherAboutThisGameShowError(about, &about->m_dialogId, ret);
}

/* the module stopped and unloaded, its heap deleted (0x810CC802) */
static int applauncherAboutThisGameStopRight(ApplauncherAboutThisGameJob *job, void *arg)
{
	ApplauncherGameRight *right = (ApplauncherGameRight *)arg;
	int status;
	int ret = sceKernelStopUnloadModule(right->modid, 0, NULL, 0, NULL, &status);

	if (right->owner != NULL)
		delete right->owner;
	delete right;

	return ret;
}

/* the module's allocation functions, on its heap (0x810CC84E, 0x810CC856) */
static void *applauncherAboutThisGameMemalign(memory::HeapAllocator *heap, size_t alignment,
					      size_t size)
{
	return heap->sce_paf_memalign(alignment, size);
}

static void applauncherAboutThisGameFree(memory::HeapAllocator *heap, void *ptr)
{
	heap->sce_paf_free(ptr);
}
