#include "regcam_internal.h"

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

const RegCamFormKey g_form_keys[81] = {
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

const char * const g_gender_strs[3] = { "XXX", "m", "f" };


int regcam_get_endpoint(int a1, u16 a2, u32 *a3, u32 *a4)
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
        *a3 = g_service_prefixes[a1];
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


s32 regcam_send_req_1(int a1, int a2, int a3, u16 a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = regcam_get_endpoint(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return regcam_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


int regcam_send_req_2(int a1, int a2, int a3, u16 a4, int *a5, int a6)
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
      result = regcam_emit_str_field(v11 != 0, v14, 14, v16, 0);
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
      result = regcam_get_endpoint(a1, v8, &v17, &v18);
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


s32 regcam_send_req_3(int a1, int a2, int a3, u16 a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = regcam_get_endpoint(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return regcam_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


s32 regcam_send_req_4(int a1, int a2, int a3, u16 a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = regcam_get_endpoint(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return regcam_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


int regcam_send_req_5(int a1, int a2, int a3, u16 a4, u32 *a5, int a6)
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
    v13 = regcam_emit_str_field(v11 != 0, *a5, 25, "CC", 0);
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
      result = regcam_get_endpoint(a1, v7, &v15, &v16);
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


s32 regcam_send_req_6(int a1, int a2, int a3, u16 a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = regcam_get_endpoint(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return regcam_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


int regcam_send_req_7(int a1, int a2, int a3, u16 a4, int *a5, int a6)
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
    result = regcam_form_build_1(v12 != 0, i, &v17, 0, a6, 0, 0);
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
      result = regcam_get_endpoint(a1, v9, &v19, &v20);
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


int regcam_send_req_8(int a1, int a2, int a3, u16 a4, int *a5, int a6)
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
    result = regcam_emit_uint_field(v11 != 0, i, 2, a6, 0);
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
      result = regcam_get_endpoint(a1, v8, &v14, &v15);
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


int regcam_send_req_9(int a1, int a2, int a3, u16 a4, int *a5, int a6, int a7, int a8, int a9)
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
    result = regcam_form_build_bind_account(v15 != 0, i, &v20, a6, a7, a8, a9);
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
      result = regcam_get_endpoint(a1, v11, &v22, &v23);
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


int regcam_send_req_10(
        int a1,
        int a2,
        int a3,
        u16 a4,
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
    result = regcam_form_build_2(v18 != 0, i, &v23, a6, a7, a8, a9, a10, a11, a12);
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
      result = regcam_get_endpoint(a1, v13, &v25, &v26);
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


s32 regcam_send_req_11(int a1, int a2, int a3, u16 a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = regcam_get_endpoint(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return regcam_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


int regcam_send_req_12(int a1, int a2, int a3, u16 a4, int *a5, int a6, int a7, int a8, int a9, int a10)
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
    result = regcam_form_build_3(v16 != 0, i, &v21, a6, a7, a8, a9, a10);
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
      result = regcam_get_endpoint(a1, v12, &v23, &v24);
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


int regcam_send_req_13(int a1, int a2, int a3, u16 a4, int *a5, int a6)
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
    result = regcam_form_build_4(v12 != 0, i, &v17, a6);
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
      result = regcam_get_endpoint(a1, v9, &v19, &v20);
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


int regcam_send_req_14(int a1, int a2, int a3, u16 a4, int *a5, int a6)
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
    result = regcam_form_build_5(v12 != 0, i, &v17, a6);
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
      result = regcam_get_endpoint(a1, v9, &v19, &v20);
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


int regcam_send_req_15(int a1, int a2, int a3, u16 a4, int *a5, int a6)
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
    result = regcam_form_build_6(v12 != 0, i, &v17, a6);
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
      result = regcam_get_endpoint(a1, v9, &v19, &v20);
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


s32 regcam_send_req_16(int a1, int a2, int a3, u16 a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = regcam_get_endpoint(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return regcam_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


int regcam_send_req_17(
        int a1,
        int a2,
        int a3,
        u16 a4,
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
    result = regcam_form_build_7(v18 != 0, i, &v23, a6, a7, a8, a9, a10, a11, a12);
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
      result = regcam_get_endpoint(a1, v13, &v25, &v26);
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


int regcam_send_req_18(int a1, int a2, int a3, u16 a4, int *a5, int a6)
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
    result = regcam_form_build_1(v12 != 0, i, &v17, 1, a6, 0, 0);
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
      result = regcam_get_endpoint(a1, v9, &v19, &v20);
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


int regcam_send_req_19(int a1, int a2, int a3, u16 a4, int *a5, int a6, int a7)
{
  int v8;
  unsigned int v12;
  int i;
  int v14;
  int v15;
  int v16;
  s64 v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int result;
  u32 v23[8];
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;

  v26 = a2;
  v27 = a3;
  v28 = 0;
  v29 = 0;
  v8 = a4;
  v12 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    sceKernelMemset(v23, 0, 32);
    v23[0] = "koreanId";
    v14 = regcam_emit_prefixed_field(v12 != 0, i, v23, 1, 20, a6, 0);
    v15 = v14;
    if ( v14 >= 0 )
    {
      v16 = i;
      v17 = v14;
      v18 = regcam_emit_prefixed_field(v12 != 0, v16, v23, 1, 21, a7, 1);
      v15 = v18;
      if ( v18 >= 0 )
      {
        v19 = __CFADD__((u32)v17, v18);
        v20 = HIDWORD(v17) + (v18 >> 31);
        v21 = v17 + v18;
        if ( v12 == 0 )
        {
          v28 = v21;
          v29 = v20 + v19;
        }
        v15 = 0;
      }
    }
    result = v15;
    if ( v15 < 0 )
      break;
    if ( v12 != 0 )
    {
      ++v12;
    }
    else
    {
      v25 = 0;
      v24 = 0;
      result = regcam_get_endpoint(a1, v8, &v24, &v25);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v26, a1, v8, v24, v25, 0, v27, a5, v28, v29);
      v12 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v12 >= 2 )
      return 0;
  }
  return result;
}


s32 regcam_send_req_20(int a1, int a2, int a3, u16 a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = regcam_get_endpoint(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return regcam_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


int regcam_send_req_21(int a1, int a2, int a3, u16 a4, int *a5, int a6, int a7)
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
    result = regcam_form_build_8(v13 != 0, i, &v18, a6, a7);
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
      result = regcam_get_endpoint(a1, v9, &v20, &v21);
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


int regcam_send_req_22(int a1, int a2, int a3, u16 a4, int *a5, int a6, int a7)
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
    result = regcam_form_build_9(v13 != 0, i, &v18, a6, a7);
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
      result = regcam_get_endpoint(a1, v9, &v20, &v21);
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


int regcam_send_req_23(int a1, int a2, int a3, u16 a4, int *a5, int a6, int a7, int a8, int a9, int a10)
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
    result = regcam_form_build_10(v16 != 0, i, &v21, a6, a7, a8, a9, a10);
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
      result = regcam_get_endpoint(a1, v12, &v23, &v24);
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


int regcam_send_req_24(int a1, int a2, int a3, u16 a4, int *a5, int a6, int a7)
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
    result = regcam_form_build_11(v13 != 0, i, &v18, a6, a7);
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
      result = regcam_get_endpoint(a1, v9, &v20, &v21);
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


s32 regcam_send_req_25(int a1, int a2, int a3, u16 a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = regcam_get_endpoint(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return regcam_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


int regcam_send_req_26(int a1, int a2, int a3, u16 a4, int *a5, int a6, int a7)
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
    result = regcam_form_build_12(v13 != 0, i, &v18, a6, a7);
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
      result = regcam_get_endpoint(a1, v9, &v20, &v21);
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


s32 regcam_send_req_27(int a1, int a2, int a3, u16 a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = regcam_get_endpoint(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return regcam_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


int regcam_send_req_28(int a1, int a2, int a3, u16 a4, int *a5, int a6, int a7, int a8, int a9)
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
    result = regcam_form_build_13(v15 != 0, i, &v20, a6, a7, a8, a9);
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
      result = regcam_get_endpoint(a1, v11, &v22, &v23);
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


int regcam_send_req_29(
        int a1,
        int a2,
        int a3,
        u16 a4,
        int *a5,
        int a6,
        int a7,
        int a8,
        int a9,
        int a10,
        int a11,
        int a12,
        int a13)
{
  int v14;
  unsigned int v19;
  int i;
  int result;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;

  v28 = a2;
  v29 = a3;
  v24 = 0;
  v25 = 0;
  v14 = a4;
  v19 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = regcam_form_build_14(v19 != 0, i, &v24, a6, a7, a8, a9, a10, a11, a12, a13);
    if ( result < 0 )
      break;
    if ( v19 != 0 )
    {
      ++v19;
    }
    else
    {
      v27 = 0;
      v26 = 0;
      v22 = v24;
      v23 = v25;
      result = regcam_get_endpoint(a1, v14, &v26, &v27);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v28, a1, v14, v26, v27, 0, v29, a5, v22, v23);
      v19 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v19 >= 2 )
      return 0;
  }
  return result;
}


int regcam_send_req_30(int a1, int a2, int a3, u16 a4, int *a5, int a6, int a7, int a8)
{
  int v9;
  unsigned int v13;
  int i;
  int result;
  u8 v16[32];
  int v17;
  int v18;
  int v19;
  int v20;
  s64 v21;

  v19 = a2;
  v20 = a3;
  v21 = 0;
  v9 = a4;
  v13 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    sceNpCommerce2_22CEC5E2(v16, 32, a7, a8, 1);
    result = regcam_emit_str_field(v13 != 0, i, 72, v16, 0);
    if ( result >= 0 )
    {
      if ( v13 == 0 )
        v21 = result;
      result = 0;
    }
    if ( result < 0 )
      break;
    if ( v13 != 0 )
    {
      ++v13;
    }
    else
    {
      v18 = 0;
      v17 = 0;
      result = regcam_get_endpoint(a1, v9, &v17, &v18);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v19, a1, v9, v17, v18, 0, v20, a5, v21, HIDWORD(v21));
      v13 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v13 >= 2 )
      return 0;
  }
  return result;
}


int regcam_send_req_31(int a1, int a2, int a3, u16 a4, int *a5, int a6, int a7)
{
  int v8;
  unsigned int v12;
  int i;
  int v14;
  int v15;
  int v16;
  s64 v17;
  int v18;
  s64 v19;
  int result;
  int v21;
  int v22;
  int v23;
  int v24;
  s64 v25;

  v23 = a2;
  v24 = a3;
  v25 = 0;
  v8 = a4;
  v12 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    v14 = regcam_emit_uint_field(v12 != 0, i, 7, a6, 0);
    v15 = v14;
    if ( v14 >= 0 )
    {
      v16 = i;
      v17 = v14;
      v18 = regcam_emit_uint_field(v12 != 0, v16, 8, a7, 1);
      v15 = v18;
      if ( v18 >= 0 )
      {
        v19 = v17 + v18;
        if ( v12 == 0 )
          v25 = v19;
        v15 = 0;
      }
    }
    result = v15;
    if ( v15 < 0 )
      break;
    if ( v12 != 0 )
    {
      ++v12;
    }
    else
    {
      v22 = 0;
      v21 = 0;
      result = regcam_get_endpoint(a1, v8, &v21, &v22);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v23, a1, v8, v21, v22, 0, v24, a5, v25, HIDWORD(v25));
      v12 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v12 >= 2 )
      return 0;
  }
  return result;
}


int regcam_send_req_32(int a1, int a2, int a3, u16 a4, int *a5, int a6, int a7, int a8)
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
    result = regcam_form_build_15(v14 != 0, i, &v19, a6, a7, a8);
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
      result = regcam_get_endpoint(a1, v10, &v21, &v22);
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


int regcam_send_req_33(int a1, int a2, int a3, u16 a4, int *a5, int a6, int a7, int a8)
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
    result = regcam_form_build_16(v14 != 0, i, &v19, a6, a7, a8);
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
      result = regcam_get_endpoint(a1, v10, &v21, &v22);
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


int regcam_send_req_34(
        int a1,
        int a2,
        int a3,
        u16 a4,
        int *a5,
        int a6,
        int a7,
        int a8,
        int a9,
        int a10,
        int a11)
{
  int v13;
  unsigned int v17;
  int i;
  int result;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;

  v26 = a2;
  v27 = a3;
  v22 = 0;
  v23 = 0;
  v13 = a4;
  v17 = 0;
  for ( i = *a5; ; i = *a5 )
  {
    result = regcam_form_build_17(v17 != 0, i, &v22, a6, a7, a8, a9, a10, a11);
    if ( result < 0 )
      break;
    if ( v17 != 0 )
    {
      ++v17;
    }
    else
    {
      v25 = 0;
      v24 = 0;
      v20 = v22;
      v21 = v23;
      result = regcam_get_endpoint(a1, v13, &v24, &v25);
      if ( result >= 0 )
        result = sceNpCommerce2_382803C8(v26, a1, v13, v24, v25, 0, v27, a5, v20, v21);
      v17 = 1;
      if ( result < 0 )
        return result;
    }
    if ( v17 >= 2 )
      return 0;
  }
  return result;
}


int regcam_send_req_35(int a1, int a2, int a3, u16 a4, int *a5, int a6, int a7, int a8)
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
    result = regcam_form_build_18(v14 != 0, i, &v19, a6, a7, a8);
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
      result = regcam_get_endpoint(a1, v10, &v21, &v22);
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


int regcam_send_req_36(int a1, int a2, int a3, u16 a4, int *a5, int a6, int a7)
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
    result = regcam_form_build_1(v13 != 0, i, &v18, 0, a6, 1, a7);
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
      result = regcam_get_endpoint(a1, v9, &v20, &v21);
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


s32 regcam_send_req_37(int a1, int a2, int a3, u16 a4, u32 *a5)
{
  int v5;
  int v10;
  int v11;
  int v13;
  u32 v14[3];

  v5 = a4;
  v14[0] = 0;
  v13 = 0;
  v10 = regcam_get_endpoint(a1, a4, &v13, v14);
  if ( v10 >= 0 )
    v10 = sceNpCommerce2_382803C8(a2, a1, v5, v13, v14[0], 0, a3, a5, 0, 0);
  v11 = v10;
  if ( v10 >= 0 )
    return regcam_min_s32(sceNpCommerce2_AE614CC6(*a5, 0, 0), 0);
  return v11;
}


int regcam_send_req_38(int a1, int a2, int a3, u16 a4, int *a5, int a6)
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
    result = regcam_form_build_1(v12 != 0, i, &v17, 0, a6, 0, 0);
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
      result = regcam_get_endpoint(a1, v9, &v19, &v20);
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


int regcam_send_req_39(int a1, int a2, int a3, u16 a4, int *a5, int a6)
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
    result = regcam_form_build_19(v12 != 0, i, &v17, a6);
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
      result = regcam_get_endpoint(a1, v9, &v19, &v20);
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


int regcam_emit_str_field(int a1, int a2, int a3, int a4, int a5)
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


int regcam_emit_uint_field(int a1, int a2, int a3, int a4, int a5)
{
  int v5;
  int v6;
  int v7;
  int v9;

  if ( a1 == 1 )
    return regcam_write_uint_field(a2, a3, a4, a5);
  v6 = (a5 == 1) + g_form_keys[a3].len;
  v5 = sceNet_lib_CF705E46(&v9, "%u", a4);
  v7 = -2141909231;
  if ( v5 < 16 )
    return v5 + v6;
  return v7;
}


int regcam_emit_prefixed_field(int a1, int a2, int *a3, unsigned int a4, int a5, int a6, int a7)
{
  int v13;
  int v15;
  int v16;
  unsigned int v17;
  int *v18;
  int v19;

  v13 = sceNet_lib_D8722983(a6);
  v15 = v13;
  if ( a1 == 1 )
    return regcam_write_prefixed_field(a2, a3, a4, a5, a6, v13, a7);
  v16 = a7 == 1;
  v17 = 0;
  if ( a4 != 0 )
  {
    v18 = a3;
    do
    {
      v19 = *v18;
      ++v17;
      ++v18;
      v16 += sceNet_lib_D8722983(v19);
    }
    while ( v17 < a4 );
  }
  return v16 + g_form_keys[a5].len + v15;
}


int regcam_form_build_1(int a1, int a2, u32 *a3, int a4, int a5, int a6, int a7)
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
  int v22;
  int v23;
  int v24;
  u8 *v25;
  u8 *v26;
  u8 *v27;
  u8 *v28;
  int v29;
  u8 *v30;
  int v31;
  u8 *v32;
  int v33;
  u8 *v34;
  int v35;
  u8 *v36;
  int v37;
  int v38;
  int v39;
  int v40; // dc
  u32 *v41;
  const char *v43;
  int v44;
  int v45;
  int v46;
  int v47;
  int v48;
  int v49;
  int v50;
  u32 *v51;
  int v52;
  int v53;
  int v54;
  unsigned int v55;
  u32 *v56;
  int v57;
  int v58;
  int v59;
  unsigned int v60;
  int *v61;
  int v62;
  int v63;
  int v64;
  unsigned int v65;
  u32 *v66;
  int v67;
  int v68;
  int v69;
  unsigned int v70;
  int *v71;
  int v72;
  int v73;
  int v74;
  unsigned int v75;
  u32 *v76;
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
  int v97;
  int v98;
  int v99;
  u8 v100[784];
  u32 v101[8];
  u8 v102[16];
  u8 v103[16];
  u8 v104[16];
  u8 v105[16];
  int v106;
  u32 *v107;
  int v108;
  int v109;
  int v110;
  int v111;
  int v112;
  int v113;
  int v114;

  v106 = a2;
  v107 = a3;
  v8 = 0;
  v9 = 0;
  v108 = a6;
  v109 = a7;
  sceKernelMemset(v101, 0, 32);
  v12 = "creditCard.";
  if ( a4 != 0 )
  {
    if ( a4 != 1 )
      goto LABEL_3;
    v12 = "account.";
  }
  v101[0] = v12;
LABEL_3:
  v13 = *(u8 **)a5;
  v101[1] = "address.";
  v101[2] = "address";
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
      v17 = regcam_write_prefixed_field(v106, v101, 1, 26, v13, v16, 0);
    else
      v17 = sceNet_lib_D8722983(v101[0]) + v16 + 16;
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
        v97 = sceNpCommerce2_66EC0829(v100, 769, v14, v20, 0);
        v18 = v97;
        if ( v97 < 0 )
          return v18;
        v98 = sceNet_lib_D8722983(v100);
        v99 = a1 == 1 ? regcam_write_prefixed_field(v106, v101, 1, 27, v100, v98, v9) : v9 + sceNet_lib_D8722983(v101[0]) + v98 + 11;
        v18 = v99;
        if ( v99 < 0 )
          return v18;
        v8 += v99;
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
      v95 = sceNet_lib_D8722983(v19);
      if ( a1 == 1 )
        v96 = regcam_write_prefixed_field(v106, v101, 1, 28, v19, v95, v9);
      else
        v96 = v9 + sceNet_lib_D8722983(v101[0]) + v95 + 11;
      v18 = v96;
      if ( v96 < 0 )
        return v18;
      v8 += v96;
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
    v92 = sceNet_lib_CF705E46(v102, "%u", v21);
    v18 = -2141909231;
    if ( v92 >= 16 )
      return v18;
    v93 = sceNet_lib_D8722983(v102);
    v94 = a1 == 1 ? regcam_write_prefixed_field(v106, v101, 1, 29, v102, v93, v9) : v9 + sceNet_lib_D8722983(v101[0]) + v93 + 10;
    v18 = v94;
    if ( v94 < 0 )
      return v18;
    v8 += v94;
    v9 = 1;
  }
  v22 = *(u32 *)(a5 + 24);
  if ( v22 != 0 )
  {
    v89 = sceNet_lib_CF705E46(v103, "%u", v22);
    v18 = -2141909231;
    if ( v89 >= 16 )
      return v18;
    v90 = sceNet_lib_D8722983(v103);
    v91 = a1 == 1 ? regcam_write_prefixed_field(v106, v101, 1, 30, v103, v90, v9) : v9 + sceNet_lib_D8722983(v101[0]) + v90 + 11;
    v18 = v91;
    if ( v91 < 0 )
      return v18;
    v8 += v91;
    v9 = 1;
  }
  v23 = *(u32 *)(a5 + 28);
  if ( v23 != 0 )
  {
    v86 = sceNet_lib_CF705E46(v104, "%u", v23);
    v18 = -2141909231;
    if ( v86 >= 16 )
      return v18;
    v87 = sceNet_lib_D8722983(v104);
    v88 = a1 == 1 ? regcam_write_prefixed_field(v106, v101, 1, 31, v104, v87, v9) : v9 + sceNet_lib_D8722983(v101[0]) + v87 + 11;
    v18 = v88;
    if ( v88 < 0 )
      return v18;
    v8 += v88;
    v9 = 1;
  }
  v24 = *(u32 *)(a5 + 32);
  if ( v24 != 0 )
  {
    v83 = sceNet_lib_CF705E46(v105, "%u", v24);
    v18 = -2141909231;
    if ( v83 >= 16 )
      return v18;
    v84 = sceNet_lib_D8722983(v105);
    v85 = a1 == 1 ? regcam_write_prefixed_field(v106, v101, 1, 32, v105, v84, v9) : v9 + sceNet_lib_D8722983(v101[0]) + 12 + v84;
    v18 = v85;
    if ( v85 < 0 )
      return v18;
    v8 += v85;
    v9 = 1;
  }
  v25 = *(u8 **)(a5 + 36);
  if ( v25 != NULL )
  {
    if ( *v25 != 0 )
    {
      v81 = sceNet_lib_D8722983(*(u32 *)(a5 + 36));
      if ( a1 == 1 )
        v82 = regcam_write_prefixed_field(v106, v101, 1, 33, v25, v81, v9);
      else
        v82 = v9 + sceNet_lib_D8722983(v101[0]) + 13 + v81;
      v18 = v82;
      if ( v82 < 0 )
        return v18;
      v8 += v82;
      v9 = 1;
    }
    v26 = *(u8 **)(a5 + 40);
  }
  else
  {
    v26 = *(u8 **)(a5 + 40);
  }
  if ( v26 != NULL )
  {
    if ( *v26 != 0 )
    {
      v79 = sceNet_lib_D8722983(v26);
      if ( a1 == 1 )
        v80 = regcam_write_prefixed_field(v106, v101, 1, 34, v26, v79, v9);
      else
        v80 = v9 + sceNet_lib_D8722983(v101[0]) + v79 + 12;
      v18 = v80;
      if ( v80 < 0 )
        return v18;
      v8 += v80;
      v9 = 1;
    }
    v27 = *(u8 **)(a5 + 44);
  }
  else
  {
    v27 = *(u8 **)(a5 + 44);
  }
  if ( v27 != NULL )
  {
    if ( *v27 != 0 )
    {
      v29 = *(u32 *)(a5 + 48);
      if ( v29 != 0 )
      {
        v18 = sceNpCommerce2_66EC0829(v100, 769, v27, v29, 0);
        if ( v18 < 0 )
          return v18;
        v110 = sceNet_lib_D8722983(v100);
        v74 = v9;
        v75 = 0;
        v76 = v101;
        if ( a1 == 1 )
        {
          v78 = regcam_write_prefixed_field(v106, v101, 3, 20, v100, v110, v9);
        }
        else
        {
          do
          {
            ++v75;
            v77 = sceNet_lib_D8722983(*v76++);
            v74 += v77;
          }
          while ( v75 < 3 );
          v78 = v74 + v110 + 2;
        }
        v18 = v78;
        if ( v78 < 0 )
          return v18;
        v8 += v78;
        v9 = 1;
      }
      v28 = *(u8 **)(a5 + 52);
    }
    else
    {
      v28 = *(u8 **)(a5 + 52);
    }
  }
  else
  {
    v28 = *(u8 **)(a5 + 52);
  }
  if ( v28 != NULL )
  {
    if ( *v28 != 0 )
    {
      v31 = *(u32 *)(a5 + 56);
      if ( v31 != 0 )
      {
        v18 = sceNpCommerce2_66EC0829(v100, 769, v28, v31, 0);
        if ( v18 < 0 )
          return v18;
        v111 = sceNet_lib_D8722983(v100);
        v69 = v9;
        v70 = 0;
        v71 = v101;
        if ( a1 == 1 )
        {
          v73 = regcam_write_prefixed_field(v106, v101, 3, 21, v100, v111, v9);
        }
        else
        {
          do
          {
            v72 = *v71;
            ++v70;
            ++v71;
            v69 += sceNet_lib_D8722983(v72);
          }
          while ( v70 < 3 );
          v73 = v69 + v111 + 2;
        }
        v18 = v73;
        if ( v73 < 0 )
          return v18;
        v8 += v73;
        v9 = 1;
      }
      v30 = *(u8 **)(a5 + 60);
    }
    else
    {
      v30 = *(u8 **)(a5 + 60);
    }
  }
  else
  {
    v30 = *(u8 **)(a5 + 60);
  }
  if ( v30 != NULL )
  {
    if ( *v30 != 0 )
    {
      v33 = *(u32 *)(a5 + 64);
      if ( v33 != 0 )
      {
        v18 = sceNpCommerce2_66EC0829(v100, 769, v30, v33, 0);
        if ( v18 < 0 )
          return v18;
        v112 = sceNet_lib_D8722983(v100);
        v64 = v9;
        v65 = 0;
        v66 = v101;
        if ( a1 == 1 )
        {
          v68 = regcam_write_prefixed_field(v106, v101, 3, 22, v100, v112, v9);
        }
        else
        {
          do
          {
            ++v65;
            v67 = sceNet_lib_D8722983(*v66++);
            v64 += v67;
          }
          while ( v65 < 3 );
          v68 = v64 + v112 + 2;
        }
        v18 = v68;
        if ( v68 < 0 )
          return v18;
        v8 += v68;
        v9 = 1;
      }
      v32 = *(u8 **)(a5 + 68);
    }
    else
    {
      v32 = *(u8 **)(a5 + 68);
    }
  }
  else
  {
    v32 = *(u8 **)(a5 + 68);
  }
  if ( v32 != NULL )
  {
    if ( *v32 != 0 )
    {
      v35 = *(u32 *)(a5 + 72);
      if ( v35 != 0 )
      {
        v18 = sceNpCommerce2_66EC0829(v100, 769, v32, v35, 0);
        if ( v18 < 0 )
          return v18;
        v113 = sceNet_lib_D8722983(v100);
        v59 = v9;
        v60 = 0;
        v61 = v101;
        if ( a1 == 1 )
        {
          v63 = regcam_write_prefixed_field(v106, v101, 2, 35, v100, v113, v9);
        }
        else
        {
          do
          {
            v62 = *v61;
            ++v60;
            ++v61;
            v59 += sceNet_lib_D8722983(v62);
          }
          while ( v60 < 2 );
          v63 = v59 + v113 + 5;
        }
        v18 = v63;
        if ( v63 < 0 )
          return v18;
        v8 += v63;
        v9 = 1;
      }
      v34 = *(u8 **)(a5 + 76);
    }
    else
    {
      v34 = *(u8 **)(a5 + 76);
    }
  }
  else
  {
    v34 = *(u8 **)(a5 + 76);
  }
  if ( v34 != NULL )
  {
    if ( *v34 != 0 )
    {
      v37 = *(u32 *)(a5 + 80);
      if ( v37 != 0 )
      {
        v18 = sceNpCommerce2_66EC0829(v100, 769, v34, v37, 0);
        if ( v18 < 0 )
          return v18;
        v114 = sceNet_lib_D8722983(v100);
        v54 = v9;
        v55 = 0;
        v56 = v101;
        if ( a1 == 1 )
        {
          v58 = regcam_write_prefixed_field(v106, v101, 2, 36, v100, v114, v9);
        }
        else
        {
          do
          {
            ++v55;
            v57 = sceNet_lib_D8722983(*v56++);
            v54 += v57;
          }
          while ( v55 < 2 );
          v58 = v54 + v114 + 9;
        }
        v18 = v58;
        if ( v58 < 0 )
          return v18;
        v8 += v58;
        v9 = 1;
      }
      v36 = *(u8 **)(a5 + 84);
    }
    else
    {
      v36 = *(u8 **)(a5 + 84);
    }
  }
  else
  {
    v36 = *(u8 **)(a5 + 84);
  }
  v38 = v108;
  if ( v36 != NULL )
  {
    v38 = v108;
    if ( *v36 != 0 )
    {
      v39 = *(u32 *)(a5 + 88);
      if ( v39 != 0 )
      {
        v18 = sceNpCommerce2_66EC0829(v100, 769, v36, v39, 0);
        if ( v18 < 0 )
          return v18;
        v47 = sceNet_lib_D8722983(v100);
        v48 = v47;
        v49 = v9;
        v50 = 0;
        v51 = v101;
        if ( a1 == 1 )
        {
          v53 = regcam_write_prefixed_field(v106, v101, 2, 37, v100, v47, v9);
        }
        else
        {
          do
          {
            ++v50;
            v52 = sceNet_lib_D8722983(*v51++);
            v49 += v52;
          }
          while ( v101 != NULL );
          v53 = v49 + v48 + 11;
        }
        v18 = v53;
        if ( v53 < 0 )
          return v18;
        v8 += v53;
        v9 = 1;
      }
      v38 = v108;
    }
  }
  if ( v38 != 1 )
  {
LABEL_73:
    v40 = a1 == 1;
    v41 = v107;
    if ( !v40 )
    {
      *v107 = v8;
      v41[1] = HIDWORD(v8);
    }
    return 0;
  }
  v43 = "true";
  if ( v109 != 1 )
    v43 = "false";
  if ( a1 == 1 )
  {
    if ( v9 != 1 || (v44 = sceNpCommerce2_AE614CC6(v106, "&", 1)) >= 0 )
    {
      v45 = sceNpCommerce2_AE614CC6(v106, "save=", 5);
      if ( v45 < 0 )
        return v45;
      v46 = sceNet_lib_D8722983(v43);
      v44 = sceNpCommerce2_AE614CC6(v106, v43, v46);
    }
  }
  else
  {
    v44 = v9 + sceNet_lib_D8722983(v43) + 5;
  }
  v18 = v44;
  if ( v44 >= 0 )
  {
    v8 += v44;
    goto LABEL_73;
  }
  return v18;
}


int regcam_form_build_bind_account(int pass, int http_req, int *out_len, int ver, int user, int pwd, int cid)
{
    u8 enc[784];
    char num[16];
    s64 total = 0;
    int r, n;

    if (pass == 1) {
        r = regcam_write_uint_field(http_req, 2, ver, 0);
    } else {
        n = sceNet_lib_CF705E46(num, "%u", ver);
        r = (n < 16) ? (n + (int)g_form_keys[2].len) : -2141909231;
    }
    if (r < 0) return r;
    total = r;

    r = sceNpCommerce2_66EC0829(enc, 769, user, sceNet_lib_D8722983(user), 0);
    if (r < 0) return r;
    if (pass == 1) {
        r = sceNpCommerce2_AE614CC6(http_req, g_form_keys[0].str, g_form_keys[0].len);
        if (r < 0) return r;
        r = sceNpCommerce2_AE614CC6(http_req, g_form_keys[3].str, g_form_keys[3].len);
        if (r < 0) return r;
        r = sceNpCommerce2_AE614CC6(http_req, enc, sceNet_lib_D8722983(enc));
    } else {
        r = (int)g_form_keys[3].len + sceNet_lib_D8722983(enc) + 1;
    }
    if (r < 0) return r;
    total += r;

    r = sceNpCommerce2_66EC0829(enc, 769, pwd, sceNet_lib_D8722983(pwd), 0);
    if (r < 0) return r;
    if (pass == 1) {
        r = sceNpCommerce2_AE614CC6(http_req, g_form_keys[0].str, g_form_keys[0].len);
        if (r < 0) return r;
        r = sceNpCommerce2_AE614CC6(http_req, g_form_keys[4].str, g_form_keys[4].len);
        if (r < 0) return r;
        r = sceNpCommerce2_AE614CC6(http_req, enc, sceNet_lib_D8722983(enc));
    } else {
        r = (int)g_form_keys[4].len + sceNet_lib_D8722983(enc) + 1;
    }
    if (r < 0) return r;
    total += r;

    if (pass == 1) {
        r = sceNpCommerce2_AE614CC6(http_req, g_form_keys[0].str, g_form_keys[0].len);
        if (r < 0) return r;
        r = sceNpCommerce2_AE614CC6(http_req, g_form_keys[5].str, g_form_keys[5].len);
        if (r < 0) return r;
        r = sceNpCommerce2_AE614CC6(http_req, cid, sceNet_lib_D8722983(cid));
    } else {
        r = (int)g_form_keys[5].len + sceNet_lib_D8722983(cid) + 1;
    }
    if (r < 0) return r;
    total += r;

    if (pass != 1) {
        out_len[0] = (int)(u32)total;
        out_len[1] = (int)(u32)((u64)total >> 32);
    }
    return 0;
}

int regcam_form_build_2(int a1, int a2, u32 *a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)
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
    v17 = regcam_write_uint_field(a2, 2, a4, 0);
  }
  else
  {
    v16 = sceNet_lib_CF705E46(v45, "%u", a4);
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
    v22 = a1 == 1 ? regcam_write_prefixed_field(a2, v43, 1, 41, v44, v21, 1) : sceNet_lib_D8722983(v43[0]) + 1 + v21 + 8;
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
      v26 = a1 == 1 ? regcam_write_prefixed_field(a2, v43, 1, 42, v44, v25, 1) : sceNet_lib_D8722983(v43[0]) + 1 + v25 + 9;
      v18 = v26;
      if ( v26 >= 0 )
      {
        v27 = v23 + v26;
        sceNet_lib_CF705E46(v44, "%u", a7);
        v28 = sceNet_lib_D8722983(v44);
        v29 = a1 == 1 ? regcam_write_prefixed_field(a2, v43, 1, 43, v44, v28, 1) : sceNet_lib_D8722983(v43[0]) + 1 + v28 + 4;
        v18 = v29;
        if ( v29 >= 0 )
        {
          v27 = ((u64)(v27) & 0xFFFFFFFF00000000ULL) | (u32)(v27 + v29);
          v30 = HIDWORD(v27) + (v29 >> 31);
          v31 = (unsigned int)v27 < v29;
          sceNet_lib_CF705E46(v44, "%u", v47);
          v32 = sceNet_lib_D8722983(v44);
          v33 = a1 == 1 ? regcam_write_prefixed_field(a2, v43, 1, 44, v44, v32, 1) : sceNet_lib_D8722983(v43[0]) + 1 + v32 + 4;
          v18 = v33;
          if ( v33 >= 0 )
          {
            v27 = ((u64)(v27) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v30 + v31) << 32);
            v34 = v27 + v33;
            sceNet_lib_CF705E46(v44, "%u", a9);
            v35 = sceNet_lib_D8722983(v44);
            v36 = a1 == 1 ? regcam_write_prefixed_field(a2, v43, 1, 45, v44, v35, 1) : sceNet_lib_D8722983(v43[0]) + 1 + v35 + 4;
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


int regcam_form_build_3(int a1, int a2, u32 *a3, int a4, int a5, int a6, int a7, int a8)
{
  s64 v11;
  int v13;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  s64 v24;
  int v25;
  int v26;
  u32 *v27;
  u8 v29[784];
  u32 v30[4];
  u32 *v31;
  int v32;
  int v33;

  v31 = a3;
  v11 = 0;
  v13 = 0;
  v32 = a7;
  v33 = a8;
  sceKernelMemset(v30, 0, 8);
  v30[0] = "account.";
  if ( a4 != 0 )
  {
    v15 = sceNet_lib_D8722983(a4);
    v16 = sceNpCommerce2_66EC0829(v29, 769, a4, v15, 0);
    if ( v16 < 0 )
      return v16;
    v17 = sceNet_lib_D8722983(v29);
    v18 = a1 == 1 ? regcam_write_prefixed_field(a2, v30, 1, 46, v29, v17, 0) : sceNet_lib_D8722983(v30[0]) + v17 + 10;
    v16 = v18;
    if ( v18 < 0 )
      return v16;
    v13 = 1;
    v11 = v18;
  }
  if ( a5 != 0 )
  {
    v19 = sceNet_lib_D8722983(a5);
    v16 = sceNpCommerce2_66EC0829(v29, 769, a5, v19, 0);
    if ( v16 < 0 )
      return v16;
    v20 = sceNet_lib_D8722983(v29);
    v21 = a1 == 1 ? regcam_write_prefixed_field(a2, v30, 1, 4, v29, v20, v13) : v13 + sceNet_lib_D8722983(v30[0]) + v20 + 9;
    v16 = v21;
    if ( v21 < 0 )
      return v16;
    v11 += v21;
    v13 = 1;
  }
  sceNet_lib_CF705E46(v29, "%u", a6);
  v22 = sceNet_lib_D8722983(v29);
  if ( a1 == 1 )
    v23 = regcam_write_prefixed_field(a2, v30, 1, 47, v29, v22, v13);
  else
    v23 = v13 + sceNet_lib_D8722983(v30[0]) + v22 + 17;
  v16 = v23;
  if ( v23 >= 0 )
  {
    v24 = v11 + v23;
    v16 = sceNpCommerce2_66EC0829(v29, 769, v32, v33, 0);
    if ( v16 >= 0 )
    {
      v25 = sceNet_lib_D8722983(v29);
      v26 = a1 == 1 ? regcam_write_prefixed_field(a2, v30, 1, 48, v29, v25, 1) : sceNet_lib_D8722983(v30[0]) + 1 + v25 + 15;
      v16 = v26;
      if ( v26 >= 0 )
      {
        v24 = ((u64)(v24) & 0xFFFFFFFF00000000ULL) | (u32)(v24 + v26);
        if ( a1 != 1 )
        {
          v27 = v31;
          *v31 = v24;
          v27[1] = HIDWORD(v24) + (v26 >> 31) + ((unsigned int)v24 < v26);
        }
        return 0;
      }
    }
  }
  return v16;
}


int regcam_form_build_4(int a1, int a2, u64 *a3, int a4)
{
  int v8;
  int v9;
  int v10;
  int v11;
  u8 v13[784];
  u32 v14[4];

  sceKernelMemset(v14, 0, 8);
  v14[0] = "account.";
  v8 = sceNet_lib_D8722983(a4);
  v9 = sceNpCommerce2_66EC0829(v13, 769, a4, v8, 0);
  if ( v9 >= 0 )
  {
    v10 = sceNet_lib_D8722983(v13);
    if ( a1 == 1 )
      v11 = regcam_write_prefixed_field(a2, v14, 1, 46, v13, v10, 0);
    else
      v11 = sceNet_lib_D8722983(v14[0]) + v10 + 10;
    v9 = v11;
    if ( v11 >= 0 )
    {
      v9 = 0;
      if ( a1 != 1 )
        *a3 = v11;
    }
  }
  return v9;
}


int regcam_form_build_5(int a1, int a2, u64 *a3, int a4)
{
  int v8;
  int v9;
  int v10;
  int v11;
  u8 v13[784];
  u32 v14[4];

  sceKernelMemset(v14, 0, 8);
  v14[0] = "account.";
  v8 = sceNet_lib_D8722983(a4);
  v9 = sceNpCommerce2_66EC0829(v13, 769, a4, v8, 0);
  if ( v9 >= 0 )
  {
    v10 = sceNet_lib_D8722983(v13);
    if ( a1 == 1 )
      v11 = regcam_write_prefixed_field(a2, v14, 1, 4, v13, v10, 0);
    else
      v11 = sceNet_lib_D8722983(v14[0]) + v10 + 9;
    v9 = v11;
    if ( v11 >= 0 )
    {
      v9 = 0;
      if ( a1 != 1 )
        *a3 = v11;
    }
  }
  return v9;
}


int regcam_form_build_6(int a1, int a2, u64 *a3, int a4)
{
  int v8;
  int v9;
  int v10;
  int v11;
  u8 v13[784];
  u32 v14[4];

  sceKernelMemset(v14, 0, 8);
  v14[0] = "account.";
  v8 = sceNet_lib_D8722983(a4);
  v9 = sceNpCommerce2_66EC0829(v13, 769, a4, v8, 0);
  if ( v9 >= 0 )
  {
    v10 = sceNet_lib_D8722983(v13);
    if ( a1 == 1 )
      v11 = regcam_write_prefixed_field(a2, v14, 1, 49, v13, v10, 0);
    else
      v11 = sceNet_lib_D8722983(v14[0]) + v10 + 9;
    v9 = v11;
    if ( v11 >= 0 )
    {
      v9 = 0;
      if ( a1 != 1 )
        *a3 = v11;
    }
  }
  return v9;
}


int regcam_form_build_7(int a1, int a2, u32 *a3, int a4, int a5, int a6, int a7, u8 *a8, u8 *a9, int a10)
{
  int v17;
  int v18;
  int v19;
  s64 v20;
  int v21;
  int v22;
  s64 v23;
  int v24;
  int v25; // dc
  u32 *v26;
  char *v28;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  int v34;
  int v35;
  int v36;
  u8 v37[784];
  u32 v38[4];
  u32 *v39;

  v39 = a3;
  sceKernelMemset(v38, 0, 8);
  v38[0] = "account.";
  v17 = sceNpCommerce2_66EC0829(v37, 769, a4, a5, 0);
  if ( v17 >= 0 )
  {
    v18 = sceNet_lib_D8722983(v37);
    v19 = a1 == 1 ? regcam_write_prefixed_field(a2, v38, 1, 50, v37, v18, 0) : sceNet_lib_D8722983(v38[0]) + v18 + 10;
    v17 = v19;
    if ( v19 >= 0 )
    {
      v20 = v19;
      v17 = sceNpCommerce2_66EC0829(v37, 769, a6, a7, 0);
      if ( v17 >= 0 )
      {
        v21 = sceNet_lib_D8722983(v37);
        v22 = a1 == 1 ? regcam_write_prefixed_field(a2, v38, 1, 51, v37, v21, 1) : sceNet_lib_D8722983(v38[0]) + 1 + v21 + 9;
        v17 = v22;
        if ( v22 >= 0 )
        {
          v23 = v20 + v22;
          if ( a8 != NULL && *a8 != 0 )
          {
            v34 = sceNet_lib_D8722983(a8);
            v17 = sceNpCommerce2_66EC0829(v37, 769, a8, v34, 0);
            if ( v17 < 0 )
              return v17;
            v35 = sceNet_lib_D8722983(v37);
            v36 = a1 == 1 ? regcam_write_prefixed_field(a2, v38, 1, 52, v37, v35, 1) : sceNet_lib_D8722983(v38[0]) + 1 + v35 + 15;
            v17 = v36;
            if ( v36 < 0 )
              return v17;
            v23 += v36;
          }
          v24 = a10;
          if ( a9 != NULL )
          {
            if ( *a9 != 0 )
            {
              v31 = sceNet_lib_D8722983(a9);
              v17 = sceNpCommerce2_66EC0829(v37, 769, a9, v31, 0);
              if ( v17 < 0 )
                return v17;
              v32 = sceNet_lib_D8722983(v37);
              v33 = a1 == 1 ? regcam_write_prefixed_field(a2, v38, 1, 53, v37, v32, 1) : sceNet_lib_D8722983(v38[0]) + 1 + v32 + 14;
              v17 = v33;
              if ( v33 < 0 )
                return v17;
              v23 += v33;
            }
            v24 = a10;
          }
          if ( (unsigned int)(v24 - 1) < 2 )
          {
            v28 = g_gender_strs[v24];
            v29 = sceNet_lib_D8722983(v28);
            if ( a1 == 1 )
              v30 = regcam_write_prefixed_field(a2, v38, 1, 54, v28, v29, 1);
            else
              v30 = sceNet_lib_D8722983(v38[0]) + 1 + v29 + 7;
            v17 = v30;
            if ( v30 < 0 )
              return v17;
            v23 += v30;
          }
          v25 = a1 == 1;
          v26 = v39;
          if ( !v25 )
          {
            *v39 = v23;
            v26[1] = HIDWORD(v23);
          }
          return 0;
        }
      }
    }
  }
  return v17;
}


int regcam_form_build_8(int a1, int a2, u64 *a3, int a4, int a5)
{
  int v7;
  s64 v9;
  const char *v11;
  int v12;
  int v13;
  const char *v14;
  int v15;
  int v17;
  int v18;
  int v19;

  v7 = 0;
  v9 = 0;
  if ( a4 != 0 )
  {
    v11 = "true";
    if ( a4 != 2 )
      v11 = "false";
    if ( a1 == 1 )
    {
      v13 = sceNpCommerce2_AE614CC6(a2, "scexOptinFlag=", 14);
      if ( v13 < 0 )
        return v13;
      v19 = sceNet_lib_D8722983(v11);
      v12 = sceNpCommerce2_AE614CC6(a2, v11, v19);
    }
    else
    {
      v12 = sceNet_lib_D8722983(v11) + 14;
    }
    v13 = v12;
    if ( v12 < 0 )
      return v13;
    v7 = 1;
    v9 = v12;
  }
  if ( a5 == 0 )
    goto LABEL_15;
  v14 = "true";
  if ( a5 != 2 )
    v14 = "false";
  if ( a1 == 1 )
  {
    if ( v7 != 1 || (v15 = sceNpCommerce2_AE614CC6(a2, "&", 1)) >= 0 )
    {
      v17 = sceNpCommerce2_AE614CC6(a2, "affiliatesOptinFlag=", 20);
      if ( v17 < 0 )
        return v17;
      v18 = sceNet_lib_D8722983(v14);
      v15 = sceNpCommerce2_AE614CC6(a2, v14, v18);
    }
  }
  else
  {
    v15 = v7 + sceNet_lib_D8722983(v14) + 20;
  }
  v13 = v15;
  if ( v15 >= 0 )
  {
    v9 += v15;
LABEL_15:
    v13 = 0;
    if ( a1 != 1 )
      *a3 = v9;
  }
  return v13;
}


int regcam_form_build_9(int a1, int a2, u64 *a3, int a4, int a5)
{
  int v10;
  int v11;
  int v12;
  s64 v13;
  int v14;
  int v15;
  s64 v16;
  int v18;
  int v19;
  int v20;
  int v21;
  u8 v22[784];

  v10 = sceNet_lib_D8722983(a4);
  v11 = sceNpCommerce2_66EC0829(v22, 769, a4, v10, 0);
  if ( v11 >= 0 )
  {
    if ( a1 == 1 )
    {
      v20 = sceNpCommerce2_AE614CC6(a2, "username=", 9);
      if ( v20 < 0 )
        return v20;
      v21 = sceNet_lib_D8722983(v22);
      v12 = sceNpCommerce2_AE614CC6(a2, v22, v21);
    }
    else
    {
      v12 = sceNet_lib_D8722983(v22) + 9;
    }
    v11 = v12;
    if ( v12 >= 0 )
    {
      v13 = v12;
      v14 = sceNet_lib_D8722983(a5);
      v11 = sceNpCommerce2_66EC0829(v22, 769, a5, v14, 0);
      if ( v11 >= 0 )
      {
        if ( a1 == 1 )
        {
          v18 = sceNpCommerce2_AE614CC6(a2, "&", 1);
          if ( v18 < 0 )
            return v18;
          v11 = sceNpCommerce2_AE614CC6(a2, "password=", 9);
          if ( v11 < 0 )
            return v11;
          v19 = sceNet_lib_D8722983(v22);
          v15 = sceNpCommerce2_AE614CC6(a2, v22, v19);
        }
        else
        {
          v15 = sceNet_lib_D8722983(v22) + 10;
        }
        v11 = v15;
        if ( v15 >= 0 )
        {
          v16 = v13 + v15;
          if ( a1 != 1 )
            *a3 = v16;
          return 0;
        }
      }
    }
  }
  return v11;
}


int regcam_form_build_10(int a1, int a2, u32 *a3, int a4, int a5, int a6, int a7, int a8)
{
  int v13;
  int v14;
  int v15;
  const char *v16;
  s64 v17;
  int v18;
  s64 v19;
  const char *v20;
  int v21;
  s64 v22;
  int v23;
  s64 v24;
  const char *v25;
  int v26;
  s64 v27;
  int v28; // dc
  u32 *v29;
  int v31;
  int v32;
  int v33;
  int v34;
  int v35;
  int v36;
  int v37;
  int v38;
  int v39;
  int v40;
  int v41;
  int v42;
  u8 v43[784];
  u8 v44[16];
  u32 *v45;
  int v46;
  int v47;

  v45 = a3;
  v46 = a6;
  v47 = a8;
  v13 = sceNet_lib_D8722983(a4);
  v14 = sceNpCommerce2_66EC0829(v43, 769, a4, v13, 0);
  if ( v14 >= 0 )
  {
    if ( a1 == 1 )
    {
      v41 = sceNpCommerce2_AE614CC6(a2, "subLoginName=", 13);
      if ( v41 < 0 )
        return v41;
      v42 = sceNet_lib_D8722983(v43);
      v15 = sceNpCommerce2_AE614CC6(a2, v43, v42);
    }
    else
    {
      v15 = sceNet_lib_D8722983(v43) + 13;
    }
    v14 = v15;
    if ( v15 >= 0 )
    {
      v16 = "true";
      v17 = v15;
      if ( a5 != 1 )
        v16 = "false";
      if ( a1 == 1 )
      {
        v14 = sceNpCommerce2_AE614CC6(a2, "&", 1);
        if ( v14 < 0 )
          return v14;
        v39 = sceNpCommerce2_AE614CC6(a2, "contentRating=", 14);
        if ( v39 < 0 )
          return v39;
        v40 = sceNet_lib_D8722983(v16);
        v18 = sceNpCommerce2_AE614CC6(a2, v16, v40);
      }
      else
      {
        v18 = sceNet_lib_D8722983(v16) + 15;
      }
      v14 = v18;
      if ( v18 >= 0 )
      {
        v19 = v17 + v18;
        v20 = "true";
        if ( v46 != 1 )
          v20 = "false";
        if ( a1 == 1 )
        {
          v14 = sceNpCommerce2_AE614CC6(a2, "&", 1);
          if ( v14 < 0 )
            return v14;
          v37 = sceNpCommerce2_AE614CC6(a2, "chatControl=", 12);
          if ( v37 < 0 )
            return v37;
          v38 = sceNet_lib_D8722983(v20);
          v21 = sceNpCommerce2_AE614CC6(a2, v20, v38);
        }
        else
        {
          v21 = sceNet_lib_D8722983(v20) + 13;
        }
        v14 = v21;
        if ( v21 >= 0 )
        {
          v22 = v19 + v21;
          if ( a1 == 1 )
          {
            v14 = sceNpCommerce2_AE614CC6(a2, "&", 1);
            if ( v14 < 0 )
              return v14;
            v35 = sceNpCommerce2_AE614CC6(a2, "searchControl=", 14);
            if ( v35 < 0 )
              return v35;
            v36 = sceNet_lib_D8722983("true");
            v23 = sceNpCommerce2_AE614CC6(a2, "true", v36);
          }
          else
          {
            v23 = sceNet_lib_D8722983("true") + 15;
          }
          v14 = v23;
          if ( v23 >= 0 )
          {
            v24 = v22 + v23;
            v25 = "true";
            if ( a7 != 1 )
              v25 = "false";
            if ( a1 == 1 )
            {
              v14 = sceNpCommerce2_AE614CC6(a2, "&", 1);
              if ( v14 < 0 )
                return v14;
              v33 = sceNpCommerce2_AE614CC6(a2, "spendingLimitFlag=", 18);
              if ( v33 < 0 )
                return v33;
              v34 = sceNet_lib_D8722983(v25);
              v26 = sceNpCommerce2_AE614CC6(a2, v25, v34);
            }
            else
            {
              v26 = sceNet_lib_D8722983(v25) + 19;
            }
            v14 = v26;
            if ( v26 >= 0 )
            {
              v27 = v24 + v26;
              if ( a7 != 1 )
              {
LABEL_24:
                v28 = a1 == 1;
                v29 = v45;
                if ( !v28 )
                {
                  *v45 = v27;
                  v29[1] = HIDWORD(v27);
                }
                return 0;
              }
              if ( a1 == 1 )
              {
                v32 = regcam_write_uint_field(a2, 62, v47, 1);
              }
              else
              {
                v31 = sceNet_lib_CF705E46(v44, "%u", v47);
                v32 = -2141909231;
                if ( v31 < 16 )
                  v32 = 12 + v31 + 3;
              }
              v14 = v32;
              if ( v32 >= 0 )
              {
                v27 += v32;
                goto LABEL_24;
              }
            }
          }
        }
      }
    }
  }
  return v14;
}


int regcam_form_build_11(int a1, int a2, u64 *a3, int a4, int a5)
{
  int v10;
  int v11;
  int v12;
  s64 v13;
  int v14;
  int v15;
  int v16;
  int v18;
  int v19;
  u8 v20[784];
  u32 v21[4];

  sceKernelMemset(v21, 0, 8);
  v21[0] = "account.";
  v10 = sceNet_lib_D8722983(a4);
  v11 = sceNpCommerce2_66EC0829(v20, 769, a4, v10, 0);
  if ( v11 >= 0 )
  {
    if ( a1 == 1 )
    {
      v18 = sceNpCommerce2_AE614CC6(a2, "subLoginName=", 13);
      if ( v18 < 0 )
        return v18;
      v19 = sceNet_lib_D8722983(v20);
      v12 = sceNpCommerce2_AE614CC6(a2, v20, v19);
    }
    else
    {
      v12 = sceNet_lib_D8722983(v20) + 13;
    }
    v11 = v12;
    if ( v12 >= 0 )
    {
      v13 = v12;
      v14 = sceNet_lib_D8722983(a5);
      v11 = sceNpCommerce2_66EC0829(v20, 769, a5, v14, 0);
      if ( v11 >= 0 )
      {
        v15 = sceNet_lib_D8722983(v20);
        v16 = a1 == 1 ? regcam_write_prefixed_field(a2, v21, 1, 4, v20, v15, 1) : sceNet_lib_D8722983(v21[0]) + 1 + v15 + 9;
        v11 = v16;
        if ( v16 >= 0 )
        {
          if ( a1 != 1 )
            *a3 = v13 + v16;
          return 0;
        }
      }
    }
  }
  return v11;
}


int regcam_form_build_12(int a1, int a2, u64 *a3, int a4, int a5)
{
  int v7;
  s64 v9;
  const char *v11;
  int v12;
  int v13;
  const char *v14;
  int v15;
  int v17;
  int v18;
  int v19;

  v7 = 0;
  v9 = 0;
  if ( a4 != 0 )
  {
    v11 = "true";
    if ( a4 != 2 )
      v11 = "false";
    if ( a1 == 1 )
    {
      v13 = sceNpCommerce2_AE614CC6(a2, "autoDepositFlag=", 16);
      if ( v13 < 0 )
        return v13;
      v19 = sceNet_lib_D8722983(v11);
      v12 = sceNpCommerce2_AE614CC6(a2, v11, v19);
    }
    else
    {
      v12 = sceNet_lib_D8722983(v11) + 16;
    }
    v13 = v12;
    if ( v12 < 0 )
      return v13;
    v7 = 1;
    v9 = v12;
  }
  if ( a5 == 0 )
    goto LABEL_15;
  v14 = "true";
  if ( a5 != 2 )
    v14 = "false";
  if ( a1 == 1 )
  {
    if ( v7 != 1 || (v15 = sceNpCommerce2_AE614CC6(a2, "&", 1)) >= 0 )
    {
      v17 = sceNpCommerce2_AE614CC6(a2, "commercePasswordFlag=", 21);
      if ( v17 < 0 )
        return v17;
      v18 = sceNet_lib_D8722983(v14);
      v15 = sceNpCommerce2_AE614CC6(a2, v14, v18);
    }
  }
  else
  {
    v15 = v7 + sceNet_lib_D8722983(v14) + 21;
  }
  v13 = v15;
  if ( v15 >= 0 )
  {
    v9 += v15;
LABEL_15:
    v13 = 0;
    if ( a1 != 1 )
      *a3 = v9;
  }
  return v13;
}


int regcam_form_build_13(int a1, int a2, u32 *a3, int a4, int a5, int a6, int a7)
{
  int v13;
  int v14;
  int v15;
  s64 v16;
  int v17;
  int v18;
  int v19;
  s64 v20;
  int v21;
  s64 v22;
  int v23;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  u8 v32[784];
  u8 v33[16];

  if ( a1 == 1 )
  {
    v14 = regcam_write_uint_field(a2, 2, a4, 0);
  }
  else
  {
    v13 = sceNet_lib_CF705E46(v33, "%u", a4);
    v14 = -2141909231;
    if ( v13 < 16 )
      v14 = v13 + 8;
  }
  v15 = v14;
  if ( v14 >= 0 )
  {
    v16 = v14;
    v17 = sceNet_lib_D8722983(a5);
    v15 = sceNpCommerce2_66EC0829(v32, 769, a5, v17, 0);
    if ( v15 >= 0 )
    {
      if ( a1 == 1 )
      {
        v29 = sceNpCommerce2_AE614CC6(a2, "&", 1);
        if ( v29 < 0 )
          return v29;
        v30 = sceNpCommerce2_AE614CC6(a2, "username=", 9);
        if ( v30 < 0 )
          return v30;
        v31 = sceNet_lib_D8722983(v32);
        v18 = sceNpCommerce2_AE614CC6(a2, v32, v31);
      }
      else
      {
        v18 = sceNet_lib_D8722983(v32) + 10;
      }
      v15 = v18;
      if ( v18 >= 0 )
      {
        v20 = v16 + v18;
        v19 = sceNet_lib_D8722983(a6);
        v15 = sceNpCommerce2_66EC0829(v32, 769, a6, v19, 0);
        if ( v15 >= 0 )
        {
          if ( a1 == 1 )
          {
            v27 = sceNpCommerce2_AE614CC6(a2, "&", 1);
            if ( v27 < 0 )
              return v27;
            v15 = sceNpCommerce2_AE614CC6(a2, "password=", 9);
            if ( v15 < 0 )
              return v15;
            v28 = sceNet_lib_D8722983(v32);
            v21 = sceNpCommerce2_AE614CC6(a2, v32, v28);
          }
          else
          {
            v21 = sceNet_lib_D8722983(v32) + 10;
          }
          v15 = v21;
          if ( v21 >= 0 )
          {
            v22 = v20 + v21;
            if ( a1 == 1 )
            {
              v25 = sceNpCommerce2_AE614CC6(a2, "&", 1);
              if ( v25 < 0 )
                return v25;
              v15 = sceNpCommerce2_AE614CC6(a2, "consoleId=", 10);
              if ( v15 < 0 )
                return v15;
              v26 = sceNet_lib_D8722983(a7);
              v23 = sceNpCommerce2_AE614CC6(a2, a7, v26);
            }
            else
            {
              v23 = sceNet_lib_D8722983(a7) + 11;
            }
            v15 = v23;
            if ( v23 >= 0 )
            {
              v22 = ((u64)(v22) & 0xFFFFFFFF00000000ULL) | (u32)(v22 + v23);
              if ( a1 != 1 )
              {
                *a3 = v22;
                a3[1] = HIDWORD(v22) + (v23 >> 31) + ((unsigned int)v22 < v23);
              }
              return 0;
            }
          }
        }
      }
    }
  }
  return v15;
}


int regcam_form_build_14(int a1, int a2, u32 *a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11)
{
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  s64 v22;
  int v23;
  int v24;
  s64 v25;
  int v26;
  int v27;
  s64 v28;
  int v29;
  int v30;
  s64 v31;
  int v32;
  int v33;
  s64 v34;
  int v35;
  int v36;
  s64 v37;
  int v38;
  int v39;
  s64 v40;
  int v41;
  int v42;
  u32 *v43;
  u8 v45[784];
  u32 v46[4];
  u32 *v47;
  int v48;

  v47 = a3;
  v48 = a8;
  sceKernelMemset(v46, 0, 8);
  v46[0] = "transactionSearchCriteria.";
  v17 = sceNet_lib_D8722983(a4);
  v18 = sceNpCommerce2_66EC0829(v45, 769, a4, v17, 0);
  if ( v18 >= 0 )
  {
    v19 = sceNet_lib_D8722983(v45);
    v20 = a1 == 1 ? regcam_write_prefixed_field(a2, v46, 1, 46, v45, v19, 0) : sceNet_lib_D8722983(v46[0]) + v19 + 10;
    v18 = v20;
    if ( v20 >= 0 )
    {
      v21 = a5;
      v22 = v20;
      sceNet_lib_CF705E46(v45, "%u", v21);
      v23 = sceNet_lib_D8722983(v45);
      v24 = a1 == 1 ? regcam_write_prefixed_field(a2, v46, 1, 65, v45, v23, 1) : sceNet_lib_D8722983(v46[0]) + 1 + v23 + 9;
      v18 = v24;
      if ( v24 >= 0 )
      {
        v25 = v22 + v24;
        sceNet_lib_CF705E46(v45, "%u", a6);
        v26 = sceNet_lib_D8722983(v45);
        v27 = a1 == 1 ? regcam_write_prefixed_field(a2, v46, 1, 66, v45, v26, 1) : sceNet_lib_D8722983(v46[0]) + 1 + v26 + 10;
        v18 = v27;
        if ( v27 >= 0 )
        {
          v28 = v25 + v27;
          sceNet_lib_CF705E46(v45, "%u", a7);
          v29 = sceNet_lib_D8722983(v45);
          v30 = a1 == 1 ? regcam_write_prefixed_field(a2, v46, 1, 67, v45, v29, 1) : sceNet_lib_D8722983(v46[0]) + 1 + v29 + 8;
          v18 = v30;
          if ( v30 >= 0 )
          {
            v31 = v28 + v30;
            sceNet_lib_CF705E46(v45, "%u", v48);
            v32 = sceNet_lib_D8722983(v45);
            v33 = a1 == 1 ? regcam_write_prefixed_field(a2, v46, 1, 68, v45, v32, 1) : sceNet_lib_D8722983(v46[0]) + 1 + v32 + 7;
            v18 = v33;
            if ( v33 >= 0 )
            {
              v34 = v31 + v33;
              sceNet_lib_CF705E46(v45, "%u", a9);
              v35 = sceNet_lib_D8722983(v45);
              v36 = a1 == 1 ? regcam_write_prefixed_field(a2, v46, 1, 69, v45, v35, 1) : sceNet_lib_D8722983(v46[0]) + 1 + v35 + 8;
              v18 = v36;
              if ( v36 >= 0 )
              {
                v37 = v34 + v36;
                sceNet_lib_CF705E46(v45, "%u", a10);
                v38 = sceNet_lib_D8722983(v45);
                v39 = a1 == 1 ? regcam_write_prefixed_field(a2, v46, 1, 70, v45, v38, 1) : sceNet_lib_D8722983(v46[0]) + 1 + v38 + 6;
                v18 = v39;
                if ( v39 >= 0 )
                {
                  v40 = v37 + v39;
                  sceNet_lib_CF705E46(v45, "%d", a11);
                  v41 = sceNet_lib_D8722983(v45);
                  v42 = a1 == 1 ? regcam_write_prefixed_field(a2, v46, 1, 71, v45, v41, 1) : sceNet_lib_D8722983(v46[0]) + 1 + v41 + 9;
                  v18 = v42;
                  if ( v42 >= 0 )
                  {
                    v40 = ((u64)(v40) & 0xFFFFFFFF00000000ULL) | (u32)(v40 + v42);
                    if ( a1 != 1 )
                    {
                      v43 = v47;
                      *v47 = v40;
                      v43[1] = HIDWORD(v40) + (v42 >> 31) + ((unsigned int)v40 < v42);
                    }
                    return 0;
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return v18;
}


int regcam_form_build_15(int a1, int a2, u32 *a3, int a4, int a5, int a6)
{
  int v12;
  int v13;
  int v14;
  s64 v15;
  int v16;
  int v17;
  s64 v18;
  int v19;
  int v20;
  int v22;
  int v23;
  u8 v24[784];
  u8 v25[16];

  v12 = sceNet_lib_D8722983(a4);
  v13 = sceNpCommerce2_66EC0829(v24, 769, a4, v12, 0);
  if ( v13 >= 0 )
  {
    if ( a1 == 1 )
    {
      v22 = sceNpCommerce2_AE614CC6(a2, "serviceId=", 10);
      if ( v22 < 0 )
        return v22;
      v23 = sceNet_lib_D8722983(v24);
      v14 = sceNpCommerce2_AE614CC6(a2, v24, v23);
    }
    else
    {
      v14 = sceNet_lib_D8722983(v24) + 10;
    }
    v13 = v14;
    if ( v14 >= 0 )
    {
      v15 = v14;
      if ( a1 == 1 )
      {
        v17 = regcam_write_uint_field(a2, 7, a5, 1);
      }
      else
      {
        v16 = sceNet_lib_CF705E46(v25, "%u", a5);
        v17 = -2141909231;
        if ( v16 < 16 )
          v17 = 12 + v16 + 3;
      }
      v13 = v17;
      if ( v17 >= 0 )
      {
        v18 = v15 + v17;
        if ( a1 == 1 )
        {
          v20 = regcam_write_uint_field(a2, 8, a6, 1);
        }
        else
        {
          v19 = sceNet_lib_CF705E46(v25, "%u", a6);
          v20 = -2141909231;
          if ( v19 < 16 )
            v20 = 16 + v19 + 3;
        }
        v13 = v20;
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
    }
  }
  return v13;
}


int regcam_form_build_16(int a1, int a2, u32 *a3, int a4, int a5, int a6)
{
  int v12;
  int v13;
  int v14;
  s64 v15;
  int v16;
  int v17;
  s64 v18;
  int v19;
  int v20;
  int v22;
  int v23;
  u8 v24[784];
  u8 v25[16];

  v12 = sceNet_lib_D8722983(a4);
  v13 = sceNpCommerce2_66EC0829(v24, 769, a4, v12, 0);
  if ( v13 >= 0 )
  {
    if ( a1 == 1 )
    {
      v22 = sceNpCommerce2_AE614CC6(a2, "serviceId=", 10);
      if ( v22 < 0 )
        return v22;
      v23 = sceNet_lib_D8722983(v24);
      v14 = sceNpCommerce2_AE614CC6(a2, v24, v23);
    }
    else
    {
      v14 = sceNet_lib_D8722983(v24) + 10;
    }
    v13 = v14;
    if ( v14 >= 0 )
    {
      v15 = v14;
      if ( a1 == 1 )
      {
        v17 = regcam_write_uint_field(a2, 7, a5, 1);
      }
      else
      {
        v16 = sceNet_lib_CF705E46(v25, "%u", a5);
        v17 = -2141909231;
        if ( v16 < 16 )
          v17 = 12 + v16 + 3;
      }
      v13 = v17;
      if ( v17 >= 0 )
      {
        v18 = v15 + v17;
        if ( a1 == 1 )
        {
          v20 = regcam_write_uint_field(a2, 8, a6, 1);
        }
        else
        {
          v19 = sceNet_lib_CF705E46(v25, "%u", a6);
          v20 = -2141909231;
          if ( v19 < 16 )
            v20 = 16 + v19 + 3;
        }
        v13 = v20;
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
    }
  }
  return v13;
}


int regcam_form_build_17(int a1, int a2, u32 *a3, int a4, int a5, int a6, int a7, int a8, int a9)
{
  int v15;
  int v16;
  int v17;
  s64 v18;
  int v19;
  int v20;
  int v21;
  s64 v22;
  int v23;
  int v24;
  s64 v25;
  int v26;
  int v27;
  s64 v28;
  int v29;
  int v30;
  s64 v31;
  int v32;
  u32 *v33;
  int v35;
  u8 v36[784];
  u32 v37[4];
  u8 v38[16];
  u32 *v39;
  int v40;

  v39 = a3;
  v40 = a8;
  sceKernelMemset(v37, 0, 8);
  v37[0] = "account.";
  if ( a1 == 1 )
  {
    v16 = regcam_write_uint_field(a2, 2, a4, 0);
  }
  else
  {
    v15 = sceNet_lib_CF705E46(v38, "%u", a4);
    v16 = -2141909231;
    if ( v15 < 16 )
      v16 = v15 + 8;
  }
  v17 = v16;
  if ( v16 >= 0 )
  {
    v18 = v16;
    v19 = sceNet_lib_D8722983(a5);
    v17 = sceNpCommerce2_66EC0829(v36, 769, a5, v19, 0);
    if ( v17 >= 0 )
    {
      v20 = sceNet_lib_D8722983(v36);
      v21 = a1 == 1 ? regcam_write_prefixed_field(a2, v37, 1, 46, v36, v20, 1) : sceNet_lib_D8722983(v37[0]) + 1 + v20 + 10;
      v17 = v21;
      if ( v21 >= 0 )
      {
        v22 = v18 + v21;
        sceNet_lib_CF705E46(v36, "%u", a6);
        v23 = sceNet_lib_D8722983(v36);
        v24 = a1 == 1 ? regcam_write_prefixed_field(a2, v37, 1, 43, v36, v23, 1) : sceNet_lib_D8722983(v37[0]) + 1 + v23 + 4;
        v17 = v24;
        if ( v24 >= 0 )
        {
          v25 = v22 + v24;
          sceNet_lib_CF705E46(v36, "%u", a7);
          v26 = sceNet_lib_D8722983(v36);
          v27 = a1 == 1 ? regcam_write_prefixed_field(a2, v37, 1, 44, v36, v26, 1) : sceNet_lib_D8722983(v37[0]) + 1 + v26 + 4;
          v17 = v27;
          if ( v27 >= 0 )
          {
            v28 = v25 + v27;
            sceNet_lib_CF705E46(v36, "%u", v40);
            v29 = sceNet_lib_D8722983(v36);
            v30 = a1 == 1 ? regcam_write_prefixed_field(a2, v37, 1, 45, v36, v29, 1) : sceNet_lib_D8722983(v37[0]) + 1 + v29 + 4;
            v17 = v30;
            if ( v30 >= 0 )
            {
              v31 = v28 + v30;
              if ( a1 == 1 )
              {
                v17 = sceNpCommerce2_AE614CC6(a2, "&", 1);
                if ( v17 < 0 )
                  return v17;
                v17 = sceNpCommerce2_AE614CC6(a2, "consoleId=", 10);
                if ( v17 < 0 )
                  return v17;
                v35 = sceNet_lib_D8722983(a9);
                v32 = sceNpCommerce2_AE614CC6(a2, a9, v35);
              }
              else
              {
                v32 = sceNet_lib_D8722983(a9) + 11;
              }
              v17 = v32;
              if ( v32 >= 0 )
              {
                v31 = ((u64)(v31) & 0xFFFFFFFF00000000ULL) | (u32)(v31 + v32);
                if ( a1 != 1 )
                {
                  v33 = v39;
                  *v39 = v31;
                  v33[1] = HIDWORD(v31) + (v32 >> 31) + ((unsigned int)v31 < v32);
                }
                return 0;
              }
            }
          }
        }
      }
    }
  }
  return v17;
}


int regcam_form_build_18(int a1, int a2, u32 *a3, int a4, int a5, int a6)
{
  int v12;
  int v13;
  int v14;
  int v15;
  s64 v16;
  int v17;
  int v18;
  int v19;
  s64 v20;
  u8 v22[784];
  u32 v23[4];

  sceKernelMemset(v23, 0, 8);
  v23[0] = "account.";
  sceNet_lib_CF705E46(v22, "%u", a4);
  v12 = sceNet_lib_D8722983(v22);
  if ( a1 == 1 )
    v13 = regcam_write_prefixed_field(a2, v23, 1, 47, v22, v12, 0);
  else
    v13 = sceNet_lib_D8722983(v23[0]) + v12 + 17;
  v14 = v13;
  if ( v13 >= 0 )
  {
    v15 = a5;
    v16 = v13;
    v14 = sceNpCommerce2_66EC0829(v22, 769, v15, a6, 0);
    if ( v14 >= 0 )
    {
      v17 = sceNet_lib_D8722983(v22);
      v18 = a1 == 1 ? regcam_write_prefixed_field(a2, v23, 1, 48, v22, v17, 1) : sceNet_lib_D8722983(v23[0]) + 1 + v17 + 15;
      v14 = v18;
      if ( v18 >= 0 )
      {
        v19 = v16 + v18;
        v20 = v16 + v18;
        if ( a1 != 1 )
        {
          *a3 = v19;
          a3[1] = HIDWORD(v20);
        }
        return 0;
      }
    }
  }
  return v14;
}


int regcam_form_build_19(int a1, int a2, u64 *a3, int a4)
{
  unsigned int v8;
  int result;
  int v10[4];
  u8 v11[16];

  v8 = 0;
  sceKernelMemset(v10, 0, 8);
  v10[0] = (int)"account.";
  while ( v8 < sceNet_lib_D8722983(a4) && v8 < 0x10 )
  {
    v11[v8] = sceNet_lib_96EF9DA1(*(char *)(a4 + v8));
    ++v8;
  }
  if ( v8 == 16 )
    v11[15] = 0;
  else
    v11[v8] = 0;
  result = regcam_emit_prefixed_field(a1, a2, v10, 1u, 42, (int)v11, 0);
  if ( result >= 0 )
  {
    if ( a1 != 1 )
      *a3 = result;
    return 0;
  }
  return result;
}


int regcam_write_prefixed_field(int a1, u32 *a2, unsigned int a3, int a4, int a5, int a6, int a7)
{
  unsigned int v13;
  int v14;
  int result;
  int v16;

  if ( a7 != 1 || (result = sceNpCommerce2_AE614CC6(a1, "&", 1)) >= 0 )
  {
    v13 = 0;
    if ( a3 != 0 )
    {
      do
      {
        ++v13;
        if ( *a2 != 0 )
        {
          v16 = sceNet_lib_D8722983(*a2);
          result = sceNpCommerce2_AE614CC6(a1, *a2, v16);
          v14 = v13 < a3;
          if ( result < 0 )
            return result;
        }
        else
        {
          v14 = v13 < a3;
        }
        ++a2;
      }
      while ( v14 );
    }
    result = sceNpCommerce2_AE614CC6(a1, g_form_keys[a4].str, g_form_keys[a4].len);
    if ( result >= 0 )
      return sceNpCommerce2_AE614CC6(a1, a5, a6);
  }
  return result;
}


int regcam_write_uint_field(int a1, int a2, int a3, int a4)
{
  int v7;
  int v8;
  int v9;
  u8 v11[16];

  if ( a4 != 1 || (v7 = sceNpCommerce2_AE614CC6(a1, "&", 1)) >= 0 )
  {
    v7 = sceNpCommerce2_AE614CC6(a1, g_form_keys[a2].str, g_form_keys[a2].len);
    if ( v7 >= 0 )
    {
      v8 = sceNet_lib_CF705E46(v11, "%u", a3);
      v7 = -2141909231;
      if ( v8 < 16 )
      {
        v9 = sceNet_lib_D8722983(v11);
        return sceNpCommerce2_AE614CC6(a1, v11, v9);
      }
    }
  }
  return v7;
}


