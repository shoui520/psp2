/*
 * nsx_push_info.cpp: NSX PushInformation fetcher, XML parser, and NewEvent notification poster
 * 3.65-CEX: 0x8122B088 - 0x8122CC58
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/ctype.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/map>
#include <std/vector>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <common/string_util.h>
#include <datetime/datetime.h>
#include <file/httpfile.h>
#include "src/livespace_db/lsdb_keys.h"
#include <file/localfile.h>
#include <rtc/rtc.h>
#include <system/system.h>
#include "../shell_svc/src/shell_svc_newevent.h"
#include "nsx_internal.h"

struct AppInfoSClient {
	void *client;
};

extern "C" {
extern paf::string SceLsdb_C2B7D81B(void);
extern paf::string SceLsdb_88FDC55C(int source);
extern int SceLsdb_1673AD48(void);
extern int SceLsdb_315B9FD6(NewEventDbEntry *entry, bool notify);
extern void SceLsdb_D322A870(AppInfoSClient *client);
extern int SceLsdb_76EC65E3(AppInfoSClient *client, const paf::string *titleId);
extern void SceLsdb_56D8DCF0(AppInfoSClient *client);
}

namespace {

static const char *const kPushInfoTempPath = "ur0:shell/push_info/temp.xml";
static const char *const kPushInfoUrlFormat =
	"https://nsx.sec.%s.dl.playstation.net/nsx/sec/Xz78TMQ1Uf31VCYr/c/NSXVID/"
	"NSXVID-PN.VT.%s-PUSHINFORMATION.xml";

static const SceSize kPushInfoChunkBytes = 0x2000U;
static const SceSize kPushInfoIconMaxBytes = 0x8000U;
static const int kPushInfoActType = 11;
static const int kPushInfoExecMode = 0x00120000;

/* 0x8122CB0C..0x8122CB8A */
class NsxPushInfoParser : public NsxXmlParserBase {
public:
	NsxPushInfoParser(const paf::rtc::Tick *referenceTick)
		: NsxXmlParserBase(referenceTick),
		  m_userAge(SceLsdb_1673AD48())
	{
	}

	virtual ~NsxPushInfoParser()
	{
	}

	virtual bool ValidateRootNode(const NsxXmlNode *rootNode)
	{
		return NsxXmlParserBase::ValidateRootNode(rootNode) &&
		       CheckMinSystemVersion(rootNode);
	}

	virtual bool ValidateMaterial(const NsxMaterialItem *material)
	{
		if (!NsxXmlParserBase::ValidateMaterial(material))
			return false;

		int deviceFilter = nsxGetAnnotationInt(material->annotations, paf::string("device"), 0);
		bool deviceMatched =
			(deviceFilter == 0) ||
			(deviceFilter == 1 && paf::system::Model::GetType() == 0) ||
			(deviceFilter == 2 && paf::system::Model::GetType() == 1);
		if (!deviceMatched)
			return false;

		if (material->targetType != NSX_TARGET_URI ||
		    material->description.size() > 0x100U ||
		    material->targetUri.size() > 0x3C0U) {
			return false;
		}

		if (m_userAge >= 0 &&
		    (material->minAge > m_userAge || m_userAge > material->maxAge)) {
			return false;
		}
		return true;
	}

private:
	int m_userAge;
};

/* 0x8122B1BA */
static int setLocalFileTimestamps(const char *path, paf::rtc::Tick mtimeTick, paf::rtc::Tick ctimeTick)
{
	paf::LocalFileStat fileStat;
	int ret = paf::LocalFile::Getstat(path, &fileStat);
	if (ret != 0)
		return ret;

	SceUInt32 changeBits = 0;
	if (ctimeTick != 0) {
		paf::rtc::Tick tickVal = ctimeTick;
		fileStat.st_ctime = paf::datetime::DateTime::ConvertRtcTickToDateTime(&tickVal);
		changeBits |= 0x08U;
	}
	if (mtimeTick != 0) {
		paf::rtc::Tick tickVal = mtimeTick;
		fileStat.st_mtime = paf::datetime::DateTime::ConvertRtcTickToDateTime(&tickVal);
		changeBits |= 0x20U;
	}

	if (changeBits != 0)
		return paf::LocalFile::Chstat(path, &fileStat, changeBits);
	return 0;
}

/* 0x8122B38A */
static paf::string buildPushTargetUri(const NsxMaterialItem *material)
{
	if (sce_paf_memcmp(material->targetUri.c_str(), "psns:", 5) != 0)
		return material->targetUri;

	const char *uriStart = material->targetUri.c_str();
	const char *queryPos = sce_paf_strchr(uriStart, '?');
	if (queryPos == NULL)
		return material->targetUri;

	size_t queryOffset = (size_t)(queryPos - uriStart);
	paf::map<paf::string, paf::string> queryParams;
	paf::common::string_util::mapping(queryParams, material->targetUri, queryOffset + 1, '&', '=');

	const char *productId = NULL;
	for (paf::map<paf::string, paf::string>::iterator iter = queryParams.begin();
	     iter != queryParams.end(); ++iter) {
		if (iter->first == "product" || iter->first == "content") {
			productId = iter->second.c_str();
			break;
		}
	}

	char idBuf[64];
	sce_paf_snprintf(idBuf, sizeof(idBuf), "%lld", (long long)material->materialId);

	paf::string result = material->targetUri;
	result += "&_vita_internal_=notification&matid=";
	result += idBuf;
	if (productId != NULL) {
		result += "&prdid=";
		result += productId;
	}
	return result;
}

