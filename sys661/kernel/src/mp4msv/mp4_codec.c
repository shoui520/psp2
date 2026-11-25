#include "mp4msv_internal.h"

int mp4_codec_fn_1(u32 *a1, unsigned int a2, u32 *a3, int *a4, int *a5)
{
  u32 *v8;
  unsigned int v9;
  int v10;
  int v12;
  int v13;
  int *v14;
  int v15;
  int v16;
  int v17;
  unsigned int v18;
  unsigned int v19;
  unsigned int v20;
  int v21;
  unsigned int v22;
  int v23;
  unsigned int v24;
  int v25;
  int v26;
  int v27;
  unsigned int v28;
  unsigned int i;
  unsigned int v30;
  unsigned int v31;
  int v32;
  int v33;
  unsigned int v34;
  int v35;
  int v36;
  int v37;
  int v38;
  int v39;
  int v40;

  v8 = (u32 *)a1[70];
  v9 = v8[1];
  if ( a2 >= v9 + a1[53] || a2 < v9 )
    return 4110;
  v10 = 4110;
  if ( a1[35] == 0 )
    return v10;
  if ( a1[37] == 0 )
    a1[38] = *v8;
  v12 = mp4_codec_fn_4(a1 + 31, a1[37], 0);
  v13 = v12 - 1;
  if ( a2 < v12 - 1 )
  {
    v14 = (int *)a1[70];
    if ( a2 - v14[1] >= v13 - a2 )
    {
      v15 = a1[37];
      goto LABEL_14;
    }
    a1[37] = 0;
    v16 = *v14;
    goto LABEL_12;
  }
  if ( v12 - 1 < a2 )
  {
    v36 = a1[35] - 1;
    if ( a1[37] == v36 )
      goto LABEL_13;
    v37 = a1[70];
    v38 = a1[53];
    if ( *(u32 *)(v37 + 4) + v38 - a2 < a2 - v13 )
    {
      a1[37] = v36;
      v39 = *(u32 *)(v37 + 4) + v38 + 1;
      v40 = mp4_codec_fn_4(a1 + 31, v36, 0);
      v16 = *(u32 *)a1[70] + a1[44] - (v39 - v40) * mp4_codec_fn_4(a1 + 31, a1[35] - 1, 1);
LABEL_12:
      a1[38] = v16;
LABEL_13:
      v15 = a1[37];
      goto LABEL_14;
    }
    v15 = a1[37];
  }
  else
  {
    v15 = a1[37];
  }
LABEL_14:
  v17 = mp4_codec_fn_4(a1 + 31, v15, 0);
  if ( a2 >= v17 - 1 )
  {
    v18 = a1[37];
    v19 = a1[35];
    v20 = v17 - 1;
    v21 = a1[38];
    if ( v18 < v19 )
    {
      while ( 1 )
      {
        v22 = v18 == v19 - 1 ? *(u32 *)(a1[70] + 4) + a1[53] : mp4_codec_fn_4(a1 + 31, v18 + 1, 0) - 1;
        v23 = mp4_codec_fn_4(a1 + 31, v18, 1);
        v24 = v22 - v20;
        if ( a2 < v22 )
          break;
        v19 = a1[35];
        ++v18;
        v20 = v22;
        v21 += v24 * v23;
        if ( v18 >= v19 )
          return 8195;
      }
      *a3 = v21 + (a2 - v20) * v23;
      *a4 = v23;
      v25 = mp4_codec_fn_4(a1 + 31, v18, 2);
      v10 = 0;
      *a5 = v25;
      a1[38] = v21;
      a1[37] = v18;
      return v10;
    }
    return 8195;
  }
  v26 = v17 - 1;
  v27 = a1[38];
  v28 = 0;
  if ( a1[37] == 0 )
    return 8195;
  for ( i = a1[37]; ; i = v34 - v28 )
  {
    v30 = i - 1;
    v31 = mp4_codec_fn_4(a1 + 31, i - 1, 0) - 1;
    v32 = mp4_codec_fn_4(a1 + 31, v30, 1);
    v33 = (v26 - v31) * v32;
    ++v28;
    v26 = v31;
    v27 -= v33;
    if ( a2 >= v31 )
      break;
    v34 = a1[37];
    if ( v28 >= v34 )
      return 8195;
  }
  *a3 = v27 + (a2 - v31) * v32;
  *a4 = v32;
  v35 = mp4_codec_fn_4(a1 + 31, v30, 2);
  v10 = 0;
  *a5 = v35;
  a1[38] = v27;
  a1[37] = v30;
  return v10;
}


int mp4_codec_fn_2(u32 *a1, unsigned int a2, u32 *a3, u32 *a4)
{
  unsigned int *v6;
  int v7;
  unsigned int v8;
  int v9;
  unsigned int v11;
  unsigned int v12;
  u32 *v13;
  unsigned int v14;
  int v15;
  unsigned int v16;
  unsigned int v17;
  int v18;
  unsigned int v19;
  int v20;
  int v21;
  unsigned int v22;
  unsigned int v23;
  unsigned int v24;
  u32 *v25;
  unsigned int v26;
  unsigned int v27;
  unsigned int v28;
  int v29;
  unsigned int v30;
  unsigned int v31;
  int v32;
  int v33;
  int v34;
  int v35;
  int v36;
  u32 *v37;
  u32 *v38;
  unsigned int v39;
  int v40;
  unsigned int v41;

  v6 = (unsigned int *)a1[70];
  v7 = a1[44];
  v37 = a3;
  v8 = *v6;
  v38 = a4;
  v39 = 0;
  v40 = 0;
  v41 = 0;
  if ( a2 >= v8 + v7 || a2 < v8 )
    return 8201;
  if ( a1[31] == 0 )
    return 8195;
  if ( a1[49] == 0 )
    return 8195;
  v11 = a1[37];
  if ( v11 == 0 )
    a1[38] = v8;
  v12 = a1[38];
  if ( a2 >= v12 )
  {
    v13 = a1 + 31;
    if ( *v6 + v7 - a2 < a2 - v12 )
    {
      v32 = a1[35] - 1;
      if ( v11 != v32 )
      {
        a1[37] = v32;
        v36 = v6[1] + a1[53] + 1;
        v33 = mp4_codec_fn_4(a1 + 31, v32, 0);
        v34 = mp4_codec_fn_4(a1 + 31, a1[35] - 1, 1);
        v11 = a1[37];
        v35 = *(u32 *)a1[70] + a1[44] - (v36 - v33) * v34;
        a1[38] = v35;
        v12 = v35;
      }
    }
  }
  else
  {
    v13 = a1 + 31;
    if ( a2 - *v6 < v12 - a2 )
    {
      a1[37] = 0;
      v11 = 0;
      v12 = *v6;
      a1[38] = *v6;
    }
  }
  if ( a2 < v12 )
  {
    v27 = v12;
    v28 = v11 - 1;
    v29 = mp4_codec_fn_4(v13, v11, 0);
    v30 = 0;
    v31 = v28;
    while ( 1 )
    {
      v36 = mp4_codec_fn_4(v13, v31, 0);
      ++v30;
      v22 = mp4_codec_fn_4(v13, v31, 1);
      v27 -= (v29 - v36) * v22;
      v29 = v36;
      if ( a2 >= v27 )
        break;
      v31 = v28 - v30;
      if ( v28 < v30 )
        return 8195;
    }
    v41 = v31;
    v39 = v27;
    v40 = v36 - 1;
  }
  else
  {
    v14 = v11;
    v16 = v12;
    v15 = mp4_codec_fn_4(v13, v11, 0);
    v17 = a1[35];
    v18 = v15;
    v19 = v14;
    if ( v14 >= v17 )
      return 8195;
    while ( 1 )
    {
      if ( v19 == v17 - 1 )
      {
        mp4_codec_fn_26(a1, &v36);
        ++v36;
      }
      else
      {
        v36 = mp4_codec_fn_4(v13, v19 + 1, 0);
      }
      v20 = mp4_codec_fn_4(v13, v19, 1);
      v21 = (v36 - v18) * v20;
      v22 = v20;
      if ( a2 < v16 + v21 )
        break;
      v17 = a1[35];
      ++v19;
      v16 += v21;
      v18 = v36;
      if ( v19 >= v17 )
        return 8195;
    }
    v41 = v19;
    v39 = v16;
    v40 = v18 - 1;
  }
  if ( v22 == 0 )
    _break(0, 7u);
  v23 = v39;
  v24 = (a2 - v39) / v22;
  v25 = v38;
  v26 = v41;
  v9 = 0;
  *v37 = v40 + v24;
  *v25 = v23 + v24 * v22;
  a1[38] = v23;
  a1[37] = v26;
  return v9;
}


int mp4_codec_fn_3(u32 *a1, unsigned int a2, int a3, int a4)
{
  u32 *v7;
  int v9;
  int v10;

  v7 = (u32 *)mp4_codec_fn_142(a1);
  if ( mp4_box_fn_21(v7) )
    return mp4_api_fn_30(a1[71], a2, a3, a4);
  v9 = 8195;
  if ( a1[40] != 0 )
  {
    v10 = a1[70];
    v9 = 7;
    if ( v10 != 0 )
    {
      v9 = 8201;
      if ( a2 >= *(u32 *)(v10 + 56) )
      {
        v9 = mp4_codec_fn_15(a1, a2, a4);
        if ( v9 == 0 )
          return mp4_codec_fn_27(a1, a2, a3);
      }
    }
  }
  return v9;
}


int mp4_codec_fn_4(int a1, int a2, int a3)
{
  int v3;
  int v4;
  u32 v6[4];

  v3 = -1;
  if ( a1 != 0 )
  {
    v4 = mp4_codec_fn_91(*(u32 *)(a1 + 20), a2, a3, v6);
    v3 = -1;
    if ( v4 == 0 )
      return v6[0];
  }
  return v3;
}


int mp4_codec_fn_5(int a1, int a2, int a3)
{
  int v3;
  int v4;
  u32 v6[4];

  v3 = -1;
  if ( a1 != 0 )
  {
    v4 = mp4_codec_fn_91(*(u32 *)(a1 + 20), a2, a3, v6);
    v3 = -1;
    if ( v4 == 0 )
      return v6[0];
  }
  return v3;
}


int mp4_codec_fn_6(int a1, int a2, int a3)
{
  int v3;
  int v4;
  u32 v6[4];

  v3 = -1;
  if ( a1 != 0 )
  {
    v4 = mp4_codec_fn_91(*(u32 *)(a1 + 20), a2, a3, v6);
    v3 = -1;
    if ( v4 == 0 )
      return v6[0];
  }
  return v3;
}


int mp4_codec_fn_7(int a1, unsigned int a2, int a3)
{
  int v4;
  unsigned int v7;
  int v8;
  int v9;
  int v11;

  v4 = 20495;
  if ( a1 != 0 )
  {
    v7 = *(u32 *)(a1 + 16);
    if ( a2 >= v7 )
      return mp4_codec_fn_111();
    v8 = *(u32 *)(a1 + 12);
    v4 = 0;
    if ( v8 == 0 )
      return mp4_codec_fn_90(a1 + 20, a2, 1, 0, a3);
    v9 = v7 < 2;
    if ( v8 != a3 )
    {
      if ( v9 )
      {
        *(u32 *)(a1 + 12) = a3;
        return v4;
      }
      v11 = mp4_codec_fn_112();
      v4 = 1;
      if ( v11 != 0 )
        return v4;
      return mp4_codec_fn_90(a1 + 20, a2, 1, 0, a3);
    }
  }
  return v4;
}


int mp4_codec_fn_8(int a1, unsigned int a2, int a3, int a4)
{
  int result;

  result = 20495;
  if ( a1 != 0 )
  {
    if ( a2 < *(u32 *)(a1 + 16) )
      return mp4_codec_fn_95(a1 + 20, a2, 1, 0, a3, a4);
    else
      return mp4_codec_fn_137();
  }
  return result;
}


int mp4_codec_fn_9(int a1, unsigned int a2, int a3, int a4)
{
  int result;
  u32 v5[4];

  result = 20495;
  if ( a1 != 0 )
  {
    if ( a2 < *(u32 *)(a1 + 16) )
    {
      return mp4_codec_fn_90(a1 + 20, a2, 3, a3, a4);
    }
    else
    {
      memset(v5, 0, 12);
      v5[a3] = a4;
      return mp4_codec_fn_138(a1, v5, 3);
    }
  }
  return result;
}


int mp4_codec_fn_10(int a1, unsigned int a2, int a3, int a4)
{
  int result;
  u32 v5[4];

  result = 20495;
  if ( a1 != 0 )
  {
    if ( a2 < *(u32 *)(a1 + 16) )
    {
      return mp4_codec_fn_90(a1 + 20, a2, 2, a3, a4);
    }
    else
    {
      v5[0] = 0;
      v5[1] = 0;
      v5[a3] = a4;
      return mp4_codec_fn_139(a1, v5);
    }
  }
  return result;
}


int mp4_codec_fn_11(int a1, unsigned int a2, int a3)
{
  int result;

  result = 20495;
  if ( a1 != 0 )
  {
    if ( a2 < *(u32 *)(a1 + 16) )
      return mp4_codec_fn_90(a1 + 20, a2, 1, 0, a3);
    else
      return mp4_codec_fn_141(a1, a3, 1);
  }
  return result;
}


int mp4_codec_fn_12(int a1, unsigned int a2, int a3, int a4)
{
  int result;
  u32 v5[4];

  result = 20495;
  if ( a1 != 0 )
  {
    if ( a2 < *(u32 *)(a1 + 16) )
    {
      return mp4_codec_fn_90(a1 + 20, a2, 2, a3, a4);
    }
    else
    {
      memset(v5, 0, 12);
      v5[a3] = a4;
      return mp4_codec_fn_140(a1, v5, 2);
    }
  }
  return result;
}


int mp4_codec_fn_13(int a1, unsigned int a2, int a3, int a4)
{
  int v4;
  int v7;

  v4 = 7;
  v7 = *(u32 *)(a1 + 280);
  if ( v7 != 0 )
  {
    v4 = 8201;
    if ( a2 >= *(u32 *)(v7 + 56) )
    {
      v4 = mp4_codec_fn_15(a1, a2, a4);
      if ( v4 == 0 )
        return mp4_codec_fn_14(a1, a2, a3);
    }
  }
  return v4;
}


int mp4_codec_fn_14(int a1, int a2, int a3)
{
  int v4;

  return mp4_codec_fn_18(a1, a2, &v4, a3);
}


int mp4_codec_fn_15(int *a1, unsigned int a2, int a3)
{
  unsigned int *v5;
  unsigned int v6;
  int v7;
  int v9;
  unsigned int v10;
  int v11;
  int v12;
  int v13;

  if ( a1 == NULL )
    return 7;
  v5 = (unsigned int *)a1[70];
  if ( v5 == NULL )
    return 7;
  v6 = *v5;
  v9 = a1[44];
  v10 = v6 + v9;
  if ( v5[10] != 1 || (v7 = 8201, a2 < v5[11]) )
  {
    v11 = a2 < v6;
    v7 = 0;
    if ( v11 || a2 >= v10 )
    {
      v7 = 2;
      if ( a3 != 0 )
      {
        if ( v11 )
        {
          v13 = mp4_api_fn_66(a1);
          v7 = 10;
          if ( v13 == 0 )
            goto LABEL_12;
        }
        else
        {
          while ( 1 )
          {
            v7 = 0;
            if ( a2 >= *v5 && a2 < *v5 + v9 )
              break;
            v12 = mp4_api_fn_73((int)a1);
            if ( v12 != 0 )
            {
              v7 = 8201;
              if ( v12 != 0x5010 )
                return v12;
              return v7;
            }
LABEL_12:
            v5 = (unsigned int *)a1[70];
            v9 = a1[44];
          }
        }
      }
    }
  }
  return v7;
}


int mp4_codec_fn_16(int a1, unsigned int a2, unsigned int a3, u32 *a4)
{
  unsigned int v5;
  unsigned int v6;
  int v7;
  int v8;
  int v9;
  int v10;

  v5 = **(u32 **)(a1 + 280);
  v6 = v5 + *(u32 *)(a1 + 176) - 1;
  v7 = v6 < a2;
  v8 = v6 < a3;
  v9 = 8201;
  if ( a2 >= v5 && !v7 && a3 >= v5 && !v8 )
  {
    v10 = *(u32 *)(a1 + 172);
    v9 = 0;
    if ( v10 != 0 )
      *a4 = v10 * (a3 - a2 + 1);
    else
      return mp4_codec_fn_87(*(u32 *)(a1 + 180), a2 - v5, a3 - v5, 0, a4);
  }
  return v9;
}


int mp4_codec_fn_17(int *a1, unsigned int a2, int a3, int a4, int a5)
{
  u32 *v9;
  int v11;
  int v12;

  v9 = (u32 *)mp4_codec_fn_142(a1);
  if ( mp4_box_fn_21(v9) )
    return mp4_api_fn_43(a1[71], a2, a3, a4, a5);
  v11 = a1[70];
  v12 = 7;
  if ( v11 != 0 )
  {
    v12 = 8201;
    if ( a2 >= *(u32 *)(v11 + 56) )
    {
      v12 = mp4_codec_fn_15(a1, a2, a5);
      if ( v12 == 0 )
        return mp4_codec_fn_18(a1, a2, a3, a4);
    }
  }
  return v12;
}


