/*
 * The dialog's trophy work (Sony's file, named in the 0.99x builds' messages): the session, the
 * application's title for the caller's context, and an installed title the user may remove.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
extern "C" {
#include <regmgr/regmgr.h>
}
#include <np/np_trophy.h>
#include <np/np_error.h>
#include <plugin.h>
#include <common/char_encoding.h>
#include <graphics/surface/surface.h>
#include "../../../app/common/src/np/np_trophy/np_trophy.h"
#include "np_trophy_setup_dialog_plugin.h"

using namespace paf;

/* the system parameter that fails the setup for debugging (name ours) */
#define TROPHY_SETUP_SYSTEM_PARAM_DEBUG_FAILURE	0x003317A1
/* SceNpTrophy_D70DAFA7's arguments (meaning not known) */
#define TROPHY_SETUP_OPEN_ARG1			0x01230001
#define TROPHY_SETUP_OPEN_ARG2			16
/* the icon's buffer, and the size it is shown at */
#define TROPHY_SETUP_ICON_BUFFER_SIZE		204800
#define TROPHY_SETUP_ICON_WIDTH			128
#define TROPHY_SETUP_ICON_HEIGHT		70

/* the title states */
enum {
	TROPHY_SETUP_STATUS_1 = 1,
	/* installed: it may be removed */
	TROPHY_SETUP_STATUS_INSTALLED = 2,
	TROPHY_SETUP_STATUS_3 = 3,
	TROPHY_SETUP_STATUS_4 = 4,
	TROPHY_SETUP_STATUS_5 = 5
};

SetupController::SetupController(SceNpTrophyContext context)
	: m_session(-1), m_context(context), m_title(0), m_status(0), m_titleInfo(NULL)
{
}

SetupController::~SetupController()
{
	if (m_titleInfo != NULL)
		delete m_titleInfo;
	if (m_title != 0)
		SceNpTrophy_97D19AB6(m_title);
	if (m_session != 0)
		SceNpTrophy_44E0F73E(m_session);
}

int SetupController::Init()
{
	int ret;

	ret = SceNpTrophy_17FB1D9E(&m_session);
	if (ret >= 0)
		ret = 0;
	return ret;
}

void SetupController::Abort()
{
	SceNpTrophy_FD34F7CF(m_session);
}

int SetupController::Open()
{
	SceUInt32 title = 0;
	int value = 0;
	int ret;

	if (sceRegMgrSystemParamGetInt(TROPHY_SETUP_SYSTEM_PARAM_DEBUG_FAILURE, &value) == 0 &&
	    value == 1) {
		sceKernelDelayThread(500000);
		ret = SCE_NP_TROPHY_ERROR_DEBUG_FAILURE;
	} else {
		ret = SceNpTrophy_D70DAFA7(m_session, TROPHY_SETUP_OPEN_ARG1, TROPHY_SETUP_OPEN_ARG2,
					   0, 0, 0, &title);
		if (ret >= 0) {
			ret = SceNpTrophy_62E37599(m_session, title, m_context, &m_status);
			if (ret >= 0) {
				m_title = title;
				return 0;
			}
		}
	}
	if (title != 0)
		SceNpTrophy_97D19AB6(title);
	return ret;
}

int SetupController::LoadIcon(const TrophySetupTitleInfo *info)
{
	SceInt32 handle = -1;
	SceSize size;
	graph::Surface::LoadOption option;
	SceUInt8 *buffer;
	Plugin *plugin;
	int ret;

	ret = SceNpTrophy_C80794CF(&handle, info, 0);
	if (ret >= 0) {
		buffer = new SceUInt8[TROPHY_SETUP_ICON_BUFFER_SIZE];
		if (buffer == NULL) {
			ret = TROPHY_SETUP_PLUGIN_ERROR_NO_MEMORY;
		} else {
			ret = SceNpTrophy_2EE76D88(m_session, m_title, handle, buffer,
						   TROPHY_SETUP_ICON_BUFFER_SIZE, &size);
			if (ret >= 0) {
				plugin = Plugin::Find(TROPHY_SETUP_PLUGIN_NAME);
				if (plugin == NULL) {
					ret = TROPHY_SETUP_PLUGIN_ERROR_INTERNAL;
				} else {
					option.scale.w = TROPHY_SETUP_ICON_WIDTH;
					option.scale.h = TROPHY_SETUP_ICON_HEIGHT;
					m_icon = graph::Surface::Load(plugin->m_surface_pool, buffer, size, &option);
					ret = option.error;
					if (ret >= 0)
						ret = 0;
					else
						m_icon = NULL;
				}
			}
		}
		if (buffer != NULL)
			delete[] buffer;
	}
	if (handle != -1)
		SceNpTrophy_10371F25(handle);
	return ret;
}

