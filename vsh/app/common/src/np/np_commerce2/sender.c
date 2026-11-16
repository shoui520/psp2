/*
 * SceNpCommerce2: building and sending the store's requests. A request is an HTTPS request to
 * native-vita.<environment>.ac.playstation.net, to a path made of the service of the mode, the
 * operation's name and "action"; the operations that take arguments send them in the body as
 * form fields.
 */

#include "np_commerce2_internal.h"

extern char g_npEnvironment[17];

/* the services of the modes (1 to 4 are the store's) */
static const char * const g_modePrefixes[9] = {
	"", "store/", "store/", "store/", "store/", "reg/", "cam/", "upgrade/", "pwd/",
};

/* the operations' names, by the request type (the strings the original keeps as the library's own) */
static const char * const g_operationNames[114] = {
	(const char *)0x73,
	"crSess", "delSess", "getCat", "getProd",
	"srchProd", "getWal", "getDrm", "purPrev",
	"purch", "getCC", "topupCC", "vchrRedPrev",
	"vchrRed", "topEdy", "topEdyRes", "addCart",
	"remCart", "clearCart", "getCart", "getProv",
	"getPay", "getBill", "regCC", "getAddr",
	"getResAddr", "eventLog", "getCountries", "bindAccount",
	"startReg", "getSecQ", "setCred", "setLid",
	"updPass", "verifyOid", "setOid", "getName",
	"setName", "setResAddr", "setRrn", "getOptIn",
	"setOptIn", "authMaster", "getBillInfo", "setSubCtrl",
	"resetSubPass", "getWalOpt", "setWalOpt", "submitReg",
	"authenticate", "remPay", "getTranHist", "getTranDetail",
	"getSrvs", "getSrvDetail", "getSubscr", "getSubscrPlan",
	"changeSubscr", "cancelSubscr", "getCnslAct", "acptEula",
	"verifyDob", "verifySecAns", "sendPassResetMail", "verifyMasterBill",
	"getEpw", "crGuestSess", "resetPass", "getMasterBill",
	"sendConsentMail", "getMasterResAddr", "getProdList", "getSkuList",
	"getReward", "setSecQ", "submitUpgrade", "recomProds",
	"verifyBill", "regDevice", "deregDevice", "getProdIds",
	"crSessTicket", "getUsersFromCID", "getActCode", "setAccountLang",
	"getPivotForProd", "getProdForPivot", "cancelOrder", "checkAccountEula",
	"useEntlmt", "", "", "",
	"", "", "", "getEntState",
	"esnDuid", "crSessExpTicket", "", "sendEmailVer",
	"heRegistration", "getEntRev", "crSessByTickLite", "crSessExpTickLite",
	"crSessByAccessToken", "", "", "",
	"", "", "", "crSessByAccessTokenLite",
	"verifyAccountPass",
};

/* the headers every request carries: content type, the shell's credentials, media information, languages */
static const char * const g_requestHeaders[8] = {
	"Content-Type", "application/x-www-form-urlencoded",
	"Authorization", "Basic cjI0NjpHUG9FZmJkYQ==",
	"X-MediaInformation", "PSP2/480x272",
	"Accept-Language", "ja, en, fr, es, de, it, nl, pt, ru, ko, zh, ch, fi, sv, da, no, pl, tr",
};

/* the names of the body's form fields, with their lengths */
#define KEY(text)	{ sizeof(text) - 1, text }
const struct NpCommerce2QueryKey g_queryKeys[87] = {
	KEY("&"),
	KEY(","),
	KEY("version="),
	KEY("username="),
	KEY("password="),
	KEY("consoleId="),
	KEY("categoryId="),
	KEY("startPosition="),
	KEY("maxCountOfResults="),
	KEY("productId="),
	KEY("storeType="),
	KEY("queryKey="),
	KEY("queryValue="),
	KEY("contentSortMethod="),
	KEY("skuId="),
	KEY("skuIds="),
	KEY("titleId="),
	KEY("drmContentType="),
	KEY("quickPurchase="),
	KEY("topupAmount="),
	KEY("1="),
	KEY("2="),
	KEY("3="),
	KEY("edyReceiptId="),
	KEY("resultOfTopupFromEdy="),
	KEY("paymentType="),
	KEY("paymentMethodId="),
	KEY("holderName="),
	KEY("cardNumber="),
	KEY("startYear="),
	KEY("startMonth="),
	KEY("expireYear="),
	KEY("expireMonth="),
	KEY("securityCode="),
	KEY("issueNumber="),
	KEY("city="),
	KEY("province="),
	KEY("postalCode="),
	KEY("eventType="),
	KEY("id="),
	KEY("url="),
	KEY("country="),
	KEY("language="),
	KEY("yob="),
	KEY("mob="),
	KEY("dob="),
	KEY("loginName="),
	KEY("securityQuestion="),
	KEY("securityAnswer="),
	KEY("psHandle="),
	KEY("firstName="),
	KEY("lastName="),
	KEY("firstNameASCII="),
	KEY("lastNameASCII="),
	KEY("gender="),
	KEY("scexOptinFlag="),
	KEY("affiliatesOptinFlag="),
	KEY("subLoginName="),
	KEY("contentRating="),
	KEY("chatControl="),
	KEY("searchControl="),
	KEY("spendingLimitFlag="),
	KEY("spendingLimit="),
	KEY("autoDepositFlag="),
	KEY("commercePasswordFlag="),
	KEY("fromYear="),
	KEY("fromMonth="),
	KEY("fromDay="),
	KEY("toYear="),
	KEY("toMonth="),
	KEY("toDay="),
	KEY("timezone="),
	KEY("transactionId="),
	KEY("serviceId="),
	KEY("save="),
	KEY("rewardId="),
	KEY("rewardIds="),
	KEY("productIds="),
	KEY("recommendationType="),
	KEY("contentId="),
	KEY("fileId="),
	KEY("autoDownloadFlag="),
	KEY("orderItemId="),
	KEY("serviceEntity="),
	KEY("phoneNumber="),
	KEY("cpfNumber="),
	KEY("currentPassword="),
};

