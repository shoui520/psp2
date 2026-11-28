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

extern LsdbJobClientHolder *g_shellLsdbClient;		/* 0x8156B860 */

struct LsdbPageLayoutItem {
	void *begin;
	void *end;
	void *capacity;
	int allocator;
};

struct LsdbPageThemeItem {
	paf::string themeFile;
	int bgColor;
	int texWidth;
	int texHeight;
	int imageWidth;
	int imageHeight;
	int fontColorWithShadow;

	LsdbPageThemeItem()
		: bgColor(0), texWidth(0), texHeight(0), imageWidth(0), imageHeight(0),
		  fontColorWithShadow(0x01FFFFFF) {}
};

struct LsdbIconInfo {
	SceInt64 rowid;
	int pos;
	paf::string title;
	paf::string iconPath;
	int type;
	paf::string titleId;
	int icon0Type;
	paf::string command;
	int parentalLockLv;
	int status;
	paf::common::SharedPtr<int> folder;

	LsdbIconInfo()
		: rowid(0), pos(0), type(0), icon0Type(0), parentalLockLv(0), status(0) {}
};

struct LsdbFolderSurfacePair {
	int pos;
	paf::graph::Surface *surface;

	LsdbFolderSurfacePair(const LsdbFolderSurfacePair &other)
		: pos(other.pos), surface(other.surface)
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
		SceInt64 intVal;
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

struct ApplauncherFolderSurface;
extern void *applauncherIconCaptureFolder(const paf::vector<ApplauncherFolderSurface> *surfaces,
					  int *outSize); /* 0x813C3A72 */

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
				item->begin = NULL;
				item->end = NULL;
				item->capacity = NULL;
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
	int r = SceLsdb_D74AC354(g_shellLsdbClient->client, m_pageId, m_count, &m_pages);
	m_done = 1;
	return r;
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
	int r = SceLsdb_CF4B3684(g_shellLsdbClient->client, m_pageId, 1);
	m_done = 1;
	if (r >= 0)
		m_iconNum = r;
	return r;
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
	int r = SceLsdb_8FAD093B(g_shellLsdbClient->client, m_pageId, m_count, &m_themes);
	m_done = 1;
	return r;
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
	int r = SceLsdb_6769DBC4(g_shellLsdbClient->client, &m_pageIds, &m_themes);
	m_done = 1;
	return r;
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
				item->begin = NULL;
				item->end = NULL;
				item->capacity = NULL;
			}
			tmp.push_back(paf::common::SharedPtr<LsdbPageLayoutItem>(item));
			int r = SceLsdb_D74AC354(g_shellLsdbClient->client, pageId, 1, &tmp);
			if (r != 0) {
				m_done = 1;
				return r;
			}
			SceUInt8 used[10];
			sce_paf_memset(used, 0, sizeof(used));
			paf::vector<paf::common::SharedPtr<LsdbIconInfo> > *icons =
				(paf::vector<paf::common::SharedPtr<LsdbIconInfo> > *)tmp[0].get();
			for (SceUInt32 j = 0; j < icons->size(); ++j) {
				SceUInt32 pos = (SceUInt32)(*icons)[j]->pos;
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
	int r = SceLsdb_BF7AFB42(g_shellLsdbClient->client, &m_srcPages, &m_srcPositions, &m_dstPages, &m_dstPositions);
	m_done = 1;
	if (m_postCb != NULL)
		m_postCb(this);
	return r;
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
	int r = SceLsdb_3387B5A3(g_shellLsdbClient->client, m_pageIdx, &m_icons, &m_theme);
	m_done = 1;
	return r;
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
	int r = SceLsdb_76C05B2E(g_shellLsdbClient->client, m_pageId);
	m_done = 1;
	return r;
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
	int r = SceLsdb_94128795(g_shellLsdbClient->client);
	if (r >= 0)
		m_totalPage = r;
	m_done = 1;
	return r;
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
	int r = SceLsdb_87EE0526(g_shellLsdbClient->client, &m_titleId, &m_appInfo);
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
	int r = SceLsdb_DE0DA538(g_shellLsdbClient->client, &m_titleId, &m_icon, 0, 0);
	m_done = 1;
	return r;
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
	int r = SceLsdb_C1A82AC8(g_shellLsdbClient->client, m_pageId, &m_icon, m_flags, 0, 0);
	m_done = 1;
	return r;
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
	int r = SceLsdb_84D3F674(g_shellLsdbClient->client, &m_appInfo, m_pageId, &m_icon, &m_icons);
	m_done = 1;
	return r;
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
	int r = SceLsdb_DEA13663(g_shellLsdbClient->client, &m_appInfo, &m_icon, &m_icons);
	m_done = 1;
	return r;
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
	int r = SceLsdb_1AD44F35(g_shellLsdbClient->client, &m_appInfo, &m_icon, m_flags, &m_icons);
	m_done = 1;
	return r;
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
	int r = SceLsdb_3387B5A3(g_shellLsdbClient->client, m_pageIdx, &emptyIcons, &m_theme);
	if (r != 0) {
		m_done = 1;
		return r;
	}
	r = SceLsdb_84D3F674(g_shellLsdbClient->client, &m_appInfo, m_pageIdx, &m_icon, &m_icons);
	if (r != 0) {
		SceLsdb_76C05B2E(g_shellLsdbClient->client, m_pageIdx);
		m_done = 1;
		return r;
	}
	m_done = 1;
	return 0;
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
		buf = applauncherIconCaptureFolder(
			reinterpret_cast<const paf::vector<ApplauncherFolderSurface> *>(&m_surfaces),
			&bufSize);
	int r = SceLsdb_E786531F(g_shellLsdbClient->client, m_pageId, &m_icon, &m_folderCopy, buf, bufSize);
	if (buf != NULL)
		sce_paf_free(buf);
	m_done = 1;
	return (r != 0) ? r : 0;
}

