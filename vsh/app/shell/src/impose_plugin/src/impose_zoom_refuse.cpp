/*
 * impose_zoom_refuse.cpp: impose_plugin's notice that the zoom is not available (0x8115000C-
 * 0x8115013C). The file's and the names are ours (PLACEHOLDER), after the notice's name
 * "ps_insensitive".
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include "../../livespace_db/lsdb_keys.h"
#include "../../../plugins.h"
#include "impose.h"

using namespace paf;

/* the notice's icon (PLACEHOLDER name) */
#define ZOOM_NOTICE_ICON	0x0E22C099

/* (0x81564E70) */
static ImposeZoomRefuse *s_zoom;

/* (0x8115000C) */
ImposeZoomRefuse::ImposeZoomRefuse(Plugin *plugin) : m_plugin(plugin)
{
	s_zoom = this;
}

/* (0x8115001C) */
ImposeZoomRefuse::~ImposeZoomRefuse()
{
	s_zoom = NULL;
}

/* the interface's zoomRefuse (0x8115002C) */
void ImposeZoomRefuse::ShowRefused()
{
	ImposeNotice notice;

	notice.name = "ps_insensitive";
	notice.priority = -1;
	notice.iconHash = ZOOM_NOTICE_ICON;
	notice.flags = IMPOSE_NOTICE_FLAG_TIMEOUT;
	imposeNotificationShowNotice(&notice);
}