int mp4_codec_fn_18(u32 *a1, unsigned int a2, u32 *a3, int *a4)
{
  unsigned int *v6;
  int v7;
  unsigned int v8;
  u32 *v9;
  int v10;
  unsigned int v12;
  unsigned int v13;
  unsigned int v14;
  unsigned int v15;
  int v16;
  unsigned int v17;
  unsigned int v18;
  unsigned int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  unsigned int v26;
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

  v6 = (unsigned int *)a1[70];
  v7 = a1[44];
  v8 = *v6;
  if ( a2 < *v6 + v7 )
  {
    v9 = a1 + 13;
    if ( a2 >= v8 )
    {
      v12 = a1[19];
      if ( v12 == 0 )
      {
        a1[20] = v8;
        a1[21] = v6[2];
      }
      v13 = a1[20];
      if ( a2 >= v13 )
      {
        v14 = a1[17];
        if ( v13 < a2 )
        {
          v31 = v14 - 1;
          if ( v12 != v14 - 1 )
          {
            v16 = a2 < v13;
            if ( *v6 + v7 - a2 >= a2 - v13 )
            {
LABEL_12:
              v17 = v12 - 1;
              if ( v16 )
              {
                v18 = v13;
                v10 = 8195;
                if ( v17 >= v14 )
                  return v10;
                v25 = 0;
                v26 = 0;
                v27 = v12 - 1;
                while ( 1 )
                {
                  v29 = mp4_codec_fn_5((int)v9, v27, 0);
                  v28 = mp4_codec_fn_5((int)v9, v27, 1);
                  v18 -= v29;
                  ++v26;
                  v25 += v28 * v29;
                  if ( a2 >= v18 )
                    break;
                  v27 = v17 - v26;
                  if ( v17 < v26 )
                    return 8195;
                }
                *a3 = v9[8] - v25 + (a2 - v18) * v28;
                *a4 = v28;
                v30 = v9[8];
                v9[6] = v27;
                v24 = v30 - v25;
              }
              else
              {
                v18 = v13;
                v10 = 8195;
                if ( v12 >= v14 )
                  return v10;
                v19 = v12;
                v20 = 0;
                while ( 1 )
                {
                  v21 = mp4_codec_fn_5((int)v9, v19, 0);
                  v22 = mp4_codec_fn_5((int)v9, v19, 1);
                  if ( a2 < v18 + v21 )
                    break;
                  ++v19;
                  v18 += v21;
                  v20 += v22 * v21;
                  if ( v19 >= v9[4] )
                    return 8195;
                }
                *a3 = v9[8] + v20 + (a2 - v18) * v22;
                *a4 = v22;
                v23 = v9[8];
                v9[6] = v19;
                v24 = v23 + v20;
              }
              v9[8] = v24;
              v10 = 0;
              v9[7] = v18;
              return v10;
            }
            a1[19] = v31;
            v32 = *v6 + v7;
            a1[20] = v32 - mp4_codec_fn_5((int)(a1 + 13), v31, 0);
            v33 = (int)(a1 + 13);
            v34 = a1[70];
            v35 = a1[17];
            v36 = *(u32 *)(v34 + 12);
            v37 = *(u32 *)(v34 + 8);
            v39 = mp4_codec_fn_5(v33, v35 - 1, 0);
            v38 = mp4_codec_fn_5((int)v9, v9[4] - 1, 1);
            v12 = v9[6];
            v13 = v9[7];
            v14 = v9[4];
            v9[8] = v37 + v36 - v39 * v38;
          }
        }
      }
      else if ( a2 - *v6 < v13 - a2 )
      {
        a1[19] = 0;
        v12 = 0;
        v15 = *v6;
        v14 = a1[17];
        a1[20] = *v6;
        v13 = v15;
        a1[21] = v6[2];
      }
      else
      {
        v14 = a1[17];
      }
      v16 = a2 < v13;
      goto LABEL_12;
    }
  }
  return 8201;
}


int mp4_codec_fn_19(int a1, unsigned int a2, unsigned int *a3, u32 *a4, int a5, int a6)
{
  u32 *v11;
  int v13;
  int v14;
  int v16;

  v11 = (u32 *)mp4_codec_fn_142(a1);
  if ( mp4_box_fn_21(v11) )
    return mp4_io_fn_54(*(u32 *)(a1 + 284), a2, a3, a4, a5, a6);
  v13 = *(u32 *)(a1 + 280);
  v14 = 7;
  if ( v13 != 0 )
  {
    v14 = 8197;
    if ( a2 >= *(u32 *)(v13 + 60) )
    {
      v14 = mp4_codec_fn_39(a1, a2, a6);
      if ( v14 == 0 )
      {
        v14 = mp4_codec_fn_40(a1, a2, a3, a5);
        if ( v14 == 0 )
        {
          v16 = mp4_codec_fn_22(a1, *a3, a4);
          v14 = 8195;
          if ( v16 != 0x100D )
            return v16;
        }
      }
    }
  }
  return v14;
}


int mp4_codec_fn_20(int *a1, unsigned int a2, int a3, int *a4, int a5)
{
  u32 *v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  unsigned int v21;
  int v22;
  int v23;
  unsigned int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  unsigned int v31;
  int v32;
  unsigned int v33;
  int v34;
  int v35;
  u32 v36[3];

  v10 = (u32 *)mp4_codec_fn_142(a1);
  if ( mp4_box_fn_21(v10) )
  {
    v11 = a1[71];
    return mp4_io_fn_50(v11, a2, a3, a4, a5);
  }
  v12 = mp4_codec_fn_142(a1);
  if ( mp4_box_fn_19(v12) )
  {
    v11 = a1[71];
    return mp4_io_fn_50(v11, a2, a3, a4, a5);
  }
  v13 = a1[70];
  v14 = 7;
  if ( v13 != 0 )
  {
    v14 = 8201;
    if ( a2 >= *(u32 *)(v13 + 56) )
    {
      v14 = mp4_codec_fn_15(a1, a2, a5);
      if ( v14 == 0 )
      {
        v16 = mp4_codec_fn_41(a1, a2, a4 + 2, &v31);
        v14 = 8195;
        if ( v16 == 0 )
        {
          v17 = -1;
          if ( a1 != (int *)-160 )
          {
            v17 = a1[43];
            if ( v17 == 0 )
            {
              v30 = mp4_codec_fn_91(a1[45], a2 - *(u32 *)a1[70], 0, &v32);
              v17 = -1;
              if ( v30 == 0 )
                v17 = v32;
            }
          }
          *a4 = v17;
          v18 = mp4_codec_fn_18(a1, a2, &v33, a4 + 1);
          v14 = 8195;
          if ( v18 == 0 )
          {
            if ( a1[22] != 0 )
            {
              v29 = mp4_codec_fn_22(a1, a2, a4 + 4);
              v14 = 8195;
              if ( v29 != 0 )
                return v14;
            }
            else
            {
              a4[4] = 1;
            }
            if ( a1[58] != 0 )
            {
              v28 = mp4_codec_fn_30(a1, a2, a4 + 5);
              v14 = 8195;
              if ( v28 != 0 )
                return v14;
            }
            else
            {
              a4[5] = 0;
            }
            v19 = mp4_codec_fn_1(a1, v31, &v34, &v35, a4 + 9);
            v14 = 8195;
            if ( v19 != 0 )
              return v14;
            v20 = v35;
            if ( a3 != 1 )
            {
LABEL_20:
              v14 = 0;
              *(u32 *)(a1[70] + 20) = a2 + 1;
              return v14;
            }
            if ( v35 == 0 )
              return v14;
            a4[6] = 0;
            v21 = v34 + v20 - 1;
            v22 = mp4_codec_fn_16((int)a1, a2, v21, a4 + 6);
            v14 = 8195;
            if ( v22 != 0 )
              return v14;
            a4[7] = 0;
            v23 = a2;
            v33 = a2;
            if ( v21 < a2 )
            {
              v24 = v21 - a2;
LABEL_25:
              a4[8] = v24 + 1;
              goto LABEL_20;
            }
            while ( 1 )
            {
              v25 = mp4_codec_fn_14((int)a1, v23, (int)v36);
              v14 = 8195;
              if ( v25 != 0 )
                break;
              v26 = v33 + 1;
              v27 = v21 < v33 + 1;
              a4[7] += v36[0];
              v23 = v26;
              v33 = v26;
              if ( v27 )
              {
                v24 = v21 - a2;
                goto LABEL_25;
              }
            }
          }
        }
      }
    }
  }
  return v14;
}


int mp4_codec_fn_21(int *a1, unsigned int a2, int a3, int a4)
{
  int v4;
  int v7;

  v4 = 7;
  v7 = a1[70];
  if ( v7 != 0 )
  {
    v4 = 8201;
    if ( a2 >= *(u32 *)(v7 + 56) )
    {
      v4 = mp4_codec_fn_15(a1, a2, a4);
      if ( v4 == 0 )
        return mp4_codec_fn_22(a1, a2, a3);
    }
  }
  return v4;
}


int mp4_codec_fn_22(u32 *a1, unsigned int a2, u32 *a3)
{
  unsigned int *v5;
  int v7;
  u32 *v9;
  unsigned int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  unsigned int v17;
  int v18;
  unsigned int v19;
  unsigned int v20;
  unsigned int v21;
  int v22;
  int v23;
  int v24;
  unsigned int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  unsigned int v30;
  int v31;
  int v32;
  int v33;
  int v34;
  int v35;
  int v36;
  int v37;
  int v38[4];

  v5 = (unsigned int *)a1[70];
  if ( a2 >= *v5 + a1[44] || a2 < *v5 )
    return 8201;
  v9 = a1 + 22;
  if ( a1[22] == 0 )
    goto LABEL_45;
  v7 = 0;
  if ( a1[26] == 0 )
    goto LABEL_31;
  if ( a1[28] != 0 )
  {
    v10 = a1[29];
  }
  else
  {
    v11 = -1;
    if ( a1 != (u32 *)-88 )
    {
      v12 = mp4_codec_fn_91(a1[27], 0, 0, &v34);
      v11 = -1;
      if ( v12 == 0 )
        v11 = v34;
    }
    a1[29] = v11 - 1;
    v10 = a1[29];
  }
  if ( a2 >= v10 )
  {
    v13 = v10 < a2;
  }
  else
  {
    v13 = v10 < a2;
    if ( a1[28] != 0 )
    {
      v14 = a2 < v10;
      if ( a2 - *(u32 *)a1[70] >= v10 - a2 )
        goto LABEL_21;
      v9[6] = 0;
      v15 = -1;
      if ( v9 != NULL )
      {
        v16 = mp4_codec_fn_91(v9[5], 0, 0, &v35);
        v15 = -1;
        if ( v16 == 0 )
          v15 = v35;
      }
      v10 = v15 - 1;
      v9[7] = v15 - 1;
      goto LABEL_20;
    }
  }
  v14 = a2 < v10;
  if ( v13 )
  {
    v31 = a1[26] - 1;
    if ( a1[28] != v31 && *(u32 *)a1[70] + a1[44] - a2 < a2 - v10 )
    {
      a1[28] = v31;
      v32 = -1;
      if ( a1 != (u32 *)-88 )
      {
        v33 = mp4_codec_fn_91(a1[27], v31, 0, &v36);
        v32 = -1;
        if ( v33 == 0 )
          v32 = v36;
      }
      a1[29] = v32 - 1;
      v10 = v32 - 1;
LABEL_20:
      v14 = a2 < v10;
    }
  }
LABEL_21:
  v17 = 0;
  if ( v14 )
  {
    v18 = v9[6];
    v19 = v18 - 1;
    if ( v18 != 0 )
    {
      while ( 1 )
      {
        v20 = v19 - v17;
        v21 = v19 - v17++;
        v22 = -1;
        if ( v9 != NULL )
        {
          v23 = mp4_codec_fn_91(v9[5], v21, 0, &v37);
          v22 = -1;
          if ( v23 == 0 )
            v22 = v37;
        }
        v24 = v22 - 1;
        if ( v22 - 1 == a2 )
          break;
        if ( v22 - 1 < a2 )
        {
          v7 = 0;
          goto LABEL_31;
        }
        v9[6] = v20;
        v9[7] = v24;
        if ( v19 < v17 )
          goto LABEL_30;
      }
      v7 = 0;
      *a3 = 1;
      v9[7] = v24;
      v9[6] = v20;
      return v7;
    }
LABEL_30:
    v7 = 0;
LABEL_31:
    *a3 = 0;
    return v7;
  }
  if ( v10 == a2 )
  {
LABEL_45:
    v7 = 0;
    *a3 = 1;
    return v7;
  }
  v25 = v9[6] + 1;
  if ( v25 < v9[4] )
  {
    while ( 1 )
    {
      v26 = -1;
      if ( v9 != NULL )
      {
        v27 = mp4_codec_fn_91(v9[5], v25, 0, v38);
        v26 = -1;
        if ( v27 == 0 )
          v26 = v38[0];
      }
      v28 = v26 - 1;
      v29 = a2 < v26 - 1;
      if ( v26 - 1 == a2 )
        break;
      v7 = 0;
      if ( v29 )
        goto LABEL_31;
      v30 = v9[4];
      v9[6] = v25++;
      v9[7] = v28;
      if ( v25 >= v30 )
      {
        *a3 = 0;
        return 0;
      }
    }
    v7 = 0;
    *a3 = 1;
    v9[7] = v28;
    v9[6] = v25;
  }
  else
  {
    *a3 = 0;
    return 0;
  }
  return v7;
}


int mp4_codec_fn_23(int a1, unsigned int a2, int a3, unsigned int *a4, u32 *a5, int a6)
{
  u32 *v12;
  u32 *v13;
  unsigned int v14;
  unsigned int v15;
  unsigned int i;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;

  v12 = (u32 *)mp4_codec_fn_142(a1);
  if ( mp4_box_fn_21(v12) )
    return mp4_api_fn_42(*(u32 **)(a1 + 284), a2, a3, a4, a5, a6);
  v13 = *(u32 **)(a1 + 280);
  v23 = 7;
  if ( v13 != NULL )
  {
    v23 = 8201;
    if ( a2 >= v13[14] )
    {
      v23 = 8200;
      if ( a2 != 0 || a3 != 0 )
      {
        if ( v13[10] != 1 || (v23 = 8201, v13[11] >= a2 + 1) )
        {
          v23 = mp4_codec_fn_17((int *)a1, a2, (int)&v24, (int)&v25, 1);
          if ( v23 == 0 )
          {
            v14 = a2 + 1;
            if ( a3 != 0 )
            {
              while ( 1 )
              {
                v21 = mp4_codec_fn_21((int *)a1, v14, (int)&v26, a6);
                if ( v21 != 0 )
                {
                  v23 = v21;
                  if ( v21 == 8201 )
                    return 8200;
                  return v23;
                }
                if ( v26 == 1 )
                  break;
                ++v14;
              }
              *a4 = v14;
              v22 = mp4_codec_fn_18((u32 *)a1, v14, &v27, &v25);
              v23 = v22;
              if ( v22 == 0 )
              {
                v19 = v27;
                v20 = v24;
LABEL_19:
                v23 = 0;
                *a5 = v19 - v20;
              }
            }
            else
            {
              v15 = 0;
              if ( a2 != 0 )
              {
                for ( i = a2 - 1; ; --i )
                {
                  ++v15;
                  v23 = mp4_codec_fn_21((int *)a1, i, (int)&v26, a6);
                  if ( v23 != 0 )
                    break;
                  if ( v26 == 1 )
                  {
                    *a4 = i;
                    v18 = mp4_codec_fn_18((u32 *)a1, i, &v27, &v25);
                    v23 = v18;
                    if ( v18 == 0 )
                    {
                      v19 = v24;
                      v20 = v27;
                      goto LABEL_19;
                    }
                    return v23;
                  }
                  if ( v15 >= a2 )
                    return 8200;
                }
              }
              else
              {
                return 8200;
              }
            }
          }
        }
      }
    }
  }
  return v23;
}


int mp4_codec_fn_24(u32 *a1, u32 *a2, int a3)
{
  u32 *v5;
  int v7;
  int v8;
  int v9;
  int v11;

  v5 = (u32 *)mp4_codec_fn_142(a1);
  if ( mp4_box_fn_21(v5) )
    return mp4_api_fn_7(a1[71], a2);
  v7 = a1[70];
  v11 = 7;
  if ( v7 != 0 )
  {
    v11 = 8195;
    if ( a1[40] != 0 )
    {
      if ( *(u32 *)(v7 + 40) == 1 )
      {
        v8 = *(u32 *)(v7 + 44);
        v11 = 0;
      }
      else
      {
        v11 = 2;
        if ( a3 == 0 )
          return v11;
        while ( 1 )
        {
          v9 = mp4_api_fn_73((int)a1);
          v11 = v9;
          if ( v9 == 20496 )
            break;
          if ( v9 != 0 )
            return v11;
        }
        v11 = 0;
        v8 = *(u32 *)(a1[70] + 44);
      }
      *a2 = v8;
    }
  }
  return v11;
}


int mp4_codec_fn_25(int a1, u32 *a2, int a3)
{
  u32 *v5;
  int v7;
  int v8;
  int v9;
  int v11;

  v5 = (u32 *)mp4_codec_fn_142(a1);
  if ( mp4_box_fn_21(v5) )
    return mp4_api_fn_61(*(u32 *)(a1 + 284), a2);
  v7 = *(u32 *)(a1 + 280);
  v11 = 7;
  if ( v7 != 0 )
  {
    if ( *(u32 *)(v7 + 40) == 1 )
    {
      v8 = *(u32 *)(v7 + 48);
      v11 = 0;
    }
    else
    {
      v11 = 2;
      if ( a3 == 0 )
        return v11;
      while ( 1 )
      {
        v9 = mp4_api_fn_73(a1);
        v11 = v9;
        if ( v9 == 20496 )
          break;
        if ( v9 != 0 )
          return v11;
      }
      v11 = 0;
      v8 = *(u32 *)(*(u32 *)(a1 + 280) + 48);
    }
    *a2 = v8;
  }
  return v11;
}


int mp4_codec_fn_26(u32 *a1, int *a2)
{
  u32 *v3;
  int v5;

  v3 = (u32 *)mp4_codec_fn_142(a1);
  if ( mp4_box_fn_21(v3) )
    return mp4_api_fn_59(a1[71], a2);
  v5 = 0;
  *a2 = *(u32 *)(a1[70] + 4) + a1[53];
  return v5;
}


int mp4_codec_fn_27(int a1, unsigned int a2, int *a3)
{
  unsigned int *v4;
  int v5;
  unsigned int v6;
  unsigned int v7;
  int v8;
  int v9;
  int v10;
  int v12;
  int v13[4];

  v4 = *(unsigned int **)(a1 + 280);
  v5 = 7;
  if ( v4 != NULL )
  {
    v6 = *v4;
    v5 = 8194;
    if ( a2 >= *v4 )
    {
      v7 = a2 - v6;
      v8 = a2 < v6 + *(u32 *)(a1 + 176);
      v9 = a1 + 160;
      if ( v8 )
      {
        v10 = -1;
        if ( a1 != -160 )
        {
          v10 = *(u32 *)(v9 + 12);
          if ( v10 == 0 )
          {
            v12 = mp4_codec_fn_91(*(u32 *)(v9 + 20), v7, 0, v13);
            v10 = -1;
            if ( v12 == 0 )
              v10 = v13[0];
          }
        }
        *a3 = v10;
        return 0;
      }
    }
  }
  return v5;
}


