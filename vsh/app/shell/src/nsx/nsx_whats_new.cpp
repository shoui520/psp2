/*
 * nsx_whats_new.cpp: NSX WhatsNew XML updater, local image cache sync, and device token query
 * 3.65-CEX: 0x8122CD08 - 0x81230DF0
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/ctype.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/list>
#include <std/vector>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <common/string_util.h>
#include <datetime/datetime.h>
#include <dir/dir.h>
#include <file/httpfile.h>
#include <file/localfile.h>
#include <job/job.h>
#include <plugin.h>
#include <rtc/rtc.h>
#include <system/system.h>
#include <thread/thread.h>
#include <kernel/iofilemgr.h>
#include "../../plugins.h"
#include "nsx_internal.h"

extern "C" {
extern paf::string SceLsdb_C2B7D81B(void);
extern paf::string SceLsdb_88FDC55C(int source);
extern int shellAesInitEncrypt(void *aesCtx, SceSize blockBits, SceSize keyBits, const void *key);
extern void shellAesEncryptBlock(const void *aesCtx, const void *inBlock, void *outBlock);
}

extern int liveareaCronEnqueueJob(paf::common::SharedPtr<paf::job::JobItem> &item);
extern void liveareaCronRegister(void (*callback)(void *), void *arg);
extern void liveareaCronUnregister(void (*callback)(void *));
extern int npGenerateDeviceToken(void *outTokenBuf);
extern int shellNpHasAccountId(int *hasAccount);
extern int consoleInfoFileReadFlag(int index, SceUInt8 *value, int mode);
extern paf::string telephonyPlmnFormatCode(const TelephonyPlmn *plmn);

namespace {

static const char *const kWhatsNewTempDir = "ur0:shell/whats_new/temp";
static const char *const kWhatsNewTempXml = "ur0:shell/whats_new/temp/whatsnew_temp.xml";
static const char *const kWhatsNewUrlFormat =
	"https://nsx.sec.%s.dl.playstation.net/nsx/sec/Xz78TMQ1Uf31VCYr/c/NSXWSV/"
	"NSXWSV-PN.%s.%s-WHATSNEW00000001.xml%s";

static const SceSize kWhatsNewChunkBytes = 0x2000U;
static const int kWhatsNewCacheHours = 23;
static const int kWhatsNewUpdateDownloaded = 1;
static const int kWhatsNewUpdateNotModified = 2;

static const SceUInt8 kMaskedAesKey[16] = {
	0x2F, 0xAC, 0x4B, 0x65, 0xB4, 0x03, 0x92, 0xF4,
	0xF5, 0x9B, 0xC6, 0x6D, 0x30, 0xEE, 0xE2, 0x7B
};

static const SceUInt8 kMaskedAesIv[16] = {
	0x87, 0x1C, 0xAE, 0xF1, 0x19, 0x75, 0xF8, 0x21,
	0xD3, 0x78, 0xBA, 0x88, 0x98, 0xB4, 0x70, 0x2C
};

struct NsxImagePair {
	paf::string fileName;
	paf::string sourceUrl;
};

struct NsxAesContext {
	SceUInt32 headerWords[2];
	SceUInt32 blockWordCount;
	SceUInt8 keySchedule[996];
};

struct NsxTelephonyModemVtable {
	void (*getOperatorIdSlot[16])(void);
	void (*getOperatorId)(void *self, int *operatorId);
};

struct NsxTelephonyModem {
	const NsxTelephonyModemVtable *vtable;
};

struct NsxTelephonyServiceVtable {
	void (*leadingSlots[9])(void);
	TelephonyResult (*getImei)(void *self, paf::string *outImei);		/* +0x24 */
	TelephonyResult (*getImsi)(void *self, paf::string *outImsi);		/* +0x28 */
	void (*middleSlots[14])(void);
	TelephonyResult (*getHomePlmn)(void *self, TelephonyPlmn *outPlmn);	/* +0x64 */
};

struct NsxTelephonyService {
	const NsxTelephonyServiceVtable *vtable;
};

struct NsxTelephonyDevicePayload {
	SceUInt32 localTickLow;
	char imei[15];
	char imsi[20];
	char plmnCode[6];
	SceUInt8 zeroTail[3];
};

