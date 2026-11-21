/*
 * podl_cache.cpp: the podl cache (0x8113C278-0x8113E2E4), where an auto download's request is
 * kept until it is handed over: a folder per request under the user's "/podl/cache", its
 * info.dat (value, size and URL) and its icon made a 64x64 DXT3 DDS; at most 32 entries,
 * the oldest removed first. With the lock on "ur0:temp/podl", the downloads and file reads
 * and writes it is made with (tried again when the transfer is cut off) and the DRM set that
 * enables or disables a cached product. The file's name and every name in it are ours
 * (PLACEHOLDER): no record or message names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <kernel/iofilemgr.h>
#include <appmgr/appmgr.h>
#include <vshbridge/vshbridge.h>
#include <common/shared_ptr.h>
#include <common/string_util.h>
#include <buffer/buffer.h>
#include <file/localfile.h>
#include <file/httpfile.h>
#include <dir/dir.h>
#include <image/image.h>
#include <framework.h>

#include "download_service.h"

using namespace paf;

extern "C" int SceNpManager_DDF30B0E(char *loginId, void *password, void *reserved);

/* the cache's errors (PLACEHOLDER names) */
#define PODL_ERROR_NO_MEMORY		((int)0x80107301)
#define PODL_ERROR_INVALID_ARG		((int)0x8010731F)	/* no id, or no such file */
#define PODL_ERROR_IMAGE		((int)0x80107320)	/* the icon could not be made */
#define PODL_ERROR_TOO_LARGE		((int)0x80107321)

/* paf::HttpFile's errors (values only): the size not given, and the transfer cut off */
#define PODL_HTTP_NO_LENGTH		-2135994287	/* 0x80AF5051 */
#define PODL_HTTP_NO_LENGTH2		-2135994286	/* 0x80AF5052 */
#define PODL_HTTP_ABORTED		-2135994332	/* 0x80AF5024 */

#define PODL_CACHE_DIR			"/podl/cache"
#define PODL_CACHE_MAX			32
#define PODL_TEMP_DIR			"ur0:temp/podl"
#define PODL_LOCK_FILE			"ur0:temp/podl/lock.tmp"
#define PODL_ICON_TEMP			"ur0:temp/podl/icon.tmp"
#define PODL_ICON_SIZE			64
#define PODL_ICON_MAX_SIZE		0x80000
#define PODL_COPY_SIZE			0x10000
#define PODL_RETRIES			3

/* vshIoFlock's operations (PLACEHOLDER names) */
#define PODL_FLOCK_EX			2
#define PODL_FLOCK_UN			0x10

/* a temporary file, removed when it goes (PLACEHOLDER names); the constructor and the getter
   are folded by Sony's linker with other code like them (0x811FEEC8, 0x8124E288) */
class PodlTempFile {
public:
	__attribute__((noinline)) PodlTempFile(const char *path) : m_path(path)
	{
	}

	~PodlTempFile();

	__attribute__((noinline)) const char *GetPath(void)
	{
		return m_path;
	}

	const char *m_path;
};

/* the request's id is older than the one in oldest: its value first, then its id */
static inline bool podlCacheIsOlder(DownloadServiceRequest *request,
	DownloadServiceRequest *oldest)
{
	if (*request->GetSize() < *oldest->GetSize())
		return true;
	if (*request->GetSize() == *oldest->GetSize()) {
		const string *id = request->GetId();
		const string *oldestId = oldest->GetId();
		const char *a = id->c_str();
		const char *b = oldestId->c_str();

		/* paf::string's operator<: the first characters, then strcmp */
		if ((SceUInt8)a[0] < (SceUInt8)b[0] || sce_paf_strcmp(a, b) < 0)
			return true;
	}
	return false;
}

/* the cache entry of the id removed */
int Bgdl_8113C278(const char *id)
{
	string path(PODL_CACHE_DIR);
	int ret = Bgdl_8113D5CC(&path);

	if (ret < 0)
		return ret;
	path.append(1, '/');
	path += id;
	Dir::RemoveRecursive(path.c_str());
	return 0;
}

/* with the cache full, the oldest entry removed to make room for the request; *full when the
   request is older than every entry */
