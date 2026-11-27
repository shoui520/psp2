/*
 * SceSigninExt (vs0:sys/external/signin_ext.suprx):
 * SceNpCommerce2 lifecycle and async JobTask subclasses for country list,
 * session login, EULA acceptance, DOB verification, password reset/change,
 * and session logout.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <framework.h>
#include <sysmodule/sysmodule.h>
#include "signin_ext_internal.h"

extern "C" {
int sceNpCommerce2Init(void);
int sceNpCommerce2Term(void);
int sceNpCommerce2DestroyReq(uint32_t reqId);
int sceNpCommerce2AbortReq(uint32_t reqId);
}

using namespace paf;

#define SCE_SYSMODULE_NP_COMMERCE2_ID		0x001D
#define SCE_SYSMODULE_INTERNAL_NP_COMMERCE2_ID	0x8000001DU
#define COMMERCE2_RECV_BUF_SIZE			0x10000U

static bool s_commerceInitialized = false;
static bool s_sysmoduleLoaded = false;
static bool s_commerce2Inited = false;
static uint32_t s_commerceCtxId = 0;
static void *s_commerceRecvBuf = NULL;
static bool s_sessionActive = false;

/* ---- Global Commerce2 lifecycle ---- */

int32_t SigninExtCommerceTerm(void)
{
	if (!s_commerceInitialized)
		return 0;

	if (s_commerceCtxId != 0) {
		SceNpCommerce2_1EC16D19(s_commerceCtxId);
		s_commerceCtxId = 0;
	}

	if (s_commerceRecvBuf != NULL) {
		SigninExtFree(s_commerceRecvBuf);
		s_commerceRecvBuf = NULL;
	}

	s_sessionActive = false;

	if (s_commerce2Inited) {
		sceNpCommerce2Term();
		s_commerce2Inited = false;
	}

	if (s_sysmoduleLoaded) {
		Framework *fw = Framework::Instance();
		if (fw->GetMode() == 5) {
			SysmoduleUserOpt opt = { 0, NULL, { 0, 0 } };
			sceSysmoduleUnloadModuleInternalWithArg(
				SCE_SYSMODULE_INTERNAL_NP_COMMERCE2_ID, 0, NULL, &opt);
		} else {
			sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_COMMERCE2_ID);
		}
		s_sysmoduleLoaded = false;
	}

	s_commerceInitialized = false;
	return 0;
}

int32_t SigninExtCommerceSetServerEnv(int32_t envMode)
{
	int32_t mapped;
	if (envMode == 1)
		mapped = 6;
	else if (envMode == 2)
		mapped = 1;
	else if (envMode == 3)
		mapped = 7;
	else if (envMode == 4)
		mapped = 8;
	else
		mapped = 5;
	return SceNpCommerce2_8ECAE666(s_commerceCtxId, mapped);
}

int32_t SigninExtCommerceInit(void)
{
	if (s_commerceInitialized)
		return 0;

	int32_t ret;
	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NP_COMMERCE2_ID) < 0) {
		Framework *fw = Framework::Instance();
		if (fw->GetMode() == 5) {
			SysmoduleUserOpt opt = { 16, NULL, { 0, 0 } };
			ret = sceSysmoduleLoadModuleInternalWithArg(
				SCE_SYSMODULE_INTERNAL_NP_COMMERCE2_ID, 0, NULL, &opt);
		} else {
			ret = sceSysmoduleLoadModule(SCE_SYSMODULE_NP_COMMERCE2_ID);
		}
		if (ret < 0) {
			SigninExtCommerceTerm();
			return ret;
		}
		s_sysmoduleLoaded = true;
	}

	if (SceNpCommerce2_25C58D33() == 0) {
		ret = sceNpCommerce2Init();
		if (ret < 0) {
			SigninExtCommerceTerm();
			return ret;
		}
		s_commerce2Inited = true;
	}

	s_commerceRecvBuf = SigninExtAlloc(COMMERCE2_RECV_BUF_SIZE);
	if (s_commerceRecvBuf == NULL) {
		SigninExtCommerceTerm();
		return SIGNIN_EXT_ERROR_NOMEM;
	}

	ret = SceNpCommerce2_AA090F5E(2, &s_commerceCtxId);
	if (ret < 0) {
		SigninExtCommerceTerm();
		return ret;
	}

	SigninExtCommerceSetServerEnv(1);
	s_commerceInitialized = true;
	return ret;
}