/* 0x8122CDA8 */
static int encryptDevicePayloadAesCbc(SceUInt8 *buffer, int length)
{
	SceUInt8 key[16];
	SceUInt8 iv[16];
	for (int index = 0; index < 16; ++index) {
		key[index] = kMaskedAesKey[index] ^ 0x88U;
		iv[index] = kMaskedAesIv[index] ^ 0x88U;
	}

	NsxAesContext aesCtx;
	sce_paf_memset(&aesCtx, 0, sizeof(aesCtx));
	int ret = shellAesInitEncrypt(&aesCtx, 128, 128, key);
	if (ret < 0)
		return ret;

	SceUInt8 chainBlock[16];
	SceUInt32 blockBytes = 4U * aesCtx.blockWordCount;
	if (blockBytes > sizeof(chainBlock))
		blockBytes = sizeof(chainBlock);
	sce_paf_memcpy(chainBlock, iv, blockBytes);

	SceUInt8 xoredBlock[16];
	for (int offset = 0; offset < length; offset += 16) {
		for (SceUInt32 bytePos = 0; bytePos < blockBytes; ++bytePos)
			xoredBlock[bytePos] = buffer[offset + bytePos] ^ chainBlock[bytePos];
		shellAesEncryptBlock(&aesCtx, xoredBlock, &buffer[offset]);
		sce_paf_memcpy(chainBlock, &buffer[offset], blockBytes);
	}
	return 0;
}

static paf::string bytesToLowerHex(const SceUInt8 *bytes, SceSize length)
{
	static const char kHexDigits[17] = "0123456789abcdef";
	char hexBuf[128];
	SceSize outPos = 0;
	for (SceSize index = 0; index < length && outPos + 2 < sizeof(hexBuf); ++index) {
		hexBuf[outPos++] = kHexDigits[bytes[index] >> 4];
		hexBuf[outPos++] = kHexDigits[bytes[index] & 0x0FU];
	}
	hexBuf[outPos] = '\0';
	return paf::string(hexBuf);
}

/* 0x81230042 */
static paf::string buildTelephonySidToken(void)
{
	NsxTelephonyDevicePayload payload;
	sce_paf_memset(&payload, 0xFF, sizeof(payload));
	sce_paf_memset(payload.zeroTail, 0, sizeof(payload.zeroTail));

	paf::rtc::Tick localTick = 0;
	if (paf::rtc::GetCurrentLocalTick(&localTick) == 0)
		payload.localTickLow = (SceUInt32)localTick;
	else
		payload.localTickLow = 0;

	paf::Plugin *telPlugin = paf::Plugin::Find("telephony_plugin");
	if (telPlugin != NULL) {
		TelephonyPluginInterface *telIf =
			(TelephonyPluginInterface *)telPlugin->GetInterface(1);
		if (telIf != NULL) {
			NsxTelephonyService *service = (NsxTelephonyService *)telIf->getService();
			if (service != NULL) {
				paf::string imeiStr;
				paf::string imsiStr;
				TelephonyPlmn plmn;

				TelephonyResult imeiRes = service->vtable->getImei(service, &imeiStr);
				if (imeiRes.GetCode() == 0) {
					SceSize copyLen = imeiStr.size();
					if (copyLen > sizeof(payload.imei))
						copyLen = sizeof(payload.imei);
					sce_paf_memcpy(payload.imei, imeiStr.c_str(), copyLen);
				}

				TelephonyResult imsiRes = service->vtable->getImsi(service, &imsiStr);
				if (imsiRes.GetCode() == 0) {
					SceSize copyLen = imsiStr.size();
					if (copyLen > sizeof(payload.imsi))
						copyLen = sizeof(payload.imsi);
					sce_paf_memcpy(payload.imsi, imsiStr.c_str(), copyLen);
				}

				TelephonyResult plmnRes = service->vtable->getHomePlmn(service, &plmn);
				if (plmnRes.GetCode() == 0) {
					paf::string plmnStr = telephonyPlmnFormatCode(&plmn);
					SceSize copyLen = plmnStr.size();
					if (copyLen > sizeof(payload.plmnCode))
						copyLen = sizeof(payload.plmnCode);
					sce_paf_memcpy(payload.plmnCode, plmnStr.c_str(), copyLen);
				}
			}
		}
	}

	encryptDevicePayloadAesCbc((SceUInt8 *)&payload, (int)sizeof(payload));
	return bytesToLowerHex((const SceUInt8 *)&payload, sizeof(payload));
}

/* 0x812302C8 */
static paf::string buildSidQueryString(void)
{
	SceUInt8 deviceToken[32];
	sce_paf_memset(deviceToken, 0, sizeof(deviceToken));
	npGenerateDeviceToken(deviceToken);

	paf::string sidToken = bytesToLowerHex(deviceToken, sizeof(deviceToken));
	paf::string telToken = buildTelephonySidToken();

	paf::string query("?sid1=");
	query = query + sidToken;
	query = query + "&sid2=";
	query = query + telToken;
	return query;
}

/* 0x812305C0 */
static void recordWhatsNewUsageCounter(bool succeeded)
{
	if (!succeeded) {
		shellUsageCounterAdd(20, 1U, 0);
		return;
	}

	SceUInt8 netKind = 0;
	if (consoleInfoFileReadFlag(1, &netKind, 0) != 0)
		return;

	if (netKind == 1U)
		shellUsageCounterAdd(18, 1U, 0);
	else if (netKind == 2U)
		shellUsageCounterAdd(19, 1U, 0);
	else if (netKind == 3U)
		shellUsageCounterAdd(276, 1U, 0);
}

