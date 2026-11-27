/*
 * SceShell shell_svc: Activity URI builder, text sanitizer & status jobs
 * 3.65-CEX: 0x811BD998 - 0x811C0164 (sti 0x811C00F4)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/map>
#include <std/string>
#include <common/shared_ptr.h>
#include <common/string_util.h>
#include <plugin.h>

#include "../../../plugins.h"
#include "../../livespace_db/lsdb_keys.h"
#include "shell_svc_activity.h"

struct LsdbAttrVal {
	int type;
	int pad;
	union {
		SceInt64 intVal;
		paf::string *str;
	};
};

extern "C" {
extern void SceLsdb_A6D8772C(void *ctx);
extern int SceLsdb_E90B81CD(void *ctx);
extern void SceLsdb_DEF5C3F9(void *ctx);
extern void SceLsdb_E2101B03(void *ctx);
extern int SceLsdb_D239031A(void *ctx, const paf::string *titleId,
	paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrVal> > *row);
extern int SceLsdb_F4325AD0(paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrVal> > *row);
extern const char *sceLsdbGetStitle(paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrVal> > *row);
extern const char *sceLsdbGetTitle(paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrVal> > *row);
extern int sceHttpUriEscape(char *out, SceSize *require, SceSize prepare, const char *in);
extern SceSize sce_paf_wcsnlen(const wchar_t *s, SceSize maxlen);
}

#include <activity_db/activity_db_internal.h>

static inline int shellSvcJobPostActivityStatus(void *db, const ShellSvcActivityRecord *rec)
{
	return static_cast<ActivityDbNetDaemon *>(db)->PostStatus(
		reinterpret_cast<const ActivityDbStatus *>(rec), 0x7FFFFFFFFFFFFFFFLL);
}

extern int npAccountIsSignedIn(void);			/* 0x81326D30 */

/* 0x811BD998 */
int shellSvcActivityBuildUri(ShellSvcActivityRecord *rec, SceUInt32 reqVer, const paf::string *param)
{
	char buf[0x401];
	sce_paf_memset(buf, 0, sizeof(buf));
	char *p = buf + sce_paf_snprintf(buf, 0x400, "psgm:play?type=ACTIVITY");
	if (rec->m_titleId.size() != 0)
		p += sce_paf_snprintf(p, sizeof(buf) - (SceSize)(p - buf), "&titleid=%s", rec->m_titleId.c_str());
	if (rec->m_contentId.size() != 0)
		p += sce_paf_snprintf(p, sizeof(buf) - (SceSize)(p - buf), "&content=%s", rec->m_contentId.c_str());
	if (reqVer != 0)
		p += sce_paf_snprintf(p, sizeof(buf) - (SceSize)(p - buf), "&reqver=%02d.%02d",
			reqVer / 100u, reqVer % 100u);
	if (param->size() != 0)
		sce_paf_snprintf(p, sizeof(buf) - (SceSize)(p - buf), "&param=%s", param->c_str());
	buf[0x400] = '\0';
	rec->m_uri = buf;
	return 0;
}

/* 0x811BDAA8 */
void shellSvcActivityStripNewlines(paf::string &s)
{
	SceSize pos;
	while ((pos = s.find("\n")) != paf::string::npos)
		s.replace(pos, 1, " ");
}

/* 0x811BDB86 */
int PostGameStatusJob::Run()
{
	char ctx[4];
	SceLsdb_A6D8772C(ctx);
	int r = SceLsdb_E90B81CD(ctx);
	if (r < 0) {
		SceLsdb_E2101B03(ctx);
		return r;
	}

	paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrVal> > row(
		new paf::map<SceUInt32, LsdbAttrVal>());
	r = SceLsdb_D239031A(ctx, &m_rec.m_titleId, &row);
	SceLsdb_DEF5C3F9(ctx);
	if (r < 0) {
		m_rec.m_titleName = "";
		m_rec.m_contentId = "";
	} else {
		if (!SceLsdb_F4325AD0(&row)) {
			SceLsdb_E2101B03(ctx);
			return (int)0x80101041;
		}
		const char *stitle = sceLsdbGetStitle(&row);
		m_rec.m_titleName = stitle ? stitle : "";
		if (m_rec.m_titleName.size() == 0) {
			const char *title = sceLsdbGetTitle(&row);
			m_rec.m_titleName = title ? title : "";
		}
		if (m_rec.m_titleName.size() != 0)
			shellSvcActivityStripNewlines(m_rec.m_titleName);
		else
			m_rec.m_titleName = "";

		paf::map<SceUInt32, LsdbAttrVal>::iterator it = row->find(0x4D6FEFCAu);
		if (it == row->end()) {
			m_rec.m_contentId = "";
		} else {
			const char *cid = (it->second.type == 1 && it->second.str != NULL)
				? it->second.str->c_str() : "";
			m_rec.m_contentId = cid ? cid : "";
		}
	}

	if (m_buildUri != 0) {
		r = shellSvcActivityBuildUri(&m_rec, m_reqVer, &m_param);
		if (r < 0) {
			SceLsdb_E2101B03(ctx);
			return r;
		}
	}

	r = shellSvcJobPostActivityStatus(SceActivityDb_7B68F69D(), &m_rec);
	SceLsdb_E2101B03(ctx);
	return r;
}

