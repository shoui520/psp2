/*
 * SceCompanionDialogPlugin (vs0:sys/external/libcdlg_companion.suprx).
 * PS4 Link / Second Screen / Remote Play Companion Common Dialog plugin.
 */
#include <stdint.h>
#include <scetypes.h>
#include <plugin.h>
#include <module/module.h>
#include <std/map>
#include <std/vector>
#include <versatileparam/idparam.h>
#include <common/main_thread_call_list.h>
#include <widget/core/widget.h>
#include <widget/core/event.h>
#include <widget/w_button.h>
#include "src/furl_stack.h"
#include "src/remote_play/ui/remoteplay_ui_devreg.h"

extern "C" {
extern void (*__start__Zinit_array[])(void);
extern void (*__stop__Zinit_array[])(void);
void *__dso_handle = &__dso_handle;
void __cxa_finalize(void *dso);
SceInt32 SceCommonDialogMain_B9D0E5A8(SceInt32 slot);
void *SceCommonDialogMain_DD2CAC30(SceInt32 slot);
SceInt32 SceCommonDialogMain_5739BC42(SceInt32 slot);
SceInt32 SceCommonDialogMain_05EFC635(SceInt32 slot);
SceInt32 SceCommonDialogMain_75F10F83(SceInt32 slot);
void *sce_paf_memcpy(void *dst, const void *src, SceSize size);
void *sce_paf_memset(void *dst, int c, SceSize size);
int sce_paf_strncmp(const char *lhs, const char *rhs, SceSize n);
char *sce_paf_strncpy(char *dst, const char *src, SceSize n);
SceSize sce_paf_strlen(const char *s);
long long sce_paf_atoll(const char *nptr);
SceInt32 sceRegMgrSystemParamGetBin(SceInt32 key, void *buf, SceSize size);
SceInt32 sceRegMgrSystemParamSetBin(SceInt32 key, const void *buf, SceSize size);
SceInt32 sceRegMgrSystemParamSetInt(SceInt32 key, SceInt32 val);
SceInt32 sceCompanionUtilSearchToOrbis(void *param);
SceInt32 sceCompanionUtilStop(void);
SceInt32 sceCompanionUtilSetCallbackHandlerInternal(void (*cb)(SceInt32, void *), void *arg);
SceInt32 SceCompanionUtil_7AB433EC(void *initParam);
SceInt32 SceCompanionUtil_53445DBF(void *targetInfo);
SceInt32 SceCompanionUtil_2B0EF5F8(void *orbisInfo, void *dialogParam);
SceInt32 SceCompanionUtil_C93BB8F0(void *orbisInfo);
}

static const SceInt32 kDialogSlotCompanion = 24;
static const SceInt32 kRegKeyOrbisList = 9757472;
static const SceInt32 kRegKeyOrbisCount = 5209696;
static const SceInt32 kErrCommonDialogLock = (SceInt32)0x80920063;
static const SceInt32 kErrCommonDialogUnlock = (SceInt32)0x80920064;

static const SceUInt32 kPageHashConnect = 0x0F247E94U;
static const SceUInt32 kPageHashSelect = 0xDEDAE656U;
static const SceUInt32 kPageHashDone = 0xE88F65C4U;
static const SceUInt32 kPageHashError = 0xF6AB0D4BU;

struct OrbisRegistryEntry {
	uint8_t macBytes[6];
	SceUInt32 registKeyBe;
};

struct OrbisRegistryTable {
	SceInt32 version;
	SceInt32 count;
	OrbisRegistryEntry entries[10];
	uint8_t reserved[20];
};

struct CompanionDialogResult {
	SceInt16 statusCode;
	uint8_t payload[42];
};

struct CompanionOrbisConnectInfo {
	uint8_t header[6];
	char macHex[18];
	SceUInt32 registKey;
	uint8_t tail[8];
};

struct CompanionSharedWork {
	uint8_t header[80];
	uint8_t connectParam[44];
	SceUInt16 modeFlag;
	uint8_t middle[34];
	CompanionDialogResult result;
	uint8_t pad[4];
	char lockTag[64];
};

static OrbisRegistryTable g_orbisRegistry;
static uint8_t g_orbisRegistryLoaded = 0;

static int DecodeHexNibble(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	return -1;
}

