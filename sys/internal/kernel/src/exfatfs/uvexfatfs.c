/* Mounts which do not use the per-file cluster cache. */
#include <stddef.h>
#include <scetypes.h>
#include <string.h>
#include <sysmem/sysclib.h>
#include <sysmem/debug.h>
#include <iofilemgr/iofilemgr.h>
#include "exfatfs_internal.h"

const char *g_clusterCacheDisabledMounts[] = {
	"sd0:",
	"os0:",
	"tm0:",
	"sa0:",
	"pd0:",
	NULL,
};

int g_codepageTablesEnabled = 1;
int g_sharedResourceInitialized;
struct ThreadmgrFastMutex g_sharedResourceMutex;

SceExfatfsSharedResource g_sharedResource;

/* File-size ceilings, indexed by FAT and exFAT filesystem type minus one. */
static const SceUInt64 g_fileSizeLimits[] = {
	0xffffffffULL,
	0xffffffffffffffffULL,
};

extern int UVFAT_Initialize(struct SceExfatfsSharedResource *sharedResource);
extern int UVFAT_readallocationbitmap(struct SceExfatfsSharedResource *sharedResource,
	struct SceExfatfsFileSystem *fileSystem);
extern SceUInt32 UVFAT_GetFatEmptyClust(struct SceExfatfsFileSystem *fileSystem,
	SceUInt32 cluster);
extern int sceFatfsInitializeHeap(int reuseHeap);
extern int sceFatfsCpInitTable(void);

extern SceUInt32 _root_cluster(struct SceExfatfsFileSystem *fileSystem);
extern void *allocateHeapBuffer(SceExfatfsDrive *unusedDrive);
extern void freeHeapBuffer(int unused, void *buffer);
extern void *allocateCommonHeapMemory(int unused, SceSize size);

extern int sceFatfsPrepareNameset(int unused, const unsigned char *path, SceFatfsNameset **nameset);

int _utf16_to_utf8(unsigned char *destination, unsigned int remaining,
		const SceUInt16 *source)
{
	unsigned char *output = destination;
	unsigned int unicode = *source;
	unsigned int low;
	const SceUInt16 *previous;
	int result;

	while (unicode) {
		previous = source;
		if (unicode <= 127) {
			if (!remaining) {
#if FW_VERSION >= 0x04000000
				SCE_DBG_PRINTF_LEVEL(1, 0, 2054,
					"%s: buffer too small, len(%d), left(%d)\n",
					"_utf16_to_utf8", output - destination, remaining);
#endif
				result = -1;
				goto finish;
			}
			--remaining;
			*output++ = unicode;
		} else if (unicode <= 0x7ff) {
			if (remaining <= 1) {
#if FW_VERSION >= 0x04000000
				SCE_DBG_PRINTF_LEVEL(1, 0, 2059,
					"%s: buffer too small, len(%d), left(%d) < 2\n",
					"_utf16_to_utf8", output - destination, remaining);
#endif
				result = -1;
				goto finish;
			}
			output += 2;
			remaining -= 2;
			output[-2] = (unsigned char)(unicode >> 6) | 0xc0;
			output[-1] = (*source & 63) | 128;
		} else if ((unicode & 0xfc00) == 0xd800) {
			low = source[1];
			if ((low & 0xfc00) != 0xdc00) {
#if FW_VERSION >= 0x04000000
				SCE_DBG_PRINTF_LEVEL(1, 0, 2071,
					"%s: invalid code[%#x] after [%#x]\n",
					"_utf16_to_utf8", low, unicode);
#endif
				result = -2;
				goto finish;
			}
			if (remaining <= 3) {
#if FW_VERSION >= 0x04000000
				SCE_DBG_PRINTF_LEVEL(1, 0, 2074,
					"%s: buffer too small, len(%d), left(%d) < 4\n",
					"_utf16_to_utf8", output - destination, remaining);
#endif
				result = -1;
				goto finish;
			}
			output += 4;
			previous = source + 1;
			unicode = (low & 1023) | ((unicode & 1023) << 10);
			unicode += 0x10000;
			remaining -= 4;
			output[-4] = (unsigned char)(unicode >> 18) | 0xf0;
			output[-3] = ((unicode >> 12) & 63) | 128;
			output[-2] = ((unicode >> 6) & 63) | 128;
			output[-1] = (unicode & 63) | 128;
		} else {
			result = 0;
			if ((unicode & 0xfc00) == 0xdc00) {
				result = -2;
#if FW_VERSION >= 0x04000000
				SCE_DBG_PRINTF_LEVEL(1, 0, 2086,
					"%s: invalid code[%#x]\n", "_utf16_to_utf8", unicode);
#endif
			}
			if (remaining <= 2) {
#if FW_VERSION >= 0x04000000
				SCE_DBG_PRINTF_LEVEL(1, 0, 2090,
					"%s: buffer too small, len(%d), left(%d) < 2\n",
					"_utf16_to_utf8", output - destination, remaining);
#endif
				result = -1;
				goto finish;
			}
			output[0] = (unsigned char)(unicode >> 12) | 0xe0;
			output[1] = ((unicode >> 6) & 63) | 128;
			remaining -= 3;
			output[2] = (unicode & 63) | 128;
			output += 3;
			if (result)
				goto finish;
		}
		unicode = previous[1];
		source = previous + 1;
	}
	result = unicode;
finish:
	if (remaining) {
		*output++ = 0;
		if (!result)
			return output - destination - 1;
	} else {
		if (!result)
			result = -1;
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 2118,
			"%s: buffer too small, len(%d), no space for NUL\n",
			"_utf16_to_utf8", output - destination);
#endif
	}
	return result;
}

extern int sceFatfsACGetDirPermission(SceExfatfsDrive *drive,
		unsigned char attribute);

/* Sony's wrapper name is unknown; its mode helper was inlined. */
#if FW_VERSION >= 0x04000000
int _file_vnode_check(SceExfatfsFnode *fnode, const char *caller)
{
	SceUInt16 *path;
	int type = fnode->Stat.st_type;
	SceUInt32 cluster;
	int refCount;
	int fdCount;
	SceUInt16 *fileName;

	if (!type) {
		cluster = fnode->DirCluster;
		if (cluster == (SceUInt32)-2) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 1815,
				"%s: unexpected DirCluster[%#lx], fnode[%p], StartCluster[%#lx]\n",
				"_file_vnode_check", cluster, fnode, fnode->Stat.StartCluster);
			return -4;
		}
		if (!cluster) {
			int rootDirectorySectors = fnode->fileSystem->rootDirectorySectors;

			if (!rootDirectorySectors) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 1820,
					"%s: unexpected DirCluster[%#lx], fnode[%p], StartCluster[%#lx]\n",
					"_file_vnode_check", rootDirectorySectors,
					fnode, fnode->Stat.StartCluster);
				return rootDirectorySectors;
			}
		}
		return type;
	}
	fileName = fnode->FileName;
	refCount = fnode->refCount;
	fdCount = fnode->fdCount;
	if (fileName)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1825,
			"%s: unexpected st_type[%#x], fnode[%p], refCount(%hd), fdCount(%hd), path[%p], name[%p], path(",
			caller, type, fnode, refCount, fdCount,
			fnode->FilePath, fileName);
	else
		SCE_DBG_PRINTF_LEVEL(1, 0, 1827,
			"%s: unexpected st_type[%#x], fnode[%p], refCount(%hd), fdCount(%hd), path[%p], renamed, path(",
			caller, type, fnode, refCount, fdCount, fnode->FilePath);
	path = fnode->FilePath;
	while (*path) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1830, "%c", (signed char)*path);
		++path;
	}
	SCE_DBG_PRINTF_LEVEL(1, 0, 1833, ")\n");
	return -6;
}

int _directory_vnode_check(SceExfatfsFnode *fnode, const char *caller)
{
	int type = fnode->Stat.st_type;
	SceUInt32 cluster;
	SceUInt32 root;
	SceUInt16 *path;
	SceUInt16 *fileName;
	int refCount;
	int fdCount;

	if (type == 6) {
		cluster = fnode->DirCluster;
		if (cluster == (SceUInt32)-2) {
			root = _root_cluster(fnode->fileSystem);
			cluster = fnode->DirCluster;
			if (root != fnode->Stat.StartCluster) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 1775,
					"%s: unexpected DirCluster[%#lx], fnode[%p], StartCluster[%#lx]\n",
					"_directory_vnode_check", cluster,
					fnode, fnode->Stat.StartCluster);
				return -4;
			}
			SCE_DBG_PRINTF_LEVEL(2, 0, 1773,
				"%s: StartCluster[%#lx], DirCluster[%#lx], fnode[%p]\n",
				"_directory_vnode_check", root, cluster, fnode);
			cluster = fnode->DirCluster;
		}
		if (!cluster) {
			if (!fnode->fileSystem->rootDirectorySectors)
				SCE_DBG_PRINTF_LEVEL(1, 0, 1781,
					"%s: unexpected DirCluster[%#lx], fnode[%p], StartCluster[%#lx]\n",
					"_directory_vnode_check", cluster, fnode, fnode->Stat.StartCluster);
			return cluster;
		}
		return 0;
	}
	fileName = fnode->FileName;
	refCount = fnode->refCount;
	fdCount = fnode->fdCount;
	if (fileName)
		SCE_DBG_PRINTF_LEVEL(1, 0, 1786,
			"%s: unexpected st_type[%#x], fnode[%p], refCount(%hd), fdCount(%hd), path[%p], name[%p], path(",
			caller, type, fnode, refCount, fdCount, fnode->FilePath, fileName);
	else
		SCE_DBG_PRINTF_LEVEL(1, 0, 1788,
			"%s: unexpected st_type[%#x], fnode[%p], refCount(%hd), fdCount(%hd), path[%p], renamed, path(",
			caller, type, fnode, refCount, fdCount, fnode->FilePath);
	path = fnode->FilePath;
	while (*path) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1791, "%c", (signed char)*path);
		++path;
	}
	SCE_DBG_PRINTF_LEVEL(1, 0, 1794, ")\n");
	return -6;
}

unsigned char *_temporal_utf8_fullpath(int unused, SceExfatfsFnode *fnode,
		const void *suffix, SceSize suffixLength)
{
	unsigned char *buffer = allocateHeapBuffer((SceExfatfsDrive *)unused);
	unsigned char *failedBuffer;
	int length;
	SceSize remaining;

	if (!buffer)
		return buffer;
	memset(buffer, 0, 2048);
	if (!fnode->FileName)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2140, "%s: renamed\n", "_temporal_utf8_fullpath");
	length = _utf16_to_utf8(buffer, 2048, fnode->FilePath);
	if (length < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2144,
			"%s: invalid utf16 or too long path\n", "_temporal_utf8_fullpath");
		goto fail;
	}
	if (length == 1) {
		if (buffer[0] != '/')
			SCE_DBG_PRINTF_LEVEL(1, 0, 2155,
				"%s: unexpected first charcter(%#x)\n",
				"_temporal_utf8_fullpath", buffer[0]);
		remaining = 2047;
	} else {
		remaining = 2048 - length;
		if (suffix) {
			buffer[length] = '/';
			remaining = 2047 - length;
			++length;
		}
	}
	if (remaining <= suffixLength) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2170,
			"%s: too long path length\n", "_temporal_utf8_fullpath");
		goto fail;
	}
	if (suffixLength) {
		memcpy(buffer + length, suffix, suffixLength);
		length += suffixLength;
	}
	buffer[length] = 0;
	return buffer;
fail:
	failedBuffer = buffer;
	buffer = NULL;
	freeHeapBuffer(unused, failedBuffer);
	return buffer;
}

void _copy_position_and_fd_cache(SceExfatfsFile *destination,
		SceExfatfsFile *source, int directoryMode)
{
	SceExfatfsFnode *dst;
	SceExfatfsFnode *src;
	SceExfatfsClusterCacheElement *element;
	SceExfatfsClusterCacheElement *end;

	destination->Position = source->Position;
	destination->Cluster = source->Cluster;
	destination->clusterIndex = source->clusterIndex;
	dst = destination->fnode;
	src = source->fnode;
	if (!directoryMode) {
		dst->clusterCount = src->clusterCount;
		dst->chunkCount = src->chunkCount;
		element = dst->cluster_cache_array;
		if (element && src->cluster_cache_array) {
			if (dst->cache_element_count <= src->cache_element_count) {
				memcpy(element, src->cluster_cache_array,
					sizeof(*element) * dst->cache_element_count);
			} else {
				end = element + dst->cache_element_count;
				memcpy(element, src->cluster_cache_array,
					sizeof(*element) * src->cache_element_count);
				element += source->fnode->cache_element_count;
				while (element < end) {
					SCE_DBG_ASSERT_FUNC(element != NULL, 485,
						"_invalidate_cluster_cache_element");
					if (element) {
						element->cluster = -2;
						element->index = 0;
						element->count = 0;
					}
					++element;
				}
			}
			dst = destination->fnode;
			src = source->fnode;
		}
		goto copyTail;
	} else {
		if (dst->clusterCount != src->clusterCount) {
			if (dst->clusterCount < src->clusterCount) {
				if (dst->clusterCount <= src->tailIndex)
					SCE_DBG_PRINTF_LEVEL(2, 0, 3826,
						"%s: src->_clusterCount[%#lx] != dst->clusterCount[%#lx], src->checkedIndex[%#lx], dst->checkedIndex[%#lx]\n",
						"_copy_position_and_fd_cache", src->clusterCount,
						dst->clusterCount, src->tailIndex, dst->tailIndex);
				else
					SCE_DBG_PRINTF_LEVEL(1, 0, 3828,
						"%s: diretory_size is checked?, src->dirSize[%#x], dst->dirSize[%#x], src->_clusterCount[%#lx] != dst->clusterCount[%#lx], src->checkedIndex[%#lx], dst->checkedIndex[%#lx]\n",
						"_copy_position_and_fd_cache", src->dirSize,
						dst->dirSize, src->clusterCount, dst->clusterCount,
						src->tailIndex, dst->tailIndex);
			} else {
				SCE_DBG_PRINTF_LEVEL(1, 0, 3831,
					"%s: unexpected src->_clusterCount[%#lx] != dst->clusterCount[%#lx], src->checkedIndex[%#lx], dst->checkedIndex[%#lx]\n",
					"_copy_position_and_fd_cache", src->clusterCount,
					dst->clusterCount, src->tailIndex, dst->tailIndex);
			}
			dst = destination->fnode;
			src = source->fnode;
		}
		if (dst->tailIndex == src->tailIndex)
			goto copyBuffer;
	}
copyTail:
	dst->tailIndex = src->tailIndex;
	dst->tailCluster = src->tailCluster;
copyBuffer:
	if (!destination->Buf) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 3842,
			"%s: NULL dst->Buf\n", "_copy_position_and_fd_cache");
		return;
	}
	if (!source->Buf) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 3844,
			"%s: NULL src->Buf\n", "_copy_position_and_fd_cache");
		return;
	}
	if (!directoryMode || destination->cacheIndex != source->cacheIndex) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 3848,
			"%s: copy size[%#lx], dst->Buf[%p], src->Buf[%p]\n",
			"_copy_position_and_fd_cache", src->fileSystem->fileBufferSize,
			destination->Buf, source->Buf);
		destination->cacheFlags = source->cacheFlags;
		destination->cacheCluster = source->cacheCluster;
		destination->clustBlockNo = source->clustBlockNo;
		destination->cacheIndex = source->cacheIndex;
		memcpy(destination->Buf, source->Buf, source->fnode->fileSystem->fileBufferSize);
	}
}
#else
/* Sony inlined this helper into the release directory reader. */
static inline void _copy_position_and_fd_cache(SceExfatfsFile *destination,
		SceExfatfsFile *source, int directoryMode)
{
	SceExfatfsFnode *dst;
	SceExfatfsFnode *src;
	SceExfatfsClusterCacheElement *element, *end;

	destination->Cluster = source->Cluster;
	destination->Position = source->Position;
	destination->clusterIndex = source->clusterIndex;
	dst = destination->fnode;
	src = source->fnode;
	if (!directoryMode) {
		dst->clusterCount = src->clusterCount;
		dst->chunkCount = src->chunkCount;
		element = dst->cluster_cache_array;
		if (element && src->cluster_cache_array) {
			if (dst->cache_element_count <= src->cache_element_count) {
				memcpy(element, src->cluster_cache_array,
					sizeof(*element) * dst->cache_element_count);
			} else {
				end = element + dst->cache_element_count;
				memcpy(element, src->cluster_cache_array,
					sizeof(*element) * src->cache_element_count);
				element += source->fnode->cache_element_count;
				while (element < end) {
					if (element) {
						element->cluster = -2;
						element->index = 0;
						element->count = 0;
					}
					++element;
				}
			}
			dst = destination->fnode;
			src = source->fnode;
		}
		dst->tailCluster = src->tailCluster;
		dst->tailIndex = src->tailIndex;
	} else if (dst->tailIndex != src->tailIndex) {
		dst->tailCluster = src->tailCluster;
		dst->tailIndex = src->tailIndex;
	}
	if (destination->Buf && source->Buf &&
	    (!directoryMode || destination->cacheIndex != source->cacheIndex)) {
		destination->cacheIndex = source->cacheIndex;
		destination->cacheCluster = source->cacheCluster;
		destination->clustBlockNo = source->clustBlockNo;
		destination->cacheFlags = source->cacheFlags;
		memcpy(destination->Buf, source->Buf, source->fnode->fileSystem->fileBufferSize);
	}
}
#endif

void exfatfsStatToIoStat(SceExfatfsDrive *drive,
		const SceExfatfsStat *source, SceIoStat *destination)
{
	unsigned int attribute = source->st_attr;
	int mode;

	destination->st_attr = attribute;
	mode = sceFatfsACGetDirPermission(drive, attribute);
#if FW_VERSION >= 0x04000000
	if (mode < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 930,
			"_sceFatfsWriteModeFromStat",
			"%s: sceFatfsACGetDirPermission for attr[%#x] failed[%#x]\n",
			"_sceFatfsWriteModeFromStat", attribute, mode);
#endif
	if ((attribute & 0x10) || attribute == 15)
		mode |= 0x1000;
	else if (!(attribute & 8))
		mode |= 0x2000;
	destination->st_mode = mode;
	destination->st_size = source->Size;
	destination->st_ctime.year = source->ctime.year;
	destination->st_ctime.month = source->ctime.month;
	destination->st_ctime.day = source->ctime.day;
	destination->st_ctime.hour = source->ctime.hour;
	destination->st_ctime.minute = source->ctime.minute;
	destination->st_ctime.second = source->ctime.second;
	destination->st_ctime.microsecond = 0;
	destination->st_atime.year = source->atime.year;
	destination->st_atime.month = source->atime.month;
	destination->st_atime.day = source->atime.day;
	destination->st_atime.hour = source->atime.hour;
	destination->st_atime.minute = source->atime.minute;
	destination->st_atime.second = source->atime.second;
	destination->st_atime.microsecond = 0;
	destination->st_mtime.year = source->mtime.year;
	destination->st_mtime.month = source->mtime.month;
	destination->st_mtime.day = source->mtime.day;
	destination->st_mtime.hour = source->mtime.hour;
	destination->st_mtime.minute = source->mtime.minute;
	destination->st_mtime.second = source->mtime.second;
	destination->st_mtime.microsecond = 0;
}

#if FW_VERSION < 0x04000000
SceFatfsNameset *_temporal_nameset(int unused, SceExfatfsFnode *fnode,
	const void *suffix, size_t suffixLength)
{
	SceFatfsNameset *nameset = NULL;
	unsigned char *buffer;
	unsigned int remaining;
	int length;

	buffer = allocateHeapBuffer((SceExfatfsDrive *)unused);
	if (!buffer)
		return nameset;
	memset(buffer, 0, 0x800);
	length = _utf16_to_utf8(buffer, 0x800, fnode->FilePath);
	if (length < 0)
		goto free;
	if (length == 1) {
		remaining = 0x7ff;
	} else {
		remaining = 0x800 - length;
		if (suffix) {
			buffer[length] = '/';
			++length;
			--remaining;
		}
	}
	if (suffixLength >= remaining)
		goto free;
	if (suffixLength) {
		memcpy(buffer + length, suffix, suffixLength);
		length += suffixLength;
	}
	buffer[length] = 0;
	sceFatfsPrepareNameset(unused, buffer, &nameset);
free:
	freeHeapBuffer(unused, buffer);
	return nameset;
}
#endif

int _exfat_lock_shared_resource(const char *caller)
{
	int result = lockFastMutex(&g_sharedResourceMutex);

#if FW_VERSION >= 0x04000000
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0, 99,
			"%s: sceKernelLockFastMutex failed[%#x]\n", caller, result);
#endif
	return result;
}

int _exfat_unlock_shared_resource(const char *caller)
{
	int result = sceKernelUnlockFastMutex(&g_sharedResourceMutex);

#if FW_VERSION >= 0x04000000
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0, 119,
			"%s: sceKernelUnlockFastMutex failed[%#x]\n", caller, result);
#endif
	return result;
}

void _print_fnode(SceExfatfsFnode *fnode, const char *caller)
{
	SceUInt16 *path;
	unsigned int character;

#if FW_VERSION >= 0x04000000
	int refCount = fnode->refCount;
	int fdCount = fnode->fdCount;

	SCE_DBG_PRINTF_LEVEL(1, 0, 134,
		"%s: fnode[%p], refCount(%hd), fdCount(%hd), path(",
		caller, fnode, refCount, fdCount);
#endif
	character = fnode->FilePath[0];
	if (character) {
		path = fnode->FilePath;
		do {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(1, 0, 136, "%lc", character);
#endif
			character = *++path;
		} while (character);
	}
#if FW_VERSION >= 0x04000000
	SCE_DBG_PRINTF_LEVEL(1, 0, 139, "), FilePath[%p], FileName[%p]\n",
		fnode->FilePath, fnode->FileName);
#endif
}

extern int _has_parent_directory(SceExfatfsFnode *fnode);

int _deleted_entry(SceExfatfsFnode *fnode)
{
	int hasParent;
	SceUInt32 root;

	if (!fnode->FilePath[0]) {
		SCE_DBG_ASSERT(fnode->DirCluster == (SceUInt32)-2, 233);
		SCE_DBG_ASSERT(fnode->refCount, 234);
#if FW_VERSION >= 0x04000000
		if (!fnode->FileName)
			SCE_DBG_PRINTF_LEVEL(2, 0, 238,
				"%s: fnode[%p], refCount(%hd), fdCount(%hd), StartCluster[%#lx], FilePath[0]=[%#x], unexpected NULL FileName\n",
				"_deleted_entry", fnode, fnode->refCount, fnode->fdCount,
				fnode->Stat.StartCluster, fnode->FilePath[0]);
#endif
		return fnode->fileSystem != NULL;
	}
	hasParent = _has_parent_directory(fnode);
	if (hasParent)
		return 0;
	if (fnode->fileSystem) {
		if (fnode->Stat.st_type == 6 &&
		    (root = _root_cluster(fnode->fileSystem)) == fnode->Stat.StartCluster) {
#if FW_VERSION >= 0x04000000
			if (!fnode->FileName)
				SCE_DBG_PRINTF_LEVEL(2, 0, 263,
					"%s: root directory fnode[%p], refCount(%hd), fdCount(%hd), StartCluster[%#lx], FilePath[0]=[%#x], unexpected NULL FileName\n",
					"_deleted_entry", fnode, fnode->refCount, fnode->fdCount,
					root, fnode->FilePath[0]);
#endif
		} else {
#if FW_VERSION >= 0x04000000
			if (fnode->FileName)
				SCE_DBG_PRINTF_LEVEL(2, 0, 268,
					"%s: fnode[%p], refCount(%hd), fdCount(%hd), StartCluster[%#lx], FilePath[0]=[%#x], unexpected FileName[0]=[%#x], DirCluster[%#lx]\n",
					"_deleted_entry", fnode, fnode->refCount, fnode->fdCount,
					fnode->Stat.StartCluster, fnode->FilePath[0],
					*fnode->FileName, fnode->DirCluster);
#endif
		}
	}
	return hasParent;
}

void clearClusterCacheStorage(SceExfatfsFnode *fnode)
{
	fnode->cache_element_count = 0;
	fnode->cluster_cache_array = NULL;
}

void _invalidate_cluster_cache_element(SceExfatfsClusterCacheElement *element)
{
	SCE_DBG_ASSERT(element != NULL, 485);
	if (element) {
		element->cluster = -2;
		element->index = 0;
		element->count = 0;
	}
}

void _initialize_cluster_cache_data(SceExfatfsFnode *fnode)
{
	SceExfatfsClusterCacheElement *element;
	SceExfatfsClusterCacheElement *end;
	SceExfatfsFileSystem *fileSystem;

	SCE_DBG_ASSERT(fnode != NULL, 544);
	if (!fnode)
		return;
	fileSystem = fnode->fileSystem;
	fnode->tailCluster = fnode->tailIndex = -2;
	fnode->clusterCount = fnode->chunkCount = 0;
	element = fnode->cluster_cache_array;
	if (!(fileSystem->mount_mode & 0x400))
		return;
	if (!element) {
		element = allocateCommonHeapMemory((int)fileSystem->drive, 512);
		fnode->cluster_cache_array = element;
		if (!element) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 529, "_setup_cluster_cache_data",
				"%s: sceFatfsCreateHeapVl failed\n", "_setup_cluster_cache_data");
#endif
			element = fnode->cluster_cache_array;
		}
	}
	if (!element)
		return;
	fnode->cache_element_count = 42;
	end = element + 42;
	do {
		SCE_DBG_ASSERT_FUNC(element != NULL, 485,
			"_invalidate_cluster_cache_element");
		if (element) {
			element->cluster = -2;
			element->index = 0;
			element->count = 0;
		}
		++element;
	} while (element != end);
}

