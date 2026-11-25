#include "store_internal.h"

int store_tlv_fn_1(int a1, int a2, unsigned int *a3, int a4, int a5, int a6)
{
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  unsigned int v17;
  u8 v19[784];

  sceNet_lib_CF705E46(v19, (const char *)"%u", a4);
  v11 = store_req_fn_33(a1, a2, 10, (int)v19, 0);
  v12 = v11;
  if ( v11 >= 0 )
  {
    v13 = v11 >> 31;
    v14 = a2;
    v15 = v11;
    if ( a1 == 1 )
      v16 = store_tlv_fn_5(v14, 77, a5, a6, v19, 769, 0, 1);
    else
      v16 = store_tlv_fn_9(77, a5, a6, v19, 769, 0, 1);
    v12 = v16;
    if ( v16 >= 0 )
    {
      v17 = v15 + v16;
      v12 = 0;
      if ( a1 != 1 )
      {
        *a3 = v17;
        a3[1] = v13 + (v16 >> 31) + (v17 < v16);
      }
    }
  }
  return v12;
}


int store_tlv_fn_2(int a1, int a2, u32 *a3, int a4, int a5, int a6, int a7, int a8, int a9)
{
  int v13;
  int v14;
  s64 v15;
  s64 v16;
  int v17;
  int v18;
  int v19;
  s64 v20;
  int v21;
  int v22;
  s64 v23;
  int v24;
  int v25;
  u32 *v26;
  int v28;
  int v29;
  u8 v30[784];
  u8 v31[16];
  u32 *v32;
  int v33;
  int v34;

  v32 = a3;
  v33 = a7;
  v34 = a8;
  sceNet_lib_CF705E46(v30, (const char *)"%u", a4);
  if ( a1 == 1 )
  {
    v28 = sceNpCommerce2_AE614CC6(a2, "storeType=", 10);
    if ( v28 < 0 )
      return v28;
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
    v15 = v13;
    v16 = v13;
    if ( a5 != 0 && a6 != 0 )
    {
      if ( a1 == 1 )
        v17 = store_tlv_fn_5(a2, 77, a5, a6, v30, 769, 0, 1);
      else
        v17 = store_tlv_fn_9(77, a5, a6, v30, 769, 0, 1);
      v14 = v17;
      if ( v17 < 0 )
        return v14;
      v16 = v15 + v17;
    }
    if ( a1 == 1 )
    {
      v19 = store_tlv_fn_8(a2, 7, v33, 1);
    }
    else
    {
      v18 = sceNet_lib_CF705E46(v31, (const char *)"%u", v33);
      v19 = -2141909231;
      if ( v18 < 16 )
        v19 = 12 + v18 + 3;
    }
    v14 = v19;
    if ( v19 >= 0 )
    {
      v20 = v16 + v19;
      if ( a1 == 1 )
      {
        v22 = store_tlv_fn_8(a2, 8, v34, 1);
      }
      else
      {
        v21 = sceNet_lib_CF705E46(v31, (const char *)"%u", v34);
        v22 = -2141909231;
        if ( v21 < 16 )
          v22 = 16 + v21 + 3;
      }
      v14 = v22;
      if ( v22 >= 0 )
      {
        v23 = v20 + v22;
        if ( a1 == 1 )
        {
          v25 = store_tlv_fn_8(a2, 78, a9, 1);
        }
        else
        {
          v24 = sceNet_lib_CF705E46(v31, (const char *)"%u", a9);
          v25 = -2141909231;
          if ( v24 < 16 )
            v25 = 20 + v24;
        }
        v14 = v25;
        if ( v25 >= 0 )
        {
          v23 = ((u64)(v23) & 0xFFFFFFFF00000000ULL) | (u32)(v23 + v25);
          if ( a1 != 1 )
          {
            v26 = v32;
            *v32 = v23;
            v26[1] = HIDWORD(v23) + (v25 >> 31) + ((unsigned int)v23 < v25);
          }
          return 0;
        }
      }
    }
  }
  return v14;
}


int store_tlv_fn_3(int a1, int a2, u64 *a3, int a4, int a5)
{
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v16;
  const char *v17;
  int v18;
  int v19;
  int v20;
  u8 v21[784];

  if ( a4 != 0 )
  {
    v10 = sceNet_lib_D8722983(a4);
    v11 = sceNpCommerce2_66EC0829(v21, 769, a4, v10, 0);
    if ( v11 < 0 )
      return v11;
    if ( a1 == 1 )
    {
      v16 = 10;
      v17 = "contentId=";
LABEL_12:
      v18 = sceNpCommerce2_AE614CC6(a2, v17, v16);
      if ( v18 < 0 )
        return v18;
      v19 = sceNet_lib_D8722983(v21);
      v14 = sceNpCommerce2_AE614CC6(a2, v21, v19);
      goto LABEL_7;
    }
    v12 = sceNet_lib_D8722983(v21);
    v13 = 10;
  }
  else
  {
    v20 = sceNet_lib_D8722983(a5);
    v11 = sceNpCommerce2_66EC0829(v21, 769, a5, v20, 0);
    if ( v11 < 0 )
      return v11;
    if ( a1 == 1 )
    {
      v16 = 7;
      v17 = "fileId=";
      goto LABEL_12;
    }
    v12 = sceNet_lib_D8722983(v21);
    v13 = 7;
  }
  v14 = v13 + v12;
LABEL_7:
  v11 = v14;
  if ( v14 >= 0 )
  {
    if ( a1 != 1 )
      *a3 = v14;
    return 0;
  }
  return v11;
}


int store_tlv_fn_4(int a1, u32 *a2, unsigned int a3, int a4, int a5, int a6, int a7)
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


int store_tlv_fn_5(int a1, int a2, u32 *a3, unsigned int a4, int a5, int a6, int a7, int a8)
{
  int v14;
  unsigned int v15;
  u32 *v16;
  u8 *v17;
  int v19;
  int v20;

  if ( a8 != 1 || (v14 = sceNpCommerce2_AE614CC6(a1, "&", 1)) >= 0 )
  {
    v14 = sceNpCommerce2_AE614CC6(a1, g_form_keys[a2].str, g_form_keys[a2].len);
    if ( v14 >= 0 )
    {
      v15 = 0;
      if ( a4 != 0 )
      {
        v16 = a3;
        do
        {
          if ( v15 != 0 )
          {
            v14 = sceNpCommerce2_AE614CC6(a1, ",", 1);
            if ( v14 < 0 )
              break;
          }
          v17 = (u8 *)(a7 == 1 ? *a3 : *v16);
          ++v15;
          ++v16;
          if ( v17 != NULL && *v17 != 0 )
          {
            v19 = sceNet_lib_D8722983(v17);
            v14 = sceNpCommerce2_66EC0829(a5, a6, v17, v19, 0);
            if ( v14 < 0 )
              break;
            v20 = sceNet_lib_D8722983(a5);
            v14 = sceNpCommerce2_AE614CC6(a1, a5, v20);
            if ( v14 < 0 )
              break;
          }
        }
        while ( v15 < a4 );
      }
    }
  }
  return v14;
}


int store_tlv_fn_6(int a1, int a2, int a3, unsigned int a4, int a5, int a6, int a7)
{
  int v13;
  int result;
  unsigned int v15;
  int v16;
  int v17;

  if ( a7 == 1 )
  {
    result = sceNpCommerce2_AE614CC6(a1, "&", 1);
    if ( result < 0 )
      return result;
    v13 = 2 * a2;
  }
  else
  {
    v13 = 2 * a2;
  }
  result = sceNpCommerce2_AE614CC6(a1, g_form_keys[(v13) >> 1].str, g_form_keys[(v13) >> 1].len);
  if ( result >= 0 )
  {
    v15 = 0;
    if ( a4 != 0 )
    {
      v16 = a3;
      do
      {
        if ( v15 != 0 )
        {
          result = sceNpCommerce2_AE614CC6(a1, ",", 1);
          if ( result < 0 )
            break;
        }
        result = sceNpCommerce2_66EC0829(a5, a6, *(u32 *)(v16 + 4), *(u32 *)(v16 + 8), 0);
        ++v15;
        v16 += 12;
        if ( result < 0 )
          break;
        v17 = sceNet_lib_D8722983(a5);
        result = sceNpCommerce2_AE614CC6(a1, a5, v17);
        if ( result < 0 )
          break;
      }
      while ( v15 < a4 );
    }
  }
  return result;
}


int store_tlv_fn_7(int a1, int a2, u32 *a3, int a4, unsigned int a5, int a6, int a7, int a8)
{
  int v15;
  int result;
  unsigned int v17;
  u32 *v18;
  int v19;
  int v20;

  if ( a8 == 1 )
  {
    result = sceNpCommerce2_AE614CC6(a1, "&", 1);
    if ( result < 0 )
      return result;
    v15 = 2 * a2;
  }
  else
  {
    v15 = 2 * a2;
  }
  result = sceNpCommerce2_AE614CC6(a1, g_form_keys[(v15) >> 1].str, g_form_keys[(v15) >> 1].len);
  if ( result >= 0 )
  {
    v17 = 0;
    if ( a5 != 0 )
    {
      v18 = a3;
      do
      {
        if ( v17 != 0 )
        {
          result = sceNpCommerce2_AE614CC6(a1, ",", 1);
          if ( result < 0 )
            break;
        }
        ++v17;
        v19 = sceNet_lib_D8722983(*(u32 *)(4 * *v18 + a4));
        result = sceNpCommerce2_66EC0829(a6, a7, *(u32 *)(4 * *v18 + a4), v19, 0);
        v18 += 3;
        if ( result < 0 )
          break;
        v20 = sceNet_lib_D8722983(a6);
        result = sceNpCommerce2_AE614CC6(a1, a6, v20);
        if ( result < 0 )
          break;
      }
      while ( v17 < a5 );
    }
  }
  return result;
}


int store_tlv_fn_8(int a1, int a2, int a3, int a4)
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
      v8 = sceNet_lib_CF705E46(v11, (const char *)"%u", a3);
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


int store_tlv_fn_9(int a1, u8 **a2, unsigned int a3, int a4, int a5, int a6, int a7)
{
  unsigned int v11;
  int v12;
  u8 **v13;
  int i;
  u8 *v15;
  int result;
  int v17;

  v11 = 0;
  v12 = (a7 == 1) + g_form_keys[a1].len;
  if ( a3 != 0 )
  {
    v13 = a2;
    for ( i = 0; ; i = v11 != 0 )
    {
      v12 += i;
      if ( a6 == 1 )
        v15 = *a2;
      else
        v15 = *v13;
      ++v11;
      ++v13;
      if ( v15 != NULL && *v15 != 0 )
      {
        v17 = sceNet_lib_D8722983(v15);
        result = sceNpCommerce2_66EC0829(a4, a5, v15, v17, 0);
        if ( result < 0 )
          return result;
        v12 += sceNet_lib_D8722983(a4);
      }
      if ( v11 >= a3 )
        return v12;
    }
  }
  return v12;
}


int store_tlv_fn_10(int *a1, int *a2, int a3)
{
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  u16 v18;
  int v19;
  int v20;
  int *v21;
  int *v22;
  int *v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int *v29;
  int v30;
  int v31;
  int v32;
  u16 v34;
  s16 v35;
  s16 v36;
  s16 v37;
  s16 v38;
  s16 v39;
  s16 v40;

  v6 = a1[5];
  if ( a3 == 3 )
    v7 = 139;
  else
    v7 = 38;
  sceKernelMemset(a2, 0, 104);
  v8 = sceNpCommerce2_E3455E27(v6, a1[4] - (v6 - a1[3]), v7, 101, &v34);
  v9 = v8;
  if ( v8 >= 0 )
  {
    v10 = a1[1];
    v11 = v6 + v8;
    v12 = *a1;
    v13 = v34;
    sceKernelMemset(a2, 0, 40);
    a2[1] = v10;
    *a2 = v12;
    a2[4] = v13;
    a2[3] = v11;
    v14 = v11;
    v15 = store_disp_fn_60(a1[1], v11, a2 + 10, a2 + 11, a2 + 12, a2 + 14, a2 + 15, a2 + 16, a2 + 17, a2 + 18);
    v9 = v15;
    if ( v15 >= 0 )
    {
      v16 = v11 + v15;
      v17 = v16 - v14;
      v18 = v34 - (v16 - v14);
      a2[19] = a2[1] + 4;
      v19 = v18;
      a2[5] = v16;
      v20 = sceNpCommerce2_E3455E27(v16, v18, 7, 0, &v35);
      v21 = a2 + 20;
      v22 = a2 + 21;
      v23 = a2 + 22;
      if ( v20 >= 0 && v35 != 0 )
        a2[19] = v16 + v20;
      if ( v20 >= 0 )
      {
        v24 = 0;
        if ( a2 != NULL )
          v24 = a2[1] + 4;
        *v21 = v24;
        v20 = sceNpCommerce2_E3455E27(v16, v19, 12, 0, &v36);
        if ( v20 >= 0 && v36 != 0 )
          *v21 = v16 + v20;
        if ( v20 >= 0 )
        {
          v25 = 0;
          if ( a2 != NULL )
            v25 = a2[1] + 4;
          *v22 = v25;
          v20 = sceNpCommerce2_E3455E27(v16, v19, 16, 0, &v37);
          if ( v20 >= 0 && v37 != 0 )
            *v22 = v16 + v20;
          if ( v20 >= 0 )
          {
            v26 = 0;
            if ( a2 != NULL )
              v26 = a2[1] + 4;
            *v23 = v26;
            v20 = sceNpCommerce2_E3455E27(v16, v19, 4, 0, &v38);
            if ( v20 >= 0 && v38 != 0 )
              *v23 = v16 + v20;
          }
        }
      }
      v9 = v20;
      if ( v20 >= 0 )
      {
        v27 = v34;
        if ( a3 == 3 )
        {
          v9 = sceNpCommerce2_3F6D413C(v16, v34 - v17, 17, 0, a2 + 25);
          if ( ((v9 != 0x80550F87) & ((unsigned int)v9 >> 31)) != 0 )
            return v9;
          goto LABEL_32;
        }
        a2[25] = a2[1] + 4;
        v28 = sceNpCommerce2_E3455E27(v16, v27 - v17, 17, 0, &v39);
        if ( v28 >= 0 && v39 != 0 )
          a2[25] = v16 + v28;
        v9 = v28;
        if ( v28 >= 0 )
        {
LABEL_32:
          v9 = sceNpCommerce2_3F6D413C(v16, v34 - v17, 24, 0, a2 + 23);
          if ( v9 >= 0 )
          {
            v29 = a2 + 24;
            v30 = 0;
            v31 = v34 - v17;
            if ( a2 != NULL )
              v30 = a2[1] + 4;
            *v29 = v30;
            v32 = sceNpCommerce2_E3455E27(v16, v31, 68, 0, &v40);
            if ( v32 >= 0 && v40 != 0 )
              *v29 = v16 + v32;
            v9 = v32;
            if ( ((v32 != 0x80550F87) & ((unsigned int)v32 >> 31)) == 0 )
              return 0;
          }
        }
      }
    }
  }
  return v9;
}


