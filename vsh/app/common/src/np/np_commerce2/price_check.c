/*
 * SceNpCommerce2: the library's own test flow (signing up a test account and buying with it, on the
 * networks that are not the production one), and the request it keeps to be able to abort it.
 */

#include "np_commerce2_internal.h"

SceUInt32 g_priceReqId = 0;		/* 0x81026004: the request the flow waits for */
int g_priceAborted = 0;			/* 0x81026008 */

/* aborts the flow: the request it waits for, then the flow itself at its next step */
int SceNpCommerce2_785095D9(void)
{
	if (g_priceReqId != 0)
		sceNpCommerce2AbortReq(g_priceReqId);
	g_priceAborted = 1;
	return 0;
}

/* the flow calls the library's own exports, with the arguments the exports take */
extern int SceNpCommerce2_B75D54D3();
extern int SceNpCommerce2_FE0EB375();
extern int SceNpCommerce2_4A36892A();
extern int SceNpCommerce2_115DF6B1();
extern int SceNpCommerce2_AED6006D();

/* a result the flow reads, and one of its items (the last word of an item is the value the flow wants) */
typedef struct NpCommerce2FlowResult {
	SceUInt32 words[28];
} NpCommerce2FlowResult;

typedef struct NpCommerce2FlowItem {
	SceUInt32 words[28];
} NpCommerce2FlowItem;

/* asks the store for one thing of the account and gives back the value of the first item the answer lists */
int npCommerce2FlowCheck(SceUInt32 ctxId, int a2, int a3, int a4, SceUInt32 *value)
{
	NpCommerce2FlowResult result;
	NpCommerce2FlowItem item;
	char buf[2048];
	int size;
	int ret;

	ret = SceNpCommerce2_B75D54D3(ctxId, &g_priceReqId);
	if (ret >= 0) {
		ret = SceNpCommerce2_FE0EB375(g_priceReqId, a2, a3, a4);
		if (ret >= 0) {
			ret = SceNpCommerce2_4A36892A(g_priceReqId, buf, 2048, &size);
			if (ret >= 0 && value != NULL) {
				ret = SceNpCommerce2_115DF6B1(&result, buf, size);
				if (ret >= 0) {
					ret = SceNpCommerce2_AED6006D(&result, 0, &item);
					if (ret >= 0)
						*value = item.words[27];
				}
			}
		}
	}
	sceNpCommerce2DestroyReq(g_priceReqId);
	g_priceReqId = 0;
	return ret;
}

extern void SceNpCommerce2_FF13E2C3(const char *label, const char **titleId, int enable);
extern int SceNpCommerce2_00F16110();
extern int SceNpCommerce2_6218343E();
extern int SceNpCommerce2_7DE103BB();
extern int SceNpCommerce2_8ECAE666();
extern int SceNpCommerce2_AA090F5E();
extern int SceNpCommerce2_1A9E8047();
extern int SceNpCommerce2_8B4BEA99();
extern int SceNpCommerce2_00DEFA26();
extern int SceNpCommerce2_AD4F85E1();
extern int SceNpCommerce2_9E082DB2();
extern int SceNpCommerce2_F988A255();
extern int SceNpCommerce2_C737A929();
extern int SceNpCommerce2_2DC2A0F9();
extern int SceNpCommerce2_7E3C9196();
extern int SceNpCommerce2_1EC16D19();

/* a result's data with the count of its items behind it */
typedef struct NpCommerce2FlowCount {
	char data[100];
	int count;
} NpCommerce2FlowCount;

