/*
 * SceNpEulaDialogPlugin (vs0:sys/external/libcdlg_npeula.suprx): the NP EULA dialog's plugin,
 * which SceCommonDialogMain loads for libcdlg's sceNpEulaDialog*. It fetches the title's
 * index.xml from the EULA server of the NP environment, then the text for the account's
 * country and language and the images in it, shows them on its page, and puts the user's
 * answer in the work memory (CommonDialogNpEulaParam). The file's name and the names in it
 * are ours: no Sony path or record names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include <np/np_manager.h>
#include <std/string>
#include <plugin.h>
#include <module/module.h>
#include <common/main_thread_call_list.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"
#include "npeula_dialog_plugin.h"

extern "C" {
#include "../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;

/* the smallest SceHttp memory pool the dialog works with */
#define NPEULA_HTTP_POOL_MIN		0x10000

/* the job queue's threads */
#define NPEULA_JOB_THREADS		8
#define NPEULA_JOB_PRIORITY		0x100000F3
#define NPEULA_JOB_STACK_SIZE		0x10000

/* the message dialog's option for an error */
#define NPEULA_ERROR_DIALOG_FLAGS	16

/* the results the dialog ends with (CommonDialogNpEulaParam::errorCode, names ours) */
#define NPEULA_RESULT_ACCEPTED		1
#define NPEULA_RESULT_UP_TO_DATE	2
#define NPEULA_RESULT_ERROR		3
#define NPEULA_RESULT_CLOSED		4

extern "C" void *__dso_handle;
void *__dso_handle;

/* SNC's linker-provided constructor bounds */
extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

static Plugin::ModuleInterface s_moduleInterface;

static EulaDialog s_eulaDialog;

/* a handler that calls a function */
class EulaDialogHandler : public CommonGuiDialogHandler {
public:
	EulaDialogHandler(CommonGuiDialogFunction func, void *arg) : m_func(func), m_arg(arg)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		m_func(value, button, m_arg);
	}

	CommonGuiDialogFunction m_func;
	void *m_arg;
};

static void eulaDialogInit(SceSize argSize, const void *argp);
static void eulaDialogTerm(void);
static void OnUpdate(void *userdata);
static void OnErrorDialog(SceInt32 value, SceInt32 button, void *arg);
static void OnReceive(int size, int error, EulaHttpJob *job);
static void OnTextJobFinished(int result, int received, EulaHttpJob *job);
static void OnIndexJobFinished(int result, int received, EulaHttpJob *job);
static void OnImageReceive(int size, int error, EulaHttpJob *job);
static void OnImageJobFinished(int result, int received, EulaHttpJob *job);
static void eulaDialogInitPlugin(Plugin *plugin);
static void eulaDialogStart(Plugin *plugin);
static void eulaDialogStop(Plugin *plugin);
static void eulaDialogExit(Plugin *plugin);
static void eulaDialogSetParam(Plugin::InitParam2 *param);

extern "C" int module_start(SceSize argSize, const void *argp)
{
	eulaDialogInit(argSize, argp);
	return 0;
}

extern "C" int module_stop(SceSize argSize, const void *argp)
{
	eulaDialogTerm();
	return 0;
}

/* the static objects' constructors, and the plugin's functions for SceCommonDialogMain's
   paf::Module */
static __attribute__((noinline)) void eulaDialogInit(SceSize argSize, const void *argp)
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
	s_moduleInterface.init_func = eulaDialogInitPlugin;
	s_moduleInterface.start_func = eulaDialogStart;
	s_moduleInterface.stop_func = eulaDialogStop;
	s_moduleInterface.exit_func = eulaDialogExit;
	s_moduleInterface.set_param_func = eulaDialogSetParam;
	module->SetInterface(0, &s_moduleInterface);
}

static __attribute__((noinline)) void eulaDialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

