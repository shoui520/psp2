#include "mp4msv_internal.h"

int mp4_io_fn_1(unsigned int a1, unsigned int a2, u8 *a3, unsigned int *a4, int a5, int *a6)
{
  int v7;
  char v8;
  unsigned int v14;
  unsigned int v15;
  unsigned int v16;
  int v17;
  int v18;
  int v19;
  unsigned int v20;
  unsigned int v21;
  int v22;
  unsigned int v23;
  int v24;
  int *v26;
  int v27;
  u16 v28;
  int v29;

  v7 = 0;
  v8 = 0;
  *a4 = 0;
  v14 = mp4_box_fn_2(*a6);
  v16 = v15;
  v17 = false;
  if ( v14 == -1 )
    v17 = v15 == -1;
  v18 = 0;
  if ( a2 >= v16 )
  {
    if ( v16 != a2 )
    {
LABEL_5:
      v19 = v17 | v18;
      goto LABEL_6;
    }
    v19 = v17;
    if ( a1 >= v14 )
    {
LABEL_6:
      v20 = a1 - v14;
      if ( v19 != 0 )
      {
        mp4_io_fn_12(1635148611, 0x20000000, a6);
        *a3 = 0;
        return 20481;
      }
      else
      {
        v21 = ((v21) & ~0xFFU) | ((u8)(0));
        if ( *a3 != 0 )
        {
          v22 = v20 < 2;
          while ( !v22 )
          {
            v20 -= 2;
            v23 = mp4_box_fn_101(a6);
            v28 = v23;
            if ( v23 != 0 )
            {
              if ( v20 < v23 )
                break;
              v24 = mp4_sample_fn_5(v23);
              v29 = v24;
              if ( v24 == 0 )
                goto LABEL_23;
              mp4_box_fn_103(v24, v28, a6);
              v20 -= v28;
            }
            else
            {
              v29 = 0;
            }
            if ( a5 == 0 || mp4_codec_fn_43(a5, &v28) != 0 )
            {
              if ( v29 != 0 )
                mp4_track_fn_65(v29);
LABEL_23:
              v26 = a6;
              v27 = 0x40000000;
              goto LABEL_24;
            }
            v21 = (u8)(v21 + 1);
            ++v8;
            if ( (a6[1] & 0xE0000000) != 0 )
            {
              v7 = 20481;
              *a4 = v20;
              goto LABEL_19;
            }
            v22 = v20 < 2;
            if ( v21 >= (u8)*a3 )
              goto LABEL_18;
          }
          v26 = a6;
          v27 = 0x20000000;
LABEL_24:
          v7 = 20481;
          mp4_io_fn_12(1635148611, v27, v26);
          *a4 = v20;
        }
        else
        {
LABEL_18:
          *a4 = v20;
        }
LABEL_19:
        *a3 = v8;
        return v7;
      }
    }
  }
  v18 = 1;
  goto LABEL_5;
}


int mp4_io_fn_2(int a1, int a2, int *a3)
{
  u64 v6;
  unsigned int v7;
  int v8;
  int *v9;
  int v10;
  int v11;
  int v12;
  unsigned int v14;
  int v15;
  unsigned int v16;
  int v17;
  int v18;
  int v19;
  unsigned int v20;
  unsigned int v21;
  unsigned int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  unsigned int v27;
  unsigned int v28;
  unsigned int v29;
  int v30;
  unsigned int v31;
  int *v32;
  int v33;
  int v34;
  unsigned int v35;
  u32 *v36;
  u32 *v37;
  unsigned int v38;
  int v39;
  s64 v40;
  int v41;
  int v42;
  int v43;
  unsigned int v44;
  unsigned int v45;
  int v46;
  unsigned int v47;
  int v48;
  int v49;
  unsigned int v50;
  int v51;
  u32 v52[4];
  u8 v53[8];
  u64 v54;
  unsigned int v55[4];

  v6 = ((u64)(v6) & 0xFFFFFFFF00000000ULL) | (u32)(mp4_box_fn_4(*a3));
  v7 = v6 - 1;
  v54 = v6;
  if ( HIDWORD(v6) - 1 + ((u32)v6 != 0) != 0 )
  {
    v8 = HIDWORD(v54);
    goto LABEL_7;
  }
  v8 = HIDWORD(v54);
  if ( v7 >= 7 )
  {
LABEL_7:
    if ( v8 == 0 )
    {
      v12 = 0;
      if ( (unsigned int)v54 < 8 )
        return v12;
    }
    while ( 1 )
    {
      v14 = mp4_box_fn_99(a3);
      v55[0] = v14;
      v12 = 0;
      if ( v14 == 0 )
        return v12;
      if ( v14 < 8 )
        goto LABEL_4;
      v15 = mp4_box_fn_99(a3);
      v16 = v15;
      if ( v54 < v55[0] )
      {
        mp4_io_fn_12(v15, 1, a3);
        v55[0] = v54;
      }
      v17 = mp4_box_fn_2(*a3);
      v20 = v17 + v55[0];
      v21 = v20 - 8;
      v22 = v18 + (v20 < v55[0]) - 1 + (v20 - 8 < 0xFFFFFFF8);
      if ( v16 != 1718909296 )
        break;
      if ( (a2 & 1) != 0 )
      {
        v33 = 1718909296;
        if ( v55[0] < 0x10 )
          goto LABEL_46;
        if ( mp4_sample_fn_30(a1 + 236, &v54, v55, a3) == 20481 )
        {
          v26 = a3[1];
          goto LABEL_19;
        }
        --a2;
      }
      else
      {
        mp4_box_fn_24(*a3, v19, v20 - 8, v18 + (v20 < v55[0]) - 1 + (v20 - 8 < 0xFFFFFFF8));
        v33 = 1718909296;
        if ( *(u32 *)(a1 + 236) != 0 )
        {
          v34 = 8;
          goto LABEL_47;
        }
      }
LABEL_18:
      v26 = a3[1];
LABEL_19:
      v12 = 20481;
      if ( (v26 & 0xE0000000) != 0 )
        return v12;
      v12 = 0;
      if ( a2 == 0 )
        return v12;
      v27 = mp4_box_fn_2(*a3);
      if ( v28 >= v22 )
      {
        if ( v22 != v28 )
          goto LABEL_24;
        v29 = v16;
        if ( v27 >= v21 )
          goto LABEL_24;
      }
      else
      {
        v29 = v16;
      }
      mp4_io_fn_12(v29, 1, a3);
      mp4_box_fn_24(*a3, v30, v21, v22);
LABEL_24:
      v54 -= v55[0];
      if ( HIDWORD(v54) == 0 )
      {
        v12 = 0;
        if ( (unsigned int)v54 < 8 )
          return v12;
      }
    }
    if ( v16 <= 0x66747970 )
      goto LABEL_15;
    if ( v16 == 1836019574 )
    {
      v50 = v55[0];
      if ( (a2 & 0xC) == 0 )
      {
        mp4_box_fn_24(*a3, v19, v20 - 8, v22);
        if ( *(u32 *)a1 == 0 )
        {
          v26 = a3[1];
          goto LABEL_19;
        }
        v33 = 1836019574;
        goto LABEL_59;
      }
      *(u32 *)(a1 + 152) = a1;
      *(u32 *)(a1 + 4) = 1836019574;
      *(u32 *)a1 = v50;
      v51 = mp4_sample_fn_22(a1, v50 - 8, a2 & 0xC, a3);
      v12 = 20481;
      if ( v51 == 20481 )
        return v12;
      if ( *(u32 *)(a1 + 12) != 0 )
      {
        v23 = *a3;
      }
      else
      {
        if ( *(u32 *)(a1 + 168) == 0 )
        {
          mp4_codec_fn_100(a1);
          *(u32 *)a1 = 0;
          *(u32 *)(a1 + 4) = 0;
          *(u32 *)(a1 + 152) = 0;
        }
        v23 = *a3;
      }
      v24 = v21;
      v25 = v22;
      a2 &= 0xFFFFFFF3;
      goto LABEL_17;
    }
    if ( v16 != 1970628964 )
    {
LABEL_15:
      v23 = *a3;
LABEL_16:
      v24 = v21;
      v25 = v22;
LABEL_17:
      mp4_box_fn_24(v23, v19, v24, v25);
      goto LABEL_18;
    }
    v31 = 0;
    if ( v55[0] < 0x18 )
    {
      v9 = a3;
      v10 = 1970628964;
      goto LABEL_55;
    }
    v32 = v52;
    do
    {
      ++v31;
      *v32++ = mp4_box_fn_99(a3);
    }
    while ( v31 < 4 );
    if ( v52[1] != 567431118 )
    {
      v23 = *a3;
      goto LABEL_16;
    }
    if ( v52[2] != -1148688036 )
    {
      v23 = *a3;
      goto LABEL_16;
    }
    if ( v52[3] != -87439552 )
    {
      v23 = *a3;
      goto LABEL_16;
    }
    v19 = 1279852544;
    if ( v52[0] != 1279869769 )
    {
      if ( v52[0] > 0x4C494349u )
      {
        if ( v52[0] == 1297371972 )
        {
          v40 = ((u64)(v40) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(*(u32 *)(a1 + 320)) << 32);
          v40 = ((u64)(v40) & 0xFFFFFFFF00000000ULL) | (u32)(*(u32 *)(a1 + 324));
          if ( v40 != 0 )
          {
            v33 = 1970628964;
            goto LABEL_59;
          }
          v41 = mp4_box_fn_2(*a3);
          v19 = 0;
          *(u32 *)(a1 + 320) = v55[0];
          *(u32 *)(a1 + 324) = 0;
          v24 = v21;
          v23 = *a3;
          *(u32 *)(a1 + 312) = v41 - 24;
          *(u32 *)(a1 + 316) = v42 - 1 + ((unsigned int)(v41 - 24) < 0xFFFFFFE8);
          v25 = v22;
          goto LABEL_17;
        }
        if ( v52[0] != 1347571526 )
        {
          v23 = *a3;
          goto LABEL_16;
        }
        if ( (a2 & 2) == 0 )
        {
          mp4_box_fn_24(*a3, 1279852544, v21, v22);
          if ( *(u32 *)(a1 + 184) == 0 )
          {
            v26 = a3[1];
            goto LABEL_19;
          }
          v33 = 1970628964;
LABEL_59:
          v34 = 8;
LABEL_47:
          mp4_io_fn_12(v33, v34, a3);
          v26 = a3[1];
          goto LABEL_19;
        }
        if ( v55[0] < 0xC )
        {
          v33 = 1970628964;
LABEL_46:
          v34 = 0x20000000;
          goto LABEL_47;
        }
        mp4_box_fn_103((int)v53, 1, a3);
        if ( v53[0] == 0 )
        {
          v35 = 0;
          if ( v55[0] < 0x20 )
          {
            v10 = *(u32 *)(a1 + 188);
            v9 = a3;
LABEL_55:
            v11 = 0x20000000;
            goto LABEL_5;
          }
          *(u32 *)(a1 + 184) = v55[0];
          v36 = (u32 *)(a1 + 192);
          *(u32 *)(a1 + 188) = 1970628964;
          v37 = v52;
          do
          {
            ++v35;
            *v36++ = *v37++;
          }
          while ( v35 < 4 );
          *(u8 *)(a1 + 208) = v53[0];
          a2 -= 2;
          mp4_box_fn_103(a1 + 209, 3, a3);
          v38 = mp4_box_fn_99(a3);
          v39 = mp4_sample_fn_31((u32 *)(a1 + 184), v38, a3);
          a3[1] &= 0xE000000B;
          if ( v39 != 0 )
            return 20481;
          goto LABEL_18;
        }
      }
      goto LABEL_15;
    }
    if ( (a2 & 0x10) == 0 )
    {
      v23 = *a3;
      goto LABEL_16;
    }
    v43 = 8;
    if ( *(u32 *)(a1 + 288) != 0 )
    {
LABEL_70:
      mp4_io_fn_12(1279869769, v43, a3);
      v23 = *a3;
      goto LABEL_16;
    }
    v44 = v55[0];
    *(u32 *)(a1 + 292) = v52[0];
    *(u32 *)(a1 + 288) = v44;
    if ( v44 < 0x20 || (v45 = mp4_box_fn_99(a3), mp4_box_fn_99(a3), v45 < 8) || v55[0] < v45 + 24 )
    {
LABEL_69:
      v43 = 0x20000000;
      goto LABEL_70;
    }
    *(u32 *)(a1 + 296) = v45 - 8;
    v46 = mp4_sample_fn_5(v45 - 8);
    *(u32 *)(a1 + 300) = v46;
    if ( v46 != 0 )
    {
      mp4_box_fn_103(v46, v45 - 8, a3);
      v47 = mp4_box_fn_99(a3);
      mp4_box_fn_99(a3);
      if ( v47 < 8 || v55[0] < v45 + v47 + 24 )
        goto LABEL_69;
      v48 = v47 - 8;
      *(u32 *)(a1 + 304) = v48;
      v49 = mp4_sample_fn_5(v48);
      *(u32 *)(a1 + 308) = v49;
      if ( v49 != 0 )
      {
        mp4_box_fn_103(v49, v48, a3);
        v23 = *a3;
        goto LABEL_16;
      }
    }
    v43 = 0x40000000;
    goto LABEL_70;
  }
LABEL_4:
  v9 = a3;
  v10 = 1936549485;
  v11 = 1;
LABEL_5:
  mp4_io_fn_12(v10, v11, v9);
  return 0;
}


int mp4_io_fn_3(int a1, int *a2)
{
  int result;

  *(u16 *)a1 = mp4_box_fn_101(a2);
  *(u16 *)(a1 + 2) = mp4_box_fn_101(a2);
  *(u32 *)(a1 + 4) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 8) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 12) = mp4_box_fn_99(a2);
  *(u16 *)(a1 + 16) = mp4_box_fn_102(a2);
  *(u16 *)(a1 + 18) = mp4_box_fn_102(a2);
  *(u32 *)(a1 + 20) = mp4_box_fn_100(a2);
  *(u32 *)(a1 + 24) = mp4_box_fn_100(a2);
  *(u32 *)(a1 + 28) = mp4_box_fn_99(a2);
  *(u16 *)(a1 + 32) = mp4_box_fn_101(a2);
  mp4_box_fn_103(a1 + 36, 32, a2);
  *(u16 *)(a1 + 68) = mp4_box_fn_102(a2);
  result = mp4_box_fn_101(a2);
  *(u16 *)(a1 + 70) = result;
  return result;
}


int mp4_io_fn_4(int a1, unsigned int a2, int *a3)
{
  int v5;
  int result;

  v5 = a2;
  *(u16 *)(a1 + 2) = mp4_box_fn_101(a3);
  *(u32 *)(a1 + 4) = mp4_box_fn_99(a3);
  *(u16 *)(a1 + 8) = mp4_box_fn_101(a3);
  *(u16 *)(a1 + 10) = mp4_box_fn_101(a3);
  *(u16 *)(a1 + 12) = mp4_box_fn_101(a3);
  *(u16 *)(a1 + 14) = mp4_box_fn_101(a3);
  result = mp4_box_fn_99(a3);
  *(u32 *)(a1 + 16) = result;
  if ( v5 == 1 )
  {
    *(u32 *)(a1 + 20) = mp4_box_fn_99(a3);
    *(u32 *)(a1 + 24) = mp4_box_fn_99(a3);
    *(u32 *)(a1 + 28) = mp4_box_fn_99(a3);
    result = mp4_box_fn_99(a3);
    *(u32 *)(a1 + 32) = result;
  }
  return result;
}


int mp4_io_fn_5(int a1, int *a2)
{
  int result;

  *(u16 *)a1 = mp4_box_fn_101(a2);
  mp4_box_fn_101(a2);
  *(u16 *)(a1 + 2) = mp4_box_fn_101(a2);
  *(u16 *)(a1 + 4) = mp4_box_fn_101(a2);
  *(u32 *)(a1 + 8) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 12) = mp4_box_fn_99(a2);
  *(u16 *)(a1 + 16) = mp4_box_fn_101(a2);
  *(u16 *)(a1 + 18) = mp4_box_fn_101(a2);
  result = mp4_box_fn_99(a2);
  *(u8 *)(a1 + 20) = HIBYTE(result);
  return result;
}