int Bgdl_8113C368(DownloadServiceRequest *request, bool *full)
{
	int count = 0;
	int ret;

	*full = false;

	DownloadServiceRequest entry;
	DownloadServiceRequest oldest;

	{
		PodlCacheIterator it;

		for (;;) {
			ret = it.Next(&entry, false);
			if (ret < 0)
				return ret;
			if (it.m_index < 0)
				break;
			if (oldest.GetId()->size() == 0 || podlCacheIsOlder(&entry, &oldest)) {
				oldest.SetId(entry.GetId()->c_str());
				oldest.SetSize(entry.GetSize());
			}
			count++;
		}
	}
	if (count < PODL_CACHE_MAX)
		return 0;
	if (podlCacheIsOlder(request, &oldest)) {
		*full = true;
		return 0;
	}
	ret = Bgdl_8113C278(oldest.GetId()->c_str());
	if (ret < 0)
		return ret;
	return 0;
}

int Bgdl_8113CCF6(const char *url, const char *path, const SceUInt64 *maxSize);
int Bgdl_8113D8C2(const char *path, const void *buf, SceSize size, SceUInt32 flags,
		  SceUInt32 mode);
int Bgdl_8113DF7A(const char *src, const char *dst);

/* the request kept in the cache: its info.dat, and its icon (downloaded first when only its
   URL is known); with autoDownload not when the cache is full of newer entries */
int Bgdl_8113C5B2(DownloadServiceRequest *request, bool autoDownload)
{
	int ret;

	if (request->GetId()->size() == 0)
		return PODL_ERROR_INVALID_ARG;
	if (autoDownload) {
		bool full;

		ret = Bgdl_8113C368(request, &full);
		if (ret < 0)
			return ret;
		if (full)
			return 0;
	}

	string path(PODL_CACHE_DIR);

	ret = Bgdl_8113D5CC(&path);
	if (ret < 0)
		return ret;
	path.append(1, '/');
	path.append(*request->GetId());
	if (!Dir::IsExist(path.c_str())) {
		ret = Dir::Create(path.c_str());
		if (ret < 0)
			return ret;
	}

	string icon = path + "/icon.dds";
	PodlCacheInfo info = {0};

	info.value = *request->GetValue();
	info.size = *request->GetSize();
	sce_paf_strncpy(info.url, request->GetUrl()->c_str(), sizeof(info.url));
	path += "/info.dat";
	ret = Bgdl_8113D8C2(path.c_str(), &info, PODL_CACHE_INFO_SIZE,
			    SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 6);
	if (ret < 0)
		return ret;
	if (request->GetName()->size() == 0 ||
	    icon.c_str()[0] != request->GetName()->c_str()[0] ||
	    sce_paf_strcmp(icon.c_str(), request->GetName()->c_str()) != 0) {
		PodlTempFile temp(PODL_ICON_TEMP);

		if (request->GetName()->size() == 0 && request->GetIconUrl()->size() != 0) {
			const char *url = request->GetIconUrl()->c_str();
			SceUInt64 maxSize = PODL_ICON_MAX_SIZE;

			if (Bgdl_8113CCF6(url, temp.GetPath(), &maxSize) < 0)
				return 0;
			request->SetName(temp.GetPath());
		}
		if (request->GetName()->size() != 0) {
			if (Bgdl_8113DF7A(request->GetName()->c_str(), icon.c_str()) < 0)
				return 0;
			request->SetName(icon.c_str());
		}
	}
	return 0;
}

/* the request's cache entry removed */
int Bgdl_8113C9B4(DownloadServiceRequest *request)
{
	int ret = Bgdl_8113C278(request->GetId()->c_str());

	if (ret < 0)
		return ret;
	return 0;
}

PodlCacheLock::PodlCacheLock()
{
	m_fd = -1;
}

PodlCacheLock::~PodlCacheLock()
{
	if (m_fd >= 0) {
		vshIoFlock(m_fd, PODL_FLOCK_UN);
		sceIoClose(m_fd);
	}
}

int PodlCacheLock::Lock(void)
{
	int ret;

	if (!Dir::IsExist(PODL_TEMP_DIR)) {
		ret = Dir::CreateRecursive(PODL_TEMP_DIR, 6);
		if (ret < 0)
			return ret;
	}
	ret = m_fd = sceIoOpen(PODL_LOCK_FILE, SCE_O_RDWR | SCE_O_CREAT, 6);
	if (ret < 0)
		return ret;
	ret = vshIoFlock(m_fd, PODL_FLOCK_EX);
	if (ret < 0)
		return ret;
	return 0;
}

