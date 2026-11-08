/*
 * paf::ui::Layouter and Animator (TOOL 0x81145fd8..0x811460e8): what lays a widget out and what
 * animates it each update, and whether such a pass is under way. No record or string names the
 * file.
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/drawobj/draw_obj.h>
#include <widget/core/layouter.h>

namespace paf {
namespace ui {

int32_t Layouter::s_enter_layout;
int32_t Animator::s_enter_animator;

int32_t *Layouter::Finish()
{
	s_enter_layout = 0;
	return &s_enter_layout;
}

int32_t *Layouter::Enter()
{
	s_enter_layout = 1;
	return &s_enter_layout;
}

int32_t Layouter::OnUpdate()
{
	s_enter_layout = 1;
	OnUpdate_core();
	s_enter_layout = 0;
	return 0;
}

int32_t Layouter::OnUpdate_core()
{
	if (m_widget == NULL)
		return -1;
	if (m_cb_layout != NULL)
		m_cb_layout(this, m_widget, NULL, NULL, NULL);
	m_dirty = 0;
	m_first = 0;
	m_reso_dirty_begin = false;
	m_reso_dirty_end = false;
	return 0;
}

bool Layouter::IsEnter()
{
	return s_enter_layout != 0;
}

void Animator::Finish()
{
	graph::DrawObj::FinishSafety();
	s_enter_animator = 0;
}

void Animator::Enter()
{
	s_enter_animator = 1;
	graph::DrawObj::EnterSafety();
}

int32_t Animator::OnUpdate()
{
	s_enter_animator = 1;
	graph::DrawObj::EnterSafety();
	OnUpdate_core();
	graph::DrawObj::FinishSafety();
	s_enter_animator = 0;
	return 0;
}

int32_t Animator::OnUpdate_core()
{
	if (m_widget == NULL)
		return -1;
	if (m_cb_anim != NULL)
		m_cb_anim(this, m_widget, NULL, NULL, NULL);
	m_first = 0;
	m_dirty = 0;
	return 0;
}

bool Animator::IsEnter()
{
	return s_enter_animator != 0;
}

}
}
