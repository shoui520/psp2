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
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <dir/dir.h>
#include <kernel/libkernel.h>
#include <sysmodule/sysmodule.h>
#include <sysmodule/sysmodule_ids.h>
#include <libScePhotoExport/photo_export.h>
#include <libSceMusicExport/music_export.h>
#include <libSceVideoExport/video_export.h>

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

/* the error an export returns when its file is not there (PLACEHOLDER name) */
#define BGDL_ERROR_NO_CONTENT		0x80103A2F

#define BGDL_EXPORT_WORK_SIZE		0x10000

class EseTask;
class BgdlManager;
class BgdlHttpDownload;

BgdlManager *bgdlManagerGet(void);		/* (0x810FD8E0) esetask.cpp */
/* another file's (0x8113B3A0, 0x8113B436; PLACEHOLDER names): a content installed from the
   package at path, and the install stopped */
int Bgdl_8113B3A0(const char *path, const char *contentId);
void Bgdl_8113B436(void);

/* a downloaded file handed to an export library, with its work memory (PLACEHOLDER names) */
class BgdlExporter {
public:
	BgdlExporter(BgdlHttpDownload *download)
		: m_download(download), m_contentId(0), m_mem(NULL), m_result(0)
	{
	}

	/* (0x8141626C) */
	virtual ~BgdlExporter()
	{
		if (m_mem != NULL) {
			sce_paf_free(m_mem);
			m_mem = NULL;
		}
	}

	/* (0x8141628C) what every export checks first */
	virtual int Export(int type, const char *path, SceUInt64 *contentId)
	{
		if (m_result != 0)
			return m_result;
		if (m_mem == NULL) {
			m_result = BGDL_ERROR_NO_MEMORY;
			return m_result;
		}
		if (path == NULL) {
			m_result = BGDL_ERROR_NULL_FOLDER;
			return m_result;
		}
		return 0;
	}

	BgdlHttpDownload *m_download;
	SceUInt64 m_contentId;
	char *m_mem;
	int m_result;
};

/* a photo given to the photo export library (PLACEHOLDER names) */
class BgdlPhotoExporter : public BgdlExporter {
public:
	BgdlPhotoExporter(BgdlHttpDownload *download) : BgdlExporter(download), m_loaded(false)
	{
		m_result = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_PHOTO_EXPORT);
		if (m_result == 0) {
			m_loaded = true;
			m_mem = (char *)sce_paf_malloc(BGDL_EXPORT_WORK_SIZE);
			if (m_mem == NULL)
				m_result = BGDL_ERROR_NO_MEMORY;
		}
	}

	/* (0x814162BC) */
	virtual ~BgdlPhotoExporter()
	{
		if (m_mem != NULL) {
			sce_paf_free(m_mem);
			m_mem = NULL;
		}
		if (m_loaded == true) {
			sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_PHOTO_EXPORT);
			m_loaded = false;
		}
	}

	/* (0x814162FC) the photo exported, its content id kept */
	virtual int Export(int type, const char *path, SceUInt64 *contentId)
	{
		ScePhotoExportParam param;
		int ret;

		ret = BgdlExporter::Export(type, path, contentId);
		if (ret != 0)
			return ret;
		sceClibMemset(&param, 0, sizeof(param));
		param.version = SCE_PSP2_SDK_VERSION;
		param.gameTitle = (const SceChar8 *)"";
		param.gameComment = (const SceChar8 *)"";
		param.photoTitle = NULL;

		paf::string name("");
		paf::string title("");

		ret = Bgdl_81111CA2(path, &name);
		if (ret != 0)
			return ret;
		ret = Bgdl_81111C6A(name.c_str(), &title);
		if (ret != 0)
			return ret;
		param.photoTitle = (const SceChar8 *)title.c_str();
		ret = ScePhotoExport_76640642((const SceChar8 *)path, &param, 0, m_mem, NULL, NULL,
					      (SceInt32 *)&m_contentId);
		if (ret != 0)
			return ret;
		if (contentId != NULL)
			*contentId = m_contentId;
		return 0;
	}

	/* (0x814164CC) the photo exported, where it went written to exportedPath */
	virtual int Export(int type, const char *path, char *exportedPath, int length)
	{
		ScePhotoExportParam param;
		int ret;

		ret = BgdlExporter::Export(type, path, NULL);
		if (ret != 0)
			return ret;
		if (exportedPath == NULL || length == 0)
			return BGDL_ERROR_NULL_FOLDER;
		sceClibMemset(&param, 0, sizeof(param));
		param.version = SCE_PSP2_SDK_VERSION;
		param.gameTitle = (const SceChar8 *)"";
		param.gameComment = (const SceChar8 *)"";
		param.photoTitle = NULL;

		paf::string name("");
		paf::string title("");

		ret = Bgdl_81111CA2(path, &name);
		if (ret != 0)
			return ret;
		ret = Bgdl_81111C6A(name.c_str(), &title);
		if (ret != 0)
			return ret;
		param.photoTitle = (const SceChar8 *)title.c_str();
		ret = ScePhotoExport_719ACE85((const SceChar8 *)path, &param, 0, m_mem, NULL, NULL,
					      (SceChar8 *)exportedPath, length);
		if (ret != 0)
			return ret;
		return 0;
	}

	bool m_loaded;
};