/* 0x8122B088..0x8122C9EE */
class NsxPushInfoSession {
public:
	NsxPushInfoSession();
	~NsxPushInfoSession();

	int Execute();

private:
	int OpenSources();
	int ParseOldAndNew(paf::vector<NsxMaterialItem *> *oldMaterials,
			   paf::vector<NsxMaterialItem *> *newMaterials);
	int PostNewMaterials(const paf::vector<NsxMaterialItem *> &oldMaterials,
			     const paf::vector<NsxMaterialItem *> &newMaterials);
	int PostSingleMaterial(const NsxMaterialItem *material);
	int CommitCacheFile();

	paf::string m_cachePath;
	paf::common::SharedPtr<paf::File> m_localFile;
	paf::HttpFile m_httpFile;
	paf::rtc::Tick m_currentTick;
	paf::rtc::Tick m_cachedMtimeTick;
};

/* 0x8122B088 */
NsxPushInfoSession::NsxPushInfoSession()
	: m_currentTick(0),
	  m_cachedMtimeTick(0)
{
	paf::string npEnv = SceLsdb_C2B7D81B();
	paf::string country = SceLsdb_88FDC55C(1);
	char pathBuf[256];
	sce_paf_snprintf(pathBuf, sizeof(pathBuf), "ur0:shell/push_info/%s_%s.xml",
			 npEnv.c_str(), country.c_str());
	m_cachePath = pathBuf;
	m_currentTick = nsxGetNetworkTick();
}

/* 0x8122B140 */
NsxPushInfoSession::~NsxPushInfoSession()
{
}

/* 0x8122B302 */
int NsxPushInfoSession::CommitCacheFile()
{
	if (m_httpFile.IsOpened()) {
		SceUInt64 lastMod = nsxHttpGetLastModifiedTick(&m_httpFile);
		int ret = setLocalFileTimestamps(kPushInfoTempPath, m_currentTick, lastMod);
		if (ret != 0)
			return ret;

		if (m_localFile.get() != NULL) {
			m_localFile->Close();
			paf::LocalFile::RemoveFile(m_cachePath.c_str());
		}
		return paf::LocalFile::RenameFile(kPushInfoTempPath, m_cachePath.c_str());
	}

	if (m_localFile.get() == NULL)
		return 0;
	return setLocalFileTimestamps(m_cachePath.c_str(), m_currentTick, 0);
}

/* 0x8122B8CC */
int NsxPushInfoSession::PostSingleMaterial(const NsxMaterialItem *material)
{
	NewEventDbEntry notice;
	notice.m_titleId = "NSX";

	char idBuf[64];
	sce_paf_snprintf(idBuf, sizeof(idBuf), "%lld", (long long)material->materialId);
	notice.m_itemId = idBuf;
	notice.m_msgType = 256;
	notice.m_actType = kPushInfoActType;
	notice.m_execMode = kPushInfoExecMode;
	notice.m_execArg = buildPushTargetUri(material);
	notice.m_newFlag = 1;
	notice.m_desc = material->description;

	paf::common::SharedPtr<paf::MallocBuffer> iconBuf;
	if (!material->iconUrl.empty() &&
	    nsxGetAnnotationInt(material->annotations, paf::string("customIcon"), 0) == 1) {
		int httpErr = 0;
		paf::common::SharedPtr<paf::HttpFile> iconHttp =
			paf::HttpFile::Open(material->iconUrl.c_str(), &httpErr, 0);
		if (httpErr != 0)
			return httpErr;

		iconBuf = paf::MallocBuffer::Allocate(kPushInfoIconMaxBytes);
		if (iconBuf.get() == NULL)
			return kNsxErrorNoMemory;

		int readLen = iconHttp->Read(iconBuf->GetAddr(), iconBuf->GetSize());
		if (readLen < 0)
			return readLen;

		notice.m_iconData = iconBuf->GetAddr();
		notice.m_iconDataSize = readLen;
	}

	return SceLsdb_315B9FD6(&notice, true);
}

/* 0x8122C02C */
int NsxPushInfoSession::PostNewMaterials(const paf::vector<NsxMaterialItem *> &oldMaterials,
					 const paf::vector<NsxMaterialItem *> &newMaterials)
{
	for (size_t newIndex = 0; newIndex < newMaterials.size(); ++newIndex) {
		const NsxMaterialItem *candidate = newMaterials[newIndex];
		bool alreadyPosted = false;
		for (size_t oldIndex = 0; oldIndex < oldMaterials.size(); ++oldIndex) {
			if (candidate->materialId == oldMaterials[oldIndex]->materialId) {
				alreadyPosted = true;
				break;
			}
		}
		if (alreadyPosted)
			continue;

		paf::string skipTitle = nsxGetAnnotationString(
			candidate->annotations, paf::string("doNotPushTitleId"), paf::string());
		if (!skipTitle.empty()) {
			AppInfoSClient lsdbClient;
			SceLsdb_D322A870(&lsdbClient);
			int installed = SceLsdb_76EC65E3(&lsdbClient, &skipTitle);
			SceLsdb_56D8DCF0(&lsdbClient);
			if (installed != 0)
				continue;
		}

		int ret = PostSingleMaterial(candidate);
		if (ret != 0)
			return ret;
	}
	return 0;
}

