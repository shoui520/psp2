/*
 * The game plugin's promoters: an application's folder made an installed application, its
 * record built and registered (promoter.cpp, 0x810695D0..0x81069D54), and the promoters of each
 * kind of content built on it. PLACEHOLDER names: the shell's "GamePromoter" work pool names the
 * family; the classes and their methods are named from what they do.
 */

#ifndef GAME_PROMOTER_H
#define GAME_PROMOTER_H

#include <scetypes.h>
#include <std/string>

#include "game_plugin.h"
#include "game_appinfo.h"

/* the length of a UTF-8 string in characters, up to the first invalid one (0x810695D0) */
int gameUtf8Length(const char *str);

/*
 * A UTF-8 string converted to UTF-16 into dst, size bytes with its terminator; an invalid or
 * unencodable character gives 0. The characters converted (0x81069622).
 */
int gameUtf8ToUtf16(SceWChar16 *dst, SceSize size, const char *src);

/*
 * PLACEHOLDER: what a promotion's source is built from, 60 bytes: the PSP promoter builds it
 * whole (0x8106A346), the folder sources with what follows. The constructors that read it are in
 * another file of the plugin (0x8107A6F0, 0x8107A90A); its names are ours.
 */
struct GameAppSourceParam {
	GameAppSourceParam(const char *dir, const char *path, const char *name, const char *sysDir)
		: dir(dir), path(path), name(name), options(), sysDir(sysDir)
	{
	}

	GameAppSourceParam(const char *dir, const char *path, const char *name,
			   const paf::string &sysDir)
		: dir(dir), path(path), name(name), sysDir(sysDir)
	{
		options[0] = 0;
		options[1] = 0;
		options[2] = 0;
	}

	~GameAppSourceParam()
	{
	}

	paf::string dir;		/* the folder read: the mount point, else the path */
	paf::string path;		/* the folder promoted */
	paf::string name;		/* its name */
	int options[3];			/* 0 from the promoters */
	paf::string sysDir;		/* dir's sce_sys; the LiveArea's folder for a PSP game */
};

/* what a folder source is built from, 92 bytes; its names are ours */
struct GameFolderSourceParam : GameAppSourceParam {
	GameFolderSourceParam(const char *dir, const char *path, const char *name,
			      const char *sysDir, const char *metaDir, const LsdbAppInfoPtr &info)
		: GameAppSourceParam(dir, path, name, sysDir), metaDir(metaDir), appPath(path),
		  info(info)
	{
	}

	/* the appmeta promoter's: the application's folder given, and no record */
	GameFolderSourceParam(const char *dir, const char *path, const char *name,
			      const char *sysDir, const char *metaDir, const char *appPath)
		: GameAppSourceParam(dir, path, name, sysDir), metaDir(metaDir), appPath(appPath)
	{
	}

	paf::string metaDir;		/* the LiveArea's folder (ur0:appmeta/<name>) */
	paf::string appPath;		/* the folder promoted, again */
	LsdbAppInfoPtr info;		/* the record it had */
};

typedef char GameAppSourceParamSize[sizeof(GameAppSourceParam) == 60 ? 1 : -1];
typedef char GameFolderSourceParamSize[sizeof(GameFolderSourceParam) == 92 ? 1 : -1];

/*
 * PLACEHOLDER: what a promoter reads its application's information from (each kind its own),
 * defined in another file of the plugin (constructor 0x8107A6F0, vtable 0x8152EEB0), 88 bytes.
 * The names are ours.
 */
class GamePromoteSource {
public:
	GamePromoteSource(const GameAppSourceParam &param);
	virtual ~GamePromoteSource();

	/* the application's information set into reg (0x8107A83C) */
	virtual int SetInfo(GameAppRegistration *reg);

	/* the information read into reg */
	virtual int Load(GameAppRegistration *reg) = 0;

	bool m_ready;			/* false from the constructor */
	paf::string m_dir;		/* the folder read */
	paf::string m_path;		/* the folder promoted */
	paf::string m_name;		/* its name */
	int m_option;			/* param.options[2] */
	paf::string m_titleId;		/* the title ID: the name, else the path's last part */
	paf::string m_folderName;	/* likewise */
	SceUInt32 m_flags;		/* GAME_SOURCE_FLAG_* */
	int m_options[2];		/* param.options[0..1] */
	paf::string *m_detail;		/* where an error's detail goes, NULL */
};

typedef char GamePromoteSourceSize[sizeof(GamePromoteSource) == 88 ? 1 : -1];

/* GamePromoteSource::m_flags */
#define GAME_SOURCE_FLAG_UX0		0x1	/* the application is on ux0: */
#define GAME_SOURCE_FLAG_MANUAL		0x2	/* it has a manual */
#define GAME_SOURCE_FLAG_INSTALLED	0x4	/* GameFolderSource::m_installed */

/*
 * PLACEHOLDER: the source of an application folder with its sce_sys, defined in another file of
 * the plugin (constructor 0x8107A90A, vtable 0x8152EEC8), 172 bytes. The names are ours.
 */
class GameFolderSource : public GamePromoteSource {
public:
	GameFolderSource(const GameFolderSourceParam &param);
	virtual ~GameFolderSource();

	virtual int Load(GameAppRegistration *reg);

	/* the application's data folders set into reg */
	virtual int SetDirs(GameAppRegistration *reg) = 0;

