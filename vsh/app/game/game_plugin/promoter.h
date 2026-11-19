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

	/* an installer's: the application's folder given, and the record it had */
	GameFolderSourceParam(const char *dir, const char *path, const char *name,
			      const char *sysDir, const char *metaDir, const char *appPath,
			      const LsdbAppInfoPtr &info)
		: GameAppSourceParam(dir, path, name, sysDir), metaDir(metaDir), appPath(appPath),
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
 * What an installer's SetInfo reads an application's information from: an interface, its
 * destructor shared by the plugin (0x814101BC, vtable 0x8152EE9C). The name is ours.
 */
class GameInfoSource {
public:
	virtual ~GameInfoSource()
	{
	}

	/* the application's information set into reg */
	virtual int SetInfo(GameAppRegistration *reg) = 0;
};

/*
 * What a promoter reads its application's information from (each kind its own), in
 * promote_source.cpp (constructor 0x8107A6F0, vtable 0x8152EEB0), 88 bytes. The names are ours.
 */
class GamePromoteSource : public GameInfoSource {
public:
	GamePromoteSource(const GameAppSourceParam &param);
	virtual ~GamePromoteSource();

	/* the application's information set into reg (0x8107A83C) */
	virtual int SetInfo(GameAppRegistration *reg);

	/* the information read into reg */
	virtual int Load(GameAppRegistration *reg) = 0;

private:
	void Init(const GameAppSourceParam &param);

public:
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
 * The source of an application folder with its sce_sys: its param.sfo and those of its related
 * applications (param2.sfo), in folder_source.cpp (constructor 0x8107A90A, vtable 0x8152EEC8),
 * 172 bytes. The names are ours.
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

	/* a related application's attributes from its installed record app (0x8107B990) */
	virtual int SetRelatedAttributes(unsigned int index, const LsdbAppInfoPtr *app,
					 LsdbAppInfoPtr *related);

	/* a related application's title ID, paths, type and promotion state (0x8107BA88) */
	virtual int SetRelatedPaths(unsigned int index, LsdbAppInfoPtr *related);

	/* a copy of the param.sfo's digest kept, to load it verified (0x8107A8C0) */
	int SetDigest(const void *digest, SceSize size);

private:
	int CopyRelated(unsigned int index, const LsdbAppInfoPtr *info, LsdbAppInfoPtr *related);
	int LoadSfo(const paf::string &path, LsdbAppInfoPtr *info);
	int LoadMain(GameAppRegistration *reg);
	int LoadRelated(const paf::string &path, unsigned int index, LsdbAppInfoPtr *related);

public:
	paf::string m_sysDir;		/* the folder's sce_sys */
	paf::string m_metaDir;		/* the LiveArea's folder */
	paf::string m_appDir;		/* the application's folder, "" here */
	paf::string m_appPath;		/* the folder promoted */
	paf::string m_category;		/* the param.sfo's CATEGORY */
	SceUInt32 m_upStatus;		/* the record's #_up_status, 0 from the constructor */
	LsdbAppInfoPtr m_oldInfo;	/* the record the application had */
	bool m_installed;		/* the param.sfo is loaded verified against m_digest */
	void *m_digest;			/* the param.sfo's digest given (SetDigest), NULL from the constructor */
	SceSize m_digestSize;
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
 */

class GameInstallSource;

/* the stages of an install, for its progress */
#define GAME_INSTALL_STAGES	6

/*
 * PLACEHOLDER: the installers' base for a package of GameInstallParam, defined in another file of
 * the plugin (constructor 0x81095EB8, destructor 0x8109610C, vtable 0x8152F0B8), 128 bytes: like
 * GamePackageInstaller's, the package installed by its source, its record built and registered,
 * by stages. The names are ours.
 */
class GameInstaller : public GamePromoterIf {
public:
	GameInstaller(const GameInstallParam *param, GameJobProgress *progress,
		      paf::string *errorTitleId, float scale);
	virtual ~GameInstaller();

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	/* the installed application's record read, and whether it has one (0x8109675E) */
	virtual int Prepare(bool flag);

	/* the source of the package */
	virtual GameInstallSource *CreateSource(void)
	{
		return NULL;
	}

	/* once the source has installed the package */
	virtual int Installed(void)
	{
		return 0;
	}

	/* what is undone after a failed or finished install (0x810967A4) */
	virtual int Cleanup(void);

	/* what is left once the record is built */
	virtual int Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg)
	{
		return 0;
	}

	/* the record built from the source (0x810967B8) */
	virtual int SetInfo(bool registerApp, GameAppRegistration *reg);

	const GameInstallParam *m_param;
	LsdbAppInfoPtr m_appInfo;		/* the installed application's record (Prepare) */
	GameInstallSource *m_source;		/* CreateSource's */
	GamePromoteSource *m_infoSource;	/* the source SetInfo reads */
	GameJobProgress m_progress;
	float m_weights[GAME_INSTALL_STAGES];	/* each stage's part of the progress */
	bool m_installed;			/* the application has a record (Prepare) */
	bool m_prepareFlag;			/* Prepare's */
	bool m_otherFlag;
	paf::string m_titleId;			/* the parameters' */
	paf::string m_appmetaDir;		/* the package's appmeta folder */
	paf::string m_dir;			/* ur0:appmeta/<title ID> */
	paf::string m_licenseDir;		/* the package's license folder, when it is mounted */
	paf::string *m_errorTitleId;		/* the title ID an error names */
	bool m_freeTrial;			/* installed with its free trial license only */
	bool m_versionChanged;			/* the record's version is not the installed one's */
};

