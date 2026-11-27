/*
 * SceSigninExt (vs0:sys/external/signin_ext.suprx):
 * Standalone PAF plugin entrypoints and SigninExtPlugin singleton managing
 * module lifecycle, mutex serialization, and SigninExtApp execution.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <plugin.h>
#include <module/module.h>
#include <thread/thread.h>
#include "signin_ext_internal.h"

extern "C" {
#include "../../../../../vsh/paf/src/std/stdc++/atexit.h"

void *__dso_handle;
extern void (*__start__Zpreinit_array[])(void);
extern void (*__stop__Zpreinit_array[])(void);
extern void (*__start__Zinit_array[])(void);
extern void (*__stop__Zinit_array[])(void);
}

using namespace paf;
using namespace paf::thread;

SigninExtPlugin *SigninExtPlugin::s_instance = NULL;

static Plugin::ModuleInterface s_signinExtModuleInterface = {
	SigninExtSetParam,
	SigninExtInit,
	SigninExtStart,
	SigninExtStop,
	SigninExtExit
};

__attribute__((noinline))
static void SigninExtModuleInit(void)
{
	__dso_handle = &__dso_handle;
	for (void (**ctor)(void) = __start__Zinit_array; ctor < __stop__Zinit_array; ++ctor) {
		if (*ctor != NULL)
			(*ctor)();
	}
}

__attribute__((noinline))
static void SigninExtModuleTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

extern "C" int module_start(SceSize arglen, const void *argp)
{
	(void)arglen;
	SigninExtModuleInit();
	Module *module = *(Module *const *)argp;
	module->SetInterface(1, &s_signinExtModuleInterface);
	return 0;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	SigninExtModuleTerm();
	return 0;
}

void SigninExtSetParam(Plugin::InitParam2 *param)
{
	SigninExtRegisterWidgetFactory();
	param->draw_priority = 100;
	param->graphics_flag = 6;
}

void SigninExtInit(Plugin *plugin)
{
	SigninExtPlugin::Create(plugin);
}

void SigninExtStart(Plugin *plugin)
{
	(void)plugin;
	SigninExtPlugin::StartInstance();
}

void SigninExtStop(Plugin *plugin)
{
	(void)plugin;
	SigninExtPlugin::StopInstance();
}

void SigninExtExit(Plugin *plugin)
{
	(void)plugin;
	SigninExtPlugin::Destroy();
	SigninExtUnregisterWidgetFactory();
}

SigninExtPlugin::~SigninExtPlugin()
{
}

void SigninExtPlugin::Create(Plugin *plugin)
{
	s_instance = new SigninExtPlugin(plugin);
	SigninExtJobQueueInit();
	SigninExtImageManagerCreate(NULL);
}

void SigninExtPlugin::Destroy(void)
{
	if (s_instance != NULL) {
		Mutex *mutex = &s_instance->m_mutex;
		mutex->Lock();
		mutex->Unlock();
	}
	SigninExtImageManagerDestroy();
	SigninExtJobQueueTerm();
	if (s_instance != NULL) {
		delete s_instance;
		s_instance = NULL;
	}
}

void SigninExtPlugin::OnAppFinished(int32_t result, void *userData)
{
	(void)userData;
	if (m_app != NULL) {
		delete m_app;
		m_app = NULL;
	}
	if (result > 0)
		result = -3;
	m_plugin->Notify(result);
	m_mutex.Unlock();
}

void SigninExtPlugin::Start(void)
{
	m_mutex.Lock();
	m_app = new SigninExtApp();
	if (m_app != NULL) {
		Callback1<int32_t> *cb = new MemberCallback1<SigninExtPlugin, int32_t>(
			this, &SigninExtPlugin::OnAppFinished);
		m_app->Start(cb);
	} else {
		m_plugin->Notify(-3);
		m_mutex.Unlock();
	}
}

void SigninExtPlugin::StartInstance(void)
{
	if (s_instance != NULL)
		s_instance->Start();
}

void SigninExtPlugin::StopInstance(void)
{
	if (s_instance != NULL && s_instance->m_app != NULL)
		s_instance->m_app->Abort();
}
