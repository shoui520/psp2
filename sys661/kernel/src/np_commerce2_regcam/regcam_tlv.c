#include "regcam_internal.h"

int regcam_tlv_parse_1(int *a1, int a2, u32 *a3)
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
  v7 = regcam_tlv_parse_54(v6, a1[4] - (v6 - a1[3]), 128, 0, a2, &v31);
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


int regcam_tlv_parse_2(int *a1, u32 *a2)
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


int regcam_tlv_parse_3(u32 *a1, int a2, int a3)
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

  result = regcam_tlv_parse_53(a2, a3, &v24, v25, 0);
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
              return regcam_min_s32(v17, 0);
            }
          }
        }
      }
    }
  }
  return result;
}


int regcam_tlv_parse_4(int *a1, int a2, u32 *a3)
{
  int v5;
  int result;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11;
  int v12;
  u16 v13;
  u32 v14;
  u32 v15;

  v5 = a1[3];
  result = regcam_tlv_parse_54(v5, a1[4], 91, 15, a2, &v13);
  v7 = v5 + result;
  if ( result >= 0 )
  {
    sceKernelMemset(a3, 0, 60);
    v8 = *a1;
    v9 = v13;
    v10 = a1[1];
    sceKernelMemset(a3, 0, 40);
    a3[1] = v10;
    *a3 = v8;
    a3[4] = v9;
    a3[3] = v7;
    a3[10] = v7;
    sceKernelMemcpy(&v14, v7 + 3, 4);
    a3[11] = byteswap32(v14);
    sceKernelMemcpy(&v15, v7 + 7, 4);
    v11 = v15;
    v12 = v13;
    a3[5] = v7 + 11;
    a3[13] = byteswap32(v11);
    result = sceNpCommerce2_3F6D413C(v7 + 11, v12 - 11, 92, 0, a3 + 12);
    if ( result >= 0 )
      return sceNpCommerce2_3F6D413C(v7 + 11, v13 - 11, 155, 0, a3 + 14);
  }
  return result;
}


int regcam_tlv_parse_5(int *a1, u32 *a2)
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


int regcam_tlv_parse_6(int *a1, u32 *a2)
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


int regcam_tlv_parse_7(int *a1, u32 *a2)
{
  int v4;
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  u32 v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int *v18;
  int v19;
  int v20;
  u16 v21;
  s16 v22;
  s16 v23;
  u32 v24;

  v4 = a1[3];
  result = sceNpCommerce2_E3455E27(v4, a1[4], 103, 78, &v21);
  if ( result >= 0 )
  {
    v6 = v4 + result;
    sceKernelMemset(a2, 0, 60);
    v7 = *a1;
    v8 = v21;
    v9 = a1[1];
    sceKernelMemset(a2, 0, 40);
    a2[1] = v9;
    *a2 = v7;
    v10 = v6 + 4;
    a2[3] = v6;
    v11 = v6;
    a2[4] = v8;
    sceKernelMemcpy(&v24, v6, 4);
    v12 = v6 + 37;
    v13 = byteswap32(v24);
    v14 = v6 + 70;
    v15 = v21;
    v16 = a2[1] + 4;
    a2[11] = v10;
    v17 = v10 + 66 - v11;
    a2[12] = v12;
    a2[10] = v13;
    a2[13] = v16;
    a2[5] = v14;
    result = sceNpCommerce2_E3455E27(v14, v15 - v17, 101, 0, &v22);
    if ( result >= 0 && v22 != 0 )
      a2[13] = v14 + result;
    if ( result >= 0 )
    {
      v18 = a2 + 14;
      v19 = 0;
      v20 = v21 - v17;
      if ( a2 != NULL )
        v19 = a2[1] + 4;
      *v18 = v19;
      result = sceNpCommerce2_E3455E27(v14, v20, 102, 0, &v23);
      if ( result >= 0 && v23 != 0 )
        *v18 = v14 + result;
    }
  }
  return result;
}


int regcam_tlv_parse_8(int *a1, u32 *a2)
{
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11;
  int v12;
  u16 v14;
  u32 v15;
  u32 v16;

  v4 = a1[3];
  v5 = sceNpCommerce2_E3455E27(v4, a1[4], 105, 8, &v14);
  v6 = v4 + v5;
  if ( v5 >= 0 )
  {
    sceKernelMemset(a2, 0, 48);
    v7 = *a1;
    v8 = v14;
    v9 = a1[1];
    sceKernelMemset(a2, 0, 40);
    a2[1] = v9;
    *a2 = v7;
    a2[4] = v8;
    a2[3] = v6;
    sceKernelMemcpy(&v15, v6, 4);
    v10 = 1;
    if ( byteswap32(v15) != 0 )
      v10 = 2;
    a2[10] = v10;
    sceKernelMemcpy(&v16, v6 + 4, 4);
    v11 = v16;
    v12 = 1;
    a2[5] = v6 + 8;
    if ( byteswap32(v11) != 0 )
      v12 = 2;
    a2[11] = v12;
  }
  return v5;
}


int regcam_tlv_parse_9(u32 *a1, int a2, int a3)
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

  v5 = regcam_tlv_parse_53(a2, a3, &v14, &v15, 0);
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


