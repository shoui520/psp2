/*
 * SceNpUtility: the bandwidth test. A thread of its own uploads to post.net.playstation.net
 * and downloads from get.net.playstation.net, timing each transfer; sceNpBandwidthTestShutdown
 * waits for it and hands back the rates. Besides the SDK's full test, two exports run the
 * upload or the download alone and report a rate from what was moved even when the transfer
 * fails part way.
 */

#include "np_utility_internal.h"

/* the test modes, one thread entry each */
#define BANDWIDTH_TEST_FULL		0
#define BANDWIDTH_TEST_UPLOAD		1
#define BANDWIDTH_TEST_DOWNLOAD		2

/* the test's state, its thread's argument */
typedef struct NpBandwidthTest {
	int status;				/* 0x00: SCE_NP_BANDWIDTH_TEST_STATUS_* */
	SceUID thread;				/* 0x04 */
	int httpTemplate;			/* 0x08 */
	int connection;				/* 0x0C */
	int request;				/* 0x10 */
	SceUInt8 reserved[4];			/* 0x14 */
	SceNpBandwidthTestResult result;	/* 0x18 */
} NpBandwidthTest;

static int s_initialized;
static char s_buffer[1024];
static NpBandwidthTest s_test;

static __attribute__((noinline)) int startTest(int mode, SceInt32 initPriority,
					       SceInt32 cpuAffinityMask);
static __attribute__((noinline)) int testThread(SceSize argSize, void *argBlock);
static __attribute__((noinline)) int uploadTestThread(SceSize argSize, void *argBlock);
static __attribute__((noinline)) int downloadTestThread(SceSize argSize, void *argBlock);

static int startTest(int mode, SceInt32 initPriority, SceInt32 cpuAffinityMask)
{
	SceHttpMemoryPoolStats stats;
	SceKernelThreadEntry entry;
	NpBandwidthTest *arg;
	int ret;

	if (s_initialized == 1)
		return SCE_NP_BANDWIDTH_TEST_ERROR_ALREADY_INITIALIZED;
	arg = &s_test;
	ret = sceHttpGetMemoryPoolStats(&stats);
	if (ret < 0)
		return ret;
	if (stats.poolSize - stats.currentInuseSize < SCE_NP_BANDWIDTH_TEST_LEAST_HTTP_POOL_SIZE)
		return SCE_NP_BANDWIDTH_TEST_ERROR_HTTP_POOL_TOO_SHORT;

	sceClibMemset(&s_test, 0, sizeof(s_test));
	s_test.status = SCE_NP_BANDWIDTH_TEST_STATUS_NONE;
	switch (mode) {
	case BANDWIDTH_TEST_FULL:
		entry = testThread;
		break;
	case BANDWIDTH_TEST_UPLOAD:
		entry = uploadTestThread;
		break;
	case BANDWIDTH_TEST_DOWNLOAD:
		entry = downloadTestThread;
		break;
	default:
		return SCE_NP_ERROR_INVALID_ARGUMENT;
	}
	ret = sceKernelCreateThread("SceNpBandwidthTest", entry, initPriority,
				    SCE_NP_BANDWIDTH_TEST_THREAD_STACK_SIZE, 0, cpuAffinityMask, NULL);
	if (ret < 0)
		return ret;
	s_test.thread = ret;
	ret = sceKernelStartThread(ret, sizeof(arg), &arg);
	if (ret < 0)
		return ret;
	s_initialized = 1;
	return 0;
}

int sceNpBandwidthTestInitStart(SceInt32 initPriority, SceInt32 cpuAffinityMask)
{
	return startTest(BANDWIDTH_TEST_FULL, initPriority, cpuAffinityMask);
}

/* the upload test alone */
int SceNpUtility_D46E45DE(SceInt32 initPriority, SceInt32 cpuAffinityMask)
{
	return startTest(BANDWIDTH_TEST_UPLOAD, initPriority, cpuAffinityMask);
}

/* the download test alone */
int SceNpUtility_B7A52202(SceInt32 initPriority, SceInt32 cpuAffinityMask)
{
	return startTest(BANDWIDTH_TEST_DOWNLOAD, initPriority, cpuAffinityMask);
}

int sceNpBandwidthTestGetStatus(void)
{
	if (s_initialized == 0)
		return SCE_NP_BANDWIDTH_TEST_ERROR_NOT_INITIALIZED;
	return s_test.status;
}

int sceNpBandwidthTestShutdown(SceNpBandwidthTestResult *result)
{
	if (s_initialized == 0)
		return 0;
	if (result == NULL)
		return SCE_NP_BANDWIDTH_TEST_ERROR_INVALID_ARGUMENT;
	if (s_test.thread >= 0) {
		sceKernelWaitThreadEnd(s_test.thread, NULL, NULL);
		sceKernelDeleteThread(s_test.thread);
		s_test.thread = -1;
	}
	sceClibMemcpy_safe(result, &s_test.result, sizeof(*result));
	s_initialized = 0;
	return 0;
}

