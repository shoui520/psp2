#include "store_internal.h"

int store_disp_fn_1()
{
  return store_disp_fn_67();
}


int store_disp_fn_2()
{
  return store_disp_fn_68();
}


int store_disp_fn_3(int *a1, int a2, int a3, u32 *a4)
{
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  u32 v13;
  u16 v15;
  u32 v16[3];

  v6 = a1[5];
  if ( a2 == 1 )
    v7 = store_disp_fn_55(v6, a1[4] - (v6 - a1[3]), 78, 105, a3, &v15);
  else
    v7 = sceNpCommerce2_E3455E27(v6, a1[4] - (v6 - a1[3]), 78, 105, &v15);
  v8 = v7;
  v9 = v6 + v7;
  if ( v7 >= 0 )
  {
    sceKernelMemset(a4, 0, 44);
    v10 = *a1;
    v11 = v15;
    v12 = a1[1];
    sceKernelMemset(a4, 0, 40);
    a4[1] = v12;
    *a4 = v10;
    a4[4] = v11;
    a4[3] = v9;
    sceKernelMemcpy(v16, v9, 4);
    v13 = v16[0];
    a4[5] = v9 + 4;
    a4[10] = byteswap32(v13);
  }
  return v8;
}


int store_disp_fn_4(u32 *a1, int a2, u32 *a3)
{
  int v4;
  int result;
  int v6;
  int v7;

  v4 = a1[5];
  result = store_disp_fn_55(v4, a1[4] - (v4 - a1[3]), 62, 0, a2, &v7);
  v6 = v4 + result;
  if ( result >= 0 )
    *a3 = v6;
  return result;
}


int store_disp_fn_5(u32 *a1, int a2, int a3)
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


int store_disp_fn_6()
{
  return store_disp_fn_67();
}


int store_disp_fn_7()
{
  return store_disp_fn_68();
}


int store_disp_fn_8(u32 *a1, int a2, int a3)
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


int store_disp_fn_9()
{
  return store_disp_fn_67();
}


int store_disp_fn_10()
{
  return store_disp_fn_68();
}


int store_disp_fn_11(u32 *a1, int a2, int a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v13;
  int v14;
  u32 v15;
  u32 v16;

  v5 = store_disp_fn_54(a2, a3, &v13, &v14, 0);
  v6 = a2 + v5;
  if ( v5 >= 0 )
  {
    sceKernelMemset(a1, 0, 48);
    v7 = v13;
    v8 = v14;
    sceKernelMemset(a1, 0, 40);
    v9 = v14;
    *a1 = v7;
    a1[4] = v8;
    a1[2] = v6 - a2 + v9;
    a1[1] = a2;
    a1[3] = v6;
    v10 = sceNpCommerce2_E3455E27(v6, v9, 45, 4, 0);
    if ( v10 >= 0 )
    {
      sceKernelMemcpy(&v15, v6 + v10, 4);
      v15 = byteswap32(v15);
      sceKernelMemcpy(a1 + 10, &v15, 4);
    }
    v5 = v10;
    if ( ((v10 != 0x80550F87) & ((unsigned int)v10 >> 31)) == 0 )
    {
      v11 = sceNpCommerce2_E3455E27(v6, v14, 21, 4, 0);
      if ( v11 >= 0 )
      {
        sceKernelMemcpy(&v16, v6 + v11, 4);
        a1[11] = byteswap32(v16);
      }
      v5 = v11;
      if ( ((v11 != 0x80550F87) & ((unsigned int)v11 >> 31)) == 0 )
        return 0;
    }
  }
  return v5;
}


int store_disp_fn_12()
{
  return store_disp_fn_67();
}


int store_disp_fn_13(int a1, int a2)
{
  return store_disp_fn_69(a1, *(u32 *)(a1 + 12), *(u32 *)(a1 + 16), a2);
}


int store_disp_fn_14()
{
  return store_disp_fn_68();
}


int store_disp_fn_15(u32 *a1, int a2, int a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 v12;
  int v14;
  int v15;
  u32 v16;
  u32 v17[4];

  v5 = store_disp_fn_54(a2, a3, &v14, &v15, 0);
  v6 = a2 + v5;
  if ( v5 >= 0 )
  {
    sceKernelMemset(a1, 0, 56);
    v7 = v14;
    v8 = v15;
    sceKernelMemset(a1, 0, 40);
    v9 = v15;
    *a1 = v7;
    a1[4] = v8;
    a1[2] = v6 - a2 + v9;
    a1[1] = a2;
    a1[3] = v6;
    v10 = sceNpCommerce2_E3455E27(v6, v9, 21, 4, 0);
    if ( v10 >= 0 )
    {
      sceKernelMemcpy(&v16, v6 + v10, 4);
      a1[10] = byteswap32(v16);
    }
    v5 = v10;
    if ( ((v10 != 0x80550F87) & ((unsigned int)v10 >> 31)) == 0 )
    {
      v11 = sceNpCommerce2_E3455E27(v6, v15, 3, 8, 0);
      if ( v11 >= 0 )
      {
        sceKernelMemcpy(v17, v6 + v11, 8);
        v12 = byteswap32(v17[0]);
        a1[12] = byteswap32(v17[1]);
        a1[13] = v12;
      }
      v5 = v11;
      if ( ((v11 != 0x80550F87) & ((unsigned int)v11 >> 31)) == 0 )
        return 0;
    }
  }
  return v5;
}


int store_disp_fn_16()
{
  return store_disp_fn_67();
}


int store_disp_fn_17()
{
  return store_disp_fn_68();
}


int store_disp_fn_18(int a1, int a2)
{
  return store_disp_fn_69(a1, *(u32 *)(a1 + 12), *(u32 *)(a1 + 16), a2);
}


int store_disp_fn_19(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11[3];

  result = store_disp_fn_54(a2, a3, &v10, v11, 0);
  v6 = a2 + result;
  if ( result >= 0 )
  {
    sceKernelMemset(a1, 0, 40);
    v7 = v10;
    v8 = v11[0];
    sceKernelMemset(a1, 0, 40);
    *a1 = v7;
    v9 = v11[0];
    a1[1] = a2;
    a1[2] = v6 - a2 + v9;
    a1[4] = v8;
    a1[3] = v6;
    return 0;
  }
  return result;
}


int store_disp_fn_20(int *a1, u32 *a2)
{
  return store_tlv_fn_23(a1, a2);
}


s32 store_disp_fn_21(u32 *a1, int a2, int a3)
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
    return store_min_s32(sceNpCommerce2_3F6D413C(v6, v9, 55, 0, a1 + 10), 0);
  }
  return v5;
}


int store_disp_fn_22()
{
  return store_disp_fn_70();
}


int store_disp_fn_23(int *a1, int a2, u32 *a3)
{
  int v5;
  int result;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  u16 v15;
  u16 v16[7];

  v5 = a1[5];
  result = store_disp_fn_55(v5, a1[4] - (v5 - a1[3]), 59, 69, a2, &v15);
  v7 = v5 + result;
  v8 = v5 + result;
  if ( result >= 0 )
  {
    sceKernelMemset(a3, 0, 48);
    v9 = a1[1];
    v10 = v15;
    v11 = *a1;
    sceKernelMemset(a3, 0, 40);
    v12 = v9 + 4;
    v13 = v15;
    a3[3] = v7;
    if ( a3 == NULL )
      v12 = 0;
    a3[10] = v7;
    v14 = v7 + 65;
    a3[11] = v12;
    *a3 = v11;
    a3[4] = v10;
    a3[1] = v9;
    a3[5] = v14;
    result = sceNpCommerce2_E3455E27(v14, v13 - (v14 - v8), 58, 0, v16);
    if ( result >= 0 && v16[0] != 0 )
      a3[11] = v14 + result;
  }
  return result;
}


s32 store_disp_fn_24(u32 *a1, int a2, int a3)
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
    return store_min_s32(sceNpCommerce2_3F6D413C(v6, v9, 57, 0, a1 + 10), 0);
  }
  return v5;
}


int store_disp_fn_25()
{
  return store_disp_fn_71();
}


s32 store_disp_fn_26(u32 *a1, int a2, int a3)
{
  u32 *v4;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v13;
  int v14;
  int v15;

  v4 = a1 + 10;
  v6 = store_disp_fn_54(a2, a3, &v13, &v14, &v15);
  v7 = a2 + v6;
  if ( v6 >= 0 )
  {
    sceKernelMemset(a1, 0, 52);
    v8 = v13;
    v9 = v14;
    sceKernelMemset(a1, 0, 40);
    v10 = v14;
    v11 = v15;
    a1[2] = v7 - a2 + v14;
    *a1 = v8;
    a1[4] = v9;
    a1[1] = a2;
    a1[3] = v7;
    a1[6] = v11;
    v6 = sceNpCommerce2_69FEA1C8(v7, v10, 43, 0, v4, v11);
    if ( v6 >= 0 )
      return store_min_s32(store_disp_fn_58(a1, 43, 125, v4), 0);
  }
  return v6;
}


