#include "mp4msv_internal.h"

int mp4_sample_fn_1(int a1)
{
  int v1;
  unsigned int v2;
  int v3;
  u32 *v5;
  unsigned int v6;
  int result;
  u32 v8[4];

  v1 = a1 + 392;
  v2 = 0;
  v3 = 0;
  if ( *(u32 *)(a1 + 428) != 0 )
  {
    v5 = (u32 *)(a1 + 416);
    do
    {
      v6 = v2 + 1;
      if ( mp4_track_fn_73(v5, v2 + 1, v8) != 0 )
      {
        v3 = 1;
      }
      else if ( mp4_track_fn_72(*(u16 *)(v8[0] + 14), a1 + 368, (u32 *)(v8[0] + 20)) != 0 )
      {
        v3 = 1;
      }
      v2 = v6;
    }
    while ( v6 < *(u32 *)(v1 + 36) );
  }
  result = 20481;
  if ( v3 != 1 )
    return 0;
  return result;
}


int mp4_sample_fn_2(int a1, unsigned int a2, int a3, int a4, int a5, unsigned int a6, unsigned int a7)
{
  unsigned int v10;
  unsigned int v13;
  int v14;
  unsigned int v15;
  unsigned int v16;
  int v17;
  unsigned int v18;
  int v19;
  u16 v20;
  int v22;

  v10 = a2;
  if ( *(u32 *)(a1 + 16) == 0 )
  {
    v22 = mp4_box_fn_8(440);
    *(u32 *)(a1 + 16) = v22;
    v14 = 1;
    if ( v22 == 0 )
      return v14;
    *(u32 *)(a1 + 12) = 10;
  }
  v13 = *(u32 *)(a1 + 12);
  if ( a6 < v13 )
  {
LABEL_10:
    v19 = 44 * a6 + *(u32 *)(a1 + 16);
    *(u32 *)(v19 + 4) = a3;
    *(u16 *)(v19 + 8) = a4;
    *(u16 *)(v19 + 10) = a5;
    *(u16 *)v19 = v10;
    if ( v10 >= 0xB )
    {
      if ( v10 < 0x2A )
      {
        mp4_box_fn_23(v19 + 12, 0x20u, a7, v10 - 10);
        v20 = *(u16 *)(a1 + 8);
LABEL_14:
        v14 = 0;
        if ( a6 >= v20 )
          *(u16 *)(a1 + 8) = v20 + 1;
        return v14;
      }
      mp4_box_fn_23(v19 + 12, 0x20u, a7, 0x1Fu);
      *(u8 *)(44 * a6 + *(u32 *)(a1 + 16) + 43) = 0;
    }
    v20 = *(u16 *)(a1 + 8);
    goto LABEL_14;
  }
  v14 = 6;
  if ( a6 == v13 )
  {
    v15 = mp4_box_fn_8(44 * a6 + 440);
    v16 = v15;
    v17 = 1;
    if ( v15 != 0 )
    {
      v18 = 44 * a6;
      if ( 44 * a6 >= 44 * a6 + 440 )
        v18 = 44 * a6 + 440;
      mp4_box_fn_23(v15, 44 * a6 + 440, *(u32 *)(a1 + 16), v18);
      mp4_box_fn_9();
      *(u32 *)(a1 + 16) = v16;
      v17 = 0;
    }
    v14 = 1;
    if ( v17 != 1 )
    {
      *(u32 *)(a1 + 12) += 10;
      goto LABEL_10;
    }
  }
  return v14;
}


int mp4_sample_fn_3(int a1, int a2, int a3, int a4, int a5, unsigned int a6)
{
  unsigned int v12;
  int v13;
  unsigned int v14;
  unsigned int v15;
  unsigned int v16;
  unsigned int v17;
  int v18;
  unsigned int v19;
  int v20;
  int v21;
  int v22;
  u16 v23;
  int v25;

  if ( *(u32 *)(a1 + 16) == 0 )
  {
    v25 = mp4_box_fn_8(400);
    *(u32 *)(a1 + 16) = v25;
    v20 = 1;
    if ( v25 == 0 )
      return v20;
    *(u32 *)(a1 + 12) = 10;
  }
  v12 = *(u32 *)(a1 + 12);
  if ( a6 < v12 )
  {
    v13 = *(u32 *)(a1 + 16);
LABEL_10:
    v21 = 40 * a6 + v13;
    v22 = *(u32 *)(v21 + 36);
    *(u16 *)v21 = a2;
    *(u32 *)(v21 + 4) = a3;
    *(u16 *)(v21 + 8) = a4;
    *(u16 *)(v21 + 10) = a5;
    mp4_track_fn_65(v22);
    v23 = *(u16 *)(a1 + 8);
    *(u32 *)(40 * a6 + *(u32 *)(a1 + 16) + 36) = 0;
    if ( a6 >= v23 )
      *(u16 *)(a1 + 8) = v23 + 1;
    return 0;
  }
  v14 = 40 * v12;
  v15 = 40 * v12 + 400;
  v16 = mp4_box_fn_8(v15);
  v17 = v16;
  v18 = 1;
  if ( v16 != 0 )
  {
    v19 = v14;
    if ( v14 >= v15 )
      v19 = v15;
    mp4_box_fn_23(v16, v15, *(u32 *)(a1 + 16), v19);
    mp4_box_fn_9();
    *(u32 *)(a1 + 16) = v17;
    v18 = 0;
  }
  v20 = 1;
  if ( v18 != 1 )
  {
    *(u32 *)(a1 + 12) += 10;
    v13 = *(u32 *)(a1 + 16);
    goto LABEL_10;
  }
  return v20;
}


int mp4_sample_fn_4(int a1, unsigned int a2, int a3, int a4, int a5, unsigned int a6, unsigned int a7)
{
  unsigned int v7;
  int v11;
  unsigned int v12;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  unsigned int v19;

  v7 = a2;
  v11 = mp4_sample_fn_3(a1, a2, a3, a4, a5, a6);
  if ( v11 == 0 )
  {
    v12 = v7 - 10;
    if ( v7 >= 0xB )
    {
      v14 = 40 * a6;
      v15 = v7 - 10;
      v17 = v14 + *(u32 *)(a1 + 16);
      v16 = mp4_box_fn_8(v15);
      v18 = *(u32 *)(a1 + 16);
      *(u32 *)(v17 + 36) = v16;
      v19 = *(u32 *)(v14 + v18 + 36);
      v11 = 1;
      if ( v19 != 0 )
      {
        mp4_box_fn_23(v19, v12, a7, v12);
        return 0;
      }
    }
    else
    {
      return 0;
    }
  }
  return v11;
}


int mp4_sample_fn_5(int a1)
{
  return mp4_box_fn_8(a1);
}


int mp4_sample_fn_6(int a1)
{
  return mp4_box_fn_7(a1);
}


int mp4_sample_fn_7(unsigned int *a1, unsigned int a2, unsigned int a3)
{
  int v5;
  unsigned int v7;
  unsigned int v8;
  int result;

  v5 = mp4_box_fn_8(a3);
  v7 = v5;
  if ( a2 >= a3 )
    a2 = a3;
  v8 = v5;
  result = 1;
  if ( v7 != 0 )
  {
    mp4_box_fn_23(v8, a3, *a1, a2);
    mp4_box_fn_9();
    *a1 = v7;
    return 0;
  }
  return result;
}


int mp4_sample_fn_8(int a1)
{
  int v2;
  int result;

  mp4_track_fn_65(*(u32 *)(a1 + 24));
  v2 = *(u32 *)(a1 + 12);
  *(u32 *)(a1 + 24) = 0;
  result = mp4_track_fn_65(v2);
  *(u32 *)(a1 + 12) = 0;
  return result;
}


int mp4_sample_fn_9(u32 *a1, int a2, int a3, int a4, int a5)
{
  int v6;

  *(u32 *)(a5 + 32) = 0;
  v6 = mp4_sample_fn_19();
  if ( v6 == 20481 )
    return v6;
  if ( (unsigned int)(a1[59] - 1) < 0xF )
    a1[59] = 16;
  if ( a1[46] != 0 )
    mp4_codec_fn_130(a1 + 46);
  if ( *a1 == 0 )
    return v6;
  mp4_codec_fn_113(a1);
  return v6;
}


int mp4_sample_fn_10(u32 *a1, int a2, int a3)
{
  int v5;

  *(u32 *)(a3 + 32) = 0;
  v5 = mp4_io_fn_2();
  if ( (*(u32 *)(a3 + 4) & 0xE0000000) != 0 )
    v5 = 20481;
  if ( v5 == 20481 )
    return 20481;
  if ( (unsigned int)(a1[59] - 1) < 0xF )
    a1[59] = 16;
  if ( a1[46] != 0 )
    mp4_codec_fn_130(a1 + 46);
  if ( *a1 != 0 )
    mp4_codec_fn_113(a1);
  return v5;
}


int mp4_sample_fn_11(int a1, u32 *a2)
{
  int v2;

  v2 = 8194;
  if ( a1 != 0 )
  {
    v2 = 0;
    *a2 = *(u32 *)(a1 + 32);
  }
  return v2;
}


int mp4_sample_fn_12(int a1, int a2, int *a3)
{
  unsigned int v5;
  int v6;
  int v7;
  int v8;
  int v10;

  v5 = 0;
  v6 = a1;
  if ( *(u32 *)(a1 + 8) == 0 )
    return 20481;
  while ( 1 )
  {
    v7 = mp4_track_fn_69(a1, v5);
    a1 = v6;
    if ( *(u32 *)(v7 + 32) == a2 )
      break;
    if ( ++v5 >= *(u32 *)(v6 + 8) )
      return 20481;
  }
  v10 = mp4_track_fn_69(v6, v5);
  v8 = 0;
  *a3 = v10;
  return v8;
}


int mp4_sample_fn_13(int a1, unsigned int a2, unsigned int a3, int a4)
{
  int v4;
  int v5;

  v4 = 20481;
  if ( a1 != 0 )
  {
    v4 = 20490;
    if ( a2 < *(u32 *)(a1 + 12) && a3 < 3 )
    {
      v5 = mp4_codec_fn_91(*(u32 *)(a1 + 16), a2, a3, a4);
      v4 = 20481;
      if ( v5 == 0 )
        return 0;
    }
  }
  return v4;
}


int mp4_sample_fn_14(int a1, u32 *a2)
{
  int v2;

  v2 = 8194;
  if ( a1 != 0 )
  {
    *a2 = *(u32 *)(a1 + 680);
    if ( *(u32 *)(a1 + 680) != 0 )
      return 0;
  }
  return v2;
}


int mp4_sample_fn_15(unsigned int a1, int a2)
{
  return (a1 & (1 << a2)) >> a2;
}


int mp4_sample_fn_16(u32 *a1, int a2)
{
  int v2;

  v2 = 20494;
  if ( !__CFADD__(*a1, a2) )
  {
    *a1 += a2;
    return 0;
  }
  return v2;
}


int mp4_sample_fn_17(u64 *a1, unsigned int a2)
{
  u64 v2;
  int result;

  v2 = *a1 + a2;
  if ( v2 < *a1 )
    return 20494;
  result = 0;
  if ( v2 < a2 )
    return 20494;
  *a1 = v2;
  return result;
}


int mp4_sample_fn_18(int a1, int a2, int a3, int a4, int a5, unsigned int a6, int a7, u32 *a8)
{
  int v12;
  int v13;

  v12 = mp4_sample_fn_3(a1, a2, a3, a4, a5, a6);
  if ( v12 == 0 )
  {
    v12 = 0;
    v13 = 40 * a6 + *(u32 *)(a1 + 16);
    *a8 = v13 + 12;
    *(u32 *)(v13 + 12) = a7;
    *(u32 *)(v13 + 32) = v13;
  }
  return v12;
}


int mp4_sample_fn_19(int a1, int a2, int a3, int a4, int a5, int a6)
{
  int v7;
  int v9;

  *(u32 *)(a5 + 4) = 0;
  v7 = mp4_sample_fn_20();
  v9 = 20481;
  if ( v7 != 20481 && (*(u32 *)(a5 + 4) & 0xE0000000) == 0 )
  {
    v9 = 20485;
    if ( (u8)*(u32 *)(a5 + 4) == 0 )
    {
      v9 = 8207;
      if ( a6 == 1 )
      {
        if ( v7 != 8207 )
          return 0;
      }
      else
      {
        return 0;
      }
    }
  }
  return v9;
}