/* the empty store check: asks the store whether a category (kind 1) or a product (kind 2) has content */
int sceNpCommerce2StartEmptyStoreCheck(int kind, const char *id, void *buf, SceSize bufSize, int *empty)
{
	const char *idList = id;
	char productResult[124];
	char productItem[96];
	NpCommerce2FlowCount categoryResult;
	NpCommerce2FlowCount product;
	char dummy[4];
	char unused[36];
	SceSize fill;
	int ret;
	int r;
	int mode;
	int started = 0;
	int zero1 = 0;
	int zero2 = 0;
	SceUInt32 ctxId;

	g_checkAborted = 0;
	if (kind <= 0 || kind > 2) {
		ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
		goto cleanup;
	}
	if (id == NULL || buf == NULL || bufSize == 0 || empty == NULL) {
		ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
		goto cleanup;
	}
	sceClibMemset(unused, 0, 0x24);
	SceNpCommerce2_FF13E2C3("NpCommerce2 EmptyStoreCheck", &idList, 1);
	ret = SceNpCommerce2_AA090F5E(2, &g_checkCtxId);
	if (ret < 0)
		goto cleanup;
	ret = SceNpCommerce2_8ECAE666(g_checkCtxId, 3);
	if (ret < 0)
		goto cleanup;
	r = SceNpCommerce2_7DE103BB(g_checkCtxId, &g_checkReqId);
	if (r >= 0) {
		ret = SceNpCommerce2_00F16110(g_checkReqId);
		if (ret >= 0)
			ret = SceNpCommerce2_6218343E(g_checkReqId, buf, bufSize, &fill);
		sceNpCommerce2DestroyReq(g_checkReqId);
		g_checkReqId = 0;
	} else {
		ret = r;
	}
	if (ret < 0)
		goto cleanup;
	if (g_checkAborted == 1) {
		ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
		started = 1;
		goto cleanup;
	}
	if (kind > 1) {
		r = sceNpCommerce2GetProductInfoCreateReq(g_checkCtxId, &g_checkReqId);
		if (r >= 0) {
			ret = SceNpCommerce2_00DEFA26(g_checkReqId, 0, idList, 0, 0);
			if (ret >= 0)
				ret = SceNpCommerce2_AD4F85E1(g_checkReqId, buf, bufSize, &fill);
			sceNpCommerce2DestroyReq(g_checkReqId);
			g_checkReqId = 0;
		} else {
			ret = r;
		}
		started = 1;
		if (ret < 0)
			goto cleanup;
		ret = sceNpCommerce2InitGetProductInfoResult((SceNpCommerce2GetProductInfoResult *)productResult, buf, fill);
		if (ret >= 0) {
			ret = SceNpCommerce2_9E082DB2((int)productResult, (int)productItem);
			if (ret >= 0)
				ret = SceNpCommerce2_F988A255((const SceNpCommerce2GetProductInfoResult *)productItem, &product);
		}
		started = 1;
		if (ret < 0)
			goto cleanup;
		mode = npCommerce2GetEnvironment();
		if (mode > 1) {
			if (mode <= 2)
				*empty = 1;
		} else if (mode >= 0) {
			if (product.count != 0)
				*empty = zero1;
			else
				*empty = 1;
		}
	} else {
		r = sceNpCommerce2GetCategoryContentsCreateReq(g_checkCtxId, &g_checkReqId);
		if (r >= 0) {
			ret = SceNpCommerce2_1A9E8047(g_checkReqId, idList, 0, 0, 0, 1);
			if (ret >= 0)
				ret = SceNpCommerce2_8B4BEA99(g_checkReqId, buf, bufSize, &fill);
			sceNpCommerce2DestroyReq(g_checkReqId);
			g_checkReqId = 0;
		} else {
			ret = r;
		}
		started = 1;
		if (ret < 0)
			goto cleanup;
		ret = sceNpCommerce2InitGetCategoryContentsResult((SceNpCommerce2GetCategoryContentsResult *)&categoryResult, buf, fill);
		started = 1;
		if (ret < 0)
			goto cleanup;
		mode = npCommerce2GetEnvironment();
		if (mode > 1) {
			if (mode <= 2)
				*empty = 1;
		} else if (mode >= 0) {
			if (categoryResult.count != 0)
				*empty = zero1;
			else
				*empty = 1;
		}
	}
	ret = zero2;
	started = 1;
cleanup:
	ctxId = g_checkCtxId;
	if (started == 1 && g_checkAborted == 0) {
		r = SceNpCommerce2_C737A929(g_checkCtxId, &g_checkReqId);
		if (r >= 0) {
			if (SceNpCommerce2_2DC2A0F9(g_checkReqId) >= 0)
				SceNpCommerce2_7E3C9196(g_checkReqId, buf, bufSize, dummy);
		}
		sceNpCommerce2DestroyReq(g_checkReqId);
		g_checkReqId = zero2;
		ctxId = g_checkCtxId;
	}
	if (ctxId != 0)
		SceNpCommerce2_1EC16D19(ctxId);
	return ret;
}

