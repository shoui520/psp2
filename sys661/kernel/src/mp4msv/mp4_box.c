#include "mp4msv_internal.h"

u8 g_mp4_data[0x80] __attribute__((aligned(4))) = {
    0x64, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
u8 g_mp4_bss[0x80] __attribute__((aligned(8)));

const u32 g_mp4_state_table[360] = {
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0x00000004U, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000001U, 0x00000007U, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000001U, 0x00000006U, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0x00000005U, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0x00000002U, 0x00000001U, 0x00000002U,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000001U, 0x00000008U, 0x00000001U, 0x00000008U, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0x00000001U, 0x00000001U, 0x00000001U,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000001U, 0x00000001U, 0x00000001U, 0x00000001U, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0x00000000U, 0x00000001U, 0x00000000U,
    0x00000001U, 0x00000000U, 0x00000001U, 0x00000000U, 0x00000001U, 0x00000000U, 0x00000001U, 0x00000000U,
    0x00000001U, 0x00000000U, 0x00000001U, 0x00000000U, 0x00000001U, 0x00000000U, 0x00000001U, 0x00000000U,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000001U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
    0x00000000U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU, 0x00000001U, 0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFFU,
};
const u32 g_mp4_err_table[28] = {
    0xFFFFFFFFU, 0xFFFFFFFFU, 0x00000003U, 0x00000003U, 0x00000007U, 0x00000007U, 0x00000007U, 0x00000007U,
    0x00000009U, 0x00000009U, 0x00021798U, 0x00021798U, 0x00021778U, 0x000217CCU, 0x00021798U, 0x00021798U,
    0x00021798U, 0x00021798U, 0x00021798U, 0x00021798U, 0x00021798U, 0x000217F0U, 0x00021798U, 0x00021798U,
    0x00021798U, 0x00021798U, 0x00021778U, 0x000217F8U,
};

int mp4msv_9CA13D1A(int a1, int *a2)
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
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;

  if ( a1 > 0 )
  {
    v2 = *a2;
    v3 = a2[1];
    v4 = a2[2];
    v5 = a2[4];
    v6 = a2[5];
    v7 = a2[6];
    v8 = a2[7];
    v9 = a2[8];
    v10 = a2[9];
    v11 = a2[10];
    v12 = a2[11];
    v13 = a2[12];
    v14 = a2[13];
    v15 = a2[14];
    v16 = a2[16];
    v17 = a2[15];
    v18 = a2[3];
    MP4_BSS_U32(0x0) = v2;
    MP4_BSS_U32(0x4) = v3;
    MP4_BSS_U32(0x8) = v4;
    MP4_BSS_U32(0xC) = v18;
    MP4_BSS_U32(0x10) = v5;
    MP4_BSS_U32(0x14) = v6;
    MP4_BSS_U32(0x18) = v7;
    MP4_BSS_U32(0x1C) = v8;
    MP4_BSS_U32(0x20) = v9;
    MP4_BSS_U32(0x24) = v10;
    MP4_BSS_U32(0x28) = v11;
    MP4_BSS_U32(0x2C) = v12;
    MP4_BSS_U32(0x30) = v13;
    MP4_BSS_U32(0x34) = v14;
    MP4_BSS_U32(0x38) = v15;
    MP4_BSS_U32(0x3C) = v17;
    MP4_BSS_U32(0x40) = v16;
  }
  return 0;
}


int mp4msv_2A2D949A()
{
  return 1;
}


int *mp4msv_4A7AD789()
{
  return &MP4_DATA_U32(0x0);
}


int mp4msv_3C2183C7(int a1, int *a2)
{
  int v2;
  int v3;
  int v4;
  int v5;

  if ( a1 > 0 )
  {
    v2 = a2[4];
    v3 = a2[1];
    v4 = a2[2];
    v5 = a2[3];
    MP4_DATA_U32(0x0) = *a2;
    MP4_DATA_U32(0x10) = v2;
    MP4_DATA_U32(0x4) = v3;
    MP4_DATA_U32(0x8) = v4;
    MP4_DATA_U32(0xC) = v5;
  }
  return 0;
}


int mp4_box_fn_2(int a1)
{
  return ((int (*)())MP4_BSS_U32(0x8))(a1);
}


int mp4_box_fn_3(int a1, int a2, int a3, int a4, int a5)
{
  return ((int (*)())MP4_BSS_U32(0xC))(a1, a3, a4, a5);
}


int mp4_box_fn_4(int a1)
{
  return ((int (*)())MP4_BSS_U32(0x10))(a1);
}


int mp4_box_fn_5(int a1, int a2, int a3)
{
  return ((int (*)())MP4_BSS_U32(0x14))(a1, a2, a3);
}


int mp4_box_fn_6(int a1, int a2, int a3)
{
  return ((int (*)())MP4_BSS_U32(0x18))(a1, a2, a3);
}


int mp4_box_fn_7(int a1)
{
  return ((int (*)())MP4_BSS_U32(0x1C))(a1);
}


int mp4_box_fn_8(int a1)
{
  return ((int (*)())MP4_BSS_U32(0x20))(a1);
}


int mp4_box_fn_9()
{
  return ((int (*)())MP4_BSS_U32(0x24))();
}


int mp4_box_fn_10(int a1, int a2, int a3, int a4)
{
  return ((int (*)())MP4_BSS_U32(0x28))(a1, a2, a3, a4);
}


int mp4_box_fn_11(int a1, int a2, int a3)
{
  return ((int (*)())MP4_BSS_U32(0x30))(a1, a2, a3);
}


int mp4_box_fn_12(int a1)
{
  return ((int (*)())MP4_BSS_U32(0x40))(a1);
}


int mp4msv_F595F917(int *a1)
{
  int *v2;
  int v3;
  int v4;
  int v5;
  s32 v6;
  s32 v7;
  int v8;
  u32 *v10;
  int v11;
  int v12;
  int v13;
  int v14;

  v2 = mp4msv_4A7AD789();
  v3 = 6;
  v4 = *v2;
  v5 = *v2 * v2[2];
  v6 = v5 * v2[3];
  v7 = *v2 * v2[1];
  if ( v6 >= *v2 )
  {
    v8 = mp4_sample_fn_5(12);
    *a1 = v8;
    v3 = 1;
    if ( v8 != 0 )
    {
      *(u32 *)(v8 + 4) = mp4_sample_fn_5(88);
      v10 = *(u32 **)(*a1 + 4);
      if ( v10 != NULL )
      {
        v10[20] = 3;
        v10[14] = 0;
        v10[15] = 0;
        v10[16] = 0;
        v10[17] = 0;
        v10[18] = 0;
        v10[19] = 0;
        v10[4] = v4;
        v10[6] = v5;
        v10[7] = v6;
        v10[5] = mp4_min_s32(v6, v7);
        v10[10] = 0;
        v10[12] = 0;
        v10[13] = 0;
        v12 = mp4_box_fn_14(v10 + 21);
        v3 = 0;
        if ( v12 != 0 )
        {
          mp4_track_fn_65(*(u32 *)(*a1 + 4), *a1, v11);
          mp4_track_fn_65(*a1, v13, v14);
          *a1 = 0;
          return v12;
        }
      }
      else
      {
        mp4_track_fn_65(*a1, 0, 84);
        v3 = 1;
        *a1 = 0;
      }
    }
  }
  return v3;
}


int mp4_box_fn_13(int *a1)
{
  int v1;
  int v3;
  int v4;

  v1 = 4;
  if ( a1 != NULL )
  {
    v3 = *a1;
    v1 = 0;
    if ( *a1 != 0 )
    {
      if ( *(u32 *)(v3 + 280) != 0 )
      {
        v4 = mp4_box_fn_108(*(u32 *)(v3 + 280));
        v1 = 10;
        if ( v4 != 0 )
          return v1;
        *(u32 *)(*a1 + 280) = 0;
      }
      mp4_codec_fn_42(a1);
      if ( *a1 != 0 )
        mp4_track_fn_64(a1, 1936549485);
      return 0;
    }
  }
  return v1;
}


int mp4msv_68FBFBE1(int a1)
{
  int v1;
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;

  v1 = 4;
  if ( a1 == 0 )
    return v1;
  v14 = a1;
  v2 = mp4_track_fn_63(a1, &v12);
  v1 = 10;
  if ( v2 != 0 )
    return v1;
  if ( *(u32 *)(v12 + 280) != 0 )
  {
    v10 = mp4_track_fn_62(v14, &v13);
    v1 = 10;
    if ( v10 != 0 )
      return v1;
    v11 = mp4_track_fn_3(*(u32 *)(v12 + 280), v13);
    v1 = 10;
    if ( v11 != 0 )
      return v1;
  }
  v3 = v14;
  if ( *(u32 *)(v14 + 540) == 0 && *(u32 *)(v14 + 536) == 0 )
  {
LABEL_17:
    mp4_track_fn_64(&v14, 1836019558);
    return 0;
  }
  v4 = *(u32 *)(v14 + 540);
  if ( v4 == 0 )
    return 0;
  v5 = *(u32 *)(v4 + 264);
  if ( v5 == v14 )
  {
    v3 = v14;
    *(u32 *)(v4 + 264) = *(u32 *)(v14 + 536);
LABEL_16:
    v9 = *(u32 *)(v3 + 540);
    *(u32 *)(v3 + 536) = 0;
    --*(u32 *)(v9 + 260);
    goto LABEL_17;
  }
  v6 = *(u32 *)(v4 + 264);
  if ( v5 == 0 )
    goto LABEL_16;
  do
  {
    v7 = *(u32 *)(v6 + 536);
    if ( v7 == v14 )
    {
      v3 = v14;
      *(u32 *)(v6 + 536) = *(u32 *)(v7 + 536);
      goto LABEL_16;
    }
    v6 = *(u32 *)(v6 + 536);
  }
  while ( v7 != 0 );
  return 8195;
}


int mp4msv_DEBFCBCA(s64 a1, int a2)
{
  int v2;
  u32 *v4;
  int v5;
  u32 *v6;
  int v7;
  int v8;
  unsigned int v9;
  int *v10;
  int i;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  unsigned int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  unsigned int v22;
  int v23;
  int v24;
  s16 v25;
  unsigned int v27;
  int v28;
  int v29;

  v2 = 4;
  if ( a1 != 0 && ((u32)a1 == 0 || HIDWORD(a1) != 0 || *(u32 *)a1 != 0) )
  {
    if ( HIDWORD(a1) != 0 )
    {
      v5 = *(u32 *)(HIDWORD(a1) + 688);
      v2 = 8194;
      if ( v5 == 0 )
        return v2;
      v6 = (u32 *)(HIDWORD(a1) + 688);
      v7 = v5 + 44;
      if ( *(u32 *)(v5 + 44) == 0 )
        return v2;
    }
    else
    {
      v4 = *(u32 **)a1;
      v2 = 8195;
      if ( *v4 == 0 )
        return v2;
      v29 = v4[42];
      v2 = 8194;
      if ( v29 == 0 )
        return v2;
      v6 = v4 + 42;
      if ( *(u32 *)(v29 + 44) == 0 )
        return v2;
      v7 = v29 + 44;
    }
    v8 = 0;
    v9 = 0;
    if ( *(u16 *)(v7 + 8) == 0 )
      return 8194;
    v10 = (int *)(*(u32 *)(v7 + 16) + 4);
    for ( i = *v10; ; i = *v10 )
    {
      ++v9;
      v12 = v8 + 1;
      v10 += 10;
      if ( i == a2 )
        v8 = (u16)(v8 + 1);
      if ( v9 >= *(u16 *)(v7 + 8) )
        break;
    }
    v13 = *(u16 *)(v7 + 8);
    if ( v8 == 0 )
      return 8194;
    if ( v13 == v8 )
    {
      v27 = 0;
      v28 = 0;
      do
      {
        ++v27;
        mp4_sample_fn_8(*(u32 *)(v7 + 16) + v28 + 12, v12);
        v28 += 40;
      }
      while ( v27 < *(u16 *)(v7 + 8) );
      v16 = 0;
      v18 = 0;
    }
    else
    {
      v14 = mp4_sample_fn_5(40 * (v13 - v8));
      v16 = v14;
      v2 = 1;
      if ( v14 == 0 )
        return v2;
      v17 = 0;
      v18 = 0;
      if ( *(u16 *)(v7 + 8) != 0 )
      {
        v19 = 0;
        v20 = v14;
        do
        {
          v21 = *(u32 *)(v7 + 16);
          if ( *(u32 *)(v19 + v21 + 4) == a2 )
          {
            mp4_sample_fn_8(v19 + v21 + 12, 40);
          }
          else
          {
            ++v18;
            mp4_box_fn_23(v20, 40, v19 + v21, 40);
            v20 += 40;
          }
          v22 = *(u16 *)(v7 + 8);
          v15 = ++v17 < v22;
          v19 += 40;
        }
        while ( v17 < v22 );
      }
    }
    mp4_track_fn_65(*(u32 *)(v7 + 16), v12, v15);
    v25 = *(u16 *)(v7 + 8);
    *(u32 *)(v7 + 12) = v18;
    *(u16 *)(v7 + 8) = v25 - v8;
    *(u32 *)(v7 + 16) = v16;
    if ( v16 == 0 )
    {
      *(u32 *)(v7 + 4) = 0;
      *(u32 *)v7 = 0;
      v2 = 0;
      if ( *(u32 *)(*v6 + 24) != 0 )
        return v2;
      mp4_track_fn_65(*v6, v23, v24);
      *v6 = 0;
    }
    return 0;
  }
  return v2;
}


int mp4msv_576C693B(s64 a1, int a2)
{
  int v2;
  u32 *v4;
  int v5;
  u32 *v6;
  int v7;
  int v8;
  unsigned int v9;
  unsigned int v10;
  int *v11;
  int i;
  int v13;
  int v14;
  int v15;
  int v16;
  u16 v17;
  unsigned int v18;
  int v19;
  int v20;
  int v21;
  int j;
  int v23;
  int v24;
  int v25;
  s16 v26;
  int v28;

  v2 = 4;
  if ( a1 != 0 && ((u32)a1 == 0 || HIDWORD(a1) != 0 || *(u32 *)a1 != 0) )
  {
    if ( HIDWORD(a1) != 0 )
    {
      v5 = *(u32 *)(HIDWORD(a1) + 688);
      v2 = 8194;
      if ( v5 == 0 )
        return v2;
      v6 = (u32 *)(HIDWORD(a1) + 688);
      v7 = v5 + 24;
      if ( *(u32 *)(v5 + 24) == 0 )
        return v2;
    }
    else
    {
      v4 = *(u32 **)a1;
      v2 = 8195;
      if ( *v4 == 0 )
        return v2;
      v28 = v4[42];
      v2 = 8194;
      if ( v28 == 0 )
        return v2;
      v6 = v4 + 42;
      if ( *(u32 *)(v28 + 36) == 0 )
        return v2;
      v7 = v28 + 24;
    }
    v8 = 0;
    v9 = 0;
    if ( *(u16 *)(v7 + 8) == 0 )
      return 8194;
    v10 = *(u16 *)(v7 + 8);
    v11 = (int *)(*(u32 *)(v7 + 16) + 4);
    for ( i = *v11; ; i = *v11 )
    {
      v13 = ++v9 < v10;
      v11 += 11;
      if ( i == a2 )
        v8 = (u16)(v8 + 1);
      if ( v9 >= v10 )
        break;
    }
    v14 = *(u16 *)(v7 + 8);
    if ( v8 == 0 )
      return 8194;
    if ( v14 == v8 )
    {
      v16 = 0;
      v19 = 0;
    }
    else
    {
      v15 = mp4_sample_fn_5(44 * (v14 - v8));
      v16 = v15;
      v2 = 1;
      if ( v15 == 0 )
        return v2;
      v17 = *(u16 *)(v7 + 8);
      v18 = 0;
      v19 = 0;
      if ( v17 != 0 )
      {
        v20 = 0;
        v21 = v15;
        for ( j = *(u32 *)(v7 + 16); ; j = *(u32 *)(v7 + 16) )
        {
          ++v18;
          v9 = v20 + j;
          v23 = *(u32 *)(v20 + j + 4);
          v20 += 44;
          v13 = 44;
          if ( v23 != a2 )
          {
            ++v19;
            mp4_box_fn_23(v21, 44, v9, 44);
            v17 = *(u16 *)(v7 + 8);
            v21 += 44;
          }
          if ( v18 >= v17 )
            break;
        }
      }
    }
    mp4_track_fn_65(*(u32 *)(v7 + 16), v13, v9);
    v26 = *(u16 *)(v7 + 8);
    *(u32 *)(v7 + 12) = v19;
    *(u16 *)(v7 + 8) = v26 - v8;
    *(u32 *)(v7 + 16) = v16;
    if ( v16 == 0 )
    {
      *(u32 *)(v7 + 4) = 0;
      *(u32 *)(v7 + 12) = 0;
      v2 = 0;
      if ( *(u32 *)(*v6 + 44) != 0 )
        return v2;
      mp4_track_fn_65(*v6, v24, v25);
      *v6 = 0;
    }
    return 0;
  }
  return v2;
}


