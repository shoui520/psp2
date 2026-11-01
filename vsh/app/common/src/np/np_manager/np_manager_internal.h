#ifndef NP_MANAGER_INTERNAL_H
#define NP_MANAGER_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <kernel/sysmem/memblock.h>
extern "C" {
#include <regmgr/regmgr.h>
}
#include <np.h>
#include <np/np_auth.h>
#include <np/np_auth_oauth.h>
#include <np/np_manager.h>
#include <np/np_error.h>
#include "../../../../../lowlayer/src/ipmi/client.h"
#include "../../../../../lowlayer/src/ipmi_wrapper/ipmi_wrapper_client.h"
#include "../../../../../lowlayer/src/blocking_ipc/blocking_ipc_client.h"

/* Error codes the SDK headers do not name (names unknown). */
#define NP_MANAGER_ERROR_REQUEST_FAILED		-2141911679	/* 0x80550581 */
#define NP_MANAGER_ERROR_REQUEST_NOT_FOUND	-2141911678	/* 0x80550582 */
#define NP_MANAGER_ERROR_REQUEST_BUSY		-2141911677	/* 0x80550583 */
#define NP_IMPS_UTILITY_ERROR_ALREADY_INITIALIZED	-2141906173	/* 0x80551B03 */
#define NP_IMPS_UTILITY_ERROR_NOT_INITIALIZED	-2141906172	/* 0x80551B04 */
#define NP_IMPS_UTILITY_ERROR_OUT_OF_MEMORY	-2141906167	/* 0x80551B09 */
#define NP_SERVER_ERROR_ABORTED			-2141873146	/* 0x80559C06 */
#define NP_SERVER_ERROR_SIGNED_OUT		-2141873145	/* 0x80559C07 */

/* the ShellSvc events of the NP service */
#define NP_MANAGER_EVENT_REQUEST		51

/* the NP settings the shell's NP service keeps (SceNpCommon's NpCommonSettings) */
typedef struct NpCommonSettings {
	int fullTest;			/* 0x00 */
	char serverName[40];		/* 0x04 */
	int test;			/* 0x2C */
	int param749261;		/* 0x30 */
	int param858341;		/* 0x34 */
	int param1401993;		/* 0x38 */
} NpCommonSettings;

/* SceNpManagerOAuth_C62F86A6's parameter: an authorization code for a confidential client */
typedef struct NpAuthGetAuthorizationCodeExParameter {
	SceSize size;
	const SceNpClientId *pClientId;
	const SceChar8 *pClientSecret;
	const SceChar8 *pScope;
} NpAuthGetAuthorizationCodeExParameter;

/* the size of a ticket the NP service hands out */
#define NP_MANAGER_TICKET_SIZE			1536

/* a ticket request's parameter: the request and the size of the caller's buffer */
typedef struct NpManagerTicketParam {
	int reqId;
	SceSize bufferSize;
} NpManagerTicketParam;

/* SceNpManager_2FE3625C's request: which of the two results the caller wants */
typedef struct NpManagerIpcParam_2FE3625C {
	int reqId;
	int wantData1;
	int wantData2;
} NpManagerIpcParam_2FE3625C;

/* SceNpManager_0F6AC47E's request */
typedef struct NpManagerIpcParam_0F6AC47E {
	int reqId;
	int value1;
	int value2;
	int value3;
} NpManagerIpcParam_0F6AC47E;

/* SceNpManager_FF0D6501's request */
typedef struct NpManagerIpcParam_FF0D6501 {
	int reqId;
	int value;
} NpManagerIpcParam_FF0D6501;

/* SceNpManager_2BB7D844's request: its values and which data the caller left out */
typedef struct NpManagerIpcParam_2BB7D844 {
	int reqId;
	int data1Absent;
	int data2Absent;
	int data3Absent;
	int value1Absent;
	int data4Absent;
	int value2;
	int value3Absent;
	int data5Absent;
	int value4;
	SceSize data6Size;
} NpManagerIpcParam_2BB7D844;

/* a list request: the request, two values and how many entries the caller has room for */
typedef struct NpManagerIpcListParam {
	int reqId;
	int value1;
	int value2;
	SceUInt32 count;
} NpManagerIpcListParam;

/* SceNpManager_C7F6ADC4's request */
typedef struct NpManagerIpcParam_C7F6ADC4 {
	int reqId;
	SceSize size;
	int value1;
	int value2;
	int value3;
	int value4;
} NpManagerIpcParam_C7F6ADC4;