static void ParseHexPadded(uint8_t *dst, const char *src, SceInt32 hexLen)
{
	char padded[16];
	sce_paf_strncpy(padded, "000000000000", 13);
	sce_paf_memset(dst, 0, (SceSize)(hexLen / 2));
	if ((hexLen & 1) != 0 || hexLen > 12)
		return;

	SceInt32 actualLen = hexLen;
	for (SceInt32 idx = 0; idx < hexLen; ++idx) {
		if (src[idx] == '\0') {
			actualLen = idx;
			break;
		}
	}
	sce_paf_memcpy(padded + (hexLen - actualLen), src, (SceSize)actualLen);
	for (SceInt32 idx = 0; idx < hexLen; idx += 2) {
		int hi = DecodeHexNibble(padded[idx]);
		int lo = DecodeHexNibble(padded[idx + 1]);
		if (hi < 0 || lo < 0)
			break;
		dst[idx >> 1] = (uint8_t)((hi << 4) | lo);
	}
}

static char EncodeHexNibble(uint8_t val)
{
	return (val <= 9) ? (char)('0' + val) : (char)('a' + (val - 10));
}

static SceInt32 LoadOrbisRegistry()
{
	SceInt32 ret = sceRegMgrSystemParamGetBin(kRegKeyOrbisList, &g_orbisRegistry, sizeof(g_orbisRegistry));
	if (ret < 0 || g_orbisRegistry.version > 1) {
		sce_paf_memset(&g_orbisRegistry, 0, sizeof(g_orbisRegistry));
		g_orbisRegistry.version = 1;
		return ret;
	}
	if (g_orbisRegistry.version == 0) {
		sce_paf_memset(&g_orbisRegistry, 0, sizeof(g_orbisRegistry));
		g_orbisRegistry.version = 1;
	} else {
		g_orbisRegistry.version = 1;
	}
	g_orbisRegistryLoaded = 1;
	return ret;
}

extern "C" SceInt32 CompanionRegistrySaveOrbis(const char *ps4Mac, const char *ps4RegistKey)
{
	SceInt32 ret = LoadOrbisRegistry();
	if (ret < 0)
		return ret;

	uint8_t macBytes[6];
	ParseHexPadded(macBytes, ps4Mac, 12);

	SceInt32 matchIdx = -1;
	for (SceInt32 idx = 0; idx < g_orbisRegistry.count; ++idx) {
		bool same = true;
		for (int b = 0; b < 6; ++b) {
			if (g_orbisRegistry.entries[idx].macBytes[b] != macBytes[b]) {
				same = false;
				break;
			}
		}
		if (same) {
			matchIdx = idx;
			break;
		}
	}

	if (matchIdx < 0) {
		if (g_orbisRegistry.count < 10)
			++g_orbisRegistry.count;
		for (SceInt32 idx = 9; idx >= 1; --idx)
			g_orbisRegistry.entries[idx] = g_orbisRegistry.entries[idx - 1];
		sce_paf_memcpy(g_orbisRegistry.entries[0].macBytes, macBytes, 6);
	} else if (matchIdx > 0) {
		OrbisRegistryEntry tmp = g_orbisRegistry.entries[0];
		g_orbisRegistry.entries[0] = g_orbisRegistry.entries[matchIdx];
		g_orbisRegistry.entries[matchIdx] = tmp;
	}

	uint8_t keyBytes[4];
	ParseHexPadded(keyBytes, ps4RegistKey, 8);
	g_orbisRegistry.entries[0].registKeyBe =
		((SceUInt32)keyBytes[0] << 24) |
		((SceUInt32)keyBytes[1] << 16) |
		((SceUInt32)keyBytes[2] << 8) |
		(SceUInt32)keyBytes[3];

	ret = sceRegMgrSystemParamSetBin(kRegKeyOrbisList, &g_orbisRegistry, sizeof(g_orbisRegistry));
	if (ret >= 0)
		ret = sceRegMgrSystemParamSetInt(kRegKeyOrbisCount, g_orbisRegistry.count);
	return ret;
}

static SceInt32 GetOrbisRegistryCount()
{
	return (g_orbisRegistryLoaded != 0) ? g_orbisRegistry.count : -1;
}

