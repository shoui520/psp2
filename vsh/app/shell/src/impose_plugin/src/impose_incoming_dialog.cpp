/*
 * impose_incoming_dialog.cpp: Sony's SceIncomingDialogMgr (0x81150574-0x811516E4): the incoming
 * dialog requests of the shell's server, each shown as a dialog over the home screen, or as an
 * indicator while an application is in front (the dialog once it is not), with the request's
 * sound played meanwhile; accepted, the application is launched with "incoming_dialog:" and the
 * request's argument. The class's name is Sony's; the file's and the other names are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <framework.h>
#include <std/string>
#include <std/stdlib.h>
#include <std/stdio.h>
#include <versatileparam/idparam.h>
#include <common/shared_ptr.h>
#include <common/char_encoding.h>
#include <cache/cache_image.h>
#include <job/job.h>
#include <thread/thread.h>
#include <widget/core/widget.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <kernel/processmgr.h>
#include "../../../../game/game_plugin/game_plugin.h"
#include "../../../shell.h"
#include "impose.h"

using namespace paf;

/* the shell's events the manager follows (PLACEHOLDER names) */
#define INCOMING_EVENT_START_SCREEN_ON	2
#define INCOMING_EVENT_START_SCREEN_OFF	3
#define INCOMING_EVENT_APP_OFF		6
#define INCOMING_EVENT_APP_ON		7

/* the results a request is given back with (PLACEHOLDER names) */
#define INCOMING_RESULT_ACCEPTED	3
#define INCOMING_RESULT_REJECTED	4
#define INCOMING_RESULT_CANCELED	5
#define INCOMING_RESULT_TIMEOUT		6

/* the dialog's buttons (PLACEHOLDER names): 0 when it closed without one */
#define INCOMING_BUTTON_CLOSED		0
#define INCOMING_BUTTON_ACCEPT		2
#define INCOMING_BUTTON_REJECT		3

/* the dialog's parts: its icon and its buttons (PLACEHOLDER names) */
#define INCOMING_PART_ICON		5
#define INCOMING_PART_BUTTON		12
#define INCOMING_PART_SECOND_BUTTON	13

/* the icon the dialog shows for an application without one (PLACEHOLDER name) */
#define INCOMING_DEFAULT_ICON		0xAFDEF9D0

/* what the jobs return (PLACEHOLDER names) */
#define INCOMING_ERROR_NO_GAME_PLUGIN	((int)0x80106200)
#define INCOMING_ERROR_NO_APP		((int)0x80106202)

/* the plugins' functions the manager calls (PLACEHOLDER names) */
#define INCOMING_TOPMENU_FUNC_BUSY	37
#define INCOMING_TOPMENU_FUNC_GET_FRAME	8
#define INCOMING_INDICATOR_FUNC_SHOW	14
#define INCOMING_INDICATOR_FUNC_HIDE	15
#define INCOMING_POWER_FUNC_NOTIFY	13
#define INCOMING_POWER_FUNC_LAUNCH	17
#define INCOMING_POWER_FUNC_DIALOG	22
#define INCOMING_APPLAUNCHER_FUNC_ICON	3

#define INCOMING_ASSERT(ret) \
	do { \
		int __ret = (ret); \
		if (__ret != 0) \
			sceClibPrintf("[SceIncomingDialogMgr] ASSERTION FAILURE (0x%x)\\n", __ret); \
	} while (0)

/* the user side of _sceFiosKernelOverlayResolveSync */
extern "C" int sceFiosKernelOverlayResolveSync(SceUID pid, int resolveFlag, const char *inPath,
					       char *outPath, SceSize maxPath);

/* the shell's sound player (PLACEHOLDER names: 0x811CC2F8-0x811CCABC, in the shell's own
   code) */
struct ShellSoundCommand {
	SceInt32 type;
	SceInt32 arg[4];
};

struct ShellSoundStatus {
	SceUInt8 reserved[68];
	SceInt32 ready;
};