int store_disp_fn_27(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int *v12;
  int v13;
  s32 v14;
  s16 v15;
  s16 v16;
  s16 v17;
  int v18;
  int v19;

  result = store_disp_fn_54(a2, a3, &v18, &v19, 0);
  if ( result >= 0 )
  {
    v6 = a2 + result;
    sceKernelMemset(a1, 0, 52);
    v7 = v18;
    v8 = v19;
    sceKernelMemset(a1, 0, 40);
    v9 = v19;
    a1[2] = v6 - a2 + v19;
    *a1 = v7;
    a1[4] = v8;
    a1[10] = a2 + 4;
    a1[1] = a2;
    a1[3] = v6;
    result = sceNpCommerce2_E3455E27(v6, v9, 79, 0, &v15);
    if ( result >= 0 && v15 != 0 )
      a1[10] = v6 + result;
    if ( result >= 0 )
    {
      v10 = v19;
      a1[11] = a1[1] + 4;
      result = sceNpCommerce2_E3455E27(v6, v10, 82, 0, &v16);
      if ( result >= 0 && v16 != 0 )
        a1[11] = v6 + result;
      if ( result >= 0 )
      {
        v11 = v19;
        v12 = a1 + 12;
        v13 = 0;
        if ( a1 != NULL )
          v13 = a1[1] + 4;
        *v12 = v13;
        v14 = sceNpCommerce2_E3455E27(v6, v11, 54, 0, &v17);
        if ( v14 >= 0 && v17 != 0 )
          *v12 = v6 + v14;
        return store_min_s32(v14, 0);
      }
    }
  }
  return result;
}


int store_disp_fn_28(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11[3];

  result = store_disp_fn_54(a2, a3, &v10, v11, 0);
  v6 = a2 + result;
  if ( result >= 0 )
  {
    sceKernelMemset(a1, 0, 40);
    v7 = v10;
    v8 = v11[0];
    sceKernelMemset(a1, 0, 40);
    *a1 = v7;
    v9 = v11[0];
    a1[1] = a2;
    a1[2] = v6 - a2 + v9;
    a1[4] = v8;
    a1[3] = v6;
    return 0;
  }
  return result;
}


int store_disp_fn_29()
{
  return store_disp_fn_67();
}


int store_disp_fn_30(u32 *a1, int a2, int a3)
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
  s32 v15;
  u32 v16;
  int v17;
  int v18;
  u32 v19;
  u32 v20;
  u32 v21[4];

  result = store_disp_fn_54(a2, a3, &v17, &v18, 0);
  if ( result >= 0 )
  {
    v6 = a2 + result;
    sceKernelMemset(a1, 0, 56);
    v7 = v17;
    v8 = v18;
    sceKernelMemset(a1, 0, 40);
    v9 = v18;
    *a1 = v7;
    a1[4] = v8;
    a1[2] = v6 - a2 + v9;
    a1[1] = a2;
    a1[3] = v6;
    v10 = sceNpCommerce2_E3455E27(v6, v9, 21, 4, 0);
    v11 = v10;
    if ( v10 >= 0 )
    {
      sceKernelMemcpy(&v19, v6 + v10, 4);
      a1[10] = byteswap32(v19);
    }
    result = v11;
    if ( v11 >= 0 )
    {
      v12 = sceNpCommerce2_E3455E27(v6, v18, 45, 4, 0);
      v13 = v12;
      if ( v12 >= 0 )
      {
        sceKernelMemcpy(&v20, v6 + v12, 4);
        v20 = byteswap32(v20);
        sceKernelMemcpy(a1 + 11, &v20, 4);
      }
      result = v13;
      if ( v13 >= 0 )
      {
        v14 = sceNpCommerce2_E3455E27(v6, v18, 3, 8, 0);
        v15 = v14;
        if ( v14 >= 0 )
        {
          sceKernelMemcpy(v21, v6 + v14, 8);
          v16 = byteswap32(v21[0]);
          a1[12] = byteswap32(v21[1]);
          a1[13] = v16;
        }
        return store_min_s32(v15, 0);
      }
    }
  }
  return result;
}


int store_disp_fn_31(int *a1, u32 *a2)
{
  return store_tlv_fn_23(a1, a2);
}


int store_disp_fn_32(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11[3];

  result = store_disp_fn_54(a2, a3, &v10, v11, 0);
  v6 = a2 + result;
  if ( result >= 0 )
  {
    sceKernelMemset(a1, 0, 40);
    v7 = v10;
    v8 = v11[0];
    sceKernelMemset(a1, 0, 40);
    *a1 = v7;
    v9 = v11[0];
    a1[1] = a2;
    a1[2] = v6 - a2 + v9;
    a1[4] = v8;
    a1[3] = v6;
    return 0;
  }
  return result;
}


int store_disp_fn_33(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  s32 v10;
  int v11;
  int v12;
  u32 v13;

  result = store_disp_fn_54(a2, a3, &v11, &v12, 0);
  v6 = a2 + result;
  if ( result >= 0 )
  {
    sceKernelMemset(a1, 0, 44);
    v7 = v11;
    v8 = v12;
    sceKernelMemset(a1, 0, 40);
    v9 = v12;
    *a1 = v7;
    a1[4] = v8;
    a1[2] = v6 - a2 + v9;
    a1[1] = a2;
    a1[3] = v6;
    v10 = sceNpCommerce2_E3455E27(v6, v9, 49, 4, 0);
    if ( v10 >= 0 )
    {
      sceKernelMemcpy(&v13, v6 + v10, 4);
      a1[10] = byteswap32(v13);
    }
    return store_min_s32(v10, 0);
  }
  return result;
}


int store_disp_fn_34()
{
  return store_disp_fn_72();
}


int store_disp_fn_35(int *a1, u32 *a2)
{
  int v4;
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u16 v11[8];

  v4 = a1[3];
  result = sceNpCommerce2_E3455E27(v4, a1[4], 51, 4, v11);
  v6 = v4 + result;
  if ( result >= 0 )
  {
    sceKernelMemset(a2, 0, 44);
    v7 = *a1;
    v8 = v11[0];
    v9 = a1[1];
    sceKernelMemset(a2, 0, 40);
    v10 = v11[0];
    a2[1] = v9;
    *a2 = v7;
    a2[4] = v8;
    a2[3] = v6;
    a2[5] = v6;
    return sceNpCommerce2_3F6D413C(v6, v10, 122, 0, a2 + 10);
  }
  return result;
}


int store_disp_fn_36(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  s32 v11;
  u32 v12;
  int v13;
  int v14;
  u32 v15;
  u32 v16[4];

  result = store_disp_fn_54(a2, a3, &v13, &v14, 0);
  v6 = a2 + result;
  if ( result >= 0 )
  {
    sceKernelMemset(a1, 0, 56);
    v7 = v13;
    v8 = v14;
    sceKernelMemset(a1, 0, 40);
    v9 = v14;
    *a1 = v7;
    a1[4] = v8;
    a1[2] = v6 - a2 + v9;
    a1[1] = a2;
    a1[3] = v6;
    v10 = sceNpCommerce2_E3455E27(v6, v9, 49, 4, 0);
    if ( v10 >= 0 )
    {
      sceKernelMemcpy(&v15, v6 + v10, 4);
      a1[10] = byteswap32(v15);
    }
    result = v10;
    if ( v10 >= 0 )
    {
      v11 = sceNpCommerce2_E3455E27(v6, v14, 3, 8, 0);
      if ( v11 >= 0 )
      {
        sceKernelMemcpy(v16, v6 + v11, 8);
        v12 = byteswap32(v16[0]);
        a1[12] = byteswap32(v16[1]);
        a1[13] = v12;
      }
      return store_min_s32(v11, 0);
    }
  }
  return result;
}


int store_disp_fn_37()
{
  return store_disp_fn_72();
}


int store_disp_fn_38(int *a1, u32 *a2)
{
  int v4;
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u16 v11[8];

  v4 = a1[3];
  result = sceNpCommerce2_E3455E27(v4, a1[4], 51, 4, v11);
  v6 = v4 + result;
  if ( result >= 0 )
  {
    sceKernelMemset(a2, 0, 44);
    v7 = *a1;
    v8 = v11[0];
    v9 = a1[1];
    sceKernelMemset(a2, 0, 40);
    v10 = v11[0];
    a2[1] = v9;
    *a2 = v7;
    a2[4] = v8;
    a2[3] = v6;
    a2[5] = v6;
    return sceNpCommerce2_3F6D413C(v6, v10, 122, 0, a2 + 10);
  }
  return result;
}


s32 store_disp_fn_39(u32 *a1, int a2, int a3)
{
  u32 *v4;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v13;
  int v14;
  int v15;

  v4 = a1 + 10;
  v6 = store_disp_fn_54(a2, a3, &v13, &v14, &v15);
  v7 = a2 + v6;
  if ( v6 >= 0 )
  {
    sceKernelMemset(a1, 0, 52);
    v8 = v13;
    v9 = v14;
    sceKernelMemset(a1, 0, 40);
    v10 = v14;
    v11 = v15;
    a1[2] = v7 - a2 + v14;
    *a1 = v8;
    a1[4] = v9;
    a1[1] = a2;
    a1[3] = v7;
    a1[6] = v11;
    v6 = sceNpCommerce2_69FEA1C8(v7, v10, 40, 0, v4, v11);
    if ( v6 >= 0 )
      return store_min_s32(store_disp_fn_58(a1, 40, 4, v4), 0);
  }
  return v6;
}