int mp4_codec_fn_28(int a1, int a2)
{
  int v4;
  int v5;
  int v6;
  u32 *v8;
  unsigned int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  u32 *v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  u32 v21[4];

  v4 = mp4_box_fn_21(*(u32 **)(a2 + 84));
  v5 = 0;
  if ( !v4 )
  {
    if ( mp4_box_fn_19(*(u32 *)(a2 + 84)) )
      return 0;
    v5 = 20495;
    if ( a1 != 0 )
    {
      v5 = 8195;
      if ( *(u32 *)(a1 + 16) != 0 && *(u32 *)(a1 + 124) != 0 && *(u32 *)(a1 + 52) != 0 )
      {
        v5 = 8195;
        if ( *(u32 *)(a1 + 160) != 0 )
        {
          v6 = *(u32 *)(a1 + 196);
          if ( v6 == 0 )
            return 8195;
          if ( *(u32 *)(a1 + 280) != 0 )
            mp4_api_fn_65(a1, 8195, v6);
          v8 = (u32 *)mp4_sample_fn_5(72);
          *(u32 *)(a1 + 280) = v8;
          v5 = 1;
          if ( v8 != NULL )
          {
            *(u32 *)(a2 + 12) = 2;
            v9 = 0;
            v8[3] = 0;
            v8[4] = 0;
            v8[10] = 0;
            v10 = *(u32 *)(a1 + 68);
            v8[13] = a2;
            if ( v10 != 0 )
            {
              v11 = 0;
              do
              {
                v13 = mp4_codec_fn_5(a1 + 52, v11, 0);
                v12 = mp4_codec_fn_5(a1 + 52, v9, 1);
                v14 = *(u32 **)(a1 + 280);
                ++v9;
                v14[3] += v13 * v12;
                v11 = v9;
              }
              while ( v9 < *(u32 *)(a1 + 68) );
            }
            else
            {
              v14 = *(u32 **)(a1 + 280);
            }
            v14[4] = 1;
            v15 = *(u32 *)(a1 + 276);
            *(u8 *)(a1 + 268) = 1;
            v16 = *(u32 *)(v15 + 700);
            v17 = *(u32 *)(a1 + 212);
            v18 = *(u32 *)(v16 + 148);
            v14[6] = *(u32 *)(v16 + 144);
            v14[7] = v18;
            if ( v17 == 0 )
              goto LABEL_20;
            v14[9] = 1;
            v21[0] = 1;
            v21[1] = 0;
            v21[2] = *(u32 *)(a1 + 212) + v14[1] - 1;
            v20 = mp4_codec_fn_89(v14 + 8, 3, v21, 2);
            v5 = v20;
            if ( v20 == 0 )
            {
              v14 = *(u32 **)(a1 + 280);
LABEL_20:
              v19 = v14[3];
              v14[10] = 1;
              v5 = 0;
              v14[12] = v19;
              v14[11] = *(u32 *)(a1 + 176);
            }
          }
        }
      }
    }
  }
  return v5;
}


int mp4_codec_fn_29(u32 *a1)
{
  u32 *v1;
  int *v3;
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
  int v15;
  int v16[4];

  v1 = (u32 *)mp4_codec_fn_142(a1);
  if ( mp4_box_fn_21(v1) )
  {
    v3 = (int *)a1[71];
  }
  else
  {
    v4 = mp4_codec_fn_142(a1);
    if ( !mp4_box_fn_19(v4) )
    {
      v5 = a1[58];
      goto LABEL_6;
    }
    v3 = (int *)a1[71];
  }
  if ( v3 == NULL )
  {
    v5 = a1[58];
LABEL_6:
    a1[4] = 8;
    if ( v5 != 0 )
    {
      a1[58] = 8 * a1[62] + 16;
      if ( mp4_codec_fn_32(a1 + 58) == 0 )
      {
        v6 = a1[13];
        goto LABEL_8;
      }
      a1[4] += a1[58];
    }
    v6 = a1[13];
LABEL_8:
    if ( v6 != 0 )
    {
      v7 = 8 * a1[17] + 16;
      a1[4] += v7;
      a1[13] = v7;
    }
    if ( a1[31] != 0 )
    {
      v8 = 12 * a1[35] + 16;
      a1[4] += v8;
      a1[31] = v8;
    }
    if ( a1[22] != 0 )
    {
      v9 = 4 * a1[26] + 16;
      a1[4] += v9;
      a1[22] = v9;
    }
    if ( a1[40] != 0 )
    {
      if ( a1[43] != 0 )
      {
        v11 = a1[4];
        a1[40] = 20;
        a1[4] = v11 + 20;
      }
      else
      {
        v15 = 4 * a1[44] + 20;
        a1[4] += v15;
        a1[40] = v15;
      }
      v10 = a1[49];
    }
    else
    {
      v10 = a1[49];
    }
    v12 = 0;
    if ( v10 != 0 )
    {
      v13 = 4 * a1[53] + 16;
      a1[4] += v13;
      a1[49] = v13;
    }
    return v12;
  }
  v12 = mp4_api_fn_32(v3, v16);
  if ( v12 == 0 )
  {
    v12 = 0;
    a1[4] = v16[0];
  }
  return v12;
}


int mp4_codec_fn_30(u32 *a1, unsigned int a2, int *a3)
{
  unsigned int *v5;
  int v6; // dc
  int v7;
  int v8;
  u32 *v10;
  unsigned int v11;
  unsigned int v12;
  unsigned int v13;
  int v14;
  int v15;
  int v16;
  unsigned int v17;
  unsigned int v18;
  int v19;

  v5 = (unsigned int *)a1[70];
  v6 = a2 >= *v5 + a1[44];
  v7 = a2 < *v5;
  if ( v6 || v7 )
    return 8201;
  v10 = a1 + 58;
  if ( a1[58] != 0 )
  {
    v11 = a1[64];
    if ( v11 == 0 )
      v10[7] = *v5;
    if ( a2 < v10[7] )
    {
      v17 = v11 - 1;
      v12 = v10[7];
      v8 = 8195;
      if ( v11 - 1 >= v10[4] )
        return v8;
      v18 = 0;
      v19 = v11 - 1;
      while ( 1 )
      {
        v12 -= mp4_codec_fn_6((int)v10, v19, 0);
        ++v18;
        if ( a2 >= v12 )
          break;
        v19 = v17 - v18;
        if ( v17 < v18 )
          return 8195;
      }
      v16 = v19;
    }
    else
    {
      v12 = v10[7];
      v8 = 8195;
      if ( v11 >= v10[4] )
        return v8;
      v13 = v11;
      v14 = v11;
      while ( 1 )
      {
        v15 = mp4_codec_fn_6((int)v10, v14, 0);
        if ( a2 < v12 + v15 )
          break;
        ++v13;
        v12 += v15;
        v14 = v13;
        if ( v13 >= v10[4] )
          return 8195;
      }
      v16 = v13;
    }
    *a3 = mp4_codec_fn_6((int)v10, v16, 1);
    v8 = 0;
    v10[7] = v12;
    v10[6] = v16;
    return v8;
  }
  v8 = 0;
  *a3 = 0;
  return v8;
}


int mp4_codec_fn_31(int a1)
{
  u32 *v1;
  int v2;
  int v3;
  int v4;

  v1 = *(u32 **)(a1 + 280);
  v2 = 7;
  if ( v1 != NULL )
  {
    v3 = v1[2];
    v4 = v1[1];
    v1[14] = *v1;
    v2 = 0;
    v1[16] = v4;
    v1[15] = v3;
  }
  return v2;
}


int mp4_codec_fn_32(int a1)
{
  int v1;
  int result;
  unsigned int v4;
  int v5;
  int v6;
  int v7;
  unsigned int v8;

  v1 = 0;
  if ( a1 == 0 )
    return v1;
  result = 0;
  if ( *(u32 *)(a1 + 20) != 0 && *(u32 *)(a1 + 16) != 0 )
  {
    v4 = 0;
    v6 = 0;
    do
    {
      v7 = mp4_codec_fn_6(a1, v6, 1);
      v8 = *(u32 *)(a1 + 16);
      if ( v7 != 0 )
      {
        v5 = v4 < v8;
        goto LABEL_6;
      }
      v6 = ++v4;
    }
    while ( v4 < v8 );
    v5 = false;
LABEL_6:
    if ( v5 )
      return 1;
    return v1;
  }
  return result;
}


int mp4_codec_fn_33(u32 *a1, u32 *a2)
{
  *a2 = *a1 == 0;
  return 0;
}


int mp4_codec_fn_34(int a1, u32 *a2)
{
  *a2 = *(u32 *)(a1 + 4);
  return 0;
}


int mp4_codec_fn_35(int a1, u32 *a2)
{
  int v2;

  v2 = *(u32 *)(a1 + 12);
  *a2 = *(u32 *)(a1 + 8);
  a2[1] = v2;
  return 0;
}


int mp4_codec_fn_36(int a1)
{
  return mp4_api_fn_73(a1);
}


int mp4_codec_fn_37(u32 *a1, u32 *a2, u32 *a3)
{
  u32 *v5;
  u32 *v7;
  int v8;

  v5 = (u32 *)mp4_codec_fn_142(a1);
  if ( mp4_box_fn_21(v5) )
    return mp4_api_fn_62(a1[71], a2, a3);
  v7 = (u32 *)a1[70];
  v8 = 7;
  if ( v7 != NULL )
  {
    v8 = 0;
    *a2 = *v7;
    *a3 = a1[44];
  }
  return v8;
}


int mp4_codec_fn_38(int a1, u32 *a2, u32 *a3)
{
  if ( *(u32 *)(a1 + 88) != 0 )
  {
    *a2 = 1;
    *a3 = *(u32 *)(a1 + 104);
  }
  else
  {
    *a2 = 0;
    *a3 = 0;
  }
  return 0;
}


int mp4_codec_fn_39(int *a1, unsigned int a2, int a3)
{
  int v4;
  u32 *v6;
  unsigned int v7;
  int v8;
  int v10;

  v4 = 7;
  v6 = (u32 *)a1[70];
  if ( v6 != NULL )
  {
    if ( v6[10] != 1 || (v4 = 8197, a2 < v6[12]) )
    {
      v7 = v6[2];
      v4 = 0;
      if ( a2 < v7 || a2 >= v7 + v6[3] )
      {
        v4 = 2;
        if ( a3 != 0 )
        {
          if ( a2 < v7 )
          {
            v10 = mp4_api_fn_66(a1);
            v4 = 10;
            if ( v10 == 0 )
              goto LABEL_8;
          }
          else
          {
            while ( 1 )
            {
              v4 = 0;
              if ( a2 >= v6[2] && a2 < v6[2] + v6[3] )
                break;
              v8 = mp4_api_fn_73((int)a1);
              if ( v8 != 0 )
              {
                v4 = 8197;
                if ( v8 != 0x5010 )
                  return v8;
                return v4;
              }
LABEL_8:
              v6 = (u32 *)a1[70];
            }
          }
        }
      }
    }
  }
  return v4;
}


int mp4_codec_fn_40(u32 *a1, unsigned int a2, u32 *a3, u32 *a4)
{
  int v4;
  u32 *v7;
  unsigned int v8;
  u32 *v9;
  unsigned int v11;
  unsigned int v12;
  unsigned int v13;
  int v14;
  unsigned int v15;
  unsigned int v16;
  unsigned int v17;
  int v18;
  unsigned int v19;
  int v20;
  int v21;
  int v22;
  unsigned int v23;
  unsigned int v24;
  int v25;
  unsigned int v26;
  int v27;
  int v28;
  unsigned int v29;
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
  int v40;

  v4 = 8195;
  if ( a1[13] != 0 )
  {
    v7 = (u32 *)a1[70];
    v8 = v7[2];
    if ( v8 + v7[3] < a2 )
      return 8197;
    v9 = a1 + 13;
    if ( a2 < v8 )
      return 8197;
    v11 = a1[19];
    if ( v11 == 0 )
    {
      a1[20] = *v7;
      a1[21] = v7[2];
    }
    v12 = a1[21];
    if ( a2 >= v12 )
    {
      v13 = a1[17];
      if ( v12 < a2 )
      {
        v31 = v13 - 1;
        if ( v11 != v13 - 1 )
        {
          v14 = a2 < v12;
          if ( v7[2] + v7[3] - a2 >= a2 - v12 )
          {
LABEL_13:
            v15 = v11 - 1;
            if ( v14 )
            {
              v24 = v12;
              v4 = 8195;
              if ( v15 >= v13 )
                return v4;
              v25 = 0;
              v26 = 0;
              v27 = v11 - 1;
              while ( 1 )
              {
                v28 = mp4_codec_fn_5((int)v9, v27, 0);
                ++v26;
                v29 = mp4_codec_fn_5((int)v9, v27, 1);
                v24 -= v28 * v29;
                v25 += v28;
                if ( a2 >= v24 )
                  break;
                v27 = v15 - v26;
                if ( v15 < v26 )
                  return 8195;
              }
              v23 = v27;
              if ( v29 == 0 )
                _break(0, 7u);
              *a3 = v9[7] - v25 + (a2 - v24) / v29;
              *a4 = a2 - (v24 + (a2 - v24) / v29 * v29);
              v30 = v9[7];
              v9[8] = v24;
              v22 = v30 - v25;
            }
            else
            {
              v16 = v12;
              v4 = 8195;
              if ( v11 >= v13 )
                return v4;
              v17 = v11;
              v18 = 0;
              while ( 1 )
              {
                v20 = mp4_codec_fn_5((int)v9, v17, 0);
                v19 = mp4_codec_fn_5((int)v9, v17, 1);
                if ( a2 < v16 + v20 * v19 )
                  break;
                ++v17;
                v16 += v20 * v19;
                v18 += v20;
                if ( v17 >= v9[4] )
                  return 8195;
              }
              v23 = v17;
              if ( v19 == 0 )
                _break(0, 7u);
              *a3 = v9[7] + v18 + (a2 - v16) / v19;
              *a4 = a2 - (v16 + (a2 - v16) / v19 * v19);
              v21 = v9[7];
              v9[8] = v16;
              v22 = v21 + v18;
            }
            v9[6] = v23;
            v4 = 0;
            v9[7] = v22;
            return v4;
          }
          a1[19] = v31;
          v33 = *v7 + a1[44];
          v32 = mp4_codec_fn_5((int)(a1 + 13), v31, 0);
          v34 = a1[17];
          a1[20] = v33 - v32;
          v35 = (int)(a1 + 13);
          v36 = a1[70];
          v37 = *(u32 *)(v36 + 12);
          v38 = *(u32 *)(v36 + 8);
          v40 = mp4_codec_fn_5(v35, v34 - 1, 0);
          v39 = mp4_codec_fn_5((int)v9, v9[4] - 1, 1);
          v11 = v9[6];
          v13 = v9[4];
          v12 = v38 + v37 - v40 * v39;
          v9[8] = v12;
        }
      }
    }
    else if ( a2 - v7[2] < v12 - a2 )
    {
      a1[19] = 0;
      v11 = 0;
      v13 = a1[17];
      a1[20] = *v7;
      v12 = v7[2];
      a1[21] = v12;
    }
    else
    {
      v13 = a1[17];
    }
    v14 = a2 < v12;
    goto LABEL_13;
  }
  return v4;
}


int mp4_codec_fn_41(int a1, unsigned int a2, int a3, u32 *a4)
{
  unsigned int v7;
  int v9;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  s64 v16;
  int v17;
  unsigned int v18;
  unsigned int v19;
  unsigned int v20;
  s64 v21;
  u32 v22[4];

  v7 = **(u32 **)(a1 + 280);
  if ( a2 >= v7 + *(u32 *)(a1 + 176) || a2 < v7 )
    return 8201;
  v11 = mp4_codec_fn_2((u32 *)a1, a2, a4, &v19);
  v9 = 8195;
  if ( v11 != 0 )
    return v9;
  if ( v19 < a2 )
  {
    v15 = mp4_codec_fn_16(a1, v19, a2 - 1, &v20);
    v9 = 8195;
    if ( v15 != 0 )
      return v9;
    v16 = -1;
    if ( a1 != -196 )
    {
      v17 = mp4_codec_fn_96(*(u32 *)(a1 + 216), *a4 - *(u32 *)(*(u32 *)(a1 + 280) + 4), 0, &v21);
      v16 = -1;
      v18 = v20;
      if ( v17 != 0 )
      {
LABEL_16:
        *(u64 *)a3 = v16 + v18;
        return 0;
      }
      v16 = v21;
    }
    v18 = v20;
    goto LABEL_16;
  }
  v12 = -1;
  v13 = -1;
  if ( a1 != -196 )
  {
    v14 = mp4_codec_fn_96(*(u32 *)(a1 + 216), *a4 - *(u32 *)(*(u32 *)(a1 + 280) + 4), 0, v22);
    v12 = -1;
    v13 = -1;
    if ( v14 == 0 )
    {
      v13 = v22[0];
      v12 = v22[1];
    }
  }
  *(u32 *)a3 = v13;
  *(u32 *)(a3 + 4) = v12;
  return 0;
}


int mp4msv_32AB9DCB(int *a1, u32 *a2)
{
  int v2;
  int v3;
  int v4;

  v2 = 4;
  if ( a1 != NULL && a1[1] != 0 )
  {
    v3 = *a1;
    v2 = 7;
    if ( v3 != 0 )
    {
      v4 = *(u32 *)(v3 + 136);
      v2 = 8194;
      if ( v4 != 0 )
      {
        v2 = 0;
        *a2 = *(u32 *)(v4 + 20);
      }
    }
  }
  return v2;
}


int mp4_codec_fn_42(int *a1)
{
  int v1;
  unsigned int v2;
  unsigned int v4;
  int v5;

  v1 = *a1;
  v2 = 0;
  if ( *(u32 *)(*a1 + 8) != 0 )
  {
    v4 = 0;
    do
    {
      ++v2;
      v5 = *(u32 *)(mp4_track_fn_69(v1, v4) + 720);
      if ( v5 != 0 )
        mp4_codec_fn_65(v5);
      v4 = v2;
    }
    while ( v2 < *(u32 *)(v1 + 8) );
  }
  return 0;
}


int mp4_codec_fn_43(u32 *a1, int a2)
{
  unsigned int v3;
  int v5;
  unsigned int v7;
  int v8;
  unsigned int v9;
  int v10;

  v3 = a1[1];
  if ( v3 == a1[2] )
  {
    v5 = mp4_codec_fn_47();
    if ( v5 != 0 )
      return v5;
    v3 = a1[1];
  }
  a1[1] = v3 + 1;
  if ( v3 >= v3 + 1 )
    return 12;
  v7 = a1[3];
  if ( v7 == 0 )
    _break(0, 7u);
  v8 = mp4_codec_fn_46(a1, v3 / v7);
  v9 = a1[3];
  if ( v9 == 0 )
    _break(0, 7u);
  v10 = mp4_codec_fn_56(v8, v3 % v9 * *a1, *a1, a2);
  v5 = 0;
  if ( v10 != 0 )
    return v10;
  return v5;
}


