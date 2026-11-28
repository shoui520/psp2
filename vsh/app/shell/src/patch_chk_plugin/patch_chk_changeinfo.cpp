/*
 * patch_chk_changeinfo.cpp: downloads and parses title patch changeinfo XML
 * over HTTP using paf::HttpFile and bXCe::Sax::Parser, formats version entries
 * into HTML, and provides the shared merge-sort helper changeInfoSortEntries
 * (0x812AFF04-0x812B1584).
 */
#include <stddef.h>
#include <scetypes.h>
#include <bXCe/bxce_internal.h>
#include <file/httpfile.h>
#include <memory/heap_allocator.h>
#include <memory/pool_allocator.h>
#include <plugin.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "../../../game/game_plugin/game_plugin.h"
#include "../../../../paf/include/rtc/rtc.h"
#include "../applauncher_plugin/src/applauncher_changeinfo.h"
#include "src/livespace_db/lsdb_keys.h"
#include "patch_chk_internal.h"

using namespace paf;

extern "C" paf::memory::HeapAllocator *GetGlobalHeapAllocator(void);


#define CHANGEINFO_MAX_DOWNLOAD_SIZE	0x10000
#define CHANGEINFO_ERROR_EMPTY_FILE	(-2135994334)	/* 0x80AF5022 */

/* Parses "XX.YY" / "X.YY" string into integer version (e.g. "01.02" -> 102) (0x812B12A8) */
static int parseChangeInfoVersion(const char *verStr, SceSize maxLen, SceUInt32 *outVersion)
{
	SceSize len = 0;
	if (verStr != NULL) {
		while (len < maxLen && verStr[len] != '\0')
			++len;
	}
	if (len > maxLen - 1)
		return PATCH_CHK_ERROR_VERSION_OVERFLOW;

	SceUInt32 result = 0;
	SceUInt32 place = 1;
	for (int i = (int)len - 1; i >= 0; --i) {
		char ch = verStr[i];
		if (ch != '.') {
			result += (SceUInt32)(ch - '0') * place;
			place *= 10;
		}
	}
	*outVersion = result;
	return 0;
}

/* Formats integer version into "X.YY" or "XX.YY" string (0x812B14D2) */
static int formatChangeInfoVersion(SceUInt32 versionNum, string *outStr)
{
	if (versionNum > 9999)
		return -1;

	char buf[8];
	int nullPos = (versionNum < 1000) ? 4 : 5;
	int dotPos = (versionNum < 1000) ? 1 : 2;
	int cur = nullPos - 1;
	buf[nullPos] = '\0';

	SceUInt32 rem = versionNum;
	while (cur >= 0) {
		if (cur == dotPos) {
			buf[cur] = '.';
		} else {
			buf[cur] = (char)((rem % 10) + '0');
			rem /= 10;
		}
		--cur;
	}
	outStr->assign(buf);
	return 0;
}

/* bXCe SAX handler extracting <changes app_ver="..."> CDATA bodies newer than currentAppVer (0x812B11DC) */
class PatchChkChangeInfoXmlHandler : public bXCe::Sax::DocumentHandler {
public:
	PatchChkChangeInfoXmlHandler(PatchChkChangeInfoReader *reader, const char *currentAppVer)
		: m_reader(reader),
		  m_currentAppVer(currentAppVer != NULL ? currentAppVer : ""),
		  m_attrParseResult(0),
		  m_entryVersion(0),
		  m_captureBody(false)
	{
	}

	virtual ~PatchChkChangeInfoXmlHandler() {}

	virtual int startElement(void *userData, const char *name, int length,
				 const bXCe::AttributeList *attrList)
	{
		(void)userData;
		(void)length;
		if (sce_paf_strncmp(name, "changes", 8) == 0) {
			SceUInt32 currentVer = 0;
			parseChangeInfoVersion(m_currentAppVer.c_str(),
					       m_currentAppVer.length() + 1, &currentVer);

			int attrCount = attrList->getLength();
			for (int i = 0; i < attrCount; ++i) {
				const bXCe::Attr *attr = (const bXCe::Attr *)attrList->getAttribute(i);
				if (sce_paf_strncmp(attr->getName(), "app_ver", 8) == 0) {
					SceUInt32 entryVer = 0;
					m_attrParseResult = parseChangeInfoVersion(
						attr->getValue(),
						(SceSize)(attr->getValueLength() + 1), &entryVer);
					m_entryVersion = entryVer;
					if (entryVer > currentVer)
						m_captureBody = true;
					return 0;
				}
			}
		}
		return 0;
	}

