/*
 * SceShell livearea_plugin: ContentParserJob
 * 3.65-CEX: 0x811B6624 - 0x811B71E8 (sti 0x811B7176)
 */
#include <std/string.h>
#include "livearea_internal.h"

extern "C" {
extern LiParser *SceLsdb_800809E3(void *mem, const char *xmlPath, bool system, bool useDefault);
extern void SceLsdb_29B275BA(paf::string *titleId, const char *xmlPath);
extern LiParser *SceLsdb_4B153968(LiParser *parser);
extern int SceLsdb_60B535FD(int appAttr, int defaultVal);
extern int SceLsdb_1673AD48(void);
extern void SceLsdb_43495B68(LiParser *parser, const int *language, const char *country, int age);
extern int SceLsdb_BE72A404(const char *titleId);
extern bool SceLsdb_69B32F6C(int appAttr);
extern int SceLsdb_632ACD0F(LiParser *parser);
}

class ContentParserErrorDialogHandler : public CommonGuiDialogHandler {
public:
	ContentParserErrorDialogHandler(LiveAreaPageInner *inner) : m_inner(inner) {}
	virtual ~ContentParserErrorDialogHandler() {}
	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		(void)value;
		(void)button;
	}

	LiveAreaPageInner *m_inner;
};

class LiveAreaContentParserJob : public paf::job::JobItem {
public:
	LiveAreaContentParserJob(LiveAreaPageInner *inner, const char *xmlPath,
				 const char *contentDir, SceUInt8 useLsdb, SceUInt8 isDefault,
				 int reloadMask, SceUInt8 allowDialog);
	virtual ~LiveAreaContentParserJob();
	virtual int Run();
	virtual void Finish(int result);

	void CancelAndDetach();

private:
	paf::string m_titleId;					/* +0x1C */
	paf::string m_contentDir;				/* +0x28 */
	bool m_hasKind;						/* +0x34 */
	SceUInt8 m_useLsdb;					/* +0x35 */
	SceUInt8 m_allowDialog;					/* +0x36 */
	SceUInt8 m_isDefault;					/* +0x37 */
	int m_reloadMask;					/* +0x38 */
	paf::common::SharedPtr<LiTemplate> m_parsedTemplate;	/* +0x3C */
	LiParser *m_lsdbParser;					/* +0x44 */
	LiveAreaPageInner *m_inner;				/* +0x48 */
	paf::graph::SurfacePool *m_surfacePool;			/* +0x4C */
	paf::string m_templateName;				/* +0x50 */
	int m_appAttr;						/* +0x5C */
	int m_style;						/* +0x60 */
};

/* 0x811B6624 */
LiveAreaContentParserJob::LiveAreaContentParserJob(LiveAreaPageInner *inner, const char *xmlPath,
						   const char *contentDir, SceUInt8 useLsdb,
						   SceUInt8 isDefault, int reloadMask,
						   SceUInt8 allowDialog)
	: paf::job::JobItem("ContentParserJob", NULL),
	  m_titleId(inner->m_page->GetTitleId() ? inner->m_page->GetTitleId() : ""),
	  m_contentDir(contentDir ? contentDir : ""),
	  m_hasKind(inner->m_kind != 0),
	  m_useLsdb(useLsdb),
	  m_allowDialog(allowDialog),
	  m_isDefault(isDefault),
	  m_reloadMask(reloadMask),
	  m_lsdbParser(NULL),
	  m_inner(inner),
	  m_surfacePool(inner->m_page->m_surfacePool),
	  m_templateName(inner->m_page->m_templateName),
	  m_appAttr(inner->m_appAttr),
	  m_style(inner->m_style)
{
	if (sce_paf_strncmp(m_titleId.c_str(), "NPXS10998", sce_paf_strlen("NPXS10998")) == 0 ||
	    sce_paf_strncmp(m_titleId.c_str(), "NPXS10999", sce_paf_strlen("NPXS10999")) == 0) {
		m_useLsdb = 1;
	}
	if (isDefault != 0) {
		m_useLsdb = 0;
	} else if (m_useLsdb != 0) {
		void *mem = operator new(sizeof(LiParser));
		m_lsdbParser = mem ? SceLsdb_800809E3(mem, xmlPath, true, true) : NULL;
		SceLsdb_29B275BA(&m_titleId, xmlPath);
	}
}

/* 0x811B67AA */
int liveareaEnqueueContentParserJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
				    LiveAreaPageInner *inner, const char *xmlPath,
				    const char *contentDir, SceUInt8 useLsdb,
				    SceUInt8 isDefault, int reloadMask, SceUInt8 allowDialog)
{
	LiveAreaContentParserJob *job = new LiveAreaContentParserJob(
		inner, xmlPath, contentDir, useLsdb, isDefault, reloadMask, allowDialog);
	paf::common::SharedPtr<paf::job::JobItem> jobRef(job);
	*outJob = jobRef;

	paf::job::JobQueue *queue = g_liveareaMgr->m_topmenuIf->getJobQueue();
	return queue->Enqueue(*outJob);
}