void _invalidate_cluster_cache_data(SceExfatfsFnode *fnode)
{
	SceExfatfsClusterCacheElement *element, *end;

	SCE_DBG_ASSERT(fnode != NULL, 554);
	if (!fnode)
		return;
	fnode->tailCluster = fnode->tailIndex = -2;
	fnode->clusterCount = fnode->chunkCount = 0;
	element = fnode->cluster_cache_array;
	if (!element)
		return;
	end = element + 42;
	fnode->cache_element_count = 42;
	do {
		SCE_DBG_ASSERT_FUNC(element != NULL, 485,
			"_invalidate_cluster_cache_element");
		if (element) {
			element->cluster = -2;
			element->index = 0;
			element->count = 0;
		}
		++element;
	} while (element != end);
}

/* Sony's identifier for this internal error translation is unknown. */
int exfatfsErrorToIoError(int error)
{
	switch (error) {
	case (int)0x8032001a:
		return error;
	case -50:
		return 0x80010013;
	case -49:
		return 0x8001000c;
	case -48:
		return 0x80010051;
	case -47:
		return 0x80010078;
	case -1:
		return 0x80010078;
	case -38:
		return 0x8001003e;
	case -36:
		return 0x80010006;
	case -34:
		return 0x80010005;
	case -20:
		return 0x80010005;
	case -16:
		return 0x80010005;
	case -13:
		return 0x80010005;
	case -12:
		return 0x80010005;
	case -31:
		return 0x80010011;
	case -9:
		return 0x80010011;
	case -30:
		return 0x80010047;
	case -29:
		return 0x80010010;
	case -22:
		return 0x80010018;
	case -11:
		return 0x80010087;
	case -10:
		return 0x8001001c;
	case -8:
		return 0x80010001;
	case -3:
		return 0x80010001;
	case -6:
		return 0x80010015;
	case -5:
		return 0x80010002;
	case -4:
		return 0x80010002;
	case -2:
		return 0x80010009;
	default:
		return 0x80010016;
	}
}

int sceExfatfsInit(void)
{
	int result;

	result = sceKernelInitializeFastMutex(&g_sharedResourceMutex,
		"SceExfatfsSharedResource", 0x2000, 0);
	if (result < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 0, 598,
			"%s: sceKernelInitializeFastMutex for shared resource failed[%#x]\n",
			"sceExfatfsInit", result);
#endif
		return result;
	}
	result = sceFatfsInitializeHeap(0);
	if (result < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 0, 604,
			"%s: sceFatfsInitializeHeap() failed[%#x]\n", "sceExfatfsInit", result);
#endif
		return result;
	}
	if (g_codepageTablesEnabled) {
		result = sceFatfsCpInitTable();
		if (result < 0) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(2, 15, 611,
				"convertInitTable() failed (0x%08X)\n", result);
#endif
			return result;
		}
	}
	if (!g_sharedResourceInitialized) {
		memset(&g_sharedResource, 0, sizeof(g_sharedResource));
		g_sharedResourceInitialized = 1;
	}
	result = UVFAT_Initialize(&g_sharedResource);
	if (result) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 0, 621,
			"%s: UVFAT_Initialize failed(%d)\n", "sceExfatfsInit", result);
#endif
		return exfatfsErrorToIoError(result);
	}
	return result;
}

#if FW_VERSION >= 0x04000000
int _check_free_count(SceExfatfsFnode *fnode, unsigned long long newSize)
{
	struct SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	unsigned long long clusterSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
	unsigned long long capacity = clusterSize * fileSystem->clusterCount;
	SceUInt32 clusterCount = fileSystem->clusterCount;
	SceUInt32 newCount;
	SceUInt32 oldCount;
	SceUInt32 check;
	SceUInt32 checkStart;
	SceUInt32 found;
	SceUInt32 unused;
	int result;

	if (newSize >= 0x100000000ULL && fileSystem->fileSystemType != 2) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3222, "_exfatfs_can_change_file_size",
			"%s: newSize[%#llx] > file size limit of FAT, type(%d)\n",
			"_exfatfs_can_change_file_size", newSize, fileSystem->fileSystemType);
		return 0x8001001b;
	}
	if (capacity < newSize) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 3228, "_exfatfs_can_change_file_size",
			"%s: newSize[%#llx] > capacity[%#llx], cluster_size[%#x], cluster_count[%#lx]\n",
			"_exfatfs_can_change_file_size", newSize, capacity,
			(unsigned int)clusterSize, clusterCount);
		return 0x8001001c;
	}
	newCount = (clusterSize - 1 + newSize) / clusterSize;
	oldCount = (fnode->Stat.Size - 1 + clusterSize) / clusterSize;
	if (newCount <= oldCount)
		return 0;
	check = newCount - oldCount + fileSystem->fakeFreeClusterCount;
	if (fileSystem->fileSystemType == 2) {
		result = UVFAT_readallocationbitmap(&g_sharedResource, fileSystem);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 3155, "Cannot count free cluster num\n");
			return exfatfsErrorToIoError(result);
		}
		unused = fileSystem->bitmapFreeClusterCount;
		if (check > unused) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 3161,
				"%s: check[%#lx] > unused[%#lx]\n", "_check_free_count", check, unused);
			return 0x8001001c;
		}
		return 0;
	}
	if (check <= fileSystem->freeClusterCount)
		return 0;
	checkStart = fileSystem->nextFreeCluster;
	if (!check)
		return 0;
	for (;;) {
		found = UVFAT_GetFatEmptyClust(fileSystem, checkStart);
		if (found < checkStart) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 3178,
				"%s: last check_start[%#lx], found[%#lx], left[%#lx]\n",
				"_check_free_count", checkStart, found, check);
			return 0x8001001c;
		}
		if (clusterCount + 2 <= found) {
			SCE_DBG_PRINTF_LEVEL(2, 0, 3187,
				"%s: last check_start[%#lx], result[%#lx], left[%#lx]\n",
				"_check_free_count", checkStart, found, check);
			return 0x8001001c;
		}
		checkStart = found + 1;
		if (!--check)
			return 0;
	}
}
#endif

int _sceFatfsWriteModeFromStat(SceExfatfsDrive *drive, int attribute)
{
	unsigned int fsAttribute = attribute & 0xff;
	int mode = sceFatfsACGetDirPermission(drive, fsAttribute);

#if FW_VERSION >= 0x04000000
	if (mode < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0, 930,
			"%s: sceFatfsACGetDirPermission for attr[%#x] failed[%#x]\n",
			"_sceFatfsWriteModeFromStat", fsAttribute, mode);
#endif
	if ((attribute & 0x10) || attribute == 15)
		mode |= 0x1000;
	else if (!(attribute & 8))
		mode |= 0x2000;
	return mode;
}

extern int UVFAT_Mount(struct SceExfatfsSharedResource *sharedResource,
	const char *devname, int mode, const SceExfatfsMountParam *mountParam);

int _cache_all_fat_data_drive(SceExfatfsDrive *drive, int unused,
		const char *devname, int unusedMode, int flags)
{
	const char **mount = g_clusterCacheDisabledMounts + 1;
	const char *assignName = drive->assignName;
	SceExfatfsMountParam mountParam;
	char systemMount[8];
	int mountMode;
	int result;

	if (flags & 1)
		mountMode = 1;
	else if (flags & 2)
		mountMode = 0x102;
	else
		mountMode = 2;
	while (*mount) {
		if (!strncmp(assignName, *mount, 5))
			goto checkSystemMount;
		++mount;
	}
	mountMode |= 0x400;
checkSystemMount:
	__builtin_memcpy(systemMount, "sa0:", 5);
	if (!strncmp(assignName, systemMount, 5)) {
		mountMode |= 0x200;
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 374,
			"%s: cache_all_fat_data for assignName(%s) is enabled\n",
			"_cache_all_fat_data_drive", assignName);
#endif
	}
	mountParam.drive = drive;
	mountParam.mount_mode = mountMode;
	result = UVFAT_Mount(&g_sharedResource, devname, 1, &mountParam);
	if (result) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 1111, "sceExfatfsOpMount",
			"%s: UVFAT_Mount failed[%#x], devname(%s)\n",
			"sceExfatfsOpMount", result, devname);
#endif
		return exfatfsErrorToIoError(result);
	}
	return result;
}

extern SceUInt32 getInitializedFileCount(SceExfatfsSharedResource *sharedResource);
extern SceUInt32 getInitializedFnodeCount(SceExfatfsSharedResource *sharedResource);

int _used_resource_count(SceVfsMount *mount)
{
	SceExfatfsDrive *drive = mount->pData;
	SceExfatfsFileSystem *fileSystem = NULL;
	SceUInt32 fileCount;
	SceUInt32 fnodeCount;
	SceUInt32 i;
	int usedFiles = 0;
	int usedFnodes = 0;
	int result;
#if FW_VERSION >= 0x04000000
	int lockResult;
#endif

	int mountIndex;

	if (g_sharedResource.fileSystems[0].devname &&
		g_sharedResource.fileSystems[0].drive == drive)
		mountIndex = 0;
	else if (g_sharedResource.fileSystems[1].devname &&
		g_sharedResource.fileSystems[1].drive == drive)
		mountIndex = 1;
	else if (g_sharedResource.fileSystems[2].devname &&
		g_sharedResource.fileSystems[2].drive == drive)
		mountIndex = 2;
	else if (g_sharedResource.fileSystems[3].devname &&
		g_sharedResource.fileSystems[3].drive == drive)
		mountIndex = 3;
	else if (g_sharedResource.fileSystems[4].devname &&
		g_sharedResource.fileSystems[4].drive == drive)
		mountIndex = 4;
	else if (g_sharedResource.fileSystems[5].devname &&
		g_sharedResource.fileSystems[5].drive == drive)
		mountIndex = 5;
	else if (g_sharedResource.fileSystems[6].devname &&
		g_sharedResource.fileSystems[6].drive == drive)
		mountIndex = 6;
	else if (g_sharedResource.fileSystems[7].devname &&
		g_sharedResource.fileSystems[7].drive == drive)
		mountIndex = 7;
	else if (g_sharedResource.fileSystems[8].devname &&
		g_sharedResource.fileSystems[8].drive == drive)
		mountIndex = 8;
	else if (g_sharedResource.fileSystems[9].devname &&
		g_sharedResource.fileSystems[9].drive == drive)
		mountIndex = 9;
	else if (g_sharedResource.fileSystems[10].devname &&
		g_sharedResource.fileSystems[10].drive == drive)
		mountIndex = 10;
	else if (g_sharedResource.fileSystems[11].devname &&
		g_sharedResource.fileSystems[11].drive == drive)
		mountIndex = 11;
	else if (g_sharedResource.fileSystems[12].devname &&
		g_sharedResource.fileSystems[12].drive == drive)
		mountIndex = 12;
	else if (g_sharedResource.fileSystems[13].devname &&
		g_sharedResource.fileSystems[13].drive == drive)
		mountIndex = 13;
	else if (g_sharedResource.fileSystems[14].devname &&
		g_sharedResource.fileSystems[14].drive == drive)
		mountIndex = 14;
	else
		mountIndex = -1;
	if (mountIndex >= 0)
		fileSystem = &g_sharedResource.fileSystems[mountIndex];
	result = lockFastMutex(&g_sharedResourceMutex);
	if (result < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 99, "_exfat_lock_shared_resource",
			"%s: sceKernelLockFastMutex failed[%#x]\n", "_used_resource_count", result);
#endif
		return result;
	}
	if (!fileSystem || (fileSystem->drive && (fileSystem->drive->state & 0x4000))) {
		result = -1;
		goto unlock;
	}
	fileCount = getInitializedFileCount(&g_sharedResource);
#if FW_VERSION >= 0x04000000
	lockResult = sceKernelTryLockFastMutex(&fileSystem->drive->fastmutex);
	if (lockResult != (int)0x80028162) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1261, "_used_fd_count",
			"%s: unexpected result[%#x]\n", "_used_fd_count", lockResult);
		SCE_DBG_ASSERT_FUNC(0, 1262, "_used_fd_count");
	}
#endif
	for (i = 1; i < fileCount; ++i) {
		SceExfatfsFile *file = &g_sharedResource.files[i];

		if (file->mode && file->fnode && file->fnode->fileSystem == fileSystem)
			++usedFiles;
	}
	fnodeCount = getInitializedFnodeCount(&g_sharedResource);
	for (i = 0; i < fnodeCount; ++i) {
		if (g_sharedResource.fnodes[i].fileSystem == fileSystem)
			++usedFnodes;
	}
	result = usedFiles + usedFnodes;
unlock:
	{
		int unlockResult = sceKernelUnlockFastMutex(&g_sharedResourceMutex);

#if FW_VERSION >= 0x04000000
		if (unlockResult < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 119, "_exfat_unlock_shared_resource",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_used_resource_count", unlockResult);
#endif
	}
	return result;
}

extern int UVFAT_FinalizeLockMountPoint(SceExfatfsFileSystem *fileSystem);
extern int releaseMediaStorage(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, int mountMode);
extern void freeCommonHeapMemory(int unused, void *memory);

void _free_all_cluster_cache(SceExfatfsDrive *drive)
{
	SceExfatfsFileSystem *fileSystem;
	const char **mount = g_clusterCacheDisabledMounts + 1;
	SceExfatfsClusterCacheElement *cache;
	SceUInt32 fnodeCount;
	SceUInt32 i;
	int mountIndex;
	int result;

	if (!drive)
		return;
	if (g_sharedResource.fileSystems[0].devname &&
		g_sharedResource.fileSystems[0].drive == drive)
		mountIndex = 0;
	else if (g_sharedResource.fileSystems[1].devname &&
		g_sharedResource.fileSystems[1].drive == drive)
		mountIndex = 1;
	else if (g_sharedResource.fileSystems[2].devname &&
		g_sharedResource.fileSystems[2].drive == drive)
		mountIndex = 2;
	else if (g_sharedResource.fileSystems[3].devname &&
		g_sharedResource.fileSystems[3].drive == drive)
		mountIndex = 3;
	else if (g_sharedResource.fileSystems[4].devname &&
		g_sharedResource.fileSystems[4].drive == drive)
		mountIndex = 4;
	else if (g_sharedResource.fileSystems[5].devname &&
		g_sharedResource.fileSystems[5].drive == drive)
		mountIndex = 5;
	else if (g_sharedResource.fileSystems[6].devname &&
		g_sharedResource.fileSystems[6].drive == drive)
		mountIndex = 6;
	else if (g_sharedResource.fileSystems[7].devname &&
		g_sharedResource.fileSystems[7].drive == drive)
		mountIndex = 7;
	else if (g_sharedResource.fileSystems[8].devname &&
		g_sharedResource.fileSystems[8].drive == drive)
		mountIndex = 8;
	else if (g_sharedResource.fileSystems[9].devname &&
		g_sharedResource.fileSystems[9].drive == drive)
		mountIndex = 9;
	else if (g_sharedResource.fileSystems[10].devname &&
		g_sharedResource.fileSystems[10].drive == drive)
		mountIndex = 10;
	else if (g_sharedResource.fileSystems[11].devname &&
		g_sharedResource.fileSystems[11].drive == drive)
		mountIndex = 11;
	else if (g_sharedResource.fileSystems[12].devname &&
		g_sharedResource.fileSystems[12].drive == drive)
		mountIndex = 12;
	else if (g_sharedResource.fileSystems[13].devname &&
		g_sharedResource.fileSystems[13].drive == drive)
		mountIndex = 13;
	else if (g_sharedResource.fileSystems[14].devname &&
		g_sharedResource.fileSystems[14].drive == drive)
		mountIndex = 14;
	else
		return;
	fileSystem = &g_sharedResource.fileSystems[mountIndex];
	if (!fileSystem->devname)
		return;
	while (*mount) {
		if (!strncmp(drive->assignName, *mount, 5))
			goto finalize;
		++mount;
	}
	fnodeCount = getInitializedFnodeCount(&g_sharedResource);
	result = lockFastMutex(&g_sharedResourceMutex);
	if (result < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 99, "_exfat_lock_shared_resource",
			"%s: sceKernelLockFastMutex failed[%#x]\n", "_free_all_cluster_cache", result);
		SCE_DBG_PRINTF_LEVEL(2, 0, 1521,
			"%s: _exfat_lock_shared_resource failed[%#x]\n", "_free_all_cluster_cache", result);
#endif
		goto finalize;
	}
	for (i = 0; i != fnodeCount; ++i) {
		SceExfatfsFnode *fnode = &g_sharedResource.fnodes[i];

		if (fnode->fileSystem == fileSystem) {
			cache = fnode->cluster_cache_array;
			fnode->cache_element_count = 0;
			fnode->cluster_cache_array = NULL;
			if (cache)
				freeCommonHeapMemory((int)fnode->fileSystem->drive, cache);
		}
	}
	result = sceKernelUnlockFastMutex(&g_sharedResourceMutex);
#if FW_VERSION >= 0x04000000
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 119, "_exfat_unlock_shared_resource",
			"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_free_all_cluster_cache", result);
#endif
	cache = fileSystem->fnode.cluster_cache_array;
	fileSystem->fnode.cache_element_count = 0;
	fileSystem->fnode.cluster_cache_array = NULL;
	if (cache)
		freeCommonHeapMemory((int)fileSystem->fnode.fileSystem->drive, cache);
finalize:
	UVFAT_FinalizeLockMountPoint(fileSystem);
	releaseMediaStorage(&g_sharedResource, fileSystem, fileSystem->mount_mode);
	fileSystem->devname = NULL;
}

int _exfatfs_renamed_file(const SceExfatfsFnode *vnode, const SceExfatfsFnode *file)
{
	int result = 1;

	if (vnode->Stat.st_type != file->Stat.st_type) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 1715,
			"%s: vnode->st_type(%d) != file->st_type(%d)\n",
			"_exfatfs_renamed_file", vnode->Stat.st_type, file->Stat.st_type);
#endif
		result = 0;
	}
	if (vnode->Stat.StartCluster != file->Stat.StartCluster) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 1718,
			"%s: vnode->StartCluster[%#lx] != file->StartCluster[%#lx]\n",
			"_exfatfs_renamed_file", vnode->Stat.StartCluster, file->Stat.StartCluster);
#endif
		if ((vnode->Stat.Size || vnode->Stat.StartCluster) &&
			(file->Stat.Size || file->Stat.StartCluster))
			return 0;
	}
	return result;
}

uint64_t fnodeToFileId(const SceExfatfsFnode *fnode)
{
	return (uintptr_t)fnode;
}

SceSize getDriveClusterSize(SceExfatfsDrive *drive)
{
	SceExfatfsFileSystem *fileSystem;
	int mountIndex;

	if (g_sharedResource.fileSystems[0].devname &&
		drive == g_sharedResource.fileSystems[0].drive)
		mountIndex = 0;
	else if (g_sharedResource.fileSystems[1].devname &&
		drive == g_sharedResource.fileSystems[1].drive)
		mountIndex = 1;
	else if (g_sharedResource.fileSystems[2].devname &&
		drive == g_sharedResource.fileSystems[2].drive)
		mountIndex = 2;
	else if (g_sharedResource.fileSystems[3].devname &&
		drive == g_sharedResource.fileSystems[3].drive)
		mountIndex = 3;
	else if (g_sharedResource.fileSystems[4].devname &&
		drive == g_sharedResource.fileSystems[4].drive)
		mountIndex = 4;
	else if (g_sharedResource.fileSystems[5].devname &&
		drive == g_sharedResource.fileSystems[5].drive)
		mountIndex = 5;
	else if (g_sharedResource.fileSystems[6].devname &&
		drive == g_sharedResource.fileSystems[6].drive)
		mountIndex = 6;
	else if (g_sharedResource.fileSystems[7].devname &&
		drive == g_sharedResource.fileSystems[7].drive)
		mountIndex = 7;
	else if (g_sharedResource.fileSystems[8].devname &&
		drive == g_sharedResource.fileSystems[8].drive)
		mountIndex = 8;
	else if (g_sharedResource.fileSystems[9].devname &&
		drive == g_sharedResource.fileSystems[9].drive)
		mountIndex = 9;
	else if (g_sharedResource.fileSystems[10].devname &&
		drive == g_sharedResource.fileSystems[10].drive)
		mountIndex = 10;
	else if (g_sharedResource.fileSystems[11].devname &&
		drive == g_sharedResource.fileSystems[11].drive)
		mountIndex = 11;
	else if (g_sharedResource.fileSystems[12].devname &&
		drive == g_sharedResource.fileSystems[12].drive)
		mountIndex = 12;
	else if (g_sharedResource.fileSystems[13].devname &&
		drive == g_sharedResource.fileSystems[13].drive)
		mountIndex = 13;
	else {
		if (!g_sharedResource.fileSystems[14].devname)
			return 0;
		if (drive != g_sharedResource.fileSystems[14].drive)
			return 0;
		mountIndex = 14;
	}
	fileSystem = &g_sharedResource.fileSystems[mountIndex];
	return fileSystem->sectorSize * fileSystem->clusterSectorCount;
}

extern int sceFatfsACGetRootDirAttribute(SceExfatfsDrive *drive, unsigned char *attribute);
extern SceExfatfsFnode *UVFAT_GetFnode(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, const SceUInt16 *path);
extern int UVFAT_ReleaseFnode(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFnode *fnode);
extern int UVFAT_SearchFile(SceExfatfsSharedResource *sharedResource,
	SceFatfsNameset *nameset, SceExfatfsFileSystem *fileSystem,
	SceExfatfsFnode *fnode, int search);
extern int _set_root_cluster_and_cluster_count(SceExfatfsFnode *fnode,
	SceUInt32 cluster, SceUInt64 size);

int _sceExfatfsVopSetRoot(SceExfatfsDrive *drive, SceExfatfsFnode **root)
{
	SceExfatfsFileSystem *fileSystem;
	SceExfatfsFileSystem *cacheFileSystem;
	SceExfatfsFnode *fnode;
	SceExfatfsFnode *cacheFnode;
	SceExfatfsClusterCacheElement *element;
	SceExfatfsClusterCacheElement *end;
	SceFatfsNameset *nameset;
	SceUInt16 rootPath[2] = { '/', 0 };
	unsigned char path[4];
	unsigned char attribute = 0;
	int result;
	int mountIndex;
#if FW_VERSION >= 0x04000000
	char systemMount[8];
#endif

	result = sceFatfsACGetRootDirAttribute(drive, &attribute);
#if FW_VERSION >= 0x04000000
	if (*root)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2242, "%s: unexpected vnode_data[%p]\n",
			"_sceExfatfsVopSetRoot", *root);
#endif
	*root = NULL;
	if (result < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 2246,
			"%s: sceFatfsACGetRootDirAttribute for drive[%p] failed[%#x]\n",
			"_sceExfatfsVopSetRoot", drive, result);
#endif
		if (result == (int)0x80010002)
			result = (int)0x80010016;
		return result;
	}
	if (g_sharedResource.fileSystems[0].devname &&
		drive == g_sharedResource.fileSystems[0].drive)
		mountIndex = 0;
	else if (g_sharedResource.fileSystems[1].devname &&
		drive == g_sharedResource.fileSystems[1].drive)
		mountIndex = 1;
	else if (g_sharedResource.fileSystems[2].devname &&
		drive == g_sharedResource.fileSystems[2].drive)
		mountIndex = 2;
	else if (g_sharedResource.fileSystems[3].devname &&
		drive == g_sharedResource.fileSystems[3].drive)
		mountIndex = 3;
	else if (g_sharedResource.fileSystems[4].devname &&
		drive == g_sharedResource.fileSystems[4].drive)
		mountIndex = 4;
	else if (g_sharedResource.fileSystems[5].devname &&
		drive == g_sharedResource.fileSystems[5].drive)
		mountIndex = 5;
	else if (g_sharedResource.fileSystems[6].devname &&
		drive == g_sharedResource.fileSystems[6].drive)
		mountIndex = 6;
	else if (g_sharedResource.fileSystems[7].devname &&
		drive == g_sharedResource.fileSystems[7].drive)
		mountIndex = 7;
	else if (g_sharedResource.fileSystems[8].devname &&
		drive == g_sharedResource.fileSystems[8].drive)
		mountIndex = 8;
	else if (g_sharedResource.fileSystems[9].devname &&
		drive == g_sharedResource.fileSystems[9].drive)
		mountIndex = 9;
	else if (g_sharedResource.fileSystems[10].devname &&
		drive == g_sharedResource.fileSystems[10].drive)
		mountIndex = 10;
	else if (g_sharedResource.fileSystems[11].devname &&
		drive == g_sharedResource.fileSystems[11].drive)
		mountIndex = 11;
	else if (g_sharedResource.fileSystems[12].devname &&
		drive == g_sharedResource.fileSystems[12].drive)
		mountIndex = 12;
	else if (g_sharedResource.fileSystems[13].devname &&
		drive == g_sharedResource.fileSystems[13].drive)
		mountIndex = 13;
	else if (g_sharedResource.fileSystems[14].devname &&
		drive == g_sharedResource.fileSystems[14].drive)
		mountIndex = 14;
	else {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 15, 2258, "%s(): Cannot get mountpoint\n",
			"_sceExfatfsVopSetRoot");
#endif
		return (int)0x80010002;
	}
	fileSystem = &g_sharedResource.fileSystems[mountIndex];
	fnode = UVFAT_GetFnode(&g_sharedResource, fileSystem, rootPath);
	if (!fnode)
		return (int)0x8001000c;
	__builtin_memcpy(path, "/", 2);
	nameset = NULL;
	result = sceFatfsPrepareNameset((int)drive, path, &nameset);
	if (result < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 2318,
			"%s: sceFatfsPrepareNameset failed[%#x]\n",
			"_sceExfatfsVopSetRoot", result);