	virtual int endElement(void *userData, const char *name, int length)
	{
		(void)userData;
		(void)length;
		if (sce_paf_strncmp(name, "changes", 8) == 0) {
			m_attrParseResult = 0;
			m_entryVersion = 0;
			m_captureBody = false;
		}
		return 0;
	}

	virtual int skippedText(void *userData, int tokenType, const char *text, int length)
	{
		(void)userData;
		(void)tokenType;
		if (m_captureBody && m_attrParseResult >= 0 && m_entryVersion != 0) {
			string body;
			body.append(text, (size_t)length);
			size_t cdataEnd = body.rfind("]]>", string::npos, 3);
			if (cdataEnd != string::npos)
				body.erase(cdataEnd, 3);
			if (m_reader != NULL)
				m_reader->AddEntry(m_entryVersion, body);
		}
		return 0;
	}

private:
	PatchChkChangeInfoReader *m_reader;
	string m_currentAppVer;
	SceInt32 m_attrParseResult;
	SceUInt32 m_entryVersion;
	bool m_captureBody;
};

/*
 * Shared descending-version merge sort (0x812B046C).
 * Also called by applauncher_changeinfo.cpp (ApplauncherChangeInfoEntry shares
 * the leading SceUInt32 versionNumber layout with PatchChkChangeInfoEntry).
 */
void changeInfoSortEntries(void *owner, vector<ApplauncherChangeInfoEntry *> *entries,
			   int first, int last, ApplauncherChangeInfoEntry **work)
{
	(void)owner;
	if (last <= first)
		return;

	int mid = (first + last) / 2;
	changeInfoSortEntries(owner, entries, first, mid, work);
	changeInfoSortEntries(owner, entries, mid + 1, last, work);

	int left = first;
	int right = mid + 1;
	int out = first;
	while (left <= mid || right <= last) {
		if (right <= last &&
		    (left > mid || (*entries)[left]->version <= (*entries)[right]->version)) {
			work[out++] = (*entries)[right++];
		} else {
			work[out++] = (*entries)[left++];
		}
	}

	for (int i = first; i <= last; ++i)
		(*entries)[i] = work[i];
}

/* PatchChkChangeInfoReader (0x812B03C6-0x812B1169) */
PatchChkChangeInfoReader::PatchChkChangeInfoReader()
	: m_canceled(false)
{
}

/* Frees all collected PatchChkChangeInfoEntry pointers (0x812B03D2) */
PatchChkChangeInfoReader::~PatchChkChangeInfoReader()
{
	vector<PatchChkChangeInfoEntry *>::iterator it = m_entries.begin();
	while (it != m_entries.end()) {
		delete *it;
		it = m_entries.erase(it);
	}
}

/* Marks reader canceled (0x812B0462) */
void PatchChkChangeInfoReader::Cancel()
{
	if (!m_canceled)
		m_canceled = true;
}

/* Parses downloaded XML buffer and merge-sorts entries newest-first (0x812B053C) */
int PatchChkChangeInfoReader::ParseXml(const char *xmlBuffer, int xmlLength,
				       const char *currentAppVer)
{
	if (m_canceled)
		return 0;

	bXCe::Sax::Parser *parser = new bXCe::Sax::Parser();
	if (parser == NULL)
		return PATCH_CHK_ERROR_XML_INIT_FAILED;

	int ret = parser->initialize(NULL);
	if (ret == 0) {
		parser->setResolveEntity(false);
		parser->setSkipIgnorableWhiteSpace(true);

		PatchChkChangeInfoXmlHandler *handler =
			new PatchChkChangeInfoXmlHandler(this, currentAppVer);
		if (handler != NULL) {
			parser->setDocumentHandler(handler);
			ret = parser->parse(xmlBuffer, xmlLength, true);
			if (ret == 0 && !m_entries.empty()) {
				ApplauncherChangeInfoEntry **work =
					new ApplauncherChangeInfoEntry *[m_entries.size()];
				vector<ApplauncherChangeInfoEntry *> *entryVec =
					(vector<ApplauncherChangeInfoEntry *> *)&m_entries;
				changeInfoSortEntries(this, entryVec, 0,
						      (int)m_entries.size() - 1, work);
				delete[] work;
			}
			delete handler;
		} else {
			ret = PATCH_CHK_ERROR_XML_INIT_FAILED;
		}
	}

	parser->terminate();
	delete parser;
	return ret;
}

