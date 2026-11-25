#include "mp4msv_internal.h"

int mp4_track_fn_1(int a1, unsigned int a2, unsigned int a3, int a4, int a5, int a6, u32 *a7, u32 *a8)
{
  int v13;
  unsigned int v14;
  int i;
  int v17;
  unsigned int v18;
  int v19;
  int v20;
  int j;
  int v22;
  int v23;
  int v24;
  int v25;
  unsigned int v26;
  int v27;
  int v28;
  unsigned int v29;
  int v30;
  unsigned int v31;
  unsigned int v32;

  v13 = mp4_codec_fn_61(*(u32 *)(a1 + 12), &v29);
  if ( v13 == 0 )
  {
    v14 = 0;
    if ( v29 != 0 )
    {
      for ( i = *(u32 *)(a1 + 12); ; i = *(u32 *)(a1 + 12) )
      {
        v17 = mp4_codec_fn_62(i, v14++, &v30);
        v13 = v17;
        if ( v17 != 0 )
          break;
        v13 = mp4_track_fn_9(v30, &v31);
        if ( v13 != 0 )
          break;
        if ( a2 == v31 )
        {
          v13 = mp4_track_fn_10(v30, &v32);
          if ( v13 != 0 )
            return v13;
          if ( v32 == 0 )
          {
            v13 = 0;
            goto LABEL_4;
          }
          v13 = mp4_track_fn_11(v30, 1, &v25);
          if ( v13 == 0 )
          {
            if ( v26 >= a3 )
            {
              v13 = 0;
              goto LABEL_4;
            }
            v18 = 2;
            if ( v32 >= 2 )
            {
              for ( j = v30; ; j = v30 )
              {
                v22 = v27;
                v23 = v28;
                v24 = mp4_track_fn_11(j, v18++, &v25);
                v13 = v24;
                if ( v24 != 0 )
                  return v13;
                if ( v26 == a3 && v25 == 0 )
                  break;
                if ( a3 < v26 )
                {
                  *a7 = 1;
LABEL_30:
                  *a8 = v22;
                  a8[1] = v23;
                  return 0;
                }
                if ( v32 < v18 )
                  goto LABEL_21;
              }
              *a7 = 1;
              goto LABEL_30;
            }
LABEL_21:
            v19 = v27;
            v20 = v28;
            *a7 = 1;
            *a8 = v19;
            a8[1] = v20;
            return 0;
          }
          return v13;
        }
        if ( a2 < v31 )
          goto LABEL_3;
        if ( v14 >= v29 )
        {
          v13 = 0;
          goto LABEL_4;
        }
      }
    }
    else
    {
LABEL_3:
      v13 = 0;
LABEL_4:
      *a7 = 0;
    }
  }
  return v13;
}


int mp4_track_fn_2(int a1, unsigned int a2, u32 *a3, u32 *a4)
{
  int result;
  unsigned int v9;
  int i;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  u32 v19[8];
  unsigned int v20;
  int v21;
  unsigned int v22;
  int v23;

  result = mp4_codec_fn_61(*(u32 *)(a1 + 12), &v20);
  if ( result == 0 )
  {
    v9 = 0;
    if ( v20 != 0 )
    {
      for ( i = *(u32 *)(a1 + 12); ; i = *(u32 *)(a1 + 12) )
      {
        result = mp4_codec_fn_62(i, v9++, &v21);
        if ( result != 0 )
          break;
        result = mp4_track_fn_9(v21, &v22);
        if ( result != 0 )
          break;
        if ( a2 == v22 )
        {
          result = mp4_track_fn_10(v21, &v23);
          if ( result != 0 )
            return result;
          result = 0;
          if ( v23 != 0 )
          {
            result = mp4_track_fn_11(v21, v23, v19);
            if ( result == 0 )
            {
              v11 = v19[0];
              v12 = v19[1];
              v13 = v19[2];
              v14 = v19[3];
              v15 = v19[4];
              v16 = v19[5];
              v17 = v19[6];
              v18 = v19[7];
              *a3 = 1;
              a4[7] = v18;
              *a4 = v11;
              a4[1] = v12;
              a4[2] = v13;
              a4[3] = v14;
              a4[4] = v15;
              a4[5] = v16;
              a4[6] = v17;
              return 0;
            }
            return result;
          }
          goto LABEL_4;
        }
        if ( a2 < v22 )
          goto LABEL_3;
        if ( v9 >= v20 )
        {
          result = 0;
          goto LABEL_4;
        }
      }
    }
    else
    {
LABEL_3:
      result = 0;
LABEL_4:
      *a3 = 0;
    }
  }
  return result;
}


int mp4_track_fn_3(int a1, int a2)
{
  int v4;
  unsigned int v5;
  int v6;
  int v7;
  int i;
  unsigned int v10;
  int v11;
  int v12;
  int v13;

  v4 = mp4_codec_fn_61(*(u32 *)(a1 + 16), &v10);
  if ( v4 == 0 )
  {
    v5 = v10;
    v6 = 0;
    if ( v10 != 0 )
    {
      for ( i = *(u32 *)(a1 + 16); ; i = *(u32 *)(a1 + 16) )
      {
        v4 = mp4_codec_fn_62(i, v6, &v11);
        if ( v4 != 0 )
          break;
        v4 = mp4_track_fn_21(v11, &v12);
        if ( v4 != 0 )
          break;
        v5 = v10;
        if ( a2 == v12 )
          goto LABEL_3;
        if ( ++v6 >= v10 )
          goto LABEL_3;
      }
    }
    else
    {
LABEL_3:
      v4 = 0;
      if ( v6 != v5 )
      {
        v4 = mp4_track_fn_22(v11, &v13);
        if ( v4 == 0 )
        {
          if ( v13 == 0 )
            *(u32 *)(a1 + 24) = 1;
          mp4_track_fn_20(v11);
          v7 = mp4_codec_fn_45(*(u32 *)(a1 + 16), v6);
          v4 = 0;
          if ( v7 != 0 )
            return v7;
        }
      }
    }
  }
  return v4;
}


int mp4_track_fn_4(int a1, int a2, unsigned int a3, int *a4)
{
  s64 v8;
  unsigned int i;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  unsigned int v16;
  int v17;
  int j;
  int v20;
  int v21;
  int v22;
  s64 v23;
  int v24;
  unsigned int v25;
  unsigned int v26;
  int v27;
  unsigned int v28;
  int v29;
  s64 v30;

  *a4 = 0;
  v8 = ((u64)(v8) & 0xFFFFFFFF00000000ULL) | (u32)(mp4_box_fn_2(a2));
  if ( (u32)v8 == -1 )
  {
    v11 = 2;
    if ( HIDWORD(v8) == -1 )
      return v11;
  }
  v30 = v8 + a3;
  for ( i = a3; i >= 8; i -= v25 )
  {
    v10 = mp4_box_fn_6((int)&v25, 4, a2);
    v11 = 2;
    if ( v10 != 4 )
      return v11;
    v11 = 4;
    if ( v25 < 8 )
      goto LABEL_29;
    v12 = mp4_box_fn_6((int)&v26, 4, a2);
    v11 = 2;
    if ( v12 != 4 )
      return v11;
    v13 = v25;
    if ( i < v25 )
    {
LABEL_29:
      v23 = v30;
      *a4 |= 1u;
      v24 = mp4_box_fn_24(a2, v11, v23, SHIDWORD(v23));
      v11 = 8195;
      if ( v24 == 0 )
        return 2;
      return v11;
    }
    if ( v26 == 1835430511 )
    {
      v21 = mp4_track_fn_6(a1, a2, v25 - 8, &v27);
      v22 = *a4;
    }
    else
    {
      if ( v26 <= 0x6D66726F )
      {
        v14 = *a4;
        goto LABEL_13;
      }
      if ( v26 != 1952871009 )
      {
        v14 = *a4;
LABEL_13:
        *a4 = v14 | 4;
        v15 = mp4_box_fn_3(a2, 2, v13, 0, 1);
        v11 = 2;
        if ( v15 == 0 )
          return v11;
        continue;
      }
      v21 = mp4_track_fn_5(a1, a2, v25 - 8, &v27);
      v22 = *a4;
    }
    v11 = v21;
    *a4 = v22 | v27;
    if ( v21 != 0 )
      return v11;
  }
  v11 = mp4_codec_fn_61(*(u32 *)(a1 + 12), &v28);
  if ( v11 == 0 )
  {
    v16 = 0;
    if ( v28 != 0 )
    {
      for ( j = *(u32 *)(a1 + 12); ; j = *(u32 *)(a1 + 12) )
      {
        v20 = mp4_codec_fn_62(j, v16++, &v29);
        v11 = v20;
        if ( v20 != 0 )
          break;
        v11 = mp4_track_fn_13(v29);
        if ( v11 != 0 )
          break;
        if ( v16 >= v28 )
        {
          v17 = *a4;
          goto LABEL_18;
        }
      }
    }
    else
    {
      v17 = *a4;
LABEL_18:
      v11 = 0;
      *a4 = v17 | 0x80000;
    }
  }
  return v11;
}


int mp4_track_fn_5(int a1, int a2, int a3, u32 *a4)
{
  int v8;
  int v9;
  int v10;
  int v11;
  unsigned int v12;
  int v13;
  int v14;
  unsigned int v15;
  int v16;
  int v17;
  unsigned int v18;
  int i;
  unsigned int v20;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  unsigned int v29;
  unsigned int v30;
  int v31;
  u32 v32[4];

  *a4 = 0;
  v8 = mp4_box_fn_2(a2);
  v10 = v9;
  if ( v8 != -1 || (v16 = 2, v10 != 0) )
  {
    v11 = v8 + a3;
    v14 = v10 + __CFADD__(v8, a3);
    v12 = mp4_track_fn_7(&v28);
    v15 = v12;
    v16 = v12;
    if ( v12 < 2 )
    {
      if ( v12 == 1 )
        goto LABEL_19;
      v17 = mp4_track_fn_14(v28, a2, a3, a4);
      if ( v17 != 0 || (v17 = mp4_track_fn_9(v28, &v29)) != 0 || (v17 = mp4_codec_fn_61(*(u32 *)(a1 + 12), &v30)) != 0 )
      {
LABEL_25:
        mp4_track_fn_8(v28);
        return v17;
      }
      v18 = 0;
      if ( v30 != 0 )
      {
        for ( i = *(u32 *)(a1 + 12); ; i = *(u32 *)(a1 + 12) )
        {
          v17 = mp4_codec_fn_62(i, v18, &v31);
          if ( v17 != 0 )
            break;
          v17 = mp4_track_fn_9(v31, v32);
          if ( v17 != 0 )
            break;
          if ( v29 < v32[0] )
          {
            v25 = mp4_codec_fn_44(*(u32 *)(a1 + 12), v18, &v28);
            v17 = v25;
            if ( v25 == 1 )
            {
              v27 = mp4_box_fn_24(a2, v26, v11, v14);
              v16 = 2;
              if ( v27 == 0 )
                return v16;
            }
            v16 = 0;
            if ( v17 == 0 )
              return v16;
            goto LABEL_25;
          }
          ++v18;
          if ( v29 == v32[0] )
          {
            v17 = mp4_track_fn_12(v31, v28);
            if ( v17 != 1 || (v24 = mp4_box_fn_24(a2, v23, v11, v14), v16 = 2, v24 != 0) )
            {
              if ( v17 != 0 )
                goto LABEL_25;
              mp4_track_fn_8(v28);
              return 0;
            }
            return v16;
          }
          if ( v18 >= v30 )
            goto LABEL_16;
        }
        goto LABEL_25;
      }
LABEL_16:
      v20 = mp4_codec_fn_43(*(u32 *)(a1 + 12), &v28);
      v15 = v20;
      v16 = v20;
      if ( v20 < 2 )
      {
        v16 = 0;
        if ( v20 == 1 )
        {
LABEL_19:
          v22 = mp4_box_fn_24(a2, v13, v11, v14);
          v16 = 2;
          if ( v22 != 0 )
            return v15;
        }
      }
    }
  }
  return v16;
}


int mp4_track_fn_6(int a1, int a2, unsigned int a3, int *a4)
{
  s64 v8;
  s64 v9;
  s64 v10;
  int v11;
  int v12;
  int v13;
  u32 v15[4];

  *a4 = 0;
  v8 = ((u64)(v8) & 0xFFFFFFFF00000000ULL) | (u32)(mp4_box_fn_2(a2));
  v9 = v8;
  if ( (u32)v8 != -1 || (v11 = 2, HIDWORD(v9) != 0) )
  {
    v10 = v9 + a3;
    v11 = mp4_track_fn_18(*(u32 *)(a1 + 8), v15);
    if ( v11 == 0 )
    {
      if ( v15[0] != 0 )
      {
        v12 = *a4 | 8;
        *a4 = v12;
        v13 = mp4_box_fn_24(a2, v12, v10, SHIDWORD(v10));
        v11 = 0;
        if ( v13 != 0 )
          return 0;
      }
      else
      {
        v11 = mp4_track_fn_19(*(u32 *)(a1 + 8), a2, a3, a4);
        if ( v11 == 0 )
          return 0;
      }
    }
  }
  return v11;
}


int mp4_track_fn_7(int *a1)
{
  int v2;
  int v3;
  int v4;
  int v5;
  u32 v7[4];

  v2 = mp4_box_fn_7(44);
  v3 = 1;
  if ( v2 != 0 )
  {
    *(u32 *)(v2 + 4) = 1952871009;
    *(u32 *)v2 = 24;
    *(u16 *)(v2 + 8) = 1;
    *(u8 *)(v2 + 10) = 0;
    *(u8 *)(v2 + 11) = 0;
    *(u32 *)(v2 + 12) = 0;
    *(u8 *)(v2 + 16) = 0;
    *(u8 *)(v2 + 17) = 0;
    *(u8 *)(v2 + 18) = 0;
    *(u32 *)(v2 + 20) = 0;
    v4 = mp4_codec_fn_59(32, 100, v7);
    if ( v4 != 0 )
    {
      mp4_box_fn_9();
      return v4;
    }
    else
    {
      v5 = v7[0];
      *a1 = v2;
      v3 = 0;
      *(u32 *)(v2 + 28) = 0;
      *(u32 *)(v2 + 32) = 0;
      *(u32 *)(v2 + 36) = 0;
      *(u32 *)(v2 + 40) = 0;
      *(u32 *)(v2 + 24) = v5;
    }
  }
  return v3;
}


int mp4_track_fn_8(int a1)
{
  int v2;
  int result;

  v2 = *(u32 *)(a1 + 24);
  if ( v2 != 0 )
    { mp4_codec_fn_60(v2); result = 0; }
  if ( a1 != 0 )
    return mp4_box_fn_9();
  return result;
}


int mp4_track_fn_9(int a1, u32 *a2)
{
  *a2 = *(u32 *)(a1 + 12);
  return 0;
}


int mp4_track_fn_10(int a1, int a2)
{
  return mp4_codec_fn_61(*(u32 *)(a1 + 24), a2);
}


int mp4_track_fn_11(int a1, int a2, int a3)
{
  return mp4_codec_fn_62(*(u32 *)(a1 + 24), a2 - 1, a3);
}


int mp4_track_fn_12(int a1, int a2)
{
  int v4;
  unsigned int v5;
  u8 v6;
  unsigned int v7;
  int i;
  int v10;
  u8 v11[32];
  u32 v12[4];

  v4 = mp4_codec_fn_61(*(u32 *)(a2 + 24), v12);
  if ( v4 == 0 )
  {
    v5 = 0;
    if ( v12[0] != 0 )
    {
      for ( i = *(u32 *)(a2 + 24); ; i = *(u32 *)(a2 + 24) )
      {
        v10 = mp4_codec_fn_62(i, v5++, v11);
        v4 = v10;
        if ( v10 != 0 )
          break;
        v4 = mp4_codec_fn_43(*(u32 *)(a1 + 24), v11);
        if ( v4 != 0 )
          break;
        if ( v5 >= v12[0] )
        {
          v6 = *(u8 *)(a2 + 16);
          goto LABEL_4;
        }
      }
    }
    else
    {
      v6 = *(u8 *)(a2 + 16);
LABEL_4:
      v7 = *(u8 *)(a1 + 16);
      *(u32 *)(a1 + 20) += *(u32 *)(a2 + 20);
      if ( v7 < v6 )
        *(u8 *)(a1 + 16) = v6;
      if ( *(u8 *)(a1 + 17) < (unsigned int)*(u8 *)(a2 + 17) )
        *(u8 *)(a1 + 17) = *(u8 *)(a2 + 17);
      if ( *(u8 *)(a1 + 18) < (unsigned int)*(u8 *)(a2 + 18) )
        *(u8 *)(a1 + 18) = *(u8 *)(a2 + 18);
      return 0;
    }
  }
  return v4;
}


