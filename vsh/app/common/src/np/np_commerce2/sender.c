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

/* the names of the fields searches may name, by the kind of search (native, game, comic) */
static const char * const g_nativeSearchFields[7] = {
	"nativeText", "genre", "actor", "writer", "nativeTitle", "nativeDirectorProducer", "nativeDescription",
};
static const char * const g_gameSearchFields[7] = {
	"gameAll", "XXX", "XXX", "XXX", "gameTitle", "XXX", "gameDescription",
};
static const char * const g_comicSearchFields[7] = {
	"comicAll", "XXX", "XXX", "XXX", "comicTitle", "XXX", "XXX",
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

/* the URI escaping of a text (the original takes the length of the text too and does not use it) */
__attribute__((noinline))
int npCommerce2UriEscape(char *out, SceSize prepare, const char *in, int length, SceSize *require)
{
	return sceHttpUriEscape(out, require, prepare, in);
}

/* adds a cookie to the requests of the template and the connection */
int npCommerce2AddCookie(int httpTemplate, int httpConnection, const char *cookie)
{
	if (sceHttpAddRequestHeader(httpTemplate, "Cookie", cookie, 0) >= 0)
		sceHttpAddRequestHeader(httpConnection, "Cookie", cookie, 0);
	return 0;
}

/* one field of the body, "<key><text>" after an "&" unless it is the first: the number of bytes it takes
 * (when not sending), or sends it to the request */
__attribute__((noinline))
int npCommerce2WriteTextField(int sending, int httpRequest, int key, const char *text, int notFirst)
{
	int ret;

	if (sending != 1)
		return g_queryKeys[key].length + sceClibStrnlen(text, 0x400) + (notFirst == 1);
	if (notFirst != 1 || (ret = npCommerce2SendHttpRequest(httpRequest, g_queryKeys[0].text, g_queryKeys[0].length)) >= 0) {
		ret = npCommerce2SendHttpRequest(httpRequest, g_queryKeys[key].text, g_queryKeys[key].length);
		if (ret >= 0) {
			SceSize length = sceClibStrnlen(text, 0x400);

			return npCommerce2SendHttpRequest(httpRequest, text, length);
		}
	}
	return ret;
}

/* the same for a field whose value is a number */
__attribute__((noinline))
int npCommerce2WriteNumberField(int sending, int httpRequest, int key, SceUInt32 value, int notFirst)
{
	char text[16];
	int size;
	int ret;

	if (sending == 1) {
		if (notFirst != 1 || (ret = npCommerce2SendHttpRequest(httpRequest, g_queryKeys[0].text, g_queryKeys[0].length)) >= 0) {
			ret = npCommerce2SendHttpRequest(httpRequest, g_queryKeys[key].text, g_queryKeys[key].length);
			if (ret >= 0) {
				if (sceClibSnprintf(text, 16, "%u", value) < 16) {
					size = sceClibStrnlen(text, 0x400);
					ret = npCommerce2SendHttpRequest(httpRequest, text, size);
				} else {
					ret = SCE_NP_COMMERCE2_ERROR_REQUEST_BUF_TOO_SMALL;
				}
			}
		}
	} else {
		int base = (notFirst == 1) + g_queryKeys[key].length;
		int written = sceClibSnprintf(text, 16, "%u", value);

		ret = written + base;
		if (written >= 16)
			ret = SCE_NP_COMMERCE2_ERROR_REQUEST_BUF_TOO_SMALL;
	}
	return ret;
}

/* a request with the SKU ID as its one field */
int npCommerce2Send_8101940E(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char *text)
{
	char escaped[0x304];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int handle;
	int ret;

	for (;;) {
		sending = pass == 1;
		handle = *httpRequest;
		ret = npCommerce2UriEscape(escaped, 0x301, text, sceClibStrnlen(text, 0x301), NULL);
		if (ret >= 0) {
			ret = npCommerce2WriteTextField(sending, handle, 14, escaped, 0);
			if (ret >= 0) {
				if (!sending)
					length = (SceULong64)(SceInt64)ret;
				ret = 0;
			}
		}
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with the payment type as its one field, only for argument 0 */
int npCommerce2Send_810198B6(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     int argument)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	for (;;) {
		sending = pass != 0;
		if (argument != 0) {
			ret = NP_COMMERCE2_ERROR_UNSUPPORTED_ARGUMENT;
		} else {
			ret = npCommerce2WriteTextField(sending, *httpRequest, 25, "CC", 0);
			if (ret >= 0) {
				if (!sending)
					length = (SceULong64)(SceInt64)ret;
				ret = 0;
			}
		}
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with an optional text and two flags as fields */
int npCommerce2Send_8101961A(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char *text, int flagA, int flagB)
{
	char escaped[0x304];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	SceULong64 total;
	SceULong64 first;
	int pass = 0;
	int sending;
	int notFirst;
	int handle;
	int ret;

	do {
		sending = pass != 0;
		notFirst = 0;
		handle = *httpRequest;
		first = 0;
		if (text != NULL && *text != '\0') {
			ret = npCommerce2UriEscape(escaped, 0x301, text, sceClibStrnlen(text, 0x301), NULL);
			if (ret < 0)
				goto done;
			ret = npCommerce2WriteTextField(sending, handle, 4, escaped, notFirst);
			if (ret < 0)
				goto done;
			first = (SceULong64)(SceInt64)ret;
			notFirst = 1;
		}
		ret = npCommerce2WriteTextField(sending, handle, 18, flagA == 1 ? "true" : "false", notFirst);
		if (ret >= 0) {
			total = first + (SceULong64)(SceInt64)ret;
			ret = npCommerce2WriteTextField(sending, handle, 63, flagB == 1 ? "true" : "false", 1);
			if (ret >= 0) {
				if (sending != 1)
					length = total + (SceULong64)(SceInt64)ret;
				ret = 0;
			}
		}
done:
		if (ret < 0)
			return ret;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				return ret;
		}
		pass++;
	} while (pass < 2);
	return 0;
}

/* a request with the Edy receipt ID and its result as fields */
int npCommerce2Send_8101A688(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char *text, int result)
{
	char escaped[0x304];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	SceULong64 first;
	int pass = 0;
	int sending;
	int handle;
	int ret;

	for (;;) {
		sending = pass != 0;
		handle = *httpRequest;
		ret = npCommerce2UriEscape(escaped, 0x301, text, sceClibStrnlen(text, 0x301), NULL);
		if (ret >= 0) {
			ret = npCommerce2WriteTextField(sending, handle, 23, escaped, 0);
			if (ret >= 0) {
				first = (SceULong64)(SceInt64)ret;
				ret = npCommerce2WriteTextField(sending, handle, 24, result == 1 ? "OK" : "NG", 1);
				if (ret >= 0) {
					if (sending != 1)
						length = first + (SceULong64)(SceInt64)ret;
					ret = 0;
				}
			}
		}
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* one field of the body with prefixes before its name ("account.", "address." ...) */
__attribute__((noinline))
int npCommerce2WritePrefixedField(int sending, int httpRequest, const char * const *prefixes, SceUInt32 count,
				  int key, const char *text, int length, int notFirst)
{
	SceUInt32 i;
	int ret;
	int size;

	if (sending == 1) {
		if (notFirst != 1 || (ret = npCommerce2SendHttpRequest(httpRequest, g_queryKeys[0].text,
								      g_queryKeys[0].length)) >= 0) {
			i = 0;
			if (count != 0) {
				for (;;) {
					const char *prefix = prefixes[i];

					if (prefix != NULL) {
						ret = npCommerce2SendHttpRequest(httpRequest, prefix,
										 sceClibStrnlen(prefix, 0x400));
						if (ret < 0)
							return ret;
					}
					if (++i >= count)
						break;
				}
			}
			ret = npCommerce2SendHttpRequest(httpRequest, g_queryKeys[key].text, g_queryKeys[key].length);
			if (ret >= 0)
				return npCommerce2SendHttpRequest(httpRequest, text, length);
		}
		return ret;
	}
	size = notFirst == 1;
	for (i = count; i != 0; i--)
		size += sceClibStrnlen(*prefixes++, 0x400);
	return g_queryKeys[key].length + length + size;
}

/* the fields of an address (and, when asked, the "save" flag), "creditCard." or "account." and
 * "address." in front of their names; gives their size in total (when not sending) */
#define ADDRESS_TEXT(field, key, count)							\
	do {											\
		const char *value = address->field;						\
		if (value != NULL && *value != '\0') {						\
			size = sceClibStrnlen(value, 0x400);					\
			ret = npCommerce2WritePrefixedField(sending, httpRequest, prefixes, count, key, value, size, notFirst); \
			if (ret < 0)								\
				return ret;							\
			sum += ret;								\
			notFirst = 1;								\
		}										\
	} while (0)

#define ADDRESS_NUMBER(field, key)								\
	do {											\
		if (address->field != 0) {							\
			ret = SCE_NP_COMMERCE2_ERROR_REQUEST_BUF_TOO_SMALL;			\
			if (sceClibSnprintf(number, 16, "%u", address->field) < 16) {		\
				size = sceClibStrnlen(number, 0x400);				\
				ret = npCommerce2WritePrefixedField(sending, httpRequest, prefixes, 1, key, number, size, notFirst); \
			}									\
			if (ret < 0)								\
				return ret;							\
			sum += ret;								\
			notFirst = 1;								\
		}										\
	} while (0)

#define ADDRESS_ESCAPED(field, lengthField, key, count)					\
	do {											\
		const char *value = address->field;						\
		if (value != NULL && *value != '\0' && address->lengthField != 0) {		\
			ret = npCommerce2UriEscape(escaped, 0x301, value, address->lengthField, NULL); \
			if (ret < 0)								\
				return ret;							\
			size = sceClibStrnlen(escaped, 0x400);					\
			ret = npCommerce2WritePrefixedField(sending, httpRequest, prefixes, count, key, escaped, size, notFirst); \
			if (ret < 0)								\
				return ret;							\
			sum += ret;								\
			notFirst = 1;								\
		}										\
	} while (0)

int npCommerce2WriteAddressFields(int sending, int httpRequest, SceUInt64 *total, SceUInt32 kind,
				  const NpCommerce2Address *address, int hasFlag, int flag)
{
	const char *prefixes[8];
	char escaped[0x304];
	char number[16];
	SceUInt64 sum = 0;
	int notFirst = 0;
	int size;
	int ret;

	sceClibMemset(prefixes, 0, 0x20);
	if (kind != 0) {
		if (kind <= 1)
			prefixes[0] = "account.";
	} else {
		prefixes[0] = "creditCard.";
	}
	prefixes[1] = "address.";
	prefixes[2] = "address";
	{
		const char *value = address->text0;

		if (value != NULL && *value != '\0' && address->length0 != 0) {
			int length = address->length0;

			if (value[length - 1] == '\0')
				length--;
			ret = npCommerce2WritePrefixedField(sending, httpRequest, prefixes, 1, 26, value, length, 0);
			if (ret < 0)
				return ret;
			sum = (SceULong64)(SceInt64)ret;
			notFirst = 1;
		}
	}
	ADDRESS_ESCAPED(text1, length1, 27, 1);
	ADDRESS_TEXT(text2, 28, 1);
	ADDRESS_NUMBER(number0, 29);
	ADDRESS_NUMBER(number1, 30);
	ADDRESS_NUMBER(number2, 31);
	ADDRESS_NUMBER(number3, 32);
	ADDRESS_TEXT(text3, 33, 1);
	ADDRESS_TEXT(text4, 34, 1);
	ADDRESS_TEXT(text5, 84, 1);
	ADDRESS_TEXT(text6, 85, 1);
	ADDRESS_ESCAPED(text7, length7, 20, 3);
	ADDRESS_ESCAPED(text8, length8, 21, 3);
	ADDRESS_ESCAPED(text9, length9, 22, 3);
	ADDRESS_ESCAPED(text10, length10, 35, 2);
	ADDRESS_ESCAPED(text11, length11, 36, 2);
	ADDRESS_ESCAPED(text12, length12, 37, 2);
	if (hasFlag == 1) {
		ret = npCommerce2WriteTextField(sending, httpRequest, 74, flag == 1 ? "true" : "false", notFirst);
		if (ret < 0)
			return ret;
		sum += (SceULong64)(SceInt64)ret;
	}
	if (sending != 1)
		*total = sum;
	return 0;
}

/* a request with the postal code as its one field */
int npCommerce2Send_8101A43E(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, const char *text)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	for (;;) {
		sending = pass == 1;
		{
			char escaped[0x304];
			int handle = *httpRequest;

			ret = npCommerce2UriEscape(escaped, 0x301, text, sceClibStrnlen(text, 0x301), NULL);
			if (ret >= 0) {
				ret = npCommerce2WriteTextField(sending, handle, 37, escaped, 0);
				if (ret >= 0) {
					if (!sending)
						length = (SceULong64)(SceInt64)ret;
					ret = 0;
				}
			}
		}
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with the top-up amount as its one field */
int npCommerce2Send_8101A5C2(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, SceUInt32 number)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	for (;;) {
		sending = pass == 1;
		ret = npCommerce2WriteNumberField(sending, *httpRequest, 19, number, 0);
		if (ret >= 0) {
			if (!sending)
				length = (SceULong64)(SceInt64)ret;
			ret = 0;
		}
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with the login name as its one field */
int npCommerce2Send_8101B2A8(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, const char *text)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	for (;;) {
		sending = pass == 1;
		{
			char escaped[0x304];
			const char *prefixes[2];
			int handle = *httpRequest;

			sceClibMemset(prefixes, 0, 8);
			prefixes[0] = "account.";
			ret = npCommerce2UriEscape(escaped, 0x301, text, sceClibStrnlen(text, 0x301), NULL);
			if (ret >= 0) {
				ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 46, escaped,
								    sceClibStrnlen(escaped, 0x400), 0);
				if (ret >= 0) {
					if (!sending)
						length = (SceULong64)(SceInt64)ret;
					ret = 0;
				}
			}
		}
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with the PS handle as its one field */
int npCommerce2Send_8101B560(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, const char *text)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	for (;;) {
		sending = pass == 1;
		{
			char escaped[0x304];
			const char *prefixes[2];
			int handle = *httpRequest;

			sceClibMemset(prefixes, 0, 8);
			prefixes[0] = "account.";
			ret = npCommerce2UriEscape(escaped, 0x301, text, sceClibStrnlen(text, 0x301), NULL);
			if (ret >= 0) {
				ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 49, escaped,
								    sceClibStrnlen(escaped, 0x400), 0);
				if (ret >= 0) {
					if (!sending)
						length = (SceULong64)(SceInt64)ret;
					ret = 0;
				}
			}
		}
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with a credit card address as its fields */
int npCommerce2Send_8101A378(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, const NpCommerce2Address *address)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	for (;;) {
		sending = pass != 0;
		ret = npCommerce2WriteAddressFields(sending, *httpRequest, &length, 0, address, 0, 0);
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with an account address as its fields */
int npCommerce2Send_8101B9B2(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, const NpCommerce2Address *address)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	for (;;) {
		sending = pass != 0;
		ret = npCommerce2WriteAddressFields(sending, *httpRequest, &length, 1, address, 0, 0);
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request logging an event: the mode, the event type, an ID and an optional URL */
int npCommerce2Send_8101A9D6(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int eventType, const char *id, const char *url)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	for (;;) {
		sending = pass != 0;
		{
			char number[4];
			int handle = *httpRequest;
			SceULong64 sum;

			sceClibSnprintf(number, 4, "%u", mode);
			ret = npCommerce2WriteTextField(sending, handle, 10, number, 0);
			if (ret >= 0) {
				sum = (SceULong64)(SceInt64)ret;
				sceClibSnprintf(number, 4, "%u", eventType);
				ret = npCommerce2WriteTextField(sending, handle, 38, number, 1);
				if (ret >= 0) {
					sum += (SceULong64)(SceInt64)ret;
					ret = npCommerce2WriteTextField(sending, handle, 39, id, 1);
					if (ret >= 0) {
						sum += (SceULong64)(SceInt64)ret;
						if (url != NULL && *url != '\0') {
							ret = npCommerce2WriteTextField(sending, handle, 40, url, 1);
							if (ret < 0)
								goto done;
							sum += (SceULong64)(SceInt64)ret;
						}
						if (sending != 1)
							length = sum;
						ret = 0;
					}
				}
			}
done:
			;
		}
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with the Korean ID and a second text as fields */
int npCommerce2Send_8101BA78(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, const char *koreanId, const char *text)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	for (;;) {
		sending = pass != 0;
		{
			const char *prefixes[8];
			int handle = *httpRequest;
			SceULong64 first;

			sceClibMemset(prefixes, 0, 0x20);
			prefixes[0] = "koreanId";
			ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 20, koreanId,
							    sceClibStrnlen(koreanId, 0x400), 0);
			if (ret >= 0) {
				first = (SceULong64)(SceInt64)ret;
				ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 21, text,
								    sceClibStrnlen(text, 0x400), 1);
				if (ret >= 0) {
					if (sending != 1)
						length = first + (SceULong64)(SceInt64)ret;
					ret = 0;
				}
			}
		}
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with two optional three-state flags (the content and chat controls) */
int npCommerce2Send_8101BC40(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int flagA, int flagB)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	do {
		sending = pass != 0;
		{
			SceULong64 sum = 0;
			int notFirst = 0;
			int handle = *httpRequest;

			ret = 0;
			if (flagA != 0) {
				ret = npCommerce2WriteTextField(sending, handle, 55, flagA == 2 ? "true" : "false", 0);
				if (ret < 0)
					goto done;
				sum = (SceULong64)(SceInt64)ret;
				notFirst = 1;
			}
			if (flagB != 0) {
				ret = npCommerce2WriteTextField(sending, handle, 56, flagB == 2 ? "true" : "false", notFirst);
				if (ret < 0)
					goto done;
				sum += (SceULong64)(SceInt64)ret;
			}
			if (!sending)
				length = sum;
			ret = 0;
done:
			;
		}
		if (ret < 0)
			return ret;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				return ret;
		}
		pass++;
	} while (pass < 2);
	return 0;
}

/* a request with two optional three-state flags (the deposit and password flags) */
int npCommerce2Send_8101C2AC(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int flagA, int flagB)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	do {
		sending = pass != 0;
		{
			SceULong64 sum = 0;
			int notFirst = 0;
			int handle = *httpRequest;

			ret = 0;
			if (flagA != 0) {
				ret = npCommerce2WriteTextField(sending, handle, 63, flagA == 2 ? "true" : "false", 0);
				if (ret < 0)
					goto done;
				sum = (SceULong64)(SceInt64)ret;
				notFirst = 1;
			}
			if (flagB != 0) {
				ret = npCommerce2WriteTextField(sending, handle, 64, flagB == 2 ? "true" : "false", notFirst);
				if (ret < 0)
					goto done;
				sum += (SceULong64)(SceInt64)ret;
			}
			if (!sending)
				length = sum;
			ret = 0;
done:
			;
		}
		if (ret < 0)
			return ret;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				return ret;
		}
		pass++;
	} while (pass < 2);
	return 0;
}

/* a request with the login name and the password as fields */
int npCommerce2Send_8101BD76(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, const char *loginName, const char *password)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	do {
		sending = pass != 0;
		{
			char escaped[0x304];
			int handle = *httpRequest;
			SceULong64 first;

			ret = npCommerce2UriEscape(escaped, 0x301, loginName, sceClibStrnlen(loginName, 0x301), NULL);
			if (ret >= 0) {
				ret = npCommerce2WriteTextField(sending, handle, 3, escaped, 0);
				if (ret >= 0) {
					first = (SceULong64)(SceInt64)ret;
					ret = npCommerce2UriEscape(escaped, 0x301, password,
								   sceClibStrnlen(password, 0x301), NULL);
					if (ret >= 0) {
						ret = npCommerce2WriteTextField(sending, handle, 4, escaped, 1);
						if (ret >= 0) {
							if (sending != 1)
								length = first + (SceULong64)(SceInt64)ret;
							ret = 0;
						}
					}
				}
			}
		}
		if (ret < 0)
			return ret;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				return ret;
		}
		pass++;
	} while (pass < 2);
	return 0;
}

/* a 64-bit number as text, in decimal or as 16 hexadecimal digits */
__attribute__((noinline))
int npCommerce2FormatU64(char *buf, SceSize size, SceUInt32 high, SceUInt32 low, int decimal)
{
	SceUInt64 value = ((SceUInt64)high << 32) | low;

	if (decimal == 1)
		return sceClibSnprintf(buf, size, "%llu", value);
	return sceClibSnprintf(buf, size, "%16llx", value);
}

/* a request with a 64-bit transaction number as its field */
int npCommerce2Send_8101C818(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, int unused, SceUInt32 high, SceUInt32 low)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	do {
		sending = pass != 0;
		{
			char number[32];

			npCommerce2FormatU64(number, 32, high, low, 1);
			ret = npCommerce2WriteTextField(sending, *httpRequest, 72, number, 0);
			if (ret >= 0) {
				if (!sending)
					length = (SceULong64)(SceInt64)ret;
				ret = 0;
			}
		}
		if (ret < 0)
			return ret;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				return ret;
		}
		pass++;
	} while (pass < 2);
	return 0;
}

/* a request with a start position and a count as fields */
int npCommerce2Send_8101C8F4(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, SceUInt32 startPosition, SceUInt32 maxCount)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	do {
		sending = pass != 0;
		{
			int handle = *httpRequest;
			SceULong64 first;

			ret = npCommerce2WriteNumberField(sending, handle, 7, startPosition, 0);
			if (ret >= 0) {
				first = (SceULong64)(SceInt64)ret;
				ret = npCommerce2WriteNumberField(sending, handle, 8, maxCount, 1);
				if (ret >= 0) {
					if (sending != 1)
						length = first + (SceULong64)(SceInt64)ret;
					ret = 0;
				}
			}
		}
		if (ret < 0)
			return ret;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				return ret;
		}
		pass++;
	} while (pass < 2);
	return 0;
}