int mp4_codec_fn_44(u32 *a1, unsigned int a2, int a3)
{
  int v4;
  unsigned int v6;
  unsigned int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  unsigned int v14;
  int v15;
  int v17[4];

  v4 = 12;
  v6 = a1[1];
  if ( v6 >= a2 )
  {
    if ( v6 == a1[2] )
    {
      v4 = mp4_codec_fn_47();
      if ( v4 != 0 )
        return v4;
      v6 = a1[1];
    }
    v8 = a1[3];
    a1[1] = v6 + 1;
    if ( v8 == 0 )
      _break(0, 7u);
    v9 = mp4_codec_fn_46(a1, a2 / v8);
    v10 = a1[6];
    v11 = v9;
    if ( v10 == v9 )
    {
LABEL_9:
      v14 = a1[3];
      if ( v14 == 0 )
        _break(0, 7u);
      v15 = mp4_codec_fn_57(v11, a2 % v14 * *a1, *a1, a3);
      v4 = 0;
      if ( v15 != 0 )
        return v15;
    }
    else
    {
      while ( 1 )
      {
        v12 = mp4_codec_fn_51(v10);
        v4 = mp4_codec_fn_55(v12, (a1[3] - 1) * *a1, v17);
        v13 = v10;
        v10 = v12;
        if ( v4 != 0 )
          break;
        v4 = mp4_codec_fn_57(v13, 0, *a1, v17[0]);
        if ( v4 != 0 )
          break;
        if ( v12 == v11 )
          goto LABEL_9;
      }
    }
  }
  return v4;
}


int mp4_codec_fn_45(u32 *a1, unsigned int a2)
{
  int v2;
  unsigned int v4;
  unsigned int v6;
  int v7;
  unsigned int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v14[4];

  v2 = 12;
  v4 = a1[1];
  if ( a2 < v4 )
  {
    v6 = a1[3];
    a1[1] = v4 - 1;
    if ( v6 == 0 )
      _break(0, 7u);
    v7 = mp4_codec_fn_46(a1, a2 / v6);
    v8 = a1[3];
    v9 = v7;
    if ( v8 == 0 )
      _break(0, 7u);
    v2 = mp4_codec_fn_58(v7, a2 % v8 * *a1, *a1);
    if ( v2 == 0 )
    {
      if ( v9 == a1[6] )
      {
LABEL_12:
        v12 = mp4_codec_fn_48(a1);
        v2 = 0;
        if ( v12 != 0 )
          return v12;
      }
      else
      {
        while ( 1 )
        {
          v10 = mp4_codec_fn_53(v9);
          v2 = mp4_codec_fn_55(v10, 0, v14);
          v11 = v9;
          v9 = v10;
          if ( v2 != 0 )
            break;
          v2 = mp4_codec_fn_56(v11, (a1[3] - 1) * *a1, *a1, v14[0]);
          if ( v2 != 0 )
            break;
          v2 = mp4_codec_fn_58(v10, 0, *a1);
          if ( v2 != 0 )
            break;
          if ( v10 == a1[6] )
            goto LABEL_12;
        }
      }
    }
  }
  return v2;
}


int mp4_codec_fn_46(u32 *a1, unsigned int a2)
{
  int v2;
  int v3;
  int v4;
  unsigned int v5;
  int v7;

  v2 = a1[4];
  v3 = v2 - a2;
  if ( a2 >= (unsigned int)(v2 + 1) >> 1 )
  {
    v7 = v3 - 2;
    v4 = a1[6];
    if ( v3 != 1 )
    {
      do
      {
        --v7;
        v4 = mp4_codec_fn_51(v4);
      }
      while ( v7 != -1 );
    }
  }
  else
  {
    v4 = a1[5];
    v5 = a2 - 1;
    if ( a2 != 0 )
    {
      do
      {
        --v5;
        v4 = mp4_codec_fn_53(v4);
      }
      while ( v5 != -1 );
    }
  }
  return v4;
}


int mp4_codec_fn_47(u32 *a1)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;
  u32 v8[4];

  v2 = mp4_codec_fn_49(*a1 * a1[3], v8);
  if ( v2 == 0 )
  {
    v3 = a1[4];
    v4 = v8[0];
    if ( v3 != 0 )
    {
      mp4_codec_fn_54(a1[6], v8[0]);
      mp4_codec_fn_52(v8[0], a1[6]);
      v3 = a1[4];
      a1[6] = v8[0];
    }
    else
    {
      a1[6] = v8[0];
      a1[5] = v4;
    }
    v5 = a1[2];
    v6 = a1[3];
    a1[4] = v3 + 1;
    v2 = 0;
    a1[2] = v5 + v6;
  }
  return v2;
}


int mp4_codec_fn_48(u32 *a1)
{
  int i;
  int v4;
  unsigned int v5;
  unsigned int v6;
  unsigned int v7;

  if ( a1[4] >= 2u && (unsigned int)(a1[2] - a1[1]) >= a1[3] )
  {
    for ( i = a1[6]; ; i = a1[6] )
    {
      v4 = mp4_codec_fn_51(i);
      a1[6] = v4;
      mp4_codec_fn_54(v4, 0);
      mp4_codec_fn_50(i);
      v5 = a1[3];
      v6 = a1[2] - v5;
      v7 = v6 - a1[1];
      --a1[4];
      a1[2] = v6;
      if ( v7 < v5 )
        break;
    }
  }
  return 0;
}


int mp4_codec_fn_49(int a1, int **a2)
{
  int result;
  int *v5;
  int v6;

  result = 6;
  if ( a1 != 0 )
  {
    v5 = (int *)mp4_box_fn_7(16);
    result = 1;
    if ( v5 != NULL )
    {
      *v5 = a1;
      v6 = mp4_box_fn_7(a1);
      v5[1] = v6;
      if ( v6 != 0 )
      {
        v5[2] = 0;
        result = 0;
        v5[3] = 0;
        *a2 = v5;
      }
      else
      {
        mp4_box_fn_9();
        return 1;
      }
    }
  }
  return result;
}


int mp4_codec_fn_50(int a1)
{
  int result;

  if ( *(u32 *)(a1 + 4) != 0 )
    result = mp4_box_fn_9();
  if ( a1 != 0 )
    return mp4_box_fn_9();
  return result;
}


int mp4_codec_fn_51(int a1)
{
  return *(u32 *)(a1 + 8);
}


void mp4_codec_fn_52(int a1, int a2)
{
  *(u32 *)(a1 + 8) = a2;
}


int mp4_codec_fn_53(int a1)
{
  return *(u32 *)(a1 + 12);
}


void mp4_codec_fn_54(int a1, int a2)
{
  *(u32 *)(a1 + 12) = a2;
}


int mp4_codec_fn_55(unsigned int *a1, unsigned int a2, u32 *a3)
{
  int v3;

  v3 = 12;
  if ( a2 < *a1 )
  {
    v3 = 0;
    *a3 = a1[1] + a2;
  }
  return v3;
}


int mp4_codec_fn_56(u32 *a1, int a2, int a3, char *a4)
{
  int result;
  int v5;
  u8 *i;
  char v7;

  result = 12;
  if ( *a1 >= (unsigned int)(a2 + a3) )
  {
    v5 = a3 - 1;
    for ( i = (u8 *)(a1[1] + a2); v5 != -1; ++i )
    {
      v7 = *a4;
      --v5;
      ++a4;
      *i = v7;
    }
    return 0;
  }
  return result;
}


int mp4_codec_fn_57(int *a1, int a2, int a3, char *a4)
{
  int v4;
  int v5;
  int v6;
  int v7;
  char *v8;
  u8 *v9;
  char v10;

  v4 = *a1;
  v5 = 12;
  if ( *a1 >= (unsigned int)(a2 + a3) )
  {
    v6 = v4 - (a2 + a3) - 1;
    v7 = a1[1] + v4;
    v8 = (char *)(v7 - a3 - 1);
    v9 = (u8 *)(v7 - 1);
    if ( v4 != a2 + a3 )
    {
      do
      {
        v10 = *v8;
        --v6;
        --v8;
        *v9-- = v10;
      }
      while ( v6 != -1 );
    }
    return mp4_codec_fn_56(a1, a2, a3, a4);
  }
  return v5;
}


int mp4_codec_fn_58(u32 *a1, int a2, int a3)
{
  int v3;
  int v4;
  char *v5;
  u8 *v6;
  char v7;

  v3 = 12;
  if ( *a1 >= (unsigned int)(a2 + a3) )
  {
    v4 = *a1 - (a2 + a3) - 1;
    v5 = (char *)(a1[1] + a2 + a3);
    v6 = (u8 *)(a1[1] + a2);
    v3 = 0;
    if ( v4 != -1 )
    {
      do
      {
        v7 = *v5;
        --v4;
        ++v5;
        *v6++ = v7;
      }
      while ( v4 != -1 );
      return 0;
    }
  }
  return v3;
}


int mp4_codec_fn_59(int a1, int a2, u32 *a3)
{
  int v4;
  u32 *v7;

  v4 = 6;
  if ( a1 != 0 && a2 != 0 )
  {
    v7 = (u32 *)mp4_box_fn_7(28);
    v4 = 1;
    if ( v7 != NULL )
    {
      *a3 = v7;
      v4 = 0;
      *v7 = a1;
      v7[3] = a2;
      v7[1] = 0;
      v7[2] = 0;
      v7[4] = 0;
      v7[5] = 0;
      v7[6] = 0;
    }
  }
  return v4;
}


void mp4_codec_fn_60(int a1)
{
  int v1;
  int v3;

  v1 = *(u32 *)(a1 + 20);
  if ( v1 != 0 )
  {
    do
    {
      v3 = mp4_codec_fn_53(v1);
      mp4_codec_fn_50(v1);
      v1 = v3;
    }
    while ( v3 != 0 );
  }
  if ( a1 != 0 )
    mp4_box_fn_9();
}


int mp4_codec_fn_61(int a1, u32 *a2)
{
  *a2 = *(u32 *)(a1 + 4);
  return 0;
}


int mp4_codec_fn_62(int *a1, unsigned int a2, u8 *a3)
{
  int v3;
  unsigned int v7;
  int v8;
  unsigned int v9;
  u32 *v10;
  int v11;
  int v12;
  int v13;
  char *v14;
  u8 *v15;
  char v16;

  v3 = 12;
  if ( a2 < a1[1] )
  {
    v7 = a1[3];
    if ( v7 == 0 )
      _break(0, 7u);
    v8 = mp4_codec_fn_46(a1, a2 / v7);
    v9 = a1[3];
    v10 = (u32 *)v8;
    v3 = 12;
    if ( v9 == 0 )
      _break(0, 7u);
    v11 = *a1;
    v12 = a2 % v9 * *a1;
    if ( *v10 >= (unsigned int)(v12 + *a1) )
    {
      v13 = v11 - 1;
      v14 = (char *)(v10[1] + v12);
      v15 = a3;
      if ( v11 != 0 )
      {
        do
        {
          v16 = *v14;
          --v13;
          ++v14;
          *v15++ = v16;
        }
        while ( v13 != -1 );
      }
      return 0;
    }
  }
  return v3;
}


int mp4_codec_fn_63(int a1, int a2, u32 *a3)
{
  int v3;
  unsigned int v4;
  unsigned int v5;
  u8 *v6;
  u8 *i;
  int v8;
  s64 v9;

  a2 = (u16)a2;
  v3 = 6;
  if ( (u16)a2 != 0 )
  {
    v4 = *(u32 *)(a1 + 12);
    v5 = 0;
    if ( v4 != 0 )
    {
      v6 = *(u8 **)(a1 + 16);
      for ( i = v6; ; i += 8 )
      {
        v8 = *v6;
        ++v5;
        v9 = ((u64)(v9) & 0xFFFFFFFF00000000ULL) | (u32)(v8 == 1);
        v9 = ((u64)(v9) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v8 == 0x11) << 32);
        v6 += 8;
        if ( v9 != 0 )
          a2 = (u16)(a2 - 1);
        if ( a2 == 0 )
          break;
        if ( v5 >= v4 )
          return 8194;
      }
      v3 = 0;
      *a3 = *((u32 *)i + 1);
    }
    else
    {
      return 8194;
    }
  }
  return v3;
}


int mp4_codec_fn_64(int a1, unsigned int a2, u32 *a3)
{
  unsigned int v3;
  unsigned int v4;
  int i;
  int result;
  int v7;

  v3 = *(u32 *)(a1 + 12);
  v4 = 0;
  if ( v3 == 0 )
    return 8194;
  for ( i = *(u32 *)(a1 + 16); ; i += 8 )
  {
    ++v4;
    if ( *(u8 *)i == 11 )
    {
      v7 = *(u32 *)(i + 4);
      result = 0;
      if ( *(u8 *)(v7 + 1) == a2 )
        break;
    }
    if ( v4 >= v3 )
      return 8194;
  }
  *a3 = v7;
  return result;
}


int mp4_codec_fn_65(int a1)
{
  unsigned int v1;
  int i;
  int v5;

  v1 = 0;
  if ( *(u32 *)(a1 + 12) != 0 )
  {
    for ( i = *(u32 *)(a1 + 16); ; i = *(u32 *)(a1 + 16) )
    {
      v5 = 8 * v1++;
      mp4_codec_fn_69(*(u32 *)(v5 + i + 4), *(u8 *)(v5 + i));
      if ( v1 >= *(u32 *)(a1 + 12) )
        break;
    }
  }
  mp4_box_fn_9();
  mp4_box_fn_9();
  return 0;
}


int mp4_codec_fn_66(u8 *a1)
{
  mp4_codec_fn_69(*((u32 *)a1 + 1), *a1);
  mp4_track_fn_65((int)a1);
  return 0;
}


int mp4_codec_fn_67(int a1, u16 *a2)
{
  unsigned int v2;
  unsigned int v3;
  u8 *v4;
  int i;
  s64 v6;

  v2 = *(u32 *)(a1 + 12);
  *a2 = 0;
  v3 = 0;
  if ( v2 != 0 )
  {
    v4 = *(u8 **)(a1 + 16);
    for ( i = *v4; ; i = *v4 )
    {
      ++v3;
      v6 = ((u64)(v6) & 0xFFFFFFFF00000000ULL) | (u32)(i == 1);
      v6 = ((u64)(v6) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(i == 0x11) << 32);
      v4 += 8;
      if ( v6 != 0 )
        ++*a2;
      if ( v3 >= v2 )
        break;
    }
  }
  return 0;
}


int mp4_codec_fn_68(int a1, int a2, int a3)
{
  int v6;
  int v7;
  int v8;

  v6 = mp4_sample_fn_7((unsigned int *)(a1 + 16), 8 * *(u32 *)(a1 + 12), 8 * *(u32 *)(a1 + 12) + 8);
  v7 = 1;
  if ( v6 == 0 )
  {
    v7 = 0;
    *(u8 *)(8 * *(u32 *)(a1 + 12) + *(u32 *)(a1 + 16)) = a2;
    v8 = *(u32 *)(a1 + 12);
    *(u32 *)(8 * v8 + *(u32 *)(a1 + 16) + 4) = a3;
    *(u32 *)(a1 + 12) = v8 + 1;
  }
  return v7;
}


int mp4_codec_fn_69(u32 *a1, int a2)
{
  int v3;
  int v4;
  int v6;
  int v7;

  switch ( a2 )
  {
    case 2:
    case 16:
      v3 = a1[7];
      goto LABEL_3;
    case 3:
      v6 = a1[6];
      if ( v6 != 0 )
        mp4_track_fn_65(v6);
      v4 = a1[13];
      goto LABEL_6;
    case 11:
      v4 = a1[1];
      goto LABEL_6;
    case 17:
      v7 = a1[6];
      if ( v7 != 0 )
        mp4_track_fn_65(v7);
      v3 = a1[8];
LABEL_3:
      if ( v3 != 0 )
        mp4_track_fn_65(v3);
      v4 = a1[3];
LABEL_6:
      if ( v4 != 0 )
        mp4_track_fn_65(v4);
      mp4_track_fn_65((int)a1);
      break;
    default:
      return 0;
  }
  return 0;
}


int mp4_codec_fn_70(unsigned int a1, int a2, int *a3)
{
  int v6;
  unsigned int v7;
  int v8;
  u8 *v9;
  int v10;
  int v11;
  int v12;
  u8 *v13;
  unsigned int v14;
  unsigned int v15;
  u8 v16;
  u8 *v17;
  int v18;
  unsigned int v19;
  u8 *v20;
  int v22;
  unsigned int v23;
  unsigned int v24;
  u8 v25[16];
  char v26;
  u8 v27[3];
  u32 v28[3];

  while ( 1 )
  {
    v6 = mp4_codec_fn_82(a2, a1, &v26, v28, v27);
    if ( v6 != 0 )
      return v6;
    v6 = 8195;
    v7 = a1 - v27[0];
    v8 = a2 + v27[0];
    if ( v7 < v28[0] )
      return v6;
    if ( v26 == 16 )
    {
      mp4_codec_fn_83(v25, v8);
      v9 = (u8 *)mp4_sample_fn_5(8);
      v10 = (int)v9;
      v6 = 1;
      if ( v9 != NULL )
      {
        *v9 = 16;
        v11 = mp4_sample_fn_5(36);
        v12 = v11;
        *(u32 *)(v10 + 4) = v11;
        if ( v11 != 0 )
        {
          *(u8 *)v11 = 16;
          v6 = 8195;
          if ( v7 >= 2 )
          {
            *(u16 *)(v11 + 2) = mp4_codec_fn_84(v25, 10);
            *(u32 *)(v12 + 4) = mp4_codec_fn_84(v25, 1);
            *(u32 *)(v12 + 16) = mp4_codec_fn_84(v25, 1);
            mp4_codec_fn_84(v25, 4);
            v13 = (u8 *)(v8 + 2);
            v14 = v7 - 2;
            if ( *(u32 *)(v12 + 4) == 1 )
            {
              v22 = *v13;
              v23 = (unsigned int)(v13 + 1);
              *(u8 *)(v12 + 8) = v22;
              v24 = mp4_sample_fn_5(v22);
              *(u32 *)(v12 + 12) = v24;
              if ( v24 != 0 )
              {
                mp4_box_fn_23(v24, *(u8 *)(v12 + 8), v23, *(u8 *)(v12 + 8));
                return 0;
              }
              else
              {
                mp4_codec_fn_66((u8 *)v10);
                return 1;
              }
            }
            else
            {
              v6 = 8195;
              if ( v14 >= 5 )
              {
                v15 = v14 - 5;
                *(u8 *)(v12 + 20) = *v13;
                *(u8 *)(v12 + 21) = v13[1];
                *(u8 *)(v12 + 22) = v13[2];
                *(u8 *)(v12 + 23) = v13[3];
                v16 = v13[4];
                v17 = v13 + 5;
                *(u8 *)(v12 + 24) = v16;
                if ( v15 < 2 )
                {
LABEL_16:
                  v6 = 8195;
                  if ( v15 == 0 )
                  {
                    *a3 = v10;
                    return 0;
                  }
                }
                else
                {
                  while ( 1 )
                  {
                    v6 = mp4_codec_fn_82(v17, v15, &v26, v28, v27);
                    if ( v6 != 0 )
                      break;
                    v18 = v28[0];
                    v6 = 8195;
                    v19 = v15 - v27[0];
                    v20 = &v17[v27[0]];
                    if ( v19 < v28[0] )
                      break;
                    if ( v26 == 14 )
                    {
                      v6 = mp4_codec_fn_81(v12, v28[0], v20, 8195);
                      if ( v6 != 0 )
                        return v6;
                      v18 = v28[0];
                    }
                    v15 = v19 - v18;
                    v17 = &v20[v18];
                    if ( v15 < 2 )
                      goto LABEL_16;
                  }
                }
              }
            }
          }
        }
        else
        {
          mp4_track_fn_65(v10);
          return 1;
        }
      }
      return v6;
    }
    a1 = v7 - v28[0];
    a2 = v8 + v28[0];
  }
}


