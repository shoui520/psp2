/*
 * SceShell's game plugin: the game card promoter. It installs ("promotes") the contents of an
 * inserted game card when the home screen is up, on its own job queue, shows the progress in
 * dialogs, and keeps the system awake meanwhile. The file's and the class's names are ours,
 * after Sony's names for its job queue and mutex ("SceGameGcPromoteJobQueue",
 * "SceGameGcPromote").
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/processmgr.h>
#include <appmgr/appmgr.h>
#include <std/string.h>
#include <std/stdio.h>
#include <std/string>
#include <std/vector>
#include <std/map>
#include <std/memory>
#include <thread/thread.h>
#include <job/job.h>
#include <timer/timer.h>
#include <graphics/surface/surface.h>
#include <widget/core/widget.h>
#include <widget/w_plane.h>
#include <widget/w_text.h>
#include <widget/w_progress_bar.h>
#include <widget/w_dialog.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <common/char_encoding.h>
#include <kernel/iofilemgr.h>
#include <file/localfile.h>
#include <dir/dir.h>

#include "../../shell/shell.h"
#include "../../shell/plugins.h"
#include "../../shell/sysupchk.h"
#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

/* the plugin's error for a job canceled before it ran (value only) */
#define GAME_ERROR_CANCELED		-2146430707	/* 0x8010110D */

/* the home screen's title ID */
#define GAME_HOME_TITLE_ID		"NPXS19999"

/* the dialogs' parts by ID (names ours from their use) and by index in the layout */
#define GAME_GC_PROMOTE_PART_ICON	0x2A35FC7A	/* the title's icon */
#define GAME_GC_PROMOTE_PART_TITLE	0x34B3D856	/* the title's name */
#define GAME_GC_PROMOTE_PART_PARTS3	0x51DF3C38
#define GAME_GC_PROMOTE_PART_TEXT	0xD0791765	/* the message */
#define GAME_GC_PROMOTE_PART_BUTTON1	0x5049F5E4
#define GAME_GC_PROMOTE_PART_BUTTON2	0xBD559A97
#define GAME_GC_PROMOTE_PART_PARTS27	0xD713BB35
#define GAME_GC_PROMOTE_PART_PARTS28	0x2EF9EC65
#define GAME_GC_PROMOTE_PART_PARTS18	0x8EED7923	/* the progress bar */
#define GAME_GC_PROMOTE_PART_PARTS19	0xEAFA0A2F

enum {
	GAME_GC_PROMOTE_INDEX_TEXT	= 1,
	GAME_GC_PROMOTE_INDEX_BUTTON1	= 12,
	GAME_GC_PROMOTE_INDEX_BUTTON2	= 13,
	GAME_GC_PROMOTE_INDEX_PROGRESS	= 18,
	GAME_GC_PROMOTE_INDEX_PROGRESS2	= 19,
	GAME_GC_PROMOTE_INDEX_ICON	= 24,
	GAME_GC_PROMOTE_INDEX_TITLE	= 25,
	GAME_GC_PROMOTE_INDEX_PARTS26	= 26,
	GAME_GC_PROMOTE_INDEX_PARTS27	= 27,
	GAME_GC_PROMOTE_INDEX_PARTS28	= 28,
};

/* the content list's error for a list with nothing to promote (value only) */
#define GAME_ERROR_NO_PROMOTE		-2146420224	/* 0x80104000 */

/* the entry type of a title the promote stops the promoter at (value only) */
#define GAME_GC_PROMOTE_TYPE_STOP	16

/* the texts and textures the promoter takes from the plugins (names ours) */
#define GAME_GC_PROMOTE_MSG_PROMOTING	0x2A85A167	/* applauncher's text over a promote */
#define GAME_GC_PROMOTE_MSG_PROMOTING2	0x632B2D33	/* the same, without a progress bar */
#define GAME_GC_PROMOTE_MSG_INSTALLING	0x0D4CB4BA	/* applauncher's text over an install */
#define GAME_GC_PROMOTE_MSG_ERROR	0xC51A4FA4	/* applauncher's text of a failed promote */
#define GAME_GC_PROMOTE_MSG_ICON_LIMIT	0xB7076A87	/* applauncher's text of the icon limit */
#define GAME_GC_PROMOTE_MSG_CONFLICT	0xC3A8DBA7	/* applauncher's title of a conflict */
#define GAME_GC_PROMOTE_MSG_NOTICE	0x77F31AA2	/* applauncher's text over an error notice */

/* the most icons the home screen holds */
#define GAME_ICONS_MAX			500
#define GAME_GC_PROMOTE_TEX_NO_ICON	0xBB28F6AC	/* indicator's icon of a title without one */

/*
 * A title of the card to promote, as the shell's content list gives it, 92 bytes (names ours,
 * from the list's filler, 0x8110E99A, and the promoter's use).
 */
struct GameGcPromoteEntry {
	SceInt32 id;				/* the title's entry in the content list */
	string titleId;
	string titleId2;			/* the title ID again */
	string iconPath;			/* its icon0, the list's parameter 106 */
	string optionText;			/* the list's parameter 230, when its 239 is set */
	string title;				/* its name, the list's parameter 105 */
	SceInt32 location;			/* 0..3, from the title's bits 23..25 */
	SceInt32 type;				/* 8 or 16, from its content kind */
	bool reserved1;
	bool reserved2;
	SceInt32 reserved3;
	void (*progressFunc)(void *arg, float progress);	/* told the promote's progress */
	void *progressArg;
	bool reserved4;
};

/*
 * The shell's list of the card's titles to promote, and a promote of one, stopped when cancel
 * says so (0x810FCB60, 0x810FCB68). PLACEHOLDER names: defined in another file of the shell.
 */
int gameGcGetPromoteEntries(vector<GameGcPromoteEntry> *entries);
int gameGcPromoteEntry(GameGcPromoteEntry *entry, bool (*cancel)(void *arg), void *arg);

/* the installer's error that keeps the title it stopped at (value only) */
#define GAME_ERROR_INSTALL_TITLE	-2146430664	/* 0x80101138 */

/* a card title's state before it is promoted (value only), and how many titles a card lists */
#define GAME_GC_TITLE_NOT_PROMOTED	-2146430710	/* 0x8010110A */
#define GAME_GC_TITLES_MAX		1

/* the promoter's error for a card it knows no title of (value only) */
#define GAME_ERROR_NO_GC_TITLE		-2146430651	/* 0x80101145 */

/* the plugin's error for a plugin that did not come up in time (value only) */
#define GAME_ERROR_TIMEOUT		-2146430708	/* 0x8010110C */

/* the promoter's error for a card with nothing to install or promote (value only) */
#define GAME_ERROR_GC_NO_APP		-2146430691	/* 0x8010111D */

/* the icon pages the promoter counts (value only) */
#define GAME_GC_PROMOTE_ICON_PAGE	-100000000

/* an error code as the system shows it, "C1-2345-6" (SceDriverUser's) */
extern "C" int sceErrorGetExternalString(char *buf, SceUInt32 code);

/* the plugin's error for a home screen with too many icons; applauncher's icons are tidied
   after it (value only) */
#define GAME_ERROR_TOO_MANY_ICONS	-2146430701	/* 0x80101113 */

/* the promote's errors shown as a plain error dialog, and with the conflicting title (values
   only) */
#define GAME_ERROR_GC_PLAIN		-2146430696	/* 0x80101118 */
#define GAME_ERROR_GC_CONFLICT		-2146430642	/* 0x8010114E */

/*
 * The results a promote's job ends with that its end handles itself (values only; names ours from
 * what gameGcPromoteFinish does with them): nothing to show; the card's application to update;
 * the system to update; the job stopped; the card's NPDRM license expired, shown as the card's
 * own error.
 */
#define GAME_ERROR_GC_QUIET		-2146430680	/* 0x80101128 */
#define GAME_ERROR_GC_APP_UPDATE	-2146430687	/* 0x80101121 */
#define GAME_ERROR_GC_STOPPED		-2144206822	/* 0x8032001A */
#define GAME_NPDRM_ERROR_EXPIRED	-2138636282	/* 0x80870006 */
#define GAME_ERROR_GC_LICENSE_EXPIRED	-2146430649	/* 0x80101147 */

/* the install-title notice's button that migrates the title, and the error shown when the
   migration does not start (values only; names ours) */
#define GAME_GC_PROMOTE_BUTTON_MIGRATE	4
#define GAME_ERROR_GC_MIGRATION		-2146430706	/* 0x8010110E */

/* applauncher's text of the installer stopped at a title (value only) */
#define GAME_GC_PROMOTE_MSG_INSTALL_TITLE	0x888126CA

/* where a game card's applications are */
#define GAME_GC_APP_DIR			"gro0:app"
#define GAME_GC_APPINST_DIR		"gro0:appinst"
#define GAME_GC_ACINST_DIR		"gro0:acinst"

/* the shell events the promoter listens to (values only; names ours from what it does) */
enum {
	GAME_GC_EVENT_INSERTED	= 9,	/* a game card was inserted */
	GAME_GC_EVENT_REMOVED	= 10,	/* the game card was taken out */
};

/* PLACEHOLDER names, defined in other files of the shell */
int gameGcGetPromoteCount(void);		/* 0x810FCB30 */
int shellGetGcTitleSet(void);			/* 0x81004D70: which of the system's own card titles it has */
int gamePowerManageAddCallback(void (*func)(void *arg), void *arg);	/* 0x81063A60 */
int gamePowerManageRemoveCallback(void (*func)(void *arg), void *arg);	/* 0x81063A8C */
SceUInt32 gameErrorGetMessageId(int error);	/* 0x81063AB8: applauncher's text of an error */
wstring shellFormatText(const wchar_t *format, ...);	/* 0x81343700: %s-style substitution */
static void gameGcPromoteOnPowerEvent(void *arg);
extern "C" int SceShell_548A3111(int event, ShellEventCallback callback, void *arg);
extern "C" int SceShell_F197032C(int event, ShellEventCallback callback, void *arg);
int shellAppEventServerRemoveListener(int event, ShellEventCallback callback, void *arg);

/*
 * A job of the promoter: a function run on the queue and one called with its result
 * (GAME_ERROR_CANCELED when it was canceled), each with the promoter.
 */
class GameGcPromoteJob : public job::JobItem {
public:
	typedef int (*RunFunc)(GameGcPromoteJob *job, void *arg);
	typedef void (*FinishFunc)(int result, void *arg);

	GameGcPromoteJob(const char *name, void *arg, RunFunc run, FinishFunc finish)
		: job::JobItem(name, NULL), m_run(run), m_finish(finish), m_reserved(0), m_arg(arg)
	{
	}

	virtual ~GameGcPromoteJob()
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
				result = GAME_ERROR_CANCELED;
			m_finish(result, m_arg);
		}
	}

	RunFunc m_run;
	FinishFunc m_finish;
	SceUInt32 m_reserved;
	void *m_arg;
};

/* the titles on the inserted cards the promoter knows of: each title ID to whether its promote is
   done (0: not yet) */
typedef map<string, int> GameGcPromoteTitles;

/* the dialog's update handler; PLACEHOLDER: not declared in the library's header yet */
extern "C" int SceCommonGuiDialog_17091699(int dialogId, CommonGuiDialogHandler *handler);

/*
 * A dialog handler that calls a function with the dialog's value and button and its two
 * arguments. PLACEHOLDER: defined in another file of the shell (its constructor 0x81063A30), names
 * ours.
 */
class GameDialogFuncHandler : public CommonGuiDialogHandler {
public:
	typedef void (*Func)(SceInt32 value, SceInt32 button, void *arg1, void *arg2);

	GameDialogFuncHandler(Func func, void *arg1, void *arg2);

	virtual void operator()(SceInt32 value, SceInt32 button);

	Func m_func;
	void *m_arg1;
	void *m_arg2;
};

/* the widget of the parent with the ID, when it is of the type */
#define GAME_GC_PROMOTE_FIND(parent, type, id)						\
	((parent)->FindChild(IDParam(s_empty_name, (id)), 0) != NULL &&			\
	 (parent)->FindChild(IDParam(s_empty_name, (id)), 0)->IsInherit(type::TypeName()) ?	\
	 (type *)(parent)->FindChild(IDParam(s_empty_name, (id)), 0) : NULL)

/* the dialog's part with the index, when it is of the type */
#define GAME_GC_PROMOTE_FIND_INDEX(dialogId, type, index)				\
	(SceCommonGuiDialog_8947FC23((dialogId), (index)) != NULL &&			\
	 SceCommonGuiDialog_8947FC23((dialogId), (index))->IsInherit(type::TypeName()) ?	\
	 (type *)SceCommonGuiDialog_8947FC23((dialogId), (index)) : NULL)

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

/* the progress last shown */
static float s_shownProgress;

