/*
 * sysupchk_np.cpp: NP system update list checker (SceSystemUpdateCheckNp) and
 * XML SAX handler for update_data_list / region / np (0x8136408C..0x81365413).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <thread/thread.h>
#include <file/file.h>
#include <file/httpfile.h>
#include <datetime/datetime.h>
#include <bXCe/bxce_internal.h>
#include <system_update/system_update_registry.h>
#include <system_update/system_update_version.h>
#include <system_update/system_update_mtphttpfile.h>

#define SYSUPCHK_NP_ERR_NO_MEMORY	-2145902591	/* 0x80182001 */
#define SYSUPCHK_NP_ERR_INVALID_ARG	-2145902587	/* 0x80182005 */
#define SYSUPCHK_NP_ERR_INVALID_STATE	-2145902585	/* 0x80182007 */
#define SYSUPCHK_NP_ERR_BUSY		-2145902584	/* 0x80182008 */
#define SYSUPCHK_NP_ERR_CANCELED	-2145902583	/* 0x80182009 */
#define SYSUPCHK_NP_ERR_NOT_FOUND	-2145902582	/* 0x8018200A */
#define SYSUPCHK_NP_ERR_XML_BAD_ROOT	-2145902576	/* 0x80182010 */
#define SYSUPCHK_NP_ERR_XML_UNEXPECTED	-2145902575	/* 0x80182011 */
#define SYSUPCHK_NP_ERR_XML_NESTING	-2145902574	/* 0x80182012 */
#define SYSUPCHK_NP_ERR_XML_BAD_VERSION	-2145902573	/* 0x80182013 */
#define SYSUPCHK_NP_ERR_XML_NO_REGION	-2145902572	/* 0x80182014 */

#define SYSUPCHK_NP_CACHE_LIFETIME_USEC	86400000000ULL
#define SYSUPCHK_NP_READ_BUF_SIZE	0x2000U
#define SYSUPCHK_NP_LEVEL_COUNT		4

#ifdef FW_VERSION
#define SYSUPCHK_NP_CURRENT_FW_VERSION	((SceUInt32)(FW_VERSION))
#else
#define SYSUPCHK_NP_CURRENT_FW_VERSION	0x03650011U
#endif

extern "C" {
extern int vshSblAimgrIsCEX(void);
extern int vshSblAimgrIsDolce(void);
extern int vshSblUtMgrHasComTestFlag(void);
extern int sceRegMgrGetKeyInt(const char *category, const char *name, int *outVal);
}

enum SysupchkXmlState {
	XML_STATE_INIT		= 0,
	XML_STATE_DOC_STARTED	= 1,
	XML_STATE_UPDATE_LIST	= 2,
	XML_STATE_REGION	= 3,
	XML_STATE_NP_NODE	= 4,
	XML_STATE_SKIP		= 5
};

struct SysupchkVersionAttrSpec {
	const char *attrName;
	int attrLen;
	int levelIndex;
};

/* 0x8151FADC: XML attributes parsed on <np> / <np_d> */
static const SysupchkVersionAttrSpec s_npVersionAttrs[SYSUPCHK_NP_LEVEL_COUNT] = {
	{ "level0_system_version", 21, 1 },
	{ "level1_system_version", 21, 2 },
	{ "level2_system_version", 21, 3 },
	{ "map",                    3, 0 },
};

/* 0x81547558 / 0x8154755C: target XML element name ("np" on Vita, "np_d" on Dolce) */
static const char *s_npElementTag = "np";
static int s_npElementTagLen = 2;

/* 0x81547548 / 0x815C6690: cached update-required results and timestamp */
static int s_cachedUpdateRequired[SYSUPCHK_NP_LEVEL_COUNT];
static SceUInt64 s_cacheTimestampTick = 0;

/* 0x81364F24 */
static bool sysupchkNpIsCacheValid(void)
{
	SceUInt64 nowTick = 0;
	if (paf::rtc::GetCurrentTick(&nowTick) < 0)
		return false;
	return (s_cacheTimestampTick + SYSUPCHK_NP_CACHE_LIFETIME_USEC) > nowTick;
}