int regcam_tlv_parse_10(int *a1, int a2, int *a3)
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
  u32 v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int *v23;
  int v24;
  u32 v25[10];
  u32 v26;
  u32 v27;
  u16 v28;
  u16 v29;
  s16 v30;
  u32 v31;
  u32 v32;
  u32 v33;
  u32 v34;
  u32 v35;
  u32 v36;
  u32 v37[4];

  v5 = a1[3];
  result = regcam_tlv_parse_54(v5, a1[4], 100, 24, a2, &v28);
  if ( result >= 0 )
  {
    v7 = v5 + result;
    sceKernelMemset(a3, 0, 68);
    v8 = *a1;
    v9 = a1[1];
    v10 = v28;
    sceKernelMemset(a3, 0, 40);
    a3[1] = v9;
    v11 = v7;
    *a3 = v8;
    v12 = v7;
    a3[4] = v10;
    v13 = v7 + 4;
    a3[3] = v7;
    v7 += 8;
    sceKernelMemcpy(&v31, v11, 4);
    a3[10] = byteswap32(v31);
    sceKernelMemcpy(&v32, v13, 4);
    a3[11] = byteswap32(v32) != 0;
    v14 = v7 + 12;
    sceKernelMemcpy(&v33, v7, 4);
    a3[12] = byteswap32(v33) != 0;
    v15 = v7 + 12 - v12;
    sceKernelMemcpy(&v34, v7 + 4, 4);
    a3[13] = byteswap32(v34) != 0;
    sceKernelMemcpy(&v35, v7 + 8, 4);
    v16 = v35;
    v17 = v28;
    a3[5] = v7 + 12;
    a3[14] = byteswap32(v16) != 0;
    v18 = sceNpCommerce2_E3455E27(v7 + 12, v17 - v15, 88, 8, &v29);
    if ( v18 >= 0 )
    {
      sceKernelMemset(v25, 0, 48);
      v19 = v29;
      v20 = a3[1];
      v21 = *a3;
      sceKernelMemset(v25, 0, 40);
      v25[4] = v19;
      v25[1] = v20;
      v25[0] = v21;
      v25[3] = v14 + v18;
      sceKernelMemcpy(&v36, v14 + v18, 4);
      v26 = byteswap32(v36);
      sceKernelMemcpy(v37, v14 + v18 + 4, 4);
      v25[5] = v14 + v18 + 8;
      v27 = byteswap32(v37[0]);
    }
    result = v18;
    if ( v18 >= 0 )
    {
      v22 = v28;
      v23 = a3 + 16;
      a3[15] = v26;
      v24 = 0;
      if ( a3 != NULL )
        v24 = a3[1] + 4;
      *v23 = v24;
      result = sceNpCommerce2_E3455E27(v14, v22 - v15, 93, 0, &v30);
      if ( result >= 0 && v30 != 0 )
        *v23 = v14 + result;
    }
  }
  return result;
}


int regcam_tlv_parse_11(int *a1, int a2, u32 *a3)
{
  int v5;
  int result;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  u32 v13;
  u32 v14;
  int v15;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int *v22;
  int v23;
  int v24;
  u16 v25;
  s16 v26;
  s16 v27;
  u32 v28[2];
  u32 v29[2];
  u64 v30;
  u32 v31;
  u32 v32;
  u32 v33;
  u32 v34;
  u64 v35; // 0:$a3.4,4:$a5.4

  v5 = a1[3];
  result = regcam_tlv_parse_54(v5, a1[4], 107, 48, a2, &v25);
  if ( result >= 0 )
  {
    v7 = v5 + result;
    sceKernelMemset(a3, 0, 88);
    v8 = *a1;
    v9 = a1[1];
    v10 = v25;
    sceKernelMemset(a3, 0, 40);
    a3[1] = v9;
    v11 = v7;
    *a3 = v8;
    a3[4] = v10;
    v12 = v7;
    a3[3] = v7;
    v7 += 8;
    sceKernelMemcpy(v28, v11, 8);
    v13 = byteswap32(v28[0]);
    a3[10] = byteswap32(v28[1]);
    a3[11] = v13;
    sceKernelMemcpy(v29, v7, 8);
    v14 = byteswap32(v29[0]);
    a3[12] = byteswap32(v29[1]);
    a3[13] = v14;
    v15 = v7 + 8;
    sceKernelMemcpy(&v30, v15, 8);
    v35 = ((u64)(v35) & 0xFFFFFFFF00000000ULL) | (u32)(byteswap32(HIDWORD(v30)));
    v35 = ((u64)(v35) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(byteswap32(v30)) << 32);
    v30 = v35;
    if ( v35 != 0 )
    {
      a3[14] = 1000 * v35 - 13910016;
      a3[15] = ((1000 * v35) >> 32) + 14467070 + ((unsigned int)(1000 * v35 - 13910016) < 0xFF2BC000);
      v30 = 1000 * v35;
    }
    else
    {
      sceKernelMemset(a3 + 14, 0, 8);
    }
    sceKernelMemcpy(&v31, v15 + 8, 4);
    a3[16] = byteswap32(v31);
    sceKernelMemcpy(&v32, v15 + 12, 4);
    a3[17] = byteswap32(v32);
    sceKernelMemcpy(&v33, v15 + 16, 4);
    v18 = v15 + 20;
    v17 = v15 + 16;
    a3[18] = byteswap32(v33);
    sceKernelMemcpy(&v34, v15 + 20, 4);
    v19 = a3[1];
    v20 = v15 + 24;
    v21 = v25;
    a3[19] = byteswap32(v34);
    a3[20] = v19 + 4;
    a3[5] = v17 + 8;
    result = sceNpCommerce2_E3455E27(v17 + 8, v21 - (v20 - v12), 106, 0, &v26);
    if ( result >= 0 && v26 != 0 )
      a3[20] = v20 + result;
    if ( result >= 0 )
    {
      v22 = a3 + 21;
      v23 = 0;
      v24 = v25 - (v20 - v12);
      if ( a3 != NULL )
        v23 = a3[1] + 4;
      *v22 = v23;
      result = sceNpCommerce2_E3455E27(v18 + 4, v24, 93, 0, &v27);
      if ( result >= 0 && v27 != 0 )
        *v22 = v20 + result;
    }
  }
  return result;
}