/* A dialog's progress bar kept at the promoter's progress while the dialog is open. */
class GameGcPromoteProgressHandler : public CommonGuiDialogHandler {
public:
	GameGcPromoteProgressHandler(const float *progress) : m_progress(progress)
	{
	}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		if (m_progress == NULL || s_shownProgress == *m_progress)
			return;

		s_shownProgress = *m_progress;

		ui::ProgressBar *bar = (ui::ProgressBar *)GAME_GC_PROMOTE_FIND_INDEX(dialogId,
				ui::ProgressBar, GAME_GC_PROMOTE_INDEX_PROGRESS);

		if (bar == NULL)
			return;

		if (*m_progress == 0.0f) {
			bar->SetMaxValue(100.0f);
			bar->SetValue(0.0f, false, ui::ProgressBar::BAR_FORE);
		} else {
			bar->SetValue(*m_progress, true, ui::ProgressBar::BAR_FORE);
		}
	}

	const float *m_progress;
};

class GameGcPromote {
public:
	GameGcPromote(GamePlugin *owner);
	virtual ~GameGcPromote();

	int GetPendingTitles(vector<string> *titles);
	int StartPromote(bool inserted, bool resume);
	void LoadVcInfo();
	void SetDialogHold(bool hold);
	void ChangeGcStatus();
	void ReleaseTitles();
	void Reset();
	bool IsPromotable(const string &titleId);
	int LoadIcon(const string &path, bool keep, intrusive_ptr<graph::Surface> *icon);
	int GetEntryInfo(const GameGcPromoteEntry &entry, string *title,
			 intrusive_ptr<graph::Surface> *icon);
	int PromoteTitles(job::JobItem *job);
	int ListTitles(job::JobItem *job, const string &dir);
	int InstallCard(job::JobItem *job);
	int LoadCardInfo(job::JobItem *job);
	void GetErrorText(wstring *text, int error);
	void ShowError(int *dialogId, int error, bool frame);
	void LoadInstaller();
	int CheckInstalled(job::JobItem *job, bool empty, bool *installed, bool *update);
	int LoadTitleInfo(const string &appPath, string *stitle, intrusive_ptr<graph::Surface> *icon,
			  int *parentalLevel);
	int InstallTitles(job::JobItem *job, bool noProgress, const string &dir,
			  const LsdbAppInfoPtr &appInfo, bool installDialog, bool option);

	/*
	 * Whether a promote may start: at the home screen, with no application in front, no
	 * dialog held, no game card installer up and nothing stopping it.
	 */
	bool CanPromote()
	{
		s_mutex.Lock();
		bool atHome = m_atHome;
		bool held = m_dialogHold != 0;
		bool appFront = m_appFront;
		bool installing = m_installing;
		bool stopped = m_stopped;
		s_mutex.Unlock();

		if (!atHome || appFront == true || held == true || installing == true)
			return false;
		if (Plugin::Find("gamecard_installer_plugin") != NULL)
			return false;
		if (stopped)
			return false;
		return true;
	}

	/*
	 * A promote started when one may and there is one to do: of every title of a card just
	 * inserted, else of what is left.
	 */
	inline void TryPromote();
	inline void StartPending();

	/* topmenu kept from sleeping while a promote runs (its functions 31 and 32), counted */
	void TopmenuLock()
	{
		Plugin *topmenu = Plugin::Find("topmenu_plugin");

		if (topmenu != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

			if (iface != NULL) {
				s_mutex.Lock();
				if (m_topmenuLock == 0)
					iface->func[31]();
				m_topmenuLock++;
				s_mutex.Unlock();
			}
		}
	}

	void TopmenuUnlock()
	{
		Plugin *topmenu = Plugin::Find("topmenu_plugin");

		if (topmenu != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

			if (iface != NULL) {
				s_mutex.Lock();
				if (--m_topmenuLock == 0)
					iface->func[32]();
				s_mutex.Unlock();
			}
		}
	}

	/* the system kept awake while a promote runs, counted */
	void PowerLock()
	{
		s_mutex.Lock();
		if (m_powerLock == 0)
			sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
		m_powerLock++;
		s_mutex.Unlock();
	}

	void PowerUnlock()
	{
		s_mutex.Lock();
		if (--m_powerLock == 0)
			sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
		s_mutex.Unlock();
	}

	static thread::RMutex s_mutex;

	GamePlugin *m_owner;
	string m_title;
	intrusive_ptr<graph::Surface> m_icon;
	int m_parentalLevel;
	bool m_titleInfoLoaded;
	LsdbAppInfoPtr m_appInfo;
	job::JobQueue *m_jobQueue;
	common::SharedPtr<job::JobItem> m_job;
	int m_dialogId;
	int m_progressDialogId;
	int m_iconCount;		/* the icons before the promote */
	int m_iconCountAfter;		/* and after it */
	int m_userIconCount;		/* the user's icons after it */
	GameGcPromoteTitles m_titles;
	vector<string> m_promoted;
	string m_contentId;
	bool m_inserted;
	bool m_atHome;
	bool m_sysupchkLoaded;
	bool m_appFront;
	bool m_resume;
	bool m_vcInfoLoaded;
	bool m_installing;
	int m_state;
	int m_dialogHold;
	int m_busy;
	string m_installTitleId;
	float m_progress;
	bool m_noticeHeld;
	bool m_stopped;
	int m_topmenuLock;
	int m_powerLock;
};

/* the dialogs' pages (the common dialog library's resource IDs) */
static const CommonGuiDialogPage s_noticePages[4] = {
	{ { 0x3A918C2C, 0x438C7285, 0x91DA95F1 } },
	{ { 0x3A918C2C, 0x438C7285, 0x91FFECEB } },
	{ { 0x3A918C2C, 0x438C7285, 0x564A69EC } },
	{ { 0x3A918C2C, 0x438C7285, 0x5B84AD08 } },
};

static const CommonGuiDialogPage s_promotePage = {
	{ 0x9AC337E5, 0x438C7285, 0x6AE5E21C }
};

static const CommonGuiDialogPage s_promoteProgressPage = {
	{ 0x9AC337E5, 0x438C7285, 0x388D5CD6 }
};

static const CommonGuiDialogPage s_promoteProgressPage2 = {
	{ 0xE3650494, 0x438C7285, 0xA0C5B14F }
};

static const CommonGuiDialogPage s_installProgressPage = {
	{ 0xA9F5537C, 0x438C7285, 0xD2C44556 }
};

/* the dialogs' parts */
static ui::Dialog::PartsList s_noticeParts[] = {
	{ GAME_GC_PROMOTE_INDEX_ICON, IDParam((uint32_t)GAME_GC_PROMOTE_PART_ICON) },
	{ GAME_GC_PROMOTE_INDEX_TITLE, IDParam((uint32_t)GAME_GC_PROMOTE_PART_TITLE) },
	{ GAME_GC_PROMOTE_INDEX_PARTS26, IDParam((uint32_t)GAME_GC_PROMOTE_PART_PARTS3) },
	{ GAME_GC_PROMOTE_INDEX_TEXT, IDParam((uint32_t)GAME_GC_PROMOTE_PART_TEXT) },
	{ GAME_GC_PROMOTE_INDEX_BUTTON1, IDParam((uint32_t)GAME_GC_PROMOTE_PART_BUTTON1) },
	{ 0, IDParam() },
};

static ui::Dialog::PartsList s_noticeParts2[] = {
	{ GAME_GC_PROMOTE_INDEX_ICON, IDParam((uint32_t)GAME_GC_PROMOTE_PART_ICON) },
	{ GAME_GC_PROMOTE_INDEX_TITLE, IDParam((uint32_t)GAME_GC_PROMOTE_PART_TITLE) },
	{ GAME_GC_PROMOTE_INDEX_PARTS26, IDParam((uint32_t)GAME_GC_PROMOTE_PART_PARTS3) },
	{ GAME_GC_PROMOTE_INDEX_TEXT, IDParam((uint32_t)GAME_GC_PROMOTE_PART_TEXT) },
	{ GAME_GC_PROMOTE_INDEX_PARTS27, IDParam((uint32_t)GAME_GC_PROMOTE_PART_PARTS27) },
	{ GAME_GC_PROMOTE_INDEX_PARTS28, IDParam((uint32_t)GAME_GC_PROMOTE_PART_PARTS28) },
	{ GAME_GC_PROMOTE_INDEX_BUTTON1, IDParam((uint32_t)GAME_GC_PROMOTE_PART_BUTTON1) },
	{ 0, IDParam() },
};

static ui::Dialog::PartsList s_noticeParts3[] = {
	{ GAME_GC_PROMOTE_INDEX_ICON, IDParam((uint32_t)GAME_GC_PROMOTE_PART_ICON) },
	{ GAME_GC_PROMOTE_INDEX_TITLE, IDParam((uint32_t)GAME_GC_PROMOTE_PART_TITLE) },
	{ GAME_GC_PROMOTE_INDEX_PARTS26, IDParam((uint32_t)GAME_GC_PROMOTE_PART_PARTS3) },
	{ GAME_GC_PROMOTE_INDEX_BUTTON1, IDParam((uint32_t)GAME_GC_PROMOTE_PART_BUTTON1) },
	{ 0, IDParam() },
};

static ui::Dialog::PartsList s_noticeParts4[] = {
	{ GAME_GC_PROMOTE_INDEX_ICON, IDParam((uint32_t)GAME_GC_PROMOTE_PART_ICON) },
	{ GAME_GC_PROMOTE_INDEX_TITLE, IDParam((uint32_t)GAME_GC_PROMOTE_PART_TITLE) },
	{ GAME_GC_PROMOTE_INDEX_PARTS26, IDParam((uint32_t)GAME_GC_PROMOTE_PART_PARTS3) },
	{ GAME_GC_PROMOTE_INDEX_TEXT, IDParam((uint32_t)GAME_GC_PROMOTE_PART_TEXT) },
	{ GAME_GC_PROMOTE_INDEX_BUTTON1, IDParam((uint32_t)GAME_GC_PROMOTE_PART_BUTTON1) },
	{ GAME_GC_PROMOTE_INDEX_BUTTON2, IDParam((uint32_t)GAME_GC_PROMOTE_PART_BUTTON2) },
	{ 0, IDParam() },
};

static ui::Dialog::PartsList s_promoteParts[] = {
	{ GAME_GC_PROMOTE_INDEX_ICON, IDParam((uint32_t)GAME_GC_PROMOTE_PART_ICON) },
	{ GAME_GC_PROMOTE_INDEX_TITLE, IDParam((uint32_t)GAME_GC_PROMOTE_PART_TITLE) },
	{ GAME_GC_PROMOTE_INDEX_TEXT, IDParam((uint32_t)GAME_GC_PROMOTE_PART_TEXT) },
	{ 0, IDParam() },
};

static ui::Dialog::PartsList s_promoteProgressParts[] = {
	{ GAME_GC_PROMOTE_INDEX_ICON, IDParam((uint32_t)GAME_GC_PROMOTE_PART_ICON) },
	{ GAME_GC_PROMOTE_INDEX_TITLE, IDParam((uint32_t)GAME_GC_PROMOTE_PART_TITLE) },
	{ GAME_GC_PROMOTE_INDEX_TEXT, IDParam((uint32_t)GAME_GC_PROMOTE_PART_TEXT) },
	{ GAME_GC_PROMOTE_INDEX_PROGRESS, IDParam((uint32_t)GAME_GC_PROMOTE_PART_PARTS18) },
	{ GAME_GC_PROMOTE_INDEX_PROGRESS2, IDParam((uint32_t)GAME_GC_PROMOTE_PART_PARTS19) },
	{ 0, IDParam() },
};

static GameGcPromote *s_promote;

inline void GameGcPromote::StartPending()
{
	GameGcPromote *promote = s_promote;

	s_mutex.Lock();
	bool inserted = promote->m_inserted;
	bool resume = promote->m_resume;
	s_mutex.Unlock();

	int count = gameGcGetPromoteCount();

	if ((inserted == true && resume == false) || count > 0)
		StartPromote(inserted, resume);
}

inline void GameGcPromote::TryPromote()
{
	if (CanPromote())
		StartPending();
}

/* gamecard_installer_plugin once loaded, and whether it was loaded to install add-on content */
static Plugin *s_installer;
static int s_installerAc;


thread::RMutex GameGcPromote::s_mutex("SceGameGcPromote", 0);

/* the title a promote conflicted with, its ID and name, for the error notice (names ours) */
static string s_conflictTitleId;
static string s_conflictTitleName;

/* what the indicator shows of the game card: its icon and its parental level (names ours) */
struct GameGcIndicatorInfo {
	GameGcIndicatorInfo() : parentalLevel(0)
	{
	}

	GameGcIndicatorInfo(const intrusive_ptr<graph::Surface> &icon, int parentalLevel)
		: icon(icon), parentalLevel(parentalLevel)
	{
	}

	intrusive_ptr<graph::Surface> icon;
	int parentalLevel;
};

/* the indicator shows the game card's access, and a game card is in */
static bool s_gcIndicator;
static bool s_gcInserted;

