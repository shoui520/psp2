#include "npdrm_package_internal.h"

namespace {

static SceUInt64 readPerfClock(void)
{
	if ((g_npDrmInstallPerf.enabled & 0xFFULL) == 0) {
		return 0ULL;
	}
	return (SceUInt64)sceKernelGetSystemTimeWide();
}

static void recordPerfDelta(SceUInt64 *accumulator)
{
	SceUInt64 now = readPerfClock();

	*accumulator += now - g_npDrmInstallPerf.lastStampUs;
	g_npDrmInstallPerf.lastStampUs = now;
}

static SceUInt32 swapBe32(SceUInt32 value)
{
	return ((value & 0x000000FFU) << 24)
		| ((value & 0x0000FF00U) << 8)
		| ((value & 0x00FF0000U) >> 8)
		| ((value & 0xFF000000U) >> 24);
}

static int isDirectoryPath(const char *path)
{
	SceIoStat statBuf;

	if (sceIoGetstat(path, &statBuf) == 0 && (statBuf.st_mode & 0xF000U) == 0x1000U) {
		return 0;
	}
	return SCE_NPDRM_PKG_ERROR_NOT_DIRECTORY;
}

static int ensureParentDirs(char *pathBuf, SceSize pathLen, SceMode mode)
{
	char *cursor = pathBuf;
	char *limit = pathBuf + pathLen;

	while (cursor < limit) {
		char *slash = (char *)sceClibMemchr(cursor, '/', (SceSize)(limit - cursor));

		if (slash == NULL) {
			break;
		}
		if (slash != pathBuf) {
			SceIoStat statBuf;
			int err;

			*slash = '\0';
			if (sceIoGetstat(pathBuf, &statBuf) != 0) {
				err = sceIoMkdir(pathBuf, mode);
			} else {
				err = isDirectoryPath(pathBuf);
			}
			*slash = '/';
			if (err != 0) {
				return err;
			}
		}
		cursor = slash + 1;
	}
	return 0;
}

static int buildRelativePath(char *pathBuf, SceSize *pathLenOut, const char *rootDir,
	SceSize rootDirLen, const char *relPath, SceSize relLen)
{
	if (rootDirLen + relLen > NPDRM_PKG_PATH_MAX) {
		return SCE_NPDRM_PKG_ERROR_PATH_TOO_LONG;
	}
	sceClibMemcpy_safe(pathBuf, rootDir, rootDirLen);
	sceClibMemcpy_safe(&pathBuf[rootDirLen], relPath, relLen);
	*pathLenOut = rootDirLen + relLen;
	return 0;
}

static int openIoFile(SceUID *fdPtr, const char *path, int flags, SceMode mode)
{
	SceUID fd = sceIoOpen(path, flags, mode);

	if (fd < 0) {
		*fdPtr = -1;
		return fd;
	}
	*fdPtr = fd;
	return 0;
}

static int closeIoFile(SceUID *fdPtr)
{
	if (*fdPtr < 0) {
		return 0;
	}
	int err = sceIoClose(*fdPtr);

	*fdPtr = -1;
	return err;
}

static int pwriteExact(SceUID fd, const void *buf, SceSize size, SceOff offset)
{
	int written = sceIoPwrite(fd, buf, size, offset);

	if (written < 0) {
		return written;
	}
	return ((SceSize)written == size) ? 0 : SCE_NPDRM_PKG_ERROR_SHORT_WRITE;
}

static int preadExact(SceUID fd, void *buf, SceSize size, SceOff offset)
{
	int got = sceIoPread(fd, buf, size, offset);

	if (got < 0) {
		return got;
	}
	return ((SceSize)got == size) ? 0 : SCE_NPDRM_PKG_ERROR_SHORT_READ;
}

static int writeStatFileWithIo(NpDrmStatFile *statFile, NpDrmIoHandle *ioHandle,
	SceSize rootDirLen, SceSize *pathLenOut, const NpDrmPkgMeta *pkgMeta)
{
	int openFlags = ((pkgMeta->openFlags & NPDRM_OPEN_FLAG_DIRECT_IO) != 0) ? 67074 : 1538;
	int err = buildRelativePath(ioHandle->pathBuf, pathLenOut, ioHandle->rootDir,
		rootDirLen, "sce_sys/package/stat.bin", 25U);
	if (err != 0) {
		return err;
	}
	err = openIoFile(ioHandle->fdPtr, ioHandle->pathBuf, openFlags, 6);
	if (err != 0) {
		return err;
	}

	SceSize statSize = statFile->statSize;
	SceUInt8 *statBody = (SceUInt8 *)&statFile->magic;

	npDrmStatComputeCmac(statFile->cmac, statBody, statSize - 16U, pkgMeta->keySlotId);
	npDrmStatEncryptCbc(statBody, NPDRM_PKG_STAT_SIZE_EXT, pkgMeta->keySlotId);
	err = pwriteExact(*ioHandle->fdPtr, statFile, statSize, 0);
	npDrmStatDecryptCbc(statBody, NPDRM_PKG_STAT_SIZE_EXT, pkgMeta->keySlotId);

	if (err != 0) {
		closeIoFile(ioHandle->fdPtr);
		return err;
	}
	return closeIoFile(ioHandle->fdPtr);
}

static int isHwAccelEnabled(const NpDrmPkgMeta *pkgMeta)
{
	if (((pkgMeta->openFlags & NPDRM_OPEN_FLAG_DIRECT_IO) != 0
		|| (pkgMeta->metaFlags & NPDRM_META_FLAG_HW_CRYPTO) != 0)
		&& g_npDrmHwAccelActive != 0) {
		return 1;
	}
	return 0;
}

static int updateSha1WithAccel(SceSha1Context *sha1Ctx, SceUInt64 *hashedBytes,
	const SceUInt8 *src, SceSize size, const NpDrmPkgMeta *pkgMeta)
{
	if (!isHwAccelEnabled(pkgMeta)) {
		sceSha1BlockUpdate(sha1Ctx, src, size);
		*hashedBytes += size;
		return 0;
	}

	SceSize remaining = size;
	const SceUInt8 *cursor = src;

	if ((*hashedBytes & 63ULL) != 0) {
		SceSize lead = (SceSize)(64ULL - (*hashedBytes & 63ULL));

		if (lead > remaining) {
			lead = remaining;
		}
		sceSha1BlockUpdate(sha1Ctx, cursor, lead);
		*hashedBytes += lead;
		cursor += lead;
		remaining -= lead;
	}

	if (remaining >= 64U) {
		SceSize alignedLen = remaining & ~63U;
		NpDrmTransformState hwState;

		sceClibMemset(&hwState, 0, sizeof(hwState));
		if (sha1Ctx->usRemains == 0) {
			for (SceSize idx = 0; idx < 5U; ++idx) {
				hwState.hashWords[idx] = swapBe32(sha1Ctx->h[idx]);
			}
			hwState.bitCount = sha1Ctx->ullTotalLen * 8ULL;
		}
		int err = sceNpDrmPackageTransform((void *)cursor, alignedLen, &hwState, NPDRM_TRANSFORM_SHA1);
		if (err < 0) {
			return err;
		}
		sceClibMemset(sha1Ctx, 0, sizeof(*sha1Ctx));
		for (SceSize idx = 0; idx < 5U; ++idx) {
			sha1Ctx->h[idx] = swapBe32(hwState.hashWords[idx]);
		}
		sha1Ctx->ullTotalLen = hwState.bitCount >> 3;
		*hashedBytes += alignedLen;
		cursor += alignedLen;
		remaining -= alignedLen;
	}

	if (remaining != 0) {
		sceSha1BlockUpdate(sha1Ctx, cursor, remaining);
		*hashedBytes += remaining;
	}
	return 0;
}

static int updateSha256Segment(SceSha256Context *sha256Ctx, SceUInt64 *hashedBytes,
	const SceUInt8 *src, SceSize size, const NpDrmPkgMeta *pkgMeta)
{
	int doHash = ((pkgMeta->openFlags & NPDRM_OPEN_FLAG_SKIP_BODY_HASH) == 0
		|| (pkgMeta->metaFlags & NPDRM_META_FLAG_FINALIZED) != 0);

	if (!doHash || !isHwAccelEnabled(pkgMeta)) {
		if (doHash) {
			sceSha256BlockUpdate(sha256Ctx, src, size);
		}
		*hashedBytes += size;
		return 0;
	}

	SceSize remaining = size;
	const SceUInt8 *cursor = src;

	if ((*hashedBytes & 63ULL) != 0) {
		SceSize lead = (SceSize)(64ULL - (*hashedBytes & 63ULL));

		if (lead > remaining) {
			lead = remaining;
		}
		sceSha256BlockUpdate(sha256Ctx, cursor, lead);
		*hashedBytes += lead;
		cursor += lead;
		remaining -= lead;
	}

	if (remaining >= 64U) {
		SceSize alignedLen = remaining & ~63U;
		NpDrmTransformState hwState;

		sceClibMemset(&hwState, 0, sizeof(hwState));
		if (sha256Ctx->usRemains == 0) {
			for (SceSize idx = 0; idx < 8U; ++idx) {
				hwState.hashWords[idx] = swapBe32(sha256Ctx->h[idx]);
			}
			hwState.bitCount = sha256Ctx->ullTotalLen * 8ULL;
		}
		int err = sceNpDrmPackageTransform((void *)cursor, alignedLen, &hwState, NPDRM_TRANSFORM_SHA256);
		if (err < 0) {
			return err;
		}
		sceClibMemset(sha256Ctx, 0, sizeof(*sha256Ctx));
		for (SceSize idx = 0; idx < 8U; ++idx) {
			sha256Ctx->h[idx] = swapBe32(hwState.hashWords[idx]);
		}
		sha256Ctx->ullTotalLen = hwState.bitCount >> 3;
		*hashedBytes += alignedLen;
		cursor += alignedLen;
		remaining -= alignedLen;
	}

	if (remaining != 0) {
		if (doHash) {
			sceSha256BlockUpdate(sha256Ctx, cursor, remaining);
		}
		*hashedBytes += remaining;
	}
	return 0;
}

static int compareBytesConst(const SceUInt8 *lhs, const SceUInt8 *rhs, SceSize len)
{
	int diff = 0;

	for (SceSize idx = 0; idx < len; ++idx) {
		diff += (SceUInt8)(lhs[idx] - rhs[idx]);
	}
	return diff;
}

static int writeTailSegment(NpDrmInstallContext *ctx, NpDrmWriteCursor *cursor,
	SceUInt64 segmentLimit)
{
	if (cursor->offset >= segmentLimit || cursor->length == 0) {
		return 0;
	}
	SceUInt64 remainInSeg = segmentLimit - cursor->offset;
	SceSize toWrite = (remainInSeg >= (SceUInt64)cursor->length)
		? cursor->length : (SceSize)remainInSeg;

	NpDrmHashState stagedHash = ctx->statFile.hashState;
	sceSha1BlockUpdate(&stagedHash.pkgSha1Ctx, cursor->data, toWrite);
	stagedHash.pkgHashedBytes += toWrite;
	sceSha256BlockUpdate(&stagedHash.fileSha256Ctx, cursor->data, toWrite);
	stagedHash.fileHashedBytes += toWrite;

	int err = pwriteExact(ctx->activeFd, cursor->data, toWrite,
		(SceOff)(cursor->offset - ctx->pkgMeta.tailOffset));
	if (err != 0) {
		return err;
	}
	cursor->data += toWrite;
	cursor->length -= toWrite;
	cursor->offset += toWrite;
	ctx->statFile.hashState = stagedHash;
	return 0;
}

} /* namespace */

