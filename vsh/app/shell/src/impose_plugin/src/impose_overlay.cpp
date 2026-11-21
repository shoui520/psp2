/*
 * impose_overlay.cpp: impose_plugin's overlay pages (0x811516E4-0x81151B08): one of three
 * pages opened over everything at a time, by the interface's open and close. The file's, the
 * class's and the page names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include "../../livespace_db/lsdb_keys.h"
#include "impose.h"

using namespace paf;

/* the pages (PLACEHOLDER names) */
#define OVERLAY_PAGE_0		0xF7D65BF9
#define OVERLAY_PAGE_1		0xBAF32F54
#define OVERLAY_PAGE_2		0x4FCB5566

/* the fade in and out, in milliseconds */
#define OVERLAY_FADE_TIME	100.0f
/* the pages' draw priority */
#define OVERLAY_PRIORITY	40

/* (0x815448F0) */
static ImposeOverlay *s_overlay;

/* (0x811516E4) */
ImposeOverlay::ImposeOverlay() : m_page(NULL), m_kind(0)
{
	s_overlay = this;
}

/* (0x811516F8) */
ImposeOverlay::~ImposeOverlay()
{
	if (m_page != NULL) {
		m_page->DestroyWidget();
		m_page = NULL;
	}
	s_overlay = NULL;
}

/* the open page closed (0x8115171C) */
__attribute__((noinline)) void ImposeOverlay::Close()
{
	if (m_page != NULL) {
		Plugin::PageCloseParam param;

		param.fade = true;
		param.fade_time_ms = OVERLAY_FADE_TIME;
		if (m_kind == 0)
			g_imposePlugin->PageClose(IDParam(OVERLAY_PAGE_0), param);
		else if (m_kind == 1)
			g_imposePlugin->PageClose(IDParam(OVERLAY_PAGE_1), param);
		else if (m_kind == 2)
			g_imposePlugin->PageClose(IDParam(OVERLAY_PAGE_2), param);
		m_page = NULL;
	}
}

/* the page of the kind opened, the one open before closed (0x811518CA) */
void ImposeOverlay::Open(int kind)
{
	if (m_page == NULL || kind != m_kind) {
		Close();
		m_kind = kind;

		Plugin::PageOpenParam param;

		param.fade = true;
		param.fade_time_ms = OVERLAY_FADE_TIME;
		param.overwrite_draw_priority = OVERLAY_PRIORITY;
		if (m_kind == 0)
			m_page = g_imposePlugin->PageOpen(IDParam(OVERLAY_PAGE_0), param);
		else if (m_kind == 1)
			m_page = g_imposePlugin->PageOpen(IDParam(OVERLAY_PAGE_1), param);
		else if (m_kind == 2)
			m_page = g_imposePlugin->PageOpen(IDParam(OVERLAY_PAGE_2), param);
		if (m_page != NULL)
			ScePafWidget_66847A11(m_page, true);
	}
}
