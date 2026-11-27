/*
 * indicator.cpp: indicator_plugin's bar (0x8115B59C-0x8115F7C4): its status given to the
 * items, its layout for each mode, the theme's colors and images, and the interface functions
 * the shell's other plugins call. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <math/ngp/v4.h>
#include <versatileparam/idparam.h>
#include <timer/timer.h>
#include <system/system.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include <graphics/drawobj/draw_obj.h>
#include "../../../theme.h"
#include "../../../plugins.h"
#include "indicator.h"

using namespace paf;

/* the bar's widgets (PLACEHOLDER names) */
#define INDICATOR_BAR			0xFC7100D8	/* the bar's plane */
#define INDICATOR_COUNTER_A		0x6B85F455	/* the two counters UpdateCounters shows */
#define INDICATOR_COUNTER_B		0x8542FFED
#define INDICATOR_FLIGHT_MODE_ICON		0xC8761902	/* the flight mode's icon */

/* the bar's textures (PLACEHOLDER names) */
#define INDICATOR_TEX_BAR		0xBEF34D7F
#define INDICATOR_TEX_BAR_DARK		0x2F413545
#define INDICATOR_TEX_BAR_DOLCE		0xE7AA163E
#define INDICATOR_TEX_BAR_THEME		0x94BAEAEE

/* the bar's animations (PLACEHOLDER names) */
#define INDICATOR_ANIM_SLIDE		0x1000C
#define INDICATOR_ANIM_HIDE		0x1000D
#define INDICATOR_ANIM_LAYOUT		0x1000E
#define INDICATOR_ANIM_FLIGHT_MODE_X		0xD010000
#define INDICATOR_ANIM_FLIGHT_MODE_Y		0xE010000

/* the hide's time, in milliseconds */
#define INDICATOR_HIDE_TIME		200.0f

/* the resolutions a position is given for (PLACEHOLDER names) */
#define INDICATOR_RES_QHD		15
#define INDICATOR_RES_HD		16

Indicator *g_indicator;

/* the widget shown over the time, at once without a timer */
static inline void indicatorShowWidget(ui::Widget *widget, Timer *timer)
{
	widget->Show(0xff, timer, NULL, 0.0f);
}

static inline void indicatorHideWidget(ui::Widget *widget, Timer *timer)
{
	widget->Hide(0xff, timer, NULL, 0.0f);
}

/* the status kept, and given to every item (0x8115B59C) */
void Indicator::SetStatus(IndicatorStatus status)
{
	m_status = status;
	for (vector<IndicatorItem *>::iterator it = m_items.begin(); it != m_items.end(); ++it)
		(*it)->OnStatus(m_status);
}

/* the bar hidden, over the hide's time or at once, and moved up by its height (0x8115B750) */
int Indicator::Hide(bool animate)
{
	int ret = -1;
	ui::Widget *root = indicatorFindWidget(IDParam(s_empty_name, INDICATOR_PAGE),
					       IDParam(s_empty_name, INDICATOR_ROOT));

	if (root != NULL) {
		m_status.visible = false;
		SetStatus(m_status);

		float time = animate ? INDICATOR_HIDE_TIME : 0.0f;

		if (time > 0.0f)
			indicatorHideWidget(root, new Timer(time));
		else
			indicatorHideWidget(root, NULL);

		math::v4 size = *root->GetSize(0);
		math::v4 pos = root->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		ui::SceneResolution *res = ScePafWidget_CDC90BA2(root->m_scene->m_env_param,
								 root->m_scene->m_resolution_id);

		if (res != NULL) {
			ui::ResolutionData data[2] = {
				{ INDICATOR_RES_QHD, { 0.0f, 0.0f, 0.0f } },
				{ INDICATOR_RES_HD, { 0.0f, 0.0f, 0.0f } },
			};

			data[0].m_value[0] = pos.extract_x();
			data[0].m_value[1] = res->m_scissor_height * 0.5f + size.extract_y();
			data[0].m_value[2] = pos.extract_z();
			data[1].m_value[0] = pos.extract_x();
			data[1].m_value[1] = res->m_scissor_width * 0.5f + size.extract_y();
			data[1].m_value[2] = pos.extract_z();
			root->KillAnim(INDICATOR_ANIM_HIDE);
			ret = root->SetPos(data, 2,
					   animate ? new Timer(INDICATOR_HIDE_TIME, Timer::FUNC_QUAD_OUT)
						   : NULL,
					   INDICATOR_ANIM_HIDE, 0, 0.0f, NULL, NULL);
		}
	}
	return ret;
}