int mp4_track_fn_13(int a1)
{
  int v2;
  unsigned int v3;
  int i;
  int v6;
  int v7;
  u32 v8[4];

  v2 = mp4_codec_fn_61(*(u32 *)(a1 + 24), v8);
  if ( v2 == 0 )
  {
    v3 = 0;
    if ( v8[0] != 0 )
    {
      for ( i = *(u32 *)(a1 + 24); ; i = *(u32 *)(a1 + 24) )
      {
        v6 = mp4_codec_fn_62(i, v3++, &v7);
        v2 = v6;
        if ( v6 != 0 )
          break;
        if ( v3 >= v8[0] )
          return 0;
      }
    }
    else
    {
      return 0;
    }
  }
  return v2;
}


int mp4_track_fn_14(int a1, int a2, unsigned int a3, int *a4)
{
  int v4;
  unsigned int v6;
  int v8;
  int v9;
  int v11;
  int v12;
  u8 v13;
  u8 v14;
  unsigned int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  unsigned int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  unsigned int v28;
  int v29;
  unsigned int v30;
  int v31;
  int v32;
  int v33;
  int v34;
  u8 v35;
  u8 v36;
  u8 v37;
  u8 v38;
  u8 v39;
  u8 v40;
  u8 v41;
  u8 v42;
  u8 v43;
  u8 v44;
  u8 v45;
  u8 v46;
  u8 v47;
  u8 v48;
  u8 v49;
  u8 v50;
  char v51[16];
  u32 v52[4];
  char v53[4];
  char v54[4];
  char v55[8];
  unsigned int v56;

  v4 = a2;
  v6 = a3;
  *a4 = 0;
  if ( a3 < 4 )
  {
    *a4 = 0x20000000;
    if ( a3 == 0 )
      return 8195;
    goto LABEL_3;
  }
  v11 = mp4_box_fn_5((int)&v35, 4, a2);
  v9 = 2;
  if ( v11 != 4 )
    return v9;
  v12 = v35;
  v13 = v37;
  v14 = v38;
  a2 = 1;
  *(u8 *)(a1 + 9) = v36;
  v15 = v6 - 4;
  v9 = 8195;
  *(u8 *)(a1 + 10) = v13;
  *(u8 *)(a1 + 11) = v14;
  *(u8 *)(a1 + 8) = v12;
  if ( v12 != 1 )
    return v9;
  if ( (*(u32 *)(a1 + 8) & 0xFFFFFF00) != 0 )
  {
    v16 = v15;
    *a4 |= 4u;
    v17 = v4;
    if ( v15 == 0 )
      return 0;
LABEL_11:
    v18 = mp4_box_fn_3(v17, a2, v16, 0, 1);
    v9 = 2;
    if ( v18 != 0 )
      return v9;
    return 0;
  }
  if ( v15 >= 0xC )
  {
    v20 = mp4_box_fn_6(a1 + 12, 4, v4);
    v9 = 2;
    if ( v20 != 4 )
      return v9;
    v21 = mp4_box_fn_5((int)&v35, 4, v4);
    v22 = v15 - 4;
    v9 = 2;
    if ( v21 != 4 )
      return v9;
    v23 = (v38 >> 4) & 3;
    v24 = (v38 >> 2) & 3;
    v25 = v38 & 3;
    *(u8 *)(a1 + 16) = v23;
    *(u8 *)(a1 + 17) = v24;
    v26 = v23 + v24 + v25 + 19;
    *(u8 *)(a1 + 18) = v25;
    v28 = v22 - 4;
    v27 = mp4_box_fn_6(a1 + 20, 4, v4);
    v9 = 2;
    if ( v27 != 4 )
      return v9;
    v29 = *(u32 *)(a1 + 20);
    v15 = v28 - 4;
    a2 = v26 * v29;
    if ( v15 >= v26 * v29 )
    {
      v56 = 0;
      if ( v29 != 0 )
      {
        v30 = v56;
        do
        {
          v56 = v30 + 1;
          v31 = mp4_box_fn_5((int)&v35, v26, v4);
          v9 = 2;
          if ( v31 != v26 )
            return v9;
          v32 = *(u8 *)(a1 + 16);
          v52[0] = (v35 << 24) + (v36 << 16) + (v37 << 8) + v38;
          v52[1] = (v39 << 24) + (v40 << 16) + (v41 << 8) + v42;
          v52[2] = (v47 << 24) + (v48 << 16) + (v49 << 8) + v50;
          v52[3] = (v43 << 24) + (v44 << 16) + (v45 << 8) + v46;
          v15 -= v26;
          v33 = mp4_track_fn_15(a1, v32, v51, v53);
          v34 = mp4_track_fn_15(a1, *(u8 *)(a1 + 17), v33, v54);
          mp4_track_fn_15(a1, *(u8 *)(a1 + 18), v34, v55);
          v9 = mp4_codec_fn_43(*(u32 *)(a1 + 24), v52);
          if ( v9 != 0 )
            return v9;
          v30 = v56;
        }
        while ( v56 < *(u32 *)(a1 + 20) );
      }
      v9 = 0;
      if ( v15 == 0 )
        return v9;
      v17 = v4;
      v16 = v15;
      *(u32 *)a1 -= v15;
      goto LABEL_11;
    }
    v19 = *a4 | 2;
  }
  else
  {
    v19 = *a4 | 0x20000000;
  }
  *a4 = v19;
  if ( v15 == 0 )
    return 8195;
  a3 = v15;
LABEL_3:
  v8 = mp4_box_fn_3(v4, a2, a3, 0, 1);
  v9 = 2;
  if ( v8 == 0 )
    return 8195;
  return v9;
}


u8 *mp4_track_fn_15(int a1, unsigned int a2, u8 *a3, int *a4)
{
  int v4;
  int v6;
  int v7;

  if ( a2 == 1 )
  {
    v4 = *a3;
    v6 = a3[1];
    a3 += 2;
    v7 = (v4 << 8) + v6;
LABEL_9:
    *a4 = v7;
    return a3;
  }
  if ( a2 < 2u )
  {
    v7 = *a3++;
    goto LABEL_9;
  }
  if ( a2 == 2 )
  {
    v7 = (*a3 << 16) + (a3[1] << 8) + a3[2];
    a3 += 3;
    goto LABEL_9;
  }
  if ( a2 == 3 )
  {
    *a4 = (*a3 << 24) + (a3[1] << 16) + (a3[2] << 8) + a3[3];
    a3 += 4;
  }
  return a3;
}


int mp4_track_fn_16(int *a1)
{
  int v2;
  int result;

  v2 = mp4_box_fn_7(16);
  result = 1;
  if ( v2 != 0 )
  {
    *(u8 *)(v2 + 9) = 0;
    *(u8 *)(v2 + 10) = 0;
    *(u8 *)(v2 + 11) = 0;
    *(u32 *)v2 = 16;
    strcpy((char *)(v2 + 4), "orfm");
    *a1 = v2;
    *(u32 *)(v2 + 12) = 0;
    return 0;
  }
  return result;
}


int mp4_track_fn_17(int a1)
{
  int result;

  if ( a1 != 0 )
    return mp4_box_fn_9();
  return result;
}


int mp4_track_fn_18(int a1, u32 *a2)
{
  *a2 = *(u32 *)(a1 + 12);
  return 0;
}


int mp4_track_fn_19(int a1, int a2, unsigned int a3, u32 *a4)
{
  unsigned int v4;
  int v5;
  int v8;
  int v9;
  int v11;
  char v12;
  char v13;
  char v14;
  int v15;
  int v16;
  int v17;
  int v18;
  u8 v19[16];

  v4 = a3;
  v5 = a2;
  *a4 = 0;
  if ( a3 < 4 )
  {
    *a4 = 0x20000000;
    if ( a3 == 0 )
      return 8195;
    goto LABEL_3;
  }
  v11 = mp4_box_fn_5((int)v19, 4, a2);
  v9 = 2;
  if ( v11 != 4 )
    return v9;
  v12 = v19[2];
  v13 = v19[3];
  v14 = v19[0];
  *(u8 *)(a1 + 9) = v19[1];
  *(u8 *)(a1 + 8) = v14;
  *(u8 *)(a1 + 10) = v12;
  *(u8 *)(a1 + 11) = v13;
  a2 = *(u32 *)(a1 + 8);
  if ( a2 != 0 )
  {
    v15 = v4 - 4;
    *a4 |= 4u;
    v16 = v5;
    if ( v4 == 4 )
      return 0;
LABEL_10:
    v17 = mp4_box_fn_3(v16, a2, v15, 0, 1);
    v9 = 2;
    if ( v17 != 0 )
      return v9;
    return 0;
  }
  if ( v4 - 4 >= 4 )
  {
    v18 = mp4_box_fn_6(a1 + 12, 4, v5);
    v9 = 2;
    if ( v18 != 4 )
      return v9;
    v9 = 0;
    if ( v4 == 8 )
      return v9;
    v15 = v4 - 8;
    *(u32 *)a1 -= v4 - 8;
    v16 = v5;
    goto LABEL_10;
  }
  *a4 |= 0x20000000u;
  if ( v4 == 4 )
    return 8195;
  a3 = v4 - 4;
LABEL_3:
  v8 = mp4_box_fn_3(v5, a2, a3, 0, 1);
  v9 = 2;
  if ( v8 == 0 )
    return 8195;
  return v9;
}


int mp4_track_fn_20(int a1)
{
  int v2;
  int result;

  v2 = *(u32 *)(a1 + 4);
  if ( v2 != 0 )
    { mp4_codec_fn_60(v2); result = 0; }
  if ( a1 != 0 )
    return mp4_box_fn_9();
  return result;
}


int mp4_track_fn_21(u32 *a1, u32 *a2)
{
  *a2 = *a1;
  return 0;
}


int mp4_track_fn_22(int a1, u32 *a2)
{
  int v4;
  unsigned int v5;
  int i;
  int v8;
  u32 v9[8];
  u32 v10[4];

  v4 = mp4_codec_fn_61(*(u32 *)(a1 + 4), v10);
  if ( v4 == 0 )
  {
    v5 = 0;
    if ( v10[0] != 0 )
    {
      for ( i = *(u32 *)(a1 + 4); ; i = *(u32 *)(a1 + 4) )
      {
        v8 = mp4_codec_fn_62(i, v5++, v9);
        v4 = v8;
        if ( v8 != 0 )
          return v4;
        if ( v9[3] == 0 )
          break;
        if ( v9[4] == 0 )
        {
          v4 = 0;
          goto LABEL_14;
        }
        if ( v5 >= v10[0] )
          goto LABEL_3;
      }
      v4 = 0;
LABEL_14:
      *a2 = 0;
      return v4;
    }
LABEL_3:
    *a2 = 1;
    return 0;
  }
  return v4;
}


int mp4_track_fn_23(int a1, int a2, int a3)
{
  int v3;
  int v7;
  int v8;
  u32 *v9;
  int v10;
  int v11;
  unsigned int v12;
  unsigned int v13;
  unsigned int v14;
  u32 *v15;
  int v16;
  unsigned int v17;
  u32 *v19;
  int v20;
  int v21;
  unsigned int *v22;
  int v23;
  u32 *v24;
  int v25;
  unsigned int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  u32 *v31;
  int v32;
  int v33;
  int v34;
  int v35;
  unsigned int v36;
  int v37;
  int v38;
  int v39;
  int v40;
  unsigned int v41;
  u32 *v42;

  v3 = 20495;
  if ( a1 != 0 )
  {
    if ( *(u32 *)(a1 + 136) != 0 )
    {
      v7 = *(u32 *)(a1 + 8);
    }
    else
    {
      v8 = mp4_track_fn_24();
      v3 = 1;
      if ( v8 != 0 )
        return v3;
      v7 = *(u32 *)(a1 + 8);
    }
    v11 = mp4_sample_fn_5(4 * v7);
    v3 = 1;
    if ( v11 != 0 )
    {
      v12 = *(u32 *)(a1 + 8);
      v13 = 0;
      if ( v12 != 0 )
      {
        while ( 1 )
        {
          v21 = mp4_track_fn_69(a1, v13) + 392;
          v19 = (u32 *)mp4_sample_fn_5(288);
          v22 = (unsigned int *)(4 * v13 + a2);
          v23 = v13;
          v42 = v19;
          if ( v19 == NULL )
            break;
          v19[5] = 1937007212;
          v19[4] = 8;
          v19[69] = mp4_track_fn_69(a1, v13);
          mp4_codec_fn_24(v21, &v41, 0);
          v24 = v42;
          if ( v41 < *v22 )
            *v22 = v41;
          mp4_api_fn_63(v24, a3);
          *((u8 *)v42 + 268) = 1;
          v25 = mp4_track_fn_25(v21, v42, *v22);
          v9 = (u32 *)(4 * v13 + v11);
          if ( v25 != 0 )
          {
            v26 = 0;
            if ( v13 != 0 )
            {
              v27 = v11;
              do
              {
                v28 = *(u32 *)v27;
                *(u32 *)(*(u32 *)v27 + 40) = 0;
                ++v26;
                mp4_track_fn_64(v27, 1937007212);
                mp4_track_fn_65(v28, v29, v30);
                v27 += 4;
              }
              while ( v26 < v13 );
            }
            v31 = v42;
            v42[10] = 0;
            mp4_track_fn_64(&v42, 1937007212);
            mp4_track_fn_65(v31, v32, v33);
            mp4_track_fn_65(v11, v34, v35);
            return v25;
          }
          v12 = *(u32 *)(a1 + 8);
          ++v13;
          *v9 = v42;
          if ( v13 >= v12 )
            goto LABEL_8;
        }
        v36 = 0;
        if ( v13 != 0 )
        {
          v37 = v11;
          do
          {
            v38 = *(u32 *)v37;
            *(u32 *)(*(u32 *)v37 + 40) = 0;
            ++v36;
            mp4_track_fn_64(v37, 1937007212);
            mp4_track_fn_65(v38, v39, v40);
            v23 = v36 < v13;
            v37 += 4;
          }
          while ( v36 < v13 );
        }
        mp4_track_fn_65(v11, v23, v20);
        return 1;
      }
      else
      {
LABEL_8:
        v14 = 0;
        if ( v12 != 0 )
        {
          v15 = (u32 *)v11;
          do
          {
            v16 = mp4_track_fn_69(a1, v14);
            v17 = *(u32 *)(a1 + 8);
            ++v14;
            *(u32 *)(v16 + 680) = *v15++;
          }
          while ( v14 < v17 );
        }
        mp4_track_fn_65(v11, v9, v10);
        return 0;
      }
    }
  }
  return v3;
}


int mp4_track_fn_24(int a1)
{
  int result;
  u32 *v3;
  unsigned int v4;
  unsigned int i;
  int v6;
  int v7;
  int v8;
  int v9;
  u32 *v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  u8 v20[4];

  result = 20495;
  if ( a1 != 0 )
  {
    v3 = (u32 *)mp4_sample_fn_5(536);
    *(u32 *)(a1 + 136) = v3;
    result = 1;
    if ( v3 != NULL )
    {
      v3[1] = 1836475768;
      *v3 = 8;
      v4 = 0;
      if ( *(u32 *)(a1 + 8) != 0 )
      {
        for ( i = 0; ; i = v4 )
        {
          v6 = mp4_track_fn_69(a1, i);
          v7 = v6 + 392;
          v9 = v6;
          v8 = mp4_sample_fn_5(32);
          v10 = (u32 *)v8;
          if ( v8 == 0 )
            break;
          *(u32 *)v8 = 36;
          strcpy((char *)(v8 + 4), "xert");
          v11 = *(u32 *)(v9 + 32);
          *(u32 *)(v8 + 16) = 1;
          *(u32 *)(v8 + 12) = v11;
          if ( mp4_codec_fn_13(v7, 0, &v17, 0) != 0 )
            v10[5] = 0;
          else
            v10[5] = v17;
          mp4_codec_fn_27(v7, 0, &v18);
          v10[6] = v18;
          if ( mp4_codec_fn_38(v7, &v19, v20) != 0 )
          {
            v10[7] = 0;
          }
          else
          {
            v12 = 0;
            if ( v19 != 0 )
              v12 = 0x10000;
            v10[7] = v12;
          }
          v13 = *(u32 *)(a1 + 136);
          v14 = 4 * v4;
          v15 = 4 * v4++ + v13;
          if ( *(u32 *)(v15 + 28) != 0 )
          {
            mp4_track_fn_64(v15 + 28, 1953654136);
            v13 = *(u32 *)(a1 + 136);
          }
          v16 = *(u32 *)(v13 + 24);
          *(u32 *)(v14 + v13 + 28) = v10;
          *(u32 *)(v13 + 24) = v16 + 1;
          if ( v4 >= *(u32 *)(a1 + 8) )
            return 0;
        }
        mp4_track_fn_64(a1 + 136, 1836475768);
        return 1;
      }
      else
      {
        return 0;
      }
    }
  }
  return result;
}