/* url opened in *file, and its size in *size (0 when the server does not give it) */
int Bgdl_8113CA44(common::SharedPtr<File> *file, const char *url, SceUInt64 *size)
{
	int err;

	*file = HttpFile::Open(url, &err, 0);
	if (err < 0)
		return err;

	SceInt64 length = (*file)->GetFileSize();

	if (length >= 0) {
		*size = length;
		return 0;
	}
	if (length == PODL_HTTP_NO_LENGTH || length == PODL_HTTP_NO_LENGTH2) {
		*size = 0;
		return 0;
	}
	return (int)length;
}

/*
 * src copied to dst through buf, at most *size bytes, or *maxSize when the size is not known
 * (then *size is left what came); *aborted when the transfer was cut off
 */
int Bgdl_8113CBAC(common::SharedPtr<File> src, common::SharedPtr<File> dst, SceUInt64 *size,
		  common::SharedPtr<MallocBuffer> buf, const SceUInt64 *maxSize, bool *aborted)
{
	int chunk;
	int ret;

	*aborted = false;
	chunk = buf->GetSize();

	SceUInt64 left = *size;

	if (left == 0)
		left = *maxSize;
	if (left != 0) {
		for (;;) {
			if (left < (SceUInt32)chunk)
				chunk = left;

			int n = src->Read(buf->GetAddr(), chunk);

			if (n == PODL_HTTP_ABORTED) {
				*aborted = true;
				return 0;
			}
			if (n == 0)
				break;
			if (n < 0)
				return n;
			ret = dst->Write(buf->GetAddr(), n);
			if (ret == PODL_HTTP_ABORTED) {
				*aborted = true;
				return 0;
			}
			if (n != ret)
				return ret;
			left -= n;
			if (left == 0)
				goto done;
		}
		if (*size == 0)
			*size = *maxSize - left;
	}
done:
	if (*size != 0)
		return 0;
	return PODL_ERROR_TOO_LARGE;
}

/* url downloaded to path, at most *maxSize bytes; tried again when cut off */
int Bgdl_8113CCF6(const char *url, const char *path, const SceUInt64 *maxSize)
{
	common::SharedPtr<MallocBuffer> buf = MallocBuffer::Allocate(PODL_COPY_SIZE);

	if (buf->GetAddr() == NULL)
		return PODL_ERROR_NO_MEMORY;
	{
		string dir = common::StripFilename(string(path), string("P"));

		if (dir.size() != 0 && !Dir::IsExist(dir.c_str())) {
			int ret = Dir::CreateRecursive(dir.c_str(), 6);

			if (ret < 0)
				return ret;
		}
	}
	for (int retry = 0; retry < PODL_RETRIES; retry++) {
		common::SharedPtr<File> http;
		SceUInt64 size;
		int ret = Bgdl_8113CA44(&http, url, &size);

		if (ret < 0)
			return ret;
		if (size > *maxSize)
			return PODL_ERROR_TOO_LARGE;

		int err;
		common::SharedPtr<File> out = LocalFile::Open(path, SCE_O_WRONLY | SCE_O_CREAT |
							      SCE_O_TRUNC, 6, &err);
		bool aborted;

		if (err < 0)
			return err;
		ret = Bgdl_8113CBAC(http, out, &size, buf, maxSize, &aborted);
		if (ret < 0)
			return ret;
		if (!aborted)
			break;
		http->Close();
		out->Close();
	}
	return 0;
}

