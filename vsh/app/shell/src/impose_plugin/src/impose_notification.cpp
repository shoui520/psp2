/*
 * impose_notification.cpp: impose_plugin's notices, shown one at a time at the screen's corner,
 * queued by priority, with their icons loaded through the plugin's cache manager
 * (0x81148088-0x81149A7C). The file's and the names are ours (PLACEHOLDER), after its
 * "notificationPush" and "notificationCancel".
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <framework.h>
#include <std/string>
#include <std/string.h>
#include <std/vector>
#include <std/memory>
#include <versatileparam/idparam.h>
#include <timer/timer.h>
#include <thread/thread.h>
#include <common/char_encoding.h>
#include <cache/cache_manager.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_text.h>
#include <appmgr/appmgr.h>
#include "../../livespace_db/lsdb_keys.h"
#include "impose.h"

PAF_VECTOR_INLINE_DTOR(ImposeNotificationItem *)

using namespace paf;

/* the notice's template and widgets (PLACEHOLDER names) */
#define IMPOSE_NOTIFICATION_TEMPLATE		0xD203C46E
#define IMPOSE_NOTIFICATION_ROOT		0x1986D1BF
#define IMPOSE_NOTIFICATION_FRAME		0x27EFA476
#define IMPOSE_NOTIFICATION_ICON		0xC3407951
#define IMPOSE_NOTIFICATION_TEXT		0x7B1A3B96
/* the plugin's notice icon, the common texture laid over an image icon, and the icon kept in
   the cache (PLACEHOLDER names) */
#define IMPOSE_NOTIFICATION_TEX_DEFAULT		0x2617A32D
#define IMPOSE_NOTIFICATION_TEX_FRAME		0x45BE9610
#define IMPOSE_NOTIFICATION_TEX_CACHED		0x0E22C099
/* the timer the shown notice ends by */
#define IMPOSE_NOTIFICATION_TIMER		0x100

/* the most notices kept waiting */
#define IMPOSE_NOTIFICATION_MAX_WAITING		16

/* the shown one's time, its time once more, and its fade (ms) */
#define IMPOSE_NOTIFICATION_TIME		2200.0f
#define IMPOSE_NOTIFICATION_TIME_MORE		3000.0f
#define IMPOSE_NOTIFICATION_FADE		200.0f

/* what the callback is told (PLACEHOLDER names) */
#define IMPOSE_NOTIFICATION_ENDED		0
#define IMPOSE_NOTIFICATION_CANCELLED		2
#define IMPOSE_NOTIFICATION_DROPPED		3

/* (0x81564D68) */
ImposeNotification *g_imposeNotification;

/* shows a widget over the timer's time (inline in paf's callers) */
static inline void imposeNotificationShowWidget(ui::Widget *widget, Timer *timer)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, NULL);
}