/* a request with a service ID, a start position and a count as fields */
int npCommerce2Send_8101C9F2(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, const char *text, SceUInt32 startPosition, SceUInt32 maxCount)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	do {
		sending = pass != 0;
		{
			char escaped[0x304];
			int handle = *httpRequest;
			SceULong64 first;
			SceULong64 second;

			ret = npCommerce2UriEscape(escaped, 0x301, text, sceClibStrnlen(text, 0x301), NULL);
			if (ret >= 0) {
				ret = npCommerce2WriteTextField(sending, handle, 73, escaped, 0);
				if (ret >= 0) {
					first = (SceULong64)(SceInt64)ret;
					ret = npCommerce2WriteNumberField(sending, handle, 7, startPosition, 1);
					if (ret >= 0) {
						second = first + (SceULong64)(SceInt64)ret;
						ret = npCommerce2WriteNumberField(sending, handle, 8, maxCount, 1);
						if (ret >= 0) {
							if (sending != 1)
								length = second + (SceULong64)(SceInt64)ret;
							ret = 0;
						}
					}
				}
			}
		}
		if (ret < 0)
			return ret;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				return ret;
		}
		pass++;
	} while (pass < 2);
	return 0;
}

/* a request with a service ID, a start position and a count as fields */
int npCommerce2Send_8101CB3C(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, const char *text, SceUInt32 startPosition, SceUInt32 maxCount)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	do {
		sending = pass != 0;
		{
			char escaped[0x304];
			int handle = *httpRequest;
			SceULong64 first;
			SceULong64 second;

			ret = npCommerce2UriEscape(escaped, 0x301, text, sceClibStrnlen(text, 0x301), NULL);
			if (ret >= 0) {
				ret = npCommerce2WriteTextField(sending, handle, 73, escaped, 0);
				if (ret >= 0) {
					first = (SceULong64)(SceInt64)ret;
					ret = npCommerce2WriteNumberField(sending, handle, 7, startPosition, 1);
					if (ret >= 0) {
						second = first + (SceULong64)(SceInt64)ret;
						ret = npCommerce2WriteNumberField(sending, handle, 8, maxCount, 1);
						if (ret >= 0) {
							if (sending != 1)
								length = second + (SceULong64)(SceInt64)ret;
							ret = 0;
						}
					}
				}
			}
		}
		if (ret < 0)
			return ret;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				return ret;
		}
		pass++;
	} while (pass < 2);
	return 0;
}

