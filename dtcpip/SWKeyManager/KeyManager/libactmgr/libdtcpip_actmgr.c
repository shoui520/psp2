/*
 * swagner's activation key-ID manager (SWKeyManager/KeyManager/libactmgr/libdtcpip_actmgr.c,
 * from its assertion string): selects the active DTCP-IP key ID for the current secure RTC
 * timestamp (plus the key-store clock offset) from the two-entry key schedule.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <scetypes.h>
#include <rtc.h>

#define DTCPIP_ACTMGR_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\SWKeyManager\\KeyManager\\libactmgr\\libdtcpip_actmgr.c"
#define DTCPIP_ACTMGR_NPARAM	2
#define DTCPIP_ACTMGR_VALID_SEC	31470526ULL	/* 0x1E033BE */
#define SCE_RTC_ERROR_INVALID_TICK	(-2145054718)

struct DtcpIpActMgrParam {
	unsigned short keyId;
	unsigned short reservedPad[3];
	SceUInt64 genTime;
};

/* libc assertion hook and key-store / secure-RTC helpers */
void _Assert(const char *message, const char *func);
int vshRtcGetCurrentSecureTick(SceRtcTick *tick);
int swKeyStoreLoadVtrm(void);
int swKeyStoreGetTimeOffset(long long *outOffset);

static struct DtcpIpActMgrParam g_actMgrParams[DTCPIP_ACTMGR_NPARAM] = {
	{ 0x1007, { 0, 0, 0 }, 0x5771ba80ULL },
	{ 0x1020, { 0, 0, 0 }, 0x5951ee3eULL },
};

/* select the active key ID for the current secure clock */
int swKeyStoreGetKeyId(unsigned short *keyId)
{
	int ret;
	int i;
	SceRtcTick tick;
	SceUInt64 currentTime;
	long long offset;
	SceDateTime dateTime;

	if (keyId == NULL)
		_Assert(DTCPIP_ACTMGR_SOURCE "(36) : Assertion Failed (key_id != 0)",
			"dtcpip_actmgr_get_key_id");

	fprintf(stderr, "[dtcpip_actmgr_get_key_id] nparam: %d\n", DTCPIP_ACTMGR_NPARAM);
	ret = vshRtcGetCurrentSecureTick(&tick);
	if (ret < 0) {
		if (ret == SCE_RTC_ERROR_INVALID_TICK)
			return -3;
		return -2;
	}
	if (sceRtcSetTick(&dateTime, &tick) < 0)
		return -2;
	if (sceRtcGetTime64_t(&dateTime, &currentTime) < 0)
		return -2;

	swKeyStoreLoadVtrm();
	if (swKeyStoreGetTimeOffset(&offset) == 0)
		currentTime = (SceUInt64)((long long)currentTime + offset);

	fprintf(stderr, "[dtcpip_actmgr_get_key_id] current time: %lld %s\n",
		(long long)currentTime, ctime((const time_t *)&currentTime));

	if (currentTime < g_actMgrParams[0].genTime) {
		fprintf(stderr, "[dtcpip_actmgr_get_key_id] wait for:%lld sec\n",
			(long long)(g_actMgrParams[0].genTime - currentTime));
		return -2;
	}

	for (i = 0; i < DTCPIP_ACTMGR_NPARAM; ++i) {
		if (g_actMgrParams[i].genTime <= currentTime &&
		    (unsigned int)(currentTime - g_actMgrParams[i].genTime) <
			    DTCPIP_ACTMGR_VALID_SEC) {
			SceUInt64 remain = DTCPIP_ACTMGR_VALID_SEC -
					   (currentTime - g_actMgrParams[i].genTime);

			fprintf(stderr, "[dtcpip_actmgr_get_key_id] key_id:0x%x remain:%lld sec\n",
				g_actMgrParams[i].keyId, (long long)remain);
			*keyId = g_actMgrParams[i].keyId;
			return 0;
		}
	}
	return -1;
}

/* replace the two-entry activation key schedule */
int dtcpip_actmgr_update_info(const void *params, int count)
{
	int i;

	if (count != DTCPIP_ACTMGR_NPARAM)
		return -1;
	memcpy(g_actMgrParams, params, sizeof(g_actMgrParams));
	for (i = 0; i < DTCPIP_ACTMGR_NPARAM; ++i)
		fprintf(stderr, "[dtcpip_actmgr_update_info] key_id:0x%x gen_time:%ld\n",
			g_actMgrParams[i].keyId, (long long)g_actMgrParams[i].genTime);
	return DTCPIP_ACTMGR_NPARAM;
}