EulaDialog::EulaDialog()
	: m_plugin(NULL), m_state(NPEULA_STATE_IDLE), m_view(this), m_result(0), m_errorCode(0),
	  m_npCommId(), m_version(0), m_resultVersion(0), m_textSize(0), m_jobsCanceled(true),
	  m_mutex("SceNpEulaMutex", 0), m_errorShown(false), m_option(0)
{
}

EulaDialog::~EulaDialog()
{
	m_text.reset();
}

/* the dialog's next state: an error does not undo its end */
void EulaDialog::SetState(int state)
{
	if ((m_state == NPEULA_STATE_ABORTED || m_state == NPEULA_STATE_FINISH ||
	     m_state == NPEULA_STATE_CANCELED) && state == NPEULA_STATE_ERROR)
		return;
	m_state = state;
}

/* the caller's title and options, and the job queue; the dialog runs on the main thread */
void EulaDialog::Init(Plugin *plugin)
{
	CommonDialogNpEulaParam *work;

	m_plugin = plugin;
	work = (CommonDialogNpEulaParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_NPEULA);
	m_npCommId = work->npCommId;
	m_version = work->version;
	m_option = work->option != 0;
	job::JobQueue::Option option;
	SceHttpMemoryPoolStats stats;

	option.numThreads = NPEULA_JOB_THREADS;
	option.priority = NPEULA_JOB_PRIORITY;
	option.stackSize = NPEULA_JOB_STACK_SIZE;
	m_jobQueue = new job::JobQueue("SceNPEULAJobManager", &option);
	m_jobsCanceled = false;
	if (sceHttpGetMemoryPoolStats(&stats) == SCE_OK && stats.poolSize < NPEULA_HTTP_POOL_MIN) {
		m_result = NPEULA_ERROR_HTTP_POOL;
		m_state = NPEULA_STATE_ERROR_SHOWN;
		common::MainThreadCallList::Register(OnUpdate, plugin);
		return;
	}
	m_state = NPEULA_STATE_OPEN;
	common::MainThreadCallList::Register(OnUpdate, plugin);
}

void EulaDialog::Term(Plugin *plugin)
{
	common::MainThreadCallList::Unregister(OnUpdate, plugin);
	m_text.reset();
	if (m_jobQueue != NULL) {
		delete m_jobQueue;
		m_jobQueue = NULL;
	}
	m_mutex.Lock();
	m_images.clear();
	m_mutex.Unlock();
	m_view.Close();
	plugin->PageCloseAll();
}

/* the image of the URL if it has come, else a job that fetches it into the text's index */
intrusive_ptr<graph::Surface> EulaDialog::GetImage(const string &url, int index)
{
	string key;
	intrusive_ptr<graph::Surface> surface;
	int ret;

	key.append(m_url);
	key.append(url);
	m_mutex.Lock();
	for (map<int, EulaImage>::iterator it = m_images.begin(); it != m_images.end(); ++it) {
		if (key == it->second.url) {
			surface = it->second.surface;
			break;
		}
	}
	m_mutex.Unlock();
	if (surface.get() != NULL)
		return surface;
	if (m_url.length() != 0) {
		common::SharedPtr<job::JobItem> job(new EulaHttpJob(key, common::SharedPtr<Buffer>(),
								    ::OnImageReceive, OnImageJobFinished,
								    this, index));

		ret = m_jobQueue->Enqueue(job);
		if (ret != 0) {
			m_result = ret;
			SetState(NPEULA_STATE_ERROR);
			return intrusive_ptr<graph::Surface>();
		}
		return intrusive_ptr<graph::Surface>();
	}
	return intrusive_ptr<graph::Surface>();
}

bool EulaDialog::IsJobQueueEmpty()
{
	if (m_jobQueue == NULL)
		return true;
	return m_jobQueue->NumItems() == 0;
}

/* the index of the title's EULA:
   http://a1.ww.<env>.dl.playstation.net/eula/<env>/<NP communication ID>/index.xml */
