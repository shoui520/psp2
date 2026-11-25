/*
 * SceShell lsdb_job: LiveSpace DB async jobs
 * 3.65-CEX: 0x811C4CB8 - 0x811C6ECC (sti 0x811C6E5C)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/map>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <graphics/surface/surface.h>
#include <job/job.h>

#include "../../livespace_db/lsdb_keys.h"

struct LsdbJobClientHolder {
	void *client;
};

extern LsdbJobClientHolder *g_lsdbJobClientHolder;		/* 0x8156B860 */

struct LsdbPageLayoutItem {
	void *f0;
	int f4;
	int f8;
	int fC;
};

struct LsdbPageThemeItem {
	paf::string f0;
	int f0C;
	int f10;
	int f14;
	int f18;
	int f1C;
	int f20;

	LsdbPageThemeItem()
		: f0C(0), f10(0), f14(0), f18(0), f1C(0), f20(0x01FFFFFF) {}
};

struct LsdbIconInfo {
	int f0;
	int f4;
	int f8;
	paf::string fC;
	paf::string f18;
	int f24;
	paf::string f28;
	int f34;
	paf::string f38;
	int f44;
	int f48;
	paf::common::SharedPtr<int> f4C;

	LsdbIconInfo()
		: f0(0), f4(0), f8(0), f24(0), f34(0), f44(0), f48(0) {}
};

struct LsdbFolderSurfacePair {
	int f0;
	paf::graph::Surface *surface;

	LsdbFolderSurfacePair(const LsdbFolderSurfacePair &o)
		: f0(o.f0), surface(o.surface)
	{
		if (surface)
			surface->AddRef();
	}
	~LsdbFolderSurfacePair()
	{
		if (surface)
			surface->Release();
	}
};

struct LsdbAttrMapVal {
	int type;
	int pad;
	union {
		SceInt64 i64;
		paf::string *str;
	};
};

extern "C" {
extern int SceLsdb_D74AC354(void *client, int pageId, int count, void *outVec);
extern int SceLsdb_CF4B3684(void *client, int pageId, int flag);
extern int SceLsdb_8FAD093B(void *client, int pageId, int count, void *outVec);
extern int SceLsdb_6769DBC4(void *client, void *pageIds, void *themes);
extern int SceLsdb_BF7AFB42(void *client, void *srcPages, void *srcPositions, void *dstPages, void *dstPositions);
extern int SceLsdb_3387B5A3(void *client, int pageIdx, void *icons, void *theme);
extern int SceLsdb_76C05B2E(void *client, int pageId);
extern int SceLsdb_94128795(void *client);
extern int SceLsdb_87EE0526(void *client, const paf::string *titleId, void *outMap);
extern int SceLsdb_DE0DA538(void *client, const paf::string *titleId, void *outIcon, int typeFlag, int modeFlag);
extern int SceLsdb_C1A82AC8(void *client, int pageId, void *iconSp, int flags, int extraFlags, int optMode);
extern int SceLsdb_84D3F674(void *client, void *appMap, int pageId, void *iconSp, void *vec);
extern int SceLsdb_DEA13663(void *client, void *appMap, void *iconSp, void *vec);
extern int SceLsdb_1AD44F35(void *client, void *appMap, void *iconSp, int flags, void *vec);
extern int SceLsdb_E786531F(void *client, int pageId, void *iconSp, void *folderIconSp, void *buf, int size);
}

extern void *shellBuildFolderThumbnail(void *surfaces, int *outSize); /* 0x813C3A72 */

typedef void (*LsdbJobCallback)(void *job, void *userData);

class LsdbJobBase : public paf::job::JobItem {
public:
	LsdbJobBase(const char *name, LsdbJobCallback cb, void *userData)
		: paf::job::JobItem(name, NULL),
		  m_cb(cb), m_userData(userData), m_result(0), m_done(0) {}
	virtual ~LsdbJobBase();					/* 0x811C4CB8 / 0x811C4CD4 */
	virtual int Run() { return 0; }
	virtual void Finish(int result);			/* 0x811C4CF4 */

	LsdbJobCallback m_cb;					/* +28 */
	void *m_userData;					/* +32 */
	int m_result;						/* +36 */
	SceUInt8 m_done;					/* +40 */
};

/* 0x811C4CB8 / 0x811C4CD4 */
LsdbJobBase::~LsdbJobBase()
{
}

