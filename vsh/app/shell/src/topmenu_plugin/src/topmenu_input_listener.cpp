/*
 * topmenu_input_listener.cpp: pad InputListener handling IDU kiosk exit combo
 * and /CONFIG/SHELL test_crash debug key combos (0x81299AC0-0x81299C4F).
 */
#include "topmenu_internal.h"
#include <inputdevice/inputlistener.h>
#include <inputdevice/pad/pad.h>
#include <inputdevice/inputdevice.h>

namespace {

static const SceUInt32 TOPMENU_PAD_IDU_EXIT_COMBO = 0x0E01u;
static const SceUInt32 TOPMENU_PAD_TEST_CRASH_COMBO = 0x0C41u;
static const SceUInt32 TOPMENU_PAD_TEST_HANG_COMBO = 0x0C82u;
static const float TOPMENU_IDU_EXIT_HOLD_SECONDS = 5.0f;

class TopmenuInputListener : public paf::inputdevice::InputListener {
public:
	/* 0x81299AC0 */
	TopmenuInputListener()
		: paf::inputdevice::InputListener(paf::inputdevice::DEVICE_TYPE_PAD),
		  m_iduExitHoldSec(0.0f),
		  m_unusedReserved(0)
	{
	}

	virtual ~TopmenuInputListener() {}

	/* 0x81299ADC */
	virtual void OnUpdate(const paf::inputdevice::Data *data)
	{
		if (g_topWindow == NULL)
			return;

		const paf::inputdevice::pad::Data *padData = data->m_pad_data;

		if (shellStartupGetIduMode() == 1) {
			if (g_topWindow->m_currentTab != NULL &&
			    g_topWindow->m_currentTab->m_index == 0 &&
			    padData->buttons == TOPMENU_PAD_IDU_EXIT_COMBO) {
				m_iduExitHoldSec +=
					paf::Timer::s_tick_unit * paf::Timer::s_tick_interval;
				if (m_iduExitHoldSec >= TOPMENU_IDU_EXIT_HOLD_SECONDS) {
					if (shellStartupLeaveIduMode())
						topmenuDismissIduOverlay(g_topWindow);
				}
			} else {
				m_iduExitHoldSec = 0.0f;
			}

			if (g_topWindow->m_foregroundApp != NULL) {
				const char *fgTitleId = g_topWindow->m_foregroundApp->m_titleId.c_str();
				if (sce_paf_strncmp(fgTitleId, "NPXS10010",
						    sce_paf_strlen("NPXS10010")) == 0) {
					if ((padData->buttons & 0xFFu) != 0)
						topmenuReturnFromIduDemoApp(g_topWindow);
				}
			}
		}

		if (g_topmenuTestCrashEnabled) {
			if (padData->buttons == TOPMENU_PAD_TEST_CRASH_COMBO) {
				*(volatile int *)0 = 5;
				return;
			}
			if (padData->buttons == TOPMENU_PAD_TEST_HANG_COMBO)
				paf::thread::Sleep(0x7FFFFFFF);
		}
	}

private:
	float m_iduExitHoldSec;
	SceUInt32 m_unusedReserved;
};

} /* namespace */

paf::inputdevice::InputListener *topmenuCreateInputListener(void)
{
	return new TopmenuInputListener();
}