#endif
		goto release;
	}
	cacheFnode = &fileSystem->fnode;
	SCE_DBG_ASSERT_FUNC(1, 544, "_initialize_cluster_cache_data");
	cacheFileSystem = cacheFnode->fileSystem;
	cacheFnode->tailCluster = -2;
	cacheFnode->tailIndex = -2;
	cacheFnode->clusterCount = 0;
	cacheFnode->chunkCount = 0;
	element = cacheFnode->cluster_cache_array;
	if (cacheFileSystem->mount_mode & 0x400) {
		if (!element) {
			element = allocateCommonHeapMemory((int)cacheFileSystem->drive, 512);
			cacheFnode->cluster_cache_array = element;
			if (!element) {
#if FW_VERSION >= 0x04000000
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 529, "_setup_cluster_cache_data",
					"%s: sceFatfsCreateHeapVl failed\n", "_setup_cluster_cache_data");
#endif
				element = cacheFnode->cluster_cache_array;
			}
		}
		if (element) {
			cacheFnode->cache_element_count = 42;
			end = element + 42;
			do {
				SCE_DBG_ASSERT_FUNC(element != NULL, 485,
					"_invalidate_cluster_cache_element");
				if (element) {
					element->cluster = -2;
					element->index = 0;
					element->count = 0;
				}
				++element;
			} while (element != end);
		}
	}
	if (fnode->refCount == 1) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 0, 2324,
			"%s: fnode[%p], _root_cluster[%#lx], StartCluster[%#lx], Size[%#llx]\n",
			"_sceExfatfsVopSetRoot", fnode, _root_cluster(fileSystem),
			fnode->Stat.StartCluster, fnode->Stat.Size);
#endif
		_initialize_cluster_cache_data(fnode);
	} else {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 2322, "%s: unexpected refCount(%d)\n",
			"_sceExfatfsVopSetRoot", fnode->refCount);
#endif
	}
	result = UVFAT_SearchFile(&g_sharedResource, nameset, fileSystem, fnode, 0);
	if (result) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 2366, "%s: UVFAT_Stat failed(%d)\n",
			"_sceExfatfsVopSetRoot", result);
#endif
		result = exfatfsErrorToIoError(result);
	}
#if FW_VERSION >= 0x04000000
	if (fnode->refCount != 1)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2370, "%s: unexpected refCount(%d)\n",
			"_sceExfatfsVopSetRoot", fnode->refCount);
#endif
	if (fnode->refCount == 1) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 0, 2376,
			"%s: fnode[%p], _root_cluster[%#lx], StartCluster[%#lx], Size[%#llx]\n",
			"_sceExfatfsVopSetRoot", fnode, _root_cluster(fileSystem),
			fnode->Stat.StartCluster, fnode->Stat.Size);
#endif
		_set_root_cluster_and_cluster_count(fnode, _root_cluster(fileSystem), fnode->Stat.Size);
	} else {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 2374, "%s: unexpected refCount(%d)\n",
			"_sceExfatfsVopSetRoot", fnode->refCount);
#endif
	}
	freeHeapBuffer((int)drive, nameset);
	if (result < 0)
		goto release;
#if FW_VERSION >= 0x04000000
	if (fnode->fileSystem->fileSystemType != 2) {
		__builtin_memcpy(systemMount, "ur0:", 5);
		if (!strncmp(drive->assignName, systemMount, 5))
			SCE_DBG_PRINTF_LEVEL(1, 0, 2392,
				"%s: assignName(%s) isn't exFAT, please format\n",
				"_sceExfatfsVopSetRoot", drive->assignName);
	}
#endif
	*root = fnode;
	return result;
release:
	UVFAT_ReleaseFnode(&g_sharedResource, fnode);
	return result;
}

int _sceExfatfsVopGetRoot(SceExfatfsDrive *drive, SceExfatfsFnode **root)
{
	SceExfatfsFileSystem *fileSystem;
	SceExfatfsFnode *fnode;
	SceUInt16 rootPath[2];
	int mountIndex;

	rootPath[0] = '/';
	rootPath[1] = 0;
	if (g_sharedResource.fileSystems[0].devname &&
		drive == g_sharedResource.fileSystems[0].drive)
		mountIndex = 0;
	else if (g_sharedResource.fileSystems[1].devname &&
		drive == g_sharedResource.fileSystems[1].drive)
		mountIndex = 1;
	else if (g_sharedResource.fileSystems[2].devname &&
		drive == g_sharedResource.fileSystems[2].drive)
		mountIndex = 2;
	else if (g_sharedResource.fileSystems[3].devname &&
		drive == g_sharedResource.fileSystems[3].drive)
		mountIndex = 3;
	else if (g_sharedResource.fileSystems[4].devname &&
		drive == g_sharedResource.fileSystems[4].drive)
		mountIndex = 4;
	else if (g_sharedResource.fileSystems[5].devname &&
		drive == g_sharedResource.fileSystems[5].drive)
		mountIndex = 5;
	else if (g_sharedResource.fileSystems[6].devname &&
		drive == g_sharedResource.fileSystems[6].drive)
		mountIndex = 6;
	else if (g_sharedResource.fileSystems[7].devname &&
		drive == g_sharedResource.fileSystems[7].drive)
		mountIndex = 7;
	else if (g_sharedResource.fileSystems[8].devname &&
		drive == g_sharedResource.fileSystems[8].drive)
		mountIndex = 8;
	else if (g_sharedResource.fileSystems[9].devname &&
		drive == g_sharedResource.fileSystems[9].drive)
		mountIndex = 9;
	else if (g_sharedResource.fileSystems[10].devname &&
		drive == g_sharedResource.fileSystems[10].drive)
		mountIndex = 10;
	else if (g_sharedResource.fileSystems[11].devname &&
		drive == g_sharedResource.fileSystems[11].drive)
		mountIndex = 11;
	else if (g_sharedResource.fileSystems[12].devname &&
		drive == g_sharedResource.fileSystems[12].drive)
		mountIndex = 12;
	else if (g_sharedResource.fileSystems[13].devname &&
		drive == g_sharedResource.fileSystems[13].drive)
		mountIndex = 13;
	else if (g_sharedResource.fileSystems[14].devname &&
		drive == g_sharedResource.fileSystems[14].drive)
		mountIndex = 14;
	else {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 15, 2414, "%s(): Cannot get mountpoint\n",
			"_sceExfatfsVopGetRoot");
#endif
		return (int)0x80010002;
	}
	fileSystem = &g_sharedResource.fileSystems[mountIndex];
#if FW_VERSION >= 0x04000000
	if (*root)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2419, "%s: unexpected vnode_data[%p]\n",
			"_sceExfatfsVopGetRoot", *root);
#endif
	fnode = UVFAT_GetFnode(&g_sharedResource, fileSystem, rootPath);
	*root = fnode;
	if (!fnode) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 2423, "%s: UVFAT_GetFnode failed\n",
			"_sceExfatfsVopGetRoot");
#endif
		return (int)0x80010002;
	}
#if FW_VERSION >= 0x04000000
	if (fnode->refCount <= 1)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2431,
			"%s: unexpected refCount(%d) of fnode[%p]\n",
			"_sceExfatfsVopGetRoot", fnode->refCount, fnode);
#endif
	return 0;
}

int sceExfatfsVopInactive(SceExfatfsFnode *fnode)
{
	int result;
#if FW_VERSION >= 0x04000000
	SceUInt16 *path;
	unsigned int character;
	int refCount;
	short fdCount;

	if (_deleted_entry(fnode)) {
		fdCount = fnode->fdCount;
		refCount = fnode->refCount;
		if (refCount - fdCount != 1) {
			if (fnode->FilePath[0]) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 2500,
					"%s: renamed fnode[%p], refCount(%hd), fdCount(%hd)\n",
					"sceExfatfsVopInactive", fnode, refCount, fdCount);
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 134, "_print_fnode",
					"%s: fnode[%p], refCount(%hd), fdCount(%hd), path(",
					"sceExfatfsVopInactive", fnode, fnode->refCount, fnode->fdCount);
				path = fnode->FilePath;
				character = *path;
				if (character)
					do {
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 136, "_print_fnode", "%lc", character);
						character = *++path;
					} while (character);
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 139, "_print_fnode",
					"), FilePath[%p], FileName[%p]\n", fnode->FilePath, fnode->FileName);
			} else if (fdCount) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 2493,
					"%s: deleted fnode[%p], refCount(%hd), fdCount(%hd)\n",
					"sceExfatfsVopInactive", fnode, refCount, fdCount);
			} else {
				SCE_DBG_PRINTF_LEVEL(2, 0, 2496,
					"%s: deleted fnode[%p], refCount(%hd), fdCount(%hd)\n",
					"sceExfatfsVopInactive", fnode, refCount, fdCount);
			}
		}
	}
#else
	if (fnode->FilePath[0] && !_has_parent_directory(fnode)) {
		if (fnode->fileSystem && fnode->Stat.st_type == 6)
			_root_cluster(fnode->fileSystem);
	}
#endif
	result = UVFAT_ReleaseFnode(&g_sharedResource, fnode);
	if (result < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 2512,
			"%s: UVFAT_ReleaseFnode for fnode[%p] failed[%#x]\n",
			"sceExfatfsVopInactive", fnode, result);
#endif
		return exfatfsErrorToIoError(result);
	}
	return 0;
}

extern void freePathBuffer(int unused, void *buffer);
extern int UVFAT_ToUpperBuf(SceUInt16 *destination, const SceUInt16 *source,
	int count, SceExfatfsFileSystem *fileSystem);

int sceExfatfsVopDecodePathElem(SceExfatfsDrive *drive, const unsigned char *path,
	SceSize *offset, SceSize *length, unsigned char *destination, SceSize size)
{
	SceFatfsNameset *nameset = NULL;
	SceExfatfsFileSystem *fileSystem;
	unsigned char *buffer;
	unsigned char *element;
	const unsigned char *input;
	SceUInt16 *name;
	SceUInt16 *end;
	int result;
	int converted;
	SceSize elementLength;
	int upperResult;
	int count;
	int mountIndex;

	result = sceFatfsPrepareNameset((int)drive, path, &nameset);
	*offset = 0;
	*length = 0;
	if (result) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 2674,
			"%s: sceFatfsPrepareNameset for path(%s), failed[%#x]\n",
			"sceExfatfsVopDecodePathElem", path, result);
#endif
		return result;
	}
	buffer = allocateHeapBuffer(drive);
	if (!buffer) {
		result = (int)0x8001000c;
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 2669, "%s: sceFatfsCreateHeap failed\n",
			"sceExfatfsVopDecodePathElem");
#endif
		goto freeNameset;
	}
	converted = _utf16_to_utf8(buffer, 0x800, nameset->filePath);
#if FW_VERSION >= 0x04000000
	if (!converted)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2608, "%s: unexpected length(%d)\n",
			"sceExfatfsVopDecodePathElem", converted);
#endif
	if (converted < 0) {
		result = converted == -1 ? (int)0x8001005b : (int)0x80010016;
		goto free;
	}
	if (buffer[0] != '/') {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 2618, "%s: unexpected start char_code[%#x]\n",
			"sceExfatfsVopDecodePathElem", buffer[0]);
#endif
		result = (int)0x80010016;
		goto free;
	}
	input = path + *offset;
	while (buffer[1] != *input) {
		*offset = input - path + 1;
		++input;
		if (!*input || *input == '/') {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(1, 0, 2629, "%s: no character\n",
				"sceExfatfsVopDecodePathElem");
#endif
			goto mismatch;
		}
	}
	*length = 0;
	element = buffer + 1;
	if (*element != '/' && *element) {
		if (path[*offset] != *element)
			goto notMatch;
		for (;;) {
			elementLength = element - buffer;
			*length = elementLength;
			++element;
			if (*element == '/' || !*element)
				break;
			if (path[*offset + elementLength] != *element)
				goto notMatch;
		}
	}
	if (!size || !destination)
		goto free;
	if (g_sharedResource.fileSystems[0].devname &&
		drive == g_sharedResource.fileSystems[0].drive)
		mountIndex = 0;
	else if (g_sharedResource.fileSystems[1].devname &&
		drive == g_sharedResource.fileSystems[1].drive)
		mountIndex = 1;
	else if (g_sharedResource.fileSystems[2].devname &&
		drive == g_sharedResource.fileSystems[2].drive)
		mountIndex = 2;
	else if (g_sharedResource.fileSystems[3].devname &&
		drive == g_sharedResource.fileSystems[3].drive)
		mountIndex = 3;
	else if (g_sharedResource.fileSystems[4].devname &&
		drive == g_sharedResource.fileSystems[4].drive)
		mountIndex = 4;
	else if (g_sharedResource.fileSystems[5].devname &&
		drive == g_sharedResource.fileSystems[5].drive)
		mountIndex = 5;
	else if (g_sharedResource.fileSystems[6].devname &&
		drive == g_sharedResource.fileSystems[6].drive)
		mountIndex = 6;
	else if (g_sharedResource.fileSystems[7].devname &&
		drive == g_sharedResource.fileSystems[7].drive)
		mountIndex = 7;
	else if (g_sharedResource.fileSystems[8].devname &&
		drive == g_sharedResource.fileSystems[8].drive)
		mountIndex = 8;
	else if (g_sharedResource.fileSystems[9].devname &&
		drive == g_sharedResource.fileSystems[9].drive)
		mountIndex = 9;
	else if (g_sharedResource.fileSystems[10].devname &&
		drive == g_sharedResource.fileSystems[10].drive)
		mountIndex = 10;
	else if (g_sharedResource.fileSystems[11].devname &&
		drive == g_sharedResource.fileSystems[11].drive)
		mountIndex = 11;
	else if (g_sharedResource.fileSystems[12].devname &&
		drive == g_sharedResource.fileSystems[12].drive)
		mountIndex = 12;
	else if (g_sharedResource.fileSystems[13].devname &&
		drive == g_sharedResource.fileSystems[13].drive)
		mountIndex = 13;
	else if (g_sharedResource.fileSystems[14].devname &&
		drive == g_sharedResource.fileSystems[14].drive)
		mountIndex = 14;
	else {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2543, "_set_case_insensitive_name",
			"%s: NULL fileSystem, drive[%p]\n", "_set_case_insensitive_name", drive);
#endif
		result = (int)0x80010013;
		goto free;
	}
	fileSystem = &g_sharedResource.fileSystems[mountIndex];
	name = nameset->filePath + 1;
	end = name;
	count = 0;
	if (*name != '/' && *name) {
		do {
			++end;
			++count;
		} while (*end != '/' && *end);
	}
	*end = 0;
	upperResult = UVFAT_ToUpperBuf(name, name, count, fileSystem);
	if (upperResult < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2553, "_set_case_insensitive_name",
			"%s: UVFAT_ToUpperBuf failed[%#x]\n", "_set_case_insensitive_name", upperResult);
#endif
		result = upperResult == (int)0x8001005b ? 0 : upperResult;
		goto free;
	}
	converted = _utf16_to_utf8(destination, size, name);
	if (converted == -2)
		result = (int)0x80010016;
	else if (converted == -1)
		result = 0;
	else
		result = upperResult;
	goto free;
notMatch:
#if FW_VERSION >= 0x04000000
	SCE_DBG_PRINTF_LEVEL(1, 0, 2642, "%s: not match\n", "sceExfatfsVopDecodePathElem");
#endif
mismatch:
	result = (int)0x80010016;
	*offset = 0;
	*length = 0;
free:
	freeHeapBuffer((int)drive, buffer);
freeNameset:
	freePathBuffer((int)drive, nameset);
	return result;
}

extern int getFnodeReferenceCount(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, const SceUInt16 *path);

int _exfatfs_reference_count(SceExfatfsFnode *parent, const SceIoDirent *entry, int *error)
{
	SceExfatfsFileSystem *fileSystem;
	SceFatfsNameset *nameset;
	SceFatfsNameset *preparedNameset;
	SceUInt16 *filePath;
	size_t suffixLength;
	int result;
	int references = 0;
#if FW_VERSION >= 0x04000000
	unsigned char *path;
	int drive;

	if (_deleted_entry(parent)) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2691, "%s: parent_fnode has been deleted\n",
			"_exfatfs_reference_count");
		result = (int)0x80010002;
		goto finish;
	}
#else
	if (!parent->FilePath[0]) {
		if (parent->fileSystem) {
			result = (int)0x80010002;
			goto finish;
		}
	} else if (!_has_parent_directory(parent)) {
		if (parent->fileSystem && parent->Stat.st_type == 6)
			_root_cluster(parent->fileSystem);
	}
#endif
	fileSystem = parent->fileSystem;
	suffixLength = strnlen(entry->d_name, 256);
#if FW_VERSION >= 0x04000000
	drive = (int)fileSystem->drive;
	nameset = NULL;
	path = _temporal_utf8_fullpath(drive, parent, entry->d_name, suffixLength);
	if (path) {
		result = sceFatfsPrepareNameset(drive, path, &nameset);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2199, "_temporal_nameset",
				"%s: sceFatfsPrepareNameset failed[%#x]\n", "_temporal_nameset", result);
		freeHeapBuffer(drive, path);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2207, "_temporal_nameset",
			"%s: sceFatfsCreateHeap failed\n", "_temporal_nameset");
	}
#else
	nameset = _temporal_nameset((int)fileSystem->drive, parent,
		entry->d_name, suffixLength);
#endif
	preparedNameset = nameset;
	if (!preparedNameset) {
		result = (int)0x8001000c;
		goto finish;
	}
	filePath = preparedNameset->filePath;
	result = UVFAT_ToUpperBuf(filePath, filePath, 515, fileSystem);
#if FW_VERSION >= 0x04000000
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2711, "%s: UVFAT_ToUpperBuf failed[%#x]\n",
			"_exfatfs_reference_count", result);
#endif
	if (!result)
		references = getFnodeReferenceCount(&g_sharedResource, fileSystem, filePath);
	freeHeapBuffer((int)fileSystem->drive, preparedNameset);
finish:
	if (error)
		*error = result;
	return references;
}

extern int UVFAT_SearchFileFromDir(SceExfatfsSharedResource *sharedResource,
	SceUInt16 *name, SceExfatfsFileSystem *fileSystem, SceExfatfsFnode *fnode);
extern int _set_start_cluster_and_cluster_count(SceExfatfsFnode *fnode,
	SceUInt32 cluster, SceUInt64 size);
extern unsigned int lookupUpcaseCharacter(unsigned int unicode, SceExfatfsUpcaseTable *table);

int sceExfatfsVopLookup(SceExfatfsFnode *parent, const void *suffix,
	int suffixLength, SceExfatfsFnode **output)
{
	SceExfatfsFileSystem *fileSystem;
	SceFatfsNameset *nameset = NULL;
	SceFatfsNameset *preparedNameset;
	SceExfatfsFnode *fnode;
	unsigned char *path;
	int drive;
	int result;
	int released;
#if FW_VERSION >= 0x04000000
	const char *assignName;
	SceExfatfsDrive *nodeDrive;
	SceExfatfsFnode *alias;
	SceUInt16 *nodePath;
	SceUInt16 *aliasPath;
	SceUInt16 *printPath;
	unsigned int nodeChar;
	unsigned int aliasChar;
	unsigned int nodeUpper;
	unsigned int count;
	unsigned int i;
	int questionable = 0;
	int locked;
#endif

#if FW_VERSION >= 0x04000000
	if (_directory_vnode_check(parent, "sceExfatfsVopLookup") < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2746, "%s: parent_vnode_data[%p] isn't directory\n",
			"sceExfatfsVopLookup", parent);
		return (int)0x80010014;
	}
#else
	if (parent->Stat.st_type != 6) {
		SceUInt16 *name = parent->FilePath;

		while (*name)
			++name;
		return (int)0x80010014;
	}
	if (parent->DirCluster == (SceUInt32)-2 &&
		_root_cluster(parent->fileSystem) != parent->Stat.StartCluster)
		return (int)0x80010014;
#endif
	if (!parent->FileName)
		return (int)0x80010002;
	fileSystem = parent->fileSystem;
	SCE_DBG_ASSERT(suffixLength > 0, 2759);
	drive = (int)fileSystem->drive;
#if FW_VERSION >= 0x04000000
	path = _temporal_utf8_fullpath(drive, parent, suffix, suffixLength);
	if (path) {
		result = sceFatfsPrepareNameset(drive, path, &nameset);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2199, "_temporal_nameset",
				"%s: sceFatfsPrepareNameset failed[%#x]\n", "_temporal_nameset", result);
		freeHeapBuffer(drive, path);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2207, "_temporal_nameset",
			"%s: sceFatfsCreateHeap failed\n", "_temporal_nameset");
	}
#else
	{
		unsigned int remaining;
		int length;

		path = allocateHeapBuffer((SceExfatfsDrive *)drive);
		if (path) {
			memset(path, 0, 0x800);
			length = _utf16_to_utf8(path, 0x800, parent->FilePath);
			if (length < 0)
				goto freePath;
			if (length == 1) {
				remaining = 0x7ff;
			} else {
				remaining = 0x800 - length;
				if (suffix) {
					path[length] = '/';
					++length;
					--remaining;
				}
			}
			if ((unsigned int)suffixLength >= remaining)
				goto freePath;
			if (suffixLength) {
				memcpy(path + length, suffix, suffixLength);
				length += suffixLength;
			}
			path[length] = 0;
			sceFatfsPrepareNameset(drive, path, &nameset);
freePath:
			freeHeapBuffer(drive, path);
		}
	}
#endif
	preparedNameset = nameset;
	if (output)
		*output = NULL;
	if (!preparedNameset)
		return (int)0x8001000c;
	fnode = UVFAT_GetFnode(&g_sharedResource, fileSystem, preparedNameset->filePath);
	if (!fnode) {
		result = (int)0x8001000c;
		goto freeNameset;
	}
#if FW_VERSION >= 0x04000000
	if (fnode->fileSystem != parent->fileSystem)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2819, "%s: FileSystem[%p] != expected[%p]\n",
			"sceExfatfsVopLookup", fnode->fileSystem, parent->fileSystem);
#endif
	if (fnode->refCount == 1) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 0, 2853,
			"%s: fnode[%p], uninitialized StartCluster[%#lx], uninitialized Size[%#llx]\n",
			"sceExfatfsVopLookup", fnode, fnode->Stat.StartCluster, fnode->Stat.Size);
#endif
		_initialize_cluster_cache_data(fnode);
	} else {
#if FW_VERSION >= 0x04000000
		if (output) {
			nodeDrive = fnode->fileSystem->drive;
			assignName = nodeDrive ? nodeDrive->assignName : NULL;
			if (fnode->fdCount) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 2831,
					"%s: unexpected fdCount(%hd), refCount(%hd), elem(%s), assignName(%s), fnode[%p]\n",
					"sceExfatfsVopLookup", fnode->fdCount, fnode->refCount,
					suffix, assignName, fnode);
			} else if (fnode->refCount <= 255) {
				SCE_DBG_PRINTF_LEVEL(2, 0, 2840,
					"%s: questionable refCount(%hd), fdCount(%hd), elem(%s), assignName(%s), fnode[%p]\n",
					"sceExfatfsVopLookup", fnode->refCount, fnode->fdCount,
					suffix, assignName, fnode);
				questionable = 1;
			} else {
				SCE_DBG_PRINTF_LEVEL(1, 0, 2842,
					"%s: questionable refCount(%hd), fdCount(%hd), elem(%s), assignName(%s), fnode[%p], please report to Bug 37633\n",
					"sceExfatfsVopLookup", fnode->refCount, fnode->fdCount,
					suffix, assignName, fnode);
			}
		} else if (parent != parent->fileSystem->whiteout_directory) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 2849,
				"%s: unexpected refCount(%hd) for exist check, fdCount(%hd)\n",
				"sceExfatfsVopLookup", fnode->refCount, fnode->fdCount);
		}
