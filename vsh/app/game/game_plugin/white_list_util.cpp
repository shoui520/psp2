/*
 * The white lists' file helpers, under Sony's "SceGameUpdateWhiteListUtil" lock: a file's
 * HMAC-SHA256 (keyed by the system from a 16-character key) written or checked, a list
 * downloaded, a list package unwrapped by the update service, the teleport list's header read,
 * tm0:teleport's force flag, and folder and file copies. The file and function names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <libhmac.h>
#include <update_mgr/update_mgr.h>
#include <modulemgr/certified_file.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <common/string_util.h>
#include <thread/thread.h>
#include <buffer/buffer.h>
#include <file/localfile.h>
#include <file/httpfile.h>
#include <dir/dir.h>

#include "white_list.h"

using namespace paf;

/* the key the system makes a MAC key from, and the key it gives back */
typedef struct GameWhiteListAppKey {
	SceSize size;
	SceUInt8 key[32];
} GameWhiteListAppKey;

extern "C" int vshSblSsGenerateAppKey(const GameWhiteListAppKey *src, GameWhiteListAppKey *key);

/* the list packages the update service unwraps, and how it is asked */
#define WHITE_LIST_SPKG_TYPE		27
#define WHITE_LIST_SPKG_FLAGS		9
#define WHITE_LIST_SPKG_NODE		3
#define WHITE_LIST_SPKG_DONE		5	/* the request's progress when it has ended */
#define WHITE_LIST_SPKG_POLL_USEC	500000

/* paf::HttpFile's errors (values only): the size not given, and the transfer cut off */
#define WHITE_LIST_HTTP_NO_LENGTH	-2135994287	/* 0x80AF5051 */
#define WHITE_LIST_HTTP_NO_LENGTH2	-2135994286	/* 0x80AF5052 */
#define WHITE_LIST_HTTP_ABORTED		-2135994332	/* 0x80AF5024 */

#define WHITE_LIST_DOWNLOAD_RETRIES	3
#define WHITE_LIST_COPY_SIZE		0x10000
#define WHITE_LIST_MAC_BUF_SIZE		4096
#define WHITE_LIST_MAC_SIZE		32
#define WHITE_LIST_TELEPORT_MIN_SIZE	55

#define WHITE_LIST_TELEPORT_DIR		"tm0:teleport"
#define WHITE_LIST_TELEPORT_FORCE	"tm0:teleport/force.dat"
#define WHITE_LIST_TELEPORT_LIST	"tm0:teleport/list_launch_teleport.dat"
#define WHITE_LIST_TELEPORT_OLD		"tm0:teleport/list_launch_teleport.old"
#define WHITE_LIST_TELEPORT_TMP		"tm0:teleport/list_launch_teleport.tmp"

static thread::RMutex s_utilMutex("SceGameUpdateWhiteListUtil", 0);

/*
 * The HMAC-SHA256 of file read through buf, with the macSize bytes at macOffset taken as zero;
 * the MAC goes to mac, and with verify it is compared with the one the file holds there
 * (0x810A5744).
 */