static SceInt32 GetOrbisRegistryEntry(SceInt32 index, char *outMacHex, char *outKeyHex)
{
	if (g_orbisRegistryLoaded == 0 || index < 0 || index >= g_orbisRegistry.count)
		return -1;
	const OrbisRegistryEntry &entry = g_orbisRegistry.entries[index];
	for (int idx = 0; idx < 6; ++idx) {
		outMacHex[idx * 2] = EncodeHexNibble((uint8_t)(entry.macBytes[idx] >> 4));
		outMacHex[idx * 2 + 1] = EncodeHexNibble((uint8_t)(entry.macBytes[idx] & 0xF));
	}
	uint8_t keyBytes[4];
	keyBytes[0] = (uint8_t)((entry.registKeyBe >> 24) & 0xFF);
	keyBytes[1] = (uint8_t)((entry.registKeyBe >> 16) & 0xFF);
	keyBytes[2] = (uint8_t)((entry.registKeyBe >> 8) & 0xFF);
	keyBytes[3] = (uint8_t)(entry.registKeyBe & 0xFF);
	for (int idx = 0; idx < 4; ++idx) {
		outKeyHex[idx * 2] = EncodeHexNibble((uint8_t)(keyBytes[idx] >> 4));
		outKeyHex[idx * 2 + 1] = EncodeHexNibble((uint8_t)(keyBytes[idx] & 0xF));
	}
	return 0;
}

static SceInt32 LookupOrbisRegistKey(char *outKeyHex, const char *macHex)
{
	SceInt32 count = GetOrbisRegistryCount();
	if (count <= 0)
		return -1;

	char normalized[12];
	sce_paf_memcpy(normalized, macHex, 12);
	for (int idx = 0; idx < 12; ++idx) {
		if (normalized[idx] >= 'A' && normalized[idx] <= 'F')
			normalized[idx] = (char)(normalized[idx] + 32);
	}

	for (SceInt32 idx = 0; idx < count; ++idx) {
		char entryMac[12];
		char entryKey[8];
		GetOrbisRegistryEntry(idx, entryMac, entryKey);
		if (sce_paf_strncmp(entryMac, normalized, 12) == 0) {
			sce_paf_memset(outKeyHex, 0, 10);
			sce_paf_memcpy(outKeyHex, entryKey, 8);
			return 0;
		}
	}
	return -1;
}

class CompanionPageBase {
public:
	CompanionPageBase(paf::Plugin *plugin, const paf::IDParam &pageId)
		: m_plugin(plugin)
		, m_pageId(pageId)
		, m_rootWidget(0)
	{
	}
	virtual ~CompanionPageBase() {}
	virtual void OnCreate() {}
	virtual void OnInit(paf::ui::Widget *widget) { m_rootWidget = widget; }
	virtual void OnUpdate() {}
	virtual void OnClose() {}

protected:
	paf::Plugin *m_plugin;
	paf::IDParam m_pageId;
	paf::ui::Widget *m_rootWidget;
};

class CompanionConnectPage : public CompanionPageBase {
public:
	CompanionConnectPage(paf::Plugin *plugin, const paf::IDParam &pageId)
		: CompanionPageBase(plugin, pageId)
		, m_furlStack(true)
	{
	}
	virtual ~CompanionConnectPage() {}
	virtual void OnInit(paf::ui::Widget *widget)
	{
		CompanionPageBase::OnInit(widget);
		m_furlStack.Push(widget);
		RemotePlayUiStart(m_plugin);
	}
	virtual void OnClose()
	{
		RemotePlayUiStop(m_plugin);
		m_furlStack.Clear();
	}

private:
	FurlStack m_furlStack;
};

class CompanionSelectPage : public CompanionPageBase {
public:
	CompanionSelectPage(paf::Plugin *plugin, const paf::IDParam &pageId)
		: CompanionPageBase(plugin, pageId)
	{
	}
	virtual ~CompanionSelectPage() {}
	virtual void OnInit(paf::ui::Widget *widget)
	{
		CompanionPageBase::OnInit(widget);
		RemotePlayUiInitPinPage(widget);
	}
};

class CompanionDonePage : public CompanionPageBase {
public:
	CompanionDonePage(paf::Plugin *plugin, const paf::IDParam &pageId)
		: CompanionPageBase(plugin, pageId)
	{
	}
	virtual ~CompanionDonePage() {}
	virtual void OnInit(paf::ui::Widget *widget)
	{
		CompanionPageBase::OnInit(widget);
		RemotePlayUiInitWaitPage(widget);
	}
};

class CompanionErrorPage : public CompanionPageBase {
public:
	CompanionErrorPage(paf::Plugin *plugin, const paf::IDParam &pageId)
		: CompanionPageBase(plugin, pageId)
	{
	}
	virtual ~CompanionErrorPage() {}
};

class CompanionPageManager {
public:
	CompanionPageManager()
		: m_reserved(0)
		, m_connectPage(0)
		, m_selectPage(0)
		, m_donePage(0)
		, m_errorPage(0)
	{
	}
	~CompanionPageManager()
	{
		delete m_connectPage;
		delete m_selectPage;
		delete m_donePage;
		delete m_errorPage;
	}

