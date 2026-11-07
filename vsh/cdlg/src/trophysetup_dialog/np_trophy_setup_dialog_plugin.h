/*
 * SceTrophySetupDialogPlugin's own interface between its files. Sony's names where the 0.99x
 * builds' messages give them (StartTrophyOpenJob, StartTrophySetupJob, TrophyOpenJobFinished,
 * InitSetupController, SetTrophySetupResult, CheckTitleForRemoval, RemoveTitle, ExitProcess, the
 * jobs' names); the classes, the other functions and this header's name are ours.
 */
#ifndef TROPHYSETUP_DIALOG_NP_TROPHY_SETUP_DIALOG_PLUGIN_H
#define TROPHYSETUP_DIALOG_NP_TROPHY_SETUP_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <np/np_trophy.h>
#include <plugin.h>
#include <job/job.h>
#include <std/string>
#include <std/memory>
#include <graphics/surface/surface.h>
#include <widget/core/handler.h>
#include <widget/w_scene.h>
#include <common_gui_dialog/common_gui_dialog.h>

/* the plugin's errors: the work memory missing for a call, taken by another call; an internal
   error; no memory */
#define TROPHY_SETUP_PLUGIN_ERROR_NO_WORK_MEMORY	-2146425262	/* 0x80102652 */
#define TROPHY_SETUP_PLUGIN_ERROR_API_NAME		-2146425263	/* 0x80102651 */
#define TROPHY_SETUP_PLUGIN_ERROR_INTERNAL		-2146425216	/* 0x80102680 */
#define TROPHY_SETUP_PLUGIN_ERROR_NO_MEMORY		-2146425215	/* 0x80102681 */

/* the dialog's resources: the plugin, its page */
#define TROPHY_SETUP_PLUGIN_NAME		"trophysetup_dialog_plugin"
#define TROPHY_SETUP_PAGE_ID			0xE104DBE3

/* the trophy title's information the dialog shows when it asks to remove it */
struct TrophySetupTitleInfo {
	SceUInt8 info[12];
	/* nonzero: the dialog does not ask (what the field means is not known) */
	SceUInt32 locked;
	SceUInt8 reserved[88];
	/* two words the dialog asks only when one is set */
	SceUInt32 extra[2] __attribute__((aligned(8)));
	SceNpTrophyGameDetails details;
};

/* what is installed of the title (SceNpTrophy_0AC3CEA8) */
struct TrophySetupTitleState {
	SceUInt32 percentage;
	SceUInt32 reserved[7];
};

/*
 * The trophy work of the dialog: the session, the application's title for the context, its
 * state, and what the dialog shows of an installed title it may remove.
 */
class SetupController {
public:
	SetupController(SceNpTrophyContext context);
	~SetupController();

	int Init();
	void Abort();
	int Open();
	/* info: the title's information just read, which the trophy handle is made for */
	int LoadIcon(const TrophySetupTitleInfo *info);
	int CheckTitleForRemoval();
	bool GetRemovalInfo(paf::wstring *title, paf::intrusive_ptr<paf::graph::Surface> *icon);
	int RemoveTitle();
	int Setup();
	void Close();

	SceInt32 m_session;
	SceNpTrophyContext m_context;
	SceUInt32 m_title;
	SceInt32 m_status;
	TrophySetupTitleInfo *m_titleInfo;
	paf::intrusive_ptr<paf::graph::Surface> m_icon;
};

/* what a job calls on the main thread when it ends: canceled, its result */
typedef void (*TrophyJobFinishedFunction)(bool canceled, SceInt32 result, SetupController *controller);

/* opens the title and reads its state (open_job.cpp) */
class TrophyOpenJob : public paf::job::JobItem {
public:
	TrophyOpenJob(TrophyJobFinishedFunction finished, SetupController *controller)
		: JobItem("SceNpTrophyOpen", NULL), m_finished(finished), m_controller(controller)
	{
	}

	virtual ~TrophyOpenJob();
	virtual void Cancel();
	virtual int Run();
	virtual void Finish(int result);

	TrophyJobFinishedFunction m_finished;
	SetupController *m_controller;
	/* never read or written */
	SceUInt32 reserved;
};

/* removes the installed title if asked, then sets the title up (setup_job.cpp) */
class TrophySetupJob : public paf::job::JobItem {
public:
	TrophySetupJob(TrophyJobFinishedFunction finished, SetupController *controller)
		: JobItem("SceNpTrophySetup", NULL), m_finished(finished), m_controller(controller)
	{
	}

	virtual ~TrophySetupJob();
	virtual void Cancel();
	virtual int Run();
	virtual void Finish(int result);

	TrophyJobFinishedFunction m_finished;
	SetupController *m_controller;
};

int StartTrophyOpenJob(paf::job::JobQueue *queue, TrophyJobFinishedFunction finished,
		       SetupController *controller);
int StartTrophySetupJob(paf::job::JobQueue *queue, TrophyJobFinishedFunction finished,
			SetupController *controller);

/* the dialog's page (page.cpp) */
class TrophySetupPage {
public:
	static TrophySetupPage *GetInstance();

	TrophySetupPage();
	~TrophySetupPage();

	int Open();
	void Close();

	paf::Plugin *m_plugin;
	paf::ui::Scene *m_scene;
	/* never read */
	SceInt32 m_reserved;
};

/* the dialog's states while one of its dialogs is open */
enum {
	TROPHY_SETUP_DIALOG_STATE_NONE = 0,
	TROPHY_SETUP_DIALOG_STATE_PROCESSING = 1,
	/* the processing dialog has shown long enough to be closed */
	TROPHY_SETUP_DIALOG_STATE_PROCESSED = 2
};

/* the one dialog the plugin has open (dialog_controller.cpp) */
class TrophySetupDialogController {
public:
	static TrophySetupDialogController *GetInstance();

	TrophySetupDialogController();
	~TrophySetupDialogController();

	int Close(bool hideDimmer);
	void ShowError(SceInt32 error);
	void ShowRemovalQuestion(const paf::wstring &title, paf::intrusive_ptr<paf::graph::Surface> &icon,
				 CommonGuiDialogFunction func, void *arg);
	void ShowExitNotice();
	/* the processing dialog's ID, or a negative error */
	int ShowProcessing();
	int Hide();
	bool IsProcessing() const;
	void CloseAfterProcessing(bool hideDimmer, void (*func)(void));

	SceInt32 m_dialogId;
	bool m_dimmerShown;
	SceInt32 m_state;
	void (*m_closedFunc)(void);
	bool m_hideDimmer;
};

/* the dialogs (dialogs.cpp): each returns its ID, or a negative error */
int OpenErrorDialog(SceInt32 error, CommonGuiDialogFunction func, void *arg);
int OpenProcessingDialog(paf::ui::HandlerCB func, void *data);
int OpenExitNoticeDialog(void);
int OpenRemovalQuestionDialog(const paf::wstring &title, CommonGuiDialogFunction func, void *arg);
void SetRemovalQuestionContents(paf::Plugin *plugin, paf::ui::Widget *contents, paf::ui::Widget *text,
				const paf::wstring &title, paf::intrusive_ptr<paf::graph::Surface> &icon);
int OpenRemovalQuestionDialogWithIcon(const paf::wstring &title,
				      paf::intrusive_ptr<paf::graph::Surface> &icon,
				      CommonGuiDialogFunction func, void *arg);

#endif