int store_disp_fn_40(int *a1, int a2, u32 *a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11;
  u16 v13;
  u32 v14[3];

  v5 = a1[3];
  v6 = store_disp_fn_57(v5, a1[4], 40, 4, a2, &v13, a1[6]);
  v7 = v5 + v6;
  if ( v6 >= 0 )
  {
    sceKernelMemset(a3, 0, 44);
    v8 = *a1;
    v9 = v13;
    v10 = a1[1];
    sceKernelMemset(a3, 0, 40);
    a3[1] = v10;
    *a3 = v8;
    a3[4] = v9;
    a3[3] = v7;
    sceKernelMemcpy(v14, v7, 4);
    v11 = v14[0];
    a3[5] = v7 + 4;
    a3[10] = byteswap32(v11);
  }
  return v6;
}


int store_disp_fn_41(u32 *a1, int a2, u32 *a3)
{
  int v4;
  int result;
  int v6;
  int v7;

  v4 = a1[5];
  result = store_disp_fn_55(v4, a1[4] - (v4 - a1[3]), 92, 0, a2, &v7);
  v6 = v4 + result;
  if ( result >= 0 )
    *a3 = v6;
  return result;
}


int store_disp_fn_42(int *a1, int a2, u32 *a3)
{
  int v5;
  int result;
  int v7;
  int v8;
  int v9;
  int v10;
  u16 v11[8];

  v5 = a1[5];
  result = store_disp_fn_55(v5, a1[4] - (v5 - a1[3]), 155, 71, a2, v11);
  v7 = v5 + result;
  if ( result >= 0 )
  {
    v8 = *a1;
    v9 = v11[0];
    v10 = a1[1];
    sceKernelMemset(a3, 0, 48);
    sceKernelMemset(a3, 0, 40);
    a3[5] = v7 + 71;
    a3[1] = v10;
    *a3 = v8;
    a3[4] = v9;
    a3[3] = v7;
    a3[10] = v7;
    a3[11] = v7 + 6;
    return 0;
  }
  return result;
}


int store_disp_fn_43(int *a1, u32 *a2)
{
  int v4;
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  u16 v10[8];

  v4 = a1[5];
  result = sceNpCommerce2_E3455E27(v4, a1[4] - (v4 - a1[3]), 155, 71, v10);
  v6 = v4 + result;
  if ( result >= 0 )
  {
    v7 = *a1;
    v8 = v10[0];
    v9 = a1[1];
    sceKernelMemset(a2, 0, 48);
    sceKernelMemset(a2, 0, 40);
    a2[5] = v6 + 71;
    a2[1] = v9;
    *a2 = v7;
    a2[4] = v8;
    a2[3] = v6;
    a2[10] = v6;
    a2[11] = v6 + 6;
    return 0;
  }
  return result;
}


int store_disp_fn_44()
{
  return store_disp_fn_70();
}


int store_disp_fn_45()
{
  return store_disp_fn_71();
}


int store_disp_fn_46(int *a1, u32 *a2)
{
  return store_tlv_fn_23(a1, a2);
}


s32 store_disp_fn_47(u32 *a1, int a2, int a3)
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
    return store_min_s32(sceNpCommerce2_3F6D413C(v6, v9, 116, 0, a1 + 10), 0);
  }
  return v5;
}


int store_disp_fn_48(u32 *a1, int a2, int a3)
{
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  s16 v13;
  int v14;
  int v15;

  int v_r1 = store_disp_fn_54(a2, a3, &v14, &v15, 0);
  v6 = a2 + v_r1;
  if ( v_r1 >= 0 )
  {
    sceKernelMemset(a1, 0, 44);
    v7 = v14;
    v8 = v15;
    sceKernelMemset(a1, 0, 40);
    v9 = v15;
    v10 = a2 + 4;
    if ( a1 == NULL )
      v10 = 0;
    a1[2] = v6 - a2 + v15;
    *a1 = v7;
    a1[4] = v8;
    a1[10] = v10;
    a1[1] = a2;
    a1[3] = v6;
    int v_r0 = sceNpCommerce2_E3455E27(v6, v9, 118, 0, &v13);
    v_r1 = store_min_s32(v_r0, 0);
    if ( v_r0 >= 0 && v13 != 0 )
      a1[10] = v6 + v_r0;
  }
  return v_r1;
}


s32 store_disp_fn_49(u32 *a1, int a2, int a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v12;
  int v13;
  int v14;

  v5 = store_disp_fn_54(a2, a3, &v12, &v13, &v14);
  v6 = a2 + v5;
  if ( v5 >= 0 )
  {
    sceKernelMemset(a1, 0, 44);
    v7 = v12;
    v8 = v13;
    sceKernelMemset(a1, 0, 40);
    v9 = v13;
    v10 = v14;
    a1[2] = v6 - a2 + v13;
    *a1 = v7;
    a1[4] = v8;
    a1[1] = a2;
    a1[3] = v6;
    a1[6] = v10;
    return store_min_s32(sceNpCommerce2_3546C737(v6, v9, 40, 0, a1 + 10, v10), 0);
  }
  return v5;
}


int store_disp_fn_50(int *a1, int a2, u32 *a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11;
  u16 v13;
  u32 v14[3];

  v5 = a1[3];
  v6 = store_disp_fn_56(v5, a1[4], 40, 4, a2, &v13, a1[6]);
  v7 = v5 + v6;
  if ( v6 >= 0 )
  {
    sceKernelMemset(a3, 0, 44);
    v8 = *a1;
    v9 = v13;
    v10 = a1[1];
    sceKernelMemset(a3, 0, 40);
    a3[1] = v10;
    *a3 = v8;
    a3[4] = v9;
    a3[3] = v7;
    sceKernelMemcpy(v14, v7, 4);
    v11 = v14[0];
    a3[5] = v7 + 4;
    a3[10] = byteswap32(v11);
  }
  return v6;
}


s32 store_disp_fn_51(u32 *a1, int a2, int a3)
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
  int v16;

  v5 = store_disp_fn_54(a2, a3, &v14, &v15, &v16);
  v6 = a2 + v5;
  if ( v5 >= 0 )
  {
    sceKernelMemset(a1, 0, 56);
    v7 = v14;
    v8 = v15;
    sceKernelMemset(a1, 0, 40);
    v9 = v15;
    *a1 = v7;
    a1[4] = v8;
    v10 = v16;
    a1[2] = v6 - a2 + v9;
    a1[13] = a2 + 4;
    a1[6] = v10;
    a1[1] = a2;
    a1[3] = v6;
    v11 = sceNpCommerce2_E3455E27(v6, v9, 148, 0, &v13);
    v5 = v11;
    if ( v11 >= 0 && v13 != 0 )
      a1[13] = v6 + v11;
    if ( ((v11 != 0x80550F87) & ((unsigned int)v11 >> 31)) == 0 )
    {
      v5 = sceNpCommerce2_69FEA1C8(v6, v15, 40, 0, a1 + 10, v16);
      if ( v5 >= 0 )
        return store_min_s32(store_disp_fn_58(a1, 40, 4, a1 + 10), 0);
    }
  }
  return v5;
}


s32 store_disp_fn_52(u32 *a1, int a2, int a3)
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
    return store_min_s32(sceNpCommerce2_3F6D413C(v6, v9, 56, 0, a1 + 10), 0);
  }
  return v5;
}


int store_disp_fn_53(int a1, int a2, u32 *a3)
{
  int v4;
  int result;
  int v6;
  int v7;

  v4 = *(u32 *)(a1 + 12);
  result = store_disp_fn_55(v4, *(u32 *)(a1 + 16), 56, 0, a2, &v7);
  v6 = v4 + result;
  if ( result >= 0 )
    *a3 = v6;
  return result;
}


int store_disp_fn_54(int a1, unsigned int a2, u32 *a3, u32 *a4, u32 *a5)
{
  int v5;
  int v12;
  int v13;
  int v14;
  u8 v15[16];

  v5 = -2141909228;
  if ( a2 >= 0x3C )
  {
    sceKernelMemcpy(&v12, a1, 56);
    sceKernelMemcpy(v15, a1 + 56, 4);
    v5 = -2141909228;
    if ( a2 >= v13 + 56 )
    {
      *a4 = v13 - 4;
      v5 = 60;
      if ( a3 != NULL )
        *a3 = v12;
      if ( a5 != NULL )
        *a5 = v14;
    }
  }
  return v5;
}