/* the user's folder put before path, and the folder made */
int Bgdl_8113D5CC(string *path)
{
	char dir[32] = "";
	int ret;

	ret = sceAppMgrGetUserDirPath(1, dir, sizeof(dir));
	if (ret < 0)
		return ret;
	*path = dir + *path;
	if (!Dir::IsExist(path->c_str())) {
		ret = Dir::CreateRecursive(path->c_str(), 6);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* size bytes of the file at path read into buf; tried again when cut off */
int Bgdl_8113D6F8(const char *path, void *buf, SceSize size)
{
	int ret;

	for (int retry = 0; retry < PODL_RETRIES; retry++) {
		common::SharedPtr<File> file = LocalFile::Open(path, SCE_O_RDONLY, 0, &ret);

		if (ret < 0)
			return ret;
		ret = file->Read(buf, size);
		if (ret != PODL_HTTP_ABORTED) {
			if (ret >= 0)
				return ret;
			return ret;
		}
		file->Close();
	}
	return ret;
}

/* size bytes of buf written to the file at path; tried again when cut off */
int Bgdl_8113D8C2(const char *path, const void *buf, SceSize size, SceUInt32 flags,
		  SceUInt32 mode)
{
	int ret;

	for (int retry = 0; retry < PODL_RETRIES; retry++) {
		common::SharedPtr<File> file = LocalFile::Open(path, flags, mode, &ret);

		if (ret < 0)
			return ret;
		ret = file->Write(buf, size);
		if (ret != PODL_HTTP_ABORTED) {
			if (ret == (int)size)
				return ret;
			return ret;
		}
		file->Close();
	}
	return ret;
}

/* the image at path decoded at extent's size and made of mode, in *image and *pixels */
int Bgdl_8113DA98(const char *path, ImageMode mode, image::ImageExtent extent,
		  common::SharedPtr<image::Image> *image, image::ImageBuffer *pixels)
{
	if (!LocalFile::IsFile(path))
		return PODL_ERROR_INVALID_ARG;

	memory::HeapAllocator *heap = NULL;
	Framework *fw = Framework::Instance();

	if (fw != NULL)
		heap = fw->GetDecodeHeap();

	common::SharedPtr<image::Image> img = image::Image::Open(path, heap,
							      image::ImageFormat_None, 0, 0);

	if (img.get() == NULL)
		return PODL_ERROR_IMAGE;
	if (!img->SetDecExtent(extent, image::ImageResizeType_NearestNeighbor))
		return PODL_ERROR_IMAGE;

	common::SharedPtr<image::Image> rgba;

	if (img->GetMode() == ImageMode_RGBA8888)
		rgba = img;
	else
		rgba = img->ConvertMode(ImageMode_RGBA8888, image::ImageConvertModeType_Default);
	if (rgba.get() == NULL)
		return PODL_ERROR_IMAGE;
	*image = rgba->ConvertMode(mode, image::ImageConvertModeType_Default);
	if (image->get() == NULL)
		return PODL_ERROR_IMAGE;
	*pixels = (*image)->ToBuffer(true);
	if (pixels->buff == NULL)
		return PODL_ERROR_IMAGE;
	return 0;
}

/* the image at src saved at dst as the cache's icon, a 64x64 DXT3 DDS */
int Bgdl_8113DF7A(const char *src, const char *dst)
{
	image::ImageExtent extent(PODL_ICON_SIZE, PODL_ICON_SIZE);
	common::SharedPtr<image::Image> image;
	image::ImageBuffer pixels;
	int ret = Bgdl_8113DA98(src, ImageMode_DXT3, extent, &image, &pixels);

	if (ret < 0)
		return ret;
	{
		string dir = common::StripFilename(string(dst), string("P"));

		if (dir.size() != 0 && !Dir::IsExist(dir.c_str())) {
			ret = Dir::CreateRecursive(dir.c_str(), 6);
			if (ret < 0)
				return ret;
		}
	}
	if (image::Image::SaveAsDds(dst, pixels.buff, extent.w, extent, ImageMode_DXT3, 6, NULL) ==
	    0)
		return PODL_ERROR_IMAGE;
	return 0;
}

PodlTempFile::~PodlTempFile()
{
	if (LocalFile::IsFile(m_path))
		LocalFile::RemoveFile(m_path);
}

/* the DRM set sent for the cached product id, enabled */
int Bgdl_8113E22C(const char *id)
{
	char reserved[8];
	char password[36];
	char loginId[68];
	int ret;

	if (id == NULL || id[0] == '\0')
		return PODL_ERROR_INVALID_ARG;
	ret = SceNpManager_DDF30B0E(loginId, password, reserved);
	if (ret < 0)
		return ret;
	ret = Bgdl_8113B456(loginId, id, 1);
	if (ret < 0)
		return ret;
	return 0;
}

/* and disabled */
int Bgdl_8113E288(const char *id)
{
	char reserved[8];
	char password[36];
	char loginId[68];
	int ret;

	if (id == NULL || id[0] == '\0')
		return PODL_ERROR_INVALID_ARG;
	ret = SceNpManager_DDF30B0E(loginId, password, reserved);
	if (ret < 0)
		return ret;
	ret = Bgdl_8113B456(loginId, id, 0);
	if (ret < 0)
		return ret;
	return 0;
}