#endif
	}
	fnode->Stat = parent->Stat;
	fnode->DirCluster = parent->Stat.StartCluster;
	fnode->DirOffset = 0;
	fnode->DirOffClust = parent->Stat.StartCluster;
	fnode->dirGeneralSecondaryFlags = parent->Stat.generalSecondaryFlags;
	fnode->dirSize = parent->Stat.Size;
	result = UVFAT_SearchFileFromDir(&g_sharedResource, preparedNameset->fileName, fileSystem, fnode);
	if (result < 0) {
		released = UVFAT_ReleaseFnode(&g_sharedResource, fnode);
#if FW_VERSION >= 0x04000000
		if (released < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0, 2866,
				"%s: UVFAT_ReleaseFnode failed(%d) after error(%d) of UVFAT_SearchFileFromDir\n",
				"sceExfatfsVopLookup", released, result);
#endif
		result = exfatfsErrorToIoError(result);
		goto freeNameset;
	}
	if (!output) {
		released = UVFAT_ReleaseFnode(&g_sharedResource, fnode);
#if FW_VERSION >= 0x04000000
		if (released)
			SCE_DBG_PRINTF_LEVEL(1, 0, 2900,
				"%s: UVFAT_ReleaseFnode after exist check failed\n", "sceExfatfsVopLookup");
#endif
		goto freeNameset;
	}
	if (fnode->refCount - fnode->fdCount == 1) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 0, 2882,
			"%s: fnode[%p], StartCluster[%#lx], Size[%#llx]\n",
			"sceExfatfsVopLookup", fnode, fnode->Stat.StartCluster, fnode->Stat.Size);
		if (fnode->Stat.st_type && fnode->Stat.st_type != 6)
			SCE_DBG_PRINTF_LEVEL(1, 0, 2888,
				"%s: unexpected type(%d), fnode[%p], StartCluster[%#lx], Size[%#llx], mode[%#o]\n",
				"sceExfatfsVopLookup", fnode->Stat.st_type, fnode, fnode->Stat.StartCluster,
				fnode->Stat.Size, fnode->Stat.st_mode);
		locked = lockFastMutex(&g_sharedResourceMutex);
		if (locked < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 99, "_exfat_lock_shared_resource",
				"%s: sceKernelLockFastMutex failed[%#x]\n", "_check_alias_fnode", locked);
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 330, "_check_alias_fnode",
				"%s: _exfat_lock_shared_resource failed[%#x]\n", "_check_alias_fnode", locked);
		} else {
			count = getInitializedFnodeCount(&g_sharedResource);
			for (i = 0; i != count; ++i) {
				alias = &g_sharedResource.fnodes[i];
				if (!alias->refCount || alias->fileSystem != fnode->fileSystem || alias == fnode)
					continue;
				nodePath = fnode->FilePath;
				aliasPath = alias->FilePath;
				while (*nodePath && *aliasPath) {
					nodeChar = *nodePath;
					aliasChar = *aliasPath;
					if (nodeChar != aliasChar) {
						if (fnode->fileSystem->fileSystemType == 2) {
							nodeUpper = lookupUpcaseCharacter(nodeChar, &fnode->fileSystem->upcase);
							if (nodeUpper != lookupUpcaseCharacter(aliasChar, &fnode->fileSystem->upcase))
								break;
						} else {
							if (nodeChar - 'a' <= 'z' - 'a')
								nodeChar -= 'a' - 'A';
							if (aliasChar - 'a' <= 'z' - 'a')
								aliasChar -= 'a' - 'A';
							if (nodeChar != aliasChar)
								break;
						}
					}
					++nodePath;
					++aliasPath;
				}
				if (*nodePath != *aliasPath)
					continue;
				if (_deleted_entry(alias)) {
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 342, "_check_alias_fnode",
						"%s: deleted fnode[%p], new_fnode[%p], elem(%s)\n",
						"_check_alias_fnode", alias, fnode, suffix);
				} else {
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 345, "_check_alias_fnode",
						"%s: already existing fnode[%p], new_fnode[%p], elem(%s), please report to Bug 37633\n",
						"_check_alias_fnode", alias, fnode, suffix);
				}
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 134, "_print_fnode",
					"%s: fnode[%p], refCount(%hd), fdCount(%hd), path(",
					"_check_alias_fnode", alias, alias->refCount, alias->fdCount);
				printPath = alias->FilePath;
				while (*printPath) {
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 136, "_print_fnode", "%lc", *printPath);
					++printPath;
				}
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 139, "_print_fnode",
					"), FilePath[%p], FileName[%p]\n", alias->FilePath, alias->FileName);
			}
			locked = sceKernelUnlockFastMutex(&g_sharedResourceMutex);
			if (locked < 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 119, "_exfat_unlock_shared_resource",
					"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_check_alias_fnode", locked);
		}
#endif
		_set_start_cluster_and_cluster_count(fnode, fnode->Stat.StartCluster, fnode->Stat.Size);
	} else {
#if FW_VERSION >= 0x04000000
		if (!questionable) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 2876,
				"%s: alias vnode will be created for fnode[%p], refCount(%hd), fdCount(%hd), StartCluster[%#lx], Size[%#llx]\n",
				"sceExfatfsVopLookup", fnode, fnode->refCount, fnode->fdCount,
				fnode->Stat.StartCluster, fnode->Stat.Size);
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 134, "_print_fnode",
					"%s: fnode[%p], refCount(%hd), fdCount(%hd), path(",
					"sceExfatfsVopLookup", fnode, fnode->refCount, fnode->fdCount);
				printPath = fnode->FilePath;
				while (*printPath) {
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 136, "_print_fnode", "%lc", *printPath);
					++printPath;
				}
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 139, "_print_fnode",
					"), FilePath[%p], FileName[%p]\n", fnode->FilePath, fnode->FileName);
		}
#endif
	}
	*output = fnode;
freeNameset:
	freeHeapBuffer((int)fileSystem->drive, preparedNameset);
	return result;
}

/* Sony's whiteout destination probe returns ENOENT unconditionally. */
int probeWhiteoutDestination(SceExfatfsFnode *parent, const SceVfsPath *path)
{
	return 0x80010002;
}

int sceExfatfsVopGetstat(SceExfatfsFnode *fnode, SceIoStat *stat)
{
	struct SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
#if FW_VERSION >= 0x04000000
	int refCount;
	int fdCount;
#endif

	memset(stat, 0, sizeof(*stat));
#if FW_VERSION >= 0x04000000
	if (_deleted_entry(fnode)) {
		fdCount = fnode->fdCount;
		refCount = fnode->refCount;
		SCE_DBG_PRINTF_LEVEL(1, 0, 2949,
			"%s: deleted fnode[%p], refCount(%hd), fdCount(%hd)\n",
			"sc", fnode, refCount, fdCount);
	}
#else
	if (fnode->FilePath[0] && !_has_parent_directory(fnode)) {
		if (fnode->fileSystem && fnode->Stat.st_type == 6)
			_root_cluster(fnode->fileSystem);
	}
#endif
	exfatfsStatToIoStat(fileSystem->drive, &fnode->Stat, stat);
	return 0;
}

extern int closeFileDescriptor(SceExfatfsSharedResource *sharedResource, unsigned int fd);

#if FW_VERSION >= 0x04000000
int sceExfatfsVopDclose(unsigned int fd)
{
	SceExfatfsFnode *fnode;
	int result;

	if (!fd || fd >= getInitializedFileCount(&g_sharedResource)) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1864, "_directory_fd_check",
			"%s: bad fd(%d)\n", "sceExfatfsVopDclose", fd);
		return exfatfsErrorToIoError(-48);
	}
	if (g_sharedResource.files[fd].mode != 3) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1868, "_directory_fd_check",
			"%s: mode(%d) != expected(%d)\n", "sceExfatfsVopDclose",
			g_sharedResource.files[fd].mode, 3);
		return exfatfsErrorToIoError(-2);
	}
	result = _directory_vnode_check(g_sharedResource.files[fd].fnode, "sceExfatfsVopDclose");
	if (result)
		return exfatfsErrorToIoError(result);
	fnode = g_sharedResource.files[fd].fnode;
	if (fnode->fileSystem && fnode->fileSystem->drive &&
	    (fnode->fileSystem->drive->state & 0x4000))
		SCE_DBG_PRINTF_LEVEL(1, 0, 2973,
			"%s: force_umounting, fnode[%p], refCount(%hd), fdCount(%hd), ret[%#x]\n",
			"sceExfatfsVopDclose", fnode, fnode->refCount, fnode->fdCount, 0);
	if (_deleted_entry(fnode))
		SCE_DBG_PRINTF_LEVEL(1, 0, 2979,
			"%s: deleted fnode[%p], refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopDclose", fnode, fnode->refCount, fnode->fdCount);
	result = closeFileDescriptor(&g_sharedResource, fd);
	if (result)
		return exfatfsErrorToIoError(result);
	return result;
}
#else
int sceExfatfsVopDclose(unsigned int fd)
{
	SceExfatfsFnode *fnode;
	SceUInt16 *path;
	int result;

	if (!fd || fd >= getInitializedFileCount(&g_sharedResource))
		return exfatfsErrorToIoError(-48);
	if (g_sharedResource.files[fd].mode != 3)
		return exfatfsErrorToIoError(-2);
	fnode = g_sharedResource.files[fd].fnode;
	if (fnode->Stat.st_type != 6) {
		path = fnode->FilePath;
		while (*path)
			++path;
		return exfatfsErrorToIoError(-6);
	}
	if (fnode->DirCluster == (SceUInt32)-2) {
		if (_root_cluster(fnode->fileSystem) != fnode->Stat.StartCluster)
			return exfatfsErrorToIoError(-4);
		fnode = g_sharedResource.files[fd].fnode;
	}
	if (fnode->FilePath[0] && !_has_parent_directory(fnode)) {
		if (fnode->fileSystem && fnode->Stat.st_type == 6)
			_root_cluster(fnode->fileSystem);
	}
	result = closeFileDescriptor(&g_sharedResource, fd);
	if (result)
		return exfatfsErrorToIoError(result);
	return result;
}
#endif

extern int UVFAT_FlushFdBuf(SceExfatfsFile *file, struct SceExfatfsFileSystem *fileSystem);
extern int _exfatfs_flush_metadata(SceExfatfsSharedResource *sharedResource, SceExfatfsFnode *fnode);
extern int flushFileSystemMetadata(struct SceExfatfsFileSystem *fileSystem);

int sceExfatfsVopClose(unsigned int fd)
{
	SceExfatfsFile *files;
	SceExfatfsFile *file;
	SceExfatfsFnode *fnode;
	SceExfatfsFnode *flushFnode;
	unsigned int mode;
	int result;
#if FW_VERSION < 0x04000000
	SceUInt16 *path;
#endif

	if (!fd || fd >= getInitializedFileCount(&g_sharedResource)) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1923, "_file_fd_check",
			"%s: bad fd(%d)\n", "sceExfatfsVopClose", fd);
#endif
		result = -48;
		goto error;
	}
	files = g_sharedResource.files;
	file = &files[fd];
	mode = file->mode;
	if (mode != 6 && mode - 1 > 1) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1929, "_file_fd_check",
			"%s: mode(%d) != expected(%d, %d or %d)\n",
			"sceExfatfsVopClose", mode, 1, 2, 6);
#endif
		result = -2;
		goto error;
	}
#if FW_VERSION >= 0x04000000
	result = _file_vnode_check(files[fd].fnode, "sceExfatfsVopClose");
	if (result)
		goto error;
	files = g_sharedResource.files;
	fnode = files[fd].fnode;
	file = &files[fd];
	if (!fnode->fileSystem)
		goto close;
	if (fnode->fileSystem->drive && (fnode->fileSystem->drive->state & 0x4000)) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 3041,
			"%s: force_umounting, fnode[%p], refCount(%hd), fdCount(%hd), ret[%#x]\n",
			"sceExfatfsVopClose", fnode, fnode->refCount, fnode->fdCount, result);
		if (!fnode->fileSystem)
			goto close;
	}
	mode = file->mode;
	if (_deleted_entry(fnode))
		SCE_DBG_PRINTF_LEVEL(1, 0, 3049,
			"%s: deleted fnode[%p], refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopClose", fnode, fnode->refCount, fnode->fdCount);
#else
	fnode = files[fd].fnode;
	if (fnode->Stat.st_type) {
		path = fnode->FilePath;
		while (*path)
			++path;
		result = -6;
		goto error;
	}
	if (fnode->DirCluster == (SceUInt32)-2) {
		result = -4;
		goto error;
	}
	if (!fnode->fileSystem)
		goto close;
	if (fnode->FilePath[0] && !_has_parent_directory(fnode)) {
		if (fnode->fileSystem && fnode->Stat.st_type == 6)
			_root_cluster(fnode->fileSystem);
	}
#endif
	if ((mode & ~4U) == 2) {
		flushFnode = files[fd].fnode;
		result = UVFAT_FlushFdBuf(file, flushFnode->fileSystem);
		if (result) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3022, "_flush_file", "flush write buf Fail\n");
#endif
			flushFileSystemMetadata(flushFnode->fileSystem);
		} else {
			result = _exfatfs_flush_metadata(&g_sharedResource, flushFnode);
			if (result) {
#if FW_VERSION >= 0x04000000
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3015, "_flush_file", "Update meta data Fail\n");
#endif
			} else {
				flushFnode->flags &= ~4;
				result = flushFileSystemMetadata(flushFnode->fileSystem);
			}
		}
		if (result)
			goto error;
	}
close:
	result = closeFileDescriptor(&g_sharedResource, fd);
	if (!result)
		return result;
error:
	return exfatfsErrorToIoError(result);
}

SceOff _exfatfs_vnode_size(SceExfatfsFnode *fnode)
{
#if FW_VERSION >= 0x04000000
	SceExfatfsFileSystem *fileSystem = fnode->fileSystem;

	SCE_DBG_PRINTF_LEVEL(2, 0, 3078,
		"%s: Size[%#llx], cluster_count[%#lx], cluster_size[%#x]\n",
		"_exfatfs_vnode_size", fnode->Stat.Size, fnode->clusterCount,
		fileSystem->clusterSectorCount * fileSystem->sectorSize);
#endif
	return fnode->Stat.Size;
}

SceOff _exfatfs_file_size(unsigned int fd)
{
	SceExfatfsFile *files = g_sharedResource.files;
#if FW_VERSION >= 0x04000000
	SceExfatfsFnode *fnode = files[fd].fnode;
	SceExfatfsFileSystem *fileSystem = fnode->fileSystem;

	SCE_DBG_PRINTF_LEVEL(2, 0, 3088,
		"%s: Size[%#llx], cluster_count[%#lx], cluster_size[%#x]\n",
		"_exfatfs_file_size", fnode->Stat.Size, fnode->clusterCount,
		fileSystem->sectorSize * fileSystem->clusterSectorCount);
#endif
	return files[fd].fnode->Stat.Size;
}

int sceExfatfsVopLseek(unsigned int fd, SceOff offset, int whence, SceOff *position)
{
	SceExfatfsFile *files;
	SceExfatfsFile *file;
	SceExfatfsFnode *fnode;
	unsigned int mode;
	SceOff nextPosition;
	int result;
#if FW_VERSION < 0x04000000
	SceUInt16 *path;
#endif

	if (!fd || fd >= getInitializedFileCount(&g_sharedResource)) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1923, "_file_fd_check",
			"%s: bad fd(%d)\n", "sceExfatfsVopLseek", fd);
#endif
		result = -48;
		goto error;
	}
	files = g_sharedResource.files;
	file = &files[fd];
	mode = file->mode;
	if (mode != 6 && mode - 1 > 1) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1929, "_file_fd_check",
			"%s: mode(%d) != expected(%d, %d or %d)\n",
			"sceExfatfsVopLseek", mode, 1, 2, 6);
#endif
		result = -2;
		goto error;
	}
#if FW_VERSION >= 0x04000000
	result = _file_vnode_check(files[fd].fnode, "sceExfatfsVopLseek");
	if (result)
		goto error;
	files = g_sharedResource.files;
	file = &files[fd];
	if (_deleted_entry(files[fd].fnode)) {
		fnode = files[fd].fnode;
		SCE_DBG_PRINTF_LEVEL(1, 0, 3114,
			"%s: deleted fnode[%p], refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopLseek", fnode, fnode->refCount, fnode->fdCount);
	}
#else
	fnode = files[fd].fnode;
	if (fnode->Stat.st_type) {
		path = fnode->FilePath;
		while (*path)
			++path;
		result = -6;
		goto error;
	}
	if (fnode->DirCluster == (SceUInt32)-2) {
		result = -4;
		goto error;
	}
	if (fnode->FilePath[0] && !_has_parent_directory(fnode)) {
		if (fnode->fileSystem && fnode->Stat.st_type == 6)
			_root_cluster(fnode->fileSystem);
	}
	result = 0;
#endif
	switch (whence) {
	case 1:
		nextPosition = file->Position + offset;
		break;
	case 2:
		nextPosition = files[fd].fnode->Stat.Size + offset;
		break;
	case 0:
		nextPosition = offset;
		break;
	default:
		return 0x80010016;
	}
	if ((SceUInt64)nextPosition >= 0x100000000ULL &&
	    files[fd].fnode->fileSystem->fileSystemType != 2)
		return 0x8001001b;
	file->Position = nextPosition;
	*position = nextPosition;
	return result;
error:
	return exfatfsErrorToIoError(result);
}

static inline int _read_fd_check(unsigned int fd, const char *caller)
{
	unsigned int mode;
#if FW_VERSION < 0x04000000
	SceExfatfsFnode *fnode;
	SceUInt16 *path;
#endif

	if (!fd || fd >= getInitializedFileCount(&g_sharedResource)) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 1883, "%s: bad fd(%d)\n", caller, fd);
#endif
		return -48;
	}
	mode = g_sharedResource.files[fd].mode;
	if (mode != 6 && mode != 1) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 1888, "%s: mode(%d) != expected(%d or %d)\n",
			caller, mode, 1, 6);
#endif
		return -2;
	}
#if FW_VERSION >= 0x04000000
	return _file_vnode_check(g_sharedResource.files[fd].fnode, caller);
#else
	fnode = g_sharedResource.files[fd].fnode;
	if (fnode->Stat.st_type) {
		path = fnode->FilePath;
		while (*path)
			++path;
		return -6;
	}
	if (fnode->DirCluster == (SceUInt32)-2)
		return -4;
	return 0;
#endif
}

extern int UVFAT_Read(SceExfatfsSharedResource *sharedResource,
	unsigned int fd, void *buffer, unsigned int size);

int sceExfatfsVopPread(unsigned int fd, void *buffer, unsigned int size,
	SceOff offset, unsigned int *readSize)
{
	SceExfatfsFile *files;
	SceExfatfsFile *file;
	SceExfatfsFnode *fnode;
	SceOff oldPosition;
	int result;
	int read;

	result = _read_fd_check(fd, "sceExfatfsVopPread");
	*readSize = 0;
	if (result) {
		result = exfatfsErrorToIoError(result);
	} else {
		files = g_sharedResource.files;
		file = &files[fd];
#if FW_VERSION >= 0x04000000
		if (_deleted_entry(file->fnode)) {
			fnode = files[fd].fnode;
			SCE_DBG_PRINTF_LEVEL(1, 0, 3321,
				"%s: deleted fnode[%p], refCount(%hd), fdCount(%hd)\n",
				"sceExfatfsVopPread", fnode, fnode->refCount, fnode->fdCount);
		}
#else
		fnode = file->fnode;
		if (fnode->FilePath[0] && !_has_parent_directory(fnode)) {
			if (fnode->fileSystem && fnode->Stat.st_type == 6)
				_root_cluster(fnode->fileSystem);
		}
#endif
		if (size && (SceUInt64)offset < (SceUInt64)files[fd].fnode->Stat.Size) {
			oldPosition = file->Position;
			file->Position = offset;
			read = UVFAT_Read(&g_sharedResource, fd, buffer, size);
			if (read < 0)
				result = exfatfsErrorToIoError(read);
			else {
				*readSize = read;
				result = 0;
			}
			file->Position = oldPosition;
		}
	}
	_read_fd_check(fd, "sceExfatfsVopPread(exit)");
	return result;
}

static inline int _write_fd_check(unsigned int fd, const char *caller)
{
	unsigned int mode;
#if FW_VERSION < 0x04000000
	SceExfatfsFnode *fnode;
	SceUInt16 *path;
#endif

	if (!fd || fd >= getInitializedFileCount(&g_sharedResource)) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 1903, "%s: bad fd(%d)\n", caller, fd);
#endif
		return -48;
	}
	mode = g_sharedResource.files[fd].mode;
	if ((mode & ~4U) != 2) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 1908, "%s: mode(%d) != expected(%d or %d)\n",
			caller, mode, 2, 6);
#endif
		return -2;
	}
#if FW_VERSION >= 0x04000000
	return _file_vnode_check(g_sharedResource.files[fd].fnode, caller);
#else
	fnode = g_sharedResource.files[fd].fnode;
	if (fnode->Stat.st_type) {
		path = fnode->FilePath;
		while (*path)
			++path;
		return -6;
	}
	if (fnode->DirCluster == (SceUInt32)-2)
		return -4;
	return 0;
#endif
}


#if FW_VERSION < 0x04000000
static inline int _check_free_count(SceExfatfsFnode *fnode, unsigned long long newSize)
{
	struct SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	unsigned long long clusterSize = fileSystem->sectorSize * fileSystem->clusterSectorCount;
	SceUInt32 clusterCount = fileSystem->clusterCount;
	SceUInt32 newCount;
	SceUInt32 oldCount;
	SceUInt32 check;
	SceUInt32 checkStart;
	SceUInt32 found;
	int result;

	if (newSize >= 0x100000000ULL && fileSystem->fileSystemType != 2)
		return 0x8001001b;
	if (clusterSize * clusterCount < newSize)
		return 0x8001001c;
	newCount = (clusterSize - 1 + newSize) / clusterSize;
	oldCount = (fnode->Stat.Size - 1 + clusterSize) / clusterSize;
	if (newCount <= oldCount)
		return 0;
	check = newCount - oldCount + fileSystem->fakeFreeClusterCount;
	if (fileSystem->fileSystemType == 2) {
		result = UVFAT_readallocationbitmap(&g_sharedResource, fileSystem);
		if (result)
			return exfatfsErrorToIoError(result);
		if (check > fileSystem->bitmapFreeClusterCount)
			return 0x8001001c;
		return 0;
	}
	if (check <= fileSystem->freeClusterCount)
		return 0;
	checkStart = fileSystem->nextFreeCluster;
	if (!check)
		return 0;
	for (;;) {
		found = UVFAT_GetFatEmptyClust(fileSystem, checkStart);
		if (found < checkStart || clusterCount + 2 <= found)
			return 0x8001001c;
		checkStart = found + 1;
		if (!--check)
			return 0;
	}
}
#endif

extern int UVFAT_Write(SceExfatfsSharedResource *sharedResource,
	unsigned int fd, const void *buffer, unsigned int size);

int sceExfatfsVopPwrite(unsigned int fd, const void *buffer, unsigned int size,
	SceOff offset, unsigned int *writeSize)
{
	SceExfatfsFile *files;
	SceExfatfsFile *file;
	SceExfatfsFnode *fnode;
	SceOff oldPosition;
	int result;
	int written;
#if FW_VERSION >= 0x04000000
	SceUInt64 fileSize;
	SceUInt64 nextPosition;
#endif

	result = _write_fd_check(fd, "sceExfatfsVopPwrite");
	*writeSize = 0;
	if (result) {
		result = exfatfsErrorToIoError(result);
	} else {
		files = g_sharedResource.files;
		file = &files[fd];
		oldPosition = file->Position;
#if FW_VERSION >= 0x04000000
		if (_deleted_entry(file->fnode)) {
			fnode = files[fd].fnode;
			SCE_DBG_PRINTF_LEVEL(1, 0, 3368,
				"%s: deleted fnode[%p], refCount(%hd), fdCount(%hd)\n",
				"sceExfatfsVopPwrite", fnode, fnode->refCount, fnode->refCount);
		}
#else
		fnode = file->fnode;
		if (fnode->FilePath[0] && !_has_parent_directory(fnode)) {
			if (fnode->fileSystem && fnode->Stat.st_type == 6)
				_root_cluster(fnode->fileSystem);
		}
#endif
		result = _check_free_count(files[fd].fnode, offset + size);
		if (!result) {
			file->Position = offset;
			written = UVFAT_Write(&g_sharedResource, fd, buffer, size);
			if (written < 0) {
				result = exfatfsErrorToIoError(written);
			} else {
#if FW_VERSION >= 0x04000000
				nextPosition = file->Position;
				fileSize = files[fd].fnode->Stat.Size;
#endif
				*writeSize = written;
				result = 0;
#if FW_VERSION >= 0x04000000
				if (fileSize < nextPosition)
					SCE_DBG_PRINTF_LEVEL(1, 0, 3392,
						"%s: Position[%#llx] > size[%#llx], wsize(%ld)\n",
						"sceExfatfsVopPwrite", nextPosition, fileSize, written);
#endif
			}
			file->Position = oldPosition;
		}
	}
	_write_fd_check(fd, "sceExfatfsVopPwrite(exit)");
	return result;
}

extern int _ReserveUnusedFd(SceExfatfsSharedResource *sharedResource, int mode);
extern int _AllocateFdBuffer(SceExfatfsFile *file);
extern void _ReleaseFp(SceExfatfsSharedResource *sharedResource, SceExfatfsFile *file);
extern void _flush_fd_cache(SceExfatfsSharedResource *sharedResource, SceExfatfsFnode *fnode);

SceExfatfsFile *_exfatfs_open_common(SceExfatfsFnode *fnode, int mode,
	SceExfatfsSharedResource *sharedResource, int *error)
{
	SceExfatfsFile *file;
	int result;
#if FW_VERSION >= 0x04000000
	short fdCount;
#else
	unsigned short fdCount;
#endif
	short refCount;
#if FW_VERSION < 0x04000000
	SceUInt16 *path;
#endif

	result = _ReserveUnusedFd(sharedResource, mode);
	switch (mode) {
	case 1:
	case 6:
#if FW_VERSION >= 0x04000000
		_file_vnode_check(fnode, "_exfatfs_open_common(Open, after assign)");
#else
		if (fnode->Stat.st_type) {
			path = fnode->FilePath;
			while (*path)
				++path;
		}
#endif
		break;
	case 3:
#if FW_VERSION >= 0x04000000
		_directory_vnode_check(fnode, "_exfatfs_open_common(OpenDir, after assign)");
#else
		if (fnode->Stat.st_type == 6)
			goto checkDirectoryCluster;
		path = fnode->FilePath;
		while (*path)
			++path;
#endif
		break;
	case 8:
#if FW_VERSION >= 0x04000000
		if (!fnode->Stat.st_type)
			_file_vnode_check(fnode, "_exfatfs_open_common(Chstat, after assign)");
		else if (fnode->Stat.st_type == 6)
			_directory_vnode_check(fnode, "_exfatfs_open_common(Chstat, after assign)");
		else
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1852, "_vnode_check",
				"%s: unsupported st_type(%d)\n",
				"_exfatfs_open_common(Chstat, after assign)", fnode->Stat.st_type);
#else
		if (fnode->Stat.st_type == 6) {
checkDirectoryCluster:
			if (fnode->DirCluster == (SceUInt32)-2)
				_root_cluster(fnode->fileSystem);
		}
#endif
		break;
	default:
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 3422,
			"%s: unsupported open_mode(%d)\n", "_exfatfs_open_common", mode);
#endif
		break;
	}
	if (result < 0) {
		file = NULL;
		result = exfatfsErrorToIoError(result);
	} else if ((unsigned int)result >= getInitializedFileCount(sharedResource)) {
		file = NULL;
	} else {
		fdCount = fnode->fdCount + 1;
		refCount = fnode->refCount + 1;
		fnode->refCount = refCount;
		fnode->fdCount = fdCount;
		if (fdCount != 1) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(2, 0, 3433,
				"%s: fnode[%p], refCount(%hd), fdCount(%hd)\n",
				"_exfatfs_open_common", fnode, refCount, fdCount);
#endif
			_flush_fd_cache(sharedResource, fnode);
		}
		file = &sharedResource->files[result];
		file->fnode = fnode;
		result = _AllocateFdBuffer(file);
		if (result < 0) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(1, 0, 3441,
				"%s: _AllocateFdBuffer failed[%#x]\n", "_exfatfs_open_common", result);