int mp4_sample_fn_20(int a1, s64 a2, int *a3, int a4)
{
  int v5;
  unsigned int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  unsigned int v15;
  unsigned int v16;
  int v17;
  int v18;
  s64 v19;
  int v20;
  int *v21;
  int v22;
  int v23;
  s64 v24;
  int v25;
  unsigned int v26;
  unsigned int v27;
  int v28;
  int v29;
  int v30;
  int v32;
  int v33;
  int v34;
  int v35;
  int v36;
  int v37;
  unsigned int v38;
  int *v39;
  int v40;
  int v41;
  int v42;
  int v43;
  int v44;
  unsigned int v45;
  u32 *v46;
  u32 *v47;
  int v48;
  int v49;
  int *v50;
  int v51;
  int v52;
  int v53;
  unsigned int v54;
  unsigned int v55;
  int v56;
  unsigned int v57;
  int v58;
  int v59;
  unsigned int v60;
  int v61;
  unsigned int v62;
  unsigned int v63;
  unsigned int v64;
  s64 v65;
  int v66;
  unsigned int v67;
  int v68;
  unsigned int i;
  int v70;
  int v71;
  unsigned int v72;
  int v73;
  int v74;
  unsigned int v75;
  int v76;
  int v77;
  int v78;
  int v79;
  int v80;
  u32 *v81;
  unsigned int v82;
  int v83;
  int v84;
  unsigned int v85;
  int v86;
  int v87;
  unsigned int v88;
  int v89;
  u32 v90[4];
  char v91[8];
  s64 v92;
  unsigned int v93;
  int v94;
  int v95;
  unsigned int v96;
  u32 *v97;
  int v98;
  int v99;
  s64 v100;

  v5 = 0;
  v100 = a2;
  v92 = a2;
  v94 = 0;
  v99 = 0;
  if ( HIDWORD(a2) - 1 + ((u32)a2 != 0) != 0 || (unsigned int)(a2 - 1) >= 7 )
  {
    if ( HIDWORD(v92) == 0 )
    {
      v29 = v92;
      if ( (unsigned int)v92 < 8 )
      {
        v30 = HIDWORD(v92);
LABEL_28:
        if ( (v29 | v30) != 0 )
          mp4_io_fn_12(1936549485, 1, a3);
        if ( a4 != 1 )
          return 0;
        v10 = 8207;
        if ( *(u32 *)a1 != 0 )
          return 0;
        return v10;
      }
    }
    while ( 1 )
    {
      v9 = mp4_box_fn_99(a3);
      v93 = v9;
      v10 = 0;
      if ( v9 == 0 )
        return v10;
      if ( v9 < 8 )
        goto LABEL_155;
      v11 = mp4_box_fn_99(a3);
      v95 = v11;
      if ( HIDWORD(v92) != 0 )
        goto LABEL_7;
      if ( (unsigned int)v92 < v93 )
        break;
      v12 = *a3;
LABEL_8:
      v13 = mp4_box_fn_2(v12);
      v14 = v95;
      v15 = 860111956;
      v16 = v13 + v93;
      v18 = v17 + (v16 < v93);
      if ( ((*(u8 *)(a1 + 284) ^ 1) & 1 & (v95 == 0x6D667261)) == 0 )
        v15 = v95;
      v19 = ((u64)(v19) & 0xFFFFFFFF00000000ULL) | (u32)(v16 - 8);
      v19 = ((u64)(v19) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v18 - 1 + (v16 - 8 < 0xFFFFFFF8)) << 32);
      if ( v15 == 1835295092 )
        goto LABEL_17;
      v10 = 1835991040;
      if ( v15 > 0x6D646174 )
      {
        switch ( v15 )
        {
          case 0x6D667261u:
            v20 = v95;
            if ( v93 < 8 )
            {
              v21 = a3;
              v22 = 0x20000000;
              goto LABEL_16;
            }
            v21 = a3;
            if ( *(u32 *)(a1 + 280) != 0 )
              goto LABEL_42;
            if ( mp4_box_fn_107(&v97) != 0 )
            {
              v25 = a3[1];
            }
            else
            {
              v35 = mp4_track_fn_4((int)v97, *a3, v93 - 8, &v98);
              a3[1] |= v98;
              if ( v35 == 0 )
              {
                *(u32 *)(a1 + 280) = v97;
                goto LABEL_20;
              }
              mp4_box_fn_108(v97, v36, v37);
              v25 = a3[1];
            }
            break;
          case 0x6D6F6F66u:
            if ( v5 != 0 )
            {
              if ( v99 != 0 )
                *(u64 *)(v99 + 560) = v19 - v93;
            }
            else
            {
              *(u64 *)(a1 + 144) = v19 - v93;
              a3[8] = 1;
              v89 = mp4_box_fn_19(a3[21]);
              v10 = 0;
              if ( v89 )
                return v10;
            }
            if ( a4 == 1 )
            {
              v73 = mp4_io_fn_13(&v94, v95, 568, v15, v19, HIDWORD(v19), a3);
              v74 = v94;
              if ( v73 != 20481 )
              {
                v75 = v93;
                v76 = v94;
                *(u32 *)v94 = v93;
                *(u32 *)(v74 + 4) = v95;
                v77 = mp4_track_fn_28(v76, v75 - 8, a3);
                v78 = v94;
                if ( v77 == 20481 )
                {
                  v85 = 0;
                  v86 = v94;
                  if ( *(u32 *)(v94 + 24) != 0 )
                  {
                    do
                    {
                      v87 = 4 * v85;
                      mp4_track_fn_37(*(u32 *)(4 * v85 + v86 + 28));
                      v78 = v94;
                      v85 = (u8)(v85 + 1);
                      v88 = *(u32 *)(v94 + 24);
                      *(u32 *)(v87 + v94 + 28) = 0;
                      v86 = v78;
                    }
                    while ( v85 < v88 );
                    *(u32 *)(v78 + 24) = 0;
                  }
                  else
                  {
                    *(u32 *)(v94 + 24) = 0;
                  }
                }
                mp4_track_fn_42(a1, v78);
                v79 = v94;
                v80 = *(u32 *)(v94 + 24);
                *(u64 *)(v94 + 544) = v19 - v93;
                v81 = NULL;
                if ( v80 != 0 )
                {
                  v82 = 0;
                  do
                  {
                    v83 = 4 * v82;
                    v84 = *(u32 *)(4 * v82++ + v79 + 28);
                    mp4_track_fn_49(v81, (u32 *)v79, v84);
                    v79 = v94;
                    v81 = *(u32 **)(v83 + v94 + 28);
                  }
                  while ( v82 < *(u32 *)(v94 + 24) );
                }
                v5 = (u16)(v5 + 1);
                v99 = v79;
                *(u64 *)(v79 + 560) = v100;
                v94 = 0;
              }
              goto LABEL_20;
            }
            if ( a4 == 2 )
            {
              v23 = *a3;
              v24 = v19;
              v5 = (u16)(v5 + 1);
LABEL_19:
              mp4_box_fn_24(v23, v10, v24, SHIDWORD(v24));
LABEL_20:
              v25 = a3[1];
              break;
            }
            if ( a4 != 3 )
            {
LABEL_72:
              v21 = a3;
              v20 = 1936549485;
LABEL_15:
              v22 = 4;
LABEL_16:
              mp4_io_fn_12(v20, v22, v21);
LABEL_17:
              v23 = *a3;
LABEL_18:
              v24 = v19;
              goto LABEL_19;
            }
            v5 = (u16)(v5 + 1);
            mp4_box_fn_3(*a3, 1936523264, v100 - 12, HIDWORD(v100) - 1 + ((unsigned int)(v100 - 12) < 0xFFFFFFF4), 0);
            mp4_box_fn_6((int)&v95, 4, *a3);
            v71 = *a3;
            if ( v95 != 1835430511 )
            {
              mp4_box_fn_3(v71, v70, 0, 0, 2);
              v72 = v93;
LABEL_134:
              v92 = v72;
              v25 = a3[1];
              break;
            }
            mp4_box_fn_3(v71, v70, v100 - 4, HIDWORD(v100) - 1 + ((unsigned int)(v100 - 4) < 0xFFFFFFFC), 0);
            mp4_box_fn_6((int)&v96, 4, *a3);
            mp4_box_fn_3(
              *a3,
              SHIDWORD(v100),
              v100 - v96 + 4,
              ((v100 - (u64)v96) >> 32) + ((unsigned int)v100 - v96 >= 0xFFFFFFFC),
              0);
            mp4_box_fn_6((int)&v95, 4, *a3);
            if ( v95 == 1835430497 )
            {
              mp4_box_fn_3(*a3, 1835430497, v100 - v96, (v100 - (u64)v96) >> 32, 0);
              v72 = v93 + v96;
              goto LABEL_134;
            }
            v25 = a3[1];
            break;
          case 0x6D6F6F76u:
            v20 = v95;
            if ( *(u32 *)a1 != 0 )
            {
LABEL_41:
              v21 = a3;
LABEL_42:
              v22 = 8;
              goto LABEL_16;
            }
            v60 = v93;
            *(u32 *)(a1 + 152) = a1;
            *(u32 *)(a1 + 4) = v14;
            *(u32 *)a1 = v60;
            v61 = mp4_sample_fn_21(a1, v60 - 8, a3);
            v10 = 20481;
            if ( v61 == 20481 )
              return v10;
            v62 = *(u32 *)(a1 + 8);
            v63 = 0;
            if ( v62 != 0 )
            {
              for ( i = 0; ; i = v63 )
              {
                ++v63;
                *(u32 *)(mp4_track_fn_69(a1, i) + 104) = *(u32 *)(a1 + 32);
                v62 = *(u32 *)(a1 + 8);
                if ( v63 >= v62 )
                  break;
              }
              v64 = v93;
            }
            else
            {
              v64 = v93;
            }
            v65 = v100;
            *(u64 *)(a1 + 160) = v19 - v64;
            *(u64 *)(a1 + 144) = v65;
            if ( a4 != 0 )
            {
              v66 = *(u32 *)(a1 + 128);
              v67 = 0;
              if ( v62 != 0 && v66 != 0 )
              {
                v68 = v66 + 392;
                do
                {
                  *(u32 *)(v66 + 680) = v68;
                  ++v67;
                  v66 = *(u32 *)(v66 + 8);
                  v68 = v66 + 392;
                }
                while ( v67 < v62 && v66 != 0 );
              }
              *(u32 *)(a1 + 272) = 1;
              goto LABEL_20;
            }
            v25 = a3[1];
            break;
          case 0x75756964u:
            v38 = 0;
            if ( v93 < 0x18 )
            {
              v44 = v95;
              goto LABEL_83;
            }
            v39 = v90;
            do
            {
              ++v38;
              *v39++ = mp4_box_fn_99(a3);
            }
            while ( v38 < 4 );
            if ( v90[1] != 567431118 )
            {
              v40 = 0;
LABEL_62:
              if ( v40 != 1 )
              {
                v21 = a3;
                if ( v40 == 0 )
                {
                  v41 = mp4_box_fn_2(*a3);
                  mp4_io_fn_14(a1, v43, v41 - 24, v42 - 1 + ((unsigned int)(v41 - 24) < 0xFFFFFFE8), v93, v90);
                  v23 = *a3;
                  goto LABEL_18;
                }
LABEL_66:
                v22 = 4;
                goto LABEL_67;
              }
              switch ( v90[0] )
              {
                case 0x4C494349:
                  v20 = 1279869769;
                  v22 = 8;
                  if ( *(u32 *)(a1 + 288) == 0 )
                  {
                    v54 = v93;
                    *(u32 *)(a1 + 292) = v90[0];
                    *(u32 *)(a1 + 288) = v54;
                    if ( v54 >= 0x20 && (v55 = mp4_box_fn_99(a3), mp4_box_fn_99(a3), v55 >= 8) && v93 >= v55 + 24 )
                    {
                      *(u32 *)(a1 + 296) = v55 - 8;
                      v56 = mp4_sample_fn_5(v55 - 8);
                      *(u32 *)(a1 + 300) = v56;
                      if ( v56 != 0 )
                      {
                        mp4_box_fn_103(v56, v55 - 8, a3);
                        v57 = mp4_box_fn_99(a3);
                        mp4_box_fn_99(a3);
                        if ( v57 < 8 || v93 < v55 + v57 + 24 )
                          goto LABEL_93;
                        v58 = v57 - 8;
                        *(u32 *)(a1 + 304) = v58;
                        v59 = mp4_sample_fn_5(v58);
                        *(u32 *)(a1 + 308) = v59;
                        if ( v59 != 0 )
                        {
                          mp4_box_fn_103(v59, v58, a3);
                          v23 = *a3;
                          goto LABEL_18;
                        }
                      }
                      v20 = 1279869769;
                      v22 = 0x40000000;
                    }
                    else
                    {
LABEL_93:
                      v20 = 1279869769;
                      v22 = 0x20000000;
                    }
                  }
                  v21 = a3;
                  goto LABEL_16;
                case 0x4D545344:
                  if ( *(u64 *)(a1 + 320) == 0 )
                  {
                    v52 = mp4_box_fn_2(*a3);
                    v10 = 0;
                    *(u32 *)(a1 + 320) = v93;
                    *(u32 *)(a1 + 324) = 0;
                    v24 = v19;
                    v23 = *a3;
                    *(u32 *)(a1 + 312) = v52 - 24;
                    *(u32 *)(a1 + 316) = v53 - 1 + ((unsigned int)(v52 - 24) < 0xFFFFFFE8);
                    goto LABEL_19;
                  }
                  v34 = 8;
                  break;
                case 0x50524F46:
                  v34 = 0x20000000;
                  if ( v93 >= 0xC )
                  {
                    mp4_box_fn_103((int)v91, 1, a3);
                    if ( v91[0] == 0 )
                    {
                      if ( v93 < 0x20 )
                      {
                        v44 = *(u32 *)(a1 + 188);
LABEL_83:
                        v50 = a3;
                        v51 = 0x20000000;
                        goto LABEL_84;
                      }
                      v45 = 0;
                      if ( *(u32 *)(a1 + 184) == 0 )
                      {
                        *(u32 *)(a1 + 184) = v93;
                        v46 = (u32 *)(a1 + 192);
                        v47 = v90;
                        *(u32 *)(a1 + 188) = v95;
                        do
                        {
                          ++v45;
                          *v46++ = *v47++;
                        }
                        while ( v45 < 4 );
                        *(u8 *)(a1 + 208) = v91[0];
                        mp4_box_fn_103(a1 + 209, 3, a3);
                        v48 = mp4_box_fn_99(a3);
                        v49 = mp4_sample_fn_31(a1 + 184, v48, a3);
                        v10 = 20481;
                        if ( v49 != 0 )
                          return v10;
                        goto LABEL_20;
                      }
                      v21 = a3;
                      v22 = 8;
LABEL_67:
                      v20 = v95;
                      goto LABEL_16;
                    }
                    v21 = a3;
                    goto LABEL_66;
                  }
                  break;
                default:
                  goto LABEL_72;
              }
              v33 = v95;
              goto LABEL_43;
            }
            v40 = 0;
            if ( v90[2] != -1148688036 || v90[3] != -87439552 )
              goto LABEL_62;
            if ( v90[0] != 1279869769 )
            {
              if ( v90[0] <= 0x4C494349u )
              {
                v40 = -1;
                goto LABEL_62;
              }
              if ( v90[0] != 1297371972 )
              {
                if ( v90[0] == 1347571526 )
                  v40 = 1;
                else
                  v40 = -1;
                goto LABEL_62;
              }
            }
            v40 = 1;
            goto LABEL_62;
          default:
            goto LABEL_14;
        }
      }
      else
      {
        if ( v15 == 1718773093 )
          goto LABEL_17;
        if ( v15 != 1718909296 )
        {
LABEL_14:
          v20 = 1936549485;
          v21 = a3;
          goto LABEL_15;
        }
        if ( v93 < 0x10 )
        {
          v33 = 1718909296;
          v34 = 0x20000000;
LABEL_43:
          mp4_io_fn_12(v33, v34, a3);
          v25 = a3[1];
          goto LABEL_21;
        }
        v20 = v95;
        if ( *(u32 *)(a1 + 236) != 0 )
          goto LABEL_41;
        mp4_sample_fn_30(a1 + 236, &v92, &v93, a3);
        v25 = a3[1];
      }
LABEL_21:
      v10 = 20481;
      if ( (v25 & 0xE0000000) != 0 )
        return v10;
      v26 = mp4_box_fn_2(*a3);
      if ( v27 >= HIDWORD(v19) )
      {
        if ( HIDWORD(v19) != v27 )
          goto LABEL_25;
        v28 = v95;
        if ( v26 >= (unsigned int)v19 )
          goto LABEL_25;
      }
      else
      {
        v28 = v95;
      }
      mp4_io_fn_12(v28, 1, a3);
      mp4_box_fn_24(*a3, v32, v19, SHIDWORD(v19));
LABEL_25:
      v92 -= v93;
      if ( HIDWORD(v92) == 0 )
      {
        v29 = v92;
        if ( (unsigned int)v92 < 8 )
        {
          v30 = HIDWORD(v92);
          goto LABEL_28;
        }
      }
    }
    mp4_io_fn_12(v11, 1, a3);
    v93 = v92;
LABEL_7:
    v12 = *a3;
    goto LABEL_8;
  }
LABEL_155:
  v50 = a3;
  v44 = 1936549485;
  v51 = 1;
LABEL_84:
  mp4_io_fn_12(v44, v51, v50);
  return 0;
}


