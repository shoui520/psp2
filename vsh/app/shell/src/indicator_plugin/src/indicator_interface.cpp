/*
 * indicator_interface.cpp: indicator_plugin's cache manager, its interface table given to the
 * shell, the jobs that read the Bluetooth configuration and the registry values for the bar, and
 * the bar's left group and first items (0x8115F7C4-0x811614D8). The file's and the names are
 * ours (PLACEHOLDER), but for the jobs' classes, named by the names they give their jobs.
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <framework.h>
#include <std/stdlib.h>
#include <std/string>
#include <std/list>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <thread/thread.h>
#include <cache/cache_manager.h>
#include <timer/timer.h>
#include <widget/core/layouter.h>
#include <widget/w_scene.h>
#include <regmgr/regmgr.h>
#include <bt/bt.h>
#include "indicator.h"

using namespace paf;

/* sceBtGetConfiguration while the driver is busy: asked again (PLACEHOLDER name) */
#define INDICATOR_BT_ERROR_BUSY		((int)0x802F1501)
#define INDICATOR_BT_RETRIES		3
#define INDICATOR_BT_RETRY_WAIT		100
/* the configuration's bit for a driver error, read from sceBtGetLastError (PLACEHOLDER) */
#define INDICATOR_BT_CONFIG_ERROR	0x40

/* the flight mode icon's template and the icon in it (PLACEHOLDER names) */
#define INDICATOR_FLIGHT_MODE_TEMPLATE	0xC8B5915B
#define INDICATOR_FLIGHT_MODE_ICON	0xC6F5E77C

/* the plugin's cache manager, made when it starts (8 bytes; PLACEHOLDER name) */
class IndicatorCache {
public:
	IndicatorCache(Plugin *plugin);

	~IndicatorCache()
	{
		m_cache->RequestShutdown();
		delete m_cache;
		g_indicatorCache = NULL;
	}

	cache::CacheManager *m_cache;
};

/* (0x8156686C-0x81566878) */
IndicatorCache *g_indicatorCache;
memory::HeapAllocator *g_indicatorHeap;
IndicatorPluginInterface *g_indicatorInterface;
Plugin *g_indicatorPlugin;

/* (0x81544960); the bar and its items fill it */
static IndicatorPluginInterface s_interface = { NULL };

/* "indicator_plugin_CacheManager", on paf's decode heap and the plugin's surface pool
   (0x8115F7C4) */
__attribute__((noinline))
IndicatorCache::IndicatorCache(Plugin *plugin)
{
	g_indicatorCache = this;

	cache::CacheManager::Params param;

	param.name = string("indicator_plugin_CacheManager");
	param.longlife_heap = Framework::Instance()->GetDecodeHeap();
	param.instant_heap = Framework::Instance()->GetDecodeHeap();
	param.cache_instance_heap = NULL;
	param.surface_pool = plugin->m_surface_pool;
	param.cache_longlife_heap = false;
	param.image_cache_divide_thread = true;

	m_cache = new cache::CacheManager(&param);
}

/* (0x8115F8DA) */
void indicatorCacheCreate(Plugin *plugin)
{
	new IndicatorCache(plugin);
}

/* (0x8115F8F0) */
void indicatorCacheDestroy(void)
{
	if (g_indicatorCache != NULL)
		delete g_indicatorCache;
}

/* an image of a file made in the cache, on the queue given (0x8115F924) */
int indicatorCacheCreateFile(const string &path, intrusive_ptr<cache::CacheImageInterface> &image,
			     const cache::CacheImage::Option *option, job::JobQueue *queue)
{
	return g_indicatorCache->m_cache->CreateCache(path, image, option, queue, NULL);
}

/* an image of a plugin's texture made in the cache (0x8115F94E) */
int indicatorCacheCreateTexture(Plugin *plugin, IDParam &id,
				intrusive_ptr<cache::CacheImageInterface> &image,
				const cache::CacheImage::Option *option, job::JobQueue *queue)
{
	return g_indicatorCache->m_cache->CreateCache(plugin, id, image, option, queue);
}