extern "C" {

const NpDrmFileSpec g_npDrmHeadBinSpec = { "sce_sys/package/head.bin", 25U, 32768U };
const NpDrmFileSpec g_npDrmInstBinSpec = { "sce_sys/package/inst.bin", 25U, 256U };
const NpDrmFileSpec g_npDrmTempBinSpec = { "sce_sys/package/temp.bin", 25U, 512U };
const NpDrmFileSpec g_npDrmWorkBinSpec = { "sce_sys/package/work.bin", 25U, 512U };

int npDrmHashChunkSha256(NpDrmChunkSlot *chunk, SceSha256Context *sha256Ctx, SceUInt64 *hashedOut)
{
	int err = 0;

	if ((chunk->chunkFlags & NPDRM_CHUNK_FLAG_EBOOT_PBP) != 0) {
		NpDrmStatFile *statFile = chunk->pipeline->statFilePtr;

		if (chunk->pkgOffset == 0ULL && chunk->length >= 40U) {
			const SceUInt32 *pbpHeader = (const SceUInt32 *)chunk->buffer;

			/* Check "\0PBP" magic and version 0x10000 / 0x10001 */
			if (pbpHeader[0] == 0x50425000U
				&& (pbpHeader[1] == 0x00010001U || pbpHeader[1] == 0x00010000U)) {
				SceUInt32 psarOffset = pbpHeader[9];

				if (chunk->itemDataSize >= (SceUInt64)psarOffset) {
					SceUInt64 partialLimit = (SceUInt64)psarOffset + 0x1C0000ULL;

					if (chunk->itemDataSize < partialLimit) {
						partialLimit = chunk->itemDataSize;
					}
					statFile->ebootDigestEndOffset = partialLimit;
				}
			}
		}

		SceUInt64 digestEnd = statFile->ebootDigestEndOffset;
		if (digestEnd != 0ULL && chunk->pkgOffset < digestEnd
			&& digestEnd <= chunk->pkgOffset + (SceUInt64)chunk->length) {
			SceSize firstLen = (SceSize)(digestEnd - chunk->pkgOffset);

			if (firstLen > chunk->length) {
				firstLen = chunk->length;
			}
			err = updateSha256Segment(sha256Ctx, hashedOut, chunk->buffer, firstLen, chunk->pkgMeta);
			recordPerfDelta(&g_npDrmInstallPerf.sha256Us);
			if (err != 0) {
				return err;
			}
			SceSha256Context snapshotCtx = *sha256Ctx;

			sceSha256BlockResult(&snapshotCtx, statFile->ebootPartialDigest);
			statFile->version = 2U;
			statFile->statSize = NPDRM_PKG_STAT_SIZE_EXT;

			SceSize secondLen = chunk->length - firstLen;
			err = updateSha256Segment(sha256Ctx, hashedOut, &chunk->buffer[firstLen],
				secondLen, chunk->pkgMeta);
			recordPerfDelta(&g_npDrmInstallPerf.sha256Us);
			return err;
		}
	}

	err = updateSha256Segment(sha256Ctx, hashedOut, chunk->buffer, chunk->length, chunk->pkgMeta);
	recordPerfDelta(&g_npDrmInstallPerf.sha256Us);
	return err;
}

int npDrmProcessPkgHeader(NpDrmInstallContext *ctx, const void *headerBuf, SceSize headerSize,
	SceUInt8 isResume)
{
	int err = npDrmParsePkgMeta(&ctx->pkgMeta, headerBuf, headerSize, (SceUInt32)ctx->slotIndex);
	if (err != 0) {
		return err;
	}

	if ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_SKIP_BODY_HASH) != 0
		&& (ctx->pkgMeta.metaFlags & NPDRM_META_FLAG_FINALIZED) == 0) {
		ctx->statFile.statFlags |= 1U;
	}

	if (isResume == 0) {
		SceUInt32 headSize = ctx->pkgMeta.headSize;

		ctx->statFile.hashState.pkgTotalBytes = ctx->pkgMeta.tailOffset + ctx->pkgMeta.tailSize;
		ctx->statFile.hashState.bodyTotalBytes = (SceUInt64)(ctx->pkgMeta.pkgInfo.itemCount * 32U);
		if (headerSize < headSize) {
			return SCE_NPDRM_PKG_ERROR_HEAD_TOO_LARGE;
		}
		sceSha256Digest(headerBuf, headSize, ctx->statFile.headSha256Digest);
		ctx->statFile.headSize = headSize;
	}

	SceUInt32 contentType = ctx->pkgMeta.pkgInfo.contentType;
	int needsLicenseCheck = (contentType == 21U || contentType == 22U || contentType == 31U
		|| contentType == 6U || contentType == 7U || contentType == 14U
		|| contentType == 15U || contentType == 16U || contentType == 17U);

	if (!needsLicenseCheck || (ctx->pkgMeta.pkgInfo.packageType & 0x10U) != 0) {
		return 0;
	}
	if (vshSysconIsIduMode() != 0) {
		return 0;
	}
	if (ctx->slotIndex != 1 && (SceInt32)ctx->pkgMeta.pkgInfo.packageType >= 0) {
		return 0;
	}
	if ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_SKIP_WORK_BIN) != 0
		&& (SceInt32)ctx->pkgMeta.pkgInfo.packageType >= 0) {
		return 0;
	}
	if ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_IGNORE_LICENSE) != 0) {
		return 0;
	}

	int isCertDrmType = (ctx->pkgMeta.pkgInfo.drmType == 13U || ctx->pkgMeta.pkgInfo.drmType == 3U);
	if (ctx->slotIndex == 2 && isCertDrmType) {
		return 0;
	}

	SceUInt8 workBuf[512];
	int readRet;

	sceClibMemset(workBuf, 0, sizeof(workBuf));
	err = buildRelativePath(ctx->workPath, &ctx->workPathLen, ctx->rootDir, ctx->rootDirLen,
		"sce_sys/package/work.bin", 25U);
	if (err == 0) {
		err = openIoFile(&ctx->activeFd, ctx->workPath, 1, 6);
		if (err == 0) {
			SceOff fileLen = sceIoLseek(ctx->activeFd, 0, SCE_SEEK_END);

			if (fileLen < 0) {
				readRet = (int)fileLen;
			} else {
				SceSize toRead = (fileLen < (SceOff)sizeof(workBuf))
					? (SceSize)fileLen : sizeof(workBuf);
				int gotErr = preadExact(ctx->activeFd, workBuf, toRead, 0);

				if (gotErr != 0) {
					closeIoFile(&ctx->activeFd);
					readRet = gotErr;
				} else {
					int closeErr = closeIoFile(&ctx->activeFd);
					readRet = (closeErr != 0) ? closeErr : (int)toRead;
				}
			}
		} else {
			readRet = err;
		}
	} else {
		readRet = err;
	}

	int verifyResult = 0;
	if (readRet == SCE_NPDRM_PKG_IO_ENOENT) {
		verifyResult = SCE_NPDRM_PKG_ERROR_NO_WORK_BIN;
	} else if (readRet < 0) {
		verifyResult = readRet;
	} else {
		if (compareBytesConst((const SceUInt8 *)ctx->pkgMeta.pkgInfo.contentId, &workBuf[16], 48U) != 0) {
			verifyResult = SCE_NPDRM_PKG_ERROR_CONTENT_ID_MISMATCH;
		} else {
			static const SceUInt8 kCertMagic[8] = { 0x7F, 'C', 'E', 'R', 0x00, 0x00, 0x00, 0x06 };

			if (compareBytesConst(workBuf, kCertMagic, sizeof(kCertMagic)) != 0) {
				int rifRet = sceNpDrmGetRifInfo(workBuf, sizeof(workBuf), 1,
					ctx->scratch.rifContentId, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

				if (rifRet == SCE_NPDRM_PKG_ERROR_RIF_EXPIRED) {
					verifyResult = SCE_NPDRM_PKG_ERROR_RIF_EXPIRED;
				} else if (compareBytesConst((const SceUInt8 *)ctx->pkgMeta.pkgInfo.contentId,
					(const SceUInt8 *)ctx->scratch.rifContentId, 48U) != 0) {
					verifyResult = SCE_NPDRM_PKG_ERROR_CONTENT_ID_MISMATCH;
				}
			} else if (isCertDrmType) {
				int decRet = npDrmDecryptWorkBin(workBuf, sizeof(workBuf));
				if (decRet != 0) {
					verifyResult = decRet;
				}
			} else {
				verifyResult = SCE_NPDRM_PKG_ERROR_INVALID_CERT;
			}
		}
	}

	sceClibMemset(workBuf, 0, sizeof(workBuf));
	return verifyResult;
}