	/* the application's paths set into reg */
	virtual int SetPaths(GameAppRegistration *reg) = 0;

	virtual int LoadIcons(GameAppRegistration *reg);		/* 0x8107B990 */
	virtual int LoadLiveArea(GameAppRegistration *reg);	/* 0x8107BA88 */

	paf::string m_sysDir;		/* the folder's sce_sys */
	paf::string m_metaDir;		/* the LiveArea's folder */
	paf::string m_appDir;		/* the application's folder, "" here */
	paf::string m_appPath;		/* the folder promoted */
	paf::string m_extra;		/* "" from the constructor */
	int m_count;			/* 0 from the constructor */
	LsdbAppInfoPtr m_oldInfo;	/* the record the application had */
	bool m_installed;		/* false from the constructor */
	int m_reserved[2];		/* 0 from the constructor */
};

typedef char GameFolderSourceSize[sizeof(GameFolderSource) == 172 ? 1 : -1];

/* the source of an application's folder (constructor 0x810475B2, vtable 0x8152EADC), 172 bytes */
class GameAppSource : public GameFolderSource {
public:
	GameAppSource(const GameFolderSourceParam &param);
	virtual ~GameAppSource();

	virtual int SetDirs(GameAppRegistration *reg);
	virtual int SetPaths(GameAppRegistration *reg);
};

typedef char GameAppSourceSize[sizeof(GameAppSource) == 172 ? 1 : -1];

/*
 * The source of a PSP application's folder: its EBOOT.PBP's param.sfo, or that of its PBOOT.PBP
 * or PARAM.PBP update (constructor 0x8104888E, vtable 0x8152EB04), 172 bytes. The names are ours.
 */
class GamePspSource : public GamePromoteSource {
public:
	GamePspSource(const GameAppSourceParam &param);
	virtual ~GamePspSource();

	virtual int Load(GameAppRegistration *reg);

	/* the folders under the title ID, and the manual (0x81048414) */
	void Init(const GameAppSourceParam &param);

	/* the application's paths and kind set into reg (0x8104959E) */
	int SetPaths(const paf::string *pbpPath, GameAppRegistration *reg);

	/* the GAMEDATA_ID of the titles that share NPJH90038's (0x81049A74) */
	int FixGamedataId(LsdbAppInfoPtr *info, const paf::string *titleId);

	/* the title converted, and the system and application versions (0x81049B4A) */
	int SetTitle(GameAppRegistration *reg);

	/* src's parameters but BOOTABLE and CATEGORY copied into dst (0x81049F98) */
	int CopyInfo(const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst);

	/*
	 * The param.sfo of the PBP at pbpPath read into info, and the newest of its PBOOT.PBP
	 * updates chosen (0x8104A10E); the last two arguments are not read.
	 */
	int LoadSfo(const paf::string &titleId, paf::string *pbpPath, LsdbAppInfoPtr *info,
		    int unused1, int unused2);

	int m_reserved;			/* 0 from the constructor */
	LsdbAppInfoPtr m_sfo;		/* an empty record */
	paf::string m_baseDir;		/* m_dir without the title ID */
	paf::string m_basePath;		/* m_path without the title ID */
	paf::string m_drive;		/* "ux0:" */
	paf::string m_sysDir;		/* param.sysDir */
	paf::string m_paramPath;	/* the PBP the parameters were read from */
	paf::string m_selfPath;		/* the PBP run, under m_basePath */
};

typedef char GamePspSourceSize[sizeof(GamePspSource) == 172 ? 1 : -1];

/* a promotion, as the plugin runs it (vtable 0x8152EC20) */
class GamePromoterIf {
public:
	virtual ~GamePromoterIf()
	{
	}

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg) = 0;
};

/* what gameAppRegister passes on to applauncher_plugin with a promotion (PLACEHOLDER names) */
struct GameRegisterParam {
	bool replace;			/* not set in promoter.cpp */
	bool registerEnd;		/* gameAppRegisterEnd after the registration */
	bool noLicense;			/* the #_up_status from no license */
	bool updateApp;			/* an update: registered with mode 2 */
	bool updateData;		/* likewise */
};

/* the stages of a promotion, for its progress */
#define GAME_PROMOTE_STAGES	4

/*
 * An application's folder at path promoted, its record built from the promoter's source and
 * registered (constructor 0x81069748, destructor 0x81069878, vtable 0x8152EC34), 124 bytes.
 */
class GamePromoter : public GamePromoterIf {
public:
	GamePromoter(const char *path, const GameJobProgress *progress, int userData, float scale);
	virtual ~GamePromoter();

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	/* the folder made ready for the promotion */
	virtual int Prepare(bool flag) = 0;

	/* the source of the application's information, deleted by the caller */
	virtual int CreateSource(GamePromoteSource **source) = 0;

	/* the record built from source, and the application's entry */
	virtual int SetInfo(GamePromoteSource *source, bool registerApp, GameAppRegistration *reg);

