/*
 * applauncher_iconeffect.cpp: what moves the home's icons on their own (0x810CC8D0-
 * 0x810CF650): the wobble of an icon's view while the home is shown, the edit's tilt and the
 * drop of a view into its slot, the launch effect that flies a tapped icon's copy toward the
 * viewer and spins it into the LiveArea, and the pages' sway while the wave fades in. The
 * file's name and the names here are ours (PLACEHOLDER): no record of the 3.6x shell names
 * them.
 */
#include <stddef.h>
#include <math.h>
#include <arm_neon.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/stdlib.h>
#include <std/math.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <math/ngp/quaternion.h>
#include <math/ngp/matrix.h>
#include <system/system.h>
#include <timer/timer.h>
#include <graphics/drawobj/draw_obj.h>
#include <widget/core/widget.h>
#include <widget/w_appicon2d.h>
#include <widget/w_scene.h>
#include <widget/w_scroll_view_base.h>
#include <common/shared_ptr.h>
#include "../../../shell.h"
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"
#include "applauncher_home.h"

using namespace paf;
using sce::Vectormath::Simd::floatInVec;

typedef sce::Vectormath::Simd::Aos::Vector3 Vector3;
typedef sce::Vectormath::Simd::Aos::Vector4 Vector4;

/* the closest a rotation is taken as none, and the epsilon of the rotations' conversions */
#define ICON_EFFECT_ROT_EPSILON		1.0e-5f
#define ICON_EFFECT_EPSILON		1.0e-4f

/* PLACEHOLDER: the order the slots' views drop in, in steps of 25 ms (0x81507DEC) */
static float s_dropOrder[] = { 6.0f, 1.0f, 7.0f, 5.0f, 0.0f, 2.0f, 8.0f, 4.0f, 3.0f, 9.0f };

/* PLACEHOLDER: the launch effect, while the home has one (0x81544354) */
class ApplauncherLaunchEffect;
static ApplauncherLaunchEffect *s_launchEffect;

/* the seconds since the last frame */
static inline float frameTime(void)
{
	return Timer::s_tick_unit * Timer::s_tick_interval;
}

/* the unit vector along xyz, its w kept */
static inline math::v4 iconEffectNormalize3(math::v4 v)
{
	Vector3 xyz = v.getImpl().getXYZ();

	v.getImpl().setXYZ(xyz * sce::Vectormath::Simd::recipf(length(xyz)));
	return v;
}

/* PLACEHOLDER: the same, out of line where the wobble calls it (Sony's copy 0x81412B44) */
inline __attribute__((noinline)) math::v4 iconEffectUnit3(math::v4 v)
{
	return iconEffectNormalize3(v);
}

/* PLACEHOLDER: the cross product of the xyz, a's w kept (Sony's copy out of line 0x81412BD8) */
inline __attribute__((noinline)) math::v4 iconEffectCross3(math::v4 a, math::v4 b)
{
	return math::v4(Vector4(cross(a.getImpl().getXYZ(), b.getImpl().getXYZ()).get128()));
}

/* PLACEHOLDER: the identity, out of line where the wobble asks for it (Sony's copy
   0x81412AF0) */
inline __attribute__((noinline)) math::matrix iconEffectIdentity(void)
{
	return math::matrix::Identity();
}

/* whether the rotations are within ICON_EFFECT_ROT_EPSILON of each other in every lane */
static inline bool iconEffectRotNear(const math::quaternion &a, const math::quaternion &b)
{
	uint32x4_t near = vcaleq_f32(vsubq_f32(a.getImpl().get128(), b.getImpl().get128()),
				     vdupq_n_f32(ICON_EFFECT_ROT_EPSILON));
	uint32x2_t lanes;

	near = vandq_u32(near, vrev64q_u32(near));
	lanes = vand_u32(vget_low_u32(near), vget_high_u32(near));
	return vget_lane_u32(vdup_lane_u32(lanes, 0), 0) != 0;
}

/* the depth's part of the way from the edit's depth at the start to its depth now */
static inline float iconEffectDepthScale(float start, float now)
{
	float scale = 0.0f;

	if (start - g_applauncherEditDepth != 0.0f)
		scale = (now - g_applauncherEditDepth) / (start - g_applauncherEditDepth);
	return scale;
}

