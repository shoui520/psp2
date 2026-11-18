/*
 * game_plugin.h: the game plugin, a module of its own in Sony's early builds
 * (vs0:vsh/game/game_plugin) and linked into SceShell since. The names are ours (no record
 * names them).
 */
#ifndef _GAME_PLUGIN_H
#define _GAME_PLUGIN_H

#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <memory/pool_allocator.h>

/*
 * PLACEHOLDER: the plugin's GetInterface(1) table, 65 functions (0x815431FC), each
 * written out with the plugin's code.
 */
struct GamePluginInterface {
	void (*func[65])(void);
};
extern GamePluginInterface gamePluginInterface;

/*
 * The plugin's main object, made at start and deleted at stop (0x8102B296, 0x8102B2BA). Its
 * name is ours: nothing of Sony's names it.
 */
class GamePlugin {
public:
	GamePlugin(paf::Plugin *plugin);		/* 0x8102AFFC */
	virtual ~GamePlugin();				/* 0x8103037C */

	paf::Plugin *m_plugin;
};

void gamePluginCreate(paf::Plugin *plugin);
void gamePluginDestroy(void);

/* the plugin's errors for a call before start, and while a stop is requested (values only) */
#define GAME_ERROR_NOT_STARTED		-2146430717	/* 0x80101103 */
#define GAME_ERROR_BAD_STORAGE		-2146430715	/* 0x80101105: an application installed on no known storage */
#define GAME_ERROR_STOPPING		-2146430704	/* 0x80101110 */
/* a game card's application missing from gro0:app (value only) */
#define GAME_ERROR_NO_GC_APP		-2146430690	/* 0x8010111E */
/* a mount point that does not fit the caller's buffer (value only) */
#define GAME_ERROR_BUFFER_TOO_SMALL	-2146430697	/* 0x80101117 */
/* the work pool lent out already (value only) */
#define GAME_ERROR_POOL_BUSY		-2146430670	/* 0x80101132 */

/* whether the game plugin has a stop requested (0x8102B2DC) */
bool gameIsStopRequested(void);

/*
 * topmenu_plugin and power_manage_plugin told the plugin is at work (0x8102B2EE), and that it
 * is done (0x8102B386); topmenu's busy count, +1 (0x8102B430) and -1 (0x8102B49A).
 */
void gameShellHold(bool topmenu, bool powerManage);
void gameShellRelease(void);
void gameTopmenuBusyBegin(void);
void gameTopmenuBusyEnd(void);

/* whether an application's category is a game's (0x8102B4F2), EG (0x8102B524), ME (0x8102B556)
   or a PlayStation Mobile application's (0x8102B588) */
bool gameCategoryIsGame(const char *category);
bool gameCategoryIsEg(const char *category);
bool gameCategoryIsMe(const char *category);
bool gameCategoryIsPsm(const char *category);

/*
 * An application's directory mounted, its mount point (at most 16 characters) left in mountPoint
 * (0x8102B736), and unmounted (0x8102B7FC).
 */
int gameAppDirMount(const char *appPath, char *mountPoint, SceSize size);
int gameAppDirUnmount(const char *mountPoint);

/* the PSP emulator's work directory mounted (0x8102B860) and unmounted (0x8102B886) */
int gamePluginPspemuWorkMount(void);
int gamePluginPspemuWorkUnmount(void);

namespace paf {
namespace job {
class JobQueue;
}
}

/* the promoter's job queue, NULL before start (0x8102B8AC); its dialog held (0x8102B8E2) */
paf::job::JobQueue *gamePluginGetJobQueue(void);
void gamePluginSetDialogHold(bool hold);

/* the game card promoter (gc_promote.cpp) */
void gameGcPromoteCreate(GamePlugin *owner);
void gameGcPromoteDestroy(void);
int gameGcPromoteGetPendingTitles(paf::vector<paf::string> *titles);
void gameGcPromoteSetDialogHold(bool hold);
paf::job::JobQueue *gameGcPromoteGetJobQueue(void);
void gameGcPromoteSetNoticeHeld(bool held);