int mp4msv_3D8D41A0(int a1, int a2)
{
  int v2;
  u32 *v3;

  v2 = 4;
  if ( a1 != 0 )
  {
    v3 = *(u32 **)(a1 + 4);
    if ( v3 != NULL )
    {
      *v3 = a2;
      v2 = 0;
      v3[10] = 1;
      v3[9] = a2;
    }
  }
  return v2;
}


int mp4msv_9D56A7CC(int *a1)
{
  int result;
  int v3;
  int v4;
  int v5;
  int v6;

  result = 4;
  if ( a1 != NULL )
  {
    mp4_box_fn_15(*(u32 *)(a1[1] + 84));
    if ( a1[1] != 0 )
      mp4_track_fn_65(a1[1], v3, v4);
    mp4_box_fn_13(a1);
    mp4_track_fn_65(a1, v5, v6);
    return 0;
  }
  return result;
}


int mp4msv_A824645B(int a1, int a2, int a3, int a4)
{
  int result;
  int v7;
  int v8;
  u8 v9[4];
  u8 v10[4];

  result = 6;
  if ( a2 != 0 )
  {
    mp4_track_fn_80(a1, a3, &v7, &v8);
    mp4_track_fn_81(v7, v8, a2, v9, a4, v10);
    return 0;
  }
  return result;
}


int mp4msv_98ADC323(int a1, int a2, int a3)
{
  int v3;

  v3 = 4;
  if ( a1 != 0 )
  {
    v3 = mp4_box_fn_16(*(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(a1 + 700) + 152) + 276) + 4) + 84), 14);
    if ( v3 == 0 )
    {
      v3 = mp4_box_fn_35(a1, a2, a3);
      if ( v3 == 0 )
      {
        mp4_box_fn_17(*(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(a1 + 700) + 152) + 276) + 4) + 84));
        return 0;
      }
    }
  }
  return v3;
}


int mp4msv_5087548E(int a1)
{
  int v2;

  v2 = 4;
  if ( a1 != 0 )
  {
    v2 = mp4_box_fn_16(*(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(a1 + 700) + 152) + 276) + 4) + 84), 13);
    if ( v2 == 0 )
    {
      v2 = mp4_box_fn_36(a1);
      if ( v2 == 0 )
      {
        mp4_box_fn_17(*(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(a1 + 700) + 152) + 276) + 4) + 84));
        return 0;
      }
    }
  }
  return v2;
}


int mp4msv_8A0FD93F(int a1, int a2, int a3, int a4, int a5)
{
  int v5;

  v5 = 4;
  if ( a1 != 0 )
  {
    v5 = mp4_box_fn_16(*(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(a1 + 700) + 152) + 276) + 4) + 84), 15);
    if ( v5 == 0 )
    {
      v5 = mp4_box_fn_31(a1, a2, a3, a4, a5);
      if ( v5 == 0 )
      {
        mp4_box_fn_17(*(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(a1 + 700) + 152) + 276) + 4) + 84));
        return 0;
      }
    }
  }
  return v5;
}


int mp4msv_46186C26(u32 *a1)
{
  int v1;

  v1 = 4;
  if ( a1 != NULL && *a1 != 0 )
    return mp4_box_fn_32();
  return v1;
}


int mp4msv_BD23D34A(u32 *a1)
{
  int v1;

  v1 = 4;
  if ( a1 != NULL && *a1 != 0 )
    return mp4_box_fn_25();
  return v1;
}


int mp4msv_D8BA6AFE(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_33();
  return v1;
}


int mp4msv_0C0843B0(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_26();
  return v1;
}


int mp4msv_089364A9(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_27();
  return v1;
}


int mp4msv_E5C33D61(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_28();
  return v1;
}


int mp4msv_21BD7274(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_29();
  return v1;
}


int mp4msv_9501780A(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_30();
  return v1;
}


int mp4msv_21BE7D96(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_34();
  return v1;
}


int mp4msv_0D6843EA(int a1, int a2, int a3)
{
  int result;
  unsigned int v4;

  result = 4;
  if ( a1 != 0 )
  {
    result = 6;
    if ( a2 == 0 )
    {
      if ( a3 != 0 )
        v4 = *(u32 *)(a1 + 8) | 1;
      else
        v4 = *(u32 *)(a1 + 8) & 0xFFFFFFFE;
      *(u32 *)(a1 + 8) = v4;
      return 0;
    }
  }
  return result;
}


int mp4msv_67AF9E0F(int a1, int a2, int a3)
{
  int v3;
  int v7;

  v3 = 4;
  if ( a1 != 0 )
  {
    v7 = *(u32 *)(a1 + 4);
    if ( v7 != 0 )
    {
      v3 = mp4_box_fn_16(*(u32 *)(v7 + 84), 7);
      if ( v3 == 0 )
      {
        v3 = mp4_box_fn_39(a1, a2, a3);
        if ( v3 == 0 )
        {
          mp4_box_fn_17(*(u32 *)(*(u32 *)(a1 + 4) + 84));
          return 0;
        }
      }
    }
  }
  return v3;
}


int mp4msv_BE203EA3(int a1, int a2, int a3)
{
  int v3;
  int v7;

  v3 = 4;
  if ( a1 != 0 )
  {
    v7 = *(u32 *)(a1 + 4);
    if ( v7 != 0 )
    {
      v3 = mp4_box_fn_16(*(u32 *)(v7 + 84), 8);
      if ( v3 == 0 )
      {
        v3 = mp4_box_fn_39(a1, a2, a3);
        if ( v3 == 0 )
        {
          mp4_box_fn_17(*(u32 *)(*(u32 *)(a1 + 4) + 84));
          return 0;
        }
      }
    }
  }
  return v3;
}


int mp4msv_CFF25FAC(int a1, int a2)
{
  int v2;
  int v5;

  v2 = 4;
  if ( a1 != 0 )
  {
    v5 = *(u32 *)(a1 + 4);
    if ( v5 != 0 )
    {
      v2 = mp4_box_fn_16(*(u32 *)(v5 + 84), 9);
      if ( v2 == 0 )
      {
        v2 = mp4_box_fn_40(a1, a2);
        if ( v2 == 0 )
        {
          mp4_box_fn_17(*(u32 *)(*(u32 *)(a1 + 4) + 84));
          return 0;
        }
      }
    }
  }
  return v2;
}


int mp4msv_3149D53B(int a1, int a2, int a3)
{
  int v3;
  int v7;

  v3 = 4;
  if ( a1 != 0 )
  {
    v7 = *(u32 *)(a1 + 4);
    if ( v7 != 0 )
    {
      v3 = mp4_box_fn_16(*(u32 *)(v7 + 84), 10);
      if ( v3 == 0 )
      {
        v3 = mp4_box_fn_41(a1, a2, a3);
        if ( v3 == 0 )
        {
          mp4_box_fn_17(*(u32 *)(*(u32 *)(a1 + 4) + 84));
          return 0;
        }
      }
    }
  }
  return v3;
}


int mp4msv_27412E93(int a1, int a2, int a3, int a4)
{
  int v5;

  v5 = 4;
  if ( a1 != 0 )
  {
    if ( a2 != 0 )
      return mp4_box_fn_90(a1, a3, a4);
    v5 = mp4_box_fn_89();
    if ( v5 == 0 )
      return mp4_box_fn_90(a1, a3, a4);
  }
  return v5;
}


int mp4msv_E641D779(int a1, unsigned int a2, int a3, int a4, int a5)
{
  int v6;
  int v7;
  u16 v10;

  v6 = 4;
  v7 = a2;
  if ( a1 != 0 )
  {
    if ( a3 != 0 )
      return mp4_box_fn_54(a1, v7, a4, a5);
    v6 = mp4_box_fn_52();
    if ( v6 == 0 )
    {
      a4 = v10;
      return mp4_box_fn_54(a1, v7, a4, a5);
    }
  }
  return v6;
}


int mp4msv_BE558D75(int a1, unsigned int a2, int a3, int a4, int a5)
{
  int v6;
  int v7;
  u16 v10;

  v6 = 4;
  v7 = a2;
  if ( a1 != 0 )
  {
    if ( a3 != 0 )
      return mp4_box_fn_55(a1, v7, a4, a5);
    v6 = mp4_box_fn_53();
    if ( v6 == 0 )
    {
      a4 = v10;
      return mp4_box_fn_55(a1, v7, a4, a5);
    }
  }
  return v6;
}


int mp4msv_CE9C6D39(int a1, int a2, int a3, int a4)
{
  int v5;

  v5 = 4;
  if ( a1 != 0 )
  {
    v5 = 6;
    if ( a4 != 0 )
    {
      if ( a2 != 0 )
        return mp4_box_fn_93(a1, a3, a4);
      v5 = mp4_box_fn_92();
      if ( v5 == 0 )
        return mp4_box_fn_93(a1, a3, a4);
    }
  }
  return v5;
}


int mp4msv_5565A318(s64 a1, int a2, int a3, int a4, u16 *a5)
{
  int v6;
  int v7;
  s16 v9;

  v6 = 4;
  if ( a1 != 0 && ((u32)a1 == 0 || HIDWORD(a1) != 0 || *(u32 *)a1 != 0) )
  {
    v7 = mp4_box_fn_57();
    if ( a5 != NULL )
      *a5 = v9;
    return v7;
  }
  return v6;
}


int mp4msv_9076AA02(s64 a1, int a2, int a3, int a4, int a5, unsigned int a6, int a7)
{
  u32 *v7;
  int v8;
  int v9;

  v7 = (u32 *)a1;
  v8 = 4;
  if ( a1 != 0 )
  {
    v8 = 6;
    if ( a3 != 0 && a4 != 0 )
    {
      v9 = (u32)a1 != 0 && HIDWORD(a1) == 0;
      if ( a7 != 0 )
      {
        if ( !v9 || (v8 = 4, *v7 != 0) )
        {
          v8 = 6;
          if ( a5 == 0 || a6 >= 0x20 )
            return mp4_box_fn_62(v7);
        }
      }
    }
  }
  return v8;
}


int mp4msv_6AE9B944(s64 a1, int a2, int a3, int a4, int a5, int a6, unsigned int a7, int a8)
{
  int v8;

  v8 = 4;
  if ( a1 != 0 )
  {
    v8 = 6;
    if ( a3 != 0 && a4 != 0 && a5 != 0 && a8 != 0 )
    {
      if ( (u32)a1 == 0 || HIDWORD(a1) != 0 || (v8 = 4, *(u32 *)a1 != 0) )
      {
        v8 = 6;
        if ( a6 == 0 || a7 >= 0x20 )
          return mp4_box_fn_63(a1);
      }
    }
  }
  return v8;
}


int mp4msv_AADD18E7(int a1, unsigned int a2, int a3, int a4, int a5)
{
  int v7;
  int v8;
  int v11;

  v7 = a2;
  v8 = 4;
  if ( a1 != 0 )
  {
    if ( a3 != 0 )
      return mp4_box_fn_70(a1, v7, a4, a5);
    v8 = mp4_box_fn_69();
    if ( v8 == 0 )
    {
      a4 = v11;
      return mp4_box_fn_70(a1, v7, a4, a5);
    }
  }
  return v8;
}


int mp4msv_E4AFDAD3(int *a1, u32 *a2)
{
  int v2;
  int v3;
  int v4;

  v2 = 4;
  if ( a1 != NULL )
  {
    v3 = *a1;
    if ( v3 != 0 )
    {
      v2 = 9;
      if ( *(u64 *)(v3 + 312) != 0 )
      {
        v2 = 6;
        if ( a2 != NULL )
        {
          v4 = *(u32 *)(v3 + 316);
          v2 = 0;
          *a2 = *(u32 *)(v3 + 312);
          a2[1] = v4;
        }
      }
    }
  }
  return v2;
}


int mp4msv_4154992A(u32 *a1, u32 *a2, u32 *a3)
{
  int v3;

  v3 = 4;
  if ( a1 != NULL )
  {
    v3 = 6;
    if ( a2 != NULL && a3 != NULL )
    {
      v3 = 9;
      if ( (unsigned int)(a1[1] - 2564) < 2 )
      {
        v3 = 0;
        *a2 = a1[3];
        *a3 = a1[5];
      }
    }
  }
  return v3;
}


int mp4msv_C38F9765(u32 *a1, unsigned int a2, u32 *a3, u32 *a4, u32 *a5, u32 *a6)
{
  int v6;
  u32 *v7;
  int v8;
  int v10;

  v6 = 4;
  if ( a1 != NULL )
  {
    v6 = 6;
    if ( a2 != 0 )
    {
      v6 = 9;
      if ( a1[5] >= a2 )
      {
        v6 = 6;
        if ( a3 != NULL && a4 != NULL && a5 != NULL && a6 != NULL )
        {
          v7 = (u32 *)a1[7];
          v8 = a1[6] + 12 * a2;
          if ( *(u32 *)(v8 - 12) == v7[7] && v7[28] >= *(u32 *)(v8 - 8) )
          {
            *a3 = v7;
            v6 = 0;
            v10 = v7[29] + 20 * *(u32 *)(v8 - 8);
            *a4 = *(u32 *)(v10 - 20);
            *a5 = *(u32 *)(v10 - 16);
            *a6 = *(u32 *)(v10 - 12);
          }
          else
          {
            return 9;
          }
        }
      }
    }
  }
  return v6;
}


int mp4msv_63752099(int a1, unsigned int a2, u32 *a3, int a4)
{
  int v4;
  int v7;
  unsigned int v8;
  unsigned int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v18;
  u32 v19[4];
  s16 v20;
  s16 v21;

  v4 = 4;
  if ( a1 != 0 )
  {
    v4 = 6;
    if ( a2 != 0 && a3 != NULL )
    {
      v7 = *(u32 *)(a1 + 88);
      v4 = 9;
      if ( *(u32 *)(a1 + 84) >= a2 )
      {
        v8 = a2 - 1;
        v9 = 0;
        if ( a2 != 1 )
        {
          while ( 1 )
          {
            ++v9;
            v4 = 9;
            if ( v7 == 0 )
              break;
            v7 = *(u32 *)(v7 + 32);
            if ( v9 >= v8 )
              goto LABEL_8;
          }
        }
        else
        {
LABEL_8:
          v4 = 9;
          if ( v7 != 0 && *(u32 *)(v7 + 4) == 1347307296 )
          {
            v10 = mp4_box_fn_86(v7, &v18, 1347307296, a4, 9);
            v4 = 9;
            if ( v10 == 0 )
            {
              v11 = mp4msv_2A296DDA(a1, *(u16 *)(v7 + 14), v19);
              v4 = 9;
              if ( v11 == 0 )
              {
                v12 = v19[0];
                v13 = v19[2];
                v14 = v20;
                v15 = v21;
                v16 = v18;
                a3[5] = v19[3];
                a3[1] = v12;
                a3[2] = v14;
                a3[3] = v15;
                a3[4] = v13;
                *a3 = v16;
                return 0;
              }
            }
          }
        }
      }
    }
  }
  return v4;
}


int mp4msv_F50268BF(int a1, u32 *a2)
{
  int result;
  int v3;
  int i;

  result = 4;
  if ( a1 != 0 )
  {
    result = 6;
    if ( a2 != NULL )
    {
      v3 = *(u32 *)(a1 + 64);
      for ( i = 0; v3 != 0; ++i )
        v3 = *(u32 *)(v3 + 20);
      *a2 = i;
      return 0;
    }
  }
  return result;
}


int mp4msv_2A296DDA(int a1, int a2, u32 *a3)
{
  int result;
  unsigned int v4;
  int v5;
  unsigned int v6;

  result = 4;
  if ( a1 != 0 )
  {
    result = 6;
    if ( a2 != 0 && a3 != NULL )
    {
      v4 = a2 - 1;
      v5 = *(u32 *)(a1 + 64);
      v6 = 0;
      if ( a2 != 1 )
      {
        while ( 1 )
        {
          ++v6;
          result = 9;
          if ( v5 == 0 )
            break;
          v5 = *(u32 *)(v5 + 20);
          if ( v6 >= v4 )
            goto LABEL_7;
        }
      }
      else
      {
LABEL_7:
        result = 9;
        if ( v5 != 0 )
        {
          *a3 = v5;
          return 0;
        }
      }
    }
  }
  return result;
}