/* 0x8122D78C */
static int updateFileTimestamps(const char *path, const paf::rtc::Tick *ctimeTick,
				const paf::rtc::Tick *mtimeTick)
{
	if (path == NULL)
		return kNsxErrorInvalidState;

	paf::LocalFileStat fileStat;
	if (paf::LocalFile::Getstat(path, &fileStat) != 0)
		return kNsxErrorFileOpen;

	SceUInt32 changeBits = 0;
	if (ctimeTick != NULL && *ctimeTick != 0) {
		paf::rtc::Tick tickVal = *ctimeTick;
		fileStat.st_ctime = paf::datetime::DateTime::ConvertRtcTickToDateTime(&tickVal);
		changeBits |= 0x08U;
	}
	if (mtimeTick != NULL && *mtimeTick != 0) {
		paf::rtc::Tick tickVal = *mtimeTick;
		fileStat.st_mtime = paf::datetime::DateTime::ConvertRtcTickToDateTime(&tickVal);
		changeBits |= 0x20U;
	}

	if (changeBits != 0)
		return paf::LocalFile::Chstat(path, &fileStat, changeBits);
	return 0;
}

/* 0x8122FED2 */
static paf::rtc::Tick getLocalFileMtimeTick(const char *path)
{
	if (path == NULL)
		return 0;
	paf::LocalFileStat fileStat;
	if (paf::LocalFile::Getstat(path, &fileStat) != 0)
		return 0;
	return paf::datetime::DateTime::ConvertDateTimeToRtcTick(fileStat.st_mtime);
}

/* 0x8122FF8A */
static paf::rtc::Tick getLocalFileCtimeTick(const char *path)
{
	if (path == NULL)
		return 0;
	paf::LocalFileStat fileStat;
	if (paf::LocalFile::Getstat(path, &fileStat) != 0)
		return 0;
	return paf::datetime::DateTime::ConvertDateTimeToRtcTick(fileStat.st_ctime);
}

/* 0x8122D9EE */
static int downloadImageToFile(const paf::string &destPath, const paf::string &sourceUrl)
{
	paf::common::SharedPtr<paf::MallocBuffer> ioBuf =
		paf::MallocBuffer::Allocate(kWhatsNewChunkBytes);
	if (ioBuf.get() == NULL)
		return kNsxErrorNoMemory;

	int httpErr = 0;
	paf::common::SharedPtr<paf::HttpFile> httpFile =
		paf::HttpFile::Open(sourceUrl.c_str(), &httpErr, 0);
	if (httpErr != 0)
		return httpErr;

	int localErr = 0;
	paf::common::SharedPtr<paf::File> localFile =
		paf::LocalFile::Open(destPath.c_str(), 0x10602U, 0x06U, &localErr);
	if (localErr != 0)
		return localErr;

	while (!httpFile->IsEof()) {
		int readBytes = httpFile->Read(ioBuf->GetAddr(), ioBuf->GetSize());
		if (readBytes < 0)
			return readBytes;
		if (readBytes == 0)
			break;
		int writeRet = localFile->Write(ioBuf->GetAddr(), (SceSize)readBytes);
		if (writeRet < 0)
			return writeRet;
	}
	return 0;
}

/* 0x81230BAC..0x81230C6C */
class NsxWhatsNewParser : public NsxXmlParserBase {
public:
	NsxWhatsNewParser(const paf::rtc::Tick *referenceTick, int operatorId)
		: NsxXmlParserBase(referenceTick),
		  m_nextRefreshTick(0),
		  m_horizonTick(0),
		  m_operatorId(operatorId)
	{
		paf::rtc::TickAddDays(&m_horizonTick, referenceTick, 1);
	}

	virtual ~NsxWhatsNewParser()
	{
	}

	virtual bool ValidateMaterial(const NsxMaterialItem *material)
	{
		if (!NsxXmlParserBase::ValidateMaterial(material))
			return false;

		int requiredOperator = nsxMaterialGetOperatorId(material);
		return requiredOperator == 0 || m_operatorId == requiredOperator;
	}

	virtual bool ValidateWindow(const NsxMaterialItem *material)
	{
		if (NsxXmlParserBase::ValidateWindow(material))
			return true;

		if (m_referenceTick < material->validFromTick &&
		    material->validFromTick < m_horizonTick) {
			return true;
		}

		if (m_horizonTick < material->validFromTick) {
			if (m_nextRefreshTick == 0 || material->validFromTick < m_nextRefreshTick)
				m_nextRefreshTick = material->validFromTick;
		}
		return false;
	}

	paf::rtc::Tick GetNextRefreshTick() const { return m_nextRefreshTick; }

private:
	paf::rtc::Tick m_nextRefreshTick;
	paf::rtc::Tick m_horizonTick;
	int m_operatorId;
};

