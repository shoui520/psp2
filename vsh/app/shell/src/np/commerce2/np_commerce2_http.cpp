/*
 * np_commerce2_http.cpp: shell-internal SceNpCommerce2 endpoint tables, form-encoded
 * HTTP request builders, context list management, and HTTP transport helpers
 * (0x812BFDF4 - 0x812C16B3).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include "np_commerce2_core.h"

extern "C" {
extern int sceHttpsEnableOptionPrivate(int tmplId, SceUInt32 sslFlags);
extern int sceHttpsDisableOptionPrivate(int tmplId, SceUInt32 sslFlags);
}

#define NP_COMM2_MAX_SHELL_CTX		32u
#define NP_COMM2_URI_ESCAPE_BUF_SIZE	0x301u
#define NP_COMM2_SSL_ENABLE_FLAGS	357u
#define NP_COMM2_SSL_DISABLE_FLAGS	24u

struct FormKeyEntry {
	SceSize length;
	const char *key;
};

struct DefaultHttpHeader {
	const char *name;
	const char *value;
};

/* Service path prefixes indexed by serviceType (1..8, 0x8151AF10) */
static const char *const s_servicePrefixes[9] = {
	NULL,
	"store/",
	"store/",
	"store/",
	"store/",
	"reg/",
	"cam/",
	"upgrade/",
	"pwd/"
};

/* Action names indexed by operation ID (0..113, 0x8151AD18) */
static const char *const s_actionNames[SHELL_NP_COMM2_OP_MAX] = {
	NULL,
	"crSess",
	"delSess",
	"getCat",
	"getProd",
	"srchProd",
	"getWal",
	"getDrm",
	"purPrev",
	"purch",
	"getCC",
	"topupCC",
	"vchrRedPrev",
	"vchrRed",
	"topEdy",
	"topEdyRes",
	"addCart",
	"remCart",
	"clearCart",
	"getCart",
	"getProv",
	"getPay",
	"getBill",
	"regCC",
	"getAddr",
	"getResAddr",
	"eventLog",
	"getCountries",
	"bindAccount",
	"startReg",
	"getSecQ",
	"setCred",
	"setLid",
	"updPass",
	"verifyOid",
	"setOid",
	"getName",
	"setName",
	"setResAddr",
	"setRrn",
	"getOptIn",
	"setOptIn",
	"authMaster",
	"getBillInfo",
	"setSubCtrl",
	"resetSubPass",
	"getWalOpt",
	"setWalOpt",
	"submitReg",
	"authenticate",
	"remPay",
	"getTranHist",
	"getTranDetail",
	"getSrvs",
	"getSrvDetail",
	"getSubscr",
	"getSubscrPlan",
	"changeSubscr",
	"cancelSubscr",
	"getCnslAct",
	"acptEula",
	"verifyDob",
	"verifySecAns",
	"sendPassResetMail",
	"verifyMasterBill",
	"getEpw",
	"crGuestSess",
	"resetPass",
	"getMasterBill",
	"sendConsentMail",
	"getMasterResAddr",
	"getProdList",
	"getSkuList",
	"getReward",
	"setSecQ",
	"submitUpgrade",
	"recomProds",
	"verifyBill",
	"regDevice",
	"deregDevice",
	"getProdIds",
	"crSessTicket",
	"getUsersFromCID",
	"getActCode",
	"setAccountLang",
	"getPivotForProd",
	"getProdForPivot",
	"cancelOrder",
	"checkAccountEula",
	"useEntlmt",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"getEntState",
	"esnDuid",
	"crSessExpTicket",
	NULL,
	"sendEmailVer",
	"heRegistration",
	"getEntRev",
	"crSessByTickLite",
	"crSessExpTickLite",
	"crSessByAccessToken",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"crSessByAccessTokenLite",
	"verifyAccountPass"
};

/* Form field keys (0x8151B3B0) */
enum FormKeyIndex {
	FORM_KEY_VERSION		= 2,
	FORM_KEY_USERNAME		= 3,
	FORM_KEY_PASSWORD		= 4,
	FORM_KEY_CONSOLE_ID		= 5,
	FORM_KEY_COUNTRY		= 41,
	FORM_KEY_LANGUAGE		= 42,
	FORM_KEY_YOB			= 43,
	FORM_KEY_MOB			= 44,
	FORM_KEY_DOB			= 45,
	FORM_KEY_LOGIN_NAME		= 46,
	FORM_KEY_SERVICE_ENTITY		= 83
};