int mp4_io_fn_6(u16 *a1, int *a2)
{
  int result;

  mp4_box_fn_103((int)a1 + 9, 3, a2);
  a1[6] = mp4_box_fn_102(a2);
  a1[7] = mp4_box_fn_102(a2);
  a1[8] = mp4_box_fn_102(a2);
  result = mp4_box_fn_102(a2);
  a1[9] = result;
  return result;
}


int mp4_io_fn_7(int a1, int *a2)
{
  int result;

  mp4_box_fn_103(a1 + 9, 3, a2);
  *(u16 *)(a1 + 12) = mp4_box_fn_102(a2);
  result = mp4_box_fn_101(a2);
  *(u16 *)(a1 + 14) = result;
  return result;
}


int mp4_io_fn_8(int a1, int *a2)
{
  return mp4_box_fn_103(a1 + 9, 3, a2);
}


int mp4_io_fn_9(int a1, int *a2)
{
  int result;

  mp4_box_fn_103(a1 + 9, 3, a2);
  *(u32 *)(a1 + 12) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 16) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 20) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 24) = mp4_box_fn_99(a2);
  *(u16 *)(a1 + 28) = mp4_box_fn_101(a2);
  result = mp4_box_fn_101(a2);
  *(u16 *)(a1 + 30) = result;
  return result;
}


int mp4_io_fn_10(int a1, int *a2)
{
  unsigned int v4;
  int v5;
  int *v6;
  unsigned int i;
  int result;

  v4 = 0;
  mp4_box_fn_103(a1 + 9, 3, a2);
  *(u32 *)(a1 + 12) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 16) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 20) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 24) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 28) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 32) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 36) = mp4_box_fn_99(a2);
  *(u16 *)(a1 + 40) = mp4_box_fn_102(a2);
  *(u16 *)(a1 + 42) = mp4_box_fn_102(a2);
  *(u16 *)(a1 + 44) = mp4_box_fn_102(a2);
  *(u16 *)(a1 + 46) = mp4_box_fn_101(a2);
  v5 = 0;
  do
  {
    v6 = (int *)(4 * (v5 + v4) + a1 + 48);
    for ( i = 0; i < 3; ++i )
      *v6++ = mp4_box_fn_100(a2);
    ++v4;
    v5 = 2 * v4;
  }
  while ( v4 < 3 );
  *(u32 *)(a1 + 84) = mp4_box_fn_99(a2);
  result = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 88) = result;
  return result;
}


int mp4_io_fn_11(int a1, int *a2)
{
  unsigned int v4;
  int v5;
  int *v6;
  unsigned int i;
  int result;

  v4 = 0;
  mp4_box_fn_103(a1 + 9, 3, a2);
  *(u32 *)(a1 + 12) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 16) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 20) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 24) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 28) = mp4_box_fn_100(a2);
  *(u16 *)(a1 + 32) = mp4_box_fn_102(a2);
  *(u16 *)(a1 + 34) = mp4_box_fn_102(a2);
  *(u32 *)(a1 + 36) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 40) = mp4_box_fn_99(a2);
  v5 = 0;
  do
  {
    v6 = (int *)(4 * (v5 + v4) + a1 + 44);
    for ( i = 0; i < 3; ++i )
      *v6++ = mp4_box_fn_100(a2);
    ++v4;
    v5 = 2 * v4;
  }
  while ( v4 < 3 );
  *(u32 *)(a1 + 80) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 84) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 88) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 92) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 96) = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 100) = mp4_box_fn_99(a2);
  result = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 104) = result;
  return result;
}


int mp4_io_fn_12(int a1, int a2, int a3)
{
  *(u32 *)(a3 + 4) |= a2;
  return 0;
}


int mp4_io_fn_13(int *a1, int a2, int a3, int a4, int a5, int a6, int *a7)
{
  int v12;
  int v13;
  int v15;

  if ( *a1 != 0 )
  {
    mp4_io_fn_12(a2, 8, (int)a7);
    mp4_box_fn_24(*a7, v15, a5, a6);
    return 20481;
  }
  else
  {
    v12 = mp4_sample_fn_5(a3);
    *a1 = v12;
    v13 = 0;
    if ( v12 == 0 )
    {
      mp4_io_fn_12(a2, 0x40000000, (int)a7);
      return 20481;
    }
  }
  return v13;
}


int mp4_io_fn_14(int a1, int a2, int a3, int a4, int a5, int a6)
{
  int v7;
  u32 *v11;
  u32 *v12;
  u32 *v13;
  u32 *v14;
  unsigned int v15;
  int v16;
  u32 *v17;

  v7 = 20481;
  if ( a1 != 0 && a6 != 0 )
  {
    v11 = (u32 *)(a1 + 328);
    if ( *(u32 *)(a1 + 328) != 0 )
    {
      v12 = *(u32 **)(a1 + 328);
      do
      {
        v11 = v12;
        v7 = 0;
        if ( v12[2] == a3 )
        {
          if ( v12[3] == a4 )
            return v7;
          v13 = (u32 *)*v12;
        }
        else
        {
          v13 = (u32 *)*v12;
        }
        v12 = v13;
      }
      while ( v13 != NULL );
    }
    v14 = (u32 *)mp4_sample_fn_5(32);
    v7 = 20481;
    if ( v14 != NULL )
    {
      v14[2] = a3;
      v14[3] = a4;
      v15 = 0;
      v14[1] = a5;
      do
      {
        v16 = v15;
        v17 = (u32 *)(4 * v15++ + a6);
        v14[v16 + 4] = *v17;
      }
      while ( v15 < 4 );
      *v11 = v14;
      return 0;
    }
  }
  return v7;
}


int mp4_io_fn_15(int *a1, int a2)
{
  int v4;
  int v5;
  int v6;
  int v8;
  int v9;
  int v10;
  u32 *v11;

  v4 = *a1;
  if ( v4 != 0 )
    mp4_track_fn_65(v4);
  v5 = mp4_sample_fn_5(464);
  *a1 = v5;
  if ( v5 != 0 )
  {
    *(u8 *)(v5 + 452) = 1;
    v9 = *a1;
    v8 = mp4_sample_fn_5(48);
    v10 = *a1;
    *(u32 *)(v9 + 460) = v8;
    v6 = 0;
    v11 = *(u32 **)(v10 + 460);
    v11[11] = a2;
    v11[10] = 0;
    v11[8] = 0;
    v11[9] = 0;
  }
  else
  {
    mp4_io_fn_12(0, 0x40000000, a2);
    return 1;
  }
  return v6;
}


int mp4_io_fn_16(u32 *a1, int a2, int a3, int a4, unsigned int a5, unsigned int a6, int *a7)
{
  int v11;
  int v12;
  int v13;
  unsigned int v14;
  unsigned int v15;
  int v16;
  int v17;
  int v19;
  int v20;
  u32 *v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29[4];

  if ( a1 == NULL )
  {
    mp4_io_fn_12(a3, 0x20000000, (int)a7);
    return 20481;
  }
  v11 = a7[5];
  if ( a3 == 1937011555 )
  {
    if ( mp4_io_fn_19(a1 + 24, a2, 1937011555, 16, 12, a6, a5, a6, a7) == 0 )
    {
      v28 = a1[28];
      v13 = 0;
      if ( v28 == 0 )
        return v13;
      if ( mp4_io_fn_17(a1 + 33, v28, 3, a7) != 0 )
      {
        v12 = a7[1];
        goto LABEL_7;
      }
      if ( mp4_io_fn_24(a1 + 24, 0, v11, *(u32 *)(a1[115] + 32), *(u32 *)(a1[115] + 40), a7) != 0 )
      {
        v12 = a7[1];
        goto LABEL_7;
      }
      v22 = mp4_api_fn_29(a1 + 62);
      goto LABEL_27;
    }
    v12 = a7[1];
    goto LABEL_30;
  }
  if ( a3 > 1937011555 )
  {
    if ( a3 == 1937011578 )
    {
      if ( mp4_io_fn_19(a1 + 48, a2, 1937011578, 20, 4, a6, a5, a6, a7) == 0 )
      {
        v13 = 0;
        if ( a1[52] == 0 )
          return v13;
        v26 = a1[115];
        *(u32 *)(v26 + 32) = mp4_api_fn_8(a1 + 48);
        if ( mp4_api_fn_28(a1 + 76) != 0 )
        {
          v27 = a1[52];
        }
        else
        {
          if ( mp4_api_fn_22(a1 + 76, *(u32 *)(a1[115] + 32)) != 0 )
          {
            v12 = a7[1];
            goto LABEL_7;
          }
          v27 = a1[52];
        }
        if ( mp4_io_fn_17(a1 + 57, v27, 1, a7) == 0 )
          mp4_io_fn_27(a1 + 48, 0, v11, 0, a1[115] + 24, a7);
        v12 = a7[1];
        goto LABEL_7;
      }
      v12 = a7[1];
    }
    else if ( a3 > 1937011578 )
    {
      if ( a3 != 1937011827 )
      {
        v12 = a7[1];
        goto LABEL_7;
      }
      if ( mp4_io_fn_19(a1 + 2, a2, 1937011827, 16, 8, a6, a5, a6, a7) == 0 )
      {
        v25 = a1[6];
        v13 = 0;
        if ( v25 == 0 )
          return v13;
        if ( mp4_io_fn_17(a1 + 11, v25, 2, a7) == 0 )
          mp4_io_fn_21(a1 + 2, 0, v11, *(u32 *)(a1[115] + 32), *(u32 *)(a1[115] + 36), a7);
        v12 = a7[1];
        goto LABEL_7;
      }
      v12 = a7[1];
    }
    else
    {
      if ( a3 != 1937011571 )
      {
        v12 = a7[1];
        goto LABEL_7;
      }
      if ( mp4_io_fn_19(a1 + 76, a2, 1937011571, 16, 4, a6, a5, a6, a7) == 0 )
      {
        v24 = a1[80];
        v13 = 0;
        if ( v24 == 0 )
          return v13;
        if ( mp4_io_fn_17(a1 + 85, v24, 1, a7) == 0 )
          mp4_io_fn_33(a1 + 76, 0, v11, *(u32 *)(a1[115] + 32), a7);
        v12 = a7[1];
        goto LABEL_7;
      }
      v12 = a7[1];
    }
LABEL_30:
    v13 = 20481;
    if ( (v12 & 0xE0000000) != 0 )
      return v13;
    goto LABEL_7;
  }
  if ( a3 != 1668576371 )
  {
    if ( a3 != 1937007471 )
    {
      v12 = a7[1];
      goto LABEL_7;
    }
    if ( mp4_io_fn_19(a1 + 62, a2, 1937007471, 16, 4, 1937007471, a5, a6, a7) != 0 )
    {
      v12 = a7[1];
      goto LABEL_30;
    }
    v13 = 0;
    if ( a1[66] == 0 )
      return v13;
    v19 = a1[115];
    *(u32 *)(v19 + 40) = mp4_api_fn_9(a1 + 62);
    if ( mp4_io_fn_17(a1 + 71, a1[66], 1, a7) != 0 )
    {
      v12 = a7[1];
      goto LABEL_7;
    }
    v20 = mp4_api_fn_27(a1 + 24);
    v21 = a1 + 24;
    if ( v20 == 0 )
    {
      if ( mp4_api_fn_21(v21, *(u32 *)(a1[115] + 40) - 1) != 0 )
      {
        v12 = a7[1];
        goto LABEL_7;
      }
      v21 = a1 + 24;
    }
    mp4_api_fn_11(v21, 0, 0, 0, v29);
    v22 = v29[0];
LABEL_27:
    if ( v22 != 0 )
      mp4_api_fn_56(a1, 0, v11, a7);
    v12 = a7[1];
    goto LABEL_7;
  }
  if ( mp4_io_fn_19(a1 + 94, a2, 1668576371, 16, 8, a6, a5, a6, a7) != 0 )
  {
    v12 = a7[1];
    goto LABEL_30;
  }
  v23 = a1[98];
  v13 = 0;
  if ( v23 == 0 )
    return v13;
  if ( mp4_io_fn_17(a1 + 103, v23, 2, a7) == 0 )
    mp4_io_fn_36(a1 + 94, 0, v11, *(u32 *)(a1[115] + 32), a7);
  v12 = a7[1];
LABEL_7:
  v13 = 20481;
  if ( (v12 & 0xE0000000) == 0 )
  {
    v14 = mp4_box_fn_2(*a7);
    if ( v15 < a6 )
    {
      v17 = *a7;
LABEL_16:
      mp4_box_fn_24(v17, v16, a5, a6);
      return 0;
    }
    if ( a6 == v15 )
    {
      if ( v14 < a5 )
      {
        v17 = *a7;
        goto LABEL_16;
      }
      return 0;
    }
    else
    {
      return 0;
    }
  }
  return v13;
}


int mp4_io_fn_17(u32 *a1, unsigned int a2, int a3, int a4)
{
  int result;
  unsigned int v8;
  unsigned int v9;
  unsigned int v10;
  int v11;
  u32 *v12;
  int v13;
  int v14;

  result = 0;
  if ( a2 != 0 )
  {
    v8 = *(u32 *)(a4 + 28);
    result = 6;
    if ( v8 != 0 )
    {
      if ( v8 >= a2 )
      {
        v10 = *(u32 *)(a4 + 16);
        if ( v10 == 0 )
          _break(0, 7u);
        v11 = (a2 - 1) / v10 + 1;
      }
      else
      {
        v9 = *(u32 *)(a4 + 16);
        if ( v9 == 0 )
          _break(0, 7u);
        v11 = v8 / v9 + 2;
      }
      result = 0;
      if ( v11 != 0 )
      {
        while ( 1 )
        {
          --v11;
          v12 = (u32 *)mp4_sample_fn_5(32);
          *a1 = v12;
          if ( v12 == NULL )
            break;
          v13 = mp4_sample_fn_6(4 * a3 * *(u32 *)(a4 + 16));
          v12[3] = v13;
          if ( v13 == 0 )
          {
            mp4_io_fn_12(0, 0x40000000, a4);
            mp4_track_fn_65((int)v12);
            *a1 = 0;
            return 1;
          }
          v14 = *(u32 *)(a4 + 16);
          a1 = v12 + 7;
          v12[2] = a3;
          v12[1] = v14;
          if ( v11 == 0 )
            return 0;
        }
        mp4_io_fn_12(0, 0x40000000, a4);
        return 1;
      }
    }
  }
  return result;
}


int mp4_io_fn_18(int *a1)
{
  int v1;
  int v3;

  v1 = *a1;
  if ( *a1 != 0 )
  {
    do
    {
      v3 = *(u32 *)(v1 + 28);
      if ( *(u32 *)(v1 + 12) != 0 )
        mp4_track_fn_65(*(u32 *)(v1 + 12));
      mp4_track_fn_65(*a1);
      *a1 = v3;
      v1 = v3;
    }
    while ( v3 != 0 );
    *a1 = 0;
  }
  return 0;
}