int mp4msv_A41C1F5F(int *a1, u32 *a2)
{
  int v2;
  int result;
  int v4;
  u32 *v5;

  v2 = 0;
  result = 4;
  if ( a1 != NULL )
  {
    result = 6;
    if ( a2 != NULL )
    {
      v4 = *a1;
      result = 4;
      if ( v4 != 0 )
      {
        v5 = *(u32 **)(v4 + 328);
        if ( v5 != NULL )
        {
          do
          {
            v5 = (u32 *)*v5;
            ++v2;
          }
          while ( v5 != NULL );
          *a2 = v2;
        }
        else
        {
          *a2 = 0;
        }
        return 0;
      }
    }
  }
  return result;
}


int mp4msv_B919743C(int *a1, int (*a2)(int a1, int *a2), u32 *a3, u32 *a4, int a5)
{
  int v5;
  s64 v6;
  int v7;
  u32 *v9;
  int (*i)(int, int *);
  int v11;
  int v12;
  unsigned int v13;
  int v14;
  int v15;

  v5 = 4;
  if ( a1 != NULL )
  {
    if ( a2 == NULL || a3 == NULL )
      return 6;
    v6 = ((u64)(v6) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(a5 == 0) << 32);
    v6 = ((u64)(v6) & 0xFFFFFFFF00000000ULL) | (u32)(a4 == NULL);
    if ( v6 != 0 )
    {
      return 6;
    }
    else
    {
      v7 = *a1;
      v5 = 4;
      if ( v7 != 0 )
      {
        v9 = *(u32 **)(v7 + 328);
        if ( v9 != NULL )
        {
          for ( i = mp4_box_fn_1; i != a2; i = (int (*)(int, int *))((char *)i + 1) )
          {
            v9 = (u32 *)*v9;
            if ( v9 == NULL )
              return 9;
          }
          v11 = v9[1];
          v12 = v9[3];
          v13 = 0;
          *a3 = v9[2];
          a3[1] = v12;
          *a4 = v11;
          do
          {
            v14 = 4 * v13;
            v15 = v9[v13++ + 4];
            *(u32 *)(v14 + a5) = v15;
            v5 = 0;
          }
          while ( v13 < 4 );
        }
        else
        {
          return 9;
        }
      }
    }
  }
  return v5;
}


int mp4msv_032F3500(u32 *a1, u32 *a2, u32 *a3)
{
  u32 count = 0;
  u32 *node;

  if (a1 == NULL)
    return 4;
  if (a2 == NULL || a3 == NULL)
    return 6;
  if (*a1 == 0)
    return 4;
  node = *(u32 **)(*a1 + 0x148);
  while (node != NULL) {
    if (node[4] == a2[0] && node[5] == a2[1] && node[6] == a2[2] && node[7] == a2[3])
      count++;
    node = (u32 *)node[0];
  }
  *a3 = count;
  return 0;
}

int mp4msv_509B3BAC(int *a1, u32 *a2, int a3, u32 *a4, u32 *a5)
{
  int v5;
  int v6;
  s64 v7;
  int v8;
  u32 *v10;
  int i;
  int v12;
  int v13;

  v5 = 0;
  v6 = 4;
  if ( a1 != NULL )
  {
    v6 = 6;
    if ( a2 != NULL && a3 != 0 )
    {
      v7 = ((u64)(v7) & 0xFFFFFFFF00000000ULL) | (u32)(a4 == NULL);
      v7 = ((u64)(v7) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(a5 == NULL) << 32);
      if ( v7 != 0 )
      {
        return 6;
      }
      else
      {
        v8 = *a1;
        v6 = 4;
        if ( v8 != 0 )
        {
          v10 = *(u32 **)(v8 + 328);
          if ( v10 != NULL )
          {
            for ( i = v10[4]; ; i = v10[4] )
            {
              if ( *a2 == i )
              {
                if ( a2[1] == v10[5] )
                {
                  if ( a2[2] == v10[6] )
                  {
                    if ( a2[3] == v10[7] )
                    {
                      if ( ++v5 == a3 )
                      {
                        v12 = v10[2];
                        v13 = v10[3];
                        *a5 = v10[1];
                        *a4 = v12;
                        a4[1] = v13;
                        return 0;
                      }
                      v10 = (u32 *)*v10;
                    }
                    else
                    {
                      v10 = (u32 *)*v10;
                    }
                  }
                  else
                  {
                    v10 = (u32 *)*v10;
                  }
                }
                else
                {
                  v10 = (u32 *)*v10;
                }
              }
              else
              {
                v10 = (u32 *)*v10;
              }
              if ( v10 == NULL )
                return 9;
            }
          }
          return 9;
        }
      }
    }
  }
  return v6;
}


int mp4msv_07C60A23(u32 *a1)
{
  int v1;

  v1 = 4;
  if ( a1 != NULL && *a1 != 0 )
    return mp4_box_fn_75();
  return v1;
}


int mp4msv_8FD8E0A4(u32 *a1)
{
  int v1;

  v1 = 4;
  if ( a1 != NULL && *a1 != 0 )
    return mp4_box_fn_76();
  return v1;
}


int mp4msv_E042AB3C(u32 *a1)
{
  int v1;

  v1 = 4;
  if ( a1 != NULL && *a1 != 0 )
    return mp4_box_fn_77();
  return v1;
}


int mp4msv_182FF541(u32 *a1)
{
  int v1;

  v1 = 4;
  if ( a1 != NULL && *a1 != 0 )
    return mp4_box_fn_78();
  return v1;
}


int mp4msv_3D57BA52(u32 *a1)
{
  int v1;

  v1 = 4;
  if ( a1 != NULL && *a1 != 0 )
    return mp4_box_fn_79();
  return v1;
}


int mp4msv_0C06DACB(u32 *a1)
{
  int v1;

  v1 = 4;
  if ( a1 != NULL && *a1 != 0 )
    return mp4_box_fn_42();
  return v1;
}


int mp4msv_8D564A92(u32 *a1)
{
  int v1;

  v1 = 4;
  if ( a1 != NULL && *a1 != 0 )
    return mp4_box_fn_80();
  return v1;
}


int mp4msv_A55AF94C(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_43();
  return v1;
}


int mp4msv_6B94A29D(s64 a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 && ((u32)a1 == 0 || *(u32 *)a1 != 0) )
    return mp4_box_fn_44();
  return v1;
}


int mp4msv_D3505C12(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_45();
  return v1;
}


int mp4msv_A2AE6F30(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_46();
  return v1;
}


int mp4msv_54410730(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_81();
  return v1;
}


int mp4msv_EE62F96D(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_47();
  return v1;
}


int mp4msv_9BB5E338(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_82();
  return v1;
}


int mp4msv_FF9E6F58(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_48();
  return v1;
}


int mp4msv_3F6B6C59(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_83();
  return v1;
}


int mp4msv_5EE48079(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_84();
  return v1;
}


int mp4msv_162503F3(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_49();
  return v1;
}


int mp4msv_A9FD62E2(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_85();
  return v1;
}


int mp4msv_6D619263(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_50();
  return v1;
}


int mp4msv_2BAF7C25(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_87();
  return v1;
}


int mp4msv_A418509D(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_88();
  return v1;
}


int mp4msv_8C998600(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_89();
  return v1;
}


int mp4msv_35144B9C(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_91();
  return v1;
}


int mp4msv_2B1B8D23(int a1, unsigned int a2)
{
  int v2;

  v2 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_52(a1, a2);
  return v2;
}


int mp4msv_A09878FC(int a1, unsigned int a2)
{
  int v2;

  v2 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_53(a1, a2);
  return v2;
}


int mp4msv_08F96BE2(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_92();
  return v1;
}


int mp4msv_287B3FDA(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_56();
  return v1;
}


int mp4msv_886CFC3D(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_94();
  return v1;
}


int mp4msv_9632797F(u16 *a1, int a2, int a3, int a4)
{
  int result;

  result = 4;
  if ( a1 != NULL )
  {
    if ( a2 == 0 )
      a3 = *a1 - 10;
    return mp4_box_fn_59(a1, a3, a4);
  }
  return result;
}


int mp4msv_EF6D76BC(s64 a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 && ((u32)a1 == 0 || HIDWORD(a1) != 0 || *(u32 *)a1 != 0) )
    return mp4_box_fn_60(a1);
  return v1;
}


int mp4msv_4C677A7E(s64 a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 && ((u32)a1 == 0 || HIDWORD(a1) != 0 || *(u32 *)a1 != 0) )
    return mp4_box_fn_61(a1);
  return v1;
}


int mp4msv_0DF0B9AD(u32 *a1)
{
  int v1;

  v1 = 4;
  if ( a1 != NULL && *a1 != 0 )
    return mp4_box_fn_64();
  return v1;
}


int mp4msv_AD3AF34E(u32 *a1)
{
  int v1;

  v1 = 4;
  if ( a1 != NULL && *a1 != 0 )
    return mp4_box_fn_65();
  return v1;
}


int mp4msv_0D32271B(u32 *a1)
{
  int v1;

  v1 = 4;
  if ( a1 != NULL && *a1 != 0 )
    return mp4_box_fn_95();
  return v1;
}


int mp4msv_D070C0AA(u32 *a1)
{
  int v1;

  v1 = 4;
  if ( a1 != NULL && *a1 != 0 )
    return mp4_box_fn_66();
  return v1;
}


int mp4msv_888C1115(u32 *a1)
{
  int v1;

  v1 = 4;
  if ( a1 != NULL && *a1 != 0 )
    return mp4_box_fn_67();
  return v1;
}


int mp4msv_E8DE0EB8(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_68();
  return v1;
}


int mp4msv_CD733822(int a1, unsigned int a2)
{
  int v2;

  v2 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_69(a1, a2);
  return v2;
}


int mp4msv_9A8B21D2(u32 *a1)
{
  int v1;

  v1 = 4;
  if ( a1 != NULL && *a1 != 0 )
    return mp4_box_fn_96();
  return v1;
}


int mp4msv_65C17387(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_71();
  return v1;
}


int mp4msv_2BFE233D(int a1)
{
  int v1;

  v1 = 4;
  if ( a1 != 0 )
    return mp4_box_fn_72();
  return v1;
}


int mp4msv_190E442D(int a1, int a2)
{
  int v2;

  v2 = 4;
  if ( a1 != 0 )
  {
    v2 = 6;
    if ( a2 != 0 )
      return mp4_box_fn_73();
  }
  return v2;
}


int mp4msv_9878A6B3(int a1, u32 *a2)
{
  int v2;

  v2 = 4;
  if ( a1 != 0 )
  {
    v2 = 6;
    if ( a2 != NULL )
    {
      v2 = 0;
      *a2 = *(u32 *)(a1 + 84);
    }
  }
  return v2;
}


int mp4msv_C9D3D397(int a1, int a2)
{
  int v2;

  v2 = 4;
  if ( a1 != 0 )
  {
    v2 = 6;
    if ( a2 != 0 )
      return mp4_box_fn_74();
  }
  return v2;
}


int mp4msv_D5936DBD(int a1, int a2, int a3)
{
  int result;

  result = 4;
  if ( a1 != 0 )
  {
    result = 6;
    if ( a3 != 0 && a2 != 0 )
      return mp4_box_fn_97();
  }
  return result;
}


int mp4_box_fn_14(u32 *a1)
{
  u32 *v2;
  int v3;

  v2 = (u32 *)mp4_sample_fn_5(8);
  v3 = 1;
  *a1 = v2;
  if ( v2 != NULL )
  {
    *v2 = 0;
    return 0;
  }
  return v3;
}


int mp4_box_fn_15(int a1, int a2, int a3)
{
  mp4_track_fn_65(a1, a2, a3);
  return 0;
}


int mp4_box_fn_16(u32 *a1, int a2)
{
  int *v2;
  int v3;

  v2 = &g_mp4_state_table[20 * a2 + 2 * *a1];
  v3 = 17;
  if ( *v2 != 0 )
  {
    v3 = 0;
    a1[1] = v2[1];
  }
  return v3;
}


int mp4_box_fn_17(u32 *a1)
{
  int v1;
  int result;

  v1 = a1[1];
  result = 0;
  if ( v1 != -1 )
    *a1 = v1;
  return result;
}


int mp4_box_fn_18(int a1)
{
  *(u32 *)(a1 + 4) = g_mp4_err_table[*(u32 *)(a1 + 4)];
  return 0;
}


int mp4_box_fn_19(int a1)
{
  return *(u32 *)(a1 + 4) == 2;
}


int mp4_box_fn_20(u32 *a1)
{
  return *a1 == 9;
}


int mp4_box_fn_21(u32 *a1)
{
  return *a1 == 2;
}


int mp4_box_fn_22(unsigned int a1, u16 *a2)
{
  unsigned int v4;
  int v5;
  int i;
  unsigned int v7;
  unsigned int v8;
  char v9;
  unsigned int v10;
  int v11;
  unsigned int v12;
  unsigned int v13;
  unsigned int v15;
  u32 v16[4];

  v16[0] = 505355295;
  v16[1] = 522133023;
  v16[2] = 522067742;
  v4 = 1904;
  v5 = 0;
  for ( i = 0; ; i = v4 & 3 )
  {
    if ( i != 0 || (v7 = v4 % 0x190) == 100 || v7 == 200 || (v8 = 31622400, v9 = 1, v7 == 300) )
    {
      v8 = 31536000;
      v9 = 0;
    }
    if ( a1 >= v8 )
    {
      a1 -= v8;
      v4 = (u16)(v4 + 1);
    }
    else
    {
      a2[5] = v4;
      v5 = 1;
    }
    if ( v5 != 0 )
      break;
  }
  v10 = 0;
  v11 = 1;
  do
  {
    v12 = 2505600;
    if ( ((v11 == 0) & (u8)v9) == 0 )
      v12 = 86400 * *((u8 *)v16 + v10);
    if ( a1 >= v12 )
    {
      a1 -= v12;
    }
    else
    {
      a2[4] = v10 + 1;
      v10 = ((v10) & ~0xFFFFU) | ((u16)(12));
    }
    v10 = (u16)(v10 + 1);
    v11 = v10 ^ 1;
  }
  while ( v10 < 0xC );
  a2[3] = a1 / 0x15180 + 1;
  v13 = a1 - 86400 * (u16)(a1 / 0x15180 + 1) + 86400;
  a2[2] = v13 / 0xE10;
  v15 = v13 - 3600 * (u16)(v13 / 0xE10);
  *a2 = v15 % 0x3C;
  a2[1] = v15 / 0x3C;
  return 0;
}


int mp4_box_fn_23(unsigned int a1, unsigned int a2, unsigned int a3, unsigned int a4)
{
  int v5;
  unsigned int v6;
  unsigned int v7;
  int v8;
  s64 v10;
  unsigned int v11;
  unsigned int v12;
  int i;
  unsigned int v14;
  unsigned int v15;
  int v16;

  v5 = a3;
  v6 = a1;
  v7 = a4;
  if ( a1 == 0 || a3 == 0 || a2 < a4 )
    return 6;
  v10 = ((u64)(v10) & 0xFFFFFFFF00000000ULL) | (u32)(a3 == a1);
  v10 = ((u64)(v10) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(a4 == 0) << 32);
  v8 = 0;
  if ( v10 != 0 )
    return v8;
  if ( a3 < a1 )
  {
    v11 = a1 - a3;
    if ( a1 < a3 + a4 )
    {
      v12 = a1 + a4;
      for ( i = a3 + a4; a1 < v12; v7 -= v11 )
      {
        if ( v7 < v11 )
          v11 = v7;
        v12 -= v11;
        i -= v11;
        mp4_box_fn_10(v12, v11, i, v11);
      }
      return 0;
    }
  }
  v14 = a1 + a4;
  if ( a1 >= a3 || a3 >= v14 )
  {
    mp4_box_fn_10(a1, a2, a3, a4);
    return 0;
  }
  v15 = a3 - a1;
  if ( a1 >= v14 )
    return 0;
  do
  {
    if ( v7 < v15 )
      v15 = v7;
    v16 = v6;
    v6 += v15;
    mp4_box_fn_10(v16, v15, v5, v15);
    v7 -= v15;
    v5 += v15;
  }
  while ( v6 < v14 );
  return 0;
}


int mp4_box_fn_24(int a1, int a2, int a3, int a4)
{
  int v4;
  int result;

  v4 = mp4_box_fn_3(a1, a2, a3, a4, 0);
  result = 2;
  if ( v4 == 0 )
    return 0;
  return result;
}


int mp4_box_fn_25(int *a1, int a2, u32 *a3)
{
  int v3;
  int v4;
  unsigned int v5;
  unsigned int v6;
  int *v7;
  int i;

  v3 = *a1;
  v4 = 8195;
  if ( *(u32 *)(v3 + 236) != 0 )
  {
    v5 = *(u16 *)(v3 + 252);
    *a3 = 0;
    v6 = 0;
    if ( v5 != 0 )
    {
      v7 = *(int **)(v3 + 256);
      for ( i = *v7; ; i = *v7 )
      {
        ++v6;
        ++v7;
        if ( i == a2 )
          break;
        if ( v6 >= v5 )
          return 0;
      }
      *a3 = 1;
    }
    return 0;
  }
  return v4;
}


