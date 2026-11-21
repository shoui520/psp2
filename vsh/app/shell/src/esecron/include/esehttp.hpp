/*
 * esehttp.hpp: the background download library's HTTP side (Sony's esehttp.cpp, named by the
 * 0.990 prototype's records; 0x81114642 on): a download's request query (downloadId, the
 * digest, the server's address, the download's type) and what the content server answered.
 * No record names the classes or their functions: the names are ours (PLACEHOLDER).
 */
#ifndef ESECRON_ESEHTTP_HPP
#define ESECRON_ESEHTTP_HPP

#include <stddef.h>
#include <scetypes.h>
#include <std/string>

/* the parameters a request's query may carry, each set for some kinds of request */
enum BgdlQueryParam {
	BGDL_QUERY_DOWNLOAD_ID = 2,	/* downloadId=%08x */
	BGDL_QUERY_DIGEST,		/* q=, the digest in hex */
	BGDL_QUERY_SERVER_IP,		/* serverIpAddr= */
	BGDL_QUERY_DOWNLOAD_TYPE,	/* downloadType=fs, fm, bg, pg, st, vf, vb */
	BGDL_QUERY_INSTANT,		/* ptype=instant */
	BGDL_QUERY_START,		/* start=1 */
	BGDL_QUERY_PUSH,		/* push=true */
	BGDL_QUERY_PARAM_COUNT
};

#define BGDL_REQUEST_KIND_COUNT	6
#define BGDL_DIGEST_SIZE	32

/* a download's request: the query a package's URL is asked with (PLACEHOLDER names) */
class BgdlHttpRequest {
public:
	BgdlHttpRequest();						/* (0x81114642) */

	const char *GetQuery(void);					/* (0x811146BE) */
	static int GetHost(const char *url, paf::string *host);	/* (0x811146CC) */
	int ResolveServer(const char *url);				/* (0x8111493C) */
	int SetDigest(const char *digest);				/* (0x81114A26) */
	void SetDownloadId(SceUInt32 id);				/* (0x81114A8C) */
	void Setup(int type, int kind, const char *url, SceUInt32 downloadId,
		   const char *digest, const char *extra, bool start, bool push);	/* (0x81114AFC) */
	SceUInt32 Bgdl_8118FD0C(void);					/* (0x8118FD0C) another file's: m_downloadId */
	int GetDigest(char *digest);					/* (0x81115C22) */

	int m_kind;			/* the column of the query's table, -1: not set up */
	int m_type;			/* downloadType's, 0-6 */
	paf::string m_reserved0;	/* PLACEHOLDER: only cleared */
	paf::string m_reserved1;
	paf::string m_extra;		/* added to the query as it is */
	SceUInt32 m_downloadId;
	int m_hasDigest;
	SceUInt8 m_digest[BGDL_DIGEST_SIZE];
	paf::string m_serverIpAddr;	/* the URL's host, resolved */
	bool m_start;
	bool m_push;
	paf::string m_query;
};

/* a value's size and alignment, as a table elsewhere is made of them (PLACEHOLDER names) */
struct BgdlSizeAlign {
	BgdlSizeAlign(int size, int align);				/* (0x81115CC4) */

	int size;
	int align;
};

/* fills digest with the console's (0x812218A2, PLACEHOLDER name) */
int Bgdl_812218A2(SceUInt8 *digest);

/* the error a download step returns to be run again from the start (PLACEHOLDER name) */
#define BGDL_ERROR_RETRY		0x80103A34

class EseTask;
class BgdlManager;

BgdlManager *bgdlManagerGet(void);		/* (0x810FD8E0) esetask.cpp */
/* another file's (0x8113B3A0, 0x8113B436; PLACEHOLDER names): a content installed from the
   package at path, and the install stopped */
int Bgdl_8113B3A0(const char *path, const char *contentId);
void Bgdl_8113B436(void);

/* a task's download as EseTask::execute runs it, made for each try (PLACEHOLDER names) */
class BgdlHttpDownload {
public:
	BgdlHttpDownload(EseTask *task, int *runStatus) : m_task(task), m_runStatus(runStatus)
	{
	}

	int StartFile(int index);		/* (0x81115CCA) the param file set for file index */
	int NextFile(void);			/* (0x811161DA) */
	int InstallContent(void);		/* (0x811162AA) a type 4 task's content installed */

	EseTask *m_task;
	int *m_runStatus;
};

/* the context the manager's cancel handler gets while a content is installed (PLACEHOLDER) */
struct BgdlHttpCancel {
	/* (0x81416724) the install stopped once the manager is cancelled */
	__attribute__((noinline)) static void OnCancel(void *arg)
	{
		BgdlHttpCancel *cancel = (BgdlHttpCancel *)arg;

		if (cancel != NULL && cancel->cancelled != true &&
		    bgdlManagerGet()->IsCancelled()) {
			cancel->cancelled = true;
			Bgdl_8113B436();
		}
	}

	BgdlHttpDownload *download;
	bool running;
	bool cancelled;
};

/* what the content server's answer gives of a package (PLACEHOLDER names) */
struct BgdlServerData {
	int reserved0;
	int category;			/* 6, 7, 14-16: a game's (key 114 1); 24, 29, 31 */
	SceUInt32 attribute;		/* 0x10, bit 31: no patch looked for */
	char reserved1[28];
	char contentId[48];		/* the package's key 217 */
	char titleId[32];		/* key 220, the folder it goes to */
};

/* what the content server answered (PLACEHOLDER names) */
struct BgdlServerInfo {
	BgdlServerInfo()
	{
		Clear();
	}

	void Clear(void);		/* (0x81115C58) */

	bool flag;
	int status;
	int httpStatus;
	int reserved0;
	SceUInt64 size;
	paf::string url;
	paf::string titleId;
	paf::string contentId;
	paf::string message;		/* the server's, with an HTTP 401 */
	SceUInt64 value0;
	SceUInt64 value1;
	SceUInt64 value2;
	SceUInt64 value3;
	SceUInt64 value4;
	int kind;
	int reserved1;
	BgdlServerData data;
};

#endif
