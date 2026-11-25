#include "store_internal.h"

int store_util_fn_1(int a1, int a2)
{
  return store_util_fn_27(a1, 11, a2, 0);
}


int store_util_fn_2(int a1, int a2)
{
  return store_util_fn_27(a1, 14, a2, 0);
}


int store_util_fn_3(unsigned int a1, int a2)
{
  int v4;
  int v5;
  int v6;
  int v7;
  s64 v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v14;
  int v15;
  s64 v16;

  v14 = 0;
  v15 = 0;
  v4 = sceNpCommerce2_0A6AB496(&g_store_mgr, HIWORD(a1), &v14, 1);
  if ( v4 < 0 )
    goto LABEL_14;
  v4 = sceNpCommerce2_851E7844(&g_store_mgr, a1, &v15);
  if ( v4 < 0 )
    goto LABEL_14;
  v6 = v14;
  v5 = sceNpCommerce2_FC47B63C(&v16);
  v7 = v14;
  if ( v5 < 0 )
    goto LABEL_10;
  v8 = *(u64 *)(v6 + 1072);
  if ( HIDWORD(v16) - HIDWORD(v8) != (unsigned int)v16 < (unsigned int)v8 )
  {
    v9 = *(u32 *)(v6 + 24);
LABEL_9:
    sceNpCommerce2_75DF99A9(v9, v6 + 28);
    v7 = v14;
LABEL_10:
    v10 = v15;
    goto LABEL_11;
  }
  v10 = v15;
  if ( (u64)(v16 - v8) >> 32 == 0 )
  {
    v11 = a2;
    if ( (unsigned int)(v16 - v8) <= 0x98967F )
      goto LABEL_12;
    v9 = *(u32 *)(v6 + 24);
    goto LABEL_9;
  }
LABEL_11:
  v11 = a2;
LABEL_12:
  v4 = store_req_fn_23(*(u32 *)(v7 + 1064), *(u32 *)(v7 + 24), v7 + 28, *(u16 *)(v10 + 20), (int *)(v10 + 16), v11);
  if ( v4 >= 0 )
    sceNpCommerce2_FC47B63C(v14 + 1072);
LABEL_14:
  if ( v15 != 0 )
  {
    if ( (*(u32 *)(v15 + 12) & 2) != 0 )
      v4 = -2141909232;
    sceNpCommerce2_8642C532(&g_store_mgr);
    v12 = v14;
  }
  else
  {
    v12 = v14;
  }
  if ( v12 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v4;
}


int store_util_fn_4(int a1, int a2)
{
  return store_util_fn_27(a1, 15, a2, 0);
}


int store_util_fn_5(unsigned int a1, int a2, int a3)
{
  int v6;
  int v7;
  int v8;
  int v9;
  s64 v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v16;
  int v17;
  s64 v18;

  v16 = 0;
  v17 = 0;
  v6 = sceNpCommerce2_0A6AB496(&g_store_mgr, HIWORD(a1), &v16, 1);
  if ( v6 < 0 )
    goto LABEL_14;
  v6 = sceNpCommerce2_851E7844(&g_store_mgr, a1, &v17);
  if ( v6 < 0 )
    goto LABEL_14;
  v8 = v16;
  v7 = sceNpCommerce2_FC47B63C(&v18);
  v9 = v16;
  if ( v7 < 0 )
    goto LABEL_10;
  v10 = *(u64 *)(v8 + 1072);
  if ( HIDWORD(v18) - HIDWORD(v10) != (unsigned int)v18 < (unsigned int)v10 )
  {
    v11 = *(u32 *)(v8 + 24);
LABEL_9:
    sceNpCommerce2_75DF99A9(v11, v8 + 28);
    v9 = v16;
LABEL_10:
    v12 = v17;
    goto LABEL_11;
  }
  v12 = v17;
  if ( (u64)(v18 - v10) >> 32 == 0 )
  {
    v13 = a2;
    if ( (unsigned int)(v18 - v10) <= 0x98967F )
      goto LABEL_12;
    v11 = *(u32 *)(v8 + 24);
    goto LABEL_9;
  }
LABEL_11:
  v13 = a2;
LABEL_12:
  v6 = store_req_fn_24(*(u32 *)(v9 + 1064), *(u32 *)(v9 + 24), v9 + 28, *(u16 *)(v12 + 20), (int *)(v12 + 16), v13, a3);
  if ( v6 >= 0 )
    sceNpCommerce2_FC47B63C(v16 + 1072);
LABEL_14:
  if ( v17 != 0 )
  {
    if ( (*(u32 *)(v17 + 12) & 2) != 0 )
      v6 = -2141909232;
    sceNpCommerce2_8642C532(&g_store_mgr);
    v14 = v16;
  }
  else
  {
    v14 = v16;
  }
  if ( v14 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v6;
}


int store_util_fn_6(int a1, int a2)
{
  return store_util_fn_27(a1, 12, a2, 0);
}


int store_util_fn_7(int a1, int a2)
{
  return store_util_fn_27(a1, 13, a2, 0);
}


int store_util_fn_8(unsigned int a1, int a2, int a3, int a4, int a5)
{
  int v10;
  int v11;
  int v12;
  int v13;
  s64 v14;
  int v15;
  int v16;
  int v18;
  int v19;
  s64 v20;

  v18 = 0;
  v19 = 0;
  v10 = sceNpCommerce2_0A6AB496(&g_store_mgr, HIWORD(a1), &v18, 1);
  if ( v10 < 0 )
    goto LABEL_13;
  v10 = sceNpCommerce2_851E7844(&g_store_mgr, a1, &v19);
  if ( v10 < 0 )
    goto LABEL_13;
  v12 = v18;
  v11 = sceNpCommerce2_FC47B63C(&v20);
  v13 = v19;
  if ( v11 >= 0 )
  {
    v14 = *(u64 *)(v12 + 1072);
    if ( HIDWORD(v20) - HIDWORD(v14) != (unsigned int)v20 < (unsigned int)v14 )
    {
      v15 = *(u32 *)(v12 + 24);
LABEL_10:
      sceNpCommerce2_75DF99A9(v15, v12 + 28);
      v13 = v19;
      goto LABEL_11;
    }
    if ( (u64)(v20 - v14) >> 32 != 0 )
    {
      v13 = v19;
      goto LABEL_11;
    }
    v13 = v19;
    if ( (unsigned int)(v20 - v14) > 0x98967F )
    {
      v15 = *(u32 *)(v12 + 24);
      goto LABEL_10;
    }
  }
LABEL_11:
  v10 = store_req_fn_25(
          *(u32 *)(v18 + 1064),
          *(u32 *)(v18 + 24),
          v18 + 28,
          *(u16 *)(v13 + 20),
          (int *)(v13 + 16),
          a2,
          a3,
          a4,
          a5);
  if ( v10 >= 0 )
    sceNpCommerce2_FC47B63C(v18 + 1072);
LABEL_13:
  if ( v19 != 0 )
  {
    if ( (*(u32 *)(v19 + 12) & 2) != 0 )
      v10 = -2141909232;
    sceNpCommerce2_8642C532(&g_store_mgr);
    v16 = v18;
  }
  else
  {
    v16 = v18;
  }
  if ( v16 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v10;
}


int store_util_fn_9(int a1, int a2)
{
  return store_util_fn_27(a1, 26, a2, 0);
}


int store_util_fn_10(unsigned int a1, int a2, int a3, int a4)
{
  int v8;
  int v9;
  int v10;
  int v11;
  s64 v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v18;
  int v19;
  s64 v20;

  v18 = 0;
  v19 = 0;
  v8 = sceNpCommerce2_0A6AB496(&g_store_mgr, HIWORD(a1), &v18, 1);
  if ( v8 < 0 )
    goto LABEL_14;
  v8 = sceNpCommerce2_851E7844(&g_store_mgr, a1, &v19);
  if ( v8 < 0 )
    goto LABEL_14;
  v10 = v18;
  v9 = sceNpCommerce2_FC47B63C(&v20);
  v11 = v18;
  if ( v9 < 0 )
    goto LABEL_10;
  v12 = *(u64 *)(v10 + 1072);
  if ( HIDWORD(v20) - HIDWORD(v12) != (unsigned int)v20 < (unsigned int)v12 )
  {
    v13 = *(u32 *)(v10 + 24);
LABEL_9:
    sceNpCommerce2_75DF99A9(v13, v10 + 28);
    v11 = v18;
LABEL_10:
    v14 = v19;
    goto LABEL_11;
  }
  v14 = v19;
  if ( (u64)(v20 - v12) >> 32 == 0 )
  {
    v15 = a2;
    if ( (unsigned int)(v20 - v12) <= 0x98967F )
      goto LABEL_12;
    v13 = *(u32 *)(v10 + 24);
    goto LABEL_9;
  }
LABEL_11:
  v15 = a2;
LABEL_12:
  v8 = store_req_fn_26(
         *(u32 *)(v11 + 1064),
         *(u32 *)(v11 + 24),
         v11 + 28,
         *(u16 *)(v14 + 20),
         (int *)(v14 + 16),
         v15,
         a3,
         a4);
  if ( v8 >= 0 )
    sceNpCommerce2_FC47B63C(v18 + 1072);
LABEL_14:
  if ( v19 != 0 )
  {
    if ( (*(u32 *)(v19 + 12) & 2) != 0 )
      v8 = -2141909232;
    sceNpCommerce2_8642C532(&g_store_mgr);
    v16 = v18;
  }
  else
  {
    v16 = v18;
  }
  if ( v16 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v8;
}


int store_util_fn_11(int a1, int a2)
{
  return store_util_fn_27(a1, 5, a2, 0);
}


int store_util_fn_12(unsigned int a1, int a2, int a3, int a4, int a5, int a6, int a7)
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
  int v24;
  int v25;
  s64 v26;

  v24 = 0;
  v25 = 0;
  v14 = sceNpCommerce2_0A6AB496(&g_store_mgr, HIWORD(a1), &v24, 1);
  if ( v14 < 0 )
    goto LABEL_14;
  v14 = sceNpCommerce2_851E7844(&g_store_mgr, a1, &v25);
  if ( v14 < 0 )
    goto LABEL_14;
  v16 = v24;
  v15 = sceNpCommerce2_FC47B63C(&v26);
  v17 = v25;
  if ( v15 < 0 )
    goto LABEL_10;
  v18 = *(u64 *)(v16 + 1072);
  if ( HIDWORD(v26) - HIDWORD(v18) != (unsigned int)v26 < (unsigned int)v18 )
  {
    v19 = *(u32 *)(v16 + 24);
LABEL_9:
    sceNpCommerce2_75DF99A9(v19, v16 + 28);
    v17 = v25;
LABEL_10:
    v20 = v24;
    goto LABEL_11;
  }
  v20 = v24;
  if ( (u64)(v26 - v18) >> 32 == 0 )
  {
    v21 = a2;
    if ( (unsigned int)(v26 - v18) <= 0x98967F )
      goto LABEL_12;
    v19 = *(u32 *)(v16 + 24);
    goto LABEL_9;
  }
LABEL_11:
  v21 = a2;
LABEL_12:
  v14 = store_req_fn_27(v21, *(u32 *)(v20 + 24), v20 + 28, *(u16 *)(v17 + 20), (int *)(v17 + 16), a3, a4, a5, a6, a7);
  if ( v14 >= 0 )
    sceNpCommerce2_FC47B63C(v24 + 1072);
LABEL_14:
  if ( v25 != 0 )
  {
    if ( (*(u32 *)(v25 + 12) & 2) != 0 )
      v14 = -2141909232;
    sceNpCommerce2_8642C532(&g_store_mgr);
    v22 = v24;
  }
  else
  {
    v22 = v24;
  }
  if ( v22 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v14;
}


int store_util_fn_13(int a1, int a2)
{
  return store_util_fn_27(a1, 59, a2, 0);
}


s32 store_util_fn_14(unsigned int a1)
{
  s32 v2;
  int v3;
  int v4;
  int v5;
  s64 v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v12;
  int v13;
  s64 v14;

  v12 = 0;
  v13 = 0;
  v2 = sceNpCommerce2_0A6AB496(&g_store_mgr, HIWORD(a1), &v12, 1);
  if ( v2 < 0 )
    goto LABEL_15;
  v2 = sceNpCommerce2_851E7844(&g_store_mgr, a1, &v13);
  if ( v2 < 0 )
    goto LABEL_15;
  v4 = v12;
  v3 = sceNpCommerce2_FC47B63C(&v14);
  v5 = v12;
  if ( v3 < 0 )
    goto LABEL_11;
  v6 = *(u64 *)(v4 + 1072);
  if ( HIDWORD(v14) - HIDWORD(v6) != (unsigned int)v14 < (unsigned int)v6 )
  {
    v7 = *(u32 *)(v4 + 24);
LABEL_10:
    sceNpCommerce2_75DF99A9(v7, v4 + 28);
    v5 = v12;
LABEL_11:
    v8 = v13;
    goto LABEL_12;
  }
  v8 = v13;
  if ( (u64)(v14 - v6) >> 32 == 0 )
  {
    if ( (unsigned int)(v14 - v6) <= 0x98967F )
    {
      v9 = *(u32 *)(v12 + 24);
      goto LABEL_13;
    }
    v7 = *(u32 *)(v4 + 24);
    goto LABEL_10;
  }
LABEL_12:
  v9 = *(u32 *)(v5 + 24);
LABEL_13:
  v2 = store_req_fn_16(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
  if ( v2 >= 0 )
    sceNpCommerce2_FC47B63C(v12 + 1072);
LABEL_15:
  if ( v13 != 0 )
  {
    if ( (*(u32 *)(v13 + 12) & 2) != 0 )
      v2 = -2141909232;
    sceNpCommerce2_8642C532(&g_store_mgr);
    v10 = v12;
  }
  else
  {
    v10 = v12;
  }
  if ( v10 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v2;
}


int store_util_fn_15(int a1, int a2)
{
  return store_util_fn_27(a1, 65, a2, 0);
}


s32 store_util_fn_16(unsigned int a1)
{
  s32 v2;
  int v3;
  int v4;
  int v5;
  s64 v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v12;
  int v13;
  s64 v14;

  v12 = 0;
  v13 = 0;
  v2 = sceNpCommerce2_0A6AB496(&g_store_mgr, HIWORD(a1), &v12, 1);
  if ( v2 < 0 )
    goto LABEL_15;
  v2 = sceNpCommerce2_851E7844(&g_store_mgr, a1, &v13);
  if ( v2 < 0 )
    goto LABEL_15;
  v4 = v12;
  v3 = sceNpCommerce2_FC47B63C(&v14);
  v5 = v12;
  if ( v3 < 0 )
    goto LABEL_11;
  v6 = *(u64 *)(v4 + 1072);
  if ( HIDWORD(v14) - HIDWORD(v6) != (unsigned int)v14 < (unsigned int)v6 )
  {
    v7 = *(u32 *)(v4 + 24);
LABEL_10:
    sceNpCommerce2_75DF99A9(v7, v4 + 28);
    v5 = v12;
LABEL_11:
    v8 = v13;
    goto LABEL_12;
  }
  v8 = v13;
  if ( (u64)(v14 - v6) >> 32 == 0 )
  {
    if ( (unsigned int)(v14 - v6) <= 0x98967F )
    {
      v9 = *(u32 *)(v12 + 24);
      goto LABEL_13;
    }
    v7 = *(u32 *)(v4 + 24);
    goto LABEL_10;
  }
LABEL_12:
  v9 = *(u32 *)(v5 + 24);
LABEL_13:
  v2 = store_req_fn_28(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
  if ( v2 >= 0 )
    sceNpCommerce2_FC47B63C(v12 + 1072);
LABEL_15:
  if ( v13 != 0 )
  {
    if ( (*(u32 *)(v13 + 12) & 2) != 0 )
      v2 = -2141909232;
    sceNpCommerce2_8642C532(&g_store_mgr);
    v10 = v12;
  }
  else
  {
    v10 = v12;
  }
  if ( v10 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v2;
}


int store_util_fn_17(int a1, int a2)
{
  return store_util_fn_27(a1, 66, a2, 0);
}


int store_util_fn_18(int a1, int a2)
{
  return store_util_fn_27(a1, 71, a2, 0);
}


int store_util_fn_19(unsigned int a1, int a2, int a3)
{
  int v6;
  int v7;
  int v8;
  int v9;
  s64 v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v16;
  int v17;
  s64 v18;

  v16 = 0;
  v17 = 0;
  v6 = sceNpCommerce2_0A6AB496(&g_store_mgr, HIWORD(a1), &v16, 1);
  if ( v6 < 0 )
    goto LABEL_14;
  v6 = sceNpCommerce2_851E7844(&g_store_mgr, a1, &v17);
  if ( v6 < 0 )
    goto LABEL_14;
  v8 = v16;
  v7 = sceNpCommerce2_FC47B63C(&v18);
  v9 = v16;
  if ( v7 < 0 )
    goto LABEL_10;
  v10 = *(u64 *)(v8 + 1072);
  if ( HIDWORD(v18) - HIDWORD(v10) != (unsigned int)v18 < (unsigned int)v10 )
  {
    v11 = *(u32 *)(v8 + 24);
LABEL_9:
    sceNpCommerce2_75DF99A9(v11, v8 + 28);
    v9 = v16;
LABEL_10:
    v12 = v17;
    goto LABEL_11;
  }
  v12 = v17;
  if ( (u64)(v18 - v10) >> 32 == 0 )
  {
    v13 = a2;
    if ( (unsigned int)(v18 - v10) <= 0x98967F )
      goto LABEL_12;
    v11 = *(u32 *)(v8 + 24);
    goto LABEL_9;
  }
LABEL_11:
  v13 = a2;
LABEL_12:
  v6 = store_req_fn_30(*(u32 *)(v9 + 1064), *(u32 *)(v9 + 24), v9 + 28, *(u16 *)(v12 + 20), (int *)(v12 + 16), v13, a3);
  if ( v6 >= 0 )
    sceNpCommerce2_FC47B63C(v16 + 1072);
LABEL_14:
  if ( v17 != 0 )
  {
    if ( (*(u32 *)(v17 + 12) & 2) != 0 )
      v6 = -2141909232;
    sceNpCommerce2_8642C532(&g_store_mgr);
    v14 = v16;
  }
  else
  {
    v14 = v16;
  }
  if ( v14 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v6;
}


int store_util_fn_20(int a1, int a2)
{
  return store_util_fn_27(a1, 76, a2, 0);
}


int store_util_fn_21(unsigned int a1, int a2, int a3, int a4, int a5, int a6)
{
  int v12;
  int v13;
  int v14;
  int v15;
  s64 v16;
  int v17;
  int v18;
  int v20;
  int v21;
  s64 v22;

  v20 = 0;
  v21 = 0;
  v12 = sceNpCommerce2_0A6AB496(&g_store_mgr, HIWORD(a1), &v20, 1);
  if ( v12 < 0 )
    goto LABEL_13;
  v12 = sceNpCommerce2_851E7844(&g_store_mgr, a1, &v21);
  if ( v12 < 0 )
    goto LABEL_13;
  v14 = v20;
  v13 = sceNpCommerce2_FC47B63C(&v22);
  v15 = v21;
  if ( v13 >= 0 )
  {
    v16 = *(u64 *)(v14 + 1072);
    if ( HIDWORD(v22) - HIDWORD(v16) != (unsigned int)v22 < (unsigned int)v16 )
    {
      v17 = *(u32 *)(v14 + 24);
LABEL_10:
      sceNpCommerce2_75DF99A9(v17, v14 + 28);
      v15 = v21;
      goto LABEL_11;
    }
    if ( (u64)(v22 - v16) >> 32 != 0 )
    {
      v15 = v21;
      goto LABEL_11;
    }
    v15 = v21;
    if ( (unsigned int)(v22 - v16) > 0x98967F )
    {
      v17 = *(u32 *)(v14 + 24);
      goto LABEL_10;
    }
  }
LABEL_11:
  v12 = store_req_fn_31(
          *(u32 *)(v20 + 1064),
          *(u32 *)(v20 + 24),
          v20 + 28,
          *(u16 *)(v15 + 20),
          (int *)(v15 + 16),
          a2,
          a3,
          a4,
          a5,
          a6);
  if ( v12 >= 0 )
    sceNpCommerce2_FC47B63C(v20 + 1072);
LABEL_13:
  if ( v21 != 0 )
  {
    if ( (*(u32 *)(v21 + 12) & 2) != 0 )
      v12 = -2141909232;
    sceNpCommerce2_8642C532(&g_store_mgr);
    v18 = v20;
  }
  else
  {
    v18 = v20;
  }
  if ( v18 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v12;
}


int store_util_fn_22(int a1, int a2)
{
  return store_util_fn_27(a1, 80, a2, 0);
}


int store_util_fn_23(unsigned int a1, int a2, int a3)
{
  int v6;
  int v7;
  int v8;
  int v9;
  s64 v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v16;
  int v17;
  s64 v18;

  v16 = 0;
  v17 = 0;
  v6 = sceNpCommerce2_0A6AB496(&g_store_mgr, HIWORD(a1), &v16, 1);
  if ( v6 < 0 )
    goto LABEL_14;
  v6 = sceNpCommerce2_851E7844(&g_store_mgr, a1, &v17);
  if ( v6 < 0 )
    goto LABEL_14;
  v8 = v16;
  v7 = sceNpCommerce2_FC47B63C(&v18);
  v9 = v16;
  if ( v7 < 0 )
    goto LABEL_10;
  v10 = *(u64 *)(v8 + 1072);
  if ( HIDWORD(v18) - HIDWORD(v10) != (unsigned int)v18 < (unsigned int)v10 )
  {
    v11 = *(u32 *)(v8 + 24);
LABEL_9:
    sceNpCommerce2_75DF99A9(v11, v8 + 28);
    v9 = v16;
LABEL_10:
    v12 = v17;
    goto LABEL_11;
  }
  v12 = v17;
  if ( (u64)(v18 - v10) >> 32 == 0 )
  {
    v13 = a2;
    if ( (unsigned int)(v18 - v10) <= 0x98967F )
      goto LABEL_12;
    v11 = *(u32 *)(v8 + 24);
    goto LABEL_9;
  }
LABEL_11:
  v13 = a2;
LABEL_12:
  v6 = store_req_fn_32(*(u32 *)(v9 + 1064), *(u32 *)(v9 + 24), v9 + 28, *(u16 *)(v12 + 20), (int *)(v12 + 16), v13, a3);
  if ( v6 >= 0 )
    sceNpCommerce2_FC47B63C(v16 + 1072);
LABEL_14:
  if ( v17 != 0 )
  {
    if ( (*(u32 *)(v17 + 12) & 2) != 0 )
      v6 = -2141909232;
    sceNpCommerce2_8642C532(&g_store_mgr);
    v14 = v16;
  }
  else
  {
    v14 = v16;
  }
  if ( v14 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v6;
}


int store_util_fn_24(int a1, int a2)
{
  int v3;
  u32 v5[4];

  v5[0] = 0;
  v3 = sceNpCommerce2_0A6AB496(&g_store_mgr, a1, v5, 1);
  if ( v3 >= 0 )
    v3 = sceNpCommerce2_EE7255FE(v5[0], a2);
  if ( v5[0] != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v3;
}


int store_util_fn_25(int a1, int a2)
{
  int v3;
  int v4;
  u32 v6[4];

  v6[0] = 0;
  v3 = sceNpCommerce2_0A6AB496(&g_store_mgr, a1, v6, 1);
  v4 = v6[0];
  if ( v3 >= 0 )
    *(u32 *)(v6[0] + 1064) = a2;
  if ( v4 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v3;
}


int store_util_fn_26(int a1, u32 *a2)
{
  int result;
  int v4;
  int v5;
  u32 v6[4];

  v6[0] = 0;
  result = sceNpCommerce2_0A6AB496(&g_store_mgr, a1, v6, 1);
  v4 = result;
  v5 = v6[0];
  if ( result >= 0 )
    *a2 = *(u32 *)(v6[0] + 1084);
  if ( v5 != 0 )
  {
    sceNpCommerce2_77BE411B(&g_store_mgr);
    return v4;
  }
  return result;
}


int store_util_fn_27(int a1, int a2, int a3)
{
  int v6;
  int v7;
  int v8;
  int v9;
  u32 v11[4];

  v11[0] = 0;
  v6 = sceNpCommerce2_0A6AB496(&g_store_mgr, a1, v11, 1);
  if ( v6 < 0
    || (v7 = sceNetIfhandle_lib_15CFE3C0(24), v6 = -2141909237, v8 = 0, v7 != 0)
    && (sceKernelMemset(v7, 0, 24), *(u16 *)(v7 + 20) = a2,
                                    (v6 = sceNpCommerce2_06618C81(&g_store_mgr, v7, a1, a3)) >= 0)
    || v7 == 0 )
  {
    v9 = v11[0];
  }
  else
  {
    sceNetIfhandle_lib_76BAD213(v7, v8);
    v9 = v11[0];
  }
  if ( v9 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v6;
}


int store_util_fn_28(int a1, int a2, int a3, int a4, unsigned int a5)
{
  u16 v9;
  int v10;
  int v12;
  int *v13;
  int v14;
  int v15; // dc
  int v16;
  int v18;
  u32 v19[24];
  u32 v20[12];
  u32 v21[16];

  v9 = *(u16 *)(a2 + 20);
  v10 = v9;
  if ( v9 == 49 )
  {
    v13 = v21;
    v18 = store_tlv_fn_36(v21, a3, a5);
    goto LABEL_18;
  }
  v12 = 1;
  if ( v9 < 0x32u || (v12 = 66, v9 != 61) )
  {
    v13 = v20;
    if ( v9 != v12 )
    {
LABEL_5:
      v10 = v9;
      goto LABEL_6;
    }
    v18 = store_tlv_fn_41(v20, a3, a5);
LABEL_18:
    if ( v18 >= 0 )
    {
      if ( store_tlv_fn_33(v13, v19) >= 0 )
      {
        sceNet_lib_B5CE388A(a1 + 1088, v19[16], 4);
        *(u8 *)(a1 + 1091) = 0;
        sceNet_lib_B5CE388A(a1 + 1092, v19[17], 4);
        *(u8 *)(a1 + 1091) = 0;
        sceNpCommerce2_F8D175FA(a3, a4, a1 + 1088, a1 + 1092);
      }
      v9 = *(u16 *)(a2 + 20);
    }
    else
    {
      v9 = *(u16 *)(a2 + 20);
    }
    goto LABEL_5;
  }
LABEL_6:
  if ( v10 != 49 )
  {
    if ( v10 >= 50 )
    {
      v14 = 66;
      if ( v10 != 61 )
      {
LABEL_9:
        v15 = v10 == v14;
        v16 = a5 < 0x401;
        if ( !v15 )
          return 0;
        goto LABEL_12;
      }
    }
    else
    {
      v14 = 29;
      if ( v10 != 1 )
        goto LABEL_9;
    }
  }
  v16 = a5 < 0x401;
LABEL_12:
  if ( v16 )
  {
    *(u32 *)(a1 + 1060) = a5;
    sceKernelMemcpy(a1 + 36, a3, a5);
  }
  else
  {
    *(u32 *)(a1 + 1060) = 0;
  }
  return 0;
}