int regcam_tlv_parse_12(int *a1, u32 *a2)
{
  int v4;
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 v12;
  u32 v13;
  int v14;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  u16 v23;
  s16 v24;
  s16 v25;
  u32 v26[2];
  u32 v27[2];
  u64 v28;
  u32 v29;
  u32 v30;
  u32 v31;
  u32 v32;
  u64 v33; // 0:$a3.4,4:$a5.4

  v4 = a1[3];
  result = sceNpCommerce2_E3455E27(v4, a1[4], 112, 52, &v23);
  if ( result >= 0 )
  {
    v6 = v4 + result;
    sceKernelMemset(a2, 0, 96);
    v7 = *a1;
    v8 = a1[1];
    v9 = v23;
    sceKernelMemset(a2, 0, 40);
    a2[1] = v8;
    v10 = v6;
    *a2 = v7;
    a2[4] = v9;
    v11 = v6;
    a2[3] = v6;
    v6 += 8;
    sceKernelMemcpy(v26, v10, 8);
    v12 = byteswap32(v26[0]);
    a2[10] = byteswap32(v26[1]);
    a2[11] = v12;
    sceKernelMemcpy(v27, v6, 8);
    v13 = byteswap32(v27[0]);
    a2[12] = byteswap32(v27[1]);
    a2[13] = v13;
    v14 = v6 + 8;
    sceKernelMemcpy(&v28, v14, 8);
    v33 = ((u64)(v33) & 0xFFFFFFFF00000000ULL) | (u32)(byteswap32(HIDWORD(v28)));
    v33 = ((u64)(v33) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(byteswap32(v28)) << 32);
    v28 = v33;
    if ( v33 != 0 )
    {
      a2[14] = 1000 * v33 - 13910016;
      a2[15] = ((1000 * v33) >> 32) + 14467070 + ((unsigned int)(1000 * v33 - 13910016) < 0xFF2BC000);
      v28 = 1000 * v33;
    }
    else
    {
      sceKernelMemset(a2 + 14, 0, 8);
    }
    v16 = v14 + 8;
    sceKernelMemcpy(&v29, v14 + 8, 4);
    a2[16] = byteswap32(v29);
    sceKernelMemcpy(&v30, v14 + 12, 4);
    v17 = v14 + 16;
    a2[17] = byteswap32(v30);
    sceKernelMemcpy(&v31, v14 + 16, 4);
    a2[18] = byteswap32(v31);
    sceKernelMemcpy(&v32, v14 + 20, 4);
    v18 = a2[1];
    v19 = v14 + 24;
    v20 = v23;
    a2[19] = byteswap32(v32);
    v21 = v16 + 16 - v11;
    a2[20] = v18 + 4;
    a2[5] = v16 + 16;
    result = sceNpCommerce2_E3455E27(v16 + 16, v20 - v21, 106, 0, &v24);
    if ( result >= 0 && v24 != 0 )
      a2[20] = v19 + result;
    if ( result >= 0 )
    {
      v22 = v23;
      a2[21] = a2[1] + 4;
      result = sceNpCommerce2_E3455E27(v17 + 8, v22 - v21, 93, 0, &v25);
      if ( result >= 0 && v25 != 0 )
        a2[21] = v19 + result;
      if ( result >= 0 )
        return sceNpCommerce2_3F6D413C(v17 + 8, v23 - v21, 111, 0, a2 + 22);
    }
  }
  return result;
}


unsigned int regcam_tlv_parse_13(int *a1, int a2, u32 *a3)
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
  int v14;
  u32 v15;
  u64 v16;
  u32 v17;
  int v18;
  int v19;
  int v20;
  u16 v22;
  u16 v23[3];
  u32 v24[2];
  s64 v25;
  u32 v26;
  u32 v27;

  v5 = a1[5];
  v6 = regcam_tlv_parse_54(v5, a1[4] - (v5 - a1[3]), 111, 28, a2, &v22);
  v7 = v6;
  if ( v6 >= 0 )
  {
    v8 = v5 + v6;
    sceKernelMemset(a3, 0, 72);
    v9 = *a1;
    v10 = a1[1];
    v11 = v22;
    sceKernelMemset(a3, 0, 40);
    a3[1] = v10;
    v12 = v8;
    *a3 = v9;
    a3[4] = v11;
    v13 = v8;
    a3[3] = v8;
    v14 = v8 + 8;
    sceKernelMemcpy(v24, v12, 8);
    v15 = byteswap32(v24[0]);
    a3[10] = byteswap32(v24[1]);
    a3[11] = v15;
    sceKernelMemcpy(&v25, v14, 8);
    v16 = __PAIR64__(byteswap32(v25), byteswap32(HIDWORD(v25)));
    v25 = v16;
    if ( v16 != 0 )
    {
      a3[12] = 1000 * v16 - 13910016;
      a3[13] = ((1000 * v16) >> 32) + 14467070 + ((unsigned int)(1000 * v16 - 13910016) < 0xFF2BC000);
      v25 = 1000 * v16;
    }
    else
    {
      sceKernelMemset(a3 + 12, 0, 8);
    }
    sceKernelMemcpy(&v26, v14 + 8, 4);
    a3[14] = byteswap32(v26);
    sceKernelMemcpy(&v27, v14 + 12, 4);
    v17 = byteswap32(v27);
    v18 = v22;
    a3[16] = a3[1] + 4;
    v19 = v14 + 16 - v13;
    a3[15] = v17;
    a3[5] = v14 + 16;
    v20 = sceNpCommerce2_E3455E27(v14 + 16, v18 - v19, 110, 0, v23);
    if ( v20 >= 0 && v23[0] != 0 )
      a3[16] = v14 + 16 + v20;
    v7 = v20;
    if ( v20 >= 0 )
    {
      v7 = sceNpCommerce2_3F6D413C(v14 + 16, v22 - v19, 109, 0, a3 + 17);
      if ( ((v7 != 0x80550F87) & (v7 >> 31)) == 0 )
        return 0;
    }
  }
  return v7;
}


