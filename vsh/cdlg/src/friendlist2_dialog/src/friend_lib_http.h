/*
 * SceFriendList2DialogPlugin: SceFriendLibHttp and SceFriendLibImage
 * (HTTP/HTTPS connection-pooled downloader and PAF Surface image decoder).
 */
#ifndef FRIENDLIST2_DIALOG_FRIEND_LIB_HTTP_H
#define FRIENDLIST2_DIALOG_FRIEND_LIB_HTTP_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/list>
#include <std/string>
#include <common/shared_ptr.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <job/job.h>
#include <buffer/buffer.h>
#include <memory/heap_allocator.h>
#include <url/url.h>

class FriendLibHttpConnectionPool;

/* Callback interface for HTTP download completion */
class FriendLibHttpCallback {
public:
	virtual ~FriendLibHttpCallback() {}
	virtual void OnComplete(int result, int httpStatus,
				paf::common::SharedPtr<paf::Buffer> responseBody) = 0;
};

/* Callback interface for image decode completion */
class FriendLibImageCallback {
public:
	virtual ~FriendLibImageCallback() {}
	virtual void OnComplete(int result,
				paf::intrusive_ptr<paf::graph::Surface> surface) = 0;
};

/* Image decode resize/pool options */
struct FriendLibImageOption {
	int32_t maxWidth;
	int32_t maxHeight;
	paf::graph::SurfacePool *surfacePool;
	FriendLibImageCallback *decodeHook;
	paf::common::SharedPtr<paf::Buffer> sourceBuffer;
};

/* Global SceFriendLibHttp template lifecycle */
int FriendLibHttpInit(void);
void FriendLibHttpTerm(void);

/* Connection-pooled HTTP/HTTPS job item ("SceFriendLibHttp") */
class FriendLibHttpJob : public paf::job::JobItem {
public:
	FriendLibHttpJob(FriendLibHttpConnectionPool *pool, const paf::string &urlStr,
			 int responseMode, uint32_t optionFlags, FriendLibHttpCallback *callback);
	virtual ~FriendLibHttpJob();
	virtual int Run();
	virtual void Finish(int result);
	virtual void Cancel();

private:
	int ReadFixedLength(uint32_t contentLength);
	int ReadChunked();

	FriendLibHttpConnectionPool *m_pool;
	paf::Url m_url;
	int m_responseMode;
	uint32_t m_optionFlags;
	FriendLibHttpCallback *m_callback;
	int m_requestId;
	paf::common::SharedPtr<paf::Buffer> m_responseBody;
	int m_httpStatus;
};

/* Image download & decode manager ("SceFriendLibImage") */
class FriendLibImageLoader {
public:
	explicit FriendLibImageLoader(paf::job::JobQueue *jobQueue);
	virtual ~FriendLibImageLoader();

	int LoadAsync(const char *urlOrPath, FriendLibImageCallback *callback,
		      const FriendLibImageOption &option);
	void CancelAll();

	paf::job::JobQueue *m_jobQueue;
	FriendLibHttpConnectionPool *m_httpPool;
	uint32_t m_maxHttpConnections;
	uint32_t m_maxHttpsConnections;
};

/* Allocate a PAF Buffer from the FriendLib heap allocator */
paf::common::SharedPtr<paf::Buffer> FriendLibAllocBuffer(size_t size);

#endif /* FRIENDLIST2_DIALOG_FRIEND_LIB_HTTP_H */