int mp4_track_fn_25(u32 *a1, u32 *a2, unsigned int a3)
{
  unsigned int v4;
  u32 *v5;
  int v7;
  int v9;
  unsigned int v10;
  unsigned int v11;
  unsigned int v12;
  u32 *v13;
  int v14;
  u32 v15[8];
  unsigned int v16;
  int v17;

  v4 = 0;
  v5 = a1;
  a2[6] = a1[6];
  a2[7] = a1[7];
  a2[8] = a1[8];
  a2[9] = a1[9];
  a2[10] = a1[10];
  a2[11] = a1[11];
  a2[12] = a1[12];
  if ( a3 == 0 )
    return 0;
  while ( 1 )
  {
    v9 = mp4_codec_fn_20(a1, v4, 1, v15, 0);
    v7 = 4122;
    if ( v9 != 0 )
      return v7;
    v10 = v16;
    v11 = 0;
    if ( v16 != 0 )
    {
      v12 = v4;
      do
      {
        v13 = v15;
        ++v4;
        if ( v11 != 0 )
        {
          v14 = mp4_codec_fn_20(v5, v12, 0, v15, 1);
          v7 = 4122;
          if ( v14 != 0 )
            return v7;
        }
        ++v11;
        v7 = mp4_api_fn_69(a2, 1, v15[1], v13, v15[2], v15[3], v15[0], v15[4] != 1, v15[5], v17);
        if ( v7 != 0 )
          return v7;
        v12 = v4;
      }
      while ( v11 < v10 );
    }
    mp4_api_fn_71(a2, 0);
    if ( v4 >= a3 )
      return 0;
    a1 = v5;
  }
}


int mp4_track_fn_26(int a1, unsigned int a2, int *a3)
{
  unsigned int v4;
  int v6;
  unsigned int v8;
  int v9;
  int v10;
  unsigned int v11;
  int v12;
  unsigned int v13;
  unsigned int v14;
  int v15;
  int *v16;
  int v17;
  int v18;
  int v19;
  unsigned int v20;
  unsigned int v21;
  int v22;
  int v23;
  int v24;
  unsigned int *v25;
  unsigned int *v26;
  unsigned int *v27;
  int v28;
  unsigned int *v29;
  unsigned int *v30;
  unsigned int *v31;
  unsigned int v32;
  char v33[4];
  unsigned int *v34;

  v4 = a2;
  while ( v4 >= 8 )
  {
    v8 = mp4_box_fn_99(a3);
    if ( v8 < 8 )
      goto LABEL_5;
    v9 = mp4_box_fn_99(a3);
    v10 = v9;
    if ( v4 < v8 )
    {
      mp4_io_fn_12(v9, 1, a3);
      v8 = v4;
    }
    v11 = mp4_box_fn_2(*a3) + v8;
    v13 = v11 - 8;
    v14 = v12 + (v11 < v8) - 1 + (v11 - 8 < 0xFFFFFFF8);
    if ( v10 == 1835362404 )
    {
      v24 = 1835362404;
      if ( v8 < 0xC )
        goto LABEL_28;
      mp4_box_fn_103((int)v33, 1, a3);
      v16 = a3;
      v15 = 1835362404;
      if ( v33[0] != 0 )
        goto LABEL_12;
      if ( *(u32 *)(a1 + 8) != 0 )
      {
        v16 = a3;
        v15 = 1835362404;
        v17 = 8;
        goto LABEL_13;
      }
      *(u32 *)(a1 + 8) = v8;
      strcpy((char *)(a1 + 12), "dhem");
      mp4_track_fn_34(a1 + 8, a3, a3);
      v19 = a3[1];
    }
    else
    {
      if ( v10 != 1953654136 )
      {
        v15 = 1836475768;
        v16 = a3;
LABEL_12:
        v17 = 4;
LABEL_13:
        mp4_io_fn_12(v15, v17, v16);
        mp4_box_fn_24(*a3, v18, v13, v14);
LABEL_14:
        v19 = a3[1];
        goto LABEL_15;
      }
      if ( v8 < 0x20 )
      {
        v24 = 1953654136;
LABEL_28:
        mp4_io_fn_12(v24, 0x20000000, a3);
        v19 = a3[1];
        goto LABEL_15;
      }
      v25 = (unsigned int *)mp4_sample_fn_5(32);
      v34 = v25;
      v6 = 20481;
      if ( v25 == NULL )
        return v6;
      *v25 = v8;
      v25[1] = 1953654136;
      mp4_box_fn_103((int)(v25 + 2), 1, a3);
      mp4_box_fn_103((int)v34 + 9, 3, a3);
      v26 = v34;
      v26[3] = mp4_box_fn_99(a3);
      v27 = v34;
      v27[4] = mp4_box_fn_99(a3);
      v29 = v34;
      v28 = mp4_box_fn_99(a3);
      v30 = v34;
      v29[5] = v28;
      v30[6] = mp4_box_fn_99(a3);
      v31 = v34;
      v31[7] = mp4_box_fn_99(a3);
      v32 = *(u32 *)(a1 + 24);
      if ( v32 < 0x7F )
      {
        *(u32 *)(4 * v32 + a1 + 28) = v34;
        *(u32 *)(a1 + 24) = v32 + 1;
        goto LABEL_14;
      }
      mp4_io_fn_12(1953654136, 16, a3);
      mp4_track_fn_64(&v34, 1953654136);
      v19 = a3[1];
    }
LABEL_15:
    v6 = 20481;
    if ( (v19 & 0xE0000000) != 0 )
      return v6;
    v20 = mp4_box_fn_2(*a3);
    if ( v21 < v14 )
    {
      v22 = v10;
LABEL_25:
      mp4_io_fn_12(v22, 1, a3);
      mp4_box_fn_24(*a3, v23, v13, v14);
      v4 -= v8;
      continue;
    }
    if ( v14 == v21 )
    {
      if ( v20 < v13 )
      {
        v22 = v10;
        goto LABEL_25;
      }
      v4 -= v8;
    }
    else
    {
      v4 -= v8;
    }
  }
  if ( v4 == 0 )
    return 0;
LABEL_5:
  mp4_io_fn_12(1836475768, 1, a3);
  return 0;
}


int mp4_track_fn_27(int a1, int *a2, s64 a3, u32 *a4, int a5)
{
  int v7;
  s64 v8;
  int v9;
  s64 v10;
  int v11;
  int v12;
  u64 i;
  int v14;
  unsigned int v15;
  unsigned int v16;
  unsigned int v17;
  unsigned int v18;
  int v19;
  int v20;
  int v21;
  u64 v22;
  int v23;
  int v24;
  u64 v25;
  int v27;
  u64 *v28;
  unsigned int v29;
  int v30;
  int j;
  int v32;
  int v33;
  int v34; // dc
  int v35;
  int v36;
  int v37;
  int v38;
  int v39;
  int v40;
  int v41;
  u32 *v42;
  int v43;
  int v44;
  int v45;
  int v46;
  int v47;
  unsigned int v48;
  int v49;
  int v50;
  int v51;
  unsigned int v52;
  int v53;
  int v54;
  u32 *v55;
  int v56;
  int v57;
  int v58;
  unsigned int v59;
  int v60;

  v54 = a1;
  v7 = *a2;
  v56 = a5;
  v55 = a4;
  v53 = 0;
  v8 = ((u64)(v8) & 0xFFFFFFFF00000000ULL) | (u32)(mp4_box_fn_4(v7));
  v9 = *a2;
  v10 = v8;
  v57 = 0;
  mp4_box_fn_24(v9, v11, a3, SHIDWORD(a3));
  *(u64 *)v56 = v10;
  if ( HIDWORD(v10) < HIDWORD(a3) )
    return 2;
  if ( HIDWORD(a3) == HIDWORD(v10) )
  {
    if ( (unsigned int)v10 < (unsigned int)a3 )
      return 2;
    a2[1] = 0;
  }
  else
  {
    a2[1] = 0;
  }
  for ( i = v10 - a3; ; i -= v16 )
  {
    if ( i < 8 )
    {
      v14 = a2[1];
      goto LABEL_22;
    }
    v15 = mp4_box_fn_99(a2);
    v16 = v15;
    if ( v15 < 8 )
    {
      v14 = a2[1];
      goto LABEL_22;
    }
    if ( i < v15 )
      v16 = i;
    v17 = mp4_box_fn_99(a2);
    if ( a2[1] < 0 )
      break;
    v18 = mp4_box_fn_2(*a2) + v16;
    v21 = 1835295092;
    v22 = ((u64)(v22) & 0xFFFFFFFF00000000ULL) | (u32)(v18 - 8);
    v22 = ((u64)(v22) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v20 + (v18 < v16) - 1 + (v18 - 8 < 0xFFFFFFF8)) << 32);
    if ( v17 == 1835295092 )
      goto LABEL_16;
    if ( v17 <= 0x6D646174 )
    {
      if ( v17 != 1718773093 )
      {
        v23 = 1718909296;
        if ( v17 <= 0x66726565 )
          return 20496;
LABEL_15:
        v12 = 20496;
        if ( v17 != v23 )
          return v12;
      }
LABEL_16:
      v24 = v57 < 2;
      mp4_box_fn_24(*a2, v21, v22, SHIDWORD(v22));
      goto LABEL_17;
    }
    if ( v17 != 1836019558 )
    {
      v23 = 1835430497;
      if ( v17 > 0x6D6F6F66 )
      {
        v21 = 1835991040;
        v23 = 1836019574;
      }
      goto LABEL_15;
    }
    v24 = ++v57 < 2;
    if ( v57 < 2 )
    {
      v29 = *(u32 *)(v54 + 260);
      if ( v29 >= 2 )
      {
        v30 = *(u32 *)(v54 + 264);
        v58 = 0;
        if ( v30 != 0 )
        {
LABEL_39:
          for ( j = *(u32 *)(v30 + 552); ; j = *(u32 *)(v30 + 552) )
          {
            if ( j != 0 )
            {
              v32 = *(u32 *)(v30 + 536);
              if ( v30 == *(u32 *)(v54 + 264) )
                *(u32 *)(v54 + 264) = v32;
              v33 = v58;
              v34 = v58 == 0;
              *(u32 *)(v30 + 536) = 0;
              if ( !v34 )
                *(u32 *)(v33 + 536) = v32;
              mp4_track_fn_36();
              v30 = v32;
              v35 = *(u32 *)(v54 + 260);
              *(u32 *)(v54 + 260) = v35 - 1;
              if ( v32 == 0 )
              {
                v29 = v35 - 1;
                goto LABEL_47;
              }
              goto LABEL_39;
            }
            v58 = v30;
            if ( v30 == 0 )
              break;
            v30 = *(u32 *)(v30 + 536);
            if ( v30 == 0 )
            {
              v19 = v54;
              v29 = *(u32 *)(v54 + 260);
              goto LABEL_47;
            }
          }
          v29 = *(u32 *)(v54 + 260);
        }
LABEL_47:
        while ( v29 >= 2 )
        {
          v36 = *(u32 *)(v54 + 264);
          if ( v36 != 0 )
          {
            v37 = *(u32 *)(v36 + 536);
            *(u32 *)(v36 + 536) = 0;
            mp4_track_fn_36();
            v38 = v54;
            v39 = *(u32 *)(v54 + 260);
            *(u32 *)(v54 + 264) = v37;
            v29 = v39 - 1;
            *(u32 *)(v38 + 260) = v39 - 1;
          }
        }
      }
      if ( mp4_io_fn_13(&v53, 1836019558, 568, v19, v22, HIDWORD(v22), a2) != 20481 )
      {
        v40 = v53;
        v41 = *(u32 *)(v56 + 4);
        v42 = v55;
        *(u32 *)(v53 + 560) = *(u32 *)v56;
        *(u32 *)(v40 + 564) = v41;
        *(u64 *)(v40 + 544) = v22 - v16;
        *v42 = v40;
        *(u32 *)v40 = v16;
        *(u32 *)(v40 + 4) = 1836019558;
        v43 = mp4_track_fn_28(v40, v16 - 8, a2);
        v44 = v53;
        if ( v43 == 20481 )
        {
          v48 = 0;
          v49 = v53;
          if ( *(u32 *)(v53 + 24) != 0 )
          {
            do
            {
              v50 = 4 * v48;
              v51 = *(u32 *)(4 * v48 + v49 + 28);
              v59 = v48;
              mp4_track_fn_37(v51);
              v44 = v53;
              v52 = *(u32 *)(v53 + 24);
              v48 = (u8)(v59 + 1);
              *(u32 *)(v50 + v53 + 28) = 0;
              v49 = v44;
            }
            while ( v48 < v52 );
            *(u32 *)(v44 + 24) = 0;
          }
          else
          {
            *(u32 *)(v53 + 24) = 0;
          }
        }
        mp4_track_fn_42(v54, v44);
        v45 = 0;
        if ( *(u32 *)(v53 + 24) != 0 )
        {
          v46 = 0;
          do
          {
            v47 = 4 * v46;
            v60 = v46;
            mp4_track_fn_49(v45);
            v46 = v60 + 1;
            v45 = *(u32 *)(v47 + v53 + 28);
          }
          while ( (unsigned int)(v60 + 1) < *(u32 *)(v53 + 24) );
        }
      }
    }
    else
    {
      v28 = (u64 *)v56;
      *(u64 *)(v53 + 560) = v22 - v16;
      *v28 = v22 - v16;
    }
LABEL_17:
    v14 = a2[1];
    if ( !v24 || (v14 & 0xE0000000) != 0 )
      goto LABEL_22;
    v25 = ((u64)(v25) & 0xFFFFFFFF00000000ULL) | (u32)(mp4_box_fn_2(*a2));
    if ( v25 < v22 )
    {
      mp4_io_fn_12(v17, 1, a2);
      mp4_box_fn_24(*a2, v27, v22, SHIDWORD(v22));
    }
  }
  mp4_io_fn_12(1936549485, 1, a2);
  v14 = a2[1];
LABEL_22:
  v12 = 2;
  if ( v14 >= 0 )
  {
    v12 = 1;
    if ( (v14 & 0x40000000) == 0 )
    {
      v12 = 8195;
      if ( (v14 & 0x2000003B) == 0 )
      {
        v12 = 20496;
        if ( v57 != 0 )
        {
          v12 = 0;
          if ( v57 <= 0 )
            return 8195;
        }
      }
    }
  }
  return v12;
}


int mp4_track_fn_28(int a1, unsigned int a2, int *a3)
{
  unsigned int v4;
  int v6;
  unsigned int v8;
  int v9;
  int v10;
  unsigned int v11;
  int v12;
  unsigned int v13;
  unsigned int v14;
  int v15;
  int *v16;
  int v17;
  int v18;
  int v19;
  unsigned int v20;
  unsigned int v21;
  int v22;
  int v23;
  unsigned int *v24;
  unsigned int v25;
  char v26[4];
  unsigned int *v27;

  v4 = a2;
  while ( v4 >= 8 )
  {
    v8 = mp4_box_fn_99(a3);
    if ( v8 < 8 )
      goto LABEL_5;
    v9 = mp4_box_fn_99(a3);
    v10 = v9;
    if ( v4 < v8 )
    {
      mp4_io_fn_12(v9, 1, a3);
      v8 = v4;
    }
    v11 = mp4_box_fn_2(*a3) + v8;
    v13 = v11 - 8;
    v14 = v12 + (v11 < v8) - 1 + (v11 - 8 < 0xFFFFFFF8);
    if ( v10 == 1835427940 )
    {
      if ( v8 < 0xC )
        goto LABEL_38;
      mp4_box_fn_103((int)v26, 1, a3);
      v16 = a3;
      v15 = 1835427940;
      if ( v26[0] != 0 )
        goto LABEL_12;
      if ( v8 < 0x10 )
      {
LABEL_38:
        mp4_io_fn_12(1835427940, 0x20000000, a3);
        v19 = a3[1];
      }
      else
      {
        if ( *(u32 *)(a1 + 8) != 0 )
        {
          v16 = a3;
          v15 = 1835427940;
          v17 = 8;
LABEL_13:
          mp4_io_fn_12(v15, v17, v16);
          mp4_box_fn_24(*a3, v18, v13, v14);
LABEL_14:
          v19 = a3[1];
          goto LABEL_15;
        }
        *(u32 *)(a1 + 8) = v8;
        strcpy((char *)(a1 + 12), "dhfm");
        mp4_track_fn_35(a1 + 8, a3, a3);
        v19 = a3[1];
      }
    }
    else
    {
      if ( v10 != 1953653094 )
      {
        v15 = 1836019558;
        v16 = a3;
LABEL_12:
        v17 = 4;
        goto LABEL_13;
      }
      v24 = (unsigned int *)mp4_sample_fn_5(96);
      v27 = v24;
      v6 = 20481;
      if ( v24 == NULL )
        return v6;
      *v24 = v8;
      v24[1] = 1953653094;
      if ( mp4_track_fn_33(v24, v8 - 8, a3) == 20481 )
      {
        mp4_track_fn_37(v27);
        v6 = 20481;
        v27 = NULL;
        return v6;
      }
      v25 = *(u32 *)(a1 + 24);
      if ( v25 < 0x7F )
      {
        *(u32 *)(4 * v25 + a1 + 28) = v27;
        *(u32 *)(a1 + 24) = v25 + 1;
        goto LABEL_14;
      }
      mp4_io_fn_12(1953653094, 16, a3);
      mp4_track_fn_64(&v27, 1953653094);
      v19 = a3[1];
    }
LABEL_15:
    v6 = 20481;
    if ( (v19 & 0xE0000000) != 0 )
      return v6;
    v20 = mp4_box_fn_2(*a3);
    if ( v21 < v14 )
    {
      v22 = v10;
LABEL_25:
      mp4_io_fn_12(v22, 1, a3);
      mp4_box_fn_24(*a3, v23, v13, v14);
      v4 -= v8;
      continue;
    }
    if ( v14 == v21 )
    {
      if ( v20 < v13 )
      {
        v22 = v10;
        goto LABEL_25;
      }
      v4 -= v8;
    }
    else
    {
      v4 -= v8;
    }
  }
  if ( v4 == 0 )
    return 0;
LABEL_5:
  mp4_io_fn_12(1836019558, 1, a3);
  return 0;
}