/* 0x8122E476 */
static bool writeWhatsNewXmlHeader(paf::common::SharedPtr<paf::File> outFile,
				   const paf::string &rootTagName)
{
	if (rootTagName != kNsxTagRoot)
		return false;
	static const char kHeaderText[] = "<nsx><spc>\n";
	outFile->Write(kHeaderText, sizeof(kHeaderText) - 1);
	return true;
}

/* 0x8122E500 */
static bool filterAndWriteMaterials(paf::common::SharedPtr<paf::File> outFile,
				    NsxXmlNode *spaceNode,
				    paf::list<NsxImagePair> *imageList,
				    bool isFinal)
{
	paf::list<NsxXmlNode>::iterator matIter = spaceNode->children.begin();
	while (matIter != spaceNode->children.end()) {
		if (!isFinal && spaceNode->children.size() == 1)
			return true;

		if (matIter->name == kNsxTagMaterial) {
			paf::list<NsxXmlNode>::iterator childIter = matIter->children.begin();
			while (childIter != matIter->children.end()) {
				if (childIter->name == kNsxTagUrl ||
				    childIter->name == kNsxTagTarget ||
				    childIter->name == kNsxTagCountry ||
				    childIter->name == kNsxTagLang) {
					++childIter;
				} else {
					childIter = matIter->children.erase(childIter);
				}
			}

			for (paf::list<NsxXmlNode>::iterator urlIter = matIter->children.begin();
			     urlIter != matIter->children.end(); ++urlIter) {
				if (urlIter->name == kNsxTagUrl) {
					NsxImagePair pair;
					pair.sourceUrl = urlIter->text;
					paf::list<paf::string> urlParts;
					paf::common::string_util::tokenize(urlParts, urlIter->text, 0, '/');
					if (urlParts.size() != 0)
						pair.fileName = urlParts.back();
					imageList->push_back(pair);
					urlIter->text = pair.fileName;
				}
			}

			paf::string serialized;
			if (shellXmlNodeSerialize(&serialized, &(*matIter), 0) != 0)
				return false;
			if (outFile->Write(serialized.c_str(), serialized.size()) < 0)
				return false;
		}
		matIter = spaceNode->children.erase(matIter);
	}
	return false;
}

/* 0x8122E7D4 */
static bool processWhatsNewSpaces(paf::common::SharedPtr<paf::File> outFile,
				  NsxXmlNode *rootNode,
				  paf::list<NsxImagePair> *imageList,
				  bool isFinal)
{
	paf::list<NsxXmlNode>::iterator iter = rootNode->children.begin();
	while (iter != rootNode->children.end()) {
		if (!isFinal && rootNode->children.size() == 1)
			return true;

		if (iter->name == kNsxTagSpace) {
			if (!filterAndWriteMaterials(outFile, &(*iter), imageList, isFinal))
				return false;
		}
		iter = rootNode->children.erase(iter);
	}
	return true;
}

/* 0x8122E91C */
static bool writeWhatsNewXmlFooter(paf::common::SharedPtr<paf::File> outFile)
{
	static const char kFooterText[] = "</spc></nsx>";
	outFile->Write(kFooterText, sizeof(kFooterText) - 1);
	return true;
}

/* 0x81230B04..0x81230B8C */
class NsxUpdateWhatsNewJob : public paf::job::JobItem {
public:
	NsxUpdateWhatsNewJob(NsxWhatsNewAccessorImpl *owner, void (*callback)(void *),
			     void *callbackArg, bool forceRefresh);
	virtual ~NsxUpdateWhatsNewJob();
	virtual int Run();
	virtual void Finish(int result);

private:
	NsxWhatsNewAccessorImpl *m_owner;
	void (*m_callback)(void *);
	void *m_callbackArg;
	bool m_forceRefresh;
};

} /* namespace */

/* 0x8122CEBE..0x81230A88 */
class NsxWhatsNewAccessorImpl {
public:
	NsxWhatsNewAccessorImpl(void (*callback)(void *), void *callbackArg);
	~NsxWhatsNewAccessorImpl();

	int LoadMaterials(paf::vector<NsxMaterialItem *> *outMaterials,
			  paf::rtc::Tick *outNextRefreshTick);
	int EnqueueUpdate(bool forceRefresh);
	int RunUpdate(bool forceRefresh);

private:
	int ParseCachedXml(const paf::string &xmlPath, const paf::rtc::Tick *referenceTick,
			   paf::rtc::Tick *outNextRefreshTick,
			   paf::vector<NsxMaterialItem *> *outMaterials);
	int ReplaceCacheXml(const char *tempPath, const char *destPath,
			    const paf::rtc::Tick *ctimeTick, const paf::rtc::Tick *mtimeTick);
	int SyncImagesAndCommit(const paf::string &destXmlPath,
				const paf::rtc::Tick *ctimeTick,
				const paf::rtc::Tick *mtimeTick,
				const paf::list<NsxImagePair> &neededImages,
				const paf::string &npEnv);
	int StreamFilterHttpToTemp(paf::HttpFile *httpFile,
				   paf::common::SharedPtr<paf::File> outFile,
				   paf::list<NsxImagePair> *outImages);
	int RewriteHttpToTempFile(paf::HttpFile *httpFile, paf::list<NsxImagePair> *outImages);

