/*
 * indicator_interface.cpp: indicator_plugin's cache manager, its interface table given to the
 * shell, the jobs that read the Bluetooth configuration and the registry values for the bar, and
 * the bar's left group and its page dots (0x8115F7C4-0x81161548). The file's and the names are
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
#include <std/stdio.h>
#include <widget/core/handler.h>
#include "../../../plugins.h"
#include "../../applauncher_plugin/src/applauncher.h"
#include "../../impose_plugin/src/impose.h"
#include "../../livespace_db/lsdb_keys.h"
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

/* the page dots' template and a dot in it, the box's timer of a refresh after a dot is added or
   removed, and a dot's timer of its destruction (PLACEHOLDER names) */
#define INDICATOR_PAGE_DOT_TEMPLATE	0x09C61366
#define INDICATOR_PAGE_DOT		0x90BE1658
#define INDICATOR_PAGE_DOTS_REFRESH_TIMER	0x1001000
#define INDICATOR_PAGE_DOT_REMOVE_TIMER	0x1001001

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

/* the page dots, while the bar has them (0x81566880) */
static IndicatorPageDots *s_pageDots;

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

static int indicatorGroupOnLayout(ui::Layouter *layouter, ui::Widget *widget, void *data1,
				  void *data2, void *data3);

/* the group's items made in its widget, laid out by indicatorGroupOnLayout: the mobile network's
   with a modem that needs its check, then the Wi-Fi's, the battery's but on the Dolce, and the
   clock's (0x8115FDA0) */
IndicatorLeftGroup::IndicatorLeftGroup(Plugin *plugin, ui::Widget *widget)
	: IndicatorItem(widget), m_root(widget)
{
	if (widget == NULL)
		return;
	widget->SetLayouter(new ui::Layouter(indicatorGroupOnLayout, NULL));

	TelephonyPluginInterface *telephony = (TelephonyPluginInterface *)
		applauncherPluginGetInterface("telephony_plugin", 1);

	if (telephony != NULL && telephony->getModem() != NULL) {
		TelephonyModem *modem = telephony->getModem();

		if (modem->vtable->needsInitialCheck(modem) == 1) {
			IndicatorItem *item = new IndicatorMobileSignal(plugin, m_widget);

			if (item != NULL)
				m_items.push_back(item);
			item = new IndicatorMobileNetwork(plugin, m_widget);
			if (item != NULL)
				m_items.push_back(item);
			item = new IndicatorMobileOperator(plugin, m_widget);
			if (item != NULL)
				m_items.push_back(item);
			item = new IndicatorMobileCall(plugin, m_widget);
			if (item != NULL)
				m_items.push_back(item);
		}
	}

	IndicatorItem *item = new IndicatorWireless(plugin, m_widget);

	if (item != NULL)
		m_items.push_back(item);
	if (!imposeIsDolce()) {
		item = new IndicatorBattery(plugin, m_widget);
		if (item != NULL)
			m_items.push_back(item);
	}
	item = new IndicatorClock(plugin, m_widget);
	if (item != NULL)
		m_items.push_back(item);
}

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
			child->SetPos(pos, new Timer(200.0f), ui::Widget::ANIM_MOVE, 0);
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

