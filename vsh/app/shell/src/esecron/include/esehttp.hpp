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
#include <timer/timer.h>
#include <common/state_check.h>
#include <kernel/libkernel.h>
#include <kernel/sysmem/memblock.h>
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

	__attribute__((noinline)) const char *GetQuery(void);		/* (0x811146BE) */
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

/* PLACEHOLDER names: what a content's download finds wrong */
#define BGDL_ERROR_RESUME_SIZE		0x80103A25	/* the partial file is not the size resumed from */
#define BGDL_ERROR_SHORT_RESPONSE	0x80103A26	/* the response ended before its length */
#define BGDL_ERROR_WRITE		0x80103A27	/* the file took fewer bytes than were written */
#define BGDL_ERROR_CONTENT_ID		0x80103A2B	/* the package is not the content of key 217 */
#define BGDL_ERROR_NO_HEADER		0x80103A2C	/* no room for a package's header */
#define BGDL_ERROR_NOT_HTTP		0x80103A10	/* the URL is not http:// or https:// */
#define BGDL_ERROR_NO_SERVER_DATA	0x80103A37	/* key 232 not read */
#define BGDL_ERROR_HTTP_STOPPED		0x80103A3B	/* the transfer stopped with 0x804101A1 */
#define BGDL_ERROR_HTTP_STOPPED_KIND	0x80103A4A	/* and Bgdl_8126E2AE gives 0 for its kind */

/* the error an export returns when its file is not there (PLACEHOLDER name) */
#define BGDL_ERROR_NO_CONTENT		0x80103A2F

#define BGDL_EXPORT_WORK_SIZE		0x10000

/* PLACEHOLDER names: a game_plugin install that failed because its media went (0x80010013,
   0x8032001A), named by where it was mounted: a game card's, a memory card's */
#define BGDL_ERROR_GAMECARD_REMOVED	0x80103928
#define BGDL_ERROR_MEMCARD_REMOVED	0x80103930
/* PLACEHOLDER name: the task's package folder is not a folder Bgdl_81113984 accepts */
#define BGDL_ERROR_BAD_FOLDER		0x80103A55
/* PLACEHOLDER name: the param file's 500 not read for a part-by-part install */
#define BGDL_ERROR_NO_PART_COUNT	0x12345678

class EseTask;
class BgdlManager;
class BgdlHttpDownload;
struct BgdlTaskInfo;

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
	int download_content(void);		/* (0x81117734) Sony's name (its timer's) */
	int download_package(void);		/* (0x81119152) Sony's name (its timer's) */
	int Download(void);			/* (0x8111B06A) PLACEHOLDER name: either, retried */
	int Prepare(void);			/* (0x8111C702) PLACEHOLDER name: the headers asked first */
	bool InstallLocal(int *ret);		/* (0x8111F12C) PLACEHOLDER name: a file:// package */
	int Run(void);				/* (0x8111FD8E) PLACEHOLDER name: the task run once */
	/* (0x8112142C) PLACEHOLDER name: the task's package folder installed by game_plugin */
	int InstallTask(EseTask *task, BgdlTaskInfo *info);
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

/* another file's (0x8126E252, PLACEHOLDER name): the largest download of a kind (2: a
   package of kind 5, 3: key 111 1, 4: the others) sent without its own SSL option */
int Bgdl_8126E252(int kind, SceUInt64 *size);

/* another file's (0x8126E2AE, PLACEHOLDER name): a check of a download kind (as
   Bgdl_8126E252's) through SceShellSvc B31E7F1C */
int Bgdl_8126E2AE(int kind);

/* another file's (0x8136D6A2, PLACEHOLDER name): a package opened to be unpacked into
   path; param 0x800 bytes */
int Bgdl_8136D6A2(const char *path, int flags, int mode, void *param);

/* another file's (0x8136D828, PLACEHOLDER name): the handle closed */
int Bgdl_8136D828(int handle);

/* another file's (0x8136D898, PLACEHOLDER name): the package's first bytes, already there,
   given again when a download resumes */
