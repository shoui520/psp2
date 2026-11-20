/*
 * applauncher_iconinfo_power.cpp: the list of a power icon's information, one item with the
 * icon's title (0x810DAEB0-0x810DB040). The file's name is ours (PLACEHOLDER): no record of
 * the 3.6x shell names it.
 */

#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <widget/w_list_view.h>
#include <widget/w_list_item.h>
#include <common/shared_ptr.h>
#include <common/char_encoding.h>
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher_home.h"
#include "applauncher_iconinfo.h"

using namespace paf;

/* PLACEHOLDER: the label of the power icon's item */
#define APPLAUNCHER_INFO_POWER_LABEL	0x0BB94ACA

/* (0x810DAEB0) */
ApplauncherIconInfoPowerFactory::ApplauncherIconInfoPowerFactory(Plugin *plugin,
	ui::Widget *view, const common::SharedPtr<ApplauncherIcon> &icon, LsdbAppInfoPtr *appInfo)
	: ApplauncherIconInfoFactory(plugin, icon, appInfo)
{
	m_view = view;
	m_itemCount = 1;
}

/* (0x810DAED4, 0x810DAEF0) */
ApplauncherIconInfoPowerFactory::~ApplauncherIconInfoPowerFactory()
{
}

/* (0x810DAF10) */
ui::ListItem *ApplauncherIconInfoPowerFactory::CreateItem(CreateParam &param)
{
	if (param.m_cell != 0)
		return NULL;

	ApplauncherIconInfoItem *item = NULL;
	wstring title;

	item = new ApplauncherIconInfoItem(param.m_parent, NULL);
	if (item != NULL) {
		title = applauncherIconInfoUtf16(m_icon->m_data->title);
		item->Set(APPLAUNCHER_INFO_POWER_LABEL, title);
	}
	return item;
}

/* (0x810DAFC4) */
ui::ListItem *ApplauncherIconInfoPowerFactory::Create(CreateParam &param)
{
	m_listView = param.m_list_view;
	return CreateItem(param);
}