int npDrmFinalizeHeadBin(NpDrmIoHandle *ioHandle, NpDrmHashState *hashState,
	NpDrmStatFile *statFile, const NpDrmPkgMeta *pkgMeta)
{
	int err = closeIoFile(ioHandle->fdPtr);
	if (err != 0) {
		return err;
	}

	sceSha256BlockResult(&hashState->fileSha256Ctx, statFile->headFileSha256Digest);
	sceSha256BlockInit(&hashState->fileSha256Ctx);
	hashState->fileHashedBytes = 0ULL;
	hashState->fileTotalBytes = 0ULL;

	SceSize *rootLenPtr = (SceSize *)&ioHandle->rootDir[NPDRM_PKG_ROOT_MAX];
	SceSize *pathLenPtr = (SceSize *)&ioHandle->pathBuf[NPDRM_PKG_PATH_MAX];

	if ((pkgMeta->metaFlags & NPDRM_META_FLAG_USRDIR) != 0) {
		err = buildRelativePath(ioHandle->pathBuf, pathLenPtr, ioHandle->rootDir,
			*rootLenPtr, "USRDIR/CONTENT/", 15U);
		if (err != 0) {
			return err;
		}
		err = ensureParentDirs(ioHandle->pathBuf, *pathLenPtr, pkgMeta->openMode);
		if (err != 0) {
			return err;
		}
	}

	if ((pkgMeta->openFlags & NPDRM_OPEN_FLAG_SKIP_BODY_HASH) == 0
		|| (pkgMeta->metaFlags & NPDRM_META_FLAG_FINALIZED) != 0) {
		err = writeStatFileWithIo(statFile, ioHandle, *rootLenPtr, pathLenPtr, pkgMeta);
		if (err != 0) {
			return err;
		}
	}

	int openFlags = ((pkgMeta->openFlags & NPDRM_OPEN_FLAG_DIRECT_IO) != 0) ? 67074 : 1538;
	err = buildRelativePath(ioHandle->pathBuf, pathLenPtr, ioHandle->rootDir,
		*rootLenPtr, "sce_sys/package/body.bin", 25U);
	if (err != 0) {
		return err;
	}
	err = openIoFile(ioHandle->fdPtr, ioHandle->pathBuf, openFlags, 6);
	if (err != 0) {
		return err;
	}
	if ((pkgMeta->openFlags & NPDRM_OPEN_FLAG_PREALLOC) != 0
		|| ((pkgMeta->openFlags & NPDRM_OPEN_FLAG_SKIP_BODY_HASH) != 0
			&& (pkgMeta->metaFlags & NPDRM_META_FLAG_FINALIZED) == 0)) {
		SceIoStat chstatBuf;

		chstatBuf.st_size = (SceOff)(pkgMeta->pkgInfo.itemCount * 32U);
		err = sceIoChstatByFd(*ioHandle->fdPtr, &chstatBuf, 4U);
		if (err != 0) {
			return err;
		}
	}
	return closeIoFile(ioHandle->fdPtr);
}