	/* what is left once the record is built */
	virtual int Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg)
	{
		return 0;
	}

	/* the progress after stage, of GAME_PROMOTE_STAGES */
	void SetProgress(int stage)
	{
		float percent = gameSumPercent(stage, m_weights, GAME_PROMOTE_STAGES);

		if (m_progress.percent == NULL || percent < *m_progress.percent ||
		    percent > 100.0f)
			return;

		*m_progress.percent = percent;
		if (m_progress.func != NULL)
			m_progress.func(m_progress.arg, *m_progress.percent);
		m_progress.added = 0;
	}

	paf::string m_path;			/* the folder promoted */
	LsdbAppInfoPtr m_info;			/* the record it had */
	bool m_install;				/* registered as an install */
	bool m_checkVersion;			/* cleared by the PSM promoter */
	bool m_keepLiveArea;
	bool m_notifyTopmenu;
	paf::string m_name;			/* the folder's name, after its last '/' */
	paf::string m_appDir;
	paf::string m_dir;
	paf::string m_licensePath;		/* the #_up_status's license folder, when set */
	GameJobProgress m_progress;
	float m_weights[GAME_PROMOTE_STAGES];	/* each stage's part of the progress */
	int m_userData;
	GameRegisterParam m_param;
};

typedef char GamePromoterSize[sizeof(GamePromoter) == 124 ? 1 : -1];

/* promote_patch.cpp: an application's patch promoted (0x8103FB34..0x81045DF6) */

/* the APP_VER of info as a number, 0 when it has none (0x81040852) */
int gameAppInfoGetAppVer(const LsdbAppInfoPtr *info, SceUInt32 *version);

/* the APP_VER of the game card's patch of titleId (0x810408CC) */
int gameGcPatchGetAppVer(const paf::string &titleId, SceUInt32 *version);

/* whether a patch of titleId is in ux0:patch, grw0:patch, the game card's gro0:patch */
bool gameUx0PatchExists(const paf::string &titleId);		/* 0x81040DEE */
bool gameGrw0PatchExists(const paf::string &titleId);		/* 0x81041010 */
bool gameGcPatchExists(const paf::string &titleId);		/* 0x81041232 */

/*
 * Whether the card's title needs its patch installed over what storage holds: storage 1 when
 * grw0 has none, storage 2 when ux0 has none or an older one (0x81041454).
 */
bool gameAppIsUpdateNeeded(const LsdbAppInfoPtr *info, const paf::string &titleId,
			   const int *storage);

/* gameUx0PatchExists, for another file (0x810414D8) */
bool gamePatchExists(const paf::string &titleId);

/*
 * The check of an update against the installed application (update_checker.cpp, constructor
 * 0x8104B81C, vtable 0x8152EB1C), 40 bytes. PLACEHOLDER names.
 */
class GameUpdateChecker {
public:
	GameUpdateChecker(const paf::string &path, bool replace, bool notifyTopmenu,
			  GameJobProgress *progress, paf::string *detail);
	virtual ~GameUpdateChecker();

	int CheckUx0(int installedStorage, int storage, const LsdbAppInfoPtr *installed,
		     const LsdbAppInfoPtr *info, bool versionDiffers);
	int ReturnToCard(const LsdbAppInfoPtr *installed);
	int MoveToTemp(const LsdbAppInfoPtr *installed);
	int CheckGro0(int installedStorage, int storage, const LsdbAppInfoPtr *installed,
		      const LsdbAppInfoPtr *info, bool versionDiffers);
	int Check(const LsdbAppInfoPtr *installed, const LsdbAppInfoPtr *info);

	paf::string m_path;
	paf::string m_name;		/* the path's last component */
	bool m_replace;			/* a card's application may replace its copy on ux0: */
	bool m_reserved;
	bool m_notifyTopmenu;
	GameJobProgress *m_progress;	/* NULL for none */
	paf::string *m_detail;		/* where a failed move's drive goes */
};

typedef char GameUpdateCheckerSize[sizeof(GameUpdateChecker) == 40 ? 1 : -1];

/* what makes an application's appmeta from its folder (vtable 0x8152EB2C); its names are ours */
class GameAppmetaWriterIf {
public:
	virtual ~GameAppmetaWriterIf()
	{
	}

	virtual int Run(GameAppRegistration *reg, const paf::string *dir) = 0;
};

/*
 * An application's appmeta (its icons, LiveArea and parameters) made from its folder in
 * ux0:appmeta or ur0:appmeta (livearea_copier.cpp, constructor 0x8104DAC4, vtable 0x8152EB40),
 * 64 bytes; its names are ours.
 */
class GameLiveAreaCopier : public GameAppmetaWriterIf {
public:
	GameLiveAreaCopier(bool flag, const paf::string &name, GameCancelFunc cancel, void *arg,
			   const GameJobProgress *progress, paf::string *detail, float scale);
	virtual ~GameLiveAreaCopier();

	/* the appmeta of reg's application, a game's, from its folder dir */
	virtual int Run(GameAppRegistration *reg, const paf::string *dir);

private:
	int Write(const paf::string *dir, unsigned int kind, bool gcParam,
		  GameJobProgress *progress, float scale);
	int CopyToNew(const paf::string *dir, const paf::string *titleId, unsigned int kind,
		      bool gcParam, bool onUx0);
	int CopyToUx0(const paf::string *dir, const paf::string *titleId, unsigned int kind,
		      bool gcParam);
	int Copy(const LsdbAppInfoPtr *info, const paf::string *dir);
	int CopyFttIcon(const paf::string *mountPoint, const LsdbAppInfoPtr *info);
	int WriteFtt(const LsdbAppInfoPtr *info, const paf::string *dir);
	int CopyFtt(const LsdbAppInfoPtr *info, const paf::string *dir);

