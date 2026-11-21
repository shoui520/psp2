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

#include "../include/esebase.hpp"
#include "../include/esetask.hpp"
#include "../include/eseparam.hpp"
#include "../include/eseutil.hpp"
#include "../include/esehttp.hpp"
#include "src/livespace_db/livespace_db_internal.h"

using namespace paf;

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
	m_reserved0.clear();
	m_reserved1.clear();
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
		ret = Bgdl_812218A2(dst);
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
	url.clear();
	titleId.clear();
	contentId.clear();
	message.clear();
	value0 = 0;
	value1 = 0;
	value2 = 0;
	value3 = 0;
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
