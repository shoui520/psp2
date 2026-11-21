/*
 * system_update_file.h: the system update's local files (system_update_file.cpp, Sony's file by
 * 0.990's records): the update's information file and the update itself, and the errors their
 * calls give. Every name is ours (PLACEHOLDER) until a record or string names it.
 */
#ifndef _SYSTEM_UPDATE_FILE_H
#define _SYSTEM_UPDATE_FILE_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <file/file.h>
#include <system_update/system_update_job.h>
#include <system_update/system_update_registry.h>

/* an update being fetched, as its information file keeps it (PLACEHOLDER names) */
struct SystemUpdateInfo {
	SystemUpdateInfo() : offset(0), size(0), state(1), done(false)
	{
	}

	char version[32];
	char url[1024];			/* where the update is read from */
	char lastModified[32];		/* its date */
	SceInt64 offset;		/* what of it is there */
	SceInt64 size;
	int state;			/* 0 none, 1 downloading, 2 downloaded */
	int type;			/* the SystemUpdateCtrl's */
	bool done;
};

/* the update's state, as the update's files give it (PLACEHOLDER names) */
struct SystemUpdateStatus {
	SystemUpdateStatus() : state(3), value(0)
	{
		version.erase(0, paf::string::npos);
		url.erase(0, paf::string::npos);
	}

	int state;			/* 0 started, 1 none yet, 2 downloaded, 3 none */
	SceInt64 value;			/* what of it is there */
	paf::string version;		/* the information's version, when there is an update */
	paf::string url;		/* where it is read from */
	int type;			/* the SystemUpdateCtrl's */
};

/* the update's local files: its information at m_infoPath, the update at m_pupPath (PLACEHOLDER) */
class SystemUpdateCtrl {
public:
	SystemUpdateCtrl(int type, const char *infoPath, const char *pupPath);

	~SystemUpdateCtrl();
	SystemUpdateResult Bgdl_811310CA(void);				/* removes them */
	SystemUpdateResult Bgdl_81131168(SystemUpdateInfo info);	/* the information written */
	SystemUpdateResult Bgdl_81131280(SystemUpdateInfo info);	/* made and written */
	SystemUpdateResult Bgdl_81131A18(paf::string version, paf::string url, paf::string lastModified,
					 SceInt64 offset, SceInt64 size, int state,
					 SystemUpdateInfo *info);
	bool Bgdl_81131ABC(void);					/* the update is there */
	SystemUpdateResult Bgdl_81131AC6(SystemUpdateInfo *info, bool *there, bool resuming);
	bool Bgdl_81131B54(void);					/* its information is there */
	SystemUpdateResult Bgdl_81131B5E(SystemUpdateInfo *info, bool keep);	/* read */
	SystemUpdateResult Bgdl_81131CE6(int state, SystemUpdateInfo *info, SceInt64 offset);
	SystemUpdateResult Bgdl_81132558(SystemUpdateStatus *status, bool keep, bool resuming);
	SystemUpdateResult Bgdl_81132734(SystemUpdateStatus *status);	/* the update's state */
	SystemUpdateResult Bgdl_8113276C(paf::string url, paf::string lastModified, SceInt64 size,
					 bool *ok);

	int m_type;			/* the information's kind */
	const char *m_infoPath;
	const char *m_pupPath;
	paf::string m_string1;		/* PLACEHOLDER: not read */
	paf::string m_string2;		/* PLACEHOLDER: not read */
	paf::string m_string3;		/* PLACEHOLDER: not read */
	SceInt64 m_offset;		/* where the download starts */
	int reserved[4];
};

int Bgdl_81130840(int error, const char *path);
int Bgdl_81130C9C(int type, int error);
SystemUpdateResult Bgdl_81130D9E(paf::common::SharedPtr<paf::File> file, int error,
				 SceInt64 knownSize);
int Bgdl_81130EC4(const char *path);
SystemUpdateResult Bgdl_81130ECC(const char *path);
SystemUpdateResult Bgdl_81130F2A(const char *path);
void Bgdl_81132986(bool value);
void Bgdl_81132992(void);
void Bgdl_811329A2(SystemUpdateClient *client);

/* other files' */
const char *Bgdl_8137C484(int reserved);

#endif