#define NP_COMMERCE2_FLOW_ABORTED	-2141909038		/* 0x80550fd2 */
#define NP_COMMERCE2_FLOW_NAME_TOO_LONG	-2141909039		/* 0x80550fd1 */
#define NP_COMMERCE2_FLOW_NO_ENTRY	-2141909040		/* 0x80550fd0 */
#define NP_COMMERCE2_FLOW_PRODUCTION	-2141909037		/* 0x80550fd3 */
#define NP_COMMERCE2_FLOW_NAME_USED	-2141908924
#define NP_COMMERCE2_FLOW_PASSWORD_USED	-2141908920
#define NP_COMMERCE2_FLOW_NO_ADDRESS	-2141908988

extern int SceNpCommerce2_5A1ACD90();
extern int SceNpCommerce2_E8F39860();
extern int SceNpCommerce2_A6CC5E4A();
extern int SceNpCommerce2_2837D9E2();
extern int SceNpCommerce2_0DE0C8B1();
extern int SceNpCommerce2_DACD1180();
extern int SceNpCommerce2_EB8775F5();
extern int SceNpCommerce2_2D3073AF();
extern int SceNpCommerce2_95C185B5();
extern int SceNpCommerce2_D3509F1B();
extern int SceNpCommerce2_8D8509B1();
extern int SceNpCommerce2_C28139BD();
extern int SceNpCommerce2_1A37B2BA();
extern int SceNpCommerce2_60187EF1();
extern int SceNpCommerce2_B7D151D1();
extern int SceNpCommerce2_92DCC788();
extern int SceNpCommerce2_C81DF030();
extern int SceNpCommerce2_76DA0D80();
extern int SceNpCommerce2_F50D7BE4();
extern int SceNpCommerce2_F40D2B1E();
extern int SceNpCommerce2_6E0F1699();
extern int SceNpCommerce2_BAB8A533();
extern int SceNpCommerce2_E89CF937();
extern int SceNpCommerce2_693EAE89();
extern int SceNpCommerce2_BD0555F8();
extern int SceNpCommerce2_E6BB6285();
extern int SceNpCommerce2_F8D3F362();
extern int SceNpCommerce2_C9C5EBF7();
extern int SceNpCommerce2_22640144();
extern int SceNpCommerce2_B70BF176();
extern int SceNpCommerce2_C3B1179E();
extern int SceNpCommerce2_F682B145();
extern int SceNpCommerce2_4E4A2898();
extern int SceNpCommerce2_D4D3D6D2();
extern int SceNpCommerce2_E16A3E4F();
extern int SceNpCommerce2_65743DF2();
extern int SceNpCommerce2_E13EBC17();
extern int SceNpCommerce2_39E7EB72();
extern int SceNpCommerce2_1729CBC6();
extern int SceNpCommerce2_81C509DA();
extern int SceNpCommerce2_BC05DE41();
extern int SceNpCommerce2_E0B222F3();
extern int SceNpCommerce2_527C3D90();
extern int SceNpCommerce2_2AA6C91E();

/* one question to the store: makes a request, starts it, reads its answer, and ends it */
#define NP_COMMERCE2_FLOW_ASK(ret, create, ctx, start, result)					\
	do {											\
		(ret) = create((ctx), &g_priceReqId);						\
		if ((ret) >= 0) {								\
			(ret) = (start);							\
			if ((ret) >= 0)								\
				(ret) = (result);						\
			sceNpCommerce2DestroyReq(g_priceReqId);					\
			g_priceReqId = 0;							\
		}											\
	} while (0)

#define NP_COMMERCE2_FLOW_CHECK_ABORT								\
	do {											\
		if (g_priceAborted == 1) {							\
			ret = NP_COMMERCE2_FLOW_ABORTED;					\
			goto cleanup;								\
		}										\
	} while (0)

/* the character of a random number for the names and passwords: digits, then capitals, then small letters */
#define NP_COMMERCE2_FLOW_CHAR(n)	((n) >= 10 ? ((n) >= 36 ? (n) + 61 : (n) + 55) : (n) + 48)

/* signs up a test account with random data and gives it the answers the store asks for; kind 1 also asks
 * for the account's wallet; gives the account's name and password */
