/*
 * vs0:vsh/etc/index.dat reader and build-number verification
 * (sys/internal/usermode/src/psp2swu/index_dat.cpp).
 */
#include <stddef.h>
#include <scetypes.h>
#include <file/localfile.h>
#include <kernel/iofilemgr.h>
#include <libaes.h>
#include <libkernel/libkernel.h>
#include <libsha256.h>
#include <ss_mgr/ss_mgr.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <update_mgr/update_mgr.h>
#include <updater_core/updater_core.h>
#include <vshbridge/vshbridge.h>
#include "index_dat.h"

using namespace paf;

#define INDEX_DAT_PATH				"vs0:vsh/etc/index.dat"
#define INDEX_DAT_HASH_SIZE			32
#define INDEX_DAT_KEY_SIZE			32
#define INDEX_DAT_IV_SIZE			16
#define INDEX_DAT_BLOCK_SIZE			16
#define INDEX_DAT_KEY_COUNT			2
#define INDEX_DAT_MAX_LINES			21
#define INDEX_DAT_MAX_FIELDS			3
#define INDEX_DAT_RELEASE_STR_LEN		10
#define INDEX_DAT_ERROR_NO_MEMORY		((SceInt32)0x8001000C)
#define INDEX_DAT_ERROR_BAD_VSH_BUILD		((SceInt32)0x80182414)
#define INDEX_DAT_ERROR_BAD_DEVICE_VERSION	((SceInt32)0x80182415)

struct ScePortabilityKeyBuf {
	SceSize size;
	SceUInt8 bytes[INDEX_DAT_KEY_SIZE];
};

static const SceUInt8 s_indexDatWrappedKeys[INDEX_DAT_KEY_COUNT][INDEX_DAT_KEY_SIZE] = {
	{
		0xec, 0x42, 0xb9, 0x32, 0x3f, 0xf4, 0x79, 0x83,
		0x96, 0xc3, 0x27, 0xa9, 0xf5, 0x00, 0xa4, 0xa2,
		0x78, 0x9f, 0x95, 0xeb, 0x68, 0xe7, 0xbf, 0x8c,
		0x73, 0xea, 0x2b, 0xfc, 0x6b, 0x02, 0x49, 0xaf
	},
	{
		0x4a, 0x2b, 0xad, 0x5e, 0xb9, 0x55, 0x27, 0xbc,
		0xce, 0x8a, 0xe1, 0x4e, 0x79, 0x06, 0xcd, 0xac,
		0x9a, 0x72, 0xce, 0x6d, 0xfa, 0xd7, 0xf6, 0x59,
		0xb1, 0x41, 0x20, 0xd7, 0xd4, 0xa5, 0xb3, 0x20
	}
};

static const SceUInt8 s_indexDatIv[INDEX_DAT_IV_SIZE] = {
	0x37, 0xfa, 0x4e, 0xd2, 0xb6, 0x61, 0x8b, 0x59,
	0xb3, 0x4f, 0x77, 0x0f, 0xbb, 0x92, 0x94, 0x7b
};

static void ClearSensitiveBuffer(void *buffer, SceSize size)
{
	volatile SceUInt8 *cursor = static_cast<volatile SceUInt8 *>(buffer);

	while (size > 0) {
		size--;
		cursor[size] = 0;
	}
}

static void ClearFirstIndexEntry(IndexDatEntry *entry)
{
	if (entry->key.c_str() != NULL)
		entry->key = "";
	if (entry->value.c_str() != NULL)
		entry->value = "";
	if (entry->detail.c_str() != NULL)
		entry->detail = "";
}

