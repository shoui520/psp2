/*
 * nptrophy_storage.cpp: local storage layer controllers for SceNpTrophy
 * (TrophyTitleStorage, TrophyConfStorage, TrophyUserStorage, TrophyUserIndexStorage,
 * TrophyAssetExtractor, TrophyConfStream, TrophySignedSfmParser/Stream/Reader,
 * TrophySfmTextExtractor, TrophyHandlePoolEntry, TrophySnsFeedPublisher;
 * 0x812D00C0..0x812D8763).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <bXCe/bxce_internal.h>
#include "nptrophy_storage.h"
#include "nptrophy_net.h"

namespace {

static const char *const kActiveDirSuffix = "";
static const char *const kBackupDirSuffix = "_BK";
static const int kTrophyOpenFlagsCreate = 0x00010603;	/* 67075 */
static const int kTrophyOpenFlagsReadOnly = 0x00010001;	/* 65537 */
static const int kTrophyOpenFlagsReadWrite = 0x00010003;/* 65539 */
static const SceMode kTrophyFileMode = 6;
static const int kRtcErrNotInitialized = -2145054718;	/* 0x80251002 */

static bool isStorageFileSystemError(int err)
{
	return (((uint32_t)err >> 16) & 0x0FFFu) == 0x14u;
}

class TrophySnsCancelToken : public TrophyScopedCancelToken {
public:
	explicit TrophySnsCancelToken(int snsContext) : m_snsContext(snsContext) {}
	virtual ~TrophySnsCancelToken() {}

protected:
	virtual void OnCancel(int errorCode) { (void)errorCode; }

private:
	int m_snsContext;
};

} /* namespace */

/* =========================================================================
 * System signature requirement & dual clock helpers (0x812D7EA8, 0x812D7EC0)
 * ========================================================================= */

/* 0x812D7EA8 */
int npTrophyRequireSfmSignature(bool *outRequired)
{
	*outRequired = (vshSblSsIsDevelopmentMode() == 0);
	return 0;
}

/* 0x812D7EC0 */
int npTrophyGetDualTick(TrophyDualTick *outTicks)
{
	int ret = sceRtcGetCurrentTick(&outTicks->localTick);
	if (ret < 0)
		return ret;
	ret = vshRtcGetCurrentSecureTick(&outTicks->secureTick);
	if (ret == kRtcErrNotInitialized) {
		outTicks->secureTick.tick = 0;
		return 0;
	}
	return ret < 0 ? ret : 0;
}

/* =========================================================================
 * TrophyDataDirGuard
 * ========================================================================= */

TrophyDataDirGuard::~TrophyDataDirGuard()
{
	if (m_arbiter != NULL)
		Release();
}

int TrophyDataDirGuard::AcquireUserRoot(TrophyStorageArbiter *arbiter, int userId)
{
	int ret = npTrophyArbiterLockUserRoot(arbiter, userId, m_path, sizeof(m_path));
	if (ret >= 0)
		m_arbiter = arbiter;
	return ret;
}

int TrophyDataDirGuard::AcquireTitleData(TrophyStorageArbiter *arbiter, int userId,
					 const TrophyCommId *commId)
{
	int ret = npTrophyArbiterLockTitleData(arbiter, userId, commId, m_path, sizeof(m_path));
	if (ret >= 0)
		m_arbiter = arbiter;
	return ret;
}

int TrophyDataDirGuard::AcquireTitleBackup(TrophyStorageArbiter *arbiter, int userId,
					   const TrophyCommId *commId)
{
	int ret = npTrophyArbiterLockTitleBackup(arbiter, userId, commId, m_path, sizeof(m_path));
	if (ret >= 0)
		m_arbiter = arbiter;
	return ret;
}

int TrophyDataDirGuard::Release()
{
	if (m_arbiter == NULL)
		return 0;
	int ret = npTrophyArbiterUnlockPath(m_arbiter, m_path);
	m_arbiter = NULL;
	m_path[0] = '\0';
	return ret;
}

/* =========================================================================
 * TrophyExtractedAssetReport & TrophyAssetExtractor (0x812D6780..0x812D6E4F)
 * ========================================================================= */

/* 0x812D6780 */
void TrophyExtractedAssetReport::Init()
{
	commId.Clear();
	defaultLang = -2;
	sfmLang = -2;
	titleIconLang = -2;
	for (int i = 0; i < 32; ++i)
		groupIconLangs[i] = -2;
}

/* 0x812D6812 */
void TrophyExtractedAssetReport::ResetWithCommId(const TrophyCommId *id, int lang)
{
	Init();
	commId = *id;
	defaultLang = lang;
}

/* 0x812D67B8 */
void TrophyExtractedAssetReport::RecordAsset(const TrophyAssetDesc *desc)
{
	int lang = desc->langOrTrophyId;
	bool validLang = (lang == -1 || (lang >= 0 && lang < 20));
	if (!validLang)
		return;
	if (desc->kind == TROPHY_ASSET_SFM) {
		sfmLang = lang;
	} else if (desc->kind == TROPHY_ASSET_TITLE_ICON) {
		titleIconLang = lang;
	} else if (desc->kind == TROPHY_ASSET_GROUP_ICON) {
		if (desc->groupId >= 0 && desc->groupId < 32)
			groupIconLangs[desc->groupId] = lang;
	}
}

/* 0x812D68A4 */
TrophyAssetExtractor::TrophyAssetExtractor()
	: m_sfmStream(NULL),
	  m_teeStream(NULL)
{
	sceClibMemset(&m_assetDesc, 0, sizeof(m_assetDesc));
	m_filename[0] = '\0';
	m_destPath[0] = '\0';
}

/* 0x812D68DC */
TrophyAssetExtractor::~TrophyAssetExtractor()
{
	if (m_sfmStream != NULL) {
		delete m_sfmStream;
		m_sfmStream = NULL;
	}
	if (m_teeStream != NULL) {
		delete m_teeStream;
		m_teeStream = NULL;
	}
}

/* 0x812D6DBA */
bool TrophyAssetExtractor::ShouldExtractAsset(const char *filename,
					      const TrophyExtractOptions *options)
{
	TrophyAssetDesc desc;
	if (npTrophyParseAssetFilename(filename, &desc) < 0)
		return false;
	if (desc.kind == TROPHY_ASSET_SFM)
		return (options->assetMask & 1u) != 0;
	if (desc.kind == TROPHY_ASSET_TITLE_ICON)
		return (options->assetMask & 2u) != 0;
	if (desc.kind == TROPHY_ASSET_GROUP_ICON)
		return (options->assetMask & 4u) != 0;
	if (desc.kind == TROPHY_ASSET_TROPHY_ICON)
		return (options->assetMask & 0x18u) != 0;
	return false;
}

/* 0x812D6936 */
int TrophyAssetExtractor::Begin(const char *filename, TrophyInputStream *input,
				const TrophyExtractOptions *options, TrophyHashSink *hashSink)
{
	m_commId = *options->commId;
	npTrophyStrlcpy(m_filename, filename, sizeof(m_filename));

	int ret = npTrophyParseAssetFilename(m_filename, &m_assetDesc);
	if (ret < 0)
		return ret;

	TrophyOutputStream *destStream = NULL;
	if ((options->flags & 0x10000u) == 0) {
		char outName[32];
		if (m_assetDesc.kind == TROPHY_ASSET_SFM) {
			int targetLang = (m_assetDesc.langOrTrophyId == -2) ? -2 : -1;
			ret = npTrophyFormatSfmFilename(outName, targetLang);
			if (ret >= 0 && (options->assetMask & 1u) == 0)
				ret = NP_TROPHY_ERR_GENERIC;
			if (ret >= 0)
				ret = npTrophyUr0ConfFilePath(m_destPath, options->userId,
							      options->commId, outName,
							      kActiveDirSuffix);
		} else if (m_assetDesc.kind == TROPHY_ASSET_TITLE_ICON) {
			ret = npTrophyFormatTitleIconFilename(outName, -1, 0);
			if (ret >= 0 && (options->assetMask & 2u) == 0)
				ret = NP_TROPHY_ERR_GENERIC;
			if (ret >= 0)
				ret = npTrophyUr0ConfFilePath(m_destPath, options->userId,
							      options->commId, outName,
							      kActiveDirSuffix);
		} else if (m_assetDesc.kind == TROPHY_ASSET_GROUP_ICON) {
			ret = npTrophyFormatGroupIconFilename(outName, m_assetDesc.groupId, -1, 0);
			if (ret >= 0 && (options->assetMask & 4u) == 0)
				ret = NP_TROPHY_ERR_GENERIC;
			if (ret >= 0)
				ret = npTrophyUr0ConfFilePath(m_destPath, options->userId,
							      options->commId, outName,
							      kActiveDirSuffix);
		} else if (m_assetDesc.kind == TROPHY_ASSET_TROPHY_ICON) {
			ret = npTrophyFormatTrophyIconFilename(outName,
							       m_assetDesc.langOrTrophyId, 0);
			if (ret >= 0) {
				if ((options->assetMask & 8u) != 0) {
					ret = npTrophyUr0ConfFilePath(m_destPath, options->userId,
								      options->commId, outName,
								      kActiveDirSuffix);
				} else if ((options->assetMask & 0x10u) != 0) {
					ret = npTrophyUx0ConfFilePath(m_destPath, options->userId,
								      options->commId, outName,
								      kActiveDirSuffix);
				} else {
					ret = NP_TROPHY_ERR_GENERIC;
				}
			}
		} else {
			ret = NP_TROPHY_ERR_GENERIC;
		}

		if (ret >= 0) {
			ret = m_destFile.Open(m_destPath, kTrophyOpenFlagsCreate, kTrophyFileMode);
			if (ret >= 0)
				destStream = &m_destFile;
		}
	}

	if (ret >= 0) {
		if (m_assetDesc.kind == TROPHY_ASSET_SFM) {
			m_sfmStream = new TrophySignedSfmStream();
			if (m_sfmStream == NULL) {
				ret = NP_TROPHY_ERR_OUT_OF_MEMORY;
			} else {
				ret = m_sfmStream->Init(options->parseOptions, input, destStream,
							hashSink);
			}
		} else if (m_assetDesc.kind >= TROPHY_ASSET_TITLE_ICON &&
			   m_assetDesc.kind <= TROPHY_ASSET_TROPHY_ICON) {
			m_teeStream = new TrophyTeeStream();
			if (m_teeStream == NULL) {
				ret = NP_TROPHY_ERR_OUT_OF_MEMORY;
			} else {
				m_teeStream->Init(input, destStream, hashSink);
			}
		} else {
			ret = NP_TROPHY_ERR_GENERIC;
		}
	}

	if (ret < 0) {
		if (m_destFile.IsOpen())
			m_destFile.Close();
		if (m_sfmStream != NULL) {
			delete m_sfmStream;
			m_sfmStream = NULL;
		}
		if (m_teeStream != NULL) {
			delete m_teeStream;
			m_teeStream = NULL;
		}
		return ret;
	}
	return 0;
}

/* 0x812D6CC6 */
int TrophyAssetExtractor::End()
{
	if (!IsActive())
		return NP_TROPHY_ERR_STREAM_NOT_INIT;
	if (m_destFile.IsOpen()) {
		int ret = m_destFile.Close();
		if (ret < 0)
			return ret;
	}
	if (m_sfmStream != NULL) {
		delete m_sfmStream;
		m_sfmStream = NULL;
	}
	if (m_teeStream != NULL) {
		delete m_teeStream;
		m_teeStream = NULL;
	}
	return 0;
}

/* 0x812D6D22 */
int TrophyAssetExtractor::Process(TrophyHandle *handle, TrophyAssetDesc *outDesc)
{
	int ret = NP_TROPHY_ERR_GENERIC;
	if (m_sfmStream != NULL) {
		ret = m_sfmStream->Parse(handle);
	} else if (m_teeStream != NULL) {
		ret = m_teeStream->DrainAll(handle);
	}
	if (ret < 0)
		return ret;
	if (outDesc != NULL)
		*outDesc = m_assetDesc;
	return 0;
}

/* 0x812D6D6E */
int TrophyAssetExtractor::TakeConfHandle(TrophyConfHandle **outHandle)
{
	if (m_sfmStream == NULL)
		return NP_TROPHY_ERR_GENERIC;
	return m_sfmStream->TakeConfHandle(&m_commId, m_assetDesc.langOrTrophyId, outHandle);
}

/* 0x812D6D9A */
uint64_t TrophyAssetExtractor::GetBytesProcessed() const
{
	if (m_sfmStream != NULL)
		return m_sfmStream->GetTotalBytes();
	if (m_teeStream != NULL)
		return m_teeStream->GetTotalBytes();
	return 0;
}

/* =========================================================================
 * TrophyConfStream, TrophySignedSfmParser/Stream/Reader (0x812D6E50..0x812D7D03)
 * ========================================================================= */

/* 0x812D6E50 */
TrophyConfStream::TrophyConfStream()
{
}

/* 0x812D6E7C */
TrophyConfStream::~TrophyConfStream()
{
}

/* 0x812D6EE2 */
int TrophyConfStream::OpenSfm(TrophyHandle *handle, int userId, const TrophyCommId *commId,
			      int lang, bool fallbackDefault)
{
	char filename[32];
	int ret = npTrophyFormatSfmFilename(filename, lang);
	if (ret >= 0) {
		ret = OpenConfFile(handle, userId, commId, filename, 1);
		if (ret == NP_TROPHY_ERR_FILE_NOT_FOUND && lang >= 0 && fallbackDefault) {
			ret = npTrophyFormatSfmFilename(filename, -1);
			if (ret >= 0)
				ret = OpenConfFile(handle, userId, commId, filename, 1);
		}
	}
	if (ret >= 0)
		return 0;

	m_stripper.Term();
	if (IsOpen())
		TrophyConfFile::Close();
	return (ret == NP_TROPHY_ERR_FILE_NOT_FOUND) ? NP_TROPHY_ERR_SFM_NOT_FOUND : ret;
}