int sceNpBandwidthTestAbort(void)
{
	if (s_initialized == 0)
		return SCE_NP_BANDWIDTH_TEST_ERROR_NOT_INITIALIZED;
	sceHttpAbortRequest(s_test.request);
	return 0;
}

/* deletes the test's request, connection and template */
static inline void closeHttp(NpBandwidthTest *test)
{
	if (test->request != 0) {
		sceHttpDeleteRequest(test->request);
		test->request = 0;
	}
	if (test->connection != 0) {
		sceHttpDeleteConnection(test->connection);
		test->connection = 0;
	}
	if (test->httpTemplate != 0) {
		sceHttpDeleteTemplate(test->httpTemplate);
		test->httpTemplate = 0;
	}
}

/* posts size bytes to server's path in 1 KiB pieces; *sent gets what was sent */
static int upload(NpBandwidthTest *test, const char *server, const char *path, SceSize size,
		  SceSize *sent)
{
	SceSize left = size;
	int ret;

	*sent = 0;
	ret = sceHttpCreateTemplate("btest/1.0", SCE_HTTP_VERSION_1_1, SCE_TRUE);
	if (ret >= 0) {
		test->httpTemplate = ret;
		ret = sceHttpCreateConnection(ret, server, "http", 80, SCE_FALSE);
		if (ret >= 0) {
			test->connection = ret;
			ret = sceHttpCreateRequest(ret, SCE_HTTP_METHOD_POST, path, size);
			if (ret >= 0) {
				test->request = ret;
				do {
					ret = sceHttpSendRequest(test->request, s_buffer,
								 sizeof(s_buffer));
					if (ret < 0)
						break;
					left -= sizeof(s_buffer);
				} while (left != 0);
			}
		}
	}
	closeHttp(test);
	*sent = size - left;
	return ret;
}

/* gets server's path, which must be size bytes; *received gets what was read */
static int download(NpBandwidthTest *test, const char *server, const char *path, SceSize size,
		    SceSize *received)
{
	SceUInt64 contentLength = 0;
	SceSize left = size;
	int statusCode;
	int ret;

	*received = 0;
	ret = sceHttpCreateTemplate("btest/1.0", SCE_HTTP_VERSION_1_1, SCE_TRUE);
	if (ret >= 0) {
		test->httpTemplate = ret;
		ret = sceHttpCreateConnection(ret, server, "http", 80, SCE_FALSE);
		if (ret >= 0) {
			test->connection = ret;
			ret = sceHttpCreateRequest(ret, SCE_HTTP_METHOD_GET, path, 0);
			if (ret >= 0) {
				test->request = ret;
				ret = sceHttpSendRequest(test->request, NULL, 0);
				if (ret >= 0) {
					ret = sceHttpGetStatusCode(test->request, &statusCode);
					if (ret >= 0) {
						if (statusCode != 200) {
							ret = SCE_NP_BANDWIDTH_TEST_ERROR_BAD_RESPONSE;
						} else {
							sceHttpGetResponseContentLength(test->request,
											&contentLength);
							if (contentLength != 0 &&
							    contentLength != size) {
								ret = SCE_NP_BANDWIDTH_TEST_ERROR_BAD_RESPONSE;
							} else {
								do {
									ret = sceHttpReadData(test->request,
											      s_buffer,
											      sizeof(s_buffer));
									if (ret < 0)
										break;
									if (ret == 0) {
										if (left != 0)
											ret = SCE_NP_BANDWIDTH_TEST_ERROR_BAD_RESPONSE;
										break;
									}
									left -= ret;
								} while (left != 0);
							}
						}
					}
				}
			}
		}
	}
	closeHttp(test);
	*received = size - left;
	return ret;
}

/* bits per second of bytes moved in time microseconds */
#define BPS(bytes, time)	((double)((bytes) * 8) / ((double)(time) / 1000000.0))

