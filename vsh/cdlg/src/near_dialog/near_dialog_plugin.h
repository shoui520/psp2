/*
 * SceNearDialogPlugin's own interface between its files: the plugin (near_dialog_plugin.cpp,
 * Sony's file), the dialog it shows (near_dialog.cpp, Sony's copy of the message dialog's), the
 * states it steps through for each mode (near_dialog_state.cpp), the common functions it shares
 * with the system software's applications (near_dialog_util.cpp) and the check-in library that
 * talks to the near background service (near_checkin_lib.cpp). Sony's names here are the
 * plugin's function names it writes into the work memory (OnUpdate, NearDialogCallback,
 * StartCheckinCallback, StartItemDownloadCallback, TermCallback) and the library's job names;
 * the rest, and the other files' names, are ours.
 */
#ifndef NEAR_DIALOG_NEAR_DIALOG_PLUGIN_H
#define NEAR_DIALOG_NEAR_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <near_dialog_util.h>
#include <plugin.h>
#include <std/string>
#include <std/map>
#include <std/vector>
#include <thread/thread.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include <widget/w_scene.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../cdlg_mgr/common_dialog_mgr.h"

/* the plugin's page */
#define NEAR_DIALOG_PAGE_ID		0x9573096E

/* what the plugin is asked to do on the main thread (CommonDialogNearParam::flags) */
#define NEAR_DIALOG_FLAG_INIT			0x001
#define NEAR_DIALOG_FLAG_OPEN			0x002
#define NEAR_DIALOG_FLAG_PROGRESS		0x008
#define NEAR_DIALOG_FLAG_START_CHECKIN		0x010
#define NEAR_DIALOG_FLAG_CHECKIN_STATUS		0x020
#define NEAR_DIALOG_FLAG_CANCEL_CHECKIN		0x040
#define NEAR_DIALOG_FLAG_START_DOWNLOAD		0x080
#define NEAR_DIALOG_FLAG_DOWNLOAD_STATUS	0x100
#define NEAR_DIALOG_FLAG_CANCEL_DOWNLOAD	0x200
#define NEAR_DIALOG_FLAG_TERM			0x400

/* the dialog's close request (CommonDialogNearParam::closeRequest) */
#define NEAR_DIALOG_CLOSE_REQUESTED	1
#define NEAR_DIALOG_CLOSE_DONE		2

/* the check-in library's job (CommonDialogNearParam::jobState) */
#define NEAR_DIALOG_JOB_NONE		0
#define NEAR_DIALOG_JOB_STARTED		1
#define NEAR_DIALOG_JOB_CALLED_BACK	2
#define NEAR_DIALOG_JOB_RUNNING		3

/* a step whose result has not come yet (NearDialogGetStepResult) */
#define NEAR_DIALOG_STEP_PENDING	2

/* the plugin's own errors (names ours): the plugin's errors are 0x80107E80 to 0x80107EFF, which
   the caller gets as an unexpected error */
#define NEAR_DIALOG_PLUGIN_ERROR_FIRST			-2146402688	/* 0x80107E80 */
#define NEAR_DIALOG_PLUGIN_ERROR_LAST			-2146402561	/* 0x80107EFF */
#define NEAR_DIALOG_PLUGIN_ERROR_NO_WORK_MEMORY		-2146402686	/* 0x80107E82 */
#define NEAR_DIALOG_PLUGIN_ERROR_INVALID		-2146402623	/* 0x80107EC1 */
#define NEAR_DIALOG_PLUGIN_ERROR_NO_MEMORY		-2146402622	/* 0x80107EC2 */
#define NEAR_DIALOG_PLUGIN_ERROR_NO_SDK_VERSION		-2146402608	/* 0x80107ED0 */
#define NEAR_DIALOG_PLUGIN_ERROR_NO_ALLOWED_SDK_VERSION	-2146402607	/* 0x80107ED1 */
#define NEAR_DIALOG_PLUGIN_ERROR_SDK_VERSION_TOO_NEW	-2146402606	/* 0x80107ED2 */
#define NEAR_DIALOG_PLUGIN_ERROR_STATE			-2146402605	/* 0x80107ED3 */
#define NEAR_DIALOG_PLUGIN_ERROR_DIALOG_OPEN		-2146402604	/* 0x80107ED4 */

/* the near background service's error for a gift its own download is fetching (name ours: the
   dialog then asks the user to finish the download in the near application) */
#define NEAR_SERVICE_ERROR_GIFT_DOWNLOADING		-2146417382	/* 0x8010451A */

/* what the dialog calls when it has closed: its result, its mode, the button that closed it and
   the user data it was made with */
typedef void (*NearDialogFunction)(SceInt32 result, SceInt32 mode, SceInt32 button, void *userdata);

