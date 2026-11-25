#include "regcam_internal.h"

signed int regcam_recv_response(unsigned int a1, int a2, unsigned int a3, int *a4)
{
  int v6;
  int v8;
  signed int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18; // dc
  int v19;
  int v20;
  int v21;
  int v22;
  int v24;
  char v25[4];
  int v26;
  int v27;
  int v28;
  int v29[4];

  v6 = a2 == 0 || a3 == 0;
  v8 = 0;
  v26 = 0;
  v27 = 0;
  if ( v6 || a4 == NULL )
  {
    v10 = -2141909245;
    goto LABEL_23;
  }
  v10 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v26, 1);
  if ( v10 < 0 )
    goto LABEL_22;
  v10 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v27);
  if ( v10 < 0 )
    goto LABEL_22;
  v8 = v27;
  if ( (unsigned int)*(u16 *)(v27 + 20) - 3 >= 2 )
  {
    v11 = v26;
  }
  else
  {
    v11 = v26;
    if ( *(u32 *)(v26 + 1080) != 1 )
    {
      v10 = -2141909224;
      if ( a3 <= 0x3FFFF )
        goto LABEL_23;
    }
  }
  v10 = sceNpCommerce2_4D43B84D(*(u32 *)(v27 + 16), v11 + 1084);
  if ( v10 < 0
    || (v12 = sceNpCommerce2_C27FEAB1(v26, *(u32 *)(v27 + 16), *(u16 *)(v27 + 20), v25, a2, a3),
        v10 = v12,
        v12 < 0)
    || (v13 = a2 + v12, v14 = sceNpCommerce2_D4C319C6(*(u32 *)(v27 + 16), a2, a2 + v12, a3, &v28), v10 = v14, v14 < 0) )
  {
LABEL_22:
    v8 = v27;
    goto LABEL_23;
  }
  v15 = v13 + v14;
  if ( v28 != 1 )
    goto LABEL_12;
  v8 = v27;
  v24 = *(u16 *)(v27 + 20);
  if ( v24 == 3 || v24 == 7 )
  {
    v16 = v14;
  }
  else
  {
    if ( v24 == 5 || v24 == 53 || v24 == 54 || v24 == 55 )
    {
LABEL_12:
      v16 = v14;
      goto LABEL_13;
    }
    v16 = v14;
    if ( v24 != 76 )
    {
      v10 = -2141909230;
      goto LABEL_23;
    }
  }
LABEL_13:
  v10 = sceNpCommerce2_ADAA054C(a2, a3, v16);
  if ( v10 < 0 )
    goto LABEL_22;
  v17 = (u8)v25[0];
  v18 = v25[0] == 0;
  *a4 = v15 - a2;
  if ( v18 )
  {
    v10 = 0;
    regcam_post_process_response(v26, v27, a2, a3, *a4);
    goto LABEL_22;
  }
  v8 = v27;
  v19 = *(u16 *)(v27 + 20);
  v10 = v17 | 0x80551000;
  if ( v19 == 38 )
  {
    if ( v10 == -2141908991 )
    {
      v10 = -2141908927;
      goto LABEL_23;
    }
    v21 = *a4;
  }
  else
  {
    v20 = 23;
    if ( *(u16 *)(v27 + 20) >= 0x27u )
      v20 = 64;
    if ( v19 == v20 )
    {
      if ( v10 == -2141908991 )
      {
        v10 = -2141908949;
        goto LABEL_23;
      }
      v21 = *a4;
    }
    else
    {
      v21 = *a4;
    }
  }
  regcam_tlv_parse_21(a2, v21, v29);
  v8 = v27;
  if ( v29[0] == 1 )
    v10 = -2141909101;
LABEL_23:
  if ( v8 != 0 )
  {
    if ( (*(u32 *)(v8 + 12) & 2) != 0 )
      v10 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v22 = v26;
  }
  else
  {
    v22 = v26;
  }
  if ( v22 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v10;
}


int regcam_dispatch_1(unsigned int a1)
{
  int v2;
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
  int v14;
  s64 v15;

  v12 = 0;
  v13 = 0;
  v2 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v12, 1);
  if ( v2 < 0 )
    goto LABEL_16;
  v2 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v13);
  if ( v2 < 0 )
    goto LABEL_16;
  v2 = sceNpCommerce2_F0900551(v12, &v14);
  if ( v2 < 0 )
    goto LABEL_16;
  v4 = v12;
  v3 = sceNpCommerce2_FC47B63C(&v15);
  v5 = v12;
  if ( v3 < 0 )
    goto LABEL_12;
  v6 = *(u64 *)(v4 + 1072);
  if ( (HIDWORD(v15) - HIDWORD(v6)) != ((unsigned int)v15 < (unsigned int)v6) )
  {
    v7 = *(u32 *)(v4 + 24);
LABEL_11:
    sceNpCommerce2_75DF99A9(v7, v4 + 28);
    v5 = v12;
LABEL_12:
    v8 = v13;
    goto LABEL_13;
  }
  v8 = v13;
  if ( (u64)(v15 - v6) >> 32 == 0 )
  {
    if ( (unsigned int)(v15 - v6) <= 0x98967F )
    {
      v9 = *(u32 *)(v12 + 24);
      goto LABEL_14;
    }
    v7 = *(u32 *)(v4 + 24);
    goto LABEL_11;
  }
LABEL_13:
  v9 = *(u32 *)(v5 + 24);
LABEL_14:
  v2 = regcam_send_req_8(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (int *)(v8 + 16), v14);
  if ( v2 >= 0 )
    sceNpCommerce2_FC47B63C(v12 + 1072);
LABEL_16:
  if ( v13 != 0 )
  {
    if ( (*(u32 *)(v13 + 12) & 2) != 0 )
      v2 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v10 = v12;
  }
  else
  {
    v10 = v12;
  }
  if ( v10 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v2;
}


int regcam_dispatch_2(unsigned int a1, int a2, int a3)
{
  int v6;
  int v7;
  int v8;
  int v9;
  s64 v10;
  int v11;
  int v12;
  u8 v14[80];
  int v15;
  int v16;
  int v17;
  s64 v18;

  v15 = 0;
  v16 = 0;
  v6 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v15, 1);
  if ( v6 < 0 )
    goto LABEL_15;
  v6 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v16);
  if ( v6 < 0 )
    goto LABEL_15;
  v6 = sceNpCommerce2_F0900551(v15, &v17);
  if ( v6 < 0 )
    goto LABEL_15;
  v6 = sceNpCommerce2_157FA8AA(v14, 65);
  if ( v6 < 0 )
    goto LABEL_15;
  v8 = v15;
  v7 = sceNpCommerce2_FC47B63C(&v18);
  v9 = v16;
  if ( v7 >= 0 )
  {
    v10 = *(u64 *)(v8 + 1072);
    if ( (HIDWORD(v18) - HIDWORD(v10)) != ((unsigned int)v18 < (unsigned int)v10) )
    {
      v11 = *(u32 *)(v8 + 24);
LABEL_12:
      sceNpCommerce2_75DF99A9(v11, v8 + 28);
      v9 = v16;
      goto LABEL_13;
    }
    if ( (u64)(v18 - v10) >> 32 != 0 )
    {
      v9 = v16;
      goto LABEL_13;
    }
    v9 = v16;
    if ( (unsigned int)(v18 - v10) > 0x98967F )
    {
      v11 = *(u32 *)(v8 + 24);
      goto LABEL_12;
    }
  }
LABEL_13:
  v6 = regcam_send_req_9(
         *(u32 *)(v15 + 1064),
         *(u32 *)(v15 + 24),
         v15 + 28,
         *(u16 *)(v9 + 20),
         (int *)(v9 + 16),
         v17,
         a2,
         a3,
         (int)v14);
  if ( v6 >= 0 )
    sceNpCommerce2_FC47B63C(v15 + 1072);
LABEL_15:
  if ( v16 != 0 )
  {
    if ( (*(u32 *)(v16 + 12) & 2) != 0 )
      v6 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v12 = v15;
  }
  else
  {
    v12 = v15;
  }
  if ( v12 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v6;
}


