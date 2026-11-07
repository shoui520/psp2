/*
 * SceInvitationDlgPlugin (vs0:sys/external/libcdlg_invitation.suprx): the invitation dialog's
 * plugin, which SceCommonDialogMain loads for libcdlg's sceInvitationDialog*. Its thread runs
 * the dialog's steps one request at a time: the lists the user picks the addressees from
 * (contacts_list_plugin, friend_select_plugin, party_memberlist_plugin), then the dialog itself
 * (invitation_dialog_impl_plugin, libcdlg_invitation_impl), and closes the dialog when the impl
 * plugin's dialog ends. The names the plugin writes into the work memory (CommonDialogWork's
 * apiName) are Sony's function names: Initialize, Run, the three ...SelectedCallbackProc and
 * the work memory's accessors. The file's name and the other names are ours.
 *
 * SceGameCustomDataDlgPlugin (vs0:sys/external/libcdlg_game_custom_data.suprx) is Sony's copy
 * of this plugin for the game custom data dialog, laid out function for function the same:
 * ../game_custom_data_dialog builds this file with GAME_CUSTOM_DATA_DIALOG defined, which
 * selects its dialog type, work memory, names and errors below. Its copy calls SceLibKernel's
 * sceClib* where this one calls paf's sce_paf_* in the work memory's lock, the mounts, the
 * party list's selection, the dialog's end and the three ...SelectedCallbackProc.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/libkernel.h>
#include <libkernel/libkernel.h>
#include <appmgr.h>
#include <libhttp.h>
#include <libssl.h>
#include <np/np_error.h>
#include <invitation_dialog.h>
#include <game_custom_data_dialog.h>
#include <sysmodule/sysmodule.h>
#include <plugin.h>
#include <framework.h>
#include <module/module.h>
#include <thread/thread.h>
#include <common/main_thread_call_list.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include <pfsmgr/pfsmgr.h>
#include <std/string.h>
#include <std/list>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_mgr/common_dialog_mgr_internal.h"
#include "../cdlg_main/common_dialog_main.h"
#include "../../../app/common/src/np/np_manager/np_manager_internal.h"
#include "../../../app/common/src/np/np_community/np_community.h"
#include "../../../app/common/src/np/np_message_contacts/np_message_contacts.h"
#include "../../../../sys/internal/usermode/src/party_member_list/party_member_list.h"

extern "C" {
#include "../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;

#if defined(GAME_CUSTOM_DATA_DIALOG)
#define PLUGIN_DIALOG_TYPE			COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA
typedef CommonDialogGameCustomDataParam PluginWorkParam;
typedef SceGameCustomDataDialogResult PluginResult;
#define PLUGIN_NAME				"SceGameCustomDataDlgPlugin"
#define PLUGIN_PAGE_PLUGIN			"game_custom_data_dialog_plugin"
#define PLUGIN_IMPL_PLUGIN			"game_custom_data_dialog_impl_plugin"
#define PLUGIN_IMPL_RESOURCE			"game_custom_data_dialog_impl_plugin.rco"
#define PLUGIN_IMPL_MODULE			"libcdlg_game_custom_data_impl.suprx"
#define PLUGIN_ADDRESS_MAX			SCE_GAME_CUSTOM_DATA_DIALOG_ADDRESS_USER_LIST_MAX_NUMBER
#define PLUGIN_LEAST_HTTP_POOL_SIZE		SCE_GAME_CUSTOM_DATA_DIALOG_LEAST_HTTP_POOL_SIZE
#define PLUGIN_LEAST_SSL_POOL_SIZE		SCE_GAME_CUSTOM_DATA_DIALOG_LEAST_SSL_POOL_SIZE
#define PLUGIN_ERROR_LACK_OF_LIBHTTP_POOL_SIZE	SCE_GAME_CUSTOM_DATA_DIALOG_ERROR_LACK_OF_LIBHTTP_POOL_SIZE
#define PLUGIN_ERROR_LACK_OF_LIBSSL_POOL_SIZE	SCE_GAME_CUSTOM_DATA_DIALOG_ERROR_LACK_OF_LIBSSL_POOL_SIZE
#define plugin_strncpy				sceClibStrncpy
#define plugin_memset				sceClibMemset
#define plugin_strncmp				sceClibStrncmp
#define plugin_strncasecmp			sceClibStrncasecmp
#else
#define PLUGIN_DIALOG_TYPE			COMMON_DIALOG_TYPE_INVITATION
typedef CommonDialogInvitationParam PluginWorkParam;
typedef SceInvitationDialogResult PluginResult;
#define PLUGIN_NAME				"SceInvitationDlgPlugin"
#define PLUGIN_PAGE_PLUGIN			"invitation_dialog_plugin"
#define PLUGIN_IMPL_PLUGIN			"invitation_dialog_impl_plugin"
#define PLUGIN_IMPL_RESOURCE			"invitation_dialog_impl_plugin.rco"
#define PLUGIN_IMPL_MODULE			"libcdlg_invitation_impl.suprx"
#define PLUGIN_ADDRESS_MAX			SCE_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_NUMBER
#define PLUGIN_LEAST_HTTP_POOL_SIZE		SCE_INVITATION_DIALOG_LEAST_HTTP_POOL_SIZE
#define PLUGIN_LEAST_SSL_POOL_SIZE		SCE_INVITATION_DIALOG_LEAST_SSL_POOL_SIZE
#define PLUGIN_ERROR_LACK_OF_LIBHTTP_POOL_SIZE	SCE_INVITATION_DIALOG_ERROR_LACK_OF_LIBHTTP_POOL_SIZE
#define PLUGIN_ERROR_LACK_OF_LIBSSL_POOL_SIZE	SCE_INVITATION_DIALOG_ERROR_LACK_OF_LIBSSL_POOL_SIZE
#define plugin_strncpy				sce_paf_strncpy
#define plugin_memset				sce_paf_memset
#define plugin_strncmp				sce_paf_strncmp
#define plugin_strncasecmp			sce_paf_strncasecmp
#endif

/* the plugin's page and the widget its background colour goes to */
#define INVITATION_DIALOG_PAGE_ID		0x9573096e
#define INVITATION_DIALOG_BG_PLANE_ID		0x80f87c88

