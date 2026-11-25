/*
 * livearea_rco_loader.cpp: livearea_plugin per-title RCO template loader
 * (0x811ACF10-0x811AD166, sti 0x811AD0F4).
 */
#include <std/stdio.h>
#include <file/localfile.h>
#include "livearea_internal.h"

static void liveareaRcoOnLoad(paf::Plugin *plugin);		/* 0x811AD0D8 */
static void liveareaRcoOnUnload(const char *name);		/* 0x811AD0EC */
static void liveareaRcoSetParamNoop(paf::Plugin::InitParam2 *param); /* 0x811AD0D6 */

/* 0x811ACF10 */
int liveareaLoadRco(const paf::string *rcoName, const paf::string *pluginName, int drawPriority)
{
	paf::Plugin::InitParam param;
	char path[1024];
	sce_paf_snprintf(path, sizeof(path), "%s%s.rco", "vs0:vsh/shell/livearea/", rcoName->c_str());
	if (!paf::LocalFile::Exists(path))
		return 0;

	param.name = *pluginName;
	param.resource_file = path;
	param.caller_name = "__main__";
	param.set_param_func = liveareaRcoSetParamNoop;
	param.private_heap = g_liveareaPlugin->m_private_heap;
	param.draw_priority = drawPriority;
	paf::Plugin::LoadAsync(param, liveareaRcoOnLoad, true);
	return 1;
}

/* 0x811AD0C2 */
void liveareaUnloadRco(const paf::string *pluginName)
{
	paf::Plugin::UnloadAsync(pluginName->c_str(), liveareaRcoOnUnload, true);
}

/* 0x811AD0D6 */
static void liveareaRcoSetParamNoop(paf::Plugin::InitParam2 *param)
{
	(void)param;
}

/* 0x811AD0D8 */
static void liveareaRcoOnLoad(paf::Plugin *plugin)
{
	LiveAreaPage *page = liveareaPageFindByTemplate(plugin->GetName());
	if (page != NULL)
		page->m_inner.OnRcoLoaded();
}

/* 0x811AD0EC */
static void liveareaRcoOnUnload(const char *name)
{
	liveareaPageFindByTemplate(name);
}