static void gameGcPromoteOnGcEvent(int code, const void *data, SceSize size, void *arg);
static bool gameGcPromoteIsCanceled(void *arg);
static void gameGcPromoteOnProgress(void *arg, float progress);
static void gameGcPromoteOnErrorDialog(SceInt32 dialogId, SceInt32 button, void *arg1, void *arg2);
static void gameGcPromoteOnSysupchkPower(void *arg);
static void gameGcPromoteOnSysupchk(int result, void *arg);
static void gameGcPromoteOnInstallDialog(SceInt32 dialogId, SceInt32 button, void *arg1, void *arg2);
static void gameGcPromoteOnAppEvent(int code, const void *data, SceSize size, void *arg);
static void gameGcPromoteOnEvent6(int code, const void *data, SceSize size, void *arg);
static void gameGcPromoteOnAppStateEvent(int code, const void *data, SceSize size, void *arg);
static void gameGcPromoteOnEvent1(int code, const void *data, SceSize size, void *arg);
static void gameGcPromoteOnEvent2(int code, const void *data, SceSize size, void *arg);
static void gameGcPromoteOnEvent3(int code, const void *data, SceSize size, void *arg);
static void gameGcPromoteOnEvent11(int code, const void *data, SceSize size, void *arg);
static void gameGcPromoteOnEvent12(int code, const void *data, SceSize size, void *arg);
int gameGcPromoteRunPromotes(GameGcPromoteJob *job, void *arg);
int gameGcPromoteRunStart(GameGcPromoteJob *job, void *arg);
int gameGcPromoteRunMigration(GameGcPromoteJob *job, void *arg);
void gameGcPromoteFinish(int result, void *arg);
int gameGcPromoteRunLoadVcInfo(GameGcPromoteJob *job, void *arg);
void gameGcPromoteFinishLoadVcInfo(int result, void *arg);

GameGcPromote::GameGcPromote(GamePlugin *owner)
	: m_owner(owner), m_parentalLevel(0), m_titleInfoLoaded(false),
	  m_appInfo(new LsdbAppInfo), m_jobQueue(NULL), m_dialogId(-1), m_progressDialogId(-1),
	  m_iconCount(0), m_inserted(false), m_atHome(true), m_sysupchkLoaded(false), m_appFront(true),
	  m_resume(false), m_vcInfoLoaded(false), m_installing(false), m_state(4), m_dialogHold(0),
	  m_busy(0), m_progress(0), m_noticeHeld(false), m_stopped(false), m_topmenuLock(0),
	  m_powerLock(0)
{
	job::JobQueue::Option option;

	option.priority = 229;
	option.stackSize = 0x3000;
	option.numThreads = 1;
	m_jobQueue = new job::JobQueue("SceGameGcPromoteJobQueue", &option);

	SceShell_548A3111(GAME_GC_EVENT_INSERTED, gameGcPromoteOnGcEvent, this);
	SceShell_548A3111(GAME_GC_EVENT_REMOVED, gameGcPromoteOnGcEvent, this);
	shellAppEventServerAddListener(1, gameGcPromoteOnAppEvent, this);
	SceShell_7FE4E732(6, gameGcPromoteOnEvent6, this);
	SceShell_83D16CA8(7, gameGcPromoteOnAppStateEvent, this);
	SceShell_7FE4E732(1, gameGcPromoteOnEvent1, this);
	SceShell_7FE4E732(2, gameGcPromoteOnEvent2, this);
	SceShell_7FE4E732(3, gameGcPromoteOnEvent3, this);
	SceShell_7FE4E732(11, gameGcPromoteOnEvent11, this);
	SceShell_7FE4E732(12, gameGcPromoteOnEvent12, this);
}

/* The promoter made for the plugin (0x81020012). */
void gameGcPromoteCreate(GamePlugin *owner)
{
	s_promote = new GameGcPromote(owner);
}

/* The promoter deleted (0x81020036). */
void gameGcPromoteDestroy(void)
{
	if (s_promote != NULL)
		delete s_promote;
	s_promote = NULL;
}

GameGcPromote::~GameGcPromote()
{
	m_icon.clear();

	SceShell_D4C7D2B7(12, gameGcPromoteOnEvent12, this);
	SceShell_D4C7D2B7(11, gameGcPromoteOnEvent11, this);
	SceShell_D4C7D2B7(3, gameGcPromoteOnEvent3, this);
	SceShell_D4C7D2B7(2, gameGcPromoteOnEvent2, this);
	SceShell_D4C7D2B7(1, gameGcPromoteOnEvent1, this);
	SceShell_E3D3A162(7, gameGcPromoteOnAppStateEvent, this);
	SceShell_D4C7D2B7(6, gameGcPromoteOnEvent6, this);
	shellAppEventServerRemoveListener(1, gameGcPromoteOnAppEvent, this);
	SceShell_F197032C(GAME_GC_EVENT_REMOVED, gameGcPromoteOnGcEvent, this);
	SceShell_F197032C(GAME_GC_EVENT_INSERTED, gameGcPromoteOnGcEvent, this);

	if (m_jobQueue != NULL)
		delete m_jobQueue;
}

/* The titles whose promote is not done yet, into titles (0x810202B4). */
int GameGcPromote::GetPendingTitles(vector<string> *titles)
{
	titles->clear();

	thread::RMutexLock lock(&s_mutex);

	GameGcPromoteTitles::iterator end = m_titles.end();

	for (GameGcPromoteTitles::iterator it = m_titles.begin(); it != end; it++) {
		if (it->second == 0) {
			for (unsigned int i = 0; i < m_titles.size(); i++)
				titles->push_back(it->first);
		}
	}

	return 0;
}

/* The titles whose promote is not done yet (0x81020416). */
int gameGcPromoteGetPendingTitles(vector<string> *titles)
{
	if (s_promote == NULL)
		return GAME_ERROR_NOT_READY;

	return s_promote->GetPendingTitles(titles);
}

/* A title taken off the promoter's list once its application directory on gro0: is gone (0x81020438). */
void gameAppDirCleanup(string *appDir)
{
	if (s_promote == NULL)
		return;

	if (appDir->find("gro0:") == string::npos)
		return;

	string::size_type pos = appDir->rfind("/", string::npos, sce_paf_strlen("/"));
	string titleId = appDir->substr(pos + 1, appDir->length() - pos);
	GameGcPromote *promote = s_promote;

	thread::RMutexLock lock(&GameGcPromote::s_mutex);

	GameGcPromoteTitles::iterator it = promote->m_titles.find(titleId);

	if (it != promote->m_titles.end())
		promote->m_titles.erase(it->first);
}

/*
 * A promote started on the job queue: of every title on the card when one was just inserted
 * (Promotes), else of what is left to promote (StartPromote); undone when there is nothing to
 * do (0x8102054E).
 */
int GameGcPromote::StartPromote(bool inserted, bool resume)
{
	Plugin *topmenu = Plugin::Find("topmenu_plugin");

	if (topmenu != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

		if (iface != NULL)
			iface->func[27]();
	}
	gameTopmenuBusyBegin();
	m_busy++;

	PowerLock();
	TopmenuLock();

	if (m_job.get() == NULL) {
		if (inserted == true && resume == false) {
			m_job = common::SharedPtr<job::JobItem>(new GameGcPromoteJob("Promotes", this,
					gameGcPromoteRunPromotes, gameGcPromoteFinish));
		} else {
			if (gameGcGetPromoteCount() == 0)
				goto undo;
			m_job = common::SharedPtr<job::JobItem>(new GameGcPromoteJob("StartPromote", this,
					gameGcPromoteRunStart, gameGcPromoteFinish));
		}
		if (m_job.get() != NULL) {
			int ret = m_jobQueue->Enqueue(m_job);

			if (ret >= 0)
				return ret;
		}
	}

undo:
	if (m_topmenuLock != 0)
		TopmenuUnlock();
	if (m_powerLock != 0)
		PowerUnlock();
	if (m_busy > 0) {
		gameTopmenuBusyEnd();
		m_busy--;
	}

	return m_busy;
}

/* The card's information loaded on the job queue (0x81020950). */
void GameGcPromote::LoadVcInfo()
{
	common::SharedPtr<job::JobItem> job(new GameGcPromoteJob("LoadVcInfo", this,
			gameGcPromoteRunLoadVcInfo, gameGcPromoteFinishLoadVcInfo));

	if (job.get() != NULL)
		m_jobQueue->Enqueue(job);
}

/*
 * A dialog that holds the promotes back opened (hold) or closed; with the last one closed, the
 * card's information is loaded again and a promote started when one may (0x81020A48).
 */
void GameGcPromote::SetDialogHold(bool hold)
{
	if (hold)
		m_dialogHold++;
	else
		m_dialogHold--;

	if (m_dialogHold != 0 || s_promote == NULL)
		return;

	s_promote->LoadVcInfo();
	if (!s_promote->CanPromote())
		return;

	GameGcPromote *promote = s_promote;

	s_mutex.Lock();
	bool inserted = promote->m_inserted;
	bool resume = promote->m_resume;
	s_mutex.Unlock();

	int count = gameGcGetPromoteCount();

	if ((inserted == true && resume == false) || count > 0)
		s_promote->StartPromote(inserted, resume);
}

/* The promoter's dialog hold set (0x81020B16). */
void gameGcPromoteSetDialogHold(bool hold)
{
	if (s_promote != NULL)
		s_promote->SetDialogHold(hold);
}

/* The promoter's job queue, NULL without the promoter (0x81020B2E). */
job::JobQueue *gameGcPromoteGetJobQueue(void)
{
	if (s_promote == NULL)
		return NULL;

	return s_promote->m_jobQueue;
}

/*
 * A promote dialog's icon (with topmenu's frame over it when frame is set), title and text set,
 * each shown again, or only hidden when show is not set (0x81020B42).
 */
static __attribute__((noinline)) void gameGcPromoteDialogSet(int dialogId, const string &title,
				   const intrusive_ptr<graph::Surface> &icon, const wstring &text,
				   bool frame, bool show)
{
	if (dialogId == -1)
		return;

	ui::Widget *contents = SceCommonGuiDialog_8947FC23(dialogId, 4);

	if (contents == NULL)
		return;

	ui::Plane *plane = GAME_GC_PROMOTE_FIND(contents, ui::Plane, GAME_GC_PROMOTE_PART_ICON);

	if (plane != NULL) {
		if (show) {
			HideWidget(plane, NULL);
			((ui::Widget *)plane)->SetTexture(icon);
			if (frame) {
				Plugin *topmenu = Plugin::Find("topmenu_plugin");

				if (topmenu != NULL) {
					ShellPluginInterface *iface =
						(ShellPluginInterface *)topmenu->GetInterface(1);

					if (iface != NULL)
						((ui::Widget *)plane)->SetTexture(*((const intrusive_ptr<graph::Surface> *(*)(void))
								    iface->func[8])(), 0, 1);
				}
			}
			ShowWidget(plane, new Timer(0.0f));
		} else {
			HideWidget(plane, new Timer(0.0f));
		}
	}

	ui::Text *name = GAME_GC_PROMOTE_FIND(contents, ui::Text, GAME_GC_PROMOTE_PART_TITLE);

	if (name != NULL) {
		if (show) {
			HideWidget(name, NULL);

			wstring wtitle;

			common::Utf8ToUtf16(title, &wtitle, 0);
			name->SetString(wtitle);
			ShowWidget(name, new Timer(0.0f));
		} else {
			HideWidget(name, new Timer(0.0f));
		}
	}

	ui::Text *message = GAME_GC_PROMOTE_FIND(contents, ui::Text, GAME_GC_PROMOTE_PART_TEXT);

	if (message != NULL) {
		if (show) {
			HideWidget(message, NULL);
			message->SetString(text);
			ShowWidget(message, new Timer(0.0f));
		} else {
			HideWidget(message, new Timer(0.0f));
		}
	}
}

/*
 * A promote dialog opened over applauncher: with a progress bar kept at *progress when progress
 * is not NULL (in its second look when second is set). Its ID, or a negative error (0x810214CC).
 */
static __attribute__((noinline)) int gameGcPromoteDialogOpen(const string &title, intrusive_ptr<graph::Surface> icon,
				   const wstring &text, bool frame, const float *progress,
				   bool second)
{
	Plugin *plugin = Plugin::Find("applauncher_plugin");
	CommonGuiDialogLayout layout;

	sce_paf_memset(&layout, 0, sizeof(layout));
	if (progress != NULL)
		layout.page = second ? &s_promoteProgressPage2 : &s_promoteProgressPage;
	else
		layout.page = &s_promotePage;
	layout.parts = progress != NULL ? s_promoteProgressParts : s_promoteParts;

	CommonGuiLayoutDialogParam param;

	param.resourcePlugin = plugin;
	param.text = text;
	param.layout = layout;
	param.dimmerColor[0] = 0.0f;
	param.dimmerColor[1] = 0.0f;
	param.dimmerColor[2] = 0.0f;
	param.dimmerColor[3] = 0.3f;
	param.priority = 34;
	param.pageOption = 6;

	int dialogId = SceCommonGuiDialog_CCFCD9A6(&param);

	if (dialogId < 0)
		return dialogId;

	if (SceCommonGuiDialog_8947FC23(dialogId, GAME_GC_PROMOTE_INDEX_PROGRESS) != NULL)
		SceCommonGuiDialog_17091699(dialogId, new GameGcPromoteProgressHandler(progress));

	gameGcPromoteDialogSet(dialogId, title, icon, text, frame, true);

	return dialogId;
}