	bool m_flag;			/* a free-trial application, its license file at m_name */
	paf::string m_name;
	GameCancelFunc m_cancel;
	void *m_arg;
	GameJobProgress m_progress;
	float m_weights[3];		/* each step's share of m_progress */
	paf::string *m_detail;		/* where a failed copy's drive goes */
};

typedef char GameLiveAreaCopierSize[sizeof(GameLiveAreaCopier) == 64 ? 1 : -1];

/*
 * A PSM application's LiveArea template.xml written from its app.info (psm_livearea_writer.cpp,
 * constructor 0x810520C4, vtable 0x8152EB54), 52 bytes; its names are ours.
 */
class GamePsmLiveAreaWriter : public GameAppmetaWriterIf {
public:
	GamePsmLiveAreaWriter(GameCancelFunc cancel, void *arg, const GameJobProgress *progress,
			      paf::string *detail, float scale);

	/* the step's share of the progress added, when the registry has room */
	virtual int Run(GameAppRegistration *reg, const paf::string *dir);

	/* the template at path written for the record info */
	int Write(const paf::string &path, const LsdbAppInfoPtr *info);

	GameCancelFunc m_cancel;
	void *m_arg;
	GameJobProgress m_progress;
	float m_weights[3];		/* each step's share of m_progress */
	paf::string *m_detail;
	GamePsmAppInfo *m_appInfo;	/* the app.info the template is written from */
};

typedef char GamePsmLiveAreaWriterSize[sizeof(GamePsmLiveAreaWriter) == 52 ? 1 : -1];

/*
 * The appmeta step of a PSP application's promotion, which only counts its share of the progress
 * (psp_icon.cpp, constructor 0x81053840, vtable 0x8152EB68), 48 bytes; its names are ours. Its
 * Run is GamePsmLiveAreaWriter's code, which Sony's link keeps once (0x81052114).
 */
class GamePspAppmetaWriter : public GameAppmetaWriterIf {
public:
	GamePspAppmetaWriter(GameCancelFunc cancel, void *arg, const GameJobProgress *progress,
			     paf::string *detail, float scale);

	/* the commit's share of the progress added, when the registry has room */
	virtual int Run(GameAppRegistration *reg, const paf::string *dir);

	GameCancelFunc m_cancel;
	void *m_arg;
	GameJobProgress m_progress;
	float m_weights[3];		/* each step's share of m_progress */
	paf::string *m_detail;
};

typedef char GamePspAppmetaWriterSize[sizeof(GamePspAppmetaWriter) == 48 ? 1 : -1];

/* the drives a patch is promoted on */
#define GAME_PATCH_DRIVE_GRW0	1
#define GAME_PATCH_DRIVE_UX0	2

/*
 * An application's patch promoted: its folder moved from the drive's temp/promote into its
 * patch folder (constructor 0x8103FB34, destructor 0x8103FBA0, vtable 0x8152EA88), 148 bytes.
 * PLACEHOLDER names.
 */
class GamePatchPromoter : public GamePromoter {
public:
	GamePatchPromoter(const char *path, const LsdbAppInfoPtr *sfo, bool flag1, bool flag2,
			  const GameJobProgress *progress, int userData, float scale);
	virtual ~GamePatchPromoter();

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);
	virtual int Prepare(bool flag);
	virtual int CreateSource(GamePromoteSource **source);
	virtual int SetInfo(GamePromoteSource *source, bool registerApp, GameAppRegistration *reg);
	virtual int Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg);

	/* the promoted folder moved into the drive's patch folder (0x8103FC64) */
	int MovePatch();

	/* the game card's patch copied into the drive's temp/promote (0x810414E0) */
	int CopyCardPatch(GameJobProgress *progress);

	/*
	 * When m_move is set, the game card's patch copied into the drive's temp/promote and the
	 * card's application mounted with it (0x81041C0C).
	 */
	int MountCardPatch();

	/* the patch's param.sfo read into info (0x81042EA0) */
	int ReadSfo(LsdbAppInfoPtr *info);

	/* the installed application's record checked against the patch's (0x81042154) */
	int CheckInstalled(LsdbAppInfoPtr *installed, bool flag);

	const LsdbAppInfoPtr *m_sfo;	/* the parameters an update keeps, when set */
	paf::string m_mountPoint;	/* unmounted when set */
	bool m_flag1;
	bool m_flag2;
	bool m_move;			/* MovePatch after the promotion */
	int m_drive;			/* GAME_PATCH_DRIVE_* */
};

typedef char GamePatchPromoterSize[sizeof(GamePatchPromoter) == 148 ? 1 : -1];

/*
 * install_job.cpp: the install of a package of GameInstallParam (0x81045DF6..0x8104639C).
 *
 * PLACEHOLDER: the promoters of each kind of package, defined in other files of the plugin
 * (constructors 0x81073E90, 0x8107832C, 0x81076694, 0x81077624, on the base 0x81096008); their
 * names are ours, from the kind each one is made for.
 */