/* the name of the environment the library runs against, copied into a buffer */
__attribute__((noinline))
int npCommerce2GetEnvironmentName(char *buf, SceSize len)
{
	sceClibStrncpy(buf, g_npEnvironment, len);
	buf[len - 1] = '\0';
	return 0;
}

/* the service prefix and the name of the operation of a type in a mode */
__attribute__((noinline))
int npCommerce2GetRequestPath(SceUInt32 mode, SceUInt16 type, const char **prefix, const char **name)
{
	char environment[20];
	int index = type;
	int ret;

	if (type < 0x72) {
		if (mode != 0 && mode < 9) {
			ret = npCommerce2GetEnvironmentName(environment, 17);
			if (ret >= 0) {
				*prefix = g_modePrefixes[mode];
				if (mode == 7) {
					if (index > 31) {
						if (index == 48)
							index = 75;
					} else if (index > 30) {
						index = 74;
					}
				}
				*name = g_operationNames[index];
			}
		} else {
			ret = NP_COMMERCE2_ERROR_INVALID_MODE;
		}
	} else {
		ret = NP_COMMERCE2_ERROR_INVALID_TYPE;
	}
	return ret;
}

/* makes the connection of the template (when it has none) and the request to the path, with the headers */
__attribute__((noinline))
int npCommerce2CreateHttpRequest(int httpTemplate, int mode, int type, const char *prefix, const char *name,
				 int unused, int *httpConnection, int *httpRequest, SceULong64 contentLength)
{
	char environment[20];
	char server[64];
	char path[64];
	int connection = *httpConnection;
	int request;
	int i;
	int ret;

	ret = npCommerce2GetEnvironmentName(environment, 17);
	if (ret >= 0) {
		if (connection == 0) {
			sceClibSnprintf(server, 64, "%s.%s.%s", "native-vita", environment, "ac.playstation.net");
			server[63] = '\0';
			ret = sceHttpCreateConnection(httpTemplate, server, "https", 443, 1);
			if (ret < 0)
				return ret;
			*httpConnection = ret;
			connection = ret;
		}
		sceClibSnprintf(path, 64, "/%s/%s%s.%s", "native", prefix, name, "action");
		path[63] = '\0';
		ret = sceHttpCreateRequest(connection, 1, path, contentLength);
		if (ret >= 0) {
			request = ret;
			for (i = 0; i < 4; i++) {
				ret = sceHttpAddRequestHeader(request, g_requestHeaders[2 * i],
							      g_requestHeaders[2 * i + 1], 0);
				if (ret < 0)
					break;
			}
			if (i >= 4) {
				*httpRequest = request;
				return ret;
			}
			if (request != 0)
				sceHttpDeleteRequest(request);
		}
	}
	return ret;
}

__attribute__((noinline))
int npCommerce2SendHttpRequest(int httpRequest, const void *data, SceSize size)
{
	return sceHttpSendRequest(httpRequest, data, size);
}

/* a request that takes no arguments: made and sent with an empty body */
#define NP_COMMERCE2_SEND_PLAIN(name)								\
int name(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest)		\
{												\
	const char *prefix = NULL;								\
	const char *opName = NULL;								\
	int ret;										\
												\
	ret = npCommerce2GetRequestPath(mode, type, &prefix, &opName);				\
	if (ret >= 0)										\
		ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, opName, 0,	\
						   httpConnection, httpRequest, 0);		\
	if (ret >= 0) {										\
		ret = npCommerce2SendHttpRequest(*httpRequest, NULL, 0);			\
		if (ret >= 0)									\
			ret = 0;								\
	}											\
	return ret;										\
}

NP_COMMERCE2_SEND_PLAIN(npCommerce2Send_8101950A)
NP_COMMERCE2_SEND_PLAIN(npCommerce2Send_81019592)
NP_COMMERCE2_SEND_PLAIN(npCommerce2Send_810197A6)
NP_COMMERCE2_SEND_PLAIN(npCommerce2Send_8101982E)
NP_COMMERCE2_SEND_PLAIN(npCommerce2Send_81019BEC)
NP_COMMERCE2_SEND_PLAIN(npCommerce2Send_8101A53A)
NP_COMMERCE2_SEND_PLAIN(npCommerce2Send_8101AFE4)
NP_COMMERCE2_SEND_PLAIN(npCommerce2Send_8101B684)
NP_COMMERCE2_SEND_PLAIN(npCommerce2Send_8101BBB8)
NP_COMMERCE2_SEND_PLAIN(npCommerce2Send_8101C224)
NP_COMMERCE2_SEND_PLAIN(npCommerce2Send_8101C3E2)
NP_COMMERCE2_SEND_PLAIN(npCommerce2Send_8101CF56)
NP_COMMERCE2_SEND_PLAIN(npCommerce2Send_81018C7A)
