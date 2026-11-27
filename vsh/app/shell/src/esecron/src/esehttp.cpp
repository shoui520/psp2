/*
 * esehttp.cpp: the background download library's HTTP side (0x81114642 on; Sony's file name,
 * from the 0.990 prototype's records): a download's request query and what the content
 * server answered. The classes' and functions' names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <plugin.h>
#include <libhttp/libhttp_url.h>
#include <net/socket.h>
#include <net/inet.h>
#include <net/resolver.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <http/http.h>
#include <vshbridge/vshbridge.h>

#include "../include/esebase.hpp"
#include "../include/esetask.hpp"
#include "../include/eseparam.hpp"
#include "../include/eseutil.hpp"
#include "../include/esehttp.hpp"
#include "src/livespace_db/livespace_db_internal.h"
#include "../../applauncher_plugin/src/applauncher.h"
#include "bgdl_api.h"

using namespace paf;

extern int npGenerateAccountIdHash(void *outDigest);

SceUInt32 BgdlHttpRequest::Bgdl_8118FD0C(void)
{
	return m_downloadId;
}

/* (0x8150A6C4) which kinds of request carry each parameter of the query */
static const bool s_queryParams[BGDL_QUERY_PARAM_COUNT][BGDL_REQUEST_KIND_COUNT] = {
	{ 1, 1, 1, 1, 1, 0 },
	{ 0, 0, 0, 0, 0, 0 },
	{ 1, 1, 1, 0, 1, 0 },	/* BGDL_QUERY_DOWNLOAD_ID */
	{ 1, 1, 1, 0, 1, 0 },	/* BGDL_QUERY_DIGEST */
	{ 1, 1, 1, 0, 1, 0 },	/* BGDL_QUERY_SERVER_IP */
	{ 1, 1, 1, 1, 1, 0 },	/* BGDL_QUERY_DOWNLOAD_TYPE */
	{ 0, 0, 0, 0, 0, 0 },	/* BGDL_QUERY_INSTANT */
	{ 0, 0, 0, 0, 0, 0 },	/* BGDL_QUERY_START */
	{ 0, 0, 0, 0, 0, 0 },	/* BGDL_QUERY_PUSH */
};

/* (0x81114642) */
BgdlHttpRequest::BgdlHttpRequest()
	: m_kind(-1), m_type(-1), m_downloadId(0), m_start(true), m_push(false)
{
	m_unusedHeader.clear();
	m_unusedDetail.clear();
	m_extra.clear();
	m_serverIpAddr.clear();
	m_hasDigest = 0;
	sce_paf_memset(m_digest, 0, sizeof(m_digest));
}

/* (0x811146BE) the query, NULL while it is empty */
const char *BgdlHttpRequest::GetQuery(void)
{
	if (m_query.length() != 0)
		return m_query.c_str();
	return NULL;
}

/* (0x811146CC) url's host name */
int BgdlHttpRequest::GetHost(const char *url, string *host)
{
	SceHttpUriElement element;
	SceSize require;
	int ret;

	host->clear();
	common::SharedPtr<MallocBuffer> pool;

	ret = sceHttpUriParse(NULL, url, NULL, &require, 0);
	if (ret < 0)
		return ret;
	pool = MallocBuffer::Allocate(require);
	if (pool.get() == NULL)
		return SCE_PAF_ERROR_HTTP_NOT_FOUND;
	ret = sceHttpUriParse(&element, url, pool->GetAddr(), NULL, pool->GetSize());
	if (ret < 0)
		return ret;
	host->assign(element.hostname);
	if (host->length() == 0)
		return SCE_PAF_ERROR_HTTP_NOT_FOUND;
	return 0;
}

/* (0x8111493C) the address of url's host, as text */
int BgdlHttpRequest::ResolveServer(const char *url)
{
	char text[16];
	SceNetInAddr addr;
	string host;
	const char *name;
	int ret;

	ret = GetHost(url, &host);
	name = host.c_str();
	if (ret == 0 && host.length() != 0) {
		if (name == NULL) {
			ret = -1;
		} else {
			SceNetId rid = sceNetResolverCreate("resolver", NULL, 0);

			ret = rid;
			if (rid >= 0) {
				int res = sceNetResolverStartNtoa(rid, name, &addr, 0, 0, 0);

				if (res < 0) {
					sceNetResolverDestroy(rid);
					ret = res;
				} else {
					ret = sceNetResolverDestroy(rid);
					if (ret >= 0 && ret == 0) {
						if (sceNetInetNtop(SCE_NET_AF_INET, &addr, text,
								   sizeof(text)) != NULL)
							m_serverIpAddr.assign(text);
						else
							m_serverIpAddr.clear();
					}
				}
			}
		}
	}
	return ret;
}

/* (0x81114A26) the digest copied from digest, or the console's when it is NULL */
int BgdlHttpRequest::SetDigest(const char *digest)
{
	SceUInt8 *dst = m_digest;
	int ret = 0;
	bool set;

	sce_paf_memset(dst, 0, BGDL_DIGEST_SIZE);
	m_hasDigest = 0;
	if (digest == NULL) {
		ret = npGenerateAccountIdHash(dst);
		set = ret == 0;
	} else {
		for (int i = 0; i < BGDL_DIGEST_SIZE; i++)
			*dst++ = *digest++;
		set = true;
	}
	m_hasDigest = set;
	return ret;
}

/* (0x81114A8C) the download's id; 0: one made from the time and the console's digest */
void BgdlHttpRequest::SetDownloadId(SceUInt32 id)
{
	if (id != 0) {
		m_downloadId = id;
	} else {
		m_downloadId = get_time_internal();
		if (SetDigest(NULL) == 0) {
			int sum = 0;

			for (int i = 0; i < BGDL_DIGEST_SIZE; i++)
				sum += m_digest[i];
			m_downloadId += (sum << 16) | 0x80000000;
		}
	}
}

/* the query's separator before the next parameter, "?" before the first */
static inline void addSeparator(string *query, bool *first)
{
	if (*first == true) {
		*query += "?";
		*first = false;
	} else {
		*query += "&";
	}
}

/* (0x81114AFC) the query of a request of kind to url */
void BgdlHttpRequest::Setup(int type, int kind, const char *url, SceUInt32 downloadId,
			    const char *digest, const char *extra, bool start, bool push)
{
	char buf[128] = {0};
	bool first;

	m_query = "";
	if (type == -1 || kind == -1 || url == NULL)
		return;
	m_kind = kind;
	m_type = type;
	first = sce_paf_strrchr(url, '?') == NULL;
	if (extra != NULL)
		m_extra.assign(extra, sce_paf_strlen(extra));
	else
		m_extra.clear();
	SetDownloadId(downloadId);
	SetDigest(digest);
	ResolveServer(url);
	m_start = start;
	m_push = push;
	if (m_extra.length() != 0) {
		buf[0] = '\0';
		addSeparator(&m_query, &first);
		m_query.append(m_extra);
	}
	if (s_queryParams[BGDL_QUERY_DOWNLOAD_ID][m_kind] && m_downloadId != 0) {
		buf[0] = '\0';
		addSeparator(&m_query, &first);
		sce_paf_snprintf(buf, sizeof(buf) - 1, "downloadId=%08x", m_downloadId);
		m_query += buf;
	}
	if (s_queryParams[BGDL_QUERY_DIGEST][m_kind]) {
		sce_paf_memset(buf, 0, sizeof(buf));
		addSeparator(&m_query, &first);
		for (int i = 0; i < BGDL_DIGEST_SIZE; i++) {
			char hex[8] = "";

			sce_paf_snprintf(hex, sizeof(hex) - 1, "%02x", m_digest[i]);
			sce_paf_strlcat(buf, hex, sizeof(buf));
		}
		m_query += "q=";
		m_query += buf;
	}
	if (s_queryParams[BGDL_QUERY_SERVER_IP][m_kind] && m_serverIpAddr.length() != 0) {
		addSeparator(&m_query, &first);
		m_query += "serverIpAddr=";
		m_query.append(m_serverIpAddr);
	}
	if (s_queryParams[BGDL_QUERY_DOWNLOAD_TYPE][m_kind]) {
		addSeparator(&m_query, &first);
		switch (m_type) {
		case 0:
			m_query += "downloadType=fs";
			break;
		case 1:
			m_query += "downloadType=fm";
			break;
		case 2:
			m_query += "downloadType=bg";
			break;
		case 3:
			m_query += "downloadType=pg";
			break;
		case 4:
			m_query += "downloadType=st";
			break;
		case 5:
			m_query += "downloadType=vf";
			break;
		case 6:
			m_query += "downloadType=vb";
			break;
		default:
			break;
		}
	}
	if (s_queryParams[BGDL_QUERY_INSTANT][m_kind]) {
		addSeparator(&m_query, &first);
		m_query += "ptype=instant";
	}
	if (s_queryParams[BGDL_QUERY_START][m_kind] && m_start) {
		addSeparator(&m_query, &first);
		m_query += "start=1";
	}
	if (s_queryParams[BGDL_QUERY_PUSH][m_kind] && m_push) {
		addSeparator(&m_query, &first);
		m_query += "push=true";
	}
}

/* (0x81115C22) the digest copied to digest */
int BgdlHttpRequest::GetDigest(char *digest)
{
	if (digest != NULL) {
		const SceUInt8 *src = m_digest;

		for (int i = 0; i < BGDL_DIGEST_SIZE; i++)
			*digest++ = *src++;
	}
	return 0;
}

/* (0x81115C58) */
void BgdlServerInfo::Clear(void)
{
	size = 0;
	status = 0;
	httpStatus = 0;
	fileName.clear();
	mimeType.clear();
	lastModified.clear();
	realm.clear();
	storageSize = 0;
	storageUsed = 0;
	storageFree = 0;
	storageNeeded = 0;
	value4 = 0;
	flag = false;
	kind = 0;
	sce_paf_memset(&data, 0, sizeof(data));
}

/* (0x81115CC4) */
BgdlSizeAlign::BgdlSizeAlign(int size, int align) : size(size), align(align)
{
}

/* (0x81115CCA) the param file set for the package's file index: its name, size and digest */
int BgdlHttpDownload::StartFile(int index)
{
	BgdlTaskPath name;
	BgdlTaskPath file;
	SceUInt64 current = 0;
	SceUInt64 size;
	SceUInt32 downloadId = 0;
	int parts = 0;
	char digest[BGDL_DIGEST_SIZE];
	int ret;

	{
		EseParam param(m_task->id);
		int fileNumber;
		SceUInt64 filesTotal;
		SceUInt64 filesBase;
		EseTask *task;

		ret = param.open();
		if (ret != 0)
			return ret;
		fileNumber = 0;
		ret = param.read(500, &fileNumber, 4);
		if (ret != 0)
			return ret;
		filesTotal = 0;
		ret = param.read(505, &filesTotal, 8);
		if (ret != 0)
			return ret;
		filesBase = 0;
		for (int i = 0; i < index; i++) {
			ret = param.read(5002 + 32 * i, &size, 8);
			if (ret != 0)
				return ret;
			filesBase += size;
		}
		task = m_task;
		task->status.fileCount = index + 1;
		task->status.fileNumber = fileNumber;
		task->status.filesBase = filesBase;
		task->status.filesCurrent = filesBase;
		task->status.filesTotal = filesTotal;
		if (task->status.fileNumber >= 2 && task->status.fileCount <= 1)
			task->status.filesRemaining = 0;
		bgdlManagerGet()->SetStatus(&task->status);
		task->files.Start(filesTotal, filesBase, 0, 512000);

		ret = param.read(5000 + 32 * index, &name);
		if (ret != 0)
			return ret;
		ret = param.read(5003 + 32 * index, &file);
		if (ret != 0)
			return ret;
		ret = param.read(5002 + 32 * index, &size, 8);
		if (ret != 0)
			return ret;
		param.read(5004 + 32 * index, &downloadId, 4);
		param.read(5005 + 32 * index, digest, BGDL_DIGEST_SIZE);
		param.read(5006 + 32 * index, &parts, 4);
	}

	EseParam param(m_task->id);

	ret = param.openWrite();
	if (ret != 0)
		return ret;
	ret = param.write(202, &name);
	if (ret != 0)
		return ret;
	ret = param.write(203, &file);
	if (ret != 0)
		return ret;
	ret = param.write(206, &size, 8);
	if (ret != 0)
		return ret;
	ret = param.write(208, &current, 8);
	if (ret != 0)
		return ret;
	ret = param.write(501, &index, 4);
	if (ret != 0)
		return ret;
	{
		BgdlHttpRequest request;

		request.SetDownloadId(downloadId);
		if (downloadId == 0) {
			downloadId = request.Bgdl_8118FD0C();
			if (downloadId != 0) {
				param.write(5004 + 32 * index, &downloadId, 4);
				request.SetDigest(NULL);
				request.GetDigest(digest);
				param.write(5005 + 32 * index, digest, BGDL_DIGEST_SIZE);
			}
		}
		param.write(236, &downloadId, 4);
		param.write(237, digest, BGDL_DIGEST_SIZE);
	}
	param.write(238, &parts, 4);
	ret = param.commit();
	if (ret != 0)
		return ret;
	return 0;
}