int mp4_codec_fn_71(unsigned int a1, int a2, int *a3)
{
  int v6;
  unsigned int v7;
  int v8;
  u8 *v9;
  int v10;
  int v11;
  int v12;
  u8 *v13;
  unsigned int v14;
  int v15;
  int v16;
  unsigned int v17;
  u8 *v18;
  int v20;
  unsigned int v21;
  unsigned int v22;
  u8 v23[16];
  char v24;
  u8 v25[3];
  u32 v26[3];

  while ( 1 )
  {
    v6 = mp4_codec_fn_82(a2, a1, &v24, v26, v25);
    if ( v6 != 0 )
      return v6;
    v6 = 8195;
    v7 = a1 - v25[0];
    v8 = a2 + v25[0];
    if ( v7 < v26[0] )
      return v6;
    if ( v24 == 3 )
      break;
    a1 = v7 - v26[0];
    a2 = v8 + v26[0];
  }
  mp4_codec_fn_83(v23, v8);
  v9 = (u8 *)mp4_sample_fn_5(8);
  v10 = (int)v9;
  v6 = 1;
  if ( v9 != NULL )
  {
    *v9 = 3;
    v11 = mp4_sample_fn_5(136);
    v12 = v11;
    *(u32 *)(v10 + 4) = v11;
    if ( v11 == 0 )
    {
      mp4_track_fn_65(v10);
      return 1;
    }
    *(u8 *)v11 = 3;
    v6 = 8195;
    if ( v7 >= 3 )
    {
      *(u16 *)(v11 + 2) = mp4_codec_fn_84(v23, 16);
      *(u32 *)(v12 + 4) = mp4_codec_fn_84(v23, 1);
      *(u32 *)(v12 + 8) = mp4_codec_fn_84(v23, 1);
      *(u32 *)(v12 + 12) = mp4_codec_fn_84(v23, 1);
      mp4_codec_fn_84(v23, 5);
      v13 = (u8 *)(v8 + 3);
      v14 = v7 - 3;
      if ( *(u32 *)(v12 + 4) == 1 )
      {
        v14 -= 2;
        *(u16 *)(v12 + 18) = v13[1] + (*v13 << 8);
        v13 += 2;
      }
      if ( *(u32 *)(v12 + 8) == 1 )
      {
        v15 = *v13;
        v21 = v14 - 1;
        ++v13;
        *(u8 *)(v12 + 20) = v15;
        v22 = mp4_sample_fn_5(v15);
        *(u32 *)(v12 + 24) = v22;
        if ( v22 == 0 )
        {
          mp4_codec_fn_66((u8 *)v10);
          return 1;
        }
        mp4_box_fn_23(v22, *(u8 *)(v12 + 20), (unsigned int)v13, *(u8 *)(v12 + 20));
        v14 = v21 - *(u8 *)(v12 + 20);
      }
      if ( *(u32 *)(v12 + 12) == 1 )
      {
        v14 -= 2;
        *(u16 *)(v12 + 28) = v13[1] + (*v13 << 8);
        v13 += 2;
      }
      for ( ; v14 >= 2; v13 = &v18[v16] )
      {
        v6 = mp4_codec_fn_82(v13, v14, &v24, v26, v25);
        if ( v6 != 0 )
          return v6;
        v16 = v26[0];
        v6 = 8195;
        v17 = v14 - v25[0];
        v18 = &v13[v25[0]];
        if ( v17 < v26[0] )
          return v6;
        if ( v24 == 4 )
        {
          v20 = mp4_codec_fn_78(v12, v26[0], v18);
        }
        else
        {
          if ( v24 != 6 )
            goto LABEL_20;
          v20 = mp4_codec_fn_80(v12, v26[0], v18);
        }
        v6 = v20;
        if ( v20 != 0 )
          return v6;
        v16 = v26[0];
LABEL_20:
        v14 = v17 - v16;
      }
      v6 = 8195;
      if ( v14 == 0 )
      {
        *a3 = v10;
        return 0;
      }
    }
  }
  return v6;
}


int mp4_codec_fn_72(int a1, unsigned int a2, int a3)
{
  int v4;
  unsigned int v5;
  int i;
  int v7;
  unsigned int v8;
  unsigned int v9;
  int v10;
  char v12;
  char v13[3];
  unsigned int v14[3];

  v4 = a3;
  v5 = a2;
  if ( a2 < 2 )
  {
LABEL_8:
    v7 = 8195;
    if ( v5 == 0 )
      return 0;
  }
  else
  {
    for ( i = a3; ; i = v4 )
    {
      v7 = mp4_codec_fn_82(i, v5, &v12, v14, v13);
      if ( v7 != 0 )
        break;
      v8 = v14[0];
      v7 = 8195;
      v9 = v5 - (u8)v13[0];
      v10 = v4 + (u8)v13[0];
      if ( v9 < v14[0] )
        break;
      if ( v12 == 17 )
      {
        v7 = mp4_codec_fn_74(a1, v14[0], v10);
        if ( v7 != 0 )
          return v7;
        v8 = v14[0];
      }
      v5 = v9 - v8;
      v4 = v10 + v8;
      if ( v5 < 2 )
        goto LABEL_8;
    }
  }
  return v7;
}


int mp4_codec_fn_73(int a1, unsigned int a2, int a3)
{
  int v4;
  unsigned int v5;
  int i;
  int v7;
  unsigned int v8;
  unsigned int v9;
  int v10;
  char v12;
  char v13[3];
  unsigned int v14[3];

  v4 = a3;
  v5 = a2;
  if ( a2 < 2 )
  {
LABEL_8:
    v7 = 8195;
    if ( v5 == 0 )
      return 0;
  }
  else
  {
    for ( i = a3; ; i = v4 )
    {
      v7 = mp4_codec_fn_82(i, v5, &v12, v14, v13);
      if ( v7 != 0 )
        break;
      v8 = v14[0];
      v7 = 8195;
      v9 = v5 - (u8)v13[0];
      v10 = v4 + (u8)v13[0];
      if ( v9 < v14[0] )
        break;
      if ( v12 == 11 )
      {
        v7 = mp4_codec_fn_77(a1, v14[0], v10);
        if ( v7 != 0 )
          return v7;
        v8 = v14[0];
      }
      v5 = v9 - v8;
      v4 = v10 + v8;
      if ( v5 < 2 )
        goto LABEL_8;
    }
  }
  return v7;
}


int mp4_codec_fn_74(int a1, unsigned int a2, int a3)
{
  int v6;
  int v7;
  int v8;
  int v9;
  unsigned int v10;
  unsigned int v11;
  unsigned int v12;
  int v13;
  int v15;
  int v16;
  unsigned int v17;
  u8 v18[16];
  char v19;
  char v20[3];
  unsigned int v21[3];

  mp4_codec_fn_83(v18, a3);
  v6 = mp4_sample_fn_5(44);
  v7 = v6;
  v8 = 1;
  if ( v6 != 0 )
  {
    v8 = mp4_codec_fn_68(a1, 17, v6);
    if ( v8 == 0 )
    {
      *(u8 *)v7 = 17;
      v8 = 8195;
      if ( a2 >= 2 )
      {
        *(u16 *)(v7 + 2) = mp4_codec_fn_84(v18, 10);
        *(u32 *)(v7 + 4) = mp4_codec_fn_84(v18, 1);
        mp4_codec_fn_84(v18, 5);
        v9 = a3 + 2;
        v10 = a2 - 2;
        if ( *(u32 *)(v7 + 4) == 1 )
        {
          v16 = *(u8 *)(a3 + 2);
          *(u8 *)(v7 + 8) = v16;
          v17 = mp4_sample_fn_5(v16);
          *(u32 *)(v7 + 12) = v17;
          v8 = 1;
          if ( v17 != 0 )
          {
            mp4_box_fn_23(v17, *(u8 *)(v7 + 8), a3 + 3, *(u8 *)(v7 + 8));
            return 0;
          }
        }
        else
        {
          for ( ; v10 >= 2; v9 = v13 + v11 )
          {
            v8 = mp4_codec_fn_82(v9, v10, &v19, v21, v20);
            if ( v8 != 0 )
              return v8;
            v11 = v21[0];
            v8 = 8195;
            v12 = v10 - (u8)v20[0];
            v13 = v9 + (u8)v20[0];
            if ( v12 < v21[0] )
              return v8;
            if ( v19 == 10 )
            {
              v15 = mp4_codec_fn_76(v7, v21[0], v13);
            }
            else
            {
              if ( v19 != 15 )
                goto LABEL_10;
              v15 = mp4_codec_fn_75(v7, v21[0], v13);
            }
            v8 = v15;
            if ( v15 != 0 )
              return v8;
            v11 = v21[0];
LABEL_10:
            v10 = v12 - v11;
          }
          v8 = 8195;
          if ( v10 == 0 )
            return 0;
        }
      }
    }
  }
  return v8;
}


int mp4_codec_fn_75(int a1, int a2, u8 *a3)
{
  int v3;
  int v6;
  unsigned int v8;

  v3 = 8195;
  if ( a2 == 2 )
  {
    if ( *(u32 *)(a1 + 24) != 0 )
    {
      v8 = mp4_sample_fn_5(2 * *(u8 *)(a1 + 28) + 2);
      v3 = 1;
      if ( v8 == 0 )
        return v3;
      mp4_box_fn_23(v8, 2 * *(u8 *)(a1 + 28) + 2, *(u32 *)(a1 + 24), 2 * *(u8 *)(a1 + 28));
      mp4_box_fn_9();
      *(u32 *)(a1 + 24) = v8;
      goto LABEL_4;
    }
    v6 = mp4_sample_fn_5(2);
    *(u32 *)(a1 + 24) = v6;
    v3 = 1;
    if ( v6 != 0 )
    {
LABEL_4:
      *(u16 *)(2 * *(u8 *)(a1 + 28) + *(u32 *)(a1 + 24)) = a3[1] + (*a3 << 8);
      v3 = 0;
      ++*(u8 *)(a1 + 28);
    }
  }
  return v3;
}


int mp4_codec_fn_76(int a1, int a2, u8 *a3)
{
  int v4;
  int v6;
  unsigned int v8;

  v4 = 8195;
  if ( a2 == 1 )
  {
    if ( *(u32 *)(a1 + 32) != 0 )
    {
      v8 = mp4_sample_fn_5(*(u8 *)(a1 + 36) + 1);
      v4 = 1;
      if ( v8 == 0 )
        return v4;
      mp4_box_fn_23(v8, *(u8 *)(a1 + 36) + 1, *(u32 *)(a1 + 32), *(u8 *)(a1 + 36));
      mp4_box_fn_9();
      *(u32 *)(a1 + 32) = v8;
      goto LABEL_4;
    }
    v6 = mp4_sample_fn_5(1);
    *(u32 *)(a1 + 32) = v6;
    v4 = 1;
    if ( v6 != 0 )
    {
LABEL_4:
      v4 = 0;
      *(u8 *)(*(u32 *)(a1 + 32) + (u8)(*(u8 *)(a1 + 36))++) = *a3;
    }
  }
  return v4;
}


int mp4_codec_fn_77(int a1, unsigned int a2, u8 *a3)
{
  int v6;
  int v7;
  int v8;
  unsigned int v9;
  unsigned int v11;

  v6 = mp4_sample_fn_5(12);
  v7 = 1;
  if ( v6 != 0 )
  {
    v7 = mp4_codec_fn_68(a1, 11, v6);
    v8 = a2 < 3;
    if ( v7 == 0 )
    {
      *(u8 *)v6 = 11;
      v9 = a2 - 3;
      v7 = 8195;
      if ( !v8 )
      {
        *(u8 *)(v6 + 1) = *a3;
        *(u16 *)(v6 + 2) = (u8)a3[2] + ((u8)a3[1] << 8);
        if ( a2 != 3 )
        {
          v11 = mp4_sample_fn_5(v9);
          v7 = 1;
          *(u32 *)(v6 + 4) = v11;
          if ( v11 == 0 )
            return v7;
          mp4_box_fn_23(v11, v9, (unsigned int)(a3 + 3), v9);
          *(u32 *)(v6 + 8) = v9;
        }
        return 0;
      }
    }
  }
  return v7;
}


int mp4_codec_fn_78(int a1, unsigned int a2, int a3)
{
  int v6;
  unsigned int v7;
  int v8;
  unsigned int v9;
  unsigned int v10;
  int v11;
  u8 v13[16];
  char v14;
  char v15[3];
  unsigned int v16[3];

  mp4_codec_fn_83(v13, a3);
  *(u8 *)(a1 + 32) = 4;
  v6 = 8195;
  if ( a2 >= 0xD )
  {
    *(u8 *)(a1 + 33) = mp4_codec_fn_84(v13, 8);
    *(u8 *)(a1 + 34) = mp4_codec_fn_84(v13, 6);
    *(u32 *)(a1 + 36) = mp4_codec_fn_84(v13, 1);
    mp4_codec_fn_84(v13, 1);
    *(u32 *)(a1 + 40) = mp4_codec_fn_84(v13, 24);
    *(u32 *)(a1 + 44) = mp4_codec_fn_84(v13, 32);
    v7 = a2 - 13;
    *(u32 *)(a1 + 48) = mp4_codec_fn_84(v13, 32);
    v8 = a3 + 13;
    if ( v7 < 2 )
    {
LABEL_7:
      v6 = 8195;
      if ( v7 == 0 )
        return 0;
    }
    else
    {
      while ( 1 )
      {
        v6 = mp4_codec_fn_82(v8, v7, &v14, v16, v15);
        if ( v6 != 0 )
          break;
        v9 = v16[0];
        v6 = 8195;
        v10 = v7 - (u8)v15[0];
        v11 = v8 + (u8)v15[0];
        if ( v10 < v16[0] )
          break;
        if ( v14 == 5 )
        {
          v6 = mp4_codec_fn_79(a1, v16[0], v11);
          if ( v6 != 0 )
            return v6;
          v9 = v16[0];
        }
        v7 = v10 - v9;
        v8 = v11 + v9;
        if ( v7 < 2 )
          goto LABEL_7;
      }
    }
  }
  return v6;
}


int mp4_codec_fn_79(int a1, unsigned int a2, unsigned int a3)
{
  int v3;
  unsigned int v8;

  v3 = 0;
  if ( *(u32 *)(a1 + 52) == 0 )
  {
    v8 = mp4_sample_fn_5(a2);
    *(u32 *)(a1 + 52) = v8;
    v3 = 1;
    if ( v8 != 0 )
    {
      mp4_box_fn_23(v8, a2, a3, a2);
      *(u32 *)(a1 + 56) = a2;
      return 0;
    }
  }
  return v3;
}


int mp4_codec_fn_80(int a1, int a2, int a3)
{
  int v5;
  char v6;
  unsigned int v7;
  int v8;
  unsigned int v9;
  int v10;
  u8 v11;
  int v12;
  int v13;
  int v14;
  u8 v16[16];

  mp4_codec_fn_83(v16, a3);
  *(u8 *)(a1 + 60) = 6;
  v5 = 8195;
  if ( a2 == 0 )
    return v5;
  v6 = mp4_codec_fn_84(v16, 8);
  *(u8 *)(a1 + 61) = v6;
  v7 = a2 - 1;
  if ( v6 != 0 )
  {
    if ( v6 == 1 )
    {
      *(u32 *)(a1 + 96) = 1000;
      *(u8 *)(a1 + 104) = 32;
      *(u32 *)(a1 + 84) = 0;
    }
    else
    {
      if ( v6 != 2 )
      {
        v8 = *(u32 *)(a1 + 92);
        goto LABEL_6;
      }
      *(u32 *)(a1 + 84) = 1;
    }
  }
  else
  {
    v5 = 8195;
    if ( v7 < 0xF )
      return v5;
    *(u32 *)(a1 + 64) = mp4_codec_fn_84(v16, 1);
    *(u32 *)(a1 + 68) = mp4_codec_fn_84(v16, 1);
    *(u32 *)(a1 + 72) = mp4_codec_fn_84(v16, 1);
    *(u32 *)(a1 + 76) = mp4_codec_fn_84(v16, 1);
    *(u32 *)(a1 + 80) = mp4_codec_fn_84(v16, 1);
    *(u32 *)(a1 + 84) = mp4_codec_fn_84(v16, 1);
    *(u32 *)(a1 + 88) = mp4_codec_fn_84(v16, 1);
    *(u32 *)(a1 + 92) = mp4_codec_fn_84(v16, 1);
    *(u32 *)(a1 + 96) = mp4_codec_fn_84(v16, 32);
    *(u32 *)(a1 + 100) = mp4_codec_fn_84(v16, 32);
    *(u8 *)(a1 + 104) = mp4_codec_fn_84(v16, 8);
    *(u8 *)(a1 + 105) = mp4_codec_fn_84(v16, 8);
    *(u8 *)(a1 + 106) = mp4_codec_fn_84(v16, 8);
    *(u8 *)(a1 + 107) = mp4_codec_fn_84(v16, 8);
    *(u8 *)(a1 + 108) = mp4_codec_fn_84(v16, 4);
    *(u8 *)(a1 + 109) = mp4_codec_fn_84(v16, 5);
    *(u8 *)(a1 + 110) = mp4_codec_fn_84(v16, 5);
    mp4_codec_fn_84(v16, 2);
    v7 -= 15;
  }
  v8 = *(u32 *)(a1 + 92);
LABEL_6:
  if ( v8 == 1 )
  {
    v5 = 8195;
    if ( v7 < 8 )
      return v5;
    *(u32 *)(a1 + 112) = mp4_codec_fn_84(v16, 32);
    *(u16 *)(a1 + 116) = mp4_codec_fn_84(v16, 16);
    *(u16 *)(a1 + 118) = mp4_codec_fn_84(v16, 16);
    v7 -= 8;
  }
  v5 = 8195;
  if ( *(u32 *)(a1 + 84) == 0 )
  {
    v9 = *(u8 *)(a1 + 104);
    if ( v7 < (int)(2 * v9 - 1) / 8 + 1 )
      return v5;
    if ( v9 >= 0x21 )
    {
      v5 = 8195;
      if ( v9 >= 0x41 )
        return v5;
      *(u32 *)(a1 + 124) = mp4_codec_fn_84(v16, (u8)(*(u8 *)(a1 + 104) - 32));
      v9 = 32;
    }
    v10 = mp4_codec_fn_84(v16, v9);
    v11 = *(u8 *)(a1 + 104);
    v12 = v11;
    *(u32 *)(a1 + 120) = v10;
    if ( v11 >= 0x21u )
    {
      v5 = 8195;
      if ( v11 >= 0x41u )
        return v5;
      *(u32 *)(a1 + 132) = mp4_codec_fn_84(v16, (u8)(v11 - 32));
      v12 = 32;
    }
    v13 = mp4_codec_fn_84(v16, v12);
    v14 = *(u8 *)(a1 + 104);
    *(u32 *)(a1 + 128) = v13;
    v7 = v7 - (2 * v14 - 1) / 8 - 1;
    v5 = 8195;
  }
  if ( v7 == 0 )
    return 0;
  return v5;
}