int store_disp_fn_55(
        int a1,
        int a2,
        unsigned int a3,
        unsigned int a4,
        unsigned int a5,
        u16 *a6)
{
  int v6;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  unsigned int v15;
  int v16;
  u16 v18;
  u16 v19;
  u32 v20[3];

  v6 = a3;
  v10 = a4;
  v11 = sceNpCommerce2_3F6D413C(a1, a2, a3, &v19, v20);
  v12 = v11;
  if ( v11 >= 0 )
  {
    v13 = a1 + v11;
    v14 = a1 + v11;
    v12 = -2141909242;
    if ( a5 < v20[0] )
    {
      v15 = 0;
      v16 = v19;
      while ( 1 )
      {
        if ( v15 != 0 )
          v13 += v18;
        v12 = sceNpCommerce2_E3455E27(v13, v16 - (v13 - v14), v6, v10, &v18);
        ++v15;
        v13 += v12;
        if ( v12 < 0 )
          break;
        v16 = v19;
        if ( a5 < v15 )
        {
          v12 = v13 - a1;
          *a6 = v18;
          return v12;
        }
      }
    }
  }
  return v12;
}


int store_disp_fn_56(
        int a1,
        int a2,
        unsigned int a3,
        unsigned int a4,
        unsigned int a5,
        u16 *a6,
        int a7)
{
  int v7;
  int v10;
  int v13;
  int v14;
  int v15;
  unsigned int v16;
  int v17;
  u16 v19;
  u32 v20[3];

  v7 = a3;
  v10 = a4;
  v13 = sceNpCommerce2_3546C737(a1, a2, a3, 0, v20, a7);
  v14 = v13;
  if ( v13 >= 0 )
  {
    v15 = a1 + v13;
    v14 = -2141909242;
    if ( a5 < v20[0] )
    {
      v16 = 0;
      v17 = v7;
      while ( 1 )
      {
        if ( v16 != 0 )
          v15 += v19;
        v14 = sceNpCommerce2_E3455E27(v15, a2 - (v15 - a1), v17, v10, &v19);
        ++v16;
        v15 += v14;
        if ( v14 < 0 )
          break;
        v17 = v7;
        if ( a5 < v16 )
        {
          v14 = v15 - a1;
          *a6 = v19;
          return v14;
        }
      }
    }
  }
  return v14;
}


int store_disp_fn_57(int a1, int a2, unsigned int a3, unsigned int a4, unsigned int a5, u16 *a6, int a7)
{
  int v7;
  int v10;
  int v13;
  int v14;
  int v15;
  unsigned int v16;
  int v17;
  u32 v19[4];
  u16 v20[8];

  v7 = a3;
  v10 = a4;
  v13 = sceNpCommerce2_69FEA1C8(a1, a2, a3, 0, v19, a7);
  v14 = v13;
  if ( v13 >= 0 )
  {
    v15 = a1 + v13;
    v14 = -2141909242;
    if ( a5 < v19[1] )
    {
      v16 = 0;
      v17 = v7;
      while ( 1 )
      {
        if ( v16 != 0 )
          v15 += v20[0];
        v14 = sceNpCommerce2_E3455E27(v15, a2 - (v15 - a1), v17, v10, v20);
        ++v16;
        v15 += v14;
        if ( v14 < 0 )
          break;
        v17 = v7;
        if ( a5 < v16 )
        {
          v14 = v15 - a1;
          *a6 = v20[0];
          return v14;
        }
      }
    }
  }
  return v14;
}


int store_disp_fn_58(u32 *a1, unsigned int a2, unsigned int a3, int a4)
{
  int v4;
  int v5;
  unsigned int v9;
  int i;
  u16 v13[8];

  v4 = 0;
  v5 = 0;
  v9 = 0;
  if ( *(u32 *)(a4 + 4) == 0 )
  {
LABEL_2:
    if ( v5 == 1 )
      *(u32 *)(a4 + 4) = v9;
    return v4;
  }
  for ( i = a1[3]; ; i = a1[3] )
  {
    v4 = store_disp_fn_57(i, a1[4], a2, a3, v9, v13, a1[6]);
    if ( v4 < 0 )
      break;
    if ( ++v9 >= *(u32 *)(a4 + 4) )
      goto LABEL_2;
  }
  if ( v9 != 0 )
  {
    v4 = 0;
    v5 = 1;
    goto LABEL_2;
  }
  return v4;
}


int store_disp_fn_59(int *a1, unsigned int a2, int a3)
{
  int v5;
  int result;
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
  int *v19;
  int v20;
  int v21;
  u16 v22;
  u16 v23;
  s16 v24;
  s16 v25;
  u32 v26;
  u32 v27;
  u32 v28;
  u32 v29;
  u32 v30;

  v5 = a1[3];
  result = store_disp_fn_55(v5, a1[4], 0x16u, 0x2Au, a2, &v22);
  if ( result >= 0 )
  {
    v7 = v5 + result;
    sceKernelMemset(a3, 0, 84);
    v8 = *a1;
    v9 = v22;
    v10 = a1[1];
    sceKernelMemset(a3, 0, 40);
    *(u32 *)(a3 + 44) = v7 + 4;
    *(u32 *)(a3 + 12) = v7;
    *(u32 *)(a3 + 40) = v7;
    *(u32 *)(a3 + 4) = v10;
    *(u32 *)a3 = v8;
    *(u32 *)(a3 + 16) = v9;
    sceKernelMemcpy(&v23, v7 + 8, 2);
    *(u16 *)(a3 + 48) = _wsbh(v23);
    v11 = v7 + 18;
    sceKernelMemcpy(&v26, v7 + 10, 4);
    *(u32 *)(a3 + 52) = byteswap32(v26);
    v12 = v7 + 22;
    sceKernelMemcpy(&v27, v7 + 14, 4);
    *(u32 *)(a3 + 56) = byteswap32(v27);
    v13 = v7 + 26;
    sceKernelMemcpy(&v28, v7 + 18, 4);
    *(u32 *)(a3 + 64) = byteswap32(v28);
    v14 = v7 + 30;
    sceKernelMemcpy(&v29, v11 + 4, 4);
    *(u32 *)(a3 + 68) = byteswap32(v29);
    sceKernelMemcpy(&v30, v12 + 4, 4);
    v15 = *(u32 *)(a3 + 12);
    v16 = v22;
    *(u32 *)(a3 + 72) = byteswap32(v30);
    *(u32 *)(a3 + 20) = v14;
    result = sceNpCommerce2_3F6D413C(v13 + 4, v16 - (v14 - v15), 21, 0, a3 + 60);
    v17 = v22;
    if ( result >= 0 )
    {
      v18 = *(u32 *)(a3 + 12);
      *(u32 *)(a3 + 76) = *(u32 *)(a3 + 4) + 4;
      result = sceNpCommerce2_E3455E27(v13 + 4, v17 - (v14 - v18), 125, 0, &v24);
      if ( result >= 0 && v24 != 0 )
        *(u32 *)(a3 + 76) = v14 + result;
      if ( result >= 0 )
      {
        v19 = (int *)(a3 + 80);
        v20 = v22 - (v14 - *(u32 *)(a3 + 12));
        v21 = 0;
        if ( a3 != 0 )
          v21 = *(u32 *)(a3 + 4) + 4;
        *v19 = v21;
        result = sceNpCommerce2_E3455E27(v11 + 12, v20, 126, 0, &v25);
        if ( result >= 0 && v25 != 0 )
          *v19 = v14 + result;
      }
    }
  }
  return result;
}


int store_disp_fn_60(
        int a1,
        int a2,
        u32 *a3,
        u32 *a4,
        u32 *a5,
        u32 *a6,
        u32 *a7,
        u32 *a8,
        u32 *a9,
        u32 *a10)
{
  u32 v17;
  int v18;
  int result;
  u32 v20;
  u64 v21;
  u32 v22;
  u32 v23;
  u32 v24;
  u32 v25;
  u64 v26; // 0:$a4.4,4:$v1.4

  *a3 = a2;
  sceKernelMemcpy(&v20, a2 + 49, 4);
  *a4 = byteswap32(v20);
  sceKernelMemcpy(&v21, a2 + 53, 8);
  v26 = ((u64)(v26) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(byteswap32(v21)) << 32);
  v26 = ((u64)(v26) & 0xFFFFFFFF00000000ULL) | (u32)(byteswap32(HIDWORD(v21)));
  v21 = v26;
  if ( ((unsigned int)v26 | HIDWORD(v26)) != 0 )
  {
    *a5 = 1000 * v26 - 13910016;
    a5[1] = ((1000 * v26) >> 32) + 14467070 + ((unsigned int)(1000 * v26 - 13910016) < 0xFF2BC000);
    v21 = 1000 * v26;
  }
  else
  {
    sceKernelMemset(a5, 0, 8);
  }
  sceKernelMemcpy(&v22, a2 + 61, 4);
  *a6 = byteswap32(v22);
  sceKernelMemcpy(&v23, a2 + 65, 4);
  *a7 = byteswap32(v23);
  sceKernelMemcpy(&v24, a2 + 69, 4);
  v17 = byteswap32(v24);
  *a9 = v17;
  *a8 = (v17 & 0xA) != 0;
  sceKernelMemcpy(&v25, a2 + 73, 4);
  v18 = byteswap32(v25);
  if ( v18 < 0 )
    *a10 |= 1u;
  if ( (v18 & 0x40000000) != 0 )
    *a10 |= 2u;
  result = 77;
  if ( (v18 & 0x20000000) != 0 )
    *a10 |= 4u;
  return result;
}


