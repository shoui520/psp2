/*
 * The launch white lists of a PlayStation TV (0x810A12B8..0x810A4C4B): version_launch.dat and
 * the lists it names read, ur0:game/launch brought up to date from vs0:data/internal/launch, from
 * ux0:game/launch or from the network, the teleport list kept in tm0:teleport in step, and an
 * application's title ID checked against the Vita or PSP list. Sony's mutex names the feature;
 * the file's name and the functions' are ours.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <regmgr/regmgr.h>
#include <std/string.h>
#include <std/stdlib.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <thread/thread.h>
#include <file/localfile.h>
#include <dir/dir.h>

#include "game_plugin.h"
#include "white_list.h"

using namespace paf;

/* the system's lists, the ones in use, and where newer ones are put together */
#define WHITE_LIST_VS0_VERSION		"vs0:data/internal/launch/version_launch.dat"
#define WHITE_LIST_VS0_VITA		"vs0:data/internal/launch/list_launch_vita.dat"
#define WHITE_LIST_VS0_EMU		"vs0:data/internal/launch/list_launch_emu.dat"
#define WHITE_LIST_VS0_TELEPORT		"vs0:data/internal/launch/list_launch_teleport.dat"
#define WHITE_LIST_UR0_GAME		"ur0:game"
#define WHITE_LIST_UR0_DIR		"ur0:game/launch"
#define WHITE_LIST_UR0_VERSION		"ur0:game/launch/version_launch.dat"
#define WHITE_LIST_UR0_VITA		"ur0:game/launch/list_launch_vita.dat"
#define WHITE_LIST_UR0_EMU		"ur0:game/launch/list_launch_emu.dat"
#define WHITE_LIST_UR0_TELEPORT		"ur0:game/launch/list_launch_teleport.dat"
#define WHITE_LIST_UR0_TEMP_DIR		"ur0:temp/game/launch"
#define WHITE_LIST_UR0_TEMP_VERSION	"ur0:temp/game/launch/version_launch.dat"
#define WHITE_LIST_UR0_TEMP_VITA	"ur0:temp/game/launch/list_launch_vita.dat"
#define WHITE_LIST_UR0_TEMP_EMU		"ur0:temp/game/launch/list_launch_emu.dat"
#define WHITE_LIST_UR0_TEMP_TELEPORT	"ur0:temp/game/launch/list_launch_teleport.dat"
#define WHITE_LIST_UR0_NEW_DIR		"ur0:temp/game/launch_"
#define WHITE_LIST_UX0_GAME		"ux0:game"
#define WHITE_LIST_UX0_DIR		"ux0:game/launch"
#define WHITE_LIST_UX0_VERSION		"ux0:game/launch/version_launch.dat"
#define WHITE_LIST_UX0_VITA		"ux0:game/launch/list_launch_vita.dat"
#define WHITE_LIST_UX0_EMU		"ux0:game/launch/list_launch_emu.dat"
#define WHITE_LIST_UX0_TELEPORT		"ux0:game/launch/list_launch_teleport.dat"
#define WHITE_LIST_UX0_TEMP_DIR		"ux0:temp/game/launch"
#define WHITE_LIST_VERSION_NAME		"/version_launch.dat"
#define WHITE_LIST_VITA_NAME		"/list_launch_vita.dat"
#define WHITE_LIST_EMU_NAME		"/list_launch_emu.dat"
#define WHITE_LIST_TELEPORT_NAME	"/list_launch_teleport.dat"
#define WHITE_LIST_VERSION_URL \
	"http://vitacl.ww.dl.playstation.net/vitacl/ww/j/version_launch.dat"

/* the teleport list in tm0:, its copies while it is replaced */
#define WHITE_LIST_TM0_DIR		"tm0:teleport"
#define WHITE_LIST_TM0_TELEPORT		"tm0:teleport/list_launch_teleport.dat"
#define WHITE_LIST_TM0_TELEPORT_OLD	"tm0:teleport/list_launch_teleport.old"
#define WHITE_LIST_TM0_TELEPORT_TMP	"tm0:teleport/list_launch_teleport.tmp"
#define WHITE_LIST_TM0_FORCE		"tm0:teleport/force.dat"

/* the largest version_launch.dat and list a source may give, and a teleport list */
#define WHITE_LIST_VERSION_MAX_SIZE	1791
#define WHITE_LIST_LIST_MAX_SIZE	101311
#define WHITE_LIST_TELEPORT_MAX_SIZE	4096

/* an entry's URL is shorter than this */
#define WHITE_LIST_URL_LIMIT		129

/* "LVER<n>", the first entry of a list, gives its version */
#define WHITE_LIST_VERSION_TAG		"LVER"

static thread::RMutex s_whiteListMutex("SceGameUpdateWhiteList", 0);

/*
 * version_launch.dat read: "<x>:<format>:<vita version>:<vita url>\n<x>:<emu version>:<emu url>
 * \n<x>:<teleport version>:<teleport url>\n", the teleport line optional; nothing when the file is
 * not there, no versions when it is bad (0x810A12B8)
 */
