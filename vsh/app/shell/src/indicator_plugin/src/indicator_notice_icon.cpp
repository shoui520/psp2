/*
 * indicator_notice_icon.cpp: the icon the shell puts on the bar through the interface: its
 * frame risen into the bar and dropped out of it, the image faded in once its texture is set,
 * the parental lock's image for content the lock holds back, and the frame's texture lit while
 * the icon is active (0x81180FD8-0x81182050). The file's and all names here are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/string.h>
#include <std/memory>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <cache/cache_image.h>
#include <graphics/surface/surface.h>
#include <regmgr/regmgr.h>
#include "../../livespace_db/lsdb_keys.h"
#include "indicator.h"

using namespace paf;

/* the icon's template, its item, the item's frame and image, and the frame's textures lit and
   not (RCO hashes without labels) */
#define INDICATOR_NOTICE_ICON_TEMPLATE	0xFE977644
#define INDICATOR_NOTICE_ICON_ITEM	0x1556A45E
#define INDICATOR_NOTICE_ICON_FRAME	0xA1BD1275
#define INDICATOR_NOTICE_ICON_IMAGE	0x1EF7CBE5
#define INDICATOR_NOTICE_ICON_UNLIT	0x93DDE3EE
#define INDICATOR_NOTICE_ICON_LIT	0xA8C803E2

/* a parental level the lock always holds back */
#define INDICATOR_NOTICE_ICON_LEVEL_LOCKED	99

/* the frame's place out of the bar, and past its place in it as it rises or drops */
#define INDICATOR_NOTICE_ICON_OUT_Y	30.0f
#define INDICATOR_NOTICE_ICON_PAST_Y	-7.0f

/* the icon, while the bar has it (0x81566B30) */
IndicatorNoticeIcon *g_indicatorNoticeIcon;

static void indicatorNoticeIconShow(const IndicatorNoticeIconParam *param);
static void indicatorNoticeIconHide(void);
static void indicatorNoticeIconSetActive(int active);
static void indicatorNoticeIconUpdate(void);

/* the image's texture set: faded in when it was hidden (0x811813FA) */
static void indicatorNoticeIconOnTexture(int32_t type, ui::Handler *self, ui::Event *e,
					 void *data)
{
	if (self == NULL || !((ui::Widget *)self)->IsInherit(ui::Widget::TypeName()))
		return;

	ui::Widget *image = (ui::Widget *)self;

	if (image->m_meta_alpha != 0.0f)
		return;

	intrusive_ptr<graph::Surface> texture;

	image->GetTexture(texture);
	if (texture.get() != NULL)
		image->SetMetaAlpha(1.0f, new Timer(200.0f));
}

/* the image's texture not set: taken off and the image hidden (0x811814E2) */
static void indicatorNoticeIconOnNoTexture(int32_t type, ui::Handler *self, ui::Event *e,
					   void *data)
{
	if (self == NULL || !((ui::Widget *)self)->IsInherit(ui::Widget::TypeName()))
		return;

	ui::Widget *image = (ui::Widget *)self;

	if (image->m_meta_alpha == 0.0f)
		return;
	image->SetTexture(intrusive_ptr<graph::Surface>(NULL));
	image->SetMetaAlpha(0.0f);
}

/* the icon's template opened on the widget, its frame put out of the bar and its image's
   texture events listened to; the interface's notice icon slots filled (0x81180FD8) */
IndicatorNoticeIcon::IndicatorNoticeIcon(Plugin *plugin, ui::Widget *widget)
	: m_root(widget), m_plugin(plugin), m_texture(NULL), m_parentalLevel(0), m_shown(false),
	  m_active(false), m_locked(false)
{
	if (widget != NULL &&
	    plugin->TemplateOpen(widget, IDParam(s_empty_name, INDICATOR_NOTICE_ICON_TEMPLATE),
				 Plugin::TemplateOpenParam()) == 0) {
		m_widget = widget->FindChild(IDParam(s_empty_name, INDICATOR_NOTICE_ICON_ITEM), 0);

		ui::Widget *frame =
			m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NOTICE_ICON_FRAME), 0);
		math::v4 pos = frame->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		pos.set_y(INDICATOR_NOTICE_ICON_OUT_Y);
		frame->SetPos(pos);

		ui::Widget *image =
			m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NOTICE_ICON_IMAGE), 0);

		image->AddEventListener(ui::Handler::CB_STATE_READY_CACHEIMAGE,
					new ui::EventCBListener(indicatorNoticeIconOnTexture, this),
					true);
		image->AddEventListener(ui::Handler::CB_COMMAND,
					new ui::EventCBListener(indicatorNoticeIconOnNoTexture, this),
					true);
	}
	g_indicatorInterface->ShowNoticeIcon = indicatorNoticeIconShow;
	g_indicatorInterface->HideNoticeIcon = indicatorNoticeIconHide;
	g_indicatorInterface->SetNoticeIconActive = indicatorNoticeIconSetActive;
	g_indicatorInterface->UpdateNoticeIcon = indicatorNoticeIconUpdate;
	g_indicatorNoticeIcon = this;
}

/* (0x811812FE) */
IndicatorNoticeIcon::~IndicatorNoticeIcon()
{
	ui::Widget *image =
		m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NOTICE_ICON_IMAGE), 0);

	image->DeleteEventCallback(ui::Handler::CB_STATE_READY_CACHEIMAGE, NULL, NULL);
	image->DeleteEventCallback(ui::Handler::CB_COMMAND, NULL, NULL);
	m_texture.clear();
	g_indicatorNoticeIcon = NULL;
}

/* the frame risen from out of the bar past its place and back, the image hidden and, with a
   texture, faded in after it (0x81181562) */