	static void OnCronUpdate(void *userArg);

	paf::thread::Mutex m_mutex;
	bool m_canceled;
	void (*m_callback)(void *);
	void *m_callbackArg;
};

NsxUpdateWhatsNewJob::NsxUpdateWhatsNewJob(NsxWhatsNewAccessorImpl *owner,
					   void (*callback)(void *),
					   void *callbackArg,
					   bool forceRefresh)
	: paf::job::JobItem("SceNsxUpdateNsxObjJob", NULL),
	  m_owner(owner),
	  m_callback(callback),
	  m_callbackArg(callbackArg),
	  m_forceRefresh(forceRefresh)
{
}

NsxUpdateWhatsNewJob::~NsxUpdateWhatsNewJob()
{
}

int NsxUpdateWhatsNewJob::Run()
{
	return m_owner->RunUpdate(m_forceRefresh);
}

void NsxUpdateWhatsNewJob::Finish(int result)
{
	if (m_callback != NULL && result == kWhatsNewUpdateDownloaded && !IsCanceled())
		m_callback(m_callbackArg);
}

/* 0x8122CEBE */
NsxWhatsNewAccessorImpl::NsxWhatsNewAccessorImpl(void (*callback)(void *), void *callbackArg)
	: m_mutex("SceShellNsxAcsXmlMutex", 0),
	  m_canceled(false),
	  m_callback(callback),
	  m_callbackArg(callbackArg)
{
	liveareaCronRegister(OnCronUpdate, this);
}

/* 0x8122CEF2 */
NsxWhatsNewAccessorImpl::~NsxWhatsNewAccessorImpl()
{
	liveareaCronUnregister(OnCronUpdate);
}

/* 0x81230A88 */
void NsxWhatsNewAccessorImpl::OnCronUpdate(void *userArg)
{
	NsxWhatsNewAccessorImpl *self = (NsxWhatsNewAccessorImpl *)userArg;
	self->EnqueueUpdate(false);
}

/* 0x8122CF10 */
int NsxWhatsNewAccessorImpl::ParseCachedXml(const paf::string &xmlPath,
					    const paf::rtc::Tick *referenceTick,
					    paf::rtc::Tick *outNextRefreshTick,
					    paf::vector<NsxMaterialItem *> *outMaterials)
{
	paf::common::SharedPtr<paf::MallocBuffer> ioBuf =
		paf::MallocBuffer::Allocate(kWhatsNewChunkBytes);
	if (ioBuf.get() == NULL)
		return kNsxErrorNoMemory;

	int openErr = 0;
	paf::common::SharedPtr<paf::File> localFile =
		paf::LocalFile::Open(xmlPath.c_str(), 0x10001U, 0U, &openErr);
	if (localFile.get() == NULL || openErr != 0)
		return kNsxErrorFileOpen;

	int operatorId = 0;
	paf::Plugin *telPlugin = paf::Plugin::Find("telephony_plugin");
	if (telPlugin != NULL) {
		TelephonyPluginInterface *telIf =
			(TelephonyPluginInterface *)telPlugin->GetInterface(1);
		if (telIf != NULL) {
			NsxTelephonyModem *modem = (NsxTelephonyModem *)telIf->getModem();
			if (modem != NULL)
				modem->vtable->getOperatorId(modem, &operatorId);
		}
	}

	NsxWhatsNewParser parser(referenceTick, operatorId);
	bool reachedEnd = false;
	while (!reachedEnd) {
		int readBytes = localFile->Read(ioBuf->GetAddr(), kWhatsNewChunkBytes);
		if (readBytes < 0)
			return kNsxErrorParseFailed;

		reachedEnd = ((SceSize)readBytes < kWhatsNewChunkBytes);
		int ret = parser.ParseChunk((const char *)ioBuf->GetAddr(), (SceSize)readBytes,
					    reachedEnd, outMaterials);
		if (ret != 0)
			return ret;
	}

	*outNextRefreshTick = parser.GetNextRefreshTick();
	return 0;
}