int regcam_tlv_parse_14(int *a1, int a2, u32 *a3)
{
  int v5;
  int result;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  u32 v13;
  u32 v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int *v23;
  int v24;
  int v25;
  u16 v26;
  s16 v27;
  s16 v28;
  s16 v29;
  u16 v30[4];
  u32 v31[2];
  u32 v32[2];
  u32 v33;
  u32 v34;
  u32 v35;
  u32 v36;

  v5 = a1[5];
  result = regcam_tlv_parse_54(v5, a1[4] - (v5 - a1[3]), 109, 93, a2, &v26);
  if ( result >= 0 )
  {
    v7 = v5 + result;
    sceKernelMemset(a3, 0, 96);
    v8 = a1[1];
    v9 = *a1;
    v10 = v26;
    sceKernelMemset(a3, 0, 40);
    a3[1] = v8;
    *a3 = v9;
    v11 = v7;
    a3[4] = v10;
    v12 = v7 + 57;
    a3[3] = v7;
    a3[10] = v7;
    v7 += 65;
    sceKernelMemcpy(v31, v12, 8);
    v13 = byteswap32(v31[0]);
    a3[12] = byteswap32(v31[1]);
    a3[13] = v13;
    sceKernelMemcpy(v32, v7, 8);
    v14 = byteswap32(v32[0]);
    a3[14] = byteswap32(v32[1]);
    a3[15] = v14;
    sceKernelMemcpy(&v33, v7 + 8, 4);
    v15 = v7 + 12;
    a3[16] = -byteswap32(v33);
    sceKernelMemcpy(&v34, v7 + 12, 4);
    v16 = v7 + 16;
    a3[17] = -byteswap32(v34);
    sceKernelMemcpy(&v35, v7 + 16, 4);
    a3[18] = -byteswap32(v35);
    sceKernelMemcpy(&v36, v7 + 20, 4);
    v17 = a3[1];
    v18 = v26;
    v19 = v7 + 24;
    a3[20] = byteswap32(v36);
    v20 = v15 + 12 - v11;
    a3[19] = v17 + 4;
    a3[5] = v16 + 8;
    result = sceNpCommerce2_E3455E27(v16 + 8, v18 - v20, 108, 0, &v27);
    if ( result >= 0 && v27 != 0 )
      a3[19] = v19 + result;
    if ( result >= 0 )
    {
      v21 = v26;
      a3[21] = a3[1] + 4;
      result = sceNpCommerce2_E3455E27(v19, v21 - v20, 4, 0, &v28);
      if ( result >= 0 && v28 != 0 )
        a3[21] = v19 + result;
      if ( result >= 0 )
      {
        v22 = v26;
        a3[22] = a3[1] + 4;
        result = sceNpCommerce2_E3455E27(v19, v22 - v20, 7, 0, &v29);
        if ( result >= 0 && v29 != 0 )
          a3[22] = v19 + result;
        if ( result >= 0 )
        {
          v23 = a3 + 23;
          v24 = 0;
          v25 = v26 - v20;
          if ( a3 != NULL )
            v24 = a3[1] + 4;
          *v23 = v24;
          result = sceNpCommerce2_E3455E27(v19, v25, 8, 0, v30);
          if ( result >= 0 && v30[0] != 0 )
            *v23 = v19 + result;
        }
      }
    }
  }
  return result;
}


int regcam_tlv_parse_15(int *a1, u32 *a2, int a3)
{
  int v3;
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
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  u16 v23;
  s16 v24;
  u32 v25;
  u32 v26;
  u32 v27;

  v3 = 142;
  if ( a3 != 3 )
    v3 = 124;
  v6 = a1[5];
  v7 = sceNpCommerce2_E3455E27(v6, a1[4] - (v6 - a1[3]), v3, 12, &v23);
  v8 = v6 + v7;
  v9 = v7;
  v10 = v6 + v7;
  if ( v7 >= 0 )
  {
    v11 = a2 + 11;
    sceKernelMemset(a2, 0, 56);
    v12 = *a1;
    v13 = v23;
    v14 = a1[1];
    sceKernelMemset(a2, 0, 40);
    a2[1] = v14;
    v15 = v8;
    *a2 = v12;
    v16 = v8 + 4;
    a2[4] = v13;
    v17 = v8 + 8;
    a2[3] = v8;
    v18 = v8 + 12;
    sceKernelMemcpy(&v25, v15, 4);
    a2[13] = byteswap32(v25);
    sceKernelMemcpy(&v26, v16, 4);
    a2[12] = byteswap32(v26);
    sceKernelMemcpy(&v27, v17, 4);
    v19 = v23 - (v18 - v10);
    a2[10] = byteswap32(v27);
    v20 = 0;
    a2[5] = v18;
    if ( a2 != NULL )
      v20 = a2[1] + 4;
    *v11 = v20;
    v21 = sceNpCommerce2_E3455E27(v18, v19, 68, 0, &v24);
    v9 = v21;
    if ( v21 >= 0 && v24 != 0 )
      *v11 = v18 + v21;
    if ( ((v21 != 0x80550F87) & ((unsigned int)v21 >> 31)) == 0 )
      return 0;
  }
  return v9;
}