int regcam_dispatch_3(unsigned int a1, int a2, int a3, int a4, int a5, int a6)
{
  int v12;
  int v13;
  int v14;
  int v15;
  s64 v16;
  int v17;
  int v18;
  u8 v20[80];
  int v21;
  int v22;
  int v23;
  s64 v24;

  v21 = 0;
  v22 = 0;
  v12 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v21, 1);
  if ( v12 < 0 )
    goto LABEL_14;
  v12 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v22);
  if ( v12 < 0 )
    goto LABEL_14;
  v12 = sceNpCommerce2_F0900551(v21, &v23);
  if ( v12 < 0 )
    goto LABEL_14;
  v12 = sceNpCommerce2_157FA8AA(v20, 65);
  if ( v12 < 0 )
    goto LABEL_14;
  v14 = v21;
  v13 = sceNpCommerce2_FC47B63C(&v24);
  v15 = v22;
  if ( v13 >= 0 )
  {
    v16 = *(u64 *)(v14 + 1072);
    if ( (HIDWORD(v24) - HIDWORD(v16)) != ((unsigned int)v24 < (unsigned int)v16) )
    {
      v17 = *(u32 *)(v14 + 24);
LABEL_11:
      sceNpCommerce2_75DF99A9(v17, v14 + 28);
      v15 = v22;
      goto LABEL_12;
    }
    if ( (u64)(v24 - v16) >> 32 == 0 && (unsigned int)(v24 - v16) > 0x98967F )
    {
      v17 = *(u32 *)(v14 + 24);
      goto LABEL_11;
    }
  }
LABEL_12:
  v12 = regcam_send_req_10(
          *(u32 *)(v21 + 1064),
          *(u32 *)(v21 + 24),
          v21 + 28,
          *(u16 *)(v15 + 20),
          (int *)(v15 + 16),
          v23,
          a2,
          a3,
          a4,
          a5,
          a6,
          (int)v20);
  if ( v12 >= 0 )
    sceNpCommerce2_FC47B63C(v21 + 1072);
LABEL_14:
  if ( v22 != 0 )
  {
    if ( (*(u32 *)(v22 + 12) & 2) != 0 )
      v12 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v18 = v21;
  }
  else
  {
    v18 = v21;
  }
  if ( v18 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v12;
}


int regcam_dispatch_4(unsigned int a1, int a2, u8 *a3, u8 *a4)
{
  int v8;
  int v9;
  int v10;
  int v11;
  s64 v12;
  int v13;
  int v14;
  u8 v16[80];
  u8 v17[48];
  u8 v18[80];
  int v19;
  int v20;
  int v21;
  s64 v22;

  v19 = 0;
  v20 = 0;
  v8 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v19, 1);
  if ( v8 >= 0 )
  {
    v8 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v20);
    if ( v8 >= 0 )
    {
      v8 = sceNpCommerce2_F0900551(v19, &v21);
      if ( v8 >= 0 )
      {
        if ( a2 == 0 )
        {
          v8 = sceNpCommerce2_02CADA41(v19, v18, v17, 0);
          if ( v8 < 0 )
            goto LABEL_18;
          a3 = v18;
          a4 = v17;
        }
        v8 = sceNpCommerce2_157FA8AA(v16, 65);
        if ( v8 >= 0 )
        {
          v10 = v19;
          v9 = sceNpCommerce2_FC47B63C(&v22);
          v11 = v20;
          if ( v9 < 0 )
            goto LABEL_16;
          v12 = *(u64 *)(v10 + 1072);
          if ( (HIDWORD(v22) - HIDWORD(v12)) == ((unsigned int)v22 < (unsigned int)v12) )
          {
            if ( (u64)(v22 - v12) >> 32 != 0 )
            {
              v11 = v20;
              goto LABEL_16;
            }
            v11 = v20;
            if ( (unsigned int)(v22 - v12) <= 0x98967F )
            {
LABEL_16:
              v8 = regcam_send_req_28(
                     *(u32 *)(v19 + 1064),
                     *(u32 *)(v19 + 24),
                     v19 + 28,
                     *(u16 *)(v11 + 20),
                     (int *)(v11 + 16),
                     v21,
                     (int)a3,
                     (int)a4,
                     (int)v16);
              if ( v8 >= 0 )
                sceNpCommerce2_FC47B63C(v19 + 1072);
              goto LABEL_18;
            }
            v13 = *(u32 *)(v10 + 24);
          }
          else
          {
            v13 = *(u32 *)(v10 + 24);
          }
          sceNpCommerce2_75DF99A9(v13, v10 + 28);
          v11 = v20;
          goto LABEL_16;
        }
      }
    }
  }
LABEL_18:
  if ( v20 != 0 )
  {
    if ( (*(u32 *)(v20 + 12) & 2) != 0 )
      v8 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v14 = v19;
  }
  else
  {
    v14 = v19;
  }
  if ( v14 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v8;
}


int regcam_dispatch_5(unsigned int a1, int a2, u8 *a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)
{
  int v17;
  u8 *v18;
  int v19;
  int v20;
  int v21;
  s64 v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  u8 v29[80];
  int v30;
  int v31;
  s64 v32;
  int v33;

  v33 = a4;
  v30 = 0;
  v31 = 0;
  v17 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v30, 1);
  if ( v17 < 0 || (v17 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v31)) < 0 )
  {
LABEL_16:
    v26 = v31;
    goto LABEL_17;
  }
  v18 = a3;
  if ( a2 != 0 || (v17 = sceNpCommerce2_02CADA41(v30, v29, 0, 0), v18 = v29, v17 >= 0) )
  {
    v20 = v30;
    v19 = sceNpCommerce2_FC47B63C(&v32);
    v21 = v31;
    if ( v19 >= 0 )
    {
      v22 = *(u64 *)(v20 + 1072);
      if ( (HIDWORD(v32) - HIDWORD(v22)) == ((unsigned int)v32 < (unsigned int)v22) )
      {
        v24 = a9;
        if ( (u64)(v32 - v22) >> 32 != 0 )
        {
LABEL_13:
          v25 = a10;
LABEL_14:
          v17 = regcam_send_req_29(
                  *(u32 *)(v30 + 1064),
                  *(u32 *)(v30 + 24),
                  v30 + 28,
                  *(u16 *)(v21 + 20),
                  (int *)(v21 + 16),
                  (int)v18,
                  v33,
                  a5,
                  a6,
                  a7,
                  a8,
                  v24,
                  v25);
          if ( v17 >= 0 )
            sceNpCommerce2_FC47B63C(v30 + 1072);
          goto LABEL_16;
        }
        if ( (unsigned int)(v32 - v22) <= 0x98967F )
        {
          v25 = a10;
          goto LABEL_14;
        }
        v23 = *(u32 *)(v20 + 24);
      }
      else
      {
        v23 = *(u32 *)(v20 + 24);
      }
      sceNpCommerce2_75DF99A9(v23, v20 + 28);
      v21 = v31;
    }
    v24 = a9;
    goto LABEL_13;
  }
  v26 = v31;
LABEL_17:
  if ( v26 != 0 )
  {
    if ( (*(u32 *)(v26 + 12) & 2) != 0 )
      v17 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v27 = v30;
  }
  else
  {
    v27 = v30;
  }
  if ( v27 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v17;
}


int regcam_dispatch_6(unsigned int a1, int a2, int a3, int a4, int a5)
{
  int v10;
  int v11;
  int v12;
  int v13;
  s64 v14;
  int v15;
  int v16;
  u8 v18[80];
  int v19;
  int v20;
  int v21;
  s64 v22;

  v19 = 0;
  v20 = 0;
  v10 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v19, 1);
  if ( v10 < 0 )
    goto LABEL_14;
  v10 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v20);
  if ( v10 < 0 )
    goto LABEL_14;
  v10 = sceNpCommerce2_F0900551(v19, &v21);
  if ( v10 < 0 )
    goto LABEL_14;
  v10 = sceNpCommerce2_157FA8AA(v18, 65);
  if ( v10 < 0 )
    goto LABEL_14;
  v12 = v19;
  v11 = sceNpCommerce2_FC47B63C(&v22);
  v13 = v20;
  if ( v11 >= 0 )
  {
    v14 = *(u64 *)(v12 + 1072);
    if ( (HIDWORD(v22) - HIDWORD(v14)) != ((unsigned int)v22 < (unsigned int)v14) )
    {
      v15 = *(u32 *)(v12 + 24);
LABEL_11:
      sceNpCommerce2_75DF99A9(v15, v12 + 28);
      v13 = v20;
      goto LABEL_12;
    }
    if ( (u64)(v22 - v14) >> 32 == 0 && (unsigned int)(v22 - v14) > 0x98967F )
    {
      v15 = *(u32 *)(v12 + 24);
      goto LABEL_11;
    }
  }
LABEL_12:
  v10 = regcam_send_req_34(
          *(u32 *)(v19 + 1064),
          *(u32 *)(v19 + 24),
          v19 + 28,
          *(u16 *)(v13 + 20),
          (int *)(v13 + 16),
          v21,
          a2,
          a3,
          a4,
          a5,
          (int)v18);
  if ( v10 >= 0 )
    sceNpCommerce2_FC47B63C(v19 + 1072);
LABEL_14:
  if ( v20 != 0 )
  {
    if ( (*(u32 *)(v20 + 12) & 2) != 0 )
      v10 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v16 = v19;
  }
  else
  {
    v16 = v19;
  }
  if ( v16 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v10;
}