int npDrmWriteHeadSection(NpDrmInstallContext *ctx, NpDrmWriteCursor *cursor)
{
	NpDrmIoHandle ioHandle;

	ioHandle.fdPtr = &ctx->activeFd;
	ioHandle.pathBuf = ctx->workPath;
	ioHandle.rootDir = ctx->rootDir;

	SceUInt32 headSize = ctx->pkgMeta.headSize;
	if (cursor->offset < (SceUInt64)headSize && ctx->activeFd < 0) {
		int err = buildRelativePath(ctx->workPath, &ctx->workPathLen, ctx->rootDir,
			ctx->rootDirLen, "sce_sys/package/head.bin", 25U);
		if (err != 0) {
			return err;
		}
		err = ensureParentDirs(ctx->workPath, ctx->workPathLen, ctx->pkgMeta.openMode);
		if (err != 0) {
			return err;
		}
		int openFlags = ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_DIRECT_IO) != 0) ? 67074 : 1538;

		err = openIoFile(&ctx->activeFd, ctx->workPath, openFlags, 6);
		if (err != 0) {
			return err;
		}
		if (ctx->statFile.hashState.fileTotalBytes == 0ULL) {
			sceSha256BlockInit(&ctx->statFile.hashState.fileSha256Ctx);
			ctx->statFile.hashState.fileTotalBytes = (SceUInt64)headSize;
			ctx->statFile.hashState.fileHashedBytes = 0ULL;
		}
		if ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_PREALLOC) != 0) {
			SceIoStat chstatBuf;

			chstatBuf.st_size = (SceOff)headSize;
			err = sceIoChstatByFd(ctx->activeFd, &chstatBuf, 4U);
			if (err != 0) {
				return err;
			}
		}
	}

	if (cursor->offset < (SceUInt64)headSize) {
		SceUInt64 remainHead = (SceUInt64)headSize - cursor->offset;
		SceSize toWrite = (remainHead >= (SceUInt64)cursor->length)
			? cursor->length : (SceSize)remainHead;
		NpDrmHashState stagedHash = ctx->statFile.hashState;

		sceSha1BlockUpdate(&stagedHash.pkgSha1Ctx, cursor->data, toWrite);
		stagedHash.pkgHashedBytes += toWrite;
		sceSha256BlockUpdate(&stagedHash.fileSha256Ctx, cursor->data, toWrite);
		stagedHash.fileHashedBytes += toWrite;

		int err = pwriteExact(ctx->activeFd, cursor->data, toWrite, (SceOff)cursor->offset);
		if (err != 0) {
			return err;
		}
		cursor->data += toWrite;
		cursor->length -= toWrite;
		cursor->offset += toWrite;
		ctx->statFile.hashState = stagedHash;
	}

	if (cursor->offset == (SceUInt64)headSize) {
		return npDrmFinalizeHeadBin(&ioHandle, &ctx->statFile.hashState,
			&ctx->statFile, &ctx->pkgMeta);
	}
	return 0;
}

int npDrmReadItemMetaFromHead(NpDrmItemMeta *outItem, char *outNameBuf, SceUInt32 itemIndex,
	NpDrmIoHandle *tempIo, NpDrmIoHandle *headIo, const NpDrmPkgMeta *pkgMeta)
{
	int keepOpen = (pkgMeta->openFlags & NPDRM_OPEN_FLAG_KEEP_FDS_OPEN) != 0;
	NpDrmIoHandle *io = keepOpen ? headIo : tempIo;
	int openFlags = ((pkgMeta->openFlags & NPDRM_OPEN_FLAG_DIRECT_IO) != 0) ? 65537 : 1;
	int err = 0;

	if (!keepOpen || *io->fdPtr < 0) {
		SceSize *rootLenPtr = (SceSize *)&io->rootDir[NPDRM_PKG_ROOT_MAX];
		SceSize *pathLenPtr = (SceSize *)&io->pathBuf[NPDRM_PKG_PATH_MAX];

		err = buildRelativePath(io->pathBuf, pathLenPtr, io->rootDir,
			*rootLenPtr, "sce_sys/package/head.bin", 25U);
		if (err != 0) {
			return err;
		}
		err = openIoFile(io->fdPtr, io->pathBuf, openFlags, 6);
		if (err != 0) {
			return err;
		}
	}

	SceUInt8 rawEntry[32];
	sceClibMemset(outItem, 0, sizeof(*outItem));
	outItem->itemIndex = -1;

	SceUInt32 entryOffset = npDrmGetItemEntryOffset(itemIndex, pkgMeta);
	err = preadExact(*io->fdPtr, rawEntry, sizeof(rawEntry), (SceOff)entryOffset);
	if (err == 0) {
		npDrmDecryptPkgRegion(rawEntry, sizeof(rawEntry), (SceUInt64)entryOffset,
			pkgMeta, (SceUInt8)isHwAccelEnabled(pkgMeta));
		err = npDrmDecodeItemMeta(outItem, itemIndex, rawEntry, sizeof(rawEntry), pkgMeta);
	}

	if (err == 0 && outItem->isExtractable != 0) {
		if (outItem->nameSize >= 256U) {
			err = SCE_NPDRM_PKG_ERROR_NAME_TOO_LONG;
		} else {
			err = preadExact(*io->fdPtr, outNameBuf, outItem->nameSize, (SceOff)outItem->nameOffset);
			if (err == 0) {
				npDrmDecryptPkgRegion(outNameBuf, outItem->nameSize, (SceUInt64)outItem->nameOffset,
					pkgMeta, (SceUInt8)isHwAccelEnabled(pkgMeta));
				outNameBuf[outItem->nameSize] = '\0';
				*(SceSize *)&outNameBuf[256] = outItem->nameSize + 1U;
			}
		}
	}

	if (!keepOpen) {
		closeIoFile(io->fdPtr);
	}
	return err;
}

int npDrmOpenNextItem(NpDrmIoHandle *ioHandle, NpDrmHashState *hashState, NpDrmItemMeta *outItem,
	char *outNameBuf, const char *prefixFilter, SceSize prefixFilterLen,
	NpDrmIoHandle *headIo, const NpDrmPkgMeta *pkgMeta)
{
	int err = npDrmReadItemMetaFromHead(outItem, outNameBuf, hashState->currentItemIndex,
		ioHandle, headIo, pkgMeta);
	recordPerfDelta(&g_npDrmInstallPerf.itemOpenUs);
	if (err != 0) {
		return err;
	}
	if (outItem->isExtractable == 0) {
		return 0;
	}

	if ((pkgMeta->metaFlags & NPDRM_META_FLAG_LEGACY_PSP) != 0) {
		SceUInt32 contentType = pkgMeta->pkgInfo.contentType;

		if (contentType == 6U || contentType == 7U || contentType == 14U
			|| contentType == 15U || contentType == 16U) {
			if (sceClibMemcmp(outNameBuf, "USRDIR/CONTENT/EBOOT.PBP", 25U) == 0) {
				outItem->isEbootPbp = 1U;
			}
		}
	}

	if (hashState->fileTotalBytes == 0ULL) {
		if ((pkgMeta->openFlags & NPDRM_OPEN_FLAG_SKIP_BODY_HASH) == 0
			|| (pkgMeta->metaFlags & NPDRM_META_FLAG_FINALIZED) != 0) {
			sceSha256BlockInit(&hashState->fileSha256Ctx);
		}
		hashState->fileHashedBytes = 0ULL;
		hashState->fileTotalBytes = outItem->dataSize;
	}
	recordPerfDelta(&g_npDrmInstallPerf.sha256Us);

	if (sceClibMemcmp(outNameBuf, prefixFilter, prefixFilterLen) != 0) {
		err = SCE_NPDRM_PKG_ERROR_PREFIX_MISMATCH;
	} else {
		SceSize nameLenWithNul = *(SceSize *)&outNameBuf[256];
		SceSize trimmedLen = nameLenWithNul - prefixFilterLen;
		SceSize *rootLenPtr = (SceSize *)&ioHandle->rootDir[NPDRM_PKG_ROOT_MAX];
		SceSize *pathLenPtr = (SceSize *)&ioHandle->pathBuf[NPDRM_PKG_PATH_MAX];

		err = buildRelativePath(ioHandle->pathBuf, pathLenPtr, ioHandle->rootDir,
			*rootLenPtr, &outNameBuf[prefixFilterLen], trimmedLen);
		if (err == 0) {
			if (outItem->isDirectory != 0) {
				SceIoStat statBuf;

				if (sceIoGetstat(ioHandle->pathBuf, &statBuf) != 0) {
					err = sceIoMkdir(ioHandle->pathBuf, pkgMeta->openMode);
				} else {
					err = isDirectoryPath(ioHandle->pathBuf);
				}
			} else {
				int openFlags = (hashState->fileHashedBytes != 0ULL) ? 2 : 1538;

				if ((pkgMeta->openFlags & NPDRM_OPEN_FLAG_DIRECT_IO) != 0) {
					openFlags |= 0x10000;
				}
				err = openIoFile(ioHandle->fdPtr, ioHandle->pathBuf, openFlags, pkgMeta->openMode);
			}
		}
	}
	recordPerfDelta(&g_npDrmInstallPerf.fileCreateUs);
	if (err != 0) {
		return err;
	}

	if ((pkgMeta->openFlags & NPDRM_OPEN_FLAG_PREALLOC) != 0 && outItem->dataSize > 0x8000ULL) {
		SceIoStat chstatBuf;

		chstatBuf.st_size = (SceOff)outItem->dataSize;
		err = sceIoChstatByFd(*ioHandle->fdPtr, &chstatBuf, 4U);
		recordPerfDelta(&g_npDrmInstallPerf.fileWriteUs);
		if (err != 0) {
			return err;
		}
	}
	return 0;
}