/* the theme's notice glow color, the default's without a theme (0x8115BB2A) */
math::v4 indicatorThemeNoticeGlowColor(void)
{
	common::SharedPtr<LsdbThemeInfo> info = *shellThemeGetInfo();

	if (info.get() == NULL)
		return math::v4(56.0f / 255.0f, 182.0f / 255.0f, 1.0f, 1.0f);

	uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(
		info->m_infomationBarProperty.m_noticeGlowColor)));
	math::v4 color(sce::Vectormath::Simd::Aos::Vector4(
		vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))),
			  vdupq_n_f32(1.0f / 255.0f))));

	color.set_w(1.0f);
	return color;
}

/* the theme's notice font color, white without a theme (0x8115BBE4) */
math::v4 indicatorThemeNoticeFontColor(void)
{
	common::SharedPtr<LsdbThemeInfo> info = *shellThemeGetInfo();

	if (info.get() == NULL)
		return math::v4(1.0f, 1.0f, 1.0f, 1.0f);

	uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(
		info->m_infomationBarProperty.m_noticeFontColor)));
	math::v4 color(sce::Vectormath::Simd::Aos::Vector4(
		vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))),
			  vdupq_n_f32(1.0f / 255.0f))));

	color.set_w(1.0f);
	return color;
}

/* the theme's image for new notices, nothing without a theme or an image (0x8115BC92) */
string indicatorThemeNewNoticeFile(void)
{
	common::SharedPtr<LsdbThemeInfo> info = *shellThemeGetInfo();

	if (info.get() == NULL)
		return string("");
	if (info->m_infomationBarProperty.m_newNoticeFilePath.length() == 0)
		return string("");

	string path;

	shellThemeInfoGetFilePath(info.get(), &path,
				  info->m_infomationBarProperty.m_newNoticeFilePath);
	return path;
}

/* the theme's image for no notices (0x8115BE08) */
string indicatorThemeNoNoticeFile(void)
{
	common::SharedPtr<LsdbThemeInfo> info = *shellThemeGetInfo();

	if (info.get() == NULL)
		return string("");
	if (info->m_infomationBarProperty.m_noNoticeFilePath.length() == 0)
		return string("");

	string path;

	shellThemeInfoGetFilePath(info.get(), &path,
				  info->m_infomationBarProperty.m_noNoticeFilePath);
	return path;
}

/* the interface's function 2: the mode's type (0x8115BF80) */
int indicatorGetModeType(int *type)
{
	int ret = -1;

	if (g_indicator != NULL) {
		ret = 0;
		*type = g_indicator->m_mode.type;
	}
	return ret;
}

/* (0x8115BF9A) */
bool indicatorIsDolceDefault(void)
{
	return shellThemeGetInfo()->get() == NULL && system::Model::GetType() == 1 &&
	       g_indicator != NULL && g_indicator->m_mode.type == 0;
}

/* the theme's indicator color, white without a theme (0x8115BFC8) */
math::v4 indicatorThemeIndicatorColor(void)
{
	common::SharedPtr<LsdbThemeInfo> info = *shellThemeGetInfo();

	if (info.get() == NULL)
		return math::v4(1.0f, 1.0f, 1.0f, 1.0f);

	uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(
		info->m_infomationBarProperty.m_indicatorColor)));
	math::v4 color(sce::Vectormath::Simd::Aos::Vector4(
		vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))),
			  vdupq_n_f32(1.0f / 255.0f))));

	color.set_w(1.0f);
	return color;
}

/* the icons' and the texts' colors: the theme's, the dark mode's grays, or white (light gray
   on a Vita) (0x8115C076) */
void Indicator::SetIconColors(ui::Widget *root, IndicatorMode mode)
{
	math::v4 icon;
	math::v4 text;

	if (shellThemeGetInfo()->get() != NULL && mode.style == 2) {
		icon = indicatorThemeIndicatorColor();
		text = indicatorThemeIndicatorColor();
	} else if (mode.style == 1) {
		icon = math::v4(0.42f, 0.42f, 0.42f, 1.0f);
		text = math::v4(0.22f, 0.22f, 0.22f, 0.8f);
	} else {
		math::v4 color;

		if (indicatorIsDolceDefault())
			color = math::v4(1.0f, 1.0f, 1.0f, 0.0f);
		else
			color = math::v4(246.0f, 246.0f, 246.0f, 0.0f) * (1.0f / 255.0f);
		icon = color;
		icon.set_w(1.0f);
		text = math::v4(1.0f, 1.0f, 1.0f, 1.0f);
	}
	m_status.iconColor = icon;
	m_status.textColor = text;
}

/* the theme's bar color, white without a theme (0x8115C188) */
math::v4 indicatorThemeBarColor(void)
{
	common::SharedPtr<LsdbThemeInfo> info = *shellThemeGetInfo();

	if (info.get() == NULL)
		return math::v4(1.0f, 1.0f, 1.0f, 1.0f);

	uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(
		info->m_infomationBarProperty.m_barColor)));
	math::v4 color(sce::Vectormath::Simd::Aos::Vector4(
		vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))),
			  vdupq_n_f32(1.0f / 255.0f))));

	color.set_w(1.0f);
	return color;
}