int mp4_box_fn_26(int a1, unsigned int a2, u32 *a3, unsigned int a4, int a5, u32 *a6)
{
  int v7;
  int v8;
  int v12;
  int v14;

  v7 = a4 == 0 && a2 == 0;
  v8 = 8197;
  if ( v7 )
    return v8;
  v12 = (__PAIR64__(a2, a4) - 1) >> 32;
  if ( *(u32 *)(*(u32 *)(*(u32 *)(a1 + 700) + 152) + 280) != 0 )
  {
    v8 = mp4_box_fn_30(a1, v12, a3, a5, a6);
    if ( v8 != 0 )
      return v8;
    goto LABEL_4;
  }
  v14 = mp4_codec_fn_19(a1 + 392, v12, a3, a5, a6, 1);
  v8 = 8195;
  if ( v14 == 7 )
    return v8;
  if ( v14 == 0 )
  {
    ++*a3;
LABEL_4:
    v8 = 0;
    if ( a4 == 0 )
      ++*a6;
    return v8;
  }
  v8 = 8;
  if ( (unsigned int)(v14 - 4120) >= 2 && v14 != 0x101B )
    return v14;
  return v8;
}


int mp4_box_fn_27(int a1, int a2, int a3, u32 *a4)
{
  int v4;
  int v6;
  int v7;
  int v8;
  int v9;
  s64 v10;

  v4 = a2 == 1 && a3 == 0;
  v6 = 8201;
  if ( a2 != 0 )
  {
    v7 = a1 + 392;
    v8 = a2 - 1;
    v6 = 8200;
    if ( !v4 )
    {
      v9 = mp4_codec_fn_23(v7, v8);
      v6 = 8195;
      if ( v9 != 7 )
      {
        v6 = 8;
        v10 = ((u64)(v10) & 0xFFFFFFFF00000000ULL) | (u32)(v9 == 0x101B);
        v10 = ((u64)(v10) & 0x00000000FFFFFFFFULL) | ((u64)(u32)((unsigned int)(v9 - 4120) < 2) << 32);
        if ( v10 == 0 )
        {
          v6 = v9;
          if ( v9 == 0 )
            ++*a4;
        }
      }
    }
  }
  return v6;
}


int mp4_box_fn_28(int a1, int a2, int a3, int a4)
{
  int v5;
  int v8;
  int v9;
  int v10;
  int v12;

  v5 = a2 - 1;
  v8 = a1 + 392;
  v9 = 8201;
  if ( a2 != 0 )
  {
    v10 = mp4_codec_fn_20(v8, v5, a3, a4, 1);
    if ( v10 != 0 )
    {
      v9 = 8195;
      if ( v10 != 7 )
      {
        v9 = 8;
        if ( (unsigned int)(v10 - 4120) >= 2 && v10 != 0x101B )
          return v10;
      }
    }
    else
    {
      v12 = mp4_track_fn_73(a1 + 416, *(u32 *)(a4 + 36), a4 + 36);
      v9 = 8195;
      if ( v12 == 0 )
        return 0;
    }
  }
  return v9;
}


int mp4_box_fn_29(int a1, int a2, int a3, int a4)
{
  int v5;
  int v6;
  int v7;
  int v8;

  v5 = a2 - 1;
  v6 = a1 + 392;
  v7 = 8201;
  if ( a2 != 0 )
  {
    v8 = mp4_codec_fn_17(v6, v5, a3, a4, 1);
    v7 = 0;
    if ( v8 != 0 )
    {
      v7 = 8195;
      if ( v8 != 7 )
      {
        v7 = 8;
        if ( (unsigned int)(v8 - 4120) >= 2 && v8 != 0x101B )
          return v8;
      }
    }
  }
  return v7;
}


int mp4_box_fn_30(int a1, int a2, u32 *a3, int a4, int a5)
{
  int v10;
  int v11;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  u8 v21[4];
  int v22;
  u8 v23[4];
  int v24;

  v10 = a1 + 392;
  v11 = mp4_box_fn_37();
  if ( v11 == 0 )
  {
    v11 = mp4_codec_fn_19(v10, a2, &v19, &v20, v21, 0);
    if ( v11 == 0 )
    {
      while ( 1 )
      {
        v13 = mp4_codec_fn_21(v10, v19, &v20, 0);
        v14 = v10;
        v11 = 8195;
        if ( v13 == 20481 )
          break;
        v11 = 10;
        if ( v13 != 0 )
          break;
        v15 = a2;
        if ( v20 != 0 )
          goto LABEL_15;
        v16 = mp4_codec_fn_23(v10, v19);
        v14 = v10;
        v11 = v16;
        if ( v16 != 0 && v16 != 0x2008 )
          return v11;
        if ( v16 == 0 )
        {
          v15 = a2;
LABEL_15:
          v11 = mp4_codec_fn_19(v14, v15, a3, a4, a5, 1);
          if ( v11 == 0 )
          {
            v11 = 0;
            ++*a3;
          }
          return v11;
        }
        v17 = mp4_codec_fn_33(v10, &v22);
        v11 = 10;
        if ( v17 != 0 )
          return v11;
        v11 = 16;
        if ( v22 != 0 )
          return v11;
        v11 = mp4_box_fn_38(a1);
        if ( v11 != 0 )
          return v11;
        v18 = mp4_codec_fn_37(v10, v23, &v24);
        v11 = 10;
        if ( v18 != 0 )
          return v11;
        v19 = v24 - 1;
      }
    }
  }
  return v11;
}


int mp4_box_fn_31(int a1, int a2, int a3, u32 *a4, u32 *a5)
{
  int v6;
  int v9;
  int v11;
  int v13;

  v6 = a2 - 1;
  v9 = 8201;
  if ( a2 != 0 )
  {
    v11 = mp4_api_fn_2(
            *(u32 *)(a1 + 676),
            v6,
            a3,
            *(u32 *)(*(u32 *)(*(u32 *)(*(u32 *)(a1 + 700) + 152) + 276) + 4));
    v9 = 8195;
    if ( v11 != 7 )
      v9 = v11;
    if ( v11 == 0 )
    {
      v13 = mp4_codec_fn_37(a1 + 392, a4, a5);
      if ( v13 == 0 )
      {
        if ( *a5 == 0 )
        {
          v9 = 8195;
          goto LABEL_11;
        }
        ++*a4;
      }
      v9 = 8195;
LABEL_11:
      if ( v13 != 7 )
        return v13;
    }
  }
  return v9;
}


int mp4_box_fn_32(u32 **a1)
{
  u32 *v1;
  int v2;

  v1 = *a1;
  v2 = 8195;
  if ( *v1 != 0 && v1[3] != 0 )
    return 0;
  return v2;
}


int mp4_box_fn_33()
{
  return mp4_track_fn_74();
}


int mp4_box_fn_34(int a1)
{
  return mp4_codec_fn_34(a1 + 392);
}


int mp4_box_fn_35(int a1, u32 *a2, u32 *a3)
{
  int v4;
  int v6;

  v4 = mp4_codec_fn_37(a1 + 392, a2, a3);
  v6 = 8195;
  if ( v4 != 7 )
    v6 = v4;
  if ( v4 == 0 && *a3 != 0 )
    ++*a2;
  return v6;
}


int mp4_box_fn_36(int a1)
{
  int v1;
  int v2;

  v1 = mp4_api_fn_72(a1 + 392);
  v2 = 8195;
  if ( v1 != 7 )
  {
    v2 = 8;
    if ( (unsigned int)(v1 - 4120) >= 2 && v1 != 0x101B )
      return v1;
  }
  return v2;
}


int mp4_box_fn_37(int a1, unsigned int a2)
{
  int v2;
  int v3;
  int v5;
  u32 *v7;
  int v8;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int i;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  u32 v22[4];
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  u32 v28[2];
  u32 v29[2];
  int v30;
  int v31;

  v2 = a1 + 392;
  v3 = 1;
  v5 = 0;
  v7 = *(u32 **)(*(u32 *)(*(u32 *)(a1 + 700) + 152) + 276);
  if ( a2 < *(u32 *)(a1 + 184) )
    return mp4_api_fn_67(a1 + 392);
  v10 = mp4_sample_fn_11(a1, &v26);
  v8 = 10;
  if ( v10 != 0 )
    return v8;
  v12 = *(u32 *)(*v7 + 280);
  if ( v12 != 0 )
  {
    v21 = mp4_box_fn_109(v12, v26, a2, &v27, v22);
    v8 = 10;
    if ( v21 != 0 )
      return v8;
  }
  else
  {
    v27 = 0;
  }
  v13 = v27;
  v14 = v27 == 0;
  if ( v27 != 0 )
  {
    if ( v23 == 0 )
    {
      v3 = 1;
      goto LABEL_17;
    }
    if ( v24 != 0 )
    {
      if ( v25 != 0 )
      {
        v19 = mp4_track_fn_29(*v7, v7[1], v22[2], v22[3], v28, v29);
        v3 = 0;
        if ( v19 != 20496 )
        {
          v8 = v19;
          if ( v19 != 0 )
            return v8;
          v20 = mp4_track_fn_31(v28[0], v26, *(u32 *)(*v7 + 136), v23 - 1, v24 - 1, v25 - 1, &v30);
          if ( (unsigned int)(v20 - 20498) >= 2 && v20 != 0x2009 )
          {
            v8 = 10;
            if ( v20 != 0 )
              return v8;
            v5 = v22[1] - v30;
            v8 = mp4_track_fn_30(v28[0], v26, *(u32 *)(*v7 + 136), &v31);
            if ( v8 != 0 )
              return v8;
            v13 = v27;
            goto LABEL_16;
          }
        }
        v13 = v27;
      }
      v3 = 1;
    }
    else
    {
      v3 = 1;
    }
LABEL_16:
    v14 = v13 == 0;
  }
LABEL_17:
  if ( (v14 | v3) == 0 )
    goto LABEL_19;
  v18 = mp4_track_fn_29(*v7, v7[1], *(u32 *)(*v7 + 144), *(u32 *)(*v7 + 148), v28, v29);
  v8 = 8197;
  if ( v18 != 20496 )
  {
    v8 = v18;
    if ( v18 == 0 )
    {
      v5 = *(u32 *)(a1 + 184);
      v8 = mp4_track_fn_30(v28[0], v26, *(u32 *)(*v7 + 136), &v31);
      if ( v8 == 0 )
      {
        for ( i = v31; a2 >= v5 + i; i = v31 )
        {
          v16 = mp4_track_fn_29(*v7, v7[1], v29[0], v29[1], v28, v29);
          v8 = 8197;
          if ( v16 == 20496 )
            return v8;
          v8 = v16;
          if ( v16 != 0 )
            return v8;
          v5 += v31;
          v8 = mp4_track_fn_30(v28[0], v26, *(u32 *)(*v7 + 136), &v31);
          if ( v8 != 0 )
            return v8;
LABEL_19:
          ;
        }
        v17 = mp4_api_fn_68(v2, v28[0], v5, v11, v28, v29);
        v8 = 0;
        if ( v17 != 0 )
          return v17;
      }
    }
  }
  return v8;
}


int mp4_box_fn_38(int a1)
{
  int v1;
  int v2;
  int v4;
  u32 *v5;
  int v6;
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
  unsigned int v22;
  unsigned int v23;
  int v24;
  int v25;
  int v26;
  u32 v27[2];
  u32 v28[2];
  unsigned int v29;
  unsigned int v30;
  int v31;

  v1 = 0;
  v2 = a1 + 392;
  v5 = *(u32 **)(*(u32 *)(*(u32 *)(a1 + 700) + 152) + 276);
  v4 = mp4_codec_fn_33(a1 + 392, &v21);
  v6 = 10;
  if ( v4 == 0 )
  {
    v6 = 0;
    if ( v21 == 0 )
    {
      v8 = mp4_codec_fn_35(v2, &v22);
      v6 = 10;
      if ( v8 == 0 )
      {
        v9 = mp4_codec_fn_34(v2);
        v6 = 10;
        if ( v9 == 0 )
        {
          if ( *(u32 *)(*v5 + 144) == v22 )
          {
            if ( *(u32 *)(*v5 + 148) == v23 )
              return mp4_api_fn_67(v2);
            v10 = a1;
          }
          else
          {
            v10 = a1;
          }
          v11 = mp4_sample_fn_11(v10, &v25);
          v6 = 10;
          if ( v11 == 0 )
          {
            v13 = *(u32 *)(*v5 + 280);
            if ( v13 != 0 )
            {
              v20 = mp4_track_fn_1(v13, v25, v24, v12, v22, v23, &v26, v27);
              v6 = 10;
              if ( v20 != 0 )
                return v6;
            }
            else
            {
              v26 = 0;
            }
            v14 = v26;
            if ( v26 != 0 )
            {
              v19 = mp4_track_fn_29(*v5, v5[1], v27[0], v27[1], v28, &v29);
              if ( v19 == 20496 )
              {
                v14 = v26;
                v1 = 1;
              }
              else
              {
                v6 = v19;
                if ( v19 != 0 )
                  return v6;
                v14 = v26;
              }
            }
            if ( ((v14 == 0) | v1) == 0
              || (v18 = mp4_track_fn_29(*v5, v5[1], *(u32 *)(*v5 + 144), *(u32 *)(*v5 + 148), v28, &v29),
                  v6 = 10,
                  v18 != 20496)
              && (v6 = v18, v18 == 0) )
            {
              do
              {
                if ( v30 >= v23 )
                {
                  if ( v23 != v30 )
                  {
                    v15 = *v5;
LABEL_22:
                    v6 = mp4_track_fn_30(v28[0], v25, *(u32 *)(v15 + 136), &v31);
                    if ( v6 == 0 )
                      return mp4_api_fn_68(v2, v28[0], v24 - v31, v16, v31, v17);
                    return v6;
                  }
                  if ( v29 >= v22 )
                  {
                    v15 = *v5;
                    goto LABEL_22;
                  }
                }
                v6 = mp4_track_fn_29(*v5, v5[1], v29, v30, v28, &v29);
              }
              while ( v6 == 0 );
            }
          }
        }
      }
    }
  }
  return v6;
}


