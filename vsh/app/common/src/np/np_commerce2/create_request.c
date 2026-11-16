/*
 * SceNpCommerce2: the request makers. One per operation of the store's web API (the
 * operations' names are the library's own strings: "crSess", "getProd", ...), each makes a
 * request of its operation's type on a context.
 */

#include "np_commerce2_internal.h"

int npCommerce2CreateReqGetProd(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_PROD, reqId, 0);
}

int npCommerce2CreateReqAddCart(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_ADD_CART, reqId, 0);
}

int npCommerce2CreateReqRemCart(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_REM_CART, reqId, 0);
}

int npCommerce2CreateReqClearCart(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_CLEAR_CART, reqId, 0);
}

int npCommerce2CreateReqGetCart(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_CART, reqId, 0);
}

int npCommerce2CreateReqPurPrev(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_PUR_PREV, reqId, 0);
}

int npCommerce2CreateReqPurch(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_PURCH, reqId, 0);
}

int npCommerce2CreateReqGetCC(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_C_C, reqId, 0);
}

int npCommerce2CreateReqGetProv(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_PROV, reqId, 0);
}

int npCommerce2CreateReqGetPay(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_PAY, reqId, 0);
}

int npCommerce2CreateReqGetBillInfo(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_BILL_INFO, reqId, 0);
}

int npCommerce2CreateReqGetMasterBill(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_MASTER_BILL, reqId, 0);
}

int npCommerce2CreateReqGetDrm(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_DRM, reqId, 0);
}

int npCommerce2CreateReqGetBill(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_BILL, reqId, 0);
}

int npCommerce2CreateReqGetResAddr(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_RES_ADDR, reqId, 0);
}

int npCommerce2CreateReqGetMasterResAddr(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_MASTER_RES_ADDR, reqId, 0);
}

int npCommerce2CreateReqRegCC(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_REG_C_C, reqId, 0);
}

int npCommerce2CreateReqGetAddr(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_ADDR, reqId, 0);
}

int npCommerce2CreateReqGetWal(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_WAL, reqId, 0);
}

int npCommerce2CreateReqTopupCC(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_TOPUP_C_C, reqId, 0);
}

int npCommerce2CreateReqTopEdy(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_TOP_EDY, reqId, 0);
}

int npCommerce2CreateReqTopEdyRes(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_TOP_EDY_RES, reqId, 0);
}

int npCommerce2CreateReqVchrRedPrev(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_VCHR_RED_PREV, reqId, 0);
}

int npCommerce2CreateReqVchrRed(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_VCHR_RED, reqId, 0);
}

int npCommerce2CreateReqEventLog(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_EVENT_LOG, reqId, 0);
}

int npCommerce2CreateReqSrchProd(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_SRCH_PROD, reqId, 0);
}

int npCommerce2CreateReqGetCountries(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_COUNTRIES, reqId, 0);
}

int npCommerce2CreateReqBindAccount(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_BIND_ACCOUNT, reqId, 0);
}

int npCommerce2CreateReqStartReg(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_START_REG, reqId, 0);
}

int npCommerce2CreateReqGetSecQ(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_SEC_Q, reqId, 0);
}

int npCommerce2CreateReqSetCred(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_SET_CRED, reqId, 0);
}

int npCommerce2CreateReqSetLid(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_SET_LID, reqId, 0);
}

int npCommerce2CreateReqUpdPass(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_UPD_PASS, reqId, 0);
}

int npCommerce2CreateReqVerifyAccountPass(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_VERIFY_ACCOUNT_PASS, reqId, 0);
}

int npCommerce2CreateReqVerifyOid(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_VERIFY_OID, reqId, 0);
}

int npCommerce2CreateReqSetOid(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_SET_OID, reqId, 0);
}

int npCommerce2CreateReqGetName(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_NAME, reqId, 0);
}

int npCommerce2CreateReqSetName(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_SET_NAME, reqId, 0);
}

int npCommerce2CreateReqSetResAddr(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_SET_RES_ADDR, reqId, 0);
}

