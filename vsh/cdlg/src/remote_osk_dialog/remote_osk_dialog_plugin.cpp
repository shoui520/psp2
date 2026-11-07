/*
 * SceRemoteOSKDialogPlugin (vs0:sys/external/libcdlg_remote_osk.suprx): the remote OSK dialog's
 * plugin, which SceCommonDialogMain loads for libcdlg's sceRemoteOSKDialog*. It listens to the
 * companion device (SceCompanionUtil) whose keyboard opens and closes text fields and types their
 * text, and on the main thread opens the dialog once the caller's parameter is in the work memory
 * (CommonDialogRemoteOskParam), shows the companion's text in it, closes it on the caller's abort,
 * and puts the dialog's result there when it closes. The names the plugin writes into the work
 * memory (Init, Exit, Callback, OnUpdate, CloseDialogCallback) are Sony's function names.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <libime.h>
#include <ime_dialog.h>
#include <std/string.h>
#include <std/string>
#include <plugin.h>
#include <module/module.h>
#include <versatileparam/idparam.h>
#include <common/main_thread_call_list.h>
#include <companion_util/companion_util.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"
#include "remote_osk_dialog_plugin.h"

extern "C" {
#include "../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;

/* the work memory is missing for a call (name ours) */
#define REMOTE_OSK_DIALOG_PLUGIN_ERROR_NO_WORK_MEMORY	-2146431710	/* 0x80100D22 */

/*
 * The work memory, locked for one of the plugin's functions under the function's name, as
 * libcdlg's own calls lock it (CommonDialogApiScope): ret is the lock's error. Unlike libcdlg's
 * scope it lets the lock go without checking the name it finds.
 */
class RemoteOskDialogPluginApiScope {
public:
	RemoteOskDialogPluginApiScope(const char *name, int &ret) : m_name(name), m_work(NULL)
	{
		ret = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_REMOTE_OSK);
		if (ret != SCE_OK)
			return;
		m_work = (CommonDialogRemoteOskParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_REMOTE_OSK);
		if (m_work == NULL) {
			ret = REMOTE_OSK_DIALOG_PLUGIN_ERROR_NO_WORK_MEMORY;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_REMOTE_OSK);
			return;
		}
		sce_paf_strncpy(m_work->apiName, name, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		ret = SCE_OK;
	}

	~RemoteOskDialogPluginApiScope()
	{
		if (m_work == NULL)
			return;
		if (sce_paf_strncmp(m_work->apiName, m_name, sizeof(m_work->apiName)) == 0) {
			sce_paf_memset(m_work->apiName, 0, sizeof(m_work->apiName));
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_REMOTE_OSK);
		}
	}

	CommonDialogRemoteOskParam *work(void) const
	{
		return m_work;
	}

private:
	const char *m_name;
	CommonDialogRemoteOskParam *m_work;
};

/* the plugin's draw priority and texture filter (Plugin::InitParam2) */
#define REMOTE_OSK_DIALOG_DRAW_PRIORITY		10
#define REMOTE_OSK_DIALOG_TEXTURE_LOAD_FILTER	1

/* what the companion tells the plugin (SceCompanionUtilEventHandler; names ours) */
enum {
	REMOTE_OSK_EVENT_OPEN = 9,
	REMOTE_OSK_EVENT_CLOSE = 10,
	REMOTE_OSK_EVENT_CONNECT = 16,
	REMOTE_OSK_EVENT_TEXT = 17
};

/* an event's data: the text, its length, and (REMOTE_OSK_EVENT_OPEN) the text field's flags and
   length */
typedef struct RemoteOskEventData {
	const wchar_t *text;
	SceUInt32 textLength;
	SceUInt32 flags;
	SceUInt32 maxTextLength;
} RemoteOskEventData;

/* the companion's flags of a text field: its type (two fields), password, enter label,
   capitalization (names ours) */