static __attribute__((noinline)) int gameWhiteListFileMac(common::SharedPtr<File> file,
	const char *key, void *buf, SceSize bufSize, SceUInt32 macOffset, char *mac,
	SceSize macSize, bool verify)
{
	SceInt64 total = 0;

	if (file.get() == NULL || key == NULL || buf == NULL || mac == NULL)
		return GAME_WHITE_LIST_ERROR_BAD_FILE;
	if (!file->IsOpened())
		return GAME_WHITE_LIST_ERROR_NO_FILE;

	SceInt64 size = file->GetFileSize();

	if (size < 0)
		return (int)size;

	SceInt64 macLength = macSize;
	SceInt64 macStart = macOffset;

	if (size - macLength < macStart)
		return GAME_WHITE_LIST_ERROR_BAD_FILE;

	SceUInt32 macEnd = macSize + macOffset + 1;
	GameWhiteListAppKey src;
	GameWhiteListAppKey appKey;

	sce_paf_memset(&src, 0, sizeof(src));
	sce_paf_memset(&appKey, 0, sizeof(appKey));
	src.size = 16;
	sce_paf_memcpy(src.key, key, 16);

	int ret = vshSblSsGenerateAppKey(&src, &appKey);

	if (ret < 0)
		return ret;

	SceHmacSha256Context *ctx = new SceHmacSha256Context;
	char *stored = NULL;

	if (ctx == NULL) {
		ret = GAME_WHITE_LIST_ERROR_NO_MEMORY;
	} else {
		ret = sceHmacSha256BlockInit(ctx, appKey.key, appKey.size);
		if (ret >= 0) {
			stored = new char[macSize];
			if (stored == NULL)
				ret = GAME_WHITE_LIST_ERROR_NO_MEMORY;
		}
	}

	if (ret >= 0) {
		sce_paf_memset(stored, 0, macSize);
		sce_paf_memset(mac, 0, macSize);

		SceInt64 first = macOffset + 1;
		SceInt64 last = macEnd;
		SceInt64 length = bufSize;

		for (;;) {
			sce_paf_memset(buf, 0, bufSize);

			int n = file->Read(buf, bufSize);

			if (n < 0) {
				ret = n;
				break;
			}
			if (n == 0)
				break;

			SceInt64 start = total;

			total += (SceUInt32)n;
			if (start <= first && last <= total) {
				/* the whole MAC in this block */
				if (macStart < start) {
					ret = GAME_WHITE_LIST_ERROR_MAC_RANGE;
					break;
				}

				char *p = (char *)buf + (macStart - start);

				sce_paf_memcpy(stored, p, macSize);
				sce_paf_memset(p, 0, macSize);
			} else if (start <= first && first <= total && total <= last) {
				/* the MAC's start at this block's end */
				SceInt64 part = total - macStart;

				if (length < part || part < 0) {
					ret = GAME_WHITE_LIST_ERROR_MAC_RANGE;
					break;
				}

				char *p = (char *)buf + (length - part);

				sce_paf_memcpy(stored, p, part);
				sce_paf_memset(p, 0, part);
			} else if (first <= start && start <= last && last <= total) {
				/* the MAC's end at this block's start */
				SceInt64 done = start - macStart;

				if (macLength < done || done < 0) {
					ret = GAME_WHITE_LIST_ERROR_MAC_RANGE;
					break;
				}

				SceSize rest = macSize - done;

				sce_paf_memcpy(stored + done, buf, rest);
				sce_paf_memset(buf, 0, rest);
			}

			SceSize len = n;

			if ((SceSize)n < bufSize)
				len = (n & ~7) + 8;
			ret = sceHmacSha256BlockUpdate(ctx, buf, len);
			if (ret < 0)
				break;
		}
		if (ret >= 0) {
			ret = sceHmacSha256BlockResult(ctx, (SceUChar8 *)mac);
			if (ret >= 0 && verify && sce_paf_strncmp(stored, mac, macSize) != 0)
				ret = GAME_WHITE_LIST_ERROR_BAD_MAC;
		}
	}

	if (ctx != NULL)
		delete ctx;
	if (stored != NULL)
		delete[] stored;
	return ret;
}

/* the file's MAC written at macOffset (0x810A5AD6) */
int gameWhiteListFileSign(common::SharedPtr<File> file, const char *key, SceUInt32 macOffset)
{
	char mac[WHITE_LIST_MAC_SIZE];

	if (file.get() == NULL || key == NULL)
		return GAME_WHITE_LIST_ERROR_BAD_FILE;
	if (!file->IsOpened())
		return GAME_WHITE_LIST_ERROR_BAD_FILE;

	char *buf = new char[WHITE_LIST_MAC_BUF_SIZE];
	int ret;

	if (buf == NULL) {
		ret = GAME_WHITE_LIST_ERROR_NO_MEMORY;
	} else {
		SceInt64 pos = file->Seek(0, SCE_SEEK_SET);

		if (pos < 0) {
			ret = (int)pos;
		} else {
			ret = gameWhiteListFileMac(file, key, buf, WHITE_LIST_MAC_BUF_SIZE, macOffset,
						   mac, sizeof(mac), false);
			if (ret >= 0) {
				pos = file->Seek(macOffset, SCE_SEEK_SET);
				if (pos < 0) {
					ret = (int)pos;
				} else {
					int n = file->Write(mac, sizeof(mac));

					if (n < 0)
						ret = n;
					else
						file->Flush();
				}
			}
		}
	}
	if (buf != NULL)
		delete[] buf;
	return ret;
}