int store_disp_fn_61(int a1, int a2, int a3, int a4, int a5, u32 *a6)
{
  int v12;
  u32 *v13;
  int v14;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int *v21;
  int *v22;
  int *v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v29;
  int *v30;
  int v31;
  int v32;
  int v33;
  s16 v34;
  s16 v35;
  s16 v36;
  s16 v37;
  s16 v38;
  s16 v39;
  u32 v40;
  u64 v41;
  u32 v42;
  u64 v43; // 0:$a4.4,4:$t6.4

  sceKernelMemcpy(&v33, a3, 56);
  v12 = (unsigned int)sceKernelGetCompiledSdkVersion() > 0x602FFFF;
  v13 = a6;
  v14 = 128;
  if ( !v12 )
  {
    v13 = a6;
    v14 = 88;
  }
  sceKernelMemset(v13, 0, v14);
  sceKernelMemset(a6, 0, 40);
  a6[1] = a3;
  *a6 = a4;
  a6[3] = a1;
  a6[4] = a2;
  a6[10] = a5;
  a6[11] = a1;
  sceKernelMemcpy(&v40, a1 + 49, 4);
  a6[12] = byteswap32(v40);
  sceKernelMemcpy(&v41, a1 + 53, 8);
  v43 = ((u64)(v43) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(byteswap32(v41)) << 32);
  v43 = ((u64)(v43) & 0xFFFFFFFF00000000ULL) | (u32)(byteswap32(HIDWORD(v41)));
  v41 = v43;
  if ( ((unsigned int)v43 | HIDWORD(v43)) != 0 )
  {
    a6[14] = 1000 * v43 - 13910016;
    a6[15] = ((1000 * v43) >> 32) + 14467070 + ((unsigned int)(1000 * v43 - 13910016) < 0xFF2BC000);
    v41 = 1000 * v43;
  }
  else
  {
    sceKernelMemset(a6 + 14, 0, 8);
  }
  sceKernelMemcpy(&v42, a1 + 73, 4);
  v16 = a1 + 77;
  v17 = _bswapw(v42);
  if ( v12 )
  {
    if ( v17 < 0 )
      a6[22] |= 1u;
    if ( (v17 & 0x40000000) != 0 )
      a6[22] |= 2u;
    if ( (v17 & 0x20000000) == 0 )
    {
      v18 = a6[1];
      goto LABEL_7;
    }
    a6[22] |= 4u;
  }
  v18 = a6[1];
LABEL_7:
  a6[16] = v18 + 4;
  v19 = (u16)(a2 - 77);
  a6[5] = v16;
  v20 = sceNpCommerce2_E3455E27(v16, v19, 7, 0, &v34);
  v21 = a6 + 17;
  v22 = a6 + 18;
  v23 = a6 + 19;
  if ( v20 >= 0 && v34 != 0 )
    a6[16] = v16 + v20;
  if ( v20 >= 0 )
  {
    v24 = 0;
    if ( a6 != NULL )
      v24 = a6[1] + 4;
    *v21 = v24;
    v20 = sceNpCommerce2_E3455E27(v16, v19, 12, 0, &v35);
    if ( v20 >= 0 && v35 != 0 )
      *v21 = v16 + v20;
    if ( v20 >= 0 )
    {
      v25 = 0;
      if ( a6 != NULL )
        v25 = a6[1] + 4;
      *v22 = v25;
      v20 = sceNpCommerce2_E3455E27(v16, v19, 16, 0, &v36);
      if ( v20 >= 0 && v36 != 0 )
        *v22 = v16 + v20;
      if ( v20 >= 0 )
      {
        v26 = 0;
        if ( a6 != NULL )
          v26 = a6[1] + 4;
        *v23 = v26;
        v20 = sceNpCommerce2_E3455E27(v16, v19, 4, 0, &v37);
        if ( v20 >= 0 && v37 != 0 )
          *v23 = v16 + v20;
      }
    }
  }
  v27 = v20;
  if ( v20 >= 0 && a5 == 1 )
  {
    a6[20] = a6[1] + 4;
    v29 = sceNpCommerce2_E3455E27(v16, a2 - 77, 13, 0, &v38);
    if ( v29 >= 0 && v38 != 0 )
      a6[20] = v16 + v29;
    v27 = v29;
    if ( v29 >= 0 )
    {
      v30 = a6 + 21;
      v31 = 0;
      if ( a6 != NULL )
        v31 = a6[1] + 4;
      *v30 = v31;
      v32 = sceNpCommerce2_E3455E27(v16, a2 - 77, 119, 0, &v39);
      if ( v32 >= 0 && v39 != 0 )
        *v30 = v16 + v32;
      v27 = v32;
      if ( ((v32 != 0x80550F87) & ((unsigned int)v32 >> 31)) == 0 )
        return 0;
    }
  }
  return v27;
}


int store_disp_fn_62(int a1, int a2, int a3, int a4, u32 *a5)
{
  int v9;
  int v11;
  int v12;
  u16 v14[8];

  v9 = a1 + 17;
  sceKernelMemset(a5, 0, 52);
  sceKernelMemset(a5, 0, 40);
  a5[3] = a1;
  a5[10] = a1;
  a5[11] = a3 + 4;
  *a5 = a4;
  a5[1] = a3;
  a5[4] = a2;
  a5[5] = v9;
  v11 = sceNpCommerce2_E3455E27(a1 + 17, a2 - 17, 16, 0, v14);
  v12 = v11;
  if ( v11 >= 0 && v14[0] != 0 )
    a5[11] = v9 + v11;
  if ( v11 >= 0 )
    return sceNpCommerce2_3F6D413C(v9, a2 - 17, 23, 0, a5 + 12);
  return v12;
}


int store_disp_fn_63(int a1, int a2, int a3)
{
  u32 v3;
  int v7;
  u32 *v8;
  u32 v9;
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
  int result;
  u32 *v21;
  u32 *v22;
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
  u32 v34;

  v3 = 0;
  v7 = a1 + 61;
  **(u32 **)(a2 + 12) = a1;
  sceKernelMemcpy(&v23, a1 + 57, 4);
  v8 = *(u32 **)(a2 + 16);
  v9 = byteswap32(v23);
  *v8 = v9;
  if ( v9 != 2 )
    *v8 = 1;
  sceKernelMemcpy(&v24, v7, 4);
  **(u32 **)(a2 + 20) = byteswap32(v24);
  sceKernelMemcpy(&v25, a1 + 65, 4);
  v10 = *(u32 *)(a2 + 28);
  **(u32 **)(a2 + 24) = byteswap32(v25);
  if ( v10 != 0 )
  {
    sceKernelMemcpy(&v26, a1 + 69, 4);
    v11 = a1 + 73;
    **(u32 **)(a2 + 28) = byteswap32(v26);
  }
  else
  {
    v11 = a1 + 73;
  }
  if ( *(u32 *)(a2 + 32) != 0 )
  {
    sceKernelMemcpy(&v27, v11, 4);
    v12 = v11 + 4;
    **(u32 **)(a2 + 32) = byteswap32(v27);
  }
  else
  {
    v12 = v11 + 4;
  }
  sceKernelMemcpy(&v28, v12, 4);
  **(u32 **)(a2 + 36) = byteswap32(v28) != 0;
  v13 = v12 + 4;
  if ( (*(u32 *)(a2 + 4) & 4) != 0 )
  {
    sceKernelMemcpy(&v29, v13, 4);
    v14 = v13 + 4;
    v3 = _bswapw(v29);
  }
  else
  {
    v14 = v13 + 4;
  }
  if ( *(u32 *)(a2 + 40) != 0 )
  {
    sceKernelMemcpy(&v30, v14, 4);
    v15 = v14 + 4;
    **(u32 **)(a2 + 40) = byteswap32(v30);
  }
  else
  {
    v15 = v14 + 4;
  }
  if ( *(u32 *)(a2 + 44) != 0 )
  {
    sceKernelMemcpy(&v31, v15, 4);
    v16 = v15 + 4;
    **(u32 **)(a2 + 44) = byteswap32(v31);
  }
  else
  {
    v16 = v15 + 4;
  }
  if ( *(u32 *)(a2 + 48) != 0 )
  {
    sceKernelMemcpy(&v32, v16, 4);
    v17 = v16 + 4;
    **(u32 **)(a2 + 48) = byteswap32(v32);
  }
  else
  {
    v17 = v16 + 4;
  }
  if ( a3 != 0 )
  {
    v18 = v17 + 4;
  }
  else if ( *(u32 *)(a2 + 52) != 0 )
  {
    sceKernelMemcpy(&v33, v17, 4);
    v18 = v17 + 4;
    **(u32 **)(a2 + 52) = byteswap32(v33);
  }
  else
  {
    v18 = v17 + 4;
  }
  if ( a3 != 0 )
  {
    v19 = v18 + 4;
  }
  else if ( *(u32 *)(a2 + 56) != 0 )
  {
    sceKernelMemcpy(&v34, v18, 4);
    v19 = v18 + 4;
    **(u32 **)(a2 + 56) = byteswap32(v34);
  }
  else
  {
    v19 = v18 + 4;
  }
  result = v19 - a1;
  if ( a3 == 0 )
  {
    v21 = *(u32 **)(a2 + 28);
    if ( v21 != NULL )
    {
      if ( *v21 != 0 )
        **(u32 **)(a2 + 8) |= 1u;
      v22 = *(u32 **)(a2 + 36);
    }
    else
    {
      v22 = *(u32 **)(a2 + 36);
    }
    if ( v22 != NULL && *v22 != 0 )
      **(u32 **)(a2 + 8) |= 2u;
    result = v19 - a1;
    if ( v3 != 0 )
      **(u32 **)(a2 + 8) |= 4u;
  }
  return result;
}


