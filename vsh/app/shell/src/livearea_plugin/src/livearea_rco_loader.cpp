/*
 * livearea_rco_loader.cpp: livearea_plugin per-title RCO template loader
 * (0x811ACF10-0x811AD166, sti 0x811AD0F4). The file and class names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <plugin.h>
#include <file/localfile.h>
#include "../../livespace_db/lsdb_keys.h"

extern paf::Plugin *g_liveareaPlugin;				/* 0x81566F94 */
extern void *liveareaPageFindByTemplate(const char *name);	/* 0x8119C078 */
extern void liveareaPageInnerOnRcoLoaded(void *inner);		/* 0x811A9CCC */

static void liveareaRcoOnLoad(paf::Plugin *plugin);		/* 0x811AD0D8 */
static void liveareaRcoOnUnload(const char *name);		/* 0x811AD0EC */
static void liveareaRcoNoop(void);				/* 0x811AD0D6 */

/* 0x811ACF10 */
int liveareaLoadRco(const paf::string *rcoName, const paf::string *pluginName, int arg)
{
	paf::Plugin::InitParam param;
	char path[1024];
	sce_paf_snprintf(path, sizeof(path), "%s%s.rco", "vs0:vsh/shell/livearea/", rcoName->c_str());
	if (!paf::LocalFile::Exists(path))
		return 0;

	param.name = *pluginName;
	param.resource_file = path;
	param.caller_name = "__main__";
	*(void **)((char *)&param + 24) = (void *)liveareaRcoNoop;
	*(void **)((char *)&param + 72) = *(void **)((char *)g_liveareaPlugin + 192);
	*(int *)((char *)&param + 84) = arg;
	paf::Plugin::LoadAsync(param, liveareaRcoOnLoad, true);
	return 1;
}

/* 0x811AD0C2 */
void liveareaUnloadRco(const paf::string *pluginName)
{
	paf::Plugin::UnloadAsync(pluginName->c_str(), liveareaRcoOnUnload, true);
}

/* 0x811AD0D6 */
static void liveareaRcoNoop(void)
{
}

/* 0x811AD0D8 */
static void liveareaRcoOnLoad(paf::Plugin *plugin)
{
	void *page = liveareaPageFindByTemplate(*(const char **)((char *)plugin + 4));
	if (page != NULL)
		liveareaPageInnerOnRcoLoaded((char *)page + 48);
}

/* 0x811AD0EC */
static void liveareaRcoOnUnload(const char *name)
{
	liveareaPageFindByTemplate(name);
}
