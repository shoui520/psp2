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

/* the plugin's main object, made at start and deleted at stop (0x8102B296, 0x8102B2BA) */
void gamePluginCreate(paf::Plugin *plugin);
void gamePluginDestroy(void);

/*
 * The plugin's work pool, 64 KiB blocks, lent to one user at a time while the plugin runs
 * (0x8102B5F6), and given back (0x8102B6CC).
 */
int gamePluginGetPool(const char *user, paf::common::SharedPtr<paf::memory::PoolAllocator> *pool);
void gamePluginReleasePool(paf::common::SharedPtr<paf::memory::PoolAllocator> *pool);

/* a debug dump of a buffer, empty in this build (0x814098DC) */
void gameDebugDump(const char *label, const void *data, SceSize size);

/*
 * An application's PFS mounted with its keystone, the mount point left in mountPoint
 * (0x8101E23C), and unmounted (0x8101E434).
 */
int gamePfsMount(const paf::string &dir, const paf::string &name, const void *keystone,
		 paf::string *mountPoint);
int gamePfsUnmount(paf::string *mountPoint);

/* a package's content type: additional content */
#define GAME_CONTENT_TYPE_ADDCONT	22

/* the plugin's error for an argument out of range (value only) */
#define GAME_ERROR_INVALID_ARGUMENT	-2146430720	/* 0x80101100 */

/* paf's error for a file that does not exist (value only) */
#define GAME_PAF_ERROR_NOT_FOUND	-2135994334	/* 0x80AF5022 */

/*
 * PLACEHOLDER: what 0x8136D9D4 reads from a split package's head.bin, 0x78 bytes; only the
 * content type and the body's size, in 32-byte blocks, are known.
 */
struct GamePackageInfo {
	SceUInt32 headerFields[1];
	SceUInt32 contentType;
	SceUInt32 moreHeaderFields[1];
	SceUInt32 bodyBlocks;
	SceUInt32 otherFields[26];
};

/*
 * PLACEHOLDER: an installation's package, its head.bin's information first and the
 * application's keystone at 0x278; the rest is not known yet.
 */
struct GameInstallPackage {
	GamePackageInfo info;
	SceUInt8 otherFields[0x278 - sizeof(GamePackageInfo)];
	SceUInt8 keystone[0x60];
};

/* the title IDs of the additional content an installation left (0x8102B8C4) */
int gamePluginGetAddcontTitles(paf::vector<paf::string> *titles);

/*
 * An application's folder mounted for reading, the mount point left in mountPoint
 * (0x8101DE16), and unmounted (0x8101E0B2). PLACEHOLDER: the meaning of the two words is not
 * known; install_cleanup.cpp passes 0 for each.
 */
int gameAppMount(const char *path, const char *drive, int arg2, int arg3,
		 paf::string *mountPoint);
int gameAppUnmount(paf::string *mountPoint);

/* PLACEHOLDER: what 0x8101BFB4 finds for path and leaves in detail, when detail is given */
void gamePathDetail(const paf::string &path, paf::string *detail);

/*
 * A directory removed, by a job of its own (0x8101BA08). PLACEHOLDER: the meaning of the
 * three words and the flag is not known; this file passes 0, 0, 0 and true.
 */
int gameRemoveDirectory(const char *path, int arg1, int arg2, int arg3, bool wait);

/*
 * A file copied, by a job of its own, with buf as its work memory (0x8101AB80). PLACEHOLDER:
 * the meaning of the other words is not known; this file passes 0 for each.
 */
int gameCopyFile(const char *src, const char *dst, int arg2, int arg3, void *buf,
		 SceSize bufSize, int arg6, int arg7);

/* a split package's information read from its head.bin at path, with buf as work memory */
int gamePackageReadInfo(GamePackageInfo *info, const char *path, void *buf, SceSize bufSize);

#endif
