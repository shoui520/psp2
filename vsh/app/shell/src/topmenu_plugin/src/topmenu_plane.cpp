/*
 * topmenu_plane.cpp: full-screen input-blocking plane creation helper
 * (0x812844A0-0x812845A7).
 */
#include "topmenu_internal.h"

/* 0x812844A0 */
paf::ui::Widget *topmenuCreatePlane(paf::ui::Widget *parent)
{
	paf::ui::Widget *hostParent = parent;
	if (hostParent == NULL)
		hostParent = g_topWindow->m_scrollView->m_parent;

	paf::ui::Widget *plane = new paf::ui::Widget(hostParent, NULL);
	if (plane != NULL) {
		paf::math::v4 size(1000.0f, 600.0f, 0.0f, 0.0f);
		plane->SetSize(size, NULL, 0x10004, 0, 0.0f, NULL, NULL);
		plane->m_hit_detect_mode = 0;
		plane->m_hit_detect_b_mode = 0;
		ScePafWidget_42C5A4D4(plane, 1, 0);
	}
	return plane;
}
