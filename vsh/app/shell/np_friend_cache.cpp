/*
 * np_friend_cache.cpp: SceFriendCacheCreater — downloads, caches, locks, and prunes
 * PSN friend avatar/panel images under appdata mount 101 (/cache/friends/).
 * 3.65-CEX: 0x81404082..0x81405E4D
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_dirent.h>
#include <rtc.h>
#include <libmd5.h>
#include <std/list>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/httpfile.h>
#include <file/localfile.h>
#include <rtc/rtc.h>
#include <thread/thread.h>
#include "np_friend_importer_internal.h"

#define FRIEND_CACHE_APPDATA_MOUNT_ID		101
#define FRIEND_CACHE_APPDATA_ALREADY_MOUNTED	(-2139095037)	/* 0x80800003 */
#define FRIEND_CACHE_IO_ENOENT			(-2147418110)	/* 0x80010002 */
#define FRIEND_CACHE_HTTP_NO_LENGTH		(-2135994287)	/* 0x80AF5051 */

#define FRIEND_CACHE_FILE_MAGIC			0x80101C00U
#define FRIEND_CACHE_FILE_VERSION		1U
#define FRIEND_CACHE_CHUNK_SIZE			0x10000U
#define FRIEND_CACHE_EXPIRY_TICKS		0x8CD0E3A000ULL	/* 7 days in microseconds */
#define FRIEND_CACHE_MAX_BYTES_THRESHOLD	23592960.0	/* 22.5 MiB */
#define FRIEND_CACHE_PRUNE_TARGET_BYTES		5242879.5	/* 5 MiB */

#define FRIEND_CACHE_FLOCK_LOCK_EX_NB		0x104
#define FRIEND_CACHE_FLOCK_UNLOCK		0x10

extern "C" {
extern int sceAppMgrAppDataMount(int mountId, char *mountPoint);
extern int sceAppMgrGetUserDirPath(int dirType, char *outPath, SceSize bufSize);
extern int vshIoFlock(SceUID fd, int operation);
extern int sceRtcGetTick(const SceDateTime *dateTime, SceRtcTick *tick);
}

namespace {

struct FriendCacheFileHeader {
	SceUInt32 magic;
	SceUInt32 version;
	SceInt64 contentLength;
	SceUInt32 urlLength;
	SceUInt32 reserved;
};

struct CachePruneRecord {
	SceUInt64 ctimeTick;
	SceOff fileSize;
	paf::string filePath;
};

class FriendCacheCreator {
public:
	FriendCacheCreator();
	virtual ~FriendCacheCreator();

	int PruneCacheDirectory(void);
	int WriteCacheTempFile(paf::common::SharedPtr<paf::HttpFile> httpFile,
			       const char *tempPath, const char *imageUrl);
	paf::string ComputeUrlMd5Hex(const char *imageUrl);
	int DownloadAvatar(const char *imageUrl);

	static paf::string MountCacheDirectory(void);
	static int EnsureUserCacheDirectories(void);

	void SetCacheDir(const paf::string &dir) { m_cacheDir = dir; }
	const paf::string &GetCacheDir(void) const { return m_cacheDir; }
	void ClearNeedsPrune(void) { m_needsPrune = false; }

private:
	static void AbortAndCloseFile(paf::common::SharedPtr<paf::File> file);
	static void AbortAndCloseHttpFile(paf::common::SharedPtr<paf::HttpFile> file);
	int CheckCacheStatus(const char *cachePath) const;
	SceUID AcquireLockFile(const char *lockPath);
	void ReleaseAndRemoveLockFile(SceUID lockFd, const char *lockPath);
	int RemoveCacheFile(const char *path);
	int RenameTempToCache(const char *tempPath, const char *cachePath);