int mp4_box_fn_39(int *a1, int *a2, int a3)
{
  int *v5;
  int v6;
  int v7;
  int *v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  u32 *v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  u32 *v26;
  u32 *v27;
  int v28;
  int v29;
  int v30;
  int v31;
  u32 *v32;
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
  unsigned int v44;
  int v45;
  int v46;
  unsigned int v47;
  unsigned int i;
  int v49;
  unsigned int v50;
  int v51; // dc
  int v52;
  int v53;
  int v54;
  int v55;
  unsigned int v56;
  int v57;
  unsigned int v58;
  int v59;
  int v60;
  int v61;
  int v62;
  int v63;
  int j;
  int v65;
  int v66;
  int v67;
  int v68;
  int v69;
  int v70;
  int v71;
  int v72;
  int v73[12];
  int v74;
  int v75;
  int v76;
  u32 *v77;

  v5 = a1;
  v6 = mp4_box_fn_106(a1[1]);
  v7 = 2;
  if ( v6 == 0 )
  {
    v8 = (int *)v5[1];
    v9 = *v5;
    v8[20] = 3;
    v10 = *v8;
    if ( v9 != 0 )
      mp4_box_fn_13(v5);
    v11 = mp4_sample_fn_5(336);
    *v5 = v11;
    v7 = 1;
    if ( v11 != 0 )
    {
      v14 = v5[2];
      *(u32 *)(v11 + 276) = v5;
      *(u32 *)(v11 + 284) = v14;
      mp4_box_fn_24(v10, v12, 0, 0);
      v15 = mp4_box_fn_4(*(u32 *)v5[1]);
      v18 = mp4_sample_fn_9(*v5, v17, v15, v16, v5[1], a3);
      if ( v18 != 0 )
      {
        v19 = (u32 *)v5[1];
        if ( v19[1] != 4 )
          goto LABEL_9;
        v20 = v19[8];
      }
      else
      {
        v19 = (u32 *)v5[1];
        v20 = v19[8];
      }
      if ( v20 == 0 )
      {
        v21 = v19[1];
LABEL_10:
        *a2 = v21;
        if ( (v21 & 0xE0000000) != 0 )
        {
          mp4_box_fn_13(v5);
          v66 = *a2;
          v5 = NULL;
          v7 = 2;
          if ( *a2 < 0 )
            return v7;
          v7 = 1;
          if ( (v66 & 0x40000000) != 0 )
            return v7;
          v7 = 8193;
          if ( (v66 & 0x20000000) != 0 )
            return v7;
        }
        if ( v18 != 0 )
        {
          v23 = *a2;
          if ( *a2 != 4 )
          {
LABEL_15:
            if ( (unsigned int)(a3 - 1) < 3 )
            {
              v24 = *(u32 *)(*v5 + 136);
              if ( v24 != 0 )
              {
                if ( *(u32 *)(v24 + 8) != 0 )
                {
                  *a2 = v23 | 0x40000;
                  v23 |= 0x40000u;
                }
                v25 = v5[1];
              }
              else
              {
                v25 = v5[1];
              }
              if ( *(u32 *)(v25 + 32) == 1 )
              {
                *a2 = v23 | 0x10000;
                v23 |= 0x10000u;
              }
            }
            if ( (v23 & 0xFFF0FFFB) != 0 )
            {
              mp4_box_fn_13(v5);
              return 8195;
            }
            else
            {
              v26 = (u32 *)*v5;
              v27 = *(u32 **)(*v5 + 132);
              v7 = 0;
              if ( v27 != NULL )
              {
                v28 = mp4_codec_fn_70(*v27 - 12, v27[3], &v76);
                v7 = 0;
                if ( v28 == 0 )
                {
                  v29 = *(u32 *)(v76 + 4);
                  if ( *(u8 *)(v29 + 32) != 0 )
                  {
                    v30 = **(u32 **)(v29 + 28);
                    mp4_codec_fn_66();
                    v31 = mp4_sample_fn_12(v26, v30, &v77);
                    v7 = 0;
                    if ( v31 == 0 )
                    {
                      v32 = v77;
                      v33 = v26[33];
                      v77[180] = 0;
                      v34 = v26[8];
                      v35 = v32[45];
                      *(u32 *)(v33 + 16) = v32;
                      if ( v34 == v35 && (v36 = 2, v26[9] == v32[10]) )
                      {
                        v37 = v5[1];
                      }
                      else
                      {
                        v36 = 1;
                        v37 = v5[1];
                      }
                      v32[181] = v36;
                      if ( *(u32 *)(v37 + 32) == 1 )
                        v32[181] = 1;
                      v38 = mp4_box_fn_28((int)v32, 1, 0, (int)v73);
                      v7 = 0;
                      if ( v38 == 0 )
                      {
                        v40 = mp4_sample_fn_5(v73[0]);
                        v7 = 0;
                        if ( v40 != 0 )
                        {
                          mp4_box_fn_24(v10, v39, v73[2], v73[3]);
                          v41 = mp4_box_fn_5(v40, v73[0], v10);
                          if ( v41 == v73[0] )
                            mp4_codec_fn_85(v41, v40, v77 + 180);
                          mp4_track_fn_65(v40, v42, v43);
                          return 0;
                        }
                      }
                    }
                  }
                  else
                  {
                    mp4_codec_fn_66();
                    return 0;
                  }
                }
              }
            }
            return v7;
          }
          v22 = *v5;
        }
        else
        {
          v22 = *v5;
        }
        v44 = *(u32 *)(v22 + 8);
        v45 = 0;
        if ( v44 == 0 )
        {
          v46 = v5[1];
LABEL_44:
          if ( *(u32 *)(v46 + 32) == 1 )
          {
            if ( v44 >= 0x80 )
            {
              v50 = 126;
              v51 = v44 > 0x7E;
              v52 = 126;
              if ( v51 )
              {
                do
                {
                  v63 = mp4_track_fn_69(v22, v52);
                  ++v50;
                  mp4_track_fn_66(v22, *(u32 *)(v63 + 32));
                  v52 = v50;
                }
                while ( v50 < *(u32 *)(v22 + 8) );
                v53 = *a2;
              }
              else
              {
                v53 = *a2;
              }
              *a2 = v53 | 0x10;
            }
            v54 = mp4_sample_fn_5(508);
            v55 = v54;
            if ( v54 != 0 )
            {
              v56 = 0;
              if ( *(u32 *)(v22 + 8) != 0 )
              {
                v57 = v54;
                v58 = 0;
                do
                {
                  v59 = mp4_track_fn_69(v22, v58);
                  ++v56;
                  v60 = v57;
                  v57 += 4;
                  if ( v59 != 0 )
                    mp4_codec_fn_24(v59 + 392, v60, 0);
                  v58 = v56;
                }
                while ( v56 < *(u32 *)(v22 + 8) );
              }
              mp4_track_fn_23(v22, v55, v5[1]);
              mp4_track_fn_65(v55, v61, v62);
            }
            v46 = v5[1];
          }
          if ( mp4_box_fn_19(*(u32 *)(v46 + 84)) )
          {
            v23 = *a2;
          }
          else
          {
            v47 = 0;
            if ( *(u32 *)(v22 + 8) != 0 )
            {
              for ( i = 0; ; i = v47 )
              {
                v49 = mp4_track_fn_69(v22, i);
                ++v47;
                mp4_api_fn_64(v49 + 392, v5[1]);
                if ( v47 >= *(u32 *)(v22 + 8) )
                  break;
              }
              v23 = *a2;
            }
            else
            {
              v23 = *a2;
            }
          }
          goto LABEL_15;
        }
        for ( j = 0; ; j = v45 )
        {
          v65 = mp4_track_fn_69(v22, j);
          if ( v65 == 0 )
            goto LABEL_71;
          if ( !mp4_box_fn_19(*(u32 *)(v5[1] + 84)) )
            break;
          if ( *(u32 *)(v65 + 676) == 0 )
            goto LABEL_71;
          mp4_api_fn_7(*(u32 *)(v65 + 676), &v74, 1);
          mp4_api_fn_59(*(u32 *)(v65 + 676), &v75);
          mp4_api_fn_58(*(u32 *)(v65 + 676), v74, *(u32 *)(v65 + 184), v75);
          v44 = *(u32 *)(v22 + 8);
LABEL_72:
          if ( ++v45 >= v44 )
          {
            v46 = v5[1];
            goto LABEL_44;
          }
        }
        if ( mp4_codec_fn_28(v65 + 392, v5[1]) == 1 )
        {
          mp4_box_fn_13(v5);
          return 1;
        }
LABEL_71:
        v44 = *(u32 *)(v22 + 8);
        goto LABEL_72;
      }
      if ( mp4_box_fn_19(v19[21]) )
      {
        mp4_box_fn_13(v5);
        v67 = mp4_sample_fn_5(336);
        *v5 = v67;
        v7 = 1;
        if ( v67 == 0 )
          return v7;
        v69 = v5[2];
        *(u32 *)(v67 + 276) = v5;
        *(u32 *)(v67 + 284) = v69;
        mp4_box_fn_24(v10, v68, 0, 0);
        mp4_box_fn_18(*(u32 *)(v5[1] + 84));
        v70 = mp4_box_fn_4(*(u32 *)v5[1]);
        v18 = mp4_sample_fn_9(*v5, v72, v70, v71, v5[1], a3);
      }
      else
      {
        mp4_box_fn_18(*(u32 *)(v5[1] + 84));
      }
      v19 = (u32 *)v5[1];
LABEL_9:
      v21 = v19[1];
      goto LABEL_10;
    }
  }
  return v7;
}


int mp4_box_fn_40(int *a1, int *a2)
{
  int *v3;
  int v4;
  int v5;
  int v6; // dc
  int v7;
  int v8;
  int *v10;
  int v11;
  int v12;

  v3 = a1;
  v4 = mp4_box_fn_106(a1[1]);
  v5 = 2;
  if ( v4 == 0 )
  {
    v6 = *v3 != 0;
    *(u32 *)(v3[1] + 80) = 3;
    if ( v6 )
      mp4_box_fn_13(v3);
    v7 = mp4_sample_fn_5(336);
    *v3 = v7;
    v5 = 1;
    if ( v7 != 0 )
    {
      v10 = (int *)v3[1];
      *(u32 *)(v7 + 276) = v3;
      mp4_box_fn_24(*v10, v8, 0, 0);
      mp4_sample_fn_10(*v3, 3, v3[1]);
      v11 = *(u32 *)(v3[1] + 4);
      *a2 = v11;
      v12 = ((v12) & ~0xFFU) | ((u8)(v11));
      if ( (v11 & 0xE0000000) == 0
        || (mp4_box_fn_13(v3), v12 = *a2, v3 = NULL, v5 = 2, *a2 >= 0)
        && (v5 = 1, (v12 & 0x40000000) == 0)
        && (v5 = 8193, (v12 & 0x20000000) == 0) )
      {
        v5 = 0;
        if ( (v12 & 0xB) != 0 )
        {
          mp4_box_fn_13(v3);
          return 8195;
        }
      }
    }
  }
  return v5;
}


int mp4_box_fn_41(int *a1, int a2, int *a3)
{
  int *v5;
  int v6;
  int v7;
  int v8; // dc
  int v9;
  int v10;
  int *v12;
  int v13;
  int v14;

  v5 = a1;
  v6 = mp4_box_fn_106(a1[1]);
  v7 = 2;
  if ( v6 == 0 )
  {
    v8 = *v5 != 0;
    *(u32 *)(v5[1] + 80) = 3;
    if ( v8 )
      mp4_box_fn_13(v5);
    v9 = mp4_sample_fn_5(336);
    *v5 = v9;
    v7 = 1;
    if ( v9 != 0 )
    {
      v12 = (int *)v5[1];
      *(u32 *)(v9 + 276) = v5;
      mp4_box_fn_24(*v12, v10, 0, 0);
      mp4_sample_fn_10(*v5, a2, v5[1]);
      v13 = *(u32 *)(v5[1] + 4);
      *a3 = v13;
      v14 = ((v14) & ~0xFFU) | ((u8)(v13));
      if ( (v13 & 0xE0000000) == 0
        || (mp4_box_fn_13(v5), v14 = *a3, v5 = NULL, v7 = 2, *a3 >= 0)
        && (v7 = 1, (v14 & 0x40000000) == 0)
        && (v7 = 8193, (v14 & 0x20000000) == 0) )
      {
        v7 = 0;
        if ( (v14 & 0xB) != 0 )
        {
          mp4_box_fn_13(v5);
          return 8195;
        }
      }
    }
  }
  return v7;
}


int mp4_box_fn_42(u32 **a1, unsigned int a2, int *a3)
{
  int v4;
  u32 *v5;
  int v6;

  v4 = 8203;
  if ( a2 != 0 )
  {
    v5 = *a1;
    v4 = 8195;
    if ( *v5 != 0 )
    {
      v4 = 8203;
      if ( v5[2] >= (unsigned int)a2 )
      {
        v6 = mp4_track_fn_69(v5, a2 - 1);
        v4 = 8195;
        if ( v6 != 0 )
        {
          *a3 = v6;
          return 0;
        }
      }
    }
  }
  return v4;
}


int mp4_box_fn_43(int a1, int a2)
{
  int v2;
  s16 v3;
  int v4;
  s16 v5;

  v2 = 8195;
  if ( *(u32 *)(a1 + 12) != 0 && *(u32 *)(a1 + 160) != 0 )
  {
    *(u32 *)a2 = *(u32 *)(a1 + 32);
    *(u32 *)(a2 + 4) = *(u32 *)(a1 + 40);
    *(u32 *)(a2 + 8) = (*(u8 *)(a1 + 23) & 1) != 0;
    v3 = *(u16 *)(a1 + 52);
    v2 = 0;
    *(u32 *)(a2 + 12) = *(u16 *)(a1 + 98);
    *(u16 *)(a2 + 20) = v3;
    v4 = *(s16 *)(a1 + 54);
    v5 = *(u16 *)(a1 + 188);
    *(u32 *)(a2 + 16) = *(u16 *)(a1 + 102);
    *(u16 *)(a2 + 22) = v5;
    *(u32 *)(a2 + 24) = v4;
  }
  return v2;
}


int mp4_box_fn_44(int *a1, int a2, int a3)
{
  unsigned int v3;
  int v4;
  int v5;
  u32 *v6;
  unsigned int v7;
  int v8;
  int v10;
  int v11;
  int i;
  u32 *v13;
  int v14;
  unsigned int v15;

  v3 = 0;
  if ( a2 != 0 )
  {
    v4 = 0;
    do
    {
      v5 = 4 * (v4 + v3);
      v6 = (u32 *)(v5 + a2 + 60);
      v7 = 0;
      v8 = v5;
      do
      {
        ++v7;
        *(u32 *)(v8 + a3) = *v6;
        v8 += 4;
        ++v6;
      }
      while ( v7 < 3 );
      ++v3;
      v4 = 2 * v3;
    }
    while ( v3 < 3 );
  }
  else if ( a1 != NULL )
  {
    v10 = *a1;
    v11 = 0;
    for ( i = 0; ; i = 2 * v11 )
    {
      v13 = (u32 *)(4 * (i + v11) + v10 + 56);
      v14 = 4 * (i + v11);
      v15 = 0;
      do
      {
        ++v15;
        *(u32 *)(v14 + a3) = *v13;
        v14 += 4;
        ++v13;
      }
      while ( v15 < 3 );
      if ( (unsigned int)++v11 >= 3 )
        break;
    }
  }
  return 0;
}


int mp4_box_fn_45(int a1, int a2, unsigned int *a3)
{
  int v3;
  int v4;
  u32 *v5;

  v3 = *(u32 *)(a1 + 696);
  v4 = 8194;
  if ( v3 != 0 )
  {
    if ( a2 == 1836085092 )
    {
      v5 = *(u32 **)(v3 + 12);
    }
    else
    {
      if ( a2 != 1937337955 )
        return v4;
      v5 = *(u32 **)(v3 + 8);
    }
    if ( v5 != NULL )
    {
      v4 = 0;
      *a3 = (unsigned int)(*v5 - 8) >> 2;
    }
  }
  return v4;
}


int mp4_box_fn_46(int a1, int a2, unsigned int a3, int a4)
{
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v11;

  v5 = 4;
  if ( *(u32 *)(a1 + 700) != 0 )
  {
    v6 = *(u32 *)(a1 + 696);
    v5 = 8194;
    if ( v6 != 0 )
    {
      v5 = 6;
      if ( a3 != 0 )
      {
        v7 = 0;
        if ( a2 == 1836085092 )
        {
          v11 = *(u32 *)(v6 + 12);
          v5 = 8194;
          if ( v11 == 0 || *(u16 *)(v11 + 8) < a3 )
            return v5;
          v7 = *(u32 *)(4 * a3 + *(u32 *)(v11 + 12) - 4);
        }
        if ( a2 == 1937337955 )
        {
          v8 = *(u32 *)(v6 + 8);
          v5 = 8194;
          if ( v8 == 0 || *(u16 *)(v8 + 8) < a3 )
            return v5;
          v7 = *(u32 *)(4 * a3 + *(u32 *)(v8 + 12) - 4);
        }
        v5 = 8194;
        if ( v7 != 0 )
        {
          v9 = mp4_sample_fn_12(*(u32 *)(a1 + 700), v7, a4);
          v5 = 8195;
          if ( v9 == 0 )
            return 0;
        }
      }
    }
  }
  return v5;
}


int mp4_box_fn_47(u32 *a1, unsigned int a2, int a3, int a4, int a5)
{
  int v7;
  u32 *v9;
  unsigned int v10;
  unsigned int v11;
  u32 *v12;
  unsigned int v13;
  int v14;
  int v15;
  int v16;

  v7 = 6;
  if ( a2 != 0 )
  {
    v7 = 8194;
    if ( a1[31] != 0 )
    {
      v9 = a1 + 33;
      if ( a1[33] != 0 )
      {
        v10 = a1[36];
        v11 = a2 - 1;
        v12 = a1 + 33;
        v13 = a2 - 1;
        if ( v10 >= a2 )
        {
          v14 = mp4_sample_fn_13(v12, v13, 0, a3);
          v7 = 8195;
          if ( v14 == 0 )
          {
            v15 = mp4_sample_fn_13(v9, v11, 1, a4);
            v7 = 8195;
            if ( v15 == 0 )
            {
              v16 = mp4_sample_fn_13(v9, v11, 2, a5);
              v7 = 8195;
              if ( v16 == 0 )
                return 0;
            }
          }
        }
      }
    }
  }
  return v7;
}


int mp4_box_fn_48(int a1, int a2)
{
  int v4;
  int v5;

  v4 = mp4_track_fn_78(a1, a2, 0);
  if ( v4 == 2 )
  {
    mp4_track_fn_79(*(u32 *)(a1 + 700), a2, 1);
    v4 = mp4_codec_fn_25(a1 + 392, a2, 1);
  }
  v5 = 8195;
  if ( v4 != 7 )
  {
    v5 = 8;
    if ( (unsigned int)(v4 - 4120) >= 2 && v4 != 0x101B )
      return v4;
  }
  return v5;
}