typedef char GameInstallerSize[sizeof(GameInstaller) == 128 ? 1 : -1];

/*
 * The installer of an application's package: its app data and game data mounted, its
 * gc_param.sfo read and its record built from its ux0:app or gro0:app folder
 * (app_installer.cpp, constructor 0x81073E90, destructor 0x81073ED0, vtable 0x8152EDE4), 160
 * bytes. The names are ours.
 */
class GameAppInstaller : public GameInstaller {				/* kind 0 */
public:
	GameAppInstaller(const GameInstallParam *param, GameJobProgress *progress,
			 paf::string *errorTitleId, float scale = 1.0f);
	virtual ~GameAppInstaller();

	virtual GameInstallSource *CreateSource(void);
	virtual int Installed(void);
	virtual int Cleanup(void);
	virtual int Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg);
	virtual int SetInfo(bool registerApp, GameAppRegistration *reg);

	/* the install information of the package's app folder (0x81073FB4) */
	int ReadAppInfo(SceNpDrmInstallInfo *info);

	/* whether the license folder holds the free trial license (0x810741F0) */
	bool HasFttLicense(const paf::string *licenseDir);

	LsdbAppInfoPtr m_sfoInfo;		/* the package's gc_param.sfo */
	paf::string m_gameMountPoint;		/* the application's game data, when it is mounted */
	paf::string m_appDataMountPoint;	/* the new application's app data */
};

typedef char GameAppInstallerSize[sizeof(GameAppInstaller) == 160 ? 1 : -1];

/*
 * The promoter of kind 3, GamePsmAppInstaller, follows GamePsmAppReader.
 *
 * The promoter of a license's package: the installed application's record taken again with the
 * new license's up status, its LiveArea copied again from the game data of a free trial that the
 * license makes whole (license_installer.cpp, vtable 0x8152EE18). The names are ours.
 */
class GameLicenseInstaller : public GameInstaller {			/* kind 2 */
public:
	GameLicenseInstaller(const GameInstallParam *param, GameJobProgress *progress,
			     paf::string *errorTitleId, float scale = 1.0f);
	virtual ~GameLicenseInstaller();

	virtual int Prepare(bool flag);
	virtual GameInstallSource *CreateSource(void);
	virtual int Installed(void);
	virtual int Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg);
	virtual int SetInfo(bool registerApp, GameAppRegistration *reg);

	/* the free trial's LiveArea copied from the application's game data (0x810769C4) */
	int CopyLiveArea(GameAppRegistration *reg, GameCancelFunc cancel, void *arg);

	bool m_noLicense;			/* the parameters' */
};

typedef char GameLicenseInstallerSize[sizeof(GameLicenseInstaller) == 128 ? 1 : -1];

/* The promoter of kind 1, GamePspAppInstaller, follows GamePspPbootInstaller. */

/* the kinds of GameInstallParam */
#define GAME_INSTALL_KIND_APP		0
#define GAME_INSTALL_KIND_PSP			1
#define GAME_INSTALL_KIND_LICENSE	2
#define GAME_INSTALL_KIND_PSM		3

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
 * information (0x8104639C..0x81046D04). The installers of each content type follow
 * GamePackageInstaller below.
 */

/*
 * The check of an application's update package against the installed application, in
 * gd_update_check.cpp (constructor 0x8107DA08, vtable 0x8152EF18), 36 bytes. The names are ours.
 */
class GameGdUpdateCheck {
public:
	GameGdUpdateCheck(const paf::string &titleId, const paf::string &path, bool notImported,
			  paf::string *errorTitleId);
	virtual ~GameGdUpdateCheck();

	/* the check against the installed application's record (0x8107DD86) */
	int Check(const LsdbAppInfoPtr *info);

private:
	int CheckStorage(int storage, const LsdbAppInfoPtr *info);

public:
	paf::string m_titleId;		/* the application's title ID */
	paf::string m_path;		/* the update's folder */
	bool m_notImported;		/* passed to gameAppInfoMoveToUx0 */
	paf::string *m_errorTitleId;	/* where an error's detail goes */
};

typedef char GameGdUpdateCheckSize[sizeof(GameGdUpdateCheck) == 36 ? 1 : -1];

/*
 * The appmeta of a game's update package (gd_update_appmeta_writer.cpp, constructor 0x8107DF50,
 * vtable 0x8152EF28), 48 bytes: an additional content's (gp, gpc) parameters, EBOOTs' parameters
 * and images made as "new" files in its application data and renamed over the old ones. The
 * names are ours.
 */
class GameGdUpdateAppmetaWriter : public GameAppmetaWriterIf {
public:
	GameGdUpdateAppmetaWriter(GameCancelFunc cancel, void *arg, const GameJobProgress *progress,
				  paf::string *detail, float scale);
	virtual ~GameGdUpdateAppmetaWriter();

	virtual int Run(GameAppRegistration *reg, const paf::string *dir);

private:
	int CopyToNew(const paf::string *dir, const paf::string *titleId, unsigned int kind,
		      bool gcParam, bool onUx0);
	int Copy(const LsdbAppInfoPtr *info, const paf::string *dir);

	GameCancelFunc m_cancel;
	void *m_arg;
	GameJobProgress m_progress;
	float m_weights[3];		/* each step's share of m_progress */
	paf::string *m_detail;		/* where a failed copy's drive goes */
};