class GameAppInstaller : public GamePromoterIf {		/* kind 0, 160 bytes */
public:
	GameAppInstaller(const GameInstallParam *param, GameJobProgress *progress,
			 paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[156];		/* PLACEHOLDER: laid out with its file */
};

class GamePatchInstaller : public GamePromoterIf {		/* kind 1, 156 bytes */
public:
	GamePatchInstaller(const GameInstallParam *param, GameJobProgress *progress,
			   paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[152];		/* PLACEHOLDER: laid out with its file */
};

class GameLicenseInstaller : public GamePromoterIf {		/* kind 2, 128 bytes */
public:
	GameLicenseInstaller(const GameInstallParam *param, GameJobProgress *progress,
			     paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[124];		/* PLACEHOLDER: laid out with its file */
};

class GameAddcontInstaller : public GamePromoterIf {		/* kind 3, 172 bytes */
public:
	GameAddcontInstaller(const GameInstallParam *param, GameJobProgress *progress,
			     paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[168];		/* PLACEHOLDER: laid out with its file */
};

/* the kinds of GameInstallParam */
#define GAME_INSTALL_KIND_APP		0
#define GAME_INSTALL_KIND_PATCH		1
#define GAME_INSTALL_KIND_LICENSE	2
#define GAME_INSTALL_KIND_ADDCONT	3

/*
 * The install of a package of GameInstallParam: the promoter of its kind run on the job's
 * registration (constructor 0x81045E92, destructor 0x81046052, vtable 0x8152EAAC), 300 bytes.
 * PLACEHOLDER names.
 */
class GameInstallJob {
public:
	GameInstallJob(const GameInstallParam *param, bool flag, bool (*isStopRequested)(void),
		       int arg4);
	virtual ~GameInstallJob();

	/* the install run: the promoter of the package's kind, then the progress at 100 */
	int Run();

	/* the parameters checked; the package's name taken from its path (0x81045DF6) */
	int CheckParam();

	/* the promoter of param's kind (0x81046196) */
	int CreatePromoter(const GameInstallParam *param, GamePromoterIf **promoter,
			   GameJobProgress *progress, paf::string *errorTitleId);

	int m_result;				/* CheckParam's */
	GameInstallParam m_param;
	GameAppRegistration m_reg;
	paf::string m_name;			/* the package's name, after its path's last '/' */
	paf::string m_otherName;		/* not read in this file */
	bool m_flag;				/* the promoter's registerApp */
	bool m_otherFlag;			/* not read in this file */
	bool (*m_isStopRequested)(void);	/* the promoter's cancel */
	int m_arg4;				/* its argument */
	float m_percent;
	GameJobProgress m_progress;
	paf::string m_errorTitleId;		/* the title ID an error names */
};

typedef char GameInstallJobSize[sizeof(GameInstallJob) == 300 ? 1 : -1];

/*
 * install_ex_job.cpp: the install of a package folder of GameInstallExParam, by its install
 * information (0x8104639C..0x81046D04).
 *
 * PLACEHOLDER: the promoters of each content type, defined in other files of the plugin; their
 * names are ours, from the content type each one is made for.
 */
/* a PSP game without PBOOT.PBP (constructor 0x81071890), 156 bytes */
class GamePspInstaller : public GamePromoterIf {
public:
	GamePspInstaller(const char *path, const SceNpDrmInstallInfo *info,
	                 GameJobProgress *progress, paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[152];		/* PLACEHOLDER: laid out with its file */
};

/* a PSP game with PBOOT.PBP (constructor 0x810729E4), 152 bytes */
class GamePspPbootInstaller : public GamePromoterIf {
public:
	GamePspPbootInstaller(const char *path, const SceNpDrmInstallInfo *info,
	                      GameJobProgress *progress, paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[148];		/* PLACEHOLDER: laid out with its file */
};

/* content type 17 (constructor 0x81073D34), 124 bytes */
class GameType17Installer : public GamePromoterIf {
public:
	GameType17Installer(const char *path, const SceNpDrmInstallInfo *info,
	                    GameJobProgress *progress, paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[120];		/* PLACEHOLDER: laid out with its file */
};

/*
 * PLACEHOLDER: the check of an application's update package against the installed application,
 * defined in another file of the plugin (constructor 0x8107DA08, vtable 0x8152EF18), 36 bytes.
 */
class GameGdUpdateCheck {
public:
	GameGdUpdateCheck(const paf::string &titleId, const paf::string &path, bool notImported,
			  paf::string *errorTitleId);
	virtual ~GameGdUpdateCheck();

	/* the check against the installed application's record (0x8107DD86) */
	int Check(const LsdbAppInfoPtr *info);

	SceUInt8 m_body[32];
};

typedef char GameGdUpdateCheckSize[sizeof(GameGdUpdateCheck) == 36 ? 1 : -1];

/*
 * PLACEHOLDER: the installers' base, defined in another file of the plugin (constructor
 * 0x81095248, destructor 0x8109541A, vtable 0x8152F090), 124 bytes: the package folder's source
 * made, its record built and registered, by stages. The names are ours.
 */
class GameInstallSource;

class GamePackageInstaller : public GamePromoterIf {
public:
	GamePackageInstaller(const char *path, const SceNpDrmInstallInfo *info,
			     GameJobProgress *progress, paf::string *errorTitleId, float scale);
	virtual ~GamePackageInstaller();

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	/* the folder made ready for the install */
	virtual int Prepare(bool flag)
	{
		return 0;
	}

	/* the source of the package folder, its error in *ret */
	virtual GameInstallSource *CreateSource(int *ret)
	{
		*ret = 0;
		return NULL;
	}