/* what the music and video exports share: BgdlExporter's members, without its table
   (PLACEHOLDER names) */
class BgdlExportWork {
public:
	BgdlExportWork(BgdlHttpDownload *download)
		: m_download(download), m_contentId(0), m_mem(NULL), m_result(0)
	{
	}

	~BgdlExportWork()
	{
		if (m_mem != NULL) {
			sce_paf_free(m_mem);
			m_mem = NULL;
		}
	}

	/* what every export checks first */
	int Check(const char *path)
	{
		if (m_result != 0)
			return m_result;
		if (m_mem == NULL) {
			m_result = BGDL_ERROR_NO_MEMORY;
			return m_result;
		}
		if (path == NULL) {
			m_result = BGDL_ERROR_NULL_FOLDER;
			return m_result;
		}
		return 0;
	}

	BgdlHttpDownload *m_download;
	SceUInt64 m_contentId;
	void *m_mem;
	int m_result;
};

/* a file given to the music export library (PLACEHOLDER names) */
class BgdlMusicExporter : public BgdlExportWork {
public:
	BgdlMusicExporter(BgdlHttpDownload *download) : BgdlExportWork(download), m_loaded(false)
	{
		m_result = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_MUSIC_EXPORT);
		if (m_result == 0) {
			m_loaded = true;
			m_mem = sce_paf_malloc(BGDL_EXPORT_WORK_SIZE);
			if (m_mem == NULL)
				m_result = BGDL_ERROR_NO_MEMORY;
		}
	}

	~BgdlMusicExporter()
	{
		if (m_mem != NULL) {
			sce_paf_free(m_mem);
			m_mem = NULL;
		}
		if (m_loaded == true) {
			sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_MUSIC_EXPORT);
			m_loaded = false;
		}
	}

	int Export(int type, const char *path, SceUInt64 *contentId)
	{
		MusicExportItem item;
		MusicExportResult result;
		int ret;

		ret = Check(path);
		if (ret != 0)
			return ret;
		sce_paf_memset(&item, 0, sizeof(item));
		sce_paf_memset(&result, 0, sizeof(result));
		sce_paf_strncpy(item.path, path, sizeof(item.path));
		ret = SceMusicExport_D1011570(&item, 1, m_mem, NULL, NULL, NULL, 0, &result);
		if (ret != 0)
			return ret;
		m_contentId = (SceUInt64)(SceUInt32)result.contentIdHigh << 32 | (SceUInt32)result.contentIdLow;
		if (contentId != NULL)
			*contentId = m_contentId;
		return 0;
	}

	bool m_loaded;
};

/* a file given to the video export library (PLACEHOLDER names) */
class BgdlVideoExporter : public BgdlExportWork {
public:
	BgdlVideoExporter(BgdlHttpDownload *download) : BgdlExportWork(download), m_loaded(false)
	{
		m_result = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_VIDEO_EXPORT);
		if (m_result == 0) {
			m_loaded = true;
			m_mem = sce_paf_malloc(BGDL_EXPORT_WORK_SIZE);
			if (m_mem == NULL)
				m_result = BGDL_ERROR_NO_MEMORY;
		}
	}

	~BgdlVideoExporter()
	{
		if (m_mem != NULL) {
			sce_paf_free(m_mem);
			m_mem = NULL;
		}
		if (m_loaded == true) {
			sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_VIDEO_EXPORT);
			m_loaded = false;
		}
	}

	/* type 4: PLACEHOLDER, the export's mode 1 */
	int Export(int type, const char *path, SceUInt64 *contentId)
	{
		VideoExportInputParam in;
		VideoExportOutputParam out;
		int ret;

		ret = Check(path);
		if (ret != 0)
			return ret;
		sce_paf_memset(&in, 0, sizeof(in));
		sce_paf_memset(&out, 0, sizeof(out));
		sce_paf_strncpy(in.path, path, sizeof(in.path));
		in.mode = type == 4;
		ret = sceVideoExportFromFile(&in, 1, m_mem, NULL, NULL, NULL, in.mode, &out);
		if (ret != 0)
			return ret;
		m_contentId = (SceUInt64)(SceUInt32)out.contentIdHigh << 32 | (SceUInt32)out.contentIdLow;
		if (contentId != NULL)
			*contentId = m_contentId;
		return 0;
	}

	bool m_loaded;
};