struct GameAppRegistration;

/*
 * An application promoted from its directory at path into reg: 0, 1 for ux0:appmeta/new, 2 or
 * 1 by what the promoter found when report is set, or an error (0x8102B8F8).
 */
int gamePluginPromote(const char *path, bool report, GameAppRegistration *reg);

/*
 * PLACEHOLDER: the promoter of an application's directory, defined in another file of the
 * plugin (constructor 0x81046D04, Run 0x810473E8; 96 bytes). Only what this plugin's main file
 * reads is written out; the flags' names are ours and their meaning is not read yet.
 */
struct GamePromoteParam;

class GameAppPromoter {
public:
	GameAppPromoter(const char *path, bool flag, GameAppRegistration *reg,
			bool (*isStopRequested)(void), int arg5, int arg6, int arg7, int arg8);
	GameAppPromoter(const GamePromoteParam *param, bool flag, GameAppRegistration *reg,
			bool (*cancel)(void *arg), void *arg);	/* 0x81046E2C */
	virtual ~GameAppPromoter();

	int Run();

	int m_result;			/* the constructor's result */
	SceUInt8 otherFields[38];
	bool m_installed;
	bool m_promoted;
	bool m_otherFlag;
	bool m_replaced;
	SceUInt8 moreFields[34];
	paf::string m_errorTitleId;	/* the title ID an error names */
};

/*
 * PLACEHOLDER: what an install (0x8102BFB2) is asked for, 176 bytes, and what an install of the
 * second kind (0x8102C228) is asked for; only the title ID an error names is known.
 */
struct GameInstallParam {
	char path[128];			/* the package's path */
	char titleId[12];
	int kind;			/* 2 for a license update's package */
	bool flag1;
	bool flag2;
	SceUInt8 reserved[2];
	void (*progressFunc)(void *arg, float percent);	/* the install's progress, when set */
	void *progressArg;
	char errorTitleId[16];
	SceUInt8 moreFields[4];
};

typedef char GameInstallParamSize[sizeof(GameInstallParam) == 176 ? 1 : -1];

struct GameInstallExParam {
	char path[128];			/* the package folder's path */
	SceUInt8 otherFields[8];
	void (*progressFunc)(void *arg, float percent);	/* the install's progress, when set */
	void *progressArg;
	char errorTitleId[16];
	SceUInt8 moreFields[128];
};

typedef char GameInstallExParamSize[sizeof(GameInstallExParam) == 288 ? 1 : -1];

/* GameInstallJob and GameInstallExJob, the plugin's two installers: promoter.h */

/* PLACEHOLDER names: what gamePluginParamJobRun's job is given, 140 bytes */
struct GameJobParam {
	char path[128];				/* the package folder */
	bool extracted;				/* PLACEHOLDER name: a theme already in ux0:theme */
	SceUInt8 padding[3];
	void (*progressFunc)(void *arg, float percent);
	void *progressArg;
};

/*
 * PLACEHOLDER: the plugin's license updater, its "SceGameUpdateLicense" state (constructor
 * 0x8106471C, destructor 0x81064750, 272 bytes), defined in another file of the plugin.
 */
class GameLicenseUpdater {
public:
	GameLicenseUpdater();
	~GameLicenseUpdater();

	/* PLACEHOLDER names: the update of titleId's license (0x81064EFE), and 0x81065FC0 */
	int Update(const char *titleId);
	int Run(void *arg);

	/* PLACEHOLDER name: a license update's package found, its folder and path (0x8106715C) */
	int Prepare(const paf::string &titleId, paf::string *contentId, paf::string *dir,
		    paf::string *path);

	SceUInt8 otherFields[272];
};

/*
 * The plugin's work pool, 64 KiB blocks, lent to one user at a time while the plugin runs
 * (0x8102B5F6), and given back (0x8102B6CC).
 */
int gamePluginGetPool(const char *user, paf::common::SharedPtr<paf::memory::PoolAllocator> *pool);
void gamePluginReleasePool(paf::common::SharedPtr<paf::memory::PoolAllocator> *pool);