int mp4_track_fn_29(int a1, int *a2, s64 a3, u32 *a4, int a5)
{
  int v9;
  int v11;

  v9 = mp4_track_fn_53();
  v11 = 0;
  if ( v9 != 0 )
  {
    v11 = 10;
    if ( v9 == 8194 )
      return mp4_track_fn_27(a1, a2, a3, a4, a5);
  }
  return v11;
}


int mp4_track_fn_30(u32 *a1, int a2, int a3, u32 *a4)
{
  int v7;
  int v8;
  unsigned int v9;
  int v10;
  unsigned int v11;
  int *v12;
  int i;
  int v15;
  int v16;
  u32 v17[3];

  v7 = mp4_track_fn_56(*(u32 *)(a1[135] + 136), a2, &v16);
  v8 = 10;
  if ( v7 == 0 )
  {
    v9 = a1[6];
    v10 = 0;
    v11 = 0;
    if ( v9 != 0 )
    {
      v12 = a1 + 7;
      for ( i = a1[7]; ; i = *v12 )
      {
        ++v11;
        ++v12;
        if ( *(u32 *)(i + 20) == a2 )
        {
          v15 = mp4_track_fn_57(i, v16, v17);
          v8 = 10;
          if ( v15 != 0 )
            return v8;
          v9 = a1[6];
          v10 += v17[0];
        }
        if ( v11 >= v9 )
          break;
      }
    }
    *a4 = v10;
    return 0;
  }
  return v8;
}


int mp4_track_fn_31(u32 *a1, int a2, int a3, unsigned int a4, int a5, int a6, u32 *a7)
{
  int v13;
  int v14;
  unsigned int v15;
  int v16;
  unsigned int v17;
  int *v18;
  int v19;
  int v20;
  int v22;
  int v23;
  int v24;
  int v25;

  v13 = mp4_track_fn_56(*(u32 *)(a1[135] + 136), a2, &v23);
  v14 = 10;
  if ( v13 == 0 )
  {
    v15 = a1[6];
    v14 = 20498;
    if ( a4 < v15 )
    {
      v16 = 0;
      v17 = 0;
      if ( v15 != 0 )
      {
        v18 = a1 + 7;
        while ( v17 != a4 )
        {
          v19 = *v18;
          ++v17;
          v20 = *(u32 *)(*v18++ + 20);
          if ( v20 == a2 )
          {
            v22 = mp4_track_fn_57(v19, v23, &v24);
            v14 = 10;
            if ( v22 != 0 )
              return v14;
            v15 = a1[6];
            v16 += v24;
          }
          if ( v17 >= v15 )
            break;
        }
      }
      v14 = mp4_track_fn_58(a1[v17 + 7], v23, a5, a6, &v25);
      if ( v14 == 0 )
      {
        v14 = 0;
        *a7 = v16 + v25;
      }
    }
  }
  return v14;
}


int mp4_track_fn_32(u32 *a1, int a2, int a3, unsigned int a4, int a5, int a6, u32 *a7)
{
  int v13;
  int v14;
  int v15;
  unsigned int v16;
  int v17;
  unsigned int v18;
  int *v19;
  int i;
  int v22;
  int v23;
  int v24;
  int v25;
  u32 *v26;

  v13 = a1[135];
  v26 = a7;
  v14 = mp4_track_fn_56(*(u32 *)(v13 + 136), a2, &v23);
  v15 = 10;
  if ( v14 == 0 )
  {
    v16 = a1[6];
    v15 = 20498;
    if ( a4 < v16 )
    {
      v17 = 0;
      v18 = 0;
      if ( v16 != 0 )
      {
        v19 = a1 + 7;
        for ( i = a1[7]; ; i = *v19 )
        {
          ++v18;
          ++v19;
          if ( *(u32 *)(i + 20) == a2 )
          {
            v22 = mp4_track_fn_57(i, v23, &v24);
            v15 = 10;
            if ( v22 != 0 )
              return v15;
            v16 = a1[6];
            v17 += v24;
          }
          if ( v18 >= v16 )
            break;
        }
      }
      v15 = mp4_track_fn_31(a1, a2, a3, a4, a5, a6, &v25);
      if ( v15 == 0 )
      {
        v15 = 0;
        *v26 = v17 - v25;
      }
    }
  }
  return v15;
}


int mp4_track_fn_33(int a1, unsigned int a2, int *a3)
{
  unsigned int v4;
  int v6;
  unsigned int v8;
  int v9;
  int v10;
  unsigned int v11;
  int v12;
  unsigned int v13;
  unsigned int v14;
  int v15;
  int *v16;
  int v17;
  int v18;
  int v19;
  unsigned int v20;
  unsigned int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  char v26;
  int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  unsigned int v34;
  unsigned int v35;
  int v36;
  unsigned int v37;
  unsigned int v38;
  unsigned int v39;
  unsigned int v40;
  int v41;
  int v42;
  int v43;
  int v44;
  int v45;
  unsigned int v46;
  unsigned int v47;
  u8 v48[16];
  int v49;
  int v50;
  int v51;
  int v52;
  int v53[4];

  v4 = a2;
  while ( v4 >= 8 )
  {
    v8 = mp4_box_fn_99(a3);
    if ( v8 < 8 )
      goto LABEL_5;
    v9 = mp4_box_fn_99(a3);
    v10 = v9;
    if ( v4 < v8 )
    {
      mp4_io_fn_12(v9, 1, a3);
      v8 = v4;
    }
    v11 = mp4_box_fn_2(*a3) + v8;
    v13 = v11 - 8;
    v14 = v12 + (v11 < v8) - 1 + (v11 - 8 < 0xFFFFFFF8);
    if ( v10 == 1952868452 )
    {
      if ( v8 < 0xC )
        goto LABEL_89;
      mp4_box_fn_103((int)v48, 1, a3);
      v16 = a3;
      v15 = 1952868452;
      if ( v48[0] != 0 )
        goto LABEL_12;
      if ( v8 < 0x10 )
      {
LABEL_89:
        v24 = 1952868452;
        goto LABEL_30;
      }
      if ( *(u32 *)(a1 + 8) != 0 )
      {
        v16 = a3;
        v15 = 1952868452;
        v17 = 8;
LABEL_13:
        mp4_io_fn_12(v15, v17, v16);
        mp4_box_fn_24(*a3, v18, v13, v14);
LABEL_14:
        v19 = a3[1];
        goto LABEL_15;
      }
      *(u32 *)(a1 + 8) = v8;
      strcpy((char *)(a1 + 12), "dhft");
      mp4_box_fn_103(a1 + 17, 3, a3);
      *(u32 *)(a1 + 20) = mp4_box_fn_99(a3);
      if ( mp4_sample_fn_15(*(u8 *)(a1 + 19), 0) != 0 )
      {
        *(u32 *)(a1 + 24) = mp4_box_fn_99(a3);
        *(u32 *)(a1 + 28) = mp4_box_fn_99(a3);
      }
      if ( mp4_sample_fn_15(*(u8 *)(a1 + 19), 1) != 0 )
        *(u32 *)(a1 + 32) = mp4_box_fn_99(a3);
      if ( mp4_sample_fn_15(*(u8 *)(a1 + 19), 3) != 0 )
        *(u32 *)(a1 + 36) = mp4_box_fn_99(a3);
      if ( mp4_sample_fn_15(*(u8 *)(a1 + 19), 4) != 0 )
        *(u32 *)(a1 + 40) = mp4_box_fn_99(a3);
      if ( mp4_sample_fn_15(*(u8 *)(a1 + 19), 5) != 0 )
        *(u32 *)(a1 + 44) = mp4_box_fn_99(a3);
      v46 = mp4_box_fn_2(*a3);
      if ( v47 < v14 )
      {
        v24 = 1952868452;
        goto LABEL_30;
      }
      if ( v14 == v47 )
      {
        if ( v46 < v13 )
        {
          v24 = 1952868452;
          goto LABEL_30;
        }
        v19 = a3[1];
      }
      else
      {
        v19 = a3[1];
      }
    }
    else
    {
      if ( v10 != 1953658222 )
      {
        v15 = 1953653094;
        v16 = a3;
LABEL_12:
        v17 = 4;
        goto LABEL_13;
      }
      if ( v8 < 0xC )
        goto LABEL_29;
      mp4_box_fn_103((int)v48, 1, a3);
      if ( v48[0] != 0 )
      {
        v16 = a3;
        v15 = 1953658222;
        goto LABEL_12;
      }
      if ( v8 < 0x10 )
      {
LABEL_29:
        v24 = 1953658222;
LABEL_30:
        mp4_io_fn_12(v24, 0x20000000, a3);
        v19 = a3[1];
        goto LABEL_15;
      }
      v25 = mp4_sample_fn_5(48);
      v53[0] = v25;
      v6 = 20481;
      if ( v25 == 0 )
        return v6;
      *(u32 *)v25 = v8;
      v26 = v48[0];
      *(u32 *)(v25 + 4) = 1953658222;
      *(u8 *)(v25 + 8) = v26;
      mp4_box_fn_103(v53[0] + 9, 3, a3);
      v28 = v53[0];
      v27 = mp4_box_fn_99(a3);
      v29 = v53[0];
      *(u32 *)(v28 + 12) = v27;
      v30 = mp4_sample_fn_15(*(u8 *)(v29 + 11), 0);
      v31 = v53[0];
      if ( v30 != 0 )
        *(u32 *)(v31 + 16) = mp4_box_fn_99(a3);
      v32 = mp4_sample_fn_15(*(u8 *)(v53[0] + 11), 2);
      v33 = v53[0];
      if ( v32 != 0 )
        *(u32 *)(v33 + 20) = mp4_box_fn_99(a3);
      v34 = mp4_box_fn_2(*a3);
      if ( v14 >= v35 )
      {
        if ( v35 != v14 )
        {
          v36 = v53[0];
LABEL_39:
          v37 = 0;
          if ( (*(u8 *)(v36 + 10) & 0xF) == 0 || (v38 = *(u32 *)(v36 + 12), *(u32 *)(v36 + 12) = 0, v38 == 0) )
          {
LABEL_58:
            v45 = mp4_track_fn_40(a1, v36);
            v6 = 20481;
            if ( v45 != 0 )
              return v6;
            *(u32 *)(a1 + 60) += *(u32 *)(v53[0] + 12);
            goto LABEL_14;
          }
          v49 = 0;
          while ( 2 )
          {
            v50 = 0;
            v51 = 0;
            v52 = 0;
            if ( mp4_sample_fn_15(*(u8 *)(v36 + 10), 0) != 0 )
              v49 = mp4_box_fn_99(a3);
            if ( mp4_sample_fn_15(*(u8 *)(v53[0] + 10), 1) != 0 )
              v50 = mp4_box_fn_99(a3);
            if ( mp4_sample_fn_15(*(u8 *)(v53[0] + 10), 2) != 0 )
              v51 = mp4_box_fn_99(a3);
            if ( mp4_sample_fn_15(*(u8 *)(v53[0] + 10), 3) != 0 )
              v52 = mp4_box_fn_99(a3);
            v39 = mp4_box_fn_2(*a3);
            if ( v14 < v40 )
            {
              v42 = *a3;
              goto LABEL_62;
            }
            if ( v40 != v14 )
            {
              v43 = v53[0];
              goto LABEL_54;
            }
            v43 = v53[0];
            if ( v13 >= v39 )
            {
LABEL_54:
              v44 = mp4_track_fn_41(v43, &v49);
              v6 = 20481;
              if ( v44 != 0 )
                return v6;
              ++v37;
            }
            else
            {
              v42 = *a3;
LABEL_62:
              mp4_box_fn_24(v42, v41, v13, v14);
              mp4_io_fn_12(1953658222, 1, a3);
              v37 = v38 + 1;
            }
            v36 = v53[0];
            if ( v37 >= v38 )
              goto LABEL_58;
            v49 = 0;
            continue;
          }
        }
        v36 = v53[0];
        if ( v13 >= v34 )
          goto LABEL_39;
      }
      mp4_io_fn_12(1953658222, 0x20000000, a3);
      mp4_track_fn_64(v53, 1953658222);
      v19 = a3[1];
    }
LABEL_15:
    v6 = 20481;
    if ( (v19 & 0xE0000000) != 0 )
      return v6;
    v20 = mp4_box_fn_2(*a3);
    if ( v21 < v14 )
    {
      v22 = v10;
LABEL_25:
      mp4_io_fn_12(v22, 1, a3);
      mp4_box_fn_24(*a3, v23, v13, v14);
      v4 -= v8;
      continue;
    }
    if ( v14 == v21 )
    {
      if ( v20 < v13 )
      {
        v22 = v10;
        goto LABEL_25;
      }
      v4 -= v8;
    }
    else
    {
      v4 -= v8;
    }
  }
  if ( v4 == 0 )
    return 0;
LABEL_5:
  mp4_io_fn_12(1953653094, 1, a3);
  return 0;
}


int mp4_track_fn_34(int a1, int *a2)
{
  int result;

  mp4_box_fn_103(a1 + 9, 3, a2);
  result = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 12) = result;
  return result;
}


int mp4_track_fn_35(int a1, int *a2)
{
  int result;

  mp4_box_fn_103(a1 + 9, 3, a2);
  result = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 12) = result;
  return result;
}


int mp4_track_fn_36(int a1, int a2, int a3)
{
  int v3;
  int v4;
  unsigned int v5;
  int *v6;
  int i;
  int result;

  v3 = a1;
  if ( a1 != 0 )
  {
    do
    {
      v4 = *(u32 *)(v3 + 536);
      v5 = 0;
      v6 = (int *)(v3 + 28);
      for ( i = *(u32 *)(v3 + 28); ; i = *v6 )
      {
        ++v5;
        ++v6;
        if ( i != 0 )
          mp4_track_fn_37(i);
        if ( v5 >= 0x7F )
          break;
      }
      result = mp4_track_fn_65(v3, a2, a3);
      v3 = v4;
    }
    while ( v4 != 0 );
  }
  return result;
}


int mp4_track_fn_37(int a1)
{
  int v2;
  int v3;
  int result;

  if ( a1 != 0 )
  {
    mp4_track_fn_38(*(u32 *)(a1 + 56));
    return mp4_track_fn_65(a1, v2, v3);
  }
  return result;
}


int mp4_track_fn_38(int a1, int a2, int a3)
{
  int v3;
  int v4;
  int result;

  v3 = a1;
  if ( a1 != 0 )
  {
    do
    {
      v4 = *(u32 *)(v3 + 32);
      if ( *(u32 *)(v3 + 28) != 0 )
        mp4_codec_fn_102(*(u32 *)(v3 + 28));
      result = mp4_track_fn_65(v3, a2, a3);
      v3 = v4;
    }
    while ( v4 != 0 );
  }
  return result;
}


void mp4_track_fn_39(u32 *a1)
{
  int v1;
  unsigned int v2;
  unsigned int v3;
  u32 **v4;
  u32 *i;

  v1 = a1[2];
  *a1 = 8;
  if ( v1 != 0 )
  {
    a1[2] = 16;
    *a1 = 24;
  }
  v2 = a1[6];
  v3 = 0;
  if ( v2 != 0 )
  {
    v4 = (u32 **)(a1 + 7);
    for ( i = (u32 *)a1[7]; ; i = *v4 )
    {
      ++v3;
      ++v4;
      if ( i != NULL )
      {
        *i = 32;
        v2 = a1[6];
        *a1 += 32;
      }
      if ( v3 >= v2 )
        break;
    }
  }
}


int mp4_track_fn_40(int a1, int a2)
{
  int v2;
  int v4;

  v2 = 20495;
  if ( a1 != 0 && a2 != 0 )
  {
    v4 = *(u32 *)(a1 + 52);
    if ( v4 != 0 )
    {
      *(u32 *)(mp4_track_fn_43() + 32) = a2;
      v4 = *(u32 *)(a1 + 52);
    }
    else
    {
      *(u32 *)(a1 + 56) = a2;
    }
    *(u32 *)(a1 + 52) = v4 + 1;
    return 0;
  }
  return v2;
}


int mp4_track_fn_41(int a1, u32 *a2)
{
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  u32 v11[4];

  v3 = a1 == 0 || a2 == NULL;
  v4 = a1 + 28;
  v5 = 20495;
  if ( !v3 )
  {
    v6 = a2[3];
    v7 = a2[1];
    v8 = a2[2];
    v11[0] = *a2;
    v11[1] = v7;
    v11[2] = v8;
    v11[3] = v6;
    v9 = mp4_codec_fn_89(v4, 4, v11, 20);
    v5 = 1;
    if ( v9 == 0 )
    {
      v5 = 0;
      ++*(u32 *)(a1 + 12);
    }
  }
  return v5;
}


