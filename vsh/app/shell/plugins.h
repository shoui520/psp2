/*
 * plugins.h: the plugins linked into SceShell, as the shell's files load and call them. Names
 * ours: the 3.6x shell names none of them.
 */
#ifndef _SHELL_PLUGINS_H
#define _SHELL_PLUGINS_H

#include <plugin.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <rtc/rtc.h>

struct LsdbThemeInfo;

/*
 * PLACEHOLDER: the function table a shell plugin gives through GetInterface(1), indexed until
 * each plugin's table is written out from its own file.
 */
struct ShellPluginInterface {
	void (*func[1])(void);
};

/* topmenu_plugin's functions */
enum {
	TOPMENU_PLUGIN_FUNC_GET_CUR_PAGE	= 9,	/* PLACEHOLDER names: the key of the page in view, */
	TOPMENU_PLUGIN_FUNC_GET_PAGE_INDEX	= 10,	/* a page's place by its key (-1: none), */
	TOPMENU_PLUGIN_FUNC_GET_PAGE_KEY	= 11,	/* and the key of the page at a place */
	TOPMENU_PLUGIN_FUNC_GAME_HOLD		= 31,	/* PLACEHOLDER names: the game plugin at work, */
	TOPMENU_PLUGIN_FUNC_GAME_RELEASE	= 32,	/* and done; what topmenu does is not read yet */
	TOPMENU_PLUGIN_FUNC_HOLD_BEGIN		= 33,	/* PLACEHOLDER names: taken inside the game hold by */
	TOPMENU_PLUGIN_FUNC_HOLD_END		= 34,	/* the icon deletion, and given back */
	TOPMENU_PLUGIN_FUNC_BUSY_BEGIN		= 35,	/* the first of the game plugin's busy calls, */
	TOPMENU_PLUGIN_FUNC_BUSY_END		= 36,	/* and the last */
	TOPMENU_PLUGIN_FUNC_CREATE_PLANE	= 51,	/* PLACEHOLDER name: a plane made in its layer (0: the first) */
	TOPMENU_PLUGIN_FUNC_GET_RUNNING		= 59,	/* PLACEHOLDER name: the titles running, into a vector */
	TOPMENU_PLUGIN_FUNC_SET_APPLAUNCHER	= 60,	/* PLACEHOLDER names: applauncher's table given */
	TOPMENU_PLUGIN_FUNC_APPLAUNCHER_INIT	= 68,	/* (NULL: taken back), and its init done */
	TOPMENU_PLUGIN_FUNC_UP_STATUS		= 66,	/* PLACEHOLDER name: an application's #_up_status changed */
	TOPMENU_PLUGIN_FUNC_GET_APP_INFO	= 69,	/* PLACEHOLDER name: an application's LsdbAppInfoPtr, by title ID */
	TOPMENU_PLUGIN_FUNC_BOOT_DONE		= 88,
};

/* livearea_plugin's functions (indexes in its PLACEHOLDER table) */
enum {
	LIVEAREA_PLUGIN_FUNC_LICENSE_ADDED	= 8,	/* PLACEHOLDER name: a PocketStation license written */
	LIVEAREA_PLUGIN_FUNC_REVERT		= 10,
};

/* power_manage_plugin's functions */
enum {
	POWER_MANAGE_PLUGIN_FUNC_ADD_EXCLUSIVE_CALLBACK = 0,	/* PLACEHOLDER: the exclusive function manager's power callback */
	POWER_MANAGE_PLUGIN_FUNC_ADD_CALLBACK	= 2,	/* PLACEHOLDER names: a function called on */
	POWER_MANAGE_PLUGIN_FUNC_REMOVE_CALLBACK = 3,	/* the power events, and no longer */
	POWER_MANAGE_PLUGIN_FUNC_GAME_HOLD	= 5,	/* PLACEHOLDER names: the game plugin at work, */
	POWER_MANAGE_PLUGIN_FUNC_GAME_RELEASE	= 6,	/* and done */
	POWER_MANAGE_PLUGIN_FUNC_MENU		= 14,	/* PLACEHOLDER name: the power menu shown */
	POWER_MANAGE_PLUGIN_FUNC_POWER_EVENT	= 15,
	POWER_MANAGE_PLUGIN_FUNC_BOOT_DONE	= 16,
	POWER_MANAGE_PLUGIN_FUNC_MENU_OPENED	= 20,	/* PLACEHOLDER names: impose's menu opened, */
	POWER_MANAGE_PLUGIN_FUNC_GET_LAYER	= 21,	/* and the widget it opens in */
	POWER_MANAGE_PLUGIN_FUNC_IS_BUSY	= 25,	/* PLACEHOLDER: the console info is not written while it is not 0 */
};