void EulaDialog::StartIndex()
{
	char npCommId[32];
	char environment[16];

	sce_paf_memset(npCommId, 0, sizeof(npCommId));
	sce_paf_memcpy(npCommId, &m_npCommId, sizeof(m_npCommId.data));
	SceNpCommon_E426FEE8(&m_npCommId, npCommId, sizeof(npCommId));
	sce_paf_memset(environment, 0, sizeof(environment));
	m_result = sceNpCommonGetNpEnviroment(environment);
	if (m_result != 0) {
		SetState(NPEULA_STATE_ERROR);
		return;
	}
	m_view.SetBusy(true);
	m_url = "http://a1.ww.";
	m_url += environment;
	m_url += ".dl.playstation.net/eula/";
	m_url += environment;
	m_url += "/";
	m_url += npCommId;
	m_url += "/";
	string url(m_url);

	m_view.SetBaseUrl(m_url);
	url += "index.xml";
	common::SharedPtr<job::JobItem> job(new EulaHttpJob(url, common::SharedPtr<Buffer>(),
							    OnReceive, OnIndexJobFinished, this));

	m_result = m_jobQueue->Enqueue(job);
	if (m_result != 0) {
		SetState(NPEULA_STATE_ERROR);
		return;
	}
}

void EulaDialog::SetText()
{
	m_view.SetText((const char *)m_text->GetAddr(), m_textSize);
}

/* the error in a message dialog, once */
void EulaDialog::ShowError()
{
	if (m_errorShown)
		return;
	m_view.SetBusy(false);
	CommonGuiMessageDialogParam param;

	param.option.flags = NPEULA_ERROR_DIALOG_FLAGS;
	param.type = m_result;
	SceCommonGuiDialog_A6422883(m_plugin, &param, new EulaDialogHandler(::OnErrorDialog, this),
				    NULL);
	m_errorShown = true;
}

/* the dialog's result for the caller, and SceCommonDialogMain finishes the call */
void EulaDialog::Finish(SceInt32 result, SceInt32 errorCode)
{
	CommonDialogNpEulaParam *work;

	work = (CommonDialogNpEulaParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_NPEULA);
	work->result = result;
	work->errorCode = errorCode;
	work->resultVersion = m_resultVersion;
	SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_NPEULA);
}

/* on the main thread: the dialog's next step; the caller's abort ends it */
void EulaDialog::Update(Plugin *plugin)
{
	int aborted = SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_NPEULA);
	int state = m_state;

	/* the caller has aborted the dialog */
	if (aborted && state != NPEULA_STATE_ABORTED && state != NPEULA_STATE_FINISH)
		state = NPEULA_STATE_ABORTED;
	switch (state) {
	case NPEULA_STATE_IDLE:
		break;
	case NPEULA_STATE_OPEN:
		m_view.Open(plugin);
		m_state = NPEULA_STATE_START;
		break;
	case NPEULA_STATE_START:
		StartIndex();
		m_state = NPEULA_STATE_IDLE;
		break;
	case NPEULA_STATE_TEXT_READY:
		m_state = NPEULA_STATE_SET_TEXT;
		break;
	case NPEULA_STATE_SET_TEXT:
		SetText();
		m_state = NPEULA_STATE_WAIT_TEXT;
		break;
	case NPEULA_STATE_WAIT_TEXT:
		if (m_jobQueue->NumItems() <= 0 && m_view.m_laidOut) {
			m_view.ShowText(true);
			m_view.SetBusy(false);
			m_state = NPEULA_STATE_IDLE;
		}
		break;
	case NPEULA_STATE_ERROR:
		ShowError();
		m_state = NPEULA_STATE_IDLE;
		m_resultVersion = 0;
		break;
	case NPEULA_STATE_ACCEPTED:
		m_state = NPEULA_STATE_FINISH;
		m_result = 0;
		m_errorCode = NPEULA_RESULT_ACCEPTED;
		break;
	case NPEULA_STATE_CLOSED:
		m_state = NPEULA_STATE_FINISH;
		m_result = 0;
		m_errorCode = NPEULA_RESULT_CLOSED;
		m_resultVersion = 0;
		break;
	case NPEULA_STATE_DECLINED:
		m_state = NPEULA_STATE_FINISH;
		m_result = 1;
		m_errorCode = 0;
		m_resultVersion = 0;
		break;
	case NPEULA_STATE_UP_TO_DATE:
		m_state = NPEULA_STATE_FINISH;
		m_result = 0;
		m_errorCode = NPEULA_RESULT_UP_TO_DATE;
		break;
	case NPEULA_STATE_ABORTED:
		m_state = NPEULA_STATE_FINISH;
		m_result = 2;
		m_errorCode = 0;
		m_resultVersion = 0;
		break;
	case NPEULA_STATE_CANCELED:
		m_state = NPEULA_STATE_FINISH;
		m_result = 1;
		m_errorCode = 0;
		m_resultVersion = 0;
		break;
	case NPEULA_STATE_ERROR_SHOWN:
		m_state = NPEULA_STATE_FINISH;
		m_errorCode = NPEULA_RESULT_ERROR;
		m_resultVersion = 0;
		break;
	case NPEULA_STATE_FINISH:
		if (m_jobQueue != NULL) {
			if (!m_jobsCanceled) {
				m_jobQueue->CancelAllItems();
				m_jobsCanceled = true;
			}
			if (m_jobQueue->NumItems() != 0)
				break;
		}
		Finish(m_result, m_errorCode);
		break;
	}
}