/* the draw priority when the caller sets none, and the lists' */
#define INVITATION_DIALOG_DRAW_PRIORITY		120
#define INVITATION_DIALOG_LIST_DRAW_PRIORITY	70

/* the SDK version from which the dialog checks libhttp's and libssl's pools */
#define INVITATION_DIALOG_SDK_VERSION_POOL_CHECK	0x03500000

/* how many addressees the user picks at most when the caller sets no limit */
#define INVITATION_DIALOG_ADDRESS_MAX		PLUGIN_ADDRESS_MAX

/* the impl plugin's dialog: how it ended (the work memory's dialogStatus) */
#define INVITATION_DIALOG_STATUS_SEND		2
#define INVITATION_DIALOG_STATUS_CONTACTS	3
#define INVITATION_DIALOG_STATUS_PARTY		4

/* the contacts list's option when the dialog was left for it */
#define INVITATION_DIALOG_CONTACTS_OPTION_RETURN	4

/* the friends database, under the application data mount */
#define INVITATION_DIALOG_FRIENDS_DB		"/database/friends.db"
#define INVITATION_DIALOG_APPDATA_MOUNT_ID	101

/* the path buffers for the impl plugin's module and resource */
#define INVITATION_DIALOG_PATH_SIZE		128

/* the plugins the thread loads, by the order of their requests */
enum InvitationDialogPluginType {
	INVITATION_DIALOG_PLUGIN_CONTACTS = 0,
	INVITATION_DIALOG_PLUGIN_FRIEND_SELECT = 1,
	INVITATION_DIALOG_PLUGIN_PARTY = 2,
	INVITATION_DIALOG_PLUGIN_IMPL = 3
};

/* the thread's requests: each list or the dialog opens, and is done */
enum InvitationDialogRequest {
	INVITATION_DIALOG_REQUEST_CONTACTS = 0,
	INVITATION_DIALOG_REQUEST_CONTACTS_DONE = 1,
	INVITATION_DIALOG_REQUEST_FRIEND_SELECT = 2,
	INVITATION_DIALOG_REQUEST_FRIEND_SELECT_DONE = 3,
	INVITATION_DIALOG_REQUEST_PARTY = 4,
	INVITATION_DIALOG_REQUEST_PARTY_DONE = 5,
	INVITATION_DIALOG_REQUEST_IMPL = 6,
	INVITATION_DIALOG_REQUEST_IMPL_DONE = 7
};

/* contacts_list_plugin's interface: it opens the list with the parameter below */
struct ContactsListParam {
	SceInt32 reserved[2];
	string title;
	SceInt32 type;
	SceInt32 mode;
	SceNpId npId;
	SceInt32 option;
	void (*selected)(SceInt32 result, SceInt32 status, const SceNpId *npId);
	SceInt32 reserved72[4];
};

struct ContactsListInterface {
	void (*open)(ContactsListParam *param);
};

/* party_memberlist_plugin's interface: the members it starts with, and those picked */
struct PartyMemberListEntry {
	SceInt32 reserved;
	SceNpId npId;
};

struct PartyMemberListSelection {
	SceInt32 type;
	SceUInt32 maxNum;
	SceUInt32 num;
	PartyMemberListEntry *entries;
};

struct PartyMemberListParam {
	SceInt32 type;
	SceInt32 mode;
	SceInt32 sort;
	void (*selected)(SceInt32 result, SceInt32 status, const PartyMemberListSelection *selection);
	SceInt32 reserved[2];
	PartyMemberListSelection *selection;
};

struct PartyMemberListInterface {
	void (*open)(PartyMemberListParam *param);
};

/* friend_select_plugin's multi_data argument: the most it picks, the addressees it starts
   with, and those the user picked */
struct FriendSelectMultiData {
	SceUInt32 maxNum;
	list<SceNpId> preselected;
	list<SceNpId> selected;
};

/*
 * The work memory, locked for one of the plugin's functions: the function's name stays in
 * apiName while the plugin holds it.
 */
class InvitationDialogWork {
public:
	InvitationDialogWork(const char *function)
	{
		SceCommonDialogMain_B9D0E5A8(PLUGIN_DIALOG_TYPE);
		m_work = (PluginWorkParam *)SceCommonDialogMain_DD2CAC30(PLUGIN_DIALOG_TYPE);
		plugin_strncpy(m_work->apiName, function, sizeof(m_work->apiName) - 1);
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
	}