/* 0x81364F8A */
static int sysupchkNpReadCache(int checkLevel, int *outStatus)
{
	if (s_cacheTimestampTick == 0)
		return SYSUPCHK_NP_ERR_INVALID_STATE;
	*outStatus = s_cachedUpdateRequired[checkLevel];
	return 0;
}

/* SAX document handler parsing update_data_list XML (0x81364FC0..0x81365413) */
class SysupchkNpXmlHandler : public bXCe::Sax::DocumentHandler {
public:
	explicit SysupchkNpXmlHandler(int productCode);
	virtual ~SysupchkNpXmlHandler() {}

	virtual int startDocument(void *userData);
	virtual int startElement(void *userData, const char *name, int length,
				 const bXCe::AttributeList *attrList);
	virtual int endElement(void *userData, const char *name, int length);
	virtual int endDocument(void *userData);

	int GetError() const { return m_error; }
	SceUInt32 GetLevelVersion(int index) const { return m_levelVersions[index]; }

private:
	int ParseNpAttributes(const bXCe::AttributeList *attrList);
	bool MatchesConsoleRegion(const char *name, int length,
				  const bXCe::AttributeList *attrList);

	int m_productCode;
	bool m_foundRegion;
	int m_error;
	int m_state;
	int m_skipReturnState;
	int m_skipDepth;
	SceUInt32 m_levelVersions[SYSUPCHK_NP_LEVEL_COUNT];
};

/* 0x81364FC0 */
SysupchkNpXmlHandler::SysupchkNpXmlHandler(int productCode)
	: m_productCode(productCode),
	  m_foundRegion(false),
	  m_error(0),
	  m_state(XML_STATE_INIT),
	  m_skipReturnState(XML_STATE_SKIP),
	  m_skipDepth(0)
{
	for (int i = 0; i < SYSUPCHK_NP_LEVEL_COUNT; ++i)
		m_levelVersions[i] = 0;
	if (vshSblAimgrIsDolce() > 0) {
		s_npElementTag = "np_d";
		s_npElementTagLen = 4;
	}
}

/* 0x81365014 */
int SysupchkNpXmlHandler::startDocument(void *userData)
{
	(void)userData;
	if (m_state != XML_STATE_INIT) {
		m_error = SYSUPCHK_NP_ERR_XML_BAD_ROOT;
		return m_error;
	}
	m_state = XML_STATE_DOC_STARTED;
	return 0;
}

/* 0x8136502E */
int SysupchkNpXmlHandler::ParseNpAttributes(const bXCe::AttributeList *attrList)
{
	for (int i = 0; i < SYSUPCHK_NP_LEVEL_COUNT; ++i) {
		const bXCe::Attr *attr =
			(const bXCe::Attr *)attrList->getAttribute(s_npVersionAttrs[i].attrName,
								   s_npVersionAttrs[i].attrLen);
		if (attr != NULL) {
			const char *valStr = attr->getValue();
			paf::string verText(valStr != NULL ? valStr : "");
			int parsedVer = Bgdl_81135E8C(verText);
			m_levelVersions[i] = (SceUInt32)parsedVer;
			if (parsedVer == -1) {
				m_error = SYSUPCHK_NP_ERR_XML_BAD_VERSION;
				return m_error;
			}
		} else {
			m_levelVersions[i] = 0;
		}
	}
	m_foundRegion = true;
	return 0;
}

/* 0x81365116 */
bool SysupchkNpXmlHandler::MatchesConsoleRegion(const char *name, int length,
						const bXCe::AttributeList *attrList)
{
	if (length != 6 || sce_paf_memcmp(name, "region", 6) != 0)
		return false;

	const bXCe::Attr *idAttr = (const bXCe::Attr *)attrList->getAttribute("id", 2);
	if (idAttr == NULL)
		return false;

	paf::string consoleRegion;
	if (Bgdl_811361D4(&consoleRegion) < 0)
		return false;

	const char *attrVal = idAttr->getValue();
	return sce_paf_memcmp(consoleRegion.c_str(), attrVal, consoleRegion.size() + 1) == 0;
}