unsigned int store_tlv_fn_11(int *a1, u32 *a2, int a3)
{
  int v5;
  int v6;
  int v7;
  unsigned int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  u32 v20[20];
  u16 v21;
  s16 v22;
  int v23;

  v5 = a1[5];
  if ( a3 == 3 )
    v6 = 135;
  else
    v6 = 30;
  sceKernelMemset(a2, 0, 144);
  v7 = sceNpCommerce2_E3455E27(v5, a1[4] - (v5 - a1[3]), v6, 97, &v21);
  v8 = v7;
  if ( v7 >= 0 )
  {
    v9 = a1[1];
    v10 = *a1;
    v11 = v21;
    v12 = v5 + v7;
    sceKernelMemset(a2, 0, 40);
    a2[1] = v9;
    *a2 = v10;
    a2[4] = v11;
    a2[3] = v12;
    v23 = v12;
    sceKernelMemset(v20, 0, 80);
    sceKernelMemset(v20, 0, 60);
    v20[0] = a1[1];
    v20[1] = 7;
    v20[2] = a2 + 35;
    v20[3] = a2 + 10;
    v20[4] = a2 + 11;
    v20[5] = a2 + 12;
    v20[6] = a2 + 13;
    v20[7] = a2 + 14;
    v20[8] = a2 + 15;
    v20[9] = a2 + 16;
    v20[10] = a2 + 17;
    v20[13] = a2 + 20;
    v20[14] = a2 + 21;
    v20[11] = a2 + 18;
    v20[12] = a2 + 19;
    v20[15] = a2 + 22;
    v20[16] = a2 + 24;
    v20[17] = a2 + 26;
    v20[18] = a2 + 28;
    v20[19] = a2 + 30;
    v13 = store_disp_fn_64(v12, v20);
    v8 = v13;
    if ( v13 >= 0 )
    {
      v14 = v23;
      v15 = v12 + v13;
      v16 = v21;
      a2[32] = a2[1] + 4;
      v17 = v15 - v14;
      a2[5] = v15;
      v18 = sceNpCommerce2_E3455E27(v15, v16 - (v15 - v14), 8, 0, &v22);
      if ( v18 >= 0 && v22 != 0 )
        a2[32] = v15 + v18;
      v8 = v18;
      if ( v18 >= 0 )
      {
        v8 = sceNpCommerce2_3F6D413C(v15, v21 - v17, 128, 0, a2 + 33);
        if ( ((v8 != 0x80550F87) & (v8 >> 31)) == 0 )
        {
          v8 = sceNpCommerce2_3F6D413C(v15, v21 - v17, 149, 0, a2 + 34);
          if ( ((v8 != 0x80550F87) & (v8 >> 31)) == 0 )
            return 0;
        }
      }
    }
  }
  return v8;
}