struct ShellSoundOption {
	SceInt32 loop;
	SceInt32 reserved[3];
};

struct MusicCorePublicStatus;
int musicCoreExOpen(const int *openParam);
int musicCoreExClose(void);
int musicCoreExLoadUri(const char *uri, const void *opt);
int musicCoreExControl(SceUInt8 cmd, int param);
int musicCoreExGetStatus(MusicCorePublicStatus *outStatus);
int musicCoreExSetRepeat(SceUInt8 mode);

int parentalGetRemainingSec(void);

int shellStateWatcherOpenDialog(CommonGuiLayoutDialogParam *param, bool playSound, int level);
int shellStateWatcherCloseDialog(int dialogId);

/* what indicator_plugin shows (PLACEHOLDER names) */
struct IncomingIndicatorParam {
	IncomingIndicatorParam(const char *id, const char *titleId)
		: id(id), titleId(titleId), iconPath(""), text(""), timeout(0), alpha(1.0f), type(0)
	{
	}

	/* declared, so SNC inlines it as in Sony's build */
	~IncomingIndicatorParam()
	{
	}

	string id;
	string titleId;
	string iconPath;
	string text;
	SceInt32 timeout;
	float alpha;
	SceInt32 type;
};

/* the request's notice written as a new event (PLACEHOLDER name) */
class IncomingDialogMgrNoticeJob : public job::JobItem {
public:
	IncomingDialogMgrNoticeJob(const char *titleId, const wchar_t *text, const char *execArg)
		: job::JobItem("IncomingDialogMgr", NULL)
	{
		sce_paf_strlcpy(m_titleId, titleId, sizeof(m_titleId));
		sce_paf_wcsncpy(m_text, text, 64);
		m_text[63] = 0;
		m_execArg = execArg;
	}

	virtual ~IncomingDialogMgrNoticeJob()
	{
	}

	virtual int Run()
	{
		LsdbNewEvent event;

		event.titleId = m_titleId;
		event.itemId = "g00";
		{
			wstring text(m_text);
			string desc;

			common::Utf16ToUtf8(text, &desc);
			event.desc = desc;
		}
		event.msgType = 258;
		event.actType = 2;
		event.newFlag = true;
		event.popupNo = true;
		event.execMode = 0x20000;
		event.execTitleId = event.titleId;
		event.execArg = m_execArg;
		SceLsdb_315B9FD6(&event, true);
		return 0;
	}

	virtual void Finish(int result)
	{
	}

	char m_titleId[10];
	wchar_t m_text[64];
	string m_execArg;
};

/* the application's folder mounted and its sound played (PLACEHOLDER name) */
class IncomingDialogMgrPlayJob : public job::JobItem {
public:
	IncomingDialogMgrPlayJob(SceUID processId, const char *titleId, const char *path,
				 char *mountPoint)
		: job::JobItem("IncomingDialogMgr", NULL), m_processId(processId),
		  m_mountPoint(mountPoint)
	{
		sce_paf_strlcpy(m_titleId, titleId, sizeof(m_titleId));
		sce_paf_strlcpy(m_path, path, 256);
	}

	virtual ~IncomingDialogMgrPlayJob()
	{
	}