	/* once the source has installed the package to dir and path */
	virtual int Installed(paf::string *dir, paf::string *path)
	{
		return 0;
	}

	/* what is undone after a failed or finished install */
	virtual int Cleanup(void)
	{
		return 0;
	}

	/* the record built from the source (0x81095DA4) */
	virtual int SetInfo(bool registerApp, GameAppRegistration *reg);

	/* the record of an install the source reports as not to be registered */
	virtual int SetOtherInfo(bool flag, GameAppRegistration *reg, paf::string *dir,
				 paf::string *path)
	{
		return 0;
	}

	/* what is left once the record is registered */
	virtual int Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg)
	{
		return 0;
	}

	const char *m_path;			/* the package folder, at the head of its GameInstallExParam */
	const SceNpDrmInstallInfo *m_info;
	GameInstallSource *m_source;		/* CreateSource's */
	GameInstallSource *m_infoSource;	/* the source SetInfo reads */
	GameJobProgress m_progress;
	float m_weights[6];			/* each stage's part of the progress */
	bool m_flag;
	bool m_prepareFlag;			/* Prepare's */
	bool m_otherFlag;
	paf::string m_titleId;			/* the install information's */
	paf::string m_appDir;
	paf::string m_dir;
	paf::string m_otherDir;
	paf::string *m_errorTitleId;		/* the title ID an error names */
	bool m_noLicense;			/* the folder's GameInstallExParam names no license */
};

typedef char GamePackageInstallerSize[sizeof(GamePackageInstaller) == 124 ? 1 : -1];

/* PLACEHOLDER: a source of an installer, defined in another file of the plugin */
class GameInstallSource {
public:
	virtual ~GameInstallSource();
};

/*
 * PLACEHOLDER: the source of an additional content's package folder, defined in another file of
 * the plugin (constructor 0x810875F0), 64 bytes.
 */
class GameAddcontSource : public GameInstallSource {
public:
	GameAddcontSource(const char *path, const SceNpDrmInstallInfo *info, LsdbAppInfoPtr *appInfo,
			  paf::string *errorTitleId);

	SceUInt8 m_body[60];
};

/*
 * An additional content installed (ac_installer.cpp, constructor 0x8106D0B0, vtable 0x8152ECC4),
 * 132 bytes: its application's record read, the install held while the application runs with
 * its additional contents, and the #appsize's additional contents' field reset.
 */
class GameAcInstaller : public GamePackageInstaller {
public:
	GameAcInstaller(const char *path, const SceNpDrmInstallInfo *info,
	                GameJobProgress *progress, paf::string *errorTitleId, float scale = 1.0f);
	virtual ~GameAcInstaller();

	virtual int Prepare(bool flag);
	virtual GameInstallSource *CreateSource(int *ret);
	virtual int Installed(paf::string *dir, paf::string *path);

	LsdbAppInfoPtr m_appInfo;		/* the application's record */
};

typedef char GameAcInstallerSize[sizeof(GameAcInstaller) == 132 ? 1 : -1];

/*
 * PLACEHOLDER: the sources of an application's package folder, defined in other files of the
 * plugin: an update's (constructor 0x8108A81C, 36 bytes) and an application's (constructor
 * 0x8108A0FC, 40 bytes).
 */
class GameGdUpdateSource : public GameInstallSource {
public:
	GameGdUpdateSource(const SceNpDrmInstallInfo *info, const LsdbAppInfoPtr *appInfo,
			   paf::string *errorTitleId);

	SceUInt8 m_body[32];
};

class GameGdAppSource : public GameInstallSource {
public:
	GameGdAppSource(const SceNpDrmInstallInfo *info, bool noLicense, const paf::string &mountPoint,
			paf::string *errorTitleId);

	SceUInt8 m_body[36];
};

/*
 * An application installed (gd_installer.cpp, constructor 0x8106D3EC, vtable 0x8152ECF8), 156
 * bytes: its record read and checked against the update, its folder moved into place and its
 * record built from its param.sfo.
 */
class GameGdInstaller : public GamePackageInstaller {
public:
	GameGdInstaller(const char *path, const SceNpDrmInstallInfo *info,
	                GameJobProgress *progress, paf::string *errorTitleId, float scale = 1.0f);
	virtual ~GameGdInstaller();

	virtual int Prepare(bool flag);
	virtual GameInstallSource *CreateSource(int *ret);
	virtual int Installed(paf::string *dir, paf::string *path);
	virtual int Cleanup(void);
	virtual int SetInfo(bool registerApp, GameAppRegistration *reg);
	virtual int SetOtherInfo(bool flag, GameAppRegistration *reg, paf::string *dir,
				 paf::string *path);
	virtual int Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg);

	/* the application unmounted, unless the caller mounted it, and its mount point cleared */
	int UnmountApp(void)
	{
		if (m_noLicense) {
			m_mountPoint.assign(NULL, 0);
			return 0;
		}

		int ret = gameAppUnmount(&m_mountPoint);

		if (ret < 0) {
			m_mountPoint.assign(NULL, 0);
			return ret;
		}

		m_mountPoint.assign(NULL, 0);
		return 0;
	}

	/* GAME_ERROR_BAD_DATA when bit 0 of the two records' ATTRIBUTE differs (0x8106D832) */
	int CheckAttribute(const LsdbAppInfoPtr *installed, const LsdbAppInfoPtr *update);

