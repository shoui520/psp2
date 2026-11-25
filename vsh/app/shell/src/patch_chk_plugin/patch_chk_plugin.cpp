/*
 * patch_chk_plugin.cpp: plugin lifecycle hooks and GetInterface(1) table for
 * patch_chk_plugin (0x812AB958-0x812ABB62).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include "patch_chk_internal.h"

using namespace paf;

static PatchChkController *s_controller = NULL;	/* 0x81545B50 */

/* Interface slot 0: start a patch check workflow (0x812ABA24) */
static int patchChkStartCheck(PatchChkStartParam param,
			      PatchChkFinishCallback finishCallback, void *userData)
{
	if (s_controller != NULL)
		s_controller->StartCheck(param, finishCallback, userData);
	return 0;
}

/* Interface slot 1: register state change callback (0x812ABB10) */
static int patchChkSetStateCallback(PatchChkStateCallback callback, void *userData)
{
	return s_controller->SetStateCallback(callback, userData);
}

/* Interface slot 2: clear state change callback (0x812ABB28) */
static void patchChkClearStateCallback(void)
{
	s_controller->ClearStateCallback();
}

/* Interface slot 3: cancel in-progress check/download/changeinfo (0x812ABB3A) */
static void patchChkCancel(void)
{
	s_controller->Cancel();
}

/* Interface slot 4: forward external UI event (0x812ABB4C) */
static void patchChkHandleEvent(int eventId)
{
	s_controller->HandleEvent(eventId);
}

static PatchChkInterface s_patchChkInterface = {	/* 0x81545B54 */
	patchChkStartCheck,
	patchChkSetStateCallback,
	patchChkClearStateCallback,
	patchChkCancel,
	patchChkHandleEvent
};

/* Plugin InitParam2 customization hook (0x812AB958) */
void patchChkPluginSetParam(Plugin::InitParam2 *param)
{
	param->draw_priority = 45;
	param->surface_pool_size = 6;
}

/* Plugin init hook: creates controller, job queues, and exposes interface 1 (0x812AB962) */
void patchChkPluginInit(Plugin *plugin)
{
	patchChkSetPlugin(plugin);
	if (s_controller == NULL)
		s_controller = new PatchChkController();

	PatchChkJobQueueMgr::Create();
	PatchChkJobQueueMgr *mgr = PatchChkJobQueueMgr::GetInstance();
	if (mgr != NULL)
		mgr->InitializeQueues();

	plugin->SetInterface(1, &s_patchChkInterface);
}

/* Plugin start hook (0x812AB9BA) */
void patchChkPluginStart(Plugin *plugin)
{
	(void)plugin;
}

/* Plugin stop hook: detaches active async operations (0x812AB9D0) */
void patchChkPluginStop(Plugin *plugin)
{
	(void)plugin;
	if (s_controller != NULL)
		s_controller->Stop();
}

/* Plugin exit hook: tears down controller and job queues (0x812AB9E6) */
void patchChkPluginExit(Plugin *plugin)
{
	(void)plugin;
	if (s_controller != NULL) {
		s_controller->Cleanup();
		delete s_controller;
		s_controller = NULL;
	}
	patchChkClearPlugin();
	PatchChkJobQueueMgr::Destroy();
}