#define REMOTE_OSK_FLAG_TYPE_LOW		0x003
#define REMOTE_OSK_FLAG_MULTILINE		0x004
#define REMOTE_OSK_FLAG_PASSWORD		0x008
#define REMOTE_OSK_FLAG_ENTER_LABEL_SHIFT	4
#define REMOTE_OSK_FLAG_TYPE_HIGH_SHIFT		8
#define REMOTE_OSK_FLAG_AUTO_CAPITALIZATION	0x400

/* how the plugin stands with the companion (names ours) */
enum {
	REMOTE_OSK_STATE_NONE = 0,
	REMOTE_OSK_STATE_WAIT = 1,
	/* the companion is there: the dialog may open */
	REMOTE_OSK_STATE_READY = 2,
	/* a text came, for the dialog to show */
	REMOTE_OSK_STATE_TEXT = 3,
	/* the dialog is open */
	REMOTE_OSK_STATE_OPEN = 4
};

/* the frames between the companion's state requests while no text field is open */
#define REMOTE_OSK_KEEP_ALIVE_FRAMES	60

extern "C" void *__dso_handle;
void *__dso_handle;

/* SNC's linker-provided constructor bounds */
extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

/* the empty name of the plugin's resource IDs: paf's headers declare it weak, so each module that
   builds an IDParam from it keeps its own copy (Sony's code tests it for NULL at every use) */
const char paf::s_empty_name[] = "";

Plugin *g_remoteOskDialogPlugin;

static int s_state;
/* no text field of the companion's is open */
static int s_idle;
/* the open text field's flags and length */
static SceUInt32 s_imeFlags;
static SceUInt32 s_maxTextLength;

/* the text the companion typed, for the dialog */
static wstring s_text(L"");

static void remoteOskDialogPageInit(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);

/* the page's init event handler: nothing to do */
static InitEventFunctionEntry s_initEventTable[] = {
	{IDParam(REMOTE_OSK_DIALOG_PAGE_ID), remoteOskDialogPageInit},
	{IDParam((uint32_t)0), NULL}
};

static int s_keepAliveFrames;

static Plugin::ModuleInterface s_moduleInterface;

static void remoteOskDialogInit(SceSize argSize, const void *argp);
static void remoteOskDialogTerm(void);
static void Init(Plugin *plugin);
static void remoteOskDialogStart(Plugin *plugin);
static void remoteOskDialogStop(Plugin *plugin);
static void Exit(Plugin *plugin);
static void remoteOskDialogSetParam(Plugin::InitParam2 *param);
static void Callback(SceUInt16 event, void *data);
static int remoteOskDialogTextChanged(int composeStart, int composeLength, SceUInt32 caret,
				      int replaceStart, int replaceLength, const SceWChar16 *text);
static void CloseDialogCallback(SceUInt32 status, SceUInt32 result);
static void OnUpdate(void *userdata);

/* the dialog's result for GetResult: the status of the companion's text field, the common
   dialog's result (aborted when the caller aborted) and the error */
static inline void SetDialogResult(CommonDialogRemoteOskParam *work, SceUInt32 status,
				   SceUInt32 result, SceInt32 error)
{
	sce_paf_memset(&work->result, 0, sizeof(work->result));
	if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_REMOTE_OSK))
		result = SCE_COMMON_DIALOG_RESULT_ABORTED;
	work->result.dialogData[0] = result;
	work->result.dialogData[1] = status;
	work->result.result = error;
}

/* no text field of the companion's is open */
static inline void SetIdle(void)
{
	s_idle = 1;
	s_maxTextLength = 1;
	s_imeFlags = 0;
	s_text = L"";
}

extern "C" int module_start(SceSize argSize, const void *argp)
{
	remoteOskDialogInit(argSize, argp);
	return 0;
}

extern "C" int module_stop(SceSize argSize, const void *argp)
{
	remoteOskDialogTerm();
	return 0;
}

/* the static objects' constructors, and the plugin's functions for SceCommonDialogMain's
   paf::Module */