/* ---- Commerce2InitTask / Commerce2TermTask ---- */

Commerce2InitTask::Commerce2InitTask(Callback1<int32_t> *callback)
	: JobTask(SigninExtGetJobQueue(), callback, "SceSigninExtCommerce2")
{
}

int32_t Commerce2InitTask::Run(void)
{
	return SigninExtCommerceInit();
}

Commerce2TermTask::Commerce2TermTask(Callback1<int32_t> *callback)
	: JobTask(SigninExtGetJobQueue(), callback, "SceSigninExtCommerce2")
{
}

int32_t Commerce2TermTask::Run(void)
{
	return SigninExtCommerceTerm();
}

/* ---- Commerce2RequestTask ---- */

Commerce2RequestTask::Commerce2RequestTask(Callback1<int32_t> *callback)
	: JobTask(SigninExtGetJobQueue(), callback, "SceSigninExtCommerce2"),
	  m_reqId(0), m_recvSize(0)
{
}

Commerce2RequestTask::~Commerce2RequestTask()
{
}

int32_t Commerce2RequestTask::Run(void)
{
	int32_t ret = CreateReq();
	if (ret >= 0) {
		if (IsCanceled()) {
			ret = -1;
		} else {
			ret = SendReq();
			if (ret >= 0)
				ret = RecvResp();
		}
	}
	if (m_reqId != 0) {
		sceNpCommerce2DestroyReq(m_reqId);
		m_reqId = 0;
	}
	return ret;
}

void Commerce2RequestTask::Finish(int32_t result)
{
	if (result == SIGNIN_EXT_ERROR_COMMERCE_SESSION_EXPIRED)
		s_sessionActive = false;
	JobTask::Finish(result);
}

void Commerce2RequestTask::Cancel(void)
{
	if (m_reqId != 0)
		sceNpCommerce2AbortReq(m_reqId);
	JobItem::Cancel();
}

/* ---- Logout helper (inline synchronous logout before new login) ---- */

static void ExecuteSyncLogout(void)
{
	Commerce2LogoutTask *task = new Commerce2LogoutTask(NULL);
	if (task != NULL) {
		int32_t ret = task->Run();
		task->OnComplete(ret);
		delete task;
	}
}

/* ---- 1. Country list task ---- */

Commerce2GetCountryListTask::Commerce2GetCountryListTask(Callback1<int32_t> *callback)
	: Commerce2RequestTask(callback)
{
}

int32_t Commerce2GetCountryListTask::CreateReq(void)
{
	return SceNpCommerce2_749C8571(s_commerceCtxId, &m_reqId);
}

int32_t Commerce2GetCountryListTask::SendReq(void)
{
	return SceNpCommerce2_D09A38DA(m_reqId);
}

int32_t Commerce2GetCountryListTask::RecvResp(void)
{
	return SceNpCommerce2_45ACE466(m_reqId, s_commerceRecvBuf,
				       COMMERCE2_RECV_BUF_SIZE, &m_recvSize);
}

void Commerce2GetCountryListTask::OnComplete(int32_t result)
{
	if (result >= 0)
		result = SigninExtCountryTableParse(s_commerceRecvBuf, m_recvSize);
	TaskBase::OnComplete(result);
}

/* ---- 2. Login for EULA acceptance ---- */

Commerce2EulaLoginTask::Commerce2EulaLoginTask(Callback1<int32_t> *callback)
	: Commerce2RequestTask(callback)
{
}

int32_t Commerce2EulaLoginTask::Run(void)
{
	if (s_sessionActive)
		ExecuteSyncLogout();
	return Commerce2RequestTask::Run();
}