static int gameWhiteListVersionRead(const string &path, GameWhiteListVersion *version)
{
	int ret = 0;
	LocalFileStat stat;

	sce_paf_memset(version, 0, sizeof(*version));
	if (!LocalFile::Exists(path.c_str()))
		return 0;

	ret = LocalFile::Getstat(path.c_str(), &stat);
	if (ret < 0)
		return ret;

	char *buf = (char *)sce_paf_malloc((size_t)stat.st_size);

	if (buf == NULL)
		return GAME_WHITE_LIST_ERROR_NO_MEMORY;

	common::SharedPtr<File> file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &ret);

	if (file.get() != NULL) {
		int n = file->Read(buf, (size_t)stat.st_size);

		if (n < 0) {
			ret = n;
		} else if (n != stat.st_size) {
			ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
		} else {
			char format[4];
			char vitaVersion[11];
			char emuVersion[11];
			char teleportVersion[11];
			const char *start;
			char *end;
			char *p;
			int len;

			sce_paf_memset(format, 0, sizeof(format));
			sce_paf_memset(vitaVersion, 0, sizeof(vitaVersion));
			sce_paf_memset(emuVersion, 0, sizeof(emuVersion));
			sce_paf_memset(teleportVersion, 0, sizeof(teleportVersion));

			do {
				p = sce_paf_strchr(buf, ':');
				if (p == NULL) {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
					break;
				}
				start = p + 1;
				sce_paf_strncpy(format, start, 3);
				format[3] = '\0';
				version->format = sce_paf_atoi(format);

				p = sce_paf_strchr(start, ':');
				if (p == NULL) {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
					break;
				}
				start = p + 1;
				sce_paf_strncpy(vitaVersion, start, 10);
				vitaVersion[10] = '\0';
				version->vita.version = sce_paf_atoi(vitaVersion);
				p = sce_paf_strchr(start, ':');
				if (p == NULL) {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
					break;
				}
				start = p + 1;
				end = sce_paf_strchr(start, '\n');
				if (end == NULL) {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
					break;
				}
				len = end[-1] == '\r' ? (end - start - 1) : (end - start);
				if (len >= WHITE_LIST_URL_LIMIT || len <= 0) {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
					break;
				}
				sce_paf_strncpy(version->vita.url, start, len);
				version->vita.url[len] = '\0';

				p = sce_paf_strchr(end, ':');
				if (p == NULL) {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
					break;
				}
				start = p + 1;
				sce_paf_strncpy(emuVersion, start, 10);
				emuVersion[10] = '\0';
				version->emu.version = sce_paf_atoi(emuVersion);
				p = sce_paf_strchr(start, ':');
				if (p == NULL) {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
					break;
				}
				start = p + 1;
				end = sce_paf_strchr(start, '\n');
				if (end == NULL) {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
					break;
				}
				len = end[-1] == '\r' ? (end - start - 1) : (end - start);
				if (len >= WHITE_LIST_URL_LIMIT || len <= 0) {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
					break;
				}
				sce_paf_strncpy(version->emu.url, start, len);
				version->emu.url[len] = '\0';

				/* the teleport line is optional */
				p = sce_paf_strchr(end, ':');
				if (p == NULL) {
					ret = 0;
					break;
				}
				start = p + 1;
				sce_paf_strncpy(teleportVersion, start, 10);
				teleportVersion[10] = '\0';
				version->teleport.version = sce_paf_atoi(teleportVersion);
				p = sce_paf_strchr(start, ':');
				if (p == NULL) {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
					break;
				}
				start = p + 1;
				end = sce_paf_strchr(start, '\n');
				if (end == NULL) {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
					break;
				}
				len = end[-1] == '\r' ? (end - start - 1) : (end - start);
				if (len >= WHITE_LIST_URL_LIMIT || len <= 0) {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
					break;
				}
				sce_paf_strncpy(version->teleport.url, start, len);
				version->teleport.url[len] = '\0';
			} while (0);
		}
	}

	if (ret < 0) {
		version->vita.version = 0;
		version->emu.version = 0;
		version->teleport.version = 0;
	}
	if (file.get() != NULL)
		file->Close();
	sce_paf_free(buf);
	return ret;
}

/* a list's kind, from its first line: 'w' white, 'b' black (0x810A172A) */
static int gameWhiteListTypeRead(const string &path, int *type)
{
	int ret = 0;

	*type = 0;

	LocalFileStat stat;

	if (!LocalFile::Exists(path.c_str()))
		return GAME_WHITE_LIST_ERROR_NOT_FOUND;

	ret = LocalFile::Getstat(path.c_str(), &stat);
	if (ret < 0)
		return ret;

	int64_t size = 48;

	if (stat.st_size < 48)
		size = stat.st_size;

	char *buf = (char *)sce_paf_malloc((size_t)size);

	if (buf == NULL)
		return GAME_WHITE_LIST_ERROR_NO_MEMORY;

	common::SharedPtr<File> file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &ret);

	if (file.get() != NULL) {
		int n = file->Read(buf, (size_t)size);

		if (n < 0) {
			ret = n;
		} else if (n != size) {
			ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
		} else {
			char format[4];

			file->Close();
			sce_paf_memset(format, 0, sizeof(format));

			char *p = sce_paf_strchr(buf, ':');

			if (p != NULL) {
				sce_paf_strncpy(format, p + 1, 3);
				format[3] = '\0';
				p = sce_paf_strchr(p + 1, ':');
				if (p != NULL) {
					if (p[1] == 'w')
						*type = GAME_WHITE_LIST_TYPE_WHITE;
					else if (p[1] == 'b')
						*type = GAME_WHITE_LIST_TYPE_BLACK;
					else
						ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
				} else {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
				}
			} else {
				ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
			}
		}
	}
	if (file.get() != NULL && file->IsOpened())
		file->Close();
	sce_paf_free(buf);
	return ret;
}

/*
 * The teleport list in tm0:teleport replaced by file unless it is as new: the old one kept as
 * .old, the new one written as .tmp first (0x810A196A)
 */