/* the bar's texture and color: dark for the dark mode with the notices hidden, the Dolce's own
   look added (blend 1), the theme's color for its mode (0x8115C236) */
void Indicator::SetBarTexture(ui::Widget *bar, IndicatorMode mode)
{
	IDParam texture(s_empty_name, mode.style == 1 ? INDICATOR_TEX_BAR_DARK : INDICATOR_TEX_BAR);

	if (mode.type != 1)
		texture.m_idhash = INDICATOR_TEX_BAR;
	if (indicatorIsDolceDefault()) {
		texture.m_idhash = INDICATOR_TEX_BAR_DOLCE;

		uint32_t style = bar->GetDrawObj(0)->m_draw_style.m_value;

		bar->GetDrawObj(0)->m_draw_style.m_value = (style & ~0xF) | 1;
	} else {
		uint32_t style = bar->GetDrawObj(0)->m_draw_style.m_value;

		bar->GetDrawObj(0)->m_draw_style.m_value = style & ~0xF;
	}
	bar->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	if (shellThemeGetInfo()->get() != NULL && mode.style == 2) {
		texture.m_idhash = INDICATOR_TEX_BAR_THEME;
		m_status.barColor = indicatorThemeBarColor();
		bar->SetColor(m_status.barColor);
	}

	intrusive_ptr<graph::Surface> surface = g_indicatorPlugin->GetTexture(texture);

	bar->SetTexture(surface);
}

/* the two counters' places: by the flight mode's icon, or past it */
#define INDICATOR_COUNTER_X_QHD		-472.0f
#define INDICATOR_COUNTER_X_HD		-264.0f
#define INDICATOR_COUNTER_SHIFT		32.0f

/* the two counters: none while the registry values are read, else the first moved by the flight mode's icon and
   shown while on, the second while off (0x8115C3E8) */