/* 0x811B687A (deleting dtor 0x811B6942) */
LiveAreaContentParserJob::~LiveAreaContentParserJob()
{
	if (m_lsdbParser != NULL) {
		LiParser *parser = SceLsdb_4B153968(m_lsdbParser);
		operator delete(parser);
	}
}

/* 0x811B6952 */
int LiveAreaContentParserJob::Run()
{
	if (IsCanceled())
		return -1;

	if (m_useLsdb == 0) {
		int res = liveareaMgrGetCachedTemplate(&m_parsedTemplate, &m_titleId, m_isDefault);
		if (res == (int)0x80101005 && m_allowDialog != 0) {
			int needWait = 0;
			LsdbAppInfoPtr appInfo(new LsdbAppInfo());
			LiveAreaTopmenuInterface *topmenuIf = g_liveareaMgr->m_topmenuIf;
			LiveAreaGameInterface *gameIf = g_liveareaMgr->m_gameIf;

			if (topmenuIf->getAppInfo(&appInfo, m_titleId.c_str()) == 0)
				return (int)0x80108E00;

			gameIf->getStorageState(&appInfo, &needWait);
			if (needWait != 0) {
				g_shellMutex.Lock();
				CommonGuiLayoutDialogParam dlgParam;
				dlgParam.resourcePlugin = g_liveareaPlugin;
				dlgParam.priority = 40;
				const wchar_t *msg = g_liveareaPlugin->GetString(paf::IDParam(1663788851U));
				dlgParam.text = msg ? msg : L"";
				int dlgId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
				gameIf->setDialogHold(1);
				topmenuIf->beginGameHold();
				g_shellMutex.Unlock();

				int installRes = liveareaIfInstallContent(&appInfo, false, false);

				g_shellMutex.Lock();
				SceCommonGuiDialog_BC311DC0(dlgId);
				gameIf->setDialogHold(0);
				topmenuIf->endGameHold();
				g_shellMutex.Unlock();

				if (installRes != 0)
					return installRes;
				res = liveareaMgrGetCachedTemplate(&m_parsedTemplate, &m_titleId, m_isDefault);
			}
		}
		return res;
	}

	int language = SceLsdb_60B535FD(m_appAttr, -1);
	paf::string country = SceLsdb_88FDC55C(m_appAttr);
	if (country.size() != 0) {
		int userAge = SceLsdb_1673AD48();
		SceLsdb_43495B68(m_lsdbParser, &language, country.c_str(), userAge);
		if (m_style == 3)
			m_lsdbParser->m_pokesute = SceLsdb_BE72A404(m_titleId.c_str());
		if (m_hasKind)
			m_lsdbParser->m_supportedFormatOnly = true;
		m_lsdbParser->m_networkTime = SceLsdb_69B32F6C(m_appAttr);
		return SceLsdb_632ACD0F(m_lsdbParser);
	}

	paf::string welcomeParkTitle("NPXS10015");
	if (welcomeParkTitle == m_titleId)
		return SceLsdb_632ACD0F(m_lsdbParser);
	return -5;
}

/* 0x811B6F18 */
void LiveAreaContentParserJob::Finish(int result)
{
	if (IsCanceled() || m_inner == NULL)
		return;

	if (m_useLsdb == 0) {
		if (result == 0) {
			m_inner->OnParseComplete(&m_parsedTemplate, m_reloadMask, &m_contentDir);
			return;
		}
		if (m_allowDialog != 0 && result == (int)0x8010111E) {
			CommonGuiLayoutDialogParam dlgParam;
			dlgParam.resourcePlugin = g_liveareaPlugin;
			dlgParam.soundId = (SceUInt32)-682994459;
			dlgParam.priority = 40;
			const wchar_t *msg = g_liveareaPlugin->GetString(paf::IDParam((SceUInt32)-198752243));
			dlgParam.text = msg ? msg : L"";
			dlgParam.handler = new ContentParserErrorDialogHandler(m_inner);
			int dlgId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
			g_liveareaMgr->m_topmenuIf->registerDialogId(dlgId);
			m_inner->OnParseFail();
			g_liveareaMgr->m_applauncherIf->endIconEffects();
			return;
		}
		m_inner->LoadDefaultXml();
		return;
	}

	if (result != 0) {
		if (result != -3) {
			m_inner->OnParseFail();
			return;
		}
		m_inner->LoadDefaultXml();
		return;
	}

	if (m_lsdbParser->m_isDefault)
		m_contentDir = "vs0:data/internal/livearea/default/sce_sys/livearea/contents/";
	m_inner->OnParseComplete(&m_lsdbParser->m_template, m_reloadMask, &m_contentDir);
}

/* 0x811B7168 */
void LiveAreaContentParserJob::CancelAndDetach()
{
	m_inner = NULL;
	Cancel();
}