	LsdbAppInfoPtr m_appInfo;		/* the installed application's record */
	paf::string m_mountPoint;		/* the application's, unmounted unless m_noLicense */
	int m_otherWord;
	bool m_install;				/* cleared when the update is already in place */
	int m_lastWord;
};

typedef char GameGdInstallerSize[sizeof(GameGdInstaller) == 156 ? 1 : -1];

/* a PlayStation Mobile game (constructor 0x810713C0), 184 bytes */
class GamePsmInstaller : public GamePromoterIf {
public:
	GamePsmInstaller(const char *path, const SceNpDrmInstallInfo *info,
	                 GameJobProgress *progress, paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[180];		/* PLACEHOLDER: laid out with its file */
};

/* the content types an install information names (values from the switch at 0x810468E2) */
#define GAME_CONTENT_TYPE_17		17	/* PLACEHOLDER: what it is is not read */
#define GAME_CONTENT_TYPE_GD		21
#define GAME_CONTENT_TYPE_PSM		24
#define GAME_CONTENT_TYPE_PSM_UNITY	29

/*
 * The install of a package folder of GameInstallExParam: the promoter of its content type run
 * on the job's registration (constructor 0x810465F8, vtable 0x8152EABC), 1336 bytes.
 * PLACEHOLDER names.
 */
class GameInstallExJob {
public:
	GameInstallExJob(const GameInstallExParam *param, bool flag, int arg3, int arg4);
	virtual ~GameInstallExJob();

	/* the install run: the promoter of the content type, then the progress at 100 */
	int Run();

	/* the parameters checked, and the folder's install information read (0x8104654A) */
	int CheckParam();

	/* the promoter of info's content type, for the folder at path (0x810468E2) */
	static int CreatePromoter(const char *path, const SceNpDrmInstallInfo *info,
				  GamePromoterIf **promoter, GameJobProgress *progress,
				  paf::string *errorTitleId);

	int m_result;				/* CheckParam's */
	GameInstallExParam m_param;
	bool m_readInfo;			/* CheckParam reads m_info */
	SceNpDrmInstallInfo m_info __attribute__((aligned(8)));	/* at 304 in Sony's layout */
	GameAppRegistration m_reg;
	float m_percent;
	GameJobProgress m_progress;
	bool m_flag;				/* the promoter's registerApp */
	bool m_otherFlag;			/* not read in this file */
	bool m_flag2;				/* not read in this file */
	int m_arg3;				/* the promoter's cancel */
	int m_arg4;				/* its argument */
	paf::string m_errorTitleId;		/* the title ID an error names */
};

typedef char GameInstallExJobSize[sizeof(GameInstallExJob) == 1336 ? 1 : -1];

/*
 * app_promoter.cpp: the promotion of an application's directory, by the promoter its path asks
 * for (0x81046D04..0x810475B2).
 *
 * PLACEHOLDER: the promoters of a PSP game, of an application in ux0:appmeta and of a
 * PlayStation Mobile game, defined in other files of the plugin; their names are ours.
 */
/*
 * A PSP game's folder in ux0:pspemu/PSP/GAME promoted from its EBOOT.PBP's param.sfo
 * (psp_promoter.cpp, constructor 0x8106A1E8, destructor 0x8106A248, vtable 0x8152EC7C), 152
 * bytes. PLACEHOLDER names.
 */
class GamePspPromoter : public GamePromoter {
public:
	GamePspPromoter(const char *path, const char *installPath, bool writeAppmeta,
			const GameJobProgress *progress, int userData, float scale);
	virtual ~GamePspPromoter();

	virtual int Prepare(bool flag);
	virtual int CreateSource(GamePromoteSource **source);
	virtual int SetInfo(GamePromoteSource *source, bool registerApp, GameAppRegistration *reg);
	virtual int Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg);

	paf::string m_installPath;	/* the folder the game is installed to, when not m_path */
	paf::string m_paramPath;	/* the source's PBP the parameters were read from */
	bool m_writeAppmeta;		/* the appmeta step run after the record */
};

typedef char GamePspPromoterSize[sizeof(GamePspPromoter) == 152 ? 1 : -1];

/*
 * PLACEHOLDER: the source of an application promoted from its ux0:appmeta folder, defined in
 * another file of the plugin (constructor 0x8107C0DC), 188 bytes. The names are ours.
 */
class GameAppmetaSource : public GameFolderSource {
public:
	GameAppmetaSource(const GameFolderSourceParam &param);
	virtual ~GameAppmetaSource();

	virtual int SetDirs(GameAppRegistration *reg);
	virtual int SetPaths(GameAppRegistration *reg);

	SceUInt8 m_body[16];		/* PLACEHOLDER: laid out with its file */
};

typedef char GameAppmetaSourceSize[sizeof(GameAppmetaSource) == 188 ? 1 : -1];

/*
 * An application promoted from its ux0:appmeta folder, the game card's or ux0:app's
 * (appmeta_promoter.cpp, constructor 0x8106AC10, destructor 0x8106AC6C, vtable 0x8152ECA0), 164
 * bytes. PLACEHOLDER names.
 */
class GameAppmetaPromoter : public GamePromoter {
public:
	GameAppmetaPromoter(const char *path, const GameJobProgress *progress, int userData,
			    float scale);
	virtual ~GameAppmetaPromoter();