/* SceNpManager_BBCD1B6D's request */
typedef struct NpManagerIpcParam_BBCD1B6D {
	int reqId;
	SceSize size;
	int value1;
	int value2;
} NpManagerIpcParam_BBCD1B6D;

/* SceNpManager_557D5CC1's request */
typedef struct NpManagerIpcParam_557D5CC1 {
	int reqId;
	int value1;
	int value2;
	int value3;
	int value4;
	int value5;
	int value6;
} NpManagerIpcParam_557D5CC1;

/* SceNpManager_D891AB9A's request */
typedef struct NpManagerIpcParam_D891AB9A {
	int reqId;
	int value1;
	int value2;
	int value3;
	int value4;
} NpManagerIpcParam_D891AB9A;

/* SceNpManager_455E05C1's request: a value and which data the caller left out */
typedef struct NpManagerIpcParam_455E05C1 {
	SceUInt64 value;
	int data1Absent;
	int data2Absent;
} NpManagerIpcParam_455E05C1;

/* which of a request's optional data the caller gave */
typedef struct NpManagerIpcFlags3 {
	int hasData1;
	int hasData2;
	int hasData3;
} NpManagerIpcFlags3;

typedef struct NpManagerIpcFlags4 {
	int hasData1;
	int hasData2;
	int hasData3;
	int hasData4;
} NpManagerIpcFlags4;

/* a callback of the NP service's event (SceNpManager_351A74C2) */
typedef void (*NpManagerCallback)(int event, void *arg);

/*
 * The functions a caller of SceNpManager_77DE17D4 answers in place of the NP service, each
 * named after the export it stands in for; the process id is the caller's.
 */
typedef struct NpManagerOverride {
	SceUInt32 reserved[2];
	int (*func_375A8BC0)(int *pValue, int value, SceUID pid);
	int (*func_F35CC917)(int *pValue, int value, SceUID pid);
	int (*func_E41932D4)(int *pValue, int value, SceUID pid);
	int (*func_D1D191FB)(int value);
	int (*func_EB16514C)(int value);
	int (*func_96534006)(int value, int *pValue);
} NpManagerOverride;

/* an allocator the IMPS utility uses */
typedef struct NpAllocator {
	void *(*alloc)(SceSize size);
	void (*free)(void *ptr);
} NpAllocator;

/* the account information the NP service keeps (sceNpManagerGetCachedParam's source) */
typedef struct NpManagerAccountInfo {
	SceNpId npId;			/* 0x000 */
	SceUInt8 reserved1[20];		/* 0x024 */
	SceNpAvatarUrl avatarUrl;	/* 0x038 */
	SceUInt8 reserved2[144];	/* 0x0B8 */
} NpManagerAccountInfo;