	SceInt32 CreatePages(paf::Plugin *plugin)
	{
		if (m_connectPage == 0)
			m_connectPage = new CompanionConnectPage(plugin, paf::IDParam(kPageHashConnect));
		if (m_selectPage == 0)
			m_selectPage = new CompanionSelectPage(plugin, paf::IDParam(kPageHashSelect));
		if (m_donePage == 0)
			m_donePage = new CompanionDonePage(plugin, paf::IDParam(kPageHashDone));
		if (m_errorPage == 0)
			m_errorPage = new CompanionErrorPage(plugin, paf::IDParam(kPageHashError));
		return 0;
	}

	void RegisterPagePlugin(paf::Plugin *plugin)
	{
		if (plugin == 0)
			return;
		m_pagePlugins[kPageHashConnect] = plugin;
		m_pagePlugins[kPageHashSelect] = plugin;
		m_pagePlugins[kPageHashDone] = plugin;
		m_pagePlugins[kPageHashError] = plugin;
	}

	void OnPageInit(paf::ui::Widget *widget)
	{
		if (widget == 0)
			return;
		SceUInt32 hash = widget->m_name.GetIDHash();
		if (hash == kPageHashError && m_errorPage != 0)
			m_errorPage->OnInit(widget);
		else if (hash == kPageHashDone && m_donePage != 0)
			m_donePage->OnInit(widget);
		else if (hash == kPageHashSelect && m_selectPage != 0)
			m_selectPage->OnInit(widget);
		else if (hash == kPageHashConnect && m_connectPage != 0)
			m_connectPage->OnInit(widget);
	}

private:
	SceUInt32 m_reserved;
	CompanionConnectPage *m_connectPage;
	CompanionSelectPage *m_selectPage;
	CompanionDonePage *m_donePage;
	CompanionErrorPage *m_errorPage;
	paf::map<SceUInt32, paf::Plugin *> m_pagePlugins;
};

class CompanionSceneManager {
public:
	CompanionSceneManager()
		: m_reserved(0)
	{
	}
	~CompanionSceneManager() {}

	SceInt32 OpenPage(paf::Plugin *plugin, const paf::IDParam &pageId,
		const paf::Plugin::PageOpenParam &param)
	{
		plugin->PageOpen(pageId, param);
		return 0;
	}

	void UpdateAll()
	{
		for (paf::map<SceUInt32, CompanionPageBase *>::iterator it = m_activePages.begin();
			it != m_activePages.end(); ++it) {
			if (it->second != 0)
				it->second->OnUpdate();
		}
		for (SceSize idx = 0; idx < m_pendingRemove.size(); ++idx)
			m_activePages.erase(m_pendingRemove[idx]);
		m_pendingRemove.clear();
	}

private:
	SceUInt32 m_reserved;
	paf::map<SceUInt32, CompanionPageBase *> m_activePages;
	paf::vector<SceUInt32> m_pendingRemove;
};

class CompanionDialogManager {
public:
	CompanionDialogManager()
		: m_state(0)
		, m_isRemotePlayMode(false)
	{
		sce_paf_memset(m_targetInfo, 0, sizeof(m_targetInfo));
	}
	virtual ~CompanionDialogManager();

	SceInt32 InitUI(paf::Plugin *plugin);
	SceInt32 SetResult(const CompanionDialogResult *result);
	SceInt32 Start(paf::Plugin *plugin);
	SceInt32 Stop(paf::Plugin *plugin);
	SceInt32 ConnectToOrbis(CompanionOrbisConnectInfo *info);
	void Update();

	bool IsRemotePlayMode() const { return m_isRemotePlayMode; }
	SceUInt32 GetTargetIpAddr() const
	{
		SceUInt32 info[7];
		sce_paf_memcpy(info, m_targetInfo, sizeof(info));
		SceCompanionUtil_53445DBF(info);
		return info[0];
	}

private:
	SceInt32 m_state;
	bool m_isRemotePlayMode;
	uint8_t m_pad[3];
	SceUInt32 m_targetInfo[7];
};

static CompanionDialogManager *g_dialogManager = 0;
static CompanionPageManager *g_pageManager = 0;
static CompanionSceneManager *g_sceneManager = 0;

CompanionDialogManager::~CompanionDialogManager()
{
	delete g_pageManager;
	g_pageManager = 0;
	delete g_sceneManager;
	g_sceneManager = 0;
}

