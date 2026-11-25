#include "mp4msv_internal.h"

int mp4_api_fn_1(u32 *a1, unsigned int a2, u32 *a3, u32 *a4, u32 *a5)
{
  unsigned int v8;
  int v10;
  int v11;
  unsigned int v13;
  unsigned int v14;
  unsigned int v15;
  unsigned int v16;
  unsigned int *v17;
  unsigned int v18;
  unsigned int v19;
  unsigned int *v20;
  unsigned int v21;
  unsigned int v22;
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
  s64 v33;
  unsigned int v34;
  unsigned int *v35;
  unsigned int v36;
  unsigned int v37;
  unsigned int v38;
  int v39;
  unsigned int v40;
  unsigned int v41;
  unsigned int *v42;
  unsigned int v43;
  unsigned int v44;
  unsigned int v45;
  unsigned int *v46;
  unsigned int v47;
  unsigned int i;
  unsigned int v49;
  unsigned int v50;
  unsigned int *v51;
  unsigned int v52;
  unsigned int j;
  unsigned int v54;
  unsigned int v55;
  unsigned int v56;
  unsigned int v57;
  int v58;
  int v59;
  unsigned int *v60;
  unsigned int v61;
  unsigned int v62;
  unsigned int v63;
  unsigned int *v64;
  unsigned int v65;
  int v66;
  int v67;
  int v68;
  int v69;
  unsigned int v70;
  unsigned int v71;
  int v72;
  int v73;
  unsigned int v74;
  unsigned int v75;
  unsigned int v76;
  unsigned int v77;
  unsigned int v78;
  unsigned int v79;

  v8 = a1[12];
  if ( a2 < v8 )
    return 8201;
  v10 = a1[13];
  if ( v8 + v10 - 1 < a2 )
    return 8201;
  v11 = 8195;
  if ( *a1 == 0 )
    return v11;
  v13 = a1[10];
  v14 = a1[16];
  if ( v14 >= v13 )
  {
    if ( v13 + a1[11] - 1 >= v14 )
      goto LABEL_11;
    a1[16] = 0;
  }
  else
  {
    a1[16] = 0;
  }
  v14 = 0;
  a1[17] = 0;
LABEL_11:
  if ( v14 == 0 )
  {
    a1[16] = v13;
    a1[17] = v8;
  }
  v15 = a1[17];
  if ( a2 < v15 )
  {
    if ( a2 - v8 < v15 - a2 )
    {
      v17 = (unsigned int *)a1[8];
      v15 = v8;
      v16 = v13;
      a1[17] = v8;
      a1[16] = v13;
      goto LABEL_17;
    }
    v16 = a1[16];
    goto LABEL_74;
  }
  v16 = a1[16];
  if ( v8 + v10 - a2 >= a2 - v15 )
  {
LABEL_74:
    v17 = (unsigned int *)a1[8];
    goto LABEL_17;
  }
  v56 = a1[11];
  v57 = v13 + v56 - 1;
  v17 = (unsigned int *)a1[8];
  if ( v16 == v57 )
  {
LABEL_17:
    v18 = v15;
    if ( a2 < v15 )
    {
      v40 = v16 - 1;
      if ( v17 == NULL )
        return 10;
      v11 = 10;
      if ( v17[2] != 0 && v16 >= v13 )
      {
        v41 = *v17;
        v76 = v16 - v13;
        v42 = v17;
        if ( *v17 < v16 - v13 + 1 )
        {
          do
          {
            v42 = (unsigned int *)v42[7];
            v43 = v76 - v41 + 1;
            v76 -= v41;
            if ( v42 == NULL )
              return 10;
            v41 = *v42;
          }
          while ( *v42 < v43 );
        }
        v44 = v40 - v13;
        v45 = 0;
        v67 = *(u32 *)(4 * v76 * v42[2] + v42[3]);
        while ( 1 )
        {
          if ( v17[2] == 0 || v40 - v45 < v13 )
            return 10;
          v77 = v44;
          v46 = v17;
          if ( *v17 < v44 + 1 )
          {
            v47 = *v17;
            for ( i = v44; ; i = v49 )
            {
              v46 = (unsigned int *)v46[7];
              v49 = i - v47;
              v50 = i - v47 + 1;
              v77 = v49;
              if ( v46 == NULL )
                return 10;
              v47 = *v46;
              if ( *v46 >= v50 )
                break;
            }
          }
          v73 = *(u32 *)(4 * v77 * v46[2] + v46[3]);
          if ( v17[2] <= 1 || v40 - v45 < v13 )
            return 10;
          v78 = v44;
          v51 = v17;
          if ( *v17 < v44 + 1 )
          {
            v52 = *v17;
            for ( j = v44; ; j = v54 )
            {
              v51 = (unsigned int *)v51[7];
              v54 = j - v52;
              v55 = j - v52 + 1;
              v78 = v54;
              if ( v51 == NULL )
                return 10;
              v52 = *v51;
              if ( *v51 >= v55 )
                break;
            }
          }
          v30 = *(u32 *)(4 * (v78 * v51[2] + 1) + v51[3]);
          v18 -= (v67 - v73) * v30;
          if ( a2 >= v18 )
            break;
          ++v45;
          v67 = v73;
          --v44;
          if ( v40 < v45 )
            return 8195;
        }
        v37 = v40 - v45;
        v38 = v18;
        v39 = v73 - 1;
LABEL_40:
        if ( v30 == 0 )
          _break(0, 7u);
        *a3 = v39 + (a2 - v38) / v30;
        *a4 = v38 + (a2 - v38) / v30 * v30;
        v32 = a1[10];
        v33 = ((u64)(v33) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v37 < v32) << 32);
        v33 = ((u64)(v33) & 0xFFFFFFFF00000000ULL) | (u32)(v17[2] <= 2);
        if ( v33 != 0 )
        {
          return 10;
        }
        else
        {
          v34 = *v17;
          v35 = v17;
          v79 = v37 - v32;
          if ( *v17 < v37 - v32 + 1 )
          {
            do
            {
              v35 = (unsigned int *)v35[7];
              v36 = v79 - v34 + 1;
              v79 -= v34;
              if ( v35 == NULL )
                return 10;
              v34 = *v35;
            }
            while ( *v35 < v36 );
          }
          v11 = 0;
          *a5 = *(u32 *)(4 * (v79 * v35[2] + 2) + v35[3]);
          a1[17] = v38;
          a1[16] = v37;
        }
      }
    }
    else
    {
      if ( v17 == NULL )
        return 10;
      if ( v17[2] == 0 || v16 < v13 )
      {
        return 10;
      }
      else
      {
        v19 = *v17;
        v71 = v16 - v13;
        v20 = v17;
        if ( *v17 < v16 - v13 + 1 )
        {
          do
          {
            v20 = (unsigned int *)v20[7];
            v21 = v71 - v19 + 1;
            v71 -= v19;
            if ( v20 == NULL )
              return 10;
            v19 = *v20;
          }
          while ( *v20 < v21 );
        }
        v66 = *(u32 *)(4 * v71 * v20[2] + v20[3]);
        if ( v16 < v13 + a1[11] )
        {
          v22 = v16 - v13 + 1;
          v23 = v13 + a1[11];
          while ( 1 )
          {
            if ( v16 == v23 - 1 )
            {
              v72 = a1[14] + a1[15] + 1;
            }
            else
            {
              if ( v17[2] == 0 || v16 + 1 < v13 )
                return 10;
              v74 = v22;
              v24 = v17;
              if ( *v17 < v22 + 1 )
              {
                v25 = *v17;
                do
                {
                  v24 = (unsigned int *)v24[7];
                  v26 = v74 - v25 + 1;
                  v74 -= v25;
                  if ( v24 == NULL )
                    return 10;
                  v25 = *v24;
                }
                while ( *v24 < v26 );
              }
              v72 = *(u32 *)(4 * v74 * v24[2] + v24[3]);
            }
            v27 = v17;
            if ( v17[2] <= 1 || v16 < v13 )
              return 10;
            v28 = *v17;
            v75 = v16 - v13;
            if ( *v17 < v22 )
            {
              do
              {
                v27 = (unsigned int *)v27[7];
                v29 = v75 - v28 + 1;
                v75 -= v28;
                if ( v27 == NULL )
                  return 10;
                v28 = *v27;
              }
              while ( *v27 < v29 );
            }
            v30 = *(u32 *)(4 * (v75 * v27[2] + 1) + v27[3]);
            v31 = (v72 - v66) * v30;
            if ( a2 < v18 + v31 )
            {
              v37 = v16;
              v38 = v18;
              v39 = v66 - 1;
              goto LABEL_40;
            }
            ++v16;
            v66 = v72;
            v18 += v31;
            ++v22;
            v23 = v13 + a1[11];
            if ( v16 >= v23 )
              return 8195;
          }
        }
        return 8195;
      }
    }
    return v11;
  }
  v58 = a1[14];
  v59 = a1[15];
  a1[16] = v57;
  if ( v17 == NULL )
    return 10;
  if ( v17[2] == 0 || v57 < v13 )
    return 10;
  v69 = v56 - 1;
  v60 = v17;
  if ( *v17 < v56 )
  {
    v61 = *v17;
    do
    {
      v60 = (unsigned int *)v60[7];
      v62 = v69 - v61 + 1;
      v69 -= v61;
      if ( v60 == NULL )
        return 10;
      v61 = *v60;
    }
    while ( *v60 < v62 );
  }
  v16 = a1[16];
  v68 = *(u32 *)(4 * v69 * v60[2] + v60[3]);
  v11 = 10;
  if ( v17[2] > 1 && v16 >= v13 )
  {
    v63 = *v17;
    v70 = v16 - v13;
    v64 = v17;
    if ( *v17 < v16 - v13 + 1 )
    {
      do
      {
        v64 = (unsigned int *)v64[7];
        v65 = v70 - v63 + 1;
        v70 -= v63;
        if ( v64 == NULL )
          return 10;
        v63 = *v64;
      }
      while ( *v64 < v65 );
    }
    v15 = v8 + v10 - (v58 + v59 + 1 - v68) * *(u32 *)(4 * (v70 * v64[2] + 1) + v64[3]);
    a1[17] = v15;
    goto LABEL_17;
  }
  return v11;
}


