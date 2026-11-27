/*
 * indicator_inhale_anim.cpp: an icon drawn into the notices' button: the animation's page opened
 * over the bar, the icon flown from the screen's middle to the button and shrunk into it, the
 * button pulsing as it takes it, and the page closed and the caller told when the button has
 * faded (0x8117F6A4-0x81180848). The file's and all names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <math/ngp/v4.h>
#include <thread/thread.h>
#include <timer/timer.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include <common/transition.h>
#include <cache/cache_image.h>
#include <graphics/surface/surface.h>
/* Sony built this file without the keys' header, so its static initializer has no keys */
#define LSDB_KEYS_H
#include "indicator.h"

using namespace paf;

/* the animation's page and its widgets (RCO hashes without labels) */
#define INDICATOR_INHALE_PAGE		0x679D98A7
#define INDICATOR_INHALE_ICON		0x22FA39B4
#define INDICATOR_INHALE_BUTTON		0xD1B0E68D

/* the size the icon's file is loaded at */
#define INDICATOR_INHALE_ICON_SIZE	200

/* the page open, its icon and its button, and the callback told when it closes (0x81544CB4-
   0x81544CC0) */
static ui::Widget *s_page = NULL;
static ui::Widget *s_icon = NULL;
static ui::Widget *s_button = NULL;
static ui::AnimCB s_callback = NULL;

/* the icon's file and its texture, kept while the page is open (0x81566B18, 0x81566B24) */
static intrusive_ptr<graph::Surface> s_texture(NULL);
static string s_iconPath(NULL);

static void indicatorInhaleAnimLanded(Timer *t, ui::AnimListener *listener, void *target,
				      void *userData, void *context);
static void indicatorInhaleAnimEnd(Timer *t, ui::AnimListener *listener, void *target,
				   void *userData, void *context);

/* the icon given its texture, or its file's image, and the button the image the bar shows
   without new notices, laid out over the bar's button (0x8117F6A4) */
static void indicatorInhaleAnimSetup(ui::Widget *page)
{
	s_page = page;
	s_icon = s_page->FindChild(IDParam(s_empty_name, INDICATOR_INHALE_ICON), 0);
	s_button = s_page->FindChild(IDParam(s_empty_name, INDICATOR_INHALE_BUTTON), 0);

	intrusive_ptr<graph::Surface> texture = s_texture;
	intrusive_ptr<graph::Surface> surface = g_indicatorNewEvent->GetNoNoticeImage()->GetImage();

	if (s_icon != NULL) {
		if (texture.get() != NULL) {
			s_icon->SetTexture(texture);
		} else if (s_iconPath.length() != 0) {
			cache::CacheImage::Option option;

			indicatorIconOption(&option, INDICATOR_INHALE_ICON_SIZE);

			intrusive_ptr<cache::CacheImageInterface> image;

			indicatorCacheCreateFile(s_iconPath, image, &option, NULL);
			s_icon->SetDrawObjMaskBit(&image, 0, 0);
		}
	}
	if (s_button != NULL && surface.get() != NULL) {
		SceUInt32 width = surface->GetWidth() - surface->GetMarginLeft() -
				  surface->GetMarginRight();
		SceUInt32 height = surface->GetHeight() - surface->GetMarginTop() -
				   surface->GetMarginBottom();

		s_button->Hide(0xff, NULL, NULL, 0.0f);
		s_button->SetPos(math::v4(15.0f, 15.0f, 0.0f, 0.0f));
		s_button->SetSize(math::v4((float)width, (float)height, 0.0f, 0.0f));
		s_button->SetTexture(surface);
		s_button->SetAlign(2, 2, 0, NULL);
		s_button->SetAnchor(2, 2, 0, NULL);
		s_button->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	}
}

/* the icon in the screen's middle, flown on a curve to the button, shrinking as it lands
   (0x8117F9FC) */