int regcam_validate_field(unsigned int a1, u8 *a2, int *a3)
{
  int v3;
  int v7;
  int v8;
  char v9;
  unsigned int v10;
  char v11;
  s64 v12;
  int v13;
  unsigned int v14;
  char *v15;
  unsigned int v16;
  unsigned int v17;
  char *v18;
  int v19;
  unsigned int v20;
  unsigned int i;
  unsigned int v22;
  unsigned int v23;
  int j;
  char *v25;
  int v27;
  int v28;
  unsigned int v29;
  unsigned int v30;
  unsigned int v31;
  u8 *v32;
  unsigned int v33;
  char *v34;
  u8 *v35;
  unsigned int v36;
  unsigned int v37;
  char *v38;
  int v39;
  u8 v40[16];

  v3 = 0;
  switch ( a1 )
  {
    case 0x10u:
      if ( sceNet_lib_D8722983(a2) == 0 )
        goto LABEL_7;
      if ( (unsigned int)sceNet_lib_D8722983(a2) >= 3 )
      {
        if ( (unsigned int)sceNet_lib_D8722983(a2) < 0x11 )
        {
          v9 = *a2;
LABEL_12:
          v10 = 1;
          if ( (u8)(v9 - 65) >= 0x1Au && (u8)(v9 - 97) >= 0x1Au )
            *a3 |= 0x40u;
          while ( 1 )
          {
            while ( 1 )
            {
              if ( v10 >= sceNet_lib_D8722983(a2) )
                goto LABEL_21;
              v11 = a2[v10];
              v12 = ((u64)(v12) & 0x00000000FFFFFFFFULL) | ((u64)(u32)((u8)(v11 - 97) < 0x1Au) << 32);
              v12 = ((u64)(v12) & 0xFFFFFFFF00000000ULL) | (u32)((u8)(v11 - 65) < 0x1Au);
              if ( v12 == 0 && (u8)(v11 - 48) >= 0xAu )
                break;
LABEL_20:
              ++v10;
            }
            if ( v11 != 95 )
            {
              if ( v11 != 45 )
                goto LABEL_21;
              goto LABEL_20;
            }
            ++v10;
          }
        }
        v8 = *a3;
      }
      else
      {
        v8 = *a3;
      }
      *a3 = v8 | 2;
      v9 = *a2;
      goto LABEL_12;
    case 0x27u:
      v36 = 0;
      if ( sceNet_lib_D8722983(a2) == 0 )
        goto LABEL_7;
      while ( 1 )
      {
        v37 = sceNet_lib_D8722983(a2);
        v38 = &a2[v36];
        if ( v36 >= v37 || *v38 < 0 || (sceNet_lib_750F705D(*v38) & 0x97) == 0 )
          break;
        ++v36;
      }
      if ( v36 != sceNet_lib_D8722983(a2) )
        *a3 |= 0x80u;
      if ( *a2 == 64 )
      {
        v13 = *a3;
        goto LABEL_24;
      }
      if ( a2[sceNet_lib_D8722983(a2) - 1] == 64 )
      {
        v13 = *a3;
        goto LABEL_24;
      }
      v39 = sceNet_lib_BCBE14CF(a2, 64);
      if ( v39 == 0 )
      {
        v13 = *a3;
        goto LABEL_24;
      }
      if ( sceNet_lib_BCBE14CF(v39, 46) == 0 )
      {
        v13 = *a3;
        goto LABEL_24;
      }
      return 0;
    case 0x28u:
      v16 = 0;
      if ( sceNet_lib_D8722983(a2) == 0 )
        goto LABEL_7;
      while ( 1 )
      {
        v17 = sceNet_lib_D8722983(a2);
        v18 = &a2[v16];
        if ( v16 >= v17 || *v18 < 0 || (sceNet_lib_750F705D(*v18) & 0x97) == 0 )
          break;
        ++v16;
      }
      if ( v16 != sceNet_lib_D8722983(a2) )
        *a3 |= 0x80u;
      if ( (unsigned int)sceNet_lib_D8722983(a2) >= 8 )
      {
        v20 = 0;
        if ( (unsigned int)sceNet_lib_D8722983(a2) < 0x1F )
          goto LABEL_45;
        v19 = *a3;
      }
      else
      {
        v19 = *a3;
      }
      *a3 = v19 | 2;
      v20 = 0;
LABEL_45:
      while ( v20 < sceNet_lib_D8722983(a2) && (u8)(a2[v20] - 48) >= 0xAu )
        ++v20;
      if ( v20 == sceNet_lib_D8722983(a2) )
        *a3 |= 4u;
      for ( i = 0;
            i < sceNet_lib_D8722983(a2)
         && (u8)(a2[i] - 65) >= 0x1Au
         && (u8)(a2[i] - 97) >= 0x1Au;
            ++i )
      {
        ;
      }
      if ( i == sceNet_lib_D8722983(a2) )
        *a3 |= 8u;
      v22 = 0;
      v23 = 0;
      for ( j = 0; ; j = *v25 )
      {
        v25 = &a2[v22];
        if ( v22++ >= sceNet_lib_D8722983(a2) )
          break;
        v27 = sceNet_lib_C13C9307(j);
        v28 = v27 ^ sceNet_lib_C13C9307(*v25);
        v29 = v23 + 1;
        v23 = 1;
        if ( v28 == 0 )
          v23 = v29;
        if ( v23 >= 3 )
        {
          *a3 |= 0x10u;
          break;
        }
      }
      v30 = 0;
      v31 = 0;
      v32 = a2;
      while ( v30 < sceNet_lib_D8722983(a2) )
      {
        v33 = 0;
        if ( v31 != 0 )
        {
          v34 = v40;
          do
          {
            if ( *v34 == (char)*v32 )
              break;
            v34 = &v40[++v33];
          }
          while ( v33 < v31 );
        }
        if ( v33 == v31 )
        {
          v35 = &v40[v31++];
          *v35 = *v32;
        }
        ++v32;
        if ( v31 >= 3 )
          break;
        ++v30;
      }
      if ( v30 == sceNet_lib_D8722983(a2) )
      {
        v7 = *a3 | 0x20;
        goto LABEL_8;
      }
      return 0;
    case 0xFF00u:
      v10 = 0;
      if ( sceNet_lib_D8722983(a2) != 0 )
      {
        while ( 1 )
        {
          v14 = sceNet_lib_D8722983(a2);
          v15 = &a2[v10];
          if ( v10 >= v14 || *v15 < 0 || (sceNet_lib_750F705D(*v15) & 0x97) == 0 )
            break;
          ++v10;
        }
LABEL_21:
        if ( v10 == sceNet_lib_D8722983(a2) )
          return 0;
        v13 = *a3;
LABEL_24:
        v7 = v13 | 0x80;
        goto LABEL_8;
      }
LABEL_7:
      v7 = *a3 | 1;
LABEL_8:
      *a3 = v7;
      return v3;
    default:
      return -2141909245;
  }
}


int regcam_init_internal()
{
  sceKernelMemset(&g_regcam_mgr, 0, 4);
  return 0;
}


void regcam_term_internal()
{
  ;
}


int regcam_destroy_req_internal(unsigned int a1)
{
  int v2;
  int v4;
  u32 v5[3];

  v4 = 0;
  v5[0] = 0;
  v2 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v4, 1);
  if ( v2 >= 0 )
  {
    v2 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, v5);
    if ( v2 >= 0 )
    {
      v2 = 0;
      sceNpCommerce2_02C307C0(&g_regcam_mgr, a1);
    }
  }
  if ( v5[0] != 0 )
    sceNpCommerce2_8642C532(&g_regcam_mgr);
  if ( v4 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v2;
}


int regcam_abort_req_internal(unsigned int a1)
{
  int v2;
  int v3;
  int v5;
  u32 v6[3];

  v5 = 0;
  v6[0] = 0;
  v2 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v5, 1);
  if ( v2 < 0 || (v2 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, v6)) < 0 )
  {
    v3 = v6[0];
  }
  else
  {
    v2 = 0;
    sceNpCommerce2_63B96D73(*(u32 *)(v6[0] + 16));
    v3 = v6[0];
    *(u32 *)(v6[0] + 12) |= 2u;
  }
  if ( v3 != 0 )
    sceNpCommerce2_8642C532(&g_regcam_mgr);
  if ( v5 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v2;
}


int regcam_dispatch_7(int a1, int a2)
{
  return regcam_create_req(a1, 2, a2);
}