/* 0x811C4CF4 */
void LsdbJobBase::Finish(int result)
{
	m_result = result;
	if (!IsCanceled() && m_cb != NULL)
		m_cb(this, m_userData);
}

/* 1. GetPageLayoutJob (0x811C4D10 - 0x811C4F52) */
class GetPageLayoutJob : public LsdbJobBase {
public:
	GetPageLayoutJob(int pageId, SceUInt32 count, LsdbJobCallback cb, void *userData); /* 0x811C4D10 */
	virtual ~GetPageLayoutJob();				/* 0x811C4EC8 / 0x811C4EF8 */
	virtual int Run();					/* 0x811C4F2C */

	int m_pageId;						/* +44 */
	SceUInt32 m_count;					/* +48 */
	paf::vector<paf::common::SharedPtr<LsdbPageLayoutItem> > m_pages; /* +52 */
};

/* 0x811C4D10 */
GetPageLayoutJob::GetPageLayoutJob(int pageId, SceUInt32 count, LsdbJobCallback cb, void *userData)
	: LsdbJobBase("GetPageLayoutJob", cb, userData), m_pageId(pageId), m_count(count)
{
	if (count != 0) {
		m_pages.reserve(count);
		for (SceUInt32 i = 0; i < m_count; ++i) {
			LsdbPageLayoutItem *item = new LsdbPageLayoutItem();
			if (item != NULL) {
				item->f0 = NULL;
				item->f4 = 0;
				item->f8 = 0;
			}
			m_pages.push_back(paf::common::SharedPtr<LsdbPageLayoutItem>(item));
		}
	}
}

/* 0x811C4EC8 / 0x811C4EF8 */
GetPageLayoutJob::~GetPageLayoutJob()
{
}

/* 0x811C4F2C */
int GetPageLayoutJob::Run()
{
	int r = SceLsdb_D74AC354(g_lsdbJobClientHolder->client, m_pageId, m_count, &m_pages);
	m_done = 1;
	return r;
}

/* 0x811C4E4C */
int lsdbEnqueueGetPageLayoutJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue, int pageId, SceUInt32 count, LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(new GetPageLayoutJob(pageId, count, cb, userData));
	return queue->Enqueue(*out);
}

/* 2. GetIconNumJob (0x811C4F52 - 0x811C5012) */
class GetIconNumJob : public LsdbJobBase {
public:
	GetIconNumJob(int pageId, LsdbJobCallback cb, void *userData)
		: LsdbJobBase("GetIconNumJob", cb, userData), m_pageId(pageId), m_iconNum(0) {}
	virtual ~GetIconNumJob() {}
	virtual int Run();					/* 0x811C4FEA */

	int m_pageId;						/* +44 */
	int m_iconNum;						/* +48 */
};

/* 0x811C4FEA */
int GetIconNumJob::Run()
{
	int r = SceLsdb_CF4B3684(g_lsdbJobClientHolder->client, m_pageId, 1);
	m_done = 1;
	if (r >= 0)
		m_iconNum = r;
	return r;
}

/* 0x811C4F52 */
int lsdbEnqueueGetIconNumJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue, int pageId, LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(new GetIconNumJob(pageId, cb, userData));
	return queue->Enqueue(*out);
}

/* 3. GetPageThemeJob (0x811C5012 - 0x811C5278) */
class GetPageThemeJob : public LsdbJobBase {
public:
	GetPageThemeJob(int pageId, SceUInt32 count, LsdbJobCallback cb, void *userData); /* 0x811C5012 */
	virtual ~GetPageThemeJob();				/* 0x811C51EE / 0x811C521E */
	virtual int Run();					/* 0x811C5252 */

	int m_pageId;						/* +44 */
	SceUInt32 m_count;					/* +48 */
	paf::vector<paf::common::SharedPtr<LsdbPageThemeItem> > m_themes; /* +52 */
};

/* 0x811C5012 */
GetPageThemeJob::GetPageThemeJob(int pageId, SceUInt32 count, LsdbJobCallback cb, void *userData)
	: LsdbJobBase("GetPageThemeJob", cb, userData), m_pageId(pageId), m_count(count)
{
	if (count != 0) {
		m_themes.reserve(count);
		for (SceUInt32 i = 0; i < m_count; ++i)
			m_themes.push_back(paf::common::SharedPtr<LsdbPageThemeItem>(new LsdbPageThemeItem()));
	}
}

/* 0x811C51EE / 0x811C521E */
GetPageThemeJob::~GetPageThemeJob()
{
}