	~InvitationDialogWork()
	{
		plugin_memset(m_work->apiName, 0, sizeof(m_work->apiName));
		SceCommonDialogMain_5739BC42(PLUGIN_DIALOG_TYPE);
	}

	PluginWorkParam *m_work;
};

static inline SceUInt32 GetSdkVersion(void)
{
	InvitationDialogWork work("GetSdkVersion");

	return work.m_work->param.sdkVersion;
}

static inline SceInt32 GetDialogStatus(void)
{
	InvitationDialogWork work("GetDialogStatus");

	return work.m_work->dialogStatus;
}

static inline SceInt32 GetDrawPriority(void)
{
	SceInt32 priority = INVITATION_DIALOG_DRAW_PRIORITY;
	InvitationDialogWork work("GetDrawPriority");

	if (work.m_work->drawPriority != -1)
		priority = work.m_work->drawPriority;
	return priority;
}

static inline SceCommonDialogColor GetDialogBgColor(void)
{
	InvitationDialogWork work("GetDialogBgColor");

	return work.m_work->bgColor;
}

/* the dialog's result: the error and the result, each when the dialog has one */
static inline void SetResult(PluginResult result)
{
	InvitationDialogWork work("SetResult");

	if (result.errorCode != 0)
		work.m_work->errorCode = result.errorCode;
	if (result.result != 0)
		work.m_work->result = result.result;
}

static inline void SetErrorCode(SceInt32 errorCode)
{
	PluginResult result;

	sce_paf_memset(&result, 0, sizeof(result));
	result.errorCode = errorCode;
	SetResult(result);
}

class InvitationDialogThread : public thread::Thread {
public:
	InvitationDialogThread();
	virtual ~InvitationDialogThread();
	virtual void EntryFunction();

	void Stop(bool async);
	int Initialize();
	void Request(int request)
	{
		m_mutex.Lock();
		m_requests.push_back(request);
		m_requestSema.Release();
		m_mutex.Unlock();
	}

	void UnloadPlugin(SceUInt32 type);
	void LoadPlugin(SceUInt32 type, const Plugin::InitParam &init);
	void contactListSelectedCallbackProc(SceInt32 result, SceInt32 status, const SceNpId *npId);
	void friendListSelectedCallbackProc(SceInt32 result);
	void partyMemberListSelectedCallbackProc(SceInt32 result, SceInt32 status,
						 const PartyMemberListSelection *selection);

	thread::Mutex m_mutex;
	thread::Semaphore m_requestSema;
	list<int> m_requests;
	/* released when a plugin is loaded or unloaded */
	thread::Semaphore m_loadSema;
	FriendSelectMultiData m_friendSelect;
	bool m_aborted;
	bool m_contactsOpen;
	bool m_npMessageContactsLoaded;
	bool m_dbRecoveryUtilityLoaded;
	bool m_partyMemberListLoaded;
};

extern "C" void *__dso_handle;
void *__dso_handle;

/* SNC's linker-provided constructor bounds */
extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

static InvitationDialogThread *s_dialog = NULL;
/* the dialog, for the plugins' callbacks */
static InvitationDialogThread *s_instance = NULL;
static Plugin::ModuleInterface s_moduleInterface;

static void invitationDialogInit(SceSize argSize, const void *argp);
static void invitationDialogTerm(void);
static void invitationDialogSetParam(Plugin::InitParam2 *param);
static void invitationDialogPluginInit(Plugin *plugin);
static void invitationDialogPluginStart(Plugin *plugin);
static void invitationDialogPluginStop(Plugin *plugin);
static void invitationDialogPluginExit(Plugin *plugin);
static void invitationDialogLoadFinish(Plugin *plugin);
static void invitationDialogUnloadFinish(const char *name);
static void invitationDialogUpdate(void *userdata);
static void invitationDialogContactsSelected(SceInt32 result, SceInt32 status, const SceNpId *npId);
static void invitationDialogFriendSelectNotify(SceInt32 event, void *userdata);
static void invitationDialogPartySelected(SceInt32 result, SceInt32 status,
					  const PartyMemberListSelection *selection);
static void invitationDialogImplNotify(SceInt32 event, void *userdata);

extern "C" int module_start(SceSize argSize, const void *argp)
{
	invitationDialogInit(argSize, argp);
	return 0;
}

extern "C" int module_stop(SceSize argSize, const void *argp)
{
	invitationDialogTerm();
	return 0;
}

/* the static objects' constructors, and the plugin's functions for SceCommonDialogMain's
   paf::Module */
static __attribute__((noinline)) void invitationDialogInit(SceSize argSize, const void *argp)
{
	void (**init)(void);

	__dso_handle = &__dso_handle;
	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	s_moduleInterface.init_func = invitationDialogPluginInit;
	s_moduleInterface.start_func = invitationDialogPluginStart;
	s_moduleInterface.stop_func = invitationDialogPluginStop;
	s_moduleInterface.exit_func = invitationDialogPluginExit;
	s_moduleInterface.set_param_func = invitationDialogSetParam;
	(*(Module *const *)argp)->SetInterface(0, &s_moduleInterface);
}

static __attribute__((noinline)) void invitationDialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

/* the NP modules the dialog needs, and the NP community, in an application */
static __attribute__((noinline)) void invitationDialogNpInit(void);
static __attribute__((noinline)) void invitationDialogNpTerm(void);