int mp4_io_fn_19(
        int a1,
        unsigned int a2,
        int a3,
        unsigned int a4,
        unsigned int a5,
        int a6,
        int a7,
        int a8,
        int *a9)
{
  int v16;
  int v17;
  int v18;
  unsigned int v19;
  int v20;
  unsigned int v21;
  int v22;
  int v23;
  int v24;
  s64 v25;
  s64 v26; // kr00_8
  int v27;
  int v28;
  u8 v30[16];

  if ( a2 < a4 )
  {
LABEL_25:
    v16 = a3;
    v17 = (int)a9;
    v18 = 0x20000000;
    goto LABEL_26;
  }
  mp4_box_fn_103((int)v30, 1, a9);
  v16 = a3;
  v17 = (int)a9;
  v18 = 4;
  if ( v30[0] == 0 )
  {
    v16 = a3;
    if ( *(u32 *)a1 != 0 )
    {
      v18 = 8;
      goto LABEL_26;
    }
    *(u8 *)(a1 + 8) = 0;
    *(u32 *)a1 = a2;
    *(u32 *)(a1 + 4) = a3;
    mp4_box_fn_103(a1 + 9, 3, a9);
    if ( a3 == 1937011578 )
      *(u32 *)(a1 + 12) = mp4_box_fn_99(a9);
    v19 = mp4_box_fn_99(a9);
    v20 = *(u32 *)(a1 + 12);
    *(u32 *)(a1 + 16) = v19;
    if ( v20 == 0 )
    {
      v21 = (a2 - a4) / a5;
      if ( v21 < v19 )
      {
        mp4_io_fn_12(a3, 2, (int)a9);
        *(u32 *)(a1 + 16) = v21;
      }
    }
    *(u32 *)(a1 + 24) = mp4_box_fn_2(*a9);
    *(u32 *)(a1 + 28) = v22;
    v23 = *(u32 *)(a1 + 24);
    if ( v23 == -1 && *(u32 *)(a1 + 28) == -1 )
      goto LABEL_25;
    if ( a3 == 1937011578 )
    {
      if ( *(u32 *)(a1 + 12) != 0 )
      {
        if ( v23 == a7 && *(u32 *)(a1 + 28) == a8 )
          return 0;
        v28 = (int)a9;
        v27 = 1937011578;
LABEL_15:
        mp4_io_fn_12(v27, 1, v28);
        return 0;
      }
      v24 = *(u32 *)(a1 + 16);
    }
    else
    {
      v24 = *(u32 *)(a1 + 16);
    }
    v25 = *(u64 *)(a1 + 24);
    v26 = v25 + v24 * a5;
    if ( (u32)v25 + v24 * a5 == a7 )
    {
      v27 = a3;
      if ( HIDWORD(v26) == a8 )
        return 0;
    }
    else
    {
      v27 = a3;
    }
    v28 = (int)a9;
    goto LABEL_15;
  }
LABEL_26:
  mp4_io_fn_12(v16, v18, v17);
  return 20481;
}


int mp4_io_fn_20(unsigned int a1, int a2, unsigned int a3, int a4, int a5, int *a6, int *a7)
{
  unsigned int v7;
  unsigned int v8;
  int v9;
  int v10;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;

  v7 = a1 + a2 - 1;
  v8 = a3 + a4 - 1;
  v9 = a1 >= a3;
  v10 = v8 >= v7;
  *a6 = 268435458;
  *a7 = 2;
  if ( !v9 || !v10 )
  {
    if ( a1 < a3 && v8 < v7 )
    {
      *a6 = 536870915;
      *a7 = 0;
      return 0;
    }
    v12 = 0x10000000;
    if ( v9 && v8 >= a1 )
      goto LABEL_9;
    if ( v7 >= a3 && v10 )
    {
      v12 = 0x20000000;
LABEL_9:
      v13 = v12 | 1;
      v14 = 1;
LABEL_10:
      *a7 = v14;
      *a6 = v13;
      return 0;
    }
    if ( a3 >= a1 )
    {
      if ( a3 - v7 >= a1 )
      {
        v13 = 268435458;
        v14 = 2;
        goto LABEL_10;
      }
      v16 = 0x20000000;
    }
    else
    {
      v15 = 536870914;
      if ( a5 - v7 < a1 - v8 )
      {
LABEL_15:
        *a7 = 2;
        *a6 = v15;
        return 0;
      }
      v16 = 0x10000000;
    }
    v15 = v16 | 1;
    goto LABEL_15;
  }
  *a7 = 0;
  *a6 = 0;
  return 0;
}


int mp4_io_fn_21(u32 *a1, unsigned int a2, int a3, int a4, int a5, int a6)
{
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v16;
  int v17;
  int v19;
  unsigned int v20;
  u32 *v21;
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
  int v35;
  int v36;
  int v37;
  int v38;
  int v39;
  int v40;
  int v41;
  int v42;
  int v43;

  v6 = 6;
  v7 = 0;
  v8 = 0;
  v9 = true;
  v10 = 0;
  v37 = 268435458;
  v42 = a6;
  v43 = 0;
  if ( a3 != 0 )
  {
    v6 = 8195;
    if ( *a1 != 0 )
    {
      v6 = 0;
      if ( a1[4] != 0 )
      {
        v16 = a1[13];
        if ( v16 != 0 )
        {
          v26 = a4 - 1;
          v27 = mp4_io_fn_20(a2, a3, a1[12], v16, a4 - 1, &v37, &v38);
          v6 = 10;
          if ( v27 != 0 )
            return v6;
          v28 = v37;
          v6 = 0;
          switch ( v37 )
          {
            case 0:
              return v6;
            case 268435457:
              v7 = a1[12] + a1[13];
              v8 = a1[14] + a1[15];
              v10 = a1[10] + a1[11];
              break;
            case 268435458:
LABEL_38:
              break;
            case 536870913:
            case 536870915:
              v8 = a1[14] - 1;
              v7 = a1[12] - 1;
              v10 = a1[10] - 1;
              break;
            case 536870914:
              v7 = v26;
              v8 = a5 - 1;
              v10 = a1[4] - 1;
              break;
            default:
              v28 = 268435458;
              v38 = 2;
              v37 = 268435458;
              goto LABEL_38;
          }
          v9 = (v28 & 0x10000000) != 0;
          if ( v38 != 0 )
          {
            if ( v38 == 1 )
            {
              v6 = mp4_io_fn_45(
                     a1 + 8,
                     a2,
                     a3,
                     a1[12] + a1[13] - 1,
                     a1[14] + a1[15] - 1,
                     0,
                     v9,
                     &v41,
                     &v39,
                     0,
                     &v40,
                     a1 + 9);
              if ( v6 != 0 )
                return v6;
              if ( a1[8] != 0 )
              {
                if ( v9 )
                {
                  v31 = a1[12] + v41;
                  v32 = a1[14] + v39;
                  v33 = a1[10] + v40;
                  v34 = a1[13] - v41;
                  v35 = a1[15] - v39;
                  v36 = a1[11] - v40;
                  v43 = 1;
                  a1[13] = v34;
                  a1[15] = v35;
                  a1[11] = v36;
                  a1[12] = v31;
                  a1[14] = v32;
                  a1[10] = v33;
                }
                else
                {
                  v29 = a1[15] - v39;
                  v30 = a1[11] - v40;
                  a1[13] -= v41;
                  a1[15] = v29;
                  a1[11] = v30;
                }
              }
              else
              {
                v9 = true;
                v10 = 0;
                v7 = 0;
                v8 = 0;
                v37 = 268435458;
                v38 = 2;
              }
            }
            else if ( v38 == 2 )
            {
              mp4_api_fn_14(a1);
            }
            else
            {
              mp4_api_fn_14(a1);
              v38 = 2;
              v9 = true;
              v37 = 268435458;
              v10 = 0;
              v7 = 0;
              v8 = 0;
            }
          }
        }
        v17 = v43;
        if ( v9 )
        {
          v19 = v42;
          v21 = a1;
          v20 = a2;
          v22 = a3;
          v23 = v10;
          v24 = v7;
          v25 = v8;
        }
        else
        {
          v6 = mp4_io_fn_23(a1, a2, a3, v10, v7, v8, v43, v42);
          if ( v6 != 0 )
            return v6;
          if ( v37 != 536870915 )
            return 0;
          v19 = v42;
          v20 = a2;
          v21 = a1;
          v22 = a3;
          v23 = a1[10] + a1[11];
          v24 = a1[12] + a1[13];
          v25 = a1[14] + a1[15];
          v17 = 1;
        }
        v6 = mp4_io_fn_22(v21, v20, v22, v23, v24, v25, v17, v19);
        if ( v6 == 0 )
          return 0;
      }
    }
  }
  return v6;
}


int mp4_io_fn_22(int a1, unsigned int a2, int a3, int a4, int a5, int a6, int a7, int *a8)
{
  s64 v13;
  int v14;
  int v15;
  int v16;
  unsigned int v17;
  int v18;
  int v19;
  int v20;
  unsigned int v21;
  unsigned int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v30;
  int v31;
  int v32;
  unsigned int v33;
  int v34;
  int v35;

  v33 = a2;
  v34 = a3;
  v35 = a7;
  v14 = mp4_io_fn_47(a1 + 32);
  v13 = *(u64 *)(a1 + 24) + (unsigned int)(8 * a4);
  v17 = *(u32 *)(a1 + 16) - a4;
  if ( mp4_box_fn_2(*a8) != (u32)v13 )
  {
    v18 = *a8;
    goto LABEL_3;
  }
  if ( v15 != HIDWORD(v13) )
  {
    v18 = *a8;
LABEL_3:
    v19 = mp4_box_fn_24(v18, v16, v13, SHIDWORD(v13));
    v20 = 2;
    if ( v19 != 0 )
      return v20;
  }
  if ( v17 == 0 )
    return 0;
  v21 = v33 + v34 - 1;
  while ( 1 )
  {
    v22 = a8[4];
    if ( v17 < v22 )
      v22 = v17;
    v20 = mp4_io_fn_39(v14, v22, 1, a5, a6, &v30, &v31, &v32, a1 + 36, a8);
    if ( v20 != 0 )
      break;
    v23 = v30;
    v24 = v32;
    if ( a5 + v30 - 1 >= v33 )
    {
      v25 = v31;
      if ( v35 == 0 )
      {
        v35 = 1;
        *(u32 *)(a1 + 40) = a4;
        *(u32 *)(a1 + 48) = a5;
        *(u32 *)(a1 + 56) = a6;
      }
      v26 = *(u32 *)(a1 + 60);
      v27 = *(u32 *)(a1 + 52) + v23;
      *(u32 *)(a1 + 44) += v24;
      *(u32 *)(a1 + 52) = v27;
      *(u32 *)(a1 + 60) = v26 + v25;
      v28 = mp4_io_fn_48(v14);
      v23 = v30;
      v14 = v28;
      if ( a5 + v30 - 1 >= v21 )
        return 0;
      v24 = v32;
    }
    a5 += v23;
    v17 -= v24;
    a4 += v24;
    a6 += v31;
    if ( v17 == 0 )
      return 0;
  }
  return v20;
}


int mp4_io_fn_23(int a1, unsigned int a2, int a3, unsigned int a4, int a5, int a6, int a7, int *a8)
{
  unsigned int v12;
  int v13;
  int v14;
  unsigned int v15;
  int v16;
  s64 v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  unsigned int v25;
  int v26;
  int v28;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33; // dc
  int v34;
  int v35;
  int v36;
  int v37;
  unsigned int v38;
  int v39;
  int v40;
  unsigned int v41;

  v12 = a8[4];
  v34 = 0;
  if ( v12 == 0 )
    _break(0, 7u);
  v13 = *(u32 *)(a1 + 16);
  v38 = a2;
  v14 = v13;
  v39 = a7;
  v15 = a4 / v12 * v12;
  v16 = v13 - v15;
  if ( v13 - v12 >= v15 )
  {
    v14 = a4 + 1;
    v16 = v12;
  }
  if ( v14 != 0 )
  {
    v40 = a1 + 36;
    v41 = v38 + a3 - 1;
    while ( 1 )
    {
      v17 = *(u64 *)(a1 + 24) + 8 * v15;
      if ( mp4_box_fn_2(*a8) == (u32)v17 && (v22 = v40, v18 == HIDWORD(v17)) )
      {
        v23 = v16;
      }
      else
      {
        v20 = mp4_box_fn_24(*a8, v19, v17, SHIDWORD(v17));
        v21 = 2;
        if ( v20 != 0 )
          return v21;
        v22 = v40;
        v23 = v16;
      }
      v21 = mp4_io_fn_39(&v34, v23, 0, a5, a6, &v35, &v36, &v37, v22, a8);
      if ( v21 != 0 )
        break;
      v24 = v35;
      v25 = a5 - v35 + 1;
      v26 = v36;
      if ( v41 >= v25 )
      {
        *(u32 *)(a1 + 48) = v25;
        *(u32 *)(a1 + 40) = v15;
        v28 = v34;
        v29 = *(u32 *)(a1 + 52) + v24;
        v30 = *(u32 *)(a1 + 60) + v26;
        *(u32 *)(a1 + 44) += v37;
        v31 = *(u32 *)(a1 + 32);
        *(u32 *)(a1 + 56) = a6 - v26 + 1;
        *(u32 *)(a1 + 52) = v29;
        *(u32 *)(a1 + 60) = v30;
        mp4_io_fn_49(v28, v31);
        v24 = v35;
        v32 = v34;
        v34 = 0;
        v33 = v38 < a5 - v35 + 1;
        *(u32 *)(a1 + 32) = v32;
        if ( !v33 )
          return 0;
      }
      v14 -= v37;
      a5 -= v24;
      v16 = a8[4];
      v15 -= v16;
      a6 -= v36;
      if ( v14 == 0 )
        goto LABEL_13;
    }
  }
  else
  {
LABEL_13:
    v21 = 8195;
    if ( v39 != 0 )
      return 0;
  }
  return v21;
}


int mp4_io_fn_24(u32 *a1, unsigned int a2, int a3, int a4, int a5, int a6)
{
  int v6;
  int v8;
  int v9;
  int v10;
  int v11;
  int v16;
  int v18;
  u32 *v19;
  unsigned int v20;
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
  int v35;
  int v36;
  int v37;
  int v38;
  int v39;
  int v40;
  int v41;
  int v42;
  int v43;
  int v44;
  int v45;

  v6 = 6;
  v8 = 0;
  v9 = 0;
  v10 = true;
  v11 = 0;
  v39 = 268435458;
  v44 = a6;
  v45 = 0;
  if ( a3 != 0 )
  {
    v6 = 8195;
    if ( *a1 != 0 )
    {
      v6 = 0;
      if ( a1[4] != 0 )
      {
        v16 = a1[13];
        if ( v16 != 0 )
        {
          v26 = a4 - 1;
          v27 = mp4_io_fn_20(a2, a3, a1[12], v16, a4 - 1, &v39, &v40);
          v6 = 10;
          if ( v27 != 0 )
            return v6;
          v29 = v39;
          v6 = 0;
          if ( v39 == 0 )
            return v6;
          v30 = 0x10000000;
          if ( v39 == 268435457 )
          {
            v8 = a1[12] + a1[13];
            v9 = a1[14] + a1[15];
            v11 = a1[10] + a1[11];
          }
          else
          {
            v30 = 0x20000000;
            if ( v39 != 268435458 )
            {
              v28 = 536870915;
              if ( v39 == 0x20000001 || v39 == 0x20000003 )
              {
                v8 = a1[12] - 1;
                v9 = a1[14] - 1;
                v11 = a1[10] - 1;
              }
              else if ( v39 == 536870914 )
              {
                v8 = v26;
                v9 = a5 - 1;
                v11 = a1[4] - 1;
              }
              else
              {
                v29 = 268435458;
                v40 = 2;
                v39 = 268435458;
              }
            }
          }
          v10 = (v29 & 0x10000000) != 0;
          if ( v40 != 0 )
          {
            if ( v40 == 1 )
            {
              v6 = mp4_io_fn_45(
                     a1 + 8,
                     a2,
                     a3,
                     a1[12] + a1[13] - 1,
                     0,
                     a1[14] + a1[15] - 1,
                     v10,
                     &v43,
                     0,
                     &v41,
                     &v42,
                     a1 + 9);
              if ( v6 != 0 )
                return v6;
              if ( a1[8] != 0 )
              {
                if ( v10 )
                {
                  v33 = a1[12] + v43;
                  v34 = a1[14] + v41;
                  v35 = a1[10] + v42;
                  v36 = a1[13] - v43;
                  v37 = a1[15] - v41;
                  v38 = a1[11] - v42;
                  v45 = 1;
                  a1[13] = v36;
                  a1[15] = v37;
                  a1[11] = v38;
                  a1[12] = v33;
                  a1[14] = v34;
                  a1[10] = v35;
                }
                else
                {
                  v31 = a1[15] - v41;
                  v32 = a1[11] - v42;
                  a1[13] -= v43;
                  a1[15] = v31;
                  a1[11] = v32;
                }
              }
              else
              {
                v10 = true;
                v11 = 0;
                v8 = 0;
                v9 = 0;
                v39 = 268435458;
                v40 = 2;
              }
            }
            else if ( v40 == 2 )
            {
              mp4_api_fn_15(a1, v28, 2, v30);
            }
            else
            {
              mp4_api_fn_15(a1, v28, 2, v30);
              v11 = 0;
              v39 = 268435458;
              v8 = 0;
              v10 = true;
              v40 = 2;
              v9 = 0;
            }
          }
        }
        if ( v10 )
        {
          v25 = v45;
          v19 = a1;
          v20 = a2;
          v21 = a3;
          v22 = v11;
          v18 = v8;
          v23 = v9;
          v24 = a5;
        }
        else
        {
          v6 = mp4_io_fn_26(a1, a2, a3, v11, v8, v9, v45, a5, v44);
          if ( v6 != 0 )
            return v6;
          v6 = 0;
          if ( v39 != 536870915 )
            return v6;
          v18 = a1[12] + a1[13];
          v19 = a1;
          v20 = a2;
          v21 = a3;
          v22 = a1[10] + a1[11];
          v23 = a1[14] + a1[15];
          v24 = a5;
          v25 = 1;
        }
        v6 = mp4_io_fn_25(v19, v20, v21, v22, v18, v23, v25, v24, v44);
        if ( v6 == 0 )
          return 0;
      }
    }
  }
  return v6;
}