/* (0x811161DA) the package's next file started, or the task marked done after its last */
int BgdlHttpDownload::NextFile(void)
{
	int fileNumber = 0;
	int fileCount = -1;
	int next;
	int ret;

	Bgdl_81113890(1);
	{
		EseParam param(m_task->id);

		ret = param.open();
		if (ret != 0)
			return ret;
		param.read(500, &fileNumber, 4);
		if (fileNumber == 0)
			return 0;
		param.read(501, &fileCount, 4);
		next = fileCount + 1;
	}
	if (fileNumber > next) {
		ret = StartFile(next);
		if (ret == 0)
			return BGDL_ERROR_RETRY;
		return ret;
	}

	bool done = true;

	m_task->m_ParamCache.key113 = 1;
	Bgdl_811233A4(m_task->id, 113, &done, 1);
	*m_runStatus = 0;
	return 0;
}

/* (0x811162AA) a type 4 task's content (key 221, its content id key 217) installed */
int BgdlHttpDownload::InstallContent(void)
{
	Bgdl_81113890(1);

	BgdlHttpCancel cancel;
	BgdlTaskPath path;
	BgdlTaskPath contentId;
	int key107;

	cancel.download = this;
	cancel.cancelled = false;
	cancel.running = false;
	key107 = 0;

	EseParam param(m_task->id);

	if (param.open() != 0)
		return 0;
	if (param.read(107, &key107, 4) == 0 && key107 == 4) {
		int ret;

		if (param.read(221, &path) == BGDL_ERROR_NULL_FOLDER)
			return 0;
		ret = param.read(221, &path);
		if (ret == BGDL_ERROR_NULL_FOLDER)
			return 0;
		if (ret == 0) {
			ret = param.read(217, &contentId);
			if (ret == BGDL_ERROR_NULL_FOLDER)
				return 0;
			if (ret == 0) {
				bgdlManagerGet()->SetCancelHandler(BgdlHttpCancel::OnCancel, &cancel);
				cancel.running = true;
				if (Bgdl_8113B3A0(path.m_begin, contentId.m_begin) >= 0) {
					cancel.running = false;
					bgdlManagerGet()->SetCancelHandler(NULL, NULL);
				} else {
					bgdlManagerGet()->IsCancelled();
				}
			}
		}
	}
	return 0;
}

/* a name ending in ".mp4" */
static inline bool isMp4(const char *path)
{
	const char *ext = sce_paf_strrchr(path, '.');

	return ext != NULL && sce_paf_strcasecmp(ext, ".mp4") == 0;
}

/*
 * (0x8111644E) the downloaded file put in place: a package (type 6, 12) unpacked into its
 * title's folder, a photo, music or video (types 1-4) given to its export library
 */
int BgdlHttpDownload::Complete(void)
{
	int type = 0;
	BgdlTaskPath file;		/* key 203 */
	BgdlTaskPath titleId;		/* key 220 */
	BgdlTaskPath key230;
	BgdlTaskPath key222;
	BgdlTaskPath contentId;		/* key 217 */
	int state;
	int ret;

	Bgdl_81113890(1);
	ret = m_task->setState(3, &state, false);
	if (ret != 0)
		return ret;

	BgdlServerData data;

	sce_paf_memset(&data, 0, sizeof(data));

	paf::string folder;
	paf::string unused("");		/* PLACEHOLDER: made and never read */

	do {
		BgdlTaskPath path;
		SceUInt64 exportId = 0;
		int key111 = 7;

		ret = m_task->GetFolder(&path, true);
		if (ret != 0)
			break;
		{
			EseParam param(m_task->id);

			ret = param.open();
			if (ret != 0)
				break;
			ret = param.read(107, &type, sizeof(type));
			if (ret != 0)
				break;
			param.read(230, &key230);
			param.read(111, &key111, sizeof(key111));
			if (type != 12 && type != 8 && type != 7 && type != 6) {
				if (type == 4)
					param.read(222, &key222);
				ret = param.read(203, &file);
				if (ret != 0)
					break;
			} else {
				if (type == 8) {
					int count = 0;
					int index;

					param.read(500, &count, sizeof(count));
					if (count == 0)
						return 0;
					index = -1;
					param.read(501, &index, sizeof(index));
					if (index + 1 < count) {
						ret = 0;
						break;
					}
				}
				ret = param.read(220, &titleId);
				if (ret != 0)
					break;
				if (param.read(232, &data, sizeof(data)) != 0) {
					*m_runStatus = 0;
					return BGDL_ERROR_NO_TITLE_ID;
				}
				param.read(217, &contentId);
				if (titleId.m_end == titleId.m_begin) {
					ret = BGDL_ERROR_NO_TITLE_ID;
					break;
				}

				BgdlAppFolder app(this);

				ret = app.Prepare(false, "p", data.category, path.m_begin);
				if (ret != 0)
					break;

				paf::string from;

				from = app.GetPath();
				ret = app.Prepare(false, titleId.m_begin, data.category, path.m_begin);
				if (ret != 0)
					break;
				folder = app.GetPath();
				if (paf::Dir::IsExist(folder.c_str()) == true) {
					ret = paf::Dir::RemoveRecursive(folder.c_str());
					if (ret != 0)
						break;
				}
				ret = paf::LocalFile::RenameFile(from.c_str(), folder.c_str());
				if (ret != 0)
					break;
			}
		}
		if (type != 12 && type != 8 && type != 7 && type != 6) {
			BgdlTaskPath target;

			ret = m_task->getFilePath(&target, true);
			if (ret != 0)
				break;
			if (target.m_end == target.m_begin) {
				ret = BGDL_ERROR_NO_CONTENT;
				break;
			}
			{
				paf::string info(path.m_begin);

				info += "/";
				info += "av_contents.bin";
				paf::Dir::Remove(info.c_str());
			}

			int newType = type;
			char *exported;

			switch (type) {
			case 1:
				if (key111 == 6 && (exported = new char[1024]) != NULL) {
					sce_paf_memset(exported, 0, 1024);
					ret = ExportPhoto(type, target.m_begin, exported, 1024);
					if (ret == 0 && target.m_begin != NULL) {
						paf::Plugin *plugin = paf::Plugin::Find("applauncher_plugin");

						if (plugin != NULL) {
							ApplauncherInterface *iface =
								(ApplauncherInterface *)plugin->GetInterface(1);

							if (iface != NULL)
								((void (*)(const char *, char *))iface->func[20])(
									target.m_begin, exported);
						}
					}
					delete[] exported;
				} else {
					ret = ExportPhoto(type, target.m_begin, &exportId);
				}
				break;
			case 2:
				ret = ExportMusic(type, target.m_begin, &exportId);
				if (ret != 0 && ret != BGDL_ERROR_NO_CONTENT && target.m_begin != NULL &&
				    isMp4(target.m_begin)) {
					ret = ExportVideo(type, target.m_begin, &exportId);
					if (ret == 0)
						newType = 3;
				}
				break;
			case 3:
			case 4:
				ret = ExportVideo(type, target.m_begin, &exportId);
				if (ret != 0 && ret != BGDL_ERROR_NO_CONTENT && target.m_begin != NULL &&
				    isMp4(target.m_begin)) {
					ret = ExportMusic(type, target.m_begin, &exportId);
					if (ret == 0)
						newType = 2;
				}
				break;
			case 5: {
				EseParam param(m_task->id);

				ret = param.openWrite();
				if (ret == 0) {
					bool done = true;

					ret = param.write(113, &done, sizeof(done));
					if (ret == 0) {
						m_task->m_ParamCache.key113 = done;
						ret = param.commit();
					}
				}
				break;
			}
			default:
				ret = 0;
				break;
			}
			if (ret == 0) {
				m_task->value = exportId;
				Bgdl_811233A4(m_task->id, 233, &exportId, sizeof(exportId));
				if (newType != type) {
					type = newType;
					Bgdl_811233A4(m_task->id, 107, &type, sizeof(type));
					m_task->m_ParamCache.key107 = type;
				}
			}
		} else if (type == 7) {
			bool done;

			m_task->m_ParamCache.key113 = 1;
			done = true;
			ret = Bgdl_811233A4(m_task->id, 113, &done, sizeof(done));
			*m_runStatus = 0;
		} else if (type == 8) {
			ret = 0;
		} else {
			int check = -1;

			switch (data.category) {
			case 6:
			case 7:
			case 14:
			case 15:
			case 16:
				check = Bgdl_81113400(folder.c_str());
				break;
			case 21:
			case 22:
			case 24:
			case 29:
			case 31:
				break;
			default:
				ret = BGDL_ERROR_BAD_STORAGE;
				break;
			}
			if (ret != 0)
				break;
			{
				EseParam param(m_task->id);

				ret = param.openWrite();
				if (ret == 0) {
					bool key102 = false;

					ret = param.write(102, &key102, sizeof(key102));
					if (ret == 0) {
						bool done;

						m_task->m_ParamCache.key102 = key102;
						done = true;
						ret = param.write(113, &done, sizeof(done));
						if (ret == 0) {
							int newType;

							m_task->m_ParamCache.key113 = done;
							newType = type;
							if (check == 1 || check == 2)
								newType = 7;
							if (newType != type) {
								type = newType;
								ret = param.write(107, &type, sizeof(type));
								if (ret == 0)
									m_task->m_ParamCache.key107 = type;
							}
							if (ret == 0)
								ret = param.commit();
						}
					}
				}
			}
			*m_runStatus = 0;
		}
	} while (0);
	if (ret != 0)
		*m_runStatus = 0;
	return ret;
}

/* the headers every content request sends (PLACEHOLDER names) */
struct BgdlRequestHeader {
	const char *name;
	const char *value;
};

static const BgdlRequestHeader s_requestHeaders[] = {
	{ "Pragma", "no-cache" },
	{ "Cache-Control", "no-cache" },
	{ "Accept", "*/*" },
	{ NULL, NULL },
};

/* (0x8111770C) PLACEHOLDER name */
static int bgdlSetRequestHeaders(http::Transaction *trans)
{
	const BgdlRequestHeader *header = s_requestHeaders;
	const char *name = header->name;
	int ret = 0;

	while (name != NULL) {
		ret = trans->SetRequestHeader(name, header->value);
		if (ret != 0)
			break;
		name = header[1].name;
		header++;
	}
	return ret;
}

/*
 * (0x81117734) the task's file downloaded from key 202's URL, resumed from what is already there
 * (key 208, 32 KiB before it) unless key 112 says to append; key 208 is written about every
 * 1 % of the size (1 to 8 MiB).
 */