void shellWidgetDestroyCB(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static ui::Widget *indicatorAddPageDot(int key);
static void indicatorRemovePageDot(int key);
static void indicatorRefreshPageDots(void);
static void indicatorSetPageDotImage(int key, const intrusive_ptr<ui::TextureSource> *image);
static void indicatorSetPageDotTexture(int key, const intrusive_ptr<graph::Surface> &texture);
static int indicatorPageDotsOnLayout(ui::Layouter *layouter, ui::Widget *widget, void *data1,
				     void *data2, void *data3);
static void indicatorPageDotsOnRefreshTimer(SceInt32 type, ui::Handler *self, ui::Event *e,
					    void *data);

/* the dots laid out by their layouter in the widget; the interface's page dot slots filled
   (0x8116067C) */
IndicatorPageDots::IndicatorPageDots(Plugin *plugin, ui::Widget *widget)
	: m_root(widget), m_plugin(plugin), m_reserved(-1), m_curPage(-1)
{
	if (widget != NULL) {
		m_widget = widget;
		widget->SetLayouter(new ui::Layouter(indicatorPageDotsOnLayout, NULL));
	}
	g_indicatorInterface->AddPageDot = indicatorAddPageDot;
	g_indicatorInterface->RemovePageDot = indicatorRemovePageDot;
	g_indicatorInterface->RefreshPageDots = indicatorRefreshPageDots;
	g_indicatorInterface->SetPageDotImage = indicatorSetPageDotImage;
	g_indicatorInterface->SetPageDotTexture = indicatorSetPageDotTexture;
	s_pageDots = this;
}

/* (0x81160774) */
IndicatorPageDots::~IndicatorPageDots()
{
	s_pageDots = NULL;
}

/* the dots not being destroyed put in the order of topmenu's pages, each in front of the one
   before; the page in view's dot drawn opaque, the others at half (0x8116078E) */
void IndicatorPageDots::Refresh()
{
	ShellPluginInterface *topmenu =
		(ShellPluginInterface *)applauncherPluginGetInterface("topmenu_plugin", 1);

	if (topmenu == NULL)
		return;

	ui::Widget *prev = NULL;
	int index = 0;

	for (list<ui::Widget *>::iterator it = m_widget->m_children.begin();
	     it != m_widget->m_children.end(); ++it, ++index) {
		ui::Widget *child = *it;

		if (child->m_hold_count <= 0) {
			char name[64];

			sce_paf_snprintf(name, sizeof(name), "%d",
					 ((int (*)(int))topmenu->func[TOPMENU_PLUGIN_FUNC_GET_PAGE_KEY])(index));

			ui::Widget *root = m_widget;
			ui::Widget *dot = root->FindChild(IDParam(string(name)), 0);

			if (dot != child && dot != NULL)
				dot->SetForward(prev);
			prev = dot;
		}
	}

	int cur = ((int (*)(void))topmenu->func[TOPMENU_PLUGIN_FUNC_GET_CUR_PAGE])();

	if (cur == m_curPage)
		return;
	for (list<ui::Widget *>::iterator it = m_widget->m_children.begin();
	     it != m_widget->m_children.end(); ++it) {
		ui::Widget *dot = *it;

		if (dot->m_map_t_listener.find(INDICATOR_PAGE_DOT_REMOVE_TIMER) !=
		    dot->m_map_t_listener.end())
			continue;

		int key = sce_paf_strtol(dot->m_name.m_id.c_str(), NULL, 10);
		math::v4 color = dot->GetColor();

		if (cur == key) {
			color.set_w(1.0f);
			dot->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0);
		} else {
			color.set_w(0.5f);
			dot->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0);
		}
	}
	m_curPage = cur;
}

/* (0x811609E4) */
void IndicatorPageDots::OnStatus(IndicatorStatus status)
{
	Refresh();
	UpdateColor(status);
}

/* the first dot given the icons' color, keeping its alpha (0x81160AD8) */
void IndicatorPageDots::UpdateColor(IndicatorStatus status)
{
	list<ui::Widget *>::iterator it = m_widget->m_children.begin();

	if (it != m_widget->m_children.end()) {
		ui::Widget *dot = *it;
		math::v4 color = status.iconColor;

		color.set_w(dot->m_color_alpha);
		dot->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0);
	}
}

/* a dot opened in the widget before its second child and named after the key, grown in from
   nothing; the dots refreshed a moment after (0x81160B46) */
ui::Widget *IndicatorPageDots::AddDot(int key)
{
	Plugin::TemplateOpenParam param;

	param.sibling = m_widget->GetChild(1);
	m_plugin->TemplateOpen(m_widget, IDParam(s_empty_name, INDICATOR_PAGE_DOT_TEMPLATE), param);

	ui::Widget *dot = m_widget->FindChild(IDParam(s_empty_name, INDICATOR_PAGE_DOT), 0);

	if (dot != NULL) {
		char name[64];

		sce_paf_snprintf(name, sizeof(name), "%d", key);
		dot->m_name.SetID(string(name));
		dot->SetMetaAlpha(0.0f, NULL, ui::Widget::ANIM_META_ALPHA, 0);
		dot->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f), NULL, ui::Widget::ANIM_COLOR, 0);

		math::v4 size = *dot->GetSize(0);

		dot->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_SIZE, 0);
		dot->SetSize(size, new Timer(200.0f), ui::Widget::ANIM_SIZE, 0);
		m_widget->SetTimerListener(INDICATOR_PAGE_DOTS_REFRESH_TIMER, 200.0f,
					   new ui::EventCBListener(indicatorPageDotsOnRefreshTimer, this));
	}
	return dot;
}

/* the key's dot faded out and shrunk, and destroyed when that is done; the dots refreshed a
   moment after (0x81160E02) */