typedef char GameGdUpdateAppmetaWriterSize[sizeof(GameGdUpdateAppmetaWriter) == 48 ? 1 : -1];

/*
 * PLACEHOLDER: the installers' base, defined in another file of the plugin (constructor
 * 0x81095248, destructor 0x8109541A, vtable 0x8152F090), 124 bytes: the package folder's source
 * made, its record built and registered, by stages. The names are ours.
 */
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
	GamePromoteSource *m_infoSource;	/* the source SetInfo reads */
	GameJobProgress m_progress;
	float m_weights[GAME_INSTALL_STAGES];	/* each stage's part of the progress */
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

/* a source of an installer: its package installed, then completed (vtable 0x8152EF58) */
class GameInstallSource {
public:
	virtual ~GameInstallSource()
	{
	}

	/*
	 * The package folder at path installed as titleId's: the folder and path it is installed
	 * to in *dir and *installPath, with scale of progress (GAME_ERROR_GC_QUIET: nothing to
	 * install); the names are ours
	 */
	virtual int Install(const paf::string *path, const paf::string *titleId, paf::string *dir,
			    paf::string *installPath, GameJobProgress *progress, int reserved1,
			    int reserved2, float scale) = 0;

	/* the install of dir and installPath completed (the name is ours) */
	virtual int Complete(paf::string *dir, paf::string *installPath, int reserved1,
			     int reserved2, int reserved3, float scale) = 0;
};

/*
 * The source of an application's package (app_install_source.cpp, 0x81081F28..0x81084C9E,
 * vtable 0x8152EF70), 40 bytes: its appmeta moved to ux0:appmeta/new and its application data
 * mounted; once installed, the application's old app, patch, additional content and license
 * folders, saved data and appmeta moved aside, the package's moved into place, and its appmeta
 * renamed to the application's. The names are ours.
 */
class GameAppInstallSource : public GameInstallSource {
public:
	GameAppInstallSource(const GameInstallParam *param, paf::string *errorTitleId);
	virtual ~GameAppInstallSource();

	virtual int Install(const paf::string *path, const paf::string *titleId, paf::string *dir,
			    paf::string *installPath, GameJobProgress *progress, int reserved1,
			    int reserved2, float scale);
	virtual int Complete(paf::string *dir, paf::string *installPath, int reserved1,
			     int reserved2, int reserved3, float scale);

	/* the package installed with its free trial license only (0x81084C98) */
	void SetFreeTrial(bool freeTrial);

private:
	int ListDirs(const paf::string &path, paf::vector<paf::string> *names);
	int MoveNew(const paf::string &path, const paf::string &titleId,
		    const paf::string &userDir, paf::string *detail);
	int MoveOld(const paf::string &oldDir, const paf::string &uxOldDir,
		    const paf::string &path, const paf::string &titleId,
		    const paf::string &userDir, paf::string *detail);

	const GameInstallParam *m_param;
	paf::string m_mountPoint;		/* the application data mounted while installing */
	paf::string m_oldDir;			/* where the old ur0: folders are moved */
	paf::string *m_errorTitleId;		/* where a failed step's drive goes */
	bool m_freeTrial;
	bool m_hasFttLicense;			/* the old license folder has the free trial license */
};

typedef char GameAppInstallSourceSize[sizeof(GameAppInstallSource) == 40 ? 1 : -1];

/*
 * The source of a license's package (license_install_source.cpp, 0x81084D10..0x8108521C, vtable
 * 0x8152EF90), 36 bytes: its license read from the package (none for a package without one);
 * once installed, the license written to ux0:license/app/<title ID>, or the free trial license
 * of a package without one installed, and the package removed. The names are ours.
 */
class GameLicenseInstallSource : public GameInstallSource {
public:
	GameLicenseInstallSource(const GameInstallParam *param, bool noLicense,
				 paf::string *errorTitleId);
	virtual ~GameLicenseInstallSource();

	virtual int Install(const paf::string *path, const paf::string *titleId, paf::string *dir,
			    paf::string *installPath, GameJobProgress *progress, int reserved1,
			    int reserved2, float scale);
	virtual int Complete(paf::string *dir, paf::string *installPath, int reserved1,
			     int reserved2, int reserved3, float scale);

	const GameInstallParam *m_param;
	bool m_noLicense;			/* a package of a free trial, without its license */
	paf::string m_reserved;			/* constructed and destroyed only */
	SceNpDrmLicense *m_license;		/* the package's license, read by Install */
	int m_licenseSize;
	paf::string *m_errorTitleId;		/* where a failed step's drive goes */
};

typedef char GameLicenseInstallSourceSize[sizeof(GameLicenseInstallSource) == 36 ? 1 : -1];

/*
 * The source of a PlayStation Mobile application's package (psm_app_install_source.cpp,
 * 0x8108521C..0x81085DF0, vtable 0x8152EFA0), 12 bytes: installed to ux0:psm/<title ID> with its
 * license. The names are ours.
 */
class GamePsmAppInstallSource : public GameInstallSource {
public:
	GamePsmAppInstallSource(const GameInstallParam *param, paf::string *errorTitleId);
	virtual ~GamePsmAppInstallSource();

	virtual int Install(const paf::string *path, const paf::string *titleId, paf::string *dir,
			    paf::string *installPath, GameJobProgress *progress, int reserved1,
			    int reserved2, float scale);
	virtual int Complete(paf::string *dir, paf::string *installPath, int reserved1,
			     int reserved2, int reserved3, float scale);

private:
	int WriteLicense(const paf::string *path, const paf::string *dir, paf::string *licensePath,
			 paf::string *detail);