/* 0x811BDF1C */
int ShellSvcActivityJobBase::Run()
{
	int r = Prepare();
	if (r >= 0)
		return shellSvcJobPostActivityStatus(SceActivityDb_7B68F69D(), &m_rec);
	return r;
}

/* 0x811BDF40 */
void ShellSvcActivityJobBase::Finish(int)
{
}

/* 0x811BDF42 */
int shellSvcActivitySetContentId(paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrVal> > *row,
	ShellSvcActivityRecord *rec)
{
	if (sce_paf_strcmp(rec->m_titleId.c_str(), "NPXS10007") == 0) {
		rec->m_contentId = "";
		return 0;
	}
	paf::map<SceUInt32, LsdbAttrVal>::iterator it = (*row)->find(0x4D6FEFCAu);
	if (it == (*row)->end()) {
		rec->m_contentId = "";
	} else {
		const char *cid = (it->second.type == 1 && it->second.str != NULL)
			? it->second.str->c_str() : "";
		rec->m_contentId = cid ? cid : "";
	}
	return 0;
}

static int getLiveAreaString(SceUInt32 hash, paf::wstring &out)
{
	paf::IDParam id(hash);
	paf::Plugin *plugin = paf::Plugin::Find("livearea_plugin");
	if (plugin == NULL)
		return (int)0x80101005;
	const wchar_t *ws = plugin->GetString(id);
	out = ws ? ws : L"";
	return 0;
}

/* 0x811BDFE4 */
int PostSystemSoftwareStatusJob::Prepare()
{
	paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrVal> > row(
		new paf::map<SceUInt32, LsdbAttrVal>());
	char ctx[4];
	SceLsdb_A6D8772C(ctx);
	int r = SceLsdb_E90B81CD(ctx);
	if (r >= 0) {
		r = SceLsdb_D239031A(ctx, &m_rec.m_titleId, &row);
		SceLsdb_DEF5C3F9(ctx);
	}
	SceLsdb_E2101B03(ctx);
	if (r < 0)
		return r;

	if (!SceLsdb_F4325AD0(&row))
		return (int)0x80101041;

	const char *stitle = sceLsdbGetStitle(&row);
	m_rec.m_titleName = stitle ? stitle : "";
	if (m_rec.m_titleName.size() == 0) {
		const char *title = sceLsdbGetTitle(&row);
		m_rec.m_titleName = title ? title : "";
		if (m_rec.m_titleName.size() == 0)
			return (int)0x80101041;
	}
	shellSvcActivityStripNewlines(m_rec.m_titleName);

	r = shellSvcActivitySetContentId(&row, &m_rec);
	if (r < 0)
		return r;

	const char *cleanTitleStr = sceLsdbGetTitle(&row);
	paf::string titleClean(cleanTitleStr ? cleanTitleStr : "");
	if (titleClean.size() == 0)
		return (int)0x80101041;
	shellSvcActivityStripNewlines(titleClean);

	const char *rawTitleStr = sceLsdbGetTitle(&row);
	paf::string titleRaw(rawTitleStr ? rawTitleStr : "");
	if (titleRaw.size() == 0)
		return (int)0x80101041;

	char *escBuf = NULL;
	paf::string escaped("");
	if (titleRaw.size() != 0) {
		SceSize req = 0;
		if (sceHttpUriEscape(NULL, &req, 0, titleRaw.c_str()) == 0 && req != 0) {
			escBuf = new char[req];
			sce_paf_memset(escBuf, 0, req);
			if (sceHttpUriEscape(escBuf, &req, req, titleRaw.c_str()) == 0)
				escaped = escBuf;
		}
	}

	char uriBuf[0x401];
	sce_paf_memset(uriBuf, 0, sizeof(uriBuf));
	char *p = uriBuf + sce_paf_snprintf(uriBuf, 0x400, "psgm:open?titleid=%s", m_rec.m_titleId.c_str());
	if (escaped.size() != 0)
		p += sce_paf_snprintf(p, sizeof(uriBuf) - (SceSize)(p - uriBuf), "&titlename=%s", escaped.c_str());
	if (m_rec.m_contentId.size() != 0)
		sce_paf_snprintf(p, sizeof(uriBuf) - (SceSize)(p - uriBuf), "&content=%s", m_rec.m_contentId.c_str());
	uriBuf[0x400] = '\0';
	m_rec.m_uri = uriBuf;
	if (escBuf != NULL)
		delete[] escBuf;

	paf::wstring fmt;
	r = getLiveAreaString(0xF3046794u, fmt);
	if (r < 0)
		return r;

	paf::wstring titleW;
	paf::common::Utf8ToUtf16(titleClean.c_str(), &titleW);
	paf::wstring bodyW = shellFormatText(fmt.c_str(), titleW.c_str());
	if (sce_paf_wcsnlen(bodyW.c_str(), 257) > 256) {
		bodyW = bodyW.substr(0, 253);
		bodyW.append(L"...");
	}
	paf::string bodyUtf8;
	paf::common::Utf16ToUtf8(bodyW, &bodyUtf8);
	m_rec.m_body = bodyUtf8;
	return 0;
}

