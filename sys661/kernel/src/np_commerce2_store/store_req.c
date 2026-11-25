#include "store_internal.h"

const char * const g_endpoint_names[86] = {
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
};
const char * const g_service_prefixes[9] = {
    "",
    "store/",
    "store/",
    "store/",
    "store/",
    "reg/",
    "cam/",
    "upgrade/",
    "pwd/",
};
const StoreFormKey g_form_keys[81] = {
    { 1, "&" },
    { 1, "," },
    { 8, "version=" },
    { 9, "username=" },
    { 9, "password=" },
    { 10, "consoleId=" },
    { 11, "categoryId=" },
    { 14, "startPosition=" },
    { 18, "maxCountOfResults=" },
    { 10, "productId=" },
    { 10, "storeType=" },
    { 9, "queryKey=" },
    { 11, "queryValue=" },
    { 18, "contentSortMethod=" },
    { 6, "skuId=" },
    { 7, "skuIds=" },
    { 8, "titleId=" },
    { 15, "drmContentType=" },
    { 14, "quickPurchase=" },
    { 12, "topupAmount=" },
    { 2, "1=" },
    { 2, "2=" },
    { 2, "3=" },
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
    { 5, "city=" },
    { 9, "province=" },
    { 11, "postalCode=" },
    { 10, "eventType=" },
    { 3, "id=" },
    { 4, "url=" },
    { 8, "country=" },
    { 9, "language=" },
    { 4, "yob=" },
    { 4, "mob=" },
    { 4, "dob=" },
    { 10, "loginName=" },
    { 17, "securityQuestion=" },
    { 15, "securityAnswer=" },
    { 9, "psHandle=" },
    { 10, "firstName=" },
    { 9, "lastName=" },
    { 15, "firstNameASCII=" },
    { 14, "lastNameASCII=" },
    { 7, "gender=" },
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
    { 9, "fromYear=" },
    { 10, "fromMonth=" },
    { 8, "fromDay=" },
    { 7, "toYear=" },
    { 8, "toMonth=" },
    { 6, "toDay=" },
    { 9, "timezone=" },
    { 14, "transactionId=" },
    { 10, "serviceId=" },
    { 5, "save=" },
    { 9, "rewardId=" },
    { 10, "rewardIds=" },
    { 11, "productIds=" },
    { 19, "recommendationType=" },
    { 10, "contentId=" },
    { 7, "fileId=" },
};
const char * const g_video_query_keys[7] = {
    "nativeText",
    "genre",
    "actor",
    "writer",
    "nativeTitle",
    "nativeDirectorProducer",
    "nativeDescription",
};
const char * const g_game_query_keys[7] = {
    "gameAll",
    "XXX",
    "XXX",
    "XXX",
    "gameTitle",
    "XXX",
    "gameDescription",
};
const char * const g_comic_query_keys[7] = {
    "comicAll",
    "XXX",
    "XXX",
    "XXX",
    "comicTitle",
    "XXX",
    "XXX",
};
const char * const g_country_table[59] = {
    "us",
    "",
    "ca",
    "mx",
    "gb",
    "ie",
    "be",
    "lu",
    "nl",
    "fr",
    "de",
    "at",
    "ch",
    "it",
    "pt",
    "dk",
    "fi",
    "no",
    "se",
    "au",
    "nz",
    "es",
    "ru",
    "ae",
    "za",
    "pl",
    "gr",
    "sa",
    "cz",
    "hk",
    "tw",
    "sg",
    "kr",
    "jp",
    "",
    "cl",
    "pe",
    "ar",
    "co",
    "br",
    "my",
    "bg",
    "hr",
    "ro",
    "si",
    "hu",
    "sk",
    "tr",
    "bh",
    "kw",
    "lb",
    "om",
    "qa",
    "il",
    "mt",
    "is",
    "cy",
    "in",
    "",
};

int sceNpCommerce2Store_42769097(int a1)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_util_fn_16(a1, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_C42B0607(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_BCD5575A(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_48();
  }
  return v3;
}


int sceNpCommerce2Store_C7AFBA23(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_util_fn_17(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_D31C1673(int a1, int a2, int a3, int a4, int a5, int a6)
{
  int v6;

  v6 = -2141909247;
  if ( g_store_init != 0 )
  {
    v6 = -2141909245;
    if ( a2 != 0 && a3 != 0 && a4 != 0 && a5 != 0 && a6 != 0 )
      return store_disp_fn_76();
  }
  return v6;
}


int sceNpCommerce2Store_5C15E8DA(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_C2A7C748(int a1, int a2)
{
  int v3;
  int v5;
  int v6;

  v3 = _sceKernelCheckThreadStack();
  v5 = -2141913084;
  if ( v3 >= 4080 )
  {
    v6 = -2141909247;
    if ( g_store_init != 0 )
    {
      v6 = -2141909245;
      if ( a2 != 0 )
        return store_util_fn_18(a1, a2);
    }
    return v6;
  }
  return v5;
}


int sceNpCommerce2Store_E3A5AC0C(int a1, int a2, int a3)
{
  return store_req_fn_6(a1, a2, a3, 1);
}


int sceNpCommerce2Store_85F7B364(int a1, int a2, int a3)
{
  return store_req_fn_6(a1, a2, a3, 0);
}


int sceNpCommerce2Store_C9652539(int a1, int a2, unsigned int a3, int a4)
{
  int v7;
  int v9;
  int v10;

  v7 = _sceKernelCheckThreadStack();
  v9 = -2141913084;
  if ( v7 >= 4080 )
  {
    v10 = -2141909247;
    if ( g_store_init != 0 )
    {
      v10 = -2141909224;
      if ( a3 > 0x3FFFF )
        return store_disp_fn_74(a1, a2, a3, a4, -2141909224, -2141913088);
    }
    return v10;
  }
  return v9;
}


int sceNpCommerce2Store_B9108E1C(int a1, int a2, int a3, int a4)
{
  int v7;
  int v9;
  int v10;

  v7 = _sceKernelCheckThreadStack();
  v9 = -2141913084;
  if ( v7 >= 4080 )
  {
    v10 = -2141909247;
    if ( g_store_init != 0 )
      return store_disp_fn_74(a1, a2, a3, a4, -2141909247);
    return v10;
  }
  return v9;
}


int sceNpCommerce2Store_20A1FDAD(int a1, int a2, int a3)
{
  int v5;
  int v7;
  int v8;

  v5 = _sceKernelCheckThreadStack();
  v7 = -2141913084;
  if ( v5 >= 4080 )
  {
    v8 = -2141909247;
    if ( g_store_init != 0 )
    {
      v8 = -2141909245;
      if ( a2 != 0 && a3 != 0 )
        return store_disp_fn_49(a1, a2, a3);
    }
    return v8;
  }
  return v7;
}


int sceNpCommerce2Store_7BBE5E6F(int a1, int a2, int a3)
{
  int v6;
  int v7;
  int v8;
  u8 v10[48];

  v6 = _sceKernelCheckThreadStack();
  v7 = -2141913084;
  if ( v6 >= 4032 )
  {
    v7 = -2141909247;
    if ( g_store_init != 0 )
    {
      v8 = _sceKernelCheckThreadStack();
      v7 = -2141913084;
      if ( v8 >= 4080 )
      {
        v7 = -2141909247;
        if ( g_store_init != 0 )
        {
          v7 = -2141909245;
          if ( a1 != 0 )
            v7 = store_disp_fn_50(a1, a2, v10);
        }
        if ( v7 >= 0 )
        {
          v7 = -2141909247;
          if ( g_store_init != 0 )
          {
            v7 = -2141909245;
            if ( a3 != 0 )
              return store_tlv_fn_45(v10, a3);
          }
        }
      }
    }
  }
  return v7;
}


int sceNpCommerce2Store_0F42E599(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_15();
  }
  return v3;
}


int sceNpCommerce2Store_F132A92F(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_15();
  }
  return v3;
}


int sceNpCommerce2Store_8C9B3738(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_4();
  }
  return v3;
}


int sceNpCommerce2Store_C6FDB930(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_15();
  }
  return v3;
}


int sceNpCommerce2Store_96E5B56F(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_49();
  }
  return v2;
}


int sceNpCommerce2Store_EF296653(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_4();
  }
  return v3;
}


int sceNpCommerce2Store_7EB1A34C(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_21();
  }
  return v2;
}


int sceNpCommerce2Store_43CF3A7A(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_31();
  }
  return v2;
}


int sceNpCommerce2Store_BAFBB9D2(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_51();
  }
  return v3;
}


int sceNpCommerce2Store_31FFC518(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_52();
  }
  return v3;
}


int sceNpCommerce2Store_E9E20646(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_14();
  }
  return v3;
}


int sceNpCommerce2Store_CC69487B(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_49();
  }
  return v2;
}


int sceNpCommerce2Store_26781C1F(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_4();
  }
  return v3;
}


int sceNpCommerce2Store_E2367A5C(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_44();
  }
  return v3;
}


int sceNpCommerce2Store_8E216CEB(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_18();
  }
  return v3;
}


int sceNpCommerce2Store_4F460A0D(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_11();
  }
  return v2;
}


int sceNpCommerce2Store_4F945E73(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_47();
  }
  return v3;
}


int sceNpCommerce2Store_3370450A(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_49();
  }
  return v2;
}


int sceNpCommerce2Store_7411D8E7(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_44();
  }
  return v3;
}


int sceNpCommerce2Store_9E63F7D5(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_50();
  }
  return v3;
}


int sceNpCommerce2Store_D830992C(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_13();
  }
  return v3;
}


int sceNpCommerce2Store_94E55872(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_51();
  }
  return v3;
}


int sceNpCommerce2Store_E4E600A5(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_49();
  }
  return v2;
}


int sceNpCommerce2Store_93A4DBE4(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_52();
  }
  return v3;
}


int sceNpCommerce2Store_E1B3A08D(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_14();
  }
  return v3;
}


int sceNpCommerce2Store_A47A19C1(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_44();
  }
  return v3;
}


int sceNpCommerce2Store_1FBC9F13(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_10();
  }
  return v2;
}


int sceNpCommerce2Store_4C78B957(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_12();
  }
  return v2;
}


int sceNpCommerce2Store_0DD765EC(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_25();
  }
  return v2;
}