/* still and upright (0x810CC8D0) */
void ApplauncherIconWobble::Reset()
{
	m_bob = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	m_angle = 0.0f;
	m_fling = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	m_normal = math::v4(0.0f, 0.0f, 1.0f, 0.0f);
	m_spin = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	m_move = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	sce_paf_rand();
	m_phase = 0.0f;
	m_limit = 1.0f;
}

/* (0x810CC914) */
ApplauncherIconEffect::ApplauncherIconEffect()
{
}

/* the widget fallen back to the page's plane and bounced, its tilt turned back with each
   bounce (0x810CC946) */
void ApplauncherIconTilt::Update(ApplauncherIconView *view)
{
	ui::AppIconBase *widget = view->m_widget;
	ApplauncherIcon *icon = view->m_owner.get();
	float dt = frameTime();
	float y = icon->m_page->GetWidget()->GetWorldMatrix().w().extract_z();

	if (!(g_applauncherHome->m_fadeTime < 0.2f)) {
		math::quaternion target;

		if (y < m_z) {
			m_speed = m_speed + dt * -10000.0f;
			m_z = m_z + m_speed * dt;
			target = m_target;
		} else {
			m_z = y;
			m_speed = m_speed * -0.75f;
			if (__builtin_fabsf(m_speed) < 0.1f)
				m_speed = 0.0f;
			if (iconEffectRotNear(m_rot, math::quaternion::Identity())) {
				m_target = math::quaternion::Identity();
				target = math::quaternion::Identity();
			} else {
				math::v3 euler = m_rot.inverse().toMatrix(ICON_EFFECT_EPSILON)
							.extractEulerAnglesYXZ();
				math::v3 axis;
				float angle;

				euler.set_z(0.0f);
				math::quaternion::FromEulerAnglesYXZ(euler)
					.toAxisAngle(axis, angle);
				angle = angle * __builtin_fabsf(m_speed);
				if (angle > 2.3561945f)
					angle = 2.3561945f;

				float half = angle * 0.5f;
				Vector3 sin = axis.getImpl() * sce_paf_sinf(half);

				target = math::quaternion(
					sce::Vectormath::Simd::Aos::Quat(sin, sce_paf_cosf(half)));
				m_target = target;
			}
		}
		if (!iconEffectRotNear(target, math::quaternion::Identity())) {
			math::quaternion step =
				math::quaternion::Slerp(math::quaternion::Identity(), target,
							0.9999f, ICON_EFFECT_EPSILON);

			m_target = step;
			step = math::quaternion::Slerp(math::quaternion::Identity(), step, dt,
						       ICON_EFFECT_EPSILON);
			m_rot = m_rot * step;
		}
		if (!iconEffectRotNear(m_rot, math::quaternion::Identity())) {
			float scale = iconEffectDepthScale(m_pos.extract_z(), m_z);
			math::v3 euler =
				m_rot.toMatrix(ICON_EFFECT_EPSILON).extractEulerAnglesYXZ();

			euler.set_z(euler.extract_z() * scale);
			m_rot = math::quaternion::FromEulerAnglesYXZ(euler);
		}
	}

	float scale = iconEffectDepthScale(m_pos.extract_z(), m_z);
	float x = m_pos.extract_x() * scale;
	float z = -y + m_z;
	math::v4 pos = m_pos;

	pos.set_x(x);
	pos.set_y(m_pos.extract_y() * scale);
	pos.set_z(z);
	widget->GetDrawObj(0)->SetPos(math::v3(pos.getImpl().getXYZ()));

	math::v3 euler = m_rot.toMatrix(ICON_EFFECT_EPSILON).extractEulerAnglesYXZ();

	widget->GetDrawObj(0)->SetRot(euler);
	widget->m_focus_shape->m_offset = math::v3(x, pos.extract_y(), z);
}

/* the widget tipped about its normal, the page's moves pushing it, and bobbed while its icon
   is selected (0x810CCDFE) */
