/*
 * applauncher_iconinfo_list.cpp: the items of an icon's information list and the factories making
 * them: a label and its value, the application's size counted by a job, its save data's state,
 * and the lists of a game and of another application (0x810DB96C-0x810DFEFC). The file's name is
 * ours (PLACEHOLDER): no record of the 3.6x shell names it.
 */

#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <appmgr.h>
#if !defined(FW_TYPE_CEX)
#include <kernel/iofilemgr.h>
#include <libkernel/libkernel.h>
extern "C" int sceAppMgrUmount(const char *mountPoint);
#endif
#include <plugin.h>
#include <std/string>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <widget/w_check_box.h>
#include <file/localfile.h>
#include <rtc/rtc.h>
#include <datetime/datetime.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include <common/char_encoding.h>
#include "../../livespace_db/lsdb_keys.h"
#include "../../../shell.h"
#include "../../../plugins.h"
#include "applauncher.h"
#include "applauncher_home.h"
#include "applauncher_iconinfo.h"
#include "../../../../game/game_plugin/game_appsize.h"

using namespace paf;

/* an item's template, and its widgets: the box holding the label and the value */
#define APPLAUNCHER_INFO_ITEM_TEMPLATE		0x0C3F5EBD
#define APPLAUNCHER_INFO_ITEM_BOX		0x63678FEE
#define APPLAUNCHER_INFO_ITEM_LABEL		0x6254DF7B
#define APPLAUNCHER_INFO_ITEM_VALUE		0xFF7C6472

/* PLACEHOLDER: the text of a value not known */
#define APPLAUNCHER_INFO_MSG_NONE		0xAF471967

/* PLACEHOLDER: the size item's timer, adding a dot to its value until the size is counted */
#define APPLAUNCHER_INFO_SIZE_TIMER		(ui::Handler::CB_INTERVAL | 0x303A)
#define APPLAUNCHER_INFO_SIZE_TIMER_INTERVAL	50.0f
#define APPLAUNCHER_INFO_SIZE_DOTS		10

/* PLACEHOLDER: the shell events the size item listens to: its count stopped, and counted again */
#define APPLAUNCHER_INFO_EVENT_STOP		2
#define APPLAUNCHER_INFO_EVENT_RESTART		3

/* PLACEHOLDER: the sizes gamePluginTitleGetSizes is asked for */
#define APPLAUNCHER_INFO_SIZES_APP		1
#define APPLAUNCHER_INFO_SIZES_PATCH		2
#define APPLAUNCHER_INFO_SIZES_ADDCONT		4
#define APPLAUNCHER_INFO_SIZES_DATA		8
#define APPLAUNCHER_INFO_SIZES_ALL		15

/* PLACEHOLDER: an application's _apptype of a game */
#define APPLAUNCHER_INFO_APPTYPE_GAME		1

/* a field of an #appsize set to units (game_appsize.h), its top bits cleared */
static inline void applauncherAppSizeSetField(SceUInt64 *appSize, unsigned int units,
					      unsigned int shift, SceUInt64 mask)
{
	*appSize = ((*appSize & ~(mask << shift)) | ((SceUInt64)(units & mask) << shift)) &
		   GAME_APPSIZE_MAX;
}


/* hides a widget over the timer's time (inline in paf's callers) */
static inline void HideWidget(ui::Widget *widget, Timer *timer)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(timer != NULL ? timer->GetTotalTime() : 0.0f, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

#define APPLAUNCHER_INFO_FIND(parent, type, id)						\
	((parent)->FindChild(IDParam(s_empty_name, (id)), 0) != NULL &&			\
	 (parent)->FindChild(IDParam(s_empty_name, (id)), 0)->IsInherit(type::TypeName()) ?	\
	 (type *)(parent)->FindChild(IDParam(s_empty_name, (id)), 0) : NULL)

/* an item's label or value laid out: the item sized to it (0x810DFA5A) */
static void applauncherIconInfoItemOnReady(int32_t type, ui::Handler *self, ui::Event *e,
					   void *data);

/* (0x810DB96C) */
ApplauncherIconInfoItem::ApplauncherIconInfoItem(ui::Widget *parent,
						 ui::ListItem::CreateParam *param)
	: ui::ListItem(parent, param)
{
}

/* the item's template opened, its label the plugin's string of label (0x810DB984) */
void ApplauncherIconInfoItem::Set(unsigned int label, const wstring &value)
{
	Plugin *plugin = Plugin::Find("applauncher_plugin");

	if (plugin == NULL)
		return;

	plugin->TemplateOpen(this, IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_TEMPLATE),
			     Plugin::TemplateOpenParam());

	ui::Widget *box = APPLAUNCHER_INFO_FIND(this, ui::ListItem, APPLAUNCHER_INFO_ITEM_BOX);

	if (box == NULL)
		return;

	ui::Widget *labelText = box->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_LABEL), 0);

	if (labelText != NULL) {
		HideWidget(labelText, NULL);
		labelText->SetString(wstring(applauncherGetString(IDParam(s_empty_name, label))));
		labelText->AddEventListener(ui::Handler::CB_STATE_READY,
			new ui::EventCBListener(applauncherIconInfoItemOnReady, this));
	}

	ui::Widget *valueText = box->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_VALUE), 0);

	if (valueText != NULL) {
		HideWidget(valueText, NULL);

		wstring text = value;

		if (text.length() == 0)
			text = applauncherGetString(IDParam(s_empty_name, APPLAUNCHER_INFO_MSG_NONE));
		valueText->SetString(text);
		valueText->AddEventListener(ui::Handler::CB_STATE_READY,
			new ui::EventCBListener(applauncherIconInfoItemOnReady, this));
	}
}

/* the item's template opened, with its label's text (0x810DBEBA) */
void ApplauncherIconInfoItem::Set(const wstring &label, const wstring &value)
{
	Plugin *plugin = Plugin::Find("applauncher_plugin");

	if (plugin == NULL)
		return;

	plugin->TemplateOpen(this, IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_TEMPLATE),
			     Plugin::TemplateOpenParam());

	ui::Widget *box = APPLAUNCHER_INFO_FIND(this, ui::ListItem, APPLAUNCHER_INFO_ITEM_BOX);

	if (box == NULL)
		return;

	ui::Widget *labelText = box->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_LABEL), 0);

	if (labelText != NULL) {
		HideWidget(labelText, NULL);
		labelText->SetString(label);
		labelText->AddEventListener(ui::Handler::CB_STATE_READY,
			new ui::EventCBListener(applauncherIconInfoItemOnReady, this));
	}

	ui::Widget *valueText = box->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_VALUE), 0);

	if (valueText != NULL) {
		HideWidget(valueText, NULL);

		wstring text = value;

		if (text.length() == 0)
			text = applauncherGetString(IDParam(s_empty_name, APPLAUNCHER_INFO_MSG_NONE));
		valueText->SetString(text);
		valueText->AddEventListener(ui::Handler::CB_STATE_READY,
			new ui::EventCBListener(applauncherIconInfoItemOnReady, this));
	}
}

/* (0x810DC350, 0x810DC36C) */
ApplauncherIconInfoItem::~ApplauncherIconInfoItem()
{
}

/*
 * PLACEHOLDER: the job counting an application's size (72 bytes, vtable 0x8152FB18): a game's
 * from the fields of its #appsize not counted yet, which it then writes, another application's
 * whole.
 */
class ApplauncherIconInfoSizeJob : public job::JobItem {
public:
	ApplauncherIconInfoSizeJob(ApplauncherIconInfoSizeItem *item)
		: job::JobItem("GameAppGetSizeJob", NULL), m_titleId(item->m_titleId),
		  m_appType(item->m_appType), m_hasAppSize(item->m_hasAppSize),
		  m_appSize(item->m_appSize), m_item(item), m_size(0)
	{
	}
	virtual ~ApplauncherIconInfoSizeJob()
	{
	}