int sceNpCommerce2Store_47DC8C9F(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_util_fn_20(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_DA223D9D(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_7B0C0787(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_51();
  }
  return v3;
}


int sceNpCommerce2Store_B7CD88D7(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_40();
  }
  return v3;
}


int sceNpCommerce2Store_264EAA5B(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_util_fn_22(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_31B3119F(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( (a2 | a3) != 0 )
      return store_util_fn_23();
  }
  return v3;
}


int sceNpCommerce2Store_861ACADC(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_00E9235B(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_52();
  }
  return v3;
}


int sceNpCommerce2Store_FBE5903B(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_53();
  }
  return v3;
}


int sceNpCommerce2Store_6CD2ED8C(int a1)
{
  int v2;
  int v3;
  unsigned int v4;
  int v5;

  v2 = -2141909245;
  if ( a1 != 0 )
  {
    v3 = a1;
    v4 = 0;
    while ( 1 )
    {
      v5 = sceNet_lib_BCBE14CF(v3, 45);
      ++v4;
      v2 = -2141909245;
      if ( v5 == 0 )
        break;
      v3 = v5 + 1;
      if ( v4 >= 2 )
        return v3 - a1 - 1;
    }
  }
  return v2;
}


int sceNpCommerce2Store_6E216646(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_util_fn_24(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_DA570F5D(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909098;
    if ( (unsigned int)(a2 - 1) < 8 )
      return store_util_fn_25();
  }
  return v2;
}


int sceNpCommerce2Store_B953E742(int a1, int a2, int a3)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a3 != 0 )
      return store_disp_fn_77(a1, a2, a3, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_39D8C7D9(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_util_fn_26(a1, a2, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_3F18219A(int a1, int a2, int a3, int a4, unsigned int a5)
{
  const char *v7;
  int v8;
  const char *v11;

  if ( a1 == 0 || a2 == 0 || a3 == 0 || a4 == 0 )
    return -2141909245;
  switch ( *(u32 *)(a3 + 84) )
  {
    case 1:
      v7 = "IP9101-NPIA90005";
      break;
    case 2:
      v7 = "IP9102-NPIA90006";
      break;
    case 3:
      v7 = "IP9103-NPIA90007";
      break;
    case 4:
      v7 = "IP9104-NPIA90008";
      break;
    case 5:
      v7 = "IP9105-NPIA90009";
      break;
    default:
      return -2141909245;
  }
  v11 = v7;
  v8 = sceNet_lib_D8722983(v7);
  if ( v8 + sceNet_lib_D8722983("_01--STORE") < a5 )
  {
    sceNet_lib_CF705E46(a4, "%s%s", v11, "_01--STORE");
    return 0;
  }
  return -2141909116;
}


int store_req_fn_1(int a1, int a2, unsigned int a3, int a4, int a5)
{
  int v10;
  int v11;
  int v12;
  int v13;
  int v15;

  if ( sceKernelGetCompiledSdkVersion() <= 84213775
    || (v15 = _sceKernelCheckThreadStack(), v13 = -2141913084, v15 >= 14320) )
  {
    v10 = -2141909247;
    if ( g_store_init != 0 )
    {
      v12 = sceKernelGetCompiledSdkVersion() > 84213775;
      v10 = -2141913088;
      if ( !v12 )
        return store_disp_fn_74(a1, a2, a3, a4, v11, v10);
      v10 = -2141909224;
      if ( a5 != 1 || a3 > 0x3FFFF )
        return store_disp_fn_74(a1, a2, a3, a4, v11, v10);
    }
    return v10;
  }
  return v13;
}


int store_req_fn_2(int a1, u8 *a2, int a3, int a4, int a5, unsigned int a6, int a7)
{
  int v13;
  u8 *v14;
  int v15;
  int v17;
  char *i;
  char v19;
  int v20;
  unsigned int v21;
  int v22;
  char v23[32];
  u8 *v24;

  v24 = a2;
  if ( sceKernelGetCompiledSdkVersion() <= 84213775
    || (v22 = _sceKernelCheckThreadStack(), v15 = -2141913084, v22 >= 14320) )
  {
    v13 = -2141909247;
    if ( g_store_init != 0 )
    {
      if ( v24 != NULL && (a7 != 1 || sceKernelGetCompiledSdkVersion() <= 84213775 || a6 < 0x3D) )
      {
        if ( a7 == 1 )
        {
          v17 = sceNp_2E4769C3();
          v14 = v24;
          if ( v17 != 0 )
          {
            v14 = v24;
            if ( "GetCategoryContents" != NULL && &v24 != NULL )
            {
              i = v24;
              if ( *v24 != 0 && *v24 != 0x2D )
              {
                for ( i = v24 + 1; ; ++i )
                {
                  v19 = *i;
                  if ( *i == 0 || *i == 0x2D )
                    break;
                }
              }
              else
              {
                v19 = *v24;
              }
              v20 = v19;
              if ( v19 != 0 )
                v20 = *++i;
              while ( v20 != 0 && v20 != 0x2D )
                v20 = *++i;
              v21 = (unsigned int)((const char *)i - (const char *)v24);
              if ( v21 > 0x1F )
                v21 = 31;
              sceKernelMemcpy(v23, v24, v21);
              v23[v21] = 0;
              KprintfForUser("%s: %s\n", "GetCategoryContents", v23);
              v14 = v24;
            }
          }
        }
        else
        {
          v14 = v24;
        }
        return store_disp_fn_75(a1, v14, a3, a4, a5, a6);
      }
      else
      {
        return -2141909245;
      }
    }
    return v13;
  }
  return v15;
}


int store_req_fn_3(int a1, int a2, unsigned int a3, int a4, int a5)
{
  int v10;
  int v11;
  int v12;
  int v13;
  int v15;

  if ( sceKernelGetCompiledSdkVersion() <= 84213775
    || (v15 = _sceKernelCheckThreadStack(), v13 = -2141913084, v15 >= 14320) )
  {
    v10 = -2141909247;
    if ( g_store_init != 0 )
    {
      v12 = sceKernelGetCompiledSdkVersion() > 84213775;
      v10 = -2141913088;
      if ( !v12 )
        return store_disp_fn_74(a1, a2, a3, a4, v11, v10);
      v10 = -2141909224;
      if ( a5 != 1 || a3 > 0x3FFFF )
        return store_disp_fn_74(a1, a2, a3, a4, v11, v10);
    }
    return v10;
  }
  return v13;
}


int store_req_fn_4(int a1, int a2, u8 *a3, int a4, int a5, int a6)
{
  unsigned int v11;
  int v12;
  u8 *v13;
  int v14;
  int v16;
  char *i;
  char v18;
  int v19;
  unsigned int v20;
  int v21;
  char v22[32];
  u8 *v23;

  v23 = a3;
  if ( sceKernelGetCompiledSdkVersion() <= 84213775
    || (v21 = _sceKernelCheckThreadStack(), v14 = -2141913084, v21 >= 14320) )
  {
    v11 = 0x20000u;
    v12 = -2141909247;
    if ( g_store_init != 0 )
    {
      v13 = v23;
      v12 = -2141909245;
      if ( v23 != NULL )
      {
        if ( a6 == 1 )
        {
          v16 = sceNp_2E4769C3();
          v13 = v23;
          if ( v16 != 0 && "GetProductInfo" != NULL && &v23 != NULL )
          {
            i = v23;
            if ( *v23 != 0 && *v23 != 0x2D )
            {
              for ( i = v23 + 1; ; ++i )
              {
                v18 = *i;
                if ( *i == 0 || *i == 0x2D )
                  break;
              }
            }
            else
            {
              v18 = *v23;
            }
            v19 = v18;
            if ( v18 != 0 )
              v19 = *++i;
            while ( v19 != 0 && v19 != 0x2D )
              v19 = *++i;
            v20 = (unsigned int)((const char *)i - (const char *)v23);
            if ( v20 > 0x1F )
              v20 = 31;
            sceKernelMemcpy(v22, v23, v20);
            v22[v20] = 0;
            KprintfForUser("%s: %s\n", "GetProductInfo", v22);
            v13 = v23;
          }
        }
        return store_disp_fn_79(a1, a2, v13, a4, a5, v11);
      }
    }
    return v12;
  }
  return v14;
}


int store_req_fn_5(int a1, int a2, unsigned int a3, int a4, int a5)
{
  int v10;
  int v11;
  int v12;
  int v13;
  int v15;

  if ( sceKernelGetCompiledSdkVersion() <= 84213775
    || (v15 = _sceKernelCheckThreadStack(), v13 = -2141913084, v15 >= 14320) )
  {
    v10 = -2141909247;
    if ( g_store_init != 0 )
    {
      v12 = sceKernelGetCompiledSdkVersion() > 84213775;
      v10 = -2141913088;
      if ( !v12 )
        return store_disp_fn_74(a1, a2, a3, a4, v11, v10);
      v10 = -2141909224;
      if ( a5 != 1 || a3 > 0x3FFFF )
        return store_disp_fn_74(a1, a2, a3, a4, v11, v10);
    }
    return v10;
  }
  return v13;
}


int store_req_fn_6(int a1, u8 **a2, unsigned int a3, int a4)
{
  int v8;
  int v9;
  int v10;
  int v12;
  u8 **v13;
  int v14;
  char *v15;
  char *v16;
  char v17;
  int v18;
  unsigned int v19;
  unsigned int v20;
  char **v21;
  char *v22;
  char *v23;
  char v24;
  int v25;
  unsigned int v26;
  char v27[32];

  v8 = _sceKernelCheckThreadStack();
  v9 = -2141913084;
  if ( v8 >= 4080 )
  {
    v10 = -2141909247;
    if ( g_store_init == 0 )
      return v10;
    if ( a2 == NULL || a3 == 0 || a4 == 1 && a3 >= 0x3D )
      return -2141909245;
    if ( a4 == 1 )
    {
      v14 = sceNp_2E4769C3();
      v12 = a1;
      if ( v14 != 0 )
      {
        v13 = a2;
        if ( "GetProductInfoList" == NULL || a2 == NULL || a3 == 0 )
          return store_util_fn_19(v12, v13, a3);
        v15 = *a2;
        v16 = *a2;
        if ( **a2 != 0 && **a2 != 0x2D )
        {
          ++v16;
          while ( 1 )
          {
            v17 = *v16;
            if ( *v16 == 0 || *v16 == 0x2D )
              break;
            ++v16;
          }
        }
        else
        {
          v17 = *v15;
        }
        v18 = v17;
        if ( v17 != 0 )
          v18 = *++v16;
        while ( 1 )
        {
          v19 = v16 - v15;
          if ( v18 == 0 || v18 == 0x2D )
            break;
          v18 = *++v16;
        }
        if ( v19 > 0x1F )
          v19 = 31;
        sceKernelMemcpy(v27, v15, v19);
        v27[v19] = 0;
        if ( a3 < 2 )
        {
          KprintfForUser("%s: %s\n", "GetProductInfoList", v27);
          v12 = a1;
        }
        else
        {
          v20 = 1;
          KprintfForUser("%s: %s", "GetProductInfoList", v27);
          v21 = a2 + 1;
          do
          {
            v22 = *v21;
            v23 = *v21;
            if ( **v21 != 0 && **v21 != 0x2D )
            {
              ++v23;
              while ( 1 )
              {
                v24 = *v23;
                if ( *v23 == 0 || *v23 == 0x2D )
                  break;
                ++v23;
              }
            }
            else
            {
              v24 = *v22;
            }
            v25 = v24;
            if ( v24 != 0 )
              v25 = *++v23;
            while ( v25 != 0 && v25 != 0x2D )
              v25 = *++v23;
            v26 = v23 - v22;
            if ( v26 > 0x1F )
              v26 = 31;
            sceKernelMemcpy(v27, v22, v26);
            v27[v26] = 0;
            ++v20;
            KprintfForUser(", %s", v27);
            ++v21;
          }
          while ( v20 < a3 );
          KprintfForUser("\n");
          v12 = a1;
        }
      }
    }
    else
    {
      v12 = a1;
    }
    v13 = a2;
    return store_util_fn_19(v12, v13, a3);
  }
  return v9;
}


int store_req_fn_7(int a1, unsigned int a2, u32 *a3, u32 *a4)
{
  int v4;
  int v5;
  int v6;
  int v11;

  v4 = a2;
  v5 = a2 < 0x57u;
  v6 = -2141909119;
  if ( v5 )
  {
    v6 = -2141909098;
    if ( (unsigned int)(a1 - 1) < 8 )
    {
      v6 = sceNpCommerce2_460A1D92(&v11, 9);
      if ( v6 >= 0 )
      {
        *a3 = *(g_service_prefixes + a1);
        if ( a1 == 7 )
        {
          if ( v4 == 31 )
          {
            v4 = 74;
          }
          else if ( v4 == 48 )
          {
            v4 = 75;
          }
        }
        *a4 = g_endpoint_names[v4 - 1];
      }
    }
  }
  return v6;
}


int store_req_fn_8(int a1, int a2, int a3, unsigned int a4, int *a5, int a6, int a7, int a8, int a9, int a10)
{
  int v12;
  unsigned int v16;
  int i;
  int result;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;

  v25 = a2;
  v26 = a3;
  v21 = 0;
  v22 = 0;
  v12 = a4;
  v16 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = store_req_fn_35(v16 != 0, i, &v21, a1, a6, a7, a8, a9, a10);
    if ( result < 0 )
      break;
    if ( v16 != 0 )
    {
      ++v16;
    }
    else
    {
      v24 = 0;
      v23 = 0;
      v19 = v21;
      v20 = v22;
      result = store_req_fn_7(a1, v12, &v23, &v24);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v25, a1, v12, v23, v24, 0, v26, a5, v19, v20);
      v16 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v16 >= 2 )
      return 0;
  }
  return result;
}


int store_req_fn_9(int a1, int a2, int a3, unsigned int a4, int *a5, int a6, int a7, int a8, int a9)
{
  int v11;
  unsigned int v15;
  int i;
  int result;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;

  v24 = a2;
  v25 = a3;
  v20 = 0;
  v21 = 0;
  v11 = a4;
  v15 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = store_req_fn_36(v15 != 0, i, &v20, a1, a6, a7, a8, a9);
    if ( result < 0 )
      break;
    if ( v15 != 0 )
    {
      ++v15;
    }
    else
    {
      v23 = 0;
      v22 = 0;
      v18 = v20;
      v19 = v21;
      result = store_req_fn_7(a1, v11, &v22, &v23);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v24, a1, v11, v22, v23, 0, v25, a5, v18, v19);
      v15 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v15 >= 2 )
      return 0;
  }
  return result;
}