int mp4_track_fn_42(int a1, int a2)
{
  int v3;
  int v5;
  int v7;
  int v8;

  v3 = 20481;
  if ( a1 != 0 && a2 != 0 )
  {
    v5 = *(u32 *)(a1 + 260);
    if ( v5 != 0 )
    {
      v7 = ((int (*)(void))mp4_track_fn_45)();
      v3 = 20481;
      if ( *(u32 *)(v7 + 536) != 0 )
        return v3;
      v8 = mp4_track_fn_45(a1, *(u32 *)(a1 + 260) - 1, 20481);
      v5 = *(u32 *)(a1 + 260);
      *(u32 *)(v8 + 536) = a2;
      goto LABEL_5;
    }
    if ( *(u32 *)(a1 + 264) == 0 )
    {
      *(u32 *)(a1 + 264) = a2;
LABEL_5:
      *(u32 *)(a2 + 540) = a1;
      v3 = 0;
      *(u32 *)(a1 + 260) = v5 + 1;
    }
  }
  return v3;
}


int mp4_track_fn_43(int a1, unsigned int a2)
{
  int v2;
  int v3;
  unsigned int v4;
  int i;

  v2 = 0;
  if ( a2 < *(u32 *)(a1 + 52) )
  {
    v3 = *(u32 *)(a1 + 56);
    v4 = 0;
    if ( a2 != 0 )
    {
      for ( i = *(u32 *)(v3 + 32); ; i = *(u32 *)(i + 32) )
      {
        ++v4;
        v3 = i;
        v2 = 0;
        if ( i == 0 )
          break;
        if ( v4 >= a2 )
          return v3;
      }
    }
    else
    {
      return v3;
    }
  }
  return v2;
}


int mp4_track_fn_44(int a1, int a2, int a3)
{
  int v6;
  int v7;

  v6 = mp4_codec_fn_91(*(u32 *)(a1 + 28), a2, 0, a3);
  v7 = 20481;
  if ( v6 == 0 )
  {
    mp4_codec_fn_91(*(u32 *)(a1 + 28), a2, 1, a3 + 4);
    mp4_codec_fn_91(*(u32 *)(a1 + 28), a2, 2, a3 + 8);
    mp4_codec_fn_91(*(u32 *)(a1 + 28), a2, 3, a3 + 12);
    return 0;
  }
  return v7;
}


int mp4_track_fn_45(int a1, unsigned int a2)
{
  int v2;
  int v3;
  unsigned int v4;
  int i;

  v2 = 0;
  if ( a2 < *(u32 *)(a1 + 260) )
  {
    v3 = *(u32 *)(a1 + 264);
    v4 = 0;
    if ( a2 != 0 )
    {
      for ( i = *(u32 *)(v3 + 536); ; i = *(u32 *)(i + 536) )
      {
        ++v4;
        v3 = i;
        v2 = 0;
        if ( i == 0 )
          break;
        if ( v4 >= a2 )
          return v3;
      }
    }
    else
    {
      return v3;
    }
  }
  return v2;
}


int mp4_track_fn_46(int a1, int a2, int a3, unsigned int a4, unsigned int a5, u32 *a6)
{
  int v12;
  unsigned int v13;
  int v14;
  int v16;
  int v17;
  u32 v18[4];

  *a6 = 0;
  v12 = 4109;
  if ( a4 < *(u32 *)(a3 + 12) )
  {
    if ( mp4_sample_fn_15(*(u8 *)(a3 + 10), 1) == 0 )
    {
      if ( mp4_sample_fn_15(*(u8 *)(a2 + 19), 4) != 0 )
      {
        v17 = *(u32 *)(a2 + 40);
      }
      else
      {
        v12 = 8195;
        if ( a1 == 0 )
          return v12;
        v17 = *(u32 *)(a1 + 24);
      }
      *a6 += (a5 - a4 + 1) * v17;
      return 0;
    }
    v13 = a4;
    if ( a5 < a4 )
      return 0;
    v14 = a4;
    if ( a4 >= *(u32 *)(a3 + 12) )
      return 0;
    while ( 1 )
    {
      v16 = mp4_track_fn_44(a3, v14, (int)v18);
      ++v13;
      v12 = 4109;
      if ( v16 != 0 )
        break;
      *a6 += v18[1];
      if ( a5 < v13 )
        return 0;
      v14 = v13;
      if ( v13 >= *(u32 *)(a3 + 12) )
        return 0;
    }
  }
  return v12;
}


int mp4_track_fn_47(int a1, unsigned int a2, int a3, int a4, u32 *a5)
{
  int v6;
  int v7;
  u32 v9[4];

  v6 = 4109;
  if ( *(u32 *)(a1 + 12) >= a2 )
  {
    v6 = 8195;
    if ( (*(u8 *)(a1 + 10) & 1) != 0 )
    {
      mp4_track_fn_44(a1, a2, (int)v9);
      v6 = 0;
      *a5 = v9[0];
      return v6;
    }
    if ( a3 != 0 )
    {
      v6 = 0;
      if ( (*(u8 *)(a3 + 11) & 8) != 0 )
      {
        v7 = *(u32 *)(a3 + 28);
      }
      else
      {
        v6 = 8195;
        if ( a4 == 0 )
          return v6;
        v7 = *(u32 *)(a4 + 20);
        v6 = 0;
      }
      *a5 = v7;
    }
  }
  return v6;
}


int mp4_track_fn_48(int a1, unsigned int a2, int a3, int a4, u32 *a5)
{
  int v6;
  char v7;
  int v8;
  u32 v10[4];

  v6 = 4109;
  if ( *(u32 *)(a1 + 12) < a2 )
    return v6;
  if ( a2 != 0 )
  {
    v7 = *(u8 *)(a1 + 10);
    goto LABEL_12;
  }
  if ( (*(u8 *)(a1 + 11) & 4) == 0 )
  {
    v7 = *(u8 *)(a1 + 10);
LABEL_12:
    if ( (v7 & 4) != 0 )
    {
      mp4_track_fn_44(a1, a2, (int)v10);
      v8 = v10[2];
    }
    else
    {
      v6 = 8195;
      if ( a3 == 0 )
        return v6;
      if ( (*(u8 *)(a3 + 11) & 0x20) != 0 )
      {
        v8 = *(u32 *)(a3 + 36);
      }
      else
      {
        v6 = 8195;
        if ( a4 == 0 )
          return v6;
        v8 = *(u32 *)(a4 + 28);
      }
    }
    goto LABEL_7;
  }
  v8 = *(u32 *)(a1 + 20);
LABEL_7:
  *a5 = (v8 & 0x10000) == 0;
  return 0;
}


int mp4_track_fn_49(u32 *a1, u32 *a2, int a3)
{
  unsigned int v6;
  unsigned int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 *v12;
  unsigned int v13;
  int i;
  int v15;
  unsigned int v16;
  int v17;
  int v18;
  int v19;
  u32 *v20;
  unsigned int v21;
  unsigned int v22;
  unsigned int v23;
  int v24;
  unsigned int v25;
  int v26;
  int v28;
  int v29;
  int v30;
  int v31;
  unsigned int v32;
  int v33;
  int v34;
  int v35;
  unsigned int v36;
  unsigned int v37;
  int v38;
  unsigned int v39;
  int v40;
  unsigned int v41;
  int v42;

  if ( (*(u8 *)(a3 + 19) & 1) != 0 )
  {
    v31 = *(u32 *)(a3 + 24);
    v32 = *(u32 *)(a3 + 28);
    goto LABEL_17;
  }
  if ( a1 == NULL )
  {
    v31 = a2[137];
    v32 = a2[136];
    goto LABEL_17;
  }
  v6 = 0;
  v7 = 0;
  v8 = mp4_sample_fn_12(a2[135], a1[5], &v33);
  v9 = 0;
  v10 = 8194;
  if ( v8 != 0 )
    return v10;
  v11 = 0;
  mp4_track_fn_70(v33, &v34);
  v12 = (u32 *)a1[14];
  v13 = a1[13];
  if ( v13 == 0 || v12 == NULL )
    goto LABEL_15;
  for ( i = v12[10]; ; i = v12[10] )
  {
    if ( i != -1 )
    {
      v15 = v12[3];
      goto LABEL_9;
    }
    if ( v12[11] != -1 )
    {
      v15 = v12[3];
LABEL_9:
      v16 = v12[11];
      v36 = v12[10];
      v37 = v16;
      if ( mp4_track_fn_46(v34, (int)a1, (int)v12, 0, v15 - 1, &v35) != 0 )
      {
        v13 = a1[13];
LABEL_11:
        v12 = (u32 *)v12[8];
        goto LABEL_12;
      }
      if ( mp4_sample_fn_17(&v36, v35) != 0 )
      {
        v13 = a1[13];
        goto LABEL_11;
      }
      if ( v7 >= v37 )
      {
        if ( v37 != v7 )
        {
          v13 = a1[13];
LABEL_44:
          ++v9;
          goto LABEL_11;
        }
        if ( v6 >= v36 )
        {
          v13 = a1[13];
          goto LABEL_44;
        }
      }
      v6 = v36;
      v7 = v37;
      v13 = a1[13];
      goto LABEL_44;
    }
    v12 = (u32 *)v12[8];
LABEL_12:
    if ( ++v11 >= v13 || v12 == NULL )
      break;
  }
  v17 = v7;
  if ( v9 == 0 )
  {
LABEL_15:
    v6 = 0xFFFFFFFF;
    v17 = -1;
  }
  v31 = v17;
  v32 = v6;
LABEL_17:
  v19 = *(u32 *)(a3 + 56);
  v18 = mp4_sample_fn_12(a2[135], *(u32 *)(a3 + 20), &v33);
  v20 = NULL;
  v10 = 8194;
  if ( v18 == 0 )
  {
    v21 = 0;
    mp4_track_fn_70(v33, &v34);
    v22 = *(u32 *)(a3 + 52);
    v10 = 0;
    if ( v22 != 0 )
    {
      while ( 2 )
      {
        v38 = v31;
        v39 = v32;
        if ( (*(u8 *)(v19 + 11) & 1) != 0 )
        {
          v30 = mp4_track_fn_85(&v38, &v39, *(u32 *)(v19 + 16));
          v22 = *(u32 *)(a3 + 52);
          if ( v30 == 0 )
          {
            v25 = v39;
            v24 = v38;
LABEL_24:
            v26 = v24;
LABEL_25:
            *(u32 *)(v19 + 40) = v25;
            *(u32 *)(v19 + 44) = v26;
            goto LABEL_26;
          }
LABEL_34:
          *(u32 *)(v19 + 40) = -1;
          *(u32 *)(v19 + 44) = -1;
          ++v21;
        }
        else
        {
          if ( v21 == 0 )
          {
            v24 = v31;
            v25 = v32;
            goto LABEL_24;
          }
          v23 = v20[3];
          if ( v23 == 0 )
          {
            v25 = v20[10];
            v26 = v20[11];
            goto LABEL_25;
          }
          if ( mp4_track_fn_46(v34, a3, (int)v20, 0, v23, &v40) != 0 )
          {
            v22 = *(u32 *)(a3 + 52);
            goto LABEL_34;
          }
          v28 = v20[11];
          v41 = v20[10];
          v42 = v28;
          v29 = mp4_track_fn_86(&v41, v40);
          v22 = *(u32 *)(a3 + 52);
          if ( v29 == 0 )
          {
            v25 = v41;
            v26 = v42;
            goto LABEL_25;
          }
LABEL_26:
          ++v21;
        }
        v20 = (u32 *)v19;
        v19 = *(u32 *)(v19 + 32);
        if ( v21 >= v22 )
          return 0;
        continue;
      }
    }
  }
  return v10;
}


int mp4_track_fn_50(int a1, unsigned int a2, int a3, int a4, u32 *a5)
{
  int v6;
  int v8;
  u32 v9[4];

  if ( a1 != 0 && *(u32 *)(a1 + 12) >= a2 )
  {
    if ( (*(u8 *)(a1 + 10) & 2) != 0 )
    {
      mp4_track_fn_44(a1, a2, (int)v9);
      v8 = v9[1];
    }
    else
    {
      v6 = 8195;
      if ( a3 == 0 )
        return v6;
      if ( (*(u8 *)(a3 + 11) & 0x10) != 0 )
      {
        v8 = *(u32 *)(a3 + 32);
      }
      else
      {
        v6 = 8195;
        if ( a4 == 0 )
          return v6;
        v8 = *(u32 *)(a4 + 24);
      }
    }
    v6 = 0;
    *a5 = v8;
    return v6;
  }
  return 4109;
}


int mp4_track_fn_51(int a1, int a2, u32 *a3)
{
  int v3;
  int result;

  if ( (*(u8 *)(a1 + 11) & 2) != 0 )
  {
    v3 = *(u32 *)(a1 + 24);
LABEL_3:
    *a3 = v3;
    return 0;
  }
  result = 8195;
  if ( a2 != 0 )
  {
    v3 = *(u32 *)(a2 + 16);
    goto LABEL_3;
  }
  return result;
}


int mp4_track_fn_52(u32 *a1, int a2, unsigned int a3, u32 *a4, u32 *a5)
{
  u32 *v10;
  int v11;
  u32 *i;
  int v13;
  unsigned int v14;
  unsigned int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  unsigned int v29;
  unsigned int v30;
  unsigned int v31;
  unsigned int v32;
  int v33;
  int v34;
  int v35;
  int v36;
  u32 v37[3];
  int v38;
  unsigned int v39;
  unsigned int v40;
  u32 *v41;
  int v42;
  int v43;
  int v44;
  int v45;
  int v46;
  u32 v47[4];

  v10 = (u32 *)a1[17];
  v40 = 0;
  v41 = NULL;
  if ( v10 == NULL )
  {
    i = (u32 *)a1[14];
LABEL_32:
    v29 = a3;
    v30 = 0;
    if ( i == NULL )
      return 4109;
    v31 = i[3];
    if ( a3 >= v31 )
    {
      v32 = a1[13];
      if ( v32 != 0 )
      {
        for ( i = (u32 *)i[8]; ; i = (u32 *)i[8] )
        {
          ++v30;
          v29 -= v31;
          if ( i == NULL )
            break;
          v31 = i[3];
          if ( v29 < v31 || v30 >= v32 )
            goto LABEL_40;
        }
        return 4109;
      }
    }
LABEL_40:
    v41 = i;
    v17 = (int)i;
    v39 = v30;
    v33 = (int)i;
    v34 = i[10];
    v40 = v29;
    if ( v34 == -1 )
    {
      v21 = 8;
      if ( *(u32 *)(v17 + 44) == -1 )
        return v21;
    }
    v35 = *(u32 *)(v33 + 44);
    v14 = v40;
    v43 = *(u32 *)(v33 + 40);
    v44 = v35;
    if ( v40 != 0 )
    {
      v36 = mp4_track_fn_46(a2, (int)a1, v33, 0, v40 - 1, &v42);
      v21 = 8;
      if ( v36 != 0 )
        return v21;
      v24 = v42;
LABEL_22:
      v25 = mp4_sample_fn_17(&v43, v24);
      v21 = 8;
      if ( v25 != 0 )
        return v21;
      v17 = (int)v41;
      v14 = v40;
      goto LABEL_10;
    }
    goto LABEL_10;
  }
  v11 = a3 - a1[19] + a1[18];
  if ( v11 < 0 )
  {
    i = (u32 *)a1[14];
    goto LABEL_32;
  }
  if ( (unsigned int)v11 >= v10[3] )
  {
    i = (u32 *)a1[14];
    goto LABEL_32;
  }
  v13 = v10[10];
  v41 = v10;
  v40 = v11;
  if ( v13 == -1 )
  {
    v21 = 8;
    if ( v10[11] == -1 )
      return v21;
  }
  v14 = v40;
  v15 = a1[18];
  if ( v40 == v15 )
  {
    v27 = a1[20];
    v28 = a1[21];
    v17 = (int)v41;
LABEL_27:
    v43 = v27;
    v44 = v28;
    goto LABEL_10;
  }
  v16 = a1[21];
  v43 = a1[20];
  v44 = v16;
  if ( v40 < v15 )
  {
    v21 = mp4_track_fn_46(a2, (int)a1, (int)v41, v40, v15 - 1, &v45);
    if ( v21 != 0 )
      return v21;
    v46 = v44;
    v47[0] = v43;
    v26 = mp4_track_fn_83(&v46, v47, 0, v45);
    v21 = 8195;
    if ( v26 != 0 )
      return v21;
    v27 = v47[0];
    v17 = (int)v41;
    v14 = v40;
    v28 = v46;
    goto LABEL_27;
  }
  v17 = (int)v41;
  if ( v15 < v40 )
  {
    v21 = mp4_track_fn_46(a2, (int)a1, (int)v41, v15, v40 - 1, &v45);
    if ( v21 != 0 )
      return v21;
    v24 = v45;
    goto LABEL_22;
  }
LABEL_10:
  v18 = v43;
  v19 = v44;
  a1[19] = a3;
  a1[20] = v18;
  a1[21] = v19;
  a1[18] = v14;
  a4[2] = v18;
  a4[3] = v19;
  a1[17] = v17;
  if ( (*(u8 *)(v17 + 10) & 8) != 0 )
  {
    memset(v37, 0, sizeof(v37));
    v38 = 0;
    v23 = mp4_track_fn_44(v17, v14, (int)v37);
    v21 = 4109;
    if ( v23 != 0 )
      return v21;
    v14 = v40;
    v17 = (int)v41;
    a4[5] = v38;
  }
  else
  {
    a4[5] = 0;
  }
  v20 = (int)(a1 + 2);
  v21 = mp4_track_fn_50(v17, v14, v20, a2, a4);
  if ( v21 == 0 )
  {
    v21 = mp4_track_fn_47((int)v41, v40, v20, a2, a4 + 1);
    if ( v21 == 0 )
    {
      v21 = mp4_track_fn_48((int)v41, v40, v20, a2, a4 + 4);
      if ( v21 == 0 )
        return mp4_track_fn_51(v20, a2, a5);
    }
  }
  return v21;
}