int mp4_codec_fn_81(int a1, int a2, u8 *a3)
{
  int v3;
  int v6;
  unsigned int v8;

  v3 = 8195;
  if ( a2 == 4 )
  {
    if ( *(u32 *)(a1 + 28) != 0 )
    {
      v8 = mp4_sample_fn_5(4 * *(u8 *)(a1 + 32) + 4);
      v3 = 1;
      if ( v8 == 0 )
        return v3;
      mp4_box_fn_23(v8, 4 * *(u8 *)(a1 + 32) + 4, *(u32 *)(a1 + 28), 4 * *(u8 *)(a1 + 32));
      mp4_box_fn_9();
      *(u32 *)(a1 + 28) = v8;
      goto LABEL_4;
    }
    v6 = mp4_sample_fn_5(4);
    *(u32 *)(a1 + 28) = v6;
    v3 = 1;
    if ( v6 != 0 )
    {
LABEL_4:
      *(u32 *)(4 * *(u8 *)(a1 + 32) + *(u32 *)(a1 + 28)) = (*a3 << 24)
                                                                            + (a3[1] << 16)
                                                                            + (a3[2] << 8)
                                                                            + a3[3];
      v3 = 0;
      ++*(u8 *)(a1 + 32);
    }
  }
  return v3;
}


int mp4_codec_fn_82(u8 *a1, unsigned int a2, u8 *a3, u32 *a4, u8 *a5)
{
  int v5;
  u8 *v6;
  unsigned int v7;
  unsigned int i;
  unsigned int v9;
  int v10;

  v5 = 8195;
  if ( a2 >= 2 )
  {
    *a4 = 0;
    v6 = a1 + 1;
    v7 = a2 - 1;
    *a5 = 0;
    *a3 = *a1;
    ++*a5;
    if ( v7 != 0 )
    {
      for ( i = *v6; ; i = *v6 )
      {
        --v7;
        *a4 = (*a4 << 7) + (i & 0x7F);
        v9 = i >> 7;
        ++v6;
        v5 = 0;
        v10 = (u8)++*a5;
        if ( v9 == 0 )
          break;
        v5 = 8195;
        if ( v10 == 5 )
          break;
        if ( v7 == 0 )
          return 8195;
      }
    }
  }
  return v5;
}


int mp4_codec_fn_83(int a1, int a2)
{
  *(u32 *)a1 = a2;
  *(u8 *)(a1 + 4) = 0;
  return 0;
}


int mp4_codec_fn_84(u8 **a1, unsigned int a2)
{
  unsigned int v2;
  int v3;
  char i;
  int v5;
  unsigned int v6;

  v2 = a2;
  v3 = 0;
  if ( a2 != 0 )
  {
    for ( i = *((u8 *)a1 + 4); ; i = *((u8 *)a1 + 4) )
    {
      v5 = (int)*a1;
      v6 = (u8)(8 - i);
      if ( v2 < v6 )
        v6 = ((v6) & ~0xFFU) | ((u8)(v2));
      v3 = (v3 << v6) + (u8)((int)(u8)(**a1 << i) >> (8 - v6));
      *((u8 *)a1 + 4) = v6 + i;
      v2 = (u8)(v2 - v6);
      if ( (u8)(v6 + i) == 8 )
      {
        *a1 = (u8 *)(v5 + 1);
        *((u8 *)a1 + 4) = 0;
      }
      if ( v2 == 0 )
        break;
    }
  }
  return v3;
}


int mp4_codec_fn_85(int a1, int a2, int *a3)
{
  int v6;
  int result;
  int v8;

  v6 = mp4_sample_fn_5(20);
  result = 1;
  if ( v6 != 0 )
  {
    v8 = mp4_codec_fn_86(v6, a1, a2);
    if ( v8 != 0 )
    {
      mp4_codec_fn_65(v6);
      return v8;
    }
    else
    {
      *a3 = v6;
      return 0;
    }
  }
  return result;
}


int mp4_codec_fn_86(u8 *a1, unsigned int a2, u8 *a3)
{
  unsigned int i;
  int v6;
  unsigned int v7;
  unsigned int v8;
  int v9;
  char v11;
  u8 v12[3];
  unsigned int v13[3];

  for ( i = a2; i >= 2; a3 = (u8 *)(v9 + v7) )
  {
    v6 = mp4_codec_fn_82(a3, i, &v11, v13, v12);
    if ( v6 != 0 )
      return v6;
    v7 = v13[0];
    v6 = 8195;
    v8 = i - v12[0];
    v9 = (int)&a3[v12[0]];
    if ( v8 < v13[0] )
      return v6;
    if ( v11 == 1 )
    {
      if ( *a1 != 0 )
      {
        i = v8 - v13[0];
        continue;
      }
      v6 = mp4_codec_fn_72((int)a1, v13[0], v9);
      if ( v6 != 0 )
        return v6;
      v7 = v13[0];
      ++*a1;
    }
    else if ( v11 == 5 )
    {
      if ( a1[1] != 0 )
      {
        i = v8 - v13[0];
        continue;
      }
      v6 = mp4_codec_fn_73((int)a1, v13[0], v9);
      if ( v6 != 0 )
        return v6;
      v7 = v13[0];
      ++a1[1];
    }
    i = v8 - v7;
  }
  v6 = 8195;
  if ( i == 0 )
    return 0;
  return v6;
}


int mp4_codec_fn_87(int a1, unsigned int a2, unsigned int a3, int a4, int *a5)
{
  u32 *v9;
  int result;
  unsigned int v11;
  int v12;
  int v13;
  int v14;
  u32 v15[4];

  *a5 = 0;
  v15[0] = a2;
  v9 = (u32 *)mp4_codec_fn_92(a1, v15);
  result = 20481;
  if ( v9 != NULL )
  {
    v11 = a2;
    if ( a3 < a2 )
    {
      return 0;
    }
    else
    {
      while ( 1 )
      {
        v12 = v15[0];
        ++v11;
        if ( *v9 == v15[0] )
        {
          v9 = (u32 *)v9[3];
          v15[0] = 0;
          v12 = 0;
          result = 20481;
          if ( v9 == NULL )
            break;
        }
        v13 = *a5;
        v14 = *(u32 *)(4 * (v9[2] * v12 + a4) + v9[4]);
        v15[0] = v12 + 1;
        *a5 = v13 + v14;
        if ( a3 < v11 )
          return 0;
      }
    }
  }
  return result;
}


int mp4_codec_fn_88(unsigned int *a1, unsigned int a2, int a3, u32 *a4)
{
  int v4;
  int v5;
  unsigned int v6;
  unsigned int v7;
  unsigned int *v8;
  int v9;
  int v10; // dc

  v4 = 0;
  v5 = 20481;
  if ( a1 != NULL )
  {
    v6 = *a1;
    if ( *a1 != 0 )
    {
      while ( 1 )
      {
        v7 = 0;
        if ( v6 != 0 )
          break;
LABEL_7:
        a1 = (unsigned int *)a1[3];
        v4 += v6;
        if ( a1 == NULL )
        {
          *a4 = v4 - 1;
          return 0;
        }
        v6 = *a1;
      }
      v8 = (unsigned int *)(4 * a3 + a1[4]);
      v9 = v4 - 1;
      while ( 1 )
      {
        v10 = a2 < *v8;
        v8 += a1[2];
        if ( v10 )
          break;
        ++v7;
        ++v9;
        if ( v7 >= v6 )
          goto LABEL_7;
      }
      v5 = 20481;
      if ( (v4 | v7) != 0 )
      {
        v5 = 0;
        *a4 = v9;
      }
    }
  }
  return v5;
}


int mp4_codec_fn_89(u32 **a1, unsigned int a2, int a3, int a4)
{
  u32 *v8;
  u32 *v9;
  int v10;
  u32 *v11;
  int v12;
  int v13;
  unsigned int v14;
  int v15;
  u32 *v16;
  int v17;
  unsigned int v18;
  unsigned int v19;
  int v20;
  int i;
  unsigned int v22;
  int v24;

  v8 = *a1;
  v9 = *a1;
  if ( *a1 != NULL )
  {
    v10 = 20481;
    if ( v8[2] < a2 )
      return v10;
  }
  else
  {
    v8 = (u32 *)mp4_sample_fn_5(28);
    *a1 = v8;
    v10 = 1;
    if ( v8 == NULL )
      return v10;
  }
  if ( v8[4] != 0 )
  {
    v11 = (u32 *)v8[3];
  }
  else
  {
    if ( mp4_codec_fn_97(v8, a4, a2) != 0 )
    {
      if ( v9 == NULL )
        mp4_track_fn_65((int)v8);
      v10 = 1;
      *a1 = NULL;
      return v10;
    }
    v11 = (u32 *)v8[3];
  }
  if ( v11 != NULL )
  {
    do
    {
      v8 = v11;
      v11 = (u32 *)v11[3];
    }
    while ( v11 != NULL );
    v12 = v8[1];
  }
  else
  {
    v12 = v8[1];
  }
  v13 = *v8;
  if ( v12 == *v8 )
  {
    v24 = mp4_sample_fn_5(28);
    v8[3] = v24;
    v10 = 1;
    if ( v24 == 0 )
      return v10;
    if ( mp4_codec_fn_97(v24, a4, v8[2]) != 0 )
    {
      mp4_track_fn_65(v8[3]);
      v10 = 1;
      v8[3] = 0;
      return v10;
    }
    v8 = (u32 *)v8[3];
    v13 = *v8;
  }
  v14 = 0;
  if ( a2 != 0 )
  {
    v15 = v8[4];
    while ( 1 )
    {
      v16 = (u32 *)(4 * v14 + a3);
      v17 = 4 * (v13 * v8[2] + v14++);
      *(u32 *)(v17 + v15) = *v16;
      if ( v14 >= a2 )
        break;
      v13 = *v8;
    }
    v13 = *v8;
  }
  v18 = v8[2];
  v19 = a2;
  if ( a2 < v18 )
  {
    v20 = v8[4];
    for ( i = v13 * v18; ; i = v13 * v22 )
    {
      *(u32 *)(4 * (i + v19++) + v20) = 0;
      v22 = v8[2];
      v13 = *v8;
      if ( v19 >= v22 )
        break;
    }
  }
  *v8 = v13 + 1;
  return 0;
}


int mp4_codec_fn_90(int *a1, int a2, unsigned int a3, unsigned int a4, int a5)
{
  int v8;
  int v9;
  int v10;
  int v11;
  u32 v13[4];

  v8 = *a1;
  v13[0] = a2;
  if ( v8 == 0 )
    return 20481;
  v9 = *(u32 *)(v8 + 8) < a3 || a4 >= a3;
  v10 = 20481;
  if ( !v9 )
  {
    v11 = mp4_codec_fn_92(v8, v13);
    if ( v11 != 0 )
    {
      v10 = 0;
      *(u32 *)(4 * (v13[0] * a3 + a4) + *(u32 *)(v11 + 16)) = a5;
      return v10;
    }
    return 20481;
  }
  return v10;
}


int mp4_codec_fn_91(int a1, int a2, unsigned int a3, u32 *a4)
{
  int v6;
  int v7;
  u32 v9[4];

  v9[0] = a2;
  if ( a1 == 0 || (v6 = 20481, a3 < *(u32 *)(a1 + 8)) )
  {
    v7 = mp4_codec_fn_92(a1, v9);
    v6 = 20481;
    if ( v7 != 0 )
    {
      v6 = 0;
      *a4 = *(u32 *)(4 * (v9[0] * *(u32 *)(v7 + 8) + a3) + *(u32 *)(v7 + 16));
    }
  }
  return v6;
}


u32 *mp4_codec_fn_92(u32 *a1, int *a2)
{
  u32 *result;
  u32 *v3;
  int v4;

  result = NULL;
  if ( a1 != NULL )
  {
    result = a1;
    if ( *a1 < (unsigned int)(*a2 + 1) )
    {
      while ( 1 )
      {
        v3 = (u32 *)a1[3];
        result = NULL;
        if ( v3 == NULL )
          break;
        v4 = *a2 - *a1;
        *a2 = v4;
        a1 = v3;
        if ( *v3 >= (unsigned int)(v4 + 1) )
          return v3;
      }
    }
  }
  return result;
}


u32 *mp4_codec_fn_93(u32 *a1)
{
  u32 *v1;
  int i;
  u32 *v3;
  int v4;
  int v5;
  u32 *result;
  u32 *v7;
  u32 *v8;
  int v9;

  v1 = a1;
  if ( *a1 != 0 )
  {
LABEL_12:
    v7 = v1;
    if ( v1[3] != 0 )
    {
      v8 = (u32 *)v1[3];
      do
      {
        if ( *v8 != 0 )
        {
          v7 = v8;
        }
        else
        {
          v9 = v8[3];
          if ( v8[4] != 0 )
          {
            mp4_track_fn_65(v8[4]);
            v8 = (u32 *)v7[3];
          }
          if ( v8[5] != 0 )
          {
            mp4_track_fn_65(v8[5]);
            v8 = (u32 *)v7[3];
          }
          if ( v8[6] != 0 )
            mp4_track_fn_65(v8[6]);
          mp4_track_fn_65(v7[3]);
          v7[3] = v9;
        }
        v8 = (u32 *)v7[3];
      }
      while ( v8 != NULL );
    }
    return v1;
  }
  else
  {
    for ( i = a1[4]; ; i = v3[4] )
    {
      v3 = (u32 *)v1[3];
      if ( i != 0 )
        mp4_track_fn_65(i);
      v4 = v1[5];
      if ( v4 != 0 )
        mp4_track_fn_65(v4);
      v5 = v1[6];
      if ( v5 != 0 )
        mp4_track_fn_65(v5);
      mp4_track_fn_65((int)v1);
      v1 = v3;
      result = NULL;
      if ( v3 == NULL )
        break;
      if ( *v3 != 0 )
        goto LABEL_12;
    }
  }
  return result;
}


int mp4_codec_fn_94(u32 **a1, unsigned int a2, u32 *a3, int a4)
{
  u32 *v8;
  u32 *v9;
  int v10;
  u32 *v11;
  int v12;
  int v13;
  unsigned int v14;
  unsigned int v15;
  u32 *v16;
  u32 *v17;
  int v18;
  unsigned int v19;
  u32 *v20;
  int v22;

  v8 = *a1;
  v9 = *a1;
  if ( *a1 != NULL )
  {
    v10 = 20481;
    if ( v8[2] < a2 )
      return v10;
  }
  else
  {
    v8 = (u32 *)mp4_sample_fn_5(28);
    *a1 = v8;
    v10 = 1;
    if ( v8 == NULL )
      return v10;
  }
  if ( v8[6] != 0 )
  {
    v11 = (u32 *)v8[3];
  }
  else
  {
    if ( mp4_codec_fn_98(v8, a4, a2) != 0 )
    {
      if ( v9 == NULL )
        mp4_track_fn_65((int)v8);
      v10 = 1;
      *a1 = NULL;
      return v10;
    }
    v11 = (u32 *)v8[3];
  }
  if ( v11 != NULL )
  {
    do
    {
      v8 = v11;
      v11 = (u32 *)v11[3];
    }
    while ( v11 != NULL );
    v12 = v8[1];
  }
  else
  {
    v12 = v8[1];
  }
  v13 = *v8;
  if ( v12 == *v8 )
  {
    v22 = mp4_sample_fn_5(28);
    v8[3] = v22;
    v10 = 1;
    if ( v22 == 0 )
      return v10;
    if ( mp4_codec_fn_98(v22, a4, v8[2]) != 0 )
    {
      mp4_track_fn_65(v8[3]);
      v10 = 1;
      v8[3] = 0;
      return v10;
    }
    v8 = (u32 *)v8[3];
    v13 = *v8;
  }
  v14 = 0;
  if ( a2 != 0 )
  {
    v15 = v8[2];
    v16 = a3;
    v17 = (u32 *)(8 * v13 * v15 + v8[6]);
    do
    {
      v18 = v16[1];
      ++v14;
      *v17 = *v16;
      v17[1] = v18;
      v16 += 2;
      v17 += 2;
    }
    while ( v14 < a2 );
  }
  else
  {
    v15 = v8[2];
  }
  v19 = a2;
  if ( a2 < v15 )
  {
    v20 = (u32 *)(8 * (v13 * v15 + a2) + v8[6]);
    do
    {
      ++v19;
      *v20 = 0;
      v20[1] = 0;
      v20 += 2;
    }
    while ( v19 < v15 );
  }
  *v8 = v13 + 1;
  return 0;
}


int mp4_codec_fn_95(u32 **a1, int a2, unsigned int a3, unsigned int a4, int a5, int a6)
{
  u32 *v10;
  int v11;
  int v12;
  u32 *v13;
  u32 *v14;
  int v16[4];

  v10 = *a1;
  v16[0] = a2;
  if ( v10 == NULL )
    return 20481;
  v11 = v10[2] < a3 || a4 >= a3;
  v12 = 20481;
  if ( !v11 )
  {
    v13 = mp4_codec_fn_92(v10, v16);
    if ( v13 != NULL )
    {
      v12 = 0;
      v14 = (u32 *)(8 * (v16[0] * a3 + a4) + v13[6]);
      *v14 = a5;
      v14[1] = a6;
      return v12;
    }
    return 20481;
  }
  return v12;
}