int store_req_fn_10(int a1, int a2, int a3, unsigned int a4, int *a5, int a6, int a7, int a8, int a9)
{
  int v11;
  unsigned int v15;
  int i;
  int result;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;

  v24 = a2;
  v25 = a3;
  v20 = 0;
  v21 = 0;
  v11 = a4;
  v15 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = store_req_fn_37(v15 != 0, i, &v20, a1, a6, a7, a8, a9);
    if ( result < 0 )
      break;
    if ( v15 != 0 )
    {
      ++v15;
    }
    else
    {
      v23 = 0;
      v22 = 0;
      v18 = v20;
      v19 = v21;
      result = store_req_fn_7(a1, v11, &v22, &v23);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v24, a1, v11, v22, v23, 0, v25, a5, v18, v19);
      v15 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v15 >= 2 )
      return 0;
  }
  return result;
}


int store_req_fn_11(int a1, int a2, int a3, unsigned int a4, int *a5, int a6)
{
  s64 v7;
  int v8;
  unsigned int v11;
  int v12;
  int v13;
  int v14;
  int result;
  u8 v16[784];
  int v17;
  int v18;
  int v19;
  int v20;

  v19 = a2;
  v20 = a3;
  v7 = 0;
  v8 = a4;
  v11 = 0;
  v12 = a6;
  while ( 1 )
  {
    v14 = *a5;
    v13 = sceNet_lib_D8722983(v12);
    result = sceNpCommerce2_66EC0829(v16, 769, a6, v13, 0);
    if ( result >= 0 )
    {
      result = store_req_fn_33(v11 != 0, v14, 14, v16, 0);
      if ( result >= 0 )
      {
        if ( v11 == 0 )
          v7 = result;
        result = 0;
      }
    }
    if ( result < 0 )
      break;
    if ( v11 != 0 )
    {
      ++v11;
    }
    else
    {
      v18 = 0;
      v17 = 0;
      result = store_req_fn_7(a1, v8, &v17, &v18);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v19, a1, v8, v17, v18, 0, v20, a5, v7, HIDWORD(v7));
      v11 = 1;
      if ( result < 0 )
        return result;
    }
    v12 = a6;
    if ( v11 >= 2 )
      return 0;
  }
  return result;
}


s32 store_req_fn_12(int a1, int a2, int a3, unsigned int a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = store_req_fn_7(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return store_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


s32 store_req_fn_13(int a1, int a2, int a3, unsigned int a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = store_req_fn_7(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return store_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


int store_req_fn_14(int a1, int a2, int a3, unsigned int a4, int *a5, int a6, int a7, int a8)
{
  int v13;
  unsigned int v14;
  int i;
  int result;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;

  v23 = a2;
  v24 = a3;
  v19 = 0;
  v20 = 0;
  v13 = a4;
  v14 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = store_req_fn_38(v14 != 0, i, &v19, v13, a6, a7, a8);
    if ( result < 0 )
      break;
    if ( v14 != 0 )
    {
      ++v14;
    }
    else
    {
      v22 = 0;
      v21 = 0;
      v17 = v19;
      v18 = v20;
      result = store_req_fn_7(a1, v13, &v21, &v22);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v23, a1, v13, v21, v22, 0, v24, a5, v17, v18);
      v14 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v14 >= 2 )
      return 0;
  }
  return result;
}


s32 store_req_fn_15(int a1, int a2, int a3, unsigned int a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = store_req_fn_7(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return store_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


s32 store_req_fn_16(int a1, int a2, int a3, unsigned int a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = store_req_fn_7(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return store_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


int store_req_fn_17(int a1, int a2, int a3, unsigned int a4, u32 *a5, int a6)
{
  int v7;
  s64 v8;
  unsigned int v11;
  int result;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;

  v17 = a2;
  v18 = a3;
  v7 = a4;
  v8 = 0;
  v11 = 0;
  while ( 1 )
  {
    result = -2141909118;
    if ( a6 != 0 )
      break;
    v13 = store_req_fn_33(v11 != 0, *a5, 25, "CC", 0);
    v14 = v13;
    if ( v13 >= 0 )
    {
      if ( v11 == 0 )
        v8 = v13;
      v14 = 0;
    }
    result = v14;
    if ( v14 < 0 )
      break;
    if ( v11 != 0 )
    {
      ++v11;
    }
    else
    {
      v16 = 0;
      v15 = 0;
      result = store_req_fn_7(a1, v7, &v15, &v16);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v17, a1, v7, v15, v16, 0, v18, a5, v8, HIDWORD(v8));
      v11 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v11 >= 2 )
      return 0;
  }
  return result;
}


int store_req_fn_18(
        int a1,
        int a2,
        int a3,
        unsigned int a4,
        int *a5,
        int a6,
        int a7,
        int a8,
        int a9,
        int a10,
        int a11,
        int a12)
{
  int v13;
  unsigned int v18;
  int i;
  int result;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;

  v27 = a2;
  v28 = a3;
  v23 = 0;
  v24 = 0;
  v13 = a4;
  v18 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = store_req_fn_39(v18 != 0, i, &v23, a6, a7, a8, a9, a10, a11, a12);
    if ( result < 0 )
      break;
    if ( v18 != 0 )
    {
      ++v18;
    }
    else
    {
      v26 = 0;
      v25 = 0;
      v21 = v23;
      v22 = v24;
      result = store_req_fn_7(a1, v13, &v25, &v26);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v27, a1, v13, v25, v26, 0, v28, a5, v21, v22);
      v18 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v18 >= 2 )
      return 0;
  }
  return result;
}


s32 store_req_fn_19(int a1, int a2, int a3, unsigned int a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = store_req_fn_7(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return store_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


int store_req_fn_20(int a1, int a2, int a3, unsigned int a4, int *a5, int a6)
{
  int v9;
  unsigned int v12;
  int i;
  int result;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;

  v17 = 0;
  v18 = 0;
  v9 = a4;
  v12 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = store_req_fn_40(v12 != 0, i, &v17, 0, a6, 0, 0);
    if ( result < 0 )
      break;
    if ( v12 != 0 )
    {
      ++v12;
    }
    else
    {
      v20 = 0;
      v19 = 0;
      v15 = v17;
      v16 = v18;
      result = store_req_fn_7(a1, v9, &v19, &v20);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(a2, a1, v9, v19, v20, 0, a3, a5, v15, v16);
      v12 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v12 >= 2 )
      return 0;
  }
  return result;
}


int store_req_fn_21(int a1, int a2, int a3, unsigned int a4, int *a5, int a6)
{
  s64 v7;
  int v8;
  unsigned int v11;
  int v12;
  int v13;
  int v14;
  int result;
  u8 v16[784];
  int v17;
  int v18;
  int v19;
  int v20;

  v19 = a2;
  v20 = a3;
  v7 = 0;
  v8 = a4;
  v11 = 0;
  v12 = a6;
  while ( 1 )
  {
    v14 = *a5;
    v13 = sceNet_lib_D8722983(v12);
    result = sceNpCommerce2_66EC0829(v16, 769, a6, v13, 0);
    if ( result >= 0 )
    {
      result = store_req_fn_33(v11 != 0, v14, 37, v16, 0);
      if ( result >= 0 )
      {
        if ( v11 == 0 )
          v7 = result;
        result = 0;
      }
    }
    if ( result < 0 )
      break;
    if ( v11 != 0 )
    {
      ++v11;
    }
    else
    {
      v18 = 0;
      v17 = 0;
      result = store_req_fn_7(a1, v8, &v17, &v18);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v19, a1, v8, v17, v18, 0, v20, a5, v7, HIDWORD(v7));
      v11 = 1;
      if ( result < 0 )
        return result;
    }
    v12 = a6;
    if ( v11 >= 2 )
      return 0;
  }
  return result;
}


s32 store_req_fn_22(int a1, int a2, int a3, unsigned int a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = store_req_fn_7(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return store_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


int store_req_fn_23(int a1, int a2, int a3, unsigned int a4, int *a5, int a6)
{
  s64 v6;
  int v8;
  unsigned int v11;
  int i;
  int result;
  int v14;
  int v15;
  int v16;
  int v17;

  v16 = a2;
  v17 = a3;
  v6 = 0;
  v8 = a4;
  v11 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = store_req_fn_34(v11 != 0, i, 19, a6, 0);
    if ( result >= 0 )
    {
      if ( v11 == 0 )
        v6 = result;
      result = 0;
    }
    if ( result < 0 )
      break;
    if ( v11 != 0 )
    {
      ++v11;
    }
    else
    {
      v15 = 0;
      v14 = 0;
      result = store_req_fn_7(a1, v8, &v14, &v15);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v16, a1, v8, v14, v15, 0, v17, a5, v6, HIDWORD(v6));
      v11 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v11 >= 2 )
      return 0;
  }
  return result;
}


int store_req_fn_24(int a1, int a2, int a3, unsigned int a4, int *a5, int a6, int a7)
{
  int v9;
  unsigned int v13;
  int i;
  int result;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;

  v22 = a2;
  v18 = 0;
  v19 = 0;
  v9 = a4;
  v13 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = store_req_fn_41(v13 != 0, i, &v18, a6, a7);
    if ( result < 0 )
      break;
    if ( v13 != 0 )
    {
      ++v13;
    }
    else
    {
      v21 = 0;
      v20 = 0;
      v16 = v18;
      v17 = v19;
      result = store_req_fn_7(a1, v9, &v20, &v21);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v22, a1, v9, v20, v21, 0, a3, a5, v16, v17);
      v13 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v13 >= 2 )
      return 0;
  }
  return result;
}


int store_req_fn_25(int a1, int a2, int a3, unsigned int a4, int *a5, int a6, int a7, int a8, int a9)
{
  int v11;
  unsigned int v15;
  int i;
  int result;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;

  v24 = a2;
  v25 = a3;
  v20 = 0;
  v21 = 0;
  v11 = a4;
  v15 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = store_req_fn_42(v15 != 0, i, &v20, a6, a7, a8, a9);
    if ( result < 0 )
      break;
    if ( v15 != 0 )
    {
      ++v15;
    }
    else
    {
      v23 = 0;
      v22 = 0;
      v18 = v20;
      v19 = v21;
      result = store_req_fn_7(a1, v11, &v22, &v23);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v24, a1, v11, v22, v23, 0, v25, a5, v18, v19);
      v15 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v15 >= 2 )
      return 0;
  }
  return result;
}