int mp4_track_fn_53(int a1, int a2, int a3, int a4, u32 *a5, u32 *a6)
{
  u32 *v6;
  unsigned int v7;
  unsigned int v8;
  int v10;
  int v11;

  v6 = *(u32 **)(a1 + 264);
  v7 = 1;
  while ( 1 )
  {
    if ( v6 == NULL )
    {
      *a5 = 0;
      goto LABEL_9;
    }
    if ( v6[136] != a3 )
    {
      v8 = *(u32 *)(a1 + 260);
      goto LABEL_6;
    }
    if ( v6[137] == a4 )
      break;
    v8 = *(u32 *)(a1 + 260);
LABEL_6:
    if ( v8 < v7++ )
    {
      *a5 = v6;
      goto LABEL_9;
    }
    v6 = (u32 *)v6[134];
  }
  *a5 = v6;
LABEL_9:
  v10 = 8194;
  if ( v6 != NULL )
  {
    v11 = v6[141];
    v10 = 0;
    *a6 = v6[140];
    a6[1] = v11;
  }
  return v10;
}


int mp4_track_fn_54(int a1, int a2, u32 *a3)
{
  int result;
  unsigned int v4;
  int v5;
  unsigned int v6;
  int v7;
  int v8;

  result = 20495;
  if ( a1 != 0 )
  {
    v4 = *(u32 *)(a1 + 24);
    v5 = 0;
    v6 = 0;
    if ( v4 != 0 )
    {
      v7 = a1 + 28;
      do
      {
        ++v6;
        v8 = *(u32 *)(*(u32 *)v7 + 20);
        v7 += 4;
        v5 += v8 == a2;
      }
      while ( v6 < v4 );
    }
    *a3 = v5;
    return 0;
  }
  return result;
}


int mp4_track_fn_55(int a1, int a2, int a3, u32 *a4)
{
  int result;
  unsigned int v5;
  int v6;
  unsigned int v7;
  int *v8;
  int i;
  int v10;

  result = 20495;
  if ( a1 != 0 )
  {
    v5 = *(u32 *)(a1 + 24);
    v6 = 0;
    v7 = 0;
    if ( v5 != 0 )
    {
      v8 = (int *)(a1 + 28);
      for ( i = *v8; ; i = *v8 )
      {
        ++v7;
        ++v8;
        v10 = v6;
        if ( *(u32 *)(i + 20) == a2 )
        {
          ++v6;
          if ( v10 == a3 )
            break;
        }
        if ( v7 >= v5 )
          return 8194;
      }
      *a4 = i;
      return 0;
    }
    else
    {
      return 8194;
    }
  }
  return result;
}


int mp4_track_fn_56(int a1, int a2, u32 *a3)
{
  int v3;
  unsigned int v4;
  unsigned int v5;
  int *v6;
  int i;

  v3 = 0;
  if ( a1 != 0 )
  {
    v4 = *(u32 *)(a1 + 24);
    v5 = 0;
    if ( v4 != 0 )
    {
      v6 = (int *)(a1 + 28);
      for ( i = *v6; ; i = *v6 )
      {
        ++v5;
        ++v6;
        if ( *(u32 *)(i + 12) == a2 )
          v3 = *(u32 *)(i + 20);
        if ( v5 >= v4 )
          break;
      }
    }
  }
  *a3 = v3;
  return 0;
}


int mp4_track_fn_57(int a1, int a2, u32 *a3)
{
  int v6;
  unsigned int v7;
  int v8;
  unsigned int i;
  int v11;
  unsigned int v12;
  unsigned int v13;
  unsigned int v14;
  int v15;
  int v16[4];

  if ( (*(u8 *)(a1 + 19) & 8) != 0 )
    a2 = *(u32 *)(a1 + 36);
  v6 = 0;
  v7 = 0;
  if ( *(u32 *)(a1 + 52) != 0 )
  {
    for ( i = 0; ; i = v7 )
    {
      v11 = mp4_track_fn_43(a1, i);
      v12 = *(u32 *)(v11 + 12);
      if ( (*(u8 *)(v11 + 10) & 1) != 0 )
      {
        v13 = 0;
        if ( v12 != 0 )
        {
          while ( 1 )
          {
            v15 = mp4_codec_fn_91(*(u32 *)(v11 + 28), v13++, 0, v16);
            v8 = 10;
            if ( v15 != 0 )
              return v8;
            v6 += v16[0];
            if ( v13 >= v12 )
            {
              v14 = *(u32 *)(a1 + 52);
              goto LABEL_11;
            }
          }
        }
      }
      else
      {
        v6 += a2 * v12;
      }
      v14 = *(u32 *)(a1 + 52);
LABEL_11:
      if ( ++v7 >= v14 )
      {
        *a3 = v6;
        return 0;
      }
    }
  }
  *a3 = 0;
  return 0;
}


int mp4_track_fn_58(int a1, int a2, unsigned int a3, unsigned int a4, u32 *a5)
{
  int v9;
  unsigned int v10;
  int v11;
  int v12;
  unsigned int v13;
  int i;
  int v15;
  unsigned int v16;
  unsigned int v17;
  int v18;
  unsigned int v19;
  u32 *v20;
  int v22;
  int v23;
  int v24;
  u32 *v25;

  v9 = *(u8 *)(a1 + 19) & 8;
  v25 = a5;
  if ( v9 != 0 )
    a2 = *(u32 *)(a1 + 36);
  v10 = *(u32 *)(a1 + 52);
  v11 = 20499;
  if ( a3 >= v10 )
    return v11;
  v12 = 0;
  v13 = 0;
  if ( a3 == 0 )
  {
LABEL_11:
    v18 = mp4_track_fn_43(a1, a3);
    v11 = 8201;
    if ( a4 >= *(u32 *)(v18 + 12) )
      return v11;
    if ( (*(u8 *)(v18 + 10) & 1) != 0 )
    {
      v19 = 0;
      if ( a4 != 0 )
      {
        while ( 1 )
        {
          v22 = mp4_codec_fn_91(*(u32 *)(v18 + 28), v19++, 0, &v24);
          v11 = 10;
          if ( v22 != 0 )
            return v11;
          v12 += v24;
          if ( v19 >= a4 )
          {
            v20 = v25;
            goto LABEL_15;
          }
        }
      }
    }
    else
    {
      v12 += a2 * a4;
    }
    v20 = v25;
LABEL_15:
    v11 = 0;
    *v20 = v12;
    return v11;
  }
  for ( i = a1; ; i = a1 )
  {
    v15 = mp4_track_fn_43(i, v13);
    v16 = *(u32 *)(v15 + 12);
    if ( (*(u8 *)(v15 + 10) & 1) == 0 )
    {
      v12 += a2 * v16;
      goto LABEL_8;
    }
    v17 = 0;
    if ( v16 != 0 )
      break;
LABEL_8:
    ++v13;
LABEL_9:
    if ( v13 >= a3 )
      goto LABEL_11;
  }
  while ( 1 )
  {
    v23 = mp4_codec_fn_91(*(u32 *)(v15 + 28), v17++, 0, &v24);
    v11 = 10;
    if ( v23 != 0 )
      return v11;
    v12 += v24;
    if ( v17 >= v16 )
    {
      ++v13;
      goto LABEL_9;
    }
  }
}


int mp4_track_fn_59(int a1, int i, int a3)
{
  unsigned int v4;
  unsigned int v5;
  int *v6;

  v4 = *(u32 *)(a1 + 24);
  v5 = 0;
  if ( v4 != 0 )
  {
    v6 = (int *)(a1 + 28);
    for ( i = *(u32 *)(a1 + 28); ; i = *v6 )
    {
      ++v5;
      ++v6;
      if ( i != 0 )
      {
        mp4_track_fn_65(i, i, a3);
        v4 = *(u32 *)(a1 + 24);
      }
      if ( v5 >= v4 )
        break;
    }
  }
  return mp4_track_fn_65(a1, i, a3);
}


int mp4_track_fn_60(int a1, unsigned int a2, u32 *a3)
{
  int v4;
  int v5;

  if ( a1 == 0 )
    return 8194;
  v4 = mp4_track_fn_43(a1, a2);
  v5 = 8194;
  if ( v4 != 0 )
  {
    v5 = 0;
    *a3 = *(u32 *)(v4 + 12);
  }
  return v5;
}


int mp4_track_fn_61(int a1, u32 *a2)
{
  int v2;
  int v3;

  v2 = 20495;
  if ( a1 != 0 )
  {
    v3 = *(u32 *)(a1 + 548);
    v2 = 0;
    *a2 = *(u32 *)(a1 + 544);
    a2[1] = v3;
  }
  return v2;
}


int mp4_track_fn_62(int a1, u32 *a2)
{
  int v2;

  v2 = 20495;
  if ( a1 != 0 )
  {
    v2 = 0;
    *a2 = *(u32 *)(a1 + 20);
  }
  return v2;
}


int mp4_track_fn_63(int a1, u32 *a2)
{
  int v2;

  v2 = 20495;
  if ( a1 != 0 )
  {
    v2 = 0;
    *a2 = *(u32 *)(a1 + 540);
  }
  return v2;
}


int mp4_track_fn_64(int *a1, unsigned int a2, int a3)
{
  int v4;
  int result;

  v4 = *a1;
  result = 20481;
  if ( v4 != 0 )
  {
    if ( a2 == 1836475768 )
    {
      mp4_track_fn_59(v4, 1836475768, a3);
      *a1 = 0;
      return 0;
    }
    if ( a2 > 0x6D766578 )
    {
      if ( a2 == 1953653099 )
      {
        mp4_codec_fn_133();
        *a1 = 0;
        return 0;
      }
      if ( a2 > 0x7472616B )
      {
        a3 = a2 > 0x74726578;
        if ( a2 != 1953654136 )
        {
          if ( a2 > 0x74726578 )
          {
            if ( a2 == 1953658222 )
            {
              mp4_track_fn_38(v4, 1953658222, a3);
              *a1 = 0;
            }
            else
            {
              if ( a2 != 1987338860 )
                return 20481;
              mp4_codec_fn_102(v4);
              *a1 = 0;
            }
          }
          else
          {
            if ( a2 != 1953654118 )
              return 20481;
            mp4_codec_fn_134();
            *a1 = 0;
          }
          return 0;
        }
      }
      else
      {
        if ( a2 == 1937007212 )
        {
          mp4_codec_fn_136();
          *a1 = 0;
          return 0;
        }
        if ( a2 <= 0x7374626C )
        {
          if ( a2 != 1936549485 )
            return 20481;
          mp4_codec_fn_99();
          *a1 = 0;
          return 0;
        }
        if ( a2 != 1937337955 )
        {
          if ( a2 != 1953653094 )
            return 20481;
          mp4_track_fn_37(v4);
          *a1 = 0;
          return 0;
        }
      }
    }
    else
    {
      a3 = 1718878208;
      if ( a2 != 1718909296 )
      {
        if ( a2 <= 0x66747970 )
        {
          switch ( a2 )
          {
            case 0x61766343u:
              mp4_codec_fn_132();
              *a1 = 0;
              break;
            case 0x6472656Eu:
              mp4_codec_fn_135();
              *a1 = 0;
              break;
            case 0x65736473u:
              mp4_codec_fn_105();
              *a1 = 0;
              break;
            default:
              return 20481;
          }
          return 0;
        }
        if ( a2 == 1836019558 )
        {
          mp4_track_fn_36(v4, 1836019558, 1718878208);
          *a1 = 0;
          return 0;
        }
        a3 = 1836056576;
        if ( a2 <= 0x6D6F6F66 )
        {
          if ( a2 != 1768907891 )
            return 20481;
          mp4_codec_fn_101();
          *a1 = 0;
          return 0;
        }
        if ( a2 == 1836070006 )
        {
          mp4_codec_fn_103();
          *a1 = 0;
          return 0;
        }
        if ( a2 != 1836085092 )
          return 20481;
      }
    }
    mp4_track_fn_65(*a1, a2, a3);
    *a1 = 0;
    return 0;
  }
  return result;
}


int mp4_track_fn_65(int a1)
{
  int v1;

  v1 = 20481;
  if ( a1 != 0 )
  {
    mp4_box_fn_9();
    return 0;
  }
  return v1;
}


int mp4_track_fn_66(u32 *a1, int a2)
{
  int v3;
  unsigned int v5;
  unsigned int v6;
  unsigned int v7;
  int v8;
  int v10;
  int v11;
  unsigned int v12;
  unsigned int v13;
  int (*v14)(int, int *);
  int v15;
  int v16;
  unsigned int v17;
  int v18;

  v3 = 4;
  if ( a1 != NULL )
  {
    v3 = 8195;
    if ( a1[3] != 0 )
    {
      v5 = a1[2];
      v6 = 0;
      if ( v5 != 0 )
      {
        v17 = 0;
        while ( 1 )
        {
          v18 = mp4_track_fn_69(a1, v17);
          ++v6;
          v3 = 8195;
          if ( *(u32 *)(v18 + 12) == 0 )
            break;
          v5 = a1[2];
          v17 = v6;
          if ( v6 >= v5 )
            goto LABEL_4;
        }
      }
      else
      {
LABEL_4:
        v7 = 0;
        if ( v5 != 0 )
        {
          v8 = 0;
          while ( *(u32 *)(mp4_track_fn_69(a1, v8) + 32) != a2 )
          {
            v8 = ++v7;
            if ( v7 >= a1[2] )
              return 8194;
          }
          v10 = mp4_track_fn_69(a1, v7);
          if ( v7 != 0 )
          {
            v16 = mp4_track_fn_69(a1, v7 - 1);
            *(u32 *)(v16 + 8) = *(u32 *)(mp4_track_fn_69(a1, v7) + 8);
          }
          else
          {
            a1[32] = *(u32 *)(mp4_track_fn_69(a1, 0) + 8);
          }
          mp4_codec_fn_133(v10);
          v11 = a1[2];
          v12 = 0;
          v13 = 0;
          a1[2] = v11 - 1;
          if ( v11 == 1 )
          {
            v14 = mp4_box_fn_1;
          }
          else
          {
            v15 = 0;
            do
            {
              if ( *(u32 *)(mp4_track_fn_69(a1, v15) + 12) != 0 && v12 < *(u32 *)(mp4_track_fn_69(a1, v13) + 32) )
                v12 = *(u32 *)(mp4_track_fn_69(a1, v13) + 32);
              v15 = ++v13;
            }
            while ( v13 < a1[2] );
            v14 = (int (*)(int, int *))(v12 + 1);
          }
          a1[29] = v14;
          mp4_track_fn_77(a1);
          return 0;
        }
        else
        {
          return 8194;
        }
      }
    }
  }
  return v3;
}


int mp4_track_fn_67(int a1, unsigned int a2)
{
  int v2;
  int v5;
  int v7;

  v2 = 4;
  if ( a1 != 0 )
  {
    v2 = 8194;
    if ( a2 < *(u32 *)(a1 + 8) )
    {
      v5 = mp4_track_fn_69(a1, a2);
      if ( a2 != 0 )
      {
        v7 = mp4_track_fn_69(a1, a2 - 1);
        *(u32 *)(v7 + 8) = *(u32 *)(mp4_track_fn_69(a1, a2) + 8);
      }
      else
      {
        *(u32 *)(a1 + 128) = *(u32 *)(mp4_track_fn_69(a1, 0) + 8);
      }
      mp4_codec_fn_133(v5);
      v2 = 0;
      --*(u32 *)(a1 + 8);
    }
  }
  return v2;
}