int BgdlHttpDownload::download_content(void)
{
	SceUInt64 current = 0;		/* key 208 */
	SceUInt64 offset = 0;		/* key 211 */
	SceUInt64 total = (SceUInt64)-1;	/* key 206 */
	SceUInt64 length = (SceUInt64)-1;
	bool key218 = false;
	bool key110 = false;
	bool append = false;		/* key 112 */
	BgdlTaskPath file;
	BgdlTaskPath lastModified;	/* key 204 */
	BgdlTaskPath url;		/* key 202 */
	BgdlTaskPath titleId;		/* key 220 */
	BgdlTaskPath userKey;		/* key 200 */
	BgdlTaskPath passwordKey;	/* key 201 */
	BgdlTaskPath referer;		/* key 209 */
	int type = 0;			/* key 107 */
	int kind = 7;			/* key 111 */
	int key114 = 0;
	SceUInt32 digestId = 0;		/* key 236 */
	BgdlTaskPath key203;
	string user;
	string password;
	char digest[32];		/* key 237 */
	int state;
	int status;
	int ret;

	BgdlScopeTimer timer("download_content");

	Bgdl_81113890(1);
	ret = m_task->setState(2, &state, false);
	if (ret != 0)
		return ret;
	{
		EseParam param(m_task->id);

		ret = param.open();
		if (ret != 0)
			return ret;
		param.read(208, &current, sizeof(current));
		param.read(211, &offset, sizeof(offset));
		param.read(206, &total, sizeof(total));
		param.read(204, &lastModified);
		param.read(111, &kind, sizeof(kind));
		param.read(112, &append, sizeof(append));
		param.read(209, &referer);
		param.read(203, &key203);
		param.read(236, &digestId, sizeof(digestId));
		param.read(237, digest, sizeof(digest));
		param.read(110, &key110, sizeof(key110));
		param.read(218, &key218, sizeof(key218));
		param.read(114, &key114, sizeof(key114));
		if (total == current)
			return 0;
		param.read(202, &url);
		if (url.m_end == url.m_begin) {
			*m_runStatus = 0;
			return BGDL_ERROR_NULL_FOLDER;
		}
		param.read(107, &type, sizeof(type));
		param.read(200, &userKey);
		if (userKey.m_end != userKey.m_begin)
			Bgdl_811221E0(userKey.m_begin, 31, &user);
		param.read(201, &passwordKey);
		if (passwordKey.m_end != passwordKey.m_begin)
			Bgdl_811221E0(passwordKey.m_begin, 31, &password);
		if (bgdlKindIsGame(type)) {
			param.read(220, &titleId);
			if (titleId.m_end == titleId.m_begin) {
				*m_runStatus = 0;
				return BGDL_ERROR_NO_TITLE_ID;
			}
		}
	}
	if (bgdlManagerGet()->IsCancelled())
		return BGDL_ERROR_INTERRUPTED;

	http::Client::Param clientParam(string(""));

	if (user.length() != 0)
		clientParam.m_user_name = user.c_str();
	if (password.length() != 0)
		clientParam.m_password = password.c_str();
	clientParam.m_resolve_timeout = 0;
	clientParam.m_resolve_retry = 0;
	clientParam.m_connect_timeout = 60;
	clientParam.m_send_timeout = 60;
	clientParam.m_recv_timeout = 60;

	http::Client client(clientParam);
	http::Transaction trans(client);
	BgdlHttpStateCheck check(this);
	common::SharedPtr<File> out;
	BgdlFileCloser closer;

	{
		string target(url.m_begin);
		int method = 6;
		int requestKind = 0;

		if (key110)
			method = 5;
		switch (kind) {
		case 1:
		case 4:
			break;
		default:
			switch (type) {
			case 3:
			case 4:
				requestKind = 1;
				break;
			case 5:
				requestKind = 3;
				break;
			}
			break;
		}
		if (requestKind != 0) {
			BgdlHttpRequest request;

			request.Setup(method, requestKind, target.c_str(), digestId,
				      digestId != 0 ? digest : NULL, NULL, true, false);
			if (digestId == 0) {
				digestId = request.Bgdl_8118FD0C();
				if (digestId != 0) {
					Bgdl_811233A4(m_task->id, 236, &digestId, sizeof(digestId));
					request.GetDigest(digest);
					Bgdl_811233A4(m_task->id, 237, digest, sizeof(digest));
				}
			}

			const char *query = request.GetQuery();

			if (query != NULL && sce_paf_strlen(query) != 0)
				target += query;
		}
		ret = ScePafMisc_A86A5D0E(&trans, target.c_str(), "GET", &check);
	}
	if (ret != 0)
		return ret;
	bgdlSetRequestHeaders(&trans);
	if (referer.m_end != referer.m_begin) {
		common::SharedPtr<MallocBuffer> buf = MallocBuffer::Allocate(sce_paf_strlen(referer.m_begin) + 16);

		if (buf.get() == NULL || buf->GetSize() == 0)
			return BGDL_ERROR_NO_MEMORY;
		sce_paf_snprintf((char *)buf->GetAddr(), buf->GetSize() - 1, "%s", referer.m_begin);
		ret = trans.SetRequestHeader("Referer", (const char *)buf->GetAddr());
		if (ret != 0)
			return ret;
	}
	ret = m_task->getFilePath(&file, true);
	if (ret != 0)
		return ret;
	if (!bgdlKindIsGame(type) && append == false) {
		SceUInt64 size;

		if (Dir::IsExist(file.m_begin) == true && (size = LocalFile::GetFileSize(file.m_begin)) > 0) {
			if (current != size)
				current = size;
		} else {
			current = 0;
		}
	}
	if (current > 0x8000 || offset != 0) {
		if (offset != 0 && current < 0x8000)
			current = 0;
		else
			current -= 0x8000;
		if (lastModified.m_end != lastModified.m_begin) {
			{
				common::SharedPtr<MallocBuffer> buf =
					MallocBuffer::Allocate(sce_paf_strlen("bytes=%llu-") + 64);

				if (buf.get() == NULL || buf->GetSize() == 0)
					return BGDL_ERROR_NO_MEMORY;
				sce_paf_snprintf((char *)buf->GetAddr(), buf->GetSize() - 1, "bytes=%llu-",
						 current + offset);
				ret = trans.SetRequestHeader("Range", (const char *)buf->GetAddr());
				if (ret != 0)
					return ret;
			}
			{
				common::SharedPtr<MallocBuffer> buf =
					MallocBuffer::Allocate(sce_paf_strlen(lastModified.m_begin) + 64);

				if (buf.get() == NULL || buf->GetSize() == 0)
					return BGDL_ERROR_NO_MEMORY;
				sce_paf_snprintf((char *)buf->GetAddr(), buf->GetSize() - 1, "%s",
						 lastModified.m_begin);
				ret = trans.SetRequestHeader("If-Range", (const char *)buf->GetAddr());
				if (ret != 0)
					return ret;
			}
		}
	}
	if (kind == 1) {
		trans.SetAutoRedirect(true);
		trans.m_ssl_error_mask = 2;
	}
	if (!bgdlManagerGet()->SetCancelHandler(NULL, NULL))
		return BGDL_ERROR_INTERRUPTED;

	SceUInt64 limit = 0;
	int limitKind = 4;

	if (kind == 1)
		limitKind = 3;
	else if (type == 5)
		limitKind = 2;
	if (Bgdl_8126E252(limitKind, &limit) != 0 || limit == 0)
		limit = 20 * 1024 * 1024;
	if (limit <= total)
		ScePafMisc_5BF60DE9(&trans, 2);
	ret = trans.SendRequest(NULL, 0);
	if (ret != 0)
		return ret;
	if (bgdlManagerGet()->IsCancelled())
		return BGDL_ERROR_INTERRUPTED;
	status = 0;
	trans.GetStatusCode(status);
	if ((status != 200 && status != 206) || (status == 200 && offset != 0)) {
		Bgdl_811233A4(m_task->id, 207, &status, sizeof(status));
		*m_runStatus = 0;
		return BGDL_ERROR_SERVER;
	}
	if (trans.GetContentLength(length) != 0 || length == (SceUInt64)-1) {
		if (kind != 1 || type != 1 || status != 200) {
			*m_runStatus = 0;
			return BGDL_ERROR_NO_SIZE;
		}
		length = total;
	}

	common::SharedPtr<MallocBuffer> buf = MallocBuffer::Allocate(0x8000);

	if (buf.get() == NULL || buf->GetSize() == 0) {
		*m_runStatus = 0;
		return BGDL_ERROR_NO_MEMORY;
	}

	BgdlTaskPath folder;

	ret = m_task->GetFolder(&folder, true);
	if (ret != 0)
		return ret;
	if (folder.m_begin == NULL || folder.m_end == folder.m_begin) {
		*m_runStatus = 0;
		return BGDL_ERROR_INVALID_ARGUMENT;
	}

	string folderName(folder.m_begin);	/* PLACEHOLDER: made and never read */

	if (append) {
		if (status == 200)
			current = 0;
		out = LocalFile::Open(file.m_begin, SCE_O_RDWR | 0x10000, 6, &ret);
		if (ret != 0)
			return ret;
	} else if (status == 200) {
		current = 0;
		out = LocalFile::Open(file.m_begin, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC | 0x10000, 6, &ret);
		if (ret != 0)
			return ret;
	} else {
		out = LocalFile::Open(file.m_begin, SCE_O_RDWR | 0x10000, 6, &ret);
		if (ret != 0)
			return ret;
	}
	closer.Set(out);
	if (status == 206 && (SceUInt64)out->Seek(current, SCE_SEEK_SET) != current) {
		*m_runStatus = 0;
		return BGDL_ERROR_RESUME_SIZE;
	}
	{
		EseParam param(m_task->id);

		ret = param.openWrite();
		if (ret != 0)
			return ret;

		string modified;

		if (trans.GetResponseHeader("Last-Modified", modified) == 0) {
			ret = param.write(204, modified.c_str(), modified.length() + 1);
			if (ret != 0)
				return ret;
		}
		if (status == 200) {
			ret = param.write(206, &length, sizeof(length));
			if (ret != 0)
				return ret;
			total = length;
		}
		if (current == 0) {
			ret = param.write(208, &current, sizeof(current));
			if (ret != 0)
				return ret;
		}
		m_task->setProgress(true, total, current);
		ret = param.commit();
		if (ret != 0)
			return ret;
	}

	SceUInt64 written = 0;
	SceUInt32 size = 0;
	SceUInt64 step = (total / 100) & ~(SceUInt64)0x7FFF;
	SceUInt32 reported;

	if (step < 0x100000)
		step = 0x100000;
	else if (step > 0x800000)
		step = 0x800000;

	BgdlPowerTicker ticker(1);

	Bgdl_81113890(1);
	reported = 0;
	while (written < length) {
		ticker.Tick();
		ret = trans.ReceiveResponse((common::SharedPtr<Buffer> &)buf, &size);
		if (ret != 0) {
			if (bgdlManagerGet()->IsCancelled())
				ret = BGDL_ERROR_INTERRUPTED;
			break;
		}
		if (size == 0) {
			if (written < length)
				ret = BGDL_ERROR_SHORT_RESPONSE;
			break;
		}
		if (buf->GetSize() > size && written + size < length) {
			ret = BGDL_ERROR_SHORT_RESPONSE;
			break;
		}
		if (bgdlManagerGet()->IsSuspended() == true) {
			ret = BGDL_ERROR_INTERRUPTED;
			break;
		}
		if (bgdlManagerGet()->IsHeld() == true) {
			ret = BGDL_ERROR_INTERRUPTED;
			break;
		}
		if ((SceUInt32)out->Write(buf->GetAddr(), size) != size) {
			*m_runStatus = 0;
			ret = BGDL_ERROR_WRITE;
			break;
		}
		current += size;
		written += size;
		m_task->updateProgress(true, current);
		if (written != 0 && reported < written / step) {
			Bgdl_811233A4(m_task->id, 208, &current, sizeof(current));
			reported = written / step;
		}
		thread::Sleep(1);
	}
	if (written == length)
		ret = 0;
	if (written != 0) {
		int err = Bgdl_811233A4(m_task->id, 208, &current, sizeof(current));
		EseParam param(m_task->id);

		if (err != 0)
			return err;
	}
	return ret;
}

SceUInt32 BgdlAppFolder::s_openParam[0x800 / 4];
char BgdlAppFolder::s_work[0xA00];

/*
 * (0x81119152) the task's package downloaded from key 202's URL and unpacked into its title's
 * folder as it arrives (type 8: the folder's p/<key 501> part), resumed from key 208 at 128 KiB
 * or more; key 208 is written about every 1 % of the size (1 to 8 MiB).
 */