/* coldboot_plugin's GetInterface(1) */
struct ColdbootPluginInterface {
	void (*finish)(void);			/* the boot is done: close the cold-boot screens */
};

/* dbrecovery_plugin's GetInterface(1) */
struct DbrecoveryPluginInterface {
	void (*finish)(void);			/* the boot is done: close the recovery screens */
	void (*setFinishCallback)(void (*callback)(paf::Plugin *plugin));
	int (*getState)(void);			/* the database state the recovery left */
	void (*setTheme)(paf::common::SharedPtr<LsdbThemeInfo> *theme);
};

void powerManagePluginSetParam(paf::Plugin::InitParam2 *param);
void powerManagePluginInit(paf::Plugin *plugin);
void powerManagePluginStart(paf::Plugin *plugin);
void powerManagePluginStop(paf::Plugin *plugin);
void powerManagePluginExit(paf::Plugin *plugin);

void telephonyPluginInit(paf::Plugin *plugin);
void telephonyPluginStart(paf::Plugin *plugin);
void telephonyPluginStop(paf::Plugin *plugin);
void telephonyPluginExit(paf::Plugin *plugin);

void musiccorePluginInit(paf::Plugin *plugin);
void musiccorePluginStart(paf::Plugin *plugin);
void musiccorePluginStop(paf::Plugin *plugin);
void musiccorePluginExit(paf::Plugin *plugin);

void imposePluginSetParam(paf::Plugin::InitParam2 *param);
void imposePluginInit(paf::Plugin *plugin);
void imposePluginStart(paf::Plugin *plugin);
void imposePluginStop(paf::Plugin *plugin);
void imposePluginExit(paf::Plugin *plugin);

void indicatorPluginSetParam(paf::Plugin::InitParam2 *param);
void indicatorPluginInit(paf::Plugin *plugin);
void indicatorPluginStart(paf::Plugin *plugin);
void indicatorPluginStop(paf::Plugin *plugin);
void indicatorPluginExit(paf::Plugin *plugin);

void gamePluginInit(paf::Plugin *plugin);
void gamePluginStart(paf::Plugin *plugin);
void gamePluginStop(paf::Plugin *plugin);
void gamePluginExit(paf::Plugin *plugin);

void topmenuPluginSetParam(paf::Plugin::InitParam2 *param);
void topmenuPluginInit(paf::Plugin *plugin);
void topmenuPluginStart(paf::Plugin *plugin);
void topmenuPluginStop(paf::Plugin *plugin);
void topmenuPluginExit(paf::Plugin *plugin);

void coldbootPluginSetParam(paf::Plugin::InitParam2 *param);
void coldbootPluginInit(paf::Plugin *plugin);
void coldbootPluginStart(paf::Plugin *plugin);
void coldbootPluginStop(paf::Plugin *plugin);
void coldbootPluginExit(paf::Plugin *plugin);

void applauncherPluginSetParam(paf::Plugin::InitParam2 *param);
void applauncherPluginInit(paf::Plugin *plugin);
void applauncherPluginStart(paf::Plugin *plugin);
void applauncherPluginStop(paf::Plugin *plugin);
void applauncherPluginExit(paf::Plugin *plugin);

void downloadPluginInit(paf::Plugin *plugin);
void downloadPluginStart(paf::Plugin *plugin);
void downloadPluginExit(paf::Plugin *plugin);