/* The install dialog, with its progress bar kept at *progress: its ID, or a negative error
   (0x810216CE). */
static __attribute__((noinline)) int gameGcPromoteDialogOpenInstall(const string &title,
					  intrusive_ptr<graph::Surface> icon,
					  const wstring &text, bool frame, const float *progress)
{
	Plugin *plugin = Plugin::Find("applauncher_plugin");
	CommonGuiDialogLayout layout;

	sce_paf_memset(&layout, 0, sizeof(layout));
	layout.page = &s_installProgressPage;
	layout.parts = s_promoteProgressParts;

	CommonGuiLayoutDialogParam param;

	param.resourcePlugin = plugin;
	param.text = text;
	param.layout = layout;
	param.dimmerColor[0] = 0.0f;
	param.dimmerColor[1] = 0.0f;
	param.dimmerColor[2] = 0.0f;
	param.dimmerColor[3] = 0.3f;
	param.priority = 34;
	param.pageOption = 6;

	int dialogId = SceCommonGuiDialog_CCFCD9A6(&param);

	if (dialogId < 0)
		return dialogId;

	if (SceCommonGuiDialog_8947FC23(dialogId, GAME_GC_PROMOTE_INDEX_PROGRESS) != NULL)
		SceCommonGuiDialog_17091699(dialogId, new GameGcPromoteProgressHandler(progress));

	gameGcPromoteDialogSet(dialogId, title, icon, text, frame, true);

	return dialogId;
}

/* A promote dialog let go (0x810218A6). */
static __attribute__((noinline)) void gameGcPromoteDialogClose(int dialogId)
{
	if (dialogId != -1)
		SceCommonGuiDialog_BC311DC0(dialogId);
}

/*
 * A notice dialog's parts set and shown: its icon (with topmenu's frame when frame is set), title
 * and, when showText is set, text; the second icon when there is one (else hidden and sized to
 * nothing), and the second text when it is not empty (0x810218B4).
 */
static __attribute__((noinline)) void gameGcPromoteNoticeSet(int dialogId, intrusive_ptr<graph::Surface> icon,
				   const wstring &title, const wstring &text, bool showText,
				   bool frame, const intrusive_ptr<graph::Surface> *icon2,
				   bool frame2, const wstring &text2)
{
	if (Plugin::Find("applauncher_plugin") == NULL)
		return;

	ui::Widget *contents = SceCommonGuiDialog_8947FC23(dialogId, 4);

	if (contents == NULL)
		return;

	ui::Plane *plane = GAME_GC_PROMOTE_FIND(contents, ui::Plane, GAME_GC_PROMOTE_PART_ICON);

	if (plane != NULL) {
		HideWidget(plane, NULL);
		((ui::Widget *)plane)->SetTexture(icon);
		if (frame) {
			Plugin *topmenu = Plugin::Find("topmenu_plugin");

			if (topmenu != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)topmenu->GetInterface(1);

				if (iface != NULL)
					((ui::Widget *)plane)->SetTexture(
						*((const intrusive_ptr<graph::Surface> *(*)(void))
						  iface->func[8])(), 0, 1);
			}
		}
		ShowWidget(plane, new Timer(0.0f));
	}

	ui::Text *name = GAME_GC_PROMOTE_FIND(contents, ui::Text, GAME_GC_PROMOTE_PART_TITLE);

	if (name != NULL) {
		HideWidget(name, NULL);
		name->SetString(title);
		ShowWidget(name, new Timer(0.0f));
	}

	if (showText) {
		ui::Text *message = GAME_GC_PROMOTE_FIND(contents, ui::Text,
							 GAME_GC_PROMOTE_PART_PARTS3);

		if (message != NULL) {
			HideWidget(message, NULL);
			message->SetString(text);
			ShowWidget(message, new Timer(0.0f));
		}
	}

	if (icon2 != NULL) {
		ui::Plane *plane2 = GAME_GC_PROMOTE_FIND(contents, ui::Plane,
							 GAME_GC_PROMOTE_PART_PARTS27);

		if (plane2 != NULL) {
			HideWidget(plane2, NULL);
			((ui::Widget *)plane2)->SetTexture(*icon2);
			if (frame2) {
				Plugin *topmenu = Plugin::Find("topmenu_plugin");

				if (topmenu != NULL) {
					ShellPluginInterface *iface =
						(ShellPluginInterface *)topmenu->GetInterface(1);

					if (iface != NULL)
						((ui::Widget *)plane2)->SetTexture(
							*((const intrusive_ptr<graph::Surface> *(*)(void))
							  iface->func[8])(), 0, 1);
				}
			}
			ShowWidget(plane2, new Timer(0.0f));
		}
	} else {
		ui::Plane *plane2 = GAME_GC_PROMOTE_FIND(contents, ui::Plane,
							 GAME_GC_PROMOTE_PART_PARTS27);

		if (plane2 != NULL) {
			HideWidget(plane2, NULL);
			plane2->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		}
	}

	if (text2.length() != 0) {
		ui::Text *message2 = GAME_GC_PROMOTE_FIND(contents, ui::Text,
							  GAME_GC_PROMOTE_PART_PARTS28);

		if (message2 != NULL) {
			HideWidget(message2, NULL);
			message2->SetString(text2);
			ShowWidget(message2, new Timer(0.0f));
		}
	}
}

/*
 * A notice dialog opened in place of *dialogId, its buttons passed to func: its message, when
 * there is one, under the parts gameGcPromoteNoticeSet sets. 0, or a negative error
 * (0x8102265A).
 */
static __attribute__((noinline)) int gameGcPromoteNoticeOpen(int *dialogId, GameDialogFuncHandler::Func func, void *arg1,
				   void *arg2, intrusive_ptr<graph::Surface> icon,
				   const wstring &title, const wstring &text, const wstring &message,
				   bool frame, const intrusive_ptr<graph::Surface> *icon2, bool frame2,
				   const wstring &text2)
{
	if (*dialogId != -1) {
		SceCommonGuiDialog_BC311DC0(*dialogId);
		*dialogId = -1;
	}

	Plugin *topmenu = Plugin::Find("topmenu_plugin");
	Plugin *applauncher = Plugin::Find("applauncher_plugin");

	if (topmenu == NULL || applauncher == NULL)
		return GAME_ERROR_NOT_READY;

	if (message.length() == 0) {
		CommonGuiLayoutDialogParam param;

		param.resourcePlugin = applauncher;
		param.layout.page = &s_noticePages[2];
		param.layout.parts = s_noticeParts3;
		param.priority = 34;
		param.dimmerColor[0] = 0.0f;
		param.dimmerColor[1] = 0.0f;
		param.dimmerColor[2] = 0.0f;
		param.dimmerColor[3] = 0.3f;
		param.notifyClose = true;
		param.pageOption = 6;
		param.handler = new GameDialogFuncHandler(func, arg1, arg2);

		int ret = SceCommonGuiDialog_CCFCD9A6(&param);

		if (ret < 0)
			return ret;

		*dialogId = ret;
	} else {
		CommonGuiLayoutDialogParam param;

		param.resourcePlugin = applauncher;
		if (icon2 != NULL || text2.length() != 0) {
			param.layout.page = &s_noticePages[1];
			param.layout.parts = s_noticeParts2;
		} else {
			param.layout.page = &s_noticePages[0];
			param.layout.parts = s_noticeParts;
		}
		param.priority = 34;
		param.dimmerColor[0] = 0.0f;
		param.dimmerColor[1] = 0.0f;
		param.dimmerColor[2] = 0.0f;
		param.dimmerColor[3] = 0.3f;
		param.notifyClose = true;
		param.pageOption = 6;
		param.handler = new GameDialogFuncHandler(func, arg1, arg2);
		param.text = message;

		int ret = SceCommonGuiDialog_CCFCD9A6(&param);

		if (ret < 0)
			return ret;

		*dialogId = ret;
	}

	gameGcPromoteNoticeSet(*dialogId, icon, title, text, true, frame, icon2, frame2, text2);

	return 0;
}

/* The promoter's notices held back or let through (0x81022A12). */
void gameGcPromoteSetNoticeHeld(bool held)
{
	if (s_promote != NULL)
		s_promote->m_noticeHeld = held;
}

/* gamecard_installer_plugin loaded: topmenu shows it, unless it installs add-on content
   (0x81022A24). */
static void gameGcPromoteOnInstallerLoad(Plugin *plugin)
{
	if (s_installerAc != 0)
		return;

	Plugin *topmenu = Plugin::Find("topmenu_plugin");

	if (topmenu == NULL)
		return;

	ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

	if (iface == NULL)
		return;

	((void (*)(Plugin *, int, int, int, int))iface->func[52])(plugin, 42, 1, 0, 0);
	s_installer = plugin;
}

/* A promote of what the card has left started on the job queue, when one may (0x81022A70). */
void GameGcPromote::ChangeGcStatus()
{
	if (!CanPromote())
		return;

	if (gameGcGetPromoteCount() == 0)
		return;

	m_job = common::SharedPtr<job::JobItem>(new GameGcPromoteJob("ChangeGcStatus", this,
			gameGcPromoteRunStart, gameGcPromoteFinish));
	if (m_job.get() != NULL)
		m_jobQueue->Enqueue(m_job);
}

/*
 * gamecard_installer_plugin unloaded: topmenu and the system let go, the card's drives unmounted
 * when it has no application, and a promote of what is left started (0x81022C38).
 */
static void gameGcPromoteOnInstallerUnload(void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;

	if (promote == NULL)
		return;

	if (promote->m_topmenuLock != 0)
		promote->TopmenuUnlock();
	if (promote->m_busy > 0) {
		gameTopmenuBusyEnd();
		promote->m_busy--;
	}

	if (s_installerAc == 0) {
		Plugin *topmenu = Plugin::Find("topmenu_plugin");

		if (topmenu != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

			if (iface != NULL) {
				((void (*)(Plugin *, int))iface->func[53])(s_installer, 230);
				s_installer = NULL;
			}
		}
	}

	if (!gameDirHasSubDirs(string("gro0:app"))) {
		if (promote->m_vcInfoLoaded) {
			promote->m_vcInfoLoaded = false;
			promote->m_inserted = false;
			gameUnmountGameDrives();
		}
		if (s_gcIndicator) {
			Plugin *indicator = Plugin::Find("indicator_plugin");

			if (indicator != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)indicator->GetInterface(1);

				if (iface != NULL)
					((void (*)(int))iface->func[25])(0);
			}
		}
	}

	promote->ChangeGcStatus();
}

/* Whether a title of the card is one to promote: not the system's own card title for its set
   (0x81022D90). */
bool GameGcPromote::IsPromotable(const string &titleId)
{
	switch (shellGetGcTitleSet()) {
	case 1:
		if (titleId == "PCSI90015")
			return false;
		if (titleId == "PCSI90016")
			return false;
		break;
	case 2:
		if (titleId == "PCSI90015")
			return false;
		break;
	default:
		if (titleId == "PCSI90016")
			return false;
		break;
	}

	return true;
}

/* topmenu's function 49: whether the home screen is in front */
static inline bool gameTopmenuAtHome(void)
{
	Plugin *topmenu = Plugin::Find("topmenu_plugin");

	if (topmenu == NULL)
		return true;

	ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

	if (iface == NULL)
		return true;

	return ((int (*)(void))iface->func[49])() != 0;
}

/* Shell event 6: the home screen back in front, and a promote started when one may
   (0x81022E20). */
static void gameGcPromoteOnEvent6(int code, const void *data, SceSize size, void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;

	if (!gameTopmenuAtHome())
		return;

	GameGcPromote::s_mutex.Lock();
	promote->m_atHome = true;
	GameGcPromote::s_mutex.Unlock();

	promote->TryPromote();
}

/* An application came to the front: the home screen is up when it is the home screen itself,
   else when topmenu says so; no install runs any more (0x81022EF8). */