#endif
			_ReleaseFp(sharedResource, file);
			file = NULL;
		}
	}
	if (error)
		*error = result;
	return file;
}

static inline int _directory_fd_check(unsigned int fd, const char *caller)
{
	unsigned int mode;
#if FW_VERSION < 0x04000000
	SceExfatfsFnode *fnode;
	SceUInt16 *path;
#endif

	if (!fd || fd >= getInitializedFileCount(&g_sharedResource)) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 1864, "%s: bad fd(%d)\n", caller, fd);
#endif
		return -48;
	}
	mode = g_sharedResource.files[fd].mode;
	if (mode != 3) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 1868, "%s: mode(%d) != expected(%d)\n",
			caller, mode, 3);
#endif
		return -2;
	}
#if FW_VERSION >= 0x04000000
	return _directory_vnode_check(g_sharedResource.files[fd].fnode, caller);
#else
	fnode = g_sharedResource.files[fd].fnode;
	if (fnode->Stat.st_type != 6) {
		path = fnode->FilePath;
		while (*path)
			++path;
		return -6;
	}
	if (fnode->DirCluster == (SceUInt32)-2 &&
	    _root_cluster(fnode->fileSystem) != fnode->Stat.StartCluster)
		return -4;
	return 0;
#endif
}

extern int UVFAT_InitFd(SceExfatfsSharedResource *sharedResource,
	unsigned int fd, struct SceExfatfsFileSystem *fileSystem);

int sceExfatfsVopDopen(SceExfatfsFnode *fnode, unsigned int *fd)
{
	SceExfatfsFile *files;
	SceExfatfsFile *file;
	int descriptor;
	int result;
#if FW_VERSION >= 0x04000000
	short fdCount;
#else
	unsigned short fdCount;
#endif
	short refCount;
#if FW_VERSION >= 0x04000000
	SceUInt32 root;
#else
	SceUInt16 *path;
#endif

#if FW_VERSION >= 0x04000000
	descriptor = _directory_vnode_check(fnode, "sceExfatfsVopDopen");
	if (descriptor)
		goto assigned;
	if (_deleted_entry(fnode)) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 3493,
			"%s: deleted fnode[%p], refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopDopen", fnode, fnode->refCount, fnode->fdCount);
		return 0x80010002;
	}
#else
	if (fnode->Stat.st_type != 6) {
		path = fnode->FilePath;
		while (*path)
			++path;
		descriptor = -6;
		goto assigned;
	}
	if (fnode->DirCluster == (SceUInt32)-2 &&
	    _root_cluster(fnode->fileSystem) != fnode->Stat.StartCluster) {
		descriptor = -4;
		goto assigned;
	}
	if (fnode->FilePath[0]) {
		if (!_has_parent_directory(fnode) && fnode->fileSystem && fnode->Stat.st_type == 6)
			_root_cluster(fnode->fileSystem);
	} else if (fnode->fileSystem) {
		return 0x80010002;
	}
#endif
	if (!fnode->FileName)
		return 0x80010002;
	if (fnode->DirCluster == fnode->Stat.StartCluster) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 3501,
			"%s: fnode[%p], DirCluster[%#lx] == StartCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx]\n",
			"sceExfatfsVopDopen", fnode, fnode->DirCluster, fnode->DirCluster,
			fnode->DirOffset, fnode->DirOffClust);
#endif
		return 0x8001002f;
	}
	descriptor = _ReserveUnusedFd(&g_sharedResource, 3);
assigned:
	if (descriptor < 0) {
		result = exfatfsErrorToIoError(descriptor);
		if (result >= 0)
			return result;
		goto error;
	}
	if ((unsigned int)descriptor >= getInitializedFileCount(&g_sharedResource)) {
		result = 0x80010018;
		goto error;
	}
	fdCount = fnode->fdCount + 1;
	files = g_sharedResource.files;
	refCount = fnode->refCount + 1;
	fnode->fdCount = fdCount;
	fnode->refCount = refCount;
	file = &files[descriptor];
	if (fdCount != 1) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 0, 3513,
			"%s: fnode[%p], refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopDopen", fnode, refCount, fdCount);
#endif
		_flush_fd_cache(&g_sharedResource, fnode);
	}
	files[descriptor].fnode = fnode;
	_directory_fd_check(descriptor, "sceExfatfsVopDopen(after assign)");
	result = _AllocateFdBuffer(file);
	_directory_fd_check(descriptor, "sceExfatfsVopDopen(after allocate buffer)");
	if (result < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 3521,
			"%s: _AllocateFdBuffer failed[%#x]\n", "sceExfatfsVopDopen", result);
#endif
		goto release;
	}
	if (fnode->DirCluster == (SceUInt32)-2) {
#if FW_VERSION >= 0x04000000
		root = _root_cluster(fnode->fileSystem);
		if (root == fnode->Stat.StartCluster)
			SCE_DBG_PRINTF_LEVEL(2, 0, 3525,
				"%s: StartCluster[%#lx] DirCluster[%#lx], fnode[%p]\n",
				"sceExfatfsVopDopen", root, fnode->DirCluster, fnode);
		else
			SCE_DBG_PRINTF_LEVEL(1, 0, 3527,
				"%s: unexpected DirCluster[%#lx], fnode[%p]\n",
				"sceExfatfsVopDopen", fnode->DirCluster, fnode);
#else
		_root_cluster(fnode->fileSystem);
#endif
	}
	result = UVFAT_InitFd(&g_sharedResource, descriptor, fnode->fileSystem);
	if (result < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 3561,
			"%s: UVFAT_InitFd failed[%#x]\n", "sceExfatfsVopDopen", result);
#endif
		result = exfatfsErrorToIoError(result);
		if (result < 0)
			goto release;
	} else {
		_directory_fd_check(descriptor, "sceExfatfsVopDopen(after UVFAT_InitFd)");
	}
	*fd = descriptor;
	return result;
release:
	_ReleaseFp(&g_sharedResource, file);
error:
	*fd = 0;
	return result;
}

int sceExfatfsVopOpen(SceExfatfsFnode *fnode, int flags, int *fd)
{
	SceExfatfsFile *files;
	SceExfatfsFile *file;
	struct SceExfatfsFileSystem *fileSystem;
	int descriptor;
	int mode;
	int result;
#if FW_VERSION >= 0x04000000
	short fdCount;
#else
	unsigned short fdCount;
#endif
	short refCount;
#if FW_VERSION < 0x04000000
	SceUInt16 *path;
#endif

#if FW_VERSION >= 0x04000000
	descriptor = _file_vnode_check(fnode, "sceExfatfsVopOpen");
	*fd = 0;
	if (_deleted_entry(fnode)) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 3655,
			"%s: deleted fnode[%p], refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopOpen", fnode, fnode->refCount, fnode->fdCount);
		return 0x80010002;
	}
#else
	if (fnode->Stat.st_type) {
		path = fnode->FilePath;
		while (*path)
			++path;
		descriptor = -6;
	} else {
		descriptor = fnode->DirCluster == (SceUInt32)-2 ? -4 : 0;
	}
	*fd = 0;
	if (fnode->FilePath[0]) {
		if (!_has_parent_directory(fnode) && fnode->fileSystem && fnode->Stat.st_type == 6)
			_root_cluster(fnode->fileSystem);
	} else if (fnode->fileSystem) {
		return 0x80010002;
	}
	if (descriptor)
		return exfatfsErrorToIoError(descriptor);
#endif
	if (!descriptor) {
		mode = 6;
		if (((unsigned int)flags & 3) - 1 > 2) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 880, "_flag_convert",
				"%s: unexpected open flag[%#x]\n", "_flag_convert", flags);
#endif
			mode = 1;
		}
		descriptor = _ReserveUnusedFd(&g_sharedResource, mode);
	}
	if (descriptor < 0)
		return exfatfsErrorToIoError(descriptor);
	if ((unsigned int)descriptor >= getInitializedFileCount(&g_sharedResource))
		return 0x80010018;
	fdCount = fnode->fdCount + 1;
	files = g_sharedResource.files;
	refCount = fnode->refCount + 1;
	fnode->fdCount = fdCount;
	fnode->refCount = refCount;
	file = &files[descriptor];
	if (fdCount != 1) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 0, 3674,
			"%s: fnode[%p], refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopOpen", fnode, refCount, fdCount);
#endif
		_flush_fd_cache(&g_sharedResource, fnode);
	}
	files[descriptor].fnode = fnode;
	result = _AllocateFdBuffer(file);
	if (result < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 3686,
			"%s: _AllocateFdBuffer failed[%#x]\n", "sceExfatfsVopOpen", result);
#endif
		_ReleaseFp(&g_sharedResource, file);
		return result;
	}
	fileSystem = fnode->fileSystem;
	if (!fileSystem) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3604, "_file_open_setup",
			"%s: unexpected fileSystem == NULL\n", "_file_open_setup");
#endif
		for (;;)
			;
	}
#if FW_VERSION >= 0x04000000
	if (fnode->DirCluster == (SceUInt32)-2)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3608, "_file_open_setup",
			"%s: unexpected DirCluster[%#lx], fnode[%p]\n",
			"_file_open_setup", fnode->DirCluster, fnode);
#endif
	if (fileSystem->fileSystemType == 2)
		UVFAT_InitFd(&g_sharedResource, descriptor, fileSystem);
	else
		UVFAT_InitFd(&g_sharedResource, descriptor, fileSystem);
	fnode->fileSystem = fileSystem;
	*fd = descriptor;
	return 0;
}

extern int initializeFnode(SceExfatfsFnode *fnode);
extern int UVFAT_SearchGetFile_ex(int mode, SceExfatfsSharedResource *sharedResource,
	SceUInt16 *filename, struct SceExfatfsFileSystem *fileSystem,
	SceExfatfsFnode *fnode, int flags);
extern int _ReadDir_fat(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFile *file, SceUInt16 *filename, SceExfatfsStat *stat,
	unsigned char *shortName, int flags);
extern int sceFatfsACCheckRDirEntry(SceExfatfsDrive *drive, unsigned char attribute);
extern int codepageToUtf16(SceUInt16 *destination, int capacity,
	const unsigned char *source);
extern int utf16ToUtf8(unsigned char *destination, unsigned int capacity,
	const SceUInt16 *source);

int sceExfatfsVopDread(unsigned int fd, SceIoDirent *dirent)
{
	SceExfatfsFile *file;
	SceExfatfsFnode *fnode, *temporary;
	struct SceExfatfsFileSystem *fileSystem;
	SceExfatfsClusterCacheElement *element, *end, *allocation;
	SceUInt16 *filename;
	unsigned char *wname;
	SceExfatfsStat stat;
	unsigned char shortName[13] = { 0 };
	int readResult;
	int result;

	result = _directory_fd_check(fd, "sceExfatfsVopDread");
	if (result)
		return exfatfsErrorToIoError(result);
	file = &g_sharedResource.files[fd];
	fnode = file->fnode;
	fileSystem = fnode->fileSystem;
#if FW_VERSION >= 0x04000000
	if (_deleted_entry(fnode))
		SCE_DBG_PRINTF_LEVEL(1, 0, 3914,
			"%s: deleted fnode[%p], refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopDread", fnode, fnode->refCount, fnode->fdCount);
#else
	if (fnode->FilePath[0] && !_has_parent_directory(fnode) &&
	    fnode->fileSystem && fnode->Stat.st_type == 6)
		_root_cluster(fnode->fileSystem);
#endif
	if (fileSystem->file.mode)
		return 0x80010078;
	filename = allocateHeapBuffer(fileSystem->drive);
	memset(&stat, 0, sizeof(stat));
	if (!filename) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 3932,
			"%s: sceFatfsCreateHeap for filename failed\n", "sceExfatfsVopDread");
#endif
		return 0x8001000c;
	}
	memset(filename, 0, 2048);
	if (!fileSystem->file.mode && file->mode == 3) {
		fileSystem->file.mode = 3;
#if FW_VERSION >= 0x04000000
		if (fileSystem->file.fnode->fileSystem != file->fnode->fileSystem)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3870, "_setup_for_directory_search",
				"%s: unexpected FileSystem[%p] != [%p]\n",
				"_setup_for_directory_search", fileSystem->file.fnode->fileSystem,
				file->fnode->fileSystem);
#endif
		fileSystem->file.fnode->dirGeneralSecondaryFlags = file->fnode->Stat.generalSecondaryFlags;
		fileSystem->file.fnode->DirCluster = file->fnode->Stat.StartCluster;
		fileSystem->file.fnode->dirSize = file->fnode->Stat.Size;
		_copy_position_and_fd_cache(&fileSystem->file, file, 0);
	}
	if (fd >= getInitializedFileCount(&g_sharedResource)) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 0, 3945, "bad fd[%d]\n", fd);
#endif
		readResult = -48;
		fileSystem->file.mode = 0;
		goto read_error;
	}
	if (fnode->fileSystem->fileSystemType == 2) {
		temporary = allocateCommonHeapMemory((int)fileSystem->drive, sizeof(*temporary));
		if (!temporary) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(1, 0, 3964,
				"%s: sceFatfsCreateHeapVl failed\n", "sceExfatfsVopDread");
#endif
			fileSystem->file.mode = 0;
			result = 0x8001000c;
			goto free_filename;
		}
		temporary->cache_element_count = 0;
		temporary->cluster_cache_array = NULL;
#if FW_VERSION >= 0x04000000
		SCE_DBG_ASSERT_FUNC(1, 554, "_invalidate_cluster_cache_data");
		temporary->tailCluster = -2;
		temporary->tailIndex = -2;
		temporary->clusterCount = 0;
		temporary->chunkCount = 0;
		element = temporary->cluster_cache_array;
		if (element) {
			temporary->cache_element_count = 42;
			end = element + 42;
			do {
				SCE_DBG_ASSERT_FUNC(element != NULL, 485, "_invalidate_cluster_cache_element");
				if (element) {
					element->cluster = -2;
					element->index = 0;
					element->count = 0;
				}
				++element;
			} while (element != end);
		}
#else
		_invalidate_cluster_cache_data(temporary);
#endif
		initializeFnode(temporary);
		temporary->fileSystem = fileSystem;
		readResult = UVFAT_SearchGetFile_ex(1, &g_sharedResource, filename,
			fileSystem, temporary, 0);
		stat = temporary->Stat;
		SCE_DBG_ASSERT_FUNC(1, 554, "_invalidate_cluster_cache_data");
		temporary->tailCluster = -2;
		temporary->tailIndex = -2;
		temporary->clusterCount = 0;
		temporary->chunkCount = 0;
		element = temporary->cluster_cache_array;
		if (element) {
			temporary->cache_element_count = 42;
			end = element + 42;
			do {
				SCE_DBG_ASSERT_FUNC(element != NULL, 485, "_invalidate_cluster_cache_element");
				if (element) {
					element->cluster = -2;
					element->index = 0;
					element->count = 0;
				}
				++element;
			} while (element != end);
			allocation = temporary->cluster_cache_array;
			temporary->cache_element_count = 0;
			temporary->cluster_cache_array = NULL;
			if (allocation)
				freeCommonHeapMemory((int)temporary->fileSystem->drive, allocation);
		} else {
			temporary->cache_element_count = 0;
		}
		freeCommonHeapMemory((int)fileSystem->drive, temporary);
	} else {
		readResult = _ReadDir_fat(&g_sharedResource, &fileSystem->file,
			filename, &stat, shortName, 0);
	}
	_copy_position_and_fd_cache(file, &fileSystem->file, 1);
	fileSystem->file.mode = 0;
	if (readResult == -7) {
		result = 0x80010002;
		goto free_filename;
	}
	if (readResult) {
read_error:
		result = exfatfsErrorToIoError(readResult);
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 0, 3981,
			"%s: UVFAT_ReadDir for fd(%d) failed[%#x->%#x]\n",
			"sceExfatfsVopDread", fd, readResult, result);
#endif
		goto free_filename;
	}
	exfatfsStatToIoStat(fileSystem->drive, &stat, &dirent->d_stat);
	result = sceFatfsACCheckRDirEntry(fileSystem->drive, dirent->d_stat.st_attr & 255);
	if (result < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 0, 3992,
			"%s: ACCheckRDirEntry failed[%#x]\n", "sceExfatfsVopDread", result);
#endif
		if (result == (int)0x80010016)
			result = 0x8001000d;
		memset(&dirent->d_stat, 0, sizeof(dirent->d_stat));
		goto free_filename;
	}
	if (result)
		goto free_filename;
	wname = allocateHeapBuffer(fileSystem->drive);
	if (!wname) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 4008,
			"%s: sceFatfsCreateHeap for wname failed\n", "sceExfatfsVopDread");
#endif
		result = 0x8001000c;
		goto free_filename;
	}
	if (!filename[0]) {
		result = codepageToUtf16(filename, 1024, shortName);
		if (result < 0)
			goto free_wname;
	}
	memset(wname, 0, 2048);
	utf16ToUtf8(wname, 2048, filename);
	if (strnlen((char *)wname, 256) <= 255) {
		strncpy(dirent->d_name, (char *)wname, 256);
	} else if (shortName[0]) {
		memset(wname, 0, 2048);
		result = codepageToUtf16((SceUInt16 *)wname, 1024, shortName);
		if (result >= 0)
			utf16ToUtf8((unsigned char *)dirent->d_name, 256, (SceUInt16 *)wname);
	} else {
		result = 0x80010034;
	}
free_wname:
	freeHeapBuffer((int)fileSystem->drive, wname);
free_filename:
	freeHeapBuffer((int)fileSystem->drive, filename);
	return result;
}

extern int sceFatfsACSetFSAttributeByPermission(SceExfatfsDrive *drive,
	SceUInt32 permission, unsigned char *attribute);
extern int applyFatAttributeToStat(SceExfatfsDrive *drive,
	unsigned char attribute, SceExfatfsStat *stat);
extern int _Update_FAT_Size(SceExfatfsFnode *fnode, SceExfatfsStat *stat,
	unsigned int updateTime, SceExfatfsSharedResource *sharedResource);

int _chstat_setup(SceExfatfsFnode *fnode, SceIoStat *stat, unsigned int bits)
{
	struct SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	SceExfatfsDrive *drive;
	SceFatfsNameset *nameset;
	SceFatfsNameset *preparedNameset;
	SceExfatfsStat updatedStat;
	unsigned char *path;
	unsigned char attribute;
	unsigned int oldAttribute;
	unsigned int newAttribute;
	int result;
	int length;

#if FW_VERSION >= 0x04000000
	if (!fnode->Stat.st_type) {
		result = _file_vnode_check(fnode, "sceExfatfsVopChstat");
	} else if (fnode->Stat.st_type == 6) {
		result = _directory_vnode_check(fnode, "sceExfatfsVopChstat");
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1852, "_vnode_check",
			"%s: unsupported st_type(%d)\n", "sceExfatfsVopChstat", fnode->Stat.st_type);
		result = -6;
	}
	if (_deleted_entry(fnode)) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 4114, "sceExfatfsVopChstat",
			"%s: deleted fnode[%p], refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopChstat", fnode, fnode->refCount, fnode->fdCount);
		return 0x80010002;
	}
#else
	if (!fnode->Stat.st_type) {
		result = fnode->DirCluster == (SceUInt32)-2 ? -4 : 0;
	} else if (fnode->Stat.st_type == 6) {
		result = 0;
		if (fnode->DirCluster == (SceUInt32)-2 &&
		    _root_cluster(fileSystem) != fnode->Stat.StartCluster)
			result = -4;
	} else {
		result = -6;
	}
	if (fnode->FilePath[0]) {
		if (!_has_parent_directory(fnode) && fnode->fileSystem && fnode->Stat.st_type == 6)
			_root_cluster(fnode->fileSystem);
	} else if (fnode->fileSystem) {
		return 0x80010002;
	}
#endif
	if (result)
		return exfatfsErrorToIoError(result);
	if (!fnode->FileName)
		return 0x80010002;
	if ((bits & 4) && fnode->Stat.st_type == 6)
		return 0x80010015;
	drive = fileSystem->drive;
	nameset = NULL;
#if FW_VERSION >= 0x04000000
	path = _temporal_utf8_fullpath((int)drive, fnode, NULL, 0);
	if (path) {
		result = sceFatfsPrepareNameset((int)drive, path, &nameset);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2199, "_temporal_nameset",
				"%s: sceFatfsPrepareNameset failed[%#x]\n", "_temporal_nameset", result);
		freeHeapBuffer((int)drive, path);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2207, "_temporal_nameset",
			"%s: sceFatfsCreateHeap failed\n", "_temporal_nameset");
	}
#else
	path = allocateHeapBuffer(drive);
	if (path) {
		memset(path, 0, 2048);
		length = _utf16_to_utf8(path, 2048, fnode->FilePath);
		if (length >= 0 && length != 2048) {
			path[length] = 0;
			sceFatfsPrepareNameset((int)drive, path, &nameset);
		}
		freeHeapBuffer((int)drive, path);
	}
#endif
	preparedNameset = nameset;
	if (!preparedNameset)
		return 0x8001000c;
	updatedStat = fnode->Stat;
	oldAttribute = updatedStat.st_attr;
	result = sceFatfsACCheckRDirEntry(drive, oldAttribute);
	if (result < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 4041,
			"%s: sceFatfsACCheckRDirEntry failed[%#x]\n", "_chstat_setup", result);
#endif
		goto free_nameset;
	}
	if (bits & 1) {
		attribute = 0;
		result = sceFatfsACSetFSAttributeByPermission(drive, stat->st_mode, &attribute);
		if (result < 0)
			goto free_nameset;
		newAttribute = (oldAttribute & ~7U) | attribute;
		attribute = newAttribute;
		if ((bits & 0x10000) && stat->st_attr != newAttribute) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(1, 0, 4054,
				"%s: st_attr[%#x] != attr[%#x] from mode[%o]\n",
				"_chstat_setup", stat->st_attr, newAttribute, stat->st_mode);
#endif
			result = 0x80010016;
			goto free_nameset;
		}
		applyFatAttributeToStat(drive, attribute, &updatedStat);
	}
	if (bits & 0x10000) {
		if (stat->st_attr & 0xc8) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(1, 0, 4067,
				"%s: invalid attribute[%#x]\n", "_chstat_setup", stat->st_attr);
#endif
			result = 0x80010016;
			goto free_nameset;
		}
		if ((updatedStat.st_attr ^ stat->st_attr) & 0x10) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(2, 15, 4072, "file != dir\n");
#endif
			result = 0x80010016;
			goto free_nameset;
		}
		if (sceFatfsACGetDirPermission(drive, stat->st_attr) < 0) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(1, 0, 4076,
				"%s: invalid attribute[%#x]\n", "_chstat_setup", stat->st_attr);
#endif
			result = 0x80010016;
			goto free_nameset;
		}
		applyFatAttributeToStat(drive, stat->st_attr, &updatedStat);
	}
	if ((bits & 8) && &stat->st_ctime) {
		updatedStat.ctime.year = stat->st_ctime.year;
		updatedStat.ctime.month = stat->st_ctime.month;
		updatedStat.ctime.day = stat->st_ctime.day;
		updatedStat.ctime.hour = stat->st_ctime.hour;
		updatedStat.ctime.minute = stat->st_ctime.minute;
		updatedStat.ctime.second = stat->st_ctime.second;
	}
	if ((bits & 0x20) && &stat->st_mtime) {
		updatedStat.mtime.year = stat->st_mtime.year;
		updatedStat.mtime.month = stat->st_mtime.month;
		updatedStat.mtime.day = stat->st_mtime.day;
		updatedStat.mtime.hour = stat->st_mtime.hour;
		updatedStat.mtime.minute = stat->st_mtime.minute;
		updatedStat.mtime.second = stat->st_mtime.second;
	}
	if ((bits & 0x10) && &stat->st_atime) {
		updatedStat.atime.year = stat->st_atime.year;
		updatedStat.atime.month = stat->st_atime.month;
		updatedStat.atime.day = stat->st_atime.day;
		updatedStat.atime.hour = stat->st_atime.hour;
		updatedStat.atime.minute = stat->st_atime.minute;
		updatedStat.atime.second = stat->st_atime.second;
	}
	if (bits & 4) {
		updatedStat.Size = stat->st_size;
		result = _check_free_count(fnode, stat->st_size);
		if (result)
			goto free_nameset;
	}
	result = _Update_FAT_Size(fnode, &updatedStat, bits & 0x20, &g_sharedResource);
	if (result)
		result = exfatfsErrorToIoError(result);
free_nameset:
	freeHeapBuffer((int)drive, preparedNameset);
	return result;
}