	/* the application's folder mounted, from its _org_path */
	int Mount()
	{
		LsdbAppInfoPtr info(new LsdbAppInfo());
		AppInfoQClient client;

		SceLsdb_A6D8772C(&client);
		if (info.get() == NULL || SceLsdb_E90B81CD(&client) != 0) {
			SceLsdb_E2101B03(&client);
			return INCOMING_ERROR_NO_APP;
		}
		if (SceLsdb_D239031A(&client, &string(m_titleId), &info) != 0) {
			SceLsdb_E2101B03(&client);
			return INCOMING_ERROR_NO_APP;
		}

		LsdbAppInfo *app = info.get();
		LsdbAppInfo::iterator it = app->find(LsdbKeyHasher<9>::Hash("_org_path"));

		if (it == app->end()) {
			SceLsdb_E2101B03(&client);
			return INCOMING_ERROR_NO_APP;
		}

		Plugin *plugin = Plugin::Find("game_plugin");

		if (plugin == NULL) {
			SceLsdb_E2101B03(&client);
			return INCOMING_ERROR_NO_GAME_PLUGIN;
		}

		GamePluginInterface *game = (GamePluginInterface *)plugin->GetInterface(1);

		if (game == NULL) {
			SceLsdb_E2101B03(&client);
			return INCOMING_ERROR_NO_GAME_PLUGIN;
		}

		int ret = ((int (*)(const char *, char *, SceSize))
			game->func[GAME_PLUGIN_FUNC_APP_DIR_MOUNT])(LsdbValueText(it->second),
								    m_mountPoint, 64);

		SceLsdb_E2101B03(&client);
		return ret;
	}

	virtual int Run()
	{
		int ret = Mount();

		if (ret != 0)
			return ret;

		ShellSoundCommand command;
		ShellSoundOption option;
		char path[260];
		char resolved[1025];

		sce_paf_memset(&command, 0, sizeof(command));
		command.type = 5;
		command.arg[0] = 0;
		command.arg[1] = 0;
		command.arg[2] = 0;
		command.arg[3] = 0;
		INCOMING_ASSERT(musicCoreExOpen((const int *)&command));
		sce_paf_memset(resolved, 0, sizeof(resolved));
		sce_paf_memset(&option, 0, sizeof(option));
		if (sce_paf_strncmp(m_path, "app0:", 5) == 0)
			sceFiosKernelOverlayResolveSync(m_processId, 0, m_path, path, 256);
		else
			sce_paf_strlcpy(path, m_path, 255);
		sce_paf_strncpy(resolved, path, sizeof(resolved));
		option.loop = -1;
		INCOMING_ASSERT(musicCoreExLoadUri(resolved, &option));
		INCOMING_ASSERT(musicCoreExSetRepeat(0));
		INCOMING_ASSERT(musicCoreExControl(0, 0));
		return 0;
	}

	virtual void Finish(int result)
	{
	}

	SceUID m_processId;
	char m_titleId[10];
	char m_path[256];
	char *m_mountPoint;
	SceUInt32 reserved;
};

/* the sound stopped and the application's folder unmounted (PLACEHOLDER name) */
class IncomingDialogMgrStopJob : public job::JobItem {
public:
	IncomingDialogMgrStopJob(const char *mountPoint) : job::JobItem("IncomingDialogMgr", NULL)
	{
		sce_paf_strlcpy(m_mountPoint, mountPoint, sizeof(m_mountPoint));
	}

	virtual ~IncomingDialogMgrStopJob()
	{
	}

	virtual int Run()
	{
		ShellSoundStatus status;

		INCOMING_ASSERT(musicCoreExControl(2, 0));
		for (;;) {
			musicCoreExGetStatus((MusicCorePublicStatus *)&status);
			if (status.ready != 0)
				break;
			thread::Sleep(300);
		}
		INCOMING_ASSERT(musicCoreExClose());

		Plugin *plugin = Plugin::Find("game_plugin");
		int ret;

		if (plugin == NULL) {
			ret = INCOMING_ERROR_NO_GAME_PLUGIN;
		} else {
			GamePluginInterface *game = (GamePluginInterface *)plugin->GetInterface(1);

			if (game == NULL)
				ret = INCOMING_ERROR_NO_GAME_PLUGIN;
			else
				ret = ((int (*)(const char *))
					game->func[GAME_PLUGIN_FUNC_APP_DIR_UNMOUNT])(m_mountPoint);
		}
		INCOMING_ASSERT(ret);
		return 0;
	}

	virtual void Finish(int result)
	{
	}