int mp4_io_fn_25(int a1, unsigned int a2, int a3, int a4, int a5, int a6, int a7, int a8, int *a9)
{
  int v14;
  int v15;
  unsigned int v16;
  int v17;
  s64 v18;
  s64 v19;
  int v20;
  int v21;
  int v22;
  unsigned int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  int v31;
  int v32;
  int v33;
  unsigned int v34;
  int v35;
  int v36;
  int v37;
  unsigned int v38;

  v34 = a2;
  v35 = a7;
  v36 = a8;
  v14 = mp4_io_fn_47(a1 + 32);
  v15 = *(u32 *)(a1 + 16);
  v16 = v15 - a4;
  v17 = v14;
  if ( v15 == a4 )
    return 0;
  v37 = a1 + 36;
  v38 = v34 + a3 - 1;
  while ( 1 )
  {
    v18 = *(u64 *)(a1 + 24) + (unsigned int)(12 * a4);
    v19 = ((u64)(v19) & 0xFFFFFFFF00000000ULL) | (u32)(mp4_box_fn_2(*a9));
    if ( v19 == v18 )
    {
      v23 = a9[4];
    }
    else
    {
      v21 = mp4_box_fn_24(*a9, v20, v18, SHIDWORD(v18));
      v22 = 2;
      if ( v21 != 0 )
        return v22;
      v23 = a9[4];
    }
    if ( v16 < v23 )
      v23 = v16;
    v22 = mp4_io_fn_40(v17, v23, 1, a5, a6, v16 == v23, v36, a1 + 72, &v31, &v32, &v33, v37, a9);
    if ( v22 != 0 )
      break;
    v24 = v31;
    if ( a5 + v31 - 1 >= v34 )
    {
      v26 = v33;
      v27 = v32;
      if ( v35 == 0 )
      {
        v35 = 1;
        *(u32 *)(a1 + 40) = a4;
        *(u32 *)(a1 + 48) = a5;
        *(u32 *)(a1 + 56) = a6;
      }
      v28 = *(u32 *)(a1 + 60);
      v29 = *(u32 *)(a1 + 52) + v24;
      *(u32 *)(a1 + 44) += v26;
      *(u32 *)(a1 + 52) = v29;
      *(u32 *)(a1 + 60) = v28 + v27;
      v17 = mp4_io_fn_48(v17);
      if ( *(u32 *)(a1 + 72) == 0 )
        return 0;
      v24 = v31;
      v25 = v33;
      if ( a5 + v31 - 1 >= v38 )
        return 0;
    }
    else
    {
      v25 = v33;
    }
    a5 += v24;
    v16 -= v25;
    a4 += v25;
    a6 += v32;
    if ( v16 == 0 )
      return 0;
  }
  return v22;
}


int mp4_io_fn_26(int a1, unsigned int a2, int a3, unsigned int a4, int a5, int a6, int a7, int a8, int *a9)
{
  unsigned int v12;
  int v13;
  int v14;
  unsigned int v15;
  int v16;
  s64 v17;
  s64 v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  unsigned int v24;
  int v25;
  int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  int v32; // dc
  int v33;
  int v34;
  int v35;
  int v36;
  unsigned int v37;
  int v38;
  int v39;
  int v40;
  unsigned int v41;

  v37 = a2;
  v12 = a9[4];
  v36 = 0;
  if ( v12 == 0 )
    _break(0, 7u);
  v13 = *(u32 *)(a1 + 16);
  v38 = a7;
  v14 = v13;
  v39 = a8;
  v15 = a4 / v12 * v12;
  v16 = v13 - v15;
  if ( v13 - v12 >= v15 )
  {
    v14 = a4 + 1;
    v16 = v12;
  }
  if ( v14 != 0 )
  {
    v40 = a1 + 36;
    v41 = v37 + a3 - 1;
    while ( 1 )
    {
      v17 = *(u64 *)(a1 + 24) + 12 * v15;
      v18 = ((u64)(v18) & 0xFFFFFFFF00000000ULL) | (u32)(mp4_box_fn_2(*a9));
      if ( v18 == v17 )
      {
        v22 = *(u32 *)(a1 + 16);
      }
      else
      {
        v20 = mp4_box_fn_24(*a9, v19, v17, SHIDWORD(v17));
        v21 = 2;
        if ( v20 != 0 )
          return v21;
        v22 = *(u32 *)(a1 + 16);
      }
      v21 = mp4_io_fn_40(&v36, v16, 0, a5, a6, v14 == v22, v39, 0, &v33, &v34, &v35, v40, a9);
      if ( v21 != 0 )
        break;
      v23 = v33;
      v24 = a5 - v33 + 1;
      v25 = v34;
      if ( v41 >= v24 )
      {
        *(u32 *)(a1 + 48) = v24;
        *(u32 *)(a1 + 40) = v15;
        v27 = *(u32 *)(a1 + 60);
        v28 = *(u32 *)(a1 + 32);
        v29 = *(u32 *)(a1 + 52) + v23;
        *(u32 *)(a1 + 44) += v35;
        *(u32 *)(a1 + 56) = a6 - v25 + 1;
        v30 = v36;
        *(u32 *)(a1 + 52) = v29;
        *(u32 *)(a1 + 60) = v27 + v25;
        mp4_io_fn_49(v30, v28);
        v23 = v33;
        v31 = v36;
        v36 = 0;
        v32 = v37 < a5 - v33 + 1;
        *(u32 *)(a1 + 32) = v31;
        if ( !v32 )
          goto LABEL_14;
      }
      v14 -= v35;
      a5 -= v23;
      v16 = a9[4];
      v15 -= v16;
      a6 -= v34;
      if ( v14 == 0 )
        goto LABEL_13;
    }
  }
  else
  {
LABEL_13:
    v21 = 8195;
    if ( v38 != 0 )
    {
LABEL_14:
      *(u32 *)(a1 + 72) = 1;
      return 0;
    }
  }
  return v21;
}


int mp4_io_fn_27(u32 *a1, unsigned int a2, unsigned int a3, int a4, int *a5, int a6)
{
  int v11;
  int v12;
  unsigned int v13;
  int v14;
  int v15;
  unsigned int v16;
  int v17;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  u32 *v24;
  int v25;
  int v26;
  int v27;
  int v28;
  u32 *v29;
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
  int v41;
  int v42;

  v11 = 6;
  v12 = a3;
  if ( a3 == 0 )
    return v11;
  v11 = 8195;
  if ( *a1 == 0 )
    return v11;
  v13 = a1[4];
  v11 = 0;
  if ( v13 == 0 )
    return v11;
  v14 = *a5;
  *a5 = 0;
  if ( v13 < a3 )
    v12 = v13;
  v15 = a1[3];
  v37 = 268435458;
  v16 = a2;
  v17 = v12;
  v41 = a2 + v12 - 1;
  v42 = 0;
  v40 = 1;
  if ( v15 != 0 )
  {
    a1[11] = v12;
    a1[10] = a2;
    *a5 = (a4 - a2) * v15;
    return v11;
  }
  v19 = a1[11];
  if ( v19 == 0 )
    goto LABEL_11;
  v27 = mp4_io_fn_20(a2, v12, a1[10], v19, a1[4] - 1, &v37, &v38);
  v11 = 10;
  if ( v27 != 0 )
    return v11;
  switch ( v37 )
  {
    case 0:
      v11 = 0;
      *a5 = v14;
      return v11;
    case 536870913:
      v36 = v38;
      if ( v38 != v40 )
        goto LABEL_31;
      v28 = a1[10];
      goto LABEL_48;
    case 536870915:
      v28 = a1[10];
      v36 = v38;
LABEL_48:
      v17 = v28 - a2;
      v16 = v28 - 1;
      v40 = 0;
      goto LABEL_31;
    default:
      break;
  }
  v36 = v38;
LABEL_31:
  if ( v36 == 0 )
  {
    v20 = *a5;
    goto LABEL_12;
  }
  if ( v36 != 1 )
  {
    v29 = a1;
    if ( v36 != 2 )
    {
      v37 = 268435458;
      v29 = a1;
      v40 = 1;
      v38 = 2;
    }
    mp4_api_fn_16(v29);
    v20 = *a5;
    goto LABEL_12;
  }
  if ( v37 == 268435457 )
  {
    v35 = mp4_api_fn_24(a1, a4, a2, v14, a5);
    v11 = v35;
    if ( v35 != 0 )
      return v11;
    v30 = a1[10];
  }
  else
  {
    v30 = a1[10];
  }
  v11 = mp4_io_fn_45(a1 + 8, a2, v12, v30 + a1[11] - 1, 0, 0, v40, &v39, 0, 0, 0, a1 + 9);
  if ( v11 != 0 )
    return v11;
  if ( a1[8] != 0 )
  {
    if ( v40 == 1 )
    {
      v32 = a1[11];
      v33 = a1[10];
      v34 = v33 + v39;
      v16 = v33 + v32;
      v17 = v41 - (v33 + v32) + 1;
      v42 = 1;
      a1[11] = v32 - v39;
      a1[10] = v34;
    }
    else
    {
      v31 = a1[10];
      v17 = v31 - a2;
      v16 = v31 - 1;
      a1[11] -= v39;
    }
  }
  else
  {
    v40 = 1;
    v37 = 268435458;
    v38 = 2;
  }
LABEL_11:
  v20 = *a5;
LABEL_12:
  if ( v20 == 0 && a4 != a2 )
  {
    v11 = mp4_api_fn_25(a1, a4, a2 - 1, a5, a6);
    if ( v11 != 0 )
      return v11;
    v21 = v40;
  }
  else
  {
    v21 = v40;
  }
  v22 = v42;
  if ( v21 == 1 )
  {
    v24 = a1;
    v25 = v16;
    v26 = v17;
    v23 = a6;
  }
  else
  {
    v11 = mp4_io_fn_29(a1, v16, v17, a6);
    if ( v11 != 0 )
      return v11;
    if ( v37 != 536870915 )
      return 0;
    v23 = a6;
    v24 = a1;
    v25 = a1[10] + a1[11];
    v26 = v41 - v25 + 1;
    v22 = 1;
  }
  v11 = mp4_io_fn_28(v24, v25, v26, v22, v23);
  if ( v11 == 0 )
    return 0;
  return v11;
}


int mp4_io_fn_28(int a1, int a2, unsigned int a3, int a4, int *a5)
{
  unsigned int v10;
  s64 v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  unsigned int v18;
  int v19;
  int v21[4];

  v10 = a3;
  v12 = mp4_io_fn_47(a1 + 32);
  v11 = *(u64 *)(a1 + 24) + (unsigned int)(4 * a2);
  if ( mp4_box_fn_2(*a5) != (u32)v11 )
  {
    v15 = *a5;
    goto LABEL_3;
  }
  if ( v13 != HIDWORD(v11) )
  {
    v15 = *a5;
LABEL_3:
    v16 = mp4_box_fn_24(v15, v14, v11, SHIDWORD(v11));
    v17 = 2;
    if ( v16 != 0 )
      return v17;
  }
  if ( a3 == 0 )
    return 0;
  while ( 1 )
  {
    v18 = a5[4];
    if ( v10 < v18 )
      v18 = v10;
    v17 = mp4_io_fn_41(v12, a2, v18, v21, a1 + 36, a5);
    if ( v17 != 0 )
      break;
    v19 = v21[0];
    if ( a4 == 0 )
    {
      *(u32 *)(a1 + 40) = a2;
      a4 = 1;
    }
    v10 -= v19;
    a2 += v19;
    *(u32 *)(a1 + 44) += v19;
    v12 = mp4_io_fn_48(v12);
    if ( v10 == 0 )
      return 0;
  }
  return v17;
}


int mp4_io_fn_29(int a1, int a2, int a3, int *a4)
{
  int v5;
  int v7;
  int v8;
  int v9;
  s64 v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  u32 v21[3];

  v5 = a3;
  v7 = a4[4];
  v20 = 0;
  v8 = a2 - v7 + 1;
  if ( a3 == 0 )
    return 0;
  while ( 1 )
  {
    v11 = *(u64 *)(a1 + 24) + (unsigned int)(4 * v8);
    if ( mp4_box_fn_2(*a4) != (u32)v11 || (v13 = v8, v12 != HIDWORD(v11)) )
    {
      v14 = mp4_box_fn_24(*a4, v13, v11, SHIDWORD(v11));
      v9 = 2;
      if ( v14 != 0 )
        return v9;
      v13 = v8;
    }
    v9 = mp4_io_fn_41(&v20, v13, v7, v21, a1 + 36, a4);
    if ( v9 != 0 )
      break;
    v15 = v21[0];
    v16 = *(u32 *)(a1 + 44);
    *(u32 *)(a1 + 40) = v8;
    v5 -= v15;
    v17 = v20;
    v18 = *(u32 *)(a1 + 32);
    *(u32 *)(a1 + 44) = v16 + v15;
    v8 -= v15;
    mp4_io_fn_49(v17, v18);
    v19 = v20;
    v20 = 0;
    *(u32 *)(a1 + 32) = v19;
    if ( v5 == 0 )
      return 0;
  }
  return v9;
}


int mp4_io_fn_30(u32 *a1, unsigned int a2, int a3, int a4)
{
  int v4;
  int v9;
  int v10;
  int v11;
  unsigned int v12;
  int v13;
  int v14;
  int v15;
  int v17;
  u32 *v18;
  int v19;
  int v20;
  int v21;
  int v22;
  u32 *v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  int v32;

  v4 = 6;
  if ( a3 == 0 )
    return v4;
  v4 = 8195;
  if ( *a1 == 0 )
    return v4;
  v9 = a1[4];
  v4 = 0;
  if ( v9 == 0 )
    return v4;
  v10 = a1[11];
  v29 = 268435458;
  v11 = a2 + a3 - 1;
  v12 = a2;
  v32 = 0;
  v13 = a3;
  v14 = 1;
  if ( v10 != 0 )
  {
    v21 = mp4_io_fn_20(a2, a3, a1[10], v10, v9 - 1, &v29, &v30);
    v4 = 10;
    if ( v21 != 0 )
      return v4;
    v4 = 0;
    switch ( v29 )
    {
      case 0:
        return v4;
      case 536870913:
        v28 = v30;
        if ( v30 != 1 )
        {
LABEL_19:
          if ( v28 != 0 )
          {
            if ( v28 == 1 )
            {
              v4 = mp4_io_fn_46(a1 + 8, a2, a3, v14, &v31, a1 + 9);
              if ( v4 != 0 )
                return v4;
              if ( a1[8] != 0 )
              {
                if ( v14 == 1 )
                {
                  v25 = a1[11];
                  v26 = a1[10];
                  v27 = v26 + v31;
                  v12 = v26 + v25;
                  v13 = v11 - (v26 + v25) + 1;
                  v32 = 1;
                  a1[11] = v25 - v31;
                  a1[10] = v27;
                }
                else
                {
                  v24 = a1[10];
                  v13 = v24 - a2;
                  v12 = v24 - 1;
                  a1[11] -= v31;
                }
              }
              else
              {
                v14 = 1;
                v29 = 268435458;
                v30 = 2;
              }
            }
            else
            {
              v23 = a1;
              if ( v28 != 2 )
              {
                v29 = 268435458;
                v23 = a1;
                v14 = 1;
                v30 = 2;
              }
              mp4_api_fn_17(v23);
            }
          }
          goto LABEL_5;
        }
        v22 = a1[10];
        break;
      case 536870915:
        v22 = a1[10];
        v28 = v30;
        break;
      default:
        v28 = v30;
        goto LABEL_19;
    }
    v14 = 0;
    v13 = v22 - a2;
    v12 = v22 - 1;
    goto LABEL_19;
  }
LABEL_5:
  v15 = v32;
  if ( v14 == 1 )
  {
    v18 = a1;
    v19 = v12;
    v20 = v13;
    v17 = a4;
  }
  else
  {
    v4 = mp4_io_fn_32(a1, v12, v13, a4);
    if ( v4 != 0 )
      return v4;
    v4 = 0;
    if ( v29 != 536870915 )
      return v4;
    v17 = a4;
    v18 = a1;
    v19 = a1[10] + a1[11];
    v20 = v11 - v19 + 1;
    v15 = 1;
  }
  v4 = mp4_io_fn_31(v18, v19, v20, v15, v17);
  if ( v4 == 0 )
    return 0;
  return v4;
}


