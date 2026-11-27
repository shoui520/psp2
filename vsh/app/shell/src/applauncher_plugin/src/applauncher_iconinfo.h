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
#include <std/string>
#include <job/job.h>
#include <common/char_encoding.h>
#include <rtc/rtc.h>
#include "../../livespace_db/livespace_db_internal.h"
#include "../../../../game/game_plugin/game_plugin.h"
#include "../../../../game/game_plugin/game_appinfo.h"

class ApplauncherIcon;

/*
 * PLACEHOLDER: an item of the information's list (720 bytes, constructor 0x810DB96C): a label
 * and its value, the value's own text when the value is empty (Set 0x810DB984, 0x810DBEBA).
 */
class ApplauncherIconInfoItem : public paf::ui::ListItem {
public:
	ApplauncherIconInfoItem(paf::ui::Widget *parent, paf::ui::ListItem::CreateParam *param);
	virtual ~ApplauncherIconInfoItem();

	void Set(unsigned int label, const paf::wstring &value);
	void Set(const paf::wstring &label, const paf::wstring &value);
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
 * PLACEHOLDER: the item of the application's size (768 bytes, constructor 0x810DC38C): dots
 * until the job counting it is done, a game's size read from its #appsize where it has one.
 */
class ApplauncherIconInfoSizeItem : public paf::ui::ListItem {
public:
	ApplauncherIconInfoSizeItem(const paf::string &titleId, paf::ui::Widget *parent, int storage,
				    int appType, bool hasAppSize, SceUInt64 appSize,
				    paf::ui::ListItem::CreateParam *param);
	virtual ~ApplauncherIconInfoSizeItem();

	/* the template opened (0x810DCBEC), and its label and value set (0x810DC434) */
	void Setup(unsigned int label);
	void Update(unsigned int label);

	paf::string m_titleId;
	paf::common::SharedPtr<paf::job::JobItem> m_job;	/* the job counting the size */
	int m_dots;				/* the dots the value shows */
	int m_reserved;				/* PLACEHOLDER: -1 from the constructor, not read */
	unsigned int m_label;
	int m_storage;				/* 1: game card storage, shows size unavailable */
	int m_appType;				/* the application's _apptype */
	bool m_hasAppSize;			/* the application has a #appsize */
	SceUInt64 m_appSize;			/* its #appsize */
};

/*
 * PLACEHOLDER: the item of the application's save data (constructor 0x810DD8BC): its state read
 * by a job from AppMgr once, kept for the next list.
 */
class ApplauncherIconInfoSaveDataItem : public paf::ui::ListItem {
public:
	ApplauncherIconInfoSaveDataItem(const paf::string &path, const paf::string &dir, int attr,
					paf::ui::Widget *parent, paf::ui::ListItem::CreateParam *param);
	virtual ~ApplauncherIconInfoSaveDataItem();

	/* the value's text for the state (0x810DD928) */
	void SetValue(paf::ui::Widget *valueText);
	/* the template opened, its label and its value, the state read where it is not yet
	   (0x810DDB9C) */
	void Setup(unsigned int label);

	paf::string m_path;			/* the application's folder */
	paf::string m_dir;			/* its save data's */
	int m_attr;			/* the application's ATTRIBUTE */
	paf::common::SharedPtr<paf::job::JobItem> m_job;
	int m_reserved;			/* PLACEHOLDER: -1 from the constructor, not read */
};

/*
 * PLACEHOLDER: the item of a check box, bit 0x800 of the application's #appconf cleared while it
 * is checked (constructor 0x810DE684): read by one job, written by another as it changes.
 */
class ApplauncherIconInfoCheckItem : public paf::ui::ListItem {
public:
	ApplauncherIconInfoCheckItem(const paf::string &titleId, paf::ui::Widget *parent,
				     paf::ui::ListItem::CreateParam *param);
	virtual ~ApplauncherIconInfoCheckItem();

	/* the template opened, and the check read (0x810DE6D8) */
	void Setup();

	paf::common::SharedPtr<paf::job::JobItem> m_setJob;	/* the job writing the check */
	paf::common::SharedPtr<paf::job::JobItem> m_job;		/* the job reading it */
	paf::string m_titleId;
	bool m_checked;
	paf::ui::Widget *m_box;
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

	/* PLACEHOLDER: the application's title (0x810DF726) */
	void GetTitle(paf::wstring *title);

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
	virtual ~ApplauncherIconInfoBasicFactory();

	virtual paf::ui::ListItem *Create(CreateParam &param);

	/* PLACEHOLDER: the one item, the application's title */
	paf::ui::ListItem *CreateItem(CreateParam &param);

	paf::ui::Widget *m_view;
};

/*
 * PLACEHOLDER: the factory of a game's information (_apptype 1; 360 bytes, constructor
 * 0x810E005C): its items' table chosen by the application's type, whether it has a license with
 * a period or a free trial, and what may be upgraded; the license's period it shows.
 */
class ApplauncherIconInfoGameFactory : public ApplauncherIconInfoFactory {
public:
	ApplauncherIconInfoGameFactory(paf::Plugin *plugin, paf::ui::Widget *view,
				       const paf::common::SharedPtr<ApplauncherIcon> &icon,
				       LsdbAppInfoPtr *appInfo, GameLicenseStatus *license,
				       GameFttInfo *ftt, bool hasCheck, bool hasFtt);
	virtual ~ApplauncherIconInfoGameFactory();

	virtual paf::ui::ListItem *Create(CreateParam &param);

	/* PLACEHOLDER: the item of the table's kind at the cell, for the application's type */
	paf::ui::ListItem *CreateItem(int type, CreateParam &param);

	paf::ui::Widget *m_view;
	paf::common::SharedPtr<paf::job::JobItem> m_job;	/* cancelled when it is destroyed */
	SceUInt64 m_unusedAppSize;	/* 0 from the constructor, never read */
	int m_unusedStorage;		/* 0 from the constructor, never read */
	int reserved;			/* nothing in the shell reads or writes it */
	GameLicenseStatus *m_license;
	GameFttInfo *m_ftt;
	SceUInt64 m_startTime;		/* the license's period, 0 and INT64_MAX without one */
	SceUInt64 m_expireTime;
	SceUInt8 m_reserved[264];	/* nothing in the shell reads or writes them */
};

/* the text in UTF-16 */
static inline paf::wstring applauncherIconInfoUtf16(const paf::string &text)
{
	paf::wstring utf16;

	paf::common::Utf8ToUtf16(text, &utf16, 0);
	return utf16;
}

/* PLACEHOLDER names: a time in UTC (0x810DF792) and an SQLite time in UTC (0x810DF844) as the
   local time's text, and the application's APP_VER as it is shown (0x810DF90E) */
int applauncherIconInfoFormatTime(const paf::rtc::Tick *utc, paf::wstring *text);
int applauncherIconInfoFormatSQLiteTime(const paf::string *sqlTime, paf::wstring *text);
void applauncherIconInfoGetVersion(LsdbAppInfoPtr *appInfo, paf::wstring *version);

/* PLACEHOLDER: the application's save data, read by the information's list: 4 not read yet
   (0x815443E0) */
extern int g_applauncherIconInfoSaveDataState;

/* PLACEHOLDER: the application's size the list shows, 0 until it is counted (0x81555E90) */
extern SceUInt64 g_applauncherIconInfoAppSize;

#endif /* _APPLAUNCHER_ICONINFO_H */