int store_req_fn_26(int a1, int a2, int a3, unsigned int a4, int *a5, int a6, int a7, int a8)
{
  int v10;
  unsigned int v14;
  int i;
  int result;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;

  v23 = a2;
  v24 = a3;
  v19 = 0;
  v20 = 0;
  v10 = a4;
  v14 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = store_req_fn_43(v14 != 0, i, &v19, a1, a6, a7, a8);
    if ( result < 0 )
      break;
    if ( v14 != 0 )
    {
      ++v14;
    }
    else
    {
      v22 = 0;
      v21 = 0;
      v17 = v19;
      v18 = v20;
      result = store_req_fn_7(a1, v10, &v21, &v22);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v23, a1, v10, v21, v22, 0, v24, a5, v17, v18);
      v14 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v14 >= 2 )
      return 0;
  }
  return result;
}


int store_req_fn_27(int a1, int a2, int a3, unsigned int a4, int *a5, int a6, int a7, int a8, int a9, int a10)
{
  int v12;
  unsigned int v16;
  int i;
  int result;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;

  v25 = a2;
  v26 = a3;
  v21 = 0;
  v22 = 0;
  v12 = a4;
  v16 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = store_req_fn_44(v16 != 0, i, &v21, a1, a6, a7, a8, a9, a10);
    if ( result < 0 )
      break;
    if ( v16 != 0 )
    {
      ++v16;
    }
    else
    {
      v24 = 0;
      v23 = 0;
      v19 = v21;
      v20 = v22;
      result = store_req_fn_7(a1, v12, &v23, &v24);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v25, a1, v12, v23, v24, 0, v26, a5, v19, v20);
      v16 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v16 >= 2 )
      return 0;
  }
  return result;
}


s32 store_req_fn_28(int a1, int a2, int a3, unsigned int a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = store_req_fn_7(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return store_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


int store_req_fn_29(
        int a1,
        int a2,
        int a3,
        unsigned int a4,
        int *a5,
        int a6,
        int a7,
        int a8,
        int a9,
        int a10,
        int a11,
        int a12)
{
  int v13;
  unsigned int v18;
  int i;
  int result;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;

  v27 = a2;
  v28 = a3;
  v23 = 0;
  v24 = 0;
  v13 = a4;
  v18 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = store_req_fn_45(v18 != 0, i, &v23, a6, a7, a8, a9, a10, a11, a12);
    if ( result < 0 )
      break;
    if ( v18 != 0 )
    {
      ++v18;
    }
    else
    {
      v26 = 0;
      v25 = 0;
      v21 = v23;
      v22 = v24;
      result = store_req_fn_7(a1, v13, &v25, &v26);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v27, a1, v13, v25, v26, 0, v28, a5, v21, v22);
      v18 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v18 >= 2 )
      return 0;
  }
  return result;
}


int store_req_fn_30(int a1, int a2, int a3, unsigned int a4, int *a5, int a6, int a7)
{
  int v9;
  unsigned int v13;
  int i;
  int result;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;

  v22 = a2;
  v18 = 0;
  v19 = 0;
  v9 = a4;
  v13 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = store_tlv_fn_1(v13 != 0, i, &v18, a1, a6, a7);
    if ( result < 0 )
      break;
    if ( v13 != 0 )
    {
      ++v13;
    }
    else
    {
      v21 = 0;
      v20 = 0;
      v16 = v18;
      v17 = v19;
      result = store_req_fn_7(a1, v9, &v20, &v21);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v22, a1, v9, v20, v21, 0, a3, a5, v16, v17);
      v13 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v13 >= 2 )
      return 0;
  }
  return result;
}


int store_req_fn_31(int a1, int a2, int a3, unsigned int a4, int *a5, int a6, int a7, int a8, int a9, int a10)
{
  int v12;
  unsigned int v16;
  int i;
  int result;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;

  v25 = a2;
  v26 = a3;
  v21 = 0;
  v22 = 0;
  v12 = a4;
  v16 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = store_tlv_fn_2(v16 != 0, i, &v21, a1, a6, a7, a8, a9, a10);
    if ( result < 0 )
      break;
    if ( v16 != 0 )
    {
      ++v16;
    }
    else
    {
      v24 = 0;
      v23 = 0;
      v19 = v21;
      v20 = v22;
      result = store_req_fn_7(a1, v12, &v23, &v24);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v25, a1, v12, v23, v24, 0, v26, a5, v19, v20);
      v16 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v16 >= 2 )
      return 0;
  }
  return result;
}


int store_req_fn_32(int a1, int a2, int a3, unsigned int a4, int *a5, int a6, int a7)
{
  int v9;
  unsigned int v13;
  int i;
  int result;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;

  v22 = a2;
  v18 = 0;
  v19 = 0;
  v9 = a4;
  v13 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = store_tlv_fn_3(v13 != 0, i, &v18, a6, a7);
    if ( result < 0 )
      break;
    if ( v13 != 0 )
    {
      ++v13;
    }
    else
    {
      v21 = 0;
      v20 = 0;
      v16 = v18;
      v17 = v19;
      result = store_req_fn_7(a1, v9, &v20, &v21);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v22, a1, v9, v20, v21, 0, a3, a5, v16, v17);
      v13 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v13 >= 2 )
      return 0;
  }
  return result;
}


int store_req_fn_33(int a1, int a2, int a3, int a4, int a5)
{
  int result;
  int v9;

  if ( a1 != 1 )
    return (a5 == 1) + g_form_keys[a3].len + sceNet_lib_D8722983(a4);
  if ( a5 != 1 || (result = sceNpCommerce2_AE614CC6(a2, "&", 1)) >= 0 )
  {
    result = sceNpCommerce2_AE614CC6(a2, g_form_keys[a3].str, g_form_keys[a3].len);
    if ( result >= 0 )
    {
      v9 = sceNet_lib_D8722983(a4);
      return sceNpCommerce2_AE614CC6(a2, a4, v9);
    }
  }
  return result;
}


int store_req_fn_34(int a1, int a2, int a3, int a4, int a5)
{
  int v5;
  int v6;
  int v7;
  int v9;

  if ( a1 == 1 )
    return store_tlv_fn_8(a2, a3, a4, a5);
  v6 = (a5 == 1) + g_form_keys[a3].len;
  v5 = sceNet_lib_CF705E46(&v9, (const char *)"%u", a4, a5, a2);
  v7 = -2141909231;
  if ( v5 < 16 )
    return v5 + v6;
  return v7;
}


int store_req_fn_35(int a1, int a2, u32 *a3, int a4, int a5, u32 *a6, int a7, int a8, int a9)
{
  int v9;
  int v10;
  int v16;
  int v17;
  s64 v18;
  int v19;
  int v20;
  s64 v21;
  int v22;
  int v23;
  s64 v24;
  int v25;
  int v26;
  u32 *v27;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  int v34;
  int v35;
  int v36;
  u8 v37[784];
  u8 v38[16];
  u32 *v39;
  int v40;

  v9 = a7 != 0;
  v10 = a6 != NULL;
  v39 = a3;
  v40 = a8;
  if ( v10 && v9 )
    a4 = 1;
  sceNet_lib_CF705E46(v37, (const char *)"%u", a4);
  if ( a1 == 1 )
  {
    v35 = sceNpCommerce2_AE614CC6(a2, "storeType=", 10);
    if ( v35 < 0 )
      return v35;
    v36 = sceNet_lib_D8722983(v37);
    v16 = sceNpCommerce2_AE614CC6(a2, v37, v36);
  }
  else
  {
    v16 = sceNet_lib_D8722983(v37) + 10;
  }
  v17 = v16;
  if ( v16 >= 0 )
  {
    v18 = v16;
    v19 = sceNet_lib_D8722983(a5);
    v17 = sceNpCommerce2_66EC0829(v37, 769, a5, v19, 0);
    if ( v17 >= 0 )
    {
      if ( a1 == 1 )
      {
        v17 = sceNpCommerce2_AE614CC6(a2, "&", 1);
        if ( v17 < 0 )
          return v17;
        v33 = sceNpCommerce2_AE614CC6(a2, "categoryId=", 11);
        if ( v33 < 0 )
          return v33;
        v34 = sceNet_lib_D8722983(v37);
        v20 = sceNpCommerce2_AE614CC6(a2, v37, v34);
      }
      else
      {
        v20 = sceNet_lib_D8722983(v37) + 12;
      }
      v17 = v20;
      if ( v20 >= 0 )
      {
        v21 = v18 + v20;
        if ( !v10 || !v9 )
        {
LABEL_11:
          if ( a1 == 1 )
          {
            v23 = store_tlv_fn_8(a2, 7, v40, 1);
          }
          else
          {
            v22 = sceNet_lib_CF705E46(v38, (const char *)"%u", v40);
            v23 = -2141909231;
            if ( v22 < 16 )
              v23 = 12 + v22 + 3;
          }
          v17 = v23;
          if ( v23 >= 0 )
          {
            v24 = v21 + v23;
            if ( a1 == 1 )
            {
              v26 = store_tlv_fn_8(a2, 8, a9, 1);
            }
            else
            {
              v25 = sceNet_lib_CF705E46(v38, (const char *)"%u", a9);
              v26 = -2141909231;
              if ( v25 < 16 )
                v26 = 16 + v25 + 3;
            }
            v17 = v26;
            if ( v26 >= 0 )
            {
              v24 = ((u64)(v24) & 0xFFFFFFFF00000000ULL) | (u32)(v24 + v26);
              if ( a1 != 1 )
              {
                v27 = v39;
                *v39 = v24;
                v27[1] = HIDWORD(v24) + (v26 >> 31) + ((unsigned int)v24 < v26);
              }
              return 0;
            }
          }
          return v17;
        }
        v29 = sceNet_lib_D8722983(*a6);
        v17 = sceNpCommerce2_66EC0829(v37, 769, *a6, v29, 0);
        if ( v17 >= 0 )
        {
          if ( a1 == 1 )
          {
            v17 = sceNpCommerce2_AE614CC6(a2, "&", 1);
            if ( v17 < 0 )
              return v17;
            v31 = sceNpCommerce2_AE614CC6(a2, "rewardId=", 9);
            if ( v31 < 0 )
              return v31;
            v32 = sceNet_lib_D8722983(v37);
            v30 = sceNpCommerce2_AE614CC6(a2, v37, v32);
          }
          else
          {
            v30 = sceNet_lib_D8722983(v37) + 10;
          }
          v17 = v30;
          if ( v30 >= 0 )
          {
            v21 += v30;
            goto LABEL_11;
          }
        }
      }
    }
  }
  return v17;
}


