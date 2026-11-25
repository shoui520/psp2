/*
 * SceShell shell_svc: Activity / Status service internal types
 */

#ifndef SHELL_SVC_ACTIVITY_H
#define SHELL_SVC_ACTIVITY_H

#include <scetypes.h>
#include <std/string>
#include <ipmi/session.h>
#include <job/job.h>

struct ShellSvcActivityRecord {
	paf::string m_id;					/* +0x00 */
	int m_f0c;						/* +0x0C (init 3) */
	SceInt64 m_f10;						/* +0x10 */
	int m_f18;						/* +0x18 (init -1) */
	int m_type;						/* +0x1C */
	SceInt64 m_f20;						/* +0x20 */
	SceInt64 m_f28;						/* +0x28 */
	SceUInt8 m_f30;						/* +0x30 */
	SceUInt8 m_f31;						/* +0x31 */
	paf::string m_titleId;					/* +0x34 */
	paf::string m_titleName;				/* +0x40 */
	paf::string m_contentId;				/* +0x4C */
	paf::string m_body;					/* +0x58 */
	int m_f64[3];						/* +0x64 */
	paf::string m_uri;					/* +0x70 */
	paf::string m_iconUri;					/* +0x7C */
	paf::string m_f88;					/* +0x88 */
	SceUInt32 m_color;					/* +0x94 (init 0x989898FF) */
	paf::string m_f98;					/* +0x98 */

	ShellSvcActivityRecord()
		: m_f0c(3), m_f10(0), m_f18(-1), m_type(0),
		  m_f20(0), m_f28(0), m_f30(0), m_f31(0),
		  m_color(0x989898FFu)
	{
		m_f64[0] = 0;
		m_f64[1] = 0;
		m_f64[2] = 0;
	}
};

/* Base activity job (0x811BDF1C Run, 0x811BDF40 Finish) */
class ShellSvcActivityJobBase : public paf::job::JobItem {
public:
	ShellSvcActivityJobBase(const char *name, int kind, const ShellSvcActivityRecord &rec)
		: paf::job::JobItem(name, NULL), m_kind(kind), m_rec(rec) {}
	virtual ~ShellSvcActivityJobBase() {}
	virtual int Run();					/* 0x811BDF1C */
	virtual void Finish(int);				/* 0x811BDF40 */
	virtual int Prepare() = 0;				/* vtable +20 */

	int m_kind;						/* +28 */
	ShellSvcActivityRecord m_rec;				/* +32 (size 168 -> +200) */
};

class PostSystemSoftwareStatusJob : public ShellSvcActivityJobBase {
public:
	PostSystemSoftwareStatusJob(const ShellSvcActivityRecord &rec)
		: ShellSvcActivityJobBase("PostSystemSoftwareStatusJob", 0, rec) {}
	virtual ~PostSystemSoftwareStatusJob() {}
	virtual int Prepare();					/* 0x811BDFE4 */
};

class PostTrophyStatusJob : public ShellSvcActivityJobBase {
public:
	PostTrophyStatusJob(int kind, const ShellSvcActivityRecord &rec,
		const paf::string &gameName, const paf::string &trophyName, int grade)
		: ShellSvcActivityJobBase("PostSystemSoftwareStatusJob", kind, rec),
		  m_gameName(gameName), m_trophyName(trophyName), m_grade(grade) {}
	virtual ~PostTrophyStatusJob() {}
	virtual int Prepare();					/* 0x811BEB56 */

	paf::string m_gameName;					/* +200 */
	paf::string m_trophyName;				/* +212 */
	int m_grade;						/* +224 */
};