/* 0x811C5252 */
int GetPageThemeJob::Run()
{
	int r = SceLsdb_8FAD093B(g_lsdbJobClientHolder->client, m_pageId, m_count, &m_themes);
	m_done = 1;
	return r;
}

/* 0x811C5172 */
int lsdbEnqueueGetPageThemeJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue, int pageId, SceUInt32 count, LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(new GetPageThemeJob(pageId, count, cb, userData));
	return queue->Enqueue(*out);
}

/* 4. SetPageThemeJob (0x811C5278 - 0x811C53DA) */
class SetPageThemeJob : public LsdbJobBase {
public:
	SetPageThemeJob(const paf::vector<int> &pageIds,
		const paf::vector<paf::common::SharedPtr<LsdbPageThemeItem> > &themes,
		LsdbJobCallback cb, void *userData)
		: LsdbJobBase("SetPageThemeJob", cb, userData),
		  m_pageIds(pageIds), m_themes(themes) {}
	virtual ~SetPageThemeJob();				/* 0x811C532A / 0x811C536E */
	virtual int Run();					/* 0x811C53B6 */

	paf::vector<int> m_pageIds;				/* +44 */
	paf::vector<paf::common::SharedPtr<LsdbPageThemeItem> > m_themes; /* +60 */
};

/* 0x811C532A / 0x811C536E */
SetPageThemeJob::~SetPageThemeJob()
{
}

/* 0x811C53B6 */
int SetPageThemeJob::Run()
{
	int r = SceLsdb_6769DBC4(g_lsdbJobClientHolder->client, &m_pageIds, &m_themes);
	m_done = 1;
	return r;
}

/* 0x811C5278 */
int lsdbEnqueueSetPageThemeJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue, const paf::vector<int> &pageIds,
	const paf::vector<paf::common::SharedPtr<LsdbPageThemeItem> > &themes,
	LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(new SetPageThemeJob(pageIds, themes, cb, userData));
	return queue->Enqueue(*out);
}

/* 5. SetPageLayoutJob (0x811C53DA - 0x811C57C0) */
class SetPageLayoutJob : public LsdbJobBase {
public:
	SetPageLayoutJob(const paf::vector<int> &srcPages, const paf::vector<int> &srcPositions,
		const paf::vector<int> &dstPages, const paf::vector<int> &dstPositions,
		const paf::vector<void *> &iconItems, void (*postCb)(void *),
		LsdbJobCallback cb, void *userData)
		: LsdbJobBase("SetPageLayoutJob", cb, userData),
		  m_srcPages(srcPages), m_srcPositions(srcPositions),
		  m_dstPages(dstPages), m_dstPositions(dstPositions),
		  m_iconItems(iconItems), m_postCb(postCb) {}
	virtual ~SetPageLayoutJob();				/* 0x811C54BC / 0x811C553E */
	virtual int Run();					/* 0x811C554E */

	paf::vector<int> m_srcPages;				/* +44 */
	paf::vector<int> m_srcPositions;			/* +60 */
	paf::vector<int> m_dstPages;				/* +76 */
	paf::vector<int> m_dstPositions;			/* +92 */
	paf::vector<void *> m_iconItems;			/* +108 */
	void (*m_postCb)(void *);				/* +124 */
};

/* 0x811C54BC / 0x811C553E */
SetPageLayoutJob::~SetPageLayoutJob()
{
}

/* 0x811C554E */
int SetPageLayoutJob::Run()
{
	for (SceUInt32 i = 0; i < m_dstPages.size(); ++i) {
		int pageId = m_dstPages[i];
		if (pageId >= -20000000 && pageId < -10000000 && m_dstPositions[i] < 0) {
			paf::vector<paf::common::SharedPtr<LsdbPageLayoutItem> > tmp;
			LsdbPageLayoutItem *item = new LsdbPageLayoutItem();
			if (item != NULL) {
				item->f0 = NULL;
				item->f4 = 0;
				item->f8 = 0;
			}
			tmp.push_back(paf::common::SharedPtr<LsdbPageLayoutItem>(item));
			int r = SceLsdb_D74AC354(g_lsdbJobClientHolder->client, pageId, 1, &tmp);
			if (r != 0) {
				m_done = 1;
				return r;
			}
			SceUInt8 used[10];
			sce_paf_memset(used, 0, sizeof(used));
			paf::vector<paf::common::SharedPtr<LsdbIconInfo> > *icons =
				(paf::vector<paf::common::SharedPtr<LsdbIconInfo> > *)tmp[0].get();
			for (SceUInt32 j = 0; j < icons->size(); ++j) {
				SceUInt32 pos = (SceUInt32)(*icons)[j]->f8;
				if (pos <= 9u)
					used[pos] = 1;
			}
			int freePos = -1;
			for (int k = 0; k < 10; ++k) {
				if (used[k] == 0) {
					freePos = k;
					break;
				}
			}
			if (freePos >= 0)
				m_dstPositions[i] = freePos;
			if (m_dstPositions[i] < 0) {
				m_done = 1;
				return -1;
			}
		}
	}
	int r = SceLsdb_BF7AFB42(g_lsdbJobClientHolder->client, &m_srcPages, &m_srcPositions, &m_dstPages, &m_dstPositions);
	m_done = 1;
	if (m_postCb != NULL)
		m_postCb(this);
	return r;
}