int store_tlv_fn_12(int *a1, int *a2, int a3)
{
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  u16 v18;
  int v19;
  int v20;
  int *v21;
  int *v22;
  int *v23;
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
  int *v35;
  int v36;
  int v37;
  int v38;
  u16 v40;
  s16 v41;
  s16 v42;
  s16 v43;
  s16 v44;
  s16 v45;
  s16 v46;
  s16 v47;
  s16 v48;
  s16 v49[7];

  v6 = a1[5];
  if ( a3 == 3 )
    v7 = 140;
  else
    v7 = 39;
  sceKernelMemset(a2, 0, 136);
  v8 = sceNpCommerce2_E3455E27(v6, a1[4] - (v6 - a1[3]), v7, 137, &v40);
  v9 = v8;
  if ( v8 >= 0 )
  {
    v10 = a1[1];
    v11 = v6 + v8;
    v12 = *a1;
    v13 = v40;
    sceKernelMemset(a2, 0, 40);
    a2[1] = v10;
    *a2 = v12;
    a2[4] = v13;
    a2[3] = v11;
    v14 = v11;
    v15 = store_disp_fn_60(a1[1], v11, a2 + 10, a2 + 12, a2 + 14, a2 + 16, a2 + 11, a2 + 17, a2 + 18, a2 + 19);
    v9 = v15;
    if ( v15 >= 0 )
    {
      v16 = v11 + v15;
      v17 = v16 - v14;
      v18 = v40 - (v16 - v14);
      a2[20] = a2[1] + 4;
      v19 = v18;
      a2[5] = v16;
      v20 = sceNpCommerce2_E3455E27(v16, v18, 7, 0, &v41);
      v21 = a2 + 21;
      v22 = a2 + 22;
      v23 = a2 + 23;
      if ( v20 >= 0 && v41 != 0 )
        a2[20] = v16 + v20;
      if ( v20 >= 0 )
      {
        v24 = 0;
        if ( a2 != NULL )
          v24 = a2[1] + 4;
        *v21 = v24;
        v20 = sceNpCommerce2_E3455E27(v16, v19, 12, 0, &v42);
        if ( v20 >= 0 && v42 != 0 )
          *v21 = v16 + v20;
        if ( v20 >= 0 )
        {
          v25 = 0;
          if ( a2 != NULL )
            v25 = a2[1] + 4;
          *v22 = v25;
          v20 = sceNpCommerce2_E3455E27(v16, v19, 16, 0, &v43);
          if ( v20 >= 0 && v43 != 0 )
            *v22 = v16 + v20;
          if ( v20 >= 0 )
          {
            v26 = 0;
            if ( a2 != NULL )
              v26 = a2[1] + 4;
            *v23 = v26;
            v20 = sceNpCommerce2_E3455E27(v16, v19, 4, 0, &v44);
            if ( v20 >= 0 && v44 != 0 )
              *v23 = v16 + v20;
          }
        }
      }
      v9 = v20;
      if ( v20 >= 0 )
      {
        v27 = v40;
        if ( a3 == 3 )
        {
          v9 = sceNpCommerce2_3F6D413C(v16, v40 - v17, 17, 0, a2 + 32);
          if ( ((v9 != 0x80550F87) & ((unsigned int)v9 >> 31)) != 0 )
            return v9;
          goto LABEL_32;
        }
        a2[32] = a2[1] + 4;
        v28 = sceNpCommerce2_E3455E27(v16, v27 - v17, 17, 0, &v45);
        if ( v28 >= 0 && v45 != 0 )
          a2[32] = v16 + v28;
        v9 = v28;
        if ( v28 >= 0 )
        {
LABEL_32:
          v9 = sceNpCommerce2_3F6D413C(v16, v40 - v17, 24, 0, a2 + 24);
          if ( v9 >= 0 )
          {
            v29 = v40;
            a2[25] = a2[1] + 4;
            v30 = sceNpCommerce2_E3455E27(v16, v29 - v17, 13, 0, &v46);
            if ( v30 >= 0 && v46 != 0 )
              a2[25] = v16 + v30;
            v9 = v30;
            if ( v30 >= 0 )
            {
              v9 = sceNpCommerce2_3F6D413C(v16, v40 - v17, 35, 0, a2 + 26);
              if ( v9 >= 0 )
              {
                v9 = sceNpCommerce2_3F6D413C(v16, v40 - v17, 36, 0, a2 + 27);
                if ( v9 >= 0 )
                {
                  v9 = sceNpCommerce2_3F6D413C(v16, v40 - v17, 37, 0, a2 + 28);
                  if ( v9 >= 0 )
                  {
                    v31 = v40;
                    a2[29] = a2[1] + 4;
                    v32 = sceNpCommerce2_E3455E27(v16, v31 - v17, 68, 0, &v47);
                    if ( v32 >= 0 && v47 != 0 )
                      a2[29] = v16 + v32;
                    v9 = v32;
                    if ( ((v32 != 0x80550F87) & ((unsigned int)v32 >> 31)) == 0 )
                    {
                      v33 = v40;
                      a2[30] = a2[1] + 4;
                      v34 = sceNpCommerce2_E3455E27(v16, v33 - v17, 69, 0, &v48);
                      if ( v34 >= 0 && v48 != 0 )
                        a2[30] = v16 + v34;
                      v9 = v34;
                      if ( ((v34 != 0x80550F87) & ((unsigned int)v34 >> 31)) == 0 )
                      {
                        v35 = a2 + 31;
                        v36 = 0;
                        v37 = v40 - v17;
                        if ( a2 != NULL )
                          v36 = a2[1] + 4;
                        *v35 = v36;
                        v38 = sceNpCommerce2_E3455E27(v16, v37, 119, 0, v49);
                        if ( v38 >= 0 && v49[0] != 0 )
                          *v35 = v16 + v38;
                        v9 = v38;
                        if ( ((v38 != 0x80550F87) & ((unsigned int)v38 >> 31)) == 0 )
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
    }
  }
  return v9;
}


int store_tlv_fn_13(int *a1, int a2, u32 *a3, int a4)
{
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  u16 v17;
  int v18;
  int v19;
  int *v20;
  int *v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  int v32;
  u32 v33[20];
  u16 v34;
  s16 v35;
  s16 v36;
  s16 v37;
  s16 v38;
  int v39;
  int v40;

  v39 = a1[5];
  if ( a4 == 3 )
    v7 = 137;
  else
    v7 = 31;
  sceKernelMemset(a3, 0, 168);
  v8 = store_disp_fn_55(v39, a1[4] - (v39 - a1[3]), v7, 101, a2, &v34);
  v9 = v8;
  if ( v8 >= 0 )
  {
    v39 += v8;
    v10 = a1[1];
    v11 = *a1;
    v12 = v34;
    sceKernelMemset(a3, 0, 40);
    v13 = v39;
    a3[1] = v10;
    a3[3] = v13;
    *a3 = v11;
    a3[4] = v12;
    v40 = v13;
    sceKernelMemset(v33, 0, 80);
    sceKernelMemset(v33, 0, 60);
    v33[0] = a1[1];
    v33[1] = 7;
    v33[2] = a3 + 41;
    v33[3] = a3 + 10;
    v33[4] = a3 + 11;
    v33[5] = a3 + 12;
    v33[6] = a3 + 13;
    v33[7] = a3 + 14;
    v33[8] = a3 + 15;
    v33[9] = a3 + 16;
    v33[10] = a3 + 17;
    v33[13] = a3 + 20;
    v33[14] = a3 + 21;
    v33[11] = a3 + 18;
    v33[12] = a3 + 19;
    v33[15] = a3 + 22;
    v33[16] = a3 + 24;
    v33[17] = a3 + 26;
    v33[18] = a3 + 28;
    v33[19] = a3 + 30;
    v14 = store_disp_fn_64(v39, v33);
    v9 = v14;
    if ( v14 >= 0 )
    {
      v15 = v39 + v14;
      v16 = v39 + v14 - v40;
      v17 = v34 - v16;
      a3[32] = a3[1] + 4;
      v18 = v17;
      a3[5] = v15;
      v19 = v15;
      v39 = v15;
      v20 = a3 + 33;
      v21 = a3 + 34;
      v23 = a3[11];
      v22 = sceNpCommerce2_E3455E27(v19, v18, 8, 0, &v35);
      if ( v22 >= 0 && v35 != 0 )
        a3[32] = v39 + v22;
      v9 = v22;
      if ( v22 >= 0 )
      {
        v24 = 0;
        if ( a3 != NULL )
          v24 = a3[1] + 4;
        *v20 = v24;
        v25 = sceNpCommerce2_E3455E27(v39, v18, 56, 0, &v36);
        v26 = v25;
        if ( v25 >= 0 && v36 != 0 )
          *v20 = v39 + v25;
        v9 = v25;
        if ( v25 >= 0 )
        {
          v27 = 0;
          if ( a3 != NULL )
            v27 = a3[1] + 4;
          *v21 = v27;
          if ( v23 == 2 )
          {
            v32 = 0;
            if ( a3 != NULL )
              v32 = a3[1] + 4;
            *v21 = v32;
            v26 = sceNpCommerce2_E3455E27(v39, v18, 18, 0, &v37);
            if ( v26 >= 0 && v37 != 0 )
              *v21 = v39 + v26;
          }
          v9 = v26;
          if ( v26 >= 0 )
          {
            v28 = v34;
            v29 = v39;
            a3[35] = a3[1] + 4;
            v30 = sceNpCommerce2_E3455E27(v29, v28 - v16, 60, 0, &v38);
            if ( v30 >= 0 && v38 != 0 )
              a3[35] = v39 + v30;
            v9 = v30;
            if ( ((v30 != 0x80550F87) & ((unsigned int)v30 >> 31)) == 0 )
            {
              v9 = sceNpCommerce2_3F6D413C(v39, v34 - v16, 61, 0, a3 + 36);
              if ( v9 >= 0 )
              {
                v9 = sceNpCommerce2_3F6D413C(v39, v34 - v16, 62, 0, a3 + 37);
                if ( v9 >= 0 )
                {
                  v9 = sceNpCommerce2_3F6D413C(v39, v34 - v16, 128, 0, a3 + 38);
                  if ( ((v9 != 0x80550F87) & ((unsigned int)v9 >> 31)) == 0 )
                  {
                    v9 = sceNpCommerce2_3F6D413C(v39, v34 - v16, 133, 0, a3 + 39);
                    if ( ((v9 != 0x80550F87) & ((unsigned int)v9 >> 31)) == 0 )
                    {
                      v9 = sceNpCommerce2_3F6D413C(v39, v34 - v16, 149, 0, a3 + 40);
                      if ( ((v9 != 0x80550F87) & ((unsigned int)v9 >> 31)) == 0 )
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
  }
  return v9;
}


int store_tlv_fn_14(int *a1, int a2, u32 *a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int *v20;
  int v21;
  int v22;
  int v23;
  u16 v25;
  s16 v26;
  s16 v27;
  s16 v28;
  u32 v29;

  v5 = a1[5];
  v6 = store_disp_fn_55(v5, a1[4] - (v5 - a1[3]), 37, 0, a2, &v25);
  v7 = v6;
  if ( v6 >= 0 )
  {
    v8 = v5 + v6;
    sceKernelMemset(a3, 0, 56);
    v9 = *a1;
    v10 = v25;
    v11 = a1[1];
    sceKernelMemset(a3, 0, 40);
    a3[1] = v11;
    v12 = v8;
    a3[3] = v8;
    v13 = v8 + 4;
    *a3 = v9;
    v14 = v13 - v12;
    a3[4] = v10;
    sceKernelMemcpy(&v29, v12, 4);
    v15 = a3[1];
    v16 = v25;
    a3[10] = byteswap32(v29);
    a3[11] = v15 + 4;
    a3[5] = v13;
    v17 = sceNpCommerce2_E3455E27(v13, v16 - v14, 63, 0, &v26);
    if ( v17 >= 0 && v26 != 0 )
      a3[11] = v13 + v17;
    v7 = v17;
    if ( v17 >= 0 )
    {
      v18 = v25;
      a3[12] = a3[1] + 4;
      v19 = sceNpCommerce2_E3455E27(v13, v18 - v14, 64, 0, &v27);
      if ( v19 >= 0 && v27 != 0 )
        a3[12] = v13 + v19;
      v7 = v19;
      v20 = a3 + 13;
      if ( ((v19 != 0x80550F87) & ((unsigned int)v19 >> 31)) == 0 )
      {
        v21 = 0;
        v22 = v25 - v14;
        if ( a3 != NULL )
          v21 = a3[1] + 4;
        *v20 = v21;
        v23 = sceNpCommerce2_E3455E27(v13, v22, 65, 0, &v28);
        if ( v23 >= 0 && v28 != 0 )
          *v20 = v13 + v23;
        v7 = v23;
        if ( ((v23 != 0x80550F87) & ((unsigned int)v23 >> 31)) == 0 )
          return 0;
      }
    }
  }
  return v7;
}


int store_tlv_fn_15(int *a1, int a2, u32 *a3)
{
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  u32 v14;
  u32 v15;
  int v16;
  u32 v17;
  u64 v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int *v25;
  int v26;
  int v27;
  int v28;
  int v30;
  u16 v31;
  s16 v32;
  s16 v33;
  u32 v34;
  u32 v35;
  u32 v36;
  u32 v37;
  u32 v38;
  s64 v39;
  u32 v40;

  v6 = a1[5];
  sceKernelMemcpy(&v30, a1[1], 56);
  v7 = store_disp_fn_55(v6, a1[4] - (v6 - a1[3]), 128, 0, a2, &v31);
  v8 = v7;
  if ( v7 >= 0 )
  {
    v9 = v6 + v7;
    sceKernelMemset(a3, 0, 104);
    v10 = *a1;
    v11 = a1[1];
    v12 = v31;
    sceKernelMemset(a3, 0, 40);
    a3[1] = v11;
    *a3 = v10;
    v13 = v9;
    a3[4] = v12;
    a3[3] = v9;
    a3[10] = v9;
    sceKernelMemcpy(&v34, v9 + 49, 4);
    v14 = byteswap32(v34);
    a3[12] = v9 + 53;
    a3[11] = v14;
    a3[13] = v9 + 110;
    sceKernelMemcpy(&v35, v9 + 159, 4);
    a3[14] = byteswap32(v35);
    sceKernelMemcpy(&v36, v9 + 163, 4);
    a3[15] = byteswap32(v36);
    sceKernelMemcpy(&v37, v9 + 167, 4);
    v15 = v37;
    a3[17] = v9 + 171;
    a3[16] = byteswap32(v15);
    sceKernelMemcpy(&v38, v9 + 228, 4);
    v16 = v9 + 232;
    a3[18] = byteswap32(v38);
    sceKernelMemcpy(&v39, v16, 8);
    v17 = byteswap32(v39);
    v18 = __PAIR64__(v17, byteswap32(HIDWORD(v39)));
    v39 = __PAIR64__(v17, v18);
    if ( ((unsigned int)v18 | v17) != 0 )
    {
      a3[20] = 1000 * v18 - 13910016;
      a3[21] = ((1000 * v18) >> 32) + 14467070 + ((unsigned int)(1000 * v18 - 13910016) < 0xFF2BC000);
      v39 = 1000 * v18;
    }
    else
    {
      sceKernelMemset(a3 + 20, 0, 8);
    }
    v19 = v16 + 8;
    sceKernelMemcpy(&v40, v16 + 8, 4);
    v20 = v31;
    v21 = a3[1] + 4;
    v22 = v19 + 8;
    a3[22] = byteswap32(v40);
    v23 = v19 + 8 - v13;
    a3[23] = v21;
    a3[5] = v19 + 8;
    v24 = sceNpCommerce2_E3455E27(v19 + 8, v20 - v23, 8, 0, &v32);
    if ( v24 >= 0 && v32 != 0 )
      a3[23] = v22 + v24;
    v8 = v24;
    if ( v24 >= 0 )
    {
      v25 = a3 + 24;
      v26 = 0;
      v27 = v31 - v23;
      if ( a3 != NULL )
        v26 = a3[1] + 4;
      *v25 = v26;
      v28 = sceNpCommerce2_E3455E27(v19 + 8, v27, 132, 0, &v33);
      if ( v28 >= 0 && v33 != 0 )
        *v25 = v22 + v28;
      v8 = v28;
      if ( ((v28 != 0x80550F87) & ((unsigned int)v28 >> 31)) == 0 )
        return 0;
    }
  }
  return v8;
}


unsigned int store_tlv_fn_16(int *a1, int a2, u32 *a3)
{
  int v5;
  int v6;
  unsigned int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  u32 v13;
  u32 v14;
  int v15;
  u64 v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  u32 v24;
  int v25;
  int v26;
  int v27;
  u16 v29;
  s16 v30;
  u32 v31;
  u32 v32;
  u32 v33;
  u32 v34;
  u32 v35;
  s64 v36;
  u32 v37;
  u32 v38;
  u32 v39;
  u32 v40[2];
  u32 v41;
  u32 v42;

  v5 = a1[5];
  v6 = store_disp_fn_55(v5, a1[4] - (v5 - a1[3]), 149, 0, a2, &v29);
  v7 = v6;
  if ( v6 >= 0 )
  {
    v8 = v5 + v6;
    sceKernelMemset(a3, 0, 120);
    v9 = *a1;
    v10 = v29;
    v11 = a1[1];
    sceKernelMemset(a3, 0, 40);
    a3[1] = v11;
    *a3 = v9;
    v12 = v8;
    a3[4] = v10;
    a3[3] = v8;
    a3[10] = v8;
    sceKernelMemcpy(&v31, v8 + 49, 4);
    v13 = v31;
    a3[12] = v8 + 110;
    a3[11] = byteswap32(v13);
    sceKernelMemcpy(&v32, v8 + 159, 4);
    a3[13] = byteswap32(v32);
    sceKernelMemcpy(&v33, v8 + 163, 4);
    a3[14] = byteswap32(v33);
    sceKernelMemcpy(&v34, v8 + 167, 4);
    v14 = v34;
    a3[16] = v8 + 171;
    a3[15] = byteswap32(v14);
    sceKernelMemcpy(&v35, v8 + 228, 4);
    v15 = v8 + 232;
    a3[17] = byteswap32(v35);
    sceKernelMemcpy(&v36, v15, 8);
    v16 = __PAIR64__(byteswap32(v36), byteswap32(HIDWORD(v36)));
    v36 = v16;
    if ( v16 != 0 )
    {
      a3[18] = 1000 * v16 - 13910016;
      a3[19] = ((1000 * v16) >> 32) + 14467070 + ((unsigned int)(1000 * v16 - 13910016) < 0xFF2BC000);
      v36 = 1000 * v16;
    }
    else
    {
      sceKernelMemset(a3 + 18, 0, 8);
    }
    v17 = v15 + 8;
    sceKernelMemcpy(&v37, v15 + 8, 4);
    v18 = v15 + 12;
    a3[20] = byteswap32(v37);
    sceKernelMemcpy(&v38, v15 + 12, 4);
    v19 = v15 + 16;
    v20 = v29;
    a3[21] = byteswap32(v38);
    v21 = v17 + 8 - v12;
    a3[5] = v18 + 4;
    v22 = sceNpCommerce2_E3455E27(v18 + 4, v20 - v21, 131, 20, 0);
    v7 = v22;
    if ( v22 >= 0 )
    {
      v23 = v19 + v22;
      sceKernelMemcpy(&v39, v19 + v22, 4);
      a3[24] = byteswap32(v39);
      sceKernelMemcpy(v40, v23 + 4, 8);
      v24 = byteswap32(v40[0]);
      a3[26] = byteswap32(v40[1]);
      a3[27] = v24;
      sceKernelMemcpy(&v41, v23 + 12, 4);
      a3[28] = byteswap32(v41);
      sceKernelMemcpy(&v42, v23 + 16, 4);
      v25 = v29;
      v26 = a3[1] + 4;
      a3[29] = byteswap32(v42);
      a3[22] = v26;
      v27 = sceNpCommerce2_E3455E27(v19, v25 - v21, 151, 0, &v30);
      if ( v27 >= 0 && v30 != 0 )
        a3[22] = v19 + v27;
      v7 = v27;
      if ( ((v27 != 0x80550F87) & ((unsigned int)v27 >> 31)) == 0 )
      {
        v7 = sceNpCommerce2_3F6D413C(v19, v29 - v21, 150, 0, a3 + 23);
        if ( ((v7 != 0x80550F87) & (v7 >> 31)) == 0 )
          return 0;
      }
    }
  }
  return v7;
}


int store_tlv_fn_17(int *a1, int a2, u32 *a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11;
  u32 v12;
  u32 v13;
  u16 v15;
  u32 v16;
  u32 v17;
  u32 v18;
  u32 v19[2];
  u32 v20[2];

  v5 = a1[5];
  v6 = store_disp_fn_55(v5, a1[4] - (v5 - a1[3]), 156, 0, a2, &v15);
  if ( v6 >= 0 )
  {
    sceKernelMemset(a3, 0, 72);
    v7 = *a1;
    v8 = a1[1];
    v9 = v15;
    v10 = v5 + v6;
    sceKernelMemset(a3, 0, 40);
    a3[1] = v8;
    *a3 = v7;
    a3[4] = v9;
    a3[3] = v10;
    a3[10] = v10;
    sceKernelMemcpy(&v16, v10 + 49, 4);
    a3[11] = byteswap32(v16);
    sceKernelMemcpy(&v17, v10 + 53, 4);
    a3[12] = byteswap32(v17);
    sceKernelMemcpy(&v18, v10 + 57, 4);
    a3[13] = byteswap32(v18);
    sceKernelMemcpy(v19, v10 + 61, 8);
    v11 = byteswap32(v19[0]);
    a3[14] = byteswap32(v19[1]);
    a3[15] = v11;
    sceKernelMemcpy(v20, v10 + 69, 8);
    v12 = byteswap32(v20[1]);
    v13 = byteswap32(v20[0]);
    a3[5] = v10 + 77;
    a3[16] = v12;
    a3[17] = v13;
  }
  return v6;
}


s32 store_tlv_fn_18(int *a1, int a2, u32 *a3, int a4)
{
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  u32 v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  u16 v27;
  s16 v28;
  s16 v29;
  u32 v30;
  u32 v31;
  u32 v32;
  u32 v33;
  u32 v34;
  u32 v35[4];

  v8 = a1[5];
  if ( a4 == 3 )
    v9 = 144;
  else
    v9 = 133;
  sceKernelMemset(a3, 0, 88);
  v10 = store_disp_fn_55(v8, a1[4] - (v8 - a1[3]), v9, 0, a2, &v27);
  v11 = v10;
  if ( v10 < 0 )
    return v11;
  v12 = *a1;
  v13 = v27;
  v14 = a1[1];
  v15 = v8 + v10;
  sceKernelMemset(a3, 0, 40);
  a3[1] = v14;
  *a3 = v12;
  v16 = v15;
  a3[4] = v13;
  a3[3] = v15;
  a3[10] = v15;
  sceKernelMemcpy(&v30, v15 + 49, 4);
  a3[11] = byteswap32(v30);
  v17 = v15 + 61;
  sceKernelMemcpy(&v31, v15 + 53, 4);
  a3[12] = byteswap32(v31);
  sceKernelMemcpy(&v32, v15 + 57, 4);
  a3[13] = byteswap32(v32);
  sceKernelMemcpy(&v33, v15 + 61, 4);
  v18 = v15 + 65;
  a3[14] = byteswap32(v33);
  v19 = v15 + 77;
  sceKernelMemcpy(&v34, v18, 4);
  a3[15] = byteswap32(v34);
  sceKernelMemcpy(v35, v17 + 8, 8);
  v20 = byteswap32(v35[0]);
  a3[16] = byteswap32(v35[1]);
  a3[17] = v20;
  a3[5] = v19;
  if ( a4 == 3 )
  {
    v22 = v19 - v16;
    v11 = sceNpCommerce2_3F6D413C(v19, v27 - (v19 - v16), 17, 0, a3 + 21);
    if ( ((v11 != 0x80550F87) & ((unsigned int)v11 >> 31)) != 0 )
      return v11;
    goto LABEL_9;
  }
  v21 = v27;
  v22 = v19 - v16;
  a3[21] = a3[1] + 4;
  v23 = sceNpCommerce2_E3455E27(v19, v21 - (v19 - v16), 17, 0, &v28);
  if ( v23 >= 0 && v28 != 0 )
    a3[21] = v19 + v23;
  v11 = v23;
  if ( v23 >= 0 )
  {
LABEL_9:
    v24 = v27;
    a3[18] = a3[1] + 4;
    v25 = sceNpCommerce2_E3455E27(v19, v24 - v22, 60, 0, &v29);
    if ( v25 >= 0 && v29 != 0 )
      a3[18] = v19 + v25;
    v11 = v25;
    if ( ((v25 != 0x80550F87) & ((unsigned int)v25 >> 31)) == 0 )
    {
      v11 = sceNpCommerce2_3F6D413C(v19, v27 - v22, 61, 0, a3 + 19);
      if ( v11 >= 0 )
        return store_min_s32(sceNpCommerce2_3F6D413C(v19, v27 - v22, 62, 0, a3 + 20), 0);
    }
  }
  return v11;
}


int store_tlv_fn_19(int *a1, int a2, u32 *a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  u16 v16;
  u32 v17;
  u32 v18;
  u32 v19;
  u32 v20;
  u32 v21;
  u32 v22;
  u32 v23;

  v5 = a1[5];
  v6 = store_disp_fn_56(v5, a1[4] - (v5 - a1[3]), 46, 24, a2, &v16, a1[6]);
  v7 = v6;
  if ( v6 >= 0 )
  {
    v8 = v5 + v6;
    sceKernelMemset(a3, 0, 72);
    v9 = *a1;
    v10 = v16;
    v11 = a1[1];
    sceKernelMemset(a3, 0, 40);
    a3[1] = v11;
    *a3 = v9;
    a3[4] = v10;
    a3[3] = v8;
    sceKernelMemcpy(&v17, v8, 4);
    a3[10] = byteswap32(v17);
    sceKernelMemcpy(&v18, v8 + 4, 4);
    a3[11] = byteswap32(v18);
    sceKernelMemcpy(&v19, v8 + 8, 4);
    a3[12] = byteswap32(v19);
    sceKernelMemcpy(&v20, v8 + 12, 4);
    a3[13] = byteswap32(v20);
    sceKernelMemcpy(&v21, v8 + 16, 4);
    a3[14] = byteswap32(v21);
    sceKernelMemcpy(&v22, v8 + 20, 4);
    a3[15] = byteswap32(v22);
    sceKernelMemcpy(&v23, v8 + 24, 4);
    v12 = a3[3];
    v13 = a3[4];
    a3[17] = byteswap32(v23);
    a3[5] = v8 + 28;
    v14 = sceNpCommerce2_E3455E27(v8 + 28, v13 - (v8 + 28 - v12), 24, 12, 0);
    v7 = v14;
    if ( v14 == -2141909113 )
    {
      a3[16] = 0;
      return 0;
    }
    if ( v14 >= 0 )
    {
      a3[16] = 1;
      return 0;
    }
  }
  return v7;
}


unsigned int store_tlv_fn_20(int *a1, int *a2)
{
  int v4;
  int v5;
  unsigned int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  u16 v15;
  int v16;
  int v17;
  int v18;
  int *v19;
  int *v20;
  int *v21;
  int *v22;
  int v23;
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
  int v35;
  int v36;
  u32 v37[16];
  u8 v38[64];
  u16 v39;
  s16 v40;
  s16 v41;
  s16 v42;
  s16 v43;
  s16 v44;
  s16 v45;
  int *v46;
  u32 *v47;

  v4 = a1[5];
  sceKernelMemcpy(v38, a1[1], 56);
  v5 = sceNpCommerce2_E3455E27(v4, a1[4] - (v4 - a1[3]), 76, 101, &v39);
  v6 = v5;
  if ( v5 >= 0 )
  {
    v7 = v4 + v5;
    sceKernelMemset(a2, 0, 104);
    v8 = *a1;
    v9 = a1[1];
    v10 = v39;
    sceKernelMemset(a2, 0, 40);
    a2[1] = v9;
    *a2 = v8;
    v11 = v7;
    a2[4] = v10;
    a2[3] = v7;
    sceKernelMemset(v37, 0, 60);
    v37[0] = v38;
    v37[1] = 2;
    v37[2] = a2 + 24;
    v37[3] = a2 + 10;
    v37[4] = a2 + 11;
    v37[5] = a2 + 12;
    v37[6] = a2 + 13;
    v37[9] = a2 + 14;
    v37[13] = a2 + 15;
    v37[14] = a2 + 16;
    v12 = store_disp_fn_63(v7, v37, 0, 0);
    v6 = v12;
    if ( v12 >= 0 )
    {
      v13 = v7 + v12 + 36;
      v14 = v13 - v11;
      v15 = v39;
      a2[17] = a2[1] + 4;
      v16 = (u16)(v15 - v14);
      a2[5] = v13;
      v18 = a2[11];
      v17 = sceNpCommerce2_E3455E27(v13, v16, 8, 0, &v40);
      v19 = a2 + 18;
      v20 = a2 + 19;
      v21 = a2 + 20;
      v22 = a2 + 21;
      v46 = a2 + 22;
      v47 = a2 + 23;
      if ( v17 >= 0 && v40 != 0 )
        a2[17] = v13 + v17;
      v23 = v17;
      if ( v17 >= 0 )
      {
        v24 = 0;
        if ( a2 != NULL )
          v24 = a2[1] + 4;
        *v19 = v24;
        v25 = sceNpCommerce2_E3455E27(v13, v16, 56, 0, &v41);
        v26 = v25;
        if ( v25 >= 0 && v41 != 0 )
          *v19 = v13 + v25;
        v23 = v25;
        if ( v25 >= 0 )
        {
          v27 = 0;
          if ( a2 != NULL )
            v27 = a2[1] + 4;
          *v20 = v27;
          if ( v18 == 2 )
          {
            v35 = 0;
            if ( a2 != NULL )
              v35 = a2[1] + 4;
            *v20 = v35;
            v36 = sceNpCommerce2_E3455E27(v13, v16, 18, 0, &v42);
            v26 = v36;
            if ( v36 >= 0 && v42 != 0 )
              *v20 = v13 + v36;
          }
          v23 = v26;
          if ( v26 >= 0 )
          {
            v28 = 0;
            if ( a2 != NULL )
              v28 = a2[1] + 4;
            *v21 = v28;
            v29 = sceNpCommerce2_E3455E27(v13, v16, 7, 0, &v43);
            if ( v29 >= 0 && v43 != 0 )
              *v21 = v13 + v29;
            v23 = v29;
            if ( v29 >= 0 )
            {
              v30 = 0;
              if ( a2 != NULL )
                v30 = a2[1] + 4;
              *v22 = v30;
              v31 = sceNpCommerce2_E3455E27(v13, v16, 16, 0, &v44);
              if ( v31 >= 0 && v44 != 0 )
                *v22 = v13 + v31;
              v23 = v31;
              if ( v31 >= 0 )
              {
                v32 = 0;
                if ( a2 != NULL )
                  v32 = a2[1] + 4;
                *v46 = v32;
                v33 = sceNpCommerce2_E3455E27(v13, v16, 4, 0, &v45);
                if ( v33 >= 0 && v45 != 0 )
                  *v46 = v13 + v33;
                v23 = v33;
                if ( v33 >= 0 )
                {
                  v23 = sceNpCommerce2_3F6D413C(v13, v16, 128, 0, v47);
                  if ( ((v23 != 0x80550F87) & ((unsigned int)v23 >> 31)) == 0 )
                    v23 = 0;
                }
              }
            }
          }
        }
      }
      v6 = v23;
      if ( v23 >= 0 )
      {
        v6 = sceNpCommerce2_3F6D413C(v13, v39 - v14, 156, 0, a2 + 25);
        if ( ((v6 != 0x80550F87) & (v6 >> 31)) == 0 )
          return 0;
      }
    }
  }
  return v6;
}


int store_tlv_fn_21(int *a1, int *a2, int a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  u16 v17;
  int v18;
  int v19;
  int v20;
  int *v21;
  int *v22;
  int *v23;
  int *v24;
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
  int v36;
  int v37;
  int v38;
  int v40; // dc
  int v41;
  int v42;
  int v43;
  u32 v44[20];
  u16 v45;
  s16 v46;
  s16 v47;
  s16 v48;
  s16 v49;
  s16 v50;
  s16 v51;
  s16 v52;
  s16 v53;
  int v54;
  int *v55;
  u32 *v56;

  v54 = a3;
  v5 = a1[5];
  if ( a3 == 3 )
    v6 = 138;
  else
    v6 = 77;
  sceKernelMemset(a2, 0, 176);
  v7 = sceNpCommerce2_E3455E27(v5, a1[4] - (v5 - a1[3]), v6, 157, &v45);
  v8 = v7;
  if ( v7 >= 0 )
  {
    v9 = a1[1];
    v10 = *a1;
    v11 = v45;
    v12 = v5 + v7;
    sceKernelMemset(a2, 0, 40);
    a2[1] = v9;
    *a2 = v10;
    a2[4] = v11;
    a2[3] = v12;
    v13 = v12;
    sceKernelMemset(v44, 0, 80);
    sceKernelMemset(v44, 0, 60);
    v44[0] = a1[1];
    v44[1] = 2;
    v44[2] = a2 + 42;
    v44[3] = a2 + 10;
    v44[4] = a2 + 11;
    v44[5] = a2 + 12;
    v44[6] = a2 + 13;
    v44[9] = a2 + 14;
    v44[13] = a2 + 17;
    v44[14] = a2 + 18;
    v44[11] = a2 + 15;
    v44[12] = a2 + 16;
    v44[15] = a2 + 20;
    v44[16] = a2 + 22;
    v44[17] = a2 + 24;
    v44[18] = a2 + 26;
    v44[19] = a2 + 28;
    v14 = store_disp_fn_64(v12, v44);
    v8 = v14;
    if ( v14 >= 0 )
    {
      v15 = v12 + v14;
      v16 = v15 - v13;
      v17 = v45 - (v15 - v13);
      a2[30] = a2[1] + 4;
      v18 = v17;
      a2[5] = v15;
      v20 = a2[11];
      v19 = sceNpCommerce2_E3455E27(v15, v17, 8, 0, &v46);
      v21 = a2 + 31;
      v22 = a2 + 32;
      v23 = a2 + 36;
      v24 = a2 + 37;
      v55 = a2 + 38;
      v56 = a2 + 40;
      if ( v19 >= 0 && v46 != 0 )
        a2[30] = v15 + v19;
      v8 = v19;
      if ( v19 >= 0 )
      {
        v25 = 0;
        if ( a2 != NULL )
          v25 = a2[1] + 4;
        *v21 = v25;
        v26 = sceNpCommerce2_E3455E27(v15, v18, 56, 0, &v47);
        v27 = v26;
        if ( v26 >= 0 && v47 != 0 )
          *v21 = v15 + v26;
        v8 = v26;
        if ( v26 >= 0 )
        {
          v28 = 0;
          if ( a2 != NULL )
            v28 = a2[1] + 4;
          *v22 = v28;
          if ( v20 == 2 )
          {
            v42 = 0;
            if ( a2 != NULL )
              v42 = a2[1] + 4;
            *v22 = v42;
            v43 = sceNpCommerce2_E3455E27(v15, v18, 18, 0, &v48);
            v27 = v43;
            if ( v43 >= 0 && v48 != 0 )
              *v22 = v15 + v43;
          }
          v8 = v27;
          if ( v27 >= 0 )
          {
            v29 = 0;
            if ( a2 != NULL )
              v29 = a2[1] + 4;
            *v23 = v29;
            v30 = sceNpCommerce2_E3455E27(v15, v18, 7, 0, &v49);
            if ( v30 >= 0 && v49 != 0 )
              *v23 = v15 + v30;
            v8 = v30;
            if ( v30 >= 0 )
            {
              v31 = 0;
              if ( a2 != NULL )
                v31 = a2[1] + 4;
              *v24 = v31;
              v32 = sceNpCommerce2_E3455E27(v15, v18, 16, 0, &v50);
              if ( v32 >= 0 && v50 != 0 )
                *v24 = v15 + v32;
              v8 = v32;
              if ( v32 >= 0 )
              {
                v33 = 0;
                if ( a2 != NULL )
                  v33 = a2[1] + 4;
                *v55 = v33;
                v34 = sceNpCommerce2_E3455E27(v15, v18, 4, 0, &v51);
                if ( v34 >= 0 && v51 != 0 )
                  *v55 = v15 + v34;
                v8 = v34;
                if ( v34 >= 0 )
                {
                  v8 = sceNpCommerce2_3F6D413C(v15, v18, 128, 0, v56);
                  if ( ((v8 != 0x80550F87) & ((unsigned int)v8 >> 31)) == 0 )
                    v8 = 0;
                }
              }
            }
          }
        }
      }
      if ( v8 >= 0 )
      {
        v35 = v45;
        a2[33] = a2[1] + 4;
        v36 = sceNpCommerce2_E3455E27(v15, v35 - v16, 60, 0, &v52);
        if ( v36 >= 0 && v52 != 0 )
          a2[33] = v15 + v36;
        v8 = v36;
        if ( ((v36 != 0x80550F87) & ((unsigned int)v36 >> 31)) == 0 )
        {
          v8 = sceNpCommerce2_3F6D413C(v15, v45 - v16, 61, 0, a2 + 34);
          if ( v8 >= 0 )
          {
            v8 = sceNpCommerce2_3F6D413C(v15, v45 - v16, 62, 0, a2 + 35);
            if ( v8 >= 0 )
            {
              v37 = v45;
              a2[39] = a2[1] + 4;
              v38 = sceNpCommerce2_E3455E27(v15, v37 - v16, 68, 0, &v53);
              if ( v38 >= 0 && v53 != 0 )
                a2[39] = v15 + v38;
              v8 = v38;
              if ( ((v38 != 0x80550F87) & ((unsigned int)v38 >> 31)) == 0 )
              {
                if ( v54 == 2 )
                {
                  v40 = sceNpCommerce2_E3455E27(v15, v45 - v16, 75, 8, 0) < 0;
                  v41 = 1;
                  if ( !v40 )
                    v41 = 3;
                  a2[43] = v41;
                }
                v8 = sceNpCommerce2_3F6D413C(v15, v45 - v16, 133, 0, a2 + 41);
                if ( ((v8 != 0x80550F87) & ((unsigned int)v8 >> 31)) == 0 )
                  return 0;
              }
            }
          }
        }
      }
    }
  }
  return v8;
}


int store_tlv_fn_22(u32 *a1, int a2, u32 *a3, u32 *a4)
{
  int v6;
  int v7;
  int v8;
  unsigned int v9;
  int v10;
  u16 v12;
  u32 v13[3];

  v6 = a1[5];
  v7 = store_disp_fn_55(v6, a1[4] - (v6 - a1[3]), 61, 0, a2, &v12);
  v8 = v6 + v7;
  if ( v7 >= 0 )
  {
    v9 = v12;
    *a3 = v8;
    v10 = v8 + 3;
    if ( v9 >= 7 )
    {
      if ( a4 != NULL )
      {
        sceKernelMemcpy(v13, v10, 4);
        *a4 = byteswap32(v13[0]);
      }
    }
    else if ( a4 != NULL )
    {
      *a4 = 0;
    }
  }
  return v7;
}


int store_tlv_fn_23(int *a1, u32 *a2)
{
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int *v20;
  int v21;
  int v22;
  u16 v24;
  s16 v25;
  s16 v26;
  u32 v27;
  u32 v28;
  u32 v29;
  u32 v30[3];

  v4 = a1[3];
  v5 = sceNpCommerce2_E3455E27(v4, a1[4], 48, 25, &v24);
  v6 = v5;
  if ( v5 >= 0 )
  {
    v7 = v4 + v5;
    sceKernelMemset(a2, 0, 68);
    v8 = a1[1];
    v9 = v24;
    v10 = *a1;
    sceKernelMemset(a2, 0, 40);
    v11 = v7 + 21;
    v12 = v24;
    *a2 = v10;
    a2[11] = v8 + 4;
    a2[4] = v9;
    a2[1] = v8;
    a2[3] = v7;
    a2[10] = v7;
    a2[5] = v7 + 21;
    v13 = sceNpCommerce2_E3455E27(v7 + 21, v12 - 21, 15, 0, &v25);
    v6 = v13;
    if ( v13 >= 0 && v25 != 0 )
      a2[11] = v11 + v13;
    if ( v13 >= 0 )
    {
      v14 = sceNpCommerce2_E3455E27(v7 + 21, v24 - 21, 99, 20, 0);
      v6 = v14;
      if ( v14 == -2141909113 )
      {
        return 0;
      }
      else
      {
        v15 = v11 + v14;
        if ( v14 >= 0 )
        {
          sceKernelMemcpy(&v27, v11 + v14, 4);
          a2[12] = byteswap32(v27);
          sceKernelMemcpy(&v28, v15 + 4, 4);
          v17 = v15 + 8;
          v16 = v15 + 4;
          a2[13] = byteswap32(v28);
          sceKernelMemcpy(&v29, v15 + 8, 4);
          a2[14] = byteswap32(v29);
          sceKernelMemcpy(v30, v15 + 12, 4);
          v18 = v24;
          a2[15] = byteswap32(v30[0]);
          v19 = v18 - (v15 + 16 - v7);
          v20 = a2 + 16;
          v21 = 0;
          if ( a2 != NULL )
            v21 = a2[1] + 4;
          *v20 = v21;
          v22 = sceNpCommerce2_E3455E27(v17 + 8, v19, 98, 0, &v26);
          v6 = v22;
          if ( v22 >= 0 && v26 != 0 )
            *v20 = v16 + 12 + v22;
        }
      }
    }
  }
  return v6;
}


unsigned int store_tlv_fn_24(int *a1, int a2, u32 *a3)
{
  int v6;
  int v7;
  unsigned int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  u32 v14;
  int v15;
  u32 v16;
  u64 v17;
  int v18;
  u32 v19;
  u64 v20;
  int v21;
  u32 v22;
  u64 v23;
  u32 v24;
  int v25;
  u32 v26;
  int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  int v34;
  int v35;
  int v36;
  int v37;
  int v38;
  int v39;
  unsigned int v40;
  int v41;
  int v42;
  int v43;
  int v44;
  int v46;
  int v47;
  u32 v48;
  int v49;
  int v50;
  int v51;
  int v52;
  int v53;
  int v54;
  int v55;
  int v56;
  int v57;
  int v58;
  int v59;
  int v60;
  int v61;
  int v62; // dc
  int v63;
  int v64;
  int v65;
  u16 v66;
  s16 v67;
  s16 v68;
  s16 v69;
  s16 v70;
  s16 v71;
  s16 v72;
  s16 v73;
  s16 v74;
  s16 v75;
  s16 v76;
  s16 v77;
  u32 v78;
  u32 v79;
  u32 v80;
  u32 v81[2];
  s64 v82;
  s64 v83;
  u32 v84;
  s64 v85;
  u32 v86[2];
  u32 v87;
  u32 v88;
  u32 v89;
  u32 v90[2];
  u32 v91;
  u32 v92[3];

  v6 = a1[3];
  sceKernelMemcpy(&v65, a1[1], 56);
  v7 = store_disp_fn_57(v6, a1[4], 43, 125, a2, &v66, a1[6]);
  v8 = v7;
  if ( v7 >= 0 )
  {
    v9 = v6 + v7;
    sceKernelMemset(a3, 0, 184);
    v10 = *a1;
    v11 = a1[1];
    v12 = v66;
    sceKernelMemset(a3, 0, 40);
    a3[1] = v11;
    v13 = v9;
    *a3 = v10;
    a3[4] = v12;
    a3[3] = v9;
    a3[10] = v9;
    v9 += 49;
    sceKernelMemcpy(&v78, v9, 4);
    v9 += 4;
    a3[11] = byteswap32(v78);
    sceKernelMemcpy(&v79, v9, 4);
    v9 += 4;
    a3[12] = byteswap32(v79);
    sceKernelMemcpy(&v80, v9, 4);
    v9 += 4;
    a3[13] = byteswap32(v80);
    sceKernelMemcpy(v81, v9, 8);
    v14 = byteswap32(v81[0]);
    a3[14] = byteswap32(v81[1]);
    a3[15] = v14;
    v15 = v9 + 8;
    sceKernelMemcpy(&v82, v15, 8);
    v16 = byteswap32(HIDWORD(v82));
    v17 = __PAIR64__(byteswap32(v82), v16);
    v82 = v17;
    if ( v17 != 0 )
    {
      a3[16] = 1000 * v16 - 13910016;
      a3[17] = ((1000 * v17) >> 32) + 14467070 + (1000 * v16 - 13910016 < 0xFF2BC000);
      v82 = 1000 * v17;
    }
    else
    {
      sceKernelMemset(a3 + 16, 0, 8);
    }
    v18 = v15 + 8;
    sceKernelMemcpy(&v83, v18, 8);
    v19 = byteswap32(v83);
    v20 = __PAIR64__(v19, byteswap32(HIDWORD(v83)));
    v83 = __PAIR64__(v19, v20);
    if ( ((unsigned int)v20 | v19) != 0 )
    {
      a3[18] = 1000 * v20 - 13910016;
      a3[19] = ((1000 * v20) >> 32) + 14467070 + ((unsigned int)(1000 * v20 - 13910016) < 0xFF2BC000);
      v83 = 1000 * v20;
    }
    else
    {
      sceKernelMemset(a3 + 18, 0, 8);
    }
    v21 = v18 + 8;
    sceKernelMemcpy(&v84, v18 + 8, 4);
    a3[20] = byteswap32(v84) != 0;
    sceKernelMemcpy(&v85, v18 + 12, 8);
    v22 = byteswap32(v85);
    v23 = __PAIR64__(v22, byteswap32(HIDWORD(v85)));
    v85 = __PAIR64__(v22, v23);
    if ( ((unsigned int)v23 | v22) != 0 )
    {
      a3[22] = 1000 * v23 - 13910016;
      a3[23] = ((1000 * v23) >> 32) + 14467070 + ((unsigned int)(1000 * v23 - 13910016) < 0xFF2BC000);
      v85 = 1000 * v23;
    }
    else
    {
      sceKernelMemset(a3 + 22, 0, 8);
    }
    sceKernelMemcpy(v86, v18 + 20, 8);
    v24 = byteswap32(v86[0]);
    a3[42] = byteswap32(v86[1]);
    a3[43] = v24;
    sceKernelMemcpy(&v87, v18 + 28, 4);
    v25 = v18 + 32;
    v26 = byteswap32(v87);
    v27 = v66;
    a3[24] = a3[1] + 4;
    v28 = v21 + 24 - v13;
    a3[45] = v26;
    a3[5] = v21 + 24;
    v29 = sceNpCommerce2_E3455E27(v21 + 24, v27 - v28, 9, 0, &v67);
    if ( v29 >= 0 && v67 != 0 )
      a3[24] = v25 + v29;
    v8 = v29;
    if ( v29 >= 0 )
    {
      v30 = v66;
      a3[25] = a3[1] + 4;
      v31 = sceNpCommerce2_E3455E27(v25, v30 - v28, 10, 0, &v68);
      if ( v31 >= 0 && v68 != 0 )
        a3[25] = v25 + v31;
      v8 = v31;
      if ( v31 >= 0 )
      {
        v32 = v66;
        a3[26] = a3[1] + 4;
        v33 = sceNpCommerce2_E3455E27(v25, v32 - v28, 4, 0, &v69);
        if ( v33 >= 0 && v69 != 0 )
          a3[26] = v25 + v33;
        v8 = v33;
        if ( v33 >= 0 )
        {
          v34 = v66;
          a3[27] = a3[1] + 4;
          v35 = sceNpCommerce2_E3455E27(v25, v34 - v28, 5, 0, &v70);
          if ( v35 >= 0 && v70 != 0 )
            a3[27] = v25 + v35;
          v8 = v35;
          if ( v35 >= 0 )
          {
            v36 = v66;
            a3[28] = a3[1] + 4;
            v37 = sceNpCommerce2_E3455E27(v25, v36 - v28, 16, 0, &v71);
            if ( v37 >= 0 && v71 != 0 )
              a3[28] = v25 + v37;
            v8 = v37;
            if ( v37 >= 0 )
            {
              v38 = v66;
              a3[29] = a3[1] + 4;
              v39 = sceNpCommerce2_E3455E27(v25, v38 - v28, 20, 0, &v72);
              if ( v39 >= 0 && v72 != 0 )
                a3[29] = v25 + v39;
              v8 = v39;
              if ( v39 >= 0 )
              {
                v40 = a3[12];
                if ( v40 == 2 )
                {
                  v41 = v66;
                }
                else
                {
                  if ( v40 < 3 )
                  {
                    if ( v40 == 1 )
                    {
                      v46 = sceNpCommerce2_E3455E27(v25, v66 - v28, 152, 8, 0);
                      v8 = v46;
                      if ( v46 < 0 )
                        return v8;
                      v47 = v25 + v46;
                      sceKernelMemcpy(&v88, v25 + v46, 4);
                      a3[30] = byteswap32(v88);
                      sceKernelMemcpy(&v89, v47 + 4, 4);
                      a3[31] = byteswap32(v89);
                      sceKernelMemcpy(v90, v47 + 8, 8);
                      v48 = byteswap32(v90[0]);
                      a3[32] = byteswap32(v90[1]);
                      a3[33] = v48;
                      v42 = a3[1];
                      goto LABEL_37;
                    }
                    goto LABEL_36;
                  }
                  if ( v40 != 4 )
                  {
                    v42 = a3[1];
LABEL_37:
                    v43 = v66;
                    a3[40] = v42 + 4;
                    v44 = sceNpCommerce2_E3455E27(v25, v43 - v28, 56, 0, &v77);
                    if ( v44 >= 0 && v77 != 0 )
                      a3[40] = v25 + v44;
                    v8 = v44;
                    if ( ((v44 != 0x80550F87) & ((unsigned int)v44 >> 31)) == 0 )
                    {
                      v8 = sceNpCommerce2_3F6D413C(v25, v66 - v28, 128, 0, a3 + 44);
                      if ( ((v8 != 0x80550F87) & (v8 >> 31)) == 0 )
                        return 0;
                    }
                    return v8;
                  }
                  v41 = v66;
                }
                v49 = 141;
                if ( v40 != 4 )
                  v49 = 86;
                v50 = sceNpCommerce2_E3455E27(v25, v41 - (v21 + 24 - v13), v49, 8, 0);
                v8 = v50;
                if ( v50 < 0 )
                  return v8;
                v51 = v25 + v50;
                sceKernelMemcpy(&v91, v25 + v50, 4);
                v52 = v51 + 4;
                a3[30] = byteswap32(v91);
                sceKernelMemcpy(v92, v51 + 4, 4);
                v53 = v66;
                v54 = a3[1] + 4;
                v25 = v51 + 16;
                a3[31] = byteswap32(v92[0]);
                v28 = v51 + 16 - v13;
                a3[35] = v54;
                v55 = sceNpCommerce2_E3455E27(v52 + 12, v53 - v28, 89, 0, &v73);
                if ( v55 >= 0 && v73 != 0 )
                  a3[35] = v25 + v55;
                v8 = v55;
                if ( v55 < 0 )
                  return v8;
                v56 = v66;
                a3[36] = a3[1] + 4;
                v57 = sceNpCommerce2_E3455E27(v52 + 12, v56 - v28, 85, 0, &v74);
                if ( v57 >= 0 && v74 != 0 )
                  a3[36] = v25 + v57;
                v8 = v57;
                if ( ((v57 != 0x80550F87) & ((unsigned int)v57 >> 31)) != 0 )
                  return v8;
                v58 = v66;
                a3[37] = a3[1] + 4;
                v59 = sceNpCommerce2_E3455E27(v52 + 12, v58 - v28, 7, 0, &v75);
                if ( v59 >= 0 && v75 != 0 )
                  a3[37] = v25 + v59;
                v8 = v59;
                if ( v59 < 0 )
                  return v8;
                v60 = v66;
                a3[39] = a3[1] + 4;
                v61 = sceNpCommerce2_E3455E27(v52 + 12, v60 - v28, 68, 0, &v76);
                if ( v61 >= 0 && v76 != 0 )
                  a3[39] = v25 + v61;
                v8 = v61;
                if ( ((v61 != 0x80550F87) & ((unsigned int)v61 >> 31)) != 0 )
                  return v8;
                v62 = sceNpCommerce2_E3455E27(v52 + 12, v66 - v28, 75, 8, 0) < 0;
                v63 = 1;
                if ( !v62 )
                  v63 = 3;
                v64 = v66;
                a3[38] = v63;
                v8 = sceNpCommerce2_3F6D413C(v52 + 12, v64 - v28, 157, 0, a3 + 34);
                if ( ((v8 != 0x80550F87) & (v8 >> 31)) != 0 )
                  return v8;
LABEL_36:
                v42 = a3[1];
                goto LABEL_37;
              }
            }
          }
        }
      }
    }
  }
  return v8;
}


int store_tlv_fn_25(u32 *a1, int a2, int a3)
{
  int v3;
  int v5;
  int v8;
  int result;
  int v10;
  int v11;
  int v12;
  int v13;
  u16 v14[8];

  v3 = a3 ^ 3;
  v5 = 141;
  if ( v3 != 0 )
    v5 = 86;
  v8 = a1[5];
  result = sceNpCommerce2_E3455E27(v8, a1[4] - (v8 - a1[3]), v5, 8, v14);
  v10 = v8 + result;
  if ( result >= 0 )
  {
    v11 = v10;
    v12 = v10;
    v13 = v10 + 8;
    if ( a3 == 3 )
      return store_disp_fn_66(*a1, a1[1], v12, v14[0], v13, a2);
    else
      return store_disp_fn_65(*a1, a1[1], v11, v14[0], v13, a2);
  }
  return result;
}


int store_tlv_fn_26(int *a1, int a2, u32 *a3)
{
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int *v26;
  int v27;
  int v28;
  int v29;
  u16 v31;
  s16 v32;
  s16 v33;
  s16 v34;
  u32 v35;
  u32 v36;
  u32 v37[4];

  v6 = a1[5];
  v7 = sceNpCommerce2_E3455E27(v6, a1[4] - (v6 - a1[3]), 86, 8, 0);
  v8 = v7;
  if ( v7 >= 0 )
  {
    v9 = v6 + v7 + 16;
    v10 = store_disp_fn_55(v9, a1[4] - (v9 - a1[3]), 157, 0, a2, &v31);
    v8 = v10;
    if ( v10 >= 0 )
    {
      v11 = v9 + v10;
      sceKernelMemset(a3, 0, 72);
      v12 = *a1;
      v13 = v31;
      v14 = a1[1];
      sceKernelMemset(a3, 0, 40);
      a3[11] = v11 + 49;
      a3[3] = v11;
      v15 = v11;
      a3[10] = v11;
      a3[1] = v14;
      *a3 = v12;
      a3[4] = v13;
      v16 = v11 + 106;
      sceKernelMemcpy(&v35, v11 + 98, 4);
      v17 = v11 + 102;
      a3[12] = byteswap32(v35);
      v18 = v11 + 110;
      sceKernelMemcpy(&v36, v17, 4);
      v19 = v16;
      a3[13] = byteswap32(v36);
      v20 = v18 - v15;
      sceKernelMemcpy(v37, v19, 4);
      v21 = a3[1];
      v22 = v31;
      a3[14] = byteswap32(v37[0]);
      a3[15] = v21 + 4;
      a3[5] = v18;
      v23 = sceNpCommerce2_E3455E27(v18, v22 - (v18 - v15), 20, 0, &v32);
      if ( v23 >= 0 && v32 != 0 )
        a3[15] = v18 + v23;
      v8 = v23;
      if ( v23 >= 0 )
      {
        v24 = v31;
        a3[16] = a3[1] + 4;
        v25 = sceNpCommerce2_E3455E27(v18, v24 - v20, 89, 0, &v33);
        if ( v25 >= 0 && v33 != 0 )
          a3[16] = v18 + v25;
        v8 = v25;
        if ( v25 >= 0 )
        {
          v26 = a3 + 17;
          v27 = 0;
          v28 = v31 - v20;
          if ( a3 != NULL )
            v27 = a3[1] + 4;
          *v26 = v27;
          v29 = sceNpCommerce2_E3455E27(v18, v28, 159, 0, &v34);
          if ( v29 >= 0 && v34 != 0 )
            *v26 = v18 + v29;
          v8 = v29;
          if ( ((v29 != 0x80550F87) & ((unsigned int)v29 >> 31)) == 0 )
            return 0;
        }
      }
    }
  }
  return v8;
}


int store_tlv_fn_27(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int *v15;
  int v16;
  s32 v17;
  s16 v18;
  s16 v19;
  s16 v20;
  s16 v21;
  s16 v22;
  s16 v23;
  int v24;
  u32 v25[4];

  result = store_disp_fn_54(a2, a3, &v24, v25, 0);
  if ( result >= 0 )
  {
    v6 = a2 + result;
    sceKernelMemset(a1, 0, 64);
    v7 = v24;
    v8 = v25[0];
    sceKernelMemset(a1, 0, 40);
    v9 = v25[0];
    a1[2] = v6 - a2 + v25[0];
    *a1 = v7;
    a1[4] = v8;
    a1[10] = a2 + 4;
    a1[1] = a2;
    a1[3] = v6;
    result = sceNpCommerce2_E3455E27(v6, v9, 79, 0, &v18);
    if ( result >= 0 && v18 != 0 )
      a1[10] = v6 + result;
    if ( result >= 0 )
    {
      v10 = v25[0];
      a1[11] = a1[1] + 4;
      result = sceNpCommerce2_E3455E27(v6, v10, 80, 0, &v19);
      if ( result >= 0 && v19 != 0 )
        a1[11] = v6 + result;
      if ( result >= 0 )
      {
        v11 = v25[0];
        a1[12] = a1[1] + 4;
        result = sceNpCommerce2_E3455E27(v6, v11, 81, 0, &v20);
        if ( result >= 0 && v20 != 0 )
          a1[12] = v6 + result;
        if ( result >= 0 )
        {
          v12 = v25[0];
          a1[13] = a1[1] + 4;
          result = sceNpCommerce2_E3455E27(v6, v12, 82, 0, &v21);
          if ( result >= 0 && v21 != 0 )
            a1[13] = v6 + result;
          if ( result >= 0 )
          {
            v13 = v25[0];
            a1[14] = a1[1] + 4;
            result = sceNpCommerce2_E3455E27(v6, v13, 54, 0, &v22);
            if ( result >= 0 && v22 != 0 )
              a1[14] = v6 + result;
            if ( result >= 0 )
            {
              v14 = v25[0];
              v15 = a1 + 15;
              v16 = 0;
              if ( a1 != NULL )
                v16 = a1[1] + 4;
              *v15 = v16;
              v17 = sceNpCommerce2_E3455E27(v6, v14, 84, 0, &v23);
              if ( v17 >= 0 && v23 != 0 )
                *v15 = v6 + v17;
              return store_min_s32(v17, 0);
            }
          }
        }
      }
    }
  }
  return result;
}


int store_tlv_fn_28(int *a1, u32 *a2)
{
  int v4;
  int result;
  int v6;
  int v7;
  int *v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  u32 v17;
  int v18;
  u16 v19;
  s16 v20;
  u32 v21;
  u32 v22;

  v4 = a1[3];
  result = sceNpCommerce2_E3455E27(v4, a1[4], 52, 29, &v19);
  v6 = v4 + result;
  v7 = v4 + result;
  if ( result >= 0 )
  {
    v8 = a2 + 13;
    sceKernelMemset(a2, 0, 56);
    v9 = *a1;
    v10 = v19;
    v11 = a1[1];
    sceKernelMemset(a2, 0, 40);
    a2[1] = v11;
    v12 = v6;
    *a2 = v9;
    v13 = v6 + 4;
    a2[3] = v6;
    v14 = v6 + 8;
    v15 = v6 + 25;
    a2[4] = v10;
    sceKernelMemcpy(&v21, v12, 4);
    a2[10] = byteswap32(v21);
    sceKernelMemcpy(&v22, v13, 4);
    v16 = v19;
    v17 = byteswap32(v22);
    a2[12] = v14;
    a2[11] = v17;
    v18 = 0;
    a2[5] = v15;
    if ( a2 != NULL )
      v18 = a2[1] + 4;
    *v8 = v18;
    result = sceNpCommerce2_E3455E27(v15, v16 - (v15 - v7), 19, 0, &v20);
    if ( result >= 0 && v20 != 0 )
      *v8 = v15 + result;
  }
  return result;
}


unsigned int store_tlv_fn_29(int *a1, int a2, u32 *a3)
{
  int v5;
  int v6;
  unsigned int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  unsigned int v15;
  int v16;
  int v17;
  u32 v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
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
  u16 v36;
  s16 v37;
  s16 v38;
  s16 v39;
  s16 v40;
  s16 v41;
  s16 v42;
  s16 v43;
  u64 v44;
  u32 v45;
  u32 v46;
  u32 v47[4];
  u64 v48; // 0:$a4.4,4:$t7.4

  v5 = a1[5];
  v6 = store_disp_fn_55(v5, a1[4] - (v5 - a1[3]), 122, 93, a2, &v36);
  v7 = v6;
  if ( v6 >= 0 )
  {
    v8 = v5 + v6;
    sceKernelMemset(a3, 0, 104);
    v9 = *a1;
    v10 = a1[1];
    v11 = v36;
    sceKernelMemset(a3, 0, 40);
    a3[1] = v10;
    v12 = v8;
    *a3 = v9;
    a3[3] = v8;
    a3[10] = v8;
    v13 = v8 + 57;
    a3[4] = v11;
    sceKernelMemcpy(&v44, v13, 8);
    v48 = ((u64)(v48) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(byteswap32(v44)) << 32);
    v48 = ((u64)(v48) & 0xFFFFFFFF00000000ULL) | (u32)(byteswap32(HIDWORD(v44)));
    v44 = v48;
    if ( ((unsigned int)v48 | HIDWORD(v48)) != 0 )
    {
      v15 = 1000 * v48 - 13910016;
      a3[12] = v15;
      a3[13] = ((1000 * v48) >> 32) + 14467070 + (v15 < 0xFF2BC000);
      v44 = 1000 * v48;
    }
    else
    {
      sceKernelMemset(a3 + 12, 0, 8);
    }
    v16 = v13 + 8;
    sceKernelMemcpy(&v45, v13 + 8, 4);
    a3[14] = byteswap32(v45);
    sceKernelMemcpy(&v46, v13 + 12, 4);
    a3[22] = byteswap32(v46);
    sceKernelMemcpy(v47, v13 + 16, 4);
    v17 = v13 + 20;
    v18 = byteswap32(v47[0]);
    a3[23] = v18;
    if ( v18 != 3 && v18 != 5 )
      a3[14] = -1;
    v19 = a3[1];
    v20 = v36;
    a3[5] = v17;
    v21 = v17 - v12;
    a3[15] = v19 + 4;
    v22 = sceNpCommerce2_E3455E27(v16 + 12, v20 - (v17 - v12), 4, 0, &v37);
    if ( v22 >= 0 && v37 != 0 )
      a3[15] = v17 + v22;
    v7 = v22;
    if ( ((v22 != 0x80550F87) & ((unsigned int)v22 >> 31)) == 0 )
    {
      v23 = v36;
      a3[16] = a3[1] + 4;
      v24 = sceNpCommerce2_E3455E27(v16 + 12, v23 - v21, 7, 0, &v38);
      if ( v24 >= 0 && v38 != 0 )
        a3[16] = v17 + v24;
      v7 = v24;
      if ( v24 >= 0 )
      {
        v25 = v36;
        a3[17] = a3[1] + 4;
        v26 = sceNpCommerce2_E3455E27(v17, v25 - v21, 8, 0, &v39);
        if ( v26 >= 0 && v39 != 0 )
          a3[17] = v17 + v26;
        v7 = v26;
        if ( v26 >= 0 )
        {
          v27 = v36;
          a3[18] = a3[1] + 4;
          v28 = sceNpCommerce2_E3455E27(v17, v27 - v21, 16, 0, &v40);
          if ( v28 >= 0 && v40 != 0 )
            a3[18] = v17 + v28;
          v7 = v28;
          if ( v28 >= 0 )
          {
            v29 = v36;
            a3[19] = a3[1] + 4;
            v30 = sceNpCommerce2_E3455E27(v17, v29 - v21, 12, 0, &v41);
            if ( v30 >= 0 && v41 != 0 )
              a3[19] = v17 + v30;
            v7 = v30;
            if ( v30 >= 0 )
            {
              v31 = v36;
              a3[20] = a3[1] + 4;
              v32 = sceNpCommerce2_E3455E27(v17, v31 - v21, 13, 0, &v42);
              if ( v32 >= 0 && v42 != 0 )
                a3[20] = v17 + v32;
              v7 = v32;
              if ( v32 >= 0 )
              {
                v33 = v36;
                a3[21] = a3[1] + 4;
                v34 = sceNpCommerce2_E3455E27(v17, v33 - v21, 119, 0, &v43);
                if ( v34 >= 0 && v43 != 0 )
                  a3[21] = v17 + v34;
                v7 = v34;
                if ( v34 >= 0 )
                {
                  v7 = sceNpCommerce2_3F6D413C(v17, v36 - v21, 128, 0, a3 + 24);
                  if ( ((v7 != 0x80550F87) & (v7 >> 31)) == 0 )
                    return 0;
                }
              }
            }
          }
        }
      }
    }
  }
  return v7;
}


int store_tlv_fn_30(int *a1, u32 *a2)
{
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11;
  u16 v13;
  u32 v14;
  u32 v15;

  v4 = a1[5];
  sceKernelMemset(a2, 0, 52);
  v5 = sceNpCommerce2_E3455E27(v4, a1[4] - (v4 - a1[3]), 153, 72, &v13);
  v6 = v4 + v5;
  if ( v5 >= 0 )
  {
    v7 = *a1;
    v8 = v13;
    v9 = a1[1];
    sceKernelMemset(a2, 0, 40);
    *a2 = v7;
    a2[1] = v9;
    a2[4] = v8;
    a2[3] = v6;
    sceKernelMemcpy(&v14, v6 + 28, 4);
    a2[10] = byteswap32(v14);
    sceKernelMemcpy(&v15, v6 + 32, 4);
    v10 = v13;
    v11 = byteswap32(v15);
    a2[5] = v6 + 72;
    a2[11] = v11;
    v5 = sceNpCommerce2_3F6D413C(v6 + 72, v10 - 72, 156, 0, a2 + 12);
    if ( ((v5 != 0x80550F87) & ((unsigned int)v5 >> 31)) == 0 )
      return 0;
  }
  return v5;
}


int store_tlv_fn_31(int *a1, u32 *a2, int a3)
{
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  u32 v20;
  int v21;
  u32 v22;
  u64 v23;
  u32 v24;
  u64 v25;
  u32 v26;
  u64 v27;
  u32 v28;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  int v34;
  int v35;
  int v36;
  int v37;
  int v38;
  int v40;
  u16 v41;
  s16 v42;
  s16 v43;
  s16 v44;
  s16 v45;
  u32 v46;
  u32 v47;
  u32 v48;
  u32 v49;
  u32 v50;
  u32 v51;
  u32 v52;
  u32 v53;
  u32 v54;
  s64 v55;
  s64 v56;
  s64 v57;
  u32 v58;
  u32 v59[4];

  v6 = a1[5];
  sceKernelMemcpy(&v40, a1[1], 56);
  if ( a3 == 3 )
  {
    v7 = 143;
    v8 = 144;
  }
  else
  {
    v7 = 127;
    v8 = 133;
  }
  sceKernelMemset(a2, 0, 160);
  v9 = sceNpCommerce2_E3455E27(v6, a1[4] - (v6 - a1[3]), v7, 100, &v41);
  v10 = v9;
  if ( v9 < 0 )
    return v10;
  v11 = *a1;
  v12 = v41;
  v13 = a1[1];
  v14 = v6 + v9;
  sceKernelMemset(a2, 0, 40);
  a2[1] = v13;
  v15 = v14;
  *a2 = v11;
  v16 = v14;
  a2[4] = v12;
  v17 = v14 + 4;
  v18 = v14 + 8;
  a2[3] = v14;
  v19 = v14 + 12;
  sceKernelMemcpy(&v46, v15, 4);
  a2[10] = byteswap32(v46);
  sceKernelMemcpy(&v47, v17, 4);
  a2[11] = byteswap32(v47);
  sceKernelMemcpy(&v48, v18, 4);
  a2[12] = byteswap32(v48) != 0;
  sceKernelMemcpy(&v49, v19, 4);
  a2[13] = byteswap32(v49);
  sceKernelMemcpy(&v50, v19 + 4, 4);
  a2[14] = byteswap32(v50);
  sceKernelMemcpy(&v51, v19 + 8, 4);
  a2[15] = byteswap32(v51);
  sceKernelMemcpy(&v52, v19 + 12, 4);
  v20 = byteswap32(v52);
  a2[16] = v20;
  a2[12] = (v20 & 0xA) != 0;
  sceKernelMemcpy(&v53, v19 + 16, 4);
  v21 = v19 + 20;
  a2[17] = byteswap32(v53);
  sceKernelMemcpy(&v54, v19 + 20, 4);
  a2[18] = byteswap32(v54);
  sceKernelMemcpy(&v55, v19 + 24, 8);
  v22 = byteswap32(v55);
  v23 = __PAIR64__(v22, byteswap32(HIDWORD(v55)));
  v55 = __PAIR64__(v22, v23);
  if ( ((unsigned int)v23 | v22) != 0 )
  {
    a2[20] = 1000 * v23 - 13910016;
    a2[21] = ((1000 * v23) >> 32) + 14467070 + ((unsigned int)(1000 * v23 - 13910016) < 0xFF2BC000);
    v55 = 1000 * v23;
  }
  else
  {
    sceKernelMemset(a2 + 20, 0, 8);
  }
  sceKernelMemcpy(&v56, v19 + 32, 8);
  v24 = byteswap32(v56);
  v25 = __PAIR64__(v24, byteswap32(HIDWORD(v56)));
  v56 = __PAIR64__(v24, v25);
  if ( ((unsigned int)v25 | v24) != 0 )
  {
    a2[22] = 1000 * v25 - 13910016;
    a2[23] = ((1000 * v25) >> 32) + 14467070 + ((unsigned int)(1000 * v25 - 13910016) < 0xFF2BC000);
    v56 = 1000 * v25;
  }
  else
  {
    sceKernelMemset(a2 + 22, 0, 8);
  }
  sceKernelMemcpy(&v57, v19 + 40, 8);
  v26 = byteswap32(v57);
  v27 = __PAIR64__(v26, byteswap32(HIDWORD(v57)));
  v57 = __PAIR64__(v26, v27);
  if ( ((unsigned int)v27 | v26) != 0 )
  {
    a2[24] = 1000 * v27 - 13910016;
    a2[25] = ((1000 * v27) >> 32) + 14467070 + ((unsigned int)(1000 * v27 - 13910016) < 0xFF2BC000);
    v57 = 1000 * v27;
  }
  else
  {
    sceKernelMemset(a2 + 24, 0, 8);
  }
  sceKernelMemcpy(&v58, v19 + 48, 4);
  a2[26] = byteswap32(v58);
  sceKernelMemcpy(v59, v19 + 52, 8);
  v28 = byteswap32(v59[0]);
  v29 = v19 + 60;
  a2[28] = byteswap32(v59[1]);
  a2[29] = v28;
  a2[5] = v21 + 40;
  if ( a3 == 3 )
  {
    v31 = v29 - v16;
    v10 = sceNpCommerce2_3F6D413C(v21 + 40, v41 - v31, 17, 0, a2 + 39);
    if ( ((v10 != 0x80550F87) & ((unsigned int)v10 >> 31)) != 0 )
      return v10;
    goto LABEL_19;
  }
  v30 = v41;
  v31 = v29 - v16;
  a2[39] = a2[1] + 4;
  v32 = sceNpCommerce2_E3455E27(v29, v30 - v31, 17, 0, &v42);
  if ( v32 >= 0 && v42 != 0 )
    a2[39] = v29 + v32;
  v10 = v32;
  if ( v32 >= 0 )
  {
LABEL_19:
    v10 = sceNpCommerce2_3F6D413C(v29, v41 - v31, 35, 0, a2 + 30);
    if ( v10 >= 0 )
    {
      v10 = sceNpCommerce2_3F6D413C(v29, v41 - v31, 36, 0, a2 + 31);
      if ( v10 >= 0 )
      {
        v10 = sceNpCommerce2_3F6D413C(v29, v41 - v31, 37, 0, a2 + 32);
        if ( v10 >= 0 )
        {
          v33 = v41;
          a2[33] = a2[1] + 4;
          v34 = sceNpCommerce2_E3455E27(v29, v33 - v31, 68, 0, &v43);
          if ( v34 >= 0 && v43 != 0 )
            a2[33] = v29 + v34;
          v10 = v34;
          if ( ((v34 != 0x80550F87) & ((unsigned int)v34 >> 31)) == 0 )
          {
            v35 = v41;
            a2[34] = a2[1] + 4;
            v36 = sceNpCommerce2_E3455E27(v29, v35 - v31, 69, 0, &v44);
            if ( v36 >= 0 && v44 != 0 )
              a2[34] = v29 + v36;
            v10 = v36;
            if ( ((v36 != 0x80550F87) & ((unsigned int)v36 >> 31)) == 0 )
            {
              v37 = v41;
              a2[35] = a2[1] + 4;
              v38 = sceNpCommerce2_E3455E27(v29, v37 - v31, 60, 0, &v45);
              if ( v38 >= 0 && v45 != 0 )
                a2[35] = v29 + v38;
              v10 = v38;
              if ( ((v38 != 0x80550F87) & ((unsigned int)v38 >> 31)) == 0 )
              {
                v10 = sceNpCommerce2_3F6D413C(v29, v41 - v31, 61, 0, a2 + 36);
                if ( v10 >= 0 )
                {
                  v10 = sceNpCommerce2_3F6D413C(v29, v41 - v31, 62, 0, a2 + 37);
                  if ( v10 >= 0 )
                  {
                    v10 = sceNpCommerce2_3F6D413C(v29, v41 - v31, v8, 0, a2 + 38);
                    if ( ((v10 != 0x80550F87) & ((unsigned int)v10 >> 31)) == 0 )
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
  return v10;
}


int store_tlv_fn_32(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int *v15;
  int v16;
  s32 v17;
  s16 v18;
  s16 v19;
  s16 v20;
  s16 v21;
  s16 v22;
  s16 v23;
  int v24;
  u32 v25[4];

  result = store_disp_fn_54(a2, a3, &v24, v25, 0);
  if ( result >= 0 )
  {
    v6 = a2 + result;
    sceKernelMemset(a1, 0, 64);
    v7 = v24;
    v8 = v25[0];
    sceKernelMemset(a1, 0, 40);
    v9 = v25[0];
    a1[2] = v6 - a2 + v25[0];
    *a1 = v7;
    a1[4] = v8;
    a1[10] = a2 + 4;
    a1[1] = a2;
    a1[3] = v6;
    result = sceNpCommerce2_E3455E27(v6, v9, 79, 0, &v18);
    if ( result >= 0 && v18 != 0 )
      a1[10] = v6 + result;
    if ( result >= 0 )
    {
      v10 = v25[0];
      a1[11] = a1[1] + 4;
      result = sceNpCommerce2_E3455E27(v6, v10, 80, 0, &v19);
      if ( result >= 0 && v19 != 0 )
        a1[11] = v6 + result;
      if ( result >= 0 )
      {
        v11 = v25[0];
        a1[12] = a1[1] + 4;
        result = sceNpCommerce2_E3455E27(v6, v11, 81, 0, &v20);
        if ( result >= 0 && v20 != 0 )
          a1[12] = v6 + result;
        if ( result >= 0 )
        {
          v12 = v25[0];
          a1[13] = a1[1] + 4;
          result = sceNpCommerce2_E3455E27(v6, v12, 82, 0, &v21);
          if ( result >= 0 && v21 != 0 )
            a1[13] = v6 + result;
          if ( result >= 0 )
          {
            v13 = v25[0];
            a1[14] = a1[1] + 4;
            result = sceNpCommerce2_E3455E27(v6, v13, 54, 0, &v22);
            if ( result >= 0 && v22 != 0 )
              a1[14] = v6 + result;
            if ( result >= 0 )
            {
              v14 = v25[0];
              v15 = a1 + 15;
              v16 = 0;
              if ( a1 != NULL )
                v16 = a1[1] + 4;
              *v15 = v16;
              v17 = sceNpCommerce2_E3455E27(v6, v14, 84, 0, &v23);
              if ( v17 >= 0 && v23 != 0 )
                *v15 = v6 + v17;
              return store_min_s32(v17, 0);
            }
          }
        }
      }
    }
  }
  return result;
}


int store_tlv_fn_33(int *a1, u32 *a2)
{
  int v4;
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int *v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  u16 v17;
  u16 v18[3];
  u32 v19[2];
  u32 v20;
  u32 v21;
  u32 v22;

  v4 = a1[3];
  result = sceNpCommerce2_E3455E27(v4, a1[4], 94, 50, &v17);
  if ( result >= 0 )
  {
    v6 = v4 + result;
    sceKernelMemset(a2, 0, 88);
    v7 = *a1;
    v8 = v17;
    v9 = a1[1];
    sceKernelMemset(a2, 0, 40);
    a2[1] = v9;
    *a2 = v7;
    v10 = v6;
    a2[4] = v8;
    v11 = a2 + 19;
    a2[3] = v6;
    sceKernelMemcpy(v19, v6, 8);
    sceNpCommerce2_22CEC5E2(a2 + 10, 17, byteswap32(v19[1]), byteswap32(v19[0]), 0);
    a2[15] = v6 + 8;
    a2[16] = v6 + 25;
    a2[17] = v6 + 28;
    v12 = v6 + 42;
    sceKernelMemcpy(&v20, v6 + 34, 4);
    v13 = v6 + 38;
    a2[18] = byteswap32(v20);
    v14 = v6 + 46;
    sceKernelMemcpy(&v21, v13, 4);
    a2[20] = byteswap32(v21) != 0;
    sceKernelMemcpy(&v22, v12, 4);
    v15 = v17;
    v16 = 0;
    a2[21] = byteswap32(v22);
    a2[5] = v12 + 4;
    if ( a2 != NULL )
      v16 = a2[1] + 4;
    *v11 = v16;
    result = sceNpCommerce2_E3455E27(v12 + 4, v15 - (v12 + 4 - v10), 93, 0, v18);
    if ( result >= 0 && v18[0] != 0 )
      *v11 = v14 + result;
  }
  return result;
}


int store_tlv_fn_34(int *a1, u32 *a2)
{
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  u32 v14;
  u32 v15;
  int v17;
  u16 v18;
  u32 v19;
  u32 v20;
  u32 v21;
  u32 v22;
  u32 v23;
  u32 v24;
  u32 v25;
  u32 v26;
  u32 v27;
  u32 v28;
  u32 v29;
  u32 v30;
  u32 v31;
  u32 v32;
  u32 v33;

  v4 = a1[3];
  v5 = sceNpCommerce2_E3455E27(v4, a1[4], 95, 44, &v18);
  if ( v5 >= 0 )
  {
    sceKernelMemset(a2, 0, 104);
    v6 = *a1;
    v7 = v18;
    v8 = a1[1];
    sceKernelMemset(a2, 0, 40);
    a2[1] = v8;
    v9 = v4 + v5;
    *a2 = v6;
    a2[4] = v7;
    v10 = v4 + v5 + 4;
    v11 = v4 + v5 + 8;
    a2[3] = v4 + v5;
    v12 = v4 + v5 + 12;
    v13 = v4 + v5 + 16;
    sceKernelMemcpy(&v19, v9, 4);
    a2[10] = byteswap32(v19);
    sceKernelMemcpy(&v20, v10, 4);
    a2[11] = byteswap32(v20);
    sceKernelMemcpy(&v21, v11, 4);
    a2[12] = byteswap32(v21);
    sceKernelMemcpy(&v22, v12, 4);
    v14 = byteswap32(v22);
    a2[13] = v14;
    switch ( v14 )
    {
      case 0u:
        a2[14] = 0;
LABEL_8:
        sceKernelMemcpy(&v23, v13, 4);
        a2[15] = byteswap32(v23);
        sceKernelMemcpy(&v24, v13 + 4, 4);
        a2[16] = byteswap32(v24);
        sceKernelMemcpy(&v25, v13 + 8, 4);
        a2[17] = byteswap32(v25);
        sceKernelMemcpy(&v26, v13 + 12, 4);
        a2[18] = byteswap32(v26);
        sceKernelMemcpy(&v27, v13 + 16, 4);
        a2[19] = byteswap32(v27);
        sceKernelMemcpy(&v28, v13 + 20, 4);
        a2[20] = byteswap32(v28);
        sceKernelMemcpy(&v29, v13 + 24, 4);
        a2[21] = byteswap32(v29);
        sceKernelMemcpy(&v30, v13 + 28, 4);
        a2[22] = byteswap32(v30);
        sceKernelMemcpy(&v31, v13 + 32, 4);
        a2[23] = byteswap32(v31);
        sceKernelMemcpy(&v32, v13 + 36, 4);
        a2[24] = byteswap32(v32);
        sceKernelMemcpy(&v33, v13 + 40, 4);
        v15 = v33;
        a2[5] = v13 + 44;
        a2[25] = byteswap32(v15);
        return v5;
      case 1u:
        v17 = 3;
        break;
      case 4u:
        v17 = 2;
        break;
      case 8u:
        a2[14] = 1;
        goto LABEL_8;
      default:
        goto LABEL_8;
    }
    a2[14] = v17;
    goto LABEL_8;
  }
  return v5;
}


int store_tlv_fn_35(u32 *a1, int a2, int a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  s16 v24;
  s16 v25;
  s16 v26;
  s16 v27;
  s16 v28;
  s16 v29;
  int v30;
  u32 v31[4];

  v5 = store_disp_fn_54(a2, a3, &v30, v31, 0);
  v6 = v5;
  if ( v5 >= 0 )
  {
    v7 = a2 + v5;
    sceKernelMemset(a1, 0, 76);
    v8 = v30;
    v9 = v31[0];
    sceKernelMemset(a1, 0, 40);
    v10 = v31[0];
    *a1 = v8;
    a1[2] = v7 - a2 + v10;
    a1[4] = v9;
    a1[1] = a2;
    a1[3] = v7;
    v6 = sceNpCommerce2_3F6D413C(v7, v10, 55, 0, a1 + 10);
    if ( v6 >= 0 )
    {
      v6 = sceNpCommerce2_3F6D413C(v7, v31[0], 57, 0, a1 + 11);
      if ( v6 >= 0 )
      {
        a1[12] = a1[1] + 4;
        v11 = sceNpCommerce2_E3455E27(v7, v31[0], 79, 0, &v24);
        v6 = v11;
        if ( v11 >= 0 && v24 != 0 )
          a1[12] = v7 + v11;
        v12 = v31[0];
        if ( v11 >= 0 )
        {
          a1[13] = a1[1] + 4;
          v13 = sceNpCommerce2_E3455E27(v7, v12, 80, 0, &v25);
          v6 = v13;
          if ( v13 >= 0 && v25 != 0 )
            a1[13] = v7 + v13;
          v14 = v31[0];
          if ( v13 >= 0 )
          {
            a1[14] = a1[1] + 4;
            v15 = sceNpCommerce2_E3455E27(v7, v14, 81, 0, &v26);
            v6 = v15;
            if ( v15 >= 0 && v26 != 0 )
              a1[14] = v7 + v15;
            v16 = v31[0];
            if ( v15 >= 0 )
            {
              a1[15] = a1[1] + 4;
              v17 = sceNpCommerce2_E3455E27(v7, v16, 82, 0, &v27);
              v6 = v17;
              if ( v17 >= 0 && v27 != 0 )
                a1[15] = v7 + v17;
              v18 = v31[0];
              if ( v17 >= 0 )
              {
                a1[16] = a1[1] + 4;
                v19 = sceNpCommerce2_E3455E27(v7, v18, 54, 0, &v28);
                v6 = v19;
                if ( v19 >= 0 && v28 != 0 )
                  a1[16] = v7 + v19;
                v20 = v31[0];
                if ( v19 >= 0 )
                {
                  a1[17] = a1[1] + 4;
                  v21 = sceNpCommerce2_E3455E27(v7, v20, 84, 0, &v29);
                  v6 = v21;
                  if ( v21 >= 0 && v29 != 0 )
                    a1[17] = v7 + v21;
                  if ( v21 >= 0 )
                  {
                    v22 = sceNpCommerce2_E3455E27(v7, v31[0], 48, 25, 0);
                    v6 = v22;
                    if ( v22 == -2141909113 )
                    {
                      a1[18] = 0;
                      return 0;
                    }
                    if ( v22 >= 0 )
                    {
                      a1[18] = 1;
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
  }
  return v6;
}


int store_tlv_fn_36(u32 *a1, int a2, int a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  s16 v13;
  int v14;
  int v15;

  v5 = store_disp_fn_54(a2, a3, &v14, &v15, 0);
  v6 = v5;
  if ( v5 >= 0 )
  {
    v7 = a2 + v5;
    sceKernelMemset(a1, 0, 56);
    v8 = v14;
    v9 = v15;
    sceKernelMemset(a1, 0, 40);
    v10 = v15;
    a1[2] = v7 - a2 + v15;
    *a1 = v8;
    a1[4] = v9;
    a1[13] = a2 + 4;
    a1[1] = a2;
    a1[3] = v7;
    v11 = sceNpCommerce2_E3455E27(v7, v10, 164, 0, &v13);
    if ( v11 >= 0 && v13 != 0 )
      a1[13] = v7 + v11;
    v6 = v11;
    if ( ((v11 != 0x80550F87) & ((unsigned int)v11 >> 31)) == 0 )
    {
      if ( v11 != -2141909113 )
        return 0;
      v6 = sceNpCommerce2_3F6D413C(v7, v15, 22, 0, a1 + 10);
      if ( v6 >= 0 )
      {
        v6 = sceNpCommerce2_3F6D413C(v7, v15, 100, 0, a1 + 11);
        if ( v6 >= 0 )
        {
          v6 = sceNpCommerce2_3F6D413C(v7, v15, 88, 0, a1 + 12);
          if ( v6 >= 0 )
            return 0;
        }
      }
    }
  }
  return v6;
}


int store_tlv_fn_37(int *a1, int a2, u32 *a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  u32 v14;
  u32 v15;
  u16 v17;
  u32 v18;
  u32 v19;
  u32 v20;
  u32 v21;
  u32 v22[3];

  v5 = a1[3];
  v6 = store_disp_fn_55(v5, a1[4], 116, 12, a2, &v17);
  v7 = v5 + v6;
  if ( v6 >= 0 )
  {
    sceKernelMemset(a3, 0, 64);
    v8 = *a1;
    v9 = v17;
    v10 = a1[1];
    sceKernelMemset(a3, 0, 40);
    a3[1] = v10;
    v11 = v7;
    a3[3] = v7;
    v12 = v7 + 4;
    *a3 = v8;
    v13 = v7 + 8;
    a3[4] = v9;
    sceKernelMemcpy(&v18, v11, 4);
    v14 = byteswap32(v18);
    a3[10] = v14;
    a3[11] = v14;
    if ( v14 - 1 >= 2 )
      a3[10] = 0;
    sceKernelMemcpy(&v19, v12, 4);
    a3[12] = byteswap32(v19);
    sceKernelMemcpy(&v20, v13, 4);
    a3[13] = byteswap32(v20) != 0;
    sceKernelMemcpy(&v21, v13 + 4, 4);
    a3[14] = byteswap32(v21);
    sceKernelMemcpy(v22, v13 + 8, 4);
    v15 = v22[0];
    a3[5] = v13 + 12;
    a3[15] = byteswap32(v15);
  }
  return v6;
}


int store_tlv_fn_38(int a1, int a2, u32 *a3)
{
  int result;
  int v6;
  int v7;
  int v8;

  v8 = 0;
  result = store_disp_fn_54(a1, a2, &v6, &v7, 0);
  if ( result >= 0 )
  {
    *a3 = sceNpCommerce2_3F6D413C(a1 + result, v7, 120, 0, &v8) >= 0;
    return 0;
  }
  return result;
}


s32 store_tlv_fn_39(u32 *a1, int a2, int a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v11;
  u32 v12[3];

  v5 = store_disp_fn_54(a2, a3, &v11, v12, 0);
  v6 = a2 + v5;
  if ( v5 >= 0 )
  {
    sceKernelMemset(a1, 0, 44);
    v7 = v11;
    v8 = v12[0];
    sceKernelMemset(a1, 0, 40);
    v9 = v12[0];
    *a1 = v7;
    a1[2] = v6 - a2 + v9;
    a1[4] = v8;
    a1[1] = a2;
    a1[3] = v6;
    return store_min_s32(sceNpCommerce2_3F6D413C(v6, v9, 120, 0, a1 + 10), 0);
  }
  return v5;
}


int store_tlv_fn_40(int *a1, int a2, u32 *a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11;
  u16 v13;
  u32 v14;
  u32 v15;

  v5 = a1[3];
  v6 = store_disp_fn_55(v5, a1[4], 120, 8, a2, &v13);
  v7 = v5 + v6;
  if ( v6 >= 0 )
  {
    sceKernelMemset(a3, 0, 48);
    v8 = *a1;
    v9 = v13;
    v10 = a1[1];
    sceKernelMemset(a3, 0, 40);
    a3[1] = v10;
    *a3 = v8;
    a3[4] = v9;
    a3[3] = v7;
    sceKernelMemcpy(&v14, v7, 4);
    a3[10] = byteswap32(v14);
    sceKernelMemcpy(&v15, v7 + 4, 4);
    v11 = v15;
    a3[5] = v7 + 8;
    a3[11] = byteswap32(v11);
  }
  return v6;
}


s32 store_tlv_fn_41(u32 *a1, int a2, int a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v11;
  u32 v12[3];

  v5 = store_disp_fn_54(a2, a3, &v11, v12, 0);
  v6 = a2 + v5;
  if ( v5 >= 0 )
  {
    sceKernelMemset(a1, 0, 44);
    v7 = v11;
    v8 = v12[0];
    sceKernelMemset(a1, 0, 40);
    v9 = v12[0];
    *a1 = v7;
    a1[2] = v6 - a2 + v9;
    a1[4] = v8;
    a1[1] = a2;
    a1[3] = v6;
    return store_min_s32(sceNpCommerce2_3F6D413C(v6, v9, 22, 0, a1 + 10), 0);
  }
  return v5;
}


int store_tlv_fn_42()
{
  return store_disp_fn_59();
}


int store_tlv_fn_43(int *a1, u32 *a2)
{
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  u32 v10;
  u16 v12;
  u32 v13[3];

  v4 = a1[3];
  v5 = sceNpCommerce2_E3455E27(v4, a1[4], 87, 4, &v12);
  v6 = v4 + v5;
  if ( v5 >= 0 )
  {
    sceKernelMemset(a2, 0, 44);
    v7 = *a1;
    v8 = v12;
    v9 = a1[1];
    sceKernelMemset(a2, 0, 40);
    a2[1] = v9;
    *a2 = v7;
    a2[4] = v8;
    a2[3] = v6;
    sceKernelMemcpy(v13, v6, 4);
    v10 = v13[0];
    a2[5] = v6 + 4;
    a2[10] = byteswap32(v10) != 0;
  }
  return v5;
}


int store_tlv_fn_44(u32 *a1, int a2, u32 *a3)
{
  int v4;
  int result;
  int v6;
  int v7;

  v4 = a1[5];
  result = store_disp_fn_55(v4, a1[4] - (v4 - a1[3]), 17, 0, a2, &v7);
  v6 = v4 + result;
  if ( result >= 0 )
    *a3 = v6;
  return result;
}


int store_tlv_fn_45(u32 *a1, int a2)
{
  int v4;
  int result;
  u16 v6[8];

  v4 = a1[5];
  result = sceNpCommerce2_E3455E27(v4, a1[4] - (v4 - a1[3]), 33, 81, v6);
  if ( result >= 0 )
    return store_disp_fn_61(v4 + result, v6[0], a1[1], *a1, 0, a2);
  return result;
}


int store_tlv_fn_46(u32 *a1, int a2)
{
  int v4;
  int result;
  u16 v6[8];

  v4 = a1[5];
  result = sceNpCommerce2_E3455E27(v4, a1[4] - (v4 - a1[3]), 24, 12, v6);
  if ( result >= 0 )
    return store_disp_fn_62(v4 + result, v6[0], a1[1], *a1, a2);
  return result;
}


int store_tlv_fn_47(u32 *a1, int a2, int a3)
{
  int v5;
  int result;
  u16 v7[8];

  v5 = a1[5];
  result = store_disp_fn_55(v5, a1[4] - (v5 - a1[3]), 24, 12, a2, v7);
  if ( result >= 0 )
    return store_disp_fn_62(v5 + result, v7[0], a1[1], *a1, a3);
  return result;
}


int store_tlv_fn_48(u32 *a1, int a2)
{
  return store_disp_fn_65(*a1, a1[1], a1[3], a1[4], a1[5], a2);
}


int store_tlv_fn_49(u32 *a1, int a2)
{
  return store_disp_fn_66(*a1, a1[1], a1[3], a1[4], a1[5], a2);
}


int store_tlv_fn_50(u32 *a1, int a2, int a3)
{
  int v5;
  int result;
  u16 v7[8];

  v5 = a1[5];
  result = store_disp_fn_55(v5, a1[4] - (v5 - a1[3]), 24, 12, a2, v7);
  if ( result >= 0 )
    return store_disp_fn_62(v5 + result, v7[0], a1[1], *a1, a3);
  return result;
}


int store_tlv_fn_51(u32 *a1, int a2, u32 *a3)
{
  int v4;
  int result;
  int v6;
  int v7;

  v4 = a1[5];
  result = store_disp_fn_55(v4, a1[4] - (v4 - a1[3]), 35, 0, a2, &v7);
  v6 = v4 + result;
  if ( result >= 0 )
    *a3 = v6;
  return result;
}


int store_tlv_fn_52(int *a1, int a2, u32 *a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int *v15;
  int v16;
  int v17;
  u16 v19;
  s16 v20;
  s16 v21[6];

  v5 = a1[5];
  v6 = store_disp_fn_55(v5, a1[4] - (v5 - a1[3]), 36, 0, a2, &v19);
  v7 = v6;
  if ( v6 >= 0 )
  {
    v8 = v5 + v6;
    sceKernelMemset(a3, 0, 48);
    v9 = a1[1];
    v10 = v19;
    v11 = *a1;
    sceKernelMemset(a3, 0, 40);
    v12 = v19;
    *a3 = v11;
    a3[4] = v10;
    a3[10] = v9 + 4;
    a3[1] = v9;
    a3[3] = v8;
    a3[5] = v8;
    v13 = sceNpCommerce2_E3455E27(v8, v12, 66, 0, &v20);
    if ( v13 >= 0 && v20 != 0 )
      a3[10] = v8 + v13;
    v7 = v13;
    if ( v13 >= 0 )
    {
      v14 = v19;
      v15 = a3 + 11;
      v16 = 0;
      if ( a3 != NULL )
        v16 = a3[1] + 4;
      *v15 = v16;
      v17 = sceNpCommerce2_E3455E27(v8, v14, 67, 0, v21);
      if ( v17 >= 0 && v21[0] != 0 )
        *v15 = v8 + v17;
      v7 = v17;
      if ( ((v17 != 0x80550F87) & ((unsigned int)v17 >> 31)) == 0 )
        return 0;
    }
  }
  return v7;
}


int store_tlv_fn_53(int *a1, int a2, u32 *a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u16 v12[8];

  v5 = a1[5];
  v6 = store_disp_fn_55(v5, a1[4] - (v5 - a1[3]), 150, 0, a2, v12);
  v7 = v5 + v6;
  if ( v6 >= 0 )
  {
    sceKernelMemset(a3, 0, 48);
    v8 = *a1;
    v9 = v12[0];
    v10 = a1[1];
    sceKernelMemset(a3, 0, 40);
    a3[3] = v7;
    a3[10] = v7;
    a3[11] = v7 + 57;
    a3[1] = v10;
    *a3 = v8;
    a3[4] = v9;
  }
  return v6;
}


int store_tlv_fn_54(u32 *a1, int a2, int a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v14;
  int v15;
  int v16;
  u32 v17;
  u32 v18[4];

  v5 = store_disp_fn_54(a2, a3, &v14, &v15, &v16);
  v6 = a2 + v5;
  if ( v5 >= 0 )
  {
    sceKernelMemset(a1, 0, 48);
    v7 = v14;
    v8 = v15;
    sceKernelMemset(a1, 0, 40);
    v9 = v15;
    *a1 = v7;
    a1[4] = v8;
    v10 = v16;
    a1[2] = v6 - a2 + v9;
    a1[6] = v10;
    a1[1] = a2;
    a1[3] = v6;
    v11 = sceNpCommerce2_E3455E27(v6, v9, 45, 4, 0);
    if ( v11 >= 0 )
    {
      sceKernelMemcpy(&v17, v6 + v11, 4);
      v17 = byteswap32(v17);
      sceKernelMemcpy(a1 + 10, &v17, 4);
    }
    v5 = v11;
    if ( v11 >= 0 )
    {
      v12 = sceNpCommerce2_E3455E27(v6, v15, 21, 4, 0);
      if ( v12 >= 0 )
      {
        sceKernelMemcpy(v18, v6 + v12, 4);
        a1[11] = byteswap32(v18[0]);
      }
      v5 = v12;
      if ( ((v12 != 0x80550F87) & ((unsigned int)v12 >> 31)) == 0 )
        return 0;
    }
  }
  return v5;
}