	virtual int32_t Run();
	virtual void Finish(int32_t result);

	/* the #appsize's fields, and which are not counted (0x810DD01E) */
	int ReadAppSize(bool *noApp, bool *noPatch, bool *noAddcont, SceUInt64 *appSize,
			SceUInt64 *app, SceUInt64 *patch, SceUInt64 *addcont);
	/* a game's size, its #appsize's missing fields counted and written (0x810DD1C4) */
	int CountGameSize(GamePluginInterface *game);

	string m_titleId;
	int m_appType;
	bool m_hasAppSize;
	SceUInt64 m_appSize;
	ApplauncherIconInfoSizeItem *m_item;
	SceUInt64 m_size;		/* the size counted */
};

/* the dots' timer: one dot more, fading in, up to ten (0x810DCC9A) */
static void applauncherIconInfoSizeItemOnTimer(int32_t type, ui::Handler *self, ui::Event *e,
					       void *data);
/* the shell's events: the count stopped (0x810DCE2A), and counted again (0x810DCEA4) */
static void applauncherIconInfoSizeItemOnStop(int code, const void *data, SceSize size,
					      void *arg);
static void applauncherIconInfoSizeItemOnRestart(int code, const void *data, SceSize size,
						 void *arg);

/* (0x810DC38C) */
ApplauncherIconInfoSizeItem::ApplauncherIconInfoSizeItem(const string &titleId,
	ui::Widget *parent, int storage, int appType, bool hasAppSize, SceUInt64 appSize,
	ui::ListItem::CreateParam *param)
	: ui::ListItem(parent, param), m_titleId(titleId), m_dots(0), m_reserved(-1), m_label(0),
	  m_storage(storage), m_appType(appType), m_hasAppSize(hasAppSize), m_appSize(appSize)
{
	SceShell_7FE4E732(APPLAUNCHER_INFO_EVENT_STOP, applauncherIconInfoSizeItemOnStop, this);
	SceShell_7FE4E732(APPLAUNCHER_INFO_EVENT_RESTART, applauncherIconInfoSizeItemOnRestart,
			  this);
}

/*
 * The label set, and the value: the size where it is known, else dots until the job counting it
 * is done, the size not known where the job could not be started (0x810DC434).
 */
void ApplauncherIconInfoSizeItem::Update(unsigned int label)
{
	ui::Widget *box = APPLAUNCHER_INFO_FIND(this, ui::ListItem, APPLAUNCHER_INFO_ITEM_BOX);

	if (box == NULL)
		return;

	ui::Widget *labelText = box->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_LABEL), 0);

	if (labelText != NULL) {
		HideWidget(labelText, NULL);
		labelText->SetString(wstring(applauncherGetString(IDParam(s_empty_name, label))));
		labelText->AddEventListener(ui::Handler::CB_STATE_READY,
			new ui::EventCBListener(applauncherIconInfoItemOnReady, this));
	}

	ui::Widget *valueText = box->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_VALUE), 0);

	if (valueText != NULL) {
		if (m_storage == 1) {
			valueText->SetString(wstring(applauncherGetString(
				IDParam(s_empty_name, APPLAUNCHER_INFO_MSG_NONE))));
		} else if (g_applauncherIconInfoAppSize != 0) {
			wstring text;

			shellSizeText(&text, g_applauncherIconInfoAppSize);
			valueText->SetString(text);
			SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
			return;
		} else {
			valueText->SetString(applauncherIconInfoUtf16(string(".")));
		}
	}

	SetTimerListener(APPLAUNCHER_INFO_SIZE_TIMER, APPLAUNCHER_INFO_SIZE_TIMER_INTERVAL,
			 new ui::EventCBListener(applauncherIconInfoSizeItemOnTimer, this));
	m_job = common::SharedPtr<job::JobItem>(new ApplauncherIconInfoSizeJob(this));
	if ((m_job.get() == NULL || job::JobQueue::default_queue->Enqueue(m_job) < 0) &&
	    valueText != NULL)
		valueText->SetString(wstring(applauncherGetString(
			IDParam(s_empty_name, APPLAUNCHER_INFO_MSG_NONE))));
}

/* (0x810DCBEC) */
void ApplauncherIconInfoSizeItem::Setup(unsigned int label)
{
	m_label = label;

	Plugin *plugin = Plugin::Find("applauncher_plugin");

	if (plugin == NULL)
		return;

	plugin->TemplateOpen(this, IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_TEMPLATE),
			     Plugin::TemplateOpenParam());
	Update(m_label);
}

static void applauncherIconInfoSizeItemOnTimer(int32_t type, ui::Handler *self, ui::Event *e,
					       void *data)
{
	ApplauncherIconInfoSizeItem *item = (ApplauncherIconInfoSizeItem *)data;
	char dots[APPLAUNCHER_INFO_SIZE_DOTS + 2];
	int i;

	for (i = 0; i < item->m_dots; i++)
		dots[i] = '.';
	dots[i] = '\0';

	wstring text = applauncherIconInfoUtf16(string(dots));
	ui::Widget *valueText = item->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_VALUE),
						0);

	if (valueText != NULL) {
		valueText->SetString(text);
		valueText->SetColor(math::v4(1.0f, 1.0f, 1.0f, (float)((float)item->m_dots * 0.1)));
	}
	if (++item->m_dots >= APPLAUNCHER_INFO_SIZE_DOTS)
		item->m_dots = 0;
}

static void applauncherIconInfoSizeItemOnStop(int code, const void *data, SceSize size,
					      void *arg)
{
	ApplauncherIconInfoSizeItem *item = (ApplauncherIconInfoSizeItem *)arg;

	if (item->m_job.get() != NULL) {
		item->m_job->Cancel();
		item->m_job.reset();
	}
}

static void applauncherIconInfoSizeItemOnRestart(int code, const void *data, SceSize size,
						 void *arg)
{
	ApplauncherIconInfoSizeItem *item = (ApplauncherIconInfoSizeItem *)arg;

	if (g_applauncherIconInfoAppSize == 0)
		item->Update(item->m_label);
}

/* (0x810DCECE, 0x810DCFA2) */
ApplauncherIconInfoSizeItem::~ApplauncherIconInfoSizeItem()
{
	SceShell_D4C7D2B7(APPLAUNCHER_INFO_EVENT_RESTART, applauncherIconInfoSizeItemOnRestart,
			  this);
	SceShell_D4C7D2B7(APPLAUNCHER_INFO_EVENT_STOP, applauncherIconInfoSizeItemOnStop, this);
	if (m_job.get() != NULL) {
		m_job->Cancel();
		m_job.reset();
	}
}

/* a size from its #appsize field: in 32 KiB units, the not-counted values kept as they are */
static inline SceUInt64 applauncherAppSizeBytes(unsigned int units)
{
	if (units == GAME_APPSIZE_PATCH_MASK || units == GAME_APPSIZE_ADDCONT_MASK)
		return units;
	return (SceUInt64)units << GAME_APPSIZE_UNIT_SHIFT;
}