int mp4_api_fn_2(u32 *a1, unsigned int a2, int a3, int a4)
{
  int v5;
  int v9;
  int v10;
  int v11;
  int v12;
  unsigned int *v13;
  unsigned int v14;
  unsigned int v15;
  unsigned int v17;
  unsigned int v18;
  unsigned int v19;
  unsigned int v20;
  unsigned int v21;
  unsigned int v22;
  unsigned int v23;
  int v24;
  unsigned int v25;
  unsigned int v26;
  int v27;
  int v28;
  int v29;
  int v30;

  v5 = 8195;
  if ( a1 != NULL )
  {
    v5 = 7;
    if ( a1[115] != 0 )
    {
      v5 = 6;
      if ( a3 != 0 )
      {
        v9 = mp4_api_fn_3(a1 + 2);
        v5 = 8195;
        if ( v9 != 0 )
        {
          v10 = mp4_api_fn_4(a1 + 24);
          v5 = 8195;
          if ( v10 != 0 )
          {
            v11 = mp4_api_fn_5(a1 + 48);
            v5 = 8195;
            if ( v11 != 0 )
            {
              v12 = mp4_api_fn_6(a1 + 62);
              v5 = 8195;
              if ( v12 != 0 )
              {
                v13 = (unsigned int *)a1[115];
                v14 = v13[8];
                if ( v14 != 0 && a2 < v14 )
                {
                  v15 = v13[11];
                  v17 = *(u32 *)(v15 + 16);
                  if ( v17 == 0 )
                    _break(0, 7u);
                  v18 = (a2 + a3 - 1) / v17 * v17 + v17;
                  v19 = a2 / v17 * v17;
                  v20 = v18 - v19;
                  v21 = v18 - 1;
                  if ( v14 - v19 < v18 - v19 )
                  {
                    v20 = v14 - v19;
                    v21 = v14 - 1;
                  }
                  v22 = *(u32 *)(v15 + 28);
                  if ( v22 < v20 )
                  {
                    v20 = *(u32 *)(v15 + 28);
                    v21 = v19 + v22 - 1;
                  }
                  v23 = v13[1];
                  if ( v23 != 0 )
                  {
                    if ( v13[3] != 0 )
                    {
                      v25 = *v13;
                      if ( v19 >= *v13 )
                      {
                        v26 = v25 + v23 - 1;
                        if ( v26 >= v19 )
                        {
                          if ( v21 >= v25 )
                          {
                            v5 = 0;
                            if ( v26 >= v21 )
                              return v5;
                            v24 = v13[8];
                          }
                          else
                          {
                            v24 = v13[8];
                          }
                        }
                        else
                        {
                          v24 = v13[8];
                        }
                      }
                      else
                      {
                        v24 = v13[8];
                      }
                    }
                    else
                    {
                      v24 = v13[8];
                    }
                  }
                  else
                  {
                    v24 = v13[8];
                  }
                  v27 = mp4_io_fn_21(a1 + 2, v19, v20, v24, v13[9], a4);
                  if ( v27 == 0 )
                  {
                    v27 = mp4_io_fn_24(a1 + 24, v19, v20, v13[8], v13[10], a4);
                    if ( v27 == 0 )
                    {
                      v27 = mp4_api_fn_1(a1 + 24, v19, &v28, &v29, &v30);
                      if ( v27 == 0 )
                      {
                        v27 = mp4_io_fn_27(a1 + 48, v19, v20, v29, (int *)(a1[115] + 24), a4);
                        if ( v27 == 0 )
                        {
                          v27 = mp4_api_fn_56(a1, v19, v20, a4);
                          if ( v27 == 0 )
                          {
                            v27 = mp4_io_fn_33(a1 + 76, v19, v20, v13[8], a4);
                            if ( v27 == 0 )
                              v27 = mp4_io_fn_36(a1 + 94, v19, v20, v13[8], a4);
                          }
                        }
                      }
                    }
                  }
                  mp4_api_fn_12(a1);
                  return v27;
                }
                else
                {
                  return 8201;
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


int mp4_api_fn_3(u32 *a1)
{
  return *a1 != 0;
}


int mp4_api_fn_4(u32 *a1)
{
  return *a1 != 0;
}


int mp4_api_fn_5(u32 *a1)
{
  return *a1 != 0;
}


int mp4_api_fn_6(u32 *a1)
{
  return *a1 != 0;
}


int mp4_api_fn_7(int a1, u32 *a2)
{
  int v2;
  int v4;
  int v5;
  u32 *v6;
  int v7;

  v2 = 8195;
  *a2 = 0;
  if ( a1 != 0 )
  {
    v4 = *(u32 *)(a1 + 460);
    v5 = a1 + 192;
    v6 = (u32 *)(a1 + 192);
    v2 = 7;
    if ( v4 != 0 )
    {
      v7 = mp4_api_fn_5(v6);
      v2 = 8195;
      if ( v7 )
      {
        *a2 = mp4_api_fn_8(v5);
        return 0;
      }
    }
  }
  return v2;
}


int mp4_api_fn_8(int a1)
{
  return *(u32 *)(a1 + 16);
}


int mp4_api_fn_9(int a1)
{
  return *(u32 *)(a1 + 16);
}


void mp4_api_fn_10(u32 *a1, u32 *a2, u32 *a3, u32 *a4, u32 *a5)
{
  if ( a2 != NULL )
    *a2 = a1[12];
  if ( a3 != NULL )
    *a3 = a1[13];
  if ( a4 != NULL )
    *a4 = a1[14];
  if ( a5 != NULL )
    *a5 = a1[15];
}


void mp4_api_fn_11(u32 *a1, u32 *a2, u32 *a3, u32 *a4, u32 *a5)
{
  if ( a2 != NULL )
    *a2 = a1[12];
  if ( a3 != NULL )
    *a3 = a1[13];
  if ( a4 != NULL )
    *a4 = a1[14];
  if ( a5 != NULL )
    *a5 = a1[15];
}


int mp4_api_fn_12(u32 *a1)
{
  int v1;
  u32 *v3;
  u32 *v4;
  int v5;
  int v6;
  int v7;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  u32 v14[3];

  v1 = 8195;
  if ( a1 != NULL )
  {
    v3 = (u32 *)a1[115];
    v4 = a1 + 2;
    v1 = 7;
    if ( v3 != NULL )
    {
      mp4_api_fn_10(v4, (u32 *)a1[115], v3 + 1, v3 + 4, v3 + 5);
      mp4_api_fn_11(a1 + 24, &v13, v14, (u32 *)(a1[115] + 8), (u32 *)(a1[115] + 12));
      mp4_api_fn_13(a1[115], a1[115] + 4, v13, v14[0]);
      v5 = a1[115];
      v6 = a1[59];
      v13 = a1[58];
      v14[0] = v6;
      mp4_api_fn_13(v5, v5 + 4, v13, v6);
      mp4_api_fn_13(a1[115] + 8, a1[115] + 12, a1[72], a1[73]);
      if ( a1[76] != 0 && a1[80] != 0 )
      {
        v7 = a1[89];
        v11 = a1[115];
        v13 = a1[88];
        v14[0] = v7;
        mp4_api_fn_13(v11, v11 + 4, v13, v7);
        v12 = a1[94];
      }
      else
      {
        v12 = a1[94];
      }
      if ( v12 != 0 )
      {
        v9 = a1[115];
        v10 = a1[107];
        v13 = a1[106];
        v14[0] = v10;
        mp4_api_fn_13(v9, v9 + 4, v13, v10);
      }
      return 0;
    }
  }
  return v1;
}


int mp4_api_fn_13(unsigned int *a1, int *a2, s64 a3)
{
  unsigned int v3;
  unsigned int v4;
  int v5;
  int v6;
  unsigned int v7;
  unsigned int v8;

  v3 = *a1;
  v4 = *a1;
  if ( *a1 != 0 || *a2 != 0 )
  {
    if ( a3 == 0 )
    {
      *a1 = 0;
LABEL_10:
      *a2 = 0;
      return 0;
    }
    v5 = *a2;
    if ( v3 == (u32)a3 )
    {
      v6 = v4 + v5;
      if ( v5 == HIDWORD(a3) )
        return 0;
    }
    else
    {
      v6 = v4 + v5;
    }
    v7 = v6 - 1;
    v8 = a3 + HIDWORD(a3) - 1;
    if ( v6 - 1 < (unsigned int)a3 || v8 < v4 )
    {
      *a1 = 0;
      goto LABEL_10;
    }
    if ( v4 < (unsigned int)a3 )
    {
      *a1 = a3;
      v3 = a3;
    }
    if ( v8 < v7 )
      v7 = v8;
    *a2 = v7 - v3 + 1;
  }
  return 0;
}


int mp4_api_fn_14(u32 *a1)
{
  u32 *v2;
  int v3;
  u32 *v4;
  int result;

  v2 = a1 + 8;
  v3 = a1[8];
  v4 = a1 + 9;
  if ( v3 != 0 )
    result = mp4_api_fn_20(v4, v2);
  a1[11] = 0;
  a1[12] = 0;
  a1[13] = 0;
  a1[14] = 0;
  a1[15] = 0;
  a1[10] = 0;
  return result;
}


int mp4_api_fn_15(u32 *a1)
{
  u32 *v2;
  int v3;
  u32 *v4;
  int result;

  v2 = a1 + 8;
  v3 = a1[8];
  v4 = a1 + 9;
  if ( v3 != 0 )
    result = mp4_api_fn_20(v4, v2);
  a1[11] = 0;
  a1[12] = 0;
  a1[13] = 0;
  a1[14] = 0;
  a1[15] = 0;
  a1[10] = 0;
  return result;
}


int mp4_api_fn_16(u32 *a1)
{
  u32 *v2;
  int v3;
  u32 *v4;
  int result;

  v2 = a1 + 8;
  v3 = a1[8];
  v4 = a1 + 9;
  if ( v3 != 0 )
    result = mp4_api_fn_20(v4, v2);
  a1[11] = 0;
  a1[10] = 0;
  return result;
}


int mp4_api_fn_17(u32 *a1)
{
  u32 *v2;
  int v3;
  u32 *v4;
  int result;

  v2 = a1 + 8;
  v3 = a1[8];
  v4 = a1 + 9;
  if ( v3 != 0 )
    result = mp4_api_fn_20(v4, v2);
  a1[11] = 0;
  a1[10] = 0;
  return result;
}


int mp4_api_fn_18(u32 *a1)
{
  u32 *v2;
  int v3;
  u32 *v4;
  int result;

  v2 = a1 + 8;
  v3 = a1[8];
  v4 = a1 + 9;
  if ( v3 != 0 )
    result = mp4_api_fn_20(v4, v2);
  a1[11] = 0;
  a1[12] = 0;
  a1[13] = 0;
  a1[10] = 0;
  return result;
}


int mp4_api_fn_19(u32 *a1)
{
  u32 *v2;
  int v3;
  u32 *v4;
  int result;

  v2 = a1 + 8;
  v3 = a1[8];
  v4 = a1 + 9;
  if ( v3 != 0 )
    result = mp4_api_fn_20(v4, v2);
  a1[11] = 0;
  a1[12] = 0;
  a1[13] = 0;
  a1[10] = 0;
  return result;
}


int mp4_api_fn_20(int *a1, int *a2)
{
  int v2;
  int v3;
  int result;

  v2 = *a2;
  if ( *a2 != 0 )
  {
    do
    {
      v3 = *(u32 *)(v2 + 28);
      if ( v2 != 0 )
      {
        *(u32 *)(v2 + 28) = *a1;
        result = *a2;
        *a1 = *a2;
        *a2 = 0;
      }
      *a2 = v3;
      v2 = v3;
    }
    while ( v3 != 0 );
    *a2 = 0;
  }
  return result;
}


int mp4_api_fn_21(u32 *a1, int a2)
{
  int v3;
  int v4;
  int i;
  int v6;
  int v7;
  int v9;
  u32 v10[3];

  v3 = a1[8];
  v4 = 10;
  if ( v3 != 0 )
  {
    for ( i = *(u32 *)(v3 + 28); i != 0; i = *(u32 *)(i + 28) )
      v3 = i;
    v4 = mp4_api_fn_23(v3, a2, &v9, v10);
    if ( v4 == 0 )
    {
      v6 = a1[13] + v10[0];
      v7 = a1[11] + 1;
      a1[15] += v9;
      a1[13] = v6;
      a1[11] = v7;
      a1[18] = 1;
      return 0;
    }
  }
  return v4;
}


int mp4_api_fn_22(int a1, int a2)
{
  *(u32 *)(a1 + 52) = a2;
  return 0;
}


int mp4_api_fn_23(u32 *a1, int a2, int *a3, u32 *a4)
{
  int v5; // dc
  int v6;
  unsigned int v7;
  int v8;
  int v9;
  int v11;
  int v12;

  v5 = a1 == NULL;
  v6 = 10;
  if ( !v5 )
  {
    v7 = a1[2];
    v8 = *a1 - 1;
    if ( v7 != 0 )
      v11 = *(u32 *)(4 * v8 * v7 + a1[3]);
    if ( v7 >= 2 && a1 != NULL )
      v12 = *(u32 *)(4 * (v8 * v7 + 1) + a1[3]);
    v6 = 0;
    v9 = a2 - v11 + 2;
    *a3 = v9;
    *a4 = v9 * v12;
  }
  return v6;
}


int mp4_api_fn_24(int a1, unsigned int a2, int a3, int a4, u32 *a5)
{
  int v7;
  unsigned int v8;
  int v9;
  u32 v11[4];

  v7 = 0;
  *a5 = 0;
  if ( a2 != a3 )
  {
    v8 = *(u32 *)(a1 + 40);
    v9 = a3 - 1;
    if ( a3 == v8 )
    {
      *a5 = a4;
    }
    else
    {
      if ( a2 < v8 )
      {
        *a5 = a4;
        a2 = *(u32 *)(a1 + 40);
      }
      v7 = mp4_api_fn_40(a1, a2, v9, v11);
      if ( v7 == 0 )
      {
        v7 = 0;
        *a5 += v11[0];
      }
    }
  }
  return v7;
}


int mp4_api_fn_25(int a1, unsigned int a2, unsigned int a3, u32 *a4, int *a5)
{
  int v5;
  int v11;
  int v12;
  int v13;
  s64 v15;
  int v16;
  int v17;
  int v18;
  int v19;
  unsigned int v20;
  unsigned int v21;
  int v22;
  unsigned int v23;
  int *v24;
  int *v25;
  int v26;
  int v27;

  v5 = 0;
  *a4 = 0;
  v11 = *(u32 *)(a1 + 16);
  v25 = NULL;
  v12 = 8195;
  if ( v11 - 1 < a2 || v11 - 1 < a3 )
    return v12;
  v13 = *(u32 *)(a1 + 12);
  if ( v13 != 0 )
  {
    v12 = 0;
    *a4 = v13 * (a3 - a2 + 1);
    return v12;
  }
  v15 = *(u64 *)(a1 + 24) + 4 * a2;
  if ( mp4_box_fn_2(*a5) != (u32)v15 )
  {
    v18 = *a5;
LABEL_8:
    v19 = mp4_box_fn_24(v18, v17, v15, SHIDWORD(v15));
    v12 = 2;
    if ( v19 != 0 )
      return v12;
    v20 = a3 - a2;
    goto LABEL_10;
  }
  if ( v16 != HIDWORD(v15) )
  {
    v18 = *a5;
    goto LABEL_8;
  }
  v20 = a3 - a2;
LABEL_10:
  v21 = v20 + 1;
  v22 = a2;
  if ( v20 != -1 )
  {
    do
    {
      v23 = a5[4];
      if ( v21 < v23 )
        v23 = v21;
      v5 = mp4_io_fn_41(&v25, v22, v23, &v26, (int **)(a1 + 36), a5);
      if ( v5 != 0 )
        break;
      v5 = mp4_api_fn_26(v25, &v27);
      if ( v5 != 0 )
      {
        v5 = 10;
        break;
      }
      v21 -= v26;
      v22 += v26;
      *a4 += v27;
    }
    while ( v21 != 0 );
  }
  v12 = v5;
  if ( v25 != NULL )
  {
    v25[7] = *(u32 *)(a1 + 36);
    v24 = v25;
    v25 = NULL;
    *(u32 *)(a1 + 36) = v24;
  }
  return v12;
}


int mp4_api_fn_26(unsigned int *a1, u32 *a2)
{
  unsigned int v2;
  u32 *i;

  *a2 = 0;
  v2 = 0;
  for ( i = (u32 *)a1[3]; v2 < *a1; ++i )
  {
    ++v2;
    *a2 += *i;
  }
  return 0;
}


int mp4_api_fn_27(u32 *a1)
{
  int v1;

  v1 = 1;
  if ( *a1 != 0 && a1[8] != 0 )
    return a1[18];
  return v1;
}


int mp4_api_fn_28(u32 *a1)
{
  int v1;

  v1 = 1;
  if ( *a1 != 0 && a1[8] != 0 )
    return a1[16];
  return v1;
}


int mp4_api_fn_29(int a1)
{
  int v1;

  v1 = 0;
  if ( *(u64 *)(a1 + 24) != 0 )
    return *(u32 *)(a1 + 44) == 0;
  return v1;
}


int mp4_api_fn_30(int a1, unsigned int a2, int a3, int a4)
{
  int v6;
  int v9;
  int v10;
  u32 *v11;
  int v12;
  unsigned int v13;

  v6 = 8195;
  if ( a1 != 0 )
  {
    v9 = *(u32 *)(a1 + 460);
    v10 = a1 + 192;
    v11 = (u32 *)(a1 + 192);
    v6 = 7;
    if ( v9 != 0 )
    {
      v12 = mp4_api_fn_5(v11);
      v6 = 8195;
      if ( v12 )
      {
        v13 = *(u32 *)(v9 + 32);
        v6 = 8201;
        if ( v13 != 0 && a2 < v13 )
        {
          v6 = mp4_api_fn_48(a1, a2, a4, 8201);
          if ( v6 == 0 )
            return mp4_api_fn_31(v10, a2, a3);
        }
      }
    }
  }
  return v6;
}


int mp4_api_fn_31(u32 *a1, unsigned int a2, u32 *a3)
{
  int v3;
  unsigned int *i;
  unsigned int v6;
  int v7;
  unsigned int v8;
  unsigned int v9;
  unsigned int v10;
  unsigned int v11;

  v3 = a1[3];
  if ( v3 != 0 )
  {
    *a3 = v3;
    return 0;
  }
  i = (unsigned int *)a1[8];
  v6 = a1[10];
  if ( i == NULL )
    return 10;
  if ( i[2] != 0 && a2 >= v6 )
  {
    v8 = a2 - v6;
    v11 = a2 - v6;
    v9 = *i;
    if ( *i >= a2 - v6 + 1 )
    {
LABEL_11:
      v7 = 0;
      *a3 = *(u32 *)(4 * v11 * i[2] + i[3]);
      return v7;
    }
    for ( i = (unsigned int *)i[7]; ; i = (unsigned int *)i[7] )
    {
      v10 = v8 - v9 + 1;
      v8 -= v9;
      v11 = v8;
      if ( i == NULL )
        break;
      v9 = *i;
      if ( *i >= v10 )
        goto LABEL_11;
    }
    return 10;
  }
  return 10;
}


int mp4_api_fn_32(int *a1, int *a2)
{
  int v2;
  int v5;
  u32 *v6;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  u32 v14[4];

  v2 = 8195;
  if ( a1 != NULL )
  {
    v5 = a1[115];
    v6 = a1 + 2;
    v2 = 7;
    if ( v5 != 0 )
    {
      *a1 = 8;
      v2 = mp4_api_fn_33(v6, v14, *(u32 *)(v5 + 44));
      if ( v2 == 0 )
      {
        v8 = a1[115];
        *a1 += v14[0];
        v2 = mp4_api_fn_34(a1 + 24, v14, *(u32 *)(v8 + 44));
        if ( v2 == 0 )
        {
          v9 = *(u32 *)(a1[115] + 44);
          *a1 += v14[0];
          v2 = mp4_api_fn_35(a1 + 48, v14, v9);
          if ( v2 == 0 )
          {
            v10 = *(u32 *)(a1[115] + 44);
            *a1 += v14[0];
            v2 = mp4_api_fn_36(a1 + 62, v14, v10);
            if ( v2 == 0 )
            {
              v11 = a1[115];
              *a1 += v14[0];
              v2 = mp4_api_fn_37(a1 + 76, v14, *(u32 *)(v11 + 44));
              if ( v2 == 0 )
              {
                v12 = *(u32 *)(a1[115] + 44);
                *a1 += v14[0];
                v2 = mp4_api_fn_38(a1 + 94, v14, v12);
                if ( v2 == 0 )
                {
                  v2 = 0;
                  v13 = *a1 + v14[0];
                  *a1 = v13;
                  *a2 = v13;
                }
              }
            }
          }
        }
      }
    }
  }
  return v2;
}


int mp4_api_fn_33(int *a1, int *a2)
{
  int v2;
  int result;

  v2 = *a1;
  result = 0;
  if ( *a1 != 0 )
  {
    v2 = 8 * a1[4] + 16;
    *a1 = v2;
  }
  *a2 = v2;
  return result;
}


int mp4_api_fn_34(int *a1, int *a2)
{
  int v2;
  int result;

  v2 = *a1;
  result = 0;
  if ( *a1 != 0 )
  {
    v2 = 12 * a1[4] + 16;
    *a1 = v2;
  }
  *a2 = v2;
  return result;
}


int mp4_api_fn_35(int *a1, int *a2)
{
  int v2;
  int result;
  int v4;

  v2 = *a1;
  result = 0;
  if ( *a1 != 0 )
  {
    v2 = 20;
    if ( a1[3] != 0 )
    {
      *a1 = 20;
    }
    else
    {
      v4 = 4 * a1[4] + 20;
      *a1 = v4;
      v2 = v4;
    }
    result = 0;
  }
  *a2 = v2;
  return result;
}


int mp4_api_fn_36(int *a1, int *a2)
{
  int v2;
  int result;

  v2 = *a1;
  result = 0;
  if ( *a1 != 0 )
  {
    v2 = 4 * a1[4] + 16;
    *a1 = v2;
  }
  *a2 = v2;
  return result;
}


int mp4_api_fn_37(int *a1, int *a2)
{
  int v2;
  int result;

  v2 = *a1;
  result = 0;
  if ( *a1 != 0 )
  {
    v2 = 4 * a1[4] + 16;
    *a1 = v2;
  }
  *a2 = v2;
  return result;
}


int mp4_api_fn_38(int *a1, int *a2)
{
  int v2;
  int result;

  v2 = *a1;
  result = 0;
  if ( *a1 != 0 )
  {
    v2 = 8 * a1[4] + 16;
    *a1 = v2;
  }
  *a2 = v2;
  return result;
}


int mp4_api_fn_39(u32 *a1, unsigned int a2, u32 *a3, unsigned int *a4)
{
  unsigned int *v7;
  int v9;
  u32 *v11;
  unsigned int v12;
  unsigned int v13;
  int v14;
  u32 *v15;
  unsigned int *v16;
  unsigned int v17;
  unsigned int *v18;
  unsigned int v19;
  unsigned int v20;
  unsigned int v21;
  unsigned int v22;
  unsigned int v23;
  int v24;
  unsigned int v25;
  unsigned int v26;
  int v27;
  u32 *v28;
  unsigned int v29;
  unsigned int v30;
  u32 v31[2];
  unsigned int v32;
  unsigned int v33[3];

  v7 = (unsigned int *)a1[115];
  *a3 = 0;
  a3[1] = 0;
  if ( a2 >= *v7 && *v7 + v7[1] - 1 >= a2 )
  {
    v9 = mp4_api_fn_1(a1 + 24, a2, &v29, &v30, v31);
    if ( v9 != 0 )
      return v9;
    v11 = (u32 *)a1[115];
    v12 = v29;
    v13 = v11[2];
    if ( v29 >= v13 )
    {
      v15 = a1 + 62;
      if ( v13 + v11[3] - 1 >= v29 )
        goto LABEL_12;
      v14 = v11[11];
    }
    else
    {
      v14 = v11[11];
    }
    v9 = mp4_api_fn_56(a1, *v11, v11[1], v14);
    if ( v9 != 0 )
      return v9;
    v12 = v29;
    v15 = a1 + 62;
LABEL_12:
    v16 = (unsigned int *)v15[8];
    v17 = v15[10];
    v32 = v12;
    if ( v16 == NULL || v16[2] == 0 || v12 < v17 )
      return 10;
    v18 = v16;
    v19 = *v16;
    v32 = v12 - v17;
    if ( v19 < v12 - v17 + 1 )
    {
      do
      {
        v18 = (unsigned int *)v18[7];
        v20 = v32 - v19 + 1;
        v32 -= v19;
        if ( v18 == NULL )
          return 10;
        v19 = *v18;
      }
      while ( *v18 < v20 );
    }
    v21 = v18[2];
    v22 = v18[3];
    v23 = v30;
    v24 = *(u32 *)(4 * v32 * v21 + v22);
    *a4 = v12;
    v31[1] = v24;
    *a3 = v24;
    a3[1] = 0;
    if ( a2 != v23 )
    {
      v25 = a1[58];
      v26 = a2 - 1;
      if ( v23 >= v25 )
      {
        v28 = a1 + 48;
      }
      else
      {
        v27 = *(u32 *)(a1[115] + 24);
        *a3 = v24 + v27;
        a3[1] = (v27 >> 31) + __CFADD__(v24, v27);
        v26 = a2 - 1;
        v28 = a1 + 48;
        v23 = v25;
        if ( a2 == v25 )
          return 0;
      }
      v9 = mp4_api_fn_40(v28, v23, v26, v33);
      if ( v9 != 0 )
        return v9;
      *(u64 *)a3 += v33[0];
    }
    return 0;
  }
  return 8201;
}


int mp4_api_fn_40(u32 *a1, unsigned int a2, unsigned int a3, u32 *a4)
{
  int v4;
  unsigned int v5;
  unsigned int v6;
  int v7;
  int v8;

  v4 = 8201;
  v5 = a1[10];
  if ( a2 >= v5 )
  {
    v6 = v5 + a1[11] - 1;
    if ( v6 >= a2 )
    {
      v7 = v6 < a3;
      if ( a3 >= v5 && !v7 )
      {
        v8 = a1[3];
        v4 = 0;
        if ( v8 != 0 )
          *a4 = v8 * (a3 - a2 + 1);
        else
          return mp4_api_fn_41(a1[8], a1[10], a2, a3, 0, a4);
      }
    }
  }
  return v4;
}


int mp4_api_fn_41(unsigned int *i, int a2, int a3, int a4, int a5, u32 *a6)
{
  unsigned int v6;
  unsigned int v7;
  unsigned int v8;
  unsigned int v9;
  unsigned int v10;
  unsigned int *v11;
  int result;
  unsigned int v13;
  unsigned int v14;
  unsigned int v15;

  v6 = a3 - a2;
  *a6 = 0;
  v7 = a4 - a2;
  v15 = v6;
  if ( i != NULL )
  {
    v8 = *i;
    v9 = v6;
    if ( *i < v6 + 1 )
    {
      for ( i = (unsigned int *)i[7]; ; i = (unsigned int *)i[7] )
      {
        v10 = v9 - v8 + 1;
        v9 -= v8;
        v15 = v9;
        if ( i == NULL )
          break;
        v8 = *i;
        if ( *i >= v10 )
          break;
      }
    }
  }
  v11 = i;
  result = 10;
  if ( i != NULL )
  {
    v13 = v6;
    if ( v7 < v6 )
    {
      return 0;
    }
    else
    {
      while ( 1 )
      {
        v14 = v15;
        ++v13;
        if ( *v11 == v15 )
        {
          v11 = (unsigned int *)v11[7];
          v14 = 0;
          result = 10;
          if ( v11 == NULL )
            break;
        }
        v15 = v14 + 1;
        *a6 += *(u32 *)(4 * (v11[2] * v14 + a5) + v11[3]);
        if ( v7 < v13 )
          return 0;
      }
    }
  }
  return result;
}


int mp4_api_fn_42(u32 *a1, unsigned int a2, int a3, unsigned int *a4, u32 *a5, int a6)
{
  int v8;
  int v13;
  int v14;
  unsigned int v15;
  unsigned int v17;
  unsigned int v18;
  unsigned int i;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  u8 v27[4];
  int v28;
  int v29;

  v8 = 8195;
  if ( a1 != NULL )
  {
    v13 = a1[115];
    v8 = 7;
    if ( v13 != 0 )
    {
      v14 = mp4_api_fn_3(a1 + 2);
      v8 = 8195;
      if ( v14 )
      {
        v15 = *(u32 *)(v13 + 32);
        if ( v15 != 0 && a2 < v15 )
        {
          v8 = 8200;
          if ( a2 != 0 || a3 != 0 )
          {
            v8 = mp4_api_fn_44(a1, a2, &v26, v27, a6);
            if ( v8 == 0 )
            {
              v17 = a2 + 1;
              if ( a3 != 0 )
              {
                while ( 1 )
                {
                  v8 = 8200;
                  if ( v17 >= *(u32 *)(a1[115] + 32) )
                    break;
                  v24 = mp4_api_fn_49(a1, v17, a6);
                  if ( v24 == 0 )
                    v24 = mp4_io_fn_53(a1 + 76, v17, &v28);
                  if ( v24 != 0 )
                  {
                    v8 = v24;
                    if ( v24 == 8201 )
                      return 8200;
                    return v8;
                  }
                  if ( v28 == 1 )
                  {
                    *a4 = v17;
                    v25 = mp4_api_fn_44(a1, v17, &v29, v27, a6);
                    v8 = v25;
                    if ( v25 == 0 )
                    {
                      v22 = v29;
                      v23 = v26;
LABEL_22:
                      v8 = 0;
                      *a5 = v22 - v23;
                    }
                    return v8;
                  }
                  ++v17;
                }
              }
              else
              {
                v18 = 0;
                if ( a2 != 0 )
                {
                  for ( i = a2 - 1; ; --i )
                  {
                    ++v18;
                    v20 = mp4_api_fn_49(a1, i, a6);
                    if ( v20 == 0 )
                      v20 = mp4_io_fn_53(a1 + 76, i, &v28);
                    v8 = v20;
                    if ( v20 != 0 )
                      break;
                    if ( v28 == 1 )
                    {
                      *a4 = i;
                      v21 = mp4_api_fn_44(a1, i, &v29, v27, a6);
                      v8 = v21;
                      if ( v21 == 0 )
                      {
                        v22 = v26;
                        v23 = v29;
                        goto LABEL_22;
                      }
                      return v8;
                    }
                    if ( v18 >= a2 )
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
        else
        {
          return 8201;
        }
      }
    }
  }
  return v8;
}


int mp4_api_fn_43(int a1, unsigned int a2, int a3, int a4, int a5)
{
  int v5;
  int v11;
  u32 *v12;
  int v13;
  unsigned int v14;

  v5 = 8195;
  if ( a1 != 0 )
  {
    v11 = *(u32 *)(a1 + 460);
    v5 = 7;
    v12 = (u32 *)(a1 + 8);
    if ( v11 != 0 )
    {
      v13 = mp4_api_fn_3(v12);
      v5 = 8195;
      if ( v13 )
      {
        v14 = *(u32 *)(v11 + 32);
        v5 = 8201;
        if ( v14 != 0 && a2 < v14 )
          return mp4_api_fn_44(a1, a2, a3, a4, a5);
      }
    }
  }
  return v5;
}


int mp4_api_fn_44(int a1, int a2, int a3, int a4, int a5)
{
  int v9;

  v9 = mp4_api_fn_46(a1, a2, a5);
  if ( v9 == 0 )
    return mp4_api_fn_45(a1 + 8, a2, a3, a4);
  return v9;
}


int mp4_api_fn_45(u32 *a1, unsigned int a2, u32 *a3, u32 *a4)
{
  unsigned int v8;
  int v9;
  unsigned int v10;
  int v11;
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
  unsigned int v24;
  unsigned int *v25;
  unsigned int v26;
  unsigned int v27;
  unsigned int *v28;
  unsigned int v29;
  unsigned int v30;
  int v31;
  int v32;
  unsigned int v33;
  unsigned int *v34;
  int v35;
  int v36;
  unsigned int i;
  unsigned int v38;
  unsigned int *v39;
  unsigned int v40;
  unsigned int v41;
  unsigned int *v42;
  unsigned int v43;
  unsigned int v44;
  int v45;
  unsigned int v46;
  unsigned int *v47;
  unsigned int *v48;
  unsigned int v49;
  unsigned int v50;
  unsigned int v51;
  unsigned int *v52;
  unsigned int v53;
  unsigned int v54;
  int v55;
  int v56;
  int v57;
  int v58;
  unsigned int v59;
  int v60;
  int v61;
  int v62;
  unsigned int v63;
  unsigned int v64;
  unsigned int v65;
  unsigned int v66;
  unsigned int v67;

  v8 = a1[12];
  if ( a2 < v8 )
    return 8201;
  v9 = a1[13];
  if ( v8 + v9 - 1 < a2 )
    return 8201;
  v10 = a1[10];
  v13 = a1[16];
  if ( v13 >= v10 )
  {
    if ( v10 + a1[11] - 1 >= v13 )
      goto LABEL_11;
    a1[16] = 0;
  }
  else
  {
    a1[16] = 0;
  }
  v13 = 0;
  a1[17] = 0;
  a1[18] = 0;
LABEL_11:
  if ( v13 == 0 )
  {
    v15 = a1[14];
    a1[16] = v10;
    a1[18] = v15;
    a1[17] = v8;
  }
  v14 = a1[17];
  if ( a2 < v14 )
  {
    if ( a2 - v8 < v14 - a2 )
    {
      v14 = v8;
      v16 = v10;
      a1[18] = a1[14];
      a1[16] = v10;
      a1[17] = v8;
    }
    else
    {
      v16 = a1[16];
    }
LABEL_17:
    v17 = a2 < v14;
    goto LABEL_18;
  }
  v16 = a1[16];
  if ( v14 >= a2 )
    goto LABEL_17;
  v46 = v10 + a1[11] - 1;
  if ( v16 == v46 )
    goto LABEL_17;
  if ( v8 + v9 - a2 < a2 - v14 )
  {
    v47 = (unsigned int *)a1[8];
    a1[16] = v46;
    if ( v47 == NULL )
      return 10;
    v11 = 10;
    if ( v47[2] == 0 || v46 < v10 )
      return v11;
    v59 = v46 - v10;
    v48 = v47;
    if ( *v47 < v46 - v10 + 1 )
    {
      v49 = *v47;
      do
      {
        v48 = (unsigned int *)v48[7];
        v50 = v59 - v49 + 1;
        v59 -= v49;
        if ( v48 == NULL )
          return 10;
        v49 = *v48;
      }
      while ( *v48 < v50 );
    }
    v16 = a1[16];
    v58 = *(u32 *)(4 * v59 * v48[2] + v48[3]);
    v11 = 10;
    if ( v47[2] <= 1 || v16 < v10 )
      return v11;
    v51 = *v47;
    v52 = v47;
    v63 = v16 - v10;
    if ( *v47 < v16 - v10 + 1 )
    {
      do
      {
        v52 = (unsigned int *)v52[7];
        v53 = v63 - v51 + 1;
        v63 -= v51;
        if ( v52 == NULL )
          return 10;
        v51 = *v52;
      }
      while ( *v52 < v53 );
    }
    v54 = v8 + v9 - v58;
    v55 = a1[14] + a1[15];
    v14 = v54;
    v62 = *(u32 *)(4 * (v63 * v52[2] + 1) + v52[3]);
    a1[17] = v54;
    a1[18] = v55 - v58 * v62;
    goto LABEL_17;
  }
  v17 = a2 < v14;
LABEL_18:
  if ( v17 )
  {
    v33 = v16 - 1;
    v18 = v14;
    v11 = 8195;
    if ( v16 - 1 >= a1[4] )
      return v11;
    v34 = (unsigned int *)a1[8];
    v35 = 0;
    v36 = 0;
    for ( i = v33 - v10; ; --i )
    {
      v38 = v33 - v36;
      if ( v34 == NULL || v34[2] == 0 || v38 < v10 )
        break;
      v66 = i;
      v39 = (unsigned int *)a1[8];
      if ( *v34 < i + 1 )
      {
        v40 = *v34;
        do
        {
          v39 = (unsigned int *)v39[7];
          v41 = v66 - v40 + 1;
          v66 -= v40;
          if ( v39 == NULL )
            return 10;
          v40 = *v39;
        }
        while ( *v39 < v41 );
      }
      v57 = *(u32 *)(4 * v66 * v39[2] + v39[3]);
      if ( v34[2] <= 1 || v38 < v10 )
        return 10;
      v42 = (unsigned int *)a1[8];
      v67 = i;
      if ( *v34 < i + 1 )
      {
        v43 = *v34;
        do
        {
          v42 = (unsigned int *)v42[7];
          v44 = v67 - v43 + 1;
          v67 -= v43;
          if ( v42 == NULL )
            return 10;
          v43 = *v42;
        }
        while ( *v42 < v44 );
      }
      v18 -= v57;
      v61 = *(u32 *)(4 * (v67 * v42[2] + 1) + v42[3]);
      v35 += v61 * v57;
      if ( a2 >= v18 )
      {
        *a3 = a1[18] - v35 + (a2 - v18) * v61;
        *a4 = v61;
        v45 = a1[18];
        a1[16] = v33 - v36;
        v32 = v45 - v35;
LABEL_36:
        a1[18] = v32;
        v11 = 0;
        a1[17] = v18;
        return v11;
      }
      if ( v33 < ++v36 )
        return 8195;
    }
    return 10;
  }
  v18 = v14;
  v11 = 8195;
  if ( v16 < a1[4] )
  {
    v19 = a1[11];
    v20 = 0;
    v21 = v16;
    if ( v16 < v10 + v19 )
    {
      v22 = (unsigned int *)a1[8];
      v23 = v10 + v19;
      v24 = v16 - v10;
      while ( v22 != NULL && v22[2] != 0 && v21 >= v10 )
      {
        v64 = v24;
        v25 = (unsigned int *)a1[8];
        if ( *v22 < v24 + 1 )
        {
          v26 = *v22;
          do
          {
            v25 = (unsigned int *)v25[7];
            v27 = v64 - v26 + 1;
            v64 -= v26;
            if ( v25 == NULL )
              return 10;
            v26 = *v25;
          }
          while ( *v25 < v27 );
        }
        v56 = *(u32 *)(4 * v64 * v25[2] + v25[3]);
        if ( v22[2] <= 1 || v21 < v10 )
          return 10;
        v65 = v24;
        v28 = (unsigned int *)a1[8];
        if ( *v22 < v24 + 1 )
        {
          v29 = *v22;
          do
          {
            v28 = (unsigned int *)v28[7];
            v30 = v65 - v29 + 1;
            v65 -= v29;
            if ( v28 == NULL )
              return 10;
            v29 = *v28;
          }
          while ( *v28 < v30 );
        }
        v60 = *(u32 *)(4 * (v65 * v28[2] + 1) + v28[3]);
        if ( a2 < v18 + v56 )
        {
          *a3 = a1[18] + v20 + (a2 - v18) * v60;
          *a4 = v60;
          v31 = a1[18];
          a1[16] = v21;
          v32 = v31 + v20;
          goto LABEL_36;
        }
        ++v21;
        ++v24;
        v18 += v56;
        v20 += v60 * v56;
        if ( v21 >= v23 )
          return 8195;
      }
      return 10;
    }
    return 8195;
  }
  return v11;
}


int mp4_api_fn_46(u32 *a1, unsigned int a2, int a3)
{
  int v6;
  u32 *v7;
  int v8;
  int v10;
  unsigned int v11;
  int v12;
  unsigned int v13;
  unsigned int v14;
  int v15;
  unsigned int v16;
  unsigned int v17[4];

  if ( a1 == NULL )
    return 7;
  v6 = a1[115];
  v7 = a1 + 2;
  if ( v6 == 0 )
    return 7;
  v10 = mp4_api_fn_3(a1 + 2);
  v8 = 8195;
  if ( v10 )
  {
    v11 = *(u32 *)(v6 + 32);
    v8 = 8201;
    if ( v11 != 0 )
    {
      v8 = 8201;
      if ( a2 < v11 )
      {
        mp4_api_fn_10(v7, &v13, &v14, NULL, &v15);
        if ( v14 == 0 || v15 == 0 || a2 < v13 || (v8 = 0, v13 + v14 - 1 < a2) )
        {
          v8 = 2;
          if ( a3 != 0 )
          {
            v8 = mp4_box_fn_106(*(u32 **)(a1[115] + 44));
            if ( v8 == 0 )
            {
              v8 = mp4_io_fn_52(a2, v13, v14, *(u32 *)(a1[115] + 32), &v16, v17, a1[115]);
              if ( v8 == 0 )
              {
                v12 = mp4_io_fn_21(
                        v7,
                        v16,
                        v17[0],
                        *(u32 *)(a1[115] + 32),
                        *(u32 *)(a1[115] + 36),
                        *(u32 *)(a1[115] + 44));
                mp4_api_fn_12(a1);
                return v12;
              }
            }
          }
        }
      }
    }
  }
  return v8;
}


int mp4_api_fn_47(u32 *a1, unsigned int a2, int a3)
{
  int v6;
  u32 *v7;
  int v8;
  int v10;
  unsigned int v11;
  int v12;
  unsigned int v13;
  unsigned int v14;
  unsigned int v15;
  int v16;

  if ( a1 == NULL )
    return 7;
  v6 = a1[115];
  v7 = a1 + 24;
  if ( v6 == 0 )
    return 7;
  v10 = mp4_api_fn_4(a1 + 24);
  v8 = 8195;
  if ( v10 )
  {
    v11 = *(u32 *)(v6 + 32);
    v8 = 8201;
    if ( v11 != 0 )
    {
      v8 = 8201;
      if ( a2 < v11 )
      {
        mp4_api_fn_11(v7, &v13, &v14, NULL, NULL);
        if ( v14 == 0 || a2 < v13 || (v8 = 0, v13 + v14 - 1 < a2) )
        {
          v8 = 2;
          if ( a3 != 0 )
          {
            v8 = mp4_box_fn_106(*(u32 **)(a1[115] + 44));
            if ( v8 == 0 )
            {
              v8 = mp4_io_fn_52(a2, v13, v14, *(u32 *)(a1[115] + 32), &v15, (unsigned int *)&v16, a1[115]);
              if ( v8 == 0 )
              {
                v12 = mp4_io_fn_24(
                        v7,
                        v15,
                        v16,
                        *(u32 *)(a1[115] + 32),
                        *(u32 *)(a1[115] + 40),
                        *(u32 *)(a1[115] + 44));
                mp4_api_fn_12(a1);
                return v12;
              }
            }
          }
        }
      }
    }
  }
  return v8;
}


int mp4_api_fn_48(u32 *a1, unsigned int a2, int a3)
{
  int v6;
  u32 *v7;
  int v8;
  int v10;
  unsigned int v11;
  unsigned int v12;
  unsigned int v13;
  unsigned int v14;
  int v15;
  unsigned int v16;
  unsigned int v17;
  int v18;
  int v19;
  u32 v20[4];

  if ( a1 == NULL )
    return 7;
  v6 = a1[115];
  v7 = a1 + 48;
  if ( v6 == 0 )
    return 7;
  v10 = mp4_api_fn_5(a1 + 48);
  v8 = 8195;
  if ( v10 )
  {
    v11 = *(u32 *)(v6 + 32);
    v8 = 8201;
    if ( v11 != 0 )
    {
      if ( a2 >= v11 )
      {
        return 8201;
      }
      else
      {
        v12 = v7[11];
        v13 = v7[10];
        if ( v12 == 0 || a2 < v13 || (v8 = 0, v13 + v12 - 1 < a2) )
        {
          v8 = 2;
          if ( a3 != 0 )
          {
            v8 = mp4_box_fn_106(*(u32 **)(v6 + 44));
            if ( v8 == 0 )
            {
              v8 = mp4_io_fn_52(a2, v13, v12, *(u32 *)(a1[115] + 32), &v16, &v17, a1[115]);
              if ( v8 == 0 )
              {
                v14 = v16;
                v8 = mp4_api_fn_47(a1, v16, a3);
                if ( v8 == 0 )
                  v8 = mp4_api_fn_1(a1 + 24, v14, &v18, &v19, v20);
                if ( v8 == 0 )
                {
                  v15 = mp4_io_fn_27(v7, v16, v17, v19, (int *)(a1[115] + 24), *(u32 *)(a1[115] + 44));
                  mp4_api_fn_12(a1);
                  return v15;
                }
              }
            }
          }
        }
      }
    }
  }
  return v8;
}


int mp4_api_fn_49(u32 *a1, unsigned int a2, int a3)
{
  int v5;
  int v6;
  unsigned int v8;
  unsigned int v9;
  unsigned int v10;
  int v11;
  unsigned int v12;
  unsigned int v13[3];

  if ( a1 == NULL )
    return 7;
  v5 = a1[115];
  if ( v5 == 0 )
    return 7;
  v6 = 0;
  if ( a1[76] != 0 )
  {
    v8 = *(u32 *)(v5 + 32);
    v6 = 8201;
    if ( v8 != 0 )
    {
      if ( a2 < v8 )
      {
        v6 = 0;
        if ( a1[80] != 0 )
        {
          v9 = a1[89];
          v10 = a1[88];
          if ( v9 == 0 || a2 < v10 || v10 + v9 - 1 < a2 )
          {
            v6 = 2;
            if ( a3 != 0 )
            {
              v6 = mp4_box_fn_106(*(u32 **)(v5 + 44));
              if ( v6 == 0 )
              {
                v6 = mp4_io_fn_52(a2, v10, v9, *(u32 *)(a1[115] + 32), &v12, v13, a1[115]);
                if ( v6 == 0 )
                {
                  v11 = mp4_io_fn_33(a1 + 76, v12, v13[0], *(u32 *)(a1[115] + 32), *(u32 *)(a1[115] + 44));
                  mp4_api_fn_12(a1);
                  return v11;
                }
              }
            }
          }
        }
      }
      else
      {
        return 8201;
      }
    }
  }
  return v6;
}


int mp4_api_fn_50(u32 *a1, unsigned int a2, u32 *a3)
{
  unsigned int v4;
  int v6;
  int v7;
  unsigned int v9;
  unsigned int v10;
  unsigned int v11;
  unsigned int v12;
  int v13;
  unsigned int v14;
  int v15;
  unsigned int v16;
  unsigned int *v17;
  unsigned int v18;
  unsigned int *v19;
  unsigned int v20;
  unsigned int v21;
  unsigned int *v22;
  unsigned int v23;
  unsigned int *v24;
  unsigned int v25;
  unsigned int v26;
  unsigned int v27;
  unsigned int v28;
  int v29;
  unsigned int v30;
  unsigned int v31;
  unsigned int *v32;
  unsigned int i;
  unsigned int v34;
  unsigned int v35;
  unsigned int v36;
  unsigned int *v37;
  unsigned int v38;
  unsigned int *v39;
  unsigned int v40;
  unsigned int v41;
  unsigned int v42;
  int v43;
  unsigned int v44;
  unsigned int v45;
  unsigned int v46;
  unsigned int v47;

  v4 = a1[12];
  if ( a2 >= v4 )
  {
    v6 = a1[13];
    if ( v4 + v6 - 1 >= a2 )
    {
      if ( *a1 == 0 )
      {
        v7 = 0;
        *a3 = 0;
        return v7;
      }
      v9 = a1[10];
      v10 = a1[14];
      if ( v10 >= v9 )
      {
        if ( v9 + a1[11] - 1 >= v10 )
          goto LABEL_13;
        a1[14] = 0;
      }
      else
      {
        a1[14] = 0;
      }
      v10 = 0;
      a1[15] = 0;
LABEL_13:
      if ( v10 == 0 )
      {
        a1[14] = v9;
        a1[15] = v4;
      }
      v11 = a1[15];
      if ( a2 >= v11 )
      {
        v12 = a1[14];
        if ( v11 >= a2 )
          goto LABEL_20;
        v36 = v9 + a1[11] - 1;
        if ( v12 == v36 )
          goto LABEL_20;
        if ( v4 + v6 - a2 >= a2 - v11 )
        {
          v13 = a2 < v11;
LABEL_21:
          if ( v13 )
          {
            v28 = v12 - 1;
            v14 = v11;
            v7 = 8195;
            if ( v12 - 1 >= a1[4] )
              return v7;
            v22 = (unsigned int *)a1[8];
            v29 = 0;
            v30 = v12 - 1;
            while ( v22 != NULL )
            {
              v7 = 10;
              if ( v22[2] == 0 || v30 < v9 )
                return v7;
              v31 = *v22;
              v46 = v30 - v9;
              v32 = (unsigned int *)a1[8];
              if ( *v22 < v30 - v9 + 1 )
              {
                for ( i = v30 - v9; ; i = v34 )
                {
                  v32 = (unsigned int *)v32[7];
                  v34 = i - v31;
                  v35 = i - v31 + 1;
                  v46 = v34;
                  if ( v32 == NULL )
                    break;
                  v31 = *v32;
                  if ( *v32 >= v35 )
                    goto LABEL_51;
                }
                return 10;
              }
LABEL_51:
              v14 -= *(u32 *)(4 * v46 * v32[2] + v32[3]);
              if ( a2 >= v14 )
              {
                v21 = v30;
LABEL_34:
                if ( v22 != NULL )
                {
                  v7 = 10;
                  if ( v22[2] <= 1 || v21 < v9 )
                    return v7;
                  v23 = *v22;
                  v24 = v22;
                  v47 = v21 - v9;
                  if ( *v22 >= v21 - v9 + 1 )
                  {
LABEL_39:
                    v26 = v24[2];
                    v27 = v24[3];
                    v7 = 0;
                    *a3 = *(u32 *)(4 * (v47 * v26 + 1) + v27);
                    a1[15] = v14;
                    a1[14] = v21;
                    return v7;
                  }
                  while ( 1 )
                  {
                    v24 = (unsigned int *)v24[7];
                    v25 = v47 - v23 + 1;
                    v47 -= v23;
                    if ( v24 == NULL )
                      break;
                    v23 = *v24;
                    if ( *v24 >= v25 )
                      goto LABEL_39;
                  }
                }
                return 10;
              }
              if ( v28 < ++v29 )
                return 8195;
              v30 = v28 - v29;
            }
          }
          else
          {
            v14 = v11;
            v7 = 8195;
            if ( v12 >= a1[4] )
              return v7;
            v15 = a1[11];
            v16 = v12;
            if ( v12 >= v9 + v15 )
              return 8195;
            v17 = (unsigned int *)a1[8];
            while ( v17 != NULL && v17[2] != 0 && v16 >= v9 )
            {
              v18 = *v17;
              v45 = v16 - v9;
              v19 = (unsigned int *)a1[8];
              if ( *v17 < v16 - v9 + 1 )
              {
                do
                {
                  v19 = (unsigned int *)v19[7];
                  v20 = v45 - v18 + 1;
                  v45 -= v18;
                  if ( v19 == NULL )
                    return 10;
                  v18 = *v19;
                }
                while ( *v19 < v20 );
              }
              v43 = *(u32 *)(4 * v45 * v19[2] + v19[3]);
              if ( a2 < v14 + v43 )
              {
                v21 = v16;
                v22 = (unsigned int *)a1[8];
                goto LABEL_34;
              }
              ++v16;
              v14 += v43;
              if ( v16 >= v9 + v15 )
                return 8195;
            }
          }
          return 10;
        }
        v37 = (unsigned int *)a1[8];
        a1[14] = v36;
        if ( v37 == NULL )
          return 10;
        v7 = 10;
        if ( v37[2] == 0 || v36 < v9 )
          return v7;
        v38 = *v37;
        v39 = v37;
        v44 = v36 - v9;
        if ( *v37 < v36 - v9 + 1 )
        {
          do
          {
            v39 = (unsigned int *)v39[7];
            v40 = v44 - v38 + 1;
            v44 -= v38;
            if ( v39 == NULL )
              return 10;
            v38 = *v39;
          }
          while ( *v39 < v40 );
        }
        v41 = v39[2];
        v42 = v39[3];
        v12 = a1[14];
        v4 = v4 + v6 - *(u32 *)(4 * v44 * v41 + v42);
        v11 = v4;
      }
      else
      {
        if ( a2 - v4 >= v11 - a2 )
        {
          v12 = a1[14];
LABEL_20:
          v13 = a2 < v11;
          goto LABEL_21;
        }
        v11 = v4;
        v12 = v9;
        a1[14] = v9;
      }
      a1[15] = v4;
      goto LABEL_20;
    }
  }
  return 8201;
}


int mp4_api_fn_51(u32 *a1, unsigned int a2, unsigned int a3, u32 *a4, int *a5)
{
  int v9;
  int v10;
  unsigned int v11;
  int v12;
  u32 v14[4];

  *a4 = 0;
  v9 = a1[10];
  v10 = a1[11];
  v11 = v9 + v10;
  if ( a3 >= v9 + v10 )
  {
    v12 = mp4_api_fn_40(a1, a2, v9 + v10 - 1, a4);
    if ( v12 == 0 )
    {
      v12 = mp4_api_fn_25((int)a1, v11, a3, v14, a5);
      if ( v12 == 0 )
      {
        *a4 += v14[0];
        return 0;
      }
    }
  }
  else
  {
    v12 = mp4_api_fn_40(a1, a2, a3, a4);
    if ( v12 == 0 )
      return 0;
  }
  return v12;
}


int mp4_api_fn_52(int a1, int a2, unsigned int a3, u32 *a4, int a5)
{
  int v9;
  int v10;
  int v11;
  u32 v13[4];

  *a4 = 0;
  v9 = *(u32 *)(a1 + 48);
  v10 = *(u32 *)(a1 + 52);
  if ( a3 >= v9 + v10 )
  {
    v11 = mp4_api_fn_53(a1, a2, v9 + v10 - 1);
    if ( v11 == 0 )
    {
      v11 = mp4_api_fn_54(a1, a3, v13, a5);
      if ( v11 == 0 )
      {
        *a4 += v13[0];
        return 0;
      }
    }
  }
  else
  {
    v11 = mp4_api_fn_53(a1, a2, a3);
    if ( v11 == 0 )
      return 0;
  }
  return v11;
}


int mp4_api_fn_53(u32 *a1, unsigned int a2, unsigned int a3, u32 *a4)
{
  unsigned int v5;
  unsigned int v7;
  int v8;
  unsigned int v9;
  unsigned int v11;
  int v12;
  int v13;
  int v14[3];

  *a4 = 0;
  v5 = a1[12];
  if ( a2 >= v5 )
  {
    v7 = v5 + a1[13] - 1;
    if ( v7 < a2 )
      return 8201;
    if ( a3 >= v5 )
    {
      v9 = a3 + 1;
      if ( v7 >= a3 )
      {
        v11 = a2;
        v8 = 0;
        if ( a2 < v9 )
        {
          while ( 1 )
          {
            v12 = mp4_api_fn_45(a1, v11++, &v13, v14);
            v8 = v12;
            if ( v12 != 0 )
              break;
            *a4 += v14[0];
            if ( v11 >= v9 )
              return 0;
          }
        }
        return v8;
      }
    }
  }
  return 8201;
}


int mp4_api_fn_54(int a1, unsigned int a2, u32 *a3, int *a4)
{
  int v6;
  s64 v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  s64 v15;
  int v16;
  unsigned int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  unsigned int i;
  unsigned int v24;
  unsigned int v25;
  int *v26;
  int *v27;
  int v28;
  unsigned int v29;
  int v30;
  unsigned int v31;
  int *v32;
  unsigned int v33;
  unsigned int v34;
  int v35;
  int v36;
  int *v37;
  int *v39;
  int v40;
  int v41;
  unsigned int v42;
  int v43;
  unsigned int v44;
  int v45;
  unsigned int v46;
  unsigned int v47;

  v6 = 0;
  *a3 = 0;
  v8 = *(u64 *)(a1 + 24);
  v9 = *(u32 *)(a1 + 40) + *(u32 *)(a1 + 44);
  v10 = *(u32 *)(a1 + 48);
  v11 = *(u32 *)(a1 + 52);
  v12 = *(u32 *)(a1 + 16);
  v13 = *a4;
  v14 = *(u32 *)(a1 + 56) + *(u32 *)(a1 + 60);
  v16 = v10 + v11;
  v15 = v8 + (unsigned int)(8 * v9);
  v47 = a2;
  v17 = v12 - v9;
  v39 = NULL;
  if ( mp4_box_fn_2(v13) != (u32)v15 )
  {
    v20 = *a4;
    goto LABEL_3;
  }
  if ( v18 != HIDWORD(v15) )
  {
    v20 = *a4;
LABEL_3:
    v21 = mp4_box_fn_24(v20, v19, v15, SHIDWORD(v15));
    v22 = 2;
    if ( v21 != 0 )
      return v22;
  }
  if ( v17 != 0 )
  {
    for ( i = a4[4]; ; i = a4[4] )
    {
      if ( v17 < i )
        i = v17;
      v6 = mp4_io_fn_39(&v39, i, 1, v16, v14, &v40, &v41, (int *)&v42, (int **)(a1 + 36), a4);
      if ( v6 != 0 )
        break;
      v24 = v42;
      v25 = 0;
      if ( v42 != 0 )
      {
        v26 = v39;
        while ( 1 )
        {
          v44 = v25;
          if ( v26 == NULL || v26[2] == 0 )
            break;
          v27 = v26;
          if ( *v26 < v25 + 1 )
          {
            v28 = *v26;
            do
            {
              v27 = (int *)v27[7];
              v29 = v44 - v28 + 1;
              v44 -= v28;
              if ( v27 == NULL )
                goto LABEL_30;
              v28 = *v27;
            }
            while ( *v27 < v29 );
          }
          v30 = *(u32 *)(4 * v44 * v27[2] + v27[3]);
          v46 = v25;
          v43 = v30;
          if ( (unsigned int)v26[2] <= 1 )
            break;
          v31 = *v26;
          v46 = v25;
          v32 = v26;
          if ( v31 < v25 + 1 )
          {
            v33 = v31;
            do
            {
              v32 = (int *)v32[7];
              v34 = v46 - v33 + 1;
              v46 -= v33;
              if ( v32 == NULL )
                goto LABEL_30;
              v33 = *v32;
            }
            while ( *v32 < v34 );
          }
          v35 = v43;
          v6 = 0;
          v36 = *(u32 *)(4 * (v46 * v32[2] + 1) + v32[3]);
          v45 = v36;
          if ( v47 < v16 + v43 )
          {
            *a3 += (v47 - v16 + 1) * v45;
            goto LABEL_26;
          }
          ++v25;
          v16 += v43;
          *a3 += v43 * v36;
          v14 += v35 * v36;
          if ( v25 >= v24 )
            goto LABEL_24;
        }
LABEL_30:
        v6 = 10;
        goto LABEL_26;
      }
LABEL_24:
      ;
    }
  }
  v26 = v39;
LABEL_26:
  v22 = v6;
  if ( v26 != NULL )
  {
    v26[7] = *(u32 *)(a1 + 36);
    v37 = v39;
    v39 = NULL;
    *(u32 *)(a1 + 36) = v37;
  }
  return v22;
}


int mp4_api_fn_55(u32 *a1, unsigned int a2, u32 *a3, u32 *a4, u32 *a5)
{
  unsigned int v8;
  int v10;
  int v11;
  unsigned int v13;
  unsigned int v14;
  unsigned int *v15;
  int v16;
  unsigned int v17;
  unsigned int *v18;
  unsigned int v19;
  unsigned int v20;
  int v21;
  unsigned int v22;
  unsigned int *v23;
  unsigned int v24;
  int v25;
  int v26;
  unsigned int v27;
  int v28;
  int v29;
  unsigned int v30;
  unsigned int v31;
  unsigned int *v32;
  unsigned int v33;
  unsigned int v34;
  unsigned int v35;
  unsigned int *v36;
  unsigned int v37;
  int v38;
  int v39;
  unsigned int v40;
  s64 v41;
  unsigned int v42;
  unsigned int *v43;
  unsigned int v44;
  unsigned int v45;
  int v46;
  int i;
  unsigned int v48;
  unsigned int *v49;
  unsigned int v50;
  unsigned int v51;
  unsigned int *v52;
  unsigned int v53;
  unsigned int v54;
  int v55;
  unsigned int v56;
  unsigned int v57;
  unsigned int *v58;
  unsigned int v59;
  unsigned int v60;
  unsigned int v61;
  int v62;
  unsigned int *v63;
  unsigned int v64;
  unsigned int v65;
  unsigned int v66;
  unsigned int *v67;
  unsigned int v68;
  unsigned int v69;
  unsigned int v70;
  int v71;
  int v72;
  int v73;
  unsigned int v74;
  unsigned int v75;
  unsigned int v76;
  unsigned int v77;
  unsigned int v78;
  unsigned int v79;
  unsigned int v80;
  int v81;
  int v82;
  unsigned int v83;

  v8 = a1[14];
  if ( a2 >= v8 )
  {
    v10 = a1[15];
    if ( v8 + v10 - 1 >= a2 )
    {
      v11 = 4110;
      if ( a1[4] == 0 )
        return v11;
      v13 = a1[10];
      v14 = a1[16];
      if ( v14 >= v13 )
      {
        if ( v13 + a1[11] - 1 >= v14 )
          goto LABEL_11;
        a1[16] = 0;
      }
      else
      {
        a1[16] = 0;
      }
      v14 = 0;
      a1[17] = 0;
LABEL_11:
      if ( v14 == 0 )
      {
        v16 = a1[12];
        a1[16] = v13;
        v14 = v13;
        a1[17] = v16;
      }
      v15 = (unsigned int *)a1[8];
      if ( v15 != NULL )
      {
        v11 = 10;
        if ( v15[2] == 0 || v14 < v13 )
          return v11;
        v17 = *v15;
        v69 = v14 - v13;
        v18 = (unsigned int *)a1[8];
        if ( *v15 < v14 - v13 + 1 )
        {
          do
          {
            v18 = (unsigned int *)v18[7];
            v19 = v69 - v17 + 1;
            v69 -= v17;
            if ( v18 == NULL )
              return 10;
            v17 = *v18;
          }
          while ( *v18 < v19 );
        }
        v20 = *(u32 *)(4 * v69 * v18[2] + v18[3]) - 1;
        if ( a2 >= v20 )
        {
          if ( v20 < a2 )
          {
            v60 = a1[11];
            v61 = v13 + v60 - 1;
            v62 = v8 + v10;
            if ( v14 != v61 && v62 - a2 < a2 - v20 )
            {
              a1[16] = v61;
              if ( v15[2] == 0 || v61 < v13 )
                return 10;
              v73 = v60 - 1;
              v63 = v15;
              if ( *v15 < v60 )
              {
                v64 = *v15;
                do
                {
                  v63 = (unsigned int *)v63[7];
                  v65 = v73 - v64 + 1;
                  v73 -= v64;
                  if ( v63 == NULL )
                    return 10;
                  v64 = *v63;
                }
                while ( *v63 < v65 );
              }
              v14 = a1[16];
              v72 = *(u32 *)(4 * v73 * v63[2] + v63[3]);
              if ( v15[2] <= 1 || v14 < v13 )
                return 10;
              v66 = *v15;
              v74 = v14 - v13;
              v67 = v15;
              if ( *v15 < v14 - v13 + 1 )
              {
                do
                {
                  v67 = (unsigned int *)v67[7];
                  v68 = v74 - v66 + 1;
                  v74 -= v66;
                  if ( v67 == NULL )
                    return 10;
                  v66 = *v67;
                }
                while ( *v67 < v68 );
              }
              a1[17] = a1[12] + a1[13] - (v62 + 1 - v72) * *(u32 *)(4 * (v74 * v67[2] + 1) + v67[3]);
            }
          }
        }
        else if ( a2 - v8 < v20 - a2 )
        {
          v21 = a1[12];
          v14 = v13;
          a1[16] = v13;
          a1[17] = v21;
        }
        v11 = 10;
        if ( v15[2] == 0 || v14 < v13 )
          return v11;
        v22 = *v15;
        v75 = v14 - v13;
        v23 = v15;
        if ( *v15 < v14 - v13 + 1 )
        {
          do
          {
            v23 = (unsigned int *)v23[7];
            v24 = v75 - v22 + 1;
            v75 -= v22;
            if ( v23 == NULL )
              return 10;
            v22 = *v23;
          }
          while ( *v23 < v24 );
        }
        v25 = *(u32 *)(4 * v75 * v23[2] + v23[3]);
        if ( a2 < v25 - 1 )
        {
          v71 = v25 - 1;
          v45 = 0;
          v46 = a1[17];
          if ( v14 != 0 )
          {
            for ( i = v14 - v13 - 1; ; --i )
            {
              v48 = v14 - v45 - 1;
              if ( v15[2] == 0 || v48 < v13 )
                break;
              v81 = i;
              v49 = v15;
              if ( *v15 < i + 1 )
              {
                v50 = *v15;
                do
                {
                  v49 = (unsigned int *)v49[7];
                  v51 = v81 - v50 + 1;
                  v81 -= v50;
                  if ( v49 == NULL )
                    return 10;
                  v50 = *v49;
                }
                while ( *v49 < v51 );
              }
              v77 = *(u32 *)(4 * v81 * v49[2] + v49[3]) - 1;
              if ( v15[2] <= 1 || v48 < v13 )
                return 10;
              v82 = i;
              v52 = v15;
              if ( *v15 < i + 1 )
              {
                v53 = *v15;
                do
                {
                  v52 = (unsigned int *)v52[7];
                  v54 = v82 - v53 + 1;
                  v82 -= v53;
                  if ( v52 == NULL )
                    return 10;
                  v53 = *v52;
                }
                while ( *v52 < v54 );
              }
              v55 = *(u32 *)(4 * (v82 * v52[2] + 1) + v52[3]);
              v46 -= (v71 - v77) * v55;
              if ( a2 >= v77 )
              {
                *a3 = v46 + (a2 - v77) * v55;
                *a4 = v55;
                v56 = a1[10];
                v11 = 10;
                if ( v15[2] <= 2 || v48 < v56 )
                  return v11;
                v57 = *v15;
                v58 = v15;
                v83 = v48 - v56;
                if ( *v15 >= v14 - v45 - v56 )
                {
LABEL_68:
                  v11 = 0;
                  *a5 = *(u32 *)(4 * (v83 * v58[2] + 2) + v58[3]);
                  a1[17] = v46;
                  a1[16] = v48;
                  return v11;
                }
                while ( 1 )
                {
                  v58 = (unsigned int *)v58[7];
                  v59 = v83 - v57 + 1;
                  v83 -= v57;
                  if ( v58 == NULL )
                    return 10;
                  v57 = *v58;
                  if ( *v58 >= v59 )
                    goto LABEL_68;
                }
              }
              ++v45;
              v71 = v77;
              if ( v45 >= v14 )
                return 8195;
            }
            return 10;
          }
        }
        else
        {
          v26 = a1[11];
          v70 = v25 - 1;
          v27 = v14;
          v28 = v13 + v26;
          v29 = a1[17];
          if ( v14 < v13 + v26 )
          {
            v30 = v8 + v10;
            v31 = v14 - v13 + 1;
            while ( 1 )
            {
              if ( v27 == v28 - 1 )
              {
                v76 = v30;
              }
              else
              {
                if ( v15[2] == 0 || v27 + 1 < v13 )
                  return 10;
                v78 = v31;
                v32 = v15;
                if ( *v15 < v31 + 1 )
                {
                  v33 = *v15;
                  do
                  {
                    v32 = (unsigned int *)v32[7];
                    v34 = v78 - v33 + 1;
                    v78 -= v33;
                    if ( v32 == NULL )
                      return 10;
                    v33 = *v32;
                  }
                  while ( *v32 < v34 );
                }
                v76 = *(u32 *)(4 * v78 * v32[2] + v32[3]) - 1;
              }
              if ( v15[2] <= 1 || v27 < v13 )
                return 10;
              v35 = *v15;
              v79 = v27 - v13;
              v36 = v15;
              if ( *v15 < v31 )
              {
                do
                {
                  v36 = (unsigned int *)v36[7];
                  v37 = v79 - v35 + 1;
                  v79 -= v35;
                  if ( v36 == NULL )
                    return 10;
                  v35 = *v36;
                }
                while ( *v36 < v37 );
              }
              v38 = *(u32 *)(4 * (v79 * v36[2] + 1) + v36[3]);
              if ( a2 < v76 )
                break;
              ++v27;
              v39 = (v76 - v70) * *(u32 *)(4 * (v79 * v36[2] + 1) + v36[3]);
              v70 = v76;
              ++v31;
              v28 = v13 + v26;
              v29 += v39;
              if ( v27 >= v13 + v26 )
                return 8195;
            }
            *a3 = v29 + (a2 - v70) * v38;
            *a4 = v38;
            v40 = a1[10];
            v41 = ((u64)(v41) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v27 < v40) << 32);
            v41 = ((u64)(v41) & 0xFFFFFFFF00000000ULL) | (u32)(v15[2] <= 2);
            v11 = 10;
            if ( v41 != 0 )
              return v11;
            v42 = *v15;
            v43 = v15;
            v80 = v27 - v40;
            if ( *v15 >= v27 - v40 + 1 )
            {
LABEL_46:
              v11 = 0;
              *a5 = *(u32 *)(4 * (v80 * v43[2] + 2) + v43[3]);
              a1[17] = v29;
              a1[16] = v27;
              return v11;
            }
            while ( 1 )
            {
              v43 = (unsigned int *)v43[7];
              v44 = v80 - v42 + 1;
              v80 -= v42;
              if ( v43 == NULL )
                return 10;
              v42 = *v43;
              if ( *v43 >= v44 )
                goto LABEL_46;
            }
          }
        }
        return 8195;
      }
      return 10;
    }
  }
  return 4110;
}


int mp4_api_fn_56(u32 *a1, unsigned int a2, int a3, int a4)
{
  u32 *v4;
  int v9;
  unsigned int v11;
  unsigned int v12;
  unsigned int v13;
  int v14;
  unsigned int v15;
  unsigned int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  unsigned int v23;

  v4 = a1 + 24;
  v9 = mp4_api_fn_1(a1 + 24, a2, &v16, &v17, &v18);
  if ( v9 == 0 )
  {
    v11 = *(u32 *)(a4 + 16);
    if ( v11 == 0 )
      _break(0, 7u);
    v16 = v16 / v11 * v11;
    mp4_api_fn_11(v4, &v19, &v20, &v21, &v22);
    if ( v22 == 1 )
    {
      v12 = v16;
      v23 = v16;
    }
    else if ( v19 + v20 >= a2 + a3 )
    {
      v9 = mp4_api_fn_1(v4, a2 + a3 - 1, &v23, &v17, &v18);
      if ( v9 != 0 )
        return v9;
      v12 = v16;
    }
    else
    {
      v12 = v16;
      v23 = v21 + v22 - 1;
    }
    v13 = *(u32 *)(a4 + 16);
    if ( v13 == 0 )
      _break(0, 7u);
    v14 = *(u32 *)(a1[115] + 40);
    v23 = v23 / v13 * v13 + v13 - 1;
    v15 = v23;
    if ( v14 - 1 < v23 )
    {
      v23 = v14 - 1;
      v15 = v14 - 1;
    }
    return mp4_io_fn_30(a1 + 62, v12, v15 - v12 + 1, a4);
  }
  return v9;
}


int mp4_api_fn_57(int a1, int a2)
{
  int result;

  result = 8195;
  if ( a1 != 0 )
  {
    *(u32 *)(a1 + 448) = a2;
    return 0;
  }
  return result;
}


int mp4_api_fn_58(int a1, int a2, int a3, int a4)
{
  int result;
  u32 *v5;

  result = 8195;
  if ( a1 != 0 )
  {
    v5 = *(u32 **)(a1 + 460);
    result = 7;
    if ( v5 != NULL )
    {
      v5[10] = a4;
      v5[8] = a2;
      v5[9] = a3;
      return 0;
    }
  }
  return result;
}


int mp4_api_fn_59(int a1, int *a2)
{
  int v2;
  int v4;
  int v5;
  u32 *v6;
  int v7;

  v2 = 8195;
  *a2 = 0;
  if ( a1 != 0 )
  {
    v4 = *(u32 *)(a1 + 460);
    v5 = a1 + 248;
    v6 = (u32 *)(a1 + 248);
    v2 = 7;
    if ( v4 != 0 )
    {
      v7 = mp4_api_fn_6(v6);
      v2 = 8195;
      if ( v7 )
      {
        *a2 = mp4_api_fn_9(v5);
        return 0;
      }
    }
  }
  return v2;
}


int mp4_api_fn_60(int *a1)
{
  u32 *v2;
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int result;

  if ( a1 != NULL )
  {
    v2 = a1 + 2;
    if ( a1[10] != 0 )
    {
      mp4_io_fn_18(a1 + 10);
      v3 = v2[9];
    }
    else
    {
      v3 = a1[11];
    }
    if ( v3 != 0 )
      mp4_io_fn_18(a1 + 11);
    v4 = a1[32];
    v2[11] = 0;
    v2[12] = 0;
    v2[13] = 0;
    v2[14] = 0;
    v2[15] = 0;
    v2[10] = 0;
    if ( v4 != 0 )
      mp4_io_fn_18(a1 + 32);
    if ( a1[33] != 0 )
      mp4_io_fn_18(a1 + 33);
    v5 = a1[56];
    a1[35] = 0;
    a1[36] = 0;
    a1[37] = 0;
    a1[38] = 0;
    a1[39] = 0;
    a1[34] = 0;
    if ( v5 != 0 )
      mp4_io_fn_18(a1 + 56);
    if ( a1[57] != 0 )
      mp4_io_fn_18(a1 + 57);
    v6 = a1[70];
    a1[59] = 0;
    a1[58] = 0;
    if ( v6 != 0 )
      mp4_io_fn_18(a1 + 70);
    if ( a1[71] != 0 )
      mp4_io_fn_18(a1 + 71);
    v7 = a1[84];
    a1[73] = 0;
    a1[72] = 0;
    if ( v7 != 0 )
      mp4_io_fn_18(a1 + 84);
    if ( a1[85] != 0 )
      mp4_io_fn_18(a1 + 85);
    v8 = a1[102];
    a1[87] = 0;
    a1[88] = 0;
    a1[89] = 0;
    a1[86] = 0;
    if ( v8 != 0 )
      mp4_io_fn_18(a1 + 102);
    if ( a1[103] != 0 )
      mp4_io_fn_18(a1 + 103);
    v9 = a1[115];
    a1[105] = 0;
    a1[106] = 0;
    a1[107] = 0;
    a1[104] = 0;
    mp4_track_fn_65(v9);
    return mp4_track_fn_65((int)a1);
  }
  return result;
}


int mp4_api_fn_61(int a1, u32 *a2)
{
  int v2;
  int v3;

  v2 = 8195;
  if ( a1 != 0 )
  {
    v3 = *(u32 *)(a1 + 460);
    v2 = 7;
    if ( v3 != 0 )
    {
      v2 = 0;
      *a2 = *(u32 *)(v3 + 36);
    }
  }
  return v2;
}


int mp4_api_fn_62(int a1, u32 *a2, u32 *a3)
{
  int v3;
  u32 *v4;

  v3 = 8195;
  if ( a1 != 0 )
  {
    v4 = *(u32 **)(a1 + 460);
    v3 = 7;
    if ( v4 != NULL )
    {
      v3 = 0;
      *a2 = *v4;
      *a3 = v4[1];
    }
  }
  return v3;
}


int mp4_api_fn_63(u32 *a1, int a2)
{
  int v3;
  int v4;
  u32 *v5;

  v3 = 20495;
  if ( a1 != NULL )
  {
    a1[14] = 1937011827;
    a1[2] = 0;
    a1[3] = 0;
    a1[32] = 1937011555;
    a1[41] = 1937011578;
    a1[49] = 8;
    a1[50] = 1937007471;
    *a1 = 0;
    a1[1] = 0;
    a1[13] = 8;
    a1[31] = 8;
    a1[40] = 8;
    a1[22] = 0;
    a1[58] = 0;
    v4 = mp4_api_fn_64(a1, a2);
    v3 = 1;
    if ( v4 == 0 )
    {
      v5 = (u32 *)a1[70];
      v3 = 0;
      v5[10] = 1;
      v5[11] = 0;
      v5[12] = 0;
    }
  }
  return v3;
}


int mp4_api_fn_64(int a1, int a2)
{
  int v4;
  u32 *v5;
  int v7;
  int v8;

  v4 = 20495;
  if ( a1 != 0 && a2 != 0 )
  {
    v4 = 8195;
    if ( *(u32 *)(a1 + 16) != 0 )
    {
      v4 = 8195;
      if ( *(u32 *)(a1 + 124) != 0 && *(u32 *)(a1 + 52) != 0 && *(u32 *)(a1 + 160) != 0 )
      {
        if ( *(u32 *)(a1 + 196) != 0 )
        {
          v5 = *(u32 **)(a1 + 280);
          if ( v5 != NULL )
          {
            v7 = *(u32 *)(a2 + 32);
          }
          else
          {
            v5 = (u32 *)mp4_sample_fn_5(72);
            *(u32 *)(a1 + 280) = v5;
            v4 = 1;
            if ( v5 == NULL )
              return v4;
            v7 = *(u32 *)(a2 + 32);
          }
          *(u32 *)(a2 + 12) = 2;
          v5[13] = a2;
          if ( v7 == 1 )
          {
            v8 = mp4_api_fn_66(a1);
            v4 = 10;
            if ( v8 != 0 )
              return v4;
            v5 = *(u32 **)(a1 + 280);
            v5[10] = 0;
          }
          v5[4] = 1;
          v4 = 0;
          *(u8 *)(a1 + 268) = 1;
        }
        else
        {
          return 8195;
        }
      }
    }
  }
  return v4;
}


int mp4_api_fn_65(int a1, int a2, int a3)
{
  int result;
  int v5;

  result = 20495;
  if ( a1 != 0 )
  {
    v5 = *(u32 *)(a1 + 280);
    if ( v5 != 0 )
    {
      if ( *(u32 *)(v5 + 32) != 0 )
      {
        mp4_track_fn_64((int *)(v5 + 32), 0x7674626Cu, a3);
        *(u32 *)(*(u32 *)(a1 + 280) + 32) = 0;
      }
      mp4_track_fn_65(*(u32 *)(a1 + 280));
      *(u32 *)(a1 + 280) = 0;
      return 0;
    }
    else
    {
      return 0;
    }
  }
  return result;
}


int mp4_api_fn_66(int *a1)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  u32 *v10;
  int v11;
  int v12;
  int v13;
  int v14;

  v2 = a1[70];
  v3 = 7;
  if ( v2 != 0 )
  {
    v4 = *(u32 *)(v2 + 52);
    v3 = 3;
    if ( *(u32 *)(v4 + 32) != 0 )
    {
      if ( a1[18] != 0 )
        mp4_track_fn_64(a1 + 18, 0x7674626Cu, v4);
      v5 = a1[63];
      a1[17] = 0;
      if ( v5 != 0 )
        mp4_track_fn_64(a1 + 63, 0x7674626Cu, v4);
      v6 = a1[36];
      a1[62] = 0;
      if ( v6 != 0 )
        mp4_track_fn_64(a1 + 36, 0x7674626Cu, v4);
      v7 = a1[45];
      a1[35] = 0;
      if ( v7 != 0 )
        mp4_track_fn_64(a1 + 45, 0x7674626Cu, v4);
      v8 = a1[54];
      a1[44] = 0;
      a1[43] = 0;
      if ( v8 != 0 )
        mp4_track_fn_64(a1 + 54, 0x7674626Cu, v4);
      v9 = a1[27];
      a1[53] = 0;
      if ( v9 != 0 )
        mp4_track_fn_64(a1 + 27, 0x7674626Cu, v4);
      v10 = (u32 *)a1[70];
      a1[26] = 0;
      if ( v10[8] != 0 )
      {
        mp4_track_fn_64(v10 + 8, 0x7674626Cu, v4);
        v10 = (u32 *)a1[70];
        v10[8] = 0;
      }
      *v10 = 0;
      v10[1] = 0;
      v11 = a1[2];
      v12 = a1[3];
      v13 = a1[1];
      v14 = v10[10];
      v10[4] = 1;
      v10[2] = v13;
      v10[6] = v11;
      v10[7] = v12;
      v10[3] = 0;
      v10[9] = 0;
      v10[5] = 0;
      if ( v14 == 0 )
      {
        v10[12] = 0;
        v10[11] = 0;
      }
      v10[16] = 0;
      v3 = 0;
      v10[14] = 0;
      v10[15] = 0;
      a1[66] = 0;
      a1[19] = 0;
      a1[20] = 0;
      a1[21] = 0;
      a1[37] = 0;
      a1[38] = 0;
      a1[39] = 0;
      a1[28] = 0;
      a1[29] = 0;
      a1[30] = 0;
      a1[64] = 0;
      a1[65] = 0;
    }
  }
  return v3;
}


int mp4_api_fn_67(int *a1)
{
  int v1;
  u32 *v2;
  int v4;
  int v5;

  v1 = 3;
  v2 = (u32 *)a1[70];
  if ( *(u32 *)(v2[13] + 32) != 0 )
  {
    *a1 = 0;
    a1[1] = 0;
    a1[2] = 0;
    a1[3] = 0;
    v2[12] = 0;
    v2[10] = 0;
    v2[11] = 0;
    v4 = mp4_api_fn_66(a1);
    v1 = 10;
    if ( v4 == 0 )
    {
      v5 = mp4_api_fn_73(a1);
      v1 = 10;
      if ( v5 != 0x5010 )
        return v5;
    }
  }
  return v1;
}


int mp4_api_fn_68(int *a1, int a2, int a3)
{
  int v6;
  u32 *v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  u32 v14[2];

  v6 = 3;
  if ( *(u32 *)(*(u32 *)(a1[70] + 52) + 32) != 0 )
  {
    v6 = mp4_track_fn_62(a2, &v13);
    if ( v6 == 0 )
    {
      v6 = mp4_track_fn_61(a2, v14);
      if ( v6 == 0 )
      {
        a1[1] = a3;
        v8 = (u32 *)a1[70];
        *a1 = v13;
        v9 = v14[0];
        v10 = v14[1];
        v8[12] = a3;
        a1[2] = v9;
        a1[3] = v10;
        v8[10] = 0;
        v8[11] = 0;
        v11 = mp4_api_fn_66(a1);
        v6 = 10;
        if ( v11 == 0 )
        {
          v12 = mp4_api_fn_73(a1);
          v6 = 10;
          if ( v12 != 0x5010 )
            return v12;
        }
      }
    }
  }
  return v6;
}


int mp4_api_fn_69(int a1, int a2, int a3)
{
  int v3;
  int v6;
  int v8;
  int v9;

  v3 = 7;
  v6 = *(u32 *)(a1 + 280);
  if ( v6 != 0 )
  {
    v3 = 4;
    if ( *(u32 *)(v6 + 40) == 1 )
    {
      v3 = mp4_api_fn_70(a1, a2);
      if ( v3 == 0 )
      {
        v8 = *(u32 *)(a1 + 280);
        v3 = 0;
        v9 = *(u32 *)(v8 + 48) + a2 * a3;
        *(u32 *)(v8 + 44) += a2;
        *(u32 *)(v8 + 48) = v9;
      }
    }
  }
  return v3;
}


int mp4_api_fn_70(
        int a1,
        unsigned int a2,
        unsigned int a3,
        int a4,
        int a5,
        int a6,
        int a7,
        int a8,
        unsigned int a9,
        unsigned int a10)
{
  int v13;
  int v18;
  int v19;
  int *v20;
  unsigned int v21;
  int v22;
  int v23;
  int v24;
  unsigned int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  int v33;
  unsigned int v34;
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
  int v45;
  int v46;
  int v47;
  unsigned int v48;
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
  int *v61;
  unsigned int v62;
  int v63;
  int v64;
  unsigned int v65;
  int v66;
  u32 v67[7];
  u32 v68[4];

  v13 = 0;
  v66 = 0;
  memset(v67, 0, 16);
  if ( a2 == 0 )
    return v13;
  v13 = 7;
  if ( *(u32 *)(a1 + 280) == 0 )
    return v13;
  mp4_track_fn_80(a2, a3, &v66, v67);
  v13 = 4121;
  if ( v66 != 0 )
    return v13;
  v68[0] = *(u32 *)(*(u32 *)(a1 + 280) + 12) + *(u32 *)(*(u32 *)(a1 + 280) + 8);
  v18 = mp4_sample_fn_16(v68, a2 * a3);
  v13 = 4121;
  if ( v18 != 0 )
    return v13;
  v68[0] = **(u32 **)(a1 + 280) + *(u32 *)(a1 + 176);
  v19 = mp4_sample_fn_16(v68, a2);
  v13 = 4123;
  if ( v19 != 0 )
    return v13;
  if ( a8 != 2 || a2 == 1 )
  {
    if ( a8 == 2 && a2 == 1 )
    {
      v58 = *(u32 *)(a1 + 88);
    }
    else
    {
      if ( a8 != 0 )
      {
        if ( a8 == 1 )
        {
          if ( *(u32 *)(a1 + 88) == 0 )
          {
            *(u32 *)(a1 + 88) = 16;
            *(u32 *)(a1 + 92) = 1937011571;
            v61 = *(int **)(a1 + 280);
            v62 = *v61 + *(u32 *)(a1 + 176);
            v63 = *v61;
            v68[0] = *v61;
            if ( v68[0] < v62 )
            {
              while ( 1 )
              {
                v64 = mp4_codec_fn_11(a1 + 88, *(u32 *)(a1 + 104), v63 + 1);
                v13 = 1;
                if ( v64 != 0 )
                  return v13;
                v65 = **(u32 **)(a1 + 280) + *(u32 *)(a1 + 176);
                v63 = ++v68[0];
                if ( v68[0] >= v65 )
                {
                  v27 = *(u8 *)(a1 + 268);
                  goto LABEL_15;
                }
              }
            }
            goto LABEL_14;
          }
          v27 = *(u8 *)(a1 + 268);
        }
        else
        {
          v27 = *(u8 *)(a1 + 268);
        }
LABEL_15:
        if ( v27 == 1 )
        {
          v56 = mp4_codec_fn_8(a1 + 196, *(u32 *)(a1 + 212), a5, a6);
          v13 = 1;
          if ( v56 != 0 )
            return v13;
          v28 = *(u32 *)(a1 + 164);
        }
        else
        {
          v28 = *(u32 *)(a1 + 164);
        }
        if ( v28 == 1937011578 )
        {
          v68[0] = 0;
          if ( a2 != 0 )
          {
            while ( 1 )
            {
              v55 = mp4_codec_fn_7(a1 + 160, *(u32 *)(a1 + 176), a7);
              v13 = 1;
              if ( v55 != 0 )
                return v13;
              if ( ++v68[0] >= a2 )
              {
                v29 = *(u32 *)(a1 + 68);
                goto LABEL_20;
              }
            }
          }
        }
        v29 = *(u32 *)(a1 + 68);
LABEL_20:
        if ( v29 != 0 )
        {
          if ( mp4_codec_fn_5(a1 + 52, v29 - 1, 1) == a3 )
          {
            if ( *(u32 *)(*(u32 *)(a1 + 280) + 16) == 1 )
            {
              if ( *(u8 *)(a1 + 268) == 1 )
              {
                v30 = *(u32 *)(a1 + 68);
                goto LABEL_22;
              }
              v53 = *(u32 *)(a1 + 68);
            }
            else
            {
              v53 = *(u32 *)(a1 + 68);
            }
            v54 = mp4_codec_fn_5(a1 + 52, v53 - 1, 0);
            v33 = a1 + 52;
            v35 = *(u32 *)(a1 + 68) - 1;
            v34 = v54 + a2;
            v13 = 0;
            goto LABEL_25;
          }
          v30 = *(u32 *)(a1 + 68);
        }
        else
        {
          v30 = *(u32 *)(a1 + 68);
        }
LABEL_22:
        v31 = mp4_codec_fn_10(a1 + 52, v30, 0, a2);
        v13 = 1;
        if ( v31 != 0 )
          return v13;
        v33 = a1 + 52;
        v34 = a3;
        v35 = *(u32 *)(a1 + 68) - 1;
LABEL_25:
        v36 = mp4_codec_fn_10(v33, v35, v13, v34);
        v13 = 1;
        if ( v36 != 0 )
          return v13;
        if ( *(u8 *)(a1 + 268) == 1 )
        {
          v51 = mp4_codec_fn_9(
                  a1 + 124,
                  *(u32 *)(a1 + 140),
                  0,
                  *(u32 *)(a1 + 212) + *(u32 *)(*(u32 *)(a1 + 280) + 4));
          v13 = 1;
          if ( v51 != 0 )
            return v13;
          v52 = mp4_codec_fn_9(a1 + 124, *(u32 *)(a1 + 140) - 1, 1, a2);
          v13 = 1;
          if ( v52 != 0 )
            return v13;
          v40 = a10;
          v38 = a1 + 124;
          v39 = *(u32 *)(a1 + 140) - 1;
          v41 = 2;
        }
        else
        {
          v37 = mp4_codec_fn_4(a1 + 124, *(u32 *)(a1 + 140) - 1, 1);
          v38 = a1 + 124;
          v39 = *(u32 *)(a1 + 140) - 1;
          v40 = v37 + a2;
          v41 = 1;
        }
        v42 = mp4_codec_fn_9(v38, v39, v41, v40);
        v13 = 1;
        if ( v42 != 0 )
          return v13;
        if ( *(u32 *)(a1 + 232) == 0 )
        {
          *(u32 *)(a1 + 232) = 16;
          *(u32 *)(a1 + 236) = 1668576371;
        }
        if ( *(u32 *)(*(u32 *)(a1 + 280) + 16) == 1 && (v44 = a1 + 232, *(u8 *)(a1 + 268) == 1) )
        {
          v45 = *(u32 *)(a1 + 248);
        }
        else
        {
          v43 = *(u32 *)(a1 + 248);
          v44 = a1 + 232;
          if ( v43 != 0 )
          {
            v66 = mp4_codec_fn_6(a1 + 232, v43 - 1, 1);
            if ( v66 == a9 )
            {
              v50 = mp4_codec_fn_6(a1 + 232, *(u32 *)(a1 + 248) - 1, 0);
              v48 = v50 + a2;
              v47 = a1 + 232;
              v49 = *(u32 *)(a1 + 248) - 1;
              v13 = 0;
              v67[0] = v50;
              goto LABEL_36;
            }
          }
          v45 = *(u32 *)(a1 + 248);
        }
        v46 = mp4_codec_fn_12(v44, v45, 0, a2);
        v13 = 1;
        if ( v46 != 0 )
          return v13;
        v47 = v44;
        v48 = a9;
        v49 = *(u32 *)(a1 + 248) - 1;
LABEL_36:
        mp4_codec_fn_12(v47, v49, v13, v48);
        *(u8 *)(a1 + 268) = 0;
        v13 = 0;
        *(u32 *)(*(u32 *)(a1 + 280) + 12) += a2 * a3;
        return v13;
      }
      v58 = *(u32 *)(a1 + 88);
    }
    if ( v58 == 0 )
    {
      v27 = *(u8 *)(a1 + 268);
      goto LABEL_15;
    }
    v68[0] = 0;
    v59 = 0;
    if ( a2 != 0 )
    {
      while ( 1 )
      {
        v60 = mp4_codec_fn_11(a1 + 88, *(u32 *)(a1 + 104), *(u32 *)(a1 + 176) + **(u32 **)(a1 + 280) + v59 + 1);
        v13 = 1;
        if ( v60 != 0 )
          return v13;
        v59 = ++v68[0];
        if ( v68[0] >= a2 )
        {
          v27 = *(u8 *)(a1 + 268);
          goto LABEL_15;
        }
      }
    }
LABEL_14:
    v27 = *(u8 *)(a1 + 268);
    goto LABEL_15;
  }
  if ( *(u32 *)(a1 + 88) != 0 )
  {
    v20 = *(int **)(a1 + 280);
    v57 = *(u32 *)(a1 + 176);
  }
  else
  {
    *(u32 *)(a1 + 88) = 16;
    *(u32 *)(a1 + 92) = 1937011571;
    v20 = *(int **)(a1 + 280);
    v57 = *(u32 *)(a1 + 176);
    v21 = *v20 + v57;
    v22 = *v20;
    v68[0] = *v20;
    if ( v68[0] < v21 )
    {
      v23 = a1 + 88;
      while ( 1 )
      {
        v24 = mp4_codec_fn_11(a1 + 88, *(u32 *)(a1 + 104), v22 + 1);
        v13 = 1;
        if ( v24 != 0 )
          return v13;
        v20 = *(int **)(a1 + 280);
        v57 = *(u32 *)(a1 + 176);
        v25 = *v20 + v57;
        v22 = ++v68[0];
        if ( v68[0] >= v25 )
          goto LABEL_13;
      }
    }
  }
  v23 = a1 + 88;
LABEL_13:
  v26 = mp4_codec_fn_11(v23, *(u32 *)(a1 + 104), *v20 + v57 + 1);
  v13 = 1;
  if ( v26 == 0 )
    goto LABEL_14;
  return v13;
}


int mp4_api_fn_71(int a1, int a2)
{
  int v4;
  int v5;
  u32 *v6;
  unsigned int v7;
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
  u32 v21[4];

  v4 = *(u8 *)(a1 + 268);
  v5 = 4109;
  if ( v4 == 1 )
    return v5;
  v6 = *(u32 **)(a1 + 280);
  v5 = 7;
  if ( v6 == NULL )
    return v5;
  v5 = 4109;
  if ( *(u32 *)(a1 + 176) == 0 )
    return v5;
  v7 = *(u32 *)(a1 + 140);
  if ( v7 >= 2 )
  {
    v12 = mp4_codec_fn_4(a1 + 124, v7 - 2, 1);
    if ( v12 == mp4_codec_fn_4(a1 + 124, *(u32 *)(a1 + 140) - 1, 1) )
    {
      v14 = mp4_codec_fn_4(a1 + 124, *(u32 *)(a1 + 140) - 2, 2);
      v13 = mp4_codec_fn_4(a1 + 124, *(u32 *)(a1 + 140) - 1, 2);
      v6 = *(u32 **)(a1 + 280);
      if ( v14 == v13 && v6[4] == 0 )
      {
        v15 = *(u32 *)(a1 + 140);
        if ( *(u32 *)(a1 + 148) == v15 - 1 )
        {
          v17 = mp4_codec_fn_4(a1 + 124, v15 - 2, 0);
          v19 = mp4_codec_fn_4(a1 + 124, *(u32 *)(a1 + 140) - 2, 1);
          v18 = mp4_codec_fn_4(a1 + 124, *(u32 *)(a1 + 140) - 1, 0);
          v15 = *(u32 *)(a1 + 140);
          v20 = *(u32 *)(a1 + 152) - (v18 - v17) * v19;
          --*(u32 *)(a1 + 148);
          *(u32 *)(a1 + 152) = v20;
        }
        v16 = mp4_track_fn_75((u32 *)(a1 + 144), v15 - 1, v15 - 1, 3);
        v5 = 1;
        if ( v16 != 0 )
          return v5;
        v6 = *(u32 **)(a1 + 280);
        --*(u32 *)(a1 + 140);
      }
    }
    else
    {
      v6 = *(u32 **)(a1 + 280);
    }
  }
  *(u8 *)(a1 + 268) = 1;
  if ( a2 != 1 )
  {
    v6[4] = 0;
    return 0;
  }
  v9 = v6[9];
  v6[4] = 1;
  v6[9] = v9 + 1;
  v21[0] = 0;
  v21[1] = 0;
  v21[2] = *(u32 *)(a1 + 212) + v6[1] - 1;
  v5 = mp4_codec_fn_89(v6 + 8, 3, v21, 2);
  if ( v5 == 0 )
  {
    if ( *(u32 *)(*(u32 *)(*(u32 *)(a1 + 280) + 52) + 12) >= *(u32 *)(*(u32 *)(a1 + 280) + 36) )
      return 0;
    v10 = mp4_api_fn_74(a1, 1);
    v5 = 10;
    if ( v10 == 0 )
    {
      v11 = mp4_codec_fn_31(a1);
      v5 = 10;
      if ( v11 == 0 )
        return 0;
    }
  }
  return v5;
}


int mp4_api_fn_72(int a1)
{
  int *v2;
  int v3;
  int v5;
  int v6;
  unsigned int v7;
  unsigned int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  unsigned int v14;
  int i;
  int v16;
  unsigned int v17;
  u32 v18[3];

  if ( a1 == 0 )
    return 7;
  v2 = *(int **)(a1 + 280);
  if ( v2 == NULL )
    return 7;
  v5 = v2[9];
  v6 = *(u32 *)(v2[13] + 12);
  v7 = v6 - v5;
  v8 = 1;
  if ( v6 != v5 )
    goto LABEL_12;
  v9 = *v2;
  v10 = v2[5];
  v11 = v5;
  if ( v10 != v9 + *(u32 *)(a1 + 176)
    && (mp4_codec_fn_2(a1, v10, &v17, v18), v12 = *(u32 *)(a1 + 280), v11 = 0, v13 = 0, *(u32 *)(v12 + 36) != 0) )
  {
    for ( i = *(u32 *)(v12 + 32); ; i = *(u32 *)(v16 + 32) )
    {
      mp4_codec_fn_91(i, v13, 2, v18);
      if ( v18[0] >= v17 )
      {
        v11 = v13;
        goto LABEL_8;
      }
      v16 = *(u32 *)(a1 + 280);
      if ( (unsigned int)++v13 >= *(u32 *)(v16 + 36) )
        break;
    }
    v8 = v7;
  }
  else
  {
LABEL_8:
    v8 = v11 + v7;
  }
  if ( v8 >= 2 )
    v8 = 1;
  v3 = 20497;
  if ( v8 != 0 )
  {
LABEL_12:
    v14 = 0;
    do
    {
      ++v14;
      v3 = mp4_api_fn_73(a1);
      if ( v3 != 0 )
        break;
      v3 = 0;
    }
    while ( v14 < v8 );
  }
  return v3;
}


int mp4_api_fn_73(int a1)
{
  unsigned int v1;
  int v3;
  u32 *v4;
  int v5;
  int v7;
  int v8;
  unsigned int v9;
  unsigned int v10;
  int v11;
  u32 *v12;
  int v13;
  int v14;
  int v15;
  u32 *v16;
  int v17;
  unsigned int v18;
  unsigned int v19;
  unsigned int v20;
  int *v21;
  int v22;
  int v23;
  int v24;
  int v25;
  unsigned int v26;
  int v27;
  unsigned int v28;
  unsigned int v29;
  int v30;
  unsigned int v31;
  unsigned int v32;
  u32 *v33;
  int v34;
  int v35;
  int v36;
  int v37;
  int v38;
  int v39;
  int v40;
  u32 *v41;
  int v42;
  u32 *v43;
  int v44;
  int v45;
  int v46;
  int v47;
  unsigned int v48;
  int v49;
  int v50;
  int v51;
  unsigned int v52;
  unsigned int v53;
  unsigned int v54;
  u32 v55[4];
  u32 *v56;
  unsigned int v57;
  int v58[2];
  int v59;
  int v60;
  int v61;
  int v62;
  unsigned int v63;
  u32 *v64;
  unsigned int v65;
  int v66;

  v1 = 0;
  if ( a1 == 0 )
    return 7;
  v3 = *(u32 *)(a1 + 280);
  if ( v3 == 0 )
    return 7;
  v4 = *(u32 **)(v3 + 52);
  v5 = 20496;
  if ( v4[8] != 0 )
  {
    mp4_box_fn_106(v4);
    v7 = *(u32 *)(a1 + 280);
    if ( *(u64 *)(v7 + 24) != 0 )
    {
      if ( mp4_track_fn_53(
             *(u32 *)(*(u32 *)(*(u32 *)(a1 + 276) + 700) + 152),
             *(u32 *)(v7 + 24) | *(u32 *)(v7 + 28),
             *(u32 *)(v7 + 24),
             *(u32 *)(v7 + 28),
             v58,
             &v59) == 8194
        && ((v41 = (u32 *)mp4_codec_fn_142(a1), v42 = 20496, mp4_box_fn_20(v41))
         || (v42 = mp4_track_fn_27(
                     *(u32 *)(*(u32 *)(*(u32 *)(a1 + 276) + 700) + 152),
                     *(int **)(*(u32 *)(a1 + 280) + 52),
                     *(u64 *)(*(u32 *)(a1 + 280) + 24),
                     v58,
                     (int)&v59)) != 0) )
      {
        if ( v42 == 20496 )
        {
          v43 = *(u32 **)(a1 + 280);
          v44 = v43[2];
          v45 = v43[3];
          v46 = *v43 + *(u32 *)(a1 + 176);
          v43[10] = 1;
          v43[11] = v46;
          v43[12] = v44 + v45;
        }
        return v42;
      }
      else if ( *(u32 *)(*(u32 *)(a1 + 280) + 36) != *(u32 *)(*(u32 *)(*(u32 *)(a1 + 280) + 52) + 12)
             || (v40 = mp4_api_fn_74(a1, 1), v5 = 10, v40 == 0) )
      {
        mp4_sample_fn_11(*(u32 *)(a1 + 276), &v61);
        mp4_track_fn_70(*(u32 *)(a1 + 276), &v62);
        v23 = mp4_track_fn_54(v58[0], v61, &v63);
        v5 = 10;
        if ( v23 == 0 )
        {
          if ( v63 != 0 )
          {
            v26 = 0;
            v27 = v58[0];
            while ( 2 )
            {
              v28 = 0;
              v29 = 0;
              if ( mp4_track_fn_55(v27, v61, v26, &v64) != 0 )
              {
                return 10;
              }
              else
              {
                while ( 1 )
                {
                  v30 = mp4_track_fn_60((int)v64, v28, &v57);
                  v31 = v57;
                  if ( v30 != 0 )
                    break;
                  v32 = 0;
                  if ( v57 != 0 )
                  {
                    v33 = v64;
                    while ( 1 )
                    {
                      ++v32;
                      v35 = mp4_track_fn_52(v33, v62, v29++, &v47, &v65);
                      if ( v35 != 0 )
                      {
                        v38 = mp4_api_fn_66((int *)a1);
                        v5 = 10;
                        if ( v38 == 0 )
                        {
                          v5 = 4120;
                          if ( v35 != 8 )
                          {
                            v5 = 4109;
                            if ( v35 != 4109 )
                              return 8195;
                          }
                        }
                        return v5;
                      }
                      v36 = mp4_api_fn_70(a1, 1u, v48, v34, v49, v50, v47, v51 != 1, v52, v65);
                      if ( v36 != 0 )
                        break;
                      v31 = v57;
                      v33 = v64;
                      if ( v32 >= v57 )
                        goto LABEL_48;
                    }
                    v37 = mp4_api_fn_66((int *)a1);
                    v5 = v36;
                    if ( v37 == 0 )
                      return v5;
                    return 10;
                  }
LABEL_48:
                  if ( v31 != 0 )
                  {
                    mp4_api_fn_71(a1, 0);
                    v1 += v57;
                  }
                  ++v28;
                }
                ++v26;
                v27 = v58[0];
                if ( v26 < v63 )
                  continue;
                v39 = mp4_track_fn_62(v58[0], &v66);
                v5 = 10;
                if ( v39 == 0 )
                {
                  v14 = v59;
                  v15 = v60;
                  v12 = *(u32 **)(a1 + 280);
                  goto LABEL_19;
                }
              }
              break;
            }
          }
          else
          {
            v24 = *(u32 *)(a1 + 280);
            v25 = v60;
            *(u32 *)(v24 + 24) = v59;
            *(u32 *)(v24 + 28) = v25;
            return 0;
          }
        }
      }
    }
    else
    {
      mp4_sample_fn_14(*(u32 *)(a1 + 276), &v56);
      v5 = 8195;
      if ( v56 != NULL && v56[13] != 0 && v56[31] != 0 && v56[40] != 0 && v56[49] != 0 )
      {
        v8 = mp4_codec_fn_24(v56, &v57, 0);
        v5 = 8195;
        if ( v8 == 0 )
        {
          v9 = v57;
          v10 = 0;
          if ( v57 != 0 )
          {
            v16 = v56;
            while ( 1 )
            {
              v17 = mp4_codec_fn_20(v16, v10, 1, &v47, 0);
              v5 = 8195;
              if ( v17 != 0 )
                break;
              v18 = v53;
              v19 = 0;
              if ( v53 != 0 )
              {
                v20 = v10;
                do
                {
                  v21 = &v47;
                  ++v10;
                  if ( v19 != 0 )
                  {
                    v22 = mp4_codec_fn_20(v56, v20, 0, &v47, 0);
                    v5 = 8195;
                    if ( v22 != 0 )
                      return v5;
                  }
                  ++v19;
                  v5 = mp4_api_fn_70(a1, 1u, v48, (int)v21, v49, v50, v47, v51 != 1, v52, v54);
                  if ( v5 != 0 )
                    return v5;
                  v20 = v10;
                }
                while ( v19 < v18 );
              }
              mp4_api_fn_71(a1, 0);
              v9 = v57;
              v16 = v56;
              if ( v10 >= v57 )
              {
                v11 = *(u32 *)(a1 + 276);
                goto LABEL_18;
              }
            }
          }
          else
          {
            v11 = *(u32 *)(a1 + 276);
LABEL_18:
            v1 = v9;
            v12 = *(u32 **)(a1 + 280);
            v13 = *(u32 *)(v11 + 700);
            v66 = 0;
            v14 = *(u32 *)(v13 + 144);
            v15 = *(u32 *)(v13 + 148);
LABEL_19:
            v12[6] = v14;
            v12[7] = v15;
            v12[4] = 1;
            *(u8 *)(a1 + 268) = 1;
            if ( v1 != 0 )
            {
              ++v12[9];
              v55[0] = 1;
              v55[1] = v66;
              v55[2] = *(u32 *)(a1 + 212) + v12[1] - 1;
              v5 = mp4_codec_fn_89(v12 + 8, 3, v55, 2);
              if ( v5 == 0 )
                return 0;
            }
            else
            {
              return 0;
            }
          }
        }
      }
    }
  }
  return v5;
}


int mp4_api_fn_74(u32 *a1, unsigned int a2)
{
  int v2;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v13;
  unsigned int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  u32 *v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  unsigned int v29;
  int v30;
  unsigned int *v31;
  unsigned int v32;
  unsigned int v33;
  unsigned int v34;
  unsigned int v35;
  unsigned int v36;
  unsigned int v37;
  int v38;
  int v39;
  int v40;
  int v41;
  unsigned int v42;
  int v43;
  unsigned int v44;
  unsigned int v45;
  unsigned int v46[4];

  v2 = 0;
  v5 = a1[70];
  v6 = 7;
  if ( v5 != 0 )
  {
    if ( *(u32 *)(v5 + 36) < a2 )
      a2 = *(u32 *)(v5 + 36);
    v6 = 4109;
    if ( a2 != 0 )
    {
      v7 = mp4_codec_fn_91(*(u32 *)(v5 + 32), a2 - 1, 2, &v38);
      v6 = 4122;
      if ( v7 == 0 )
      {
        v8 = mp4_codec_fn_1(a1, v38, &v39, &v40, &v41);
        v6 = 4122;
        if ( v8 == 0 )
        {
          v9 = a1[13];
          v10 = v39 + v40 - *(u32 *)a1[70];
          v39 = v39 + v40 - 1;
          v40 = v10;
          if ( v9 == 0 )
            return 4122;
          if ( a1[31] == 0 )
            return 4122;
          if ( a1[40] == 0 )
            return 4122;
          if ( a1[49] == 0 )
            return 4122;
          v11 = a1[18];
          v41 = v10;
          v13 = mp4_codec_fn_131(v11, &v41, 0, &v42);
          v6 = 4122;
          if ( v13 == 0 )
          {
            v14 = 0;
            do
            {
              mp4_codec_fn_91(a1[18], v14, 0, &v41);
              mp4_codec_fn_91(a1[18], v14++, 1, &v43);
              v2 += v41 * v43;
            }
            while ( v42 >= v14 );
            mp4_track_fn_75(a1 + 18, 0, v42, 2);
            v15 = a1[17];
            v16 = a1[58];
            v44 = 0;
            a1[17] = v15 - v42 - 1;
            if ( v16 != 0 )
            {
              v17 = a1[63];
              v41 = v40;
              v18 = mp4_codec_fn_131(v17, &v41, 0, &v44);
              v6 = 4122;
              if ( v18 != 0 )
                return v6;
              mp4_track_fn_75(a1 + 63, 0, v44, 2);
              a1[62] = a1[62] - v44 - 1;
            }
            v19 = mp4_codec_fn_88(a1[36], v38 + 1, 0, &v45);
            v6 = 4122;
            if ( v19 != 0 )
              return v6;
            mp4_track_fn_75(a1 + 36, 0, v45, 3);
            v20 = a1[43];
            a1[35] = a1[35] - v45 - 1;
            if ( v20 == 0 )
              mp4_track_fn_75(a1 + 45, 0, v40 - 1, 1);
            v21 = a1[70];
            a1[44] -= v40;
            mp4_track_fn_76(a1 + 54, 0, v38 - *(u32 *)(v21 + 4), 1);
            v22 = (u32 *)a1[70];
            v23 = v38;
            v24 = a1[53];
            v25 = v22[1];
            v26 = a1[22];
            v46[0] = 0;
            a1[53] = v24 + v25 - v38 - 1;
            if ( v26 != 0 )
            {
              v27 = a1[27];
              if ( v27 != 0 )
              {
                if ( mp4_codec_fn_88(v27, v39 + 1, 0, v46) != 0 )
                {
                  v22 = (u32 *)a1[70];
                  v23 = v38;
                }
                else
                {
                  mp4_track_fn_75(a1 + 27, 0, v46[0], 1);
                  v23 = v38;
                  v22 = (u32 *)a1[70];
                  a1[26] = a1[26] - v46[0] - 1;
                }
              }
            }
            v28 = v22[3];
            v29 = v22[9] - a2;
            v30 = v22[2] + v2;
            *v22 = v39 + 1;
            v22[2] = v30;
            v22[1] = v23 + 1;
            v22[3] = v28 - v2;
            v22[9] = v29;
            mp4_track_fn_75(v22 + 8, 0, a2 - 1, 3);
            v31 = (unsigned int *)a1[70];
            v32 = v31[5];
            v33 = *v31;
            if ( v32 >= *v31 )
            {
              if ( v33 + a1[44] >= v32 )
              {
                v34 = a1[19];
LABEL_33:
                if ( v42 < v34 )
                  a1[19] = v34 - v42 - 1;
                else
                  a1[19] = 0;
                v35 = a1[37];
                if ( v45 < v35 )
                  a1[37] = v35 - v45 - 1;
                else
                  a1[37] = 0;
                v36 = a1[28];
                if ( v46[0] < v36 )
                  a1[28] = v36 - v46[0] - 1;
                else
                  a1[28] = 0;
                v37 = a1[64];
                if ( v44 < v37 )
                  a1[64] = v37 - v44 - 1;
                else
                  a1[64] = 0;
                return 0;
              }
              v31[5] = v33;
            }
            else
            {
              v31[5] = v33;
            }
            v34 = a1[19];
            goto LABEL_33;
          }
        }
      }
    }
  }
  return v6;
}