/* the error dialog has closed */
void EulaDialog::OnErrorDialog(SceInt32 value, SceInt32 button)
{
	SceCommonGuiDialog_BC311DC0(value);
	m_state = NPEULA_STATE_ERROR_SHOWN;
}

void EulaDialog::OnTextReceive(int size, int error)
{
}

/* the text has come: the page shows it next */
void EulaDialog::OnTextFinished(int result, common::SharedPtr<Buffer> buffer, int size)
{
	if (result == 0 && size != 0 && size <= NPEULA_TEXT_MAX) {
		if (size > 0) {
			m_text = buffer;
			m_textSize = size;
			m_state = NPEULA_STATE_TEXT_READY;
		}
		return;
	}
	if (size == 0 || size > NPEULA_TEXT_MAX)
		result = NPEULA_ERROR_TEXT_SIZE;
	m_result = result;
	SetState(NPEULA_STATE_ERROR);
}

/* the index has come: the account's file, fetched unless the caller has agreed to it */
void EulaDialog::OnIndexFinished(int result, common::SharedPtr<Buffer> buffer, int size)
{
	EulaIndexParser *parser;
	int language;
	EulaFile *file;
	EulaIndex *index;
	SceNpCountryCode country;
	int ret;

	if (result != 0) {
		OnTextFinished(result, buffer, size);
		return;
	}
	if (size <= 0 || buffer.get() == NULL)
		return;
	parser = NULL;
	if (buffer->GetSize() != 0) {
		EulaIndexParser::Create(&parser);
		ret = parser->Init(&m_npCommId);
		if (ret < 0) {
			m_result = ret;
			SetState(NPEULA_STATE_ERROR);
			EulaIndexParser::Destroy(parser);
			return;
		}
		ret = parser->Parse((const char *)buffer->GetAddr(), size, 1);
		if (ret < 0) {
			m_result = ret;
			SetState(NPEULA_STATE_ERROR);
			EulaIndexParser::Destroy(parser);
			return;
		}
		if (ret != 0)
			return;
	}
	ret = sceNpManagerGetAccountRegion(&country, &language);
	if (ret < 0) {
		m_result = ret;
		SetState(NPEULA_STATE_ERROR);
		EulaIndexParser::Destroy(parser);
		return;
	}
	index = parser->m_index;
	parser->m_index = NULL;
	ret = index->Find(language, &country, &file);
	if (ret < 0) {
		m_result = ret;
		SetState(NPEULA_STATE_ERROR);
	} else if (m_url.length() != 0 && file->m_name[0] != '\0') {
		if (m_version < file->m_version || m_option != 0) {
			m_resultVersion = file->m_version;
			string url(m_url);

			url += file->m_name;
			common::SharedPtr<job::JobItem> job(new EulaHttpJob(url, common::SharedPtr<Buffer>(),
									    OnReceive, OnTextJobFinished,
									    this));

			if (m_jobQueue->Enqueue(job) != 0) {
				/* Sony's: the error reported is the search's result */
				m_result = ret;
				SetState(NPEULA_STATE_ERROR);
				goto done;
			}
		} else {
			m_resultVersion = m_version;
			m_state = NPEULA_STATE_UP_TO_DATE;
		}
	}
done:
	EulaIndex::Destroy(index);
	EulaIndexParser::Destroy(parser);
}