static void invitationDialogPluginInit(Plugin *plugin)
{
	if (s_dialog == NULL)
		s_dialog = new InvitationDialogThread();
	if (Framework::Instance()->GetMode() == Framework::Mode_CommonDialog && s_dialog != NULL)
		invitationDialogNpInit();
}

static void invitationDialogPluginStart(Plugin *plugin)
{
	if (s_dialog != NULL)
		s_dialog->Initialize();
}

static void invitationDialogPluginStop(Plugin *plugin)
{
	if (s_dialog != NULL)
		s_dialog->Stop(false);
	plugin->PageCloseAll();
}

static void invitationDialogPluginExit(Plugin *plugin)
{
	if (Framework::Instance()->GetMode() == Framework::Mode_CommonDialog) {
		npCommunityTerm();
		if (s_dialog != NULL)
			invitationDialogNpTerm();
	}
	if (s_dialog != NULL) {
		delete s_dialog;
		s_dialog = NULL;
	}
}

static void invitationDialogSetParam(Plugin::InitParam2 *param)
{
}

InvitationDialogThread::InvitationDialogThread()
	: thread::Thread(0x10000102, 0x4000, PLUGIN_NAME, NULL),
	  m_mutex(PLUGIN_NAME, 0),
	  m_requestSema(PLUGIN_NAME, 0, 0x7fffffff, 0),
	  m_loadSema(PLUGIN_NAME, 0, 1, 0)
{
	m_aborted = false;
	m_contactsOpen = false;
	m_npMessageContactsLoaded = false;
	m_dbRecoveryUtilityLoaded = false;
	m_partyMemberListLoaded = false;
	s_instance = this;
}

/* stops the thread, and waits for it unless async */
__attribute__((noinline)) void InvitationDialogThread::Stop(bool async)
{
	common::MainThreadCallList::Unregister(invitationDialogUpdate, NULL);
	Cancel();
	m_requestSema.Release();
	if (!async)
		Join();
}

InvitationDialogThread::~InvitationDialogThread()
{
	common::MainThreadCallList::Unregister(invitationDialogUpdate, NULL);
	Cancel();
	m_requestSema.Release();
	Join();
}

/* libhttp's and libssl's pools have to hold what the dialog needs */
static __attribute__((noinline)) int invitationDialogCheckMemoryPool(void)
{
	SceHttpMemoryPoolStats httpStats;
	SceSslMemoryPoolStats sslStats;
	int ret;

	ret = sceHttpGetMemoryPoolStats(&httpStats);
	if (ret >= 0) {
		if (httpStats.poolSize - httpStats.currentInuseSize < PLUGIN_LEAST_HTTP_POOL_SIZE) {
			ret = PLUGIN_ERROR_LACK_OF_LIBHTTP_POOL_SIZE;
		} else {
			ret = sceSslGetMemoryPoolStats(&sslStats);
			if (ret < 0)
				goto error;
			if (sslStats.poolSize - sslStats.currentInuseSize >= PLUGIN_LEAST_SSL_POOL_SIZE)
				return ret;
			ret = PLUGIN_ERROR_LACK_OF_LIBSSL_POOL_SIZE;
		}
	}
error:
	SetErrorCode(ret);
	return ret;
}

/* opens the page with the caller's background colour, checks what the dialog needs, and
   starts the thread with the dialog's first request */
int InvitationDialogThread::Initialize()
{
	Plugin *plugin = Plugin::Find(PLUGIN_PAGE_PLUGIN);
	int request;
	int ret;

	if (plugin != NULL) {
		SceInt32 priority = GetDrawPriority();
		SceCommonDialogColor bgColor = GetDialogBgColor();
		Plugin::PageOpenParam openParam;

		openParam.overwrite_draw_priority = priority;
		ui::Scene *scene = plugin->PageOpen(IDParam("", INVITATION_DIALOG_PAGE_ID), openParam);

		if (scene != NULL) {
			ScePafWidget_0253DC9C(scene, true);
			ui::Widget *plane = scene->FindChild(IDParam("", INVITATION_DIALOG_BG_PLANE_ID), 0);

			if (plane != NULL) {
				math::v4 color(bgColor.r / 255.0f, bgColor.g / 255.0f, bgColor.b / 255.0f,
					       bgColor.a / 255.0f);

				plane->SetColor(color);
			}
		}
	}

	request = INVITATION_DIALOG_REQUEST_IMPL;
	{
		InvitationDialogWork work("Initialize");

		if (work.m_work == NULL || (ret = work.m_work->errorCode) >= 0) {
			if (GetSdkVersion() < INVITATION_DIALOG_SDK_VERSION_POOL_CHECK ||
			    (ret = invitationDialogCheckMemoryPool()) >= 0) {
				ret = 0;
				if (!SceNpManager_A6964218()) {
					ret = SCE_NP_ERROR_NOT_INITIALIZED;
					SetErrorCode(SCE_NP_ERROR_NOT_INITIALIZED);
				}
			}
		}
	}
	if (ret < 0)
		request = INVITATION_DIALOG_REQUEST_IMPL_DONE;
	common::MainThreadCallList::Register(invitationDialogUpdate, NULL);
	Request(request);
	Start();
	return 0;
}