int mp4_codec_fn_96(u32 *a1, int a2, unsigned int a3, u32 *a4)
{
  int v6;
  u32 *v7;
  u32 *v8;
  int v9;
  int v11[4];

  v11[0] = a2;
  if ( a1 == NULL || (v6 = 20481, a3 < a1[2]) )
  {
    v7 = mp4_codec_fn_92(a1, v11);
    v6 = 20481;
    if ( v7 != NULL )
    {
      v6 = 0;
      v8 = (u32 *)(8 * (v11[0] * v7[2] + a3) + v7[6]);
      v9 = v8[1];
      *a4 = *v8;
      a4[1] = v9;
    }
  }
  return v6;
}


int mp4_codec_fn_97(u32 *a1, int a2, int a3)
{
  int v5;
  int v7;

  v5 = mp4_sample_fn_6(4 * a2 * a3);
  v7 = 1;
  a1[4] = v5;
  if ( v5 != 0 )
  {
    a1[2] = a3;
    v7 = 0;
    a1[1] = a2;
    a1[3] = 0;
    *a1 = 0;
  }
  return v7;
}


int mp4_codec_fn_98(u32 *a1, int a2, int a3)
{
  int v5;
  int v7;

  v5 = mp4_sample_fn_6(8 * a2 * a3);
  v7 = 1;
  a1[6] = v5;
  if ( v5 != 0 )
  {
    a1[2] = a3;
    v7 = 0;
    a1[1] = a2;
    a1[3] = 0;
    *a1 = 0;
  }
  return v7;
}


int mp4_codec_fn_99(u32 *a1)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int result;

  if ( a1 != NULL )
  {
    if ( a1[59] != 0 )
    {
      v3 = a1[64];
      if ( v3 != 0 )
        mp4_track_fn_65(v3);
      v2 = a1[46];
    }
    else
    {
      v2 = a1[46];
    }
    if ( v2 != 0 )
    {
      v5 = a1[56];
      if ( v5 != 0 )
        mp4_track_fn_65(v5);
      v6 = a1[57];
      if ( v6 != 0 )
        mp4_track_fn_65(v6);
      v7 = a1[58];
      if ( v7 != 0 )
        mp4_track_fn_65(v7);
      v4 = a1[75];
    }
    else
    {
      v4 = a1[75];
    }
    if ( v4 != 0 )
      mp4_track_fn_65(v4);
    v8 = a1[77];
    if ( v8 != 0 )
      mp4_track_fn_65(v8);
    mp4_codec_fn_100(a1);
    mp4_track_fn_36(a1[66], v9, v10);
    if ( a1[82] != 0 )
      mp4_codec_fn_110();
    return mp4_track_fn_65((int)a1);
  }
  return result;
}


int mp4_codec_fn_100(u32 *a1, int a2, int a3)
{
  int v4;
  int v5;
  int v6;
  int result;

  if ( a1 != NULL )
  {
    if ( a1[33] != 0 )
      result = mp4_codec_fn_101();
    v4 = (int)a1;
    if ( a1[2] != 0 )
    {
      while ( 1 )
      {
        mp4_track_fn_67(v4, 0);
        result = a1[2];
        if ( result == 0 )
          break;
        v4 = (int)a1;
      }
      v5 = a1[42];
    }
    else
    {
      v5 = a1[42];
    }
    if ( v5 != 0 )
      result = mp4_codec_fn_108();
    if ( a1[43] != 0 )
      result = mp4_codec_fn_109();
    v6 = a1[34];
    if ( v6 != 0 )
      return mp4_track_fn_59(v6, a2, a3);
  }
  return result;
}


int mp4_codec_fn_101(int a1)
{
  int v2;

  v2 = *(u32 *)(a1 + 12);
  if ( v2 != 0 )
    mp4_track_fn_65(v2);
  return mp4_track_fn_65(a1);
}


int mp4_codec_fn_102(u32 *a1)
{
  u32 *v1;
  u32 *v2;
  int v3;
  int v4;
  int result;

  v1 = a1;
  if ( a1 != NULL )
  {
    do
    {
      v2 = (u32 *)v1[3];
      if ( v1[4] != 0 )
        mp4_track_fn_65(v1[4]);
      v3 = v1[5];
      if ( v3 != 0 )
        mp4_track_fn_65(v3);
      v4 = v1[6];
      if ( v4 != 0 )
        mp4_track_fn_65(v4);
      result = mp4_track_fn_65((int)v1);
      v1 = v2;
    }
    while ( v2 != NULL );
  }
  return result;
}


int mp4_codec_fn_103(u32 *a1)
{
  int v2;
  unsigned int v3;
  int *v4;
  int i;

  v2 = a1[9];
  if ( v2 != 0 )
    mp4_track_fn_65(v2);
  if ( a1[10] != 0 )
    mp4_codec_fn_105();
  if ( a1[11] != 0 )
    mp4_codec_fn_106();
  v3 = 0;
  if ( a1[12] != 0 )
    mp4_codec_fn_107();
  v4 = a1 + 13;
  for ( i = a1[13]; ; i = *v4 )
  {
    ++v3;
    ++v4;
    if ( i != 0 )
      mp4_codec_fn_104(i);
    if ( v3 >= 3 )
      break;
  }
  return mp4_track_fn_65((int)a1);
}


int mp4_codec_fn_104(int a1)
{
  int v2;

  v2 = *(u32 *)(a1 + 8);
  if ( v2 != 0 )
    mp4_track_fn_65(v2);
  return mp4_track_fn_65(a1);
}


int mp4_codec_fn_105(int a1)
{
  int v2;

  v2 = *(u32 *)(a1 + 12);
  if ( v2 != 0 )
    mp4_track_fn_65(v2);
  return mp4_track_fn_65(a1);
}


int mp4_codec_fn_106(int a1)
{
  int v2;
  int v3;
  unsigned int v5;
  int *j;
  unsigned int v7;
  unsigned int v8;
  unsigned int v9;
  int *i;
  unsigned int v11;
  unsigned int v12;
  char v13[4];
  int v14;
  u32 v15[4];

  v2 = *(u32 *)(a1 + 16);
  if ( v2 != 0 )
  {
    mp4_codec_fn_61(v2, v15);
    v9 = 0;
    if ( v15[0] != 0 )
    {
      for ( i = *(int **)(a1 + 16); ; i = *(int **)(a1 + 16) )
      {
        v11 = v9++;
        if ( mp4_codec_fn_62(i, v11, v13) != 0 || v14 == 0 )
        {
          v12 = v15[0];
        }
        else
        {
          mp4_track_fn_65(v14);
          v12 = v15[0];
        }
        if ( v9 >= v12 )
          break;
      }
    }
    mp4_codec_fn_60(*(u32 *)(a1 + 16));
    v3 = *(u32 *)(a1 + 24);
  }
  else
  {
    v3 = *(u32 *)(a1 + 24);
  }
  if ( v3 != 0 )
  {
    mp4_codec_fn_61(v3, v15);
    v5 = 0;
    if ( v15[0] != 0 )
    {
      for ( j = *(int **)(a1 + 24); ; j = *(int **)(a1 + 24) )
      {
        v7 = v5++;
        if ( mp4_codec_fn_62(j, v7, v13) != 0 || v14 == 0 )
        {
          v8 = v15[0];
        }
        else
        {
          mp4_track_fn_65(v14);
          v8 = v15[0];
        }
        if ( v5 >= v8 )
          break;
      }
    }
    mp4_codec_fn_60(*(u32 *)(a1 + 24));
  }
  return mp4_track_fn_65(a1);
}


int mp4_codec_fn_107(int a1)
{
  if ( a1 != 0 )
    mp4_track_fn_65(*(u32 *)(a1 + 12));
  return mp4_track_fn_65(a1);
}


int mp4_codec_fn_108(int a1)
{
  int v2;
  int v3;
  int v4;
  unsigned int v5;
  int v6;
  u32 *v7;
  int v8;
  u32 *v9;

  if ( a1 != 0 )
  {
    v2 = *(u32 *)(a1 + 40);
    if ( v2 != 0 )
      mp4_track_fn_65(v2);
    v3 = *(u32 *)(a1 + 60);
    if ( v3 != 0 )
    {
      v5 = 0;
      v6 = 0;
      if ( *(u16 *)(a1 + 52) != 0 )
      {
        while ( 1 )
        {
          mp4_sample_fn_8(v3 + v6 + 12);
          ++v5;
          v6 += 40;
          if ( v5 >= *(u16 *)(a1 + 52) )
            break;
          v3 = *(u32 *)(a1 + 60);
        }
      }
      mp4_track_fn_65(*(u32 *)(a1 + 60));
      v4 = *(u32 *)(a1 + 128);
    }
    else
    {
      v4 = *(u32 *)(a1 + 128);
    }
    if ( v4 != 0 )
    {
      do
      {
        v8 = *(u32 *)(v4 + 20);
        if ( *(u32 *)(v4 + 12) != 0 )
          mp4_track_fn_65(*(u32 *)(v4 + 12));
        mp4_track_fn_65(v4);
        v4 = v8;
      }
      while ( v8 != 0 );
      v7 = *(u32 **)(a1 + 152);
    }
    else
    {
      v7 = *(u32 **)(a1 + 152);
    }
    if ( v7 != NULL )
    {
      do
      {
        v9 = (u32 *)v7[8];
        mp4_codec_fn_103(v7);
        v7 = v9;
      }
      while ( v9 != NULL );
    }
    mp4_track_fn_65(*(u32 *)(a1 + 180));
  }
  return mp4_track_fn_65(a1);
}


int mp4_codec_fn_109(int a1)
{
  unsigned int v2;
  int i;
  int v5;

  if ( a1 != 0 )
  {
    v2 = 0;
    if ( *(u32 *)(a1 + 8) != 0 )
    {
      for ( i = *(u32 *)(a1 + 12); ; i = *(u32 *)(a1 + 12) )
      {
        v5 = 16 * v2++;
        mp4_track_fn_65(*(u32 *)(v5 + i + 12));
        if ( v2 >= *(u32 *)(a1 + 8) )
          break;
      }
    }
    mp4_track_fn_65(*(u32 *)(a1 + 12));
  }
  return mp4_track_fn_65(a1);
}


int mp4_codec_fn_110(u32 *a1)
{
  int result;
  u32 *v2;

  if ( a1 != NULL )
  {
    do
    {
      v2 = (u32 *)*a1;
      result = mp4_track_fn_65((int)a1);
      a1 = v2;
    }
    while ( v2 != NULL );
  }
  return result;
}


int mp4_codec_fn_111(int a1, int a2)
{
  int v3;
  int v4;
  int v5;
  int v7;
  u32 **v8;
  int v9;
  int v10;
  u32 v11[4];

  v3 = *(u32 *)(a1 + 12);
  v11[0] = a2;
  v4 = 0;
  if ( v3 == a2 && a2 != 0 )
  {
    v5 = *(u32 *)(a1 + 16);
  }
  else
  {
    v7 = *(u32 *)(a1 + 16);
    if ( v7 == 0 && a2 != 0 )
    {
      *(u32 *)(a1 + 16) = v7 + 1;
      *(u32 *)(a1 + 12) = a2;
      return v4;
    }
    if ( v3 != 0 )
    {
      v9 = mp4_codec_fn_112(a1);
      v4 = 1;
      if ( v9 != 0 )
        return v4;
      v8 = (u32 **)(a1 + 20);
    }
    else
    {
      v8 = (u32 **)(a1 + 20);
    }
    v10 = mp4_codec_fn_89(v8, 1u, (int)v11, 100);
    v4 = 1;
    if ( v10 != 0 )
      return v4;
    v5 = *(u32 *)(a1 + 16);
    v4 = 0;
  }
  *(u32 *)(a1 + 16) = v5 + 1;
  return v4;
}


int mp4_codec_fn_112(int a1)
{
  unsigned int v1;
  int v3;
  int v4;
  u32 **v5;
  u32 v7[4];

  v1 = 0;
  v3 = *(u32 *)(a1 + 12);
  v4 = *(u32 *)(a1 + 16);
  v7[0] = v3;
  if ( v4 != 0 )
  {
    v5 = (u32 **)(a1 + 20);
    while ( 1 )
    {
      ++v1;
      if ( mp4_codec_fn_89(v5, 1u, (int)v7, 100) != 0 )
        break;
      v5 = (u32 **)(a1 + 20);
      if ( v1 >= *(u32 *)(a1 + 16) )
        goto LABEL_5;
    }
    mp4_codec_fn_102(*(u32 **)(a1 + 20));
    *(u32 *)(a1 + 20) = 0;
    return 1;
  }
  else
  {
LABEL_5:
    *(u32 *)(a1 + 12) = 0;
    return 0;
  }
}


void mp4_codec_fn_113(int a1)
{
  int v2;
  u32 *v3;
  unsigned int v4;
  u32 *v5;
  unsigned int i;
  int v7;

  if ( a1 != 0 && *(u32 *)a1 != 0 )
  {
    v2 = *(u32 *)(a1 + 12);
    *(u32 *)a1 = 8;
    if ( v2 != 0 )
    {
      *(u32 *)(a1 + 12) = 108;
      *(u32 *)a1 = 116;
    }
    v3 = *(u32 **)(a1 + 132);
    if ( v3 != NULL )
      *(u32 *)a1 += *v3;
    v4 = 0;
    if ( *(u32 *)(a1 + 8) != 0 )
    {
      for ( i = 0; ; i = v4 )
      {
        if ( mp4_track_fn_69(a1, i) != 0 )
        {
          v7 = mp4_track_fn_69(a1, v4);
          mp4_codec_fn_114(v7);
          *(u32 *)a1 += *(u32 *)mp4_track_fn_69(a1, v4);
        }
        if ( ++v4 >= *(u32 *)(a1 + 8) )
          break;
      }
      v5 = *(u32 **)(a1 + 136);
    }
    else
    {
      v5 = *(u32 **)(a1 + 136);
    }
    if ( v5 != NULL )
    {
      mp4_track_fn_39(v5);
      *(u32 *)a1 += **(u32 **)(a1 + 136);
    }
    if ( *(u32 *)(a1 + 168) != 0 )
    {
      mp4_codec_fn_124();
      *(u32 *)a1 += **(u32 **)(a1 + 168);
    }
  }
}


int mp4_codec_fn_114(int a1)
{
  int v2;
  int result;

  v2 = *(u32 *)(a1 + 12);
  *(u32 *)a1 = 8;
  if ( v2 != 0 )
  {
    result = 100;
    *(u32 *)(a1 + 12) = 92;
    *(u32 *)a1 = 100;
  }
  if ( *(u32 *)(a1 + 124) != 0 )
  {
    result = mp4_codec_fn_116(a1 + 124);
    *(u32 *)a1 += *(u32 *)(a1 + 124);
  }
  if ( *(u32 *)(a1 + 152) != 0 )
  {
    { mp4_codec_fn_118(a1 + 152); result = 0; }
    *(u32 *)a1 += *(u32 *)(a1 + 152);
  }
  if ( *(u32 *)(a1 + 696) != 0 )
  {
    result = mp4_codec_fn_115();
    *(u32 *)a1 += **(u32 **)(a1 + 696);
  }
  if ( *(u32 *)(a1 + 688) != 0 )
  {
    result = mp4_codec_fn_124();
    *(u32 *)a1 += **(u32 **)(a1 + 688);
  }
  return result;
}


int mp4_codec_fn_115(u32 *a1)
{
  int v1;
  int v2;
  int result;
  int v4;

  v1 = a1[2];
  *a1 = 8;
  if ( v1 != 0 )
  {
    v2 = 4 * *(u16 *)(v1 + 8) + 8;
    *(u32 *)v1 = v2;
    *a1 += v2;
  }
  result = a1[3];
  if ( result != 0 )
  {
    v4 = 4 * *(u16 *)(result + 8) + 8;
    *(u32 *)result = v4;
    *a1 += v4;
  }
  return result;
}


int mp4_codec_fn_116(u32 *a1)
{
  u32 *v2;
  int v3;
  int result;

  v2 = a1 + 2;
  v3 = a1[2];
  *a1 = 8;
  if ( v3 != 0 )
  {
    mp4_codec_fn_117(v2);
    result = a1[2];
    *a1 += result;
  }
  return result;
}


int mp4_codec_fn_117(u32 *a1)
{
  int result;

  result = a1[3];
  *a1 = 12 * result + 16;
  return result;
}


void mp4_codec_fn_118(int *a1)
{
  u32 *v2;
  int v3;
  int v4;
  int v5;
  int v6;

  v2 = a1 + 38;
  v3 = a1[38];
  *a1 = 8;
  if ( v3 != 0 )
  {
    mp4_codec_fn_119(v2);
    *a1 += a1[38];
  }
  if ( a1[2] != 0 )
  {
    v4 = *a1;
    a1[2] = 32;
    *a1 = v4 + 32;
  }
  if ( a1[12] != 0 )
  {
    v5 = mp4_box_fn_12((int)(a1 + 20));
    v6 = *a1;
    a1[12] = v5 + 33;
    *a1 = v6 + v5 + 33;
  }
}


int mp4_codec_fn_119(int *a1)
{
  int v2;
  int result;
  int v4;
  int v5;

  v2 = a1[2];
  *a1 = 8;
  if ( v2 != 0 )
  {
    result = 28;
    a1[2] = 20;
    *a1 = 28;
  }
  if ( a1[7] != 0 )
  {
    v4 = *a1;
    a1[7] = 16;
    *a1 = v4 + 16;
  }
  if ( a1[11] != 0 )
  {
    v5 = *a1;
    a1[11] = 12;
    *a1 = v5 + 12;
  }
  if ( a1[14] != 0 )
  {
    result = mp4_codec_fn_127(a1 + 14);
    *a1 += a1[14];
  }
  if ( a1[94] != 0 )
  {
    result = ((int (*)(void))mp4_codec_fn_120)();
    *a1 += *(u32 *)(a1[94] + 16);
  }
  else if ( a1[26] != 0 )
  {
    mp4_codec_fn_120(a1 + 22);
    result = a1[26];
    *a1 += result;
  }
  return result;
}


int mp4_codec_fn_120(u32 *a1)
{
  int result;

  result = mp4_codec_fn_29(a1);
  if ( a1[6] != 0 )
  {
    mp4_codec_fn_121(a1 + 6, 0, a1[9], *(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(a1[69] + 700) + 152) + 276) + 4));
    result = a1[6];
    a1[4] += result;
  }
  return result;
}


int mp4_codec_fn_121(int a1, int a2, int a3, int a4)
{
  { mp4_codec_fn_122(a1, a2, a3, *(u32 *)(a4 + 80)); return 0; }
}