/* what the check-in library calls when a job has started, and when it has ended, with the user
   data it was given */
typedef void (*NearCheckinLibFunction)(SceInt32 result, void *userdata);

/* the plugin (near_dialog_plugin.cpp), which the dialog opens its page from */
extern paf::Plugin *g_nearDialogPlugin;

/* the plugin's states for each mode (near_dialog_state.cpp): the result of the step the state
   waits for, the state after the one the dialog or a job ended, and the result the dialog ends
   with when the state has one */
SceInt32 NearDialogGetStepResult(const CommonDialogNearParam *work, SceInt32 state);
int NearDialogNextState(CommonDialogNearParam *work, SceInt32 button);
bool NearDialogStateResult(CommonDialogNearParam *work, SceInt32 result, SceInt32 button,
			   SceInt32 *ret);

/* the common functions the plugin shares with the system software's applications
   (near_dialog_util.cpp): the SDK check, the %1..%11 formatter, and a number in a country's
   format */
int NearDialogCheckSdkVersion(SceUInt32 sdkVersion);
paf::wstring NearDialogFormat(const wchar_t *format, const wchar_t *arg1 = L"",
			      const wchar_t *arg2 = L"", const wchar_t *arg3 = L"",
			      const wchar_t *arg4 = L"", const wchar_t *arg5 = L"",
			      const wchar_t *arg6 = L"", const wchar_t *arg7 = L"",
			      const wchar_t *arg8 = L"", const wchar_t *arg9 = L"",
			      const wchar_t *arg10 = L"", const wchar_t *arg11 = L"");
paf::string NearDialogFormatNumberString(paf::string number, SceInt32 language,
					 paf::string country);
paf::string NearDialogFormatNumber(SceInt32 value, SceInt32 language, paf::string country);

/* a dialog's text, layout and the sound it opens with */
struct NearDialogContent {
	paf::wstring text;
	CommonGuiDialogLayout layout;
	SceInt32 sound;
};

/* the check-in's end screen: a title and lines of a label and its value in the dialog's
   contents, the two columns centred on their widest text once every text has its size
   (near_finish_info_list.cpp) */
class NearFinishInfoList {
public:
	struct Param {
		/* the prefix of its widgets' names */
		const char *name;
		paf::ui::Widget *parent;
		paf::Plugin *plugin;
		/* 0: the list opens its own template in the parent; 1 and 2 open nothing */
		SceInt32 type;
		/* the size of its texts' font */
		float fontSize;
		float width;
	};

	/* a line: its label and value texts by their names' hashes, their widths once they have
	   their sizes, and the box that holds them */
	struct Line {
		SceUInt32 titleId;
		SceUInt32 valueId;
		float titleWidth;
		float valueWidth;
		paf::ui::Widget *title;
		paf::ui::Widget *value;
		SceInt32 titleSized;
		SceInt32 valueSized;
		paf::ui::Widget *box;
	};

	NearFinishInfoList(const Param &param);
	int AddLine(paf::ui::Widget *box, paf::ui::Widget *title, paf::ui::Widget *value);
	int AddItem(const paf::wstring &title, const paf::wstring &value, const char *name);
	int AddTitle(const paf::wstring &title, const char *name, float width);

	paf::Plugin *m_plugin;
	paf::ui::Widget *m_parent;
	/* the box the template opened, which holds the lines */
	paf::ui::Widget *m_box;
	SceInt32 m_type;
	float m_fontSize;
	/* set by the constructor; nothing in the plugin reads it */
	bool m_valid;
	/* emptied by the constructor; nothing in the plugin fills them */
	paf::map<paf::string, paf::ui::Widget *> m_widgets;
	paf::vector<paf::ui::Widget *> m_titles;
	paf::vector<Line> m_lines;
};

/* the check-in's results on the end screen (near_dialog.cpp) */
class NearDialogFinishScreen {
public:
	NearDialogFinishScreen(paf::Plugin *plugin, SceInt32 dialogId, NearFinishInfoList *list,
			       SceInt32 postCount, bool metric, float totalDistance,
			       float todayDistance, paf::string country)
		: m_plugin(plugin), m_dialogId(dialogId), m_list(list), m_postCount(postCount),
		  m_metric(metric), m_totalDistance(totalDistance), m_todayDistance(todayDistance),
		  m_country(country)
	{
	}

	int Build(void);
	SceUInt32 Distance(float metres);

	paf::Plugin *m_plugin;
	SceInt32 m_dialogId;
	NearFinishInfoList *m_list;
	SceInt32 m_postCount;
	bool m_metric;
	/* metres */
	float m_totalDistance;
	float m_todayDistance;
	paf::string m_country;
};