int mp4_track_fn_68(u32 *a1, u32 *a2)
{
  int v4;
  unsigned int v5;
  int j;
  int v7;
  unsigned int v8;
  unsigned int v9;
  int v10;
  int v11;
  unsigned int v12;
  int v13;
  int (*v14)(int, int *);
  int m;
  int v17;
  unsigned int k;
  int v19;
  int v20;
  int i;
  int v22;

  v4 = 0;
  v5 = a1[2];
  j = a2[8];
  if ( v5 != 0 )
  {
    for ( i = 0; ; i = v4 )
    {
      if ( mp4_track_fn_69(a1, i) != 0 )
      {
        v22 = mp4_track_fn_69(a1, v4);
        v5 = a1[2];
        if ( *(u32 *)(v22 + 32) == j )
        {
          j = 0;
          v4 = a1[2];
        }
      }
      else
      {
        v5 = a1[2];
      }
      if ( ++v4 >= v5 )
        break;
    }
  }
  if ( j != 0 )
  {
    a2[8] = j;
  }
  else
  {
    v7 = 1;
    if ( v5 != -1 )
    {
      for ( j = 1; ; j = v7 )
      {
        v8 = 0;
        if ( v5 != 0 )
        {
          for ( k = 0; ; k = v8 )
          {
            v19 = mp4_track_fn_69(a1, k);
            v20 = v8++;
            if ( v19 != 0 && *(u32 *)(mp4_track_fn_69(a1, v20) + 32) == v7 )
              j = 0;
            v5 = a1[2];
            if ( v8 >= v5 )
              break;
          }
          v9 = v5 + 1;
        }
        else
        {
          v9 = 1;
        }
        if ( j != 0 )
          v7 = v9;
        if ( v9 < ++v7 )
          break;
      }
    }
    a2[8] = j;
  }
  v10 = a1[2];
  if ( v10 != 0 )
  {
    v17 = mp4_track_fn_69(a1, v10 - 1);
    v11 = 20481;
    if ( v17 == 0 )
      return v11;
    if ( *(u32 *)(mp4_track_fn_69(a1, a1[2] - 1) + 8) != 0 )
      return 20481;
    *(u32 *)(mp4_track_fn_69(a1, a1[2] - 1) + 8) = a2;
    v10 = a1[2];
  }
  else
  {
    v11 = 20481;
    if ( a1[32] != 0 )
      return v11;
    a1[32] = a2;
  }
  a2[2] = 0;
  v12 = 0;
  v13 = 0;
  a1[2] = v10 + 1;
  if ( v10 == -1 )
  {
    v14 = mp4_box_fn_1;
  }
  else
  {
    for ( m = 0; ; m = v13 )
    {
      if ( mp4_track_fn_69(a1, m) != 0 && v12 < *(u32 *)(mp4_track_fn_69(a1, v13) + 32) )
        v12 = *(u32 *)(mp4_track_fn_69(a1, v13) + 32);
      if ( (unsigned int)++v13 >= a1[2] )
        break;
    }
    v14 = (int (*)(int, int *))(v12 + 1);
  }
  a1[29] = v14;
  if ( v14 == NULL )
    a1[29] = -1;
  a2[175] = a1;
  return 0;
}


int mp4_track_fn_69(int a1, unsigned int a2)
{
  int v2;
  int v3;
  int i;

  v2 = 0;
  if ( a2 < *(u32 *)(a1 + 8) )
  {
    v3 = *(u32 *)(a1 + 128);
    if ( a2 != 0 )
    {
      for ( i = *(u32 *)(v3 + 8); ; i = *(u32 *)(i + 8) )
      {
        --a2;
        v2 = 0;
        v3 = i;
        if ( i == 0 )
          break;
        if ( a2 == 0 )
          return v3;
      }
    }
    else
    {
      return v3;
    }
  }
  return v2;
}


int mp4_track_fn_70(int a1, u32 *a2)
{
  int v2;
  int v3;
  int v4;
  unsigned int v5;
  unsigned int v6;
  int *v7;
  int result;

  v2 = *(u32 *)(a1 + 32);
  v3 = *(u32 *)(*(u32 *)(a1 + 700) + 136);
  v4 = 0;
  if ( v3 != 0 )
  {
    v5 = *(u32 *)(v3 + 24);
    v6 = 0;
    if ( v5 != 0 )
    {
      v7 = (int *)(v3 + 28);
      do
      {
        v4 = *v7;
        ++v6;
        if ( *v7++ == 0 )
          break;
        result = 20481;
        if ( *(u32 *)(v4 + 12) == v2 )
          goto LABEL_8;
        v4 = 0;
      }
      while ( v6 < v5 );
    }
  }
  result = 20481;
LABEL_8:
  if ( v4 != 0 )
    result = 0;
  *a2 = v4;
  return result;
}


int mp4_track_fn_71(int a1, unsigned int a2, u32 *a3, u32 *a4)
{
  int v4;
  unsigned int v9;
  int v10;
  unsigned int v11;
  u32 v13[4];

  v4 = 20495;
  if ( a1 != 0 )
  {
    v9 = 0;
    v10 = 0;
    if ( *(u32 *)(a1 + 12) != 0 )
    {
      while ( 1 )
      {
        if ( a1 != 0 )
          mp4_codec_fn_91(*(u32 *)(a1 + 16), v10, 0, v13);
        v9 += v13[0];
        if ( a2 < v9 )
          break;
        if ( (unsigned int)++v10 >= *(u32 *)(a1 + 12) )
          return 20481;
      }
      v11 = v9 - v13[0];
      v4 = 0;
      *a3 = v10;
      *a4 = a2 - v11;
    }
    else
    {
      return 20481;
    }
  }
  return v4;
}


int mp4_track_fn_72(int a1, int a2, u32 *a3)
{
  int v3;
  int v4;
  unsigned int v5;
  unsigned int v6;
  int i;

  v3 = *(u32 *)(a2 + 16);
  v4 = 8194;
  if ( v3 != 0 )
  {
    v5 = a1 - 1;
    v6 = 0;
    if ( a1 != 1 )
    {
      for ( i = *(u32 *)(v3 + 20); ; i = *(u32 *)(i + 20) )
      {
        ++v6;
        v3 = i;
        v4 = 8194;
        if ( i == 0 )
          break;
        if ( v6 >= v5 )
          goto LABEL_7;
      }
    }
    else
    {
LABEL_7:
      *a3 = v3;
      return 0;
    }
  }
  return v4;
}


int mp4_track_fn_73(u32 *a1, int a2, u32 *a3)
{
  int v3;
  unsigned int v4;
  int v6;
  unsigned int v7;
  int i;

  v3 = 4;
  if ( a1 != NULL )
  {
    if ( *a1 != 0 )
    {
      v4 = a2 - 1;
      v6 = a1[4];
      v7 = 0;
      if ( a2 != 1 )
      {
        for ( i = *(u32 *)(v6 + 32); ; i = *(u32 *)(i + 32) )
        {
          ++v7;
          v6 = i;
          v3 = 8194;
          if ( i == 0 )
            break;
          if ( v7 >= v4 )
            goto LABEL_11;
        }
      }
      else
      {
LABEL_11:
        *a3 = v6;
        return 0;
      }
    }
    else
    {
      return 4;
    }
  }
  return v3;
}


int mp4_track_fn_74(u32 *a1, unsigned int a2, int a3, u32 *a4)
{
  int v6;
  unsigned int v7;
  int v8;
  unsigned int v9;
  int v11;
  int v12;
  unsigned int i;
  unsigned int v14;
  int v15; // dc
  unsigned int v16;
  int v17;
  int v18;
  int v19;
  unsigned int v20;
  unsigned int v21;
  int v22;
  int v23;
  u8 v24[4];
  int v25;
  u8 v26[16];
  unsigned int v27;
  int v28;
  int v29;
  int v30;
  u32 v31[4];

  v6 = a1[175];
  v7 = a2;
  if ( v6 == 0 )
    return 8195;
  v8 = 8195;
  if ( *(u32 *)(v6 + 12) != 0 && a1[40] != 0 )
  {
    v8 = 8196;
    if ( a2 != 0 || a3 != 0 )
    {
      if ( *(u32 *)(v6 + 136) != 0 )
      {
        if ( a1[33] == 0 )
        {
          v9 = a2;
          v29 = 0;
LABEL_25:
          mp4_track_fn_80(v9, a1[45], &v22, &v23);
          mp4_track_fn_81(v22, v23, a1[26], v24, &v25, v26);
          v8 = 0;
          *a4 = v29 + v25;
          return v8;
        }
        if ( a3 == 0 )
        {
          if ( a2 == 0 )
            return v8;
          --a2;
        }
        if ( mp4_track_fn_71((int)(a1 + 33), a2, &v27, &v28) != 0 )
        {
          v9 = v7 - a1[10];
          v29 = a1[46];
          goto LABEL_25;
        }
        if ( a1 == (u32 *)-132 || v27 >= a1[36] )
          return 8195;
        v11 = mp4_codec_fn_91(a1[37], v27, 1, &v29);
        v8 = 8195;
        if ( v11 == 0 )
        {
          v8 = 8199;
          if ( v29 != -1 )
          {
            v12 = 0;
            for ( i = 0; i < v27; v12 += v30 )
            {
              v14 = i;
              v15 = i++ >= a1[36];
              if ( v15 || mp4_codec_fn_91(a1[37], v14, 0, &v30) != 0 )
                return 8195;
            }
LABEL_24:
            v9 = v7 - v12;
            goto LABEL_25;
          }
        }
      }
      else if ( a1[33] != 0 )
      {
        v17 = (int)(a1 + 33);
        if ( a3 == 0 )
        {
          v8 = 8196;
          if ( a2 == 0 )
            return v8;
          v17 = (int)(a1 + 33);
          --a2;
        }
        v18 = mp4_track_fn_71(v17, a2, &v27, v31);
        v8 = 8196;
        if ( v18 != 0 )
          return v8;
        if ( v17 == 0 )
          return 8195;
        v8 = 8195;
        if ( v27 < *(u32 *)(v17 + 12) )
        {
          v19 = mp4_codec_fn_91(*(u32 *)(v17 + 16), v27, 1, &v29);
          v8 = 8195;
          if ( v19 == 0 )
          {
            v8 = 8199;
            if ( v29 != -1 )
            {
              v12 = 0;
              v20 = 0;
              if ( v27 != 0 )
              {
                while ( 1 )
                {
                  v21 = v20;
                  v15 = v20++ >= *(u32 *)(v17 + 12);
                  if ( v15 || mp4_codec_fn_91(*(u32 *)(v17 + 16), v21, 0, &v30) != 0 )
                    return 8195;
                  v12 += v30;
                  if ( v20 >= v27 )
                  {
                    v9 = v7 - v12;
                    goto LABEL_25;
                  }
                }
              }
              goto LABEL_24;
            }
          }
        }
      }
      else
      {
        v16 = a1[10];
        v8 = 8196;
        if ( v16 >= a2 )
        {
          v9 = a2;
          if ( a2 < v16 || a3 != 1 )
          {
            v29 = 0;
            goto LABEL_25;
          }
        }
      }
    }
  }
  return v8;
}


int mp4_track_fn_75(u32 *a1, unsigned int a2, int a3, int a4)
{
  int result;
  int v7;
  unsigned int *v8;
  unsigned int *v9;
  int v10;
  unsigned int *v11;
  unsigned int v12;
  unsigned int *v13;
  int v14;
  int v15;
  unsigned int v16;
  unsigned int v17;
  int v18;
  unsigned int v19;
  int v20;
  u32 *v21;
  unsigned int v22;
  unsigned int v23;
  int v24;
  unsigned int v25;
  unsigned int v26;
  int v27[3];

  result = 20481;
  v7 = *a1;
  v26 = a2;
  v27[0] = a3;
  if ( v7 != 0 )
  {
    v8 = (unsigned int *)mp4_codec_fn_92(v7, &v26);
    result = 20481;
    if ( v8 != NULL )
    {
      v9 = (unsigned int *)mp4_codec_fn_92(v7, v27);
      result = 20481;
      if ( v9 != NULL )
      {
        v10 = v27[0];
        if ( v8 == v9 )
        {
          v22 = *v8;
          v23 = 0;
          v24 = v27[0];
          if ( (*v8 - v27[0]) * a4 != a4 )
          {
            v25 = v8[4];
            do
            {
              *(u32 *)(4 * (v26 * a4 + v23) + v25) = *(u32 *)(4 * v23 + (4 * v24 + 4) * a4 + v25);
              ++v23;
              v10 = v27[0];
              v22 = *v8;
              v24 = v27[0];
            }
            while ( v23 < (*v8 - v27[0] - 1) * a4 );
          }
          v16 = v22 + v26 - v10 - 1;
          *v8 = v16;
        }
        else
        {
          v11 = (unsigned int *)v8[3];
          if ( v11 == v9 )
          {
            v12 = v26;
          }
          else
          {
            do
            {
              v13 = (unsigned int *)v11[3];
              if ( v11[4] != 0 )
                mp4_track_fn_65(v11[4]);
              v14 = v11[5];
              if ( v14 != 0 )
                mp4_track_fn_65(v14);
              v15 = v11[6];
              if ( v15 != 0 )
                mp4_track_fn_65(v15);
              mp4_track_fn_65((int)v11);
              v11 = v13;
            }
            while ( v13 != v9 );
            v12 = v26;
            v10 = v27[0];
          }
          v8[3] = (unsigned int)v9;
          v16 = 0;
          *v8 = v12;
          v17 = *v9;
          v18 = v10;
          if ( (*v9 - v10) * a4 != a4 )
          {
            v19 = v9[4];
            do
            {
              v20 = 4 * v16;
              v21 = (u32 *)(4 * v16++ + v19);
              *v21 = *(u32 *)(v20 + (4 * v18 + 4) * a4 + v19);
              v10 = v27[0];
              v17 = *v9;
              v18 = v27[0];
            }
            while ( v16 < (*v9 - v27[0] - 1) * a4 );
          }
          *v9 = v17 - v10 - 1;
        }
        *a1 = mp4_codec_fn_93(*a1, v16);
        return 0;
      }
    }
  }
  return result;
}


int mp4_track_fn_76(u32 *a1, int a2, int a3, int a4)
{
  int result;
  int v7;
  int *v8;
  u32 *v9;
  int *v10;
  int v11;
  int *v12;
  int v13;
  int *v14;
  int v15;
  int v16;
  int v17;
  unsigned int v18;
  int v19;
  unsigned int v20;
  int v21;
  u32 *v22;
  int v23;
  int v24;
  int v25;
  unsigned int v26;
  int v27;
  int v28;
  unsigned int v29;
  u32 *v30;
  u32 *v31;
  int v32;
  int v33;
  u32 v34[3];

  result = 20481;
  v7 = *a1;
  v33 = a2;
  v34[0] = a3;
  if ( v7 != 0 )
  {
    v8 = (int *)mp4_codec_fn_92(v7, &v33);
    result = 20481;
    if ( v8 != NULL )
    {
      v10 = (int *)mp4_codec_fn_92(v7, v34);
      result = 20481;
      if ( v10 != NULL )
      {
        if ( v8 == v10 )
        {
          v11 = *v8;
          v24 = v34[0];
          v25 = v11 - v34[0] - 1;
          v26 = 0;
          if ( v25 * a4 != 0 )
          {
            v27 = v33;
            v28 = v8[6];
            v29 = v25 * a4;
            v30 = (u32 *)(8 * v33 * a4 + v28);
            v31 = (u32 *)((8 * v34[0] + 8) * a4 + v28);
            do
            {
              v32 = v31[1];
              ++v26;
              *v30 = *v31;
              v30[1] = v32;
              v31 += 2;
              v30 += 2;
            }
            while ( v26 < v29 );
          }
          else
          {
            v27 = v33;
          }
          v9 = (u32 *)(v11 + v27 - v24 - 1);
          *v8 = (int)v9;
        }
        else
        {
          v12 = (int *)v8[3];
          v13 = v33;
          if ( v12 != v10 )
          {
            do
            {
              v14 = (int *)v12[3];
              if ( v12[4] != 0 )
                mp4_track_fn_65(v12[4]);
              v15 = v12[5];
              if ( v15 != 0 )
                mp4_track_fn_65(v15);
              v16 = v12[6];
              if ( v16 != 0 )
                mp4_track_fn_65(v16);
              mp4_track_fn_65((int)v12);
              v12 = v14;
            }
            while ( v14 != v10 );
            v13 = v33;
          }
          v17 = v34[0];
          v8[3] = (int)v10;
          v18 = 0;
          *v8 = v13;
          v19 = *v10;
          v20 = (*v10 - v17 - 1) * a4;
          v21 = v17;
          if ( v20 != 0 )
          {
            v9 = (u32 *)v10[6];
            v22 = (u32 *)((char *)v9 + (8 * v17 + 8) * a4);
            do
            {
              v23 = v22[1];
              ++v18;
              *v9 = *v22;
              v9[1] = v23;
              v22 += 2;
              v9 += 2;
            }
            while ( v18 < v20 );
          }
          *v10 = v19 - v21 - 1;
        }
        *a1 = mp4_codec_fn_93(*a1, v9);
        return 0;
      }
    }
  }
  return result;
}