int store_disp_fn_64(int a1, int a2)
{
  int v4;
  int v5;
  int v6;
  unsigned int *v7;
  unsigned int v9;
  int v10;
  unsigned int *v11;
  unsigned int v13;
  int v14;
  unsigned int *v15;
  unsigned int v17;
  int v18;
  u32 *v19;
  u32 v20;
  u64 v22;
  u64 v23;
  u64 v24;
  u32 v25;
  u32 v26[4];
  u64 v27; // 0:$a5.4,4:$a7.4
  u64 v28; // 0:$a7.4,4:$t5.4
  u64 v29; // 0:$t5.4,4:$t7.4

  v4 = store_disp_fn_63(a1, a2, 0);
  v5 = v4;
  if ( v4 >= 0 )
  {
    v6 = a1 + v4;
    v7 = *(unsigned int **)(a2 + 60);
    sceKernelMemcpy(&v22, a1 + v4, 8);
    v27 = ((u64)(v27) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(byteswap32(v22)) << 32);
    v27 = ((u64)(v27) & 0xFFFFFFFF00000000ULL) | (u32)(byteswap32(HIDWORD(v22)));
    v22 = v27;
    if ( ((unsigned int)v27 | HIDWORD(v27)) != 0 )
    {
      v9 = 1000 * v27 - 13910016;
      v22 = 1000 * v27;
      *v7 = v9;
      v7[1] = ((1000 * v27) >> 32) + 14467070 + (v9 < 0xFF2BC000);
    }
    else
    {
      sceKernelMemset(v7, 0, 8);
    }
    v10 = v6 + 8;
    v11 = *(unsigned int **)(a2 + 64);
    sceKernelMemcpy(&v23, v10, 8);
    v28 = ((u64)(v28) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(byteswap32(v23)) << 32);
    v28 = ((u64)(v28) & 0xFFFFFFFF00000000ULL) | (u32)(byteswap32(HIDWORD(v23)));
    v23 = v28;
    if ( ((unsigned int)v28 | HIDWORD(v28)) != 0 )
    {
      v13 = 1000 * v28 - 13910016;
      v23 = 1000 * v28;
      *v11 = v13;
      v11[1] = ((1000 * v28) >> 32) + 14467070 + (v13 < 0xFF2BC000);
    }
    else
    {
      sceKernelMemset(v11, 0, 8);
    }
    v14 = v10 + 8;
    v15 = *(unsigned int **)(a2 + 68);
    sceKernelMemcpy(&v24, v14, 8);
    v29 = ((u64)(v29) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(byteswap32(v24)) << 32);
    v29 = ((u64)(v29) & 0xFFFFFFFF00000000ULL) | (u32)(byteswap32(HIDWORD(v24)));
    v24 = v29;
    if ( ((unsigned int)v29 | HIDWORD(v29)) != 0 )
    {
      v17 = 1000 * v29 - 13910016;
      v24 = 1000 * v29;
      *v15 = v17;
      v15[1] = ((1000 * v29) >> 32) + 14467070 + (v17 < 0xFF2BC000);
    }
    else
    {
      sceKernelMemset(v15, 0, 8);
    }
    v18 = v14 + 8;
    sceKernelMemcpy(&v25, v18, 4);
    **(u32 **)(a2 + 72) = byteswap32(v25);
    sceKernelMemcpy(v26, v18 + 4, 8);
    v19 = *(u32 **)(a2 + 76);
    v20 = byteswap32(v26[0]);
    *v19 = byteswap32(v26[1]);
    v19[1] = v20;
    return v18 + 12 - a1;
  }
  return v5;
}


s32 store_disp_fn_65(int a1, int a2, int a3, int a4, int a5, u32 *a6)
{
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
  int v26;
  u16 v28;
  s16 v29;
  s16 v30;
  s16 v31;
  u32 v32;
  u32 v33;

  v10 = sceNpCommerce2_E3455E27(a5, a4 - (a5 - a3), 75, 8, &v28);
  v11 = v10;
  if ( v10 >= 0 )
  {
    v12 = a5 + v10;
    sceKernelMemset(a6, 0, 76);
    v13 = v28;
    sceKernelMemset(a6, 0, 40);
    *a6 = a1;
    v14 = v12;
    a6[4] = v13;
    v15 = v12 + 4;
    a6[3] = v12;
    v16 = v12;
    v17 = v12 + 8;
    a6[1] = a2;
    sceKernelMemcpy(&v32, v14, 4);
    v18 = v15;
    a6[10] = byteswap32(v32);
    v19 = v17 - v16;
    sceKernelMemcpy(&v33, v18, 4);
    v20 = v28;
    v21 = a6[1] + 4;
    a6[11] = byteswap32(v33);
    a6[12] = v21;
    a6[5] = v17;
    v22 = sceNpCommerce2_E3455E27(v17, v20 - (v17 - v16), 90, 0, &v29);
    if ( v22 >= 0 && v29 != 0 )
      a6[12] = v17 + v22;
    v11 = v22;
    if ( ((v22 != 0x80550F87) & ((unsigned int)v22 >> 31)) == 0 )
    {
      v23 = v28;
      a6[13] = a6[1] + 4;
      v24 = sceNpCommerce2_E3455E27(v17, v23 - v19, 70, 0, &v30);
      if ( v24 >= 0 && v30 != 0 )
        a6[13] = v17 + v24;
      v11 = v24;
      if ( ((v24 != 0x80550F87) & ((unsigned int)v24 >> 31)) == 0 )
      {
        v25 = v28;
        a6[14] = a6[1] + 4;
        v26 = sceNpCommerce2_E3455E27(v17, v25 - v19, 71, 0, &v31);
        if ( v26 >= 0 && v31 != 0 )
          a6[14] = v17 + v26;
        v11 = v26;
        if ( ((v26 != 0x80550F87) & ((unsigned int)v26 >> 31)) == 0 )
          return store_min_s32(store_disp_fn_73(a6, a6 + 15, a6 + 16, a6 + 17, a6 + 18), 0);
      }
    }
  }
  return v11;
}


s32 store_disp_fn_66(int a1, int a2, int a3, int a4, int a5, u32 *a6)
{
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  u32 v19;
  int v20;
  u16 v22;
  s16 v23;
  u32 v24;
  u32 v25;

  v10 = sceNpCommerce2_E3455E27(a5, a4 - (a5 - a3), 145, 8, &v22);
  v11 = a5 + v10;
  v12 = v10;
  v13 = a5 + v10;
  if ( v10 >= 0 )
  {
    sceKernelMemset(a6, 0, 68);
    v14 = v22;
    sceKernelMemset(a6, 0, 40);
    a6[1] = a2;
    v15 = v11;
    a6[4] = v14;
    v16 = v11 + 4;
    a6[3] = v11;
    v17 = v11 + 8;
    *a6 = a1;
    sceKernelMemcpy(&v24, v15, 4);
    a6[10] = byteswap32(v24);
    sceKernelMemcpy(&v25, v16, 4);
    v18 = v22;
    v19 = byteswap32(v25);
    a6[12] = a6[1] + 4;
    a6[11] = v19;
    a6[5] = v17;
    v20 = sceNpCommerce2_E3455E27(v17, v18 - (v17 - v13), 146, 0, &v23);
    v12 = v20;
    if ( v20 >= 0 && v23 != 0 )
      a6[12] = v17 + v20;
    if ( ((v20 != 0x80550F87) & ((unsigned int)v20 >> 31)) == 0 )
      return store_min_s32(store_disp_fn_73(a6, a6 + 13, a6 + 14, a6 + 15, a6 + 16), 0);
  }
  return v12;
}


int store_disp_fn_67(int *a1, u32 *a2)
{
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 v12;
  u64 v13;
  u16 v15;
  u32 v16;
  s64 v17;

  v4 = a1[3];
  v5 = sceNpCommerce2_E3455E27(v4, a1[4], 44, 12, &v15);
  v6 = v4 + v5;
  if ( v5 >= 0 )
  {
    sceKernelMemset(a2, 0, 56);
    v7 = *a1;
    v8 = v15;
    v9 = a1[1];
    sceKernelMemset(a2, 0, 40);
    a2[3] = v6;
    v10 = v6;
    a2[1] = v9;
    v11 = v6 + 4;
    *a2 = v7;
    a2[4] = v8;
    sceKernelMemcpy(&v16, v10, 4);
    a2[10] = byteswap32(v16);
    sceKernelMemcpy(&v17, v11, 8);
    v12 = byteswap32(v17);
    v13 = __PAIR64__(v12, byteswap32(HIDWORD(v17)));
    v17 = __PAIR64__(v12, v13);
    if ( ((unsigned int)v13 | v12) != 0 )
    {
      a2[12] = 1000 * v13 - 13910016;
      a2[13] = ((1000 * v13) >> 32) + 14467070 + ((unsigned int)(1000 * v13 - 13910016) < 0xFF2BC000);
      v17 = 1000 * v13;
    }
    else
    {
      sceKernelMemset(a2 + 12, 0, 8);
    }
    a2[5] = v11 + 8;
  }
  return v5;
}