/* a request with an address and the save flag */
int npCommerce2Send_8101CE02(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest, const NpCommerce2Address *address, int flag)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	do {
		sending = pass != 0;
		ret = npCommerce2WriteAddressFields(sending, *httpRequest, &length, 0, address, 1, flag);
		if (ret < 0)
			return ret;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				return ret;
		}
		pass++;
	} while (pass < 2);
	return 0;
}

/* a field whose value is a list of texts, written with commas between (all the first text when repeatFirst) */
__attribute__((noinline))
int npCommerce2WriteListField(int sending, int httpRequest, int key, const char * const *list, SceUInt32 count,
			      char *buf, SceSize maxLength, int repeatFirst, int notFirst)
{
	const char * const *entry;
	const char *item;
	SceUInt32 i;
	int size;
	int ret;

	if (sending == 1) {
		if (notFirst != 1 || (ret = npCommerce2SendHttpRequest(httpRequest, g_queryKeys[0].text,
								      g_queryKeys[0].length)) >= 0) {
			ret = npCommerce2SendHttpRequest(httpRequest, g_queryKeys[key].text, g_queryKeys[key].length);
			if (ret >= 0) {
				i = 0;
				if (count != 0) {
					entry = list;
					do {
						if (i != 0) {
							ret = npCommerce2SendHttpRequest(httpRequest, g_queryKeys[1].text,
											 g_queryKeys[1].length);
							if (ret < 0)
								break;
						}
						item = repeatFirst == 1 ? *list : *entry;
						if (item != NULL && *item != '\0') {
							ret = npCommerce2UriEscape(buf, maxLength, item,
										   sceClibStrnlen(item, maxLength), NULL);
							if (ret < 0)
								break;
							ret = npCommerce2SendHttpRequest(httpRequest, buf,
											 sceClibStrnlen(buf, 0x400));
							if (ret < 0)
								break;
						}
						i++;
						entry++;
					} while (i < count);
				}
			}
		}
		return ret;
	}
	size = (notFirst == 1) + g_queryKeys[key].length;
	i = 0;
	if (count != 0) {
		entry = list;
		do {
			if (i != 0)
				size++;
			item = repeatFirst == 1 ? *list : *entry;
			if (item != NULL && *item != '\0') {
				ret = npCommerce2UriEscape(buf, maxLength, item, sceClibStrnlen(item, maxLength), NULL);
				if (ret < 0)
					return ret;
				size += sceClibStrnlen(buf, maxLength);
			}
			i++;
			entry++;
		} while (i < count);
	}
	return size;
}