s32 regcam_dispatch_8(unsigned int a1)
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
  v2 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v12, 1);
  if ( v2 < 0 )
    goto LABEL_15;
  v2 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v13);
  if ( v2 < 0 )
    goto LABEL_15;
  v4 = v12;
  v3 = sceNpCommerce2_FC47B63C(&v14);
  v5 = v12;
  if ( v3 < 0 )
    goto LABEL_11;
  v6 = *(u64 *)(v4 + 1072);
  if ( (HIDWORD(v14) - HIDWORD(v6)) != ((unsigned int)v14 < (unsigned int)v6) )
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
  v2 = regcam_send_req_1(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
  if ( v2 >= 0 )
  {
    sceNpCommerce2_FC47B63C(v12 + 1072);
    *(u32 *)(v12 + 1060) = 0;
  }
LABEL_15:
  if ( v13 != 0 )
  {
    if ( (*(u32 *)(v13 + 12) & 2) != 0 )
      v2 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v10 = v12;
  }
  else
  {
    v10 = v12;
  }
  if ( v10 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v2;
}


s32 regcam_dispatch_9(unsigned int a1)
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
  v2 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v12, 1);
  if ( v2 < 0 )
    goto LABEL_15;
  v2 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v13);
  if ( v2 < 0 )
    goto LABEL_15;
  v4 = v12;
  v3 = sceNpCommerce2_FC47B63C(&v14);
  v5 = v12;
  if ( v3 < 0 )
    goto LABEL_11;
  v6 = *(u64 *)(v4 + 1072);
  if ( (HIDWORD(v14) - HIDWORD(v6)) != ((unsigned int)v14 < (unsigned int)v6) )
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
  v2 = regcam_send_req_3(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
  if ( v2 >= 0 )
    sceNpCommerce2_FC47B63C(v12 + 1072);
LABEL_15:
  if ( v13 != 0 )
  {
    if ( (*(u32 *)(v13 + 12) & 2) != 0 )
      v2 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v10 = v12;
  }
  else
  {
    v10 = v12;
  }
  if ( v10 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v2;
}


int regcam_dispatch_10(int a1, int a2)
{
  return regcam_create_req(a1, 20, a2);
}


s32 regcam_dispatch_11(unsigned int a1)
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
  v2 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v12, 1);
  if ( v2 < 0 )
    goto LABEL_15;
  v2 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v13);
  if ( v2 < 0 )
    goto LABEL_15;
  v4 = v12;
  v3 = sceNpCommerce2_FC47B63C(&v14);
  v5 = v12;
  if ( v3 < 0 )
    goto LABEL_11;
  v6 = *(u64 *)(v4 + 1072);
  if ( (HIDWORD(v14) - HIDWORD(v6)) != ((unsigned int)v14 < (unsigned int)v6) )
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
  v2 = regcam_send_req_4(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
  if ( v2 >= 0 )
    sceNpCommerce2_FC47B63C(v12 + 1072);
LABEL_15:
  if ( v13 != 0 )
  {
    if ( (*(u32 *)(v13 + 12) & 2) != 0 )
      v2 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v10 = v12;
  }
  else
  {
    v10 = v12;
  }
  if ( v10 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v2;
}


int regcam_dispatch_12(int a1, int a2)
{
  return regcam_create_req(a1, 68, a2);
}


int regcam_dispatch_13(unsigned int a1, int a2)
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
  v4 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v14, 1);
  if ( v4 < 0 )
    goto LABEL_14;
  v4 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v15);
  if ( v4 < 0 )
    goto LABEL_14;
  v6 = v14;
  v5 = sceNpCommerce2_FC47B63C(&v16);
  v7 = v14;
  if ( v5 < 0 )
    goto LABEL_10;
  v8 = *(u64 *)(v6 + 1072);
  if ( (HIDWORD(v16) - HIDWORD(v8)) != ((unsigned int)v16 < (unsigned int)v8) )
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
  v4 = regcam_send_req_5(*(u32 *)(v7 + 1064), *(u32 *)(v7 + 24), v7 + 28, *(u16 *)(v10 + 20), (u32 *)(v10 + 16), v11);
  if ( v4 >= 0 )
    sceNpCommerce2_FC47B63C(v14 + 1072);
LABEL_14:
  if ( v15 != 0 )
  {
    if ( (*(u32 *)(v15 + 12) & 2) != 0 )
      v4 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v12 = v14;
  }
  else
  {
    v12 = v14;
  }
  if ( v12 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v4;
}


int regcam_dispatch_14(int a1, int a2)
{
  return regcam_create_req(a1, 70, a2);
}


s32 regcam_dispatch_15(unsigned int a1)
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
  v2 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v12, 1);
  if ( v2 < 0 )
    goto LABEL_15;
  v2 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v13);
  if ( v2 < 0 )
    goto LABEL_15;
  v4 = v12;
  v3 = sceNpCommerce2_FC47B63C(&v14);
  v5 = v12;
  if ( v3 < 0 )
    goto LABEL_11;
  v6 = *(u64 *)(v4 + 1072);
  if ( (HIDWORD(v14) - HIDWORD(v6)) != ((unsigned int)v14 < (unsigned int)v6) )
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
  v2 = regcam_send_req_6(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
  if ( v2 >= 0 )
    sceNpCommerce2_FC47B63C(v12 + 1072);
LABEL_15:
  if ( v13 != 0 )
  {
    if ( (*(u32 *)(v13 + 12) & 2) != 0 )
      v2 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v10 = v12;
  }
  else
  {
    v10 = v12;
  }
  if ( v10 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v2;
}


int regcam_dispatch_16(int a1, int a2)
{
  return regcam_create_req(a1, 23, a2);
}


int regcam_dispatch_17(unsigned int a1, int a2)
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
  v4 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v14, 1);
  if ( v4 < 0 )
    goto LABEL_14;
  v4 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v15);
  if ( v4 < 0 )
    goto LABEL_14;
  v6 = v14;
  v5 = sceNpCommerce2_FC47B63C(&v16);
  v7 = v14;
  if ( v5 < 0 )
    goto LABEL_10;
  v8 = *(u64 *)(v6 + 1072);
  if ( (HIDWORD(v16) - HIDWORD(v8)) != ((unsigned int)v16 < (unsigned int)v8) )
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
  v4 = regcam_send_req_7(*(u32 *)(v7 + 1064), *(u32 *)(v7 + 24), v7 + 28, *(u16 *)(v10 + 20), (int *)(v10 + 16), v11);
  if ( v4 >= 0 )
    sceNpCommerce2_FC47B63C(v14 + 1072);
LABEL_14:
  if ( v15 != 0 )
  {
    if ( (*(u32 *)(v15 + 12) & 2) != 0 )
      v4 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v12 = v14;
  }
  else
  {
    v12 = v14;
  }
  if ( v12 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v4;
}


int regcam_dispatch_18(int a1, int a2)
{
  return regcam_create_req(a1, 27, a2);
}


int regcam_dispatch_19(int a1, int a2)
{
  return regcam_create_req(a1, 28, a2);
}


int regcam_dispatch_20(int a1, int a2)
{
  return regcam_create_req(a1, 29, a2);
}


int regcam_dispatch_21(int a1, int a2)
{
  return regcam_create_req(a1, 30, a2);
}


s32 regcam_dispatch_22(unsigned int a1)
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
  v2 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v12, 1);
  if ( v2 < 0 )
    goto LABEL_15;
  v2 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v13);
  if ( v2 < 0 )
    goto LABEL_15;
  v4 = v12;
  v3 = sceNpCommerce2_FC47B63C(&v14);
  v5 = v12;
  if ( v3 < 0 )
    goto LABEL_11;
  v6 = *(u64 *)(v4 + 1072);
  if ( (HIDWORD(v14) - HIDWORD(v6)) != ((unsigned int)v14 < (unsigned int)v6) )
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
  v2 = regcam_send_req_11(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
  if ( v2 >= 0 )
    sceNpCommerce2_FC47B63C(v12 + 1072);
LABEL_15:
  if ( v13 != 0 )
  {
    if ( (*(u32 *)(v13 + 12) & 2) != 0 )
      v2 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v10 = v12;
  }
  else
  {
    v10 = v12;
  }
  if ( v10 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v2;
}


int regcam_dispatch_23(int a1, int a2)
{
  return regcam_create_req(a1, 31, a2);
}


int regcam_dispatch_24(unsigned int a1, int a2, int a3, int a4, int a5, int a6)
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
  v12 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v20, 1);
  if ( v12 < 0 )
    goto LABEL_13;
  v12 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v21);
  if ( v12 < 0 )
    goto LABEL_13;
  v14 = v20;
  v13 = sceNpCommerce2_FC47B63C(&v22);
  v15 = v21;
  if ( v13 >= 0 )
  {
    v16 = *(u64 *)(v14 + 1072);
    if ( (HIDWORD(v22) - HIDWORD(v16)) != ((unsigned int)v22 < (unsigned int)v16) )
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
  v12 = regcam_send_req_12(
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
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v18 = v20;
  }
  else
  {
    v18 = v20;
  }
  if ( v18 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v12;
}


int regcam_dispatch_25(int a1, int a2)
{
  return regcam_create_req(a1, 32, a2);
}


int regcam_dispatch_26(unsigned int a1, int a2)
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
  v4 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v14, 1);
  if ( v4 < 0 )
    goto LABEL_14;
  v4 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v15);
  if ( v4 < 0 )
    goto LABEL_14;
  v6 = v14;
  v5 = sceNpCommerce2_FC47B63C(&v16);
  v7 = v14;
  if ( v5 < 0 )
    goto LABEL_10;
  v8 = *(u64 *)(v6 + 1072);
  if ( (HIDWORD(v16) - HIDWORD(v8)) != ((unsigned int)v16 < (unsigned int)v8) )
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
  v4 = regcam_send_req_13(*(u32 *)(v7 + 1064), *(u32 *)(v7 + 24), v7 + 28, *(u16 *)(v10 + 20), (int *)(v10 + 16), v11);
  if ( v4 >= 0 )
    sceNpCommerce2_FC47B63C(v14 + 1072);