static void invitationDialogNpInit(void)
{
	int ret;

	ret = loadSysmoduleWithArg(SCE_SYSMODULE_INTERNAL_SQLITE_VSH, 0, NULL, 16, NULL);
	if (ret >= 0) {
		ret = loadSysmoduleWithArg(SCE_SYSMODULE_INTERNAL_DBUTIL, 0, NULL, 16, NULL);
		if (ret >= 0)
			ret = npCommunityInitWithAllocator(sce_paf_malloc, sce_paf_free);
	}
	if (ret < 0)
		SetErrorCode(ret);
}

static void invitationDialogNpTerm(void)
{
	unloadSysmoduleWithArg(SCE_SYSMODULE_INTERNAL_SQLITE_VSH, 0, NULL, 0, NULL);
	unloadSysmoduleWithArg(SCE_SYSMODULE_INTERNAL_DBUTIL, 0, NULL, 0, NULL);
}

static const char *invitationDialogPluginName(SceUInt32 type)
{
	switch (type) {
	case INVITATION_DIALOG_PLUGIN_CONTACTS:
		return "contacts_list_plugin";
	case INVITATION_DIALOG_PLUGIN_FRIEND_SELECT:
		return "friend_select_plugin";
	case INVITATION_DIALOG_PLUGIN_PARTY:
		return "party_memberlist_plugin";
	case INVITATION_DIALOG_PLUGIN_IMPL:
		return PLUGIN_IMPL_PLUGIN;
	default:
		return "";
	}
}

/* unloads a plugin, waits for it, and unloads the modules it needed */
void InvitationDialogThread::UnloadPlugin(SceUInt32 type)
{
	Plugin *plugin = Plugin::Find(invitationDialogPluginName(type));

	if (plugin != NULL) {
		Plugin::UnloadAsync(plugin, invitationDialogUnloadFinish, 0);
		m_loadSema.Acquire();
	}
	switch (type) {
	case INVITATION_DIALOG_PLUGIN_CONTACTS: {
		ScePfsRndDriveId mountPoint;

		if (m_contactsOpen) {
			m_contactsOpen = false;
			SceNpMessageContacts_FA2CA5A5();
			SceNpMessageContacts_44B59B98();
			SceNpMessageContacts_C157920F();
			SceNpMessageContacts_21B462D1();
		}
		plugin_memset(&mountPoint, 0, sizeof(mountPoint));
		sceAppMgrAppDataMount(INVITATION_DIALOG_APPDATA_MOUNT_ID, &mountPoint);
		sceAppMgrUmount(mountPoint.drive_id);
		if (m_npMessageContactsLoaded)
			unloadSysmoduleWithArg(SCE_SYSMODULE_INTERNAL_NP_MESSAGE_CONTACTS, 0, NULL, 0, NULL);
		m_npMessageContactsLoaded = false;
		if (m_dbRecoveryUtilityLoaded)
			unloadSysmoduleWithArg(SCE_SYSMODULE_INTERNAL_DB_RECOVERY_UTILITY, 0, NULL, 0, NULL);
		m_dbRecoveryUtilityLoaded = false;
		break;
	}
	case INVITATION_DIALOG_PLUGIN_PARTY:
		if (m_partyMemberListLoaded)
			unloadSysmoduleWithArg(SCE_SYSMODULE_INTERNAL_PARTY_MEMBER_LIST, 0, NULL, 0, NULL);
		m_partyMemberListLoaded = false;
		break;
	}
}

/* loads the modules a plugin needs and the plugin, and waits for it; on an error the request
   to go on without the plugin */
void InvitationDialogThread::LoadPlugin(SceUInt32 type, const Plugin::InitParam &init)
{
	SceUInt32 flags = 0;

	if (Framework::Instance()->GetMode() == Framework::Mode_CommonDialog)
		flags = 16;
	switch (type) {
	case INVITATION_DIALOG_PLUGIN_CONTACTS: {
		if (!m_dbRecoveryUtilityLoaded &&
		    loadSysmoduleWithArg(SCE_SYSMODULE_INTERNAL_DB_RECOVERY_UTILITY, 0, NULL, flags, NULL) < 0) {
			Request(INVITATION_DIALOG_REQUEST_CONTACTS_DONE);
			return;
		}
		m_dbRecoveryUtilityLoaded = true;
		if (!m_npMessageContactsLoaded &&
		    loadSysmoduleWithArg(SCE_SYSMODULE_INTERNAL_NP_MESSAGE_CONTACTS, 0, NULL, flags, NULL) < 0) {
			Request(INVITATION_DIALOG_REQUEST_CONTACTS_DONE);
			return;
		}
		m_npMessageContactsLoaded = true;

		string path;
		ScePfsRndDriveId mountPoint;
		int ret;

		plugin_memset(&mountPoint, 0, sizeof(mountPoint));
		ret = sceAppMgrAppDataMount(INVITATION_DIALOG_APPDATA_MOUNT_ID, &mountPoint);
		if (ret < 0 && ret != APPMGR_ERROR_ALREADY_MOUNTED) {
			Request(INVITATION_DIALOG_REQUEST_CONTACTS_DONE);
			return;
		}
		path = mountPoint.drive_id;
		path += INVITATION_DIALOG_FRIENDS_DB;
		if (!m_contactsOpen) {
			SceNpMessageContacts_017D299D();
			SceNpMessageContacts_3E7B69AE(path.c_str());
			SceNpMessageContacts_E5E42852();
			SceNpMessageContacts_2117B92A();
			m_contactsOpen = true;
		}
		break;
	}
	case INVITATION_DIALOG_PLUGIN_PARTY:
		if (!m_partyMemberListLoaded &&
		    loadSysmoduleWithArg(SCE_SYSMODULE_INTERNAL_PARTY_MEMBER_LIST, 0, NULL, flags, NULL) < 0) {
			Request(INVITATION_DIALOG_REQUEST_PARTY_DONE);
			return;
		}
		m_partyMemberListLoaded = true;
		break;
	case INVITATION_DIALOG_PLUGIN_FRIEND_SELECT:
	case INVITATION_DIALOG_PLUGIN_IMPL:
		break;
	default:
		return;
	}
	Plugin::LoadAsync(init, invitationDialogLoadFinish, 0);
	m_loadSema.Acquire();
}