static SceInt32 DecryptIndexDatBuffer(SceUInt8 *buffer, SceSize bufferSize,
				      const SceUInt8 wrappedKey[INDEX_DAT_KEY_SIZE])
{
	SceUInt8 *plainKey = static_cast<SceUInt8 *>(sce_paf_memalign(64, INDEX_DAT_KEY_SIZE));

	if (plainKey == NULL)
		return INDEX_DAT_ERROR_NO_MEMORY;

	SceUInt8 iv[INDEX_DAT_IV_SIZE];
	ScePortabilityKeyBuf inputKey;
	ScePortabilityKeyBuf outputKey;

	sce_paf_memcpy(iv, s_indexDatIv, INDEX_DAT_IV_SIZE);
	inputKey.size = INDEX_DAT_KEY_SIZE;
	sce_paf_memcpy(inputKey.bytes, wrappedKey, INDEX_DAT_KEY_SIZE);
	outputKey.size = INDEX_DAT_KEY_SIZE;
	sce_paf_memset(outputKey.bytes, 0, INDEX_DAT_KEY_SIZE);

	SceInt32 ret = _vshSblSsDecryptWithPortability(1, iv, &inputKey, &outputKey);

	if (ret != 0) {
		ClearSensitiveBuffer(plainKey, INDEX_DAT_KEY_SIZE);
		sce_paf_free(plainKey);
		return ret;
	}

	sce_paf_memcpy(plainKey, outputKey.bytes, INDEX_DAT_KEY_SIZE);

	SceAesContext aesCtx;

	sce_paf_memset(&aesCtx, 0, sizeof(aesCtx));
	ret = sceAesScheduleKey(&aesCtx, 128, 256, plainKey);
	if (ret < 0) {
		ClearSensitiveBuffer(plainKey, INDEX_DAT_KEY_SIZE);
		sce_paf_free(plainKey);
		return ret;
	}

	SceUInt8 cbcChain[INDEX_DAT_IV_SIZE];
	SceUInt32 blockBytes = aesCtx.blockWords * 4;

	for (SceUInt32 idx = 0; idx < blockBytes; idx++)
		cbcChain[idx] = s_indexDatIv[idx];

	SceUInt32 blockCount = bufferSize / INDEX_DAT_BLOCK_SIZE;
	SceUInt8 *blockPtr = buffer;

	for (SceUInt32 blockIdx = 0; blockIdx < blockCount; blockIdx++) {
		SceUInt8 decryptedBlock[INDEX_DAT_BLOCK_SIZE];

		sceAesDecrypt(&aesCtx, blockPtr, decryptedBlock);
		for (SceUInt32 byteIdx = 0; byteIdx < blockBytes; byteIdx++) {
			SceUInt8 plainByte = cbcChain[byteIdx] ^ decryptedBlock[byteIdx];

			cbcChain[byteIdx] = blockPtr[byteIdx];
			blockPtr[byteIdx] = plainByte;
		}
		blockPtr += INDEX_DAT_BLOCK_SIZE;
	}

	ClearSensitiveBuffer(plainKey, INDEX_DAT_KEY_SIZE);
	sce_paf_free(plainKey);
	return 0;
}

void SplitIndexString(vector<string> &outParts, const string &input,
		      const string &delimiter, SceUInt32 maxParts)
{
	outParts.clear();

	SceSize pos = 0;

	while (outParts.size() + 1 < maxParts) {
		SceSize found = input.find(delimiter, pos);

		if (found == string::npos)
			break;
		outParts.push_back(input.substr(pos, found - pos));
		pos = found + delimiter.size();
	}
	outParts.push_back(input.substr(pos, input.size() - pos));
}