int regcam_tlv_parse_16(int *a1, int a2, u32 *a3)
{
  int v5;
  int result;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  u32 v13;
  u64 v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int *v21;
  int v22;
  int v23;
  u16 v24;
  s16 v25;
  s16 v26;
  s16 v27;
  s64 v28;
  u32 v29;
  u32 v30[3];

  v5 = a1[3];
  result = regcam_tlv_parse_55(v5, a1[4], 113, 49, a2, &v24, a1[6]);
  if ( result >= 0 )
  {
    v7 = v5 + result;
    sceKernelMemset(a3, 0, 80);
    v8 = a1[1];
    v9 = *a1;
    v10 = v24;
    sceKernelMemset(a3, 0, 40);
    a3[1] = v8;
    v11 = v7;
    a3[3] = v7;
    a3[10] = v7;
    v12 = v7 + 33;
    *a3 = v9;
    a3[4] = v10;
    sceKernelMemcpy(&v28, v12, 8);
    v13 = byteswap32(v28);
    v14 = __PAIR64__(v13, byteswap32(HIDWORD(v28)));
    v28 = __PAIR64__(v13, v14);
    if ( ((unsigned int)v14 | v13) != 0 )
    {
      a3[12] = 1000 * v14 - 13910016;
      a3[13] = ((1000 * v14) >> 32) + 14467070 + ((unsigned int)(1000 * v14 - 13910016) < 0xFF2BC000);
      v28 = 1000 * v14;
    }
    else
    {
      sceKernelMemset(a3 + 12, 0, 8);
    }
    v15 = v12 + 8;
    sceKernelMemcpy(&v29, v12 + 8, 4);
    a3[14] = byteswap32(v29);
    sceKernelMemcpy(v30, v12 + 12, 4);
    v16 = v15 + 8;
    v17 = a3[1] + 4;
    v18 = v24;
    a3[15] = byteswap32(v30[0]);
    v19 = v15 + 8 - v11;
    a3[16] = v17;
    a3[5] = v15 + 8;
    result = sceNpCommerce2_E3455E27(v15 + 8, v18 - v19, 4, 0, &v25);
    if ( result >= 0 && v25 != 0 )
      a3[16] = v16 + result;
    if ( result >= 0 )
    {
      v20 = v24;
      a3[17] = a3[1] + 4;
      result = sceNpCommerce2_E3455E27(v15 + 8, v20 - v19, 5, 0, &v26);
      if ( result >= 0 && v26 != 0 )
        a3[17] = v16 + result;
      if ( result >= 0 )
      {
        v21 = a3 + 18;
        v22 = 0;
        v23 = v24 - v19;
        if ( a3 != NULL )
          v22 = a3[1] + 4;
        *v21 = v22;
        result = sceNpCommerce2_E3455E27(v16, v23, 16, 0, &v27);
        if ( result >= 0 && v27 != 0 )
          *v21 = v16 + result;
      }
    }
  }
  return result;
}


unsigned int regcam_tlv_parse_17(int *a1, int a2, u32 *a3)
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
  int v14;
  u32 v15;
  u64 v16;
  int v18;
  int v19;
  int v20;
  int v21;
  int v23;
  u16 v24;
  s16 v25;
  u32 v26;
  s64 v27;
  u64 v28;
  u32 v29;
  u32 v30;
  u64 v31; // 0:$t4.4,4:$t7.4

  v5 = a1[3];
  v6 = regcam_tlv_parse_55(v5, a1[4], 114, 81, a2, &v24, a1[6]);
  v7 = v6;
  if ( v6 >= 0 )
  {
    v8 = v5 + v6;
    sceKernelMemset(a3, 0, 88);
    v9 = *a1;
    v10 = v24;
    v11 = a1[1];
    sceKernelMemset(a3, 0, 40);
    a3[3] = v8;
    a3[10] = v8;
    v12 = v8;
    a3[1] = v11;
    a3[4] = v10;
    v13 = v8 + 106;
    a3[11] = v8 + 57;
    v14 = v8 + 110;
    *a3 = v9;
    sceKernelMemcpy(&v26, v13, 4);
    a3[12] = byteswap32(v26) != 0;
    sceKernelMemcpy(&v27, v14, 8);
    v15 = byteswap32(v27);
    v16 = __PAIR64__(v15, byteswap32(HIDWORD(v27)));
    v27 = __PAIR64__(v15, v16);
    if ( ((unsigned int)v16 | v15) != 0 )
    {
      a3[14] = 1000 * v16 - 13910016;
      a3[15] = ((1000 * v16) >> 32) + 14467070 + ((unsigned int)(1000 * v16 - 13910016) < 0xFF2BC000);
      v27 = 1000 * v16;
    }
    else
    {
      sceKernelMemset(a3 + 14, 0, 8);
    }
    v23 = v14 + 8;
    sceKernelMemcpy(&v28, v23, 8);
    v31 = ((u64)(v31) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(byteswap32(v28)) << 32);
    v31 = ((u64)(v31) & 0xFFFFFFFF00000000ULL) | (u32)(byteswap32(HIDWORD(v28)));
    v28 = v31;
    if ( ((unsigned int)v31 | HIDWORD(v31)) != 0 )
    {
      a3[16] = 1000 * v31 - 13910016;
      a3[17] = ((1000 * v31) >> 32) + 14467070 + ((unsigned int)(1000 * v31 - 13910016) < 0xFF2BC000);
      v28 = 1000 * v31;
    }
    else
    {
      sceKernelMemset(a3 + 16, 0, 8);
    }
    sceKernelMemcpy(&v29, v23 + 8, 4);
    a3[18] = byteswap32(v29);
    sceKernelMemcpy(&v30, v23 + 12, 4);
    v18 = a3[1];
    v19 = v24;
    a3[19] = byteswap32(v30);
    v20 = v23 + 16 - v12;
    a3[20] = v18 + 4;
    a3[5] = v23 + 16;
    v21 = sceNpCommerce2_E3455E27(v23 + 16, v19 - v20, 121, 0, &v25);
    if ( v21 >= 0 && v25 != 0 )
      a3[20] = v23 + 16 + v21;
    v7 = v21;
    if ( v21 >= 0 )
    {
      v7 = sceNpCommerce2_3F6D413C(v23 + 16, v24 - v20, 128, 0, a3 + 21);
      if ( ((v7 != 0x80550F87) & (v7 >> 31)) == 0 )
        return 0;
    }
  }
  return v7;
}