	char m_mountPoint[64];
};

/* the dialog's buttons, passed on to the manager (PLACEHOLDER name) */
class IncomingDialogHandler : public CommonGuiDialogHandler {
public:
	IncomingDialogHandler(IncomingDialogMgr *mgr) : m_mgr(mgr)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button);

	IncomingDialogMgr *m_mgr;
};

static void incomingDialogMgrOnEvent(int code, const void *data, SceSize size, void *arg);

/* (0x81150574) */
IncomingDialogMgr::IncomingDialogMgr(Plugin *plugin)
	: m_plugin(plugin), m_topmenu(NULL), m_state(STATE_STOPPED), m_request(NULL), m_dialog(-1),
	  m_inApp(false), m_onStartScreen(false), m_indicatorShown(false), m_soundPlaying(false),
	  m_launchFlag(false), m_launchPending(false)
{
	SceShell_7FE4E732(INCOMING_EVENT_APP_OFF, incomingDialogMgrOnEvent, this);
	SceShell_7FE4E732(INCOMING_EVENT_APP_ON, incomingDialogMgrOnEvent, this);
	SceShell_7FE4E732(INCOMING_EVENT_START_SCREEN_ON, incomingDialogMgrOnEvent, this);
	SceShell_7FE4E732(INCOMING_EVENT_START_SCREEN_OFF, incomingDialogMgrOnEvent, this);
}

/* (0x811505DC): the start screen's listeners are added again, as in Sony's code */
IncomingDialogMgr::~IncomingDialogMgr()
{
	SceShell_D4C7D2B7(INCOMING_EVENT_APP_OFF, incomingDialogMgrOnEvent, this);
	SceShell_D4C7D2B7(INCOMING_EVENT_APP_ON, incomingDialogMgrOnEvent, this);
	SceShell_7FE4E732(INCOMING_EVENT_START_SCREEN_ON, incomingDialogMgrOnEvent, this);
	SceShell_7FE4E732(INCOMING_EVENT_START_SCREEN_OFF, incomingDialogMgrOnEvent, this);
}

/* (0x81150630) */
void IncomingDialogMgr::Start()
{
	m_state = STATE_IDLE;
}

/* (0x81150638) */
void IncomingDialogMgr::StopSound()
{
	if (m_soundPlaying) {
		IncomingDialogMgrStopJob *stop = new IncomingDialogMgrStopJob(m_mountPoint);
		common::SharedPtr<job::JobItem> job(stop);

		if (stop != NULL) {
			job::JobQueue::default_queue->Enqueue(job);
			m_soundPlaying = false;
		}
	}
}

/* (0x8115073A) */
void IncomingDialogMgr::PostNoticeEvent()
{
	if (m_request->noticeText[0] != 0) {
		IncomingDialogMgrNoticeJob *notice = new IncomingDialogMgrNoticeJob(
			m_request->titleId, m_request->noticeText, m_request->param);
		common::SharedPtr<job::JobItem> job(notice);

		if (notice != NULL)
			job::JobQueue::default_queue->Enqueue(job);
	}
}

/* the indicator taken off */
inline void IncomingDialogMgr::HideIndicator()
{
	if (m_indicatorShown) {
		Plugin *plugin = Plugin::Find("indicator_plugin");

		if (plugin != NULL) {
			ShellPluginInterface *indicator =
				(ShellPluginInterface *)plugin->GetInterface(1);

			if (indicator != NULL) {
				((void (*)(const char *))indicator->func[INCOMING_INDICATOR_FUNC_HIDE])(
					m_request->titleId);
				m_indicatorShown = false;
			}
		}
	}
}

/* the dialog closed */
inline void IncomingDialogMgr::CloseDialog()
{
	if (m_dialog != -1) {
		shellStateWatcherCloseDialog(m_dialog);
		m_dialog = -1;

		Plugin *plugin = Plugin::Find("power_manage_plugin");

		if (plugin != NULL) {
			ShellPluginInterface *power = (ShellPluginInterface *)plugin->GetInterface(1);

			if (power != NULL)
				((void (*)(bool))power->func[INCOMING_POWER_FUNC_DIALOG])(false);
		}
	}
}