	virtual int Prepare(bool flag);
	virtual int CreateSource(GamePromoteSource **source);
	virtual int SetInfo(GamePromoteSource *source, bool registerApp, GameAppRegistration *reg);
	virtual int Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg);

	LsdbAppInfoPtr m_gcSfo;		/* the card's gc_param.sfo, kept in ux0:appmeta */
	paf::string m_appMountPoint;	/* ux0:app's game data, mounted */
	paf::string m_dataMountPoint;	/* the application's data, mounted */
	bool m_hasUpStatus;		/* m_upStatus set for the record */
	SceUInt32 m_upStatus;		/* the installed record's #_up_status */
};

typedef char GameAppmetaPromoterSize[sizeof(GameAppmetaPromoter) == 164 ? 1 : -1];

/*
 * PLACEHOLDER: what a PlayStation Mobile game's promotion reads its app.info with, defined in
 * another file of the plugin (constructor 0x81094CF8, destructor 0x81094D58, vtable 0x8152F080),
 * 36 bytes. The names are ours.
 */
class GamePsmAppReader {
public:
	GamePsmAppReader(int userData);
	virtual ~GamePsmAppReader();

	/* the app.info made, and whether info is installed already (0x81094E02) */
	int Prepare(bool flag, LsdbAppInfoPtr *info, bool *installed);

	/* the source of the application at path, from its app.info (0x81094E80) */
	int CreateSource(const paf::string &appInfoPath, const paf::string &dir,
			 const paf::string &path, GamePromoteSource **source);

	/* the app.info's information set into reg (0x81095056) */
	int SetInfo(bool registerApp, const LsdbAppInfoPtr *info, GameAppRegistration *reg);

	/* the content ID read from the file at path into info (0x8109506C) */
	int ReadContentId(const paf::string &path, LsdbAppInfoPtr *info);

	/* the LiveArea written from the app.info, scale of progress (0x8109510E) */
	int Finish(GameAppRegistration *reg, const GameJobProgress *progress,
		   GameCancelFunc cancel, void *arg, float scale);

	GamePsmAppInfo *m_appInfo;
	paf::string m_path;
	paf::string m_name;		/* the application's folder name */
	int m_userData;
};

typedef char GamePsmAppReaderSize[sizeof(GamePsmAppReader) == 36 ? 1 : -1];

/*
 * A PlayStation Mobile game's folder in ux0:psm promoted (psm_promoter.cpp, constructor
 * 0x81069D54, destructor 0x81069D98, vtable 0x8152EC58), 176 bytes. PLACEHOLDER names.
 */
class GamePsmPromoter : public GamePromoter {
public:
	GamePsmPromoter(const char *path, bool flag, const GameJobProgress *progress, int userData,
			float scale);
	virtual ~GamePsmPromoter();

	virtual int Prepare(bool flag);
	virtual int CreateSource(GamePromoteSource **source);
	virtual int SetInfo(GamePromoteSource *source, bool registerApp, GameAppRegistration *reg);
	virtual int Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg);

	GamePsmAppReader m_reader;
	paf::string m_otherPath;	/* not read in this file */
	bool m_flag;			/* not read in this file */
};

typedef char GamePsmPromoterSize[sizeof(GamePsmPromoter) == 176 ? 1 : -1];

/*
 * The promotion of an application's directory: the promoter its path asks for run on the
 * caller's registration (constructors 0x81046D04 and 0x81046E2C, destructor 0x81046F6E, vtable
 * 0x8152EACC), 96 bytes. PLACEHOLDER names; the flags' meaning is partly read.
 */
class GameAppPromoter {
public:
	GameAppPromoter(const char *path, bool flag, GameAppRegistration *reg,
			bool (*isStopRequested)(void), void *arg, int unused,
			void (*progressFunc)(void *arg, float percent), void *progressArg);
	GameAppPromoter(const GamePromoteParam *param, bool flag, GameAppRegistration *reg,
			bool (*cancel)(void *arg), void *arg);
	virtual ~GameAppPromoter();

	/* the promotion run: the promoter of the path, then the progress at 100 */
	int Run();

	/* whether path is in an application folder of gro0:, vs0:, ux0: or pd0: (0x81047030) */
	static bool IsAppPath(const paf::string *path);

	/* the promoter path asks for (0x810471B4) */
	static int CreatePromoter(const paf::string *path, const LsdbAppInfoPtr *sfo, bool option,
				  bool otherFlag, GamePromoter **promoter,
				  GameJobProgress *progress, paf::string *errorTitleId);

	int m_result;			/* the constructor's result */
	LsdbAppInfoPtr m_sfo;		/* the parameter's record, when given */
	GameAppRegistration *m_reg;
	paf::string m_path;
	paf::string m_name;		/* the directory's name, after its path's last '/' */
	bool m_registerApp;		/* the promoter's registerApp */
	bool m_option;			/* the parameter's option */
	bool m_installed;		/* the promoter had nothing to show */
	bool m_promoted;		/* the promoter's m_install */
	bool m_otherFlag;
	bool m_replaced;		/* GAME_ERROR_GC_REPLACED from the promoter */
	GameCancelFunc m_cancel;
	void *m_arg;
	float m_percent;
	GameJobProgress m_progress;
	paf::string m_errorTitleId;	/* the title ID an error names */
};

typedef char GameAppPromoterSize[sizeof(GameAppPromoter) == 96 ? 1 : -1];

#endif