int store_disp_fn_68(int *a1, u32 *a2)
{
  int v4;
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  u32 v10;
  int v11;
  u16 v12;
  u32 v13;
  u32 v14;
  u32 v15;

  v4 = a1[3];
  result = sceNpCommerce2_E3455E27(v4, a1[4], 47, 16, &v12);
  v6 = v4 + result;
  if ( result >= 0 )
  {
    sceKernelMemset(a2, 0, 56);
    v7 = a1[1];
    v8 = *a1;
    v9 = v12;
    sceKernelMemset(a2, 0, 40);
    a2[1] = v7;
    *a2 = v8;
    a2[4] = v9;
    a2[3] = v6;
    sceKernelMemcpy(&v13, v6, 4);
    a2[10] = byteswap32(v13);
    sceKernelMemcpy(&v14, v6 + 4, 4);
    a2[11] = byteswap32(v14);
    sceKernelMemcpy(&v15, v6 + 8, 4);
    v10 = v15;
    v11 = v12;
    a2[5] = v6 + 12;
    a2[12] = byteswap32(v10);
    return sceNpCommerce2_3546C737(v6 + 12, v11 - 12, 46, 0, a2 + 13, a1[6]);
  }
  return result;
}


int store_disp_fn_69(int *a1, int a2, int a3, u32 *a4)
{
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 v12;
  u16 v14;
  u32 v15;
  u32 v16;

  v7 = sceNpCommerce2_E3455E27(a2, a3, 88, 8, &v14);
  v8 = a2 + v7;
  if ( v7 >= 0 )
  {
    sceKernelMemset(a4, 0, 48);
    v9 = *a1;
    v10 = v14;
    v11 = a1[1];
    sceKernelMemset(a4, 0, 40);
    a4[1] = v11;
    *a4 = v9;
    a4[4] = v10;
    a4[3] = v8;
    sceKernelMemcpy(&v15, v8, 4);
    a4[10] = byteswap32(v15);
    sceKernelMemcpy(&v16, v8 + 4, 4);
    v12 = v16;
    a4[5] = v8 + 8;
    a4[11] = byteswap32(v12);
  }
  return v7;
}


int store_disp_fn_70(int *a1, unsigned int a2, u32 *a3)
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
  u16 v18;
  s16 v19[7];

  v5 = a1[3];
  v6 = store_disp_fn_55(v5, a1[4], 0x37u, 0x49u, a2, &v18);
  v7 = v5 + v6;
  v8 = v6;
  v9 = v5 + v6;
  if ( v6 >= 0 )
  {
    sceKernelMemset(a3, 0, 52);
    v10 = a1[1];
    v11 = v18;
    v12 = *a1;
    sceKernelMemset(a3, 0, 40);
    v13 = v18;
    a3[3] = v7;
    a3[10] = v7;
    v14 = v7 + 65;
    v15 = v14 - v9;
    *a3 = v12;
    a3[4] = v11;
    a3[11] = v10 + 4;
    a3[1] = v10;
    a3[5] = v14;
    v16 = sceNpCommerce2_E3455E27(v14, v13 - v15, 54, 0, v19);
    v8 = v16;
    if ( v16 >= 0 && v19[0] != 0 )
      a3[11] = v14 + v16;
    if ( v16 >= 0 )
      return sceNpCommerce2_3F6D413C(v14, v18 - v15, 59, 0, a3 + 12);
  }
  return v8;
}


int store_disp_fn_71(int *a1, unsigned int a2, u32 *a3)
{
  int v5;
  int result;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int *v13;
  int v14;
  int v15;
  u16 v16;
  s16 v17;
  s16 v18[6];

  v5 = a1[3];
  result = store_disp_fn_55(v5, a1[4], 0x39u, 0x19u, a2, &v16);
  if ( result >= 0 )
  {
    v7 = v5 + result;
    sceKernelMemset(a3, 0, 52);
    v8 = a1[1];
    v9 = v16;
    v10 = *a1;
    sceKernelMemset(a3, 0, 40);
    v11 = v7 + 17;
    v12 = v16;
    a3[3] = v7;
    a3[10] = v7;
    a3[11] = v8 + 4;
    *a3 = v10;
    a3[4] = v9;
    a3[1] = v8;
    a3[5] = v7 + 17;
    result = sceNpCommerce2_E3455E27(v7 + 17, v12 - 17, 15, 0, &v17);
    if ( result >= 0 && v17 != 0 )
      a3[11] = v11 + result;
    if ( result >= 0 )
    {
      v13 = a3 + 12;
      v14 = 0;
      v15 = v16 - 17;
      if ( a3 != NULL )
        v14 = a3[1] + 4;
      *v13 = v14;
      result = sceNpCommerce2_E3455E27(v11, v15, 16, 0, v18);
      if ( result >= 0 && v18[0] != 0 )
        *v13 = v11 + result;
    }
  }
  return result;
}


int store_disp_fn_72(int *a1, u32 *a2)
{
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  u32 v10;
  u16 v12;
  u32 v13;
  u32 v14;

  v4 = a1[3];
  v5 = sceNpCommerce2_E3455E27(v4, a1[4], 50, 8, &v12);
  v6 = v4 + v5;
  if ( v5 >= 0 )
  {
    sceKernelMemset(a2, 0, 48);
    v7 = *a1;
    v8 = v12;
    v9 = a1[1];
    sceKernelMemset(a2, 0, 40);
    a2[1] = v9;
    *a2 = v7;
    a2[4] = v8;
    a2[3] = v6;
    sceKernelMemcpy(&v13, v6, 4);
    a2[10] = byteswap32(v13);
    sceKernelMemcpy(&v14, v6 + 4, 4);
    v10 = v14;
    a2[5] = v6 + 8;
    a2[11] = byteswap32(v10);
  }
  return v5;
}


int store_disp_fn_73(u32 *a1, u32 *a2, u32 *a3, u32 *a4, u32 *a5)
{
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v15;
  u32 v16;
  u32 v17;
  u32 v18;

  v9 = a1[5];
  v10 = sceNpCommerce2_E3455E27(v9, a1[4] - (v9 - a1[3]), 74, 12, &v15);
  v11 = v10;
  if ( v10 == -2141909113 )
  {
    *a2 = 0;
    return 0;
  }
  v12 = v9 + v10;
  if ( v10 >= 0 )
  {
    sceKernelMemcpy(&v16, v12, 4);
    v13 = v12 + 4;
    *a3 = byteswap32(v16);
    sceKernelMemcpy(&v17, v13, 4);
    *a4 = byteswap32(v17);
    sceKernelMemcpy(&v18, v13 + 4, 4);
    *a5 = byteswap32(v18);
    *a2 = 1;
    return 0;
  }
  return v11;
}


signed int store_disp_fn_74(unsigned int a1, int a2, unsigned int a3, int *a4)
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
  v10 = sceNpCommerce2_0A6AB496(&g_store_mgr, HIWORD(a1), &v26, 1);
  if ( v10 < 0 )
    goto LABEL_22;
  v10 = sceNpCommerce2_851E7844(&g_store_mgr, a1, &v27);
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
    store_util_fn_28(v26, v27, a2, a3, *a4);
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
  store_tlv_fn_38(a2, v21, v29);
  v8 = v27;
  if ( v29[0] == 1 )
    v10 = -2141909101;
LABEL_23:
  if ( v8 != 0 )
  {
    if ( (*(u32 *)(v8 + 12) & 2) != 0 )
      v10 = -2141909232;
    sceNpCommerce2_8642C532(&g_store_mgr);
    v22 = v26;
  }
  else
  {
    v22 = v26;
  }
  if ( v22 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v10;
}


