/*
 * SceCheckoutDialogPlugin (vs0:sys/external/libcdlg_checkout.suprx): the store checkout dialog's
 * plugin, which SceCommonDialogMain loads for libcdlg's sceStoreCheckoutDialog*. It loads
 * store_checkout_plugin, the store's checkout, hands it the caller's parameter and colours, and
 * passes its result and the caller's abort along. The 0.94 builds' paths put the plugin in
 * vsh/app/store/checkout/plugin; the file's name and every name here are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <plugin.h>
#include <module/module.h>
#include <common/main_thread_call_list.h>
#include <std/string.h>
#include "../../../../cdlg/src/cdlg_mgr/common_dialog_mgr.h"
#include "../../../../cdlg/src/cdlg_main/common_dialog_main.h"
#include "../store_checkout_plugin.h"

extern "C" {
#include "../../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;

/* the SDK version below which the dialog passes on empty SKU IDs too */
#define CHECKOUT_DIALOG_SDK_VERSION_SKIP_EMPTY_SKU	0x01500000

extern "C" void *__dso_handle;
void *__dso_handle;

/* SNC's linker-provided constructor bounds */
extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

/* store_checkout_plugin's mode for the dialog's */
static SceInt32 s_mode;
/* store_checkout_plugin, once it is loaded */
static Plugin *s_plugin;
/* the caller's abort was passed on */
static bool s_aborted;
static Plugin::ModuleInterface s_moduleInterface;
static StoreCheckoutPluginParam s_param;

static void checkoutDialogInit(SceSize argSize, const void *argp);
static void checkoutDialogTerm(void);
static void checkoutDialogStart(Plugin *plugin);
static void checkoutDialogUpdate(void *userdata);
static void checkoutDialogStop(Plugin *plugin);
static void checkoutDialogPluginInit(Plugin *plugin);
static void checkoutDialogPluginStart(Plugin *plugin);
static void checkoutDialogPluginExit(Plugin *plugin);
static void checkoutDialogSetParam(Plugin::InitParam2 *param);
static void checkoutDialogResult(SceInt32 result, SceInt32 npError);
static void checkoutDialogLoadFinish(Plugin *plugin);

extern "C" int module_start(SceSize argSize, const void *argp)
{
	checkoutDialogInit(argSize, argp);
	return 0;
}

extern "C" int module_stop(SceSize argSize, const void *argp)
{
	checkoutDialogTerm();
	return 0;
}

/* the static objects' constructors, and the plugin's functions for SceCommonDialogMain's
   paf::Module */
static __attribute__((noinline)) void checkoutDialogInit(SceSize argSize, const void *argp)
{
	void (**init)(void);

	__dso_handle = &__dso_handle;
	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	s_moduleInterface.init_func = checkoutDialogPluginInit;
	s_moduleInterface.start_func = checkoutDialogPluginStart;
	s_moduleInterface.stop_func = checkoutDialogStop;
	s_moduleInterface.exit_func = checkoutDialogPluginExit;
	s_moduleInterface.set_param_func = checkoutDialogSetParam;
	(*(Module *const *)argp)->SetInterface(0, &s_moduleInterface);
}

static __attribute__((noinline)) void checkoutDialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

/* on the main thread: passes the caller's abort on to store_checkout_plugin, once */
static void checkoutDialogUpdate(void *userdata)
{
	Plugin *plugin = s_plugin;
	StoreCheckoutPluginInterface *iface;

	if (s_aborted)
		return;
	if (!SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_CHECKOUT))
		return;
	if (plugin == NULL)
		return;
	iface = (StoreCheckoutPluginInterface *)plugin->GetInterface(STORE_CHECKOUT_PLUGIN_INTERFACE_VERSION);
	if (iface == NULL)
		return;
	iface->abort();
	s_aborted = true;
}