int BgdlHttpDownload::download_package(void)
{
	SceUInt64 current = 0;		/* key 208 */
	SceUInt64 offset = 0;		/* key 211 */
	SceUInt64 total = (SceUInt64)-1;	/* key 206 */
	SceUInt64 length = (SceUInt64)-1;
	bool key239 = false;
	bool key218 = false;
	bool key110 = false;
	bool append = false;		/* key 112 */
	BgdlTaskPath file;
	BgdlTaskPath lastModified;	/* key 204 */
	BgdlTaskPath url;		/* key 202 */
	BgdlTaskPath titleId;		/* key 220 */
	BgdlTaskPath referer;		/* key 209 */
	BgdlTaskPath key203;
	BgdlTaskPath key230;
	BgdlTaskPath contentId;		/* key 217 */
	int type = 0;			/* key 107 */
	int kind = 7;			/* key 111 */
	int key114 = 0;
	int count = 0;			/* key 500 */
	int index = -1;			/* key 501 */
	SceUInt32 digestId = 0;		/* key 236 */
	int key238 = 0;
	BgdlServerData data;		/* key 232 */
	char digest[32];		/* key 237 */
	int state;
	int status;
	int ret;

	sce_paf_memset(&data, 0, sizeof(data));
	data.titleId[0] = '\0';

	BgdlScopeTimer timer("download_package");

	Bgdl_81113890(1);
	ret = m_task->setState(2, &state, false);
	if (ret != 0)
		return ret;
	{
		EseParam param(m_task->id);

		ret = param.open();
		if (ret != 0)
			return ret;
		param.read(208, &current, sizeof(current));
		param.read(211, &offset, sizeof(offset));
		param.read(206, &total, sizeof(total));
		param.read(204, &lastModified);
		param.read(111, &kind, sizeof(kind));
		param.read(112, &append, sizeof(append));
		param.read(209, &referer);
		param.read(500, &count, sizeof(count));
		param.read(501, &index, sizeof(index));
		param.read(203, &key203);
		param.read(230, &key230);
		param.read(236, &digestId, sizeof(digestId));
		param.read(237, digest, sizeof(digest));
		param.read(110, &key110, sizeof(key110));
		param.read(218, &key218, sizeof(key218));
		param.read(239, &key239, sizeof(key239));
		param.read(217, &contentId);
		param.read(238, &key238, sizeof(key238));
		param.read(114, &key114, sizeof(key114));
		if (total == current)
			return 0;
		param.read(202, &url);
		if (url.m_end == url.m_begin) {
			*m_runStatus = 0;
			return BGDL_ERROR_NULL_FOLDER;
		}
		param.read(107, &type, sizeof(type));
		param.read(220, &titleId);
		if (titleId.m_end == titleId.m_begin) {
			*m_runStatus = 0;
			return BGDL_ERROR_NO_TITLE_ID;
		}
		if (type != 8 && param.read(232, &data, sizeof(data)) != 0) {
			*m_runStatus = 0;
			return BGDL_ERROR_NO_SERVER_DATA;
		}
	}
	if (bgdlManagerGet()->IsCancelled())
		return BGDL_ERROR_INTERRUPTED;
	if (current != 0 && current < 0x20000) {
		current = 0;
		Bgdl_811233A4(m_task->id, 208, &current, sizeof(current));
		m_task->setProgress(true, total, current);
	}

	BgdlGamePluginLock lock(false);

	if (key114 == 1)
		lock.Lock();

	BgdlAppFolder app(this);
	string folder;
	string pkgPath("");
	SceUInt64 reserve = 0;
	SceUInt64 size64 = 0;
	common::SharedPtr<MallocBuffer> buf = MallocBuffer::Allocate(0x8000);

	if (buf.get() == NULL || buf->GetSize() == 0) {
		*m_runStatus = 0;
		return BGDL_ERROR_NO_MEMORY;
	}

	BgdlTaskPath taskFolder;

	ret = m_task->GetFolder(&taskFolder, true);
	if (ret != 0)
		return ret;
	if (taskFolder.m_begin == NULL || taskFolder.m_end == taskFolder.m_begin) {
		*m_runStatus = 0;
		return BGDL_ERROR_INVALID_ARGUMENT;
	}
	folder = taskFolder.m_begin;
	if (type == 8) {
		char part[8] = "";

		folder += "/";
		folder += "p";
		sce_paf_snprintf(part, 7, "%04d", index);
		folder += "/";
		folder += part;
		ret = app.Set(false, folder.c_str());
	} else {
		ret = app.Prepare(current == 0, "p", 21, taskFolder.m_begin);
	}
	if (ret != 0)
		return ret;
	if (key230.m_end != key230.m_begin && key239 == false) {
		string package = app.GetPath();

		package += "/sce_sys/package";
		if (!Dir::IsExist(package.c_str())) {
			ret = Dir::CreateRecursive(package.c_str(), 6);
			if (ret != 0)
				return ret;
		}
		if (Dir::IsExist(package.c_str()) != true) {
			*m_runStatus = 0;
			return BGDL_ERROR_NO_KEY230;
		}
		package = app.GetPath();
		if (Bgdl_81113336(package.c_str()) == 0) {
			ret = Bgdl_8111322E(key230.m_begin, package.c_str(), NULL);
			if (ret != 0)
				return ret;
		}
	}
	ret = app.Open(current, kind == 6, key239);
	if (ret != 0)
		return ret;
	if (current != 0) {
		ret = app.Write(buf->GetAddr(), buf->GetSize());
		if (ret != 0) {
			*m_runStatus = 0;
			return ret;
		}

		SceUInt64 pos = app.Seek(0, SCE_SEEK_CUR);

		if (pos == 0) {
			current = 0;
			Bgdl_811233A4(m_task->id, 208, &current, sizeof(current));
			return BGDL_ERROR_RESUME_SIZE;
		}
		current = pos;
	}

	http::Client::Param clientParam(string(""));

	clientParam.m_resolve_timeout = 0;
	clientParam.m_resolve_retry = 0;
	clientParam.m_connect_timeout = 60;
	clientParam.m_send_timeout = 60;
	clientParam.m_recv_timeout = 60;

	http::Client client(clientParam);
	http::Transaction trans(client);
	BgdlHttpStateCheck check(this);

	{
		string target(url.m_begin);
		int method = 6;
		int requestKind = -1;

		if (key110)
			method = 5;
		switch (kind) {
		case 1:
		case 4:
			break;
		default:
			if (type == 8) {
				requestKind = 2;
			} else {
				switch (data.category) {
				case 24:
				case 29:
					requestKind = 4;
					break;
				default:
					requestKind = 0;
					break;
				}
			}
			break;
		}
		if (requestKind != -1) {
			BgdlHttpRequest request;

			request.Setup(method, requestKind, target.c_str(), digestId,
				      digestId != 0 ? digest : NULL, NULL, true, false);
			if (digestId == 0) {
				digestId = request.Bgdl_8118FD0C();
				if (digestId != 0) {
					Bgdl_811233A4(m_task->id, 236, &digestId, sizeof(digestId));
					request.GetDigest(digest);
					Bgdl_811233A4(m_task->id, 237, digest, sizeof(digest));
				}
			}

			const char *query = request.GetQuery();

			if (query != NULL && sce_paf_strlen(query) != 0)
				target += query;
		}
		ret = ScePafMisc_A86A5D0E(&trans, target.c_str(), "GET", &check);
	}
	if (ret != 0)
		return ret;
	bgdlSetRequestHeaders(&trans);
	if (referer.m_end != referer.m_begin) {
		common::SharedPtr<MallocBuffer> buf = MallocBuffer::Allocate(sce_paf_strlen(referer.m_begin) + 16);

		if (buf.get() == NULL || buf->GetSize() == 0)
			return BGDL_ERROR_NO_MEMORY;
		sce_paf_snprintf((char *)buf->GetAddr(), buf->GetSize() - 1, "%s", referer.m_begin);
		ret = trans.SetRequestHeader("Referer", (const char *)buf->GetAddr());
		if (ret != 0)
			return ret;
	}
	ret = m_task->getFilePath(&file, true);
	if (ret != 0)
		return ret;
	if (current != 0 && lastModified.m_end != lastModified.m_begin) {
		{
			common::SharedPtr<MallocBuffer> buf =
				MallocBuffer::Allocate(sce_paf_strlen("bytes=%llu-") + 64);

			if (buf.get() == NULL || buf->GetSize() == 0)
				return BGDL_ERROR_NO_MEMORY;
			sce_paf_snprintf((char *)buf->GetAddr(), buf->GetSize() - 1, "bytes=%llu-",
					 offset + current);
			ret = trans.SetRequestHeader("Range", (const char *)buf->GetAddr());
			if (ret != 0)
				return ret;
		}
		{
			common::SharedPtr<MallocBuffer> buf =
				MallocBuffer::Allocate(sce_paf_strlen(lastModified.m_begin) + 64);

			if (buf.get() == NULL || buf->GetSize() == 0)
				return BGDL_ERROR_NO_MEMORY;
			sce_paf_snprintf((char *)buf->GetAddr(), buf->GetSize() - 1, "%s",
					 lastModified.m_begin);
			ret = trans.SetRequestHeader("If-Range", (const char *)buf->GetAddr());
			if (ret != 0)
				return ret;
		}
	}

	SceUInt64 limit = 0;

	if (Bgdl_8126E252(4, &limit) != 0 || limit == 0)
		limit = 20 * 1024 * 1024;
	if (limit <= total)
		ScePafMisc_5BF60DE9(&trans, 2);
	ret = trans.SendRequest(NULL, 0);
	if (ret != 0)
		return ret;
	if (bgdlManagerGet()->IsCancelled())
		return BGDL_ERROR_INTERRUPTED;
	status = 0;
	trans.GetStatusCode(status);
	if ((status != 200 && status != 206) || (status == 200 && offset != 0)) {
		Bgdl_811233A4(m_task->id, 207, &status, sizeof(status));
		*m_runStatus = 0;
		return BGDL_ERROR_SERVER;
	}
	trans.GetContentLength(length);
	if (length <= 0) {
		*m_runStatus = 0;
		return BGDL_ERROR_NO_SIZE;
	}

	bool first = false;		/* the package's header is in the next data */

	if (status == 200) {
		if (current != 0)
			app.Seek(0, SCE_SEEK_SET);
		total = length;
		current = 0;
		first = true;
	}
	if (append == true && key203.m_end != key203.m_begin) {
		pkgPath = folder;
		pkgPath += "/";
		pkgPath += key203.m_begin;
		reserve = total > current ? total - current : 0;
		reserve += 0x2000000;
		if (key238 >= 2) {
			if (type != 8)
				index = 0;
			size64 = total + 0x2000000;
			ret = Bgdl_81112BD8(folder.c_str(), current, size64, index, key238);
		} else {
			ret = Bgdl_81112816(pkgPath.c_str(), reserve, NULL);
		}
		if (ret != 0) {
			*m_runStatus = 0;
			return BGDL_ERROR_NO_GAME_FOLDER;
		}
	}
	{
		EseParam param(m_task->id);

		ret = param.openWrite();
		if (ret != 0)
			return ret;

		string modified;

		if (trans.GetResponseHeader("Last-Modified", modified) == 0) {
			ret = param.write(204, modified.c_str(), modified.length() + 1);
			if (ret != 0)
				return ret;
		}
		if (status == 200) {
			ret = param.write(206, &length, sizeof(length));
			if (ret != 0)
				return ret;
		}
		ret = param.write(208, &current, sizeof(current));
		if (ret != 0)
			return ret;
		ret = param.commit();
		if (ret != 0)
			return ret;
	}
	m_task->setProgress(true, total, current);

	SceUInt64 written = 0;
	SceUInt32 size = 0;
	SceUInt64 step = (total / 100) & ~(SceUInt64)0x7FFF;
	SceUInt32 reported;
	SceUInt32 reservedMiB;

	if (step < 0x100000)
		step = 0x100000;
	else if (step > 0x800000)
		step = 0x800000;

	BgdlPowerTicker ticker(1);

	Bgdl_81113890(1);
	reported = 0;
	reservedMiB = 0;
	while (written < length) {
		ticker.Tick();
		ret = trans.ReceiveResponse((common::SharedPtr<Buffer> &)buf, &size);
		if (ret != 0) {
			if (bgdlManagerGet()->IsCancelled())
				ret = BGDL_ERROR_INTERRUPTED;
			break;
		}
		if (size == 0) {
			if (written < length)
				ret = BGDL_ERROR_SHORT_RESPONSE;
			break;
		}
		if (buf->GetSize() > size && written + size < length) {
			ret = BGDL_ERROR_SHORT_RESPONSE;
			break;
		}
		if (first == true) {
			sce_paf_memset(&data, 0, sizeof(data));
			ret = Bgdl_8136D9B8(&data, buf->GetAddr(), buf->GetSize());
			if (ret != 0) {
				*m_runStatus = 0;
				break;
			}
			if (contentId.m_end != contentId.m_begin &&
			    sce_paf_strncmp(contentId.m_begin, data.contentId, sizeof(data.contentId)) != 0) {
				*m_runStatus = 0;
				ret = BGDL_ERROR_CONTENT_ID;
				break;
			}
			first = false;
			if (type == 8) {
				ret = Bgdl_811233A4(m_task->id, 232, &data, sizeof(data));
				if (ret != 0)
					break;
			}
		}

		const char *p = (const char *)buf->GetAddr();
		int left = size;

		ret = 0;
		while (left != 0) {
			int n = app.Unpack(p, left);

			if (n < 0) {
				*m_runStatus = 0;
				ret = n;
				break;
			}
			if (bgdlManagerGet()->IsCancelled()) {
				ret = BGDL_ERROR_INTERRUPTED;
				break;
			}
			if (bgdlManagerGet()->IsSuspended() == true) {
				ret = BGDL_ERROR_INTERRUPTED;
				break;
			}
			if (bgdlManagerGet()->IsHeld() == true) {
				ret = BGDL_ERROR_INTERRUPTED;
				break;
			}
			ticker.Tick();
			if (n != 0) {
				left -= n;
				if (left < 0) {
					ret = BGDL_ERROR_WRITE;
					break;
				}
				p += n;
			}
		}
		if (ret != 0)
			break;
		current += size;
		written += size;
		m_task->updateProgress(true, current);
		if (written != 0 && reported < written / step) {
			Bgdl_811233A4(m_task->id, 208, &current, sizeof(current));
			reported = written / step;
		}
		if (pkgPath.length() != 0 && reserve != 0 && written != 0 &&
		    reservedMiB < (written >> 20)) {
			reservedMiB = written >> 20;
			reserve = reserve >= 0x100000 ? reserve - 0x100000 : 0;
			if (key238 >= 2)
				Bgdl_81112BD8(folder.c_str(), current, size64, index, key238);
			else
				Bgdl_81112816(pkgPath.c_str(), reserve, NULL);
		}
		thread::Sleep(1);
	}
	if (written == length) {
		if (key238 >= 2) {
			Bgdl_81112FAA(folder.c_str(), size64, index);
		} else if (pkgPath.length() != 0) {
			LocalFileStat stat;

			LocalFile::Getstat(pkgPath.c_str(), &stat);
			Dir::Remove(pkgPath.c_str());
		}
		ret = 0;
	}
	if (written != 0) {
		int err = Bgdl_811233A4(m_task->id, 208, &current, sizeof(current));
		EseParam param(m_task->id);

		if (err != 0)
			return err;
	}
	return ret;
}