static int gameWhiteListTeleportSync(common::SharedPtr<File> file)
{
	SceSize size = 0;
	SceSize oldSize = 0;
	int ret;
	char *oldBuf = NULL;
	char *copy = NULL;
	common::SharedPtr<File> old;
	common::SharedPtr<File> out;

	if (file.get() == NULL)
		return GAME_WHITE_LIST_ERROR_BAD_FILE;

	ret = 0;
	if (!file->IsOpened())
		return GAME_WHITE_LIST_ERROR_NOT_OPENED;

	int64_t fileSize = file->GetFileSize();

	if (fileSize > WHITE_LIST_TELEPORT_MAX_SIZE)
		return GAME_WHITE_LIST_ERROR_BAD_FILE;

	size = WHITE_LIST_TELEPORT_MAX_SIZE;

	char *buf = new char[WHITE_LIST_TELEPORT_MAX_SIZE];

	if (buf == NULL)
		return GAME_WHITE_LIST_ERROR_NO_MEMORY;

	ret = gameWhiteListFileRead(file, buf, &size);
	if (ret < 0) {
		delete[] buf;
		return ret;
	}

	GameWhiteListTeleportHeader header;

	sce_paf_memset(&header, 0, sizeof(header));
	ret = gameWhiteListTeleportHeaderRead(buf, size, &header);
	delete[] buf;
	if (ret < 0)
		return ret;

	Dir::Create(WHITE_LIST_TM0_DIR);

	bool needWrite = true;

	if (!LocalFile::Exists(WHITE_LIST_TM0_TELEPORT)) {
		if (LocalFile::Exists(WHITE_LIST_TM0_TELEPORT_OLD)) {
			ret = LocalFile::RenameFile(WHITE_LIST_TM0_TELEPORT_OLD,
						    WHITE_LIST_TM0_TELEPORT);
			if (ret < 0)
				return ret;
		} else {
			needWrite = true;
		}
	}

	if (LocalFile::Exists(WHITE_LIST_TM0_TELEPORT)) {
		old = LocalFile::Open(WHITE_LIST_TM0_TELEPORT, SCE_O_RDONLY, 0, &ret);
		if (old.get() == NULL) {
			needWrite = false;
		} else {
			oldSize = WHITE_LIST_TELEPORT_MAX_SIZE;
			oldBuf = new char[WHITE_LIST_TELEPORT_MAX_SIZE];
			if (oldBuf == NULL) {
				ret = GAME_WHITE_LIST_ERROR_NO_MEMORY;
				needWrite = false;
			} else {
				ret = gameWhiteListFileRead(old, oldBuf, &oldSize);
				if (ret < 0) {
					needWrite = false;
				} else {
					old->Close();

					GameWhiteListTeleportHeader oldHeader;

					sce_paf_memset(&oldHeader, 0, sizeof(oldHeader));
					ret = gameWhiteListTeleportHeaderRead(oldBuf, oldSize,
									      &oldHeader);
					if (ret < 0) {
						needWrite = false;
					} else if (oldHeader.version >= header.version) {
						ret = 0;
						needWrite = false;
					} else {
						delete[] oldBuf;
						oldBuf = NULL;
					}
				}
			}
		}
	}

	if (needWrite) {
		oldBuf = NULL;
		if (LocalFile::Exists(WHITE_LIST_TM0_TELEPORT_TMP))
			ret = LocalFile::RemoveFile(WHITE_LIST_TM0_TELEPORT_TMP);

		if (ret >= 0) {
			out = LocalFile::Open(WHITE_LIST_TM0_TELEPORT_TMP,
					      SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 6, &ret);
			if (out.get() != NULL) {
				copy = new char[(size_t)fileSize];
				if (copy != NULL) {
					file->Seek(0, SCE_SEEK_SET);

					int n = file->Read(copy, (size_t)fileSize);

					if (n < 0) {
						ret = n;
					} else if (n != fileSize) {
						ret = GAME_WHITE_LIST_ERROR_SHORT_READ;
					} else {
						int written = out->Write(copy, n);

						if (written < 0) {
							ret = written;
						} else if (n != written) {
							ret = GAME_WHITE_LIST_ERROR_SHORT_WRITE;
						} else {
							out->Close();
							delete[] copy;
							copy = NULL;
							if (!LocalFile::Exists(WHITE_LIST_TM0_TELEPORT_OLD) ||
							    (ret = LocalFile::RemoveFile(WHITE_LIST_TM0_TELEPORT_OLD)) >= 0) {
								if (!LocalFile::Exists(WHITE_LIST_TM0_TELEPORT) ||
								    (ret = LocalFile::RenameFile(WHITE_LIST_TM0_TELEPORT,
												 WHITE_LIST_TM0_TELEPORT_OLD)) >= 0)
									ret = LocalFile::RenameFile(WHITE_LIST_TM0_TELEPORT_TMP,
												    WHITE_LIST_TM0_TELEPORT);
							}
						}
					}
				} else {
					ret = GAME_WHITE_LIST_ERROR_NO_MEMORY;
				}
				if (out->IsOpened())
					out->Close();
			}
		}
	}

	if (old.get() != NULL && old->IsOpened())
		old->Close();
	if (copy != NULL)
		delete[] copy;
	if (oldBuf != NULL)
		delete[] oldBuf;
	return ret;
}

/*
 * A list's title IDs, one per line after its first ':'-separated line, nine characters each;
 * only the first when first is set (0x810A1FE6)
 */
