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
#include <libhttp.h>

#ifdef __cplusplus
extern "C" {
#endif

struct NpLookupClient;
struct NpLookupServer;
struct NpLookupTitle;
struct NpLookupTrans;
struct NpLookupJobHandler;
struct NpLookupWorker;
struct NpLookupProfile;
struct NpLookupXmlParser;
struct NpXmlSchema;
struct NpLookupProfileParse;
struct NpLookupProfilesParse;
struct NpLookupJidParse;
struct NpLookupCensorParse;
struct NpLookupServersParse;

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

/* request.c: the requests and their XML responses */
int SceNpUtility_23B387A5(SceInt32 reqId, const SceNpOnlineId *onlineId, SceNpId *npId,
			  void *option);
int SceNpUtility_9D8E0730(SceInt32 reqId, SceInt32 avatarSizeType, const SceNpId *npId,
			  SceNpUserInformation *userInfo, SceNpAboutMe *aboutMe,
			  SceNpMyLanguages *languages, SceNpCountryCode *countryCode,
			  void *avatarImageData, SceSize avatarImageDataMaxSize,
			  SceSize *avatarImageDataSize, int avatarN, void *option);
int SceNpUtility_B2FD5E9D(SceInt32 reqId, const SceNpAvatarUrl *avatarUrl,
			  SceNpAvatarImage *avatarImage, void *option);
int SceNpUtility_60CC5AFB(SceInt32 request, SceHttpAuthType authType, const char *realm,
			  char *username, char *password, SceBool needEntity,
			  SceUChar8 **entityBody, SceSize *entitySize, SceBool *save, void *userArg);
int SceNpUtility_01C44B07(int withUrl, struct NpLookupTrans *trans, const char *server,
			  const char *scheme, SceUShort16 port);
int SceNpUtility_A08E60BA(int withUrl, struct NpLookupTrans *trans, int method, const char *path,
			  SceULong64 contentLength);
int SceNpUtility_C4FE3A57(struct NpLookupTrans *trans, SceUInt32 type, const char *name,
			  SceSize contentLength);
int SceNpUtility_34B42669(struct NpLookupTrans *trans, const void *data, SceSize size);
int SceNpUtility_866D737A(struct NpLookupTrans *trans);
int SceNpUtility_37AD71B9(void);
int SceNpUtility_8857763D(struct NpLookupTrans *trans, int operation, SceUInt64 *rest,
			  SceSize *size, int unused, SceUInt8 *signature, int whole);
int SceNpUtility_89C8F74E(struct NpLookupTrans *trans, int operation, SceUInt64 *rest,
			  SceSize *size, SceUInt8 *signature, int whole);
int SceNpUtility_548625FC(struct NpLookupTitle *title, int operation, const void *data,
			  int titleId);
int SceNpUtility_4A479D1D(int titleId);
int SceNpUtility_23414285(struct NpLookupTitle *title, int operation, const void *data,
			  int titleId, SceUInt32 service);
int SceNpUtility_B4423DAF(int titleId, SceUInt32 service);
int SceNpUtility_C63A0F6B(int reqId);
int SceNpUtility_B418204E(int reqId, SceNpId *npId);
int SceNpUtility_7BC7E7F7(int reqId, const SceNpId *npId, SceInt32 avatarSizeType,
			  int panelSizeType, struct NpLookupProfile *profile);
int SceNpUtility_08893FA1(int reqId, const SceNpAvatarUrl *url, SceSize *size, SceSize maxSize,
			  void *data, void *option);
int SceNpUtility_C9283605(int reqId, const SceNpId *npId, const char *comment,
			  char *sanitizedComment, int sanitize);
int SceNpUtility_1392F18F(struct NpLookupTrans *trans, struct NpLookupProfile *profile,
			  SceInt32 *ptlp, SceInt32 *update, SceRtcTick *date, SceSize size);
int SceNpUtility_9EEDD7F8(struct NpLookupTrans *trans, struct NpLookupProfile *profiles,
			  SceInt32 *results, SceUInt32 count, SceUInt32 requested, SceSize size);
int SceNpUtility_C2598A46(int reqId, const SceNpId *npIds, SceUInt32 count,
			  SceInt32 avatarSizeType, SceInt32 panelSizeType,
			  struct NpLookupProfile *profiles, SceInt32 *results);
int SceNpUtility_E64B4101(int reqId, const char *url, SceSize *outSize, SceSize maxSize,
			  void *data, void *option);
int SceNpUtility_4B30CAEF(struct NpLookupTrans *trans, const char *comment, char *sanitized,
			  int sanitize, int *passed, SceSize size);
int SceNpUtility_8951DA6B(char *buf, SceSize bufSize, const SceNpId *npId,
			  SceUInt32 avatarSizeType, SceUInt32 panelSizeType, int service);
int SceNpUtility_DFFF438A(char **body, const SceNpId *npIds, SceUInt32 count,
			  const SceInt32 *results, SceSize *answered, SceUInt32 avatarSizeType,
			  SceUInt32 panelSizeType, int service);
int SceNpUtility_83445FED(char *buf, SceSize bufSize, const char *onlineId);
int SceNpUtility_60D70B4A(char *buf, SceSize bufSize, const SceNpCommunicationId *commId,
			  const SceNpId *npId, const char *comment, char *sanitizedComment,
			  int sanitize, int service);

/* xml.c: the parser of the servers' XML and its handlers */
int SceNpUtility_31423835(struct NpLookupXmlParser *parser, const struct NpXmlSchema *schema,
			  int (*handler)(struct NpLookupXmlParser *, int, SceUInt32, SceUInt32,
					 const char *, const char *, void *),
			  void *arg);
int SceNpUtility_C01F7C6A(struct NpLookupXmlParser *parser, const char *buf, int size);
int SceNpUtility_E2135903(struct NpLookupXmlParser *parser, const char *buf, int size);
int SceNpUtility_E9544993(int event, SceUInt32 depth, SceUInt32 element, const char *name,
			  const char *value);
int SceNpUtility_C02139E5(int event, const char *name, const char *value, char *url, int *id,
			  SceUInt32 *flags, int *count);
int SceNpUtility_15227842(struct NpLookupXmlParser *parser, int type, int reserved,
			  struct NpLookupServersParse *state);
int SceNpUtility_AB499EA4(struct NpLookupServersParse *state);
int SceNpUtility_2580CDAF(struct NpLookupXmlParser *parser, struct NpLookupJidParse *state);
int SceNpUtility_5D75C414(struct NpLookupJidParse *state);
int SceNpUtility_839F805E(struct NpLookupXmlParser *parser, int mode, struct NpLookupProfileParse *state);
int SceNpUtility_F32ED701(struct NpLookupProfileParse *state);
int SceNpUtility_04AA56CB(struct NpLookupXmlParser *parser, struct NpLookupProfilesParse *state);
int SceNpUtility_A3767D45(struct NpLookupProfilesParse *state);
int SceNpUtility_67147C73(struct NpLookupXmlParser *parser, struct NpLookupCensorParse *state);
int SceNpUtility_A9D5542A(struct NpLookupCensorParse *state);

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

/* util.c: numbers as text, the online ID check, the response and the conversions */
void SceNpUtility_4D52A224(char *str, int value);
void SceNpUtility_2E4D8BCF(char *str, SceUInt64 value);
void SceNpUtility_FF9F1571(char *str, SceInt64 value);
SceInt64 SceNpUtility_BDFC2D58(const char *str);
SceBool SceNpUtility_475B35A8(const char *onlineId);
int SceNpUtility_A3E828ED(struct NpLookupTrans *trans, void *buf, SceSize size, SceSize *read);
int SceNpUtility_4FAC4566(struct NpLookupTrans *trans, int *statusCode, SceUInt64 *contentLength,
			  int *hasContentLength, SceUInt32 *infoLength, SceUInt8 *signature,
			  int operation);
void SceNpUtility_72FC55A7(int reqId, SceUInt32 infoLength);
int SceNpUtility_54C48F92(SceNpId *npId, const char *str, int service);
int SceNpUtility_14437509(char **str, const SceNpId *npId, int service);
int SceNpUtility_B8DFA4D1(char **dst, const char *src, int service);
int SceNpUtility_15D37FB1(char **dst, const char *src, int service);
int SceNpUtility_89484A9C(void *arg, int value);
int SceNpUtility_E916C86A(struct NpLookupServer *server, int service);
int SceNpUtility_3A1BA624(char **dst, const void *src, SceSize size, int service);
void SceNpUtility_450C80FB(char *version);

/* worker.c: the worker thread of the asynchronous requests */
int SceNpUtility_6EB4C8FE(struct NpLookupWorker *worker, int priority, SceSize stackSize,
			  int cpuAffinityMask, SceUInt32 queueSize, void *(*alloc)(SceSize size),
			  void (*free)(void *ptr));
int SceNpUtility_5631E596(struct NpLookupWorker *worker);
int SceNpUtility_575BBBC4(struct NpLookupWorker *worker, int count,
			  const struct NpLookupJobHandler *handlers);
int SceNpUtility_F73C4411(struct NpLookupWorker *worker);
int SceNpUtility_44FE1097(struct NpLookupWorker *worker, int type, int arg, void *job, int flags);

/* bandwidth_test.c: the bandwidth test's upload or download alone */
int SceNpUtility_D46E45DE(SceInt32 initPriority, SceInt32 cpuAffinityMask);
int SceNpUtility_B7A52202(SceInt32 initPriority, SceInt32 cpuAffinityMask);

#ifdef __cplusplus
}
#endif

#endif