/* the file's MAC checked against the one at macOffset (0x810A5C54) */
int gameWhiteListFileVerify(common::SharedPtr<File> file, const char *key, SceUInt32 macOffset)
{
	char mac[WHITE_LIST_MAC_SIZE];

	if (file.get() == NULL || key == NULL)
		return GAME_WHITE_LIST_ERROR_BAD_FILE;
	if (!file->IsOpened())
		return GAME_WHITE_LIST_ERROR_BAD_FILE;

	char *buf = new char[WHITE_LIST_MAC_BUF_SIZE];
	int ret;

	if (buf == NULL) {
		ret = GAME_WHITE_LIST_ERROR_NO_MEMORY;
	} else {
		file->Seek(0, SCE_SEEK_SET);
		ret = gameWhiteListFileMac(file, key, buf, WHITE_LIST_MAC_BUF_SIZE, macOffset, mac,
					   sizeof(mac), true);
	}
	if (buf != NULL)
		delete[] buf;
	return ret;
}

/*
 * src copied to dst through buf, at most *size bytes, or *maxSize when the size is not known
 * (then *size is left what came); *aborted when the transfer was cut off
 */
static inline int gameWhiteListCopyStream(common::SharedPtr<File> src,
	common::SharedPtr<File> dst, common::SharedPtr<MallocBuffer> buf, SceUInt64 *size,
	const SceUInt64 *maxSize, bool *aborted)
{
	int chunk = buf->GetSize();
	int ret;

	*aborted = false;

	SceUInt64 left = *size;

	if (left == 0)
		left = *maxSize;
	if (left != 0) {
		for (;;) {
			if (left < (SceUInt32)chunk)
				chunk = left;

			int n = src->Read(buf->GetAddr(), chunk);

			if (n == WHITE_LIST_HTTP_ABORTED) {
				*aborted = true;
				return 0;
			}
			if (n == 0)
				break;
			if (n < 0)
				return n;

			ret = dst->Write(buf->GetAddr(), n);
			if (ret == WHITE_LIST_HTTP_ABORTED) {
				*aborted = true;
				return 0;
			}
			if (n != ret)
				return ret;
			left -= n;
			if (left == 0)
				return *size != 0 ? 0 : GAME_WHITE_LIST_ERROR_TOO_LARGE;
		}
		if (*size == 0)
			*size = *maxSize - left;
	}
	return *size != 0 ? 0 : GAME_WHITE_LIST_ERROR_TOO_LARGE;
}

/* url downloaded to path, at most *maxSize bytes; tried again when cut off (0x810A5D6C) */
int gameWhiteListDownloadFile(const char *url, const char *path, const SceUInt64 *maxSize)
{
	common::SharedPtr<MallocBuffer> buf = MallocBuffer::Allocate(WHITE_LIST_COPY_SIZE);

	if (buf->GetAddr() == NULL)
		return GAME_WHITE_LIST_ERROR_NO_MEMORY;

	string dir = common::StripFilename(string(path), string("P"));

	if (dir.size() != 0 && !Dir::IsExist(dir.c_str())) {
		int ret = Dir::CreateRecursive(dir.c_str(), 6);

		if (ret < 0)
			return ret;
	}

	for (int retry = 0; retry < WHITE_LIST_DOWNLOAD_RETRIES; retry++) {
		SceUInt64 size = 0;
		int err = 0;
		common::SharedPtr<File> http = HttpFile::Open(url, &err, 0);
		bool aborted;
		int ret;

		if (err >= 0) {
			SceInt64 length = http->GetFileSize();

			if (length >= 0) {
				size = length;
				ret = 0;
			} else if (length == WHITE_LIST_HTTP_NO_LENGTH ||
				   length == WHITE_LIST_HTTP_NO_LENGTH2) {
				size = 0;
				ret = 0;
			} else {
				ret = (int)length;
			}
		} else {
			ret = err;
		}
		if (ret < 0)
			return ret;
		if (size > *maxSize)
			return GAME_WHITE_LIST_ERROR_TOO_LARGE;

		common::SharedPtr<File> out = LocalFile::Open(path, SCE_O_WRONLY | SCE_O_CREAT |
							      SCE_O_TRUNC, 6, &ret);

		if (ret < 0)
			return ret;
		ret = gameWhiteListCopyStream(http, out, buf, &size, maxSize, &aborted);
		if (ret < 0) {
			http->Close();
			out->Close();
			return ret;
		}
		if (!aborted)
			break;
		http->Close();
		out->Close();
	}
	return 0;
}