NP_COMMERCE2_SEND_PLAIN(npCommerce2Send_8101CECC)

/* a request with an address as its fields */
int npCommerce2Send_8101D2B0(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const NpCommerce2Address *address)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	for (;;) {
		sending = pass != 0;
		ret = npCommerce2WriteAddressFields(sending, *httpRequest, &length, 0, address, 0, 0);
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with the mode and a list of IDs as fields */
int npCommerce2Send_8101CFDE(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char * const *list, SceUInt32 count)
{
	char text[0x304];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int handle;
	int ret;
	SceULong64 first;

	for (;;) {
		sending = pass != 0;
		handle = *httpRequest;
		sceClibSnprintf(text, 0x301, "%u", mode);
		ret = npCommerce2WriteTextField(sending, handle, 10, text, 0);
		if (ret >= 0) {
			first = (SceULong64)(SceInt64)ret;
			ret = npCommerce2WriteListField(sending, handle, 77, list, count, text, 0x301, 0, 1);
			if (ret >= 0) {
				if (sending != 1)
					length = first + (SceULong64)(SceInt64)ret;
				ret = 0;
			}
		}
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with a content ID, or else a file ID, as its field */
int npCommerce2Send_8101D376(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char *contentId, const char *fileId)
{
	char escaped[0x304];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int handle;
	int ret;

	do {
		sending = pass != 0;
		handle = *httpRequest;
		if (contentId != NULL) {
			ret = npCommerce2UriEscape(escaped, 0x301, contentId, sceClibStrnlen(contentId, 0x301), NULL);
			if (ret < 0)
				return ret;
			ret = npCommerce2WriteTextField(sending, handle, 79, escaped, 0);
			if (ret < 0)
				return ret;
		} else {
			ret = npCommerce2UriEscape(escaped, 0x301, fileId, sceClibStrnlen(fileId, 0x301), NULL);
			if (ret < 0)
				return ret;
			ret = npCommerce2WriteTextField(sending, handle, 80, escaped, 0);
			if (ret < 0)
				return ret;
		}
		if (!sending)
			length = (SceULong64)(SceInt64)ret;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				return ret;
		}
		pass++;
	} while (pass < 2);
	return 0;
}

/* a request with the mode (or 1 when rewards are sent), the category, optional reward ID and a range */
int npCommerce2Send_81018D02(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char *categoryId, const char * const *rewardIds, int hasRewards,
			     SceUInt32 startPosition, SceUInt32 maxCount)
{
	char text[0x304];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	SceULong64 sum;
	int pass = 0;
	int sending;
	int handle;
	int ret;

	for (;;) {
		sending = pass != 0;
		handle = *httpRequest;
		if (rewardIds != NULL && hasRewards != 0)
			sceClibSnprintf(text, 0x301, "%u", 1);
		else
			sceClibSnprintf(text, 0x301, "%u", mode);
		ret = npCommerce2WriteTextField(sending, handle, 10, text, 0);
		if (ret >= 0) {
			sum = (SceULong64)(SceInt64)ret;
			ret = npCommerce2UriEscape(text, 0x301, categoryId, sceClibStrnlen(categoryId, 0x301), NULL);
			if (ret >= 0) {
				ret = npCommerce2WriteTextField(sending, handle, 6, text, 1);
				if (ret >= 0) {
					sum += (SceULong64)(SceInt64)ret;
					if (rewardIds != NULL && hasRewards != 0) {
						ret = npCommerce2UriEscape(text, 0x301, rewardIds[0],
									   sceClibStrnlen(rewardIds[0], 0x301), NULL);
						if (ret < 0)
							goto next;
						ret = npCommerce2WriteTextField(sending, handle, 75, text, 1);
						if (ret < 0)
							goto next;
						sum += (SceULong64)(SceInt64)ret;
					}
					ret = npCommerce2WriteNumberField(sending, handle, 7, startPosition, 1);
					if (ret >= 0) {
						sum += (SceULong64)(SceInt64)ret;
						ret = npCommerce2WriteNumberField(sending, handle, 8, maxCount, 1);
						if (ret >= 0) {
							if (sending != 1)
								length = sum + (SceULong64)(SceInt64)ret;
							ret = 0;
						}
					}
				}
			}
		}
next:
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with the mode, the product, optional category and reward IDs as fields */
int npCommerce2Send_81018EF8(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char *categoryId, const char *productId, const char * const *rewardIds,
			     int hasRewards)
{
	char text[0x304];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	SceULong64 sum;
	int pass = 0;
	int sending;
	int handle;
	int ret;

	do {
		sending = pass != 0;
		handle = *httpRequest;
		if (rewardIds != NULL && hasRewards != 0)
			sceClibSnprintf(text, 0x301, "%u", 1);
		else
			sceClibSnprintf(text, 0x301, "%u", mode);
		ret = npCommerce2WriteTextField(sending, handle, 10, text, 0);
		if (ret >= 0) {
			sum = (SceULong64)(SceInt64)ret;
			ret = npCommerce2UriEscape(text, 0x301, productId, sceClibStrnlen(productId, 0x301), NULL);
			if (ret >= 0) {
				ret = npCommerce2WriteTextField(sending, handle, 9, text, 1);
				if (ret >= 0) {
					sum += (SceULong64)(SceInt64)ret;
					if (categoryId != NULL && *categoryId != '\0') {
						ret = npCommerce2UriEscape(text, 0x301, categoryId,
									   sceClibStrnlen(categoryId, 0x301), NULL);
						if (ret < 0)
							goto next;
						ret = npCommerce2WriteTextField(sending, handle, 6, text, 1);
						if (ret < 0)
							goto next;
						sum += (SceULong64)(SceInt64)ret;
					}
					if (rewardIds != NULL && hasRewards != 0) {
						ret = npCommerce2UriEscape(text, 0x301, rewardIds[0],
									   sceClibStrnlen(rewardIds[0], 0x301), NULL);
						if (ret < 0)
							goto next;
						ret = npCommerce2WriteTextField(sending, handle, 75, text, 1);
						if (ret < 0)
							goto next;
						sum += (SceULong64)(SceInt64)ret;
					}
					if (sending != 1)
						length = sum;
					ret = 0;
				}
			}
		}
next:
		if (ret < 0)
			return ret;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				return ret;
		}
		pass++;
	} while (pass < 2);
	return 0;
}

/* a request with the version as its one field */
int npCommerce2Send_8101AF1E(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     SceUInt32 version)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	for (;;) {
		sending = pass != 0;
		ret = npCommerce2WriteNumberField(sending, *httpRequest, 2, version, 0);
		if (ret >= 0) {
			if (!sending)
				length = (SceULong64)(SceInt64)ret;
			ret = 0;
		}
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with the mode, a list of SKU IDs and, when there are as many, a list of reward IDs */
int npCommerce2Send_81019254(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char * const *skuIds, SceUInt32 skuCount, const char * const *rewardIds,
			     SceUInt32 rewardCount)
{
	char text[0x304];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	SceULong64 sum;
	SceUInt32 i;
	int pass = 0;
	int sending;
	int handle;
	int ret;

	do {
		sending = pass != 0;
		handle = *httpRequest;
		sceClibSnprintf(text, 0x301, "%u", mode);
		ret = npCommerce2WriteTextField(sending, handle, 10, text, 0);
		if (ret >= 0) {
			ret = npCommerce2WriteListField(sending, handle, 14, skuIds, skuCount, text, 0x301, 0, 1);
			if (ret >= 0) {
				sum = (SceULong64)(SceInt64)ret;
				if (rewardIds != NULL && rewardCount != 0 && skuCount == rewardCount) {
					const char * const *entry = rewardIds;

					i = 0;
					do {
						if (*entry != NULL && **entry != '\0')
							break;
						i++;
						entry++;
					} while (i < rewardCount);
					if (i != rewardCount) {
						ret = npCommerce2WriteListField(sending, handle, 76, rewardIds, rewardCount,
										text, 0x301, 0, 1);
						if (ret < 0)
							return ret;
						sum += (SceULong64)(SceInt64)ret;
					}
				}
				if (sending != 1)
					length = sum;
				ret = 0;
			}
		}
		if (ret < 0)
			return ret;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				return ret;
		}
		pass++;
	} while (pass < 2);
	return 0;
}

/* a request with a voucher code and two more texts as prefixed fields, and a flag */
int npCommerce2Send_8101A7BE(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char *voucherCode, const char *text1, const char *text2, int flag)
{
	char escaped[0x304];
	const char *prefixes[1];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	SceULong64 sum;
	int pass = 0;
	int sending;
	int handle;
	int ret;

	for (;;) {
		sending = pass != 0;
		handle = *httpRequest;
		sceClibMemset(prefixes, 0, 4);
		prefixes[0] = "voucherCode";
		ret = npCommerce2UriEscape(escaped, 0x301, voucherCode, sceClibStrnlen(voucherCode, 0x301), NULL);
		if (ret >= 0) {
			ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 20, escaped,
							    sceClibStrnlen(escaped, 0x400), 0);
			if (ret >= 0) {
				sum = (SceULong64)(SceInt64)ret;
				ret = npCommerce2UriEscape(escaped, 0x301, text1, sceClibStrnlen(text1, 0x301), NULL);
				if (ret >= 0) {
					ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 21, escaped,
									    sceClibStrnlen(escaped, 0x400), 1);
					if (ret >= 0) {
						sum += (SceULong64)(SceInt64)ret;
						ret = npCommerce2UriEscape(escaped, 0x301, text2,
									   sceClibStrnlen(text2, 0x301), NULL);
						if (ret >= 0) {
							ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 22,
											    escaped,
											    sceClibStrnlen(escaped, 0x400), 1);
							if (ret >= 0) {
								sum += (SceULong64)(SceInt64)ret;
								ret = npCommerce2WriteTextField(sending, handle, 63,
												flag == 1 ? "true" : "false", 1);
								if (ret >= 0) {
									if (sending != 1)
										length = sum + (SceULong64)(SceInt64)ret;
									ret = 0;
								}
							}
						}
					}
				}
			}
		}
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* the search fields: the mode, the range, then the names of the fields searched ("queryKey=a,b") and their
 * values ("queryValue=x,y") */
int npCommerce2WriteSearchFields(int sending, int httpRequest, SceUInt64 *total, SceUInt32 mode,
				 SceUInt32 startPosition, SceUInt32 maxCount,
				 const NpCommerce2SearchEntry *entries, SceUInt32 count)
{
	char text[0x304];
	const char * const *names;
	const NpCommerce2SearchEntry *entry;
	SceULong64 sum;
	SceULong64 first;
	SceUInt32 i;
	int size;
	int ret;

	sceClibSnprintf(text, 0x301, "%u", mode);
	ret = npCommerce2WriteTextField(sending, httpRequest, 10, text, 0);
	if (ret < 0)
		return ret;
	first = (SceULong64)(SceInt64)ret;
	ret = npCommerce2WriteNumberField(sending, httpRequest, 7, startPosition, 1);
	if (ret < 0)
		return ret;
	sum = first + (SceULong64)(SceInt64)ret;
	ret = npCommerce2WriteNumberField(sending, httpRequest, 8, maxCount, 1);
	if (ret < 0)
		return ret;
	sum += (SceULong64)(SceInt64)ret;
	if (mode <= 2) {
		if (mode != 1)
			names = g_nativeSearchFields;
		else
			names = g_gameSearchFields;
	} else if (mode > 3) {
		if (mode > 4)
			names = g_nativeSearchFields;
		else
			names = g_comicSearchFields;
	} else {
		names = g_gameSearchFields;
	}
	if (sending == 1) {
		ret = npCommerce2SendHttpRequest(httpRequest, g_queryKeys[0].text, g_queryKeys[0].length);
		if (ret >= 0) {
			ret = npCommerce2SendHttpRequest(httpRequest, g_queryKeys[11].text, g_queryKeys[11].length);
			if (ret >= 0 && count != 0) {
				entry = entries;
				for (i = 0; i < count; i++) {
					if (i != 0) {
						ret = npCommerce2SendHttpRequest(httpRequest, g_queryKeys[1].text, g_queryKeys[1].length);
						if (ret < 0)
							break;
					}
					ret = npCommerce2UriEscape(text, 0x301, names[entry->field],
								   sceClibStrnlen(names[entry->field], 0x301), NULL);
					if (ret < 0)
						break;
					ret = npCommerce2SendHttpRequest(httpRequest, text, sceClibStrnlen(text, 0x400));
					if (ret < 0)
						break;
					entry++;
				}
			}
		}
	} else {
		size = g_queryKeys[11].length + 1;
		ret = size;
		if (count != 0) {
			entry = entries;
			i = 0;
			do {
				if (i != 0)
					size++;
				ret = npCommerce2UriEscape(text, 0x301, names[entry->field],
							   sceClibStrnlen(names[entry->field], 0x301), NULL);
				if (ret < 0)
					break;
				i++;
				size += sceClibStrnlen(text, 0x301);
				entry++;
				ret = size;
			} while (i < count);
		}
	}
	if (ret < 0)
		return ret;
	sum += (SceULong64)(SceInt64)ret;
	if (sending == 1) {
		ret = npCommerce2SendHttpRequest(httpRequest, g_queryKeys[0].text, g_queryKeys[0].length);
		if (ret >= 0) {
			ret = npCommerce2SendHttpRequest(httpRequest, g_queryKeys[12].text, g_queryKeys[12].length);
			if (ret >= 0 && count != 0) {
				entry = entries;
				i = 0;
				do {
					if (i != 0) {
						ret = npCommerce2SendHttpRequest(httpRequest, g_queryKeys[1].text, g_queryKeys[1].length);
						if (ret < 0)
							break;
					}
					ret = npCommerce2UriEscape(text, 0x301, entry->value, entry->valueLength, NULL);
					if (ret < 0)
						break;
					ret = npCommerce2SendHttpRequest(httpRequest, text, sceClibStrnlen(text, 0x400));
					if (ret < 0)
						break;
					i++;
					entry++;
				} while (i < count);
			}
		}
	} else {
		size = g_queryKeys[12].length + 1;
		ret = size;
		if (count != 0) {
			entry = entries;
			i = 0;
			do {
				if (i != 0)
					size++;
				ret = npCommerce2UriEscape(text, 0x301, entry->value, entry->valueLength, NULL);
				if (ret < 0)
					break;
				i++;
				size += sceClibStrnlen(text, 0x301);
				entry++;
				ret = size;
			} while (i < count);
		}
	}
	if (ret >= 0) {
		if (sending != 1)
			*total = sum + (SceULong64)(SceInt64)ret;
		ret = 0;
	}
	return ret;
}

/* a search request */
int npCommerce2Send_8101AE3E(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     SceUInt32 startPosition, SceUInt32 maxCount, const NpCommerce2SearchEntry *entries,
			     SceUInt32 count, int unused)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int ret;

	for (;;) {
		sending = pass != 0;
		ret = npCommerce2WriteSearchFields(sending, *httpRequest, &length, mode, startPosition, maxCount,
						   entries, count);
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* the "m" and "f" of a form field that tells the sex; the first entry is a placeholder */
static const char * const g_sexTexts[3] = {
	"XXX",
	"m",
	"f",
};

/* a request with two account names (either may be missing), a number and an escaped text */
int npCommerce2Send_8101B06C(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char *first, const char *second, SceUInt32 number, const char *text,
			     int textLength)
{
	char escaped[0x304];
	const char *prefixes[2];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	SceULong64 sum;
	int pass = 0;
	int sending;
	int notFirst;
	int handle;
	int ret;

	for (;;) {
		sending = pass != 0;
		notFirst = 0;
		handle = *httpRequest;
		sum = 0;
		sceClibMemset(prefixes, 0, 8);
		prefixes[0] = "account.";
		if (first != NULL) {
			ret = npCommerce2UriEscape(escaped, 0x301, first, sceClibStrnlen(first, 0x301), NULL);
			if (ret < 0)
				goto checked;
			ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 46, escaped,
							    sceClibStrnlen(escaped, 0x400), notFirst);
			if (ret < 0)
				goto checked;
			sum = (SceULong64)(SceInt64)ret;
			notFirst = 1;
		}
		if (second != NULL) {
			ret = npCommerce2UriEscape(escaped, 0x301, second, sceClibStrnlen(second, 0x301), NULL);
			if (ret < 0)
				goto checked;
			ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 4, escaped,
							    sceClibStrnlen(escaped, 0x400), notFirst);
			if (ret < 0)
				goto checked;
			sum += (SceULong64)(SceInt64)ret;
			notFirst = 1;
		}
		sceClibSnprintf(escaped, 0x301, "%u", number);
		ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 47, escaped,
						    sceClibStrnlen(escaped, 0x400), notFirst);
		if (ret >= 0) {
			sum += (SceULong64)(SceInt64)ret;
			ret = npCommerce2UriEscape(escaped, 0x301, text, textLength, NULL);
			if (ret >= 0) {
				ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 48, escaped,
								    sceClibStrnlen(escaped, 0x400), 1);
				if (ret >= 0) {
					if (sending != 1)
						length = sum + (SceULong64)(SceInt64)ret;
					ret = 0;
				}
			}
		}
checked:
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with an account name and a text, the optional texts that follow and a sex */
int npCommerce2Send_8101B70C(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char *first, int firstLength, const char *second, int secondLength,
			     const char *third, const char *fourth, int sex)
{
	char escaped[0x304];
	const char *prefixes[2];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	SceULong64 sum;
	int pass = 0;
	int sending;
	int handle;
	int ret;

	for (;;) {
		sending = pass != 0;
		handle = *httpRequest;
		sceClibMemset(prefixes, 0, 8);
		prefixes[0] = "account.";
		ret = npCommerce2UriEscape(escaped, 0x301, first, firstLength, NULL);
		if (ret >= 0) {
			ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 50, escaped,
							    sceClibStrnlen(escaped, 0x400), 0);
			if (ret >= 0) {
				sum = (SceULong64)(SceInt64)ret;
				ret = npCommerce2UriEscape(escaped, 0x301, second, secondLength, NULL);
				if (ret >= 0) {
					ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 51, escaped,
									    sceClibStrnlen(escaped, 0x400), 1);
					if (ret >= 0) {
						sum += (SceULong64)(SceInt64)ret;
						if (third != NULL && *third != '\0') {
							ret = npCommerce2UriEscape(escaped, 0x301, third,
										   sceClibStrnlen(third, 0x301), NULL);
							if (ret < 0)
								goto checked;
							ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 52,
											    escaped, sceClibStrnlen(escaped, 0x400), 1);
							if (ret < 0)
								goto checked;
							sum += (SceULong64)(SceInt64)ret;
						}
						if (fourth != NULL && *fourth != '\0') {
							ret = npCommerce2UriEscape(escaped, 0x301, fourth,
										   sceClibStrnlen(fourth, 0x301), NULL);
							if (ret < 0)
								goto checked;
							ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 53,
											    escaped, sceClibStrnlen(escaped, 0x400), 1);
							if (ret < 0)
								goto checked;
							sum += (SceULong64)(SceInt64)ret;
						}
						if (sex == 1 || sex == 2) {
							const char *text = g_sexTexts[sex];

							ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 54, text,
											    sceClibStrnlen(text, 0x400), 1);
							if (ret < 0)
								goto checked;
							sum += (SceULong64)(SceInt64)ret;
						}
						if (sending != 1)
							length = sum;
						ret = 0;
					}
				}
			}
		}