/* a debug dump of a buffer, empty in this build (0x814098DC) */
void gameDebugDump(const char *label, const void *data, SceSize size);

/*
 * An installation's directory mounted with its keystone, from grw0: when dir is on it, else
 * from ux0:, the mount point left in mountPoint (0x8101E23C), and unmounted (0x8101E434).
 */
int gamePfsMount(const paf::string &dir, const paf::string &name, const void *keystone,
		 paf::string *mountPoint);
int gamePfsUnmount(paf::string *mountPoint);

/* a package's content type: an application, additional content, a PlayStation game (values only) */
#define GAME_CONTENT_TYPE_APP		31
#define GAME_CONTENT_TYPE_PS1		6
#define GAME_CONTENT_TYPE_ADDCONT	22

/* the plugin's error for an argument out of range (value only) */
#define GAME_ERROR_INVALID_ARGUMENT	-2146430720	/* 0x80101100 */

/*
 * A title promoted from param, stopped when cancel says so (game_main.cpp, 0x8102C776; the
 * parameters are in game_appinfo.h).
 */
int gamePluginPromoteParam(GamePromoteParam *param, bool (*cancel)(void *arg), void *arg);

/* whether a PlayStation Mobile application may run on this model (game_main.cpp) */
int gamePluginPsmAppIsAllowed(const paf::string &titleId, bool *allowed, int *model);

/* the plugin's errors several of its files return (values only) */
#define GAME_ERROR_NOT_READY		-2146430719	/* 0x80101101: what it needs is not there (a mount's record, the
						   promoter, the plugins a dialog is shown with) */
#define GAME_ERROR_NO_INFO_MEMORY	-2146430718	/* 0x80101102: no memory for an information */
#define GAME_ERROR_BAD_DATA		-2146430716	/* 0x80101104: a file whose contents do not check */
#define GAME_ERROR_BAD_SIGNED_SFO	-2146430662	/* 0x8010113A: a signed param.sfo whose header does not check */
#define GAME_ERROR_BAD_SELF		-2146430681	/* 0x80101127: a SELF whose headers do not check */
#define GAME_ERROR_BAD_PARAM_SIZE	-2146430659	/* 0x8010113D: a saved copy of the eboot parameters of the wrong size */
#define GAME_ERROR_NO_DISC_INFO		-2146430660	/* 0x8010113C: a disc list without the game */
#define GAME_ERROR_LICENSE_PERIOD	-2146430655	/* 0x80101141: a license outside its period */
#define GAME_ERROR_BAD_LICENSE_SIZE	-2146430653	/* 0x80101143: a license file larger than a license */
#define GAME_ERROR_NO_VALUE		-2146430713	/* 0x80101107: a key the record does not have */
/* an application the database has no record of, or a record without a key (value only) */
#define GAME_ERROR_NO_RECORD		-2146430711	/* 0x80101109 */
/* a license that gives no #_up_status (value only) */
#define GAME_ERROR_NO_UP_STATUS		-2146430652	/* 0x80101144 */
#define GAME_ERROR_NO_MEMORY		-2146430709	/* 0x8010110B */
#define GAME_ERROR_EBOOT		-2146430699	/* 0x80101115: an EG or ME application's EBOOT.PBP that does not open */
#define GAME_ERROR_FTT_MISMATCH	-2146430654	/* 0x80101142: a free-trial license of another content */
#define GAME_ERROR_NO_FTT		-2146430576	/* 0x80101190: no free-trial license file */
#define GAME_ERROR_NO_DRIVE		-2146430682	/* 0x80101126: a path without a drive */
#define GAME_ERROR_UNREGISTER		-2146430679	/* 0x80101129: applauncher_plugin kept an application */
#define GAME_ERROR_GC_QUIET		-2146430680	/* 0x80101128: nothing to show (an update already in place) */
#define GAME_ERROR_NO_UX0		-2146430676	/* 0x8010112C: ux0: is not there */
#define GAME_ERROR_NO_INSTALL_INFO	-2146430692	/* 0x8010111C: a folder without its install information */
#define GAME_ERROR_NOT_APP_PACKAGE	-2146430672	/* 0x80101130: a package of another content type */
/* a category the call does not handle (value only) */
#define GAME_ERROR_UNKNOWN_CATEGORY	-2146430673	/* 0x8010112F */
/* a path on no storage the plugin knows (value only) */
#define GAME_ERROR_NO_STORAGE		-2146430663	/* 0x80101139 */
#define GAME_ERROR_ABORTED		-2146430712	/* 0x80101108: stopped by the caller's cancel check */
#define GAME_ERROR_SYSTEM_UPDATE	-2146430650	/* 0x80101146: content for a newer system */
#define GAME_ERROR_OLD_EBOOT_SIG	-2146430575	/* 0x80101191: a PSP EBOOT signed the old way, where it is not allowed */