	const GameInstallParam *m_param;
	paf::string *m_errorTitleId;		/* where a failed step's drive goes */
};

typedef char GamePsmAppInstallSourceSize[sizeof(GamePsmAppInstallSource) == 12 ? 1 : -1];

/*
 * The source of an additional content's package folder (addcont_source.cpp, constructor
 * 0x810875F0, vtable 0x8152EFD8), 64 bytes: the package moved or copied to <drive>temp/promote,
 * its content put in <drive>addcont/<title ID>/<entitlement ID> and its license written; a
 * license with a period is noted in addcont/info by Complete. The names are ours.
 */
class GameAddcontSource : public GameInstallSource {
public:
	GameAddcontSource(const char *path, const SceNpDrmInstallInfo *info, LsdbAppInfoPtr *appInfo,
			  paf::string *errorTitleId);
	virtual ~GameAddcontSource();

	virtual int Install(const paf::string *path, const paf::string *titleId, paf::string *dir,
			    paf::string *installPath, GameJobProgress *progress, int cancel,
			    int cancelArg, float scale);
	virtual int Complete(paf::string *dir, paf::string *installPath, int reserved1,
			     int reserved2, int reserved3, float scale);

private:
	int MoveToTemp(const paf::string *from, const paf::string *to, paf::string *detail);
	int MovePackage(const paf::string *path, const paf::string *promoteDir,
			paf::string *detail);
	int CleanupCardPackage(const paf::string *path, int cancel, int cancelArg,
			       GameJobProgress *progress, paf::string *detail);
	int RemovePackage(const paf::string *path, int cancel, int cancelArg,
			  GameJobProgress *progress, paf::string *detail);
	int CopyPackage(const paf::string *path, const paf::string *promoteDir, int cancel,
			int cancelArg, GameJobProgress *progress, paf::string *detail);

	const char *m_path;			/* the package folder, at the head of its GameInstallExParam */
	const SceNpDrmInstallInfo *m_info;
	LsdbAppInfoPtr *m_appInfo;		/* the content's application */
	paf::string *m_errorTitleId;		/* where a failed step's drive goes */
	paf::string m_infoDir;			/* the addcont/info a license with a period goes to */
	paf::string m_contentId;		/* that license's */
	SceUInt64 m_startTime;
	SceUInt64 m_expireTime;
};

typedef char GameAddcontSourceSize[sizeof(GameAddcontSource) == 64 ? 1 : -1];

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
 * The sources of an application's package folder: an update's (gd_update_source.cpp,
 * constructor 0x8108A81C, vtable 0x8152F008, 36 bytes) and an application's (gd_app_source.cpp,
 * constructor 0x8108A0FC, vtable 0x8152EFF0, 40 bytes).
 */
class GameGdUpdateSource : public GameInstallSource {
public:
	GameGdUpdateSource(const SceNpDrmInstallInfo *info, const LsdbAppInfoPtr *appInfo,
			   paf::string *errorTitleId);
	virtual ~GameGdUpdateSource();

	virtual int Install(const paf::string *path, const paf::string *titleId, paf::string *dir,
			    paf::string *installPath, GameJobProgress *progress, int cancel,
			    int cancelArg, float scale);
	virtual int Complete(paf::string *dir, paf::string *installPath, int reserved1,
			     int reserved2, int reserved3, float scale);

private:
	int MergePatch(const paf::string *appDir, const paf::string *patchDir,
		       const paf::string *path, paf::string *dir);
	int MovePatch(const paf::string *appDir, const paf::string *patchDir,
		      const paf::string *path, const paf::string &promoteDir, int kind,
		      int cancel, int cancelArg);
	int CopyCardPatch(const paf::string *appDir, const paf::string *patchDir,
			  const paf::string *path, const paf::string &promoteDir, int kind,
			  GameJobProgress *progress, int cancel, int cancelArg, float scale);

	const SceNpDrmInstallInfo *m_info;
	LsdbAppInfoPtr m_appInfo;		/* the application's record */
	int m_mode;				/* PLACEHOLDER: 1 from the constructor, read by nothing here */
	paf::string m_installDir;		/* the folder Install leaves for the install */
	paf::string *m_errorTitleId;		/* where a failed step's drive goes */
};

typedef char GameGdUpdateSourceSize[sizeof(GameGdUpdateSource) == 36 ? 1 : -1];

class GameGdAppSource : public GameInstallSource {
public:
	GameGdAppSource(const SceNpDrmInstallInfo *info, bool noLicense, const paf::string &mountPoint,
			paf::string *errorTitleId);
	virtual ~GameGdAppSource();

	virtual int Install(const paf::string *path, const paf::string *titleId, paf::string *dir,
			    paf::string *installPath, GameJobProgress *progress, int reserved1,
			    int reserved2, float scale);
	virtual int Complete(paf::string *dir, paf::string *installPath, int reserved1,
			     int reserved2, int reserved3, float scale);

private:
	const SceNpDrmInstallInfo *m_info;
	bool m_noLicense;			/* a free application: its FTT installed, no license */
	paf::string m_mountPoint;		/* what the FTT is installed from */
	paf::string *m_errorTitleId;		/* where a failed step's drive goes */
	paf::string m_titleId;			/* the application's, kept by Install */
};

typedef char GameGdAppSourceSize[sizeof(GameGdAppSource) == 40 ? 1 : -1];