void IndicatorNoticeIcon::Show()
{
	ui::Widget *frame = m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NOTICE_ICON_FRAME), 0);
	ui::Widget *image = m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NOTICE_ICON_IMAGE), 0);

	frame->KillAnim(ui::Widget::ANIM_MOVE);
	image->KillAnim(ui::Widget::ANIM_SHOW_ALPHA);
	image->SetMetaAlpha(0.0f);

	math::v4 pos = frame->GetPos(1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	pos.set_y(0.0f);
	frame->SetPos(math::v4(pos.extract_x(), INDICATOR_NOTICE_ICON_OUT_Y, 0.0f, 0.0f));
	frame->SetPos(math::v4(pos.extract_x(), INDICATOR_NOTICE_ICON_PAST_Y, 0.0f, 0.0f),
		      new Timer(300.0f));
	frame->SetPos(pos, new Timer(100.0f));

	intrusive_ptr<graph::Surface> texture;

	image->GetTexture(texture);
	if (texture.get() != NULL) {
		image->SetMetaAlpha(0.0f, new Timer(400.0f));
		image->SetMetaAlpha(1.0f, new Timer(300.0f));
	}
	m_shown = true;
}

/* the frame risen past its place and dropped out of the bar, the image shown at once
   (0x811818FE) */
void IndicatorNoticeIcon::Hide()
{
	ui::Widget *frame = m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NOTICE_ICON_FRAME), 0);
	ui::Widget *image = m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NOTICE_ICON_IMAGE), 0);

	frame->KillAnim(ui::Widget::ANIM_MOVE);

	math::v4 pos = frame->GetPos(1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	frame->SetPos(math::v4(pos.extract_x(), INDICATOR_NOTICE_ICON_PAST_Y, 0.0f, 0.0f),
		      new Timer(100.0f));
	frame->SetPos(math::v4(pos.extract_x(), INDICATOR_NOTICE_ICON_OUT_Y, 0.0f, 0.0f),
		      new Timer(300.0f));

	intrusive_ptr<graph::Surface> texture;

	image->GetTexture(texture);
	if (texture.get() != NULL) {
		image->KillAnim(ui::Widget::ANIM_SHOW_ALPHA);
		image->SetMetaAlpha(1.0f);
	}
	m_shown = false;
}

/* the frame given its lit texture while active and not locked, the other one otherwise
   (0x81181B7C) */
void IndicatorNoticeIcon::UpdateFrame()
{
	ui::Widget *frame = m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NOTICE_ICON_FRAME), 0);
	Plugin *plugin = g_indicatorPlugin;

	if (!m_active || m_locked)
		frame->SetTexture(plugin->GetTexture(IDParam(s_empty_name, INDICATOR_NOTICE_ICON_UNLIT)));
	else
		frame->SetTexture(plugin->GetTexture(IDParam(s_empty_name, INDICATOR_NOTICE_ICON_LIT)));
}

/* the image hidden; the parental lock's image put in it where the registry's content control
   holds the content's level back (or cannot be read), the image's texture faded in otherwise;
   the frame's texture then (0x81181D0C) */
void IndicatorNoticeIcon::UpdateImage()
{
	ui::Widget *image = m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NOTICE_ICON_IMAGE), 0);

	image->SetMetaAlpha(0.0f);

	int control = 0;

	m_locked = true;
	if (sceRegMgrGetKeyInt("/CONFIG/SECURITY/PARENTAL", "content_start_control", &control) == 0)
		m_locked = m_parentalLevel == INDICATOR_NOTICE_ICON_LEVEL_LOCKED ||
			   (control != 0 && m_parentalLevel != 0 && m_parentalLevel > control);
	if (m_locked) {
		char path[2048];

		sce_paf_strncpy(path, "vs0:data/internal/icon/parental_lock.png", sizeof(path));

		intrusive_ptr<cache::CacheImageInterface> lock;

		indicatorCacheCreateFile(string(path), lock, NULL, NULL);
		image->SetMetaAlpha(0.0f);
		image->SetDrawObjMaskBit(&lock, 0, 0);
	} else {
		image->SetTexture(m_texture);
		image->SetMetaAlpha(m_texture.get() != NULL ? 1.0f : 0.0f, new Timer(200.0f));
	}
	UpdateFrame();
}

/* the icon made active and its image taken */
static inline void indicatorNoticeIconActivate(IndicatorNoticeIcon *icon)
{
	icon->m_active = true;
	icon->UpdateImage();
}

/* the icon given the image and the level, made active, its image taken and its frame risen
   (0x81181F42) */
static void indicatorNoticeIconShow(const IndicatorNoticeIconParam *param)
{
	IndicatorNoticeIcon *icon = g_indicatorNoticeIcon;

	if (icon == NULL)
		return;
	icon->m_texture = param->texture;
	icon->m_parentalLevel = param->parentalLevel;
	indicatorNoticeIconActivate(g_indicatorNoticeIcon);
	if (!g_indicatorNoticeIcon->m_shown)
		g_indicatorNoticeIcon->Show();
}

/* (0x81181F92) */
static void indicatorNoticeIconHide(void)
{
	if (g_indicatorNoticeIcon != NULL && g_indicatorNoticeIcon->m_shown)
		g_indicatorNoticeIcon->Hide();
}

/* (0x81181FAA) */
static void indicatorNoticeIconSetActive(int active)
{
	if (g_indicatorNoticeIcon == NULL)
		return;
	g_indicatorNoticeIcon->m_active = active == 1;
	g_indicatorNoticeIcon->UpdateFrame();
}

/* (0x81181FCA) */
static void indicatorNoticeIconUpdate(void)
{
	if (g_indicatorNoticeIcon != NULL)
		g_indicatorNoticeIcon->UpdateImage();
}