/*
 * Typed entry points used by applauncher_plugin (matching applauncher_home.h).
 */
struct LsdbIcon;
struct LsdbPage;
struct LsdbValue;
struct ApplauncherFolderSurface;
struct ApplauncherRemovedIcon;
class ApplauncherLsdbJob;

typedef paf::common::SharedPtr<LsdbIcon> ApplauncherLsdbIconPtr;
typedef paf::common::SharedPtr<paf::map<unsigned int, LsdbValue> > ApplauncherLsdbAppInfoPtr;
typedef paf::vector<paf::common::SharedPtr<LsdbPage> > ApplauncherLsdbPageList;
typedef void (*ApplauncherLsdbDoneFn)(ApplauncherLsdbJob *, void *);

/* 0x811C4E4C */
paf::common::SharedPtr<paf::job::JobItem> applauncherGetPageLayoutJobStart(
	paf::job::JobQueue *queue, int page, int count,
	ApplauncherLsdbDoneFn callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new GetPageLayoutJob(page, (SceUInt32)count,
				     reinterpret_cast<LsdbJobCallback>(callback), userData));
	queue->Enqueue(job);
	return job;
}

/* 0x811C4F52 */
paf::common::SharedPtr<paf::job::JobItem> applauncherGetIconNumJobStart(
	paf::job::JobQueue *queue, int page,
	ApplauncherLsdbDoneFn callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new GetIconNumJob(page, reinterpret_cast<LsdbJobCallback>(callback), userData));
	queue->Enqueue(job);
	return job;
}

/* 0x811C5172 */
paf::common::SharedPtr<paf::job::JobItem> applauncherGetPageThemeJobStart(
	paf::job::JobQueue *queue, int page, int count,
	ApplauncherLsdbDoneFn callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new GetPageThemeJob(page, (SceUInt32)count,
				    reinterpret_cast<LsdbJobCallback>(callback), userData));
	queue->Enqueue(job);
	return job;
}