/* 0x811BE842 */
int shellSvcActivityGetTrophyGradeText(int kind, int grade, paf::wstring &out)
{
	if (kind == 2)
		return getLiveAreaString(0x93A15826u, out);
	switch (grade) {
	case 1: return getLiveAreaString(0x73394904u, out);
	case 2: return getLiveAreaString(0x783FEE2Eu, out);
	case 3: return getLiveAreaString(0x9E13A191u, out);
	case 4: return getLiveAreaString(0xB6398A70u, out);
	default: return (int)0x80101041;
	}
}

/* 0x811BEB56 */
int PostTrophyStatusJob::Prepare()
{
	paf::common::SharedPtr<paf::map<SceUInt32, LsdbAttrVal> > row(
		new paf::map<SceUInt32, LsdbAttrVal>());
	if (sce_paf_strncmp(m_rec.m_titleId.c_str(), "NPXS10999", 0x20) != 0 &&
	    sce_paf_strncmp(m_rec.m_titleId.c_str(), "NPXS10998", 0x20) != 0) {
		char ctx[4];
		SceLsdb_A6D8772C(ctx);
		int r = SceLsdb_E90B81CD(ctx);
		if (r >= 0) {
			r = SceLsdb_D239031A(ctx, &m_rec.m_titleId, &row);
			SceLsdb_DEF5C3F9(ctx);
		}
		SceLsdb_E2101B03(ctx);
		if (r >= 0) {
			const char *stitle = sceLsdbGetStitle(&row);
			m_rec.m_titleName = stitle ? stitle : "";
			if (m_rec.m_titleName.size() == 0) {
				const char *title = sceLsdbGetTitle(&row);
				m_rec.m_titleName = title ? title : "";
			}
			if (m_rec.m_titleName.size() != 0)
				shellSvcActivityStripNewlines(m_rec.m_titleName);
			else
				m_rec.m_titleName = "";
			if (shellSvcActivitySetContentId(&row, &m_rec) < 0)
				m_rec.m_contentId = "";
		} else {
			m_rec.m_titleName = "";
			m_rec.m_contentId = "";
		}
	} else {
		m_rec.m_contentId = "";
	}

	paf::string iconUri;
	if (m_kind == 1) {
		switch (m_grade) {
		case 2: iconUri = "psactivity:icon?type=trophy&grade=gold"; break;
		case 3: iconUri = "psactivity:icon?type=trophy&grade=silver"; break;
		case 4: iconUri = "psactivity:icon?type=trophy&grade=bronze"; break;
		default: return (int)0x80101041;
		}
	} else if (m_kind == 2) {
		iconUri = "psactivity:icon?type=trophy&grade=hidden";
	} else if (m_kind == 3) {
		iconUri = "psactivity:icon?type=trophy&grade=platinum";
	}
	m_rec.m_iconUri = iconUri;

	SceUInt32 msgHash;
	if (m_kind == 1 || m_kind == 2)
		msgHash = 0xC5CA08B9u;
	else if (m_kind == 3)
		msgHash = 0xCFF033DDu;
	else
		return (int)0x80101041;

	paf::wstring fmt;
	int r = getLiveAreaString(msgHash, fmt);
	if (r < 0)
		return r;

	shellSvcActivityStripNewlines(m_gameName);
	paf::wstring gameW;
	paf::common::Utf8ToUtf16(m_gameName.c_str(), &gameW);
	paf::wstring bodyW;
	paf::wstring trophyW;
	paf::wstring gradeW;

	if (m_kind == 1) {
		bodyW = shellFormatText(fmt.c_str(), gameW.c_str());
		shellSvcActivityStripNewlines(m_trophyName);
		paf::common::Utf8ToUtf16(m_trophyName.c_str(), &trophyW);
		r = shellSvcActivityGetTrophyGradeText(m_kind, m_grade, gradeW);
		if (r < 0)
			return r;
		bodyW = shellFormatText(L"%1 %2 (%3)", bodyW.c_str(), trophyW.c_str(), gradeW.c_str());
	} else if (m_kind == 2) {
		bodyW = shellFormatText(fmt.c_str(), gameW.c_str());
		r = shellSvcActivityGetTrophyGradeText(m_kind, m_grade, gradeW);
		if (r < 0)
			return r;
		bodyW = shellFormatText(L"%1 (%2)", bodyW.c_str(), gradeW.c_str());
	} else if (m_kind == 3) {
		bodyW = shellFormatText(fmt.c_str(), gameW.c_str());
		if (m_trophyName.size() != 0) {
			shellSvcActivityStripNewlines(m_trophyName);
			paf::common::Utf8ToUtf16(m_trophyName.c_str(), &trophyW);
			r = shellSvcActivityGetTrophyGradeText(m_kind, m_grade, gradeW);
			if (r < 0)
				return r;
			bodyW = shellFormatText(L"%1 %2 (%3)", bodyW.c_str(), trophyW.c_str(), gradeW.c_str());
		}
	} else {
		return (int)0x80101041;
	}

	if (sce_paf_wcsnlen(bodyW.c_str(), 257) > 256) {
		bodyW = bodyW.substr(0, 253);
		bodyW.append(L"...");
	}
	paf::string bodyUtf8;
	paf::common::Utf16ToUtf8(bodyW, &bodyUtf8);
	m_rec.m_body = bodyUtf8;
	return 0;
}