/* 0x8122C18A */
int NsxPushInfoSession::ParseOldAndNew(paf::vector<NsxMaterialItem *> *oldMaterials,
				       paf::vector<NsxMaterialItem *> *newMaterials)
{
	paf::common::SharedPtr<paf::MallocBuffer> ioBuf =
		paf::MallocBuffer::Allocate(kPushInfoChunkBytes);
	if (ioBuf.get() == NULL)
		return kNsxErrorNoMemory;

	char *chunkPtr = (char *)ioBuf->GetAddr();
	paf::File *oldSource = m_localFile.get();
	paf::File *newSource = m_httpFile.IsOpened() ? (paf::File *)&m_httpFile : oldSource;

	NsxPushInfoParser oldParser(&m_cachedMtimeTick);
	NsxPushInfoParser newParser(&m_currentTick);

	if (oldSource != NULL) {
		bool reachedEnd = false;
		while (!reachedEnd) {
			int readBytes = oldSource->Read(chunkPtr, kPushInfoChunkBytes);
			if (readBytes < 0)
				return kNsxErrorParseFailed;

			reachedEnd = ((SceSize)readBytes < kPushInfoChunkBytes);
			int ret = oldParser.ParseChunk(chunkPtr, (SceSize)readBytes, reachedEnd, oldMaterials);
			if (ret != 0)
				return ret;

			if (newSource == oldSource) {
				ret = newParser.ParseChunk(chunkPtr, (SceSize)readBytes, reachedEnd, newMaterials);
				if (ret != 0)
					return ret;
			}
		}
	}

	if (newSource != NULL && newSource != oldSource) {
		int openErr = 0;
		paf::common::SharedPtr<paf::File> tempFile =
			paf::LocalFile::Open(kPushInfoTempPath, 0x0602U, 0x06U, &openErr);
		if (openErr != 0)
			return openErr;

		bool reachedEnd = false;
		while (!reachedEnd) {
			int readBytes = newSource->Read(chunkPtr, kPushInfoChunkBytes);
			if (readBytes < 0)
				return kNsxErrorParseFailed;

			reachedEnd = ((SceSize)readBytes < kPushInfoChunkBytes);
			int ret = newParser.ParseChunk(chunkPtr, (SceSize)readBytes, reachedEnd, newMaterials);
			if (ret != 0)
				return ret;

			int writeRet = tempFile->Write(chunkPtr, (SceSize)readBytes);
			if (writeRet < 0)
				return writeRet;
		}
	}

	return 0;
}

/* 0x8122C78E */
int NsxPushInfoSession::OpenSources()
{
	if (m_currentTick == 0)
		return kNsxErrorNoNetworkTime;

	m_localFile = paf::LocalFile::Open(m_cachePath.c_str(), 0x10001U, 0U, NULL);

	SceUInt64 ifModifiedTick = 0;
	if (m_localFile.get() != NULL) {
		paf::FileStat fileStat;
		if (m_localFile->Getstat(&fileStat) == 0) {
			ifModifiedTick = paf::datetime::DateTime::ConvertDateTimeToRtcTick(fileStat.st_ctime);
			m_cachedMtimeTick = paf::datetime::DateTime::ConvertDateTimeToRtcTick(fileStat.st_mtime);
		}
	}

	paf::string npEnv = SceLsdb_C2B7D81B();
	paf::string country = SceLsdb_88FDC55C(1);
	for (size_t index = 0; index < country.size(); ++index)
		country[index] = (char)sce_paf_toupper((unsigned char)country[index]);

	char urlBuf[256];
	sce_paf_snprintf(urlBuf, sizeof(urlBuf), kPushInfoUrlFormat, npEnv.c_str(), country.c_str());
	nsxHttpOpenIfModified(&m_httpFile, urlBuf, &ifModifiedTick);
	return 0;
}

/* 0x8122C9EE */
int NsxPushInfoSession::Execute()
{
	paf::vector<NsxMaterialItem *> oldMaterials;
	paf::vector<NsxMaterialItem *> newMaterials;

	int ret = OpenSources();
	if (ret == 0)
		ret = ParseOldAndNew(&oldMaterials, &newMaterials);
	if (ret == 0)
		ret = PostNewMaterials(oldMaterials, newMaterials);
	if (ret == 0)
		ret = CommitCacheFile();

	nsxFreeMaterialList(&newMaterials);
	nsxFreeMaterialList(&oldMaterials);
	return ret;
}

} /* namespace */

/* 0x8122B04C */
int nsxPushInfoRunCheck(void)
{
	NsxPushInfoSession session;
	return session.Execute();
}