int checkDriveWritable(SceExfatfsDrive *drive)
{
	int mountIndex;

	if (g_sharedResource.fileSystems[0].devname && drive == g_sharedResource.fileSystems[0].drive)
		mountIndex = 0;
	else if (g_sharedResource.fileSystems[1].devname && drive == g_sharedResource.fileSystems[1].drive)
		mountIndex = 1;
	else if (g_sharedResource.fileSystems[2].devname && drive == g_sharedResource.fileSystems[2].drive)
		mountIndex = 2;
	else if (g_sharedResource.fileSystems[3].devname && drive == g_sharedResource.fileSystems[3].drive)
		mountIndex = 3;
	else if (g_sharedResource.fileSystems[4].devname && drive == g_sharedResource.fileSystems[4].drive)
		mountIndex = 4;
	else if (g_sharedResource.fileSystems[5].devname && drive == g_sharedResource.fileSystems[5].drive)
		mountIndex = 5;
	else if (g_sharedResource.fileSystems[6].devname && drive == g_sharedResource.fileSystems[6].drive)
		mountIndex = 6;
	else if (g_sharedResource.fileSystems[7].devname && drive == g_sharedResource.fileSystems[7].drive)
		mountIndex = 7;
	else if (g_sharedResource.fileSystems[8].devname && drive == g_sharedResource.fileSystems[8].drive)
		mountIndex = 8;
	else if (g_sharedResource.fileSystems[9].devname && drive == g_sharedResource.fileSystems[9].drive)
		mountIndex = 9;
	else if (g_sharedResource.fileSystems[10].devname && drive == g_sharedResource.fileSystems[10].drive)
		mountIndex = 10;
	else if (g_sharedResource.fileSystems[11].devname && drive == g_sharedResource.fileSystems[11].drive)
		mountIndex = 11;
	else if (g_sharedResource.fileSystems[12].devname && drive == g_sharedResource.fileSystems[12].drive)
		mountIndex = 12;
	else if (g_sharedResource.fileSystems[13].devname && drive == g_sharedResource.fileSystems[13].drive)
		mountIndex = 13;
	else if (g_sharedResource.fileSystems[14].devname && drive == g_sharedResource.fileSystems[14].drive)
		mountIndex = 14;
	else
		return 0x80010002;
	if ((g_sharedResource.fileSystems[mountIndex].mount_mode & 255) == 1)
		return 0x8001001e;
	return 0;
}

void _exfatfs_set_whiteout_directory(SceExfatfsFnode *fnode)
{
	struct SceExfatfsFileSystem *fileSystem = fnode->fileSystem;

#if FW_VERSION >= 0x04000000
	if (fileSystem->whiteout_directory)
		SCE_DBG_PRINTF_LEVEL(1, 0, 4337,
			"%s: unexpected whiteout_directory[%p] has been set\n",
			"_exfatfs_set_whiteout_directory", fileSystem->whiteout_directory);
#endif
	fileSystem->whiteout_directory = fnode;
}

void _clear_whiteout_directory(SceExfatfsFnode *fnode)
{
	struct SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	SceExfatfsFnode *whiteout;
	int result;

	if (!fileSystem)
		return;
	whiteout = fileSystem->whiteout_directory;
	if (!whiteout)
		return;
	fileSystem->whiteout_directory = NULL;
	result = UVFAT_ReleaseFnode(&g_sharedResource, whiteout);
	if (result < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 4361,
			"%s: UVFAT_ReleaseFnode failed[%#x]\n", "_clear_whiteout_directory", result);
#endif
		fileSystem->whiteout_directory = whiteout;
	}
}

SceExfatfsFnode *getWhiteoutDirectory(SceExfatfsFnode *fnode)
{
	struct SceExfatfsFileSystem *fileSystem = fnode->fileSystem;

	return fileSystem ? fileSystem->whiteout_directory : NULL;
}

SceExfatfsFnode *getDriveWhiteoutDirectory(SceExfatfsDrive *drive)
{
	unsigned int mountIndex;

	if (g_sharedResource.fileSystems[0].devname && drive == g_sharedResource.fileSystems[0].drive)
		mountIndex = 0;
	else if (g_sharedResource.fileSystems[1].devname && drive == g_sharedResource.fileSystems[1].drive)
		mountIndex = 1;
	else if (g_sharedResource.fileSystems[2].devname && drive == g_sharedResource.fileSystems[2].drive)
		mountIndex = 2;
	else if (g_sharedResource.fileSystems[3].devname && drive == g_sharedResource.fileSystems[3].drive)
		mountIndex = 3;
	else if (g_sharedResource.fileSystems[4].devname && drive == g_sharedResource.fileSystems[4].drive)
		mountIndex = 4;
	else if (g_sharedResource.fileSystems[5].devname && drive == g_sharedResource.fileSystems[5].drive)
		mountIndex = 5;
	else if (g_sharedResource.fileSystems[6].devname && drive == g_sharedResource.fileSystems[6].drive)
		mountIndex = 6;
	else if (g_sharedResource.fileSystems[7].devname && drive == g_sharedResource.fileSystems[7].drive)
		mountIndex = 7;
	else if (g_sharedResource.fileSystems[8].devname && drive == g_sharedResource.fileSystems[8].drive)
		mountIndex = 8;
	else if (g_sharedResource.fileSystems[9].devname && drive == g_sharedResource.fileSystems[9].drive)
		mountIndex = 9;
	else if (g_sharedResource.fileSystems[10].devname && drive == g_sharedResource.fileSystems[10].drive)
		mountIndex = 10;
	else if (g_sharedResource.fileSystems[11].devname && drive == g_sharedResource.fileSystems[11].drive)
		mountIndex = 11;
	else if (g_sharedResource.fileSystems[12].devname && drive == g_sharedResource.fileSystems[12].drive)
		mountIndex = 12;
	else if (g_sharedResource.fileSystems[13].devname && drive == g_sharedResource.fileSystems[13].drive)
		mountIndex = 13;
	else {
		if (!g_sharedResource.fileSystems[14].devname)
			return NULL;
		if (drive != g_sharedResource.fileSystems[14].drive)
			return NULL;
		mountIndex = 14;
	}
	return g_sharedResource.fileSystems[mountIndex].whiteout_directory;
}

void _exfatfs_set_whiteout_bit(SceExfatfsFnode *fnode)
{
#if FW_VERSION >= 0x04000000
	if (fnode->flags & 1)
		SCE_DBG_PRINTF_LEVEL(1, 0, 4422,
			"%s: unexpected flags[%#x], fnode[%p]\n",
			"_exfatfs_set_whiteout_bit", fnode->flags, fnode);
#endif
	fnode->flags |= 1;
}

extern int UVFAT_FreeClusterLink(SceExfatfsSharedResource *sharedResource,
	struct SceExfatfsFileSystem *fileSystem, SceUInt32 cluster);
extern int UVFAT_FreeClusterLink_ex_NFC(SceExfatfsSharedResource *sharedResource,
	struct SceExfatfsFileSystem *fileSystem, SceExfatfsStat *stat, SceUInt32 cluster);

int _exfatfs_free_cluster_link(SceExfatfsFnode *fnode, SceUInt32 cluster)
{
	struct SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	SceExfatfsClusterCacheElement *element, *end;
	SceUInt32 fileSystemType;
	SceUInt16 statType;

#if FW_VERSION >= 0x04000000
	if (fnode->Stat.StartCluster != cluster)
		SCE_DBG_PRINTF_LEVEL(1, 0, 4433,
			"%s: unexpected startCluster[%#lx] != StartCluster[%#lx], fnode[%p]\n",
			"_exfatfs_free_cluster_link", cluster, fnode->Stat.StartCluster, fnode);
#endif
	if (fileSystem) {
		if (cluster > 1 && cluster < fileSystem->clusterCount + 2) {
			if (fileSystem->fileSystemType == 2 && (fnode->Stat.generalSecondaryFlags & 2))
				UVFAT_FreeClusterLink_ex_NFC(&g_sharedResource, fileSystem, &fnode->Stat, cluster);
			else
				UVFAT_FreeClusterLink(&g_sharedResource, fileSystem, cluster);
			statType = fnode->Stat.st_type;
			fileSystemType = fileSystem->fileSystemType;
			fnode->Stat.Size = 0;
			fnode->Stat.StartCluster = statType == 6 ? -1 : 0;
			if (fileSystemType == 2)
				fnode->Stat.generalSecondaryFlags &= ~2;
		}
	} else {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 4436,
			"%s: NULL fileSystem, unmounted\n", "_exfatfs_free_cluster_link");
#endif
	}
	SCE_DBG_ASSERT_FUNC(1, 554, "_invalidate_cluster_cache_data");
	element = fnode->cluster_cache_array;
	fnode->tailCluster = fnode->tailIndex = -2;
	fnode->clusterCount = fnode->chunkCount = 0;
	if (element) {
		fnode->cache_element_count = 42;
		end = element + 42;
		do {
			SCE_DBG_ASSERT_FUNC(element != NULL, 485, "_invalidate_cluster_cache_element");
			if (element) {
				element->cluster = -2;
				element->index = 0;
				element->count = 0;
			}
			++element;
		} while (element != end);
	}
	return 0;
}

extern int UVFAT_UnLinkFromDir(SceExfatfsSharedResource *sharedResource,
	SceUInt16 *fileName, struct SceExfatfsFileSystem *fileSystem,
	SceExfatfsFnode *fnode);

int sceExfatfsVopRemove(SceExfatfsFnode *parent_fnode, SceExfatfsFnode *fnode)
{
	struct SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	SceExfatfsDrive *drive;
	SceFatfsNameset *nameset, *preparedNameset;
	SceUInt32 startCluster;
	unsigned char *path;
	int result;
#if FW_VERSION < 0x04000000
	SceUInt16 *parentPath;
	int length;
#endif

#if FW_VERSION >= 0x04000000
	if (_deleted_entry(fnode)) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 4525,
			"%s: deleted fnode[%p], refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopRemove", fnode, fnode->refCount, fnode->fdCount);
		return 0x80010002;
	}
#else
	if (fnode->FilePath[0]) {
		if (!_has_parent_directory(fnode) && fnode->fileSystem && fnode->Stat.st_type == 6)
			_root_cluster(fnode->fileSystem);
	} else if (fileSystem) {
		return 0x80010002;
	}
#endif
	if (fnode->flags & 1) {
		parent_fnode = fileSystem->whiteout_directory;
	} else if (!parent_fnode) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 4535,
			"%s: NULL parent but fnode[%p] isn't whiteout entry\n",
			"sceExfatfsVopRemove", fnode);
#endif
		return 0x80010016;
	}
#if FW_VERSION >= 0x04000000
	if (_directory_vnode_check(parent_fnode, "sceExfatfsVopRemove") < 0)
		return 0x80010014;
#else
	if (parent_fnode->Stat.st_type != 6) {
		parentPath = parent_fnode->FilePath;
		while (*parentPath)
			++parentPath;
		return 0x80010014;
	}
	if (parent_fnode->DirCluster == (SceUInt32)-2 &&
	    _root_cluster(parent_fnode->fileSystem) != parent_fnode->Stat.StartCluster)
		return 0x80010014;
#endif
	if (parent_fnode->fileSystem != fileSystem) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 4547,
			"%s: parent_fnode->FileSystem[%p] != fnode->FileSystem[%p]\n",
			"sceExfatfsVopRemove", parent_fnode->fileSystem, fileSystem);
#endif
		return 0x80010012;
	}
	if (!parent_fnode->FileName || !fnode->FileName)
		return 0x80010002;
	if (fnode->Stat.st_type == 6)
		return 0x80010015;
	if (fileSystem->file.mode)
		return 0x80010078;
	drive = fileSystem->drive;
	nameset = NULL;
#if FW_VERSION >= 0x04000000
	path = _temporal_utf8_fullpath((int)drive, fnode, NULL, 0);
	if (path) {
		result = sceFatfsPrepareNameset((int)drive, path, &nameset);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2199, "_temporal_nameset",
				"%s: sceFatfsPrepareNameset failed[%#x]\n", "_temporal_nameset", result);
		freeHeapBuffer((int)drive, path);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2207, "_temporal_nameset",
			"%s: sceFatfsCreateHeap failed\n", "_temporal_nameset");
	}
#else
	path = allocateHeapBuffer(drive);
	if (path) {
		memset(path, 0, 2048);
		length = _utf16_to_utf8(path, 2048, fnode->FilePath);
		if (length >= 0 && length != 2048) {
			path[length] = 0;
			sceFatfsPrepareNameset((int)drive, path, &nameset);
		}
		freeHeapBuffer((int)drive, path);
	}
#endif
	preparedNameset = nameset;
	if (!preparedNameset)
		return 0x8001000c;
	startCluster = fnode->Stat.StartCluster;
	result = UVFAT_UnLinkFromDir(&g_sharedResource, preparedNameset->fileName, fnode->fileSystem, fnode);
	if (result < 0) {
		result = exfatfsErrorToIoError(result);
	} else if (!result) {
		if (fnode->refCount) {
			fnode->DirCluster = -2;
			fnode->DirOffset = 0;
			fnode->DirOffClust = -2;
			memset(fnode->FilePath, 0, sizeof(fnode->FilePath));
			fnode->FileName = fnode->FilePath;
		} else {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 190, "_invalidate_parent_entry",
				"%s: unexpected refCount(%hd), fdCount(%hd), fnode[%p], DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx], StartCluster[%#lx]\n",
				"_invalidate_parent_entry", 0, fnode->fdCount, fnode, fnode->DirCluster,
				fnode->DirOffset, fnode->DirOffClust, fnode->Stat.StartCluster);
#endif
		}
		if (!fnode->fdCount)
			_exfatfs_free_cluster_link(fnode, startCluster);
	}
	freeHeapBuffer((int)drive, preparedNameset);
	return result;
}

extern int UVFAT_InitDirEntry(SceExfatfsSharedResource *sharedResource,
	int fd, SceUInt32 cluster, struct SceExfatfsFileSystem *fileSystem);
extern int UVFAT_InitDirEntry_ex(SceExfatfsSharedResource *sharedResource,
	int fd, SceUInt32 cluster, struct SceExfatfsFileSystem *fileSystem,
	unsigned int flags, SceUInt32 size);
extern int _exfatfs_search_next_short_name(SceExfatfsSharedResource *sharedResource,
	int fd, struct SceExfatfsFileSystem *fileSystem, unsigned char *entry);
extern int _exfatfs_release_temporal_file(struct SceExfatfsFileSystem *fileSystem);

int _check_empty_directory(SceExfatfsFnode *parent_fnode, SceExfatfsFnode *fnode)
{
	struct SceExfatfsFileSystem *fileSystem = fnode->fileSystem;
	struct SceExfatfsFileSystem *parentFileSystem;
	struct SceExfatfsFileSystem *directoryFileSystem;
	SceExfatfsDrive *drive;
	SceFatfsNameset *nameset, *preparedNameset;
	SceUInt32 startCluster;
	unsigned char entry[32];
	unsigned char *path;
	int result;
#if FW_VERSION < 0x04000000
	SceUInt16 *parentPath;
	int length;
#endif

#if FW_VERSION >= 0x04000000
	if (_deleted_entry(fnode)) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 4639, "sceExfatfsVopRmdir",
			"%s: deleted fnode[%p], refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopRmdir", fnode, fnode->refCount, fnode->fdCount);
		return 0x80010002;
	}
#else
	if (fnode->FilePath[0]) {
		if (!_has_parent_directory(fnode) && fnode->fileSystem && fnode->Stat.st_type == 6)
			_root_cluster(fnode->fileSystem);
	} else if (fileSystem) {
		return 0x80010002;
	}
#endif
	if (fnode->flags & 1)
		parent_fnode = fileSystem->whiteout_directory;
#if FW_VERSION >= 0x04000000
	if (_directory_vnode_check(parent_fnode, "sceExfatfsVopRmdir") < 0)
		return 0x80010014;
#else
	if (parent_fnode->Stat.st_type != 6) {
		parentPath = parent_fnode->FilePath;
		while (*parentPath)
			++parentPath;
		return 0x80010014;
	}
	if (parent_fnode->DirCluster == (SceUInt32)-2 &&
	    _root_cluster(parent_fnode->fileSystem) != parent_fnode->Stat.StartCluster)
		return 0x80010014;
#endif
	parentFileSystem = parent_fnode->fileSystem;
	if (parentFileSystem != fileSystem) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 4658, "sceExfatfsVopRmdir",
			"%s: parent_fnode->FileSystem[%p] != fnode->FileSystem[%p]\n",
			"sceExfatfsVopRmdir", parentFileSystem, fileSystem);
#endif
		return 0x80010012;
	}
	if (!parent_fnode->FileName || !fnode->FileName)
		return 0x80010002;
	if (fnode->Stat.st_type != 6)
		return 0x80010014;
	if (fileSystem->file.mode)
		return 0x80010078;
	directoryFileSystem = fnode->fileSystem;
	startCluster = fnode->Stat.StartCluster;
#if FW_VERSION >= 0x04000000
	if (startCluster == (SceUInt32)-2) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 4267,
			"%s: unexpected StartCluster[%#lx], fnode[%p]\n",
			"_check_empty_directory", startCluster, fnode);
		startCluster = fnode->Stat.StartCluster;
	}
#endif
	if (directoryFileSystem->fileSystemType == 2) {
		result = UVFAT_InitDirEntry_ex(&g_sharedResource, 0, startCluster,
			directoryFileSystem, fnode->Stat.generalSecondaryFlags, fnode->Stat.Size);
		if (result < 0) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(1, 0, 4275,
				"%s: UVFAT_InitDirEntry_ex failed(%d)\n", "_check_empty_directory", result);
			SCE_DBG_ASSERT(0, 4276);
#endif
			goto checked;
		}
		result = UVFAT_SearchGetFile_ex(1, &g_sharedResource,
			directoryFileSystem->fnode.FilePath, directoryFileSystem, NULL, 0);
#if FW_VERSION >= 0x04000000
		if (result && result != -7)
			SCE_DBG_PRINTF_LEVEL(1, 0, 4285,
				"%s: UVFAT_SearchGetFile_ex failed[%#x]\n", "_check_empty_directory", result);
#endif
	} else {
		result = UVFAT_InitDirEntry(&g_sharedResource, 0, startCluster, directoryFileSystem);
		if (result < 0) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(1, 0, 4294,
				"%s: UVFAT_InitDirEntry failed(%d)\n", "_check_empty_directory", result);
			SCE_DBG_ASSERT(0, 4295);
#endif
			goto checked;
		}
		result = _exfatfs_search_next_short_name(&g_sharedResource, 0, directoryFileSystem, entry);
	}
	if (result == -7)
		result = -4;
	else if (!result)
		result = -31;
	_exfatfs_release_temporal_file(directoryFileSystem);
checked:
	if (result == -31)
		return 0x8001005a;
	if (result != -4)
		return exfatfsErrorToIoError(result);
	drive = parentFileSystem->drive;
	nameset = NULL;
#if FW_VERSION >= 0x04000000
	path = _temporal_utf8_fullpath((int)drive, fnode, NULL, 0);
	if (path) {
		result = sceFatfsPrepareNameset((int)drive, path, &nameset);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2199, "_temporal_nameset",
				"%s: sceFatfsPrepareNameset failed[%#x]\n", "_temporal_nameset", result);
		freeHeapBuffer((int)drive, path);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2207, "_temporal_nameset",
			"%s: sceFatfsCreateHeap failed\n", "_temporal_nameset");
	}
#else
	path = allocateHeapBuffer(drive);
	if (path) {
		memset(path, 0, 2048);
		length = _utf16_to_utf8(path, 2048, fnode->FilePath);
		if (length >= 0 && length != 2048) {
			path[length] = 0;
			sceFatfsPrepareNameset((int)drive, path, &nameset);
		}
		freeHeapBuffer((int)drive, path);
	}
#endif
	preparedNameset = nameset;
	if (!preparedNameset)
		return 0x8001000c;
	startCluster = fnode->Stat.StartCluster;
	result = UVFAT_UnLinkFromDir(&g_sharedResource, preparedNameset->fileName, fnode->fileSystem, fnode);
	if (result < 0) {
		result = exfatfsErrorToIoError(result);
	} else if (!result) {
		if (fnode->refCount) {
			fnode->DirCluster = -2;
			fnode->DirOffset = 0;
			fnode->DirOffClust = -2;
			memset(fnode->FilePath, 0, sizeof(fnode->FilePath));
			fnode->FileName = fnode->FilePath;
		} else {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 190, "_invalidate_parent_entry",
				"%s: unexpected refCount(%hd), fdCount(%hd), fnode[%p], DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx], StartCluster[%#lx]\n",
				"_invalidate_parent_entry", 0, fnode->fdCount, fnode, fnode->DirCluster,
				fnode->DirOffset, fnode->DirOffClust, fnode->Stat.StartCluster);
#endif
		}
		if (!fnode->fdCount)
			_exfatfs_free_cluster_link(fnode, startCluster);
	}
	freeHeapBuffer((int)drive, preparedNameset);
	return result;
}

extern int UVFAT_MakeDir_vop(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFnode *parent, SceFatfsNameset *nameset, int mode,
	SceExfatfsFnode *fnode);

int sceExfatfsVopMkdir(SceExfatfsFnode *parent, const void *suffix,
	int suffixLength, int mode, SceExfatfsFnode **output)
{
	struct SceExfatfsFileSystem *fileSystem;
	SceExfatfsDrive *drive;
	SceFatfsNameset *nameset, *preparedNameset;
	SceExfatfsFnode *fnode;
	unsigned char *path;
	int result;
#if FW_VERSION >= 0x04000000
	SceUInt16 *printPath;
	int codeUnit;
#else
	SceUInt16 *parentPath;
	unsigned int remaining;
	int length;
#endif

#if FW_VERSION >= 0x04000000
	result = _directory_vnode_check(parent, "sceExfatfsVopMkdir");
	if (result) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 4768,
			"%s: _directory_vnode_check failed(%d)\n", "sceExfatfsVopMkdir", result);
		return exfatfsErrorToIoError(result);
	}
#else
	if (parent->Stat.st_type != 6) {
		parentPath = parent->FilePath;
		while (*parentPath)
			++parentPath;
		return exfatfsErrorToIoError(-6);
	}
	if (parent->DirCluster == (SceUInt32)-2 &&
	    _root_cluster(parent->fileSystem) != parent->Stat.StartCluster)
		return exfatfsErrorToIoError(-4);
#endif
	if (!parent->FileName)
		return 0x80010002;
	fileSystem = parent->fileSystem;
	drive = fileSystem->drive;
	SCE_DBG_ASSERT(suffixLength > 0, 4817);
	nameset = NULL;
#if FW_VERSION >= 0x04000000
	path = _temporal_utf8_fullpath((int)drive, parent, suffix, suffixLength);
	if (path) {
		result = sceFatfsPrepareNameset((int)drive, path, &nameset);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2199, "_temporal_nameset",
				"%s: sceFatfsPrepareNameset failed[%#x]\n", "_temporal_nameset", result);
		freeHeapBuffer((int)drive, path);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2207, "_temporal_nameset",
			"%s: sceFatfsCreateHeap failed\n", "_temporal_nameset");
	}
#else
	path = allocateHeapBuffer(drive);
	if (path) {
		memset(path, 0, 2048);
		length = _utf16_to_utf8(path, 2048, parent->FilePath);
		if (length < 0)
			goto free_path;
		if (length == 1) {
			remaining = 2047;
		} else {
			remaining = 2048 - length;
			if (suffix) {
				path[length++] = '/';
				--remaining;
			}
		}
		if ((unsigned int)suffixLength >= remaining)
			goto free_path;
		if (suffixLength) {
			memcpy(path + length, suffix, suffixLength);
			length += suffixLength;
		}
		path[length] = 0;
		sceFatfsPrepareNameset((int)drive, path, &nameset);
free_path:
		freeHeapBuffer((int)drive, path);
	}
#endif
	preparedNameset = nameset;
	if (!preparedNameset)
		return 0x8001000c;
	fnode = UVFAT_GetFnode(&g_sharedResource, fileSystem, preparedNameset->filePath);
	if (!fnode) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 4892,
			"%s: UVFAT_GetFnode failed\n", "sceExfatfsVopMkdir");
#endif
		result = 0x8001000c;
		goto free_nameset;
	}
	if (fnode->refCount == 1) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 0, 4829,
			"%s: fnode[%p], uninitialized StartCluster[%#lx], uninitialized Size[%#llx]\n",
			"sceExfatfsVopMkdir", fnode, fnode->Stat.StartCluster, fnode->Stat.Size);
#endif
		_initialize_cluster_cache_data(fnode);
	} else {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 4823,
			"%s: unexpected refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopMkdir", fnode->refCount, fnode->fdCount);
		SCE_DBG_PRINTF_LEVEL(1, 0, 4824,
			"%s: alias vnode will be created for fnode[%p], refCount(%hd), fdCount(%hd), StartCluster[%#lx], Size[%#llx]\n",
			"sceExfatfsVopMkdir", fnode, fnode->refCount, fnode->fdCount,
			fnode->Stat.StartCluster, fnode->Stat.Size);
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 134, "_print_fnode",
			"%s: fnode[%p], refCount(%hd), fdCount(%hd), path(",
			"sceExfatfsVopMkdir", fnode, fnode->refCount, fnode->fdCount);
		printPath = fnode->FilePath;
		codeUnit = *printPath;
		while (codeUnit) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 136, "_print_fnode", "%lc", codeUnit);
			codeUnit = *++printPath;
		}
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 139, "_print_fnode",
			"), FilePath[%p], FileName[%p]\n", fnode->FilePath, fnode->FileName);
#endif
	}
	if ((fileSystem->mount_mode & 255) == 1) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 4838,
			"%s: isReadOnlyMntMode, fileSystem[%p]\n", "sceExfatfsVopMkdir", fileSystem);
#endif
		result = 0x8001001e;
		goto release;
	}
	result = UVFAT_MakeDir_vop(&g_sharedResource, parent, preparedNameset, mode, fnode);
	if (result) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 4849,
			"%s: UVFAT_MakeDir failed(%d)\n", "sceExfatfsVopMkdir", result);
#endif
		result = exfatfsErrorToIoError(result);
		flushFileSystemMetadata(fileSystem);
	} else {
		result = flushFileSystemMetadata(fileSystem);
		if (result) {
#if FW_VERSION >= 0x04000000
			SCE_DBG_PRINTF_LEVEL(1, 0, 4855,
				"%s: UVFAT_WriteManagementArea failed(%d)\n", "sceExfatfsVopMkdir", result);
#endif
			result = exfatfsErrorToIoError(result);
		}
		_set_start_cluster_and_cluster_count(fnode, fnode->Stat.StartCluster, fnode->Stat.Size);
	}
	if (result < 0)
		goto release;