int regcam_tlv_parse_18(int *a1, int a2, u32 *a3)
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
  u32 v15;
  u64 v16;
  int v17;
  u32 v18;
  u64 v19;
  int v20;
  u32 v21;
  u64 v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  u16 v32;
  s16 v33;
  s16 v34;
  s16 v35;
  u32 v36;
  s64 v37;
  u32 v38;
  u32 v39;
  s64 v40;
  s64 v41;

  v5 = a1[3];
  v6 = regcam_tlv_parse_55(v5, a1[4], 115, 85, a2, &v32, a1[6]);
  v7 = v6;
  if ( v6 >= 0 )
  {
    v8 = v5 + v6;
    sceKernelMemset(a3, 0, 112);
    v9 = *a1;
    v10 = v32;
    v11 = a1[1];
    sceKernelMemset(a3, 0, 40);
    a3[3] = v8;
    a3[10] = v8;
    v12 = v8;
    a3[1] = v11;
    a3[4] = v10;
    v13 = v8 + 114;
    a3[11] = v8 + 57;
    v14 = v8 + 118;
    *a3 = v9;
    sceKernelMemcpy(&v36, v13, 4);
    a3[12] = byteswap32(v36);
    sceKernelMemcpy(&v37, v14, 8);
    v15 = byteswap32(v37);
    v16 = __PAIR64__(v15, byteswap32(HIDWORD(v37)));
    v37 = __PAIR64__(v15, v16);
    if ( ((unsigned int)v16 | v15) != 0 )
    {
      a3[14] = 1000 * v16 - 13910016;
      a3[15] = ((1000 * v16) >> 32) + 14467070 + ((unsigned int)(1000 * v16 - 13910016) < 0xFF2BC000);
      v37 = 1000 * v16;
    }
    else
    {
      sceKernelMemset(a3 + 14, 0, 8);
    }
    sceKernelMemcpy(&v38, v14 + 8, 4);
    a3[16] = byteswap32(v38);
    sceKernelMemcpy(&v39, v14 + 12, 4);
    v17 = v14 + 16;
    a3[17] = byteswap32(v39) != 0;
    sceKernelMemcpy(&v40, v17, 8);
    v18 = byteswap32(v40);
    v19 = __PAIR64__(v18, byteswap32(HIDWORD(v40)));
    v40 = __PAIR64__(v18, v19);
    if ( ((unsigned int)v19 | v18) != 0 )
    {
      a3[18] = 1000 * v19 - 13910016;
      a3[19] = ((1000 * v19) >> 32) + 14467070 + ((unsigned int)(1000 * v19 - 13910016) < 0xFF2BC000);
      v40 = 1000 * v19;
    }
    else
    {
      sceKernelMemset(a3 + 18, 0, 8);
    }
    v20 = v17 + 8;
    sceKernelMemcpy(&v41, v20, 8);
    v21 = byteswap32(v41);
    v22 = __PAIR64__(v21, byteswap32(HIDWORD(v41)));
    v41 = __PAIR64__(v21, v22);
    if ( ((unsigned int)v22 | v21) != 0 )
    {
      a3[20] = 1000 * v22 - 13910016;
      a3[21] = ((1000 * v22) >> 32) + 14467070 + ((unsigned int)(1000 * v22 - 13910016) < 0xFF2BC000);
      v41 = 1000 * v22;
    }
    else
    {
      sceKernelMemset(a3 + 20, 0, 8);
    }
    v23 = v32;
    v24 = v20 + 8;
    a3[22] = a3[1] + 4;
    v25 = v24 - v12;
    a3[5] = v24;
    v26 = sceNpCommerce2_E3455E27(v24, v23 - (v24 - v12), 7, 0, &v33);
    if ( v26 >= 0 && v33 != 0 )
      a3[22] = v24 + v26;
    v7 = v26;
    if ( v26 >= 0 )
    {
      v27 = v32;
      a3[23] = a3[1] + 4;
      v28 = sceNpCommerce2_E3455E27(v24, v27 - v25, 130, 0, &v34);
      if ( v28 >= 0 && v34 != 0 )
        a3[23] = v24 + v28;
      v7 = v28;
      if ( ((v28 != 0x80550F87) & ((unsigned int)v28 >> 31)) == 0 )
      {
        v29 = v32;
        a3[24] = a3[1] + 4;
        v30 = sceNpCommerce2_E3455E27(v24, v29 - v25, 8, 0, &v35);
        if ( v30 >= 0 && v35 != 0 )
          a3[24] = v24 + v30;
        v7 = v30;
        if ( v30 >= 0 )
        {
          v7 = sceNpCommerce2_3F6D413C(v24, v32 - v25, 123, 0, a3 + 25);
          if ( v7 >= 0 )
          {
            v7 = sceNpCommerce2_3F6D413C(v24, v32 - v25, 128, 0, a3 + 26);
            if ( ((v7 != 0x80550F87) & ((unsigned int)v7 >> 31)) == 0 )
              return 0;
          }
        }
      }
    }
  }
  return v7;
}


int regcam_tlv_parse_19(int *a1, int a2, u32 *a3)
{
  int v5;
  int result;
  int v7;
  int v8;
  int *v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  u16 v19;
  s16 v20;
  u32 v21;
  u32 v22;
  u32 v23;

  v5 = a1[5];
  result = regcam_tlv_parse_54(v5, a1[4] - (v5 - a1[3]), 123, 73, a2, &v19);
  v7 = v5 + result;
  v8 = v5 + result;
  if ( result >= 0 )
  {
    v9 = a3 + 14;
    sceKernelMemset(a3, 0, 60);
    v10 = *a1;
    v11 = v19;
    v12 = a1[1];
    sceKernelMemset(a3, 0, 40);
    a3[1] = v12;
    *a3 = v10;
    v13 = v7 + 57;
    a3[4] = v11;
    v14 = v7 + 61;
    v15 = v7 + 65;
    a3[3] = v7;
    a3[10] = v7;
    v16 = v7 + 69;
    sceKernelMemcpy(&v21, v13, 4);
    a3[11] = byteswap32(v21);
    sceKernelMemcpy(&v22, v14, 4);
    a3[12] = byteswap32(v22);
    sceKernelMemcpy(&v23, v15, 4);
    v17 = v19 - (v16 - v8);
    a3[13] = byteswap32(v23);
    v18 = 0;
    a3[5] = v16;
    if ( a3 != NULL )
      v18 = a3[1] + 4;
    *v9 = v18;
    result = sceNpCommerce2_E3455E27(v16, v17, 8, 0, &v20);
    if ( result >= 0 && v20 != 0 )
      *v9 = v16 + result;
  }
  return result;
}


int regcam_tlv_parse_20(u32 *a1, int a2, int a3)
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

  v5 = regcam_tlv_parse_53(a2, a3, &v30, v31, 0);
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