void EulaDialog::OnImageReceive(int size, int error)
{
}

/* an image has come: it is decoded, kept, and shown in the text */
void EulaDialog::OnImageFinished(int result, int received, EulaHttpJob *job)
{
	if (result != 0) {
		m_result = result;
		SetState(NPEULA_STATE_ERROR);
		return;
	}
	if (job == NULL)
		return;
	common::SharedPtr<Buffer> buffer = job->m_buffer;

	if (buffer.get() == NULL)
		return;
	if (buffer->GetSize() != 0) {
		EulaDialog *owner = job->m_owner;

		if (owner != NULL) {
			graph::Surface::LoadOption option;
			intrusive_ptr<graph::Surface> surface = graph::Surface::Load(
				m_plugin->m_surface_pool, buffer->GetAddr(), buffer->GetSize(), &option);

			if (option.error != 0) {
				m_result = option.error;
				SetState(NPEULA_STATE_ERROR);
				return;
			}
			EulaImage image;

			image.surface = surface;
			image.url = job->m_url;
			m_mutex.Lock();
			m_images.insert(pair<int, EulaImage>(job->m_id, image));
			owner->m_view.SetImage(image.surface, job->m_id);
			m_mutex.Unlock();
		}
	}
}

/* the plugin's functions for SceCommonDialogMain */
static void eulaDialogInitPlugin(Plugin *plugin)
{
}

static void eulaDialogStart(Plugin *plugin)
{
	s_eulaDialog.Init(plugin);
}

static void eulaDialogStop(Plugin *plugin)
{
	s_eulaDialog.Term(plugin);
}

static void eulaDialogExit(Plugin *plugin)
{
}

static void eulaDialogSetParam(Plugin::InitParam2 *param)
{
}

static void OnUpdate(void *userdata)
{
	s_eulaDialog.Update((Plugin *)userdata);
}

/* the jobs' functions, on the main thread: the job's dialog gets them */
static void OnReceive(int size, int error, EulaHttpJob *job)
{
	if (job != NULL && job->m_owner != NULL)
		job->m_owner->OnTextReceive(size, error);
}

static void OnTextJobFinished(int result, int received, EulaHttpJob *job)
{
	if (job == NULL)
		return;
	EulaDialog *owner = job->m_owner;
	common::SharedPtr<Buffer> buffer = job->m_buffer;

	if (owner != NULL)
		owner->OnTextFinished(result, buffer, received);
}

static void OnIndexJobFinished(int result, int received, EulaHttpJob *job)
{
	if (job == NULL)
		return;
	EulaDialog *owner = job->m_owner;
	common::SharedPtr<Buffer> buffer = job->m_buffer;

	if (owner != NULL)
		owner->OnIndexFinished(result, buffer, received);
}

static void OnImageReceive(int size, int error, EulaHttpJob *job)
{
	if (job != NULL && job->m_owner != NULL)
		job->m_owner->OnImageReceive(size, error);
}

static void OnImageJobFinished(int result, int received, EulaHttpJob *job)
{
	if (job != NULL && job->m_owner != NULL)
		job->m_owner->OnImageFinished(result, received, job);
}

static void OnErrorDialog(SceInt32 value, SceInt32 button, void *arg)
{
	if (arg != NULL)
		((EulaDialog *)arg)->OnErrorDialog(value, button);
}