int npCommerce2FlowSignUp(int kind, int a2, int a3, SceUInt32 ctxId, int (*random)(void), const char *country,
			  const char *language, int a8, char *accountName, SceSize accountNameSize,
			  char *password, SceSize passwordSize, void *sessionInfo, SceUInt32 *address,
			  char *province, SceSize provinceSize, char *city, SceSize citySize,
			  char *postalCode, SceSize postalCodeSize, void *work, SceUInt32 workSize,
			  SceSize *fillSize)
{
	char response[2048];
	char response2[2048];
	SceSize fill;
	SceSize fill2;
	int exists;
	int exists2;
	char dummy[4];
	char session[92];
	SceUInt32 categoryResult[24];
	SceUInt32 addressResult[24];
	SceUInt32 item[25];
	const char *cityItem[25];
	const char *provinceItem[26];
	char phone[8];
	char name[20];
	char pass[32];
	char email[64];
	SceUInt32 value = 0;
	int ok = 1;
	int r;
	int ret;
	int i;
	char *p;
	SceUInt32 n;

	ret = SceNpCommerce2_8ECAE666(ctxId, 5);
	if (ret < 0)
		return ret;
	NP_COMMERCE2_FLOW_ASK(ret, SceNpCommerce2_5A1ACD90, ctxId,
			      SceNpCommerce2_E8F39860(g_priceReqId, country, language, a8, 1, 1),
			      SceNpCommerce2_A6CC5E4A(g_priceReqId, work, workSize, &fill));
	if (ret < 0)
		return ret;
	r = SceNpCommerce2_2837D9E2(session, work, fill);
	if (r >= 0)
		r = SceNpCommerce2_0DE0C8B1(session, sessionInfo);
	ret = r;
	if (ret < 0)
		goto cleanup;
	NP_COMMERCE2_FLOW_CHECK_ABORT;
	if (kind == 1) {
		NP_COMMERCE2_FLOW_ASK(ret, SceNpCommerce2_DACD1180, ctxId,
				      SceNpCommerce2_EB8775F5(g_priceReqId, a2, a3),
				      SceNpCommerce2_2D3073AF(g_priceReqId, response, 0x800, &fill2));
		if (ret < 0)
			goto cleanup;
		NP_COMMERCE2_FLOW_CHECK_ABORT;
	}
	NP_COMMERCE2_FLOW_ASK(ret, SceNpCommerce2_95C185B5, ctxId,
			      SceNpCommerce2_D3509F1B(g_priceReqId),
			      SceNpCommerce2_8D8509B1(g_priceReqId, work, workSize, &fill));
	if (ret < 0)
		goto cleanup;
	r = SceNpCommerce2_C28139BD(categoryResult, work, fill);
	if (r >= 0) {
		if (categoryResult[23] != 0) {
			r = SceNpCommerce2_1A37B2BA(categoryResult, 0, item);
			if (r >= 0)
				value = item[23];
		} else {
			r = NP_COMMERCE2_FLOW_NO_ENTRY;
		}
	}
	ret = r;
	if (ret < 0)
		goto cleanup;
	NP_COMMERCE2_FLOW_CHECK_ABORT;
	for (;;) {
		do {
			if (g_priceAborted == 1) {
				r = NP_COMMERCE2_FLOW_ABORTED;
				break;
			}
			sceClibStrncpy(name, "Q-", 0x11);
			p = &name[sceClibStrnlen("Q-", 0x11)];
			for (i = 0; i < 8; i++) {
				n = (SceUInt32)random() % 62;
				*p++ = NP_COMMERCE2_FLOW_CHAR(n);
			}
			*p = '-';
			p[1] = sceClibToupper(country[0]);
			p[2] = sceClibToupper(country[1]);
			p[3] = '-';
			p[4] = sceClibToupper(language[0]);
			p[5] = sceClibToupper(language[1]);
			p[6] = 0;
			exists = 0;
			r = SceNpCommerce2_60187EF1(16, name, &exists);
		} while (r < 0 || exists != 0);
		ret = r;
		if (ret < 0)
			goto cleanup;
		r = 0;
		if (sceClibStrnlen(name, 0x10) + 25 <= 0x40)
			sceClibSnprintf(email, 0x40, "%s%s", name, "@d0.mail.playstation.net");
		else
			r = NP_COMMERCE2_FLOW_NAME_TOO_LONG;
		ret = r;
		if (ret < 0)
			goto cleanup;
		for (;;) {
			if (g_priceAborted == 1) {
				r = NP_COMMERCE2_FLOW_ABORTED;
				break;
			}
			sceClibMemset(pass, 0, 0x1f);
			p = pass;
			for (i = 0; i < 0x1e; i++) {
				n = (SceUInt32)random() % 62;
				*p++ = NP_COMMERCE2_FLOW_CHAR(n);
			}
			exists2 = 0;
			r = SceNpCommerce2_60187EF1(40, pass, &exists2);
			if (r >= 0 && exists2 == 0)
				break;
		}
		ret = r;
		if (ret < 0)
			goto cleanup;
		NP_COMMERCE2_FLOW_ASK(r, SceNpCommerce2_B7D151D1, ctxId,
				      SceNpCommerce2_92DCC788(g_priceReqId, email, pass, value, pass,
							      sceClibStrnlen(pass, 0x1e)),
				      SceNpCommerce2_C81DF030(g_priceReqId, work, workSize, &fill));
		ret = r;
		if (ret == NP_COMMERCE2_FLOW_NAME_USED)
			continue;
		if (ret < 0)
			goto cleanup;
		NP_COMMERCE2_FLOW_CHECK_ABORT;
		if (kind == 0) {
			NP_COMMERCE2_FLOW_ASK(ret, SceNpCommerce2_76DA0D80, ctxId,
					      SceNpCommerce2_F50D7BE4(g_priceReqId, 0),
					      SceNpCommerce2_F40D2B1E(g_priceReqId, work, workSize, &fill));
			if (ret < 0)
				goto cleanup;
			NP_COMMERCE2_FLOW_CHECK_ABORT;
		}
		NP_COMMERCE2_FLOW_ASK(ret, SceNpCommerce2_6E0F1699, ctxId,
				      SceNpCommerce2_BAB8A533(g_priceReqId, name),
				      SceNpCommerce2_E89CF937(g_priceReqId, work, workSize, &fill));
		if (ret != NP_COMMERCE2_FLOW_PASSWORD_USED)
			break;
	}
	if (ret < 0)
		goto cleanup;
	NP_COMMERCE2_FLOW_CHECK_ABORT;
	NP_COMMERCE2_FLOW_ASK(ret, SceNpCommerce2_693EAE89, ctxId,
			      SceNpCommerce2_BD0555F8(g_priceReqId, "firstName", sceClibStrnlen("firstName", 0x80),
						      "lastName", sceClibStrnlen("lastName", 0x80)),
			      SceNpCommerce2_E6BB6285(g_priceReqId, work, workSize, &fill));
	if (ret < 0)
		goto cleanup;
	NP_COMMERCE2_FLOW_CHECK_ABORT;
	NP_COMMERCE2_FLOW_ASK(ret, SceNpCommerce2_F8D3F362, ctxId,
			      SceNpCommerce2_C9C5EBF7(g_priceReqId),
			      SceNpCommerce2_22640144(g_priceReqId, work, workSize, &fill));
	if (ret < 0 && ret != NP_COMMERCE2_FLOW_NO_ADDRESS)
		goto cleanup;
	ok = ret != NP_COMMERCE2_FLOW_NO_ADDRESS;
	ret = 0;
	sceClibMemset(province, provinceSize, 0);
	sceClibStrncpy(province, "Province", provinceSize);
	sceClibMemset(city, citySize, 0);
	sceClibStrncpy(city, "City", citySize);
	sceClibMemset(postalCode, postalCodeSize, 0);
	sceClibStrncpy(postalCode, "11111", postalCodeSize);
	if (ok != 0) {
		ret = SceNpCommerce2_B70BF176(addressResult, work, fill);
		if (ret >= 0 && addressResult[23] != 0) {
			ret = SceNpCommerce2_C3B1179E(addressResult, 0, provinceItem);
			if (ret >= 0) {
				sceClibStrncpy(province, provinceItem[24], provinceSize);
				if (provinceItem[25] != NULL) {
					ret = SceNpCommerce2_F682B145(provinceItem, 0, cityItem);
					if (ret >= 0)
						sceClibStrncpy(city, cityItem[24], citySize);
				}
			}
		}
	}
	if (ret < 0)
		goto cleanup;
	NP_COMMERCE2_FLOW_CHECK_ABORT;
	sceClibMemset(address, 0, 0x64);
	address[13] = (SceUInt32)"StreetAddress1";
	address[14] = sceClibStrnlen("StreetAddress1", 0x20);
	address[21] = (SceUInt32)province;
	address[22] = sceClibStrnlen(province, 0x40);
	address[19] = (SceUInt32)city;
	address[20] = sceClibStrnlen(city, 0x40);
	address[23] = (SceUInt32)postalCode;
	address[24] = sceClibStrnlen(postalCode, 0xa);
	if (sceClibStrncasecmp("US", country, 3) != 0) {
		if (sceClibStrncasecmp("CA", country, 3) == 0) {
			address[21] = (SceUInt32)"ON";
			address[22] = sceClibStrnlen("ON", 0x40);
			address[23] = (SceUInt32)"M2H 3R6";
			address[24] = sceClibStrnlen("M2H 3R6", 0xa);
		}
	} else {
		address[21] = (SceUInt32)"CA";
		address[22] = sceClibStrnlen("CA", 0x40);
		address[23] = (SceUInt32)"94404";
		address[24] = sceClibStrnlen("94404", 0xa);
	}
	NP_COMMERCE2_FLOW_ASK(ret, SceNpCommerce2_4E4A2898, ctxId,
			      SceNpCommerce2_D4D3D6D2(g_priceReqId, address),
			      SceNpCommerce2_E16A3E4F(g_priceReqId, work, workSize, &fill));
	if (ret < 0)
		goto cleanup;
	NP_COMMERCE2_FLOW_CHECK_ABORT;
	if (kind == 0 && sceClibStrncasecmp("KR", country, 3) == 0) {
		SceUInt32 number = (SceUInt32)random();

		sceClibSnprintf(phone, 8, "%07u", number % 9999999);
		NP_COMMERCE2_FLOW_ASK(ret, SceNpCommerce2_65743DF2, ctxId,
				      SceNpCommerce2_E13EBC17(g_priceReqId, "700101", phone),
				      SceNpCommerce2_39E7EB72(g_priceReqId, work, workSize, &fill));
		if (ret < 0)
			goto cleanup;
		NP_COMMERCE2_FLOW_CHECK_ABORT;
	} else if (kind == 0) {
		NP_COMMERCE2_FLOW_CHECK_ABORT;
	}
	NP_COMMERCE2_FLOW_ASK(ret, SceNpCommerce2_1729CBC6, ctxId,
			      SceNpCommerce2_81C509DA(g_priceReqId, 1, 1),
			      SceNpCommerce2_BC05DE41(g_priceReqId, work, workSize, &fill));
	if (ret < 0)
		goto cleanup;
	NP_COMMERCE2_FLOW_CHECK_ABORT;
	NP_COMMERCE2_FLOW_ASK(ret, SceNpCommerce2_E0B222F3, ctxId,
			      SceNpCommerce2_527C3D90(g_priceReqId),
			      SceNpCommerce2_2AA6C91E(g_priceReqId, work, workSize, &fill));
	if (ret < 0)
		goto cleanup;
	NP_COMMERCE2_FLOW_CHECK_ABORT;
	*fillSize = fill;
	if (accountName != NULL && accountNameSize != 0) {
		sceClibMemset(accountName, 0, accountNameSize);
		sceClibStrncpy(accountName, email, accountNameSize);
	}
	sceClibMemset(password, 0, passwordSize);
	sceClibStrncpy(password, pass, passwordSize);
cleanup:
	if (g_priceAborted == 0) {
		r = SceNpCommerce2_C737A929(ctxId, &g_priceReqId);
		if (r >= 0) {
			if (SceNpCommerce2_2DC2A0F9(g_priceReqId) >= 0)
				SceNpCommerce2_7E3C9196(g_priceReqId, response2, 2048, dummy);
		}
		sceNpCommerce2DestroyReq(g_priceReqId);
		g_priceReqId = 0;
	}
	return ret;
}