/* 0x813651E8 */
int SysupchkNpXmlHandler::startElement(void *userData, const char *name, int length,
				       const bXCe::AttributeList *attrList)
{
	(void)userData;
	switch (m_state) {
	case XML_STATE_DOC_STARTED:
		if (length == 16 && sce_paf_memcmp(name, "update_data_list", 16) == 0) {
			m_state = XML_STATE_UPDATE_LIST;
			return m_error;
		}
		m_error = SYSUPCHK_NP_ERR_XML_UNEXPECTED;
		return m_error;

	case XML_STATE_UPDATE_LIST:
		if (MatchesConsoleRegion(name, length, attrList)) {
			m_state = XML_STATE_REGION;
		} else {
			m_state = XML_STATE_SKIP;
			m_skipReturnState = XML_STATE_UPDATE_LIST;
		}
		return m_error;

	case XML_STATE_REGION:
		if (length == s_npElementTagLen &&
		    sce_paf_memcmp(name, s_npElementTag, (size_t)s_npElementTagLen) == 0) {
			m_state = XML_STATE_NP_NODE;
			m_error = ParseNpAttributes(attrList);
		} else {
			m_state = XML_STATE_SKIP;
			m_skipReturnState = XML_STATE_REGION;
		}
		return m_error;

	case XML_STATE_NP_NODE:
		m_error = SYSUPCHK_NP_ERR_XML_NESTING;
		return m_error;

	case XML_STATE_SKIP:
		++m_skipDepth;
		return m_error;

	default:
		m_error = SYSUPCHK_NP_ERR_INVALID_STATE;
		return m_error;
	}
}

/* 0x813652BA */
int SysupchkNpXmlHandler::endElement(void *userData, const char *name, int length)
{
	(void)userData;
	switch (m_state) {
	case XML_STATE_DOC_STARTED:
		m_error = SYSUPCHK_NP_ERR_XML_UNEXPECTED;
		return m_error;

	case XML_STATE_UPDATE_LIST:
		if (length == 16 && sce_paf_memcmp(name, "update_data_list", 16) == 0) {
			m_state = XML_STATE_DOC_STARTED;
			return m_error;
		}
		m_error = SYSUPCHK_NP_ERR_XML_BAD_ROOT;
		return m_error;

	case XML_STATE_REGION:
		if (length == 6 && sce_paf_memcmp(name, "region", 6) == 0) {
			m_state = XML_STATE_UPDATE_LIST;
			return m_error;
		}
		m_error = SYSUPCHK_NP_ERR_XML_BAD_ROOT;
		return m_error;

	case XML_STATE_NP_NODE:
		if (length == s_npElementTagLen &&
		    sce_paf_memcmp(name, s_npElementTag, (size_t)s_npElementTagLen) == 0) {
			m_state = XML_STATE_REGION;
			return m_error;
		}
		m_error = SYSUPCHK_NP_ERR_XML_BAD_ROOT;
		return m_error;

	case XML_STATE_SKIP:
		if (m_skipDepth != 0) {
			--m_skipDepth;
		} else {
			m_state = m_skipReturnState;
			m_skipReturnState = XML_STATE_SKIP;
		}
		return m_error;

	default:
		m_error = SYSUPCHK_NP_ERR_INVALID_STATE;
		return m_error;
	}
}

/* 0x81365394 */
int SysupchkNpXmlHandler::endDocument(void *userData)
{
	(void)userData;
	if (m_foundRegion)
		return 0;

	paf::string consoleRegion;
	Bgdl_811361D4(&consoleRegion);
	m_error = SYSUPCHK_NP_ERR_XML_NO_REGION;
	return m_error;
}

/* Per-request NP update-list download & parse session (28 bytes, 0x813644E8..0x81364F23) */
class SysupchkNpRequest {
public:
	SysupchkNpRequest();
	~SysupchkNpRequest();

	void Cleanup();
	int RunCheck(int checkLevel, int downloadOpt, int *outNeedsUpdate);
	int Abort();