/* the thread (Run): one request after the other, until it is canceled or the dialog ends */
void InvitationDialogThread::EntryFunction()
{
	char resourcePath[INVITATION_DIALOG_PATH_SIZE];
	char modulePath[INVITATION_DIALOG_PATH_SIZE];

	while (!IsCanceled()) {
		int request;

		m_requestSema.Acquire();
		m_mutex.Lock();
		request = m_requests.front();
		/* Sony's build inlines pop_front here (and keeps it out of line elsewhere in paf) */
		if (m_requests.count != 0 && m_requests.head != NULL) {
			list<int>::_Node *node = m_requests.head->next;

			node->next->prev = node->prev;
			node->prev->next = node->next;
			operator delete(node);
			--m_requests.count;
		}
		m_mutex.Unlock();
		if (m_aborted)
			break;
		switch (request) {
		case INVITATION_DIALOG_REQUEST_CONTACTS: {
			{
				Plugin::InitParam init;

				init.name = "contacts_list_plugin";
				init.resource_file = "vs0:data/external/cdlg/contacts_list_plugin.rco";
				init.caller_name = PLUGIN_IMPL_PLUGIN;
				init.init_func = SceNpMessageContacts_0212231C;
				init.start_func = SceNpMessageContacts_812F9D28;
				init.stop_func = SceNpMessageContacts_92636117;
				init.exit_func = SceNpMessageContacts_88E9872C;
				init.draw_priority = INVITATION_DIALOG_LIST_DRAW_PRIORITY;
				LoadPlugin(INVITATION_DIALOG_PLUGIN_CONTACTS, init);
			}
			Plugin *plugin = Plugin::Find("contacts_list_plugin");

			if (plugin != NULL) {
				ContactsListInterface *iface = (ContactsListInterface *)plugin->GetInterface(1);

				if (iface != NULL) {
					ContactsListParam param;

					param.reserved[0] = 0;
					param.reserved[1] = 0;
					param.type = 0;
					param.mode = 0;
					sce_paf_memset(&param.npId, 0, sizeof(param.npId));
					param.option = 0;
					param.selected = NULL;
					param.reserved72[0] = 0;
					param.reserved72[1] = 0;
					param.reserved72[2] = 0;
					param.reserved72[3] = 0;
					if (GetDialogStatus() == INVITATION_DIALOG_STATUS_CONTACTS)
						param.option = INVITATION_DIALOG_CONTACTS_OPTION_RETURN;
					param.type = 3;
					param.mode = 1;
					param.selected = invitationDialogContactsSelected;
					thread::RMutex::main_thread_mutex.Lock();
					iface->open(&param);
					thread::RMutex::main_thread_mutex.Unlock();
				}
			}
			continue;
		}
		case INVITATION_DIALOG_REQUEST_CONTACTS_DONE:
			UnloadPlugin(INVITATION_DIALOG_PLUGIN_CONTACTS);
			Request(INVITATION_DIALOG_REQUEST_IMPL);
			continue;
		case INVITATION_DIALOG_REQUEST_FRIEND_SELECT: {
			m_friendSelect.preselected.clear();
			m_friendSelect.selected.clear();
			{
				InvitationDialogWork work("Run");
				SceUInt32 maxNum = INVITATION_DIALOG_ADDRESS_MAX;
				SceUInt32 i;

				if (work.m_work->param.dataParam->SendInfo.addressParam.addressInfo.UserSelectEnableAddress.onlineIdsMaxCount != 0)
					maxNum = work.m_work->param.dataParam->SendInfo.addressParam.addressInfo.UserSelectEnableAddress.onlineIdsMaxCount;
				m_friendSelect.maxNum = maxNum;
				for (i = 0; i < work.m_work->addressCount; i++)
					m_friendSelect.preselected.push_front(work.m_work->addresses[i]);
			}
			Plugin::InitParam init;

			init.name = "friend_select_plugin";
			init.resource_file = "vs0:data/external/cdlg/friend_select_plugin.rco";
			init.caller_name = PLUGIN_IMPL_PLUGIN;
			init.module_file = "vs0:sys/external/friend_select.suprx";
			init.module_interface_version = 1;
			init.module_option = 1;
			init.notify_callback = invitationDialogFriendSelectNotify;
			init.draw_priority = INVITATION_DIALOG_LIST_DRAW_PRIORITY;
			init.arguments[string("mode")] = Plugin::Argument(1);
			init.arguments[string("multi_data")] = Plugin::Argument((int32_t)&m_friendSelect);
			init.arguments[string("sort")] = Plugin::Argument(1);
			LoadPlugin(INVITATION_DIALOG_PLUGIN_FRIEND_SELECT, init);
			continue;
		}
		case INVITATION_DIALOG_REQUEST_FRIEND_SELECT_DONE:
			UnloadPlugin(INVITATION_DIALOG_PLUGIN_FRIEND_SELECT);
			Request(INVITATION_DIALOG_REQUEST_IMPL);
			continue;
		case INVITATION_DIALOG_REQUEST_PARTY: {
			{
				Plugin::InitParam init;

				init.name = "party_memberlist_plugin";
				init.resource_file = "vs0:data/external/cdlg/party_member_list_plugin.rco";
				init.caller_name = PLUGIN_IMPL_PLUGIN;
				init.init_func = ScePartyMemberListPlugin_948AF6E3;
				init.start_func = ScePartyMemberListPlugin_8DA476F3;
				init.stop_func = ScePartyMemberListPlugin_CA3A2524;
				init.exit_func = ScePartyMemberListPlugin_852B695D;
				init.draw_priority = INVITATION_DIALOG_LIST_DRAW_PRIORITY;
				LoadPlugin(INVITATION_DIALOG_PLUGIN_PARTY, init);
			}
			Plugin *plugin = Plugin::Find("party_memberlist_plugin");

			if (plugin != NULL) {
				PartyMemberListParam param;
				PartyMemberListInterface *iface;

				param.type = 0;
				param.mode = 0;
				param.sort = 0;
				param.selected = NULL;
				param.reserved[0] = 0;
				param.reserved[1] = 0;
				param.selection = NULL;
				iface = (PartyMemberListInterface *)plugin->GetInterface(1);
				if (iface != NULL) {
					PartyMemberListSelection selection;
					PartyMemberListEntry entries[INVITATION_DIALOG_ADDRESS_MAX];

					selection.type = 0;
					selection.maxNum = 0;
					selection.num = 0;
					selection.entries = NULL;
					plugin_memset(&selection, 0, sizeof(selection));
					selection.type = 1;
					selection.maxNum = INVITATION_DIALOG_ADDRESS_MAX;
					selection.num = 0;
					selection.entries = entries;
					{
						InvitationDialogWork work("Run");
						SceUInt32 maxNum = INVITATION_DIALOG_ADDRESS_MAX;
						SceUInt32 i;

						if (work.m_work->param.dataParam->SendInfo.addressParam.addressInfo.UserSelectEnableAddress.onlineIdsMaxCount != 0)
							maxNum = work.m_work->param.dataParam->SendInfo.addressParam.addressInfo.UserSelectEnableAddress.onlineIdsMaxCount;
						selection.maxNum = maxNum;
						selection.num = work.m_work->addressCount;
						for (i = 0; i < work.m_work->addressCount; i++)
							sce_paf_memcpy(&entries[i].npId, &work.m_work->addresses[i], sizeof(SceNpId));
					}
					param.mode = 1;
					param.sort = 1;
					param.selection = &selection;
					param.type = 1;
					param.selected = invitationDialogPartySelected;
					thread::RMutex::main_thread_mutex.Lock();
					iface->open(&param);
					thread::RMutex::main_thread_mutex.Unlock();
				}
			}
			continue;
		}
		case INVITATION_DIALOG_REQUEST_PARTY_DONE:
			UnloadPlugin(INVITATION_DIALOG_PLUGIN_PARTY);
			Request(INVITATION_DIALOG_REQUEST_IMPL);
			continue;
		case INVITATION_DIALOG_REQUEST_IMPL: {
			if (SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath),
							 PLUGIN_IMPL_RESOURCE, 1) < 0)
				break;
			if (SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath),
							 PLUGIN_IMPL_MODULE, 1) < 0)
				break;
			Plugin::InitParam init;

			init.name = PLUGIN_IMPL_PLUGIN;
			init.resource_file = resourcePath;
			init.caller_name = PLUGIN_IMPL_PLUGIN;
			init.module_file = modulePath;
			init.module_interface_version = 1;
			init.module_option = 1;
			init.notify_callback = invitationDialogImplNotify;
			init.draw_priority = GetDrawPriority() - 10;
			LoadPlugin(INVITATION_DIALOG_PLUGIN_IMPL, init);
			continue;
		}
		case INVITATION_DIALOG_REQUEST_IMPL_DONE:
			UnloadPlugin(INVITATION_DIALOG_PLUGIN_IMPL);
			switch (GetDialogStatus()) {
			case INVITATION_DIALOG_STATUS_SEND:
				Request(INVITATION_DIALOG_REQUEST_FRIEND_SELECT);
				continue;
			case INVITATION_DIALOG_STATUS_CONTACTS:
				Request(INVITATION_DIALOG_REQUEST_CONTACTS);
				continue;
			case INVITATION_DIALOG_STATUS_PARTY:
				Request(INVITATION_DIALOG_REQUEST_PARTY);
				continue;
			}
			break;
		}
		break;
	}
	UnloadPlugin(INVITATION_DIALOG_PLUGIN_CONTACTS);
	UnloadPlugin(INVITATION_DIALOG_PLUGIN_FRIEND_SELECT);
	UnloadPlugin(INVITATION_DIALOG_PLUGIN_PARTY);
	UnloadPlugin(INVITATION_DIALOG_PLUGIN_IMPL);
	SceCommonDialogMain_05EFC635(PLUGIN_DIALOG_TYPE);
}