void ApplauncherIconWobble::Update(ApplauncherIconView *view)
{
	if (((int (*)(void))g_applauncherHome->m_topmenu->func[
			TOPMENU_PLUGIN_FUNC_IS_WOBBLE_ON])() == 0)
		return;
	if (view->m_owner->m_page == NULL)
		return;

	common::SharedPtr<ApplauncherIcon> icon = view->m_owner;
	ui::AppIconBase *widget = view->m_widget;
	float dt = frameTime();
	float frames = dt / (1.0f / 60.0f);
	math::matrix local = widget->GetLocalMatrix();
	math::matrix inverse = local.inverse();
	math::matrix page = iconEffectIdentity();
	ApplauncherPage *owner = icon->m_page;
	bool selected = icon->m_selected;

	if (owner->m_type == 0) {
		page.assign(owner->m_sway.m_matrix);
		page.assign(owner->m_sway.m_matrix);
	}

	math::matrix bob = iconEffectIdentity();
	math::v4 delta(0.0f, 0.0f, 0.0f, 0.0f);

	owner = icon->m_page;
	if (owner->m_type == 0)
		delta = owner->m_pos - owner->m_prevPos;

	math::v4 move(delta.extract_x(), delta.extract_y(), 0.0f, 0.0f);
	math::v4 step = move / frames;

	if ((m_move.len() - step.len()).getAsFloat() > 1000.0f) {
		m_fling = m_move;
		if (m_move.len() > floatInVec(4000.0f))
			m_fling = iconEffectUnit3(m_move) * 4000.0f;
		m_move = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	} else {
		m_move = step;
	}

	math::v4 sway;

	if (m_fling.len() > floatInVec(60.0f)) {
		m_fling = m_fling * sce_paf_powf(0.4f, frames);
		sway = m_bob + m_fling * dt;
	} else {
		sway = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		if (m_bob.len() > floatInVec(0.1f)) {
			float k = sce_paf_powf(0.9f, frames);

			m_fling = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
			sway = m_bob * k;
		}
	}
	m_spin = m_spin + move;
	sway.set_w(1.0f);
	m_bob = sway;
	m_spin = m_spin * sce_paf_powf(0.95f, frames);
	m_angle = m_angle * sce_paf_powf(0.95f, frames);
	if (m_spin.dot(m_spin) > floatInVec(10000.0f))
		m_spin = iconEffectUnit3(m_spin) * 100.0f;

	math::v4 tilt = math::v4(0.0f, 0.0f, 1.0f, 0.0f) - m_normal;

	tilt.set_z(0.0f);
	if (m_normal.getImpl().getZ() < floatInVec(0.999999f)) {
		math::v4 dir = iconEffectUnit3(tilt);
		float force = sce_paf_sinf(sce_paf_acosf(m_normal.extract_z())) * 300.0f;
		float s = sce_paf_sinf(m_angle);
		float c = sce_paf_cosf(m_angle);
		math::matrix turn(math::v4(c, s, 0.0f, 0.0f), math::v4(-s, c, 0.0f, 0.0f),
				  math::v4(0.0f, 0.0f, 1.0f, 0.0f),
				  math::v4(0.0f, 0.0f, 0.0f, 1.0f));
		math::v4 push = iconEffectUnit3(math::v4(turn.getImpl() * dir.getImpl())) * force;

		m_spin = m_spin + push * dt;
	} else {
		m_angle = 0.0f;
	}
	m_normal = iconEffectUnit3(m_normal + m_spin * dt);

	float limit = m_limit * 0.174532935f;

	if (sce_paf_cosf(limit + 0.017453292f) > m_normal.extract_z()) {
		if (m_angle == 0.0f)
			m_angle = ((sce_paf_rand() / 32767.0f - 0.5f) * 2.0f) * 3.1415927f * 0.25f;

		math::v4 side = iconEffectUnit3(iconEffectCross3(m_normal,
						math::v4(1.0f, 0.0f, 0.0f, 0.0f)));
		math::v4 front = iconEffectUnit3(iconEffectCross3(side, m_normal));
		math::matrix basis(front, side, m_normal, math::v4(0.0f, 0.0f, 0.0f, 1.0f));
		float angle = sce_paf_acosf(m_normal.extract_z());
		math::quaternion q(basis);
		math::matrix r = math::quaternion::Slerp(math::quaternion::Identity(), q,
			limit / angle, ICON_EFFECT_EPSILON).toMatrix(ICON_EFFECT_EPSILON);

		m_spin = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		m_normal = r.z();
	}

	math::v4 side = iconEffectUnit3(iconEffectCross3(m_normal,
							 math::v4(1.0f, 0.0f, 0.0f, 0.0f)));
	math::v4 front = iconEffectUnit3(iconEffectCross3(side, m_normal));

	bob = math::matrix(front, side, m_normal, m_bob);

	math::v4 lift(0.0f, 0.0f, 0.0f, 1.0f);

	if (selected) {
		float height;

		m_phase = m_phase + dt;
		while (m_phase > 0.5f)
			m_phase = m_phase - 0.5f;
		if (m_phase <= 0.25f)
			height = Timer::DoFuncSineOut(m_phase * 4.0f);
		else
			height = 1.0f - Timer::DoFuncSineIn((m_phase - 0.25f) * 4.0f);
		lift.set_z(height * 150.0f);
	}
	if (selected)
		page = math::matrix::Identity();

	math::matrix world = math::matrix(math::v4(1.0f, 0.0f, 0.0f, 0.0f),
					  math::v4(0.0f, 1.0f, 0.0f, 0.0f),
					  math::v4(0.0f, 0.0f, 1.0f, 0.0f), lift) *
			     inverse * page * local * bob;

	widget->GetDrawObj(0)->SetPos(math::v3(world.w().getImpl().getXYZ()));

	math::v3 euler = world.extractEulerAnglesYXZ();

	widget->GetDrawObj(0)->SetRot(euler);
	if (widget->m_focus_shape != NULL) {
		widget->m_focus_shape->m_offset = math::v3(world.w().getImpl().getXYZ());
		widget->m_focus_shape->m_rot = world.extractEulerAnglesYXZ();
	}
}

