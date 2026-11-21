/*
 * eseutil.hpp: the background download library's helpers after its task manager (0x81110708
 * on, eseutil.cpp): a block's timer, the game plugin and power held while a task runs, the
 * Marlin licence folder and the copies of a licence file. No Sony name is known for the file,
 * its classes or functions (PLACEHOLDER names); Sony's 0.990 prototype names only esehttp.cpp,
 * eseparam.cpp and esetask.cpp in vsh/app/shell/src/esecron/src.
 */
#ifndef ESECRON_ESEUTIL_HPP
#define ESECRON_ESEUTIL_HPP

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <timer/timer.h>
#include <file/localfile.h>

#include "../../../../game/game_plugin/game_plugin.h"
#include "esebase.hpp"
#include "esetask.hpp"
#include "src/livespace_db/livespace_db_internal.h"

struct LsdbQueryCondition;

/* PLACEHOLDER name: game_plugin has no interface to lock ux0:pspemu with */
#define BGDL_ERROR_NO_GAME_PLUGIN	0x80103A28
/* PLACEHOLDER name: the licence file to copy is not there, nor its copy */
#define BGDL_ERROR_NO_LICENSE_FILE	0x80103A35
/* PLACEHOLDER name: the device asked about is not there */
#define BGDL_ERROR_NO_DEVICE		0x80103A05
/* PLACEHOLDER name: a package's files asked of no folder */
#define BGDL_ERROR_NULL_FOLDER		0x80103A31

/* game_plugin's interface functions that hold and release ux0:pspemu (PLACEHOLDER names) */
#define BGDL_GAME_PLUGIN_FUNC_LOCK	6
/* game_plugin's interface function 9: the titles on the game card (PLACEHOLDER) */
#define BGDL_GAME_PLUGIN_FUNC_CARD_TITLES	9
#define BGDL_ERROR_NOT_ON_CARD		0x8010111C	/* PLACEHOLDER name: the title is not on the game card */
#define BGDL_APP_ID_GAME		-3	/* PLACEHOLDER name: the game application's appmgr ID */
#define BGDL_GAME_PLUGIN_FUNC_UNLOCK	7

/* the time a block of code took (PLACEHOLDER names) */
class BgdlScopeTimer {
public:
	BgdlScopeTimer(const char *name) : m_name(name)
	{
		m_start = paf::get_time_internal();
	}

	~BgdlScopeTimer()
	{
		m_end = paf::get_time_internal();
		Report();
	}

	void Report(void);		/* (0x81110708) the time taken, in milliseconds */

	paf::string m_name;
	SceUInt64 m_start;
	SceUInt64 m_end;
	SceUInt64 m_elapsed;
	SceUInt64 m_elapsedMs;
};

/* game_plugin held while ux0:pspemu is changed (PLACEHOLDER names) */
class BgdlGamePluginLock {
public:
	BgdlGamePluginLock(bool lock);	/* (0x8111076A) */
	~BgdlGamePluginLock();		/* (0x811107A0) */
	int Lock(void);			/* (0x8111072E) */

	bool m_locked;
};

/* the power kept on while it lives (PLACEHOLDER names) */
class BgdlPowerLock {
public:
	BgdlPowerLock(bool lock, int arg);	/* (0x811107D0) */
	~BgdlPowerLock();			/* (0x811107F0) */

	bool m_locked;
	int m_arg;			/* PLACEHOLDER: kept, not read */
};

/* the Marlin licence folder (PLACEHOLDER names) */
class BgdlLicenseFolder {
public:
	void Bgdl_81110808(void);	/* in use */
	void Bgdl_8111080E(void);	/* no longer */
	int Bgdl_8111081C(void);	/* ur0:/MLN_ROOT/LICENSE/ made */
	/* (0x8111102A) file copied in as ur0:/MLN_ROOT/LICENSE/<contentId>.MNL */
	int Bgdl_8111102A(const char *file, const char *contentId);
	/* (0x81111432) the content's licence copied to ux0:, and removed from ur0: when remove is set */
	int Bgdl_81111432(const char *contentId, bool remove);

	bool m_active;
};