int mp4_io_fn_31(int a1, int a2, unsigned int a3, int a4, int *a5)
{
  unsigned int v10;
  s64 v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v21[4];

  v10 = a3;
  v12 = mp4_io_fn_47(a1 + 32);
  v11 = *(u64 *)(a1 + 24) + (unsigned int)(4 * a2);
  if ( mp4_box_fn_2(*a5) != (u32)v11 )
  {
    v15 = *a5;
    goto LABEL_3;
  }
  if ( v13 != HIDWORD(v11) )
  {
    v15 = *a5;
LABEL_3:
    v16 = mp4_box_fn_24(v15, v14, v11, SHIDWORD(v11));
    v17 = 2;
    if ( v16 != 0 )
      return v17;
  }
  if ( a3 == 0 )
    return 0;
  while ( 1 )
  {
    v18 = v10;
    if ( v10 >= a5[4] )
      v18 = a5[4];
    v17 = mp4_io_fn_42(v12, a2, v18, v21, a1 + 36, a5);
    if ( v17 != 0 )
      break;
    v19 = v21[0];
    if ( a4 == 0 )
    {
      *(u32 *)(a1 + 40) = a2;
      a4 = 1;
    }
    v10 -= v19;
    a2 += v19;
    *(u32 *)(a1 + 44) += v18;
    v12 = mp4_io_fn_48(v12);
    if ( v10 == 0 )
      return 0;
  }
  return v17;
}


int mp4_io_fn_32(int a1, int a2, int a3, int *a4)
{
  int v4;
  int v7;
  int v8;
  int v9;
  s64 v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  u32 v19[3];

  v4 = a3;
  v7 = a4[4];
  v18 = 0;
  v8 = a2 - v7 + 1;
  if ( a3 == 0 )
    return 0;
  while ( 1 )
  {
    v11 = *(u64 *)(a1 + 24) + (unsigned int)(4 * v8);
    if ( mp4_box_fn_2(*a4) != (u32)v11 || (v13 = v8, v12 != HIDWORD(v11)) )
    {
      v14 = mp4_box_fn_24(*a4, v13, v11, SHIDWORD(v11));
      v9 = 2;
      if ( v14 != 0 )
        return v9;
      v13 = v8;
    }
    v9 = mp4_io_fn_42(&v18, v13, v7, v19, a1 + 36, a4);
    if ( v9 != 0 )
      break;
    v15 = v19[0];
    v16 = *(u32 *)(a1 + 44);
    *(u32 *)(a1 + 40) = v8;
    v4 -= v15;
    *(u32 *)(a1 + 44) = v16 + v15;
    v8 -= a4[4];
    mp4_io_fn_49(v18, *(u32 *)(a1 + 32));
    v17 = v18;
    v18 = 0;
    *(u32 *)(a1 + 32) = v17;
    if ( v4 == 0 )
      return 0;
  }
  return v9;
}


int mp4_io_fn_33(u32 *a1, unsigned int a2, int a3, int a4, int a5)
{
  int v5;
  int v7;
  int v9;
  int v11;
  int v14;
  unsigned int v15;
  int v16;
  u32 *v18;
  unsigned int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  u32 *v27;
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

  v5 = 6;
  v7 = 0;
  v9 = 0;
  v11 = true;
  v33 = 0x10000000;
  v37 = 0;
  if ( a3 == 0 )
    return v5;
  v5 = 0;
  if ( *a1 == 0 || a1[4] == 0 )
    return v5;
  v14 = a1[13];
  if ( v14 != 0 )
  {
    v15 = a1[12];
    v38 = a4 - 1;
    v25 = mp4_io_fn_20(a2, a3, v15, v14, a4 - 1, &v33, &v34);
    v5 = 10;
    if ( v25 != 0 )
      return v5;
    v5 = 0;
    if ( v33 == 0 )
      return v5;
    v11 = (v33 & 0x10000000) != 0;
    switch ( v33 )
    {
      case 268435457:
        v7 = a1[12] + a1[13];
        v9 = a1[10] + a1[11];
        break;
      case 536870913:
        v26 = a1[12];
LABEL_36:
        v7 = v26 - 1;
        v9 = a1[10] - 1;
        break;
      case 536870914:
        v7 = v38;
        v9 = a1[4] - 1;
        break;
      case 268435458:
        break;
      case 536870915:
        v26 = a1[12];
        goto LABEL_36;
      default:
        v33 = 268435458;
        v11 = true;
        v34 = 2;
        break;
    }
    if ( v34 != 0 )
    {
      if ( v34 == 1 )
      {
        v5 = mp4_io_fn_45(a1 + 8, a2, a3, a1[12] + a1[13] - 1, 0, 0, v11, &v36, 0, 0, &v35, a1 + 9);
        if ( v5 != 0 )
          return v5;
        if ( a1[8] != 0 )
        {
          if ( v11 )
          {
            v29 = a1[13] - v36;
            v30 = a1[12] + v36;
            v31 = a1[10] + v35;
            v32 = a1[11] - v35;
            v37 = 1;
            a1[13] = v29;
            a1[11] = v32;
            a1[12] = v30;
            a1[10] = v31;
          }
          else
          {
            v28 = a1[11] - v35;
            a1[13] -= v36;
            a1[11] = v28;
          }
        }
        else
        {
          v11 = true;
          v9 = 0;
          v7 = 0;
          v33 = 268435458;
          v34 = 2;
        }
      }
      else
      {
        v27 = a1;
        if ( v34 != 2 )
        {
          v34 = 2;
          v27 = a1;
          v33 = 268435458;
          v11 = true;
          v9 = 0;
          v7 = 0;
        }
        mp4_api_fn_18(v27);
      }
    }
  }
  v16 = v37;
  if ( v11 )
  {
    v18 = a1;
    v19 = a2;
    v20 = a3;
    v21 = v9;
    v22 = v7;
    v23 = a4;
    v24 = a5;
  }
  else
  {
    v5 = mp4_io_fn_35(a1, a2, a3, v9, v7, v37, a4, a5);
    if ( v5 != 0 )
      return v5;
    if ( v33 != 536870915 )
      return 0;
    v18 = a1;
    v19 = a2;
    v20 = a3;
    v21 = a1[10] + a1[11];
    v22 = a1[12] + a1[13];
    v23 = a4;
    v24 = a5;
    v16 = 1;
  }
  v5 = mp4_io_fn_34(v18, v19, v20, v21, v22, v16, v23, v24);
  if ( v5 == 0 )
    return 0;
  return v5;
}


int mp4_io_fn_34(int a1, unsigned int a2, int a3, int a4, int a5, int a6, int a7, int *a8)
{
  int v13;
  int v14;
  unsigned int v15;
  int v16;
  s64 v17;
  s64 v18;
  int v19;
  int v20;
  int v21;
  unsigned int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v29;
  int v30;
  unsigned int v31;
  int v32;
  int v33;
  int v34;
  unsigned int v35;

  v31 = a2;
  v32 = a6;
  v33 = a7;
  v13 = mp4_io_fn_47(a1 + 32);
  v14 = *(u32 *)(a1 + 16);
  v15 = v14 - a4;
  v16 = v14 - a4 == v14;
  if ( v14 == a4 )
    return 0;
  v34 = a1 + 36;
  v35 = v31 + a3 - 1;
  while ( 1 )
  {
    v17 = *(u64 *)(a1 + 24) + (unsigned int)(4 * a4);
    v18 = ((u64)(v18) & 0xFFFFFFFF00000000ULL) | (u32)(mp4_box_fn_2(*a8));
    if ( v18 == v17 )
    {
      v22 = a8[4];
    }
    else
    {
      v20 = mp4_box_fn_24(*a8, v19, v17, SHIDWORD(v17));
      v21 = 2;
      if ( v20 != 0 )
        return v21;
      v22 = a8[4];
    }
    if ( v15 < v22 )
      v22 = v15;
    v23 = v16;
    v16 = false;
    v21 = mp4_io_fn_43(v13, v22, v23, v15 == v22, v33, a1 + 64, &v29, &v30, v34, a8);
    if ( v21 != 0 )
      break;
    v24 = v29;
    v25 = v30;
    if ( a5 + v29 - 1 >= v31 )
    {
      v26 = v30;
      if ( v32 == 0 )
      {
        v32 = 1;
        *(u32 *)(a1 + 40) = a4;
        *(u32 *)(a1 + 48) = a5;
      }
      v27 = *(u32 *)(a1 + 52);
      *(u32 *)(a1 + 44) += v26;
      *(u32 *)(a1 + 52) = v27 + v24;
      v13 = mp4_io_fn_48(v13);
      if ( *(u32 *)(a1 + 64) == 0 )
        return 0;
      v24 = v29;
      v21 = 0;
      if ( a5 + v29 - 1 >= v35 )
        return v21;
      v25 = v30;
    }
    a5 += v24;
    v15 -= v25;
    a4 += v25;
    if ( v15 == 0 )
      return 0;
  }
  return v21;
}


int mp4_io_fn_35(int a1, unsigned int a2, int a3, unsigned int a4, int a5, int a6, int a7, int *a8)
{
  unsigned int v11;
  int v12;
  int v13;
  unsigned int v14;
  int v15;
  int v16;
  s64 v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  unsigned int v26;
  int v27;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  int v34; // dc
  int v35;
  int v36;
  int v37;
  unsigned int v38;
  int v39;
  int v40;
  int v41;
  unsigned int v42;

  v11 = a8[4];
  v35 = 0;
  if ( v11 == 0 )
    _break(0, 7u);
  v12 = *(u32 *)(a1 + 16);
  v38 = a2;
  v13 = v12;
  v39 = a6;
  v40 = a7;
  v14 = a4 / v11 * v11;
  v15 = v12 - v14;
  if ( v12 - v11 >= v14 )
  {
    v13 = a4 + 1;
    v15 = v11;
  }
  v16 = v13 == v12;
  if ( v13 != 0 )
  {
    v41 = a1 + 36;
    v42 = v38 + a3 - 1;
    while ( 1 )
    {
      v17 = *(u64 *)(a1 + 24) + 4 * v14;
      if ( mp4_box_fn_2(*a8) == (u32)v17 && (v22 = v41, v18 == HIDWORD(v17)) )
      {
        v23 = v40;
      }
      else
      {
        v20 = mp4_box_fn_24(*a8, v19, v17, SHIDWORD(v17));
        v21 = 2;
        if ( v20 != 0 )
          return v21;
        v22 = v41;
        v23 = v40;
      }
      v24 = v16;
      v16 = false;
      v21 = mp4_io_fn_43(&v35, v15, v13 == v15, v24, v23, 0, &v36, &v37, v22, a8);
      if ( v21 != 0 )
        break;
      v25 = v36;
      v26 = a5 - v36 + 1;
      if ( v42 >= v26 )
      {
        v27 = *(u32 *)(a1 + 52);
        *(u32 *)(a1 + 48) = v26;
        v29 = v27 + v25;
        *(u32 *)(a1 + 40) = v14;
        v30 = *(u32 *)(a1 + 44);
        v31 = v37;
        v32 = *(u32 *)(a1 + 32);
        *(u32 *)(a1 + 52) = v29;
        *(u32 *)(a1 + 44) = v30 + v31;
        mp4_io_fn_49(v35, v32);
        v25 = v36;
        v33 = v35;
        v35 = 0;
        v34 = v38 < a5 - v36 + 1;
        *(u32 *)(a1 + 32) = v33;
        if ( !v34 )
          goto LABEL_15;
      }
      a5 -= v25;
      v13 -= v37;
      v15 = a8[4];
      v14 -= v15;
      if ( v13 == 0 )
        goto LABEL_14;
    }
  }
  else
  {
LABEL_14:
    v21 = 8195;
    if ( v39 != 0 )
    {
LABEL_15:
      *(u32 *)(a1 + 64) = 1;
      return 0;
    }
  }
  return v21;
}


int mp4_io_fn_36(u32 *a1, unsigned int a2, int a3, int a4, int a5)
{
  int v5;
  int v6;
  int v8;
  int v9;
  int v11;
  int v15;
  u32 *v16;
  unsigned int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  u32 *v25;
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

  v5 = 6;
  v6 = 0;
  v8 = 0;
  v9 = 0;
  v11 = true;
  v31 = 0x10000000;
  if ( a3 == 0 )
    return v5;
  v5 = 0;
  if ( *a1 == 0 || a1[4] == 0 )
    return v5;
  v15 = a1[13];
  if ( v15 != 0 )
  {
    v35 = a4 - 1;
    v24 = mp4_io_fn_20(a2, a3, a1[12], v15, a4 - 1, &v31, &v32);
    v5 = 10;
    if ( v24 != 0 )
      return v5;
    v5 = 0;
    if ( v31 == 0 )
      return v5;
    v11 = (v31 & 0x10000000) != 0;
    switch ( v31 )
    {
      case 268435457:
        v8 = a1[12] + a1[13];
        v9 = a1[10] + a1[11];
        break;
      case 268435458:
        break;
      case 536870913:
      case 536870915:
        v8 = a1[12] - 1;
        v9 = a1[10] - 1;
        break;
      case 536870914:
        v8 = v35;
        v9 = a1[4] - 1;
        break;
      default:
        v31 = 268435458;
        v11 = true;
        v32 = 2;
        break;
    }
    if ( v32 != 0 )
    {
      if ( v32 == 1 )
      {
        v26 = mp4_io_fn_45(a1 + 8, a2, a3, a1[12] + a1[13] - 1, 0, 0, v11, &v34, 0, 0, &v33, a1 + 9);
        v5 = v26;
        if ( v26 != 0 )
          return v5;
        if ( a1[8] != 0 )
        {
          if ( v11 )
          {
            v28 = a1[11] - v33;
            v29 = a1[12] + v34;
            v30 = a1[10] + v33;
            v6 = 1;
            a1[13] -= v34;
            a1[11] = v28;
            a1[12] = v29;
            a1[10] = v30;
          }
          else
          {
            v27 = a1[11] - v33;
            a1[13] -= v34;
            a1[11] = v27;
          }
        }
        else
        {
          v11 = true;
          v9 = 0;
          v8 = 0;
          v31 = 268435458;
          v32 = 2;
        }
      }
      else
      {
        v25 = a1;
        if ( v32 != 2 )
        {
          v32 = 2;
          v25 = a1;
          v31 = 268435458;
          v11 = true;
          v9 = 0;
          v8 = 0;
        }
        mp4_api_fn_19(v25);
      }
    }
  }
  v16 = a1;
  if ( v11 )
  {
    v18 = a2;
    v19 = a3;
    v20 = v9;
    v21 = v8;
    v23 = v6;
    v22 = a5;
  }
  else
  {
    v5 = mp4_io_fn_38(a1, a2, a3, v9, v8, v6, a5, 1);
    if ( v5 != 0 )
      return v5;
    v5 = 0;
    if ( v31 != 536870915 )
      return v5;
    v16 = a1;
    v18 = a2;
    v19 = a3;
    v20 = a1[10] + a1[11];
    v21 = a1[12] + a1[13];
    v22 = a5;
    v23 = 1;
  }
  v5 = mp4_io_fn_37(v16, v18, v19, v20, v21, v23, v22);
  if ( v5 == 0 )
    return 0;
  return v5;
}