LABEL_14:
  if ( v15 != 0 )
  {
    if ( (*(u32 *)(v15 + 12) & 2) != 0 )
      v4 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v12 = v14;
  }
  else
  {
    v12 = v14;
  }
  if ( v12 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v4;
}


int regcam_dispatch_27(int a1, int a2)
{
  return regcam_create_req(a1, 33, a2);
}


int regcam_dispatch_28(unsigned int a1, int a2)
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
  v4 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v14, 1);
  if ( v4 < 0 )
    goto LABEL_14;
  v4 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v15);
  if ( v4 < 0 )
    goto LABEL_14;
  v6 = v14;
  v5 = sceNpCommerce2_FC47B63C(&v16);
  v7 = v14;
  if ( v5 < 0 )
    goto LABEL_10;
  v8 = *(u64 *)(v6 + 1072);
  if ( (HIDWORD(v16) - HIDWORD(v8)) != ((unsigned int)v16 < (unsigned int)v8) )
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
  v4 = regcam_send_req_14(*(u32 *)(v7 + 1064), *(u32 *)(v7 + 24), v7 + 28, *(u16 *)(v10 + 20), (int *)(v10 + 16), v11);
  if ( v4 >= 0 )
    sceNpCommerce2_FC47B63C(v14 + 1072);
LABEL_14:
  if ( v15 != 0 )
  {
    if ( (*(u32 *)(v15 + 12) & 2) != 0 )
      v4 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v12 = v14;
  }
  else
  {
    v12 = v14;
  }
  if ( v12 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v4;
}


int regcam_dispatch_29(int a1, int a2)
{
  return regcam_create_req(a1, 34, a2);
}


int regcam_dispatch_30(int a1, int a2)
{
  return regcam_create_req(a1, 35, a2);
}


int regcam_dispatch_31(unsigned int a1, int a2)
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
  v4 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v14, 1);
  if ( v4 < 0 )
    goto LABEL_14;
  v4 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v15);
  if ( v4 < 0 )
    goto LABEL_14;
  v6 = v14;
  v5 = sceNpCommerce2_FC47B63C(&v16);
  v7 = v14;
  if ( v5 < 0 )
    goto LABEL_10;
  v8 = *(u64 *)(v6 + 1072);
  if ( (HIDWORD(v16) - HIDWORD(v8)) != ((unsigned int)v16 < (unsigned int)v8) )
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
  v4 = regcam_send_req_15(*(u32 *)(v7 + 1064), *(u32 *)(v7 + 24), v7 + 28, *(u16 *)(v10 + 20), (int *)(v10 + 16), v11);
  if ( v4 >= 0 )
    sceNpCommerce2_FC47B63C(v14 + 1072);
LABEL_14:
  if ( v15 != 0 )
  {
    if ( (*(u32 *)(v15 + 12) & 2) != 0 )
      v4 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v12 = v14;
  }
  else
  {
    v12 = v14;
  }
  if ( v12 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v4;
}


int regcam_dispatch_32(int a1, int a2)
{
  return regcam_create_req(a1, 36, a2);
}


s32 regcam_dispatch_33(unsigned int a1)
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
  v2 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v12, 1);
  if ( v2 < 0 )
    goto LABEL_15;
  v2 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v13);
  if ( v2 < 0 )
    goto LABEL_15;
  v4 = v12;
  v3 = sceNpCommerce2_FC47B63C(&v14);
  v5 = v12;
  if ( v3 < 0 )
    goto LABEL_11;
  v6 = *(u64 *)(v4 + 1072);
  if ( (HIDWORD(v14) - HIDWORD(v6)) != ((unsigned int)v14 < (unsigned int)v6) )
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
  v2 = regcam_send_req_16(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
  if ( v2 >= 0 )
    sceNpCommerce2_FC47B63C(v12 + 1072);
LABEL_15:
  if ( v13 != 0 )
  {
    if ( (*(u32 *)(v13 + 12) & 2) != 0 )
      v2 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v10 = v12;
  }
  else
  {
    v10 = v12;
  }
  if ( v10 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v2;
}


int regcam_dispatch_34(int a1, int a2)
{
  return regcam_create_req(a1, 37, a2);
}


int regcam_dispatch_35(unsigned int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8)
{
  int v15;
  int v16;
  int v17;
  int v18;
  s64 v19;
  int v20;
  int v21;
  int v23;
  int v24;
  s64 v25;
  int v26;

  v26 = a4;
  v23 = 0;
  v24 = 0;
  v15 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v23, 1);
  if ( v15 < 0 )
    goto LABEL_14;
  v15 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v24);
  if ( v15 < 0 )
    goto LABEL_14;
  v17 = v23;
  v16 = sceNpCommerce2_FC47B63C(&v25);
  v18 = v24;
  if ( v16 >= 0 )
  {
    v19 = *(u64 *)(v17 + 1072);
    if ( (HIDWORD(v25) - HIDWORD(v19)) == ((unsigned int)v25 < (unsigned int)v19) )
    {
      if ( (u64)(v25 - v19) >> 32 != 0 )
      {
        v18 = v24;
        goto LABEL_12;
      }
      if ( (unsigned int)(v25 - v19) <= 0x98967F )
      {
        v18 = v24;
        goto LABEL_12;
      }
      v20 = *(u32 *)(v17 + 24);
    }
    else
    {
      v20 = *(u32 *)(v17 + 24);
    }
    sceNpCommerce2_75DF99A9(v20, v17 + 28);
    v18 = v24;
  }
LABEL_12:
  v15 = regcam_send_req_17(
          *(u32 *)(v23 + 1064),
          *(u32 *)(v23 + 24),
          v23 + 28,
          *(u16 *)(v18 + 20),
          (int *)(v18 + 16),
          a2,
          a3,
          v26,
          a5,
          a6,
          a7,
          a8);
  if ( v15 >= 0 )
    sceNpCommerce2_FC47B63C(v23 + 1072);
LABEL_14:
  if ( v24 != 0 )
  {
    if ( (*(u32 *)(v24 + 12) & 2) != 0 )
      v15 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v21 = v23;
  }
  else
  {
    v21 = v23;
  }
  if ( v21 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v15;
}


int regcam_dispatch_36(int a1, int a2)
{
  return regcam_create_req(a1, 38, a2);
}


int regcam_dispatch_37(unsigned int a1, int a2)
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
  v4 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v14, 1);
  if ( v4 < 0 )
    goto LABEL_14;
  v4 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v15);
  if ( v4 < 0 )
    goto LABEL_14;
  v6 = v14;
  v5 = sceNpCommerce2_FC47B63C(&v16);
  v7 = v14;
  if ( v5 < 0 )
    goto LABEL_10;
  v8 = *(u64 *)(v6 + 1072);
  if ( (HIDWORD(v16) - HIDWORD(v8)) != ((unsigned int)v16 < (unsigned int)v8) )
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
  v4 = regcam_send_req_18(*(u32 *)(v7 + 1064), *(u32 *)(v7 + 24), v7 + 28, *(u16 *)(v10 + 20), (int *)(v10 + 16), v11);
  if ( v4 >= 0 )
    sceNpCommerce2_FC47B63C(v14 + 1072);
LABEL_14:
  if ( v15 != 0 )
  {
    if ( (*(u32 *)(v15 + 12) & 2) != 0 )
      v4 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v12 = v14;
  }
  else
  {
    v12 = v14;
  }
  if ( v12 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v4;
}


int regcam_dispatch_38(int a1, int a2)
{
  return regcam_create_req(a1, 39, a2);
}