/* loads store_checkout_plugin with the caller's parameter */
static void checkoutDialogStart(Plugin *plugin)
{
	CommonDialogStoreCheckoutParam *work;
	SceKernelSystemSwVersion version;
	SceUInt32 i;

	work = (CommonDialogStoreCheckoutParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_CHECKOUT);
	version.size = sizeof(version);
	s_plugin = NULL;
	s_aborted = false;
	if (sceKernelGetSystemSwVersion(&version) < 0) {
		version.version = 0xffffffff;
		version.versionExtra = 0xffffffff;
	}
	if (work->param.sdkVersion > version.version)
		work->param.sdkVersion = version.version;
	switch (work->param.mode) {
	case SCE_STORE_CHECKOUT_DIALOG_MODE_CHECKOUT:
	case SCE_STORE_CHECKOUT_DIALOG_MODE_CHECKOUT2:
		s_mode = STORE_CHECKOUT_PLUGIN_MODE_CHECKOUT;
		break;
	case SCE_STORE_CHECKOUT_DIALOG_MODE_DOWNLOAD:
	case SCE_STORE_CHECKOUT_DIALOG_MODE_DOWNLOAD2:
		s_mode = STORE_CHECKOUT_PLUGIN_MODE_DOWNLOAD;
		break;
	case SCE_STORE_CHECKOUT_DIALOG_MODE_APPLY:
	case SCE_STORE_CHECKOUT_DIALOG_MODE_INSTALL_AND_UPGRADE:
		s_mode = STORE_CHECKOUT_PLUGIN_MODE_APPLY;
		break;
	case 6:
		s_mode = STORE_CHECKOUT_PLUGIN_MODE_SYSTEM1;
		break;
	case 7:
		s_mode = STORE_CHECKOUT_PLUGIN_MODE_SYSTEM2;
		break;
	case 8:
		s_mode = STORE_CHECKOUT_PLUGIN_MODE_SYSTEM3;
		break;
	default:
		s_mode = STORE_CHECKOUT_PLUGIN_MODE_NONE;
		break;
	}
	Plugin::InitParam init;

	init.arguments[string("ctx_id")] = Plugin::Argument(work->param.ctxId);

	sce_paf_memset(&s_param, 0, sizeof(s_param));
	s_param.sdkVersion = work->param.sdkVersion;
	if (work->param.mode == SCE_STORE_CHECKOUT_DIALOG_MODE_CHECKOUT2 ||
	    work->param.mode == SCE_STORE_CHECKOUT_DIALOG_MODE_DOWNLOAD2)
		s_param.flags |= STORE_CHECKOUT_PLUGIN_FLAG_MODE2;
	else if (work->param.mode == SCE_STORE_CHECKOUT_DIALOG_MODE_INSTALL_AND_UPGRADE)
		s_param.flags |= STORE_CHECKOUT_PLUGIN_FLAG_INSTALL;
	s_param.type = STORE_CHECKOUT_PLUGIN_PARAM_TYPE_DIALOG;
	for (i = 0; i < work->param.skuNum; i++) {
		if (i >= STORE_CHECKOUT_DIALOG_SKU_NUM_MAX)
			break;
		if (work->param.sdkVersion < CHECKOUT_DIALOG_SDK_VERSION_SKIP_EMPTY_SKU)
			s_param.skuIds.push_back(string((const char *)work->skuIds[i]));
		else if (work->skuIds[i][0] != '\0')
			s_param.skuIds.push_back(string((const char *)work->skuIds[i]));
	}
	s_param.skuNum = work->param.skuNum;
	s_param.serviceId = string((const char *)work->serviceId);

	init.name = "store_checkout_plugin";
	init.resource_file = "vs0:data/external/cdlg/store_checkout_plugin.rco";
	init.caller_name = "store_checkout_dialog_plugin";
	init.module_file = "vs0:sys/external/store_checkout_plugin.suprx";
	init.module_interface_version = 1;
	init.module_option = 1;
	Plugin::LoadAsync(init, checkoutDialogLoadFinish, 0);
	common::MainThreadCallList::Register(checkoutDialogUpdate, plugin);
}

static void checkoutDialogPluginInit(Plugin *plugin)
{
}

static void checkoutDialogPluginStart(Plugin *plugin)
{
	checkoutDialogStart(plugin);
}

/* unloads store_checkout_plugin */
static void checkoutDialogStop(Plugin *plugin)
{
	common::MainThreadCallList::Unregister(checkoutDialogUpdate, plugin);
	Plugin::UnloadAsync(s_plugin, NULL, 0);
	s_plugin = NULL;
	s_aborted = false;
}

static void checkoutDialogPluginExit(Plugin *plugin)
{
}

static void checkoutDialogSetParam(Plugin::InitParam2 *param)
{
}

/* store_checkout_plugin's result: the dialog's, and the dialog closes */
static void checkoutDialogResult(SceInt32 result, SceInt32 npError)
{
	CommonDialogStoreCheckoutParam *work;

	work = (CommonDialogStoreCheckoutParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_CHECKOUT);
	work->result.npError = npError;
	work->result.result = result;
	SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_CHECKOUT);
}

/* a colour of the caller's, as store_checkout_plugin takes it */
static inline void checkoutDialogColor(float *color, const SceCommonDialogColor *src)
{
	color[0] = src->r / 255.0f;
	color[1] = src->g / 255.0f;
	color[2] = src->b / 255.0f;
	color[3] = src->a / 255.0f;
}

/* store_checkout_plugin is loaded: starts it with the parameter and the caller's colours */
static void checkoutDialogLoadFinish(Plugin *plugin)
{
	CommonDialogStoreCheckoutParam *work;
	StoreCheckoutPluginInterface *iface;
	float bgColor[4];
	float dimmerColor[4];

	work = (CommonDialogStoreCheckoutParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_CHECKOUT);
	s_plugin = plugin;
	iface = (StoreCheckoutPluginInterface *)plugin->GetInterface(STORE_CHECKOUT_PLUGIN_INTERFACE_VERSION);
	if (iface != NULL) {
		iface->start(s_mode, &s_param);
		iface->setResultCallback(checkoutDialogResult);
		if (work->param.commonParam.bgColor != NULL) {
			checkoutDialogColor(bgColor, work->param.commonParam.bgColor);
			iface->setBgColor(bgColor);
		}
		if (work->param.commonParam.dimmerColor != NULL) {
			checkoutDialogColor(dimmerColor, work->param.commonParam.dimmerColor);
			iface->setDimmerColor(dimmerColor);
		}
	}
	SceCommonDialogMain_877BB562();
}