int ApplauncherIconInfoSizeJob::ReadAppSize(bool *noApp, bool *noPatch, bool *noAddcont,
					    SceUInt64 *appSize, SceUInt64 *app,
					    SceUInt64 *patch, SceUInt64 *addcont)
{
	*noApp = false;
	*noPatch = false;
	*noAddcont = false;
	*appSize = m_appSize;
	*app = 0;
	*patch = 0;
	*addcont = 0;

	if (m_hasAppSize) {
		SceUInt64 addcontSize;
		int ret;

		if ((unsigned int)(*appSize >> 61) != 0) {
			ret = -1;
			addcontSize = 0;
		} else {
			*app = applauncherAppSizeBytes(
				(unsigned int)(*appSize & GAME_APPSIZE_PATCH_MASK));
			*patch = applauncherAppSizeBytes((unsigned int)((*appSize >>
				GAME_APPSIZE_PATCH_SHIFT) & GAME_APPSIZE_PATCH_MASK));

			addcontSize = applauncherAppSizeBytes((unsigned int)((*appSize >>
				GAME_APPSIZE_ADDCONT_SHIFT) & GAME_APPSIZE_ADDCONT_MASK));
			*addcont = addcontSize;
			ret = 0;
		}

		if (ret == -1) {
			*noApp = true;
			*noPatch = true;
			*noAddcont = true;
		}
		if (*app == GAME_APPSIZE_PATCH_MASK)
			*noApp = true;
		if (*patch == GAME_APPSIZE_PATCH_MASK)
			*noPatch = true;
		if (addcontSize == GAME_APPSIZE_ADDCONT_MASK)
			*noAddcont = true;
	} else {
		*noApp = true;
		*noPatch = true;
		*noAddcont = true;
	}
	return 0;
}

/* the job's cancel test, polled by the count (0x810DD8A8) */
static int applauncherIconInfoSizeJobIsCanceled(void *arg);

int ApplauncherIconInfoSizeJob::CountGameSize(GamePluginInterface *game)
{
	SceUInt64 appSize;
	bool noApp, noPatch, noAddcont;
	SceUInt64 app, patch, addcont;
	int ret;

	appSize = 0;
	noApp = false;
	noPatch = false;
	noAddcont = false;
	app = 0;
	patch = 0;
	addcont = 0;
	ret = ReadAppSize(&noApp, &noPatch, &noAddcont, &appSize, &app, &patch, &addcont);
	if (ret < 0)
		return ret;
	if (IsCanceled())
		return APPLAUNCHER_ERROR_CANCELED;

	unsigned int flags = APPLAUNCHER_INFO_SIZES_DATA;

	if (noApp)
		flags |= APPLAUNCHER_INFO_SIZES_APP;
	if (noPatch)
		flags |= APPLAUNCHER_INFO_SIZES_PATCH;
	if (noAddcont)
		flags |= APPLAUNCHER_INFO_SIZES_ADDCONT;

	GameAppSizes sizes;

	sce_paf_memset(&sizes, 0, sizeof(sizes));
	ret = ((int (*)(const char *, unsigned int, GameCancelFunc, void *, GameAppSizes *))
	       game->func[GAME_PLUGIN_FUNC_TITLE_GET_SIZES])(m_titleId.c_str(), flags,
							      applauncherIconInfoSizeJobIsCanceled,
							      this, &sizes);
	if (ret < 0)
		return ret;
	if (IsCanceled())
		return APPLAUNCHER_ERROR_CANCELED;

	SceUInt64 total;
	unsigned int units;

	if (noApp) {
		total = sizes.app;
		units = gameAppSizeUnits(sizes.app);
		if (units <= GAME_APPSIZE_PATCH_MASK)
			applauncherAppSizeSetField(&appSize, units, 0, GAME_APPSIZE_PATCH_MASK);
	} else {
		total = app;
	}
	if (noPatch) {
		total += sizes.patch;
		units = gameAppSizeUnits(sizes.patch);
		if (units <= GAME_APPSIZE_PATCH_MASK)
			applauncherAppSizeSetField(&appSize, units, GAME_APPSIZE_PATCH_SHIFT,
						   GAME_APPSIZE_PATCH_MASK);
	} else {
		total = patch + total;
	}
	if (noAddcont) {
		total = total + sizes.addcont + sizes.data;
		if (sizes.addcontEncrypted) {
			units = gameAppSizeUnits(sizes.addcont);
			if (units <= GAME_APPSIZE_ADDCONT_MASK)
				applauncherAppSizeSetField(&appSize, units, GAME_APPSIZE_ADDCONT_SHIFT,
							   GAME_APPSIZE_ADDCONT_MASK);
		} else {
			applauncherAppSizeSetField(&appSize, GAME_APPSIZE_ADDCONT_MASK,
						   GAME_APPSIZE_ADDCONT_SHIFT, GAME_APPSIZE_ADDCONT_MASK);
		}
	} else {
		total = addcont + total + sizes.data;
	}

	if (noPatch || noApp || noAddcont) {
		LsdbValue value;

		value.integer = (SceInt64)(appSize & GAME_APPSIZE_MAX);
		SceLsdb_C36501F7(&m_titleId, LsdbKeyHasher<8>::Hash("#appsize"), &value, -1);
	}
	m_size = total;
	return 0;
}

/* the size counted: a game's by its #appsize, another application's whole (0x810DD60A) */
int32_t ApplauncherIconInfoSizeJob::Run()
{
	Plugin *plugin = Plugin::Find("game_plugin");

	if (plugin == NULL)
		return APPLAUNCHER_ERROR_NO_LIVEAREA;

	GamePluginInterface *game = (GamePluginInterface *)plugin->GetInterface(1);

	if (game == NULL)
		return APPLAUNCHER_ERROR_NO_LIVEAREA;

	int ret;

	if (m_appType == APPLAUNCHER_INFO_APPTYPE_GAME) {
		ret = CountGameSize(game);
	} else {
		GameAppSizes sizes;

		sce_paf_memset(&sizes, 0, sizeof(sizes));
		ret = ((int (*)(const char *, unsigned int, GameCancelFunc, void *,
				GameAppSizes *))game->func[GAME_PLUGIN_FUNC_TITLE_GET_SIZES])(
			m_titleId.c_str(), APPLAUNCHER_INFO_SIZES_ALL,
			applauncherIconInfoSizeJobIsCanceled, this, &sizes);
		if (ret >= 0) {
			ret = 0;
			m_size = sizes.total;
		}
	}
	if (ret < 0)
		return ret;
	return 0;
}

/* the dots stopped, and the size shown, or the size not known (0x810DD6A8) */
void ApplauncherIconInfoSizeJob::Finish(int32_t result)
{
	if (IsCanceled())
		return;

	ApplauncherIconInfoSizeItem *item = m_item;

	item->m_job.reset();
	item->KillTimer(APPLAUNCHER_INFO_SIZE_TIMER);

	ui::Widget *valueText = item->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_VALUE),
						0);

	if (valueText != NULL) {
		wstring text;

		if (result != 0 || m_size == 0)
			text = applauncherGetString(IDParam(s_empty_name, APPLAUNCHER_INFO_MSG_NONE));
		else
			shellSizeText(&text, m_size);
		valueText->SetString(text);
		valueText->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	}
}

static int applauncherIconInfoSizeJobIsCanceled(void *arg)
{
	return arg != NULL && ((job::JobItem *)arg)->IsCanceled();
}

/* PLACEHOLDER: the job of an item of the list: a function run on the queue and one called when
   done, both with the item (its code folds with ApplauncherChangeLogJob's) */
class ApplauncherIconInfoItemJob : public job::JobItem {
public:
	typedef int (*RunFunc)(ApplauncherIconInfoItemJob *job, void *arg);
	typedef void (*FinishFunc)(int result, void *arg);

	ApplauncherIconInfoItemJob(const char *name, void *arg, RunFunc run, FinishFunc finish)
		: job::JobItem(name, NULL), m_run(run), m_finish(finish), m_reserved(0), m_arg(arg)
	{
	}

	virtual ~ApplauncherIconInfoItemJob()
	{
	}

	virtual int Run()
	{
		if (m_run != NULL)
			return m_run(this, m_arg);
		return 0;
	}

	virtual void Finish(int result)
	{
		if (m_finish != NULL) {
			if (IsCanceled())
				result = APPLAUNCHER_ERROR_CANCELED;
			m_finish(result, m_arg);
		}
	}

	RunFunc m_run;
	FinishFunc m_finish;
	SceUInt32 m_reserved;
	void *m_arg;
};