/* (0x81150882) */
int IncomingDialogMgr::Close(int result)
{
	PostNoticeEvent();
	StopSound();
	HideIndicator();
	CloseDialog();
	shellIncomingDialogServerReleaseRequest(m_request, result);
	m_request = NULL;
	return 0;
}

/* (0x811508FE) */
int IncomingDialogMgr::Stop()
{
	if (m_state > STATE_IDLE && m_state <= STATE_INDICATOR)
		Close(INCOMING_RESULT_CANCELED);
	m_state = STATE_STOPPED;
	return 0;
}

static void incomingDialogMgrOnIcon(const intrusive_ptr<cache::CacheImageInterface> &icon,
				    int dialog);

/* (0x8115091C) */
int IncomingDialogMgr::OpenDialog()
{
	Plugin *plugin = Plugin::Find("applauncher_plugin");

	if (plugin == NULL)
		return -1;

	ShellPluginInterface *applauncher = (ShellPluginInterface *)plugin->GetInterface(1);

	if (applauncher == NULL)
		return -1;

	intrusive_ptr<cache::CacheImageInterface> icon;

	((bool (*)(intrusive_ptr<cache::CacheImageInterface> *, const string &, void *, void *))
		applauncher->func[INCOMING_APPLAUNCHER_FUNC_ICON])(&icon,
			string(m_request->titleId), NULL, NULL);

	IncomingDialogHandler *handler = new IncomingDialogHandler(this);

	if (handler == NULL)
		return -1;

	CommonGuiLayoutDialogParam param;
	bool oneButton = m_request->rejectText[0] == 0;
	bool hasIcon = icon.get() != NULL;

	if (oneButton)
		param.layout = SceCommonGuiDialog_7D8FE85E;
	else
		param.layout = SceCommonGuiDialog_C4A714DE;
	if (hasIcon)
		param.cachedImage = icon;
	else
		param.image = Framework::Instance()->GetCommonTexture(IDParam(INCOMING_DEFAULT_ICON));
	param.secondImage = ((const intrusive_ptr<graph::Surface> &(*)(void))
		m_topmenu->func[INCOMING_TOPMENU_FUNC_GET_FRAME])();
	param.handler = handler;
	param.text = m_request->dialogText;
	param.notifyClose = true;
	m_dialog = shellStateWatcherOpenDialog(&param, false, 1);
	if (m_dialog != -1) {
		if (oneButton) {
			SceCommonGuiDialog_8947FC23(m_dialog, INCOMING_PART_BUTTON)->SetString(
				wstring(m_request->acceptText));
		} else {
			SceCommonGuiDialog_8947FC23(m_dialog, INCOMING_PART_SECOND_BUTTON)->SetString(
				wstring(m_request->acceptText));
			SceCommonGuiDialog_8947FC23(m_dialog, INCOMING_PART_BUTTON)->SetString(
				wstring(m_request->rejectText));
		}
		if (!hasIcon)
			((bool (*)(intrusive_ptr<cache::CacheImageInterface> *, const string &, void *,
				   void *))applauncher->func[INCOMING_APPLAUNCHER_FUNC_ICON])(&icon,
				string(m_request->titleId), (void *)incomingDialogMgrOnIcon,
				(void *)m_dialog);

		Plugin *power = Plugin::Find("power_manage_plugin");

		if (power != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)power->GetInterface(1);

			if (iface != NULL)
				((void (*)(bool))iface->func[INCOMING_POWER_FUNC_DIALOG])(true);
		}
	} else {
		delete handler;
	}
	return m_dialog != -1 ? 0 : -1;
}