int store_req_fn_36(int a1, int a2, u32 *a3, int a4, u8 *a5, int a6, u32 *a7, int a8)
{
  int v8;
  int v15;
  int v16;
  s64 v17;
  int v18;
  int v19;
  s64 v20;
  int v21; // dc
  u32 *v22;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  int v34;
  int v35;
  u8 v36[784];
  u32 *v37;
  int v38;

  v8 = a7 != NULL;
  v37 = a3;
  v38 = a8 != 0;
  if ( v8 && v38 )
    a4 = 1;
  sceNet_lib_CF705E46(v36, (const char *)"%u", a4);
  if ( a1 == 1 )
  {
    v34 = sceNpCommerce2_AE614CC6(a2, "storeType=", 10);
    if ( v34 < 0 )
      return v34;
    v35 = sceNet_lib_D8722983(v36);
    v15 = sceNpCommerce2_AE614CC6(a2, v36, v35);
  }
  else
  {
    v15 = sceNet_lib_D8722983(v36) + 10;
  }
  v16 = v15;
  if ( v15 < 0 )
    return v16;
  v17 = v15;
  v18 = sceNet_lib_D8722983(a6);
  v16 = sceNpCommerce2_66EC0829(v36, 769, a6, v18, 0);
  if ( v16 < 0 )
    return v16;
  if ( a1 == 1 )
  {
    v32 = sceNpCommerce2_AE614CC6(a2, "&", 1);
    if ( v32 < 0 )
      return v32;
    v16 = sceNpCommerce2_AE614CC6(a2, "productId=", 10);
    if ( v16 < 0 )
      return v16;
    v33 = sceNet_lib_D8722983(v36);
    v19 = sceNpCommerce2_AE614CC6(a2, v36, v33);
  }
  else
  {
    v19 = sceNet_lib_D8722983(v36) + 11;
  }
  v16 = v19;
  if ( v19 < 0 )
    return v16;
  v20 = v17 + v19;
  if ( a5 != NULL && *a5 != 0 )
  {
    v28 = sceNet_lib_D8722983(a5);
    v16 = sceNpCommerce2_66EC0829(v36, 769, a5, v28, 0);
    if ( v16 < 0 )
      return v16;
    if ( a1 == 1 )
    {
      v30 = sceNpCommerce2_AE614CC6(a2, "&", 1);
      if ( v30 < 0 )
        return v30;
      v16 = sceNpCommerce2_AE614CC6(a2, "categoryId=", 11);
      if ( v16 < 0 )
        return v16;
      v31 = sceNet_lib_D8722983(v36);
      v29 = sceNpCommerce2_AE614CC6(a2, v36, v31);
    }
    else
    {
      v29 = sceNet_lib_D8722983(v36) + 12;
    }
    v16 = v29;
    if ( v29 < 0 )
      return v16;
    v20 += v29;
  }
  if ( !v8 || !v38 )
  {
LABEL_13:
    v21 = a1 == 1;
    v22 = v37;
    if ( !v21 )
    {
      *v37 = v20;
      v22[1] = HIDWORD(v20);
    }
    return 0;
  }
  v24 = sceNet_lib_D8722983(*a7);
  v16 = sceNpCommerce2_66EC0829(v36, 769, *a7, v24, 0);
  if ( v16 >= 0 )
  {
    if ( a1 == 1 )
    {
      v26 = sceNpCommerce2_AE614CC6(a2, "&", 1);
      if ( v26 < 0 )
        return v26;
      v16 = sceNpCommerce2_AE614CC6(a2, "rewardId=", 9);
      if ( v16 < 0 )
        return v16;
      v27 = sceNet_lib_D8722983(v36);
      v25 = sceNpCommerce2_AE614CC6(a2, v36, v27);
    }
    else
    {
      v25 = sceNet_lib_D8722983(v36) + 10;
    }
    v16 = v25;
    if ( v25 >= 0 )
    {
      v20 += v25;
      goto LABEL_13;
    }
  }
  return v16;
}


int store_req_fn_37(int a1, int a2, u64 *a3, int a4, int a5, int a6, u8 **a7, unsigned int a8)
{
  int v15;
  int v16;
  s64 v17;
  int v18;
  s64 v19;
  int v21;
  u8 **v22;
  u8 *i;
  int v24;
  int v25;
  int v26;
  u8 v27[784];

  sceNet_lib_CF705E46(v27, (const char *)"%u", a4);
  if ( a1 == 1 )
  {
    v25 = sceNpCommerce2_AE614CC6(a2, "storeType=", 10);
    if ( v25 < 0 )
      return v25;
    v26 = sceNet_lib_D8722983(v27);
    v15 = sceNpCommerce2_AE614CC6(a2, v27, v26);
  }
  else
  {
    v15 = sceNet_lib_D8722983(v27) + 10;
  }
  v16 = v15;
  if ( v15 >= 0 )
  {
    v17 = v15;
    v18 = a1 == 1 ? store_tlv_fn_5(a2, 14, a5, a6, v27, 769, 0, 1) : store_tlv_fn_9(14, a5, a6, v27, 769, 0, 1);
    v16 = v18;
    if ( v18 >= 0 )
    {
      v19 = v17 + v18;
      if ( a7 == NULL || a8 == 0 || a6 != a8 )
        goto LABEL_9;
      v21 = 0;
      if ( a8 != 0 )
      {
        v22 = a7;
        for ( i = *a7; ; i = *v22 )
        {
          ++v22;
          if ( i != NULL && *i != 0 )
            break;
          if ( ++v21 >= a8 )
            break;
        }
      }
      if ( v21 == a8 )
        goto LABEL_9;
      if ( a1 == 1 )
        v24 = store_tlv_fn_5(a2, 76, a7, a8, v27, 769, 0, 1);
      else
        v24 = store_tlv_fn_9(76, a7, a8, v27, 769, 0, 1);
      v16 = v24;
      if ( v24 >= 0 )
      {
        v19 += v24;
LABEL_9:
        v16 = 0;
        if ( a1 != 1 )
          *a3 = v19;
      }
    }
  }
  return v16;
}


int store_req_fn_38(int a1, int a2, u32 *a3, int a4, u8 *a5, int a6, int a7)
{
  s64 v12;
  int v13;
  const char *v15;
  int v16;
  int v17;
  s64 v18;
  const char *v19;
  int v20;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  u8 v29[784];

  v12 = 0;
  v13 = 0;
  if ( a5 != NULL && *a5 != 0 )
  {
    v25 = sceNet_lib_D8722983(a5);
    v17 = sceNpCommerce2_66EC0829(v29, 769, a5, v25, 0);
    if ( v17 < 0 )
      return v17;
    if ( a1 == 1 )
    {
      v27 = sceNpCommerce2_AE614CC6(a2, "password=", 9);
      if ( v27 < 0 )
        return v27;
      v28 = sceNet_lib_D8722983(v29);
      v26 = sceNpCommerce2_AE614CC6(a2, v29, v28);
    }
    else
    {
      v26 = sceNet_lib_D8722983(v29) + 9;
    }
    v17 = v26;
    if ( v26 < 0 )
      return v17;
    v13 = 1;
    v12 = v26;
  }
  v15 = "true";
  if ( a6 != 1 )
    v15 = "false";
  if ( a1 == 1 )
  {
    if ( v13 != 1 || (v16 = sceNpCommerce2_AE614CC6(a2, "&", 1)) >= 0 )
    {
      v23 = sceNpCommerce2_AE614CC6(a2, "quickPurchase=", 14);
      if ( v23 < 0 )
        return v23;
      v24 = sceNet_lib_D8722983(v15);
      v16 = sceNpCommerce2_AE614CC6(a2, v15, v24);
    }
  }
  else
  {
    v16 = v13 + sceNet_lib_D8722983(v15) + 14;
  }
  v17 = v16;
  if ( v16 >= 0 )
  {
    v18 = v12 + v16;
    v19 = "true";
    if ( a7 != 1 )
      v19 = "false";
    if ( a1 == 1 )
    {
      v20 = sceNpCommerce2_AE614CC6(a2, "&", 1);
      if ( v20 >= 0 )
      {
        v17 = sceNpCommerce2_AE614CC6(a2, "autoDepositFlag=", 16);
        if ( v17 < 0 )
          return v17;
        v22 = sceNet_lib_D8722983(v19);
        v20 = sceNpCommerce2_AE614CC6(a2, v19, v22);
      }
    }
    else
    {
      v20 = sceNet_lib_D8722983(v19) + 17;
    }
    v17 = v20;
    if ( v20 >= 0 )
    {
      v18 = ((u64)(v18) & 0xFFFFFFFF00000000ULL) | (u32)(v18 + v20);
      if ( a1 != 1 )
      {
        *a3 = v18;
        a3[1] = HIDWORD(v18) + (v20 >> 31) + ((unsigned int)v18 < v20);
      }
      return 0;
    }
  }
  return v17;
}


int store_req_fn_39(int a1, int a2, u64 *a3, int a4, int a5, u8 *a6, int a7, int a8, int a9, int a10)
{
  s64 v15;
  int v16;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  s64 v23;
  int v24;
  s64 v25;
  int v26;
  s64 v27;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  int v34;
  u8 v35[784];
  u8 v36[16];

  v15 = 0;
  v16 = 0;
  if ( a4 != 0 && a5 != 0 )
  {
    if ( a1 == 1 )
      v18 = store_tlv_fn_5(a2, 15, a4, a5, v35, 769, 0, 0);
    else
      v18 = store_tlv_fn_9(15, a4, a5, v35, 769, 0, 0);
    v19 = v18;
    if ( v18 < 0 )
      return v19;
    v16 = 1;
    v15 = v18;
  }
  if ( a6 != NULL && *a6 != 0 )
  {
    v31 = sceNet_lib_D8722983(a6);
    v19 = sceNpCommerce2_66EC0829(v35, 769, a6, v31, 0);
    if ( v19 < 0 )
      return v19;
    if ( a1 == 1 )
    {
      if ( v16 != 1 || (v32 = sceNpCommerce2_AE614CC6(a2, "&", 1)) >= 0 )
      {
        v33 = sceNpCommerce2_AE614CC6(a2, "serviceId=", 10);
        if ( v33 < 0 )
          return v33;
        v34 = sceNet_lib_D8722983(v35);
        v32 = sceNpCommerce2_AE614CC6(a2, v35, v34);
      }
    }
    else
    {
      v32 = v16 + sceNet_lib_D8722983(v35) + 10;
    }
    v19 = v32;
    if ( v32 < 0 )
      return v19;
    v15 += v32;
    v16 = 1;
  }
  if ( a7 == 0 )
  {
LABEL_14:
    if ( a1 == 1 )
    {
      v19 = store_tlv_fn_8(a2, 7, a8, v16);
    }
    else
    {
      v22 = sceNet_lib_CF705E46(v36, (const char *)"%u", a8);
      v19 = -2141909231;
      if ( v22 < 16 )
        v19 = v22 + v16 + 14;
    }
    if ( v19 >= 0 )
    {
      v23 = v15 + v19;
      if ( a1 == 1 )
      {
        v19 = store_tlv_fn_8(a2, 8, a9, 1);
      }
      else
      {
        v24 = sceNet_lib_CF705E46(v36, (const char *)"%u", a9);
        v19 = -2141909231;
        if ( v24 < 16 )
          v19 = 16 + v24 + 3;
      }
      if ( v19 >= 0 )
      {
        v25 = v23 + v19;
        if ( a1 == 1 )
        {
          v19 = store_tlv_fn_8(a2, 17, a10, 1);
        }
        else
        {
          v26 = sceNet_lib_CF705E46(v36, (const char *)"%u", a10);
          v19 = -2141909231;
          if ( v26 < 16 )
            v19 = 16 + v26;
        }
        if ( v19 >= 0 )
        {
          v27 = v25 + v19;
          if ( a1 != 1 )
            *a3 = v27;
          return 0;
        }
      }
    }
    return v19;
  }
  v20 = sceNet_lib_D8722983(a7);
  v19 = sceNpCommerce2_66EC0829(v35, 769, a7, v20, 0);
  if ( v19 >= 0 )
  {
    if ( a1 == 1 )
    {
      if ( v16 != 1 || (v21 = sceNpCommerce2_AE614CC6(a2, "&", 1)) >= 0 )
      {
        v29 = sceNpCommerce2_AE614CC6(a2, "rewardIds=", 10);
        if ( v29 < 0 )
          return v29;
        v30 = sceNet_lib_D8722983(v35);
        v21 = sceNpCommerce2_AE614CC6(a2, v35, v30);
      }
    }
    else
    {
      v21 = v16 + sceNet_lib_D8722983(v35) + 10;
    }
    v19 = v21;
    if ( v21 >= 0 )
    {
      v15 += v21;
      v16 = 1;
      goto LABEL_14;
    }
  }
  return v19;
}