static int testThread(SceSize argSize, void *argBlock)
{
	NpBandwidthTest *test = *(NpBandwidthTest **)argBlock;
	SceUInt64 start;
	SceUInt64 end;
	SceSize sent;
	SceSize received;
	int ret;

	test->result.result = 0;
	test->status = SCE_NP_BANDWIDTH_TEST_STATUS_RUNNING;
	ret = SceNpCommon_55F7FCF7(&start);
	if (ret < 0) {
		test->result.result = ret;
		goto end;
	}
	ret = upload(test, "post.net.playstation.net", "/networktest/post_128?p=1", 128 * 1024,
		     &sent);
	if (ret < 0) {
		test->result.result = ret;
		goto end;
	}
	ret = SceNpCommon_55F7FCF7(&end);
	if (ret < 0) {
		test->result.result = ret;
		goto end;
	}
	start = end - start;
	test->result.uploadBps = BPS(128 * 1024, start);

	/* under 0.1 s: again with 1 MiB */
	if (start < 100000) {
		ret = SceNpCommon_55F7FCF7(&start);
		if (ret < 0) {
			test->result.result = ret;
			goto end;
		}
		ret = upload(test, "post.net.playstation.net", "/networktest/post?p=1",
			     1024 * 1024, &sent);
		if (ret < 0) {
			test->result.result = ret;
			goto end;
		}
		ret = SceNpCommon_55F7FCF7(&end);
		if (ret < 0) {
			test->result.result = ret;
			goto end;
		}
		start = end - start;
		test->result.uploadBps = BPS(1024 * 1024, start);
	}

	ret = SceNpCommon_55F7FCF7(&start);
	if (ret < 0) {
		test->result.result = ret;
	} else {
		ret = download(test, "get.net.playstation.net", "/networktest/get_2m?p=1",
			       2 * 1024 * 1024, &received);
		if (ret < 0) {
			test->result.result = ret;
		} else {
			ret = SceNpCommon_55F7FCF7(&end);
			if (ret < 0) {
				test->result.result = ret;
			} else {
				start = end - start;
				test->result.downloadBps = BPS(2 * 1024 * 1024, start);
			}
		}
	}

end:
	closeHttp(test);
	test->status = SCE_NP_BANDWIDTH_TEST_STATUS_FINISHED;
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}

/* a rate from what moved when the transfer ended, took 10 s or moved half its size */
static int uploadTestThread(SceSize argSize, void *argBlock)
{
	NpBandwidthTest *test = *(NpBandwidthTest **)argBlock;
	SceUInt64 start;
	SceUInt64 end;
	SceSize sent;
	int result;
	int ret;

	test->result.result = 0;
	test->status = SCE_NP_BANDWIDTH_TEST_STATUS_RUNNING;
	ret = SceNpCommon_55F7FCF7(&start);
	if (ret < 0) {
		test->result.result = ret;
		goto end;
	}
	result = upload(test, "post.net.playstation.net", "/networktest/post_128?p=1",
			128 * 1024, &sent);
	ret = SceNpCommon_55F7FCF7(&end);
	if (ret < 0) {
		test->result.result = ret;
		goto end;
	}
	start = end - start;
	if (result >= 0 || start >= 10000000 || sent * 2 >= 128 * 1024)
		test->result.uploadBps = BPS(sent, start);

	if (start < 100000) {
		if (result < 0) {
			test->result.result = result;
			test->result.uploadBps = 0;
			goto end;
		}
		ret = SceNpCommon_55F7FCF7(&start);
		if (ret < 0) {
			test->result.result = ret;
			goto end;
		}
		result = upload(test, "post.net.playstation.net", "/networktest/post?p=1",
				1024 * 1024, &sent);
		ret = SceNpCommon_55F7FCF7(&end);
		if (ret < 0) {
			test->result.result = ret;
			goto end;
		}
		start = end - start;
		if (result >= 0 || start >= 10000000 || sent * 2 >= 1024 * 1024)
			test->result.uploadBps = BPS(sent, start);
	}
	if (result < 0)
		test->result.result = result;

end:
	closeHttp(test);
	test->status = SCE_NP_BANDWIDTH_TEST_STATUS_FINISHED;
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}

static int downloadTestThread(SceSize argSize, void *argBlock)
{
	NpBandwidthTest *test = *(NpBandwidthTest **)argBlock;
	SceUInt64 start;
	SceUInt64 end;
	SceSize received;
	int result;
	int ret;

	test->result.result = 0;
	test->status = SCE_NP_BANDWIDTH_TEST_STATUS_RUNNING;
	ret = SceNpCommon_55F7FCF7(&start);
	if (ret < 0) {
		test->result.result = ret;
	} else {
		result = download(test, "get.net.playstation.net", "/networktest/get_2m?p=1",
				  2 * 1024 * 1024, &received);
		ret = SceNpCommon_55F7FCF7(&end);
		if (ret < 0) {
			test->result.result = ret;
		} else {
			start = end - start;
			if (result >= 0 || start >= 10000000 || received * 2 >= 2 * 1024 * 1024)
				test->result.downloadBps = BPS(received, start);
			if (result < 0)
				test->result.result = result;
		}
	}

	closeHttp(test);
	test->status = SCE_NP_BANDWIDTH_TEST_STATUS_FINISHED;
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}