static int gameWhiteListRead(const string &path, vector<string> *list, int *type, bool first)
{
	int ret = 0;

	list->clear();
	*type = 0;

	LocalFileStat stat;

	if (!LocalFile::Exists(path.c_str()))
		return GAME_WHITE_LIST_ERROR_NOT_FOUND;

	ret = LocalFile::Getstat(path.c_str(), &stat);
	if (ret < 0)
		return ret;

	char *buf = (char *)sce_paf_malloc((size_t)stat.st_size);

	if (buf == NULL)
		return GAME_WHITE_LIST_ERROR_NO_MEMORY;

	common::SharedPtr<File> file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &ret);

	if (file.get() != NULL) {
		int n = file->Read(buf, (size_t)stat.st_size);

		if (n < 0) {
			ret = n;
		} else if (n != stat.st_size) {
			ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
		} else {
			char format[4];

			sce_paf_memset(format, 0, sizeof(format));

			do {
				char *p = sce_paf_strchr(buf, ':');

				if (p == NULL) {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
					break;
				}
				sce_paf_strncpy(format, p + 1, 3);
				format[3] = '\0';
				p = sce_paf_strchr(p + 1, ':');
				if (p == NULL) {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
					break;
				}

				int kind;

				if (p[1] == 'w') {
					kind = GAME_WHITE_LIST_TYPE_WHITE;
				} else if (p[1] == 'b') {
					kind = GAME_WHITE_LIST_TYPE_BLACK;
				} else {
					ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
					break;
				}
				*type = kind;

				const char *last = buf + n - 1;
				const char *line = p + 2;

				if (p[2] == '\r')
					line = p + 3;
				if (*line == '\n')
					++line;

				char *nl;

				for (const char *prev = line - 1;; prev = nl) {
					char titleId[10];

					sce_paf_memset(titleId, 0, sizeof(titleId));
					if (prev == last)
						break;
					nl = sce_paf_strchr(line, '\n');
					if (nl == NULL)
						break;
					if (nl[-1] == '\r') {
						int len = nl - line - 1;

						if (nl - line != 10) {
							ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
							break;
						}
						sce_paf_strncpy(titleId, line, len);
						titleId[len] = '\0';
					} else {
						if (nl - line != 9) {
							ret = GAME_WHITE_LIST_ERROR_BAD_FORMAT;
							break;
						}
						sce_paf_strncpy(titleId, line, 9);
						titleId[nl - line] = '\0';
					}
					{
						string id(titleId);

						line = nl + 1;
						list->push_back(id);
					}
					if (first)
						break;
				}
			} while (0);
		}
	}

	if (file.get() != NULL)
		file->Close();
	sce_paf_free(buf);
	return ret;
}

/* whether s starts with prefix */
static bool gameWhiteListStartsWith(const string &s, const string &prefix)
{
	if (s.length() < prefix.length())
		return false;
	for (string::size_type i = 0; i < prefix.length(); ++i) {
		if (prefix[i] != s[i])
			return false;
	}
	return true;
}

/* a list older than version, by its "LVER" entry, is refused; one without it is not (0x810A23BA) */
static int gameWhiteListCheckVersion(const string &path, SceUInt32 version)
{
	int type = 0;
	vector<string> list;
	int ret = gameWhiteListRead(path, &list, &type, true);

	if (ret < 0)
		return ret;
	if (list.size() != 0) {
		bool tagged = false;
		SceUInt32 listVersion = 0;

		{
			string tag(WHITE_LIST_VERSION_TAG);

			if (gameWhiteListStartsWith(list[0], tag)) {
				listVersion = sce_paf_atoi(list[0].substr(tag.length(),
									  string::npos).c_str());
				tagged = true;
			}
		}
		if (tagged && version > listVersion)
			return GAME_WHITE_LIST_ERROR_OLD_VERSION;
	}
	return 0;
}

/* a list read only for its kind: a black list is refused */
static int gameWhiteListCheckType(const string &path)
{
	int type = 0;
	int ret = gameWhiteListTypeRead(path, &type);

	if (ret >= 0) {
		ret = 0;
		if (type == GAME_WHITE_LIST_TYPE_BLACK)
			ret = GAME_WHITE_LIST_ERROR_BLACK_LIST;
	}
	return ret;
}

/*
 * ur0:game/launch brought up to date at boot: from the system's lists when they are newer or the
 * ones in use are bad, then from ux0:game/launch when it has newer ones; the teleport list in
 * tm0: follows (0x810A258C)
 */