void liveareaPluginSetParam(paf::Plugin::InitParam2 *param);
void liveareaPluginInit(paf::Plugin *plugin);
void liveareaPluginStart(paf::Plugin *plugin);
void liveareaPluginStop(paf::Plugin *plugin);
void liveareaPluginExit(paf::Plugin *plugin);

/* theme_plugin, the wallpaper picker (0x8126E4EC-0x8126E578) */
void themePluginSetParam(paf::Plugin::InitParam2 *param);
void themePluginInit(paf::Plugin *plugin);
void themePluginStart(paf::Plugin *plugin);
void themePluginStop(paf::Plugin *plugin);
void themePluginExit(paf::Plugin *plugin);

void mailResidentPluginInit(paf::Plugin *plugin);
void mailResidentPluginStart(paf::Plugin *plugin);
void mailResidentPluginStop(paf::Plugin *plugin);

/*
 * telephony_plugin's GetInterface(1): its modem and its service objects. Their methods are
 * called through their tables; only the slots the shell's files call are written out.
 */
namespace paf { class IDParam; }

/* what a telephony service method returns: a result code (0x8124E288) */
class TelephonyResult {
public:
	TelephonyResult() : m_code(0) {}
	TelephonyResult(int code) : m_code(code) {}
	~TelephonyResult();
	int GetCode() const;
	void SetCode(int code) { m_code = code; }
	bool IsSuccess() const { return m_code == 0; }

private:
	int m_code;
};

/* the network's country (MCC) and operator (MNC) codes, each with its number of
   digits, formatted strings, and network name (0x8124E28C..0x8124E57F, 0x813DD154..0x813DD158) */
class TelephonyPlmn {
public:
	TelephonyPlmn();
	virtual ~TelephonyPlmn();
	virtual bool IsValid() const;

	void SetMcc(SceUInt16 mcc, SceUInt16 digits);
	void SetMnc(SceUInt16 mnc, SceUInt16 digits);
	void SetMccMnc(SceUInt16 mcc, SceUInt16 mnc);
	void SetMccMncStrings(const paf::string &mccStr, const paf::string &mncStr);
	void SetPlmnString(const paf::string &plmnStr);
	void SetName(const paf::string &name);

	const char *GetMccString();
	const char *GetMncString();
	paf::string GetPlmnString() const;
	paf::string GetName() const;
	bool GetCountryCode(paf::string *outCountry) const;

	int GetMcc() const;
	int GetMnc() const;
	SceUInt16 GetMccDigits() const { return m_mccDigits; }
	SceUInt16 GetMncDigits() const { return m_mncDigits; }

private:
	SceUInt16 m_mcc;
	SceUInt16 m_mccDigits;
	char m_mccStr[6];
	SceUInt16 m_mnc;
	SceUInt16 m_mncDigits;
	char m_mncStr[6];
	paf::string m_name;
};

/* APN credentials / operator entry: name, user ID, password (0x8124E070..0x8124E287) */
class TelephonyOperator {
public:
	TelephonyOperator();
	virtual ~TelephonyOperator();
	virtual void *CloneObject() const;
	virtual bool IsValid() const;

	TelephonyResult SetCredentials(const paf::string &name, const paf::string &userId,
				       const paf::string &password);
	TelephonyResult GetCredentials(paf::string *outName, paf::string *outUserId,
				       paf::string *outPassword) const;
	void SetFromRawRecord(const char *rawRecord);

	const paf::string &GetName() const { return m_name; }
	const paf::string &GetUserId() const { return m_userId; }
	const paf::string &GetPassword() const { return m_password; }

protected:
	paf::string m_name;
	paf::string m_userId;
	paf::string m_password;
};

/*
 * The modem's general state, given to its general listeners whenever it changes (52 bytes;
 * PLACEHOLDER names, after the shell's TelGeneral*CallbackRegister calls the earlier builds
 * made, one per value): the network's type, the registration (1: registered), the operator's
 * names, the signal level, the roaming, the SIM card's condition (1: usable), the flight mode
 * (1: on) and the heat condition (not 0: too hot to serve).
 */