/* (0x810CDA62) */
void ApplauncherIconEffect::Update(ApplauncherIconView *view)
{
	int mode = g_applauncherHome->m_mode;

	if (mode == 0)
		m_wobble.Update(view);
	else if (mode == 1 && view->m_owner->m_page != NULL)
		m_tilt.Update(view);
}

/* (0x810CDA8C) */
void ApplauncherIconTilt::Capture(ApplauncherIconView *view)
{
	ui::AppIconBase *widget = view->m_widget;
	math::v3 pos = widget->GetDrawObj(0)->m_pos;

	m_pos = math::v4(Vector4(pos.getImpl().get128()));

	math::v3 *rot = widget->GetDrawObj(0)->m_rot;

	m_rot = math::quaternion::FromEulerAnglesYXZ(rot != NULL ? *rot : math::v3(0.0f));
	m_target = math::quaternion::Identity();
	m_speed = 0.0f;
	m_z = m_pos.extract_z();
}

/* (0x810CDB28) */
void ApplauncherIconEffect::Capture(ApplauncherIconView *view)
{
	m_tilt.Capture(view);
}

/* the widget put high above its slot and dropped into it, in the slot's turn (0x810CDB32) */
void ApplauncherIconDrop::Begin(ApplauncherIconView *view)
{
	ui::AppIconBase *widget = view->m_widget;
	math::v4 target(0.0f, 0.0f, 0.0f, 0.0f);
	math::v4 pos = widget->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	m_time = 0.0f;

	ApplauncherPage *page = view->m_owner->m_page;

	if (page != NULL) {
		target.set_x(page->GetSlotPos(view->m_owner->m_slot).extract_x());
		target.set_y(view->m_owner->m_page->GetSlotPos(view->m_owner->m_slot).extract_y());
	}
	pos.set_z(1000.0f);
	widget->SetPos(pos, NULL, ui::Widget::ANIM_MOVE, 0, 0.0f);
	widget->SetPos(target, new Timer(500.0f, Timer::FUNC_BACK_OUT, 0, -1, NULL),
		       ui::Widget::ANIM_MOVE, 0, s_dropOrder[view->m_owner->m_slot] * 25.0f);
}

/* (0x810CDCB2) */
void ApplauncherIconEffect::DropIn(ApplauncherIconView *view)
{
	m_drop.Begin(view);
}

/* (0x810CDCBC) */
__attribute__((noinline)) void ApplauncherIconTilt::Begin(ApplauncherIconView *view)
{
	float depth = g_applauncherEditDepth;

	m_pos = math::v4(0.0f, 0.0f, depth, 0.0f);
	m_z = depth;
	m_speed = 0.0f;
	m_rot = math::quaternion::Identity();
	m_target = math::quaternion::Identity();
}