static int joinMaskedSlots(NpDrmPipelineState *pipeline, SceUInt32 relativeMask)
{
	SceUInt32 count = pipeline->chunkCount;
	SceUInt32 activeMask = 0;

	for (SceUInt32 rel = 0; rel < count; ++rel) {
		if ((relativeMask & (1U << rel)) != 0) {
			SceUInt32 slotIdx = (pipeline->currentChunkIndex + count + rel) % count;

			if ((pipeline->chunks[slotIdx].chunkFlags & NPDRM_CHUNK_FLAG_ACTIVE) != 0) {
				activeMask |= (1U << slotIdx);
			}
		}
	}
	if (activeMask == 0) {
		return 0;
	}
	int err = pipeline->joinCb(pipeline, activeMask, pipeline->cbUserData);
	if (err != 0) {
		return err;
	}
	for (SceUInt32 idx = 0; idx < pipeline->chunkCount; ++idx) {
		if ((activeMask & (1U << idx)) != 0) {
			pipeline->chunks[idx].chunkFlags &= ~NPDRM_CHUNK_FLAG_ACTIVE;
		}
	}
	return 0;
}

static int forkMaskedSlots(NpDrmPipelineState *pipeline, SceUInt32 relativeMask)
{
	SceUInt32 count = pipeline->chunkCount;
	SceUInt32 dispatchMask = 0;

	for (SceUInt32 rel = 0; rel < count; ++rel) {
		if ((relativeMask & (1U << rel)) != 0) {
			SceUInt32 slotIdx = (pipeline->currentChunkIndex + count + rel) % count;

			if (pipeline->chunks[slotIdx].stage != NPDRM_STAGE_IDLE) {
				pipeline->chunks[slotIdx].chunkFlags |= NPDRM_CHUNK_FLAG_ACTIVE;
				dispatchMask |= (1U << slotIdx);
			}
		}
	}
	if (dispatchMask == 0) {
		return 0;
	}
	return pipeline->forkCb(pipeline, dispatchMask, pipeline->cbUserData);
}

static void retireCompletedSlot(NpDrmPipelineState *pipeline, NpDrmHashState *hashState)
{
	SceUInt32 curIdx = pipeline->currentChunkIndex;
	NpDrmChunkSlot *slot = &pipeline->chunks[curIdx];

	if (slot->stage == NPDRM_STAGE_DONE) {
		hashState->pkgSha1Ctx = slot->sha1Ctx;
		hashState->pkgHashedBytes = slot->sha1HashedBytes;
		hashState->pkgTotalBytes = slot->sha1TotalBytes;
		hashState->fileSha256Ctx = slot->sha256Ctx;
		hashState->fileHashedBytes = slot->sha256HashedBytes;
		hashState->fileTotalBytes = slot->sha256TotalBytes;
		slot->stage = NPDRM_STAGE_IDLE;
	}
}

int npDrmAdvancePipeline(NpDrmPipelineState *pipeline, NpDrmHashState *hashState)
{
	SceUInt32 count = pipeline->chunkCount;
	int err;

	if (count >= 5U) {
		err = joinMaskedSlots(pipeline, pipeline->joinMask);
		if (err != 0) {
			return err;
		}
		pipeline->currentChunkIndex = (pipeline->currentChunkIndex + 1U) % pipeline->chunkCount;
		retireCompletedSlot(pipeline, hashState);
		return forkMaskedSlots(pipeline, pipeline->forkMask);
	}

	err = joinMaskedSlots(pipeline, 0x0FU);
	if (err != 0) {
		return err;
	}
	pipeline->currentChunkIndex = (pipeline->currentChunkIndex + 1U) % pipeline->chunkCount;
	retireCompletedSlot(pipeline, hashState);

	err = forkMaskedSlots(pipeline, 0x0FU);
	if (err != 0) {
		return err;
	}

	SceUInt32 tailSteps = 5U - pipeline->chunkCount;
	SceUInt32 tailMask = 1U << (pipeline->chunkCount - 1U);
	for (SceUInt32 step = 0; step < tailSteps; ++step) {
		err = joinMaskedSlots(pipeline, tailMask);
		if (err != 0) {
			return err;
		}
		err = forkMaskedSlots(pipeline, tailMask);
		if (err != 0) {
			return err;
		}
	}
	return 0;
}

int npDrmProcessItemData(NpDrmPipelineState *pipeline, NpDrmWriteCursor *cursor,
	const NpDrmItemMeta *itemMeta, const NpDrmPkgMeta *pkgMeta,
	NpDrmIoHandle *ioHandle, NpDrmHashState *hashState)
{
	if (pipeline->chunks != NULL) {
		const SceUInt8 *src = cursor->data;
		SceUInt32 remaining = cursor->length;
		SceUInt64 curOffset = cursor->offset;

		while (remaining != 0) {
			NpDrmChunkSlot *slot = &pipeline->chunks[pipeline->currentChunkIndex];

			if (slot->length == 0) {
				SceUInt64 boundary = (curOffset < slot->itemDataStart)
					? slot->itemDataStart : slot->itemAlignedEnd;
				SceUInt32 cap = slot->capacity;

				slot->pkgOffset = curOffset;
				if (boundary - curOffset < (SceUInt64)cap) {
					cap = (SceUInt32)(boundary - curOffset);
				}
				slot->targetFill = cap;
			}

			SceSize space = slot->targetFill - slot->length;
			SceSize toCopy = (space >= remaining) ? remaining : space;

			sceClibMemcpy_safe(&slot->buffer[slot->length], src, toCopy);
			src += toCopy;
			curOffset += toCopy;
			remaining -= toCopy;
			slot->length += toCopy;
			slot->stage = NPDRM_STAGE_SHA1;

			if (slot->length == slot->targetFill) {
				int err = npDrmAdvancePipeline(pipeline, hashState);
				if (err != 0) {
					return err;
				}
			}
		}
		return 0;
	}

	NpDrmChunkSlot localSlot;
	sceClibMemset(&localSlot, 0, sizeof(localSlot));
	localSlot.buffer = (SceUInt8 *)cursor->data;
	localSlot.length = cursor->length;
	localSlot.pkgOffset = cursor->offset;
	localSlot.itemDataStart = (itemMeta->isExtractable != 0) ? itemMeta->dataStart : itemMeta->alignedEnd;
	localSlot.itemDataSize = itemMeta->dataSize;
	localSlot.itemAlignedEnd = itemMeta->alignedEnd;
	localSlot.pipeline = pipeline;
	localSlot.pkgMeta = (NpDrmPkgMeta *)pkgMeta;
	if (itemMeta->isEbootPbp != 0 && pipeline != NULL && pipeline->statFilePtr != NULL) {
		localSlot.chunkFlags = NPDRM_CHUNK_FLAG_EBOOT_PBP;
		pipeline->statFilePtr->version = 2U;
		pipeline->statFilePtr->statSize = NPDRM_PKG_STAT_SIZE_EXT;
		pipeline->statFilePtr->ebootItemIndex = (SceUInt32)itemMeta->itemIndex;
	}
	localSlot.sha1Ctx = hashState->pkgSha1Ctx;
	localSlot.sha1HashedBytes = hashState->pkgHashedBytes;
	localSlot.sha1TotalBytes = hashState->pkgTotalBytes;

	int err = updateSha1WithAccel(&localSlot.sha1Ctx, &localSlot.sha1HashedBytes,
		localSlot.buffer, localSlot.length, pkgMeta);
	recordPerfDelta(&g_npDrmInstallPerf.sha1Us);
	if (err != 0) {
		return err;
	}

	if (localSlot.pkgOffset < localSlot.itemDataStart) {
		SceUInt64 skip = localSlot.itemDataStart - localSlot.pkgOffset;
		SceSize skipBytes = (skip >= (SceUInt64)localSlot.length)
			? localSlot.length : (SceSize)skip;

		localSlot.buffer += skipBytes;
		localSlot.pkgOffset += skipBytes;
		localSlot.length -= skipBytes;
	}

	if (localSlot.length != 0) {
		if (localSlot.pkgOffset != localSlot.itemDataStart + hashState->fileHashedBytes) {
			return SCE_NPDRM_PKG_ERROR_INVALID_OFFSET;
		}
		err = npDrmDecryptPkgRegion(localSlot.buffer, localSlot.length, localSlot.pkgOffset,
			pkgMeta, (SceUInt8)isHwAccelEnabled(pkgMeta));
		recordPerfDelta(&g_npDrmInstallPerf.decryptUs);
		if (err != 0) {
			return err;
		}

		SceUInt64 fileOffset = localSlot.pkgOffset - localSlot.itemDataStart;
		SceUInt64 remainInItem = localSlot.itemDataSize - fileOffset;

		localSlot.pkgOffset = fileOffset;
		if (remainInItem < (SceUInt64)localSlot.length) {
			localSlot.length = (SceUInt32)remainInItem;
		}
		err = pwriteExact(*ioHandle->fdPtr, localSlot.buffer, localSlot.length, (SceOff)fileOffset);
		recordPerfDelta(&g_npDrmInstallPerf.fileWriteUs);
		if (err != 0) {
			return err;
		}
		err = npDrmHashChunkSha256(&localSlot, &hashState->fileSha256Ctx, &hashState->fileHashedBytes);
		if (err != 0) {
			return err;
		}
	}

	hashState->pkgSha1Ctx = localSlot.sha1Ctx;
	hashState->pkgHashedBytes = localSlot.sha1HashedBytes;
	hashState->pkgTotalBytes = localSlot.sha1TotalBytes;
	return 0;
}

