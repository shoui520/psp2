/*
 * paf::Dir: a directory opened for reading, and the static directory
 * operations (create, remove, test) on paths.
 */
#ifndef PAF_DIR_DIR_H
#define PAF_DIR_DIR_H

#include <stdint.h>
#include <scetypes.h>
#include <std/string>

#define SCE_PAF_ERROR_DIR_INVALID_ARGUMENT	(-2135992063)	/* 0x80AF5901 */
#define SCE_PAF_ERROR_DIR_NO_MORE_ENTRY		(-2135992062)	/* 0x80AF5902 */

namespace paf {

class DirEnt {
public:
	enum Type {
		Type_Dir = 0,
		Type_File = 1,
		Type_Reg = 2,
		Type_Lnk = 3
	};

	~DirEnt()
	{
	}

	Type type;
	string name;
	int64_t size;
};

class Dir {
public:
	Dir();
	~Dir()
	{
		Close();
	}

	int32_t Open(const char *dirname);
	void Close();
	int32_t Read(DirEnt *entry);

	static int32_t Create(const char *dirname, uint32_t mode);
	static int32_t Create(const char *dirname);
	static int32_t CreateRecursive(const char *dirname, uint32_t mode);
	static int32_t CreateRecursive(const char *dirname);
	static bool IsDirectory(const char *path);
	static bool IsExist(const char *path);
	static int32_t Remove(const char *path);
	static int32_t RemoveRecursive(const char *path);

	SceUID m_handle;
};

}

#endif