/* the save data's state the list shows */
enum {
	APPLAUNCHER_INFO_SAVEDATA_NONE		= 0,	/* PLACEHOLDER names */
	APPLAUNCHER_INFO_SAVEDATA_ATTR		= 1,	/* its attribute's bit 0 set */
	APPLAUNCHER_INFO_SAVEDATA_FOUND		= 2,
	APPLAUNCHER_INFO_SAVEDATA_NOT_FOUND	= 3,
	APPLAUNCHER_INFO_SAVEDATA_NOT_READ	= 4,
};

/* (0x815443E0) */
int g_applauncherIconInfoSaveDataState = APPLAUNCHER_INFO_SAVEDATA_NOT_READ;
#if !defined(FW_TYPE_CEX)
static bool g_applauncherIconInfoSaveDataIsOnline = false;
static SceUInt32 g_applauncherIconInfoSaveDataQuotaKb = 0;
static SceUInt32 g_applauncherIconInfoSaveDataUsedKb = 0;
extern void shellSizeTextKb(paf::wstring *text, SceUInt32 sizeKb);

#define APPLAUNCHER_INFO_LABEL_SAVEDATA_MODE	0xC2400D6Du
#define APPLAUNCHER_INFO_LABEL_SAVEDATA_SIZE	0xFCB9D3CCu
#endif

/* PLACEHOLDER: the texts of the save data's state */
#define APPLAUNCHER_INFO_MSG_SAVEDATA		0x18C0105A
#define APPLAUNCHER_INFO_MSG_SAVEDATA_ATTR	0xEABB577A

/* PLACEHOLDER: what a job's Finish gets besides its own result when it stopped (value only) */
#define APPLAUNCHER_INFO_ERROR_STOPPED		((int)0x80101108)

/* the state read (0x810DE280), and shown (0x810DE3BE) */
static int applauncherIconInfoSaveDataRun(ApplauncherIconInfoItemJob *job, void *arg);
static void applauncherIconInfoSaveDataFinish(int result, void *arg);

ApplauncherIconInfoSaveDataItem::ApplauncherIconInfoSaveDataItem(const string &path,
	const string &dir, int attr, ui::Widget *parent, ui::ListItem::CreateParam *param)
	: ui::ListItem(parent, param), m_path(path), m_dir(dir), m_attr(attr), m_reserved(-1)
{
}

void ApplauncherIconInfoSaveDataItem::SetValue(ui::Widget *valueText)
{
	int state = g_applauncherIconInfoSaveDataState;
	wstring text(state == APPLAUNCHER_INFO_SAVEDATA_FOUND ?
		applauncherGetString(IDParam(s_empty_name, APPLAUNCHER_INFO_MSG_SAVEDATA)) :
		state == APPLAUNCHER_INFO_SAVEDATA_NOT_FOUND ?
		applauncherGetString(IDParam(s_empty_name, APPLAUNCHER_INFO_MSG_SAVEDATA)) :
		state == APPLAUNCHER_INFO_SAVEDATA_ATTR ?
		applauncherGetString(IDParam(s_empty_name, APPLAUNCHER_INFO_MSG_SAVEDATA_ATTR)) :
		applauncherGetString(IDParam(s_empty_name, APPLAUNCHER_INFO_MSG_NONE)));

	valueText->SetString(text);
	valueText->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
}

void ApplauncherIconInfoSaveDataItem::Setup(unsigned int label)
{
	Plugin *plugin = Plugin::Find("applauncher_plugin");

	if (plugin == NULL)
		return;

	plugin->TemplateOpen(this, IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_TEMPLATE),
			     Plugin::TemplateOpenParam());

	ui::Widget *box = APPLAUNCHER_INFO_FIND(this, ui::ListItem, APPLAUNCHER_INFO_ITEM_BOX);

	if (box == NULL)
		return;

	ui::Widget *labelText = box->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_LABEL), 0);

	if (labelText != NULL) {
		HideWidget(labelText, NULL);
		labelText->SetString(wstring(applauncherGetString(IDParam(s_empty_name, label))));
		labelText->AddEventListener(ui::Handler::CB_STATE_READY,
			new ui::EventCBListener(applauncherIconInfoItemOnReady, this));
	}

	ui::Widget *valueText = box->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_VALUE), 0);

	if (valueText != NULL) {
		HideWidget(valueText, NULL);
		if (g_applauncherIconInfoSaveDataState != APPLAUNCHER_INFO_SAVEDATA_NOT_READ) {
			SetValue(valueText);
			valueText->AddEventListener(ui::Handler::CB_STATE_READY,
				new ui::EventCBListener(applauncherIconInfoItemOnReady, this));
			return;
		}
		valueText->SetString(wstring(applauncherGetString(
			IDParam(s_empty_name, APPLAUNCHER_INFO_MSG_NONE))));
		valueText->AddEventListener(ui::Handler::CB_STATE_READY,
			new ui::EventCBListener(applauncherIconInfoItemOnReady, this));
	}

	m_job = common::SharedPtr<job::JobItem>(new ApplauncherIconInfoItemJob("InfoApp", this,
		applauncherIconInfoSaveDataRun, applauncherIconInfoSaveDataFinish));
	if (m_job.get() != NULL)
		job::JobQueue::default_queue->Enqueue(m_job);
}

static int applauncherIconInfoSaveDataRun(ApplauncherIconInfoItemJob *job, void *arg)
{
	ApplauncherIconInfoSaveDataItem *item = (ApplauncherIconInfoSaveDataItem *)arg;
	LocalFileStat stat;
	int ret;

	if ((item->m_attr & 1) != 0 || (ret = LocalFile::Getstat(item->m_path.c_str(), &stat)) >= 0) {
		char dir[16];

		sce_paf_strncpy(dir, item->m_dir.c_str(), 9);
		dir[9] = '\0';

		ApplauncherSaveDataInfo info;

		sce_paf_memset(&info, 0, sizeof(info));
		ret = sceAppMgrGetSaveDataInfo(item->m_path.c_str(), dir, item->m_attr, &info);
		if (ret >= 0) {
			g_applauncherIconInfoSaveDataState = (info.attr & 1) != 0 ?
				APPLAUNCHER_INFO_SAVEDATA_ATTR : APPLAUNCHER_INFO_SAVEDATA_FOUND;
#if !defined(FW_TYPE_CEX)
			g_applauncherIconInfoSaveDataQuotaKb = info.quotaKb;
			g_applauncherIconInfoSaveDataUsedKb = info.usedKb;
			g_applauncherIconInfoSaveDataIsOnline = (info.attr & 2) == 0;
#endif
			ret = 0;
		} else {
			g_applauncherIconInfoSaveDataState = ret == SCE_ERROR_ERRNO_ENOENT ?
				APPLAUNCHER_INFO_SAVEDATA_NOT_FOUND : APPLAUNCHER_INFO_SAVEDATA_NONE;
			if (ret == SCE_ERROR_ERRNO_ENOENT)
				ret = 0;
		}
	} else {
		g_applauncherIconInfoSaveDataState = APPLAUNCHER_INFO_SAVEDATA_NONE;
	}

	if (job->IsCanceled())
		return APPLAUNCHER_ERROR_CANCELED;
	return ret;
}

static void applauncherIconInfoSaveDataFinish(int result, void *arg)
{
	ApplauncherIconInfoSaveDataItem *item = (ApplauncherIconInfoSaveDataItem *)arg;

	if (result == APPLAUNCHER_ERROR_CANCELED || result == APPLAUNCHER_INFO_ERROR_STOPPED)
		return;

	item->m_job.reset();

	ui::Widget *valueText = item->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_VALUE),
						0);

	if (valueText == NULL)
		return;

	if (result != 0) {
		wstring text(applauncherGetString(IDParam(s_empty_name, APPLAUNCHER_INFO_MSG_NONE)));

		valueText->SetString(text);
		valueText->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	} else {
		item->SetValue(valueText);
	}
}