	void MarkPendingDelete() { m_pendingDelete = true; }
	bool IsPendingDelete() const { return m_pendingDelete; }

private:
	int OpenHttpStream(const char *url);
	int OpenMtpHttpStream(const char *url);
	int OpenUpdateListStream();
	int EnsureParserReady();
	int DownloadAndParse(int checkLevel, int downloadOpt, int *outNeedsUpdate);

	SysupchkNpXmlHandler *m_xmlHandler;
	bXCe::Sax::Parser *m_xmlParser;
	paf::common::SharedPtr<paf::File> m_file;
	char *m_readBuffer;
	bool m_inProgress;
	bool m_pendingDelete;
	bool m_aborted;
	int m_downloadOpt;
};

/* 0x813644E8 */
SysupchkNpRequest::SysupchkNpRequest()
	: m_xmlHandler(NULL),
	  m_xmlParser(NULL),
	  m_readBuffer(NULL),
	  m_inProgress(false),
	  m_pendingDelete(false),
	  m_aborted(false),
	  m_downloadOpt(3)
{
}

/* 0x81364592 */
SysupchkNpRequest::~SysupchkNpRequest()
{
	Cleanup();
}

/* 0x81364502 */
void SysupchkNpRequest::Cleanup()
{
	if (m_xmlParser != NULL) {
		delete m_xmlParser;
		m_xmlParser = NULL;
	}
	if (m_xmlHandler != NULL) {
		delete m_xmlHandler;
		m_xmlHandler = NULL;
	}
	if (m_file.get() != NULL) {
		m_file->Close();
		m_file.reset();
	}
	if (m_readBuffer != NULL) {
		delete[] m_readBuffer;
		m_readBuffer = NULL;
	}
}

/* 0x813645F0 */
int SysupchkNpRequest::OpenHttpStream(const char *url)
{
	paf::HttpFile::OpenArg openArg;
	int err = openArg.ParseUrl(url);
	if (err < 0)
		return err;

	paf::HttpFile *httpFile = new paf::HttpFile();
	if (httpFile == NULL)
		return SYSUPCHK_NP_ERR_NO_MEMORY;

	m_file = paf::common::SharedPtr<paf::File>(httpFile);
	err = m_file->Open(&openArg);
	return (err >= 0) ? 0 : err;
}

/* 0x81364A7C */
int SysupchkNpRequest::OpenMtpHttpStream(const char *url)
{
	MtpHttpFileOpenArg openArg;
	sce_paf_memset(&openArg, 0, sizeof(openArg));
	openArg.file_type = MTP_HTTP_FILE_TYPE;
	openArg.kind = 0;
	openArg.url = url;
	openArg.bufferSize = 0x4000;
	openArg.options[0] = 0x10000100U;

	MtpHttpFile *mtpFile = new MtpHttpFile();
	if (mtpFile == NULL)
		return SYSUPCHK_NP_ERR_NO_MEMORY;

	m_file = paf::common::SharedPtr<paf::File>(mtpFile);
	int err = m_file->Open(&openArg);
	return (err >= 0) ? 0 : err;
}

/* 0x81364BFE */
int SysupchkNpRequest::OpenUpdateListStream()
{
	if (m_file.get() != NULL) {
		m_file->Close();
		m_file.reset();
	}

	paf::string url;
	SystemUpdateResult urlRes = Bgdl_81132EDA(0, &url);
	if (urlRes.error < 0)
		return urlRes.error;

	if (m_downloadOpt == 4)
		return OpenMtpHttpStream(url.c_str());
	return OpenHttpStream(url.c_str());
}

/* 0x81364CEE */
int SysupchkNpRequest::EnsureParserReady()
{
	if (m_xmlParser != NULL && m_xmlHandler != NULL)
		return 0;

	int productCode = 0;
	int err = Bgdl_811361A0(&productCode);
	if (err < 0)
		return err;

	m_xmlHandler = new SysupchkNpXmlHandler(productCode);
	if (m_xmlHandler == NULL)
		return SYSUPCHK_NP_ERR_NO_MEMORY;

	m_xmlParser = new bXCe::Sax::Parser();
	if (m_xmlParser == NULL)
		return SYSUPCHK_NP_ERR_NO_MEMORY;

	err = m_xmlParser->initialize(NULL);
	if (err != 0)
		return err;

	m_xmlParser->setSkipIgnorableWhiteSpace(true);
	m_xmlParser->setDocumentHandler(m_xmlHandler);
	return 0;
}