struct TelephonyOperatorNames {
	paf::wstring name;
	paf::wstring shortName;		/* shown when the name has 12 characters or more */
};

struct TelephonyGeneralInfo {
	int networkType;
	int registration;
	TelephonyOperatorNames operatorNames;
	int signalLevel;
	int roaming;
	int simCondition;
	int flightMode;
	int heatCondition;
};

typedef void (*TelephonyGeneralListener)(int event, int result, const TelephonyGeneralInfo *info,
					 int size);

struct TelephonyModem;
struct TelephonyModemVtable {
	void *dtorComplete;								/* +0x00 */
	void *dtorDeleting;								/* +0x04 */
	int (*startModem)(TelephonyModem *self);					/* +0x08 */
	int (*stopModemSoft)(TelephonyModem *self);					/* +0x0C */
	int (*stopModemForce)(TelephonyModem *self);					/* +0x10 */
	int (*addGeneralListener)(TelephonyModem *self, TelephonyGeneralListener listener,
				  void *userData);					/* +0x14 */
	int (*removeGeneralListener)(TelephonyModem *self, TelephonyGeneralListener listener,
				     void *userData);					/* +0x18 */
	int (*getNetworkName)(TelephonyModem *self, char *name);			/* +0x1C */
	int (*needsInitialCheck)(TelephonyModem *self);					/* +0x20 */
	int (*checkSimConditionCode)(TelephonyModem *self);				/* +0x24 */
	int (*getControlKeyStateCode)(TelephonyModem *self);				/* +0x28 */
	int (*getControlKeyBlockTypeCode)(TelephonyModem *self);			/* +0x2C */
	int (*getAntennaDisplayType)(TelephonyModem *self);				/* +0x30 */
	int (*checkContentDownloadCode)(TelephonyModem *self, int serviceType);		/* +0x34 */
	int (*getState)(TelephonyModem *self, int *query);				/* +0x38 */
	int (*getCountryCode)(TelephonyModem *self, paf::string *country);		/* +0x3C */
	int (*getProductTargetOperatorCode)(TelephonyModem *self, int *outOperator);	/* +0x40 */
};
struct TelephonyModem {
	const TelephonyModemVtable *vtable;
};