/* (0x810DE5A2, 0x810DE674) */
ApplauncherIconInfoSaveDataItem::~ApplauncherIconInfoSaveDataItem()
{
	if (m_job.get() != NULL) {
		m_job->Cancel();
		m_job.reset();
	}
}

#if !defined(FW_TYPE_CEX)
extern "C" int vshAppMgrCloudDataSrcMount(int mountType, const void *titleId, void *mountPoint);

static int applauncherIconInfoSaveDataDetailRun(ApplauncherIconInfoItemJob *job, void *userContext);
static void applauncherIconInfoSaveDataDetailFinish(int result, void *userContext);

ApplauncherIconInfoSaveDataDetailItem::ApplauncherIconInfoSaveDataDetailItem(
	ui::Widget *saveDataItem, ui::Widget *parent, ui::ListItem::CreateParam *param)
	: ui::ListItem(parent, param), m_saveDataItem(saveDataItem), m_label(0)
{
}

ApplauncherIconInfoSaveDataDetailItem::~ApplauncherIconInfoSaveDataDetailItem()
{
	if (m_job.get() != NULL) {
		m_job->Cancel();
		m_job.reset();
	}
}

void ApplauncherIconInfoSaveDataDetailItem::SetModeValue(ui::Widget *valueText)
{
	wstring text;

	if (g_applauncherIconInfoSaveDataState == APPLAUNCHER_INFO_SAVEDATA_NONE) {
		text = applauncherGetString(IDParam(s_empty_name, APPLAUNCHER_INFO_MSG_NONE));
	} else if (g_applauncherIconInfoSaveDataIsOnline) {
		text = wstring(L"Online");
	} else {
		text = wstring(L"Local");
	}
	if (valueText != NULL)
		valueText->SetString(text);
	valueText->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
}

void ApplauncherIconInfoSaveDataDetailItem::SetSizeValue(ui::Widget *valueText)
{
	wstring text;
	wstring part;

	if (g_applauncherIconInfoSaveDataState == APPLAUNCHER_INFO_SAVEDATA_NONE) {
		text = applauncherGetString(IDParam(s_empty_name, APPLAUNCHER_INFO_MSG_NONE));
	} else {
		shellSizeText(&part, (SceUInt64)(g_applauncherIconInfoSaveDataUsedKb << 10));
		text = part;
		shellSizeTextKb(&part, g_applauncherIconInfoSaveDataQuotaKb);
		text.append(L" / ");
		text.append(part);
	}
	if (valueText != NULL)
		valueText->SetString(text);
	valueText->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
}

void ApplauncherIconInfoSaveDataDetailItem::Setup(unsigned int label)
{
	m_label = label;

	Plugin *plugin = Plugin::Find("applauncher_plugin");

	if (plugin == NULL)
		return;

	plugin->TemplateOpen(this, IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_TEMPLATE),
			     Plugin::TemplateOpenParam());

	ui::Widget *box = APPLAUNCHER_INFO_FIND(this, ui::ListItem, APPLAUNCHER_INFO_ITEM_BOX);

	if (box == NULL)
		return;

	ui::Widget *labelText = box->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_LABEL), 0);

	if (labelText != NULL) {
		HideWidget(labelText, NULL);
		labelText->SetString(wstring(applauncherGetString(IDParam(s_empty_name, label))));
		labelText->AddEventListener(ui::Handler::CB_STATE_READY,
			new ui::EventCBListener(applauncherIconInfoItemOnReady, this));
	}

	ui::Widget *valueText = box->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_VALUE), 0);

	if (valueText != NULL) {
		HideWidget(valueText, NULL);
		if (g_applauncherIconInfoSaveDataState != APPLAUNCHER_INFO_SAVEDATA_NOT_READ) {
			if (m_label == APPLAUNCHER_INFO_LABEL_SAVEDATA_MODE)
				SetModeValue(valueText);
			else if (m_label == APPLAUNCHER_INFO_LABEL_SAVEDATA_SIZE)
				SetSizeValue(valueText);
			valueText->AddEventListener(ui::Handler::CB_STATE_READY,
				new ui::EventCBListener(applauncherIconInfoItemOnReady, this));
			return;
		}
		valueText->SetString(wstring(applauncherGetString(
			IDParam(s_empty_name, APPLAUNCHER_INFO_MSG_NONE))));
		valueText->AddEventListener(ui::Handler::CB_STATE_READY,
			new ui::EventCBListener(applauncherIconInfoItemOnReady, this));
	}

	if (m_saveDataItem == NULL)
		return;

	m_job = common::SharedPtr<job::JobItem>(new ApplauncherIconInfoItemJob("InfoApp", this,
		applauncherIconInfoSaveDataDetailRun, applauncherIconInfoSaveDataDetailFinish));
	if (m_job.get() != NULL)
		job::JobQueue::default_queue->Enqueue(m_job);
}

static int applauncherIconInfoSaveDataDetailRun(ApplauncherIconInfoItemJob *job, void *userContext)
{
	ApplauncherIconInfoSaveDataDetailItem *item =
		(ApplauncherIconInfoSaveDataDetailItem *)userContext;

	if (job->IsCanceled())
		return APPLAUNCHER_ERROR_CANCELED;
	if (item != NULL && item->m_saveDataItem == NULL)
		return -2146422015;
	return 0;
}

static void applauncherIconInfoSaveDataDetailFinish(int result, void *userContext)
{
	ApplauncherIconInfoSaveDataDetailItem *item =
		(ApplauncherIconInfoSaveDataDetailItem *)userContext;

	if (result == APPLAUNCHER_ERROR_CANCELED)
		return;

	item->m_job.reset();

	ui::Widget *valueText = item->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_VALUE),
						0);

	if (valueText == NULL)
		return;

	if (result != 0) {
		wstring text(applauncherGetString(IDParam(s_empty_name, APPLAUNCHER_INFO_MSG_NONE)));

		valueText->SetString(text);
		valueText->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	} else if (item->m_label == APPLAUNCHER_INFO_LABEL_SAVEDATA_MODE) {
		item->SetModeValue(valueText);
	} else if (item->m_label == APPLAUNCHER_INFO_LABEL_SAVEDATA_SIZE) {
		item->SetSizeValue(valueText);
	}
	valueText->AddEventListener(ui::Handler::CB_STATE_READY,
		new ui::EventCBListener(applauncherIconInfoItemOnReady, item));
}

/* SaveData slot chunk reader constants (TOOL/DEBUGGER) */
#define APPLAUNCHER_SDSLOT_CHUNK_SHIFT		15
#define APPLAUNCHER_SDSLOT_CHUNK_SIZE		0x8000u
#define APPLAUNCHER_SDSLOT_CHUNK_MASK		0x7FFFu
#define APPLAUNCHER_SDSLOT_HEADER_SIZE		0x400u
#define APPLAUNCHER_SDSLOT_FLAG_OFFSET		0x200u
#define APPLAUNCHER_SDSLOT_COUNT		256u
#define APPLAUNCHER_SDSLOT_CACHE_SIZE		257u
#define APPLAUNCHER_SDSLOT_MOUNT_GRO_RW		1
#define APPLAUNCHER_SDSLOT_MOUNT_GRO_RO		2
#define APPLAUNCHER_SDSLOT_MOUNT_UX		3

static int applauncherIconInfoSaveDataSlotRun(ApplauncherIconInfoItemJob *job, void *arg);
static void applauncherIconInfoSaveDataSlotFinish(int result, void *arg);

ApplauncherIconInfoSaveDataSlotItem::ApplauncherIconInfoSaveDataSlotItem(const string &path,
	const string &dir, int attr, ui::Widget *parent, ui::ListItem::CreateParam *param)
	: ui::ListItem(parent, param), m_path(path), m_dir(dir), m_attr(attr), m_error(0)
{
}