int regcam_tlv_parse_21(int a1, int a2, u32 *a3)
{
  int result;
  int v6;
  int v7;
  int v8;

  v8 = 0;
  result = regcam_tlv_parse_53(a1, a2, &v6, &v7, 0);
  if ( result >= 0 )
  {
    *a3 = sceNpCommerce2_3F6D413C(a1 + result, v7, 120, 0, &v8) >= 0;
    return 0;
  }
  return result;
}


s32 regcam_tlv_parse_22(u32 *a1, int a2, int a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v11;
  u32 v12[3];

  v5 = regcam_tlv_parse_53(a2, a3, &v11, v12, 0);
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
    return regcam_min_s32(sceNpCommerce2_3F6D413C(v6, v9, 22, 0, a1 + 10), 0);
  }
  return v5;
}


int regcam_tlv_parse_23(u32 *a1, int a2)
{
  return regcam_tlv_parse_58(*a1, a1[1], a1[3], a1[4], a1[5], a2);
}


int regcam_tlv_parse_24(u32 *a1, int a2)
{
  return regcam_tlv_parse_59(*a1, a1[1], a1[3], a1[4], a1[5], a2);
}


s32 regcam_tlv_parse_25(u32 *a1, int a2, int a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v11;
  u32 v12[3];

  v5 = regcam_tlv_parse_53(a2, a3, &v11, v12, 0);
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
    return regcam_min_s32(sceNpCommerce2_3F6D413C(v6, v9, 55, 0, a1 + 10), 0);
  }
  return v5;
}


int regcam_tlv_parse_26(int a1, int a2, int a3)
{
  return regcam_tlv_parse_60(a1, a2, a3);
}


int regcam_tlv_parse_27(int *a1, int a2, u32 *a3)
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
  result = regcam_tlv_parse_54(v5, a1[4] - (v5 - a1[3]), 59, 69, a2, &v15);
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


s32 regcam_tlv_parse_28(u32 *a1, int a2, int a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v11;
  u32 v12[3];

  v5 = regcam_tlv_parse_53(a2, a3, &v11, v12, 0);
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
    return regcam_min_s32(sceNpCommerce2_3F6D413C(v6, v9, 91, 0, a1 + 10), 0);
  }
  return v5;
}


int regcam_tlv_parse_29(u32 *a1, int a2, int a3)
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

  v5 = regcam_tlv_parse_53(a2, a3, &v14, &v15, 0);
  v6 = a2 + v5;
  if ( v5 >= 0 )
  {
    sceKernelMemset(a1, 0, 48);
    v7 = v14;
    v8 = v15;
    sceKernelMemset(a1, 0, 40);
    v9 = v15;
    v10 = v6 - a2 + v15;
    *a1 = v7;
    a1[2] = v10;
    a1[11] = a2 + 4;
    a1[4] = v8;
    a1[1] = a2;
    a1[3] = v6;
    v11 = sceNpCommerce2_E3455E27(v6, v9, 164, 0, &v13);
    v5 = v11;
    if ( v11 >= 0 && v13 != 0 )
      a1[11] = v6 + v11;
    if ( ((v11 != 0x80550F87) & ((unsigned int)v11 >> 31)) == 0 )
    {
      if ( v11 != -2141909113 )
        return 0;
      v5 = sceNpCommerce2_3F6D413C(v6, v15, 147, 0, a1 + 10);
      if ( ((v5 != 0x80550F87) & ((unsigned int)v5 >> 31)) == 0 )
        return 0;
    }
  }
  return v5;
}


int regcam_tlv_parse_30(int *a1, int a2, u32 *a3)
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
  v6 = regcam_tlv_parse_54(v5, a1[4], 147, 4, a2, &v13);
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


int regcam_tlv_parse_31(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11[3];

  result = regcam_tlv_parse_53(a2, a3, &v10, v11, 0);
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


s32 regcam_tlv_parse_32(u32 *a1, int a2, int a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v11;
  u32 v12[3];

  v5 = regcam_tlv_parse_53(a2, a3, &v11, v12, 0);
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
    return regcam_min_s32(sceNpCommerce2_3F6D413C(v6, v9, 97, 0, a1 + 10), 0);
  }
  return v5;
}


int regcam_tlv_parse_33(int a1, int a2, int a3)
{
  int v5;
  int result;
  u16 v7[8];

  v5 = *(u32 *)(a1 + 12);
  result = regcam_tlv_parse_54(v5, *(u32 *)(a1 + 16), 97, 8, a2, v7);
  if ( result >= 0 )
    return regcam_tlv_parse_62(a1, v5 + result, v5 + result, v7[0], a3);
  return result;
}


int regcam_tlv_parse_34(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11[3];

  result = regcam_tlv_parse_53(a2, a3, &v10, v11, 0);
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


int regcam_tlv_parse_35(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11[3];

  result = regcam_tlv_parse_53(a2, a3, &v10, v11, 0);
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


int regcam_tlv_parse_36(int *a1, u32 *a2)
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
  v5 = sceNpCommerce2_E3455E27(v4, a1[4], 104, 8, &v12);
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


int regcam_tlv_parse_37(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11[3];

  result = regcam_tlv_parse_53(a2, a3, &v10, v11, 0);
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


int regcam_tlv_parse_38(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11[3];

  result = regcam_tlv_parse_53(a2, a3, &v10, v11, 0);
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


int regcam_tlv_parse_39(int a1, int a2, int a3)
{
  return regcam_tlv_parse_57(a1, a2, a3);
}


int regcam_tlv_parse_40(int a1, int a2, u32 *a3)
{
  int v4;
  int v5;
  int v7;
  u32 v8[3];

  v4 = *(u32 *)(a1 + 12);
  v5 = regcam_tlv_parse_54(v4, *(u32 *)(a1 + 16), 88, 8, a2, &v7);
  if ( v5 >= 0 )
  {
    sceKernelMemcpy(v8, v4 + v5, 4);
    *a3 = byteswap32(v8[0]);
  }
  return v5;
}


s32 regcam_tlv_parse_41(u32 *a1, int a2, int a3)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v11;
  u32 v12[3];

  v5 = regcam_tlv_parse_53(a2, a3, &v11, v12, 0);
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
    return regcam_min_s32(sceNpCommerce2_3F6D413C(v6, v9, 107, 0, a1 + 10), 0);
  }
  return v5;
}


int regcam_tlv_parse_42(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11[3];

  result = regcam_tlv_parse_53(a2, a3, &v10, v11, 0);
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


s32 regcam_tlv_parse_43(u32 *a1, int a2, int a3)
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
  v6 = regcam_tlv_parse_53(a2, a3, &v13, &v14, &v15);
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
    v6 = sceNpCommerce2_69FEA1C8(v7, v10, 113, 0, v4, v11);
    if ( v6 >= 0 )
      return regcam_min_s32(regcam_tlv_parse_56(a1, 113, 49, v4), 0);
  }
  return v6;
}


s32 regcam_tlv_parse_44(u32 *a1, int a2, int a3)
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
  v6 = regcam_tlv_parse_53(a2, a3, &v13, &v14, &v15);
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
    v6 = sceNpCommerce2_69FEA1C8(v7, v10, 114, 0, v4, v11);
    if ( v6 >= 0 )
      return regcam_min_s32(regcam_tlv_parse_56(a1, 114, 81, v4), 0);
  }
  return v6;
}