extern int SceNpCommerce2_E4DEA05E();
extern int SceNpCommerce2_FA3E492B();
extern int SceNpCommerce2_89171F84();
extern int SceNpCommerce2_C0ECF440();
extern int SceNpCommerce2_7D2A014E();
extern int SceNpCommerce2_07E38F93();

/* the flow: signs up a test account (not on the production network), and when the session asks for a
 * payment method gives the account a test card and the wallet's first top up */
int npCommerce2FlowRun(SceUInt32 ctxId, int (*random)(void), const char *country, const char *language,
		       char *accountOut, SceSize accountOutSize, char *passwordOut, SceSize passwordOutSize,
		       void *work, SceUInt32 workSize, SceSize *fillOut)
{
	char response1[2048];
	char response2[2048];
	char response3[2048];
	SceSize fill1;
	SceSize fill2;
	char dummy[4];
	SceUInt32 address[25];
	SceUInt32 payment[25];
	SceUInt32 sessionInfo[39];
	char postalCode[16];
	char environment[20];
	char password[32];
	char province[64];
	char city[64];
	char account[64];
	int topUp;
	int ret;
	int r;
	int sessionType;
	SceSize fill;

	g_priceAborted = 0;
	if (random == NULL || country == NULL || language == NULL || passwordOut == NULL || work == NULL ||
	    workSize == 0 || fillOut == NULL) {
		ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
	} else if (passwordOutSize < 0x1f) {
		ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
	} else if (sceClibStrnlen(country, 2) != 2 || sceClibStrnlen(language, 2) != 2) {
		ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
	} else {
		ret = npCommerce2GetEnvironmentName(environment, 0x11);
		if (ret >= 0) {
			if (sceClibStrncasecmp("np", environment, 3) != 0) {
				ret = npCommerce2FlowSignUp(0, 0, 0, ctxId, random, country, language, 1970, account, 0x40,
							    password, 0x1f, sessionInfo, address, province, 0x40, city, 0x40,
							    postalCode, 16, work, workSize, fillOut);
				if (ret >= 0) {
					if (accountOut != NULL && accountOutSize != 0) {
						sceClibMemset(accountOut, 0, accountOutSize);
						sceClibStrncpy(accountOut, account, accountOutSize);
					}
					sceClibMemset(passwordOut, 0, passwordOutSize);
					sceClibStrncpy(passwordOut, password, passwordOutSize);
					if ((sessionInfo[32] & 1) == 0)
						goto checkAbort;
					sessionType = sessionInfo[34];
					ret = SceNpCommerce2_8ECAE666(ctxId, 6);
					if (ret >= 0) {
						if (g_priceAborted == 1) {
							ret = NP_COMMERCE2_FLOW_ABORTED;
						} else {
							ret = npCommerce2FlowCheck(ctxId, 1, (int)account, (int)password, &topUp);
							if (ret >= 0) {
								if (g_priceAborted == 1) {
									ret = NP_COMMERCE2_FLOW_ABORTED;
								} else {
									sceClibMemset(payment, 0, 0x64);
									payment[13] = address[13];
									payment[14] = address[14];
									payment[21] = address[21];
									payment[22] = address[22];
									payment[19] = address[19];
									payment[20] = address[20];
									payment[23] = address[23];
									payment[24] = address[24];
									payment[0] = (SceUInt32)"VISA";
									payment[1] = sceClibStrnlen("VISA", 0x10);
									payment[2] = (SceUInt32)"firstName lastName";
									payment[3] = sceClibStrnlen("firstName lastName", 0x20);
									payment[5] = 2008;
									payment[6] = 12;
									payment[7] = 2022;
									payment[8] = 12;
									payment[9] = (SceUInt32)"150";
									if (sessionType == 1) {
										if (sceClibStrncasecmp("BR", country, 3) != 0) {
											payment[4] = (SceUInt32)"4444444444444448";
										} else {
											payment[4] = (SceUInt32)"4111111111111111";
											payment[11] = (SceUInt32)"55412345678";
											payment[12] = (SceUInt32)"37043292239";
											payment[23] = (SceUInt32)"80240000";
											payment[24] = sceClibStrnlen("80240000", 0xa);
										}
									} else {
										payment[4] = (SceUInt32)"4111111111111111";
									}
									NP_COMMERCE2_FLOW_ASK(ret, SceNpCommerce2_E4DEA05E, ctxId,
											      SceNpCommerce2_FA3E492B(g_priceReqId, payment),
											      SceNpCommerce2_89171F84(g_priceReqId, response2, 0x800,
														      &fill1));
									if (ret >= 0) {
										if (g_priceAborted == 1) {
											ret = NP_COMMERCE2_FLOW_ABORTED;
										} else {
											NP_COMMERCE2_FLOW_ASK(ret, SceNpCommerce2_C0ECF440, ctxId,
													      SceNpCommerce2_7D2A014E(g_priceReqId,
																      topUp),
													      SceNpCommerce2_07E38F93(g_priceReqId,
																      response3, 0x800,
																      &fill2));
											if (ret >= 0 && g_priceAborted == 1)
												ret = NP_COMMERCE2_FLOW_ABORTED;
										}
									}
								}
							}
						}
					}
					r = SceNpCommerce2_C737A929(ctxId, &g_priceReqId);
					if (r >= 0) {
						if (SceNpCommerce2_2DC2A0F9(g_priceReqId) >= 0)
							SceNpCommerce2_7E3C9196(g_priceReqId, response1, 2048, dummy);
					}
					sceNpCommerce2DestroyReq(g_priceReqId);
					g_priceReqId = 0;
					if (ret >= 0) {
checkAbort:
						if (g_priceAborted == 1)
							ret = NP_COMMERCE2_FLOW_ABORTED;
					}
				}
			} else {
				ret = NP_COMMERCE2_FLOW_PRODUCTION;
			}
		}
	}
	return ret;
}