void mp4_codec_fn_122(u32 *a1, unsigned int a2, int a3)
{
  unsigned int v3;
  unsigned int v5;
  int v6;
  int v7;
  unsigned int v9;
  unsigned int v10;
  u32 **v11;
  u32 *i;
  u32 *v13;
  int v14;
  u32 v15[4];

  v3 = a2 + a3;
  *a1 = 16;
  if ( a2 < a2 + a3 )
  {
    v5 = a2 + 1;
    do
    {
      v6 = mp4_track_fn_73(a1, v5, v15);
      v7 = v15[0];
      if ( v6 == 0 )
      {
        v9 = *(u32 *)(v15[0] + 16);
        if ( v9 == 1868854125 )
        {
          *(u32 *)v15[0] = 16;
        }
        else if ( v9 > 0x6F64736D )
        {
          if ( v9 == 1936684398 )
          {
            v14 = 52;
            if ( **(u16 **)(v15[0] + 36) == 0 )
              v14 = 36;
            *(u32 *)v15[0] = v14;
          }
          else if ( v9 == 1986618469 )
          {
            *(u32 *)v15[0] = 86;
          }
        }
        else if ( v9 == 1196573008 )
        {
          *(u32 *)v15[0] = 40;
        }
        v10 = 0;
        v11 = (u32 **)(v7 + 52);
        for ( i = *(u32 **)(v7 + 52); ; i = *v11 )
        {
          ++v10;
          ++v11;
          if ( i != NULL )
            *(u32 *)v7 += *i;
          if ( v10 >= 3 )
            break;
        }
        v13 = *(u32 **)(v7 + 40);
        if ( v13 != NULL )
          *(u32 *)v7 += *v13;
        if ( *(u32 *)(v7 + 44) != 0 )
        {
          mp4_codec_fn_123();
          *(u32 *)v7 += **(u32 **)(v7 + 44);
        }
        *a1 += *(u32 *)v7;
      }
    }
    while ( v5++ < v3 );
  }
}


void mp4_codec_fn_123(int a1)
{
  int v2;
  int v3;
  unsigned int v4;
  char v5;
  int *j;
  int v7;
  unsigned int v8;
  unsigned int v9;
  char v10;
  int *i;
  int v12;
  unsigned int v13;
  u16 v14[8];
  u32 v15[4];

  v2 = *(u32 *)(a1 + 16);
  *(u32 *)a1 = 15;
  if ( v2 != 0 )
  {
    mp4_codec_fn_61(v2, v15);
    v9 = 0;
    if ( v15[0] != 0 )
    {
      for ( i = *(int **)(a1 + 16); ; i = *(int **)(a1 + 16) )
      {
        v12 = mp4_codec_fn_62(i, v9++, v14);
        v13 = v15[0];
        if ( v12 == 0 )
          *(u32 *)a1 += v14[0] + 2;
        if ( v9 >= v13 )
          break;
      }
      v10 = v15[0];
    }
    else
    {
      v10 = 0;
    }
    *(u8 *)(a1 + 15) = v10;
  }
  v3 = *(u32 *)(a1 + 24);
  if ( v3 != 0 )
  {
    mp4_codec_fn_61(v3, v15);
    v4 = 0;
    if ( v15[0] != 0 )
    {
      for ( j = *(int **)(a1 + 24); ; j = *(int **)(a1 + 24) )
      {
        v7 = mp4_codec_fn_62(j, v4++, v14);
        v8 = v15[0];
        if ( v7 == 0 )
          *(u32 *)a1 += v14[0] + 2;
        if ( v4 >= v8 )
          break;
      }
      v5 = v15[0];
    }
    else
    {
      v5 = 0;
    }
    *(u8 *)(a1 + 20) = v5;
  }
}


int mp4_codec_fn_124(u32 *a1)
{
  u32 *v1;
  u32 *v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int result;

  v1 = a1 + 16;
  v3 = a1 + 6;
  v4 = a1[6];
  *a1 = 24;
  if ( v4 != 0 )
  {
    mp4_codec_fn_125(v3);
    result = a1[6];
    *a1 += result;
  }
  if ( a1[11] != 0 )
  {
    result = mp4_codec_fn_126(a1 + 11);
    *a1 += a1[11];
  }
  if ( a1[16] != 0 )
  {
    a1[16] = 40;
    mp4_codec_fn_127(a1 + 26);
    v5 = v1[21];
    a1[16] += v1[10];
    mp4_codec_fn_122(a1 + 34, 0, v5);
    result = a1[44];
    v6 = 20 * result + 16;
    v7 = a1[16] + v1[18] + v6;
    *a1 += v7;
    a1[41] = v6;
    a1[16] = v7;
  }
  return result;
}


void mp4_codec_fn_125(int a1)
{
  unsigned int v1;
  unsigned int v2;
  u16 *v3;
  int v4;

  v1 = *(u16 *)(a1 + 8);
  *(u32 *)a1 = 10;
  v2 = 0;
  if ( v1 != 0 )
  {
    v3 = *(u16 **)(a1 + 16);
    v4 = 10;
    do
    {
      ++v2;
      v4 += *v3;
      v3 += 22;
    }
    while ( v2 < v1 );
    *(u32 *)a1 = v4;
  }
}


int mp4_codec_fn_126(int a1)
{
  int v1;
  unsigned int v2;
  u16 *v3;
  int result;

  v1 = *(u16 *)(a1 + 8);
  *(u32 *)a1 = 10;
  v2 = 0;
  if ( v1 != 0 )
  {
    v3 = *(u16 **)(a1 + 16);
    do
    {
      ++v2;
      if ( v3[5] == 257 )
      {
        result = *((u32 *)v3 + 5);
        *v3 = 12 * result + 22;
        v1 = ((v1) & ~0xFFFFU) | ((u16)(*(u16 *)(a1 + 8)));
      }
      *(u32 *)a1 += *v3;
      v3 += 20;
    }
    while ( v2 < (u16)v1 );
  }
  return result;
}


int mp4_codec_fn_127(u32 *a1)
{
  u32 *v2;
  int v3;
  int result;

  v2 = a1 + 2;
  v3 = a1[2];
  *a1 = 8;
  if ( v3 != 0 )
  {
    mp4_codec_fn_128(v2);
    result = a1[2];
    *a1 += result;
  }
  return result;
}


void mp4_codec_fn_128(u32 *a1)
{
  int v2;
  unsigned int v3;
  unsigned int v4;
  int v5;
  u32 *v6;
  u32 *v7;

  v2 = a1[3];
  *a1 = 16;
  v3 = 0;
  if ( v2 != 0 )
  {
    do
    {
      v4 = v3 + 1;
      v5 = mp4_track_fn_72(v3 + 1, (int)a1, &v7);
      v6 = v7;
      if ( v5 == 0 )
      {
        mp4_codec_fn_129(v7);
        *a1 += *v6;
      }
      v3 = v4;
    }
    while ( v4 < a1[3] );
  }
}


int mp4_codec_fn_129(int a1)
{
  int result;
  int v3;

  result = 1970405376;
  if ( *(u8 *)(a1 + 11) != 0 || *(u32 *)(a1 + 4) != 1970433056 )
  {
    *(u32 *)a1 = 12;
  }
  else
  {
    v3 = *(u32 *)(a1 + 12);
    *(u32 *)a1 = 12;
    if ( v3 != 0 )
    {
      result = mp4_box_fn_12(v3);
      *(u32 *)a1 += result + 1;
    }
  }
  return result;
}


void mp4_codec_fn_130(u32 *a1)
{
  unsigned int v1;
  unsigned int v2;
  u32 *v3;
  unsigned int v4;
  unsigned int v5;
  u32 *v6;
  unsigned int v7;
  unsigned int v8;
  u32 *v9;

  if ( a1 != NULL && *a1 != 0 )
  {
    v1 = a1[7];
    *a1 = 32;
    v2 = 0;
    if ( v1 != 0 )
    {
      v3 = (u32 *)a1[10];
      do
      {
        ++v2;
        *a1 += *v3;
        v3 += 5;
      }
      while ( v2 < v1 );
    }
    v4 = a1[8];
    v5 = 0;
    if ( v4 != 0 )
    {
      v6 = (u32 *)a1[11];
      do
      {
        ++v5;
        *a1 += *v6;
        v6 += 11;
      }
      while ( v5 < v4 );
    }
    v7 = a1[9];
    v8 = 0;
    if ( v7 != 0 )
    {
      v9 = (u32 *)a1[12];
      do
      {
        ++v8;
        *a1 += *v9;
        v9 += 13;
      }
      while ( v8 < v7 );
    }
  }
}


int mp4_codec_fn_131(int a1, u32 *a2, unsigned int a3, int *a4)
{
  int i;
  int v9;
  int v10;
  u32 v12[4];

  for ( i = 0; ; ++i )
  {
    v12[0] = 0;
    v9 = mp4_codec_fn_91(a1, i, a3, v12);
    v10 = 20481;
    if ( v12[0] >= *a2 )
      break;
    if ( v9 != 0 )
      return v10;
    *a2 -= v12[0];
  }
  *a4 = i;
  return 0;
}


int mp4_codec_fn_132(int a1)
{
  int v2;

  v2 = *(u32 *)(a1 + 8);
  if ( v2 != 0 )
    mp4_track_fn_65(v2);
  return mp4_track_fn_65(a1);
}


int mp4_codec_fn_133(u32 *a1, int a2, int a3)
{
  u32 *v3;
  int v4;
  int v6;
  u32 *v7;
  int *v8;
  u32 *v9;
  u32 *v10;
  u32 *v11;
  u32 *v12;
  u32 *v13;
  u32 *v14;
  u32 *v15;
  u32 *v16;
  u32 *v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v26;
  int v27;
  unsigned int v28;
  int (*v29)(int, int *);
  int v30; // dc
  u32 *v31;
  u32 *v32;
  u32 *v33;
  u32 *v34;
  u32 *v35;
  u32 *v36;
  u32 *v37;
  u32 *v38;

  v3 = a1 + 76;
  v4 = a1[96];
  if ( v4 != 0 )
  {
    do
    {
      v6 = *(u32 *)(v4 + 20);
      if ( *(u32 *)(v4 + 12) != 0 )
        mp4_track_fn_65(*(u32 *)(v4 + 12));
      mp4_track_fn_65(v4);
      v4 = v6;
    }
    while ( v6 != 0 );
  }
  v7 = (u32 *)v3[94];
  if ( v7 == v3 + 22 )
  {
    v8 = (int *)v3[93];
  }
  else if ( v7 != NULL )
  {
    mp4_api_fn_65(v3[94], a2, a3);
    v28 = v7[9];
    if ( v28 != 0 )
    {
      v29 = mp4_box_fn_1;
      do
      {
        v26 = (unsigned int)v29 < v28;
        v30 = (unsigned int)v29 < v28;
        v29 = (int (*)(int, int *))((char *)v29 + 1);
      }
      while ( v30 );
      v7[10] = 0;
    }
    v7[9] = 0;
    mp4_api_fn_65((int)v7, v26, v27);
    v31 = (u32 *)v7[10];
    if ( v31 != NULL )
    {
      do
      {
        v33 = (u32 *)v31[8];
        mp4_codec_fn_103(v31);
        v31 = v33;
      }
      while ( v33 != NULL );
      v32 = (u32 *)v7[18];
    }
    else
    {
      v32 = (u32 *)v7[18];
    }
    if ( v32 != NULL )
      mp4_codec_fn_102(v32);
    v34 = (u32 *)v7[27];
    if ( v34 != NULL )
      mp4_codec_fn_102(v34);
    v35 = (u32 *)v7[36];
    if ( v35 != NULL )
      mp4_codec_fn_102(v35);
    v36 = (u32 *)v7[45];
    if ( v36 != NULL )
      mp4_codec_fn_102(v36);
    v37 = (u32 *)v7[54];
    if ( v37 != NULL )
      mp4_codec_fn_102(v37);
    v38 = (u32 *)v7[63];
    if ( v38 != NULL )
      mp4_codec_fn_102(v38);
    mp4_track_fn_65((int)v7);
    v8 = (int *)v3[93];
  }
  else
  {
    v8 = (int *)v3[93];
  }
  v3[94] = 0;
  if ( v8 != NULL )
    mp4_api_fn_60(v8);
  mp4_api_fn_65((int)(v3 + 22), a2, a3);
  v9 = (u32 *)v3[32];
  if ( v9 != NULL )
  {
    do
    {
      v11 = (u32 *)v9[8];
      mp4_codec_fn_103(v9);
      v9 = v11;
    }
    while ( v11 != NULL );
    v10 = (u32 *)v3[40];
  }
  else
  {
    v10 = (u32 *)v3[40];
  }
  if ( v10 != NULL )
    mp4_codec_fn_102(v10);
  v12 = (u32 *)v3[49];
  if ( v12 != NULL )
    mp4_codec_fn_102(v12);
  v13 = (u32 *)v3[58];
  if ( v13 != NULL )
    mp4_codec_fn_102(v13);
  v14 = (u32 *)v3[67];
  if ( v14 != NULL )
    mp4_codec_fn_102(v14);
  v15 = (u32 *)v3[76];
  if ( v15 != NULL )
    mp4_codec_fn_102(v15);
  v16 = (u32 *)v3[85];
  if ( v16 != NULL )
    mp4_codec_fn_102(v16);
  v17 = (u32 *)a1[37];
  if ( v17 != NULL )
    mp4_codec_fn_102(v17);
  v18 = a1[174];
  if ( v18 != 0 )
  {
    v20 = *(u32 *)(v18 + 8);
    if ( v20 != 0 )
    {
      v22 = *(u32 *)(v20 + 12);
      if ( v22 != 0 )
        mp4_track_fn_65(v22);
      mp4_track_fn_65(v20);
      v21 = *(u32 *)(v18 + 12);
    }
    else
    {
      v21 = *(u32 *)(v18 + 12);
    }
    if ( v21 != 0 )
    {
      v23 = *(u32 *)(v21 + 12);
      if ( v23 != 0 )
        mp4_track_fn_65(v23);
      mp4_track_fn_65(v21);
    }
    mp4_track_fn_65(v18);
    v19 = a1[172];
  }
  else
  {
    v19 = a1[172];
  }
  if ( v19 != 0 )
    mp4_codec_fn_108(v19);
  v24 = a1[173];
  if ( v24 != 0 )
    mp4_codec_fn_109(v24);
  return mp4_track_fn_65((int)a1);
}


int mp4_codec_fn_134(int a1)
{
  int v1;
  int v3;

  v1 = *(u32 *)(a1 + 8);
  if ( v1 != 0 )
  {
    if ( *(u32 *)(v1 + 12) != 0 )
      mp4_track_fn_65(*(u32 *)(v1 + 12));
    mp4_track_fn_65(v1);
  }
  v3 = *(u32 *)(a1 + 12);
  if ( v3 != 0 )
  {
    if ( *(u32 *)(v3 + 12) != 0 )
      mp4_track_fn_65(*(u32 *)(v3 + 12));
    mp4_track_fn_65(v3);
  }
  return mp4_track_fn_65(a1);
}


int mp4_codec_fn_135(int a1)
{
  int v2;

  v2 = *(u32 *)(a1 + 12);
  if ( v2 != 0 )
    mp4_track_fn_65(v2);
  return mp4_track_fn_65(a1);
}


int mp4_codec_fn_136(u32 *a1, int a2, int a3)
{
  int result;
  u32 *v5;
  u32 *v6;
  u32 *v7;
  u32 *v8;
  u32 *v9;
  u32 *v10;
  u32 *v11;
  u32 *v12;

  result = mp4_api_fn_65((int)a1, a2, a3);
  v5 = (u32 *)a1[10];
  if ( v5 != NULL )
  {
    do
    {
      v7 = (u32 *)v5[8];
      result = mp4_codec_fn_103(v5);
      v5 = v7;
    }
    while ( v7 != NULL );
    v6 = (u32 *)a1[18];
  }
  else
  {
    v6 = (u32 *)a1[18];
  }
  if ( v6 != NULL )
    result = mp4_codec_fn_102(v6);
  v8 = (u32 *)a1[27];
  if ( v8 != NULL )
    result = mp4_codec_fn_102(v8);
  v9 = (u32 *)a1[36];
  if ( v9 != NULL )
    result = mp4_codec_fn_102(v9);
  v10 = (u32 *)a1[45];
  if ( v10 != NULL )
    result = mp4_codec_fn_102(v10);
  v11 = (u32 *)a1[54];
  if ( v11 != NULL )
    result = mp4_codec_fn_102(v11);
  v12 = (u32 *)a1[63];
  if ( v12 != NULL )
    return mp4_codec_fn_102(v12);
  return result;
}


int mp4_codec_fn_137(int a1, int a2, int a3, int a4)
{
  int v5;
  int v6;
  u32 v8[4];

  v8[0] = a3;
  v8[1] = a4;
  v5 = mp4_codec_fn_94((u32 **)(a1 + 20), 1u, v8, 100);
  v6 = 1;
  if ( v5 == 0 )
  {
    v6 = 0;
    ++*(u32 *)(a1 + 16);
  }
  return v6;
}


int mp4_codec_fn_138(int a1, int a2)
{
  int v3;
  int v4;

  v3 = mp4_codec_fn_89((u32 **)(a1 + 20), 3u, a2, 100);
  v4 = 1;
  if ( v3 == 0 )
  {
    v4 = 0;
    ++*(u32 *)(a1 + 16);
  }
  return v4;
}


int mp4_codec_fn_139(int a1, int a2)
{
  int v3;
  int v4;

  v3 = mp4_codec_fn_89((u32 **)(a1 + 20), 2u, a2, 100);
  v4 = 1;
  if ( v3 == 0 )
  {
    v4 = 0;
    ++*(u32 *)(a1 + 16);
  }
  return v4;
}


int mp4_codec_fn_140(int a1, int a2)
{
  int v3;
  int v4;

  v3 = mp4_codec_fn_89((u32 **)(a1 + 20), 2u, a2, 100);
  v4 = 1;
  if ( v3 == 0 )
  {
    v4 = 0;
    ++*(u32 *)(a1 + 16);
  }
  return v4;
}


int mp4_codec_fn_141(int a1, int a2)
{
  int v3;
  int v4;
  u32 v6[4];

  v6[0] = a2;
  v3 = mp4_codec_fn_89((u32 **)(a1 + 20), 1u, (int)v6, 100);
  v4 = 1;
  if ( v3 == 0 )
  {
    v4 = 0;
    ++*(u32 *)(a1 + 16);
  }
  return v4;
}


int mp4_codec_fn_142(int a1)
{
  return *(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(a1 + 276) + 700) + 152) + 276) + 4) + 84);
}