SceInt32 CompanionDialogManager::InitUI(paf::Plugin *plugin)
{
	m_state = 1;
	if (g_pageManager == 0)
		g_pageManager = new CompanionPageManager();
	if (g_sceneManager == 0)
		g_sceneManager = new CompanionSceneManager();

	if (SceCommonDialogMain_B9D0E5A8(kDialogSlotCompanion) == 0) {
		CompanionSharedWork *work = static_cast<CompanionSharedWork *>(
			SceCommonDialogMain_DD2CAC30(kDialogSlotCompanion));
		if (work != 0) {
			sce_paf_strncpy(work->lockTag, "InitiazeleUI", sizeof(work->lockTag));
			work->lockTag[sizeof(work->lockTag) - 1] = '\0';
			m_isRemotePlayMode = (work->modeFlag == 1);
			if (sce_paf_strncmp(work->lockTag, "InitiazeleUI", sizeof(work->lockTag)) != 0)
				return kErrCommonDialogUnlock;
			sce_paf_memset(work->lockTag, 0, sizeof(work->lockTag));
			if (SceCommonDialogMain_5739BC42(kDialogSlotCompanion) != 0)
				return kErrCommonDialogUnlock;
		} else {
			SceCommonDialogMain_5739BC42(kDialogSlotCompanion);
		}
	}

	if (g_pageManager != 0)
		g_pageManager->RegisterPagePlugin(plugin);
	return 0;
}

SceInt32 CompanionDialogManager::SetResult(const CompanionDialogResult *result)
{
	if (SceCommonDialogMain_B9D0E5A8(kDialogSlotCompanion) != 0)
		return kErrCommonDialogLock;
	CompanionSharedWork *work = static_cast<CompanionSharedWork *>(
		SceCommonDialogMain_DD2CAC30(kDialogSlotCompanion));
	if (work == 0) {
		SceCommonDialogMain_5739BC42(kDialogSlotCompanion);
		return kErrCommonDialogLock;
	}
	sce_paf_strncpy(work->lockTag, "SetResult", sizeof(work->lockTag));
	work->lockTag[sizeof(work->lockTag) - 1] = '\0';
	sce_paf_memcpy(&work->result, result, sizeof(work->result));
	if (sce_paf_strncmp(work->lockTag, "SetResult", sizeof(work->lockTag)) != 0)
		return kErrCommonDialogUnlock;
	sce_paf_memset(work->lockTag, 0, sizeof(work->lockTag));
	if (SceCommonDialogMain_5739BC42(kDialogSlotCompanion) != 0)
		return kErrCommonDialogUnlock;
	return 0;
}

SceInt32 CompanionDialogManager::Start(paf::Plugin *plugin)
{
	CompanionDialogResult result;
	sce_paf_memset(&result, 0, sizeof(result));

	SceUInt32 initParam = 0;
	SceInt32 ret = SceCompanionUtil_7AB433EC(&initParam);
	if (ret >= 0) {
		result.statusCode = (SceInt16)0x80FF;
		if (g_pageManager != 0)
			g_pageManager->CreatePages(plugin);
		paf::Plugin::PageOpenParam openParam;
		if (g_sceneManager != 0)
			g_sceneManager->OpenPage(plugin, paf::IDParam(kPageHashConnect), openParam);
		m_state = 2;
	} else {
		result.statusCode = (SceInt16)0x8004;
		m_state = 3;
	}
	SetResult(&result);
	return ret;
}

SceInt32 CompanionDialogManager::Stop(paf::Plugin *plugin)
{
	m_state = 4;
	if (plugin != 0)
		plugin->PageCloseAll();
	return 0;
}