/* Downloads changeinfo XML via paf::HttpFile into game_plugin pool or heap (0x812B0608) */
int PatchChkChangeInfoReader::FetchAndParse(const char *changeInfoUrl, const char *currentAppVer)
{
	HttpFile::OpenArg openArg;
	int ret = openArg.ParseUrl(changeInfoUrl);
	if (ret < 0)
		return ret;

	common::SharedPtr<HttpFile> httpFile(new HttpFile());
	if (httpFile.get() == NULL)
		return PATCH_CHK_ERROR_NO_MEMORY;

	ret = httpFile->Open(&openArg);
	if (ret < 0)
		return ret;

	SceOff fileSize = httpFile->GetFileSize();
	if (fileSize > CHANGEINFO_MAX_DOWNLOAD_SIZE)
		fileSize = CHANGEINFO_MAX_DOWNLOAD_SIZE;

	Plugin *gamePlugin = Plugin::Find("game_plugin");
	common::SharedPtr<memory::PoolAllocator> pool;
	int result = 0;

	if (fileSize <= 0) {
		result = CHANGEINFO_ERROR_EMPTY_FILE;
	} else {
		GamePluginInterface *gameIface =
			(gamePlugin != NULL) ? (GamePluginInterface *)gamePlugin->GetInterface(1)
					     : NULL;
		void *poolBuf = NULL;
		if (gameIface != NULL) {
			typedef int (*GetPoolFunc)(const char *,
						   common::SharedPtr<memory::PoolAllocator> *);
			GetPoolFunc getPool =
				(GetPoolFunc)gameIface->func[GAME_PLUGIN_FUNC_GET_POOL];
			if (getPool("patch_chk_plugin", &pool) == 0 && pool.get() != NULL)
				poolBuf = pool->Alloc();
		}

		if (poolBuf != NULL) {
			if (!m_canceled) {
				int nread = httpFile->Read(poolBuf, (size_t)fileSize);
				httpFile->Close();
				if (nread >= 0)
					result = ParseXml((const char *)poolBuf, (int)fileSize,
							  currentAppVer);
				else
					result = nread;
			}
			pool->Free(poolBuf);
			typedef void (*ReleasePoolFunc)(common::SharedPtr<memory::PoolAllocator> *);
			ReleasePoolFunc releasePool =
				(ReleasePoolFunc)gameIface->func[GAME_PLUGIN_FUNC_RELEASE_POOL];
			releasePool(&pool);
		} else {
			memory::HeapAllocator *globalHeap = GetGlobalHeapAllocator();
			void *heapBuf = globalHeap->sce_paf_malloc((size_t)fileSize);
			if (heapBuf != NULL) {
				if (!m_canceled) {
					int nread = httpFile->Read(heapBuf, (size_t)fileSize);
					httpFile->Close();
					if (nread >= 0)
						result = ParseXml((const char *)heapBuf,
								  (int)fileSize, currentAppVer);
					else
						result = nread;
				}
				globalHeap->sce_paf_free(heapBuf);
			} else {
				void *rawBuf = sce_paf_malloc((size_t)fileSize);
				if (rawBuf != NULL) {
					if (!m_canceled) {
						int nread = httpFile->Read(rawBuf,
									   (size_t)fileSize);
						httpFile->Close();
						if (nread >= 0)
							result = ParseXml((const char *)rawBuf,
									  (int)fileSize,
									  currentAppVer);
						else
							result = nread;
					}
					sce_paf_free(rawBuf);
				} else {
					result = PATCH_CHK_ERROR_XML_INIT_FAILED;
				}
			}
		}
	}

	httpFile->Close();
	return result;
}

/* Appends a parsed version changelog entry (0x812B0E9A) */
void PatchChkChangeInfoReader::AddEntry(SceUInt32 versionNumber, const string &bodyText)
{
	if (m_canceled)
		return;

	PatchChkChangeInfoEntry *entry = new PatchChkChangeInfoEntry();
	if (entry != NULL) {
		entry->versionNumber = versionNumber;
		entry->bodyText = bodyText;
	}
	m_entries.push_back(entry);
}