int store_req_fn_40(int a1, int a2, u32 *a3, int a4, int a5, int a6, int a7)
{
  s64 v8;
  int v9;
  const char *v12;
  u8 *v13;
  u8 *v14;
  int v15;
  int v16;
  int v17;
  int v18;
  u8 *v19;
  int v20;
  int v21;
  u8 *v22;
  u8 *v23;
  u8 *v24;
  u8 *v25;
  int v26;
  u8 *v27;
  int v28;
  u8 *v29;
  int v30;
  u8 *v31;
  int v32;
  u8 *v33;
  int v34;
  int v35;
  int v36;
  int v37; // dc
  u32 *v38;
  const char *v40;
  int v41;
  int v42;
  int v43;
  int v44;
  int v45;
  int v46;
  int v47;
  u32 *v48;
  int v49;
  int v50;
  int v51;
  unsigned int v52;
  u32 *v53;
  int v54;
  int v55;
  int v56;
  unsigned int v57;
  int *v58;
  int v59;
  int v60;
  int v61;
  unsigned int v62;
  u32 *v63;
  int v64;
  int v65;
  int v66;
  unsigned int v67;
  int *v68;
  int v69;
  int v70;
  int v71;
  unsigned int v72;
  u32 *v73;
  int v74;
  int v75;
  int v76;
  int v77;
  int v78;
  int v79;
  int v80;
  int v81;
  int v82;
  int v83;
  int v84;
  int v85;
  int v86;
  int v87;
  int v88;
  int v89;
  int v90;
  int v91;
  int v92;
  int v93;
  int v94;
  int v95;
  int v96;
  u8 v97[784];
  u32 v98[8];
  u8 v99[16];
  u8 v100[16];
  u8 v101[16];
  u8 v102[16];
  int v103;
  u32 *v104;
  int v105;
  int v106;
  int v107;
  int v108;
  int v109;
  int v110;
  int v111;

  v103 = a2;
  v104 = a3;
  v8 = 0;
  v9 = 0;
  v105 = a6;
  v106 = a7;
  sceKernelMemset(v98, 0, 32);
  v12 = "creditCard.";
  if ( a4 != 0 )
  {
    if ( a4 != 1 )
      goto LABEL_3;
    v12 = "account.";
  }
  v98[0] = v12;
LABEL_3:
  v13 = *(u8 **)a5;
  v98[1] = "address.";
  v98[2] = "address";
  if ( v13 == NULL )
    goto LABEL_14;
  if ( *v13 != 0 )
  {
    v15 = *(u32 *)(a5 + 4);
    if ( v15 == 0 )
    {
      v14 = *(u8 **)(a5 + 8);
      goto LABEL_15;
    }
    v16 = *(u32 *)(a5 + 4);
    if ( v13[v15 - 1] == 0 )
      v16 = v15 - 1;
    if ( a1 == 1 )
      v17 = store_tlv_fn_4(v103, v98, 1, 26, v13, v16, 0);
    else
      v17 = sceNet_lib_D8722983(v98[0]) + v16 + 16;
    v18 = v17;
    if ( v17 < 0 )
      return v18;
    v9 = 1;
    v8 = v17;
LABEL_14:
    v14 = *(u8 **)(a5 + 8);
    goto LABEL_15;
  }
  v14 = *(u8 **)(a5 + 8);
LABEL_15:
  if ( v14 != NULL )
  {
    if ( *v14 != 0 )
    {
      v20 = *(u32 *)(a5 + 12);
      if ( v20 != 0 )
      {
        v94 = sceNpCommerce2_66EC0829(v97, 769, v14, v20, 0);
        v18 = v94;
        if ( v94 < 0 )
          return v18;
        v95 = sceNet_lib_D8722983(v97);
        v96 = a1 == 1 ? store_tlv_fn_4(v103, v98, 1, 27, v97, v95, v9) : v9 + sceNet_lib_D8722983(v98[0]) + v95 + 11;
        v18 = v96;
        if ( v96 < 0 )
          return v18;
        v8 += v96;
        v9 = 1;
      }
      v19 = *(u8 **)(a5 + 16);
    }
    else
    {
      v19 = *(u8 **)(a5 + 16);
    }
  }
  else
  {
    v19 = *(u8 **)(a5 + 16);
  }
  if ( v19 != NULL )
  {
    if ( *v19 != 0 )
    {
      v92 = sceNet_lib_D8722983(v19);
      if ( a1 == 1 )
        v93 = store_tlv_fn_4(v103, v98, 1, 28, v19, v92, v9);
      else
        v93 = v9 + sceNet_lib_D8722983(v98[0]) + v92 + 11;
      v18 = v93;
      if ( v93 < 0 )
        return v18;
      v8 += v93;
      v9 = 1;
    }
    v21 = *(u32 *)(a5 + 20);
  }
  else
  {
    v21 = *(u32 *)(a5 + 20);
  }
  if ( v21 != 0 )
  {
    v89 = sceNet_lib_CF705E46(v99, (const char *)"%u");
    v18 = -2141909231;
    if ( v89 >= 16 )
      return v18;
    v90 = sceNet_lib_D8722983(v99);
    v91 = a1 == 1 ? store_tlv_fn_4(v103, v98, 1, 29, v99, v90, v9) : v9 + sceNet_lib_D8722983(v98[0]) + v90 + 10;
    v18 = v91;
    if ( v91 < 0 )
      return v18;
    v8 += v91;
    v9 = 1;
  }
  if ( *(u32 *)(a5 + 24) != 0 )
  {
    v86 = sceNet_lib_CF705E46(v100, (const char *)"%u");
    v18 = -2141909231;
    if ( v86 >= 16 )
      return v18;
    v87 = sceNet_lib_D8722983(v100);
    v88 = a1 == 1 ? store_tlv_fn_4(v103, v98, 1, 30, v100, v87, v9) : v9 + sceNet_lib_D8722983(v98[0]) + v87 + 11;
    v18 = v88;
    if ( v88 < 0 )
      return v18;
    v8 += v88;
    v9 = 1;
  }
  if ( *(u32 *)(a5 + 28) != 0 )
  {
    v83 = sceNet_lib_CF705E46(v101, (const char *)"%u");
    v18 = -2141909231;
    if ( v83 >= 16 )
      return v18;
    v84 = sceNet_lib_D8722983(v101);
    v85 = a1 == 1 ? store_tlv_fn_4(v103, v98, 1, 31, v101, v84, v9) : v9 + sceNet_lib_D8722983(v98[0]) + v84 + 11;
    v18 = v85;
    if ( v85 < 0 )
      return v18;
    v8 += v85;
    v9 = 1;
  }
  if ( *(u32 *)(a5 + 32) != 0 )
  {
    v80 = sceNet_lib_CF705E46(v102, (const char *)"%u");
    v18 = -2141909231;
    if ( v80 >= 16 )
      return v18;
    v81 = sceNet_lib_D8722983(v102);
    v82 = a1 == 1 ? store_tlv_fn_4(v103, v98, 1, 32, v102, v81, v9) : v9 + sceNet_lib_D8722983(v98[0]) + 12 + v81;
    v18 = v82;
    if ( v82 < 0 )
      return v18;
    v8 += v82;
    v9 = 1;
  }
  v22 = *(u8 **)(a5 + 36);
  if ( v22 != NULL )
  {
    if ( *v22 != 0 )
    {
      v78 = sceNet_lib_D8722983(*(u32 *)(a5 + 36));
      if ( a1 == 1 )
        v79 = store_tlv_fn_4(v103, v98, 1, 33, v22, v78, v9);
      else
        v79 = v9 + sceNet_lib_D8722983(v98[0]) + 13 + v78;
      v18 = v79;
      if ( v79 < 0 )
        return v18;
      v8 += v79;
      v9 = 1;
    }
    v23 = *(u8 **)(a5 + 40);
  }
  else
  {
    v23 = *(u8 **)(a5 + 40);
  }
  if ( v23 != NULL )
  {
    if ( *v23 != 0 )
    {
      v76 = sceNet_lib_D8722983(v23);
      if ( a1 == 1 )
        v77 = store_tlv_fn_4(v103, v98, 1, 34, v23, v76, v9);
      else
        v77 = v9 + sceNet_lib_D8722983(v98[0]) + v76 + 12;
      v18 = v77;
      if ( v77 < 0 )
        return v18;
      v8 += v77;
      v9 = 1;
    }
    v24 = *(u8 **)(a5 + 44);
  }
  else
  {
    v24 = *(u8 **)(a5 + 44);
  }
  if ( v24 != NULL )
  {
    if ( *v24 != 0 )
    {
      v26 = *(u32 *)(a5 + 48);
      if ( v26 != 0 )
      {
        v18 = sceNpCommerce2_66EC0829(v97, 769, v24, v26, 0);
        if ( v18 < 0 )
          return v18;
        v107 = sceNet_lib_D8722983(v97);
        v71 = v9;
        v72 = 0;
        v73 = v98;
        if ( a1 == 1 )
        {
          v75 = store_tlv_fn_4(v103, v98, 3, 20, v97, v107, v9);
        }
        else
        {
          do
          {
            ++v72;
            v74 = sceNet_lib_D8722983(*v73++);
            v71 += v74;
          }
          while ( v72 < 3 );
          v75 = v71 + v107 + 2;
        }
        v18 = v75;
        if ( v75 < 0 )
          return v18;
        v8 += v75;
        v9 = 1;
      }
      v25 = *(u8 **)(a5 + 52);
    }
    else
    {
      v25 = *(u8 **)(a5 + 52);
    }
  }
  else
  {
    v25 = *(u8 **)(a5 + 52);
  }
  if ( v25 != NULL )
  {
    if ( *v25 != 0 )
    {
      v28 = *(u32 *)(a5 + 56);
      if ( v28 != 0 )
      {
        v18 = sceNpCommerce2_66EC0829(v97, 769, v25, v28, 0);
        if ( v18 < 0 )
          return v18;
        v108 = sceNet_lib_D8722983(v97);
        v66 = v9;
        v67 = 0;
        v68 = v98;
        if ( a1 == 1 )
        {
          v70 = store_tlv_fn_4(v103, v98, 3, 21, v97, v108, v9);
        }
        else
        {
          do
          {
            v69 = *v68;
            ++v67;
            ++v68;
            v66 += sceNet_lib_D8722983(v69);
          }
          while ( v67 < 3 );
          v70 = v66 + v108 + 2;
        }
        v18 = v70;
        if ( v70 < 0 )
          return v18;
        v8 += v70;
        v9 = 1;
      }
      v27 = *(u8 **)(a5 + 60);
    }
    else
    {
      v27 = *(u8 **)(a5 + 60);
    }
  }
  else
  {
    v27 = *(u8 **)(a5 + 60);
  }
  if ( v27 != NULL )
  {
    if ( *v27 != 0 )
    {
      v30 = *(u32 *)(a5 + 64);
      if ( v30 != 0 )
      {
        v18 = sceNpCommerce2_66EC0829(v97, 769, v27, v30, 0);
        if ( v18 < 0 )
          return v18;
        v109 = sceNet_lib_D8722983(v97);
        v61 = v9;
        v62 = 0;
        v63 = v98;
        if ( a1 == 1 )
        {
          v65 = store_tlv_fn_4(v103, v98, 3, 22, v97, v109, v9);
        }
        else
        {
          do
          {
            ++v62;
            v64 = sceNet_lib_D8722983(*v63++);
            v61 += v64;
          }
          while ( v62 < 3 );
          v65 = v61 + v109 + 2;
        }
        v18 = v65;
        if ( v65 < 0 )
          return v18;
        v8 += v65;
        v9 = 1;
      }
      v29 = *(u8 **)(a5 + 68);
    }
    else
    {
      v29 = *(u8 **)(a5 + 68);
    }
  }
  else
  {
    v29 = *(u8 **)(a5 + 68);
  }
  if ( v29 != NULL )
  {
    if ( *v29 != 0 )
    {
      v32 = *(u32 *)(a5 + 72);
      if ( v32 != 0 )
      {
        v18 = sceNpCommerce2_66EC0829(v97, 769, v29, v32, 0);
        if ( v18 < 0 )
          return v18;
        v110 = sceNet_lib_D8722983(v97);
        v56 = v9;
        v57 = 0;
        v58 = v98;
        if ( a1 == 1 )
        {
          v60 = store_tlv_fn_4(v103, v98, 2, 35, v97, v110, v9);
        }
        else
        {
          do
          {
            v59 = *v58;
            ++v57;
            ++v58;
            v56 += sceNet_lib_D8722983(v59);
          }
          while ( v57 < 2 );
          v60 = v56 + v110 + 5;
        }
        v18 = v60;
        if ( v60 < 0 )
          return v18;
        v8 += v60;
        v9 = 1;
      }
      v31 = *(u8 **)(a5 + 76);
    }
    else
    {
      v31 = *(u8 **)(a5 + 76);
    }
  }
  else
  {
    v31 = *(u8 **)(a5 + 76);
  }
  if ( v31 != NULL )
  {
    if ( *v31 != 0 )
    {
      v34 = *(u32 *)(a5 + 80);
      if ( v34 != 0 )
      {
        v18 = sceNpCommerce2_66EC0829(v97, 769, v31, v34, 0);
        if ( v18 < 0 )
          return v18;
        v111 = sceNet_lib_D8722983(v97);
        v51 = v9;
        v52 = 0;
        v53 = v98;
        if ( a1 == 1 )
        {
          v55 = store_tlv_fn_4(v103, v98, 2, 36, v97, v111, v9);
        }
        else
        {
          do
          {
            ++v52;
            v54 = sceNet_lib_D8722983(*v53++);
            v51 += v54;
          }
          while ( v52 < 2 );
          v55 = v51 + v111 + 9;
        }
        v18 = v55;
        if ( v55 < 0 )
          return v18;
        v8 += v55;
        v9 = 1;
      }
      v33 = *(u8 **)(a5 + 84);
    }
    else
    {
      v33 = *(u8 **)(a5 + 84);
    }
  }
  else
  {
    v33 = *(u8 **)(a5 + 84);
  }
  v35 = v105;
  if ( v33 != NULL )
  {
    v35 = v105;
    if ( *v33 != 0 )
    {
      v36 = *(u32 *)(a5 + 88);
      if ( v36 != 0 )
      {
        v18 = sceNpCommerce2_66EC0829(v97, 769, v33, v36, 0);
        if ( v18 < 0 )
          return v18;
        v44 = sceNet_lib_D8722983(v97);
        v45 = v44;
        v46 = v9;
        v47 = 0;
        v48 = v98;
        if ( a1 == 1 )
        {
          v50 = store_tlv_fn_4(v103, v98, 2, 37, v97, v44, v9);
        }
        else
        {
          do
          {
            ++v47;
            v49 = sceNet_lib_D8722983(*v48++);
            v46 += v49;
          }
          while ( v98 != NULL );
          v50 = v46 + v45 + 11;
        }
        v18 = v50;
        if ( v50 < 0 )
          return v18;
        v8 += v50;
        v9 = 1;
      }
      v35 = v105;
    }
  }
  if ( v35 != 1 )
  {
LABEL_73:
    v37 = a1 == 1;
    v38 = v104;
    if ( !v37 )
    {
      *v104 = v8;
      v38[1] = HIDWORD(v8);
    }
    return 0;
  }
  v40 = "true";
  if ( v106 != 1 )
    v40 = "false";
  if ( a1 == 1 )
  {
    if ( v9 != 1 || (v41 = sceNpCommerce2_AE614CC6(v103, "&", 1)) >= 0 )
    {
      v42 = sceNpCommerce2_AE614CC6(v103, "save=", 5);
      if ( v42 < 0 )
        return v42;
      v43 = sceNet_lib_D8722983(v40);
      v41 = sceNpCommerce2_AE614CC6(v103, v40, v43);
    }
  }
  else
  {
    v41 = v9 + sceNet_lib_D8722983(v40) + 5;
  }
  v18 = v41;
  if ( v41 >= 0 )
  {
    v8 += v41;
    goto LABEL_73;
  }
  return v18;
}