checked:
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with an escaped text, four flags and, when the last flag is set, a number */
int npCommerce2Send_8101BEBC(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char *text, int flagA, int flagB, int flagC, SceUInt32 number)
{
	char escaped[0x304];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	SceULong64 sum;
	int pass = 0;
	int sending;
	int handle;
	int ret;

	for (;;) {
		sending = pass != 0;
		handle = *httpRequest;
		ret = npCommerce2UriEscape(escaped, 0x301, text, sceClibStrnlen(text, 0x301), NULL);
		if (ret >= 0) {
			ret = npCommerce2WriteTextField(sending, handle, 57, escaped, 0);
			if (ret >= 0) {
				sum = (SceULong64)(SceInt64)ret;
				ret = npCommerce2WriteTextField(sending, handle, 58, flagA == 1 ? "true" : "false", 1);
				if (ret >= 0) {
					sum += (SceULong64)(SceInt64)ret;
					ret = npCommerce2WriteTextField(sending, handle, 59, flagB == 1 ? "true" : "false", 1);
					if (ret >= 0) {
						sum += (SceULong64)(SceInt64)ret;
						ret = npCommerce2WriteTextField(sending, handle, 60, "true", 1);
						if (ret >= 0) {
							sum += (SceULong64)(SceInt64)ret;
							ret = npCommerce2WriteTextField(sending, handle, 61,
											flagC == 1 ? "true" : "false", 1);
							if (ret >= 0) {
								sum += (SceULong64)(SceInt64)ret;
								if (flagC == 1) {
									ret = npCommerce2WriteNumberField(sending, handle, 62,
													  number, 1);
									if (ret < 0)
										goto checked;
									sum += (SceULong64)(SceInt64)ret;
								}
								if (sending != 1)
									length = sum;
								ret = 0;
							}
						}
					}
				}
			}
		}
checked:
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with an escaped text and an account name */
int npCommerce2Send_8101C0B4(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char *text, const char *account)
{
	char escaped[0x304];
	const char *prefixes[2];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int sending;
	int handle;
	int ret;
	SceULong64 sum;

	for (;;) {
		sending = pass != 0;
		handle = *httpRequest;
		sceClibMemset(prefixes, 0, 8);
		prefixes[0] = "account.";
		ret = npCommerce2UriEscape(escaped, 0x301, text, sceClibStrnlen(text, 0x301), NULL);
		if (ret >= 0) {
			ret = npCommerce2WriteTextField(sending, handle, 57, escaped, 0);
			if (ret >= 0) {
				sum = (SceULong64)(SceInt64)ret;
				ret = npCommerce2UriEscape(escaped, 0x301, account, sceClibStrnlen(account, 0x301), NULL);
				if (ret >= 0) {
					ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 4, escaped,
									    sceClibStrnlen(escaped, 0x400), 1);
					if (ret >= 0) {
						if (sending != 1)
							length = sum + (SceULong64)(SceInt64)ret;
						ret = 0;
					}
				}
			}
		}
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* the transaction search criteria: an escaped text and seven numbers, all under "transactionSearchCriteria." */
#define CRITERIA_NUMBER(key, format, value)							\
	do {											\
		sceClibSnprintf(escaped, 0x301, format, value);					\
		ret = npCommerce2WritePrefixedField(sending, httpRequest, prefixes, 1, key, escaped,	\
						    sceClibStrnlen(escaped, 0x400), 1);		\
		if (ret < 0)									\
			goto done;								\
		sum += (SceULong64)(SceInt64)ret;						\
	} while (0)

int npCommerce2WriteTransactionCriteria(int sending, int httpRequest, SceUInt64 *total, const char *text,
					SceUInt32 number1, SceUInt32 number2, SceUInt32 number3,
					SceUInt32 number4, SceUInt32 number5, SceUInt32 number6, int number7)
{
	char escaped[0x304];
	const char *prefixes[2];
	SceULong64 sum;
	int ret;

	sceClibMemset(prefixes, 0, 8);
	prefixes[0] = "transactionSearchCriteria.";
	ret = npCommerce2UriEscape(escaped, 0x301, text, sceClibStrnlen(text, 0x301), NULL);
	if (ret >= 0) {
		ret = npCommerce2WritePrefixedField(sending, httpRequest, prefixes, 1, 46, escaped,
						    sceClibStrnlen(escaped, 0x400), 0);
		if (ret >= 0) {
			sum = (SceULong64)(SceInt64)ret;
			CRITERIA_NUMBER(65, "%u", number1);
			CRITERIA_NUMBER(66, "%u", number2);
			CRITERIA_NUMBER(67, "%u", number3);
			CRITERIA_NUMBER(68, "%u", number4);
			CRITERIA_NUMBER(69, "%u", number5);
			CRITERIA_NUMBER(70, "%u", number6);
			CRITERIA_NUMBER(71, "%d", number7);
			if (sending != 1)
				*total = sum;
			ret = 0;
		}
	}
done:
	return ret;
}

/* a transaction search request */
int npCommerce2Send_8101C722(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char *text, SceUInt32 number1, SceUInt32 number2, SceUInt32 number3,
			     SceUInt32 number4, SceUInt32 number5, SceUInt32 number6, int number7)
{
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	int pass = 0;
	int ret;

	for (;;) {
		ret = npCommerce2WriteTransactionCriteria(pass != 0, *httpRequest, &length, text, number1, number2,
							  number3, number4, number5, number6, number7);
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with the mode, a list of texts and three numbers */
int npCommerce2Send_8101D10A(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char * const *list, SceUInt32 count, SceUInt32 startPosition,
			     SceUInt32 maxCount, SceUInt32 number)
{
	char text[0x304];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	SceULong64 sum;
	int pass = 0;
	int sending;
	int handle;
	int ret;

	for (;;) {
		sending = pass != 0;
		handle = *httpRequest;
		sceClibSnprintf(text, 0x301, "%u", mode);
		ret = npCommerce2WriteTextField(sending, handle, 10, text, 0);
		if (ret >= 0) {
			sum = (SceULong64)(SceInt64)ret;
			if (list != NULL && count != 0) {
				ret = npCommerce2WriteListField(sending, handle, 77, list, count, text, 0x301, 0, 0);
				if (ret < 0)
					goto checked;
				sum += (SceULong64)(SceInt64)ret;
			}
			ret = npCommerce2WriteNumberField(sending, handle, 7, startPosition, 1);
			if (ret >= 0) {
				sum += (SceULong64)(SceInt64)ret;
				ret = npCommerce2WriteNumberField(sending, handle, 8, maxCount, 1);
				if (ret >= 0) {
					sum += (SceULong64)(SceInt64)ret;
					ret = npCommerce2WriteNumberField(sending, handle, 78, number, 1);
					if (ret >= 0) {
						if (sending != 1)
							length = sum + (SceULong64)(SceInt64)ret;
						ret = 0;
					}
				}
			}
		}
checked:
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with an account name (at most 16 characters), written in lower case */
int npCommerce2Send_8101D4B6(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char *accountName)
{
	char lower[16];
	const char *prefixes[2];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	SceSize i;
	int pass = 0;
	int sending;
	int handle;
	int ret;

	for (;;) {
		sending = pass != 0;
		handle = *httpRequest;
		sceClibMemset(prefixes, 0, 8);
		prefixes[0] = "account.";
		for (i = 0; i < sceClibStrnlen(accountName, 0x10) && i < 0x10; i++)
			lower[i] = sceClibTolower(accountName[i]);
		if (i == 0x10)
			lower[15] = '\0';
		else
			lower[i] = '\0';
		ret = npCommerce2WritePrefixedField(sending, handle, prefixes, 1, 42, lower,
						    sceClibStrnlen(lower, 0x400), 0);
		if (ret >= 0) {
			if (sending != 1)
				length = (SceULong64)(SceInt64)ret;
			ret = 0;
		}
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with two numbers of 64 bits */
int npCommerce2Send_8101D604(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     int unused, SceUInt64 first, SceUInt64 second)
{
	char number[32];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	SceULong64 sum;
	int pass = 0;
	int sending;
	int handle;
	int notFirst;
	int ret;

	for (;;) {
		sending = pass != 0;
		handle = *httpRequest;
		sum = 0;
		notFirst = 0;
		if (first != 0) {
			npCommerce2FormatU64(number, 32, (SceUInt32)first, (SceUInt32)(first >> 32), 1);
			ret = npCommerce2WriteTextField(sending, handle, 72, number, notFirst);
			if (ret < 0)
				goto checked;
			sum = (SceULong64)(SceInt64)ret;
			notFirst = 1;
		}
		if (second != 0) {
			npCommerce2FormatU64(number, 32, (SceUInt32)second, (SceUInt32)(second >> 32), 1);
			ret = npCommerce2WriteTextField(sending, handle, 82, number, notFirst);
			if (ret < 0)
				goto checked;
			sum += (SceULong64)(SceInt64)ret;
		}
		if (sending != 1)
			length = sum;
		ret = 0;
checked:
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/* a request with a list of texts, two texts, three numbers and a flag */
int npCommerce2Send_81019998(int mode, int httpTemplate, int *httpConnection, int type, int *httpRequest,
			     const char * const *list, SceUInt32 count, const char *text1, const char *text2,
			     SceUInt32 startPosition, SceUInt32 maxCount, SceUInt32 number, int flag)
{
	char escaped[0x304];
	const char *prefix;
	const char *name;
	SceULong64 length = 0;
	SceULong64 sum;
	int pass = 0;
	int sending;
	int notFirst;
	int handle;
	int ret;

	for (;;) {
		sending = pass != 0;
		handle = *httpRequest;
		sum = 0;
		notFirst = 0;
		if (list != NULL && count != 0) {
			ret = npCommerce2WriteListField(sending, handle, 15, list, count, escaped, 0x301, 0, 0);
			if (ret < 0)
				goto checked;
			sum = (SceULong64)(SceInt64)ret;
			notFirst = 1;
		}
		if (text1 != NULL && *text1 != '\0') {
			ret = npCommerce2UriEscape(escaped, 0x301, text1, sceClibStrnlen(text1, 0x301), NULL);
			if (ret < 0)
				goto checked;
			ret = npCommerce2WriteTextField(sending, handle, 73, escaped, notFirst);
			if (ret < 0)
				goto checked;
			sum += (SceULong64)(SceInt64)ret;
			notFirst = 1;
		}
		if (text2 != NULL) {
			ret = npCommerce2UriEscape(escaped, 0x301, text2, sceClibStrnlen(text2, 0x301), NULL);
			if (ret < 0)
				goto checked;
			ret = npCommerce2WriteTextField(sending, handle, 76, escaped, notFirst);
			if (ret < 0)
				goto checked;
			sum += (SceULong64)(SceInt64)ret;
			notFirst = 1;
		}
		ret = npCommerce2WriteNumberField(sending, handle, 7, startPosition, notFirst);
		if (ret >= 0) {
			sum += (SceULong64)(SceInt64)ret;
			ret = npCommerce2WriteNumberField(sending, handle, 8, maxCount, 1);
			if (ret >= 0) {
				sum += (SceULong64)(SceInt64)ret;
				ret = npCommerce2WriteNumberField(sending, handle, 17, number, 1);
				if (ret >= 0) {
					sum += (SceULong64)(SceInt64)ret;
					ret = npCommerce2WriteTextField(sending, handle, 81, flag == 1 ? "true" : "false", 1);
					if (ret >= 0) {
						if (sending != 1)
							length = sum + (SceULong64)(SceInt64)ret;
						ret = 0;
					}
				}
			}
		}
checked:
		if (ret < 0)
			break;
		if (pass == 0) {
			prefix = NULL;
			name = NULL;
			ret = npCommerce2GetRequestPath(mode, type, &prefix, &name);
			if (ret >= 0)
				ret = npCommerce2CreateHttpRequest(httpTemplate, mode, type, prefix, name, 0,
								   httpConnection, httpRequest, length);
			if (ret < 0)
				break;
		}
		if (++pass >= 2) {
			ret = 0;
			break;
		}
	}
	return ret;
}