static __attribute__((noinline)) void remoteOskDialogInit(SceSize argSize, const void *argp)
{
	void (**init)(void);
	Module *module;

	__dso_handle = &__dso_handle;
	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	module = *(Module *const *)argp;
	s_moduleInterface.init_func = Init;
	s_moduleInterface.start_func = remoteOskDialogStart;
	s_moduleInterface.stop_func = remoteOskDialogStop;
	s_moduleInterface.exit_func = Exit;
	s_moduleInterface.set_param_func = remoteOskDialogSetParam;
	module->SetInterface(0, &s_moduleInterface);
}

static __attribute__((noinline)) void remoteOskDialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

/* the companion's handler is set, and it is asked for its state; on an error the dialog's
   result is the error */
static void Init(Plugin *plugin)
{
	int ret;

	g_remoteOskDialogPlugin = plugin;
	s_state = REMOTE_OSK_STATE_NONE;
	s_idle = 0;
	s_imeFlags = 0;
	s_maxTextLength = 0;
	s_text = L"";
	plugin->SetInitEventHandler(s_initEventTable);
	RemoteOskDialogPluginApiScope scope(__FUNCTION__, ret);

	if (ret == SCE_OK) {
		ret = SceCompanionUtil_0634FFD8(Callback, 1);
		if (ret == SCE_OK)
			ret = SceCompanionUtil_52AA755B();
	}
	if (ret != SCE_OK) {
		if (scope.work() == NULL)
			return;
		SetDialogResult(scope.work(), 0, SCE_COMMON_DIALOG_RESULT_OK, ret);
	}
}

static void remoteOskDialogStart(Plugin *plugin)
{
	RemoteOskDialog::Reset();
	common::MainThreadCallList::Register(OnUpdate, NULL);
}

static void remoteOskDialogStop(Plugin *plugin)
{
	plugin->PageCloseAll();
}

/* the companion hears how its text field ended, and the plugin's handler goes */
static void Exit(Plugin *plugin)
{
	int ret;
	RemoteOskDialogPluginApiScope scope(__FUNCTION__, ret);
	CommonDialogRemoteOskParam *work = scope.work();

	if (work != NULL) {
		switch (work->result.dialogData[1]) {
		case 0:
			SceCompanionUtil_18D13C74(1);
			break;
		case 1:
			SceCompanionUtil_18D13C74(1);
			break;
		case 2:
			SceCompanionUtil_18D13C74(0);
			break;
		}
	}
	SceCompanionUtil_0634FFD8(NULL, 1);
}

static void remoteOskDialogSetParam(Plugin::InitParam2 *param)
{
	param->draw_priority = REMOTE_OSK_DIALOG_DRAW_PRIORITY;
	param->texture_load_filter = REMOTE_OSK_DIALOG_TEXTURE_LOAD_FILTER;
}

/* the companion's events: a text field opens or closes, or its keyboard typed a text */
static void Callback(SceUInt16 event, void *data)
{
	int ret;
	RemoteOskDialogPluginApiScope scope(__FUNCTION__, ret);
	const RemoteOskEventData *eventData = (const RemoteOskEventData *)data;

	if (ret == SCE_OK) {
	switch (event) {
	case REMOTE_OSK_EVENT_TEXT:
		if (s_state == REMOTE_OSK_STATE_OPEN || s_state == REMOTE_OSK_STATE_READY ||
		    s_state == REMOTE_OSK_STATE_TEXT) {
			SceUInt32 textLength = eventData->textLength;

			if (s_state == REMOTE_OSK_STATE_OPEN)
				s_state = REMOTE_OSK_STATE_TEXT;
			s_text = textLength != 0 ? eventData->text : L"";
		}
		break;
	case REMOTE_OSK_EVENT_CONNECT:
		if (s_state == REMOTE_OSK_STATE_NONE)
			s_state = REMOTE_OSK_STATE_READY;
		SetIdle();
		break;
	case REMOTE_OSK_EVENT_CLOSE:
		SetIdle();
		break;
	case REMOTE_OSK_EVENT_OPEN: {
		SceUInt32 textLength = eventData->textLength;

		if (s_state == REMOTE_OSK_STATE_NONE)
			s_state = REMOTE_OSK_STATE_READY;
		s_idle = 0;
		s_imeFlags = eventData->flags;
		s_maxTextLength = eventData->maxTextLength;
		s_text = textLength != 0 ? eventData->text : L"";
		break;
	}
	}
	}
}