struct TelephonyService;
struct TelephonyServiceVtable {
	void *dtorComplete;								/* +0x000 */
	void *dtorDeleting;								/* +0x004 */
	int (*stopServiceForce)(TelephonyService *self);				/* +0x008 */
	TelephonyResult (*getGeneralLogicState)(TelephonyService *self, int *outState);	/* +0x00C */
	TelephonyResult (*setFlightMode)(TelephonyService *self, int on);		/* +0x010 */
	TelephonyResult (*getFlightMode)(TelephonyService *self, SceUInt8 *on);		/* +0x014 */
	TelephonyResult (*setDataEnabled)(TelephonyService *self, bool enabled);	/* +0x018 */
	TelephonyResult (*getDataEnabled)(TelephonyService *self, SceUInt8 *enabled);	/* +0x01C */
	void *getPhoneNumber;								/* +0x020 */
	void *getImei;									/* +0x024 */
	void *getIccid;									/* +0x028 */
	void *verifyPin;								/* +0x02C */
	void *unblockPin;								/* +0x030 */
	void *setPinProtection;								/* +0x034 */
	void *getPinProtection;								/* +0x038 */
	void *getPinStatus;								/* +0x03C */
	void *getPinRemain;								/* +0x040 */
	void *changePinCode;								/* +0x044 */
	void *setEventCallback;								/* +0x048 */
	void *setLegacyCallback;							/* +0x04C */
	void *setApnInfo;								/* +0x050 */
	void *setApnInfoWithBillingPlan;						/* +0x054 */
	void *getUnsupportedProfile;							/* +0x058 */
	void *restoreApnInfo;								/* +0x05C */
	TelephonyResult (*getPlmn)(TelephonyService *self, TelephonyPlmn *plmn);	/* +0x060 */
	void *getMccMncOfNetwork;							/* +0x064 */
	void *getSimActivationState;							/* +0x068 */
	void *getProductTargetOperator;							/* +0x06C */
	void *getOperatorProfileCount;							/* +0x070 */
	void *getOperatorProfile;							/* +0x074 */
	void *getApnInfoListCount;							/* +0x078 */
	void *getApnInfoListItem;							/* +0x07C */
	TelephonyResult (*getOperator)(TelephonyService *self, TelephonyOperator *op);	/* +0x080 */
	void *removeCurrentApnInfo;							/* +0x084 */
	void *startNetworkScan;								/* +0x088 */
	void *abortNetworkScan;								/* +0x08C */
	void *getNetworkScanInfoCount;							/* +0x090 */
	void *getNetworkScanInfo;							/* +0x094 */
	void *setNetRegisterAuto;							/* +0x098 */
	void *setNetRegisterInfo;							/* +0x09C */
	void *getNetRegistrationInfo;							/* +0x0A0 */
	void *getSavedNetworkSelection;							/* +0x0A4 */
	void *setRegistrationAppNetMode;						/* +0x0A8 */
	void *setDataRoamingPermission;							/* +0x0AC */
	void *getDataRoamingPermission;							/* +0x0B0 */
	void *setDataRoamingDefinition;							/* +0x0B4 */
	void *checkSimCondition;							/* +0x0B8 */
	void *getNetworkStatusReport;							/* +0x0BC */
	void *getDeviceInfoReport;							/* +0x0C0 */
	TelephonyResult (*getRestriction)(TelephonyService *self, int *restriction);	/* +0x0C4 */
	void *getIsApnInit;								/* +0x0C8 */
	void *isOpenSimModel;								/* +0x0CC */
	void *getNetworkSelectionMode;							/* +0x0D0 */
	void *getConditionError;							/* +0x0D4 */
	void *deactivateSimLock;							/* +0x0D8 */
	void *unblockSimLock;								/* +0x0DC */
	void *getSimLockDeactivateRemain;						/* +0x0E0 */
	void *getSimLockUnblockRemain;							/* +0x0E4 */
	void *getControlKeyState;							/* +0x0E8 */
	void *getControlKeyBlockType;							/* +0x0EC */
	void *checkImsiForOperatorCode;							/* +0x0F0 */
	void *getCurrentSignalLevel;							/* +0x0F4 */
	void *setBillingPlan;								/* +0x0F8 */
	void *getBillingPlan;								/* +0x0FC */
	void *setDebugLogEnabled;							/* +0x100 */
	void *readDebugLogLine;								/* +0x104 */
	void *setDebugDiagFlag;								/* +0x108 */
	void *getDebugDiagFlag;								/* +0x10C */
	void *setDebugForceFlag;							/* +0x110 */
	void *getDebugForceFlag;							/* +0x114 */
	void *isSimActivated;								/* +0x118 */
	void *getLatestArchiveVersion;							/* +0x11C */
	void *getArchiveVersion;							/* +0x120 */
	void *checkComVersion;								/* +0x124 */
	void *releasePdpContext;							/* +0x128 */
	void *startApnListDownload;							/* +0x12C */
	void *checkContentDownloadAllowed;						/* +0x130 */
	TelephonyResult (*getStatus)(TelephonyService *self, int type, int *status);	/* +0x134 */
};
struct TelephonyService {
	const TelephonyServiceVtable *vtable;
};

struct TelephonyPluginInterface {
	void *reserved[2];
	TelephonyModem *(*getModem)(void);
	TelephonyService *(*getService)(void);
};

/*
 * a notice impose_plugin shows: its name, its place in the queue (negative: at once),
 * the icon (a texture of the plugin named, or an image file), the message, and a
 * callback told how it ended
 */