int npDrmWriteContentSection(NpDrmInstallContext *ctx, NpDrmWriteCursor *cursor)
{
	NpDrmIoHandle ioHandle;
	NpDrmIoHandle headIo;

	ioHandle.fdPtr = &ctx->activeFd;
	ioHandle.pathBuf = ctx->workPath;
	ioHandle.rootDir = ctx->rootDir;

	headIo.fdPtr = &ctx->headFd;
	headIo.pathBuf = ctx->workPath;
	headIo.rootDir = ctx->rootDir;

	while (cursor->length != 0 && ctx->statFile.hashState.currentItemIndex < ctx->pkgMeta.pkgInfo.itemCount) {
		if ((ctx->pkgMeta.metaFlags & NPDRM_META_FLAG_ABORTED) != 0) {
			return SCE_NPDRM_PKG_ERROR_ABORTED;
		}

		NpDrmHashState stagedHash = ctx->statFile.hashState;
		if ((SceInt32)stagedHash.currentItemIndex != ctx->currentItem.itemIndex) {
			int err = npDrmOpenNextItem(&ioHandle, &stagedHash, &ctx->currentItem,
				ctx->scratch.itemName.name, ctx->prefixFilter, ctx->prefixFilterLen,
				&headIo, &ctx->pkgMeta);
			if (err != 0) {
				return err;
			}

			for (SceUInt32 slotIdx = 0; slotIdx < ctx->pipeline.chunkCount; ++slotIdx) {
				NpDrmChunkSlot *slot = &ctx->pipeline.chunks[slotIdx];

				slot->sha1Ctx = stagedHash.pkgSha1Ctx;
				slot->sha1HashedBytes = stagedHash.pkgHashedBytes;
				slot->sha1TotalBytes = stagedHash.pkgTotalBytes;
				slot->sha256Ctx = stagedHash.fileSha256Ctx;
				slot->sha256HashedBytes = stagedHash.fileHashedBytes;
				slot->sha256TotalBytes = stagedHash.fileTotalBytes;
				slot->itemDataStart = (ctx->currentItem.isExtractable != 0)
					? ctx->currentItem.dataStart : ctx->currentItem.alignedEnd;
				slot->itemDataSize = ctx->currentItem.dataSize;
				slot->itemAlignedEnd = ctx->currentItem.alignedEnd;
				slot->pipeline = &ctx->pipeline;
				slot->pkgMeta = &ctx->pkgMeta;
				slot->chunkFlags = 0;
				if (ctx->currentItem.isEbootPbp != 0 && ctx->pipeline.statFilePtr != NULL) {
					slot->chunkFlags = NPDRM_CHUNK_FLAG_EBOOT_PBP;
					ctx->pipeline.statFilePtr->version = 2U;
					ctx->pipeline.statFilePtr->statSize = NPDRM_PKG_STAT_SIZE_EXT;
					ctx->pipeline.statFilePtr->ebootItemIndex = (SceUInt32)ctx->currentItem.itemIndex;
				}
			}
		}

		if (ctx->currentItem.isDirectory == 0 && cursor->offset < ctx->currentItem.alignedEnd) {
			SceUInt64 remainInItem = ctx->currentItem.alignedEnd - cursor->offset;
			NpDrmWriteCursor itemSlice;

			itemSlice.data = cursor->data;
			itemSlice.offset = cursor->offset;
			itemSlice.length = (remainInItem >= (SceUInt64)cursor->length)
				? cursor->length : (SceUInt32)remainInItem;

			int err = npDrmProcessItemData(&ctx->pipeline, &itemSlice, &ctx->currentItem,
				&ctx->pkgMeta, &ioHandle, &stagedHash);
			if (err != 0) {
				return err;
			}
			cursor->data += itemSlice.length;
			cursor->length -= itemSlice.length;
			cursor->offset += itemSlice.length;
		}

		if (ctx->currentItem.isDirectory != 0 || cursor->offset == ctx->currentItem.alignedEnd) {
			for (SceUInt32 flushIdx = 0; flushIdx < ctx->pipeline.chunkCount; ++flushIdx) {
				int err = npDrmAdvancePipeline(&ctx->pipeline, &stagedHash);
				if (err != 0) {
					return err;
				}
			}
			ctx->pipeline.currentChunkIndex = 0;

			SceUInt8 itemDigest[32];
			int finishErr = 0;

			sceClibMemset(itemDigest, 0, sizeof(itemDigest));
			if (ctx->currentItem.isExtractable != 0 && ctx->currentItem.isDirectory == 0) {
				finishErr = closeIoFile(&ctx->activeFd);
				recordPerfDelta(&g_npDrmInstallPerf.fileCloseUs);
				if (finishErr == 0) {
					if ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_SKIP_BODY_HASH) == 0
						|| (ctx->pkgMeta.metaFlags & NPDRM_META_FLAG_FINALIZED) != 0) {
						sceSha256BlockResult(&stagedHash.fileSha256Ctx, itemDigest);
						sceSha256BlockInit(&stagedHash.fileSha256Ctx);
					}
					stagedHash.fileHashedBytes = 0ULL;
					stagedHash.fileTotalBytes = 0ULL;
					recordPerfDelta(&g_npDrmInstallPerf.sha256Us);
				}
			}

			if (finishErr == 0
				&& ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_SKIP_BODY_HASH) == 0
					|| (ctx->pkgMeta.metaFlags & NPDRM_META_FLAG_FINALIZED) != 0)) {
				int keepOpen = (ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_KEEP_FDS_OPEN) != 0;

				if (!keepOpen || ctx->bodyFd < 0) {
					int openFlags = ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_DIRECT_IO) != 0)
						? 65538 : 2;

					finishErr = buildRelativePath(ctx->workPath, &ctx->workPathLen, ctx->rootDir,
						ctx->rootDirLen, "sce_sys/package/body.bin", 25U);
					if (finishErr == 0) {
						finishErr = openIoFile(&ctx->bodyFd, ctx->workPath, openFlags, 6);
					}
				}
				if (finishErr == 0) {
					SceUInt32 bodyEntryOffset = stagedHash.currentItemIndex * 32U;

					npDrmDecryptPkgRegion(itemDigest, sizeof(itemDigest),
						(SceUInt64)(SceOff)bodyEntryOffset - (SceUInt64)0x100000000ULL,
						&ctx->pkgMeta, (SceUInt8)isHwAccelEnabled(&ctx->pkgMeta));
					finishErr = pwriteExact(ctx->bodyFd, itemDigest, sizeof(itemDigest),
						(SceOff)bodyEntryOffset);
					if (!keepOpen) {
						if (finishErr != 0) {
							closeIoFile(&ctx->bodyFd);
						} else {
							finishErr = closeIoFile(&ctx->bodyFd);
						}
					}
					if (finishErr == 0) {
						sceSha256BlockUpdate(&stagedHash.bodySha256Ctx, itemDigest, sizeof(itemDigest));
						stagedHash.bodyHashedBytes += sizeof(itemDigest);
					}
				}
			}
			recordPerfDelta(&g_npDrmInstallPerf.bodyStatUs);
			if (finishErr != 0) {
				return finishErr;
			}
			++stagedHash.currentItemIndex;
		}

		ctx->statFile.hashState = stagedHash;
		if ((SceInt32)stagedHash.currentItemIndex != ctx->currentItem.itemIndex
			&& ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_SKIP_BODY_HASH) == 0
				|| (ctx->pkgMeta.metaFlags & NPDRM_META_FLAG_FINALIZED) != 0)) {
			SceSha256Context bodySnapshot = ctx->statFile.hashState.bodySha256Ctx;

			sceSha256BlockResult(&bodySnapshot, ctx->statFile.bodySha256Digest);
			recordPerfDelta(&g_npDrmInstallPerf.bodyStatUs);
			if ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_SKIP_BODY_HASH) == 0) {
				int statErr = writeStatFileWithIo(&ctx->statFile, &ioHandle, ctx->rootDirLen,
					&ctx->workPathLen, &ctx->pkgMeta);
				recordPerfDelta(&g_npDrmInstallPerf.bodyStatUs);
				if (statErr != 0) {
					return statErr;
				}
			}
		}

		if ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_STEP_ONE_ITEM) != 0
			&& (SceInt32)stagedHash.currentItemIndex != ctx->currentItem.itemIndex) {
			break;
		}
	}

	if (ctx->statFile.hashState.currentItemIndex >= ctx->pkgMeta.pkgInfo.itemCount) {
		if (cursor->offset < ctx->pkgMeta.tailOffset) {
			SceUInt64 padRemain = ctx->pkgMeta.tailOffset - cursor->offset;
			SceSize padBytes = (padRemain >= (SceUInt64)cursor->length)
				? cursor->length : (SceSize)padRemain;
			SceSha1Context stagedSha1 = ctx->statFile.hashState.pkgSha1Ctx;
			SceUInt64 stagedHashed = ctx->statFile.hashState.pkgHashedBytes;

			int err = updateSha1WithAccel(&stagedSha1, &stagedHashed, cursor->data, padBytes, &ctx->pkgMeta);
			recordPerfDelta(&g_npDrmInstallPerf.sha1Us);
			if (err != 0) {
				return err;
			}
			cursor->data += padBytes;
			cursor->length = 0;
			cursor->offset += padBytes;
			ctx->statFile.hashState.pkgSha1Ctx = stagedSha1;
			ctx->statFile.hashState.pkgHashedBytes = stagedHashed;
		}

		if (ctx->bodyFd >= 0) {
			int err = closeIoFile(&ctx->bodyFd);
			recordPerfDelta(&g_npDrmInstallPerf.bodyStatUs);
			if (err != 0) {
				return err;
			}
		}
		if (ctx->headFd >= 0) {
			int err = closeIoFile(&ctx->headFd);
			recordPerfDelta(&g_npDrmInstallPerf.itemOpenUs);
			if (err != 0) {
				return err;
			}
		}
	}
	return 0;
}