int store_req_fn_41(int a1, int a2, u64 *a3, int a4, int a5)
{
  int v10;
  int v11;
  int v12;
  const char *v13;
  s64 v14;
  int v15;
  s64 v16;
  int v18;
  int v19;
  int v20;
  u8 v21[784];

  v10 = sceNet_lib_D8722983(a4);
  v11 = sceNpCommerce2_66EC0829(v21, 769, a4, v10, 0);
  if ( v11 >= 0 )
  {
    if ( a1 == 1 )
    {
      v11 = sceNpCommerce2_AE614CC6(a2, "edyReceiptId=", 13);
      if ( v11 < 0 )
        return v11;
      v20 = sceNet_lib_D8722983(v21);
      v12 = sceNpCommerce2_AE614CC6(a2, v21, v20);
    }
    else
    {
      v12 = sceNet_lib_D8722983(v21) + 13;
    }
    v11 = v12;
    if ( v12 >= 0 )
    {
      v13 = "OK";
      v14 = v12;
      if ( a5 != 1 )
        v13 = "NG";
      if ( a1 == 1 )
      {
        v18 = sceNpCommerce2_AE614CC6(a2, "&", 1);
        if ( v18 < 0 )
          return v18;
        v11 = sceNpCommerce2_AE614CC6(a2, "resultOfTopupFromEdy=", 21);
        if ( v11 < 0 )
          return v11;
        v19 = sceNet_lib_D8722983(v13);
        v15 = sceNpCommerce2_AE614CC6(a2, v13, v19);
      }
      else
      {
        v15 = sceNet_lib_D8722983(v13) + 22;
      }
      v11 = v15;
      if ( v15 >= 0 )
      {
        v16 = v14 + v15;
        if ( a1 != 1 )
          *a3 = v16;
        return 0;
      }
    }
  }
  return v11;
}


int store_req_fn_42(int a1, int a2, u32 *a3, int a4, int a5, int a6, int a7)
{
  int v14;
  int v15;
  int v16;
  int v17;
  s64 v18;
  int v19;
  int v20;
  int v21;
  int v22;
  s64 v23;
  int v24;
  int v25;
  s64 v26;
  const char *v27;
  int v28;
  int v29;
  int v30;
  int v32;
  u8 v33[784];
  u32 v34[4];

  sceKernelMemset(v34, 0, 4);
  v34[0] = "voucherCode";
  v14 = sceNet_lib_D8722983(a4);
  v15 = sceNpCommerce2_66EC0829(v33, 769, a4, v14, 0);
  if ( v15 >= 0 )
  {
    v16 = sceNet_lib_D8722983(v33);
    v17 = a1 == 1 ? store_tlv_fn_4(a2, v34, 1, 20, v33, v16, 0) : sceNet_lib_D8722983(v34[0]) + v16 + 2;
    v15 = v17;
    if ( v17 >= 0 )
    {
      v18 = v17;
      v19 = sceNet_lib_D8722983(a5);
      v15 = sceNpCommerce2_66EC0829(v33, 769, a5, v19, 0);
      if ( v15 >= 0 )
      {
        v20 = sceNet_lib_D8722983(v33);
        v21 = a1 == 1 ? store_tlv_fn_4(a2, v34, 1, 21, v33, v20, 1) : sceNet_lib_D8722983(v34[0]) + 1 + v20 + 2;
        v15 = v21;
        if ( v21 >= 0 )
        {
          v23 = v18 + v21;
          v22 = sceNet_lib_D8722983(a6);
          v15 = sceNpCommerce2_66EC0829(v33, 769, a6, v22, 0);
          if ( v15 >= 0 )
          {
            v24 = sceNet_lib_D8722983(v33);
            v25 = a1 == 1 ? store_tlv_fn_4(a2, v34, 1, 22, v33, v24, 1) : sceNet_lib_D8722983(v34[0]) + 1 + v24 + 2;
            v15 = v25;
            if ( v25 >= 0 )
            {
              v26 = v23 + v25;
              v27 = "true";
              if ( a7 != 1 )
                v27 = "false";
              if ( a1 == 1 )
              {
                v15 = sceNpCommerce2_AE614CC6(a2, "&", 1);
                if ( v15 < 0 )
                  return v15;
                v15 = sceNpCommerce2_AE614CC6(a2, "autoDepositFlag=", 16);
                if ( v15 < 0 )
                  return v15;
                v32 = sceNet_lib_D8722983(v27);
                v28 = sceNpCommerce2_AE614CC6(a2, v27, v32);
              }
              else
              {
                v28 = sceNet_lib_D8722983(v27) + 17;
              }
              v15 = v28;
              if ( v28 >= 0 )
              {
                v26 = ((u64)(v26) & 0xFFFFFFFF00000000ULL) | (u32)(v26 + v28);
                v29 = (unsigned int)v26 < v28;
                v30 = HIDWORD(v26) + (v28 >> 31);
                if ( a1 != 1 )
                {
                  *a3 = v26;
                  a3[1] = v30 + v29;
                }
                return 0;
              }
            }
          }
        }
      }
    }
  }
  return v15;
}