/*
 * (0x8111B06A) the task's package or file downloaded, again up to three more times a second
 * apart while the run status allows it
 */
int BgdlHttpDownload::Download(void)
{
	int retries = 0;
	bool content;
	int ret;

	for (;;) {
		if (bgdlManagerGet()->IsSuspended() == true) {
			ret = BGDL_ERROR_INTERRUPTED;
			break;
		}
		bgdlManagerGet()->SetHold(false);
		switch (m_task->m_ParamCache.key107) {
		case 12:
		case 8:
		case 7:
		case 6:
			content = false;
			ret = download_package();
			break;
		default:
			content = true;
			ret = download_content();
			break;
		}
		if (ret == 0)
			break;
		if (bgdlManagerGet()->IsSuspended() == true) {
			*m_runStatus = 0;
			break;
		}
		if (bgdlManagerGet()->IsHeld() == true) {
			retries = 0;
			bgdlManagerGet()->SetHold(false);
		}
		if (ret == BGDL_ERROR_INTERRUPTED)
			break;
		if (ret == (int)0x80010013) {
			ret = BGDL_ERROR_INTERRUPTED;
			break;
		}
		if (ret == (int)0x804101A1) {
			int kind = 4;

			ret = BGDL_ERROR_HTTP_STOPPED;
			if (content == true) {
				int key111 = 7;
				int type = 0;

				Bgdl_8112260C(m_task->id, 107, &type, sizeof(type));
				if (Bgdl_8112260C(m_task->id, 111, &key111, sizeof(key111)) == 0) {
					if (key111 == 1)
						kind = 3;
					else if (type == 5)
						kind = 2;
				}
			}
			if (Bgdl_8126E2AE(kind) == 0)
				ret = BGDL_ERROR_HTTP_STOPPED_KIND;
			*m_runStatus = 0;
			break;
		}
		if (*m_runStatus == 0)
			break;
		if (retries++ >= 3) {
			*m_runStatus = 0;
			break;
		}
		thread::Sleep(1000);
	}
	return ret;
}

/* (0x8111B1CA) a package's first 32 KiB received and its header read into data */
static int bgdlReceivePackageHeader(http::Transaction *trans, BgdlServerData *data)
{
	BgdlServerData local;

	if (data == NULL)
		data = &local;

	common::SharedPtr<MallocBuffer> header = MallocBuffer::Allocate(0x8000);

	if (header.get() == NULL || header->GetSize() == 0)
		return BGDL_ERROR_NO_MEMORY;

	common::SharedPtr<MallocBuffer> buf = MallocBuffer::Allocate(0x8000);

	if (buf.get() == NULL || buf->GetSize() == 0)
		return BGDL_ERROR_NO_MEMORY;

	SceSize want = header->GetSize();
	SceSize got = 0;
	SceUInt32 size = 0;
	int ret;

	if (want == 0)
		return BGDL_ERROR_NO_HEADER;
	for (;;) {
		ret = trans->ReceiveResponse((common::SharedPtr<Buffer> &)buf, &size);
		if (ret != 0)
			return ret;
		if (size == 0)
			break;

		SceSize n = size;

		if (size + got > want)
			n = want - got;
		sce_paf_memcpy((char *)header->GetAddr() + got, buf->GetAddr(), n);
		got += size;
		if (want <= got)
			break;
	}
	return Bgdl_8136D9B8(data, header->GetAddr(), header->GetSize());
}

/* a content kind's file extensions and the MIME type its server must send (PLACEHOLDER) */
struct BgdlContentType {
	int type;
	const char **extensions;
	const char *mimeType;
};

static const char *s_photoExtensions[] = {
	"jpeg", "jpg", "png", "gif", "bmp", "tiff", "tif", "mpo", NULL
};
static const char *s_videoExtensions[] = {"mp4", "mgv", "m4v", NULL};
static const char *s_musicExtensions[] = {
	"mp3", "mp4", "m4a", "ma4", "m4b", "aac", "3gp", "3g2", "3gp2", "wav", NULL
};
static const char *s_packageExtensions[] = {"PKG", NULL};
static const char *s_mnvExtensions[] = {"MNV", NULL};
static const char *s_pupExtensions[] = {"PUP", NULL};
static const char *s_textMimeTypes[] = {"text/html", "text/plain", NULL};

static const BgdlContentType s_contentTypes[] = {
	{1, s_photoExtensions, NULL},
	{3, s_videoExtensions, NULL},
	{2, s_musicExtensions, NULL},
	{6, s_packageExtensions, NULL},
	{4, s_mnvExtensions, "video/vnd.sony.mnv"},
	{5, s_pupExtensions, NULL},
	{0, NULL, NULL}
};

/* the media kinds alone (mode 1) */
static const BgdlContentType s_mediaTypes[] = {
	{1, s_photoExtensions, NULL},
	{3, s_videoExtensions, NULL},
	{2, s_musicExtensions, NULL},
	{0, NULL, NULL}
};

/* (0x8111B63C) the content kind of file from its extension, checked against the MIME type */
static __attribute__((noinline)) int bgdlGetContentType(int mode, int *type, const string *file, const string *mimeType)
{
	const BgdlContentType *table;
	const char *ext;
	int i;

	*type = 0;
	ext = sce_paf_strrchr(file->c_str(), '.');
	if (ext == NULL)
		return BGDL_ERROR_BAD_KIND;
	ext++;
	table = s_contentTypes;
	if (mode == 1)
		table = s_mediaTypes;
	bool found = false;

	for (i = 0; table[i].type != 0; i++) {
		const char **e;

		for (e = table[i].extensions; *e != NULL; e++) {
			if (sce_paf_strcasecmp(ext, *e) == 0) {
				found = true;
				break;
			}
		}
		if (found)
			break;
	}
	if (!found)
		return BGDL_ERROR_BAD_KIND;
	if (mode == 1) {
		const char **t;

		for (t = s_textMimeTypes; *t != NULL; t++) {
			if (sce_paf_strcasecmp(mimeType->c_str(), *t) == 0) {
				*type = 0;
				return BGDL_ERROR_BAD_MIME_TYPE;
			}
		}
	}
	*type = table[i].type;
	if (mimeType->length() != 0 && table[i].mimeType != NULL &&
	    sce_paf_strcasecmp(mimeType->c_str(), table[i].mimeType) != 0) {
		*type = 0;
		return BGDL_ERROR_BAD_MIME_TYPE;
	}
	return 0;
}

/* a kind 21 package taken as kind 9 when its title is installed on a storage of kind 3 */
static inline void bgdlCheckInstalledStorage(BgdlServerInfo *info)
{
	LsdbAppInfoPtr app(new LsdbAppInfo());
	int storage;

	if (Bgdl_81123BC0(info->data.titleId, &app) == 0 && Bgdl_81113898(&app, &storage) == 0 &&
	    storage == 3)
		info->kind = 9;
}

int GetHeaderInfo(int mode, int flags, const char *device, const char *url, const char *user,
		  const char *password, const char *reserved, const char *contentId,
		  BgdlServerInfo *info, BgdlServerParam *param, MtpBgCopyHost *host, int *checkResult,
		  char *digest)
{
	BgdlScopeTimer timer("GetHeaderInfo");
	int ret;

	info->Clear();
	if (url == NULL || info == NULL || url[0] == '\0') {
		info->status = BGDL_ERROR_NULL_TITLE;
		return BGDL_ERROR_NULL_TITLE;
	}
	if (sce_paf_private_strtok(url, "file://") != NULL)
		return 0;
	do {
		info->storageSize = 0;
		info->storageUsed = 0;
		info->storageNeeded = 0;
		info->storageFree = 0;

		BgdlStorageInfo storage;

		if (device != NULL) {
			ret = Bgdl_8111234C(device, &storage);
			if (ret != 0) {
				info->status = ret;
			} else {
				info->storageSize = storage.max;
				info->storageFree = storage.free;
				info->storageUsed = storage.max - storage.free;
				info->storageNeeded = storage.clusterSize * 6;
			}
		}
		if (sce_paf_private_strtok(url, "http://") == NULL &&
		    sce_paf_private_strtok(url, "https://") == NULL) {
			ret = BGDL_ERROR_NOT_HTTP;
			break;
		}

		http::Client::Param clientParam(string(""));

		if (param != NULL) {
			clientParam.m_resolve_timeout = param->value[0];
			clientParam.m_resolve_retry = param->value[1];
			clientParam.m_connect_timeout = param->value[2];
			clientParam.m_send_timeout = param->value[3];
			clientParam.m_recv_timeout = param->value[4];
		}
		if (user != NULL)
			clientParam.m_user_name = user;
		if (password != NULL)
			clientParam.m_password = password;
		if (reserved != NULL)
			clientParam.m_reserved = reserved;

		http::Client client(clientParam);
		http::Transaction trans(client);

		ret = ScePafMisc_A86A5D0E(&trans, url, "HEAD", (StateCheck *)host);
		if (ret != 0)
			break;
		if (mode == 1)
			trans.m_ssl_error_mask = 2;
		ret = bgdlSetRequestHeaders(&trans);
		if (ret != 0)
			break;
		ret = trans.SendRequest(NULL, 0);
		if (ret != 0)
			break;
		info->httpStatus = 0;
		trans.GetStatusCode(info->httpStatus);
		if (info->httpStatus != 200) {
			ret = BGDL_ERROR_SERVER;
			if (info->httpStatus == 401)
				trans.GetRealm(info->realm);
			break;
		}
		trans.GetFileName(info->fileName);
		trans.GetMimeType(info->mimeType);
		trans.GetResponseHeader("Last-Modified", info->lastModified);
		trans.GetContentLength(info->size);
		if (info->size <= 0) {
			ret = BGDL_ERROR_NO_SIZE;
			break;
		}
		info->flag = false;
		ret = bgdlGetContentType(mode, &info->kind, &info->fileName, &info->mimeType);
		if (ret != 0)
			return ret;
		if (info->kind >= 1 && info->kind <= 3 && info->fileName.length() != 0) {
			string decoded("");

			decoded = http::url::Decode(info->fileName.c_str());
			if (decoded.length() != 0)
				info->fileName = decoded.c_str();
		}
		if (info->kind == 6) {
			http::Transaction get(client);

			if (checkResult == NULL || mode == 1 || mode == 4) {
				ScePafMisc_A86A5D0E(&get, url, "GET", (StateCheck *)host);
			} else {
				string target(url);
				BgdlHttpRequest request;

				request.Setup(6, 0, target.c_str(), 0, NULL, NULL, true, false);
				*checkResult = request.Bgdl_8118FD0C();
				if (digest != NULL)
					request.GetDigest(digest);

				const char *query = request.GetQuery();

				if (query != NULL && sce_paf_strlen(query) != 0)
					target += query;
				ScePafMisc_A86A5D0E(&get, target.c_str(), "GET", (StateCheck *)host);
			}
			ret = bgdlSetRequestHeaders(&get);
			if (ret != 0)
				break;
			ret = get.SendRequest(NULL, 0);
			if (ret != 0)
				break;

			int status = 0;

			get.GetStatusCode(status);
			if (status != 200) {
				ret = BGDL_ERROR_SERVER;
				break;
			}
			ret = bgdlReceivePackageHeader(&get, &info->data);
			if (ret != 0)
				break;
			switch (info->data.category) {
			case 6:
			case 7:
			case 14:
			case 15:
			case 16:
				break;
			case 21:
				if ((info->data.attribute & 0x1000) != 0) {
					if ((flags & 1) == 0)
						bgdlCheckInstalledStorage(info);
				} else if ((flags & 1) == 0) {
					bgdlCheckInstalledStorage(info);
				}
				break;
			case 22:
				info->kind = 7;
				break;
			case 24:
			case 29:
				info->kind = 6;
				break;
			case 31:
				info->kind = 12;
				break;
			default:
				ret = BGDL_ERROR_BAD_STORAGE;
				break;
			}
		}
		if (ret != 0)
			break;
		if (info->storageNeeded != 0) {
			switch (info->kind) {
			case 9:
				info->storageNeeded += 4 * storage.clusterSize;
				break;
			case 12:
			case 8:
			case 7:
			case 6:
				info->storageNeeded += Bgdl_81112310(info->size + 0x2000000, storage.clusterSize);
				break;
			default:
				info->storageNeeded += Bgdl_81112310(info->size, storage.clusterSize);
				info->storageNeeded += Bgdl_81112310(0x200000, storage.clusterSize);
				break;
			}
		}
		switch (info->kind) {
		case 12:
		case 8:
		case 7:
		case 6:
			if (contentId != NULL &&
			    sce_paf_strncmp(contentId, info->data.contentId, sizeof(info->data.contentId)) != 0)
				ret = BGDL_ERROR_CONTENT_ID;
			break;
		}
		if (ret != 0)
			break;
		info->flag = true;
		ret = 0;
	} while (0);
	info->status = ret;
	return ret;
}