static void gameGcPromoteOnAppEvent(int code, const void *data, SceSize size, void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;
	bool atHome;

	if (sce_paf_strncmp((const char *)data, GAME_HOME_TITLE_ID,
			    sizeof(GAME_HOME_TITLE_ID)) != 0) {
		atHome = false;

		Plugin *topmenu = Plugin::Find("topmenu_plugin");
		ShellPluginInterface *iface;

		if (topmenu == NULL ||
		    (iface = (ShellPluginInterface *)topmenu->GetInterface(1)) == NULL ||
		    ((int (*)(void))iface->func[49])() != 0)
			atHome = true;

		GameGcPromote::s_mutex.Lock();
		promote->m_installing = false;
		GameGcPromote::s_mutex.Unlock();
	} else {
		atHome = true;
	}

	GameGcPromote::s_mutex.Lock();
	promote->m_atHome = atHome;
	GameGcPromote::s_mutex.Unlock();
}

/* Shell event 1: a promote started when one may, under the main thread's lock (0x81022F76). */
static void gameGcPromoteOnEvent1(int code, const void *data, SceSize size, void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;

	thread::RMutex::main_thread_mutex.Lock();
	promote->TryPromote();
	thread::RMutex::main_thread_mutex.Unlock();
}

/* An application state event: the home screen left (0x8102302C). */
static void gameGcPromoteOnAppStateEvent(int code, const void *data, SceSize size, void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;

	GameGcPromote::s_mutex.Lock();
	promote->m_atHome = false;
	GameGcPromote::s_mutex.Unlock();
}

/* Shell event 2: an application in front (0x8102304C). */
static void gameGcPromoteOnEvent2(int code, const void *data, SceSize size, void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;

	promote->m_appFront = true;
}

/* Shell event 3: no application in front any more; the card's information loaded again and a
   promote started when one may (0x81023054). */
static void gameGcPromoteOnEvent3(int code, const void *data, SceSize size, void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;

	promote->m_appFront = false;
	promote->LoadVcInfo();
	promote->TryPromote();
}

/*
 * Shell event 11: an install started. Sony's code keeps the listener's argument itself as the
 * installing title (0x810230FE).
 */
static void gameGcPromoteOnEvent11(int code, const void *data, SceSize size, void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;

	GameGcPromote::s_mutex.Lock();
	promote->m_installing = true;
	promote->m_installTitleId = (const char *)arg;
	GameGcPromote::s_mutex.Unlock();
}

/* Shell event 12: the install of the title event 11 kept is over (0x8102313C). */
static void gameGcPromoteOnEvent12(int code, const void *data, SceSize size, void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;

	GameGcPromote::s_mutex.Lock();
	if (promote->m_installTitleId == (const char *)arg) {
		promote->m_installing = false;
		promote->m_installTitleId.clear();
	}
	GameGcPromote::s_mutex.Unlock();
}

/* Every title of the card given back to applauncher (0x81023186). */
void GameGcPromote::ReleaseTitles()
{
	Plugin *applauncher = Plugin::Find("applauncher_plugin");

	if (applauncher == NULL)
		return;

	ShellPluginInterface *iface = (ShellPluginInterface *)applauncher->GetInterface(1);

	if (iface == NULL)
		return;

	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (info.get() == NULL)
		return;

	s_mutex.Lock();

	GameGcPromoteTitles::iterator end = m_titles.end();

	for (GameGcPromoteTitles::iterator it = m_titles.begin(); it != end; ++it)
		((void (*)(const string *))iface->func[5])(&it->first);
	s_mutex.Unlock();
}

/*
 * The card taken out: its job canceled, the progress dialog closed, its titles given back,
 * the application of the card in front closed in impose when it runs from the card, and what
 * the promoter knew of the card forgotten (0x810232CA).
 */
void GameGcPromote::Reset()
{
	if (m_job.get() != NULL)
		m_job->Cancel();

	if (m_progressDialogId != -1) {
		gamePowerManageRemoveCallback(gameGcPromoteOnPowerEvent, this);
		SceCommonGuiDialog_BC311DC0(m_progressDialogId);
		m_progressDialogId = -1;
		m_stopped = false;
	}

	ReleaseTitles();

	s_mutex.Lock();

	SceAppMgrAppStatus status;

	sce_paf_memset(&status, 0, sizeof(status));
	if (sceAppMgrGetStatusByAppId(-3, &status) == 0) {
		GameGcPromoteTitles::iterator it = m_titles.find(string(status.appName));
		int storage;

		if (it != m_titles.end() && gameTitleGetStorage(status.appName, &storage) == 0 &&
		    (storage == 1 || storage == 2)) {
			Plugin *impose = Plugin::Find("impose_plugin");

			if (impose != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)impose->GetInterface(1);

				if (iface != NULL)
					((void (*)(const char *))iface->func[8])(status.appName);
			}
		}
	}

	m_titles.clear();
	m_contentId.clear();
	m_appInfo->clear();

	s_mutex.Unlock();

	if (m_sysupchkLoaded) {
		if (Plugin::Find("sysupchk_plugin") != NULL)
			Plugin::UnloadAsync("sysupchk_plugin", NULL, 0);
		m_sysupchkLoaded = false;
	}

	m_resume = false;
}

/*
 * A game card inserted (9: its information loaded again and a promote started when one may)
 * or taken out (10: the promoter reset and the indicator's card access turned off)
 * (0x8102342E).
 */
static void gameGcPromoteOnGcEvent(int code, const void *data, SceSize size, void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;

	if (code == GAME_GC_EVENT_INSERTED) {
		GameGcPromote::s_mutex.Lock();
		promote->m_inserted = true;
		s_gcInserted = true;
		GameGcPromote::s_mutex.Unlock();

		promote->LoadVcInfo();
		if (promote->CanPromote()) {
			gameGcPromoteSetNoticeHeld(false);

			GameGcPromote *current = s_promote;

			GameGcPromote::s_mutex.Lock();
			bool inserted = current->m_inserted;
			bool resume = current->m_resume;
			GameGcPromote::s_mutex.Unlock();

			int count = gameGcGetPromoteCount();

			if ((inserted == true && resume == false) || count > 0)
				promote->StartPromote(true, resume);
		}
	} else {
		GameGcPromote::s_mutex.Lock();
		promote->m_inserted = false;
		s_gcInserted = false;
		GameGcPromote::s_mutex.Unlock();

		promote->Reset();

		if (s_gcIndicator) {
			Plugin *indicator = Plugin::Find("indicator_plugin");

			if (indicator != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)indicator->GetInterface(1);

				if (iface != NULL) {
					iface->func[24]();
					s_gcIndicator = false;
				}
			}
		}

		gameGcPromoteSetNoticeHeld(false);
	}
}

/*
 * An icon file loaded into the plugin's surface pool, kept as it is when keep is set: 0, or a
 * negative error (0x81023574).
 */
int GameGcPromote::LoadIcon(const string &path, bool keep, intrusive_ptr<graph::Surface> *icon)
{
	int ret = 0;
	common::SharedPtr<File> file;
	graph::Surface::LoadOption option;

	file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &ret);

	if (ret >= 0) {
		option.reserved5 = keep;
		*icon = graph::Surface::Load(m_owner->m_plugin->m_surface_pool, file, &option);
		if (option.error < 0)
			ret = option.error;
		else
			ret = 0;
	}

	if (file.get() != NULL)
		file->Close();

	return ret;
}

/* A title's name and icon, indicator's when it has none of its own (0x810237A4). */
int GameGcPromote::GetEntryInfo(const GameGcPromoteEntry &entry, string *title,
				intrusive_ptr<graph::Surface> *icon)
{
	*title = entry.title;
	icon->clear();

	if (LoadIcon(entry.iconPath, false, icon) != 0) {
		Plugin *indicator = Plugin::Find("indicator_plugin");

		if (indicator != NULL)
			*icon = indicator->GetTexture(IDParam(s_empty_name, GAME_GC_PROMOTE_TEX_NO_ICON));
	}

	return 0;
}

/*
 * Every title of the card promoted, the list read again until nothing is left, in a dialog
 * showing the title and its progress: 0, or the first error (0x81023892).
 */
int GameGcPromote::PromoteTitles(job::JobItem *job)
{
	int ret;

	if (gameGcGetPromoteCount() == 0)
		return 0;

	if (m_dialogId != -1) {
		thread::RMutex::main_thread_mutex.Lock();
		if (m_dialogId != -1)
			SceCommonGuiDialog_BC311DC0(m_dialogId);
		m_dialogId = -1;
		thread::RMutex::main_thread_mutex.Unlock();
	}

	bool again = false;
	Plugin *applauncher = Plugin::Find("applauncher_plugin");

	for (;;) {
		vector<GameGcPromoteEntry> entries;

		ret = gameGcGetPromoteEntries(&entries);
		if (ret < 0) {
			if (ret != GAME_ERROR_NO_PROMOTE)
				goto out;
			ret = 0;
			goto out;
		}

		if (entries.size() == 0) {
			ret = 0;
			goto out;
		}

		for (SceSize i = 0; i < entries.size(); i++) {
			GameGcPromoteEntry &entry = entries[i];

			m_icon.clear();
			m_title = "-";

			ret = GetEntryInfo(entry, &m_title, &m_icon);
			if (ret < 0)
				goto out;

			m_progress = 0;

			if (!again && i == 0) {
				thread::RMutex::main_thread_mutex.Lock();
				ret = gameGcPromoteDialogOpen(m_title, m_icon,
					wstring(applauncher->GetString(IDParam(s_empty_name,
						GAME_GC_PROMOTE_MSG_PROMOTING))),
					false, &m_progress, false);
				if (ret < 0) {
					thread::RMutex::main_thread_mutex.Unlock();
					goto out;
				}
				m_dialogId = ret;
				thread::RMutex::main_thread_mutex.Unlock();
			} else {
				thread::RMutex::main_thread_mutex.Lock();
				gameGcPromoteDialogSet(m_dialogId, m_title, m_icon,
					wstring(applauncher->GetString(IDParam(s_empty_name,
						GAME_GC_PROMOTE_MSG_PROMOTING))),
					false, true);
				thread::RMutex::main_thread_mutex.Unlock();
			}

			entry.progressArg = this;
			entry.progressFunc = gameGcPromoteOnProgress;

			ret = gameGcPromoteEntry(&entry, gameGcPromoteIsCanceled, job);
			if (ret < 0) {
				if (entries[i].type == GAME_GC_PROMOTE_TYPE_STOP)
					m_state = 3;
				goto out;
			}

			if (applauncher != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)applauncher->GetInterface(1);

				if (iface == NULL) {
					ret = GAME_ERROR_NOT_READY;
					goto out;
				}
				((void (*)(const string *, int))iface->func[4])(&entry.titleId, 1);
			}

			thread::Sleep(1000);

			thread::RMutex::main_thread_mutex.Lock();
			gameGcPromoteDialogSet(m_dialogId, m_title, m_icon,
				wstring(applauncher->GetString(IDParam(s_empty_name,
					GAME_GC_PROMOTE_MSG_PROMOTING))),
				false, false);
			thread::RMutex::main_thread_mutex.Unlock();
		}

		again = true;
	}

out:
	if (m_dialogId != -1) {
		thread::RMutex::main_thread_mutex.Lock();
		if (m_dialogId != -1)
			SceCommonGuiDialog_BC311DC0(m_dialogId);
		m_dialogId = -1;
		thread::RMutex::main_thread_mutex.Unlock();
	}

	return ret;
}

/*
 * Whether a title of the card is installed already (the first the system knows), and whether
 * the card's update is to be installed over it or over the card's own application; applauncher
 * is told of the installed title unless a notice was held: 0, or a negative error (0x810245AE).
 */