static void remoteOskDialogPageInit(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
}

/* the text box's changes, to the companion */
static int remoteOskDialogTextChanged(int composeStart, int composeLength, SceUInt32 caret,
				      int replaceStart, int replaceLength, const SceWChar16 *text)
{
	return SceCompanionUtil_04BC85DC(composeStart, composeLength, caret, replaceStart,
					 replaceLength, text);
}

/* the IME's parameter of a text field of the companion's, from its flags */
static void RemoteOskImeParamSet(RemoteOskImeParam *param, SceUInt32 flags,
				 SceUInt32 maxTextLength)
{
	SceUInt32 type;

	param->inputMethod = 0;
	switch ((flags & REMOTE_OSK_FLAG_TYPE_LOW) |
		(((flags >> REMOTE_OSK_FLAG_TYPE_HIGH_SHIFT) & 3) << 2)) {
	case 0:
		type = SCE_IME_TYPE_DEFAULT;
		break;
	case 1:
		type = SCE_IME_TYPE_BASIC_LATIN;
		break;
	case 2:
		type = SCE_IME_TYPE_NUMBER;
		break;
	case 3:
		type = SCE_IME_TYPE_EXTENDED_NUMBER;
		break;
	case 4:
		type = SCE_IME_TYPE_URL;
		break;
	case 5:
		type = SCE_IME_TYPE_MAIL;
		break;
	default:
		type = SCE_IME_TYPE_DEFAULT;
		break;
	}
	param->type = type;
	/* SCE_IME_OPTION_NO_AUTO_CAPITALIZATION unless the field capitalizes, and
	   SCE_IME_OPTION_MULTILINE */
	param->option = (((~flags >> 10) & 1) << 1) | ((flags >> 2) & 1);
	param->enterLabel = (SceUInt8)flags >> REMOTE_OSK_FLAG_ENTER_LABEL_SHIFT;
	/* SCE_IME_DIALOG_TEXTBOX_MODE_PASSWORD */
	param->textBoxMode = (flags >> 3) & 1;
	param->dialogMode = SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL;
	param->maxTextLength = maxTextLength;
	param->supportedLanguages = 0;
	param->languagesForced = 0;
	param->filter = NULL;
	param->textChanged = remoteOskDialogTextChanged;
}

/* the dialog has closed: its result for GetResult, and the dialog ends; or the companion's text
   field was canceled from the dialog */
static void CloseDialogCallback(SceUInt32 status, SceUInt32 result)
{
	int ret;

	if (status == 2) {
		SceCompanionUtil_18D13C74(0);
		SetIdle();
		return;
	}
	RemoteOskDialogPluginApiScope scope(__FUNCTION__, ret);
	CommonDialogRemoteOskParam *work = scope.work();

	if (ret != SCE_OK)
		return;
	if (RemoteOskDialog::Destroy() == SCE_OK) {
		if ((status == 0 || status == 1 || status == 2) &&
		    (result == SCE_COMMON_DIALOG_RESULT_OK ||
		     result == SCE_COMMON_DIALOG_RESULT_USER_CANCELED ||
		     result == SCE_COMMON_DIALOG_RESULT_ABORTED))
			SetDialogResult(work, status, result, SCE_OK);
		work->abortResult = 0;
		common::MainThreadCallList::Unregister(OnUpdate, NULL);
		SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_REMOTE_OSK);
	}
}