int mp4_box_fn_49(int a1, int a2, int a3)
{
  int v3;
  int v4;
  int v5;
  int v6; // dc
  int v7;

  v3 = 6;
  if ( a2 != 0 )
  {
    v4 = mp4_codec_fn_3(a1 + 392, a2 - 1, a3, 1);
    v5 = v4;
    if ( v4 >= 4122 )
    {
      v3 = 8194;
      if ( v4 == 4123 )
        return 8;
      v7 = 8201;
    }
    else
    {
      v3 = 8195;
      v6 = v4 < 4120;
      v7 = 7;
      if ( !v6 )
        return 8;
    }
    if ( v5 != v7 )
      return v5;
  }
  return v3;
}


int mp4_box_fn_50(int a1, unsigned int a2, int a3)
{
  int v3;
  unsigned int v4;
  int v5;
  int v6; // dc
  int v7;

  v3 = 8195;
  if ( *(u32 *)(a1 + 416) != 0 )
  {
    v3 = 6;
    if ( a2 != 0 )
    {
      v4 = *(u32 *)(a1 + 428);
      v5 = a1 + 416;
      v6 = v4 < a2;
      v3 = 8194;
      if ( !v6 )
      {
        v7 = mp4_track_fn_73(v5, a2, a3);
        v3 = 8194;
        if ( v7 == 0 )
          return 0;
      }
    }
  }
  return v3;
}


int mp4_box_fn_51(u32 *a1, u16 *a2, int a3)
{
  int v3;
  int v4;
  int v5;
  s16 v6;
  int v7;
  s16 v8;
  int v9;

  v3 = 4;
  if ( a1[4] == 1936684398 )
  {
    v4 = a1[1];
    *(u32 *)(a3 + 4) = 0;
    v3 = 0;
    *(u32 *)a3 = v4;
    v5 = a1[9];
    *(u32 *)(a3 + 8) = a1[5];
    v6 = *(u16 *)(v5 + 8);
    *(u32 *)(a3 + 12) = *(u32 *)(v5 + 4);
    v7 = *(u32 *)(v5 + 16);
    *(u16 *)(a3 + 16) = v6;
    *(u32 *)(a3 + 20) = v7;
    v8 = *(u16 *)(v5 + 10);
    *(u32 *)(a3 + 24) = *(u32 *)(v5 + 20);
    v9 = *(u32 *)(v5 + 32);
    *(u16 *)(a3 + 18) = v8;
    *(u32 *)(a3 + 28) = v9;
    if ( a2 != NULL )
      *a2 = *(u16 *)v5;
  }
  return v3;
}


int mp4_box_fn_52(int a1, unsigned int a2, u16 *a3)
{
  int v5;
  unsigned int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u16 v12[8];

  v6 = a2;
  v5 = mp4_box_fn_98(1635148593, a1);
  v7 = 4112;
  if ( v5 != 0 )
  {
    v8 = *(u32 *)(a1 + 44);
    v7 = 8195;
    if ( v8 != 0 )
    {
      v7 = 6;
      if ( v6 != 0 )
      {
        v7 = 8194;
        if ( *(u8 *)(v8 + 15) >= v6 )
        {
          v7 = 10;
          v9 = *(u32 *)(v8 + 16);
          if ( v9 != 0 )
          {
            v10 = mp4_codec_fn_62(v9, v6 - 1, v12);
            v7 = 10;
            if ( v10 == 0 )
            {
              v7 = 0;
              *a3 = v12[0];
            }
          }
        }
      }
    }
  }
  return v7;
}


int mp4_box_fn_53(int a1, unsigned int a2, u16 *a3)
{
  int v5;
  unsigned int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u16 v12[8];

  v6 = a2;
  v5 = mp4_box_fn_98(1635148593, a1);
  v7 = 4112;
  if ( v5 != 0 )
  {
    v8 = *(u32 *)(a1 + 44);
    v7 = 8195;
    if ( v8 != 0 )
    {
      v7 = 6;
      if ( v6 != 0 )
      {
        v7 = 8194;
        if ( *(u8 *)(v8 + 20) >= v6 )
        {
          v7 = 10;
          v9 = *(u32 *)(v8 + 24);
          if ( v9 != 0 )
          {
            v10 = mp4_codec_fn_62(v9, v6 - 1, v12);
            v7 = 10;
            if ( v10 == 0 )
            {
              v7 = 0;
              *a3 = v12[0];
            }
          }
        }
      }
    }
  }
  return v7;
}


int mp4_box_fn_54(int a1, unsigned int a2, unsigned int a3, unsigned int a4)
{
  int v7;
  unsigned int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  u16 v14;
  unsigned int v15;

  v8 = a2;
  v7 = mp4_box_fn_98(1635148593, a1);
  v9 = 4112;
  if ( v7 != 0 )
  {
    v10 = *(u32 *)(a1 + 44);
    v9 = 8195;
    if ( v10 != 0 )
    {
      v9 = 6;
      if ( v8 != 0 )
      {
        v9 = 8194;
        if ( *(u8 *)(v10 + 15) >= v8 )
        {
          v9 = 10;
          v11 = *(u32 *)(v10 + 16);
          if ( v11 != 0 )
          {
            v12 = mp4_codec_fn_62(v11, v8 - 1, &v14);
            v9 = 10;
            if ( v12 == 0 )
              return mp4_box_fn_23(a4, a3, v15, v14);
          }
        }
      }
    }
  }
  return v9;
}


int mp4_box_fn_55(int a1, unsigned int a2, unsigned int a3, unsigned int a4)
{
  int v7;
  unsigned int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  u16 v14;
  unsigned int v15;

  v8 = a2;
  v7 = mp4_box_fn_98(1635148593, a1);
  v9 = 4112;
  if ( v7 != 0 )
  {
    v10 = *(u32 *)(a1 + 44);
    v9 = 8195;
    if ( v10 != 0 )
    {
      v9 = 6;
      if ( v8 != 0 )
      {
        v9 = 8194;
        if ( *(u8 *)(v10 + 20) >= v8 )
        {
          v9 = 10;
          v11 = *(u32 *)(v10 + 24);
          if ( v11 != 0 )
          {
            v12 = mp4_codec_fn_62(v11, v8 - 1, &v14);
            v9 = 10;
            if ( v12 == 0 )
              return mp4_box_fn_23(a4, a3, v15, v14);
          }
        }
      }
    }
  }
  return v9;
}


int mp4_box_fn_56(int a1, u32 *a2, int *a3, int a4)
{
  int v5;
  int v8;
  int v9;
  unsigned int v10;
  unsigned int v11;
  int i;
  unsigned int v13;
  int v14;
  u8 v15[16];

  v5 = 0;
  if ( a4 == 5 )
  {
    v8 = *(u32 *)(a1 + 688);
    v5 = 8195;
    if ( v8 != 0 && *(u32 *)(v8 + 44) != 0 )
    {
      v9 = v8 + 44;
      v10 = *(u16 *)(v9 + 8);
      v11 = 0;
      if ( *(u16 *)(v9 + 8) != 0 )
      {
        for ( i = *(u32 *)(v9 + 16); ; i += 40 )
        {
          ++v11;
          if ( *(u32 *)(i + 4) == 10 && *(u16 *)i == 18 && *(u32 *)(i + 8) == 21956 )
          {
            v13 = *(u32 *)(i + 36);
            if ( v13 != 0 )
              break;
          }
          if ( v11 >= v10 )
            return 8195;
        }
        mp4_box_fn_23((unsigned int)v15, 8u, v13, 8u);
        v14 = (v15[4] << 24) + (v15[5] << 16) + (v15[6] << 8) + v15[7];
        *a2 = (v15[0] << 24) + (v15[1] << 16) + (v15[2] << 8) + v15[3];
        v5 = 0;
        *a3 = v14;
      }
    }
  }
  return v5;
}


int mp4_box_fn_57(
        u32 **a1,
        int a2,
        int a3,
        int a4,
        unsigned int a5,
        u16 *a6,
        u32 *a7,
        u16 *a8)
{
  int v8;
  int v9;
  int v10;
  int v11;
  unsigned int v12;
  u16 *i;
  u16 v15;
  s16 v16;
  u32 *v17;
  int v18;

  if ( a2 != 0 )
  {
    v8 = *(u32 *)(a2 + 688);
    v9 = 8194;
    if ( v8 == 0 )
      return v9;
    v10 = v8 + 44;
    if ( *(u32 *)(v8 + 44) == 0 )
      return v9;
  }
  else
  {
    v17 = *a1;
    v9 = 8195;
    if ( *v17 == 0 )
      return v9;
    v18 = v17[42];
    v9 = 8194;
    if ( v18 == 0 || *(u32 *)(v18 + 44) == 0 )
      return v9;
    v10 = v18 + 44;
  }
  v11 = a4 & 0x7FFF;
  v12 = 0;
  if ( *(u16 *)(v10 + 8) == 0 )
    return 8194;
  for ( i = *(u16 **)(v10 + 16); ; i += 20 )
  {
    ++v12;
    if ( *((u32 *)i + 1) == a3 )
    {
      v15 = i[4];
      if ( (v15 & 0x7FFF) == v11 && i[5] == a5 )
        break;
    }
    if ( v12 >= *(u16 *)(v10 + 8) )
      return 8194;
  }
  v9 = 8195;
  if ( *i >= 0xAu )
  {
    *a6 = v15;
    v9 = 0;
    v16 = *i;
    *a7 = i;
    *a8 = v16 - 10;
  }
  return v9;
}


int mp4_box_fn_58(int a1, u8 *a2)
{
  unsigned int v2;
  int v3;
  int v4;
  u8 *v5;
  int v6;
  u8 *v7;
  int v8;
  u8 *v9;

  *a2 = *(u8 *)(a1 + 15);
  v2 = 0;
  a2[1] = *(u16 *)(a1 + 14);
  a2[2] = BYTE1(*(u32 *)(a1 + 12));
  a2[3] = *(u8 *)(a1 + 12);
  a2[4] = *(u8 *)(a1 + 19);
  a2[5] = *(u16 *)(a1 + 18);
  a2[6] = BYTE1(*(u32 *)(a1 + 16));
  a2[7] = *(u8 *)(a1 + 16);
  a2[8] = *(u8 *)(a1 + 23);
  a2[9] = *(u16 *)(a1 + 22);
  a2[10] = BYTE1(*(u32 *)(a1 + 20));
  a2[11] = *(u8 *)(a1 + 20);
  v3 = 12;
  if ( *(u32 *)(a1 + 20) != 0 )
  {
    v4 = 0;
    do
    {
      v5 = &a2[v3];
      v6 = *(u32 *)(a1 + 24) + v4;
      v7 = &a2[v3 + 4];
      v8 = v3 + 8;
      *v5 = *(u8 *)(v6 + 3);
      v3 += 12;
      v9 = &a2[v8];
      ++v2;
      v5[1] = *(u16 *)(v6 + 2);
      v5[2] = BYTE1(*(u32 *)v6);
      v5[3] = *(u8 *)v6;
      *v7 = *(u8 *)(v6 + 7);
      v7[1] = *(u16 *)(v6 + 6);
      v7[2] = BYTE1(*(u32 *)(v6 + 4));
      v7[3] = *(u8 *)(v6 + 4);
      *v9 = *(u8 *)(v6 + 11);
      v9[1] = *(u16 *)(v6 + 10);
      v9[2] = BYTE1(*(u32 *)(v6 + 8));
      v9[3] = *(u8 *)(v6 + 8);
      v4 += 12;
    }
    while ( v2 < *(u32 *)(a1 + 20) );
  }
  return 0;
}


int mp4_box_fn_59(u16 *a1, unsigned int a2, u8 *a3)
{
  unsigned int v3;
  int v4;
  unsigned int v5;

  v3 = *a1;
  v4 = 8195;
  if ( v3 >= 0xA )
  {
    if ( v3 != 10 )
    {
      v5 = v3 - 10;
      if ( a1[5] != 257 )
        return mp4_box_fn_23((unsigned int)a3, a2, *((u32 *)a1 + 9), v5);
      v4 = mp4_box_fn_58((int)a1, a3);
      if ( v4 != 0 )
        return v4;
    }
    return 0;
  }
  return v4;
}


int mp4_box_fn_60(u32 **a1, int a2, u32 *a3)
{
  u32 *v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int v9;

  if ( a2 != 0 )
  {
    v4 = *(u32 *)(a2 + 688);
    v5 = 0;
    if ( v4 != 0 )
    {
      v6 = *(u32 *)(v4 + 44);
      v7 = v4 + 44;
LABEL_5:
      if ( v6 != 0 )
      {
        v5 = 0;
        *a3 = *(u16 *)(v7 + 8);
        return v5;
      }
      v5 = 0;
    }
LABEL_8:
    *a3 = 0;
    return v5;
  }
  v3 = *a1;
  v5 = 8195;
  if ( *v3 != 0 )
  {
    v9 = v3[42];
    v5 = 0;
    if ( v9 == 0 )
      goto LABEL_8;
    v6 = *(u32 *)(v9 + 44);
    v7 = v9 + 44;
    goto LABEL_5;
  }
  return v5;
}


int mp4_box_fn_61(
        u32 **a1,
        int a2,
        unsigned int a3,
        u32 *a4,
        u32 *a5,
        u16 *a6,
        u16 *a7,
        u16 *a8)
{
  u32 *v8;
  int v9;
  int v10;
  int v11;
  u16 *v12;
  u16 v13;
  int v14;
  s16 v15;
  int v17;

  if ( a2 != 0 )
  {
    v9 = *(u32 *)(a2 + 688);
    v10 = 8194;
    if ( v9 == 0 )
      return v10;
    v11 = v9 + 44;
    if ( *(u32 *)(v9 + 44) == 0 )
      return v10;
  }
  else
  {
    v8 = *a1;
    v10 = 8195;
    if ( *v8 == 0 )
      return v10;
    v17 = v8[42];
    v10 = 8194;
    if ( v17 == 0 || *(u32 *)(v17 + 44) == 0 )
      return v10;
    v11 = v17 + 44;
  }
  v10 = 6;
  if ( a3 != 0 )
  {
    v10 = 8194;
    if ( *(u16 *)(v11 + 8) >= a3 )
    {
      v12 = (u16 *)(40 * (a3 - 1) + *(u32 *)(v11 + 16));
      v10 = 8195;
      if ( *v12 >= 0xAu )
      {
        v13 = v12[4];
        *a4 = v12;
        v10 = 0;
        *a6 = v13;
        v14 = *((u32 *)v12 + 1);
        *a7 = v12[5];
        v15 = *v12;
        *a5 = v14;
        *a8 = v15 - 10;
      }
    }
  }
  return v10;
}


int mp4_box_fn_62(u32 **a1, int a2, int a3, u16 *a4, u16 *a5, unsigned int a6)
{
  int v7;
  int v8;
  int v9;
  unsigned int v10;
  unsigned int v11;
  u16 *i;
  unsigned int v14;
  unsigned int v15;
  unsigned int v16;
  u32 *v17;
  int v18;

  if ( a2 != 0 )
  {
    v7 = *(u32 *)(a2 + 688);
    v8 = 8194;
    if ( v7 == 0 )
      return v8;
    v9 = v7 + 24;
    if ( *(u32 *)(v7 + 24) == 0 )
      return v8;
    v10 = *(u16 *)(v7 + 32);
  }
  else
  {
    v17 = *a1;
    v8 = 8195;
    if ( *v17 == 0 )
      return v8;
    v18 = v17[42];
    v8 = 8194;
    if ( v18 == 0 )
      return v8;
    v9 = v18 + 24;
    if ( *(u32 *)(v18 + 24) == 0 )
      return v8;
    v10 = *(u16 *)(v18 + 32);
  }
  v11 = 0;
  if ( v10 == 0 )
    return 8194;
  for ( i = *(u16 **)(v9 + 16); ; i += 22 )
  {
    ++v11;
    if ( *((u32 *)i + 1) == a3 )
      break;
    if ( v11 >= v10 )
      return 8194;
  }
  *a4 = i[4];
  *a5 = i[5];
  v14 = *i;
  v8 = 8195;
  if ( v14 >= 0xA )
  {
    v15 = v14 - 10;
    if ( i[5] == 1 )
    {
      v8 = 0;
      if ( v15 == 0 )
        return v8;
      v16 = (unsigned int)(i + 6);
      if ( v15 >= 0x21 )
      {
        mp4_box_fn_23(a6, 0x20u, v16, 0x1Eu);
        *(u8 *)(a6 + 31) = 0;
        *(u8 *)(a6 + 30) = 0;
        return 0;
      }
    }
    else
    {
      if ( v15 == 0 )
        return 0;
      if ( v15 >= 0x21 )
        v15 = 32;
      v16 = (unsigned int)(i + 6);
    }
    mp4_box_fn_23(a6, 0x20u, v16, v15);
    return 0;
  }
  return v8;
}