/* 0x81364D9C */
int SysupchkNpRequest::DownloadAndParse(int checkLevel, int downloadOpt, int *outNeedsUpdate)
{
	m_downloadOpt = downloadOpt;
	if (m_readBuffer == NULL) {
		m_readBuffer = new char[SYSUPCHK_NP_READ_BUF_SIZE];
		if (m_readBuffer == NULL)
			return SYSUPCHK_NP_ERR_NO_MEMORY;
	}

	int err = EnsureParserReady();
	if (err == 0)
		err = OpenUpdateListStream();
	if (err != 0)
		return err;

	bool isFinalChunk = false;
	for (;;) {
		if (m_aborted) {
			err = SYSUPCHK_NP_ERR_CANCELED;
			break;
		}
		int32_t bytesRead = m_file->Read(m_readBuffer, SYSUPCHK_NP_READ_BUF_SIZE);
		if (bytesRead < 0) {
			err = bytesRead;
			break;
		}
		if (bytesRead == 0)
			isFinalChunk = true;
		err = m_xmlParser->parse(m_readBuffer, bytesRead, isFinalChunk);
		if (isFinalChunk)
			break;
	}

	m_file->Close();

	if (err != 0) {
		if (m_xmlHandler->GetError() != 0)
			return m_xmlHandler->GetError();
		return err;
	}

	*outNeedsUpdate = (m_xmlHandler->GetLevelVersion(checkLevel) >
			   SYSUPCHK_NP_CURRENT_FW_VERSION) ? 1 : 0;
	for (int i = 0; i < SYSUPCHK_NP_LEVEL_COUNT; ++i) {
		s_cachedUpdateRequired[i] =
			(m_xmlHandler->GetLevelVersion(i) >
			 SYSUPCHK_NP_CURRENT_FW_VERSION) ? 1 : 0;
	}

	SceUInt64 nowTick = 0;
	if (paf::rtc::GetCurrentTick(&nowTick) >= 0)
		s_cacheTimestampTick = nowTick;

	return 0;
}

/* 0x81364EC8 */
int SysupchkNpRequest::RunCheck(int checkLevel, int downloadOpt, int *outNeedsUpdate)
{
	if (m_inProgress)
		return SYSUPCHK_NP_ERR_BUSY;
	if (m_aborted)
		return SYSUPCHK_NP_ERR_CANCELED;

	m_inProgress = true;
	int err = DownloadAndParse(checkLevel, downloadOpt, outNeedsUpdate);
	m_inProgress = false;
	Cleanup();
	return err;
}

/* 0x81364F02 */
int SysupchkNpRequest::Abort()
{
	m_aborted = true;
	if (!m_inProgress)
		return SYSUPCHK_NP_ERR_INVALID_STATE;
	if (m_file.get() != NULL)
		m_file->Abort();
	return 0;
}

/* Global request registries guarded by SceSystemUpdateCheckNp RMutex (0x81364456) */
static paf::vector<SysupchkNpRequest *> s_activeNpRequests;
static paf::vector<SysupchkNpRequest *> s_runningNpRequests;
static paf::thread::RMutex s_sysupchkNpMutex("SceSystemUpdateCheckNp", 0);

/* 0x8136412C: remove a request pointer from one of the tracked vectors */
static int sysupchkNpRemoveFromList(SysupchkNpRequest *req,
				    paf::vector<SysupchkNpRequest *> &list)
{
	s_sysupchkNpMutex.Lock();
	for (paf::vector<SysupchkNpRequest *>::iterator it = list.begin();
	     it != list.end(); ++it) {
		if (*it == req) {
			list.erase(it);
			s_sysupchkNpMutex.Unlock();
			return 0;
		}
	}
	s_sysupchkNpMutex.Unlock();
	return SYSUPCHK_NP_ERR_NOT_FOUND;
}