/* Formats all sorted entries into <h1>version</h1>body HTML (0x812B0F0E) */
void PatchChkChangeInfoReader::AppendFormattedHtml(string *outHtml) const
{
	if (m_canceled)
		return;

	string verStr;
	for (size_t i = 0; i < m_entries.size(); ++i) {
		if (i > 0)
			outHtml->append("<br>");
		if (formatChangeInfoVersion(m_entries[i]->versionNumber, &verStr) >= 0) {
			outHtml->append("<h1>");
			outHtml->append(verStr);
			outHtml->append("</h1>");
			outHtml->append(m_entries[i]->bodyText);
		}
	}
}

/* PatchChkChangeInfoController (0x812AFF1C-0x812B0351) */
PatchChkChangeInfoController::PatchChkChangeInfoController()
	: m_reader(NULL),
	  m_callback(NULL),
	  m_userData(NULL),
	  m_stopped(false)
{
}

/* Destructor (0x812AFF50) */
PatchChkChangeInfoController::~PatchChkChangeInfoController()
{
	m_job.reset();
	if (m_reader != NULL) {
		delete m_reader;
		m_reader = NULL;
	}
}

/* Enqueues ChangeInfoJob on the Default job queue (0x812B0042) */
int PatchChkChangeInfoController::Start(DoneCallback callback, const char *currentAppVer,
					const string &changeInfoUrl, SceUInt64 totalSize,
					void *userData)
{
	(void)totalSize;
	m_callback = callback;
	m_changeInfoUrl = changeInfoUrl;
	m_currentAppVer = (currentAppVer != NULL) ? currentAppVer : "";
	m_userData = userData;

	m_reader = new PatchChkChangeInfoReader();
	if (m_reader == NULL)
		return PATCH_CHK_ERROR_NO_MEMORY;

	PatchChkCallbackJob *rawJob = new PatchChkCallbackJob(
		"ChangeInfoJob", OnJobRun, OnJobFinish, NULL, this);
	m_job.reset(rawJob);
	if (m_job.get() == NULL)
		return PATCH_CHK_ERROR_NO_MEMORY;

	job::JobQueue *queue = PatchChkJobQueueMgr::GetInstance()->GetQueue(PATCH_CHK_QUEUE_DEFAULT);
	int ret = queue->Enqueue(m_job);
	if (ret < 0)
		m_job.reset();
	return ret;
}

/* Cancels in-progress job and reader (0x812B0268) */
void PatchChkChangeInfoController::Cancel()
{
	if (m_job.get() != NULL)
		m_job->Cancel();
	if (m_reader != NULL)
		m_reader->Cancel();
}

/* Stops controller permanently without firing completion callback (0x812B0280) */
void PatchChkChangeInfoController::StopAndMarkCanceled()
{
	if (!m_stopped) {
		m_stopped = true;
		if (m_job.get() != NULL)
			m_job->Cancel();
		if (m_reader != NULL)
			m_reader->Cancel();
	}
}

/* Appends formatted changelog HTML into outHtml (0x812B02A4) */
void PatchChkChangeInfoController::AppendFormattedHtml(string *outHtml) const
{
	if (m_reader != NULL)
		m_reader->AppendFormattedHtml(outHtml);
}

/* Worker thread run callback (0x812B02B0) */
int PatchChkChangeInfoController::OnJobRun(void *userData)
{
	PatchChkChangeInfoController *self = (PatchChkChangeInfoController *)userData;
	if (self == NULL || self->m_stopped)
		return 0;
	if (!self->m_changeInfoUrl.empty())
		return self->m_reader->FetchAndParse(self->m_changeInfoUrl.c_str(),
						     self->m_currentAppVer.c_str());
	return 0;
}

/* Main-thread finish callback (0x812B02DA) */
void PatchChkChangeInfoController::OnJobFinish(int result, void *userData)
{
	PatchChkChangeInfoController *self = (PatchChkChangeInfoController *)userData;
	if (self == NULL || self->m_stopped)
		return;

	self->m_job.reset();
	if (!self->m_stopped && self->m_callback != NULL)
		self->m_callback(result, self->m_userData);
}