int Bgdl_8136D898(int handle, const void *buf, SceSize size);

/* another file's (0x8136D8F4, PLACEHOLDER name): size bytes of the package unpacked; the
   bytes taken, or < 0 */
int Bgdl_8136D8F4(int handle, const void *buf, SceSize size);

/* another file's (0x8136D952, PLACEHOLDER name): the package's position */
SceUInt64 Bgdl_8136D952(int handle, SceInt64 offset, int whence);

/* another file's (0x8136D9B8, PLACEHOLDER name): a package's header in buf read into data
   (cleared on an error) */
int Bgdl_8136D9B8(struct BgdlServerData *data, const void *buf, SceSize size);

/* another file's (0x8136DBE8, PLACEHOLDER name): the unpack's work memory set */
int Bgdl_8136DBE8(int handle, void *work, SceSize workSize, void *heap, SceSize heapSize,
		  SceSize bufSize, int a6, int a7);

/* a title's folder under ux0:app/ (or the folder given) made for a downloaded package
   (PLACEHOLDER names) */
class BgdlAppFolder {
public:
	BgdlAppFolder(BgdlHttpDownload *download)
		: m_download(download), m_result(0), m_path(""), m_open(false), m_handle(0)
	{
		m_written = 0;
	}

	/* (0x81415F90) */
	~BgdlAppFolder()
	{
		Close();
	}

	/* the package closed */
	void Close(void)
	{
		if (m_handle >= 0) {
			m_result = Bgdl_8136D828(m_handle);
			m_open = false;
		}
	}

	/* (0x81415AAC) the folder name in base (or ux0:app/) made, removed first when remove */
	__attribute__((noinline)) int Prepare(bool remove, const char *name, int category, const char *base)
	{
		return doPrepare(remove, name, base);
	}

	/* Prepare's body, which Sony's compiler also places in its callers (PLACEHOLDER name) */
	int doPrepare(bool remove, const char *name, const char *base)
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

	/* (0x81415FC4) */
	__attribute__((noinline)) int Set(bool remove, const char *path)
	{
		if (path == NULL) {
			m_result = BGDL_ERROR_NULL_FOLDER;
			return BGDL_ERROR_NULL_FOLDER;
		}
		m_path = path;
		return 0;
	}

	/* (0x81415FF0) */
	__attribute__((noinline)) paf::string GetPath(void)
	{
		return m_path;
	}

	/* (0x81416008) the package opened into the folder, resumed at offset */
	__attribute__((noinline)) int Open(SceUInt64 offset, bool a, bool b)
	{
		return doOpen(offset, a, b);
	}

	/* Open's body, which Sony's compiler also places in its callers (PLACEHOLDER name) */
	int doOpen(SceUInt64 offset, bool a, bool b)
	{
		if (m_path.length() == 0)
			return BGDL_ERROR_NULL_FOLDER;

		paf::string path = m_path;
		int flags;
		int ret;

		path += "/";
		sce_paf_memset(s_openParam, 0, sizeof(s_openParam));
		s_openParam[0] = 1;
		flags = 0x10002;
		if (offset == 0)
			flags = 0x10602;
		flags |= 0x100;
		if (a == true)
			flags |= 0x20000;
		if (b == true)
			flags |= 0x40000;
		m_handle = Bgdl_8136D6A2(path.c_str(), flags, 6, s_openParam);
		if (m_handle < 0) {
			m_result = m_handle;
			return m_handle;
		}
		m_result = 0;

		SceUID heap = bgdlManagerGet()->m_drmHeap;

		if (heap >= 0) {
			void *base;

			sce_paf_memset(s_work, 0, sizeof(s_work));
			base = NULL;
			ret = sceKernelGetMemBlockBase(heap, &base);
			if (ret == 0)
				ret = Bgdl_8136DBE8(m_handle, s_work, sizeof(s_work), base, 0x28000,
						    0x8000, 5, 1);
		} else {
			ret = BGDL_ERROR_NO_MEMORY;
		}
		if (ret == 0) {
			m_open = true;
			ret = 0;
		}
		m_result = ret;
		return ret;
	}