int mp4_sample_fn_21(int a1, unsigned int a2, int *a3)
{
  unsigned int v4;
  unsigned int v6;
  int v7;
  int v8;
  unsigned int v9;
  int v10;
  unsigned int v11;
  unsigned int v12;
  int v13;
  int *v14;
  int v15;
  int v16;
  int v17;
  int v18;
  unsigned int v19;
  unsigned int v20;
  int v21;
  int v23;
  unsigned int *v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  char v30;
  int v31;
  int v32;
  int *v33;
  int v34;
  unsigned int *v35;
  unsigned int *v36;
  unsigned int v37;
  unsigned int v38;
  int *v39;
  unsigned int v40;
  unsigned int *v41;
  int v42;
  unsigned int v43;
  int v44;
  int v45;
  u32 *v46;
  u32 v47[4];
  char v48[4];
  int v49;

  v4 = a2;
  while ( v4 >= 8 )
  {
    v6 = mp4_box_fn_99(a3);
    if ( v6 < 8 )
      goto LABEL_22;
    v7 = mp4_box_fn_99(a3);
    v8 = v7;
    if ( v4 < v6 )
    {
      mp4_io_fn_12(v7, 1, a3);
      v6 = v4;
    }
    v9 = mp4_box_fn_2(*a3) + v6;
    v11 = v9 - 8;
    v12 = v10 + (v9 < v6) - 1 + (v9 - 8 < 0xFFFFFFF8);
    if ( v8 == 1836476516 )
    {
      if ( v6 < 0xC )
        goto LABEL_90;
      mp4_box_fn_103((int)v48, 1, a3);
      v14 = a3;
      v13 = 1836476516;
      if ( v48[0] != 0 )
        goto LABEL_10;
      if ( v6 < 0x6C )
      {
LABEL_90:
        v26 = 1836476516;
LABEL_32:
        v27 = 0x20000000;
LABEL_33:
        mp4_io_fn_12(v26, v27, a3);
        v17 = a3[1];
        goto LABEL_13;
      }
      v14 = a3;
      v13 = 1836476516;
      if ( *(u32 *)(a1 + 12) != 0 )
        goto LABEL_73;
      *(u32 *)(a1 + 12) = v6;
      strcpy((char *)(a1 + 16), "dhvm");
      mp4_io_fn_11(a1 + 12, a3, a3);
      v17 = a3[1];
    }
    else
    {
      if ( (unsigned int)v8 > 0x6D766864 )
      {
        if ( v8 == 1969517665 )
        {
          if ( mp4_io_fn_13(a1 + 172, 1969517665, 16, 1836476516, v11, v12, a3) == 20481 )
          {
            mp4_io_fn_12(1969517665, 0x40000000, a3);
            v44 = 20481;
          }
          else
          {
            v46 = *(u32 **)(a1 + 172);
            *v46 = 8;
            v46[1] = 1969517665;
            mp4_box_fn_24(*a3, v45, v11, v12);
            v44 = 0;
          }
LABEL_78:
          v18 = 20481;
          if ( v44 == 20481 )
            return v18;
          v17 = a3[1];
          goto LABEL_13;
        }
        if ( (unsigned int)v8 > 0x75647461 )
        {
          if ( v8 != 1970628964 )
            goto LABEL_9;
          v38 = 0;
          v39 = v47;
          if ( v6 < 0x18 )
          {
            v26 = 1431522644;
            v27 = 1;
            goto LABEL_33;
          }
          do
          {
            ++v38;
            *v39++ = mp4_box_fn_99(a3);
          }
          while ( v38 < 4 );
          if ( v47[1] != 567431118 || v47[2] != -1148688036 )
          {
            v14 = a3;
LABEL_66:
            v13 = 1431522644;
LABEL_10:
            v15 = 4;
LABEL_11:
            mp4_io_fn_12(v13, v15, v14);
            mp4_box_fn_24(*a3, v16, v11, v12);
LABEL_12:
            v17 = a3[1];
            goto LABEL_13;
          }
          v14 = a3;
          if ( v47[3] != -87439552 )
            goto LABEL_66;
          if ( v47[0] != 1431522644 )
          {
            v13 = 1936549485;
            goto LABEL_10;
          }
          if ( *(u32 *)(a1 + 168) != 0 )
          {
            v13 = 1970628964;
            v14 = a3;
LABEL_73:
            v15 = 8;
            goto LABEL_11;
          }
          v40 = 0;
          if ( mp4_io_fn_13(a1 + 168, 1431522644, 184, 1431522644, v11, v12, a3) == 20481 )
            goto LABEL_12;
          v41 = *(unsigned int **)(a1 + 168);
          *v41 = v6;
          v41[1] = 1970628964;
          do
          {
            v42 = v40;
            v43 = v47[v40++];
            v41[v42 + 2] = v43;
          }
          while ( v40 < 4 );
          v44 = mp4_sample_fn_27(*(u32 *)(a1 + 168), v6 - 24, a3);
          goto LABEL_78;
        }
        if ( v8 != 1953653099 )
          goto LABEL_9;
        v35 = (unsigned int *)mp4_sample_fn_5(728);
        v36 = v35;
        if ( v35 == NULL )
        {
          v33 = a3;
          v34 = 1953653099;
          goto LABEL_40;
        }
        *v35 = v6;
        v35[1] = 1953653099;
        v49 = mp4_sample_fn_23(v35, v6 - 8, a3);
        if ( mp4_sample_fn_1((int)v36) != 0 )
          mp4_io_fn_12(1953653099, 32, a3);
        if ( mp4_box_fn_19(a3[21]) )
        {
          v37 = v36[169];
          goto LABEL_57;
        }
        if ( mp4_box_fn_21((u32 *)a3[21]) )
        {
          v37 = v36[169];
LABEL_57:
          if ( v37 != 0 )
            mp4_api_fn_57(v37, v36);
          v36[167] = (unsigned int)v36;
        }
        else
        {
          v36[167] = (unsigned int)v36;
        }
        if ( mp4_track_fn_68((u32 *)a1, v36) == 20481 )
          mp4_io_fn_12(1953653099, 0x20000000, a3);
        v18 = v49;
        if ( v49 != 0 )
          return v18;
        v17 = a3[1];
        goto LABEL_13;
      }
      if ( v8 != 1768907891 )
      {
        if ( v8 != 1836475768 )
        {
LABEL_9:
          v13 = 1836019574;
          v14 = a3;
          goto LABEL_10;
        }
        if ( mp4_io_fn_13(a1 + 136, 1836475768, 536, 1836476516, v11, v12, a3) != 20481 )
        {
          v24 = *(unsigned int **)(a1 + 136);
          *v24 = v6;
          v24[1] = 1836475768;
          v25 = mp4_track_fn_26((int)v24, v6 - 8, a3);
          v18 = 20481;
          if ( v25 == 20481 )
            return v18;
        }
        goto LABEL_12;
      }
      if ( v6 < 0xC )
      {
        v26 = 1768907891;
        goto LABEL_32;
      }
      mp4_box_fn_103((int)v48, 1, a3);
      v14 = a3;
      v13 = 1768907891;
      if ( v48[0] != 0 )
        goto LABEL_10;
      if ( mp4_io_fn_13(a1 + 132, 1768907891, 20, v28, v11, v12, a3) == 20481 )
        goto LABEL_12;
      v29 = *(u32 *)(a1 + 132);
      *(u32 *)v29 = v6;
      v30 = v48[0];
      *(u32 *)(v29 + 4) = 1768907891;
      *(u8 *)(v29 + 8) = v30;
      mp4_box_fn_103(*(u32 *)(a1 + 132) + 9, 3, a3);
      if ( v6 == 12 )
        goto LABEL_12;
      v31 = *(u32 *)(a1 + 132);
      *(u32 *)(v31 + 12) = mp4_sample_fn_5(v6 - 12);
      v32 = *(u32 *)(*(u32 *)(a1 + 132) + 12);
      if ( v32 == 0 )
      {
        v33 = a3;
        v34 = 1768907891;
LABEL_40:
        mp4_io_fn_12(v34, 0x40000000, v33);
        return 20481;
      }
      mp4_box_fn_103(v32, v6 - 12, a3);
      v17 = a3[1];
    }
LABEL_13:
    v18 = 20481;
    if ( (v17 & 0xE0000000) != 0 )
      return v18;
    v19 = mp4_box_fn_2(*a3);
    if ( v20 < v12 )
    {
      v21 = v8;
LABEL_26:
      mp4_io_fn_12(v21, 1, a3);
      mp4_box_fn_24(*a3, v23, v11, v12);
      v4 -= v6;
      continue;
    }
    if ( v12 == v20 )
    {
      if ( v19 < v11 )
      {
        v21 = v8;
        goto LABEL_26;
      }
      v4 -= v6;
    }
    else
    {
      v4 -= v6;
    }
  }
  if ( v4 == 0 )
    return 0;
LABEL_22:
  mp4_io_fn_12(1836019574, 1, a3);
  return 0;
}


int mp4_sample_fn_22(int a1, unsigned int a2, int a3, int *a4)
{
  unsigned int v5;
  int v7;
  unsigned int v9;
  int v10;
  int v11;
  unsigned int v12;
  int v13;
  int v14;
  u64 v15;
  int v16;
  int v17;
  u64 v18;
  int v19;
  unsigned int v20;
  int *v21;
  int v22;
  int v23;
  unsigned int v24;
  unsigned int *v25;
  int v26;
  unsigned int v27;
  int v28;
  int v29;
  int v30;
  u32 v31[4];
  char v32[4];
  int v33;

  v5 = a2;
  v33 = a1;
  if ( a2 >= 8 )
  {
    while ( 1 )
    {
      v9 = mp4_box_fn_99(a4);
      if ( v9 < 8 )
      {
        mp4_io_fn_12(1836019574, 1, a4);
        return 0;
      }
      v10 = mp4_box_fn_99(a4);
      v11 = v10;
      if ( v5 < v9 )
      {
        mp4_io_fn_12(v10, 1, a4);
        v9 = v5;
      }
      v12 = mp4_box_fn_2(*a4) + v9;
      v15 = ((u64)(v15) & 0xFFFFFFFF00000000ULL) | (u32)(v12 - 8);
      v15 = ((u64)(v15) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v14 + (v12 < v9) - 1 + (v12 - 8 < 0xFFFFFFF8)) << 32);
      if ( v11 != 1836476516 )
      {
        if ( v11 != 1970628964 )
          goto LABEL_9;
        v20 = 0;
        v21 = v31;
        v22 = 1431522644;
        v23 = 1;
        if ( v9 >= 0x18 )
        {
          do
          {
            ++v20;
            *v21++ = mp4_box_fn_99(a4);
          }
          while ( v20 < 4 );
          if ( v31[1] != 567431118 )
          {
            v16 = *a4;
            goto LABEL_10;
          }
          v13 = -1148688036;
          if ( v31[2] != -1148688036 )
          {
            v16 = *a4;
            goto LABEL_10;
          }
          if ( v31[3] != -87439552 )
          {
            v16 = *a4;
            goto LABEL_10;
          }
          if ( v31[0] == 860111956 )
            goto LABEL_9;
          if ( v31[0] != 1431522644 )
          {
            v16 = *a4;
LABEL_10:
            mp4_box_fn_24(v16, v13, v15, SHIDWORD(v15));
LABEL_11:
            v17 = a4[1];
            goto LABEL_12;
          }
          if ( (a3 & 8) != 0 )
          {
            if ( mp4_io_fn_13(v33 + 168, 1431522644, 184, 567431118, v15, HIDWORD(v15), a4) != 20481 )
            {
              v24 = 0;
              v25 = *(unsigned int **)(v33 + 168);
              *v25 = v9;
              v25[1] = 1970628964;
              do
              {
                v26 = v24;
                v27 = v31[v24++];
                v25[v26 + 2] = v27;
              }
              while ( v24 < 4 );
              a3 -= 8;
              v28 = mp4_sample_fn_27(*(u32 *)(v33 + 168), v9 - 24, a4);
              a4[1] &= 0xE000000B;
              v7 = 20481;
              if ( v28 != 0 )
                return v7;
            }
            goto LABEL_11;
          }
          mp4_box_fn_24(*a4, v33, v15, SHIDWORD(v15));
          if ( *(u32 *)(v33 + 168) == 0 )
          {
            v17 = a4[1];
            goto LABEL_12;
          }
          v22 = 1970628964;
          v23 = 8;
        }
        goto LABEL_34;
      }
      if ( (a3 & 4) == 0 )
        break;
      if ( v9 < 0xC )
        goto LABEL_45;
      mp4_box_fn_103((int)v32, 1, a4);
      if ( v32[0] != 0 )
      {
LABEL_9:
        v16 = *a4;
        goto LABEL_10;
      }
      if ( v9 < 0x6C )
      {
LABEL_45:
        v22 = 1836476516;
        v23 = 0x20000000;
LABEL_34:
        mp4_io_fn_12(v22, v23, a4);
        v17 = a4[1];
        goto LABEL_12;
      }
      v30 = v33;
      a3 -= 4;
      *(u8 *)(v33 + 20) = 0;
      *(u32 *)(v30 + 12) = v9;
      *(u32 *)(v30 + 16) = 1836476516;
      mp4_io_fn_11(v30 + 12, a4, v29);
      v17 = a4[1];
LABEL_12:
      v7 = 20481;
      if ( (v17 & 0xE0000000) != 0 )
        return v7;
      v18 = ((u64)(v18) & 0xFFFFFFFF00000000ULL) | (u32)(mp4_box_fn_2(*a4));
      if ( v18 < v15 )
      {
        mp4_io_fn_12(v11, 1, a4);
        mp4_box_fn_24(*a4, v19, v15, SHIDWORD(v15));
      }
      v5 -= v9;
      if ( a3 == 0 )
        return 0;
      v7 = 0;
      if ( v5 < 8 )
        return v7;
    }
    mp4_box_fn_24(*a4, v13, v12 - 8, SHIDWORD(v15));
    v22 = 1836476516;
    if ( *(u32 *)(v33 + 12) == 0 )
      goto LABEL_11;
    v23 = 8;
    goto LABEL_34;
  }
  return 0;
}


int mp4_sample_fn_23(int a1, unsigned int a2, int *a3)
{
  unsigned int v4;
  unsigned int v6;
  int v7;
  unsigned int v8;
  unsigned int v9;
  int v10;
  unsigned int v11;
  unsigned int v12;
  int v13;
  int *v14;
  int v15;
  int v16;
  int v17;
  int v18;
  unsigned int v19;
  unsigned int v20;
  unsigned int v21;
  int v23;
  int v24;
  unsigned int *v25;
  int v26;
  unsigned int v27;
  int *v28;
  unsigned int v29;
  unsigned int *v30;
  int v31;
  unsigned int v32;
  int v33;
  int v34;
  int v35;
  u32 *v36;
  u32 v37[4];
  char v38[16];

  v4 = a2;
  if ( a2 >= 8 )
  {
    while ( 1 )
    {
      v6 = mp4_box_fn_99(a3);
      if ( v6 < 8 )
        goto LABEL_23;
      v7 = mp4_box_fn_99(a3);
      v8 = v7;
      if ( v4 < v6 )
      {
        mp4_io_fn_12(v7, 1, a3);
        v6 = v4;
      }
      v9 = mp4_box_fn_2(*a3) + v6;
      v11 = v9 - 8;
      v12 = v10 + (v9 < v6) - 1 + (v9 - 8 < 0xFFFFFFF8);
      if ( v8 != 1953196132 )
        break;
      if ( v6 < 0xC )
        goto LABEL_69;
      mp4_box_fn_103((int)v38, 1, a3);
      v14 = a3;
      v13 = 1953196132;
      if ( v38[0] != 0 )
        goto LABEL_11;
      if ( v6 < 0x5C )
      {
LABEL_69:
        v33 = 1953196132;
        v34 = 0x20000000;
        goto LABEL_60;
      }
      v13 = 1953196132;
      if ( *(u32 *)(a1 + 12) != 0 )
        goto LABEL_34;
      *(u32 *)(a1 + 12) = v6;
      strcpy((char *)(a1 + 16), "dhkt");
      mp4_io_fn_10(a1 + 12, a3, a3, 1953169408);
      v17 = a3[1];
LABEL_14:
      v18 = 20481;
      if ( (v17 & 0xE0000000) != 0 )
        return v18;
      v19 = mp4_box_fn_2(*a3);
      if ( v20 < v12 )
      {
        v21 = v8;
LABEL_27:
        mp4_io_fn_12(v21, 1, a3);
        mp4_box_fn_24(*a3, v23, v11, v12);
        v4 -= v6;
        goto LABEL_19;
      }
      if ( v12 == v20 )
      {
        if ( v19 < v11 )
        {
          v21 = v8;
          goto LABEL_27;
        }
        v4 -= v6;
      }
      else
      {
        v4 -= v6;
      }
LABEL_19:
      if ( v4 < 8 )
        goto LABEL_20;
    }
    if ( v8 > 0x746B6864 )
    {
      switch ( v8 )
      {
        case 0x74726566u:
          if ( mp4_io_fn_13(a1 + 696, 1953654118, 16, 1953628160, v11, v12, a3) != 20481 )
          {
            v25 = *(unsigned int **)(a1 + 696);
            *v25 = v6;
            v25[1] = 1953654118;
            v26 = mp4_sample_fn_24(v25, v6 - 8, a3);
            v18 = 20481;
            if ( v26 == 20481 )
              return v18;
          }
          goto LABEL_13;
        case 0x75647461u:
          if ( mp4_io_fn_13(a1 + 692, 1969517665, 16, 1953196132, v11, v12, a3) == 20481 )
          {
            mp4_io_fn_12(1969517665, 0x40000000, a3);
            v24 = 20481;
          }
          else
          {
            v36 = *(u32 **)(a1 + 692);
            *v36 = 8;
            v36[1] = 1969517665;
            mp4_box_fn_24(*a3, v35, v11, v12);
            v24 = 0;
          }
          break;
        case 0x75756964u:
          v27 = 0;
          v28 = v37;
          if ( v6 < 0x18 )
          {
            v33 = 1431522644;
            v34 = 1;
LABEL_60:
            mp4_io_fn_12(v33, v34, a3);
            v17 = a3[1];
            goto LABEL_14;
          }
          do
          {
            ++v27;
            *v28++ = mp4_box_fn_99(a3);
          }
          while ( v27 < 4 );
          if ( v37[1] != 567431118 )
          {
LABEL_48:
            v13 = 1970628964;
            goto LABEL_10;
          }
          if ( v37[2] != -1148688036 )
          {
            v13 = 1970628964;
            goto LABEL_10;
          }
          if ( v37[3] != -87439552 )
          {
            v13 = 1970628964;
            goto LABEL_10;
          }
          if ( v37[0] == 860111956 )
            goto LABEL_48;
          v13 = 1970628964;
          if ( v37[0] != 1431522644 )
            goto LABEL_10;
          v29 = 0;
          if ( mp4_io_fn_13(a1 + 688, 1431522644, 184, -1148715008, v11, v12, a3) == 20481 )
            goto LABEL_13;
          v30 = *(unsigned int **)(a1 + 688);
          *v30 = v6;
          v30[1] = 1970628964;
          do
          {
            v31 = v29;
            v32 = v37[v29++];
            v30[v31 + 2] = v32;
          }
          while ( v29 < 4 );
          v24 = mp4_sample_fn_27(*(u32 *)(a1 + 688), v6 - 24, a3);
          v18 = 20481;
          goto LABEL_31;
        default:
LABEL_9:
          v13 = 1953653099;
LABEL_10:
          v14 = a3;
LABEL_11:
          v15 = 4;
LABEL_12:
          mp4_io_fn_12(v13, v15, v14);
          mp4_box_fn_24(*a3, v16, v11, v12);
LABEL_13:
          v17 = a3[1];
          goto LABEL_14;
      }
    }
    else
    {
      if ( v8 == 1701082227 )
      {
        if ( *(u32 *)(a1 + 124) != 0 )
        {
          v14 = a3;
          v13 = 1701082227;
LABEL_34:
          v15 = 8;
          goto LABEL_12;
        }
        *(u32 *)(a1 + 124) = v6;
        *(u32 *)(a1 + 128) = 1701082227;
        v24 = mp4_sample_fn_25(a1 + 124, v6 - 8, a3);
        v18 = 20481;
        goto LABEL_31;
      }
      if ( v8 != 1835297121 )
        goto LABEL_9;
      v14 = a3;
      v13 = 1835297121;
      if ( *(u32 *)(a1 + 152) != 0 )
        goto LABEL_34;
      *(u32 *)(a1 + 152) = v6;
      *(u32 *)(a1 + 156) = 1835297121;
      v24 = mp4_sample_fn_26(a1 + 152, v6 - 8, a3);
    }
    v18 = 20481;
LABEL_31:
    if ( v24 == 20481 )
      return v18;
    v17 = a3[1];
    goto LABEL_14;
  }
LABEL_20:
  if ( v4 == 0 )
    return 0;
LABEL_23:
  mp4_io_fn_12(1953653099, 1, a3);
  return 0;
}