/*
 * On the main thread: opens the dialog once the caller's parameter is set and the companion is
 * there, shows the companion's text and text fields in it, asks the companion for its state while
 * no text field is open, and closes the dialog on the caller's abort; on an error the dialog ends.
 */
static void OnUpdate(void *userdata)
{
	int ret;
	RemoteOskDialogPluginApiScope scope(__FUNCTION__, ret);
	CommonDialogRemoteOskParam *work = scope.work();

	if (ret == SCE_OK) {
		if (work->result.result != SCE_OK) {
			ret = work->result.result;
		} else if (work->paramSet == 0) {
			if (work->abortResult == 1) {
				work->abortResult = 2;
				ret = RemoteOskDialog::Abort(SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_REMOTE_OSK));
			} else if (work->abortResult != 2) {
				if (s_state == REMOTE_OSK_STATE_TEXT) {
					s_state = REMOTE_OSK_STATE_OPEN;
					RemoteOskDialog::SetCurrentText(s_text);
					s_text.clear();
				} else if (s_state == REMOTE_OSK_STATE_OPEN) {
					bool sent = false;
					int frames = 0;
					bool active = false;

					ret = RemoteOskDialog::GetActive(&active);
					if (ret == SCE_OK) {
						bool wanted = s_idle == 0;

						if (active != wanted) {
							RemoteOskImeParam imeParam;

							RemoteOskImeParamInit(&imeParam);
							RemoteOskImeParamSet(&imeParam, s_imeFlags, s_maxTextLength);
							ret = RemoteOskImeParamCheck(&imeParam);
							if (ret == SCE_OK) {
								ret = RemoteOskDialog::SetActive(wanted, &imeParam);
								if (ret == SCE_OK) {
									int textRet = RemoteOskDialog::SetCurrentText(s_text);

									s_text.clear();
									if (textRet != SCE_OK) {
										ret = textRet;
									} else {
										sent = true;
										ret = SCE_OK;
									}
								}
							}
						}
					}
					if (ret == SCE_OK && s_idle != 0) {
						if (!sent)
							frames = s_keepAliveFrames + 1;
						s_keepAliveFrames = frames;
						if ((SceUInt32)frames >= REMOTE_OSK_KEEP_ALIVE_FRAMES) {
							s_keepAliveFrames = 0;
							SceCompanionUtil_52AA755B();
						}
					}
				}
			}
		} else if (work->paramSet == 1 && s_state != REMOTE_OSK_STATE_NONE) {
			if (s_state == REMOTE_OSK_STATE_WAIT) {
				ret = REMOTE_OSK_DIALOG_ERROR_NOT_READY;
			} else if (s_state != REMOTE_OSK_STATE_READY) {
				ret = REMOTE_OSK_DIALOG_ERROR_BUSY;
			} else {
				RemoteOskImeParam imeParam;

				s_state = REMOTE_OSK_STATE_OPEN;
				RemoteOskImeParamInit(&imeParam);
				RemoteOskImeParamSet(&imeParam, s_imeFlags, s_maxTextLength);
				ret = RemoteOskImeParamCheck(&imeParam);
				if (ret == SCE_OK) {
					ret = RemoteOskDialog::Create(&work->param, CloseDialogCallback,
								      work->infobarShown, &imeParam,
								      s_idle != 0);
					if (ret == SCE_OK) {
						ret = RemoteOskDialog::SetCurrentText(s_text);
						s_text.clear();
						if (ret == SCE_OK)
							work->paramSet = 0;
					}
				}
			}
		}
	}
	if (ret != SCE_OK) {
		RemoteOskDialog::Destroy();
		if (work != NULL) {
			SetDialogResult(work, 0, SCE_COMMON_DIALOG_RESULT_OK, ret);
			work->abortResult = 0;
		}
		common::MainThreadCallList::Unregister(OnUpdate, NULL);
		SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_REMOTE_OSK);
	}
}