#if FW_VERSION >= 0x04000000
	if (fnode->Stat.st_type != 6)
		SCE_DBG_PRINTF_LEVEL(1, 0, 4877,
			"%s: unexpected type(%d), fnode[%p], StartCluster[%#lx], Size[%#llx], mode[%#o]\n",
			"sceExfatfsVopMkdir", fnode->Stat.st_type, fnode, fnode->Stat.StartCluster,
			fnode->Stat.Size, fnode->Stat.st_mode);
#endif
	UVFAT_ToUpperBuf(fnode->FilePath, fnode->FilePath, 260, fnode->fileSystem);
	*output = fnode;
	goto free_nameset;
release:
	UVFAT_ReleaseFnode(&g_sharedResource, fnode);
free_nameset:
	freeHeapBuffer((int)drive, preparedNameset);
	return result;
}

extern int UVFAT_Create(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFnode *parent, SceFatfsNameset *nameset, int mode,
	SceExfatfsFnode **fnode);

int sceExfatfsVopCreate(SceExfatfsFnode *parent, const void *suffix,
	int suffixLength, int mode, SceExfatfsFnode **output)
{
	struct SceExfatfsFileSystem *fileSystem = parent->fileSystem;
	SceExfatfsDrive *drive = fileSystem->drive;
	SceFatfsNameset *nameset, *preparedNameset;
	SceExfatfsFnode *fnode;
	int result;
#if FW_VERSION >= 0x04000000
	unsigned char *path;
	SceUInt16 *printPath;
	int codeUnit;
#else
	SceUInt16 *parentPath;
#endif

#if FW_VERSION >= 0x04000000
	result = _directory_vnode_check(parent, "sceExfatfsVopCreate");
#else
	if (parent->Stat.st_type != 6) {
		parentPath = parent->FilePath;
		while (*parentPath)
			++parentPath;
		result = -6;
	} else if (parent->DirCluster == (SceUInt32)-2 &&
	    _root_cluster(fileSystem) != parent->Stat.StartCluster) {
		result = -4;
	} else {
		result = 0;
	}
#endif
	*output = NULL;
	if (result)
		return exfatfsErrorToIoError(result);
	if (!parent->FileName)
		return 0x80010002;
	if ((fileSystem->mount_mode & 255) == 1) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 4979,
			"%s: isReadOnlyMntMode, fileSystem[%p]\n", "sceExfatfsVopCreate", fileSystem);
#endif
		return 0x8001001e;
	}
	fnode = NULL;
	SCE_DBG_ASSERT(suffixLength > 0, 4985);
#if FW_VERSION >= 0x04000000
	nameset = NULL;
	path = _temporal_utf8_fullpath((int)drive, parent, suffix, suffixLength);
	if (path) {
		result = sceFatfsPrepareNameset((int)drive, path, &nameset);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2199, "_temporal_nameset",
				"%s: sceFatfsPrepareNameset failed[%#x]\n", "_temporal_nameset", result);
		freeHeapBuffer((int)drive, path);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2207, "_temporal_nameset",
			"%s: sceFatfsCreateHeap failed\n", "_temporal_nameset");
	}
#else
	nameset = _temporal_nameset((int)drive, parent, suffix, suffixLength);
#endif
	preparedNameset = nameset;
	if (!preparedNameset)
		return 0x8001000c;
	result = UVFAT_Create(&g_sharedResource, parent, preparedNameset, mode, &fnode);
	if (result) {
		result = exfatfsErrorToIoError(result);
		freeHeapBuffer((int)drive, preparedNameset);
		if (result)
			return result;
	} else {
		SCE_DBG_ASSERT(fnode != NULL, 4994);
		freeHeapBuffer((int)drive, preparedNameset);
	}
	if (fnode->refCount == 1) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 0, 5008,
			"%s: fnode[%p], StartCluster[%#lx], Size[%#llx]\n",
			"sceExfatfsVopCreate", fnode, fnode->Stat.StartCluster, fnode->Stat.Size);
		if (fnode->Stat.st_type)
			SCE_DBG_PRINTF_LEVEL(1, 0, 5013,
				"%s: unexpected type(%d), fnode[%p], StartCluster[%#lx], Size[%#llx], mode[%#o]\n",
				"sceExfatfsVopCreate", fnode->Stat.st_type, fnode,
				fnode->Stat.StartCluster, fnode->Stat.Size, fnode->Stat.st_mode);
#endif
		_initialize_cluster_cache_data(fnode);
		_set_start_cluster_and_cluster_count(fnode, fnode->Stat.StartCluster, 0);
	} else {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 5002,
			"%s: unexpected refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopCreate", fnode->refCount, fnode->fdCount);
		SCE_DBG_PRINTF_LEVEL(1, 0, 5003,
			"%s: alias vnode will be created for fnode[%p], refCount(%hd), fdCount(%hd), StartCluster[%#lx], Size[%#llx]\n",
			"sceExfatfsVopCreate", fnode, fnode->refCount, fnode->fdCount,
			fnode->Stat.StartCluster, fnode->Stat.Size);
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 134, "_print_fnode",
			"%s: fnode[%p], refCount(%hd), fdCount(%hd), path(",
			"sceExfatfsVopCreate", fnode, fnode->refCount, fnode->fdCount);
		printPath = fnode->FilePath;
		codeUnit = *printPath;
		while (codeUnit) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 136, "_print_fnode", "%lc", codeUnit);
			codeUnit = *++printPath;
		}
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 139, "_print_fnode",
			"), FilePath[%p], FileName[%p]\n", fnode->FilePath, fnode->FileName);
#endif
	}
	UVFAT_ToUpperBuf(fnode->FilePath, fnode->FilePath, 260, fnode->fileSystem);
	*output = fnode;
	return 0;
}

int sceExfatfsVopLink(SceExfatfsFnode *from, SceExfatfsFnode *to)
{
	int result;

#if FW_VERSION >= 0x04000000
	if (!from->Stat.st_type)
		result = _file_vnode_check(from, "sceExfatfsVopLink(from)");
	else if (from->Stat.st_type == 6)
		result = _directory_vnode_check(from, "sceExfatfsVopLink(from)");
	else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1852, "_vnode_check",
			"%s: unsupported st_type(%d)\n", "sceExfatfsVopLink(from)", from->Stat.st_type);
		result = -6;
	}
	if (result)
		goto finish;
	if (!to->Stat.st_type)
		result = _file_vnode_check(to, "sceExfatfsVopLink(to)");
	else if (to->Stat.st_type == 6)
		result = _directory_vnode_check(to, "sceExfatfsVopLink(to)");
	else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1852, "_vnode_check",
			"%s: unsupported st_type(%d)\n", "sceExfatfsVopLink(to)", to->Stat.st_type);
		result = -6;
	}
	if (result)
		goto finish;
	SCE_DBG_PRINTF_LEVEL(1, 0, 5099,
		"%s: from[%p] -> to[%p], from_refCount(%hd), to_refCount(%hd)\n",
		"sceExfatfsVopLink", from, to, from->refCount, to->refCount);
#else
	if (!from->Stat.st_type) {
		if (from->DirCluster == (SceUInt32)-2)
			goto deleted;
	} else if (from->Stat.st_type != 6) {
		goto unsupported;
	} else if (from->DirCluster == (SceUInt32)-2 &&
	    _root_cluster(from->fileSystem) != from->Stat.StartCluster) {
		goto deleted;
	}
	if (!to->Stat.st_type) {
		if (to->DirCluster == (SceUInt32)-2)
			goto deleted;
	} else if (to->Stat.st_type != 6) {
		goto unsupported;
	} else if (to->DirCluster == (SceUInt32)-2 &&
	    _root_cluster(to->fileSystem) != to->Stat.StartCluster) {
		goto deleted;
	}
	result = 0;
#endif
	++from->refCount;
	if (from != to)
		++to->refCount;
#if FW_VERSION >= 0x04000000
	return result;
finish:
	return result < 0 ? exfatfsErrorToIoError(result) : result;
#else
	goto finish;
unsupported:
	result = -6;
	goto convert;
deleted:
	result = -4;
convert:
	result = exfatfsErrorToIoError(result);
finish:
	return result;
#endif
}

int sceExfatfsVopUnlink(SceExfatfsFnode *from, SceExfatfsFnode *to)
{
	int result;
#if FW_VERSION >= 0x04000000
	SceInt16 refCount;
#else
	SceUInt16 refCount;
#endif

#if FW_VERSION >= 0x04000000
	if (!from->Stat.st_type)
		result = _file_vnode_check(from, "sceExfatfsVopUnlink(from)");
	else if (from->Stat.st_type == 6)
		result = _directory_vnode_check(from, "sceExfatfsVopUnlink(from)");
	else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1852, "_vnode_check",
			"%s: unsupported st_type(%d)\n", "sceExfatfsVopUnlink(from)", from->Stat.st_type);
		result = -6;
	}
	if (result)
		goto finish;
	if (!to->Stat.st_type)
		result = _file_vnode_check(to, "sceExfatfsVopUnlink(to)");
	else if (to->Stat.st_type == 6)
		result = _directory_vnode_check(to, "sceExfatfsVopUnlink(to)");
	else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1852, "_vnode_check",
			"%s: unsupported st_type(%d)\n", "sceExfatfsVopUnlink(to)", to->Stat.st_type);
		result = -6;
	}
	if (result)
		goto finish;
	SCE_DBG_PRINTF_LEVEL(1, 0, 5123,
		"%s: from[%p] -> to[%p], from_refCount(%hd), to_refCount(%hd)\n",
		"sceExfatfsVopUnlink", from, to, from->refCount, to->refCount);
#else
	if (!from->Stat.st_type) {
		if (from->DirCluster == (SceUInt32)-2)
			goto deleted;
	} else if (from->Stat.st_type != 6) {
		goto unsupported;
	} else if (from->DirCluster == (SceUInt32)-2 &&
	    _root_cluster(from->fileSystem) != from->Stat.StartCluster) {
		goto deleted;
	}
	if (!to->Stat.st_type) {
		if (to->DirCluster == (SceUInt32)-2)
			goto deleted;
	} else if (to->Stat.st_type != 6) {
		goto unsupported;
	} else if (to->DirCluster == (SceUInt32)-2 &&
	    _root_cluster(to->fileSystem) != to->Stat.StartCluster) {
		goto deleted;
	}
	result = 0;
#endif
	refCount = from->refCount;
	if (refCount == 1) {
#if FW_VERSION >= 0x04000000
		if (_deleted_entry(from))
			SCE_DBG_PRINTF_LEVEL(1, 0, 5126,
				"%s: deleted from_fnode[%p], refCount(%hd), fdCount(%hd)\n",
				"sceExfatfsVopUnlink", from, from->refCount, from->fdCount);
#else
		if (from->FilePath[0] && !_has_parent_directory(from) &&
		    from->fileSystem && from->Stat.st_type == 6)
			_root_cluster(from->fileSystem);
#endif
		result = UVFAT_ReleaseFnode(&g_sharedResource, from);
	} else {
		from->refCount = refCount - 1;
	}
	if (from != to) {
		refCount = to->refCount;
		if (refCount == 1) {
#if FW_VERSION >= 0x04000000
			if (_deleted_entry(to))
				SCE_DBG_PRINTF_LEVEL(1, 0, 5135,
					"%s: deleted to_fnode[%p], refCount(%hd), fdCount(%hd)\n",
					"sceExfatfsVopUnlink", to, to->refCount, to->fdCount);
#else
			if (to->FilePath[0] && !_has_parent_directory(to) &&
			    to->fileSystem && to->Stat.st_type == 6)
				_root_cluster(to->fileSystem);
#endif
			result = UVFAT_ReleaseFnode(&g_sharedResource, to);
		} else {
			to->refCount = refCount - 1;
		}
	}
finish:
	if (result < 0)
		result = exfatfsErrorToIoError(result);
	return result;
#if FW_VERSION < 0x04000000
unsupported:
	result = -6;
	goto finish;
deleted:
	result = -4;
	goto finish;
#endif
}

extern int UVFAT_LinkFromDir_ex(SceExfatfsSharedResource *sharedResource,
	const SceUInt16 *fileName, SceExfatfsFileSystem *fileSystem,
	SceExfatfsFnode *fnode, SceExfatfsFnode *parent);
extern int _change_path_element(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, const SceUInt16 *oldPath,
	int keepLength, SceExfatfsFnode *fnode);

int sceExfatfsVopRename(SceExfatfsFnode *oldFnode, SceExfatfsFnode *parent,
	const void *suffix, int suffixLength, SceExfatfsFnode **output)
{
	SceExfatfsFileSystem *fileSystem = parent->fileSystem;
	SceFatfsNameset *nameset;
	SceFatfsNameset *preparedNameset;
	SceFatfsNameset *oldNameset;
	SceExfatfsFnode *fnode;
	unsigned char *path;
	int drive;
	int result;
	int error;
	int length;
#if FW_VERSION < 0x04000000
	SceUInt16 *parentPath;
#endif

#if FW_VERSION >= 0x04000000
	result = _directory_vnode_check(parent, "sceExfatfsVopRename");
#else
	if (parent->Stat.st_type != 6) {
		parentPath = parent->FilePath;
		while (*parentPath)
			++parentPath;
		result = -6;
	} else if (parent->DirCluster == (SceUInt32)-2 &&
	    _root_cluster(fileSystem) != parent->Stat.StartCluster) {
		result = -4;
	} else {
		result = 0;
	}
#endif
	*output = NULL;
#if FW_VERSION >= 0x04000000
	if (_deleted_entry(oldFnode)) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 5163,
			"%s: deleted old_fnode[%p], refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopRename", oldFnode, oldFnode->refCount, oldFnode->fdCount);
		return 0x80010002;
	}
#else
	if (oldFnode->FilePath[0]) {
		if (!_has_parent_directory(oldFnode) && oldFnode->fileSystem &&
		    oldFnode->Stat.st_type == 6)
			_root_cluster(oldFnode->fileSystem);
	} else if (oldFnode->fileSystem) {
		return 0x80010002;
	}
#endif
	if (!oldFnode->FileName) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 5168,
			"%s: renamed old_fnode[%p], refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopRename", oldFnode, oldFnode->refCount, oldFnode->fdCount);
		return 0x80010002;
	}
	if (result) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 5173,
			"%s: _directory_vnode_check for parent_vnode failed(%d)\n",
			"sceExfatfsVopRename", result);
		return exfatfsErrorToIoError(result);
	}
	if (!parent->FileName)
		return 0x80010002;
	drive = (int)fileSystem->drive;
	nameset = NULL;
#if FW_VERSION >= 0x04000000
	path = _temporal_utf8_fullpath(drive, oldFnode, NULL, 0);
	if (path) {
		result = sceFatfsPrepareNameset(drive, path, &nameset);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2199, "_temporal_nameset",
				"%s: sceFatfsPrepareNameset failed[%#x]\n", "_temporal_nameset", result);
		freeHeapBuffer(drive, path);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2207, "_temporal_nameset",
			"%s: sceFatfsCreateHeap failed\n", "_temporal_nameset");
	}
#else
	path = allocateHeapBuffer((SceExfatfsDrive *)drive);
	if (path) {
		memset(path, 0, 2048);
		length = _utf16_to_utf8(path, 2048, oldFnode->FilePath);
		if (length >= 0 && length != 2048) {
			path[length] = 0;
			sceFatfsPrepareNameset(drive, path, &nameset);
		}
		freeHeapBuffer(drive, path);
	}
#endif
	oldNameset = nameset;
	SCE_DBG_ASSERT(suffixLength > 0, 5214);
	nameset = NULL;
#if FW_VERSION >= 0x04000000
	path = _temporal_utf8_fullpath(drive, parent, suffix, suffixLength);
	if (path) {
		result = sceFatfsPrepareNameset(drive, path, &nameset);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2199, "_temporal_nameset",
				"%s: sceFatfsPrepareNameset failed[%#x]\n", "_temporal_nameset", result);
		freeHeapBuffer(drive, path);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2207, "_temporal_nameset",
			"%s: sceFatfsCreateHeap failed\n", "_temporal_nameset");
	}
#else
	nameset = _temporal_nameset(drive, parent, suffix, suffixLength);
#endif
	preparedNameset = nameset;
	if (!oldNameset || !preparedNameset) {
		result = 0x8001000c;
		goto free_namesets;
	}
	fnode = UVFAT_GetFnode(&g_sharedResource, fileSystem, preparedNameset->filePath);
	if (!fnode) {
		result = 0x8001000c;
		goto free_namesets;
	}
	if (fnode->refCount == 1) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 5228,
			"%s: fnode[%p], uninitialized StartCluster[%#lx], uninitialized Size[%#llx]\n",
			"sceExfatfsVopRename", fnode, fnode->Stat.StartCluster, fnode->Stat.Size);
		_initialize_cluster_cache_data(fnode);
	} else {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 5222, "%s: refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopRename", fnode->refCount, fnode->fdCount);
		SCE_DBG_PRINTF_LEVEL(1, 0, 5223, "%s: fnode[%p]\n", "sceExfatfsVopRename", fnode);
		if (_deleted_entry(fnode))
			SCE_DBG_PRINTF_LEVEL(1, 0, 5225,
				"%s: deleted fnode[%p], refCount(%hd), fdCount(%hd)\n",
				"sceExfatfsVopRename", fnode, fnode->refCount, fnode->fdCount);
#else
		if (fnode->FilePath[0] && !_has_parent_directory(fnode) &&
		    fnode->fileSystem && fnode->Stat.st_type == 6)
			_root_cluster(fnode->fileSystem);
#endif
	}
	++oldFnode->refCount;
	error = -1;
	if (fileSystem->file.mode) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 5241, "%s: unexpected Mode(%d)\n",
			"sceExfatfsVopRename", fileSystem->file.mode);
	} else {
		fnode->Stat = oldFnode->Stat;
		error = UVFAT_UnLinkFromDir(&g_sharedResource, oldNameset->fileName, fileSystem, oldFnode);
		if (error) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 5247, "%s: UVFAT_UnLinkFromDir failed(%d)\n",
				"sceExfatfsVopRename", error);
		} else {
			fnode->DirCluster = parent->Stat.StartCluster;
			fnode->dirGeneralSecondaryFlags = parent->Stat.generalSecondaryFlags;
			fnode->dirSize = parent->Stat.Size;
			error = UVFAT_LinkFromDir_ex(&g_sharedResource, preparedNameset->fileName, fileSystem, fnode, parent);
			if (!error) {
				oldFnode->DirOffset = fnode->DirOffset;
				oldFnode->DirCluster = fnode->DirCluster;
				oldFnode->dirSize = fnode->dirSize;
				oldFnode->dirGeneralSecondaryFlags = fnode->dirGeneralSecondaryFlags;
				oldFnode->DirOffClust = fnode->DirOffClust;
				memcpy(oldFnode->FilePath, fnode->FilePath, sizeof(oldFnode->FilePath));
				oldFnode->FileName = oldFnode->FilePath + (fnode->FileName - fnode->FilePath);
				if (fnode->refCount) {
					fnode->DirCluster = -2;
					fnode->DirOffset = 0;
					fnode->DirOffClust = -2;
					fnode->FileName = NULL;
				} else {
					SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 190, "_invalidate_parent_entry",
						"%s: unexpected refCount(%hd), fdCount(%hd), fnode[%p], DirCluster[%#lx], DirOffset[%#llx], DirOffClust[%#lx], StartCluster[%#lx]\n",
						"_invalidate_parent_entry", fnode->refCount, fnode->fdCount, fnode,
						fnode->DirCluster, fnode->DirOffset, fnode->DirOffClust, fnode->Stat.StartCluster);
				}
				UVFAT_ReleaseFnode(&g_sharedResource, fnode);
				fnode = oldFnode;
				flushFileSystemMetadata(fileSystem);
				goto change_path;
			}
			SCE_DBG_PRINTF_LEVEL(1, 0, 5254, "%s: UVFAT_LinkFromDir failed(%d)\n",
				"sceExfatfsVopRename", error);
		}
		flushFileSystemMetadata(fileSystem);
	}
	result = exfatfsErrorToIoError(error);
	if (result < 0) {
		--oldFnode->refCount;
		goto free_namesets;
	}
	if (result)
		goto free_namesets;
change_path:
	UVFAT_ToUpperBuf(fnode->FilePath, fnode->FilePath, 260, fnode->fileSystem);
	result = _change_path_element(&g_sharedResource, fileSystem, oldNameset->filePath, 0, fnode);
	if (!result)
		*output = fnode;
free_namesets:
	if (preparedNameset)
		freeHeapBuffer(drive, preparedNameset);
	if (oldNameset)
		freeHeapBuffer(drive, oldNameset);
	return result;
}

extern int initializeFatDateTime(SceExfatfsDateTime *dateTime);

int _VopGetstatRootdir(SceExfatfsDrive *drive, SceIoStat *output)
{
	unsigned char attribute = 0;
	unsigned char specialAttribute;
	SceExfatfsStat stat;
	int permission;
	int result;

	if (sceFatfsACGetRootDirAttribute(drive, &attribute) < 0)
		return 0x80010002;
	if (attribute != 15) {
		applyFatAttributeToStat(drive, attribute | 16, &stat);
	} else {
		permission = sceFatfsACGetDirPermission(drive, 15);
		if (permission < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 5789,
				"%s: sceFatfsACGetDirPermission for special attr failed[%#x]\n",
				"_VopGetstatRootdir", permission);
			return 0x80010002;
		}
		specialAttribute = 0;
		result = sceFatfsACSetFSAttributeByPermission(drive, permission & 0666, &specialAttribute);
		if (result) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 5795,
				"%s: sceFatfsACSetFSAttributeByPermission failed[%#x], check_mode[%#o]\n",
				"_VopGetstatRootdir", result, permission);
			return 0x80010002;
		}
		applyFatAttributeToStat(drive, specialAttribute | 16, &stat);
	}
	initializeFatDateTime(&stat.ctime);
	initializeFatDateTime(&stat.atime);
	initializeFatDateTime(&stat.mtime);
	stat.Size = 0;
	exfatfsStatToIoStat(drive, &stat, output);
	return 0;
}

int sceExfatfsVopSync(SceExfatfsFnode *fnode, SceUInt32 fd, int flags)
{
	SceExfatfsFileSystem *fileSystem;
	SceExfatfsDrive *drive;
	SceExfatfsFile *file;
	SceVfsVnode *deviceVnode;
	SceVfsFile *deviceFile;
	int result;
#if FW_VERSION >= 0x04000000
	SceVfsMountData *mount;
#endif

	if (!fnode) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 5860, "%s: unexpected NULL fnode\n", "sceExfatfsVopSync");
		result = 0x80010016;
		goto finish;
	}
	fileSystem = fnode->fileSystem;
#if FW_VERSION >= 0x04000000
	if (fileSystem) {
		drive = fileSystem->drive;
		if (drive && (drive->state & 0x4000))
			SCE_DBG_PRINTF_LEVEL(1, 0, 5870,
				"%s: force_umounting, fnode[%p], refCount(%hd), fdCount(%hd)\n",
				"sceExfatfsVopSync", fnode, fnode->refCount, fnode->fdCount);
	} else {
		SCE_DBG_PRINTF_LEVEL(1, 0, 5867,
			"%s: unexpected NULL fileSystem (force umounted?)\n", "sceExfatfsVopSync");
	}
#endif
	if ((fileSystem->mount_mode & 255) == 1)
		goto success;
	if (fd) {
		if (fd >= getInitializedFileCount(&g_sharedResource)) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 5960, "%s: unexpected file_data[%p]\n",
				"sceExfatfsVopSync", (void *)fd);
			result = 0x80010051;
			goto finish;
		}
		file = &g_sharedResource.files[fd];
		if ((file->mode & ~4) == 2) {
			result = UVFAT_FlushFdBuf(file, fnode->fileSystem);
			if (result) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 5974, "%s: UVFAT_FlushWriteBuffer failed(%d)\n",
					"sceExfatfsVopSync", result);
				goto convert_error;
			}
		}
	} else {
		_flush_fd_cache(&g_sharedResource, fnode);
	}
	result = _exfatfs_flush_metadata(&g_sharedResource, fnode);
	if (result) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 5993,
			"%s: update metadata for vnode_data[%p] failed(%d)\n",
			"sceExfatfsVopSync", fnode, result);
		goto convert_error;
	}
	fnode->flags &= ~4;
	if (flags)
		goto success;
	flushFileSystemMetadata(fileSystem);
	drive = fileSystem->drive;
	if (!drive)
		goto success;
	deviceVnode = drive->deviceInfo.device_vnode;
	deviceFile = drive->deviceInfo.device_file;
	if (!deviceVnode || drive->deviceInfo.device_fd == -1) {
		result = 0x80010013;
	} else {
#if FW_VERSION >= 0x04000000
		mount = drive->deviceInfo.mount_misc;
#endif
		vfsLockVnode(deviceVnode);
		result = sceVopSync(deviceVnode, deviceFile, flags);
		vfsUnlockVnode(deviceVnode);
		if (result >= 0)
			goto finish;
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 5845, "_exfatfs_sync_device_file",
			"%s: sceVopSync failed[%#x], assignName(%s), device_vnode[%p], device_file[%p]\n",
			"_exfatfs_sync_device_file", result, mount->assignName, deviceVnode, deviceFile);
		drive = fileSystem->drive;
		if (!drive)
			goto finish;
	}
	if (drive->state & 0x4000) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 6011,
			"%s: force_umounting, fnode[%p], refCount(%hd), fdCount(%hd)\n",
			"sceExfatfsVopSync", fnode, fnode->refCount, fnode->fdCount);
		result = 0;
	}
	goto finish;
convert_error:
	drive = fileSystem->drive;
	if (drive && (drive->state & 0x4000))
		result = 0;
	result = exfatfsErrorToIoError(result);
	goto finish;
success:
	result = 0;
finish:
	return result;
}

extern SceExfatfsFile *invalidateFileCachePosition(SceExfatfsFile *file);
extern int exfatfs_cleanup_device_file_buffer(SceExfatfsDrive *drive);