int mp4_sample_fn_24(int a1, unsigned int a2, int *a3)
{
  unsigned int v4;
  unsigned int v6;
  int v7;
  int v8;
  unsigned int v9;
  int v10;
  int v11;
  unsigned int v12;
  unsigned int v13;
  int v14;
  int v15;
  int v16;
  unsigned int v17;
  unsigned int v18;
  int v19;
  int v21;
  unsigned int *v22;
  unsigned int v23;
  int v24;
  unsigned int v25;
  int v26;
  int v27;
  int v28;
  unsigned int *v29;
  unsigned int v30;
  int v31;
  unsigned int v32;
  int v33;
  int v34;

  v4 = a2;
  while ( v4 >= 8 )
  {
    v6 = mp4_box_fn_99(a3);
    if ( v6 < 8 )
      goto LABEL_20;
    v7 = mp4_box_fn_99(a3);
    v8 = v7;
    if ( v4 < v6 )
    {
      mp4_io_fn_12(v7, 1, a3);
      v6 = v4;
    }
    if ( (v6 & 3) != 0 )
    {
      mp4_io_fn_12(1953654118, 2, a3);
      v4 -= v6 - (v6 & 0xFFFFFFFC);
      v6 &= 0xFFFFFFFC;
    }
    v9 = mp4_box_fn_2(*a3) + v6;
    v12 = v9 - 8;
    v13 = v11 + (v9 < v6) - 1 + (v9 - 8 < 0xFFFFFFF8);
    if ( v8 == 1836085092 )
    {
      if ( mp4_io_fn_13(a1 + 12, 1836085092, 16, v10, v12, v13, a3) == 20481 )
        goto LABEL_10;
      v29 = *(unsigned int **)(a1 + 12);
      v30 = (u16)((v6 - 8) >> 2);
      *v29 = v6;
      v29[1] = 1836085092;
      v29[3] = mp4_sample_fn_5(4 * v30);
      v31 = *(u32 *)(a1 + 12);
      v32 = 0;
      if ( *(u32 *)(v31 + 12) == 0 )
      {
        v28 = 1836085092;
        goto LABEL_31;
      }
      *(u16 *)(v31 + 8) = v30;
      *(u16 *)(v31 + 10) = v30;
      if ( (u16)((v6 - 8) >> 2) == 0 )
        goto LABEL_10;
      do
      {
        v33 = mp4_box_fn_99(a3);
        v34 = 4 * v32++;
        *(u32 *)(v34 + *(u32 *)(*(u32 *)(a1 + 12) + 12)) = v33;
      }
      while ( v32 < v30 );
      v15 = a3[1];
    }
    else
    {
      if ( v8 != 1937337955 )
      {
        mp4_io_fn_12(1953654118, 4, a3);
        mp4_box_fn_24(*a3, v14, v12, v13);
LABEL_10:
        v15 = a3[1];
        goto LABEL_11;
      }
      if ( mp4_io_fn_13(a1 + 8, 1937337955, 16, v10, v12, v13, a3) == 20481 )
        goto LABEL_10;
      v22 = *(unsigned int **)(a1 + 8);
      v23 = (u16)((v6 - 8) >> 2);
      *v22 = v6;
      v22[1] = 1937337955;
      v22[3] = mp4_sample_fn_5(4 * v23);
      v24 = *(u32 *)(a1 + 8);
      v25 = 0;
      if ( *(u32 *)(v24 + 12) == 0 )
      {
        v28 = 1937337955;
LABEL_31:
        mp4_io_fn_12(v28, 0x40000000, a3);
        v15 = a3[1];
        goto LABEL_11;
      }
      *(u16 *)(v24 + 8) = v23;
      *(u16 *)(v24 + 10) = v23;
      if ( (u16)((v6 - 8) >> 2) == 0 )
        goto LABEL_10;
      do
      {
        v26 = mp4_box_fn_99(a3);
        v27 = 4 * v25++;
        *(u32 *)(v27 + *(u32 *)(*(u32 *)(a1 + 8) + 12)) = v26;
      }
      while ( v25 < v23 );
      v15 = a3[1];
    }
LABEL_11:
    v16 = 20481;
    if ( (v15 & 0xE0000000) != 0 )
      return v16;
    v17 = mp4_box_fn_2(*a3);
    if ( v18 < v13 )
    {
      v19 = v8;
LABEL_24:
      mp4_io_fn_12(v19, 1, a3);
      mp4_box_fn_24(*a3, v21, v12, v13);
      v4 -= v6;
      continue;
    }
    if ( v13 == v18 )
    {
      if ( v17 < v12 )
      {
        v19 = v8;
        goto LABEL_24;
      }
      v4 -= v6;
    }
    else
    {
      v4 -= v6;
    }
  }
  if ( v4 == 0 )
    return 0;
LABEL_20:
  mp4_io_fn_12(1953654118, 1, a3);
  return 0;
}


int mp4_sample_fn_25(int a1, unsigned int a2, int *a3)
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
  unsigned int v24;
  unsigned int v25;
  int v26;
  char v27[16];

  v4 = a2;
  if ( a2 >= 8 )
  {
    while ( 1 )
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
      if ( v10 != 1701606260 )
        break;
      if ( v8 >= 0xC )
      {
        mp4_box_fn_103((int)v27, 1, a3);
        v16 = a3;
        v15 = 1701606260;
        if ( v27[0] != 0 )
          goto LABEL_11;
        if ( v8 >= 0x10 )
        {
          if ( *(u32 *)(a1 + 8) == 0 )
          {
            *(u32 *)(a1 + 8) = v8;
            strcpy((char *)(a1 + 12), "tsle");
            mp4_box_fn_103(a1 + 17, 3, a3);
            v24 = mp4_box_fn_99(a3);
            v25 = (v8 - 16) / 0xC;
            *(u32 *)(a1 + 20) = v24;
            if ( v25 < v24 )
            {
              mp4_io_fn_12(1701082227, 2, a3);
              *(u32 *)(a1 + 20) = v25;
            }
            v26 = mp4_sample_fn_40(a1 + 24, *(u32 *)(a1 + 20), 3, a3);
            v6 = 20481;
            if ( v26 == 20481 )
              return v6;
            goto LABEL_13;
          }
          v16 = a3;
          v15 = 1701606260;
          v17 = 8;
LABEL_12:
          mp4_io_fn_12(v15, v17, v16);
          mp4_box_fn_24(*a3, v18, v13, v14);
LABEL_13:
          v19 = a3[1];
          goto LABEL_14;
        }
      }
      mp4_io_fn_12(1701606260, 0x20000000, a3);
      v19 = a3[1];
LABEL_14:
      v6 = 20481;
      if ( (v19 & 0xE0000000) != 0 )
        return v6;
      v20 = mp4_box_fn_2(*a3);
      if ( v21 < v14 )
      {
        v22 = v10;
LABEL_24:
        mp4_io_fn_12(v22, 1, a3);
        mp4_box_fn_24(*a3, v23, v13, v14);
        v4 -= v8;
        goto LABEL_19;
      }
      if ( v14 == v21 )
      {
        if ( v20 < v13 )
        {
          v22 = v10;
          goto LABEL_24;
        }
        v4 -= v8;
      }
      else
      {
        v4 -= v8;
      }
LABEL_19:
      if ( v4 < 8 )
        goto LABEL_2;
    }
    v15 = 1701082227;
    v16 = a3;
LABEL_11:
    v17 = 4;
    goto LABEL_12;
  }
LABEL_2:
  if ( v4 == 0 )
    return 0;
LABEL_5:
  mp4_io_fn_12(1701082227, 1, a3);
  return 0;
}


int mp4_sample_fn_26(int a1, unsigned int a2, int *a3)
{
  int v3;
  unsigned int v5;
  int v7;
  unsigned int v9;
  int v10;
  int v11;
  unsigned int v12;
  int v13;
  unsigned int v14;
  unsigned int v15;
  int v16;
  int *v17;
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
  u8 v28[16];

  v3 = 0;
  v5 = a2;
  if ( a2 >= 8 )
  {
    while ( 1 )
    {
      v9 = mp4_box_fn_99(a3);
      if ( v9 < 8 )
        goto LABEL_5;
      v10 = mp4_box_fn_99(a3);
      v11 = v10;
      if ( v5 < v9 )
      {
        mp4_io_fn_12(v10, 1, a3);
        v9 = v5;
      }
      v12 = mp4_box_fn_2(*a3) + v9;
      v14 = v12 - 8;
      v15 = v13 + (v12 < v9) - 1 + (v12 - 8 < 0xFFFFFFF8);
      if ( v11 == 1751411826 )
        break;
      if ( (unsigned int)v11 <= 0x68646C72 )
        goto LABEL_13;
      if ( v11 != 1835296868 )
      {
        if ( v11 == 1835626086 )
        {
          v17 = a3;
          v16 = 1835626086;
          if ( *(u32 *)(a1 + 152) == 0 )
          {
            *(u32 *)(a1 + 152) = v9;
            *(u32 *)(a1 + 156) = 1835626086;
            v25 = mp4_sample_fn_36(a1 + 152, v9 - 8, a3, v3);
            v7 = 20481;
            if ( v25 == 20481 )
              return v7;
            goto LABEL_16;
          }
          goto LABEL_31;
        }
LABEL_13:
        v16 = 1835297121;
        v17 = a3;
LABEL_14:
        v18 = 4;
LABEL_15:
        mp4_io_fn_12(v16, v18, v17);
        mp4_box_fn_24(*a3, v19, v14, v15);
LABEL_16:
        v20 = a3[1];
        goto LABEL_17;
      }
      if ( v9 < 0xC )
        goto LABEL_38;
      mp4_box_fn_103((int)v28, 1, a3);
      v17 = a3;
      v16 = 1835296868;
      if ( v28[0] != 0 )
        goto LABEL_14;
      if ( v9 < 0x20 )
      {
LABEL_38:
        v26 = 1835296868;
LABEL_39:
        mp4_io_fn_12(v26, 0x20000000, a3);
        v20 = a3[1];
        goto LABEL_17;
      }
      if ( *(u32 *)(a1 + 8) != 0 )
      {
        v17 = a3;
        v16 = 1835296868;
        goto LABEL_31;
      }
      *(u32 *)(a1 + 8) = v9;
      strcpy((char *)(a1 + 12), "dhdm");
      mp4_io_fn_9(a1 + 8, a3, a3);
      v20 = a3[1];
LABEL_17:
      v7 = 20481;
      if ( (v20 & 0xE0000000) != 0 )
        return v7;
      v21 = mp4_box_fn_2(*a3);
      if ( v22 < v15 )
      {
        v23 = v11;
LABEL_27:
        mp4_io_fn_12(v23, 1, a3);
        mp4_box_fn_24(*a3, v24, v14, v15);
        v5 -= v9;
        goto LABEL_22;
      }
      if ( v15 == v22 )
      {
        if ( v21 < v14 )
        {
          v23 = v11;
          goto LABEL_27;
        }
        v5 -= v9;
      }
      else
      {
        v5 -= v9;
      }
LABEL_22:
      if ( v5 < 8 )
        goto LABEL_2;
    }
    if ( v9 >= 0xC )
    {
      mp4_box_fn_103((int)v28, 1, a3);
      v17 = a3;
      v16 = 1751411826;
      if ( v28[0] != 0 )
        goto LABEL_14;
      if ( v9 >= 0x21 )
      {
        if ( *(u32 *)(a1 + 48) == 0 )
        {
          *(u32 *)(a1 + 48) = v9;
          strcpy((char *)(a1 + 52), "rldh");
          mp4_box_fn_103(a1 + 57, 3, a3);
          *(u32 *)(a1 + 60) = mp4_box_fn_99(a3);
          *(u32 *)(a1 + 64) = mp4_box_fn_99(a3);
          *(u32 *)(a1 + 68) = mp4_box_fn_99(a3);
          *(u32 *)(a1 + 72) = mp4_box_fn_99(a3);
          *(u32 *)(a1 + 76) = mp4_box_fn_99(a3);
          if ( v9 - 32 >= 0x41 )
          {
            *(u8 *)(a1 + 144) = mp4_sample_fn_35(a1 + 80, 64, a3);
            mp4_box_fn_24(*a3, v27, v14, v15);
            *(u32 *)(a1 + 48) = 96;
          }
          else
          {
            *(u8 *)(a1 + 144) = mp4_sample_fn_35(a1 + 80, v9 - 32, a3);
          }
          v3 = *(u32 *)(a1 + 64);
          goto LABEL_16;
        }
        v17 = a3;
        v16 = 1751411826;
LABEL_31:
        v18 = 8;
        goto LABEL_15;
      }
    }
    v26 = 1751411826;
    goto LABEL_39;
  }
LABEL_2:
  if ( v5 == 0 )
    return 0;
LABEL_5:
  mp4_io_fn_12(1835297121, 1, a3);
  return 0;
}