int GameGcPromote::CheckInstalled(job::JobItem *job, bool empty, bool *installed, bool *update)
{
	string titleId(s_empty_name);
	string firstId(s_empty_name);

	*installed = false;
	*update = false;

	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (info.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	s_mutex.Lock();

	GameGcPromoteTitles::iterator end = m_titles.end();
	GameGcPromoteTitles::iterator it = m_titles.begin();

	if (m_titles.size() == 0) {
		s_mutex.Unlock();
		return GAME_ERROR_NO_GC_TITLE;
	}

	firstId = it->first;
	for (; it != end; ++it) {
		if (gameAppGetInfo(it->first.c_str(), &info) == 0) {
			titleId = it->first;
			*installed = true;
			break;
		}
	}

	s_mutex.Unlock();

	int storage = 0;

	if (*installed) {
		if (gameAppGetStorage(&info, &storage) == 0)
			*update = gameAppIsUpdateNeeded(&info, titleId, &storage);
	} else {
		info->clear();

		string sfoPath = string("gro0:app/") + firstId + string("/sce_sys/param.sfo");

		if (gameSfoLoad(string(sfoPath.c_str()), gameSfoGetAttributeParams(),
				gameSfoGetCheckParamCount(), &info) == 0) {
			LsdbAppInfo::iterator attr = info->find(LSDB_KEY_ATTRIBUTE);

			if (attr != info->end()) {
				storage = (gameLsdbValueInteger(attr->second) & 1) ? 2 : 1;
				*update = gameAppIsUpdateNeeded(&info, firstId, &storage);
			}
		}
	}

	bool held = m_noticeHeld;

	m_noticeHeld = false;

	if (*installed && !held) {
		Plugin *applauncher = Plugin::Find("applauncher_plugin");

		if (applauncher != NULL) {
			ShellPluginInterface *iface =
				(ShellPluginInterface *)applauncher->GetInterface(1);

			if (iface == NULL)
				return GAME_ERROR_NOT_READY;
			((void (*)(const string *, int))iface->func[4])(&titleId, 0);
		}
	}

	return 0;
}

/*
 * The card's titles, the directories in dir, listed as not promoted yet unless the promoter
 * knows them already: 0, or a negative error (0x81024C24).
 */
int GameGcPromote::ListTitles(job::JobItem *job, const string &dir)
{
	s_mutex.Lock();
	SceSize size = m_titles.size();
	s_mutex.Unlock();

	if (size != 0)
		return 0;

	SceUInt32 count = 0;
	Dir d;
	int ret = d.Open(dir.c_str());

	if (ret < 0)
		return ret;

	DirEnt entry;

	while (d.Read(&entry) == 0) {
		if (entry.type == DirEnt::Type_Dir && sce_paf_strncmp(entry.name.c_str(), ".", 1) != 0) {
			if (job->IsCanceled()) {
				s_mutex.Lock();
				m_titles.clear();
				s_mutex.Unlock();
				return GAME_ERROR_CANCELED;
			}

			s_mutex.Lock();
			m_titles[entry.name] = GAME_GC_TITLE_NOT_PROMOTED;
			s_mutex.Unlock();

			if (++count >= GAME_GC_TITLES_MAX)
				break;
		}
	}
	d.Close();

	if (count != 0)
		return 0;

	return GAME_ERROR_NO_GC_TITLE;
}

/*
 * Every title of the card the promoter knows installed from dir, in a dialog showing the title
 * and its progress (the install dialog when installDialog is set, no progress bar when
 * noProgress is), each title's new state kept: 0, or the first error (0x81023D4C).
 */
int GameGcPromote::InstallTitles(job::JobItem *job, bool noProgress, const string &dir,
				 const LsdbAppInfoPtr &appInfo, bool installDialog, bool option)
{
	int ret;

	s_mutex.Lock();
	GameGcPromoteTitles titles(m_titles);
	s_mutex.Unlock();

	string path;
	GamePromoteParam param;
	SceUInt32 text = GAME_GC_PROMOTE_MSG_PROMOTING;

	if (noProgress)
		text = GAME_GC_PROMOTE_MSG_PROMOTING2;

	Plugin *applauncher = Plugin::Find("applauncher_plugin");
	GameGcPromoteTitles::iterator end = titles.end();

	for (GameGcPromoteTitles::iterator it = titles.begin(); it != end; ++it) {
		if (job != NULL && job->IsCanceled()) {
			ret = GAME_ERROR_CANCELED;
			goto out;
		}

		if (it->second == 0)
			continue;

		if (!IsPromotable(it->first)) {
			ret = GAME_ERROR_CANCELED;
			goto out;
		}

		path = dir + string("/") + it->first;

		if (m_icon.get() == NULL) {
			ret = GAME_ERROR_NOT_READY;
			goto out;
		}

		m_progress = 0;

		if (it == titles.begin()) {
			thread::RMutex::main_thread_mutex.Lock();
			if (installDialog)
				ret = gameGcPromoteDialogOpenInstall(m_title, m_icon,
					wstring(applauncher->GetString(IDParam(s_empty_name,
						GAME_GC_PROMOTE_MSG_INSTALLING))),
					true, &m_progress);
			else
				ret = gameGcPromoteDialogOpen(m_title, m_icon,
					wstring(applauncher->GetString(IDParam(s_empty_name, text))),
					true, noProgress ? NULL : &m_progress, false);
			if (ret < 0) {
				thread::RMutex::main_thread_mutex.Unlock();
				return ret;
			}
			m_dialogId = ret;
			thread::RMutex::main_thread_mutex.Unlock();
		} else {
			thread::RMutex::main_thread_mutex.Lock();
			gameGcPromoteDialogSet(m_dialogId, m_title, m_icon,
				wstring(applauncher->GetString(IDParam(s_empty_name, text))),
				true, true);
			thread::RMutex::main_thread_mutex.Unlock();
		}

		sce_paf_memset(&param, 0, sizeof(param));
		sce_paf_strncpy(param.path, path.c_str(), sizeof(param.path) - 1);
		param.path[sizeof(param.path) - 1] = '\0';
		param.appInfo = appInfo;
		param.option = option;
		param.progressFunc = gameGcPromoteOnProgress;
		param.progressArg = this;

		ret = gamePluginPromoteParam(&param, gameGcPromoteIsCanceled, job);
		if (ret < 0) {
			if (ret == GAME_ERROR_INSTALL_TITLE) {
				s_mutex.Lock();
				m_contentId = it->first;
				s_mutex.Unlock();
			}
			goto out;
		}

		s_mutex.Lock();
		GameGcPromoteTitles::iterator found = m_titles.find(it->first);

		if (found != m_titles.end())
			found->second = ret;
		s_mutex.Unlock();

		thread::Sleep(1000);

		thread::RMutex::main_thread_mutex.Lock();
		gameGcPromoteDialogSet(m_dialogId, m_title, m_icon,
			wstring(applauncher->GetString(IDParam(s_empty_name, text))), false, false);
		thread::RMutex::main_thread_mutex.Unlock();
	}

	ret = 0;

out:
	if (m_dialogId != -1) {
		thread::RMutex::main_thread_mutex.Lock();
		if (m_dialogId != -1)
			SceCommonGuiDialog_BC311DC0(m_dialogId);
		m_dialogId = -1;
		thread::RMutex::main_thread_mutex.Unlock();
	}

	return ret;
}

/*
 * The titles of a card just inserted installed from it, when it was not resumed: listed,
 * checked against what is installed and installed; a failed install marked (0x81024DAA).
 */
int GameGcPromote::InstallCard(job::JobItem *job)
{
	s_mutex.Lock();
	bool inserted = m_inserted;
	bool resume = m_resume;
	s_mutex.Unlock();

	if (inserted != true || resume != false)
		return 0;

	bool update = false;
	bool installed = false;

	if (job->IsCanceled())
		return GAME_ERROR_CANCELED;

	s_mutex.Lock();
	bool empty = m_titles.size() == 0;
	s_mutex.Unlock();

	LocalFileStat stat;

	if (LocalFile::Getstat(GAME_GC_APP_DIR, &stat) != 0)
		return 0;

	int ret = ListTitles(job, string(GAME_GC_APP_DIR));

	if (ret < 0)
		return ret;

	ret = CheckInstalled(job, empty, &installed, &update);
	if (ret < 0)
		return ret;

	ret = InstallTitles(job, installed, string(GAME_GC_APP_DIR), m_appInfo, update, false);
	if (ret < 0) {
		m_vcInfoLoaded = true;
		return ret;
	}

	if (m_inserted)
		m_resume = true;

	return 0;
}

/*
 * The information of a card just inserted read, when it was not resumed and the promoter knows
 * none of its titles yet: 0, or a negative error (0x81024FDC).
 */
int GameGcPromote::LoadCardInfo(job::JobItem *job)
{
	s_mutex.Lock();
	bool inserted = m_inserted;
	bool resume = m_resume;
	s_mutex.Unlock();

	if (inserted != true || resume != false)
		return 0;

	if (job->IsCanceled())
		return GAME_ERROR_CANCELED;

	s_mutex.Lock();
	bool empty = m_titles.size() == 0;
	s_mutex.Unlock();

	if (!empty)
		return 0;

	int ret = gameGcLoadParamSfo(&m_appInfo);

	if (ret < 0)
		return ret;

	if (job->IsCanceled())
		return GAME_ERROR_CANCELED;

	string version(s_empty_name);

	ret = gameAppCheckSystemVersion(&m_appInfo, &version);
	if (ret < 0)
		return ret;

	if (job->IsCanceled())
		return GAME_ERROR_CANCELED;

	return 0;
}

/* name up, waited for 500 ms at a time for a minute: 0, or a negative error */
static inline int gameGcPromoteWaitPlugin(job::JobItem *job, const char *name)
{
	SceUInt32 tries = 0;

	for (;;) {
		if (job != NULL && job->IsCanceled())
			return GAME_ERROR_CANCELED;
		if (Plugin::Find(name) != NULL)
			return 0;
		thread::Sleep(500);
		if (++tries > 120)
			return GAME_ERROR_TIMEOUT;
	}
}

/*
 * The promote job: once power_manage, topmenu and applauncher are up, the card's information
 * read, its titles installed when it has applications, then promoted, the icons counted before
 * and after (0x81025110).
 */
int gameGcPromoteRunPromotes(GameGcPromoteJob *job, void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;
	int ret;

	if (promote->m_appInfo.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = gameGcPromoteWaitPlugin(job, "power_manage_plugin");
	if (ret < 0)
		return ret;
	ret = gameGcPromoteWaitPlugin(job, "topmenu_plugin");
	if (ret < 0)
		return ret;
	ret = gameGcPromoteWaitPlugin(job, "applauncher_plugin");
	if (ret < 0)
		return ret;

	ret = gameCountIcons(GAME_GC_PROMOTE_ICON_PAGE, 1);
	if (ret >= 0) {
		promote->m_iconCount = ret;
		ret = 0;
	}
	if (ret < 0)
		return ret;

	promote->m_state = 0;

	ret = promote->LoadCardInfo(job);
	if (ret < 0)
		return ret;

	bool app = gameDirHasSubDirs(string(GAME_GC_APP_DIR));
	bool appinst = gameDirHasSubDirs(string(GAME_GC_APPINST_DIR));
	bool acinst = gameDirHasSubDirs(string(GAME_GC_ACINST_DIR));

	if (app) {
		ret = promote->InstallCard(job);
		if (ret < 0)
			return ret;
	} else if (s_gcInserted && !(appinst | acinst)) {
		return GAME_ERROR_GC_NO_APP;
	}

	promote->m_state = 1;

	ret = promote->PromoteTitles(job);
	if (ret < 0)
		return ret;

	promote->m_state = 4;

	ret = gameCountIcons(GAME_GC_PROMOTE_ICON_PAGE, 1);
	if (ret >= 0) {
		promote->m_iconCountAfter = ret;
		ret = gameCountUserIcons();
		if (ret >= 0) {
			promote->m_userIconCount = ret;
			ret = 0;
		}
	}
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The error dialog's button: the icon let go and, on its OK (2), the dialog closed, applauncher's
 * icons refreshed after an error that needs it and the card's state shown again (0x81025388).
 */
static void gameGcPromoteOnErrorDialog(SceInt32 dialogId, SceInt32 button, void *arg1, void *arg2)
{
	GameGcPromote *promote = (GameGcPromote *)arg1;

	promote->m_icon.clear();

	if (button != 2)
		return;

	if (dialogId != -1) {
		gamePowerManageRemoveCallback(gameGcPromoteOnPowerEvent, promote);
		SceCommonGuiDialog_BC311DC0(dialogId);
		promote->m_progressDialogId = -1;
	}

	if ((int)arg2 == GAME_ERROR_TOO_MANY_ICONS) {
		Plugin *applauncher = Plugin::Find("applauncher_plugin");

		if (applauncher != NULL) {
			ShellPluginInterface *iface =
				(ShellPluginInterface *)applauncher->GetInterface(1);

			if (iface != NULL)
				iface->func[9]();
		}
	}

	promote->ChangeGcStatus();
}

/* An error dialog over applauncher with text, func told of its button: 0, or a negative error
   (0x810253FC). */
static __attribute__((noinline)) int gameGcPromoteErrorOpen(int *dialogId,
		GameDialogFuncHandler::Func func, void *arg1, void *arg2, const wstring &text)
{
	Plugin *applauncher = Plugin::Find("applauncher_plugin");

	if (applauncher == NULL)
		return GAME_ERROR_NOT_READY;

	CommonGuiLayoutDialogParam param;

	param.resourcePlugin = applauncher;
	param.priority = 34;
	param.dimmerColor[0] = 0.0f;
	param.dimmerColor[1] = 0.0f;
	param.dimmerColor[2] = 0.0f;
	param.dimmerColor[3] = 0.3f;
	param.notifyClose = true;
	param.pageOption = 6;
	param.handler = new GameDialogFuncHandler(func, arg1, arg2);
	param.text = text;

	int ret = SceCommonGuiDialog_CCFCD9A6(&param);

	if (ret < 0)
		return ret;

	*dialogId = ret;

	return 0;
}

/* A failed promote's text, its error code after it (0x81025590). */
void GameGcPromote::GetErrorText(wstring *text, int error)
{
	Plugin *applauncher = Plugin::Find("applauncher_plugin");

	*text = applauncher->GetString(IDParam(s_empty_name, GAME_GC_PROMOTE_MSG_ERROR));
	*text += L"\n(";

	char code[16];

	sceErrorGetExternalString(code, error);

	string codeText(code);
	wstring wcode;

	common::Utf8ToUtf16(codeText, &wcode, 0);
	text->append(wcode);
	*text += L")";
}

/*
 * A failed promote shown: the progress dialog closed, the error's text (the icon limit with the
 * number, the plugins' own text, else the failed promote's with the code), a conflict with the
 * title that holds it, in a notice over the title, or a plain error dialog; the power callback
 * kept while it shows, else the close handled at once (0x810256F2).
 */
void GameGcPromote::ShowError(int *dialogId, int error, bool frame)
{
	if (*dialogId != -1) {
		gamePowerManageRemoveCallback(gameGcPromoteOnPowerEvent, this);
		SceCommonGuiDialog_BC311DC0(*dialogId);
		*dialogId = -1;
	}

	Plugin *topmenu = Plugin::Find("topmenu_plugin");
	Plugin *applauncher = Plugin::Find("applauncher_plugin");

	if (topmenu == NULL || applauncher == NULL) {
		gameGcPromoteOnErrorDialog(*dialogId, 0, this, NULL);
		return;
	}

	wstring message;
	SceUInt32 messageId = gameErrorGetMessageId(error);

	if (messageId == GAME_GC_PROMOTE_MSG_ERROR) {
		GetErrorText(&message, error);
	} else if (messageId == GAME_GC_PROMOTE_MSG_ICON_LIMIT) {
		wchar_t count[128];

		sce_paf_swprintf(count, 128, L"%d", GAME_ICONS_MAX);
		message = shellFormatText(applauncher->GetString(IDParam(s_empty_name, messageId)),
					  count);
	} else {
		message = applauncher->GetString(IDParam(s_empty_name, messageId));
		if (message.length() == 0) {
			message = topmenu->GetString(IDParam(s_empty_name, messageId));
			if (message.length() == 0)
				GetErrorText(&message, error);
		}
	}

	bool hasIcon2 = false;
	wstring title;

	common::Utf8ToUtf16(m_title, &title, 0);

	wstring text2(L"");
	intrusive_ptr<graph::Surface> icon2;

	if (error == GAME_ERROR_GC_CONFLICT) {
		title = applauncher->GetString(IDParam(s_empty_name, GAME_GC_PROMOTE_MSG_CONFLICT));
		common::Utf8ToUtf16(s_conflictTitleName, &text2, 0);

		string iconPath = string("ur0:appmeta/") + s_conflictTitleId + string("/icon0.dds");

		LoadIcon(iconPath, false, &icon2);
		hasIcon2 = true;
		s_conflictTitleName = s_empty_name;
		s_conflictTitleId = s_empty_name;
	}

	int ret;

	if (error == GAME_ERROR_GC_PLAIN)
		ret = gameGcPromoteErrorOpen(dialogId, gameGcPromoteOnErrorDialog, this,
					     (void *)error, message);
	else
		ret = gameGcPromoteNoticeOpen(dialogId, gameGcPromoteOnErrorDialog, this,
			(void *)error, m_icon, title,
			wstring(applauncher->GetString(IDParam(s_empty_name,
				GAME_GC_PROMOTE_MSG_NOTICE))),
			message, frame, icon2.get() != NULL ? &icon2 : NULL, hasIcon2, text2);

	if (ret >= 0)
		gamePowerManageAddCallback(gameGcPromoteOnPowerEvent, this);
	else
		gameGcPromoteOnErrorDialog(*dialogId, 0, this, (void *)ret);
}

/*
 * The system update check loaded for topmenu, with the promoter told of its result; unloaded
 * when the power state changes meanwhile: 0, or a negative error (0x81025E34).
 */
static __attribute__((noinline)) int gameGcPromoteCheckUpdate(GameGcPromote *promote)
{
	Plugin *topmenu = Plugin::Find("topmenu_plugin");

	if (topmenu == NULL)
		return GAME_ERROR_NOT_READY;

	if (topmenu->GetInterface(1) == NULL)
		return GAME_ERROR_NOT_READY;

	ShellSysupchkParam param;

	param.caller = "topmenu_plugin";
	param.callback = gameGcPromoteOnSysupchk;
	param.arg = promote;
	param.checkMode = 1;
	param.dialogMode = 3;
	param.launchMode = 1;
	param.acceptedFlag = 0;
	shellSysupchkInterface = &shellSysupchkDefaultInterface;

	CommonGuiLayoutDialogParam dialog;

	dialog.priority = -1;
	param.msgDlgParam = &dialog;

	ShellSysupchkInitParam init;

	init.SetParam(param);
	init.Load(NULL, 0);
	gamePowerManageAddCallback(gameGcPromoteOnSysupchkPower, promote);

	return 0;
}

/*
 * The game card installer loaded, to install the card's application (gro0:appinst) or its
 * additional content (gro0:acinst), unless the card's application is installed with bit 11 of
 * its #appconf set; topmenu kept busy and locked until it unloads (0x81025FE4).
 */
void GameGcPromote::LoadInstaller()
{
	gameTopmenuBusyBegin();
	m_busy++;
	TopmenuLock();

	Plugin::InitParam param;
	bool appinst = gameDirHasSubDirs(string(GAME_GC_APPINST_DIR));
	bool acinst = gameDirHasSubDirs(string(GAME_GC_ACINST_DIR));

	if (appinst | acinst) {
		if (gameDirHasSubDirs(string(GAME_GC_APP_DIR))) {
			string titleId = gameCardGetTitleId();

			if (titleId.length() == 0)
				goto fail;

			LsdbAppInfoPtr info(new LsdbAppInfo);

			if (info.get() == NULL)
				goto fail;

			if (gameAppGetInfo(titleId.c_str(), &info) < 0)
				goto fail;

			LsdbAppInfo::iterator it = info->find(LSDB_KEY_APPCONF);

			if (it != info->end() && (gameLsdbValueInteger(it->second) & 0x800) != 0)
				goto fail;
		}

		param.name = "gamecard_installer_plugin";
		param.resource_file = "vs0:vsh/game/gamecard_installer_plugin.rco";
		param.caller_name = "__main__";
		param.module_file = "vs0:vsh/game/gamecard_installer_plugin.suprx";
		param.module_interface_version = 1;
		param.module_option = 1;
		param.unload_callback = gameGcPromoteOnInstallerUnload;
		param.param = this;
		s_installerAc = acinst != 0;
		param.arguments[string("execute_mode")] = Plugin::Argument(s_installerAc);

		Plugin::LoadAsync(param, gameGcPromoteOnInstallerLoad, 0);
		return;
	}

fail:
	if (m_topmenuLock != 0)
		TopmenuUnlock();

	if (m_busy > 0) {
		gameTopmenuBusyEnd();
		m_busy--;
	}
}

/*
 * A promote's job ended: topmenu and the system let go, applauncher's icons tidied when the
 * promote added some, the installer loaded for a card just inserted; the application's or the
 * system's update checked for; the installer stopped at a title shown in a notice; else the
 * error shown, the card's drives unmounted when its information was loaded (0x8102661E).
 */
void gameGcPromoteFinish(int result, void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;

	promote->m_job.reset();

	int iconCount = promote->m_iconCount;
	int iconCountAfter = promote->m_iconCountAfter;
	int userIconCount = promote->m_userIconCount;

	promote->m_iconCount = 0;
	promote->m_iconCountAfter = 0;
	promote->m_userIconCount = 0;

	if (promote->m_topmenuLock != 0)
		promote->TopmenuUnlock();
	if (promote->m_powerLock != 0)
		promote->PowerUnlock();
	if (promote->m_busy > 0) {
		gameTopmenuBusyEnd();
		promote->m_busy--;
	}

	int state = promote->m_state;

	promote->m_state = 4;

	Plugin *applauncher = Plugin::Find("applauncher_plugin");
	bool inserted = s_gcInserted;

	s_gcInserted = false;

	if (result == 0) {
		if (iconCountAfter > 0 &&
		    (iconCountAfter != iconCount || userIconCount < GAME_ICONS_MAX) &&
		    applauncher != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)applauncher->GetInterface(1);

			if (iface != NULL)
				iface->func[9]();
		}
		if (inserted)
			promote->LoadInstaller();
	} else if (result == GAME_ERROR_GC_QUIET) {
	} else if (result == GAME_ERROR_GC_APP_UPDATE) {
		if (state == 0 && s_gcIndicator) {
			Plugin *indicator = Plugin::Find("indicator_plugin");

			if (indicator != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)indicator->GetInterface(1);

				if (iface != NULL)
					((void (*)(int))iface->func[25])(0);
			}
		}

		promote->m_sysupchkLoaded = true;

		Plugin *topmenu = Plugin::Find("topmenu_plugin");

		if (topmenu != NULL && topmenu->GetInterface(1) != NULL &&
		    gameAppCheckUpdate(&promote->m_appInfo, gameGcPromoteOnSysupchk, promote, true, 0,
				       -1) >= 0) {
			gamePowerManageAddCallback(gameGcPromoteOnSysupchkPower, promote);
			return;
		}

		if (promote->m_vcInfoLoaded) {
			promote->m_vcInfoLoaded = false;
			promote->m_inserted = false;
			gameUnmountGameDrives();
		}
	} else if (result == GAME_ERROR_SYSTEM_UPDATE) {
		promote->m_sysupchkLoaded = true;
		gameGcPromoteCheckUpdate(promote);
	} else if (result == GAME_ERROR_INSTALL_TITLE) {
		CommonGuiLayoutDialogParam param;

		param.resourcePlugin = applauncher;
		param.layout.page = &s_noticePages[3];
		param.layout.parts = s_noticeParts4;
		param.layout.buttons = SceCommonGuiDialog_6CD802FC.buttons;
		param.priority = 40;
		param.dimmerColor[0] = 0.0f;
		param.dimmerColor[1] = 0.0f;
		param.dimmerColor[2] = 0.0f;
		param.dimmerColor[3] = 0.3f;
		param.pageOption = 6;
		param.handler = new GameDialogFuncHandler(gameGcPromoteOnInstallDialog, promote,
							  (void *)GAME_ERROR_INSTALL_TITLE);
		param.text = applauncher->GetString(IDParam(s_empty_name,
							    GAME_GC_PROMOTE_MSG_INSTALL_TITLE));

		int ret = SceCommonGuiDialog_CCFCD9A6(&param);

		if (ret < 0) {
			promote->m_progressDialogId = -1;
			return;
		}

		promote->m_progressDialogId = ret;
		gamePowerManageAddCallback(gameGcPromoteOnPowerEvent, promote);

		wstring title;

		common::Utf8ToUtf16(promote->m_title, &title, 0);
		gameGcPromoteNoticeSet(promote->m_progressDialogId, promote->m_icon, title,
				       wstring(L""), false, true, NULL, false, wstring(L""));
		promote->m_stopped = true;
	} else if (result == GAME_ERROR_ABORTED || result == GAME_ERROR_TIMEOUT ||
		   result == GAME_ERROR_GC_STOPPED || result == GAME_ERROR_CANCELED) {
		if (promote->m_vcInfoLoaded) {
			promote->m_vcInfoLoaded = false;
			promote->m_inserted = false;
			gameUnmountGameDrives();
		}
	} else {
		promote->m_state = state;

		if (promote->m_vcInfoLoaded) {
			promote->m_vcInfoLoaded = false;
			promote->m_inserted = false;
			gameUnmountGameDrives();
		}

		bool frame = true;

		if (state == 0 || state == 2) {
			if (s_gcIndicator) {
				Plugin *indicator = Plugin::Find("indicator_plugin");

				if (indicator != NULL) {
					ShellPluginInterface *iface =
						(ShellPluginInterface *)indicator->GetInterface(1);

					if (iface != NULL)
						((void (*)(int))iface->func[25])(0);
				}
			}
		} else if (state == 1) {
			frame = false;
		} else if (state == 3 && result == GAME_NPDRM_ERROR_EXPIRED) {
			result = GAME_ERROR_GC_LICENSE_EXPIRED;
		}

		promote->ShowError(&promote->m_progressDialogId, result, frame);
	}
}

/*
 * The promote of a card's titles: the icons counted, the titles promoted, the card's information
 * loaded and its application installed, and the icons counted again (0x81026C3C).
 */
int gameGcPromoteRunStart(GameGcPromoteJob *job, void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;
	int ret;

	ret = gameCountIcons(GAME_GC_PROMOTE_ICON_PAGE, 1);
	if (ret >= 0) {
		promote->m_iconCount = ret;
		ret = 0;
	}
	if (ret < 0)
		return ret;

	promote->m_state = 1;

	ret = promote->PromoteTitles(job);
	if (ret < 0)
		return ret;

	promote->m_state = 0;

	ret = promote->LoadCardInfo(job);
	if (ret < 0)
		return ret;

	bool app = gameDirHasSubDirs(string(GAME_GC_APP_DIR));
	bool appinst = gameDirHasSubDirs(string(GAME_GC_APPINST_DIR));
	bool acinst = gameDirHasSubDirs(string(GAME_GC_ACINST_DIR));

	if (app) {
		ret = promote->InstallCard(job);
		if (ret < 0)
			return ret;
	} else if (s_gcInserted && !(appinst | acinst)) {
		return GAME_ERROR_GC_NO_APP;
	}

	promote->m_state = 4;

	ret = gameCountIcons(GAME_GC_PROMOTE_ICON_PAGE, 1);
	if (ret >= 0) {
		promote->m_iconCountAfter = ret;
		ret = gameCountUserIcons();
		if (ret >= 0) {
			promote->m_userIconCount = ret;
			ret = 0;
		}
	}
	if (ret < 0)
		return ret;

	return 0;
}

/* Whether a promote is to stop: its job canceled, or the plugin stopping (0x81026DD0). */
static bool gameGcPromoteIsCanceled(void *arg)
{
	job::JobItem *job = (job::JobItem *)arg;

	bool canceled = job->IsCanceled();

	if (!canceled)
		return gameIsStopRequested();

	return canceled;
}

/*
 * An application's short title, icon and parental level, read from its directory mounted: 0, or
 * a negative error (0x81026DE4).
 */
int GameGcPromote::LoadTitleInfo(const string &appPath, string *stitle,
				 intrusive_ptr<graph::Surface> *icon, int *parentalLevel)
{
	char mountPoint[20];
	int ret = gameAppDirMount(appPath.c_str(), mountPoint, 17);

	if (ret < 0)
		return ret;

	string sfoPath = string(mountPoint) + string("/sce_sys/param.sfo");
	string iconPath = string(mountPoint) + string("/sce_sys/icon0.png");
	const GameSfoParam *params = gameSfoGetTitleParams();
	unsigned int count = gameSfoGetTitleParamCount();
	LsdbAppInfoPtr info;

	info = LsdbAppInfoPtr(new LsdbAppInfo);

	if (info.get() != NULL) {
		ret = gameSfoLoad(sfoPath, params, count, &info);
		if (ret >= 0) {
			gameAppInfoGetStitle(&info, stitle);
			*parentalLevel = gameAppInfoGetParentalLevel(&info);
			icon->clear();
			ret = LoadIcon(iconPath, true, icon);
			if (ret >= 0)
				ret = 0;
		}
	} else {
		ret = GAME_ERROR_NO_INFO_MEMORY;
	}

	gameAppDirUnmount(mountPoint);

	return ret;
}

/*
 * The install-title notice's button: the notice closed; on its migrate button the title
 * migrated on the job queue (Migration), else the installer loaded again for what the card has
 * left to install, or the card's drives unmounted (0x81027270).
 */
static void gameGcPromoteOnInstallDialog(SceInt32 dialogId, SceInt32 button, void *arg1, void *arg2)
{
	GameGcPromote *promote = (GameGcPromote *)arg1;

	if (dialogId != -1) {
		gamePowerManageRemoveCallback(gameGcPromoteOnPowerEvent, promote);
		SceCommonGuiDialog_BC311DC0(dialogId);
		promote->m_progressDialogId = -1;
		promote->m_stopped = false;
	}

	if (button != GAME_GC_PROMOTE_BUTTON_MIGRATE) {
		if (s_gcIndicator) {
			Plugin *indicator = Plugin::Find("indicator_plugin");

			if (indicator != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)indicator->GetInterface(1);

				if (iface != NULL)
					((void (*)(int))iface->func[25])(0);
			}
		}

		bool appinst = gameDirHasSubDirs(string(GAME_GC_APPINST_DIR));
		bool acinst = gameDirHasSubDirs(string(GAME_GC_ACINST_DIR));

		if (appinst | acinst)
			promote->LoadInstaller();
		else
			gameUnmountGameDrives();
		return;
	}

	s_gcInserted = true;

	promote->m_job = common::SharedPtr<job::JobItem>(new GameGcPromoteJob("Migration", promote,
			gameGcPromoteRunMigration, gameGcPromoteFinish));
	if (promote->m_job.get() == NULL) {
		promote->ShowError(&promote->m_progressDialogId, GAME_ERROR_GC_MIGRATION, true);
		return;
	}

	if (promote->m_jobQueue->Enqueue(promote->m_job) < 0)
		promote->ShowError(&promote->m_progressDialogId, GAME_ERROR_GC_MIGRATION, true);
}