/* paf's error for a file that does not exist (value only) */
#define GAME_PAF_ERROR_NOT_FOUND	-2135994334	/* 0x80AF5022 */

/*
 * PLACEHOLDER: what 0x8136D9D4 reads from a split package's head.bin, 0x78 bytes, the head of an
 * installation's SceNpDrmInstallInfo; the content type, the body's size in 32-byte blocks and
 * the content ID are known.
 */
struct GamePackageInfo {
	SceUInt32 headerFields[1];
	SceUInt32 contentType;			/* GAME_CONTENT_TYPE_* */
	SceUInt32 moreHeaderFields[1];
	SceUInt32 bodyBlocks;
	SceUInt32 otherFields[6];
	char contentId[48];			/* "XX0000-XXXXNNNNN_00-XXXXXXXXXXXXXXXX" */
	char titleId[32];
};

typedef char GamePackageInfoSize[sizeof(GamePackageInfo) == 0x78 ? 1 : -1];

/*
 * What NpDrm keeps of an installation, 936 bytes: Sony's name and the names of the three fields
 * its debug dumps print ("sceNpDrmInstallInfo.license", ".keystone", ".param_digest").
 */
struct SceNpDrmInstallInfo {
	GamePackageInfo info;
	SceUInt8 license[512];
	SceUInt8 keystone[256];
	char param_digest[32];			/* what param.sfo's digest is checked against */
	SceSize licenseSize;
	SceUInt32 otherWord;
	SceSize param_digest_size;
	int swVersion;				/* what a PSP EBOOT's signature is made for */
};

typedef char SceNpDrmInstallInfoSize[sizeof(SceNpDrmInstallInfo) == 0x3A8 ? 1 : -1];

/* a job's progress, as its installer advances it */
struct GameJobProgress {
	/* a progress that only counts */
	GameJobProgress(float *percent)
		: percent(percent), busy(0), scale(0.0f), func(NULL), arg(NULL)
	{
	}

	GameJobProgress(float *percent, void (*func)(void *arg, float percent), void *arg)
		: percent(percent), busy(0), scale(1.0f), func(func), arg(arg)
	{
	}

	/* a part of progress, scaled */
	GameJobProgress(const GameJobProgress &progress, float scale)
		: percent(progress.percent), busy(0), scale(progress.scale * scale),
		  func(progress.func), arg(progress.arg)
	{
	}

	/* value added to the progress */
	void Add(float value)
	{
		if (percent == NULL)
			return;

		*percent += scale * value;
		if (func != NULL)
			func(arg, *percent);
	}

	float *percent;
	int busy;				/* cleared when the job ends */
	float scale;
	void (*func)(void *arg, float percent);
	void *arg;
};

/* set while gamePluginAppDeletePart promotes an application again (game_main.cpp) */
extern bool g_gameRepromoting;

/* the title IDs of the additional content an installation left (0x8102B8C4) */
int gamePluginGetAddcontTitles(paf::vector<paf::string> *titles);