/* (0x810CDCFA) */
void ApplauncherIconEffect::Begin(ApplauncherIconView *view)
{
	m_tilt.Begin(view);
}

/* (0x810CDD04) */
void ApplauncherIconEffect::Reset()
{
	m_wobble.Reset();
}

/*
 * PLACEHOLDER: the launch effect: a copy of the tapped icon's widget flies toward the viewer
 * (state 1), falls back spinning (2), turns to face the LiveArea and sinks into it (3); a
 * launch cancelled sends it back to the icon's place (4) (136 bytes)
 */
class ApplauncherLaunchEffect {
public:
	/* (0x810CDD0C, 0x810CDDEA) */
	ApplauncherLaunchEffect();
	~ApplauncherLaunchEffect();

	/* PLACEHOLDER names: taken down, the icon's widget shown again (0x810CDE88), begun for
	   the icon (0x810CDF7C), and moved on a frame (0x810CED0E) */
	void End();
	void Begin(const common::SharedPtr<ApplauncherIcon> &icon);
	void Update();
	/* PLACEHOLDER names: each state moved to the time, the frame's seconds given (states 4, 3,
	   2 and 1: 0x810CE518, 0x810CE676, 0x810CE9BC, 0x810CEB52) */
	void UpdateReturn(float time, float dt);
	void UpdateSink(float time, float dt);
	void UpdateFall(float time, float dt);
	void UpdateRise(float time, float dt);

	ui::AppIcon2d *m_widget;
	common::SharedPtr<ApplauncherIcon> m_icon;
	string m_reserved;
	SceUInt32 m_state;
	float m_time;				/* seconds into the state */
	math::v4 m_pos;
	math::v4 m_velocity;			/* per second, on the last frame */
	math::quaternion m_rot;
	math::v4 m_startPos;			/* where the state began */
	math::v4 m_launchPos;			/* where it sinks to */
	math::quaternion m_startRot;
	float m_spin;				/* seconds into the fall's spin */
};

/* the copy's widget made on the home's scene, hidden (0x810CDD0C) */
ApplauncherLaunchEffect::ApplauncherLaunchEffect()
	: m_widget(new ui::AppIcon2d(g_applauncherHome->m_scroller.m_scrollView->m_scene, NULL)),
	  m_state(0)
{
	ui::AppIcon2d *widget = m_widget;

	widget->m_hit_detect_b_mode = 2;
	widget->m_hit_detect_mode = 2;
	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);
	m_widget->EnableFocusEvent(false);
}

/* (0x810CDDCA) */
void applauncherLaunchEffectCreate(void)
{
	s_launchEffect = new ApplauncherLaunchEffect;
}

/* (0x810CDDEA) */
ApplauncherLaunchEffect::~ApplauncherLaunchEffect()
{
	m_widget->DestroyWidget();
}

/* (0x810CDE64) */
void applauncherLaunchEffectDestroy(void)
{
	delete s_launchEffect;
	s_launchEffect = NULL;
}

/* (0x810CDE88) */
void ApplauncherLaunchEffect::End()
{
	ui::Widget *widget = m_icon->GetWidget();

	if (widget != NULL)
		widget->GetDrawObj(0)->SetVisible(true);

	ui::AppIcon2d *copy = m_widget;

	copy->m_transition_complete = false;
	copy->m_transition = 0xff;
	copy->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(copy, NULL, 2, 0.0f, NULL);
	((void (*)(void))g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_LAUNCH_END])();
	m_icon.reset();
}

/* the copy put over the icon's widget, as it is drawn, and the icon's widget hidden
   (0x810CDF7C) */