extern "C" {

/* the SceIpmi library's IpmiWrapper::Client and BlockingIpc::Client methods (libshellsvc) */
SceSize SceIpmi_006EFA9D(const IpmiWrapper::Client::Config *pConfig);	/* IpmiWrapper::Client::getBufferSize */
int SceIpmi_22625598(IpmiWrapper::Client **ppClient, const IpmiWrapper::Client::Config *pConfig,
		     void *pMemory);				/* IpmiWrapper::Client::create */
int SceIpmi_B56BAC7F(IpmiWrapper::Client *pClient);		/* IpmiWrapper::Client::connect */
int SceIpmi_B943F5D9(IpmiWrapper::Client *pClient);		/* IpmiWrapper::Client::disconnect */
int SceIpmi_D73C03D2(IpmiWrapper::Client *pClient);		/* IpmiWrapper::Client::destroy */
IPMI::Client *SceIpmi_9EE9496B(IpmiWrapper::Client *pClient);	/* IpmiWrapper::Client::getClient */
SceSize SceIpmi_296D44D4(const BlockingIpc::Client::Config *pConfig);	/* BlockingIpc::Client::getBufferSize */
int SceIpmi_BC3A3031(BlockingIpc::Client **ppClient, const BlockingIpc::Client::Config *pConfig,
		     void *pMemory);				/* BlockingIpc::Client::create */
int SceIpmi_2C6DB642(BlockingIpc::Client *pClient);		/* BlockingIpc::Client::connect */
int SceIpmi_3554D4B0(BlockingIpc::Client *pClient);		/* BlockingIpc::Client::disconnect */
int SceIpmi_A20BB95B(BlockingIpc::Client *pClient);		/* BlockingIpc::Client::destroy */
IPMI::Client *SceIpmi_1372C7B7(BlockingIpc::Client *pClient);	/* BlockingIpc::Client::getClient */
int SceIpmi_367CEF82(BlockingIpc::Client *pClient, int *pRequestId, const void *pParam,
		     SceSize paramSize);			/* BlockingIpc::Client::createRequest */
int SceIpmi_5ECD40E9(BlockingIpc::Client *pClient,
				int requestId);	/* BlockingIpc::Client::destroyRequest */
int SceIpmi_48E072C8(BlockingIpc::Client *pClient,
				int requestId);	/* BlockingIpc::Client::abortRequest */
int SceIpmi_7C002FEC(BlockingIpc::Client *pClient, int requestId,
		     BlockingIpc::Client::Handle *pHandle);	/* BlockingIpc::Client::beginBlockingIpc */
int SceIpmi_7E079E19(BlockingIpc::Client *pClient, const BlockingIpc::Client::Handle *pHandle,
		     SceUInt32 *pTimeout);			/* BlockingIpc::Client::waitBlockingIpc */
int SceIpmi_8A0BAC4E(BlockingIpc::Client *pClient,
				int requestId);	/* BlockingIpc::Client::endBlockingIpc */

/* the shell's service client and events (libshellsvc) */
IPMI::Client *SceShellSvc_B31E7F1C(void);
int SceShellSvc_D943CE15(int event, int (*callback)(int event, void *pArg), void *pArg);
int SceShellSvc_7CBF442B(int event);
int SceShellSvc_FB03FBC9(int event);

/* SceNpCommon's imports */
int SceNpCommon_652FC5E0(const char *environment);
int SceNpCommon_DA04BA55(const void *settings);
int SceNpCommon_A385C8CD(SceSize size, SceUInt32 attr, int a2, int a3);
int SceNpCommon_E2743C07(void);
int SceNpCommon_BA6310A1(void);

/* np_manager_ipc_client.cpp */
int SceNpManager_4110B787(SceNpAuthOAuthRequestId *reqId);
int SceNpManager_3907F96A(SceNpAuthOAuthRequestId reqId);
int SceNpManager_737B2820(SceNpAuthOAuthRequestId reqId);
int SceNpManager_E45E4003(NpManagerAccountInfo *info);
int SceNpManager_2EA038D8(void *ticket, SceSize *ticketSize);
int npAuthGetAuthorizationCode(SceNpAuthOAuthRequestId reqId,
			       const SceNpAuthGetAuthorizationCodeParameter *param,
			       SceNpAuthorizationCode *authCode, int *issuerId);
int npAuthGetAuthorizationCodeEx(SceNpAuthOAuthRequestId reqId, const SceNpClientId *clientId,
				 const SceChar8 *clientSecret, const SceChar8 *scope,
				 SceNpAuthorizationCode *authCode, int *issuerId);

/* np_manager.cpp */
int SceNpManager_A6964218(void);
void *npManagerAlloc(SceSize size);
void npManagerFree(void *ptr);

/* np_imps_utility_for_np_infra.cpp */
int sceNpImpsUtilityForNpInfraInit(const NpAllocator *allocator);
int sceNpImpsUtilityForNpInfraTerm(void);
SceBool sceNpImpsUtilityForNpInfraIsInitialized(void);
int sceNpImpsUtilityForNpInfraRegisterServiceStateCallback(SceNpServiceStateCallback callback,
							    void *userdata);
int sceNpImpsUtilityForNpInfraUnregisterServiceStateCallback(void);
int sceNpImpsUtilityForNpInfraCheckCallback(void);
int sceNpImpsUtilityForNpInfraGetServiceState(SceNpServiceState *state);
int sceNpImpsUtilityForNpInfraRequestServiceState(void);
int sceNpImpsUtilityForNpInfraGetServiceStateEx(int *state);

/* np_manager_ipc_client.cpp */
int npManagerIpcInit(void);
int npManagerIpcTerm(void);

}