int mp4_io_fn_37(int a1, unsigned int a2, int a3, int a4, int a5, int a6, int *a7)
{
  s64 v12;
  int v13;
  int v14;
  int v15;
  unsigned int v16;
  int v17;
  int v18;
  int v19;
  unsigned int v20;
  unsigned int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v28;
  int v29;
  int v30;
  int v31;

  v30 = a3;
  v31 = a6;
  v13 = mp4_io_fn_47(a1 + 32);
  v12 = *(u64 *)(a1 + 24) + (unsigned int)(8 * a4);
  v16 = *(u32 *)(a1 + 16) - a4;
  if ( mp4_box_fn_2(*a7) != (u32)v12 )
  {
    v17 = *a7;
    goto LABEL_3;
  }
  if ( v14 != HIDWORD(v12) )
  {
    v17 = *a7;
LABEL_3:
    v18 = mp4_box_fn_24(v17, v15, v12, SHIDWORD(v12));
    v19 = 2;
    if ( v18 != 0 )
      return v19;
  }
  if ( v16 == 0 )
    return 0;
  v20 = a2 + v30 - 1;
  while ( 1 )
  {
    v21 = a7[4];
    if ( v16 < v21 )
      v21 = v16;
    v19 = mp4_io_fn_44(v13, v21, 1, a5, &v28, &v29, a1 + 36, a7);
    if ( v19 != 0 )
      break;
    v22 = v28;
    if ( a5 + v28 - 1 >= a2 )
    {
      v24 = v29;
      if ( v31 == 0 )
      {
        v31 = 1;
        *(u32 *)(a1 + 40) = a4;
        *(u32 *)(a1 + 48) = a5;
      }
      v25 = *(u32 *)(a1 + 44);
      *(u32 *)(a1 + 52) += v22;
      *(u32 *)(a1 + 44) = v25 + v24;
      v26 = mp4_io_fn_48(v13);
      v22 = v28;
      v13 = v26;
      if ( a5 + v28 - 1 >= v20 )
        return 0;
      v23 = v29;
    }
    else
    {
      v23 = v29;
    }
    a5 += v22;
    v16 -= v23;
    a4 += v23;
    if ( v16 == 0 )
      return 0;
  }
  return v19;
}


int mp4_io_fn_38(int a1, unsigned int a2, int a3, unsigned int a4, int a5, int a6, int *a7)
{
  unsigned int v10;
  int v11;
  int v12;
  unsigned int v13;
  int v14;
  int v15;
  unsigned int v16;
  s64 v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  unsigned int v24;
  int v25;
  int v27;
  int v28;
  int v29;
  int v30;
  int v31; // dc
  int v32;
  int v33;
  int v34;
  unsigned int v35;
  int v36;

  v10 = a7[4];
  v32 = 0;
  if ( v10 == 0 )
    _break(0, 7u);
  v11 = *(u32 *)(a1 + 16);
  v35 = a2;
  v12 = v11;
  v36 = a6;
  v13 = a4 / v10 * v10;
  v14 = v11 - v13;
  if ( v11 - v10 >= v13 )
  {
    v12 = a4 + 1;
    v14 = v10;
  }
  if ( v12 != 0 )
  {
    v16 = v35 + a3 - 1;
    while ( 1 )
    {
      v17 = *(u64 *)(a1 + 24) + 8 * v13;
      if ( mp4_box_fn_2(*a7) == (u32)v17 && (v19 = v14, v18 == HIDWORD(v17)) )
      {
        v22 = a5;
      }
      else
      {
        v20 = mp4_box_fn_24(*a7, v19, v17, SHIDWORD(v17));
        v21 = 2;
        if ( v20 != 0 )
          return v21;
        v19 = v14;
        v22 = a5;
      }
      v21 = mp4_io_fn_44(&v32, v19, 0, v22, &v33, &v34, a1 + 36, a7);
      if ( v21 != 0 )
        break;
      v23 = v33;
      v24 = a5 - v33 + 1;
      if ( v16 >= v24 )
      {
        v25 = *(u32 *)(a1 + 52);
        *(u32 *)(a1 + 48) = v24;
        v27 = v25 + v23;
        *(u32 *)(a1 + 40) = v13;
        v28 = *(u32 *)(a1 + 44);
        v29 = v34;
        *(u32 *)(a1 + 52) = v27;
        *(u32 *)(a1 + 44) = v28 + v29;
        mp4_io_fn_49(v32, *(u32 *)(a1 + 32));
        v23 = v33;
        v30 = v32;
        v32 = 0;
        v31 = v35 < a5 - v33 + 1;
        *(u32 *)(a1 + 32) = v30;
        if ( !v31 )
          return 0;
      }
      a5 -= v23;
      v12 -= v34;
      v14 = a7[4];
      v13 -= v14;
      if ( v12 == 0 )
      {
        v15 = v36;
        goto LABEL_16;
      }
    }
  }
  else
  {
    v15 = v36;
LABEL_16:
    v21 = 8195;
    if ( v15 != 0 )
      return 0;
  }
  return v21;
}


int mp4_io_fn_39(
        int **a1,
        unsigned int a2,
        int a3,
        int a4,
        int a5,
        u32 *a6,
        u32 *a7,
        int *a8,
        int **a9,
        int *a10)
{
  int *v15;
  int result;
  int *v17;
  int v18;
  int *v19;
  unsigned int v20;
  int *i;
  int v22;

  v15 = *a1;
  *a6 = 0;
  *a7 = 0;
  *a8 = 0;
  if ( v15 == NULL )
  {
    result = 10;
    if ( *a9 == NULL )
      return result;
    *a1 = *a9;
    *a9 = (int *)(*a9)[7];
    v17 = *a1;
    v17[7] = 0;
    v15 = *a1;
    v17[4] = 0;
    v17[5] = 0;
    v17[6] = 0;
    *v15 = 0;
  }
  v18 = a2;
  if ( a10[4] < a2 )
    v18 = a10[4];
  mp4_box_fn_104(v15[3], 8 * v18, a10);
  if ( (a10[1] & 0xE0000000) != 0 )
  {
    result = 2;
    if ( *a1 != NULL )
    {
      (*a1)[7] = (int)*a9;
      *a9 = *a1;
      *a1 = NULL;
    }
  }
  else
  {
    v19 = *a1;
    *v15 = v18;
    v20 = 0;
    for ( i = (int *)v19[3]; v20 < *v19; i += 2 )
    {
      v22 = *i;
      ++v20;
      *a6 += *i;
      *a7 += i[1] * v22;
    }
    *a8 = *v19;
    if ( a3 == 1 )
    {
      v19[5] = a5;
      v19[4] = a4;
    }
    else
    {
      v19[4] = a4 - *a6 + 1;
      v19[5] = a5 - *a7 + 1;
    }
    return 0;
  }
  return result;
}


int mp4_io_fn_40(
        int **a1,
        unsigned int a2,
        int a3,
        int a4,
        int a5,
        int a6,
        int a7,
        int *a8,
        u32 *a9,
        u32 *a10,
        int *a11,
        int **a12,
        int *a13)
{
  int v13;
  int *v15;
  int result;
  int *v17;
  int v18;
  int *v19;
  unsigned int v20;
  int *v21;
  int *v22;
  int *v23;
  int v24;
  int v25;
  int v26;
  unsigned int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  int v38;
  int v39;

  v13 = 0;
  v15 = *a1;
  *a9 = 0;
  *a10 = 0;
  *a11 = 0;
  v39 = 0;
  v38 = 0;
  if ( v15 == NULL )
  {
    result = 10;
    if ( *a12 == NULL )
      return result;
    *a1 = *a12;
    *a12 = (int *)(*a12)[7];
    v17 = *a1;
    v17[7] = 0;
    v15 = *a1;
    v17[4] = 0;
    v17[5] = 0;
    v17[6] = 0;
    *v15 = 0;
  }
  v18 = a2;
  if ( a13[4] < a2 )
    v18 = a13[4];
  mp4_box_fn_104(v15[3], 12 * v18, a13);
  if ( (a13[1] & 0xE0000000) != 0 )
  {
    v19 = *a1;
LABEL_32:
    result = 2;
    if ( v19 != NULL )
    {
      v19[7] = (int)*a12;
      *a12 = *a1;
      *a1 = NULL;
    }
    return result;
  }
  *v15 = v18;
  v20 = 0;
  v21 = *a1;
  v22 = (int *)(*a1)[3];
  v23 = v22 + 3;
  v24 = *v22;
  v25 = v22[1];
  if ( **a1 == 0 )
  {
LABEL_16:
    *a11 = *v21 - 1;
    goto LABEL_17;
  }
  v26 = **a1;
  while ( 1 )
  {
    v27 = v20 + 1;
    if ( v20 + 1 == v26 )
      break;
    v28 = v23[1];
    v29 = *v23;
    v23 += 3;
    v38 = v28;
    v13 = v29;
    v30 = v29 - v24;
LABEL_12:
    v20 = v27;
    *a9 += v30 * v25;
    v25 = v38;
    *a10 += v30;
    v26 = *v21;
    v24 = v13;
    if ( v27 >= *v21 )
    {
      v31 = v39;
      goto LABEL_14;
    }
  }
  v30 = a5 - v24 + 2;
  if ( a3 != 1 )
    goto LABEL_25;
  if ( a6 == 0 )
  {
    v13 = mp4_box_fn_99(a13);
    v30 = v13 - v24;
    if ( a13[1] < 0 )
    {
      v19 = *a1;
      goto LABEL_32;
    }
    v21 = *a1;
    goto LABEL_25;
  }
  v30 = a7 - v24 + 1;
  if ( a7 != 0 )
  {
LABEL_25:
    v39 = 1;
    goto LABEL_12;
  }
  v31 = v39;
LABEL_14:
  if ( v31 == 0 )
    goto LABEL_16;
  *a11 = *v21;
LABEL_17:
  if ( a3 == 1 )
  {
    v21[6] = a5;
    v21[4] = a4;
  }
  else
  {
    v21[4] = a4 - *a9 + 1;
    v21[6] = a5 - *a10 + 1;
  }
  if ( a8 != NULL )
    *a8 = v39;
  return 0;
}


int mp4_io_fn_41(int **a1, int a2, unsigned int a3, int *a4, int **a5, int *a6)
{
  int v11;
  int *v12;
  int v13;
  int *v14;
  int *v16;

  v11 = a3;
  v12 = *a1;
  if ( *a1 == NULL )
  {
    v13 = 10;
    if ( *a5 == NULL )
      return v13;
    *a1 = *a5;
    *a5 = (int *)(*a5)[7];
    v14 = *a1;
    v14[7] = 0;
    v12 = *a1;
    v14[4] = 0;
    v14[5] = 0;
    v14[6] = 0;
    *v12 = 0;
  }
  if ( a6[4] < a3 )
    v11 = a6[4];
  mp4_box_fn_104(v12[3], 4 * v11, a6);
  v13 = 2;
  if ( (a6[1] & 0xE0000000) != 0 )
  {
    if ( *a1 != NULL )
    {
      (*a1)[7] = (int)*a5;
      *a5 = *a1;
      *a1 = NULL;
    }
  }
  else
  {
    *v12 = v11;
    v13 = 0;
    v16 = *a1;
    *a4 = **a1;
    v16[4] = a2;
  }
  return v13;
}


int mp4_io_fn_42(int **a1, int a2, unsigned int a3, int *a4, int **a5, int *a6)
{
  int v11;
  int *v12;
  int v13;
  int *v14;
  int *v16;

  v11 = a3;
  v12 = *a1;
  if ( *a1 == NULL )
  {
    v13 = 10;
    if ( *a5 == NULL )
      return v13;
    *a1 = *a5;
    *a5 = (int *)(*a5)[7];
    v14 = *a1;
    v14[7] = 0;
    v12 = *a1;
    v14[4] = 0;
    v14[5] = 0;
    v14[6] = 0;
    *v12 = 0;
  }
  if ( a6[4] < a3 )
    v11 = a6[4];
  mp4_box_fn_104(v12[3], 4 * v11, a6);
  v13 = 2;
  if ( (a6[1] & 0xE0000000) != 0 )
  {
    if ( *a1 != NULL )
    {
      (*a1)[7] = (int)*a5;
      *a5 = *a1;
      *a1 = NULL;
    }
  }
  else
  {
    *v12 = v11;
    v13 = 0;
    v16 = *a1;
    *a4 = **a1;
    v16[6] = a2;
  }
  return v13;
}


int mp4_io_fn_43(
        int **a1,
        unsigned int a2,
        int a3,
        int a4,
        int a5,
        int *a6,
        int *a7,
        int *a8,
        int **a9,
        int *a10)
{
  int *v15;
  int result;
  int *v17;
  int v18;
  int *v19;
  int *v20;
  int *v21;
  u32 *v22;
  int v23;
  int v24;
  int v27;

  v15 = *a1;
  *a7 = 0;
  *a8 = 0;
  v27 = 1;
  if ( v15 == NULL )
  {
    result = 10;
    if ( *a9 == NULL )
      return result;
    *a1 = *a9;
    *a9 = (int *)(*a9)[7];
    v17 = *a1;
    v17[7] = 0;
    v15 = *a1;
    v17[4] = 0;
    v17[5] = 0;
    v17[6] = 0;
    *v15 = 0;
  }
  v18 = a2;
  if ( a10[4] < a2 )
    v18 = a10[4];
  mp4_box_fn_104(v15[3], 4 * v18, a10);
  if ( (a10[1] & 0xE0000000) != 0 )
  {
    v19 = *a1;
    goto LABEL_19;
  }
  *v15 = v18;
  v20 = *a1;
  v21 = *a1;
  v22 = (u32 *)(*a1)[3];
  if ( a3 == 1 )
    v20[4] = 0;
  else
    v20[4] = *v22 - 1;
  if ( a4 == 1 )
  {
    if ( a5 == 0 )
    {
      v27 = 0;
      *a7 = v22[*v21 - 1] - v21[4];
      goto LABEL_15;
    }
    v24 = a5 - v21[4];
  }
  else
  {
    v23 = mp4_box_fn_99(a10);
    if ( a10[1] < 0 )
    {
      v19 = *a1;
LABEL_19:
      result = 2;
      if ( v19 != NULL )
      {
        v19[7] = (int)*a9;
        *a9 = *a1;
        *a1 = NULL;
      }
      return result;
    }
    v20 = *a1;
    v24 = v23 - (*a1)[4] - 1;
  }
  *a7 = v24;
LABEL_15:
  *a8 = *v20;
  if ( a6 != NULL )
    *a6 = v27;
  return 0;
}


int mp4_io_fn_44(int **a1, unsigned int a2, int a3, int a4, u32 *a5, int *a6, int **a7, int *a8)
{
  int *v15;
  int result;
  int *v17;
  int v18;
  int *v19;
  unsigned int v20;
  u32 *i;

  v15 = *a1;
  *a5 = 0;
  *a6 = 0;
  if ( v15 == NULL )
  {
    result = 10;
    if ( *a7 == NULL )
      return result;
    *a1 = *a7;
    *a7 = (int *)(*a7)[7];
    v17 = *a1;
    v17[7] = 0;
    v15 = *a1;
    v17[4] = 0;
    v17[5] = 0;
    v17[6] = 0;
    *v15 = 0;
  }
  v18 = a2;
  if ( a8[4] < a2 )
    v18 = a8[4];
  mp4_box_fn_104(v15[3], 8 * v18, a8);
  if ( (a8[1] & 0xE0000000) != 0 )
  {
    result = 2;
    if ( *a1 != NULL )
    {
      (*a1)[7] = (int)*a7;
      *a7 = *a1;
      *a1 = NULL;
    }
  }
  else
  {
    v19 = *a1;
    *v15 = v18;
    v20 = 0;
    for ( i = (u32 *)v19[3]; v20 < *v19; i += 2 )
    {
      ++v20;
      *a5 += *i;
    }
    *a6 = *v19;
    if ( a3 == 1 )
      v19[4] = a4;
    else
      v19[4] = a4 - *a5 + 1;
    return 0;
  }
  return result;
}


