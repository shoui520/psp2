/*
 * applauncher_iconinfo_basic.cpp: the list of the information of an application of _apptype 0,
 * one item with its title (0x810DFEFC-0x810E005C). The file's name is ours (PLACEHOLDER): no
 * record of the 3.6x shell names it.
 */

#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <widget/w_list_view.h>
#include <widget/w_list_item.h>
#include <common/shared_ptr.h>
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher_home.h"
#include "applauncher_iconinfo.h"

using namespace paf;

/* PLACEHOLDER: the label of the title's item */
#define APPLAUNCHER_INFO_BASIC_LABEL	0x0BB94ACA

/* (0x810DFEFC) */
ApplauncherIconInfoBasicFactory::ApplauncherIconInfoBasicFactory(Plugin *plugin,
	ui::Widget *view, const common::SharedPtr<ApplauncherIcon> &icon, LsdbAppInfoPtr *appInfo)
	: ApplauncherIconInfoFactory(plugin, icon, appInfo)
{
	m_view = view;
	m_itemCount = 1;
}

/* (0x810DFF20, 0x810DFF3C) */
ApplauncherIconInfoBasicFactory::~ApplauncherIconInfoBasicFactory()
{
}

/* (0x810DFF5C) */
__attribute__((noinline)) ui::ListItem *
ApplauncherIconInfoBasicFactory::CreateItem(CreateParam &param)
{
	if (param.m_cell != 0)
		return NULL;

	ApplauncherIconInfoItem *item = NULL;
	wstring title;

	item = new ApplauncherIconInfoItem(param.m_parent, NULL);
	if (item != NULL) {
		GetTitle(&title);
		item->Set(APPLAUNCHER_INFO_BASIC_LABEL, title);
	}
	return item;
}

/* (0x810DFFE0) */
ui::ListItem *ApplauncherIconInfoBasicFactory::Create(CreateParam &param)
{
	m_listView = param.m_list_view;
	return CreateItem(param);
}