/* 0x811BF95E */
int PostStoreStatusJob::Prepare()
{
	if (m_isMedia != 0) {
		m_rec.m_titleId = "NPXS01008";
	} else {
		if (m_contentId.size() < 16)
			return (int)0x80101041;
		m_rec.m_titleId = m_contentId.substr(7, 9);
	}
	m_rec.m_contentId = "";

	if (m_kind != 4 && m_kind != 5)
		return (int)0x80101041;

	paf::string itemCopy = m_itemName;
	paf::wstring itemW;
	paf::common::Utf8ToUtf16(itemCopy.c_str(), &itemW);

	SceUInt32 msgHash = 0;
	if (m_kind == 4) {
		if (m_mediaKind == 0) {
			msgHash = m_isPreorder ? 0x272A5FCDu : (m_isFree ? 0x9553B3F9u : 0x0F573462u);
		} else if (m_mediaKind == 1) {
			msgHash = m_isPreorder ? 0x12194889u : (m_isFree ? 0x9194BFEBu : 0x93A35CCDu);
		} else if (m_mediaKind == 2) {
			msgHash = m_isPreorder ? 0x0B682F67u : (m_isFree ? 0xD84B37B5u : 0x39B84BCBu);
		} else {
			return (int)0x80101041;
		}
	} else if (m_kind == 5) {
		msgHash = (m_count == 1) ? 0xBDEE67BAu : 0x65771825u;
	} else {
		return (int)0x80101041;
	}

	paf::wstring fmt;
	int r = getLiveAreaString(msgHash, fmt);
	if (r < 0)
		return r;

	paf::wstring bodyW;
	if (m_kind == 4) {
		bodyW = shellFormatText(fmt.c_str(), itemW.c_str());
	} else if (m_kind == 5) {
		if (m_count == 1) {
			bodyW = shellFormatText(fmt.c_str(), itemW.c_str());
		} else {
			char numBuf[8];
			sce_paf_memset(numBuf, 0, sizeof(numBuf));
			paf::wstring numW(L"");
			if ((SceUInt32)m_count <= 5u) {
				sce_paf_snprintf(numBuf, sizeof(numBuf), "%d", m_count);
				paf::string numStr(numBuf);
				paf::common::Utf8ToUtf16(numStr.c_str(), &numW);
			}
			bodyW = shellFormatText(fmt.c_str(), itemW.c_str(), numW.c_str());
		}
	} else {
		return (int)0x80101041;
	}

	if (sce_paf_wcsnlen(bodyW.c_str(), 257) > 256) {
		bodyW = bodyW.substr(0, 253);
		bodyW.append(L"...");
	}
	paf::string bodyUtf8;
	paf::common::Utf16ToUtf8(bodyW, &bodyUtf8);
	m_rec.m_body = bodyUtf8;
	return 0;
}

/* 0x811C00DA */
int shellSvcActivityCheckEnabled(void)
{
	int r = npAccountIsSignedIn();
	if (r == 1)
		return 0;
	if (r == 0)
		return (int)0x80552C06;
	return r;
}