int mp4_io_fn_45(
        int **a1,
        unsigned int a2,
        int a3,
        int a4,
        int a5,
        int a6,
        int a7,
        u32 *a8,
        u32 *a9,
        u32 *a10,
        u32 *a11,
        int **a12)
{
  int v13;
  int result;
  int v16;
  int v19;
  int v20;
  int *v21;
  int *v22;
  int *v23;
  unsigned int v24;
  unsigned int i;
  int *v26;
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

  v13 = 0;
  result = 20495;
  v16 = 0;
  v19 = 0;
  v20 = 0;
  if ( *a1 != NULL )
  {
    v21 = *a1;
    v22 = *a1;
    if ( a7 == 1 )
    {
      if ( v21[7] != 0 )
      {
        do
        {
          v34 = *(u32 *)(v21[7] + 16);
          v35 = v21;
          if ( v34 - 1 >= a2 )
            break;
          v36 = v21[4];
          v21 = (int *)v21[7];
          v35[7] = (int)*a12;
          v13 += v34 - v36;
          *a12 = v35;
          v20 += *v35;
          v19 += v21[5] - v35[5];
          v16 += v21[6] - v35[6];
        }
        while ( v21[7] != 0 );
        *a1 = v21;
      }
      else
      {
        *a1 = v21;
      }
    }
    else
    {
      v23 = (int *)v21[7];
      if ( v23 != NULL )
      {
        v24 = a2 + a3 - 1;
        for ( i = v23[4]; ; i = v23[4] )
        {
          v26 = v23;
          v27 = a4;
          v28 = a5;
          if ( v24 < i )
          {
            v29 = a6;
            v23 = (int *)v23[7];
            if ( v23 != NULL )
            {
              v27 = v23[4] - 1;
              v28 = v23[5] - 1;
              v29 = v23[6] - 1;
            }
            v30 = v27 - v26[4];
            v31 = *v26;
            v32 = v28 - v26[5];
            v33 = v29 - v26[6];
            v26[7] = (int)*a12;
            *a12 = v26;
            v13 += v30 + 1;
            v19 += v32 + 1;
            v16 += v33 + 1;
            v20 += v31;
          }
          else
          {
            v22 = v23;
            v23 = (int *)v23[7];
          }
          if ( v23 == NULL )
            break;
        }
        v22[7] = 0;
      }
      else
      {
        v22[7] = 0;
      }
    }
    if ( a8 != NULL )
      *a8 = v13;
    if ( a9 != NULL )
      *a9 = v19;
    if ( a10 != NULL )
      *a10 = v16;
    if ( a11 != NULL )
      *a11 = v20;
    return 0;
  }
  return result;
}


int mp4_io_fn_46(u32 *a1, unsigned int a2, int a3, int a4, u32 *a5, u32 *a6)
{
  u32 *v6;
  int result;
  u32 *v8;
  u32 *v9;
  u32 *v10;
  unsigned int v11;
  unsigned int i;
  u32 *v13;
  int v14;
  int v15;
  u32 *v16;
  u32 *v17;

  v6 = (u32 *)*a1;
  *a5 = 0;
  result = 20495;
  if ( v6 != NULL )
  {
    v8 = v6;
    v9 = v6;
    if ( a4 == 1 )
    {
      if ( v6[7] != 0 )
      {
        v16 = (u32 *)v6[7];
        do
        {
          v17 = v8;
          if ( v16[6] - 1 >= a2 )
            break;
          v8 = v16;
          *a5 += *v16;
          if ( v17 != NULL )
          {
            v17[7] = *a6;
            *a6 = v17;
          }
          v16 = (u32 *)v16[7];
        }
        while ( v8[7] != 0 );
        *a1 = v8;
      }
      else
      {
        *a1 = v6;
      }
    }
    else
    {
      v10 = (u32 *)v6[7];
      if ( v10 != NULL )
      {
        v11 = a2 + a3 - 1;
        for ( i = v10[6]; ; i = v10[6] )
        {
          v13 = v10;
          if ( v11 < i )
          {
            v14 = *v10;
            v10 = (u32 *)v10[7];
            v15 = *a5 + v14;
            v13[7] = *a6;
            *a5 = v15;
            *a6 = v13;
          }
          else
          {
            v9 = v10;
            v10 = (u32 *)v10[7];
          }
          if ( v10 == NULL )
            break;
        }
      }
      v9[7] = 0;
    }
    return 0;
  }
  return result;
}


int *mp4_io_fn_47(int *a1)
{
  int i;

  for ( i = *a1; i != 0; i = *(u32 *)(i + 28) )
    a1 = (int *)(i + 28);
  return a1;
}


int mp4_io_fn_48(u32 *a1)
{
  return *a1 + 28;
}


int mp4_io_fn_49(int a1, int a2)
{
  *(u32 *)(a1 + 28) = a2;
  return 0;
}