/*
 * The patch at patchPath checked against the application at appPath and the patch it applies on
 * at basePath (gd_patch_check.cpp, 0x8108A680): 1 or 2, step the package read last.
 */
int gamePatchCheck(const char *appPath, const char *basePath, const char *patchPath,
		   int reserved, void *buf, SceSize bufSize, int *step);

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
	virtual int SetOtherInfo(bool registerApp, GameAppRegistration *reg, paf::string *dir,
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

	/*
	 * The application in dir mounted at *mountPoint: an application's with its patch on the
	 * drive of path, an update's over the installed application (0x8106D90C); Installed passes
	 * &m_patchStorage as patchStorage, which is not read (m_patchStorage is set directly)
	 */
	int MountApp(const paf::string *dir, const paf::string *path, paf::string *mountPoint,
		     int *patchStorage);

	/* GAME_ERROR_BAD_DATA when bit 0 of the two records' ATTRIBUTE differs (0x8106D832) */
	int CheckAttribute(const LsdbAppInfoPtr *installed, const LsdbAppInfoPtr *update);

	LsdbAppInfoPtr m_appInfo;		/* the installed application's record */
	paf::string m_mountPoint;		/* the application's, unmounted unless m_noLicense */
	int m_patchStorage;			/* where an update's application is installed */
	bool m_install;				/* cleared when the update is already in place */
	int m_badAttribute;			/* set when the update's ATTRIBUTE does not fit */
};

typedef char GameGdInstallerSize[sizeof(GameGdInstaller) == 156 ? 1 : -1];

/* the content types an install information names (values from the switch at 0x810468E2) */
#define GAME_CONTENT_TYPE_17		17	/* PLACEHOLDER name: a PSP game's saved data (ux0:pspemu/PSP/SAVEDATA) */
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
 * The source of an application promoted from its appmeta folder: the application's own folder
 * under it, in appmeta_source.cpp (constructor 0x8107C0DC, vtable 0x8152EEF0), 188 bytes. The
 * names are ours.
 */
class GameAppmetaSource : public GameFolderSource {
public:
	GameAppmetaSource(const GameFolderSourceParam &param);
	virtual ~GameAppmetaSource();

	virtual int SetDirs(GameAppRegistration *reg);
	virtual int SetPaths(GameAppRegistration *reg);
	virtual int SetRelatedPaths(unsigned int index, LsdbAppInfoPtr *related);

private:
	void Init(const GameFolderSourceParam &param);

public:
	bool m_hasAppDir;		/* m_dir's app/<title ID> folder is there */
	bool m_reserved;		/* false from the constructor; not read here */
	paf::string m_dataDir;		/* the folder the application's pic0.png is looked for in */
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

	/* the reader of the application name (0x81094D20) */
	GamePsmAppReader(const paf::string &name, int userData);

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

	/* the app.info and *source deleted (0x8109502E) */
	int Cleanup(GamePromoteSource **source);

	GamePsmAppInfo *m_appInfo;
	paf::string m_path;
	paf::string m_name;		/* the application's folder name */
	int m_userData;
};

typedef char GamePsmAppReaderSize[sizeof(GamePsmAppReader) == 36 ? 1 : -1];

/*
 * The promoter of a PlayStation Mobile application's package: its app.info read and its folder
 * promoted into ux0:psm by a GamePsmAppReader (psm_app_installer.cpp, vtable 0x8152EE44). The
 * names are ours.
 */
class GamePsmAppInstaller : public GameInstaller {			/* kind 3 */
public:
	GamePsmAppInstaller(const GameInstallParam *param, GameJobProgress *progress,
			    paf::string *errorTitleId, float scale = 1.0f);
	virtual ~GamePsmAppInstaller();

	virtual int Prepare(bool flag);
	virtual GameInstallSource *CreateSource(void);
	virtual int Installed(void);
	virtual int Cleanup(void);
	virtual int Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg);
	virtual int SetInfo(bool registerApp, GameAppRegistration *reg);

	GamePsmAppReader m_reader;
	LsdbAppInfoPtr m_info;			/* the package's app.info */
};

typedef char GamePsmAppInstallerSize[sizeof(GamePsmAppInstaller) == 172 ? 1 : -1];

/*
 * The source of a PlayStation Mobile game's package folder (psm_install_source.cpp, constructor
 * 0x8108CD5C, vtable 0x8152F020), 44 bytes: the package's RO and RW folders built in
 * ux0:temp/promote/<title ID> and moved to ux0:psm/<title ID>, the old game's folders, save data
 * and license carried over from ux0:temp/promote/old. The names are ours.
 */
class GamePsmInstallSource : public GameInstallSource {
public:
	GamePsmInstallSource(const SceNpDrmInstallInfo *info, const LsdbAppInfoPtr *appInfo,
			     paf::string *errorTitleId);
	virtual ~GamePsmInstallSource();

	virtual int Install(const paf::string *path, const paf::string *titleId, paf::string *dir,
			    paf::string *installPath, GameJobProgress *progress, int reserved1,
			    int reserved2, float scale);
	virtual int Complete(paf::string *dir, paf::string *installPath, int reserved1,
			     int reserved2, int reserved3, float scale);

private:
	int WriteLicense(const paf::string *path, const paf::string *dir, paf::string *licensePath,
			 paf::string *detail, bool npDrm);