int store_req_fn_43(int a1, int a2, u64 *a3, int a4, int a5, int a6, u8 *a7)
{
  int v13;
  int v14;
  int v15;
  s64 v16;
  int v17;
  s64 v18;
  int v19;
  s64 v20;
  int v21;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  u8 v30[16];

  sceNet_lib_CF705E46(v30, (const char *)"%u", a4);
  if ( a1 == 1 )
  {
    v14 = sceNpCommerce2_AE614CC6(a2, "storeType=", 10);
    if ( v14 < 0 )
      return v14;
    v29 = sceNet_lib_D8722983(v30);
    v13 = sceNpCommerce2_AE614CC6(a2, v30, v29);
  }
  else
  {
    v13 = sceNet_lib_D8722983(v30) + 10;
  }
  v14 = v13;
  if ( v13 >= 0 )
  {
    v15 = a5;
    v16 = v13;
    sceNet_lib_CF705E46(v30, (const char *)"%u", v15);
    if ( a1 == 1 )
    {
      v27 = sceNpCommerce2_AE614CC6(a2, "&", 1);
      if ( v27 < 0 )
        return v27;
      v14 = sceNpCommerce2_AE614CC6(a2, "eventType=", 10);
      if ( v14 < 0 )
        return v14;
      v28 = sceNet_lib_D8722983(v30);
      v17 = sceNpCommerce2_AE614CC6(a2, v30, v28);
    }
    else
    {
      v17 = sceNet_lib_D8722983(v30) + 11;
    }
    v14 = v17;
    if ( v17 >= 0 )
    {
      v18 = v16 + v17;
      if ( a1 == 1 )
      {
        v25 = sceNpCommerce2_AE614CC6(a2, "&", 1);
        if ( v25 < 0 )
          return v25;
        v14 = sceNpCommerce2_AE614CC6(a2, "id=", 3);
        if ( v14 < 0 )
          return v14;
        v26 = sceNet_lib_D8722983(a6);
        v19 = sceNpCommerce2_AE614CC6(a2, a6, v26);
      }
      else
      {
        v19 = sceNet_lib_D8722983(a6) + 4;
      }
      v14 = v19;
      if ( v19 >= 0 )
      {
        v20 = v18 + v19;
        if ( a7 == NULL || *a7 == 0 )
        {
LABEL_16:
          v14 = 0;
          if ( a1 != 1 )
            *a3 = v20;
          return v14;
        }
        if ( a1 == 1 )
        {
          v23 = sceNpCommerce2_AE614CC6(a2, "&", 1);
          if ( v23 < 0 )
            return v23;
          v14 = sceNpCommerce2_AE614CC6(a2, "url=", 4);
          if ( v14 < 0 )
            return v14;
          v24 = sceNet_lib_D8722983(a7);
          v21 = sceNpCommerce2_AE614CC6(a2, a7, v24);
        }
        else
        {
          v21 = sceNet_lib_D8722983(a7) + 5;
        }
        v14 = v21;
        if ( v21 >= 0 )
        {
          v20 += v21;
          goto LABEL_16;
        }
      }
    }
  }
  return v14;
}


int store_req_fn_44(int a1, int a2, u32 *a3, unsigned int a4, int a5, int a6, u32 *a7, unsigned int a8)
{
  int v12;
  int v13;
  s64 v14;
  int v15;
  int v16;
  s64 v17;
  int v18;
  int v19;
  s64 v20;
  char **v21;
  unsigned int v22;
  char *v23;
  u32 *v24;
  char *v25;
  int v26;
  int v27;
  s64 v28;
  unsigned int v29;
  char *v30;
  u32 *v31;
  int v32;
  char *v33;
  u32 *v34;
  int v36;
  int v37;
  u8 v38[784];
  u8 v39[16];
  int v40;
  u32 *v41;
  int v42;
  u32 *v43;

  v40 = a2;
  v41 = a3;
  v42 = a6;
  v43 = a7;
  sceNet_lib_CF705E46(v38, (const char *)"%u", a4);
  if ( a1 == 1 )
  {
    v36 = sceNpCommerce2_AE614CC6(v40, "storeType=", 10);
    if ( v36 < 0 )
      return v36;
    v37 = sceNet_lib_D8722983(v38);
    v12 = sceNpCommerce2_AE614CC6(v40, v38, v37);
  }
  else
  {
    v12 = sceNet_lib_D8722983(v38) + 10;
  }
  v13 = v12;
  if ( v12 >= 0 )
  {
    v14 = v12;
    if ( a1 == 1 )
    {
      v16 = store_tlv_fn_8(v40, 7, a5, 1);
    }
    else
    {
      v15 = sceNet_lib_CF705E46(v39, (const char *)"%u", a5);
      v16 = -2141909231;
      if ( v15 < 16 )
        v16 = 12 + v15 + 3;
    }
    v13 = v16;
    if ( v16 >= 0 )
    {
      v17 = v14 + v16;
      if ( a1 == 1 )
      {
        v19 = store_tlv_fn_8(v40, 8, v42, 1);
      }
      else
      {
        v18 = sceNet_lib_CF705E46(v39, (const char *)"%u", v42);
        v19 = -2141909231;
        if ( v18 < 16 )
          v19 = 16 + v18 + 3;
      }
      v13 = v19;
      if ( v19 >= 0 )
      {
        v20 = v17 + v19;
        if ( a4 != 2 )
        {
          if ( a4 >= 3 )
          {
            if ( a4 != 3 )
            {
              v21 = g_comic_query_keys;
              if ( a4 == 4 )
                goto LABEL_16;
              goto LABEL_15;
            }
          }
          else if ( a4 != 1 )
          {
            goto LABEL_15;
          }
          v21 = g_game_query_keys;
          goto LABEL_16;
        }
LABEL_15:
        v21 = g_video_query_keys;
LABEL_16:
        if ( a1 == 1 )
        {
          v27 = store_tlv_fn_7(v40, 11, v43, v21, a8, v38, 769, 1);
        }
        else
        {
          v22 = 0;
          v23 = (char *)10;
          if ( a8 != 0 )
          {
            v24 = v43;
            while ( 1 )
            {
              v25 = &v23[v22++ != 0];
              v26 = sceNet_lib_D8722983(v21[*v24]);
              v27 = sceNpCommerce2_66EC0829(v38, 769, v21[*v24], v26, 0);
              v24 += 3;
              if ( v27 < 0 )
                break;
              v23 = &v25[sceNet_lib_D8722983(v38)];
              if ( v22 >= a8 )
                goto LABEL_21;
            }
          }
          else
          {
LABEL_21:
            v27 = (int)v23;
          }
        }
        v13 = v27;
        if ( v27 >= 0 )
        {
          v28 = v20 + v27;
          if ( a1 == 1 )
          {
            v32 = store_tlv_fn_6(v40, 12, v43, a8, v38, 769, 1);
          }
          else
          {
            v29 = 0;
            v30 = (char *)12;
            if ( a8 != 0 )
            {
              v31 = v43;
              while ( 1 )
              {
                v33 = &v30[v29 != 0];
                v32 = sceNpCommerce2_66EC0829(v38, 769, v31[1], v31[2], 0);
                ++v29;
                v31 += 3;
                if ( v32 < 0 )
                  break;
                v30 = &v33[sceNet_lib_D8722983(v38)];
                if ( v29 >= a8 )
                  goto LABEL_28;
              }
            }
            else
            {
LABEL_28:
              v32 = (int)v30;
            }
          }
          v13 = v32;
          if ( v32 >= 0 )
          {
            v28 = ((u64)(v28) & 0xFFFFFFFF00000000ULL) | (u32)(v28 + v32);
            if ( a1 != 1 )
            {
              v34 = v41;
              *v41 = v28;
              v34[1] = HIDWORD(v28) + (v32 >> 31) + ((unsigned int)v28 < v32);
            }
            return 0;
          }
        }
      }
    }
  }
  return v13;
}


int store_req_fn_45(int a1, int a2, u32 *a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)
{
  int v16;
  int v17;
  int v18;
  s64 v19;
  unsigned int i;
  int v21;
  int v22;
  s64 v23;
  unsigned int j;
  int v25;
  int v26;
  s64 v27;
  int v28;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  s64 v34;
  int v35;
  int v36;
  s64 v37;
  int v38;
  u32 *v39;
  int v41;
  int v42;
  u32 v43[4];
  u8 v44[16];
  u8 v45[16];
  u32 *v46;
  int v47;

  v46 = a3;
  v47 = a8;
  sceKernelMemset(v43, 0, 8);
  v43[0] = "account.";
  if ( a1 == 1 )
  {
    v17 = store_tlv_fn_8(a2, 2, a4, 0);
  }
  else
  {
    v16 = sceNet_lib_CF705E46(v45, (const char *)"%u", a4);
    v17 = -2141909231;
    if ( v16 < 16 )
      v17 = v16 + 8;
  }
  v18 = v17;
  if ( v17 >= 0 )
  {
    v19 = v17;
    for ( i = 0; i < sceNet_lib_D8722983(a5) && i < 0x10; ++i )
      v44[i] = sceNet_lib_C13C9307(*(char *)(a5 + i));
    if ( i == 16 )
      v44[15] = 0;
    else
      v44[i] = 0;
    v21 = sceNet_lib_D8722983(v44);
    v22 = a1 == 1 ? store_tlv_fn_4(a2, v43, 1, 41, v44, v21, 1) : sceNet_lib_D8722983(v43[0]) + 1 + v21 + 8;
    v18 = v22;
    if ( v22 >= 0 )
    {
      v23 = v19 + v22;
      for ( j = 0; j < sceNet_lib_D8722983(a6) && j < 0x10; ++j )
        v44[j] = sceNet_lib_96EF9DA1(*(char *)(a6 + j));
      if ( j == 16 )
        v44[15] = 0;
      else
        v44[j] = 0;
      v25 = sceNet_lib_D8722983(v44);
      v26 = a1 == 1 ? store_tlv_fn_4(a2, v43, 1, 42, v44, v25, 1) : sceNet_lib_D8722983(v43[0]) + 1 + v25 + 9;
      v18 = v26;
      if ( v26 >= 0 )
      {
        v27 = v23 + v26;
        sceNet_lib_CF705E46(v44, (const char *)"%u", a7);
        v28 = sceNet_lib_D8722983(v44);
        v29 = a1 == 1 ? store_tlv_fn_4(a2, v43, 1, 43, v44, v28, 1) : sceNet_lib_D8722983(v43[0]) + 1 + v28 + 4;
        v18 = v29;
        if ( v29 >= 0 )
        {
          v27 = ((u64)(v27) & 0xFFFFFFFF00000000ULL) | (u32)(v27 + v29);
          v30 = HIDWORD(v27) + (v29 >> 31);
          v31 = (unsigned int)v27 < v29;
          sceNet_lib_CF705E46(v44, (const char *)"%u", v47);
          v32 = sceNet_lib_D8722983(v44);
          v33 = a1 == 1 ? store_tlv_fn_4(a2, v43, 1, 44, v44, v32, 1) : sceNet_lib_D8722983(v43[0]) + 1 + v32 + 4;
          v18 = v33;
          if ( v33 >= 0 )
          {
            v27 = ((u64)(v27) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v30 + v31) << 32);
            v34 = v27 + v33;
            sceNet_lib_CF705E46(v44, (const char *)"%u", a9);
            v35 = sceNet_lib_D8722983(v44);
            v36 = a1 == 1 ? store_tlv_fn_4(a2, v43, 1, 45, v44, v35, 1) : sceNet_lib_D8722983(v43[0]) + 1 + v35 + 4;
            v18 = v36;
            if ( v36 >= 0 )
            {
              v37 = v34 + v36;
              if ( a1 == 1 )
              {
                v18 = sceNpCommerce2_AE614CC6(a2, "&", 1);
                if ( v18 < 0 )
                  return v18;
                v41 = sceNpCommerce2_AE614CC6(a2, "consoleId=", 10);
                if ( v41 < 0 )
                  return v41;
                v42 = sceNet_lib_D8722983(a10);
                v38 = sceNpCommerce2_AE614CC6(a2, a10, v42);
              }
              else
              {
                v38 = sceNet_lib_D8722983(a10) + 11;
              }
              v18 = v38;
              if ( v38 >= 0 )
              {
                v37 = ((u64)(v37) & 0xFFFFFFFF00000000ULL) | (u32)(v37 + v38);
                if ( a1 != 1 )
                {
                  v39 = v46;
                  *v46 = v37;
                  v39[1] = HIDWORD(v37) + (v38 >> 31) + ((unsigned int)v37 < v38);
                }
                return 0;
              }
            }
          }
        }
      }
    }
  }
  return v18;
}