/* hides a widget over the timer's time (inline in paf's callers) */
static inline void imposeNotificationHideWidget(ui::Widget *widget, Timer *timer)
{
	float time = timer != NULL ? timer->GetTotalTime() : 0.0f;

	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(time, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

/* an icon's load option: ten tries, the size it is scaled to (inline in Sony's build; the
   rectangle is zero from its constructor) */
static inline void imposeNotificationIconOption(cache::CacheImage::Option *opt, int32_t width,
						int32_t height)
{
	opt->auto_delete = true;
	opt->retry = true;
	opt->retry_count = 10;
	opt->reserved = 1;
	opt->read_size = 0;
	opt->read_offset = 0;
	opt->max_size_kb = 0;
	opt->open_func = NULL;
	opt->open_userdata = NULL;
	opt->scale.w = width;
	opt->scale.h = height;
	opt->resize_type = image::ImageResizeType_NearestNeighbor;
	opt->direct_load = false;
	opt->image_order = ImageOrder_NONE;
	opt->image_order_param = 0;
	opt->check_image = NULL;
	opt->check_image_userdata = NULL;
	opt->surface_flag = false;
}

/* (0x81148088) */
ImposeNotificationItem::ImposeNotificationItem(const ImposeNotice &notice)
{
	m_name = notice.name;
	m_priority = notice.priority;
	m_reserved1 = notice.reserved1;
	if (notice.pluginName != NULL)
		m_pluginName = notice.pluginName;
	m_icon.m_id = notice.iconId;
	m_icon.m_idhash = notice.iconHash;
	m_iconPath = notice.iconPath;
	m_iconMode = notice.iconMode;
	m_message = notice.message;
	m_callback = notice.callback;
	m_callbackData = notice.callbackData;
	m_flags = notice.flags;
}

/* (0x811481A2) */
ImposeNotificationItem::~ImposeNotificationItem()
{
}

static int imposeNotificationShowNotice(ImposeNotice *notice);
static int imposeNotificationRemoveNotice(const char *name);
static void imposeNotificationOnTimeout(SceInt32 type, ui::Handler *self, ui::Event *e,
					void *data);
static void imposeNotificationOnTimeoutEnd(SceInt32 type, ui::Handler *self, ui::Event *e,
					   void *data);
static void imposeNotificationOnLayout(SceInt32 type, ui::Handler *self, ui::Event *e,
				       void *data);
static void imposeNotificationOnIconLoaded(SceInt32 type, ui::Handler *self, ui::Event *e,
					   void *data);
static void imposeNotificationOnIconFailed(SceInt32 type, ui::Handler *self, ui::Event *e,
					   void *data);

/* (0x81148260) */
ImposeNotification::ImposeNotification(Plugin *plugin)
	: m_plugin(plugin), m_current(NULL), m_widget(NULL), m_reserved(0), m_cache(NULL)
{
	g_imposeNotification = this;
	m_items.reserve(IMPOSE_NOTIFICATION_MAX_WAITING);

	cache::CacheManager::Params param;

	param.name = string("impose_plugin_CacheManager");
	param.longlife_heap = Framework::Instance()->GetDecodeHeap();
	param.instant_heap = Framework::Instance()->GetDecodeHeap();
	param.cache_instance_heap = NULL;
	param.surface_pool = plugin->m_surface_pool;
	param.cache_longlife_heap = false;
	param.image_cache_divide_thread = true;

	m_cache = new cache::CacheManager(&param);

	g_imposeInterface->showNotice = imposeNotificationShowNotice;
	g_imposeInterface->removeNotice = imposeNotificationRemoveNotice;
}

/* (0x811483AE) */
ImposeNotification::~ImposeNotification()
{
	if (m_cache != NULL) {
		m_cache->RequestShutdown();
		delete m_cache;
		m_cache = NULL;
	}
	g_imposeNotification = NULL;
}

/* the shown notice deleted, and the first one waiting made from the template and shown (0x811483EA) */
void ImposeNotification::ShowNext()
{
	if (m_current != NULL) {
		delete m_current;
		m_current = NULL;
	}
	if (m_items.size() == 0)
		return;

	ui::Scene *page = g_impose != NULL ? g_impose->m_page : NULL;

	m_current = m_items[0];
	m_items.erase(m_items.begin());
	m_plugin->TemplateOpen(page, IDParam(IMPOSE_NOTIFICATION_TEMPLATE),
			       Plugin::TemplateOpenParam());

	m_widget = page->FindChild(IDParam(IMPOSE_NOTIFICATION_ROOT), 0);
	m_widget->m_hit_detect_mode = 2;
	m_widget->m_hit_detect_b_mode = 2;
	ScePafWidget_66847A11(m_widget, true);

	ui::Widget *frame = m_widget->FindChild(IDParam(IMPOSE_NOTIFICATION_FRAME), 0);

	frame->m_hit_detect_mode = 2;
	frame->m_hit_detect_b_mode = 2;

	ui::Widget *icon = m_widget->FindChild(IDParam(IMPOSE_NOTIFICATION_ICON), 0);

	icon->m_hit_detect_mode = 2;
	icon->m_hit_detect_b_mode = 2;

	if (m_current->m_iconPath.length() == 0) {
		/* a texture of the plugin named; the ones listed kept in the cache */
		Plugin *plugin = g_imposePlugin;
		static IDParam s_cached[] = { IDParam("", IMPOSE_NOTIFICATION_TEX_CACHED) };
		bool cached;
		uint32_t i;

		for (i = 0; i < sizeof(s_cached) / sizeof(s_cached[0]); i++) {
			if (m_current->m_icon.m_idhash == s_cached[i].m_idhash) {
				cached = true;
				goto found;
			}
		}
		cached = false;
found:
		if (m_current->m_pluginName.length() != 0) {
			plugin = Plugin::Find(m_current->m_pluginName.c_str());
			cached = false;
		}
		if (plugin != NULL) {
			if (cached) {
				intrusive_ptr<cache::CacheImageInterface> image;
				cache::CacheImage::Option opt;

				imposeNotificationIconOption(&opt, 0, 0);
				if (m_cache->FindCache(plugin, m_current->m_icon, image, &opt) != 0 ||
				    image.get() == NULL || m_cache->GetCacheCount() > 1) {
					m_cache->MakeSpaceSurfacePool();
					m_cache->CreateCache(plugin, m_current->m_icon, image, &opt,
							     NULL);
				}
				if (image.get() != NULL) {
					icon->AddEventListener(ui::Handler::CB_STATE_READY_CACHEIMAGE,
						new ui::EventCBListener(
							imposeNotificationOnIconLoaded, NULL), true);
					icon->AddEventListener(ui::Handler::CB_COMMAND,
						new ui::EventCBListener(
							imposeNotificationOnIconFailed, NULL), true);
					imposeNotificationHideWidget(icon, NULL);
					icon->SetDrawObjMaskBit(&image, 0);
				}
			} else {
				intrusive_ptr<graph::Surface> tex = plugin->GetTexture(m_current->m_icon);

				if (tex.get() == NULL)
					tex = g_imposePlugin->GetTexture(
						IDParam(IMPOSE_NOTIFICATION_TEX_DEFAULT));
				icon->SetTexture(tex);
			}
		}
	} else {
		/* an image file: loaded once more when it is not cached alone */
		intrusive_ptr<cache::CacheImageInterface> image;
		cache::CacheImage::Option opt;

		imposeNotificationIconOption(&opt, 64, 64);
		if (m_cache->FindCache(string(m_current->m_iconPath.c_str()), image, &opt) != 0 ||
		    image.get() == NULL || m_cache->GetCacheCount() > 1) {
			m_cache->MakeSpaceSurfacePool();
			m_cache->CreateCache(string(m_current->m_iconPath.c_str()), image, &opt, NULL,
					     NULL);
		}
		m_cache->CreateCache(string(m_current->m_iconPath.c_str()), image, &opt, NULL, NULL);
		if (image.get() != NULL) {
			icon->AddEventListener(ui::Handler::CB_STATE_READY_CACHEIMAGE,
				new ui::EventCBListener(imposeNotificationOnIconLoaded, NULL), true);
			icon->AddEventListener(ui::Handler::CB_COMMAND,
				new ui::EventCBListener(imposeNotificationOnIconFailed, NULL), true);
			imposeNotificationHideWidget(icon, NULL);
			icon->SetDrawObjMaskBit(&image, 0);
			if (m_current->m_iconMode == 1)
				icon->SetTexture(Framework::Instance()->GetCommonTexture(
					IDParam(IMPOSE_NOTIFICATION_TEX_FRAME)), 0, 1);
		}
	}

	ui::Widget *text = m_widget->FindChild(IDParam(IMPOSE_NOTIFICATION_TEXT), 0);

	if (text != NULL) {
		text->m_hit_detect_mode = 2;
		text->m_hit_detect_b_mode = 2;
		text->SetString(m_current->m_message);
		text->AddEventListener(ui::Handler::CB_STATE_READY,
				       new ui::EventCBListener(imposeNotificationOnLayout, NULL), true);
	}
	imposeNotificationHideWidget(m_widget, NULL);
}

/* the shown notice faded out and deleted, and the next one shown (0x81148D44) */
void ImposeNotification::Close()
{
	if (m_widget != NULL) {
		m_widget->KillTimeoutEvent(IMPOSE_NOTIFICATION_TIMER);
		imposeNotificationHideWidget(m_widget, new Timer(IMPOSE_NOTIFICATION_FADE));
		ScePafWidget_5A9793B0(m_widget, IMPOSE_NOTIFICATION_FADE);
		m_widget->m_name.SetID(string("delayDestory"));
		m_widget = NULL;
	}
	g_imposeNotification->ShowNext();
}

/*
 * the notice queued by its priority; one of negative priority shown at once (the shown one
 * waiting again if it is one too), or the shown one kept longer if it is the same (0x81148EBA)
 */
void ImposeNotification::Push(ImposeNotificationItem *item)
{
	if (item->m_priority < 0) {
		if (m_current != NULL) {
			if (m_current->m_priority < 0 && m_current->m_name == item->m_name &&
			    m_current->m_message == item->m_message &&
			    m_current->m_matchId.m_idhash == item->m_matchId.m_idhash) {
				m_widget->SetTimeoutEvent(IMPOSE_NOTIFICATION_TIMER,
							  IMPOSE_NOTIFICATION_TIME,
							  imposeNotificationOnTimeout, NULL);
				delete item;
				return;
			}
			if (m_current->m_priority >= 0)
				delete m_current;
			else
				m_items.insert(m_items.begin(), m_current);
			m_current = NULL;
			Close();
		}
		m_items.insert(m_items.begin(), item);
	} else {
		vector<ImposeNotificationItem *>::iterator it;

		for (it = m_items.begin(); it != m_items.end(); ++it) {
			if (item->m_priority < (*it)->m_priority)
				break;
		}
		m_items.insert(it, item);
	}
	while (m_items.size() > IMPOSE_NOTIFICATION_MAX_WAITING) {
		ImposeNotificationItem *last = m_items.back();

		if (last->m_callback != NULL)
			last->m_callback(IMPOSE_NOTIFICATION_DROPPED, last->m_callbackData);
		m_items.pop_back();
		delete last;
	}
	if (m_widget == NULL)
		ShowNext();
}

/* (0x81149058) */
static void imposeNotificationPush(void *arg)
{
	g_imposeNotification->Push((ImposeNotificationItem *)arg);
}

/* the notices of the name taken down (0x8114906E) */
void ImposeNotification::Cancel(string *name)
{
	const char *target = name->c_str();
	vector<ImposeNotificationItem *>::iterator it = m_items.begin();

	while (it != m_items.end()) {
		if ((*it)->m_name == target) {
			if ((*it)->m_callback != NULL)
				(*it)->m_callback(IMPOSE_NOTIFICATION_CANCELLED,
						  (*it)->m_callbackData);
			delete *it;
			it = m_items.erase(it);
		} else {
			++it;
		}
	}
	if (m_current != NULL && m_current->m_name == target) {
		if (m_current->m_callback != NULL)
			m_current->m_callback(IMPOSE_NOTIFICATION_CANCELLED,
					      m_current->m_callbackData);
		Close();
	}
	delete name;
}

/* (0x8114919A) */
static void imposeNotificationCancel(void *arg)
{
	g_imposeNotification->Cancel((string *)arg);
}

/* the shown notice's time over: it ends, or it is kept once more (0x811491B0) */
void ImposeNotification::OnTimeout(void *data)
{
	ImposeNotificationItem *current = m_current;

	if ((current->m_flags & IMPOSE_NOTICE_FLAG_TIMEOUT) != 0 ||
	    (m_items.size() != 0 && (current->m_flags & IMPOSE_NOTICE_FLAG_STAY) == 0)) {
		if (current->m_callback != NULL)
			current->m_callback(IMPOSE_NOTIFICATION_ENDED, current->m_callbackData);
		delete m_current;
		m_current = NULL;
		Close();
	} else {
		m_widget->SetTimeoutEvent(IMPOSE_NOTIFICATION_TIMER, IMPOSE_NOTIFICATION_TIME_MORE,
					  imposeNotificationOnTimeoutEnd, NULL);
	}
}

/* (0x8114923E) */
static void imposeNotificationOnTimeout(SceInt32 type, ui::Handler *self, ui::Event *e,
					void *data)
{
	g_imposeNotification->OnTimeout(data);
}

/* the time once more over: the notice ends (0x81149254) */
static void imposeNotificationOnTimeoutEnd(SceInt32 type, ui::Handler *self, ui::Event *e,
					   void *data)
{
	ImposeNotification *notification = g_imposeNotification;
	ImposeNotificationItem *current = notification->m_current;

	if (current->m_callback != NULL)
		current->m_callback(IMPOSE_NOTIFICATION_ENDED, current->m_callbackData);
	delete notification->m_current;
	notification->m_current = NULL;
	notification->Close();
}

/*
 * the text laid out: the notice sized to its lines, put at the corner of each resolution, and
 * shown for its time (0x81149286)
 */
static void imposeNotificationOnLayout(SceInt32 type, ui::Handler *self, ui::Event *e,
				       void *data)
{
	ui::Text *text = NULL;

	if (self != NULL && ((ui::Widget *)self)->IsInherit(ui::Text::TypeName()))
		text = (ui::Text *)self;

	math::v4 size(410.0f, 90.0f, 0.0f, 0.0f);

	if (text != NULL) {
		text->GetSize(0);

		uint32_t lines = 0;

		text->GetLineCount(lines);
		if (lines == 1) {
			math::v4 pos = text->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			pos.set_x(-54.0f);
			text->SetPos(pos, NULL, 0x10000);
		} else {
			math::v4 pos = text->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			pos.set_x(-40.0f);
			text->SetPos(pos, NULL, 0x10000);
		}

		float height = (float)lines * 30.0f + 30.0f;

		if (height < 90.0f)
			size.set_y(90.0f);
		else
			size.set_y(height);

		wstring str(L"");

		text->GetString(str);
		if (str.length() == 0)
			size.set_x(90.0f);
	}

	g_imposeNotification->m_widget->SetSize(size, NULL, 0x10004);

	uint32_t num = g_imposeNotification->m_widget->m_pos_num;

	if (num != 0) {
		ui::ResolutionData *pos = new ui::ResolutionData[num];

		if (pos != NULL) {
			sce_paf_memcpy(pos, g_imposeNotification->m_widget->m_pos_array,
				       g_imposeNotification->m_widget->m_pos_num *
					       sizeof(ui::ResolutionData));
			for (uint32_t i = 0; i < num; i++) {
				switch (pos[i].m_id) {
				case 16:
					pos[i].m_value[0] = size.extract_x() * 0.5f + 9.0f - 272.0f;
					pos[i].m_value[1] = 480.0f - (size.extract_y() * 0.5f + 35.0f);
					break;
				case 15:
					pos[i].m_value[0] = size.extract_x() * 0.5f + 9.0f - 480.0f;
					pos[i].m_value[1] = 272.0f - (size.extract_y() * 0.5f + 35.0f);
					break;
				}
			}
			g_imposeNotification->m_widget->SetPos(pos, num, NULL, 0x10000, 0, 0.0f,
							       NULL, NULL);
			if (pos != NULL)
				delete[] pos;
		}
	}
	imposeNotificationShowWidget(g_imposeNotification->m_widget,
				     new Timer(IMPOSE_NOTIFICATION_FADE));
	g_imposeNotification->m_widget->SetTimeoutEvent(IMPOSE_NOTIFICATION_TIMER,
							IMPOSE_NOTIFICATION_TIME,
							imposeNotificationOnTimeout, NULL);
}

/* the icon's image loaded (0x811496CE) */
static void imposeNotificationOnIconLoaded(SceInt32 type, ui::Handler *self, ui::Event *e,
					   void *data)
{
	ui::Widget *icon = (ui::Widget *)self;

	if (icon != NULL)
		imposeNotificationShowWidget(icon, NULL);
}

/* the icon's image not loaded: the icon hidden, and the pool's space made (0x81149712) */
static void imposeNotificationOnIconFailed(SceInt32 type, ui::Handler *self, ui::Event *e,
					   void *data)
{
	ui::Widget *icon = (ui::Widget *)self;

	if (icon != NULL) {
		icon->SetTexture(intrusive_ptr<graph::Surface>());
		imposeNotificationHideWidget(icon, NULL);
	}
	g_imposeNotification->m_cache->MakeSpaceSurfacePool();
}

/* the notice queued from the main thread (0x811497A0) */
int ImposeNotification::Show(ImposeNotice *notice)
{
	thread::CallQueue::main_thread_callqueue.Enqueue("notificationPush",
							 imposeNotificationPush,
							 new ImposeNotificationItem(*notice));
	return 1;
}

/* each frame: a notice the application manager sends shown (0x811497DA) */
void ImposeNotification::Update()
{
	AppmgrNotificationRequest request;

	if (sceAppMgrReceiveNotificationRequestForShell(&request) != 0)
		return;

	ImposeNotice notice;
	wstring text;

	common::Utf8ToUtf16(string((const char *)request.payload), &text, 0);
	notice.name = "sys-notify";
	notice.message = text.c_str();
	notice.iconHash = IMPOSE_NOTIFICATION_TEX_DEFAULT;
	thread::CallQueue::main_thread_callqueue.Enqueue("notificationPush",
							 imposeNotificationPush,
							 new ImposeNotificationItem(notice));
}

/* the interface's showNotice (0x81149918) */
static int imposeNotificationShowNotice(ImposeNotice *notice)
{
	thread::CallQueue::main_thread_callqueue.Enqueue("notificationPush",
							 imposeNotificationPush,
							 new ImposeNotificationItem(*notice));
	return 1;
}

/* the interface's removeNotice (0x81149952) */
static int imposeNotificationRemoveNotice(const char *name)
{
	thread::CallQueue::main_thread_callqueue.Enqueue("notificationCancel",
							 imposeNotificationCancel,
							 new string(name));
	return 1;
}

/* the cache's surface pool made room in, when more than one image is cached (0x811499A6) */
void ImposeNotification::MakeSpaceIfUsed()
{
	if (g_imposeNotification->m_cache->GetCacheCount() > 1)
		g_imposeNotification->m_cache->MakeSpaceSurfacePool();
}

/* (0x811499CE) */
void ImposeNotification::MakeSpace()
{
	g_imposeNotification->m_cache->MakeSpaceSurfacePool();
}