/* the one dialog the plugin has open (near_dialog.cpp) */
class NearDialog {
public:
	NearDialog(SceInt32 mode, NearDialogFunction callback, void *userdata);
	int OpenLayout(const paf::wstring &text, const CommonGuiDialogLayout *layout);
	int OpenErrorCode(SceInt32 errorCode);
	void ProgInc(SceInt32 target, SceUInt32 rate);
	void SetValue(SceInt32 target, SceUInt32 rate);
	void ResetProgress(void);

	static int Create(const NearDialogMsgParam *param, NearDialogFunction callback,
			  void *userdata);
	static int Abort(bool soon);
	static NearDialog *Get(void);

	NearDialogFunction m_callback;
	void *m_userdata;
	SceInt32 m_mode;
	paf::ui::Scene *m_scene;
	/* the common GUI dialog (SceCommonGuiDialog), -1 while none is open */
	SceInt32 m_dialogId;
	/* the dialog has opened (its open handler ran): its progress bar takes values from then */
	SceInt32 m_opened;
	SceUInt32 m_progressValue;
	SceInt32 m_reserved;
	/* the button that closed the dialog, for the callback */
	SceInt32 m_button;
};

/* the check-in library (near_checkin_lib.cpp): the near background service's check-in and gift
   download as jobs, one at a time */
class NearCheckinLib {
public:
	/* the jobs it queues (NearCheckinLibJob::m_type), as the background service's launch
	   parameter names them (the modes of near_auto_checkin's URI) */
	enum Mode {
		MODE_AUTO_CHECKIN = 1,
		MODE_MANUAL_CHECKIN_LIVEAREA = 2,
		MODE_MANUAL_CHECKIN_TOPMENU = 3,
		MODE_MANUAL_CHECKIN_PUBLIC = 4,
		MODE_MANUAL_CHECKIN_FRIEND = 5,
		MODE_MANUAL_POST = 6,
		MODE_MANUAL_POST_EXIT = 7,
		MODE_CHECKIN_EXIT = 8,
		MODE_SDK_CHECKIN = 9,
		MODE_SDK_CHECKIN_RETRY = 10,
		MODE_SDK_ITEM_DL = 11
	};

	/* none, a job queued, terminating */
	enum State {
		STATE_NONE = 0,
		STATE_STARTED = 1,
		STATE_TERMINATING = 2
	};

	/* the library's results (names ours) */
	enum {
		RESULT_STATE = 1,
		RESULT_PARAM = 2,
		RESULT_BUSY = 3,
		RESULT_NO_MEMORY = 4,
		RESULT_NOT_STARTED = 8
	};

	NearCheckinLib();
	~NearCheckinLib();

	int StartCheckin(NearCheckinLibFunction callback, void *userdata);
	int StartItemDownload(const paf::string &giftDiscoveringId, NearCheckinLibFunction callback,
			      void *userdata);
	int StartCheckinRetry(NearCheckinLibFunction callback, void *userdata);
	int GetCheckinStatus(NearCheckinStatus *status);
	int GetItemDownloadStatus(NearItemDownloadStatus *status);
	int Cancel(void);
	int Term(NearCheckinLibFunction callback, void *userdata);
	void Update(void);

	/* what the jobs call: the status they read, and their end */
	static void UpdateCheckinStatus(NearCheckinLib *lib, SceInt32 error,
					const NearCheckinStatus *status);
	static void UpdateItemDownloadStatus(NearCheckinLib *lib, SceInt32 error,
					     const NearItemDownloadStatus *status);
	static void ConnectJobEnd(NearCheckinLib *lib, SceInt32 result);
	static void StatusJobEnd(NearCheckinLib *lib, SceInt32 result);
	static void TermJobEnd(NearCheckinLib *lib, SceInt32 result);

	bool m_initialized;
	/* the job queued last */
	SceInt32 m_jobType;
	SceInt32 m_state;
	paf::job::JobQueue *m_queue;
	paf::thread::Mutex *m_mutex;
	paf::common::SharedPtr<paf::job::JobItem> m_job;
	/* what is called when the job has started or the library has terminated */
	NearCheckinLibFunction m_callback;
	void *m_userdata;
	/* the last status's error, -999 before one has come */
	SceInt32 m_error;
	NearCheckinStatus m_checkinStatus;
	NearItemDownloadStatus m_downloadStatus;

private:
	int Init(void);
	int Fini(void);
	int Start(SceInt32 type, NearCheckinLibFunction callback, void *userdata,
		  paf::string giftDiscoveringId);
	int StartStatusJob(void);
	int StartTermJob(NearCheckinLibFunction callback, void *userdata);
};

#endif