ApplauncherIconInfoSaveDataSlotItem::~ApplauncherIconInfoSaveDataSlotItem()
{
	if (m_job.get() != NULL) {
		m_job->Cancel();
		m_job.reset();
	}
}

void ApplauncherIconInfoSaveDataSlotItem::SetValue(ui::Widget *valueText)
{
	wstring text;
	string numStr;

	if (valueText == NULL)
		return;

	if (m_error != 0) {
		text = wstring(L"-");
	} else {
		int foundCount = 0;

		for (SceUInt32 idx = 0; idx < APPLAUNCHER_SDSLOT_COUNT; ++idx) {
			SceUInt8 state = m_slotStates[idx];
			char numBuf[64];

			if (state == 1) {
				if (foundCount != 0)
					text.append(L", ");
				sce_paf_snprintf(numBuf, sizeof(numBuf), "%d", (int)idx);
				numStr = string(numBuf);
				text.append(applauncherIconInfoUtf16(numStr));
				++foundCount;
			} else if (state == 2) {
				if (foundCount != 0)
					text.append(L", ");
				sce_paf_snprintf(numBuf, sizeof(numBuf), "%d", (int)idx);
				numStr = string(numBuf);
				text.append(applauncherIconInfoUtf16(numStr));
				text.append(L"*");
				++foundCount;
			}
		}
		if (foundCount == 0)
			text = wstring(L"-");
	}

	valueText->SetString(text);
}

void ApplauncherIconInfoSaveDataSlotItem::Setup(const wstring &label, SceUInt8 *cache)
{
	Plugin *plugin = Plugin::Find("applauncher_plugin");

	if (plugin == NULL)
		return;

	plugin->TemplateOpen(this, IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_TEMPLATE),
			     Plugin::TemplateOpenParam());

	ui::Widget *box = APPLAUNCHER_INFO_FIND(this, ui::ListItem, APPLAUNCHER_INFO_ITEM_BOX);

	if (box == NULL)
		return;

	ui::Widget *labelText = box->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_LABEL), 0);

	if (labelText != NULL) {
		HideWidget(labelText, NULL);
		labelText->SetString(label);
		labelText->AddEventListener(ui::Handler::CB_STATE_READY,
			new ui::EventCBListener(applauncherIconInfoItemOnReady, this));
	}

	m_cache = cache;
	sceClibMemcpy_safe(m_slotStates, cache, APPLAUNCHER_SDSLOT_CACHE_SIZE);

	ui::Widget *valueText = box->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_VALUE), 0);

	if (m_loaded != 0) {
		if (valueText != NULL) {
			SetValue(valueText);
			valueText->AddEventListener(ui::Handler::CB_STATE_READY,
				new ui::EventCBListener(applauncherIconInfoItemOnReady, this));
		}
		return;
	}

	m_job = common::SharedPtr<job::JobItem>(new ApplauncherIconInfoItemJob("InfoApp", this,
		applauncherIconInfoSaveDataSlotRun, applauncherIconInfoSaveDataSlotFinish));
	if (m_job.get() != NULL && job::JobQueue::default_queue->Enqueue(m_job) >= 0) {
		if (valueText != NULL)
			valueText->AddEventListener(ui::Handler::CB_STATE_READY,
				new ui::EventCBListener(applauncherIconInfoItemOnReady, this));
	}
}

int ApplauncherIconInfoSaveDataSlotItem::ReadSlotChunk(char *buf, SceUID fd, SceUInt32 offset,
						       int *outInChunkOffset,
						       SceUInt32 *cachedChunk)
{
	if (buf == NULL)
		return APPLAUNCHER_ERROR_INVALID_ARG;

	SceUInt32 chunkIndex = 0;
	int inChunkOffset = 0;

	if (offset != 0) {
		chunkIndex = offset >> APPLAUNCHER_SDSLOT_CHUNK_SHIFT;
		inChunkOffset = (int)(offset & APPLAUNCHER_SDSLOT_CHUNK_MASK);
	}

	if (*cachedChunk != chunkIndex) {
		SceOff seekPos = (SceOff)(chunkIndex << APPLAUNCHER_SDSLOT_CHUNK_SHIFT);
		SceOff seekRet = sceIoLseek(fd, seekPos, SCE_SEEK_SET);

		if (seekRet < 0)
			return (int)seekRet;

		int readBytes = sceIoRead(fd, buf, APPLAUNCHER_SDSLOT_CHUNK_SIZE);

		if (readBytes < 0)
			return readBytes;
		if ((SceUInt32)readBytes < APPLAUNCHER_SDSLOT_CHUNK_SIZE) {
			for (SceUInt32 pos = (SceUInt32)readBytes;
			     pos < APPLAUNCHER_SDSLOT_CHUNK_SIZE; ++pos) {
				buf[pos] = 0;
			}
		}
	}

	*cachedChunk = chunkIndex;
	*outInChunkOffset = inChunkOffset;
	return 0;
}

int ApplauncherIconInfoSaveDataSlotItem::ParseSaveDataSlots(const char *mountPoint, char *buf)
{
	if (buf == NULL || mountPoint == NULL)
		return APPLAUNCHER_ERROR_INVALID_ARG;

	int slotStatus = 0;
	int chunkOffset = 0;
	SceUInt32 cachedChunk = 0xFFFFFFFFu;
	char slotPath[256];

	sceClibMemset(slotPath, 0, sizeof(slotPath));
	sce_paf_snprintf(slotPath, sizeof(slotPath), "%s%s/%s", mountPoint, "sce_sys", "sdslot.dat");

	SceUID fd = sceIoOpen(slotPath, SCE_O_RDONLY, 0);

	if (fd < 0)
		return fd;

	int ret = ReadSlotChunk(buf, fd, 0, &chunkOffset, &cachedChunk);

	if (ret == 0) {
		SceUInt8 header[APPLAUNCHER_SDSLOT_HEADER_SIZE];

		sceClibMemcpy_safe(header, buf + chunkOffset, sizeof(header));
		sce_paf_memset(m_slotStates, 0, APPLAUNCHER_SDSLOT_CACHE_SIZE);

		for (SceUInt32 idx = 0; idx < APPLAUNCHER_SDSLOT_COUNT; ++idx) {
			if (header[APPLAUNCHER_SDSLOT_FLAG_OFFSET + idx] == 1) {
				ret = ReadSlotChunk(buf, fd,
					APPLAUNCHER_SDSLOT_HEADER_SIZE + (idx << 10),
					&chunkOffset, &cachedChunk);
				if (ret != 0)
					break;
				sceClibMemcpy_safe(&slotStatus, buf + chunkOffset, sizeof(slotStatus));
				m_slotStates[idx] = (slotStatus != 0) ? 2 : 1;
			}
		}

		m_loaded = 0xFFu;
		sceClibMemcpy_safe(m_cache, m_slotStates, APPLAUNCHER_SDSLOT_CACHE_SIZE);
	}

	sceIoClose(fd);
	return ret;
}