int sceExfatfsVopCleanup(SceExfatfsFnode *fnode, SceUInt32 fd)
{
	SceExfatfsFileSystem *fileSystem;
	SceExfatfsFile *file;
	int result;
#if FW_VERSION >= 0x04000000
	SceExfatfsDrive *drive;
#endif

	if (!fnode) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 6034, "%s: unexpected NULL fnode\n", "sceExfatfsVopCleanup");
		return 0x80010016;
	}
	fileSystem = fnode->fileSystem;
#if FW_VERSION >= 0x04000000
	if (fileSystem) {
		drive = fileSystem->drive;
		if (drive && (drive->state & 0x4000))
			SCE_DBG_PRINTF_LEVEL(1, 0, 6044,
				"%s: force_umounting, fnode[%p], refCount(%hd), fdCount(%hd)\n",
				"sceExfatfsVopCleanup", fnode, fnode->refCount, fnode->fdCount);
	} else {
		SCE_DBG_PRINTF_LEVEL(1, 0, 6041,
			"%s: unexpected NULL fileSystem (force umounted?)\n", "sceExfatfsVopCleanup");
	}
#endif
	if ((fileSystem->mount_mode & 255) == 1)
		return 0;
	if (fd) {
		if (fd >= getInitializedFileCount(&g_sharedResource)) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 6058, "%s: unexpected file_data[%p]\n",
				"sceExfatfsVopCleanup", (void *)fd);
			return 0x80010051;
		}
		file = &g_sharedResource.files[fd];
		if ((file->mode & ~4) == 2) {
			result = UVFAT_FlushFdBuf(file, fnode->fileSystem);
			if (result) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 6068,
					"%s: UVFAT_FlushWriteBuffer failed(%d), just invalidate\n",
					"sceExfatfsVopCleanup", result);
				file->cacheFlags = 0;
				invalidateFileCachePosition(file);
			}
		}
	} else {
		SCE_DBG_PRINTF_LEVEL(1, 0, 6051, "%s: unexpected file_data[%p]\n",
			"sceExfatfsVopCleanup", (void *)fd);
		_flush_fd_cache(&g_sharedResource, fnode);
	}
	result = _exfatfs_flush_metadata(&g_sharedResource, fnode);
	if (result) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 6086,
			"%s: flush metadata for vnode_data[%p] failed(%d)\n",
			"sceExfatfsVopCleanup", fnode, result);
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6025, "_exfatfs_read_clean_metadata",
			"%s: not implemented yet, needs to call vop_inactive for fnode[%p]\n",
			"_exfatfs_read_clean_metadata", fnode);
	}
	fnode->flags &= ~4;
	flushFileSystemMetadata(fileSystem);
	if (fileSystem->drive)
		exfatfs_cleanup_device_file_buffer(fileSystem->drive);
	return 0;
}

int _exfatfs_flush_all_data(SceExfatfsDrive *drive)
{
	SceExfatfsFileSystem *fileSystem;
	SceExfatfsFileSystem *flushFileSystem;
	SceExfatfsFnode *fnode;
	SceExfatfsFile *file;
	SceUInt32 fileCount;
	SceUInt32 fd;
	int index;
	int result;

	if (!drive) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6246, "exfatfs_flush_all",
			"%s: NULL drive\n", "exfatfs_flush_all");
		return 0x80010013;
	}
	if (g_sharedResource.fileSystems[0].devname && drive == g_sharedResource.fileSystems[0].drive)
		index = 0;
	else if (g_sharedResource.fileSystems[1].devname && drive == g_sharedResource.fileSystems[1].drive)
		index = 1;
	else if (g_sharedResource.fileSystems[2].devname && drive == g_sharedResource.fileSystems[2].drive)
		index = 2;
	else if (g_sharedResource.fileSystems[3].devname && drive == g_sharedResource.fileSystems[3].drive)
		index = 3;
	else if (g_sharedResource.fileSystems[4].devname && drive == g_sharedResource.fileSystems[4].drive)
		index = 4;
	else if (g_sharedResource.fileSystems[5].devname && drive == g_sharedResource.fileSystems[5].drive)
		index = 5;
	else if (g_sharedResource.fileSystems[6].devname && drive == g_sharedResource.fileSystems[6].drive)
		index = 6;
	else if (g_sharedResource.fileSystems[7].devname && drive == g_sharedResource.fileSystems[7].drive)
		index = 7;
	else if (g_sharedResource.fileSystems[8].devname && drive == g_sharedResource.fileSystems[8].drive)
		index = 8;
	else if (g_sharedResource.fileSystems[9].devname && drive == g_sharedResource.fileSystems[9].drive)
		index = 9;
	else if (g_sharedResource.fileSystems[10].devname && drive == g_sharedResource.fileSystems[10].drive)
		index = 10;
	else if (g_sharedResource.fileSystems[11].devname && drive == g_sharedResource.fileSystems[11].drive)
		index = 11;
	else if (g_sharedResource.fileSystems[12].devname && drive == g_sharedResource.fileSystems[12].drive)
		index = 12;
	else if (g_sharedResource.fileSystems[13].devname && drive == g_sharedResource.fileSystems[13].drive)
		index = 13;
	else if (g_sharedResource.fileSystems[14].devname && drive == g_sharedResource.fileSystems[14].drive)
		index = 14;
	else
		index = -1;
	fileSystem = index < 0 ? NULL : &g_sharedResource.fileSystems[index];
	fileCount = getInitializedFileCount(&g_sharedResource);
	if (g_sharedResource.fileSystems[0].devname && drive == g_sharedResource.fileSystems[0].drive)
		index = 0;
	else if (g_sharedResource.fileSystems[1].devname && drive == g_sharedResource.fileSystems[1].drive)
		index = 1;
	else if (g_sharedResource.fileSystems[2].devname && drive == g_sharedResource.fileSystems[2].drive)
		index = 2;
	else if (g_sharedResource.fileSystems[3].devname && drive == g_sharedResource.fileSystems[3].drive)
		index = 3;
	else if (g_sharedResource.fileSystems[4].devname && drive == g_sharedResource.fileSystems[4].drive)
		index = 4;
	else if (g_sharedResource.fileSystems[5].devname && drive == g_sharedResource.fileSystems[5].drive)
		index = 5;
	else if (g_sharedResource.fileSystems[6].devname && drive == g_sharedResource.fileSystems[6].drive)
		index = 6;
	else if (g_sharedResource.fileSystems[7].devname && drive == g_sharedResource.fileSystems[7].drive)
		index = 7;
	else if (g_sharedResource.fileSystems[8].devname && drive == g_sharedResource.fileSystems[8].drive)
		index = 8;
	else if (g_sharedResource.fileSystems[9].devname && drive == g_sharedResource.fileSystems[9].drive)
		index = 9;
	else if (g_sharedResource.fileSystems[10].devname && drive == g_sharedResource.fileSystems[10].drive)
		index = 10;
	else if (g_sharedResource.fileSystems[11].devname && drive == g_sharedResource.fileSystems[11].drive)
		index = 11;
	else if (g_sharedResource.fileSystems[12].devname && drive == g_sharedResource.fileSystems[12].drive)
		index = 12;
	else if (g_sharedResource.fileSystems[13].devname && drive == g_sharedResource.fileSystems[13].drive)
		index = 13;
	else if (g_sharedResource.fileSystems[14].devname && drive == g_sharedResource.fileSystems[14].drive)
		index = 14;
	else {
		SCE_DBG_PRINTF_LEVEL(2, 15, 6124, "%s(): Cannot get mountpoint\n", "_exfatfs_flush_all_data");
		goto flush_metadata;
	}
	flushFileSystem = &g_sharedResource.fileSystems[index];
	if ((flushFileSystem->mount_mode & 255) != 1) {
#if FW_VERSION >= 0x04000000
		result = sceKernelTryLockFastMutex(&drive->fastmutex);
		if (result != (int)0x80028162) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 6154, "%s: unexpected result[%#x]\n",
				"_exfatfs_flush_all_data", result);
			SCE_DBG_ASSERT(0, 6155);
		}
#endif
		if (fileCount > 1) {
			for (fd = 1; fd != fileCount; ++fd) {
				file = &g_sharedResource.files[fd];
				fnode = file->fnode;
				if (fnode && fnode->fileSystem == flushFileSystem && (file->mode & ~4) == 2)
					sceExfatfsVopSync(fnode, fd, 1);
			}
		}
	}
flush_metadata:
	flushFileSystemMetadata(fileSystem);
	result = exfatfs_cleanup_device_file_buffer(drive);
	if (result < 0 && fileSystem && fileSystem->drive &&
	    (fileSystem->drive->state & 0x4000)) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 6241, "exfatfs_flush_all",
			"%s: force_umounting, ignore error[%#x]\n", "exfatfs_flush_all", result);
		return 0;
	}
	return result;
}

extern void _FreeFpCache(SceExfatfsFile *file);

int sceExfatfsVopUmount(SceExfatfsDrive *drive, int mode)
{
	SceExfatfsFileSystem *fileSystem;
	SceExfatfsDrive *mountedDrive;
	SceExfatfsFnode *fnode;
	SceExfatfsFnode *whiteout;
	SceExfatfsFile *file;
	SceExfatfsClusterCacheElement *cache;
	SceUInt32 fileCount;
	SceUInt32 fnodeCount;
	SceUInt32 index;
	int mountIndex;
	int used;
	int result;

	if (g_sharedResource.fileSystems[0].devname && drive == g_sharedResource.fileSystems[0].drive)
		mountIndex = 0;
	else if (g_sharedResource.fileSystems[1].devname && drive == g_sharedResource.fileSystems[1].drive)
		mountIndex = 1;
	else if (g_sharedResource.fileSystems[2].devname && drive == g_sharedResource.fileSystems[2].drive)
		mountIndex = 2;
	else if (g_sharedResource.fileSystems[3].devname && drive == g_sharedResource.fileSystems[3].drive)
		mountIndex = 3;
	else if (g_sharedResource.fileSystems[4].devname && drive == g_sharedResource.fileSystems[4].drive)
		mountIndex = 4;
	else if (g_sharedResource.fileSystems[5].devname && drive == g_sharedResource.fileSystems[5].drive)
		mountIndex = 5;
	else if (g_sharedResource.fileSystems[6].devname && drive == g_sharedResource.fileSystems[6].drive)
		mountIndex = 6;
	else if (g_sharedResource.fileSystems[7].devname && drive == g_sharedResource.fileSystems[7].drive)
		mountIndex = 7;
	else if (g_sharedResource.fileSystems[8].devname && drive == g_sharedResource.fileSystems[8].drive)
		mountIndex = 8;
	else if (g_sharedResource.fileSystems[9].devname && drive == g_sharedResource.fileSystems[9].drive)
		mountIndex = 9;
	else if (g_sharedResource.fileSystems[10].devname && drive == g_sharedResource.fileSystems[10].drive)
		mountIndex = 10;
	else if (g_sharedResource.fileSystems[11].devname && drive == g_sharedResource.fileSystems[11].drive)
		mountIndex = 11;
	else if (g_sharedResource.fileSystems[12].devname && drive == g_sharedResource.fileSystems[12].drive)
		mountIndex = 12;
	else if (g_sharedResource.fileSystems[13].devname && drive == g_sharedResource.fileSystems[13].drive)
		mountIndex = 13;
	else if (g_sharedResource.fileSystems[14].devname && drive == g_sharedResource.fileSystems[14].drive)
		mountIndex = 14;
	else {
		SCE_DBG_PRINTF_LEVEL(2, 0, 1690,
			"%s: _findRootFileSystemEntry for drive[%p] failed[%#x]\n",
			"sceExfatfsVopUmount", drive, 0x80010013);
		return 0x80010013;
	}
	fileSystem = &g_sharedResource.fileSystems[mountIndex];
	if (!mode) {
		fileCount = getInitializedFileCount(&g_sharedResource);
#if FW_VERSION >= 0x04000000
		result = sceKernelTryLockFastMutex(&fileSystem->drive->fastmutex);
		if (result != (int)0x80028162) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1261, "_used_fd_count",
				"%s: unexpected result[%#x]\n", "_used_fd_count", result);
			SCE_DBG_ASSERT_FUNC(0, 1262, "_used_fd_count");
		}
#endif
		used = 0;
		if (fileCount > 1) {
			file = g_sharedResource.files;
			do {
				if (file[1].mode && file[1].fnode && file[1].fnode->fileSystem == fileSystem)
					++used;
				++file;
			} while (file != &g_sharedResource.files[fileCount - 1]);
		}
		if (!used)
			return 0;
		result = 0x80010010;
		goto failed;
	}
	if (mode != 1) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 1681, "%s: unsupported mode[%#x]\n",
			"sceExfatfsVopUmount", mode);
		result = 0x80010016;
		goto failed;
	}
	mountedDrive = fileSystem->drive;
	if (!mountedDrive) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 1669, "%s: NULL drive\n", "sceExfatfsVopUmount");
		return 0;
	}
	if (!drive)
		return 0;
	drive->state |= 0x4000;
	if (drive != mountedDrive)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 1544, "_start_force_umount",
			"%s: unexpected drive[%p] != fileSystem->drive[%p]\n",
			"_start_force_umount", drive, mountedDrive);
	_exfatfs_flush_all_data(drive);
	whiteout = fileSystem->whiteout_directory;
	if (whiteout) {
		fileSystem->whiteout_directory = NULL;
		result = UVFAT_ReleaseFnode(&g_sharedResource, whiteout);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 4361, "_clear_whiteout_directory",
				"%s: UVFAT_ReleaseFnode failed[%#x]\n", "_clear_whiteout_directory", result);
			fileSystem->whiteout_directory = whiteout;
		}
	}
	result = lockFastMutex(&g_sharedResourceMutex);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 99, "_exfat_lock_shared_resource",
			"%s: sceKernelLockFastMutex failed[%#x]\n", "_disconnect_drive", result);
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 1501, "_disconnect_drive",
			"%s: _exfat_lock_shared_resource failed[%#x]\n", "_disconnect_drive", result);
		return 0;
	}
	fileCount = getInitializedFileCount(&g_sharedResource);
#if FW_VERSION >= 0x04000000
	result = sceKernelTryLockFastMutex(&fileSystem->drive->fastmutex);
	if (result != (int)0x80028162) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1400, "_disconnect_all_fd",
			"%s: unexpected result[%#x]\n", "_disconnect_all_fd", result);
		SCE_DBG_ASSERT_FUNC(0, 1401, "_disconnect_all_fd");
	}
#endif
	if (fileCount > 1) {
		for (index = 1; index != fileCount; ++index) {
			file = &g_sharedResource.files[index];
			switch (file->mode) {
			case 0:
				break;
			case 1:
			case 2:
			case 3:
			case 6:
				if (file->fnode && file->fnode->fileSystem == fileSystem)
					_FreeFpCache(file);
				break;
			default:
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1428, "_disconnect_all_fd",
					"%s: index(%d), unexpected mode(%d)\n", "_disconnect_all_fd", index, file->mode);
				break;
			}
		}
	}
	fnodeCount = getInitializedFnodeCount(&g_sharedResource);
	if (fnodeCount) {
		for (index = 0; index != fnodeCount; ++index) {
			fnode = &g_sharedResource.fnodes[index];
			if (fnode->fileSystem != fileSystem)
				continue;
			cache = fnode->cluster_cache_array;
			fnode->cache_element_count = 0;
			fnode->cluster_cache_array = NULL;
			if (cache)
				freeCommonHeapMemory((int)fnode->fileSystem->drive, cache);
			if (fnode->refCount)
				fnode->fileSystem = NULL;
		}
	}
	result = sceKernelUnlockFastMutex(&g_sharedResourceMutex);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 119, "_exfat_unlock_shared_resource",
			"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_disconnect_drive", result);
	return 0;
failed:
	SCE_DBG_PRINTF_LEVEL(2, 0, 1686,
		"%s: UVFAT_Umount for fileSystem[%p] failed[%#x]\n",
		"sceExfatfsVopUmount", fileSystem, result);
	return result;
}

extern int _exfatfs_get_devinf(SceExfatfsFileSystem *fileSystem,
	void *information, SceSize informationSize, SceExfatfsSharedResource *sharedResource);

int exfatfs_get_drive_information(SceExfatfsDrive *drive, void *information,
	SceSize informationSize)
{
	int index;

	if (g_sharedResource.fileSystems[0].devname && drive == g_sharedResource.fileSystems[0].drive)
		index = 0;
	else if (g_sharedResource.fileSystems[1].devname && drive == g_sharedResource.fileSystems[1].drive)
		index = 1;
	else if (g_sharedResource.fileSystems[2].devname && drive == g_sharedResource.fileSystems[2].drive)
		index = 2;
	else if (g_sharedResource.fileSystems[3].devname && drive == g_sharedResource.fileSystems[3].drive)
		index = 3;
	else if (g_sharedResource.fileSystems[4].devname && drive == g_sharedResource.fileSystems[4].drive)
		index = 4;
	else if (g_sharedResource.fileSystems[5].devname && drive == g_sharedResource.fileSystems[5].drive)
		index = 5;
	else if (g_sharedResource.fileSystems[6].devname && drive == g_sharedResource.fileSystems[6].drive)
		index = 6;
	else if (g_sharedResource.fileSystems[7].devname && drive == g_sharedResource.fileSystems[7].drive)
		index = 7;
	else if (g_sharedResource.fileSystems[8].devname && drive == g_sharedResource.fileSystems[8].drive)
		index = 8;
	else if (g_sharedResource.fileSystems[9].devname && drive == g_sharedResource.fileSystems[9].drive)
		index = 9;
	else if (g_sharedResource.fileSystems[10].devname && drive == g_sharedResource.fileSystems[10].drive)
		index = 10;
	else if (g_sharedResource.fileSystems[11].devname && drive == g_sharedResource.fileSystems[11].drive)
		index = 11;
	else if (g_sharedResource.fileSystems[12].devname && drive == g_sharedResource.fileSystems[12].drive)
		index = 12;
	else if (g_sharedResource.fileSystems[13].devname && drive == g_sharedResource.fileSystems[13].drive)
		index = 13;
	else if (g_sharedResource.fileSystems[14].devname && drive == g_sharedResource.fileSystems[14].drive)
		index = 14;
	else {
		SCE_DBG_PRINTF_LEVEL(2, 15, 6257, "%s(): Cannot get mountpoint\n",
			"exfatfs_get_drive_information");
		return 0x80010002;
	}
	return _exfatfs_get_devinf(&g_sharedResource.fileSystems[index], information,
		informationSize, &g_sharedResource);
}

int exfatfs_get_file_size_limit(SceExfatfsDrive *drive, SceUInt64 *limit)
{
	SceUInt32 type;
	int index;

	if (g_sharedResource.fileSystems[0].devname && drive == g_sharedResource.fileSystems[0].drive)
		index = 0;
	else if (g_sharedResource.fileSystems[1].devname && drive == g_sharedResource.fileSystems[1].drive)
		index = 1;
	else if (g_sharedResource.fileSystems[2].devname && drive == g_sharedResource.fileSystems[2].drive)
		index = 2;
	else if (g_sharedResource.fileSystems[3].devname && drive == g_sharedResource.fileSystems[3].drive)
		index = 3;
	else if (g_sharedResource.fileSystems[4].devname && drive == g_sharedResource.fileSystems[4].drive)
		index = 4;
	else if (g_sharedResource.fileSystems[5].devname && drive == g_sharedResource.fileSystems[5].drive)
		index = 5;
	else if (g_sharedResource.fileSystems[6].devname && drive == g_sharedResource.fileSystems[6].drive)
		index = 6;
	else if (g_sharedResource.fileSystems[7].devname && drive == g_sharedResource.fileSystems[7].drive)
		index = 7;
	else if (g_sharedResource.fileSystems[8].devname && drive == g_sharedResource.fileSystems[8].drive)
		index = 8;
	else if (g_sharedResource.fileSystems[9].devname && drive == g_sharedResource.fileSystems[9].drive)
		index = 9;
	else if (g_sharedResource.fileSystems[10].devname && drive == g_sharedResource.fileSystems[10].drive)
		index = 10;
	else if (g_sharedResource.fileSystems[11].devname && drive == g_sharedResource.fileSystems[11].drive)
		index = 11;
	else if (g_sharedResource.fileSystems[12].devname && drive == g_sharedResource.fileSystems[12].drive)
		index = 12;
	else if (g_sharedResource.fileSystems[13].devname && drive == g_sharedResource.fileSystems[13].drive)
		index = 13;
	else if (g_sharedResource.fileSystems[14].devname && drive == g_sharedResource.fileSystems[14].drive)
		index = 14;
	else {
		SCE_DBG_PRINTF_LEVEL(2, 15, 6269, "%s(): Cannot get mountpoint\n",
			"exfatfs_get_file_size_limit");
		return 0x80010002;
	}
	type = g_sharedResource.fileSystems[index].fileSystemType - 1;
	*limit = type <= 1 ? g_fileSizeLimits[type] : 0;
	return 0;
}

extern int _exfatfs_set_fake_free(SceExfatfsFileSystem *fileSystem,
	SceExfatfsSharedResource *resource, SceUInt64 fakeFree,
	SceExfatfsSharedResource *sharedResource);

int exfatfs_set_fake_free(SceExfatfsDrive *drive, SceUInt64 fakeFree)
{
	int index;

	if (g_sharedResource.fileSystems[0].devname && drive == g_sharedResource.fileSystems[0].drive)
		index = 0;
	else if (g_sharedResource.fileSystems[1].devname && drive == g_sharedResource.fileSystems[1].drive)
		index = 1;
	else if (g_sharedResource.fileSystems[2].devname && drive == g_sharedResource.fileSystems[2].drive)
		index = 2;
	else if (g_sharedResource.fileSystems[3].devname && drive == g_sharedResource.fileSystems[3].drive)
		index = 3;
	else if (g_sharedResource.fileSystems[4].devname && drive == g_sharedResource.fileSystems[4].drive)
		index = 4;
	else if (g_sharedResource.fileSystems[5].devname && drive == g_sharedResource.fileSystems[5].drive)
		index = 5;
	else if (g_sharedResource.fileSystems[6].devname && drive == g_sharedResource.fileSystems[6].drive)
		index = 6;
	else if (g_sharedResource.fileSystems[7].devname && drive == g_sharedResource.fileSystems[7].drive)
		index = 7;
	else if (g_sharedResource.fileSystems[8].devname && drive == g_sharedResource.fileSystems[8].drive)
		index = 8;
	else if (g_sharedResource.fileSystems[9].devname && drive == g_sharedResource.fileSystems[9].drive)
		index = 9;
	else if (g_sharedResource.fileSystems[10].devname && drive == g_sharedResource.fileSystems[10].drive)
		index = 10;
	else if (g_sharedResource.fileSystems[11].devname && drive == g_sharedResource.fileSystems[11].drive)
		index = 11;
	else if (g_sharedResource.fileSystems[12].devname && drive == g_sharedResource.fileSystems[12].drive)
		index = 12;
	else if (g_sharedResource.fileSystems[13].devname && drive == g_sharedResource.fileSystems[13].drive)
		index = 13;
	else if (g_sharedResource.fileSystems[14].devname && drive == g_sharedResource.fileSystems[14].drive)
		index = 14;
	else {
		SCE_DBG_PRINTF_LEVEL(2, 15, 6283, "%s(): Cannot get mountpoint\n",
			"exfatfs_set_fake_free");
		return 0x80010013;
	}
	return _exfatfs_set_fake_free(&g_sharedResource.fileSystems[index],
		&g_sharedResource, fakeFree, &g_sharedResource);
}

int exfatfs_clear_fake_free(SceExfatfsDrive *drive)
{
	int index;

	if (g_sharedResource.fileSystems[0].devname && drive == g_sharedResource.fileSystems[0].drive)
		index = 0;
	else if (g_sharedResource.fileSystems[1].devname && drive == g_sharedResource.fileSystems[1].drive)
		index = 1;
	else if (g_sharedResource.fileSystems[2].devname && drive == g_sharedResource.fileSystems[2].drive)
		index = 2;
	else if (g_sharedResource.fileSystems[3].devname && drive == g_sharedResource.fileSystems[3].drive)
		index = 3;
	else if (g_sharedResource.fileSystems[4].devname && drive == g_sharedResource.fileSystems[4].drive)
		index = 4;
	else if (g_sharedResource.fileSystems[5].devname && drive == g_sharedResource.fileSystems[5].drive)
		index = 5;
	else if (g_sharedResource.fileSystems[6].devname && drive == g_sharedResource.fileSystems[6].drive)
		index = 6;
	else if (g_sharedResource.fileSystems[7].devname && drive == g_sharedResource.fileSystems[7].drive)
		index = 7;
	else if (g_sharedResource.fileSystems[8].devname && drive == g_sharedResource.fileSystems[8].drive)
		index = 8;
	else if (g_sharedResource.fileSystems[9].devname && drive == g_sharedResource.fileSystems[9].drive)
		index = 9;
	else if (g_sharedResource.fileSystems[10].devname && drive == g_sharedResource.fileSystems[10].drive)
		index = 10;
	else if (g_sharedResource.fileSystems[11].devname && drive == g_sharedResource.fileSystems[11].drive)
		index = 11;
	else if (g_sharedResource.fileSystems[12].devname && drive == g_sharedResource.fileSystems[12].drive)
		index = 12;
	else if (g_sharedResource.fileSystems[13].devname && drive == g_sharedResource.fileSystems[13].drive)
		index = 13;
	else {
		if (g_sharedResource.fileSystems[14].devname &&
			(index = 14, drive == g_sharedResource.fileSystems[14].drive))
			goto found;
		SCE_DBG_PRINTF_LEVEL(2, 15, 6297, "%s(): Cannot get mountpoint\n",
			"exfatfs_clear_fake_free");
		return 0x80010013;
	}
found:
	g_sharedResource.fileSystems[index].fakeFreeClusterCount = 0;
	return 0;
}