/* (0x81150D9C) */
int IncomingDialogMgr::ShowIndicator()
{
	if (m_indicatorShown)
		return 0;

	Plugin *plugin = Plugin::Find("indicator_plugin");

	if (plugin != NULL) {
		ShellPluginInterface *indicator = (ShellPluginInterface *)plugin->GetInterface(1);

		if (indicator != NULL) {
			IncomingIndicatorParam param(m_request->titleId, m_request->titleId);

			{
				wstring text(m_request->dialogText);
				string utf8;

				common::Utf16ToUtf8(text, &utf8);
				param.text = utf8;
			}
			param.timeout = -1;
			param.type = 2;
			((void (*)(IncomingIndicatorParam *))
				indicator->func[INCOMING_INDICATOR_FUNC_SHOW])(&param);
			m_indicatorShown = true;
			return 0;
		}
	}
	return -1;
}

/* (0x81150F94) */
void IncomingDialogMgr::PlaySound()
{
	if (m_request->audioPath[0] != '\0') {
		IncomingDialogMgrPlayJob *play = new IncomingDialogMgrPlayJob(m_request->processId,
			m_request->titleId, m_request->audioPath, m_mountPoint);
		common::SharedPtr<job::JobItem> job(play);

		if (play != NULL) {
			job::JobQueue::default_queue->Enqueue(job);
			m_soundPlaying = true;
		}
	}
}

/* (0x811510BE) */
void IncomingDialogMgr::Update()
{
	bool canOpen = false;

	if (m_topmenu != NULL)
		canOpen = ((int (*)(void))m_topmenu->func[INCOMING_TOPMENU_FUNC_BUSY])() == 0;
	if (parentalGetRemainingSec() == 0)
		canOpen = false;
	if (!m_inApp || m_onStartScreen) {
		if (m_state == STATE_INDICATOR && canOpen) {
			HideIndicator();
			if (OpenDialog() == 0) {
				m_state = STATE_DIALOG;
			} else {
				Close(INCOMING_RESULT_CANCELED);
				m_state = STATE_IDLE;
			}
		}
	} else if (m_state == STATE_DIALOG) {
		CloseDialog();
		if (ShowIndicator() == 0) {
			m_state = STATE_INDICATOR;
		} else {
			Close(INCOMING_RESULT_CANCELED);
			m_state = STATE_IDLE;
		}
	}
}

/* (0x81151192) */
void IncomingDialogMgr::Tick()
{
	switch (m_state) {
	case STATE_DIALOG:
	case STATE_INDICATOR:
		if (m_request == NULL)
			break;
		if (m_request->canceled) {
			Close(INCOMING_RESULT_CANCELED);
			m_state = STATE_IDLE;
		} else if (m_request->startTime + m_request->timeout * 1000000 <
			   sceKernelGetProcessTimeWide()) {
			Close(INCOMING_RESULT_TIMEOUT);
			m_state = STATE_IDLE;
		} else {
			Update();
		}
		break;
	case STATE_IDLE: {
		if (m_request != NULL)
			return;
		m_request = shellIncomingDialogServerTakeRequest();
		if (m_request == NULL)
			return;
		if (m_topmenu == NULL) {
			Plugin *topmenu = Plugin::Find("topmenu_plugin");

			if (topmenu != NULL) {
				m_topmenu = (ShellPluginInterface *)topmenu->GetInterface(1);
			} else {
				Close(INCOMING_RESULT_CANCELED);
				return;
			}
		}

		bool canOpen = ((int (*)(void))m_topmenu->func[INCOMING_TOPMENU_FUNC_BUSY])() == 0;

		if (parentalGetRemainingSec() == 0)
			canOpen = false;
		PlaySound();
		m_request->startTime = sceKernelGetProcessTimeWide();
		if ((m_inApp && !m_onStartScreen) || !canOpen) {
			if (ShowIndicator() == 0)
				m_state = STATE_INDICATOR;
			else
				Close(INCOMING_RESULT_CANCELED);
		} else if (OpenDialog() == 0) {
			m_state = STATE_DIALOG;
		} else {
			Close(INCOMING_RESULT_CANCELED);
		}

		Plugin *power = Plugin::Find("power_manage_plugin");

		if (power != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)power->GetInterface(1);

			if (iface != NULL)
				((void (*)(void))iface->func[INCOMING_POWER_FUNC_NOTIFY])();
		}
		break;
	}
	case STATE_STOPPED:
		if (m_request != NULL)
			Close(INCOMING_RESULT_CANCELED);
		break;
	}
}