void ApplauncherLaunchEffect::Begin(const common::SharedPtr<ApplauncherIcon> &icon)
{
	if (m_state != 0) {
		End();
		m_state = 0;
	}
	if (icon->GetWidget() == NULL)
		return;

	ui::Widget *widget = icon->GetWidget();

	m_icon = icon;
	m_state = 1;
	m_time = 0.0f;

	math::matrix world = widget->GetWorldMatrix();
	math::v3 *rot = widget->GetDrawObj(0)->m_rot;
	math::v3 euler = math::v3(0.0f);

	if (rot != NULL)
		euler = *rot;

	math::v3 pos = widget->GetDrawObj(0)->m_pos;
	math::matrix turn = math::quaternion::FromEulerAnglesYXZ(euler)
				.toMatrix(ICON_EFFECT_EPSILON);
	math::matrix pose = world * math::matrix(turn.x(), turn.y(), turn.z(), math::v4(pos, 1.0f));

	m_pos = pose.w();
	m_startPos = pose.w();
	pose.set_w(math::v4(0.0f, 0.0f, 0.0f, 1.0f));
	m_rot = math::quaternion(sce::Vectormath::Simd::Aos::Quat(
		math::v4(Vector4(math::quaternion(pose).getImpl())).unit().getImpl().get128()));

	ui::AppIcon2d *copy = m_widget;

	if (!copy->m_transition_complete) {
		copy->m_transition = 0xff;
		copy->m_transition_complete = true;
		copy->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(copy, NULL, 2, 0.0f, NULL);
	m_widget->SetSize(*widget->GetSize(0));
	m_widget->SetMetaAlpha(1.0f);

	intrusive_ptr<graph::Surface> texture;

	widget->GetDrawObj(0)->GetTexture(texture, 0);
	m_widget->SetTexture(texture, 0, 0);
	m_widget->SetLabelString(wstring(L""));

	math::v4 at = m_pos;

	at.set_w(1.0f);
	m_widget->SetPos(at);
	m_widget->SetRot(math::v4(pose.extractEulerAnglesYXZ(), 0.0f), 0, NULL,
			 ui::Widget::ANIM_ROT, 0, 0.0f, NULL, NULL);
	m_spin = 0.0f;
	widget->GetDrawObj(0)->SetVisible(false);

	float time = 300.0f;

	if (system::Model::GetType() == 1)
		time = time * 2.0f;

	LsdbIcon *data = m_icon->m_data.get();

	if (data->type == 2) {
		((void (*)(const char *))g_applauncherHome->m_topmenu->func[
			TOPMENU_PLUGIN_FUNC_SET_LAUNCH_TITLE])("NPXS10003");
		((void (*)(float))g_applauncherHome->m_topmenu->func[
			TOPMENU_PLUGIN_FUNC_LAUNCH_BEGIN])(time);
	} else {
		((void (*)(const char *))g_applauncherHome->m_topmenu->func[
			TOPMENU_PLUGIN_FUNC_SET_LAUNCH_TITLE])(data->titleId.c_str());
		((void (*)(float))g_applauncherHome->m_topmenu->func[
			TOPMENU_PLUGIN_FUNC_LAUNCH_BEGIN])(time);
	}
}

/* (0x810CE474) */
void applauncherLaunchEffectBegin(const common::SharedPtr<ApplauncherIcon> &icon)
{
	s_launchEffect->Begin(icon);
}

/* the title's launch cancelled: its copy sent back to the icon's place (0x810CE48A) */
void applauncherIconEffectEnd(const string &titleId)
{
	ApplauncherLaunchEffect *effect = s_launchEffect;

	if (effect->m_icon.get() == NULL)
		return;
	if (titleId == effect->m_icon->m_data->titleId && effect->m_state != 0) {
		effect->m_time = 0.0f;
		effect->m_state = 4;
		effect->m_startPos = effect->m_pos;
		effect->m_startRot = effect->m_rot;
	}
}

/* (0x810CE4E2) */
void applauncherIconEffectEndAll(void)
{
	ApplauncherLaunchEffect *effect = s_launchEffect;

	if (effect->m_state != 0) {
		effect->m_state = 0;
		effect->m_time = 0.0f;
		effect->m_startPos = effect->m_pos;
		effect->m_startRot = effect->m_rot;
	}
}

/* (0x810CE518) */
void ApplauncherLaunchEffect::UpdateReturn(float time, float dt)
{
	float rate = time / 0.6f;
	float startZ = m_startPos.extract_z();
	float far = 650.0f - startZ;

	m_pos.set_z(startZ + far * Timer::DoFuncCubicOut(rate));

	math::v4 start = m_startPos;
	float eased = Timer::DoFuncCubicOut(rate);

	m_pos.set_x(start.extract_x() - start.extract_x() * eased);
	m_pos.set_y(start.extract_y() - start.extract_y() * eased);
	m_rot = math::quaternion::Slerp(m_startRot, math::quaternion::Identity(),
					Timer::DoFuncCubicOut(rate), ICON_EFFECT_EPSILON);
	m_widget->SetMetaAlpha(1.0f - Timer::DoFuncCubicOut(m_time / 0.6f), NULL,
			       ui::Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
}

/* (0x810CE676) */
void ApplauncherLaunchEffect::UpdateSink(float time, float dt)
{
	float speed = Timer::DoFuncSineIn(1.0f - time / 1.5f) * 4.0f + 1.0f;
	math::quaternion turn = math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, 0.0f,
						dt * 3.1415927f * speed));
	math::quaternion rot = turn * m_startRot;
	math::matrix m = rot.toMatrix(ICON_EFFECT_EPSILON);
	math::v4 x = iconEffectCross3(math::v4(0.0f, 1.0f, 0.0f, 0.0f), m.z());
	math::v4 y = iconEffectCross3(m.z(), x);

	m = math::matrix(iconEffectNormalize3(x), iconEffectNormalize3(y),
			 iconEffectNormalize3(m.z()), m.w());
	m_startRot = math::quaternion(m);
	m_rot = m_startRot;

	math::v4 dir = m_rot.toMatrix(ICON_EFFECT_EPSILON).z();
	math::v4 back = m_launchPos - dir * 100.0f;
	math::v4 pos = m_launchPos + dir * 20.0f;

	pos.set_z(pos.extract_z() + ((back + dir * 100.0f).extract_z() - back.extract_z()));
	m_pos = pos;
}