SceInt32 CompanionDialogManager::ConnectToOrbis(CompanionOrbisConnectInfo *info)
{
	if (SceCommonDialogMain_B9D0E5A8(kDialogSlotCompanion) != 0)
		return kErrCommonDialogLock;
	CompanionSharedWork *work = static_cast<CompanionSharedWork *>(
		SceCommonDialogMain_DD2CAC30(kDialogSlotCompanion));
	if (work == 0) {
		SceCommonDialogMain_5739BC42(kDialogSlotCompanion);
		return kErrCommonDialogLock;
	}
	sce_paf_strncpy(work->lockTag, "connectToOrbis", sizeof(work->lockTag));
	work->lockTag[sizeof(work->lockTag) - 1] = '\0';

	char keyHex[12];
	LookupOrbisRegistKey(keyHex, info->macHex);
	char hexPrefixed[12];
	hexPrefixed[0] = '0';
	hexPrefixed[1] = 'x';
	sce_paf_memcpy(hexPrefixed + 2, keyHex, 10);
	info->registKey = (SceUInt32)sce_paf_atoll(hexPrefixed);

	if (m_isRemotePlayMode)
		SceCompanionUtil_C93BB8F0(info);
	else
		SceCompanionUtil_2B0EF5F8(info, work->connectParam);

	if (sce_paf_strncmp(work->lockTag, "connectToOrbis", sizeof(work->lockTag)) != 0)
		return kErrCommonDialogUnlock;
	sce_paf_memset(work->lockTag, 0, sizeof(work->lockTag));
	if (SceCommonDialogMain_5739BC42(kDialogSlotCompanion) != 0)
		return kErrCommonDialogUnlock;
	return 0;
}

void CompanionDialogManager::Update()
{
	switch (m_state) {
	case 3:
		SceCommonDialogMain_05EFC635(kDialogSlotCompanion);
		m_state = 4;
		if (g_sceneManager != 0)
			g_sceneManager->UpdateAll();
		break;
	case 4:
		break;
	default:
		if (g_sceneManager != 0)
			g_sceneManager->UpdateAll();
		break;
	}
}

extern "C" bool CompanionIsRemotePlayMode()
{
	return (g_dialogManager != 0) ? g_dialogManager->IsRemotePlayMode() : false;
}

extern "C" SceUInt32 CompanionGetTargetIpAddr()
{
	return (g_dialogManager != 0) ? g_dialogManager->GetTargetIpAddr() : 0;
}

static void CompanionMainLoopCallback(void *userData)
{
	(void)userData;
	if (g_dialogManager != 0)
		g_dialogManager->Update();
	if (SceCommonDialogMain_75F10F83(kDialogSlotCompanion) != 0) {
		if (g_dialogManager != 0) {
			CompanionDialogResult result;
			sce_paf_memset(&result, 0, sizeof(result));
			result.statusCode = (SceInt16)0x8002;
			g_dialogManager->SetResult(&result);
		}
		SceCommonDialogMain_05EFC635(kDialogSlotCompanion);
	}
}

struct CompanionPluginInterface {
	void (*OnBoot)();
	void (*OnLoad)(paf::Plugin *plugin);
	void (*OnStart)();
	void (*OnStop)();
	void (*OnUnload)();
};

static paf::Plugin *g_companionPlugin = 0;
static CompanionPluginInterface s_companionPluginIf;

static void CompanionPluginOnBoot()
{
}

static void CompanionPluginOnLoad(paf::Plugin *plugin)
{
	g_companionPlugin = plugin;
	LoadOrbisRegistry();
	if (g_dialogManager == 0)
		g_dialogManager = new CompanionDialogManager();
	if (g_dialogManager != 0)
		g_dialogManager->InitUI(plugin);
}

static void CompanionPluginOnStart()
{
	if (g_dialogManager != 0)
		g_dialogManager->Start(g_companionPlugin);
	paf::common::MainThreadCallList::Register(CompanionMainLoopCallback, g_companionPlugin);
}

static void CompanionPluginOnStop()
{
	paf::common::MainThreadCallList::Unregister(CompanionMainLoopCallback, g_companionPlugin);
	if (g_dialogManager != 0)
		g_dialogManager->Stop(g_companionPlugin);
}

static void CompanionPluginOnUnload()
{
	delete g_dialogManager;
	g_dialogManager = 0;
	g_companionPlugin = 0;
}

extern "C" SceInt32 module_start(SceSize arglen, const void *argp)
{
	(void)arglen;
	for (void (**fn)(void) = __start__Zinit_array; fn < __stop__Zinit_array; ++fn) {
		if (*fn != 0)
			(*fn)();
	}
	s_companionPluginIf.OnBoot = CompanionPluginOnBoot;
	s_companionPluginIf.OnLoad = CompanionPluginOnLoad;
	s_companionPluginIf.OnStart = CompanionPluginOnStart;
	s_companionPluginIf.OnStop = CompanionPluginOnStop;
	s_companionPluginIf.OnUnload = CompanionPluginOnUnload;
	if (argp != 0) {
		paf::Module *mod = *(paf::Module *const *)argp;
		if (mod != 0)
			mod->SetInterface(1, &s_companionPluginIf);
	}
	return 0;
}

extern "C" SceInt32 module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	__cxa_finalize(__dso_handle);
	return 0;
}