/*
 * (0x8111C702) PLACEHOLDER name: before a transfer, the url's headers asked of its server
 * (GetHeaderInfo) and what they say written to the task's param file: its size, file name,
 * Last-Modified, kind and, for a package, its header. A task already typed (key 107) is left
 * as it is.
 */
int BgdlHttpDownload::Prepare(void)
{
	BgdlTaskPath url;		/* key 202 */
	BgdlTaskPath folder;		/* key 222 */
	BgdlTaskPath contentId;		/* key 217 */
	BgdlTaskPath userKey;		/* key 200 */
	BgdlTaskPath passwordKey;	/* key 201 */
	BgdlTaskPath referer;		/* key 209 */
	string name("");		/* key 203 */
	string target;			/* key 229 */
	SceUInt64 total = (SceUInt64)-1;	/* key 206 */
	SceUInt64 current = (SceUInt64)-1;	/* key 208 */
	string path("");
	string folderName("");
	string reserved("");		/* PLACEHOLDER: Sony's never uses it */
	string user;
	string password;
	int kind = 7;			/* key 111 */
	BgdlServerInfo info;
	BgdlEvent event;
	int state;
	int ret;

	Bgdl_81113890(1);
	ret = m_task->setState(1, &state, true);
	if (ret != 0)
		return ret;
	{
		EseParam param(m_task->id);

		ret = param.open();
		if (ret != 0)
			return ret;

		int type = 0;		/* key 107 */

		ret = param.read(107, &type, sizeof(type));
		if (ret != 0)
			return ret;
		if (type != 0)
			return 0;
		ret = param.read(111, &kind, sizeof(kind));
		if (ret != 0)
			return ret;
		param.read(202, &url);
		if (url.m_end == url.m_begin) {
			*m_runStatus = 0;
			return BGDL_ERROR_NO_URL;
		}
		param.read(222, &folder);
		param.read(217, &contentId);
		param.read(200, &userKey);
		if (userKey.m_end != userKey.m_begin)
			Bgdl_811221E0(userKey.m_begin, 31, &user);
		param.read(201, &passwordKey);
		if (passwordKey.m_end != passwordKey.m_begin)
			Bgdl_811221E0(passwordKey.m_begin, 31, &password);
		param.read(209, &referer);
		param.read(229, &target);
		param.read(203, &name);
		param.read(206, &total, sizeof(total));
		param.read(208, &current, sizeof(current));
	}
	{
		BgdlHttpStateCheck check(this);

		ret = GetHeaderInfo(kind, 0, "ux0:", url.m_begin,
				    user.length() != 0 ? user.c_str() : NULL,
				    password.length() != 0 ? password.c_str() : NULL,
				    referer.m_end != referer.m_begin ? referer.m_begin : NULL,
				    contentId.m_end != contentId.m_begin ? contentId.m_begin : NULL,
				    &info, NULL, (MtpBgCopyHost *)(void *)&check, NULL, NULL);
		if (ret != 0) {
			*m_runStatus = 0;
			return ret;
		}
		if (bgdlManagerGet()->IsCancelled())
			return BGDL_ERROR_INTERRUPTED;
		if (name.length() == 0) {
			if (info.fileName.length() != 0)
				name = info.fileName.c_str();
			if (name.length() == 0) {
				*m_runStatus = 0;
				return BGDL_ERROR_NO_FILE;
			}
		}
	}
	if (current == 0 && total == 0 && target.length() != 0) {
		path = target;
		if (name.c_str()[0] != '/')
			path += "/";
		path.append(name);
	}

	const char *base = NULL;
	bool removeExisting = true;

	if (folder.m_begin != NULL)
		base = folder.m_begin;
	if (info.kind == 4) {
		if (base == NULL || sce_paf_strlen(base) == 0) {
			removeExisting = false;
		} else {
			string baseName(base);

			if (Bgdl_81111B96(&name, &baseName, &folderName) != 0)
				removeExisting = false;
		}
	}
	if (removeExisting && path.length() != 0 && LocalFile::Exists(path.c_str()) == true)
		LocalFile::RemoveFile(path.c_str());
	{
		EseParam param(m_task->id);
		SceUInt64 start;
		bool key205;
		bool key102;

		ret = param.openWrite();
		if (ret != 0)
			return ret;
		ret = param.write(206, &info.size, sizeof(info.size));
		if (ret != 0)
			return ret;
		start = 0;
		if (current != (SceUInt64)-1 && info.size == total) {
			start = current;
		} else {
			ret = param.write(208, &start, sizeof(start));
			if (ret != 0)
				return ret;
		}
		m_task->status.id = m_task->id;
		m_task->status.current = start;
		m_task->status.total = info.size;
		m_task->transfer.Start(info.size, start, 0, 512000);
		bgdlManagerGet()->SetStatus(&m_task->status);
		if (folderName.length() != 0)
			ret = param.write(203, folderName.c_str(), folderName.length() + 1);
		else
			ret = param.write(203, name.c_str(), name.length() + 1);
		if (ret != 0)
			return ret;
		if (info.lastModified.length() != 0) {
			ret = param.write(204, info.lastModified.c_str(), info.lastModified.length() + 1);
			if (ret != 0)
				return ret;
		}
		key205 = false;
		key102 = true;
		switch (info.kind) {
		case 1:
		case 2:
		case 3:
		case 4:
			key205 = true;
			break;
		case 5:
			key102 = false;
			key205 = false;
			break;
		case 6:
		case 7:
		case 8:
			switch (info.kind) {
			case 6:
				key205 = true;
				break;
			case 7:
			case 8:
				key205 = false;
				key102 = false;
				break;
			}
			if (info.data.titleId[0] != '\0') {
				ret = param.write(220, info.data.titleId, sce_paf_strlen(info.data.titleId) + 1);
				if (ret != 0)
					return ret;
			}
			ret = param.write(232, &info.data, sizeof(info.data));
			if (ret != 0)
				return ret;
			break;
		default:
			key205 = false;
			break;
		}
		ret = param.write(205, &key205, sizeof(key205));
		if (ret != 0)
			return ret;
		ret = param.write(102, &key102, sizeof(key102));
		if (ret != 0)
			return ret;
		m_task->m_ParamCache.key102 = key102;
		ret = param.write(107, &info.kind, sizeof(info.kind));
		if (ret != 0)
			return ret;
		m_task->m_ParamCache.key107 = info.kind;
		ret = param.commit();
		if (ret != 0)
			return ret;
	}
	event.type = 3;
	event.id = m_task->id;
	event.param = m_task->m_ParamCache;
	event.result = 0;
	event.reserved = 0;
	event.value = 0;
	bgdlManagerGet()->NotifyListeners(&event);
	thread::Sleep(1000);
	return ret;
}

static BgdlGamePluginLoader *s_gamePluginLoader = NULL;	/* (0x815447B4) */

static void bgdlGamePluginLoaded(Plugin *plugin);

/* (0x8111E14C) game_plugin's interface, its plugin loaded first when it is not there; its
   load waited for half a second at a time, 12 times at most or until the manager is
   cancelled */
GamePluginInterface *BgdlGamePluginLoader::Load(void)
{
	GamePluginInterface *game;

	m_plugin = Plugin::Find("game_plugin");
	if (m_plugin != NULL) {
		game = (GamePluginInterface *)m_plugin->GetInterface(1);
		m_interface = game;
	} else {
		{
			Plugin::InitParam param;

			param.name = "game_plugin";
			param.private_heap_size = 0x4000;
			param.caller_name = "__main__";
			param.module_file = "vs0:vsh/game/game_plugin.suprx";
			param.module_interface_version = 1;
			param.module_option = 1;
			Plugin::LoadAsync(param, bgdlGamePluginLoaded, 0);

			int tries = 0;

			for (;;) {
				bool loaded;

				m_mutex.Lock();
				loaded = m_loaded;
				m_mutex.Unlock();
				if (loaded == true)
					break;
				if (tries++ > 10)
					break;
				bgdlManagerGet()->IsCancelled();
				if (bgdlManagerGet()->IsCancelled())
					break;
				thread::Sleep(500);
			}
		}
		game = m_interface;
	}
	if (game == NULL && m_plugin != NULL)
		m_plugin = NULL;
	return game;
}

/* (0x8111E2C0) PLACEHOLDER name: the header of the package file at path read into data */
static __attribute__((noinline)) int bgdlReadPackageHeader(const char *path, BgdlServerData *data)
{
	int ret;

	if (path == NULL || data == NULL)
		return BGDL_ERROR_NULL_TITLE;
	ret = 0;

	common::SharedPtr<MallocBuffer> buf = MallocBuffer::Allocate(0x8000);

	if (buf.get() == NULL || buf->GetSize() == 0) {
		ret = BGDL_ERROR_NO_MEMORY;
		return ret;
	}

	common::SharedPtr<File> file = LocalFile::Open(path, SCE_O_RDONLY | 0x10000, 6, &ret);

	if (ret != 0)
		return ret;

	BgdlFileCloser closer(file);
	int n = file->Read(buf->GetAddr(), buf->GetSize());

	if (n > 0)
		ret = Bgdl_8136D9B8(data, buf->GetAddr(), buf->GetSize());
	else
		ret = n;
	return ret;
}

/* (0x8111E7F2) the file's progress shown while it is unpacked; what was unpacked removed again
   when it fails */
int BgdlAppFolder::UnpackFile(bool remove, const char *file, const char *titleId, const char *base)
{
	int ret = -1;
	BgdlServerData data;
	SceInt64 size;
	SceInt64 done;
	common::SharedPtr<File> fp;

	sce_paf_memset(&data, 0, sizeof(data));
	done = 0;
	do {
		if (file == NULL || titleId == NULL)
			break;
		size = LocalFile::GetFileSize(file);
		if (size <= 0)
			break;
		ret = bgdlReadPackageHeader(file, &data);
		if (ret != 0)
			break;
		{
			EseTask *task = m_download->m_task;

			task->status.id = task->id;
			task->status.current = 0;
			task->status.total = size;
			task->transfer.Start(size, 0, 0, 512000);
			bgdlManagerGet()->SetStatus(&task->status);
		}
		fp = LocalFile::Open(file, SCE_O_RDONLY | 0x10000, 6, &ret);
		if (ret != 0)
			break;
		{
			BgdlFileCloser closer(fp);

			ret = doPrepare(remove, "p", base);
			ret = doOpen(0, false, false);
			if (ret != 0)
				break;

			common::SharedPtr<MallocBuffer> buf = MallocBuffer::Allocate(0x8000);

			if (buf.get() == NULL || buf->GetSize() == 0) {
				ret = BGDL_ERROR_NO_MEMORY;
				break;
			}
			while (done < size) {
				int n;
				int r;

				bgdlManagerGet()->IsCancelled();
				if (bgdlManagerGet()->IsCancelled() == true) {
					ret = BGDL_ERROR_INTERRUPTED;
					break;
				}
				n = fp->Read(buf->GetAddr(), buf->GetSize());
				if (n <= 0) {
					ret = 0;
					if (n < 0)
						ret = n;
					break;
				}
				r = doUnpack(buf->GetAddr(), n);
				if (r <= 0) {
					ret = r;
					break;
				}
				done += r;
				m_download->m_task->doUpdateProgress(true, done);
			}
			if (ret == 0) {
				ret = -1;
				if (done >= size)
					ret = 0;
			}
		}
	} while (0);
	if (ret != 0 && done != 0) {
		string path = m_path;

		if (path.length() != 0)
			LocalFile::RemoveFile(path.c_str());
	}
	Close();
	return ret;
}