/*
 * The list package src unwrapped by the update service into dst, at most *maxSize bytes;
 * src removed afterwards when removeSource (0x810A68B6)
 */
int gameWhiteListFileUnwrap(const char *src, const char *dst, SceSize *maxSize,
			    bool removeSource)
{
	thread::RMutexLock lock(&s_utilMutex);
	LocalFileStat stat;
	void *package = NULL;
	uint32_t requestId = 0;
	uint32_t seqNo = 0;
	uint32_t result = 0;
	uint32_t progress = 0;
	uint32_t writtenRates = 0;
	int ret = LocalFile::Getstat(src, &stat);

	if (ret < 0)
		return ret;
	ret = sceSblUsAllocateBuffer((SceSize)stat.st_size, &package);
	if (ret != 0)
		return ret;

	common::SharedPtr<File> in;
	common::SharedPtr<File> out;

	in = LocalFile::Open(src, SCE_O_RDONLY, 0, &ret);

	if (in.get() == NULL) {
		ret = GAME_WHITE_LIST_ERROR_NO_FILE;
	} else {
		int n = in->Read(package, (SceSize)stat.st_size);

		if (n < 0) {
			ret = n;
		} else if (n != stat.st_size) {
			ret = GAME_WHITE_LIST_ERROR_SHORT_READ;
		} else {
			ret = sceSblUsInspectSpackage(WHITE_LIST_SPKG_TYPE, package,
						      (SceSize)stat.st_size,
						      WHITE_LIST_SPKG_FLAGS, &requestId);
		}
	}

	if (ret == 0) {
		for (;;) {
			ret = sceSblUsGetStatus(WHITE_LIST_SPKG_NODE, requestId, &seqNo, &result,
						&progress, &writtenRates);
			if (ret != 0)
				break;
			if (progress == WHITE_LIST_SPKG_DONE)
				break;
			sceKernelDelayThread(WHITE_LIST_SPKG_POLL_USEC);
		}
		if (ret == 0) {
			ret = result;
			if (result == 0) {
				const cf_header *header = (const cf_header *)package;
				const SpkgPackageInfo *info = (const SpkgPackageInfo *)
					((const char *)package + header->file_offset);
				const SpkgChunkInfo *chunk = (const SpkgChunkInfo *)(info + 1);
				void *data = sce_paf_malloc(*maxSize);

				if (data == NULL) {
					ret = GAME_WHITE_LIST_ERROR_NO_MEMORY;
				} else {
					ret = sceSblUsGetExtractSpackage(WHITE_LIST_SPKG_NODE,
									 requestId,
									 (uint32_t)info->decryptedSize,
									 *maxSize, data);
					if (ret == 0) {
						out = LocalFile::Open(dst,
								      SCE_O_WRONLY | SCE_O_CREAT |
								      SCE_O_TRUNC, 0606, &ret);
						if (out.get() == NULL) {
							ret = GAME_WHITE_LIST_ERROR_NO_FILE;
						} else {
							int n = out->Write(data,
									   (SceSize)chunk->size);

							if (n < 0)
								ret = n;
							else if (n != chunk->size)
								ret = GAME_WHITE_LIST_ERROR_SHORT_WRITE;
						}
					}
				}
				if (data != NULL)
					sce_paf_free(data);
			}
		}
	}

	if (package != NULL)
		sceSblUsReleaseBuffer(package);
	if (in.get() != NULL)
		in->Close();
	if (out.get() != NULL)
		out->Close();
	if (removeSource)
		LocalFile::RemoveFile(src);
	return ret;
}

/*
 * The list package in file unwrapped by the update service into buf; *size is buf's size,
 * then the list's (0x810A6D52)
 */