static int applauncherIconInfoSaveDataSlotRun(ApplauncherIconInfoItemJob *job, void *arg)
{
	ApplauncherIconInfoSaveDataSlotItem *item = (ApplauncherIconInfoSaveDataSlotItem *)arg;

	if (job->IsCanceled())
		return APPLAUNCHER_ERROR_CANCELED;

	int mountType = APPLAUNCHER_SDSLOT_MOUNT_UX;

	if (sce_paf_strncmp(item->m_path.c_str(), "gro0:", sce_paf_strlen("gro0:")) == 0)
		mountType = (item->m_attr & 1) != 0 ?
			    APPLAUNCHER_SDSLOT_MOUNT_GRO_RO : APPLAUNCHER_SDSLOT_MOUNT_GRO_RW;

	char titleId[16];
	char mountPoint[16];

	sce_paf_memset(titleId, 0, sizeof(titleId));
	sce_paf_strncpy(titleId, item->m_dir.c_str(), 9);
	sce_paf_memset(mountPoint, 0, sizeof(mountPoint));

	int ret = vshAppMgrCloudDataSrcMount(mountType, titleId, mountPoint);

	if (ret == 0) {
		char *buf = (char *)sce_paf_malloc(APPLAUNCHER_SDSLOT_CHUNK_SIZE);

		if (buf != NULL)
			ret = item->ParseSaveDataSlots(mountPoint, buf);
		else
			ret = APPLAUNCHER_ERROR_NO_MEMORY;

		sceAppMgrUmount(mountPoint);
		if (buf != NULL)
			sce_paf_free(buf);
	}

	item->m_error = ret;
	return ret;
}

static void applauncherIconInfoSaveDataSlotFinish(int result, void *arg)
{
	ApplauncherIconInfoSaveDataSlotItem *item = (ApplauncherIconInfoSaveDataSlotItem *)arg;

	if (result == APPLAUNCHER_ERROR_CANCELED)
		return;

	item->m_job.reset();

	ui::Widget *valueText = item->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_VALUE), 0);

	item->SetValue(valueText);
}
#endif /* !FW_TYPE_CEX */

/* the auto-start check's item: its template, label and check box */
#define APPLAUNCHER_INFO_CHECK_TEMPLATE		0x3947CB9E
#define APPLAUNCHER_INFO_CHECK_LABEL		0x071E2A0C
#define APPLAUNCHER_INFO_CHECK_BOX		0xDC729926
#define APPLAUNCHER_INFO_CHECK_MSG		0x43EC15D9	/* PLACEHOLDER: the label's text */

/* PLACEHOLDER: the #appconf bit the check box shows cleared */
#define APPLAUNCHER_INFO_APPCONF_CHECK		0x800

/* PLACEHOLDER: the usage counter of the check set */
#define APPLAUNCHER_INFO_COUNTER_CHECK		244

/*
 * PLACEHOLDER: the "DispInfomationDialogJob": the check written to the application's #appconf,
 * counted when it is set (48 bytes, vtable 0x8152FCAC).
 */
class ApplauncherIconInfoCheckJob : public job::JobItem {
public:
	ApplauncherIconInfoCheckJob(const string &titleId, bool checked,
				    ApplauncherIconInfoCheckItem *item)
		: job::JobItem("DispInfomationDialogJob", NULL), m_titleId(titleId),
		  m_checked(checked), m_item(item)
	{
	}
	virtual ~ApplauncherIconInfoCheckJob()
	{
	}

	virtual int32_t Run();
	virtual void Finish(int32_t result);

	string m_titleId;
	bool m_checked;
	ApplauncherIconInfoCheckItem *m_item;
};

/* the check read (0x810DEF5C), and shown (0x810DF02A) */
static int applauncherIconInfoCheckRun(ApplauncherIconInfoItemJob *job, void *arg);
static void applauncherIconInfoCheckFinish(int result, void *arg);

ApplauncherIconInfoCheckItem::ApplauncherIconInfoCheckItem(const string &titleId,
	ui::Widget *parent, ui::ListItem::CreateParam *param)
	: ui::ListItem(parent, param), m_titleId(titleId), m_checked(false), m_box(NULL)
{
}

void ApplauncherIconInfoCheckItem::Setup()
{
	Plugin *plugin = Plugin::Find("applauncher_plugin");

	if (plugin == NULL)
		return;

	plugin->TemplateOpen(this, IDParam(s_empty_name, APPLAUNCHER_INFO_CHECK_TEMPLATE),
			     Plugin::TemplateOpenParam());
	m_box = APPLAUNCHER_INFO_FIND(this, ui::ListItem, APPLAUNCHER_INFO_ITEM_BOX);
	if (m_box == NULL)
		return;

	ui::Widget *labelText = m_box->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_CHECK_LABEL),
						 0);

	if (labelText != NULL) {
		HideWidget(labelText, NULL);
		labelText->SetString(wstring(applauncherGetString(
			IDParam(s_empty_name, APPLAUNCHER_INFO_CHECK_MSG))));
		labelText->AddEventListener(ui::Handler::CB_STATE_READY,
			new ui::EventCBListener(applauncherIconInfoItemOnReady, this));
	}

	ui::Widget *check = APPLAUNCHER_INFO_FIND(m_box, ui::CheckBox, APPLAUNCHER_INFO_CHECK_BOX);

	if (check != NULL)
		HideWidget(check, NULL);

	m_job = common::SharedPtr<job::JobItem>(new ApplauncherIconInfoItemJob("InfoApp", this,
		applauncherIconInfoCheckRun, applauncherIconInfoCheckFinish));
	if (m_job.get() != NULL)
		job::JobQueue::default_queue->Enqueue(m_job);
}

/* (0x810DED72, 0x810DEF4C) */
ApplauncherIconInfoCheckItem::~ApplauncherIconInfoCheckItem()
{
	if (m_job.get() != NULL) {
		m_job->Cancel();
		m_job.reset();
	}
	if (m_setJob.get() != NULL) {
		m_setJob->Cancel();
		m_setJob.reset();
	}
}

static int applauncherIconInfoCheckRun(ApplauncherIconInfoItemJob *job, void *arg)
{
	ApplauncherIconInfoCheckItem *item = (ApplauncherIconInfoCheckItem *)arg;

	if (job->IsCanceled())
		return APPLAUNCHER_ERROR_CANCELED;

	int ret = 0;

	if (item != NULL) {
		AppInfoSClient client;

		SceLsdb_D322A870(&client);
		{
			LsdbValue value;

			ret = SceLsdb_DB73BD7E(&client, &item->m_titleId, LSDB_KEY_APPCONF, &value);
			if (ret == 0)
				item->m_checked = (gameLsdbValueInteger(value) &
						   APPLAUNCHER_INFO_APPCONF_CHECK) == 0;
		}
		SceLsdb_56D8DCF0(&client);
	}
	return ret;
}

/* the check box changed: the job writing it started (0x810DF328) */
static void applauncherIconInfoCheckOnChanged(int32_t type, ui::Handler *self, ui::Event *e,
					      void *data);

static void applauncherIconInfoCheckFinish(int result, void *arg)
{
	ApplauncherIconInfoCheckItem *item = (ApplauncherIconInfoCheckItem *)arg;

	if (result == APPLAUNCHER_ERROR_CANCELED)
		return;

	item->m_job.reset();

	ui::Widget *check = item->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_CHECK_BOX), 0);

	if (check != NULL) {
		((ui::CheckBox *)check)->SetCheck(item->m_checked, -0.1f);
		check->AddEventListener(ui::StateButtonBase::CB_STBTN_VALUE_CHANGED,
			new ui::EventCBListener(applauncherIconInfoCheckOnChanged, item));
	}

	ui::Widget *labelText = item->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_CHECK_LABEL),
						0);
	ui::ListView *listView = item->GetListView() != NULL &&
		item->GetListView()->IsInherit(ui::ListView::TypeName()) ? item->GetListView() : NULL;

	if (listView != NULL && labelText != NULL && check != NULL) {
		listView->SetCellSize(item->GetSegmentIndex(), item->GetCellIndex(),
				      math::v4(380.0f, 80.0f, 0.0f, 0.0f));
		ScePafWidget_44B22ACA(labelText, common::transition::Type_FadeinFast, 200.0f, NULL,
				      NULL);
		ScePafWidget_44B22ACA(check, common::transition::Type_FadeinFast, 200.0f, NULL, NULL);
	}
}