/*
 * The migration of the title the installer stopped at: installed from the card's gro0:app in a
 * progress dialog, its new state kept and the card marked resumed, and the icons counted again:
 * 0, or a negative error (0x81027516).
 */
int gameGcPromoteRunMigration(GameGcPromoteJob *job, void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;
	string path;
	GamePromoteParam param;
	Plugin *applauncher = Plugin::Find("applauncher_plugin");

	promote->m_progress = 0;

	thread::RMutex::main_thread_mutex.Lock();

	int ret = gameGcPromoteDialogOpen(promote->m_title, promote->m_icon,
		wstring(applauncher->GetString(IDParam(s_empty_name, GAME_GC_PROMOTE_MSG_PROMOTING2))),
		true, &promote->m_progress, false);

	if (ret < 0) {
		thread::RMutex::main_thread_mutex.Unlock();
		return ret;
	}

	promote->m_dialogId = ret;
	thread::RMutex::main_thread_mutex.Unlock();

	ret = gameCountIcons(GAME_GC_PROMOTE_ICON_PAGE, 1);
	if (ret >= 0) {
		promote->m_iconCount = ret;
		ret = 0;
	}

	if (ret >= 0) {
		promote->m_state = 2;

		GameGcPromote::s_mutex.Lock();
		path = string("gro0:app/") + promote->m_contentId;
		GameGcPromote::s_mutex.Unlock();

		sce_paf_memset(&param, 0, sizeof(param));
		sce_paf_strncpy(param.path, path.c_str(), sizeof(param.path) - 1);
		param.path[sizeof(param.path) - 1] = '\0';
		param.appInfo = promote->m_appInfo;
		param.option = true;
		param.progressFunc = gameGcPromoteOnProgress;
		param.progressArg = promote;

		ret = gamePluginPromoteParam(&param, gameGcPromoteIsCanceled, job);
		if (ret >= 0) {
			GameGcPromote::s_mutex.Lock();
			promote->m_titles[promote->m_contentId] = ret;
			promote->m_contentId.clear();
			promote->m_resume = true;
			GameGcPromote::s_mutex.Unlock();

			promote->m_state = 4;

			ret = gameCountIcons(GAME_GC_PROMOTE_ICON_PAGE, 1);
			if (ret >= 0) {
				promote->m_iconCountAfter = ret;
				ret = gameCountUserIcons();
				if (ret >= 0) {
					promote->m_userIconCount = ret;
					ret = 0;
				}
			}
			if (ret >= 0) {
				thread::Sleep(1000);
				ret = 0;
			}
		} else {
			GameGcPromote::s_mutex.Lock();
			promote->m_contentId.clear();
			GameGcPromote::s_mutex.Unlock();
		}
	}

	if (promote->m_dialogId != -1) {
		thread::RMutex::main_thread_mutex.Lock();
		if (promote->m_dialogId != -1)
			SceCommonGuiDialog_BC311DC0(promote->m_dialogId);
		promote->m_dialogId = -1;
		thread::RMutex::main_thread_mutex.Unlock();
	}

	return ret;
}