int npDrmWriteTailSection(NpDrmInstallContext *ctx, NpDrmWriteCursor *cursor)
{
	SceUInt64 pkgEnd = ctx->pkgMeta.tailOffset + ctx->pkgMeta.tailSize;
	int err;

	if (cursor->offset < pkgEnd && ctx->activeFd < 0) {
		int openFlags = ((ctx->pkgMeta.openFlags & NPDRM_OPEN_FLAG_DIRECT_IO) != 0) ? 67074 : 1538;

		err = buildRelativePath(ctx->workPath, &ctx->workPathLen, ctx->rootDir,
			ctx->rootDirLen, "sce_sys/package/tail.bin", 25U);
		if (err != 0) {
			return err;
		}
		err = openIoFile(&ctx->activeFd, ctx->workPath, openFlags, 6);
		if (err != 0) {
			return err;
		}
		if (ctx->statFile.hashState.fileTotalBytes == 0ULL) {
			sceSha256BlockInit(&ctx->statFile.hashState.fileSha256Ctx);
			ctx->statFile.hashState.fileTotalBytes = ctx->pkgMeta.tailSize;
			ctx->statFile.hashState.fileHashedBytes = 0ULL;
		}
	}

	/* Part 1: before sha1LimitOffset */
	err = writeTailSegment(ctx, cursor, ctx->pkgMeta.sha1LimitOffset);
	if (err != 0) {
		return err;
	}

	/* Snapshot SHA-1 digest at sha1LimitOffset */
	if (cursor->offset == ctx->pkgMeta.sha1LimitOffset && cursor->length != 0) {
		SceSha1Context sha1Copy = ctx->statFile.hashState.pkgSha1Ctx;

		sceSha1BlockResult(&sha1Copy, ctx->statFile.pkgSha1Digest);
	}

	/* Part 2: between sha1LimitOffset and tailSigOffset */
	if (cursor->offset >= ctx->pkgMeta.sha1LimitOffset) {
		err = writeTailSegment(ctx, cursor, ctx->pkgMeta.tailSigOffset);
		if (err != 0) {
			return err;
		}
	}

	/* Part 3: capture tail signature bytes while writing */
	SceUInt64 sigStart = ctx->pkgMeta.tailSigOffset;
	SceUInt64 sigEnd = sigStart + (SceUInt64)ctx->pkgMeta.tailSigSize;
	if (cursor->offset >= sigStart && cursor->offset < sigEnd && cursor->length != 0) {
		SceUInt64 sigRemain = sigEnd - cursor->offset;
		SceSize copyLen = (sigRemain >= (SceUInt64)cursor->length)
			? cursor->length : (SceSize)sigRemain;

		sceClibMemcpy_safe(&ctx->scratch.tailSignature[cursor->offset - sigStart],
			cursor->data, copyLen);
		err = writeTailSegment(ctx, cursor, sigEnd);
		if (err != 0) {
			return err;
		}
	}

	/* Part 4: remaining bytes up to pkgEnd */
	if (cursor->offset >= sigEnd) {
		err = writeTailSegment(ctx, cursor, pkgEnd);
		if (err != 0) {
			return err;
		}
	}

	if (cursor->offset != pkgEnd) {
		return 0;
	}

	err = closeIoFile(&ctx->activeFd);
	if (err != 0) {
		return err;
	}

	sceSha256BlockResult(&ctx->statFile.hashState.fileSha256Ctx, ctx->statFile.tailSha256Digest);
	sceSha256BlockInit(&ctx->statFile.hashState.fileSha256Ctx);
	ctx->statFile.hashState.fileHashedBytes = 0ULL;
	ctx->statFile.hashState.fileTotalBytes = 0ULL;
	sceClibMemset(&ctx->statFile.hashState.pkgSha1Ctx, 0, 0x230U);

	NpDrmIoHandle ioHandle;
	ioHandle.fdPtr = &ctx->activeFd;
	ioHandle.pathBuf = ctx->workPath;
	ioHandle.rootDir = ctx->rootDir;

	err = writeStatFileWithIo(&ctx->statFile, &ioHandle, ctx->rootDirLen,
		&ctx->workPathLen, &ctx->pkgMeta);
	if (err != 0) {
		return err;
	}

	return npDrmVerifyTailSignature(ctx->scratch.tailSignature, ctx->pkgMeta.tailSigSize,
		ctx->statFile.pkgSha1Digest, &ctx->pkgMeta);
}