/* the spin's angle on the frame, after its ease over 1.111 seconds, within -pi and pi */
static inline float launchEffectSpin(float *spin, float dt)
{
	float angle = 12.566371f * Timer::DoFuncSineOut((*spin + dt) / 1.1110001f) -
		      Timer::DoFuncSineOut(*spin / 1.1110001f) * 12.566371f;

	*spin = *spin + dt;
	while (angle >= 3.1415927f)
		angle = angle - 6.2831855f;
	return angle;
}

/* (0x810CE9BC) */
void ApplauncherLaunchEffect::UpdateFall(float time, float dt)
{
	float startZ = m_startPos.extract_z();
	float far = -100.0f - startZ;

	m_pos.set_z(startZ + far * Timer::DoFuncCirqIn(time * 2.0f));
	m_pos.set_x(m_pos.extract_x() + m_velocity.extract_x() * dt);
	m_pos.set_y(m_pos.extract_y() + m_velocity.extract_y() * dt);

	float angle = launchEffectSpin(&m_spin, dt);

	m_rot = m_rot * math::quaternion::FromEulerAnglesYXZ(math::v3(dt * 0.0f, angle, 0.0f));
}

/* (0x810CEB52) */
void ApplauncherLaunchEffect::UpdateRise(float time, float dt)
{
	float rate = time / 0.611f;
	float startZ = m_startPos.extract_z();
	float far = 650.0f - startZ;

	m_pos.set_z(startZ + far * Timer::DoFuncBackOut(rate));

	math::v4 start = m_startPos;
	float eased = Timer::DoFuncSineOut(rate);

	m_pos.set_x(start.extract_x() - start.extract_x() * eased);
	m_pos.set_y(start.extract_y() - start.extract_y() * eased);

	float angle = launchEffectSpin(&m_spin, dt);

	m_rot = m_rot * math::quaternion::FromEulerAnglesYXZ(math::v3(dt * 0.0f, angle, 0.0f));
}