	paf::thread::RMutex m_mutex;
	SceInt64 m_totalBytes;
	bool m_needsPrune;
	paf::string m_cacheDir;
	bool m_aborted;
};

static FriendCacheCreator *s_friendCacheInstance = NULL;

FriendCacheCreator::FriendCacheCreator()
	: m_mutex("SceFriendCacheCreater", 0),
	  m_totalBytes(0),
	  m_needsPrune(false),
	  m_cacheDir(""),
	  m_aborted(false)
{
}

FriendCacheCreator::~FriendCacheCreator()
{
}

void FriendCacheCreator::AbortAndCloseFile(paf::common::SharedPtr<paf::File> file)
{
	if (file.get() != NULL && file->IsOpened()) {
		file->Abort();
		SceInt64 asyncResult = 0;
		file->WaitAsync(&asyncResult);
		file->Close();
	}
}

void FriendCacheCreator::AbortAndCloseHttpFile(paf::common::SharedPtr<paf::HttpFile> file)
{
	if (file.get() != NULL && file->IsOpened()) {
		file->Abort();
		SceInt64 asyncResult = 0;
		file->WaitAsync(&asyncResult);
		file->Close();
	}
}

/* 0x81404450: Mount appdata partition 101 and append "/cache/friends/" */
paf::string FriendCacheCreator::MountCacheDirectory(void)
{
	char mountPoint[16];
	sce_paf_memset(mountPoint, 0, sizeof(mountPoint));

	int ret = sceAppMgrAppDataMount(FRIEND_CACHE_APPDATA_MOUNT_ID, mountPoint);
	if (ret >= 0 || ret == FRIEND_CACHE_APPDATA_ALREADY_MOUNTED) {
		paf::string result(mountPoint);
		result += "/cache/friends/";
		return result;
	}
	return paf::string("");
}

/* 0x81404572: Ensure user directory /psnfriend/cache/friends/ hierarchy exists */
int FriendCacheCreator::EnsureUserCacheDirectories(void)
{
	char userDir[32];
	sce_paf_memset(userDir, 0, sizeof(userDir));

	if (sceAppMgrGetUserDirPath(1, userDir, sizeof(userDir)) > 0)
		return -1;

	paf::string fullPath(userDir);
	fullPath += "/psnfriend/cache/friends/";

	const char *baseStr = fullPath.c_str();
	const char *cursor = baseStr + 1;
	char partialPath[64];

	for (;;) {
		const char *slash = sce_paf_strchr(cursor, '/');
		if (slash == NULL)
			break;
		SceSize prefixLen = static_cast<SceSize>(slash - baseStr);
		if (prefixLen >= sizeof(partialPath))
			prefixLen = sizeof(partialPath) - 1;
		sce_paf_strncpy(partialPath, baseStr, prefixLen);
		partialPath[prefixLen] = '\0';
		paf::Dir::Create(partialPath, 0x106U);
		cursor = slash + 1;
	}
	return 0;
}

/* 0x81404082: Scan cache directory and evict oldest files if total exceeds 22.5 MiB */
int FriendCacheCreator::PruneCacheDirectory(void)
{
	paf::list<CachePruneRecord> entriesByTime;
	SceInt64 totalBytes = 0;

	SceUID dirFd = sceIoDopen(m_cacheDir.c_str());
	if (dirFd < 0)
		return -1;

	SceIoDirent dirent;
	int readRet = 0;
	for (;;) {
		sce_paf_memset(&dirent, 0, sizeof(dirent));
		readRet = sceIoDread(dirFd, &dirent);
		if (readRet <= 0)
			break;

		SceRtcTick ctimeTick;
		ctimeTick.tick = 0;
		sceRtcGetTick(&dirent.d_stat.st_ctime, &ctimeTick);

		CachePruneRecord record;
		record.ctimeTick = ctimeTick.tick;
		record.fileSize = dirent.d_stat.st_size;
		record.filePath = m_cacheDir + paf::string(dirent.d_name);

		paf::list<CachePruneRecord>::iterator insertPos = entriesByTime.begin();
		while (insertPos != entriesByTime.end() &&
		       insertPos->ctimeTick <= record.ctimeTick) {
			++insertPos;
		}
		entriesByTime.insert(insertPos, record);
		totalBytes += dirent.d_stat.st_size;
	}

	if (readRet == 0) {
		SceInt64 removedBytes = 0;
		if (static_cast<double>(totalBytes) > FRIEND_CACHE_MAX_BYTES_THRESHOLD) {
			paf::list<CachePruneRecord>::iterator iter = entriesByTime.begin();
			while (iter != entriesByTime.end()) {
				const char *filePath = iter->filePath.c_str();
				if (filePath != NULL) {
					m_mutex.Lock();
					int rmRet = sceIoRemove(filePath);
					m_mutex.Unlock();
					if (rmRet >= 0)
						removedBytes += iter->fileSize;
				}
				++iter;
				if (static_cast<double>(removedBytes) >
				    FRIEND_CACHE_PRUNE_TARGET_BYTES)
					break;
			}
		}
		m_totalBytes = totalBytes - removedBytes;
	}

	sceIoDclose(dirFd);
	return 0;
}

/* 0x81404F9C: MD5 hex digest of image URL */
paf::string FriendCacheCreator::ComputeUrlMd5Hex(const char *imageUrl)
{
	if (imageUrl == NULL)
		return paf::string("");

	unsigned char digest[32];
	sce_paf_memset(digest, 0, sizeof(digest));
	sceMd5Digest(imageUrl, sce_paf_strlen(imageUrl), digest);

	char hexStr[64];
	sce_paf_memset(hexStr, 0, sizeof(hexStr));
	sce_paf_snprintf(
		hexStr, sizeof(hexStr),
		"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		digest[0], digest[1], digest[2], digest[3],
		digest[4], digest[5], digest[6], digest[7],
		digest[8], digest[9], digest[10], digest[11],
		digest[12], digest[13], digest[14], digest[15]);

	return paf::string(hexStr);
}

/* 0x814047C6: Stream HTTP response into temp cache file with 24-byte header + URL */
int FriendCacheCreator::WriteCacheTempFile(paf::common::SharedPtr<paf::HttpFile> httpFile,
					   const char *tempPath, const char *imageUrl)
{
	if (tempPath == NULL)
		return -1;

	paf::string urlText((imageUrl != NULL) ? imageUrl : "");
	if (urlText.size() == 0)
		return -1;

	if (m_aborted)
		return -2;

	int ioErr = 0;
	paf::common::SharedPtr<paf::File> localFile =
		paf::LocalFile::Open(tempPath, 0x602U, 0x1FFU, &ioErr);
	if (localFile.get() == NULL)
		return -1;

	if (m_aborted) {
		localFile->Close();
		return -2;
	}

	FriendCacheFileHeader header;
	sce_paf_memset(&header, 0, sizeof(header));
	header.magic = FRIEND_CACHE_FILE_MAGIC;
	header.version = FRIEND_CACHE_FILE_VERSION;
	header.contentLength = httpFile->GetFileSize();
	header.urlLength = static_cast<SceUInt32>(urlText.size());

	if (m_aborted) {
		AbortAndCloseFile(localFile);
		return -2;
	}

	if (localFile->Write(&header, sizeof(header)) != static_cast<SceInt32>(sizeof(header))) {
		localFile->Close();
		return -1;
	}

	if (m_aborted) {
		AbortAndCloseFile(localFile);
		return -2;
	}

	SceInt32 urlWritten = localFile->Write(urlText.c_str(), header.urlLength);
	if (urlWritten < 0 || static_cast<SceUInt32>(urlWritten) != header.urlLength) {
		localFile->Close();
		return -1;
	}

	if (m_aborted) {
		localFile->Close();
		return -2;
	}

	void *chunkBuf = sce_paf_malloc(FRIEND_CACHE_CHUNK_SIZE);
	if (chunkBuf == NULL) {
		localFile->Close();
		return -1;
	}

	SceInt64 totalWritten = 0;
	for (;;) {
		if (m_aborted) {
			ioErr = -2;
			break;
		}
		SceInt32 readBytes = httpFile->Read(chunkBuf, FRIEND_CACHE_CHUNK_SIZE);
		if (readBytes < 0) {
			ioErr = -1;
			break;
		}
		if (readBytes == 0)
			break;
		if (m_aborted) {
			ioErr = -2;
			break;
		}
		SceInt32 writeBytes = localFile->Write(chunkBuf, static_cast<SceSize>(readBytes));
		if (writeBytes < 0) {
			ioErr = -1;
			break;
		}
		totalWritten += writeBytes;
		if (writeBytes == 0 || readBytes == static_cast<SceInt32>(FRIEND_CACHE_CHUNK_SIZE))
			break;
	}

	localFile->Close();
	sce_paf_free(chunkBuf);

	if (ioErr >= 0) {
		m_totalBytes += totalWritten;
		if (static_cast<double>(m_totalBytes) > FRIEND_CACHE_MAX_BYTES_THRESHOLD)
			m_needsPrune = true;
	}

	return ioErr;
}

int FriendCacheCreator::CheckCacheStatus(const char *cachePath) const
{
	if (cachePath == NULL)
		return -1;

	SceIoStat fileStat;
	sce_paf_memset(&fileStat, 0, sizeof(fileStat));
	int statRet = sceIoGetstat(cachePath, &fileStat);
	if (statRet == FRIEND_CACHE_IO_ENOENT)
		return 0;
	if (statRet < 0)
		return -1;

	SceRtcTick fileTick;
	fileTick.tick = 0;
	sceRtcGetTick(&fileStat.st_ctime, &fileTick);

	paf::rtc::Tick nowTick = 0;
	paf::rtc::GetCurrentTick(&nowTick);

	if (nowTick - fileTick.tick <= FRIEND_CACHE_EXPIRY_TICKS)
		return 1;
	return 2;
}

SceUID FriendCacheCreator::AcquireLockFile(const char *lockPath)
{
	if (lockPath == NULL)
		return -1;

	m_mutex.Lock();
	SceUID fd = sceIoOpen(lockPath, SCE_O_RDWR | SCE_O_CREAT, 0x186);
	if (fd < 0) {
		m_mutex.Unlock();
		return fd;
	}
	int lockRet = vshIoFlock(fd, FRIEND_CACHE_FLOCK_LOCK_EX_NB);
	m_mutex.Unlock();
	return (lockRet >= 0) ? fd : lockRet;
}

void FriendCacheCreator::ReleaseAndRemoveLockFile(SceUID lockFd, const char *lockPath)
{
	m_mutex.Lock();
	if (vshIoFlock(lockFd, FRIEND_CACHE_FLOCK_UNLOCK) >= 0)
		sceIoClose(lockFd);
	m_mutex.Unlock();

	if (lockPath != NULL) {
		m_mutex.Lock();
		sceIoRemove(lockPath);
		m_mutex.Unlock();
	}
}

int FriendCacheCreator::RemoveCacheFile(const char *path)
{
	if (path == NULL)
		return -1;

	m_mutex.Lock();
	int ret = sceIoRemove(path);
	m_mutex.Unlock();
	return (ret >= 0) ? 0 : ret;
}

int FriendCacheCreator::RenameTempToCache(const char *tempPath, const char *cachePath)
{
	m_mutex.Lock();
	int ret = -1;
	if (cachePath != NULL && tempPath != NULL && sceIoRename(tempPath, cachePath) >= 0)
		ret = 0;
	m_mutex.Unlock();
	return ret;
}

/* 0x8140511C: Ensure avatar/panel image URL is downloaded and cached locally */
int FriendCacheCreator::DownloadAvatar(const char *imageUrl)
{
	if (imageUrl == NULL)
		return -1;
	if (m_aborted)
		return -2;

	if (m_needsPrune) {
		m_needsPrune = false;
		PruneCacheDirectory();
	}
	if (m_aborted)
		return -2;

	paf::string md5Hex = ComputeUrlMd5Hex(imageUrl);
	if (md5Hex.size() == 0)
		return -1;

	paf::string cachePath = m_cacheDir + md5Hex;
	paf::string lockPath = cachePath + ".lock";
	paf::string tempPath = cachePath + ".tmp";

	if (m_aborted)
		return -2;

	int cacheState = CheckCacheStatus(cachePath.c_str());
	if (cacheState == 1)
		return 0;

	SceUID lockFd = AcquireLockFile(lockPath.c_str());
	if (lockFd < 0)
		return -1;

	int opStatus = 0;
	if (cacheState == 2)
		opStatus = RemoveCacheFile(cachePath.c_str());

	paf::common::SharedPtr<paf::HttpFile> httpFile;
	if (opStatus >= 0 && !m_aborted) {
		httpFile = paf::HttpFile::Open(imageUrl, &opStatus, 0);
		if (httpFile.get() != NULL) {
			if (m_aborted) {
				AbortAndCloseHttpFile(httpFile);
			} else {
				SceUInt16 httpCode = 0;
				httpFile->GetResponseCode(httpCode);
				if (httpCode == 200) {
					if (m_aborted) {
						AbortAndCloseHttpFile(httpFile);
					} else {
						SceInt64 contentSize = httpFile->GetFileSize();
						if (contentSize == FRIEND_CACHE_HTTP_NO_LENGTH ||
						    contentSize <= 0) {
							httpFile->Close();
						} else {
							opStatus = WriteCacheTempFile(
								httpFile, tempPath.c_str(),
								imageUrl);
							httpFile->Close();
							if (opStatus >= 0) {
								opStatus = RenameTempToCache(
									tempPath.c_str(),
									cachePath.c_str());
							}
						}
					}
				}
			}
		}
	}

	ReleaseAndRemoveLockFile(lockFd, lockPath.c_str());
	return 0;
}

} /* anonymous namespace */

