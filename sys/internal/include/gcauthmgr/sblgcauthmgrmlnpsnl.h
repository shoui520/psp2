/*
 * SceSblGcAuthMgrMlnpsnl: the PC activation's challenge and key. The records are gcauthmgr's
 * (gcauthmgr/pcact.h); the callers pass them through.
 */
#pragma once

struct GcAuthPcactActivationBlock;
struct GcAuthPcactChallenge;
struct GcAuthPcactRandomRecord;

int sceSblGcAuthMgrMlnpsnlAuth1ForDriver(const unsigned char *entropy,
		const unsigned char *rtcSeconds, struct GcAuthPcactChallenge *output,
		struct GcAuthPcactRandomRecord *random);
int sceSblGcAuthMgrMlnpsnlAuth2ForDriver(const struct GcAuthPcactActivationBlock *activation,
		const struct GcAuthPcactRandomRecord *random, unsigned char *outputKey);