	const SceNpDrmInstallInfo *m_info;
	LsdbAppInfoPtr m_appInfo;		/* the installed game's record */
	paf::string m_pkgPath;			/* the package folder Install was given */
	paf::string m_titleId;			/* the game's, kept by Install */
	paf::string *m_errorTitleId;		/* where a failed step's drive goes */
};

typedef char GamePsmInstallSourceSize[sizeof(GamePsmInstallSource) == 44 ? 1 : -1];

/*
 * A PlayStation Mobile game installed (psm_installer.cpp, constructor 0x810713C0, vtable
 * 0x8152ED28), 184 bytes: its app.info read by a GamePsmAppReader, like its promotion's.
 */
class GamePsmInstaller : public GamePackageInstaller {
public:
	GamePsmInstaller(const char *path, const SceNpDrmInstallInfo *info,
	                 GameJobProgress *progress, paf::string *errorTitleId, float scale = 1.0f);
	virtual ~GamePsmInstaller();

	virtual int Prepare(bool flag);
	virtual GameInstallSource *CreateSource(int *ret);
	virtual int Installed(paf::string *dir, paf::string *path);
	virtual int Cleanup(void);
	virtual int SetInfo(bool registerApp, GameAppRegistration *reg);
	virtual int Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg);

	GamePsmAppReader m_reader;
	LsdbAppInfoPtr m_appInfo;		/* the app.info's record */
	int m_reserved;				/* zeroed, not read */
	paf::string m_reservedPath;		/* not read */
};

typedef char GamePsmInstallerSize[sizeof(GamePsmInstaller) == 184 ? 1 : -1];

/*
 * The source of a PSP game's package folder (psp_install_source.cpp, constructor 0x8108ED68,
 * vtable 0x8152F038), 72 bytes: the installed game (or PocketStation application) and its game
 * data folder moved aside to ux0:pspemu/temp/promote/pspemu (or ps1emu) with the new one, then
 * moved back together by Complete, and the license written to ux0:pspemu/PSP/LICENSE. The names
 * are ours.
 */
class GamePspInstallSource : public GameInstallSource {
public:
	GamePspInstallSource(const SceNpDrmInstallInfo *info, const paf::string &titleId,
			     paf::string *errorTitleId);
	virtual ~GamePspInstallSource();

	virtual int Install(const paf::string *path, const paf::string *titleId, paf::string *dir,
			    paf::string *installPath, GameJobProgress *progress, int reserved1,
			    int reserved2, float scale);
	virtual int Complete(paf::string *dir, paf::string *installPath, int reserved1,
			     int reserved2, int reserved3, float scale);

private:
	static int MoveMgGame(const paf::string *path, const paf::string &titleId,
			      const paf::string &promoteDir, const LsdbAppInfoPtr *sfo,
			      paf::vector<paf::string> *moved, paf::string *detail);
	static int MoveEgMeGame(const paf::string *path, const paf::string &titleId,
				const paf::string &promoteDir, const LsdbAppInfoPtr *sfo,
				paf::vector<paf::string> *moved, paf::string *detail);
	int MovePocketStation(const paf::string *path, const paf::string &titleId,
			      const paf::string &promoteDir, paf::vector<paf::string> *moved,
			      paf::string *detail);
	int MovePackage(const paf::string *path, const SceNpDrmInstallInfo *info,
			paf::vector<paf::string> *moved);

public:
	const SceNpDrmInstallInfo *m_info;
	LsdbAppInfoPtr m_appInfo;		/* empty, not read */
	paf::vector<paf::string> m_moved;	/* the folders moved aside, under Install's dir */
	const paf::string &m_titleId;		/* not read */
	paf::string m_workDir;			/* the pspemu work folder mounted by Install */
	bool m_ebootSig;			/* the installed EBOOT.PBP's signature to be made */
	paf::string *m_errorTitleId;		/* where a failed step's drive goes */
	paf::string m_addcontInfoDir;		/* where a license's period is noted, or empty */
	bool m_pocketStation;			/* a PocketStation application, in ux0:ps1emu */
};

typedef char GamePspInstallSourceSize[sizeof(GamePspInstallSource) == 72 ? 1 : -1];

/* the steps of the saved data copier's progress */
#define GAME_PS1_SAVEDATA_STEPS	8

/*
 * The install of a PlayStation game's memory cards into ux0:pspemu/PSP/SAVEDATA/<title ID>
 * (ps1_savedata_copier.cpp, 0x81080444..0x81081F28, vtable 0x8152EF48), 4580 bytes: two empty
 * memory card images, their param.sfo with its MACs and the game's ICON0.PNG, made in a new
 * folder renamed into place at the end. The name is ours.
 */
class GamePs1SavedataCopier {
public:
	GamePs1SavedataCopier(GameCancelFunc cancel, void *arg, const GameJobProgress *progress,
			      paf::string *detail, float scale);
	virtual ~GamePs1SavedataCopier();

	/* the saved data of reg's game, its icon from its EBOOT.PBP at pbpPath (0x81081000) */
	int Run(const paf::string *pbpPath, GameAppRegistration *reg);

private:
	int CopyIcon(const paf::string *pbpPath, const paf::string *iconPath,
		     GameJobProgress *progress);
	int SetMacs(SfoHandle *sfo, void *sfoBuf, SceSize sfoSize, void *params);
	int AddParams(SfoHandle *sfo, const paf::string *dirName, const paf::string *title,
		      int parentalLevel, void *params);
	int WriteParam(const paf::string *dir, const paf::string *dirName, const paf::string *title,
		       int parentalLevel);
	int MakeCard(void *card);
	int MakeNewDir(const paf::string *parent, const char *prefix, paf::string *name);

