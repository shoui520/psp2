/*
 * plugins.h: the plugins linked into SceShell, as the shell's files load and call them. Names
 * ours: the 3.6x shell names none of them.
 */
#ifndef _SHELL_PLUGINS_H
#define _SHELL_PLUGINS_H

#include <plugin.h>
#include <std/string>

/*
 * PLACEHOLDER: the function table a shell plugin gives through GetInterface(1), indexed until
 * each plugin's table is written out from its own file.
 */
struct ShellPluginInterface {
	void (*func[1])(void);
};

/* topmenu_plugin's functions */
enum {
	TOPMENU_PLUGIN_FUNC_BOOT_DONE	= 88,
};

/* power_manage_plugin's functions */
enum {
	POWER_MANAGE_PLUGIN_FUNC_POWER_EVENT	= 15,
	POWER_MANAGE_PLUGIN_FUNC_BOOT_DONE	= 16,
};

/* the theme the shell shows (theme.cpp's; layout not written out yet) */
struct ShellTheme;

/* the theme the home screen applies (theme_apply) */
int shellThemeGetApplyState(void);
bool shellThemeIsApplying(void);
ShellTheme *shellThemeGetCurrent(void);

/* coldboot_plugin's GetInterface(1) */
struct ColdbootPluginInterface {
	void (*finish)(void);			/* the boot is done: close the cold-boot screens */
};

/* dbrecovery_plugin's GetInterface(1) */
struct DbrecoveryPluginInterface {
	void (*finish)(void);			/* the boot is done: close the recovery screens */
	void (*setFinishCallback)(void (*callback)(paf::Plugin *plugin));
	int (*getState)(void);			/* the database state the recovery left */
	void (*setTheme)(ShellTheme *theme);
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

void mailResidentPluginInit(paf::Plugin *plugin);
void mailResidentPluginStart(paf::Plugin *plugin);
void mailResidentPluginStop(paf::Plugin *plugin);

/*
 * telephony_plugin's GetInterface(1): its modem and its service objects. Their methods are
 * called through their tables; only the slots the shell's files call are written out.
 */
namespace paf { class IDParam; }

/* what a telephony service method returns: a result code (methods out of line: 0x8124E288,
   the destructor folded into an empty function) */
class TelephonyResult {
public:
	~TelephonyResult();
	int GetCode();

private:
	int m_code;
};

struct TelephonyModem;
struct TelephonyModemVtable {
	void *reserved0[7];
	int (*getNetworkName)(TelephonyModem *self, char *name);			/* +0x1C */
	void *reserved1[6];
	int (*getState)(TelephonyModem *self, int *query);				/* +0x38 */
};
struct TelephonyModem {
	const TelephonyModemVtable *vtable;
};

struct TelephonyService;
struct TelephonyServiceVtable {
	void *reserved0[7];
	TelephonyResult (*getDataEnabled)(TelephonyService *self, SceUInt8 *enabled);	/* +0x1C */
	void *reserved1[41];
	TelephonyResult (*getRestriction)(TelephonyService *self, int *restriction);	/* +0xC4 */
	void *reserved2[27];
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

/* a notice impose_plugin shows: its name, the message and fields the shell leaves 0 */
struct ImposeNotice {
	const char *name;
	int reserved0;
	int reserved1;
	int reserved2;
	paf::string title;
	int reserved3;
	int reserved4;
	int reserved5;
	const wchar_t *message;
	int reserved6;
	int reserved7;
	int reserved8;

	ImposeNotice() : name(NULL), reserved0(0), reserved1(0), reserved2(0), title(""),
		reserved3(0), reserved4(0), reserved5(0), message(NULL), reserved6(0),
		reserved7(0), reserved8(0)
	{
	}

	~ImposeNotice()
	{
	}
};

/* impose_plugin's GetInterface(1) */
struct ImposePluginInterface {
	void (*showNotice)(ImposeNotice *notice);
	void *reserved;
	const wchar_t *(*getString)(const paf::IDParam &id);
};

#endif