/* 0x81404694: Initialize SceFriendCacheCreater singleton and prune cache */
int npFriendCacheInit(void)
{
	if (s_friendCacheInstance == NULL) {
		s_friendCacheInstance = new FriendCacheCreator();
		if (s_friendCacheInstance == NULL)
			return -1;
	}

	if (FriendCacheCreator::EnsureUserCacheDirectories() != 0)
		return -1;

	paf::string cacheDir = FriendCacheCreator::MountCacheDirectory();
	s_friendCacheInstance->SetCacheDir(cacheDir);
	if (s_friendCacheInstance->GetCacheDir().size() == 0)
		return -1;

	s_friendCacheInstance->ClearNeedsPrune();
	s_friendCacheInstance->PruneCacheDirectory();
	return 0;
}

/* 0x8140479C: Terminate SceFriendCacheCreater singleton */
int npFriendCacheTerm(void)
{
	if (s_friendCacheInstance != NULL) {
		delete s_friendCacheInstance;
		s_friendCacheInstance = NULL;
	}
	return 0;
}

/* 0x8140511C wrapper: Download and cache avatar or panel URL */
int npFriendCacheDownloadAvatar(const char *imageUrl)
{
	if (s_friendCacheInstance == NULL)
		return -1;
	return s_friendCacheInstance->DownloadAvatar(imageUrl);
}