static const FormKeyEntry s_formKeys[87] = {
	{ 1,  "&" },
	{ 1,  "," },
	{ 8,  "version=" },
	{ 9,  "username=" },
	{ 9,  "password=" },
	{ 10, "consoleId=" },
	{ 11, "categoryId=" },
	{ 14, "startPosition=" },
	{ 18, "maxCountOfResults=" },
	{ 10, "productId=" },
	{ 10, "storeType=" },
	{ 9,  "queryKey=" },
	{ 11, "queryValue=" },
	{ 18, "contentSortMethod=" },
	{ 6,  "skuId=" },
	{ 7,  "skuIds=" },
	{ 8,  "titleId=" },
	{ 15, "drmContentType=" },
	{ 14, "quickPurchase=" },
	{ 12, "topupAmount=" },
	{ 2,  "1=" },
	{ 2,  "2=" },
	{ 2,  "3=" },
	{ 13, "edyReceiptId=" },
	{ 21, "resultOfTopupFromEdy=" },
	{ 12, "paymentType=" },
	{ 16, "paymentMethodId=" },
	{ 11, "holderName=" },
	{ 11, "cardNumber=" },
	{ 10, "startYear=" },
	{ 11, "startMonth=" },
	{ 11, "expireYear=" },
	{ 12, "expireMonth=" },
	{ 13, "securityCode=" },
	{ 12, "issueNumber=" },
	{ 5,  "city=" },
	{ 9,  "province=" },
	{ 11, "postalCode=" },
	{ 10, "eventType=" },
	{ 3,  "id=" },
	{ 4,  "url=" },
	{ 8,  "country=" },
	{ 9,  "language=" },
	{ 4,  "yob=" },
	{ 4,  "mob=" },
	{ 4,  "dob=" },
	{ 10, "loginName=" },
	{ 17, "securityQuestion=" },
	{ 15, "securityAnswer=" },
	{ 9,  "psHandle=" },
	{ 10, "firstName=" },
	{ 9,  "lastName=" },
	{ 15, "firstNameASCII=" },
	{ 14, "lastNameASCII=" },
	{ 7,  "gender=" },
	{ 14, "scexOptinFlag=" },
	{ 20, "affiliatesOptinFlag=" },
	{ 13, "subLoginName=" },
	{ 14, "contentRating=" },
	{ 12, "chatControl=" },
	{ 14, "searchControl=" },
	{ 18, "spendingLimitFlag=" },
	{ 14, "spendingLimit=" },
	{ 16, "autoDepositFlag=" },
	{ 21, "commercePasswordFlag=" },
	{ 9,  "fromYear=" },
	{ 10, "fromMonth=" },
	{ 8,  "fromDay=" },
	{ 7,  "toYear=" },
	{ 8,  "toMonth=" },
	{ 6,  "toDay=" },
	{ 9,  "timezone=" },
	{ 14, "transactionId=" },
	{ 10, "serviceId=" },
	{ 5,  "save=" },
	{ 9,  "rewardId=" },
	{ 10, "rewardIds=" },
	{ 11, "productIds=" },
	{ 19, "recommendationType=" },
	{ 10, "contentId=" },
	{ 7,  "fileId=" },
	{ 17, "autoDownloadFlag=" },
	{ 12, "orderItemId=" },
	{ 14, "serviceEntity=" },
	{ 12, "phoneNumber=" },
	{ 10, "cpfNumber=" },
	{ 16, "currentPassword=" }
};

/* Default request headers added to every Commerce2 POST (0x8151B7A0) */
static const DefaultHttpHeader s_defaultHeaders[4] = {
	{ "Content-Type", "application/x-www-form-urlencoded" },
	{ "Authorization", "Basic cjI0NjpHUG9FZmJkYQ==" },
	{ "X-MediaInformation", "PSP2/480x272" },
	{ "Accept-Language",
	  "ja, en, fr, es, de, it, nl, pt, ru, ko, zh, ch, fi, sv, da, no, pl, tr" }
};

static ShellNpCommerce2Context *s_contextList = NULL;		/* 0x81545BB8 */
static SceUInt32 s_lastCtxId = 0;				/* 0x81545BBC */
static SceUID s_cookieThreadId = -1;				/* 0x81545BC0 */
static char s_threadCookie[256];				/* 0x815C3738 */

/* 0x812BFDF4 */
int npComm2ResolveEndpoint(SceUInt32 serviceType, SceUInt16 op,
			   const char **outPrefix, const char **outAction)
{
	if (op >= SHELL_NP_COMM2_OP_MAX)
		return SHELL_NP_COMM2_ERR_INVALID_OP;
	if (serviceType == 0 || serviceType >= 9u)
		return SHELL_NP_COMM2_ERR_INVALID_SERVICE_TYPE;

	char env[20];
	int ret = npComm2GetEnvironment(env, 17u);
	if (ret < 0)
		return ret;

	*outPrefix = s_servicePrefixes[serviceType];
	SceUInt16 resolvedOp = op;
	if (serviceType == 7u) {
		if (resolvedOp == SHELL_NP_COMM2_OP_SET_CRED)
			resolvedOp = SHELL_NP_COMM2_OP_SET_SEC_Q;
		else if (resolvedOp == SHELL_NP_COMM2_OP_SUBMIT_REG)
			resolvedOp = SHELL_NP_COMM2_OP_SUBMIT_UPGRADE;
	}
	*outAction = s_actionNames[resolvedOp];
	return 0;
}

/* 0x812BFE86 */
static int emitUintFormParam(int sendBody, int httpReq, int keyIndex, unsigned int value,
			     int prependAmp)
{
	char numBuf[16];
	if (sendBody == 1) {
		if (prependAmp == 1) {
			int ret = npComm2HttpSendRequest(httpReq, "&", 1u);
			if (ret < 0)
				return ret;
		}
		int ret = npComm2HttpSendRequest(httpReq, s_formKeys[keyIndex].key,
						 s_formKeys[keyIndex].length);
		if (ret < 0)
			return ret;
		if (sceClibSnprintf(numBuf, sizeof(numBuf), "%u", value) >= 16)
			return SHELL_NP_COMM2_ERR_FORM_OVERFLOW;
		return npComm2HttpSendRequest(httpReq, numBuf, sceClibStrnlen(numBuf, 0x400u));
	}

	int written = sceClibSnprintf(numBuf, sizeof(numBuf), "%u", value);
	if (written >= 16)
		return SHELL_NP_COMM2_ERR_FORM_OVERFLOW;
	return static_cast<int>(s_formKeys[keyIndex].length) + ((prependAmp == 1) ? 1 : 0) + written;
}