static void indicatorInhaleAnimIconStart(void)
{
	s_icon->SetParent(s_page, NULL, 0);

	ui::Widget *icon = s_icon;

	icon->SetPos(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	icon->SetSize(math::v4(200.0f, 200.0f, 0.0f, 0.0f));
	icon->Hide(0xff, NULL, NULL, 0.0f);

	float curve[2] = { 0.2f, -0.2f };

	icon->SetPos(math::v4(380.0f, 160.0f, 0.0f, 0.0f),
		     new Timer2D(600.0f, Timer::FUNC_LINEAR, Timer::FUNC_CUSTOM_PARABOLIC_IN,
				 Timer::REPEAT_NONE, -1, NULL, curve),
		     ui::Widget::ANIM_MOVE, 2, 450.0f);
	icon->SetSize(math::v4(70.0f, 70.0f, 0.0f, 0.0f), new Timer(600.0f), ui::Widget::ANIM_SIZE,
		      2, 450.0f, indicatorInhaleAnimLanded);
	icon->Show(0xff, new Timer(200.0f), NULL, 0.0f);
	icon->SetMetaAlpha(0.0f, new Timer(180.0f, Timer::FUNC_QUAD_IN), ui::Widget::ANIM_META_ALPHA,
			   2, 870.0f);
}

/* the time from the end given back by the length given, none where it is longer */
static inline float indicatorInhaleAnimDelay(float end, float length)
{
	float delay = end - length;

	if (delay < 0.0f)
		delay = 0.0f;
	return delay;
}

/* the button shrunk, then grown past its size and grown again as the icon lands
   (0x8117FD4A) */
static void indicatorInhaleAnimButtonStart(void)
{
	float elastic[4] = { 6.0f, 0.0f, 0.0f, 0.0f };
	ui::Widget *button = s_button;

	button->SetPos(math::v4(10.0f, 5.0f, 0.0f, 0.0f));
	button->SetScale(math::v4(0.95f, 0.95f, 0.0f, 0.0f));
	button->Hide(0xff, NULL, NULL, 0.0f);
	button->Show(0xff, new Timer(200.0f), NULL, 0.0f);
	button->SetScale(math::v4(1.1f, 1.1f, 0.0f, 0.0f),
			 new Timer(800.0f, Timer::FUNC_CUSTOM_ELASTIC_OUT, Timer::REPEAT_NONE, 0,
				   elastic),
			 ui::Widget::ANIM_SCALE, 2, 0.0f);

	button->SetScale(math::v4(1.4f, 1.4f, 0.0f, 0.0f),
			 new Timer(350.0f, Timer::FUNC_BACK_OUT), ui::Widget::ANIM_SCALE, 0,
			 indicatorInhaleAnimDelay(250.0f, 100.0f));
}

/* the animation's page opened with the icon and its texture or file, the icon flown in and
   the button pulsed, the bar's new-event item burst and the screen dimmed (0x81180010) */
__attribute__((noinline))
static int indicatorInhaleAnimOpen(const IndicatorInhaleAnim *anim)
{
	thread::RMutex::main_thread_mutex.Lock();
	s_texture = anim->texture;
	s_iconPath = anim->iconPath;
	s_callback = anim->callback;

	{
		Plugin::PageOpenParam param;
		ui::Scene *page = g_indicatorPlugin->PageOpen(
			IDParam(s_empty_name, INDICATOR_INHALE_PAGE), param);

		if (page != NULL) {
			ScePafCommon_6CF3A3C8(0.0f, page, common::transition::Type_FadeinFast, false,
					      false);
			indicatorInhaleAnimSetup(page);
			indicatorInhaleAnimIconStart();
			indicatorInhaleAnimButtonStart();
			g_indicatorNewEvent->Burst();
			g_indicatorNewEvent->ShowDimmer();
		}
	}
	thread::RMutex::main_thread_mutex.Unlock();
	return 0;
}

/* (0x8118014A) */
int indicatorInhaleAnimStart(const IndicatorInhaleAnim *anim)
{
	return indicatorInhaleAnimOpen(anim);
}

/* the page closed early, its caller told first, and the bar's new-event item put back
   (0x81180152) */
void indicatorNoticePageClose(void)
{
	thread::RMutex::main_thread_mutex.Lock();
	if (s_page != NULL) {
		indicatorInhaleAnimEnd(NULL, NULL, NULL, NULL, NULL);
		g_indicatorNewEvent->Restore();
	}
	thread::RMutex::main_thread_mutex.Unlock();
}

/* the button faded: the caller told, the page closed and forgotten and the screen's dimmer
   hidden (0x811802C2) */
static void indicatorInhaleAnimEnd(Timer *t, ui::AnimListener *listener, void *target,
				   void *userData, void *context)
{
	if (s_callback != NULL)
		s_callback(t, listener, target, userData, context);
	s_callback = NULL;
	if (s_page != NULL) {
		g_indicatorPlugin->PageClose(IDParam(s_empty_name, INDICATOR_INHALE_PAGE),
					     Plugin::PageCloseParam());
		s_page = NULL;
	}
	s_icon = NULL;
	s_button = NULL;
	s_texture.clear();
	s_iconPath.clear();
	s_callback = NULL;
	g_indicatorNewEvent->HideDimmer();
}

/* the icon landed: put into the button, faded in and grown to its size there, the button
   shrunk and faded, the page closed after it (0x81180400) */
static void indicatorInhaleAnimLanded(Timer *t, ui::AnimListener *listener, void *target,
				      void *userData, void *context)
{
	ui::Widget *icon = s_icon;
	ui::Widget *button = s_button;

	icon->SetParent(button, NULL, 0);
	icon->Hide(0xff, NULL, NULL, 0.0f);
	icon->Show(0xff, new Timer(10.0f), NULL, 0.0f);
	icon->SetPos(math::v4(20.0f, 20.0f, 0.0f, 0.0f));
	icon->SetSize(math::v4(20.0f, 20.0f, 0.0f, 0.0f));
	icon->SetMetaAlpha(0.2f);
	icon->SetMetaAlpha(0.8f, new Timer(200.0f, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_SIZE, 2);
	icon->SetSize(math::v4(57.0f, 57.0f, 0.0f, 0.0f), new Timer(200.0f, Timer::FUNC_BACK_OUT),
		      ui::Widget::ANIM_SIZE, 2);
	button->SetScale(math::v4(0.6f, 0.6f, 0.0f, 0.0f), new Timer(200.0f, Timer::FUNC_QUAD_IN),
			 ui::Widget::ANIM_SCALE, 0, 500.0f);
	button->Hide(0xff, new Timer(140.0f), indicatorInhaleAnimEnd, 800.0f);
	g_indicatorNewEvent->Restore();
}