int mp4_io_fn_50(int a1, unsigned int a2, int a3, u32 *a4, int a5)
{
  int v5;
  int v11;
  int v12;
  int v13;
  int v14;
  unsigned int v15;
  int v17;
  int v18;
  int v19;
  int v20;
  u8 v21[4];
  int v22;
  int v23;

  v5 = 8195;
  if ( a1 != 0 )
  {
    v5 = 7;
    if ( *(u32 *)(a1 + 460) != 0 )
    {
      v11 = mp4_api_fn_3(a1 + 8);
      v5 = 8195;
      if ( v11 != 0 )
      {
        v12 = mp4_api_fn_4(a1 + 96);
        v5 = 8195;
        if ( v12 != 0 )
        {
          v13 = mp4_api_fn_5(a1 + 192);
          v5 = 8195;
          if ( v13 != 0 )
          {
            v14 = mp4_api_fn_6(a1 + 248);
            v5 = 8195;
            if ( v14 != 0 )
            {
              v15 = *(u32 *)(*(u32 *)(a1 + 460) + 32);
              if ( v15 == 0 || a2 >= v15 )
                return 8201;
              v5 = mp4_io_fn_51(a1, a2, a5);
              if ( v5 == 0 )
              {
                v5 = mp4_api_fn_39(a1, a2, a4 + 2, &v20);
                if ( v5 == 0 )
                {
                  v5 = mp4_api_fn_31(a1 + 192, a2, a4);
                  if ( v5 == 0 )
                  {
                    v5 = mp4_api_fn_45(a1 + 8, a2, v21, a4 + 1);
                    if ( v5 == 0 )
                    {
                      v5 = mp4_io_fn_53(a1 + 304, a2, a4 + 4);
                      if ( v5 == 0 )
                      {
                        v5 = mp4_api_fn_50(a1 + 376, a2, a4 + 5);
                        if ( v5 == 0 )
                        {
                          v5 = mp4_api_fn_55(a1 + 96, v20, &v22, &v23, a4 + 9);
                          if ( v5 == 0 )
                          {
                            v17 = v23;
                            if ( a3 != 1 )
                              return 0;
                            v5 = 8195;
                            if ( v23 != 0 )
                            {
                              a4[6] = 0;
                              v18 = v22 + v17 - 1;
                              v5 = mp4_api_fn_51(a1 + 192, a2, v18, a4 + 6, *(u32 *)(*(u32 *)(a1 + 460) + 44));
                              if ( v5 == 0 )
                              {
                                v19 = *(u32 *)(a1 + 460);
                                a4[7] = 0;
                                v5 = mp4_api_fn_52(a1 + 8, a2, v18, a4 + 7, *(u32 *)(v19 + 44));
                                if ( v5 == 0 )
                                {
                                  a4[8] = v18 - a2 + 1;
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
          }
        }
      }
    }
  }
  return v5;
}


int mp4_io_fn_51(int a1, unsigned int a2, int a3)
{
  int v5;
  unsigned int v6;
  int v7;
  int v9;
  int v10;
  int v11;
  u32 v12[3];

  if ( a1 == 0 )
    return 7;
  v5 = *(u32 *)(a1 + 460);
  if ( v5 == 0 )
    return 7;
  v6 = *(u32 *)(v5 + 32);
  v7 = 8201;
  if ( v6 != 0 )
  {
    if ( a2 >= v6 )
    {
      return 8201;
    }
    else
    {
      v9 = *(u32 *)(v5 + 4);
      if ( v9 == 0
        || *(u32 *)(v5 + 12) == 0
        || *(u32 *)(v5 + 20) == 0
        || (v10 = *(u32 *)v5 + v9, a2 < *(u32 *)v5)
        || (v7 = 0, v10 - 1 < a2) )
      {
        v7 = 2;
        if ( a3 != 0 )
        {
          v7 = mp4_box_fn_106(*(u32 **)(v5 + 44));
          if ( v7 == 0 )
          {
            v7 = mp4_io_fn_52(
                   a2,
                   **(u32 **)(a1 + 460),
                   *(u32 *)(*(u32 *)(a1 + 460) + 4),
                   *(u32 *)(*(u32 *)(a1 + 460) + 32),
                   &v11,
                   v12);
            if ( v7 == 0 )
              return mp4_api_fn_2(a1, v11, v12[0], *(u32 *)(*(u32 *)(a1 + 460) + 44));
          }
        }
      }
    }
  }
  return v7;
}


int mp4_io_fn_52(
        unsigned int a1,
        unsigned int a2,
        unsigned int a3,
        unsigned int a4,
        unsigned int *a5,
        unsigned int *a6,
        int a7)
{
  u32 *v7;
  int v8;
  int result;
  unsigned int v10;
  int v11;
  unsigned int v12;
  unsigned int v13;
  unsigned int v14;
  unsigned int v15;
  int v16;
  int v17;

  v7 = *(u32 **)(a7 + 44);
  v8 = v7[6];
  result = 6;
  if ( v8 != 0 )
  {
    if ( a3 != 0 )
    {
      if ( a1 == a2 + a3 )
      {
        v11 = v7[7];
        if ( v11 - v8 >= a3 )
        {
          *a5 = a2;
          v17 = v7[6];
          v15 = a4 - a2;
          if ( a4 - v17 >= a1 )
            v15 = a3 + v17;
          goto LABEL_15;
        }
        v16 = a1 + v8;
        v12 = v16 - v11;
        *a5 = v16 - v11;
        v13 = v7[7];
        v15 = a4 - v12;
        if ( v13 >= a4 )
        {
LABEL_15:
          *a6 = v15;
          return 0;
        }
        v14 = a4 - v13;
LABEL_12:
        if ( v14 >= v12 )
        {
          *a6 = v13;
          return 0;
        }
        v15 = a4 - v12;
        goto LABEL_15;
      }
      v10 = v7[4];
    }
    else
    {
      v10 = v7[4];
    }
    if ( v10 == 0 )
      _break(0, 7u);
    v12 = a1 / v10 * v10;
    *a5 = v12;
    v13 = v7[6];
    if ( v13 >= a4 )
    {
      *a5 = 0;
      *a6 = a4;
      return 0;
    }
    v14 = a4 - v13;
    goto LABEL_12;
  }
  return result;
}


int mp4_io_fn_53(u32 *a1, unsigned int a2, u32 *a3)
{
  int v4;
  unsigned int v5;
  unsigned int v6;
  unsigned int v8;
  unsigned int v9;
  unsigned int *v10;
  unsigned int *v11;
  int v12;
  unsigned int v13;
  unsigned int v14;
  int v15;
  int v16;
  unsigned int *v17;
  unsigned int *v18;
  unsigned int v19;
  unsigned int v20;
  int v21;
  unsigned int v22;
  unsigned int *v23;
  unsigned int v24;
  unsigned int v25;
  unsigned int v26;
  unsigned int v27;
  unsigned int *v28;
  unsigned int v29;
  unsigned int v30;
  unsigned int v31;
  unsigned int v32;
  unsigned int v33;
  unsigned int *v34;
  unsigned int v35;
  unsigned int *v36;
  unsigned int v37;
  unsigned int v38;
  unsigned int v39;
  unsigned int v40;
  unsigned int v41;
  unsigned int v42;
  unsigned int *v43;
  unsigned int v44;
  unsigned int *v45;
  unsigned int v46;
  int v47;
  int v48;
  int v49;
  unsigned int v50;
  unsigned int v51;
  int v52;

  if ( *a1 == 0 )
    goto LABEL_61;
  v4 = 0;
  if ( a1[4] == 0 )
    goto LABEL_44;
  v5 = a1[12];
  if ( a2 >= v5 )
  {
    if ( a2 >= v5 + a1[13] )
      return 8201;
    v6 = a1[10];
    v8 = a1[14];
    if ( v8 < v6 || v6 + a1[11] - 1 < v8 )
    {
      a1[14] = 0;
      v8 = 0;
      a1[15] = 0;
    }
    if ( v8 != 0 )
    {
      v9 = a1[15];
    }
    else
    {
      v10 = (unsigned int *)a1[8];
      a1[14] = v6;
      if ( v10 == NULL )
        return 10;
      v4 = 10;
      if ( v10[2] == 0 )
        return v4;
      v11 = v10;
      v48 = 0;
      v12 = 0;
      v13 = *v10;
      if ( *v10 == 0 )
      {
        do
        {
          v11 = (unsigned int *)v11[7];
          v14 = v12 - v13 + 1;
          v12 -= v13;
          v48 = v12;
          if ( v11 == NULL )
            return 10;
          v13 = *v11;
        }
        while ( *v11 < v14 );
      }
      a1[15] = *(u32 *)(4 * v48 * v11[2] + v11[3]);
      v9 = --a1[15];
    }
    if ( a2 >= v9 )
    {
      v15 = v9 < a2;
    }
    else
    {
      v15 = v9 < a2;
      if ( a1[14] != 0 )
      {
        v16 = a2 < v9;
        if ( a2 - a1[12] >= v9 - a2 )
        {
LABEL_31:
          if ( v16 )
          {
            v21 = a1[14];
            v4 = 0;
            if ( v21 != 0 )
            {
              v22 = a1[10];
              v23 = (unsigned int *)a1[8];
              v24 = v21 - 1;
              v25 = 0;
              while ( 1 )
              {
                v26 = v24 - v25;
                if ( v23 == NULL || v23[2] == 0 || v26 < v22 )
                  return 10;
                v27 = *v23;
                v51 = v26 - v22;
                v28 = v23;
                if ( *v23 < v26 - v22 + 1 )
                {
                  do
                  {
                    v28 = (unsigned int *)v28[7];
                    v29 = v51 - v27 + 1;
                    v51 -= v27;
                    if ( v28 == NULL )
                      return 10;
                    v27 = *v28;
                  }
                  while ( *v28 < v29 );
                }
                v30 = *(u32 *)(4 * v51 * v28[2] + v28[3]) - 1;
                if ( v30 == a2 )
                {
                  v4 = 0;
                  *a3 = 1;
                  a1[15] = a2;
                  a1[14] = v26;
                  return v4;
                }
                if ( v30 < a2 )
                {
                  v4 = 0;
                  break;
                }
                ++v25;
                a1[14] = v26;
                a1[15] = v30;
                if ( v24 < v25 )
                {
                  v4 = 0;
                  break;
                }
              }
            }
LABEL_44:
            *a3 = 0;
            return v4;
          }
          if ( v9 != a2 )
          {
            v31 = a1[10];
            v32 = a1[14] + 1;
            v33 = v31;
            if ( v32 >= v31 + a1[11] )
            {
LABEL_59:
              *a3 = 0;
              return 0;
            }
            v34 = (unsigned int *)a1[8];
            v35 = v31 + a1[11];
            while ( v34 != NULL && v34[2] != 0 && v32 >= v33 )
            {
              v52 = v32 - v33;
              v36 = v34;
              if ( *v34 < v32 - v33 + 1 )
              {
                v37 = *v34;
                do
                {
                  v36 = (unsigned int *)v36[7];
                  v38 = v52 - v37 + 1;
                  v52 -= v37;
                  if ( v36 == NULL )
                    return 10;
                  v37 = *v36;
                }
                while ( *v36 < v38 );
              }
              v39 = *(u32 *)(4 * v52 * v36[2] + v36[3]) - 1;
              if ( v39 == a2 )
              {
                v4 = 0;
                *a3 = 1;
                a1[15] = v39;
                a1[14] = v32;
                return v4;
              }
              v4 = 0;
              if ( a2 < v39 )
                goto LABEL_44;
              a1[14] = v32++;
              a1[15] = v39;
              v33 = v31;
              if ( v32 >= v35 )
                goto LABEL_59;
            }
            return 10;
          }
LABEL_61:
          v4 = 0;
          *a3 = 1;
          return v4;
        }
        v17 = (unsigned int *)a1[8];
        a1[14] = a1[10];
        if ( v17 != NULL )
        {
          v4 = 10;
          if ( v17[2] == 0 )
            return v4;
          v18 = v17;
          v49 = 0;
          if ( *v17 == 0 )
          {
            v19 = 0;
            do
            {
              v18 = (unsigned int *)v18[7];
              v20 = v49 - v19 + 1;
              v49 -= v19;
              if ( v18 == NULL )
                return 10;
              v19 = *v18;
            }
            while ( *v18 < v20 );
          }
          a1[15] = *(u32 *)(4 * v49 * v18[2] + v18[3]);
          v9 = a1[15] - 1;
          a1[15] = v9;
          goto LABEL_30;
        }
        return 10;
      }
    }
    v16 = a2 < v9;
    if ( !v15 )
      goto LABEL_31;
    v40 = a1[10];
    v41 = v40 + a1[11];
    v42 = v41 - 1;
    if ( a1[14] == v41 - 1 )
      goto LABEL_30;
    v16 = a2 < v9;
    if ( a1[12] + a1[13] - a2 >= a2 - v9 )
      goto LABEL_31;
    v43 = (unsigned int *)a1[8];
    a1[14] = v42;
    if ( v43 != NULL )
    {
      v4 = 10;
      if ( v43[2] == 0 || v42 < v40 )
        return v4;
      v44 = *v43;
      v45 = v43;
      v50 = v42 - v40;
      if ( *v43 < v41 - v40 )
      {
        do
        {
          v45 = (unsigned int *)v45[7];
          v46 = v50 - v44 + 1;
          v50 -= v44;
          if ( v45 == NULL )
            return 10;
          v44 = *v45;
        }
        while ( *v45 < v46 );
      }
      a1[15] = *(u32 *)(4 * v50 * v45[2] + v45[3]);
      v47 = a1[15] - 1;
      a1[15] = v47;
      v9 = v47;
LABEL_30:
      v16 = a2 < v9;
      goto LABEL_31;
    }
    return 10;
  }
  return 8201;
}


int mp4_io_fn_54(int a1, unsigned int a2, unsigned int *a3, u32 *a4, int a5, int a6)
{
  int v6;
  int v13;
  int v14;
  int v15;
  unsigned int v17;

  v6 = 8195;
  if ( a1 != 0 )
  {
    v13 = *(u32 *)(a1 + 460);
    v6 = 7;
    v14 = a1 + 8;
    if ( v13 != 0 )
    {
      v15 = mp4_api_fn_3(v14);
      v6 = 8195;
      if ( v15 != 0 )
      {
        v6 = 8197;
        if ( a2 < *(u32 *)(*(u32 *)(a1 + 460) + 36) )
        {
          v6 = mp4_io_fn_55(a1, a2, a3, a5, a6);
          if ( v6 == 0 )
          {
            v17 = *a3;
            v6 = mp4_api_fn_49(a1, *a3, a6);
            if ( v6 == 0 )
              return mp4_io_fn_53((u32 *)(a1 + 304), v17, a4);
          }
        }
      }
    }
  }
  return v6;
}


int mp4_io_fn_55(int a1, int a2, int a3, int a4, int a5)
{
  int v9;

  v9 = mp4_io_fn_56(a1, a2, a5);
  if ( v9 == 0 )
    return mp4_io_fn_57(a1 + 8, a2, a3, a4);
  return v9;
}


int mp4_io_fn_56(int a1, unsigned int a2, int a3)
{
  int v3;
  int v7;
  int v8;
  int v9;
  unsigned int v10;
  unsigned int v11;
  int v12;
  unsigned int v13;
  unsigned int v14;
  int v15;
  int v16;
  int v18;
  unsigned int v19;
  int v20;
  unsigned int v21;
  int v22;
  unsigned int v23;
  int v24;
  int v25;
  int v26[3];

  v3 = a1 + 8;
  v7 = mp4_api_fn_3(a1 + 8);
  v8 = 8195;
  if ( v7 == 0 )
    return v8;
  v8 = 8197;
  if ( a2 >= *(u32 *)(*(u32 *)(a1 + 460) + 36) )
    return v8;
  mp4_api_fn_10(v3, &v21, &v22, &v23, &v24);
  if ( v22 != 0 && v24 != 0 && a2 >= v23 )
  {
    v8 = 0;
    if ( v23 + v24 - 1 >= a2 )
      return v8;
  }
  v8 = 2;
  if ( a3 == 0 )
    return v8;
  v8 = mp4_box_fn_106(*(u32 **)(*(u32 *)(a1 + 460) + 44));
  if ( v8 != 0 )
    return v8;
  if ( v24 != 0 )
  {
    v20 = mp4_io_fn_20(a2, 1, v23, v24, *(u32 *)(*(u32 *)(a1 + 460) + 36), &v25, v26);
    v8 = 10;
    if ( v20 != 0 )
      return v8;
  }
  else
  {
    v9 = 0x10000000;
    if ( *(u32 *)(*(u32 *)(a1 + 460) + 36) - a2 < a2 )
      v9 = 0x20000000;
    v25 = v9 | 2;
    v26[0] = 0;
  }
  v8 = 0;
  switch ( v25 )
  {
    case 0:
      return v8;
    case 268435457:
      v10 = v21;
      v12 = v22;
      v13 = v21 + v22;
      goto LABEL_20;
    case 268435458:
      v12 = v22;
      v10 = v21;
      v13 = 0;
      goto LABEL_20;
    default:
      break;
  }
  if ( v25 != 0x20000001 && v25 != 0x20000003 )
  {
    if ( v25 == 536870914 )
    {
      v18 = *(u32 *)(a1 + 460);
      v19 = *(u32 *)(v18 + 32);
      v12 = v22;
      if ( *(u32 *)(*(u32 *)(v18 + 44) + 24) < v19 )
      {
        v10 = v21;
        v13 = v19 - 1;
        goto LABEL_20;
      }
    }
    else
    {
      v12 = v22;
    }
    v10 = v21;
    goto LABEL_34;
  }
  v10 = v21;
  v11 = *(u32 *)(*(u32 *)(*(u32 *)(a1 + 460) + 44) + 24);
  v12 = v22;
  if ( v11 >= v21 )
  {
LABEL_34:
    v13 = 0;
    v25 = 268435458;
    goto LABEL_20;
  }
  v13 = v21 - v11;
LABEL_20:
  while ( 1 )
  {
    v14 = v10;
    v15 = v12;
    v16 = mp4_api_fn_46(a1, v13, a3);
    if ( v16 != 0 )
      break;
    mp4_api_fn_10(v3, &v21, &v22, &v23, &v24);
    if ( a2 >= v23 && v23 + v24 - 1 >= a2 )
      break;
    if ( (v25 & 0x10000000) != 0 )
    {
      v10 = v21;
      v13 = v21 + v22;
      if ( v21 + v22 >= *(u32 *)(*(u32 *)(a1 + 460) + 32) )
      {
        v16 = 8197;
        break;
      }
      if ( v14 == v21 && v15 == v22 )
      {
LABEL_31:
        v16 = 8197;
        break;
      }
    }
    else
    {
      v10 = v21;
      if ( v21 == 0 )
        goto LABEL_31;
      v13 = v21 - 1;
    }
    v12 = v22;
  }
  mp4_api_fn_12(a1);
  return v16;
}


int mp4_io_fn_57(u32 *a1, unsigned int a2, u32 *a3, u32 *a4)
{
  int v7;
  unsigned int v9;
  int v10;
  unsigned int v11;
  unsigned int v13;
  unsigned int v14;
  int v15;
  unsigned int v16;
  int v17;
  unsigned int v18;
  int v19;
  int v20;
  unsigned int v21;
  unsigned int *v22;
  unsigned int v23;
  unsigned int *v24;
  unsigned int v25;
  unsigned int v26;
  unsigned int *v27;
  unsigned int v28;
  unsigned int v29;
  unsigned int v30;
  int v31;
  unsigned int v32;
  unsigned int v33;
  unsigned int *v34;
  int v35;
  int v36;
  unsigned int i;
  unsigned int v38;
  unsigned int *v39;
  unsigned int v40;
  unsigned int j;
  unsigned int v42;
  unsigned int v43;
  unsigned int *v44;
  unsigned int v45;
  unsigned int k;
  unsigned int v47;
  unsigned int v48;
  int v49;
  unsigned int v50;
  unsigned int v51;
  unsigned int *v52;
  s64 v53;
  unsigned int *v54;
  unsigned int v55;
  unsigned int v56;
  unsigned int v57;
  unsigned int *v58;
  unsigned int v59;
  int v60;
  int v61;
  int v62;
  int v63;
  unsigned int v64;
  int v65;
  unsigned int v66;
  unsigned int v67;
  unsigned int v68;
  unsigned int v69;
  unsigned int v70;

  v7 = 8195;
  if ( *a1 == 0 )
    return v7;
  v9 = a1[14];
  v7 = 8197;
  if ( a2 < v9 )
    return v7;
  v10 = a1[15];
  if ( v9 + v10 - 1 < a2 )
    return 8197;
  v11 = a1[10];
  v13 = a1[16];
  if ( v13 >= v11 )
  {
    if ( v11 + a1[11] - 1 >= v13 )
      goto LABEL_12;
    a1[17] = 0;
  }
  else
  {
    a1[17] = 0;
  }
  v13 = 0;
  a1[18] = 0;
  a1[16] = 0;
LABEL_12:
  if ( v13 == 0 )
  {
    v15 = a1[12];
    a1[18] = v9;
    a1[17] = v15;
    a1[16] = v11;
  }
  v14 = a1[18];
  if ( a2 < v14 )
  {
    if ( a2 - v9 < v14 - a2 )
    {
      v14 = v9;
      v16 = v11;
      a1[17] = a1[12];
      a1[18] = v9;
      a1[16] = v11;
    }
    else
    {
      v16 = a1[16];
    }
LABEL_18:
    v17 = a2 < v14;
    goto LABEL_19;
  }
  v16 = a1[16];
  if ( v14 >= a2 )
    goto LABEL_18;
  v50 = a1[11];
  v51 = v11 + v50 - 1;
  if ( v16 == v51 )
    goto LABEL_18;
  if ( v9 + v10 - a2 < a2 - v14 )
  {
    v52 = (unsigned int *)a1[8];
    a1[16] = v51;
    if ( v52 == NULL )
      return 10;
    v53 = ((u64)(v53) & 0xFFFFFFFF00000000ULL) | (u32)(v51 < v11);
    v53 = ((u64)(v53) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v52[2] == 0) << 32);
    v7 = 10;
    if ( v53 != 0 )
      return v7;
    v63 = v50 - 1;
    v54 = v52;
    if ( *v52 < v50 )
    {
      v55 = *v52;
      do
      {
        v54 = (unsigned int *)v54[7];
        v56 = v63 - v55 + 1;
        v63 -= v55;
        if ( v54 == NULL )
          return 10;
        v55 = *v54;
      }
      while ( *v54 < v56 );
    }
    v16 = a1[16];
    v62 = *(u32 *)(4 * v63 * v54[2] + v54[3]);
    v7 = 10;
    if ( v52[2] <= 1 || v16 < v11 )
      return v7;
    v57 = *v52;
    v58 = v52;
    v66 = v16 - v11;
    if ( *v52 < v16 - v11 + 1 )
    {
      do
      {
        v58 = (unsigned int *)v58[7];
        v59 = v66 - v57 + 1;
        v66 -= v57;
        if ( v58 == NULL )
          return 10;
        v57 = *v58;
      }
      while ( *v58 < v59 );
    }
    v65 = *(u32 *)(4 * (v66 * v58[2] + 1) + v58[3]);
    a1[17] = a1[12] + a1[13] - v62;
    v14 = v9 + v10 - v62 * v65;
    a1[18] = v14;
    goto LABEL_18;
  }
  v17 = a2 < v14;
LABEL_19:
  if ( v17 )
  {
    v32 = v16 - 1;
    v33 = v14;
    v7 = 8195;
    if ( v16 - 1 >= a1[4] )
      return v7;
    v34 = (unsigned int *)a1[8];
    v35 = 0;
    v36 = 0;
    for ( i = v32 - v11; ; --i )
    {
      v38 = v32 - v36;
      if ( v34 == NULL )
        break;
      v39 = (unsigned int *)a1[8];
      if ( v34[2] == 0 || v38 < v11 )
        break;
      v69 = i;
      if ( *v34 < i + 1 )
      {
        v40 = *v34;
        for ( j = i; ; j = v42 )
        {
          v39 = (unsigned int *)v39[7];
          v42 = j - v40;
          v43 = j - v40 + 1;
          v69 = v42;
          if ( v39 == NULL )
            break;
          v40 = *v39;
          if ( *v39 >= v43 )
            goto LABEL_53;
        }
        return 10;
      }
LABEL_53:
      v61 = *(u32 *)(4 * v69 * v39[2] + v39[3]);
      v44 = (unsigned int *)a1[8];
      if ( v34[2] <= 1 || v38 < v11 )
        return 10;
      v70 = i;
      if ( *v34 < i + 1 )
      {
        v45 = *v34;
        for ( k = i; ; k = v47 )
        {
          v44 = (unsigned int *)v44[7];
          v47 = k - v45;
          v48 = k - v45 + 1;
          v70 = v47;
          if ( v44 == NULL )
            break;
          v45 = *v44;
          if ( *v44 >= v48 )
            goto LABEL_59;
        }
        return 10;
      }
LABEL_59:
      v35 += v61;
      v64 = *(u32 *)(4 * (v70 * v44[2] + 1) + v44[3]);
      v33 -= v61 * v64;
      if ( a2 >= v33 )
      {
        if ( v64 == 0 )
          _break(0, 7u);
        *a3 = a1[17] - v35 + (a2 - v33) / v64;
        *a4 = a2 - (v33 + (a2 - v33) / v64 * v64);
        v49 = a1[17];
        a1[18] = v33;
        a1[16] = v32 - v36;
        a1[17] = v49 - v35;
        return 0;
      }
      if ( v32 < ++v36 )
        return 8195;
    }
    return 10;
  }
  v18 = v14;
  v7 = 8195;
  if ( v16 < a1[4] )
  {
    v19 = a1[11];
    v20 = 0;
    v21 = v16;
    if ( v16 < v11 + v19 )
    {
      v22 = (unsigned int *)a1[8];
      v23 = v16 - v11;
      while ( v22 != NULL && v22[2] != 0 && v21 >= v11 )
      {
        v67 = v23;
        v24 = (unsigned int *)a1[8];
        if ( *v22 < v23 + 1 )
        {
          v25 = *v22;
          do
          {
            v24 = (unsigned int *)v24[7];
            v26 = v67 - v25 + 1;
            v67 -= v25;
            if ( v24 == NULL )
              return 10;
            v25 = *v24;
          }
          while ( *v24 < v26 );
        }
        v60 = *(u32 *)(4 * v67 * v24[2] + v24[3]);
        v27 = (unsigned int *)a1[8];
        if ( v22[2] <= 1 || v21 < v11 )
          return 10;
        v68 = v23;
        if ( *v22 < v23 + 1 )
        {
          v28 = *v22;
          do
          {
            v27 = (unsigned int *)v27[7];
            v29 = v68 - v28 + 1;
            v68 -= v28;
            if ( v27 == NULL )
              return 10;
            v28 = *v27;
          }
          while ( *v27 < v29 );
        }
        v30 = *(u32 *)(4 * (v68 * v27[2] + 1) + v27[3]);
        if ( a2 < v18 + v60 * v30 )
        {
          if ( v30 == 0 )
            _break(0, 7u);
          *a3 = a1[17] + v20 + (a2 - v18) / v30;
          *a4 = a2 - (v18 + (a2 - v18) / v30 * v30);
          v31 = a1[17];
          a1[18] = v18;
          a1[16] = v21;
          a1[17] = v31 + v20;
          return 0;
        }
        ++v21;
        v18 += v60 * v30;
        v20 += v60;
        ++v23;
        if ( v21 >= v11 + v19 )
          return 8195;
      }
      return 10;
    }
    return 8195;
  }
  return v7;
}