/* (0x810CED0E) */
void ApplauncherLaunchEffect::Update()
{
	if (m_icon.get() == NULL)
		return;

	float dt = frameTime();
	math::v4 old = m_pos;
	float start = m_time;

	m_time = m_time + dt;
	switch (m_state) {
	case 1:
		if (m_time > 0.611f) {
			float rest = 0.611f - start;

			UpdateRise(0.611f, rest);
			dt = dt - rest;
			m_state = 2;
			m_time = m_time - 0.611f;
			m_startPos = m_pos;
		}
		break;
	case 2:
		if (m_time > 0.5f) {
			float rest = 0.5f - start;

			UpdateFall(0.5f, rest);
			dt = dt - rest;
			m_state = 3;
			m_time = m_time - 0.5f;

			math::v4 up = iconEffectNormalize3(m_rot.toMatrix(ICON_EFFECT_EPSILON).z());

			if (up.extract_z() >= 0.9999f)
				m_rot = m_rot * math::quaternion::FromEulerAnglesYXZ(
					math::v3(0.0872664675f, 0.0872664675f, 0.0f));
			m_startRot = m_rot;
			m_startPos = m_pos;

			math::v4 dir =
				iconEffectNormalize3(m_rot.toMatrix(ICON_EFFECT_EPSILON).z());
			math::v4 near = m_pos - dir * 20.0f;
			math::v4 far = (near + dir * 100.0f) -
				       math::v4(0.0f, 0.0f, 1.0f, 0.0f) * 100.0f;

			sce_paf_acosf(dir.extract_z());

			math::v4 diff = near - far;

			diff.set_z(0.0f);
			if (diff.extract_x() != 0.0f || diff.extract_y() != 0.0f)
				diff = math::v4(diff.getImpl() * sce::Vectormath::Simd::recipf(
					length(diff.getImpl().getXYZ())));

			math::v4 launch(mulPerElem(diff.getImpl(), diff.getImpl()));

			launch.set_z(far.extract_z());
			m_launchPos = launch;
			((void (*)(const char *))g_applauncherHome->m_topmenu->func[
				TOPMENU_PLUGIN_FUNC_SET_LAUNCH_TITLE])(
				m_icon->m_data->titleId.c_str());
			((void (*)(void))g_applauncherHome->m_topmenu->func[
				TOPMENU_PLUGIN_FUNC_LAUNCH_FIRE])();
		}
		break;
	case 3:
		break;
	case 4:
		if (m_time > 0.6f) {
			m_state = 0;
			m_time = 0.0f;
			m_startPos = old;
		}
		break;
	}
	if (m_state == 0) {
		End();
		return;
	}
	switch (m_state) {
	case 1:
		UpdateRise(m_time, dt);
		break;
	case 2:
		UpdateFall(m_time, dt);
		break;
	case 3:
		UpdateSink(m_time, dt);
		break;
	case 4:
		UpdateReturn(m_time, dt);
		break;
	}

	float frame = frameTime();

	if (frame > 0.0f)
		m_velocity = (m_pos - old) / frame;
	m_widget->SetRot(math::v4(m_rot.toMatrix(ICON_EFFECT_EPSILON)
					  .extractEulerAnglesYXZ(), 0.0f),
			 0, NULL, ui::Widget::ANIM_ROT, 0, 0.0f, NULL, NULL);

	math::v4 at = m_pos;

	at.set_w(1.0f);
	m_widget->SetPos(at);
}

/* (0x810CF2AA) */
void applauncherIconEffectUpdate(void)
{
	s_launchEffect->Update();
}

/* (0x810CF2BC) */
bool applauncherLaunchEffectIsNotRising(void)
{
	if (s_launchEffect == NULL)
		return true;
	if (s_launchEffect->m_state != 1)
		return true;
	return false;
}

/* (0x810CF2DC) */
ApplauncherPageSway::ApplauncherPageSway()
{
	m_matrix = math::matrix::Identity();
	m_phase = 0.0f;
}

/* (0x810CF332) */
void ApplauncherPageSway::Update(ApplauncherPage *page)
{
	float dt = frameTime();
	float phase = m_phase + dt * 0.5f;
	float amplitude = g_applauncherHome->m_fade[1] * 0.15f;

	while (phase >= 3.1415927f)
		phase = phase - 6.2831855f;
	m_phase = phase;

	float x = amplitude * sce_paf_cosf(phase);
	float y = amplitude * sce_paf_sinf(m_phase);
	math::v4 normal = iconEffectNormalize3(math::v4(x, y, 1.0f, 0.0f));
	math::v4 side = iconEffectNormalize3(iconEffectCross3(normal,
							      math::v4(1.0f, 0.0f, 0.0f, 0.0f)));
	math::v4 front = iconEffectNormalize3(iconEffectCross3(side, normal));

	m_matrix = math::matrix(front, side, normal, m_matrix.w());
}