/* 0x8122D2A8 / 0x8122D65E */
int NsxWhatsNewAccessorImpl::LoadMaterials(paf::vector<NsxMaterialItem *> *outMaterials,
					   paf::rtc::Tick *outNextRefreshTick)
{
	m_mutex.Lock();

	paf::string npEnv = SceLsdb_C2B7D81B();
	char pathBuf[256];
	sce_paf_snprintf(pathBuf, sizeof(pathBuf), "ur0:shell/whats_new/%s", npEnv.c_str());
	paf::string xmlPath(pathBuf);
	xmlPath += "/";
	xmlPath += "whatsnew.xml";

	paf::rtc::Tick nowTick = nsxGetNetworkTick();
	if (nowTick != 0 && paf::LocalFile::Exists(xmlPath.c_str())) {
		int ret = ParseCachedXml(xmlPath, &nowTick, outNextRefreshTick, outMaterials);
		if (ret != 0) {
			m_mutex.Unlock();
			return ret;
		}

		char imgDirBuf[256];
		sce_paf_snprintf(imgDirBuf, sizeof(imgDirBuf), "ur0:shell/whats_new/%s/img",
				 npEnv.c_str());
		paf::string imgDir(imgDirBuf);
		for (size_t index = 0; index < outMaterials->size(); ++index) {
			NsxMaterialItem *material = (*outMaterials)[index];
			material->iconUrl = imgDir + "/" + material->iconUrl;
		}
	}

	m_mutex.Unlock();
	return 0;
}

/* 0x8122D666 */
int NsxWhatsNewAccessorImpl::EnqueueUpdate(bool forceRefresh)
{
	paf::common::SharedPtr<paf::job::JobItem> jobRef(
		new NsxUpdateWhatsNewJob(this, m_callback, m_callbackArg, forceRefresh));
	int ret = liveareaCronEnqueueJob(jobRef);
	return (ret != 0) ? ret : 0;
}

/* 0x8122D94E */
int NsxWhatsNewAccessorImpl::ReplaceCacheXml(const char *tempPath, const char *destPath,
					     const paf::rtc::Tick *ctimeTick,
					     const paf::rtc::Tick *mtimeTick)
{
	if (destPath == NULL || tempPath == NULL || ctimeTick == NULL || mtimeTick == NULL)
		return kNsxErrorInvalidArg;
	if (!paf::LocalFile::Exists(tempPath) || !paf::LocalFile::IsFile(tempPath))
		return kNsxErrorInvalidState;

	m_mutex.Lock();
	if (paf::LocalFile::Exists(destPath)) {
		int rmRet = paf::LocalFile::RemoveFile(destPath);
		if (rmRet != 0) {
			m_mutex.Unlock();
			return rmRet;
		}
	}

	int renRet = paf::LocalFile::RenameFile(tempPath, destPath);
	if (renRet != 0) {
		m_mutex.Unlock();
		return renRet;
	}

	updateFileTimestamps(destPath, ctimeTick, mtimeTick);
	m_mutex.Unlock();
	return 0;
}

/* 0x8122DF28 */
int NsxWhatsNewAccessorImpl::SyncImagesAndCommit(const paf::string &destXmlPath,
						 const paf::rtc::Tick *ctimeTick,
						 const paf::rtc::Tick *mtimeTick,
						 const paf::list<NsxImagePair> &neededImages,
						 const paf::string &npEnv)
{
	char envDirBuf[256];
	sce_paf_snprintf(envDirBuf, sizeof(envDirBuf), "ur0:shell/whats_new/%s", npEnv.c_str());
	paf::string envDir(envDirBuf);
	if (!paf::Dir::IsExist(envDir.c_str())) {
		int mkRet = paf::Dir::Create(envDir.c_str());
		if (mkRet != 0)
			return mkRet;
	}

	char imgDirBuf[256];
	sce_paf_snprintf(imgDirBuf, sizeof(imgDirBuf), "ur0:shell/whats_new/%s/img", npEnv.c_str());
	paf::string imgDir(imgDirBuf);
	if (!paf::Dir::IsExist(imgDir.c_str())) {
		int mkRet = paf::Dir::Create(imgDir.c_str());
		if (mkRet != 0)
			return mkRet;
	}

	paf::list<NsxImagePair> staleFiles;
	SceUID dirFd = sceIoDopen(imgDir.c_str());
	if (dirFd < 0)
		return dirFd;

	SceIoDirent dirent;
	for (;;) {
		int readRes = sceIoDread(dirFd, &dirent);
		if (readRes < 0) {
			sceIoDclose(dirFd);
			return readRes;
		}
		if (readRes == 0)
			break;

		if ((dirent.d_stat.st_mode & 0xF000) != 0x1000 &&
		    dirent.d_name[0] != '.' && dirent.d_name[0] != '\0') {
			NsxImagePair existing;
			existing.fileName = dirent.d_name;
			staleFiles.push_back(existing);
		}
	}
	sceIoDclose(dirFd);

	paf::list<NsxImagePair> toDownload;
	for (paf::list<NsxImagePair>::iterator needIter =
		     const_cast<paf::list<NsxImagePair> &>(neededImages).begin();
	     needIter != const_cast<paf::list<NsxImagePair> &>(neededImages).end();
	     ++needIter) {
		paf::list<NsxImagePair>::iterator matchIter = staleFiles.begin();
		while (matchIter != staleFiles.end()) {
			if (matchIter->fileName == needIter->fileName)
				break;
			++matchIter;
		}
		if (matchIter == staleFiles.end())
			toDownload.push_back(*needIter);
		else
			staleFiles.erase(matchIter);
	}

	for (paf::list<NsxImagePair>::iterator dlIter = toDownload.begin();
	     dlIter != toDownload.end(); ++dlIter) {
		paf::string destFile = imgDir + "/" + dlIter->fileName;
		int dlRet = downloadImageToFile(destFile, dlIter->sourceUrl);
		if (dlRet != 0)
			return dlRet;
	}

	int commitRet = ReplaceCacheXml(kWhatsNewTempXml, destXmlPath.c_str(), ctimeTick, mtimeTick);
	if (commitRet != 0)
		return commitRet;

	for (paf::list<NsxImagePair>::iterator rmIter = staleFiles.begin();
	     rmIter != staleFiles.end(); ++rmIter) {
		paf::string stalePath = imgDir + "/" + rmIter->fileName;
		paf::Dir::Remove(stalePath.c_str());
	}
	return 0;
}