extern int SceNpCommerce2_5402711A();
extern int SceNpCommerce2_6632CE20();

/* the flow of the export: signs up a test account and, when asked to (flag 1), registers a second
 * account with the first one's help and has it buy; gives the password of the account made */
int SceNpCommerce2_F9DE9CDF(SceUInt32 ctxId, int (*random)(void), const char *country, const char *language,
			    int flag, int a6, int a7, int a8, char *passwordOut, SceSize passwordOutSize,
			    void *work, SceUInt32 workSize, SceSize *fillOut)
{
	char response1[2048];
	char response2[2048];
	SceSize fill1;
	SceSize fill2;
	char dummy[4];
	SceUInt32 address[25];
	SceUInt32 sessionInfo[39];
	char postalCode[16];
	char account[64];
	char password[32];
	char account2[64];
	char password2[32];
	char province[64];
	char city[64];
	int ret;
	int r;

	if (flag == 1) {
		g_priceAborted = 0;
		ret = npCommerce2FlowRun(ctxId, random, country, language, account, 0x40, password, 0x1f, work,
					 workSize, fillOut);
		if (ret < 0)
			return ret;
		if (g_priceAborted == 1) {
			ret = NP_COMMERCE2_FLOW_ABORTED;
		} else {
			ret = npCommerce2FlowSignUp(1, (int)account, (int)password, ctxId, random, country, language,
						    a6, account2, 0x40, password2, 0x1f, sessionInfo, address, province,
						    0x40, city, 0x40, postalCode, 16, work, workSize, fillOut);
			if (ret >= 0) {
				ret = SceNpCommerce2_8ECAE666(ctxId, 6);
				if (ret >= 0) {
					if (g_priceAborted == 1) {
						ret = NP_COMMERCE2_FLOW_ABORTED;
					} else {
						ret = npCommerce2FlowCheck(ctxId, 1, (int)account, (int)password, NULL);
						if (ret >= 0) {
							if (g_priceAborted == 1) {
								ret = NP_COMMERCE2_FLOW_ABORTED;
							} else {
								NP_COMMERCE2_FLOW_ASK(ret, SceNpCommerce2_5402711A, ctxId,
										      SceNpCommerce2_10B76A6F(g_priceReqId, account2),
										      SceNpCommerce2_6632CE20(g_priceReqId, response2,
													      0x800, &fill2));
								if (ret >= 0 && g_priceAborted == 1)
									ret = NP_COMMERCE2_FLOW_ABORTED;
							}
						}
					}
				}
				r = SceNpCommerce2_C737A929(ctxId, &g_priceReqId);
				if (r >= 0) {
					if (SceNpCommerce2_2DC2A0F9(g_priceReqId) >= 0)
						SceNpCommerce2_7E3C9196(g_priceReqId, response1, 2048, dummy);
				}
				sceNpCommerce2DestroyReq(g_priceReqId);
				g_priceReqId = 0;
				if (ret >= 0) {
					sceClibMemset(passwordOut, 0, passwordOutSize);
					sceClibStrncpy(passwordOut, password2, passwordOutSize);
				}
			}
		}
		return ret;
	}
	return npCommerce2FlowRun(ctxId, random, country, language, NULL, 0, passwordOut, passwordOutSize, work,
				  workSize, fillOut);
}