/* (0x811108C4) src copied to dst; dst removed when the copy fails part way */
int Bgdl_811108C4(const char *src, const char *dst, bool overwrite);
/* (0x81111220) the licence file copied to ux0:/MLN_ROOT/LICENSE/<contentId>.MNL */
int Bgdl_81111220(const char *file, const char *contentId);
extern const char *bgdlLicenseFolder;	/* (0x815447B0) ur0:/MLN_ROOT/LICENSE/ (PLACEHOLDER name) */
/* (0x81111724) a file of size zero bytes made */
int Bgdl_81111724(const char *path, int reserved, SceUInt64 size);
/* (0x81111B96) base with name's extension */
int Bgdl_81111B96(const paf::string *name, const paf::string *base, paf::string *out);
/* (0x81111C2C) the name after the path's last '/' */
bool Bgdl_81111C2C(const paf::string *path, paf::string *name);
/* (0x81111C6A) the name up to its first '.' */
int Bgdl_81111C6A(const char *name, paf::string *base);
/* (0x81111CA2) the name after the path's last '/' */
int Bgdl_81111CA2(const char *path, paf::string *name);
/* (0x81111CDC) src copied into folder under its own name, out the copy's path */
int Bgdl_81111CDC(const char *src, const char *folder, paf::string *out, bool overwrite);
/* (0x81111E5E) the path removed, a folder with everything in it */
int Bgdl_81111E5E(paf::string *path);
/* (0x811120A0) the task's folder removed with everything in it */
int Bgdl_811120A0(BgdlTaskPath *path, bool recursive);
/* (0x81112112) the path removed with everything in it */
int Bgdl_81112112(paf::string *path, bool recursive);
/* (0x8111212C) folder/name removed with everything in it */
int Bgdl_8111212C(const char *folder, const char *name);
/* (0x811121FE) the path's folders made, then the path */
int Bgdl_811121FE(const char *path, int mode);
/* (0x81112284) the path is there (LocalFile::Getstat) */
bool Bgdl_81112284(const char *path);
/* (0x81112310) size in whole clusters */
SceUInt64 Bgdl_81112310(SceUInt64 size, SceUInt32 cluster);
/* (0x8111234C) the device's size and space */
int Bgdl_8111234C(const char *device, BgdlStorageInfo *info);
/* (0x811123BA) the titles a query finds (AppInfoQClient) */
int Bgdl_811123BA(LsdbQueryCondition *conditions, paf::vector<paf::string> *titleIds);
/* (0x8111240E) the file made, or kept, and set to size bytes */
int Bgdl_8111240E(const char *path, SceUInt64 size);
/* (0x81112816) the file set to size bytes; out its status before */
int Bgdl_81112816(const char *path, SceInt64 size, paf::LocalFileStat *out);
/* (0x8111296E) the files of a package, folder/f_<first>_<n>.pkg, of 4 GiB less a byte each */
int Bgdl_8111296E(const char *folder, SceUInt64 size, int first, int *count);
/* (0x81112BD8) a package's parts grown from oldSize to newSize bytes */
int Bgdl_81112BD8(const char *folder, SceUInt64 oldSize, SceUInt64 newSize, int first, int count);
/* (0x81112FAA) the parts of a package of size bytes removed */
int Bgdl_81112FAA(const char *folder, SceUInt64 size, int first);
/* (0x8111322E) file copied to folder/sce_sys/package/work.bin; path that path */
int Bgdl_8111322E(const char *file, const char *folder, paf::string *path);
/* (0x81113336) 1: folder/sce_sys/package/work.bin is there */
int Bgdl_81113336(const char *folder);
int Bgdl_81113400(const char *folder);
/* (0x811137F6) grw0:fgdl removed, and made again when mount (PLACEHOLDER) */
int Bgdl_811137F6(bool mount);
void Bgdl_81113890(int type);	/* sceKernelPowerTick */
/* game_plugin's storage and category kind of a title, and its title id */
int Bgdl_81113898(const LsdbAppInfoPtr *info, int *storage);
int Bgdl_811138CC(const LsdbAppInfoPtr *info, int *kind);
bool Bgdl_81113900(const LsdbAppInfoPtr *info, paf::string *titleId);
int Bgdl_81113984(const char *path);	/* 1: the path is in a download folder */
/* (0x81113C72) a PSP task's folder, ux0:pspemu/bgdl/<id>; made again when make, "" on failure */
paf::string Bgdl_81113C72(SceUInt32 id, bool make);
paf::string Bgdl_81113DE6(const char *titleId);	/* the title's icon (livespace_db) */
int Bgdl_81114092(paf::vector<paf::string> *titles);	/* the game card's titles */
/* 0: the title is on the game card (kind 1; 81114220: kind 1 or 2), else 0 for other kinds */
int Bgdl_81114128(const paf::string *titleId, int kind);
int Bgdl_81114220(const paf::string *titleId, int kind);
int Bgdl_8111431C(paf::string *str, const char *from, const char *to);	/* 1: from replaced by to */
bool Bgdl_811144CE(const paf::string *titleId);	/* the game being run is the title */
GamePluginInterface *Bgdl_811115BA(void);		/* game_plugin's interface */
paf::string Bgdl_811115D0(paf::string dir, int number);	/* dir + "/" + "%04d" */

#endif