/* 0x811C5278 */
paf::common::SharedPtr<paf::job::JobItem> applauncherSetPageThemeJobStart(
	paf::job::JobQueue *queue, const paf::vector<int> &pages,
	const ApplauncherLsdbPageList &themes,
	ApplauncherLsdbDoneFn callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new SetPageThemeJob(
			pages,
			*reinterpret_cast<const paf::vector<paf::common::SharedPtr<LsdbPageThemeItem> > *>(&themes),
			reinterpret_cast<LsdbJobCallback>(callback), userData));
	queue->Enqueue(job);
	return job;
}

/* 0x811C53DA */
paf::common::SharedPtr<paf::job::JobItem> applauncherSetPageLayoutJobStart(
	paf::job::JobQueue *queue,
	const paf::vector<int> &fromPages, const paf::vector<int> &fromPositions,
	const paf::vector<int> &toPages, const paf::vector<int> &toPositions,
	const paf::vector<ApplauncherRemovedIcon> &removed,
	ApplauncherLsdbDoneFn onRun, ApplauncherLsdbDoneFn callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new SetPageLayoutJob(
			fromPages, fromPositions, toPages, toPositions,
			*reinterpret_cast<const paf::vector<void *> *>(&removed),
			reinterpret_cast<void (*)(void *)>(onRun),
			reinterpret_cast<LsdbJobCallback>(callback), userData));
	queue->Enqueue(job);
	return job;
}

/* 0x811C5848 */
paf::common::SharedPtr<paf::job::JobItem> applauncherCreateNewPageJobStart(
	paf::job::JobQueue *queue, int page,
	const paf::vector<ApplauncherLsdbAppInfoPtr> &icons,
	const LsdbPage &theme, void *callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new CreateNewPageJob(
			page,
			*reinterpret_cast<const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > *>(&icons),
			*reinterpret_cast<const LsdbPageThemeItem *>(&theme),
			reinterpret_cast<LsdbJobCallback>(callback), userData));
	queue->Enqueue(job);
	return job;
}

/* 0x811C5986 */
paf::common::SharedPtr<paf::job::JobItem> applauncherDeletePageJobStart(
	paf::job::JobQueue *queue, int page, void *callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new DeletePageJob(page, reinterpret_cast<LsdbJobCallback>(callback), userData));
	queue->Enqueue(job);
	return job;
}

/* 0x811C5A3C */
paf::common::SharedPtr<paf::job::JobItem> applauncherGetTotalPageJobStart(
	paf::job::JobQueue *queue, ApplauncherLsdbDoneFn callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new GetTotalPageJob(reinterpret_cast<LsdbJobCallback>(callback), userData));
	queue->Enqueue(job);
	return job;
}

/* 0x811C5D92 */
paf::common::SharedPtr<paf::job::JobItem> applauncherGetIconInfoJobStart(
	paf::job::JobQueue *queue, const paf::string &titleId,
	ApplauncherLsdbDoneFn callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new GetIconInfoJob(titleId, reinterpret_cast<LsdbJobCallback>(callback), userData));
	queue->Enqueue(job);
	return job;
}

/* 0x811C5F38 */
paf::common::SharedPtr<paf::job::JobItem> applauncherSetIconInfoJobStart(
	paf::job::JobQueue *queue, int page, const ApplauncherLsdbIconPtr &icon,
	int flags, ApplauncherLsdbDoneFn callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new SetIconInfoJob(
			page,
			*reinterpret_cast<const paf::common::SharedPtr<LsdbIconInfo> *>(&icon),
			flags, reinterpret_cast<LsdbJobCallback>(callback), userData));
	queue->Enqueue(job);
	return job;
}