int store_disp_fn_75(unsigned int a1, int a2, int a3, int a4, int a5, unsigned int a6)
{
  int v12;
  int v13;
  int v14;
  int v15;
  s64 v16;
  int v17;
  int v18;
  int v19;
  int v21;
  int v22;
  s64 v23;

  v21 = 0;
  v22 = 0;
  v12 = sceNpCommerce2_0A6AB496(&g_store_mgr, HIWORD(a1), &v21, 1);
  if ( v12 < 0 )
    goto LABEL_12;
  v12 = sceNpCommerce2_851E7844(&g_store_mgr, a1, &v22);
  if ( v12 < 0 )
    goto LABEL_12;
  v13 = v21;
  if ( *(u32 *)(v21 + 1080) == 0 )
  {
    v12 = -2141909245;
    if ( a6 >= 0x3D )
      goto LABEL_12;
  }
  v14 = sceNpCommerce2_FC47B63C(&v23);
  v15 = v22;
  if ( v14 < 0 )
    goto LABEL_9;
  v16 = *(u64 *)(v13 + 1072);
  if ( (u64)(v23 - v16) >> 32 != 0 || (v17 = v21, v18 = a2, (unsigned int)(v23 - v16) > 0x98967F) )
  {
    sceNpCommerce2_75DF99A9(*(u32 *)(v13 + 24), v13 + 28);
    v15 = v22;
LABEL_9:
    v17 = v21;
    v18 = a2;
  }
  v12 = store_req_fn_8(
          *(u32 *)(v17 + 1064),
          *(u32 *)(v17 + 24),
          v17 + 28,
          *(u16 *)(v15 + 20),
          (int *)(v15 + 16),
          v18,
          a3,
          a4,
          a5,
          a6);
  if ( v12 >= 0 )
    sceNpCommerce2_FC47B63C(v21 + 1072);
LABEL_12:
  if ( v22 != 0 )
  {
    if ( (*(u32 *)(v22 + 12) & 2) != 0 )
      v12 = -2141909232;
    sceNpCommerce2_8642C532(&g_store_mgr);
    v19 = v21;
  }
  else
  {
    v19 = v21;
  }
  if ( v19 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v12;
}


int store_disp_fn_76(unsigned int a1, int a2, int a3, int a4, int a5, int a6)
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
  v12 = sceNpCommerce2_0A6AB496(&g_store_mgr, HIWORD(a1), &v21, 1);
  if ( v12 < 0 )
    goto LABEL_14;
  v12 = sceNpCommerce2_851E7844(&g_store_mgr, a1, &v22);
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
    if ( HIDWORD(v24) - HIDWORD(v16) != (unsigned int)v24 < (unsigned int)v16 )
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
  v12 = store_req_fn_29(
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
    sceNpCommerce2_8642C532(&g_store_mgr);
    v18 = v21;
  }
  else
  {
    v18 = v21;
  }
  if ( v18 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v12;
}


int store_disp_fn_77(int a1, u8 *a2, int a3)
{
  int v5;
  char **i;
  char **v7;
  int v8; // dc
  int v9;
  int v10;
  char *v11;
  u8 v13[16];
  u32 v14[4];

  v14[0] = 0;
  v5 = sceNpCommerce2_0A6AB496(&g_store_mgr, a1, v14, 1);
  if ( v5 >= 0 )
  {
    sceKernelMemset(a3, 0, 8);
    if ( a2 == NULL )
    {
      v5 = sceNpCommerce2_D5932A6C(v14[0], v13, 3, 0);
      if ( v5 < 0 )
        goto LABEL_9;
      a2 = v13;
    }
    for ( i = g_country_table; ; ++i )
    {
      v7 = (&g_country_table[2]);
      if ( sceNet_lib_D8722983(*i) == 0 )
        break;
      v8 = sceNet_lib_A0F16ABD(*i, a2) == 0;
      v9 = 7;
      if ( v8 )
      {
LABEL_7:
        *(u32 *)(a3 + 4) = v9;
        v10 = a3;
        v11 = *i;
LABEL_8:
        sceNet_lib_B5CE388A(v10, v11, 3);
        goto LABEL_9;
      }
    }
    while ( 1 )
    {
      i = (&g_country_table[35]);
      if ( sceNet_lib_D8722983(*v7) == 0 )
        break;
      if ( sceNet_lib_A0F16ABD(*v7, a2) == 0 )
      {
        *(u32 *)(a3 + 4) = 3;
        v10 = a3;
        v11 = *v7;
        goto LABEL_8;
      }
      ++v7;
    }
    while ( sceNet_lib_D8722983(*i) != 0 )
    {
      v8 = sceNet_lib_A0F16ABD(*i, a2) == 0;
      v9 = 1;
      if ( v8 )
        goto LABEL_7;
      ++i;
    }
    v5 = -2141909102;
  }
LABEL_9:
  if ( v14[0] != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v5;
}


int store_disp_fn_78()
{
  sceKernelMemset(&g_store_mgr, 0, 4);
  return 0;
}


void store_term_noop()
{
  ;
}


int store_disp_fn_79(unsigned int a1, int a2, int a3, int a4, int a5)
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
  v10 = store_req_fn_9(
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


int store_disp_fn_80(int a1, int a2)
{
  return store_util_fn_27(a1, 16, a2, 0);
}


int store_disp_fn_81(unsigned int a1, int a2, int a3, int a4, int a5)
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
  v10 = store_req_fn_10(
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


int store_disp_fn_82(int a1, int a2)
{
  return store_util_fn_27(a1, 17, a2, 0);
}


int store_disp_fn_83(unsigned int a1, int a2)
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
  v4 = store_req_fn_11(*(u32 *)(v7 + 1064), *(u32 *)(v7 + 24), v7 + 28, *(u16 *)(v10 + 20), (int *)(v10 + 16), v11);
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


int store_disp_fn_84(int a1, int a2)
{
  return store_util_fn_27(a1, 18, a2, 0);
}


s32 store_disp_fn_85(unsigned int a1)
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
  v2 = store_req_fn_12(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
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


int store_disp_fn_86(int a1, int a2)
{
  return store_util_fn_27(a1, 19, a2, 0);
}


s32 store_disp_fn_87(unsigned int a1)
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
  v2 = store_req_fn_13(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
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


int store_disp_fn_88(int a1, int a2)
{
  return store_util_fn_27(a1, 8, a2, 0);
}


int store_disp_fn_89(int a1, int a2)
{
  return store_util_fn_27(a1, 9, a2, 0);
}


int store_disp_fn_90(unsigned int a1, int a2, int a3, int a4)
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
  v8 = store_req_fn_14(
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


int store_disp_fn_91(int a1, int a2)
{
  return store_util_fn_27(a1, 10, a2, 0);
}


s32 store_disp_fn_92(unsigned int a1)
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
  v2 = store_req_fn_15(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
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


int store_disp_fn_93(int a1, int a2)
{
  return store_util_fn_27(a1, 20, a2, 0);
}


s32 store_disp_fn_94(unsigned int a1)
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


int store_disp_fn_95(int a1, int a2)
{
  return store_util_fn_27(a1, 21, a2, 0);
}


int store_disp_fn_96(int a1, int a2)
{
  return store_util_fn_27(a1, 43, a2, 0);
}


int store_disp_fn_97(unsigned int a1, int a2)
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
  v4 = store_req_fn_17(*(u32 *)(v7 + 1064), *(u32 *)(v7 + 24), v7 + 28, *(u16 *)(v10 + 20), (u32 *)(v10 + 16), v11);
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


int store_disp_fn_98(int a1, int a2)
{
  return store_util_fn_27(a1, 7, a2, 0);
}


int store_disp_fn_99(unsigned int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8)
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
  v15 = sceNpCommerce2_0A6AB496(&g_store_mgr, HIWORD(a1), &v23, 1);
  if ( v15 < 0 )
    goto LABEL_14;
  v15 = sceNpCommerce2_851E7844(&g_store_mgr, a1, &v24);
  if ( v15 < 0 )
    goto LABEL_14;
  v17 = v23;
  v16 = sceNpCommerce2_FC47B63C(&v25);
  v18 = v24;
  if ( v16 >= 0 )
  {
    v19 = *(u64 *)(v17 + 1072);
    if ( HIDWORD(v25) - HIDWORD(v19) == (unsigned int)v25 < (unsigned int)v19 )
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
  v15 = store_req_fn_18(
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
    sceNpCommerce2_8642C532(&g_store_mgr);
    v21 = v23;
  }
  else
  {
    v21 = v23;
  }
  if ( v21 != 0 )
    sceNpCommerce2_77BE411B(&g_store_mgr);
  return v15;
}


int store_disp_fn_100(int a1, int a2)
{
  return store_util_fn_27(a1, 22, a2, 0);
}


int store_disp_fn_101(int a1, int a2)
{
  return store_util_fn_27(a1, 25, a2, 0);
}


s32 store_disp_fn_102(unsigned int a1)
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
  v2 = store_req_fn_19(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
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


int store_disp_fn_103(int a1, int a2)
{
  return store_util_fn_27(a1, 23, a2, 0);
}


int store_disp_fn_104(unsigned int a1, int a2)
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
  v4 = store_req_fn_20(*(u32 *)(v7 + 1064), *(u32 *)(v7 + 24), v7 + 28, *(u16 *)(v10 + 20), (int *)(v10 + 16), v11);
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


int store_disp_fn_105(int a1, int a2)
{
  return store_util_fn_27(a1, 24, a2, 0);
}


int store_disp_fn_106(unsigned int a1, int a2)
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
  v4 = store_req_fn_21(*(u32 *)(v7 + 1064), *(u32 *)(v7 + 24), v7 + 28, *(u16 *)(v10 + 20), (int *)(v10 + 16), v11);
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


int store_disp_fn_107(int a1, int a2)
{
  return store_util_fn_27(a1, 6, a2, 0);
}


s32 store_disp_fn_108(unsigned int a1)
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
  v2 = store_req_fn_22(*(u32 *)(v5 + 1064), v9, v5 + 28, *(u16 *)(v8 + 20), (u32 *)(v8 + 16));
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


