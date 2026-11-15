/*
 * SceNpUtility's exports (vs0:sys/external/np_utility.suprx) other than the SDK's word filter,
 * lookup and bandwidth test: the lookup library under them. The common dialogs reach it
 * through the community helper (../np_community/np_community.h). Sony exports them by NID
 * only and no record names them; what each does is read from the code.
 */
#ifndef NP_UTILITY_H
#define NP_UTILITY_H

#include <scetypes.h>
#include <np/np_common.h>
#include <np/np_lookup.h>

#ifdef __cplusplus
extern "C" {
#endif

struct NpLookupClient;
struct NpLookupTitle;
struct NpLookupTrans;
struct NpLookupJobHandler;

/* lookup.c: sceNpLookupUserProfile and its asynchronous form with the N avatar sizes */
int SceNpUtilityAvatarN_4A989C1C(SceInt32 reqId, SceInt32 avatarSizeType, const SceNpId *npId,
				 SceNpUserInformation *userInfo, SceNpAboutMe *aboutMe,
				 SceNpMyLanguages *languages, SceNpCountryCode *countryCode,
				 void *avatarImageData, SceSize avatarImageDataMaxSize,
				 SceSize *avatarImageDataSize, void *option);
int SceNpUtilityAvatarN_DC2B7B72(SceInt32 reqId, SceInt32 avatarSizeType, const SceNpId *npId,
				 SceNpUserInformation *userInfo, SceNpAboutMe *aboutMe,
				 SceNpMyLanguages *languages, SceNpCountryCode *countryCode,
				 void *avatarImageData, SceSize avatarImageDataMaxSize,
				 SceSize *avatarImageDataSize, void *option);

/* client.c: a service's clients, one per NP ID */
int SceNpUtility_17385829(int service);
int SceNpUtility_7A86C37E(int service);
int SceNpUtility_F4B21646(const SceNpOnlineId *onlineId, const SceNpId *npId, int service);
void SceNpUtility_75FD803B(struct NpLookupClient *client);
int SceNpUtility_ED2B86ED(int clientId, int service);
int SceNpUtility_F32ED20E(int clientId, struct NpLookupClient **client, int service);
int SceNpUtility_A15150C9(struct NpLookupClient *client, void *ptr);
void SceNpUtility_CD3A3712(struct NpLookupClient *client);
int SceNpUtility_944FE83C(int reqId);
int SceNpUtility_A0991531(int id, int resolveRetry, SceUInt32 resolveTimeout,
			  SceUInt32 connectTimeout, SceUInt32 sendTimeout, SceUInt32 recvTimeout);

/* the requests */
int SceNpUtility_23B387A5(SceInt32 reqId, const SceNpOnlineId *onlineId, SceNpId *npId,
			  void *option);
int SceNpUtility_9D8E0730(SceInt32 reqId, SceInt32 avatarSizeType, const SceNpId *npId,
			  SceNpUserInformation *userInfo, SceNpAboutMe *aboutMe,
			  SceNpMyLanguages *languages, SceNpCountryCode *countryCode,
			  void *avatarImageData, SceSize avatarImageDataMaxSize,
			  SceSize *avatarImageDataSize, int avatarN, void *option);
int SceNpUtility_B2FD5E9D(SceInt32 reqId, const SceNpAvatarUrl *avatarUrl,
			  SceNpAvatarImage *avatarImage, void *option);
int SceNpUtility_23414285(struct NpLookupTitle *title, int arg, const SceNpId *npId,
			  int titleId, int service);
int SceNpUtility_B4423DAF(int reqId, int service);
void SceNpUtility_866D737A(struct NpLookupTrans *trans);
int SceNpUtility_C63A0F6B(int reqId);
int SceNpUtility_C9283605(int reqId, const SceNpId *npId, const char *comment,
			  char *sanitizedComment, int sanitize);

/* memory.c: the services' heaps */
void *SceNpUtility_8CA24D19(SceSize size, int service);
void SceNpUtility_6C1161AD(void *ptr, int service);
int SceNpUtility_74B0D0AC(int service);
void SceNpUtility_B5711AF9(int service, SceSize size, void *memblock);
void SceNpUtility_4A3FDD3B(int service, void *(*alloc)(SceSize size), void (*free)(void *ptr));
void SceNpUtility_5FC4B7D5(int service);
void SceNpUtility_418E1324(int service);

/* title.c: a service's title contexts */
int SceNpUtility_E38548AB(SceUInt32 service);
void SceNpUtility_485F368A(struct NpLookupTitle *title, SceUInt32 service);
int SceNpUtility_B9FFB253(int titleId, struct NpLookupTitle **title, SceUInt32 service);
int SceNpUtility_4013244C(int titleId, SceUInt32 service);
int SceNpUtility_26B4AC00(SceUInt32 service);
int SceNpUtility_44A2F444(const SceNpCommunicationId *commId, const SceNpId *npId, int clientId,
			  SceUInt32 service);
int SceNpUtility_D03605CD(int titleId, struct NpLookupTitle **title);

/* trans.c: a service's transactions */
int SceNpUtility_9F142A92(int service);
int SceNpUtility_200BBAC5(int service);
void SceNpUtility_31D3542E(struct NpLookupTrans *trans);
int SceNpUtility_25D26B6F(int reqId, struct NpLookupTrans **trans);
int SceNpUtility_D5BB8464(int reqId, SceUInt32 service);
int SceNpUtility_6DF884DA(int service);
int SceNpUtility_46543307(struct NpLookupTrans *trans, void *ptr);
int SceNpUtility_A48B44C9(struct NpLookupClient *client, int operation, SceUInt32 service,
			  void **data, int titleId);
int SceNpUtility_F5086697(int reqId, int result);
int SceNpUtility_8904FE15(int reqId, int *result);
int SceNpUtility_16954FFF(void);
void SceNpUtility_5D6FC99E(int state);
SceBool SceNpUtility_2835BC9B(void);
void SceNpUtility_191D314C(void);

/* worker.c: the worker thread of the asynchronous requests */
int SceNpUtility_6EB4C8FE(void *worker, int priority, SceSize stackSize, int cpuAffinityMask,
			  int queueSize, void *(*alloc)(SceSize size), void (*free)(void *ptr));
int SceNpUtility_5631E596(void *worker);
int SceNpUtility_575BBBC4(void *worker, int count, const struct NpLookupJobHandler *handlers);
int SceNpUtility_F73C4411(void *worker);
int SceNpUtility_44FE1097(void *worker, int type, int arg, void *job, int flags);

#ifdef __cplusplus
}
#endif

#endif