static void invitationDialogLoadFinish(Plugin *plugin)
{
	s_instance->m_loadSema.Release();
}

/* a plugin is unloaded: once the dialog was aborted, its result is the user's cancel */
static void invitationDialogUnloadFinish(const char *name)
{
	s_instance->m_loadSema.Release();
	if (s_instance->m_aborted) {
		PluginResult result;

		plugin_memset(&result, 0, sizeof(result));
		result.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
		SetResult(result);
	}
}

/* on the main thread: the caller asked to close the dialog */
static void invitationDialogUpdate(void *userdata)
{
	if (SceCommonDialogMain_75F10F83(PLUGIN_DIALOG_TYPE)) {
		InvitationDialogThread *dialog = s_instance;

		if (dialog != NULL) {
			dialog->m_aborted = true;
			common::MainThreadCallList::Unregister(invitationDialogUpdate, NULL);
			dialog->Cancel();
			dialog->m_requestSema.Release();
		}
	}
}

/* the contact the user picked becomes an addressee, unless it is one */
void InvitationDialogThread::contactListSelectedCallbackProc(SceInt32 result, SceInt32 status, const SceNpId *npId)
{
	if (result == 0 && status == 0) {
		SceNpId picked;

		sce_paf_memcpy(&picked, npId, sizeof(picked));
		{
			InvitationDialogWork work("contactListSelectedCallbackProc");
			SceInt32 i = 0;

			while (plugin_strncmp(work.m_work->addresses[i].handle.data, picked.handle.data,
					       SCE_NP_ONLINEID_MAX_LENGTH) != 0) {
				if (work.m_work->addresses[i].handle.data[0] == '\0') {
					sce_paf_memcpy(&work.m_work->addresses[i], &picked, sizeof(SceNpId));
					sce_paf_memcpy(work.m_work->addresses[i].opt, "x", 1);
					break;
				}
				if (++i >= INVITATION_DIALOG_ADDRESS_MAX)
					break;
			}
		}
	}
	Request(INVITATION_DIALOG_REQUEST_CONTACTS_DONE);
}