static bool sysupchkNpContains(SysupchkNpRequest *req,
			       paf::vector<SysupchkNpRequest *> &list)
{
	for (paf::vector<SysupchkNpRequest *>::iterator it = list.begin();
	     it != list.end(); ++it) {
		if (*it == req)
			return true;
	}
	return false;
}

/* 0x8136408C */
int npSysUpdateCreateRequest(int *outReqHandle)
{
	if (outReqHandle == NULL)
		return SYSUPCHK_NP_ERR_INVALID_ARG;

	SysupchkNpRequest *req = new SysupchkNpRequest();
	*outReqHandle = (int)(intptr_t)req;
	if (req == NULL)
		return SYSUPCHK_NP_ERR_NO_MEMORY;

	s_sysupchkNpMutex.Lock();
	s_activeNpRequests.push_back(req);
	s_sysupchkNpMutex.Unlock();
	return 0;
}

/* 0x813641AC */
int npSysUpdateDestroyRequest(int reqHandle)
{
	if (reqHandle == 0)
		return SYSUPCHK_NP_ERR_INVALID_ARG;

	SysupchkNpRequest *req = (SysupchkNpRequest *)(intptr_t)reqHandle;
	int err = sysupchkNpRemoveFromList(req, s_activeNpRequests);
	if (err != 0)
		return err;

	s_sysupchkNpMutex.Lock();
	req->Abort();
	if (sysupchkNpContains(req, s_runningNpRequests))
		req->MarkPendingDelete();
	else
		delete req;
	s_sysupchkNpMutex.Unlock();
	return 0;
}

/* 0x8136422C */
int npSysUpdateCheck(int reqHandle, int checkType, int *outStatus,
		     int checkMode, int checkFlag)
{
	if (reqHandle == 0 || outStatus == NULL)
		return SYSUPCHK_NP_ERR_INVALID_ARG;

	if (vshSblAimgrIsCEX() == 1) {
		if (vshSblUtMgrHasComTestFlag() == 1) {
			*outStatus = 0;
			return 0;
		}
	} else {
		int enableToolCheck = 0;
		if (sceRegMgrGetKeyInt("/CONFIG/SYSTEM/UPDATE",
				       "enable_tool_pup_check",
				       &enableToolCheck) != 0 ||
		    enableToolCheck == 0) {
			*outStatus = 0;
			return 0;
		}
	}

	if (checkMode < 0 || checkMode > 2)
		return SYSUPCHK_NP_ERR_INVALID_ARG;

	if (checkMode == 0 && sysupchkNpIsCacheValid())
		return sysupchkNpReadCache(checkType, outStatus);

	if (checkMode == 2) {
		if (sysupchkNpIsCacheValid())
			return sysupchkNpReadCache(checkType, outStatus);
		*outStatus = -2;
		return 0;
	}

	SysupchkNpRequest *req = (SysupchkNpRequest *)(intptr_t)reqHandle;
	s_sysupchkNpMutex.Lock();
	if (!sysupchkNpContains(req, s_activeNpRequests)) {
		s_sysupchkNpMutex.Unlock();
		return SYSUPCHK_NP_ERR_INVALID_ARG;
	}
	s_runningNpRequests.push_back(req);
	s_sysupchkNpMutex.Unlock();

	int err = req->RunCheck(checkType, checkFlag, outStatus);

	s_sysupchkNpMutex.Lock();
	sysupchkNpRemoveFromList(req, s_runningNpRequests);
	if (req->IsPendingDelete())
		delete req;
	s_sysupchkNpMutex.Unlock();
	return err;
}

/* 0x813643F4 */
int npSysUpdateAbortRequest(int reqHandle)
{
	if (reqHandle == 0)
		return SYSUPCHK_NP_ERR_INVALID_ARG;

	SysupchkNpRequest *req = (SysupchkNpRequest *)(intptr_t)reqHandle;
	s_sysupchkNpMutex.Lock();
	if (!sysupchkNpContains(req, s_activeNpRequests)) {
		s_sysupchkNpMutex.Unlock();
		return SYSUPCHK_NP_ERR_INVALID_ARG;
	}
	int err = req->Abort();
	s_sysupchkNpMutex.Unlock();
	return err;
}