int mp4_box_fn_63(
        u32 **a1,
        int a2,
        unsigned int a3,
        u32 *a4,
        u16 *a5,
        u16 *a6,
        unsigned int a7)
{
  int v8;
  int v9;
  int v10;
  u16 *v11;
  int v12;
  u16 v13;
  unsigned int v14;
  unsigned int v15;
  unsigned int v16;
  u32 *v18;
  int v19;
  int v20;

  if ( a2 != 0 )
  {
    v8 = *(u32 *)(a2 + 688);
    v9 = 8194;
    if ( v8 == 0 )
      return v9;
    v10 = v8 + 24;
    if ( *(u32 *)(v8 + 24) == 0 )
      return v9;
  }
  else
  {
    v18 = *a1;
    v9 = 8195;
    if ( *v18 == 0 )
      return v9;
    v19 = v18[42];
    v9 = 8194;
    if ( v19 == 0 )
      return v9;
    v20 = *(u32 *)(v19 + 24);
    v10 = v19 + 24;
    if ( v20 == 0 )
      return v9;
  }
  v9 = 6;
  if ( a3 != 0 )
  {
    v9 = 8194;
    if ( *(u16 *)(v10 + 8) >= a3 )
    {
      v11 = (u16 *)(44 * (a3 - 1) + *(u32 *)(v10 + 16));
      v12 = *((u32 *)v11 + 1);
      *a5 = v11[4];
      v13 = v11[5];
      *a4 = v12;
      *a6 = v13;
      v14 = *v11;
      v9 = 8195;
      if ( v14 >= 0xA )
      {
        v15 = v14 - 10;
        if ( v11[5] == 1 )
        {
          v9 = 0;
          if ( v15 == 0 )
            return v9;
          v16 = (unsigned int)(v11 + 6);
          if ( v15 >= 0x21 )
          {
            mp4_box_fn_23(a7, 0x20u, v16, 0x1Eu);
            *(u8 *)(a7 + 31) = 0;
            *(u8 *)(a7 + 30) = 0;
            return 0;
          }
LABEL_12:
          mp4_box_fn_23(a7, 0x20u, v16, v15);
          return 0;
        }
        v9 = 0;
        if ( v15 != 0 )
        {
          if ( v15 >= 0x21 )
            v15 = 32;
          v16 = (unsigned int)(v11 + 6);
          goto LABEL_12;
        }
      }
    }
  }
  return v9;
}


int mp4_box_fn_64(u32 *a1, unsigned int a2, u32 *a3)
{
  u32 *v3;
  int v4;
  unsigned int v5;
  int v6;

  v3 = (u32 *)*a1;
  v4 = 8195;
  if ( v3[46] != 0 )
  {
    v4 = 6;
    if ( a2 != 0 )
    {
      v5 = v3[54];
      v4 = 8194;
      if ( v5 != 0 && v5 >= a2 )
      {
        v4 = 0;
        v6 = v3[57] + 44 * a2;
        *a3 = *(u32 *)(v6 - 32);
        a3[1] = *(u32 *)(v6 - 28);
        a3[2] = *(u32 *)(v6 - 24);
        a3[3] = *(u32 *)(v6 - 20);
        a3[4] = *(u32 *)(v6 - 16);
        a3[5] = *(u32 *)(v6 - 12);
        a3[6] = *(u32 *)(v6 - 8);
        a3[7] = *(u32 *)(v6 - 4);
      }
    }
  }
  return v4;
}


int mp4_box_fn_65(u32 *a1, unsigned int a2, u32 *a3)
{
  u32 *v3;
  int v4;
  unsigned int v5;
  int v6;

  v3 = (u32 *)*a1;
  v4 = 8195;
  if ( v3[46] != 0 )
  {
    v4 = 6;
    if ( a2 != 0 )
    {
      v5 = v3[55];
      v4 = 8194;
      if ( v5 != 0 && v5 >= a2 )
      {
        v4 = 0;
        v6 = v3[58] + 52 * a2;
        *a3 = *(u32 *)(v6 - 40);
        a3[1] = *(u32 *)(v6 - 36);
        a3[2] = *(u32 *)(v6 - 32);
        a3[3] = *(u32 *)(v6 - 28);
        a3[4] = *(u32 *)(v6 - 24);
        a3[5] = *(u32 *)(v6 - 20);
        a3[6] = *(u32 *)(v6 - 16);
        a3[7] = *(u32 *)(v6 - 12);
        a3[8] = *(u32 *)(v6 - 8);
        a3[9] = *(u32 *)(v6 - 4);
      }
    }
  }
  return v4;
}


int mp4_box_fn_66(int a1)
{
  int v1;
  int v2;
  int v3;

  v1 = *(u32 *)(*(u32 *)a1 + 132);
  v2 = 8194;
  if ( v1 != 0 )
  {
    v3 = *(u32 *)(v1 + 16);
    if ( v3 != 0 && *(u32 *)(v3 + 720) != 0 )
    {
      mp4_codec_fn_67(*(u32 *)(v3 + 720));
      return 0;
    }
  }
  return v2;
}


int mp4_box_fn_67(int a1, unsigned int a2, int a3)
{
  int v4;
  int v5;
  int v6;

  v4 = *(u32 *)(*(u32 *)a1 + 132);
  v5 = 8194;
  if ( v4 != 0 )
  {
    v6 = *(u32 *)(v4 + 16);
    if ( v6 != 0 && *(u32 *)(v6 + 720) != 0 )
    {
      v5 = mp4_codec_fn_63(*(u32 *)(v6 + 720), a2);
      if ( v5 == 0 )
      {
        v5 = 0;
        *(u32 *)(*(u32 *)a3 + 40) = v6;
      }
    }
  }
  return v5;
}


int mp4_box_fn_68(int a1, int a2)
{
  int v2;
  int v4;
  u32 v7[4];

  v2 = 4;
  v4 = *(u32 *)(a1 + 40);
  if ( v4 != 0 )
  {
    v2 = 8195;
    if ( *(u8 *)(a1 + 28) != 0 )
    {
      v2 = mp4_box_fn_46(v4, 1836085092, **(u16 **)(a1 + 24), (int)v7);
      if ( v2 == 0 )
      {
        v2 = 0;
        *(u32 *)a2 = v7[0];
        *(u8 *)(a2 + 4) = *(u8 *)(a1 + 36);
      }
    }
  }
  return v2;
}


int mp4_box_fn_69(int a1, unsigned int a2, u16 *a3, u32 *a4)
{
  int v7;
  int v8;
  int v9;
  int v10;
  s16 v11;
  u32 v13[4];

  v7 = *(u32 *)(a1 + 40);
  v8 = 4;
  if ( v7 != 0 )
  {
    v9 = *(u32 *)(v7 + 720);
    v8 = 8195;
    if ( v9 != 0 )
    {
      v8 = 6;
      if ( a2 != 0 )
      {
        v8 = 8194;
        if ( *(u8 *)(a1 + 36) >= (unsigned int)a2 )
        {
          v10 = mp4_codec_fn_64(v9, *(u8 *)(*(u32 *)(a1 + 32) + a2 - 1), v13);
          v8 = 8195;
          if ( v10 == 0 )
          {
            v8 = 0;
            v11 = *(u16 *)(v13[0] + 2);
            *a4 = *(u32 *)(v13[0] + 8);
            *a3 = v11;
          }
        }
      }
    }
  }
  return v8;
}


int mp4_box_fn_70(int a1, unsigned int a2, unsigned int a3, unsigned int a4)
{
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v12[4];

  v7 = *(u32 *)(a1 + 40);
  v8 = 4;
  if ( v7 != 0 )
  {
    v9 = *(u32 *)(v7 + 720);
    v8 = 8195;
    if ( v9 != 0 )
    {
      v8 = 6;
      if ( a2 != 0 )
      {
        v8 = 8194;
        if ( *(u8 *)(a1 + 36) >= (unsigned int)a2 )
        {
          v10 = mp4_codec_fn_64(v9, *(u8 *)(*(u32 *)(a1 + 32) + a2 - 1), v12);
          v8 = 8195;
          if ( v10 == 0 )
            return mp4_box_fn_23(a4, a3, *(u32 *)(v12[0] + 4), *(u32 *)(v12[0] + 8));
        }
      }
    }
  }
  return v8;
}


int mp4_box_fn_71(int a1, int a2)
{
  u32 *v3;
  int v4;
  int v5;
  int v6;
  int v7;
  u32 v9[4];

  v3 = *(u32 **)(a1 + 40);
  v4 = 8195;
  if ( v3 != NULL )
  {
    v5 = mp4_codec_fn_71(*v3 - 12, v3[3], v9);
    v4 = 8195;
    if ( v5 == 0 )
    {
      v6 = *(u32 *)(v9[0] + 4);
      v7 = v9[0];
      if ( *(u8 *)(v6 + 32) != 0 )
      {
        *(u8 *)a2 = *(u8 *)(v6 + 33);
        *(u8 *)(a2 + 1) = *(u8 *)(v6 + 34);
        *(u32 *)(a2 + 4) = *(u32 *)(v6 + 40);
        *(u32 *)(a2 + 8) = *(u32 *)(v6 + 44);
        *(u32 *)(a2 + 12) = *(u32 *)(v6 + 48);
        *(u32 *)(a2 + 16) = *(u32 *)(v6 + 56);
        mp4_codec_fn_66(v7);
        return 0;
      }
    }
  }
  return v4;
}


int mp4_box_fn_72(int a1, int a2, unsigned int a3, unsigned int a4)
{
  int result;
  u32 *v7;
  int v9; // dc
  int v10;
  int v11;
  u32 v12[4];

  result = 8195;
  v7 = *(u32 **)(a1 + 40);
  if ( v7 != NULL )
  {
    v9 = mp4_codec_fn_71(*v7 - 12, v7[3], v12) != 0;
    result = 8195;
    if ( !v9 )
    {
      v10 = *(u32 *)(v12[0] + 4);
      if ( *(u8 *)(v10 + 32) != 0 )
      {
        if ( a2 == 0 )
          a3 = *(u32 *)(v10 + 56);
        v11 = mp4_box_fn_23(a4, a3, *(u32 *)(v10 + 52), *(u32 *)(v10 + 56));
        mp4_codec_fn_66(v12[0]);
        return v11;
      }
      else
      {
        mp4_codec_fn_66(v12[0]);
        return 8195;
      }
    }
  }
  return result;
}


int mp4_box_fn_73(u32 *a1, int a2)
{
  int v2;
  int v3;
  int v4;
  s16 v5;
  s16 v6;
  s16 v7;
  s16 v8;

  v2 = a1[9];
  v3 = 4;
  if ( v2 != 0 )
  {
    v3 = 6;
    if ( a2 != 0 )
    {
      v4 = a1[1];
      v3 = 0;
      *(u16 *)(a2 + 8) = *(u16 *)v2;
      v5 = *(u16 *)(v2 + 2);
      *(u32 *)a2 = v4;
      *(u16 *)(a2 + 10) = v5;
      v6 = *(u16 *)(v2 + 4);
      *(u32 *)(a2 + 4) = a1[5];
      *(u16 *)(a2 + 12) = v6;
      v7 = *(u16 *)(v2 + 16);
      *(u32 *)(a2 + 16) = *(u32 *)(v2 + 8);
      *(u16 *)(a2 + 24) = v7;
      v8 = *(u16 *)(v2 + 18);
      *(u32 *)(a2 + 20) = *(u32 *)(v2 + 12);
      *(u16 *)(a2 + 26) = v8;
      *(u8 *)(a2 + 28) = *(u8 *)(v2 + 20);
    }
  }
  return v3;
}


int mp4_box_fn_74(u32 *a1, u32 *a2)
{
  int v2;

  v2 = 4112;
  if ( a1[1] == 1635148593 )
  {
    v2 = 8195;
    if ( a1[11] != 0 )
    {
      v2 = 8194;
      if ( a1[17] != 0 )
      {
        v2 = 0;
        *a2 = a1[16];
      }
    }
  }
  return v2;
}


int mp4_box_fn_75(u32 *a1, u32 *a2, u32 *a3)
{
  u32 *v3;
  int v4;

  v3 = (u32 *)*a1;
  v4 = 8195;
  if ( v3[59] != 0 )
  {
    v4 = 0;
    *a2 = v3[61];
    *a3 = v3[62];
  }
  return v4;
}


int mp4_box_fn_76(int *a1, u16 *a2, u16 *a3)
{
  int v5;
  int v6;
  int v7;

  v5 = *a1;
  v6 = *(u32 *)(*a1 + 12);
  v7 = 8195;
  if ( v6 != 0 )
  {
    mp4_box_fn_22(*(u32 *)(v5 + 24), a2);
    mp4_box_fn_22(*(u32 *)(*a1 + 28), a3);
    return 0;
  }
  return v7;
}


int mp4_box_fn_77(int *a1, u32 *a2)
{
  int v2;
  int v3;

  v2 = *a1;
  v3 = 8195;
  if ( *(u32 *)(v2 + 12) != 0 )
  {
    v3 = 0;
    *a2 = *(u32 *)(v2 + 32);
  }
  return v3;
}


int mp4_box_fn_78(u32 *a1, int a2)
{
  int v2;
  int v3;

  v2 = mp4_track_fn_79(*a1, a2, 1);
  v3 = 8195;
  if ( v2 != 7 )
  {
    v3 = 8;
    if ( (unsigned int)(v2 - 4120) >= 2 && v2 != 0x101B )
      return v2;
  }
  return v3;
}


int mp4_box_fn_79(int *a1, u16 *a2)
{
  int v2;
  int v3;

  v2 = *a1;
  v3 = 8195;
  if ( *(u32 *)v2 != 0 )
  {
    v3 = 0;
    *a2 = *(u16 *)(v2 + 8);
  }
  return v3;
}


int mp4_box_fn_80(u32 **a1, int a2, u32 *a3)
{
  u32 *v4;
  int v5;
  int v6;
  u32 v8[4];

  v4 = *a1;
  v5 = 8195;
  if ( *v4 != 0 )
  {
    v5 = 6;
    if ( a2 != 0 )
    {
      v6 = mp4_sample_fn_12(v4, a2, v8);
      v5 = 8194;
      if ( v6 == 0 )
      {
        v5 = 0;
        *a3 = v8[0];
      }
    }
  }
  return v5;
}


int mp4_box_fn_81(u32 *a1, u32 *a2)
{
  int v2;

  v2 = 8194;
  if ( a1[31] != 0 && a1[33] != 0 )
  {
    v2 = 0;
    *a2 = a1[36];
  }
  return v2;
}


int mp4_box_fn_82(int a1, u32 *a2)
{
  int v2;

  v2 = 8195;
  if ( *(u32 *)(a1 + 160) != 0 )
  {
    v2 = 0;
    *a2 = *(u32 *)(a1 + 180);
  }
  return v2;
}


int mp4_box_fn_83(int a1, u32 *a2)
{
  int v2;

  v2 = 8195;
  if ( *(u32 *)(a1 + 200) != 0 )
  {
    v2 = 0;
    *a2 = *(u32 *)(a1 + 216);
  }
  return v2;
}


int mp4_box_fn_84(int a1, int a2)
{
  int v2;
  int v3;

  v2 = mp4_codec_fn_24(a1 + 392, a2, 1);
  v3 = 8195;
  if ( v2 != 7 )
  {
    v3 = 8;
    if ( (unsigned int)(v2 - 4120) >= 2 && v2 != 0x101B )
      return v2;
  }
  return v3;
}


int mp4_box_fn_85(int a1, u32 *a2)
{
  int v2;

  v2 = 8195;
  if ( *(u32 *)(a1 + 416) != 0 )
  {
    v2 = 0;
    *a2 = *(u32 *)(a1 + 428);
  }
  return v2;
}


int mp4_box_fn_86(u32 *a1, int a2)
{
  int result;
  int v4;
  int v5;
  s16 v6;
  int v7;
  int v8;

  result = 4;
  if ( a1[4] == 0x76696465 || a1[4] == 0xFFFFFFFE )
  {
    v4 = a1[1];
    *(u32 *)(a2 + 4) = 0;
    *(u32 *)a2 = v4;
    v5 = a1[9];
    *(u32 *)(a2 + 8) = a1[5];
    v6 = *(u16 *)(v5 + 16);
    *(u32 *)(a2 + 12) = *(u32 *)(v5 + 4);
    v7 = *(u32 *)(v5 + 20);
    *(u16 *)(a2 + 24) = v6;
    *(u32 *)(a2 + 16) = v7;
    v8 = *(u32 *)(v5 + 24);
    *(u16 *)(a2 + 26) = *(u16 *)(v5 + 18);
    *(u32 *)(a2 + 20) = v8;
    mp4_box_fn_23(a2 + 28, 0x20u, v5 + 36, 0x20u);
    *(u16 *)(a2 + 60) = *(u16 *)(v5 + 68);
    return 0;
  }
  return result;
}