void IndicatorPageDots::RemoveDot(int key)
{
	char name[64];

	sce_paf_snprintf(name, sizeof(name), "%d", key);

	ui::Widget *root = m_widget;
	ui::Widget *dot = root->FindChild(IDParam(string(name)), 0);

	if (dot == NULL)
		return;
	dot->SetMetaAlpha(0.0f, new Timer(100.0f), ui::Widget::ANIM_META_ALPHA, 0);
	dot->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), new Timer(100.0f), ui::Widget::ANIM_SIZE, 0);
	dot->SetTimerListener(INDICATOR_PAGE_DOT_REMOVE_TIMER, 100.0f,
			      new ui::EventCBListener(shellWidgetDestroyCB, this));
	m_reserved = -1;
	m_widget->SetTimerListener(INDICATOR_PAGE_DOTS_REFRESH_TIMER, 200.0f,
				   new ui::EventCBListener(indicatorPageDotsOnRefreshTimer, this));
}

/* the interface's page dot slots (0x81161052-0x8116119C) */
static ui::Widget *indicatorAddPageDot(int key)
{
	return s_pageDots->AddDot(key);
}

static void indicatorRemovePageDot(int key)
{
	s_pageDots->RemoveDot(key);
}

static void indicatorRefreshPageDots(void)
{
	s_pageDots->Refresh();
}

/* the key's dot drawn with the image; without one, its image and texture dropped */
static void indicatorSetPageDotImage(int key, const intrusive_ptr<ui::TextureSource> *image)
{
	char name[64];

	sce_paf_snprintf(name, sizeof(name), "%d", key);

	ui::Widget *root = s_pageDots->m_widget;
	ui::Widget *dot = root->FindChild(IDParam(string(name)), 0);

	if (dot == NULL)
		return;
	dot->SetDrawObjMaskBit(image, 0);
	if (image->get() == NULL) {
		{
			intrusive_ptr<ui::TextureSource> none;

			dot->SetDrawObjMaskBit(&none, 0, 0);
		}
		dot->SetTexture(intrusive_ptr<graph::Surface>());
	}
}

static void indicatorSetPageDotTexture(int key, const intrusive_ptr<graph::Surface> &texture)
{
	char name[64];

	sce_paf_snprintf(name, sizeof(name), "%d", key);

	ui::Widget *root = s_pageDots->m_widget;
	ui::Widget *dot = root->FindChild(IDParam(string(name)), 0);

	if (dot != NULL)
		dot->SetTexture(texture);
}

/* the dots put side by side, 4 apart, centred on the widget as topmenu places their pages; a new
   dot (still clear) put in place and faded in, the others moved there (0x8116126A) */
static int indicatorPageDotsOnLayout(ui::Layouter *layouter, ui::Widget *widget, void *data1,
				     void *data2, void *data3)
{
	if (!layouter->IsDirty(0xff) || widget == NULL)
		return 0;

	ShellPluginInterface *topmenu =
		(ShellPluginInterface *)applauncherPluginGetInterface("topmenu_plugin", 1);

	if (topmenu == NULL)
		return 0;

	float dotSize = 28.0f;
	float gap = 4.0f;
	float width = 0.0f;
	int index = 0;

	for (list<ui::Widget *>::iterator it = widget->m_children.begin();
	     it != widget->m_children.end(); ++it, ++index) {
		ui::Widget *dot = *it;

		if (dot->m_map_t_listener.find(INDICATOR_PAGE_DOT_REMOVE_TIMER) !=
		    dot->m_map_t_listener.end())
			continue;
		if (index != 0)
			width = width + gap;
		width = width + dotSize;
	}

	float left = (dotSize - width) * 0.5f;

	for (list<ui::Widget *>::iterator it = widget->m_children.begin();
	     it != widget->m_children.end(); ++it) {
		float time = 200.0f;

		if ((*it)->m_meta_alpha == 0.0f) {
			(*it)->SetMetaAlpha(1.0f, new Timer(200.0f), ui::Widget::ANIM_META_ALPHA, 0);
			time = 0.0f;
		}

		int place = ((int (*)(int))topmenu->func[TOPMENU_PLUGIN_FUNC_GET_PAGE_INDEX])(
			sce_paf_atoi((*it)->m_name.m_id.c_str()));

		if (place >= 0)
			(*it)->SetPos(math::v4(left + (float)place * (dotSize + gap), 0.0f, 0.0f, 0.0f),
				      new Timer(time), ui::Widget::ANIM_MOVE, 0);
	}
	return 0;
}

/* the handler's widget destroyed: a timer's callback (0x811614AC) */
void shellWidgetDestroyCB(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	if (self != NULL)
		static_cast<ui::Widget *>(self)->DestroyWidget();
}

/* the refresh's timer done: the dots refreshed (0x811614BA) */
static void indicatorPageDotsOnRefreshTimer(SceInt32 type, ui::Handler *self, ui::Event *e,
					    void *data)
{
	self->KillTimer((type & 0xFFFF) | 0x1000000);
	if (data != NULL)
		((IndicatorPageDots *)data)->Refresh();
}