int32_t Commerce2EulaLoginTask::CreateReq(void)
{
	return SceNpCommerce2_B75D54D3(s_commerceCtxId, &m_reqId);
}

int32_t Commerce2EulaLoginTask::SendReq(void)
{
	string idUtf8;
	string pwUtf8;
	g_credentialsModel->m_id.ToUtf8(&idUtf8);
	g_credentialsModel->m_password.ToUtf8(&pwUtf8);
	return SceNpCommerce2_FE0EB375(m_reqId, 1, idUtf8.c_str(), pwUtf8.c_str(), 0);
}

int32_t Commerce2EulaLoginTask::RecvResp(void)
{
	return SceNpCommerce2_4A36892A(m_reqId, s_commerceRecvBuf,
				       COMMERCE2_RECV_BUF_SIZE, &m_recvSize);
}

void Commerce2EulaLoginTask::OnComplete(int32_t result)
{
	if (result >= 0) {
		s_sessionActive = true;
		SceNpCommerce2SessionResult sessRes;
		SceNpCommerce2AccountInfo acctInfo;
		result = SceNpCommerce2_115DF6B1(&sessRes, s_commerceRecvBuf, m_recvSize);
		if (result >= 0) {
			result = SceNpCommerce2_B37493FA(&sessRes, &acctInfo);
			if (result >= 0)
				result = g_accountInfoModel->LoadFromCommerceAccount(&acctInfo);
		}
	}
	TaskBase::OnComplete(result);
}

/* ---- 3. Accept EULA ---- */

Commerce2AcceptEulaTask::Commerce2AcceptEulaTask(Callback1<int32_t> *callback)
	: Commerce2RequestTask(callback)
{
}

int32_t Commerce2AcceptEulaTask::CreateReq(void)
{
	return SceNpCommerce2_76DA0D80(s_commerceCtxId, &m_reqId);
}

int32_t Commerce2AcceptEulaTask::SendReq(void)
{
	return SceNpCommerce2_F50D7BE4(m_reqId, 0);
}

int32_t Commerce2AcceptEulaTask::RecvResp(void)
{
	return SceNpCommerce2_F40D2B1E(m_reqId, s_commerceRecvBuf,
				       COMMERCE2_RECV_BUF_SIZE, &m_recvSize);
}

/* ---- 4. Verify date of birth ---- */

Commerce2VerifyDobTask::Commerce2VerifyDobTask(Callback1<int32_t> *callback)
	: Commerce2RequestTask(callback)
{
}

int32_t Commerce2VerifyDobTask::Run(void)
{
	if (s_sessionActive)
		ExecuteSyncLogout();
	return Commerce2RequestTask::Run();
}

int32_t Commerce2VerifyDobTask::CreateReq(void)
{
	return SceNpCommerce2_E1C27906(s_commerceCtxId, &m_reqId);
}

int32_t Commerce2VerifyDobTask::SendReq(void)
{
	string idUtf8;
	g_dobVerifyModel->m_signInId.ToUtf8(&idUtf8);
	int32_t year = g_dobVerifyModel->m_birthDate.m_year.GetValue();
	int32_t month = g_dobVerifyModel->m_birthDate.m_month.GetValue();
	int32_t day = g_dobVerifyModel->m_birthDate.m_day.GetValue();
	return SceNpCommerce2_1B229032(m_reqId, idUtf8.c_str(), year, month, day);
}

int32_t Commerce2VerifyDobTask::RecvResp(void)
{
	return SceNpCommerce2_2A22BEAF(m_reqId, s_commerceRecvBuf,
				       COMMERCE2_RECV_BUF_SIZE, &m_recvSize);
}

void Commerce2VerifyDobTask::OnComplete(int32_t result)
{
	if (result >= 0)
		s_sessionActive = true;
	TaskBase::OnComplete(result);
}

/* ---- 5. Send password reset email ---- */

Commerce2SendResetMailTask::Commerce2SendResetMailTask(Callback1<int32_t> *callback)
	: Commerce2RequestTask(callback)
{
}