int gameWhiteListFileRead(common::SharedPtr<File> file, char *buf, SceSize *size)
{
	thread::RMutexLock lock(&s_utilMutex);

	if (file.get() == NULL)
		return GAME_WHITE_LIST_ERROR_BAD_FILE;
	if (!file->IsOpened())
		return GAME_WHITE_LIST_ERROR_NOT_OPENED;
	if (file->GetFileSize() > *size)
		return GAME_WHITE_LIST_ERROR_BAD_FILE;

	void *package = NULL;
	uint32_t requestId = 0;
	uint32_t seqNo = 0;
	uint32_t result = 0;
	uint32_t progress = 0;
	uint32_t writtenRates = 0;

	file->Seek(0, SCE_SEEK_SET);

	SceSize packageSize = file->GetFileSize();
	int ret = sceSblUsAllocateBuffer(packageSize, &package);

	if (ret != 0)
		return ret;

	int n = file->Read(package, packageSize);

	if (n < 0) {
		ret = n;
	} else if ((SceSize)n != packageSize) {
		ret = GAME_WHITE_LIST_ERROR_SHORT_READ;
	} else {
		ret = sceSblUsInspectSpackage(WHITE_LIST_SPKG_TYPE, package, packageSize,
					      WHITE_LIST_SPKG_FLAGS, &requestId);
		if (ret == 0) {
			for (;;) {
				ret = sceSblUsGetStatus(WHITE_LIST_SPKG_NODE, requestId, &seqNo,
							&result, &progress, &writtenRates);
				if (ret != 0)
					break;
				if (progress == WHITE_LIST_SPKG_DONE) {
					ret = result;
					if (result == 0) {
						const cf_header *header = (const cf_header *)package;
						const SpkgPackageInfo *info = (const SpkgPackageInfo *)
							((const char *)package + header->file_offset);
						const SpkgChunkInfo *chunk =
							(const SpkgChunkInfo *)(info + 1);

						ret = sceSblUsGetExtractSpackage(WHITE_LIST_SPKG_NODE,
							requestId, (uint32_t)info->decryptedSize,
							packageSize, buf);
						if (ret == 0)
							*size = chunk->size;
					}
					break;
				}
				sceKernelDelayThread(WHITE_LIST_SPKG_POLL_USEC);
			}
		}
	}
	if (package != NULL)
		sceSblUsReleaseBuffer(package);
	return ret;
}

/*
 * The first line of the teleport list: "<name>:<format>:<version>:<w|b>", and where its entries
 * start, the first line that starts with a capital letter (0x810A6EDA)
 */
int gameWhiteListTeleportHeaderRead(const char *buf, SceSize size,
				    GameWhiteListTeleportHeader *header)
{
	thread::RMutexLock lock(&s_utilMutex);
	char format[4];
	char version[11];

	sce_paf_memset(header, 0, sizeof(*header));
	if (buf == NULL)
		return GAME_WHITE_LIST_ERROR_BAD_FILE;
	if (header == NULL)
		return GAME_WHITE_LIST_ERROR_BAD_FILE;
	if (size < WHITE_LIST_TELEPORT_MIN_SIZE)
		return GAME_WHITE_LIST_ERROR_BAD_FILE;

	sce_paf_memset(format, 0, sizeof(format));
	sce_paf_memset(version, 0, sizeof(version));

	const char *p = sce_paf_strchr(buf, ':');

	if (p == NULL)
		return GAME_WHITE_LIST_ERROR_BAD_FORMAT;
	p++;
	sce_paf_strncpy(format, p, 3);
	format[3] = '\0';
	header->format = sce_paf_atoi(format);

	p = sce_paf_strchr(p, ':');
	if (p == NULL)
		return GAME_WHITE_LIST_ERROR_BAD_FORMAT;
	p++;
	sce_paf_strncpy(version, p, 10);
	version[10] = '\0';
	header->version = sce_paf_atoi(version);

	p = sce_paf_strchr(p, ':');
	if (p == NULL)
		return GAME_WHITE_LIST_ERROR_BAD_FORMAT;
	if (p[1] == 'w')
		header->type = GAME_WHITE_LIST_TYPE_WHITE;
	else if (p[1] == 'b')
		header->type = GAME_WHITE_LIST_TYPE_BLACK;
	else
		return GAME_WHITE_LIST_ERROR_BAD_FORMAT;

	const char *end = buf + size - 1;
	const char *line = p + 2;

	char c = *line;

	if (c == '\r') {
		line++;
		c = *line;
	}
	if (c == '\n')
		line++;
	while (line != end) {
		c = *line;
		if (c >= 'A' && c <= 'Z') {
			header->headerSize = line - buf;
			break;
		}

		const char *nl = sce_paf_strchr(line, '\n');

		if (nl == NULL)
			break;
		line = nl + 1;
	}
	if (header->headerSize == 0)
		header->headerSize = size;
	return 0;
}