class PostStoreStatusJob : public ShellSvcActivityJobBase {
public:
	PostStoreStatusJob(int kind, const ShellSvcActivityRecord &rec,
		const paf::string &contentId, int isMedia,
		const paf::string &itemName, const paf::string &subName,
		int count, int isPreorder, int mediaKind, int isFree)
		: ShellSvcActivityJobBase("PostSystemSoftwareStatusJob", kind, rec),
		  m_contentId(contentId), m_isMedia(isMedia),
		  m_itemName(itemName), m_subName(subName),
		  m_count(count), m_isPreorder(isPreorder),
		  m_mediaKind(mediaKind), m_isFree(isFree) {}
	virtual ~PostStoreStatusJob() {}
	virtual int Prepare();					/* 0x811BF95E */

	paf::string m_contentId;				/* +200 */
	int m_isMedia;						/* +212 */
	paf::string m_itemName;					/* +216 */
	paf::string m_subName;					/* +228 */
	int m_count;						/* +240 */
	int m_isPreorder;					/* +244 */
	int m_mediaKind;					/* +248 */
	int m_isFree;						/* +252 */
};

class PostGameStatusJob : public paf::job::JobItem {
public:
	PostGameStatusJob(const ShellSvcActivityRecord &rec, SceUInt8 buildUri,
		SceUInt32 reqVer, const paf::string &param)
		: paf::job::JobItem("PostGameStatusJob", NULL),
		  m_pad(0), m_rec(rec), m_buildUri(buildUri),
		  m_reqVer(reqVer), m_param(param) {}
	virtual ~PostGameStatusJob() {}
	virtual int Run();					/* 0x811BDB86 */
	virtual void Finish(int) {}

	int m_pad;						/* +28 */
	ShellSvcActivityRecord m_rec;				/* +32 */
	SceUInt8 m_buildUri;					/* +200 */
	SceUInt32 m_reqVer;					/* +204 */
	paf::string m_param;					/* +208 */
};

/* Cross-TU activity functions */
extern int shellSvcActivityPostSystemSoftware(IPMI::Session *session, const char *titleId); /* 0x811B99E0 */
extern int shellSvcActivityPostTrophy(IPMI::Session *session, const char *titleId, const char *gameName,
	const char *trophyName, int grade, const char *uri);	/* 0x811B99EA */
extern int shellSvcActivityPostHiddenTrophy(IPMI::Session *session, const char *titleId,
	const char *gameName, const char *uri);			/* 0x811B9F8E */
extern int shellSvcActivityPostPlatinumTrophy(IPMI::Session *session, const char *titleId,
	const char *gameName, const char *uri, const char *trophyName); /* 0x811BA7DA */
extern int shellSvcActivityPostStorePurchase(IPMI::Session *session, const char *contentId, int isMedia,
	const char *itemName, const char *subName, const char *uri, const char *iconUri,
	int isPreorder, int mediaKind, int isFree);		/* 0x811BAD7C */
extern int shellSvcActivityPostStoreRating(IPMI::Session *session, const char *contentId, int isMedia,
	const char *itemName, int count, const char *uri, const char *iconUri); /* 0x811BB362 */
extern int shellSvcActivityPostGameStatus(IPMI::Session *session, int actType, int useAppMgr,
	const char *titleId, const char *contentId, const char *body,
	const char *iconUri, const char *uri);			/* 0x811BB91E */
extern int shellSvcActivityPostGameStatusWithUri(IPMI::Session *session, int actType,
	const char *body, SceUInt32 reqVer, const char *param);	/* 0x811BC434 */

extern int shellSvcActivityDispatchSync(IPMI::Session *session, SceUInt32 methodId,
	const IPMI::DataInfo *inBufs, SceUInt32 inCount,
	IPMI::BufferInfo *outBufs, SceUInt32 outCount);		/* 0x811BCFBA */
extern int shellSvcActivityPostSystemSoftwareImpl(const char *titleId); /* 0x811BD168 */

extern int shellSvcActivityBuildUri(ShellSvcActivityRecord *rec, SceUInt32 reqVer,
	const paf::string *param);				/* 0x811BD998 */
extern int shellSvcActivityCheckEnabled(void);			/* 0x811C00DA */

#endif /* SHELL_SVC_ACTIVITY_H */