SceInt32 ReadIndexDat(IndexDatEntry entries[INDEX_DAT_MAX_ENTRIES])
{
	vector<string> lines;
	SceInt32 openErr = 0;
	common::SharedPtr<File> file = LocalFile::Open(INDEX_DAT_PATH, SCE_O_RDONLY, 0, &openErr);

	if (openErr != 0 || file.get() == NULL)
		return openErr;

	SceOff fileSize = file->GetFileSize();

	if (fileSize < 0)
		return static_cast<SceInt32>(fileSize);
	if (fileSize == 0)
		return -1;

	SceSize totalSize = static_cast<SceSize>(fileSize);
	SceUInt8 *buffer = static_cast<SceUInt8 *>(sce_paf_memalign(64, totalSize));

	if (buffer == NULL)
		return INDEX_DAT_ERROR_NO_MEMORY;

	SceInt32 status = -1;

	for (SceInt32 keyIdx = 0; keyIdx < INDEX_DAT_KEY_COUNT; keyIdx++) {
		file->Seek(0, SCE_SEEK_SET);
		if (file->Read(buffer, totalSize) != static_cast<SceInt32>(totalSize)) {
			ClearSensitiveBuffer(buffer, totalSize);
			sce_paf_free(buffer);
			return -1;
		}
		if (totalSize < INDEX_DAT_HASH_SIZE) {
			status = -1;
			break;
		}

		status = DecryptIndexDatBuffer(buffer, totalSize, s_indexDatWrappedKeys[keyIdx]);
		if (status >= 0) {
			SceUInt8 computedDigest[INDEX_DAT_HASH_SIZE];

			sce_paf_memset(computedDigest, 0, sizeof(computedDigest));
			status = sceSha256Digest(buffer + INDEX_DAT_HASH_SIZE,
						 totalSize - INDEX_DAT_HASH_SIZE,
						 computedDigest);
			if (status >= 0) {
				if (sceClibMemcmpConstTime(buffer, computedDigest,
							   INDEX_DAT_HASH_SIZE) != 0)
					status = -1;
			}
			if (status >= 0)
				break;
		}
	}

	if (status == 0) {
		string body(reinterpret_cast<const char *>(buffer + INDEX_DAT_HASH_SIZE));
		string lineSep("\r\n");

		SplitIndexString(lines, body, lineSep, INDEX_DAT_MAX_LINES);
	}

	if (status != 0) {
		ClearSensitiveBuffer(buffer, totalSize);
		sce_paf_free(buffer);
		return status;
	}

	SceSize entryLimit = lines.size();

	if (entryLimit > INDEX_DAT_MAX_ENTRIES)
		entryLimit = INDEX_DAT_MAX_ENTRIES;

	status = -1;
	for (SceSize lineIdx = 0; lineIdx + 1 < entryLimit; lineIdx++) {
		vector<string> columns;
		string fieldSep(": ");

		SplitIndexString(columns, lines[lineIdx], fieldSep, INDEX_DAT_MAX_FIELDS);
		if (columns.size() >= 1)
			entries[lineIdx].key = columns[0];
		if (columns.size() >= 2)
			entries[lineIdx].value = columns[1];
		if (columns.size() >= 3)
			entries[lineIdx].detail = columns[2];

		if (columns[0] == "release") {
			if (entries[lineIdx].value.size() != INDEX_DAT_RELEASE_STR_LEN) {
				ClearFirstIndexEntry(&entries[0]);
				ClearSensitiveBuffer(buffer, totalSize);
				sce_paf_free(buffer);
				return -1;
			}

			char *endPtr = NULL;
			SceUInt32 releaseVersion = sce_paf_strtoul(entries[lineIdx].value.c_str(),
								   &endPtr, 16);

			if (*endPtr != '\0') {
				ClearFirstIndexEntry(&entries[0]);
				ClearSensitiveBuffer(buffer, totalSize);
				sce_paf_free(buffer);
				return -1;
			}

			SceSblUsSpkgInfo spkgInfo;

			sce_paf_memset(&spkgInfo, 0, sizeof(spkgInfo));
			spkgInfo.size = sizeof(spkgInfo);

			SceInt32 spkgRet = sceSblUsGetSpkgInfo(1, &spkgInfo);

			if (spkgRet != 0) {
				ClearFirstIndexEntry(&entries[0]);
				ClearSensitiveBuffer(buffer, totalSize);
				sce_paf_free(buffer);
				return spkgRet;
			}
			if ((releaseVersion >> 12) != (spkgInfo.version >> 12)) {
				ClearFirstIndexEntry(&entries[0]);
				ClearSensitiveBuffer(buffer, totalSize);
				sce_paf_free(buffer);
				return -1;
			}
			status = 0;
		}
	}

	ClearSensitiveBuffer(buffer, totalSize);
	sce_paf_free(buffer);
	return status;
}

SceInt32 ReadDeviceAndVshBuildNumbers(SceUInt32 *outDeviceVersion, SceUInt32 *outVshBuildNumber)
{
	IndexDatEntry entries[INDEX_DAT_MAX_ENTRIES];

	*outDeviceVersion = 0;
	*outVshBuildNumber = 0;

	SceInt32 ret = ReadIndexDat(entries);

	if (ret != 0)
		return ret;

	for (SceInt32 idx = 0; idx < INDEX_DAT_MAX_ENTRIES; idx++) {
		if (entries[idx].key == "")
			break;
		if (entries[idx].key == "vsh-build-number") {
			if (entries[idx].value == string("N/A"))
				return INDEX_DAT_ERROR_BAD_VSH_BUILD;
			*outVshBuildNumber = static_cast<SceUInt32>(
				sce_paf_atoi(entries[idx].value.c_str()));
		}
	}

	if (*outVshBuildNumber == 0)
		return INDEX_DAT_ERROR_BAD_VSH_BUILD;

	SceUpdaterDeviceInfo deviceInfo;

	ret = sceUpdaterGetDeviceInfo(&deviceInfo);
	if (ret != 0)
		return ret;

	*outDeviceVersion = deviceInfo.m_version;
	if (deviceInfo.m_version == 0)
		return INDEX_DAT_ERROR_BAD_DEVICE_VERSION;
	return 0;
}

SceInt32 GetPupDiffBaseInfo(const char *pupPath, bool *outIsDiff, SceUInt32 *outBaseBuildNo,
			    SceUInt32 *outBaseVersion)
{
	SceUpdaterPupInformation pupInfo;

	*outBaseBuildNo = 0;
	*outBaseVersion = 0;
	*outIsDiff = false;

	SceInt32 ret = sceUpdaterGetPupInformation(pupPath, &pupInfo);

	if (ret == 0) {
		*outBaseBuildNo = pupInfo.baseBuildNo;
		*outBaseVersion = pupInfo.baseVersion;
		*outIsDiff = (pupInfo.attribute[0] & 4) != 0;
	}
	return ret;
}