int gameWhiteListUpdate(void)
{
	int ret = 0;
	GameWhiteListVersion current;
	GameWhiteListVersion update;
	GameWhiteListVersion internal;
	common::SharedPtr<File> teleport;
	bool refresh;

	sce_paf_memset(&current, 0, sizeof(current));
	sce_paf_memset(&update, 0, sizeof(update));
	sce_paf_memset(&internal, 0, sizeof(internal));

	bool ux0 = gameWhiteListUx0Exists();
	bool fromUx0 = LocalFile::Exists(WHITE_LIST_UX0_VERSION) && ux0;

	if (!LocalFile::Exists(WHITE_LIST_VS0_VERSION))
		return 0;

	do {
		if (LocalFile::Exists(WHITE_LIST_UR0_VERSION)) {
			ret = gameWhiteListVersionRead(string(WHITE_LIST_UR0_VERSION), &current);
			if (ret < 0) {
				if (ret == GAME_WHITE_LIST_ERROR_BAD_FORMAT)
					Dir::RemoveRecursive(WHITE_LIST_UR0_DIR);
				break;
			}
			ret = gameWhiteListVersionRead(string(WHITE_LIST_VS0_VERSION), &internal);
			if (ret < 0)
				break;

			string currentVita(current.vita.url);
			string currentEmu(current.emu.url);
			string currentTeleport(current.teleport.url);
			string internalVita(internal.vita.url);
			string internalEmu(internal.emu.url);
			string internalTeleport(internal.teleport.url);

			if (currentVita == internalVita && currentEmu == internalEmu &&
			    currentTeleport == internalTeleport) {
				refresh = current.vita.version < internal.vita.version ||
					  current.emu.version < internal.emu.version ||
					  current.teleport.version < internal.teleport.version;
			} else {
				refresh = false;
				if (currentTeleport.empty())
					refresh = !internalTeleport.empty();
			}

			ret = gameWhiteListCheckType(string(WHITE_LIST_UR0_VITA));
			if (ret < 0) {
				if (ret != GAME_WHITE_LIST_ERROR_BLACK_LIST &&
				    ret != GAME_WHITE_LIST_ERROR_BAD_FORMAT)
					break;
				refresh = true;
			}
		} else {
			refresh = true;
		}

		if (refresh) {
			ret = gameWhiteListDirRecreate(WHITE_LIST_UR0_TEMP_DIR);
			if (ret < 0)
				break;
			ret = gameWhiteListFileCopy(WHITE_LIST_VS0_VERSION,
						    WHITE_LIST_UR0_TEMP_VERSION);
			if (ret < 0)
				break;
			ret = gameWhiteListFileCopy(WHITE_LIST_VS0_VITA, WHITE_LIST_UR0_TEMP_VITA);
			if (ret < 0)
				break;
			ret = gameWhiteListFileCopy(WHITE_LIST_VS0_EMU, WHITE_LIST_UR0_TEMP_EMU);
			if (ret < 0)
				break;
			if (LocalFile::Exists(WHITE_LIST_VS0_TELEPORT)) {
				ret = gameWhiteListFileCopy(WHITE_LIST_VS0_TELEPORT,
							    WHITE_LIST_UR0_TEMP_TELEPORT);
				if (ret < 0)
					break;
			}
			if (Dir::IsExist(WHITE_LIST_UR0_GAME)) {
				if (Dir::IsExist(WHITE_LIST_UR0_DIR)) {
					ret = Dir::RemoveRecursive(WHITE_LIST_UR0_DIR);
					if (ret < 0)
						break;
				}
			} else {
				ret = Dir::CreateRecursive(WHITE_LIST_UR0_GAME);
				if (ret < 0)
					break;
			}
			ret = LocalFile::RenameFile(WHITE_LIST_UR0_TEMP_DIR, WHITE_LIST_UR0_DIR);
			if (ret < 0)
				break;
			if (LocalFile::Exists(WHITE_LIST_UR0_TELEPORT)) {
				int forced = gameWhiteListTeleportForceCheck();

				ret = forced;
				if (forced < 0)
					break;
				if (forced == 0) {
					teleport = LocalFile::Open(WHITE_LIST_UR0_TELEPORT,
								   SCE_O_RDONLY, 0, &ret);
					if (teleport.get() == NULL)
						break;
					ret = gameWhiteListTeleportSync(teleport);
					if (ret < 0)
						break;
					teleport->Close();
				}
			}
		}

		if (fromUx0) {
			sce_paf_memset(&current, 0, sizeof(current));
			ret = gameWhiteListVersionRead(string(WHITE_LIST_UR0_VERSION), &current);
			if (ret < 0) {
				if (ret == GAME_WHITE_LIST_ERROR_BAD_FORMAT)
					Dir::RemoveRecursive(WHITE_LIST_UR0_DIR);
				break;
			}
			ret = gameWhiteListDirRecreate(WHITE_LIST_UR0_TEMP_DIR);
			if (ret >= 0) {
				SceSize size = WHITE_LIST_VERSION_MAX_SIZE;

				ret = gameWhiteListFileUnwrap(WHITE_LIST_UX0_VERSION,
							      WHITE_LIST_UR0_TEMP_VERSION, &size,
							      false);
				if (ret < 0) {
					Dir::RemoveRecursive(WHITE_LIST_UX0_DIR);
					break;
				}
				ret = gameWhiteListVersionRead(string(WHITE_LIST_UR0_TEMP_VERSION),
							       &update);
				if (ret < 0) {
					if (ret != GAME_WHITE_LIST_ERROR_BAD_FORMAT)
						break;
					ret = Dir::RemoveRecursive(WHITE_LIST_UX0_DIR);
					if (ret < 0)
						break;
				}

				string currentVita(current.vita.url);
				string currentEmu(current.emu.url);
				string currentTeleport(current.teleport.url);
				string updateVita(update.vita.url);
				string updateEmu(update.emu.url);
				string updateTeleport(update.teleport.url);

				if (currentVita == updateVita && currentEmu == updateEmu &&
				    currentTeleport == updateTeleport) {
					if (current.vita.version >= update.vita.version &&
					    current.emu.version >= update.emu.version &&
					    current.teleport.version >= update.teleport.version)
						break;
				} else {
					if (!currentTeleport.empty() && updateTeleport.empty())
						break;
				}

				SceSize listSize = WHITE_LIST_LIST_MAX_SIZE;

				ret = gameWhiteListFileUnwrap(WHITE_LIST_UX0_VITA,
							      WHITE_LIST_UR0_TEMP_VITA, &listSize,
							      false);
				if (ret < 0)
					break;
				ret = gameWhiteListCheckVersion(string(WHITE_LIST_UR0_TEMP_VITA),
								update.vita.version);
				if (ret < 0)
					break;
				ret = gameWhiteListCheckType(string(WHITE_LIST_UR0_TEMP_VITA));
				if (ret < 0)
					break;
				listSize = WHITE_LIST_LIST_MAX_SIZE;
				ret = gameWhiteListFileUnwrap(WHITE_LIST_UX0_EMU,
							      WHITE_LIST_UR0_TEMP_EMU, &listSize,
							      false);
				if (ret < 0)
					break;
				ret = gameWhiteListCheckVersion(string(WHITE_LIST_UR0_TEMP_EMU),
								update.emu.version);
				if (ret < 0)
					break;
				if (LocalFile::Exists(WHITE_LIST_UX0_TELEPORT)) {
					ret = gameWhiteListFileCopy(WHITE_LIST_UX0_TELEPORT,
								    WHITE_LIST_UR0_TEMP_TELEPORT);
					if (ret < 0)
						break;
				}
				if (Dir::IsExist(WHITE_LIST_UR0_GAME)) {
					if (Dir::IsExist(WHITE_LIST_UR0_DIR)) {
						ret = Dir::RemoveRecursive(WHITE_LIST_UR0_DIR);
						if (ret < 0)
							break;
					}
				} else {
					ret = Dir::CreateRecursive(WHITE_LIST_UR0_GAME);
					if (ret < 0)
						break;
				}
				ret = LocalFile::RenameFile(WHITE_LIST_UR0_TEMP_DIR,
							    WHITE_LIST_UR0_DIR);
				if (ret >= 0 && LocalFile::Exists(WHITE_LIST_UR0_TELEPORT)) {
					ret = gameWhiteListTeleportForceCheck();
					if (ret == 0) {
						teleport = LocalFile::Open(WHITE_LIST_UR0_TELEPORT,
									   SCE_O_RDONLY, 0, &ret);
						if (teleport.get() != NULL) {
							ret = gameWhiteListTeleportSync(teleport);
							if (ret >= 0)
								teleport->Close();
						}
					}
				}
			}
		}
	} while (0);

	gameWhiteListDirRemove(WHITE_LIST_UR0_TEMP_DIR);
	if (teleport.get() != NULL && teleport->IsOpened())
		teleport->Close();
	return ret;
}