	GameCancelFunc m_cancel;
	void *m_arg;
	GameJobProgress m_progress;
	float m_weights[GAME_PS1_SAVEDATA_STEPS];	/* each step's share of m_progress */
	paf::string *m_detail;			/* where a failed step's drive goes */

	/* the saved data's param.sfo texts and file list, as the PSP keeps them */
	struct {
		char savedataDetail[1024];
		char savedataDirectory[64];
		SceUInt8 savedataFileList[3168];
		char savedataTitle[128];
		char title[128];
	} m_param;
};

typedef char GamePs1SavedataCopierSize[sizeof(GamePs1SavedataCopier) == 4580 ? 1 : -1];

/*
 * A PSP or PlayStation game installed without PBOOT.PBP (psp_installer.cpp, constructor
 * 0x81071890, vtable 0x8152ED58), 156 bytes: its record read and the game data checked not in
 * use, its folder moved into ux0:pspemu/PSP/GAME, its appmeta written and its EBOOT.PBP signed.
 */
class GamePspInstaller : public GamePackageInstaller {
public:
	GamePspInstaller(const char *path, const SceNpDrmInstallInfo *info,
	                 GameJobProgress *progress, paf::string *errorTitleId, float scale = 1.0f);
	virtual ~GamePspInstaller();

	virtual int Prepare(bool flag);
	virtual GameInstallSource *CreateSource(int *ret);
	virtual int Installed(paf::string *dir, paf::string *path);
	virtual int Cleanup(void);
	virtual int SetInfo(bool registerApp, GameAppRegistration *reg);
	virtual int Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg);

	LsdbAppInfoPtr m_appInfo;		/* the game's record */
	paf::string m_paramPath;		/* the source's PBP the parameters were read from */
	paf::string m_gameDir;			/* the game's installed folder */
};

typedef char GamePspInstallerSize[sizeof(GamePspInstaller) == 156 ? 1 : -1];

/*
 * The source of a PSP game's update package with PBOOT.PBP (pboot_install_source.cpp, constructor
 * 0x8109367C, vtable 0x8152F050), 40 bytes: the games in titleIds moved to
 * ux0:pspemu/temp/promote/pspemu/PSP/GAME, the package's content moved into the game's folder
 * there, then all of them moved back to ux0:pspemu/PSP/GAME by Complete. The names are ours.
 */
class GamePspPbootInstallSource : public GameInstallSource {
public:
	GamePspPbootInstallSource(const SceNpDrmInstallInfo *info, const paf::string &titleId,
				  const paf::vector<paf::string> &titleIds, paf::string *errorTitleId);
	virtual ~GamePspPbootInstallSource();

	virtual int Install(const paf::string *path, const paf::string *titleId, paf::string *dir,
			    paf::string *installPath, GameJobProgress *progress, int reserved1,
			    int reserved2, float scale);
	virtual int Complete(paf::string *dir, paf::string *installPath, int reserved1,
			     int reserved2, int reserved3, float scale);

private:
	int MoveContent(const paf::string *content, const paf::string *gameDir,
			const paf::string *titleId, paf::string *detail);

	const SceNpDrmInstallInfo *m_info;
	LsdbAppInfoPtr m_appInfo;			/* empty, not read */
	const paf::string &m_titleId;			/* not read */
	paf::string m_workDir;				/* the pspemu work folder mounted by Install */
	const paf::vector<paf::string> &m_titleIds;	/* the games the update moves */
	paf::string *m_errorTitleId;			/* where a failed step's drive goes */
};

typedef char GamePspPbootInstallSourceSize[sizeof(GamePspPbootInstallSource) == 40 ? 1 : -1];

/* a game's promotion by GamePspPbootInstaller::Run, until the records are updated */
struct GamePspPbootTitle {
	GamePspPbootTitle(const paf::string &titleId) : titleId(titleId)
	{
	}

	paf::string titleId;
	LsdbAppInfoPtr info;			/* its record, as registered */
	bool replace;				/* its promoter's m_param.replace */
	bool notifyTopmenu;			/* its promoter's m_notifyTopmenu */
	int ret;				/* its promotion's result */
};

typedef char GamePspPbootTitleSize[sizeof(GamePspPbootTitle) == 28 ? 1 : -1];

/*
 * A PSP game installed with PBOOT.PBP (psp_pboot_installer.cpp, constructor 0x810729E4, vtable
 * 0x8152ED88), 152 bytes: the package installed by its source, then each game sharing its game
 * data promoted from its folder in ux0:pspemu/PSP/GAME and its record updated.
 */
class GamePspPbootInstaller : public GamePackageInstaller {
public:
	GamePspPbootInstaller(const char *path, const SceNpDrmInstallInfo *info,
			      GameJobProgress *progress, paf::string *errorTitleId,
			      float scale = 1.0f);
	virtual ~GamePspPbootInstaller();

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);
	virtual int Prepare(bool flag);
	virtual GameInstallSource *CreateSource(int *ret);

	/* one game's folder promoted into reg (0x81072AEA) */
	int PromoteTitle(const paf::string *gameDir, const paf::string *installPath,
			 GameAppRegistration *reg, bool registerApp, bool *replace,
			 bool *notifyTopmenu, GameCancelFunc cancel, void *arg, float scale);

	/* the games sharing titleId's game data, with titleId when it is installed (0x8107381C) */
	int FindTitles(const paf::string &titleId, paf::vector<paf::string> *titleIds);

	paf::string m_reservedPath;		/* never set */
	paf::vector<paf::string> m_titleIds;	/* FindTitles's */
};