/* (0x810DF296) */
static __attribute__((noinline)) common::SharedPtr<job::JobItem>
applauncherIconInfoCheckJobCreate(const string &titleId,
	bool checked, ApplauncherIconInfoCheckItem *item)
{
	return common::SharedPtr<job::JobItem>(new ApplauncherIconInfoCheckJob(titleId, checked,
									       item));
}

static void applauncherIconInfoCheckOnChanged(int32_t type, ui::Handler *self, ui::Event *e,
					      void *data)
{
	ApplauncherIconInfoCheckItem *item = (ApplauncherIconInfoCheckItem *)data;
	ui::Widget *check = item->m_box->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_CHECK_BOX),
						   0);

	if (check == NULL)
		return;

	item->m_setJob = applauncherIconInfoCheckJobCreate(item->m_titleId,
		((ui::CheckBox *)check)->m_is_checked, item);
	if (item->m_setJob.get() != NULL)
		job::JobQueue::default_queue->Enqueue(item->m_setJob);
}

/* (0x810DF528) */
int32_t ApplauncherIconInfoCheckJob::Run()
{
	AppInfoSClient client;

	SceLsdb_D322A870(&client);

	int ret;

	{
		LsdbValue value;

		value.integer = m_checked ? (SceInt64)0 : (SceInt64)APPLAUNCHER_INFO_APPCONF_CHECK;
		ret = SceLsdb_7D7F6809(&client, &m_titleId, LSDB_KEY_APPCONF, &value,
				       APPLAUNCHER_INFO_APPCONF_CHECK);
	}
	if (m_checked == true)
		shellUsageCounterAdd(APPLAUNCHER_INFO_COUNTER_CHECK, 1, 0);
	SceLsdb_56D8DCF0(&client);
	return ret;
}

/* (0x810DF5DC) */
void ApplauncherIconInfoCheckJob::Finish(int32_t result)
{
	if (IsCanceled())
		return;
	m_item->m_setJob.reset();
}

/* (0x810DF654) */
ApplauncherIconInfoFactory::ApplauncherIconInfoFactory(Plugin *plugin,
	const common::SharedPtr<ApplauncherIcon> &icon, LsdbAppInfoPtr *appInfo)
	: m_plugin(plugin), m_icon(icon), m_closed(false), m_appInfo(appInfo), m_index(-1),
	  m_itemCount(0), m_items(NULL)
{
}

/* (0x810DF6A4, 0x810DF70E) */
ApplauncherIconInfoFactory::~ApplauncherIconInfoFactory()
{
}

/* PLACEHOLDER: the application's title (0x810DF726) */
void ApplauncherIconInfoFactory::GetTitle(wstring *title)
{
	common::Utf8ToUtf16(string(sceLsdbGetTitle(m_appInfo)), title, 0);
}

/* PLACEHOLDER: a time in UTC as the local time's text (0x810DF792) */
int applauncherIconInfoFormatTime(const rtc::Tick *utc, wstring *text)
{
	rtc::Tick local;
	int ret = rtc::ConvertUtcToLocalTime(utc, &local);

	if (ret < 0)
		return ret;

	rtc::DateTime dt;

	ret = rtc::ConvertTickToDateTime(&local, &dt, NULL);
	if (ret < 0)
		return ret;

	wchar_t buf[64] = { 0 };
	datetime::DateTime time(dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second,
				dt.microsecond);

	time.Format(buf, sizeof(buf), L"%c", false);
	text->assign(buf, sce_paf_wcslen(buf));
	return 0;
}

/* PLACEHOLDER: an SQLite time in UTC as the local time's text (0x810DF844) */
int applauncherIconInfoFormatSQLiteTime(const string *sqlTime, wstring *text)
{
	rtc::Tick utc;
	int ret = rtc::ParseSQLiteDateTime(&utc, sqlTime->c_str());

	if (ret < 0)
		return ret;

	rtc::Tick local;

	ret = rtc::ConvertUtcToLocalTime(&utc, &local);
	if (ret < 0)
		return ret;

	rtc::DateTime dt;

	ret = rtc::ConvertTickToDateTime(&local, &dt, NULL);
	if (ret >= 0) {
		ret = 0;

		wchar_t buf[64] = { 0 };
		datetime::DateTime time(dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second,
					dt.microsecond);

		time.Format(buf, sizeof(buf), L"%c", false);
		text->assign(buf, sce_paf_wcslen(buf));
	}
	return ret;
}

/* PLACEHOLDER: the application's APP_VER as it is shown, without its leading zero; empty for
   "0.00" and "00.00" (0x810DF90E) */
void applauncherIconInfoGetVersion(LsdbAppInfoPtr *appInfo, wstring *version)
{
	version->assign(NULL, 0);

	LsdbAppInfo::iterator it = (*appInfo)->find(LSDB_KEY_APP_VER);

	if (it == (*appInfo)->end())
		return;

	const char *text = it->second.type == LSDB_VALUE_TEXT && it->second.text != NULL ?
		it->second.text->c_str() : "";

	if (text[1] == '.') {
		if (sce_paf_strncmp(text, "0.00", 5) != 0)
			common::Utf8ToUtf16(string(text), version, 0);
	} else if (text[2] == '.' && sce_paf_strncmp(text, "00.00", 6) != 0) {
		if (text[0] == '0')
			text++;
		common::Utf8ToUtf16(string(text), version, 0);
	}
}

/*
 * An item's label or value laid out: the cell sized to the taller of the two, both centred in it
 * (0x810DFA5A).
 */
static void applauncherIconInfoItemOnReady(int32_t type, ui::Handler *self, ui::Event *e,
					   void *data)
{
	if (self == NULL)
		return;

	ui::ListItem *item = (ui::ListItem *)data;
	ui::ListView *listView = item->GetListView() != NULL &&
		item->GetListView()->IsInherit(ui::ListView::TypeName()) ? item->GetListView() : NULL;
	ui::Widget *labelText = item->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_LABEL), 0);
	ui::Widget *valueText = item->FindChild(IDParam(s_empty_name, APPLAUNCHER_INFO_ITEM_VALUE), 0);

	if (listView == NULL || labelText == NULL || valueText == NULL)
		return;
	if (labelText->m_state != 0 || valueText->m_state != 0)
		return;

	math::v4 size;
	math::v4 pos;

	if (valueText->GetSize(0)->extract_y() < labelText->GetSize(0)->extract_y()) {
		size = *labelText->GetSize(0);
		listView->SetCellSize(item->GetSegmentIndex(), item->GetCellIndex(), size);
		pos = valueText->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		valueText->SetPos(math::v4(pos.extract_x(),
					   labelText->GetSize(0)->extract_y() * 0.5f, 0.0f, 0.0f));
		pos = labelText->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		labelText->SetPos(math::v4(pos.extract_x(),
					   labelText->GetSize(0)->extract_y() * 0.5f, 0.0f, 0.0f));
	} else {
		size = *valueText->GetSize(0);
		listView->SetCellSize(item->GetSegmentIndex(), item->GetCellIndex(), size);
		pos = valueText->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		valueText->SetPos(math::v4(pos.extract_x(),
					   valueText->GetSize(0)->extract_y() * 0.5f, 0.0f, 0.0f));
		pos = labelText->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		labelText->SetPos(math::v4(pos.extract_x(),
					   valueText->GetSize(0)->extract_y() * 0.5f, 0.0f, 0.0f));
	}

	valueText->SetAlign(valueText->m_align_x, 2, valueText->m_align_z, NULL);
	labelText->SetAlign(labelText->m_align_x, 2, labelText->m_align_z, NULL);
	ScePafWidget_44B22ACA(labelText, common::transition::Type_FadeinFast, 200.0f, NULL, NULL);
	ScePafWidget_44B22ACA(valueText, common::transition::Type_FadeinFast, 200.0f, NULL, NULL);
}

/* (0x81555E90) */
SceUInt64 g_applauncherIconInfoAppSize;