/*
 * The teleport list in tm0:teleport replaced by file whatever its version, when force.dat asks
 * for it; force.dat removed (0x810A3366)
 */
static int gameWhiteListTeleportInstall(common::SharedPtr<File> file)
{
	SceSize size = 0;
	int ret;
	char *copy = NULL;
	common::SharedPtr<File> out;

	if (file.get() == NULL)
		return GAME_WHITE_LIST_ERROR_BAD_FILE;

	ret = 0;
	if (!file->IsOpened())
		return GAME_WHITE_LIST_ERROR_NOT_OPENED;

	ret = gameWhiteListTeleportForceCheck();
	if (ret == 0)
		return GAME_WHITE_LIST_ERROR_NOT_FORCED;
	if (ret < 0)
		return ret;

	int64_t fileSize = file->GetFileSize();

	if (fileSize > WHITE_LIST_TELEPORT_MAX_SIZE)
		return GAME_WHITE_LIST_ERROR_BAD_FILE;

	size = WHITE_LIST_TELEPORT_MAX_SIZE;

	char *buf = new char[WHITE_LIST_TELEPORT_MAX_SIZE];

	if (buf == NULL)
		return GAME_WHITE_LIST_ERROR_NO_MEMORY;

	ret = gameWhiteListFileRead(file, buf, &size);
	if (ret < 0) {
		delete[] buf;
		return ret;
	}

	GameWhiteListTeleportHeader header;

	sce_paf_memset(&header, 0, sizeof(header));
	ret = gameWhiteListTeleportHeaderRead(buf, size, &header);
	delete[] buf;
	if (ret < 0)
		return ret;

	if (!Dir::IsExist(WHITE_LIST_TM0_DIR))
		Dir::Create(WHITE_LIST_TM0_DIR);

	out = LocalFile::Open(WHITE_LIST_TM0_TELEPORT_TMP, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
			      6, &ret);
	if (out.get() != NULL) {
		copy = new char[(size_t)fileSize];
		if (copy != NULL) {
			file->Seek(0, SCE_SEEK_SET);

			int n = file->Read(copy, (size_t)fileSize);

			if (n < 0) {
				ret = n;
			} else if (n != fileSize) {
				ret = GAME_WHITE_LIST_ERROR_SHORT_READ;
			} else {
				int written = out->Write(copy, n);

				if (written < 0) {
					ret = written;
				} else if (n != written) {
					ret = GAME_WHITE_LIST_ERROR_SHORT_WRITE;
				} else {
					out->Close();
					delete[] copy;
					ret = LocalFile::RenameFile(WHITE_LIST_TM0_TELEPORT_TMP,
								    WHITE_LIST_TM0_TELEPORT);
					copy = NULL;
					if (ret >= 0 && LocalFile::Exists(WHITE_LIST_TM0_FORCE))
						ret = LocalFile::RemoveFile(WHITE_LIST_TM0_FORCE);
				}
			}
		} else {
			ret = GAME_WHITE_LIST_ERROR_NO_MEMORY;
		}
		if (out->IsOpened())
			out->Close();
	}
	if (copy != NULL)
		delete[] copy;
	return ret;
}

/*
 * The lists fetched from the network: version_launch.dat first, then the lists it names when they
 * differ from the ones in use, each unwrapped and checked, and ur0:game/launch replaced; kept in
 * ux0:game/launch too when ux0: is there (0x810A36EE)
 */