/* 0x811C53DA */
int lsdbEnqueueSetPageLayoutJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue,
	const paf::vector<int> &srcPages, const paf::vector<int> &srcPositions,
	const paf::vector<int> &dstPages, const paf::vector<int> &dstPositions,
	const paf::vector<void *> &iconItems, void (*postCb)(void *),
	LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(
		new SetPageLayoutJob(srcPages, srcPositions, dstPages, dstPositions, iconItems, postCb, cb, userData));
	return queue->Enqueue(*out);
}

/* 6. CreateNewPageJob (0x811C57C0 - 0x811C5986) */
class CreateNewPageJob : public LsdbJobBase {
public:
	__attribute__((noinline)) CreateNewPageJob(int pageIdx, const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > &icons,
		const LsdbPageThemeItem &theme, LsdbJobCallback cb, void *userData);
	virtual ~CreateNewPageJob();				/* 0x811C58CC / 0x811C5914 */
	virtual int Run();					/* 0x811C5960 */

	int m_pageIdx;						/* +44 */
	paf::vector<paf::common::SharedPtr<LsdbIconInfo> > m_icons; /* +48 */
	LsdbPageThemeItem m_theme;				/* +64 */
};

/* 0x811C57C0 */
CreateNewPageJob::CreateNewPageJob(int pageIdx, const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > &icons,
	const LsdbPageThemeItem &theme, LsdbJobCallback cb, void *userData)
	: LsdbJobBase("CreateNewPageJob", cb, userData),
	  m_pageIdx(pageIdx), m_icons(icons), m_theme(theme)
{
}

/* 0x811C58CC / 0x811C5914 */
CreateNewPageJob::~CreateNewPageJob()
{
}

/* 0x811C5960 */
int CreateNewPageJob::Run()
{
	int r = SceLsdb_3387B5A3(g_lsdbJobClientHolder->client, m_pageIdx, &m_icons, &m_theme);
	m_done = 1;
	return r;
}

/* 0x811C5848 */
int lsdbEnqueueCreateNewPageJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue, int pageIdx,
	const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > &icons,
	const LsdbPageThemeItem &theme, LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(
		new CreateNewPageJob(pageIdx, icons, theme, cb, userData));
	return queue->Enqueue(*out);
}

/* 7. DeletePageJob (0x811C5986 - 0x811C5A3C) */
class DeletePageJob : public LsdbJobBase {
public:
	DeletePageJob(int pageId, LsdbJobCallback cb, void *userData)
		: LsdbJobBase("DeletePageJob", cb, userData), m_pageId(pageId) {}
	virtual ~DeletePageJob() {}
	virtual int Run();					/* 0x811C5A1C */

	int m_pageId;						/* +44 */
};

/* 0x811C5A1C */
int DeletePageJob::Run()
{
	int r = SceLsdb_76C05B2E(g_lsdbJobClientHolder->client, m_pageId);
	m_done = 1;
	return r;
}

/* 0x811C5986 */
int lsdbEnqueueDeletePageJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue, int pageId, LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(new DeletePageJob(pageId, cb, userData));
	return queue->Enqueue(*out);
}

/* 8. GetTotalPageJob (0x811C5A3C - 0x811C5AF0) */
class GetTotalPageJob : public LsdbJobBase {
public:
	GetTotalPageJob(LsdbJobCallback cb, void *userData)
		: LsdbJobBase("GetTotalPageJob", cb, userData), m_totalPage(-1) {}
	virtual ~GetTotalPageJob() {}
	virtual int Run();					/* 0x811C5ACC */