int mp4_sample_fn_27(int a1, unsigned int a2, int *a3)
{
  unsigned int v3;
  int v5;
  unsigned int v7;
  int v8;
  int v9;
  unsigned int v10;
  int v11;
  unsigned int v12;
  unsigned int v13;
  int v14;
  int *v15;
  int v16;
  int v17;
  int v18;
  unsigned int v19;
  unsigned int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  unsigned int v29;
  int v30;
  u16 v31;
  int v32;
  int v33;
  int *v34;
  int v35;
  unsigned int v36;
  int v37;
  int v38;
  unsigned int i;
  int v40; // dc
  unsigned int v41;
  u16 v42;
  int v43;
  u8 v44[32];
  int v45;
  int v46;
  unsigned int v47;
  unsigned int v48;
  unsigned int v49;
  int v50;
  int v51;
  int v52;
  int v53;
  int v54;
  int v55;
  int v56;

  v3 = a2;
  v46 = a1;
  while ( v3 >= 8 )
  {
    v7 = mp4_box_fn_99(a3);
    if ( v7 < 8 )
      goto LABEL_5;
    v8 = mp4_box_fn_99(a3);
    v9 = v8;
    if ( v3 < v7 )
    {
      mp4_io_fn_12(v8, 1, a3);
      v7 = v3;
    }
    v10 = mp4_box_fn_2(*a3) + v7;
    v12 = v10 - 8;
    v13 = v11 + (v10 < v7) - 1 + (v10 - 8 < 0xFFFFFFF8);
    if ( v9 == 1297368134 )
    {
      if ( v7 >= 0xA )
      {
        v15 = a3;
        v14 = 1297368134;
        if ( *(u32 *)(v46 + 24) != 0 )
          goto LABEL_31;
        v37 = v46;
        *(u32 *)(v46 + 24) = v7;
        v38 = v7 - 10;
        *(u32 *)(v37 + 28) = 1297368134;
        v49 = 0;
        v47 = mp4_box_fn_101(a3);
        if ( v47 == 0 )
          goto LABEL_15;
        for ( i = v49; ; i = v49 )
        {
          v49 = i + 1;
          v40 = v38 < 10;
          v38 -= 10;
          if ( v40 )
            break;
          v41 = mp4_box_fn_101(a3);
          v42 = v41;
          if ( v41 < 0xA )
          {
            v25 = 1297368134;
            v26 = 1;
            goto LABEL_34;
          }
          if ( v41 >= 0x2B )
          {
            v42 = 42;
            mp4_io_fn_12(1297368134, 1, a3);
          }
          v52 = mp4_box_fn_99(a3);
          v50 = mp4_box_fn_101(a3);
          v43 = (u16)(v42 - 10);
          v51 = mp4_box_fn_101(a3);
          if ( v38 < v43 )
            break;
          if ( v42 != 10 )
          {
            v38 -= v43;
            mp4_box_fn_103((int)v44, (u16)(v42 - 10), a3);
          }
          if ( mp4_sample_fn_2(v46 + 24, v42, v52, v50, v51, *(u16 *)(v46 + 32), (unsigned int)v44) == 1 )
          {
            v34 = a3;
            v35 = 1297368134;
LABEL_53:
            mp4_io_fn_12(v35, 0x40000000, v34);
            return 20481;
          }
          if ( v49 >= v47 )
          {
            v18 = a3[1];
            goto LABEL_16;
          }
        }
      }
      v25 = 1297368134;
    }
    else
    {
      if ( (unsigned int)v9 <= 0x4D544446 )
        goto LABEL_13;
      if ( v9 == 1297368148 )
      {
        if ( v7 >= 0xA )
        {
          v27 = v46;
          if ( *(u32 *)(v46 + 44) != 0 )
          {
            v15 = a3;
            v14 = 1297368148;
LABEL_31:
            v16 = 8;
LABEL_14:
            mp4_io_fn_12(v14, v16, v15);
            mp4_box_fn_24(*a3, v17, v12, v13);
LABEL_15:
            v18 = a3[1];
            goto LABEL_16;
          }
          v49 = 0;
          v28 = v7 - 10;
          *(u32 *)(v46 + 44) = v7;
          *(u32 *)(v27 + 48) = 1297368148;
          v48 = mp4_box_fn_101(a3);
          if ( v48 == 0 )
            goto LABEL_15;
          while ( 1 )
          {
            v56 = 0;
            if ( v28 < 10 )
              break;
            v29 = mp4_box_fn_101(a3);
            v55 = mp4_box_fn_99(a3);
            v53 = mp4_box_fn_101(a3);
            v54 = mp4_box_fn_101(a3);
            v30 = v28 - 10;
            if ( v29 < 0xA )
              break;
            v31 = v29 - 10;
            if ( v30 < (int)(v29 - 10) )
            {
              mp4_io_fn_12(1297368148, 1, a3);
              v29 = ((v29) & ~0xFFFFU) | ((u16)(v30 + 10));
              v31 = v30;
            }
            v32 = v31;
            v28 = v30 - v31;
            if ( v54 == 257 )
            {
              v36 = *(u16 *)(v46 + 52);
              v45 = 0;
              v5 = mp4_sample_fn_18(v46 + 44, v29, v55, v53, 257, v36, 0, &v45);
              if ( v5 != 0 )
                return v5;
              v5 = mp4_sample_fn_29(v45, v32, a3, 257);
              if ( v5 != 0 )
                return v5;
              *(u32 *)(v45 + 16) = v46 + 64;
            }
            else
            {
              v33 = v46;
              if ( v31 != 0 )
              {
                v56 = mp4_sample_fn_5(v31);
                if ( v56 == 0 )
                {
                  v34 = a3;
                  v35 = 1297368148;
                  goto LABEL_53;
                }
                mp4_box_fn_103(v56, v32, a3);
                v33 = v46;
              }
              if ( mp4_sample_fn_4(v33 + 44, v29, v55, v53, v54, *(u16 *)(v33 + 52), v56) != 0 )
              {
                mp4_io_fn_12(1297368134, 0x40000000, a3);
                mp4_track_fn_65(v56);
                return 20481;
              }
              mp4_track_fn_65(v56);
            }
            if ( ++v49 >= v48 )
            {
              v18 = a3[1];
              goto LABEL_16;
            }
          }
        }
        v25 = 1297368148;
      }
      else
      {
        if ( v9 != 1297371981 )
        {
LABEL_13:
          v14 = 1431522644;
          v15 = a3;
          v16 = 4;
          goto LABEL_14;
        }
        if ( v7 >= 8 )
        {
          v15 = a3;
          v14 = 1297371981;
          if ( *(u32 *)(v46 + 64) != 0 )
            goto LABEL_31;
          v23 = v46;
          *(u32 *)(v46 + 64) = v7;
          *(u32 *)(v23 + 68) = 1297371981;
          v24 = mp4_sample_fn_28(v23 + 64, v7 - 8, a3);
          v5 = 20481;
          if ( v24 != 0 )
            return v5;
          goto LABEL_15;
        }
        v25 = 1297371981;
      }
    }
    v26 = 0x20000000;
LABEL_34:
    mp4_io_fn_12(v25, v26, a3);
    v18 = a3[1];
LABEL_16:
    v5 = 20481;
    if ( (v18 & 0xE0000000) != 0 )
      return v5;
    v19 = mp4_box_fn_2(*a3);
    if ( v20 < v13 )
    {
      v21 = v9;
LABEL_26:
      mp4_io_fn_12(v21, 1, a3);
      mp4_box_fn_24(*a3, v22, v12, v13);
      v3 -= v7;
      continue;
    }
    if ( v13 == v20 )
    {
      if ( v19 < v12 )
      {
        v21 = v9;
        goto LABEL_26;
      }
      v3 -= v7;
    }
    else
    {
      v3 -= v7;
    }
  }
  if ( v3 == 0 )
    return 0;
LABEL_5:
  mp4_io_fn_12(1431522644, 1, a3);
  return 0;
}


int mp4_sample_fn_28(int a1, unsigned int a2, int *a3)
{
  unsigned int v4;
  int v6;
  int *v8;
  int v9;
  unsigned int v10;
  int v11;
  unsigned int v12;
  unsigned int v13;
  int v14;
  unsigned int v15;
  unsigned int v16;
  int v17;
  int *v18;
  int v19;
  int v20;
  int v21;
  unsigned int v22;
  unsigned int v23;
  unsigned int v24;
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
  unsigned int v36;
  unsigned int v37;
  int v38;
  u8 v39[4];
  unsigned int v40;

  v4 = a2;
  if ( a2 >= 8 )
  {
    while ( 1 )
    {
      v10 = mp4_box_fn_99(a3);
      v11 = mp4_box_fn_99(a3);
      v12 = v11;
      if ( v10 < 8 )
      {
        v8 = a3;
        v9 = 1297371981;
LABEL_6:
        mp4_io_fn_12(v9, 1, v8);
        return 0;
      }
      if ( v4 < v10 )
      {
        mp4_io_fn_12(v11, 1, a3);
        v10 = v4;
      }
      v13 = mp4_box_fn_2(*a3) + v10;
      v15 = v13 - 8;
      v16 = v14 + (v13 < v10) - 1 + (v13 - 8 < 0xFFFFFFF8);
      if ( v12 == 1297369156 )
        break;
      if ( v12 > 0x4D544844 )
      {
        if ( v12 == 1684631142 )
        {
          if ( *(u32 *)(a1 + 40) == 0 )
          {
            *(u32 *)(a1 + 40) = v10;
            *(u32 *)(a1 + 44) = 1684631142;
            v38 = mp4_sample_fn_37(a1 + 40, v10 - 8, a3);
            goto LABEL_49;
          }
          v18 = a3;
          v17 = 1684631142;
          goto LABEL_38;
        }
        if ( v12 != 1937011556 )
        {
LABEL_13:
          v17 = 1431522644;
          v18 = a3;
LABEL_14:
          v19 = 4;
          goto LABEL_15;
        }
        v27 = 1937011556;
        if ( v10 < 0x10 )
          goto LABEL_32;
        v18 = a3;
        v17 = 1937011556;
        if ( *(u32 *)(a1 + 72) != 0 )
          goto LABEL_38;
        mp4_box_fn_103((int)v39, 1, a3);
        v18 = a3;
        v17 = 1937011556;
        if ( v39[0] != 0 )
          goto LABEL_14;
        *(u32 *)(a1 + 72) = v10;
        strcpy((char *)(a1 + 76), "dsts");
        mp4_box_fn_103(a1 + 81, 3, a3);
        v36 = mp4_box_fn_99(a3);
        v37 = (unsigned int)(*(u32 *)(a1 + 72) - 16) >> 4;
        *(u32 *)(a1 + 84) = v36;
        if ( v37 < v36 )
        {
          *(u32 *)(a1 + 84) = v37;
          mp4_io_fn_12(1937011556, 2, a3);
        }
        if ( *(u32 *)(a1 + 84) != 0 )
        {
          v38 = mp4_sample_fn_42(a1 + 72, v10 - 16, a3, -2);
LABEL_49:
          v6 = 20481;
          if ( v38 != 0 )
            return v6;
LABEL_16:
          v21 = a3[1];
          goto LABEL_17;
        }
        v21 = a3[1];
      }
      else
      {
        if ( v12 != 1296323412 )
          goto LABEL_13;
        if ( v10 < 0x10 )
          goto LABEL_31;
        v18 = a3;
        v17 = 1296323412;
        if ( *(u32 *)(a1 + 100) != 0 )
          goto LABEL_38;
        *(u32 *)(a1 + 100) = v10;
        *(u32 *)(a1 + 104) = 1296323412;
        mp4_box_fn_103(a1 + 108, 1, a3);
        mp4_box_fn_103(a1 + 109, 3, a3);
        v26 = mp4_box_fn_99(a3);
        *(u32 *)(a1 + 112) = v26;
        if ( v10 < 20 * v26 )
        {
LABEL_31:
          v27 = 1296323412;
LABEL_32:
          mp4_io_fn_12(v27, 0x20000000, a3);
          v21 = a3[1];
          goto LABEL_17;
        }
        v28 = mp4_sample_fn_5(20 * v26);
        *(u32 *)(a1 + 116) = v28;
        v6 = 1;
        if ( v28 == 0 )
          return v6;
        v29 = *(u32 *)(a1 + 112);
        v40 = 0;
        if ( v29 == 0 )
          goto LABEL_16;
        v30 = 0;
        do
        {
          v31 = *(u32 *)(a1 + 116);
          ++v40;
          *(u32 *)(v30 + v31) = mp4_box_fn_99(a3);
          v32 = v30 + *(u32 *)(a1 + 116);
          *(u32 *)(v32 + 4) = mp4_box_fn_99(a3);
          v33 = v30 + *(u32 *)(a1 + 116);
          *(u32 *)(v33 + 8) = mp4_box_fn_99(a3);
          v34 = v30 + *(u32 *)(a1 + 116);
          *(u32 *)(v34 + 12) = mp4_box_fn_99(a3);
          v35 = v30 + *(u32 *)(a1 + 116);
          *(u32 *)(v35 + 16) = mp4_box_fn_99(a3);
          v30 += 20;
        }
        while ( v40 < *(u32 *)(a1 + 112) );
        v21 = a3[1];
      }
LABEL_17:
      v6 = 20481;
      if ( (v21 & 0xE0000000) != 0 )
        return v6;
      v22 = mp4_box_fn_2(*a3);
      if ( v23 < v16 )
      {
        v24 = v12;
LABEL_27:
        mp4_io_fn_12(v24, 1, a3);
        mp4_box_fn_24(*a3, v25, v15, v16);
        v4 -= v10;
        goto LABEL_22;
      }
      if ( v16 == v23 )
      {
        if ( v22 < v15 )
        {
          v24 = v12;
          goto LABEL_27;
        }
        v4 -= v10;
      }
      else
      {
        v4 -= v10;
      }
LABEL_22:
      if ( v4 < 8 )
        goto LABEL_2;
    }
    v27 = 1297369156;
    if ( v10 >= 0x20 )
    {
      if ( *(u32 *)(a1 + 8) == 0 )
      {
        *(u32 *)(a1 + 8) = v10;
        *(u32 *)(a1 + 12) = 1297369156;
        mp4_box_fn_103(a1 + 16, 1, a3);
        mp4_box_fn_103(a1 + 17, 3, a3);
        *(u32 *)(a1 + 20) = mp4_box_fn_99(a3);
        *(u32 *)(a1 + 24) = mp4_box_fn_99(a3);
        *(u32 *)(a1 + 28) = mp4_box_fn_99(a3);
        *(u32 *)(a1 + 32) = mp4_box_fn_99(a3);
        *(u32 *)(a1 + 36) = mp4_box_fn_99(a3);
        goto LABEL_16;
      }
      v18 = a3;
      v17 = 1297369156;
LABEL_38:
      v19 = 8;
LABEL_15:
      mp4_io_fn_12(v17, v19, v18);
      mp4_box_fn_24(*a3, v20, v15, v16);
      goto LABEL_16;
    }
    goto LABEL_32;
  }
LABEL_2:
  if ( v4 != 0 )
  {
    v8 = a3;
    v9 = 1431522644;
    goto LABEL_6;
  }
  return 0;
}


int mp4_sample_fn_29(int *a1, unsigned int a2, int *a3)
{
  int v3;
  int v7;
  int v8;
  int v9;
  unsigned int v10;
  int *v11;

  v3 = a2 < 0xC;
  v7 = 8195;
  if ( !v3 )
  {
    *a1 = mp4_box_fn_99(a3);
    a1[1] = mp4_box_fn_99(a3);
    v8 = mp4_box_fn_99(a3);
    a1[2] = v8;
    v7 = 8195;
    if ( a2 - 12 >= 12 * v8 )
    {
      v9 = mp4_sample_fn_5(12 * v8);
      a1[3] = v9;
      v7 = 1;
      if ( v9 != 0 )
      {
        v10 = 0;
        if ( a1[2] != 0 )
        {
          v11 = (int *)v9;
          do
          {
            *v11 = mp4_box_fn_99(a3);
            ++v10;
            v11[1] = mp4_box_fn_99(a3);
            v11[2] = mp4_box_fn_99(a3);
            v11 += 3;
          }
          while ( v10 < a1[2] );
        }
        return 0;
      }
    }
  }
  return v7;
}


int mp4_sample_fn_30(int a1, u64 *a2, unsigned int *a3, int *a4)
{
  unsigned int v7;
  unsigned int v9;
  s16 v10;
  int *v11;
  unsigned int v12;
  int v14;
  unsigned int v15;
  int v16;
  unsigned int v17;

  v7 = *a3;
  if ( (*a3 & 3) != 0 )
  {
    mp4_io_fn_12(1718909296, 1, a4);
    v17 = ((*a3 - 16) & 0xFFFFFFFC) + 16;
    *a2 -= *a3 - v17;
    v7 = v17;
    *a3 = v17;
  }
  *(u32 *)a1 = v7;
  *(u32 *)(a1 + 4) = 1718909296;
  *(u32 *)(a1 + 8) = mp4_box_fn_99(a4);
  *(u32 *)(a1 + 12) = mp4_box_fn_99(a4);
  v9 = (*a3 - 16) >> 2;
  *(u16 *)(a1 + 16) = v9;
  *(u16 *)(a1 + 18) = (u16)v9 / 5u;
  if ( (u16)v9 % 5u != 0 )
    *(u16 *)(a1 + 18) = (u16)v9 / 5u + 1;
  v10 = *(u16 *)(a1 + 18);
  *(u16 *)(a1 + 18) = 5 * v10;
  if ( 5 * v10 == 0 || (v16 = mp4_box_fn_7(4 * (u16)(5 * v10)), *(u32 *)(a1 + 20) = v16, v16 != 0) )
  {
    v11 = *(int **)(a1 + 20);
    v12 = 0;
    if ( *(u16 *)(a1 + 16) != 0 )
    {
      do
      {
        v14 = mp4_box_fn_99(a4);
        v15 = *(u16 *)(a1 + 16);
        ++v12;
        *v11++ = v14;
      }
      while ( v12 < v15 );
      return 0;
    }
    else
    {
      return 0;
    }
  }
  else
  {
    mp4_io_fn_12(1718909296, 0x40000000, a4);
    return 20481;
  }
}