int gameWhiteListDownload(void)
{
	int ret = 0;
	GameWhiteListVersion current;
	GameWhiteListVersion update;

	sce_paf_memset(&current, 0, sizeof(current));
	sce_paf_memset(&update, 0, sizeof(update));

	string currentVita("");
	string currentEmu("");
	string currentTeleport("");
	string updateVita("");
	string updateEmu("");
	string updateTeleport("");
	bool ux0 = gameWhiteListUx0Exists();
	const char *dir = WHITE_LIST_UR0_TEMP_DIR;

	if (ux0)
		dir = WHITE_LIST_UX0_TEMP_DIR;

	string downloadDir(dir);
	string downloadVersion = downloadDir + string(WHITE_LIST_VERSION_NAME);
	string downloadVita = downloadDir + string(WHITE_LIST_VITA_NAME);
	string downloadEmu = downloadDir + string(WHITE_LIST_EMU_NAME);
	string downloadTeleport = downloadDir + string(WHITE_LIST_TELEPORT_NAME);
	string newVersion = WHITE_LIST_UR0_NEW_DIR + string(WHITE_LIST_VERSION_NAME);
	string newVita = WHITE_LIST_UR0_NEW_DIR + string(WHITE_LIST_VITA_NAME);
	string newEmu = WHITE_LIST_UR0_NEW_DIR + string(WHITE_LIST_EMU_NAME);
	string newTeleport = WHITE_LIST_UR0_NEW_DIR + string(WHITE_LIST_TELEPORT_NAME);
	SceSize size = 0;

	ret = gameWhiteListVersionRead(string(WHITE_LIST_UR0_VERSION), &current);
	if (ret < 0) {
		if (ret != GAME_WHITE_LIST_ERROR_BAD_FORMAT)
			return ret;
		ret = Dir::RemoveRecursive(WHITE_LIST_UR0_DIR);
		if (ret < 0)
			return ret;
	}

	bool downloaded = false;
	char *buf = NULL;
	common::SharedPtr<File> teleport;

	do {
		ret = gameWhiteListDirRecreate(downloadDir.c_str());
		if (ret < 0)
			break;
		ret = gameWhiteListDirRecreate(WHITE_LIST_UR0_NEW_DIR);
		if (ret < 0)
			break;
		{
			SceUInt64 maxSize = WHITE_LIST_VERSION_MAX_SIZE;

			ret = gameWhiteListDownloadFile(WHITE_LIST_VERSION_URL,
							downloadVersion.c_str(), &maxSize);
			if (ret < 0)
				break;
		}
		{
			SceSize versionSize = WHITE_LIST_VERSION_MAX_SIZE;

			ret = gameWhiteListFileUnwrap(downloadVersion.c_str(), newVersion.c_str(),
						      &versionSize, !ux0);
			if (ret < 0)
				break;
		}
		ret = gameWhiteListVersionRead(newVersion, &update);
		if (ret < 0)
			break;

		currentVita = current.vita.url;
		currentEmu = current.emu.url;
		currentTeleport = current.teleport.url;
		updateVita = update.vita.url;
		updateEmu = update.emu.url;
		updateTeleport = update.teleport.url;

		{
			int forced = gameWhiteListTeleportForceCheck();

			if (forced < 0)
				break;
			if (forced != 1 && currentVita == updateVita && currentEmu == updateEmu &&
			    (currentTeleport == updateTeleport || currentTeleport.empty() ||
			     updateTeleport.empty()) &&
			    current.vita.version == update.vita.version &&
			    current.emu.version == update.emu.version &&
			    (updateTeleport.empty() ||
			     current.teleport.version == update.teleport.version)) {
				ret = 0;
				break;
			}
		}

		downloaded = true;
		{
			SceUInt64 maxSize = WHITE_LIST_LIST_MAX_SIZE;
			SceSize listSize;

			ret = gameWhiteListDownloadFile(update.vita.url, downloadVita.c_str(),
							&maxSize);
			if (ret < 0)
				break;
			listSize = WHITE_LIST_LIST_MAX_SIZE;
			ret = gameWhiteListFileUnwrap(downloadVita.c_str(), newVita.c_str(),
						      &listSize, !ux0);
			if (ret < 0)
				break;
			ret = gameWhiteListCheckVersion(newVita, update.vita.version);
			if (ret < 0)
				break;
			maxSize = WHITE_LIST_LIST_MAX_SIZE;
			ret = gameWhiteListDownloadFile(update.emu.url, downloadEmu.c_str(),
							&maxSize);
			if (ret < 0)
				break;
			listSize = WHITE_LIST_LIST_MAX_SIZE;
			ret = gameWhiteListFileUnwrap(downloadEmu.c_str(), newEmu.c_str(),
						      &listSize, !ux0);
			if (ret < 0)
				break;
			ret = gameWhiteListCheckVersion(newEmu, update.emu.version);
			if (ret < 0)
				break;
		}
		{
			string teleportUrl(update.teleport.url);

			if (!teleportUrl.empty()) {
				SceUInt64 maxSize = WHITE_LIST_LIST_MAX_SIZE;

				ret = gameWhiteListDownloadFile(update.teleport.url,
								downloadTeleport.c_str(),
								&maxSize);
				if (ret < 0 ||
				    (ret = gameWhiteListFileCopy(downloadTeleport.c_str(),
								 newTeleport.c_str())) < 0)
					break;
				if (!ux0)
					LocalFile::RemoveFile(downloadTeleport.c_str());
				teleport = LocalFile::Open(newTeleport.c_str(), SCE_O_RDONLY, 0,
							   &ret);
				if (teleport.get() == NULL)
					break;
				size = WHITE_LIST_TELEPORT_MAX_SIZE;
				buf = new char[WHITE_LIST_TELEPORT_MAX_SIZE];
				if (buf == NULL) {
					ret = GAME_WHITE_LIST_ERROR_NO_MEMORY;
					break;
				}
				ret = gameWhiteListFileRead(teleport, buf, &size);
				if (ret < 0)
					break;
				teleport->Close();

				GameWhiteListTeleportHeader header;

				sce_paf_memset(&header, 0, sizeof(header));
				ret = gameWhiteListTeleportHeaderRead(buf, size, &header);
				if (ret < 0)
					break;
				delete[] buf;
				buf = NULL;
				if (update.teleport.version > header.version) {
					ret = GAME_WHITE_LIST_ERROR_OLD_VERSION;
					break;
				}
			}

			if (Dir::IsExist(WHITE_LIST_UR0_DIR)) {
				ret = Dir::RemoveRecursive(WHITE_LIST_UR0_DIR);
				if (ret < 0)
					break;
			}
			ret = LocalFile::RenameFile(WHITE_LIST_UR0_NEW_DIR, WHITE_LIST_UR0_DIR);
			if (ret < 0)
				break;
			if (!LocalFile::Exists(WHITE_LIST_UR0_TELEPORT))
				break;

			int forced = gameWhiteListTeleportForceCheck();

			if (forced < 0) {
				ret = forced;
				break;
			}
			teleport = LocalFile::Open(WHITE_LIST_UR0_TELEPORT, SCE_O_RDONLY, 0, &ret);
			if (teleport.get() == NULL)
				break;
			if (forced == 1)
				ret = gameWhiteListTeleportInstall(teleport);
			else
				ret = gameWhiteListTeleportSync(teleport);
			if (ret < 0)
				break;
			teleport->Close();
		}
	} while (0);

	if (ux0 && downloaded && ret == 0) {
		if (Dir::IsExist(WHITE_LIST_UX0_GAME)) {
			if (Dir::IsExist(WHITE_LIST_UX0_DIR))
				Dir::RemoveRecursive(WHITE_LIST_UX0_DIR);
		} else {
			Dir::CreateRecursive(WHITE_LIST_UX0_GAME);
		}
		LocalFile::RenameFile(downloadDir.c_str(), WHITE_LIST_UX0_DIR);
	} else {
		gameWhiteListDirRemove(downloadDir.c_str());
	}
	gameWhiteListDirRemove(WHITE_LIST_UR0_NEW_DIR);
	if (teleport.get() != NULL && teleport->IsOpened())
		teleport->Close();
	if (buf != NULL)
		delete[] buf;
	return ret;
}