void Indicator::UpdateCounters(ui::Widget *root, int on)
{
	ui::Widget *first = root->FindChild(IDParam(s_empty_name, INDICATOR_COUNTER_A), 0);
	ui::Widget *second = root->FindChild(IDParam(s_empty_name, INDICATOR_COUNTER_B), 0);
	ui::Widget *bar = root->FindChild(IDParam(s_empty_name, INDICATOR_BAR), 0);

	if (m_regInfoJob.get() != NULL) {
		indicatorHideWidget(first, NULL);
		indicatorHideWidget(second, NULL);
		return;
	}
	if (on == m_countersOn)
		return;
	if (on == 0) {
		if (first != NULL)
			indicatorHideWidget(first, new Timer(200.0f));
		if (second != NULL) {
			float shift = m_status.flightMode != 0 ? INDICATOR_COUNTER_SHIFT : 0.0f;
			ui::ResolutionData pos[2] = {
				{ INDICATOR_RES_QHD, { shift + INDICATOR_COUNTER_X_QHD, 0.0f, 0.0f } },
				{ INDICATOR_RES_HD, { shift + INDICATOR_COUNTER_X_HD, 0.0f, 0.0f } },
			};

			second->SetPos(pos, 2, NULL, ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
			indicatorShowWidget(second, new Timer(200.0f));
		}
	} else {
		if (first != NULL) {
			float shift = m_status.flightMode != 0 ? INDICATOR_COUNTER_SHIFT : 0.0f;
			ui::ResolutionData pos[2] = {
				{ INDICATOR_RES_QHD, { shift + INDICATOR_COUNTER_X_QHD, 0.0f, 0.0f } },
				{ INDICATOR_RES_HD, { shift + INDICATOR_COUNTER_X_HD, 0.0f, 0.0f } },
			};

			first->SetPos(pos, 2, NULL, ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
			indicatorShowWidget(first, new Timer(200.0f));
		}
		if (second != NULL)
			indicatorHideWidget(second, new Timer(200.0f));
	}
	m_countersOn = on;
}

static void indicatorOnFlightModeSlid(Timer *t, ui::AnimListener *listener, void *widgetArg,
				      void *userArg, void *extraArg);

/* the flight mode's icon slid in or out: in, the first counter's icons slid past it and the icon
   moved down, faded in; out, both moved back (0x8115C906) */
void Indicator::UpdateFlightModeIcon(ui::Widget *root, int on, bool animate)
{
	ui::Widget *icons = root->FindChild(IDParam(s_empty_name, INDICATOR_COUNTER_A), 0);
	ui::Widget *plane = root->FindChild(IDParam(s_empty_name, INDICATOR_FLIGHT_MODE_ICON), 0);

	if (m_regInfoJob.get() != NULL) {
		indicatorHideWidget(icons, NULL);
		indicatorHideWidget(plane, NULL);
		return;
	}
	if (on == m_flightModeIconOn)
		return;
	if (on != 0) {
		float width = 0.0f;

		if (icons != NULL) {
			for (list<ui::Widget *>::iterator it = icons->m_children.begin();
			     it != icons->m_children.end(); ++it)
				width += (*it)->GetSize(0)->extract_x();

			ui::ResolutionData pos[2] = {
				{ INDICATOR_RES_QHD, { -(width + 472.0f), 0.0f, 0.0f } },
				{ INDICATOR_RES_HD, { -(width + 264.0f), 0.0f, 0.0f } },
			};

			if (icons->FindAnim(INDICATOR_ANIM_SLIDE) != 0)
				icons->KillAnim(INDICATOR_ANIM_SLIDE);
			icons->SetPos(pos, 2,
				      animate ? new Timer(2000.0f, Timer::FUNC_EXPO_OUT_28) : NULL,
				      INDICATOR_ANIM_SLIDE, 2, 1000.0f, indicatorOnFlightModeSlid, NULL);
			indicatorHideWidget(icons, new Timer(2000.0f));
		}
		if (plane != NULL) {
			ui::ResolutionData pos[2] = {
				{ INDICATOR_RES_QHD, { -472.0f, 0.0f, 0.0f } },
				{ INDICATOR_RES_HD, { -264.0f, 0.0f, 0.0f } },
			};

			plane->SetPos(math::v4(0.0f, 32.0f, 0.0f, 0.0f), new Timer(0.0f),
				       ui::Widget::ANIM_MOVE, 2, 0.0f);
			plane->SetPos(pos, 2,
				       animate ? new Timer(1000.0f, Timer::FUNC_EXPO_OUT_28) : NULL,
				       INDICATOR_ANIM_FLIGHT_MODE_X, 0, 0.0f, NULL, NULL);
			plane->SetPos(pos, 2,
				       animate ? new Timer(2000.0f, Timer::FUNC_EXPO_OUT_28) : NULL,
				       INDICATOR_ANIM_FLIGHT_MODE_Y, 0, 0.0f, NULL, NULL);
			indicatorShowWidget(plane, NULL);
		}
	} else {
		if (icons != NULL) {
			ui::ResolutionData pos[2] = {
				{ INDICATOR_RES_QHD, { -472.0f, 0.0f, 0.0f } },
				{ INDICATOR_RES_HD, { -264.0f, 0.0f, 0.0f } },
			};

			if (icons->FindAnim(INDICATOR_ANIM_SLIDE) != 0)
				icons->KillAnim(INDICATOR_ANIM_SLIDE);
			icons->SetPos(pos, 2,
				      animate ? new Timer(1000.0f, Timer::FUNC_SINE_INOUT) : NULL,
				      INDICATOR_ANIM_SLIDE, 2, 0.0f, NULL, NULL);
			indicatorShowWidget(icons, new Timer(1000.0f));
		}
		if (plane != NULL) {
			ui::ResolutionData pos[2] = {
				{ INDICATOR_RES_QHD, { -572.0f, 0.0f, 0.0f } },
				{ INDICATOR_RES_HD, { -364.0f, 0.0f, 0.0f } },
			};

			if (plane->FindAnim(ui::Widget::ANIM_MOVE) != 0)
				plane->KillAnimType(ui::Widget::ANIM_MOVE);
			plane->SetPos(pos, 2,
				       animate ? new Timer(1000.0f, Timer::FUNC_SINE_INOUT) : NULL,
				       ui::Widget::ANIM_MOVE, 2, 0.0f, NULL, NULL);
		}
	}
	m_flightModeIconOn = on;
}

/* the widgets SetMode shows or hides (PLACEHOLDER names) */
#define INDICATOR_MODE_ICON_A		0x89588FB3
#define INDICATOR_MODE_ICON_B		0x5BA2D706
#define INDICATOR_MODE_CLOCK		0xD62FC4E9
#define INDICATOR_MODE_CLOCK_ICON	0x1556A45E

/* the bar laid out for the mode: the parts redrawn, the theme's notice look taken, and with type 1
   the icons hidden and the clock moved right (0x8115D050) */
void Indicator::SetMode(IndicatorMode mode, bool animate)
{
	m_mode = mode;

	ui::Widget *root = indicatorFindWidget(IDParam(s_empty_name, INDICATOR_PAGE),
					       IDParam(s_empty_name, INDICATOR_ROOT));

	if (root != NULL) {
		UpdateFlightModeIcon(root, m_status.flightMode, animate);
		UpdateCounters(root, m_status.value6);
		SetBarTexture(root, m_mode);

		float alpha = m_mode.dimmed != 0 ? 0.7f : 1.0f;

		if (m_mode.type != 1)
			alpha = 1.0f;
		root->SetMetaAlpha(alpha, NULL, ui::Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
		SetIconColors(root, m_mode);
		m_status.noNoticeFile = indicatorThemeNoNoticeFile();
		m_status.newNoticeFile = indicatorThemeNewNoticeFile();
		m_status.noticeFontColor = indicatorThemeNoticeFontColor();
		m_status.noticeGlowColor = indicatorThemeNoticeGlowColor();

		if (mode.type == 1) {
			ui::Widget *icon = root->FindChild(IDParam(s_empty_name, INDICATOR_MODE_ICON_A), 0);

			if (icon != NULL)
				indicatorHideWidget(icon, NULL);
			icon = root->FindChild(IDParam(s_empty_name, INDICATOR_MODE_ICON_B), 0);
			if (icon != NULL)
				indicatorHideWidget(icon, NULL);

			ui::Widget *clock = root->FindChild(IDParam(s_empty_name, INDICATOR_MODE_CLOCK), 0);

			if (clock != NULL) {
				ui::ResolutionData pos[2] = {
					{ INDICATOR_RES_QHD, { 472.0f, 0.0f, 0.0f } },
					{ INDICATOR_RES_HD, { 264.0f, 0.0f, 0.0f } },
				};

				clock->KillAnim(INDICATOR_ANIM_LAYOUT);
				clock->SetPos(pos, 2,
					      animate ? new Timer(200.0f, Timer::FUNC_QUAD_OUT) : NULL,
					      INDICATOR_ANIM_LAYOUT, 0, 0.0f, NULL, NULL);

				ui::Widget *clockIcon = root->FindChild(
					IDParam(s_empty_name, INDICATOR_MODE_CLOCK_ICON), 0);

				if (clockIcon != NULL)
					indicatorHideWidget(clockIcon, NULL);
			}
		} else {
			ui::Widget *icon = root->FindChild(IDParam(s_empty_name, INDICATOR_MODE_ICON_A), 0);

			if (icon != NULL)
				indicatorShowWidget(icon, NULL);
			icon = root->FindChild(IDParam(s_empty_name, INDICATOR_MODE_ICON_B), 0);
			if (icon != NULL)
				indicatorShowWidget(icon, NULL);

			ui::Widget *clock = root->FindChild(IDParam(s_empty_name, INDICATOR_MODE_CLOCK), 0);

			if (clock != NULL) {
				ui::ResolutionData pos[2] = {
					{ INDICATOR_RES_QHD, { 394.0f, 0.0f, 0.0f } },
					{ INDICATOR_RES_HD, { 186.0f, 0.0f, 0.0f } },
				};

				clock->KillAnim(INDICATOR_ANIM_LAYOUT);
				clock->SetPos(pos, 2,
					      animate ? new Timer(200.0f, Timer::FUNC_QUAD_OUT) : NULL,
					      INDICATOR_ANIM_LAYOUT, 0, 0.0f, NULL, NULL);

				ui::Widget *clockIcon = clock->FindChild(
					IDParam(s_empty_name, INDICATOR_MODE_CLOCK_ICON), 0);

				if (clockIcon != NULL)
					indicatorShowWidget(clockIcon, NULL);
			}
		}
	}
	indicatorNoticePopupCloseAll();
	indicatorNoticePageClose();
}

/* the bar's page opened, hidden, and its object made (0x8115E37A) */
void indicatorCreate(void)
{
	Plugin::PageOpenParam param;

	param.overwrite_draw_priority = 20;

	ui::Scene *page = g_indicatorPlugin->PageOpen(IDParam(s_empty_name, INDICATOR_PAGE), param);

	if (page != NULL)
		ScePafWidget_66847A11(page, true);
	g_indicator = new Indicator(page);
	if (g_indicator != NULL)
		g_indicator->Hide(false);
	indicatorTelephonyListen();
}

int indicatorShow(IndicatorMode mode, bool animate);
int indicatorHide(bool animate);
int indicatorGetModeType(int *type);
bool indicatorIsVisible(void);
int indicatorSetValue4(int value);
int indicatorSetValue8(int value);
int indicatorSetFlightMode(int on);
int indicatorSetValue6(int value);
int indicatorSetBluetooth(int on);
int indicatorUnsupported(void);
int indicatorSetNotification(int on);
void indicatorUpdateTheme(void);
static void indicatorOnRegInfo(const list<IndicatorRegKey> &keys, void *data);
static void indicatorOnBtConfig(int result, int config, void *data);

/* (0x8115D882) */
Indicator::Indicator(ui::Widget *page)
	: m_reserved(-1), m_flightModeIconOn(-1), m_countersOn(-1)
{
	(void)page;
	m_mode.type = 0;
	m_mode.style = 0;
	m_mode.dimmed = 0;

	m_status.visible = false;
	m_status.value4 = 0;
	m_status.bluetooth = 0;
	m_status.flightMode = 0;
	m_status.value8 = 0;
	m_status.initLeftReady = 1;
	m_status.initRightReady = 1;
	m_status.notification = 1;
	m_status.value6 = 1;
	m_status.barColor = math::v4(1.0f, 1.0f, 1.0f, 1.0f);
	m_status.iconColor = math::v4(1.0f, 1.0f, 1.0f, 1.0f);
	m_status.textColor = math::v4(1.0f, 1.0f, 1.0f, 1.0f);
	m_status.noNoticeFile = string("");
	m_status.newNoticeFile = string("");
	m_status.noticeFontColor = math::v4(1.0f, 1.0f, 1.0f, 1.0f);
	m_status.noticeGlowColor = math::v4(56.0f / 255.0f, 182.0f / 255.0f, 1.0f, 1.0f);

	ui::Widget *root = indicatorFindWidget(IDParam(s_empty_name, INDICATOR_PAGE),
					       IDParam(s_empty_name, INDICATOR_ROOT));
	if (root != NULL) {
		ScePafWidget_A1193AAB(root, 1, 0);

		ui::Widget *child = root->FindChild(IDParam(s_empty_name, 0x5BA2D306u), 0);
		if (child != NULL) {
			IndicatorNewEvent *item = new IndicatorNewEvent(g_indicatorPlugin, child);
			if (item != NULL)
				m_items.push_back(item);
		}

		child = root->FindChild(IDParam(s_empty_name, INDICATOR_COUNTER_A), 0);
		if (child != NULL) {
			IndicatorLeftGroup *item = new IndicatorLeftGroup(g_indicatorPlugin, child);
			if (item != NULL)
				m_items.push_back(item);
		}

		child = root->FindChild(IDParam(s_empty_name, INDICATOR_FLIGHT_MODE_ICON), 0);
		if (child != NULL) {
			IndicatorFlightModeIcon *item =
				new IndicatorFlightModeIcon(g_indicatorPlugin, child);
			if (item != NULL)
				m_items.push_back(item);
		}

		child = root->FindChild(IDParam(s_empty_name, INDICATOR_COUNTER_B), 0);
		if (child != NULL) {
			IndicatorLabel *item = new IndicatorLabel(g_indicatorPlugin, child);
			if (item != NULL)
				m_items.push_back(item);
		}

		child = root->FindChild(IDParam(s_empty_name, 0x89588B73u), 0);
		if (child != NULL) {
			IndicatorPageDots *item = new IndicatorPageDots(g_indicatorPlugin, child);
			if (item != NULL)
				m_items.push_back(item);
		}

		child = root->FindChild(IDParam(s_empty_name, 0xD62FA2E9u), 0);
		if (child != NULL) {
			IndicatorRightGroup *item =
				new IndicatorRightGroup(g_indicatorPlugin, child);
			if (item != NULL)
				m_items.push_back(item);
		}

		list<IndicatorRegKey> keys;
		keys.push_back(IndicatorRegKey(string("/CONFIG/SYSTEM"), string("flight_mode")));
		keys.push_back(IndicatorRegKey(string("/CONFIG/SHELL"), string("notification")));

		m_regInfoJob = indicatorRegInfoJobStart(keys, indicatorOnRegInfo, this);
		m_btConfigJob = indicatorBtConfigJobStart(indicatorOnBtConfig, this);
		SetMode(m_mode, false);
	}

	g_indicatorInterface->Show = indicatorShow;
	g_indicatorInterface->Hide = indicatorHide;
	g_indicatorInterface->GetModeType = indicatorGetModeType;
	g_indicatorInterface->IsVisible = indicatorIsVisible;
	g_indicatorInterface->SetValue4 = indicatorSetValue4;
	g_indicatorInterface->SetValue8 = indicatorSetValue8;
	g_indicatorInterface->SetFlightMode = indicatorSetFlightMode;
	g_indicatorInterface->SetValue6 = indicatorSetValue6;
	g_indicatorInterface->SetBluetooth = indicatorSetBluetooth;
	g_indicatorInterface->Unsupported9 = indicatorUnsupported;
	g_indicatorInterface->Unsupported10 = indicatorUnsupported;
	g_indicatorInterface->SetNotification = indicatorSetNotification;
	g_indicatorInterface->UpdateTheme = indicatorUpdateTheme;
}

/* the jobs cancelled, the bar hidden and its items deleted (0x8115E45E) */
Indicator::~Indicator()
{
	if (m_regInfoJob.get() != NULL) {
		m_regInfoJob->Cancel();
		m_regInfoJob.reset();
	}
	if (m_btConfigJob.get() != NULL) {
		m_btConfigJob->Cancel();
		m_btConfigJob.reset();
	}
	Hide(false);
	while (m_items.size() != 0) {
		IndicatorItem *item = m_items[0];

		m_items.erase(m_items.begin(), m_items.begin() + 1);
		if (item != NULL)
			delete item;
	}
	g_indicator = NULL;
}

/* (0x8115E6B2) */
void indicatorDestroy(void)
{
	indicatorTelephonyUnlisten();
	if (g_indicator != NULL)
		delete g_indicator;
}

/* the bar laid out for the mode, shown, and slid in from above (0x8115E6CE) */
int Indicator::Show(IndicatorMode mode, bool animate)
{
	int ret = -1;
	ui::Widget *root = indicatorFindWidget(IDParam(s_empty_name, INDICATOR_PAGE),
					       IDParam(s_empty_name, INDICATOR_ROOT));

	if (root != NULL) {
		SetMode(mode, animate);
		m_status.visible = true;
		SetStatus(m_status);

		float time = animate ? INDICATOR_HIDE_TIME : 0.0f;

		if (time > 0.0f)
			indicatorShowWidget(root, new Timer(time));
		else
			indicatorShowWidget(root, NULL);
		root->GetSize(0);

		math::v4 pos = root->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		ui::SceneResolution *res = ScePafWidget_CDC90BA2(root->m_scene->m_env_param,
								 root->m_scene->m_resolution_id);

		if (res != NULL) {
			ui::ResolutionData data[2] = {
				{ INDICATOR_RES_QHD, { 0.0f, 0.0f, 0.0f } },
				{ INDICATOR_RES_HD, { 0.0f, 0.0f, 0.0f } },
			};

			data[0].m_value[0] = pos.extract_x();
			data[0].m_value[1] = res->m_scissor_height * 0.5f;
			data[0].m_value[2] = pos.extract_z();
			data[1].m_value[0] = pos.extract_x();
			data[1].m_value[1] = res->m_scissor_width * 0.5f;
			data[1].m_value[2] = pos.extract_z();
			root->KillAnim(INDICATOR_ANIM_HIDE);
			ret = root->SetPos(data, 2,
					   animate ? new Timer(INDICATOR_HIDE_TIME, Timer::FUNC_QUAD_IN)
						   : NULL,
					   INDICATOR_ANIM_HIDE, 0, 0.0f, NULL, NULL);
		}
	}
	return ret;
}

/* the interface's function 0 (0x8115EABC) */
int indicatorShow(IndicatorMode mode, bool animate)
{
	int ret = -1;

	if (g_indicator != NULL)
		ret = g_indicator->Show(mode, animate);
	return ret;
}

/* the interface's function 1 (0x8115EAF8) */
int indicatorHide(bool animate)
{
	int ret = -1;

	if (g_indicator != NULL)
		ret = g_indicator->Hide(animate);
	return ret;
}

/* the interface's function 4 (0x8115EB14) */
int indicatorSetValue4(int value)
{
	Indicator *indicator = g_indicator;
	int ret = -1;

	if (indicator != NULL) {
		indicator->m_status.value4 = value;
		indicator->SetStatus(indicator->m_status);
		ret = 0;
	}
	return ret;
}

/* the interface's function 8 (0x8115EC12) */
int indicatorSetValue8(int value)
{
	Indicator *indicator = g_indicator;
	int ret = -1;

	if (indicator != NULL) {
		indicator->m_status.value8 = value;
		indicator->SetStatus(indicator->m_status);
		ret = 0;
	}
	return ret;
}

/* the interface's function 5: the flight mode's icon slid in or out (0x8115ED12) */
int indicatorSetFlightMode(int on)
{
	Indicator *indicator = g_indicator;
	int ret = -1;

	if (indicator != NULL) {
		indicator->m_status.flightMode = on;
		indicator->SetMode(indicator->m_mode, true);
		indicator->SetStatus(indicator->m_status);
		ret = 0;
	}
	return ret;
}

/* the interface's function 6: the counters swapped (0x8115EE28) */
int indicatorSetValue6(int value)
{
	Indicator *indicator = g_indicator;
	int ret = -1;

	if (indicator != NULL) {
		indicator->m_status.value6 = value;
		indicator->SetMode(indicator->m_mode, true);
		indicator->SetStatus(indicator->m_status);
		ret = 0;
	}
	return ret;
}

/* the interface's function 7 (0x8115EF3E) */
int indicatorSetBluetooth(int on)
{
	Indicator *indicator = g_indicator;
	int ret = -1;

	if (indicator != NULL) {
		indicator->m_status.bluetooth = on;
		indicator->SetStatus(indicator->m_status);
		ret = 0;
	}
	return ret;
}

/* the interface's functions 9 and 10 (0x8115F040) */
int indicatorUnsupported(void)
{
	return -1;
}

/* the interface's function 11: kept, the items not told (0x8115F046) */
int indicatorSetNotification(int on)
{
	int ret = -1;

	if (g_indicator != NULL) {
		ret = 0;
		g_indicator->m_status.notification = on;
	}
	return ret;
}

/* (0x8115F05E) */
int indicatorGetNotification(int *on)
{
	int ret = -1;

	if (g_indicator != NULL) {
		ret = 0;
		*on = g_indicator->m_status.notification;
	}
	return ret;
}

static void indicatorOnUpdateTheme(void *data);

/* the interface's function 31 (0x8115F07A) */
void indicatorUpdateTheme(void)
{
	thread::CallQueue::main_thread_callqueue.Enqueue("updateTheme", indicatorOnUpdateTheme, NULL);
}

/* (0x8115F09C) */
bool Indicator::IsVisible()
{
	return m_status.visible;
}

/* the interface's function 3 (0x8115F0A2) */
bool indicatorIsVisible(void)
{
	bool ret = false;

	if (g_indicator != NULL)
		ret = g_indicator->m_status.visible;
	return ret;
}

/* shown, in the mode of type 0 (0x8115F0B8) */
bool indicatorIsShownNormal(void)
{
	bool ret = false;

	if (g_indicator != NULL)
		ret = g_indicator->m_status.visible && g_indicator->m_mode.type == 0;
	return ret;
}

/* the flight mode's icon slid in: the first counter's icons moved back by it, shown (0x8115F0D8) */
static void indicatorOnFlightModeSlid(Timer *t, ui::AnimListener *listener, void *widgetArg,
				      void *userArg, void *extraArg)
{
	(void)userArg;
	(void)extraArg;
	ui::Widget *icons = (ui::Widget *)widgetArg;

	if (icons == NULL)
		return;

	ui::ResolutionData pos[2] = {
		{ INDICATOR_RES_QHD, { -440.0f, 0.0f, 0.0f } },
		{ INDICATOR_RES_HD, { -232.0f, 0.0f, 0.0f } },
	};

	icons->SetPos(pos, 2, NULL, INDICATOR_ANIM_SLIDE, 0, 0.0f, NULL, NULL);
	indicatorShowWidget(icons, new Timer(200.0f));
}

/* two strings the same, by their characters only */
static inline bool indicatorStringIs(const string &left, const string &right)
{
	return sce_paf_strcmp(left.c_str(), right.c_str()) == 0;
}

/* the registry values read: the flight mode's and the notifications' settings taken, the bar laid
   out again (0x8115F1D4) */

void Indicator::OnRegInfo(const list<IndicatorRegKey> &keys, void *data)
{
	for (list<IndicatorRegKey>::iterator it = keys.begin(); it != keys.end(); ++it) {
		if (it->result != 0)
			continue;

		bool flightMode = indicatorStringIs(it->path, string("/CONFIG/SYSTEM")) &&
				  indicatorStringIs(it->name, string("flight_mode"));

		if (flightMode) {
			m_status.flightMode = it->value;
			m_flightModeIconOn = -1;
		}

		bool notification = indicatorStringIs(it->path, string("/CONFIG/SHELL")) &&
				    indicatorStringIs(it->name, string("notification"));

		if (notification)
			m_status.notification = it->value;
	}
	m_regInfoJob.reset();
	SetMode(m_mode, false);
	SetStatus(m_status);
}

/* (0x8115F490) */
static void indicatorOnRegInfo(const list<IndicatorRegKey> &keys, void *data)
{
	if (g_indicator != NULL)
		g_indicator->OnRegInfo(keys, data);
}

/* the Bluetooth configuration read: on when read and its bits 0 or 3 set (0x8115F4A8) */
void Indicator::OnBtConfig(int result, int config, void *data)
{
	m_btConfigJob.reset();
	m_status.bluetooth = result >= 0 && config >= 0 && (config & 9) != 0;
	SetStatus(m_status);
}

/* (0x8115F618) */
static void indicatorOnBtConfig(int result, int config, void *data)
{
	if (g_indicator != NULL)
		g_indicator->OnBtConfig(result, config, data);
}

/* the theme's look taken again: the textures dropped and asked for again (0x8115F634) */
static void indicatorOnUpdateTheme(void *data)
{
	indicatorNewEventDropTextures(g_indicatorNewEvent);
	indicatorCacheMakeSpace();
	indicatorNewEventReloadTheme(g_indicatorNewEvent);

	Indicator *indicator = g_indicator;

	if (indicator != NULL) {
		indicator->SetMode(indicator->m_mode, false);
		indicator->SetStatus(indicator->m_status);
	}
}