	/* (0x814161E4) */
	__attribute__((noinline)) int Write(const void *buf, SceSize size)
	{
		m_result = -1;
		if (m_open == true && buf != NULL)
			m_result = Bgdl_8136D898(m_handle, buf, size);
		return m_result;
	}

	/* (0x81416200) */
	__attribute__((noinline)) SceUInt64 Seek(SceInt64 offset, int whence)
	{
		if (m_open == true)
			return Bgdl_8136D952(m_handle, offset, whence);
		return 0;
	}

	/* (0x8141622C) size bytes of the package unpacked */
	__attribute__((noinline)) int Unpack(const void *buf, SceSize size)
	{
		return doUnpack(buf, size);
	}

	/* Unpack's body, which Sony's compiler also places in its callers (PLACEHOLDER name) */
	int doUnpack(const void *buf, SceSize size)
	{
		int ret;

		m_result = -1;
		ret = -1;
		if (m_open == true && buf != NULL) {
			ret = Bgdl_8136D8F4(m_handle, buf, size);
			m_result = ret;
			if (ret >= 0 && ret == size)
				m_written += size;
		}
		return ret;
	}

	/* (0x8111E7F2) PLACEHOLDER name: the package file at file unpacked into base's (or
	   ux0:app/'s) folder "p", removed first when remove */
	int UnpackFile(bool remove, const char *file, const char *titleId, const char *base);

	static SceUInt32 s_openParam[0x800 / 4];	/* (0x815631F0) */
	static char s_work[0xA00];			/* (0x815D1220) */

	BgdlHttpDownload *m_download;
	int m_result;
	paf::string m_path;
	bool m_open;
	SceUInt64 m_written;		/* the bytes unpacked */
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
	paf::string fileName;		/* the response's */
	paf::string mimeType;
	paf::string lastModified;	/* its Last-Modified header */
	paf::string realm;		/* the server's, with an HTTP 401 */
	SceUInt64 storageSize;		/* the device's (Bgdl_8111234C) */
	SceUInt64 storageUsed;
	SceUInt64 storageFree;
	SceUInt64 storageNeeded;	/* 6 clusters, and what the content takes */
	SceUInt64 value4;
	int kind;
	int reserved1;
	BgdlServerData data;
};

/* a server query's client timeouts (paf::http::Client::Param's, from m_resolve_timeout on) */
struct BgdlServerParam {
	SceUInt32 value[5];
};

/*
 * (0x8111B72A) Sony's name (its timer's): the url's headers asked of its server (HEAD), with
 * the device's space; a package's header read too (GET) for its kind. flags bit 0: no
 * livespace_db check of a kind 21 package.
 */
int GetHeaderInfo(int mode, int flags, const char *device, const char *url, const char *user,
		  const char *password, const char *reserved, const char *contentId,
		  BgdlServerInfo *info, BgdlServerParam *param, MtpBgCopyHost *host, int *checkResult,
		  char *digest);

/*
 * (0x8111FEF8) url's file got (GET) into folder, as name or the server's file name, its
 * headers in server and its path in path; with no folder only the headers. The device's
 * space is put in server too. flags and reserved0 are not read (PLACEHOLDER names).
 */
int Bgdl_8111FEF8(const char *device, const char *url, int flags, int reserved0, MtpBgCopyHost *host,
		  BgdlServerParam *param, const char *folder, const char *name, int *status,
		  BgdlServerInfo *server, paf::string *path);

/* what a download's HTTP request asks whether it is to stop (vtable 0x81530260; PLACEHOLDER
   names) */
class BgdlHttpStateCheck : public StateCheck {
public:
	BgdlHttpStateCheck(BgdlHttpDownload *download) : m_download(download)	/* (0x81415D98) */
	{
	}