/*
 * Whether a list lets titleId start: everything when /CONFIG/SYSTEM/launch_check is set,
 * otherwise what a white list lists or a black list does not (0x810A484E)
 */
static int gameWhiteListCheck(const string &path, const string &titleId, bool *allowed)
{
	*allowed = false;

	int launchCheck = 0;

	if (sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "launch_check", &launchCheck) == 0 &&
	    launchCheck != 0) {
		*allowed = true;
		return 0;
	}

	vector<string> list;
	int type = 0;
	int ret = gameWhiteListRead(path, &list, &type, false);

	if (ret < 0)
		return ret;

	int listed = 0;

	for (SceUInt32 i = 0; i < list.size(); ++i) {
		int match = 0;

		{
			string id(list[i]);

			if (titleId == id)
				match = 1;
		}
		if (match) {
			listed = 1;
			break;
		}
	}

	if (type == GAME_WHITE_LIST_TYPE_WHITE)
		*allowed = listed != 0;
	else
		*allowed = listed == 0;
	return 0;
}

/* a Vita application checked against list_launch_vita.dat (0x810A4A24) */
int gameWhiteListVitaCheck(const string &titleId, bool *allowed)
{
	*allowed = false;

	int ret = gameWhiteListCheck(string(WHITE_LIST_UR0_VITA), titleId, allowed);

	if (ret < 0)
		return ret;
	return 0;
}

/* a PSP application checked against list_launch_emu.dat (0x810A4AA2) */
int gameWhiteListEmuCheck(const string &titleId, bool *allowed)
{
	*allowed = false;

	int ret = gameWhiteListCheck(string(WHITE_LIST_UR0_EMU), titleId, allowed);

	if (ret < 0)
		return ret;
	return 0;
}

/* whether applications are checked: /CONFIG/SYSTEM/launch_check unset or 0 (0x810A4B20) */
bool gameWhiteListCheckIsOn(void)
{
	int launchCheck = 0;
	bool on = false;

	if (sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "launch_check", &launchCheck) != 0)
		on = true;
	else if (launchCheck == 0)
		on = true;
	return on;
}

/* the teleport list's file read, for the teleport application (0x810A4B6C) */
int gameWhiteListReadTeleportFile(common::SharedPtr<File> file, char *buf, SceSize *size)
{
	return gameWhiteListFileRead(file, buf, size);
}

/* its first line parsed (0x810A4C0A) */
int gameWhiteListParseTeleportHeader(const char *buf, SceSize size,
				     GameWhiteListTeleportHeader *header)
{
	return gameWhiteListTeleportHeaderRead(buf, size, header);
}

/* force.dat made, so the next update replaces the teleport list whatever its version (0x810A4C12) */
int gameWhiteListRequestTeleportUpdate(void)
{
	return gameWhiteListTeleportForceSet();
}
