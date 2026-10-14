#include <sce/libkernel_backends.h>
#include <psp2common/kernel/iofilemgr.h>

/* The kernel copies these reserved words but does not use their values. */
struct IoPathOptions {
	SceUInt reserved[2];
};

struct IoRenameOptions {
	SceUInt reserved[4];
};

struct IoPathAsyncOptions {
	void *result;
	SceUInt reserved[3];
};


SceUID sceIoOpen(const char *path, int flags, SceMode mode)
{
	struct IoPathOptions options;

	return _sceIoOpen(path, flags, mode, &options);
}

int sceIoRemove(const char *path)
{
	struct IoPathOptions options;

	return _sceIoRemove(path, &options);
}

int sceIoRename(const char *oldPath, const char *newPath)
{
	struct IoRenameOptions options;

	return _sceIoRename(oldPath, newPath, &options);
}

int sceIoMkdir(const char *path, SceMode mode)
{
	struct IoPathOptions options;

	return _sceIoMkdir(path, mode, &options);
}

int sceIoRmdir(const char *path)
{
	struct IoPathOptions options;

	return _sceIoRmdir(path, &options);
}

SceUID sceIoDopen(const char *path)
{
	struct IoPathOptions options;

	return _sceIoDopen(path, &options);
}

int sceIoGetstat(const char *path, SceIoStat *status)
{
	struct IoPathOptions options;

	return _sceIoGetstat(path, status, &options);
}

int sceIoChstat(const char *path, const SceIoStat *status, int bits)
{
	struct IoPathOptions options;

	return _sceIoChstat(path, status, bits, &options);
}

int sceIoSync(const char *device, unsigned int flags)
{
	struct IoPathOptions options;

	return _sceIoSync(device, flags, &options);
}

int sceIoOpenAsync(const char *path, int flags, SceMode mode, void *result)
{
	struct IoPathAsyncOptions options;

	options.result = result;
	return _sceIoOpenAsync(path, flags, mode, &options);
}

int sceIoRemoveAsync(const char *path, void *result)
{
	struct IoPathOptions options;

	return _sceIoRemoveAsync(path, result, &options);
}

int sceIoMkdirAsync(const char *path, SceMode mode, void *result)
{
	struct IoPathOptions options;

	return _sceIoMkdirAsync(path, mode, result, &options);
}

int sceIoRmdirAsync(const char *path, void *result)
{
	struct IoPathOptions options;

	return _sceIoRmdirAsync(path, result, &options);
}

int sceIoRenameAsync(const char *oldPath, const char *newPath, void *result)
{
	struct IoRenameOptions options;

	return _sceIoRenameAsync(oldPath, newPath, result, &options);
}

int sceIoChstatAsync(const char *path, const SceIoStat *status, int bits,
		void *result)
{
	struct IoPathAsyncOptions options;

	options.result = result;
	return _sceIoChstatAsync(path, status, bits, &options);
}

int sceIoGetstatAsync(const char *path, SceIoStat *status, void *result)
{
	struct IoPathOptions options;

	return _sceIoGetstatAsync(path, status, result, &options);
}

int sceIoSyncAsync(const char *device, unsigned int flags, void *result)
{
	struct IoPathOptions options;

	return _sceIoSyncAsync(device, flags, result, &options);
}