int regcam_dispatch_39(unsigned int a1, int a2, int a3)
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
  v6 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v16, 1);
  if ( v6 < 0 )
    goto LABEL_14;
  v6 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v17);
  if ( v6 < 0 )
    goto LABEL_14;
  v8 = v16;
  v7 = sceNpCommerce2_FC47B63C(&v18);
  v9 = v16;
  if ( v7 < 0 )
    goto LABEL_10;
  v10 = *(u64 *)(v8 + 1072);
  if ( (HIDWORD(v18) - HIDWORD(v10)) != ((unsigned int)v18 < (unsigned int)v10) )
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
  v6 = regcam_send_req_19(*(u32 *)(v9 + 1064), *(u32 *)(v9 + 24), v9 + 28, *(u16 *)(v12 + 20), (int *)(v12 + 16), v13, a3);
  if ( v6 >= 0 )
    sceNpCommerce2_FC47B63C(v16 + 1072);
LABEL_14:
  if ( v17 != 0 )
  {
    if ( (*(u32 *)(v17 + 12) & 2) != 0 )
      v6 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v14 = v16;
  }
  else
  {
    v14 = v16;
  }
  if ( v14 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v6;
}


int regcam_dispatch_40(int a1, int a2)
{
  return regcam_create_req(a1, 40, a2);
}


s32 regcam_dispatch_41(unsigned int a1)
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
  v2 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v12, 1);
  if ( v2 < 0 )
    goto LABEL_15;
  v2 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v13);
  if ( v2 < 0 )
    goto LABEL_15;
  v4 = v12;
  v3 = sceNpCommerce2_FC47B63C(&v14);
  v5 = v12;
  if ( v3 < 0 )
    goto LABEL_11;
  v6 = *(u64 *)(v4 + 1072);
  if ( (HIDWORD(v14) - HIDWORD(v6)) != ((unsigned int)v14 < (unsigned int)v6) )
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
  v2 = regcam_send_req_20(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
  if ( v2 >= 0 )
    sceNpCommerce2_FC47B63C(v12 + 1072);
LABEL_15:
  if ( v13 != 0 )
  {
    if ( (*(u32 *)(v13 + 12) & 2) != 0 )
      v2 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v10 = v12;
  }
  else
  {
    v10 = v12;
  }
  if ( v10 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v2;
}


int regcam_dispatch_42(int a1, int a2)
{
  return regcam_create_req(a1, 41, a2);
}


int regcam_dispatch_43(unsigned int a1, int a2, int a3)
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
  v6 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v16, 1);
  if ( v6 < 0 )
    goto LABEL_14;
  v6 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v17);
  if ( v6 < 0 )
    goto LABEL_14;
  v8 = v16;
  v7 = sceNpCommerce2_FC47B63C(&v18);
  v9 = v16;
  if ( v7 < 0 )
    goto LABEL_10;
  v10 = *(u64 *)(v8 + 1072);
  if ( (HIDWORD(v18) - HIDWORD(v10)) != ((unsigned int)v18 < (unsigned int)v10) )
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
  v6 = regcam_send_req_21(*(u32 *)(v9 + 1064), *(u32 *)(v9 + 24), v9 + 28, *(u16 *)(v12 + 20), (int *)(v12 + 16), v13, a3);
  if ( v6 >= 0 )
    sceNpCommerce2_FC47B63C(v16 + 1072);
LABEL_14:
  if ( v17 != 0 )
  {
    if ( (*(u32 *)(v17 + 12) & 2) != 0 )
      v6 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v14 = v16;
  }
  else
  {
    v14 = v16;
  }
  if ( v14 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v6;
}


int regcam_dispatch_44(int a1, int a2)
{
  return regcam_create_req(a1, 42, a2);
}


int regcam_dispatch_45(unsigned int a1, int a2, int a3)
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
  v6 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v16, 1);
  if ( v6 < 0 )
    goto LABEL_14;
  v6 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v17);
  if ( v6 < 0 )
    goto LABEL_14;
  v8 = v16;
  v7 = sceNpCommerce2_FC47B63C(&v18);
  v9 = v16;
  if ( v7 < 0 )
    goto LABEL_10;
  v10 = *(u64 *)(v8 + 1072);
  if ( (HIDWORD(v18) - HIDWORD(v10)) != ((unsigned int)v18 < (unsigned int)v10) )
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
  v6 = regcam_send_req_22(*(u32 *)(v9 + 1064), *(u32 *)(v9 + 24), v9 + 28, *(u16 *)(v12 + 20), (int *)(v12 + 16), v13, a3);
  if ( v6 >= 0 )
    sceNpCommerce2_FC47B63C(v16 + 1072);
LABEL_14:
  if ( v17 != 0 )
  {
    if ( (*(u32 *)(v17 + 12) & 2) != 0 )
      v6 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v14 = v16;
  }
  else
  {
    v14 = v16;
  }
  if ( v14 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v6;
}


int regcam_dispatch_46(int a1, int a2)
{
  return regcam_create_req(a1, 44, a2);
}


int regcam_dispatch_47(unsigned int a1, int a2, int a3, int a4, int a5, int a6)
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
  v12 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v20, 1);
  if ( v12 < 0 )
    goto LABEL_13;
  v12 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v21);
  if ( v12 < 0 )
    goto LABEL_13;
  v14 = v20;
  v13 = sceNpCommerce2_FC47B63C(&v22);
  v15 = v21;
  if ( v13 >= 0 )
  {
    v16 = *(u64 *)(v14 + 1072);
    if ( (HIDWORD(v22) - HIDWORD(v16)) != ((unsigned int)v22 < (unsigned int)v16) )
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
  v12 = regcam_send_req_23(
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
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v18 = v20;
  }
  else
  {
    v18 = v20;
  }
  if ( v18 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v12;
}


int regcam_dispatch_48(int a1, int a2)
{
  return regcam_create_req(a1, 45, a2);
}


int regcam_dispatch_49(unsigned int a1, int a2, int a3)
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
  v6 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v16, 1);
  if ( v6 < 0 )
    goto LABEL_14;
  v6 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v17);
  if ( v6 < 0 )
    goto LABEL_14;
  v8 = v16;
  v7 = sceNpCommerce2_FC47B63C(&v18);
  v9 = v16;
  if ( v7 < 0 )
    goto LABEL_10;
  v10 = *(u64 *)(v8 + 1072);
  if ( (HIDWORD(v18) - HIDWORD(v10)) != ((unsigned int)v18 < (unsigned int)v10) )
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
  v6 = regcam_send_req_24(*(u32 *)(v9 + 1064), *(u32 *)(v9 + 24), v9 + 28, *(u16 *)(v12 + 20), (int *)(v12 + 16), v13, a3);
  if ( v6 >= 0 )
    sceNpCommerce2_FC47B63C(v16 + 1072);
LABEL_14:
  if ( v17 != 0 )
  {
    if ( (*(u32 *)(v17 + 12) & 2) != 0 )
      v6 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v14 = v16;
  }
  else
  {
    v14 = v16;
  }
  if ( v14 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v6;
}


int regcam_dispatch_50(int a1, int a2)
{
  return regcam_create_req(a1, 46, a2);
}


s32 regcam_dispatch_51(unsigned int a1)
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
  v2 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v12, 1);
  if ( v2 < 0 )
    goto LABEL_15;
  v2 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v13);
  if ( v2 < 0 )
    goto LABEL_15;
  v4 = v12;
  v3 = sceNpCommerce2_FC47B63C(&v14);
  v5 = v12;
  if ( v3 < 0 )
    goto LABEL_11;
  v6 = *(u64 *)(v4 + 1072);
  if ( (HIDWORD(v14) - HIDWORD(v6)) != ((unsigned int)v14 < (unsigned int)v6) )
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
  v2 = regcam_send_req_25(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
  if ( v2 >= 0 )
    sceNpCommerce2_FC47B63C(v12 + 1072);
LABEL_15:
  if ( v13 != 0 )
  {
    if ( (*(u32 *)(v13 + 12) & 2) != 0 )
      v2 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v10 = v12;
  }
  else
  {
    v10 = v12;
  }
  if ( v10 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v2;
}


int regcam_dispatch_52(int a1, int a2)
{
  return regcam_create_req(a1, 47, a2);
}


int regcam_dispatch_53(unsigned int a1, int a2, int a3)
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
  v6 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v16, 1);
  if ( v6 < 0 )
    goto LABEL_14;
  v6 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v17);
  if ( v6 < 0 )
    goto LABEL_14;
  v8 = v16;
  v7 = sceNpCommerce2_FC47B63C(&v18);
  v9 = v16;
  if ( v7 < 0 )
    goto LABEL_10;
  v10 = *(u64 *)(v8 + 1072);
  if ( (HIDWORD(v18) - HIDWORD(v10)) != ((unsigned int)v18 < (unsigned int)v10) )
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
  v6 = regcam_send_req_26(*(u32 *)(v9 + 1064), *(u32 *)(v9 + 24), v9 + 28, *(u16 *)(v12 + 20), (int *)(v12 + 16), v13, a3);
  if ( v6 >= 0 )
    sceNpCommerce2_FC47B63C(v16 + 1072);