int npCommerce2CreateReqSetRrn(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_SET_RRN, reqId, 0);
}

int npCommerce2CreateReqGetOptIn(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_OPT_IN, reqId, 0);
}

int npCommerce2CreateReqSetOptIn(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_SET_OPT_IN, reqId, 0);
}

int npCommerce2CreateReqAuthMaster(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_AUTH_MASTER, reqId, 0);
}

int npCommerce2CreateReqSetSubCtrl(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_SET_SUB_CTRL, reqId, 0);
}

int npCommerce2CreateReqResetSubPass(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_RESET_SUB_PASS, reqId, 0);
}

int npCommerce2CreateReqGetWalOpt(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_WAL_OPT, reqId, 0);
}

int npCommerce2CreateReqSetWalOpt(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_SET_WAL_OPT, reqId, 0);
}

int npCommerce2CreateReqSubmitReg(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_SUBMIT_REG, reqId, 0);
}

int npCommerce2CreateReqAuthenticate(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_AUTHENTICATE, reqId, 0);
}

int npCommerce2CreateReqRemPay(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_REM_PAY, reqId, 0);
}

int npCommerce2CreateReqGetTranHist(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_TRAN_HIST, reqId, 0);
}

int npCommerce2CreateReqGetTranDetail(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_TRAN_DETAIL, reqId, 0);
}

int npCommerce2CreateReqGetSrvs(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_SRVS, reqId, 0);
}

int npCommerce2CreateReqGetSrvDetail(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_SRV_DETAIL, reqId, 0);
}

int npCommerce2CreateReqGetSubscr(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_SUBSCR, reqId, 0);
}

int npCommerce2CreateReqChangeSubscr(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_CHANGE_SUBSCR, reqId, 0);
}

int npCommerce2CreateReqCancelSubscr(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_CANCEL_SUBSCR, reqId, 0);
}

int npCommerce2CreateReqGetCnslAct(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_CNSL_ACT, reqId, 0);
}

int npCommerce2CreateReqAcptEula(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_ACPT_EULA, reqId, 0);
}

int npCommerce2CreateReqVerifyDob(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_VERIFY_DOB, reqId, 0);
}

int npCommerce2CreateReqVerifySecAns(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_VERIFY_SEC_ANS, reqId, 0);
}

int npCommerce2CreateReqSendPassResetMail(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_SEND_PASS_RESET_MAIL, reqId, 0);
}

int npCommerce2CreateReqVerifyMasterBill(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_VERIFY_MASTER_BILL, reqId, 0);
}

int npCommerce2CreateReqGetEpw(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_EPW, reqId, 0);
}

int npCommerce2CreateReqCrGuestSess(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_CR_GUEST_SESS, reqId, 0);
}

int npCommerce2CreateReqResetPass(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_RESET_PASS, reqId, 0);
}

int npCommerce2CreateReqSendConsentMail(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_SEND_CONSENT_MAIL, reqId, 0);
}

int npCommerce2CreateReqGetProdList(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_PROD_LIST, reqId, 0);
}

int npCommerce2CreateReqRecomProds(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_RECOM_PRODS, reqId, 0);
}

int npCommerce2CreateReqVerifyBill(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_VERIFY_BILL, reqId, 0);
}

int npCommerce2CreateReqGetProdIds(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_GET_PROD_IDS, reqId, 0);
}

int npCommerce2CreateReqSetAccountLang(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_SET_ACCOUNT_LANG, reqId, 0);
}

int npCommerce2CreateReqCancelOrder(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_CANCEL_ORDER, reqId, 0);
}

int npCommerce2CreateReqCheckAccountEula(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_CHECK_ACCOUNT_EULA, reqId, 0);
}

int npCommerce2CreateReqSendEmailVer(SceUInt32 ctxId, SceUInt32 *reqId)
{
	return npCommerce2CreateRequest(ctxId, NP_COMMERCE2_OP_SEND_EMAIL_VER, reqId, 0);
}