struct ImposeNotice {
	const char *name;
	int priority;
	int reserved;				/* copied, read by nothing */
	const char *pluginName;			/* the plugin with the icon's texture; impose_plugin's
						   where NULL */
	paf::string iconId;			/* the icon's texture: name and hash, laid out as an
						   IDParam */
	SceUInt32 iconHash;
	const char *iconPath;			/* an image file loaded for the icon instead */
	int iconMode;				/* 1: the common frame texture laid over it */
	const wchar_t *message;
	void (*callback)(int result, void *data);	/* 0 timed out, 2 taken down, 3 dropped */
	void *callbackData;
	SceUInt32 flags;			/* IMPOSE_NOTICE_FLAG_* */

	ImposeNotice() : name(NULL), priority(0), reserved(0), pluginName(NULL), iconId(""),
		iconHash(0), iconPath(NULL), iconMode(0), message(NULL), callback(NULL),
		callbackData(NULL), flags(0)
	{
	}

	~ImposeNotice()
	{
	}
};

/* PLACEHOLDER names: the notice ends at its time even with none waiting (4), or stays while
   others wait (2) */
#define IMPOSE_NOTICE_FLAG_STAY		0x2
#define IMPOSE_NOTICE_FLAG_TIMEOUT	0x4

/* impose_plugin's GetInterface(1) */
struct ImposePluginInterface {
	int (*showNotice)(ImposeNotice *notice);
	int (*removeNotice)(const char *name);
	const wchar_t *(*getString)(const paf::IDParam &id);
	void (*openPsMenu)(void);
	void (*closePsMenu)(void);
	void (*setPsMenuLock)(bool locked);
	void (*screenshot)(void);
	void (*zoomRefuse)(void);
	void (*quitApp)(const char *name);
	int (*isPsMenuOpen)(void);
	void (*setPsMenuDisable)(bool disabled);
	void (*openOverlay)(int mode);
	void (*closeOverlay)(void);
	void (*zoomBegin)(void);		/* before the zoom starts */
	void (*zoomEnd)(void);			/* after it ended */
	void (*zoomPrepare)(void);		/* first, on either way */
	void (*zoomShow)(int x, int y);		/* the zoom on, at the viewport's offset */
	void (*zoomHide)(int x, int y);		/* the zoom off, from the offset it had */
	int (*zoomIsBlocked)(void);		/* nonzero: the zoom is not toggled */
	void (*lockTouchEmulation)(void);
	void (*unlockTouchEmulation)(void);
	void (*openParentalControl)(int mode);
	void (*closeParentalControl)(void);
	void (*showParentalNotice)(int mode);
	void (*makeNotificationSpace)(void);
	int (*isVolumeFixed)(void);
	void (*showBusyScreen)(void (*onShown)(void *data), void *data);
};

/*
 * PLACEHOLDER name: format with its %s-style tokens replaced by the texts given, in order, the
 * texts not given empty (0x81343700, in the shell's own code)
 */
paf::wstring shellFormatText(const wchar_t *format, const wchar_t *first = L"",
			     const wchar_t *second = L"", const wchar_t *third = L"",
			     const wchar_t *fourth = L"", const wchar_t *fifth = L"",
			     const wchar_t *sixth = L"", const wchar_t *seventh = L"",
			     const wchar_t *eighth = L"", const wchar_t *ninth = L"",
			     const wchar_t *tenth = L"", const wchar_t *eleventh = L"");

/*
 * PLACEHOLDER name: an RTC tick as the shell shows it, its date in the system's format and its
 * time; empty for a zero tick or without the network's time (0x8134385E, in the shell's own
 * code). The flag is not read.
 */
paf::wstring shellTickText(const paf::rtc::Tick *tick, bool flag);

/*
 * PLACEHOLDER name: a size in bytes as the shell shows it, its unit picked by the size
 * (0x81343594, in the shell's own code)
 */
void shellSizeText(paf::wstring *text, SceUInt64 size);

/*
 * PLACEHOLDER name: count added to the shell's usage counter id, saturating (0x81376BDA, in the
 * shell's own code)
 */
int shellUsageCounterAdd(int id, unsigned int count, int flags);

#endif