/*
 * (0x8111F12C) a task whose url is file:// (a package already on a device, or host0:) put in
 * place without a download: its header read, the package unpacked into the task's folder
 * (or ux0:app/), moved to its title's folder and installed with game_plugin. False, with
 * ret the error, when the url is not file:// or the param file cannot be read; true when
 * the task was handled, ret its result.
 */
bool BgdlHttpDownload::InstallLocal(int *ret)
{
	string path("");
	string reserved("");		/* PLACEHOLDER: Sony's never uses it */
	string name("");
	int type = 0;
	string mimeType("");
	BgdlTaskPath url;		/* key 202 */
	int state = 0;
	bool key205 = true;
	BgdlServerData data;
	bool install;

	sce_paf_memset(&data, 0, sizeof(data));
	data.titleId[0] = '\0';
	{
		EseParam param(m_task->id);

		*ret = param.open();
		if (*ret != 0)
			return false;
		param.read(202, &url);
		if (url.m_end == url.m_begin) {
			*ret = BGDL_ERROR_NO_URL;
			return false;
		}
	}

	const char *local = sce_paf_private_strtok(url.m_begin, "file://");

	if (local == NULL) {
		*ret = BGDL_ERROR_NO_URL;
		return false;
	}
	path = local + sce_paf_strlen("file://");
	do {
		if (sce_paf_private_strtok(url.m_begin, "host0:") == NULL) {
			const char *colon = sce_paf_strrchr(path.c_str(), ':');

			if (colon != NULL)
				name = colon + 1;
			if (name.length() == 0) {
				*m_runStatus = 0;
				*ret = BGDL_ERROR_NO_URL;
				break;
			}
		} else if (Bgdl_81111C2C(&path, &name) != true || name.length() == 0) {
			*m_runStatus = 0;
			*ret = BGDL_ERROR_NO_URL;
			break;
		}
		*ret = m_task->setState(1, &state, false);
		if (*ret != 0)
			break;
		install = true;
		if (state == 1) {
			*ret = bgdlGetContentType(0, &type, &path, &mimeType);
			if (*ret != 0) {
				*m_runStatus = 0;
				break;
			}
			if (type != 6) {
				*ret = BGDL_ERROR_NOT_PACKAGE;
				*m_runStatus = 0;
				break;
			}
			*ret = bgdlReadPackageHeader(path.c_str(), &data);
			if (*ret != 0)
				break;
			if (data.category == 22) {
				type = 7;
				install = false;
				key205 = false;
			} else {
				install = true;
				if ((data.attribute & 0x10) != 0) {
					install = false;
					key205 = false;
				}
			}
			{
				EseParam param(m_task->id);

				*ret = param.openWrite();
				if (*ret != 0)
					break;
				*ret = param.write(203, name.c_str(), name.length() + 1);
				if (*ret != 0)
					break;
				m_task->status.id = m_task->id;
				m_task->status.current = 0;
				m_task->status.total = 0;
				m_task->transfer.Start(0, 0, 0, 512000);
				bgdlManagerGet()->SetStatus(&m_task->status);
				*ret = param.write(107, &type, sizeof(type));
				if (*ret != 0)
					break;
				*ret = param.write(205, &key205, sizeof(key205));
				if (*ret != 0)
					break;
				if (key205 == true) {
					bool key102 = true;

					*ret = param.write(102, &key102, sizeof(key102));
					if (*ret != 0)
						break;
					m_task->m_ParamCache.key102 = key102;
				}
				if (data.titleId[0] != '\0') {
					*ret = param.write(220, data.titleId, sce_paf_strlen(data.titleId) + 1);
					if (*ret != 0)
						break;

					BgdlTaskPackage package;

					package.key220 = data.titleId;
					package.serverCategory = data.category;
					package.serverAttribute = data.attribute;
					m_task->doSetPackage(&package);
				}
				*ret = param.write(232, &data, sizeof(data));
				if (*ret != 0)
					break;
				*ret = param.commit();
				if (*ret != 0)
					break;
			}
			{
				BgdlTaskPath titleId;

				Bgdl_81122700(m_task->id, 220, &titleId);
			}
			*ret = m_task->setState(2, &state, false);
			if (*ret != 0)
				break;
		}
		{
			string from;
			string to;

			if (state == 2) {
				BgdlAppFolder folder(this);
				BgdlTaskPath base;

				*ret = m_task->GetFolder(&base, true);
				if (*ret != 0)
					break;
				*ret = folder.UnpackFile(install, path.c_str(), data.titleId, base.m_begin);
				if (*ret != 0)
					break;
				{
					string unpacked = folder.m_path;

					from = unpacked;
				}
				*ret = folder.doPrepare(false, data.titleId, base.m_begin);
				if (*ret != 0)
					break;
				{
					string title = folder.m_path;

					to = title;
				}
				if (Dir::IsExist(to.c_str()) == true) {
					*ret = Dir::RemoveRecursive(to.c_str());
					if (*ret != 0)
						break;
				}
				*ret = LocalFile::RenameFile(from.c_str(), to.c_str());
				if (*ret != 0)
					break;
				*ret = m_task->setState(3, &state, false);
				if (*ret != 0)
					break;
			}
			if (state == 3) {
				if (key205 == true) {
					if (to.length() != 0) {
						BgdlGamePluginLoader loader(this);
						const char *folderPath = to.c_str();
						GamePluginInterface *game;
						int result;

						s_gamePluginLoader = &loader;
						game = loader.Load();
						result = BGDL_ERROR_NO_GAME_PLUGIN;
						if (game != NULL) {
							GameInstallExParam param;

							sce_paf_memset(&param, 0, sizeof(param));
							param.storage = 0;
							param.noOtherFlag = false;
							param.imported = false;
							sce_paf_snprintf(param.path, sizeof(param.path) - 1, "%s", folderPath);
							result = ((int (*)(GameInstallExParam *))
								  game->func[GAME_PLUGIN_FUNC_INSTALL_EX])(&param);
						}
						*ret = result;
						s_gamePluginLoader = NULL;
					}
				} else {
					bool key113 = true;

					m_task->m_ParamCache.key113 = true;
					*ret = Bgdl_811233A4(m_task->id, 113, &key113, sizeof(key113));
				}
			}
		}
	} while (0);
	*m_runStatus = 0;
	return true;
}

/* (0x8111FD8E) a file:// task installed, or the task's headers asked, downloaded, put in place
   and installed, then its next file started; finished, the task's state 4 */
int BgdlHttpDownload::Run(void)
{
	int ret;

	if (InstallLocal(&ret) == true)
		return ret;
	ret = Prepare();
	bgdlManagerGet()->SetCancelHandler(NULL, NULL);
	if (ret != 0)
		return ret;
	Bgdl_81113890(1);

	bool start = false;

	{
		EseParam param(m_task->id);
		int r = param.open();

		if (r != 0)
			return r;

		int number = 0;

		param.read(500, &number, sizeof(number));
		if (number != 0) {
			int count = -1;

			param.read(501, &count, sizeof(count));
			start = count == -1;
		}
	}
	int result;

	if (start) {
		result = StartFile(0);
		if (result != 0)
			return result;
	}
	ret = 0;
	ret = Download();
	bgdlManagerGet()->SetCancelHandler(NULL, NULL);
	if (ret != 0)
		return ret;
	result = Complete();
	if (result != 0)
		return result;
	ret = 0;
	result = InstallContent();
	if (result != 0)
		return result;
	ret = 0;
	result = NextFile();
	if (result != 0)
		return result;
	ret = 0;
	*m_runStatus = 0;
	m_task->setState(4, NULL, false);
	return 0;
}

/* (0x8111FEBC) the task run until it does not ask to be run again */
int EseTask::execute(int *runStatus)
{
	int ret;

	do {
		BgdlHttpDownload download(this, runStatus);

		ret = download.Run();
	} while (ret == (int)BGDL_ERROR_RETRY);
	return ret;
}

/* (0x8111FEF8) */
int Bgdl_8111FEF8(const char *device, const char *url, int flags, int unusedOption, MtpBgCopyHost *host,
		  BgdlServerParam *param, const char *folder, const char *name, int *status,
		  BgdlServerInfo *server, string *path)
{
	int ret;

	do {
		if (url == NULL || url[0] == '\0' || server == NULL) {
			ret = BGDL_ERROR_NULL_TITLE;
			break;
		}
		ret = 0;
		if (sce_paf_private_strtok(url, "http://") == NULL &&
		    sce_paf_private_strtok(url, "https://") == NULL) {
			ret = BGDL_ERROR_NOT_HTTP;
			break;
		}
		server->Clear();
		{
			BgdlStorageInfo storage;

			if (device != NULL) {
				ret = Bgdl_8111234C(device, &storage);
				if (ret != 0) {
					server->status = ret;
				} else {
					server->storageSize = storage.max;
					server->storageFree = storage.free;
					server->storageUsed = storage.max - storage.free;
					server->storageNeeded = storage.clusterSize * 6;
				}
			}

			http::Client::Param clientParam(string(""));

			if (param != NULL) {
				clientParam.m_resolve_timeout = param->value[0];
				clientParam.m_resolve_retry = param->value[1];
				clientParam.m_connect_timeout = param->value[2];
				clientParam.m_send_timeout = param->value[3];
				clientParam.m_recv_timeout = param->value[4];
			}

			http::Client client(clientParam);
			http::Transaction trans(client);

			ret = ScePafMisc_A86A5D0E(&trans, url, "GET", (StateCheck *)host);
			ret = bgdlSetRequestHeaders(&trans);
			if (ret != 0)
				break;
			ret = trans.SendRequest(NULL, 0);
			if (ret != 0)
				break;

			int code = 0;

			trans.GetStatusCode(code);
			if (status != NULL)
				*status = code;
			if (code != 200) {
				ret = BGDL_ERROR_SERVER;
				break;
			}
			trans.GetFileName(server->fileName);
			trans.GetMimeType(server->mimeType);
			trans.GetResponseHeader("Last-Modified", server->lastModified);
			trans.GetContentLength(server->size);
			if (server->size == 0) {
				ret = BGDL_ERROR_NO_SIZE;
				break;
			}
			if (folder == NULL) {
				ret = 0;
				break;
			}
			if (server->fileName.length() == 0) {
				ret = BGDL_ERROR_NO_FILE;
				break;
			}

			string target;
			common::SharedPtr<MallocBuffer> buf = MallocBuffer::Allocate(0x8000);

			if (buf.get() == NULL || buf->GetSize() == 0) {
				ret = BGDL_ERROR_NO_MEMORY;
				break;
			}
			target = folder;
			target += "/";
			if (name != NULL)
				target += name;
			else
				target.append(server->fileName);
			if (path != NULL)
				*path = target;

			common::SharedPtr<File> file = LocalFile::Open(target.c_str(),
								       SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC |
								       0x10000, 6, &ret);

			if (ret != 0)
				break;

			BgdlFileCloser closer(file);
			SceUInt64 done = 0;
			SceUInt32 size = 0;
			SceUInt64 total = server->size;
			BgdlPowerTicker ticker(1);

			Bgdl_81113890(1);
			if (total != 0) {
				for (;;) {
					ticker.doTick();
					ret = trans.ReceiveResponse((common::SharedPtr<Buffer> &)buf, &size);
					if (ret != 0) {
						host->isRequested();
						break;
					}
					if (size == 0)
						break;
					if (buf->GetSize() > size && done + size < total)
						break;
					if (file->Write(buf->GetAddr(), size) != (int)size)
						break;
					done += size;
					thread::Sleep(1);
					if (total <= done)
						break;
				}
			}
			ret = 0;
		}
	} while (0);
	server->status = ret;
	return ret;
}