	/* (0x81416710) the manager asked twice (Sony's discards the first answer) */
	virtual int IsReady()
	{
		bgdlManagerGet()->IsCancelled();
		return bgdlManagerGet()->IsCancelled();
	}

	BgdlHttpDownload *m_download;
};

/* game_plugin found, or loaded, for a local package's install, its load waited for under
   "SceBgdlMutexEseLoad" (PLACEHOLDER names) */
class BgdlGamePluginLoader {
public:
	BgdlGamePluginLoader(BgdlHttpDownload *download)
		: m_download(download), m_reserved(false), m_loaded(false), m_plugin(NULL),
		  m_interface(NULL), m_mutex("SceBgdlMutexEseLoad", 0)
	{
	}

	~BgdlGamePluginLoader()
	{
		if (m_interface != NULL)
			m_interface = NULL;
		if (m_plugin != NULL)
			m_plugin = NULL;
	}

	GamePluginInterface *Load(void);	/* (0x8111E14C) */

	/* PLACEHOLDER names: game_plugin's installs, of the package folder at path */
	/* (0x81120B9C) */
	__attribute__((noinline)) void CheckMediaError(int *ret, const char *mountPoint, SceSize len);
	/* (0x81120C32) */
	__attribute__((noinline)) int InstallFolder(const char *path, BgdlTaskInfo *info);
	/* (0x81120D06) */
	__attribute__((noinline)) int InstallTheme(const char *path, BgdlTaskInfo *info);
	/* (0x81120D9C) */
	__attribute__((noinline)) int InstallTitle(const char *path, BgdlTaskInfo *info);
	/* (0x81121066) */
	__attribute__((noinline)) int InstallLicense(const char *path, int kind, BgdlTaskInfo *info);
	/* (0x8112112A) */
	__attribute__((noinline)) int InstallPart(const char *path, BgdlTaskInfo *info,
						  struct BgdlInstallPart *part);

	BgdlHttpDownload *m_download;
	bool m_reserved;		/* PLACEHOLDER: set false, not read yet */
	bool m_loaded;			/* set by the plugin's load callback */
	paf::Plugin *m_plugin;
	GamePluginInterface *m_interface;
	paf::thread::Mutex m_mutex;
};

/* PLACEHOLDER names: one of a title's count parts, the progress of each a share of the whole */
struct BgdlInstallPart {
	int count;
	int index;
	BgdlTaskInfo *info;
};

/* (0x811219B8) PLACEHOLDER name: the task's content installed */
int Bgdl_811219B8(EseTask *task, BgdlTaskInfo *info);

/* Bgdl_81113890 called each time m_seconds have passed (PLACEHOLDER names) */
class BgdlPowerTicker {
public:
	BgdlPowerTicker(int seconds) : m_seconds(seconds)
	{
		m_last = paf::get_time_internal();
	}

	/* (0x81415E2C) */
	__attribute__((noinline)) void Tick(void)
	{
		doTick();
	}

	/* Tick's body, which Sony's compiler also places in its callers (PLACEHOLDER name) */
	void doTick(void)
	{
		m_now = paf::get_time_internal();
		if ((float)m_now < (float)m_last + (float)m_seconds * 1000000.0f)
			return;
		Bgdl_81113890(1);
		m_last = paf::get_time_internal();
	}

	int m_seconds;
	SceUInt64 m_last;
	SceUInt64 m_now;
};

inline void EseTask::updateProgress(bool report, SceUInt64 current)
{
	doUpdateProgress(report, current);
}

inline void EseTask::doUpdateProgress(bool report, SceUInt64 current)
{
	status.id = id;
	status.current = current;
	if (transfer.Update(current) == 1)
		status.remaining = transfer.GetRemaining();
	if (status.fileNumber >= 2) {
		status.filesCurrent = status.filesBase + current;
		if (files.Update(status.filesCurrent) == 1)
			status.filesRemaining = files.GetRemaining();
	}
	if (report == true)
		bgdlManagerGet()->SetStatus(&status);
}

#endif