/* (0x811512E2) */
void IncomingDialogMgr::PrepareLaunch()
{
	Plugin *power = Plugin::Find("power_manage_plugin");

	if (m_launchPending)
		m_launchPending = false;
	if (power != NULL && power->GetInterface(1) != NULL) {
		bool hasArg = false;

		m_launchPending = true;
		m_launchFlag = m_request->launchFlag;
		sce_paf_strncpy(m_launchTitleId, m_request->titleId, sizeof(m_launchTitleId));
		m_launchArg = "incoming_dialog:";
		if (m_onStartScreen) {
			m_launchArg += "from_start_screen=true";
			hasArg = true;
		}
		if (m_request->param != NULL) {
			if (hasArg)
				m_launchArg += "&";

			string name("param=");

			m_launchArg.append(name + m_request->param);
		}
	}
}

/* (0x81151580) */
void IncomingDialogMgr::OnDialogButton(SceInt32 id, SceInt32 button)
{
	if (button == INCOMING_BUTTON_CLOSED) {
		if (m_launchPending) {
			Plugin *plugin = Plugin::Find("power_manage_plugin");

			if (plugin != NULL) {
				ShellPluginInterface *power =
					(ShellPluginInterface *)plugin->GetInterface(1);

				if (power != NULL) {
					((void (*)(bool, const char *, const char *))
						power->func[INCOMING_POWER_FUNC_LAUNCH])(m_launchFlag,
							m_launchTitleId, m_launchArg.c_str());
					m_launchArg.clear();
				}
			}
			m_launchPending = false;
		}
	} else if (id == m_dialog) {
		switch (button) {
		case INCOMING_BUTTON_ACCEPT:
			PrepareLaunch();
			Close(INCOMING_RESULT_ACCEPTED);
			break;
		case INCOMING_BUTTON_REJECT:
			Close(INCOMING_RESULT_REJECTED);
			break;
		}
		m_state = STATE_IDLE;
	}
}

/* (0x81151606) */
void IncomingDialogMgr::OnEvent(int code, const void *data, SceSize size)
{
	switch (code) {
	case INCOMING_EVENT_APP_ON:
		m_inApp = true;
		break;
	case INCOMING_EVENT_APP_OFF:
		m_inApp = false;
		break;
	case INCOMING_EVENT_START_SCREEN_OFF:
		m_onStartScreen = false;
		break;
	case INCOMING_EVENT_START_SCREEN_ON:
		m_onStartScreen = true;
		break;
	}
	Update();
}

/* (0x81151636) */
static void incomingDialogMgrOnEvent(int code, const void *data, SceSize size, void *arg)
{
	((IncomingDialogMgr *)arg)->OnEvent(code, data, size);
}

/* (0x81151648) */
void IncomingDialogHandler::operator()(SceInt32 value, SceInt32 button)
{
	m_mgr->OnDialogButton(value, button);
}

/* the application's icon, once it is loaded, shown in the dialog (0x81151654) */
static void incomingDialogMgrOnIcon(const intrusive_ptr<cache::CacheImageInterface> &icon,
				    int dialog)
{
	ui::Widget *part = SceCommonGuiDialog_8947FC23(dialog, INCOMING_PART_ICON);

	if (part != NULL)
		part->SetDrawObjMaskBit(&icon, 0);
}