static void invitationDialogContactsSelected(SceInt32 result, SceInt32 status, const SceNpId *npId)
{
	s_instance->contactListSelectedCallbackProc(result, status, npId);
}

/* the friends the user picked become addressees, those that are not yet */
void InvitationDialogThread::friendListSelectedCallbackProc(SceInt32 result)
{
	if (result == 0) {
		InvitationDialogWork work("friendListSelectedCallbackProc");
		list<SceNpId>::iterator it;

		for (it = m_friendSelect.selected.begin(); it != m_friendSelect.selected.end(); ++it) {
			SceInt32 i;

			for (i = 0; i < INVITATION_DIALOG_ADDRESS_MAX; i++) {
				if (plugin_strncmp(work.m_work->addresses[i].handle.data, it->handle.data,
						    SCE_NP_ONLINEID_MAX_LENGTH) == 0)
					break;
				if (work.m_work->addresses[i].handle.data[0] == '\0') {
					sce_paf_memcpy(&work.m_work->addresses[i], &*it, sizeof(SceNpId));
					sce_paf_memcpy(work.m_work->addresses[i].opt, "x", 1);
					break;
				}
			}
		}
	}
	Request(INVITATION_DIALOG_REQUEST_FRIEND_SELECT_DONE);
}

static void invitationDialogFriendSelectNotify(SceInt32 event, void *userdata)
{
	s_instance->friendListSelectedCallbackProc(event);
}

/* the party members the user picked become addressees, those that are not yet */
void InvitationDialogThread::partyMemberListSelectedCallbackProc(SceInt32 result, SceInt32 status,
							   const PartyMemberListSelection *selection)
{
	if (result == 0 && status == 0 && selection != NULL) {
		InvitationDialogWork work("partyMemberListSelectedCallbackProc");
		SceUInt32 n;

		for (n = 0; n < selection->num; n++) {
			SceInt32 i = 0;

			while (plugin_strncasecmp(work.m_work->addresses[i].handle.data,
						   selection->entries[n].npId.handle.data,
						   SCE_NP_ONLINEID_MAX_LENGTH) != 0) {
				if (work.m_work->addresses[i].handle.data[0] == '\0') {
					sce_paf_memcpy(&work.m_work->addresses[i], &selection->entries[n].npId,
						       sizeof(SceNpId));
					sce_paf_memcpy(work.m_work->addresses[i].opt, "x", 1);
					break;
				}
				if (++i >= INVITATION_DIALOG_ADDRESS_MAX)
					break;
			}
		}
	}
	Request(INVITATION_DIALOG_REQUEST_PARTY_DONE);
}

static void invitationDialogPartySelected(SceInt32 result, SceInt32 status,
					  const PartyMemberListSelection *selection)
{
	s_instance->partyMemberListSelectedCallbackProc(result, status, selection);
}

/* the impl plugin's dialog ended */
static void invitationDialogImplNotify(SceInt32 event, void *userdata)
{
	s_instance->Request(INVITATION_DIALOG_REQUEST_IMPL_DONE);
}