int mp4_track_fn_77(u32 *a1)
{
  int v1;
  unsigned int v2;
  unsigned int v4;
  unsigned int v5;

  v1 = 20495;
  v2 = 0;
  if ( a1 != NULL )
  {
    v1 = 8195;
    if ( a1[3] != 0 )
    {
      v4 = 0;
      if ( a1[2] != 0 )
      {
        v5 = 0;
        do
        {
          if ( *(u32 *)(mp4_track_fn_69((int)a1, v5) + 12) != 0
            && *(u32 *)(mp4_track_fn_69((int)a1, v4) + 200) != 0
            && *(u32 *)(mp4_track_fn_69((int)a1, v4) + 216) != 1868854125
            && v2 < *(u32 *)(mp4_track_fn_69((int)a1, v4) + 40) )
          {
            v2 = *(u32 *)(mp4_track_fn_69((int)a1, v4) + 40);
          }
          v5 = ++v4;
        }
        while ( v4 < a1[2] );
      }
      a1[9] = v2;
      return 0;
    }
  }
  return v1;
}


int mp4_track_fn_78(u32 *a1, u32 *a2, int a3)
{
  int v3;
  int v5;
  int v6;
  int v8;
  unsigned int v9;
  int v10;
  int v11;
  unsigned int v12;
  unsigned int v13;
  unsigned int v14;
  int *i;
  u32 v16[2];
  s64 v17;
  int v18;
  int v19;
  int v20;
  int v21;
  u32 *v22;
  s64 v23;
  u32 v24[4];

  v3 = 8195;
  if ( a1[40] != 0 )
  {
    v5 = a1[175];
    if ( v5 != 0 )
    {
      if ( *(u32 *)(v5 + 136) == 0 )
      {
        v3 = 0;
        *a2 = a1[46];
        return v3;
      }
      v6 = *(u32 *)(v5 + 152);
    }
    else
    {
      v6 = 0x98;
    }
    v8 = *(u32 *)(v6 + 276);
    if ( *(u32 *)(v6 + 280) == 0 )
      return mp4_codec_fn_25(a1 + 98, a2, a3);
    v3 = 8194;
    if ( a1 != NULL )
    {
      v9 = a1[8];
      v3 = mp4_track_fn_2(*(u32 *)(v6 + 280), v9, &v21, v16);
      if ( v3 == 0 )
      {
        v3 = 2;
        if ( v21 != 0 )
        {
          v10 = v16[1];
          v3 = mp4_track_fn_29(*(u32 *)v8, *(int **)(v8 + 4), v17, &v22, (int)&v23);
          if ( v3 == 0 )
          {
            v3 = mp4_track_fn_32(v22, v9, *(u32 *)(*(u32 *)v8 + 136), v18 - 1, v19 - 1, v20 - 1, v24);
            if ( v3 == 0 )
            {
              v11 = v10 + v24[0];
              v13 = mp4_box_fn_4(**(u32 **)(v8 + 4));
              v14 = v12;
              if ( v13 != -1 || (v3 = 2, v12 != -1) )
              {
                if ( HIDWORD(v23) >= v12 )
                {
                  if ( HIDWORD(v23) != v12 )
                  {
LABEL_17:
                    *a2 = v11;
                    return 0;
                  }
                  if ( (unsigned int)v23 >= v13 )
                  {
                    *a2 = v11;
                    return 0;
                  }
                }
LABEL_21:
                for ( i = *(int **)(v8 + 4); ; i = *(int **)(v8 + 4) )
                {
                  v3 = mp4_track_fn_29(*(u32 *)v8, i, v23, &v22, (int)&v23);
                  if ( v3 != 0 )
                    break;
                  v3 = mp4_track_fn_32(v22, v9, *(u32 *)(*(u32 *)v8 + 136), 0, 0, 0, v24);
                  if ( v3 != 0 )
                    break;
                  v11 += v24[0];
                  if ( HIDWORD(v23) < v14 )
                    goto LABEL_21;
                  if ( HIDWORD(v23) != v14 )
                    goto LABEL_17;
                  if ( (unsigned int)v23 >= v13 )
                  {
                    *a2 = v11;
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
  return v3;
}


int mp4_track_fn_79(u32 *a1, unsigned int *a2, int a3)
{
  int v3;
  int v7;
  unsigned int v8;
  int v10;
  u32 *v11;
  int v12;
  unsigned int v13;
  unsigned int *v14;
  u32 *v15;
  int v16;
  int v17;
  int v18;
  unsigned int v19;
  int v20;
  int v21;
  unsigned int v22;
  int v23;
  unsigned int v24;
  unsigned int *v25;
  unsigned int j;
  int v27;
  int v28;
  int v29; // dc
  unsigned int v30;
  unsigned int *v31;
  unsigned int k;
  int v33;
  unsigned int v34;
  unsigned int v35;
  unsigned int *v36;
  unsigned int i;
  int v38;
  int v39;
  int v40;
  u8 v41[4];
  int v42;
  unsigned int v43;
  unsigned int *v44;

  v3 = 8195;
  if ( a1[3] == 0 )
    return v3;
  v7 = a1[34];
  if ( v7 == 0 )
  {
    v8 = a1[9];
LABEL_4:
    v3 = 0;
    *a2 = v8;
    return v3;
  }
  if ( *(u32 *)(v7 + 8) != 0 )
  {
    v8 = *(u32 *)(v7 + 20);
    goto LABEL_4;
  }
  v10 = a1[2];
  v3 = 0;
  if ( v10 == 0 )
  {
    *a2 = 0;
    return v3;
  }
  v11 = (u32 *)mp4_box_fn_8(4 * v10);
  v3 = 1;
  if ( v11 == NULL )
    return v3;
  v44 = (unsigned int *)mp4_box_fn_8(4 * a1[2]);
  if ( v44 == NULL )
  {
    mp4_track_fn_65((int)v11);
    return 1;
  }
  v12 = a1[2];
  v43 = 0;
  v13 = 0;
  if ( v12 != 0 )
  {
    v14 = v44;
    v15 = v11;
    while ( 1 )
    {
      v16 = mp4_track_fn_69((int)a1, v13);
      if ( v16 == 0 )
        goto LABEL_19;
      v17 = mp4_codec_fn_25(v16 + 392, &v42, 0);
      v18 = v17;
      if ( v17 != 0 )
        break;
      mp4_track_fn_80(v42, a1[8], &v38, &v39);
      mp4_track_fn_81(v38, v39, *(u32 *)(v16 + 180), &v40, v15, v41);
      if ( v40 != 0 )
      {
LABEL_46:
        mp4_track_fn_65((int)v11);
        mp4_track_fn_65((int)v44);
        return 4121;
      }
      v19 = a1[2];
LABEL_20:
      ++v13;
      ++v15;
      if ( v13 >= v19 )
        goto LABEL_21;
    }
    ++v43;
    if ( v17 != 2 )
      goto LABEL_47;
    *v14++ = v13;
LABEL_19:
    v19 = a1[2];
    goto LABEL_20;
  }
LABEL_21:
  v20 = a3 == 0;
  v21 = v43 != 0;
  if ( v21 && v20 )
  {
    mp4_track_fn_65((int)v11);
    mp4_track_fn_65((int)v44);
    return 2;
  }
  if ( v43 == 0 )
  {
    v22 = *v11;
LABEL_39:
    v34 = 1;
    *a2 = v22;
    v35 = a1[2];
    if ( v35 > 1 )
    {
      v36 = v11 + 1;
      for ( i = v11[1]; ; i = *v36 )
      {
        ++v34;
        ++v36;
        if ( *a2 < i )
        {
          *a2 = i;
          v35 = a1[2];
        }
        if ( v34 >= v35 )
          break;
      }
    }
    mp4_track_fn_65((int)v11);
    mp4_track_fn_65((int)v44);
    return 0;
  }
  v23 = 1;
LABEL_25:
  v24 = 0;
  if ( !v21 )
  {
LABEL_38:
    v22 = *v11;
    goto LABEL_39;
  }
  v25 = v44;
  for ( j = *v44; ; j = *v25 )
  {
    ++v24;
    v27 = mp4_track_fn_69((int)a1, j);
    v28 = mp4_codec_fn_36(v27 + 392);
    v18 = v28;
    if ( v28 == 0 )
      v23 = 0;
    ++v25;
    if ( v28 != 0 && v28 != 0x5010 )
      break;
    if ( v24 >= v43 )
    {
      v29 = v23 == 0;
      v23 = 1;
      if ( v29 )
        goto LABEL_25;
      v30 = 0;
      v31 = v44;
      for ( k = *v44; ; k = *v31 )
      {
        ++v30;
        v33 = mp4_track_fn_69((int)a1, k);
        v18 = mp4_codec_fn_25(v33 + 392, &v42, 0);
        if ( v18 != 0 )
          break;
        mp4_track_fn_80(v42, a1[8], &v38, &v39);
        mp4_track_fn_81(v38, v39, *(u32 *)(v33 + 180), &v40, &v11[*v31++], v41);
        if ( v40 != 0 )
          goto LABEL_46;
        if ( v30 >= v43 )
          goto LABEL_38;
      }
      break;
    }
  }
LABEL_47:
  mp4_track_fn_65((int)v11);
  mp4_track_fn_65((int)v44);
  return v18;
}


int mp4_track_fn_80(unsigned int a1, unsigned int a2, u32 *a3, u32 *a4)
{
  int v5;
  unsigned int v6;
  unsigned int v7;
  unsigned int v8;

  v5 = (u16)(a1 * a2);
  v6 = (((u16)a1 * (unsigned int)(u16)a2) >> 16)
     + (u16)(HIWORD(a1) * a2)
     + (u16)(a1 * HIWORD(a2));
  v7 = HIWORD(a1) * HIWORD(a2);
  v8 = ((unsigned int)(HIWORD(a1) * (u16)a2) >> 16)
     + ((unsigned int)((u16)a1 * HIWORD(a2)) >> 16)
     + (u16)(HIWORD(a1) * HIWORD(a2));
  *a3 = ((HIWORD(v7) + ((v8 + HIWORD(v6)) >> 16)) << 16) + (u16)(v8 + HIWORD(v6));
  *a4 = (v6 << 16) + v5;
  return 0;
}


int mp4_track_fn_81(unsigned int a1, unsigned int a2, unsigned int a3, u32 *a4, u32 *a5, unsigned int *a6)
{
  unsigned int v8;
  unsigned int *v10;
  unsigned int v11;
  int v12;
  unsigned int v13;
  unsigned int v14;
  int v15;
  unsigned int i;
  int v17;
  unsigned int v18;
  unsigned int v19;
  int v20;
  int v21;
  u32 *v22;
  u32 *v23;
  int v24;
  int v25;
  int v26;
  unsigned int v28;
  unsigned int v29;
  int v30;
  unsigned int v31;
  u32 v32[3];
  int v33;
  u32 v34[3];
  unsigned int v35;
  unsigned int v36;
  unsigned int *v37;

  v8 = 0;
  v35 = a1;
  v36 = a2;
  v37 = a6;
  v33 = 0;
  v34[0] = 0;
  if ( a3 == 0 )
  {
    v21 = 20481;
    *a6 = 0;
    *a5 = 0;
    *a4 = 0;
    return v21;
  }
  v10 = v37;
  *a4 = 0;
  v11 = 0;
  *a5 = 0;
  *v10 = 0;
  v12 = 1;
  do
  {
    if ( (a3 & v12) != 0 )
      v8 = v11;
    v12 = 1 << ++v11;
  }
  while ( v11 < 0x20 );
  while ( 1 )
  {
    v13 = 0;
    v14 = 0;
    v15 = 1;
    do
    {
      if ( (v36 & v15) != 0 )
        v13 = v14;
      v15 = 1 << ++v14;
    }
    while ( v14 < 0x20 );
    for ( i = 0; i < 0x20; ++i )
    {
      v17 = 1 << i;
      v18 = i + 32;
      if ( (v35 & v17) != 0 )
        v13 = v18;
    }
    if ( v8 >= v13 )
      break;
    v19 = v13 - v8;
    mp4_track_fn_82(a3, v13 - v8, &v31, v32);
    mp4_track_fn_82(1, v19, &v33, v34);
    if ( v35 < v31 )
    {
      v28 = v19 - 1;
LABEL_32:
      mp4_track_fn_82(a3, v28, &v31, v32);
      mp4_track_fn_82(1, v28, &v33, v34);
      v29 = v31;
      goto LABEL_27;
    }
    if ( v35 == v31 )
    {
      if ( v36 < v32[0] )
      {
        v28 = v19 - 1;
        goto LABEL_32;
      }
      v29 = v31;
    }
    else
    {
      v29 = v31;
    }
LABEL_27:
    v30 = mp4_track_fn_83(&v35, &v36, v29, v32[0]);
    v21 = 20493;
    if ( v30 != 0 )
      return v21;
    v24 = v33;
    v25 = v34[0];
    v22 = a4;
    v23 = a5;
LABEL_20:
    v26 = mp4_track_fn_84(v22, v23, v24, v25);
    v21 = 20494;
    if ( v26 != 0 )
      return v21;
  }
  if ( v13 == v8 && v36 >= a3 )
  {
    v20 = mp4_track_fn_83(&v35, &v36, 0, a3);
    v21 = 20493;
    if ( v20 != 0 )
      return v21;
    v22 = a4;
    v23 = a5;
    v24 = 0;
    v25 = 1;
    goto LABEL_20;
  }
  v21 = 0;
  *v37 = v36;
  return v21;
}


int mp4_track_fn_82(unsigned int a1, unsigned int a2, unsigned int *a3, u32 *a4)
{
  int v5;
  int v6;
  int v7;

  if ( a2 >= 0x40 )
  {
    *a3 = 0;
LABEL_3:
    *a4 = 0;
    return 0;
  }
  v5 = a2 - 1 < 0x1F;
  if ( a2 - 32 < 0x20 )
  {
    *a3 = a1 << (a2 - 32);
    goto LABEL_3;
  }
  v6 = -a2;
  v7 = a1 << a2;
  if ( v5 )
  {
    *a3 = a1 >> v6;
    *a4 = v7;
  }
  else
  {
    *a3 = 0;
    *a4 = a1;
  }
  return 0;
}


int mp4_track_fn_83(u32 *a1, u32 *a2, u64 a3)
{
  int v4;
  int v5;
  int v6;
  int v7;

  v4 = __PAIR64__(*a1, *a2) < a3;
  v5 = (__PAIR64__(*a1, *a2) - a3) >> 32;
  v6 = 20493;
  v7 = *a2 - a3;
  if ( !v4 )
  {
    *a1 = v5;
    v6 = 0;
    *a2 = v7;
  }
  return v6;
}


int mp4_track_fn_84(int *a1, u32 *a2, unsigned int a3, unsigned int a4)
{
  int v4;
  u64 v5; // kr00_8
  int v6;
  unsigned int v7;
  int v8;
  int v9;

  v4 = (u16)*a2;
  v5 = (u64)(HIWORD(*a2) + HIWORD(a4) + ((v4 + (unsigned int)(u16)a4) >> 16)) << 16;
  v6 = v5 + (u16)(v4 + a4);
  v7 = HIWORD(*a1) + HIWORD(a3) + (((u16)*a1 + (unsigned int)(u16)a3 + HIDWORD(v5)) >> 16);
  v8 = (v7 << 16) + (u16)(*a1 + a3 + WORD2(v5));
  v9 = 20494;
  if ( HIWORD(v7) == 0 )
  {
    *a1 = v8;
    v9 = 0;
    *a2 = v6;
  }
  return v9;
}


int mp4_track_fn_85(int *a1, u32 *a2, signed int _a2_0)
{
  int v3;
  int v5; // dc
  int v6;
  int v7;
  u64 a3; // 0:$a3.4,4:$a2.4

  v3 = -_a2_0;
  v5 = _a2_0 < 0;
  a3 = ((u64)(a3) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(0) << 32);
  if ( v5 )
  {
    a3 = ((u64)(a3) & 0xFFFFFFFF00000000ULL) | (u32)(v3);
    v6 = mp4_track_fn_83(a1, a2, a3);
    v7 = 20493;
  }
  else
  {
    v6 = mp4_track_fn_84(a1, a2, 0, _a2_0);
    v7 = 20494;
  }
  if ( v6 == 0 )
    return 0;
  return v7;
}


int mp4_track_fn_86(int *a1, int a2)
{
  int v3;
  unsigned int v4;
  s64 v5;
  unsigned int v6;
  s64 v7;
  int v9;

  if ( a2 < 0 )
  {
    v9 = -a2;
    if ( *(u64 *)a1 >= (u64)(unsigned int)v9 )
    {
      v7 = *(u64 *)a1 - (unsigned int)v9;
      goto LABEL_6;
    }
    return 20493;
  }
  else
  {
    v3 = *a1;
    v4 = a1[1];
    v5 = *(u64 *)a1 + (unsigned int)a2;
    if ( HIDWORD(v5) >= v4 )
    {
      if ( v4 != HIDWORD(v5) )
      {
        v6 = v3 + a2;
LABEL_5:
        v7 = ((u64)(v7) & 0xFFFFFFFF00000000ULL) | (u32)(v6);
        v7 = ((u64)(v7) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(a1[1] + (v6 < a2)) << 32);
LABEL_6:
        *(u64 *)a1 = v7;
        return 0;
      }
      v6 = v3 + a2;
      if ( (unsigned int)v5 >= *a1 )
        goto LABEL_5;
    }
    return 20494;
  }
}