/* room made in the surface pool (0x8115F976) */
void indicatorCacheMakeSpace(void)
{
	g_indicatorCache->m_cache->MakeSpaceSurfacePool();
}

/* (0x8115F98C) */
void indicatorInterfaceInit(void)
{
	g_indicatorHeap = g_indicatorPlugin->m_private_heap;
}

/* nothing to do when the plugin exits (the empty function at 0x814098DC) */
void indicatorInterfaceExit(void)
{
}

void indicatorExitReserved(void)
{
}

/* (0x8115F9A8) */
void indicatorInterfaceRegister(Plugin *plugin)
{
	g_indicatorPlugin = plugin;
	g_indicatorInterface = &s_interface;
	plugin->SetInterface(1, &s_interface);
}

/* (0x8115F9D0) */
ui::Widget *indicatorFindWidget(const IDParam &page, const IDParam &id)
{
	ui::Scene *root = g_indicatorPlugin->PageRoot(page);

	if (root == NULL)
		return NULL;
	return root->FindChild(id, 0);
}

/* the Bluetooth configuration read, given to the callback when the job is done (40 bytes) */
class JobGetBtConfig : public job::JobItem {
public:
	JobGetBtConfig(IndicatorBtConfigCallback callback, void *data)
		: job::JobItem("JobGetBtConfig", NULL), m_config(0), m_callback(callback),
		  m_data(data)
	{
	}

	virtual ~JobGetBtConfig()
	{
	}

	virtual int Run();
	virtual void Finish(int result);

	int m_config;		/* the configuration, or the error reading it */
	IndicatorBtConfigCallback m_callback;
	void *m_data;
};

/* (0x8115F9F8) */
common::SharedPtr<job::JobItem> indicatorBtConfigJobStart(IndicatorBtConfigCallback callback,
							  void *data)
{
	job::JobQueue *queue = job::JobQueue::default_queue;
	common::SharedPtr<job::JobItem> job(new JobGetBtConfig(callback, data));

	queue->Enqueue(job);
	return job;
}

/* the configuration read, again while the driver is busy; its error posted (0x8115FABE) */
int JobGetBtConfig::Run()
{
	int ret = -1;

	for (unsigned int i = 0; i < INDICATOR_BT_RETRIES; i++) {
		int config = sceBtGetConfiguration();

		if (config == INDICATOR_BT_ERROR_BUSY) {
			thread::Sleep(INDICATOR_BT_RETRY_WAIT);
			continue;
		}
		if (config < 0 || (config & INDICATOR_BT_CONFIG_ERROR) != 0) {
			if (config >= 0)
				config = sceBtGetLastError();
			m_config = config;
			indicatorPostError(config);
		} else {
			m_config = config;
		}
		ret = 0;
		break;
	}
	return ret;
}

/* (0x8115FB0E) */
void JobGetBtConfig::Finish(int result)
{
	if (m_callback != NULL) {
		m_callback(result, m_config, m_data);
		m_callback = NULL;
	}
}

/* the registry values read, given to the callback when the job is done (76 bytes) */
class JobGetRegistryInfo : public job::JobItem {
public:
	JobGetRegistryInfo(const list<IndicatorRegKey> &keys, IndicatorRegInfoCallback callback,
			   void *data);

	virtual ~JobGetRegistryInfo()
	{
	}

	virtual int Run();
	virtual void Finish(int result);

	string m_reserved1;	/* not read */
	string m_reserved2;
	int m_state;		/* 0 when run; not read (PLACEHOLDER) */
	IndicatorRegInfoCallback m_callback;
	void *m_data;
	list<IndicatorRegKey> m_keys;
};

/* (0x8115FB28) */
__attribute__((noinline))
JobGetRegistryInfo::JobGetRegistryInfo(const list<IndicatorRegKey> &keys,
				       IndicatorRegInfoCallback callback, void *data)
	: job::JobItem("JobGetRegistryInfo", NULL), m_state(0), m_callback(callback), m_data(data)
{
	m_keys = keys;
}