typedef char GamePspPbootInstallerSize[sizeof(GamePspPbootInstaller) == 152 ? 1 : -1];

/*
 * The promoter of a PSP game's package: the package installed by its source, then every game that
 * shares its game data promoted again from its folder in ux0:pspemu/PSP/GAME and its record
 * updated; with none, its old appmeta moved aside and its record unregistered
 * (psp_app_installer.cpp, vtable 0x8152EE70). The names are ours.
 */
class GamePspAppInstaller : public GameInstaller {			/* kind 1 */
public:
	GamePspAppInstaller(const GameInstallParam *param, GameJobProgress *progress,
			    paf::string *errorTitleId, float scale = 1.0f);
	virtual ~GamePspAppInstaller();

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);
	virtual int Prepare(bool flag);
	virtual GameInstallSource *CreateSource(void);

	/* one game's folder promoted into reg (0x81078442) */
	int PromoteTitle(const paf::string *gameDir, const paf::string *installPath,
			 GameAppRegistration *reg, bool registerApp, bool *replace,
			 bool *notifyTopmenu, GameCancelFunc cancel, void *arg, float scale);

	/* a PBOOT.PBP package: the games whose GAMEDATA_ID is its title ID (0x81079432) */
	int PreparePboot(void);

	/*
	 * an EBOOT.PBP package: its game data ID from its param.sfo, the games sharing it with the
	 * package's own title, and an EG or ME game's EBOOT.PBP signature checked (0x81079474)
	 */
	int PrepareEboot(void);

	paf::vector<paf::string> m_titleIds;	/* the games to promote */
	paf::string m_gameDataId;		/* the package's game data ID, when games share it */
};

typedef char GamePspAppInstallerSize[sizeof(GamePspAppInstaller) == 156 ? 1 : -1];

/*
 * The source of a PSP game's package (psp_app_install_source.cpp, constructor 0x81085DF0): the
 * pspemu work folder mounted while it installs, the games' old folders under ux0:pspemu moved to
 * ux0:pspemu/temp/promote, and the package's moved into place. The names are ours.
 */
class GamePspAppInstallSource : public GameInstallSource {
public:
	virtual int Install(const paf::string *path, const paf::string *titleId, paf::string *dir,
			    paf::string *installPath, GameJobProgress *progress, int reserved1,
			    int reserved2, float scale);
	virtual int Complete(paf::string *dir, paf::string *installPath, int reserved1,
			     int reserved2, int reserved3, float scale);
	GamePspAppInstallSource(const GameInstallParam *param,
				const paf::vector<paf::string> *titleIds,
				const paf::string *gameDataId, paf::string *errorTitleId);
	virtual ~GamePspAppInstallSource();

private:
	int MoveOld(const paf::string *path, const paf::string &fromDir,
		    const paf::string &toDir, int type, paf::string *detail);
	int MoveNew(const paf::string &fromDir, const paf::string &toDir, int type,
		    paf::string *detail);

	const GameInstallParam *m_param;
	paf::string m_mountPoint;			/* the pspemu work folder's */
	const paf::vector<paf::string> *m_titleIds;	/* the games sharing the game data */
	const paf::string *m_gameDataId;
	paf::string *m_errorTitleId;
};

typedef char GamePspAppInstallSourceSize[sizeof(GamePspAppInstallSource) == 32 ? 1 : -1];

/*
 * The source of a package of content type 17 (type17_install_source.cpp, constructor 0x810948D8,
 * vtable 0x8152F068), 12 bytes: a PSP game's saved data folder put in ux0:pspemu/PSP/SAVEDATA, the
 * old one moved aside to ux0:pspemu/temp/promote. The names are ours.
 */
class GameType17InstallSource : public GameInstallSource {
public:
	GameType17InstallSource(const SceNpDrmInstallInfo *info, paf::string *errorTitleId);
	virtual ~GameType17InstallSource();

	virtual int Install(const paf::string *path, const paf::string *titleId, paf::string *dir,
			    paf::string *installPath, GameJobProgress *progress, int reserved1,
			    int reserved2, float scale);

	/* nothing to complete: Install put the folder in place */
	virtual int Complete(paf::string *dir, paf::string *installPath, int reserved1,
			     int reserved2, int reserved3, float scale)
	{
		return 0;
	}

private:
	const SceNpDrmInstallInfo *m_info;
	paf::string *m_errorTitleId;		/* where a failed step's drive goes */
};

typedef char GameType17InstallSourceSize[sizeof(GameType17InstallSource) == 12 ? 1 : -1];

/*
 * A package of content type 17 installed (type17_installer.cpp, constructor 0x81073D34, vtable
 * 0x8152EDB8), 124 bytes: the base's install, once the title is not the foreground application.
 */
class GameType17Installer : public GamePackageInstaller {
public:
	GameType17Installer(const char *path, const SceNpDrmInstallInfo *info,
			    GameJobProgress *progress, paf::string *errorTitleId,
			    float scale = 1.0f);
	virtual ~GameType17Installer();

	virtual int Prepare(bool flag);
	virtual GameInstallSource *CreateSource(int *ret);
};

typedef char GameType17InstallerSize[sizeof(GameType17Installer) == 124 ? 1 : -1];

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