/* 0x8122E982 */
int NsxWhatsNewAccessorImpl::StreamFilterHttpToTemp(paf::HttpFile *httpFile,
						    paf::common::SharedPtr<paf::File> outFile,
						    paf::list<NsxImagePair> *outImages)
{
	if (outFile.get() == NULL)
		return kNsxErrorInvalidState;

	paf::common::SharedPtr<paf::MallocBuffer> ioBuf =
		paf::MallocBuffer::Allocate(kWhatsNewChunkBytes);
	if (ioBuf.get() == NULL)
		return kNsxErrorNoMemory;

	NsxXmlNode rootNode;
	NsxXmlSaxParser saxParser;
	shellXmlSaxCreate(&saxParser, &rootNode, true, true);

	bool isFirstChunk = true;
	bool reachedEnd = false;
	int status = 0;

	while (!reachedEnd) {
		if (m_canceled) {
			status = kNsxErrorCanceled;
			break;
		}

		int readBytes = httpFile->Read(ioBuf->GetAddr(), ioBuf->GetSize());
		if (readBytes < 0 || (isFirstChunk && readBytes == 0)) {
			status = kNsxErrorParseFailed;
			break;
		}

		reachedEnd = ((SceSize)readBytes < ioBuf->GetSize());
		if (shellXmlSaxParse(&saxParser, (const char *)ioBuf->GetAddr(),
				     (SceSize)readBytes, reachedEnd) != 0) {
			status = kNsxErrorParseFailed;
			break;
		}

		if (isFirstChunk) {
			isFirstChunk = false;
			if (!writeWhatsNewXmlHeader(outFile, rootNode.name)) {
				status = kNsxErrorParseFailed;
				break;
			}
		}

		if (!processWhatsNewSpaces(outFile, &rootNode, outImages, reachedEnd)) {
			status = kNsxErrorParseFailed;
			break;
		}
	}

	if (status == 0 && !writeWhatsNewXmlFooter(outFile))
		status = kNsxErrorParseFailed;

	shellXmlSaxDestroy(&saxParser);
	return status;
}

/* 0x8122FCB8 */
int NsxWhatsNewAccessorImpl::RewriteHttpToTempFile(paf::HttpFile *httpFile,
						   paf::list<NsxImagePair> *outImages)
{
	if (!httpFile->IsOpened())
		return kNsxErrorInvalidState;

	if (!paf::Dir::IsExist(kWhatsNewTempDir)) {
		int mkRet = paf::Dir::Create(kWhatsNewTempDir);
		if (mkRet != 0)
			return mkRet;
	}

	int openErr = 0;
	paf::common::SharedPtr<paf::File> tempFile =
		paf::LocalFile::Open(kWhatsNewTempXml, 0x10602U, 0x06U, &openErr);
	if (openErr != 0)
		return openErr;

	int filterRet = StreamFilterHttpToTemp(httpFile, tempFile, outImages);
	tempFile->Close();
	return filterRet;
}