/* (0x8115FBC6) */
common::SharedPtr<job::JobItem> indicatorRegInfoJobStart(const list<IndicatorRegKey> &keys,
							 IndicatorRegInfoCallback callback,
							 void *data)
{
	job::JobQueue *queue = job::JobQueue::default_queue;
	common::SharedPtr<job::JobItem> job(new JobGetRegistryInfo(keys, callback, data));

	queue->Enqueue(job);
	return job;
}

/* each key's integer read, -1 for one that cannot be (0x8115FCEE) */
int JobGetRegistryInfo::Run()
{
	m_state = 0;
	for (list<IndicatorRegKey>::iterator it = m_keys.begin(); it != m_keys.end(); ++it) {
		RegMgrKeyInfo info;

		info.size = sizeof(info);
		info.name = it->name.c_str();
		it->result = sceRegMgrGetKeysInfo(it->path.c_str(), &info, 1);
		if (it->result != 0) {
			sce_paf_printf_dummy();
			it->value = -1;
		} else if (info.type == 0) {
			int value = 0;

			it->result = sceRegMgrGetKeyInt(it->path.c_str(), it->name.c_str(), &value);
			if (it->result != 0) {
				sce_paf_printf_dummy();
				it->value = -1;
			} else {
				it->value = value;
			}
		}
	}
	return 0;
}

/* (0x8115FD84) */
void JobGetRegistryInfo::Finish(int result)
{
	if (m_callback != NULL) {
		m_callback(m_keys, m_data);
		m_callback = NULL;
	}
}

/* PLACEHOLDER: the left group's constructor (0x8115FDA0), written with its items' classes */

/* its items deleted, the first one first (0x81160084) */
IndicatorLeftGroup::~IndicatorLeftGroup()
{
	while (m_items.size() != 0) {
		IndicatorItem *item = m_items[0];

		m_items.erase(m_items.begin());
		if (item != NULL)
			delete item;
	}
}

/* each item colored and told the status (0x8116011A) */
void IndicatorLeftGroup::OnStatus(IndicatorStatus status)
{
	for (vector<IndicatorItem *>::iterator it = m_items.begin(); it != m_items.end(); ++it) {
		(*it)->UpdateColor(status);
		(*it)->OnStatus(status);
	}
}

/* the group's widgets put one after the other from the left, 4 apart, the empty ones taking no
   room (0x81160310) */
static int indicatorGroupOnLayout(ui::Layouter *layouter, ui::Widget *widget, void *data1,
				  void *data2, void *data3)
{
	if (widget != NULL && layouter->IsDirty(0xff)) {
		float x = 0.0f;

		for (list<ui::Widget *>::iterator it = widget->m_children.begin();
		     it != widget->m_children.end(); ++it) {
			ui::Widget *child = *it;
			math::v4 size = *child->GetSize(1);
			math::v4 pos = child->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			pos.set_x(x + size.extract_x() * 0.5f);
			child->SetPos(pos, new Timer(0.0f), ui::Widget::ANIM_MOVE, 0);
			if (size.extract_x() > 0.0f)
				x = size.extract_x() + 4.0f + x;
		}
	}
	return 0;
}

/* the icon's template opened in the widget, and the icon found in it (0x81160468) */
IndicatorFlightModeIcon::IndicatorFlightModeIcon(Plugin *plugin, ui::Widget *widget)
	: m_root(widget)
{
	if (widget != NULL) {
		bool opened = plugin->TemplateOpen(widget,
						   IDParam(s_empty_name, INDICATOR_FLIGHT_MODE_TEMPLATE),
						   Plugin::TemplateOpenParam()) == 0;

		if (opened)
			m_widget = widget->FindChild(IDParam(s_empty_name, INDICATOR_FLIGHT_MODE_ICON), 0);
	}
}

/* (0x8116058A) */
void IndicatorFlightModeIcon::OnStatus(IndicatorStatus status)
{
	UpdateColor(status);
}