/*
 * The plugin's counted mounts (mounter.cpp): an application's game data, with its patch and its
 * license (rif, written to ur0:temp/rif/work.bin, when given), the mount point left in
 * mountPoint (0x8101DE16), and unmounted (0x8101E0B2); the same without writing the license
 * (0x8101DBA0); an application's data (0x8101E56E, 0x8101E6F6); the PSP emulator's work
 * directory (0x8101E84C, 0x8101E9B6); a theme (0x8101ECBA, 0x8101EAE2; 0x8101EF4E); and the drive
 * a mount point stands for (0x8101D89C).
 */
int gameAppMount(const char *appPath, const char *patchPath, const void *rif, SceSize rifSize,
		 paf::string *mountPoint);
int gameAppUnmount(paf::string *mountPoint);
int gameGameDataMount(const char *appPath, const char *patchPath, const char *rifPath,
		      paf::string *mountPoint);
int gameAppDataMount(const paf::string &titleId, paf::string *mountPoint);
int gameAppDataUnmount(paf::string *mountPoint);
int gamePspemuWorkMount(paf::string *mountPoint);
int gamePspemuWorkUnmount(paf::string *mountPoint);
int gameThemeMount(const paf::string &contentId, const paf::string &themePath, const void *rif,
		   SceSize rifSize, paf::string *mountPoint);
int gameThemeDataMount(const paf::string &contentId, const paf::string &themePath,
		       const paf::string &rifPath, paf::string *mountPoint);
int gameThemeUnmount(paf::string *mountPoint);
void gameMountGetDrive(const paf::string &path, paf::string *drive);

/*
 * The drive a path is reached by: the drive of the mount that holds it, else the path's own
 * drive; drive is left as it is when neither is found (0x8101BFB4).
 */
void gamePathGetMountedDrive(const paf::string &path, paf::string *drive);

/*
 * A file or a directory tree removed, by a remover of its own (0x8101BA08); with mapErrors, an
 * error is translated by gameFileToPafError (0x8101C212). PLACEHOLDER: the meaning of the three words is not known.
 */
int gameRemoveDirectory(const char *path, int arg1, int arg2, int arg3, bool mapErrors);

/*
 * A file copied, by a job of its own, with buf as its work memory (0x8101AB80). PLACEHOLDER:
 * the meaning of the other words is not known; this file passes 0 for each.
 */
int gameCopyFile(const char *src, const char *dst, int arg2, int arg3, void *buf,
		 SceSize bufSize, int arg6, int arg7);

/* a split package's information read from its head.bin at path, with buf as work memory */
int gamePackageReadInfo(GamePackageInfo *info, const char *path, void *buf, SceSize bufSize);

/*
 * PLACEHOLDER name: the install information of the package folder at path, which ends in '/',
 * with buf as work memory (0x8136DA0A, in the NpDrm package code the shell links)
 */
int gameNpDrmGetInstallInfo(SceNpDrmInstallInfo *info, SceSize size, const char *path, void *buf,
			    SceSize bufSize);

/* The NP account ID, 0 when the registry has none (0x81018610). */
int gameNpGetAccountId(SceUInt64 *accountId);

/*
 * A PSP game's license: the RIF in ux0:pspemu/PSP/LICENSE named by the content ID in its
 * EBOOT.PBP, its check's result and its validity period (license.cpp; 32 bytes).
 */
class GamePspLicense {
public:
	GamePspLicense();
	virtual ~GamePspLicense();

	int ReadRif(const paf::string &rifPath);
	int GetRifName(const paf::string &ebootPath, paf::string *rifName);
	int Check(const paf::string &path);

	int m_result;
	bool m_valid;
	SceUInt64 m_startTime;
	SceUInt64 m_expireTime;
};

/*
 * The time left until a license's expireTime (0x8101FBEA), that time as text, "-" when it
 * does not fit in size bytes (0x8101FC78), and whether now is in the period (0x8101FD0E).
 */
int gameLicenseGetRemaining(const SceUInt64 *expireTime, SceUInt64 *remaining);
int gameLicenseFormatTime(const SceUInt64 *time, char *text, SceSize size);
int gameLicenseCheckPeriod(const SceUInt64 *startTime, const SceUInt64 *expireTime);

#endif