s32 regcam_tlv_parse_45(u32 *a1, int a2, int a3)
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
  v6 = regcam_tlv_parse_53(a2, a3, &v13, &v14, &v15);
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
    v6 = sceNpCommerce2_69FEA1C8(v7, v10, 115, 0, v4, v11);
    if ( v6 >= 0 )
      return regcam_min_s32(regcam_tlv_parse_56(a1, 115, 85, v4), 0);
  }
  return v6;
}


int regcam_tlv_parse_46(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11[3];

  result = regcam_tlv_parse_53(a2, a3, &v10, v11, 0);
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


int regcam_tlv_parse_47(int a1, int a2)
{
  int v4;
  int result;
  u16 v6[8];

  v4 = *(u32 *)(a1 + 12);
  result = sceNpCommerce2_E3455E27(v4, *(u32 *)(a1 + 16), 97, 8, v6);
  if ( result >= 0 )
    return regcam_tlv_parse_62(a1, v4 + result, v4 + result, v6[0], a2);
  return result;
}


int regcam_tlv_parse_48(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11[3];

  result = regcam_tlv_parse_53(a2, a3, &v10, v11, 0);
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


int regcam_tlv_parse_49(int a1, int a2, int a3)
{
  return regcam_tlv_parse_60(a1, a2, a3);
}


int regcam_tlv_parse_50(int a1, int a2, int a3)
{
  return regcam_tlv_parse_61(a1, a2, a3);
}


int regcam_tlv_parse_51(int *a1, u32 *a2)
{
  return regcam_tlv_parse_2(a1, a2);
}


int regcam_tlv_parse_52(u32 *a1, int a2, int a3)
{
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11[3];

  result = regcam_tlv_parse_53(a2, a3, &v10, v11, 0);
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


int regcam_tlv_parse_53(int a1, unsigned int a2, u32 *a3, u32 *a4, u32 *a5)
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


int regcam_tlv_parse_54(
        int a1,
        int a2,
        u16 a3,
        u16 a4,
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


int regcam_tlv_parse_55(int a1, int a2, u16 a3, u16 a4, unsigned int a5, u16 *a6, int a7)
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


int regcam_tlv_parse_56(u32 *a1, u16 a2, u16 a3, int a4)
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
    v4 = regcam_tlv_parse_55(i, a1[4], a2, a3, v9, v13, a1[6]);
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


int regcam_tlv_parse_57(int *a1, unsigned int a2, int a3)
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
  result = regcam_tlv_parse_54(v5, a1[4], 0x16u, 0x2Au, a2, &v22);
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


s32 regcam_tlv_parse_58(int a1, int a2, int a3, int a4, int a5, u32 *a6)
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
          return regcam_min_s32(regcam_tlv_parse_63(a6, a6 + 15, a6 + 16, a6 + 17, a6 + 18), 0);
      }
    }
  }
  return v11;
}


s32 regcam_tlv_parse_59(int a1, int a2, int a3, int a4, int a5, u32 *a6)
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
      return regcam_min_s32(regcam_tlv_parse_63(a6, a6 + 13, a6 + 14, a6 + 15, a6 + 16), 0);
  }
  return v12;
}


int regcam_tlv_parse_60(int *a1, unsigned int a2, u32 *a3)
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
  v6 = regcam_tlv_parse_54(v5, a1[4], 0x37u, 0x49u, a2, &v18);
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


int regcam_tlv_parse_61(int *a1, unsigned int a2, u32 *a3)
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
  result = regcam_tlv_parse_54(v5, a1[4], 0x39u, 0x19u, a2, &v16);
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


int regcam_tlv_parse_62(int *a1, int a2, int a3, u16 a4, u32 *a5)
{
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int *v14;
  int v15;
  int result;
  s16 v17;
  u32 v18[3];

  v9 = a4;
  sceKernelMemset(a5, 0, 48);
  v10 = a1[1];
  v11 = *a1;
  sceKernelMemset(a5, 0, 40);
  a5[1] = v10;
  v12 = a3;
  a5[3] = a3;
  v13 = a3 + 4;
  *a5 = v11;
  v14 = a5 + 11;
  a5[4] = v9;
  sceKernelMemcpy(v18, v12, 4);
  a5[10] = byteswap32(v18[0]);
  v15 = 0;
  a5[5] = v13;
  if ( a5 != NULL )
    v15 = a5[1] + 4;
  *v14 = v15;
  result = sceNpCommerce2_E3455E27(v13, v9 - (v13 - a2), 96, 0, &v17);
  if ( result >= 0 && v17 != 0 )
    *v14 = v13 + result;
  return result;
}


int regcam_tlv_parse_63(u32 *a1, u32 *a2, u32 *a3, u32 *a4, u32 *a5)
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


