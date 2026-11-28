/*
 * nptrophy_io.cpp: version/filename/path utilities, file/memfile/record-file IO,
 * cancellation handles, progress tracking, and SHA-1 / HMAC-SHA1 wrappers
 * (0x812E7868..0x812E9977, 0x812EDAA4..0x812F0207).
 */
#include "nptrophy_io.h"

struct TrophyNpJid {
	const char *node;
	const char *domain;
	const char *resource;
	SceUInt32 reserved;
};

extern "C" {
extern int SceNpCommon_5BC54899(SceNpCommunicationId *commId, const char *str);
extern int SceNpCommon_40CC4D15(TrophyNpJid *jid, const SceNpId *npId, char *buf,
				SceSize bufSize, SceSize *required);
extern int SceNpCommon_85699AE2(const TrophyNpJid *jid, char *buf, SceSize bufSize,
				SceSize *required);
extern uint32_t sceNetHtonl(uint32_t host32);
extern uint32_t sceNetNtohl(uint32_t net32);
extern uint64_t sceNetHtonll(uint64_t host64);
extern uint64_t sceNetNtohll(uint64_t net64);
}

/* Kernel IO errno values mapped to SceNpTrophy file errors */
static const int SCE_IO_ERR_ENOENT = -2147418110;	/* 0x80010002 */
static const int SCE_IO_ERR_EACCES = -2147418093;	/* 0x80010013 */
static const int SCE_NP_COMMON_ERR_INVALID_COMM_ID = -2141911542; /* 0x8055060A */

static inline SceSize trophyRecordPadLen(uint32_t recordSize)
{
	uint32_t rem = (recordSize + 16u) & 0xFu;
	return (rem != 0) ? (16u - rem) : 0u;
}

static inline uint32_t trophyRecordStride(uint32_t recordSize)
{
	return recordSize + 16u + static_cast<uint32_t>(trophyRecordPadLen(recordSize));
}

/* --- Version / CommId / JID Utilities (0x812E7868..0x812E7B2D) --- */

int npTrophyParseVersionN(const char *str, SceSize maxLen, uint32_t *outVersion)
{
	const char *minorStart = NULL;
	SceSize majorLen = 0;
	SceSize pos = 0;

	while (pos < maxLen && str[pos] != '\0') {
		if (str[pos] == '.') {
			if (pos == 0)
				return NP_TROPHY_ERR_PARSE_ERROR;
			majorLen = pos;
			minorStart = str + pos + 1;
		}
		++pos;
	}

	if (minorStart == NULL)
		return NP_TROPHY_ERR_PARSE_ERROR;

	SceSize minorLen = static_cast<SceSize>((str + pos) - minorStart);
	if (minorLen == 0)
		return NP_TROPHY_ERR_PARSE_ERROR;

	unsigned int majorVal = 0;
	unsigned int minorVal = 0;
	if (npTrophyParseUintN(str, majorLen, &majorVal) < 0 || majorVal > 0xFFFFu)
		return NP_TROPHY_ERR_PARSE_ERROR;
	if (npTrophyParseUintN(minorStart, minorLen, &minorVal) < 0 || minorVal > 0xFFFFu)
		return NP_TROPHY_ERR_PARSE_ERROR;

	*outVersion = ((majorVal & 0xFFFFu) << 16) | (minorVal & 0xFFFFu);
	return 0;
}

int npTrophyParseVersion(const char *str, uint32_t *outVersion)
{
	SceSize len = sceClibStrnlen(str, 0x7FFFFFFFu);
	int err = npTrophyParseVersionN(str, len, outVersion);
	return (err < 0) ? err : 0;
}

int npTrophyFormatVersion(uint32_t version, char *outBuf, SceSize outSize)
{
	uint16_t majorVal = static_cast<uint16_t>(version >> 16);
	uint16_t minorVal = static_cast<uint16_t>(version & 0xFFFFu);
	SceSize majorLen = 0;
	int err = npTrophyFormatUint(majorVal, outBuf, outSize, &majorLen);
	if (err < 0)
		return err;

	if (outSize < majorLen + 1)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	outBuf[majorLen] = '.';

	SceSize minorOffset = majorLen + 1;
	SceSize minorLen = 0;
	err = npTrophyFormatUint(minorVal, outBuf + minorOffset, outSize - minorOffset, &minorLen);
	if (err < 0)
		return err;

	SceSize totalLen = minorOffset + minorLen;
	if (outSize < totalLen + 1)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	outBuf[totalLen] = '\0';
	return 0;
}

int npTrophyParseCommId(const char *str, TrophyCommId *outCommId)
{
	int err = SceNpCommon_5BC54899(reinterpret_cast<SceNpCommunicationId *>(outCommId), str);
	if (err == SCE_NP_COMMON_ERR_INVALID_COMM_ID)
		return NP_TROPHY_ERR_PARSE_ERROR;
	return (err < 0) ? err : 0;
}

int npTrophyParseCommIdN(const char *str, SceSize length, TrophyCommId *outCommId)
{
	if (length > 12)
		return NP_TROPHY_ERR_PARSE_ERROR;
	char copy[13];
	sceClibMemset(copy, 0, sizeof(copy));
	sceClibMemcpy_safe(copy, str, length);
	return npTrophyParseCommId(copy, outCommId);
}

int npTrophyFormatCommId(const TrophyCommId *commId, char *outBuf, SceSize outSize)
{
	int err = SceNpCommon_E426FEE8(reinterpret_cast<const NpTrophyCommId *>(commId),
				       outBuf, outSize);
	return (err < 0) ? err : 0;
}

