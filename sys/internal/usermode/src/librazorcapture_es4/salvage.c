/*
 * SceRazorCapture: GPU lockup salvage monitor thread and job info query.
 */

#include "razor_capture_internal.h"

static SceUID s_salvageThreadId;
static SceUID s_salvageEventFlagId;
static void *s_salvageTopContext;
static void (*s_salvageCallback)(void);

typedef struct RazorSalvageJobInfo {
	uint32_t words0[2];
	uint16_t halfs[2];
	uint32_t words1[16];
} RazorSalvageJobInfo;

int razorSalvageThreadEntry(SceSize args, void *argp)
{
	int pid;
	int res;
	unsigned int outBits;

	(void)args;
	(void)argp;

	pid = sceKernelGetProcessId();
	res = sceGpuRegisterSalvage();
	if (res != 0) {
		sceClibPrintf("Unable to start salvage mode: 0x%x\n", res);
	} else {
		while (1) {
			res = sceGpuSignalWaitLockup();
			if (res == 0) {
				s_salvageCallback();
				break;
			}
			if (res != (int)0x80028005)
				break;
			if (sceKernelPollEventFlag(s_salvageEventFlagId, 1, 1, &outBits) == 0)
				break;
		}
		res = sceGpuUnregisterSalvage(pid);
		if (res != 0)
			sceClibPrintf("Unable to end salvage mode thread: 0x%x\n", res);
	}
	return 0;
}

int razorSalvageQueryJobInfo(uint32_t jobId, void *outInfo)
{
	RazorSalvageJobInfo tmp;
	void *topCtx;
	int res;

	sceClibMemset(&tmp, 0, sizeof(tmp));
	topCtx = sceGxmGetTopContextInternal();
	res = SceGpuEs4User_CCEE0336(topCtx, jobId, &tmp);
	if (res == 0) {
		RazorSalvageJobInfo *dst = (RazorSalvageJobInfo *)outInfo;
		dst->words0[0] = tmp.words0[0];
		dst->words0[1] = tmp.words0[1];
		dst->halfs[0] = tmp.halfs[0];
		dst->halfs[1] = tmp.halfs[1];
		dst->words1[0] = tmp.words1[0];
		dst->words1[1] = tmp.words1[1];
		dst->words1[2] = tmp.words1[2];
		dst->words1[3] = tmp.words1[3];
		dst->words1[4] = tmp.words1[4];
		dst->words1[5] = tmp.words1[5];
		dst->words1[6] = tmp.words1[6];
		dst->words1[7] = tmp.words1[7];
		dst->words1[8] = tmp.words1[8];
		dst->words1[9] = tmp.words1[9];
		dst->words1[10] = tmp.words1[10];
		dst->words1[11] = tmp.words1[11];
		dst->words1[12] = tmp.words1[12];
		dst->words1[13] = tmp.words1[13];
		dst->words1[14] = tmp.words1[14];
		dst->words1[15] = tmp.words1[15];
	}
	return res;
}

int razorSalvageStartThread(void (*callback)(void))
{
	SceUID tid;
	int res;

	s_salvageCallback = callback;
	s_salvageThreadId = 0;
	s_salvageTopContext = sceGxmGetTopContextInternal();
	if (s_salvageTopContext == NULL)
		return -1;

	s_salvageEventFlagId = sceKernelCreateEventFlag("sceRazorCapture", 0x1000, 0, NULL);
	tid = sceKernelCreateThread("sceRazorCapture", razorSalvageThreadEntry,
				    0x10000100, 0x2000, 0, 0, NULL);
	s_salvageThreadId = tid;
	if (tid < 0)
		return -1;

	res = sceKernelStartThread(tid, 0, NULL);
	if (res != 0)
		return -1;

	return res;
}

int razorSalvageStopThread(void)
{
	sceKernelSetEventFlag(s_salvageEventFlagId, 1);
	sceKernelWaitThreadEnd(s_salvageThreadId, NULL, NULL);
	sceKernelDeleteThread(s_salvageThreadId);
	sceKernelDeleteEventFlag(s_salvageEventFlagId);
	s_salvageThreadId = 0;
	s_salvageEventFlagId = 0;
	return 0;
}