/* 0x81230644 */
int NsxWhatsNewAccessorImpl::RunUpdate(bool forceRefresh)
{
	paf::Dir::RemoveRecursive(kWhatsNewTempDir);
	paf::rtc::Tick nowTick = nsxGetNetworkTick();
	if (nowTick == 0)
		return kNsxErrorNoNetworkTime;

	paf::string npEnv = SceLsdb_C2B7D81B();
	char pathBuf[256];
	sce_paf_snprintf(pathBuf, sizeof(pathBuf), "ur0:shell/whats_new/%s", npEnv.c_str());
	paf::string xmlPath(pathBuf);
	xmlPath += "/whatsnew.xml";

	if (!forceRefresh) {
		paf::rtc::Tick cachedMtime = getLocalFileMtimeTick(xmlPath.c_str());
		if (cachedMtime != 0) {
			paf::rtc::Tick expiryTick = cachedMtime;
			paf::rtc::TickAddHours(&expiryTick, &expiryTick, kWhatsNewCacheHours);
			if (nowTick < expiryTick)
				return kNsxErrorNotExpired;
		}
	}

	SceUInt64 ifModifiedTick = getLocalFileCtimeTick(xmlPath.c_str());
	paf::HttpFile httpFile;

	paf::string country = SceLsdb_88FDC55C(1);
	for (size_t index = 0; index < country.size(); ++index)
		country[index] = (char)sce_paf_toupper((unsigned char)country[index]);

	paf::string querySuffix;
	int hasAccount = 0;
	if (shellNpHasAccountId(&hasAccount) == 0 && hasAccount != 0)
		querySuffix = buildSidQueryString();

	const char *modelCode = (paf::system::Model::GetType() == 1) ? "DC" : "VT";
	char urlBuf[300];
	sce_paf_snprintf(urlBuf, sizeof(urlBuf), kWhatsNewUrlFormat,
			 npEnv.c_str(), modelCode, country.c_str(), querySuffix.c_str());

	int openRet = nsxHttpOpenIfModified(&httpFile, urlBuf, &ifModifiedTick);
	paf::rtc::Tick serverLastMod = 0;
	int updateOutcome = kWhatsNewUpdateDownloaded;
	if (openRet != 0) {
		SceUInt16 responseCode = 0;
		httpFile.GetResponseCode(responseCode);
		updateOutcome = (responseCode == 0) ? kNsxErrorHttpFailed : kWhatsNewUpdateNotModified;
	} else {
		serverLastMod = nsxHttpGetLastModifiedTick(&httpFile);
	}

	if (updateOutcome == kWhatsNewUpdateDownloaded) {
		paf::list<NsxImagePair> imageList;
		int rewriteRet = RewriteHttpToTempFile(&httpFile, &imageList);
		if (rewriteRet != 0) {
			recordWhatsNewUsageCounter(false);
			return rewriteRet;
		}
		recordWhatsNewUsageCounter(true);

		int syncRet = SyncImagesAndCommit(xmlPath, &serverLastMod, &nowTick, imageList, npEnv);
		if (syncRet != 0)
			return syncRet;
		return kWhatsNewUpdateDownloaded;
	}

	if (updateOutcome == kWhatsNewUpdateNotModified) {
		m_mutex.Lock();
		updateFileTimestamps(xmlPath.c_str(), NULL, &nowTick);
		m_mutex.Unlock();
		return kWhatsNewUpdateNotModified;
	}

	return updateOutcome;
}

/* Handle wrapper methods matching 0x8122CD08..0x8122CD90 (struct declared in nsx_internal.h) */

/* 0x8122CD08 */
int NsxWhatsNewAccessor::Terminate()
{
	if (impl != NULL) {
		delete impl;
		impl = NULL;
	}
	return 0;
}

/* 0x8122CD22 */
NsxWhatsNewAccessor::~NsxWhatsNewAccessor()
{
	if (impl != NULL) {
		delete impl;
		impl = NULL;
	}
}

/* 0x8122CD3E */
int NsxWhatsNewAccessor::Init(void (*callback)(void *), void *callbackArg)
{
	if (impl != NULL)
		return kNsxErrorInvalidState;
	impl = new NsxWhatsNewAccessorImpl(callback, callbackArg);
	return 0;
}

/* 0x8122CD7A */
int NsxWhatsNewAccessor::LoadMaterials(paf::vector<NsxMaterialItem *> *outMaterials,
				       paf::rtc::Tick *outNextRefreshTick)
{
	if (impl == NULL)
		return kNsxErrorInvalidState;
	return impl->LoadMaterials(outMaterials, outNextRefreshTick);
}

/* 0x8122CD90 */
int NsxWhatsNewAccessor::RequestUpdate(bool forceRefresh)
{
	if (impl == NULL)
		return kNsxErrorInvalidState;
	return impl->EnqueueUpdate(forceRefresh);
}

/* 0x81230CD4 */
int nsxMaterialGetOperatorId(const NsxMaterialItem *material)
{
	return nsxGetAnnotationInt(material->annotations, paf::string(kNsxAnnoOperator), 0);
}

/* 0x81230CE8 */
bool nsxMaterialIsEmergency(const NsxMaterialItem *material)
{
	paf::string value = nsxGetAnnotationString(
		material->annotations, paf::string(kNsxAnnoEmergency), paf::string());
	return value == kNsxAnnoEnabledValue;
}

/* 0x81230D7E */
bool nsxMaterialIsPicks(const NsxMaterialItem *material)
{
	paf::string value = nsxGetAnnotationString(
		material->annotations, paf::string(kNsxAnnoPicks), paf::string());
	return value == kNsxAnnoEnabledValue;
}