int32_t Commerce2SendResetMailTask::CreateReq(void)
{
	return SceNpCommerce2_4615377F(s_commerceCtxId, &m_reqId);
}

int32_t Commerce2SendResetMailTask::SendReq(void)
{
	return SceNpCommerce2_536DA1DA(m_reqId);
}

int32_t Commerce2SendResetMailTask::RecvResp(void)
{
	return SceNpCommerce2_2AD3766D(m_reqId, s_commerceRecvBuf,
				       COMMERCE2_RECV_BUF_SIZE, &m_recvSize);
}

void Commerce2SendResetMailTask::OnComplete(int32_t result)
{
	if (result >= 0)
		s_sessionActive = false;
	TaskBase::OnComplete(result);
}

/* ---- 6. Login for password change ---- */

Commerce2PasswordLoginTask::Commerce2PasswordLoginTask(Callback1<int32_t> *callback)
	: Commerce2RequestTask(callback)
{
}

int32_t Commerce2PasswordLoginTask::Run(void)
{
	if (s_sessionActive)
		ExecuteSyncLogout();
	return Commerce2RequestTask::Run();
}

int32_t Commerce2PasswordLoginTask::CreateReq(void)
{
	return SceNpCommerce2_B75D54D3(s_commerceCtxId, &m_reqId);
}

int32_t Commerce2PasswordLoginTask::SendReq(void)
{
	string idUtf8;
	string pwUtf8;
	g_credentialsModel->m_id.ToUtf8(&idUtf8);
	g_credentialsModel->m_password.ToUtf8(&pwUtf8);
	return SceNpCommerce2_FE0EB375(m_reqId, 1, idUtf8.c_str(), pwUtf8.c_str(), 0);
}

int32_t Commerce2PasswordLoginTask::RecvResp(void)
{
	return SceNpCommerce2_4A36892A(m_reqId, s_commerceRecvBuf,
				       COMMERCE2_RECV_BUF_SIZE, &m_recvSize);
}

void Commerce2PasswordLoginTask::OnComplete(int32_t result)
{
	if (result >= 0) {
		s_sessionActive = true;
	} else if (result != SIGNIN_EXT_ERROR_COMMERCE_PW_EXPIRED) {
		TaskBase::OnComplete(result);
		return;
	}

	SceNpCommerce2SessionResult sessRes;
	SceNpCommerce2AccountInfo acctInfo;
	int32_t parseRet = SceNpCommerce2_115DF6B1(&sessRes, s_commerceRecvBuf, m_recvSize);
	if (parseRet >= 0) {
		parseRet = SceNpCommerce2_B37493FA(&sessRes, &acctInfo);
		if (parseRet >= 0) {
			int32_t loadRet = g_passwordChangeModel->LoadFromCommerceAccount(&acctInfo);
			if (loadRet < 0)
				result = loadRet;
		} else {
			result = parseRet;
		}
	} else {
		result = parseRet;
	}

	TaskBase::OnComplete(result);
}

/* ---- 7. Change password ---- */

Commerce2ChangePasswordTask::Commerce2ChangePasswordTask(Callback1<int32_t> *callback)
	: Commerce2RequestTask(callback)
{
}

int32_t Commerce2ChangePasswordTask::CreateReq(void)
{
	return SceNpCommerce2_F4DD3FD8(s_commerceCtxId, &m_reqId);
}

int32_t Commerce2ChangePasswordTask::SendReq(void)
{
	string newPwUtf8;
	g_passwordChangeModel->m_newPassword.ToUtf8(&newPwUtf8);
	return SceNpCommerce2_A4C52416(m_reqId, newPwUtf8.c_str(), 0);
}

