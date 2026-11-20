/*
 * The utility component: the activity server's addresses, the NP environment, base64, NP ID and
 * time conversions, and the host and trophy icon URLs cached for the whole library (Sony's file:
 * component_utility.cpp's checks; the class and method names are ours).
 */
#ifndef NP_ACTIVITY_COMPONENT_UTILITY_COMPONENT_UTILITY_H
#define NP_ACTIVITY_COMPONENT_UTILITY_COMPONENT_UTILITY_H

#include <scetypes.h>
#include <np/np_common.h>
#include <rtc.h>
#include "../abstract_component.h"

/* the requests the utility gives addresses for (GetPath's switch) */
#define UTILITY_API_NONE				0
#define UTILITY_API_STATIC_RESOURCE			1
#define UTILITY_API_GET_ACCOUNT_SETTING			2
#define UTILITY_API_SET_ACCOUNT_SETTING			3
#define UTILITY_API_CHECK_STATUS			4
#define UTILITY_API_GET_TIMELINE			5
#define UTILITY_API_GET_TIMELINE_BY_TITLE		6
#define UTILITY_API_GET_ACCOUNT_STATUS			7
#define UTILITY_API_GET_ACCOUNT_STATUS_BY_TITLE		8
#define UTILITY_API_GET_STATUS_INFO			9
#define UTILITY_API_GET_STATUS_DETAIL			10
#define UTILITY_API_SET_MULTIPLE_STATUS			11
#define UTILITY_API_DELETE_STATUS			12
#define UTILITY_API_SET_COMMENT				13
#define UTILITY_API_DELETE_COMMENT			14
#define UTILITY_API_SET_VOTE				15
#define UTILITY_API_DELETE_VOTE				16

/* GetProtocol's results */
#define UTILITY_PROTOCOL_HTTP		0
#define UTILITY_PROTOCOL_HTTPS		1
#define UTILITY_PROTOCOL_NONE		2	/* the HTTP component's, while closed */

/* the cached host's lifetime: 10 minutes, in microseconds */
#define UTILITY_HOST_LIFETIME		600000000ULL

#define UTILITY_HOST_SIZE		0x100
#define UTILITY_URL_SIZE		0x101
#define UTILITY_NP_ENVIRONMENT_SIZE	17

class ComponentUtility : public Component {
public:
	ComponentUtility();
	virtual ~ComponentUtility();

	virtual int IsHostExpired();
	virtual int IsValidStaticResource(const char *data, SceSize size, const char *digest,
		SceSize digestSize);
	virtual int SetHost(const char *host, SceSize length);
	virtual int SetTrophyIconUrls(const char *base, const char *bronze, const char *silver,
		const char *gold, const char *hidden, const char *platinum);
	virtual int GetProtocol(int api);
	virtual int GetSecureProtocol(int api);
	virtual const char *GetUserAgent(int api);
	virtual const char *GetScheme(int api);
	virtual const char *GetHost(int api);
	virtual const char *GetPath(int api);
	virtual const char *GetNpEnvironment();
	virtual SceUShort16 GetPort(int protocol);
	virtual const char *GetTrophyIconBaseUrl()
	{
		return s_trophyIconBaseUrl;
	}
	virtual const char *GetTrophyIconBronze()
	{
		return s_trophyIconBronze;
	}
	virtual const char *GetTrophyIconSilver()
	{
		return s_trophyIconSilver;
	}
	virtual const char *GetTrophyIconGold()
	{
		return s_trophyIconGold;
	}
	virtual const char *GetTrophyIconHidden()
	{
		return s_trophyIconHidden;
	}
	virtual const char *GetTrophyIconPlatinum()
	{
		return s_trophyIconPlatinum;
	}
	virtual const char *GetXmlDeclaration();
	virtual const char *GetPlatform();
	virtual const char *GetNpVersion();
	virtual SceSize GetBase64EncodedSize(SceSize size);
	virtual int Base64Encode(const void *src, SceSize size, char *dst, SceSize dstSize);
	virtual SceSize GetBase64DecodedSize(SceSize size);
	virtual int Base64Decode(const char *src, SceSize size, void *dst, SceSize dstSize);
	virtual int ConvertNpIdToJid(const SceNpId *npId, char *jid, SceSize size);
	virtual int ConvertJidToNpId(const char *jid, SceNpId *npId);
	virtual int ConvertTimeTextToSceRtcTick(const char *text, SceRtcTick *tick);
	virtual int ConvertSceRtcTickToTimeText(const SceRtcTick *tick, char *text, SceSize size);
	virtual int GetCurrentNetworkTime(SceRtcTick *tick);

	/* the cached host and trophy icon URLs: empty until set */
	static __attribute__((noinline)) void ClearCache();
	/* 1: the requests go to the function server's paths (/activity/func/...); never in this
	   build */
	static int IsFunctionServer();

	char m_host[UTILITY_HOST_SIZE];			/* GetHost's static resource host */
	int m_npVersionValid;
	char m_npVersion[8];				/* SceNpCommon_0624AE44's text */
	char m_npEnvironment[UTILITY_NP_ENVIRONMENT_SIZE];

	static char s_host[UTILITY_HOST_SIZE];
	static int s_hostValid;
	static SceUInt64 s_hostTime;
	static char s_trophyIconBaseUrl[UTILITY_URL_SIZE];
	static char s_trophyIconBronze[UTILITY_URL_SIZE];
	static char s_trophyIconSilver[UTILITY_URL_SIZE];
	static char s_trophyIconGold[UTILITY_URL_SIZE];
	static char s_trophyIconPlatinum[UTILITY_URL_SIZE];
	static char s_trophyIconHidden[UTILITY_URL_SIZE];
};

#endif
