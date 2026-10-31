/*
 * paf::Dir: directory handles and directory operations on paths.
 *
 * RemoveRecursive walks the tree depth first with a stack of open directories
 * (DirStack); the full path of the directory on top is rebuilt from the names
 * on the stack whenever the stack changes.
 */
#include <dir/dir.h>
#include <std/list>
#include <std/string.h>
#include <kernel/iofilemgr.h>

extern "C" {
SceUID sceIoDopen(const char *dirname);
int sceIoDread(SceUID fd, SceIoDirent *dir);
int sceIoDclose(SceUID fd);
int sceIoMkdir(const char *dir, SceMode mode);
int sceIoRmdir(const char *dir);
int sceIoRemove(const char *file);
int sceIoGetstat(const char *file, SceIoStat *stat);
}

/* the mode Create and CreateRecursive use when given none */
#define DIR_DEFAULT_MODE	6

namespace paf {

void Dir::Close()
{
	if (m_handle != -1) {
		sceIoDclose(m_handle);
		m_handle = -1;
	}
}

int32_t Dir::Open(const char *dirname)
{
	SceUID fd = sceIoDopen(dirname);

	if (fd < 0)
		return fd;
	m_handle = fd;
	return 0;
}

Dir::Dir()
{
	m_handle = -1;
}

bool Dir::IsDirectory(const char *path)
{
	SceIoStat stat;

	return sceIoGetstat(path, &stat) == 0 && (stat.st_mode & SCE_STM_FDIR) != 0;
}

bool Dir::IsExist(const char *path)
{
	SceIoStat stat;

	return sceIoGetstat(path, &stat) == 0;
}

int32_t Dir::Remove(const char *path)
{
	if (path == NULL)
		return SCE_PAF_ERROR_DIR_INVALID_ARGUMENT;
	if (!IsExist(path))
		return 0;
	if (IsDirectory(path))
		return sceIoRmdir(path);
	return sceIoRemove(path);
}

int32_t Dir::Read(DirEnt *entry)
{
	if (entry == NULL)
		return SCE_PAF_ERROR_DIR_INVALID_ARGUMENT;
	if (m_handle == -1)
		return SCE_PAF_ERROR_DIR_INVALID_ARGUMENT;

	SceIoDirent dirent = {0};
	int32_t result = sceIoDread(m_handle, &dirent);

	if (result < 0)
		return result;
	if (result == 0)
		return SCE_PAF_ERROR_DIR_NO_MORE_ENTRY;
	if ((dirent.d_stat.st_mode & SCE_STM_FDIR) != 0)
		entry->type = DirEnt::Type_Dir;
	else if ((dirent.d_stat.st_mode & SCE_STM_FREG) != 0)
		entry->type = DirEnt::Type_File;
	else if ((dirent.d_stat.st_mode & 0x4000) != 0)
		entry->type = DirEnt::Type_Reg;
	else
		entry->type = DirEnt::Type_Lnk;
	entry->name = dirent.d_name;
	entry->size = dirent.d_stat.st_size;
	return 0;
}

int32_t Dir::Create(const char *dirname, uint32_t mode)
{
	return sceIoMkdir(dirname, mode);
}

int32_t Dir::Create(const char *dirname)
{
	return sceIoMkdir(dirname, DIR_DEFAULT_MODE);
}

int32_t Dir::CreateRecursive(const char *dirname, uint32_t mode)
{
	if (dirname != NULL && *dirname != '\0') {
		const char *sep = dirname + 1;

		while ((sep = sce_paf_strchr(sep, '/')) != NULL) {
			string parent(dirname, sep - dirname);

			sceIoMkdir(parent.c_str(), mode);
			++sep;
		}
	}
	return sceIoMkdir(dirname, mode);
}

int32_t Dir::CreateRecursive(const char *dirname)
{
	if (dirname != NULL && *dirname != '\0') {
		const char *sep = dirname + 1;

		while ((sep = sce_paf_strchr(sep, '/')) != NULL) {
			string parent(dirname, sep - dirname);

			sceIoMkdir(parent.c_str(), DIR_DEFAULT_MODE);
			++sep;
		}
	}
	return sceIoMkdir(dirname, DIR_DEFAULT_MODE);
}

/* An open directory on the walk and its name. */
class DirStackEntry {
public:
	~DirStackEntry()
	{
	}

	Dir dir;
	string name;
};

/* Sony's build inlines this list's destructor (the generic one stays a call). */
template<>
inline list<DirStackEntry>::~list()
{
	if (head != NULL) {
		_Node *node = head->next;

		if (node != head) {
			do {
				_Node *next = node->next;

				node->value.~DirStackEntry();
				operator delete(node);
				node = next;
			} while (node != head);
		}
		operator delete(head);
	}
	head = (_Node *)-1;
	count = (size_type)-1;
}

/* The directories RemoveRecursive has entered, the deepest last. */
class DirStack {
public:
	~DirStack()
	{
	}

	/* path = the names on the stack joined by '/' */
	void UpdatePath()
	{
		string joined;

		for (list<DirStackEntry>::iterator it = stack.begin(); it != stack.end(); ++it)
			joined.append(it->name + '/');
		if (joined.size() != 0)
			path.assign(joined.c_str(), joined.size() - 1);
		else
			path.clear();
	}

	int32_t Push(const char *name)
	{
		stack.push_back(DirStackEntry());

		DirStackEntry &top = stack.back();

		top.name = name;
		UpdatePath();

		int32_t result = top.dir.Open(path.c_str());

		if (result < 0) {
			stack.pop_back();
			UpdatePath();
			return result;
		}
		return 0;
	}

	int32_t Pop()
	{
		if (stack.size() == 0)
			return 0;
		stack.back().dir.Close();

		int32_t result = Dir::Remove(path.c_str());

		stack.pop_back();
		UpdatePath();
		return result;
	}

	list<DirStackEntry> stack;
	string path;
};

static const char s_current[] = ".";
static const char s_parent[] = "..";

static int32_t RemoveTree(const char *path)
{
	if (!Dir::IsExist(path))
		return 0;
	if (Dir::IsDirectory(path)) {
		DirStack dirs;
		int32_t result = dirs.Push(path);

		if (result < 0)
			return result;

		DirEnt entry;

		while (dirs.stack.size() != 0) {
			result = dirs.stack.back().dir.Read(&entry);
			if (result == SCE_PAF_ERROR_DIR_NO_MORE_ENTRY) {
				result = dirs.Pop();
				if (result < 0)
					return result;
				continue;
			}
			if (result < 0)
				return result;
			if (entry.name == s_current || entry.name == s_parent)
				continue;

			string child = dirs.path + '/' + entry.name;

			if (Dir::IsDirectory(child.c_str())) {
				result = dirs.Push(entry.name.c_str());
				if (result < 0)
					return result;
			} else {
				result = Dir::Remove(child.c_str());
				if (result < 0)
					return result;
			}
		}
		return 0;
	}
	return Dir::Remove(path);
}

int32_t Dir::RemoveRecursive(const char *path)
{
	return RemoveTree(path);
}

}