	int m_totalPage;					/* +44 */
};

/* 0x811C5ACC */
int GetTotalPageJob::Run()
{
	int r = SceLsdb_94128795(g_lsdbJobClientHolder->client);
	if (r >= 0)
		m_totalPage = r;
	m_done = 1;
	return r;
}

/* 0x811C5A3C */
int lsdbEnqueueGetTotalPageJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue, LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(new GetTotalPageJob(cb, userData));
	return queue->Enqueue(*out);
}

/* 9. GetAppInfoJob (0x811C5AF0 - 0x811C5CD6) */
class GetAppInfoJob : public LsdbJobBase {
public:
	__attribute__((noinline)) GetAppInfoJob(const paf::string &titleId, LsdbJobCallback cb, void *userData);
	virtual ~GetAppInfoJob();				/* 0x811C5C12 / 0x811C5CA2 */
	virtual int Run();					/* 0x811C5CB2 */

	paf::string m_titleId;					/* +44 */
	paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > m_appInfo; /* +56 */
};

/* 0x811C5AF0 */
GetAppInfoJob::GetAppInfoJob(const paf::string &titleId, LsdbJobCallback cb, void *userData)
	: LsdbJobBase("GetAppInfoJob", cb, userData),
	  m_titleId(titleId),
	  m_appInfo(new paf::map<SceUInt32, LsdbAttrMapVal>())
{
}

/* 0x811C5C12 / 0x811C5CA2 */
GetAppInfoJob::~GetAppInfoJob()
{
}

/* 0x811C5CB2 */
int GetAppInfoJob::Run()
{
	int r = SceLsdb_87EE0526(g_lsdbJobClientHolder->client, &m_titleId, &m_appInfo);
	m_done = 1;
	return r;
}

/* 0x811C5BA6 */
int lsdbEnqueueGetAppInfoJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue, const paf::string &titleId, LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(new GetAppInfoJob(titleId, cb, userData));
	return queue->Enqueue(*out);
}

/* 10. GetIconInfoJob (0x811C5CD6 - 0x811C5ECC) */
class GetIconInfoJob : public LsdbJobBase {
public:
	__attribute__((noinline)) GetIconInfoJob(const paf::string &titleId, LsdbJobCallback cb, void *userData);
	virtual ~GetIconInfoJob();				/* 0x811C5DFE / 0x811C5E8E */
	virtual int Run();					/* 0x811C5E9E */

	paf::string m_titleId;					/* +44 */
	paf::common::SharedPtr<LsdbIconInfo> m_icon;		/* +56 */
};

/* 0x811C5CD6 */
GetIconInfoJob::GetIconInfoJob(const paf::string &titleId, LsdbJobCallback cb, void *userData)
	: LsdbJobBase("GetIconInfoJob", cb, userData),
	  m_titleId(titleId), m_icon(new LsdbIconInfo())
{
}

/* 0x811C5DFE / 0x811C5E8E */
GetIconInfoJob::~GetIconInfoJob()
{
}

/* 0x811C5E9E */
int GetIconInfoJob::Run()
{
	int r = SceLsdb_DE0DA538(g_lsdbJobClientHolder->client, &m_titleId, &m_icon, 0, 0);
	m_done = 1;
	return r;
}

/* 0x811C5D92 */
int lsdbEnqueueGetIconInfoJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue, const paf::string &titleId, LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(new GetIconInfoJob(titleId, cb, userData));
	return queue->Enqueue(*out);
}

/* 11. SetIconInfoJob (0x811C5ECC - 0x811C6072) */
class SetIconInfoJob : public LsdbJobBase {
public:
	__attribute__((noinline)) SetIconInfoJob(int pageId, const paf::common::SharedPtr<LsdbIconInfo> &icon,
		int flags, LsdbJobCallback cb, void *userData);
	virtual ~SetIconInfoJob();				/* 0x811C5FBC / 0x811C6032 */
	virtual int Run();					/* 0x811C6042 */

	int m_pageId;						/* +44 */
	paf::common::SharedPtr<LsdbIconInfo> m_icon;		/* +48 */
	int m_flags;						/* +56 */
};

/* 0x811C5ECC */
SetIconInfoJob::SetIconInfoJob(int pageId, const paf::common::SharedPtr<LsdbIconInfo> &icon,
	int flags, LsdbJobCallback cb, void *userData)
	: LsdbJobBase("SetIconInfoJob", cb, userData),
	  m_pageId(pageId), m_icon(icon), m_flags(flags)
{
}