int npDrmDispatchWriteCursor(NpDrmInstallContext *ctx, NpDrmWriteCursor *cursor)
{
	g_npDrmInstallPerf.lastStampUs = readPerfClock();

	if ((cursor->length & 15U) != 0) {
		return SCE_NPDRM_PKG_ERROR_UNALIGNED_SIZE;
	}
	if ((cursor->offset & 15ULL) != 0) {
		return SCE_NPDRM_PKG_ERROR_UNALIGNED_OFFSET;
	}

	if (cursor->offset < ctx->maxWrittenOffset) {
		SceUInt64 skip = ctx->maxWrittenOffset - cursor->offset;
		SceSize skipBytes = (skip >= (SceUInt64)cursor->length)
			? cursor->length : (SceSize)skip;

		cursor->offset += skipBytes;
		cursor->data += skipBytes;
		cursor->length -= skipBytes;
		if (cursor->length == 0) {
			return 0;
		}
	}

	if (ctx->pkgMeta.headSize == 0) {
		if (cursor->offset != 0ULL) {
			return SCE_NPDRM_PKG_ERROR_INVALID_OFFSET;
		}
		int err = npDrmProcessPkgHeader(ctx, cursor->data, cursor->length, 0);
		recordPerfDelta(&g_npDrmInstallPerf.headPhaseUs);
		if (err != 0) {
			return err;
		}
	}

	if (cursor->offset < (SceUInt64)ctx->pkgMeta.headSize && cursor->length != 0) {
		int err = npDrmWriteHeadSection(ctx, cursor);
		recordPerfDelta(&g_npDrmInstallPerf.headPhaseUs);
		if (err != 0) {
			return err;
		}
	}

	if (cursor->offset >= (SceUInt64)ctx->pkgMeta.headSize
		&& cursor->offset < ctx->pkgMeta.tailOffset && cursor->length != 0) {
		int err = npDrmWriteContentSection(ctx, cursor);
		recordPerfDelta(&g_npDrmInstallPerf.contentPhaseUs);
		if (err != 0) {
			return err;
		}
	}

	SceUInt64 pkgEnd = ctx->pkgMeta.tailOffset + ctx->pkgMeta.tailSize;
	if (cursor->offset >= ctx->pkgMeta.tailOffset && cursor->offset < pkgEnd && cursor->length != 0) {
		int err = npDrmWriteTailSection(ctx, cursor);
		recordPerfDelta(&g_npDrmInstallPerf.tailPhaseUs);
		if (err != 0) {
			return err;
		}
	}
	return 0;
}

int npDrmLoadAndVerifyStat(NpDrmStatFile *outStat, NpDrmIoHandle *ioHandle,
	const NpDrmPkgMeta *pkgMeta)
{
	int openFlags = ((pkgMeta->openFlags & NPDRM_OPEN_FLAG_DIRECT_IO) != 0) ? 65537 : 1;
	SceSize *rootLenPtr = (SceSize *)&ioHandle->rootDir[NPDRM_PKG_ROOT_MAX];
	SceSize *pathLenPtr = (SceSize *)&ioHandle->pathBuf[NPDRM_PKG_PATH_MAX];
	int err = buildRelativePath(ioHandle->pathBuf, pathLenPtr, ioHandle->rootDir,
		*rootLenPtr, "sce_sys/package/stat.bin", 25U);
	if (err != 0) {
		return err;
	}
	err = openIoFile(ioHandle->fdPtr, ioHandle->pathBuf, openFlags, 6);
	if (err != 0) {
		return err;
	}

	SceOff statLen = sceIoLseek(*ioHandle->fdPtr, 0, SCE_SEEK_END);
	if (statLen < 0) {
		err = (int)statLen;
	} else if (statLen < (SceOff)NPDRM_PKG_STAT_SIZE_BASE
		|| statLen > (SceOff)NPDRM_PKG_STAT_SIZE_EXT) {
		err = SCE_NPDRM_PKG_ERROR_CORRUPT_STAT;
	} else {
		err = preadExact(*ioHandle->fdPtr, outStat, (SceSize)statLen, 0);
		if (err == 0) {
			SceUInt8 *statBody = (SceUInt8 *)&outStat->magic;

			npDrmStatDecryptCbc(statBody, NPDRM_PKG_STAT_SIZE_EXT, pkgMeta->keySlotId);
			err = SCE_NPDRM_PKG_ERROR_CORRUPT_STAT;
			if (outStat->magic == NPDRM_PKG_STAT_MAGIC
				&& outStat->version >= 1U && outStat->version <= 2U
				&& outStat->statSize >= NPDRM_PKG_STAT_SIZE_BASE
				&& outStat->statSize <= NPDRM_PKG_STAT_SIZE_EXT
				&& (outStat->version != 1U || outStat->statSize == NPDRM_PKG_STAT_SIZE_BASE)
				&& (outStat->version != 2U || outStat->statSize == NPDRM_PKG_STAT_SIZE_EXT)) {
				SceUInt8 expectedCmac[16];

				npDrmStatComputeCmac(expectedCmac, statBody, outStat->statSize - 16U,
					pkgMeta->keySlotId);
				if (sceClibMemcmp(outStat->cmac, expectedCmac, sizeof(expectedCmac)) == 0) {
					err = 0;
				}
			}
		}
	}

	if (err != 0) {
		closeIoFile(ioHandle->fdPtr);
		return err;
	}
	return closeIoFile(ioHandle->fdPtr);
}

int npDrmReadDirFile(const char *dirPath, void *outBuf, SceSize bufSize, const NpDrmFileSpec *spec)
{
	if (bufSize < spec->minBufferSize) {
		return SCE_NPDRM_PKG_ERROR_BUFFER_TOO_SMALL;
	}
	if (dirPath == NULL) {
		return SCE_NPDRM_PKG_ERROR_INVALID_DIR;
	}
	const char *nulPos = (const char *)sceClibMemchr(dirPath, '\0', NPDRM_PKG_ROOT_MAX);
	SceSize dirLen = (nulPos != NULL) ? (SceSize)(nulPos - dirPath) : NPDRM_PKG_ROOT_MAX;

	if (dirLen == 0 || dirPath[dirLen - 1U] != '/') {
		return SCE_NPDRM_PKG_ERROR_INVALID_DIR;
	}

	char pathBuf[NPDRM_PKG_PATH_MAX];
	SceSize pathLen = 0;
	int err = buildRelativePath(pathBuf, &pathLen, dirPath, dirLen,
		spec->relativePath, spec->pathLen);
	if (err != 0) {
		return err;
	}

	SceUID fd = -1;
	err = openIoFile(&fd, pathBuf, 1, 6);
	if (err != 0) {
		return err;
	}

	SceOff fileLen = sceIoLseek(fd, 0, SCE_SEEK_END);
	if (fileLen < 0) {
		return (int)fileLen;
	}
	SceSize toRead = (fileLen < (SceOff)bufSize) ? (SceSize)fileLen : bufSize;

	err = preadExact(fd, outBuf, toRead, 0);
	if (err != 0) {
		closeIoFile(&fd);
		return err;
	}
	err = closeIoFile(&fd);
	if (err != 0) {
		return err;
	}
	return (int)toRead;
}

int npDrmWriteDirFile(const char *dirPath, const void *inBuf, SceSize bufSize, const NpDrmFileSpec *spec)
{
	if (dirPath == NULL) {
		return SCE_NPDRM_PKG_ERROR_INVALID_DIR;
	}
	const char *nulPos = (const char *)sceClibMemchr(dirPath, '\0', NPDRM_PKG_ROOT_MAX);
	SceSize dirLen = (nulPos != NULL) ? (SceSize)(nulPos - dirPath) : NPDRM_PKG_ROOT_MAX;

	if (dirLen == 0 || dirPath[dirLen - 1U] != '/') {
		return SCE_NPDRM_PKG_ERROR_INVALID_DIR;
	}

	char pathBuf[NPDRM_PKG_PATH_MAX];
	SceSize pathLen = 0;
	int err = buildRelativePath(pathBuf, &pathLen, dirPath, dirLen,
		spec->relativePath, spec->pathLen);
	if (err != 0) {
		return err;
	}

	SceUID fd = -1;
	err = openIoFile(&fd, pathBuf, 1538, 6);
	if (err != 0) {
		return err;
	}
	err = pwriteExact(fd, inBuf, bufSize, 0);
	if (err != 0) {
		closeIoFile(&fd);
		return err;
	}
	err = closeIoFile(&fd);
	if (err != 0) {
		return err;
	}
	return (int)bufSize;
}

} /* extern "C" */