int mp4_sample_fn_31(u32 *a1, unsigned int a2, int *a3)
{
  unsigned int v4;
  int v6;
  unsigned int v8;
  int *v9;
  int v10;
  unsigned int v11;
  int v12;
  unsigned int v13;
  int v14;
  unsigned int v15;
  unsigned int v16;
  int v17;
  int *v18;
  int v19;
  int v20;
  int v21;
  unsigned int v22;
  unsigned int v23;
  int v24;
  int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  int v32;
  char v33;
  int v34;
  int v35;
  int v36;
  char v37;
  int v38;
  int v39;
  int v40;
  char v41;
  u8 v42[16];

  v4 = 0;
  v6 = 1347571526;
  v8 = *a1 - 32;
  if ( a2 != 0 )
  {
    while ( 1 )
    {
      v9 = a3;
      v10 = 1347571526;
      if ( v8 < 8 )
        goto LABEL_22;
      v11 = mp4_box_fn_99(a3);
      v12 = mp4_box_fn_99(a3);
      v6 = v12;
      if ( v8 < v11 )
      {
        mp4_io_fn_12(v12, 1, a3);
        v11 = v8;
      }
      v13 = mp4_box_fn_2(*a3) + v11;
      v15 = v13 - 8;
      v16 = v14 + (v13 < v11) - 1 + (v13 - 8 < 0xFFFFFFF8);
      if ( v6 == 1095782982 )
        break;
      if ( (unsigned int)v6 <= 0x41505246 )
        goto LABEL_9;
      if ( v6 != 1179669062 )
      {
        if ( v6 != 1448104518 )
        {
LABEL_9:
          v17 = 1936549485;
          v18 = a3;
          goto LABEL_10;
        }
        if ( v11 < 0xC )
        {
          v27 = 1448083456;
          goto LABEL_38;
        }
        mp4_box_fn_103((int)v42, 1, a3);
        v28 = 1448083456;
        if ( v42[0] != 0 )
          goto LABEL_41;
        v29 = 1448104518;
        if ( v11 >= 0x34 )
        {
          v30 = a1[9];
          if ( v30 != 0 )
          {
            if ( mp4_sample_fn_7(a1 + 12, 52 * v30, 52 * v30 + 52) != 0 )
            {
              v34 = 1448083456;
LABEL_37:
              mp4_io_fn_12(v34 | 0x5246, 0x40000000, a3);
              return 20481;
            }
            v32 = a1[12] + 52 * a1[9];
          }
          else
          {
            v31 = mp4_sample_fn_5(52);
            a1[12] = v31;
            v21 = 1;
            v32 = v31;
            if ( v31 == 0 )
              return v21;
          }
          *(u32 *)v32 = v11;
          v33 = v42[0];
          *(u32 *)(v32 + 4) = 1448104518;
          *(u8 *)(v32 + 8) = v33;
          mp4_sample_fn_34(v32, a3);
          ++a1[9];
          goto LABEL_11;
        }
        goto LABEL_39;
      }
      if ( v11 < 0xC )
      {
        v27 = 1179648000;
        goto LABEL_38;
      }
      mp4_box_fn_103((int)v42, 1, a3);
      v28 = 1179648000;
      if ( v42[0] != 0 )
        goto LABEL_41;
      if ( v11 < 0x14 )
      {
        v27 = 1179648000;
        goto LABEL_38;
      }
      v29 = 1179669062;
      v35 = 2;
      if ( a1[7] == 0 )
      {
        v36 = mp4_sample_fn_5(20);
        a1[10] = v36;
        v34 = 1179648000;
        if ( v36 == 0 )
          goto LABEL_37;
        *(u32 *)v36 = v11;
        v37 = v42[0];
        *(u32 *)(v36 + 4) = 1179669062;
        *(u8 *)(v36 + 8) = v37;
        mp4_sample_fn_32(v36, a3);
        ++a1[7];
        goto LABEL_11;
      }
LABEL_40:
      mp4_io_fn_12(v29, v35, a3);
      v20 = a3[1];
LABEL_12:
      v21 = 20481;
      if ( (v20 & 0xE0000000) != 0 )
        return v21;
      v22 = mp4_box_fn_2(*a3);
      if ( v23 < v16 )
      {
        v24 = v6;
LABEL_26:
        mp4_io_fn_12(v24, 1, a3);
        mp4_box_fn_24(*a3, v26, v15, v16);
        ++v4;
        goto LABEL_17;
      }
      if ( v16 == v23 )
      {
        if ( v22 < v15 )
        {
          v24 = v6;
          goto LABEL_26;
        }
        ++v4;
      }
      else
      {
        ++v4;
      }
LABEL_17:
      v8 -= v11;
      if ( v4 >= a2 )
        goto LABEL_18;
    }
    v27 = 1095761920;
    if ( v11 >= 0xC )
    {
      mp4_box_fn_103((int)v42, 1, a3);
      if ( v42[0] != 0 )
      {
        v28 = 1095761920;
LABEL_41:
        v18 = a3;
        v17 = v28 | 0x5246;
LABEL_10:
        mp4_io_fn_12(v17, 4, v18);
        mp4_box_fn_24(*a3, v19, v15, v16);
LABEL_11:
        v20 = a3[1];
        goto LABEL_12;
      }
      v27 = 1095761920;
      if ( v11 >= 0x2C )
      {
        v38 = a1[8];
        if ( v38 != 0 )
        {
          if ( mp4_sample_fn_7(a1 + 11, 44 * v38, 44 * v38 + 44) != 0 )
          {
LABEL_56:
            v34 = 1095761920;
            goto LABEL_37;
          }
          v40 = a1[11] + 44 * a1[8];
        }
        else
        {
          v39 = mp4_sample_fn_5(44);
          a1[11] = v39;
          v40 = v39;
          if ( v39 == 0 )
            goto LABEL_56;
        }
        *(u32 *)v40 = v11;
        v41 = v42[0];
        *(u32 *)(v40 + 4) = 1095782982;
        *(u8 *)(v40 + 8) = v41;
        mp4_sample_fn_33(v40, a3);
        ++a1[8];
        goto LABEL_11;
      }
    }
LABEL_38:
    v29 = v27 | 0x5246;
LABEL_39:
    v35 = 0x20000000;
    goto LABEL_40;
  }
LABEL_18:
  if ( v8 == 0 )
    return 0;
  v10 = v6;
  v9 = a3;
LABEL_22:
  mp4_io_fn_12(v10, 1, v9);
  return 0;
}


int mp4_sample_fn_32(int a1, int *a2)
{
  int result;

  mp4_box_fn_103(a1 + 9, 3, a2);
  *(u32 *)(a1 + 12) = mp4_box_fn_99(a2);
  result = mp4_box_fn_99(a2);
  *(u32 *)(a1 + 16) = result;
  return result;
}


int mp4_sample_fn_33(u32 *a1, int *a2)
{
  int result;

  mp4_box_fn_103((int)a1 + 9, 3, a2);
  a1[3] = mp4_box_fn_99(a2);
  a1[4] = mp4_box_fn_99(a2);
  a1[5] = mp4_box_fn_99(a2);
  a1[6] = mp4_box_fn_99(a2);
  a1[7] = mp4_box_fn_99(a2);
  a1[8] = mp4_box_fn_99(a2);
  a1[9] = mp4_box_fn_99(a2);
  result = mp4_box_fn_99(a2);
  a1[10] = result;
  return result;
}


int mp4_sample_fn_34(u32 *a1, int *a2)
{
  int result;

  mp4_box_fn_103((int)a1 + 9, 3, a2);
  a1[3] = mp4_box_fn_99(a2);
  a1[4] = mp4_box_fn_99(a2);
  a1[5] = mp4_box_fn_99(a2);
  a1[6] = mp4_box_fn_99(a2);
  a1[7] = mp4_box_fn_99(a2);
  a1[8] = mp4_box_fn_99(a2);
  a1[9] = mp4_box_fn_99(a2);
  a1[10] = mp4_box_fn_99(a2);
  a1[11] = mp4_box_fn_99(a2);
  result = mp4_box_fn_99(a2);
  a1[12] = result;
  return result;
}


int mp4_sample_fn_35(u8 *a1, unsigned int a2, int *a3)
{
  int v3;
  u8 *v4;

  v3 = 1;
  v4 = &a1[a2];
  if ( a2 != 0 )
  {
    mp4_box_fn_103((int)a1, a2, a3);
    v3 = 1;
    if ( *(v4 - 1) != 0 )
    {
      mp4_box_fn_23((unsigned int)a1, a2, (unsigned int)(a1 + 1), a2 - 1);
      *(v4 - 1) = 0;
      return 0;
    }
  }
  else
  {
    *a1 = 0;
  }
  return v3;
}


int mp4_sample_fn_36(int a1, unsigned int a2, int *a3, int a4)
{
  unsigned int v6;
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
  int v20;
  unsigned int v21;
  unsigned int v22;
  int v23;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  u8 v34[16];

  v6 = a2;
  while ( v6 >= 8 )
  {
    v8 = mp4_box_fn_99(a3);
    if ( v8 < 8 )
      goto LABEL_20;
    v9 = mp4_box_fn_99(a3);
    v10 = v9;
    if ( v6 < v8 )
    {
      mp4_io_fn_12(v9, 1, a3);
      v8 = v6;
    }
    v11 = mp4_box_fn_2(*a3) + v8;
    v13 = v11 - 8;
    v14 = v12 + (v11 < v8) - 1 + (v11 - 8 < 0xFFFFFFF8);
    if ( v10 == 1852663908 )
    {
      if ( v8 < 0xC )
      {
        v27 = 1852637184;
        goto LABEL_43;
      }
      mp4_box_fn_103((int)v34, 1, a3);
      v28 = 1852637184;
      if ( v34[0] != 0 )
      {
LABEL_45:
        v16 = a3;
        v15 = v28 | 0x6864;
LABEL_9:
        v17 = 4;
LABEL_10:
        mp4_io_fn_12(v15, v17, v16);
        mp4_box_fn_24(*a3, v18, v13, v14);
        v19 = a3[1];
        goto LABEL_11;
      }
      v16 = a3;
      if ( *(u32 *)(a1 + 44) != 0 )
        goto LABEL_42;
      *(u32 *)(a1 + 44) = v8;
      strcpy((char *)(a1 + 48), "dhmn");
      mp4_io_fn_8(a1 + 44, a3, a3);
      v19 = a3[1];
    }
    else
    {
      if ( (unsigned int)v10 <= 0x6E6D6864 )
      {
        if ( v10 != 1684631142 )
          goto LABEL_8;
        v16 = a3;
        v15 = 1684631142;
        if ( *(u32 *)(a1 + 56) != 0 )
          goto LABEL_30;
        *(u32 *)(a1 + 56) = v8;
        *(u32 *)(a1 + 60) = 1684631142;
        v26 = mp4_sample_fn_37(a1 + 56, v8 - 8, a3);
        v20 = 20481;
        goto LABEL_27;
      }
      if ( v10 == 1937007212 )
      {
        v30 = mp4_box_fn_19(a3[21]);
        v31 = a1 + 372;
        if ( v30 || (v32 = mp4_box_fn_21((u32 *)a3[21]), v31 = a1 + 372, v32) )
        {
          v33 = mp4_io_fn_15(v31, a3);
          v20 = 20481;
          if ( v33 != 0 )
            return v20;
        }
        v15 = 1937007212;
        v16 = a3;
        if ( *(u32 *)(a1 + 104) != 0 )
          goto LABEL_30;
        *(u32 *)(a1 + 104) = v8;
        *(u32 *)(a1 + 108) = 1937007212;
        v26 = mp4_sample_fn_39(a1 + 88, v8 - 8, a3, a4);
        v20 = 20481;
LABEL_27:
        if ( v26 == 20481 )
          return v20;
        v19 = a3[1];
        goto LABEL_11;
      }
      if ( (unsigned int)v10 > 0x7374626C )
      {
        if ( v10 != 1986881636 )
        {
LABEL_8:
          v15 = 1835626086;
          v16 = a3;
          goto LABEL_9;
        }
        if ( v8 < 0xC )
        {
          v27 = 1986854912;
LABEL_43:
          v29 = v27 | 0x6864;
          goto LABEL_44;
        }
        mp4_box_fn_103((int)v34, 1, a3);
        v28 = 1986854912;
        if ( v34[0] != 0 )
          goto LABEL_45;
        if ( v8 < 0x14 )
        {
          v29 = 1986881636;
          goto LABEL_44;
        }
        v16 = a3;
        if ( *(u32 *)(a1 + 8) != 0 )
          goto LABEL_42;
        *(u32 *)(a1 + 8) = v8;
        strcpy((char *)(a1 + 12), "dhmv");
        mp4_io_fn_6(a1 + 8, a3, a3);
        v19 = a3[1];
      }
      else
      {
        if ( v10 != 1936549988 )
          goto LABEL_8;
        if ( v8 < 0xC )
        {
          v27 = 1936523264;
          goto LABEL_43;
        }
        mp4_box_fn_103((int)v34, 1, a3);
        v28 = 1936523264;
        if ( v34[0] != 0 )
          goto LABEL_45;
        if ( v8 < 0x10 )
        {
          v29 = 1936549988;
LABEL_44:
          mp4_io_fn_12(v29, 0x20000000, a3);
          v19 = a3[1];
          goto LABEL_11;
        }
        if ( *(u32 *)(a1 + 28) != 0 )
        {
          v28 = 1936523264;
          v16 = a3;
LABEL_42:
          v15 = v28 | 0x6864;
LABEL_30:
          v17 = 8;
          goto LABEL_10;
        }
        *(u32 *)(a1 + 28) = v8;
        strcpy((char *)(a1 + 32), "dhms");
        mp4_io_fn_7(a1 + 28, a3);
        v19 = a3[1];
      }
    }
LABEL_11:
    v20 = 20481;
    if ( (v19 & 0xE0000000) != 0 )
      return v20;
    v21 = mp4_box_fn_2(*a3);
    if ( v22 < v14 )
    {
      v23 = v10;
LABEL_24:
      mp4_io_fn_12(v23, 1, a3);
      mp4_box_fn_24(*a3, v25, v13, v14);
      v6 -= v8;
      continue;
    }
    if ( v14 == v22 )
    {
      if ( v21 < v13 )
      {
        v23 = v10;
        goto LABEL_24;
      }
      v6 -= v8;
    }
    else
    {
      v6 -= v8;
    }
  }
  if ( v6 == 0 )
    return 0;
LABEL_20:
  mp4_io_fn_12(1835626086, 1, a3);
  return 0;
}


int mp4_sample_fn_37(int a1, unsigned int a2, int *a3)
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
  char v26[16];

  v4 = a2;
  if ( a2 >= 8 )
  {
    while ( 1 )
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
      if ( v10 != 1685218662 )
        break;
      if ( v8 >= 0xC )
      {
        mp4_box_fn_103((int)v26, 1, a3);
        v16 = a3;
        v15 = 1685218662;
        if ( v26[0] != 0 )
          goto LABEL_11;
        if ( v8 >= 0x10 )
        {
          if ( *(u32 *)(a1 + 8) == 0 )
          {
            *(u32 *)(a1 + 8) = v8;
            strcpy((char *)(a1 + 12), "ferd");
            mp4_box_fn_103(a1 + 17, 3, a3);
            v24 = mp4_box_fn_99(a3);
            *(u32 *)(a1 + 20) = v24;
            if ( v24 != 0 )
            {
              v25 = mp4_sample_fn_38(a1 + 8, v8 - 16, a3);
              v6 = 20481;
              if ( v25 == 20481 )
                return v6;
            }
            goto LABEL_13;
          }
          v16 = a3;
          v15 = 1685218662;
          v17 = 8;
LABEL_12:
          mp4_io_fn_12(v15, v17, v16);
          mp4_box_fn_24(*a3, v18, v13, v14);
LABEL_13:
          v19 = a3[1];
          goto LABEL_14;
        }
      }
      mp4_io_fn_12(1685218662, 0x20000000, a3);
      v19 = a3[1];
LABEL_14:
      v6 = 20481;
      if ( (v19 & 0xE0000000) != 0 )
        return v6;
      v20 = mp4_box_fn_2(*a3);
      if ( v21 < v14 )
      {
        v22 = v10;
LABEL_24:
        mp4_io_fn_12(v22, 1, a3);
        mp4_box_fn_24(*a3, v23, v13, v14);
        v4 -= v8;
        goto LABEL_19;
      }
      if ( v14 == v21 )
      {
        if ( v20 < v13 )
        {
          v22 = v10;
          goto LABEL_24;
        }
        v4 -= v8;
      }
      else
      {
        v4 -= v8;
      }
LABEL_19:
      if ( v4 < 8 )
        goto LABEL_2;
    }
    v15 = 1684631142;
    v16 = a3;
LABEL_11:
    v17 = 4;
    goto LABEL_12;
  }
LABEL_2:
  if ( v4 == 0 )
    return 0;
LABEL_5:
  mp4_io_fn_12(1684631142, 1, a3);
  return 0;
}