/* 0x812D6FC8 */
int TrophyConfStream::Close()
{
	if (!IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	m_stripper.Term();
	return TrophyConfFile::Close();
}

/* 0x812D7008 */
int TrophyConfStream::GetStrippedStream(TrophyHandle *handle, TrophyInputStream **outStream)
{
	int ret = Seek(handle, 0, SCE_SEEK_SET, NULL);
	if (ret < 0)
		return ret;
	ret = m_stripper.Init(this);
	if (ret >= 0)
		ret = m_stripper.StripHeader(handle);
	if (ret < 0) {
		m_stripper.Term();
		return ret;
	}
	*outStream = &m_stripper;
	return 0;
}

/* 0x812D7B18 */
TrophySignedSfmParser::TrophySignedSfmParser()
	: m_verifier(NULL),
	  m_signatureCopy(NULL)
{
}

/* 0x812D7B3C */
TrophySignedSfmParser::~TrophySignedSfmParser()
{
	if (m_verifier != NULL) {
		delete m_verifier;
		m_verifier = NULL;
	}
	if (m_signatureCopy != NULL) {
		TrophyHeapObject::operator delete(m_signatureCopy);
		m_signatureCopy = NULL;
	}
}

/* 0x812D7BAE */
int TrophySignedSfmParser::InitSigned(uint32_t parseOptions)
{
	return TrophySfmParser::Init(parseOptions);
}

/* 0x812D7BBE */
int TrophySignedSfmParser::ParseSignedStream(TrophyHandle *handle,
					     TrophySfmSignatureStripper *stripper)
{
	int ret = stripper->StripHeader(handle);
	if (ret >= 0) {
		TrophySfmSignature sig;
		bool hasSig = false;
		ret = stripper->GetSignature(&sig, &hasSig);
		if (ret >= 0 && hasSig) {
			m_signatureCopy = static_cast<TrophySfmSignature *>(
				TrophyHeapObject::operator new(sizeof(TrophySfmSignature)));
			if (m_signatureCopy == NULL) {
				ret = NP_TROPHY_ERR_OUT_OF_MEMORY;
			} else {
				*m_signatureCopy = sig;
				m_verifier = new TrophySfmSignatureVerifier();
				if (m_verifier == NULL)
					ret = NP_TROPHY_ERR_OUT_OF_MEMORY;
				else
					ret = m_verifier->Init(m_signatureCopy);
			}
		}
		if (ret >= 0) {
			TrophyHashSink *hashSink = m_verifier;
			ret = ParseStream(handle, stripper, hashSink);
			if (ret >= 0) {
				bool sigRequired = true;
				ret = npTrophyRequireSfmSignature(&sigRequired);
				if (ret >= 0 && sigRequired) {
					if (m_verifier == NULL)
						ret = NP_TROPHY_ERR_SIG_VERIFY_FAILED;
					else
						ret = m_verifier->Verify(m_signatureCopy);
				}
			}
		}
	}

	if (m_signatureCopy != NULL) {
		TrophyHeapObject::operator delete(m_signatureCopy);
		m_signatureCopy = NULL;
	}
	if (m_verifier != NULL) {
		delete m_verifier;
		m_verifier = NULL;
	}
	return ret < 0 ? ret : 0;
}

/* 0x812D7CDE */
int TrophySignedSfmParser::TakeSignedConfData(const TrophyCommId *expectedCommId,
					      TrophyConfData **outConfData)
{
	return TakeConfData(expectedCommId, outConfData);
}

/* 0x812D70CC */
TrophySignedSfmStream::TrophySignedSfmStream()
{
}

/* 0x812D70FC */
TrophySignedSfmStream::~TrophySignedSfmStream()
{
}

/* 0x812D7176 */
int TrophySignedSfmStream::Init(uint32_t parseOptions, TrophyInputStream *input,
				TrophyOutputStream *output, TrophyHashSink *hashSink)
{
	m_teeStream.Init(input, output, hashSink);
	int ret = m_stripper.Init(&m_teeStream);
	if (ret < 0)
		return ret;
	return m_parser.InitSigned(parseOptions);
}

/* 0x812D71B8 */
int TrophySignedSfmStream::Parse(TrophyHandle *handle)
{
	return m_parser.ParseSignedStream(handle, &m_stripper);
}

/* 0x812D71D2 */
int TrophySignedSfmStream::TakeConfHandle(const TrophyCommId *expectedCommId, int lang,
					  TrophyConfHandle **outHandle)
{
	TrophyConfData *confData = NULL;
	SceSize headerSize = 0;
	int ret = m_parser.TakeSignedConfData(expectedCommId, &confData);
	if (ret >= 0)
		ret = m_stripper.GetStrippedHeaderSize(&headerSize);
	if (ret < 0) {
		if (confData != NULL)
			delete confData;
		return ret;
	}

	TrophyConfHandle *confHandle = new TrophyConfHandle();
	if (confHandle == NULL) {
		if (confData != NULL)
			delete confData;
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}
	confHandle->Assign(&confData, headerSize, lang);
	*outHandle = confHandle;
	return 0;
}

/* 0x812D726C */
TrophySignedSfmReader::TrophySignedSfmReader()
{
}

/* 0x812D7290 */
TrophySignedSfmReader::~TrophySignedSfmReader()
{
}

/* 0x812D72F4 */
int TrophySignedSfmReader::Init(uint32_t parseOptions)
{
	return m_parser.InitSigned(parseOptions);
}

/* 0x812D7306 */
int TrophySignedSfmReader::Parse(TrophyHandle *handle, TrophyInputStream *input)
{
	int ret = m_stripper.Init(input);
	if (ret >= 0)
		ret = m_parser.ParseSignedStream(handle, &m_stripper);
	if (ret < 0) {
		m_stripper.Term();
		return ret;
	}
	return 0;
}

/* 0x812D734A */
int TrophySignedSfmReader::TakeConfHandle(const TrophyCommId *expectedCommId, int lang,
					  TrophyConfHandle **outHandle)
{
	TrophyConfData *confData = NULL;
	SceSize headerSize = 0;
	int ret = m_parser.TakeSignedConfData(expectedCommId, &confData);
	if (ret >= 0)
		ret = m_stripper.GetStrippedHeaderSize(&headerSize);
	if (ret < 0) {
		if (confData != NULL)
			delete confData;
		return ret;
	}

	TrophyConfHandle *confHandle = new TrophyConfHandle();
	if (confHandle == NULL) {
		if (confData != NULL)
			delete confData;
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}
	confHandle->Assign(&confData, headerSize, lang);
	*outHandle = confHandle;
	return 0;
}

/* 0x812D73E0 */
int TrophySignedSfmReader::TakeConfData(const TrophyCommId *expectedCommId,
					TrophyConfData **outConfData)
{
	return m_parser.TakeSignedConfData(expectedCommId, outConfData);
}

/* =========================================================================
 * SFM entity-decoding text extraction (0x812D73F0..0x812D7B17)
 * ========================================================================= */

/* 0x812D73F0 */
int npTrophyReadDecodedSfmText(TrophyHandle *handle, TrophyFileBase *file, uint64_t offset,
			       SceSize byteLength, char *outText, SceSize outSize)
{
	TrophyReadBuffer readBuf;
	int ret = readBuf.Init(1024);
	if (ret < 0)
		return ret;

	ret = file->Seek(handle, (SceOff)offset, SCE_SEEK_SET, NULL);
	if (ret < 0)
		return ret;

	char entityBuf[16];
	sceClibMemset(entityBuf, 0, sizeof(entityBuf));
	SceSize entityLen = 0;
	bool inEntity = false;
	SceSize produced = 0;
	char *dst = outText;
	SceSize remaining = outSize;

	while (produced < byteLength) {
		if (readBuf.IsEof())
			return NP_TROPHY_ERR_UNEXPECTED_EOF;
		if (readBuf.IsEmpty()) {
			ret = readBuf.Refill(handle, file);
			if (ret < 0)
				return ret;
			if (readBuf.IsEmpty())
				return NP_TROPHY_ERR_UNEXPECTED_EOF;
		}

		while (readBuf.GetAvailable() > 0 && produced < byteLength) {
			char ch = (char)readBuf.PeekByte(0);
			readBuf.Consume(1);

			if (!inEntity) {
				if (ch == '&') {
					inEntity = true;
					entityLen = 0;
					sceClibMemset(entityBuf, 0, sizeof(entityBuf));
				} else {
					if (remaining == 0)
						return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
					*dst++ = ch;
					--remaining;
					++produced;
				}
				continue;
			}

			if (ch != ';') {
				if (entityLen >= 15)
					return NP_TROPHY_ERR_XML_ENTITY_INVALID;
				entityBuf[entityLen++] = ch;
				entityBuf[entityLen] = '\0';
				continue;
			}

			char decoded = 0;
			if (entityLen == 2 && sceClibMemcmp(entityBuf, "lt", 2) == 0)
				decoded = '<';
			else if (entityLen == 2 && sceClibMemcmp(entityBuf, "gt", 2) == 0)
				decoded = '>';
			else if (entityLen == 3 && sceClibMemcmp(entityBuf, "amp", 3) == 0)
				decoded = '&';
			else if (entityLen == 4 && sceClibMemcmp(entityBuf, "quot", 4) == 0)
				decoded = '"';

			if (decoded != 0) {
				if (remaining == 0)
					return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
				*dst++ = decoded;
				--remaining;
				++produced;
			} else if (entityLen > 0 && entityBuf[0] == '#') {
				SceUInt32 codePoint = 0;
				int parseErr = 0;
				if (entityLen > 1 && entityBuf[1] == 'x') {
					parseErr = ScebXCe_DFB92627(&codePoint, entityBuf + 2,
								    entityLen - 2);
				} else {
					parseErr = ScebXCe_CC190FA8(&codePoint, entityBuf + 1,
								    entityLen - 1);
				}
				if (parseErr != 0)
					return NP_TROPHY_ERR_XML_ENTITY_INVALID;
				int utf8Len = 0;
				char *utf8Out = entityBuf;
				if (ScebXCe_BD9D727B(codePoint, &utf8Out, &utf8Len) != 0)
					return NP_TROPHY_ERR_XML_ENTITY_INVALID;
				SceSize actualLen = sceClibStrnlen(entityBuf, sizeof(entityBuf));
				if (actualLen > remaining)
					return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
				sceClibMemcpy_safe(dst, entityBuf, actualLen);
				dst += actualLen;
				remaining -= actualLen;
				produced += actualLen;
			}

			inEntity = false;
			entityLen = 0;
			sceClibMemset(entityBuf, 0, sizeof(entityBuf));
		}
	}

	if (remaining == 0)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	*dst = '\0';
	return 0;
}

/* 0x812D779A */
int npTrophyExtractTitleTextInfo(TrophyHandle *handle, TrophyFileBase *file,
				 const TrophyConfHandle *confHandle, void *outTitleDetails)
{
	struct GameDetailsLayout {
		SceSize structSize;
		uint32_t numGroups;
		uint32_t numTrophies;
		TrophyGradeCounts gradeCounts;
		char title[128];
		char description[1024];
	};

	const TrophyConfData *conf = confHandle->GetConfData();
	SceSize headerOffset = confHandle->GetStrippedHeaderSize();
	GameDetailsLayout *out = static_cast<GameDetailsLayout *>(outTitleDetails);

	uint32_t numGroups = 0;
	if (conf->SupportsGroups()) {
		int ret = conf->GetGroupBitset(NULL, &numGroups);
		if (ret < 0)
			return ret;
	}
	out->numGroups = numGroups;

	TrophyGradeCounts counts;
	int ret = conf->CountByGrade(&counts);
	if (ret < 0)
		return ret;
	out->numTrophies = npTrophyGradeCountTotal(&counts);
	out->gradeCounts = counts;

	TrophyConfData *mutableConf = const_cast<TrophyConfData *>(conf);
	uint64_t textOffset = 0;
	SceSize textLen = 0;
	mutableConf->GetTitleNameBuffer()->GetRange(&textOffset, &textLen);
	ret = npTrophyReadDecodedSfmText(handle, file, textOffset + headerOffset, textLen,
					 out->title, sizeof(out->title));
	if (ret < 0)
		return ret;

	mutableConf->GetTitleDetailBuffer()->GetRange(&textOffset, &textLen);
	return npTrophyReadDecodedSfmText(handle, file, textOffset + headerOffset, textLen,
					  out->description, sizeof(out->description));
}

/* 0x812D7894 */
int npTrophyExtractGroupTextInfo(TrophyHandle *handle, TrophyFileBase *file,
				 const TrophyConfHandle *confHandle, int groupId,
				 void *outGroupDetails)
{
	struct GroupDetailsLayout {
		SceSize structSize;
		int groupId;
		uint32_t numTrophies;
		TrophyGradeCounts gradeCounts;
		char title[128];
		char description[1024];
	};

	const TrophyConfData *conf = confHandle->GetConfData();
	SceSize headerOffset = confHandle->GetStrippedHeaderSize();
	if (!conf->SupportsGroups())
		return NP_TROPHY_ERR_GROUP_UNSUPPORTED;

	GroupDetailsLayout *out = static_cast<GroupDetailsLayout *>(outGroupDetails);
	out->groupId = groupId;

	TrophyGradeCounts counts;
	int ret = conf->CountGroupByGrade(groupId, &counts);
	if (ret < 0)
		return ret;
	out->numTrophies = npTrophyGradeCountTotal(&counts);
	out->gradeCounts = counts;

	TrophyGrowBuffer *nameBuf = NULL;
	TrophyGrowBuffer *detailBuf = NULL;
	TrophyConfData *mutableConf = const_cast<TrophyConfData *>(conf);
	if (groupId == -1) {
		nameBuf = mutableConf->GetTitleNameBuffer();
		detailBuf = mutableConf->GetTitleDetailBuffer();
	} else {
		TrophyConfGroup *group = NULL;
		ret = conf->GetGroup((uint32_t)groupId, &group);
		if (ret < 0)
			return ret;
		nameBuf = group->GetNameBuffer();
		detailBuf = group->GetDetailBuffer();
	}

	uint64_t textOffset = 0;
	SceSize textLen = 0;
	nameBuf->GetRange(&textOffset, &textLen);
	ret = npTrophyReadDecodedSfmText(handle, file, textOffset + headerOffset, textLen,
					 out->title, sizeof(out->title));
	if (ret < 0)
		return ret;

	detailBuf->GetRange(&textOffset, &textLen);
	return npTrophyReadDecodedSfmText(handle, file, textOffset + headerOffset, textLen,
					  out->description, sizeof(out->description));
}

/* 0x812D7A0C */
int npTrophyExtractTrophyTextInfo(TrophyHandle *handle, TrophyFileBase *file,
				  const TrophyConfHandle *confHandle, int trophyId,
				  bool revealDetails, void *outTrophyDetails)
{
	struct TrophyDetailsLayout {
		SceSize structSize;
		int trophyId;
		int trophyGrade;
		int groupId;
		int hidden;
		char name[128];
		char description[1024];
	};

	const TrophyConfData *conf = confHandle->GetConfData();
	SceSize headerOffset = confHandle->GetStrippedHeaderSize();
	TrophyConfEntry *entry = NULL;
	int ret = conf->GetTrophy((uint32_t)trophyId, &entry);
	if (ret < 0)
		return ret;

	TrophyDetailsLayout *out = static_cast<TrophyDetailsLayout *>(outTrophyDetails);
	out->trophyId = trophyId;
	out->trophyGrade = revealDetails ? entry->GetGrade() : 0;
	out->groupId = entry->GetGroupId();
	out->hidden = entry->IsHidden() ? 1 : 0;

	if (!revealDetails) {
		sceClibMemset(out->name, 0, sizeof(out->name));
		sceClibMemset(out->description, 0, sizeof(out->description));
		return 0;
	}

	uint64_t textOffset = 0;
	SceSize textLen = 0;
	entry->GetNameBuffer()->GetRange(&textOffset, &textLen);
	ret = npTrophyReadDecodedSfmText(handle, file, textOffset + headerOffset, textLen,
					 out->name, sizeof(out->name));
	if (ret < 0)
		return ret;

	entry->GetDetailBuffer()->GetRange(&textOffset, &textLen);
	return npTrophyReadDecodedSfmText(handle, file, textOffset + headerOffset, textLen,
					  out->description, sizeof(out->description));
}

/* =========================================================================
 * TrophyHandlePoolEntry & TrophyHandlePoolTable (0x812D7D04..0x812D7EA7)
 * ========================================================================= */

/* 0x812D7D04 */
TrophyHandlePoolEntry::TrophyHandlePoolEntry()
	: m_slotId(0),
	  m_inUse(0),
	  m_reservedState(1),
	  m_aborted(0),
	  m_ownerTag(0),
	  m_activeFlag(0),
	  m_ownerData(NULL),
	  m_extraData(NULL),
	  m_backPointer(NULL),
	  m_handleKind(-1),
	  m_handle(NULL)
{
}

/* 0x812D7D34 */
TrophyHandlePoolEntry::~TrophyHandlePoolEntry()
{
	if (m_handle != NULL) {
		delete m_handle;
		m_handle = NULL;
	}
	if (m_backPointer != NULL) {
		*m_backPointer = NULL;
		m_backPointer = NULL;
	}
}

/* 0x812D7DC6 */
int TrophyHandlePoolEntry::InitHandle(int handleKind)
{
	m_handle = new TrophyHandle();
	if (m_handle == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	int ret = m_handle->Init();
	if (ret < 0) {
		delete m_handle;
		m_handle = NULL;
		return ret;
	}
	m_handleKind = handleKind;
	return 0;
}

/* 0x812D7E2E */
void TrophyHandlePoolTable::CancelAll(int errorCode)
{
	mutex->Lock();
	for (uint32_t i = 0; i < capacity; ++i) {
		if (entries[i] != NULL && entries[i]->m_handle != NULL)
			entries[i]->m_handle->Cancel(errorCode);
	}
	mutex->Unlock();
}

/* 0x812D7E6E */
void TrophyHandlePoolTable::ClearAllSuspended()
{
	mutex->Lock();
	for (uint32_t i = 0; i < capacity; ++i) {
		if (entries[i] != NULL && entries[i]->m_handle != NULL)
			entries[i]->m_handle->ClearSuspended();
	}
	mutex->Unlock();
}

/* =========================================================================
 * TrophySnsFeedPublisher (0x812D7EF8..0x812D8113)
 * ========================================================================= */

/* 0x812D7EF8 */
bool npTrophyIsSnsFeedEnabled()
{
	int enabled = 0;
	if (npSnsFbIsEnabled(&enabled) < 0)
		return false;
	return enabled != 0;
}

/* 0x812D80C0 / 0x812D80EC */
TrophySnsFeedPublisher::~TrophySnsFeedPublisher()
{
	if (m_snsContext != 0) {
		npSnsFbDestroyContext(m_snsContext);
		m_snsContext = 0;
	}
}

/* 0x812D7F30 */
int TrophySnsFeedPublisher::Publish(TrophyHandle *handle, const char *titleName,
				    const char *iconUrl, const TrophySnsFeedItem *items,
				    int itemCount)
{
	TrophySnsCancelToken cancelToken(m_snsContext);
	int ret = cancelToken.Attach(handle);
	if (ret < 0)
		return ret;

	int serverError = 0;
	int postRet = npSnsFbPostTrophyFeed(m_snsContext, titleName, iconUrl, items, itemCount,
					    &serverError);
	ret = cancelToken.Detach(postRet);
	if (ret < 0)
		return ret;
	return (serverError != 0) ? NP_TROPHY_ERR_SNS_PUBLISH_FAILED : 0;
}

/* 0x812D801E */
int TrophySnsFeedPublisher::PublishBatch(TrophyHandle *handle, const char *titleName,
					 const char *iconUrl, const TrophySnsFeedItem *items,
					 int itemCount)
{
	TrophySnsFeedPublisher publisher;
	int ctxId = npSnsFbCreateContext();
	if (ctxId < 0)
		return ctxId;
	publisher.m_snsContext = ctxId;
	return publisher.Publish(handle, titleName, iconUrl, items, itemCount);
}

/* =========================================================================
 * TrophyConfStorage (0x812D3178..0x812D3E83)
 * ========================================================================= */

/* 0x812D3178 */
TrophyConfStorage::TrophyConfStorage(int userId, const TrophyCommId *commId)
	: m_userId(userId),
	  m_commId(commId)
{
}

/* 0x812D31A4 */
TrophyConfStorage::~TrophyConfStorage()
{
}

bool TrophyConfStorage::IsConfCorruptionError(int err)
{
	return err == NP_TROPHY_ERR_CONF_MARKED_BROKEN || err == NP_TROPHY_ERR_SFM_NOT_FOUND ||
	       err == NP_TROPHY_ERR_PARAM_INI_CORRUPT || err == NP_TROPHY_ERR_BROKEN_CONF;
}

/* 0x812D320E */
int TrophyConfStorage::DetermineAssetMask(TrophyHandle *handle, int userId, uint32_t *outMask)
{
	(void)handle;
	bool hasUx0 = false;
	int ret = npTrophyUx0RootExists(userId, &hasUx0);
	if (ret < 0)
		return ret;
	*outMask = hasUx0 ? 23u : 7u;
	return 0;
}

/* 0x812D3252 */
int TrophyConfStorage::EstimateInstallSpace(TrophyHandle *handle, TrophyConfSource *source,
					    int userId, const TrophyCommId *commId, int lang,
					    uint32_t assetMask, uint64_t *outUr0, uint64_t *outUx0)
{
	TrophyExtractOptions opts;
	opts.userId = userId;
	opts.commId = commId;
	opts.parseOptions = 1;
	opts.lang = lang;
	opts.assetMask = assetMask;
	opts.flags = 0;
	return source->EstimateSpace(handle, &opts, outUr0, outUx0);
}

/* 0x812D32B8 */
int TrophyConfStorage::InstallFromSource(TrophyHandle *handle, TrophyConfSource *source, int lang,
					 uint32_t assetMask, TrophyConfMeta *outMeta,
					 TrophyConfData **outDefaultConf,
					 TrophyConfHandle **outLocalizedConf)
{
	bool writeUr0 = (assetMask & 0x0Fu) != 0;
	bool writeUx0 = (assetMask & 0x10u) != 0;

	if (writeUr0) {
		int ret = npTrophyEnsureUr0ConfDir(m_userId, m_commId, kActiveDirSuffix);
		if (ret < 0)
			return ret;
	}
	if (writeUx0) {
		int ret = npTrophyEnsureUx0ConfDir(m_userId, m_commId, kActiveDirSuffix);
		if (ret < 0)
			return ret;
	}

	char confDirPath[256];
	int ret = npTrophyUr0ConfDirPath(confDirPath, m_userId, m_commId, kActiveDirSuffix);
	if (ret < 0)
		return ret;

	TrophyExtractOptions opts;
	opts.userId = m_userId;
	opts.commId = m_commId;
	opts.parseOptions = 1;
	opts.lang = lang;
	opts.assetMask = assetMask;
	opts.flags = 0;

	TrophyExtractedAssetReport report;
	report.Init();
	TrophyConfData *defaultConf = NULL;
	TrophyConfHandle *localizedConf = NULL;

	ret = source->ExtractAssets(handle, &opts, outMeta, &report, &defaultConf,
				    outLocalizedConf != NULL ? &localizedConf : NULL);
	if (ret >= 0) {
		if ((defaultConf->GetPlatformMask() & TROPHY_PLATFORM_PSP2) == 0 ||
		    defaultConf->GetConfVersion() != TROPHY_FORMAT_VERSION_V1_1) {
			ret = NP_TROPHY_ERR_UNSUPPORTED_VERSION;
		} else if (outLocalizedConf != NULL && localizedConf != NULL) {
			const TrophyConfData *locData = localizedConf->GetConfData();
			if ((locData->GetPlatformMask() & TROPHY_PLATFORM_PSP2) == 0 ||
			    locData->GetConfVersion() != TROPHY_FORMAT_VERSION_V1_1) {
				ret = NP_TROPHY_ERR_UNSUPPORTED_VERSION;
			}
		}
	}

	if (ret >= 0) {
		uint32_t appVersion = defaultConf->GetTrophySetVersion();
		if (writeUr0) {
			char ur0IniPath[256];
			ret = npTrophyUr0ConfFilePath(ur0IniPath, m_userId, m_commId,
						      "TRPPARAM.INI", kActiveDirSuffix);
			if (ret >= 0) {
				TrophyParamIniValues vals;
				vals.sysVersion = TROPHY_FORMAT_VERSION_V1;
				vals.commId = m_commId;
				vals.appVersion = appVersion;
				vals.lang = lang;
				ret = m_ur0ParamIni.Create(ur0IniPath, &vals);
			}
			if (ret >= 0)
				ret = m_ur0ParamIni.Flush();
		}
		if (ret >= 0 && writeUx0) {
			char ux0IniPath[256];
			ret = npTrophyUx0ConfFilePath(ux0IniPath, m_userId, m_commId,
						      "TRPPARAM.INI", kActiveDirSuffix);
			if (ret >= 0) {
				TrophyParamIniValues vals;
				vals.sysVersion = TROPHY_FORMAT_VERSION_V1;
				vals.commId = m_commId;
				vals.appVersion = appVersion;
				vals.lang = lang;
				ret = m_ux0ParamIni.Create(ux0IniPath, &vals);
			}
			if (ret >= 0)
				ret = m_ux0ParamIni.Flush();
		}
	}

	if (ret >= 0) {
		if (outDefaultConf != NULL) {
			*outDefaultConf = defaultConf;
			defaultConf = NULL;
		}
		if (outLocalizedConf != NULL) {
			*outLocalizedConf = localizedConf;
			localizedConf = NULL;
		}
		ret = 0;
	}

	if (defaultConf != NULL)
		delete defaultConf;
	if (localizedConf != NULL)
		delete localizedConf;
	return ret;
}

/* 0x812D3544 */
int TrophyConfStorage::CheckBrokenMarker(TrophyHandle *handle, bool *outBroken)
{
	*outBroken = false;
	TrophySimpleRecordFile statFile(TROPHY_MAGIC_CONF_STAT);
	char statPath[256];
	bool exists = false;
	if (npTrophyUr0ConfFilePath(statPath, m_userId, m_commId, "TRPSTAT.DAT",
				    kActiveDirSuffix) >= 0 &&
	    npTrophyPathExists(statPath, &exists) >= 0 && exists) {
		TrophySimpleHeader hdr;
		hdr.formatVersion = 0;
		hdr.flags = 0;
		if (statFile.Open(handle, statPath, kTrophyOpenFlagsReadOnly, 0) >= 0 &&
		    statFile.ReadHeader(handle, &hdr) >= 0 && statFile.Close() >= 0) {
			*outBroken = true;
		}
	}
	return 0;
}

/* 0x812D37C6 */
int TrophyConfStorage::WriteBrokenMarker(TrophyHandle *handle, const TrophyBrokenMarker *marker)
{
	TrophySimpleRecordFile statFile(TROPHY_MAGIC_CONF_STAT);
	char statPath[256];
	bool exists = false;
	if (npTrophyUr0ConfFilePath(statPath, m_userId, m_commId, "TRPSTAT.DAT",
				    kActiveDirSuffix) >= 0 &&
	    npTrophyPathExists(statPath, &exists) >= 0 && !exists) {
		TrophySimpleHeader hdr;
		hdr.formatVersion = marker->reason;
		hdr.flags = (uint32_t)marker->errorCode;
		if (statFile.Create(handle, statPath, kTrophyOpenFlagsCreate, kTrophyFileMode,
				    &hdr) >= 0) {
			statFile.Close();
		}
	}
	return 0;
}

/* 0x812D35F8 */
int TrophyConfStorage::Open(TrophyHandle *handle)
{
	bool hasUx0 = false;
	int ret = npTrophyUx0RootExists(m_userId, &hasUx0);
	if (ret < 0)
		return ret;

	char ur0IniPath[256];
	ret = npTrophyUr0ConfFilePath(ur0IniPath, m_userId, m_commId, "TRPPARAM.INI",
				      kActiveDirSuffix);
	if (ret >= 0)
		ret = m_ur0ParamIni.Open(ur0IniPath, m_commId);
	if (ret < 0) {
		if (m_ur0ParamIni.IsOpen())
			m_ur0ParamIni.Close();
		return ret;
	}

	if (hasUx0) {
		char ux0IniPath[256];
		if (npTrophyUx0ConfFilePath(ux0IniPath, m_userId, m_commId, "TRPPARAM.INI",
					    kActiveDirSuffix) < 0 ||
		    m_ux0ParamIni.Open(ux0IniPath, m_commId) < 0) {
			if (m_ux0ParamIni.IsOpen())
				m_ux0ParamIni.Close();
		}
	}

	bool broken = false;
	ret = CheckBrokenMarker(handle, &broken);
	if (ret < 0)
		return ret;
	return broken ? NP_TROPHY_ERR_CONF_MARKED_BROKEN : 0;
}

/* 0x812D3704 */
int TrophyConfStorage::Close()
{
	if (m_ur0ParamIni.IsOpen()) {
		int ret = m_ur0ParamIni.Close();
		if (ret < 0)
			return ret;
	}
	if (m_ux0ParamIni.IsOpen()) {
		int ret = m_ux0ParamIni.Close();
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* 0x812D3746 */
int TrophyConfStorage::NeedsUx0Refresh(TrophyHandle *handle, int userId,
				       const TrophyCommId *commId, bool *outNeedsRefresh)
{
	(void)handle;
	(void)commId;
	bool hasUx0 = false;
	int ret = npTrophyUx0RootExists(userId, &hasUx0);
	if (ret < 0)
		return ret;
	if (!hasUx0) {
		*outNeedsRefresh = false;
		return 0;
	}
	if (!m_ux0ParamIni.IsOpen()) {
		*outNeedsRefresh = true;
		return 0;
	}
	uint32_t ur0Ver = 0;
	uint32_t ux0Ver = 0;
	ret = m_ur0ParamIni.ReadAppVersion(&ur0Ver);
	if (ret < 0)
		return ret;
	ret = m_ux0ParamIni.ReadAppVersion(&ux0Ver);
	if (ret < 0)
		return ret;
	*outNeedsRefresh = (ur0Ver > ux0Ver);
	return 0;
}

/* 0x812D3870 */
int TrophyConfStorage::OpenSfmStream(TrophyHandle *handle, TrophyConfStream *stream)
{
	return stream->OpenSfm(handle, m_userId, m_commId, -1, true);
}

/* 0x812D3898 */
int TrophyConfStorage::ReadConfData(TrophyHandle *handle, uint32_t parseOptions,
				    TrophyConfData **outConf)
{
	TrophyConfStream stream;
	TrophySignedSfmReader reader;

	int ret = stream.OpenSfm(handle, m_userId, m_commId, -1, true);
	if (ret >= 0)
		ret = reader.Init(parseOptions);
	if (ret >= 0)
		ret = reader.Parse(handle, &stream);
	if (ret >= 0)
		ret = reader.TakeConfData(m_commId, outConf);

	if (stream.IsOpen())
		stream.Close();
	if (IsConfCorruptionError(ret)) {
		TrophyBrokenMarker marker;
		marker.reason = 0;
		marker.errorCode = ret;
		WriteBrokenMarker(handle, &marker);
	}
	return ret < 0 ? ret : 0;
}

/* 0x812D398A */
int TrophyConfStorage::ReadConfHandle(TrophyHandle *handle, TrophyConfHandle **outHandle)
{
	TrophyConfStream stream;
	TrophySignedSfmReader reader;

	int ret = stream.OpenSfm(handle, m_userId, m_commId, -1, true);
	if (ret >= 0)
		ret = reader.Init(1);
	if (ret >= 0)
		ret = reader.Parse(handle, &stream);
	if (ret >= 0)
		ret = reader.TakeConfHandle(m_commId, -3, outHandle);

	if (stream.IsOpen())
		stream.Close();
	if (IsConfCorruptionError(ret)) {
		TrophyBrokenMarker marker;
		marker.reason = 0;
		marker.errorCode = ret;
		WriteBrokenMarker(handle, &marker);
	}
	return ret < 0 ? ret : 0;
}

/* 0x812D3A86 */
int TrophyConfStorage::ExtractTitleText(TrophyHandle *handle, const TrophyConfHandle *confHandle,
					void *outTitleDetails)
{
	TrophyConfStream stream;
	int ret = stream.OpenSfm(handle, m_userId, m_commId, -1, true);
	if (ret >= 0)
		ret = npTrophyExtractTitleTextInfo(handle, &stream, confHandle, outTitleDetails);
	if (stream.IsOpen())
		stream.Close();
	if (IsConfCorruptionError(ret)) {
		TrophyBrokenMarker marker;
		marker.reason = 0;
		marker.errorCode = ret;
		WriteBrokenMarker(handle, &marker);
	}
	return ret < 0 ? ret : 0;
}

/* 0x812D3B52 */
int TrophyConfStorage::ExtractTrophyText(TrophyHandle *handle, const TrophyConfHandle *confHandle,
					 int trophyId, bool revealDetails, void *outTrophyDetails)
{
	TrophyConfStream stream;
	int ret = stream.OpenSfm(handle, m_userId, m_commId, -1, true);
	if (ret >= 0)
		ret = npTrophyExtractTrophyTextInfo(handle, &stream, confHandle, trophyId,
						    revealDetails, outTrophyDetails);
	if (stream.IsOpen())
		stream.Close();
	if (IsConfCorruptionError(ret)) {
		TrophyBrokenMarker marker;
		marker.reason = 0;
		marker.errorCode = ret;
		WriteBrokenMarker(handle, &marker);
	}
	return ret < 0 ? ret : 0;
}

/* 0x812D3C36 */
int TrophyConfStorage::OpenTitleIcon(TrophyHandle *handle, TrophyConfIconFile *iconFile)
{
	return iconFile->OpenTitleIcon(handle, m_userId, m_commId, -2, true);
}

/* 0x812D3C5E */
int TrophyConfStorage::OpenGroupIcon(TrophyHandle *handle, int groupId,
				     TrophyConfIconFile *iconFile)
{
	return iconFile->OpenGroupIcon(handle, m_userId, m_commId, groupId, -2, true);
}

/* 0x812D3C88 */
int TrophyConfStorage::OpenTrophyIcon(TrophyHandle *handle, int trophyId,
				      TrophyConfIconFile *iconFile)
{
	if (!m_ux0ParamIni.IsOpen())
		return NP_TROPHY_ERR_ICON_NOT_FOUND;
	return iconFile->OpenTrophyIcon(handle, m_userId, m_commId, trophyId);
}

/* 0x812D3CC4 */
int TrophyConfStorage::RemoveDirectories(int userId, const TrophyCommId *commId)
{
	int ret = npTrophyRemoveUr0ConfDir(userId, commId, kActiveDirSuffix);
	if (ret < 0)
		return ret;
	bool hasUx0 = false;
	ret = npTrophyUx0RootExists(userId, &hasUx0);
	if (ret < 0)
		return ret;
	if (!hasUx0)
		return 0;
	return npTrophyRemoveUx0ConfDir(userId, commId, kActiveDirSuffix);
}

/* 0x812D3D2A */
int TrophyConfStorage::BackupDirectories(int userId, const TrophyCommId *commId,
					 uint8_t *outBackupFlags)
{
	int ret = npTrophyRemoveUr0ConfDir(userId, commId, kBackupDirSuffix);
	if (ret < 0)
		return ret;
	bool hasUx0 = false;
	ret = npTrophyUx0RootExists(userId, &hasUx0);
	if (ret < 0)
		return ret;
	if (hasUx0) {
		ret = npTrophyRemoveUx0ConfDir(userId, commId, kBackupDirSuffix);
		if (ret < 0)
			return ret;
	}
	ret = npTrophyRenameUr0ConfDir(userId, commId, kActiveDirSuffix, kBackupDirSuffix);
	if (ret < 0)
		return ret;
	outBackupFlags[0] = 1;

	bool hasUx0Dir = false;
	ret = npTrophyUx0ConfDirExists(userId, commId, &hasUx0Dir, kActiveDirSuffix);
	if (ret < 0)
		return ret;
	if (hasUx0Dir) {
		ret = npTrophyRenameUx0ConfDir(userId, commId, kActiveDirSuffix, kBackupDirSuffix);
		if (ret < 0)
			return ret;
		outBackupFlags[1] = 1;
	}
	return 0;
}

/* 0x812D3DFC */
int TrophyConfStorage::RestoreDirectories(int userId, const TrophyCommId *commId,
					  uint8_t *ioBackupFlags)
{
	if (ioBackupFlags[1] != 0) {
		int ret = npTrophyRemoveUx0ConfDir(userId, commId, kActiveDirSuffix);
		if (ret < 0)
			return ret;
		ret = npTrophyRenameUx0ConfDir(userId, commId, kBackupDirSuffix, kActiveDirSuffix);
		if (ret < 0)
			return ret;
		ioBackupFlags[1] = 0;
	}
	if (ioBackupFlags[0] != 0) {
		int ret = npTrophyRemoveUr0ConfDir(userId, commId, kActiveDirSuffix);
		if (ret < 0)
			return ret;
		ret = npTrophyRenameUr0ConfDir(userId, commId, kBackupDirSuffix, kActiveDirSuffix);
		if (ret < 0)
			return ret;
		ioBackupFlags[0] = 0;
	}
	return 0;
}

/* =========================================================================
 * TrophyUserStorage (0x812D3E84..0x812D59D3)
 * ========================================================================= */

/* 0x812D3E84 */
TrophyUserStorage::TrophyUserStorage(TrophyStorageArbiter *arbiter,
				     const SceKernelOpenPsId *openPsId, int userId,
				     const TrophyCommId *commId)
	: m_openPsId(openPsId),
	  m_arbiter(arbiter),
	  m_userId(userId),
	  m_commId(commId),
	  m_accessMode(0),
	  m_activeTxnMask(0),
	  m_formatFlags(0)
{
}

/* 0x812D3EF0 */
TrophyUserStorage::~TrophyUserStorage()
{
	if (m_titleFile.IsOpen())
		m_titleFile.Close();
	if (m_transFile.IsOpen())
		m_transFile.Close();
	if (m_dirGuard.IsAcquired())
		m_dirGuard.Release();
}

/* 0x812D47CC */
bool TrophyUserStorage::IsOpen() const
{
	return m_titleFile.IsOpen() || m_transFile.IsOpen();
}

/* 0x812D3F8C */
int TrophyUserStorage::CreateTransFile(TrophyHandle *handle, uint32_t formatFlags,
				       const void *serverTag)
{
	char transPath[256];
	int ret = npTrophyJoinBaseFile(transPath, m_dirGuard.GetPath(), "TRPTRANS.DAT");
	if (ret >= 0) {
		uint64_t seq = 0;
		ret = npTrophyGenerateInitialSequence(&seq);
		if (ret >= 0) {
			TrophyAccountCache accountCache;
			TrophyTransCreateParams params;
			params.formatFlags = formatFlags;
			params.openPsId = m_openPsId;
			params.accountCache = &accountCache;
			params.commId = m_commId;
			params.serverTag = serverTag;
			params.initialSequence = seq;
			params.initialJournalState = 1;
			params.capacity = TROPHY_DEFAULT_TRANS_CAPACITY;
			ret = m_transFile.CreateSimple(handle, transPath, kTrophyOpenFlagsCreate,
						       kTrophyFileMode, &params);
			if (ret >= 0)
				return 0;
		}
	}
	if (m_transFile.IsOpen())
		m_transFile.Close();
	return ret;
}

/* 0x812D4044 */
int TrophyUserStorage::CreateTitleFile(TrophyHandle *handle, uint32_t formatFlags,
				       const TrophyDualTick *ticks, const TrophyConfMeta *confMeta,
				       uint32_t titleFlags, const TrophyConfData *confData)
{
	char titlePath[256];
	int ret = npTrophyJoinBaseFile(titlePath, m_dirGuard.GetPath(), "TRPTITLE.DAT");
	if (ret >= 0) {
		TrophyAccountCache accountCache;
		TrophyTitleMigrateParams params;
		params.formatFlags = formatFlags;
		params.openPsId = m_openPsId;
		params.accountCache = &accountCache;
		params.commId = m_commId;
		params.confMeta = confMeta;
		params.titleFlags = titleFlags;
		params.confData = confData;
		params.modifiedTick = ticks;
		ret = m_titleFile.CreateFresh(handle, titlePath, kTrophyOpenFlagsCreate,
					      kTrophyFileMode, &params);
		if (ret >= 0)
			return 0;
	}
	if (m_titleFile.IsOpen())
		m_titleFile.Close();
	return ret;
}

/* 0x812D4126 */
int TrophyUserStorage::Create(TrophyHandle *handle, const TrophyUserStorageInstallArgs *args,
			      uint32_t fileMask)
{
	uint32_t createdMask = 0;
	int ret = m_dirGuard.AcquireTitleData(m_arbiter, m_userId, m_commId);
	if (ret >= 0 && (fileMask & 1u) != 0) {
		ret = CreateTitleFile(handle, TROPHY_FORMAT_VERSION_V1, args->ticks,
				      args->confMeta, args->titleFlags, args->confData);
		if (ret >= 0)
			createdMask |= 1u;
	}
	if (ret >= 0 && (fileMask & 2u) != 0) {
		ret = CreateTransFile(handle, TROPHY_FORMAT_VERSION_V1, args->serverTag);
		if (ret >= 0) {
			TrophyTransRegisterInput regIn;
			regIn.ticks = args->ticks;
			regIn.deferredFlag = args->deferredFlag;
			ret = m_transFile.AddRegisterEntry(handle, &regIn);
			if (ret >= 0)
				createdMask |= 2u;
		}
	}

	if (ret >= 0) {
		m_activeTxnMask = createdMask;
		m_formatFlags = TROPHY_FORMAT_VERSION_V1;
		m_accessMode = 2;
		return 0;
	}

	if (m_titleFile.IsOpen())
		m_titleFile.Close();
	if (m_transFile.IsOpen())
		m_transFile.Close();
	if (m_dirGuard.IsAcquired())
		m_dirGuard.Release();
	return ret;
}

/* 0x812D422E */
int TrophyUserStorage::CreateMigrated(TrophyHandle *handle,
				      const TrophyUserStorageInstallArgs *args, uint32_t fileMask,
				      TrophyUserStorage *oldStorage)
{
	if (fileMask != 3u)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	int ret = m_dirGuard.AcquireTitleData(m_arbiter, m_userId, m_commId);
	if (ret >= 0) {
		char titlePath[256];
		ret = npTrophyJoinBaseFile(titlePath, m_dirGuard.GetPath(), "TRPTITLE.DAT");
		if (ret >= 0) {
			TrophyAccountCache accountCache;
			TrophyTitleMigrateParams titleParams;
			titleParams.formatFlags = TROPHY_FORMAT_VERSION_V1;
			titleParams.openPsId = m_openPsId;
			titleParams.accountCache = &accountCache;
			titleParams.commId = m_commId;
			titleParams.confMeta = args->confMeta;
			titleParams.titleFlags = args->titleFlags;
			titleParams.confData = args->confData;
			titleParams.modifiedTick = args->ticks;
			ret = m_titleFile.CreateMigrated(handle, titlePath, kTrophyOpenFlagsCreate,
							 kTrophyFileMode, &oldStorage->m_titleFile,
							 &titleParams);
		}
		if (ret >= 0) {
			char transPath[256];
			ret = npTrophyJoinBaseFile(transPath, m_dirGuard.GetPath(),
						   "TRPTRANS.DAT");
			if (ret >= 0) {
				TrophyAccountCache accountCache;
				TrophyTransMigrateParams transParams;
				transParams.formatFlags = TROPHY_FORMAT_VERSION_V1;
				transParams.openPsId = m_openPsId;
				transParams.accountCache = &accountCache;
				transParams.commId = m_commId;
				transParams.serverTag = args->serverTag;
				transParams.capacity = TROPHY_DEFAULT_TRANS_CAPACITY;
				ret = m_transFile.CreateMigrated(handle, transPath,
								 kTrophyOpenFlagsCreate,
								 kTrophyFileMode,
								 &oldStorage->m_transFile,
								 &transParams);
			}
		}
	}

	if (ret >= 0) {
		m_accessMode = 2;
		m_activeTxnMask = 3u;
		m_formatFlags = TROPHY_FORMAT_VERSION_V1;
		return 0;
	}

	if (m_titleFile.IsOpen())
		m_titleFile.Close();
	if (m_transFile.IsOpen())
		m_transFile.Close();
	if (m_dirGuard.IsAcquired())
		m_dirGuard.Release();
	return ret;
}

/* 0x812D43B0 */
int TrophyUserStorage::CheckBrokenMarker(TrophyHandle *handle, bool *outBroken)
{
	*outBroken = false;
	TrophySimpleRecordFile statFile(TROPHY_MAGIC_DATA_STAT);
	if (m_dirGuard.IsAcquired()) {
		char statPath[256];
		bool exists = false;
		if (npTrophyJoinBaseFile(statPath, m_dirGuard.GetPath(), "TRPSTAT.DAT") >= 0 &&
		    npTrophyPathExists(statPath, &exists) >= 0 && exists) {
			TrophySimpleHeader hdr;
			hdr.formatVersion = 0;
			hdr.flags = 0;
			if (statFile.Open(handle, statPath, kTrophyOpenFlagsReadOnly, 0) >= 0 &&
			    statFile.ReadHeader(handle, &hdr) >= 0 && statFile.Close() >= 0) {
				*outBroken = true;
			}
		}
	}
	return 0;
}

/* 0x812D5868 */
int TrophyUserStorage::WriteBrokenMarker(TrophyHandle *handle, const TrophyBrokenMarker *marker)
{
	TrophySimpleRecordFile statFile(TROPHY_MAGIC_DATA_STAT);
	if (m_dirGuard.IsAcquired()) {
		char statPath[256];
		bool exists = false;
		if (npTrophyJoinBaseFile(statPath, m_dirGuard.GetPath(), "TRPSTAT.DAT") >= 0 &&
		    npTrophyPathExists(statPath, &exists) >= 0 && !exists) {
			TrophySimpleHeader hdr;
			hdr.formatVersion = marker->reason;
			hdr.flags = (uint32_t)marker->errorCode;
			if (statFile.Create(handle, statPath, kTrophyOpenFlagsCreate,
					    kTrophyFileMode, &hdr) >= 0) {
				statFile.Close();
			}
		}
	}
	return 0;
}

/* 0x812D4468 */
int TrophyUserStorage::OpenTransFile(TrophyHandle *handle, int accessMode)
{
	int ioFlags = (accessMode == 2) ? kTrophyOpenFlagsReadWrite : kTrophyOpenFlagsReadOnly;
	char transPath[256];
	int ret = npTrophyJoinBaseFile(transPath, m_dirGuard.GetPath(), "TRPTRANS.DAT");
	if (ret >= 0) {
		TrophyAccountCache accountCache;
		ret = m_transFile.Open(handle, transPath, ioFlags, 0, &accountCache, m_commId);
		if (ret >= 0) {
			uint32_t formatFlags = 0;
			ret = m_transFile.GetFormatFlags(&formatFlags);
			if (ret >= 0) {
				if (formatFlags == TROPHY_FORMAT_VERSION_V1)
					return 0;
				ret = NP_TROPHY_ERR_FORMAT_VERSION_MISMATCH;
			}
		}
	}
	if (m_transFile.IsOpen())
		m_transFile.Close();
	return ret;
}

/* 0x812D4520 */
int TrophyUserStorage::OpenTitleFile(TrophyHandle *handle, int accessMode)
{
	int ioFlags = (accessMode == 2) ? kTrophyOpenFlagsReadWrite : kTrophyOpenFlagsReadOnly;
	char titlePath[256];
	int ret = npTrophyJoinBaseFile(titlePath, m_dirGuard.GetPath(), "TRPTITLE.DAT");
	if (ret >= 0) {
		TrophyAccountCache accountCache;
		ret = m_titleFile.Open(handle, titlePath, ioFlags, 0, &accountCache, m_commId,
				       NULL);
		if (ret >= 0) {
			uint32_t formatFlags = 0;
			ret = m_titleFile.GetFormatFlags(&formatFlags);
			if (ret >= 0) {
				if (formatFlags == TROPHY_FORMAT_VERSION_V1)
					return 0;
				ret = NP_TROPHY_ERR_FORMAT_VERSION_MISMATCH;
			}
		}
	}
	if (m_titleFile.IsOpen())
		m_titleFile.Close();
	return ret;
}

/* 0x812D45DA */
int TrophyUserStorage::Open(TrophyHandle *handle, uint32_t openFlags, int accessMode)
{
	int ret = 0;
	if ((openFlags & 0x10000000u) != 0)
		ret = m_dirGuard.AcquireTitleBackup(m_arbiter, m_userId, m_commId);
	else
		ret = m_dirGuard.AcquireTitleData(m_arbiter, m_userId, m_commId);

	uint32_t titleFlagsVer = 0;
	uint32_t transFlagsVer = 0;

	if (ret >= 0 && (openFlags & 1u) != 0) {
		ret = OpenTitleFile(handle, accessMode);
		if (ret >= 0)
			ret = m_titleFile.GetFormatFlags(&titleFlagsVer);
		if (ret >= 0) {
			TrophyTitleDataSnapshot snap;
			snap.Clear();
			ret = m_titleFile.ReadSnapshot(handle, &snap);
			if (ret >= 0 && snap.confMeta.sourceType == 2 && (openFlags & 2u) != 0)
				openFlags &= ~2u;
		}
	}

	if (ret >= 0 && (openFlags & 2u) != 0) {
		if ((openFlags & 1u) == 0) {
			ret = NP_TROPHY_ERR_INVALID_ARGUMENT;
		} else {
			ret = OpenTransFile(handle, accessMode);
			if (ret >= 0)
				ret = m_transFile.GetFormatFlags(&transFlagsVer);
			if (ret >= 0 && transFlagsVer != titleFlagsVer)
				ret = NP_TROPHY_ERR_FLAGS_MISMATCH;
		}
	}

	if (ret >= 0) {
		if (titleFlagsVer == 0 && transFlagsVer == 0) {
			ret = NP_TROPHY_ERR_INVALID_ARGUMENT;
		} else {
			bool broken = false;
			ret = CheckBrokenMarker(handle, &broken);
			if (ret >= 0) {
				if (broken) {
					ret = NP_TROPHY_ERR_DATA_MARKED_BROKEN;
				} else if (titleFlagsVer != TROPHY_FORMAT_VERSION_V1) {
					ret = NP_TROPHY_ERR_FORMAT_VERSION_MISMATCH;
				} else {
					m_formatFlags = TROPHY_FORMAT_VERSION_V1;
					m_activeTxnMask = 0;
					m_accessMode = accessMode;
					return 0;
				}
			}
		}
	}

	if (m_titleFile.IsOpen())
		m_titleFile.Close();
	if (m_transFile.IsOpen())
		m_transFile.Close();
	if (m_dirGuard.IsAcquired())
		m_dirGuard.Release();
	return ret;
}

/* 0x812D4780 */
int TrophyUserStorage::Close()
{
	if (m_titleFile.IsOpen()) {
		int ret = m_titleFile.Close();
		if (ret < 0)
			return ret;
	}
	if (m_transFile.IsOpen()) {
		int ret = m_transFile.Close();
		if (ret < 0)
			return ret;
	}
	if (m_dirGuard.IsAcquired())
		return m_dirGuard.Release();
	return 0;
}

/* 0x812D47F0 */
int TrophyUserStorage::ReadTitleRecord(TrophyHandle *handle, TrophyTitleRecord *outRecord)
{
	outRecord->commId = *m_commId;
	if (m_titleFile.IsOpen()) {
		int ret = m_titleFile.ReadSnapshot(handle, &outRecord->titleSnapshot);
		if (ret < 0)
			return ret;
	} else {
		outRecord->titleSnapshot.Clear();
	}

	if (!m_transFile.IsOpen()) {
		outRecord->transJournal.Clear();
		outRecord->transCounts.Clear();
		return 0;
	}

	int ret = m_transFile.GetJournal(handle, &outRecord->transJournal);
	if (ret < 0)
		return ret;
	return m_transFile.GetCounts(handle, &outRecord->transCounts);
}

/* 0x812D48A8 */
int TrophyUserStorage::VerifyTitleRecord(TrophyHandle *handle, const TrophyTitleRecord *expected)
{
	if (m_titleFile.IsOpen()) {
		int ret = m_titleFile.VerifySnapshot(handle, &expected->titleSnapshot);
		if (ret < 0)
			return ret;
	}
	if (m_transFile.IsOpen()) {
		int ret = m_transFile.VerifyJournal(handle, &expected->transJournal);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* 0x812D48F4 */
int TrophyUserStorage::BeginTransactions(TrophyHandle *handle, uint32_t journalState,
					 uint32_t fileMask)
{
	TrophyAccountCache accountCache;
	bool bound = false;
	uint64_t accountId = 0;

	if ((fileMask & 1u) != 0) {
		int ret = m_titleFile.BeginTransaction(handle, journalState, 1024);
		if (ret < 0)
			return ret;
		ret = m_titleFile.BindAccountIfUnbound(handle, &accountCache, &bound, &accountId);
		if (ret < 0)
			return ret;
	}
	if (m_transFile.IsOpen() && (fileMask & 2u) != 0) {
		int ret = m_transFile.BeginTransaction(handle, journalState, 1024);
		if (ret < 0)
			return ret;
		ret = m_transFile.BindAccountIfUnbound(handle, &accountCache, &bound, &accountId);
		if (ret < 0)
			return ret;
	}
	m_activeTxnMask = fileMask;
	return 0;
}

/* 0x812D4996 */
int TrophyUserStorage::CommitTransactions(TrophyHandle *handle, const TrophyDualTick *ticks,
					  TrophyTitleRecord *outRecord)
{
	if ((m_activeTxnMask & 1u) != 0) {
		int ret = m_titleFile.CommitTransaction(handle, ticks);
		if (ret < 0)
			return ret;
	}
	if (m_transFile.IsOpen() && (m_activeTxnMask & 2u) != 0) {
		int ret = m_transFile.CommitTransaction(handle, ticks);
		if (ret < 0)
			return ret;
	}
	if (m_titleFile.IsOpen()) {
		int ret = m_titleFile.ReadSnapshot(handle, &outRecord->titleSnapshot);
		if (ret < 0)
			return ret;
	}
	if (m_transFile.IsOpen()) {
		int ret = m_transFile.GetJournal(handle, &outRecord->transJournal);
		if (ret < 0)
			return ret;
		ret = m_transFile.GetCounts(handle, &outRecord->transCounts);
		if (ret < 0)
			return ret;
	}
	m_activeTxnMask = 0;
	return 0;
}

/* 0x812D4A3A / 0x812D4B78 */
int TrophyUserStorage::ApplySyncSession(TrophyHandle *handle, const TrophyDualTick *ticks,
					TrophySyncSessionBase *syncSession, uint32_t *outSyncFlags,
					NpTrophyFlagArray *outSyncedTrophies,
					int *outUnlockedPlatinumId)
{
	*outUnlockedPlatinumId = -1;
	*outSyncFlags = 0;
	sceClibMemset(outSyncedTrophies, 0, sizeof(*outSyncedTrophies));

	if (syncSession == NULL) {
		*outSyncFlags = 1u;
		return 0;
	}

	TrophySyncTitleResultBase *result = syncSession->FindResult(m_commId);
	if (result == NULL) {
		if (!m_transFile.IsOpen())
			return 0;
		TrophyTransCounts counts;
		counts.Clear();
		int ret = m_transFile.GetCounts(handle, &counts);
		if (ret < 0)
			return ret;
		if (counts.pendingCount != 0)
			*outSyncFlags |= 0x100u;
		return 0;
	}

	TrophyAccountCache accountCache;
	uint32_t unrecognizedTrophies = 0;
	TrophyUnlockDelta platDelta;
	sceClibMemset(&platDelta, 0, sizeof(platDelta));
	platDelta.trophyId = -1;

	int ret = m_titleFile.MergeSyncResult(handle, &accountCache, result, ticks,
					      &unrecognizedTrophies, &platDelta);
	if (ret < 0)
		return ret;
	if (unrecognizedTrophies != 0)
		*outSyncFlags |= 0x200u;

	if (!m_transFile.IsOpen())
		return 0;

	TrophyTransCounts remaining;
	remaining.Clear();
	ret = m_transFile.ApplySyncResults(handle, result, &remaining, outSyncedTrophies);
	if (ret < 0)
		return ret;
	if (remaining.pendingCount != 0)
		*outSyncFlags |= 0x100u;

	if (platDelta.trophyId != -1 && platDelta.unlocked != 0 && platDelta.unlockedOffline == 0) {
		TrophyTransUnlockInput transIn;
		transIn.trophyId = platDelta.trophyId;
		transIn.grade = platDelta.grade;
		transIn.accountCache = &accountCache;
		transIn.ticks = ticks;
		transIn.deferredFlag = platDelta.unlockedDeferred;
		ret = m_transFile.AddUnlockEntry(handle, &transIn);
		if (ret < 0)
			return ret;
		*outUnlockedPlatinumId = platDelta.trophyId;
	}
	return 0;
}

/* 0x812D4B98 */
int TrophyUserStorage::ApplyRegisterAndSync(TrophyHandle *handle, const TrophyDualTick *ticks,
					    const char *titleId, bool keepDeferred,
					    TrophySyncSessionBase *syncSession,
					    uint32_t *outSyncFlags,
					    NpTrophyFlagArray *outSyncedTrophies,
					    int *outUnlockedPlatinumId)
{
	int ret = m_titleFile.UpdateModifiedTicks(handle, ticks);
	if (ret < 0)
		return ret;
	if (titleId != NULL) {
		ret = m_titleFile.RecordTitleId(handle, titleId);
		if (ret < 0)
			return ret;
	}
	if (!keepDeferred) {
		ret = m_titleFile.ClearAllDeferredFlags(handle);
		if (ret < 0)
			return ret;
		ret = m_transFile.ClearAllDeferred(handle);
		if (ret < 0)
			return ret;
	}
	return ApplySyncSession(handle, ticks, syncSession, outSyncFlags, outSyncedTrophies,
				outUnlockedPlatinumId);
}

/* 0x812D4C26 */
int TrophyUserStorage::UnlockTrophy(TrophyHandle *handle, int trophyId,
				    const TrophyUnlockActionParams *params, uint32_t options,
				    int *outPlatinumId)
{
	if (!m_titleFile.IsOpen() || !m_transFile.IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;

	TrophyAccountCache accountCache;
	TrophyUnlockRequest req;
	req.trophyId = trophyId;
	req.accountCache = &accountCache;
	req.ticks = &params->ticks;
	req.offlineFlag = params->offlineFlag;
	req.deferredFlag = params->deferredFlag;

	TrophyUnlockDelta trophyDelta;
	TrophyUnlockDelta platDelta;
	sceClibMemset(&trophyDelta, 0, sizeof(trophyDelta));
	sceClibMemset(&platDelta, 0, sizeof(platDelta));

	int ret = m_titleFile.UnlockTrophy(handle, &req, options, &trophyDelta, &platDelta);
	if (ret < 0)
		return ret;

	if ((options & 1u) == 0 && trophyDelta.unlockedOffline == 0) {
		TrophyTransUnlockInput transIn;
		transIn.trophyId = trophyDelta.trophyId;
		transIn.grade = trophyDelta.grade;
		transIn.accountCache = &accountCache;
		transIn.ticks = &params->ticks;
		transIn.deferredFlag = trophyDelta.unlockedDeferred;
		ret = m_transFile.AddUnlockEntry(handle, &transIn);
		if (ret < 0)
			return ret;
	}

	if (platDelta.trophyId != -1 && (options & 1u) == 0 && platDelta.unlockedOffline == 0) {
		TrophyTransUnlockInput platIn;
		platIn.trophyId = platDelta.trophyId;
		platIn.grade = 1;
		platIn.accountCache = &accountCache;
		platIn.ticks = &params->ticks;
		platIn.deferredFlag = platDelta.unlockedDeferred;
		ret = m_transFile.AddUnlockEntry(handle, &platIn);
		if (ret < 0)
			return ret;
	}

	*outPlatinumId = platDelta.trophyId;
	return 0;
}

/* 0x812D4D48 */
int TrophyUserStorage::LockTrophy(TrophyHandle *handle, int trophyId, uint32_t options,
				  int *outPlatinumId)
{
	if (!m_titleFile.IsOpen() || !m_transFile.IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;

	TrophyUnlockDelta trophyDelta;
	TrophyUnlockDelta platDelta;
	sceClibMemset(&trophyDelta, 0, sizeof(trophyDelta));
	sceClibMemset(&platDelta, 0, sizeof(platDelta));

	int ret = m_titleFile.LockTrophy(handle, trophyId, options, &trophyDelta, &platDelta);
	if (ret < 0)
		return ret;

	if ((options & 1u) == 0) {
		ret = m_transFile.RemoveUnlockByTrophyId(handle, trophyId);
		if (ret < 0)
			return ret;
		if (platDelta.trophyId != -1) {
			ret = m_transFile.RemoveUnlockByTrophyId(handle, platDelta.trophyId);
			if (ret < 0)
				return ret;
		}
	}

	*outPlatinumId = platDelta.trophyId;
	return 0;
}

/* 0x812D4E0C */
int TrophyUserStorage::UnlockAllTrophies(TrophyHandle *handle,
					 const TrophyUnlockActionParams *params, uint32_t options,
					 int *outPlatinumId)
{
	if (!m_titleFile.IsOpen() || !m_transFile.IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;

	TrophyTitleStaticMeta meta;
	int ret = m_titleFile.ReadTitleStaticMeta(handle, &meta);
	if (ret < 0)
		return ret;
	TrophyTitleProgressSnapshot prog;
	sceClibMemset(&prog, 0, sizeof(prog));
	ret = m_titleFile.ReadTitleProgress(handle, &prog);
	if (ret < 0)
		return ret;

	for (int id = 0; id < 128; ++id) {
		uint32_t bit = 1u << (id & 31);
		if ((meta.allMask.flagBits[id >> 5] & bit) != 0 &&
		    (meta.platinumMask.flagBits[id >> 5] & bit) == 0 &&
		    (prog.unlockedMask.flagBits[id >> 5] & bit) == 0) {
			int platId = -1;
			ret = UnlockTrophy(handle, id, params, options, &platId);
			if (ret < 0)
				return ret;
			if (platId != -1)
				*outPlatinumId = platId;
		}
	}
	return 0;
}

/* 0x812D4F54 */
int TrophyUserStorage::LockAllTrophies(TrophyHandle *handle, uint32_t options, int *outPlatinumId)
{
	if (!m_titleFile.IsOpen() || !m_transFile.IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;

	TrophyTitleStaticMeta meta;
	int ret = m_titleFile.ReadTitleStaticMeta(handle, &meta);
	if (ret < 0)
		return ret;
	TrophyTitleProgressSnapshot prog;
	sceClibMemset(&prog, 0, sizeof(prog));
	ret = m_titleFile.ReadTitleProgress(handle, &prog);
	if (ret < 0)
		return ret;

	for (int id = 0; id < 128; ++id) {
		uint32_t bit = 1u << (id & 31);
		if ((meta.allMask.flagBits[id >> 5] & bit) != 0 &&
		    (meta.platinumMask.flagBits[id >> 5] & bit) == 0 &&
		    (prog.unlockedMask.flagBits[id >> 5] & bit) != 0) {
			int platId = -1;
			ret = LockTrophy(handle, id, options, &platId);
			if (ret < 0)
				return ret;
			if (platId != -1)
				*outPlatinumId = platId;
		}
	}
	return 0;
}

/* 0x812D5096 */
int TrophyUserStorage::UnlockGroupTrophies(TrophyHandle *handle, int groupId,
					   const TrophyUnlockActionParams *params,
					   uint32_t options, int *outPlatinumId)
{
	if (!m_titleFile.IsOpen() || !m_transFile.IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;

	TrophyGroupStaticMeta meta;
	int ret = m_titleFile.ReadGroupStaticMeta(handle, groupId, &meta);
	if (ret < 0)
		return ret;
	TrophyGroupProgressSnapshot prog;
	sceClibMemset(&prog, 0, sizeof(prog));
	ret = m_titleFile.ReadGroupProgress(handle, groupId, &prog);
	if (ret < 0)
		return ret;

	for (int id = 0; id < 128; ++id) {
		uint32_t bit = 1u << (id & 31);
		if ((meta.allMask.flagBits[id >> 5] & bit) != 0 &&
		    (meta.gradeBits.platinum.flagBits[id >> 5] & bit) == 0 &&
		    (prog.unlockedMask.flagBits[id >> 5] & bit) == 0) {
			int platId = -1;
			ret = UnlockTrophy(handle, id, params, options, &platId);
			if (ret < 0)
				return ret;
			if (platId != -1)
				*outPlatinumId = platId;
		}
	}
	return 0;
}

/* 0x812D519C */
int TrophyUserStorage::LockGroupTrophies(TrophyHandle *handle, int groupId, uint32_t options,
					 int *outPlatinumId)
{
	if (!m_titleFile.IsOpen() || !m_transFile.IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;

	TrophyGroupStaticMeta meta;
	int ret = m_titleFile.ReadGroupStaticMeta(handle, groupId, &meta);
	if (ret < 0)
		return ret;
	TrophyGroupProgressSnapshot prog;
	sceClibMemset(&prog, 0, sizeof(prog));
	ret = m_titleFile.ReadGroupProgress(handle, groupId, &prog);
	if (ret < 0)
		return ret;

	for (int id = 0; id < 128; ++id) {
		uint32_t bit = 1u << (id & 31);
		if ((meta.allMask.flagBits[id >> 5] & bit) != 0 &&
		    (meta.gradeBits.platinum.flagBits[id >> 5] & bit) == 0 &&
		    (prog.unlockedMask.flagBits[id >> 5] & bit) != 0) {
			int platId = -1;
			ret = LockTrophy(handle, id, options, &platId);
			if (ret < 0)
				return ret;
			if (platId != -1)
				*outPlatinumId = platId;
		}
	}
	return 0;
}

/* 0x812D528C */
int TrophyUserStorage::GetGroupMask(TrophyHandle *handle, uint32_t *outGroupMask)
{
	TrophyTitleStaticMeta meta;
	int ret = m_titleFile.ReadTitleStaticMeta(handle, &meta);
	if (ret < 0)
		return ret;
	*outGroupMask = meta.groupMask;
	return 0;
}

/* 0x812D52EC */
int TrophyUserStorage::GetUnlockedAndAllMasks(TrophyHandle *handle, NpTrophyFlagArray *outUnlocked,
					      NpTrophyFlagArray *outAll)
{
	TrophyTitleStaticMeta meta;
	int ret = m_titleFile.ReadTitleStaticMeta(handle, &meta);
	if (ret < 0)
		return ret;
	TrophyTitleProgressSnapshot prog;
	sceClibMemset(&prog, 0, sizeof(prog));
	ret = m_titleFile.ReadTitleProgress(handle, &prog);
	if (ret < 0)
		return ret;
	*outUnlocked = prog.unlockedMask;
	*outAll = meta.allMask;
	return 0;
}

/* 0x812D53C4 */
int TrophyUserStorage::GetTitleProgressView(TrophyHandle *handle, void *outStaticInfo,
					    TrophyTitleProgressView *outProgress)
{
	TrophyTitleStaticMeta meta;
	int ret = m_titleFile.ReadTitleStaticMeta(handle, &meta);
	if (ret < 0)
		return ret;
	TrophyTitleProgressSnapshot prog;
	sceClibMemset(&prog, 0, sizeof(prog));
	ret = m_titleFile.ReadTitleProgress(handle, &prog);
	if (ret < 0)
		return ret;

	m_commId->CopyTo(&outProgress->commId);
	outProgress->unlockedCounts.platinum =
		(uint32_t)npTrophyBitsetAnd(&meta.platinumMask, &prog.unlockedMask, NULL);
	outProgress->unlockedCounts.gold =
		(uint32_t)npTrophyBitsetAnd(&meta.goldMask, &prog.unlockedMask, NULL);
	outProgress->unlockedCounts.silver =
		(uint32_t)npTrophyBitsetAnd(&meta.silverMask, &prog.unlockedMask, NULL);
	outProgress->unlockedCounts.bronze =
		(uint32_t)npTrophyBitsetAnd(&meta.bronzeMask, &prog.unlockedMask, NULL);

	TrophyGradeBitsets gradeBits;
	gradeBits.platinum = meta.platinumMask;
	gradeBits.gold = meta.goldMask;
	gradeBits.silver = meta.silverMask;
	gradeBits.bronze = meta.bronzeMask;
	outProgress->progressPercent = (uint32_t)npTrophyComputeProgressFromBitsets(
		&gradeBits, &outProgress->unlockedCounts);
	outProgress->snapshot = prog;

	if (outStaticInfo != NULL)
		npTrophyExportTitleStaticInfo(&meta, outStaticInfo);
	return 0;
}

/* 0x812D553E */
int TrophyUserStorage::GetGroupProgressView(TrophyHandle *handle, int groupId, void *outStaticInfo,
					    TrophyGroupProgressView *outProgress)
{
	TrophyGroupStaticMeta meta;
	int ret = m_titleFile.ReadGroupStaticMeta(handle, groupId, &meta);
	if (ret < 0)
		return ret;
	TrophyGroupProgressSnapshot prog;
	sceClibMemset(&prog, 0, sizeof(prog));
	ret = m_titleFile.ReadGroupProgress(handle, groupId, &prog);
	if (ret < 0)
		return ret;

	outProgress->groupId = groupId;
	outProgress->unlockedCounts.platinum =
		(uint32_t)npTrophyBitsetAnd(&meta.gradeBits.platinum, &prog.unlockedMask, NULL);
	outProgress->unlockedCounts.gold =
		(uint32_t)npTrophyBitsetAnd(&meta.gradeBits.gold, &prog.unlockedMask, NULL);
	outProgress->unlockedCounts.silver =
		(uint32_t)npTrophyBitsetAnd(&meta.gradeBits.silver, &prog.unlockedMask, NULL);
	outProgress->unlockedCounts.bronze =
		(uint32_t)npTrophyBitsetAnd(&meta.gradeBits.bronze, &prog.unlockedMask, NULL);
	outProgress->progressPercent = (uint32_t)npTrophyComputeProgressFromBitsets(
		&meta.gradeBits, &outProgress->unlockedCounts);
	outProgress->snapshot = prog;

	if (outStaticInfo != NULL)
		npTrophyExportGroupStaticInfo(&meta, outStaticInfo);
	return 0;
}

/* 0x812D562C */
int TrophyUserStorage::GetTrophyStateView(TrophyHandle *handle, int trophyId, void *outStaticInfo,
					  TrophyEntryStateView *outState)
{
	TrophyUnlockStateInfo unlockState;
	sceClibMemset(&unlockState, 0, sizeof(unlockState));
	int ret = m_titleFile.ReadTrophyUnlockState(handle, trophyId, &unlockState);
	if (ret < 0)
		return ret;

	TrophyEntryStaticMeta meta;
	ret = m_titleFile.ReadTrophyStaticMeta(handle, trophyId, &meta);
	if (ret < 0)
		return ret;

	outState->trophyId = trophyId;
	uint32_t flags = 0;
	if (unlockState.unlockedOffline != 0)
		flags |= 1u;
	if (unlockState.unlockedDeferred != 0)
		flags |= 2u;
	outState->unlockFlags = flags;
	outState->unlockedSecureTick = unlockState.secureTick;
	outState->unlockedLocalTick = unlockState.localTick;
	outState->unlocked = (unlockState.unlocked != 0) ? 1u : 0u;
	outState->visible = (unlockState.unlocked != 0 || meta.hidden == 0) ? 1u : 0u;

	if (outStaticInfo != NULL)
		npTrophyExportTrophyStaticInfo(&meta, outStaticInfo);
	return 0;
}

/* 0x812D570A */
int TrophyUserStorage::GetGameProgressInfo(TrophyHandle *handle, void *outGameDetails)
{
	struct GameDataLayout {
		SceSize structSize;
		uint32_t unlockedTrophies;
		TrophyGradeCounts unlockedCounts;
		uint32_t progressPercentage;
	};

	uint8_t staticBuf[108];
	TrophyTitleProgressView view;
	int ret = GetTitleProgressView(handle, staticBuf, &view);
	if (ret < 0)
		return ret;

	GameDataLayout *out = static_cast<GameDataLayout *>(outGameDetails);
	out->unlockedTrophies = npTrophyGradeCountTotal(&view.unlockedCounts);
	out->unlockedCounts = view.unlockedCounts;
	out->progressPercentage = view.progressPercent;
	return 0;
}

/* 0x812D575A */
int TrophyUserStorage::GetGroupProgressInfo(TrophyHandle *handle, int groupId,
					    void *outGroupDetails)
{
	struct GroupDataLayout {
		SceSize structSize;
		int groupId;
		uint32_t unlockedTrophies;
		TrophyGradeCounts unlockedCounts;
		uint32_t progressPercentage;
	};

	uint8_t staticBuf[68];
	TrophyGroupProgressView view;
	int ret = GetGroupProgressView(handle, groupId, staticBuf, &view);
	if (ret < 0)
		return ret;

	GroupDataLayout *out = static_cast<GroupDataLayout *>(outGroupDetails);
	out->groupId = groupId;
	out->unlockedTrophies = npTrophyGradeCountTotal(&view.unlockedCounts);
	out->unlockedCounts = view.unlockedCounts;
	out->progressPercentage = view.progressPercent;
	return 0;
}

/* 0x812D57B2 */
int TrophyUserStorage::GetTrophyUnlockInfo(TrophyHandle *handle, int trophyId, bool *outVisible,
					   void *outTrophyData)
{
	struct TrophyDataLayout {
		SceSize structSize;
		int trophyId;
		uint32_t unlocked;
		uint32_t reservedPadding;
		SceRtcTick timestamp;
	};

	uint8_t staticBuf[40];
	TrophyEntryStateView view;
	int ret = GetTrophyStateView(handle, trophyId, staticBuf, &view);
	if (ret < 0)
		return ret;

	*outVisible = (view.visible != 0);
	TrophyDataLayout *out = static_cast<TrophyDataLayout *>(outTrophyData);
	out->trophyId = trophyId;
	out->unlocked = (view.unlocked != 0) ? 1u : 0u;
	out->timestamp = view.unlockedSecureTick;
	return 0;
}

/* 0x812D581E */
int TrophyUserStorage::PrepareUploadSyncSession(TrophyHandle *handle,
						TrophySyncSessionBase *syncSession)
{
	TrophySyncTitleRequestBase *req = NULL;
	int ret = static_cast<TrophyNetSyncBatch *>(syncSession)->AddTitle(m_commId, 128, &req);
	if (ret < 0)
		return ret;
	return m_transFile.PopulateUploadBatch(handle, req);
}

/* 0x812D5916 */
int TrophyUserStorage::RemoveDirectory(int userId, const TrophyCommId *commId)
{
	return npTrophyRemoveUr0DataDir(userId, commId, kActiveDirSuffix);
}

/* 0x812D592E */
int TrophyUserStorage::BackupDirectory(int userId, const TrophyCommId *commId,
				       uint8_t *outBackedUp)
{
	int ret = npTrophyRemoveUr0DataDir(userId, commId, kBackupDirSuffix);
	if (ret < 0)
		return ret;
	ret = npTrophyRenameUr0DataDir(userId, commId, kActiveDirSuffix, kBackupDirSuffix);
	if (ret == NP_TROPHY_ERR_FILE_NOT_FOUND)
		return NP_TROPHY_ERR_TITLE_DAT_MISSING;
	if (ret < 0)
		return ret;
	*outBackedUp = 1;
	return 0;
}

/* 0x812D5988 */
int TrophyUserStorage::RestoreDirectory(int userId, const TrophyCommId *commId,
					uint8_t *ioBackedUp)
{
	if (*ioBackedUp == 0)
		return 0;
	int ret = npTrophyRemoveUr0DataDir(userId, commId, kActiveDirSuffix);
	if (ret < 0)
		return ret;
	ret = npTrophyRenameUr0DataDir(userId, commId, kBackupDirSuffix, kActiveDirSuffix);
	if (ret < 0)
		return ret;
	*ioBackedUp = 0;
	return 0;
}

/* =========================================================================
 * TrophyUserIndexStorage (0x812D59D4..0x812D677F)
 * ========================================================================= */

/* 0x812D59D4 */
TrophyUserIndexStorage::TrophyUserIndexStorage(TrophyStorageArbiter *arbiter,
					       const SceKernelOpenPsId *openPsId, int userId)
	: m_openPsId(openPsId),
	  m_arbiter(arbiter),
	  m_userId(userId),
	  m_isOpen(0)
{
}

/* 0x812D5A08 */
TrophyUserIndexStorage::~TrophyUserIndexStorage()
{
	if (m_indexFile.IsOpen())
		m_indexFile.Close();
	if (m_dirGuard.IsAcquired())
		m_dirGuard.Release();
}

/* 0x812D5AAC */
int TrophyUserIndexStorage::RemoveIndexFile()
{
	TrophyDataDirGuard guard;
	int ret = guard.AcquireUserRoot(m_arbiter, m_userId);
	if (ret < 0)
		return ret;

	char indexPath[256];
	ret = npTrophyJoinBasePath(indexPath, guard.GetPath(), 1, "TRPUSER.DAT");
	if (ret < 0)
		return ret;

	bool exists = false;
	ret = npTrophyPathExists(indexPath, &exists);
	if (ret < 0)
		return ret;
	if (exists) {
		ret = npTrophyRemoveFile(indexPath);
		if (ret < 0)
			return ret;
	}
	return guard.Release();
}

/* 0x812D5B72 */
int TrophyUserIndexStorage::HasAnyInstalledTitleDir(TrophyHandle *handle, int userId,
						    bool *outHasAny)
{
	(void)handle;
	TrophyDir dir;
	char dataRoot[256];
	int ret = npTrophyUr0DataRootPath(dataRoot, userId);
	if (ret < 0)
		return ret;

	bool exists = false;
	ret = npTrophyPathExists(dataRoot, &exists);
	if (ret < 0)
		return ret;
	if (!exists) {
		*outHasAny = false;
		return 0;
	}

	ret = dir.Open(dataRoot, false);
	if (ret < 0)
		return ret;

	*outHasAny = false;
	for (;;) {
		TrophyDirEntry entry;
		bool hasEntry = false;
		ret = dir.Read(&entry, &hasEntry);
		if (ret < 0)
			return ret;
		if (!hasEntry)
			break;
		if (entry.kind == TROPHY_DIR_ENTRY_DIR &&
		    sceClibStrncmp(entry.name, ".", 2) != 0 &&
		    sceClibStrncmp(entry.name, "..", 3) != 0 &&
		    !npTrophyIsSceTropDirName(entry.name) &&
		    npTrophyParseCommDirName(entry.name, NULL)) {
			*outHasAny = true;
			break;
		}
	}

	return dir.Close();
}

/* 0x812D5C8C */
int TrophyUserIndexStorage::CreateFresh(TrophyHandle *handle, const char *path,
					const TrophyDualTick *commitTicks)
{
	bool hasExistingTitles = false;
	int ret = HasAnyInstalledTitleDir(handle, m_userId, &hasExistingTitles);
	if (ret >= 0) {
		uint64_t seq = 0;
		ret = npTrophyGenerateInitialSequence(&seq);
		if (ret >= 0) {
			TrophyAccountCache accountCache;
			TrophyUserIndexCreateParams params;
			params.initialSequence = seq;
			params.formatFlags = TROPHY_FORMAT_VERSION_V1;
			params.openPsId = m_openPsId;
			params.accountCache = &accountCache;
			ret = m_indexFile.Create(handle, path, kTrophyOpenFlagsCreate,
						 kTrophyFileMode, &params);
			if (ret >= 0 && hasExistingTitles)
				ret = m_indexFile.SetNeedsRecoveryFlag(handle);
			if (ret >= 0)
				ret = m_indexFile.CommitTransaction(handle, commitTicks);
			if (ret >= 0)
				return 0;
		}
	}

	if (m_indexFile.IsOpen())
		m_indexFile.Close();
	RemoveIndexFile();
	return ret;
}

/* 0x812D5D62 */
int TrophyUserIndexStorage::Open(TrophyHandle *handle, int accessMode,
				 const TrophyDualTick *commitTicks)
{
	int ret = m_dirGuard.AcquireUserRoot(m_arbiter, m_userId);
	if (ret < 0)
		return ret;

	char indexPath[256];
	ret = npTrophyJoinBasePath(indexPath, m_dirGuard.GetPath(), 1, "TRPUSER.DAT");
	if (ret >= 0) {
		bool exists = false;
		ret = npTrophyPathExists(indexPath, &exists);
		if (ret >= 0) {
			if (accessMode == 2) {
				if (exists) {
					TrophyAccountCache accountCache;
					ret = m_indexFile.Open(handle, indexPath,
							       kTrophyOpenFlagsReadWrite, 0,
							       &accountCache);
					if (ret < 0) {
						if (isStorageFileSystemError(ret) ||
						    ret == NP_TROPHY_ERR_RECORD_BAD_MAGIC ||
						    ret == NP_TROPHY_ERR_RECORD_SECTION_NOT_FOUND ||
						    ret == NP_TROPHY_ERR_RECORD_TRUNCATED ||
						    ret == NP_TROPHY_ERR_USER_DAT_MISSING) {
							RemoveIndexFile();
						}
						if (m_indexFile.IsOpen())
							m_indexFile.Close();
					}
				} else {
					ret = CreateFresh(handle, indexPath, commitTicks);
				}
			} else if (exists) {
				TrophyAccountCache accountCache;
				ret = m_indexFile.Open(handle, indexPath,
						       kTrophyOpenFlagsReadOnly, 0,
						       &accountCache);
				if (ret < 0) {
					if (isStorageFileSystemError(ret) ||
					    ret == NP_TROPHY_ERR_RECORD_BAD_MAGIC ||
					    ret == NP_TROPHY_ERR_RECORD_SECTION_NOT_FOUND ||
					    ret == NP_TROPHY_ERR_RECORD_TRUNCATED ||
					    ret == NP_TROPHY_ERR_USER_DAT_MISSING) {
						RemoveIndexFile();
					}
					if (m_indexFile.IsOpen())
						m_indexFile.Close();
				}
			}
		}
	}

	if (ret >= 0) {
		m_isOpen = 1;
		return 0;
	}
	if (m_dirGuard.IsAcquired())
		m_dirGuard.Release();
	return ret;
}

/* 0x812D5F2A */
int TrophyUserIndexStorage::Close()
{
	if (m_indexFile.IsOpen()) {
		int ret = m_indexFile.Close();
		if (ret < 0)
			return ret;
	}
	if (m_dirGuard.IsAcquired()) {
		int ret = m_dirGuard.Release();
		if (ret < 0)
			return ret;
	}
	m_isOpen = 0;
	return 0;
}

/* 0x812D5F68 */
int TrophyUserIndexStorage::ClearNeedsRecovery(TrophyHandle *handle)
{
	TrophyDualTick ticks;
	ticks.Clear();
	int ret = npTrophyGetDualTick(&ticks);
	if (ret < 0)
		return ret;

	ret = m_indexFile.BeginTransaction(handle, 10, 1024);
	if (ret < 0)
		return ret;

	TrophyAccountCache accountCache;
	bool bound = false;
	uint64_t accountId = 0;
	ret = m_indexFile.BindAccountIfUnbound(handle, &accountCache, &bound, &accountId);
	if (ret < 0)
		return ret;

	ret = m_indexFile.ClearNeedsRecoveryFlag(handle);
	if (ret < 0)
		return ret;

	return m_indexFile.CommitTransaction(handle, &ticks);
}

/* 0x812D5FFC */
int TrophyUserIndexStorage::GetNeedsRecovery(TrophyHandle *handle, bool *outNeedsRecovery)
{
	if (!m_indexFile.IsOpen()) {
		*outNeedsRecovery = false;
		return 0;
	}
	return m_indexFile.GetNeedsRecoveryFlag(handle, outNeedsRecovery);
}

/* 0x812D6028 */
int TrophyUserIndexStorage::HasTitle(TrophyHandle *handle, const TrophyCommId *commId,
				     bool *outExists, TrophyTitleRecord *outRecord)
{
	if (!m_indexFile.IsOpen()) {
		if (outRecord != NULL)
			outRecord->Clear();
		*outExists = false;
		return 0;
	}
	return m_indexFile.HasTitle(handle, commId, outExists, outRecord);
}

/* 0x812D606E */
int TrophyUserIndexStorage::RemoveTitle(TrophyHandle *handle, const TrophyCommId *commId)
{
	TrophyDualTick ticks;
	ticks.Clear();
	int ret = npTrophyGetDualTick(&ticks);
	if (ret < 0)
		return ret;

	ret = m_indexFile.BeginTransaction(handle, 7, 1024);
	if (ret < 0)
		return ret;

	TrophyAccountCache accountCache;
	bool bound = false;
	uint64_t accountId = 0;
	ret = m_indexFile.BindAccountIfUnbound(handle, &accountCache, &bound, &accountId);
	if (ret < 0)
		return ret;

	int removedSlot = -1;
	ret = m_indexFile.RemoveTitle(handle, commId, &removedSlot);
	if (ret < 0)
		return ret;

	return m_indexFile.CommitTransaction(handle, &ticks);
}

/* 0x812D6110 */
int TrophyUserIndexStorage::AddTitle(TrophyHandle *handle, TrophyTitleRecord *ioRecord)
{
	TrophyUserIndexSummary summary;
	sceClibMemset(&summary, 0, sizeof(summary));
	int ret = m_indexFile.ReadSummary(handle, &summary);
	if (ret < 0)
		return ret;
	if (summary.totalTitles >= TROPHY_MAX_INSTALLED_TITLES)
		return NP_TROPHY_ERR_MAX_TITLES_REACHED;

	TrophyDualTick ticks;
	ticks.Clear();
	ret = npTrophyGetDualTick(&ticks);
	if (ret < 0)
		return ret;

	ret = m_indexFile.BeginTransaction(handle, 6, 1024);
	if (ret < 0)
		return ret;

	TrophyAccountCache accountCache;
	bool bound = false;
	uint64_t accountId = 0;
	ret = m_indexFile.BindAccountIfUnbound(handle, &accountCache, &bound, &accountId);
	if (ret < 0)
		return ret;

	ret = m_indexFile.AddTitle(handle, ioRecord);
	if (ret < 0)
		return ret;

	return m_indexFile.CommitTransaction(handle, &ticks);
}

/* 0x812D61E6 */
int TrophyUserIndexStorage::GetTitle(TrophyHandle *handle, const TrophyCommId *commId,
				     TrophyTitleRecord *outRecord)
{
	if (!m_indexFile.IsOpen())
		return NP_TROPHY_ERR_TITLE_NOT_FOUND;
	int ret = m_indexFile.GetTitle(handle, commId, outRecord);
	if (ret < 0)
		return ret;
	return outRecord->IsBroken() ? NP_TROPHY_ERR_BROKEN_DATA : 0;
}

/* 0x812D6232 */
int TrophyUserIndexStorage::UpdateTitle(TrophyHandle *handle, const TrophyTitleRecord *record,
					bool markNeedsRecovery)
{
	TrophyDualTick ticks;
	ticks.Clear();
	int ret = npTrophyGetDualTick(&ticks);
	if (ret >= 0) {
		ret = m_indexFile.BeginTransaction(handle, 8, 1024);
		if (ret >= 0) {
			TrophyAccountCache accountCache;
			bool bound = false;
			uint64_t accountId = 0;
			ret = m_indexFile.BindAccountIfUnbound(handle, &accountCache, &bound,
							       &accountId);
		}
		if (ret >= 0)
			ret = m_indexFile.UpdateTitle(handle, record);
		if (ret >= 0 && markNeedsRecovery)
			ret = m_indexFile.SetNeedsRecoveryFlag(handle);
		if (ret >= 0)
			ret = m_indexFile.CommitTransaction(handle, &ticks);
	}

	if (ret < 0) {
		m_indexFile.SetNeedsRecoveryFlag(handle);
		return ret;
	}
	return 0;
}

/* 0x812D6306 */
int TrophyUserIndexStorage::FindOldestOrBrokenTitle(TrophyHandle *handle,
						    TrophyTitleRecord *outRecord)
{
	if (!m_indexFile.IsOpen()) {
		outRecord->Clear();
		return 0;
	}
	return m_indexFile.FindOldestOrBrokenTitle(handle, outRecord);
}

/* 0x812D6334 */
int TrophyUserIndexStorage::HasPendingUploads(TrophyHandle *handle, bool *outHasPending)
{
	if (!m_indexFile.IsOpen()) {
		*outHasPending = false;
		return 0;
	}
	TrophyUserIndexSummary summary;
	sceClibMemset(&summary, 0, sizeof(summary));
	int ret = m_indexFile.ReadSummary(handle, &summary);
	if (ret < 0)
		return ret;
	*outHasPending = (summary.pendingUploadTitles != 0);
	return 0;
}

/* 0x812D63A6 */
int TrophyUserIndexStorage::GetJournalStatus(TrophyHandle *handle, void *outStatus)
{
	(void)handle;
	struct JournalStatusLayout {
		uint32_t broken;
		uint32_t reservedPadding;
		uint64_t sequence;
	};

	JournalStatusLayout *dst = static_cast<JournalStatusLayout *>(outStatus);
	if (!m_indexFile.IsOpen()) {
		dst->broken = 0;
		dst->sequence = 0;
		return 0;
	}
	TrophyJournalState journal;
	int ret = m_indexFile.ReadJournal(&journal);
	if (ret < 0)
		return ret;
	dst->broken = journal.IsDirty() ? 1u : 0u;
	dst->sequence = journal.sequence;
	return 0;
}

/* 0x812D6412 */
int TrophyUserIndexStorage::GetSyncTicks(TrophyHandle *handle, void *outTicks)
{
	struct SyncTicksLayout {
		TrophyDualTick lastSyncAttempt;
		TrophyDualTick lastSyncSuccess;
	};

	SyncTicksLayout *dst = static_cast<SyncTicksLayout *>(outTicks);
	if (!m_indexFile.IsOpen()) {
		sceClibMemset(dst, 0, sizeof(*dst));
		return 0;
	}
	TrophyUserIndexSummary summary;
	sceClibMemset(&summary, 0, sizeof(summary));
	int ret = m_indexFile.ReadSummary(handle, &summary);
	if (ret < 0)
		return ret;
	dst->lastSyncAttempt = summary.lastSyncAttemptTick;
	dst->lastSyncSuccess = summary.lastSyncSuccessTick;
	return 0;
}

/* 0x812D64AA */
int TrophyUserIndexStorage::GetCounts(TrophyHandle *handle, void *outCounts)
{
	struct IndexCountsLayout {
		uint32_t totalTitles;
		uint32_t installedTitles;
		uint32_t pendingUploadTitles;
		uint32_t deferredUploadTitles;
		SceRtcTick lastAttemptTick;
		SceRtcTick lastSuccessTick;
	};

	IndexCountsLayout *dst = static_cast<IndexCountsLayout *>(outCounts);
	if (!m_indexFile.IsOpen()) {
		sceClibMemset(dst, 0, sizeof(*dst));
		return 0;
	}
	TrophyUserIndexSummary summary;
	sceClibMemset(&summary, 0, sizeof(summary));
	int ret = m_indexFile.ReadSummary(handle, &summary);
	if (ret < 0)
		return ret;
	dst->totalTitles = summary.totalTitles;
	dst->installedTitles = summary.installedTitles;
	dst->pendingUploadTitles = summary.pendingUploadTitles;
	dst->deferredUploadTitles = summary.deferredUploadTitles;
	dst->lastAttemptTick = summary.lastSyncAttemptTick.secureTick;
	dst->lastSuccessTick = summary.lastSyncSuccessTick.secureTick;
	return 0;
}

/* 0x812D6548 */
int TrophyUserIndexStorage::EnumerateTitle(TrophyHandle *handle, uint32_t startIndex,
					   uint32_t *outFoundIndex, void *outTitleInfo,
					   void *outSyncInfo)
{
	if (!m_indexFile.IsOpen()) {
		*outFoundIndex = 0xFFFFFFFFu;
		return 0;
	}
	TrophyTitleRecord record;
	record.Clear();
	int ret = m_indexFile.FindNextTitle(handle, startIndex, outFoundIndex, &record);
	if (ret < 0)
		return ret;
	if (outTitleInfo != NULL)
		record.ExportTitleInfo(outTitleInfo);
	if (outSyncInfo != NULL)
		record.ExportSyncInfo(outSyncInfo);
	return 0;
}

/* 0x812D6606 */
int TrophyUserIndexStorage::RecordSyncAttempt(TrophyHandle *handle, const SceRtcTick *tick,
					      int syncResult)
{
	TrophyDualTick commitTicks;
	commitTicks.Clear();
	int ret = npTrophyGetDualTick(&commitTicks);
	if (ret < 0)
		return ret;

	ret = m_indexFile.BeginTransaction(handle, 9, 1024);
	if (ret < 0)
		return ret;

	TrophyAccountCache accountCache;
	bool bound = false;
	uint64_t accountId = 0;
	ret = m_indexFile.BindAccountIfUnbound(handle, &accountCache, &bound, &accountId);
	if (ret < 0)
		return ret;

	const SceRtcTick *successTick = (syncResult == 0 || syncResult == 512) ? tick : NULL;
	ret = m_indexFile.UpdateSyncAttemptTicks(handle, tick, successTick);
	if (ret < 0)
		return ret;

	return m_indexFile.CommitTransaction(handle, &commitTicks);
}

/* 0x812D66C2 */
int TrophyUserIndexStorage::RecordSyncSuccess(TrophyHandle *handle, const SceRtcTick *tick,
					      int syncResult)
{
	TrophyDualTick commitTicks;
	commitTicks.Clear();
	int ret = npTrophyGetDualTick(&commitTicks);
	if (ret < 0)
		return ret;

	ret = m_indexFile.BeginTransaction(handle, 9, 1024);
	if (ret < 0)
		return ret;

	TrophyAccountCache accountCache;
	bool bound = false;
	uint64_t accountId = 0;
	ret = m_indexFile.BindAccountIfUnbound(handle, &accountCache, &bound, &accountId);
	if (ret < 0)
		return ret;

	const SceRtcTick *successTick = (syncResult == 0 || syncResult == 512) ? tick : NULL;
	ret = m_indexFile.UpdateSyncSuccessTicks(handle, tick, successTick);
	if (ret < 0)
		return ret;

	return m_indexFile.CommitTransaction(handle, &commitTicks);
}

/* =========================================================================
 * TrophyTitleStorage (0x812D00C0..0x812D3177)
 * ========================================================================= */

/* 0x812D0134 */
TrophyTitleStorage::TrophyTitleStorage(TrophyUserSession *session, const TrophyCommId *commId)
	: m_session(session),
	  m_titleRecord(NULL),
	  m_userStorage(NULL),
	  m_confStorage(NULL),
	  m_commId(*commId)
{
}

/* 0x812D0160 */
TrophyTitleStorage::~TrophyTitleStorage()
{
	DestroyComponents();
}

void TrophyTitleStorage::DestroyComponents()
{
	if (m_confStorage != NULL) {
		delete m_confStorage;
		m_confStorage = NULL;
	}
	if (m_userStorage != NULL) {
		delete m_userStorage;
		m_userStorage = NULL;
	}
	if (m_titleRecord != NULL) {
		TrophyHeapObject::operator delete(m_titleRecord);
		m_titleRecord = NULL;
	}
}

bool TrophyTitleStorage::IsDataCorruptionError(int err)
{
	return isStorageFileSystemError(err) || err == NP_TROPHY_ERR_RECORD_BAD_MAGIC ||
	       err == NP_TROPHY_ERR_RECORD_SECTION_NOT_FOUND ||
	       err == NP_TROPHY_ERR_RECORD_TRUNCATED ||
	       err == NP_TROPHY_ERR_DATA_MARKED_BROKEN ||
	       err == NP_TROPHY_ERR_PARAM_INI_MISSING ||
	       err == NP_TROPHY_ERR_TITLE_DAT_MISSING ||
	       err == NP_TROPHY_ERR_TRANS_DAT_MISSING;
}

int TrophyTitleStorage::CheckFreeSpace(uint64_t requiredUr0, uint64_t requiredUx0)
{
	uint64_t ur0Max = 0;
	uint64_t ur0Free = 0;
	int ret = npTrophyGetDeviceFreeSpace("ur0:", &ur0Max, &ur0Free);
	if (ret < 0)
		return ret;
	if (requiredUr0 + 4096ULL > ur0Free)
		return NP_TROPHY_ERR_INSUFFICIENT_UR0_SPACE;
	if (requiredUx0 != 0) {
		uint64_t ux0Max = 0;
		uint64_t ux0Free = 0;
		ret = npTrophyGetDeviceFreeSpace("ux0:", &ux0Max, &ux0Free);
		if (ret < 0)
			return ret;
		if (requiredUx0 > ux0Free)
			return NP_TROPHY_ERR_INSUFFICIENT_UX0_SPACE;
	}
	return 0;
}

/* 0x812D00C0 */
int TrophyTitleStorage::EnsureNotInstalledOrRemove(TrophyHandle *handle,
						   const TrophyCommId *commId)
{
	char dataDirPath[256];
	int userId = npTrophyUserSessionGetUserId(m_session);
	int ret = npTrophyUr0DataDirPath(dataDirPath, userId, commId, kActiveDirSuffix);
	if (ret < 0)
		return ret;

	bool exists = false;
	ret = npTrophyPathExists(dataDirPath, &exists);
	if (ret < 0)
		return ret;
	if (!exists)
		return 0;

	return npTrophyUserSessionPurgeTitle(m_session, handle, commId);
}

/* 0x812D01CA */
int TrophyTitleStorage::Close()
{
	if (m_titleRecord == NULL)
		return NP_TROPHY_ERR_STORAGE_NOT_OPEN;
	int ret = m_userStorage->Close();
	if (ret < 0)
		return ret;
	ret = m_confStorage->Close();
	if (ret < 0)
		return ret;
	DestroyComponents();
	m_commId.Clear();
	return 0;
}

/* 0x812D0228 */
int TrophyTitleStorage::PrepareComponents()
{
	if (m_titleRecord != NULL) {
		int ret = Close();
		if (ret < 0)
			return ret;
	}

	m_titleRecord = static_cast<TrophyTitleRecord *>(
		TrophyHeapObject::operator new(sizeof(TrophyTitleRecord)));
	if (m_titleRecord == NULL) {
		DestroyComponents();
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}
	m_titleRecord->Clear();

	m_userStorage = new TrophyUserStorage(npTrophyUserSessionGetArbiter(m_session),
					      npTrophyUserSessionGetOpenPsId(m_session),
					      npTrophyUserSessionGetUserId(m_session), &m_commId);
	if (m_userStorage == NULL) {
		DestroyComponents();
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}

	m_confStorage = new TrophyConfStorage(npTrophyUserSessionGetUserId(m_session), &m_commId);
	if (m_confStorage == NULL) {
		DestroyComponents();
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}
	return 0;
}

int TrophyTitleStorage::EnsureOpenAndSessionReady(TrophyHandle *handle)
{
	if (m_titleRecord == NULL)
		return NP_TROPHY_ERR_STORAGE_NOT_OPEN;
	if (m_titleRecord->IsBroken())
		return NP_TROPHY_ERR_BROKEN_DATA;
	return npTrophyUserSessionEnsureReady(m_session, handle, 2);
}

/* 0x812D02F0 */
int TrophyTitleStorage::Open(TrophyHandle *handle, uint32_t openFlags)
{
	int ret = npTrophyUserSessionEnsureReady(m_session, handle, 2);
	if (ret < 0)
		return ret;

	ret = PrepareComponents();
	if (ret < 0)
		return ret;

	TrophyUserIndexStorage *indexStorage = npTrophyUserSessionGetIndexStorage(m_session);
	ret = indexStorage->GetTitle(handle, &m_commId, m_titleRecord);
	if (ret == NP_TROPHY_ERR_TITLE_NOT_FOUND) {
		DestroyComponents();
		return NP_TROPHY_ERR_TITLE_NOT_REGISTERED;
	}
	if (ret < 0) {
		DestroyComponents();
		return ret;
	}

	if ((openFlags & 4u) != 0) {
		ret = m_confStorage->Open(handle);
		if (ret < 0) {
			if (ret == NP_TROPHY_ERR_CONF_MARKED_BROKEN ||
			    ret == NP_TROPHY_ERR_SFM_NOT_FOUND ||
			    ret == NP_TROPHY_ERR_PARAM_INI_CORRUPT ||
			    ret == NP_TROPHY_ERR_BROKEN_CONF) {
				m_titleRecord->MarkBroken(0);
				if (npTrophyUserSessionIsWritable(m_session))
					indexStorage->UpdateTitle(handle, m_titleRecord, false);
			}
			DestroyComponents();
			return ret;
		}
	}

	uint32_t userFlags = openFlags & 3u;
	ret = m_userStorage->Open(handle, userFlags, npTrophyUserSessionGetAccessMode(m_session));
	if (ret < 0) {
		if (IsDataCorruptionError(ret)) {
			m_titleRecord->MarkBroken(0);
			if (npTrophyUserSessionIsWritable(m_session))
				indexStorage->UpdateTitle(handle, m_titleRecord, false);
		}
		DestroyComponents();
		return ret;
	}

	ret = m_userStorage->VerifyTitleRecord(handle, m_titleRecord);
	if (ret < 0) {
		if (ret == NP_TROPHY_ERR_JOURNAL_MISMATCH &&
		    m_userStorage->ReadTitleRecord(handle, m_titleRecord) >= 0) {
			indexStorage->UpdateTitle(handle, m_titleRecord, true);
		}
		DestroyComponents();
		return ret;
	}
	return 0;
}

/* 0x812D04CA */
int TrophyTitleStorage::MarkBroken(TrophyHandle *handle, int brokenKind)
{
	int ret = npTrophyUserSessionEnsureReady(m_session, handle, 2);
	if (ret < 0)
		return ret;

	TrophyNoCancelGuard noCancel(handle);
	TrophyPowerLock powerLock;
	powerLock.Lock();

	if (brokenKind == 0) {
		ret = Open(handle, 7u);
		if (ret >= 0) {
			m_titleRecord->MarkBroken(0);
			npTrophyUserSessionGetIndexStorage(m_session)->UpdateTitle(
				handle, m_titleRecord, false);
			ret = Close();
		}
	} else if (brokenKind == 1) {
		ret = Open(handle, 7u);
		if (ret >= 0) {
			TrophyBrokenMarker marker;
			marker.reason = 0;
			marker.errorCode = NP_TROPHY_ERR_GENERIC;
			ret = m_userStorage->WriteBrokenMarker(handle, &marker);
			if (ret >= 0)
				ret = Close();
		}
	} else if (brokenKind == 2) {
		ret = Open(handle, 7u);
		if (ret >= 0) {
			TrophyBrokenMarker marker;
			marker.reason = 0;
			marker.errorCode = NP_TROPHY_ERR_GENERIC;
			ret = m_confStorage->WriteBrokenMarker(handle, &marker);
			if (ret >= 0)
				ret = Close();
		}
	}

	if (ret < 0 && m_titleRecord != NULL)
		DestroyComponents();
	return ret < 0 ? ret : 0;
}

/* 0x812D063E */
int TrophyTitleStorage::Uninstall(TrophyHandle *handle)
{
	int ret = npTrophyUserSessionEnsureReady(m_session, handle, 2);
	if (ret < 0)
		return ret;

	TrophyNoCancelGuard noCancel(handle);
	TrophyPowerLock powerLock;
	powerLock.Lock();

	int userId = npTrophyUserSessionGetUserId(m_session);
	ret = npTrophyUserSessionGetIndexStorage(m_session)->RemoveTitle(handle, &m_commId);
	if (ret >= 0)
		ret = TrophyUserStorage::RemoveDirectory(userId, &m_commId);
	if (ret >= 0)
		ret = TrophyConfStorage::RemoveDirectories(userId, &m_commId);
	if (ret >= 0)
		powerLock.Unlock();
	return ret < 0 ? ret : 0;
}

/* 0x812D06EE */
int TrophyTitleStorage::PublishUnlockedToSns(TrophyHandle *handle,
					     const NpTrophyFlagArray *unlockedMask)
{
	if (npTrophyBitsetCount(unlockedMask) == 0 || !npTrophyIsSnsFeedEnabled())
		return 0;

	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;

	TrophyConfData *confData = NULL;
	ret = m_confStorage->ReadConfData(handle, 0, &confData);
	if (ret < 0)
		return ret;

	TrophyConfTrophyDetails *trophyBuf = static_cast<TrophyConfTrophyDetails *>(
		TrophyHeapObject::operator new(sizeof(TrophyConfTrophyDetails) * 10));
	TrophySnsFeedItem *itemBuf = static_cast<TrophySnsFeedItem *>(
		TrophyHeapObject::operator new(sizeof(TrophySnsFeedItem) * 10));

	if (trophyBuf == NULL || itemBuf == NULL) {
		ret = NP_TROPHY_ERR_OUT_OF_MEMORY;
	} else {
		sceClibMemset(trophyBuf, 0, sizeof(TrophyConfTrophyDetails) * 10);
		sceClibMemset(itemBuf, 0, sizeof(TrophySnsFeedItem) * 10);

		TrophyConfTitleDetails titleDetails;
		ret = confData->ExtractTitleDetails(&titleDetails);
		int sysLang = 0;
		if (ret >= 0)
			ret = npTrophyGetSystemLanguage(&sysLang);
		char iconUrl[516];
		if (ret >= 0) {
			TrophyNetClient *netClient = npTrophyUserSessionGetNetClient(m_session);
			TrophyNetIconStream iconStream;
			ret = iconStream.OpenTitleIcon(handle, netClient->GetHttpTemplate(), NULL,
						       &m_commId, sysLang, 0, true, iconUrl);
			if (ret >= 0)
				iconStream.Close();
		}

		if (ret >= 0) {
			int batchCount = 0;
			for (int id = 0; id < 128; ++id) {
				if ((unlockedMask->flagBits[id >> 5] & (1u << (id & 31))) == 0)
					continue;
				TrophyConfTrophyDetails *entry = &trophyBuf[batchCount];
				sceClibMemset(entry, 0, sizeof(*entry));
				sceClibMemset(&itemBuf[batchCount], 0, sizeof(TrophySnsFeedItem));
				ret = confData->ExtractTrophyDetails((uint32_t)id, entry);
				if (ret < 0)
					break;
				if (entry->hidden != 0) {
					itemBuf[batchCount].hidden = 1;
					itemBuf[batchCount].grade = 0;
					itemBuf[batchCount].name = NULL;
					itemBuf[batchCount].detail = NULL;
				} else {
					itemBuf[batchCount].hidden = 0;
					itemBuf[batchCount].grade = entry->grade;
					itemBuf[batchCount].name = entry->name;
					itemBuf[batchCount].detail = entry->detail;
				}
				++batchCount;
				if (batchCount == 10) {
					ret = TrophySnsFeedPublisher::PublishBatch(
						handle, titleDetails.titleName, iconUrl, itemBuf,
						10);
					if (ret < 0)
						break;
					batchCount = 0;
				}
			}
			if (ret >= 0 && batchCount > 0) {
				ret = TrophySnsFeedPublisher::PublishBatch(
					handle, titleDetails.titleName, iconUrl, itemBuf,
					batchCount);
			}
		}
	}

	if (trophyBuf != NULL)
		TrophyHeapObject::operator delete(trophyBuf);
	if (itemBuf != NULL)
		TrophyHeapObject::operator delete(itemBuf);
	delete confData;
	return ret < 0 ? ret : 0;
}

/* 0x812D096E */
int TrophyTitleStorage::BuildInitialSyncSession(TrophyHandle *handle,
						TrophySyncSessionBase **outSession)
{
	*outSession = NULL;
	if (npTrophyServiceIsSyncInGameDisabled())
		return NP_TROPHY_ERR_GENERIC;

	int online = SceNpManager_A775A452();
	if (online <= 0)
		return online < 0 ? online : 0;
	if (!npTrophyNetIsOnline())
		return 0;

	char env[20];
	int ret = npTrophyGetNpEnvironment(env);
	if (ret < 0 || !npTrophyIsValidNpEnvironment(env))
		return ret < 0 ? ret : 0;

	TrophyNetSyncBatch *session = new TrophyNetSyncBatch();
	if (session == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	TrophyHttpTimeouts httpTimeouts = { 30000000u, 30000000u, 30000000u };
	session->SetTimeouts(&httpTimeouts);
	SceNpId npId;
	sceClibMemset(&npId, 0, sizeof(npId));
	TrophySyncTitleRequestBase *req = NULL;
	ret = session->AddTitle(&m_commId, 0, &req);
	if (ret >= 0)
		ret = TrophyTicketClient::FetchNpId(handle, &npId);
	if (ret >= 0)
		ret = session->ExecuteWithFilter(handle, npTrophyUserSessionGetNetClient(m_session),
						 &npId);
	if (ret < 0) {
		delete session;
		return ret;
	}
	*outSession = session;
	return 0;
}

/* 0x812D1428 */
int TrophyTitleStorage::BuildDownloadSyncSession(TrophyHandle *handle,
						 TrophySyncSessionBase **outSession)
{
	*outSession = NULL;
	char env[20];
	int ret = npTrophyGetNpEnvironment(env);
	if (ret < 0 || !npTrophyIsValidNpEnvironment(env))
		return ret < 0 ? ret : 0;

	TrophyNetSyncBatch *session = new TrophyNetSyncBatch();
	if (session == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	SceNpId npId;
	sceClibMemset(&npId, 0, sizeof(npId));
	TrophySyncTitleRequestBase *req = NULL;
	ret = session->AddTitle(&m_commId, 0, &req);
	if (ret >= 0)
		ret = TrophyTicketClient::FetchNpId(handle, &npId);
	if (ret >= 0)
		ret = session->ExecuteWithFilter(handle, npTrophyUserSessionGetNetClient(m_session),
						 &npId);
	if (ret < 0) {
		delete session;
		return ret;
	}
	*outSession = session;
	return 0;
}

/* 0x812D17DC */
int TrophyTitleStorage::BuildPeriodicSyncSession(TrophyHandle *handle, const TrophyDualTick *now,
						 TrophySyncSessionBase **outSession)
{
	*outSession = NULL;
	if (npTrophyServiceIsSyncInGameDisabled())
		return NP_TROPHY_ERR_GENERIC;

	int online = SceNpManager_A775A452();
	if (online <= 0)
		return online < 0 ? online : 0;
	if (!npTrophyNetIsOnline())
		return 0;

	char env[20];
	int ret = npTrophyGetNpEnvironment(env);
	if (ret < 0 || !npTrophyIsValidNpEnvironment(env))
		return ret < 0 ? ret : 0;

	TrophyHttpTimeouts httpTimeouts = { 30000000u, 30000000u, 30000000u };
	uint64_t intervalSec = 0;
	ret = npTrophyUserSessionGetNetClient(m_session)->GetSyncInterval(
		handle, &intervalSec, &httpTimeouts);
	if (ret < 0)
		return ret;

	bool shouldSync = true;
	if (intervalSec != 0) {
		TrophyTitleProgressView prog;
		ret = m_userStorage->GetTitleProgressView(handle, NULL, &prog);
		if (ret < 0)
			return ret;
		uint64_t lastSyncTick = prog.snapshot.lastSyncedTick.secureTick.tick;
		if (lastSyncTick != 0 && lastSyncTick + intervalSec * 1000000ULL > now->secureTick.tick)
			shouldSync = false;
	}
	if (!shouldSync)
		return 0;

	TrophyNetSyncBatch *session = new TrophyNetSyncBatch();
	if (session == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	SceNpId npId;
	sceClibMemset(&npId, 0, sizeof(npId));
	ret = m_userStorage->PrepareUploadSyncSession(handle, session);
	if (ret >= 0)
		ret = TrophyTicketClient::FetchNpId(handle, &npId);
	if (ret >= 0)
		ret = session->ExecuteWithFilter(handle, npTrophyUserSessionGetNetClient(m_session),
						 &npId);
	if (ret < 0) {
		delete session;
		return ret;
	}
	*outSession = session;
	return 0;
}

/* 0x812D272C */
int TrophyTitleStorage::BuildUploadSyncSession(TrophyHandle *handle,
					       TrophySyncSessionBase **outSession)
{
	*outSession = NULL;
	char env[20];
	int ret = npTrophyGetNpEnvironment(env);
	if (ret < 0 || !npTrophyIsValidNpEnvironment(env))
		return ret < 0 ? ret : 0;

	TrophyNetSyncBatch *session = new TrophyNetSyncBatch();
	if (session == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	SceNpId npId;
	sceClibMemset(&npId, 0, sizeof(npId));
	ret = m_userStorage->PrepareUploadSyncSession(handle, session);
	if (ret >= 0)
		ret = TrophyTicketClient::FetchNpId(handle, &npId);
	if (ret >= 0)
		ret = session->ExecuteWithFilter(handle, npTrophyUserSessionGetNetClient(m_session),
						 &npId);
	if (ret < 0) {
		delete session;
		return ret;
	}
	*outSession = session;
	return 0;
}

/* 0x812D0E66 */
int TrophyTitleStorage::BackupForUpgrade(TrophyHandle *handle, TrophyTitleBackupState *outBackup)
{
	TrophyNoCancelGuard noCancel(handle);
	TrophyPowerLock powerLock;
	powerLock.Lock();

	int userId = npTrophyUserSessionGetUserId(m_session);
	int ret = npTrophyUserSessionGetIndexStorage(m_session)->GetTitle(
		handle, &m_commId, &outBackup->savedRecord);
	if (ret < 0)
		return ret;
	outBackup->hasIndexRecord = 1;

	ret = TrophyUserStorage::BackupDirectory(userId, &m_commId, &outBackup->dataBackedUp);
	if (ret < 0)
		return ret;
	ret = TrophyConfStorage::BackupDirectories(userId, &m_commId, outBackup->confBackupFlags);
	if (ret < 0)
		return ret;

	powerLock.Unlock();
	return 0;
}

/* 0x812D0DC2 */
int TrophyTitleStorage::RestoreFromBackup(TrophyHandle *handle, TrophyTitleBackupState *backup)
{
	TrophyNoCancelGuard noCancel(handle);
	int userId = npTrophyUserSessionGetUserId(m_session);
	int ret = TrophyConfStorage::RestoreDirectories(userId, &m_commId,
							backup->confBackupFlags);
	if (ret < 0)
		return ret;
	ret = TrophyUserStorage::RestoreDirectory(userId, &m_commId, &backup->dataBackedUp);
	if (ret < 0)
		return ret;
	if (backup->hasIndexRecord != 0) {
		ret = npTrophyUserSessionGetIndexStorage(m_session)->UpdateTitle(
			handle, &backup->savedRecord, false);
		if (ret < 0)
			return ret;
		backup->hasIndexRecord = 0;
	}
	return 0;
}

/* 0x812D0A86 */
int TrophyTitleStorage::InstallFromContext(TrophyHandle *handle, TrophyContext *context,
					   TrophyConfSource *source, int lang, int *outPlatinumId)
{
	int ret = npTrophyUserSessionEnsureReady(m_session, handle, 2);
	if (ret < 0)
		return ret;

	const void *serverTag = NULL;
	ret = npTrophyContextGetServerTag(context, &serverTag);
	if (ret < 0)
		return ret;

	int userId = npTrophyUserSessionGetUserId(m_session);
	uint32_t assetMask = 0;
	ret = TrophyConfStorage::DetermineAssetMask(handle, userId, &assetMask);
	if (ret < 0)
		return ret;

	uint64_t requiredUr0 = 0;
	uint64_t requiredUx0 = 0;
	ret = TrophyConfStorage::EstimateInstallSpace(handle, source, userId, &m_commId, lang,
						      assetMask, &requiredUr0, &requiredUx0);
	if (ret >= 0)
		ret = CheckFreeSpace(requiredUr0, requiredUx0);
	if (ret < 0)
		return ret;

	TrophyDualTick nowTicks;
	nowTicks.Clear();
	uint8_t isTrial = 0;
	TrophySyncSessionBase *syncSession = NULL;
	TrophyConfData *defaultConf = NULL;

	ret = npTrophyGetDualTick(&nowTicks);
	if (ret >= 0)
		ret = npTrophyContextGetTrialFlag(context, &isTrial);
	if (ret >= 0) {
		BuildInitialSyncSession(handle, &syncSession);
		ret = Uninstall(handle);
	}
	if (ret >= 0)
		ret = PrepareComponents();

	if (ret >= 0) {
		TrophyConfMeta confMeta;
		confMeta.Clear();
		ret = m_confStorage->InstallFromSource(handle, source, lang, assetMask, &confMeta,
						       &defaultConf, NULL);
		if (ret >= 0) {
			TrophyUserStorageInstallArgs installArgs;
			installArgs.ticks = &nowTicks;
			installArgs.confMeta = &confMeta;
			installArgs.titleFlags = 529u;
			installArgs.confData = defaultConf;
			installArgs.serverTag = serverTag;
			installArgs.deferredFlag = isTrial;
			ret = m_userStorage->Create(handle, &installArgs, 3u);
		}
		if (defaultConf != NULL) {
			delete defaultConf;
			defaultConf = NULL;
		}
		if (ret >= 0) {
			uint32_t syncFlags = 0;
			NpTrophyFlagArray syncedTrophies;
			sceClibMemset(&syncedTrophies, 0, sizeof(syncedTrophies));
			ret = m_userStorage->ApplyRegisterAndSync(
				handle, &nowTicks, npTrophyContextGetTitleId(context),
				isTrial != 0, syncSession, &syncFlags, &syncedTrophies,
				outPlatinumId);
			if (ret >= 0) {
				{
					TrophyNoCancelGuard noCancel(handle);
					ret = m_userStorage->ReadTitleRecord(handle,
									     m_titleRecord);
					if (ret >= 0)
						ret = m_userStorage->CommitTransactions(
							handle, &nowTicks, m_titleRecord);
					if (ret >= 0)
						ret = npTrophyUserSessionGetIndexStorage(m_session)
							      ->AddTitle(handle, m_titleRecord);
				}
				if (ret >= 0) {
					PublishUnlockedToSns(handle, &syncedTrophies);
					if (syncSession != NULL)
						delete syncSession;
					return 0;
				}
			}
		}
	}

	DestroyComponents();
	Uninstall(handle);
	if (syncSession != NULL)
		delete syncSession;
	if (defaultConf != NULL)
		delete defaultConf;
	return ret;
}

/* 0x812D0F28 */
int TrophyTitleStorage::UpgradeFromContext(TrophyHandle *handle, TrophyContext *context,
					   TrophyConfSource *source, int lang, int *outPlatinumId)
{
	int ret = npTrophyUserSessionEnsureReady(m_session, handle, 2);
	if (ret < 0)
		return ret;

	const void *serverTag = NULL;
	ret = npTrophyContextGetServerTag(context, &serverTag);
	if (ret < 0)
		return ret;

	int userId = npTrophyUserSessionGetUserId(m_session);
	uint32_t assetMask = 0;
	ret = TrophyConfStorage::DetermineAssetMask(handle, userId, &assetMask);
	if (ret < 0)
		return ret;

	uint64_t requiredUr0 = 0;
	uint64_t requiredUx0 = 0;
	ret = TrophyConfStorage::EstimateInstallSpace(handle, source, userId, &m_commId, lang,
						      assetMask, &requiredUr0, &requiredUx0);
	if (ret >= 0)
		ret = CheckFreeSpace(requiredUr0, requiredUx0);
	if (ret < 0)
		return ret;

	TrophyUserStorage oldUserStorage(npTrophyUserSessionGetArbiter(m_session),
					 npTrophyUserSessionGetOpenPsId(m_session), userId,
					 &m_commId);
	TrophyTitleBackupState backup;
	sceClibMemset(&backup, 0, sizeof(backup));
	backup.savedRecord.Clear();
	TrophyPowerLock powerLock;

	TrophyDualTick nowTicks;
	nowTicks.Clear();
	uint8_t isTrial = 0;
	TrophyConfData *defaultConf = NULL;

	ret = npTrophyGetDualTick(&nowTicks);
	if (ret >= 0)
		ret = npTrophyContextGetTrialFlag(context, &isTrial);
	if (ret >= 0) {
		powerLock.Lock();
		ret = BackupForUpgrade(handle, &backup);
	}
	if (ret >= 0)
		ret = PrepareComponents();
	if (ret >= 0)
		ret = oldUserStorage.Open(handle, 0x10000003u, 1);
	if (ret >= 0)
		ret = TrophyUserStorage::RemoveDirectory(userId, &m_commId);
	if (ret >= 0)
		ret = TrophyConfStorage::RemoveDirectories(userId, &m_commId);
	if (ret >= 0) {
		ret = npTrophyUserSessionGetIndexStorage(m_session)->GetTitle(handle, &m_commId,
									      m_titleRecord);
	}
	if (ret >= 0) {
		TrophyConfMeta confMeta;
		confMeta.Clear();
		ret = m_confStorage->InstallFromSource(handle, source, lang, assetMask, &confMeta,
						       &defaultConf, NULL);
		if (ret >= 0) {
			TrophyUserStorageInstallArgs installArgs;
			installArgs.ticks = &nowTicks;
			installArgs.confMeta = &confMeta;
			installArgs.titleFlags = 529u;
			installArgs.confData = defaultConf;
			installArgs.serverTag = serverTag;
			installArgs.deferredFlag = isTrial;
			ret = m_userStorage->CreateMigrated(handle, &installArgs, 3u,
							    &oldUserStorage);
		}
		if (defaultConf != NULL) {
			delete defaultConf;
			defaultConf = NULL;
		}
		if (ret >= 0) {
			uint32_t syncFlags = 0;
			NpTrophyFlagArray syncedTrophies;
			sceClibMemset(&syncedTrophies, 0, sizeof(syncedTrophies));
			ret = m_userStorage->ApplyRegisterAndSync(
				handle, &nowTicks, npTrophyContextGetTitleId(context),
				isTrial != 0, NULL, &syncFlags, &syncedTrophies, outPlatinumId);
			if (ret >= 0) {
				{
					TrophyNoCancelGuard noCancel(handle);
					ret = m_userStorage->CommitTransactions(handle, &nowTicks,
										m_titleRecord);
					if (ret >= 0) {
						ret = npTrophyUserSessionGetIndexStorage(m_session)
							      ->UpdateTitle(handle, m_titleRecord,
									    false);
					}
				}
				if (ret >= 0) {
					powerLock.Unlock();
					PublishUnlockedToSns(handle, &syncedTrophies);
					return 0;
				}
			}
		}
	}

	DestroyComponents();
	if (oldUserStorage.IsOpen())
		oldUserStorage.Close();
	if (IsDataCorruptionError(ret) && backup.hasIndexRecord != 0)
		backup.savedRecord.MarkBroken(0);
	RestoreFromBackup(handle, &backup);
	if (defaultConf != NULL)
		delete defaultConf;
	return ret;
}

/* 0x812D14FC */
int TrophyTitleStorage::InstallFromNetwork(TrophyHandle *handle, TrophyConfSource *source,
					   int lang)
{
	int ret = npTrophyUserSessionEnsureReady(m_session, handle, 2);
	if (ret < 0)
		return ret;

	int userId = npTrophyUserSessionGetUserId(m_session);
	uint32_t assetMask = 0;
	ret = TrophyConfStorage::DetermineAssetMask(handle, userId, &assetMask);
	if (ret < 0)
		return ret;

	uint64_t requiredUr0 = 0;
	uint64_t requiredUx0 = 0;
	ret = TrophyConfStorage::EstimateInstallSpace(handle, source, userId, &m_commId, lang,
						      assetMask, &requiredUr0, &requiredUx0);
	if (ret >= 0)
		ret = CheckFreeSpace(requiredUr0, requiredUx0);
	if (ret < 0)
		return ret;

	TrophyDualTick nowTicks;
	nowTicks.Clear();
	TrophySyncSessionBase *syncSession = NULL;
	TrophyConfData *defaultConf = NULL;

	ret = npTrophyGetDualTick(&nowTicks);
	if (ret >= 0)
		ret = BuildDownloadSyncSession(handle, &syncSession);
	if (ret >= 0)
		ret = Uninstall(handle);
	if (ret >= 0)
		ret = PrepareComponents();

	if (ret >= 0) {
		TrophyConfMeta confMeta;
		confMeta.Clear();
		ret = m_confStorage->InstallFromSource(handle, source, lang, assetMask, &confMeta,
						       &defaultConf, NULL);
		if (ret >= 0) {
			TrophyUserStorageInstallArgs installArgs;
			installArgs.ticks = &nowTicks;
			installArgs.confMeta = &confMeta;
			installArgs.titleFlags = 529u;
			installArgs.confData = defaultConf;
			installArgs.serverTag = NULL;
			installArgs.deferredFlag = 0;
			ret = m_userStorage->Create(handle, &installArgs, 1u);
		}
		if (defaultConf != NULL) {
			delete defaultConf;
			defaultConf = NULL;
		}
		if (ret >= 0) {
			uint32_t syncFlags = 0;
			int unlockedPlatId = -1;
			NpTrophyFlagArray syncedTrophies;
			sceClibMemset(&syncedTrophies, 0, sizeof(syncedTrophies));
			ret = m_userStorage->ApplySyncSession(handle, &nowTicks, syncSession,
							      &syncFlags, &syncedTrophies,
							      &unlockedPlatId);
			if (ret >= 0) {
				{
					TrophyNoCancelGuard noCancel(handle);
					ret = m_userStorage->ReadTitleRecord(handle,
									     m_titleRecord);
					if (ret >= 0)
						ret = m_userStorage->CommitTransactions(
							handle, &nowTicks, m_titleRecord);
					if (ret >= 0)
						ret = npTrophyUserSessionGetIndexStorage(m_session)
							      ->AddTitle(handle, m_titleRecord);
				}
				if (ret >= 0) {
					PublishUnlockedToSns(handle, &syncedTrophies);
					if (syncSession != NULL)
						delete syncSession;
					return 0;
				}
			}
		}
	}

	DestroyComponents();
	Uninstall(handle);
	if (syncSession != NULL)
		delete syncSession;
	if (defaultConf != NULL)
		delete defaultConf;
	return ret;
}

/* 0x812D19AC */
int TrophyTitleStorage::RegisterExistingContext(TrophyHandle *handle, TrophyContext *context,
						TrophyConfSource *source, int lang,
						int *outPlatinumId)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;

	int userId = npTrophyUserSessionGetUserId(m_session);
	bool needsUx0Refresh = false;
	ret = m_confStorage->NeedsUx0Refresh(handle, userId, &m_commId, &needsUx0Refresh);
	if (ret < 0)
		return ret;

	uint32_t ux0Mask = 0;
	if (needsUx0Refresh) {
		ux0Mask = 16u;
		uint64_t requiredUr0 = 0;
		uint64_t requiredUx0 = 0;
		ret = TrophyConfStorage::EstimateInstallSpace(handle, source, userId, &m_commId,
							      lang, ux0Mask, &requiredUr0,
							      &requiredUx0);
		if (ret >= 0)
			ret = CheckFreeSpace(requiredUr0, requiredUx0);
		if (ret < 0)
			return ret;
	}

	TrophyDualTick nowTicks;
	nowTicks.Clear();
	uint8_t isTrial = 0;
	TrophySyncSessionBase *syncSession = NULL;

	ret = npTrophyGetDualTick(&nowTicks);
	if (ret >= 0)
		ret = npTrophyContextGetTrialFlag(context, &isTrial);
	if (ret >= 0) {
		BuildPeriodicSyncSession(handle, &nowTicks, &syncSession);
		NpTrophyFlagArray syncedTrophies;
		sceClibMemset(&syncedTrophies, 0, sizeof(syncedTrophies));

		TrophyPowerLock powerLock;
		powerLock.Lock();

		if (needsUx0Refresh) {
			ret = m_confStorage->Close();
			if (ret >= 0) {
				TrophyConfMeta meta;
				meta.Clear();
				ret = m_confStorage->InstallFromSource(handle, source, lang,
								       ux0Mask, &meta, NULL, NULL);
			}
		}
		if (ret >= 0)
			ret = m_userStorage->BeginTransactions(handle, 3, 3u);
		if (ret >= 0) {
			uint32_t syncFlags = 0;
			ret = m_userStorage->ApplyRegisterAndSync(
				handle, &nowTicks, npTrophyContextGetTitleId(context),
				isTrial != 0, syncSession, &syncFlags, &syncedTrophies,
				outPlatinumId);
		}
		if (ret >= 0) {
			TrophyNoCancelGuard noCancel(handle);
			ret = m_userStorage->CommitTransactions(handle, &nowTicks, m_titleRecord);
			if (ret >= 0)
				ret = npTrophyUserSessionGetIndexStorage(m_session)->UpdateTitle(
					handle, m_titleRecord, false);
		}
		if (ret >= 0) {
			powerLock.Unlock();
			PublishUnlockedToSns(handle, &syncedTrophies);
		}
	}

	if (syncSession != NULL)
		delete syncSession;
	return ret < 0 ? ret : 0;
}

/* 0x812D1CD0 */
int TrophyTitleStorage::BuildUnlockParams(TrophyContext *context, bool forceDebug,
					  TrophyUnlockActionParams *outParams,
					  bool *outDisallowUnlock)
{
	TrophyDualTick ticks;
	ticks.Clear();
	int ret = npTrophyGetDualTick(&ticks);
	if (ret < 0)
		return ret;

	uint8_t isTrial = 0;
	uint8_t unlockAllowed = 1;
	uint8_t offlineFlag = 0;
	if (!forceDebug) {
		ret = npTrophyContextGetTrialFlag(context, &isTrial);
		if (ret < 0)
			return ret;
		ret = npTrophyContextGetUnlockAllowed(context, &unlockAllowed);
		if (ret < 0)
			return ret;
		ret = npTrophyContextGetOfflineFlag(context, &offlineFlag);
		if (ret < 0)
			return ret;
	}

	outParams->ticks = ticks;
	outParams->offlineFlag = (forceDebug || offlineFlag != 0) ? 1 : 0;
	outParams->deferredFlag = isTrial;
	*outDisallowUnlock = (unlockAllowed == 0 && outParams->offlineFlag == 0);
	return 0;
}

/* 0x812D1D98 */
int TrophyTitleStorage::UnlockSingleInternal(TrophyHandle *handle, TrophyContext *context,
					     int trophyId, bool forceDebug, int *outPlatinumId)
{
	if (m_titleRecord->IsBroken())
		return NP_TROPHY_ERR_BROKEN_DATA;

	TrophyUnlockActionParams params;
	sceClibMemset(&params, 0, sizeof(params));
	bool disallow = false;
	int ret = BuildUnlockParams(context, forceDebug, &params, &disallow);
	if (ret < 0)
		return ret;

	TrophyPowerLock powerLock;
	ret = m_userStorage->UnlockTrophy(handle, trophyId, &params, 1u, outPlatinumId);
	if (ret < 0)
		return ret;
	if (disallow)
		return NP_TROPHY_ERR_UNLOCK_NOT_ALLOWED;

	powerLock.Lock();
	ret = m_userStorage->BeginTransactions(handle, 4, 3u);
	if (ret < 0)
		return ret;
	ret = m_userStorage->UnlockTrophy(handle, trophyId, &params, 0u, outPlatinumId);
	if (ret < 0)
		return ret;

	{
		TrophyNoCancelGuard noCancel(handle);
		ret = m_userStorage->CommitTransactions(handle, &params.ticks, m_titleRecord);
		if (ret >= 0)
			ret = npTrophyUserSessionGetIndexStorage(m_session)->UpdateTitle(
				handle, m_titleRecord, false);
	}
	if (ret < 0)
		return ret;

	powerLock.Unlock();
	return 0;
}

/* 0x812D1F12 */
int TrophyTitleStorage::UnlockTrophy(TrophyHandle *handle, TrophyContext *context, int trophyId,
				     int *outPlatinumId)
{
	if (m_titleRecord == NULL)
		return NP_TROPHY_ERR_STORAGE_NOT_OPEN;
	int ret = npTrophyUserSessionEnsureReady(m_session, handle, 2);
	if (ret < 0)
		return ret;
	return UnlockSingleInternal(handle, context, trophyId, false, outPlatinumId);
}

/* 0x812D1F6C */
int TrophyTitleStorage::UnlockGroupInternal(TrophyHandle *handle, TrophyContext *context,
					    int groupId, bool forceDebug, int *outPlatinumId)
{
	if (m_titleRecord->IsBroken())
		return NP_TROPHY_ERR_BROKEN_DATA;

	TrophyUnlockActionParams params;
	sceClibMemset(&params, 0, sizeof(params));
	bool disallow = false;
	int ret = BuildUnlockParams(context, forceDebug, &params, &disallow);
	if (ret < 0)
		return ret;

	TrophyPowerLock powerLock;
	ret = m_userStorage->UnlockGroupTrophies(handle, groupId, &params, 1u, outPlatinumId);
	if (ret < 0)
		return ret;
	if (disallow)
		return NP_TROPHY_ERR_UNLOCK_NOT_ALLOWED;

	powerLock.Lock();
	ret = m_userStorage->BeginTransactions(handle, 4, 3u);
	if (ret < 0)
		return ret;
	ret = m_userStorage->UnlockGroupTrophies(handle, groupId, &params, 0u, outPlatinumId);
	if (ret < 0)
		return ret;

	{
		TrophyNoCancelGuard noCancel(handle);
		ret = m_userStorage->CommitTransactions(handle, &params.ticks, m_titleRecord);
		if (ret >= 0)
			ret = npTrophyUserSessionGetIndexStorage(m_session)->UpdateTitle(
				handle, m_titleRecord, false);
	}
	if (ret < 0)
		return ret;

	powerLock.Unlock();
	return 0;
}

/* 0x812D20E6 */
int TrophyTitleStorage::UnlockAllInternal(TrophyHandle *handle, TrophyContext *context,
					  bool forceDebug, int *outPlatinumId)
{
	if (m_titleRecord->IsBroken())
		return NP_TROPHY_ERR_BROKEN_DATA;

	TrophyUnlockActionParams params;
	sceClibMemset(&params, 0, sizeof(params));
	bool disallow = false;
	int ret = BuildUnlockParams(context, forceDebug, &params, &disallow);
	if (ret < 0)
		return ret;

	TrophyPowerLock powerLock;
	ret = m_userStorage->UnlockAllTrophies(handle, &params, 1u, outPlatinumId);
	if (ret < 0)
		return ret;
	if (disallow)
		return NP_TROPHY_ERR_UNLOCK_NOT_ALLOWED;

	powerLock.Lock();
	ret = m_userStorage->BeginTransactions(handle, 4, 3u);
	if (ret < 0)
		return ret;
	ret = m_userStorage->UnlockAllTrophies(handle, &params, 0u, outPlatinumId);
	if (ret < 0)
		return ret;

	{
		TrophyNoCancelGuard noCancel(handle);
		ret = m_userStorage->CommitTransactions(handle, &params.ticks, m_titleRecord);
		if (ret >= 0)
			ret = npTrophyUserSessionGetIndexStorage(m_session)->UpdateTitle(
				handle, m_titleRecord, false);
	}
	if (ret < 0)
		return ret;

	powerLock.Unlock();
	return 0;
}

/* 0x812D224E */
int TrophyTitleStorage::DebugUnlock(TrophyHandle *handle, const TrophyDebugTrophyTarget *target,
				    int *outPlatinumId)
{
	if (m_titleRecord == NULL)
		return NP_TROPHY_ERR_STORAGE_NOT_OPEN;
	int ret = npTrophyUserSessionEnsureReady(m_session, handle, 2);
	if (ret < 0)
		return ret;
	if (target->scope == 1)
		return UnlockAllInternal(handle, NULL, true, outPlatinumId);
	if (target->scope == 2)
		return UnlockGroupInternal(handle, NULL, target->targetId, true, outPlatinumId);
	if (target->scope == 3)
		return UnlockSingleInternal(handle, NULL, target->targetId, true, outPlatinumId);
	return NP_TROPHY_ERR_INVALID_ARGUMENT;
}

/* 0x812D22F6 */
int TrophyTitleStorage::LockSingleInternal(TrophyHandle *handle, int trophyId, int *outPlatinumId)
{
	if (m_titleRecord->IsBroken())
		return NP_TROPHY_ERR_BROKEN_DATA;

	TrophyDualTick ticks;
	ticks.Clear();
	int ret = npTrophyGetDualTick(&ticks);
	if (ret < 0)
		return ret;

	TrophyPowerLock powerLock;
	ret = m_userStorage->LockTrophy(handle, trophyId, 1u, outPlatinumId);
	if (ret < 0)
		return ret;

	powerLock.Lock();
	ret = m_userStorage->BeginTransactions(handle, 4, 3u);
	if (ret < 0)
		return ret;
	ret = m_userStorage->LockTrophy(handle, trophyId, 0u, outPlatinumId);
	if (ret < 0)
		return ret;

	{
		TrophyNoCancelGuard noCancel(handle);
		ret = m_userStorage->CommitTransactions(handle, &ticks, m_titleRecord);
		if (ret >= 0)
			ret = npTrophyUserSessionGetIndexStorage(m_session)->UpdateTitle(
				handle, m_titleRecord, false);
	}
	if (ret < 0)
		return ret;

	powerLock.Unlock();
	return 0;
}

/* 0x812D2432 */
int TrophyTitleStorage::LockGroupInternal(TrophyHandle *handle, int groupId, int *outPlatinumId)
{
	if (m_titleRecord->IsBroken())
		return NP_TROPHY_ERR_BROKEN_DATA;

	TrophyDualTick ticks;
	ticks.Clear();
	int ret = npTrophyGetDualTick(&ticks);
	if (ret < 0)
		return ret;

	TrophyPowerLock powerLock;
	ret = m_userStorage->LockGroupTrophies(handle, groupId, 1u, outPlatinumId);
	if (ret < 0)
		return ret;

	powerLock.Lock();
	ret = m_userStorage->BeginTransactions(handle, 4, 3u);
	if (ret < 0)
		return ret;
	ret = m_userStorage->LockGroupTrophies(handle, groupId, 0u, outPlatinumId);
	if (ret < 0)
		return ret;

	{
		TrophyNoCancelGuard noCancel(handle);
		ret = m_userStorage->CommitTransactions(handle, &ticks, m_titleRecord);
		if (ret >= 0)
			ret = npTrophyUserSessionGetIndexStorage(m_session)->UpdateTitle(
				handle, m_titleRecord, false);
	}
	if (ret < 0)
		return ret;

	powerLock.Unlock();
	return 0;
}

/* 0x812D256E */
int TrophyTitleStorage::LockAllInternal(TrophyHandle *handle, int *outPlatinumId)
{
	if (m_titleRecord->IsBroken())
		return NP_TROPHY_ERR_BROKEN_DATA;

	TrophyDualTick ticks;
	ticks.Clear();
	int ret = npTrophyGetDualTick(&ticks);
	if (ret < 0)
		return ret;

	TrophyPowerLock powerLock;
	ret = m_userStorage->LockAllTrophies(handle, 1u, outPlatinumId);
	if (ret < 0)
		return ret;

	powerLock.Lock();
	ret = m_userStorage->BeginTransactions(handle, 4, 3u);
	if (ret < 0)
		return ret;
	ret = m_userStorage->LockAllTrophies(handle, 0u, outPlatinumId);
	if (ret < 0)
		return ret;

	{
		TrophyNoCancelGuard noCancel(handle);
		ret = m_userStorage->CommitTransactions(handle, &ticks, m_titleRecord);
		if (ret >= 0)
			ret = npTrophyUserSessionGetIndexStorage(m_session)->UpdateTitle(
				handle, m_titleRecord, false);
	}
	if (ret < 0)
		return ret;

	powerLock.Unlock();
	return 0;
}

/* 0x812D269E */
int TrophyTitleStorage::DebugLock(TrophyHandle *handle, const TrophyDebugTrophyTarget *target,
				  int *outPlatinumId)
{
	if (m_titleRecord == NULL)
		return NP_TROPHY_ERR_STORAGE_NOT_OPEN;
	int ret = npTrophyUserSessionEnsureReady(m_session, handle, 2);
	if (ret < 0)
		return ret;
	if (target->scope == 1)
		return LockAllInternal(handle, outPlatinumId);
	if (target->scope == 2)
		return LockGroupInternal(handle, target->targetId, outPlatinumId);
	if (target->scope == 3)
		return LockSingleInternal(handle, target->targetId, outPlatinumId);
	return NP_TROPHY_ERR_INVALID_ARGUMENT;
}

/* 0x812D2818 */
int TrophyTitleStorage::SyncUpload(TrophyHandle *handle, uint32_t *outSyncFlags,
				   int *outPlatinumId)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;

	handle->AdvanceProgress(1);
	TrophyDualTick ticks;
	ticks.Clear();
	ret = npTrophyGetDualTick(&ticks);
	if (ret < 0)
		return ret;

	TrophySyncSessionBase *syncSession = NULL;
	ret = BuildUploadSyncSession(handle, &syncSession);
	if (ret >= 0) {
		handle->AdvanceProgress(1);
		NpTrophyFlagArray syncedTrophies;
		sceClibMemset(&syncedTrophies, 0, sizeof(syncedTrophies));

		TrophyPowerLock powerLock;
		powerLock.Lock();
		ret = m_userStorage->BeginTransactions(handle, 9, 3u);
		if (ret >= 0) {
			handle->AdvanceProgress(1);
			ret = m_userStorage->ApplySyncSession(handle, &ticks, syncSession,
							      outSyncFlags, &syncedTrophies,
							      outPlatinumId);
		}
		if (ret >= 0) {
			handle->AdvanceProgress(1);
			TrophyNoCancelGuard noCancel(handle);
			ret = m_userStorage->CommitTransactions(handle, &ticks, m_titleRecord);
			if (ret >= 0)
				ret = npTrophyUserSessionGetIndexStorage(m_session)->UpdateTitle(
					handle, m_titleRecord, false);
		}
		if (ret >= 0) {
			powerLock.Unlock();
			PublishUnlockedToSns(handle, &syncedTrophies);
			handle->AdvanceProgress(1);
		}
	}

	if (syncSession != NULL)
		delete syncSession;
	return ret < 0 ? ret : 0;
}

/* 0x812D29BE */
int TrophyTitleStorage::SyncDownload(TrophyHandle *handle, uint32_t *outSyncFlags,
				     int *outPlatinumId)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;

	handle->AdvanceProgress(1);
	TrophyDualTick ticks;
	ticks.Clear();
	ret = npTrophyGetDualTick(&ticks);
	if (ret < 0)
		return ret;

	TrophySyncSessionBase *syncSession = NULL;
	ret = BuildDownloadSyncSession(handle, &syncSession);
	if (ret >= 0) {
		handle->AdvanceProgress(1);
		NpTrophyFlagArray syncedTrophies;
		sceClibMemset(&syncedTrophies, 0, sizeof(syncedTrophies));

		TrophyPowerLock powerLock;
		powerLock.Lock();
		ret = m_userStorage->BeginTransactions(handle, 9, 3u);
		if (ret >= 0) {
			handle->AdvanceProgress(1);
			ret = m_userStorage->ApplySyncSession(handle, &ticks, syncSession,
							      outSyncFlags, &syncedTrophies,
							      outPlatinumId);
		}
		if (ret >= 0) {
			handle->AdvanceProgress(1);
			TrophyNoCancelGuard noCancel(handle);
			ret = m_userStorage->CommitTransactions(handle, &ticks, m_titleRecord);
			if (ret >= 0)
				ret = npTrophyUserSessionGetIndexStorage(m_session)->UpdateTitle(
					handle, m_titleRecord, false);
		}
		if (ret >= 0) {
			powerLock.Unlock();
			PublishUnlockedToSns(handle, &syncedTrophies);
			handle->AdvanceProgress(1);
		}
	}

	if (syncSession != NULL)
		delete syncSession;
	return ret < 0 ? ret : 0;
}

/* 0x812D2B64 */
int TrophyTitleStorage::GetGroupMask(TrophyHandle *handle, uint32_t *outGroupMask)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;
	return m_userStorage->GetGroupMask(handle, outGroupMask);
}

/* 0x812D2BBA */
int TrophyTitleStorage::GetUnlockedAndAllMasks(TrophyHandle *handle,
					       NpTrophyFlagArray *outUnlocked,
					       NpTrophyFlagArray *outAll)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;
	return m_userStorage->GetUnlockedAndAllMasks(handle, outUnlocked, outAll);
}

/* 0x812D2C18 */
int TrophyTitleStorage::GetTitleInfo(TrophyHandle *handle, void *outTitleInfo)
{
	if (m_titleRecord == NULL)
		return NP_TROPHY_ERR_STORAGE_NOT_OPEN;
	int ret = npTrophyUserSessionEnsureReady(m_session, handle, 2);
	if (ret < 0)
		return ret;
	m_titleRecord->ExportTitleInfo(outTitleInfo);
	return 0;
}

/* 0x812D2C4E */
int TrophyTitleStorage::GetSyncInfo(TrophyHandle *handle, void *outSyncInfo)
{
	if (m_titleRecord == NULL)
		return NP_TROPHY_ERR_STORAGE_NOT_OPEN;
	int ret = npTrophyUserSessionEnsureReady(m_session, handle, 2);
	if (ret < 0)
		return ret;
	m_titleRecord->ExportSyncInfo(outSyncInfo);
	return 0;
}

/* 0x812D2C84 */
int TrophyTitleStorage::GetTitleProgressView(TrophyHandle *handle, void *outStaticInfo,
					     TrophyTitleProgressView *outProgress)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;
	return m_userStorage->GetTitleProgressView(handle, outStaticInfo, outProgress);
}

/* 0x812D2CE2 */
int TrophyTitleStorage::GetGroupProgressView(TrophyHandle *handle, int groupId,
					     void *outStaticInfo,
					     TrophyGroupProgressView *outProgress)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;
	return m_userStorage->GetGroupProgressView(handle, groupId, outStaticInfo, outProgress);
}

/* 0x812D2D4C */
int TrophyTitleStorage::GetTrophyStateView(TrophyHandle *handle, int trophyId, void *outStaticInfo,
					   TrophyEntryStateView *outState)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;
	return m_userStorage->GetTrophyStateView(handle, trophyId, outStaticInfo, outState);
}

/* 0x812D2DB6 */
int TrophyTitleStorage::GetGameProgressInfo(TrophyHandle *handle, void *outGameDetails)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;
	return m_userStorage->GetGameProgressInfo(handle, outGameDetails);
}

/* 0x812D2E0E */
int TrophyTitleStorage::GetGroupProgressInfo(TrophyHandle *handle, int groupId,
					     void *outGroupDetails)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;
	return m_userStorage->GetGroupProgressInfo(handle, groupId, outGroupDetails);
}

/* 0x812D2E6E */
int TrophyTitleStorage::GetTrophyUnlockInfo(TrophyHandle *handle, int trophyId, bool *outVisible,
					    void *outTrophyData)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;
	return m_userStorage->GetTrophyUnlockInfo(handle, trophyId, outVisible, outTrophyData);
}

/* 0x812D2EDA */
int TrophyTitleStorage::ReadConfHandle(TrophyHandle *handle, TrophyConfHandle **outHandle)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;
	return m_confStorage->ReadConfHandle(handle, outHandle);
}

/* 0x812D2F32 */
int TrophyTitleStorage::ExtractTitleText(TrophyHandle *handle, const TrophyConfHandle *confHandle,
					 void *outTitleDetails)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;
	return m_confStorage->ExtractTitleText(handle, confHandle, outTitleDetails);
}

/* 0x812D2F92 */
int TrophyTitleStorage::ExtractTrophyText(TrophyHandle *handle, const TrophyConfHandle *confHandle,
					  int trophyId, bool revealDetails, void *outTrophyDetails)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;
	return m_confStorage->ExtractTrophyText(handle, confHandle, trophyId, revealDetails,
						outTrophyDetails);
}

/* 0x812D3006 */
int TrophyTitleStorage::OpenSfmStream(TrophyHandle *handle, TrophyConfStream *stream)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;
	return m_confStorage->OpenSfmStream(handle, stream);
}

/* 0x812D305E */
int TrophyTitleStorage::OpenTitleIcon(TrophyHandle *handle, TrophyConfIconFile *iconFile)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;
	return m_confStorage->OpenTitleIcon(handle, iconFile);
}

/* 0x812D30B6 */
int TrophyTitleStorage::OpenGroupIcon(TrophyHandle *handle, int groupId,
				      TrophyConfIconFile *iconFile)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;
	return m_confStorage->OpenGroupIcon(handle, groupId, iconFile);
}

/* 0x812D3116 */
int TrophyTitleStorage::OpenTrophyIcon(TrophyHandle *handle, int trophyId,
				       TrophyConfIconFile *iconFile)
{
	int ret = EnsureOpenAndSessionReady(handle);
	if (ret < 0)
		return ret;
	return m_confStorage->OpenTrophyIcon(handle, trophyId, iconFile);
}