/* The power state changed under the update check: the check unloaded (0x81027A3A). */
static void gameGcPromoteOnSysupchkPower(void *arg)
{
	gamePowerManageRemoveCallback(gameGcPromoteOnSysupchkPower, arg);
	Plugin::UnloadAsync("sysupchk_plugin", NULL, 0);
}

/*
 * The update check's result: an update to install (0x10, 0x20) marks the card resumed; the
 * check unloaded unless it has not ended (0) (0x81027A5E).
 */
static void gameGcPromoteOnSysupchk(int result, void *arg)
{
	switch (result & 0xF0) {
	case 0x10:
		((GameGcPromote *)arg)->m_resume = true;
		break;
	case 0x20:
		((GameGcPromote *)arg)->m_resume = true;
		break;
	case 0:
		return;
	default:
		break;
	}

	gamePowerManageRemoveCallback(gameGcPromoteOnSysupchkPower, arg);
	Plugin::UnloadAsync("sysupchk_plugin", NULL, 0);
}

/* A promote's progress kept for its dialog (0x81027A9C). */
static void gameGcPromoteOnProgress(void *arg, float progress)
{
	((GameGcPromote *)arg)->m_progress = progress;
}

/*
 * The power state changed while a promote's dialog shows: the dialog closed and, for the
 * install-title notice, the installer loaded again for what the card has left to install, else
 * the card's drives unmounted; the callback dropped and the promote of what is left started when
 * it was promoting (0x81027AA2).
 */
static void gameGcPromoteOnPowerEvent(void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;

	if (promote == NULL)
		return;

	if (promote->m_progressDialogId != -1) {
		SceCommonGuiDialog_BC311DC0(promote->m_progressDialogId);
		promote->m_progressDialogId = -1;

		if (promote->m_stopped) {
			promote->m_stopped = false;

			if (s_gcIndicator) {
				Plugin *indicator = Plugin::Find("indicator_plugin");

				if (indicator != NULL) {
					ShellPluginInterface *iface =
						(ShellPluginInterface *)indicator->GetInterface(1);

					if (iface != NULL)
						((void (*)(int))iface->func[25])(0);
				}
			}

			bool appinst = gameDirHasSubDirs(string(GAME_GC_APPINST_DIR));
			bool acinst = gameDirHasSubDirs(string(GAME_GC_ACINST_DIR));

			if (appinst | acinst) {
				promote->LoadInstaller();
				return;
			}

			gameUnmountGameDrives();
		}
	}

	gamePowerManageRemoveCallback(gameGcPromoteOnPowerEvent, promote);

	if (promote->m_state == 1)
		promote->ChangeGcStatus();
}

/*
 * The card's title shown while a promote runs, its short title, icon and parental level read from
 * the card's application: 0, or a negative error (0x81027BCA).
 */
int gameGcPromoteRunLoadVcInfo(GameGcPromoteJob *job, void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;
	int ret = 0;

	GameGcPromote::s_mutex.Lock();
	promote->m_icon.clear();
	promote->m_title = "-";
	promote->m_parentalLevel = 0;
	promote->m_titleInfoLoaded = false;
	GameGcPromote::s_mutex.Unlock();

	string titleId = gameCardGetTitleId();

	if (titleId.length() != 0) {
		string path = string(GAME_GC_APP_DIR) + string("/") + titleId;

		GameGcPromote::s_mutex.Lock();
		promote->m_titleInfoLoaded = true;
		ret = promote->LoadTitleInfo(path, &promote->m_title, &promote->m_icon,
					     &promote->m_parentalLevel);
		GameGcPromote::s_mutex.Unlock();
	}

	return ret;
}

/*
 * The card's information loaded: the indicator shows the card with its icon and parental level,
 * or a plain card without an icon or when the load failed, its access then shown off
 * (0x81027E68).
 */
void gameGcPromoteFinishLoadVcInfo(int result, void *arg)
{
	GameGcPromote *promote = (GameGcPromote *)arg;

	if (!promote->m_titleInfoLoaded)
		return;

	if (result == 0) {
		if (promote->m_icon.get() != NULL) {
			GameGcPromote::s_mutex.Lock();

			int parentalLevel = promote->m_parentalLevel;
			Plugin *indicator = Plugin::Find("indicator_plugin");

			if (indicator != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)indicator->GetInterface(1);

				if (iface != NULL) {
					GameGcIndicatorInfo info(promote->m_icon, parentalLevel);

					((void (*)(const GameGcIndicatorInfo *))iface->func[23])(&info);
					s_gcIndicator = true;
				}
			}

			GameGcPromote::s_mutex.Unlock();
		} else {
			Plugin *indicator = Plugin::Find("indicator_plugin");

			if (indicator != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)indicator->GetInterface(1);

				if (iface != NULL) {
					GameGcIndicatorInfo info;

					((void (*)(const GameGcIndicatorInfo *))iface->func[23])(&info);
					s_gcIndicator = true;
				}
			}
		}
	} else {
		Plugin *indicator = Plugin::Find("indicator_plugin");

		if (indicator != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)indicator->GetInterface(1);

			if (iface != NULL) {
				GameGcIndicatorInfo info;

				((void (*)(const GameGcIndicatorInfo *))iface->func[23])(&info);
				s_gcIndicator = true;
			}
		}

		if (s_gcIndicator) {
			Plugin *indicator = Plugin::Find("indicator_plugin");

			if (indicator != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)indicator->GetInterface(1);

				if (iface != NULL)
					((void (*)(int))iface->func[25])(0);
			}
		}
	}
}