int mp4_box_fn_87(u32 *a1, int a2)
{
  int v3;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  s16 v10;
  s16 v11;
  u8 v12[32];
  s16 v13;

  v3 = 4;
  if ( a1[4] == 0x76696465 || a1[4] == 0xFFFFFFFE )
  {
    v5 = a1[9];
    v6 = a1[1];
    v7 = a1[5];
    v10 = *(u16 *)(v5 + 16);
    v8 = *(u32 *)(v5 + 20);
    v11 = *(u16 *)(v5 + 18);
    v9 = *(u32 *)(v5 + 24);
    mp4_box_fn_23((unsigned int)v12, 0x20u, v5 + 36, 0x20u);
    v3 = 0;
    v13 = *(u16 *)(v5 + 68);
    *(u32 *)(a2 + 16) = v9;
    *(u32 *)a2 = v6;
    *(u32 *)(a2 + 4) = v7;
    *(u16 *)(a2 + 8) = v10;
    *(u16 *)(a2 + 10) = v11;
    *(u32 *)(a2 + 12) = v8;
  }
  return v3;
}


int mp4_box_fn_88(u32 *a1, int a2)
{
  int result;
  int v4;
  int v5;
  s16 v6;
  u32 v7[4];
  s16 v8;
  int v9;

  result = mp4_box_fn_51(a1, NULL, (int)v7);
  if ( result == 0 )
  {
    v4 = v7[2];
    v5 = v7[0];
    v6 = v8;
    *(u32 *)(a2 + 12) = v9;
    *(u32 *)a2 = v5;
    *(u32 *)(a2 + 4) = v4;
    *(u16 *)(a2 + 8) = v6;
  }
  return result;
}


int mp4_box_fn_89(int a1, u32 *a2)
{
  u32 *v2;

  v2 = *(u32 **)(a1 + 40);
  if ( v2 != NULL )
  {
    if ( *v2 < 0xCu )
      *a2 = 0;
    else
      *a2 = *v2 - 12;
  }
  else
  {
    *a2 = 0;
  }
  return 0;
}


int mp4_box_fn_90(int a1, unsigned int a2, unsigned int a3)
{
  int v3;
  u32 *v4;

  v3 = 8195;
  v4 = *(u32 **)(a1 + 40);
  if ( v4 != NULL && *v4 >= 0xCu && v4[3] != 0 )
    return mp4_box_fn_23(a3, a2, v4[3], *v4 - 12);
  return v3;
}


int mp4_box_fn_91(int a1, u8 *a2, u8 *a3, u8 *a4)
{
  int v7;
  int v9;
  int v10;

  v7 = mp4_box_fn_98(1635148593, a1);
  v9 = 4112;
  if ( v7 != 0 )
  {
    v10 = *(u32 *)(a1 + 44);
    v9 = 8195;
    if ( v10 != 0 )
    {
      v9 = 0;
      *a2 = *(u8 *)(v10 + 9);
      a2[1] = *(u8 *)(*(u32 *)(a1 + 44) + 10);
      a2[2] = *(u8 *)(*(u32 *)(a1 + 44) + 11);
      a2[3] = *(u8 *)(*(u32 *)(a1 + 44) + 13);
      *a3 = *(u8 *)(*(u32 *)(a1 + 44) + 15);
      *a4 = *(u8 *)(*(u32 *)(a1 + 44) + 20);
    }
  }
  return v9;
}


int mp4_box_fn_92(int a1, u32 *a2)
{
  int v2;

  v2 = *(u32 *)(a1 + 12);
  if ( v2 != 0 )
    *a2 = mp4_box_fn_12(v2) + 1;
  else
    *a2 = 0;
  return 0;
}


int mp4_box_fn_93(int a1, unsigned int a2, int a3)
{
  int v3;
  int v7;
  int v8;

  v3 = 9;
  v7 = *(u32 *)(a1 + 12);
  if ( v7 != 0 )
  {
    v8 = mp4_box_fn_12(v7);
    v3 = 6;
    if ( a2 >= v8 + 1 )
    {
      mp4_box_fn_11(a3, *(u32 *)(a1 + 12), a2);
      return 0;
    }
  }
  return v3;
}


int mp4_box_fn_94(u32 **a1, u32 *a2)
{
  u32 *v2;
  int v3;
  int v5;

  v2 = *a1;
  v3 = 4;
  if ( v2 != NULL )
  {
    v3 = 8195;
    if ( *v2 != 0 && v2[1] == 1836019574 )
    {
      v5 = v2[41];
      *a2 = v2[40];
      a2[1] = v5;
      return 0;
    }
  }
  return v3;
}


int mp4_box_fn_95(u32 *a1, u32 *a2)
{
  u32 *v2;
  int v3;

  v2 = (u32 *)*a1;
  v3 = 8195;
  if ( v2[46] != 0 )
  {
    v3 = 8194;
    if ( v2[53] != 0 )
    {
      v3 = 0;
      *a2 = *(u32 *)(v2[56] + 12);
    }
  }
  return v3;
}


int mp4_box_fn_96(int a1, u8 *a2)
{
  u32 *v3;
  int v4;
  int v5;
  u8 *v6;
  int v7;
  u32 v9[4];

  v3 = *(u32 **)(*(u32 *)a1 + 132);
  v4 = 8194;
  if ( v3 != NULL )
  {
    v5 = mp4_codec_fn_70(*v3 - 12, v3[3], v9);
    v4 = 8195;
    if ( v5 == 0 )
    {
      v6 = *(u8 **)(v9[0] + 4);
      v7 = v9[0];
      *a2 = v6[22];
      a2[1] = v6[23];
      a2[2] = v6[20];
      mp4_codec_fn_66(v7);
      return 0;
    }
  }
  return v4;
}


int mp4_box_fn_97(u32 *a1, unsigned int a2, unsigned int a3)
{
  int v6;
  unsigned int v8[4];

  v8[0] = 0;
  v6 = mp4_box_fn_74(a1, v8);
  if ( v6 == 0 )
  {
    v6 = 6;
    if ( a2 >= v8[0] )
    {
      mp4_box_fn_23(a3, a2, a1[17], a1[16]);
      return 0;
    }
  }
  return v6;
}


int mp4_box_fn_98(int a1, int a2)
{
  int v2;
  int v4;
  int v5;

  v2 = *(u32 *)(a2 + 4);
  v4 = 0;
  if ( v2 == a1 )
    return true;
  if ( v2 == 0x656E6376 || v2 == 0x656E6361 )
  {
    v5 = *(u32 *)(a2 + 48);
    if ( v5 != 0 )
      return __builtin_bswap32(*(u32 *)(*(u32 *)(v5 + 12) + 8)) == a1;
  }
  return v4;
}


int mp4_box_fn_99(int *a1)
{
  int v2;
  u32 v4[4];

  v2 = *a1;
  v4[0] = 0;
  if ( mp4_box_fn_6((int)v4, 4, v2) != 4 )
  {
    v4[0] = 0;
    mp4_io_fn_12(0, 0x80000000, a1);
  }
  return v4[0];
}


int mp4_box_fn_100(int *a1)
{
  int v2;
  u32 v4[4];

  v2 = *a1;
  v4[0] = 0;
  if ( mp4_box_fn_6((int)v4, 4, v2) != 4 )
  {
    v4[0] = 0;
    mp4_io_fn_12(0, 0x80000000, a1);
  }
  return v4[0];
}


int mp4_box_fn_101(int *a1)
{
  int v2;
  u16 v4[8];

  v2 = *a1;
  v4[0] = 0;
  if ( mp4_box_fn_6((int)v4, 2, v2) != 2 )
  {
    v4[0] = 0;
    mp4_io_fn_12(0, 0x80000000, a1);
  }
  return v4[0];
}


int mp4_box_fn_102(int *a1)
{
  int v2;
  u16 v4[8];

  v2 = *a1;
  v4[0] = 0;
  if ( mp4_box_fn_6((int)v4, 2, v2) != 2 )
  {
    v4[0] = 0;
    mp4_io_fn_12(0, 0x80000000, a1);
  }
  return v4[0];
}


int mp4_box_fn_103(int a1, int a2, int *a3)
{
  int v5;

  v5 = mp4_box_fn_5(a1, a2, *a3);
  if ( v5 != a2 )
    mp4_io_fn_12(0, 0x80000000, a3);
  return v5;
}


int mp4_box_fn_104(int a1, unsigned int a2, int *a3)
{
  u8 *v5;
  int v6;
  unsigned int i;

  v5 = (u8 *)a1;
  v6 = mp4_box_fn_5(a1, a2, *a3);
  if ( v6 != a2 )
    mp4_io_fn_12(0, 0x80000000, a3);
  for ( i = 0; i < a2 >> 2; v5 += 4 )
  {
    ++i;
    *(u32 *)v5 = (*v5 << 24) + (v5[1] << 16) + (v5[2] << 8) + v5[3];
  }
  return v6;
}


unsigned int mp4_box_fn_105(u8 *a1, unsigned int a2, int *a3)
{
  unsigned int v3;
  unsigned int i;

  v3 = a2 >> 2;
  for ( i = 0; i < v3; a1 += 8 )
  {
    ++i;
    if ( mp4_box_fn_5((int)a1, 4, *a3) != 4 )
      mp4_io_fn_12(0, 0x80000000, a3);
    *(u32 *)a1 = (*a1 << 24) + (a1[1] << 16) + (a1[2] << 8) + a1[3];
    *((u32 *)a1 + 1) = 0;
  }
  return a2;
}


int mp4_box_fn_106(u32 *a1)
{
  int v1;

  v1 = 2;
  if ( a1[10] != 0 )
  {
    v1 = 0;
    *a1 = a1[9];
  }
  return v1;
}


int mp4_box_fn_107(u32 *a1)
{
  u32 *v2;
  int v3;
  int v4;
  int v5;
  u32 *v6;
  int v7;
  int v9;
  int v10;
  u32 v11[2];

  v2 = (u32 *)mp4_box_fn_7(28);
  v3 = 1;
  if ( v2 != NULL )
  {
    *v2 = 24;
    v2[1] = 1835430497;
    v4 = mp4_track_fn_16(&v9);
    if ( v4 == 0 )
    {
      v2[2] = v9;
      v4 = mp4_codec_fn_59(4, 10, &v10);
      v5 = 10;
      v6 = v11;
      if ( v4 == 0 )
      {
        v2[3] = v10;
        v4 = mp4_codec_fn_59(4, 10, v11);
        if ( v4 == 0 )
        {
          v7 = v11[0];
          *a1 = v2;
          v3 = 0;
          v2[5] = 0;
          v2[6] = 0;
          v2[4] = v7;
          return v3;
        }
        mp4_codec_fn_60(v2[3]);
      }
      mp4_track_fn_17(v2[2], v5, v6);
    }
    mp4_box_fn_9();
    return v4;
  }
  return v3;
}


int mp4_box_fn_108(u32 *a1, int a2, int a3)
{
  int v4;
  int v5;
  unsigned int v6;
  int v7;
  unsigned int v8;
  int v9;
  int v11;
  int v12;
  unsigned int v13;
  int v14;
  unsigned int v15;
  int v16;

  v4 = a1[2];
  if ( v4 != 0 )
    mp4_track_fn_17(v4, a2, a3);
  v5 = mp4_codec_fn_61(a1[3], &v13);
  if ( v5 == 0 )
  {
    v6 = 0;
    if ( v13 != 0 )
    {
      v7 = a1[3];
      while ( 1 )
      {
        v12 = mp4_codec_fn_62(v7, v6++, &v14);
        v5 = v12;
        if ( v12 != 0 )
          break;
        mp4_track_fn_8(v14);
        v7 = a1[3];
        if ( v6 >= v13 )
          goto LABEL_6;
      }
    }
    else
    {
      v7 = a1[3];
LABEL_6:
      if ( v7 != 0 )
        mp4_codec_fn_60(v7);
      v5 = mp4_codec_fn_61(a1[4], &v15);
      if ( v5 == 0 )
      {
        v8 = 0;
        if ( v15 != 0 )
        {
          v9 = a1[4];
          while ( 1 )
          {
            v11 = mp4_codec_fn_62(v9, v8++, &v16);
            v5 = v11;
            if ( v11 != 0 )
              break;
            mp4_track_fn_20(v16);
            v9 = a1[4];
            if ( v8 >= v15 )
              goto LABEL_11;
          }
        }
        else
        {
          v9 = a1[4];
LABEL_11:
          if ( v9 != 0 )
            mp4_codec_fn_60(v9);
          v5 = 0;
          if ( a1 != NULL )
          {
            mp4_box_fn_9();
            return 0;
          }
        }
      }
    }
  }
  return v5;
}


int mp4_box_fn_109(int a1, unsigned int a2, unsigned int a3, u32 *a4, u32 *a5)
{
  int v10;
  unsigned int v11;
  int i;
  int v14;
  int v15;
  unsigned int v16;
  int v17;
  unsigned int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  unsigned int j;
  unsigned int v26;
  unsigned int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  int v34;
  int v35;
  unsigned int v36;
  int v37;
  int v38;
  int v39;
  int v40;
  int v41;
  int v42;
  int v43;
  unsigned int v44;
  int v45;
  int v46;
  int v47;
  int v48;
  int v49;
  int v50;
  unsigned int v51;
  int v52;
  unsigned int v53;
  unsigned int v54;

  v10 = mp4_codec_fn_61(*(u32 *)(a1 + 12), &v51);
  if ( v10 == 0 )
  {
    v11 = 0;
    if ( v51 != 0 )
    {
      for ( i = *(u32 *)(a1 + 12); ; i = *(u32 *)(a1 + 12) )
      {
        v14 = mp4_codec_fn_62(i, v11++, &v52);
        v10 = v14;
        if ( v14 != 0 )
          break;
        v10 = mp4_track_fn_9(v52, &v53);
        if ( v10 != 0 )
          break;
        if ( a2 == v53 )
        {
          v10 = mp4_track_fn_10(v52, &v54);
          if ( v10 != 0 )
            return v10;
          v10 = 0;
          if ( v54 == 0 )
            goto LABEL_4;
          v10 = mp4_track_fn_11(v52, 1, &v35);
          if ( v10 != 0 )
            return v10;
          if ( v36 == a3 )
          {
            v15 = a3 < v36;
            if ( v35 == 0 )
            {
              v29 = v37;
              v30 = v38;
              v31 = v39;
              v32 = v40;
              v33 = v41;
              v34 = v42;
              v10 = 0;
              *a4 = 1;
              a5[7] = v34;
              a5[1] = a3;
LABEL_34:
              a5[2] = v29;
              a5[3] = v30;
              a5[4] = v31;
              a5[5] = v32;
              a5[6] = v33;
              *a5 = 0;
              return v10;
            }
          }
          else
          {
            v15 = a3 < v36;
          }
          if ( v15 )
          {
            v10 = 0;
            goto LABEL_4;
          }
          v16 = 2;
          if ( v54 >= 2 )
          {
            for ( j = v36; ; j = v36 )
            {
              v26 = v16;
              v44 = j;
              ++v16;
              v43 = v35;
              v45 = v37;
              v46 = v38;
              v47 = v39;
              v48 = v40;
              v49 = v41;
              v50 = v42;
              v10 = mp4_track_fn_11(v52, v26, &v35);
              if ( v10 != 0 )
                break;
              v27 = v36;
              if ( v36 == a3 )
              {
                v28 = v42;
                if ( v35 == 0 )
                {
                  v29 = v37;
                  v30 = v38;
                  v31 = v39;
                  v32 = v40;
                  v33 = v41;
                  *a4 = 1;
                  v10 = 0;
                  a5[7] = v28;
                  a5[1] = v27;
                  goto LABEL_34;
                }
              }
              if ( a3 < v36 )
              {
                v17 = v43;
                v18 = v44;
                v19 = v45;
                v20 = v46;
                v21 = v47;
                v22 = v48;
                v23 = v49;
                v24 = v50;
                v10 = 0;
                *a4 = 1;
                goto LABEL_23;
              }
              if ( v54 < v16 )
                goto LABEL_22;
            }
          }
          else
          {
LABEL_22:
            v17 = v35;
            v18 = v36;
            v19 = v37;
            v20 = v38;
            v21 = v39;
            v22 = v40;
            v23 = v41;
            v24 = v42;
            *a4 = 1;
            v10 = 0;
LABEL_23:
            a5[7] = v24;
            *a5 = v17;
            a5[1] = v18;
            a5[2] = v19;
            a5[3] = v20;
            a5[4] = v21;
            a5[5] = v22;
            a5[6] = v23;
          }
          return v10;
        }
        if ( a2 < v53 )
          goto LABEL_3;
        if ( v11 >= v51 )
        {
          v10 = 0;
          goto LABEL_4;
        }
      }
    }
    else
    {
LABEL_3:
      v10 = 0;
LABEL_4:
      *a4 = 0;
    }
  }
  return v10;
}