/* 0x811C5FBC / 0x811C6032 */
SetIconInfoJob::~SetIconInfoJob()
{
}

/* 0x811C6042 */
int SetIconInfoJob::Run()
{
	int r = SceLsdb_C1A82AC8(g_lsdbJobClientHolder->client, m_pageId, &m_icon, m_flags, 0, 0);
	m_done = 1;
	return r;
}

/* 0x811C5F38 */
int lsdbEnqueueSetIconInfoJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue, int pageId,
	const paf::common::SharedPtr<LsdbIconInfo> &icon, int flags,
	LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(
		new SetIconInfoJob(pageId, icon, flags, cb, userData));
	return queue->Enqueue(*out);
}

/* 12. InsertAppInfoJob (0x811C6072 - 0x811C62AA) */
class InsertAppInfoJob : public LsdbJobBase {
public:
	__attribute__((noinline)) InsertAppInfoJob(const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > &appInfo,
		const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > &icons,
		int pageId, const paf::common::SharedPtr<LsdbIconInfo> &icon,
		LsdbJobCallback cb, void *userData);
	virtual ~InsertAppInfoJob();				/* 0x811C619C / 0x811C626A */
	virtual int Run();					/* 0x811C627A */

	paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > m_appInfo; /* +44 */
	paf::vector<paf::common::SharedPtr<LsdbIconInfo> > m_icons; /* +52 */
	int m_pageId;						/* +68 */
	paf::common::SharedPtr<LsdbIconInfo> m_icon;		/* +72 */
};

/* 0x811C6072 */
InsertAppInfoJob::InsertAppInfoJob(const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > &appInfo,
	const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > &icons,
	int pageId, const paf::common::SharedPtr<LsdbIconInfo> &icon,
	LsdbJobCallback cb, void *userData)
	: LsdbJobBase("InsertAppInfoJob", cb, userData),
	  m_appInfo(appInfo), m_icons(icons), m_pageId(pageId), m_icon(icon)
{
}

/* 0x811C619C / 0x811C626A */
InsertAppInfoJob::~InsertAppInfoJob()
{
}

/* 0x811C627A */
int InsertAppInfoJob::Run()
{
	int r = SceLsdb_84D3F674(g_lsdbJobClientHolder->client, &m_appInfo, m_pageId, &m_icon, &m_icons);
	m_done = 1;
	return r;
}

/* 0x811C6110 */
int lsdbEnqueueInsertAppInfoJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue,
	const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > &appInfo,
	const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > &icons,
	int pageId, const paf::common::SharedPtr<LsdbIconInfo> &icon,
	LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(
		new InsertAppInfoJob(appInfo, icons, pageId, icon, cb, userData));
	return queue->Enqueue(*out);
}

/* 13. UpdateAppInfoJob (0x811C62AA - 0x811C64CA) */
class UpdateAppInfoJob : public LsdbJobBase {
public:
	__attribute__((noinline)) UpdateAppInfoJob(const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > &appInfo,
		const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > &icons,
		const paf::common::SharedPtr<LsdbIconInfo> &icon,
		LsdbJobCallback cb, void *userData);
	virtual ~UpdateAppInfoJob();				/* 0x811C63C4 / 0x811C6492 */
	virtual int Run();					/* 0x811C64A2 */

	paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > m_appInfo; /* +44 */
	paf::vector<paf::common::SharedPtr<LsdbIconInfo> > m_icons; /* +52 */
	int m_reserved;						/* +68 */
	paf::common::SharedPtr<LsdbIconInfo> m_icon;		/* +72 */
};

/* 0x811C62AA */
UpdateAppInfoJob::UpdateAppInfoJob(const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > &appInfo,
	const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > &icons,
	const paf::common::SharedPtr<LsdbIconInfo> &icon,
	LsdbJobCallback cb, void *userData)
	: LsdbJobBase("UpdateAppInfoJob", cb, userData),
	  m_appInfo(appInfo), m_icons(icons), m_reserved(0), m_icon(icon)
{
}

/* 0x811C63C4 / 0x811C6492 */
UpdateAppInfoJob::~UpdateAppInfoJob()
{
}

/* 0x811C64A2 */
int UpdateAppInfoJob::Run()
{
	int r = SceLsdb_DEA13663(g_lsdbJobClientHolder->client, &m_appInfo, &m_icon, &m_icons);
	m_done = 1;
	return r;
}