int npTrophyNpIdToJidString(const SceNpId *npId, char *outBuf, SceSize outSize)
{
	TrophyNpJid jid;
	SceSize required = 0;
	int err = SceNpCommon_40CC4D15(&jid, npId, NULL, 0, &required);
	if (err < 0)
		return err;

	char *workBuf = static_cast<char *>(npTrophyHeapAlloc(required));
	if (workBuf == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	err = SceNpCommon_40CC4D15(&jid, npId, workBuf, required, NULL);
	if (err >= 0) {
		err = SceNpCommon_85699AE2(&jid, outBuf, outSize, NULL);
		if (err >= 0)
			err = 0;
	}

	npTrophyHeapFree(workBuf);
	return err;
}

/* --- TrophyDir & Filesystem Utilities (0x812E7B2E..0x812E800F) --- */

TrophyDir::~TrophyDir()
{
	if (m_dirFd != -1) {
		if (sceIoDclose(m_dirFd) >= 0)
			m_dirFd = -1;
	}
}

int TrophyDir::Open(const char *dirPath, bool enableIoctl)
{
	SceUID fd = sceIoDopen(dirPath);
	if (fd < 0)
		return fd;

	if (enableIoctl) {
		int err = sceIoIoctl(fd, 0x1802u, NULL, 0, NULL, 0);
		if (err < 0) {
			sceIoDclose(fd);
			return err;
		}
	}
	m_dirFd = fd;
	return 0;
}

int TrophyDir::Close()
{
	if (m_dirFd == -1)
		return NP_TROPHY_ERR_DIR_NOT_OPEN;
	int err = sceIoDclose(m_dirFd);
	if (err < 0)
		return err;
	m_dirFd = -1;
	return 0;
}

int TrophyDir::Read(TrophyDirEntry *outEntry, bool *outHasEntry)
{
	SceIoDirent dirent;
	sceClibMemset(&dirent, 0, sizeof(dirent));
	int res = sceIoDread(m_dirFd, &dirent);
	if (res < 0)
		return res;
	if (res == 0) {
		*outHasEntry = false;
		return 0;
	}

	if ((dirent.d_stat.st_mode & 0x1000) != 0)
		outEntry->kind = TROPHY_DIR_ENTRY_DIR;
	else if ((dirent.d_stat.st_mode & 0x2000) != 0)
		outEntry->kind = TROPHY_DIR_ENTRY_LINK;
	else if ((dirent.d_stat.st_mode & 0x4000) != 0)
		outEntry->kind = TROPHY_DIR_ENTRY_FILE;
	else
		outEntry->kind = TROPHY_DIR_ENTRY_OTHER;

	SceSize nameLen = sceClibStrnlen(dirent.d_name, 0x7FFFFFFFu);
	if (nameLen >= sizeof(outEntry->name))
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	sceClibStrncpy(outEntry->name, dirent.d_name, sizeof(outEntry->name));
	outEntry->name[nameLen] = '\0';
	*outHasEntry = true;
	return 0;
}

int npTrophyPathExists(const char *path, bool *outExists)
{
	SceIoStat stat;
	int err = sceIoGetstat(path, &stat);
	if (err == 0) {
		*outExists = true;
		return 0;
	}
	if (err == SCE_IO_ERR_EACCES || err == SCE_IO_ERR_ENOENT) {
		*outExists = false;
		return 0;
	}
	return err;
}

int npTrophyEnsureDir(const char *path, SceMode mode)
{
	bool exists = false;
	int err = npTrophyPathExists(path, &exists);
	if (err < 0)
		return err;
	if (exists)
		return 0;
	err = sceIoMkdir(path, mode);
	return (err < 0) ? err : 0;
}

int npTrophyRemoveFile(const char *path)
{
	int err = sceIoRemove(path);
	if (err >= 0)
		return 0;
	if (err == SCE_IO_ERR_ENOENT)
		return NP_TROPHY_ERR_FILE_NOT_FOUND;
	return err;
}

int npTrophyRenamePath(const char *oldPath, const char *newPath)
{
	int err = sceIoRename(oldPath, newPath);
	if (err >= 0)
		return 0;
	if (err == SCE_IO_ERR_ENOENT)
		return NP_TROPHY_ERR_FILE_NOT_FOUND;
	return err;
}

static int removeTreeRecursive(char *pathBuf)
{
	SceIoStat stat;
	bool isDir = false;
	int err = sceIoGetstat(pathBuf, &stat);
	if (err == 0) {
		isDir = (stat.st_mode & 0x1000) != 0;
	} else if (err != SCE_IO_ERR_EACCES && err != SCE_IO_ERR_ENOENT) {
		return err;
	}

	if (!isDir)
		return npTrophyRemoveFile(pathBuf);

	TrophyDir dir;
	err = dir.Open(pathBuf, true);
	if (err < 0)
		return err;

	for (;;) {
		TrophyDirEntry entry;
		bool hasEntry = false;
		err = dir.Read(&entry, &hasEntry);
		if (err < 0)
			return err;
		if (!hasEntry)
			break;
		if (sceClibStrncmp(entry.name, ".", 2) == 0 ||
		    sceClibStrncmp(entry.name, "..", 3) == 0)
			continue;

		err = npTrophyPathAppend(pathBuf, 1, entry.name);
		if (err < 0)
			return err;
		err = removeTreeRecursive(pathBuf);
		if (err < 0)
			return err;
		err = npTrophyPathPop(pathBuf);
		if (err < 0)
			return err;
	}

	err = dir.Close();
	if (err < 0)
		return err;

	err = sceIoGetstat(pathBuf, &stat);
	if (err == 0) {
		err = sceIoRmdir(pathBuf);
		return (err < 0) ? err : 0;
	}
	if (err == SCE_IO_ERR_EACCES || err == SCE_IO_ERR_ENOENT)
		return 0;
	return err;
}

int npTrophyRemoveTree(const char *path)
{
	bool exists = false;
	int err = npTrophyPathExists(path, &exists);
	if (err < 0)
		return err;
	if (!exists)
		return 0;

	char pathBuf[256];
	err = npTrophyPathInit(pathBuf, 1, path);
	if (err < 0)
		return err;
	err = removeTreeRecursive(pathBuf);
	if (err < 0)
		return err;

	SceIoStat stat;
	err = sceIoGetstat(pathBuf, &stat);
	if (err == 0) {
		err = sceIoRmdir(pathBuf);
		return (err < 0) ? err : 0;
	}
	if (err == SCE_IO_ERR_EACCES || err == SCE_IO_ERR_ENOENT)
		return 0;
	return err;
}

/* --- Trophy Asset Filename Parser & Formatter (0x812E8010..0x812E8C87) --- */

int npTrophyFormatCommDirName(char *outName, const TrophyCommId *commId, const char *suffix)
{
	sceClibMemset(outName, 0, 32);
	int err = commId->FormatToString(outName, 32);
	if (err < 0)
		return err;
	if (npTrophyStrlcat(outName, suffix, 32) >= 32)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	return 0;
}

int npTrophyFormatCommDirNamePlain(char *outName, const TrophyCommId *commId)
{
	return npTrophyFormatCommDirName(outName, commId, "");
}

bool npTrophyIsSceTropDirName(const char *name)
{
	return sceClibStrncmp(name, "sce_trop", 9) == 0;
}

bool npTrophyParseCommDirName(const char *name, TrophyCommId *outCommId)
{
	TrophyCommId localId;
	TrophyCommId *target = (outCommId != NULL) ? outCommId : &localId;
	if (target->ParseFromString(name) >= 0)
		return true;
	target->Clear();
	return false;
}

bool TrophyAssetDesc::IsDefaultLang() const
{
	if (kind >= TROPHY_ASSET_SFM && kind <= TROPHY_ASSET_GROUP_ICON)
		return langOrTrophyId == -1;
	return false;
}

bool TrophyAssetDesc::HasSpecificLang() const
{
	if (kind < TROPHY_ASSET_SFM || kind > TROPHY_ASSET_GROUP_ICON)
		return false;
	return langOrTrophyId != -1;
}

bool TrophyAssetDesc::IsValidLang() const
{
	if (kind >= TROPHY_ASSET_SFM && kind <= TROPHY_ASSET_GROUP_ICON)
		return static_cast<unsigned int>(langOrTrophyId) < 20u;
	return false;
}

void TrophyAssetDesc::SetLang(int lang)
{
	if (kind >= TROPHY_ASSET_SFM && kind <= TROPHY_ASSET_GROUP_ICON)
		langOrTrophyId = lang;
}

int TrophyAssetDesc::GetLang() const
{
	if (kind >= TROPHY_ASSET_SFM && kind <= TROPHY_ASSET_GROUP_ICON)
		return langOrTrophyId;
	return -2;
}

static bool matchLangSuffixFilename(const char *filename, const char *prefix,
				    const char *extension, int *outLang)
{
	SceSize prefixLen = sceClibStrnlen(prefix, 0x7FFFFFFFu);
	SceSize extLen = sceClibStrnlen(extension, 0x7FFFFFFFu);
	SceSize totalLen = sceClibStrnlen(filename, 0x7FFFFFFFu);
	if (totalLen != prefixLen + extLen + 3 && totalLen != prefixLen + extLen)
		return false;
	if (sceClibStrncmp(filename, prefix, prefixLen) != 0)
		return false;

	const char *cursor = filename + prefixLen;
	int lang = -1;
	if (*cursor == '_') {
		if (cursor[1] < '0' || cursor[1] > '9' || cursor[2] < '0' || cursor[2] > '9')
			return false;
		lang = (cursor[1] - '0') * 10 + (cursor[2] - '0');
		cursor += 3;
	}
	if (sceClibStrncmp(cursor, extension, extLen) != 0)
		return false;
	if (outLang != NULL)
		*outLang = lang;
	return true;
}

static bool matchIndexedFilename(const char *filename, const char *prefix,
				 const char *extension, int *outNumber, int *outLang)
{
	SceSize prefixLen = sceClibStrnlen(prefix, 0x7FFFFFFFu);
	SceSize extLen = sceClibStrnlen(extension, 0x7FFFFFFFu);
	SceSize totalLen = sceClibStrnlen(filename, 0x7FFFFFFFu);
	if (totalLen != prefixLen + extLen + 6 && totalLen != prefixLen + extLen + 3)
		return false;
	if (sceClibStrncmp(filename, prefix, prefixLen) != 0)
		return false;

	const char *cursor = filename + prefixLen;
	if (cursor[0] < '0' || cursor[0] > '9' ||
	    cursor[1] < '0' || cursor[1] > '9' ||
	    cursor[2] < '0' || cursor[2] > '9')
		return false;
	int number = (cursor[0] - '0') * 100 + (cursor[1] - '0') * 10 + (cursor[2] - '0');
	cursor += 3;

	int lang = -1;
	if (*cursor == '_') {
		if (cursor[1] < '0' || cursor[1] > '9' || cursor[2] < '0' || cursor[2] > '9')
			return false;
		lang = (cursor[1] - '0') * 10 + (cursor[2] - '0');
		cursor += 3;
	}
	if (sceClibStrncmp(cursor, extension, extLen) != 0)
		return false;
	if (outNumber != NULL)
		*outNumber = number;
	if (outLang != NULL)
		*outLang = lang;
	return true;
}

int npTrophyParseAssetFilename(const char *filename, TrophyAssetDesc *outDesc)
{
	int lang = -1;
	int groupId = 0;
	int trophyId = 0;

	if (sceClibStrncmp(filename, "TROPCONF.SFM", 13) == 0) {
		outDesc->kind = TROPHY_ASSET_SFM;
		outDesc->langOrTrophyId = -2;
		return 0;
	}
	if (matchLangSuffixFilename(filename, "TROP", ".SFM", &lang)) {
		outDesc->kind = TROPHY_ASSET_SFM;
		outDesc->langOrTrophyId = lang;
		return 0;
	}
	if (matchLangSuffixFilename(filename, "ICON0", ".PNG", &lang)) {
		outDesc->kind = TROPHY_ASSET_TITLE_ICON;
		outDesc->langOrTrophyId = lang;
		return 0;
	}
	if (matchIndexedFilename(filename, "GR", ".PNG", &groupId, &lang)) {
		outDesc->kind = TROPHY_ASSET_GROUP_ICON;
		outDesc->langOrTrophyId = lang;
		outDesc->groupId = groupId;
		return 0;
	}
	if (matchIndexedFilename(filename, "TROP", ".PNG", &trophyId, &lang) && lang == -1) {
		outDesc->kind = TROPHY_ASSET_TROPHY_ICON;
		outDesc->langOrTrophyId = trophyId;
		return 0;
	}
	return NP_TROPHY_ERR_INVALID_FILENAME;
}

static int formatLangFilename(const char *prefix, const char *extension, int lang,
			      int smallFlag, char *outBuf, SceSize outSize)
{
	char langSuffix[4];
	sceClibMemset(outBuf, 0, outSize);
	if (static_cast<unsigned int>(lang) < 0xFFFFFFFEu) {
		langSuffix[0] = '_';
		langSuffix[1] = static_cast<char>((lang / 10) % 10 + '0');
		langSuffix[2] = static_cast<char>(lang % 10 + '0');
		langSuffix[3] = '\0';
	} else {
		langSuffix[0] = '\0';
	}
	if (npTrophyStrlcat(outBuf, prefix, outSize) >= outSize ||
	    npTrophyStrlcat(outBuf, langSuffix, outSize) >= outSize)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	if (smallFlag == 1 && npTrophyStrlcat(outBuf, "_S", outSize) >= outSize)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	if (npTrophyStrlcat(outBuf, extension, outSize) >= outSize)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	return 0;
}

static int formatIndexedFilename(const char *prefix, const char *extension, int number,
				 int lang, int smallFlag, char *outBuf, SceSize outSize)
{
	char langSuffix[4];
	char numStr[4];
	sceClibMemset(outBuf, 0, outSize);
	if (static_cast<unsigned int>(lang) < 0xFFFFFFFEu) {
		langSuffix[0] = '_';
		langSuffix[1] = static_cast<char>((lang / 10) % 10 + '0');
		langSuffix[2] = static_cast<char>(lang % 10 + '0');
		langSuffix[3] = '\0';
	} else {
		langSuffix[0] = '\0';
	}
	numStr[0] = static_cast<char>((number / 100) % 10 + '0');
	numStr[1] = static_cast<char>((number / 10) % 10 + '0');
	numStr[2] = static_cast<char>(number % 10 + '0');
	numStr[3] = '\0';

	if (npTrophyStrlcat(outBuf, prefix, outSize) >= outSize ||
	    npTrophyStrlcat(outBuf, numStr, outSize) >= outSize ||
	    npTrophyStrlcat(outBuf, langSuffix, outSize) >= outSize)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	if (smallFlag == 1 && npTrophyStrlcat(outBuf, "_S", outSize) >= outSize)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	if (npTrophyStrlcat(outBuf, extension, outSize) >= outSize)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	return 0;
}

int npTrophyFormatSfmFilename(char *outName, int lang)
{
	if (lang == -2) {
		sceClibMemset(outName, 0, 32);
		return (npTrophyStrlcat(outName, "TROPCONF.SFM", 32) >= 32)
			       ? NP_TROPHY_ERR_INSUFFICIENT_BUFFER
			       : 0;
	}
	return formatLangFilename("TROP", ".SFM", lang, 0, outName, 32);
}

int npTrophyFormatTitleIconFilename(char *outName, int lang, int smallFlag)
{
	return formatLangFilename("ICON0", ".PNG", lang, smallFlag, outName, 32);
}

int npTrophyFormatGroupIconFilename(char *outName, int groupId, int lang, int smallFlag)
{
	return formatIndexedFilename("GR", ".PNG", groupId, lang, smallFlag, outName, 32);
}

int npTrophyFormatTrophyIconFilename(char *outName, int trophyId, int smallFlag)
{
	return formatIndexedFilename("TROP", ".PNG", trophyId, -2, smallFlag, outName, 32);
}

int npTrophyFormatAssetFilename(char *outName, const TrophyAssetDesc *desc)
{
	switch (desc->kind) {
	case TROPHY_ASSET_SFM:
		return npTrophyFormatSfmFilename(outName, desc->langOrTrophyId);
	case TROPHY_ASSET_TITLE_ICON:
		return npTrophyFormatTitleIconFilename(outName, desc->langOrTrophyId, 0);
	case TROPHY_ASSET_GROUP_ICON:
		return npTrophyFormatGroupIconFilename(outName, desc->groupId,
						       desc->langOrTrophyId, 0);
	case TROPHY_ASSET_TROPHY_ICON:
		return npTrophyFormatTrophyIconFilename(outName, desc->langOrTrophyId, 0);
	default:
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	}
}

/* --- Path Building Utilities (0x812E8C88..0x812E8E8B, 0x812E9268..0x812E9313, 0x812EDAA4..0x812EE493) --- */

int npTrophyAppendPathSep(char *path, SceSize pathSize, char sep)
{
	char sepStr[2] = { sep, '\0' };
	SceSize len = sceClibStrnlen(path, 0x7FFFFFFFu);
	if (len == 0)
		return 0;
	char lastCh = path[len - 1];
	if (lastCh == sep || lastCh == ':')
		return 0;
	return (npTrophyStrlcat(path, sepStr, pathSize) < pathSize)
		       ? 0
		       : NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
}

int npTrophyAppendPathComponentsV(char *path, SceSize pathSize, char sep, int count, va_list ap)
{
	for (int index = 0; index < count; ++index) {
		const char *component = va_arg(ap, const char *);
		int err = npTrophyAppendPathSep(path, pathSize, sep);
		if (err < 0)
			return err;
		if (npTrophyStrlcat(path, component, pathSize) >= pathSize)
			return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	}
	return 0;
}

int npTrophyAppendPathComponents(char *path, SceSize pathSize, char sep, int count, ...)
{
	va_list ap;
	va_start(ap, count);
	int err = npTrophyAppendPathComponentsV(path, pathSize, sep, count, ap);
	va_end(ap);
	return err;
}

int npTrophyBuildPathV(char *outPath, SceSize pathSize, char sep, int count, va_list ap)
{
	sceClibMemset(outPath, 0, pathSize);
	int err = npTrophyAppendPathComponentsV(outPath, pathSize, sep, count, ap);
	if (err < 0) {
		sceClibMemset(outPath, 0, pathSize);
		return err;
	}
	return 0;
}

int npTrophyPopPathComponent(char *path, SceSize pathSize, char sep)
{
	(void)pathSize;
	SceSize len = sceClibStrnlen(path, 0x7FFFFFFFu);
	char *cursor = path + len;
	while (cursor >= path) {
		if (*cursor == sep) {
			*cursor = '\0';
			return 0;
		}
		--cursor;
	}
	return NP_TROPHY_ERR_GENERIC;
}

int npTrophyPathInit(char *outPath, int count, ...)
{
	va_list ap;
	va_start(ap, count);
	int err = npTrophyBuildPathV(outPath, 256, '/', count, ap);
	va_end(ap);
	return err;
}

int npTrophyPathAppend(char *path, int count, ...)
{
	va_list ap;
	va_start(ap, count);
	int err = npTrophyAppendPathComponentsV(path, 256, '/', count, ap);
	va_end(ap);
	return err;
}

int npTrophyPathPop(char *path)
{
	int err = npTrophyPopPathComponent(path, 256, '/');
	return (err < 0) ? err : 0;
}

int npTrophyUr0Path(char *outPath, int userId, int count, ...)
{
	int err = npTrophyGetUr0UserDir(userId, outPath, 256);
	if (err < 0)
		return err;
	va_list ap;
	va_start(ap, count);
	err = npTrophyAppendPathComponentsV(outPath, 256, '/', count, ap);
	va_end(ap);
	return err;
}

int npTrophyUr0ConfDirPath(char *outPath, int userId, const TrophyCommId *commId,
			   const char *suffix)
{
	char commDir[32];
	int err = npTrophyFormatCommDirName(commDir, commId, suffix);
	if (err < 0)
		return err;
	return npTrophyUr0Path(outPath, userId, 3, "trophy", "conf", commDir);
}

int npTrophyUr0ConfFilePath(char *outPath, int userId, const TrophyCommId *commId,
			    const char *filename, const char *suffix)
{
	char commDir[32];
	int err = npTrophyFormatCommDirName(commDir, commId, suffix);
	if (err < 0)
		return err;
	return npTrophyUr0Path(outPath, userId, 4, "trophy", "conf", commDir, filename);
}

int npTrophyUr0DataRootPath(char *outPath, int userId)
{
	return npTrophyUr0Path(outPath, userId, 2, "trophy", "data");
}

int npTrophyUr0DataDirPath(char *outPath, int userId, const TrophyCommId *commId,
			   const char *suffix)
{
	char commDir[32];
	int err = npTrophyFormatCommDirName(commDir, commId, suffix);
	if (err < 0)
		return err;
	return npTrophyUr0Path(outPath, userId, 3, "trophy", "data", commDir);
}

int npTrophyEnsureUr0ConfRoot(int userId)
{
	char userRoot[256];
	char trophyRoot[256];
	char confRoot[256];
	int err = npTrophyUr0Path(userRoot, userId, 0);
	if (err >= 0)
		err = npTrophyEnsureDir(userRoot, 6);
	if (err >= 0)
		err = npTrophyUr0Path(trophyRoot, userId, 1, "trophy");
	if (err >= 0)
		err = npTrophyEnsureDir(trophyRoot, 6);
	if (err >= 0)
		err = npTrophyUr0Path(confRoot, userId, 2, "trophy", "conf");
	if (err >= 0)
		err = npTrophyEnsureDir(confRoot, 6);
	return (err < 0) ? err : 0;
}

int npTrophyEnsureUr0ConfDir(int userId, const TrophyCommId *commId, const char *suffix)
{
	char dirPath[256];
	int err = npTrophyEnsureUr0ConfRoot(userId);
	if (err < 0)
		return err;
	err = npTrophyUr0ConfDirPath(dirPath, userId, commId, suffix);
	if (err < 0)
		return err;
	err = npTrophyEnsureDir(dirPath, 6);
	return (err < 0) ? err : 0;
}

int npTrophyRemoveUr0ConfRoot(int userId)
{
	char path[256];
	int err = npTrophyUr0Path(path, userId, 2, "trophy", "conf");
	if (err < 0)
		return err;
	err = npTrophyRemoveTree(path);
	return (err < 0) ? err : 0;
}

int npTrophyRemoveUr0ConfDir(int userId, const TrophyCommId *commId, const char *suffix)
{
	char path[256];
	int err = npTrophyUr0ConfDirPath(path, userId, commId, suffix);
	if (err < 0)
		return err;
	err = npTrophyRemoveTree(path);
	return (err < 0) ? err : 0;
}

int npTrophyRemoveUr0DataRoot(int userId)
{
	char path[256];
	int err = npTrophyUr0Path(path, userId, 2, "trophy", "data");
	if (err < 0)
		return err;
	err = npTrophyRemoveTree(path);
	return (err < 0) ? err : 0;
}

int npTrophyRemoveUr0SceTropDir(int userId)
{
	char path[256];
	int err = npTrophyUr0Path(path, userId, 3, "trophy", "data", "sce_trop");
	if (err < 0)
		return err;
	err = npTrophyRemoveTree(path);
	return (err < 0) ? err : 0;
}

int npTrophyRemoveUr0DataDir(int userId, const TrophyCommId *commId, const char *suffix)
{
	char path[256];
	int err = npTrophyUr0DataDirPath(path, userId, commId, suffix);
	if (err < 0)
		return err;
	err = npTrophyRemoveTree(path);
	return (err < 0) ? err : 0;
}

int npTrophyRenameUr0ConfDir(int userId, const TrophyCommId *commId, const char *oldSuffix,
			     const char *newSuffix)
{
	char oldPath[256];
	char newPath[256];
	int err = npTrophyUr0ConfDirPath(oldPath, userId, commId, oldSuffix);
	if (err < 0)
		return err;
	err = npTrophyUr0ConfDirPath(newPath, userId, commId, newSuffix);
	if (err < 0)
		return err;
	err = npTrophyRenamePath(oldPath, newPath);
	return (err < 0) ? err : 0;
}

int npTrophyRenameUr0DataDir(int userId, const TrophyCommId *commId, const char *oldSuffix,
			     const char *newSuffix)
{
	char oldPath[256];
	char newPath[256];
	int err = npTrophyUr0DataDirPath(oldPath, userId, commId, oldSuffix);
	if (err < 0)
		return err;
	err = npTrophyUr0DataDirPath(newPath, userId, commId, newSuffix);
	if (err < 0)
		return err;
	err = npTrophyRenamePath(oldPath, newPath);
	return (err < 0) ? err : 0;
}

int npTrophyJoinBasePath(char *outPath, const char *basePath, int count, ...)
{
	if (npTrophyStrlcpy(outPath, basePath, 256) >= 256)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	va_list ap;
	va_start(ap, count);
	int err = npTrophyAppendPathComponentsV(outPath, 256, '/', count, ap);
	va_end(ap);
	return err;
}

int npTrophyJoinBaseFile(char *outPath, const char *basePath, const char *filename)
{
	int err = npTrophyJoinBasePath(outPath, basePath, 1, filename);
	return (err < 0) ? err : 0;
}

int npTrophyUx0Path(char *outPath, int userId, int count, ...)
{
	int err = npTrophyGetUx0UserDir(userId, outPath, 256);
	if (err < 0)
		return err;
	va_list ap;
	va_start(ap, count);
	err = npTrophyAppendPathComponentsV(outPath, 256, '/', count, ap);
	va_end(ap);
	return err;
}

int npTrophyUx0ConfFilePath(char *outPath, int userId, const TrophyCommId *commId,
			    const char *filename, const char *suffix)
{
	char commDir[32];
	int err = npTrophyFormatCommDirName(commDir, commId, suffix);
	if (err < 0)
		return err;
	return npTrophyUx0Path(outPath, userId, 4, "trophy", "conf", commDir, filename);
}

int npTrophyUx0RootExists(int userId, bool *outExists)
{
	char path[256];
	int err = npTrophyUx0Path(path, userId, 0);
	if (err < 0)
		return err;
	err = npTrophyPathExists(path, outExists);
	return (err < 0) ? err : 0;
}

int npTrophyUx0ConfDirExists(int userId, const TrophyCommId *commId, bool *outExists,
			     const char *suffix)
{
	char commDir[32];
	char path[256];
	int err = npTrophyFormatCommDirName(commDir, commId, suffix);
	if (err < 0)
		return err;
	err = npTrophyUx0Path(path, userId, 3, "trophy", "conf", commDir);
	if (err < 0)
		return err;
	err = npTrophyPathExists(path, outExists);
	return (err < 0) ? err : 0;
}

int npTrophyEnsureUx0ConfRoot(int userId)
{
	char userRoot[256];
	char trophyRoot[256];
	char confRoot[256];
	int err = npTrophyUx0Path(userRoot, userId, 0);
	if (err >= 0)
		err = npTrophyEnsureDir(userRoot, 6);
	if (err >= 0)
		err = npTrophyUx0Path(trophyRoot, userId, 1, "trophy");
	if (err >= 0)
		err = npTrophyEnsureDir(trophyRoot, 6);
	if (err >= 0)
		err = npTrophyUx0Path(confRoot, userId, 2, "trophy", "conf");
	if (err >= 0)
		err = npTrophyEnsureDir(confRoot, 6);
	return (err < 0) ? err : 0;
}

int npTrophyEnsureUx0ConfDir(int userId, const TrophyCommId *commId, const char *suffix)
{
	char commDir[32];
	char dirPath[256];
	int err = npTrophyEnsureUx0ConfRoot(userId);
	if (err < 0)
		return err;
	err = npTrophyFormatCommDirName(commDir, commId, suffix);
	if (err < 0)
		return err;
	err = npTrophyUx0Path(dirPath, userId, 3, "trophy", "conf", commDir);
	if (err < 0)
		return err;
	err = npTrophyEnsureDir(dirPath, 6);
	return (err < 0) ? err : 0;
}

int npTrophyRemoveUx0ConfRoot(int userId)
{
	char path[256];
	int err = npTrophyUx0Path(path, userId, 2, "trophy", "conf");
	if (err < 0)
		return err;
	err = npTrophyRemoveTree(path);
	return (err < 0) ? err : 0;
}

int npTrophyRemoveUx0ConfDir(int userId, const TrophyCommId *commId, const char *suffix)
{
	char commDir[32];
	char path[256];
	int err = npTrophyFormatCommDirName(commDir, commId, suffix);
	if (err < 0)
		return err;
	err = npTrophyUx0Path(path, userId, 3, "trophy", "conf", commDir);
	if (err < 0)
		return err;
	err = npTrophyRemoveTree(path);
	return (err < 0) ? err : 0;
}

int npTrophyRenameUx0ConfDir(int userId, const TrophyCommId *commId, const char *oldSuffix,
			     const char *newSuffix)
{
	char oldDir[32];
	char newDir[32];
	char oldPath[256];
	char newPath[256];
	int err = npTrophyFormatCommDirName(oldDir, commId, oldSuffix);
	if (err < 0)
		return err;
	err = npTrophyUx0Path(oldPath, userId, 3, "trophy", "conf", oldDir);
	if (err < 0)
		return err;
	err = npTrophyFormatCommDirName(newDir, commId, newSuffix);
	if (err < 0)
		return err;
	err = npTrophyUx0Path(newPath, userId, 3, "trophy", "conf", newDir);
	if (err < 0)
		return err;
	err = npTrophyRenamePath(oldPath, newPath);
	return (err < 0) ? err : 0;
}

/* --- TrophyCancelToken, TrophyProgress & TrophyHandle (0x812E9314..0x812E985B) --- */

TrophyCancelToken::TrophyCancelToken()
	: m_cancelError(0), m_cancelEnabled(1), m_noCancelDepth(0), m_child(NULL)
{
}

TrophyCancelToken::~TrophyCancelToken()
{
}

int TrophyCancelToken::Init()
{
	int err = m_mutex.Init("SceNpTrophyHandle", true);
	return (err < 0) ? err : 0;
}

void TrophyCancelToken::Term()
{
	m_mutex.Term();
}

void TrophyCancelToken::PushNoCancel()
{
	m_mutex.Lock();
	++m_noCancelDepth;
	if (m_child != NULL)
		m_child->PushNoCancel();
	m_mutex.Unlock();
}

void TrophyCancelToken::PopNoCancel()
{
	m_mutex.Lock();
	if (m_child != NULL)
		m_child->PopNoCancel();
	--m_noCancelDepth;
	m_mutex.Unlock();
}

int TrophyCancelToken::GetCancelError()
{
	m_mutex.Lock();
	int err = 0;
	if (m_cancelEnabled != 0 && m_noCancelDepth == 0)
		err = m_cancelError;
	m_mutex.Unlock();
	return err;
}

void TrophyCancelToken::Cancel(int errorCode)
{
	m_mutex.Lock();
	m_cancelError = errorCode;
	if (m_child != NULL)
		m_child->Cancel(errorCode);
	if (m_cancelEnabled != 0 && m_noCancelDepth == 0)
		OnCancel(errorCode);
	m_mutex.Unlock();
}

void TrophyCancelToken::ClearSuspended()
{
	m_mutex.Lock();
	if (m_cancelError == NP_TROPHY_ERR_SUSPENDED)
		m_cancelError = 0;
	m_mutex.Unlock();
}

int TrophyCancelToken::SetChild(TrophyCancelToken *child)
{
	m_mutex.Lock();
	if (m_cancelEnabled != 0 && m_noCancelDepth == 0 && m_cancelError < 0) {
		int err = m_cancelError;
		m_mutex.Unlock();
		return err;
	}
	if (m_child != NULL) {
		m_mutex.Unlock();
		return NP_TROPHY_ERR_BUSY;
	}
	m_child = child;
	m_mutex.Unlock();
	return 0;
}

void TrophyCancelToken::ClearChild(TrophyCancelToken *child)
{
	m_mutex.Lock();
	if (child == NULL || m_child == child)
		m_child = NULL;
	m_mutex.Unlock();
}

void TrophyProgress::Advance(int delta)
{
	completedSteps += delta;
	if (completedSteps > totalSteps)
		completedSteps = totalSteps;
}

int TrophyProgress::GetPercent() const
{
	if (totalSteps == 0)
		return 0;
	return (100 * completedSteps) / totalSteps;
}

TrophyHandle::TrophyHandle()
	: m_refCount(0)
{
	m_progress.Reset();
}

TrophyHandle::~TrophyHandle()
{
}

int TrophyHandle::Init()
{
	return m_cancelToken.Init();
}

void TrophyHandle::Term()
{
	m_cancelToken.Term();
}

void TrophyHandle::AddRef()
{
	npTrophyAtomicInc(&m_refCount);
}

void TrophyHandle::ReleaseRef()
{
	npTrophyAtomicDec(&m_refCount);
}

void TrophyHandle::SetProgress(const TrophyProgress *progress)
{
	m_cancelToken.PushNoCancel();
	if (progress != NULL)
		m_progress = *progress;
	else
		m_progress.Reset();
	m_cancelToken.PopNoCancel();
}

int TrophyHandle::AdvanceProgress(int delta)
{
	m_cancelToken.PushNoCancel();
	m_progress.Advance(delta);
	int pct = m_progress.GetPercent();
	m_cancelToken.PopNoCancel();
	return pct;
}

int TrophyHandle::CompleteProgress()
{
	m_cancelToken.PushNoCancel();
	m_progress.Complete();
	int pct = m_progress.GetPercent();
	m_cancelToken.PopNoCancel();
	return pct;
}

int TrophyHandle::GetProgressPercent()
{
	m_cancelToken.PushNoCancel();
	int pct = m_progress.GetPercent();
	m_cancelToken.PopNoCancel();
	return pct;
}

TrophyNoCancelGuard::TrophyNoCancelGuard(TrophyHandle *handle)
	: m_handle(handle)
{
	if (m_handle != NULL)
		m_handle->GetCancelToken()->PushNoCancel();
}

TrophyNoCancelGuard::~TrophyNoCancelGuard()
{
	if (m_handle != NULL)
		m_handle->GetCancelToken()->PopNoCancel();
}

TrophyScopedCancelToken::TrophyScopedCancelToken()
	: m_parentHandle(NULL)
{
}

TrophyScopedCancelToken::~TrophyScopedCancelToken()
{
	if (m_parentHandle != NULL)
		m_parentHandle->GetCancelToken()->ClearChild(this);
}

int TrophyScopedCancelToken::Attach(TrophyHandle *handle)
{
	int err = Init();
	if (err < 0)
		return err;
	err = handle->GetCancelToken()->SetChild(this);
	if (err < 0) {
		Term();
		return err;
	}
	m_parentHandle = handle;
	return 0;
}

int TrophyScopedCancelToken::Detach(int operationResult)
{
	if (m_parentHandle != NULL) {
		m_parentHandle->GetCancelToken()->ClearChild(this);
		m_parentHandle = NULL;
	}
	int cancelErr = GetCancelError();
	int result = (cancelErr < 0) ? cancelErr : operationResult;
	Term();
	return result;
}

/* --- TrophyFileBase & TrophyFile (0x812E813C..0x812E838F, 0x812E8E8C..0x812E925A) --- */

int TrophyFileBase::MoveRange(TrophyHandle *handle, SceOff srcOffset, SceOff dstOffset,
			      SceOff byteCount, void *workBuf, SceSize workBufSize)
{
	if (workBufSize < 512)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	if (srcOffset == dstOffset || byteCount <= 0)
		return 0;

	if (srcOffset < dstOffset) {
		SceOff moved = 0;
		while (moved < byteCount) {
			SceOff remaining = byteCount - moved;
			SceSize chunk = (remaining <= static_cast<SceOff>(workBufSize))
						? static_cast<SceSize>(remaining)
						: workBufSize;
			SceOff readPos = srcOffset + remaining - chunk;
			SceOff writePos = dstOffset + remaining - chunk;
			int err = Seek(handle, readPos, SCE_SEEK_SET, NULL);
			if (err < 0)
				return err;
			err = npTrophyStreamReadExact(this, handle, workBuf, chunk, NULL);
			if (err < 0)
				return err;
			err = Seek(handle, writePos, SCE_SEEK_SET, NULL);
			if (err < 0)
				return err;
			err = npTrophyStreamWriteExact(this, handle, workBuf, chunk, NULL);
			if (err < 0)
				return err;
			moved += chunk;
		}
		return 0;
	}

	SceOff moved = 0;
	while (moved < byteCount) {
		SceOff remaining = byteCount - moved;
		SceSize chunk = (remaining <= static_cast<SceOff>(workBufSize))
					? static_cast<SceSize>(remaining)
					: workBufSize;
		int err = Seek(handle, srcOffset + moved, SCE_SEEK_SET, NULL);
		if (err < 0)
			return err;
		err = npTrophyStreamReadExact(this, handle, workBuf, chunk, NULL);
		if (err < 0)
			return err;
		err = Seek(handle, dstOffset + moved, SCE_SEEK_SET, NULL);
		if (err < 0)
			return err;
		err = npTrophyStreamWriteExact(this, handle, workBuf, chunk, NULL);
		if (err < 0)
			return err;
		moved += chunk;
	}
	return 0;
}

TrophyFile::TrophyFile()
	: m_fileFd(-1)
{
}

TrophyFile::~TrophyFile()
{
	if (m_fileFd != -1) {
		if (sceIoClose(m_fileFd) >= 0)
			m_fileFd = -1;
	}
}

bool TrophyFile::IsOpen() const
{
	return m_fileFd != -1;
}

int TrophyFile::Open(const char *path, int flags, SceMode mode)
{
	if (IsOpen())
		return NP_TROPHY_ERR_FILE_ALREADY_OPEN;

	SceUID fd = sceIoOpen(path, flags, mode);
	if (fd < 0) {
		if (m_fileFd != -1) {
			sceIoClose(m_fileFd);
			m_fileFd = -1;
		}
		if (fd == SCE_IO_ERR_ENOENT)
			return NP_TROPHY_ERR_FILE_NOT_FOUND;
		return fd;
	}
	m_fileFd = fd;
	return 0;
}

int TrophyFile::Close()
{
	if (!IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	int err = sceIoClose(m_fileFd);
	if (err < 0)
		return err;
	m_fileFd = -1;
	return 0;
}

int TrophyFile::GetSize(SceOff *outSize)
{
	if (!IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;

	SceOff savedPos = sceIoLseek(m_fileFd, 0, SCE_SEEK_CUR);
	if (savedPos < 0) {
		int err = static_cast<int>(savedPos);
		return (err == SCE_IO_ERR_ENOENT) ? NP_TROPHY_ERR_FILE_NOT_FOUND : err;
	}

	SceOff endPos = sceIoLseek(m_fileFd, 0, SCE_SEEK_END);
	if (endPos < 0) {
		int err = static_cast<int>(endPos);
		return (err == SCE_IO_ERR_ENOENT) ? NP_TROPHY_ERR_FILE_NOT_FOUND : err;
	}

	SceOff restorePos = sceIoLseek(m_fileFd, savedPos, SCE_SEEK_SET);
	if (restorePos < 0) {
		int err = static_cast<int>(restorePos);
		return (err == SCE_IO_ERR_ENOENT) ? NP_TROPHY_ERR_FILE_NOT_FOUND : err;
	}

	*outSize = endPos;
	return 0;
}

int TrophyFile::Seek(TrophyHandle *handle, SceOff offset, int whence, SceOff *outPos)
{
	(void)handle;
	if (!IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	if (whence < 0 || whence > 2)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	SceOff newPos = sceIoLseek(m_fileFd, offset, whence);
	if (newPos < 0) {
		int err = static_cast<int>(newPos);
		return (err == SCE_IO_ERR_ENOENT) ? NP_TROPHY_ERR_FILE_NOT_FOUND : err;
	}
	if (outPos != NULL)
		*outPos = newPos;
	return 0;
}

int TrophyFile::Truncate(TrophyHandle *handle, SceOff length)
{
	(void)handle;
	(void)length;
	return IsOpen() ? NP_TROPHY_ERR_GENERIC : NP_TROPHY_ERR_FILE_NOT_OPEN;
}

int TrophyFile::Sync(TrophyHandle *handle)
{
	(void)handle;
	return IsOpen() ? NP_TROPHY_ERR_GENERIC : NP_TROPHY_ERR_FILE_NOT_OPEN;
}

int TrophyFile::Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead)
{
	(void)handle;
	if (!IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	SceSSize got = sceIoRead(m_fileFd, outBuf, size);
	if (got < 0) {
		if (got == SCE_IO_ERR_ENOENT)
			return NP_TROPHY_ERR_FILE_NOT_FOUND;
		return static_cast<int>(got);
	}
	*outRead = static_cast<SceSize>(got);
	return 0;
}

int TrophyFile::Write(TrophyHandle *handle, const void *srcBuf, SceSize size, SceSize *outWritten)
{
	(void)handle;
	if (!IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	SceSSize wrote = sceIoWrite(m_fileFd, srcBuf, size);
	if (wrote < 0) {
		if (wrote == SCE_IO_ERR_ENOENT)
			return NP_TROPHY_ERR_FILE_NOT_FOUND;
		return static_cast<int>(wrote);
	}
	*outWritten = static_cast<SceSize>(wrote);
	return 0;
}

/* --- TrophyMemFile (0x812EFD40..0x812F0207) --- */

TrophyMemFile::TrophyMemFile()
	: m_isOpen(0), m_maxSize(0), m_growStep(0), m_capacity(0),
	  m_buffer(NULL), m_size(0), m_position(0), m_dirty(0)
{
}

TrophyMemFile::~TrophyMemFile()
{
	if (m_isOpen != 0)
		Close();
}

int TrophyMemFile::Open(const TrophyMemFileConfig *config)
{
	if (IsOpen())
		return NP_TROPHY_ERR_FILE_ALREADY_OPEN;

	uint8_t *buf = NULL;
	if (config->initialSize != 0) {
		buf = static_cast<uint8_t *>(npTrophyHeapAlloc(config->initialSize));
		if (buf == NULL)
			return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}
	m_maxSize = config->maxSize;
	m_growStep = config->growStep;
	m_capacity = config->initialSize;
	m_buffer = buf;
	m_size = 0;
	m_position = 0;
	m_isOpen = 1;
	m_dirty = 0;
	return 0;
}

int TrophyMemFile::Close()
{
	if (!IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	if (m_buffer != NULL) {
		npTrophyHeapFree(m_buffer);
		m_buffer = NULL;
	}
	m_isOpen = 0;
	m_dirty = 0;
	return 0;
}

int TrophyMemFile::GetSize(SceOff *outSize)
{
	if (!IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	*outSize = static_cast<SceOff>(m_size);
	return 0;
}

int TrophyMemFile::Seek(TrophyHandle *handle, SceOff offset, int whence, SceOff *outPos)
{
	(void)handle;
	if (!IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;

	SceOff target = 0;
	if (whence == SCE_SEEK_SET)
		target = offset;
	else if (whence == SCE_SEEK_CUR)
		target = static_cast<SceOff>(m_position) + offset;
	else if (whence == SCE_SEEK_END)
		target = static_cast<SceOff>(m_size) + offset;
	else
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	if (target < 0 || target > static_cast<SceOff>(m_size))
		return NP_TROPHY_ERR_GENERIC;

	m_position = static_cast<SceSize>(target);
	if (outPos != NULL)
		*outPos = target;
	return 0;
}

int TrophyMemFile::Truncate(TrophyHandle *handle, SceOff length)
{
	(void)handle;
	if (!IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	if (length > static_cast<SceOff>(m_maxSize))
		return NP_TROPHY_ERR_GENERIC;
	m_dirty = 1;
	m_size = static_cast<SceSize>(length);
	return 0;
}

int TrophyMemFile::Sync(TrophyHandle *handle)
{
	(void)handle;
	return NP_TROPHY_ERR_GENERIC;
}

int TrophyMemFile::Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead)
{
	(void)handle;
	if (!IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;

	if (m_position >= m_size) {
		*outRead = 0;
		return 0;
	}
	SceSize avail = m_size - m_position;
	SceSize toRead = (avail < size) ? avail : size;
	sceClibMemcpy_safe(outBuf, m_buffer + m_position, toRead);
	m_position += toRead;
	*outRead = toRead;
	return 0;
}

int TrophyMemFile::Write(TrophyHandle *handle, const void *srcBuf, SceSize size,
			 SceSize *outWritten)
{
	(void)handle;
	if (!IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	if (m_position > m_size)
		return NP_TROPHY_ERR_GENERIC;

	while (m_position + size > m_capacity) {
		if (m_capacity + m_growStep > m_maxSize)
			return NP_TROPHY_ERR_MEMFILE_LIMIT;
		SceSize nextCap = m_capacity + m_growStep;
		uint8_t *grown = static_cast<uint8_t *>(npTrophyHeapRealloc(m_buffer, nextCap));
		if (grown == NULL)
			return NP_TROPHY_ERR_OUT_OF_MEMORY;
		m_buffer = grown;
		m_capacity = nextCap;
	}

	sceClibMemcpy_safe(m_buffer + m_position, srcBuf, size);
	m_position += size;
	if (m_position > m_size)
		m_size = m_position;
	*outWritten = size;
	m_dirty = 1;
	return 0;
}

int TrophyMemFile::LoadFromFile(TrophyHandle *handle, TrophyFileBase *source)
{
	SceOff fileSize = 0;
	int err = source->GetSize(&fileSize);
	if (err < 0)
		return err;

	while (fileSize > static_cast<SceOff>(m_capacity)) {
		if (m_capacity + m_growStep > m_maxSize)
			return NP_TROPHY_ERR_MEMFILE_LIMIT;
		SceSize nextCap = m_capacity + m_growStep;
		uint8_t *grown = static_cast<uint8_t *>(npTrophyHeapRealloc(m_buffer, nextCap));
		if (grown == NULL)
			return NP_TROPHY_ERR_OUT_OF_MEMORY;
		m_buffer = grown;
		m_capacity = nextCap;
	}

	err = source->Seek(handle, 0, SCE_SEEK_SET, NULL);
	if (err < 0)
		return err;
	err = npTrophyStreamReadExact(source, handle, m_buffer, static_cast<SceSize>(fileSize),
				      NULL);
	if (err < 0)
		return err;

	m_position = 0;
	m_size = static_cast<SceSize>(fileSize);
	return 0;
}

int TrophyMemFile::FlushToFile(TrophyHandle *handle, TrophyFileBase *dest)
{
	int err = dest->Seek(handle, 0, SCE_SEEK_SET, NULL);
	if (err < 0)
		return err;
	err = npTrophyStreamWriteExact(dest, handle, m_buffer, m_size, NULL);
	if (err < 0)
		return err;
	m_dirty = 0;
	return 0;
}

/* --- TrophyRecordFile (0x812EE494..0x812EF587) --- */

struct TrophyRecordFileHeaderOnDisk {
	uint32_t magic;
	uint32_t formatVersion;
	uint32_t sectionCount;
	uint32_t reserved[9];
};

struct TrophyRecordSectionOnDisk {
	uint32_t sectionId;
	uint32_t recordSize;
	uint32_t sectionKind;
	uint32_t recordCount;
	uint64_t fileOffset;
	uint64_t reserved;
};

struct TrophyRecordEntryHeaderOnDisk {
	uint32_t sectionId;
	uint32_t recordSize;
	uint32_t reserved[2];
};

TrophyRecordFile::TrophyRecordFile(uint32_t magic)
	: m_diskFile(NULL), m_memFile(NULL), m_activeFile(NULL),
	  m_magic(magic), m_sectionCount(0), m_sections(NULL)
{
}

TrophyRecordFile::~TrophyRecordFile()
{
	if (m_memFile != NULL) {
		delete m_memFile;
		m_memFile = NULL;
	}
	if (m_diskFile != NULL) {
		delete m_diskFile;
		m_diskFile = NULL;
	}
	if (m_sections != NULL) {
		npTrophyHeapFree(m_sections);
		m_sections = NULL;
	}
	m_sectionCount = 0;
	m_activeFile = NULL;
}

void TrophyRecordFile::CleanupFiles()
{
	if (m_memFile != NULL) {
		if (m_memFile->IsOpen())
			m_memFile->Close();
		delete m_memFile;
		m_memFile = NULL;
	}
	if (m_diskFile != NULL) {
		if (m_diskFile->IsOpen())
			m_diskFile->Close();
		delete m_diskFile;
		m_diskFile = NULL;
	}
	m_activeFile = NULL;
}

int TrophyRecordFile::WriteInitialLayout(TrophyHandle *handle,
					 const TrophyRecordSectionSpec *sections, int sectionCount)
{
	m_sections = static_cast<TrophyRecordSectionMeta *>(
		npTrophyHeapAlloc(sizeof(TrophyRecordSectionMeta) * sectionCount));
	if (m_sections == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	m_sectionCount = static_cast<uint32_t>(sectionCount);

	TrophyRecordFileHeaderOnDisk diskHdr;
	sceClibMemset(&diskHdr, 0, sizeof(diskHdr));
	diskHdr.magic = sceNetHtonl(m_magic);
	diskHdr.formatVersion = sceNetHtonl(0x10000u);
	diskHdr.sectionCount = sceNetHtonl(m_sectionCount);

	int err = m_activeFile->Seek(handle, 0, SCE_SEEK_SET, NULL);
	if (err >= 0)
		err = npTrophyStreamWriteExact(m_activeFile, handle, &diskHdr, sizeof(diskHdr),
					       NULL);

	uint64_t nextOffset = sizeof(TrophyRecordFileHeaderOnDisk) +
			      static_cast<uint64_t>(sectionCount) *
				      sizeof(TrophyRecordSectionOnDisk);

	for (int index = 0; err >= 0 && index < sectionCount; ++index) {
		TrophyRecordSectionMeta *meta = &m_sections[index];
		sceClibMemset(meta, 0, sizeof(*meta));
		meta->sectionId = sections[index].sectionId;
		meta->recordSize = sections[index].recordSize;
		meta->sectionKind = sections[index].sectionKind;
		meta->recordCount = sections[index].recordCount;
		meta->fileOffset = nextOffset;

		TrophyRecordSectionOnDisk diskMeta;
		sceClibMemset(&diskMeta, 0, sizeof(diskMeta));
		diskMeta.sectionId = sceNetHtonl(meta->sectionId);
		diskMeta.recordSize = sceNetHtonl(meta->recordSize);
		diskMeta.sectionKind = sceNetHtonl(meta->sectionKind);
		diskMeta.recordCount = sceNetHtonl(meta->recordCount);
		diskMeta.fileOffset = sceNetHtonll(meta->fileOffset);

		err = npTrophyStreamWriteExact(m_activeFile, handle, &diskMeta, sizeof(diskMeta),
					       NULL);
		nextOffset += static_cast<uint64_t>(trophyRecordStride(sections[index].recordSize)) *
			      sections[index].recordCount;
	}

	for (int index = 0; err >= 0 && index < sectionCount; ++index) {
		const TrophyRecordSectionSpec &spec = sections[index];
		SceSize padLen = trophyRecordPadLen(spec.recordSize);
		for (uint32_t recIdx = 0; err >= 0 && recIdx < spec.recordCount; ++recIdx) {
			TrophyRecordEntryHeaderOnDisk recHdr;
			sceClibMemset(&recHdr, 0, sizeof(recHdr));
			recHdr.sectionId = sceNetHtonl(spec.sectionId);
			recHdr.recordSize = sceNetHtonl(spec.recordSize);
			err = npTrophyStreamWriteExact(m_activeFile, handle, &recHdr,
						       sizeof(recHdr), NULL);
			if (err < 0)
				break;

			if (spec.initialRecordBuf != NULL) {
				if (spec.initCallback != NULL) {
					err = spec.initCallback(spec.initialRecordBuf,
								spec.recordSize, recIdx,
								spec.initUserCtx);
				}
				if (err >= 0) {
					err = npTrophyStreamWriteExact(m_activeFile, handle,
								       spec.initialRecordBuf,
								       spec.recordSize, NULL);
				}
			} else {
				err = npTrophyStreamFill(m_activeFile, handle, 0, spec.recordSize);
			}

			if (err >= 0 && padLen != 0)
				err = npTrophyStreamFill(m_activeFile, handle, 0, padLen);
		}
	}

	if (err < 0) {
		npTrophyHeapFree(m_sections);
		m_sections = NULL;
		m_sectionCount = 0;
		return err;
	}
	return 0;
}

int TrophyRecordFile::ReadHeaderAndDirectory(TrophyHandle *handle)
{
	int err = m_activeFile->Seek(handle, 0, SCE_SEEK_SET, NULL);
	if (err < 0)
		return err;

	TrophyRecordFileHeaderOnDisk diskHdr;
	err = npTrophyStreamReadExact(m_activeFile, handle, &diskHdr, sizeof(diskHdr), NULL);
	if (err == NP_TROPHY_ERR_UNEXPECTED_EOF)
		return NP_TROPHY_ERR_RECORD_TRUNCATED;
	if (err < 0)
		return err;

	uint32_t magic = sceNetNtohl(diskHdr.magic);
	uint32_t version = sceNetNtohl(diskHdr.formatVersion);
	uint32_t sectionCount = sceNetNtohl(diskHdr.sectionCount);
	if (magic != m_magic)
		return NP_TROPHY_ERR_RECORD_BAD_MAGIC;
	if (version != 0x10000u)
		return NP_TROPHY_ERR_RECORD_BAD_VERSION;

	m_sections = static_cast<TrophyRecordSectionMeta *>(
		npTrophyHeapAlloc(sizeof(TrophyRecordSectionMeta) * sectionCount));
	if (m_sections == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	m_sectionCount = sectionCount;

	for (uint32_t index = 0; index < sectionCount; ++index) {
		TrophyRecordSectionOnDisk diskMeta;
		err = npTrophyStreamReadExact(m_activeFile, handle, &diskMeta, sizeof(diskMeta),
					      NULL);
		if (err == NP_TROPHY_ERR_UNEXPECTED_EOF)
			return NP_TROPHY_ERR_RECORD_TRUNCATED;
		if (err < 0)
			return err;

		TrophyRecordSectionMeta *meta = &m_sections[index];
		sceClibMemset(meta, 0, sizeof(*meta));
		meta->sectionId = sceNetNtohl(diskMeta.sectionId);
		meta->recordSize = sceNetNtohl(diskMeta.recordSize);
		meta->sectionKind = sceNetNtohl(diskMeta.sectionKind);
		meta->recordCount = sceNetNtohl(diskMeta.recordCount);
		meta->fileOffset = sceNetNtohll(diskMeta.fileOffset);
	}
	return 0;
}

int TrophyRecordFile::Create(TrophyHandle *handle, const char *path, int flags, SceMode mode,
			     const TrophyRecordSectionSpec *sections, int sectionCount,
			     bool useMemCache)
{
	if (m_activeFile != NULL)
		return NP_TROPHY_ERR_RECORD_ALREADY_OPEN;

	TrophyMemFileConfig memCfg = { 102400u, 1024u, 1024u };
	m_diskFile = new TrophyFile();
	if (m_diskFile == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int err = m_diskFile->Open(path, flags, mode);
	if (err >= 0) {
		if (useMemCache) {
			m_memFile = new TrophyMemFile();
			if (m_memFile == NULL) {
				err = NP_TROPHY_ERR_OUT_OF_MEMORY;
			} else {
				err = m_memFile->Open(&memCfg);
				if (err >= 0)
					m_activeFile = m_memFile;
			}
		} else {
			m_activeFile = m_diskFile;
		}
		if (err >= 0) {
			err = WriteInitialLayout(handle, sections, sectionCount);
			if (err >= 0)
				return 0;
		}
	}

	CleanupFiles();
	return err;
}

int TrophyRecordFile::Open(TrophyHandle *handle, const char *path, int flags, SceMode mode,
			   bool useMemCache)
{
	if (m_activeFile != NULL)
		return NP_TROPHY_ERR_RECORD_ALREADY_OPEN;

	TrophyMemFileConfig memCfg = { 102400u, 1024u, 1024u };
	m_diskFile = new TrophyFile();
	if (m_diskFile == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int err = m_diskFile->Open(path, flags, mode);
	if (err >= 0) {
		if (useMemCache) {
			m_memFile = new TrophyMemFile();
			if (m_memFile == NULL) {
				err = NP_TROPHY_ERR_OUT_OF_MEMORY;
			} else {
				err = m_memFile->Open(&memCfg);
				if (err >= 0)
					err = m_memFile->LoadFromFile(handle, m_diskFile);
				if (err >= 0)
					m_activeFile = m_memFile;
			}
		} else {
			m_activeFile = m_diskFile;
		}
		if (err >= 0) {
			err = ReadHeaderAndDirectory(handle);
			if (err >= 0)
				return 0;
		}
	}

	CleanupFiles();
	return err;
}

int TrophyRecordFile::Close()
{
	if (m_activeFile == NULL)
		return NP_TROPHY_ERR_RECORD_NOT_OPEN;

	if (m_memFile != NULL && m_memFile->IsDirty()) {
		TrophyHandle flushHandle;
		int err = flushHandle.Init();
		if (err < 0)
			return err;
		err = m_memFile->FlushToFile(&flushHandle, m_diskFile);
		if (err >= 0)
			err = m_memFile->Close();
		if (err < 0)
			return err;
		delete m_memFile;
		m_memFile = NULL;
	}

	int err = m_diskFile->Close();
	if (err < 0)
		return err;

	delete m_diskFile;
	m_diskFile = NULL;
	if (m_sections != NULL) {
		npTrophyHeapFree(m_sections);
		m_sections = NULL;
	}
	m_sectionCount = 0;
	m_activeFile = NULL;
	return 0;
}

int TrophyRecordFile::FindSection(uint32_t sectionId, uint32_t *outSectionIndex) const
{
	if (m_activeFile == NULL)
		return NP_TROPHY_ERR_RECORD_NOT_OPEN;
	for (uint32_t index = 0; index < m_sectionCount; ++index) {
		if (m_sections[index].sectionId == sectionId) {
			*outSectionIndex = index;
			return 0;
		}
	}
	return NP_TROPHY_ERR_RECORD_SECTION_NOT_FOUND;
}

int TrophyRecordFile::GetRecordCount(uint32_t sectionIndex, uint32_t *outRecordCount) const
{
	if (m_activeFile == NULL)
		return NP_TROPHY_ERR_RECORD_NOT_OPEN;
	if (sectionIndex >= m_sectionCount)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	*outRecordCount = m_sections[sectionIndex].recordCount;
	return 0;
}

int TrophyRecordFile::AppendRecord(TrophyHandle *handle, uint32_t sectionIndex,
				   const void *recordBuf, SceSize recordSize,
				   uint32_t *outRecordIndex)
{
	if (m_activeFile == NULL)
		return NP_TROPHY_ERR_RECORD_NOT_OPEN;
	if (sectionIndex >= m_sectionCount)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyRecordSectionMeta *meta = &m_sections[sectionIndex];
	if (recordSize != meta->recordSize)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	uint32_t newRecordIndex = meta->recordCount;
	uint32_t stride = trophyRecordStride(meta->recordSize);
	SceSize padLen = trophyRecordPadLen(meta->recordSize);
	SceOff expectedEnd = static_cast<SceOff>(meta->fileOffset +
						 static_cast<uint64_t>(stride) * newRecordIndex);

	SceOff fileSize = 0;
	int err = m_activeFile->GetSize(&fileSize);
	if (err < 0)
		return err;
	if (fileSize != expectedEnd)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	err = m_activeFile->Seek(handle, expectedEnd, SCE_SEEK_SET, NULL);
	if (err < 0)
		return err;

	TrophyRecordEntryHeaderOnDisk recHdr;
	sceClibMemset(&recHdr, 0, sizeof(recHdr));
	recHdr.sectionId = sceNetHtonl(meta->sectionId);
	recHdr.recordSize = sceNetHtonl(meta->recordSize);

	err = npTrophyStreamWriteExact(m_activeFile, handle, &recHdr, sizeof(recHdr), NULL);
	if (err >= 0) {
		if (recordBuf != NULL)
			err = npTrophyStreamWriteExact(m_activeFile, handle, recordBuf,
						       meta->recordSize, NULL);
		else
			err = npTrophyStreamFill(m_activeFile, handle, 0, meta->recordSize);
	}
	if (err >= 0 && padLen != 0)
		err = npTrophyStreamFill(m_activeFile, handle, 0, padLen);

	if (err >= 0) {
		SceOff metaOffset = static_cast<SceOff>(sizeof(TrophyRecordFileHeaderOnDisk) +
							sectionIndex *
								sizeof(TrophyRecordSectionOnDisk));
		err = m_activeFile->Seek(handle, metaOffset, SCE_SEEK_SET, NULL);
		if (err >= 0) {
			++meta->recordCount;
			TrophyRecordSectionOnDisk diskMeta;
			sceClibMemset(&diskMeta, 0, sizeof(diskMeta));
			diskMeta.sectionId = sceNetHtonl(meta->sectionId);
			diskMeta.recordSize = sceNetHtonl(meta->recordSize);
			diskMeta.sectionKind = sceNetHtonl(meta->sectionKind);
			diskMeta.recordCount = sceNetHtonl(meta->recordCount);
			diskMeta.fileOffset = sceNetHtonll(meta->fileOffset);

			err = npTrophyStreamWriteExact(m_activeFile, handle, &diskMeta,
						       sizeof(diskMeta), NULL);
			if (err >= 0) {
				*outRecordIndex = newRecordIndex;
				return 0;
			}
		}
	}

	if (expectedEnd > 0)
		m_activeFile->Truncate(handle, expectedEnd);
	return err;
}

int TrophyRecordFile::MoveRecord(TrophyHandle *handle, uint32_t sectionIndex, uint32_t fromIndex,
				 uint32_t toIndex, void *workBuf, SceSize workBufSize)
{
	if (m_activeFile == NULL)
		return NP_TROPHY_ERR_RECORD_NOT_OPEN;
	if (fromIndex == toIndex)
		return 0;
	if (sectionIndex >= m_sectionCount)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyRecordSectionMeta *meta = &m_sections[sectionIndex];
	if (fromIndex >= meta->recordCount || toIndex >= meta->recordCount)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	uint32_t stride = trophyRecordStride(meta->recordSize);
	if (workBufSize < stride + 512u)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	uint8_t *savedRecord = static_cast<uint8_t *>(workBuf);
	uint8_t *moveScratch = savedRecord + stride;
	SceSize moveScratchSize = workBufSize - stride;

	SceOff fromOffset = static_cast<SceOff>(meta->fileOffset +
						static_cast<uint64_t>(stride) * fromIndex);
	SceOff toOffset = static_cast<SceOff>(meta->fileOffset +
					      static_cast<uint64_t>(stride) * toIndex);

	int err = m_activeFile->Seek(handle, fromOffset, SCE_SEEK_SET, NULL);
	if (err < 0)
		return err;
	err = npTrophyStreamReadExact(m_activeFile, handle, savedRecord, stride, NULL);
	if (err < 0)
		return err;

	uint32_t shiftSrcIndex = (fromIndex > toIndex) ? toIndex : (fromIndex + 1);
	uint32_t shiftDstIndex = (fromIndex > toIndex) ? (toIndex + 1) : fromIndex;
	uint32_t shiftCount = (fromIndex > toIndex) ? (fromIndex - toIndex) : (toIndex - fromIndex);

	err = m_activeFile->MoveRange(
		handle,
		static_cast<SceOff>(meta->fileOffset +
				    static_cast<uint64_t>(stride) * shiftSrcIndex),
		static_cast<SceOff>(meta->fileOffset +
				    static_cast<uint64_t>(stride) * shiftDstIndex),
		static_cast<SceOff>(static_cast<uint64_t>(stride) * shiftCount),
		moveScratch, moveScratchSize);
	if (err < 0)
		return err;

	err = m_activeFile->Seek(handle, toOffset, SCE_SEEK_SET, NULL);
	if (err < 0)
		return err;
	err = npTrophyStreamWriteExact(m_activeFile, handle, savedRecord, stride, NULL);
	return (err < 0) ? err : 0;
}

int TrophyRecordFile::WriteRecordRange(TrophyHandle *handle, uint32_t sectionIndex,
				       uint32_t recordIndex, SceSize byteOffset,
				       const void *srcBuf, SceSize byteCount)
{
	if (m_activeFile == NULL)
		return NP_TROPHY_ERR_RECORD_NOT_OPEN;
	if (sectionIndex >= m_sectionCount)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyRecordSectionMeta *meta = &m_sections[sectionIndex];
	if (recordIndex >= meta->recordCount)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	if (byteOffset + byteCount > meta->recordSize)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	SceOff recStart = static_cast<SceOff>(meta->fileOffset +
					      static_cast<uint64_t>(trophyRecordStride(
						      meta->recordSize)) *
						      recordIndex);
	int err = m_activeFile->Seek(handle, recStart, SCE_SEEK_SET, NULL);
	if (err < 0)
		return err;

	TrophyRecordEntryHeaderOnDisk recHdr;
	err = npTrophyStreamReadExact(m_activeFile, handle, &recHdr, sizeof(recHdr), NULL);
	if (err == NP_TROPHY_ERR_UNEXPECTED_EOF)
		return NP_TROPHY_ERR_RECORD_TRUNCATED;
	if (err < 0)
		return err;

	if (sceNetNtohl(recHdr.sectionId) != meta->sectionId ||
	    sceNetNtohl(recHdr.recordSize) != meta->recordSize)
		return NP_TROPHY_ERR_RECORD_BAD_MAGIC;

	err = m_activeFile->Seek(handle, recStart + sizeof(recHdr) + byteOffset, SCE_SEEK_SET,
				 NULL);
	if (err < 0)
		return err;
	err = npTrophyStreamWriteExact(m_activeFile, handle, srcBuf, byteCount, NULL);
	return (err < 0) ? err : 0;
}

int TrophyRecordFile::ReadRecordRange(TrophyHandle *handle, uint32_t sectionIndex,
				      uint32_t recordIndex, SceSize byteOffset,
				      void *outBuf, SceSize byteCount)
{
	if (m_activeFile == NULL)
		return NP_TROPHY_ERR_RECORD_NOT_OPEN;
	if (sectionIndex >= m_sectionCount)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyRecordSectionMeta *meta = &m_sections[sectionIndex];
	if (recordIndex >= meta->recordCount)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	if (byteOffset + byteCount > meta->recordSize)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	SceOff recStart = static_cast<SceOff>(meta->fileOffset +
					      static_cast<uint64_t>(trophyRecordStride(
						      meta->recordSize)) *
						      recordIndex);
	int err = m_activeFile->Seek(handle, recStart, SCE_SEEK_SET, NULL);
	if (err < 0)
		return err;

	TrophyRecordEntryHeaderOnDisk recHdr;
	err = npTrophyStreamReadExact(m_activeFile, handle, &recHdr, sizeof(recHdr), NULL);
	if (err == NP_TROPHY_ERR_UNEXPECTED_EOF)
		return NP_TROPHY_ERR_RECORD_TRUNCATED;
	if (err < 0)
		return err;

	if (sceNetNtohl(recHdr.sectionId) != meta->sectionId ||
	    sceNetNtohl(recHdr.recordSize) != meta->recordSize)
		return NP_TROPHY_ERR_RECORD_BAD_MAGIC;

	err = m_activeFile->Seek(handle, recStart + sizeof(recHdr) + byteOffset, SCE_SEEK_SET,
				 NULL);
	if (err < 0)
		return err;
	err = npTrophyStreamReadExact(m_activeFile, handle, outBuf, byteCount, NULL);
	if (err == NP_TROPHY_ERR_UNEXPECTED_EOF)
		return NP_TROPHY_ERR_RECORD_TRUNCATED;
	return (err < 0) ? err : 0;
}

/* --- TrophySimpleRecordFile (0x812EF588..0x812EF7B3) --- */

TrophySimpleRecordFile::TrophySimpleRecordFile(uint32_t magic)
	: m_recordFile(magic), m_sectionIndex(0)
{
}

TrophySimpleRecordFile::~TrophySimpleRecordFile()
{
}

int TrophySimpleRecordFile::SerializeHeaderCallback(void *recordBuf, SceSize recordSize,
						    uint32_t recordIndex, void *userCtx)
{
	(void)recordSize;
	(void)recordIndex;
	const TrophySimpleHeader *src = static_cast<const TrophySimpleHeader *>(userCtx);
	uint32_t *diskWords = static_cast<uint32_t *>(recordBuf);
	sceClibMemset(diskWords, 0, 48);
	diskWords[0] = sceNetHtonl(src->formatVersion);
	diskWords[1] = sceNetHtonl(src->flags);
	return 0;
}

int TrophySimpleRecordFile::Create(TrophyHandle *handle, const char *path, int flags,
				   SceMode mode, const TrophySimpleHeader *initialHeader)
{
	TrophyNoCancelGuard noCancel(handle);
	uint8_t recordBuf[48];
	TrophyRecordSectionSpec *spec = static_cast<TrophyRecordSectionSpec *>(
		npTrophyHeapAlloc(sizeof(TrophyRecordSectionSpec)));
	if (spec == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	spec->sectionId = 1;
	spec->recordSize = 48;
	spec->sectionKind = 1;
	spec->recordCount = 1;
	spec->initialRecordBuf = recordBuf;
	spec->initCallback = SerializeHeaderCallback;
	spec->initUserCtx = const_cast<TrophySimpleHeader *>(initialHeader);

	int err = m_recordFile.Create(handle, path, flags, mode, spec, 1, true);
	if (err >= 0)
		err = m_recordFile.FindSection(1, &m_sectionIndex);

	npTrophyHeapFree(spec);
	return (err < 0) ? err : 0;
}

int TrophySimpleRecordFile::Open(TrophyHandle *handle, const char *path, int flags, SceMode mode)
{
	int err = m_recordFile.Open(handle, path, flags, mode, true);
	if (err < 0)
		return err;
	err = m_recordFile.FindSection(1, &m_sectionIndex);
	if (err < 0) {
		if (m_recordFile.IsOpen())
			m_recordFile.Close();
		return err;
	}
	return 0;
}

int TrophySimpleRecordFile::Close()
{
	if (!m_recordFile.IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	int err = m_recordFile.Close();
	return (err < 0) ? err : 0;
}

int TrophySimpleRecordFile::ReadHeader(TrophyHandle *handle, TrophySimpleHeader *outHeader)
{
	uint32_t diskWords[12];
	int err = m_recordFile.ReadRecordRange(handle, m_sectionIndex, 0, 0, diskWords,
					       sizeof(diskWords));
	if (err < 0)
		return err;
	outHeader->formatVersion = sceNetNtohl(diskWords[0]);
	outHeader->flags = sceNetNtohl(diskWords[1]);
	return 0;
}

/* --- TrophyConfFile & TrophyConfIconFile (0x812EF7B4..0x812EFD3F) --- */

TrophyConfFile::TrophyConfFile()
	: m_ownerHandle(NULL)
{
}

TrophyConfFile::~TrophyConfFile()
{
	if (m_ownerHandle != NULL) {
		m_ownerHandle->ReleaseRef();
		m_ownerHandle = NULL;
	}
}

int TrophyConfFile::OpenConfFile(TrophyHandle *handle, int userId, const TrophyCommId *commId,
				 const char *filename, int storageLocation)
{
	char path[256];
	int err = 0;
	if (storageLocation == 1)
		err = npTrophyUr0ConfFilePath(path, userId, commId, filename, "");
	else if (storageLocation == 2)
		err = npTrophyUx0ConfFilePath(path, userId, commId, filename, "");
	else
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	if (err < 0)
		return err;

	bool exists = false;
	err = npTrophyPathExists(path, &exists);
	if (err < 0)
		return err;
	if (!exists)
		return NP_TROPHY_ERR_FILE_NOT_FOUND;

	err = m_file.Open(path, 0x10001, 0);
	if (err < 0)
		return err;

	handle->AddRef();
	m_ownerHandle = handle;
	return 0;
}

bool TrophyConfFile::IsOpen() const
{
	return m_file.IsOpen();
}

int TrophyConfFile::Close()
{
	if (!m_file.IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	int err = m_file.Close();
	if (err < 0)
		return err;
	if (m_ownerHandle != NULL) {
		m_ownerHandle->ReleaseRef();
		m_ownerHandle = NULL;
	}
	return 0;
}

int TrophyConfFile::Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead)
{
	if (handle != m_ownerHandle)
		return NP_TROPHY_ERR_BUSY;
	return m_file.Read(handle, outBuf, size, outRead);
}

TrophyConfIconFile::TrophyConfIconFile()
{
}

TrophyConfIconFile::~TrophyConfIconFile()
{
}

int TrophyConfIconFile::OpenTitleIcon(TrophyHandle *handle, int userId,
				      const TrophyCommId *commId, int lang, bool fallbackDefault)
{
	char filename[32];
	int err = npTrophyFormatTitleIconFilename(filename, lang, 0);
	if (err < 0)
		return err;
	err = OpenConfFile(handle, userId, commId, filename, 1);
	if (err == NP_TROPHY_ERR_FILE_NOT_FOUND)
		err = NP_TROPHY_ERR_ICON_NOT_FOUND;
	if (err >= 0)
		return 0;
	if (err != NP_TROPHY_ERR_ICON_NOT_FOUND || lang < 0 || !fallbackDefault)
		return err;

	err = npTrophyFormatTitleIconFilename(filename, -1, 0);
	if (err < 0)
		return err;
	err = OpenConfFile(handle, userId, commId, filename, 1);
	if (err == NP_TROPHY_ERR_FILE_NOT_FOUND)
		err = NP_TROPHY_ERR_ICON_NOT_FOUND;
	return (err < 0) ? err : 0;
}

int TrophyConfIconFile::OpenGroupIcon(TrophyHandle *handle, int userId,
				      const TrophyCommId *commId, int groupId, int lang,
				      bool fallbackDefault)
{
	char filename[32];
	int err = (groupId == -1) ? npTrophyFormatTitleIconFilename(filename, lang, 0)
				  : npTrophyFormatGroupIconFilename(filename, groupId, lang, 0);
	if (err < 0)
		return err;
	err = OpenConfFile(handle, userId, commId, filename, 1);
	if (err == NP_TROPHY_ERR_FILE_NOT_FOUND)
		err = NP_TROPHY_ERR_ICON_NOT_FOUND;
	if (err >= 0)
		return 0;
	if (err != NP_TROPHY_ERR_ICON_NOT_FOUND || lang < 0 || !fallbackDefault)
		return err;

	err = (groupId == -1) ? npTrophyFormatTitleIconFilename(filename, -1, 0)
			      : npTrophyFormatGroupIconFilename(filename, groupId, -1, 0);
	if (err < 0)
		return err;
	err = OpenConfFile(handle, userId, commId, filename, 1);
	if (err == NP_TROPHY_ERR_FILE_NOT_FOUND)
		err = NP_TROPHY_ERR_ICON_NOT_FOUND;
	return (err < 0) ? err : 0;
}

int TrophyConfIconFile::OpenTrophyIcon(TrophyHandle *handle, int userId,
				       const TrophyCommId *commId, int trophyId)
{
	char filename[32];
	int err = npTrophyFormatTrophyIconFilename(filename, trophyId, 0);
	if (err < 0)
		return err;
	err = OpenConfFile(handle, userId, commId, filename, 2);
	if (err == NP_TROPHY_ERR_FILE_NOT_FOUND)
		err = NP_TROPHY_ERR_ICON_NOT_FOUND;
	return (err < 0) ? err : 0;
}

/* --- SHA-1 & HMAC-SHA1 Wrappers (0x812E985C..0x812E9977) --- */

void TrophySha1Digest::Clear()
{
	sceClibMemset(bytes, 0, sizeof(bytes));
}

int TrophySha1Digest::Set(const void *src, SceSize length)
{
	if (length != sizeof(bytes))
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	sceClibMemcpy_safe(bytes, src, sizeof(bytes));
	return 0;
}

int TrophySha1Digest::CopyTo(void *dst, SceSize dstSize) const
{
	if (dstSize < sizeof(bytes))
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	sceClibMemset(dst, 0, dstSize);
	sceClibMemcpy_safe(dst, bytes, sizeof(bytes));
	return 0;
}

bool TrophySha1Digest::Equals(const TrophySha1Digest &other) const
{
	return sceClibMemcmp(bytes, other.bytes, sizeof(bytes)) == 0;
}

int TrophySha1Digest::FormatHexUpper(char *outHex, SceSize outSize) const
{
	return npTrophyBytesToHex(bytes, sizeof(bytes), outHex, outSize, true);
}

int TrophySha1Hasher::Init()
{
	return sceSha1BlockInit(&m_sha1Ctx);
}

int TrophySha1Hasher::Update(const void *bytes, SceSize size)
{
	return sceSha1BlockUpdate(&m_sha1Ctx, bytes, size);
}

int TrophySha1Hasher::Final(TrophySha1Digest *outDigest)
{
	return sceSha1BlockResult(&m_sha1Ctx, outDigest->bytes);
}

int TrophyHmacSha1Hasher::Init(const void *key, SceSize keyLen)
{
	return sceHmacSha1BlockInit(&m_hmacCtx, key, keyLen);
}

int TrophyHmacSha1Hasher::Update(const void *bytes, SceSize size)
{
	return sceHmacSha1BlockUpdate(&m_hmacCtx, bytes, size);
}

int TrophyHmacSha1Hasher::Final(TrophySha1Digest *outDigest)
{
	return sceHmacSha1BlockResult(&m_hmacCtx, outDigest->bytes);
}

int TrophyHmacSha1Hasher::ComputeDigest(const void *key, SceSize keyLen, const void *bytes,
					SceSize size, TrophySha1Digest *outDigest)
{
	SceHmacSha1Context ctx;
	int err = sceHmacSha1BlockInit(&ctx, key, keyLen);
	if (err < 0)
		return err;
	err = sceHmacSha1BlockUpdate(&ctx, bytes, size);
	if (err < 0)
		return err;
	err = sceHmacSha1BlockResult(&ctx, outDigest->bytes);
	return (err < 0) ? err : 0;
}