/* 0x812BFF5E */
static int emitStringFormParam(int sendBody, int httpReq, int keyIndex, const char *value,
			       int prependAmp)
{
	if (sendBody != 1) {
		return static_cast<int>(s_formKeys[keyIndex].length +
					sceClibStrnlen(value, 0x400u) +
					((prependAmp == 1) ? 1u : 0u));
	}

	if (prependAmp == 1) {
		int ret = npComm2HttpSendRequest(httpReq, "&", 1u);
		if (ret < 0)
			return ret;
	}
	int ret = npComm2HttpSendRequest(httpReq, s_formKeys[keyIndex].key,
					 s_formKeys[keyIndex].length);
	if (ret < 0)
		return ret;
	return npComm2HttpSendRequest(httpReq, value, sceClibStrnlen(value, 0x400u));
}

/* 0x812BFFDC */
int npComm2SendEmptyAction(SceUInt32 serviceType, int httpTemplate, int *httpConn,
			   SceUInt16 op, int *outHttpReq)
{
	const char *prefix = NULL;
	const char *action = NULL;
	int ret = npComm2ResolveEndpoint(serviceType, op, &prefix, &action);
	if (ret >= 0)
		ret = npComm2HttpCreateRequest(httpTemplate, serviceType, op, prefix, action, 0,
					       httpConn, outHttpReq, 0ULL);
	if (ret >= 0) {
		ret = npComm2HttpSendRequest(*outHttpReq, NULL, 0u);
		if (ret >= 0)
			return 0;
	}
	return ret;
}

/* 0x812C0064 */
static int emitPrefixedFormParam(int sendBody, int httpReq, const char *const *prefixes,
				 unsigned int prefixCount, int keyIndex, const char *value,
				 SceSize valueLen, int prependAmp)
{
	if (sendBody == 1) {
		if (prependAmp == 1) {
			int ret = npComm2HttpSendRequest(httpReq, "&", 1u);
			if (ret < 0)
				return ret;
		}
		for (unsigned int i = 0; i < prefixCount; ++i) {
			if (prefixes[i] != NULL) {
				int ret = npComm2HttpSendRequest(httpReq, prefixes[i],
								 sceClibStrnlen(prefixes[i], 0x400u));
				if (ret < 0)
					return ret;
			}
		}
		int ret = npComm2HttpSendRequest(httpReq, s_formKeys[keyIndex].key,
						 s_formKeys[keyIndex].length);
		if (ret < 0)
			return ret;
		return npComm2HttpSendRequest(httpReq, value, valueLen);
	}

	SceSize total = (prependAmp == 1) ? 1u : 0u;
	for (unsigned int i = 0; i < prefixCount; ++i)
		total += sceClibStrnlen(prefixes[i], 0x400u);
	return static_cast<int>(total + s_formKeys[keyIndex].length + valueLen);
}