/* 0x811C6340 */
int lsdbEnqueueUpdateAppInfoJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue,
	const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > &appInfo,
	const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > &icons,
	const paf::common::SharedPtr<LsdbIconInfo> &icon,
	LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(
		new UpdateAppInfoJob(appInfo, icons, icon, cb, userData));
	return queue->Enqueue(*out);
}

/* 14. UpdateAppInfoDiffJob (0x811C64CA - 0x811C6704) */
class UpdateAppInfoDiffJob : public LsdbJobBase {
public:
	__attribute__((noinline)) UpdateAppInfoDiffJob(const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > &appInfo,
		const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > &icons,
		const paf::common::SharedPtr<LsdbIconInfo> &icon, int flags,
		LsdbJobCallback cb, void *userData);
	virtual ~UpdateAppInfoDiffJob();			/* 0x811C65F4 / 0x811C66C2 */
	virtual int Run();					/* 0x811C66D2 */

	paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > m_appInfo; /* +44 */
	paf::vector<paf::common::SharedPtr<LsdbIconInfo> > m_icons; /* +52 */
	int m_reserved;						/* +68 */
	paf::common::SharedPtr<LsdbIconInfo> m_icon;		/* +72 */
	int m_flags;						/* +80 */
};

/* 0x811C64CA */
UpdateAppInfoDiffJob::UpdateAppInfoDiffJob(const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > &appInfo,
	const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > &icons,
	const paf::common::SharedPtr<LsdbIconInfo> &icon, int flags,
	LsdbJobCallback cb, void *userData)
	: LsdbJobBase("UpdateAppInfoDiffJob", cb, userData),
	  m_appInfo(appInfo), m_icons(icons), m_reserved(0), m_icon(icon), m_flags(flags)
{
}

/* 0x811C65F4 / 0x811C66C2 */
UpdateAppInfoDiffJob::~UpdateAppInfoDiffJob()
{
}

/* 0x811C66D2 */
int UpdateAppInfoDiffJob::Run()
{
	int r = SceLsdb_1AD44F35(g_lsdbJobClientHolder->client, &m_appInfo, &m_icon, m_flags, &m_icons);
	m_done = 1;
	return r;
}

/* 0x811C6568 */
int lsdbEnqueueUpdateAppInfoDiffJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue,
	const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > &appInfo,
	const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > &icons,
	const paf::common::SharedPtr<LsdbIconInfo> &icon, int flags,
	LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(
		new UpdateAppInfoDiffJob(appInfo, icons, icon, flags, cb, userData));
	return queue->Enqueue(*out);
}

/* 15. CreateNewPageAndInsertAppJob (0x811C6704 - 0x811C6A18) */
class CreateNewPageAndInsertAppJob : public LsdbJobBase {
public:
	__attribute__((noinline)) CreateNewPageAndInsertAppJob(int pageIdx, const LsdbPageThemeItem &theme,
		const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > &appInfo,
		const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > &icons,
		const paf::common::SharedPtr<LsdbIconInfo> &icon,
		LsdbJobCallback cb, void *userData);
	virtual ~CreateNewPageAndInsertAppJob();		/* 0x811C6874 / 0x811C695C */
	virtual int Run();					/* 0x811C696C */

	int m_pageIdx;						/* +44 */
	LsdbPageThemeItem m_theme;				/* +48 */
	paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > m_appInfo; /* +84 */
	paf::vector<paf::common::SharedPtr<LsdbIconInfo> > m_icons; /* +92 */
	paf::common::SharedPtr<LsdbIconInfo> m_icon;		/* +108 */
};

/* 0x811C6704 */
CreateNewPageAndInsertAppJob::CreateNewPageAndInsertAppJob(int pageIdx, const LsdbPageThemeItem &theme,
	const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > &appInfo,
	const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > &icons,
	const paf::common::SharedPtr<LsdbIconInfo> &icon,
	LsdbJobCallback cb, void *userData)
	: LsdbJobBase("CreateNewPageAndInsertAppJob", cb, userData),
	  m_pageIdx(pageIdx), m_theme(theme),
	  m_appInfo(appInfo), m_icons(icons), m_icon(icon)
{
}

/* 0x811C6874 / 0x811C695C */
CreateNewPageAndInsertAppJob::~CreateNewPageAndInsertAppJob()
{
}