/* 0x811C6110 */
paf::common::SharedPtr<paf::job::JobItem> applauncherInsertAppInfoJobStart(
	paf::job::JobQueue *queue, const ApplauncherLsdbAppInfoPtr &appInfo,
	const paf::vector<ApplauncherLsdbAppInfoPtr> &icons,
	int page, const ApplauncherLsdbIconPtr &icon,
	ApplauncherLsdbDoneFn callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new InsertAppInfoJob(
			*reinterpret_cast<const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > *>(&appInfo),
			*reinterpret_cast<const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > *>(&icons),
			page,
			*reinterpret_cast<const paf::common::SharedPtr<LsdbIconInfo> *>(&icon),
			reinterpret_cast<LsdbJobCallback>(callback), userData));
	queue->Enqueue(job);
	return job;
}

/* 0x811C6340 */
paf::common::SharedPtr<paf::job::JobItem> applauncherUpdateAppInfoJobStart(
	paf::job::JobQueue *queue, const ApplauncherLsdbAppInfoPtr &appInfo,
	const paf::vector<ApplauncherLsdbAppInfoPtr> &icons,
	const ApplauncherLsdbIconPtr &icon,
	ApplauncherLsdbDoneFn callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new UpdateAppInfoJob(
			*reinterpret_cast<const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > *>(&appInfo),
			*reinterpret_cast<const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > *>(&icons),
			*reinterpret_cast<const paf::common::SharedPtr<LsdbIconInfo> *>(&icon),
			reinterpret_cast<LsdbJobCallback>(callback), userData));
	queue->Enqueue(job);
	return job;
}

/* 0x811C6568 */
paf::common::SharedPtr<paf::job::JobItem> applauncherUpdateAppInfoDiffJobStart(
	paf::job::JobQueue *queue, const ApplauncherLsdbAppInfoPtr &appInfo,
	const paf::vector<ApplauncherLsdbAppInfoPtr> &icons,
	const ApplauncherLsdbIconPtr &icon, int diffFlags,
	ApplauncherLsdbDoneFn callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new UpdateAppInfoDiffJob(
			*reinterpret_cast<const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > *>(&appInfo),
			*reinterpret_cast<const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > *>(&icons),
			*reinterpret_cast<const paf::common::SharedPtr<LsdbIconInfo> *>(&icon),
			diffFlags, reinterpret_cast<LsdbJobCallback>(callback), userData));
	queue->Enqueue(job);
	return job;
}

/* 0x811C67E2 */
paf::common::SharedPtr<paf::job::JobItem> applauncherCreateNewPageAndInsertAppJobStart(
	paf::job::JobQueue *queue, int page, const LsdbPage &theme,
	const ApplauncherLsdbAppInfoPtr &appInfo,
	const paf::vector<ApplauncherLsdbAppInfoPtr> &icons,
	const ApplauncherLsdbIconPtr &icon,
	ApplauncherLsdbDoneFn callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new CreateNewPageAndInsertAppJob(
			page,
			*reinterpret_cast<const LsdbPageThemeItem *>(&theme),
			*reinterpret_cast<const paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrMapVal> > *>(&appInfo),
			*reinterpret_cast<const paf::vector<paf::common::SharedPtr<LsdbIconInfo> > *>(&icons),
			*reinterpret_cast<const paf::common::SharedPtr<LsdbIconInfo> *>(&icon),
			reinterpret_cast<LsdbJobCallback>(callback), userData));
	queue->Enqueue(job);
	return job;
}

/* 0x811C6BF2 */
paf::common::SharedPtr<paf::job::JobItem> applauncherCreateNewFolderJobStart(
	paf::job::JobQueue *queue, int page,
	const ApplauncherLsdbIconPtr &icon, const ApplauncherLsdbIconPtr &folder,
	const paf::vector<ApplauncherFolderSurface> &surfaces,
	ApplauncherLsdbDoneFn callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new CreateNewFolderJob(
			page,
			*reinterpret_cast<const paf::common::SharedPtr<LsdbIconInfo> *>(&icon),
			*reinterpret_cast<const paf::common::SharedPtr<LsdbIconInfo> *>(&folder),
			*reinterpret_cast<const paf::vector<LsdbFolderSurfacePair> *>(&surfaces),
			reinterpret_cast<LsdbJobCallback>(callback), userData));
	queue->Enqueue(job);
	return job;
}