int mp4_sample_fn_38(u32 *a1, unsigned int a2, int *a3)
{
  unsigned int v3;
  unsigned int v6;
  int i;
  unsigned int v9;
  int v10;
  int v11;
  unsigned int v12;
  int v13;
  unsigned int v14;
  unsigned int v15;
  int v16;
  int *v17;
  int v18;
  int v19;
  int v20;
  unsigned int v21;
  unsigned int v22;
  int *v24;
  int v25;
  int v26;
  int v27;
  int v28;
  char v29;
  s64 v30;
  unsigned int v31;
  int *v32;
  int v33;
  char v34[4];
  int *v35;

  v3 = 0;
  v6 = (*a1 - 16) / 0xCu;
  if ( v6 < a1[3] )
  {
    a1[3] = v6;
    mp4_io_fn_12(1685218662, 2, a3);
  }
  v35 = a1 + 4;
  if ( a2 >= 8 )
  {
    for ( i = a1[3]; ; i = a1[3] )
    {
      if ( v3 == i )
      {
        mp4_io_fn_12(1685218662, 2, a3);
        break;
      }
      v9 = mp4_box_fn_99(a3);
      if ( v9 < 8 )
      {
        v24 = a3;
        v25 = 1;
        goto LABEL_25;
      }
      v10 = mp4_box_fn_99(a3);
      v11 = v10;
      if ( a2 < v9 )
      {
        mp4_io_fn_12(v10, 1, a3);
        v9 = a2;
      }
      v12 = mp4_box_fn_2(*a3) + v9;
      v14 = v12 - 8;
      v15 = v13 + (v12 < v9) - 1 + (v12 - 8 < 0xFFFFFFF8);
      if ( v11 == 1970433056 )
      {
        if ( v9 < 0xC )
        {
          mp4_io_fn_12(1970433056, 0x20000000, a3);
          v19 = a3[1];
          goto LABEL_13;
        }
        mp4_box_fn_103((int)v34, 1, a3);
        v17 = a3;
        v16 = 1970433056;
        if ( v34[0] == 0 )
        {
          v27 = mp4_sample_fn_5(24);
          v28 = v27;
          if ( v27 == 0 )
            goto LABEL_41;
          *(u32 *)v27 = v9;
          v29 = v34[0];
          *(u32 *)(v27 + 4) = 1970433056;
          *(u8 *)(v27 + 8) = v29;
          mp4_box_fn_103(v27 + 9, 3, a3);
          v30 = ((u64)(v30) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(*(u8 *)(v28 + 11)) << 32);
          v30 = ((u64)(v30) & 0xFFFFFFFF00000000ULL) | (u32)((*(u8 *)(v28 + 9) << 16) | (*(u8 *)(v28 + 10) << 8));
          v31 = v3 + 1;
          if ( v30 == 0 )
          {
            if ( v9 == 12 )
            {
              v32 = v35;
LABEL_39:
              v35 = (int *)(v28 + 20);
              *v32 = v28;
              v3 = v31;
              *(u16 *)(v28 + 16) = v31;
              goto LABEL_12;
            }
            v33 = mp4_sample_fn_5(v9 - 11);
            *(u32 *)(v28 + 12) = v33;
            if ( v33 == 0 )
            {
              mp4_track_fn_65(v28);
LABEL_41:
              mp4_io_fn_12(1970433056, 0x40000000, a3);
              return 20481;
            }
            mp4_box_fn_103(v33, v9 - 12, a3);
            *(u8 *)(*(u32 *)(v28 + 12) + v9 - 12) = 0;
            v31 = v3 + 1;
          }
          v32 = v35;
          goto LABEL_39;
        }
      }
      else
      {
        v16 = 1685218662;
        v17 = a3;
      }
      mp4_io_fn_12(v16, 4, v17);
      mp4_box_fn_24(*a3, v18, v14, v15);
LABEL_12:
      v19 = a3[1];
LABEL_13:
      v20 = 20481;
      if ( (v19 & 0xE0000000) != 0 )
        return v20;
      v21 = mp4_box_fn_2(*a3);
      if ( v22 < v15 )
        goto LABEL_28;
      if ( v15 != v22 )
      {
        a2 -= v9;
        goto LABEL_17;
      }
      if ( v21 < v14 )
      {
LABEL_28:
        mp4_io_fn_12(v11, 1, a3);
        mp4_box_fn_24(*a3, v26, v14, v15);
        a2 -= v9;
      }
      else
      {
        a2 -= v9;
      }
LABEL_17:
      if ( a2 < 8 )
        break;
    }
  }
  if ( a2 != 0 )
    mp4_io_fn_12(1685218662, 1, a3);
  if ( v3 >= a1[3] )
    return 0;
  a1[3] = v3;
  v24 = a3;
  v25 = 2;
LABEL_25:
  mp4_io_fn_12(1685218662, v25, v24);
  return 0;
}


int mp4_sample_fn_39(int a1, unsigned int a2, int *a3, int a4)
{
  int v4;
  unsigned int v6;
  unsigned int v8;
  int v9;
  unsigned int v10;
  s64 v11;
  s64 v12;
  unsigned int v13;
  unsigned int v14;
  int v15;
  int v16;
  int *v17;
  int v18;
  int v19;
  int v20;
  int v21;
  unsigned int v22;
  unsigned int v23;
  unsigned int v24;
  int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  unsigned int v31;
  char v32;
  unsigned int v33;
  int v34;
  int v35;
  unsigned int v36;
  int v37;
  unsigned int v38;
  unsigned int v39;
  int v40;
  u8 v41[4];
  int v42;
  unsigned int v43;
  u32 v44[2];
  unsigned int v45;
  u32 v46[3];

  v4 = 0;
  v6 = a2;
  v42 = a4;
  if ( a2 >= 8 )
  {
    qmemcpy(v46, "octszstsstts", sizeof(v46));
    qmemcpy(v44, "cstssttc", sizeof(v44));
    while ( 1 )
    {
      v8 = mp4_box_fn_99(a3);
      if ( v8 < 8 )
      {
        mp4_io_fn_12(1937007212, 1, a3);
        return 0;
      }
      v9 = mp4_box_fn_99(a3);
      v10 = v9;
      if ( v6 < v8 )
      {
        mp4_io_fn_12(v9, 1, a3);
        v8 = v6;
      }
      v11 = ((u64)(v11) & 0xFFFFFFFF00000000ULL) | (u32)(mp4_box_fn_2(*a3));
      v12 = v11 + v8;
      v13 = v12 - 8;
      v14 = HIDWORD(v12) - 1 + ((unsigned int)(v12 - 8) < 0xFFFFFFF8);
      if ( v10 == 1937011555 )
        break;
      if ( v10 > 0x73747363 )
      {
        if ( v10 == 1937011571 )
          break;
        if ( v10 <= 0x73747373 )
        {
          if ( v10 != 1937011556 )
            goto LABEL_12;
          if ( v8 >= 0xC )
          {
            mp4_box_fn_103((int)v41, 1, a3);
            v17 = a3;
            v16 = 1937011556;
            if ( v41[0] != 0 )
              goto LABEL_14;
            if ( v8 >= 0x10 )
            {
              v17 = a3;
              v16 = 1937011556;
              if ( *(u32 *)(a1 + 24) == 0 )
              {
                *(u32 *)(a1 + 24) = v8;
                strcpy((char *)(a1 + 28), "dsts");
                mp4_box_fn_103(a1 + 33, 3, a3);
                v38 = mp4_box_fn_99(a3);
                v39 = (unsigned int)(*(u32 *)(a1 + 24) - 16) >> 4;
                *(u32 *)(a1 + 36) = v38;
                if ( v39 < v38 )
                {
                  *(u32 *)(a1 + 36) = v39;
                  mp4_io_fn_12(1937011556, 2, a3);
                }
                if ( *(u32 *)(a1 + 36) != 0 )
                {
                  v40 = mp4_sample_fn_42(a1 + 24, v8 - 16, a3, v42);
                  v21 = 20481;
                  if ( v40 == 20481 )
                    return v21;
                  *(u32 *)(a1 + 272) = *(u32 *)(a1 + 40);
                }
                *(u32 *)(a1 + 44) = a1;
                goto LABEL_16;
              }
LABEL_59:
              v18 = 8;
LABEL_15:
              mp4_io_fn_12(v16, v18, v17);
              mp4_box_fn_24(*a3, v19, v13, v14);
LABEL_16:
              v20 = a3[1];
              goto LABEL_17;
            }
          }
          v30 = 1937011556;
          goto LABEL_79;
        }
        if ( v10 == v46[1] )
          break;
        if ( v10 != v46[2] )
          goto LABEL_12;
        v15 = a3[21];
      }
      else
      {
        if ( v10 == 1668576371 )
          break;
        if ( v10 <= 0x63747473 || v10 != v46[0] )
        {
LABEL_12:
          v16 = 1937007212;
LABEL_13:
          v17 = a3;
LABEL_14:
          v18 = 4;
          goto LABEL_15;
        }
        v15 = a3[21];
      }
LABEL_36:
      if ( mp4_box_fn_19(v15) )
      {
        v29 = *(u32 *)(a1 + 284);
LABEL_81:
        v37 = mp4_io_fn_16(v29, v8, v10, v28, v13, v14, a3);
        v21 = 20481;
        goto LABEL_72;
      }
      if ( mp4_box_fn_21((u32 *)a3[21]) )
      {
        v29 = *(u32 *)(a1 + 284);
        goto LABEL_81;
      }
      if ( v8 < 0xC )
      {
        v30 = v10;
LABEL_79:
        mp4_io_fn_12(v30, 0x20000000, a3);
        v20 = a3[1];
        goto LABEL_17;
      }
      mp4_box_fn_103((int)v41, 1, a3);
      v16 = v10;
      if ( v41[0] != 0 )
        goto LABEL_13;
      v43 = 16;
      if ( v10 == v46[1] )
      {
        v43 = 20;
        v4 = a1 + 160;
      }
      if ( v8 < v43 )
      {
        v30 = v10;
        goto LABEL_79;
      }
      if ( v10 == v46[2] )
      {
        v4 = a1 + 52;
      }
      else if ( v10 == v44[0] )
      {
        v4 = a1 + 124;
      }
      else if ( v10 == v46[0] )
      {
        v4 = a1 + 196;
      }
      else
      {
        if ( v10 == v44[1] )
          v4 = a1 + 232;
        if ( v10 == 0x73747373 )
          v4 = a1 + 88;
      }
      v31 = 4;
      if ( v10 == v44[0] )
        v31 = 12;
      if ( v10 == v46[2] || v10 == v44[1] )
        v31 = 8;
      if ( *(u32 *)v4 != 0 )
      {
        v16 = v10;
        v17 = a3;
        goto LABEL_59;
      }
      *(u32 *)v4 = v8;
      v32 = v41[0];
      *(u32 *)(v4 + 4) = v10;
      *(u8 *)(v4 + 8) = v32;
      mp4_box_fn_103(v4 + 9, 3, a3);
      if ( v43 == 20 )
        *(u32 *)(v4 + 12) = mp4_box_fn_99(a3);
      v33 = mp4_box_fn_99(a3);
      v34 = *(u32 *)(v4 + 12);
      *(u32 *)(v4 + 16) = v33;
      if ( v34 != 0 )
        goto LABEL_16;
      v45 = (v8 - v43) / v31;
      if ( v45 < v33 )
      {
        mp4_io_fn_12(v10, 2, a3);
        v34 = *(u32 *)(v4 + 12);
        *(u32 *)(v4 + 16) = v45;
      }
      if ( v34 != 0 )
      {
        v20 = a3[1];
        goto LABEL_17;
      }
      v35 = *(u32 *)(v4 + 16);
      if ( v35 == 0 )
      {
        v20 = a3[1];
        goto LABEL_17;
      }
      v36 = v31 >> 2;
      if ( v10 == v46[0] )
        v37 = mp4_sample_fn_41(v4 + 20, v35, v36, a3);
      else
        v37 = mp4_sample_fn_40(v4 + 20, v35, v36, a3);
      v21 = 20481;
LABEL_72:
      if ( v37 == 20481 )
        return v21;
      v20 = a3[1];
LABEL_17:
      v21 = 20481;
      if ( (v20 & 0xE0000000) != 0 )
        return v21;
      v22 = mp4_box_fn_2(*a3);
      if ( v23 < v14 )
      {
        v24 = v10;
LABEL_34:
        mp4_io_fn_12(v24, 1, a3);
        mp4_box_fn_24(*a3, v27, v13, v14);
        v6 -= v8;
        goto LABEL_22;
      }
      if ( v14 == v23 )
      {
        if ( v22 < v13 )
        {
          v24 = v10;
          goto LABEL_34;
        }
        v6 -= v8;
      }
      else
      {
        v6 -= v8;
      }
LABEL_22:
      if ( v6 < 8 )
        goto LABEL_23;
    }
    v15 = a3[21];
    goto LABEL_36;
  }
LABEL_23:
  if ( v6 != 0 )
    mp4_io_fn_12(1937007212, 1, a3);
  if ( !mp4_box_fn_19(a3[21]) && !mp4_box_fn_21((u32 *)a3[21]) )
    return 0;
  v26 = mp4_api_fn_12(*(u32 *)(a1 + 284));
  v21 = 20481;
  if ( v26 == 0 )
    return 0;
  return v21;
}


int mp4_sample_fn_40(int *a1, unsigned int a2, int a3, int *a4)
{
  unsigned int v7;
  int v8;
  int *v9;
  int *v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int result;
  int v16;

  v7 = a2;
  if ( a2 == 0 )
    return 0;
  while ( *a1 == 0 )
  {
    v8 = mp4_sample_fn_5(28);
    *a1 = v8;
    v9 = (int *)v8;
    a1 = (int *)(v8 + 12);
    v10 = a4;
    if ( v8 == 0 )
      goto LABEL_12;
    v11 = *(u32 *)(v8 + 16);
    *(u32 *)(v8 + 8) = a3;
    if ( v11 != 0 )
      goto LABEL_15;
    v13 = 100;
    v12 = mp4_sample_fn_6(400 * a3);
    if ( v7 <= 0x64 )
      v13 = v7;
    v10 = a4;
    v7 -= v13;
    v9[4] = v12;
    if ( v12 == 0 )
    {
LABEL_12:
      v16 = 0x40000000;
      goto LABEL_13;
    }
    v14 = v9[2];
    v9[1] = 100;
    mp4_box_fn_104(v12, 4 * v13 * v14, a4);
    result = 20481;
    if ( (a4[1] & 0xE0000000) != 0 )
      return result;
    *v9 = v13;
    if ( v7 == 0 )
      return 0;
  }
  v10 = a4;
LABEL_15:
  v16 = 0x20000000;
LABEL_13:
  mp4_io_fn_12(0, v16, v10);
  return 20481;
}


int mp4_sample_fn_41(int *a1, unsigned int a2, int a3, int *a4)
{
  unsigned int v7;
  int v8;
  int *v9;
  int *v10;
  int v11;
  u8 *v12;
  int v13;
  int v14;
  int result;
  int v16;

  v7 = a2;
  if ( a2 == 0 )
    return 0;
  while ( *a1 == 0 )
  {
    v8 = mp4_sample_fn_5(28);
    *a1 = v8;
    v9 = (int *)v8;
    a1 = (int *)(v8 + 12);
    v10 = a4;
    if ( v8 == 0 )
      goto LABEL_12;
    v11 = *(u32 *)(v8 + 24);
    *(u32 *)(v8 + 8) = a3;
    if ( v11 != 0 )
      goto LABEL_15;
    v13 = 100;
    v12 = (u8 *)mp4_sample_fn_6(800 * a3);
    if ( v7 <= 0x64 )
      v13 = v7;
    v10 = a4;
    v7 -= v13;
    v9[6] = (int)v12;
    if ( v12 == NULL )
    {
LABEL_12:
      v16 = 0x40000000;
      goto LABEL_13;
    }
    v14 = v9[2];
    v9[1] = 100;
    mp4_box_fn_105(v12, 4 * v13 * v14, a4);
    result = 20481;
    if ( (a4[1] & 0xE0000000) != 0 )
      return result;
    *v9 = v13;
    if ( v7 == 0 )
      return 0;
  }
  v10 = a4;
LABEL_15:
  v16 = 0x20000000;
LABEL_13:
  mp4_io_fn_12(0, v16, v10);
  return 20481;
}