/* (0x81120B9C) an install that failed because its media went, named by where it was mounted */
void BgdlGamePluginLoader::CheckMediaError(int *ret, const char *mountPoint, SceSize len)
{
	if (*ret != (int)0x80010013 && *ret != (int)0x8032001A)
		return;
	if (mountPoint == NULL || mountPoint[0] == '\0')
		return;
	if (sce_paf_strncmp(mountPoint, "gro0:", len) == 0)
		*ret = BGDL_ERROR_GAMECARD_REMOVED;
	else if (sce_paf_strncmp(mountPoint, "grw0:", len) == 0)
		*ret = BGDL_ERROR_GAMECARD_REMOVED;
	else if (sce_paf_strncmp(mountPoint, "ux0:", len) == 0)
		*ret = BGDL_ERROR_MEMCARD_REMOVED;
}

static void bgdlInstallProgress(void *arg, float percent);
static void bgdlInstallPartProgress(void *arg, float percent);

/* (0x81120C32) the package folder at path installed, its progress to the task's own function
   or into the task's description */
int BgdlGamePluginLoader::InstallFolder(const char *path, BgdlTaskInfo *info)
{
	int ret = -1;
	GamePluginInterface *game;
	GameInstallExParam param;
	void (*progressFunc)(void *arg, float percent);
	void *progressArg;

	if (path == NULL)
		ret = BGDL_ERROR_NULL_FOLDER;
	game = Load();
	if (game == NULL)
		return BGDL_ERROR_NO_GAME_PLUGIN;
	sce_paf_memset(&param, 0, sizeof(param));
	param.noOtherFlag = info->noOtherFlag;
	param.imported = info->imported;
	info->progress = 0;
	progressFunc = info->progressFunc;
	progressArg = info->progressArg;
	if (progressFunc == NULL && progressArg == NULL) {
		progressFunc = bgdlInstallProgress;
		progressArg = info;
	}
	param.progressFunc = progressFunc;
	param.progressArg = progressArg;
	sce_paf_snprintf(param.path, sizeof(param.path) - 1, "%s", path);
	/* the content id, 128 bytes from mountPoint on */
	if (info->contentId.length() != 0)
		sce_paf_snprintf(param.mountPoint, 0x7F, "%s", info->contentId.c_str());
	ret = ((int (*)(GameInstallExParam *))game->func[GAME_PLUGIN_FUNC_INSTALL_EX])(&param);
	if (ret != 0)
		CheckMediaError(&ret, param.errorTitleId, sizeof(param.errorTitleId));
	return ret;
}

/* (0x81120D06) the theme at path installed by game_plugin's job (its interface's 57th) */
int BgdlGamePluginLoader::InstallTheme(const char *path, BgdlTaskInfo *info)
{
	GamePluginInterface *game;
	GameJobParam param;
	void (*progressFunc)(void *arg, float percent);
	void *progressArg;

	if (path == NULL)
		return BGDL_ERROR_NULL_FOLDER;
	game = Load();
	if (game == NULL)
		return BGDL_ERROR_NO_GAME_PLUGIN;
	sce_paf_memset(&param, 0, sizeof(param));
	param.extracted = false;
	sce_paf_snprintf(param.path, sizeof(param.path) - 1, "%s", path);
	progressFunc = info->progressFunc;
	info->progress = 0;
	if (progressFunc != NULL || info->progressArg != NULL) {
		progressArg = info->progressArg;
	} else {
		progressFunc = bgdlInstallProgress;
		progressArg = info;
	}
	param.progressFunc = progressFunc;
	param.progressArg = progressArg;
	return ((int (*)(GameJobParam *))game->func[57])(&param);
}

/* (0x81120D9C) the title's package folder at path installed, onto the storage its
   livespace_db row says, its storage checked first for a task that asks it */
int BgdlGamePluginLoader::InstallTitle(const char *path, BgdlTaskInfo *info)
{
	int ret;
	GamePluginInterface *game;
	GameInstallExParam param;
	int storage;

	if (path == NULL)
		return BGDL_ERROR_NULL_FOLDER;
	if (info->url.length() == 0)
		return BGDL_ERROR_NO_TITLE_ID;
	ret = -1;
	game = Load();
	if (game == NULL)
		return BGDL_ERROR_NO_GAME_PLUGIN;
	sce_paf_memset(&param, 0, sizeof(param));
	param.storage = 0;

	LsdbAppInfoPtr app(new LsdbAppInfo());

	if (Bgdl_81123BC0(info->path.c_str(), &app) != 0) {
		ret = BGDL_ERROR_NO_APP;
		return ret;
	}
	ret = Bgdl_81113898(&app, &storage);
	if (ret != 0)
		return ret;
	if (info->kind == 0) {
		ret = Bgdl_81114128(&info->path, storage);
		if (ret != 0)
			return ret;
	}
	param.storage = storage;
	param.noOtherFlag = info->noOtherFlag;
	param.imported = info->imported;
	info->progress = 0;
	param.progressFunc = bgdlInstallProgress;
	param.progressArg = info;
	sce_paf_snprintf(param.path, sizeof(param.path) - 1, "%s", path);
	ret = ((int (*)(GameInstallExParam *))game->func[GAME_PLUGIN_FUNC_INSTALL_EX])(&param);
	if (ret != 0)
		CheckMediaError(&ret, param.errorTitleId, sizeof(param.errorTitleId));
	return ret;
}

/* (0x81121066) the package at path installed by game_plugin's first install (its
   interface's 12th), as an install of the kind given */
int BgdlGamePluginLoader::InstallLicense(const char *path, int kind, BgdlTaskInfo *info)
{
	GamePluginInterface *game;
	GameInstallParam param;

	if (path == NULL)
		return BGDL_ERROR_NULL_FOLDER;
	if (info->url.length() == 0)
		return BGDL_ERROR_NO_TITLE_ID;
	game = Load();
	if (game == NULL)
		return BGDL_ERROR_NO_GAME_PLUGIN;
	sce_paf_memset(&param, 0, sizeof(param));
	sce_paf_snprintf(param.path, sizeof(param.path) - 1, "%s", path);
	sce_paf_snprintf(param.titleId, sizeof(param.titleId) - 1, "%s", info->url.c_str());
	param.kind = kind;
	param.skipOtherFlag = info->noOtherFlag;
	param.skipTopmenuNotify = info->imported;
	info->progress = 0;
	param.progressFunc = bgdlInstallProgress;
	param.progressArg = info;
	param.noLicense = info->contentId.length() != 0;
	return ((int (*)(GameInstallParam *))game->func[12])(&param);
}

/* (0x8112112A) one part of the title's package folders installed, at path, its progress the
   part's share of the whole */
int BgdlGamePluginLoader::InstallPart(const char *path, BgdlTaskInfo *info, BgdlInstallPart *part)
{
	int ret = -1;
	GamePluginInterface *game;
	GameInstallExParam param;
	int storage;
	float progress;

	if (path == NULL)
		ret = BGDL_ERROR_NULL_FOLDER;
	game = Load();
	if (game == NULL)
		return BGDL_ERROR_NO_GAME_PLUGIN;
	sce_paf_memset(&param, 0, sizeof(param));
	param.storage = 0;

	LsdbAppInfoPtr app(new LsdbAppInfo());

	if (Bgdl_81123BC0(info->path.c_str(), &app) != 0) {
		ret = BGDL_ERROR_NO_APP;
		return ret;
	}
	ret = Bgdl_81113898(&app, &storage);
	if (ret != 0)
		return ret;
	if (info->kind == 0) {
		ret = Bgdl_81114220(&info->path, storage);
		if (ret != 0) {
			ret = BGDL_ERROR_GAMECARD_REMOVED;
			return ret;
		}
	}
	param.noOtherFlag = info->noOtherFlag;
	param.imported = info->imported;
	progress = 0;
	if (part->count > 1)
		progress = 1.0f / part->count * part->index * 100.0f;
	info->progress = progress;
	param.progressFunc = bgdlInstallPartProgress;
	param.progressArg = part;
	sce_paf_snprintf(param.path, sizeof(param.path) - 1, "%s", path);
	ret = ((int (*)(GameInstallExParam *))game->func[GAME_PLUGIN_FUNC_INSTALL_EX])(&param);
	if (ret != 0)
		CheckMediaError(&ret, param.errorTitleId, sizeof(param.errorTitleId));
	return ret;
}


/* (0x8112142C) the task's package folder installed by game_plugin, the way key 107 says */
int BgdlHttpDownload::InstallTask(EseTask *task, BgdlTaskInfo *info)
{
	SceUInt32 kind = task->m_ParamCache.key107;
	BgdlTaskPath folder;
	int ret;

	ret = task->GetFolder(&folder, true);
	if (ret != 0)
		return ret;

	string path(folder.m_begin);

	path += "/";

	BgdlTaskPackage package;

	s_packageMutex.Lock();
	package = task->package;
	s_packageMutex.Unlock();
	if (package.key220.length() == 0)
		return BGDL_ERROR_NO_TITLE_ID;
	path += package.key220.c_str();
	if (Bgdl_81113984(path.c_str()) != true)
		return BGDL_ERROR_BAD_FOLDER;
	if (Dir::IsExist(path.c_str()) != true)
		return BGDL_ERROR_NO_FOLDER;

	BgdlGamePluginLoader loader(this);

	s_gamePluginLoader = &loader;
	switch (kind) {
	case 6:
		ret = loader.InstallFolder(path.c_str(), info);
		break;
	case 7:
		ret = loader.InstallTitle(path.c_str(), info);
		break;
	case 8: {
		int count = 0;

		if (Bgdl_8112260C(task->id, 500, &count, sizeof(count)) != 0) {
			ret = BGDL_ERROR_NO_PART_COUNT;
			break;
		}

		int index = 0;

		ret = Bgdl_8112260C(task->id, 502, &index, sizeof(index));
		if (ret != 0)
			break;
		if (index < 0)
			index = 0;

		char name[16] = { 0 };
		int part;
		string partPath;

		for (part = index; part < count; part++) {
			sce_paf_snprintf(name, sizeof(name) - 1, "%04d", part);
			partPath = path;
			partPath += "/";
			partPath += name;
			Bgdl_811233A4(task->id, 502, &part, sizeof(part));

			BgdlInstallPart install;

			install.count = count;
			install.index = part;
			install.info = info;
			ret = loader.InstallPart(partPath.c_str(), info, &install);
			if (ret != 0)
				break;
		}
		break;
	}
	case 9: {
		string contentId;

		ret = Bgdl_8112278A(task->id, 230, &contentId);
		if (ret != 0)
			break;
		ret = loader.InstallLicense(contentId.c_str(), 2, info);
		if (ret == 0) {
			char titleId[16];

			sce_paf_memset(titleId, 0, sizeof(titleId));
			sce_paf_strncpy(titleId, info->url.c_str(), sizeof(titleId) - 1);
			ret = vshAppMgrUpdateRifInfo(titleId);
			if (ret == 0 || ret == (int)0x80800002)
				ret = 0;
		}
		break;
	}
	case 12:
		ret = loader.InstallTheme(path.c_str(), info);
		break;
	default:
		ret = BGDL_ERROR_BAD_KIND;
		break;
	}
	s_gamePluginLoader = NULL;
	return ret;
}

/* (0x811219B8) */
int Bgdl_811219B8(EseTask *task, BgdlTaskInfo *info)
{
	int status = 0;
	BgdlHttpDownload download(task, &status);

	return download.InstallTask(task, info);
}

/* (0x811219EE) an install's progress kept in the task's description */
static void bgdlInstallProgress(void *arg, float percent)
{
	BgdlTaskInfo *info = (BgdlTaskInfo *)arg;

	if (info != NULL)
		info->progress = percent;
}

/* (0x811219F6) a part's progress kept in the task's description as its share of the whole */
static void bgdlInstallPartProgress(void *arg, float percent)
{
	BgdlInstallPart *part = (BgdlInstallPart *)arg;

	if (part != NULL) {
		if (part->count <= 1) {
			part->info->progress = percent;
		} else {
			float share = 1.0f / part->count;

			part->info->progress = share * percent + share * part->index * 100.0f;
		}
	}
}

/* (0x81121A3C) game_plugin's interface taken when the plugin has loaded */
static void bgdlGamePluginLoaded(Plugin *plugin)
{
	BgdlGamePluginLoader *loader = s_gamePluginLoader;

	if (loader != NULL) {
		loader->m_plugin = Plugin::Find("game_plugin");
		if (loader->m_plugin != NULL)
			loader->m_interface = (GamePluginInterface *)loader->m_plugin->GetInterface(1);
		loader->m_mutex.Lock();
		loader->m_loaded = true;
		loader->m_mutex.Unlock();
	}
}