LABEL_14:
  if ( v17 != 0 )
  {
    if ( (*(u32 *)(v17 + 12) & 2) != 0 )
      v6 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v14 = v16;
  }
  else
  {
    v14 = v16;
  }
  if ( v14 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v6;
}


int regcam_dispatch_54(int a1, int a2)
{
  return regcam_create_req(a1, 48, a2);
}


s32 regcam_dispatch_55(unsigned int a1)
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
  v2 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v12, 1);
  if ( v2 < 0 )
    goto LABEL_15;
  v2 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v13);
  if ( v2 < 0 )
    goto LABEL_15;
  v4 = v12;
  v3 = sceNpCommerce2_FC47B63C(&v14);
  v5 = v12;
  if ( v3 < 0 )
    goto LABEL_11;
  v6 = *(u64 *)(v4 + 1072);
  if ( (HIDWORD(v14) - HIDWORD(v6)) != ((unsigned int)v14 < (unsigned int)v6) )
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
  v2 = regcam_send_req_27(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
  if ( v2 >= 0 )
    sceNpCommerce2_FC47B63C(v12 + 1072);
LABEL_15:
  if ( v13 != 0 )
  {
    if ( (*(u32 *)(v13 + 12) & 2) != 0 )
      v2 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v10 = v12;
  }
  else
  {
    v10 = v12;
  }
  if ( v10 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v2;
}


int regcam_dispatch_56(int a1, int a2)
{
  return regcam_create_req(a1, 49, a2);
}


int regcam_dispatch_57(int a1, int a2)
{
  return regcam_create_req(a1, 50, a2);
}


int regcam_dispatch_58(int a1, int a2)
{
  return regcam_create_req(a1, 51, a2);
}


int regcam_dispatch_59(int a1, int a2)
{
  return regcam_create_req(a1, 52, a2);
}


int regcam_dispatch_60(unsigned int a1, int a2, int a3, int a4)
{
  int v7;
  int v8;
  int v9;
  int v10;
  s64 v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v17;
  int v18;
  s64 v19;

  v17 = 0;
  v18 = 0;
  v7 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v17, 1);
  if ( v7 < 0 )
    goto LABEL_14;
  v7 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v18);
  if ( v7 < 0 )
    goto LABEL_14;
  v9 = v17;
  v8 = sceNpCommerce2_FC47B63C(&v19);
  v10 = v17;
  if ( v8 < 0 )
    goto LABEL_10;
  v11 = *(u64 *)(v9 + 1072);
  if ( (HIDWORD(v19) - HIDWORD(v11)) != ((unsigned int)v19 < (unsigned int)v11) )
  {
    v12 = *(u32 *)(v9 + 24);
LABEL_9:
    sceNpCommerce2_75DF99A9(v12, v9 + 28);
    v10 = v17;
LABEL_10:
    v13 = v18;
    goto LABEL_11;
  }
  v13 = v18;
  if ( (u64)(v19 - v11) >> 32 == 0 )
  {
    v14 = a3;
    if ( (unsigned int)(v19 - v11) <= 0x98967F )
      goto LABEL_12;
    v12 = *(u32 *)(v9 + 24);
    goto LABEL_9;
  }
LABEL_11:
  v14 = a3;
LABEL_12:
  v7 = regcam_send_req_30(
         *(u32 *)(v10 + 1064),
         *(u32 *)(v10 + 24),
         v10 + 28,
         *(u16 *)(v13 + 20),
         (int *)(v13 + 16),
         v10,
         v14,
         a4);
  if ( v7 >= 0 )
    sceNpCommerce2_FC47B63C(v17 + 1072);
LABEL_14:
  if ( v18 != 0 )
  {
    if ( (*(u32 *)(v18 + 12) & 2) != 0 )
      v7 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v15 = v17;
  }
  else
  {
    v15 = v17;
  }
  if ( v15 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v7;
}


int regcam_dispatch_61(int a1, int a2)
{
  return regcam_create_req(a1, 53, a2);
}


int regcam_dispatch_62(unsigned int a1, int a2, int a3)
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
  v6 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v16, 1);
  if ( v6 < 0 )
    goto LABEL_14;
  v6 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v17);
  if ( v6 < 0 )
    goto LABEL_14;
  v8 = v16;
  v7 = sceNpCommerce2_FC47B63C(&v18);
  v9 = v16;
  if ( v7 < 0 )
    goto LABEL_10;
  v10 = *(u64 *)(v8 + 1072);
  if ( (HIDWORD(v18) - HIDWORD(v10)) != ((unsigned int)v18 < (unsigned int)v10) )
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
  v6 = regcam_send_req_31(*(u32 *)(v9 + 1064), *(u32 *)(v9 + 24), v9 + 28, *(u16 *)(v12 + 20), (int *)(v12 + 16), v13, a3);
  if ( v6 >= 0 )
    sceNpCommerce2_FC47B63C(v16 + 1072);
LABEL_14:
  if ( v17 != 0 )
  {
    if ( (*(u32 *)(v17 + 12) & 2) != 0 )
      v6 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v14 = v16;
  }
  else
  {
    v14 = v16;
  }
  if ( v14 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v6;
}


int regcam_dispatch_63(int a1, int a2)
{
  return regcam_create_req(a1, 54, a2);
}


int regcam_dispatch_64(unsigned int a1, int a2, int a3, int a4)
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
  v8 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v18, 1);
  if ( v8 < 0 )
    goto LABEL_14;
  v8 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v19);
  if ( v8 < 0 )
    goto LABEL_14;
  v10 = v18;
  v9 = sceNpCommerce2_FC47B63C(&v20);
  v11 = v18;
  if ( v9 < 0 )
    goto LABEL_10;
  v12 = *(u64 *)(v10 + 1072);
  if ( (HIDWORD(v20) - HIDWORD(v12)) != ((unsigned int)v20 < (unsigned int)v12) )
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
  v8 = regcam_send_req_32(
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
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v16 = v18;
  }
  else
  {
    v16 = v18;
  }
  if ( v16 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v8;
}


int regcam_dispatch_65(int a1, int a2)
{
  return regcam_create_req(a1, 55, a2);
}


int regcam_dispatch_66(unsigned int a1, int a2, int a3, int a4)
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
  v8 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v18, 1);
  if ( v8 < 0 )
    goto LABEL_14;
  v8 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v19);
  if ( v8 < 0 )
    goto LABEL_14;
  v10 = v18;
  v9 = sceNpCommerce2_FC47B63C(&v20);
  v11 = v18;
  if ( v9 < 0 )
    goto LABEL_10;
  v12 = *(u64 *)(v10 + 1072);
  if ( (HIDWORD(v20) - HIDWORD(v12)) != ((unsigned int)v20 < (unsigned int)v12) )
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
  v8 = regcam_send_req_33(
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
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v16 = v18;
  }
  else
  {
    v16 = v18;
  }
  if ( v16 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v8;
}


int regcam_dispatch_67(int a1, int a2)
{
  return regcam_create_req(a1, 57, a2);
}


int regcam_dispatch_68(unsigned int a1, int a2)
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
  v4 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v14, 1);
  if ( v4 < 0 )
    goto LABEL_14;
  v4 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v15);
  if ( v4 < 0 )
    goto LABEL_14;
  v6 = v14;
  v5 = sceNpCommerce2_FC47B63C(&v16);
  v7 = v14;
  if ( v5 < 0 )
    goto LABEL_10;
  v8 = *(u64 *)(v6 + 1072);
  if ( (HIDWORD(v16) - HIDWORD(v8)) != ((unsigned int)v16 < (unsigned int)v8) )
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
  v4 = regcam_send_req_2(*(u32 *)(v7 + 1064), *(u32 *)(v7 + 24), v7 + 28, *(u16 *)(v10 + 20), (int *)(v10 + 16), v11);
  if ( v4 >= 0 )
    sceNpCommerce2_FC47B63C(v14 + 1072);
LABEL_14:
  if ( v15 != 0 )
  {
    if ( (*(u32 *)(v15 + 12) & 2) != 0 )
      v4 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v12 = v14;
  }
  else
  {
    v12 = v14;
  }
  if ( v12 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v4;
}


int regcam_dispatch_69(int a1, int a2)
{
  return regcam_create_req(a1, 58, a2);
}