/* tm0:teleport/force.dat made: the teleport list is installed again (0x810A7088) */
int gameWhiteListTeleportForceSet(void)
{
	thread::RMutexLock lock(&s_utilMutex);

	Dir::Create(WHITE_LIST_TELEPORT_DIR, 6);
	if (!LocalFile::Exists(WHITE_LIST_TELEPORT_FORCE)) {
		int ret = 0;
		common::SharedPtr<File> file = LocalFile::Open(WHITE_LIST_TELEPORT_FORCE,
							       SCE_O_WRONLY | SCE_O_CREAT, 6, &ret);

		if (file.get() == NULL)
			return ret;
		file->Close();
	}
	return 0;
}

/*
 * Whether tm0:teleport/force.dat is there (1), the teleport lists then removed; 0 when it is not
 * (0x810A7160)
 */
int gameWhiteListTeleportForceCheck(void)
{
	thread::RMutexLock lock(&s_utilMutex);
	int ret;

	if (!LocalFile::Exists(WHITE_LIST_TELEPORT_FORCE))
		return 0;
	if (LocalFile::Exists(WHITE_LIST_TELEPORT_LIST)) {
		ret = LocalFile::RemoveFile(WHITE_LIST_TELEPORT_LIST);
		if (ret < 0)
			return ret;
	}
	if (LocalFile::Exists(WHITE_LIST_TELEPORT_OLD)) {
		ret = LocalFile::RemoveFile(WHITE_LIST_TELEPORT_OLD);
		if (ret < 0)
			return ret;
	}
	if (LocalFile::Exists(WHITE_LIST_TELEPORT_TMP)) {
		ret = LocalFile::RemoveFile(WHITE_LIST_TELEPORT_TMP);
		if (ret < 0)
			return ret;
	}
	return 1;
}

/* whether ux0: is there (0x810A7218) */
bool gameWhiteListUx0Exists(void)
{
	LocalFileStat stat;

	return LocalFile::Getstat("ux0:", &stat) == 0;
}

/* dir made again, empty (0x810A72A0) */
int gameWhiteListDirRecreate(const char *dir)
{
	int ret;

	if (Dir::IsExist(dir)) {
		ret = Dir::RemoveRecursive(dir);
		if (ret < 0)
			return ret;
	}
	ret = Dir::CreateRecursive(dir);
	if (ret < 0)
		return ret;
	return 0;
}

/* dir removed when it is there (0x810A72C6) */
int gameWhiteListDirRemove(const char *dir)
{
	if (!Dir::IsExist(dir))
		return 0;

	int ret = Dir::RemoveRecursive(dir);

	if (ret < 0)
		return ret;
	return 0;
}

/* src copied to dst, which is replaced (0x810A72E2) */
int gameWhiteListFileCopy(const char *src, const char *dst)
{
	int ret = 0;

	if (LocalFile::Exists(dst)) {
		ret = LocalFile::RemoveFile(dst);
		if (ret < 0)
			return ret;
	}

	LocalFileStat stat;
	void *buf = sce_paf_malloc(WHITE_LIST_COPY_SIZE);

	if (buf == NULL)
		return GAME_WHITE_LIST_ERROR_NO_MEMORY;

	common::SharedPtr<File> in;
	common::SharedPtr<File> out;

	in = LocalFile::Open(src, SCE_O_RDONLY, 0, &ret);

	if (in.get() != NULL) {
		out = LocalFile::Open(dst, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0606, &ret);
		if (out.get() != NULL) {
			int n;
			int written;

			for (;;) {
				n = in->Read(buf, WHITE_LIST_COPY_SIZE);
				if (n < 0) {
					ret = n;
					break;
				}
				if (n == 0)
					break;
				written = out->Write(buf, n);
				if (written < 0) {
					ret = written;
					break;
				}
				if (n != written) {
					ret = GAME_WHITE_LIST_ERROR_SHORT_WRITE;
					break;
				}
			}
		}
		if (in.get() != NULL)
			in->Close();
	}
	if (out.get() != NULL)
		out->Close();
	if (buf != NULL)
		sce_paf_free(buf);
	return ret;
}