/* 0x812C0128 */
int npComm2SendCredsAction(SceUInt32 serviceType, int httpTemplate, int *httpConn,
			   SceUInt16 op, int *outHttpReq, int protoVer,
			   const char *username, const char *password, const char *consoleId)
{
	char escaped[NP_COMM2_URI_ESCAPE_BUF_SIZE];
	SceUInt64 bodyLen = 0;

	for (int pass = 0; pass < 2; ++pass) {
		int sendBody = (pass != 0) ? 1 : 0;
		int httpReq = *outHttpReq;
		SceUInt64 total = 0;

		int ret = emitUintFormParam(sendBody, httpReq, FORM_KEY_VERSION,
					    static_cast<unsigned int>(protoVer), 0);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		ret = npComm2HttpUriEscape(escaped, sizeof(escaped), username,
					   sceClibStrnlen(username, sizeof(escaped)), NULL);
		if (ret < 0)
			return ret;
		ret = emitStringFormParam(sendBody, httpReq, FORM_KEY_USERNAME, escaped, 1);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		ret = npComm2HttpUriEscape(escaped, sizeof(escaped), password,
					   sceClibStrnlen(password, sizeof(escaped)), NULL);
		if (ret < 0)
			return ret;
		ret = emitStringFormParam(sendBody, httpReq, FORM_KEY_PASSWORD, escaped, 1);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		ret = emitStringFormParam(sendBody, httpReq, FORM_KEY_CONSOLE_ID, consoleId, 1);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		if (pass == 0) {
			bodyLen = total;
			const char *prefix = NULL;
			const char *action = NULL;
			ret = npComm2ResolveEndpoint(serviceType, op, &prefix, &action);
			if (ret >= 0) {
				ret = npComm2HttpCreateRequest(httpTemplate, serviceType, op,
							       prefix, action, 0, httpConn,
							       outHttpReq, bodyLen);
			}
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

/* 0x812C02B4 (startReg form body with optional serviceEntity) */
static int encodeAccountLocaleDobBody(int sendBody, int httpReq, SceUInt64 *outBodyLen,
				      int protoVer, const char *country, const char *language,
				      int birthYear, int birthMonth, int birthDay,
				      const char *consoleId, const char *serviceEntity)
{
	const char *prefixes[2] = { "account.", NULL };
	char buf[16];
	SceUInt64 total = 0;

	int ret = emitUintFormParam(sendBody, httpReq, FORM_KEY_VERSION,
				    static_cast<unsigned int>(protoVer), 0);
	if (ret < 0)
		return ret;
	total += static_cast<SceUInt64>(ret);

	SceSize countryLen = sceClibStrnlen(country, 16u);
	SceSize idx = 0;
	for (; idx < countryLen && idx < 16u; ++idx)
		buf[idx] = static_cast<char>(sceClibToupper(country[idx]));
	buf[(idx == 16u) ? 15u : idx] = '\0';
	ret = emitPrefixedFormParam(sendBody, httpReq, prefixes, 1u, FORM_KEY_COUNTRY,
				    buf, sceClibStrnlen(buf, 0x400u), 1);
	if (ret < 0)
		return ret;
	total += static_cast<SceUInt64>(ret);

	SceSize langLen = sceClibStrnlen(language, 16u);
	idx = 0;
	for (; idx < langLen && idx < 16u; ++idx)
		buf[idx] = static_cast<char>(sceClibTolower(language[idx]));
	buf[(idx == 16u) ? 15u : idx] = '\0';
	ret = emitPrefixedFormParam(sendBody, httpReq, prefixes, 1u, FORM_KEY_LANGUAGE,
				    buf, sceClibStrnlen(buf, 0x400u), 1);
	if (ret < 0)
		return ret;
	total += static_cast<SceUInt64>(ret);

	sceClibSnprintf(buf, sizeof(buf), "%u", birthYear);
	ret = emitPrefixedFormParam(sendBody, httpReq, prefixes, 1u, FORM_KEY_YOB,
				    buf, sceClibStrnlen(buf, 0x400u), 1);
	if (ret < 0)
		return ret;
	total += static_cast<SceUInt64>(ret);

	sceClibSnprintf(buf, sizeof(buf), "%u", birthMonth);
	ret = emitPrefixedFormParam(sendBody, httpReq, prefixes, 1u, FORM_KEY_MOB,
				    buf, sceClibStrnlen(buf, 0x400u), 1);
	if (ret < 0)
		return ret;
	total += static_cast<SceUInt64>(ret);

	sceClibSnprintf(buf, sizeof(buf), "%u", birthDay);
	ret = emitPrefixedFormParam(sendBody, httpReq, prefixes, 1u, FORM_KEY_DOB,
				    buf, sceClibStrnlen(buf, 0x400u), 1);
	if (ret < 0)
		return ret;
	total += static_cast<SceUInt64>(ret);

	ret = emitStringFormParam(sendBody, httpReq, FORM_KEY_CONSOLE_ID, consoleId, 1);
	if (ret < 0)
		return ret;
	total += static_cast<SceUInt64>(ret);

	if (serviceEntity != NULL) {
		ret = emitStringFormParam(sendBody, httpReq, FORM_KEY_SERVICE_ENTITY,
					  serviceEntity, 1);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);
	}

	if (sendBody != 1)
		*outBodyLen = total;
	return 0;
}

/* 0x812C0558 */
int npComm2SendStartRegAction(SceUInt32 serviceType, int httpTemplate, int *httpConn,
			      SceUInt16 op, int *outHttpReq, int protoVer,
			      const char *country, const char *language,
			      int birthYear, int birthMonth, int birthDay,
			      const char *consoleId, int usePsnEntity)
{
	SceUInt64 bodyLen = 0;
	const char *entity = (usePsnEntity == 1) ? "urn:service-entity:psn2" : NULL;

	for (int pass = 0; pass < 2; ++pass) {
		int ret = encodeAccountLocaleDobBody((pass != 0) ? 1 : 0, *outHttpReq, &bodyLen,
						     protoVer, country, language, birthYear,
						     birthMonth, birthDay, consoleId, entity);
		if (ret < 0)
			return ret;
		if (pass == 0) {
			const char *prefix = NULL;
			const char *action = NULL;
			ret = npComm2ResolveEndpoint(serviceType, op, &prefix, &action);
			if (ret >= 0) {
				ret = npComm2HttpCreateRequest(httpTemplate, serviceType, op,
							       prefix, action, 0, httpConn,
							       outHttpReq, bodyLen);
			}
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

/* 0x812C065E */
int npComm2SendAuthAction(SceUInt32 serviceType, int httpTemplate, int *httpConn,
			  SceUInt16 op, int *outHttpReq, int protoVer,
			  const char *username, const char *password,
			  const char *consoleId, int usePsnEntity)
{
	char escaped[NP_COMM2_URI_ESCAPE_BUF_SIZE];
	SceUInt64 bodyLen = 0;
	const char *entity = (usePsnEntity == 1) ? "urn:service-entity:psn2" : NULL;

	for (int pass = 0; pass < 2; ++pass) {
		int sendBody = (pass != 0) ? 1 : 0;
		int httpReq = *outHttpReq;
		SceUInt64 total = 0;

		int ret = emitUintFormParam(sendBody, httpReq, FORM_KEY_VERSION,
					    static_cast<unsigned int>(protoVer), 0);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		ret = npComm2HttpUriEscape(escaped, sizeof(escaped), username,
					   sceClibStrnlen(username, sizeof(escaped)), NULL);
		if (ret < 0)
			return ret;
		ret = emitStringFormParam(sendBody, httpReq, FORM_KEY_USERNAME, escaped, 1);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		ret = npComm2HttpUriEscape(escaped, sizeof(escaped), password,
					   sceClibStrnlen(password, sizeof(escaped)), NULL);
		if (ret < 0)
			return ret;
		ret = emitStringFormParam(sendBody, httpReq, FORM_KEY_PASSWORD, escaped, 1);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		ret = emitStringFormParam(sendBody, httpReq, FORM_KEY_CONSOLE_ID, consoleId, 1);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		if (entity != NULL) {
			ret = emitStringFormParam(sendBody, httpReq, FORM_KEY_SERVICE_ENTITY,
						  entity, 1);
			if (ret < 0)
				return ret;
			total += static_cast<SceUInt64>(ret);
		}

		if (pass == 0) {
			bodyLen = total;
			const char *prefix = NULL;
			const char *action = NULL;
			ret = npComm2ResolveEndpoint(serviceType, op, &prefix, &action);
			if (ret >= 0) {
				ret = npComm2HttpCreateRequest(httpTemplate, serviceType, op,
							       prefix, action, 0, httpConn,
							       outHttpReq, bodyLen);
			}
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

/* 0x812C082E */
int npComm2SendVerifyDobAction(SceUInt32 serviceType, int httpTemplate, int *httpConn,
			       SceUInt16 op, int *outHttpReq, int protoVer,
			       const char *loginName, int birthYear, int birthMonth,
			       int birthDay, const char *consoleId)
{
	char escaped[NP_COMM2_URI_ESCAPE_BUF_SIZE];
	const char *prefixes[2] = { "account.", NULL };
	SceUInt64 bodyLen = 0;

	for (int pass = 0; pass < 2; ++pass) {
		int sendBody = (pass != 0) ? 1 : 0;
		int httpReq = *outHttpReq;
		SceUInt64 total = 0;

		int ret = emitUintFormParam(sendBody, httpReq, FORM_KEY_VERSION,
					    static_cast<unsigned int>(protoVer), 0);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		ret = npComm2HttpUriEscape(escaped, sizeof(escaped), loginName,
					   sceClibStrnlen(loginName, sizeof(escaped)), NULL);
		if (ret < 0)
			return ret;
		ret = emitPrefixedFormParam(sendBody, httpReq, prefixes, 1u, FORM_KEY_LOGIN_NAME,
					    escaped, sceClibStrnlen(escaped, 0x400u), 1);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		sceClibSnprintf(escaped, sizeof(escaped), "%u", birthYear);
		ret = emitPrefixedFormParam(sendBody, httpReq, prefixes, 1u, FORM_KEY_YOB,
					    escaped, sceClibStrnlen(escaped, 0x400u), 1);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		sceClibSnprintf(escaped, sizeof(escaped), "%u", birthMonth);
		ret = emitPrefixedFormParam(sendBody, httpReq, prefixes, 1u, FORM_KEY_MOB,
					    escaped, sceClibStrnlen(escaped, 0x400u), 1);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		sceClibSnprintf(escaped, sizeof(escaped), "%u", birthDay);
		ret = emitPrefixedFormParam(sendBody, httpReq, prefixes, 1u, FORM_KEY_DOB,
					    escaped, sceClibStrnlen(escaped, 0x400u), 1);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		ret = emitStringFormParam(sendBody, httpReq, FORM_KEY_CONSOLE_ID, consoleId, 1);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		if (pass == 0) {
			bodyLen = total;
			const char *prefix = NULL;
			const char *action = NULL;
			ret = npComm2ResolveEndpoint(serviceType, op, &prefix, &action);
			if (ret >= 0) {
				ret = npComm2HttpCreateRequest(httpTemplate, serviceType, op,
							       prefix, action, 0, httpConn,
							       outHttpReq, bodyLen);
			}
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

/* 0x812C0A90 (crGuestSess form body) */
static int encodeGuestSessBody(int sendBody, int httpReq, SceUInt64 *outBodyLen,
			       int protoVer, const char *country, const char *language,
			       int birthYear, int birthMonth, int birthDay,
			       const char *consoleId)
{
	const char *prefixes[2] = { "account.", NULL };
	char buf[16];
	SceUInt64 total = 0;

	int ret = emitUintFormParam(sendBody, httpReq, FORM_KEY_VERSION,
				    static_cast<unsigned int>(protoVer), 0);
	if (ret < 0)
		return ret;
	total += static_cast<SceUInt64>(ret);

	SceSize countryLen = sceClibStrnlen(country, 16u);
	SceSize idx = 0;
	for (; idx < countryLen && idx < 16u; ++idx)
		buf[idx] = static_cast<char>(sceClibToupper(country[idx]));
	buf[(idx == 16u) ? 15u : idx] = '\0';
	ret = emitPrefixedFormParam(sendBody, httpReq, prefixes, 1u, FORM_KEY_COUNTRY,
				    buf, sceClibStrnlen(buf, 0x400u), 1);
	if (ret < 0)
		return ret;
	total += static_cast<SceUInt64>(ret);

	SceSize langLen = sceClibStrnlen(language, 16u);
	idx = 0;
	for (; idx < langLen && idx < 16u; ++idx)
		buf[idx] = static_cast<char>(sceClibTolower(language[idx]));
	buf[(idx == 16u) ? 15u : idx] = '\0';
	ret = emitPrefixedFormParam(sendBody, httpReq, prefixes, 1u, FORM_KEY_LANGUAGE,
				    buf, sceClibStrnlen(buf, 0x400u), 1);
	if (ret < 0)
		return ret;
	total += static_cast<SceUInt64>(ret);

	sceClibSnprintf(buf, sizeof(buf), "%u", birthYear);
	ret = emitPrefixedFormParam(sendBody, httpReq, prefixes, 1u, FORM_KEY_YOB,
				    buf, sceClibStrnlen(buf, 0x400u), 1);
	if (ret < 0)
		return ret;
	total += static_cast<SceUInt64>(ret);

	sceClibSnprintf(buf, sizeof(buf), "%u", birthMonth);
	ret = emitPrefixedFormParam(sendBody, httpReq, prefixes, 1u, FORM_KEY_MOB,
				    buf, sceClibStrnlen(buf, 0x400u), 1);
	if (ret < 0)
		return ret;
	total += static_cast<SceUInt64>(ret);

	sceClibSnprintf(buf, sizeof(buf), "%u", birthDay);
	ret = emitPrefixedFormParam(sendBody, httpReq, prefixes, 1u, FORM_KEY_DOB,
				    buf, sceClibStrnlen(buf, 0x400u), 1);
	if (ret < 0)
		return ret;
	total += static_cast<SceUInt64>(ret);

	ret = emitStringFormParam(sendBody, httpReq, FORM_KEY_CONSOLE_ID, consoleId, 1);
	if (ret < 0)
		return ret;
	total += static_cast<SceUInt64>(ret);

	if (sendBody != 1)
		*outBodyLen = total;
	return 0;
}

/* 0x812C0D0A */
int npComm2SendGuestSessAction(SceUInt32 serviceType, int httpTemplate, int *httpConn,
			       SceUInt16 op, int *outHttpReq, int protoVer,
			       const char *country, const char *language,
			       int birthYear, int birthMonth, int birthDay,
			       const char *consoleId)
{
	SceUInt64 bodyLen = 0;

	for (int pass = 0; pass < 2; ++pass) {
		int ret = encodeGuestSessBody((pass != 0) ? 1 : 0, *outHttpReq, &bodyLen,
					      protoVer, country, language, birthYear,
					      birthMonth, birthDay, consoleId);
		if (ret < 0)
			return ret;
		if (pass == 0) {
			const char *prefix = NULL;
			const char *action = NULL;
			ret = npComm2ResolveEndpoint(serviceType, op, &prefix, &action);
			if (ret >= 0) {
				ret = npComm2HttpCreateRequest(httpTemplate, serviceType, op,
							       prefix, action, 0, httpConn,
							       outHttpReq, bodyLen);
			}
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

/* 0x812C0DF8 */
int npComm2SendEmailVerAction(SceUInt32 serviceType, int httpTemplate, int *httpConn,
			      SceUInt16 op, int *outHttpReq, int protoVer,
			      const char *username, const char *consoleId)
{
	char escaped[NP_COMM2_URI_ESCAPE_BUF_SIZE];
	SceUInt64 bodyLen = 0;

	for (int pass = 0; pass < 2; ++pass) {
		int sendBody = (pass != 0) ? 1 : 0;
		int httpReq = *outHttpReq;
		SceUInt64 total = 0;

		int ret = emitUintFormParam(sendBody, httpReq, FORM_KEY_VERSION,
					    static_cast<unsigned int>(protoVer), 0);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		ret = npComm2HttpUriEscape(escaped, sizeof(escaped), username,
					   sceClibStrnlen(username, sizeof(escaped)), NULL);
		if (ret < 0)
			return ret;
		ret = emitStringFormParam(sendBody, httpReq, FORM_KEY_USERNAME, escaped, 1);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		ret = emitStringFormParam(sendBody, httpReq, FORM_KEY_CONSOLE_ID, consoleId, 1);
		if (ret < 0)
			return ret;
		total += static_cast<SceUInt64>(ret);

		if (pass == 0) {
			bodyLen = total;
			const char *prefix = NULL;
			const char *action = NULL;
			ret = npComm2ResolveEndpoint(serviceType, op, &prefix, &action);
			if (ret >= 0) {
				ret = npComm2HttpCreateRequest(httpTemplate, serviceType, op,
							       prefix, action, 0, httpConn,
							       outHttpReq, bodyLen);
			}
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

/* 0x812C0F3C */
int npComm2AddContext(SceKernelLwMutexWork *lock, ShellNpCommerce2Context *ctx,
		      SceUInt32 *outCtxId, int poolKind)
{
	int lockRet = sceKernelLockLwMutex(lock, 1, NULL);
	unsigned int samePoolCount = 0;

	for (ShellNpCommerce2Context *cur = s_contextList; cur != NULL; cur = cur->next) {
		if (cur->ctxPoolKind == poolKind)
			samePoolCount++;
	}

	unsigned int limit = (poolKind == 1) ? NP_COMM2_MAX_SHELL_CTX : 1u;
	int ret;
	if (samePoolCount >= limit) {
		ret = SHELL_NP_COMM2_ERR_CTX_MAX;
	} else {
		SceUInt32 candidate = (s_lastCtxId == 0xFFFFu) ? 1u : (s_lastCtxId + 1u);
		while (candidate != s_lastCtxId) {
			ShellNpCommerce2Context *cur = s_contextList;
			while (cur != NULL && cur->id != candidate)
				cur = cur->next;
			if (cur == NULL)
				break;
			candidate = (candidate == 0xFFFFu) ? 1u : (candidate + 1u);
		}
		if (candidate == s_lastCtxId) {
			ret = SHELL_NP_COMM2_ERR_CTX_ID_FULL;
		} else {
			ctx->ctxPoolKind = poolKind;
			ctx->id = candidate;
			*outCtxId = candidate;
			ctx->next = s_contextList;
			s_contextList = ctx;
			s_lastCtxId = candidate;
			ret = 0;
		}
	}

	if (lockRet == 0)
		sceKernelUnlockLwMutex(lock, 1);
	return ret;
}

/* 0x812C1032 */
int npComm2AcquireContext(SceKernelLwMutexWork *lock, SceUInt32 ctxId,
			  ShellNpCommerce2Context **outCtx, int rejectRemoved)
{
	int lockRet = sceKernelLockLwMutex(lock, 1, NULL);
	ShellNpCommerce2Context *cur = s_contextList;
	while (cur != NULL && cur->id != ctxId)
		cur = cur->next;

	int ret = 0;
	if (cur == NULL) {
		ret = SHELL_NP_COMM2_ERR_CTX_NOT_FOUND;
	} else if (rejectRemoved != 0 && (cur->flags & SHELL_NP_COMM2_CTX_FLAG_REMOVED) != 0) {
		ret = SHELL_NP_COMM2_ERR_CTX_NOT_FOUND;
	} else {
		cur->refCount++;
		*outCtx = cur;
	}

	if (lockRet == 0)
		sceKernelUnlockLwMutex(lock, 1);
	return ret;
}

/* 0x812C10B6 */
int npComm2ReleaseContext(SceKernelLwMutexWork *lock, ShellNpCommerce2Context *ctx)
{
	int lockRet = sceKernelLockLwMutex(lock, 1, NULL);
	ctx->refCount--;
	if ((ctx->flags & SHELL_NP_COMM2_CTX_FLAG_REMOVED) != 0 &&
	    ctx->refCount == 0 && ctx->requests == NULL) {
		if (ctx->httpTemplate != 0)
			npComm2HttpDeleteTemplate(ctx->httpTemplate, ctx->httpConnection);
		ctx->httpTemplate = 0;
		npCommerce2Free(ctx);
	}
	if (lockRet == 0)
		return sceKernelUnlockLwMutex(lock, 1);
	return lockRet;
}

/* 0x812C10FA */
int npComm2RemoveContext(SceKernelLwMutexWork *lock, SceUInt32 ctxId)
{
	int lockRet = sceKernelLockLwMutex(lock, 1, NULL);
	ShellNpCommerce2Context *cur = s_contextList;
	ShellNpCommerce2Context *prev = NULL;

	while (cur != NULL) {
		if (cur->id == ctxId) {
			if (prev != NULL)
				prev->next = cur->next;
			else
				s_contextList = cur->next;
			cur->next = NULL;
			break;
		}
		prev = cur;
		cur = cur->next;
	}

	int ret = 0;
	if (cur != NULL) {
		cur->flags |= SHELL_NP_COMM2_CTX_FLAG_REMOVED;
		if (cur->refCount == 0 && cur->requests == NULL) {
			if (cur->httpTemplate != 0)
				npComm2HttpDeleteTemplate(cur->httpTemplate, cur->httpConnection);
			cur->httpTemplate = 0;
			npCommerce2Free(cur);
		}
	} else {
		ret = SHELL_NP_COMM2_ERR_CTX_NOT_FOUND;
	}

	if (lockRet == 0)
		sceKernelUnlockLwMutex(lock, 1);
	return ret;
}

/* 0x812C1194 */
int npComm2HttpCreateTemplate(int *outTmpl, int *outConn)
{
	int tmpl = sceHttpCreateTemplate("commerce2/1.0", 2, 0);
	if (tmpl < 0)
		return tmpl;

	int ret = sceHttpSetCookieEnabled(tmpl, 0);
	if (ret < 0) {
		if (tmpl != 0)
			sceHttpDeleteTemplate(tmpl);
		return ret;
	}

	sceHttpsEnableOptionPrivate(tmpl, NP_COMM2_SSL_ENABLE_FLAGS);
	sceHttpsDisableOptionPrivate(tmpl, NP_COMM2_SSL_DISABLE_FLAGS);
	*outConn = 0;
	*outTmpl = tmpl;
	return 0;
}

/* 0x812C11EC */
int npComm2HttpDeleteTemplate(int tmplId, int connId)
{
	if (connId != 0)
		sceHttpDeleteConnection(connId);
	if (tmplId != 0)
		sceHttpDeleteTemplate(tmplId);
	return 0;
}

/* 0x812C1204 */
int npComm2HttpCreateRequest(int tmplId, SceUInt32 serviceType, SceUInt16 op,
			     const char *prefix, const char *action, int unusedFlag,
			     int *httpConn, int *outHttpReq, SceUInt64 contentLength)
{
	(void)serviceType;
	(void)op;
	(void)unusedFlag;

	char env[20];
	char host[64];
	char path[64];

	int ret = npComm2GetEnvironment(env, 17u);
	if (ret < 0)
		return ret;

	int conn = *httpConn;
	if (conn == 0) {
		sceClibSnprintf(host, sizeof(host), "%s.%s.%s", "native-vita", env,
				"ac.playstation.net");
		host[sizeof(host) - 1] = '\0';
		conn = sceHttpCreateConnection(tmplId, host, "https", 443u, 0);
		if (conn < 0)
			return conn;
		*httpConn = conn;
	}

	sceClibSnprintf(path, sizeof(path), "/%s/%s%s.%s", "native", prefix, action, "action");
	path[sizeof(path) - 1] = '\0';

	int httpReq = sceHttpCreateRequest(conn, SCE_HTTP_METHOD_POST, path, contentLength);
	if (httpReq < 0)
		return httpReq;

	for (unsigned int i = 0; i < 4u; ++i) {
		ret = sceHttpAddRequestHeader(httpReq, s_defaultHeaders[i].name,
					      s_defaultHeaders[i].value,
					      SCE_HTTP_HEADER_OVERWRITE);
		if (ret < 0) {
			sceHttpDeleteRequest(httpReq);
			return ret;
		}
	}

	if (sceKernelGetThreadId() == s_cookieThreadId && s_threadCookie[0] != '\0') {
		ret = sceHttpAddRequestHeader(httpReq, "Cookie", s_threadCookie,
					      SCE_HTTP_HEADER_OVERWRITE);
		if (ret < 0) {
			sceHttpDeleteRequest(httpReq);
			return ret;
		}
	}

	*outHttpReq = httpReq;
	return ret;
}

/* 0x812C1366 */
int npComm2HttpDeleteRequest(int httpReq)
{
	return sceHttpDeleteRequest(httpReq);
}

/* 0x812C136E */
int npComm2HttpSendRequest(int httpReq, const void *body, SceSize len)
{
	return sceHttpSendRequest(httpReq, body, len);
}

/* 0x812C1376 */
int npComm2HttpCheckStatus(int httpReq, int *nativeBuildVersion)
{
	int statusCode = 0;
	int ret = sceHttpGetStatusCode(httpReq, &statusCode);
	if (ret < 0)
		return ret;

	if (*nativeBuildVersion == 0) {
		char *headerBuf = NULL;
		SceSize headerSize = 0;
		const char *fieldValue = NULL;
		SceSize valueLen = 0;
		if (sceHttpGetAllResponseHeaders(httpReq, &headerBuf, &headerSize) >= 0 &&
		    sceHttpParseResponseHeader(headerBuf, headerSize, "X-NativeBuildVersion",
					       &fieldValue, &valueLen) >= 0) {
			if (sceClibStrnlen("XX.XX.XX.XX", 12u) <= valueLen) {
				int version = 0;
				for (const char *cursor = fieldValue; cursor <= fieldValue + 9;
				     cursor += 3) {
					int tens = (cursor[0] - '0') * 10;
					int ones = cursor[1] - '0';
					version = version * 100 + tens + ones;
				}
				*nativeBuildVersion = version;
			}
		}
	}

	if (statusCode == 503 || statusCode == 500)
		return SHELL_NP_COMM2_ERR_SERVER_MAINTENANCE;
	if (statusCode == 200)
		return 0;
	return SHELL_NP_COMM2_ERR_SERVER_UNKNOWN;
}

/* 0x812C1452 */
int npComm2HttpReadExact(int httpReq, char *buf, SceSize size)
{
	SceSize remaining = size;
	int total = 0;

	while (remaining != 0) {
		int n = sceHttpReadData(httpReq, buf, remaining);
		if (n < 0)
			return n;
		if (n == 0)
			break;
		remaining -= static_cast<SceSize>(n);
		buf += n;
		total += n;
	}
	return total;
}

/* 0x812C1480 */
int npComm2HttpReadBody(int httpReq, const char *base, char *dst, SceSize totalSize,
			int *outTruncated, int skipBytes)
{
	SceUInt64 contentLength = 0;
	int totalRead = 0;
	bool eofReached = false;

	*outTruncated = 0;
	sceHttpGetResponseContentLength(httpReq, &contentLength);
	if (static_cast<SceUInt32>(contentLength >> 32) != 0)
		return SHELL_NP_COMM2_ERR_CONTENT_TOO_LONG;

	SceSize capacity = totalSize - static_cast<SceSize>(dst - base);
	SceSize want = capacity;
	SceSize expected = 0;
	if (static_cast<SceUInt32>(contentLength) != 0) {
		expected = static_cast<SceUInt32>(contentLength) - static_cast<SceSize>(skipBytes);
		if (expected < want)
			want = expected;
	}

	while (want != 0) {
		int n = sceHttpReadData(httpReq, dst, want);
		if (n < 0)
			return n;
		if (n == 0) {
			eofReached = true;
			break;
		}
		want -= static_cast<SceSize>(n);
		dst += n;
		totalRead += n;
	}

	if (contentLength != 0) {
		if (expected > capacity)
			*outTruncated = 1;
		else if (static_cast<SceSize>(totalRead) != expected)
			return SHELL_NP_COMM2_ERR_INVALID_RESPONSE;
	} else if (!eofReached) {
		char probeByte = 0;
		int probe = sceHttpReadData(httpReq, &probeByte, 1u);
		if (probe < 0)
			return probe;
		if (probe != 0)
			*outTruncated = 1;
	}
	return totalRead;
}

/* 0x812C1598 */
int npComm2HttpAbortRequest(int httpReq)
{
	return sceHttpAbortRequest(httpReq);
}

/* 0x812C15A0 */
int npComm2HttpResetConnection(int tmplId, int *httpConn)
{
	(void)tmplId;
	if (*httpConn != 0)
		sceHttpDeleteConnection(*httpConn);
	*httpConn = 0;
	return 0;
}

/* 0x812C15B2 */
int npComm2HttpUriEscape(char *outBuf, SceSize outSize, const char *src,
			 SceSize srcLen, SceSize *required)
{
	(void)srcLen;
	return sceHttpUriEscape(outBuf, required, outSize, src);
}

/* 0x812C15C2 */
int npComm2HttpUpdateCookie(int tmplId, int connId, int httpReq, char *cookieBuf,
			    SceSize cookieBufSize)
{
	char *headerBuf = NULL;
	SceSize headerSize = 0;
	const char *fieldValue = NULL;
	SceSize valueLen = 0;

	if (sceHttpGetAllResponseHeaders(httpReq, &headerBuf, &headerSize) >= 0 &&
	    sceHttpParseResponseHeader(headerBuf, headerSize, "Set-Cookie",
				       &fieldValue, &valueLen) >= 0) {
		cookieBuf[0] = '\0';
		if (cookieBufSize > valueLen) {
			sceClibMemcpy_safe(cookieBuf, fieldValue, valueLen);
			cookieBuf[valueLen] = '\0';
			char *semi = sceClibStrchr(cookieBuf, ';');
			if (semi != NULL) {
				*semi = '\0';
				if (sceHttpAddRequestHeader(tmplId, "Cookie", cookieBuf,
							    SCE_HTTP_HEADER_OVERWRITE) >= 0) {
					sceHttpAddRequestHeader(connId, "Cookie", cookieBuf,
								SCE_HTTP_HEADER_OVERWRITE);
				}
			}
		}
	}
	return 0;
}

/* 0x812C1678 */
int npComm2HttpSetThreadCookie(SceUID threadId, const char *cookie)
{
	s_cookieThreadId = threadId;
	sceClibStrlcpy(s_threadCookie, cookie, sizeof(s_threadCookie));
	return 0;
}

/* 0x812C1698 */
int npComm2HttpClearThreadCookie(void)
{
	s_cookieThreadId = -1;
	s_threadCookie[0] = '\0';
	return 0;
}