/* np_manager_ipc_stub.cpp */
int npManagerIpcStart_877251F3(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId);
int npManagerIpcFinish_877251F3(IPMI::Client *client, int requestId, void *buf);
int npManagerIpcStart_2841C20F(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId);
int npManagerIpcFinish_2841C20F(IPMI::Client *client, int requestId, void *buf);
int npManagerIpcStart_80895A1E(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       SceSize bufferSize);
int npManagerIpcFinish_80895A1E(IPMI::Client *client, int requestId, SceSize *pTicketSize,
				SceSize *pSize, void *ticket);
int npManagerIpcStart_E2FAC2CA(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       SceSize bufferSize);
int npManagerIpcFinish_E2FAC2CA(IPMI::Client *client, int requestId, SceSize *pTicketSize,
				SceSize *pSize, void *ticket);
int npManagerIpcStart_5EB63590(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       SceSize bufferSize);
int npManagerIpcFinish_5EB63590(IPMI::Client *client, int requestId, SceSize *pTicketSize,
				SceSize *pSize, void *ticket);
int npManagerIpcStart_2FE3625C(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId, int wantData1,
			       int wantData2);
int npManagerIpcFinish_2FE3625C(IPMI::Client *client, int requestId, void *buf1, void *buf2);
int npManagerIpcStart_63EF4853(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const void *data1, const void *data2);
int npManagerIpcFinish_63EF4853(IPMI::Client *client, int requestId);
int npManagerIpcStart_0F6AC47E(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId, int value1,
			       int value2, int value3);
int npManagerIpcFinish_0F6AC47E(IPMI::Client *client, int requestId, int *value);
int npManagerIpc_sceNpManagerGetNpId(IPMI::Client *client, SceNpId *npId);
int npManagerIpc_1796CD2E(IPMI::Client *client, int value);
int npManagerIpc_A775A452(IPMI::Client *client, int value);
int npManagerIpc_455E05C1(IPMI::Client *client, SceUInt64 value, const void *data1,
			  int data1Absent, const void *data2, int data2Absent);
int npManagerIpc_D83C8CCB(IPMI::Client *client, int value);
int npManagerIpc_E45E4003(IPMI::Client *client, NpManagerAccountInfo *info);
int npManagerIpc_D4180D28(IPMI::Client *client);
int npManagerIpc_2EA038D8(IPMI::Client *client, void *ticket, SceSize size, SceSize *pSize);
int npManagerIpc_4341692A(IPMI::Client *client, int *value);
int npManagerIpc_C80D8C3E(IPMI::Client *client, int value, void *ticket, SceSize *pSize);
int npManagerIpc_6CDB2AEC(IPMI::Client *client, int hasData1, const void *data1, int hasData2,
			  const void *data2, int hasData3, const void *data3);
int npManagerIpc_ED12AF16(IPMI::Client *client, int hasData1, const void *data1, int hasData2,
			  const void *data2, int hasData3, const void *data3, int hasData4,
			  const void *data4);
int npManagerIpc_DDF30B0E(IPMI::Client *client, int hasData1, void *data1, int hasData2,
			  void *data2, int hasData3, void *data3);
int npManagerIpc_4E94274C(IPMI::Client *client);
int npManagerIpc_sceNpManagerGetAccountRegion(IPMI::Client *client, SceNpCountryCode *countryCode,
			  int *languageCode);
int npManagerIpc_97291707(IPMI::Client *client, void *data1, void *data2);
int npManagerIpc_sceNpManagerGetContentRatingFlag(IPMI::Client *client, int *isRestricted,
			  int *age);
int npManagerIpc_sceNpManagerGetChatRestrictionFlag(IPMI::Client *client, int *isRestricted);
int npManagerIpc_BB384A4D(IPMI::Client *client, int value);
int npManagerIpc_351A74C2(IPMI::Client *client, int value);
int npManagerIpc_8C6EACC4(IPMI::Client *client, int value);
int npManagerIpc_FFBFC540(IPMI::Client *client, void *buf);
int npManagerIpc_375A8BC0(IPMI::Client *client, int *pValue, int value);
int npManagerIpc_F35CC917(IPMI::Client *client, int *pValue, int value);
int npManagerIpc_E41932D4(IPMI::Client *client, int *pValue, int value);
int npManagerIpc_D1D191FB(IPMI::Client *client, int value);
int npManagerIpc_EB16514C(IPMI::Client *client, int value);
int npManagerIpc_96534006(IPMI::Client *client, int value, int *pValue);
int npManagerIpcStart_FF0D6501(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const void *data, int value);
int npManagerIpcFinish_FF0D6501(IPMI::Client *client, int requestId, void *buf);
int npManagerIpcStart_2BB7D844(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const void *data1,
			       int data1Absent, const void *data2, int data2Absent,
			       const void *data3,
			       int data3Absent, const int *pValue1, int value1Absent,
			       const void *data4,
			       int data4Absent, int value2, const int *pValue3, int value3Absent,
			       const void *data5, int data5Absent, int value4, const void *data6,
			       SceSize data6Size);