/* an installed title the user has completed is shown with its icon, for the dialog to ask
   before it is removed */
int SetupController::CheckTitleForRemoval()
{
	TrophySetupTitleInfo *info = NULL;
	TrophySetupTitleState state;
	int ret;

	if (m_titleInfo != NULL) {
		delete m_titleInfo;
		m_titleInfo = NULL;
	}
	switch (m_status) {
	case TROPHY_SETUP_STATUS_1:
	case TROPHY_SETUP_STATUS_3:
	case TROPHY_SETUP_STATUS_4:
	case TROPHY_SETUP_STATUS_5:
		break;
	case TROPHY_SETUP_STATUS_INSTALLED:
		ret = SceNpTrophy_0AC3CEA8(m_session, m_title, &state);
		if (ret < 0)
			return ret;
		if (state.percentage < 100)
			break;
		info = new TrophySetupTitleInfo;
		if (info == NULL) {
			ret = TROPHY_SETUP_PLUGIN_ERROR_NO_MEMORY;
			goto error;
		}
		sceClibMemset(info->info, 0, offsetof(TrophySetupTitleInfo, extra));
		sceClibMemset(info->extra, 0, sizeof(info->extra));
		sceClibMemset(&info->details, 0, sizeof(info->details));
		info->details.size = sizeof(info->details);
		ret = SceNpTrophy_061DE3F5(m_session, m_title, info, info->extra, &info->details);
		if (ret < 0)
			goto error;
		if (LoadIcon(info) < 0)
			m_icon = NULL;
		break;
	default:
		return TROPHY_SETUP_PLUGIN_ERROR_INTERNAL;
	}
	m_titleInfo = info;
	return 0;
error:
	if (info != NULL)
		delete info;
	return ret;
}

/* the title's name and icon, when the dialog is to ask before removing it */
bool SetupController::GetRemovalInfo(wstring *title, intrusive_ptr<graph::Surface> *icon)
{
	TrophySetupTitleInfo *info = m_titleInfo;

	if (info == NULL)
		return false;
	if (info->locked != 0)
		return false;
	if (info->extra[0] == 0 && info->extra[1] == 0)
		return false;
	common::Utf8ToUtf16((const char *)info->details.title, title);
	*icon = m_icon;
	return true;
}

int SetupController::RemoveTitle()
{
	SceInt32 handle;
	int ret;

	if (m_titleInfo == NULL)
		return 0;
	handle = -1;
	ret = SceNpTrophy_C80794CF(&handle, m_titleInfo, 0);
	if (ret >= 0) {
		ret = SceNpTrophy_C338E582(m_session, m_title, handle);
		if (ret >= 0) {
			delete m_titleInfo;
			ret = 0;
			m_titleInfo = NULL;
		}
	}
	if (handle != -1)
		SceNpTrophy_10371F25(handle);
	return ret;
}

int SetupController::Setup()
{
	SceInt32 result = -1;
	int ret;

	switch (m_status) {
	case TROPHY_SETUP_STATUS_1:
	case TROPHY_SETUP_STATUS_INSTALLED:
		ret = SceNpTrophy_2B0CC831(m_session, m_title, m_context, &result);
		break;
	case TROPHY_SETUP_STATUS_3:
		ret = SceNpTrophy_3AC04020(m_session, m_title, m_context, &result);
		break;
	case TROPHY_SETUP_STATUS_4:
		ret = SceNpTrophy_10182F1B(m_session, m_title, m_context, &result);
		break;
	case TROPHY_SETUP_STATUS_5:
		ret = SceNpTrophy_2B0CC831(m_session, m_title, m_context, &result);
		break;
	default:
		return TROPHY_SETUP_PLUGIN_ERROR_INTERNAL;
	}
	if (ret >= 0)
		ret = 0;
	return ret;
}

void SetupController::Close()
{
	m_status = 0;
	if (m_titleInfo != NULL) {
		delete m_titleInfo;
		m_titleInfo = NULL;
	}
	if (m_title != 0) {
		SceNpTrophy_97D19AB6(m_title);
		m_title = 0;
	}
}