/* a task's download as EseTask::execute runs it, made for each try (PLACEHOLDER names) */
class BgdlHttpDownload {
public:
	BgdlHttpDownload(EseTask *task, int *runStatus) : m_task(task), m_runStatus(runStatus)
	{
	}

	int StartFile(int index);		/* (0x81115CCA) the param file set for file index */
	int NextFile(void);			/* (0x811161DA) */
	int InstallContent(void);		/* (0x811162AA) a type 4 task's content installed */
	int Complete(void);			/* (0x8111644E) the downloaded file put in place */

	/* the file at path exported as a photo; where it went written to exportedPath */
	int ExportPhoto(int type, const char *path, char *exportedPath, int length)
	{
		if (path == NULL || paf::Dir::IsExist(path) != true)
			return BGDL_ERROR_NO_CONTENT;

		BgdlPhotoExporter exporter(this);

		return exporter.Export(type, path, exportedPath, length);
	}

	/* the file at path exported as a photo */
	int ExportPhoto(int type, const char *path, SceUInt64 *contentId)
	{
		if (path == NULL || paf::Dir::IsExist(path) != true)
			return BGDL_ERROR_NO_CONTENT;

		BgdlPhotoExporter exporter(this);

		return exporter.Export(type, path, contentId);
	}

	/* (0x81415C34) the file at path exported as music; Sony's inlines it in one of its two
	   calls */
	__attribute__((noinline)) int ExportMusic(int type, const char *path, SceUInt64 *contentId);
	/* the file at path exported as a video */
	int ExportVideo(int type, const char *path, SceUInt64 *contentId);

	EseTask *m_task;
	int *m_runStatus;
};

inline int BgdlHttpDownload::ExportMusic(int type, const char *path, SceUInt64 *contentId)
{
	if (path == NULL || paf::Dir::IsExist(path) != true)
		return BGDL_ERROR_NO_CONTENT;

	BgdlMusicExporter exporter(this);

	return exporter.Export(type, path, contentId);
}

inline int BgdlHttpDownload::ExportVideo(int type, const char *path, SceUInt64 *contentId)
{
	if (path == NULL || paf::Dir::IsExist(path) != true)
		return BGDL_ERROR_NO_CONTENT;

	BgdlVideoExporter exporter(this);

	return exporter.Export(type, path, contentId);
}

/* another file's (0x8136D828, PLACEHOLDER name): the handle closed */
int Bgdl_8136D828(int handle);

/* a title's folder under ux0:app/ (or the folder given) made for a downloaded package
   (PLACEHOLDER names) */
class BgdlAppFolder {
public:
	BgdlAppFolder(BgdlHttpDownload *download)
		: m_download(download), m_result(0), m_path(""), m_open(false), m_handle(0)
	{
		m_reserved0 = 0;
		m_reserved1 = 0;
	}

	/* (0x81415F90) */
	~BgdlAppFolder()
	{
		if (m_handle >= 0) {
			m_result = Bgdl_8136D828(m_handle);
			m_open = false;
		}
	}

	/* (0x81415AAC) the folder name in base (or ux0:app/) made, removed first when remove */
	__attribute__((noinline)) int Prepare(bool remove, const char *name, int category, const char *base)
	{
		int ret;

		if (base != NULL) {
			m_path = base;
		} else {
			m_path = "ux0:app/";
			if (paf::Dir::IsExist(m_path.c_str()) != true) {
				m_result = paf::Dir::Create(m_path.c_str(), 6);
				if (m_result != 0)
					return m_result;
			}
		}
		if (m_path.length() == 0) {
			m_result = BGDL_ERROR_NULL_FOLDER;
			return m_result;
		}
		if (m_path.c_str()[m_path.length() - 1] != '/')
			m_path += "/";
		m_path += name;
		if (m_path.length() == 0) {
			ret = BGDL_ERROR_NULL_FOLDER;
		} else {
			bool exist = paf::Dir::IsExist(m_path.c_str());

			if (remove == true && exist == true) {
				paf::Dir::RemoveRecursive(m_path.c_str());
				exist = false;
			}
			ret = 0;
			if (exist != true)
				ret = paf::Dir::Create(m_path.c_str(), 6);
		}
		m_result = ret;
		return ret;
	}

	paf::string GetPath(void)
	{
		return m_path;
	}

	BgdlHttpDownload *m_download;
	int m_result;
	paf::string m_path;
	bool m_open;
	int m_reserved0;		/* PLACEHOLDER: set by the package's unpack (0x81119152) */
	int m_reserved1;
	int m_handle;
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