int32_t Commerce2ChangePasswordTask::RecvResp(void)
{
	int32_t ret = SceNpCommerce2_2FC09E2B(m_reqId, s_commerceRecvBuf,
					      COMMERCE2_RECV_BUF_SIZE, &m_recvSize);
	if (ret >= 0) {
		string idUtf8;
		g_passwordChangeModel->m_signInId.ToUtf8(&idUtf8);
		char idBuf[68];
		sce_paf_memset(idBuf, 0, 0x41);
		sce_paf_strncpy(idBuf, idUtf8.c_str(), 0x40);

		string pwUtf8;
		g_passwordChangeModel->m_newPassword.ToUtf8(&pwUtf8);
		char pwBuf[36];
		sce_paf_memset(pwBuf, 0, 0x21);
		sce_paf_strncpy(pwBuf, pwUtf8.c_str(), 0x1E);

		ret = SceNpManager_455E05C1(g_passwordChangeModel->m_accountId, idBuf, pwBuf);
		if (ret == SIGNIN_EXT_ERROR_NP_ACCOUNT_NOT_MATCH)
			ret = 0;
	} else if (ret == SIGNIN_EXT_ERROR_COMMERCE_BAD_REQUEST) {
		SceNpCommerce2ErrorList errList;
		if (SceNpCommerce2_8F24F115(&errList, s_commerceRecvBuf, m_recvSize) >= 0) {
			for (uint32_t i = 0; i < errList.count; ++i) {
				SceNpCommerce2ErrorItem item;
				if (SceNpCommerce2_1B0B5798(&errList, i, &item) < 0)
					break;
				if (item.code == 117) {
					ret = SIGNIN_EXT_ERROR_PW_SAME_AS_OLD;
					break;
				}
			}
		}
	}
	return ret;
}

void Commerce2ChangePasswordTask::OnComplete(int32_t result)
{
	g_passwordChangeModel->m_newPassword.SetServerResult(result);
	TaskBase::OnComplete(result);
}

/* ---- 8. Reset password by sign-in ID ---- */

Commerce2ResetPasswordTask::Commerce2ResetPasswordTask(Callback1<int32_t> *callback)
	: Commerce2RequestTask(callback)
{
}

int32_t Commerce2ResetPasswordTask::CreateReq(void)
{
	return SceNpCommerce2_9FD5705B(s_commerceCtxId, &m_reqId);
}

int32_t Commerce2ResetPasswordTask::SendReq(void)
{
	string idUtf8;
	g_credentialsModel->m_id.ToUtf8(&idUtf8);
	return SceNpCommerce2_B7552026(m_reqId, idUtf8.c_str());
}

int32_t Commerce2ResetPasswordTask::RecvResp(void)
{
	return SceNpCommerce2_FFFC0101(m_reqId, s_commerceRecvBuf,
				       COMMERCE2_RECV_BUF_SIZE, &m_recvSize);
}

void Commerce2ResetPasswordTask::OnComplete(int32_t result)
{
	if (result == SIGNIN_EXT_ERROR_COMMERCE_RESET_SENT)
		result = 0;
	else if (result == SIGNIN_EXT_ERROR_COMMERCE_NOT_FOUND)
		result = SIGNIN_EXT_ERROR_ACCOUNT_NOT_FOUND;
	TaskBase::OnComplete(result);
}

/* ---- 9. Logout ---- */

Commerce2LogoutTask::Commerce2LogoutTask(Callback1<int32_t> *callback)
	: Commerce2RequestTask(callback)
{
}

int32_t Commerce2LogoutTask::Run(void)
{
	if (s_sessionActive)
		return Commerce2RequestTask::Run();
	return 0;
}

int32_t Commerce2LogoutTask::CreateReq(void)
{
	return SceNpCommerce2_C737A929(s_commerceCtxId, &m_reqId);
}

int32_t Commerce2LogoutTask::SendReq(void)
{
	return SceNpCommerce2_2DC2A0F9(m_reqId);
}

int32_t Commerce2LogoutTask::RecvResp(void)
{
	return SceNpCommerce2_7E3C9196(m_reqId, s_commerceRecvBuf,
				       COMMERCE2_RECV_BUF_SIZE, &m_recvSize);
}

void Commerce2LogoutTask::OnComplete(int32_t result)
{
	(void)result;
	s_sessionActive = false;
	TaskBase::OnComplete(0);
}
