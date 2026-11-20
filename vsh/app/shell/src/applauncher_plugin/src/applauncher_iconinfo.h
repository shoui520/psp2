/*
 * applauncher_iconinfo.h: what an icon's information (applauncher_iconinfo.cpp) shares with the
 * files after it: the pad listener that opens "about this game" on a key sequence, the item
 * factories of the information's list, and the values they keep. The names are ours
 * (PLACEHOLDER): no record of the 3.6x shell names them.
 */
#ifndef _APPLAUNCHER_ICONINFO_H
#define _APPLAUNCHER_ICONINFO_H

#include <scetypes.h>
#include <plugin.h>
#include <inputdevice/inputlistener.h>
#include <widget/w_list_view.h>
#include <widget/w_list_item.h>
#include <common/shared_ptr.h>
#include "../../livespace_db/livespace_db_internal.h"
#include "../../../../game/game_plugin/game_plugin.h"
#include "../../../../game/game_plugin/game_appinfo.h"

class ApplauncherIcon;

/*
 * PLACEHOLDER: an item of the information's list (720 bytes, constructor 0x810DB96C): a label
 * and its value, set by Set (0x810DB984), the value's own text when the value is empty.
 */
class ApplauncherIconInfoItem : public paf::ui::ListItem {
public:
	ApplauncherIconInfoItem(paf::ui::Widget *parent, paf::ui::ListItem::CreateParam *param);

	void Set(unsigned int label, const paf::wstring &value);

	SceUInt8 m_fields[720 - sizeof(paf::ui::ListItem)];	/* PLACEHOLDER: with its file */
};

/*
 * PLACEHOLDER: a pad listener calling back once its keys are pressed in order (36 bytes,
 * constructor 0x810DB87C, OnUpdate 0x810DB8A8); it calls back only while enabled.
 */
class ApplauncherKeySequenceListener : public paf::inputdevice::InputListener {
public:
	typedef void (*Callback)(void *arg);

	ApplauncherKeySequenceListener(const unsigned int *keys, int keyCount, Callback callback,
				       void *arg);

	virtual void OnUpdate(const paf::inputdevice::Data *data);

	int m_matched;			/* the keys pressed in order so far */
	const unsigned int *m_keys;
	int m_keyCount;
	Callback m_callback;
	void *m_arg;
	bool m_enabled;
};

/*
 * PLACEHOLDER: what the information's list items are made by (40 bytes, constructor
 * 0x810DF654): the icon and its application's database row, and the table of the items it
 * makes. It makes nothing once closed.
 */
class ApplauncherIconInfoFactory : public paf::ui::listview::ItemFactory {
public:
	ApplauncherIconInfoFactory(paf::Plugin *plugin,
				   const paf::common::SharedPtr<ApplauncherIcon> &icon,
				   LsdbAppInfoPtr *appInfo);
	virtual ~ApplauncherIconInfoFactory();

	paf::Plugin *m_plugin;
	paf::common::SharedPtr<ApplauncherIcon> m_icon;
	paf::ui::ListView *m_listView;		/* the list it makes items for */
	bool m_closed;
	LsdbAppInfoPtr *m_appInfo;
	int m_index;				/* PLACEHOLDER: -1 at first */
	int m_itemCount;
	const void *m_items;			/* the items' table */
};

/* PLACEHOLDER: the factory of a power icon's information (44 bytes, constructor 0x810DAEB0) */
class ApplauncherIconInfoPowerFactory : public ApplauncherIconInfoFactory {
public:
	ApplauncherIconInfoPowerFactory(paf::Plugin *plugin, paf::ui::Widget *view,
					const paf::common::SharedPtr<ApplauncherIcon> &icon,
					LsdbAppInfoPtr *appInfo);
	virtual ~ApplauncherIconInfoPowerFactory();

	virtual paf::ui::ListItem *Create(CreateParam &param);

	/* PLACEHOLDER: the one item, the icon's title */
	paf::ui::ListItem *CreateItem(CreateParam &param);

	paf::ui::Widget *m_view;		/* the information's view */
};

/* PLACEHOLDER: the factory of an application's information of _apptype 0 (44 bytes,
   constructor 0x810DFEFC) */
class ApplauncherIconInfoBasicFactory : public ApplauncherIconInfoFactory {
public:
	ApplauncherIconInfoBasicFactory(paf::Plugin *plugin, paf::ui::Widget *view,
					const paf::common::SharedPtr<ApplauncherIcon> &icon,
					LsdbAppInfoPtr *appInfo);

	paf::ui::Widget *m_view;
};

/*
 * PLACEHOLDER: the factory of a game's information (_apptype 1; 360 bytes, constructor
 * 0x810E005C), with the license's status and the free trial's license the job read, whether the
 * application's #appconf has bit 0x400, and whether the free trial was read.
 */
class ApplauncherIconInfoGameFactory : public ApplauncherIconInfoFactory {
public:
	ApplauncherIconInfoGameFactory(paf::Plugin *plugin, paf::ui::Widget *view,
				       const paf::common::SharedPtr<ApplauncherIcon> &icon,
				       LsdbAppInfoPtr *appInfo, GameLicenseStatus *license,
				       GameFttInfo *ftt, bool flag, bool hasFtt);

	paf::ui::Widget *m_view;
	/* PLACEHOLDER: the rest of its fields, written out with its file */
	SceUInt8 m_fields[316];
};

/* PLACEHOLDER: the application's save data, read by the information's list: 4 not read yet
   (0x815443E0) */
extern int g_applauncherIconInfoSaveDataState;

/* PLACEHOLDER: the application's size the list shows, 0 until it is counted (0x81555E90) */
extern SceUInt64 g_applauncherIconInfoAppSize;

#endif /* _APPLAUNCHER_ICONINFO_H */