int npManagerIpcFinish_2BB7D844(IPMI::Client *client, int requestId, void *buf1, int *value1,
				int *value2, void *buf2);
int npManagerIpcStart_A5F0BD9C(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId, int value1,
			       int value2, SceUInt32 count);
int npManagerIpcFinish_A5F0BD9C(IPMI::Client *client, int requestId, SceUInt32 *pCount,
				void *entries, int *pValue);
int npManagerIpcStart_D6C54CC2(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const int *data, int value1, int value2, SceUInt32 count);
int npManagerIpcFinish_D6C54CC2(IPMI::Client *client, int requestId, SceUInt32 *pCount, int *values,
				void *entries, int *pValue);
int npManagerIpcStart_89E693FB(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId, int value1,
			       int value2, SceUInt32 count);
int npManagerIpcFinish_89E693FB(IPMI::Client *client, int requestId, SceUInt32 *pCount,
				void *entries, int *pValue);
int npManagerIpcStart_28DED5D1(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const int *data, int value1, int value2, SceUInt32 count);
int npManagerIpcFinish_28DED5D1(IPMI::Client *client, int requestId, SceUInt32 *pCount,
				int *values1, int *values2, void *entries, int *pValue);
int npManagerIpcStart_6D13F55F(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId, int value1,
			       int value2, SceUInt32 count);
int npManagerIpcFinish_6D13F55F(IPMI::Client *client, int requestId, SceUInt32 *pCount,
				void *entries, int *pValue);
int npManagerIpcStart_C7F6ADC4(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const void *data,
			       SceSize size, const int *pValue, int value1, int value2, int value3,
			       int value4);
int npManagerIpcFinish_C7F6ADC4(IPMI::Client *client, int requestId, SceUInt32 *pCount, int *values,
				void *entries, SceUInt32 *pCount2,
				int *values2, int *pValue1, int *pValue2);
int npManagerIpcStart_BBCD1B6D(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const void *data, SceSize size, int value1, int value2);
int npManagerIpcFinish_BBCD1B6D(IPMI::Client *client, int requestId, void *buf);
int npManagerIpcStart_557D5CC1(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId, int value1,
			       int value2, const int *pValue, int value3, int value4, int value5,
			       int value6);
int npManagerIpcFinish_557D5CC1(IPMI::Client *client, int requestId, SceUInt32 *pCount, int *values,
				void *entries, SceUInt32 *pCount2,
				int *values2, int *pValue1, int *pValue2);
int npManagerIpcStart_D891AB9A(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId, int value1,
			       int value2, int value3, int value4);
int npManagerIpcFinish_D891AB9A(IPMI::Client *client, int requestId, void *buf);
int npManagerIpcStart_FDE78C74(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId);
int npManagerIpcFinish_FDE78C74(IPMI::Client *client, int requestId, void *buf);
int npManagerIpcStart_npAuthGetAuthorizationCode(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const SceNpClientId *pClientId, const SceChar8 *pScope);
int npManagerIpcFinish_npAuthGetAuthorizationCode(IPMI::Client *client, int requestId,
				SceNpAuthorizationCode *authCode, int *issuerId);
int npManagerIpcStart_npAuthGetAuthorizationCodeEx(IPMI::Client *client, int *pRequestId,
			       const BlockingIpc::Client::Handle *pHandle, int reqId,
			       const SceNpClientId *pClientId, const SceChar8 *pClientSecret,
			       const SceChar8 *pScope);
int npManagerIpcFinish_npAuthGetAuthorizationCodeEx(IPMI::Client *client, int requestId,
				SceNpAuthorizationCode *authCode, int *issuerId);
int npManagerIpc_F66A7AAE(IPMI::Client *client);
int npManagerIpc_1F0C6314(IPMI::Client *client, void *buf, SceUInt8 *value);

#endif /* NP_MANAGER_INTERNAL_H */