/* 0x811C696C */
int CreateNewPageAndInsertAppJob::Run()
{
	paf::vector<paf::common::SharedPtr<LsdbIconInfo> > emptyIcons;
	int r = SceLsdb_3387B5A3(g_lsdbJobClientHolder->client, m_pageIdx, &emptyIcons, &m_theme);
	if (r != 0) {
		m_done = 1;
		return r;
	}
	r = SceLsdb_84D3F674(g_lsdbJobClientHolder->client, &m_appInfo, m_pageIdx, &m_icon, &m_icons);
	if (r != 0) {
		SceLsdb_76C05B2E(g_lsdbJobClientHolder->client, m_pageIdx);
		m_done = 1;
		return r;
	}
	m_done = 1;
	return 0;
}

/* 0x811C67E2 */
int lsdbEnqueueCreateNewPageAndInsertAppJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue, int pageIdx, const LsdbPageThemeItem &theme,
	const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > &appInfo,
	const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > &icons,
	const paf::common::SharedPtr<LsdbIconInfo> &icon,
	LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(
		new CreateNewPageAndInsertAppJob(pageIdx, theme, appInfo, icons, icon, cb, userData));
	return queue->Enqueue(*out);
}

/* 16. CreateNewFolderJob (0x811C6A18 - 0x811C6E5C) */
class CreateNewFolderJob : public LsdbJobBase {
public:
	CreateNewFolderJob(int pageId, const paf::common::SharedPtr<LsdbIconInfo> &icon,
		const paf::common::SharedPtr<LsdbIconInfo> &folderIcon,
		const paf::vector<LsdbFolderSurfacePair> &surfaces,
		LsdbJobCallback cb, void *userData);		/* 0x811C6A18 */
	virtual ~CreateNewFolderJob();				/* 0x811C6C7E / 0x811C6DC0 */
	virtual int Run();					/* 0x811C6DD0 */

	int m_pageId;						/* +44 */
	paf::common::SharedPtr<LsdbIconInfo> m_icon;		/* +48 */
	paf::vector<LsdbFolderSurfacePair> m_surfaces;		/* +56 */
	paf::common::SharedPtr<LsdbIconInfo> m_folderCopy;	/* +72 */
	paf::common::SharedPtr<LsdbIconInfo> m_folderSrc;	/* +80 */
};

/* 0x811C6A18 */
CreateNewFolderJob::CreateNewFolderJob(int pageId,
	const paf::common::SharedPtr<LsdbIconInfo> &icon,
	const paf::common::SharedPtr<LsdbIconInfo> &folderIcon,
	const paf::vector<LsdbFolderSurfacePair> &surfaces,
	LsdbJobCallback cb, void *userData)
	: LsdbJobBase("CreateNewFolderJob", cb, userData),
	  m_pageId(pageId), m_icon(icon), m_surfaces(surfaces),
	  m_folderCopy(new LsdbIconInfo()), m_folderSrc(folderIcon)
{
	*m_folderCopy = *m_folderSrc;
}

/* 0x811C6C7E / 0x811C6DC0 */
CreateNewFolderJob::~CreateNewFolderJob()
{
}

/* 0x811C6DD0 */
int CreateNewFolderJob::Run()
{
	paf::vector<paf::common::SharedPtr<LsdbIconInfo> > tmp;
	void *buf = NULL;
	int bufSize = 0;
	if (m_surfaces.size() != 0)
		buf = shellBuildFolderThumbnail(&m_surfaces, &bufSize);
	int r = SceLsdb_E786531F(g_lsdbJobClientHolder->client, m_pageId, &m_icon, &m_folderCopy, buf, bufSize);
	if (buf != NULL)
		sce_paf_free(buf);
	m_done = 1;
	return (r != 0) ? r : 0;
}

/* 0x811C6BF2 */
int lsdbEnqueueCreateNewFolderJob(paf::common::SharedPtr<paf::job::JobItem> *out,
	paf::job::JobQueue *queue, int pageId,
	const paf::common::SharedPtr<LsdbIconInfo> &icon,
	const paf::common::SharedPtr<LsdbIconInfo> &folderIcon,
	const paf::vector<LsdbFolderSurfacePair> &surfaces,
	LsdbJobCallback cb, void *userData)
{
	*out = paf::common::SharedPtr<paf::job::JobItem>(
		new CreateNewFolderJob(pageId, icon, folderIcon, surfaces, cb, userData));
	return queue->Enqueue(*out);
}