int regcam_dispatch_70(unsigned int a1, int a2)
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
  v4 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v14, 1);
  if ( v4 < 0 )
    goto LABEL_14;
  v4 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v15);
  if ( v4 < 0 )
    goto LABEL_14;
  v6 = v14;
  v5 = sceNpCommerce2_FC47B63C(&v16);
  v7 = v14;
  if ( v5 < 0 )
    goto LABEL_10;
  v8 = *(u64 *)(v6 + 1072);
  if ( (HIDWORD(v16) - HIDWORD(v8)) != ((unsigned int)v16 < (unsigned int)v8) )
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
  v4 = regcam_send_req_2(*(u32 *)(v7 + 1064), *(u32 *)(v7 + 24), v7 + 28, *(u16 *)(v10 + 20), (int *)(v10 + 16), v11);
  if ( v4 >= 0 )
    sceNpCommerce2_FC47B63C(v14 + 1072);
LABEL_14:
  if ( v15 != 0 )
  {
    if ( (*(u32 *)(v15 + 12) & 2) != 0 )
      v4 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v12 = v14;
  }
  else
  {
    v12 = v14;
  }
  if ( v12 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v4;
}


int regcam_dispatch_71(int a1, int a2)
{
  return regcam_create_req(a1, 60, a2);
}


int regcam_dispatch_72(int a1, int a2)
{
  return regcam_create_req(a1, 61, a2);
}


int regcam_dispatch_73(int a1, int a2)
{
  return regcam_create_req(a1, 62, a2);
}


int regcam_dispatch_74(unsigned int a1, int a2, int a3, int a4)
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
  v8 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v18, 1);
  if ( v8 < 0 )
    goto LABEL_14;
  v8 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v19);
  if ( v8 < 0 )
    goto LABEL_14;
  v10 = v18;
  v9 = sceNpCommerce2_FC47B63C(&v20);
  v11 = v18;
  if ( v9 < 0 )
    goto LABEL_10;
  v12 = *(u64 *)(v10 + 1072);
  if ( (HIDWORD(v20) - HIDWORD(v12)) != ((unsigned int)v20 < (unsigned int)v12) )
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
  v8 = regcam_send_req_35(
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
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v16 = v18;
  }
  else
  {
    v16 = v18;
  }
  if ( v16 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v8;
}


int regcam_dispatch_75(int a1, int a2)
{
  return regcam_create_req(a1, 63, a2);
}


int regcam_dispatch_76(int a1, int a2)
{
  return regcam_create_req(a1, 64, a2);
}


int regcam_dispatch_77(unsigned int a1, int a2, int a3)
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
  v6 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v16, 1);
  if ( v6 < 0 )
    goto LABEL_14;
  v6 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v17);
  if ( v6 < 0 )
    goto LABEL_14;
  v8 = v16;
  v7 = sceNpCommerce2_FC47B63C(&v18);
  v9 = v16;
  if ( v7 < 0 )
    goto LABEL_10;
  v10 = *(u64 *)(v8 + 1072);
  if ( (HIDWORD(v18) - HIDWORD(v10)) != ((unsigned int)v18 < (unsigned int)v10) )
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
  v6 = regcam_send_req_36(*(u32 *)(v9 + 1064), *(u32 *)(v9 + 24), v9 + 28, *(u16 *)(v12 + 20), (int *)(v12 + 16), v13, a3);
  if ( v6 >= 0 )
    sceNpCommerce2_FC47B63C(v16 + 1072);
LABEL_14:
  if ( v17 != 0 )
  {
    if ( (*(u32 *)(v17 + 12) & 2) != 0 )
      v6 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v14 = v16;
  }
  else
  {
    v14 = v16;
  }
  if ( v14 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v6;
}


int regcam_dispatch_78(int a1, int a2)
{
  return regcam_create_req(a1, 67, a2);
}


int regcam_dispatch_79(int a1, int a2)
{
  return regcam_create_req(a1, 69, a2);
}


s32 regcam_dispatch_80(unsigned int a1)
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
  v2 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v12, 1);
  if ( v2 < 0 )
    goto LABEL_15;
  v2 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v13);
  if ( v2 < 0 )
    goto LABEL_15;
  v4 = v12;
  v3 = sceNpCommerce2_FC47B63C(&v14);
  v5 = v12;
  if ( v3 < 0 )
    goto LABEL_11;
  v6 = *(u64 *)(v4 + 1072);
  if ( (HIDWORD(v14) - HIDWORD(v6)) != ((unsigned int)v14 < (unsigned int)v6) )
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
  v2 = regcam_send_req_37(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
  if ( v2 >= 0 )
  {
    sceNpCommerce2_FC47B63C(v12 + 1072);
    *(u32 *)(v12 + 1060) = 0;
  }
LABEL_15:
  if ( v13 != 0 )
  {
    if ( (*(u32 *)(v13 + 12) & 2) != 0 )
      v2 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v10 = v12;
  }
  else
  {
    v10 = v12;
  }
  if ( v10 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v2;
}


int regcam_dispatch_81(int a1, int a2)
{
  return regcam_create_req(a1, 77, a2);
}


int regcam_dispatch_82(unsigned int a1, int a2)
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
  v4 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v14, 1);
  if ( v4 < 0 )
    goto LABEL_14;
  v4 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v15);
  if ( v4 < 0 )
    goto LABEL_14;
  v6 = v14;
  v5 = sceNpCommerce2_FC47B63C(&v16);
  v7 = v14;
  if ( v5 < 0 )
    goto LABEL_10;
  v8 = *(u64 *)(v6 + 1072);
  if ( (HIDWORD(v16) - HIDWORD(v8)) != ((unsigned int)v16 < (unsigned int)v8) )
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
  v4 = regcam_send_req_38(*(u32 *)(v7 + 1064), *(u32 *)(v7 + 24), v7 + 28, *(u16 *)(v10 + 20), (int *)(v10 + 16), v11);
  if ( v4 >= 0 )
    sceNpCommerce2_FC47B63C(v14 + 1072);
LABEL_14:
  if ( v15 != 0 )
  {
    if ( (*(u32 *)(v15 + 12) & 2) != 0 )
      v4 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v12 = v14;
  }
  else
  {
    v12 = v14;
  }
  if ( v12 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v4;
}


int regcam_dispatch_83(int a1, int a2)
{
  return regcam_create_req(a1, 84, a2);
}


int regcam_dispatch_84(unsigned int a1, int a2)
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
  v4 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, HIWORD(a1), &v14, 1);
  if ( v4 < 0 )
    goto LABEL_14;
  v4 = sceNpCommerce2_851E7844(&g_regcam_mgr, a1, &v15);
  if ( v4 < 0 )
    goto LABEL_14;
  v6 = v14;
  v5 = sceNpCommerce2_FC47B63C(&v16);
  v7 = v14;
  if ( v5 < 0 )
    goto LABEL_10;
  v8 = *(u64 *)(v6 + 1072);
  if ( (HIDWORD(v16) - HIDWORD(v8)) != ((unsigned int)v16 < (unsigned int)v8) )
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
  v4 = regcam_send_req_39(*(u32 *)(v7 + 1064), *(u32 *)(v7 + 24), v7 + 28, *(u16 *)(v10 + 20), (int *)(v10 + 16), v11);
  if ( v4 >= 0 )
    sceNpCommerce2_FC47B63C(v14 + 1072);
LABEL_14:
  if ( v15 != 0 )
  {
    if ( (*(u32 *)(v15 + 12) & 2) != 0 )
      v4 = -2141909232;
    sceNpCommerce2_8642C532(&g_regcam_mgr);
    v12 = v14;
  }
  else
  {
    v12 = v14;
  }
  if ( v12 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v4;
}


int regcam_set_service_type(int a1, int a2)
{
  int v3;
  int v4;
  u32 v6[4];

  v6[0] = 0;
  v3 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, a1, v6, 1);
  v4 = v6[0];
  if ( v3 >= 0 )
    *(u32 *)(v6[0] + 1064) = a2;
  if ( v4 != 0 )
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v3;
}


int regcam_create_req(int a1, s16 a2, int a3)
{
  int v6;
  int v7;
  int v8;
  int v9;
  u32 v11[4];

  v11[0] = 0;
  v6 = sceNpCommerce2_0A6AB496(&g_regcam_mgr, a1, v11, 1);
  if ( v6 < 0
    || (v7 = sceNetIfhandle_lib_15CFE3C0(24), v6 = -2141909237, v8 = 0, v7 != 0)
    && (sceKernelMemset(v7, 0, 24), *(u16 *)(v7 + 20) = a2,
                                    (v6 = sceNpCommerce2_06618C81(&g_regcam_mgr, v7, a1, a3)) >= 0)
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
    sceNpCommerce2_77BE411B(&g_regcam_mgr);
  return v6;
}


int regcam_post_process_response(int a1, int a2, int a3, int a4, unsigned int a5)
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
    v18 = regcam_tlv_parse_9(v21, a3, a5);
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
    v18 = regcam_tlv_parse_22(v20, a3, a5);
LABEL_18:
    if ( v18 >= 0 )
    {
      if ( regcam_tlv_parse_5(v13, v19) >= 0 )
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