int mp4_sample_fn_42(unsigned int a1, unsigned int a2, int *a3, int a4)
{
  int v4;
  int v6;
  unsigned int v7;
  int v9;
  int i;
  unsigned int v11;
  unsigned int v12;
  int v13;
  unsigned int v14;
  unsigned int v15;
  int v16;
  unsigned int *v17;
  unsigned int *v18;
  s16 v19;
  unsigned int v20;
  int v21;
  int v22;
  unsigned int v23;
  unsigned int v24;
  int v25;
  int *v27;
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
  int *v39;
  int v40;
  int v42;
  int v43;
  u16 *v44;

  v4 = a4;
  v6 = a1 + 16;
  v7 = a2;
  v43 = *(u32 *)(a1 + 12);
  if ( *(u32 *)(a1 + 16) != 0 )
  {
    do
    {
      v9 = *(u32 *)(*(u32 *)v6 + 32);
      v6 = *(u32 *)v6 + 32;
    }
    while ( v9 != 0 );
  }
  *(u32 *)(a1 + 12) = 0;
  if ( a2 >= 8 )
  {
    for ( i = *(u32 *)(a1 + 12); ; i = *(u32 *)(a1 + 12) )
    {
      if ( i == v43 )
      {
        v27 = a3;
        v28 = 2;
LABEL_30:
        mp4_io_fn_12(1937011556, v28, v27);
        return 0;
      }
      v11 = mp4_box_fn_99(a3);
      if ( v11 < 8 )
        goto LABEL_29;
      v42 = mp4_box_fn_99(a3);
      if ( v7 < v11 )
      {
        mp4_io_fn_12(v42, 1, a3);
        v11 = v7;
      }
      v12 = mp4_box_fn_2(*a3) + v11;
      v14 = v12 - 8;
      v15 = v13 + (v12 < v11) - 1 + (v12 - 8 < 0xFFFFFFF8);
      v16 = v42;
      if ( v11 < 0x10 )
      {
        v39 = a3;
        v40 = 0x20000000;
        goto LABEL_66;
      }
      v17 = (unsigned int *)mp4_sample_fn_5(72);
      v18 = v17;
      *(u32 *)v6 = v17;
      if ( v17 == NULL )
      {
        v16 = v42;
        v39 = a3;
        v40 = 0x40000000;
LABEL_66:
        mp4_io_fn_12(v16, v40, v39);
        return 20481;
      }
      ++*(u32 *)(a1 + 12);
      v17[1] = v42;
      v6 = (int)(v17 + 8);
      *v17 = v11;
      mp4_box_fn_103((int)(v17 + 2), 6, a3);
      v19 = mp4_box_fn_101(a3);
      v18[4] = a4;
      v20 = *(u32 *)(a1 + 12);
      *((u16 *)v18 + 7) = v19;
      v18[6] = v20;
      v18[7] = a1;
      if ( a4 == -2 )
      {
        v4 = 1986618469;
        if ( v18[1] != 0x504E4720 )
          v4 = -1;
      }
      if ( v4 == 1868854125 )
      {
        v18[9] = 0;
        v11 -= 16;
        v35 = v11 < 8;
        v7 -= 16;
      }
      else
      {
        if ( (unsigned int)v4 <= 0x6F64736D )
        {
          if ( v4 != 1196573008 )
            goto LABEL_15;
          v31 = v42;
          if ( v11 >= 0x28 )
          {
            v33 = mp4_sample_fn_5(24);
            v18[9] = v33;
            if ( v33 != 0 )
            {
              mp4_io_fn_5(v33, a3);
              v11 -= 40;
              v7 -= 40;
              goto LABEL_18;
            }
            v31 = v42;
LABEL_42:
            v32 = 0x40000000;
          }
          else
          {
LABEL_38:
            v32 = 0x20000000;
          }
          mp4_io_fn_12(v31, v32, a3);
          v21 = a3[1];
          goto LABEL_19;
        }
        if ( v4 == 1936684398 )
        {
          v31 = v42;
          if ( v11 < 0x24 )
            goto LABEL_38;
          v37 = mp4_sample_fn_5(36);
          v31 = v42;
          if ( v37 == 0 )
            goto LABEL_42;
          v18[9] = v37;
          v44 = (u16 *)v37;
          v38 = mp4_box_fn_101(a3);
          *v44 = v38;
          if ( v38 != 0 )
          {
            v31 = v42;
            if ( v11 < 0x34 )
              goto LABEL_38;
            mp4_io_fn_4(v44, 1, a3);
            v11 -= 52;
            v7 -= 52;
          }
          else
          {
            mp4_io_fn_4(v44, 0, a3);
            v11 -= 36;
            v7 -= 36;
          }
        }
        else
        {
          if ( v4 != 1986618469 )
          {
LABEL_15:
            mp4_io_fn_12(1937011556, 4, a3);
            if ( v11 < 0x11 )
            {
              v18[9] = 0;
              v7 -= v11;
              goto LABEL_17;
            }
            v30 = mp4_sample_fn_5(v11 - 16);
            v18[9] = v30;
            if ( v30 != 0 )
            {
              mp4_box_fn_103(v30, v11 - 16, a3);
              v7 -= v11;
LABEL_17:
              v11 = 0;
            }
LABEL_18:
            v21 = a3[1];
            goto LABEL_19;
          }
          v31 = v42;
          if ( v11 < 0x56 )
            goto LABEL_38;
          v34 = mp4_sample_fn_5(72);
          v31 = v42;
          if ( v34 == 0 )
            goto LABEL_42;
          v18[9] = v34;
          v11 -= 86;
          mp4_io_fn_3(v34, a3);
          v7 -= 86;
        }
        v35 = v11 < 8;
      }
      if ( v35 )
      {
        v21 = a3[1];
      }
      else
      {
        v36 = mp4_sample_fn_43(v18, v11, 0, a3);
        v22 = 20481;
        if ( v36 == 20481 )
          return v22;
        v21 = a3[1];
      }
LABEL_19:
      v22 = 20481;
      if ( (v21 & 0xE0000000) != 0 )
        return v22;
      v23 = mp4_box_fn_2(*a3);
      if ( v24 >= v15 )
      {
        if ( v15 != v24 )
        {
          v7 -= v11;
          goto LABEL_24;
        }
        if ( v23 >= v14 )
        {
          v7 -= v11;
          goto LABEL_24;
        }
        v25 = v42;
      }
      else
      {
        v25 = v42;
      }
      mp4_io_fn_12(v25, 1, a3);
      mp4_box_fn_24(*a3, v29, v14, v15);
      v7 -= v11;
LABEL_24:
      if ( v7 < 8 )
        break;
    }
  }
  if ( v7 != 0 )
  {
LABEL_29:
    v27 = a3;
    v28 = 1;
    goto LABEL_30;
  }
  return 0;
}


int mp4_sample_fn_43(u32 *a1, unsigned int a2, int a3, int *a4)
{
  unsigned int v5;
  unsigned int v7;
  int v8;
  unsigned int v9;
  unsigned int v10;
  int v11;
  int v12;
  unsigned int v13;
  unsigned int v14;
  u32 *v15;
  int v16;
  int v17;
  int v18;
  int v19;
  unsigned int v20;
  unsigned int v21;
  unsigned int v22;
  int v23;
  int *v25;
  int v26;
  int v27;
  int *v28;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  int v34;
  unsigned int v35;
  char v36;
  int v37;
  unsigned int v38;
  int v39;
  int v40;
  unsigned int v41;
  int *v42;
  unsigned int v43;
  int v44;
  int v45;
  int v46;
  unsigned int *v47;
  u32 *v48;
  int v49;
  int v50;
  int v51;
  int v52;
  char v53;
  int v54;
  int v55;
  char v56;
  u8 v57[15];
  u32 v58[4];
  unsigned int v59;
  int v60;

  v5 = a2;
  if ( a2 >= 8 )
  {
    v60 = 1986618469;
    while ( 1 )
    {
      v7 = mp4_box_fn_99(a4);
      if ( v7 < 8 )
      {
        v23 = a1[1];
        goto LABEL_24;
      }
      v8 = mp4_box_fn_99(a4);
      v9 = v8;
      if ( v5 < v7 )
      {
        mp4_io_fn_12(v8, 1, a4);
        v7 = v5;
      }
      v10 = mp4_box_fn_2(*a4) + v7;
      v12 = 1702061171;
      v13 = v10 - 8;
      v14 = v11 + (v10 < v7) - 1 + (v10 - 8 < 0xFFFFFFF8);
      if ( v9 == 1702061171 )
        break;
      if ( v9 > 0x65736473 )
      {
        if ( v9 != 1936289382 )
        {
          v15 = a1;
          if ( v9 != 1970628964 )
            goto LABEL_10;
          v41 = 0;
          v42 = v58;
          if ( v7 < 0x18 )
          {
            v25 = a4;
            v23 = 1970628964;
            v26 = 0x20000000;
            goto LABEL_25;
          }
          do
          {
            ++v41;
            *v42++ = mp4_box_fn_99(a4);
          }
          while ( v41 < 4 );
          if ( v58[1] != 567431118 )
          {
            v29 = 1970628964;
LABEL_61:
            v28 = a4;
            goto LABEL_35;
          }
          v29 = 1970628964;
          if ( v58[2] != -1148688036 )
            goto LABEL_61;
          v28 = a4;
          if ( v58[3] != -87439552 )
            goto LABEL_35;
          v30 = 4;
          if ( v58[0] != 860111955 )
            goto LABEL_33;
          v30 = 8;
          if ( a1[17] != 0 )
            goto LABEL_33;
          v43 = v7 - 24;
          v44 = mp4_sample_fn_5(v7 - 24);
          v45 = v44;
          if ( v44 == 0 )
          {
            v34 = 860111955;
            goto LABEL_41;
          }
          if ( mp4_box_fn_103(v44, v7 - 24, a4) == v43 )
          {
            a1[17] = v45;
            a1[16] = v43;
            goto LABEL_12;
          }
          mp4_track_fn_65(v45);
          v34 = 860111955;
          v35 = 0x80000000;
LABEL_42:
          mp4_io_fn_12(v34, v35, a4);
          v19 = a4[1];
          goto LABEL_13;
        }
        v46 = a1[4];
        v12 = v46 ^ 0x736F756E;
        if ( v46 != v60 && v46 != 0x736F756E )
        {
          v15 = a1;
          v16 = 1936289382;
          goto LABEL_11;
        }
        if ( mp4_io_fn_13(a1 + 12, 1936289382, 16, v12, v10 - 8, v14, a4) == 20481 )
          goto LABEL_12;
        v47 = (unsigned int *)a1[12];
        *v47 = v7;
        v47[1] = 1936289382;
        v47[2] = v7 - 8;
        v47[3] = mp4_sample_fn_5(v7 - 8);
        v48 = (u32 *)a1[12];
        v49 = v48[3];
        if ( v49 == 0 )
        {
          v34 = v48[1];
LABEL_41:
          v35 = 0x40000000;
          goto LABEL_42;
        }
        v50 = v48[2];
        goto LABEL_78;
      }
      if ( v9 != 1635148611 )
      {
        v15 = a1;
LABEL_10:
        v16 = v9;
LABEL_11:
        v17 = mp4_sample_fn_44(v15, v16, v7, v12, v13, v14, a4);
        v18 = 20481;
        if ( v17 != 0 )
          return v18;
LABEL_12:
        v19 = a4[1];
        goto LABEL_13;
      }
      v15 = a1;
      v16 = 1635148611;
      if ( a1[4] != v60 )
        goto LABEL_11;
      if ( v7 < 0xF )
      {
        v28 = a4;
        v29 = 1635148611;
        v30 = 0x20000000;
LABEL_33:
        mp4_io_fn_12(v29, v30, v28);
        mp4_box_fn_24(*a4, v31, v13, v14);
        v19 = a4[1];
        goto LABEL_13;
      }
      mp4_box_fn_103((int)&v56, 1, a4);
      v28 = a4;
      v29 = 1635148611;
      if ( v56 != 1 )
        goto LABEL_35;
      if ( mp4_io_fn_13(a1 + 11, 1635148611, 28, v32, v13, v14, a4) == 20481 )
        goto LABEL_12;
      if ( mp4_codec_fn_59(8, 5, a1[11] + 16) != 0 || mp4_codec_fn_59(8, 5, a1[11] + 24) != 0 )
      {
        v34 = 1635148611;
        goto LABEL_41;
      }
      v33 = a1[11];
      *(u32 *)v33 = v7;
      v36 = v56;
      *(u32 *)(v33 + 4) = 1635148611;
      *(u8 *)(v33 + 8) = v36;
      mp4_box_fn_103(a1[11] + 9, 1, a4);
      mp4_box_fn_103(a1[11] + 10, 1, a4);
      mp4_box_fn_103(a1[11] + 11, 1, a4);
      mp4_box_fn_103((int)v57, 1, a4);
      *(u8 *)(a1[11] + 12) = v57[0] >> 2;
      *(u8 *)(a1[11] + 13) = v57[0] & 3;
      mp4_box_fn_103((int)v57, 1, a4);
      *(u8 *)(a1[11] + 14) = v57[0] >> 5;
      *(u8 *)(a1[11] + 15) = v57[0] & 0x1F;
      v37 = a1[11];
      if ( *(u8 *)(v37 + 15) != 0 )
      {
        v40 = mp4_io_fn_1(v13, v14, v37 + 15, &v59, *(u32 *)(v37 + 16), a4);
        v38 = v59;
        if ( v40 == 20481 )
        {
          v19 = a4[1];
          goto LABEL_13;
        }
      }
      else
      {
        v59 = v7 - 14;
        v38 = v7 - 14;
      }
      if ( v38 == 0 )
      {
        v34 = 1635148611;
LABEL_49:
        v35 = 0x20000000;
        goto LABEL_42;
      }
      mp4_box_fn_103(a1[11] + 20, 1, a4);
      v39 = a1[11];
      if ( *(u8 *)(v39 + 20) != 0 )
        mp4_io_fn_1(v13, v14, v39 + 20, &v59, *(u32 *)(v39 + 24), a4);
      v19 = a4[1];
LABEL_13:
      v18 = 20481;
      if ( (v19 & 0xE0000000) != 0 )
        return v18;
      v20 = mp4_box_fn_2(*a4);
      if ( v21 < v14 )
      {
        v22 = v9;
LABEL_29:
        mp4_io_fn_12(v22, 1, a4);
        mp4_box_fn_24(*a4, v27, v13, v14);
        v5 -= v7;
        goto LABEL_18;
      }
      if ( v14 == v21 )
      {
        if ( v20 < v13 )
        {
          v22 = v9;
          goto LABEL_29;
        }
        v5 -= v7;
      }
      else
      {
        v5 -= v7;
      }
LABEL_18:
      if ( v5 < 8 )
        goto LABEL_19;
    }
    v34 = 1702061171;
    if ( v7 < 0xC )
      goto LABEL_49;
    mp4_box_fn_103((int)&v56, 1, a4);
    if ( v56 != 0 )
    {
      v28 = a4;
      v29 = 1702061171;
LABEL_35:
      v30 = 4;
      goto LABEL_33;
    }
    v51 = a1[4];
    v12 = v51 ^ 0x736F756E;
    if ( v51 != v60 && v51 != 0x736F756E && v51 != 1868854125 )
    {
      v15 = a1;
      v16 = 1702061171;
      goto LABEL_11;
    }
    if ( mp4_io_fn_13(a1 + 10, 1702061171, 16, v12, v13, v14, a4) == 20481 )
      goto LABEL_12;
    v52 = a1[10];
    *(u32 *)v52 = v7;
    v53 = v56;
    *(u32 *)(v52 + 4) = 1702061171;
    *(u8 *)(v52 + 8) = v53;
    mp4_box_fn_103(a1[10] + 9, 3, a4);
    v54 = *(u32 *)(a1[10] + 12);
    if ( v54 != 0 )
      mp4_track_fn_65(v54);
    if ( v7 == 12 )
      goto LABEL_12;
    v55 = a1[10];
    *(u32 *)(v55 + 12) = mp4_sample_fn_5(v7 - 12);
    v49 = *(u32 *)(a1[10] + 12);
    v50 = v7 - 12;
    if ( v49 == 0 )
    {
      mp4_io_fn_12(1702061171, 0x40000000, a4);
      return 20481;
    }
LABEL_78:
    mp4_box_fn_103(v49, v50, a4);
    v19 = a4[1];
    goto LABEL_13;
  }
LABEL_19:
  if ( v5 == 0 )
    return 0;
  v23 = a1[1];
LABEL_24:
  v25 = a4;
  v26 = 1;
LABEL_25:
  mp4_io_fn_12(v23, v26, v25);
  return 0;
}


int mp4_sample_fn_44(int a1, int a2, int a3, int a4, int a5, int a6, int *a7)
{
  int v12;
  unsigned int v14;
  int v15;
  int v16;
  u32 *v17;
  int v18;
  int v19;
  int v20;
  int v22;

  v12 = 0xFFFF;
  mp4_io_fn_12(*(u32 *)(a1 + 4), 4, a7);
  v14 = 0;
  v15 = 0;
  do
  {
    if ( *(u32 *)(v15 + a1 + 52) != 0 )
    {
      ++v14;
    }
    else
    {
      v12 = v14;
      v14 = (unsigned int)(void *)0x4;
    }
    v15 = 4 * v14;
  }
  while ( v14 < 3 );
  if ( v12 == 0xFFFF )
  {
    mp4_io_fn_12(a2, 2, a7);
    mp4_box_fn_24(*a7, v22, a5, a6);
    v16 = 262140;
  }
  else
  {
    v16 = 4 * v12;
  }
  v18 = v16 + a1;
  v17 = (u32 *)mp4_sample_fn_5(12);
  *(u32 *)(v18 + 52) = v17;
  if ( v17 != NULL )
  {
    *v17 = a3;
    v17[1] = a2;
    if ( a3 == 8 )
      return 0;
    v19 = a3 - 8;
    v17[2] = mp4_sample_fn_5(v19);
    v20 = *(u32 *)(*(u32 *)(v18 + 52) + 8);
    if ( v20 != 0 )
    {
      mp4_box_fn_103(v20, v19, a7);
      return 0;
    }
  }
  mp4_io_fn_12(a2, 0x40000000, a7);
  return 20481;
}


